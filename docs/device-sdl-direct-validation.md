# H150101 SDL Direct Framebuffer Validation

Run this checklist on an H150101 device (or its matching cross-build host).
Do not treat a successful build as device validation.

## Build and deploy

Set the H150101 cross-compiler prefix and name the two cores selected for the
A/B run. `CROSS_COMPILE` is the controlling toolchain input: the Makefile
derives `CC` and `SYSROOT` from `${CROSS_COMPILE}gcc`.

```sh
export CROSS_COMPILE=/path/to/toolchain/bin/mipsel-linux-
DIRECT_CORE=replace_with_rgb565_core_target
FALLBACK_CORE=replace_with_fallback_core_target
make platform=h150101 DEBUG=1 picoarch
make platform=h150101 DEBUG=1 "${DIRECT_CORE}_libretro.so"
make platform=h150101 DEBUG=1 "${FALLBACK_CORE}_libretro.so"
```

Set `DIRECT_CORE` to an existing Makefile core target that emits RGB565 and
implements
`RETRO_ENVIRONMENT_GET_CURRENT_SOFTWARE_FRAMEBUFFER` and an XRGB8888 or
software-framebuffer-incompatible target for `FALLBACK_CORE`. Verify that
each selected downstream core build supports the H150101
`platform`/toolchain settings; this frontend Makefile cannot guarantee that
for every core. If the SDK requires an explicit sysroot override, pass it as a
make command-line assignment (for example, `SYSROOT=/sdk/sysroot`) rather
than exporting it. Copy `picoarch`, both resulting `.so` files, and legally
obtained test content to the device.

## Direct-path run

```sh
DIRECT_CORE=replace_with_rgb565_core_target
DIRECT_CONTENT=/path/to/direct-test-content
PICOARCH_PROFILE=1 ./picoarch "./${DIRECT_CORE}_libretro.so" "$DIRECT_CONTENT" \
	2>&1 | tee /tmp/picoarch-direct.log
grep -F 'SDL direct framebuffer:' /tmp/picoarch-direct.log
grep -E 'PROFILE (sdl|sdl2|main|corecb)' /tmp/picoarch-direct.log
```

Expected evidence:

- exactly one `SDL direct framebuffer: enabled WIDTHxHEIGHT pitch=PITCH RGB565`
  transition for stable gameplay;
- `PROFILE sdl` reports `direct` near the measured FPS and `upload=0` while
  no HUD or menu is displayed;
- image, scaling, input, audio, pause, and resume are correct;
- showing a HUD or entering a menu remains correct even when it temporarily
  requires upload rendering.

## Fallback-path run

```sh
FALLBACK_CORE=replace_with_fallback_core_target
FALLBACK_CONTENT=/path/to/fallback-test-content
PICOARCH_PROFILE=1 ./picoarch "./${FALLBACK_CORE}_libretro.so" "$FALLBACK_CONTENT" \
	2>&1 | tee /tmp/picoarch-fallback.log
grep -F 'SDL direct framebuffer:' /tmp/picoarch-fallback.log || true
grep -E 'PROFILE (sdl|sdl2|main|corecb)' /tmp/picoarch-fallback.log
```

Expected evidence:

- one fallback transition only if a framebuffer was successfully offered and
  the subsequent non-null callback failed exact pointer, pitch, or dimension
  matching;
- `PROFILE sdl` reports `direct=0` and `upload` near FPS;
- rendering, scaling, input, audio, pause, and resume remain correct.

No fallback transition is expected when the core never queries the interface
or the query is rejected before an offer. Pre-offer rejection includes
XRGB8888, rotation, an active HUD, unsupported dimensions, lack of native
RGB565 renderer support, and other eligibility failures. In all these cases,
`direct=0`, upload counts near FPS, and correct rendering are the required
evidence; absence of the transition is not a failure.

## Duplicate-frame checks

For both direct and fallback cores, select content or a reproducible scene
known to issue Libretro duplicate-frame callbacks (`video_refresh(NULL, ...)`)
after ordinary frames. Capture at least ten seconds containing duplicates:

```sh
PICOARCH_PROFILE=1 ./picoarch "./${DIRECT_CORE}_libretro.so" "$DIRECT_CONTENT" \
	2>&1 | tee /tmp/picoarch-direct-duplicate.log
PICOARCH_PROFILE=1 ./picoarch "./${FALLBACK_CORE}_libretro.so" "$FALLBACK_CONTENT" \
	2>&1 | tee /tmp/picoarch-fallback-duplicate.log
```

Observe the display at the duplicate transition and resume of new frames.
Expected: the last valid image remains visible without a black, stale,
partially updated, or unlocked-texture frame; animation resumes correctly.
Direct mode continues to report direct frames when new exact-match frames
arrive, while fallback continues safe upload rendering. A duplicate callback
itself reuses the last valid presentation and need not increment either
per-frame path counter.

## Runtime resolution-change checks

Use a core/game with a reproducible in-session geometry change (for example,
opening/closing a high-resolution mode) and record the before/change/after
interval:

```sh
PICOARCH_PROFILE=1 ./picoarch "./${DIRECT_CORE}_libretro.so" "$DIRECT_CONTENT" \
	2>&1 | tee /tmp/picoarch-resolution-change.log
grep -E 'SDL direct framebuffer:|PROFILE sdl' \
	/tmp/picoarch-resolution-change.log
```

Expected: the new resolution is displayed with correct dimensions and
scaling, with no stale border, old-resolution frame, corruption, or crash.
If the new geometry remains eligible and the callback exactly matches the
offered buffer, direct rendering continues (and an enabled transition may
name the new dimensions after a path transition). If it is not eligible or
does not match, rendering safely uses uploads; the fallback transition is
required only after an offer followed by a non-null mismatch. Switching back
must likewise restore correct output and the appropriate direct or upload
profile counts.

## Profile-off run

```sh
PICOARCH_PROFILE=0 ./picoarch "./${DIRECT_CORE}_libretro.so" "$DIRECT_CONTENT" \
	2>&1 | tee /tmp/picoarch-no-profile.log
if grep -E 'PROFILE (sdl|sdl2|main|corecb)' \
	/tmp/picoarch-no-profile.log; then
	echo 'FAIL: profiling output present'
else
	echo 'PASS: profiling output absent'
fi
```

Expected: `PASS: profiling output absent`. Repeat once with
`PICOARCH_PROFILE` unset if practical; unset, empty, and `0` all disable
periodic profiling.

## Old/new A/B checklist

Use the same device and test conditions for the baseline and direct builds:

- same core build, game, save state or scene, and test duration;
- same CPU clock, governor, battery/charger state, and thermal state;
- same audio configuration, volume, scaling mode, rotation, and HUD state;
- record `PROFILE sdl`, `PROFILE sdl2`, `PROFILE main`, and `PROFILE corecb`
  lines plus any underrun or late-frame messages;
- compare average/max `update`, `corecb video`, `present`, underruns, and late
  frames;
- confirm direct mode lowers `update` and `corecb video` time without
  increasing underruns, late frames, or maximum present time;
- retain both logs and note any visual, input, audio, pause, or resume
  regression.

Device observations must be recorded from an actual run; this document does
not claim H150101 build or runtime results.
