# H15010x Select + Start Menu Design

## Goal

On the H150101 and H150102 SDL2 input path, open the emulator menu when
Select and Start are held together. Preserve the normal in-game behavior of
each button when it is pressed alone.

## Scope

The change is limited to the shared H15010x SDL2 input driver in
`plat_h150101_sdl2_input.c`. Other platforms and input drivers are unchanged.
The existing dedicated menu button remains supported.

## Behavior

- Pressing Select alone continues to report the libretro Select button.
- Pressing Start alone continues to report the libretro Start button.
- Holding Select and Start together emits `EACTION_MENU` once.
- The combination must be released by releasing at least one of its buttons
  before it can emit another menu action.
- The existing Select + Start long-hold quit behavior is removed completely.
- The dedicated H15010x menu button continues to emit `EACTION_MENU`.

## Implementation

Track whether the Select + Start combination is latched in each SDL2 joystick
state. During the driver's update, detect the transition from an inactive
combination to both buttons being held and add `EACTION_MENU` to the emulator
action result. Clear the latch after either button is released.

Remove the quit counter and all code that emits `EACTION_QUIT` from this
combination. Keep the existing player-button binding loop unchanged so Select
and Start remain visible to the active libretro player.

## Validation

Add focused input-driver tests that verify the source-level behavior and build
the relevant H15010x input test target where available. Confirm that:

1. the combination maps to `EACTION_MENU`;
2. no combination path maps to `EACTION_QUIT`;
3. the action is edge-triggered through a latch;
4. the individual Select and Start player bindings remain present; and
5. the dedicated menu-button binding remains present.
