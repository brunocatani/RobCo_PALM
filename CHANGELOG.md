# Changelog

## Unreleased

### Changed

- Hand gestures are off by default. Turn them on in Config → PALM settings →
  Wheel sections. A save that already shows them keeps them.

### Fixed

- A gesture can be put on the hand you point at the wheel with. The buttons
  that hand uses to click made it read as "Hand poses unavailable", so the
  right-hand gestures never worked with the right pointer. Gestures chosen
  with the wheel opened by a binding other than grip and trigger no longer get
  refused either.
- A refused or interrupted gesture writes the reason for each hand to
  `RobCoPALM.log`.

## 2.0.0

### Requirements

- RPS UI Framework 2.0.0 is required. ROCK 1.0.0 is optional and adds physical
  item handling; hand gestures also need FRIK 0.79.1 or newer with FRIK API
  2.3. A loaded ROCK older than 1.0 stops the game at the main menu with an
  error box naming the mismatch.
- The settings pages for PAPER and SCISSORS need the PAPER and SCISSORS
  versions built for ROCK 1.0.

### Added

- **Colors.** The wheel and Config draw in Fallout 4's own HUD green with
  rounded corners; a chosen choice is a tint with a bright outline instead of a
  solid block. Config → RPS mods → RPS UI → Theme switches every panel to
  Pip-Boy (green), New Vegas (amber) or Vault-Tec (blue and yellow).
- **Console tab** (Config → Console): read everything the game console prints
  and run commands, with the on-screen keyboard or your real keyboard while the
  game window has focus. Enter runs the line, Up and Down recall earlier
  commands, Page Up and Page Down scroll, Escape clears the line.
- **Console shortcut buttons** from `ConsoleCommands.ini` in
  `Mods_Config\RobCo_PALM\`: each `[Label]` section is one button that runs its
  lines in order. PALM writes a few examples the first time.
- **Equip into the hand you point with.** Choosing a weapon on the wheel equips
  it into the selecting hand through ROCK, so you can fill both hands. You can
  also add the weapon you are holding to a wheel slot, which puts it away.
- **Item Spawner:** an All ammo source, amounts up to 1000, and search. With
  Flying BRICKS loaded, All ammo lists its rounds.
- **ROCK page:** a Setup wizard button that opens ROCK's setup wizard, dropdowns
  and binding icons for the button layout, and X/Y/Z rows for ROCK's offsets.
- **Settings pages** for SCISSORS (player and developer, including its active
  ragdoll numbers), RPS UI Framework (theme, sharpness, controller ticks), ROCK
  Emitters and CSAH.
- **Developer wheel actions** shared by supported mods (`[Developer] Enabled`).
- PALM draws with your RPS UI theme, and the Config panel can be moved by
  holding grip on it; Reset puts it back.
- Opening PALM with a stick click follows ROCK's Palm binding.

### Changed

- Settings move to `PALM.toml` in `Mods_Config\RobCo_PALM\`, with each option's
  help above it. The first start carries your `PALM.ini` settings over and
  deletes the INI; nothing to do by hand.
- The PAPER and SCISSORS pages edit their new TOML settings files. The page for
  the original SCISSORS mod is gone.
- Each stick click acts for its own hand: a bipod or an object placement on one
  hand no longer blocks PALM on the other.

### Fixed

- Choosing an equipped favourite takes it off again with ROCK 1.0 installed.
- Holding an object, or carrying a weapon part, no longer blocks unequipping a
  weapon from the wheel.
- Wheel hints in press mode are correct, and long labels fit their boxes.
- Scrolling up in the console log stops it following new lines; long scrolls
  stay on the list.
- PALM keeps running when another mod hooks the game's equipment functions
  (Heisenberg and others), and a failing equipment call no longer takes the
  rest of the wheel with it.

### For mod authors

- Provider API **2.0**: the Menu and Developer interfaces in
  `include/RobCoPALM/`, discovered through `PALMAPI_QueryInterfaceV1` in
  `wheelmenu.dll`, one page each in `docs/api/`. Mods built against PALM's
  older API are refused and must be rebuilt. The SDK folder is gone.
