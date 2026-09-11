#include "IconAtlas.h"
#include <wincodec.h>
#include <vector>

namespace wheel {
bool IconAtlas::create(ID3D11Device* device,ID3D11DeviceContext* context) {
 clear();if(!device || !context)return false;
 const auto initialized=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
 if(FAILED(initialized) && initialized!=RPC_E_CHANGED_MODE)return false;
 struct ComScope {bool owned;~ComScope(){if(owned)CoUninitialize();}} com{SUCCEEDED(initialized)};
 HMODULE module{};
 if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
    reinterpret_cast<LPCWSTR>(&drawIcon),&module))return false;
 using Microsoft::WRL::ComPtr;
 ComPtr<IWICImagingFactory> factory;
 if(FAILED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory))))return false;
 decltype(_textures) textures;
 for(unsigned sheet=0;sheet<kIconSheetCount;++sheet) {
  const auto resource=FindResourceW(module,MAKEINTRESOURCEW(201+sheet),MAKEINTRESOURCEW(10)); // RT_RCDATA
  if(!resource)return false;
  const auto loaded=LoadResource(module,resource);
  auto* bytes=static_cast<BYTE*>(LockResource(loaded));const DWORD size=SizeofResource(module,resource);
  if(!bytes || !size)return false;
  ComPtr<IWICStream> stream;ComPtr<IWICBitmapDecoder> decoder;ComPtr<IWICBitmapFrameDecode> frame;ComPtr<IWICFormatConverter> rgba;
  if(FAILED(factory->CreateStream(&stream)) || FAILED(stream->InitializeFromMemory(bytes,size)) ||
     FAILED(factory->CreateDecoderFromStream(stream.Get(),nullptr,WICDecodeMetadataCacheOnLoad,&decoder)) ||
     FAILED(decoder->GetFrame(0,&frame)) || FAILED(factory->CreateFormatConverter(&rgba)) ||
     FAILED(rgba->Initialize(frame.Get(),GUID_WICPixelFormat32bppRGBA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom)))return false;
  UINT width{},height{};
  if(FAILED(rgba->GetSize(&width,&height)) || width<256 || width!=height || width>4096)return false;
  std::vector<BYTE> pixels(static_cast<std::size_t>(width)*height*4);
  if(FAILED(rgba->CopyPixels(nullptr,width*4,static_cast<UINT>(pixels.size()),pixels.data())))return false;
  // Source art is a monochrome coverage mask: black contributes no ink.
  // Preserve its generated alpha and tint the visible outline in ImGui.
  for(std::size_t i=0;i<pixels.size();i+=4) {
   const unsigned ink=(static_cast<unsigned>(pixels[i])+pixels[i+1]+pixels[i+2])/3;
   pixels[i+3]=static_cast<BYTE>((ink*pixels[i+3]+127)/255);
   pixels[i]=pixels[i+1]=pixels[i+2]=255;
  }
  D3D11_TEXTURE2D_DESC description{};description.Width=width;description.Height=height;
  description.MipLevels=0;description.ArraySize=1;description.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
  description.SampleDesc.Count=1;description.Usage=D3D11_USAGE_DEFAULT;
  description.BindFlags=D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_RENDER_TARGET;
  description.MiscFlags=D3D11_RESOURCE_MISC_GENERATE_MIPS;
  ComPtr<ID3D11Texture2D> texture;
  if(FAILED(device->CreateTexture2D(&description,nullptr,&texture)) ||
     FAILED(device->CreateShaderResourceView(texture.Get(),nullptr,&textures[sheet])))return false;
  context->UpdateSubresource(texture.Get(),0,nullptr,pixels.data(),width*4,0);
  // Thin outlines need proper minification at the wheel's smaller icon sizes.
  context->GenerateMips(textures[sheet].Get());
 }
 _textures=std::move(textures);return true;
}
}
