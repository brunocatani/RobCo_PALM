#pragma once
#include <ROCKProviderApi.h>

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
}
