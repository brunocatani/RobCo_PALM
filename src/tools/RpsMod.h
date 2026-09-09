#pragma once
#include <array>
#include <cstdint>
#include <string_view>

namespace rock_configurator {
enum class RpsMod : std::uint8_t { Rock, Paper, Scissors };
struct RpsModInfo {
 RpsMod id;
 const char* name;
 const wchar_t* module;
 const char* directory;
 const char* ini;
 const char* applyHint;
};
inline constexpr std::array kRpsMods{
 RpsModInfo{RpsMod::Rock,"ROCK",L"ROCK.dll","ROCK_Config","ROCK.ini","Changes are saved to ROCK.ini"},
 RpsModInfo{RpsMod::Paper,"PAPER",L"PAPER.dll","PAPER_Config","PAPER.ini","Restart to apply; live reload requires PAPER's opt-in watcher"},
 RpsModInfo{RpsMod::Scissors,"SCISSORS",L"SCISSORS.dll","SCISSORS_Config","SCISSORS.ini","Changes are saved for SCISSORS' configuration watcher"}
};
inline constexpr const RpsModInfo& modInfo(RpsMod mod){return kRpsMods[static_cast<std::size_t>(mod)];}
}
