# mednafen_wswan Audio Stability Defaults

## Goal

Reduce intermittent audio underruns when running `mednafen_wswan`, especially
on low-power handhelds, by selecting less demanding core defaults. The change
applies to `mednafen_wswan` on every supported platform and does not affect
other cores.

## Scope

Change only the option overrides in `overrides/mednafen_wswan.h`:

- Use 16-bit color output by default.
- Enable the core's 60 Hz display mode by default.
- Use automatic frameskip by default.
- Use a 22050 Hz audio sample rate by default.

Existing per-core and per-game configuration files continue to take
precedence. The new values apply to fresh configurations and after restoring
defaults.

## Design

Set `default_value` on the existing override entries. Do not add platform
conditionals, change the frontend-wide audio buffer, or modify the shared SDL
audio callback. This keeps the behavior local to `mednafen_wswan` and avoids
adding latency or changing audio behavior for other cores.

The selected defaults reduce rendering and resampling work. Automatic
frameskip lets the core respond to frontend audio-buffer pressure instead of
allowing audio starvation to accumulate.

## Compatibility and Failure Handling

The override system resolves a requested default against the values advertised
by the loaded core. Tests must confirm that the chosen strings match the
current core option values. If an installed core version does not advertise a
chosen value, the existing option lookup falls back safely rather than
introducing a new runtime failure.

## Verification

- Add or extend a focused source-level test that checks the four defaults.
- Run the applicable test suite.
- Run `git diff --check`.
- Build for H150101 when its cross-toolchain is available; otherwise report
  that device-build verification remains pending.

Device acceptance criteria:

- Fresh or reset `mednafen_wswan` configuration shows the four selected values.
- Existing saved configurations remain unchanged.
- Audio underruns are reduced during representative WonderSwan gameplay.
- Other cores retain their current defaults.
