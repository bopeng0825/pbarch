# Task 2 Report: Shared Controller Configuration Matching

## Status

Implemented shared controller configuration matching for the H150101/H150102
SDL2 input driver and retained exact matching for all other input drivers.

## Files

- `libpicofe/input.h`
- `libpicofe/input.c`
- `libpicofe/config_file.c`
- `plat_h150101_sdl2_input.c`
- `tests/test_h15010x_two_player.py`
- `.superpowers/sdd/2026-08-28-h150102-input-and-shared-controller-config/task-2-report.md`

## Implementation

- Added the optional `in_drv_t.config_match` callback and an exact-match default
  installed by the existing driver stub setup.
- Added `in_config_parse_devs()`. It first calls `in_config_parse_dev()` to
  preserve exact/placeholder behavior, then appends unique driver-matched device
  IDs without exceeding the caller's capacity.
- Added the shared SDL2 matcher. Exact names always match; a base
  `h150101-sdl2:NAME` also matches `h150101-sdl2:p2:NAME`; explicit P2 names
  remain exact-only; different controller names do not match.
- Updated `config_read_keys()` to clear and apply each bind section to all
  matched targets. Sequential parsing preserves file order, so a later explicit
  P2 section clears and replaces only P2.

## TDD Evidence

### RED

Command:

```text
C:\Users\shzjt\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe -m unittest tests.test_h15010x_two_player.H15010xTwoPlayerTest.test_base_device_config_matches_identical_player_two tests.test_h15010x_two_player.H15010xTwoPlayerTest.test_shared_matcher_restricts_aliases_to_identical_names tests.test_h15010x_two_player.H15010xTwoPlayerTest.test_multi_device_config_results_are_unique_and_bounded -v
```

Result: exit 1, three expected failures. The failures reported that
`config_match`, `in_config_parse_devs`, and the H150101 SDL2 matcher were absent.

### GREEN

The same command after the minimal implementation returned exit 0:

```text
Ran 3 tests in 0.001s
OK
```

## Verification

- Focused H15010x/H150102 tests: 17 passed, 1 unrelated baseline failure.
  `test_resolution_constants_are_device_specific` expects 640x360, while the
  existing `scale.h` contains 1280x720.
- Full Python suite: 61 passed, 2 unrelated baseline failures. In addition to
  the resolution failure above, `test_audio_stability_defaults` expects an
  existing `wswan_frameskip = auto` override that is absent.
- `git diff --check`: passed in both the `libpicofe` submodule and parent tree.
- Native build was unavailable because this Windows environment has no `make`;
  WSL is installed without a Linux distribution, so it could not provide one.

## Commits

- `libpicofe`: `97f4254 share key config across identical controllers`
- Parent repository: the `share key config across identical controllers`
  commit containing this report and the updated submodule pointer.

## Self-review

- Confirmed the matcher cannot alias an explicit P2 name to P1 or alias
  different controller names.
- Confirmed exact IDs cannot be duplicated when the matcher scan reaches the
  device returned by `in_config_parse_dev()`.
- Confirmed the capacity check bounds every append.
- Confirmed each section clears every selected device before applying binds and
  that later explicit sections therefore affect only their exact device.
- Confirmed existing drivers receive the exact-match stub automatically.

## Concerns

- No device cross-build was possible in the available environment.
- The two full-suite failures are pre-existing and outside Task 2 scope.

## Fix Round 1

Review identified a namespace collision in the shared SDL2 matcher. A configured
name such as `h150101-sdl2:p2:X` could match a P1 controller literally named
`p2:X` and then alias a P2 controller named `p2:p2:X`. The matcher now keeps its
exact-match path, but rejects any configured suffix beginning with `p2:` before
performing base-to-P2 alias matching.

The executable Python guards were strengthened to check:

- explicit P2 rejection appears before the alias suffix comparison;
- different names remain governed by a full suffix `strcmp`;
- result capacity guards and uniqueness checks precede alias insertion;
- each config section clears all targets before applying bindings, within the
  single file-order section loop.

### Fix Round RED

Command:

```text
C:\Users\shzjt\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe -m unittest tests.test_h15010x_two_player.H15010xTwoPlayerTest.test_explicit_player_two_config_never_aliases -v
```

Result: exit 1. The test failed because the explicit configured-suffix
`strncmp(..., "p2:", 3)` guard was absent from the matcher.

### Fix Round GREEN

The same command after the matcher change returned exit 0:

```text
Ran 1 test in 0.001s
OK
```

Focused amended-file verification:

```text
C:\Users\shzjt\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe -m unittest tests.test_h15010x_two_player -v
Ran 15 tests in 0.010s
OK
```

The deferred `bind_analog` first-target behavior was intentionally unchanged;
the analog slot is global and cannot represent multiple target IDs.

## Final Review Fix Wave

### Changes

- Moved `in_drv_t.config_match` after the existing `defbinds` and `pdata`
  fields, preserving the mapping of downstream positional initializers.
- Extracted the pure shared SDL2 name matcher into
  `plat_h150101_sdl2_config.h`. The production driver and behavioral harness
  now compile the same implementation.
- Added `tests/h15010x_config_harness.c` and a Python compile/run test. On a
  capable CI host, the harness links the real `libpicofe/input.c` and
  `libpicofe/config_file.c`, registers two real input devices, and verifies:
  base-to-both matching, explicit-P2-only replacement, different-name
  isolation, unique IDs, and capacity limits.
- Updated the English design and the `ANALOG_BINDS` loader comment: `in_adev`
  stores one device ID, so `bind_analog` intentionally remains first-target and
  single-owner. Shared analog ownership requires a separate redesign.

Files added or changed in this wave:

- `libpicofe/input.h`
- `libpicofe/config_file.c`
- `plat_h150101_sdl2_config.h`
- `plat_h150101_sdl2_input.c`
- `tests/h15010x_config_harness.c`
- `tests/test_h15010x_two_player.py`
- `docs/superpowers/specs/2026-08-28-h150102-input-and-shared-controller-config-design.md`
- `.superpowers/sdd/2026-08-28-h150102-input-and-shared-controller-config/task-2-report.md`

### Compatibility RED

```text
C:\Users\shzjt\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe -m unittest tests.test_h15010x_two_player.H15010xTwoPlayerTest.test_config_match_preserves_positional_initializer_compatibility tests.test_h15010x_two_player.H15010xTwoPlayerTest.test_shared_config_behavior_harness -v
```

Result: exit 1. The positional-compatibility test reported that `pdata`
followed `config_match`. The behavioral harness was discovered and skipped with
`no C compiler available for behavioral harness`.

### Compatibility GREEN and Local Harness Evidence

The same command after moving the field returned:

```text
Ran 2 tests in 0.027s
OK (skipped=1)
```

The compatibility test passed. The behavioral test again reported the explicit
local skip because `cc`, `gcc`, and `clang` are all absent. Its compile/run path
remains active for capable CI environments.

Focused H15010x verification:

```text
C:\Users\shzjt\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe -m unittest tests.test_h15010x_two_player -v
Ran 17 tests in 0.029s
OK (skipped=1)
```

`git diff --check` passed in the parent repository and `libpicofe` submodule.

Final-wave nested commit:

- `libpicofe`: `257f34f preserve input driver initializer layout`
