# H150101 Eight-Axis SDL2 Input Design

## Goal

Allow the H150101 SDL2 input driver to recognize joystick axes 0 through 7 so controllers that report their directional pad on axes 6 and 7 can be mapped through a startup key configuration file.

## Scope

- Increase the driver's supported SDL joystick axis count from four to eight.
- Expose key names `axis 4-` through `axis 7+` to the existing key configuration parser.
- Preserve the existing dead-zone and digital negative/positive axis behavior.
- Do not add default bindings for axes 4 through 7.
- Do not add controller-name-specific behavior or special handling for Betop trigger axes.

## Input Behavior

SDL axis events for indices 0 through 7 are converted to the existing digital axis keys. Values below `-16384` activate the negative key, values above `16384` activate the positive key, and values inside the dead zone release both keys. Axis indices 8 and above remain ignored.

Axes 4 and 5 on the observed Betop controller rest at `-32767`. The driver may therefore track their negative digital keys as active, but they produce no frontend or libretro action because this change adds no default binding for them. The launch key configuration should bind only axes 6 and 7 for the directional pad.

Example:

```ini
bind axis 6- = player1 LEFT
bind axis 6+ = player1 RIGHT
bind axis 7- = player1 UP
bind axis 7+ = player1 DOWN
```

## Compatibility

Existing axis 0 through 3 mappings and default bindings are unchanged. The larger key table is internal to the H150101 SDL2 driver and remains within the existing libpicofe binding model.

## Verification

Add regression coverage that checks:

- the declared axis count is eight;
- names exist for both directions of axes 4 through 7;
- the existing event range checks use the expanded axis count;
- no new platform default bindings are introduced for axes 4 through 7.

Run the focused regression test, the complete repository test suite, and whitespace validation.
