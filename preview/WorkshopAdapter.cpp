#include "render/NativeRenderer.h"
#include "render/FrameworkPanelRenderer.h"
#include "ConfiguratorRuntime.h"
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
 // Match the runtime's five roles; only the desktop startup path reads files here.
 wchar_t directory[MAX_PATH]{}; if (!GetWindowsDirectoryW(directory, MAX_PATH)) return;
 const std::array files{"segoeui.ttf", "seguisb.ttf", "seguisb.ttf", "segoeuib.ttf", "consola.ttf"};
 const std::array sizes{20.0f,21.0f,27.0f,32.0f,18.0f};
 for (std::size_t i=0;i<files.size();++i) {
  auto path=std::filesystem::path(directory)/"Fonts"/files[i];
  std::error_code error;
  if (i==static_cast<std::size_t>(FontRole::Mono)) {
   const auto cascadia=std::filesystem::path(directory)/"Fonts"/"CascadiaMono.ttf";
   if(std::filesystem::is_regular_file(cascadia,error))path=cascadia;
  }
  ImFontConfig config;config.RasterizerMultiply=1.08f;
  config.SizePixels=sizes[i];
  configFonts[i]=std::filesystem::is_regular_file(path,error) ?
   ImGui::GetIO().Fonts->AddFontFromFileTTF(path.string().c_str(),sizes[i],&config) :
   ImGui::GetIO().Fonts->AddFontDefaultVector(&config);
 }
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
