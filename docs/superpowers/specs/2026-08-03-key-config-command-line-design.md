# Command-Line Key Configuration Design

## Goal

Add the same `--key-config <path>` command-line interface to picoarch and
fba-a320. A caller can select an external remapping file at launch without
copying it into either application's normal configuration directory.

The two applications retain their existing native configuration formats.
A picoarch key configuration therefore uses `binddev` and `bind` entries,
while an fba-a320 key configuration uses `JOY_* integer` entries.

## Command-Line Interface

Both applications accept either spelling:

```text
--key-config /media/mmc/config/remapping/example.cfg
--key-config=/media/mmc/config/remapping/example.cfg
```

The option requires a non-empty path. A missing value is a command-line error
and prevents launch. Existing command-line options and positional core, ROM,
and content arguments retain their current meaning.

## picoarch Behavior

`struct app_args` gains a `key_config_path` member populated by
`app_args_parse()`. Picoarch continues to load its automatic, per-game, or
per-core configuration normally. After input devices have been initialized,
`load_config_keys()` first applies the normal configuration's bindings and
then applies the explicitly named file with the existing
`config_read_keys()` parser. Consequently, bindings in `--key-config` have
the highest priority while non-key settings remain sourced from the normal
configuration.

The existing protection that forces the default menu key to remain bound is
applied after all binding files. An external mapping cannot make the menu
permanently inaccessible.

If the explicit file cannot be opened or read, picoarch prints a warning that
names the path and continues with the normal automatic, game, or core key
configuration. Invalid individual entries retain the current parser behavior.

## fba-a320 Behavior

The existing `getopt_long()` parser gains `--key-config`, and command parsing
occurs before the joypad file is selected. `ConfigJoyDefault()` still installs
compiled defaults first.

When an explicit path is present, fba-a320 loads that file instead of
`$HOME/.fba/joypad.cfg`. When the explicit file cannot be opened, it prints a
warning naming the path and falls back to the existing default
`$HOME/.fba/joypad.cfg`; if that is also unavailable, the compiled mappings
remain active. Successfully opening the explicit file means it is the sole
file-based joypad mapping source; values omitted from it retain compiled
defaults rather than values from the default file.

The file continues to use the existing `JOY_UP`, `JOY_FIRE1`, `JOY_MENU`, and
related integer entries. Invalid lines or values are ignored without
preventing valid entries from loading.

## Compatibility and Scope

This feature does not introduce a shared configuration syntax, configuration
conversion, preset browser, menu entry, or file copying. It does not change
how either application saves configurations. Launching without
`--key-config` preserves existing behavior exactly.

Paths are consumed for the lifetime of startup only and are not persisted to
application configuration files. Both relative and absolute paths are passed
to the operating system unchanged.

## Testing

Picoarch parser tests cover the separated and equals forms, interaction with
existing options and positional arguments, and missing or empty values.
Integration tests cover the normal-then-explicit load order, explicit-file
failure fallback, and final menu-key protection.

fba-a320 tests cover option parsing without consuming the ROM argument,
explicit path selection, fallback to `joypad.cfg` when the explicit path is
unreadable, and the unchanged no-option path. Existing test suites in both
repositories must continue to pass, except for previously documented baseline
failures unrelated to this feature.
