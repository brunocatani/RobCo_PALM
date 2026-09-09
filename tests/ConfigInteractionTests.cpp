#include "ConfiguratorRuntime.h"
#include "render/UiVisualStyle.h"
#include "render/ConfigUi.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <fstream>
#include <Windows.h>

namespace {
void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
void frame(bool back = false) {
    ImGui::NewFrame();
    (void)rock_configurator::drawImGui(0, 0, 1440, 540, back);
    ImGui::Render();
    rock_configurator::drainPreviewActions();
}
void click(float x, float y) {
    auto& io = ImGui::GetIO();
    io.AddMousePosEvent(x, y); frame();
    io.AddMouseButtonEvent(0, true); frame();
    io.AddMouseButtonEvent(0, false); frame();
    for (int i = 0; i < 5; ++i) frame(); // Let nested child sizing settle after the tab changes.
}
ImGuiWindow* window(const char* part) {
    for (auto* w : ImGui::GetCurrentContext()->Windows)
        if (w->Active && std::strstr(w->Name, part)) return w;
    return nullptr;
}
void scroll(ImGuiWindow* target) {
    require(target != nullptr, "test column is missing");
    if (target->ScrollMax.y <= 0) throw std::runtime_error(std::string(target->Name) + " has no scroll range; content=" + std::to_string(target->ContentSize.y) + " height=" + std::to_string(target->Size.y));
    auto& io = ImGui::GetIO();
    io.AddMousePosEvent(target->InnerRect.Min.x + 24, target->InnerRect.Min.y + 80);
    frame();
    const float before = target->Scroll.y;
    for (int i = 0; i < 30; ++i) { io.AddMouseWheelEvent(0, -0.1f); frame(); }
    require(target->Scroll.y > before, "hovered column did not scroll without a click");
}
}

int main() {
    const auto path = std::filesystem::temp_directory_path() /
        ("WheelConfigInteraction-" + std::to_string(GetCurrentProcessId()) + ".ini");
    struct Cleanup { std::filesystem::path path; ~Cleanup() { std::error_code error; std::filesystem::remove(path, error); } } cleanup{path};
    { std::ofstream file(path); file << "[General]\n";
      for (int n = 0; n < 40; ++n) file << "bTest" << n << "=true\n"; }
    ImGui::CreateContext();
    try {
        auto& io = ImGui::GetIO();
        io.IniFilename = nullptr; io.LogFilename = nullptr;
        io.DisplaySize = {1440, 540}; io.DeltaTime = 1.0f / 90;
        io.FontDefault = io.Fonts->AddFontDefaultVector();
        unsigned char* pixels{}; int width{}, height{};
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        devui::visual::applyStyle();
        rock_configurator::initializePreview(path);
        rock_configurator::setPreviewOpen(true);
        frame(); frame();
        const float railWidth = devui::visual::railWidth(1440);
        click(railWidth + 250, 50); // ROCK settings
        require(window("settings-rows") != nullptr, "settings tab did not open");
        auto* rail = window("/rail_");
        auto* rows = window("settings-rows");
        const float railBefore = rail ? rail->Scroll.y : 0;
        scroll(rows);
        require(!rail || rail->Scroll.y == railBefore, "settings scroll moved the navigation column");
        frame(true); frame();
        require(rock_configurator::isOpen() && window("wheel-category-content"), "back did not return to Config home");
        click(railWidth + 440, 50); // Spawner
        require(window("spawn-items") != nullptr, "spawner tab did not open");
        scroll(window("spawn-items"));
        auto* items = window("spawn-items");
        click(items->InnerRect.Min.x + 80, items->InnerRect.Min.y + 80);
        require(window("spawn-item-head") != nullptr, "item selection did not open actions");
        frame(true); for (int i = 0; i < 5; ++i) frame();
        require(rock_configurator::isOpen() && window("spawn-items"), "back skipped the spawner item list");
        frame(true); frame();
        require(rock_configurator::isOpen() && window("wheel-category-content"), "spawner back closed Config instead of returning home");
        frame(true);
        require(!rock_configurator::isOpen(), "back at Config home did not close it");
        // Every loaded-mod combination, including none, uses the real tab widgets.
        for (unsigned mask = 0; mask < 8; ++mask) {
            rock_configurator::initializeRpsPreview({path, path, path},
                {(mask & 1) != 0, (mask & 2) != 0, (mask & 4) != 0});
            rock_configurator::setPreviewOpen(true);frame();frame();
            click(railWidth + 250, 50);
            auto* firstRows = window("settings-rows");
            require((firstRows != nullptr) == (mask != 0), "unloaded mod exposed settings");
            if (!mask) continue;
            const auto firstId = firstRows->ID;
            const int count = static_cast<int>((mask & 1) != 0) + static_cast<int>((mask & 2) != 0) + static_cast<int>((mask & 4) != 0);
            ImGuiID previous = firstId;
            for (int mod = 1; mod < count; ++mod) {
                click(45.0f + mod * 168.0f, 120);
                auto* currentRows = window("settings-rows");
                require(currentRows && currentRows->ID != previous, "mod tabs did not isolate identical setting identities");
                previous = currentRows->ID;
            }
            click(45, 120);
            require(window("settings-rows")->ID == firstId, "first available mod did not restore its workspace");
            click(45.0f + count * 168.0f, 120);
            require(window("settings-rows")->ID == firstId, "hidden mod tab remained clickable");
        }
        std::cout << "Config headless interaction and contextual scrolling passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; ImGui::DestroyContext(); return 1;
    }
    ImGui::DestroyContext(); return 0;
}
