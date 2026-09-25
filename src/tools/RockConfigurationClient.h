#pragma once
#include <ROCK/Client.h>
#include <ROCK/Configuration.h>
#include "LoadedRpsMods.h"
#ifndef WHEEL_DESKTOP_PREVIEW
#include <Windows.h>
#endif
namespace rock_configurator {
struct ConfigurationConnection {
    const rock::api::configuration::ApiV1* api{};
    rock::api::OwnerToken owner{};
    rock::api::Status status{rock::api::Status::NotReady};
};
inline ConfigurationConnection configurationConnection(std::optional<RpsMod> requested = std::nullopt) noexcept {
#ifndef WHEEL_DESKTOP_PREVIEW
    const auto selected = requested ? requested : loadedRockMod();
    if (!selected || !isRockConfiguration(*selected)) return {};
    // Separate process-lifetime registrations for the two providers; each is
    // shared only by that provider's consumer/developer pages.
    // Connection and visits are performed on the game thread.
    static auto* clients=new std::array<rock::api::Client, 2>();
    static std::array<const rock::api::configuration::ApiV1*, 2> apis{};
    const auto index = isV2Rock(*selected) ? 1u : 0u;
    auto* client=&(*clients)[index];
    auto& api=apis[index];
    if (!api) {
        const auto module=GetModuleHandleW(modInfo(*selected).module);
        const auto query=module?reinterpret_cast<rock::api::QueryInterfaceV1>(GetProcAddress(module,rock::api::kQueryExportName)):nullptr;
        if(!client->owner()) {
            const auto status=client->connect(query,"RobCo PALM Configuration");
            if(status!=rock::api::Status::Ok) return {nullptr,0,status};
        }
        const auto status=client->acquire(3,api);
        if(status!=rock::api::Status::Ok) return {nullptr,client->owner(),status};
    }
    return {api,client->owner(),rock::api::Status::Ok};
#else
    return {};
#endif
}
}
