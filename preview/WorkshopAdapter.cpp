#include "render/NativeRenderer.h"
#include "render/FrameworkPanelRenderer.h"
#include "ConfiguratorRuntime.h"
#include <imgui.h>

// Desktop presentation only. The same workshop model and widgets are compiled
// with WHEEL_DESKTOP_PREVIEW: saves stay in memory and spawning uses sample data.
namespace devui::render {
ImFont* GetFont(FontRole) noexcept { return ImGui::GetIO().FontDefault; }
void PrepareFonts() noexcept {}
void SetPanelOpen(bool, const PanelPose*) noexcept {}
void Shutdown() noexcept {}
bool InstallFrameworkPanel() noexcept { return false; }
bool ResetFrameworkPanelSize() noexcept {
 rock_configurator::onFrameworkPanelFrame(kDefaultPanelPhysicalWidth,
  rock_configurator::panel_resize::Handle::None,rock_configurator::panel_resize::Handle::None);
 return true;
}
}
