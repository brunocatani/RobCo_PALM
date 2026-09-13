#include "IniSettingsStore.h"

#include <Windows.h>
#include <nlohmann/json.hpp>

#include <array>
#include <cmath>
#include <fstream>
#include <set>
#include <stdexcept>

namespace rock_configurator {
namespace {
using Json = nlohmann::json;

std::string scalar(const Json& value)
{
    if (value.is_string()) {
        auto text = value.get<std::string>();
        if (text.empty() || text.size() > 128 ||
            text.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.-+") != std::string::npos)
            throw std::runtime_error("Invalid CSAH setting value");
        return text;
    }
    if (value.is_boolean() || value.is_number()) return value.dump();
    throw std::runtime_error("Setting value must be a scalar");
}

std::string identifier(const Json& value)
{
    auto text = value.get<std::string>();
    if (text.empty() || text.size() > 96 ||
        text.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-") != std::string::npos)
        throw std::runtime_error("Invalid CSAH setting identifier");
    return text;
}

std::string label(const Json& value)
{
    auto text = value.get<std::string>();
    if (text.empty() || text.size() > 2048 || text.find_first_of("\r\n\t") != std::string::npos || text.find('\0') != std::string::npos)
        throw std::runtime_error("Invalid CSAH setting text");
    return text;
}

std::filesystem::path productionCatalog()
{
    std::array<wchar_t, 32768> executable{};
    const auto size = GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
    if (!size || size >= executable.size()) return {};
    return std::filesystem::path(executable.data()).parent_path() / "Data/F4SE/Plugins/CSAH/PALMSettings.json";
}
}

bool IniSettingsStore::loadCsahSettings()
{
    try {
        auto catalog = _catalogPath;
#ifndef WHEEL_DESKTOP_PREVIEW
        if (catalog.empty()) catalog = productionCatalog();
#endif
        std::error_code error;
        if (catalog.empty() || std::filesystem::file_size(catalog, error) > 1024 * 1024 || error)
            throw std::runtime_error("CSAH's PALM settings catalog is missing or unreadable; install the matching CSAH build");
        if (!std::filesystem::is_regular_file(_path, error) || error)
            throw std::runtime_error("CSAH.ini is unavailable; CSAH must initialize its settings before editing");
        std::ifstream input(catalog);
        const auto menu = Json::parse(input);
        if (menu.at("schemaVersion") != 1 || menu.at("id") != "csah")
            throw std::runtime_error("Unsupported CSAH settings catalog");
        std::vector<SettingRecord> settings;
        std::set<std::string> ids, bindings;
        for (const auto& tab : menu.at("tabs")) {
            for (const auto& page : tab.at("pages")) {
                const auto category = label(tab.at("label")) + " / " + label(page.at("label"));
                for (const auto& group : page.at("groups")) {
                    for (const auto& control : group.at("controls")) {
                        if (settings.size() >= 512) throw std::runtime_error("Too many CSAH settings");
                        SettingRecord setting;
                        const auto& binding = control.at("binding");
                        if (binding.at("type") != "ini" || binding.at("root") != "documents" ||
                            binding.at("path") != "Mods_Config/CSAH/CSAH.ini")
                            throw std::runtime_error("CSAH settings must belong to CSAH.ini");
                        setting.id = identifier(control.at("id"));
                        setting.section = identifier(binding.at("section"));
                        setting.key = identifier(binding.at("key"));
                        if (!ids.insert(setting.id).second || !bindings.insert(setting.section + "." + setting.key).second)
                            throw std::runtime_error("Duplicate CSAH setting or binding");
                        setting.label = label(control.at("label"));
                        setting.category = category;
                        if (control.contains("help")) setting.description = label(control.at("help"));
                        if (group.contains("description")) {
                            if (!setting.description.empty()) setting.description += " ";
                            setting.description += label(group.at("description"));
                        }
                        setting.defaultValue = scalar(control.at("default"));
                        const auto type = control.at("type").get<std::string>();
                        if (type == "toggle") {
                            if (!control.at("default").is_boolean()) throw std::runtime_error("Invalid toggle default");
                            setting.type = SettingType::Boolean;
                            setting.control.kind = setting_control::Kind::Checkbox;
                        } else if (type == "choice") {
                            setting.type = control.at("default").is_string() ? SettingType::String : SettingType::Integer;
                            setting.control.kind = setting_control::Kind::Dropdown;
                            const auto& choices = control.at("choices");
                            if (choices.empty() || choices.size() > 64) throw std::runtime_error("Invalid CSAH choices");
                            std::set<std::string> values;
                            for (const auto& choice : choices) {
                                auto value = scalar(choice.at("value"));
                                if (!values.insert(value).second) throw std::runtime_error("Duplicate CSAH choice");
                                setting.control.options.push_back({std::move(value), label(choice.at("label"))});
                            }
                            if (!values.contains(setting.defaultValue)) throw std::runtime_error("Invalid CSAH choice default");
                        } else if (type == "slider" || type == "number") {
                            setting.type = setting.key.starts_with('i') ? SettingType::Integer : SettingType::Float;
                            auto& spec = setting.control;
                            spec.kind = setting_control::Kind::Numeric;
                            spec.bounded = true;
                            spec.minimum = control.at("min").get<double>();
                            spec.maximum = control.at("max").get<double>();
                            spec.step = control.at("step").get<double>();
                            const auto fallback = control.at("default").get<double>();
                            if (!std::isfinite(spec.minimum) || !std::isfinite(spec.maximum) || !std::isfinite(spec.step) ||
                                !std::isfinite(fallback) || spec.minimum >= spec.maximum || spec.step <= 0 ||
                                fallback < spec.minimum || fallback > spec.maximum)
                                throw std::runtime_error("Invalid CSAH numeric range or default");
                        } else throw std::runtime_error("Unsupported CSAH control type");

                        // Read through the same Windows INI interface as CSAH. Catalog order,
                        // types and labels survive missing keys and integer-looking floats.
                        const std::wstring section(setting.section.begin(), setting.section.end());
                        const std::wstring key(setting.key.begin(), setting.key.end());
                        const std::wstring fallback(setting.defaultValue.begin(), setting.defaultValue.end());
                        std::array<wchar_t, 256> value{};
                        const auto count = GetPrivateProfileStringW(section.c_str(), key.c_str(), fallback.c_str(),
                            value.data(), static_cast<DWORD>(value.size()), _path.c_str());
                        if (count >= value.size() - 1) throw std::runtime_error("CSAH setting value is too long");
                        std::array<char, 1024> utf8{};
                        const auto bytes = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
                            static_cast<int>(count), utf8.data(), static_cast<int>(utf8.size()), nullptr, nullptr);
                        if (count && !bytes) throw std::runtime_error("Invalid CSAH setting encoding");
                        setting.value.assign(utf8.data(), bytes);
                        if (setting.type == SettingType::Boolean) {
                            const auto text = trim(setting.value);
                            if (text == "1" || setting_control::equalsIgnoreCase(text, "true") ||
                                setting_control::equalsIgnoreCase(text, "on") || setting_control::equalsIgnoreCase(text, "yes"))
                                setting.value = "true";
                            else if (text == "0" || setting_control::equalsIgnoreCase(text, "false") ||
                                setting_control::equalsIgnoreCase(text, "off") || setting_control::equalsIgnoreCase(text, "no"))
                                setting.value = "false";
                            else setting.value = setting.defaultValue;
                        }
                        refreshControl(setting);
                        settings.push_back(std::move(setting));
                    }
                }
            }
        }
        if (settings.empty()) throw std::runtime_error("CSAH settings catalog is empty");
        _settings = std::move(settings);
        return true;
    } catch (const std::exception& error) {
        _lastError = std::string("CSAH settings: ") + error.what();
        return false;
    }
}

bool IniSettingsStore::saveCsahSetting(const SettingRecord& setting, std::string_view value)
{
#ifndef WHEEL_DESKTOP_PREVIEW
    std::error_code error;
    if (!std::filesystem::is_regular_file(_path, error) || error) {
        _lastError = "CSAH.ini is unavailable; no settings were written";
        return false;
    }
    const std::wstring section(setting.section.begin(), setting.section.end());
    const std::wstring key(setting.key.begin(), setting.key.end());
    const std::string stored = setting.type == SettingType::Boolean ? (value == "true" ? "1" : "0") : std::string(value);
    const std::wstring text(stored.begin(), stored.end());
    // Change only the selected key, including a missing key. Do not replace an
    // earlier whole-file snapshot over edits made by another settings writer.
    if (!WritePrivateProfileStringW(section.c_str(), key.c_str(), text.c_str(), _path.c_str())) {
        _lastError = "CSAH setting write failed: Windows error " + std::to_string(GetLastError());
        return false;
    }
#endif
    return true;
}
}
