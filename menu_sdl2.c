#include "menu_sdl2.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <png.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include "menu_layout.h"
#include "text_cache.h"

struct menu_sdl2_state {
	int width;
	int height;
	int main_px;
	int small_px;
	TTF_Font *fonts[2];
	uint16_t *background;
	SDL_Surface *destination;
	void *destination_pixels;
	int destination_pitch;
	struct text_cache cache;
};

static struct menu_sdl2_state state;

static void release_surface(void *value)
{
	SDL_FreeSurface(value);
}

static int role_valid(enum menu_font_role role)
{
	return role == MENU_FONT_MAIN || role == MENU_FONT_SMALL;
}

static uint16_t rgb565(unsigned char red, unsigned char green,
		       unsigned char blue)
{
	return (uint16_t)(((red & 0xf8u) << 8) |
			  ((green & 0xfcu) << 3) |
			  (blue >> 3));
}

static void load_background(const char *path)
{
	png_image image;
	unsigned char *rgb = NULL;
	size_t rgb_size;
	size_t image_row;
	int x;
	int y;

	if (path == NULL)
		return;

	memset(&image, 0, sizeof(image));
	image.version = PNG_IMAGE_VERSION;
	if (!png_image_begin_read_from_file(&image, path)) {
		fprintf(stderr, "menu: unable to load background %s: %s\n",
			path, image.message);
		return;
	}

	image.format = PNG_FORMAT_RGB;
	if (image.width == 0 || image.height == 0 ||
	    image.width > SIZE_MAX / 3u ||
	    image.height > SIZE_MAX / ((size_t)image.width * 3u)) {
		fprintf(stderr, "menu: invalid background dimensions in %s\n", path);
		png_image_free(&image);
		return;
	}

	image_row = (size_t)image.width * 3u;
	rgb_size = image_row * image.height;
	rgb = malloc(rgb_size);
	if (rgb == NULL) {
		fprintf(stderr, "menu: unable to allocate background decode buffer\n");
		png_image_free(&image);
		return;
	}

	if (!png_image_finish_read(&image, NULL, rgb, 0, NULL)) {
		fprintf(stderr, "menu: unable to decode background %s: %s\n",
			path, image.message);
		free(rgb);
		png_image_free(&image);
		return;
	}

	for (y = 0; y < state.height; y++) {
		size_t source_y = (size_t)y * image.height / state.height;

		for (x = 0; x < state.width; x++) {
			size_t source_x = (size_t)x * image.width / state.width;
			size_t source = source_y * image_row + source_x * 3u;

			state.background[(size_t)y * state.width + x] =
				rgb565(rgb[source], rgb[source + 1],
				       rgb[source + 2]);
		}
	}

	free(rgb);
	png_image_free(&image);
}

static SDL_Surface *destination_surface(uint16_t *pixels, int pitch_pixels)
{
	if (pixels == NULL || pitch_pixels < state.width ||
	    pitch_pixels > INT_MAX / (int)sizeof(*pixels))
		return NULL;

	if (state.destination != NULL &&
	    state.destination_pixels == pixels &&
	    state.destination_pitch == pitch_pixels)
		return state.destination;

	SDL_FreeSurface(state.destination);
	state.destination = SDL_CreateRGBSurfaceWithFormatFrom(
		pixels, state.width, state.height, 16,
		pitch_pixels * (int)sizeof(*pixels), SDL_PIXELFORMAT_RGB565);
	if (state.destination == NULL) {
		state.destination_pixels = NULL;
		state.destination_pitch = 0;
		fprintf(stderr, "menu: unable to wrap RGB565 framebuffer: %s\n",
			SDL_GetError());
		return NULL;
	}

	state.destination_pixels = pixels;
	state.destination_pitch = pitch_pixels;
	return state.destination;
}

static SDL_Color sdl_color(uint16_t color)
{
	SDL_Color result;

	result.r = (Uint8)(((color >> 11) & 0x1f) * 255 / 31);
	result.g = (Uint8)(((color >> 5) & 0x3f) * 255 / 63);
	result.b = (Uint8)((color & 0x1f) * 255 / 31);
	result.a = SDL_ALPHA_OPAQUE;
	return result;
}

int menu_sdl2_init(const char *font_path, const char *background_path,
		   int width, int height)
{
	size_t count;
	size_t i;

	menu_sdl2_finish();
	if (width <= 0 || height <= 0 ||
	    (size_t)width > SIZE_MAX / (size_t)height ||
	    (size_t)width * (size_t)height > SIZE_MAX / sizeof(uint16_t)) {
		fprintf(stderr, "menu: invalid SDL2 menu dimensions %dx%d\n",
			width, height);
		return -1;
	}

	state.width = width;
	state.height = height;
	state.main_px = menu_main_font_px(height);
	state.small_px = menu_small_font_px(state.main_px);
	text_cache_init(&state.cache, MENU_TEXT_CACHE_LIMIT, release_surface);

	count = (size_t)width * (size_t)height;
	state.background = malloc(count * sizeof(*state.background));
	if (state.background == NULL) {
		fprintf(stderr, "menu: unable to allocate SDL2 menu background\n");
		menu_sdl2_finish();
		return -1;
	}
	for (i = 0; i < count; i++)
		state.background[i] = MENU_FALLBACK_BG;
	load_background(background_path);

	if (font_path == NULL) {
		fprintf(stderr, "menu: no SDL2 menu font path configured\n");
		return 0;
	}

	state.fonts[MENU_FONT_MAIN] = TTF_OpenFont(font_path, state.main_px);
	if (state.fonts[MENU_FONT_MAIN] == NULL)
		fprintf(stderr, "menu: unable to open font %s at %d px: %s\n",
			font_path, state.main_px, TTF_GetError());
	state.fonts[MENU_FONT_SMALL] = TTF_OpenFont(font_path, state.small_px);
	if (state.fonts[MENU_FONT_SMALL] == NULL)
		fprintf(stderr, "menu: unable to open font %s at %d px: %s\n",
			font_path, state.small_px, TTF_GetError());

	if (!menu_sdl2_available()) {
		if (state.fonts[MENU_FONT_MAIN] != NULL)
			TTF_CloseFont(state.fonts[MENU_FONT_MAIN]);
		if (state.fonts[MENU_FONT_SMALL] != NULL)
			TTF_CloseFont(state.fonts[MENU_FONT_SMALL]);
		state.fonts[MENU_FONT_MAIN] = NULL;
		state.fonts[MENU_FONT_SMALL] = NULL;
	}
	return 0;
}

int menu_sdl2_available(void)
{
	return state.fonts[MENU_FONT_MAIN] != NULL &&
	       state.fonts[MENU_FONT_SMALL] != NULL;
}

int menu_sdl2_main_font_px(void)
{
	return state.main_px;
}

int menu_sdl2_small_font_px(void)
{
	return state.small_px;
}

int menu_sdl2_line_height(enum menu_font_role role)
{
	if (!role_valid(role) || state.fonts[role] == NULL)
		return 0;
	return TTF_FontLineSkip(state.fonts[role]);
}

int menu_sdl2_text_width(enum menu_font_role role, const char *utf8)
{
	int width;

	if (!role_valid(role) || state.fonts[role] == NULL || utf8 == NULL)
		return 0;
	if (TTF_SizeUTF8(state.fonts[role], utf8, &width, NULL) != 0) {
		fprintf(stderr, "menu: unable to measure UTF-8 text: %s\n",
			TTF_GetError());
		return 0;
	}
	return width;
}

int menu_sdl2_draw_text(uint16_t *pixels, int pitch_pixels,
			enum menu_font_role role, int x, int y,
			uint16_t color, const char *utf8)
{
	SDL_Surface *destination;
	SDL_Surface *rendered;
	SDL_Rect target;
	size_t bytes;
	int cached;

	if (!role_valid(role) || state.fonts[role] == NULL || utf8 == NULL)
		return -1;
	destination = destination_surface(pixels, pitch_pixels);
	if (destination == NULL)
		return -1;

	rendered = text_cache_get(&state.cache, (unsigned)role, color, utf8);
	cached = rendered != NULL;
	if (!cached) {
		rendered = TTF_RenderUTF8_Blended(state.fonts[role], utf8,
						 sdl_color(color));
		if (rendered == NULL) {
			fprintf(stderr, "menu: unable to render UTF-8 text: %s\n",
				TTF_GetError());
			return -1;
		}
		bytes = (size_t)rendered->pitch * (size_t)rendered->h;
		if (text_cache_put(&state.cache, (unsigned)role, color, utf8,
				   rendered, bytes) == 0)
			cached = 1;
	}

	target.x = x;
	target.y = y;
	target.w = 0;
	target.h = 0;
	if (SDL_BlitSurface(rendered, NULL, destination, &target) != 0) {
		fprintf(stderr, "menu: unable to blit UTF-8 text: %s\n",
			SDL_GetError());
		if (!cached)
			SDL_FreeSurface(rendered);
		return -1;
	}
	if (!cached)
		SDL_FreeSurface(rendered);
	return 0;
}

void menu_sdl2_copy_background(uint16_t *pixels, int pitch_pixels)
{
	int y;

	if (pixels == NULL || state.background == NULL ||
	    pitch_pixels < state.width ||
	    (size_t)pitch_pixels > SIZE_MAX / (size_t)state.height)
		return;
	for (y = 0; y < state.height; y++)
		memcpy(pixels + (size_t)y * pitch_pixels,
		       state.background + (size_t)y * state.width,
		       (size_t)state.width * sizeof(*pixels));
}

size_t menu_sdl2_cache_entries(void)
{
	return text_cache_count(&state.cache);
}

void menu_sdl2_clear_cache(void)
{
	text_cache_clear(&state.cache);
}

void menu_sdl2_finish(void)
{
	text_cache_clear(&state.cache);
	SDL_FreeSurface(state.destination);
	if (state.fonts[MENU_FONT_MAIN] != NULL)
		TTF_CloseFont(state.fonts[MENU_FONT_MAIN]);
	if (state.fonts[MENU_FONT_SMALL] != NULL)
		TTF_CloseFont(state.fonts[MENU_FONT_SMALL]);
	free(state.background);
	memset(&state, 0, sizeof(state));
}
