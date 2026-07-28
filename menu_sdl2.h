#ifndef MENU_SDL2_H
#define MENU_SDL2_H

#include <stddef.h>
#include <stdint.h>

enum menu_font_role {
	MENU_FONT_MAIN = 0,
	MENU_FONT_SMALL
};

#define MENU_FALLBACK_BG 0x18e3
#define MENU_TEXT_CACHE_LIMIT (512u * 1024u)

int menu_sdl2_init(const char *font_path, const char *background_path,
		   int width, int height);
int menu_sdl2_available(void);
int menu_sdl2_main_font_px(void);
int menu_sdl2_small_font_px(void);
int menu_sdl2_line_height(enum menu_font_role role);
int menu_sdl2_text_width(enum menu_font_role role, const char *utf8);
uint32_t menu_sdl2_scale_coordinate(uint32_t coordinate,
				    uint32_t source_size,
				    uint32_t destination_size);
int menu_sdl2_draw_text(uint16_t *pixels, int pitch_pixels,
			enum menu_font_role role, int x, int y,
			uint16_t color, const char *utf8);
void menu_sdl2_copy_background(uint16_t *pixels, int pitch_pixels);
size_t menu_sdl2_cache_entries(void);
void menu_sdl2_clear_cache(void);
void menu_sdl2_finish(void);

#endif
