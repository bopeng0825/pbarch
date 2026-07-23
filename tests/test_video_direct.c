#include <assert.h>
#include <stdint.h>
#include "video_direct.h"

static void offer(struct video_direct_state *state, void *data,
		  unsigned width, unsigned height, size_t pitch)
{
	video_direct_begin(state);
	video_direct_offer(state, data, width, height, pitch);
}

int main(void)
{
	uint16_t pixels[320 * 240];
	struct video_direct_state state = {0};

	video_direct_offer(&state, pixels, 320, 240, 640);
	assert(!video_direct_match(&state, pixels, 320, 240, 640));

	offer(&state, NULL, 320, 240, 640);
	assert(!video_direct_match(&state, NULL, 320, 240, 640));
	offer(&state, pixels, 0, 240, 640);
	assert(!video_direct_match(&state, pixels, 0, 240, 640));
	offer(&state, pixels, 320, 0, 640);
	assert(!video_direct_match(&state, pixels, 320, 0, 640));
	offer(&state, pixels, 320, 240, 0);
	assert(!video_direct_match(&state, pixels, 320, 240, 0));

	offer(&state, pixels, 320, 240, 640);
	assert(!video_direct_match(&state, pixels + 1, 320, 240, 640));
	offer(&state, pixels, 320, 240, 640);
	assert(!video_direct_match(&state, pixels, 319, 240, 640));
	offer(&state, pixels, 320, 240, 640);
	assert(!video_direct_match(&state, pixels, 320, 239, 640));
	offer(&state, pixels, 320, 240, 640);
	assert(!video_direct_match(&state, pixels, 320, 240, 642));

	offer(&state, pixels, 320, 240, 640);
	assert(video_direct_match(&state, pixels, 320, 240, 640));
	assert(!video_direct_match(&state, pixels, 320, 240, 640));

	offer(&state, pixels, 320, 240, 640);
	video_direct_begin(&state);
	assert(!video_direct_match(&state, pixels, 320, 240, 640));
	video_direct_offer(&state, pixels, 320, 240, 640);
	assert(video_direct_match(&state, pixels, 320, 240, 640));
	video_direct_begin(&state);
	assert(!video_direct_match(&state, pixels, 320, 240, 640));

	video_direct_end(&state);
	assert(!video_direct_match(&state, pixels, 320, 240, 640));
	return 0;
}
