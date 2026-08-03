# picoarch - a libretro frontend designed for small screens and low power

picoarch uses libpicofe and SDL to create a small frontend to libretro cores. It's designed for small (320x240 2.0-2.4") screen, low-powered devices like the Trimui Model S (PowKiddy A66) and FunKey S.

## Running

picoarch can be run by specifying the core library and the content to run:

```
./picoarch /path/to/core_name_libretro.so /path/to/game.gba
```

If you do not specify core or content, picoarch will have you select a core from the current directory and content using the built-in file browser.

The compact menu used by default hides Options, Load new game, and About.
Pass `--full-menu` to show those entries:

```
./picoarch --full-menu /path/to/core_name_libretro.so /path/to/game.gba
```

To overlay key bindings from a picoarch-format CFG file, use either
`--key-config PATH` or `--key-config=PATH`:

```
picoarch --key-config /media/mmc/config/remapping/gba.cfg CORE ROM
```

The file uses the same key-binding format as picoarch's saved CFG files; it is
not compatible with FBA's `joypad.cfg` format. If it cannot be read, picoarch
warns and continues with its normal configuration and required menu binding.

### Menu language and SDL2 skin

The frontend menu defaults to English. To select a language persistently,
create `~/.picoarch/ui.cfg`:

```
language = en
```

The supported canonical codes are `en`, `zh_CN`, and `zh_TW`. The aliases
`zh-CN` and `zh-TW` are also accepted. An unrecognized code falls back to
English, as does an empty translated catalog cell. A command-line selection
has highest precedence:

```
./picoarch --language zh_TW /path/to/core_libretro.so /path/to/content
```

The effective order is `--language CODE`, then `~/.picoarch/ui.cfg`, then
English. Translation covers picoarch's frontend menu. Options and help text
provided by a libretro core are displayed unchanged.

SDL2 builds use the files in `skin/` for the menu font and background.
H150101 currently has no package target, so direct deployments must copy the
complete `skin/` directory beside `picoarch`; the runtime platform glue
resolves that installed skin location with `plat_get_skin_dir()`.

## Building

The frontend can currently be built for the TrimUI Model S, FunKey S, and Linux (useful for testing and debugging).

First, fetch the repo with submodules:

```
git clone --recurse-submodules https://git.crowdedwood.com/picoarch
```

### Linux instructions

To build picoarch itself, you need libSDL 1.2, libpng, and libasound. Different cores may need additional dependencies.

After that, `make` builds picoarch and all supported cores into this directory.

### TrimUI instructions

To build for TrimUI, you need to set up the [toolchain](https://git.crowdedwood.com/trimui-toolchain/about/) first.

To build generic binaries:

```
make platform=trimui
```

If you want to build for MinUI, you need to install [libmmenu](https://github.com/shauninman/libmmenu) into the toolchain. Then:

```
make platform=trimui MINUI=1
```

`MINUI=1` will change save/config/system paths to match MinUI standards. If you just want to include mmenu, you can run:

```
make platform=trimui MMENU=1
```

To build for distribution:

```
make platform=trimui dist-gmenu
make platform=trimui MINUI=1 dist-minui
```

These will output a directory structure that can be moved onto the SD card into `pkg/gmenunx` or `pkg/MinUI`.

Or run

```
make platform=trimui picoarch.zip
```

To build a .zip file ready for SD card.

### FunKey S instructions

To build for FunKey S, you need a toolchain first, following [instructions](https://doc.funkey-project.com/developer_guide/tutorials/build_system/build_program_using_sdk/) on the FunKey wiki.

To build generic binaries:

```
make platform=funkey-s
```

To build a specific core as .opk file:

```
make platform=funkey-s picoarch-gambatte.opk
```

Or run

```
make platform=funkey-s picoarch-funkey-s.zip
```

To build a .zip file containing all .opk files.


### Other build options

To debug:

```
make DEBUG=1
```

To build a specific supported core:

```
make gpsp_libretro.so
```

To clean a core so it will be built again:

```
make clean-gpsp
```

To completely clean the repo (will delete, pull, and patch all core repos from scratch)

```
make distclean
```

To build profiles for profile-guided optimization:

```
make PROFILE=GENERATE
```

To apply the generated profiles:

```
make PROFILE=APPLY
```

PGO can give noticeable speed improvements with some emulators.

### Updating menu catalogs and assets

After editing `locales/ui.tsv`, regenerate the C catalog:

```
python3 tools/gen_ui_catalog.py
```

The bundled `skin/picoarch-ui.ttf` is a UI-only Regular-weight subset of
Noto Sans Mono CJK SC. Rebuild it with the pinned fonttools 4.63.0 and the
matching `Sans/LICENSE`:

```
python3 -m pip install fonttools==4.63.0
python3 tools/subset_ui_font.py \
  --source path/to/NotoSansMonoCJKsc-VF.ttf \
  --license path/to/LICENSE
python3 tools/subset_ui_font.py --check
```

The script verifies the pinned source SHA-256, instantiates its Regular
(`wght=400`) face, then subsets it. It includes all catalog characters,
printable ASCII, and U+00A0, renames the modified family to `Picoarch UI`,
and regenerates the solid RGB `(24, 28, 24)` background.

The source used for the checked-in subset is the official notofonts
Sans2.004 [NotoSansMonoCJKsc variable TTF pinned at commit
`165c01b46ea533872e002e0785ff17e44f6d97d8`](https://raw.githubusercontent.com/notofonts/noto-cjk/165c01b46ea533872e002e0785ff17e44f6d97d8/Sans/Variable/TTF/Mono/NotoSansMonoCJKsc-VF.ttf),
SHA-256
`9fe57a71eb48e50cccb77903123d08857edefcf289c7b309462c4c3d82208126`.
The unmodified `skin/OFL.txt` came from the matching official
[`13_NotoSansMonoCJKsc.zip`](https://github.com/notofonts/noto-cjk/releases/download/Sans2.004/13_NotoSansMonoCJKsc.zip),
archive SHA-256
`e252c39994f8a278676507600a955663c23c24a7827dc63a4300b2f7b427cd5d`.
The font and derivative subset are distributed under the SIL Open Font
License Version 1.1 in `skin/OFL.txt`.

## Notes on cores

In order to make development and testing easier, the Makefile will pull and build supported cores.

You will have to make changes when adding a core, since TrimUI is not a supported libretro platform. picoarch has a `patches/` directory containing needed changes to make cores work well in picoarch. Patches are applied in order after checking out the repository. 

At a minimum, you need to add a `platform=trimui` section to the core Makefile if you are building for trimui.

Some features and fixes are also included in `patches` -- it would be best to try to upstream them.

picoarch keeps the running core name in a global variable. This is used to override defaults and core settings to work more nicely within picoarch. Overrides based on core name are kept in `overrides/` and referenced in `overrides.c`. These are used to:

- Shorten core option text and change defaults for small screen / low power devices
- Rename buttons to match the core's system
- Reference frameskip core options to make fast-forward faster
- Display extra options or hide unnecessary options
