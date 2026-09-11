#pragma once
#include <imgui.h>

namespace palm::theme {
inline ImVec4 green(float alpha=1.f) noexcept {return {124/255.f,240/255.f,108/255.f,alpha};}
inline ImVec4 ink(float alpha=1.f) noexcept {return {5/255.f,16/255.f,7/255.f,alpha};}
inline ImVec4 muted(float alpha=1.f) noexcept {return green(alpha*.75f);}
inline ImVec4 border(float alpha=1.f) noexcept {return green(alpha/3);}
}
