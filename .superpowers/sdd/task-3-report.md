# Task 3: SDL Direct Framebuffer Integration

## Status

DONE_WITH_CONCERNS

Implementation commit: `6c35e1d`

The direct framebuffer path is integrated for SDL2, while SDL1 retains
no-op/false stubs. The only outstanding concern is environmental: this
Windows sandbox has neither `make` nor a C compiler, so a native build could
not be executed here.

## RED evidence

The first run of:

```powershell
.\tests\test_software_framebuffer_contract.ps1
```

failed with:

```text
platform frame-begin API is missing
```

This was the expected failure: the platform lifecycle API and its integration
did not yet exist.

## GREEN evidence

After implementation, the same source-contract test exits zero and prints:

```text
software framebuffer source contract: PASS
```

`git diff --check` also exits zero. The contract checks cover:

- lifecycle declarations and `retro_run()` bracketing;
- the Libretro environment callback;
- exact direct-frame recognition in the video callback;
- renderer-format capability inspection;
- texture lock/unlock operations;
- write-access, RGB565, rotation, and HUD eligibility gates;
- profile-gated direct/upload counters; and
- absence of `SDL_UpdateTexture()` from the isolated direct submission path.

The portable C test `tests/test_software_framebuffer.c` exercises the SDL1
stub contract and is ready for the plan's compiler invocation on a Unix
development host.

The requested build command was attempted:

```powershell
make DEBUG=1
```

PowerShell reported that `make` is not installed. `gcc`, `cc`, and `clang`
were also not found.

## Lifecycle reasoning

`core_run_frame()` begins the direct-frame lifecycle immediately before
`retro_run()` and ends it immediately afterward in both profiling branches.
The environment callback accepts dimensions from the core; pitch, format,
data pointer, and memory flags are filled by the frontend after locking the
texture.

Eligibility is limited to active SDL2 RGB565 frames with write access, no
rotation, no active HUD message, valid dimensions, hardware-scale-compatible
pitch, renderer limits, and native RGB565 advertised by
`SDL_GetRendererInfo()`. Successful texture creation alone is not treated as
native-format support.

The callback only takes the direct path when pointer, dimensions, and pitch
exactly match the offered framebuffer. It marks the frame dirty, computes the
same hardware-scaling rectangles used by uploads, and leaves pacing and
present logic unchanged. It never calls `video_process()` or
`SDL_UpdateTexture()`.

If a nonmatching non-NULL callback arrives while the texture is locked, the
texture is unlocked before the ordinary processing path can update, destroy,
or recreate it. Frame end, texture destruction/recreation, menu texture
replacement, renderer recreation, and shutdown all converge on an
unlock-before-destroy path. A missing query, missing callback, duplicate
frame, or lock failure is cleaned up at frame end without disabling the
existing path.

HUD frames are not offered directly, preserving the copied RGB565 compositing
path. XRGB8888, SDL1, rotation, invalid dimensions/pitch, unsupported renderer
formats, and lock failures retain their existing upload/scaling behavior.

## Observability

The SDL profile report now includes `direct` and `upload`. Both counters are
updated only when `profile_is_enabled()` is true. Direct counts exact matches;
upload counts RGB565 or XRGB8888 hardware texture submissions.

Status logging is transition-based rather than per frame:

- successful direct matching logs dimensions and frontend-supplied pitch;
- a failed match after a direct attempt logs texture-upload fallback.

## Self-review

- Confirmed framebuffer width/height are never overwritten by the frontend.
- Confirmed renderer support comes from `SDL_RendererInfo.texture_formats`.
- Confirmed a locked texture is unlocked before mismatch fallback or resource
  destruction.
- Confirmed SDL1 has definitions for every new public function.
- Confirmed direct submission does not alter the existing flip/pacing path.
- Confirmed duplicate callbacks remain routed through `plat_video_dupe()`.
- Confirmed profiling-disabled execution avoids the new counter work.
- Confirmed unrelated `.superpowers` artifacts were not included in the
  implementation commit.

## Remaining verification

On a host with the Unix toolchain, run:

```sh
cc -Wall -Wextra -I. -Ilibretro-common/include \
	tests/test_software_framebuffer.c video_direct.c \
	-o /tmp/test_software_framebuffer
/tmp/test_software_framebuffer
cc -Wall -Wextra -I. tests/test_profile.c profile.c -o /tmp/test_profile
/tmp/test_profile
cc -Wall -Wextra -I. tests/test_video_direct.c video_direct.c \
	-o /tmp/test_video_direct
/tmp/test_video_direct
make DEBUG=1
```

Device validation should additionally cover a direct-capable RGB565 core, an
upload-only core, HUD display, menu enter/leave, duplicate frames, and a
runtime resolution change.
