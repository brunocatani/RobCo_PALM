#include "PCH.h"

#include "ConsoleTab.h"

#include "ConsoleService.h"
#include "render/ConfigUi.h"

namespace rock_configurator
{
    enum class ConsoleTab::KeyAction : std::uint8_t
    {
        Text,
        Backspace,
        Delete,
        Run,
        Clear,
        Shift,
        Caps,
        Left,
        Right,
        Home,
        End,
        Previous,
        Next,
    };

    struct ConsoleTab::VirtualKey
    {
        const char* label;
        const char* shifted;
        float units;
        KeyAction action;
    };

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
        constexpr float kKeyHeight = 54.0f;
        constexpr float kRowUnits = 14.5f;
        constexpr float kMinimumLogHeight = 120.0f;

        [[nodiscard]] ImU32 packed(const ImVec4& color) noexcept { return ImGui::ColorConvertFloat4ToU32(color); }

        // Draws the label itself: ImGui's "##" ID syntax cannot show a '#' key.
        [[nodiscard]] bool keyButton(const char* label, ImVec2 size, bool selected, bool repeats)
        {
            if (repeats) {
                ImGui::PushItemFlag(ImGuiItemFlags_ButtonRepeat, true);
            }
            const ImVec2 minimum = ImGui::GetCursorScreenPos();
            const bool clicked = ImGui::InvisibleButton("key", size);
            if (repeats) {
                ImGui::PopItemFlag();
            }
            const bool hovered = ImGui::IsItemHovered();
            const ImVec2 maximum{ minimum.x + size.x, minimum.y + size.y };
            auto* draw = ImGui::GetWindowDrawList();
            if (selected || hovered) {
                draw->AddRectFilled(minimum, maximum, packed(devui::visual::accent()));
            }
            draw->AddRect(minimum, maximum, packed(palm::theme::border()));
            auto* font = ImGui::GetFont();
            const float fontSize = ImGui::GetFontSize();
            const auto extent = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, label);
            draw->AddText(font, fontSize,
                { minimum.x + (size.x - extent.x) * 0.5f, minimum.y + (size.y - extent.y) * 0.5f },
                packed(selected || hovered ? devui::visual::surface() : devui::visual::text()), label);
            return clicked;
        }
    }

    void ConsoleTab::draw(const ConsoleStatus& status)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { kGap, kGap });
        ImGui::SetCursorPos({ kPadding, kPadding });
        ImGui::BeginGroup();
        const float bodyHeight = ImGui::GetWindowHeight() - kPadding * 2.0f;
        const float keyboardHeight = _keyboardVisible ? 5.0f * (kKeyHeight + kGap) : 0.0f;
        const float logHeight = (std::max)(kMinimumLogHeight,
            bodyHeight - kStatusHeight - kCommandHeight - keyboardHeight - kGap * 3.0f);
        drawStatusRow(status);
        drawLog(logHeight);
        drawCommandRow();
        if (_keyboardVisible) {
            drawKeyboard();
        }
        ImGui::EndGroup();
        ImGui::PopStyleVar();
    }

    void ConsoleTab::drawStatusRow(const ConsoleStatus& status)
    {
        constexpr float kClearWidth = 130.0f;
        constexpr float kLatestWidth = 110.0f;
        constexpr float kKeyboardToggleWidth = 180.0f;
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
        const float buttons = kClearWidth + kLatestWidth + kKeyboardToggleWidth + kGap * 2.0f;
        ImGui::SetCursorPos({ start + rowWidth - buttons, top });
        if (devui::visual::button("Clear log", { kClearWidth, kStatusHeight })) {
            _rows.clear();
        }
        ImGui::SameLine();
        if (devui::visual::button("Latest", { kLatestWidth, kStatusHeight }, _followLatest)) {
            _followLatest = true;
        }
        ImGui::SameLine();
        if (devui::visual::button(_keyboardVisible ? "Hide keyboard" : "Show keyboard", { kKeyboardToggleWidth, kStatusHeight })) {
            _keyboardVisible = !_keyboardVisible;
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
                _followLatest = _followLatest ? ImGui::GetIO().MouseWheel >= 0.0f || !ImGui::IsWindowHovered() :
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

    void ConsoleTab::drawCommandText(float width, float height)
    {
        const ImVec2 minimum = ImGui::GetCursorScreenPos();
        const ImVec2 maximum{ minimum.x + width, minimum.y + height };
        auto* draw = ImGui::GetWindowDrawList();
        draw->AddRect(minimum, maximum, packed(devui::visual::accent(0.7f)));
        ImGui::Dummy({ width, height });

        Font font(FontRole::Mono, 26.0f);
        auto* face = ImGui::GetFont();
        const float size = ImGui::GetFontSize();
        constexpr float kInset = 12.0f;
        const float textWidth = width - kInset * 2.0f;
        const float textY = minimum.y + (height - size) * 0.5f;
        const auto& text = _line.text();
        if (text.empty()) {
            draw->AddText(face, size, { minimum.x + kInset, textY }, packed(devui::visual::muted()), "Type a console command");
        }
        const float caretOffset = face->CalcTextSizeA(size, FLT_MAX, 0.0f, text.data(), text.data() + _line.caret()).x;
        if (caretOffset - _commandScroll > textWidth) {
            _commandScroll = caretOffset - textWidth;
        } else if (caretOffset < _commandScroll) {
            _commandScroll = caretOffset;
        }
        draw->PushClipRect({ minimum.x + kInset, minimum.y }, { maximum.x - kInset, maximum.y }, true);
        const float originX = minimum.x + kInset - _commandScroll;
        draw->AddText(face, size, { originX, textY }, packed(devui::visual::text()), text.data(), text.data() + text.size());
        if (std::fmod(ImGui::GetTime(), 1.0) < 0.6) {
            draw->AddRectFilled({ originX + caretOffset, textY }, { originX + caretOffset + 2.0f, textY + size }, packed(devui::visual::accent()));
        }
        draw->PopClipRect();
    }

    void ConsoleTab::drawCommandRow()
    {
        constexpr float kRunWidth = 120.0f;
        const float width = ImGui::GetWindowWidth() - kPadding * 2.0f;
        drawCommandText(width - kRunWidth - kGap, kCommandHeight);
        ImGui::SameLine();
        Font font(FontRole::Medium, 24.0f);
        if (devui::visual::button("Run", { kRunWidth, kCommandHeight })) {
            submit();
        }
    }

    void ConsoleTab::pressVirtualKey(const VirtualKey& key)
    {
        switch (key.action) {
        case KeyAction::Text: {
            const bool letter = key.label[0] >= 'a' && key.label[0] <= 'z';
            _line.insert((letter ? _shift != _caps : _shift) ? key.shifted : key.label);
            _shift = false;
            break;
        }
        case KeyAction::Backspace: _line.backspace(); break;
        case KeyAction::Delete: _line.erase(); break;
        case KeyAction::Run: submit(); break;
        case KeyAction::Clear: _line.clear(); break;
        case KeyAction::Shift: _shift = !_shift; break;
        case KeyAction::Caps: _caps = !_caps; break;
        case KeyAction::Left: _line.moveLeft(); break;
        case KeyAction::Right: _line.moveRight(); break;
        case KeyAction::Home: _line.moveHome(); break;
        case KeyAction::End: _line.moveEnd(); break;
        case KeyAction::Previous: _line.historyPrevious(); break;
        case KeyAction::Next: _line.historyNext(); break;
        }
    }

    void ConsoleTab::drawKeyboard()
    {
        using A = KeyAction;
        // Every row adds up to kRowUnits, so the edges line up. US layout: the
        // characters console commands use, whatever the physical keyboard is.
        static constexpr VirtualKey row0[]{ {"`","~",1,A::Text},{"1","!",1,A::Text},{"2","@",1,A::Text},{"3","#",1,A::Text},{"4","$",1,A::Text},{"5","%",1,A::Text},{"6","^",1,A::Text},{"7","&",1,A::Text},{"8","*",1,A::Text},{"9","(",1,A::Text},{"0",")",1,A::Text},{"-","_",1,A::Text},{"=","+",1,A::Text},{"Back","Back",1.5f,A::Backspace} };
        static constexpr VirtualKey row1[]{ {"Caps","Caps",1.5f,A::Caps},{"q","Q",1,A::Text},{"w","W",1,A::Text},{"e","E",1,A::Text},{"r","R",1,A::Text},{"t","T",1,A::Text},{"y","Y",1,A::Text},{"u","U",1,A::Text},{"i","I",1,A::Text},{"o","O",1,A::Text},{"p","P",1,A::Text},{"[","{",1,A::Text},{"]","}",1,A::Text},{"\\","|",1,A::Text} };
        static constexpr VirtualKey row2[]{ {"Clear","Clear",1.75f,A::Clear},{"a","A",1,A::Text},{"s","S",1,A::Text},{"d","D",1,A::Text},{"f","F",1,A::Text},{"g","G",1,A::Text},{"h","H",1,A::Text},{"j","J",1,A::Text},{"k","K",1,A::Text},{"l","L",1,A::Text},{";",":",1,A::Text},{"'","\"",1,A::Text},{"Enter","Enter",1.75f,A::Run} };
        static constexpr VirtualKey row3[]{ {"Shift","Shift",2.25f,A::Shift},{"z","Z",1,A::Text},{"x","X",1,A::Text},{"c","C",1,A::Text},{"v","V",1,A::Text},{"b","B",1,A::Text},{"n","N",1,A::Text},{"m","M",1,A::Text},{",","<",1,A::Text},{".",">",1,A::Text},{"/","?",1,A::Text},{"Shift","Shift",2.25f,A::Shift} };
        static constexpr VirtualKey row4[]{ {"Prev","Prev",1.25f,A::Previous},{"Next","Next",1.25f,A::Next},{"Left","Left",1.25f,A::Left},{"Right","Right",1.25f,A::Right},{"Space","Space",6.5f,A::Text},{"Home","Home",1,A::Home},{"End","End",1,A::End},{"Del","Del",1,A::Delete} };
        struct Row { const VirtualKey* keys; std::size_t count; };
        static constexpr Row rows[]{ {row0,std::size(row0)},{row1,std::size(row1)},{row2,std::size(row2)},{row3,std::size(row3)},{row4,std::size(row4)} };

        const float rowWidth = ImGui::GetWindowWidth() - kPadding * 2.0f;
        Font font(FontRole::Medium, 26.0f);
        ImGui::PushID("console-keyboard");
        int id = 0;
        for (const auto& row : rows) {
            const float unit = (rowWidth - kGap * static_cast<float>(row.count - 1)) / kRowUnits;
            for (std::size_t index = 0; index < row.count; ++index) {
                const auto& key = row.keys[index];
                if (index > 0) {
                    ImGui::SameLine();
                }
                const bool letter = key.action == A::Text && key.label[0] >= 'a' && key.label[0] <= 'z';
                const bool upper = letter ? _shift != _caps : _shift;
                const bool space = key.action == A::Text && std::strcmp(key.label, "Space") == 0;
                const bool repeats = key.action == A::Backspace || key.action == A::Delete || key.action == A::Left || key.action == A::Right;
                const bool selected = (key.action == A::Shift && _shift) || (key.action == A::Caps && _caps);
                ImGui::PushID(id++);
                if (keyButton(upper && !space ? key.shifted : key.label, { unit * key.units, kKeyHeight }, selected, repeats)) {
                    if (space) {
                        _line.insert(" ");
                        _shift = false;
                    } else {
                        pressVirtualKey(key);
                    }
                }
                ImGui::PopID();
            }
        }
        ImGui::PopID();
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
        // The engine's WM_CHAR path gives one code unit per key, so no surrogate pairs.
        const auto unit = static_cast<wchar_t>(utf16Unit);
        std::array<char, 4> utf8{};
        const auto length = WideCharToMultiByte(CP_UTF8, 0, &unit, 1, utf8.data(), static_cast<int>(utf8.size()), nullptr, nullptr);
        if (length > 0) {
            _line.insert({ utf8.data(), static_cast<std::size_t>(length) });
        }
    }

    void ConsoleTab::onKey(std::uint32_t virtualKey)
    {
        switch (virtualKey) {
        case VK_RETURN: submit(); break;
        case VK_BACK: _line.backspace(); break;
        case VK_DELETE: _line.erase(); break;
        case VK_ESCAPE: _line.clear(); break;
        case VK_LEFT: _line.moveLeft(); break;
        case VK_RIGHT: _line.moveRight(); break;
        case VK_HOME: _line.moveHome(); break;
        case VK_END: _line.moveEnd(); break;
        case VK_UP: _line.historyPrevious(); break;
        case VK_DOWN: _line.historyNext(); break;
        case VK_PRIOR: _pageRequest = -1.0f; break;
        case VK_NEXT: _pageRequest = 1.0f; break;
        default: break;
        }
    }

    std::vector<std::string> ConsoleTab::takeCommands()
    {
        return std::exchange(_commands, {});
    }
}
