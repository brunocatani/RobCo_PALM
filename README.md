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

## Requirements

Fallout 4 VR, F4SEVR, and RPS UI Framework.

ROCK adds physical item handling; hand gestures also need FRIK 0.79 / API 2.3 for current source. PAPER and SCISSORS are optional integrations for their settings pages.

## Using PALM

**Mode 1 (default):** click and release the physical right analog stick to open the wheel, point, and click with the pointing hand's trigger or A/X to select. Click **Cancel** to close.

**Mode 2:** hold **right trigger + grab** for **0.50 seconds** to open the wheel, point, and release to select. Aim at the center/Cancel area and release to cancel.

Choose your items in **Config → Items**. Select either preset or **Mode 3 — Custom** in **Config → PALM settings → Controls**. Custom mode unlocks remapping and the choice between **Release to select** and **Click to select**; its binding is retained when you switch back to a preset.

In **Click to select**, release the opening buttons before selecting with the pointing hand's trigger or A/X. The wheel stays open until you click **Cancel** or enter Config. Opening bindings support press, tap, double tap, hold, long press, and release, with an optional modifier on either hand. Touch and axis inputs are excluded; thumbstick click is supported. Tap and release bindings use click-to-select.

PALM saves its controls and pointer calibration to `Documents\My Games\Fallout4VR\Mods_Config\RobCo_PALM\PALM.ini`; manual edits are read at startup.

New configurations use `Controls/iInputMode = 1`. Existing files without that key retain their previous binding by selecting the matching preset or Custom. Presets do not use `sMode` or `sOpenMenu` as their binding; those keys store Custom's settings.

ROCK's trigger-equip, Rocky gesture, and bipod clicks take priority over opening PALM. A bound hand inside a Virtual Holsters zone or unrelated held buttons can also block opening. Release the opening buttons fully before trying again.

Install the `.7z` asset from [GitHub Releases](https://github.com/brunocatani/RobCo_PALM/releases/latest) through your mod manager. The plugin remains `Data/F4SE/Plugins/wheelmenu.dll`; GitHub's automatic source archives are for development.

## Adding a mod section

The public headers are in [`SDK/include/PALMMenuApi.h`](SDK/include/PALMMenuApi.h) and [`PALMIcons.h`](SDK/include/PALMIcons.h). Resolve `GetPALMMenuApi` from `wheelmenu.dll` and request API version 1. The [`physical magazine example`](SDK/examples/PhysicalMagazines.cpp) compiles as part of the local build.

A mod registers a unique section ID, display name, icon, and selection callback, then publishes a copied snapshot of up to eight items. PALM supports up to ten registered mod sections. New mod sections start hidden until enabled in PALM settings.

Selections run on the F4SE game task thread. Hold mode closes the wheel before dispatch; press mode keeps it open. Your mod owns the action and its vanilla or ROCK integration; a magazine entry can represent your mod's physical object rather than an ammo form. Respect the owning API's thread rules when performing the action. PALM's item snapshots are cleared on a game load, and mods republish for the new session. Unregister successfully before freeing callback state; `CallbackBusy` requires a later retry.

## Local development

From this repository, run:

```powershell
cmake --preset custom-fast
cmake --build build-fast --config Release --parallel 2 -- /p:CL_MPCount=2 /nodeReuse:false
ctest --test-dir build-fast -C Release --output-on-failure --parallel 2
```

The preset automatically deploys the DLL and PDB to `D:/FO4/mods/RobCo_PALM/F4SE/Plugins/`. `PALMPreview.exe` provides a desktop preview; `PALMCapture.exe` renders screenshots without opening a window. Desktop previews do not validate VR input or stereo presentation.

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
