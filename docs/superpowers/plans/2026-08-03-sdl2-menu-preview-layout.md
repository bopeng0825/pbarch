# SDL2 Menu Preview Layout Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Give ordinary SDL2 menu lists a responsive left text column and an aspect-correct right game preview at 640x480 and 1280x720.

**Architecture:** Add pure responsive geometry to `menu_layout`, then make libpicofe list rendering consume that geometry while `menu_sdl2` composes the captured RGB565 frame into the preview rectangle. Keep emulator actions and special full-screen menu pages unchanged, and fall back to one column or a background-only menu when geometry or preview data is unavailable.

**Tech Stack:** C99, libpicofe menu renderer, SDL2/SDL2_image/SDL2_ttf, RGB565 buffers, Python contract tests, host C assertion tests, GNU Make.

## Global Constraints

- Enable the new presentation only under `USE_SDL2`; legacy SDL behavior must not change.
- Main font is output height divided by 24, clamped to 12-32 pixels; small font is four fifths of main.
- Outer margin and column gap equal the main font size; menu column receives 42 percent of usable width.
- Preview target width is 70 percent of output height, rounded down to a multiple of four, with a 4:3 frame centered in its column.
- Aspect-fit the source without stretching or cropping and draw no preview border.
- Hide the preview unless both columns are at least twelve main-font widths wide.
- Preserve temporary status/error messages and existing special full-screen pages; remove only permanent bottom help text.
- Do not refactor emulator actions, input handling, or non-SDL2 platform code.

---

### Task 1: Pure responsive menu geometry

**Files:**
- Modify: `menu_layout.h`
- Modify: `menu_layout.c`
- Modify: `tests/test_menu_layout.c`

**Interfaces:**
- Consumes: existing `menu_main_font_px(int)` and `menu_small_font_px(int)`.
- Produces: `struct menu_rect`, `struct menu_responsive_layout`, `menu_calculate_responsive_layout(int, int, struct menu_responsive_layout *)`, `menu_centered_block_y(int, int, int)`, and `menu_aspect_fit(int, int, const struct menu_rect *, struct menu_rect *)`.

- [ ] **Step 1: Add failing geometry tests**

Extend `tests/test_menu_layout.c` with exact assertions for:

```c
struct menu_responsive_layout layout;
struct menu_rect fitted;

menu_calculate_responsive_layout(640, 480, &layout);
assert(layout.main_font_px == 20);
assert(layout.outer_margin == 20);
assert(layout.show_preview == 1);
assert(layout.preview.w == 336);
assert(layout.preview.h == 252);

menu_calculate_responsive_layout(1280, 720, &layout);
assert(layout.main_font_px == 30);
assert(layout.preview.w == 504);
assert(layout.preview.h == 378);

menu_calculate_responsive_layout(320, 240, &layout);
assert(layout.show_preview == 0);
assert(layout.preview.w == 0 && layout.preview.h == 0);

assert(menu_centered_block_y(20, 440, 148) == 166);
assert(menu_centered_block_y(20, 100, 120) == 20);

layout.preview.x = 100; layout.preview.y = 50;
layout.preview.w = 336; layout.preview.h = 252;
menu_aspect_fit(256, 224, &layout.preview, &fitted);
assert(fitted.h == 252);
assert(fitted.w == 288);
assert(fitted.x == 124 && fitted.y == 50);
```

- [ ] **Step 2: Compile to verify the new API is missing**

Run:

```powershell
gcc -std=c99 -Wall -Wextra -I. tests/test_menu_layout.c menu_layout.c -o tests/test_menu_layout.exe
```

Expected: compilation fails because the responsive layout types/functions do not exist.

- [ ] **Step 3: Implement the minimal pure geometry API**

Add integer-only rectangle/layout types to `menu_layout.h`. Implement calculations matching `fba-a320` in `menu_layout.c`: clamp invalid dimensions to zero, calculate margins and columns, enforce the twelve-font-width threshold, cap preview width by its column and available 4:3 height, align width with `& ~3`, and zero the preview rectangle in single-column mode. `menu_centered_block_y` returns `top` when the block is invalid or does not fit. `menu_aspect_fit` centers the largest non-cropped source rectangle inside the bounds using 64-bit intermediates.

- [ ] **Step 4: Run the focused layout test**

Run:

```powershell
gcc -std=c99 -Wall -Wextra -I. tests/test_menu_layout.c menu_layout.c -o tests/test_menu_layout.exe
.\tests\test_menu_layout.exe
```

Expected: exit code 0 with no warnings.

- [ ] **Step 5: Commit geometry**

```powershell
git add menu_layout.c menu_layout.h tests/test_menu_layout.c
git commit -m "add responsive menu preview geometry"
```

### Task 2: RGB565 preview composition

**Files:**
- Modify: `menu_sdl2.h`
- Modify: `menu_sdl2.c`
- Modify: `tests/test_menu_sdl2.c`

**Interfaces:**
- Consumes: `struct menu_rect` and `menu_aspect_fit` from Task 1.
- Produces: `menu_sdl2_draw_preview(uint16_t *destination, int destination_pitch, const uint16_t *source, int source_width, int source_height, int source_pitch, const struct menu_rect *bounds)` returning 0 on success and -1 for invalid/unavailable input.

- [ ] **Step 1: Add failing preview-composition tests**

Create small patterned RGB565 source and destination arrays in `tests/test_menu_sdl2.c`. Assert that a 4x2 source drawn into a 4x4 bound is vertically centered, preserves untouched pixels outside the fitted rectangle, respects both pitches, and returns `-1` for null buffers, non-positive dimensions, or a zero-sized bound.

```c
assert(menu_sdl2_draw_preview(dst, 8, src, 4, 2, 6, &bounds) == 0);
assert(dst[1 * 8 + bounds.x] == src[0]);
assert(dst[2 * 8 + bounds.x + 3] == src[3]);
assert(dst[0 * 8 + bounds.x] == sentinel);
assert(menu_sdl2_draw_preview(NULL, 8, src, 4, 2, 6, &bounds) == -1);
```

- [ ] **Step 2: Compile to verify the compositor is missing**

Run in a POSIX build shell:

```sh
cc -std=c99 -Wall -Wextra -DUSE_SDL2 -DMENU_SDL2_TEST \
  -DMENU_SDL2_TEST_ALLOC -I. \
  $(sdl2-config --cflags) $(pkg-config --cflags SDL2_ttf) \
  tests/test_menu_sdl2.c menu_sdl2.c menu_layout.c text_cache.c \
  -lpng $(sdl2-config --libs) $(pkg-config --libs SDL2_ttf) \
  -o tests/test_menu_sdl2
```

Expected: link failure for `menu_sdl2_draw_preview`.

- [ ] **Step 3: Implement preview drawing**

Use `menu_aspect_fit` to obtain the destination rectangle. Scale by nearest-neighbor integer coordinates, copying RGB565 pixels directly and validating pitch/dimension multiplication before indexing. Do not allocate, darken, crop, or draw a border. Leave destination pixels outside the fitted rectangle unchanged.

- [ ] **Step 4: Run SDL2 renderer tests**

Run the complete compile command from Step 2, then:

```sh
SDL_VIDEODRIVER=dummy ./tests/test_menu_sdl2
```

Expected: exit code 0, including existing background, UTF-8 cache, allocation-failure, and stride tests.

- [ ] **Step 5: Commit the compositor**

```powershell
git add menu_sdl2.c menu_sdl2.h tests/test_menu_sdl2.c
git commit -m "add SDL2 menu frame preview"
```

### Task 3: Apply responsive layout to ordinary list menus

**Files:**
- Modify: `libpicofe/menu.c`
- Modify: `libpicofe/menu.h`
- Modify: `menu.c`
- Modify: `tests/check_ui_literals.py`
- Modify: `tests/test_check_ui_literals.py`
- Modify: `tests/test_menu_legacy_geometry.py`

**Interfaces:**
- Consumes: Task 1 layout helpers, Task 2 preview compositor, `g_menubg_src_ptr/g_menubg_src_w/g_menubg_src_h/g_menubg_src_pp`, and existing `menu_draw_begin/menu_draw_end` lifecycle.
- Produces: SDL2-only list-layout activation state exposed through narrowly scoped menu helpers; legacy list drawing remains byte-for-byte behaviorally equivalent.

- [ ] **Step 1: Add failing source-contract tests**

Extend Python tests to require the SDL2 path to:

- calculate `struct menu_responsive_layout` from `g_menuscreen_w/h`;
- draw the captured `g_menubg_src_ptr` into `layout.preview` after copying the skin background;
- position ordinary list blocks with `menu_centered_block_y`;
- constrain list width to `layout.menu`;
- omit the permanent help-line draw while retaining `menu_error_msg` drawing;
- keep the existing non-SDL2 full-screen centering statements inside the legacy branch.

Run:

```powershell
python -m unittest tests.test_check_ui_literals tests.test_menu_legacy_geometry -v
```

Expected: failures identifying the missing responsive list and preview contracts.

- [ ] **Step 2: Add SDL2 list-layout state without changing controllers**

In `menu.c`, calculate layout when beginning an ordinary SDL2 list frame. Copy the skin background, then call `menu_sdl2_draw_preview` using the last completed RGB565 source buffer. If preview drawing returns `-1`, continue with the skin background. Preserve `drew_alt_bg` so a selected save-state screenshot remains the preview source until that modal flow ends.

- [ ] **Step 3: Position and fit ordinary lists inside the left column**

In `libpicofe/menu.c`, keep current menu entry measurement and value generation. Under `USE_SDL2`, replace whole-screen `x/y/w` placement only for `me_draw`: use the menu rectangle's stable left inset, cap width at the menu rectangle's right edge, and calculate `y` from the visible block height with `menu_centered_block_y`. Keep confirmation boxes, ROM selection, binding pages, credits, and other specialized functions on their existing full-screen geometry.

- [ ] **Step 4: Remove permanent help text but preserve messages**

Remove only unconditional bottom help rendering in the ordinary SDL2 list path. Keep `menu_error_msg`, save/config status, failure feedback, and modal instructions. Fit temporary messages to the full output width minus outer margins.

- [ ] **Step 5: Run contracts and focused C tests**

Run:

```powershell
python -m unittest discover -s tests -p 'test_*.py' -v
gcc -std=c99 -Wall -Wextra -I. tests/test_menu_layout.c menu_layout.c -o tests/test_menu_layout.exe
.\tests\test_menu_layout.exe
```

Expected: all Python and layout tests pass; legacy geometry assertions still pass.

- [ ] **Step 6: Build the affected platform**

Run:

```powershell
make platform=h150101 DEBUG=1
```

Expected: successful SDL2 H150101 build. If the cross-toolchain is not installed, record the exact missing command/library and still run `make DEBUG=1` for the local Unix configuration when available.

- [ ] **Step 7: Commit integrated list layout**

```powershell
git add libpicofe/menu.c libpicofe/menu.h menu.c tests/check_ui_literals.py tests/test_check_ui_literals.py tests/test_menu_legacy_geometry.py
git commit -m "sync responsive SDL2 menu layout"
```

### Task 4: Final verification and documentation consistency

**Files:**
- Modify: `README.md` only if its SDL2 menu description contradicts the implemented behavior.
- Verify: `docs/superpowers/specs/2026-08-03-sdl2-menu-preview-layout-design.md`

**Interfaces:**
- Consumes: all earlier tasks.
- Produces: verified implementation and an accurate user-facing description.

- [ ] **Step 1: Run the complete available verification set**

Run all Python tests, the layout C test, the SDL2 renderer C test under the dummy video driver, `git diff --check`, and the applicable DEBUG build. Expected: every available command exits 0.

- [ ] **Step 2: Inspect both target layouts**

Exercise or instrument 640x480 and 1280x720 outputs and verify: stable left text origin, vertically centered item block, preview frames of 328x246 (with a 20-pixel column gap) and 504x378 respectively, aspect-correct game image, no border, no permanent help line, visible temporary messages, and one-column fallback on a narrow output.

- [ ] **Step 3: Check special pages and failure fallback**

Verify file selection, binding, dialogs, and long text retain full-screen geometry. Enter a menu with no valid captured frame and confirm the skin background and navigation remain functional. Open save/load state selection and confirm its screenshot behavior remains intact.

- [ ] **Step 4: Update documentation only when needed**

If `README.md` describes the former full-screen darkened-frame menu, replace that sentence with the responsive left-list/right-preview behavior and state that narrow SDL2 outputs automatically use one column. Otherwise make no documentation edit.

- [ ] **Step 5: Commit any final documentation correction**

```powershell
git add README.md
git commit -m "document responsive SDL2 menu"
```

Skip this commit when no README change is necessary.
