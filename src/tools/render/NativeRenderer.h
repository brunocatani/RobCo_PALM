#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

struct ImFont;

// The font roles PALM's settings pages draw with. In game they are the RPS UI settings
// menu's theme faces (src/SettingsMenu.cpp); the desktop preview loads its own.
namespace devui::render
{
    enum class FontRole : std::uint8_t
    {
        Body,
        Medium,
        Heading,
        Display,
        Mono,
        Count,
    };
    inline constexpr std::array<float,static_cast<std::size_t>(FontRole::Count)> kFontSizes{20,21,27,32,18};

    void PrepareFonts() noexcept;
    [[nodiscard]] ImFont* GetFont(FontRole role) noexcept;
    // A role's size, scaled by the theme's body size.
    [[nodiscard]] float FontSize(FontRole role) noexcept;
}
