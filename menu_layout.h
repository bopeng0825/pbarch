#ifndef MENU_LAYOUT_H
#define MENU_LAYOUT_H

#include <stddef.h>

typedef int (*menu_utf8_width_fn)(const char *text, void *opaque);

struct menu_rect {
	int x;
	int y;
	int w;
	int h;
};

struct menu_responsive_layout {
	int output_width;
	int output_height;
	int main_font_px;
	int small_font_px;
	int outer_margin;
	int column_gap;
	int show_preview;
	struct menu_rect menu;
	struct menu_rect preview;
};

int menu_main_font_px(int menu_height);
int menu_small_font_px(int main_px);
size_t menu_utf8_cells(const char *text);
size_t menu_utf8_truncate_cells(const char *src, size_t max_cells,
				char *dst, size_t dst_size);
size_t menu_utf8_fit_width(const char *src, int max_width,
			   char *dst, size_t dst_size,
			   menu_utf8_width_fn width, void *opaque);
int menu_bitmap_text_width(const char *text, int glyph_width);
void menu_calculate_responsive_layout(int output_width, int output_height,
				      struct menu_responsive_layout *layout);
int menu_centered_block_y(int top, int height, int block_height);
void menu_aspect_fit(int source_width, int source_height,
		     const struct menu_rect *bounds, struct menu_rect *fitted);

#endif
