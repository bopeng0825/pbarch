#include <assert.h>
#include <limits.h>
#include <string.h>

#include "menu_layout.h"

static int bitmap_text_width(const char *text, void *opaque)
{
	int glyph_width = *(int *)opaque;

	return menu_bitmap_text_width(text, glyph_width);
}

static int cjk_pixel_width(const char *text, void *opaque)
{
	(void)opaque;
	return strcmp(text, "中") == 0 ? 15 : (int)strlen(text) * 5;
}

static void test_font_sizes(void)
{
	assert(menu_main_font_px(240) == 12);
	assert(menu_main_font_px(480) == 20);
	assert(menu_main_font_px(720) == 30);
	assert(menu_main_font_px(1080) == 32);
	assert(menu_main_font_px(INT_MAX) == 32);
	assert(menu_small_font_px(20) == 16);
}

static void test_utf8_cell_widths(void)
{
	assert(menu_utf8_cells("ABC") == 3);
	assert(menu_utf8_cells("选项") == 4);
	assert(menu_utf8_cells("A选B") == 4);
	assert(menu_utf8_cells("\xe1\x84\x80") == 2);
	assert(menu_utf8_cells("\xe3\x84\xb1") == 2);
	assert(menu_utf8_cells("\xef\xbc\x81") == 2);
	assert(menu_utf8_cells("\xef\xbd\xa0") == 2);
	assert(menu_utf8_cells("\xef\xbf\xa0") == 2);
	assert(menu_utf8_cells("\xef\xbf\xa6") == 2);
	assert(menu_utf8_cells("\xef\xbf\xa8") == 1);
	assert(menu_utf8_cells("\xff") == 1);
	assert(menu_utf8_cells("\xe9") == 1);
	assert(menu_utf8_cells("\xe9\x80") == 2);
	assert(menu_utf8_cells("\xc0\xaf") == 2);
}

static void test_utf8_truncation(void)
{
	char out[32];

	assert(menu_utf8_truncate_cells("A选B", 3, out, sizeof(out)) == 3);
	assert(strcmp(out, "A选") == 0);
	assert(menu_utf8_truncate_cells("A选B", 2, out, sizeof(out)) == 1);
	assert(strcmp(out, "A") == 0);
}

static void test_utf8_truncation_small_destinations(void)
{
	char out[4];
	char byte = 'X';

	assert(menu_utf8_truncate_cells("选B", 3, out, sizeof(out)) == 2);
	assert(strcmp(out, "选") == 0);

	memset(out, 'X', sizeof(out));
	assert(menu_utf8_truncate_cells("选", 2, out, 3) == 0);
	assert(out[0] == '\0');

	assert(menu_utf8_truncate_cells("A", 1, &byte, 1) == 0);
	assert(byte == '\0');
	assert(menu_utf8_truncate_cells("A", 1, NULL, 0) == 0);
}

static void test_malformed_utf8_truncation(void)
{
	char out[8];

	assert(menu_utf8_truncate_cells("\xe9\x80", 1, out, sizeof(out)) == 1);
	assert((unsigned char)out[0] == 0xef);
	assert((unsigned char)out[1] == 0xbf);
	assert((unsigned char)out[2] == 0xbd);
	assert(out[3] == '\0');

	assert(menu_utf8_truncate_cells("\xff" "A", 2, out, sizeof(out)) == 2);
	assert((unsigned char)out[0] == 0xef);
	assert((unsigned char)out[1] == 0xbf);
	assert((unsigned char)out[2] == 0xbd);
	assert(out[3] == 'A');
	assert(out[4] == '\0');
}

static void test_valid_replacement_codepoint(void)
{
	char out[8];

	assert(menu_utf8_cells("\xef\xbf\xbd" "A") == 2);
	assert(menu_utf8_truncate_cells("\xef\xbf\xbd" "A", 2, out,
					sizeof(out)) == 2);
	assert(strcmp(out, "\xef\xbf\xbd" "A") == 0);
}

static void test_width_fitting_uses_bitmap_byte_width(void)
{
	char out[32];
	int glyph_width = 8;

	assert(menu_utf8_fit_width("中", 2 * glyph_width, out, sizeof(out),
				   bitmap_text_width, &glyph_width) == 0);
	assert(strcmp(out, "") == 0);
	assert(menu_utf8_fit_width("A中B", 2 * glyph_width, out, sizeof(out),
				   bitmap_text_width, &glyph_width) == 1);
	assert(strcmp(out, "A") == 0);
	assert(menu_utf8_fit_width("A中.txt", 6 * glyph_width, out, sizeof(out),
				   bitmap_text_width, &glyph_width) == 5);
	assert(strcmp(out, "A中.t") == 0);
}

static void test_width_fitting_uses_supplied_pixel_measurement(void)
{
	char out[8];

	assert(menu_utf8_fit_width("中", 15, out, sizeof(out),
				   cjk_pixel_width, NULL) == 2);
	assert(strcmp(out, "中") == 0);
}

int main(void)
{
	test_font_sizes();
	test_utf8_cell_widths();
	test_utf8_truncation();
	test_utf8_truncation_small_destinations();
	test_malformed_utf8_truncation();
	test_valid_replacement_codepoint();
	test_width_fitting_uses_bitmap_byte_width();
	test_width_fitting_uses_supplied_pixel_measurement();
	return 0;
}
