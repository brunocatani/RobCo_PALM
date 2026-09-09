#pragma once

#include "RockSettingControlPolicy.h"

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
        explicit IniSettingsStore(std::filesystem::path path = {}) : _path(std::move(path)) {}

        [[nodiscard]] bool load();
        [[nodiscard]] bool reload();

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

        [[nodiscard]] static std::filesystem::path resolveProductionIniPath();
        [[nodiscard]] static SettingType inferType(std::string_view value);
        static void refreshControl(SettingRecord& setting);
        [[nodiscard]] static std::string cycleStringValue(const SettingRecord& setting, int direction);
        [[nodiscard]] static std::string adjustNumericValue(const SettingRecord& setting, int direction);

        [[nodiscard]] bool save();
        [[nodiscard]] SettingChangeResult setValueByIndex(std::size_t index, std::string_view value);
        [[nodiscard]] std::pair<std::size_t, std::size_t> categoryBounds(std::size_t index) const;

        std::filesystem::path _path;
        std::vector<IniLine> _lines;
        std::vector<SettingRecord> _settings;
        std::string _lastError;
    };
}
