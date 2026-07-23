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
	struct video_direct_validity validity = {0};

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
	assert(video_direct_classify(&state, pixels, 320, 240, 640) ==
	       VIDEO_DIRECT_ACCEPT);
	assert(video_direct_classify(&state, pixels, 320, 240, 640) ==
	       VIDEO_DIRECT_REPEAT);
	assert(video_direct_classify(&state, pixels, 319, 240, 640) ==
	       VIDEO_DIRECT_OFFERED_MISMATCH);
	assert(video_direct_classify(&state, pixels + 1, 320, 240, 640) ==
	       VIDEO_DIRECT_EXTERNAL);
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

	/* Query plus NULL/no callback leaves later duplicate frames suppressed. */
	video_direct_validity_upload(&validity);
	assert(video_direct_validity_can_dupe(&validity));
	video_direct_validity_offer(&validity);
	assert(!video_direct_validity_can_dupe(&validity));
	video_direct_begin(&state);
	video_direct_end(&state);
	assert(!video_direct_validity_can_dupe(&validity));

	/* An accepted direct callback or full upload restores validity. */
	video_direct_validity_accept(&validity);
	assert(video_direct_validity_can_dupe(&validity));
	video_direct_validity_offer(&validity);
	assert(!video_direct_validity_can_dupe(&validity));
	video_direct_validity_upload(&validity);
	assert(video_direct_validity_can_dupe(&validity));
	video_direct_validity_reset(&validity);
	assert(!video_direct_validity_can_dupe(&validity));
	return 0;
}
