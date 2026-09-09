#include "ConfiguratorRuntime.h"
#include "WheelConfig.h"
#include "WheelView.h"
#include "Fonts.h"
#include "render/ConfigUi.h"
#include <Windows.h>
#include <d3d11.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>

using Microsoft::WRL::ComPtr;
void check(HRESULT hr) { if (FAILED(hr)) throw std::runtime_error("Offscreen capture failed: " + std::to_string(hr)); }

// Renders the real Config widgets without creating a window or using the desktop.
int main(int argc, char** argv) {
 if(argc<2)return 1;
 try {
  check(CoInitializeEx(nullptr, COINIT_MULTITHREADED));
  const int width=argc>2?std::stoi(argv[2]):1760, height=900;
  const std::filesystem::path output=argv[1];std::filesystem::create_directories(output);
  ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;
  check(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context));
  D3D11_TEXTURE2D_DESC desc{};desc.Width=width;desc.Height=height;desc.MipLevels=desc.ArraySize=1;
  desc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;desc.SampleDesc.Count=1;desc.BindFlags=D3D11_BIND_RENDER_TARGET;
  ComPtr<ID3D11Texture2D> texture;check(device->CreateTexture2D(&desc,nullptr,&texture));
  ComPtr<ID3D11RenderTargetView> target;check(device->CreateRenderTargetView(texture.Get(),nullptr,&target));
  ImGui::CreateContext();wheel::prepareFonts();wheel::installFonts();devui::render::PrepareFonts();devui::visual::applyStyle();
  ImGui_ImplDX11_Init(device.Get(),context.Get());
  auto& io=ImGui::GetIO();io.DisplaySize={static_cast<float>(width),static_cast<float>(height)};io.DeltaTime=1.0f/90;
  wheel::publishWheelInventory(wheel::demoInventory());rock_configurator::initializePreview();rock_configurator::setPreviewOpen(true);
  bool showWheel=false;wheel::Model wheelModel;wheel::View wheelView;
  const auto frame=[&] {
   ImGui_ImplDX11_NewFrame();ImGui::NewFrame();
   if(showWheel)(void)wheel::drawWheel(wheelModel,wheelView);
   else (void)rock_configurator::drawImGui(0,0,static_cast<float>(width),static_cast<float>(height));
   ImGui::Render();rock_configurator::drainPreviewActions();
   auto* view=target.Get();context->OMSetRenderTargets(1,&view,nullptr);
   const float clear[4]{0,0,0,1};context->ClearRenderTargetView(view,clear);ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
  };
  const auto settle=[&]{for(int i=0;i<10;++i)frame();};
  const auto click=[&](float x,float y){io.AddMousePosEvent(x,y);frame();io.AddMouseButtonEvent(0,true);frame();io.AddMouseButtonEvent(0,false);settle();};
  const auto save=[&](const wchar_t* name) {
   io.AddMousePosEvent(-100,-100);settle();
   auto cpuDesc=desc;cpuDesc.BindFlags=0;cpuDesc.Usage=D3D11_USAGE_STAGING;cpuDesc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
   ComPtr<ID3D11Texture2D> cpu;check(device->CreateTexture2D(&cpuDesc,nullptr,&cpu));context->CopyResource(cpu.Get(),texture.Get());
   D3D11_MAPPED_SUBRESOURCE map{};check(context->Map(cpu.Get(),0,D3D11_MAP_READ,0,&map));
   ComPtr<IWICImagingFactory> factory;check(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)));
   ComPtr<IWICStream> stream;check(factory->CreateStream(&stream));check(stream->InitializeFromFilename((output/name).c_str(),GENERIC_WRITE));
   ComPtr<IWICBitmapEncoder> encoder;check(factory->CreateEncoder(GUID_ContainerFormatPng,nullptr,&encoder));check(encoder->Initialize(stream.Get(),WICBitmapEncoderNoCache));
   ComPtr<IWICBitmapFrameEncode> png;check(encoder->CreateNewFrame(&png,nullptr));check(png->Initialize(nullptr));check(png->SetSize(width,height));
   WICPixelFormatGUID format=GUID_WICPixelFormat32bppBGRA;check(png->SetPixelFormat(&format));
   check(png->WritePixels(height,map.RowPitch,map.RowPitch*height,static_cast<BYTE*>(map.pData)));check(png->Commit());check(encoder->Commit());
   context->Unmap(cpu.Get(),0);
  };
  const float rail=devui::visual::railWidth(static_cast<float>(width));
  settle();
  click(rail+60,344);click(rail+60,392);click(rail+60,440);
  save(L"wheel-items.png");
  click(100,367);click(rail+60,344);click(rail+60,392);save(L"weapon-favorites.png");
  click(100,429);click(rail+60,344);save(L"armor-favorites.png");
  wheelModel=wheel::selectedWheelInventory(wheel::demoInventory());wheelModel.category=wheel::Category::Weapons;
  showWheel=true;save(L"equipment-wheel.png");showWheel=false;settle();
  click(rail+250,55);save(L"rock-settings.png");
  click(100,385);save(L"rock-settings-controls.png");
  click(rail+440,55);save(L"spawner.png");
  click(rail+100,335);save(L"item-actions.png");
  ImGui_ImplDX11_Shutdown();ImGui::DestroyContext();CoUninitialize();
  std::cout<<"Rendered Config views to "<<output.string()<<'\n';return 0;
 }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
