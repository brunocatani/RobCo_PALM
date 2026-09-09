#include "PCH.h"

#include "ConfiguratorRuntime.h"

#include "IniSettingsStore.h"
#include "PanelResizePolicy.h"
#include "SpawnBrowser.h"
#include "WheelConfig.h"
#include "render/FrameworkPanelRenderer.h"
#include "render/NativeRenderer.h"


namespace rock_configurator
{
    namespace
    {

        constexpr std::size_t kRuntimeActionQueueCapacity = 32;
        constexpr std::uint64_t kSettingTargetIdentityMask =
            (std::uint64_t{ 1 } << 60) - 1;
        constexpr std::uint64_t kInvalidRightHorizontalTarget =
            (std::numeric_limits<std::uint64_t>::max)();
        constexpr std::uint64_t kSemanticFnvOffset = 14695981039346656037ull;
        constexpr std::uint64_t kSemanticFnvPrime = 1099511628211ull;
        constexpr panel_resize::Constraints kPanelResizeConstraints{
            .aspectRatio = devui::render::kPanelAspectRatio,
            .minimumWidth = devui::render::kMinimumPanelPhysicalWidth,
            .maximumWidth = devui::render::kMaximumPanelPhysicalWidth,
        };

        enum class ConfiguratorTab : std::uint8_t
        {
            Wheel,
            Settings,
            Spawn,
        };

        enum class RuntimeActionKind : std::uint8_t
        {
            NoOp,
            OpenPanel,
            ClosePanel,
            UiClose,
            UiSelectTab,
            UiSelectRow,
            UiAdjustRow,
            UiSetBooleanRow,
            UiSetNumericRow,
            UiSetOptionRow,
            UiReload,
            UiResetPanelSize,
            UiSpawnSelectPlugin,
            UiSpawnSelectCategory,
            UiSpawnOpenItem,
            UiSpawnActivateAction,
            UiSpawnBack,
        };

        struct PanelPose
        {
            DirectX::XMFLOAT3 position{};
            DirectX::XMFLOAT3 right{};
            DirectX::XMFLOAT3 up{};
            DirectX::XMFLOAT3 front{};
            float physicalWidth{ devui::render::kDefaultPanelPhysicalWidth };
            float physicalHeight{ devui::render::kDefaultPanelPhysicalHeight };
        };

        struct RuntimeAction
        {
            RuntimeActionKind kind{ RuntimeActionKind::NoOp };
            ConfiguratorTab tab{ ConfiguratorTab::Settings };
            int value{ 0 };
            std::size_t index{ 0 };
            std::uint32_t formId{ 0 };
            std::uint64_t horizontalTarget{ kInvalidRightHorizontalTarget };
            double numericValue{ 0.0 };
            bool hasPanelPose{ false };
            PanelPose panelPose{};
            std::uint64_t generation{ 0 };
        };

        struct SelectionAnchor
        {
            std::optional<std::string> id;
            std::size_t fallbackIndex{ 0 };
        };

        struct PendingNumericEdit
        {
            std::string id;
            std::size_t index{ 0 };
            std::uint64_t target{ kInvalidRightHorizontalTarget };
            double value{ 0.0 };
        };

        IniSettingsStore s_store;
        SpawnBrowser s_spawnBrowser;
        std::mutex s_runtimeMutex;
        bool s_storeLoaded = false;
        std::uint64_t s_storeIdentity = 0;
        SelectionAnchor s_storeAnchor;
        std::size_t s_activeIndex = 0;
        std::string s_statusMessage = "Waiting for ROCK.ini";
        std::optional<PendingNumericEdit> s_pendingNumericEdit;

        std::atomic<ConfiguratorTab> s_activeTab{ ConfiguratorTab::Wheel };
        std::atomic_bool s_panelOpen = false;
        std::atomic<float> s_sessionPanelPhysicalWidth{
            devui::render::kDefaultPanelPhysicalWidth
        };
        std::atomic_bool s_providerInputReady = false;
        std::atomic_bool s_inventoryRefreshRequested = false;

        std::atomic<panel_resize::Handle> s_hoveredResizeHandle{
            panel_resize::Handle::None
        };
        std::atomic<panel_resize::Handle> s_activeResizeHandle{
            panel_resize::Handle::None
        };

        std::mutex s_actionMutex;
        std::array<RuntimeAction, kRuntimeActionQueueCapacity> s_actionQueue{};
        std::size_t s_actionReadIndex = 0;
        std::size_t s_actionWriteIndex = 0;
        std::size_t s_actionCount = 0;
        std::atomic_bool s_actionDrainScheduled = false;
        std::atomic_uint64_t s_actionGeneration{ 1 };
        std::atomic_bool s_actionGenerationExhausted = false;
        std::atomic_uint64_t s_actionQueueFullDrops{ 0 };

        void drainRuntimeActions();

        void hashByte(std::uint64_t& hash, std::uint8_t value) noexcept
        {
            hash ^= value;
            hash *= kSemanticFnvPrime;
        }

        void hashUint64(std::uint64_t& hash, std::uint64_t value) noexcept
        {
            for (unsigned shift = 0; shift < 64; shift += 8) {
                hashByte(hash, static_cast<std::uint8_t>(value >> shift));
            }
        }

        void hashString(std::uint64_t& hash, std::string_view value) noexcept
        {
            hashUint64(hash, value.size());
            for (const char character : value) {
                hashByte(hash, static_cast<std::uint8_t>(character));
            }
        }

        template <class Store>
        [[nodiscard]] std::uint64_t settingsIdentity(const Store& store, bool loaded)
        {
            std::uint64_t identity = kSemanticFnvOffset;
            hashByte(identity, loaded ? 1 : 0);
            if (!loaded) {
                return identity;
            }
            const auto& settings = store.settings();
            hashUint64(identity, settings.size());
            for (std::size_t index = 0; index < settings.size(); ++index) {
                hashString(identity, settings[index].id);
                hashByte(identity, static_cast<std::uint8_t>(settings[index].type));
                hashByte(identity, static_cast<std::uint8_t>(settings[index].control.kind));
                hashByte(identity, settings[index].control.bounded ? 1 : 0);
                hashByte(identity, store.isSlider(index) ? 1 : 0);
            }
            return identity;
        }

        [[nodiscard]] std::uint64_t selectedIdentity(
            std::uint64_t identity,
            std::size_t index) noexcept
        {
            hashUint64(identity, index);
            return identity & kSettingTargetIdentityMask;
        }

        [[nodiscard]] std::uint64_t settingTargetLocked(std::size_t index) noexcept
        {
            return selectedIdentity(s_storeIdentity, index);
        }

        template <class Store>
        [[nodiscard]] bool readStoreLocked(
            Store& store,
            bool& loaded,
            std::size_t& activeIndex,
            SelectionAnchor& anchor,
            std::uint64_t& identity,
            bool reload)
        {
            const auto& previous = store.settings();
            if (activeIndex < previous.size()) {
                anchor.id = previous[activeIndex].id;
                anchor.fallbackIndex = activeIndex;
            }
            loaded = reload ? store.reload() : store.load();
            const auto& settings = store.settings();
            if (!loaded || settings.empty()) {
                activeIndex = 0;
            } else {
                const auto anchored = anchor.id ? store.indexForId(*anchor.id) : std::nullopt;
                activeIndex = anchored ? *anchored :
                    (std::min)(anchor.fallbackIndex, settings.size() - 1);
                anchor.id = settings[activeIndex].id;
                anchor.fallbackIndex = activeIndex;
            }
            identity = settingsIdentity(store, loaded);
            return loaded;
        }

        [[nodiscard]] bool readRockStoreLocked(bool reload)
        {
            s_pendingNumericEdit.reset();
            return readStoreLocked(
                s_store, s_storeLoaded, s_activeIndex, s_storeAnchor, s_storeIdentity, reload);
        }

        [[nodiscard]] bool spawnTabActive() noexcept
        {
            return s_activeTab.load(std::memory_order_acquire) == ConfiguratorTab::Spawn;
        }

        void loadRockSettingsLocked(bool reloadLoaded)
        {
            (void)readRockStoreLocked(reloadLoaded && s_storeLoaded);
            s_statusMessage = s_storeLoaded ?
                std::format("Loaded {} ROCK settings", s_store.settings().size()) :
                s_store.lastError();
        }

        void invalidateQueuedRuntimeActions()
        {
            std::scoped_lock lock(s_actionMutex);
            const auto generation = s_actionGeneration.load(std::memory_order_relaxed);
            if (generation == (std::numeric_limits<std::uint64_t>::max)()) {
                if (!s_actionGenerationExhausted.exchange(true, std::memory_order_relaxed)) {
                    logger::critical("Wheel Config runtime action generation exhausted; input disabled");
                }
            } else {
                s_actionGeneration.store(generation + 1, std::memory_order_release);
            }
            s_actionReadIndex = 0;
            s_actionWriteIndex = 0;
            s_actionCount = 0;
        }

        bool publishPanelToRenderer(const PanelPose& pose) noexcept
        {
            const devui::render::PanelPose nativePose{
                .center = { pose.position.x, pose.position.y, pose.position.z },
                .right = { pose.right.x, pose.right.y, pose.right.z },
                .up = { pose.up.x, pose.up.y, pose.up.z },
                .front = { pose.front.x, pose.front.y, pose.front.z },
                .physicalWidth = pose.physicalWidth,
                .physicalHeight = pose.physicalHeight,
            };
            return devui::render::SetPanelOpen(true, &nativePose);
        }

        void openPanelLocked(const RuntimeAction& action)
        {
            if (s_panelOpen.load(std::memory_order_acquire)) {
                return;
            }
            if (!s_providerInputReady.load(std::memory_order_acquire)) {
                s_statusMessage = "ROCK provider is not ready for configurator input";
                return;
            }
            if (!action.hasPanelPose) {
                s_statusMessage = "Open the workshop from the wheel";
                return;
            }

            loadRockSettingsLocked(true);
            s_pendingNumericEdit.reset();
            if (spawnTabActive() && !s_spawnBrowser.ensureIndexBuilt()) {
                s_statusMessage = s_spawnBrowser.lastResult();
            }

            s_hoveredResizeHandle.store(panel_resize::Handle::None, std::memory_order_release);
            s_activeResizeHandle.store(panel_resize::Handle::None, std::memory_order_release);
            if (!publishPanelToRenderer(action.panelPose)) {
                s_statusMessage = "The UI framework rejected the Config panel";
                return;
            }
            s_panelOpen.store(true, std::memory_order_release);
            logger::info(
                "Wheel Config fixed ROCK configurator panel opened at {:.2f},{:.2f},{:.2f}",
                action.panelPose.position.x,
                action.panelPose.position.y,
                action.panelPose.position.z);
        }

        void closePanelLocked(const char* reason)
        {
            if (!s_panelOpen.exchange(false, std::memory_order_acq_rel)) {
                return;
            }
            devui::render::SetPanelOpen(false);
            s_hoveredResizeHandle.store(panel_resize::Handle::None, std::memory_order_release);
            s_activeResizeHandle.store(panel_resize::Handle::None, std::memory_order_release);
            invalidateQueuedRuntimeActions();
            s_pendingNumericEdit.reset();
            s_statusMessage = reason ? reason : "Panel closed";
            logger::info("Wheel Config panel closed ({})", s_statusMessage);
        }

        void applyRockChangeLocked(const SettingChangeResult& result)
        {
            s_statusMessage = result.message.empty() ? "No change" : result.message;
            if (result.changed && result.saved) {
#ifndef WHEEL_DESKTOP_PREVIEW
                (void)readRockStoreLocked(true);
                s_statusMessage = s_storeLoaded ?
                    std::format("Saved {}", result.setting.key) : s_store.lastError();
#else
                s_statusMessage = std::format("PREVIEW: {} changed in memory", result.setting.key);
#endif
            }
        }

        void setActiveTabLocked(ConfiguratorTab tab)
        {
            if (s_activeTab.exchange(tab, std::memory_order_acq_rel) == tab) {
                return;
            }
            if (tab == ConfiguratorTab::Spawn && !s_spawnBrowser.ensureIndexBuilt()) {
                s_statusMessage = s_spawnBrowser.lastResult();
            }
        }

        void selectRowLocked(ConfiguratorTab tab, std::size_t index)
        {
            if (tab != s_activeTab.load(std::memory_order_acquire)) {
                return;
            }
            switch (tab) {
            case ConfiguratorTab::Settings:
                if (index < s_store.settings().size()) {
                    s_activeIndex = index;
                }
                break;
            case ConfiguratorTab::Wheel:
            case ConfiguratorTab::Spawn:
                break;
            }
        }

        [[nodiscard]] bool validatesSettingTargetLocked(const RuntimeAction& action) noexcept
        {
            return action.tab == ConfiguratorTab::Settings &&
                   s_activeTab.load(std::memory_order_acquire) == ConfiguratorTab::Settings &&
                   s_storeLoaded && action.index < s_store.settings().size() &&
                   action.horizontalTarget == settingTargetLocked(action.index);
        }

        void setBooleanRowLocked(const RuntimeAction& action)
        {
            if (!validatesSettingTargetLocked(action)) {
                return;
            }
            s_activeIndex = action.index;
            applyRockChangeLocked(s_store.setBooleanByIndex(action.index, action.value != 0));
        }

        void setNumericRowLocked(const RuntimeAction& action)
        {
            if (!validatesSettingTargetLocked(action)) {
                return;
            }
            s_activeIndex = action.index;
            applyRockChangeLocked(s_store.setNumericByIndex(action.index, action.numericValue));
        }

        void setOptionRowLocked(const RuntimeAction& action)
        {
            if (!validatesSettingTargetLocked(action) || action.value < 0) {
                return;
            }
            s_activeIndex = action.index;
            applyRockChangeLocked(s_store.setOptionByIndex(
                action.index, static_cast<std::size_t>(action.value)));
        }

        void reloadActiveLocked(ConfiguratorTab tab)
        {
            if (tab != s_activeTab.load(std::memory_order_acquire)) {
                return;
            }
            switch (tab) {
            case ConfiguratorTab::Settings:
                (void)readRockStoreLocked(true);
                s_statusMessage = s_storeLoaded ? "Reloaded ROCK.ini" : s_store.lastError();
                break;
            case ConfiguratorTab::Wheel:
            case ConfiguratorTab::Spawn:
                break;
            }
        }

        void resetPanelSizeLocked()
        {
            s_hoveredResizeHandle.store(panel_resize::Handle::None, std::memory_order_release);
            s_activeResizeHandle.store(panel_resize::Handle::None, std::memory_order_release);
            if (devui::render::ResetFrameworkPanelSize()) {
                s_sessionPanelPhysicalWidth.store(
                    devui::render::kDefaultPanelPhysicalWidth,
                    std::memory_order_release);
                s_statusMessage = "Panel size reset to 100%";
            } else {
                s_statusMessage = "RPS UI Framework rejected the panel size reset";
            }
        }

        void applyRuntimeActionLocked(const RuntimeAction& action)
        {
            if (s_actionGenerationExhausted.load(std::memory_order_acquire) ||
                action.generation != s_actionGeneration.load(std::memory_order_acquire)) {
                return;
            }
            const bool lifecycle = action.kind == RuntimeActionKind::NoOp ||
                                   action.kind == RuntimeActionKind::OpenPanel ||
                                   action.kind == RuntimeActionKind::ClosePanel;
            if (!lifecycle && !s_panelOpen.load(std::memory_order_acquire)) {
                return;
            }

            switch (action.kind) {
            case RuntimeActionKind::NoOp:
                break;
            case RuntimeActionKind::OpenPanel:
                openPanelLocked(action);
                break;
            case RuntimeActionKind::ClosePanel:
                closePanelLocked("Closed because ROCK provider input is unavailable");
                break;
            case RuntimeActionKind::UiClose:
                closePanelLocked("Closed by panel button");
                break;
            case RuntimeActionKind::UiSelectTab:
                setActiveTabLocked(action.tab);
                break;
            case RuntimeActionKind::UiSelectRow:
                selectRowLocked(action.tab, action.index);
                break;
            case RuntimeActionKind::UiAdjustRow:
                if (validatesSettingTargetLocked(action)) {
                    s_activeIndex = action.index;
                    applyRockChangeLocked(s_store.adjustByIndex(action.index, action.value));
                }
                break;
            case RuntimeActionKind::UiSetBooleanRow:
                setBooleanRowLocked(action);
                break;
            case RuntimeActionKind::UiSetNumericRow:
                setNumericRowLocked(action);
                break;
            case RuntimeActionKind::UiSetOptionRow:
                setOptionRowLocked(action);
                break;
            case RuntimeActionKind::UiReload:
                reloadActiveLocked(action.tab);
                break;
            case RuntimeActionKind::UiResetPanelSize:
                resetPanelSizeLocked();
                break;
            case RuntimeActionKind::UiSpawnSelectPlugin:
                if (spawnTabActive()) {
                    (void)s_spawnBrowser.selectPlugin(action.index);
                }
                break;
            case RuntimeActionKind::UiSpawnSelectCategory:
                if (spawnTabActive()) {
                    (void)s_spawnBrowser.selectCategory(action.index);
                }
                break;
            case RuntimeActionKind::UiSpawnOpenItem:
                if (spawnTabActive()) {
                    (void)s_spawnBrowser.openItem(action.formId);
                }
                break;
            case RuntimeActionKind::UiSpawnActivateAction:
                if (spawnTabActive()) {
                    (void)s_spawnBrowser.activateMenuAction(action.index, action.formId);
                    s_inventoryRefreshRequested.store(true, std::memory_order_release);
                }
                break;
            case RuntimeActionKind::UiSpawnBack:
                if (spawnTabActive()) {
                    if (action.value != 0) {
                        s_spawnBrowser.returnToBrowse();
                    } else {
                        (void)s_spawnBrowser.back();
                    }
                }
                break;
            }
        }

        [[nodiscard]] bool popRuntimeAction(RuntimeAction& outAction)
        {
            std::scoped_lock lock(s_actionMutex);
            if (s_actionCount == 0) {
                return false;
            }
            outAction = s_actionQueue[s_actionReadIndex];
            s_actionReadIndex = (s_actionReadIndex + 1) % s_actionQueue.size();
            --s_actionCount;
            return true;
        }

        void scheduleRuntimeActionDrain()
        {
#ifdef WHEEL_DESKTOP_PREVIEW
            return; // Preview explicitly drains after drawing, never through F4SE.
#else
            bool expected = false;
            if (!s_actionDrainScheduled.compare_exchange_strong(
                    expected, true, std::memory_order_acq_rel)) {
                return;
            }
            const auto* tasks = F4SE::GetTaskInterface();
            if (!tasks) {
                s_actionDrainScheduled.store(false, std::memory_order_release);
                return;
            }
            tasks->AddTask([]() { drainRuntimeActions(); });
#endif
        }

        [[nodiscard]] bool enqueueRuntimeAction(
            RuntimeAction action,
            std::uint64_t producerGeneration)
        {
            if (s_actionGenerationExhausted.load(std::memory_order_acquire)) {
                return false;
            }
            {
                std::scoped_lock lock(s_actionMutex);
                if (producerGeneration != s_actionGeneration.load(std::memory_order_acquire) ||
                    s_actionCount >= s_actionQueue.size()) {
                    if (s_actionCount >= s_actionQueue.size()) {
                        s_actionQueueFullDrops.fetch_add(1, std::memory_order_relaxed);
                    }
                    return false;
                }
                action.generation = producerGeneration;
                s_actionQueue[s_actionWriteIndex] = action;
                s_actionWriteIndex = (s_actionWriteIndex + 1) % s_actionQueue.size();
                ++s_actionCount;
            }
            scheduleRuntimeActionDrain();
            return true;
        }

        void drainRuntimeActions()
        {
            std::array<RuntimeAction, kRuntimeActionQueueCapacity> batch{};
            std::size_t batchCount = 0;
            RuntimeAction action{};
            while (batchCount < batch.size() && popRuntimeAction(action)) {
                batch[batchCount++] = action;
            }
            if (batchCount > 0) {
                std::scoped_lock lock(s_runtimeMutex);
                for (std::size_t index = 0; index < batchCount; ++index) {
                    applyRuntimeActionLocked(batch[index]);
                }
            }
            s_actionDrainScheduled.store(false, std::memory_order_release);
            {
                std::scoped_lock lock(s_actionMutex);
                if (s_actionCount == 0) {
                    return;
                }
            }
            scheduleRuntimeActionDrain();
        }

        [[nodiscard]] bool queueUiAction(RuntimeAction action)
        {
            return enqueueRuntimeAction(
                action, s_actionGeneration.load(std::memory_order_acquire));
        }

        [[nodiscard]] ImVec4 accentColor(float alpha = 1.0f) noexcept
        {
            return { 0.408f, 0.886f, 0.757f, alpha };
        }

        [[nodiscard]] ImVec4 textColor(float alpha = 1.0f) noexcept
        {
            return { 0.914f, 0.949f, 0.937f, alpha };
        }

        [[nodiscard]] ImVec4 mutedColor(float alpha = 1.0f) noexcept
        {
            return { 0.573f, 0.651f, 0.643f, alpha };
        }

        [[nodiscard]] ImVec4 surfaceColor(float alpha = 1.0f) noexcept
        {
            return { 0.051f, 0.098f, 0.114f, alpha };
        }

        [[nodiscard]] ImU32 packed(const ImVec4& color) noexcept
        {
            return ImGui::ColorConvertFloat4ToU32(color);
        }

        class ScopedFont final
        {
        public:
            explicit ScopedFont(devui::render::FontRole role, float size = 0.0f) noexcept
            {
                if (auto* font = devui::render::GetFont(role)) {
                    ImGui::PushFont(font, size > 0.0f ? size : font->LegacySize);
                    pushed_ = true;
                }
            }

            ~ScopedFont() noexcept
            {
                if (pushed_) {
                    ImGui::PopFont();
                }
            }

            ScopedFont(const ScopedFont&) = delete;
            ScopedFont& operator=(const ScopedFont&) = delete;

        private:
            bool pushed_{ false };
        };

        [[nodiscard]] ImFont* fontFor(devui::render::FontRole role) noexcept
        {
            if (auto* font = devui::render::GetFont(role)) {
                return font;
            }
            return ImGui::GetFont();
        }

        [[nodiscard]] bool tabChip(
            const char* ordinal,
            const char* label,
            bool active,
            ImVec2 size)
        {
            ImGui::PushID(label);
            const ImVec2 minimum = ImGui::GetCursorScreenPos();
            const bool clicked = ImGui::InvisibleButton("tab", size);
            const bool hovered = ImGui::IsItemHovered();
            const ImVec2 maximum{ minimum.x + size.x, minimum.y + size.y };
            auto* draw = ImGui::GetWindowDrawList();
            const ImU32 background = packed(
                active ? accentColor(0.18f) :
                hovered ? ImVec4(0.12f, 0.178f, 0.183f, 1.0f) :
                          ImVec4(0.065f, 0.098f, 0.103f, 1.0f));
            const ImU32 border = packed(
                active ? accentColor(0.78f) :
                hovered ? ImVec4(0.42f, 0.46f, 0.52f, 0.55f) :
                          ImVec4(0.31f, 0.35f, 0.41f, 0.28f));
            draw->AddRectFilled(minimum, maximum, background, 9.0f);
            draw->AddRect(minimum, maximum, border, 9.0f, 0, active ? 2.0f : 1.0f);
            if (active) {
                draw->AddRectFilled(
                    { minimum.x + 12.0f, maximum.y - 3.0f },
                    { maximum.x - 12.0f, maximum.y },
                    packed(accentColor()),
                    2.0f);
            }
            draw->AddText(
                fontFor(devui::render::FontRole::Mono),
                14.0f,
                { minimum.x + 13.0f, minimum.y + 7.0f },
                packed(active ? accentColor() : mutedColor()),
                ordinal);
            draw->AddText(
                fontFor(devui::render::FontRole::Medium),
                18.0f,
                { minimum.x + 13.0f, minimum.y + 25.0f },
                packed(active ? textColor() : textColor(hovered ? 0.95f : 0.72f)),
                label);
            ImGui::PopID();
            return clicked;
        }

        [[nodiscard]] bool navigationRow(
            const char* label,
            std::size_t count,
            bool active,
            float height = 40.0f)
        {
            ImGui::PushID(label);
            const ImVec2 minimum = ImGui::GetCursorScreenPos();
            const ImVec2 size{ ImGui::GetContentRegionAvail().x, height };
            const bool clicked = ImGui::InvisibleButton("nav-row", size);
            const bool hovered = ImGui::IsItemHovered();
            const ImVec2 maximum{ minimum.x + size.x, minimum.y + size.y };
            auto* draw = ImGui::GetWindowDrawList();
            if (active || hovered) {
                draw->AddRectFilled(
                    minimum,
                    maximum,
                    packed(active ? accentColor(0.13f) : ImVec4(0.12f, 0.178f, 0.183f, 0.72f)),
                    7.0f);
            }
            if (active) {
                draw->AddRectFilled(
                    minimum,
                    { minimum.x + 4.0f, maximum.y },
                    packed(accentColor()),
                    3.0f);
            }
            draw->AddText(
                fontFor(devui::render::FontRole::Medium),
                18.0f,
                { minimum.x + 14.0f, minimum.y + 12.0f },
                packed(active ? textColor() : textColor(hovered ? 0.9f : 0.67f)),
                label);
            std::array<char, 24> countText{};
            std::snprintf(countText.data(), countText.size(), "%zu", count);
            const auto countSize = fontFor(devui::render::FontRole::Mono)->CalcTextSizeA(
                15.0f, FLT_MAX, 0.0f, countText.data());
            draw->AddText(
                fontFor(devui::render::FontRole::Mono),
                15.0f,
                { maximum.x - countSize.x - 13.0f, minimum.y + 14.0f },
                packed(active ? accentColor() : mutedColor(0.8f)),
                countText.data());
            ImGui::PopID();
            return clicked;
        }

        [[nodiscard]] const char* tabSubtitle(ConfiguratorTab tab) noexcept
        {
            switch (tab) {
            case ConfiguratorTab::Settings:
                return "ROCK runtime configuration";
            case ConfiguratorTab::Spawn:
                return "Load-order item browser";
            }
            return "Choose wheel categories and items";
        }

        [[nodiscard]] const char* tabName(ConfiguratorTab tab) noexcept
        {
            switch (tab) {
            case ConfiguratorTab::Settings:
                return "ROCK settings";
            case ConfiguratorTab::Spawn:
                return "Spawn";
            }
            return "Wheel items";
        }

        void queueTab(ConfiguratorTab tab)
        {
            (void)queueUiAction({
                .kind = RuntimeActionKind::UiSelectTab,
                .tab = tab,
            });
        }

        void drawTopBar()
        {
            constexpr std::array tabs{
                ConfiguratorTab::Wheel,
                ConfiguratorTab::Settings,
                ConfiguratorTab::Spawn,
            };
            constexpr std::array ordinals{ "01", "02", "03" };
            constexpr std::array labels{ "Wheel items", "ROCK settings", "Spawner" };
            const auto active = s_activeTab.load(std::memory_order_acquire);

            const ImVec2 windowPosition = ImGui::GetWindowPos();
            const float windowWidth = ImGui::GetWindowWidth();
            auto* draw = ImGui::GetWindowDrawList();
            draw->AddRectFilledMultiColor(
                { windowPosition.x, windowPosition.y },
                { windowPosition.x + windowWidth, windowPosition.y + 92.0f },
                packed(ImVec4(0.055f, 0.086f, 0.091f, 1.0f)),
                packed(ImVec4(0.043f, 0.071f, 0.076f, 1.0f)),
                packed(ImVec4(0.035f, 0.063f, 0.068f, 1.0f)),
                packed(ImVec4(0.043f, 0.071f, 0.076f, 1.0f)));
            draw->AddRectFilled(
                { windowPosition.x, windowPosition.y },
                { windowPosition.x + 5.0f, windowPosition.y + 92.0f },
                packed(accentColor()));
            draw->AddLine(
                { windowPosition.x + 20.0f, windowPosition.y + 92.0f },
                { windowPosition.x + windowWidth - 20.0f, windowPosition.y + 92.0f },
                packed(ImVec4(0.36f, 0.40f, 0.46f, 0.28f)));

            ImGui::SetCursorPos({ 28.0f, 13.0f });
            {
                ScopedFont font(devui::render::FontRole::Medium, 14.0f);
                ImGui::TextColored(accentColor(), "R O C K   /   F I E L D   K I T");
            }
            ImGui::SetCursorPos({ 27.0f, 37.0f });
            {
                ScopedFont font(devui::render::FontRole::Display, 29.0f);
                ImGui::TextUnformatted("CONFIG");
            }

            for (std::size_t index = 0; index < tabs.size(); ++index) {
                ImGui::SetCursorPos({ 240.0f + static_cast<float>(index) * 156.0f, 24.0f });
                if (tabChip(ordinals[index], labels[index], tabs[index] == active, { 146.0f, 54.0f })) {
                    queueTab(tabs[index]);
                }
            }

            const float sizePercent =
                s_sessionPanelPhysicalWidth.load(std::memory_order_acquire) /
                devui::render::kDefaultPanelPhysicalWidth * 100.0f;
            std::array<char, 48> sizeLabel{};
            std::snprintf(sizeLabel.data(), sizeLabel.size(), "SIZE  %.0f%%", sizePercent);
            ImGui::SetCursorPos({ windowWidth - 226.0f, 24.0f });
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.07f, 0.108f, 0.113f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, accentColor(0.18f));
            ImGui::PushStyleColor(ImGuiCol_Border, accentColor(0.45f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
            {
                ScopedFont font(devui::render::FontRole::Mono, 16.0f);
                if (ImGui::Button(sizeLabel.data(), { 142.0f, 54.0f })) {
                    (void)queueUiAction({ .kind = RuntimeActionKind::UiResetPanelSize });
                }
            }
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(3);

            ImGui::SetCursorPos({ windowWidth - 74.0f, 24.0f });
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.075f, 0.075f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.75f, 0.18f, 0.16f, 0.72f));
            {
                ScopedFont font(devui::render::FontRole::Heading, 23.0f);
                if (ImGui::Button("X##close", { 48.0f, 54.0f })) {
                    (void)queueUiAction({ .kind = RuntimeActionKind::UiClose });
                }
            }
            ImGui::PopStyleColor(2);
        }

        template <class Store>
        void drawSettingsRail(
            const Store& store,
            bool loaded,
            std::size_t activeIndex,
            ConfiguratorTab tab,
            const std::string& status)
        {
            {
                ScopedFont font(devui::render::FontRole::Medium, 13.0f);
                ImGui::TextColored(accentColor(), "NAVIGATION");
            }
            {
                ScopedFont font(devui::render::FontRole::Heading, 26.0f);
                ImGui::TextUnformatted(tabName(tab));
            }
            {
                ScopedFont font(devui::render::FontRole::Body, 15.0f);
                ImGui::TextColored(mutedColor(), "%s", tabSubtitle(tab));
            }
            ImGui::Dummy({ 0.0f, 5.0f });
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.075f, 0.114f, 0.119f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.35f, 0.39f, 0.45f, 0.38f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
            if (ImGui::Button("Reload from disk", ImVec2(-1.0f, 44.0f))) {
                (void)queueUiAction({ .kind = RuntimeActionKind::UiReload, .tab = tab });
            }
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(2);
            ImGui::Dummy({ 0.0f, 8.0f });
            ImGui::SeparatorText("GROUPS");

            const auto& settings = store.settings();
            if (!loaded || settings.empty()) {
                ScopedFont font(devui::render::FontRole::Body, 17.0f);
                ImGui::TextColored(mutedColor(), "%s", status.c_str());
                return;
            }
            const std::string_view activeGroup =
                activeIndex < settings.size() ? settings[activeIndex].category : std::string_view{};
            std::string_view previous;
            for (std::size_t index = 0; index < settings.size(); ++index) {
                const std::string_view group = settings[index].category;
                if (index != 0 && group == previous) {
                    continue;
                }
                previous = group;
                std::size_t groupCount = 0;
                for (std::size_t cursor = index;
                     cursor < settings.size() && std::string_view(settings[cursor].category) == group;
                     ++cursor) {
                    ++groupCount;
                }
                ImGui::PushID(static_cast<int>(index));
                if (navigationRow(
                        group.empty() ? "General" : settings[index].category.c_str(),
                        groupCount,
                        group == activeGroup)) {
                    (void)queueUiAction({
                        .kind = RuntimeActionKind::UiSelectRow,
                        .tab = tab,
                        .index = index,
                    });
                }
                ImGui::PopID();
                ImGui::Dummy({ 0.0f, 2.0f });
            }
            ImGui::Dummy({ 0.0f, 8.0f });
            ImGui::SeparatorText("SOURCE");
            {
                ScopedFont font(devui::render::FontRole::Mono, 14.0f);
                ImGui::TextColored(mutedColor(), "%zu SETTINGS", settings.size());
                const auto path = store.path().string();
                ImGui::PushTextWrapPos();
                ImGui::TextColored(mutedColor(0.78f), "%s", path.c_str());
                ImGui::PopTextWrapPos();
            }
        }

        [[nodiscard]] const char* controlTypeName(const SettingRecord& setting) noexcept
        {
            switch (setting.control.kind) {
            case setting_control::Kind::Checkbox:
                return "CHECKBOX";
            case setting_control::Kind::Numeric:
                if (setting.type == SettingType::Integer) {
                    return setting.control.bounded ? "INTEGER SLIDER" : "INTEGER DRAG";
                }
                return setting.control.bounded ? "FLOAT SLIDER" : "FLOAT DRAG";
            case setting_control::Kind::Dropdown:
                return "DROPDOWN";
            case setting_control::Kind::Text:
                return "TEXT";
            }
            return "VALUE";
        }

        [[nodiscard]] const char* numericFormat(const SettingRecord& setting) noexcept
        {
            if (setting.type == SettingType::Integer) {
                return "%.0f";
            }
            if (setting.control.step < 0.01) {
                return "%.3f";
            }
            if (setting.control.step < 0.1) {
                return "%.2f";
            }
            if (setting.control.step < 1.0) {
                return "%.1f";
            }
            return "%.0f";
        }

        void drawSettingRow(
            const SettingRecord& setting,
            std::size_t index,
            std::size_t activeIndex,
            ConfiguratorTab tab)
        {
            ImGui::PushID(static_cast<int>(index));
            const bool active = index == activeIndex;
            auto* expansionStorage = ImGui::GetStateStorage();
            const ImGuiID expansionId = ImGui::GetID("details-expanded");
            const bool expanded = expansionStorage->GetBool(expansionId, false);
            const float rowHeight = expanded ?
                (setting.description.empty() ? 98.0f : 122.0f) : 72.0f;
            ImGui::PushStyleColor(
                ImGuiCol_ChildBg,
                active ? ImVec4(0.055f, 0.138f, 0.143f, 1.0f) : surfaceColor());
            ImGui::PushStyleColor(
                ImGuiCol_Border,
                active ? accentColor(0.72f) : ImVec4(0.31f, 0.35f, 0.41f, 0.25f));
            ImGui::BeginChild("setting", ImVec2(0.0f, rowHeight), ImGuiChildFlags_Borders);
            const ImVec2 windowPosition = ImGui::GetWindowPos();
            const float windowWidth = ImGui::GetWindowWidth();
            const float controlsWidth = (std::min)(420.0f, windowWidth * 0.48f);
            const float controlsStart = windowWidth - controlsWidth;
            const float leftWidth = controlsStart - 58.0f;
            auto* draw = ImGui::GetWindowDrawList();
            if (active) {
                draw->AddRectFilled(
                    windowPosition,
                    { windowPosition.x + 5.0f, windowPosition.y + rowHeight },
                    packed(accentColor()),
                    4.0f);
            }

            ImGui::SetCursorPos({ 14.0f, 19.0f });
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { 7.0f, 7.0f });
            if (ImGui::ArrowButton(
                    "details", expanded ? ImGuiDir_Down : ImGuiDir_Right)) {
                expansionStorage->SetBool(expansionId, !expanded);
            }
            ImGui::PopStyleVar();
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip(expanded ? "Hide details" : "Show details");
            }

            ImGui::SetCursorPos({ 52.0f, 7.0f });
            {
                ScopedFont font(devui::render::FontRole::Mono, 13.0f);
                ImGui::TextColored(
                    active ? accentColor() : mutedColor(0.8f),
                    "%s  /  %s",
                    setting.section.c_str(),
                    controlTypeName(setting));
            }
            ImGui::SetCursorPos({ 48.0f, 26.0f });
            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, accentColor(0.09f));
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, accentColor(0.15f));
            {
                ScopedFont font(devui::render::FontRole::Medium, 19.0f);
                if (ImGui::Selectable(
                        setting.key.c_str(),
                        active,
                        ImGuiSelectableFlags_AllowOverlap,
                        { leftWidth, 30.0f })) {
                    (void)queueUiAction({
                        .kind = RuntimeActionKind::UiSelectRow,
                        .tab = tab,
                        .index = index,
                    });
                }
            }
            ImGui::PopStyleColor(3);

            const auto settingTarget = settingTargetLocked(index);
            switch (setting.control.kind) {
            case setting_control::Kind::Checkbox: {
                bool enabled = setting_control::equalsIgnoreCase(setting.value, "true") ||
                               setting.value == "1";
                ImGui::SetCursorPos({ windowWidth - 61.0f, 16.0f });
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { 8.0f, 8.0f });
                if (ImGui::Checkbox("##checkbox", &enabled)) {
                    (void)queueUiAction({
                        .kind = RuntimeActionKind::UiSetBooleanRow,
                        .tab = tab,
                        .value = enabled ? 1 : 0,
                        .index = index,
                        .horizontalTarget = settingTarget,
                    });
                }
                ImGui::PopStyleVar();
                break;
            }
            case setting_control::Kind::Numeric: {
                ImGui::SetCursorPos({ controlsStart, 16.0f });
                if (ImGui::Button("-", { 40.0f, 40.0f })) {
                    (void)queueUiAction({
                        .kind = RuntimeActionKind::UiAdjustRow,
                        .tab = tab,
                        .value = -1,
                        .index = index,
                        .horizontalTarget = settingTarget,
                    });
                }
                ImGui::SameLine();
                double numericValue = setting.numericValue;
                if (s_pendingNumericEdit && s_pendingNumericEdit->id == setting.id) {
                    numericValue = s_pendingNumericEdit->value;
                }
                ImGui::SetNextItemWidth(controlsWidth - 110.0f);
                const bool changed = setting.control.bounded ?
                    ImGui::SliderScalar(
                        "##numeric",
                        ImGuiDataType_Double,
                        &numericValue,
                        &setting.control.minimum,
                        &setting.control.maximum,
                        numericFormat(setting),
                        ImGuiSliderFlags_AlwaysClamp) :
                    ImGui::DragScalar(
                        "##numeric",
                        ImGuiDataType_Double,
                        &numericValue,
                        static_cast<float>(setting.control.step),
                        nullptr,
                        nullptr,
                        numericFormat(setting));
                if (changed) {
                    numericValue = setting_control::snapNumeric(setting.control, numericValue);
                    s_pendingNumericEdit = PendingNumericEdit{
                        .id = setting.id,
                        .index = index,
                        .target = settingTarget,
                        .value = numericValue,
                    };
                }
                const bool commit = ImGui::IsItemDeactivatedAfterEdit() &&
                    s_pendingNumericEdit && s_pendingNumericEdit->id == setting.id;
                if (commit) {
                    const auto pending = *s_pendingNumericEdit;
                    if (!queueUiAction({
                            .kind = RuntimeActionKind::UiSetNumericRow,
                            .tab = tab,
                            .index = pending.index,
                            .horizontalTarget = pending.target,
                            .numericValue = pending.value,
                        })) {
                        s_statusMessage = "Numeric change was not queued; try again";
                    }
                    s_pendingNumericEdit.reset();
                }
                ImGui::SameLine();
                if (ImGui::Button("+", { 40.0f, 40.0f })) {
                    (void)queueUiAction({
                        .kind = RuntimeActionKind::UiAdjustRow,
                        .tab = tab,
                        .value = 1,
                        .index = index,
                        .horizontalTarget = settingTarget,
                    });
                }
                break;
            }
            case setting_control::Kind::Dropdown: {
                const char* preview = setting.value.c_str();
                if (setting.selectedOptionIndex &&
                    *setting.selectedOptionIndex < setting.control.options.size()) {
                    preview = setting.control.options[*setting.selectedOptionIndex].label.c_str();
                }
                ImGui::SetCursorPos({ controlsStart, 16.0f });
                ImGui::SetNextItemWidth(controlsWidth - 20.0f);
                if (ImGui::BeginCombo("##dropdown", preview)) {
                    for (std::size_t optionIndex = 0;
                         optionIndex < setting.control.options.size(); ++optionIndex) {
                        const bool selected = setting.selectedOptionIndex &&
                            *setting.selectedOptionIndex == optionIndex;
                        if (ImGui::Selectable(
                                setting.control.options[optionIndex].label.c_str(), selected)) {
                            (void)queueUiAction({
                                .kind = RuntimeActionKind::UiSetOptionRow,
                                .tab = tab,
                                .value = static_cast<int>(optionIndex),
                                .index = index,
                                .horizontalTarget = settingTarget,
                            });
                        }
                        if (selected) {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                    ImGui::EndCombo();
                }
                break;
            }
            case setting_control::Kind::Text: {
                const ImVec2 valueMinimum{
                    windowPosition.x + controlsStart,
                    windowPosition.y + 16.0f
                };
                const ImVec2 valueMaximum{
                    windowPosition.x + windowWidth - 20.0f,
                    valueMinimum.y + 40.0f
                };
                draw->AddRectFilled(
                    valueMinimum,
                    valueMaximum,
                    packed(ImVec4(0.035f, 0.063f, 0.068f, 1.0f)),
                    7.0f);
                draw->AddRect(
                    valueMinimum,
                    valueMaximum,
                    packed(ImVec4(0.35f, 0.39f, 0.45f, 0.30f)),
                    7.0f);
                draw->PushClipRect(
                    { valueMinimum.x + 10.0f, valueMinimum.y },
                    { valueMaximum.x - 10.0f, valueMaximum.y },
                    true);
                draw->AddText(
                    fontFor(devui::render::FontRole::Mono),
                    16.0f,
                    { valueMinimum.x + 12.0f, valueMinimum.y + 10.0f },
                    packed(textColor(0.82f)),
                    setting.value.c_str());
                draw->PopClipRect();
                break;
            }
            }

            if (expanded) {
                if (!setting.description.empty()) {
                    ImGui::SetCursorPos({ 52.0f, 68.0f });
                    ImGui::PushTextWrapPos(windowWidth - 24.0f);
                    {
                        ScopedFont font(devui::render::FontRole::Body, 15.0f);
                        ImGui::TextColored(
                            mutedColor(0.92f), "%s", setting.description.c_str());
                    }
                    ImGui::PopTextWrapPos();
                }
                ImGui::SetCursorPos({ 52.0f, rowHeight - 23.0f });
                {
                    ScopedFont font(devui::render::FontRole::Mono, 12.0f);
                    ImGui::TextColored(mutedColor(0.62f), "%s", setting.id.c_str());
                }
            }
            ImGui::EndChild();
            ImGui::PopStyleColor(2);
            ImGui::PopID();
        }

        template <class Store>
        void drawSettingsWorkspace(
            const Store& store,
            bool loaded,
            std::size_t activeIndex,
            ConfiguratorTab tab,
            const std::string& status)
        {
            const auto& settings = store.settings();
            if (!loaded || settings.empty()) {
                {
                    ScopedFont heading(devui::render::FontRole::Heading, 27.0f);
                    ImGui::TextColored(accentColor(), "NO DATA");
                }
                {
                    ScopedFont body(devui::render::FontRole::Body, 17.0f);
                    ImGui::TextWrapped("%s", status.c_str());
                }
                return;
            }
            const std::string_view group =
                activeIndex < settings.size() ? settings[activeIndex].category : std::string_view{};
            std::size_t groupCount = 0;
            for (const auto& setting : settings) {
                if (std::string_view(setting.category) == group) {
                    ++groupCount;
                }
            }

            {
                ScopedFont font(devui::render::FontRole::Medium, 13.0f);
                ImGui::TextColored(
                    accentColor(),
                    "%s  /  ACTIVE GROUP",
                    tabName(tab));
            }
            {
                ScopedFont font(devui::render::FontRole::Heading, 28.0f);
                ImGui::TextUnformatted(
                    group.empty() ? "General" : settings[activeIndex].category.c_str());
            }
            ImGui::SameLine();
            ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 150.0f);
            {
                ScopedFont font(devui::render::FontRole::Mono, 14.0f);
                ImGui::TextColored(mutedColor(), "%zu CONTROLS", groupCount);
            }
            {
                ScopedFont font(devui::render::FontRole::Body, 15.0f);
                ImGui::TextColored(mutedColor(0.82f), "%s", status.c_str());
            }
            ImGui::Dummy({ 0.0f, 5.0f });
            ImGui::Separator();
            ImGui::Dummy({ 0.0f, 4.0f });

            ImGui::BeginChild("settings-rows", { 0.0f, 0.0f }, ImGuiChildFlags_None);
            static std::size_t lastActive = (std::numeric_limits<std::size_t>::max)();
            const bool selectionChanged = lastActive != activeIndex;
            lastActive = activeIndex;
            for (std::size_t index = 0; index < settings.size(); ++index) {
                if (std::string_view(settings[index].category) != group) {
                    continue;
                }
                drawSettingRow(settings[index], index, activeIndex, tab);
                if (selectionChanged && index == activeIndex) {
                    ImGui::SetScrollHereY(0.5f);
                }
                ImGui::Dummy({ 0.0f, 3.0f });
            }
            ImGui::EndChild();
        }

        void drawSpawnRail()
        {
            {
                ScopedFont font(devui::render::FontRole::Medium, 13.0f);
                ImGui::TextColored(accentColor(), "LOAD ORDER");
            }
            {
                ScopedFont font(devui::render::FontRole::Heading, 26.0f);
                ImGui::TextUnformatted("Plugins");
            }
            {
                ScopedFont font(devui::render::FontRole::Body, 15.0f);
                ImGui::TextColored(mutedColor(), "Select an item source");
            }
            ImGui::Dummy({ 0.0f, 8.0f });
            ImGui::SeparatorText("AVAILABLE");
            if (!s_spawnBrowser.indexBuilt()) {
                ImGui::TextWrapped("%s", s_spawnBrowser.lastResult().c_str());
                return;
            }
            ImGuiListClipper clipper;
            clipper.Begin(static_cast<int>(s_spawnBrowser.pluginCount()), 49.0f);
            while (clipper.Step()) {
                for (int rawIndex = clipper.DisplayStart; rawIndex < clipper.DisplayEnd; ++rawIndex) {
                    const auto index = static_cast<std::size_t>(rawIndex);
                    const auto* plugin = s_spawnBrowser.pluginAt(index);
                    if (!plugin) {
                        continue;
                    }
                    ImGui::PushID(rawIndex);
                    if (navigationRow(
                            plugin->name.c_str(),
                            plugin->items.size(),
                            index == s_spawnBrowser.pluginCursor(),
                            46.0f)) {
                        (void)queueUiAction({
                            .kind = RuntimeActionKind::UiSpawnSelectPlugin,
                            .tab = ConfiguratorTab::Spawn,
                            .index = index,
                        });
                    }
                    ImGui::PopID();
                    ImGui::Dummy({ 0.0f, 3.0f });
                }
            }
            ImGui::SeparatorText("INDEX");
            {
                ScopedFont font(devui::render::FontRole::Mono, 14.0f);
                ImGui::TextColored(mutedColor(), "%zu PLUGINS", s_spawnBrowser.pluginCount());
            }
        }

        void drawSpawnBrowse()
        {
            const auto* plugin = s_spawnBrowser.pluginAt(s_spawnBrowser.pluginCursor());
            {
                ScopedFont font(devui::render::FontRole::Medium, 13.0f);
                ImGui::TextColored(accentColor(), "SPAWN  /  BROWSE");
            }
            {
                ScopedFont font(devui::render::FontRole::Heading, 28.0f);
                ImGui::TextUnformatted(plugin ? plugin->name.c_str() : "No plugin");
            }
            ImGui::SameLine();
            ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 140.0f);
            {
                ScopedFont font(devui::render::FontRole::Mono, 14.0f);
                ImGui::TextColored(
                    mutedColor(), "%zu ITEMS", s_spawnBrowser.filteredItemCount());
            }
            ImGui::Dummy({ 0.0f, 2.0f });

            for (std::size_t index = 0; index < s_spawnBrowser.categoryCount(); ++index) {
                if (index != 0) {
                    ImGui::SameLine();
                }
                ImGui::PushID(static_cast<int>(index));
                const bool active = index == s_spawnBrowser.categoryCursor();
                if (active) {
                    ImGui::PushStyleColor(ImGuiCol_Button, accentColor(0.72f));
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.055f, 0.045f, 0.035f, 1.0f));
                }
                const auto category = s_spawnBrowser.categoryName(index);
                {
                    ScopedFont font(devui::render::FontRole::Medium, 15.0f);
                    if (ImGui::Button(category.data(), { 111.0f, 40.0f })) {
                        (void)queueUiAction({
                            .kind = RuntimeActionKind::UiSpawnSelectCategory,
                            .tab = ConfiguratorTab::Spawn,
                            .index = index,
                        });
                    }
                }
                if (active) {
                    ImGui::PopStyleColor(2);
                }
                ImGui::PopID();
            }
            ImGui::Dummy({ 0.0f, 6.0f });
            ImGui::Separator();
            ImGui::Dummy({ 0.0f, 5.0f });

            ImGui::BeginChild("spawn-items", { 0.0f, 0.0f }, ImGuiChildFlags_None);
            static std::size_t lastCursor = (std::numeric_limits<std::size_t>::max)();
            const bool cursorChanged = lastCursor != s_spawnBrowser.itemCursor();
            lastCursor = s_spawnBrowser.itemCursor();
            ImGuiListClipper clipper;
            clipper.Begin(static_cast<int>(s_spawnBrowser.filteredItemCount()), 62.0f);
            while (clipper.Step()) {
                for (int rawIndex = clipper.DisplayStart; rawIndex < clipper.DisplayEnd; ++rawIndex) {
                    const auto index = static_cast<std::size_t>(rawIndex);
                    const auto* item = s_spawnBrowser.filteredItemAt(index);
                    if (!item) {
                        continue;
                    }
                    ImGui::PushID(rawIndex);
                    const ImVec2 minimum = ImGui::GetCursorScreenPos();
                    const ImVec2 size{ ImGui::GetContentRegionAvail().x, 57.0f };
                    const bool clicked = ImGui::InvisibleButton("spawn-item", size);
                    const bool hovered = ImGui::IsItemHovered();
                    const bool selected = index == s_spawnBrowser.itemCursor();
                    const ImVec2 maximum{ minimum.x + size.x, minimum.y + size.y };
                    auto* draw = ImGui::GetWindowDrawList();
                    draw->AddRectFilled(
                        minimum,
                        maximum,
                        packed(selected ? accentColor(0.12f) :
                               hovered ? ImVec4(0.105f, 0.153f, 0.158f, 1.0f) : surfaceColor()),
                        8.0f);
                    draw->AddRect(
                        minimum,
                        maximum,
                        packed(selected ? accentColor(0.65f) :
                               ImVec4(0.31f, 0.35f, 0.41f, 0.24f)),
                        8.0f,
                        0,
                        selected ? 2.0f : 1.0f);
                    if (selected) {
                        draw->AddRectFilled(
                            minimum,
                            { minimum.x + 4.0f, maximum.y },
                            packed(accentColor()),
                            3.0f);
                    }
                    std::array<char, 48> metadata{};
                    std::snprintf(
                        metadata.data(),
                        metadata.size(),
                        "%s  /  %08X",
                        s_spawnBrowser.categoryName(item->category).data(),
                        item->formId);
                    const auto metadataSize = fontFor(devui::render::FontRole::Mono)->CalcTextSizeA(
                        14.0f, FLT_MAX, 0.0f, metadata.data());
                    draw->PushClipRect(
                        { minimum.x + 14.0f, minimum.y },
                        { maximum.x - metadataSize.x - 32.0f, maximum.y },
                        true);
                    draw->AddText(
                        fontFor(devui::render::FontRole::Medium),
                        19.0f,
                        { minimum.x + 16.0f, minimum.y + 16.0f },
                        packed(selected ? textColor() : textColor(0.82f)),
                        item->name.c_str());
                    draw->PopClipRect();
                    draw->AddText(
                        fontFor(devui::render::FontRole::Mono),
                        14.0f,
                        { maximum.x - metadataSize.x - 15.0f, minimum.y + 19.0f },
                        packed(selected ? accentColor() : mutedColor(0.82f)),
                        metadata.data());
                    if (clicked) {
                        (void)queueUiAction({
                            .kind = RuntimeActionKind::UiSpawnOpenItem,
                            .tab = ConfiguratorTab::Spawn,
                            .formId = item->formId,
                        });
                    }
                    if (cursorChanged && index == s_spawnBrowser.itemCursor()) {
                        ImGui::SetScrollHereY(0.5f);
                    }
                    ImGui::PopID();
                    ImGui::Dummy({ 0.0f, 5.0f });
                }
            }
            ImGui::EndChild();
        }

        void drawSpawnMenu()
        {
            const auto* item = s_spawnBrowser.currentItem();
            {
                ScopedFont font(devui::render::FontRole::Medium, 13.0f);
                ImGui::TextColored(accentColor(), "SPAWN  /  ITEM ACTIONS");
            }
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.075f, 0.113f, 0.118f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Border, accentColor(0.38f));
            ImGui::BeginChild("spawn-item-head", { 0.0f, 112.0f }, ImGuiChildFlags_Borders);
            {
                ScopedFont font(devui::render::FontRole::Heading, 29.0f);
                ImGui::TextUnformatted(item ? item->name.c_str() : "Selected item");
            }
            {
                ScopedFont font(devui::render::FontRole::Mono, 15.0f);
                ImGui::TextColored(
                    accentColor(),
                    "%08X  /  %s",
                    s_spawnBrowser.menuFormId(),
                    item ? s_spawnBrowser.categoryName(item->category).data() : "");
            }
            {
                ScopedFont font(devui::render::FontRole::Body, 15.0f);
                ImGui::TextColored(mutedColor(), "%s", s_spawnBrowser.lastResult().c_str());
            }
            ImGui::EndChild();
            ImGui::PopStyleColor(2);
            ImGui::Dummy({ 0.0f, 10.0f });
            ImGui::SeparatorText("CHOOSE ACTION");
            for (std::size_t index = 0; index < s_spawnBrowser.menuActionCount(); ++index) {
                const auto* action = s_spawnBrowser.menuActionAt(index);
                if (!action) {
                    continue;
                }
                ImGui::PushID(static_cast<int>(index));
                const bool selected = index == s_spawnBrowser.menuCursor();
                if (selected) {
                    ImGui::PushStyleColor(ImGuiCol_Header, accentColor(0.18f));
                    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, accentColor(0.25f));
                }
                {
                    ScopedFont font(devui::render::FontRole::Medium, 19.0f);
                    if (ImGui::Selectable(
                            action->label.c_str(), selected, 0, { 0.0f, 44.0f })) {
                        (void)queueUiAction({
                            .kind = RuntimeActionKind::UiSpawnActivateAction,
                            .tab = ConfiguratorTab::Spawn,
                            .index = index,
                            .formId = s_spawnBrowser.menuFormId(),
                        });
                    }
                }
                if (selected) {
                    ImGui::PopStyleColor(2);
                }
                ImGui::PopID();
                ImGui::Dummy({ 0.0f, 3.0f });
            }
            ImGui::Dummy({ 0.0f, 8.0f });
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.075f, 0.114f, 0.119f, 1.0f));
            if (ImGui::Button("Back to item list", { 230.0f, 46.0f })) {
                (void)queueUiAction({
                    .kind = RuntimeActionKind::UiSpawnBack,
                    .tab = ConfiguratorTab::Spawn,
                });
            }
            ImGui::PopStyleColor();
        }

        void drawSpawnWorkspace()
        {
            if (!s_spawnBrowser.indexBuilt()) {
                ImGui::TextWrapped("%s", s_spawnBrowser.lastResult().c_str());
                return;
            }
            if (s_spawnBrowser.view() == SpawnView::ItemMenu) {
                drawSpawnMenu();
            } else {
                drawSpawnBrowse();
            }
        }

        void drawCurrentTabBody()
        {
            if(s_activeTab.load(std::memory_order_acquire)==ConfiguratorTab::Wheel) {
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{18.0f,14.0f});
                ImGui::BeginChild("wheel-config",{0,0},ImGuiChildFlags_Borders);
                wheel::drawWheelConfig();
                ImGui::EndChild();ImGui::PopStyleVar();return;
            }
            constexpr float railWidth = 240.0f;
            const auto tab = s_activeTab.load(std::memory_order_acquire);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 16.0f, 15.0f });
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.045f, 0.076f, 0.081f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.31f, 0.35f, 0.41f, 0.30f));
            ImGui::BeginChild("rail", { railWidth, 0.0f }, ImGuiChildFlags_Borders);
            switch (tab) {
            case ConfiguratorTab::Settings:
                drawSettingsRail(s_store, s_storeLoaded, s_activeIndex, tab, s_statusMessage);
                break;
            case ConfiguratorTab::Spawn:
                drawSpawnRail();
                break;
            }
            ImGui::EndChild();
            ImGui::PopStyleColor(2);
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.040f, 0.068f, 0.073f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.31f, 0.35f, 0.41f, 0.30f));
            ImGui::BeginChild("workspace", { 0.0f, 0.0f }, ImGuiChildFlags_Borders);
            switch (tab) {
            case ConfiguratorTab::Settings:
                drawSettingsWorkspace(s_store, s_storeLoaded, s_activeIndex, tab, s_statusMessage);
                break;
            case ConfiguratorTab::Spawn:
                drawSpawnWorkspace();
                break;
            }
            ImGui::EndChild();
            ImGui::PopStyleColor(2);
            ImGui::PopStyleVar();
        }

        void drawResizeChrome()
        {
            const auto hovered = s_hoveredResizeHandle.load(std::memory_order_acquire);
            const auto active = s_activeResizeHandle.load(std::memory_order_acquire);
            const auto effective = active != panel_resize::Handle::None ? active : hovered;
            switch (effective) {
            case panel_resize::Handle::Left:
            case panel_resize::Handle::Right:
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
                break;
            case panel_resize::Handle::Top:
            case panel_resize::Handle::Bottom:
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
                break;
            case panel_resize::Handle::TopLeft:
            case panel_resize::Handle::BottomRight:
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNWSE);
                break;
            case panel_resize::Handle::TopRight:
            case panel_resize::Handle::BottomLeft:
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNESW);
                break;
            case panel_resize::Handle::None:
                break;
            }

            const ImVec2 minimum = ImGui::GetWindowPos();
            const ImVec2 maximum{
                minimum.x + ImGui::GetWindowWidth(),
                minimum.y + ImGui::GetWindowHeight()
            };
            auto* draw = ImGui::GetWindowDrawList();
            draw->AddRect(
                { minimum.x + 3.0f, minimum.y + 3.0f },
                { maximum.x - 3.0f, maximum.y - 3.0f },
                packed(active != panel_resize::Handle::None ? accentColor(0.72f) : accentColor(0.24f)),
                0.0f,
                0,
                active != panel_resize::Handle::None ? 3.0f : 1.0f);

            const auto handleColor = [&](panel_resize::Handle handle) {
                if (handle == active) {
                    return packed(accentColor());
                }
                if (handle == hovered) {
                    return packed(accentColor(0.92f));
                }
                return packed(accentColor(0.30f));
            };
            const auto thickness = [&](panel_resize::Handle handle) {
                return handle == active ? 5.0f : handle == hovered ? 4.0f : 2.0f;
            };
            constexpr float inset = 7.0f;
            constexpr float corner = 38.0f;
            constexpr float edge = 52.0f;
            const float left = minimum.x + inset;
            const float right = maximum.x - inset;
            const float top = minimum.y + inset;
            const float bottom = maximum.y - inset;
            const float middleX = (left + right) * 0.5f;
            const float middleY = (top + bottom) * 0.5f;

            const auto line = [&](ImVec2 from, ImVec2 to, panel_resize::Handle handle) {
                draw->AddLine(from, to, handleColor(handle), thickness(handle));
            };
            line({ left, top }, { left + corner, top }, panel_resize::Handle::TopLeft);
            line({ left, top }, { left, top + corner }, panel_resize::Handle::TopLeft);
            line({ right - corner, top }, { right, top }, panel_resize::Handle::TopRight);
            line({ right, top }, { right, top + corner }, panel_resize::Handle::TopRight);
            line({ left, bottom }, { left + corner, bottom }, panel_resize::Handle::BottomLeft);
            line({ left, bottom - corner }, { left, bottom }, panel_resize::Handle::BottomLeft);
            line({ right - corner, bottom }, { right, bottom }, panel_resize::Handle::BottomRight);
            line({ right, bottom - corner }, { right, bottom }, panel_resize::Handle::BottomRight);
            line({ left, middleY - edge * 0.5f }, { left, middleY + edge * 0.5f }, panel_resize::Handle::Left);
            line({ right, middleY - edge * 0.5f }, { right, middleY + edge * 0.5f }, panel_resize::Handle::Right);
            line({ middleX - edge * 0.5f, top }, { middleX + edge * 0.5f, top }, panel_resize::Handle::Top);
            line({ middleX - edge * 0.5f, bottom }, { middleX + edge * 0.5f, bottom }, panel_resize::Handle::Bottom);
        }

        // END_PRIVATE_IMPLEMENTATION
    }

    bool drawImGui(float x, float y, float width, float height) noexcept
    {
        std::unique_lock lock(s_runtimeMutex, std::try_to_lock);
        if (!lock.owns_lock() || !s_panelOpen.load(std::memory_order_acquire)) {
            return false;
        }

        try {
            ImGui::SetNextWindowPos(ImVec2(x, y), ImGuiCond_Always);
            ImGui::SetNextWindowSize(
                ImVec2(width, height),
                ImGuiCond_Always);
            constexpr ImGuiWindowFlags flags =
                ImGuiWindowFlags_NoDecoration |
                ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoSavedSettings |
                ImGuiWindowFlags_NoBringToFrontOnFocus |
                ImGuiWindowFlags_NoNavFocus;
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
            if (!ImGui::Begin("Wheel Config##root", nullptr, flags)) {
                ImGui::End();
                ImGui::PopStyleVar();
                return true;
            }

            const ImVec2 windowPosition = ImGui::GetWindowPos();
            const float windowWidth = ImGui::GetWindowWidth();
            const float windowHeight = ImGui::GetWindowHeight();
            auto* draw = ImGui::GetWindowDrawList();
            draw->AddRectFilledMultiColor(
                windowPosition,
                { windowPosition.x + windowWidth, windowPosition.y + windowHeight },
                packed(ImVec4(0.035f, 0.063f, 0.068f, 1.0f)),
                packed(ImVec4(0.026f, 0.051f, 0.056f, 1.0f)),
                packed(ImVec4(0.022f, 0.045f, 0.050f, 1.0f)),
                packed(ImVec4(0.029f, 0.055f, 0.060f, 1.0f)));
            drawTopBar();
            constexpr float bodyTop = 106.0f;
            ImGui::SetCursorPos({ 22.0f, bodyTop });
            ImGui::BeginChild(
                "body",
                { windowWidth - 44.0f, windowHeight - bodyTop - 22.0f },
                ImGuiChildFlags_None);
            drawCurrentTabBody();
            ImGui::EndChild();
            drawResizeChrome();
            ImGui::End();
            ImGui::PopStyleVar();
            return true;
        } catch (...) {
            static std::atomic_bool logged = false;
            if (!logged.exchange(true, std::memory_order_relaxed)) {
                logger::error("Wheel Config contained an exception while building the ImGui frame");
            }
            return false;
        }
    }

    void onFrameworkPanelFrame(
        float physicalWidth,
        panel_resize::Handle hovered,
        panel_resize::Handle active) noexcept
    {
        if (std::isfinite(physicalWidth)) {
            s_sessionPanelPhysicalWidth.store(
                std::clamp(
                    physicalWidth,
                    devui::render::kMinimumPanelPhysicalWidth,
                    devui::render::kMaximumPanelPhysicalWidth),
                std::memory_order_release);
        }
        s_hoveredResizeHandle.store(hovered, std::memory_order_release);
        s_activeResizeHandle.store(active, std::memory_order_release);
    }

    void onGameDataReady()
    {
        {
            std::scoped_lock lock(s_runtimeMutex);
            if (!s_spawnBrowser.ensureIndexBuilt()) {
                logger::warn(
                    "Wheel Config spawn index not ready at game-data-ready ({}); it will retry on first use",
                    s_spawnBrowser.lastResult());
            }
        }

    }

    void onGameSessionReady()
    {
        std::scoped_lock lock(s_runtimeMutex);
        loadRockSettingsLocked(false);
    }

    bool isOpen() noexcept { return s_panelOpen.load(std::memory_order_acquire); }
    bool takeInventoryRefreshRequest() noexcept { return s_inventoryRefreshRequested.exchange(false); }

    void setAvailable(bool available) noexcept
    {
        const bool wasAvailable = s_providerInputReady.exchange(available, std::memory_order_acq_rel);
        if (!available && wasAvailable) close();
    }

    void close() noexcept
    {
        s_panelOpen.store(false, std::memory_order_release);
        devui::render::SetPanelOpen(false);
        invalidateQueuedRuntimeActions();
    }

    void openBeside(const devui::render::PanelPose& wheelPose)
    {
        if (isOpen() || !s_providerInputReady.load(std::memory_order_acquire)) return;
        const float width = s_sessionPanelPhysicalWidth.load(std::memory_order_acquire);
        const float offset = wheelPose.physicalWidth * 0.5f + width * 0.5f + 4.0f;
        PanelPose pose;
        pose.position = {wheelPose.center.x + wheelPose.right.x * offset,
                         wheelPose.center.y + wheelPose.right.y * offset,
                         wheelPose.center.z + wheelPose.right.z * offset};
        pose.right = {wheelPose.right.x, wheelPose.right.y, wheelPose.right.z};
        pose.up = {wheelPose.up.x, wheelPose.up.y, wheelPose.up.z};
        pose.front = {wheelPose.front.x, wheelPose.front.y, wheelPose.front.z};
        pose.physicalWidth = width;
        pose.physicalHeight = width / devui::render::kPanelAspectRatio;
        (void)queueUiAction({.kind = RuntimeActionKind::OpenPanel, .hasPanelPose = true, .panelPose = pose});
    }

#ifdef WHEEL_DESKTOP_PREVIEW
    void initializePreview()
    {
        s_providerInputReady.store(true);
        loadRockSettingsLocked(false); // Read-only snapshot; preview saves are memory-only.
        s_spawnBrowser.ensureIndexBuilt(); // Compiled preview fixture, no engine calls.
    }

    void setPreviewOpen(bool open) { s_panelOpen.store(open); }
    void drainPreviewActions() { drainRuntimeActions(); }
#endif

    void shutdown()
    {
        close();
        devui::render::Shutdown();
    }
}
