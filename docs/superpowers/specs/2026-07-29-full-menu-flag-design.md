# Full Menu Flag Design

## Goal

Keep the normal in-game main menu compact by hiding `Options`, `Load new
game`, and `About` unless the frontend is started with `--full-menu`.

## Command-Line Behavior

- The default is the compact menu.
- `--full-menu` enables all three extra entries for the lifetime of the
  process.
- The flag is accepted alongside `--language CODE` and is removed from the
  positional arguments before core and content paths are selected.
- Unknown options retain the current command-line error behavior.

Example:

```sh
picoarch --full-menu --language zh_CN core_libretro.so game.rom
```

## Implementation Boundaries

The existing UI command-line parser will expose a boolean `full_menu` result.
`main.c` will pass that setting to the menu layer during initialization. The
menu layer will keep the setting and apply it whenever the in-game main menu
opens.

`Options` will receive a stable main-menu ID so the same `me_enable()` path
can control all three entries:

- `Options`
- `Load new game`
- `About`

No other menu entries or startup core/content selection flows will change.

## Error and Compatibility Behavior

The new flag takes no value. Repeating it is harmless and leaves full-menu
mode enabled. Existing invocations without the flag remain valid and receive
the new compact menu. Existing language precedence and positional argument
handling remain unchanged.

## Verification

Tests will prove:

1. the CLI parser defaults `full_menu` to false;
2. `--full-menu` sets it to true;
3. the flag is removed without shifting or losing core/content arguments;
4. language parsing still works when combined with the new flag;
5. the three entries are disabled by default and enabled together in
   full-menu mode.

Host tests and repository hygiene checks will be run. Target builds remain
dependent on the external SDL2 and H150101 toolchain.
