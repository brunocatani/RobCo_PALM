#pragma once
#include <filesystem>
#include <array>
#include <cstdint>
#include <span>
#include "PanelResizePolicy.h"
#include "render/NativeRenderer.h"
namespace rock::api::configuration { struct Api; }
namespace rock_configurator {
void onGameDataReady();
void onGameSessionReady();
void shutdown();
bool isOpen() noexcept;
bool isOpening() noexcept;
bool takeInventoryRefreshRequest() noexcept;
void setAvailable(bool available) noexcept;
void close() noexcept;
void openAt(const devui::render::PanelPose& wheelPose);
[[nodiscard]] bool drawImGui(float x=0, float y=0,
 float width=devui::render::kPanelPixelWidth, float height=devui::render::kPanelPixelHeight, bool backRequested=false) noexcept;
// The keyboard panel under Config: shown on the Console tab, and on the Spawner while a search is open.
[[nodiscard]] bool drawKeyboardPanel(float x, float y, float width, float height, bool backRequested = false) noexcept;
void onFrameworkPanelFrame(float physicalWidth, panel_resize::Handle hovered, panel_resize::Handle active) noexcept;
struct ConsoleKeyEvent { bool character{ false }; std::uint32_t code{ 0 }; };
// True while the keyboard panel is up, which is also when the physical keyboard is captured.
[[nodiscard]] bool keyboardInputActive() noexcept;
// Game thread, once per input frame. Opens, moves or closes the keyboard panel, hands the
// physical keyboard's input to the line it types into, and runs the console's queued commands.
void serviceConsole(bool keyboardHeld, bool windowFocused, std::span<const ConsoleKeyEvent> keys);
#ifdef WHEEL_DESKTOP_PREVIEW
void initializePreview(const std::filesystem::path& settingsPath = {});
void initializeRpsPreview(const std::array<std::filesystem::path, 3>& paths, const std::array<bool, 3>& available,
    const rock::api::configuration::Api* rockApi = nullptr, const std::filesystem::path& csahPath = {},
    const std::filesystem::path& csahCatalog = {});
void setPreviewOpen(bool open);
[[nodiscard]] float previewTabCenterX(unsigned index, float width);
void drainPreviewActions();
#endif
}
