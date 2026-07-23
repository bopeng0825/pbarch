# H150101 SDL Direct Framebuffer Validation

Run this checklist on an H150101 device (or its matching cross-build host).
Do not treat a successful build as device validation.

## Build and deploy

Set the existing H150101 toolchain variables as required by the local SDK,
then build the frontend and the two cores selected for the A/B run:

```sh
export SYSROOT=/path/to/h150101/sysroot
export CROSS_COMPILE=/path/to/toolchain/bin/mipsel-linux-
make platform=h150101 DEBUG=1 picoarch
make platform=h150101 DEBUG=1 compatible_core_libretro.so
make platform=h150101 DEBUG=1 fallback_core_libretro.so
```

Replace the two core targets with an RGB565 core that implements
`RETRO_ENVIRONMENT_GET_CURRENT_SOFTWARE_FRAMEBUFFER` and an XRGB8888 or
software-framebuffer-incompatible core. Copy `picoarch`, both `.so` files,
and legally obtained test content to the device without renaming the core
libraries.

## Direct-path run

```sh
PICOARCH_PROFILE=1 ./picoarch ./compatible_core_libretro.so ./game.rom \
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
PICOARCH_PROFILE=1 ./picoarch ./fallback_core_libretro.so ./game.rom \
	2>&1 | tee /tmp/picoarch-fallback.log
grep -F 'SDL direct framebuffer: unavailable, using texture upload' \
	/tmp/picoarch-fallback.log
grep -E 'PROFILE (sdl|sdl2|main|corecb)' /tmp/picoarch-fallback.log
```

Expected evidence:

- one fallback transition if the core requested a direct framebuffer but
  returned incompatible frame data;
- `PROFILE sdl` reports `direct=0` and `upload` near FPS;
- rendering, scaling, input, audio, pause, and resume remain correct.

A core that never requests the software framebuffer can legitimately use the
upload path without printing the fallback transition. In that case,
`direct=0`, upload counts, and correct rendering are the evidence.

## Profile-off run

```sh
PICOARCH_PROFILE=0 ./picoarch ./compatible_core_libretro.so ./game.rom \
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
