# RobCo PALM

RobCo PALM is the wheel menu and the configuration pages for the RPS mods. PALM's runtime identity is defined in the UI workspace instructions.

- Build with `cmake --preset custom-fast`, then `cmake --build build-fast --config Release --target RobCoPALM`. Targeted CTest suites also use `build-fast`, Release. Deploys `wheelmenu.dll` and its PDB only to `F4SE/Plugins` under the mod folder set by `COPY_PLUGIN_BASE_PATH` in `CMakeUserPresets.json`.
- Docs: the UI workspace's `Docs/ROCK_Whell_Menu/docs`. Older documents may remain in the ROCK suite workspace's `Docs/ROCK_Whell_Menu/docs`; check their relevance before use.

## Provider pages

- Runtime availability comes from the loaded modules (`src/tools/RpsMod.h`). PALM never changes a provider's published contract.
- ROCK's consumer and developer pages share one configuration registration: read the catalog from ROCK's negotiated Configuration API and route writes to ROCK. PAPER uses its INI adapter and watcher behaviour.
- Item and gesture integration use ROCK when it is loaded; without it, PALM keeps its standalone RPS UI behaviour.
- SCISSORS Redux pages (consumer and developer) bind only `SCISSORS_Redux.dll`'s `SCISSORS_Redux_GetConfiguration` 2.0 table, from Redux's own `include/` (`SCISSORS_REDUX_INCLUDE_DIR`). Never forward NPC settings through ROCK or write Redux INIs directly. `bPhysicalMelee` lives on the Redux Dev page and goes through Redux Configuration, never ROCK configuration.
- Preview fixture flags never alter production discovery or write runtime INIs; preview never discovers or writes a real provider, and injected fixtures stay read-only.

## Pending API rework

When work on RobCo PALM's API starts, rework it the way ROCK's 1.0 reset did (master `AGENTS.md`, section 15):

- Move the public headers into `include/RobCoPALM/`, self-contained, with `kMajor`/`kMinor` in each interface header and one page per interface in `docs/api/`. Drop every SDK copy and bind other providers from their own `include/` directories.
- Squash every earlier version and minor of each interface into exactly 2.0. Every request for a major-1 version fails closed: discovery returns a logged unsupported-version status and never a table.
- Whatever is not carried into the squashed version is deleted with its code, tests and docs. No code or behaviour is kept to serve an older version: it is either in the new version or dead.
- Covers the Menu (section) and Developer APIs in `SDK/include/`.
- Update `providers/RobCo_PALM.toml` in the API release registry (headers, `headers_ready = true`) and run `check.py`.
