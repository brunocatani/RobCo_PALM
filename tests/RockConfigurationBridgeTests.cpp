#include "IniSettingsStore.h"
#include <array>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace
{
    using namespace rock::configuration_api;
    std::uint64_t revision = 1;
    std::string configured = "false";
    std::string written;
    bool refuseWrite = false;
    std::uint64_t currentRevision() noexcept { return revision; }
    bool visit(Group group, VisitorV1 callback, void* context) noexcept
    {
        if (group != Group::Developer) return false;
        const SettingV1 toggle{"Debug", "bDeveloperModeEnabled", configured.c_str(), "false",
            "Developer", "Enables developer controls", ValueType::Boolean, configured == "false" ? 0u : 1u};
        const SettingV1 number{"PhysicsInteraction", "fDebugVideoSyncMarkerSize", "4", "4",
            "Developer", "Marker size", ValueType::Float, 0};
        callback(&toggle, context);
        callback(&number, context);
        return true;
    }
    bool set(Group group, const char* section, const char* key, const char* value, char* error, std::uint32_t capacity) noexcept
    {
        if (refuseWrite) {
            if (capacity > 14) std::strcpy(error, "write refused");
            return false;
        }
        if (group != Group::Developer || std::string_view(section) != "Debug" ||
            std::string_view(key) != "bDeveloperModeEnabled") return false;
        written = value;
        return true;
    }
    void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
}

int main()
{
    using namespace rock_configurator;
    try {
        const rock::configuration_api::ApiV1 api{1, sizeof(rock::configuration_api::ApiV1), currentRevision, visit, set};
        const auto absent = std::filesystem::temp_directory_path() / "ROCK-absent-developer-bridge.ini";
        require(!std::filesystem::exists(absent), "fixture path must not exist");
        IniSettingsStore store(absent, RpsMod::RockDeveloper, &api);
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
        std::cout << "Wheel configuration bridge checks passed.\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
