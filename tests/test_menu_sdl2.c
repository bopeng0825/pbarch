#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <png.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include "menu_sdl2.h"

static void write_test_png(const char *path, png_uint_32 format,
			   const unsigned char *pixels)
{
	png_image image;

	SDL_memset(&image, 0, sizeof(image));
	image.version = PNG_IMAGE_VERSION;
	image.width = 2;
	image.height = 2;
	image.format = format;
	assert(png_image_write_to_file(&image, path, 0, pixels, 0, NULL));
}

static void assert_scaled_background(const uint16_t *pixels, int pitch,
				     const uint16_t expected[4])
{
	int y;
	int x;

	for (y = 0; y < 4; y++) {
		for (x = 0; x < 4; x++)
			assert(pixels[y * pitch + x] ==
			       expected[(y / 2) * 2 + x / 2]);
		assert(pixels[y * pitch + 4] == 0xdead);
		assert(pixels[y * pitch + 5] == 0xdead);
	}
}

static void test_valid_renderer(void)
{
	uint16_t pixels[640 * 480];
	size_t entries;

	assert(menu_sdl2_init("skin/picoarch-ui.ttf",
			      "skin/background.png", 640, 480) == 0);
	assert(menu_sdl2_available());
	assert(menu_sdl2_main_font_px() == 20);
	assert(menu_sdl2_small_font_px() == 16);
	assert(menu_sdl2_title_font_px() == 34);
	assert(menu_sdl2_line_height(MENU_FONT_MAIN) > 0);
	assert(menu_sdl2_line_height(MENU_FONT_TITLE) > 0);
	assert(menu_sdl2_font_height(MENU_FONT_MAIN) > 0);
#ifdef MENU_SDL2_TEST
	assert(menu_sdl2_metric_cache_entries() == 0);
	assert(menu_sdl2_measure_calls() == 0);
#endif
	assert(menu_sdl2_text_width(MENU_FONT_MAIN, "Options") > 0);
#ifdef MENU_SDL2_TEST
	assert(menu_sdl2_metric_cache_entries() == 1);
	assert(menu_sdl2_measure_calls() == 1);
#endif
	assert(menu_sdl2_text_width(MENU_FONT_MAIN, "Options") > 0);
#ifdef MENU_SDL2_TEST
	assert(menu_sdl2_metric_cache_entries() == 1);
	assert(menu_sdl2_measure_calls() == 1);
#endif
	assert(menu_sdl2_draw_text(pixels, 640, MENU_FONT_MAIN, 0, 0,
				   0xffff, "Options") == 0);
	entries = menu_sdl2_cache_entries();
	assert(entries == 2);
	assert(menu_sdl2_draw_text(pixels, 640, MENU_FONT_MAIN, 0, 0,
				   0xffff, "Options") == 0);
	assert(menu_sdl2_cache_entries() == entries);
#ifdef MENU_SDL2_TEST
	assert(menu_sdl2_text_width(MENU_FONT_MAIN, "Options") > 0);
	assert(menu_sdl2_metric_cache_entries() == 1);
	assert(menu_sdl2_measure_calls() == 1);
#endif
#ifdef MENU_SDL2_TEST_ALLOC
	menu_sdl2_test_fail_allocations_after(0);
	assert(menu_sdl2_text_width(MENU_FONT_MAIN, "uncached") > 0);
	assert(menu_sdl2_metric_cache_entries() == 1);
	assert(menu_sdl2_measure_calls() == 2);
	assert(menu_sdl2_text_width(MENU_FONT_MAIN, "uncached") > 0);
	assert(menu_sdl2_metric_cache_entries() == 1);
	assert(menu_sdl2_measure_calls() == 3);
	menu_sdl2_test_fail_allocations_after(-1);
#endif

	menu_sdl2_clear_cache();
	assert(menu_sdl2_cache_entries() == 0);
#ifdef MENU_SDL2_TEST
	assert(menu_sdl2_metric_cache_entries() == 0);
#endif
	menu_sdl2_finish();
}

static void test_unshadowed_text_omits_shadow_pixels(void)
{
	uint16_t shadowed[160 * 80];
	uint16_t unshadowed[160 * 80];
	struct menu_rect clip = { 0, 0, 160, 80 };
	const uint16_t background = 0x7bef;
	int foreground_pixels = 0;
	int shadow_only_pixels = 0;
	int i;

	for (i = 0; i < 160 * 80; i++) {
		shadowed[i] = background;
		unshadowed[i] = background;
	}
	assert(menu_sdl2_init("skin/picoarch-ui.ttf",
			      "skin/background.png", 160, 80) == 0);
	assert(menu_sdl2_draw_text(shadowed, 160, MENU_FONT_MAIN,
				   10, 10, 0xffff, "Selected") == 0);
	assert(menu_sdl2_draw_text_clipped_unshadowed(unshadowed, 160,
						      MENU_FONT_MAIN, 10, 10,
						      0xffff, "Selected",
						      &clip) == 0);
	for (i = 0; i < 160 * 80; i++) {
		if (unshadowed[i] != background)
			foreground_pixels++;
		if (unshadowed[i] == background && shadowed[i] != background)
			shadow_only_pixels++;
	}
	assert(foreground_pixels > 0);
	assert(shadow_only_pixels > 0);
	menu_sdl2_finish();
}

static void test_rgb_and_rgba_background_scaling(void)
{
	static const unsigned char rgb[] = {
		248, 0, 0, 0, 252, 0,
		0, 0, 248, 248, 252, 248
	};
	static const unsigned char rgba[] = {
		248, 252, 0, 255, 248, 0, 248, 255,
		0, 252, 248, 255, 0, 0, 0, 255
	};
	static const uint16_t rgb_expected[] = {
		0xf800, 0x07e0, 0x001f, 0xffff
	};
	static const uint16_t rgba_expected[] = {
		0xffe0, 0xf81f, 0x07ff, 0x0000
	};
	const char *rgb_path = "tests/.menu-sdl2-rgb.png";
	const char *rgba_path = "tests/.menu-sdl2-rgba.png";
	uint16_t pixels[6 * 4];
	size_t i;

	assert(menu_sdl2_scale_coordinate(UINT32_MAX - 1, UINT32_MAX,
					  UINT32_MAX) == UINT32_MAX - 1);

	write_test_png(rgb_path, PNG_FORMAT_RGB, rgb);
	for (i = 0; i < sizeof(pixels) / sizeof(pixels[0]); i++)
		pixels[i] = 0xdead;
	assert(menu_sdl2_init("skin/picoarch-ui.ttf", rgb_path, 4, 4) == 0);
	menu_sdl2_copy_background(pixels, 6);
	assert_scaled_background(pixels, 6, rgb_expected);
	menu_sdl2_finish();
	assert(remove(rgb_path) == 0);

	write_test_png(rgba_path, PNG_FORMAT_RGBA, rgba);
	for (i = 0; i < sizeof(pixels) / sizeof(pixels[0]); i++)
		pixels[i] = 0xdead;
	assert(menu_sdl2_init("skin/picoarch-ui.ttf", rgba_path, 4, 4) == 0);
	menu_sdl2_copy_background(pixels, 6);
	assert_scaled_background(pixels, 6, rgba_expected);
	menu_sdl2_finish();
	assert(remove(rgba_path) == 0);
}

static void test_missing_background(void)
{
	uint16_t pixels[640 * 480] = { 0 };

	assert(menu_sdl2_init("skin/picoarch-ui.ttf",
			      "skin/absent-background.png", 640, 480) == 0);
	menu_sdl2_copy_background(pixels, 640);
	assert(pixels[0] == MENU_FALLBACK_BG);
	assert(pixels[(640 * 480) - 1] == MENU_FALLBACK_BG);
	menu_sdl2_finish();
}

static void test_missing_font(void)
{
	assert(menu_sdl2_init("skin/absent-font.ttf",
			      "skin/background.png", 640, 480) == 0);
	assert(!menu_sdl2_available());
	assert(menu_sdl2_text_width(MENU_FONT_MAIN, "Options") == 0);
	menu_sdl2_finish();
}

static void test_invalid_geometry_and_reinitialization(void)
{
	uint16_t pixels[640 * 480];

	assert(menu_sdl2_init("skin/picoarch-ui.ttf",
			      "skin/background.png", 0, 480) == -1);
	assert(!menu_sdl2_available());
	assert(menu_sdl2_draw_text(pixels, 639, MENU_FONT_MAIN, 0, 0,
				   0xffff, "Options") == -1);
	menu_sdl2_copy_background(pixels, 639);
	menu_sdl2_finish();

	assert(menu_sdl2_init("skin/picoarch-ui.ttf",
			      "skin/background.png", 640, 480) == 0);
	assert(menu_sdl2_available());
	assert(menu_sdl2_draw_text(NULL, 640, MENU_FONT_MAIN, 0, 0,
				   0xffff, "Options") == -1);
	assert(menu_sdl2_draw_text(pixels, 0, MENU_FONT_MAIN, 0, 0,
				   0xffff, "Options") == -1);
	menu_sdl2_finish();
	menu_sdl2_finish();
}

static void test_rejects_overflowing_framebuffer_span(void)
{
	uint16_t pixel = 0xdead;

	assert(menu_sdl2_init("skin/picoarch-ui.ttf",
			      "skin/background.png", 1, 3) == 0);
	menu_sdl2_copy_background(&pixel, INT_MAX);
	assert(pixel == 0xdead);
	assert(menu_sdl2_draw_text(&pixel, 800000000, MENU_FONT_MAIN,
				   0, 0, 0xffff, "X") == -1);
	menu_sdl2_finish();

	assert(menu_sdl2_init("skin/picoarch-ui.ttf",
			      "skin/background.png", 1, INT_MAX) == -1);
}

static void test_preview_is_aspect_fitted_and_respects_pitch(void)
{
	static const uint16_t source[6 * 2] = {
		0x0001, 0x0002, 0x0003, 0x0004, 0xeeee, 0xeeee,
		0x0011, 0x0012, 0x0013, 0x0014, 0xeeee, 0xeeee
	};
	struct menu_rect bounds = { 2, 0, 4, 4 };
	uint16_t destination[8 * 4];
	size_t i;

	for (i = 0; i < sizeof(destination) / sizeof(destination[0]); i++)
		destination[i] = 0xdead;

	assert(menu_sdl2_draw_preview(destination, 8, source, 4, 2, 6,
				      &bounds) == 0);
	assert(destination[1 * 8 + 2] == 0x0001);
	assert(destination[1 * 8 + 5] == 0x0004);
	assert(destination[2 * 8 + 2] == 0x0011);
	assert(destination[2 * 8 + 5] == 0x0014);
	assert(destination[0 * 8 + 2] == 0xdead);
	assert(destination[3 * 8 + 2] == 0xdead);
	assert(destination[1 * 8 + 1] == 0xdead);
	assert(destination[1 * 8 + 6] == 0xdead);
}

static void test_preview_rejects_invalid_geometry(void)
{
	uint16_t destination[8 * 4] = { 0 };
	uint16_t source[6 * 2] = { 0 };
	struct menu_rect bounds = { 2, 0, 4, 4 };
	struct menu_rect empty_bounds = { 0, 0, 0, 4 };

	assert(menu_sdl2_draw_preview(NULL, 8, source, 4, 2, 6,
				      &bounds) == -1);
	assert(menu_sdl2_draw_preview(destination, 8, NULL, 4, 2, 6,
				      &bounds) == -1);
	assert(menu_sdl2_draw_preview(destination, 8, source, 0, 2, 6,
				      &bounds) == -1);
	assert(menu_sdl2_draw_preview(destination, 8, source, 4, 0, 6,
				      &bounds) == -1);
	assert(menu_sdl2_draw_preview(destination, 8, source, 4, 2, 6,
				      &empty_bounds) == -1);
	assert(menu_sdl2_draw_preview(destination, 8, source, 4, 2, 6,
				      NULL) == -1);
}

static int rect_has_pixels(const uint16_t *pixels, int pitch,
			   const struct menu_rect *rect)
{
	int x;
	int y;

	for (y = rect->y; y < rect->y + rect->h; y++)
		for (x = rect->x; x < rect->x + rect->w; x++)
			if (pixels[y * pitch + x] != 0)
				return 1;
	return 0;
}

static int test_value_x(const struct menu_responsive_layout *layout)
{
	int m_width = menu_sdl2_text_width(MENU_FONT_MAIN, "M");
	int text_x = layout->menu.x + m_width * 3;
	int value_width = menu_sdl2_text_width(MENU_FONT_MAIN, "OFF");
	int value_reserve = m_width * 3;
	int measured_reserve = value_width + m_width / 2;

	if (measured_reserve > value_reserve)
		value_reserve = measured_reserve;
	return menu_value_column_x(text_x, layout->menu.x + layout->menu.w,
		layout->menu.x + layout->menu.w, value_reserve);
}

static void draw_marquee_frame(uint16_t *pixels, int width,
				const struct menu_responsive_layout *layout,
				unsigned int elapsed_ms)
{
	static const char label[] =
		"\xe6\x97\xa0\xe9\x99\x90\xe6\x8a\x80"
		"\xe8\x83\xbd\xe7\x82\xb9 Unlimited Skill Points";
	struct menu_rect clip;
	int m_width = menu_sdl2_text_width(MENU_FONT_MAIN, "M");
	int text_x = layout->menu.x + m_width * 3;
	int value_x = test_value_x(layout);
	int text_width = menu_sdl2_text_width(MENU_FONT_MAIN, label);
	int gap_width = m_width * 3;
	int offset;
	int second_x;

	clip.x = text_x;
	clip.y = layout->menu.y;
	clip.w = value_x - m_width - text_x;
	clip.h = menu_sdl2_line_height(MENU_FONT_MAIN);
	offset = menu_marquee_offset(text_width, clip.w, gap_width, elapsed_ms);
	assert(menu_sdl2_draw_text_clipped(pixels, width, MENU_FONT_MAIN,
					   text_x - offset, clip.y, 0xffff,
					   label, &clip) == 0);
	second_x = text_x - offset + text_width + gap_width;
	if (second_x < clip.x + clip.w)
		assert(menu_sdl2_draw_text_clipped(pixels, width, MENU_FONT_MAIN,
						   second_x, clip.y, 0xffff,
						   label, &clip) == 0);
	assert(menu_sdl2_draw_text(pixels, width, MENU_FONT_MAIN,
				   value_x, clip.y, 0xffff, "OFF") == 0);
}

static void test_marquee_clipping_at_resolution(int width, int height)
{
	struct menu_responsive_layout layout;
	struct menu_rect label_rect;
	struct menu_rect value_rect;
	struct menu_rect restored_rect;
	uint16_t *first = calloc((size_t)width * height, sizeof(*first));
	uint16_t *moving = calloc((size_t)width * height, sizeof(*moving));
	int m_width;
	int label_changed = 0;
	int x;
	int y;

	assert(first != NULL && moving != NULL);
	assert(menu_sdl2_init("skin/picoarch-ui.ttf",
			      "skin/background.png", width, height) == 0);
	menu_calculate_responsive_layout(width, height, &layout);
	assert(layout.show_preview);
	m_width = menu_sdl2_text_width(MENU_FONT_MAIN, "M");
	draw_marquee_frame(first, width, &layout, 0);
	draw_marquee_frame(moving, width, &layout, 3000);

	label_rect.x = layout.menu.x + m_width * 3;
	label_rect.y = layout.menu.y;
	label_rect.w = test_value_x(&layout) - m_width -
		label_rect.x;
	label_rect.h = menu_sdl2_line_height(MENU_FONT_MAIN);
	for (y = label_rect.y; y < label_rect.y + label_rect.h; y++)
		for (x = label_rect.x; x < label_rect.x + label_rect.w; x++)
			if (first[y * width + x] != moving[y * width + x])
				label_changed = 1;
	assert(label_changed);

	value_rect.x = test_value_x(&layout);
	value_rect.y = layout.menu.y;
	value_rect.w = layout.menu.x + layout.menu.w - value_rect.x;
	value_rect.h = menu_sdl2_line_height(MENU_FONT_MAIN);
	assert(menu_sdl2_text_width(MENU_FONT_MAIN, "OFF") <= value_rect.w);
	assert(rect_has_pixels(first, width, &value_rect));
	for (y = value_rect.y; y < value_rect.y + value_rect.h; y++)
		for (x = value_rect.x; x < value_rect.x + value_rect.w; x++)
			assert(first[y * width + x] == moving[y * width + x]);
	assert(!rect_has_pixels(first, width, &layout.preview));
	assert(!rect_has_pixels(moving, width, &layout.preview));

	restored_rect.x = layout.preview.x;
	restored_rect.y = layout.preview.y;
	restored_rect.w = layout.main_font_px * 2;
	restored_rect.h = menu_sdl2_line_height(MENU_FONT_MAIN);
	assert(menu_sdl2_draw_text(first, width, MENU_FONT_MAIN,
				   restored_rect.x, restored_rect.y,
				   0xffff, "X") == 0);
	assert(rect_has_pixels(first, width, &restored_rect));
	assert(menu_sdl2_draw_text_clipped(first, width, MENU_FONT_MAIN,
					   0, 0, 0xffff, "X", NULL) == -1);

	menu_sdl2_finish();
	free(first);
	free(moving);
}

static void test_text_clipping_stays_inside_menu_column(void)
{
	test_marquee_clipping_at_resolution(640, 480);
	test_marquee_clipping_at_resolution(1280, 720);
}

int main(void)
{
	SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
	assert(SDL_Init(SDL_INIT_VIDEO) == 0);
	assert(TTF_Init() == 0);

	test_valid_renderer();
	test_unshadowed_text_omits_shadow_pixels();
	test_rgb_and_rgba_background_scaling();
	test_missing_background();
	test_missing_font();
	test_invalid_geometry_and_reinitialization();
	test_rejects_overflowing_framebuffer_span();
	test_preview_is_aspect_fitted_and_respects_pitch();
	test_preview_rejects_invalid_geometry();
	test_text_clipping_stays_inside_menu_column();

	TTF_Quit();
	SDL_Quit();
	return 0;
}
