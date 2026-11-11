#include "PCH.h"
#include "SettingsMenu.h"
#include "PalmTheme.h"
#include "Renderer.h"
#include "RpsUiQuery.h"
#include "WheelConfig.h"
#include "tools/render/NativeRenderer.h"
#include <RPS_UI_Framework/ImGuiBuild.h>
#include <RPS_UI_Framework/SettingsMenu.h>
#include <imgui_internal.h>

namespace wheel {
namespace {
namespace menu = rpsui::api::settings_menu;
struct State {
 std::mutex mutex;const menu::Api* api{};std::uint64_t owner{};std::array<std::uint64_t,2> pages{};
 std::atomic_uint64_t closedSeen{};
};
State& state(){static State s;return s;}
// The faces of the page being drawn (render thread, during a page callback).
const menu::PageDrawFrame* s_pageFrame{};

enum class Page : std::uintptr_t {Items=1,Settings=2};
bool RPSUI_CALL drawPage(const menu::PageDrawFrame* frame,void* userData) noexcept {
 try {
  if(!frame || frame->size<sizeof(*frame))return false;
  RPSUI_IMGUI_BIND(frame->imgui);
  palm::theme::Scope theme(frame->theme);
  s_pageFrame=frame;struct Clear{~Clear(){s_pageFrame=nullptr;}} clear;
  if(static_cast<Page>(reinterpret_cast<std::uintptr_t>(userData))==Page::Items)drawWheelConfig();
  else drawWheelSettings();
 } catch(const std::exception& e){spdlog::error("PALM settings page failed: {}",e.what());}
 catch(...){spdlog::error("PALM settings page failed");}
 return false;
}
bool addPage(State& s,Page page,const char* id,const char* title,std::int32_t order,std::uint64_t& handle) {
 menu::PageRegistration registration;
 std::snprintf(registration.pageId,sizeof(registration.pageId),"%s",id);
 std::snprintf(registration.title,sizeof(registration.title),"%s",title);
 registration.sortOrder=order;registration.imgui=RPSUI_IMGUI_BUILD();
 registration.drawCallback=drawPage;registration.userData=reinterpret_cast<void*>(static_cast<std::uintptr_t>(page));
 const auto result=s.api->registerPage(s.owner,&registration,&handle);
 if(result!=rpsui::api::Result::Ok)spdlog::error("PALM settings page '{}' refused ({})",title,static_cast<unsigned>(result));
 return result==rpsui::api::Result::Ok;
}
}

bool installSettingsPages() {
 auto& s=state();std::scoped_lock lock(s.mutex);
 if(s.api)return true;
 s.owner=frameworkOwner();
 if(!s.owner || rpsui::api::acquire(rpsUiQuery(),s.api)!=rpsui::api::Result::Ok || !s.api) {
  s.api=nullptr;spdlog::error("PALM Config unavailable: RPS UI's settings menu could not be bound");return false;
 }
 s.closedSeen=s.api->closedCount();
 const bool items=addPage(s,Page::Items,"robco.palm.wheel-items","Wheel items",10,s.pages[0]);
 const bool settings=addPage(s,Page::Settings,"robco.palm.wheel-settings","Wheel settings",20,s.pages[1]);
 return items && settings;
}
void uninstallSettingsPages() {
 auto& s=state();std::scoped_lock lock(s.mutex);
 if(s.api)for(auto& page:s.pages)if(page){(void)s.api->unregisterPage(s.owner,page);page=0;}
 s.api=nullptr;s.owner=0;
}
bool openSettingsMenu(const rpsui::api::framework::PanelPose& pose) {
 auto& s=state();std::scoped_lock lock(s.mutex);
 if(!s.api)return false;
 menu::OpenRequest request;request.pose=pose;request.hasPose=1;
 const auto result=s.api->open(s.owner,&request);
 if(result!=rpsui::api::Result::Ok)spdlog::warn("PALM Config: the settings menu did not open ({})",static_cast<unsigned>(result));
 return result==rpsui::api::Result::Ok;
}
bool settingsMenuOpen() {
 const auto* api=state().api;return api && api->isOpen();
}
bool takeSettingsMenuClosed() {
 auto& s=state();const auto* api=s.api;if(!api)return false;
 const auto closed=api->closedCount();
 return s.closedSeen.exchange(closed)!=closed;
}
}

// The wheel pages' fonts are the menu's theme faces, by the roles PALM's layout uses.
namespace devui::render {
ImFont* GetFont(FontRole role) noexcept {
 using Face=rpsui::api::framework::FontRole;
 const auto* frame=wheel::s_pageFrame;if(!frame)return ImGui::GetFont();
 const auto face=role==FontRole::Heading || role==FontRole::Display?Face::Strong:role==FontRole::Mono?Face::Mono:Face::Interface;
 auto* font=static_cast<ImFont*>(frame->fonts[static_cast<std::size_t>(face)]);
 return font?font:ImGui::GetFont();
}
float FontSize(FontRole role) noexcept {return kFontSizes[static_cast<std::size_t>(role)]*palm::theme::textScale();}
void PrepareFonts() noexcept {}
}
