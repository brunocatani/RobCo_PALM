#include "IniSettingsStore.h"
#include "EmitterSettings.h"
#include "LoadedRpsMods.h"
#include <Windows.h>
#include <cassert>
#include <chrono>
#include <fstream>

int main(int argc,char** argv)
{
    using namespace rock_configurator;
    namespace fs=std::filesystem;
    assert(argc==3);
    assert(!configurationModLoaded(RpsMod::Emitters));
    const auto module=LoadLibraryW(fs::path(argv[1]).c_str());assert(module);
#ifndef WHEEL_DESKTOP_PREVIEW
    assert(configurationModLoaded(RpsMod::Emitters));
#else
    assert(!configurationModLoaded(RpsMod::Emitters));
#endif
    const auto path=fs::temp_directory_path()/("palm-emitters-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".ini");
    {std::ifstream source(argv[2]);assert(source);std::ofstream out(path);out<<source.rdbuf();out<<"\n; preserve user notes\n[Notes]\nsText = unchanged\n";}
    const auto read=[&]{std::ifstream in(path);return std::string(std::istreambuf_iterator<char>(in),{});};
    const auto before=read();
    IniSettingsStore store(path,RpsMod::Emitters);assert(store.load());assert(read()==before);
    assert(store.settings().size()==emitter_settings::settings.size()+1);
    for(const auto& expected:emitter_settings::settings) {
        const auto index=store.indexForId(std::string("Main.")+expected.key);assert(index);
        const auto& setting=store.settings()[*index];assert(setting.label==expected.label);
        if(expected.type==emitter_settings::Type::Number) {
            assert(setting.type==SettingType::Float&&store.isSlider(*index));
            assert(setting.control.minimum==expected.minimum&&setting.control.maximum==expected.maximum);
        } else if(expected.type==emitter_settings::Type::Toggle) assert(setting.control.kind==setting_control::Kind::Checkbox);
        else assert(setting.control.options.size()==3);
    }
    const auto brightness=*store.indexForId("Main.fReticleBrightness");
    assert(store.setNumericByIndex(brightness,1).saved);
    assert(store.settings()[brightness].type==SettingType::Float); // 1 must not become a checkbox.
    const auto radius=*store.indexForId("Main.fHitLightRadius");assert(store.setNumericByIndex(radius,8).saved);
    const auto enabled=*store.indexForId("Main.bEnabled");assert(store.setBooleanByIndex(enabled,false).saved);
#ifndef WHEEL_DESKTOP_PREVIEW
    assert(configurationModLoaded(RpsMod::Emitters)); // Page remains available to re-enable it.
#endif
    const auto color=*store.indexForId("Main.sLaserColor");assert(store.setOptionByIndex(color,1).saved);
    assert(!store.setOptionByIndex(color,3).saved);
    const auto flashlight=*store.indexForId("Main.fFlashlightIntensity");
    assert(store.setNumericByIndex(flashlight,100).saved&&store.settings()[flashlight].numericValue==20);
    assert(store.setNumericByIndex(flashlight,0).saved&&store.settings()[flashlight].numericValue==0.1);
#ifdef WHEEL_DESKTOP_PREVIEW
    assert(read()==before);
#else
    const auto saved=read();assert(saved.find("; preserve user notes")!=std::string::npos&&saved.find("sText = unchanged")!=std::string::npos);
    IniSettingsStore reopened(path,RpsMod::Emitters);assert(reopened.load());
    assert(reopened.settings()[*reopened.indexForId("Main.fReticleBrightness")].numericValue==1);
    assert(reopened.settings()[*reopened.indexForId("Main.fHitLightRadius")].numericValue==8);
    assert(reopened.settings()[*reopened.indexForId("Main.sLaserColor")].value=="Green");
    assert(reopened.setBooleanByIndex(*reopened.indexForId("Main.bEnabled"),true).saved);
#endif
    assert(FreeLibrary(module));assert(!configurationModLoaded(RpsMod::Emitters));
    fs::remove(path);
}
