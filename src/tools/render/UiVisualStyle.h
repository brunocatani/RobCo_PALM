#pragma once

#include <imgui.h>
#include "PalmTheme.h"

namespace devui::visual
{
    [[nodiscard]] inline ImVec4 accent(float alpha = 1.0f) noexcept
    {
        return palm::theme::green(alpha);
    }

    [[nodiscard]] inline ImVec4 text(float alpha = 1.0f) noexcept
    {
        return palm::theme::green(alpha);
    }

    [[nodiscard]] inline ImVec4 muted(float alpha = 1.0f) noexcept
    {
        return palm::theme::muted(alpha);
    }

    [[nodiscard]] inline ImVec4 surface(float alpha = 1.0f) noexcept
    {
        return palm::theme::ink(alpha);
    }

    inline void applyStyle() noexcept
    {
        auto& style = ImGui::GetStyle();
        style.WindowPadding = { 0.0f, 0.0f };
        style.FramePadding = { 10.0f, 6.0f };
        style.CellPadding = { 18.0f, 8.0f };
        style.ItemSpacing = { 8.0f, 4.0f };
        style.ItemInnerSpacing = { 8.0f, 7.0f };
        style.TouchExtraPadding = { 2.0f, 2.0f };
        style.ScrollbarSize = 18.0f;
        style.GrabMinSize = 20.0f;
        style.WindowRounding = 0.0f;
        style.ChildRounding = 0.0f;
        style.FrameRounding = 0.0f;
        style.PopupRounding = 0.0f;
        style.ScrollbarRounding = 0.0f;
        style.GrabRounding = 0.0f;
        style.TabRounding = 0.0f;
        style.WindowBorderSize = 0.0f;
        style.ChildBorderSize = 0.0f;
        style.FrameBorderSize = 1.0f;
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
        colors[ImGuiCol_WindowBg] = surface();
        colors[ImGuiCol_ChildBg] = ImVec4(0, 0, 0, 0);
        colors[ImGuiCol_PopupBg] = surface();
        colors[ImGuiCol_Border] = palm::theme::border();
        colors[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
        colors[ImGuiCol_FrameBg] = surface();
        colors[ImGuiCol_FrameBgHovered] = accent(0.12f);
        colors[ImGuiCol_FrameBgActive] = accent(0.18f);
        colors[ImGuiCol_TitleBg] = surface();
        colors[ImGuiCol_TitleBgActive] = colors[ImGuiCol_TitleBg];
        colors[ImGuiCol_TitleBgCollapsed] = surface();
        colors[ImGuiCol_MenuBarBg] = surface();
        colors[ImGuiCol_Button] = surface();
        colors[ImGuiCol_ButtonHovered] = accent(0.16f);
        colors[ImGuiCol_ButtonActive] = accent(0.24f);
        colors[ImGuiCol_Header] = accent(0.13f);
        colors[ImGuiCol_HeaderHovered] = accent(0.22f);
        colors[ImGuiCol_HeaderActive] = accent(0.34f);
        colors[ImGuiCol_CheckMark] = surface();
        colors[ImGuiCol_CheckboxSelectedBg] = accent();
        colors[ImGuiCol_SliderGrab] = accent(0.3f);
        colors[ImGuiCol_SliderGrabActive] = accent(0.45f);
        colors[ImGuiCol_Separator] = palm::theme::border();
        colors[ImGuiCol_SeparatorHovered] = accent(0.55f);
        colors[ImGuiCol_SeparatorActive] = accent(0.85f);
        colors[ImGuiCol_ScrollbarBg] = surface();
        colors[ImGuiCol_ScrollbarGrab] = accent(0.4f);
        colors[ImGuiCol_ScrollbarGrabHovered] = accent(0.75f);
        colors[ImGuiCol_ScrollbarGrabActive] = accent();
        colors[ImGuiCol_Tab] = surface();
        colors[ImGuiCol_TabHovered] = accent(0.24f);
        colors[ImGuiCol_TabSelected] = accent(0.20f);
        colors[ImGuiCol_NavHighlight] = accent(0.82f);
        colors[ImGuiCol_TableHeaderBg] = accent(0.06f);
        colors[ImGuiCol_TableRowBg] = surface(0.4f);
        colors[ImGuiCol_TableRowBgAlt] = accent(0.025f);
        colors[ImGuiCol_TableBorderStrong] = palm::theme::border();
        colors[ImGuiCol_TableBorderLight] = accent(0.16f);
        colors[ImGuiCol_TextLink] = accent();
        colors[ImGuiCol_TextSelectedBg] = accent(0.25f);
        colors[ImGuiCol_InputTextCursor] = accent();
        colors[ImGuiCol_ResizeGrip] = accent(0.2f);
        colors[ImGuiCol_ResizeGripHovered] = accent(0.7f);
        colors[ImGuiCol_ResizeGripActive] = accent();
        colors[ImGuiCol_TabSelectedOverline] = accent();
        colors[ImGuiCol_TabDimmed] = surface();
        colors[ImGuiCol_TabDimmedSelected] = accent(0.12f);
        colors[ImGuiCol_TabDimmedSelectedOverline] = muted();
        colors[ImGuiCol_PlotLines] = accent();
        colors[ImGuiCol_PlotLinesHovered] = accent();
        colors[ImGuiCol_PlotHistogram] = accent(0.6f);
        colors[ImGuiCol_PlotHistogramHovered] = accent();
        colors[ImGuiCol_TreeLines] = palm::theme::border();
        colors[ImGuiCol_DragDropTarget] = accent();
        colors[ImGuiCol_DragDropTargetBg] = accent(0.12f);
        colors[ImGuiCol_UnsavedMarker] = accent();
        colors[ImGuiCol_NavWindowingHighlight] = accent(0.5f);
        colors[ImGuiCol_NavWindowingDimBg] = surface(0.8f);
        colors[ImGuiCol_ModalWindowDimBg] = surface(0.8f);
    }
}
