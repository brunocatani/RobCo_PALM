#include <imgui_impl_null.h>
#include "PanelInput.h"
#include <imgui_internal.h>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
void draw() {
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize({600, 400});
    ImGui::Begin("Panel", nullptr, ImGuiWindowFlags_NoSavedSettings);
    ImGui::BeginChild("Rows", {500, 300});
    for (int i = 0; i < 300; ++i) ImGui::Text("Setting %d", i);
    ImGui::EndChild();
    ImGui::End();
    ImGui::Render();
}
void testInput(float rate, bool snapshotMode) {
    ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.DisplaySize = {600, 400};
    io.DeltaTime = 1.0f / rate;
    if (snapshotMode) wheel::configurePanelInput(io);
    // Headless: ImGui's null renderer accepts the font atlas's texture requests.
    ImGui_ImplNullRender_Init();
    for (int i = 0; i < 3; ++i) { ImGui::NewFrame(); draw(); }
    io.AddMousePosEvent(100, 100);
    ImGui::NewFrame(); draw();
    auto* rows = ImGui::GetCurrentContext()->HoveredWindow;
    require(rows && rows->ScrollMax.y > 0, "scrollable child fixture missing");
    auto* parent = rows->ParentWindow;
    const float wheelDelta = -5.5f * io.DeltaTime;
    for (int i = 0; i < static_cast<int>(rate); ++i) {
        // Pointer motion and continuous stick scrolling arrive in one snapshot.
        const float x = 100.0f + static_cast<float>(i % 7);
        io.AddMousePosEvent(x, 100);
        io.AddMouseButtonEvent(0, false);
        io.AddMouseWheelEvent(0, wheelDelta);
        const float before = rows->Scroll.y;
        ImGui::NewFrame();
        if (snapshotMode) {
            require(ImGui::GetCurrentContext()->InputEventsQueue.empty(), "input backlog grew while hand moved");
            require(io.MousePos.x == x && std::fabs(io.MouseWheel - wheelDelta) < 0.00001f,
                "pointer and stick were not consumed in the same frame");
        }
        draw();
        if (snapshotMode) {
            require(rows->Scroll.y > before, "moving-pointer scrolling skipped a frame");
            require(parent->Scroll.y == 0, "child scrolling moved the parent");
        }
    }
    if (!snapshotMode) {
        require(ImGui::GetCurrentContext()->InputEventsQueue.Size > 20,
            "default trickling no longer reproduces the moving-pointer backlog");
        ImGui::DestroyContext();
        return;
    }
    // Release must stop on the next snapshot, without draining old wheel events.
    const float stoppedAt = rows->Scroll.y;
    for (int i = 0; i < 8; ++i) {
        io.AddMousePosEvent(110.0f + i, 100);
        ImGui::NewFrame();
        require(io.MouseWheel == 0, "wheel input continued after stick release");
        draw();
        require(rows->Scroll.y == stoppedAt, "scroll continued after stick release");
    }
    io.AddMousePosEvent(120, 100);
    io.AddMouseWheelEvent(0, -wheelDelta);
    ImGui::NewFrame();
    draw();
    require(rows->Scroll.y < stoppedAt, "scroll reversal was delayed");
    // Consecutive snapshots preserve press/release edges despite pointer motion.
    for (int i = 0; i < 4; ++i) {
        const bool down = i % 2 == 0;
        io.AddMousePosEvent(125.0f + i, 100);
        io.AddMouseButtonEvent(0, down);
        ImGui::NewFrame();
        require(down ? ImGui::IsMouseClicked(0) : ImGui::IsMouseReleased(0),
            "sampled click edge was lost or delayed");
        require(ImGui::GetCurrentContext()->InputEventsQueue.empty(), "click left deferred input");
        draw();
    }
    ImGui::DestroyContext();
}
}
int main() {
    try {
        testInput(90, false);
        for (float rate : {45.0f, 90.0f, 120.0f}) testInput(rate, true);
        std::cout << "Panel snapshot input tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        if (ImGui::GetCurrentContext()) ImGui::DestroyContext();
        return 1;
    }
}
