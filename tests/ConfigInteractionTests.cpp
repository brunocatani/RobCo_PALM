#include "ConfiguratorRuntime.h"
#include "WheelConfig.h"
#include "render/UiVisualStyle.h"
#include "render/ConfigUi.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <fstream>
#include <Windows.h>
#include <ROCK/Configuration.h>

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
std::string renderedText() {
    ImGui::NewFrame();
    ImGui::LogToBuffer();
    (void)rock_configurator::drawImGui(0, 0, 1440, 540);
    std::string result = ImGui::GetCurrentContext()->LogBuffer.c_str();
    ImGui::LogFinish();
    ImGui::Render();
    rock_configurator::drainPreviewActions();
    return result;
}
rock::api::Status fixtureRevision(rock::api::OwnerToken owner,std::uint64_t* out) noexcept { if(owner!=1)return rock::api::Status::OwnerNotRegistered; *out=1; return rock::api::Status::Ok; }
rock::api::Status fixtureVisit(rock::api::OwnerToken owner,rock::api::configuration::Group group, rock::api::configuration::VisitorV1 visitor, void* context) noexcept {
    using namespace rock::api::configuration;
    using rock::api::Status;
    if(owner!=1)return Status::OwnerNotRegistered;
    const SettingV1 consumer{"PhysicsInteraction", "bConsumerFixture", "true", "true", "01. Consumer Controls", "consumer-fixture", ValueType::Boolean, 0};
    const SettingV1 developer{"PhysicsInteraction", "fMovedFixture", "4", "4", "01. Developer Controls", "developer-fixture", ValueType::Float, 0};
    visitor(group == Group::Consumer ? &consumer : &developer, context);
    return Status::Ok;
}
rock::api::Status fixtureWrite(rock::api::OwnerToken,rock::api::configuration::Group, const char*, const char*, const char*, char*, std::uint32_t) noexcept { return rock::api::Status::NotReady; }
}

int main(int argc, char** argv) {
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
        click(railWidth + 250, 50); // PALM's own settings
        require(window("wheel-settings") != nullptr, "PALM settings tab did not open");
        click(railWidth+42,294);
        require(!wheel::snapshotWheelPreferences().enabled[3] && wheel::takeWheelConfigChange(), "weapon visibility edit did not reach wheel preferences");
        click(railWidth+42,294);
        click(railWidth+600,470);
        require(!wheel::snapshotWheelPreferences().gesturesEnabled, "gesture visibility edit did not reach wheel preferences");
        auto wheelModel=wheel::selectedWheelInventory(wheel::Model{});
        require(wheel::navigationLayout(wheelModel).count==6, "hidden gestures left empty navigation slots");
        click(railWidth+600,470);
        const auto originalPreferences=wheel::snapshotWheelPreferences();
        auto full=originalPreferences;full.enabled.fill(false);full.gesturesEnabled=false;
        for(unsigned i=0;i<9;++i)require(full.setSectionEnabled("missing.section"+std::to_string(i),true), "visibility fixture could not reserve section slots");
        std::array<palm::api::SectionHandle,9> handles{};
        for(unsigned i=0;i<handles.size();++i) {
            palm::api::SectionV1 section;std::snprintf(section.id,sizeof(section.id),"missing.section%u",i);
            std::strcpy(section.name,"Fixture");std::strcpy(section.modName,"Fixture");
            section.onSelect=[](std::uint32_t,std::uint64_t,void*) noexcept {};
            require(wheel::sectionRegistry().registerSection(&section,&handles[i])==palm::api::Result::Ok,"section fixture registration failed");
        }
        wheel::restoreWheelPreferences(full);frame();click(railWidth+42,294);click(railWidth+600,470);
        require(!wheel::snapshotWheelPreferences().enabled[3] && !wheel::snapshotWheelPreferences().gesturesEnabled,
            "settings controls exceeded the ten-entry limit");
        full.sections.pop_back();wheel::restoreWheelPreferences(full);frame();click(railWidth+42,294);click(railWidth+600,470);
        require(wheel::snapshotWheelPreferences().enabled[3] && !wheel::snapshotWheelPreferences().gesturesEnabled,
            "one free slot did not admit weapons or admitted both gesture entries");
        for(auto handle:handles)(void)wheel::sectionRegistry().unregisterSection(handle);
        wheel::restoreWheelPreferences(originalPreferences);frame();
        frame(true); frame();
        require(window("wheel-category-content") != nullptr, "PALM settings back did not return to items");
        click(railWidth + 440, 50); // ROCK settings
        require(window("settings-rows") != nullptr, "settings tab did not open");
        auto* rail = window("/rail_");
        auto* rows = window("settings-rows");
        const float railBefore = rail ? rail->Scroll.y : 0;
        scroll(rows);
        require(!rail || rail->Scroll.y == railBefore, "settings scroll moved the navigation column");
        frame(true); frame();
        require(rock_configurator::isOpen() && window("wheel-category-content"), "back did not return to Config home");
        click(railWidth + 634, 50); // Spawner
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
        for (const bool rockV2 : {false,true}) for (const bool paperV2 : {false,true}) for (unsigned mask = 0; mask < 8; ++mask) {
            rock_configurator::initializeRpsPreview({path, path, path},
                {(mask & 1) != 0, (mask & 2) != 0, (mask & 4) != 0},nullptr,{},{},rockV2,paperV2);
            rock_configurator::setPreviewOpen(true);frame();frame();
            click(railWidth + 440, 50);
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
        const rock::api::configuration::ApiV1 configApi{fixtureRevision, fixtureVisit, fixtureWrite};
        for (const bool rockV2 : {false,true}) {
        rock_configurator::initializeRpsPreview({path, {}, {}}, {true, false, false}, &configApi,{},{},rockV2,false);
        rock_configurator::setPreviewOpen(true); frame(); frame();
        click(railWidth + 440, 50);
        const auto consumerText = renderedText();
        require(consumerText.find("consumer-fixture") != std::string::npos && consumerText.find("developer-fixture") == std::string::npos,
            "ROCK page mixed consumer and developer controls");
        require(consumerText.find("01. Consumer Controls") != std::string::npos, "consumer section heading lost its catalog label");
        const auto consumerWorkspace = window("settings-rows")->ID;
        click(220, 120);
        const auto developerText = renderedText();
        require(developerText.find("developer-fixture") != std::string::npos && developerText.find("consumer-fixture") == std::string::npos,
            "Developer tab is missing or does not expose moved non-debug options");
        require(developerText.find("01. Developer Controls") != std::string::npos, "developer section heading lost its catalog label");
        require(window("settings-rows")->ID != consumerWorkspace, "Developer page reused the consumer workspace");
        click(45, 120);
        require(window("settings-rows")->ID == consumerWorkspace, "ROCK tab did not restore the consumer workspace");
        }
        if (argc > 1) {
            // Exercise the shipped CSAH catalog in the real RPS tab, both alone
            // and alongside every existing page. All edits remain in memory.
            for (bool otherMods : {false, true}) {
                rock_configurator::initializeRpsPreview({path,path,path}, {otherMods,otherMods,otherMods},
                    otherMods ? &configApi : nullptr, path, argv[1]);
                rock_configurator::setPreviewOpen(true);frame();frame();click(railWidth + 440, 50);
                if (otherMods) click(45 + 4 * 168, 120);
                require(window("settings-rows"), "CSAH page did not open in RPS mods");
                require(renderedText().find("CSAH Visual Suite") != std::string::npos, "CSAH page lost its authored controls");
                scroll(window("settings-rows"));
                scroll(window("/rail_"));
            }
        }
        std::cout << "Config interaction, scrolling, and separate ROCK/Developer pages passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; ImGui::DestroyContext(); return 1;
    }
    ImGui::DestroyContext(); return 0;
}
