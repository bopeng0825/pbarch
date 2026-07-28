#ifndef MENU_LAYOUT_H
#define MENU_LAYOUT_H

#include <stddef.h>

typedef int (*menu_utf8_width_fn)(const char *text, void *opaque);

int menu_main_font_px(int menu_height);
int menu_small_font_px(int main_px);
size_t menu_utf8_cells(const char *text);
size_t menu_utf8_truncate_cells(const char *src, size_t max_cells,
				char *dst, size_t dst_size);
size_t menu_utf8_fit_width(const char *src, int max_width,
			   char *dst, size_t dst_size,
			   menu_utf8_width_fn width, void *opaque);
int menu_bitmap_text_width(const char *text, int glyph_width);

#endif
