# RobCo PALM

RobCo PALM is the wheel menu and the configuration pages for the RPS mods. PALM's runtime identity is defined in the UI workspace instructions.

- Build with `cmake --preset custom-fast`, then `cmake --build build-fast --config Release --target RobCoPALM`. Targeted CTest suites also use `build-fast`, Release. Deploys `wheelmenu.dll` and its PDB only to `F4SE/Plugins` under the mod folder set by `COPY_PLUGIN_BASE_PATH` in `CMakeUserPresets.json`.

## Provider pages

- Runtime availability comes from the loaded modules (`src/tools/RpsMod.h`). PALM never changes a provider's published contract.
- ROCK's consumer and developer pages share one configuration registration: read the catalog from ROCK's negotiated Configuration API and route writes to ROCK. PAPER uses its INI adapter and watcher behaviour.
- Item and gesture integration use ROCK when it is loaded; without it, PALM keeps its standalone RPS UI behaviour. A loaded ROCK that does not serve the ROCK API PALM was built for stops the game at the main menu with an error box (`src/RockServices.cpp`, RPS Framework's `RPS/Runtime/RequiredUpdate.h` through `RPS_FRAMEWORK_INCLUDE_DIR`).
- SCISSORS pages (consumer and developer) bind only `SCISSORS.dll`'s `SCISSORS_GetConfiguration` 2.0 table, from SCISSORS' own `include/` (`SCISSORS_INCLUDE_DIR`). Never forward NPC settings through ROCK or write SCISSORS INIs directly. `bPhysicalMelee` lives on the SCISSORS Dev page and goes through SCISSORS Configuration, never ROCK configuration.
- Preview fixture flags never alter production discovery or write runtime INIs; preview never discovers or writes a real provider, and injected fixtures stay read-only.

## Console tab

- `src/tools/ConsoleService.cpp` hooks the call to `ConsoleLog::AddString` at plugin load, after checking the live bytes, and runs commands through `Console::ExecuteCommand`, so the native Console menu is never needed. Commands containing `%` are refused: the engine echoes a command through its print function as the format string.
- The on-screen keyboard is its own RPS UI panel (`robco.palm.workshop.keyboard`, `src/tools/OnScreenKeyboard.cpp`) under the Config panel. It is up on the Console tab, and on the Spawner while one of its two searches (sources, items) is open; it takes the Config panel's width and follows its resizes. Both keyboards feed a `ConsoleLine`, not an ImGui text field, because clicking a key would take focus from that field. The real keyboard comes from RPS UI's `captureKeyboard`, requested only while the keyboard panel is up; the capture lapses by itself when PALM stops asking.
- Shortcut buttons come from `Mods_Config/RobCo_PALM/ConsoleCommands.ini` (`src/tools/ConsoleShortcuts.h` parses it; `console::loadShortcuts` reads it and writes the default list when absent). The game thread reads it only on entering the Console tab and on Reload, outside the runtime mutex; never per frame. The preview build uses the default list and touches no file.

## Spawner

- With `Flying_BRICKS.esp` loaded, the All ammo source lists only the playable rounds that plugin defines or overrides (`src/tools/SpawnBrowser.cpp`), so vanilla's non-playable companion copies it also overrides stay out; otherwise every named ammo form.

## Public API

- `include/RobCoPALM/` is the only copy of PALM's public headers: the Menu and Developer interfaces at major 2, with one page per interface in `docs/api/`, updated in the same commit as the header. They include only the C++ standard library and each other.
- Consumers bind at runtime through `PALMAPI_QueryInterfaceV1`, never link PALM and never copy its headers. `Client.h` is the helper.
- Every version refusal and retired-export call is logged with the calling DLL and stops the game at the main menu with an error box (`src/RequiredUpdates.cpp`). `GetPALMMenuApi` and `GetPALMDeveloperApi` only refuse; their names are never reused.
- PALM binds RPS UI Framework from that project's `include/` (`RPSUI_INCLUDE_DIR`). A loaded RPS UI without its 2.0 discovery stops the game the same way.
