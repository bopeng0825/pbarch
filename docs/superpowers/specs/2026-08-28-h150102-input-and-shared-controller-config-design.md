# H150102 Input and Shared Controller Configuration Design

## Goal

Give H150102 its own platform input mapping while allowing identical H150101/H150102 SDL2 controllers to share one key configuration section.

## Platform Separation

Create `plat_h150102.c` as an initial copy of `plat_h150101.c`. The H150102 Makefile branch will compile `plat_h150102.c` together with the existing shared `plat_h150101_sdl2_input.c`. The H150101 branch remains unchanged.

The copied H150102 mappings intentionally start identical to H150101. Keeping the platform files separate allows later H150102 button, menu, or default-binding changes without changing H150101 behavior. The SDL2 device discovery, event handling, axis conversion, and two-player routing remain shared.

## Shared Configuration Matching

Add an optional input-driver callback that determines whether a configured device name applies to a registered device name. The default callback preserves exact matching for every existing input driver.

The H150101 SDL2 driver callback will apply these rules:

- An exact configured name matches normally.
- A base name such as `h150101-sdl2:BETOP Controller` also matches `h150101-sdl2:p2:BETOP Controller`.
- An explicit P2 name matches only that P2 device.
- Different controller names never share mappings.

Refactor key configuration loading so one `binddev` section can apply the same parsed bindings to every matching registered device. Each target device is cleared before the section is applied, preserving the current complete-section replacement behavior.

Configuration sections continue to execute in file order. A later explicit P2 section therefore clears and replaces the shared mappings for P2 only:

```ini
binddev = h150101-sdl2:BETOP Controller
bind joy 0 = player1 B
bind joy 1 = player1 A

binddev = h150101-sdl2:p2:BETOP Controller
bind joy 0 = player1 A
bind joy 1 = player1 B
```

The shared matching behavior is part of the common SDL2 input driver, so it applies to both H150101 and H150102 builds.

## Compatibility

- Existing single-controller configuration files keep working unchanged.
- Existing explicit P2 sections keep working.
- Drivers that do not provide the new callback retain exact device-name matching.
- Player action syntax is unchanged; both physical-device sections continue to use `player1` actions because the SDL2 driver routes its second physical device to libretro port 2.
- No wildcard syntax is introduced.

## Error Handling

An unknown configured device retains the current placeholder-device behavior so configurations can be loaded before a matching device is probed. A matching section must never be applied twice to the same device. If no alias matches, only the exact or placeholder target is used.

## Verification

Add regression tests that verify:

- the H150102 Makefile branch selects `plat_h150102.c` and the shared SDL2 driver;
- `plat_h150102.c` initially contains the copied default and menu mappings;
- a base H150101 SDL2 device section applies to both identical physical devices;
- an explicit P2 section applies only to P2 and can override a prior shared section;
- different controller names do not share mappings;
- existing H150101 two-player and key configuration tests remain green.

Run the focused tests, the complete repository test suite, and `git diff --check`. Any pre-existing unrelated suite failures must be reported separately.
