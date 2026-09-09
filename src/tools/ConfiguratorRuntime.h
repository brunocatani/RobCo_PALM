#pragma once
#include "PanelResizePolicy.h"
#include "render/NativeRenderer.h"
namespace rock_configurator {
void onGameDataReady();
void onGameSessionReady();
void shutdown();
bool isOpen() noexcept;
bool takeInventoryRefreshRequest() noexcept;
void setAvailable(bool available) noexcept;
void close() noexcept;
void openBeside(const devui::render::PanelPose& wheelPose);
[[nodiscard]] bool drawImGui(float x=0, float y=0,
 float width=devui::render::kPanelPixelWidth, float height=devui::render::kPanelPixelHeight) noexcept;
void onFrameworkPanelFrame(float physicalWidth, panel_resize::Handle hovered, panel_resize::Handle active) noexcept;
#ifdef WHEEL_DESKTOP_PREVIEW
void initializePreview();
void setPreviewOpen(bool open);
void drainPreviewActions();
#endif
}
