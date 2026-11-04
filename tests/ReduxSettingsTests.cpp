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
    // Each page shows exactly its group's rows of Redux's catalog, at the module's values.
    const auto indexOf=[](const IniSettingsStore& store,std::string_view key) {
        const auto& rows=store.settings();
        std::size_t index=0;
        while(index<rows.size()&&rows[index].key!=key)++index;
        assert(index<rows.size());
        return index;
    };
    std::uint32_t defaults=0;
    for(const auto& metadata:config::kSettings) {
        if(metadata.defaultValue)defaults|=config::bit(metadata.id);
        const auto& page=metadata.group==config::Group::Consumer?consumer:developer;
        const auto& row=page.settings()[indexOf(page,metadata.key)];
        assert(row.type==SettingType::Boolean&&row.value==(metadata.defaultValue?"true":"false"));
    }
    assert(consumer.settings().size()+developer.settings().size()==config::kSettings.size());
    assert(!consumer.needsReload()&&!developer.needsReload());
    // Writes go through Redux by key, from the page that owns the setting.
    const auto collisionIndex=indexOf(consumer,"npcDynamicCollisions");
    const auto visualizerIndex=indexOf(developer,"bDebugShowColliders");
    const auto collision=consumer.setBooleanByIndex(collisionIndex,false);assert(collision.changed&&collision.saved);
    const auto visualizer=developer.setBooleanByIndex(visualizerIndex,true);assert(visualizer.changed&&visualizer.saved);
    const auto physical=developer.setBooleanByIndex(indexOf(developer,"bPhysicalMelee"),false);assert(physical.changed&&physical.saved);
    config::Snapshot state;assert(api->snapshot(&state)==config::Status::Ok);
#ifdef WHEEL_DESKTOP_PREVIEW
    assert(state.values==defaults&&state.revision==1); // No persistence or live call.
#else
    assert(state.values==((defaults&~config::bit(config::Setting::Collision)&~config::bit(config::Setting::PhysicalMelee))|config::bit(config::Setting::Visualizer)));
    assert(state.revision==4);
    assert(consumer.needsReload()&&developer.needsReload());
    assert(consumer.reload()&&consumer.settings()[collisionIndex].value=="false");
    assert(developer.reload()&&developer.settings()[visualizerIndex].value=="true");
    assert(!developer.needsReload());
#endif
    assert(!std::filesystem::exists("never-read.ini")&&!std::filesystem::exists("never-read-dev.ini"));
    FreeLibrary(module);
}
