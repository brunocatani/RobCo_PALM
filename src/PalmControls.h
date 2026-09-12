#pragma once
#include "InputBindingParser.h"
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <string>

namespace wheel {
enum class OpenMode { Hold, Press };
struct PointerAim {
 std::array<float,2> pitch{},yaw{};
 bool operator==(const PointerAim&) const=default;
};
struct Controls {
 OpenMode mode{OpenMode::Hold};
 f4cf::vrcf::InputBinding binding{.hand=f4cf::vrcf::Hand::Right,
  .type=f4cf::vrcf::ActivationType::HoldDown,.button=vr::k_EButton_SteamVR_Trigger,
  .modifier=f4cf::vrcf::InputModifier{vr::k_EButton_Grip,{}},.duration=.25f,.suppress=true};
 bool operator==(const Controls&) const=default;
};
struct ControlMasks {std::array<std::uint64_t,2> buttons{};};
inline unsigned physicalHand(f4cf::vrcf::Hand hand,bool leftHanded) {
 using f4cf::vrcf::Hand;
 return hand==Hand::Left || (hand==Hand::Primary && leftHanded) || (hand==Hand::Offhand && !leftHanded)?0:1;
}
inline ControlMasks controlMasks(const Controls& controls,bool leftHanded) {
 ControlMasks result;const auto& b=controls.binding;
 result.buttons[physicalHand(b.hand,leftHanded)]|=std::uint64_t{1}<<static_cast<unsigned>(b.button);
 if(b.modifier)result.buttons[physicalHand(b.modifier->hand.value_or(b.hand),leftHanded)]|=std::uint64_t{1}<<static_cast<unsigned>(b.modifier->button);
 return result;
}
enum class ControlEdge {None,Open,Select};
enum class ControlCapture {None,Chord,Buttons};
enum class HoldRejection {None,ExistingButton,Interaction};
inline bool isTimedHold(f4cf::vrcf::ActivationType type) {
 return type==f4cf::vrcf::ActivationType::HoldDown || type==f4cf::vrcf::ActivationType::LongPress;
}
inline double holdDuration(const Controls& controls) {
 return controls.binding.duration>0?controls.binding.duration:
  controls.binding.type==f4cf::vrcf::ActivationType::LongPress?.6:.25;
}
inline bool supportsReleaseSelection(f4cf::vrcf::ActivationType type) {
 return type!=f4cf::vrcf::ActivationType::Tap && type!=f4cf::vrcf::ActivationType::Release;
}
// Frame-owned. Every context/binding change requires a fresh release before
// opening. Timed holds observe gameplay input until qualified; only an acquired
// capture survives a partial chord release until every member is up.
struct ControlGesture {
 bool armed{},pending{},open{},draining{},clickArmed{},waitingDouble{},captured{},assembling{};
 double pressedAt{},firstPressedAt{},firstMemberAt{},lastTime{};
 HoldRejection rejection{};
 bool ownsInput() const {return captured;}
 ControlCapture captureMode(const Controls& controls,bool eligible,bool ownedBefore) const {
  if(ownedBefore || ownsInput())return ControlCapture::Buttons;
  return eligible && armed && !isTimedHold(controls.binding.type)?ControlCapture::Chord:ControlCapture::None;
 }
 ControlEdge activate(){pending=false;waitingDouble=false;draining=false;open=true;captured=true;clickArmed=false;return ControlEdge::Open;}
 ControlEdge update(bool usable,const Controls& controls,bool allDown,bool anyDown,bool click,double now,bool interactionBusy=false) {
  using f4cf::vrcf::ActivationType;
  if(!usable || !std::isfinite(now) || now<lastTime){*this={};return ControlEdge::None;}
  lastTime=now;rejection=HoldRejection::None;const auto type=controls.binding.type;
  const double duration=controls.binding.duration;
  const double doubleWindow=duration>0?duration:.4;
  if(waitingDouble && now-firstPressedAt>doubleWindow){waitingDouble=false;if(!pending){draining=anyDown;armed=!anyDown;captured=anyDown;}}
  if(open) {
   if(controls.mode==OpenMode::Hold && supportsReleaseSelection(type) && !allDown){open=false;draining=anyDown;armed=!anyDown;captured=anyDown;return ControlEdge::Select;}
   if(controls.mode==OpenMode::Press || !supportsReleaseSelection(type)) {
    if(!anyDown && !click)clickArmed=true;
    if(click && clickArmed){clickArmed=false;return ControlEdge::Select;}
   }
   return ControlEdge::None;
  }
  if(draining) {if(!anyDown){draining=false;armed=true;captured=false;}return ControlEdge::None;}
  if(isTimedHold(type)) {
   // Allow natural button skew, but never turn an established gameplay hold
   // into a menu chord by adding its other button later. Either order works.
   constexpr double chordJoinSeconds=.15;
   if(!anyDown)assembling=false;
   else if(armed && !pending && !assembling){assembling=true;firstMemberAt=now;}
   if(interactionBusy || (assembling && now-firstMemberAt>chordJoinSeconds)) {
    rejection=interactionBusy?HoldRejection::Interaction:HoldRejection::ExistingButton;
    pending=false;assembling=false;armed=false;draining=anyDown;captured=false;
    return ControlEdge::None;
   }
  }
  if(!allDown) {
   if(pending) {
    pending=false;draining=anyDown;armed=!anyDown;captured=captured && anyDown;
    const double held=now-pressedAt;
    if(type==ActivationType::DoublePress && waitingDouble){draining=false;armed=true;captured=true;return ControlEdge::None;}
    if((type==ActivationType::Tap && held<.3) || (type==ActivationType::Release && (duration<=0 || held<=duration)))return activate();
   }
   if(!anyDown)armed=true;
   return ControlEdge::None;
  }
  if(!pending) {
   if(!armed)return ControlEdge::None;
   pending=true;armed=false;assembling=false;pressedAt=now;captured=!isTimedHold(type);
   if(type==ActivationType::Press)return activate();
   if(type==ActivationType::DoublePress) {
    if(waitingDouble)return activate();
    waitingDouble=true;firstPressedAt=now;
   }
  }
  if(isTimedHold(type) && now-pressedAt>=holdDuration(controls))return activate();
  return ControlEdge::None;
 }
 void cancel(bool anyDown){open=false;pending=false;waitingDouble=false;clickArmed=false;assembling=false;draining=anyDown;armed=!anyDown;captured=captured && anyDown;}
};
bool parseControls(std::string_view mode,std::string_view binding,Controls& out,std::string& error);
std::string bindingText(const Controls& controls);
Controls snapshotControls();
void initializeControls(const std::filesystem::path& path);
bool applyControls(const Controls& controls,bool queueSave=true); // Memory only; explicit save is queued for the game task.
bool takeControlsSaveRequest();
void persistControls();
void drawControls();
PointerAim snapshotPointerAim();
bool applyPointerAim(const PointerAim& aim,bool commit=true);
bool takePointerAimChange();
void drawPointerAim();
void setGestureIntegrationAvailable(bool available);
bool gestureIntegrationAvailable();
}
