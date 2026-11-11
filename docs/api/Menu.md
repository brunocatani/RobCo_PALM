# Menu

Header: `RobCoPALM/Menu.h`, with icons in `RobCoPALM/Icons.h`. Namespace: `palm::api::menu`. Version 2.0.

A mod's own section on the wheel: a named group of up to eight items, each with an icon, a name and an optional quantity. PALM supports up to ten registered sections. New sections start hidden until the user enables them in PALM settings.

## Binding

PALM publishes its API from `include/RobCoPALM/`. The headers include only the C++ standard library and each other. Add this repository's `include/` directory through a CMake cache variable, for example `PALM_INCLUDE_DIR`, and include `<RobCoPALM/...>`. Do not copy the headers or link PALM.

Resolve `PALMAPI_QueryInterfaceV1` from the loaded `wheelmenu.dll`, then acquire the table. `Client.h` requests exactly the version and table size your headers declare.

```cpp
#include <RobCoPALM/Client.h>
#include <RobCoPALM/Menu.h>

const auto module = GetModuleHandleW(palm::api::kModuleName);
const auto query = module ? reinterpret_cast<palm::api::QueryInterfaceV1>(
    GetProcAddress(module, palm::api::kQueryExportName)) : nullptr;

const palm::api::menu::Api* menu{};
if (palm::api::acquire(query, menu) == palm::api::Result::Ok) {
    // Every function in the table is set.
}
```

`tests/ConsumerExample.cpp` shows a complete section for a physical magazine mod and is compiled with the tests.

## Version negotiation

Discovery serves an interface when the major matches exactly, the minor is at least the one requested and the table is at least the size requested. Otherwise it returns `UnknownInterface`, `UnsupportedMajor`, `UnsupportedMinor` or `TableTooSmall` and no table.

Major 1 is refused. PALM logs every version refusal with the calling DLL, then stops the game at the main menu with an error box that names the mod to update. The pre-2.0 exports `GetPALMMenuApi` and `GetPALMDeveloperApi` stay exported, return no table and report the same stop. Their names are never reused. A mod built against PALM 1.2 is updated with [Migration](Migration.md).

## Sections and items

`registerSection` takes a `Section`: a stable, globally unique `id` of ASCII letters, digits, `_`, `-` and `.`, a `name`, a `modName`, an `icon`, an `onSelect` callback and a `context`. It returns a handle. Handles are never reused.

`setItems` replaces the section's item snapshot with up to `kMaxItems` `Item` records. Each has a nonzero `id` that stays stable within the section and need not be a FormID, an `icon`, a `quantity`, `ItemFlag` bits (`Disabled`, `Equipped`, `ShowQuantity`), a `name` and `userData`. PALM copies the text and items before returning.

`unregisterSection` removes the section. Removing a section or item cancels any queued selection that refers to it.

Calls are thread-safe and bounded. No engine pointers enter the API. Only the callback and context are borrowed: keep the module and context alive until `unregisterSection` returns `Ok`. `CallbackBusy` means an invocation is still running, so retry later without blocking a game, input or render callback.

## Selection

`onSelect` runs on the F4SE game task thread without PALM locks. In press mode the wheel stays open, so do not depend on it closing before dispatch. The mod owns the action: vanilla equipment, ROCK requests or another feature. Never throw across this callback.

## Game loads

Registration survives game loads. PALM empties item snapshots at the start of a game load, so republish from a game task queued after a successful `PostLoadGame` or `NewGame`.

## Results

| Result | Meaning |
| --- | --- |
| `Ok` | Done |
| `InvalidArgument` | A null pointer, a wrong record `size` or `version`, invalid text, an invalid icon, unknown flags, or too many items |
| `NotFound` | No section with that handle |
| `DuplicateId` | The section ID is registered, or two items share an ID |
| `CapacityReached` | All sections are in use |
| `CallbackBusy` | A selection is still running |
