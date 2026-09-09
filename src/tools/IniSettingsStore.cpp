#include "IniSettingsStore.h"

#include <ShlObj.h>

#include <charconv>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace rock_configurator
{
    namespace
    {
        [[nodiscard]] bool equalsIgnoreCase(std::string_view left, std::string_view right)
        {
            if (left.size() != right.size()) {
                return false;
            }
            for (std::size_t i = 0; i < left.size(); ++i) {
                char a = left[i];
                char b = right[i];
                if (a >= 'A' && a <= 'Z') {
                    a = static_cast<char>(a - 'A' + 'a');
                }
                if (b >= 'A' && b <= 'Z') {
                    b = static_cast<char>(b - 'A' + 'a');
                }
                if (a != b) {
                    return false;
                }
            }
            return true;
        }

        [[nodiscard]] bool parseInteger(std::string_view value, long long& out)
        {
            const auto trimmed = IniSettingsStore::trim(value);
            if (trimmed.empty()) {
                return false;
            }
            const char* begin = trimmed.data();
            const char* end = begin + trimmed.size();
            auto [ptr, ec] = std::from_chars(begin, end, out);
            return ec == std::errc{} && ptr == end;
        }

        [[nodiscard]] bool parseFloat(std::string_view value, double& out)
        {
            const auto trimmed = IniSettingsStore::trim(value);
            if (trimmed.empty()) {
                return false;
            }
            char* parseEnd = nullptr;
            out = std::strtod(trimmed.c_str(), &parseEnd);
            return parseEnd && *parseEnd == '\0' && std::isfinite(out);
        }

        [[nodiscard]] std::string formatFloat(double value, double step)
        {
            std::ostringstream stream;
            stream << std::fixed << std::setprecision(step < 0.05 ? 3 : (step < 1.0 ? 2 : 1)) << value;
            auto result = stream.str();
            while (result.size() > 3 && result.back() == '0') {
                result.pop_back();
            }
            if (!result.empty() && result.back() == '.') {
                result.push_back('0');
            }
            return result;
        }

        [[nodiscard]] double floatStep(double value)
        {
            const double absValue = std::fabs(value);
            if (absValue < 1.0) {
                return 0.01;
            }
            if (absValue < 10.0) {
                return 0.05;
            }
            if (absValue < 100.0) {
                return 0.5;
            }
            return 1.0;
        }

        [[nodiscard]] std::string commentText(std::string_view raw)
        {
            auto text = IniSettingsStore::trim(raw);
            while (!text.empty() && (text.front() == ';' || text.front() == '=' || text.front() == '-' || text.front() == '#')) {
                text.erase(text.begin());
                text = IniSettingsStore::trim(text);
            }
            while (!text.empty() && (text.back() == '=' || text.back() == '-' || text.back() == '#')) {
                text.pop_back();
                text = IniSettingsStore::trim(text);
            }
            return IniSettingsStore::collapseWhitespace(text);
        }

        [[nodiscard]] bool looksLikeHeading(std::string_view text)
        {
            const auto trimmed = IniSettingsStore::trim(text);
            if (trimmed.empty()) {
                return false;
            }
            if (trimmed.find("---") != std::string::npos || trimmed.find("===") != std::string::npos) {
                return true;
            }

            const auto hasNumberedHeadingPrefix = [](std::string_view candidate) {
                std::size_t cursor = 0;
                while (cursor < candidate.size() && candidate[cursor] >= '0' && candidate[cursor] <= '9') {
                    ++cursor;
                }
                return cursor > 0 && cursor + 1 < candidate.size() && candidate[cursor] == '.' &&
                       (candidate[cursor + 1] == ' ' || candidate[cursor + 1] == '\t');
            };

            if (hasNumberedHeadingPrefix(trimmed)) {
                return true;
            }

            constexpr std::string_view developerPrefix = "Developer ";
            return trimmed.starts_with(developerPrefix) &&
                   hasNumberedHeadingPrefix(trimmed.substr(developerPrefix.size()));
        }

        [[nodiscard]] setting_control::ValueType controlValueType(SettingType type) noexcept
        {
            switch (type) {
            case SettingType::Boolean:
                return setting_control::ValueType::Boolean;
            case SettingType::Integer:
                return setting_control::ValueType::Integer;
            case SettingType::Float:
                return setting_control::ValueType::Float;
            case SettingType::String:
                return setting_control::ValueType::String;
            }
            return setting_control::ValueType::String;
        }

        [[nodiscard]] std::optional<std::string> normalizeValueForSetting(
            const SettingRecord& setting,
            std::string_view rawValue,
            std::string& error)
        {
            const auto trimmed = IniSettingsStore::trim(rawValue);

            if (setting.type == SettingType::Boolean) {
                if (equalsIgnoreCase(trimmed, "true")) {
                    return std::string("true");
                }
                if (equalsIgnoreCase(trimmed, "false")) {
                    return std::string("false");
                }
                error = "boolean values must be true or false";
                return std::nullopt;
            }

            if (setting.control.kind == setting_control::Kind::Dropdown) {
                const auto option = std::find_if(
                    setting.control.options.begin(),
                    setting.control.options.end(),
                    [&](const setting_control::Option& candidate) {
                        return equalsIgnoreCase(candidate.value, trimmed);
                    });
                if (option != setting.control.options.end()) {
                    return option->value;
                }
                error = "value is not in the allowed option list";
                return std::nullopt;
            }

            if (setting.type == SettingType::Integer) {
                long long value = 0;
                if (!parseInteger(trimmed, value)) {
                    error = "integer value is invalid";
                    return std::nullopt;
                }
                if (setting.control.bounded &&
                    (static_cast<double>(value) < setting.control.minimum ||
                     static_cast<double>(value) > setting.control.maximum)) {
                    error = std::format(
                        "integer must be between {:.0f} and {:.0f}",
                        setting.control.minimum,
                        setting.control.maximum);
                    return std::nullopt;
                }
                return std::to_string(value);
            }

            if (setting.type == SettingType::Float) {
                double value = 0.0;
                if (!parseFloat(trimmed, value)) {
                    error = "float value is invalid";
                    return std::nullopt;
                }
                if (setting.control.bounded &&
                    (value < setting.control.minimum || value > setting.control.maximum)) {
                    error = std::format(
                        "float must be between {:.2f} and {:.2f}",
                        setting.control.minimum,
                        setting.control.maximum);
                    return std::nullopt;
                }
                return formatFloat(value, setting.control.step);
            }

            return trimmed;
        }
    }

    void IniSettingsStore::refreshControl(SettingRecord& setting)
    {
        setting.selectedOptionIndex.reset();
        setting.numericValueValid = false;
        setting.type = inferType(setting.value);
        setting.control = setting_control::build(
            controlValueType(setting.type),
            setting.key,
            setting.value,
            setting.description);
        if (setting.type == SettingType::Integer) {
            long long value = 0;
            setting.numericValueValid = parseInteger(setting.value, value);
            setting.numericValue = static_cast<double>(value);
        } else if (setting.type == SettingType::Float) {
            setting.numericValueValid = parseFloat(setting.value, setting.numericValue);
        }
        if (setting.control.kind == setting_control::Kind::Dropdown) {
            const auto selected = std::find_if(
                setting.control.options.begin(),
                setting.control.options.end(),
                [&](const setting_control::Option& option) {
                    return equalsIgnoreCase(option.value, setting.value);
                });
            if (selected != setting.control.options.end()) {
                setting.selectedOptionIndex = static_cast<std::size_t>(
                    std::distance(setting.control.options.begin(), selected));
            }
        }
    }

    bool IniSettingsStore::load()
    {
        if (_path.empty()) {
            _path = resolveProductionIniPath();
        }
        return reload();
    }

    bool IniSettingsStore::reload()
    {
        if (_path.empty()) {
            _path = resolveProductionIniPath();
        }

        _lines.clear();
        _settings.clear();
        _lastError.clear();

        if (_path.empty()) {
            _lastError = "The Documents known folder could not be resolved; ROCK.ini was not accessed";
            return false;
        }

        std::ifstream input(_path);
        if (!input) {
            _lastError = std::format("ROCK.ini is not readable: {}", _path.string());
            return false;
        }

        std::string currentSection;
        std::string currentCategory;
        std::vector<std::string> pendingComments;
        std::string raw;
        while (std::getline(input, raw)) {
            if (!raw.empty() && raw.back() == '\r') {
                raw.pop_back();
            }

            IniLine line{};
            line.raw = raw;
            line.section = currentSection;

            const auto trimmed = trim(raw);
            if (trimmed.empty()) {
                pendingComments.clear();
                _lines.push_back(std::move(line));
                continue;
            }

            if (trimmed.front() == ';') {
                const auto text = commentText(trimmed);
                if (!text.empty()) {
                    if (looksLikeHeading(trimmed) || looksLikeHeading(text)) {
                        currentCategory = text;
                        pendingComments.clear();
                    } else {
                        pendingComments.push_back(text);
                    }
                }
                _lines.push_back(std::move(line));
                continue;
            }

            if (trimmed.front() == '[' && trimmed.back() == ']') {
                currentSection = trim(trimmed.substr(1, trimmed.size() - 2));
                currentCategory = currentSection;
                pendingComments.clear();
                line.section = currentSection;
                _lines.push_back(std::move(line));
                continue;
            }

            const auto equalPos = trimmed.find('=');
            if (equalPos == std::string::npos) {
                pendingComments.clear();
                _lines.push_back(std::move(line));
                continue;
            }

            line.isSetting = true;
            line.section = currentSection;
            line.key = trim(trimmed.substr(0, equalPos));
            line.value = trim(trimmed.substr(equalPos + 1));
            const auto lineIndex = _lines.size();
            _lines.push_back(line);

            std::string description;
            for (const auto& comment : pendingComments) {
                if (comment.empty()) {
                    continue;
                }
                if (!description.empty()) {
                    description += " ";
                }
                description += comment;
            }
            pendingComments.clear();

            SettingRecord setting{};
            setting.section = currentSection;
            setting.key = line.key;
            setting.id = currentSection + "." + line.key;
            setting.value = line.value;
            setting.category = currentCategory.empty() ? currentSection : currentCategory;
            setting.description = description;
            refreshControl(setting);
            setting.lineIndex = lineIndex;
            _settings.push_back(std::move(setting));
        }

        return true;
    }

    std::optional<std::size_t> IniSettingsStore::indexForId(std::string_view id) const
    {
        for (std::size_t i = 0; i < _settings.size(); ++i) {
            if (_settings[i].id == id) {
                return i;
            }
        }
        return std::nullopt;
    }

    bool IniSettingsStore::isSlider(std::size_t index) const
    {
        return index < _settings.size() &&
               _settings[index].control.kind == setting_control::Kind::Numeric;
    }

    bool IniSettingsStore::isAdjustable(std::size_t index) const
    {
        return index < _settings.size() &&
               _settings[index].control.kind != setting_control::Kind::Text;
    }

    std::pair<std::size_t, std::size_t> IniSettingsStore::categoryBounds(std::size_t index) const
    {
        if (_settings.empty()) {
            return { 0, 0 };
        }
        index = (std::min)(index, _settings.size() - 1);
        const auto& category = _settings[index].category;
        std::size_t start = index;
        while (start > 0 && _settings[start - 1].category == category) {
            --start;
        }
        std::size_t end = index;
        while (end + 1 < _settings.size() && _settings[end + 1].category == category) {
            ++end;
        }
        return { start, end };
    }

    std::size_t IniSettingsStore::moveWithinCategory(std::size_t index, int direction) const
    {
        if (_settings.empty()) {
            return 0;
        }
        index = (std::min)(index, _settings.size() - 1);
        const auto [start, end] = categoryBounds(index);
        const auto next = static_cast<long long>(index) + direction;
        const auto clamped = std::clamp(next, static_cast<long long>(start), static_cast<long long>(end));
        return static_cast<std::size_t>(clamped);
    }

    std::size_t IniSettingsStore::moveToAdjacentCategory(std::size_t index, int direction) const
    {
        if (_settings.empty()) {
            return 0;
        }
        index = (std::min)(index, _settings.size() - 1);
        const auto [start, end] = categoryBounds(index);
        if (direction < 0) {
            if (start == 0) {
                return start;
            }
            return categoryBounds(start - 1).first;
        }
        if (end + 1 >= _settings.size()) {
            return start;
        }
        return end + 1;
    }

    SettingChangeResult IniSettingsStore::adjustByIndex(std::size_t index, int direction)
    {
        if (index >= _settings.size()) {
            return { .message = "selected setting is out of range" };
        }

        const auto& setting = _settings[index];
        std::string next;
        if (setting.control.kind == setting_control::Kind::Dropdown) {
            next = cycleStringValue(setting, direction == 0 ? 1 : direction);
        } else if (setting.type == SettingType::Boolean) {
            next = equalsIgnoreCase(setting.value, "true") ? "false" : "true";
        } else if (setting.type == SettingType::Integer || setting.type == SettingType::Float) {
            next = adjustNumericValue(setting, direction == 0 ? 1 : direction);
        } else {
            return {
                .changed = false,
                .saved = false,
                .message = "text value needs direct edit mode",
                .setting = setting,
            };
        }

        if (next == setting.value) {
            return {
                .changed = false,
                .saved = false,
                .message = "value needs direct edit mode",
                .setting = setting,
            };
        }

        return setValueByIndex(index, next);
    }

    SettingChangeResult IniSettingsStore::activateByIndex(std::size_t index)
    {
        if (index >= _settings.size()) {
            return { .message = "selected setting is out of range" };
        }
        if (_settings[index].type == SettingType::Boolean) {
            return adjustByIndex(index, 1);
        }
        return {
            .changed = false,
            .saved = false,
            .message = "use left/right to adjust this setting",
            .setting = _settings[index],
        };
    }

    SettingChangeResult IniSettingsStore::setBooleanByIndex(std::size_t index, bool value)
    {
        if (index >= _settings.size()) {
            return { .message = "selected setting is out of range" };
        }
        if (_settings[index].control.kind != setting_control::Kind::Checkbox) {
            return {
                .message = "selected setting is not a checkbox",
                .setting = _settings[index],
            };
        }
        return setValueByIndex(index, value ? "true" : "false");
    }

    SettingChangeResult IniSettingsStore::setNumericByIndex(std::size_t index, double value)
    {
        if (index >= _settings.size()) {
            return { .message = "selected setting is out of range" };
        }
        const auto& setting = _settings[index];
        if (setting.control.kind != setting_control::Kind::Numeric ||
            !std::isfinite(value)) {
            return {
                .message = "selected setting is not a finite numeric control",
                .setting = setting,
            };
        }
        value = setting_control::snapNumeric(setting.control, value);
        const auto text = setting.type == SettingType::Integer ?
            std::to_string(static_cast<long long>(std::llround(value))) :
            formatFloat(value, setting.control.step);
        return setValueByIndex(index, text);
    }

    SettingChangeResult IniSettingsStore::setOptionByIndex(
        std::size_t index,
        std::size_t optionIndex)
    {
        if (index >= _settings.size()) {
            return { .message = "selected setting is out of range" };
        }
        const auto& setting = _settings[index];
        if (setting.control.kind != setting_control::Kind::Dropdown ||
            optionIndex >= setting.control.options.size()) {
            return {
                .message = "selected dropdown option is out of range",
                .setting = setting,
            };
        }
        return setValueByIndex(index, setting.control.options[optionIndex].value);
    }

    std::filesystem::path IniSettingsStore::resolveProductionIniPath()
    {
        PWSTR documents = nullptr;
        if (FAILED(SHGetKnownFolderPath(FOLDERID_Documents, KF_FLAG_DEFAULT, nullptr, &documents)) ||
            !documents) {
            return {};
        }
        std::filesystem::path path(documents);
        CoTaskMemFree(documents);
        path /= "My Games";
        path /= "Fallout4VR";
        path /= "ROCK_Config";
        path /= "ROCK.ini";
        return path;
    }

    std::string IniSettingsStore::trim(std::string_view value)
    {
        std::size_t begin = 0;
        while (begin < value.size() && std::isspace(static_cast<unsigned char>(value[begin])) != 0) {
            ++begin;
        }
        std::size_t end = value.size();
        while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
            --end;
        }
        return std::string(value.substr(begin, end - begin));
    }

    std::string IniSettingsStore::collapseWhitespace(std::string value)
    {
        std::string out;
        out.reserve(value.size());
        bool lastSpace = false;
        for (const auto ch : value) {
            if (std::isspace(static_cast<unsigned char>(ch)) != 0) {
                if (!lastSpace) {
                    out.push_back(' ');
                    lastSpace = true;
                }
            } else {
                out.push_back(ch);
                lastSpace = false;
            }
        }
        return trim(out);
    }

    SettingType IniSettingsStore::inferType(std::string_view value)
    {
        if (equalsIgnoreCase(value, "true") || equalsIgnoreCase(value, "false")) {
            return SettingType::Boolean;
        }
        long long integerValue = 0;
        if (parseInteger(value, integerValue)) {
            return SettingType::Integer;
        }
        double floatValue = 0.0;
        if (parseFloat(value, floatValue)) {
            return SettingType::Float;
        }
        return SettingType::String;
    }

    std::string IniSettingsStore::cycleStringValue(const SettingRecord& setting, int direction)
    {
        if (setting.control.kind != setting_control::Kind::Dropdown ||
            setting.control.options.empty()) {
            return setting.value;
        }

        const auto& options = setting.control.options;
        auto it = std::find_if(options.begin(), options.end(), [&](const setting_control::Option& option) {
            return equalsIgnoreCase(option.value, setting.value);
        });
        const int current = it == options.end() ? 0 :
            static_cast<int>(std::distance(options.begin(), it));
        const int count = static_cast<int>(options.size());
        int next = (current + (direction < 0 ? -1 : 1)) % count;
        if (next < 0) {
            next += count;
        }
        return options[static_cast<std::size_t>(next)].value;
    }

    std::string IniSettingsStore::adjustNumericValue(const SettingRecord& setting, int direction)
    {
        if (setting.control.kind != setting_control::Kind::Numeric) {
            return setting.value;
        }

        if (setting.type == SettingType::Integer) {
            long long value = 0;
            if (!parseInteger(setting.value, value)) {
                return setting.value;
            }
            const auto step = static_cast<long long>((std::max)(1.0, setting.control.step));
            value += direction < 0 ? -step : step;
            if (setting.control.bounded) {
                value = (std::max)(
                    static_cast<long long>(setting.control.minimum),
                    (std::min)(static_cast<long long>(setting.control.maximum), value));
            }
            return std::to_string(value);
        }

        double value = 0.0;
        if (!parseFloat(setting.value, value)) {
            return setting.value;
        }
        const double step = setting.control.step > 0.0 ?
            setting.control.step : floatStep(value);
        value += direction < 0 ? -step : step;
        if (setting.control.bounded) {
            value = std::clamp(value, setting.control.minimum, setting.control.maximum);
        }
        return formatFloat(value, step);
    }

    bool IniSettingsStore::save()
    {
#ifdef WHEEL_DESKTOP_PREVIEW
        return true; // The standalone preview must never write production settings.
#endif
        std::error_code ec;
        if (_path.empty() || !std::filesystem::is_regular_file(_path, ec) || ec) {
            _lastError = "The existing production ROCK.ini is unavailable; Wheel Config will not create or replace it";
            return false;
        }

        const auto tempPath = _path.string() + ".tmp";
        std::ofstream output(tempPath, std::ios::trunc);
        if (!output) {
            _lastError = std::format("ROCK.ini is not writable: {}", _path.string());
            return false;
        }
        for (const auto& line : _lines) {
            output << line.raw << "\n";
        }
        output.close();
        if (!output) {
            _lastError = std::format("ROCK.ini temp write failed: {}", tempPath);
            std::filesystem::remove(tempPath, ec);
            return false;
        }

        if (!MoveFileExA(tempPath.c_str(), _path.string().c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            _lastError = std::format("ROCK.ini replace failed: Windows error {}", GetLastError());
            std::filesystem::remove(tempPath, ec);
            return false;
        }
        return true;
    }

    SettingChangeResult IniSettingsStore::setValueByIndex(std::size_t index, std::string_view value)
    {
        if (index >= _settings.size()) {
            return { .message = "selected setting is out of range" };
        }
        auto& setting = _settings[index];
        std::string validationError;
        const auto normalizedValue = normalizeValueForSetting(setting, value, validationError);
        if (!normalizedValue) {
            return {
                .changed = false,
                .saved = false,
                .message = validationError,
                .setting = setting,
            };
        }

        if (setting.value == *normalizedValue) {
            return {
                .changed = false,
                .saved = true,
                .message = "value unchanged",
                .setting = setting,
            };
        }
        if (setting.lineIndex >= _lines.size()) {
            return {
                .changed = false,
                .saved = false,
                .message = "setting line is invalid",
                .setting = setting,
            };
        }

        auto& line = _lines[setting.lineIndex];
        const auto previousLine = line;
        const auto previousSetting = setting;
        const std::string nextValue(*normalizedValue);
        line.value = nextValue;
        line.raw = std::format("{} = {}", line.key, nextValue);
        setting.value = nextValue;
        refreshControl(setting);

        const bool saved = save();
        if (!saved) {
            line = previousLine;
            setting = previousSetting;
        }
        return {
            .changed = true,
            .saved = saved,
            .message = saved ? "saved" : _lastError,
            .setting = setting,
        };
    }
}
