#pragma once
#include <array>
#include <cstdint>
#include <string_view>

namespace rock_configurator {
enum class RpsMod : std::uint8_t { Rock, Paper, Scissors, RockDeveloper };
struct RpsModInfo {
 RpsMod id;
 const char* name;
 const wchar_t* module;
 const char* directory;
 const char* ini;
 const char* applyHint;
};
inline constexpr std::array kRpsMods{
 RpsModInfo{RpsMod::Rock,"ROCK",L"ROCK.dll","Mods_Config/ROCK","ROCK.ini","Consumer options are saved to ROCK.ini"},
 RpsModInfo{RpsMod::Paper,"PAPER",L"PAPER.dll","PAPER_Config","PAPER.ini","Restart to apply; live reload requires PAPER's opt-in watcher"},
 RpsModInfo{RpsMod::Scissors,"SCISSORS",L"SCISSORS.dll","SCISSORS_Config","SCISSORS.ini","Changes are saved for SCISSORS' configuration watcher"},
 RpsModInfo{RpsMod::RockDeveloper,"Developer",L"ROCK.dll","Mods_Config/ROCK","ROCK_Developer.ini","Only changes from defaults are saved to ROCK_Developer.ini"}
};
inline constexpr const RpsModInfo& modInfo(RpsMod mod){return kRpsMods[static_cast<std::size_t>(mod)];}
}
