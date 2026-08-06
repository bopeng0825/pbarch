# SDL2 Menu Column Spacing Design

## Goal

Reduce the excessive-looking horizontal whitespace between the left menu text
and the right game preview in SDL2 list menus. Preserve the existing text
origin, preview sizing rules, vertical centering, font sizes, and line spacing.
The layout must remain suitable for both 640x480 and 1280x720 output.

## Layout change

In two-column mode, assign 38 percent of the usable width to the menu column
instead of 42 percent. Place the preview frame at the left edge of the preview
column after the existing one-main-font column gap, rather than centering it
horizontally inside the remaining preview-column space.

All other responsive rules remain unchanged:

- the outer margin and column gap equal the main font size;
- the preview target width remains 70 percent of output height, capped by the
  available preview column and the height available for a 4:3 frame;
- preview width remains rounded down to a multiple of four;
- the preview remains vertically centered;
- two-column mode requires both columns to be at least eleven main-font
  widths wide, otherwise the preview is hidden and the menu uses the full
  width.

## Expected geometry

At 640x480, the main font remains 20 pixels. The wider preview column allows
the existing sizing rule to increase the preview from 328x246 to 336x252. Its
origin moves from x=292, y=117 to x=268, y=114.

At 1280x720, the main font remains 30 pixels and the preview remains 504x378.
Its horizontal origin moves from x=629 to x=523.

The left menu rectangle continues to begin at the outer margin. Text keeps its
existing inset within that rectangle. Extra wide-screen space remains to the
right of the preview instead of being split on both sides of it.

## Scope

Only the pure responsive geometry in `menu_layout.c` changes. Menu rendering,
font metrics, vertical item spacing, preview sizing rules and aspect fitting,
input, and non-SDL2 presentation are outside this change.

## Validation

Update the layout unit tests to assert the menu width and preview rectangle at
640x480 and 1280x720. Retain narrow-output fallback and integer-overflow tests.
Run the menu layout tests and the repository's applicable UI contract tests,
then confirm the diff contains no whitespace errors.
