# SDL Pixel Format and Stride Fix Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the SDL2 hardware-scaling path use the libretro-negotiated pixel format so padded RGB565 rows are not mistaken for XRGB8888.

**Architecture:** `video.c` remains the owner of negotiated libretro video state and exposes a read-only format getter plus reusable pitch validation. `plat_sdl.c` maps that explicit format to an SDL texture format and treats pitch only as row stride.

**Tech Stack:** C, libretro pixel-format API, SDL2 streaming textures, Make.

## Global Constraints

- Do not infer pixel format from pitch.
- Preserve hardware scaling for valid padded RGB565 and XRGB8888 rows.
- Keep non-SDL2 conversion behavior unchanged.
- Do not add a core-specific SNES9x workaround.
- Preserve unrelated existing modifications in `plat_sdl.c`.

---

### Task 1: Expose and test negotiated video format

**Files:**
- Create: `tests/test_video_format.c`
- Modify: `video.h`
- Modify: `video.c`

**Interfaces:**
- Produces: `enum retro_pixel_format video_get_pixel_format(void)`
- Produces: `bool video_pitch_is_valid(enum retro_pixel_format format, unsigned width, size_t pitch)`

- [ ] **Step 1: Write the failing test**

Create `tests/test_video_format.c`:

```c
#include <assert.h>
#include <stdarg.h>
#include <stddef.h>
#include "main.h"
#include "plat.h"
#include "video.h"

void pa_log(enum retro_log_level level, const char *fmt, ...)
{
	(void)level;
	(void)fmt;
}

void quit(int code)
{
	(void)code;
	assert(0 && "unexpected fatal error");
}

void plat_video_process(const void *data, unsigned width,
			unsigned height, size_t pitch)
{
	(void)data;
	(void)width;
	(void)height;
	(void)pitch;
}

int main(void)
{
	video_set_pixel_format(RETRO_PIXEL_FORMAT_RGB565);
	assert(video_get_pixel_format() == RETRO_PIXEL_FORMAT_RGB565);
	assert(video_pitch_is_valid(RETRO_PIXEL_FORMAT_RGB565, 256, 1024));
	assert(video_pitch_is_valid(RETRO_PIXEL_FORMAT_RGB565, 256, 512));
	assert(!video_pitch_is_valid(RETRO_PIXEL_FORMAT_RGB565, 256, 511));

	video_set_pixel_format(RETRO_PIXEL_FORMAT_XRGB8888);
	assert(video_get_pixel_format() == RETRO_PIXEL_FORMAT_XRGB8888);
	assert(video_pitch_is_valid(RETRO_PIXEL_FORMAT_XRGB8888, 256, 1024));
	assert(!video_pitch_is_valid(RETRO_PIXEL_FORMAT_XRGB8888, 256, 1023));
	assert(!video_pitch_is_valid(RETRO_PIXEL_FORMAT_0RGB1555, 256, 512));

	video_deinit();
	return 0;
}
```

- [ ] **Step 2: Run the test to verify it fails**

Run on a Unix build host:

```sh
cc -Wall -I. -I./libretro-common/include \
  tests/test_video_format.c video.c -o tests/test_video_format
```

Expected: link failure because `video_get_pixel_format` and
`video_pitch_is_valid` are not defined.

- [ ] **Step 3: Add declarations**

Add to `video.h` after `video_set_pixel_format`:

```c
enum retro_pixel_format video_get_pixel_format(void);
bool video_pitch_is_valid(enum retro_pixel_format format,
			  unsigned width, size_t pitch);
```

- [ ] **Step 4: Implement the minimal helpers**

Add to `video.c` after `video_set_pixel_format`:

```c
enum retro_pixel_format video_get_pixel_format(void) {
	return screen_def.pixel_format;
}

bool video_pitch_is_valid(enum retro_pixel_format format,
			  unsigned width, size_t pitch) {
	size_t bytes_per_pixel;

	switch (format) {
	case RETRO_PIXEL_FORMAT_RGB565:
		bytes_per_pixel = sizeof(uint16_t);
		break;
	case RETRO_PIXEL_FORMAT_XRGB8888:
		bytes_per_pixel = sizeof(uint32_t);
		break;
	default:
		return false;
	}

	return width > 0 && pitch >= (size_t)width * bytes_per_pixel;
}
```

- [ ] **Step 5: Run the focused test**

Run:

```sh
cc -Wall -I. -I./libretro-common/include \
  tests/test_video_format.c video.c -o tests/test_video_format &&
./tests/test_video_format
```

Expected: exit status 0 with no output.

### Task 2: Use explicit format in SDL2 hardware scaling

**Files:**
- Modify: `plat_sdl.c:267-286`
- Modify: `plat_sdl.c:970-981`

**Interfaces:**
- Consumes: `video_get_pixel_format()`
- Consumes: `video_pitch_is_valid(format, width, pitch)`

- [ ] **Step 1: Replace pitch-based format inference**

Include `video.h` with the other local headers in `plat_sdl.c`.

Replace the existing hardware-support and texture-format helpers with:

```c
static Uint32 plat_sdl_texture_format(enum retro_pixel_format format)
{
	switch (format) {
	case RETRO_PIXEL_FORMAT_RGB565:
		return SDL_PIXELFORMAT_RGB565;
	case RETRO_PIXEL_FORMAT_XRGB8888:
		return SDL_PIXELFORMAT_XRGB8888;
	default:
		return SDL_PIXELFORMAT_UNKNOWN;
	}
}

static bool plat_sdl_is_hw_scale_supported(unsigned width, unsigned height,
					    size_t pitch,
					    enum retro_pixel_format format)
{
	if (rotate_display || height == 0)
		return false;

	return plat_sdl_texture_format(format) != SDL_PIXELFORMAT_UNKNOWN &&
	       video_pitch_is_valid(format, width, pitch);
}
```

- [ ] **Step 2: Pass negotiated format through the SDL path**

At the start of the SDL2 section in `plat_video_process`, add:

```c
	enum retro_pixel_format pixel_format = video_get_pixel_format();
	bool want_hw_scaling = plat_sdl_is_hw_scale_supported(
		width, height, pitch, pixel_format);
	Uint32 texture_format = plat_sdl_texture_format(pixel_format);
```

Remove the old calls that select texture format from `width` and `pitch`.

- [ ] **Step 3: Run static checks**

Run:

```sh
git diff --check
rg -n "texture_format_for_pitch|pitch == width \\* sizeof" plat_sdl.c
```

Expected: `git diff --check` exits 0 and `rg` finds no obsolete inference.

- [ ] **Step 4: Build the frontend**

Run on a Unix host with SDL2 development files:

```sh
make DEBUG=1 picoarch
```

Expected: `picoarch` links successfully with no new compiler warnings.

- [ ] **Step 5: Commit only the fix**

Preserve the pre-existing unrelated `plat_sdl.c` work when staging. Stage
`tests/test_video_format.c`, `video.c`, `video.h`, and only the pixel-format
hunks from `plat_sdl.c`, then commit:

```sh
git commit -m "fix padded RGB565 SDL textures"
```

### Task 3: Validate SNES9x2005 on H150101

**Files:**
- No source changes.

**Interfaces:**
- Consumes: rebuilt `picoarch-sdl2`

- [ ] **Step 1: Run the original reproduction**

```sh
./picoarch-sdl2 ./snes9x2005_libretro.so \
  /media/mmc/roms/snes/034.zip
```

- [ ] **Step 2: Verify the format log and rendered frame**

Expected log:

```text
SDL HW video: 256x224 pitch=1024 format=RGB565
```

Expected display: the complete 256x224 game frame is visible and centered,
with no content compressed into the left side.

- [ ] **Step 3: Smoke-test both tightly packed formats**

Run one known RGB565 core and one known XRGB8888 core.

Expected: each log reports its negotiated format, the full frame is visible,
and no texture-creation error appears.
