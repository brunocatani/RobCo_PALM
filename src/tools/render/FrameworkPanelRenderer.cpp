#include "../PCH.h"

#include "ConfiguratorRuntime.h"
#include "render/FrameworkPanelRenderer.h"
#include "render/NativeRenderer.h"
#include "render/UiVisualStyle.h"

#include "RPSUIFrameworkApi.h"

#include <imgui_impl_dx11.h>

#include <cfloat>
#include "Fonts.h"

namespace devui::render
{
    namespace
    {
        struct RenderState
        {
            std::mutex mutex;
            Microsoft::WRL::ComPtr<ID3D11Device> device;
            Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
            ImGuiContext* imguiContext{ nullptr };
            std::array<ImFont*,
                static_cast<std::size_t>(FontRole::Count)> fonts{};
            std::uint64_t deviceGeneration{ 0 };
            bool backendReady{ false };
            bool initializationFailed{ false };
            bool backWasDown{ false };
            bool backRequested{ false };
        };

        struct FrameworkState
        {
            std::mutex mutex;
            const rpsui::sdk::ApiV1* api{ nullptr };
            std::uint64_t ownerToken{ 0 };
            std::uint64_t panelHandle{ 0 };
            std::uint64_t presentationSequence{ 1 };
            bool installed{ false };
        };

        [[nodiscard]] RenderState& renderState() noexcept
        {
            static RenderState state;
            return state;
        }

        [[nodiscard]] FrameworkState& frameworkState() noexcept
        {
            static FrameworkState state;
            return state;
        }

        void configureFonts(RenderState& state, ImGuiIO& io)
        {
            for(std::size_t i=0;i<state.fonts.size();++i)
                state.fonts[i]=wheel::addPreparedFont(kFontSizes[i]);
            io.FontDefault=state.fonts[static_cast<std::size_t>(FontRole::Body)];
            ImGui::GetStyle().FontSizeBase=20.0f;
            ImGui::GetStyle().FontScaleMain=1.0f;
        }
        void releaseRenderState(RenderState& state) noexcept
        {
            if (state.imguiContext) {
                ImGui::SetCurrentContext(state.imguiContext);
                if (state.backendReady) {
                    ImGui_ImplDX11_Shutdown();
                }
                ImGui::DestroyContext(state.imguiContext);
            }
            state.imguiContext = nullptr;
            state.fonts = {};
            state.backendReady = false;
            state.context.Reset();
            state.device.Reset();
            state.deviceGeneration = 0;
        }

        [[nodiscard]] bool initializeRenderState(
            RenderState& state,
            const rpsui::sdk::PanelRenderFrameV1& frame) noexcept
        {
            auto* device =
                static_cast<ID3D11Device*>(frame.d3dDevice);
            auto* context =
                static_cast<ID3D11DeviceContext*>(frame.d3dContext);
            if (!device ||
                !context ||
                state.initializationFailed) {
                return false;
            }
            if (state.imguiContext &&
                state.device.Get() == device &&
                state.deviceGeneration == frame.deviceGeneration) {
                return true;
            }
            releaseRenderState(state);

            try {
                state.device = device;
                state.context = context;
                state.deviceGeneration = frame.deviceGeneration;
                IMGUI_CHECKVERSION();
                state.imguiContext = ImGui::CreateContext();
                if (!state.imguiContext) {
                    throw std::runtime_error(
                        "ImGui context creation failed");
                }
                ImGui::SetCurrentContext(state.imguiContext);
                auto& io = ImGui::GetIO();
                io.IniFilename = nullptr;
                io.LogFilename = nullptr;
                io.DisplaySize = {
                    static_cast<float>(frame.pixelWidth),
                    static_cast<float>(frame.pixelHeight),
                };
                io.DisplayFramebufferScale = { 1.0f, 1.0f };
                io.MouseDrawCursor = true;
                io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
                configureFonts(state, io);
                devui::visual::applyStyle();
                if (!ImGui_ImplDX11_Init(device, context)) {
                    throw std::runtime_error(
                        "ImGui DX11 backend initialization failed");
                }
                state.backendReady = true;
                logger::info(
                    "PALM Config initialized its RPS UI consumer renderer "
                    "(device generation {})",
                    frame.deviceGeneration);
                return true;
            } catch (const std::exception& error) {
                logger::critical(
                    "PALM Config consumer renderer initialization failed: {}",
                    error.what());
            } catch (...) {
                logger::critical(
                    "PALM Config consumer renderer initialization failed");
            }
            releaseRenderState(state);
            state.initializationFailed = true;
            return false;
        }

        [[nodiscard]] rock_configurator::panel_resize::Handle localResizeHandle(
            rpsui::sdk::ResizeHandleV1 handle) noexcept
        {
            return static_cast<rock_configurator::panel_resize::Handle>(
                static_cast<std::uint32_t>(handle));
        }

        void RPSUI_CALL renderPanel(
            const rpsui::sdk::PanelRenderFrameV1* frame,
            void*) noexcept
        {
            if (!frame ||
                frame->structSize < sizeof(*frame) ||
                !frame->renderTargetView) {
                return;
            }
            auto& state = renderState();
            std::scoped_lock lock(state.mutex);
            if (!initializeRenderState(state, *frame)) {
                return;
            }

            rock_configurator::onFrameworkPanelFrame(
                frame->physicalWidth,
                localResizeHandle(frame->hoveredResizeHandle),
                localResizeHandle(frame->activeResizeHandle));

            ImGui::SetCurrentContext(state.imguiContext);
            auto& io = ImGui::GetIO();
            io.DeltaTime = std::clamp(
                frame->deltaSeconds,
                1.0f / 240.0f,
                0.1f);
            io.DisplaySize = {
                static_cast<float>(frame->pixelWidth),
                static_cast<float>(frame->pixelHeight),
            };
            if (frame->pointerValid != 0) {
                io.AddMousePosEvent(
                    frame->pointerPixelX,
                    frame->pointerPixelY);
            } else {
                io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
            }
            io.AddMouseButtonEvent(
                0,
                frame->pointerValid != 0 &&
                    frame->primaryDown != 0);
            if (frame->pointerValid != 0) {
                constexpr float wheelUnitsPerSecond = 5.5f;
                io.AddMouseWheelEvent(
                    frame->scrollAxisX *
                        wheelUnitsPerSecond *
                        io.DeltaTime,
                    frame->scrollAxisY *
                        wheelUnitsPerSecond *
                        io.DeltaTime);
            }

            auto* renderTarget =
                static_cast<ID3D11RenderTargetView*>(
                    frame->renderTargetView);
            state.context->OMSetRenderTargets(
                1,
                &renderTarget,
                nullptr);
            D3D11_VIEWPORT viewport{};
            viewport.Width = static_cast<float>(frame->pixelWidth);
            viewport.Height = static_cast<float>(frame->pixelHeight);
            viewport.MinDepth = 0.0f;
            viewport.MaxDepth = 1.0f;
            state.context->RSSetViewports(1, &viewport);

            ImGui_ImplDX11_NewFrame();
            ImGui::NewFrame();
            const bool backDown = frame->pointerValid != 0 && frame->backDown != 0;
            state.backRequested |= backDown && !state.backWasDown;
            state.backWasDown = backDown;
            if (!rock_configurator::drawImGui(0, 0, kPanelPixelWidth, kPanelPixelHeight, state.backRequested)) {
                ImGui::EndFrame();
                return;
            }
            state.backRequested = false;
            ImGui::Render();
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        }
    }

    void PrepareFonts() noexcept
    {
        try {wheel::prepareFonts();}
        catch(...){logger::error("PALM terminal font preload failed; using the ImGui font");}
    }
    ImFont* GetFont(FontRole role) noexcept
    {
        const auto index = static_cast<std::size_t>(role);
        auto& state = renderState();
        return index < state.fonts.size() ?
            state.fonts[index] :
            nullptr;
    }

    bool InstallFrameworkPanel() noexcept
    {
        auto& state = frameworkState();
        std::scoped_lock lock(state.mutex);
        if (state.installed) {
            return true;
        }

        state.api = rpsui::sdk::RequestApiV1();
        if (!state.api ||
            !state.api->registerConsumer ||
            !state.api->registerPanel ||
            !state.api->submitPanelPresentation) {
            logger::critical(
                "RPS UI Framework V1 is unavailable; PALM Config consumer disabled");
            return false;
        }

        rpsui::sdk::ConsumerRegistrationV1 consumer{};
        std::snprintf(
            consumer.consumerId,
            sizeof(consumer.consumerId),
            "robco.palm.workshop");
        std::snprintf(
            consumer.displayName,
            sizeof(consumer.displayName),
            "PALM Config");
        consumer.requestedFeatures = state.api->featureBits;
        rpsui::sdk::ConsumerHandleV1 handle{};
        if (state.api->registerConsumer(
                &consumer,
                &handle) != rpsui::sdk::ResultV1::Ok ||
            handle.ownerToken == 0) {
            logger::critical(
                "PALM Config could not register with RPS UI Framework");
            state.api = nullptr;
            return false;
        }
        state.ownerToken = handle.ownerToken;

        rpsui::sdk::PanelRegistrationV1 panel{};
        std::snprintf(
            panel.panelId,
            sizeof(panel.panelId),
            "robco.palm.workshop.main");
        std::snprintf(
            panel.displayName,
            sizeof(panel.displayName),
            "PALM Config");
        panel.pixelWidth = kPanelPixelWidth;
        panel.pixelHeight = kPanelPixelHeight;
        panel.defaultPhysicalWidth = kDefaultPanelPhysicalWidth;
        panel.minimumPhysicalWidth = kMinimumPanelPhysicalWidth;
        panel.maximumPhysicalWidth = kMaximumPanelPhysicalWidth;
        panel.sortOrder = 310;
        panel.flags = static_cast<std::uint32_t>(rpsui::sdk::PanelFlagV1::ConfigNavigation);
        panel.renderCallback = &renderPanel;
        if (state.api->registerPanel(
                state.ownerToken,
                &panel,
                &state.panelHandle) != rpsui::sdk::ResultV1::Ok ||
            state.panelHandle == 0) {
            if (state.api->unregisterConsumer) {
                (void)state.api->unregisterConsumer(state.ownerToken);
            }
            state.api = nullptr;
            state.ownerToken = 0;
            state.panelHandle = 0;
            state.presentationSequence = 1;
            state.installed = false;
            logger::critical(
                "PALM Config could not register its panel with RPS UI Framework");
            return false;
        }

        state.installed = true;
        logger::info(
            "PALM Config registered RPS UI panel {} as owner {}",
            state.panelHandle,
            state.ownerToken);
        return true;
    }

    bool ResetFrameworkPanelSize() noexcept
    {
        auto& state = frameworkState();
        std::scoped_lock lock(state.mutex);
        return state.installed &&
               state.api &&
               state.api->resetPanelSize &&
               state.api->resetPanelSize(
                   state.ownerToken,
                   state.panelHandle) ==
                   rpsui::sdk::ResultV1::Ok;
    }

    bool SetPanelOpen(bool open, const PanelPose* pose) noexcept
    {
        auto& state = frameworkState();
        std::scoped_lock lock(state.mutex);
        if (!state.installed ||
            !state.api ||
            !state.api->submitPanelPresentation) {
            return false;
        }

        rpsui::sdk::PanelPresentationV1 presentation{};
        presentation.sequence = state.presentationSequence++;
        presentation.open = open ? 1 : 0;
        if (open) {
            if (!pose) {
                return false;
            }
            const auto copy = [](const DirectX::XMFLOAT3& value, float out[3]) {
                out[0] = value.x;
                out[1] = value.y;
                out[2] = value.z;
            };
            copy(pose->center, presentation.pose.center);
            copy(pose->right, presentation.pose.right);
            copy(pose->up, presentation.pose.up);
            copy(pose->front, presentation.pose.front);
            presentation.pose.physicalWidth = pose->physicalWidth;
            presentation.pose.physicalHeight = pose->physicalHeight;
        }

        const auto result = state.api->submitPanelPresentation(
            state.ownerToken,
            state.panelHandle,
            &presentation);
        if (result != rpsui::sdk::ResultV1::Ok) {
            logger::error(
                "PALM Config panel presentation was rejected ({})",
                static_cast<std::uint32_t>(result));
            return false;
        }
        if (open && state.api->getPanelState) {
            rpsui::sdk::PanelStateV1 accepted;
            const auto query = state.api->getPanelState(state.ownerToken, state.panelHandle, &accepted);
            if (query == rpsui::sdk::ResultV1::Ok) {
                logger::info("PALM Config accepted pose: requested=({:.2f},{:.2f},{:.2f}) actual=({:.2f},{:.2f},{:.2f}) shift=({:.2f},{:.2f},{:.2f}) open={}",
                    presentation.pose.center[0], presentation.pose.center[1], presentation.pose.center[2],
                    accepted.pose.center[0], accepted.pose.center[1], accepted.pose.center[2],
                    accepted.pose.center[0]-presentation.pose.center[0],
                    accepted.pose.center[1]-presentation.pose.center[1],
                    accepted.pose.center[2]-presentation.pose.center[2], accepted.open);
            } else {
                logger::warn("PALM Config accepted-pose query failed ({})", static_cast<std::uint32_t>(query));
            }
        }
        return true;
    }

    void Shutdown() noexcept
    {
        auto& framework = frameworkState();
        {
            std::scoped_lock lock(framework.mutex);
            if (framework.api) {
                if (framework.panelHandle != 0 &&
                    framework.api->unregisterPanel) {
                    (void)framework.api->unregisterPanel(
                        framework.ownerToken,
                        framework.panelHandle);
                }
                if (framework.ownerToken != 0 &&
                    framework.api->unregisterConsumer) {
                    (void)framework.api->unregisterConsumer(
                        framework.ownerToken);
                }
            }
            framework.api = nullptr;
            framework.ownerToken = 0;
            framework.panelHandle = 0;
            framework.presentationSequence = 1;
            framework.installed = false;
        }

        auto& state = renderState();
        std::scoped_lock lock(state.mutex);
        releaseRenderState(state);
    }
}
