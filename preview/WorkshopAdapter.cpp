#include "render/NativeRenderer.h"
#include "render/FrameworkPanelRenderer.h"
#include "ConfiguratorRuntime.h"
#include "Fonts.h"
#include <imgui.h>
#include <Windows.h>
#include <array>

// Desktop presentation only. The same workshop model and widgets are compiled
// with WHEEL_DESKTOP_PREVIEW: saves stay in memory and spawning uses sample data.
namespace devui::render {
namespace { std::array<ImFont*, static_cast<std::size_t>(FontRole::Count)> configFonts{}; }
ImFont* GetFont(FontRole role) noexcept {
 auto* font = configFonts[static_cast<std::size_t>(role)];
 return font ? font : ImGui::GetIO().FontDefault;
}
void PrepareFonts() noexcept {
 try {wheel::prepareFonts();}
 catch(...){OutputDebugStringA("PALM terminal font preload failed; using the ImGui font\n");}
 for(std::size_t i=0;i<configFonts.size();++i)configFonts[i]=wheel::addPreparedFont(kFontSizes[i]);
}
bool SetPanelOpen(bool, const PanelPose*) noexcept { return true; }
void Shutdown() noexcept {}
bool InstallFrameworkPanel() noexcept { return false; }
bool ResetFrameworkPanelSize() noexcept {
 rock_configurator::onFrameworkPanelFrame(kDefaultPanelPhysicalWidth,
  rock_configurator::panel_resize::Handle::None,rock_configurator::panel_resize::Handle::None);
 return true;
}
}
