#pragma once

#include "ConsoleLine.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <vector>

namespace rock_configurator
{
    struct ConsoleStatus
    {
        std::optional<std::uint32_t> selectedReference;
        bool keyboardHeld{ false };
        bool windowFocused{ false };
        bool outputCaptured{ false };
    };

    /**
     * The Console tab: scrollback and command line. The on-screen keyboard lives on its own
     * panel under the Config panel and types into line(). Everything here is presentation
     * state. The caller holds the configurator runtime mutex around every call, because
     * drawing runs on the render thread and physical keys arrive from the game thread.
     * Commands wait in takeCommands() for the game thread.
     */
    class ConsoleTab
    {
    public:
        void draw(const ConsoleStatus& status);
        void onCharacter(std::uint32_t utf16Unit);
        void onKey(std::uint32_t virtualKey);
        void submit();
        [[nodiscard]] ConsoleLine& line() noexcept { return _line; }
        [[nodiscard]] std::vector<std::string> takeCommands();
        [[nodiscard]] bool hasCommands() const noexcept { return !_commands.empty(); }

    private:
        void drawStatusRow(const ConsoleStatus& status);
        void drawLog(float height);
        void drawCommandRow();
        void addRow(std::string_view line, float wrapWidth);
        void absorbNewLines(float wrapWidth);

        ConsoleLine _line;
        std::deque<std::string> _rows;
        std::vector<std::string> _incoming;
        std::vector<std::string> _commands;
        float _commandScroll{ 0.0f };
        float _pageRequest{ 0.0f };
        bool _followLatest{ true };
    };
}
