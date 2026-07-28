#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include "menu_sdl2.h"

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

int main(void)
{
	SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
	assert(SDL_Init(SDL_INIT_VIDEO) == 0);
	assert(TTF_Init() == 0);

	test_valid_renderer();
	test_missing_background();
	test_missing_font();
	test_invalid_geometry_and_reinitialization();

	TTF_Quit();
	SDL_Quit();
	return 0;
}
