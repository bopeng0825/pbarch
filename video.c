#include "video.h"
#include "main.h"
#include "plat.h"

static struct {
	unsigned max_width;
	unsigned max_height;
	enum retro_pixel_format pixel_format;
	uint16_t *buffer;
} screen_def;

static void video_alloc_convert_buffer(
	unsigned new_width,
	unsigned new_height,
	enum retro_pixel_format new_format
) {
	if (new_width == screen_def.max_width &&
	    new_height == screen_def.max_height &&
	    new_format == screen_def.pixel_format)
		return;

	if (screen_def.buffer) {
		free(screen_def.buffer);
		screen_def.buffer = NULL;
	}

	if ((new_width > 0 || new_height > 0) && new_format == RETRO_PIXEL_FORMAT_XRGB8888) {
		screen_def.buffer = malloc(new_width * new_height * sizeof(uint16_t));
		if (!screen_def.buffer)
			PA_FATAL("Can't allocate buffer for color format conversion\n");
	}
}

void video_set_geometry(struct retro_game_geometry *geometry) {
	video_alloc_convert_buffer(geometry->max_width,
	                           geometry->max_height,
	                           screen_def.pixel_format);

	screen_def.max_width = geometry->max_width;
	screen_def.max_height = geometry->max_height;
}

void video_set_pixel_format(enum retro_pixel_format format) {
	video_alloc_convert_buffer(screen_def.max_width,
	                           screen_def.max_height,
	                           format);

	screen_def.pixel_format = format;
}

enum retro_pixel_format video_get_pixel_format(void) {
	return screen_def.pixel_format;
}

bool video_pitch_is_valid(enum retro_pixel_format format,
			  unsigned width, size_t pitch) {
	size_t bytes_per_pixel;

	switch (format) {
	case RETRO_PIXEL_FORMAT_RGB565:
		bytes_per_pixel = sizeof(uint16_t);
		break;
	case RETRO_PIXEL_FORMAT_XRGB8888:
		bytes_per_pixel = sizeof(uint32_t);
		break;
	default:
		return false;
	}

	return width > 0 && pitch >= (size_t)width * bytes_per_pixel;
}

void video_process(const void *data, unsigned width, unsigned height, size_t pitch) {
	const uint32_t *input = data;
	uint16_t *output = screen_def.buffer;
	size_t extra = pitch / sizeof(uint32_t) - width;

	if (screen_def.pixel_format != RETRO_PIXEL_FORMAT_XRGB8888)
		return plat_video_process(data, width, height, pitch);

#ifdef USE_SDL2
	return plat_video_process(data, width, height, pitch);
#endif

	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			*output =  (*input & 0xF80000) >> 8;
			*output |= (*input & 0xFC00) >> 5;
			*output |= (*input & 0xF8) >> 3;
			input++;
			output++;
		}

		input += extra;
	}

	plat_video_process(screen_def.buffer, width, height, width * sizeof(uint16_t));
}

void video_deinit(void) {
	free(screen_def.buffer);
	screen_def.buffer = NULL;
}
