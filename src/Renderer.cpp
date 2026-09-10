#include "PCH.h"
#include "Renderer.h"
#include "Runtime.h"
#include "Fonts.h"
#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <wrl/client.h>
#include <d3d11.h>
#include <cfloat>

namespace wheel {
namespace {
using Microsoft::WRL::ComPtr;
struct ContextDeleter {void operator()(ImGuiContext* context)const{if(context)ImGui::DestroyContext(context);}};
struct RenderState {
 std::mutex mutex;
 ComPtr<ID3D11Device> device;
 std::unique_ptr<ImGuiContext,ContextDeleter> imgui;
 bool backend{};
 ~RenderState(){release();}
 void release(){if(imgui){ImGui::SetCurrentContext(imgui.get());if(backend)ImGui_ImplDX11_Shutdown();imgui.reset();}backend=false;device.Reset();}
};
RenderState& renderState(){static RenderState s;return s;}
struct PanelState {
 std::mutex mutex;const rpsui::sdk::ApiV1* api{};std::uint64_t owner{},panel{},sequence{};
 rpsui::sdk::ResultV1 lastPresentationResult{rpsui::sdk::ResultV1::Ok};
};
PanelState& panelState(){static PanelState s;return s;}
void RPSUI_CALL drawFrame(const rpsui::sdk::PanelRenderFrameV1* frame,void*) noexcept {
 std::uint64_t ticket{};
 try {
  if(!frame || frame->structSize<sizeof(*frame) || !frame->d3dDevice || !frame->d3dContext || !frame->renderTargetView)return;
  ticket=wheelDrawGeneration();if(!ticket)return;
  auto& render=renderState();std::scoped_lock lock(render.mutex);
  auto* device=static_cast<ID3D11Device*>(frame->d3dDevice);
  auto* context=static_cast<ID3D11DeviceContext*>(frame->d3dContext);
  if(render.device.Get()!=device || !render.backend) {
   render.release();render.device=device;render.imgui.reset(ImGui::CreateContext());
   if(!render.imgui)throw std::runtime_error("ImGui context unavailable");
   ImGui::SetCurrentContext(render.imgui.get());installFonts();
   if(!ImGui_ImplDX11_Init(device,context))throw std::runtime_error("DX11 backend unavailable");
   render.backend=true;spdlog::info("Wheel renderer initialized");
  }
  ImGui::SetCurrentContext(render.imgui.get());auto& io=ImGui::GetIO();
  io.DisplaySize={static_cast<float>(frame->pixelWidth),static_cast<float>(frame->pixelHeight)};
  io.DeltaTime=std::clamp(frame->deltaSeconds,1.f/240,.1f);io.MouseDrawCursor=true;
  io.AddMousePosEvent(frame->pointerValid?frame->pointerPixelX:-FLT_MAX,frame->pointerValid?frame->pointerPixelY:-FLT_MAX);
  io.AddMouseButtonEvent(0,false); // The wheel selects on B release, never pointer clicks.
  io.AddMouseWheelEvent(0,frame->scrollAxisY*5.5f*io.DeltaTime);
  auto* target=static_cast<ID3D11RenderTargetView*>(frame->renderTargetView);
  context->OMSetRenderTargets(1,&target,nullptr);
  ImGui_ImplDX11_NewFrame();ImGui::NewFrame();
  Action action;
  {auto& shared=sharedModel();std::unique_lock modelLock(shared.mutex,std::try_to_lock);
   if(modelLock.owns_lock())action=drawWheel(shared.model,shared.view);}
  ImGui::Render();ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
  publishWheelSelection(ticket,action);
 } catch(const std::exception& e){spdlog::error("Wheel render failed: {}",e.what());if(ticket)failWheelPresentation(ticket);}
 catch(...){if(ticket)failWheelPresentation(ticket);}
}
}
bool installPanel() {
 auto& p=panelState();std::scoped_lock lock(p.mutex);
 if(p.panel)return true;
 p.api=rpsui::sdk::RequestApiV1();
 if(!p.api || !p.api->registerConsumer || !p.api->unregisterConsumer || !p.api->registerPanel ||
  !p.api->unregisterPanel || !p.api->submitPanelPresentation || !p.api->isFrameworkReady ||
  !(p.api->featureBits&rpsui::sdk::featureMask(rpsui::sdk::FeatureV1::ShapedPanels))) {
  spdlog::error("Wheel requires RPS UI Framework with shaped panel support");return false;
 }
 rpsui::sdk::ConsumerRegistrationV1 consumer;
 std::snprintf(consumer.consumerId,sizeof(consumer.consumerId),"rock.wheel");
 std::snprintf(consumer.displayName,sizeof(consumer.displayName),"ROCK Wheel Menu");
 consumer.requestedFeatures=p.api->featureBits;
 rpsui::sdk::ConsumerHandleV1 handle;
 if(p.api->registerConsumer(&consumer,&handle)!=rpsui::sdk::ResultV1::Ok)return false;
 p.owner=handle.ownerToken;
 rpsui::sdk::PanelRegistrationV1 panel;
 std::snprintf(panel.panelId,sizeof(panel.panelId),"rock.wheel.main");
 std::snprintf(panel.displayName,sizeof(panel.displayName),"ROCK Wheel Menu");
 panel.pixelWidth=1024;panel.pixelHeight=1024;
 panel.defaultPhysicalWidth=95;panel.minimumPhysicalWidth=95;panel.maximumPhysicalWidth=95;
 panel.flags=static_cast<std::uint32_t>(rpsui::sdk::PanelFlagV1::Transparent)|
  static_cast<std::uint32_t>(rpsui::sdk::PanelFlagV1::FixedSize)|static_cast<std::uint32_t>(rpsui::sdk::PanelFlagV1::CircularInput);
 panel.sortOrder=300;panel.renderCallback=drawFrame;
 if(p.api->registerPanel(p.owner,&panel,&p.panel)!=rpsui::sdk::ResultV1::Ok) {
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
bool presentPanel(bool open,const rpsui::sdk::PanelPoseV1* pose) {
 auto& p=panelState();std::scoped_lock lock(p.mutex);
 if(!p.panel || !p.api || (open && !pose))return false;
 if(open && !p.api->isFrameworkReady()) {
  spdlog::warn("Wheel opening cancelled: RPS UI Framework is not ready; check RPS_UI_Framework.log");return false;
 }
 rpsui::sdk::PanelPresentationV1 presentation;
 presentation.sequence=++p.sequence;presentation.open=open?1:0;if(pose)presentation.pose=*pose;
 const auto result=p.api->submitPanelPresentation(p.owner,p.panel,&presentation);
 if(result!=rpsui::sdk::ResultV1::Ok && result!=p.lastPresentationResult)
  spdlog::error("Wheel panel {} rejected by RPS UI Framework: {}",open?"open":"close",static_cast<unsigned>(result));
 p.lastPresentationResult=result;
 return result==rpsui::sdk::ResultV1::Ok;
}
bool frameworkReady() {
 auto& p=panelState();std::scoped_lock lock(p.mutex);
 return p.panel && p.api && p.api->isFrameworkReady && p.api->isFrameworkReady();
}
}
