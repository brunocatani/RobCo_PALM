#pragma once
#include "WheelPreferences.h"
#include <span>

namespace wheel {
// Persist a named loadout variant, not an inventory-list index or engine pointer.
// Reordering stacks or changing load order must not retarget a saved choice.
inline std::string equipmentKey(std::string_view base, std::string_view name, std::span<const std::string> modifiers) {
 if(base.empty())return {};
 std::uint64_t hash=14695981039346656037ull;
 const auto mix=[&](std::string_view value){for(unsigned char c:value){hash^=c;hash*=1099511628211ull;}hash^=0xff;hash*=1099511628211ull;};
 mix(name);for(const auto& modifier:modifiers)mix(modifier);
 char suffix[32];std::snprintf(suffix,sizeof(suffix),"|variant:%016llX",static_cast<unsigned long long>(hash));
 return std::string(base)+suffix;
}
}
