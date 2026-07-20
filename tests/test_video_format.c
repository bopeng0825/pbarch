#include <assert.h>
#include <stdarg.h>
#include <stddef.h>
#include "main.h"
#include "plat.h"
#include "video.h"

void pa_log(enum retro_log_level level, const char *fmt, ...)
{
	(void)level;
	(void)fmt;
}

void quit(int code)
{
	(void)code;
	assert(0 && "unexpected fatal error");
}

void plat_video_process(const void *data, unsigned width,
			unsigned height, size_t pitch)
{
	(void)data;
	(void)width;
	(void)height;
	(void)pitch;
}

int main(void)
{
	video_set_pixel_format(RETRO_PIXEL_FORMAT_RGB565);
	assert(video_get_pixel_format() == RETRO_PIXEL_FORMAT_RGB565);
	assert(video_pitch_is_valid(RETRO_PIXEL_FORMAT_RGB565, 256, 1024));
	assert(video_pitch_is_valid(RETRO_PIXEL_FORMAT_RGB565, 256, 512));
	assert(!video_pitch_is_valid(RETRO_PIXEL_FORMAT_RGB565, 256, 511));

	video_set_pixel_format(RETRO_PIXEL_FORMAT_XRGB8888);
	assert(video_get_pixel_format() == RETRO_PIXEL_FORMAT_XRGB8888);
	assert(video_pitch_is_valid(RETRO_PIXEL_FORMAT_XRGB8888, 256, 1024));
	assert(!video_pitch_is_valid(RETRO_PIXEL_FORMAT_XRGB8888, 256, 1023));
	assert(!video_pitch_is_valid(RETRO_PIXEL_FORMAT_0RGB1555, 256, 512));

	video_deinit();
	return 0;
}
