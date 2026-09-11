#pragma once

#include <DirectXMath.h>

#include <cstdint>
#include <array>

struct ImFont;

namespace devui::render
{
    inline constexpr std::uint32_t kPanelPixelWidth = 1440;
    inline constexpr std::uint32_t kPanelPixelHeight = 900;
    inline constexpr float kPanelAspectRatio =
        static_cast<float>(kPanelPixelWidth) / static_cast<float>(kPanelPixelHeight);
    inline constexpr float kDefaultPanelPhysicalWidth = 96.0f;
    inline constexpr float kDefaultPanelPhysicalHeight =
        kDefaultPanelPhysicalWidth / kPanelAspectRatio;
    inline constexpr float kMinimumPanelPhysicalWidth = 72.0f;
    inline constexpr float kMaximumPanelPhysicalWidth = 180.0f;

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

    struct PanelPose
    {
        DirectX::XMFLOAT3 center{};
        DirectX::XMFLOAT3 right{};
        DirectX::XMFLOAT3 up{};
        DirectX::XMFLOAT3 front{};
        float physicalWidth{ kDefaultPanelPhysicalWidth };
        float physicalHeight{ kDefaultPanelPhysicalHeight };
    };

    void PrepareFonts() noexcept;
    [[nodiscard]] ImFont* GetFont(FontRole role) noexcept;
    bool SetPanelOpen(bool open, const PanelPose* pose = nullptr) noexcept;
    void Shutdown() noexcept;
}
