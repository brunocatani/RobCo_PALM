#include "IniSettingsStore.h"
#include <Windows.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace rock_configurator;
using Json = nlohmann::json;
namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
std::string read(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), {}};
}
Json fixture() {
    return Json::parse(R"({"schemaVersion":1,"id":"csah","tabs":[{"label":"Features","pages":[
      {"label":"Lighting","groups":[{"controls":[
        {"id":"enabled","type":"toggle","label":"Visual suite","default":false,
         "binding":{"type":"ini","root":"documents","path":"Mods_Config/CSAH/CSAH.ini","section":"CommunityShaders","key":"bEnabled"}},
        {"id":"strength","type":"slider","label":"Strength","default":0.5,"min":0,"max":2,"step":0.05,
         "binding":{"type":"ini","root":"documents","path":"Mods_Config/CSAH/CSAH.ini","section":"Lighting","key":"fStrength"}},
        {"id":"quality","type":"choice","label":"Quality","help":"Restart after changing quality.","default":2,
         "choices":[{"label":"Low","value":0},{"label":"High","value":2}],
         "binding":{"type":"ini","root":"documents","path":"Mods_Config/CSAH/CSAH.ini","section":"Lighting","key":"iQuality"}}
      ]}]}]}]})");
}
}

int main(int argc, char** argv) {
    const auto directory = std::filesystem::temp_directory_path() / ("PalmCsah-" + std::to_string(GetCurrentProcessId()));
    std::filesystem::create_directory(directory);
    const auto ini = directory / "CSAH.ini";
    const auto catalog = directory / "PALMSettings.json";
    struct Cleanup {
        std::filesystem::path ini, catalog, directory;
        ~Cleanup() { std::error_code error; std::filesystem::remove(ini, error); std::filesystem::remove(catalog, error); std::filesystem::remove(directory, error); }
    } cleanup{ini, catalog, directory};
    try {
        { std::ofstream file(ini); file << "; keep comment\n[CommunityShaders]\nbEnabled=1\n[Lighting]\nfStrength=1\n[Unrelated]\nother=keep\n"; }
        const auto original = read(ini);
        auto menu = fixture();
        const auto writeCatalog = [&] { std::ofstream file(catalog); file << menu; };
        writeCatalog();
        IniSettingsStore store(ini, RpsMod::Csah, nullptr, catalog);
        require(store.load(), store.lastError().c_str());
        require(store.settings().size() == 3, "catalog omitted missing INI keys or exposed unrelated keys");
        require(store.settings()[0].type == SettingType::Boolean && store.settings()[0].value == "true", "0/1 switch became a numeric field");
        require(store.settings()[1].type == SettingType::Float, "integer-looking float lost slider precision");
        require(store.settings()[2].selectedOptionIndex == 1, "missing choice did not use the authored default");
        require(read(ini) == original, "loading settings modified the INI");

        // An external writer changes another key after our snapshot.
        require(WritePrivateProfileStringW(L"Unrelated", L"other", L"external", ini.c_str()), "could not update external fixture");
        const auto beforeWrites = read(ini);
        require(store.setBooleanByIndex(0, false).saved, "boolean write failed");
        require(store.setNumericByIndex(1, 1.15).saved && std::abs(store.settings()[1].numericValue - 1.15) < 0.001, "float precision was lost");
        require(store.setOptionByIndex(2, 0).saved, "missing choice key could not be written");
        require(!store.setNumericByIndex(1, std::numeric_limits<double>::quiet_NaN()).saved, "NaN was accepted");
        require(!store.setOptionByIndex(2, 3).saved, "invalid choice was accepted");
#ifdef WHEEL_DESKTOP_PREVIEW
        require(read(ini) == beforeWrites, "preview wrote the INI");
#else
        require(store.reload(), store.lastError().c_str());
        require(store.settings()[0].value == "false" && store.settings()[2].value == "0", "saved values did not round-trip");
        const auto contents = read(ini);
        require(contents.find("other=external") != std::string::npos && contents.find("; keep comment") != std::string::npos,
            "single-key write overwrote external changes or comments");
#endif
        menu["tabs"][0]["pages"][0]["groups"][0]["controls"][1]["binding"]["path"] = "../other.ini";
        writeCatalog();
        require(!store.reload() && store.settings().empty(), "foreign binding was accepted");
        menu = fixture();
        menu["tabs"][0]["pages"][0]["groups"][0]["controls"].push_back(menu["tabs"][0]["pages"][0]["groups"][0]["controls"][0]);
        writeCatalog();
        require(!store.reload() && store.settings().empty(), "duplicate setting was accepted");
        menu = fixture(); menu["tabs"][0]["pages"][0]["groups"][0]["controls"][1]["step"] = 0;
        writeCatalog(); require(!store.reload(), "zero-step slider was accepted");
        if (argc > 1) {
            IniSettingsStore authored(ini, RpsMod::Csah, nullptr, std::filesystem::path(argv[1]));
            require(authored.load(), authored.lastError().c_str());
            require(authored.settings().size() == 141, "production CSAH catalog lost controls");
            std::cout << "Validated all " << authored.settings().size() << " authored CSAH controls\n";
        }
        std::filesystem::remove(ini);
        menu = fixture(); writeCatalog();
        require(!store.reload(), "missing INI was created by the menu");
        std::cout << "CSAH settings bridge passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
