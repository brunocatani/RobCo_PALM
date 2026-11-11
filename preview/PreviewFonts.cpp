#include "render/NativeRenderer.h"
#include "Fonts.h"
#include "PalmTheme.h"
#include <imgui.h>
#include <Windows.h>
#include <array>

// The desktop preview's faces for PALM's settings pages; in game they are the RPS UI
// settings menu's theme faces (src/SettingsMenu.cpp).
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
float FontSize(FontRole role) noexcept { return kFontSizes[static_cast<std::size_t>(role)] * palm::theme::textScale(); }
}