# Full Menu Flag Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Hide `Options`, `Load new game`, and `About` by default and restore all three with `--full-menu`.

**Architecture:** Extend the existing `app_args` parser with one boolean, then pass it through `main.c` to a menu-layer setter. The menu applies one stored process-wide setting to the three identified main-menu entries whenever the in-game menu opens.

**Tech Stack:** C99, existing picoarch menu framework, host C unit tests, Python source integration tests.

## Global Constraints

- The new flag is exactly `--full-menu` and takes no value.
- Existing `--language` precedence and core/content positional parsing must not change.
- Startup core and content selection must not change.
- Only `Options`, `Load new game`, and `About` are controlled by the flag.
- Work on the user-authorized `main` checkout and preserve unrelated changes.

---

### Task 1: Parse the Full-Menu Flag

**Files:**
- Modify: `ui_config.h`
- Modify: `ui_config.c`
- Modify: `tests/test_ui_config.c`

**Interfaces:**
- Consumes: existing `int app_args_parse(int argc, char **argv, struct app_args *out)`
- Produces: `struct app_args.full_menu`, an `int` that is zero by default and one after `--full-menu`

- [ ] **Step 1: Add failing CLI assertions**

Add these argument arrays and assertions to `tests/test_ui_config.c`:

```c
char *full_menu_argv[] = {
	"picoarch", "--full-menu", "--language", "zh_CN",
	"core.so", "game.rom"
};
char *repeated_full_menu_argv[] = {
	"picoarch", "--full-menu", "--full-menu", "core.so"
};

assert(app_args_parse(5, argv1, &args) == 0);
assert(args.full_menu == 0);

assert(app_args_parse(6, full_menu_argv, &args) == 0);
assert(args.full_menu == 1);
assert(strcmp(args.language_override, "zh_CN") == 0);
assert(strcmp(args.core_path, "core.so") == 0);
assert(strcmp(args.content_path, "game.rom") == 0);

assert(app_args_parse(4, repeated_full_menu_argv, &args) == 0);
assert(args.full_menu == 1);
assert(strcmp(args.core_path, "core.so") == 0);
```

- [ ] **Step 2: Run the test and verify RED**

Run:

```sh
cc -std=c99 -Wall -Wextra -I. tests/test_ui_config.c \
  ui_config.c ui_language.c -o tests/test_ui_config
./tests/test_ui_config
```

Expected: compilation fails because `struct app_args` has no `full_menu`.

- [ ] **Step 3: Implement minimal parsing**

Add the field to `struct app_args` in `ui_config.h`:

```c
int full_menu;
```

Add this branch before the generic unknown-option branch in
`app_args_parse()`:

```c
} else if (strcmp(arg, "--full-menu") == 0) {
	out->full_menu = 1;
```

`memset(out, 0, sizeof(*out))` supplies the required default.

- [ ] **Step 4: Run the test and verify GREEN**

Run:

```sh
cc -std=c99 -Wall -Wextra -I. tests/test_ui_config.c \
  ui_config.c ui_language.c -o tests/test_ui_config
./tests/test_ui_config
```

Expected: exit 0 with no compiler warnings.

- [ ] **Step 5: Commit parser behavior**

```sh
git add ui_config.h ui_config.c tests/test_ui_config.c
git commit -m "parse full menu option"
```

### Task 2: Apply Compact Main-Menu Visibility

**Files:**
- Modify: `main.c`
- Modify: `menu.h`
- Modify: `menu.c`
- Create: `tests/test_full_menu_visibility.py`

**Interfaces:**
- Consumes: `struct app_args.full_menu`
- Produces: `void menu_set_full_menu(int enabled)`

- [ ] **Step 1: Add a failing source integration test**

Create `tests/test_full_menu_visibility.py`:

```python
import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class FullMenuVisibilityTest(unittest.TestCase):
	def test_three_entries_share_full_menu_visibility(self):
		source = (ROOT / "menu.c").read_text(encoding="utf-8")
		self.assertRegex(source, r"MA_MAIN_OPTIONS,")
		for menu_id in (
			"MA_MAIN_OPTIONS",
			"MA_MAIN_CONTENT_SEL",
			"MA_MAIN_CREDITS",
		):
			self.assertRegex(
				source,
				r"me_enable\(e_menu_main,\s*" + menu_id +
				r",\s*full_menu_enabled\);",
			)

	def test_main_passes_parsed_flag_to_menu(self):
		source = (ROOT / "main.c").read_text(encoding="utf-8")
		self.assertRegex(
			source,
			r"menu_set_full_menu\(args\.full_menu\);",
		)


if __name__ == "__main__":
	unittest.main()
```

- [ ] **Step 2: Run the integration test and verify RED**

Run:

```sh
python3 -m unittest tests/test_full_menu_visibility.py -v
```

Expected: failures because the menu ID, setter call, and three visibility
calls do not exist.

- [ ] **Step 3: Add the menu setting and stable Options ID**

In `menu.c`, add process-wide state:

```c
static int full_menu_enabled;
```

Add `MA_MAIN_OPTIONS` to `menu_id` adjacent to the other main-menu IDs.
Change the Options declaration to:

```c
mee_handler_id_t(UI_TEXT_OPTIONS, MA_MAIN_OPTIONS, menu_loop_options),
```

Add the public setter:

```c
void menu_set_full_menu(int enabled)
{
	full_menu_enabled = enabled != 0;
}
```

Declare it in `menu.h`:

```c
void menu_set_full_menu(int enabled);
```

In `menu_loop()`, add:

```c
me_enable(e_menu_main, MA_MAIN_OPTIONS, full_menu_enabled);
me_enable(e_menu_main, MA_MAIN_CONTENT_SEL, full_menu_enabled);
me_enable(e_menu_main, MA_MAIN_CREDITS, full_menu_enabled);
```

- [ ] **Step 4: Pass the parsed flag from main**

Immediately after successful `app_args_parse()` and before menu use in
`main.c`, call:

```c
menu_set_full_menu(args.full_menu);
```

Keep the existing help, language, core, and content behavior unchanged.

- [ ] **Step 5: Run focused tests and verify GREEN**

Run:

```sh
python3 -m unittest tests/test_full_menu_visibility.py -v
cc -std=c99 -Wall -Wextra -I. tests/test_ui_config.c \
  ui_config.c ui_language.c -o tests/test_ui_config
./tests/test_ui_config
```

Expected: all tests pass with no compiler warnings.

- [ ] **Step 6: Run regression and hygiene checks**

Run:

```sh
python3 -m unittest discover -s tests -p 'test_*.py' -v
git diff --check
git status --short
```

Expected: Python tests pass, no whitespace errors, and only intentional
source/test/plan changes plus the local test executable.

- [ ] **Step 7: Remove the local test executable and commit**

Remove `tests/test_ui_config`, then run:

```sh
git add main.c menu.c menu.h ui_config.c ui_config.h \
  tests/test_ui_config.c tests/test_full_menu_visibility.py
git commit -m "hide extra main menu entries by default"
```

Expected: a focused implementation commit with no generated executable.
