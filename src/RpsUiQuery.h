#pragma once
#include "RequiredUpdates.h"
#include <RPS_UI_Framework/Client.h>
#include <Windows.h>

namespace wheel {
// Null when RPS UI Framework is not loaded, or when the loaded one has no 2.0
// discovery, which also stops the game at the main menu.
inline rpsui::api::QueryInterfaceV1 rpsUiQuery() noexcept {
    const auto module=GetModuleHandleW(rpsui::api::kModuleName);
    if(!module)return nullptr;
    const auto query=reinterpret_cast<rpsui::api::QueryInterfaceV1>(GetProcAddress(module,rpsui::api::kQueryExportName));
    if(!query)required_updates::noteRpsUiWithoutDiscovery();
    return query;
}
}
