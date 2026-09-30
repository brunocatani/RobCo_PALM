#include "IniSettingsStore.h"
#include <DIAMOND/Configuration.h>
#include <Windows.h>
#include <cassert>
#include <fstream>
#include <chrono>

int main(int argc,char** argv) {
    using namespace rock_configurator;namespace fs=std::filesystem;
    assert(argc==2);const auto module=LoadLibraryW(fs::path(argv[1]).c_str());assert(module);
    const auto calls=reinterpret_cast<unsigned(*)() noexcept>(GetProcAddress(module,"DIAMONDTestReloadCount"));
    const auto accept=reinterpret_cast<void(*)(bool) noexcept>(GetProcAddress(module,"DIAMONDTestAcceptReload"));assert(calls&&accept);
    const auto path=fs::temp_directory_path()/("palm-diamond-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".ini");
    {std::ofstream out(path);out<<"; retained user comment\n";std::string_view section;
        for(const auto& setting:diamond::configuration::kSettings) {
            if(section!=setting.section){section=setting.section;out<<'['<<section<<"]\n";}
            out<<setting.key<<" = "<<setting.defaultValue<<'\n';
        }
        out<<"[UserNotes]\nsNote = keep this\n";
    }
    const auto read=[&]{std::ifstream in(path);return std::string(std::istreambuf_iterator<char>(in),{});};
    const auto before=read();IniSettingsStore store(path,RpsMod::Diamond);assert(store.load());
    assert(std::wstring_view(modInfo(RpsMod::Diamond).module)==L"DIAMOND.dll");
    const auto height=store.indexForId("Pouch.fUpGame"),rotation=store.indexForId("Pouch.fRotationXDegrees"),enabled=store.indexForId("Pouch.bEnabled");
    assert(height&&rotation&&enabled&&store.settings().size()==diamond::configuration::kSettings.size()+1);
    assert(store.settings()[*height].label=="Height"&&store.isSlider(*height));
    assert(store.settings()[*rotation].type==SettingType::Float&&store.settings()[*rotation].control.minimum==-180);
    assert(store.setNumericByIndex(*height,1000).saved&&store.settings()[*height].numericValue==80); // PALM clamps slider edits.
    assert(store.setNumericByIndex(*height,12.5).saved);
    assert(store.setBooleanByIndex(*enabled,false).saved);
#ifdef WHEEL_DESKTOP_PREVIEW
    assert(calls()==0&&read()==before);
#else
    assert(calls()==3);const auto saved=read();assert(saved.find("fUpGame = 12.5")!=std::string::npos);
    assert(saved.find("; retained user comment")!=std::string::npos&&saved.find("sNote = keep this")!=std::string::npos);
    accept(false);const auto result=store.setNumericByIndex(*rotation,25);
    assert(result.saved&&result.message.find("could not apply live")!=std::string::npos&&calls()==4);
#endif
    assert(FreeLibrary(module));fs::remove(path);
}
