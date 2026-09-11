# RobCo P.A.L.M. — Fallout 4 VR Wheel Menu

**Personal Access & Loadout Manager**

RobCo P.A.L.M. is a wheel menu for Fallout 4 VR. Hold B, point at an item, and release to select it. It brings weapons, armor, consumables, hand gestures, and configuration into a green RobCo-style radial interface.

## Features

- **Five item categories:** Weapons, Armor, Aid, Food, and Grenades, with up to eight chosen items in each category.
- **Weapons first:** The wheel opens on Weapons by default.
- **Equipment selection:** Equip a weapon or armor piece, or select equipped gear again to unequip it.
- **Physical item access:** Bring one aid item, food item, or grenade from your inventory into a free hand for use with ROCK.
- **Eight hand gestures:** Thumbs up, middle finger, rock and roll, peace, pointing, fist, open hand, and shaka. Choose the left or right hand from the inner ring. A busy hand shows a warning; a pose clears when that hand starts another action or interaction.
- **Fallout-specific outline icons:** Distinct icons for weapon types, equipment, consumables, and gestures.
- **PALM settings:** Show or hide Aid, Food, Grenades, Weapons, Armor, and Gestures. Visible sections redistribute around the inner ring without empty gaps.
- **In-game configuration:** Choose wheel items and access ROCK, ROCK developer, PAPER, and SCISSORS settings.
- **Mod sections:** Other native mods can add named sections and up to eight items per section through PALM's API. Enable those sections in PALM settings.
- **Item spawner:** Browse by plugin and category and add items to your inventory.

## Requirements

- Fallout 4 VR and F4SEVR.
- ROCK and RPS UI Framework, together with their requirements.
- FRIK v78.2 or later for the custom hand-pose interface.

PAPER and SCISSORS are optional integrations for their corresponding configuration pages.

## Controls

1. Hold the physical right controller's **B** button for approximately 0.25 seconds.
2. Point at a category in the inner ring or an item in the outer ring.
3. Release **B** to choose the highlighted item. Point at the center to cancel; **CANCEL** beneath the **P.A.L.M** branding grows when the center is highlighted.

Choose **Config** to set up your wheel items. The **Left Gestures** and **Right Gestures** entries choose which hand receives a pose. Selecting the active gesture again clears it.

The **Items** tab chooses inventory entries. **PALM settings** controls visible sections. The inner ring supports up to **10 entries**, including Config; Gestures occupies two entries. Hiding sections keeps their item choices, and Config and Cancel always remain accessible.

A short B tap keeps its native action. While PALM is available, it replaces ROCK's grenade quick draw; grenades remain accessible through the wheel.

## Plugin and configuration

The plugin is **`F4SE/Plugins/wheelmenu.dll`**. Its registration name is `RobCoPALM` and its log is `RobCoPALM.log` in the F4SE log directory.

The configuration pages edit the owning mods' settings under `Documents\My Games\Fallout4VR\Mods_Config\`: `ROCK\ROCK.ini`, `ROCK\ROCK_Developer.ini`, `PAPER\PAPER.ini`, and `SCISSORS\SCISSORS.ini`. Changes follow each mod's own apply and reload rules.

## Adding a mod section

The public headers are in [`SDK/include/PALMMenuApi.h`](SDK/include/PALMMenuApi.h) and [`PALMIcons.h`](SDK/include/PALMIcons.h). Resolve `GetPALMMenuApi` from `wheelmenu.dll` and request API version 1. The [`physical magazine example`](SDK/examples/PhysicalMagazines.cpp) compiles as part of the local build.

A mod registers a unique section ID, display name, icon, and selection callback, then publishes a copied snapshot of up to eight items. PALM supports up to ten registered mod sections. New mod sections start hidden until enabled in PALM settings.

Selections run on the F4SE game task thread after the wheel closes. Your mod owns the action and its vanilla or ROCK integration; a magazine entry can represent your mod's physical object rather than an ammo form. Respect the owning API's thread rules when performing the action. PALM's item snapshots are cleared on a game load, and mods republish for the new session. Unregister successfully before freeing callback state; `CallbackBusy` requires a later retry.

## Local development

From this repository, run:

```powershell
cmake --preset custom-fast
cmake --build build-fast --config Release --parallel 2 -- /p:CL_MPCount=2 /nodeReuse:false
ctest --test-dir build-fast -C Release --output-on-failure --parallel 2
```

The preset automatically deploys the DLL and PDB to `D:/FO4/mods/RobCo_PALM/F4SE/Plugins/`. `PALMPreview.exe` provides a desktop preview; `PALMCapture.exe` renders screenshots without opening a window. Desktop previews do not validate VR input or stereo presentation.
