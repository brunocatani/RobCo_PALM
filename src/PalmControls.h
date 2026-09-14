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
// User pitch is relative to the tested forward aim, not the native wand axis.
inline constexpr float nativePointerPitch(float adjustmentDegrees){return -70.f+adjustmentDegrees;}
struct Controls {
 OpenMode mode{OpenMode::Press};
 f4cf::vrcf::InputBinding binding{.hand=f4cf::vrcf::Hand::Right,
  .type=f4cf::vrcf::ActivationType::Release,.button=vr::k_EButton_SteamVR_Touchpad,
  .duration=0,.suppress=true};
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
inline bool isPlainStickClick(const Controls& controls) {
 return controls.mode==OpenMode::Press && controls.binding.type==f4cf::vrcf::ActivationType::Release &&
  controls.binding.button==vr::k_EButton_SteamVR_Touchpad && !controls.binding.modifier && controls.binding.duration==0;
}
inline bool openingInputAllowed(const ControlMasks& masks,const std::array<std::uint64_t,2>& pressed,
 const std::array<bool,2>& valid,const std::array<bool,2>& inHolster) {
 for(unsigned hand=0;hand<2;++hand)
  if(!valid[hand] || (pressed[hand]&~masks.buttons[hand]) || (masks.buttons[hand] && inHolster[hand]))return false;
 return true;
}
inline bool supportsReleaseSelection(f4cf::vrcf::ActivationType type) {
 return type!=f4cf::vrcf::ActivationType::Tap && type!=f4cf::vrcf::ActivationType::Release;
}
// Frame-owned. Every context/binding change requires a fresh release before
// opening. Capture survives a partial chord release until every member is up.
struct ControlGesture {
 bool armed{},pending{},open{},draining{},clickArmed{},waitingDouble{};
 double pressedAt{},firstPressedAt{},lastTime{};
 bool ownsInput() const {return pending || open || draining || waitingDouble;}
 ControlEdge activate(){pending=false;waitingDouble=false;draining=false;open=true;clickArmed=false;return ControlEdge::Open;}
 ControlEdge update(bool usable,const Controls& controls,bool allDown,bool anyDown,bool click,double now,bool openingAllowed=true) {
  using f4cf::vrcf::ActivationType;
  if(!usable || !std::isfinite(now) || now<lastTime){*this={};return ControlEdge::None;}
  // A competing gesture/holster invalidates the entire click. Releasing its
  // other button or leaving its zone while still pressed must not reopen PALM.
  if(!open && !openingAllowed){*this={};return ControlEdge::None;}
  lastTime=now;const auto type=controls.binding.type;
  const double duration=controls.binding.duration;
  const double doubleWindow=duration>0?duration:.4;
  if(waitingDouble && now-firstPressedAt>doubleWindow){waitingDouble=false;if(!pending){draining=anyDown;armed=!anyDown;}}
  if(open) {
   if(controls.mode==OpenMode::Hold && supportsReleaseSelection(type) && !allDown){open=false;draining=anyDown;armed=!anyDown;return ControlEdge::Select;}
   if(controls.mode==OpenMode::Press || !supportsReleaseSelection(type)) {
    if(!anyDown && !click)clickArmed=true;
    if(click && clickArmed){clickArmed=false;return ControlEdge::Select;}
   }
   return ControlEdge::None;
  }
  if(draining) {if(!anyDown){draining=false;armed=true;}return ControlEdge::None;}
  if(!allDown) {
   if(pending) {
    pending=false;draining=anyDown;armed=!anyDown;
    const double held=now-pressedAt;
    if(type==ActivationType::DoublePress && waitingDouble){draining=false;armed=true;return ControlEdge::None;}
    if((type==ActivationType::Tap && held<.3) || (type==ActivationType::Release && (duration<=0 || held<=duration)))return activate();
   }
   if(!anyDown)armed=true;
   return ControlEdge::None;
  }
  if(!pending) {
   if(!armed)return ControlEdge::None;
   pending=true;armed=false;pressedAt=now;
   if(type==ActivationType::Press)return activate();
   if(type==ActivationType::DoublePress) {
    if(waitingDouble)return activate();
    waitingDouble=true;firstPressedAt=now;
   }
  }
  if(type==ActivationType::HoldDown && now-pressedAt>=duration)return activate();
  if(type==ActivationType::LongPress && now-pressedAt>=(duration>0?duration:.6))return activate();
  return ControlEdge::None;
 }
 void cancel(bool anyDown){open=false;pending=false;waitingDouble=false;clickArmed=false;draining=anyDown;armed=!anyDown;}
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
