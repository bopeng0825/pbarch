#include <assert.h>
#include <stdint.h>
#include "video_direct.h"

int main(void)
{
	uint16_t pixels[320 * 240];
	struct video_direct_state state = {0};

	video_direct_begin(&state);
	video_direct_offer(&state, pixels, 320, 240, 640);
	assert(video_direct_match(&state, pixels, 320, 240, 640));
	assert(!video_direct_match(&state, pixels + 1, 320, 240, 640));
	assert(!video_direct_match(&state, pixels, 320, 239, 640));
	assert(!video_direct_match(&state, pixels, 320, 240, 642));
	video_direct_end(&state);
	assert(!video_direct_match(&state, pixels, 320, 240, 640));
	return 0;
}
