#include "PCH.h"

#include "ConsoleTab.h"

#include "ConsoleService.h"
#include "OnScreenKeyboard.h"
#include "render/ConfigUi.h"

namespace rock_configurator
{
    namespace
    {
        using Font = devui::visual::Font;
        using devui::render::FontRole;

        constexpr std::size_t kRowLimit = 6000;
        constexpr float kLogFontSize = 22.0f;
        constexpr float kPadding = 16.0f;
        constexpr float kGap = 6.0f;
        constexpr float kStatusHeight = 46.0f;
        constexpr float kCommandHeight = 56.0f;
        constexpr float kMinimumLogHeight = 120.0f;
    }

    void ConsoleTab::draw(const ConsoleStatus& status)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { kGap, kGap });
        ImGui::SetCursorPos({ kPadding, kPadding });
        ImGui::BeginGroup();
        const float bodyHeight = ImGui::GetWindowHeight() - kPadding * 2.0f;
        const float logHeight = (std::max)(kMinimumLogHeight,
            bodyHeight - kStatusHeight - kCommandHeight - kGap * 2.0f);
        drawStatusRow(status);
        drawLog(logHeight);
        drawCommandRow();
        ImGui::EndGroup();
        ImGui::PopStyleVar();
    }

    void ConsoleTab::drawStatusRow(const ConsoleStatus& status)
    {
        constexpr float kClearWidth = 130.0f;
        constexpr float kLatestWidth = 110.0f;
        const float start = ImGui::GetCursorPosX();
        const float rowWidth = ImGui::GetWindowWidth() - kPadding * 2.0f;
        const float top = ImGui::GetCursorPosY();

        std::array<char, 40> selected{};
        if (status.selectedReference) {
            std::snprintf(selected.data(), selected.size(), "Selected: %08X", *status.selectedReference);
        } else {
            std::snprintf(selected.data(), selected.size(), "Selected: none");
        }
        const char* keyboard = !status.keyboardHeld ? "Real keyboard: unavailable" :
            status.windowFocused ? "Real keyboard: typing here" : "Real keyboard: click the game window on your PC";
        {
            Font font(FontRole::Body, 20.0f);
            ImGui::SetCursorPos({ start, top + 12.0f });
            ImGui::TextColored(devui::visual::muted(), "%s", selected.data());
            ImGui::SetCursorPos({ start + 240.0f, top + 12.0f });
            ImGui::TextColored(devui::visual::muted(), "%s", keyboard);
        }

        Font font(FontRole::Medium, 20.0f);
        const float buttons = kClearWidth + kLatestWidth + kGap;
        ImGui::SetCursorPos({ start + rowWidth - buttons, top });
        if (devui::visual::button("Clear log", { kClearWidth, kStatusHeight })) {
            _rows.clear();
        }
        ImGui::SameLine();
        if (devui::visual::button("Latest", { kLatestWidth, kStatusHeight }, _followLatest)) {
            _followLatest = true;
        }
        ImGui::SetCursorPos({ start, top + kStatusHeight + kGap });
    }

    void ConsoleTab::addRow(std::string_view line, float wrapWidth)
    {
        auto* font = ImGui::GetFont();
        const char* begin = line.data();
        const char* end = begin + line.size();
        do {
            const char* wrap = font->CalcWordWrapPosition(kLogFontSize, begin, end, wrapWidth);
            if (wrap <= begin) {
                wrap = (std::min)(begin + 1, end); // One glyph wider than the row.
            }
            _rows.emplace_back(begin, wrap);
            begin = wrap;
            while (begin < end && *begin == ' ') {
                ++begin;
            }
        } while (begin < end);
        while (_rows.size() > kRowLimit) {
            _rows.pop_front();
        }
    }

    void ConsoleTab::absorbNewLines(float wrapWidth)
    {
        console::takeLines(_incoming);
        for (const auto& line : _incoming) {
            addRow(line, wrapWidth);
        }
        _incoming.clear();
    }

    void ConsoleTab::drawLog(float height)
    {
        ImGui::PushStyleColor(ImGuiCol_Border, palm::theme::border());
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 10.0f, 6.0f });
        const float width = ImGui::GetWindowWidth() - kPadding * 2.0f;
        ImGui::BeginChild("console-log", { width, height }, ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding);
        {
            Font font(FontRole::Mono, kLogFontSize);
            // The rows are wrapped once, for the width that leaves room for the scrollbar,
            // so every row has one height and the clipper can skip the rest.
            absorbNewLines(ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ScrollbarSize);

            if (!console::captureInstalled()) {
                ImGui::TextColored(devui::visual::muted(), "Console output is unavailable; see RobCoPALM.log. Commands still run.");
            }
            ImGuiListClipper clipper;
            clipper.Begin(static_cast<int>(_rows.size()), ImGui::GetTextLineHeightWithSpacing());
            while (clipper.Step()) {
                for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                    const auto& row = _rows[static_cast<std::size_t>(i)];
                    ImGui::TextUnformatted(row.data(), row.data() + row.size());
                }
            }

            // Scrolling away from the bottom stops following; returning to it resumes.
            if (_pageRequest != 0.0f) {
                ImGui::SetScrollY(ImGui::GetScrollY() + _pageRequest * ImGui::GetWindowHeight() * 0.9f);
                _pageRequest = 0.0f;
                _followLatest = false;
            } else {
                // A positive wheel value scrolls up, away from the newest row.
                _followLatest = _followLatest ? ImGui::GetIO().MouseWheel <= 0.0f || !ImGui::IsWindowHovered() :
                    ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f;
            }
            if (_followLatest) {
                ImGui::SetScrollHereY(1.0f);
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
    }

    void ConsoleTab::drawCommandRow()
    {
        constexpr float kRunWidth = 120.0f;
        const float width = ImGui::GetWindowWidth() - kPadding * 2.0f;
        drawLineField(_line, width - kRunWidth - kGap, kCommandHeight, 26.0f, "Type a console command", _commandScroll, true);
        ImGui::SameLine();
        Font font(FontRole::Medium, 24.0f);
        if (devui::visual::button("Run", { kRunWidth, kCommandHeight })) {
            submit();
        }
    }

    void ConsoleTab::submit()
    {
        auto command = _line.take();
        if (command.empty()) {
            return;
        }
        _followLatest = true;
        if (!commandIsSafe(command)) {
            // The rows are read on this same call path, so this needs no wrap width yet.
            _rows.push_back("Not run: console commands cannot contain %");
            return;
        }
        _commands.push_back(std::move(command));
    }

    void ConsoleTab::onCharacter(std::uint32_t utf16Unit)
    {
        typeCharacter(_line, utf16Unit);
    }

    void ConsoleTab::onKey(std::uint32_t virtualKey)
    {
        switch (virtualKey) {
        case VK_RETURN: submit(); break;
        case VK_PRIOR: _pageRequest = -1.0f; break;
        case VK_NEXT: _pageRequest = 1.0f; break;
        default: (void)applyEditKey(_line, virtualKey); break;
        }
    }

    std::vector<std::string> ConsoleTab::takeCommands()
    {
        return std::exchange(_commands, {});
    }
}
