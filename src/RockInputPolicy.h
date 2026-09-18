#pragma once
#include <ROCKProviderApi.h>
#include <array>

namespace wheel {
// ROCK owns the enabled setting and contact/latch decision. A failed or stale
// query cannot grant an opening click against an active provider.
inline bool bipodAllowsOpening(const rock::provider::RockProviderEquippedWeaponStateV1& weapon,
 const rock::provider::RockProviderFrameSnapshot& frame) {
 using Flag=rock::provider::RockProviderEquippedWeaponStateFlagV1;
 return (weapon.flags&static_cast<std::uint32_t>(Flag::Valid)) &&
  weapon.frameIndex==frame.frameIndex && weapon.worldGeneration==frame.worldGeneration &&
  weapon.skeletonGeneration==frame.skeletonGeneration && weapon.providerGeneration==frame.providerGeneration &&
  !(weapon.flags&static_cast<std::uint32_t>(Flag::BipodInputReserved));
}
// Query only physical hands whose trigger participates in the opening binding.
// A current empty hand permits capture; an unknown hand cannot take equip input.
inline bool triggerEquipAllowsOpening(const rock::provider::RockProviderHandInteractionStateV1& state,
 const rock::provider::RockProviderFrameSnapshot& frame,rock::provider::RockProviderHand hand) {
 using Flag=rock::provider::RockProviderHandInteractionFlagV1;
 return (state.flags&static_cast<std::uint32_t>(Flag::Valid)) && state.hand==hand &&
  state.frameIndex==frame.frameIndex && state.worldGeneration==frame.worldGeneration &&
  state.skeletonGeneration==frame.skeletonGeneration && state.providerGeneration==frame.providerGeneration &&
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
