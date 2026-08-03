# Key Configuration Command-Line Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add `--key-config <path>` to picoarch and fba-a320 so launchers can select an external native-format key mapping with highest priority and safe fallback.

**Architecture:** Each frontend parses and retains an optional path during normal command-line processing. Picoarch applies the selected file as a key-only overlay after its normal CFG; fba-a320 selects the explicit joypad file after compiled defaults and falls back to its normal `joypad.cfg` only when the explicit file cannot be read.

**Tech Stack:** C11/C99-style picoarch frontend code, C++ fba-a320 SDL frontend code, existing C assertion tests, Python unittest contract tests, Git.

## Global Constraints

- Accept both `--key-config PATH` and `--key-config=PATH`.
- Reject missing or empty option values as command-line errors.
- Keep picoarch and fba-a320 native CFG formats separate.
- Do not persist the command-line path or copy its file.
- Without the option, preserve existing behavior.
- An unreadable explicit file warns and falls back without preventing game launch.
- Picoarch must retain its forced default menu binding after all overlays.

---

### Task 1: Picoarch argument parsing

**Files:**
- Modify: `ui_config.h`
- Modify: `ui_config.c`
- Modify: `tests/test_ui_config.c`
- Modify: `main.c`

**Interfaces:**
- Produces: `struct app_args::key_config_path` as a borrowed `const char *` into `argv`.
- Consumes: existing `app_args_parse(int argc, char **argv, struct app_args *out)`.

- [ ] **Step 1: Write the failing parser assertions**

Add arrays and assertions to `tests/test_ui_config.c` covering separated and
equals syntax, preservation of core/content positional arguments, missing
value, and empty equals value:

```c
char *key_cfg_argv[] = {
	"picoarch", "--key-config", "/media/mmc/remap/gba.cfg",
	"core.so", "game.rom"
};
char *key_cfg_equals_argv[] = {
	"picoarch", "--key-config=/media/mmc/remap/gba.cfg", "core.so"
};
char *missing_key_cfg_argv[] = { "picoarch", "--key-config" };
char *empty_key_cfg_argv[] = { "picoarch", "--key-config=" };

assert(app_args_parse(5, key_cfg_argv, &args) == 0);
assert(strcmp(args.key_config_path, "/media/mmc/remap/gba.cfg") == 0);
assert(strcmp(args.core_path, "core.so") == 0);
assert(strcmp(args.content_path, "game.rom") == 0);
assert(app_args_parse(3, key_cfg_equals_argv, &args) == 0);
assert(strcmp(args.key_config_path, "/media/mmc/remap/gba.cfg") == 0);
assert(app_args_parse(2, missing_key_cfg_argv, &args) == -1);
assert(app_args_parse(2, empty_key_cfg_argv, &args) == -1);
```

- [ ] **Step 2: Run the parser test and verify RED**

Run the repository's existing `test_ui_config.c` build/run command as recorded
in its test wrapper or Makefile. Expected: compilation fails because
`struct app_args` has no `key_config_path`, or the new assertions fail because
the option is unknown.

- [ ] **Step 3: Implement minimal parsing**

Add `const char *key_config_path;` to `struct app_args`. In
`app_args_parse()`, parse both forms using the same validation rules as
`--language`. Update the usage string in `main.c` to include
`[--key-config PATH]`.

- [ ] **Step 4: Run the parser test and verify GREEN**

Run the same focused test. Expected: exit 0 and no assertion output.

- [ ] **Step 5: Commit picoarch parsing**

```bash
git add ui_config.h ui_config.c tests/test_ui_config.c main.c
git commit -m "add key config command line option"
```

### Task 2: Picoarch key-only overlay and fallback

**Files:**
- Modify: `main.c`
- Modify: `main.h`
- Modify: `tests/check_ui_literals.py`
- Modify: `tests/test_check_ui_literals.py`

**Interfaces:**
- Consumes: `args.key_config_path` from Task 1 and existing `config_read_keys(const char *)`.
- Produces: `load_config_keys(const char *key_config_path)` applying normal bindings, optional overlay, then protected menu binding.

- [ ] **Step 1: Write failing integration guards**

Add checker functions and unittest cases that assert:

```python
self.assertTrue(key_config_overlay_is_last_before_menu_protection(main_source))
self.assertTrue(key_config_open_failure_preserves_normal_keys(main_source))
```

The checker must verify `main()` passes `args.key_config_path`, normal
`config_read_keys(config)` occurs before explicit-file parsing, an `fopen()`
failure logs the selected path without returning from normal startup, and the
forced `EACTION_MENU` loop occurs after both parsers.

- [ ] **Step 2: Run the focused guards and verify RED**

Run:

```bash
python -m unittest tests.test_check_ui_literals -v
```

Expected: the two new tests fail because the external path is not consumed.

- [ ] **Step 3: Implement the overlay**

Change the loader signature to:

```c
void load_config_keys(const char *key_config_path);
```

Keep the current normal CFG allocation and parsing. Add a focused helper that
reads the explicit file into a NUL-terminated buffer, invokes
`config_read_keys()`, and reports `PA_WARN("Couldn't load key config %s\n",
key_config_path)` on open/read/allocation failure. Call it after normal key
parsing and before the existing forced-menu loop. Call
`load_config_keys(args.key_config_path)` from `main()` and use `NULL` where
the menu content-switch path reloads normal keys.

- [ ] **Step 4: Run focused and full picoarch tests**

Run the focused unittest, then the repository's complete Python/C test
commands. Expected: new tests pass; only previously documented unrelated
baseline failures may remain. Run `git diff --check`.

- [ ] **Step 5: Commit picoarch overlay**

```bash
git add main.c main.h tests/check_ui_literals.py tests/test_check_ui_literals.py
git commit -m "load command line key config overlay"
```

### Task 3: fba-a320 argument parsing and file selection

**Files:**
- Modify: `D:/PB/git/fba-a320/src/sdl-dingux/main.cpp`
- Modify: `D:/PB/git/fba-a320/src/sdl-dingux/config.cpp`
- Modify: `D:/PB/git/fba-a320/src/sdl-dingux/sdl_menu.h`
- Create: `D:/PB/git/fba-a320/tests/test_key_config_command_line.py`

**Interfaces:**
- Produces: a bounded startup path buffer populated by `parse_cmd()`.
- Produces: `int ConfigJoyLoad(const char *explicit_path)`.
- Consumes: existing `ConfigJoyDefault()` and native `JOY_* integer` parser.

- [ ] **Step 1: Write failing FBA contract tests**

Create Python source-level contract tests that verify the long option exists,
requires an argument, copies it with explicit NUL termination, parsing happens
before `ConfigJoyLoad()`, and the selected path is passed to that loader. Add
tests requiring the loader to try the explicit path first and the default
`joypad.cfg` only after explicit-open failure.

- [ ] **Step 2: Run the new tests and verify RED**

Run:

```bash
python -m unittest tests.test_key_config_command_line -v
```

Expected: failures identify the missing option, signature, and load order.

- [ ] **Step 3: Implement FBA parsing**

Add `{"key-config", required_argument, 0, 'k'}` to `long_opts`. Copy `optarg`
into a `MAX_PATH` startup buffer using bounded copy and NUL termination. Move
`parse_cmd(argc, argv, path)` before joypad file selection while retaining
`ConfigGameDefault()` and `ConfigJoyDefault()` before parsing.

- [ ] **Step 4: Implement explicit-file fallback**

Refactor `ConfigJoyLoad()` to accept `const char *explicit_path`. Attempt the
explicit path when non-empty. On failure, print a warning naming it and then
attempt the existing `szAppHomePath/joypad.cfg`. If neither opens, retain
compiled defaults. Once an explicit file opens, parse only it; do not merge
the default file.

- [ ] **Step 5: Run focused and full FBA tests**

Run the new unittest and `python -m unittest discover -s tests -p
'test_*.py' -v`. Run the available native logic tests or build target, followed
by `git diff --check`. Expected: all feature tests pass and existing behavior
tests remain green.

- [ ] **Step 6: Commit FBA support**

```bash
git add src/sdl-dingux/main.cpp src/sdl-dingux/config.cpp \
  src/sdl-dingux/sdl_menu.h tests/test_key_config_command_line.py
git commit -m "add key config command line option"
```

### Task 4: Cross-repository verification and documentation

**Files:**
- Modify: `README.md`
- Modify: `D:/PB/git/fba-a320/README.md`

**Interfaces:**
- Consumes: both completed command-line implementations.
- Produces: user-facing examples and final verification evidence.

- [ ] **Step 1: Add launch examples**

Document each program's native format and example invocation:

```bash
picoarch --key-config /media/mmc/config/remapping/gba.cfg CORE ROM
fba --key-config /media/mmc/config/remapping/arcade.cfg ROM
```

- [ ] **Step 2: Verify help/usage and repository diffs**

Confirm picoarch usage includes the option and fba-a320 `getopt_long()` accepts
both separated and equals forms. Run both full test suites, `git diff --check`
in both repositories, and inspect `git status --short` for unintended files.

- [ ] **Step 3: Commit documentation separately in each repository**

```bash
git add README.md
git commit -m "document key config option"
```
