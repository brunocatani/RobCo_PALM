#pragma once

#include <imgui.h>

namespace devui::visual
{
    [[nodiscard]] inline ImVec4 accent(float alpha = 1.0f) noexcept
    {
        return { 0.408f, 0.886f, 0.757f, alpha };
    }

    [[nodiscard]] inline ImVec4 text(float alpha = 1.0f) noexcept
    {
        return { 0.914f, 0.949f, 0.937f, alpha };
    }

    [[nodiscard]] inline ImVec4 muted(float alpha = 1.0f) noexcept
    {
        return { 0.573f, 0.651f, 0.643f, alpha };
    }

    [[nodiscard]] inline ImVec4 surface(float alpha = 1.0f) noexcept
    {
        return { 0.051f, 0.098f, 0.114f, alpha };
    }

    inline void applyStyle() noexcept
    {
        auto& style = ImGui::GetStyle();
        style.WindowPadding = { 0.0f, 0.0f };
        style.FramePadding = { 10.0f, 6.0f };
        style.CellPadding = { 10.0f, 6.0f };
        style.ItemSpacing = { 8.0f, 4.0f };
        style.ItemInnerSpacing = { 8.0f, 7.0f };
        style.TouchExtraPadding = { 2.0f, 2.0f };
        style.ScrollbarSize = 18.0f;
        style.GrabMinSize = 20.0f;
        style.WindowRounding = 0.0f;
        style.ChildRounding = 4.0f;
        style.FrameRounding = 5.0f;
        style.PopupRounding = 10.0f;
        style.ScrollbarRounding = 10.0f;
        style.GrabRounding = 10.0f;
        style.TabRounding = 8.0f;
        style.WindowBorderSize = 0.0f;
        style.ChildBorderSize = 0.0f;
        style.FrameBorderSize = 0.0f;
        style.PopupBorderSize = 1.0f;
        style.TabBorderSize = 0.0f;
        style.SeparatorTextBorderSize = 1.0f;
        style.SeparatorTextAlign = { 0.0f, 0.5f };
        style.SeparatorTextPadding = { 18.0f, 5.0f };
        style.AntiAliasedLines = true;
        style.AntiAliasedLinesUseTex = true;
        style.AntiAliasedFill = true;

        auto* colors = style.Colors;
        colors[ImGuiCol_Text] = text();
        colors[ImGuiCol_TextDisabled] = muted();
        colors[ImGuiCol_WindowBg] = ImVec4(0.035f, 0.063f, 0.068f, 1.0f);
        colors[ImGuiCol_ChildBg] = ImVec4(0.055f, 0.090f, 0.095f, 1.0f);
        colors[ImGuiCol_PopupBg] = ImVec4(0.067f, 0.102f, 0.107f, 0.98f);
        colors[ImGuiCol_Border] = ImVec4(0.38f, 0.43f, 0.49f, 0.25f);
        colors[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
        colors[ImGuiCol_FrameBg] = ImVec4(0.086f, 0.130f, 0.135f, 1.0f);
        colors[ImGuiCol_FrameBgHovered] = ImVec4(0.125f, 0.178f, 0.183f, 1.0f);
        colors[ImGuiCol_FrameBgActive] = accent(0.24f);
        colors[ImGuiCol_TitleBg] = ImVec4(0.047f, 0.075f, 0.080f, 1.0f);
        colors[ImGuiCol_TitleBgActive] = colors[ImGuiCol_TitleBg];
        colors[ImGuiCol_Button] = ImVec4(0.09f, 0.137f, 0.142f, 1.0f);
        colors[ImGuiCol_ButtonHovered] = accent(0.22f);
        colors[ImGuiCol_ButtonActive] = accent(0.40f);
        colors[ImGuiCol_Header] = accent(0.13f);
        colors[ImGuiCol_HeaderHovered] = accent(0.22f);
        colors[ImGuiCol_HeaderActive] = accent(0.34f);
        colors[ImGuiCol_CheckMark] = accent();
        colors[ImGuiCol_SliderGrab] = accent(0.82f);
        colors[ImGuiCol_SliderGrabActive] = ImVec4(0.65f, 1.0f, 0.85f, 1.0f);
        colors[ImGuiCol_Separator] = ImVec4(0.38f, 0.43f, 0.49f, 0.22f);
        colors[ImGuiCol_SeparatorHovered] = accent(0.55f);
        colors[ImGuiCol_SeparatorActive] = accent(0.85f);
        colors[ImGuiCol_ScrollbarBg] = ImVec4(0.035f, 0.063f, 0.068f, 0.4f);
        colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.22f, 0.298f, 0.303f, 0.9f);
        colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.32f, 0.36f, 0.41f, 1.0f);
        colors[ImGuiCol_ScrollbarGrabActive] = accent(0.75f);
        colors[ImGuiCol_Tab] = ImVec4(0.055f, 0.090f, 0.095f, 1.0f);
        colors[ImGuiCol_TabHovered] = accent(0.24f);
        colors[ImGuiCol_TabSelected] = accent(0.20f);
        colors[ImGuiCol_NavHighlight] = accent(0.82f);
    }
}
