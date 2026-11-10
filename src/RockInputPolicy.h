#pragma once
#include "RockFrame.h"
#include <array>

namespace wheel {
// stickUsed: the opening binding uses that hand's stick click ([0] left,
// [1] right). ROCK reserves a stick click per hand, so a click reserved on one
// hand never blocks an opener on the other hand's stick or on other buttons.
inline bool placementAllowsOpening(const rock::api::input::PlacementClickState& state,
 const wheel::RockFrame& frame,const std::array<bool,2>& stickUsed) {
 using Flag=rock::api::input::PlacementClickFlag;
 const bool reserved=(stickUsed[0] && (state.flags&static_cast<std::uint32_t>(Flag::LeftInputReserved))) ||
  (stickUsed[1] && (state.flags&static_cast<std::uint32_t>(Flag::RightInputReserved)));
 return sameRockSample(state.sample,frame) && !reserved;
}
// ROCK owns the enabled setting and contact/latch decision for the primary
// weapon (slot 0); the bipod takes its firing hand's stick click. A failed or
// stale query cannot grant an opening click against an active provider, and an
// unknown firing hand counts as both.
inline bool bipodAllowsOpening(const rock::api::weapon::WeaponState& weapon,
 const wheel::RockFrame& frame,const std::array<bool,2>& stickUsed) {
 using Flag=rock::api::weapon::WeaponStateFlag;
 const bool firingLeft=weapon.firingHand==rock::api::Hand::Left;
 const bool firingKnown=weapon.firingHand==rock::api::Hand::Left || weapon.firingHand==rock::api::Hand::Right;
 const bool stickTaken=firingKnown ? stickUsed[firingLeft?0:1] : (stickUsed[0] || stickUsed[1]);
 return (weapon.flags&static_cast<std::uint32_t>(Flag::Valid)) && sameRockSample(weapon.sample,frame) &&
  !((weapon.flags&static_cast<std::uint32_t>(Flag::BipodInputReserved)) && stickTaken);
}
// Query only physical hands whose trigger participates in the opening binding.
// A current empty hand permits capture; an unknown hand cannot take equip input.
inline bool triggerEquipAllowsOpening(const rock::api::grab::HandInteractionState& state,
 const wheel::RockFrame& frame,rock::api::Hand hand) {
 using Flag=rock::api::grab::HandInteractionFlag;
 return (state.flags&static_cast<std::uint32_t>(Flag::Valid)) && state.hand==hand &&
  state.frameIndex==frame.sample.frameIndex && state.worldGeneration==frame.sample.worldGeneration &&
  state.skeletonGeneration==frame.sample.skeletonGeneration && state.providerGeneration==frame.sample.providerGeneration &&
  !(state.flags&static_cast<std::uint32_t>(Flag::LooseWeapon));
}
// ROCK admits Rocky mode from physical levels. Relinquish PALM even if its
// one-hand hold opened first, and never replay any of the four release edges.
struct RockyInputPriority {
 bool yielding{};
 bool update(const std::array<std::uint64_t,2>& pressed,const std::array<bool,2>& valid) {
  constexpr std::uint64_t chord=(1ull<<2)|(1ull<<33);
  if(valid[0] && valid[1]) {
   if((pressed[0]&chord)==chord && (pressed[1]&chord)==chord)yielding=true;
   else if(((pressed[0]|pressed[1])&chord)==0)yielding=false;
  }
  return yielding;
 }
};
}
