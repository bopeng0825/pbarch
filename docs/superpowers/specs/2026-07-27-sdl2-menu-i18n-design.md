# SDL2 Menu Background, TTF, and Internationalization Design

## Goal

Add an SDL2-only menu renderer that uses a full-screen image background and
resolution-aware TTF text, and localize picoarch-owned menu text into English,
Simplified Chinese, and Traditional Chinese. The implementation must remain
responsive on low-performance handheld hardware and make future languages easy
to add.

## Scope

- Enable the new renderer only for `USE_SDL2`.
- Translate picoarch-owned menu entries, prompts, status text, and button help.
- Do not translate option names or values supplied dynamically by libretro
  cores. The renderer must still display UTF-8 text supplied by a core.
- Keep existing SDL1, FunKey, and other platform behavior unchanged.
- Replace the captured/darkened game image with the menu background.

## Selected Approach

Use SDL_ttf to rasterize UTF-8 strings on demand and cache the rendered
surfaces. Loading, font rasterization, image conversion, and scaling are not
performed in the steady-state menu frame loop. Each frame copies a prepared
RGB565 background and blits cached text and the selection decoration.

This approach is preferred over rendering TTF every frame because it avoids
repeated FreeType work. It is preferred over a prebuilt bitmap atlas because
it preserves scalable text, UTF-8 support, and straightforward language
extension.

## Resources and Licensing

- Store the menu background at `skin/background.png`.
- The initial background is a small, solid-color PNG. At initialization it is
  decoded, scaled once to the logical menu dimensions, converted to RGB565,
  and cached.
- If the PNG is absent, invalid, or cannot be allocated, fill the RGB565
  background cache with the same built-in color.
- Use a UI-character subset of Noto Sans Mono CJK as the bundled TTF.
- Include the upstream SIL Open Font License 1.1 beside the font.
- Include a developer-side font subsetting script and documented source URL.
  Adding language strings requires regenerating the subset before packaging.

## Language Model

Define stable text identifiers and compile-time UTF-8 tables for:

- `en` (default)
- `zh_CN` (Simplified Chinese)
- `zh_TW` (Traditional Chinese)

Lookups always return a valid string. A missing translation falls back to the
English entry. Adding a language means adding one table and registering its
canonical code; no renderer changes are required.

Accept `zh-CN` and `zh-TW` as input aliases and normalize them to `zh_CN` and
`zh_TW`. Unknown or empty codes emit one warning and select `en`.

Only frontend-owned strings use text identifiers. Dynamic filenames, core
names, option descriptions, option values, and formatted numeric values remain
runtime UTF-8 strings.

## Configuration and Command-Line Override

Read the global UI setting before menu initialization from:

```text
~/.picoarch/ui.cfg
```

The supported setting is:

```ini
language = zh_CN
```

Support both command forms:

```text
picoarch --language zh_TW core_libretro.so game.rom
picoarch --language=zh_TW core_libretro.so game.rom
```

The precedence is:

```text
command line > ui.cfg > en
```

A command-line override applies only to the current process and is never
written back. Language is global UI state rather than a core/game setting.
Options are parsed before positional core and content arguments, so the
language is active in core and content selection screens.

## Font Sizing and Layout

Calculate the main font pixel size from the logical menu height:

```text
main_px = clamp(round(menu_height / 24), 12, 32)
small_px = round(main_px * 0.8)
```

This produces:

| Logical height | Main font |
| --- | ---: |
| 240 | 12 px |
| 480 | 20 px |
| 720 | 30 px |

Use SDL_ttf metrics for line height and final string width. Derive margins,
selection height, pagination, columns, centering, truncation, and wrapping
from runtime metrics rather than the old compile-time `MENU_X2` sizes.

Use UTF-8-aware iteration. ASCII occupies one layout cell and CJK wide
characters occupy two cells for coarse menu calculations; pixel-accurate
placement uses `TTF_SizeUTF8`. No layout calculation may use byte-counting
`strlen()` as a character width.

## Rendering and Cache Lifetime

Initialize SDL_ttf after SDL platform initialization and before menu
initialization. Open main and small font handles once. Prepare the background
once for the current logical menu dimensions.

Cache text using this key:

```text
font role + RGB565 color + UTF-8 bytes
```

Each entry owns one converted/blittable surface and records its memory size
and last-use counter. Enforce a fixed byte budget with least-recently-used
eviction. Clear the cache when the language, font size, or logical resolution
changes. Dynamic strings therefore remain bounded even when core values
change.

The steady-state frame path is:

1. Copy the prepared RGB565 background into the menu framebuffer.
2. Draw the selection decoration.
3. Blit cached text surfaces.
4. Present through the existing SDL2 menu upload path.

## Failure Behavior

- Background load failure: use the built-in solid RGB565 color.
- TTF initialization or font load failure: warn once and use the existing
  embedded bitmap font in English.
- Chinese requested without a usable CJK font: warn once, select English, and
  retain a usable menu.
- Missing translation: return the English string for that identifier.
- Malformed global configuration or unsupported language: ignore the bad
  value, warn once, and select English.
- Allocation failure in the text cache: render that draw with the fallback
  path when possible and keep the process usable.

## Testing

Add host-side unit tests for:

- language normalization, aliases, missing translations, and English fallback;
- global configuration parsing and command-line precedence;
- 240p, 480p, and 720p font-size calculations;
- UTF-8 character iteration, width accounting, and safe truncation;
- cache accounting and least-recently-used eviction;
- missing font and missing background fallback decisions.

Use test-first development for each behavior. Verify each new test fails for
the intended missing behavior before adding production code.

Run the existing host tests, a debug SDL2 Unix build, and the H150101
cross-build when its toolchain is available. Render representative menu
screens in `en`, `zh_CN`, and `zh_TW` at 640x480, plus a 1280x720 sizing
fixture, and inspect them for clipping, incorrect centering, overlap, missing
glyphs, and background fallback.

## Repository and Delivery

Develop on `codex/sdl2-menu-i18n` in an isolated worktree. The original
checkout contains an unrelated uncommitted change to `plat_h150101.c`; do not
modify, move, stage, or overwrite that change.

Document the asset origin, font license, language codes, configuration path,
command-line syntax, and font-subset regeneration command in the final change.
