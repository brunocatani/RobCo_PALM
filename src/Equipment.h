#pragma once
#include "WheelModel.h"
#include "EquipmentRuntimePolicy.h"
namespace wheel {
// Initialized once before input subscription. Each rejected native operation is
// isolated; menu startup does not depend on equipment capabilities.
void initializeEquipmentRuntime() noexcept;
EquipmentSupport equipmentSupport() noexcept;
// An unequipped weapon stack for ROCK to equip into a physical hand.
struct RockEquip {
 std::uint32_t form{},stackIndex{};
 PointerHand hand{};
 // Packed for one atomic hand-off from the game task to ROCK's owner thread; 0 is none.
 std::uint64_t pack() const noexcept {return static_cast<std::uint64_t>(form)<<32|static_cast<std::uint64_t>(stackIndex)<<2|static_cast<std::uint64_t>(hand);}
 static RockEquip unpack(std::uint64_t value) noexcept {
  return {static_cast<std::uint32_t>(value>>32),static_cast<std::uint32_t>(value>>2)&0x3fffffffu,static_cast<PointerHand>(value&3u)};
 }
};
// Game-thread only; re-resolves the selected variant and reads its current equip state.
// With ROCK (owner set) an unequipped weapon is returned in outRock for
// submitRockEquip, which puts it into `hand` and replaces that hand's weapon.
// Equipped items and armor toggle natively.
const char* toggleEquipment(const Item& selected, std::uint64_t owner, PointerHand hand, RockEquip& outRock) noexcept;
// ROCK's owner thread (the input frame callback): capture and request the equip.
const char* submitRockEquip(std::uint64_t owner, const RockEquip& equip, std::uint64_t& outCommand) noexcept;
const char* useInventoryItem(std::uint32_t id) noexcept;
// Game-thread only: adds the exact inventory variant equipped at native equip
// index `equipIndex` (0: the player's own weapon, 1: ROCK's second weapon) to
// the wheel's weapon ring at `slot`; returns the status text.
const char* addHeldWeaponToWheel(std::uint32_t equipIndex, unsigned slot) noexcept;
}
