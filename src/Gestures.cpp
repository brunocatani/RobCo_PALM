#include <F4SE/Impl/PCH.h>
#include <spdlog/spdlog.h>
#include "Gestures.h"
#include "api/FRIKApiV2.h"

namespace wheel {
namespace {
using namespace rock::provider;
using Frik=frik::api::FRIKApiV2;
RockProviderHand physicalHand(unsigned index){return index?RockProviderHand::Left:RockProviderHand::Right;}
Frik::Hand skeletonHand(unsigned index){return index?Frik::Hand::Left:Frik::Hand::Right;}
}
void Gestures::initialize() {
 const auto error=Frik::initialize();
 const auto* api=Frik::inst;
 _available=!error && api && api->isSkeletonReady && api->getHandPoseLocalTransformsForPose &&
  api->getCurrentHandPose && api->isConfigOpen && api->isWristPipboyOpen && api->isOffHandGrippingWeapon &&
  RockProviderApi::inst->setHandVisualAuthorityV1 && RockProviderApi::inst->clearHandVisualAuthorityV1 &&
  supportsHandVisualAuthorityV1();
 spdlog::info("PALM gestures: {} (skeleton pose resolver result {})",_available?"available through ROCK":"unavailable",error);
}
void Gestures::clearHand(std::uint64_t owner,unsigned hand,const char* reason) {
 if(!_view.active[hand])return;
 const auto result=RockProviderApi::inst->clearHandVisualAuthorityV1(owner,physicalHand(hand));
 spdlog::info("PALM gesture cleared: hand={}, reason={}, result={}",hand?"left":"right",reason,static_cast<unsigned>(result));
 // Even if explicit cleanup is rejected, stopping renewal expires ROCK's lease.
 _view.active[hand]=0;
}
void Gestures::clear(std::uint64_t owner) {
 for(unsigned hand=0;hand<2;++hand)clearHand(owner,hand,"context/lifecycle changed");
 _view.availability={};
}
bool Gestures::publish(std::uint64_t owner,unsigned hand,const RockProviderFrameSnapshot& frame) {
 auto& request=_requests[hand];
 request.worldGeneration=frame.worldGeneration;request.skeletonGeneration=frame.skeletonGeneration;
 request.providerGeneration=frame.providerGeneration;
 const auto result=RockProviderApi::inst->setHandVisualAuthorityV1(owner,&request);
 if(result==RockProviderResultV1::Ok)return true;
 spdlog::warn("PALM gesture publication rejected: hand={}, result={}",hand?"left":"right",static_cast<unsigned>(result));
 clearHand(owner,hand,"publication rejected");return false;
}
void Gestures::update(std::uint64_t owner,const RockProviderFrameSnapshot& frame,bool usable) {
 if(!usable || !_available || !frame.frikSkeletonReady || !Frik::inst->isSkeletonReady()) {clear(owner);return;}
 const bool uiBusy=Frik::inst->isConfigOpen() || Frik::inst->isWristPipboyOpen();
 for(unsigned hand=0;hand<2;++hand) {
  RockProviderHandInteractionStateV1 interaction;
  const bool queryOk=RockProviderApi::inst->getHandInteractionStateV1(owner,physicalHand(hand),&interaction)==RockProviderResultV1::Ok &&
   interaction.hand==physicalHand(hand);
  bool inputAvailable=true,actionHeld=false;
  // Grip, trigger, A/X, B/Y and stick click. The right B wheel hold is UI
  // input while selecting, but its next press cancels an already active pose.
  for(const unsigned button:{2u,33u,7u,1u,32u}) {
   if(button==1 && hand==0 && !_view.active[hand])continue;
   RockProviderRawWandButtonStateV1 raw;
   inputAvailable&=RockProviderApi::inst->getRawWandButtonStateV1(physicalHand(hand),button,&raw) && raw.available;
   actionHeld|=raw.held!=0;
  }
  const auto pose=Frik::inst->getCurrentHandPose(skeletonHand(hand));
  // Controller-driven pointing/thumbs-up can occur on a free hand. Existing
  // custom claims and weapon grips belong to another interaction.
  const bool occupiedPose=pose==Frik::HandPoseKind::Custom || pose==Frik::HandPoseKind::HoldingWeapon ||
   pose==Frik::HandPoseKind::HoldingGun || pose==Frik::HandPoseKind::HoldingMelee ||
   pose==Frik::HandPoseKind::OffhandGrip || pose==Frik::HandPoseKind::Attaboy;
  const bool poseBusy=uiBusy || (physicalHand(hand)==frame.offhandHand && Frik::inst->isOffHandGrippingWeapon()) ||
   (_view.active[hand]?pose!=Frik::HandPoseKind::Custom:occupiedPose);
  _view.availability[hand]=gestureHandAvailability(frame,interaction,queryOk,inputAvailable,actionHeld,true,poseBusy);
  if(_view.active[hand]) {
   if(_view.availability[hand]!=GestureAvailability::Free)clearHand(owner,hand,gestureAvailabilityText(_view.availability[hand]));
   else if(!publish(owner,hand,frame))_view.availability[hand]=GestureAvailability::Unavailable;
  }
 }
}
const char* Gestures::select(std::uint64_t owner,unsigned choice,const RockProviderFrameSnapshot& frame) {
 if(!isGestureChoice(choice))return "Unknown gesture";
 const unsigned hand=gestureIsLeft(choice)?1:0;
 if(_view.active[hand]==choice){clearHand(owner,hand,"selected again");return "Gesture cleared";}
 if(_view.availability[hand]!=GestureAvailability::Free)
  return _view.availability[hand]==GestureAvailability::Busy?
   (hand?"Left hand is busy - free it before choosing a gesture":"Right hand is busy - free it before choosing a gesture"):
   gestureAvailabilityText(_view.availability[hand]);
 const auto& gesture=kGestures[gestureIndex(choice)];
 Frik::HandPoseData pose;
 std::array<Frik::FingerPoseData*,5> fingers{&pose.thumb,&pose.index,&pose.middle,&pose.ring,&pose.pinky};
 for(unsigned i=0;i<fingers.size();++i)*fingers[i]={gesture.flex[i],gesture.flex[i],gesture.flex[i],gesture.splay[i]};
 Frik::FingerLocalTransformOverride locals;
 // Read-only skeleton math resolves our authored flex/splay for either hand
 // and power armor. Every live pose write and clear goes through ROCK.
 if(!Frik::inst->getHandPoseLocalTransformsForPose(skeletonHand(hand),pose,&locals) ||
    locals.enabledMask!=ROCK_PROVIDER_ALL_FINGER_LOCAL_TRANSFORMS_V1)return "Gesture finger transforms unavailable";
 auto& request=_requests[hand];request={};request.hand=physicalHand(hand);
 request.flags=static_cast<unsigned>(RockProviderHandVisualAuthorityFlagV1::FingerLocalTransforms);
 request.priority=1; // Yield to ordinary interaction poses, including FRIK's internal poses.
 request.leaseFrames=3;request.fingerLocalTransformMask=locals.enabledMask;
 for(unsigned i=0;i<15;++i) {
  const auto& source=locals.localTransforms[i];auto& target=request.fingerLocalTransforms[i];
  for(unsigned row=0;row<3;++row) {
   target.rotate[row*3]=source.rotate.entry[row].x;
   target.rotate[row*3+1]=source.rotate.entry[row].y;
   target.rotate[row*3+2]=source.rotate.entry[row].z;
  }
  target.translate[0]=source.translate.x;target.translate[1]=source.translate.y;target.translate[2]=source.translate.z;target.scale=source.scale;
 }
 _view.active[hand]=choice;
 if(!publish(owner,hand,frame))return "ROCK could not activate this gesture";
 spdlog::info("PALM gesture activated: hand={}, pose={}, frame={}",hand?"left":"right",gesture.name,frame.frameIndex);
 return hand?"Gesture active on left hand":"Gesture active on right hand";
}
}
