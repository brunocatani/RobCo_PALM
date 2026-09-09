#pragma once
#include "NativeRenderer.h"
#include "UiVisualStyle.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <cfloat>
#include <string_view>

namespace devui::visual {
inline float railWidth(float width) { return std::clamp(width * 0.17f, 280.0f, 320.0f); }
struct ReadableLabel {
    std::array<char, 192> text{};
    explicit ReadableLabel(std::string_view key, bool stripTypePrefix = false) {
        std::size_t out = 0;
        const std::size_t begin = stripTypePrefix && key.size() > 1 &&
            (key[0] == 'b' || key[0] == 'f' || key[0] == 'i') && std::isupper(static_cast<unsigned char>(key[1])) ? 1 : 0;
        for (std::size_t i = begin; i < key.size() && out + 2 < text.size(); ++i) {
            if (i > begin && std::isupper(static_cast<unsigned char>(key[i])) &&
                (std::islower(static_cast<unsigned char>(key[i-1])) ||
                (std::isupper(static_cast<unsigned char>(key[i-1])) && i+1 < key.size() && std::islower(static_cast<unsigned char>(key[i+1]))))) text[out++] = ' ';
            text[out++] = key[i] == '_' ? ' ' : key[i];
        }
    }
};
inline ImU32 color(ImVec4 value) { return ImGui::ColorConvertFloat4ToU32(value); }
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
    if (selected || hovered) draw->AddRectFilled(p, {p.x + width, p.y + height}, color(accent(selected ? 0.15f : 0.07f)));
    if (selected) draw->AddRectFilled(p, {p.x + 4, p.y + height}, color(accent()));
    const float right = p.x + width - (badge ? 42 : 12);
    auto* font = render::GetFont(render::FontRole::Medium) ? render::GetFont(render::FontRole::Medium) : ImGui::GetFont();
    const float size = badge ? 20.0f : 26.0f;
    const float wrap = right - p.x - 16;
    const ReadableLabel readable(label);
    const auto extent = font->CalcTextSizeA(size, FLT_MAX, wrap, readable.text.data());
    draw->PushClipRect({p.x + 16, p.y}, {right, p.y + height}, true);
    draw->AddText(font, size, {p.x + 16, p.y + (std::max)(5.0f, (height - extent.y) * 0.5f)},
        color(selected ? text() : muted()), readable.text.data(), nullptr, wrap);
    draw->PopClipRect();
    if (badge) draw->AddText(ImGui::GetFont(), 16, {right + 8, p.y + 21}, color(muted()), badge);
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
