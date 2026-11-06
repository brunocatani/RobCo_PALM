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
     * The Console tab: scrollback, command line and on-screen keyboard. Everything
     * here is presentation state. The caller holds the configurator runtime mutex
     * around every call, because drawing runs on the render thread and physical keys
     * arrive from the game thread. Commands wait in takeCommands() for the game thread.
     */
    class ConsoleTab
    {
    public:
        void draw(const ConsoleStatus& status);
        void onCharacter(std::uint32_t utf16Unit);
        void onKey(std::uint32_t virtualKey);
        [[nodiscard]] std::vector<std::string> takeCommands();
        [[nodiscard]] bool hasCommands() const noexcept { return !_commands.empty(); }

    private:
        enum class KeyAction : std::uint8_t;
        struct VirtualKey;

        void drawStatusRow(const ConsoleStatus& status);
        void drawLog(float height);
        void drawCommandRow();
        void drawKeyboard();
        void drawCommandText(float width, float height);
        void pressVirtualKey(const VirtualKey& key);
        void submit();
        void addRow(std::string_view line, float wrapWidth);
        void absorbNewLines(float wrapWidth);

        ConsoleLine _line;
        std::deque<std::string> _rows;
        std::vector<std::string> _incoming;
        std::vector<std::string> _commands;
        float _commandScroll{ 0.0f };
        float _pageRequest{ 0.0f };
        bool _followLatest{ true };
        bool _keyboardVisible{ true };
        bool _shift{ false };
        bool _caps{ false };
    };
}
