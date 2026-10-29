#include "IniSettingsStore.h"
#include <Windows.h>
#include <array>

namespace rock_configurator {
    bool IniSettingsStore::loadReduxSettings() {
        namespace config=scissors::configuration;
#ifndef WHEEL_DESKTOP_PREVIEW
        if(!_reduxApi) {
            const auto module=GetModuleHandleW(L"SCISSORS_Redux.dll");
            const auto query=module?reinterpret_cast<config::Query>(GetProcAddress(module,config::kQueryExport)):nullptr;
            if(query)_reduxApi=query(config::kMajor,config::kMinor,sizeof(config::Api));
        }
#endif
        if(!_reduxApi || _reduxApi->major!=config::kMajor || _reduxApi->minor<config::kMinor || _reduxApi->size<sizeof(config::Api) ||
            !_reduxApi->snapshot || !_reduxApi->setValue) {
            _reduxApi=nullptr;_lastError="SCISSORS Redux Configuration 2.0 is unavailable";return false;
        }
        config::Snapshot snapshot;
        if(_reduxApi->snapshot(&snapshot)!=config::Status::Ok) {
            _lastError="SCISSORS Redux configuration is not ready";return false;
        }
        const auto group=_mod==RpsMod::ScissorsReduxDeveloper?config::Group::Developer:config::Group::Consumer;
        std::vector<SettingRecord> settings;
        for(const auto& metadata:config::kSettings) {
            if(metadata.group!=group)continue;
            SettingRecord setting;
            setting.section="PhysicsInteraction";setting.key=metadata.key;setting.id=setting.section+"."+setting.key;
            setting.value=(snapshot.values&config::bit(metadata.id))?"true":"false";
            setting.defaultValue=metadata.defaultValue?"true":"false";
            setting.category=group==config::Group::Consumer?"NPC interactions":"NPC diagnostics and experiments";
            setting.description=metadata.description;setting.overridden=setting.value!=setting.defaultValue;
            setting.type=SettingType::Boolean;
            refreshControl(setting);settings.push_back(std::move(setting));
        }
        _settings=std::move(settings);_loadedRevision=snapshot.revision;return true;
    }
    bool IniSettingsStore::saveReduxSetting(const SettingRecord& setting,bool value) {
        namespace config=scissors::configuration;
        const auto group=_mod==RpsMod::ScissorsReduxDeveloper?config::Group::Developer:config::Group::Consumer;
        for(const auto& metadata:config::kSettings) {
            if(metadata.group!=group || setting.key!=metadata.key)continue;
#ifndef WHEEL_DESKTOP_PREVIEW
            std::array<char,512> error{};
            if(!_reduxApi || _reduxApi->setValue(metadata.id,value?1u:0u,error.data(),static_cast<std::uint32_t>(error.size()))!=config::Status::Ok) {
                _lastError=error[0]?error.data():"SCISSORS Redux configuration write failed";return false;
            }
#else
            (void)value; // Preview edits are memory-only, even with an injected provider.
#endif
            return true;
        }
        _lastError="Unknown SCISSORS Redux setting";return false;
    }
}
