# Developer

Header: `RobCoPALM/Developer.h`. Namespace: `palm::api::developer`. Version 2.0.

Actions on the shared Developer wheel, for local diagnostics such as a capture switch. All mods share one wheel of `kCapacity` actions, shown only when PALM's Developer mode is on. Bind it the same way as the [Menu](Menu.md) interface.

`enabled` reports whether Developer mode is on.

`registerAction` takes an `Action`: a stable, mod-qualified `id`, a `label` that includes the mod's name, an `invoke` callback and a `context`. It returns a handle. Registration survives game loads and never replaces another ID.

`unregisterAction` removes an action. The callback and context are borrowed, so keep them alive until it returns `Ok`. `CallbackBusy` requires a nonblocking retry.

`invoke` runs on the game task thread. Enqueue work to your mod's appropriate thread. Do not throw across this callback or do blocking capture or file operations in it.

## Results

| Result | Meaning |
| --- | --- |
| `Ok` | Done |
| `InvalidArgument` | A null pointer, a wrong record `size` or `version`, invalid text or no callback |
| `Disabled` | Developer mode is off |
| `DuplicateId` | The action ID is registered |
| `CapacityReached` | The wheel is full. Every existing action is preserved, and PALM never evicts one; get the user's approval before unregistering an existing action to make room |
| `NotFound` | No action with that handle |
| `CallbackBusy` | The action's callback is still running |
