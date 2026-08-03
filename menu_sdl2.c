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

#define MENU_MAX_DIMENSION 8192

#ifdef MENU_SDL2_TEST_ALLOC
static int allocations_before_failure = -1;

static void *menu_sdl2_test_malloc(size_t size)
{
	if (allocations_before_failure == 0)
		return NULL;
	if (allocations_before_failure > 0)
		allocations_before_failure--;
	return malloc(size);
}

#define malloc menu_sdl2_test_malloc
#endif

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
	struct text_cache metric_cache;
#ifdef MENU_SDL2_TEST
	unsigned measure_calls;
#endif
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

static int framebuffer_span_valid(int pitch_pixels, int height)
{
	if (pitch_pixels <= 0 || height <= 0)
		return 0;
	return (size_t)pitch_pixels <=
	       SIZE_MAX / sizeof(uint16_t) / (size_t)height;
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
		uint32_t source_y = menu_sdl2_scale_coordinate(
			(uint32_t)y, image.height, (uint32_t)state.height);

		for (x = 0; x < state.width; x++) {
			uint32_t source_x = menu_sdl2_scale_coordinate(
				(uint32_t)x, image.width, (uint32_t)state.width);
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
	    pitch_pixels > INT_MAX / (int)sizeof(*pixels) ||
	    !framebuffer_span_valid(pitch_pixels, state.height))
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
	    width > MENU_MAX_DIMENSION || height > MENU_MAX_DIMENSION ||
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
	text_cache_init(&state.metric_cache, MENU_METRIC_CACHE_LIMIT, free);

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
	int *cached_width;
	size_t text_bytes;
	int width;

	if (!role_valid(role) || state.fonts[role] == NULL || utf8 == NULL)
		return 0;
	cached_width = text_cache_get(&state.metric_cache, (unsigned)role, 0,
				      utf8);
	if (cached_width != NULL)
		return *cached_width;
#ifdef MENU_SDL2_TEST
	state.measure_calls++;
#endif
	if (TTF_SizeUTF8(state.fonts[role], utf8, &width, NULL) != 0) {
		fprintf(stderr, "menu: unable to measure UTF-8 text: %s\n",
			TTF_GetError());
		return 0;
	}
	text_bytes = strlen(utf8) + 1;
	if (text_bytes <= SIZE_MAX - sizeof(*cached_width)) {
		cached_width = malloc(sizeof(*cached_width));
		if (cached_width != NULL) {
			*cached_width = width;
			if (text_cache_put(&state.metric_cache, (unsigned)role, 0,
					   utf8, cached_width,
					   sizeof(*cached_width) + text_bytes) != 0)
				free(cached_width);
		}
	}
	return width;
}

uint32_t menu_sdl2_scale_coordinate(uint32_t coordinate,
				    uint32_t source_size,
				    uint32_t destination_size)
{
	if (destination_size == 0)
		return 0;
	return (uint32_t)((uint64_t)coordinate * source_size /
			  destination_size);
}

int menu_sdl2_draw_preview(uint16_t *destination, int destination_pitch,
			   const uint16_t *source, int source_width,
			   int source_height, int source_pitch,
			   const struct menu_rect *bounds)
{
	struct menu_rect fitted;
	int destination_height;
	int y;

	if (destination == NULL || source == NULL || bounds == NULL ||
	    source_width <= 0 || source_height <= 0 ||
	    source_pitch < source_width || bounds->x < 0 || bounds->y < 0 ||
	    bounds->w <= 0 || bounds->h <= 0 ||
	    bounds->x > INT_MAX - bounds->w ||
	    bounds->y > INT_MAX - bounds->h)
		return -1;

	menu_aspect_fit(source_width, source_height, bounds, &fitted);
	if (fitted.w <= 0 || fitted.h <= 0 ||
	    fitted.x > INT_MAX - fitted.w || fitted.y > INT_MAX - fitted.h ||
	    destination_pitch < fitted.x + fitted.w)
		return -1;
	destination_height = fitted.y + fitted.h;
	if (!framebuffer_span_valid(source_pitch, source_height) ||
	    !framebuffer_span_valid(destination_pitch, destination_height))
		return -1;

	for (y = 0; y < fitted.h; y++) {
		uint32_t source_y = menu_sdl2_scale_coordinate(
			(uint32_t)y, (uint32_t)source_height,
			(uint32_t)fitted.h);
		int x;

		for (x = 0; x < fitted.w; x++) {
			uint32_t source_x = menu_sdl2_scale_coordinate(
				(uint32_t)x, (uint32_t)source_width,
				(uint32_t)fitted.w);

			destination[(size_t)(fitted.y + y) *
				    (size_t)destination_pitch + fitted.x + x] =
				source[(size_t)source_y * (size_t)source_pitch +
				       source_x];
		}
	}
	return 0;
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
	    !framebuffer_span_valid(pitch_pixels, state.height))
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
	text_cache_clear(&state.metric_cache);
}

void menu_sdl2_finish(void)
{
	text_cache_clear(&state.cache);
	text_cache_clear(&state.metric_cache);
	SDL_FreeSurface(state.destination);
	if (state.fonts[MENU_FONT_MAIN] != NULL)
		TTF_CloseFont(state.fonts[MENU_FONT_MAIN]);
	if (state.fonts[MENU_FONT_SMALL] != NULL)
		TTF_CloseFont(state.fonts[MENU_FONT_SMALL]);
	free(state.background);
	memset(&state, 0, sizeof(state));
}

#ifdef MENU_SDL2_TEST
size_t menu_sdl2_metric_cache_entries(void)
{
	return text_cache_count(&state.metric_cache);
}

unsigned menu_sdl2_measure_calls(void)
{
	return state.measure_calls;
}
#endif

#ifdef MENU_SDL2_TEST_ALLOC
void menu_sdl2_test_fail_allocations_after(int count)
{
	allocations_before_failure = count;
}
#endif
