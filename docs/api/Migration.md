# Migrating a mod from RobCo PALM 1.2 to PALM 2.0

PALM 1.2 published one interface, the wheel section API, in
`SDK/include/PALMMenuApi.h` and `SDK/include/PALMIcons.h`, found through the
`GetPALMMenuApi` export of `wheelmenu.dll`. PALM 2.0 publishes the Menu and
Developer interfaces at major 2 from this repository's `include/RobCoPALM/`,
found through `PALMAPI_QueryInterfaceV1`. `GetPALMMenuApi` stays exported only
to refuse: it returns no table, and PALM stops the game at the main menu with
an error box naming the mod that called it. A mod with a wheel section has to
be rebuilt with the changes on this page. The section's behaviour is
unchanged.

A mod that also binds ROCK, PAPER or RPS UI Framework moves all of them at
once; each has its own `docs/api/Migration.md`. Coding agents can follow
ROCK's `skills/rock-1.0-migration/SKILL.md`, which covers the four together.

## 1. Build

- Remove the 1.2 headers from your build: include paths ending in
  `SDK/include`, and copies of `PALMMenuApi.h` and `PALMIcons.h`.
- Add this repository's `include/` directory through a CMake cache variable,
  for example `PALM_INCLUDE_DIR`, and include `<RobCoPALM/Client.h>` and
  `<RobCoPALM/Menu.h>`. `Menu.h` includes `<RobCoPALM/Icons.h>`. Nothing is
  linked.
- The headers need C++20 and a 64-bit build.

## 2. Discovery

| PALM 1.2 | PALM 2.0 |
| --- | --- |
| `GetProcAddress(module, palm::api::kExportName)` (`GetPALMMenuApi`), then `getApi(palm::api::kVersion)` | `palm::api::acquire(query, menu)` with the query resolved from `palm::api::kQueryExportName` (`PALMAPI_QueryInterfaceV1`) |
| `palm::api::ApiV1` (`version`, `byteSize`, three calls) | `palm::api::menu::Api` (the same three calls) |

```cpp
#include <RobCoPALM/Client.h>
#include <RobCoPALM/Menu.h>

const auto module = GetModuleHandleW(palm::api::kModuleName);   // wheelmenu.dll
const auto query = module ? reinterpret_cast<palm::api::QueryInterfaceV1>(
    GetProcAddress(module, palm::api::kQueryExportName)) : nullptr;
const palm::api::menu::Api* menu{};
if (palm::api::acquire(query, menu) != palm::api::Result::Ok) {
    // PALM absent or refused: skip the section.
}
```

`acquire` requests exactly the major, minor and table size your headers
declare. Never probe with trial queries: every refused query stops the game.

## 3. Names and records

| PALM 1.2 | PALM 2.0 |
| --- | --- |
| `palm::api::SectionV1` | `palm::api::menu::Section`; 192 bytes (was 184) |
| `palm::api::ItemV1` | `palm::api::menu::Item`; 128 bytes |
| `structSize` | `size` and `version` (`kRecordVersion`, 1) |
| `palm::api::ItemFlag`, `SectionHandle`, `kMaxSections`, `kMaxItems`, `kMaxVisibleEntries` | The same, in `palm::api::menu` |
| `palm::api::Result` | Unchanged values, plus `Disabled` and the discovery results |
| `palm::api::Icon` | Unchanged, now from `<RobCoPALM/Icons.h>` |
| `SelectCallback` | Unchanged signature, now `PALM_CALL` |

Value-initialize `Section` and `Item` and the defaults are right. A wrong
`size` or `version` returns `InvalidArgument`.

`registerSection`, `unregisterSection` and `setItems` keep their arguments and
behaviour: sections start hidden until the player enables them, PALM empties
item snapshots at the start of a game load, and you republish after
`PostLoadGame` or `NewGame`. See [Menu](Menu.md).

## 4. New: Developer actions

The [Developer](Developer.md) interface is new in 2.0: actions on a shared
Developer wheel, shown only while PALM's Developer mode is on.

## 5. Check the result

1. Build against `include/RobCoPALM/` with no 1.2 header left.
2. Search your source for leftovers: `PALMMenuApi.h`, `PALMIcons.h`,
   `GetPALMMenuApi`, `kExportName`, `SectionV1`, `ItemV1`, `structSize`.
3. Start the game with PALM 2.0. No error box appears at the main menu, and
   your section appears in PALM's settings, where the player enables it.
