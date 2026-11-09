#include "PCH.h"
#include "Renderer.h"
#include "Runtime.h"
#include "Fonts.h"
#include "WheelConfig.h"
#include "IconAtlas.h"
#include "RpsUiQuery.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <RPS_UI_Framework/ImGuiBuild.h>
#include <wrl/client.h>
#include <d3d11.h>

namespace wheel {
namespace {
using Microsoft::WRL::ComPtr;
// Render thread only. RPS UI owns the wheel's ImGui context; the font and the
// icon atlases made for it in setupFrame live with that context generation.
struct RenderState {
 std::mutex mutex;
 ComPtr<ID3D11Device> device;
 IconAtlas icons;
 std::uint64_t deviceGeneration{},contextGeneration{};
 bool primaryDown{};std::uint64_t clickGeneration{};
};
RenderState& renderState(){static RenderState s;return s;}
struct PanelState {
 std::mutex mutex;const rpsui::api::framework::Api* api{};std::uint64_t owner{},panel{},sequence{};
 rpsui::api::Result lastPresentationResult{rpsui::api::Result::Ok};
};
PanelState& panelState(){static PanelState s;return s;}
void RPSUI_CALL setupFrame(const rpsui::api::framework::PanelSetup* setup,void*) noexcept {
 try {
  if(!setup || setup->size<sizeof(*setup) || !setup->d3dDevice)return;
  RPSUI_IMGUI_BIND(setup->imgui);
  auto& render=renderState();std::scoped_lock lock(render.mutex);
  installFonts();
  auto* device=static_cast<ID3D11Device*>(setup->d3dDevice);
  if(render.device.Get()!=device || render.deviceGeneration!=setup->deviceGeneration) {
   ComPtr<ID3D11DeviceContext> context;device->GetImmediateContext(&context);
   if(!render.icons.create(device,context.Get()))throw std::runtime_error("Embedded wheel icon atlases unavailable");
   render.device=device;render.deviceGeneration=setup->deviceGeneration;
  }
  render.contextGeneration=setup->contextGeneration;
  spdlog::info("PALM renderer initialized (context generation {})",setup->contextGeneration);
 } catch(const std::exception& e){spdlog::error("PALM renderer setup failed: {}",e.what());}
 catch(...){spdlog::error("PALM renderer setup failed");}
}
void RPSUI_CALL drawFrame(const rpsui::api::framework::PanelDrawFrame* frame,void*) noexcept {
 std::uint64_t ticket{};
 try {
  if(!frame || frame->size<sizeof(*frame))return;
  ticket=wheelDrawGeneration();if(!ticket)return;
  RPSUI_IMGUI_BIND(frame->imgui);
  auto& render=renderState();std::scoped_lock lock(render.mutex);
  if(render.contextGeneration!=frame->contextGeneration)throw std::runtime_error("the wheel's ImGui context was not set up");
  Action action;
  {auto& shared=sharedModel();std::unique_lock modelLock(shared.mutex,std::try_to_lock);
   if(modelLock.owns_lock()){refreshWheelSections(shared.model);action=drawWheel(shared.model,shared.view,{},{},render.icons.ids());}}
  action.hand=frame->pointerHand==rpsui::api::framework::PhysicalHand::Left?PointerHand::Left:
   frame->pointerHand==rpsui::api::framework::PhysicalHand::Right?PointerHand::Right:PointerHand::None;
  publishWheelSelection(ticket,action);
  const bool fresh=render.clickGeneration==ticket && !render.primaryDown && frame->primaryDown;
  render.clickGeneration=ticket;render.primaryDown=frame->primaryDown!=0;
  if(fresh && frame->pointerValid)requestWheelClick(ticket,action);
 } catch(const std::exception& e){spdlog::error("PALM render failed: {}",e.what());if(ticket)failWheelPresentation(ticket);}
 catch(...){if(ticket)failWheelPresentation(ticket);}
}
}
bool installPanel() {
 auto& p=panelState();std::scoped_lock lock(p.mutex);
 if(p.panel)return true;
 if(rpsui::api::acquire(rpsUiQuery(),p.api)!=rpsui::api::Result::Ok) {
  spdlog::error("PALM requires RPS UI Framework 2.0");return false;
 }
 rpsui::api::framework::ConsumerRegistration consumer;
 std::snprintf(consumer.consumerId,sizeof(consumer.consumerId),"robco.palm");
 std::snprintf(consumer.displayName,sizeof(consumer.displayName),"RobCo PALM");
 rpsui::api::framework::ConsumerHandle handle;
 const auto consumerResult=p.api->registerConsumer(&consumer,&handle);
 if(consumerResult!=rpsui::api::Result::Ok){
  spdlog::error("PALM startup failed: wheel consumer registration rejected ({})",static_cast<unsigned>(consumerResult));return false;
 }
 p.owner=handle.ownerToken;
 rpsui::api::framework::PanelRegistration panel;
 std::snprintf(panel.panelId,sizeof(panel.panelId),"robco.palm.main");
 std::snprintf(panel.displayName,sizeof(panel.displayName),"RobCo PALM");
 panel.layoutWidth=1024;panel.layoutHeight=1024;
 panel.defaultPhysicalWidth=95;panel.minimumPhysicalWidth=95;panel.maximumPhysicalWidth=95;
 panel.flags=static_cast<std::uint32_t>(rpsui::api::framework::PanelFlag::Transparent)|
  static_cast<std::uint32_t>(rpsui::api::framework::PanelFlag::FixedSize)|static_cast<std::uint32_t>(rpsui::api::framework::PanelFlag::CircularInput);
 panel.sortOrder=300;panel.imgui=RPSUI_IMGUI_BUILD();panel.setupCallback=setupFrame;panel.drawCallback=drawFrame;
 const auto panelResult=p.api->registerPanel(p.owner,&panel,&p.panel);
 if(panelResult!=rpsui::api::Result::Ok) {
  spdlog::error("PALM startup failed: wheel panel registration rejected ({})",static_cast<unsigned>(panelResult));
  (void)p.api->unregisterConsumer(p.owner);p.owner=0;return false;
 }
 return true;
}
void unregisterPanel() {
 auto& p=panelState();std::scoped_lock lock(p.mutex);
 if(p.api && p.owner) {
  if(p.panel)(void)p.api->unregisterPanel(p.owner,p.panel);
  (void)p.api->unregisterConsumer(p.owner);
 }
 p.panel=0;p.owner=0;p.api=nullptr;
}
bool presentPanel(bool open,const rpsui::api::framework::PanelPose* pose) {
 auto& p=panelState();std::scoped_lock lock(p.mutex);
 if(!p.panel || !p.api || (open && !pose))return false;
 if(open && !p.api->isFrameworkReady()) {
  spdlog::warn("PALM opening cancelled: RPS UI Framework is not ready; check RPS_UI_Framework.log");return false;
 }
 rpsui::api::framework::PanelPresentation presentation;
 presentation.sequence=++p.sequence;presentation.open=open?1:0;if(pose)presentation.pose=*pose;
 const auto result=p.api->submitPanelPresentation(p.owner,p.panel,&presentation);
 if(result!=rpsui::api::Result::Ok && result!=p.lastPresentationResult)
  spdlog::error("PALM panel {} rejected by RPS UI Framework: {}",open?"open":"close",static_cast<unsigned>(result));
 p.lastPresentationResult=result;
 return result==rpsui::api::Result::Ok;
}
bool frameworkReady() {
 auto& p=panelState();std::scoped_lock lock(p.mutex);
 return p.panel && p.api && p.api->isFrameworkReady && p.api->isFrameworkReady();
}
}
