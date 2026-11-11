#include "RockServices.h"
#include <F4SE/Impl/PCH.h>
#include <spdlog/spdlog.h>
#include "Gestures.h"
#include "api/FRIKApiV2.h"

namespace wheel {
namespace {

using Frik=frik::api::FRIKApiV2;
rock::api::Hand physicalHand(unsigned index){return index?rock::api::Hand::Left:rock::api::Hand::Right;}
Frik::Hand skeletonHand(unsigned index){return index?Frik::Hand::Left:Frik::Hand::Right;}
// Buttons that mean the hand is doing something: grip, trigger, A, B/menu, stick click.
constexpr std::array<unsigned,5> kPoseButtons{2u,33u,7u,1u,32u};
}
void Gestures::logInputs(unsigned hand,const char* event,const wheel::RockFrame& frame) const {
 const auto& in=_inputs[hand];const auto& i=in.interaction;
 spdlog::info("PALM gesture {}: hand={} availability={} query={} reportedHand={} phase={} flags={:X} held={} "
  "sample[frame,world,skeleton,provider]=[{},{},{},{}] expected=[{},{},{},{}] reservedHand={} "
  "buttons[grip,trigger,A,B,stick] unavailable={:05b} held={:05b} framePose={} uiBusy={} offhandGrip={}",
  event,hand?"left":"right",gestureAvailabilityText(_view.availability[hand]),in.queryStatus,static_cast<unsigned>(i.hand),
  static_cast<unsigned>(i.phase),i.flags,i.heldBodyCount,i.frameIndex,i.worldGeneration,i.skeletonGeneration,i.providerGeneration,
  frame.sample.frameIndex,frame.sample.worldGeneration,frame.sample.skeletonGeneration,frame.sample.providerGeneration,
  static_cast<unsigned>(frame.reservedHand),in.unavailableButtons,in.heldButtons,in.pose,in.uiBusy,in.offhandGrip);
}
void Gestures::initialize() {
 const auto error=Frik::initialize();
 const auto* api=Frik::inst;
 _available=!error && api && api->isSkeletonReady && api->getHandPoseLocalTransformsForPose &&
  api->getCurrentHandPose && api->isConfigOpen && api->isWristPipboyOpen && api->isOffHandGrippingWeapon &&
  rockServices().animation && rockServices().input && rockServices().grab;
 spdlog::info("PALM gestures: {} (skeleton pose resolver result {})",_available?"available through ROCK":"unavailable",error);
}
void Gestures::clearHand(std::uint64_t owner,unsigned hand,const char* reason) {
 if(!_view.active[hand])return;
 const auto result=rockServices().animation->clearHandVisualAuthority(owner,physicalHand(hand));
 spdlog::info("PALM gesture cleared: hand={}, reason={}, result={}",hand?"left":"right",reason,static_cast<unsigned>(result));
 // Even if explicit cleanup is rejected, stopping renewal expires ROCK's lease.
 _view.active[hand]=0;
}
void Gestures::clear(std::uint64_t owner) {
 for(unsigned hand=0;hand<2;++hand)clearHand(owner,hand,"context/lifecycle changed");
 _view.availability={};
}
bool Gestures::publish(std::uint64_t owner,unsigned hand,const wheel::RockFrame& frame) {
 auto& request=_requests[hand];
 request.worldGeneration=frame.sample.worldGeneration;request.skeletonGeneration=frame.sample.skeletonGeneration;
 request.providerGeneration=frame.sample.providerGeneration;
 const auto result=rockServices().animation->setHandVisualAuthority(owner,&request);
 if(result==rock::api::Status::Ok)return true;
 spdlog::warn("PALM gesture publication rejected: hand={}, result={}",hand?"left":"right",static_cast<unsigned>(result));
 clearHand(owner,hand,"publication rejected");return false;
}
void Gestures::update(std::uint64_t owner,const wheel::RockFrame& frame,bool usable,const std::array<std::uint64_t,2>& wheelButtons) {
 if(!usable || !_available || !frame.frikSkeletonReady || !Frik::inst->isSkeletonReady()) {clear(owner);return;}
 const bool uiBusy=Frik::inst->isConfigOpen() || Frik::inst->isWristPipboyOpen();
 for(unsigned hand=0;hand<2;++hand) {
  auto& in=_inputs[hand];in={};
  auto& interaction=in.interaction;
  in.queryStatus=static_cast<int>(rockServices().grab->getHandInteractionState(owner,physicalHand(hand),&interaction));
  const bool queryOk=in.queryStatus==static_cast<int>(rock::api::Status::Ok) && interaction.hand==physicalHand(hand);
  // UI input never makes a hand busy: the wheel's opening binding through both
  // releases, and the buttons RPS UI captures while the hand points at a
  // panel (ROCK reports those release-to-rearm). Otherwise the hand that
  // points at a gesture could never take it, and choosing would cancel it.
  for(unsigned b=0;b<kPoseButtons.size();++b) {
   const unsigned button=kPoseButtons[b];
   if(wheelButtons[hand]&(std::uint64_t{1}<<button))continue;
   rock::api::input::RawWandButtonState raw;
   if(rockServices().input->getRawWandButtonState(owner,physicalHand(hand),button,&raw)!=rock::api::Status::Ok)
    in.unavailableButtons|=1u<<b;
   else if(!raw.available && raw.availabilityReason!=rock::api::input::InputAvailabilityReason::ReleaseToRearm)
    in.unavailableButtons|=1u<<b;
   if(raw.held)in.heldButtons|=1u<<b;
  }
  const bool inputAvailable=!in.unavailableButtons,actionHeld=in.heldButtons!=0;
  const auto pose=Frik::inst->getCurrentHandPose(skeletonHand(hand));
  in.pose=static_cast<unsigned>(pose);in.uiBusy=uiBusy;
  in.offhandGrip=physicalHand(hand)==primaryOffhand(frame.roles) && Frik::inst->isOffHandGrippingWeapon();
  // Controller-driven pointing/thumbs-up can occur on a free hand. Existing
  // custom claims and weapon grips belong to another interaction.
  const bool occupiedPose=pose==Frik::HandPoseKind::Custom || pose==Frik::HandPoseKind::HoldingWeapon ||
   pose==Frik::HandPoseKind::HoldingGun || pose==Frik::HandPoseKind::HoldingMelee ||
   pose==Frik::HandPoseKind::OffhandGrip || pose==Frik::HandPoseKind::Attaboy;
  const bool poseBusy=uiBusy || in.offhandGrip || (_view.active[hand]?pose!=Frik::HandPoseKind::Custom:occupiedPose);
  _view.availability[hand]=gestureHandAvailability(frame,interaction,queryOk,inputAvailable,actionHeld,true,poseBusy);
  if(_view.active[hand]) {
   if(_view.availability[hand]!=GestureAvailability::Free) {
    logInputs(hand,"interrupted",frame);
    clearHand(owner,hand,gestureAvailabilityText(_view.availability[hand]));
   }
   else if(!publish(owner,hand,frame))_view.availability[hand]=GestureAvailability::Unavailable;
  }
 }
}
const char* Gestures::select(std::uint64_t owner,unsigned choice,const wheel::RockFrame& frame) {
 if(!isGestureChoice(choice))return "Unknown gesture";
 const unsigned hand=gestureIsLeft(choice)?1:0;
 if(_view.active[hand]==choice){clearHand(owner,hand,"selected again");return "Gesture cleared";}
 if(_view.availability[hand]!=GestureAvailability::Free) {
  logInputs(hand,"refused",frame);
  return _view.availability[hand]==GestureAvailability::Busy?
   (hand?"Left hand is busy - free it before choosing a gesture":"Right hand is busy - free it before choosing a gesture"):
   gestureAvailabilityText(_view.availability[hand]);
 }
 const auto& gesture=kGestures[gestureIndex(choice)];
 Frik::HandPoseData pose;
 std::array<Frik::FingerPoseData*,5> fingers{&pose.thumb,&pose.index,&pose.middle,&pose.ring,&pose.pinky};
 for(unsigned i=0;i<fingers.size();++i)*fingers[i]={gesture.flex[i],gesture.flex[i],gesture.flex[i],gesture.splay[i]};
 Frik::FingerLocalTransformOverride locals;
 // Read-only skeleton math resolves our authored flex/splay for either hand
 // and power armor. Every live pose write and clear goes through ROCK.
 if(!Frik::inst->getHandPoseLocalTransformsForPose(skeletonHand(hand),pose,&locals) ||
    locals.enabledMask!=((1u << rock::api::hands::kFingerLocalTransformCount) - 1u)) {
  spdlog::warn("PALM gesture finger transforms unavailable: hand={}, pose={}, mask={:X}",hand?"left":"right",gesture.name,locals.enabledMask);
  return "Gesture finger transforms unavailable";
 }
 auto& request=_requests[hand];request={};request.hand=physicalHand(hand);
 request.flags=static_cast<unsigned>(rock::api::animation::HandVisualAuthorityFlag::FingerLocalTransforms);
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
 spdlog::info("PALM gesture activated: hand={}, pose={}, frame={}",hand?"left":"right",gesture.name,frame.sample.frameIndex);
 return hand?"Gesture active on left hand":"Gesture active on right hand";
}
}
