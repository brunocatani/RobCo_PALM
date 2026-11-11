#pragma once
#include <RPS_UI_Framework/Framework.h>

// RPS UI Framework owns the settings menu (every mod's pages, the Spawner, the Console).
// PALM opens it from the wheel's Config entry and adds its own two pages, the wheel's
// items and its settings, which it draws with its own ImGui copy inside the menu.
namespace wheel {
// After the wheel panel registered: binds RPS UI's SettingsMenu and adds PALM's pages.
// False leaves the wheel working without the Config entry's menu.
bool installSettingsPages();
void uninstallSettingsPages();
// Game thread: opens the menu where the wheel was.
bool openSettingsMenu(const rpsui::api::framework::PanelPose& pose);
[[nodiscard]] bool settingsMenuOpen();
// True once after each close of the menu, which may have changed the inventory.
[[nodiscard]] bool takeSettingsMenuClosed();
}
