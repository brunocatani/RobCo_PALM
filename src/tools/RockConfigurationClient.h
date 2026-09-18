#pragma once
#include <ROCK/Client.h>
#include <ROCK/Configuration.h>
#ifndef WHEEL_DESKTOP_PREVIEW
#include <Windows.h>
#endif
namespace rock_configurator {
struct ConfigurationConnection {
    const rock::api::configuration::ApiV1* api{};
    rock::api::OwnerToken owner{};
};
inline ConfigurationConnection configurationConnection() noexcept {
#ifndef WHEEL_DESKTOP_PREVIEW
    // Process-lifetime registration shared by the consumer/developer pages.
    // Connection and visits are performed on the game thread.
    static auto* client=new rock::api::Client();
    static const rock::api::configuration::ApiV1* api{};
    if (!api) {
        const auto module=GetModuleHandleA("ROCK.dll");
        const auto query=module?reinterpret_cast<rock::api::QueryInterfaceV1>(GetProcAddress(module,rock::api::kQueryExportName)):nullptr;
        if(!client->owner() && client->connect(query,"RobCo PALM Configuration")!=rock::api::Status::Ok) return {};
        if(client->acquire(3,api)!=rock::api::Status::Ok) return {};
    }
    return {api,client->owner()};
#else
    return {};
#endif
}
}
