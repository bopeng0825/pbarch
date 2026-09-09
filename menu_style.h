#ifndef MENU_STYLE_H
#define MENU_STYLE_H

#include <stdint.h>

#include "menu_layout.h"

struct menu_style_geometry {
	struct menu_rect selection;
	int text_x;
	int text_y;
	int title_x;
	int title_y;
	int list_y;
	int first_visible;
	int visible_count;
};

struct menu_style_option_columns {
	int name_clip_right;
	int value_x;
};

/* Reserve a fixed footer row and half a row of separation. */
int menu_style_page_capacity(const struct menu_responsive_layout *layout,
			    int line_height, int title_height);

int menu_style_main_geometry(const struct menu_responsive_layout *layout,
			     int line_height, int text_height, int title_height,
			     int glyph_width,
			     int total_visible, int selected_visible,
			     struct menu_style_geometry *geometry);
int menu_style_next_savestate_slot(int current, int direction, int is_loading,
				   unsigned int used_slots, int slot_count);
int menu_style_option_columns(const struct menu_style_geometry *geometry,
			      int glyph_width, int value_width,
			      struct menu_style_option_columns *columns);
void menu_style_draw_selection(uint16_t *pixels, int width, int height,
			       int pitch, const struct menu_rect *selection,
			       uint16_t fill);

#endif
