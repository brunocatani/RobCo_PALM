#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

namespace rock_configurator {
enum class RpsMod : std::uint8_t { Rock, Paper, Scissors, RockDeveloper, Csah, RockV2, PaperV2, RockV2Developer };
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
 RpsModInfo{RpsMod::Paper,"PAPER",L"PAPER.dll","Mods_Config/PAPER","PAPER.ini","Restart to apply; live reload requires PAPER's opt-in watcher"},
 RpsModInfo{RpsMod::Scissors,"SCISSORS",L"SCISSORS.dll","Mods_Config/SCISSORS","SCISSORS.ini","Changes are saved for SCISSORS' configuration watcher"},
 RpsModInfo{RpsMod::RockDeveloper,"ROCK Dev",L"ROCK.dll","Mods_Config/ROCK","ROCK_Developer.ini","Only changes from defaults are saved to ROCK_Developer.ini"},
 RpsModInfo{RpsMod::Csah,"CSAH",L"CSAH.dll","Mods_Config/CSAH","CSAH.ini","Changes apply through CSAH's watcher; controls marked Restart apply next launch"},
 RpsModInfo{RpsMod::RockV2,"ROCK V2",L"ROCK_V2.dll","Mods_Config/ROCK_V2","ROCK_V2.ini","Consumer options are saved by ROCK V2 to ROCK_V2.ini"},
 RpsModInfo{RpsMod::PaperV2,"PAPER V2",L"PAPER_V2.dll","Mods_Config/PAPER_V2","PAPER_V2.ini","Changes are saved for PAPER V2's configuration watcher"},
 RpsModInfo{RpsMod::RockV2Developer,"ROCK V2 Dev",L"ROCK_V2.dll","Mods_Config/ROCK_V2","ROCK_V2_Developer.ini","Only changes from defaults are saved by ROCK V2 to ROCK_V2_Developer.ini"}
};
inline constexpr const RpsModInfo& modInfo(RpsMod mod){return kRpsMods[static_cast<std::size_t>(mod)];}
inline constexpr std::array kRpsModDisplayOrder{
 RpsMod::Rock,RpsMod::RockDeveloper,RpsMod::RockV2,RpsMod::RockV2Developer,
 RpsMod::Paper,RpsMod::PaperV2,RpsMod::Scissors,RpsMod::Csah
};
inline constexpr RpsMod configurationFamily(RpsMod mod) {
 switch(mod) {
 case RpsMod::RockV2: return RpsMod::Rock;
 case RpsMod::RockV2Developer: return RpsMod::RockDeveloper;
 case RpsMod::PaperV2: return RpsMod::Paper;
 default: return mod;
 }
}
inline constexpr bool isRockConfiguration(RpsMod mod) {
 return configurationFamily(mod)==RpsMod::Rock || configurationFamily(mod)==RpsMod::RockDeveloper;
}
inline constexpr bool isV2Rock(RpsMod mod) { return mod==RpsMod::RockV2 || mod==RpsMod::RockV2Developer; }
// Item/gesture integration follows V2 when loaded. Configuration pages always
// bind their explicitly named module, even if both variants are present.
inline constexpr std::optional<RpsMod> selectLoadedRock(bool released, bool v2) {
 if(v2) return RpsMod::RockV2;
 if(released) return RpsMod::Rock;
 return std::nullopt;
}
}
