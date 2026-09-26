#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <span>

namespace wheel {
struct EquipmentSupport {
 bool equip{},unequip{},names{};
 bool canToggle(bool equipped) const noexcept {return names && (equipped?unequip:equip);}
};
namespace equipment_policy {
// FO4VR 1.2.72. Heisenberg copies the first 17 bytes into its trampoline,
// replaces them with FF 25 + an absolute target + three NOPs, and leaves the
// following native instructions intact. Other modifications remain unavailable.
inline constexpr std::array<std::uint8_t,32> equipEntry{
 0x4c,0x8b,0xdc,0x49,0x89,0x53,0x10,0x55,0x56,0x41,0x54,0x41,0x57,0x49,0x8d,0x6b,
 0xd9,0x48,0x81,0xec,0xa8,0x00,0x00,0x00,0x45,0x8b,0xe1,0x49,0x8b,0xf0,0x48,0x8b};
inline constexpr std::array<std::uint8_t,8> unequipEntry{0x48,0x8b,0xc4,0x48,0x89,0x58,0x18,0x55};
inline constexpr std::array<std::uint8_t,15> nameEntry{0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x57,0x48,0x83,0xec,0x30};
inline bool prefixMatches(std::span<const std::uint8_t> actual,std::span<const std::uint8_t> expected) noexcept {
 return actual.size()>=expected.size() && std::equal(expected.begin(),expected.end(),actual.begin());
}
inline bool heisenbergEquipJump(std::span<const std::uint8_t> actual) noexcept {
 constexpr std::array<std::uint8_t,6> jump{0xff,0x25,0,0,0,0};
 return actual.size()>=equipEntry.size() && prefixMatches(actual,jump) &&
  actual[14]==0x90 && actual[15]==0x90 && actual[16]==0x90 &&
  prefixMatches(actual.subspan(17),std::span(equipEntry).subspan(17));
}
enum class EquipEntry {Unavailable,Native,Heisenberg};
inline EquipEntry classifyEquipEntry(std::span<const std::uint8_t> actual,bool heisenbergExecutableTarget) noexcept {
 if(prefixMatches(actual,std::span(equipEntry).first(8)))return EquipEntry::Native;
 if(heisenbergExecutableTarget && heisenbergEquipJump(actual))return EquipEntry::Heisenberg;
 return EquipEntry::Unavailable;
}
}
}
