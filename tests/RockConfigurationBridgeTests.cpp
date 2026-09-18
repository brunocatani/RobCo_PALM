#include "IniSettingsStore.h"
#include "../src/GrenadeSelection.h"
#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace
{
    using namespace rock::api::configuration;
    using rock::api::Status;
    std::uint64_t revision = 1;
    std::string configured = "false";
    std::string written;
    bool refuseWrite = false;
    std::string grenadeMode = "false";
    bool publishGrenade = true;
    bool grenadeVisitSucceeds = true;
    Status visitGrenade(rock::api::OwnerToken owner,Group group, VisitorV1 callback, void* context) noexcept
    {
        if(owner!=1)return Status::OwnerNotRegistered;
        if (!grenadeVisitSucceeds || group != Group::Consumer) return Status::NotReady;
        const SettingV1 unrelated{"RealisticWeapons", "bOther", "true", "true", "", "", ValueType::Boolean, 0};
        callback(&unrelated, context);
        if (publishGrenade) {
            const SettingV1 setting{"RealisticWeapons", "bImmersiveGrenades", grenadeMode.c_str(), "true", "", "", ValueType::Boolean, 0};
            callback(&setting, context);
        }
        return Status::Ok;
    }
    Status currentRevision(rock::api::OwnerToken owner,std::uint64_t* out) noexcept { if(owner!=1)return Status::OwnerNotRegistered; *out=revision; return Status::Ok; }
    Status visit(rock::api::OwnerToken owner,Group group, VisitorV1 callback, void* context) noexcept
    {
        if(owner!=1)return Status::OwnerNotRegistered;
        if (group == Group::Consumer) {
            const SettingV1 logging{"Logging", "iLogLevel", "6", "6",
                "01. Logging", "Log detail", ValueType::Integer, 0};
            const SettingV1 weapon{"ImmersiveWeapons", "bBipodMode", "true", "true",
                "04. Weapon Handling", "Surface latch", ValueType::Boolean, 0};
            callback(&logging, context);
            callback(&weapon, context);
            return Status::Ok;
        }
        const SettingV1 toggle{"Debug", "bDeveloperModeEnabled", configured.c_str(), "false",
            "Developer", "Enables developer controls", ValueType::Boolean, configured == "false" ? 0u : 1u};
        const SettingV1 number{"PhysicsInteraction", "fDebugVideoSyncMarkerSize", "4", "4",
            "Developer", "Marker size", ValueType::Float, 0};
        callback(&toggle, context);
        callback(&number, context);
        return Status::Ok;
    }
    Status set(rock::api::OwnerToken owner,Group group, const char* section, const char* key, const char* value, char* error, std::uint32_t capacity) noexcept
    {
        if(owner!=1)return Status::OwnerNotRegistered;
        if (refuseWrite) {
            if (capacity > 14) std::strcpy(error, "write refused");
            return Status::NotReady;
        }
        if (group != Group::Developer || std::string_view(section) != "Debug" ||
            std::string_view(key) != "bDeveloperModeEnabled") return Status::NotReady;
        written = value;
        return Status::Ok;
    }
    void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
}

int main()
{
    using namespace rock_configurator;
    try {
        const rock::api::configuration::ApiV1 grenadeApi{currentRevision, visitGrenade, nullptr};
        require(wheel::immersiveGrenadesEnabled(&grenadeApi,1) == false, "disabled immersive grenades did not select vanilla mode");
        grenadeMode = "true";
        require(wheel::immersiveGrenadesEnabled(&grenadeApi,1) == true, "enabled immersive grenades did not select handoff mode");
        grenadeMode = "false";
        require(wheel::immersiveGrenadesEnabled(&grenadeApi,1) == false, "mode change left a cached handoff selection");
        publishGrenade = false;
        require(!wheel::immersiveGrenadesEnabled(&grenadeApi,1).has_value(), "missing grenade setting used an unrelated boolean");
        publishGrenade = true; grenadeVisitSucceeds = false;
        require(!wheel::immersiveGrenadesEnabled(&grenadeApi,1).has_value(), "failed setting visit enabled handoff");
        require(!wheel::immersiveGrenadesEnabled(nullptr,0).has_value(), "missing ROCK configuration API was accepted");
        const rock::api::configuration::ApiV1 api{currentRevision, visit, set};
        const auto absent = std::filesystem::temp_directory_path() / "ROCK-absent-developer-bridge.ini";
        require(!std::filesystem::exists(absent), "fixture path must not exist");
        IniSettingsStore store(absent, RpsMod::RockDeveloper, &api, {}, 1);
        require(store.load(), "developer controls unavailable without a file");
        require(store.settings().size() == 2, "compiled defaults were not exposed");
        require(store.settings()[1].type == SettingType::Float, "whole-number float default changed control type");
        require(!store.settings()[0].overridden && store.settings()[0].defaultValue == "false", "default metadata missing");
        require(!store.needsReload(), "new snapshot is unexpectedly stale");
        auto result = store.setBooleanByIndex(0, true);
        require(result.saved && written == "true", "menu did not route developer change to ROCK");
        require(!std::filesystem::exists(absent), "menu wrote a second independent INI copy");
        configured = "true"; ++revision;
        require(store.needsReload(), "provider revision did not request refresh");
        require(store.reload(), "provider refresh failed");
        require(store.settings()[0].value == "true" && store.settings()[0].overridden, "effective override did not refresh");
        refuseWrite = true;
        result = store.setBooleanByIndex(0, false);
        require(!result.saved && store.settings()[0].value == "true", "failed write changed the menu state");
        refuseWrite = false;
        result = store.setBooleanByIndex(0, false);
        require(result.saved && written == "false", "restoring default was not delegated to ROCK");
        configured = "false"; ++revision;
        require(store.needsReload() && store.reload(), "external removal did not refresh");
        require(!store.settings()[0].overridden, "default still displayed as overridden");
        struct Cleanup { std::filesystem::path path; ~Cleanup() { std::error_code error; std::filesystem::remove(path, error); } } cleanup{absent};
        { std::ofstream file(absent); file << "[ImmersiveWeapons]\n; 99. Old weapon section\nbBipodMode=true\n[Logging]\n; Old help\niLogLevel=6\n"; }
        IniSettingsStore consumer(absent, RpsMod::Rock, &api, {}, 1);
        require(consumer.load(), "consumer catalog could not load");
        require(consumer.settings()[0].key == "iLogLevel" && consumer.settings()[1].key == "bBipodMode",
            "consumer menu used disk order instead of the provider catalog");
        require(consumer.settings()[0].category == "01. Logging" && consumer.settings()[1].category == "04. Weapon Handling" &&
            consumer.settings()[0].description == "Log detail", "old INI comments replaced compiled section labels or help");
        std::cout << "Wheel configuration bridge checks passed.\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
