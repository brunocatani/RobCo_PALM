#pragma once
#include <array>
#include "IconCatalog.h"
#include <imgui.h>

namespace wheel {
using IconTextures=std::array<ImTextureID,kIconSheetCount>;
inline void drawIcon(ImDrawList* draw,ImVec2 center,float halfSize,Icon icon,ImU32 tint,
 const IconTextures& textures,bool mirror=false) {
 const auto cell=static_cast<unsigned>(icon);
 if(cell>=static_cast<unsigned>(Icon::Count) || !textures[cell/16])return;
 const float x=(cell%4)*.25f,y=((cell%16)/4)*.25f;
 draw->AddImage(textures[cell/16],{center.x-halfSize,center.y-halfSize},{center.x+halfSize,center.y+halfSize},
  {mirror?x+.25f:x,y},{mirror?x:x+.25f,y+.25f},tint);
}
}
