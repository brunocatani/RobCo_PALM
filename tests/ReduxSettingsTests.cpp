#include "IniSettingsStore.h"
#include <Windows.h>
#include <cassert>
int main(int argc,char** argv) {
    using namespace rock_configurator;
    namespace config=scissors::configuration;
    assert(argc==2);
    assert(!isRockConfiguration(RpsMod::ScissorsRedux)&&!isRockConfiguration(RpsMod::ScissorsReduxDeveloper));
    assert(std::wstring_view(modInfo(RpsMod::ScissorsRedux).module)==L"SCISSORS_Redux.dll");
    assert(std::string_view(modInfo(RpsMod::ScissorsReduxDeveloper).ini)=="SCISSORS_Redux_Developer.ini");
    IniSettingsStore absent("never-read.ini",RpsMod::ScissorsRedux);assert(!absent.load());
    const auto module=LoadLibraryA(argv[1]);assert(module);
    const auto query=reinterpret_cast<config::Query>(GetProcAddress(module,config::kQueryExport));assert(query);
    assert(!query(1,1,32)); // Major 1 is retired.
    const auto* api=query(config::kMajor,config::kMinor,sizeof(config::Api));assert(api);
#ifdef WHEEL_DESKTOP_PREVIEW
    // A loaded real module is never auto-bound by preview.
    assert(!absent.load());
    const auto* fixture=api;
#else
    const config::Api* fixture=nullptr; // Exercise real named-module discovery.
#endif
    IniSettingsStore consumer("never-read.ini",RpsMod::ScissorsRedux,nullptr,{},0,fixture);
    IniSettingsStore developer("never-read-dev.ini",RpsMod::ScissorsReduxDeveloper,nullptr,{},0,fixture);
    assert(consumer.load()&&developer.load());
    assert(consumer.settings().size()==2&&developer.settings().size()==6);
    assert(consumer.settings()[0].key=="npcDynamicCollisions"&&consumer.settings()[0].value=="true");
    for(const auto& setting:developer.settings())assert(setting.type==SettingType::Boolean&&setting.value=="false");
    assert(developer.settings()[4].key=="bPhysicalMelee");
    assert(!consumer.needsReload()&&!developer.needsReload());
    const auto collision=consumer.setBooleanByIndex(0,false);assert(collision.changed&&collision.saved);
    const auto visualizer=developer.setBooleanByIndex(0,true);assert(visualizer.changed&&visualizer.saved);
    const auto physical=developer.setBooleanByIndex(4,true);assert(physical.changed&&physical.saved);
    config::Snapshot state;assert(api->snapshot(&state)==config::Status::Ok);
#ifdef WHEEL_DESKTOP_PREVIEW
    assert(state.values==1&&state.revision==1); // No persistence or live call.
#else
    assert(state.values==68&&state.revision==4);
    assert(consumer.needsReload()&&developer.needsReload());
    assert(consumer.reload()&&consumer.settings()[0].value=="false");
    assert(developer.reload()&&developer.settings()[0].value=="true");
    assert(!developer.needsReload());
#endif
    assert(!std::filesystem::exists("never-read.ini")&&!std::filesystem::exists("never-read-dev.ini"));
    FreeLibrary(module);
}
