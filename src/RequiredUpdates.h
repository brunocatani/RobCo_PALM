#pragma once

// Version mismatches stop the game at the main menu with an error box naming the
// mod to update (RPS/Runtime/RequiredUpdate.h; Plugin.cpp registers the watch):
// a consumer built for another PALM API, and a loaded RPS UI Framework without
// the discovery PALM was built for.

#include <RobCoPALM/Abi.h>

#include <cstdint>

namespace wheel::required_updates
{
    // Any thread. Called with every discovery result; acts only on version
    // refusals. callerAddress is the discovery call's return address, which lies
    // inside the consumer's DLL.
    void noteDiscoveryRefusal(palm::api::Result result, palm::api::InterfaceId id, std::uint32_t requestedMajor,
        const void* callerAddress) noexcept;

    // Any thread. A call to one of the pre-2.0 exports, which only refuse.
    void noteRetiredExport(const char* exportName, const void* callerAddress) noexcept;

    // RPS_UI_Framework.dll is loaded but older than the API PALM was built for.
    void noteRpsUiWithoutDiscovery() noexcept;
}
