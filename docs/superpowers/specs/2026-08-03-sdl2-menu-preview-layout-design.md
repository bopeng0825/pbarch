# SDL2 Menu Preview Layout Design

## Goal

Synchronize picoarch's SDL2 menu presentation with the recent `fba-a320`
menu changes. Ordinary list menus use a stable left text column and a larger
right-side game preview, while the same responsive rules support both the
current 640x480 output and a future 1280x720 output.

This change applies only when `USE_SDL2` is enabled. Legacy SDL targets retain
their current layout and rendering behavior.

## Responsive layout

Layout is calculated from the physical menu output dimensions on every menu
entry or output-size change. It must not contain coordinates specific to
640x480.

- The main font is approximately output height divided by 24, clamped to
  12-32 pixels. The small font remains four fifths of the main font.
- The outer margin and the gap between columns equal the main font size.
- After margins are removed, the menu column receives 42 percent of the usable
  width. The preview receives the remaining width after the column gap.
- Both columns use the full height between the top and bottom outer margins.
- Two-column mode is enabled only when both columns are at least twelve main
  font widths wide. Otherwise the preview is hidden and the menu becomes a
  single column.

At 640x480 the main font is 20 pixels and the target preview frame is about
336x252. At 1280x720 the main font is 30 pixels and the target preview frame is
about 504x378. Extra horizontal space at 1280x720 remains balanced whitespace;
it does not enlarge or stretch the text.

## Preview

The right-side preview frame follows the final `fba-a320` rule:

- target width is 70 percent of output height;
- width is capped by the available preview column and by the height available
  for a 4:3 frame;
- width is rounded down to a multiple of four;
- frame height is three quarters of frame width;
- the frame is centered horizontally and vertically in the preview column;
- the captured game frame is aspect-fitted inside the frame and centered,
  without stretching or cropping;
- no decorative border is drawn.

The preview uses the last completed game frame captured before menu entry.
Failure to capture or render it is non-fatal: the background remains visible
and the menu remains usable. A save-state browser may replace the game preview
with the selected slot screenshot, preserving existing slot behavior.

## Text placement

Ordinary list menus keep libpicofe's existing entry generation, selection,
value formatting, navigation, and actions. Only their presentation changes.

- The complete visible item block is vertically centered in the menu column.
- Text begins at a stable inset inside the menu column rather than being
  centered according to the longest item on each page.
- Selectors and option values remain aligned using the existing menu drawing
  conventions, but drawing and fitting are constrained to the menu column.
- Long labels and values are fitted to the available width without writing
  into the preview column.
- The permanent bottom help line is removed. Temporary success, warning, and
  error messages remain visible across the usable screen width.

## Page scope

The responsive two-column presentation applies to ordinary SDL2 list pages,
including the main menu, audio/video options, core options, configuration,
cheats, and disc control.

Special-purpose pages keep their current full-screen layout when a narrow
column would reduce usability. These include file selection, input binding,
confirmation dialogs, long informational text, and other modal pages. Their
entry and exit must preserve the captured preview so returning to a list page
restores the same right-side image.

## Code boundaries

Pure geometry belongs in `menu_layout.c` and `menu_layout.h`, including the
responsive column calculation, centered-block calculation, and aspect-fit
calculation. These helpers do not depend on SDL event handling or emulator
state and are directly testable.

The libpicofe menu renderer consumes the calculated menu rectangle for list
placement and width fitting. The SDL2 menu layer owns background and preview
composition. Platform video code remains responsible for supplying the last
completed frame and the physical output size.

No unrelated menu-controller or platform refactor is part of this change.

## Validation

Automated layout tests cover:

- exact font, margin, column, and preview geometry at 640x480;
- exact geometry at 1280x720;
- a narrow output that falls back to one column;
- centered item-block placement, including oversized blocks;
- aspect fitting for 4:3, wider, and taller source frames;
- long text remaining inside the menu column;
- preview-capture failure leaving menu presentation functional.

Existing menu, localization, and SDL2 rendering tests must continue to pass.
Build validation uses the repository's applicable local test commands and an
H150101 SDL2 build when its cross-toolchain is available. Device validation
should check menu entry and exit, every ordinary list page, save-state preview
changes, 640x480 output, and a simulated or real 1280x720 output.
