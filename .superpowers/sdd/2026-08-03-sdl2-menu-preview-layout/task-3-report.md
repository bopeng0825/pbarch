# Task 3 report: responsive ordinary SDL2 list menus

## Result

Ordinary SDL2 list menus now calculate and activate the shared responsive layout, copy the skin background, composite the last RGB565 source into the preview rectangle, and constrain/center the list in the menu rectangle. The ordinary SDL2 list no longer renders permanent entry help; timed error/status messages remain. Specialized full-screen pages do not consume the new list-layout helper, and non-SDL2 list centering remains in its legacy preprocessor branch.

## TDD evidence

RED 1: `python -m unittest tests.test_check_ui_literals tests.test_menu_legacy_geometry -v` was unavailable by command name, so the bundled Python 3.12 runtime was used. The first runnable RED had 19 tests and exactly four new failures: responsive preview, responsive list geometry, SDL2 help omission, and legacy centering branch protection.

GREEN 1: after the minimal integration, the same focused suite passed 19/19.

RED 2: a focused outer-margin message contract failed 1/1 before implementation.

GREEN 2: the final focused suite passed 20/20, and `tests/check_ui_literals.py` exited successfully.

## Verification

- Initial focused Python contracts: 20/20 pass; follow-up contracts are recorded below.
- Initial full Python discovery: 32/33 pass. The unrelated existing `test_mednafen_wswan_defaults` fails because `wswan_frameskip` is absent from the current override fixture.
- `git diff --check`: pass in both parent and `libpicofe` repositories before commit.
- Pure C layout test: initially not runnable by command name; the follow-up used the installed MinGW compiler by absolute path.
- H150101 build: not runnable because `make` is not installed or on `PATH`; therefore the SDL2 compile/link result is unverified in this environment.

## Commits

- `libpicofe`: `3b1c503 apply responsive SDL2 list geometry` on `codex/sdl2-menu-preview-layout`.
- Parent repository: recorded separately after this report and submodule pointer are committed.

## Self-review

- The activation API is SDL2-only and narrowly scoped to `me_draw`; confirmation boxes, ROM selection, bindings, credits, save-state pages, and other specialized functions retain their geometry.
- `drew_alt_bg` prevents the frame preview from replacing a selected save-state screenshot within the current frame; its precise frame-scoped behavior is corrected below.
- Preview failure is deliberately ignored after the skin copy, leaving a valid background.
- Legacy whole-screen `x/y` centering remains under `#else` and is covered by a source contract.
- An initial misplaced `#endif` found during diff review was corrected before commit and focused verification rerun.

## Concerns

Make and the H150101 cross-build environment are unavailable, so the final SDL2 target binary could not be verified here. The full Python suite also has one unrelated WonderSwan override-fixture failure as noted above.

## Review-blocker follow-up

The first review found that the preview source was cleared at SDL2 menu entry, text could extend past the menu column, long lists still drew every row, and temporary messages disappeared when the list filled the available height.

Root-cause tracing showed that hardware-scaled and XRGB frames only have a completed RGB565 representation in the renderer output. SDL2 menu entry now reads the last presented renderer output as RGB565 before changing to the menu texture, copies it into `g_menubg_src_ptr`, and lets the existing preview compositor consume that stable copy. The software-rendered `screen_pixels` buffer remains the fallback when renderer readback fails.

Ordinary SDL2 list text now fits against a temporary right clip at the menu rectangle edge. A tested `menu_visible_window` helper limits drawing to complete rows and shifts the window only enough to keep the selected enabled row visible. Timed messages reset to a full-output clip and draw at the responsive outer margin independently of list height.

Follow-up RED evidence:

- Three new focused Python contracts failed for completed-frame capture, text/row containment, and full-height message visibility.
- The new pure-C visible-window behavior test failed to compile before its API and implementation existed.

Follow-up GREEN evidence:

- Focused Python contracts pass 23/23.
- `test_menu_layout.c` compiles with MinGW GCC 9.2.0 and passes. The compiler required its `bin` directory on `PATH` so the `cc1` runtime DLLs could be found.
- Full Python discovery passes 36/37; the sole failure remains the pre-existing `wswan_frameskip` fixture issue.
- Parent and submodule `git diff --check` pass.

Follow-up commits:

- `libpicofe`: `3e0f4ff contain responsive SDL2 list content` on `codex/sdl2-menu-preview-layout`.
- Parent repository: the follow-up commit records the renderer capture, visible-window helper, contracts, report, and updated submodule pointer.

Correction to the earlier self-review: `drew_alt_bg` is frame-scoped because `menu_end` resets it after each menu frame. This is sufficient for the existing save-state page: that page reloads its selected screenshot before every frame, so `menu_begin` preserves it for that frame. It does not need to persist across the entire modal flow.

The H150101 build remains unavailable because `make` and the target SDL2 cross-build environment are not installed on `PATH` in this session.
