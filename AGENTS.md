# Repository Guidelines

## Project Structure & Module Organization

This repository contains `picoarch`, a small C libretro frontend for low-power handheld devices. Top-level `*.c` and `*.h` files hold the frontend core, menu, content loading, scaling, patching, and platform glue. Platform-specific entry points live in files such as `plat_linux.c`, `plat_trimui.c`, `plat_funkey.c`, and `plat_h150101.c`.

Bundled helper code is under `libpicofe/` and `libretro-common/`. FunKey-specific UI code is in `funkey/`. Per-core option and behavior overrides are in `overrides/`, while downstream core patches are grouped by core in `patches/`. User-facing platform notes are in `README.md`, `README.trimui.md`, and `README.funkey-s.md`.

## Build, Test, and Development Commands

- `make`: builds `picoarch` and supported cores for the default `platform=unix`.
- `make DEBUG=1`: builds with debug symbols and lower optimization for local debugging.
- `make platform=trimui`: cross-builds for TrimUI; requires the TrimUI toolchain.
- `make platform=funkey-s`: cross-builds for FunKey S; requires the FunKey SDK/toolchain.
- `make gpsp_libretro.so`: builds one supported core by target name.
- `make clean`: removes frontend build products.
- `make clean-all`: cleans frontend and all configured cores.
- `make distclean`: fully resets fetched core trees and reapplies patches on the next build.

Run locally with `./picoarch /path/to/core_libretro.so /path/to/content`.

## Coding Style & Naming Conventions

Use C in the existing style: tabs for indentation, braces on the same line for functions and control blocks, `snake_case` for functions and variables, and uppercase names for macros and enum constants. Keep public declarations in matching headers. Keep platform conditionals localized to `plat_*.c` or Makefile platform sections where possible.

## Testing Guidelines

There is no dedicated first-party test harness in the root project. Treat successful builds as the minimum validation, preferably with `make DEBUG=1` for local Unix changes and the relevant `platform=...` build for device-specific changes. When touching bundled common utilities, check whether an applicable test exists under `libretro-common/test/` or `libretro-common/samples/`.

## Commit & Pull Request Guidelines

Recent commits use short, imperative or descriptive subjects such as `sdl2 input test` and `vsync`. Keep subjects specific and under roughly 72 characters. In pull requests, include the target platform, commands run, affected cores or overrides, and any device-tested behavior. Link related issues and include screenshots or short videos for menu, scaling, or input changes.

## Agent-Specific Instructions

Avoid broad refactors in hot paths such as video, input, scaling, and core callbacks unless the request requires them. Do not edit submodule code casually; prefer patches under `patches/<core>/` for downstream core changes.
