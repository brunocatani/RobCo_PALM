#pragma once
#include "PalmControls.h"
#include "WheelConfig.h"
#include "render/ConfigUi.h"
#include <imgui.h>

// In game, PALM's two pages are tabs of RPS UI's settings menu. The desktop preview and
// the capture tool draw them the same way: under a 94-unit header like the menu's, where
// the page's area starts.
namespace preview {
enum class PalmPage : unsigned {Items,Settings};
// Returns false when the header's close button was pressed.
inline bool drawPalmPage(PalmPage& page,float x,float y,float width,float height) {
 ImGui::SetNextWindowPos({x,y});ImGui::SetNextWindowSize({width,height});
 constexpr ImGuiWindowFlags flags=ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings;
 ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{0,0});
 bool open=true;
 if(ImGui::Begin("PALM pages##preview",nullptr,flags)) {
  ImGui::SetCursorPos({28,30});
  if(devui::visual::navigation("Wheel items",page==PalmPage::Items))page=PalmPage::Items;
  ImGui::SetCursorPos({260,30});
  if(devui::visual::navigation("Wheel settings",page==PalmPage::Settings))page=PalmPage::Settings;
  ImGui::SetCursorPos({width-80,20});
  if(devui::visual::button("X##close",{52,54}))open=false;
  ImGui::SetCursorPos({0,94});
  ImGui::BeginChild("page",{width,height-94});
  if(page==PalmPage::Items)wheel::drawWheelConfig();else wheel::drawWheelSettings();
  ImGui::EndChild();
 }
 ImGui::End();ImGui::PopStyleVar();
 return open;
}
}
