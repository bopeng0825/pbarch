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

## Review fixes

The review in `task-3-review.md` identified two Important issues and one
Minor issue. All three were addressed in a follow-up RED/GREEN cycle.

### Follow-up RED

The source contract was extended to require persistent texture validity,
duplicate suppression, and software-scaled gameplay upload accounting. Its
first run failed with:

```text
an offered texture is not invalidated before returning it to the core
```

The pure-C `test_video_direct.c` now also encodes these sequences:

1. a valid texture is offered, then receives a NULL/no callback;
2. the next duplicate remains suppressed across the frame boundary;
3. an exact direct acceptance restores duplicate eligibility;
4. another offer invalidates it again;
5. a full upload restores eligibility; and
6. resource reset invalidates it.

The C test could not be compiled in this sandbox because no C compiler is
installed, but it exercises production functions rather than a test-only
model.

### Follow-up GREEN

Fresh execution of:

```powershell
.\tests\test_software_framebuffer_contract.ps1
```

prints `software framebuffer source contract: PASS`, and
`git diff --check` exits zero.

### Persistent validity lifecycle

`struct video_direct_validity` is intentionally separate from the per-frame
offer/match state. Calling `video_direct_begin()` or `video_direct_end()` does
not reset it, so an unaccepted write remains invalid across later frames.

- Locking and offering a texture invalidates its visible contents
  immediately, because the core may write before any callback.
- An exact direct callback accepts and validates those contents.
- A NULL callback does not validate them; `plat_video_dupe()` therefore
  leaves `frame_dirty` unchanged.
- A missing callback reaches frame end with validity still false. A later
  duplicate is also suppressed.
- A full hardware upload validates the texture immediately.
- A software-scaled frame validates the pending full-screen contents and
  uploads them in `fb_flip()` before presentation.
- Texture/resource destruction resets validity and pending-upload state.
- Menu entry explicitly clears gameplay upload attribution, while the menu
  upload itself still updates the texture normally.

This is minimal state tracking rather than a second texture. It cannot restore
the previous bytes after a rejected offer, so it deliberately suppresses
duplicates until a successful direct callback or ordinary full upload
establishes known-good contents, matching the requested persistent-invalid
semantics.

### Profiling correction

Software-scaled gameplay frames set `gameplay_upload_pending`. When
`fb_flip()` executes their `SDL_UpdateTexture()`, it increments
`upload_frames` only if profiling is enabled, then clears the marker.
Menu-only calls never set this marker, and menu entry clears any stale marker,
so they are excluded from the gameplay metric.

Hardware RGB565 and XRGB8888 uploads retain their existing guarded increments.
Thus all gameplay `SDL_UpdateTexture()` submission paths now contribute once.

### Warning correction and follow-up self-review

The SDL2 direct processing function now checks `pitch` against the matched
offer invariant before accepting the texture, resolving the unused-parameter
warning without discarding useful validation.

Follow-up inspection confirmed:

- an unmatched non-NULL callback still unlocks before ordinary upload;
- NULL and missing callbacks remain invalid after frame-end unlock;
- no invalid duplicate sets `frame_dirty`;
- direct acceptance happens before the existing present/pacing phase;
- software fallback counts only when its actual `SDL_UpdateTexture()` runs;
- menu and resource recreation clear validity and gameplay attribution; and
- SDL1 remains isolated from SDL2-only validity state.

## Second review fix

The re-review found that `fb_flip()` treated a menu-only texture update as a
validating gameplay upload. A new contract assertion first failed with:

```text
menu-only upload must not validate an invalid gameplay texture
```

`fb_flip()` now gates both recovery of persistent direct-texture validity and
the `upload_frames` increment on `gameplay_upload_pending`. The SDL texture is
still updated for menu rendering, but that menu-only operation cannot make a
previously rejected gameplay texture eligible for a later duplicate callback.

The permitted recovery transitions are now exactly:

- an exact direct callback accepted by `plat_video_process_direct()`;
- a hardware or software full gameplay upload; or
- resource recreation/reset.

Menu entry clears gameplay attribution before its upload. When it reuses an
existing texture rather than recreating one, persistent invalidity remains
unchanged. Fresh source-contract and whitespace checks pass after this change.
