# Task 3 report: responsive ordinary SDL2 list menus

## Result

Ordinary SDL2 list menus now calculate and activate the shared responsive layout, copy the skin background, composite the last RGB565 source into the preview rectangle, and constrain/center the list in the menu rectangle. The ordinary SDL2 list no longer renders permanent entry help; timed error/status messages remain. Specialized full-screen pages do not consume the new list-layout helper, and non-SDL2 list centering remains in its legacy preprocessor branch.

## TDD evidence

RED 1: `python -m unittest tests.test_check_ui_literals tests.test_menu_legacy_geometry -v` was unavailable by command name, so the bundled Python 3.12 runtime was used. The first runnable RED had 19 tests and exactly four new failures: responsive preview, responsive list geometry, SDL2 help omission, and legacy centering branch protection.

GREEN 1: after the minimal integration, the same focused suite passed 19/19.

RED 2: a focused outer-margin message contract failed 1/1 before implementation.

GREEN 2: the final focused suite passed 20/20, and `tests/check_ui_literals.py` exited successfully.

## Verification

- Focused Python contracts: 20/20 pass.
- Full Python discovery: 32/33 pass. The unrelated existing `test_mednafen_wswan_defaults` fails because `wswan_frameskip` is absent from the current override fixture.
- `git diff --check`: pass in both parent and `libpicofe` repositories before commit.
- Pure C layout test: not runnable because `gcc` is not installed or on `PATH`.
- H150101 build: not runnable because `make` is not installed or on `PATH`; therefore the SDL2 compile/link result is unverified in this environment.

## Commits

- `libpicofe`: `3b1c503 apply responsive SDL2 list geometry` on `codex/sdl2-menu-preview-layout`.
- Parent repository: recorded separately after this report and submodule pointer are committed.

## Self-review

- The activation API is SDL2-only and narrowly scoped to `me_draw`; confirmation boxes, ROM selection, bindings, credits, save-state pages, and other specialized functions retain their geometry.
- `drew_alt_bg` still prevents the frame preview from replacing a selected save-state screenshot until the modal flow ends.
- Preview failure is deliberately ignored after the skin copy, leaving a valid background.
- Legacy whole-screen `x/y` centering remains under `#else` and is covered by a source contract.
- An initial misplaced `#endif` found during diff review was corrected before commit and focused verification rerun.

## Concerns

The environment has neither a C compiler nor Make, so compiler warnings, SDL2 headers/libraries, and the final H150101 binary could not be verified here. The full Python suite also has one unrelated WonderSwan override-fixture failure as noted above.
