# RobCo PALM

RobCo PALM is the wheel menu for the RPS mods. Its Config opens RPS UI's settings menu, where PALM adds its own pages. Build target and F4SE registration `RobCoPALM`, DLL `wheelmenu.dll`, log `RobCoPALM.log`, settings `PALM.toml` in `Documents/My Games/Fallout4VR/Mods_Config/RobCo_PALM/`.

- Build with `cmake --preset custom-fast`, then `cmake --build build-fast --config Release --target RobCoPALM`. Tests: `cmake --build build-fast --config Release --target build_tests`, then `ctest --test-dir build-fast -C Release --output-on-failure`. Deploys `wheelmenu.dll` and its PDB only to `F4SE/Plugins` under the mod folder set by `COPY_PLUGIN_BASE_PATH` in `CMakeUserPresets.json`.

## Drawing

- RPS UI Framework owns every in-game panel's Dear ImGui context, renderer and frame. The wheel (`src/Renderer.cpp`) adds its fonts, style and icon textures in the panel's setup callback and issues ImGui calls in its draw callback. PALM's pages in the settings menu, Wheel items and Wheel settings (`src/SettingsMenu.cpp`), draw through RPS UI's SettingsMenu page API with the fonts the page frame passes. PALM's Dear ImGui copy is built with RPS UI's shared configuration (`RPS_UI_Framework/ImGuiBuild.h`) and no renderer backend; only the desktop preview, the capture tool and the headless tests add a backend.
- PALM draws with the player's RPS UI theme: `palm::theme` (`src/PalmTheme.h`) reads the theme each callback binds, PALM's pages take the theme's faces from the page frame, and sizes scale with the theme's body size. The desktop preview draws with RPS UI's built-in theme (`support/RPS_UI_Framework/ThemeDefaults.h`) and loads its own faces (`src/Fonts.cpp`).

## Settings menu

- The settings pages of every RPS mod, the Spawner and the Console belong to RPS UI Framework's settings menu (its `src/settings_menu/`). PALM's Config opens that menu at the wheel's pose through the SettingsMenu table and refreshes the wheel's inventory when the menu's `closedCount` changes.
- PALM's own settings are `PALM.toml` through RPS Framework's settings store (`RPS::Settings`, `openPalmSettings` in `src/PalmControls.cpp`); its first open migrates `PALM.ini` once and deletes it.
- Item and gesture integration use ROCK when it is loaded; without it, PALM keeps its standalone RPS UI behaviour. A loaded ROCK that does not serve the ROCK API PALM was built for stops the game at the main menu with an error box (`src/RockServices.cpp`, RPS Framework's `RPS/Runtime/RequiredUpdate.h` from its staged package, `RPSFramework_DIR`).

## Public API

- `include/RobCoPALM/` is the only copy of PALM's public headers: the Menu and Developer interfaces at major 2, with one page per interface in `docs/api/`. They include only the C++ standard library and each other.
- Consumers bind at runtime through `PALMAPI_QueryInterfaceV1`, never link PALM and never copy its headers. `Client.h` is the helper.
- A mod built against PALM 1.2 (`PALMMenuApi.h`, `GetPALMMenuApi`) is updated with `docs/api/Migration.md`; ROCK's `skills/rock-1.0-migration/SKILL.md` covers it with ROCK, PAPER and RPS UI Framework.
- Every version refusal and retired-export call is logged with the calling DLL and stops the game at the main menu with an error box (`src/RequiredUpdates.cpp`). `GetPALMMenuApi` and `GetPALMDeveloperApi` only refuse.
- PALM binds RPS UI Framework from that project's `include/` (`RPSUI_INCLUDE_DIR`). A loaded RPS UI without its 2.0 discovery stops the game the same way.
