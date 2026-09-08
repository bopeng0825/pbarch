#include <assert.h>
#include <stdint.h>

#include "menu_style.h"

static void test_readable_font_hierarchy(void)
{
	assert(menu_title_font_px(12) == 20);
	assert(menu_title_font_px(20) == 34);
	assert(menu_title_font_px(30) == 48);
	assert(menu_title_font_px(32) == 48);
	assert(menu_spaced_line_height(24, 20) == 34);
	assert(menu_spaced_line_height(36, 30) == 51);
	assert(menu_spaced_line_height(0, 20) == 0);
}

static void test_main_menu_geometry(void)
{
	struct menu_responsive_layout layout = {
		.output_width = 640,
		.output_height = 480,
		.menu = { 20, 20, 228, 440 },
	};
	struct menu_style_geometry geometry;

	assert(menu_style_main_geometry(&layout, 34, 24, 40, 12, 5, 0,
					&geometry));
	assert(geometry.selection.x == 10);
	assert(geometry.selection.y == 85);
	assert(geometry.selection.w == 226);
	assert(geometry.selection.h == 34);
	assert(geometry.text_x == 34);
	assert(geometry.text_y == 90);
	assert(geometry.title_x == 10);
	assert(geometry.title_y == 20);
	assert(geometry.list_y == 85);

	assert(menu_style_main_geometry(&layout, 34, 24, 40, 12, 5, 4,
					&geometry));
	assert(geometry.selection.y == 221);
	assert(geometry.text_y == 226);
	assert(!menu_style_main_geometry(NULL, 34, 24, 40, 12, 5, 0,
					 &geometry));
	assert(!menu_style_main_geometry(&layout, 0, 24, 40, 12, 5, 0,
					 &geometry));
	assert(!menu_style_main_geometry(&layout, 34, 24, 40, 12, 0, 0,
					 &geometry));
	assert(!menu_style_main_geometry(&layout, 34, 24, 40, 12, 5, 5,
					 &geometry));
}

static void test_h150102_geometry_stays_in_menu_column(void)
{
	struct menu_responsive_layout layout;
	struct menu_style_geometry geometry;

	menu_calculate_responsive_layout(1280, 720, &layout);
	assert(layout.output_width == 1280);
	assert(layout.output_height == 720);
	assert(menu_style_main_geometry(&layout, 51, 35, 56, 18, 10, 9,
					&geometry));
	assert(geometry.selection.x == layout.menu.x / 2);
	assert(geometry.selection.x + geometry.selection.w ==
		layout.menu.x + layout.menu.w - 18);
	assert(geometry.selection.y >= layout.menu.y);
	assert(geometry.selection.y + geometry.selection.h <=
		layout.menu.y + layout.menu.h);
}

static void test_savestate_rows_share_responsive_menu_geometry(void)
{
	struct menu_responsive_layout layout = {
		.output_width = 640,
		.output_height = 480,
		.menu = { 20, 20, 228, 440 },
	};
	struct menu_style_geometry geometry;

	assert(menu_style_main_geometry(&layout, 34, 24, 40, 12, 11, 10,
					&geometry));
	assert(geometry.first_visible == 0);
	assert(geometry.visible_count == 11);
	assert(geometry.list_y == 85);
	assert(geometry.text_y == 430);
	assert(geometry.selection.y == 425);
	assert(geometry.selection.y + geometry.selection.h == 459);
	assert(geometry.selection.x + geometry.selection.w == 236);
}

static void test_savestate_navigation_preserves_empty_slot_rules(void)
{
	unsigned int used = (1u << 2) | (1u << 7);

	assert(menu_style_next_savestate_slot(10, 1, 0, 0, 10) == 0);
	assert(menu_style_next_savestate_slot(10, 1, 1, used, 10) == 2);
	assert(menu_style_next_savestate_slot(2, 1, 1, used, 10) == 7);
	assert(menu_style_next_savestate_slot(7, 1, 1, used, 10) == 10);
	assert(menu_style_next_savestate_slot(10, -1, 1, used, 10) == 7);
	assert(menu_style_next_savestate_slot(2, -1, 1, used, 10) == 10);
	assert(menu_style_next_savestate_slot(10, 1, 1, 0, 10) == 10);
}

static void test_flat_selection_pixels(void)
{
	uint16_t pixels[8 * 14];
	struct menu_rect selection = { 2, 1, 8, 6 };
	int x;
	int y;

	for (y = 0; y < 8; y++)
		for (x = 0; x < 14; x++)
			pixels[y * 14 + x] = 0xdead;

	menu_style_draw_selection(pixels, 12, 8, 14, &selection, 0x1111);

	assert(pixels[1 * 14 + 2] == 0xdead);
	assert(pixels[1 * 14 + 3] == 0x1111);
	assert(pixels[2 * 14 + 2] == 0x1111);
	assert(pixels[3 * 14 + 5] == 0x1111);
	assert(pixels[6 * 14 + 8] == 0x1111);
	assert(pixels[6 * 14 + 9] == 0xdead);
	assert(pixels[0] == 0xdead);
	assert(pixels[7 * 14 + 11] == 0xdead);
	for (y = 0; y < 8; y++) {
		assert(pixels[y * 14 + 12] == 0xdead);
		assert(pixels[y * 14 + 13] == 0xdead);
	}
}

static void test_clipped_selection(void)
{
	uint16_t pixels[5 * 5] = { 0 };
	struct menu_rect selection = { -2, -1, 5, 4 };

	menu_style_draw_selection(pixels, 5, 5, 5, &selection, 1);
	assert(pixels[0] == 1);
	assert(pixels[1] == 1);
	assert(pixels[2] == 1);
	assert(pixels[2 * 5] == 1);
	assert(pixels[4 * 5 + 4] == 0);
}

int main(void)
{
	test_readable_font_hierarchy();
	test_main_menu_geometry();
	test_h150102_geometry_stays_in_menu_column();
	test_savestate_rows_share_responsive_menu_geometry();
	test_savestate_navigation_preserves_empty_slot_rules();
	test_flat_selection_pixels();
	test_clipped_selection();
	return 0;
}
