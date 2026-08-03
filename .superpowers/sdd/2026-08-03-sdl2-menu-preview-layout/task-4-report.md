# Task 4 report: final verification and documentation consistency

## Result

The responsive layout documentation now matches the implemented and tested
`fba-a320` geometry: 640x480 uses a 328x246 preview with a 20-pixel column gap,
and 1280x720 uses a 504x378 preview. The design and execution plan now use
these values consistently, including the Task 1 assertions and aspect-fit
example.

`README.md` was inspected and was not changed. Its SDL2 section describes skin
assets and deployment only; it does not claim that the menu uses the former
full-screen darkened frame and therefore does not contradict the implementation.

## Automated verification

- Python discovery with the bundled Python 3.12 runtime: 36/37 passed. The only
  failure is the accepted pre-existing
  `test_mednafen_wswan_defaults.MednafenWswanDefaultsTest.test_audio_stability_defaults`:
  `wswan_frameskip` is absent from the current override fixture.
- Pure C layout test: MinGW GCC 9.2.0 compiled
  `tests/test_menu_layout.c` plus `menu_layout.c` with
  `-std=c99 -Wall -Wextra -Werror`; compile exit 0 and test exit 0. The compiler's
  `bin` directory had to be prepended to `PATH` for its runtime DLLs.
- Full SDL2 renderer test: unavailable. A fresh compile attempt stopped with
  exit 1 because both `tests/test_menu_sdl2.c` and `menu_sdl2.c` could not find
  `png.h`; this host also lacks the SDL2_ttf development package identified in
  the earlier task verification.
- Dependency-isolated SDL2 preview test: using the existing temporary
  declaration stubs and the real production `menu_sdl2.c`, `menu_layout.c`, and
  `text_cache.c`, the focused driver compiled with `-Wall -Wextra -Werror` and
  ran with `SDL_VIDEODRIVER=dummy`; compile exit 0 and run exit 0. This covers
  aspect-fitted pitched preview composition and invalid-input rejection, not the
  complete renderer suite.
- Parent repository `git diff --check`: exit 0.
- `libpicofe` repository `git diff --check`: exit 0; its worktree is clean.

## Static verification

The current automated contracts and source inspection establish the following
without claiming device rendering:

- `tests/test_menu_layout.c` asserts exact 328x246 and 504x378 frames, a
  20-pixel 640x480 gap through `column_gap == main_font_px`, and one-column
  fallback at 320x240.
- The layout test covers centered item blocks, bounded extreme dimensions, and
  aspect fitting. Python integration guards cover a stable responsive menu
  rectangle, row and long-text containment, no permanent SDL2 help line,
  temporary-message placement independent of list height, preview composition,
  and preservation of alternate/save-state backgrounds.
- Source inspection confirms ordinary SDL2 list entry prepares the skin
  background and captured preview, while `drew_alt_bg` prevents that composition
  from replacing alternate pages such as save-state screenshots. Failed preview
  drawing is non-fatal because the background copy occurs first and the draw
  return value does not block menu setup.
- File selection, binding, dialogs, long informational text, and save/load state
  retain their legacy full-screen call paths; the responsive rectangle is
  consumed by the ordinary list renderer rather than globally rewriting those
  page implementations.

## Builds and visual/device verification not executed

`make` is not installed or on `PATH`, so neither the local `make DEBUG=1` build
nor `make platform=h150101 DEBUG=1` was executed. The required H150101
cross-toolchain result is therefore unverified.

No device or interactive visual run was performed. In particular, menu
entry/exit on hardware, every ordinary and special page, actual save-state slot
screenshot changes, capture-failure navigation, and physical/simulated 640x480
and 1280x720 rendering still require a suitable SDL2 build environment or
device. Automated and static checks above must not be treated as those visual
checks.

## Concerns

Integration risk remains concentrated in the unavailable full SDL2 compile/link
and target build, plus unperformed interactive/device validation. The sole
Python failure is unrelated and was present at the accepted baseline.

## Documentation review follow-up

The first documentation correction missed the Task 1 code sample in the
execution plan. That sample now asserts the 328x246 frame and uses matching
328x246 aspect-fit bounds. For a 256x224 source, integer aspect fitting produces
281x246 at `(123, 50)` inside bounds starting at `(100, 50)`. A fresh text
search found no remaining obsolete 640x480 preview pair or mismatched aspect-fit
assertion in the design, plan, or this final report.

This follow-up changes documentation only. It did not rerun unavailable SDL2,
Make, cross-toolchain, device, or interactive visual verification; the limits
listed above remain unchanged.
