# Scale Command-Line Override Design

## Goal

Allow launchers to select proportional scaling or full-screen stretching for the current picoarch process without editing or automatically saving the frontend configuration.

## Command-Line Interface

Add a `--scale` option in both supported argument forms:

```text
--scale scaled
--scale=scaled
--scale stretched
--scale=stretched
```

`scaled` selects `SCALE_SIZE_SCALED` and preserves the content aspect ratio. `stretched` selects `SCALE_SIZE_STRETCHED` and fills the complete screen.

The effective launch default is `scaled`. If `--scale` is omitted, picoarch selects `SCALE_SIZE_SCALED` for this run even when a loaded global or game configuration contains another display mode. If the option occurs more than once, the last valid value wins.

Missing, empty, or unsupported values are command-line errors. picoarch prints its usage text and exits with a nonzero status. The usage text will document `--scale scaled|stretched`.

## Data Flow and Precedence

`app_args_parse()` parses the textual value into a display-mode field in `struct app_args`. The field defaults to the scaled mode when the argument structure is initialized.

Startup retains the existing initialization order:

1. `set_defaults()` establishes frontend defaults.
2. `load_config()` reads global and game configuration.
3. The parsed launch scale is assigned to `scale_size`, overriding the loaded value.
4. Core and content startup continue normally.

This placement gives the launch choice higher precedence than persisted configuration while leaving the existing configuration parser unchanged. The command-line handler does not call configuration-writing functions. A user may still change the mode from the menu during the running session.

## Scope

The first version accepts only `scaled` and `stretched`. Native, cropped, manual, filter, and zoom controls remain menu/configuration features. No scaler implementation or rendering algorithm changes are required.

## Testing

Extend the command-line parser tests to cover:

- separated and equals forms for both accepted values;
- the scaled default when the option is absent;
- repeated options using the last value;
- missing, empty, and unsupported values returning an error;
- positional core and content arguments working before or after the scale option.

Add a source-level or focused startup test, consistent with the existing test suite, to verify that the parsed scale is applied after `load_config()`. Run the applicable unit tests and `git diff --check` before completion.
