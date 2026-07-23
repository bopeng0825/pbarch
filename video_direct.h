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

struct video_direct_validity {
	bool valid;
};

enum video_direct_result {
	VIDEO_DIRECT_EXTERNAL,
	VIDEO_DIRECT_ACCEPT,
	VIDEO_DIRECT_REPEAT,
	VIDEO_DIRECT_OFFERED_MISMATCH,
};

void video_direct_begin(struct video_direct_state *state);
void video_direct_offer(struct video_direct_state *state, void *data,
			unsigned width, unsigned height, size_t pitch);
bool video_direct_match(struct video_direct_state *state, const void *data,
			unsigned width, unsigned height, size_t pitch);
enum video_direct_result video_direct_classify(
			struct video_direct_state *state, const void *data,
			unsigned width, unsigned height, size_t pitch);
void video_direct_end(struct video_direct_state *state);
void video_direct_validity_offer(struct video_direct_validity *state);
void video_direct_validity_accept(struct video_direct_validity *state);
void video_direct_validity_upload(struct video_direct_validity *state);
void video_direct_validity_reset(struct video_direct_validity *state);
bool video_direct_validity_can_dupe(const struct video_direct_validity *state);

#endif
