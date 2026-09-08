#include "menu_style.h"

#include <stddef.h>
#include <stdint.h>

struct clipped_rect {
	int left;
	int top;
	int right;
	int bottom;
};

static int clip_rect(const struct menu_rect *rect, int width, int height,
		     struct clipped_rect *clipped)
{
	int64_t right;
	int64_t bottom;

	if (rect == NULL || clipped == NULL || width <= 0 || height <= 0 ||
	    rect->w <= 0 || rect->h <= 0)
		return 0;

	right = (int64_t)rect->x + rect->w;
	bottom = (int64_t)rect->y + rect->h;
	clipped->left = rect->x < 0 ? 0 : rect->x;
	clipped->top = rect->y < 0 ? 0 : rect->y;
	clipped->right = right > width ? width : (int)right;
	clipped->bottom = bottom > height ? height : (int)bottom;
	return clipped->left < clipped->right &&
		clipped->top < clipped->bottom;
}

int menu_style_main_geometry(const struct menu_responsive_layout *layout,
			     int line_height, int text_height, int title_height,
			     int glyph_width,
			     int total_visible, int selected_visible,
			     struct menu_style_geometry *geometry)
{
	int capacity;
	int available_height;
	int first;
	int count;
	int list_y;
	int row_y;

	if (layout == NULL || geometry == NULL || line_height <= 0 ||
	    text_height <= 0 ||
	    title_height <= 0 ||
	    glyph_width <= 0 || total_visible <= 0 || selected_visible < 0 ||
	    selected_visible >= total_visible || layout->menu.w <= glyph_width ||
	    layout->menu.h <= 0)
		return 0;

	list_y = layout->menu.y + title_height + 3 * line_height / 4;
	available_height = layout->menu.y + layout->menu.h - list_y;
	capacity = available_height / line_height;
	if (capacity <= 0)
		return 0;
	menu_visible_window(total_visible, selected_visible, capacity,
			    &first, &count);
	if (selected_visible < first || selected_visible >= first + count)
		return 0;

	row_y = list_y + (selected_visible - first) * line_height;

	geometry->selection.x = layout->menu.x / 2;
	geometry->selection.y = row_y;
	geometry->selection.w = layout->menu.x + layout->menu.w - glyph_width -
		geometry->selection.x;
	geometry->selection.h = line_height;
	geometry->text_x = geometry->selection.x + 2 * glyph_width;
	geometry->text_y = row_y;
	if (text_height < line_height)
		geometry->text_y += (line_height - text_height) / 2;
	geometry->title_x = geometry->selection.x;
	geometry->title_y = layout->menu.y;
	geometry->list_y = list_y;
	geometry->first_visible = first;
	geometry->visible_count = count;
	return 1;
}

int menu_style_next_savestate_slot(int current, int direction, int is_loading,
				   unsigned int used_slots, int slot_count)
{
	int candidate;
	int checked;

	if (slot_count <= 0)
		return 0;
	if (current < 0 || current > slot_count)
		current = slot_count;
	candidate = current;
	for (checked = 0; checked <= slot_count; checked++) {
		candidate += direction < 0 ? -1 : 1;
		if (candidate < 0)
			candidate = slot_count;
		if (candidate > slot_count)
			candidate = 0;
		if (!is_loading || candidate == slot_count ||
		    (candidate < 32 && (used_slots & (1u << candidate))))
			return candidate;
	}
	return slot_count;
}

int menu_style_option_columns(const struct menu_style_geometry *geometry,
			      int glyph_width, int value_width,
			      struct menu_style_option_columns *columns)
{
	int content_right;

	if (geometry == NULL || columns == NULL || glyph_width <= 0 ||
	    value_width <= 0 || geometry->selection.w <= 0)
		return 0;
	content_right = geometry->selection.x + geometry->selection.w -
		glyph_width;
	columns->value_x = content_right - value_width;
	if (columns->value_x < geometry->text_x)
		return 0;
	columns->name_clip_right = columns->value_x - glyph_width;
	if (columns->name_clip_right < geometry->text_x)
		columns->name_clip_right = geometry->text_x;
	return 1;
}

void menu_style_draw_selection(uint16_t *pixels, int width, int height,
			       int pitch, const struct menu_rect *selection,
			       uint16_t fill)
{
	struct clipped_rect clipped;
	int radius;
	int x;
	int y;

	if (pixels == NULL || pitch < width ||
	    !clip_rect(selection, width, height, &clipped))
		return;

	radius = selection->h / 6;
	if (radius < 1)
		radius = 1;
	if (radius * 2 > selection->w)
		radius = selection->w / 2;
	for (y = clipped.top; y < clipped.bottom; y++) {
		for (x = clipped.left; x < clipped.right; x++) {
			int dx = 0;
			int dy = 0;

			if (x < selection->x + radius)
				dx = selection->x + radius - x;
			else if (x >= selection->x + selection->w - radius)
				dx = x - (selection->x + selection->w - radius - 1);
			if (y < selection->y + radius)
				dy = selection->y + radius - y;
			else if (y >= selection->y + selection->h - radius)
				dy = y - (selection->y + selection->h - radius - 1);
			if (dx > 0 && dy > 0 && dx * dx + dy * dy > radius * radius)
				continue;
			pixels[y * pitch + x] = fill;
		}
	}
}
