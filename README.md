# RobCo P.A.L.M. — Fallout 4 VR Wheel Menu

**Personal Access & Loadout Manager**

RobCo P.A.L.M. is a customisable wheel menu for Fallout 4 VR, with quick access to your equipment, consumables, hand gestures, and mod settings.

## Features

- Choose up to eight items per category and hide sections you do not use.
- Equip weapons and armor, use consumables, or bring items into your hand with ROCK.
- Use eight hand gestures with ROCK and FRIK.
- Change PALM controls and RPS mod settings in game.
- Add wheel sections through supported mods.
- Browse and spawn items by plugin and category.
- Read the game console and run commands in game, with an on-screen keyboard or your real keyboard.

## Requirements

Fallout 4 VR, F4SEVR, and RPS UI Framework.

ROCK 1.0 adds physical item handling; hand gestures also need FRIK 0.79.1 or newer with FRIK API 2.3. PAPER 2.0 and SCISSORS 1.0 are optional integrations for their settings pages.

## Using PALM

**Mode 1 (default):** click and release the physical right analog stick to open the wheel, point, and click with the pointing hand's trigger or A/X to select. Click **Cancel** to close.

**Mode 2:** hold **right trigger + grab** for **0.50 seconds** to open the wheel, point, and release to select. Aim at the center/Cancel area and release to cancel.

Choose your items in **Config → Items**. Select either preset or **Mode 3 — Custom** in **Config → PALM settings → Controls**. Custom mode unlocks remapping and the choice between **Release to select** and **Click to select**; its binding is retained when you switch back to a preset.

In **Click to select**, release the opening buttons before selecting with the pointing hand's trigger or A/X. The wheel stays open until you click **Cancel** or enter Config. Opening bindings support press, tap, double tap, hold, long press, and release, with an optional modifier on either hand. Touch and axis inputs are excluded; thumbstick click is supported. Tap and release bindings use click-to-select.

PALM saves its controls and pointer calibration to `Documents\My Games\Fallout4VR\Mods_Config\RobCo_PALM\PALM.toml`; manual edits are read at startup. Each option has its help above it, and `[Developer] Enabled` adds the Developer section to the wheel after a restart.

New configurations use `[Controls] InputMode = 1`. Presets do not use `Mode` or `OpenMenu` as their binding; those keys store Custom's settings. The first start after an update carries your `PALM.ini` settings into `PALM.toml` and deletes the INI; an INI from before input modes keeps its binding through the matching preset or Custom.

**Config → Console** shows everything the game console prints and runs commands as if typed into it. Point and click the on-screen keyboard, or type on your real keyboard while the game window on your PC has focus; while this tab is open, key presses go to the command line instead of the game. Enter runs the line, Up and Down recall earlier commands, Page Up and Page Down scroll, and Escape clears the line. Commands containing `%` are refused, because the game reads them as a format string and can crash. Commands without a target act on the selected reference shown above the log, which you set with `prid <id>`.

The buttons above the log are shortcuts from `Documents\My Games\Fallout4VR\Mods_Config\RobCo_PALM\ConsoleCommands.ini`. Each `[Label]` section is one button, and the lines under it run in order when you press it. PALM creates the file with a few examples the first time, and reads it again each time you open the Console tab or press Reload.

ROCK's trigger-equip and bipod clicks, and an object placement on the same hand, take priority over opening PALM. A bound hand inside a Virtual Holsters zone or unrelated held buttons can also block opening. Release the opening buttons fully before trying again.

Install the `.zip` asset from [GitHub Releases](https://github.com/brunocatani/RobCo_PALM/releases/latest) through your mod manager. The plugin remains `Data/F4SE/Plugins/wheelmenu.dll`; GitHub's automatic source archives are for development.

## Adding a mod section

The public headers are in [`include/RobCoPALM/`](include/RobCoPALM/), documented in [`docs/api/Menu.md`](docs/api/Menu.md). Resolve `PALMAPI_QueryInterfaceV1` from `wheelmenu.dll` and acquire the Menu table with `palm::api::acquire`. The [`physical magazine example`](tests/ConsumerExample.cpp) compiles with the tests.

A mod registers a unique section ID, display name, icon, and selection callback, then publishes a copied snapshot of up to eight items. PALM supports up to ten registered mod sections. New mod sections start hidden until enabled in PALM settings.

Selections run on the F4SE game task thread. Hold mode closes the wheel before dispatch; press mode keeps it open. Your mod owns the action and its vanilla or ROCK integration; a magazine entry can represent your mod's physical object rather than an ammo form. Respect the owning API's thread rules when performing the action. PALM's item snapshots are cleared on a game load, and mods republish for the new session. Unregister successfully before freeing callback state; `CallbackBusy` requires a later retry.

## Local development

From this repository, run:

```powershell
cmake --preset custom-fast
cmake --build build-fast --config Release
cmake --build build-fast --config Release --target build_tests
ctest --test-dir build-fast -C Release --output-on-failure
```

The preset deploys the DLL and PDB to `F4SE/Plugins` under the mod folder set by `COPY_PLUGIN_BASE_PATH` in your `CMakeUserPresets.json`. `PALMPreview.exe` provides a desktop preview; `PALMCapture.exe` renders screenshots without opening a window. Desktop previews do not validate VR input or stereo presentation.

## Source code and documentation

- [GitHub repository and source code](https://github.com/brunocatani/RobCo_PALM)
- [DevArtificial documentation](https://devartificial.pro/docs/rps-stack/wheel-menu)
- [RPS UI Framework](https://github.com/brunocatani/RPS_UI_Framework)

## Credits

- [CommonLibF4VR](https://github.com/ArthurHub/CommonLibF4VR) and its contributors.
- [F4VR-CommonFramework](https://github.com/ArthurHub/F4VR-CommonFramework) for the controller binding parser and value types.
- [Omar Cornut](https://github.com/ocornut) and the [Dear ImGui](https://github.com/ocornut/imgui) contributors.
- [Arthur](https://github.com/ArthurHub), [RollingRock](https://github.com/rollingrock), and the FRIK contributors for FRIK and its public hand-pose API.

## License

Original RobCo PALM code is licensed under GNU General Public License version 3
or later (`GPL-3.0-or-later`). See [COPYRIGHT](COPYRIGHT) and [LICENSE](LICENSE).
Third-party components retain their respective licenses; their notices are in
[THIRD_PARTY_NOTICES.txt](THIRD_PARTY_NOTICES.txt).

## Current ROCK integration

Optional ROCK integration negotiates feature interfaces through `ROCKAPI_QueryInterfaceV1`. Configuration binds its own Read/Write permissions and uses ROCK's compiled catalog and writer; there is no separate configuration discovery export. The section API remains PALM-owned and independent of ROCK and RPS UI tables.

SCISSORS has separate consumer and developer settings pages. NPC collision,
blade penetration and native/supplemental NPC visualization controls come from
SCISSORS' own Configuration 2.0 API and are written by SCISSORS into its own settings files.
No NPC setting is forwarded through ROCK, and the NPC visualization master is
independent of ROCK's hand and weapon visualization master.
