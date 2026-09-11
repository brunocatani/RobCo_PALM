#include "PCH.h"

#include "ConfiguratorRuntime.h"

#include "IniSettingsStore.h"
#include "PanelResizePolicy.h"
#include "SpawnBrowser.h"
#include "WheelConfig.h"
#include "render/FrameworkPanelRenderer.h"
#include "render/NativeRenderer.h"
#include <imgui_internal.h>
#include "render/ConfigUi.h"


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
            UiBack,
            UiSelectTab,
            UiSelectMod,
            UiSelectRow,
            UiAdjustRow,
            UiSetBooleanRow,
            UiSetNumericRow,
            UiSetOptionRow,
            UiReload,
            UiRefreshSettings,
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

        struct ModSettings {
            IniSettingsStore store;
            bool loaded = false;
            bool available = false;
            std::uint64_t identity = 0;
            SelectionAnchor anchor;
            std::size_t activeIndex = 0;
        };
        std::array<ModSettings, 4> s_modSettings{
            ModSettings{IniSettingsStore({}, RpsMod::Rock)},
            ModSettings{IniSettingsStore({}, RpsMod::Paper)},
            ModSettings{IniSettingsStore({}, RpsMod::Scissors)},
            ModSettings{IniSettingsStore({}, RpsMod::RockDeveloper)}
        };
        std::size_t s_modIndex = 0;
        ModSettings& activeSettings() { return s_modSettings[s_modIndex]; }
        SpawnBrowser s_spawnBrowser;
        std::mutex s_runtimeMutex;
        std::string s_statusMessage;
        std::optional<PendingNumericEdit> s_pendingNumericEdit;

        std::atomic<ConfiguratorTab> s_activeTab{ ConfiguratorTab::Wheel };
        std::atomic_bool s_panelOpen = false;
        std::atomic_bool s_settingsRefreshQueued = false;
        std::atomic_bool s_panelOpening = false;
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
            auto identity = activeSettings().identity;
            hashUint64(identity, s_modIndex);
            return selectedIdentity(identity, index);
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

        [[nodiscard]] bool readActiveStoreLocked(bool reload)
        {
            s_pendingNumericEdit.reset();
            return readStoreLocked(
                activeSettings().store, activeSettings().loaded, activeSettings().activeIndex, activeSettings().anchor, activeSettings().identity, reload);
        }

        [[nodiscard]] bool spawnTabActive() noexcept
        {
            return s_activeTab.load(std::memory_order_acquire) == ConfiguratorTab::Spawn;
        }

        void updateSettingsStatusLocked()
        {
            s_statusMessage = activeSettings().loaded ?
                modInfo(static_cast<RpsMod>(s_modIndex)).applyHint :
                activeSettings().store.lastError();
        }

        void loadRpsSettingsLocked(bool reloadLoaded)
        {
            s_pendingNumericEdit.reset();
            for (std::size_t i = 0; i < s_modSettings.size(); ++i) {
                auto& settings = s_modSettings[i];
#ifndef WHEEL_DESKTOP_PREVIEW
                // F4SEVR unloads rejected plugins. Inspect loaded modules, not files in MO2.
                settings.available = GetModuleHandleW(kRpsMods[i].module) != nullptr;
#endif
                if (settings.available)
                    (void)readStoreLocked(settings.store, settings.loaded, settings.activeIndex,
                        settings.anchor, settings.identity, reloadLoaded && settings.loaded);
                else settings.loaded = false;
            }
            if (!activeSettings().available) {
                for (std::size_t i = 0; i < s_modSettings.size(); ++i)
                    if (s_modSettings[i].available) { s_modIndex = i; break; }
            }
            updateSettingsStatusLocked();
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
            s_settingsRefreshQueued.store(false);
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
            struct OpeningFinished {~OpeningFinished(){s_panelOpening.store(false);}} openingFinished;
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

            loadRpsSettingsLocked(true);
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
                "Wheel Config RPS configurator panel opened at {:.2f},{:.2f},{:.2f}",
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

        void applySettingChangeLocked(const SettingChangeResult& result)
        {
            s_statusMessage = result.message.empty() ? "No change" : result.message;
            if (result.changed && result.saved) {
#ifndef WHEEL_DESKTOP_PREVIEW
                (void)readActiveStoreLocked(true);
                s_statusMessage = activeSettings().loaded ?
                    std::format("Saved {}. {}", result.setting.key, kRpsMods[s_modIndex].applyHint) : activeSettings().store.lastError();
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
                if (index < activeSettings().store.settings().size()) {
                    activeSettings().activeIndex = index;
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
                   activeSettings().available && activeSettings().loaded && action.index < activeSettings().store.settings().size() &&
                   action.horizontalTarget == settingTargetLocked(action.index);
        }

        void setBooleanRowLocked(const RuntimeAction& action)
        {
            if (!validatesSettingTargetLocked(action)) {
                return;
            }
            activeSettings().activeIndex = action.index;
            applySettingChangeLocked(activeSettings().store.setBooleanByIndex(action.index, action.value != 0));
        }

        void setNumericRowLocked(const RuntimeAction& action)
        {
            if (!validatesSettingTargetLocked(action)) {
                return;
            }
            activeSettings().activeIndex = action.index;
            applySettingChangeLocked(activeSettings().store.setNumericByIndex(action.index, action.numericValue));
        }

        void setOptionRowLocked(const RuntimeAction& action)
        {
            if (!validatesSettingTargetLocked(action) || action.value < 0) {
                return;
            }
            activeSettings().activeIndex = action.index;
            applySettingChangeLocked(activeSettings().store.setOptionByIndex(
                action.index, static_cast<std::size_t>(action.value)));
        }

        void reloadActiveLocked(ConfiguratorTab tab)
        {
            if (tab != s_activeTab.load(std::memory_order_acquire)) {
                return;
            }
            switch (tab) {
            case ConfiguratorTab::Settings:
                (void)readActiveStoreLocked(true);
                updateSettingsStatusLocked();
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
            case RuntimeActionKind::UiSelectMod:
                if (s_activeTab.load() == ConfiguratorTab::Settings && action.index < s_modSettings.size() &&
                    s_modSettings[action.index].available && action.index != s_modIndex) {
                    s_modIndex = action.index;
                    s_pendingNumericEdit.reset();
                    invalidateQueuedRuntimeActions();
                    updateSettingsStatusLocked();
                }
                break;
            case RuntimeActionKind::UiSelectRow:
                selectRowLocked(action.tab, action.index);
                break;
            case RuntimeActionKind::UiAdjustRow:
                if (validatesSettingTargetLocked(action)) {
                    activeSettings().activeIndex = action.index;
                    applySettingChangeLocked(activeSettings().store.adjustByIndex(action.index, action.value));
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
            case RuntimeActionKind::UiRefreshSettings:
                s_settingsRefreshQueued.store(false);
                if (s_activeTab.load() == ConfiguratorTab::Settings) {
                    (void)readActiveStoreLocked(true);
                }
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
            case RuntimeActionKind::UiBack:
                if (spawnTabActive() && s_spawnBrowser.back()) break;
                if (s_activeTab.load(std::memory_order_acquire) != ConfiguratorTab::Wheel) {
                    s_pendingNumericEdit.reset();
                    s_activeTab.store(ConfiguratorTab::Wheel, std::memory_order_release);
                } else {
                    closePanelLocked("Closed by Config back navigation");
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
            return devui::visual::accent(alpha);
        }

        [[nodiscard]] ImVec4 textColor(float alpha = 1.0f) noexcept
        {
            return devui::visual::text(alpha);
        }

        [[nodiscard]] ImVec4 mutedColor(float alpha = 1.0f) noexcept
        {
            return devui::visual::muted(alpha);
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

        [[nodiscard]] bool tabChip(const char* label, bool active, ImVec2 size)
        {
            ImGui::PushID(label);
            const ImVec2 minimum = ImGui::GetCursorScreenPos();
            const bool clicked = ImGui::InvisibleButton("tab", size);
            const bool hovered = ImGui::IsItemHovered();
            const ImVec2 maximum{minimum.x + size.x, minimum.y + size.y};
            auto* draw = ImGui::GetWindowDrawList();
            if (hovered) draw->AddRectFilled(minimum, maximum, packed(accentColor(0.06f)), 4);
            if (active) draw->AddRectFilled({minimum.x + 10, maximum.y - 3},
                {maximum.x - 10, maximum.y}, packed(accentColor()), 2);
            auto* font = fontFor(devui::render::FontRole::Medium);
            const float textWidth = font->CalcTextSizeA(26, FLT_MAX, 0, label).x;
            const float fontSize = textWidth > size.x - 24 ? 26 * (size.x - 24) / textWidth : 26;
            draw->AddText(font, fontSize,
                {minimum.x + 12, minimum.y + 16},
                packed(active ? accentColor() : textColor(hovered ? 0.95f : 0.65f)), label);
            ImGui::PopID();
            return clicked;
        }

        [[nodiscard]] bool navigationRow(const char* label, std::size_t count, bool active, float = 48)
        {
            std::array<char, 24> badge{};
            std::snprintf(badge.data(), badge.size(), "%zu", count);
            return devui::visual::navigation(label, active, badge.data());
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
            constexpr std::array tabs{ConfiguratorTab::Wheel, ConfiguratorTab::Settings, ConfiguratorTab::Spawn};
            constexpr std::array labels{"Wheel items", "RPS configurator", "Spawner"};
            const auto active = s_activeTab.load(std::memory_order_acquire);
            const auto p = ImGui::GetWindowPos();
            const float width = ImGui::GetWindowWidth();
            auto* draw = ImGui::GetWindowDrawList();
            draw->AddLine({p.x, p.y + 94}, {p.x + width, p.y + 94}, packed(mutedColor(0.24f)));
            draw->AddRectFilled(p, {p.x + 5, p.y + 94}, packed(accentColor(0.7f)));
            ImGui::SetCursorPos({28, 15});
            { ScopedFont font(devui::render::FontRole::Medium, 16); ImGui::TextColored(accentColor(), "R O C K  /  F I E L D  K I T"); }
            ImGui::SetCursorPos({28, 43});
            { ScopedFont font(devui::render::FontRole::Heading, 30); ImGui::TextUnformatted("Configuration"); }
            const float start = devui::visual::railWidth(width) + 14;
            for (std::size_t i = 0; i < tabs.size(); ++i) {
                ImGui::SetCursorPos({start + static_cast<float>(i) * 194, 28});
                if (tabChip(labels[i], tabs[i] == active, {184, 62})) queueTab(tabs[i]);
            }
            ImGui::SetCursorPos({width - 322, 20});
            { ScopedFont font(devui::render::FontRole::Medium, 24);
              if (ImGui::Button("< Back", {100, 54})) (void)queueUiAction({.kind = RuntimeActionKind::UiBack}); }
            ImGui::SameLine(0, 10);
            const float percent = s_sessionPanelPhysicalWidth.load(std::memory_order_acquire) / devui::render::kDefaultPanelPhysicalWidth * 100;
            std::array<char, 32> sizeLabel{};
            std::snprintf(sizeLabel.data(), sizeLabel.size(), "SIZE  %.0f%%", percent);
            { ScopedFont font(devui::render::FontRole::Medium, 18);
              if (ImGui::Button(sizeLabel.data(), {126, 54})) (void)queueUiAction({.kind = RuntimeActionKind::UiResetPanelSize}); }
            ImGui::SameLine(0, 10);
            ImGui::PushStyleColor(ImGuiCol_Button, {0.16f, 0.075f, 0.085f, 1});
            if (ImGui::Button("X##close", {52, 54})) (void)queueUiAction({.kind = RuntimeActionKind::UiClose});
            ImGui::PopStyleColor();
        }

        template <class Store>
        void drawSettingsRail(
            const Store& store,
            bool loaded,
            std::size_t activeIndex,
            ConfiguratorTab tab,
            const std::string& status)
        {
            devui::visual::caption(modInfo(static_cast<RpsMod>(s_modIndex)).name);
            ImGui::Dummy({0, 12});
            {
                ScopedFont font(devui::render::FontRole::Body, 17);
                if (ImGui::Button("Reload from disk", {-1, 40})) {
                    (void)queueUiAction({.kind = RuntimeActionKind::UiReload, .tab = tab});
                }
            }
            ImGui::Dummy({0, 12});
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
            constexpr float rowHeight = 66.0f;
            // A row is not a scroll owner: wheel input belongs to the enclosing column.
            ImGui::PushStyleColor(ImGuiCol_ChildBg, active ? accentColor(0.055f) : ImVec4(0, 0, 0, 0));
            ImGui::BeginChild("setting", {0, rowHeight}, ImGuiChildFlags_None,
                ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
            const ImVec2 windowPosition = ImGui::GetWindowPos();
            const float windowWidth = ImGui::GetWindowWidth();
            const float controlsWidth = (std::min)(420.0f, windowWidth * 0.48f);
            const float controlsStart = windowWidth - controlsWidth;
            auto* draw = ImGui::GetWindowDrawList();
            draw->AddLine({windowPosition.x + 12, windowPosition.y + rowHeight - 1},
                {windowPosition.x + windowWidth - 12, windowPosition.y + rowHeight - 1},
                packed(mutedColor(0.15f)));
            draw->PushClipRect({windowPosition.x + 12, windowPosition.y},
                {windowPosition.x + controlsStart - 14, windowPosition.y + rowHeight}, true);
            ImGui::SetCursorPos({14, 8});
            {
                ScopedFont font(devui::render::FontRole::Medium, 25);
                const devui::visual::ReadableLabel label(setting.key, true);
                ImGui::TextUnformatted(label.text.data());
            }
            draw->PopClipRect();
            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::PushTextWrapPos(ImGui::GetFontSize() * 30);
                ImGui::TextUnformatted(setting.key.c_str());
                if (!setting.description.empty()) ImGui::TextWrapped("%s", setting.description.c_str());
                if (setting.fromRockApi) {
                    ImGui::Text("Default: %s", setting.defaultValue.c_str());
                    ImGui::TextUnformatted(setting.overridden ? "Saved override" : "Using default");
                }
                ImGui::TextDisabled("%s", setting.id.c_str());
                ImGui::PopTextWrapPos();
                ImGui::EndTooltip();
            }
            ImGui::SetCursorPos({14, 37});
            draw->PushClipRect({windowPosition.x + 12, windowPosition.y}, {windowPosition.x + controlsStart - 14, windowPosition.y + rowHeight}, true);
            {
                ScopedFont font(devui::render::FontRole::Body, 18);
                ImGui::TextColored(mutedColor(0.85f), "%s", setting.description.empty() ? setting.section.c_str() : setting.description.c_str());
            }

            draw->PopClipRect();
            const auto settingTarget = settingTargetLocked(index);
            switch (setting.control.kind) {
            case setting_control::Kind::Checkbox: {
                bool enabled = setting_control::equalsIgnoreCase(setting.value, "true") ||
                               setting.value == "1";
                ImGui::SetCursorPos({ windowWidth - 61.0f, 10.0f });
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { 6.0f, 6.0f });
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
                ImGui::SetCursorPos({ controlsStart, 10.0f });
                if (ImGui::Button("-", { 36.0f, 36.0f })) {
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
                if (ImGui::Button("+", { 36.0f, 36.0f })) {
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
                ImGui::SetCursorPos({ controlsStart, 10.0f });
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
                    packed(ImVec4(0.035f, 0.085f, 0.10f, 1.0f)),
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

            ImGui::EndChild();
            ImGui::PopStyleColor();
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
            const devui::visual::ReadableLabel groupLabel(group.empty() ? "General" : settings[activeIndex].category.c_str());
            devui::visual::heading(groupLabel.text.data(), status.c_str());
            const auto header = ImGui::GetCursorScreenPos();
            const float width = ImGui::GetContentRegionAvail().x;
            ImGui::GetWindowDrawList()->AddRectFilled(header, {header.x + width, header.y + 36}, packed(ImVec4(0.045f,0.10f,0.12f,1)));
            { ScopedFont font(devui::render::FontRole::Medium, 18);
              ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 14);
              ImGui::TextColored(mutedColor(), "SETTING");
              ImGui::SameLine();
              ImGui::SetCursorPosX(ImGui::GetWindowWidth() - (std::min)(420.0f, width * 0.48f) - 24);
              ImGui::TextColored(mutedColor(), "VALUE"); }
            ImGui::SetCursorScreenPos({header.x, header.y + 40});
            ImGui::BeginChild("settings-rows", { 0.0f, 0.0f }, ImGuiChildFlags_None);
            const auto groupId = ImGui::GetID(group.data(), group.data() + group.size());
            auto* storage = ImGui::GetStateStorage();
            const auto groupKey = ImGui::GetID("current-group");
            if (storage->GetInt(groupKey) != static_cast<int>(groupId)) {
                ImGui::SetScrollY(0);
                storage->SetInt(groupKey, static_cast<int>(groupId));
            }
            for (std::size_t index = 0; index < settings.size(); ++index) {
                if (std::string_view(settings[index].category) != group) {
                    continue;
                }
                drawSettingRow(settings[index], index, activeIndex, tab);
                ImGui::Dummy({ 0.0f, 3.0f });
            }
            ImGui::EndChild();
        }

        void drawSpawnRail()
        {
            devui::visual::caption("ITEM SOURCES");
            ImGui::Dummy({0, 12});
            if (!s_spawnBrowser.indexBuilt()) {
                ImGui::TextWrapped("%s", s_spawnBrowser.lastResult().c_str()); return;
            }
            ImGuiListClipper clipper;
            clipper.Begin(static_cast<int>(s_spawnBrowser.pluginCount()));
            while (clipper.Step()) for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                const auto* plugin = s_spawnBrowser.pluginAt(i);
                if (!plugin) continue;
                ImGui::PushID(i);
                if (navigationRow(plugin->name.c_str(), plugin->items.size(), i == s_spawnBrowser.pluginCursor()))
                    (void)queueUiAction({.kind = RuntimeActionKind::UiSpawnSelectPlugin, .tab = ConfiguratorTab::Spawn, .index = static_cast<std::size_t>(i)});
                ImGui::PopID();
            }
        }

        void drawSpawnBrowse()
        {
            const auto* plugin = s_spawnBrowser.pluginAt(s_spawnBrowser.pluginCursor());
            devui::visual::heading("Spawner", plugin ? plugin->name.c_str() : "No item source");
            const float categoryWidth = (ImGui::GetContentRegionAvail().x - 7 * 6) / 8;
            for (std::size_t i = 0; i < s_spawnBrowser.categoryCount(); ++i) {
                if (i) ImGui::SameLine(0, 6);
                ImGui::PushID(static_cast<int>(i));
                const bool active = i == s_spawnBrowser.categoryCursor();
                ImGui::PushStyleColor(ImGuiCol_Button, active ? accentColor(0.2f) : ImVec4(0,0,0,0));
                ImGui::PushStyleColor(ImGuiCol_Text, active ? accentColor() : mutedColor());
                { ScopedFont font(devui::render::FontRole::Medium, 18);
                  if (ImGui::Button(s_spawnBrowser.categoryName(i).data(), {categoryWidth, 36}))
                    (void)queueUiAction({.kind = RuntimeActionKind::UiSpawnSelectCategory, .tab = ConfiguratorTab::Spawn, .index = i}); }
                ImGui::PopStyleColor(2); ImGui::PopID();
            }
            ImGui::Dummy({0, 16});
            constexpr auto flags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg |
                ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersInnerH;
            if (ImGui::BeginTable("spawn-items", 3, flags, {0,0})) {
                ImGui::TableSetupColumn("ITEM", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("TYPE", ImGuiTableColumnFlags_WidthFixed, 130);
                ImGui::TableSetupColumn("FORM ID", ImGuiTableColumnFlags_WidthFixed, 120);
                devui::visual::tableHeader("ITEM", "TYPE", "FORM ID");
                ImGuiListClipper clipper;
                clipper.Begin(static_cast<int>(s_spawnBrowser.filteredItemCount()));
                while (clipper.Step()) for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                    const auto* item = s_spawnBrowser.filteredItemAt(i);
                    if (!item) continue;
                    ImGui::PushID(i);
                    ImGui::TableNextRow(0, 46); ImGui::TableSetColumnIndex(0);
                    if (ImGui::Selectable(item->name.c_str(), i == s_spawnBrowser.itemCursor(), ImGuiSelectableFlags_SpanAllColumns, {0,30})) {
                        (void)queueUiAction({.kind = RuntimeActionKind::UiSpawnOpenItem, .tab = ConfiguratorTab::Spawn, .formId = item->formId});
                    }
                    ImGui::TableSetColumnIndex(1);
                    { ScopedFont font(devui::render::FontRole::Body, 17); ImGui::TextColored(mutedColor(), "%s", s_spawnBrowser.categoryName(item->category).data()); }
                    ImGui::TableSetColumnIndex(2);
                    { ScopedFont font(devui::render::FontRole::Mono, 16); ImGui::TextColored(mutedColor(), "%08X", item->formId); }
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
        }

        void drawSpawnMenu()
        {
            const auto* item = s_spawnBrowser.currentItem();
            ImGui::BeginChild("spawn-item-head", {0,104}, ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar);
            devui::visual::heading(item ? item->name.c_str() : "Selected item", "Add this item to your inventory.");
            { ScopedFont font(devui::render::FontRole::Mono, 15);
              ImGui::TextColored(mutedColor(), "%08X  /  %s", s_spawnBrowser.menuFormId(), item ? s_spawnBrowser.categoryName(item->category).data() : ""); }
            ImGui::EndChild();
            ImGui::Dummy({0, 18});
            devui::visual::caption("QUANTITY & ACTIONS");
            ImGui::Dummy({0, 12});
            const float width = (std::min)(320.0f, (ImGui::GetContentRegionAvail().x - 16) / 2);
            for (std::size_t i = 0; i < s_spawnBrowser.menuActionCount(); ++i) {
                const auto* action = s_spawnBrowser.menuActionAt(i);
                if (!action) continue;
                if (i % 2) ImGui::SameLine(0, 16);
                ImGui::PushID(static_cast<int>(i));
                ImGui::PushStyleColor(ImGuiCol_Button, i == 0 ? accentColor(0.16f) : ImVec4(0.06f,0.11f,0.125f,1));
                { ScopedFont font(devui::render::FontRole::Medium, 28);
                  if (ImGui::Button(action->label.c_str(), {width,76})) {
                    (void)queueUiAction({.kind = RuntimeActionKind::UiSpawnActivateAction, .tab = ConfiguratorTab::Spawn, .index = i, .formId = s_spawnBrowser.menuFormId()});
                  } }
                ImGui::PopStyleColor(); ImGui::PopID();
                if (i % 2) ImGui::Dummy({0, 8});
            }
            ImGui::Dummy({0, 20});
            { ScopedFont font(devui::render::FontRole::Body, 16);
              ImGui::TextColored(mutedColor(), "%s", s_spawnBrowser.lastResult().c_str()); }
            ImGui::Dummy({0, 16});
            if (ImGui::Button("< Item list", {160,44})) {
                (void)queueUiAction({.kind = RuntimeActionKind::UiSpawnBack, .tab = ConfiguratorTab::Spawn});
            }
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
                wheel::drawWheelConfig(); return;
            }
            const float railWidth = devui::visual::railWidth(ImGui::GetContentRegionAvail().x);
            const auto tab = s_activeTab.load(std::memory_order_acquire);
            if (tab == ConfiguratorTab::Settings) {
                ImGui::BeginChild("rps-mod-tabs", {0, 64});
                ImGui::SetCursorPos({18, 4});
                bool first = true;
                for (const std::size_t i : {0u, 3u, 1u, 2u}) {
                    if (!s_modSettings[i].available) continue;
                    if (!first) ImGui::SameLine(0, 12);
                    first = false;
                    if (tabChip(kRpsMods[i].name, s_modIndex == i, {156, 52}))
                        (void)queueUiAction({.kind = RuntimeActionKind::UiSelectMod, .index = i});
                }
                if (first) ImGui::TextUnformatted("No supported RPS mods are loaded.");
                ImGui::EndChild();
                if (!activeSettings().available) return;
            }
            // Keep row/scroll/widget identities independent even for identical INI keys.
            ImGui::PushID(tab == ConfiguratorTab::Settings ? static_cast<int>(s_modIndex) : -1);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 18.0f, 24.0f });
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.03f, 0.08f, 0.09f, 0.7f));
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.31f, 0.35f, 0.41f, 0.30f));
            ImGui::BeginChild("rail", { railWidth, 0.0f }, ImGuiChildFlags_AlwaysUseWindowPadding);
            switch (tab) {
            case ConfiguratorTab::Settings:
                drawSettingsRail(activeSettings().store, activeSettings().loaded, activeSettings().activeIndex, tab, s_statusMessage);
                break;
            case ConfiguratorTab::Spawn:
                drawSpawnRail();
                break;
            }
            ImGui::EndChild();
            ImGui::PopStyleColor(2);
            ImGui::SameLine(0,0);
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0,0,0,0));
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.31f, 0.35f, 0.41f, 0.30f));
            ImGui::BeginChild("workspace", { 0.0f, 0.0f }, ImGuiChildFlags_AlwaysUseWindowPadding);
            switch (tab) {
            case ConfiguratorTab::Settings:
                drawSettingsWorkspace(activeSettings().store, activeSettings().loaded, activeSettings().activeIndex, tab, s_statusMessage);
                break;
            case ConfiguratorTab::Spawn:
                drawSpawnWorkspace();
                break;
            }
            ImGui::EndChild();
            ImGui::PopStyleColor(2);
            ImGui::PopStyleVar();
            ImGui::PopID();
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
                return packed(accentColor(0.10f));
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

    bool drawImGui(float x, float y, float width, float height, bool backRequested) noexcept
    {
        std::unique_lock lock(s_runtimeMutex, std::try_to_lock);
        if (!lock.owns_lock() || !s_panelOpen.load(std::memory_order_acquire)) {
            return false;
        }

        try {
            if (s_activeTab.load() == ConfiguratorTab::Settings && !s_pendingNumericEdit &&
                activeSettings().store.needsReload() && !s_settingsRefreshQueued.exchange(true)) {
                if (!queueUiAction({.kind = RuntimeActionKind::UiRefreshSettings})) s_settingsRefreshQueued.store(false);
            }
            ScopedFont configFont(devui::render::FontRole::Body, 26);
            ImGui::SetNextWindowPos(ImVec2(x, y), ImGuiCond_Always);
            if (backRequested) {
                auto& context = *ImGui::GetCurrentContext();
                if (!context.OpenPopupStack.empty()) {
                    ImGui::ClosePopupToLevel(context.OpenPopupStack.Size - 1, true);
                } else if (context.ActiveId != 0) {
                    ImGui::ClearActiveID();
                    s_pendingNumericEdit.reset();
                } else {
                    (void)queueUiAction({ .kind = RuntimeActionKind::UiBack });
                }
            }
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
                packed(ImVec4(0.025f, 0.065f, 0.078f, 1.0f)),
                packed(ImVec4(0.018f, 0.05f, 0.06f, 1.0f)),
                packed(ImVec4(0.025f, 0.06f, 0.07f, 1.0f)));
            drawTopBar();
            constexpr float bodyTop = 94.0f;
            ImGui::SetCursorPos({ 0.0f, bodyTop });
            ImGui::BeginChild(
                "body",
                { windowWidth, windowHeight - bodyTop },
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
        loadRpsSettingsLocked(false);
    }

    bool isOpen() noexcept { return s_panelOpen.load(std::memory_order_acquire); }
    bool isOpening() noexcept { return s_panelOpening.load(std::memory_order_acquire); }
    bool takeInventoryRefreshRequest() noexcept { return s_inventoryRefreshRequested.exchange(false); }

    void setAvailable(bool available) noexcept
    {
        const bool wasAvailable = s_providerInputReady.exchange(available, std::memory_order_acq_rel);
        if (!available && wasAvailable) close();
    }

    void close() noexcept
    {
        s_panelOpening.store(false);
        s_panelOpen.store(false, std::memory_order_release);
        devui::render::SetPanelOpen(false);
        invalidateQueuedRuntimeActions();
    }

    void openAt(const devui::render::PanelPose& wheelPose)
    {
        if (isOpen() || !s_providerInputReady.load(std::memory_order_acquire) || s_panelOpening.exchange(true)) return;
        const float width = s_sessionPanelPhysicalWidth.load(std::memory_order_acquire);
        PanelPose pose;
        pose.position = wheelPose.center;
        pose.right = {wheelPose.right.x, wheelPose.right.y, wheelPose.right.z};
        pose.up = {wheelPose.up.x, wheelPose.up.y, wheelPose.up.z};
        pose.front = {wheelPose.front.x, wheelPose.front.y, wheelPose.front.z};
        pose.physicalWidth = width;
        pose.physicalHeight = width / devui::render::kPanelAspectRatio;
        if (!queueUiAction({.kind = RuntimeActionKind::OpenPanel, .hasPanelPose = true, .panelPose = pose}))s_panelOpening.store(false);
    }

#ifdef WHEEL_DESKTOP_PREVIEW
    void initializePreview(const std::filesystem::path& settingsPath)
    {
        std::array<std::filesystem::path, 3> paths{settingsPath, {}, {}};
        initializeRpsPreview(paths, {true, settingsPath.empty(), settingsPath.empty()});
    }

    void initializeRpsPreview(const std::array<std::filesystem::path, 3>& paths, const std::array<bool, 3>& available,
        const rock::configuration_api::ApiV1* rockApi)
    {
        std::scoped_lock lock(s_runtimeMutex);
        invalidateQueuedRuntimeActions();
        s_modIndex = 0;
        s_activeTab = ConfiguratorTab::Wheel;
        s_modSettings[3] = ModSettings{IniSettingsStore({}, RpsMod::RockDeveloper, rockApi)};
        s_modSettings[3].available = available[0] && (paths[0].empty() || rockApi != nullptr);
        for (std::size_t i = 0; i < paths.size(); ++i) {
            s_modSettings[i] = ModSettings{IniSettingsStore(paths[i], kRpsMods[i].id, i == 0 ? rockApi : nullptr)};
            s_modSettings[i].available = available[i];
        }
        s_providerInputReady.store(true);
        loadRpsSettingsLocked(false); // Read-only snapshot; preview saves are memory-only.
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
