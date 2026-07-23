#ifndef __VIDEO_DIRECT_H__
#define __VIDEO_DIRECT_H__

#include <stdbool.h>
#include <stddef.h>

struct video_direct_state {
	void *data;
	unsigned width;
	unsigned height;
	size_t pitch;
	bool frame_active;
	bool offered;
	bool matched;
};

void video_direct_begin(struct video_direct_state *state);
void video_direct_offer(struct video_direct_state *state, void *data,
			unsigned width, unsigned height, size_t pitch);
bool video_direct_match(struct video_direct_state *state, const void *data,
			unsigned width, unsigned height, size_t pitch);
void video_direct_end(struct video_direct_state *state);

#endif
