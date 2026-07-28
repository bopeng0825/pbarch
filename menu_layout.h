#ifndef MENU_LAYOUT_H
#define MENU_LAYOUT_H

#include <stddef.h>

int menu_main_font_px(int menu_height);
int menu_small_font_px(int main_px);
size_t menu_utf8_cells(const char *text);
size_t menu_utf8_truncate_cells(const char *src, size_t max_cells,
				char *dst, size_t dst_size);

#endif
