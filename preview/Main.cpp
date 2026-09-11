#include "WheelView.h"
#include "Fonts.h"
#include "IconAtlas.h"
#include "WheelConfig.h"
#include "PalmControls.h"
#include "ConfiguratorRuntime.h"
#include "render/UiVisualStyle.h"
#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>
#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND,UINT,WPARAM,LPARAM);
namespace {
using Microsoft::WRL::ComPtr;
ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;
ComPtr<IDXGISwapChain> swap;ComPtr<ID3D11RenderTargetView> target;
UINT width{},height{};
bool focused=false,cancelGesture=false;
bool createTarget(){ComPtr<ID3D11Texture2D> back;return SUCCEEDED(swap->GetBuffer(0,IID_PPV_ARGS(&back))) && SUCCEEDED(device->CreateRenderTargetView(back.Get(),nullptr,&target));}
LRESULT CALLBACK windowProc(HWND hwnd,UINT message,WPARAM wparam,LPARAM lparam){
 if(ImGui_ImplWin32_WndProcHandler(hwnd,message,wparam,lparam))return true;
 if(message==WM_SIZE && wparam!=SIZE_MINIMIZED){width=LOWORD(lparam);height=HIWORD(lparam);return 0;}
 if(message==WM_SETFOCUS)focused=true;
 if(message==WM_KILLFOCUS){focused=false;cancelGesture=true;}
 if(message==WM_DESTROY){PostQuitMessage(0);return 0;}
 if(message==WM_SYSCOMMAND && (wparam&0xfff0)==SC_KEYMENU)return 0;
 return DefWindowProcW(hwnd,message,wparam,lparam);
}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int show){
 ImGui_ImplWin32_EnableDpiAwareness();
 WNDCLASSW wc{};wc.lpfnWndProc=windowProc;wc.hInstance=instance;wc.lpszClassName=L"PALMPreview";wc.hCursor=LoadCursor(nullptr,IDC_ARROW);
 if(!RegisterClassW(&wc))return 1;
 const auto hwnd=CreateWindowW(wc.lpszClassName,L"RobCo PALM — Personal Access & Loadout Manager",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,1800,1000,nullptr,nullptr,instance,nullptr);
 if(!hwnd)return 1;
 DXGI_SWAP_CHAIN_DESC desc{};desc.BufferCount=2;desc.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
 desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;desc.OutputWindow=hwnd;desc.SampleDesc.Count=1;desc.Windowed=TRUE;
 desc.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;
 D3D_FEATURE_LEVEL level{};
 if(FAILED(D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&desc,&swap,&device,&level,&context)) || !createTarget())return 1;
 IMGUI_CHECKVERSION();ImGui::CreateContext();wheel::prepareFonts();wheel::installFonts();
 devui::render::PrepareFonts();
 devui::visual::applyStyle();
 ImGui_ImplWin32_Init(hwnd);ImGui_ImplDX11_Init(device.Get(),context.Get());
 wheel::IconAtlas icons;if(!icons.create(device.Get(),context.Get()))return 1;
 auto catalog=wheel::demoInventory();wheel::publishWheelInventory(catalog);
 auto model=wheel::selectedWheelInventory(catalog);wheel::View view;
 rock_configurator::initializePreview();wheel::ControlGesture gesture;auto controls=wheel::snapshotControls();bool wheelOpen=false;
 ShowWindow(hwnd,show);UpdateWindow(hwnd);bool done=false;
 while(!done){
  MSG message{};while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);if(message.message==WM_QUIT)done=true;}
  if(done)break;
  if(IsIconic(hwnd)){WaitMessage();continue;}
  if(width && height){context->OMSetRenderTargets(0,nullptr,nullptr);target.Reset();
   if(FAILED(swap->ResizeBuffers(0,width,height,DXGI_FORMAT_UNKNOWN,0)) || !createTarget())break;width=height=0;}
  ImGui_ImplDX11_NewFrame();ImGui_ImplWin32_NewFrame();ImGui::NewFrame();
  const auto size=ImGui::GetIO().DisplaySize;auto* bg=ImGui::GetBackgroundDrawList();
  bg->AddRectFilled({0,0},size,IM_COL32(6,10,6,255));
  if(cancelGesture){gesture={};wheelOpen=false;cancelGesture=false;}
  // Keyboard B emulates the complete VR chord in the desktop preview.
  const bool held=ImGui::IsKeyDown(ImGuiKey_B);
  const auto currentControls=wheel::snapshotControls();if(currentControls!=controls){controls=currentControls;gesture={};wheelOpen=false;}
  const auto edge=gesture.update(focused && !rock_configurator::isOpen(),controls,held,held,ImGui::IsMouseClicked(0),ImGui::GetTime());
  if(!focused)wheelOpen=false;
  if(edge==wheel::ControlEdge::Open) {
   wheelOpen=true;view={};const auto gestures=model.gestures;
   model=wheel::selectedWheelInventory(catalog);model.category=wheel::Category::Weapons;
   model.gestures=gestures;model.gestures.showing=false;
   model.gestures.availability.fill(wheel::GestureAvailability::Free);
  }
  wheel::Action hover;
  if(wheelOpen){wheel::refreshWheelSections(model);hover=wheel::drawWheel(model,view,{},{},icons.ids());}
  std::uint64_t selectedItem=0;
  if(edge==wheel::ControlEdge::Select && wheelOpen) {
   if(controls.mode==wheel::OpenMode::Hold || hover.cancelHovered || hover.configHovered){wheelOpen=false;gesture.cancel(held);}
   if(hover.configHovered)rock_configurator::setPreviewOpen(true);
   else if(hover.section)(void)wheel::sectionRegistry().dispatch(hover.section,hover.sectionItem);
   else if(hover.hoveredGesture) {
    const unsigned hand=wheel::gestureIsLeft(hover.hoveredGesture)?1:0;
    auto& active=model.gestures.active[hand];active=active==hover.hoveredGesture?0:hover.hoveredGesture;
    model.status=active?"PREVIEW: GESTURE SELECTED":"PREVIEW: GESTURE CLEARED";
   }
   else selectedItem=hover.hoveredItem;
  }
  if(rock_configurator::isOpen())(void)rock_configurator::drawImGui(12,12,size.x-24,size.y-24);
  rock_configurator::drainPreviewActions();
  bool inventoryChanged=false;
  if(selectedItem)for(unsigned c=0;c<wheel::kCategoryCount;++c)for(auto& item:catalog.items[c])if(wheel::selectionToken(item)==selectedItem){
   if(wheel::isEquipment(static_cast<wheel::Category>(c))) {
    const bool equip=!item.equipped;
    if(equip && c==static_cast<unsigned>(wheel::Category::Weapons))for(auto& other:catalog.items[c])other.equipped=false;
    item.equipped=equip;model.status=std::string(equip?"PREVIEW: EQUIPPED  /  ":"PREVIEW: UNEQUIPPED  /  ")+item.name;
   }else {if(item.count)--item.count;model.status="PREVIEW: TAKEN TO HAND  /  "+item.name;}
   inventoryChanged=true;
  }
  if(inventoryChanged)wheel::publishWheelInventory(catalog);
  if(wheel::takeWheelConfigChange() || inventoryChanged) {
   const auto category=model.category;const auto gestures=model.gestures;
   model=wheel::selectedWheelInventory(catalog);model.category=category;model.gestures=gestures;view={};
  }
  if(ImGui::IsKeyPressed(ImGuiKey_R)){catalog=wheel::demoInventory();wheel::publishWheelInventory(catalog);model=wheel::selectedWheelInventory(catalog);view={};}
  if(ImGui::IsKeyPressed(ImGuiKey_Escape))done=true;
  ImGui::Render();const float clear[4]{0,0,0,1};auto* output=target.Get();context->OMSetRenderTargets(1,&output,nullptr);
  context->ClearRenderTargetView(output,clear);ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
  if(FAILED(swap->Present(1,0)))break;
 }
 ImGui_ImplDX11_Shutdown();ImGui_ImplWin32_Shutdown();ImGui::DestroyContext();
 DestroyWindow(hwnd);UnregisterClassW(wc.lpszClassName,instance);return 0;
}
