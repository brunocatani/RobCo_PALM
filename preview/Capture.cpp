#include "ConfiguratorRuntime.h"
#include "WheelConfig.h"
#include "WheelView.h"
#include "Fonts.h"
#include "IconAtlas.h"
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
#include <cstdio>
#include <cstring>

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
  wheel::IconAtlas icons;if(!icons.create(device.Get(),context.Get()))throw std::runtime_error("Embedded icon atlases unavailable");
  auto& io=ImGui::GetIO();io.DisplaySize={static_cast<float>(width),static_cast<float>(height)};io.DeltaTime=1.0f/90;
  wheel::publishWheelInventory(wheel::demoInventory());rock_configurator::initializePreview();rock_configurator::setPreviewOpen(true);
  bool showWheel=false,showAtlas=false;unsigned atlasSheet=0;wheel::Model wheelModel;wheel::View wheelView;
  const auto frame=[&] {
   ImGui_ImplDX11_NewFrame();ImGui::NewFrame();
   if(showAtlas) {
    const float cell=height/4.f;
    auto* draw=ImGui::GetBackgroundDrawList();
    for(unsigned i=0;i<16;++i)wheel::drawIcon(draw,{(width-height)/2.f+(i%4+.5f)*cell,(i/4+.5f)*cell},cell*.45f,
     static_cast<wheel::Icon>(atlasSheet*16+i),IM_COL32(225,235,240,255),icons.ids());
   }else if(showWheel)(void)wheel::drawWheel(wheelModel,wheelView,{},{},icons.ids());
   else (void)rock_configurator::drawImGui(0,0,static_cast<float>(width),static_cast<float>(height));
   ImGui::Render();rock_configurator::drainPreviewActions();
   auto* view=target.Get();context->OMSetRenderTargets(1,&view,nullptr);
   const float clear[4]{0,0,0,1};context->ClearRenderTargetView(view,clear);ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
  };
  const auto settle=[&]{for(int i=0;i<10;++i)frame();};
  const auto click=[&](float x,float y){io.AddMousePosEvent(x,y);frame();io.AddMouseButtonEvent(0,true);frame();io.AddMouseButtonEvent(0,false);settle();};
  const auto save=[&](const wchar_t* name,ImVec2 pointer={-100,-100}) {
   io.AddMousePosEvent(pointer.x,pointer.y);settle();
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
  const auto tab=[&](unsigned index) {
   const float start=rail+14,step=(std::min)(194.f,(width-342-start)/4);
   click(start+(index+.5f)*step,55);
  };
  settle();
  const bool settingsOnly = argc > 3 && std::string_view(argv[3]) == "--settings-only";
  if (settingsOnly) {
   tab(2);save(L"rock-settings.png");
   click(220,120);save(L"developer-settings.png");
  } else {
  click(rail+60,344);click(rail+60,392);click(rail+60,440);
  save(L"wheel-items.png");
  tab(1);save(L"palm-settings.png");click(110,265);save(L"palm-controls-hold.png");click(rail+410,260);save(L"palm-controls-press.png");click(110,328);save(L"palm-pointer-aim.png");tab(0);
  click(100,367);click(rail+60,344);click(rail+60,392);save(L"weapon-favorites.png");
  click(100,429);click(rail+60,344);save(L"armor-favorites.png");
  wheelModel=wheel::selectedWheelInventory(wheel::demoInventory());wheelModel.category=wheel::Category::Weapons;
  showWheel=true;save(L"equipment-wheel.png");
  wheelModel.gestures.showing=true;wheelModel.gestures.availability.fill(wheel::GestureAvailability::Free);
  save(L"gestures-left.png");
  wheelModel.gestures.left=false;wheelModel.gestures.availability[0]=wheel::GestureAvailability::Busy;
  save(L"gestures-right-busy.png");
  wheelModel=wheel::demoInventory();wheelModel.category=wheel::Category::Aid;save(L"aid-icons.png");
  wheelModel.category=wheel::Category::Food;save(L"food-icons.png");
  wheelModel.category=wheel::Category::Grenades;save(L"grenade-icons.png");
  wheelModel.category=wheel::Category::Armor;save(L"armor-icons.png");
  wheelModel.category=wheel::Category::Weapons;
  wheelModel.items[3]={
   {1,"10mm Pistol",1,false,"",0,wheel::Icon::Pistol10mm},
   {2,"Combat Rifle",1,false,"",0,wheel::Icon::CombatRifle},
   {3,"Minigun",1,false,"",0,wheel::Icon::Minigun},
   {4,"Super Sledge",1,false,"",0,wheel::Icon::SuperSledge},
   {5,"Laser Rifle",1,false,"",0,wheel::Icon::LaserRifle},
   {6,"Plasma Pistol",1,false,"",0,wheel::Icon::PlasmaPistol},
   {7,"Missile Launcher",1,false,"",0,wheel::Icon::MissileLauncher},
   {8,"Combat Knife",1,false,"",0,wheel::Icon::CombatKnife}
  };save(L"weapon-icons.png");
  save(L"center-selected.png",{width*.5f,height*.5f});
  wheelModel.enabled={false,false,false,true,true};wheelModel.gesturesEnabled=false;
  save(L"equipment-only.png");
  wheelModel.enabled.fill(false);save(L"config-only.png");
  // Exercise the same registration and snapshot code that a native mod uses.
  std::array<palm::api::SectionHandle,2> modSections{};
  auto preferences=wheel::snapshotWheelPreferences();
  for(unsigned i=0;i<modSections.size();++i) {
   palm::api::SectionV1 section;
   std::snprintf(section.id,sizeof(section.id),"preview.section%u",i);
   std::strcpy(section.name,i?"Tools":"Magazines");std::strcpy(section.modName,"Example mod");
   section.icon=i?wheel::Icon::Config:wheel::Icon::CombatRifle;
   section.onSelect=+[](std::uint32_t,std::uint64_t,void*) noexcept {};
   if(wheel::sectionRegistry().registerSection(&section,&modSections[i])!=palm::api::Result::Ok)throw std::runtime_error("Preview section registration failed");
   std::array<palm::api::ItemV1,2> entries;
   for(unsigned item=0;item<entries.size();++item) {
    entries[item].id=item+1;entries[item].quantity=3+item;
    entries[item].icon=i?wheel::Icon::Config:item?wheel::Icon::CombatRifle:wheel::Icon::Pistol10mm;
    entries[item].flags=i?0:static_cast<unsigned>(palm::api::ItemFlag::ShowQuantity);
    std::strcpy(entries[item].name,i?(item?"Inspect object":"Mod action"):(item?"Combat Rifle Magazine":"10mm Magazine"));
   }
   if(wheel::sectionRegistry().setItems(modSections[i],entries.data(),static_cast<std::uint32_t>(entries.size()))!=palm::api::Result::Ok)throw std::runtime_error("Preview section items failed");
   if(!preferences.setSectionEnabled(section.id,true))throw std::runtime_error("Preview section visibility failed");
  }
  wheel::restoreWheelPreferences(preferences);wheel::publishWheelInventory(wheel::demoInventory());
  wheelModel=wheel::selectedWheelInventory(wheel::demoInventory());wheelModel.activeSection=modSections[0];
  save(L"mod-sections.png");
  showWheel=false;tab(1);save(L"palm-mod-settings.png");
  showWheel=false;settle();
  showAtlas=true;
  for(atlasSheet=0;atlasSheet<wheel::kIconSheetCount;++atlasSheet) {
   const auto name=L"icon-atlas-"+std::to_wstring(atlasSheet+1)+L".png";save(name.c_str());
  }
  showAtlas=false;
  tab(2);save(L"rock-settings.png");
  click(100,385);save(L"rock-settings-controls.png");
  click(220,120);save(L"developer-settings.png");
  click(385,120);save(L"paper-settings.png");
  click(555,120);save(L"scissors-settings.png");
  tab(3);save(L"spawner.png");
  click(rail+100,335);save(L"item-actions.png");
  }
  ImGui_ImplDX11_Shutdown();ImGui::DestroyContext();CoUninitialize();
  std::cout<<"Rendered Config views to "<<output.string()<<'\n';return 0;
 }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
