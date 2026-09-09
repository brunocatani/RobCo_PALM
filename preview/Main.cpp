#include "WheelView.h"
#include "Fonts.h"
#include "WheelConfig.h"
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
bool createTarget(){ComPtr<ID3D11Texture2D> back;return SUCCEEDED(swap->GetBuffer(0,IID_PPV_ARGS(&back))) && SUCCEEDED(device->CreateRenderTargetView(back.Get(),nullptr,&target));}
LRESULT CALLBACK windowProc(HWND hwnd,UINT message,WPARAM wparam,LPARAM lparam){
 if(ImGui_ImplWin32_WndProcHandler(hwnd,message,wparam,lparam))return true;
 if(message==WM_SIZE && wparam!=SIZE_MINIMIZED){width=LOWORD(lparam);height=HIWORD(lparam);return 0;}
 if(message==WM_DESTROY){PostQuitMessage(0);return 0;}
 if(message==WM_SYSCOMMAND && (wparam&0xfff0)==SC_KEYMENU)return 0;
 return DefWindowProcW(hwnd,message,wparam,lparam);
}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int show){
 ImGui_ImplWin32_EnableDpiAwareness();
 WNDCLASSW wc{};wc.lpfnWndProc=windowProc;wc.hInstance=instance;wc.lpszClassName=L"ROCKWheelPreview";wc.hCursor=LoadCursor(nullptr,IDC_ARROW);
 if(!RegisterClassW(&wc))return 1;
 const auto hwnd=CreateWindowW(wc.lpszClassName,L"ROCK Field Kit — Wheel + Config Preview",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,1800,1000,nullptr,nullptr,instance,nullptr);
 if(!hwnd)return 1;
 DXGI_SWAP_CHAIN_DESC desc{};desc.BufferCount=2;desc.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
 desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;desc.OutputWindow=hwnd;desc.SampleDesc.Count=1;desc.Windowed=TRUE;
 desc.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;
 D3D_FEATURE_LEVEL level{};
 if(FAILED(D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&desc,&swap,&device,&level,&context)) || !createTarget())return 1;
 IMGUI_CHECKVERSION();ImGui::CreateContext();wheel::prepareFonts();wheel::installFonts();
 devui::visual::applyStyle();
 ImGui_ImplWin32_Init(hwnd);ImGui_ImplDX11_Init(device.Get(),context.Get());
 auto catalog=wheel::demoInventory();wheel::publishWheelInventory(catalog);
 auto model=wheel::selectedWheelInventory(catalog);wheel::View view;
 rock_configurator::initializePreview();
 ShowWindow(hwnd,show);UpdateWindow(hwnd);bool done=false;
 while(!done){
  MSG message{};while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);if(message.message==WM_QUIT)done=true;}
  if(done)break;
  if(IsIconic(hwnd)){WaitMessage();continue;}
  if(width && height){context->OMSetRenderTargets(0,nullptr,nullptr);target.Reset();
   if(FAILED(swap->ResizeBuffers(0,width,height,DXGI_FORMAT_UNKNOWN,0)) || !createTarget())break;width=height=0;}
  ImGui_ImplDX11_NewFrame();ImGui_ImplWin32_NewFrame();ImGui::NewFrame();
  const auto size=ImGui::GetIO().DisplaySize;auto* bg=ImGui::GetBackgroundDrawList();
  bg->AddRectFilledMultiColor({0,0},size,IM_COL32(26,40,49,255),IM_COL32(24,37,40,255),IM_COL32(8,18,24,255),IM_COL32(12,24,29,255));
  for(float x=0;x<size.x;x+=48)bg->AddLine({x,0},{x,size.y},IM_COL32(135,175,175,9));
  for(float y=0;y<size.y;y+=48)bg->AddLine({0,y},{size.x,y},IM_COL32(135,175,175,9));
  const bool workshopOpen=rock_configurator::isOpen();
  const float wheelWidth=workshopOpen?std::min(size.x*.32f,size.y*.72f):size.x;
  const auto action=wheel::drawWheel(model,view,{0,0},{wheelWidth,size.y});
  if(action.config)rock_configurator::setPreviewOpen(!workshopOpen);
  if(rock_configurator::isOpen()) {
   const float left=std::min(size.x*.32f,size.y*.72f);
   (void)rock_configurator::drawImGui(left,12,size.x-left-12,size.y-24);
  }
  rock_configurator::drainPreviewActions();
  bool inventoryChanged=false;
  if(action.useItem){for(auto& category:catalog.items)for(auto& item:category)if(item.id==action.useItem){
   if(item.count)--item.count;inventoryChanged=true;
   model.status="PREVIEW: TAKEN TO HAND  /  "+item.name;
  }}
  if(inventoryChanged)wheel::publishWheelInventory(catalog);
  if(wheel::takeWheelConfigChange() || inventoryChanged) {
   const auto category=model.category;model=wheel::selectedWheelInventory(catalog);model.category=category;view={};
  }
  if(action.close)rock_configurator::setPreviewOpen(false);
  if(ImGui::IsKeyPressed(ImGuiKey_R)){catalog=wheel::demoInventory();wheel::publishWheelInventory(catalog);model=wheel::selectedWheelInventory(catalog);view={};}
  if(ImGui::IsKeyPressed(ImGuiKey_Escape))done=true;
  ImGui::Render();const float clear[4]{0,0,0,1};auto* output=target.Get();context->OMSetRenderTargets(1,&output,nullptr);
  context->ClearRenderTargetView(output,clear);ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
  if(FAILED(swap->Present(1,0)))break;
 }
 ImGui_ImplDX11_Shutdown();ImGui_ImplWin32_Shutdown();ImGui::DestroyContext();
 DestroyWindow(hwnd);UnregisterClassW(wc.lpszClassName,instance);return 0;
}
