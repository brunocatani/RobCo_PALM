#pragma once

#include "RockSettingControlPolicy.h"
#include "RpsMod.h"
#include <ROCKConfigurationApi.h>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace rock_configurator
{
    enum class SettingType : std::uint8_t
    {
        Boolean,
        Integer,
        Float,
        String
    };

    struct SettingRecord
    {
        std::string id;
        std::string section;
        std::string key;
        std::string value;
        std::string category;
        std::string description;
        std::string defaultValue;
        bool fromRockApi = false;
        bool overridden = false;
        SettingType type{ SettingType::String };
        setting_control::Spec control;
        double numericValue{ 0.0 };
        bool numericValueValid{ false };
        std::optional<std::size_t> selectedOptionIndex;
        std::size_t lineIndex{ 0 };
    };

    struct SettingChangeResult
    {
        bool changed{ false };
        bool saved{ false };
        std::string message;
        SettingRecord setting{};
    };

    class IniSettingsStore
    {
    public:
        explicit IniSettingsStore(std::filesystem::path path = {}, RpsMod mod = RpsMod::Rock,
            const rock::configuration_api::ApiV1* api = nullptr) :
            _path(std::move(path)), _mod(mod), _configurationApi(api),
            _useRockApi(api || (_path.empty() && (mod == RpsMod::Rock || mod == RpsMod::RockDeveloper))) {}

        [[nodiscard]] bool load();
        [[nodiscard]] bool reload();
        [[nodiscard]] bool needsReload() const noexcept;

        [[nodiscard]] const std::vector<SettingRecord>& settings() const noexcept { return _settings; }
        [[nodiscard]] const std::filesystem::path& path() const noexcept { return _path; }
        [[nodiscard]] const std::string& lastError() const noexcept { return _lastError; }

        [[nodiscard]] std::optional<std::size_t> indexForId(std::string_view id) const;
        [[nodiscard]] bool isSlider(std::size_t index) const;
        [[nodiscard]] bool isAdjustable(std::size_t index) const;
        [[nodiscard]] std::size_t moveWithinCategory(std::size_t index, int direction) const;
        [[nodiscard]] std::size_t moveToAdjacentCategory(std::size_t index, int direction) const;
        [[nodiscard]] SettingChangeResult adjustByIndex(std::size_t index, int direction);
        [[nodiscard]] SettingChangeResult activateByIndex(std::size_t index);
        [[nodiscard]] SettingChangeResult setBooleanByIndex(std::size_t index, bool value);
        [[nodiscard]] SettingChangeResult setNumericByIndex(std::size_t index, double value);
        [[nodiscard]] SettingChangeResult setOptionByIndex(std::size_t index, std::size_t optionIndex);

        [[nodiscard]] static std::string trim(std::string_view value);
        [[nodiscard]] static std::string collapseWhitespace(std::string value);

    private:
        struct IniLine
        {
            std::string raw;
            std::string section;
            std::string key;
            std::string value;
            bool isSetting{ false };
        };

        [[nodiscard]] std::filesystem::path resolveProductionIniPath() const;
        [[nodiscard]] static SettingType inferType(std::string_view value);
        void refreshControl(SettingRecord& setting) const;
        [[nodiscard]] bool connectRockApi();
        [[nodiscard]] bool reloadRockSnapshot();
        [[nodiscard]] rock::configuration_api::Group rockGroup() const noexcept;
        [[nodiscard]] static std::string cycleStringValue(const SettingRecord& setting, int direction);
        [[nodiscard]] static std::string adjustNumericValue(const SettingRecord& setting, int direction);

        [[nodiscard]] bool save();
        [[nodiscard]] SettingChangeResult setValueByIndex(std::size_t index, std::string_view value);
        [[nodiscard]] std::pair<std::size_t, std::size_t> categoryBounds(std::size_t index) const;

        std::filesystem::path _path;
        RpsMod _mod;
        const rock::configuration_api::ApiV1* _configurationApi = nullptr;
        bool _useRockApi = false;
        std::uint64_t _loadedRevision = 0;
        std::vector<IniLine> _lines;
        std::vector<SettingRecord> _settings;
        std::string _lastError;
    };
}
