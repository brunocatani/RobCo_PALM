#include <F4SE/Impl/PCH.h>
#include "Gestures.h"
#include "api/FRIKApiV2.h"
#include "WheelView.h"
#include "WheelSelectionState.h"
#include <iostream>
#include <stdexcept>

namespace {
using namespace wheel;
using namespace rock::provider;
using Frik=frik::api::FRIKApiV2;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
struct Fixture {
 RockProviderFrameSnapshot frame;
 std::array<RockProviderHandInteractionStateV1,2> hands;
 std::array<Frik::HandPoseKind,2> poses{};
 std::array<RockProviderHandVisualAuthorityRequestV1,2> published;
 std::array<unsigned,2> writes{},clears{};
 std::array<std::array<bool,64>,2> buttons{};
 bool rawAvailable{true},queryOk{true},skeletonReady{true},menu{},rejectPublication{};
} f;
unsigned index(RockProviderHand hand){return hand==RockProviderHand::Left?1:0;}
unsigned index(Frik::Hand hand){return hand==Frik::Hand::Left?1:0;}
void checkRuntime() {
 RockProviderApi rock{};Frik frik{};
 RockProviderApi::inst=&rock;Frik::inst=&frik;
 rock.getProviderLimitsV1=+[](RockProviderLimitsV1* out){out->providerApiByteSize=sizeof(RockProviderApi);
  out->featureBits=static_cast<unsigned>(RockProviderFeatureBitV1::HandVisualAuthority);return true;};
 rock.getHandInteractionStateV1=+[](std::uint64_t,RockProviderHand hand,RockProviderHandInteractionStateV1* out){
  *out=f.hands[index(hand)];return f.queryOk?RockProviderResultV1::Ok:RockProviderResultV1::NotReady;};
 rock.getRawWandButtonStateV1=+[](RockProviderHand hand,std::uint32_t button,RockProviderRawWandButtonStateV1* out){
  out->available=f.rawAvailable;out->held=f.buttons[index(hand)][button];return true;};
 rock.setHandVisualAuthorityV1=+[](std::uint64_t,const RockProviderHandVisualAuthorityRequestV1* request){
  const auto hand=index(request->hand);++f.writes[hand];f.published[hand]=*request;
  if(f.rejectPublication)return RockProviderResultV1::TargetUnavailable;
  f.poses[hand]=Frik::HandPoseKind::Custom;return RockProviderResultV1::Ok;};
 rock.clearHandVisualAuthorityV1=+[](std::uint64_t,RockProviderHand hand){
  ++f.clears[index(hand)];f.poses[index(hand)]=Frik::HandPoseKind::Unset;return RockProviderResultV1::Ok;};
 frik.isSkeletonReady=+[]{return f.skeletonReady;};frik.isConfigOpen=+[]{return f.menu;};
 frik.isWristPipboyOpen=+[]{return false;};frik.isOffHandGrippingWeapon=+[]{return false;};
 frik.getCurrentHandPose=+[](Frik::Hand hand){return f.poses[index(hand)];};
 frik.getHandPoseLocalTransformsForPose=+[](Frik::Hand hand,const Frik::HandPoseData& pose,Frik::FingerLocalTransformOverride* out){
  out->enabledMask=0x7fff;
  for(auto& transform:out->localTransforms){transform.rotate.entry[0].x=1;transform.rotate.entry[1].y=1;
   transform.rotate.entry[2].z=1;transform.scale=1;transform.translate.x=index(hand)?-1.f:1.f;}
  out->localTransforms[6].translate.y=pose.middle.prox;return true;};
 f.frame.frameIndex=12;f.frame.worldGeneration=2;f.frame.skeletonGeneration=3;f.frame.providerGeneration=4;f.frame.frikSkeletonReady=1;
 for(unsigned hand=0;hand<2;++hand) {
  auto& state=f.hands[hand];state.hand=hand?RockProviderHand::Left:RockProviderHand::Right;
  state.flags=static_cast<unsigned>(RockProviderHandInteractionFlagV1::Valid);state.frameIndex=12;
  state.worldGeneration=2;state.skeletonGeneration=3;state.providerGeneration=4;
 }
 Gestures gestures;gestures.initialize();
 const auto tick=[&]{gestures.update(42,f.frame,true);};
 const auto activate=[&](bool left,unsigned gesture=1){tick();(void)gestures.select(42,gestureChoice(gesture,left),f.frame);
  require(gestures.view().active[left?1:0]!=0,"free hand did not activate");};
 activate(true);activate(false);
 require(f.published[1].fingerLocalTransforms[6].translate[1]==1 && f.published[0].fingerLocalTransforms[0].translate[0]==1 &&
  f.published[1].fingerLocalTransforms[0].translate[0]==-1,"custom pose or physical hand changed");
 require(f.published[0].flags==static_cast<unsigned>(RockProviderHandVisualAuthorityFlagV1::FingerLocalTransforms) &&
  f.published[0].fingerLocalTransformMask==0x7fff && f.published[0].leaseFrames==3 && f.published[0].worldGeneration==2,
  "finger-only publication lost its full mask or lease guards");
 auto writes=f.writes;tick();require(f.writes[0]>writes[0] && f.writes[1]>writes[1],"active leases were not renewed");
 f.buttons[1][2]=true;tick();require(!gestures.view().active[1] && gestures.view().active[0],"grab attempt did not clear just its hand");
 f.buttons[1][2]=false;tick();require(!gestures.view().active[1],"gesture resumed after action release");
 for(unsigned button:{2u,33u,7u,1u,32u}) {
  activate(true);f.buttons[1][button]=true;tick();require(!gestures.view().active[1],"hand action did not cancel");f.buttons[1][button]=false;
 }
 gestures.clear(42);
 f.poses[1]=Frik::HandPoseKind::ThumbsUp;activate(true);gestures.clear(42);
 for(auto pose:{Frik::HandPoseKind::HoldingWeapon,Frik::HandPoseKind::HoldingGun,Frik::HandPoseKind::HoldingMelee,
  Frik::HandPoseKind::OffhandGrip,Frik::HandPoseKind::Custom}) {
  f.poses[1]=pose;tick();(void)gestures.select(42,gestureChoice(0,true),f.frame);
  require(!gestures.view().active[1],"existing weapon or custom pose claim was replaced");
 }
 f.poses[1]=Frik::HandPoseKind::Unset;
 for(auto phase:{RockProviderHandInteractionPhaseV1::Pulling,RockProviderHandInteractionPhaseV1::Catching,
  RockProviderHandInteractionPhaseV1::Holding,RockProviderHandInteractionPhaseV1::Releasing}) {
  f.hands[1].phase=phase;tick();(void)gestures.select(42,gestureChoice(0,true),f.frame);
  require(!gestures.view().active[1] && gestures.view().availability[1]==GestureAvailability::Busy,"busy phase accepted a pose");
 }
 for(auto phase:{RockProviderHandInteractionPhaseV1::Idle,RockProviderHandInteractionPhaseV1::Touching,RockProviderHandInteractionPhaseV1::Selecting}) {
  f.hands[1].phase=phase;activate(true);gestures.clear(42);
 }
 f.hands[1].phase=RockProviderHandInteractionPhaseV1::Idle;
 for(auto flag:{RockProviderHandInteractionFlagV1::FiringGrip,RockProviderHandInteractionFlagV1::PartGrip,
  RockProviderHandInteractionFlagV1::PartCarry,RockProviderHandInteractionFlagV1::TouchGrab,RockProviderHandInteractionFlagV1::TransitionSuppressed}) {
  activate(true);f.hands[1].flags|=static_cast<unsigned>(flag);tick();require(!gestures.view().active[1],"weapon/surface ownership did not cancel");
  f.hands[1].flags&=~static_cast<unsigned>(flag);
 }
 activate(true);f.menu=true;tick();require(!gestures.view().active[1],"Pipboy/config context did not clear");f.menu=false;
 activate(true);f.rawAvailable=false;tick();require(!gestures.view().active[1],"lost controller did not clear");f.rawAvailable=true;
 activate(true);f.queryOk=false;tick();require(!gestures.view().active[1],"failed hand query did not clear");f.queryOk=true;
 activate(true);++f.hands[1].skeletonGeneration;tick();require(!gestures.view().active[1],"stale hand state did not clear");--f.hands[1].skeletonGeneration;
 activate(true);f.poses[1]=Frik::HandPoseKind::Pointing;tick();require(!gestures.view().active[1],"interaction pose did not take priority");
 activate(true);f.skeletonReady=false;tick();require(!gestures.view().active[1],"lost skeleton did not clear");f.skeletonReady=true;
 activate(true);gestures.update(42,f.frame,false);require(!gestures.view().active[1],"session reset did not clear");
 activate(true);(void)gestures.select(42,gestureChoice(1,true),f.frame);require(!gestures.view().active[1],"same gesture did not toggle off");
 tick();f.rejectPublication=true;(void)gestures.select(42,gestureChoice(1,true),f.frame);
 require(!gestures.view().active[1],"rejected publication remained active");
 RockProviderApi::inst=nullptr;Frik::inst=nullptr;
}
void checkSelection() {
 ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.DisplaySize={1024,1024};io.DeltaTime=1.f/90;
 unsigned char* pixels;int width,height;io.Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);
 Model model;View view;model.gestures.availability.fill(GestureAvailability::Free);
 const auto draw=[&](float x,float y){io.AddMousePosEvent(x,y);ImGui::NewFrame();auto action=drawWheel(model,view);ImGui::Render();return action;};
 for(bool left:{false,true}) {
  (void)draw(512+(left?-119.f:119.f),510);
  require(model.gestures.showing && model.gestures.left==left,"physical-side gesture entry did not select its hand");
  for(unsigned slot=0;slot<kGestureCount;++slot) {
   const auto mid=-kPi/2+slot*kPi/4;
   const auto action=draw(512+std::cos(mid)*258,510+std::sin(mid)*258);
   require(action.hoveredGesture==gestureChoice(slot,left) && !action.hoveredItem && !action.configHovered,"pose routed into wrong action");
   WheelSelectionState selection;selection.begin(8);require(selection.publish(8,action),"selection publication failed");
   require(selection.release()->hoveredGesture==action.hoveredGesture,"release lost selected hand/pose");
  }
 }
 require(!draw(512,510).hoveredGesture,"cancel selected a gesture");
 for(unsigned navigation=0;navigation<kNavigationCount;++navigation) {
  const int category=navigationCategory(navigation);if(category<0)continue;
  const auto angle=-kPi/2+navigation*2*kPi/kNavigationCount;
  (void)draw(512+std::cos(angle)*119,510+std::sin(angle)*119);
  require(!model.gestures.showing && static_cast<int>(model.category)==category,"inventory navigation changed its category meaning");
 }
 ImGui::DestroyContext();
}
}
int main(){try{checkRuntime();checkSelection();std::cout<<"Gesture runtime and wheel selection passed\n";return 0;}
 catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
