# Task 2 report: RGB565 preview composition

## Result

Added `menu_sdl2_draw_preview` to aspect-fit an RGB565 frame into menu preview bounds, nearest-neighbor scale it with independent source and destination pitches, and leave all pixels outside the fitted rectangle unchanged. Invalid buffers, dimensions, bounds, pitches, coordinate overflow, and address-span overflow return `-1`.

## RED

Added renderer tests before production code for a patterned 4x2 source in a 4x4 bound. They require vertical centering, source pitch 6, destination pitch 8, unchanged sentinel pixels around the fitted rectangle, and `-1` for null buffers, non-positive dimensions, a zero-width bound, or null bounds.

The requested complete SDL2 compile could not reach the expected missing-symbol link failure on this host. With the required compiler on `PATH`, it stopped first with exit 1:

```text
tests/test_menu_sdl2.c:7:10: fatal error: png.h: No such file or directory
menu_sdl2.c:8:10: fatal error: png.h: No such file or directory
```

No SDL2_ttf development headers or libraries were found either. Before implementation, the new API was absent from both the header and object, so the newly added test was not buildable against the old production sources.

## GREEN

Because the complete host dependencies are unavailable, used temporary, uncommitted declaration stubs for the unrelated PNG/SDL/SDL_ttf APIs and linked the real production files (`menu_sdl2.c`, `menu_layout.c`, and `text_cache.c`) with the required compiler:

```text
D:\Software\Gui-Guider\environment\mingw\bin\gcc.exe -std=c99 -Wall -Wextra -Werror \
  -IC:\tmp\pbarch-preview-stubs -I. \
  C:\tmp\pbarch-preview-stubs\preview_test.c menu_sdl2.c menu_layout.c text_cache.c \
  -o C:\tmp\pbarch-preview-stubs\preview_test.exe
compile_exit=0
test_exit=0
```

The focused driver exercises the successful pitched/centered copy, untouched sentinels, null destination rejection, and short source-pitch rejection. The repository test contains the broader invalid-input assertions.

Fresh final `git diff --check` exited 0 with no output.

## Self-review

- The implementation calls Task 1's `menu_aspect_fit`; it does not allocate, crop, darken, or draw a border.
- Source coordinates use the existing overflow-safe `menu_sdl2_scale_coordinate` helper.
- Bounds additions and pitch-by-height byte spans are validated before indexing; each index is formed with `size_t`.
- Pixels are written only inside the fitted rectangle.
- Changes are limited to the three requested source/test files plus this report. Existing unrelated `tests/__pycache__/` and `tools/__pycache__/` files were left untouched.

## Commit

Pending at report creation; the final commit is recorded in the task handoff.

## Concerns

The full SDL2 renderer suite, including existing background, UTF-8 cache, allocation-failure, and stride tests, could not be compiled or run on this Windows host because libpng and SDL2_ttf development packages are absent. Only the dependency-isolated real compositor path was compiled and executed here; a POSIX environment with the documented packages must run the complete command before integration.
