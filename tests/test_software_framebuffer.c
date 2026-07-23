#include <assert.h>
#include <stdbool.h>
#include <stddef.h>

#include "plat.h"

int main(void)
{
	struct retro_framebuffer framebuffer = {0};

	framebuffer.width = 320;
	framebuffer.height = 240;
	framebuffer.access_flags = RETRO_MEMORY_ACCESS_WRITE;

	plat_video_frame_begin();
	assert(!plat_video_get_software_framebuffer(&framebuffer));
	assert(!plat_video_frame_is_direct(NULL, 320, 240, 640));
	plat_video_frame_end();
	return 0;
}
