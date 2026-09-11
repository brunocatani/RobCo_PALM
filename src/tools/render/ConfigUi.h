#pragma once
#include "NativeRenderer.h"
#include "UiVisualStyle.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <cfloat>
#include <string_view>
#include <cstring>

namespace devui::visual {
inline float railWidth(float width) { return std::clamp(width * 0.17f, 280.0f, 320.0f); }
struct ReadableLabel {
    std::array<char, 192> text{};
    explicit ReadableLabel(std::string_view key, bool stripTypePrefix = false) {
        std::size_t out = 0;
        const std::size_t begin = stripTypePrefix && key.size() > 1 &&
            (key[0] == 'b' || key[0] == 'f' || key[0] == 'i' || key[0] == 's') && std::isupper(static_cast<unsigned char>(key[1])) ? 1 : 0;
        for (std::size_t i = begin; i < key.size() && out + 2 < text.size(); ++i) {
            if (i > begin && std::isupper(static_cast<unsigned char>(key[i])) &&
                (std::islower(static_cast<unsigned char>(key[i-1])) ||
                (std::isupper(static_cast<unsigned char>(key[i-1])) && i+1 < key.size() && std::islower(static_cast<unsigned char>(key[i+1]))))) text[out++] = ' ';
            text[out++] = key[i] == '_' ? ' ' : key[i];
        }
    }
};
inline ImU32 color(ImVec4 value) { return ImGui::ColorConvertFloat4ToU32(value); }
inline bool button(const char* label,ImVec2 size={},bool selected=false) {
    // Keep native button input/IDs; only render the label with inverted ink.
    ImGui::PushStyleColor(ImGuiCol_Text,ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,accent());
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,accent());
    if(selected)ImGui::PushStyleColor(ImGuiCol_Button,accent());
    const bool clicked=ImGui::Button(label,size);
    if(selected)ImGui::PopStyleColor();
    ImGui::PopStyleColor(3);
    const auto minimum=ImGui::GetItemRectMin(),maximum=ImGui::GetItemRectMax();
    const char* end=std::strstr(label,"##");
    auto* font=ImGui::GetFont();float fontSize=ImGui::GetFontSize();
    auto extent=font->CalcTextSizeA(fontSize,FLT_MAX,0,label,end);
    const float available=(std::max)(1.f,maximum.x-minimum.x-2*ImGui::GetStyle().FramePadding.x);
    if(extent.x>available){fontSize*=available/extent.x;extent=font->CalcTextSizeA(fontSize,FLT_MAX,0,label,end);}
    const auto align=ImGui::GetStyle().ButtonTextAlign;
    auto* draw=ImGui::GetWindowDrawList();draw->PushClipRect(minimum,maximum,true);
    draw->AddText(font,fontSize,{minimum.x+(maximum.x-minimum.x-extent.x)*align.x,minimum.y+(maximum.y-minimum.y-extent.y)*align.y},
        ImGui::GetColorU32(selected || ImGui::IsItemHovered()?surface():text()),label,end);
    draw->PopClipRect();return clicked;
}
inline bool checkbox(const char* label,bool* value) {
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered,accent());
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive,accent());
    const bool changed=ImGui::Checkbox(label,value);
    ImGui::PopStyleColor(2);return changed;
}
class Font {
public:
    Font(render::FontRole role, float size) { ImGui::PushFont(render::GetFont(role) ? render::GetFont(role) : ImGui::GetFont(), size); }
    ~Font() { ImGui::PopFont(); }
    Font(const Font&) = delete;
    Font& operator=(const Font&) = delete;
};
inline void caption(const char* label) {
    Font font(render::FontRole::Medium, 18);
    ImGui::TextColored(muted(), "%s", label);
}
inline void heading(const char* title, const char* subtitle) {
    { Font font(render::FontRole::Heading, 40); ImGui::TextUnformatted(title); }
    { Font font(render::FontRole::Body, 24); ImGui::TextColored(muted(), "%s", subtitle); }
    ImGui::Dummy({0, 12});
}
inline bool navigation(const char* label, bool selected, const char* badge = nullptr) {
    const auto p = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    constexpr float height = 58;
    const bool clicked = ImGui::InvisibleButton(label, {width, height});
    const bool hovered = ImGui::IsItemHovered();
    auto* draw = ImGui::GetWindowDrawList();
    if (selected || hovered) draw->AddRectFilled(p, {p.x + width, p.y + height}, color(accent()));
    draw->AddRect(p,{p.x+width,p.y+height},color(palm::theme::border()));
    const float right = p.x + width - (badge ? 42 : 12);
    auto* font = render::GetFont(render::FontRole::Medium) ? render::GetFont(render::FontRole::Medium) : ImGui::GetFont();
    const float size = badge ? 20.0f : 26.0f;
    const float wrap = right - p.x - 16;
    const ReadableLabel readable(label);
    const auto extent = font->CalcTextSizeA(size, FLT_MAX, wrap, readable.text.data());
    draw->PushClipRect({p.x + 16, p.y}, {right, p.y + height}, true);
    draw->AddText(font, size, {p.x + 16, p.y + (std::max)(5.0f, (height - extent.y) * 0.5f)},
        color(selected || hovered ? surface() : muted()), readable.text.data(), nullptr, wrap);
    draw->PopClipRect();
    if (badge) draw->AddText(ImGui::GetFont(), 16, {right + 8, p.y + 21}, color(selected || hovered ? surface() : muted()), badge);
    if (hovered) ImGui::SetTooltip("%s", label);
    return clicked;
}
inline void tableHeader(const char* first, const char* second, const char* third = nullptr) {
    Font font(render::FontRole::Medium, 18);
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableNextRow(ImGuiTableRowFlags_Headers, 34);
    ImGui::TableSetColumnIndex(0); ImGui::TableHeader(first);
    ImGui::TableSetColumnIndex(1); ImGui::TableHeader(second);
    if (third) { ImGui::TableSetColumnIndex(2); ImGui::TableHeader(third); }
}
}
