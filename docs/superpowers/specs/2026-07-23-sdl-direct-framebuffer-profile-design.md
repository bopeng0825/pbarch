# SDL Direct Framebuffer and Optional Profiling Design

## Goal

Reduce the SDL2 RGB565 video submission cost by allowing compatible
Libretro cores to render directly into a locked SDL streaming texture, while
preserving the existing upload path for every unsupported or unsafe case.
Make the existing periodic profiling reports opt-in through
`PICOARCH_PROFILE=1`.

## Scope

This change applies to the SDL2 software-rendered video path. It does not
change hardware-rendered Libretro cores, SDL1 behavior, menu rendering, or
XRGB8888 handling. Direct rendering is enabled automatically when all
requirements are met and falls back without interrupting emulation.

## Architecture

The frontend will implement
`RETRO_ENVIRONMENT_GET_CURRENT_SOFTWARE_FRAMEBUFFER`. Before each
`retro_run()`, the SDL platform layer may lock the current RGB565 streaming
texture. When a core queries the software framebuffer, the frontend returns
the locked texture address, pitch, dimensions, RGB565 format, and write
access.

The video callback validates that the core returns the exact framebuffer
pointer, pitch, dimensions, and format supplied by the frontend. A matching
callback marks the frame as direct and skips `SDL_UpdateTexture()`. A
non-matching callback uses the existing `video_process()` and texture upload
path. After `retro_run()`, the platform layer unlocks any locked texture.

Direct rendering is eligible only when:

- SDL2 is in use.
- The core pixel format is RGB565.
- Software rotation is disabled.
- Hardware scaling is supported for the current dimensions and pitch.
- The SDL renderer advertises native RGB565 textures.
- The streaming texture can be locked successfully.

Texture recreation, a pixel-format change, a geometry change, or any SDL
error ends the current lock before changing resources. Failure disables the
direct path for that frame and retains the existing working upload path.

## Interfaces and State

The platform video interface gains operations equivalent to:

- `plat_video_frame_begin()` to prepare and lock an eligible texture before
  `retro_run()`.
- `plat_video_get_software_framebuffer()` to populate a requested
  `struct retro_framebuffer`.
- `plat_video_frame_end()` to unlock the texture and finalize the direct or
  fallback decision.

The core environment callback handles
`RETRO_ENVIRONMENT_GET_CURRENT_SOFTWARE_FRAMEBUFFER`. The frame-running
function surrounds `retro_run()` with the begin/end calls. The video refresh
callback distinguishes direct frames from ordinary buffers and duplicate
frames.

The direct-frame state records the locked pointer, pitch, dimensions, whether
the core requested the framebuffer, whether the callback matched it, and
whether the texture is currently locked. State transitions must be safe when
the core never queries the framebuffer, queries it but renders elsewhere, or
does not invoke the video callback.

## HUD and Compatibility

HUD drawing remains on the existing copied RGB565 buffer path. When a HUD
message must be composited, the direct path is not offered for that frame so
the current HUD behavior remains unchanged. Menus retain their existing
surface and texture path.

XRGB8888, rotated output, unsupported dimensions or pitch, renderers without
native RGB565, and cores that do not use the software framebuffer interface
continue through `SDL_UpdateTexture()`.

The frontend logs a one-time status transition when direct rendering first
succeeds or when it is unavailable and the upload path is used. It does not
emit per-frame status messages.

## Profiling Control

`PICOARCH_PROFILE=1` enables all four existing periodic reports:

- `PROFILE sdl`
- `PROFILE sdl2`
- `PROFILE main`
- `PROFILE corecb`

An unset variable, an empty value, or `PICOARCH_PROFILE=0` disables these
reports. Disabled profiling bypasses per-frame profiling accumulation and
avoids profiling timer calls where practical.

The SDL report adds `direct` and `upload` frame counters. These counters make
remote validation explicit:

- `direct` counts callbacks that used the supplied locked texture.
- `upload` counts frames submitted using `SDL_UpdateTexture()`.

The existing `PICOARCH_PROFILE_SKIP_VIDEO` diagnostic remains independent.

## Error Handling

SDL texture creation or lock failure never terminates emulation solely because
the direct path is unavailable. The platform unlocks any held texture,
invalidates direct-frame state, and uses the existing upload path.

Pointer, pitch, dimension, access, or format mismatches are treated as normal
fallback conditions. Resource destruction and menu transitions must unlock
the texture before destroying or replacing it.

## Testing and Validation

Pure C tests cover:

- `PICOARCH_PROFILE` unset, empty, `0`, and nonzero behavior.
- Acceptance of eligible RGB565 framebuffer requests.
- Rejection of XRGB8888, rotation, invalid pitch, and unavailable texture
  state.
- Correct framebuffer pointer, dimensions, pitch, format, and write-access
  flags.
- Skipping upload only for an exact callback match.
- Safe fallback for mismatched pointers, pitch, or dimensions.
- Safe cleanup after lock failure, missing framebuffer queries, duplicate
  frames, and resource recreation.

Existing video-format tests remain passing. Native builds or syntax checks
provide local validation where SDL2 is available.

Device validation uses:

```sh
PICOARCH_PROFILE=1 ./picoarch core.so game.rom 2>&1 |
	tee /tmp/picoarch-profile.log
```

A successful direct path reports a one-time enabled message and approximately
`direct=60 upload=0` at 60 FPS. An unsupported core reports the fallback and
approximately `direct=0 upload=60`. Compared with the old build in the same
game and scene, the SDL `update` and core callback `video` averages should
decrease without visual, menu, HUD, pause, duplicate-frame, or
resolution-change regressions.

