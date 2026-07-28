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
	assert(menu_sdl2_line_height(MENU_FONT_MAIN) > 0);
	assert(menu_sdl2_text_width(MENU_FONT_MAIN, "Options") > 0);
	assert(menu_sdl2_draw_text(pixels, 640, MENU_FONT_MAIN, 0, 0,
				   0xffff, "Options") == 0);
	entries = menu_sdl2_cache_entries();
	assert(entries == 1);
	assert(menu_sdl2_draw_text(pixels, 640, MENU_FONT_MAIN, 0, 0,
				   0xffff, "Options") == 0);
	assert(menu_sdl2_cache_entries() == entries);

	menu_sdl2_clear_cache();
	assert(menu_sdl2_cache_entries() == 0);
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

int main(void)
{
	SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
	assert(SDL_Init(SDL_INIT_VIDEO) == 0);
	assert(TTF_Init() == 0);

	test_valid_renderer();
	test_rgb_and_rgba_background_scaling();
	test_missing_background();
	test_missing_font();
	test_invalid_geometry_and_reinitialization();
	test_rejects_overflowing_framebuffer_span();

	TTF_Quit();
	SDL_Quit();
	return 0;
}
