#pragma once
#include "Icons.h"
#include <d3d11.h>
#include <wrl/client.h>

namespace wheel {
// Render-context-owned resources, recreated with the D3D device. Embedded
// masks are decoded only during renderer initialization, never per frame.
class IconAtlas {
public:
 bool create(ID3D11Device* device,ID3D11DeviceContext* context);
 void clear(){_textures={};}
 IconTextures ids() const {
  IconTextures result{};
  for(unsigned i=0;i<kIconSheetCount;++i)result[i]=static_cast<ImTextureID>(reinterpret_cast<std::uintptr_t>(_textures[i].Get()));
  return result;
 }
private:
 std::array<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>,kIconSheetCount> _textures;
};
}
