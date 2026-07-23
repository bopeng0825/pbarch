# SDL Direct Framebuffer and Optional Profiling Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Let compatible RGB565 Libretro cores render directly into the SDL2 streaming texture and make periodic profiling opt-in with `PICOARCH_PROFILE=1`.

**Architecture:** A small environment-controlled profiling module gates all four existing profilers. A platform-neutral direct-frame contract module validates the framebuffer handed to a core and the subsequent video callback, while `plat_sdl.c` owns SDL texture creation, locking, unlocking, presentation, and fallback.

**Tech Stack:** C11-compatible C, Libretro software framebuffer API, SDL2 streaming textures, existing assert-based C tests, GNU Make.

## Global Constraints

- Direct rendering is SDL2/RGB565-only and automatically falls back to the current upload path.
- XRGB8888, SDL1, software rotation, HUD compositing, and menus retain their current behavior.
- An unset, empty, or `0` `PICOARCH_PROFILE` disables periodic profiling; any other value enables it.
- Existing `PICOARCH_PROFILE_SKIP_VIDEO` behavior remains independent.
- No downstream core source is modified.

---

### Task 1: Add the optional profiling gate

**Files:**
- Create: `profile.h`
- Create: `profile.c`
- Create: `tests/test_profile.c`
- Modify: `Makefile`
- Modify: `main.c`
- Modify: `core.c`
- Modify: `plat_sdl.c`

**Interfaces:**
- Produces: `bool profile_is_enabled(void)` and `void profile_reset_enabled_cache(void)`.
- Consumes: the process environment variable `PICOARCH_PROFILE`.

- [ ] **Step 1: Write the failing environment test**

Create `tests/test_profile.c`:

```c
#include <assert.h>
#include <stdlib.h>
#include "profile.h"

static void check(const char *value, bool expected)
{
	if (value)
		setenv("PICOARCH_PROFILE", value, 1);
	else
		unsetenv("PICOARCH_PROFILE");
	profile_reset_enabled_cache();
	assert(profile_is_enabled() == expected);
}

int main(void)
{
	check(NULL, false);
	check("", false);
	check("0", false);
	check("1", true);
	check("yes", true);
	return 0;
}
```

- [ ] **Step 2: Run the test and verify RED**

Run:

```sh
cc -Wall -Wextra -I. tests/test_profile.c profile.c -o /tmp/test_profile
```

Expected: compilation fails because `profile.h` and `profile.c` do not exist.

- [ ] **Step 3: Add the profiling module**

Create `profile.h`:

```c
#ifndef __PROFILE_H__
#define __PROFILE_H__

#include <stdbool.h>

bool profile_is_enabled(void);
void profile_reset_enabled_cache(void);

#endif
```

Create `profile.c`:

```c
#include <stdlib.h>
#include <string.h>
#include "profile.h"

static int profile_enabled = -1;

bool profile_is_enabled(void)
{
	const char *value;

	if (profile_enabled >= 0)
		return profile_enabled != 0;
	value = getenv("PICOARCH_PROFILE");
	profile_enabled = value && value[0] && strcmp(value, "0") ? 1 : 0;
	return profile_enabled != 0;
}

void profile_reset_enabled_cache(void)
{
	profile_enabled = -1;
}
```

Add `profile.c` to `SOURCES` in `Makefile`.

- [ ] **Step 4: Gate all profiling work**

Include `profile.h` in `main.c`, `core.c`, and `plat_sdl.c`.

At the beginning of `loop_profile_frame()`, `core_profile_finish_frame()`,
and `plat_sdl_profile_frame()`, return immediately when
`!profile_is_enabled()`.

In `core_run_frame()`, avoid the two profiling timer calls when profiling is
disabled:

```c
if (profile_is_enabled()) {
	start_us = core_profile_ticks_us();
	current_core.retro_run();
	core_profile_finish_frame(core_profile_ticks_us() - start_us);
} else {
	current_core.retro_run();
}
```

At SDL call sites that exist only to collect a duration, call
`plat_get_ticks_us_u64()` and `plat_sdl_profile_add()` only when profiling is
enabled. Preserve timing used for frame pacing independently of profiling.

- [ ] **Step 5: Verify GREEN**

Run:

```sh
cc -Wall -Wextra -I. tests/test_profile.c profile.c -o /tmp/test_profile
/tmp/test_profile
```

Expected: exit status 0 with no output.

- [ ] **Step 6: Commit**

```sh
git add Makefile profile.c profile.h tests/test_profile.c main.c core.c plat_sdl.c
git commit -m "gate runtime profiling"
```

---

### Task 2: Add a testable direct-frame contract

**Files:**
- Create: `video_direct.h`
- Create: `video_direct.c`
- Create: `tests/test_video_direct.c`
- Modify: `Makefile`

**Interfaces:**
- Produces:
  - `void video_direct_begin(struct video_direct_state *state)`
  - `void video_direct_offer(struct video_direct_state *state, void *data, unsigned width, unsigned height, size_t pitch)`
  - `bool video_direct_match(struct video_direct_state *state, const void *data, unsigned width, unsigned height, size_t pitch)`
  - `void video_direct_end(struct video_direct_state *state)`
- The SDL layer owns the memory and lock; this module owns only validation state.

- [ ] **Step 1: Write the failing contract test**

Create `tests/test_video_direct.c`:

```c
#include <assert.h>
#include <stdint.h>
#include "video_direct.h"

int main(void)
{
	uint16_t pixels[320 * 240];
	struct video_direct_state state = {0};

	video_direct_begin(&state);
	video_direct_offer(&state, pixels, 320, 240, 640);
	assert(video_direct_match(&state, pixels, 320, 240, 640));
	assert(!video_direct_match(&state, pixels + 1, 320, 240, 640));
	assert(!video_direct_match(&state, pixels, 320, 239, 640));
	assert(!video_direct_match(&state, pixels, 320, 240, 642));
	video_direct_end(&state);
	assert(!video_direct_match(&state, pixels, 320, 240, 640));
	return 0;
}
```

- [ ] **Step 2: Run the test and verify RED**

```sh
cc -Wall -Wextra -I. tests/test_video_direct.c video_direct.c -o /tmp/test_video_direct
```

Expected: compilation fails because the direct-frame module is absent.

- [ ] **Step 3: Implement the state machine**

Create `video_direct.h`:

```c
#ifndef __VIDEO_DIRECT_H__
#define __VIDEO_DIRECT_H__

#include <stdbool.h>
#include <stddef.h>

struct video_direct_state {
	void *data;
	unsigned width;
	unsigned height;
	size_t pitch;
	bool frame_active;
	bool offered;
	bool matched;
};

void video_direct_begin(struct video_direct_state *state);
void video_direct_offer(struct video_direct_state *state, void *data,
			unsigned width, unsigned height, size_t pitch);
bool video_direct_match(struct video_direct_state *state, const void *data,
			unsigned width, unsigned height, size_t pitch);
void video_direct_end(struct video_direct_state *state);

#endif
```

Implement exact pointer/dimension/pitch matching in `video_direct.c`.
`video_direct_begin()` clears the previous offer and activates the frame;
`video_direct_offer()` records only a non-NULL, nonzero offer;
`video_direct_match()` sets `matched` only for one exact active offer;
`video_direct_end()` clears all state.

Add `video_direct.c` to `SOURCES`.

- [ ] **Step 4: Verify GREEN**

```sh
cc -Wall -Wextra -I. tests/test_video_direct.c video_direct.c -o /tmp/test_video_direct
/tmp/test_video_direct
```

Expected: exit status 0.

- [ ] **Step 5: Commit**

```sh
git add Makefile video_direct.c video_direct.h tests/test_video_direct.c
git commit -m "add direct video frame contract"
```

---

### Task 3: Integrate the Libretro software framebuffer with SDL2

**Files:**
- Modify: `plat.h`
- Modify: `core.c`
- Modify: `plat_sdl.c`
- Modify: `tests/test_video_format.c`

**Interfaces:**
- Produces:
  - `void plat_video_frame_begin(void)`
  - `bool plat_video_get_software_framebuffer(struct retro_framebuffer *framebuffer)`
  - `bool plat_video_frame_is_direct(const void *data, unsigned width, unsigned height, size_t pitch)`
  - `void plat_video_frame_end(void)`
- Consumes: `video_get_pixel_format()`, SDL renderer format information, and the direct-frame contract from Task 2.

- [ ] **Step 1: Extend the platform-stub test and verify RED**

Add assertions to a new `tests/test_software_framebuffer.c` using platform
stubs. Verify that the non-SDL implementation returns `false`, that RGB565
requests require write access, and that XRGB8888 requests are rejected.

Compile with:

```sh
cc -Wall -Wextra -I. -Ilibretro-common/include \
	tests/test_software_framebuffer.c video_direct.c \
	-o /tmp/test_software_framebuffer
```

Expected: compilation fails because the platform APIs are not declared.

- [ ] **Step 2: Declare the lifecycle API**

In `plat.h`, include the Libretro framebuffer declaration and add:

```c
void plat_video_frame_begin(void);
bool plat_video_get_software_framebuffer(struct retro_framebuffer *framebuffer);
bool plat_video_frame_is_direct(const void *data, unsigned width,
				unsigned height, size_t pitch);
void plat_video_frame_end(void);
```

For SDL1 builds, implement no-op begin/end functions and return `false` from
the query and match functions.

- [ ] **Step 3: Implement SDL renderer capability detection**

During SDL renderer creation, inspect `SDL_RendererInfo.texture_formats` and
cache whether `SDL_PIXELFORMAT_RGB565` is advertised. Reset this flag when
the renderer is destroyed or recreated.

Do not infer native RGB565 support merely from successful
`SDL_CreateTexture()`, because SDL can create a conversion wrapper for an
unsupported format.

- [ ] **Step 4: Implement framebuffer acquisition during `retro_run()`**

`plat_video_frame_begin()` resets direct-frame state.

`plat_video_get_software_framebuffer()` must return `false` unless:

- a frame is active;
- `framebuffer->access_flags` contains `RETRO_MEMORY_ACCESS_WRITE`;
- `video_get_pixel_format()` is `RETRO_PIXEL_FORMAT_RGB565`;
- `rotate_display == 0`;
- no HUD message is active;
- native RGB565 is advertised;
- width and height are nonzero and within renderer limits.

Use the requested `framebuffer->width` and `framebuffer->height` to ensure an
RGB565 streaming texture, lock it with `SDL_LockTexture()`, and populate:

```c
framebuffer->data = locked_pixels;
framebuffer->pitch = locked_pitch;
framebuffer->format = RETRO_PIXEL_FORMAT_RGB565;
framebuffer->memory_flags = RETRO_MEMORY_TYPE_CACHED;
```

Offer the exact returned values to `video_direct_offer()`. Reject a second
query in the same frame unless it requests the same dimensions.

- [ ] **Step 5: Recognize a direct video callback**

At the start of `pa_video_refresh()`, test
`plat_video_frame_is_direct(data, width, height, pitch)`.

For an exact match:

- mark the frame dirty;
- update the SDL source and destination rectangles;
- select hardware scaling;
- increment the direct-frame profile counter;
- do not call `video_process()` or `SDL_UpdateTexture()`.

For non-NULL non-matching data, retain the current `video_process()` path and
increment the upload counter when `SDL_UpdateTexture()` executes. Preserve
the current duplicate-frame behavior for `NULL`.

- [ ] **Step 6: Unlock safely at frame end and resource transitions**

`plat_video_frame_end()` unlocks a locked texture exactly once and clears
the active-frame state. Call the same internal unlock helper before:

- destroying or recreating `screen_texture`;
- entering the menu;
- closing video;
- recreating the renderer.

If a core requested the framebuffer but did not return it in the video
callback, unlock it and leave the ordinary upload path available on the next
frame.

- [ ] **Step 7: Wire the Libretro environment callback**

In `core_environment()`, add:

```c
case RETRO_ENVIRONMENT_GET_CURRENT_SOFTWARE_FRAMEBUFFER:
	return data &&
		plat_video_get_software_framebuffer(
			(struct retro_framebuffer *)data);
```

In `core_run_frame()`, call `plat_video_frame_begin()` immediately before
`current_core.retro_run()` and `plat_video_frame_end()` immediately after it,
including the profiling-enabled and disabled branches.

- [ ] **Step 8: Add observability**

Add `direct_frames` and `upload_frames` to `struct sdl_video_profile`. Append
this to the existing SDL profile line:

```text
 direct=%u upload=%u
```

Log one transition message:

```text
SDL direct framebuffer: enabled WIDTHxHEIGHT pitch=PITCH RGB565
```

When the ordinary path is first used after attempting direct operation, log:

```text
SDL direct framebuffer: unavailable, using texture upload
```

Do not print these transition messages every frame.

- [ ] **Step 9: Run unit and build verification**

```sh
cc -Wall -Wextra -I. tests/test_profile.c profile.c -o /tmp/test_profile
/tmp/test_profile
cc -Wall -Wextra -I. tests/test_video_direct.c video_direct.c -o /tmp/test_video_direct
/tmp/test_video_direct
make DEBUG=1
```

Expected: both tests exit 0 and the native SDL2 build completes without new
warnings.

- [ ] **Step 10: Commit**

```sh
git add plat.h core.c plat_sdl.c tests/test_software_framebuffer.c
git commit -m "render compatible cores directly to SDL texture"
```

---

### Task 4: Device A/B validation

**Files:**
- Modify only if validation finds a reproducible defect.

**Interfaces:**
- Consumes: the runtime logs introduced in Tasks 1 and 3.
- Produces: confirmation that direct and fallback paths both work on H150101.

- [ ] **Step 1: Build the device binary**

```sh
make platform=h150101 DEBUG=1
```

Expected: `picoarch` and the selected core build successfully with SDL2.

- [ ] **Step 2: Validate a compatible RGB565 core**

```sh
PICOARCH_PROFILE=1 ./picoarch core.so game.rom 2>&1 |
	tee /tmp/picoarch-direct.log
```

Expected:

- one `SDL direct framebuffer: enabled` line;
- `direct` approximately equals FPS;
- `upload=0` while no HUD is displayed;
- correct image, scaling, input, audio, pause, and resume.

- [ ] **Step 3: Validate fallback**

Run an XRGB8888 or software-framebuffer-incompatible core.

Expected:

- one fallback status line;
- `direct=0`;
- `upload` approximately equals FPS;
- rendering remains correct.

- [ ] **Step 4: Validate profile-off behavior**

```sh
PICOARCH_PROFILE=0 ./picoarch core.so game.rom 2>&1 |
	tee /tmp/picoarch-no-profile.log
```

Expected: no `PROFILE sdl`, `PROFILE sdl2`, `PROFILE main`, or
`PROFILE corecb` lines.

- [ ] **Step 5: Compare performance**

Use the same game, scene, CPU clock, audio configuration, and scaling mode for
old and new builds. Confirm that direct mode lowers `update` and
`corecb video` time without increasing underruns, late frames, or maximum
present time.

