#pragma once
#include "RpsMod.h"
#ifndef WHEEL_DESKTOP_PREVIEW
#include <Windows.h>
#endif

namespace rock_configurator {
inline std::optional<RpsMod> loadedRockMod() noexcept {
#ifndef WHEEL_DESKTOP_PREVIEW
 return selectLoadedRock(GetModuleHandleW(modInfo(RpsMod::Rock).module)!=nullptr,
     GetModuleHandleW(modInfo(RpsMod::RockV2).module)!=nullptr);
#else
 return std::nullopt;
#endif
}
}
