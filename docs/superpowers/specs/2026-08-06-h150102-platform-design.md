# H150102 Platform Design

## Goal

Add an independent `h150102` build target. H150102 is identical to H150101
for toolchain, input, audio, SDL2 integration, and filesystem layout; its only
platform difference is a 1280x720 display instead of 640x480.

## Build integration

`make platform=h150102` will reuse `plat_h150101.c` and
`plat_h150101_sdl2_input.c`. The target will use the same SDL2 sources,
compiler architecture flags, content directory, and linker flags as H150101,
while defining `H150102` instead of `H150101`.

The shared implementation avoids duplicating platform and input code. Names
and log prefixes that contain `h150101` remain internal implementation names;
renaming them is outside this change because the device behavior is shared.

## Display behavior

`scale.h` will map `H150102` to `SCREEN_WIDTH=1280` and
`SCREEN_HEIGHT=720`. Existing SDL2 initialization derives the window, logical
viewport, RGB565 texture, pitch, menu framebuffer, screenshots, and scaling
destinations from those constants.

Existing scale modes retain their current meanings. In the default
aspect-preserving scaled mode, 4:3 content occupies 960x720 with 160-pixel
bars on each side. Stretched mode fills 1280x720 with aspect distortion;
cropped mode fills the screen while removing content outside the viewport.

## Menu behavior

No H150102-specific menu layout is required. The responsive menu already has
a 1280x720 test case: it selects a 30-pixel main font, a 463-pixel menu
column, and a 504x378 preview. Background images are decoded and scaled to the
configured output size.

## Validation

- Run the menu layout tests, including the existing 1280x720 assertions.
- Run the SDL2 menu tests to guard background and text rendering behavior.
- Run `git diff --check`.
- If the H150102 cross-toolchain is available, build with
  `make platform=h150102`; otherwise report that device build verification
  remains necessary.
- On device, verify menu rendering, 4:3 pillarboxing, stretched/cropped modes,
  input mappings, and acceptable frame pacing at the higher pixel count.

## Scope

This change does not add runtime resolution detection, alter H150101, rename
the shared input implementation, or introduce device-specific performance
optimizations.
