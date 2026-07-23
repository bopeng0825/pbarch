#include <string.h>
#include "video_direct.h"

void video_direct_begin(struct video_direct_state *state)
{
	memset(state, 0, sizeof(*state));
	state->frame_active = true;
}

void video_direct_offer(struct video_direct_state *state, void *data,
			unsigned width, unsigned height, size_t pitch)
{
	if (!state->frame_active || !data || !width || !height || !pitch)
		return;

	state->data = data;
	state->width = width;
	state->height = height;
	state->pitch = pitch;
	state->offered = true;
}

bool video_direct_match(struct video_direct_state *state, const void *data,
			unsigned width, unsigned height, size_t pitch)
{
	return video_direct_classify(state, data, width, height, pitch) ==
		VIDEO_DIRECT_ACCEPT;
}

enum video_direct_result video_direct_classify(
			struct video_direct_state *state, const void *data,
			unsigned width, unsigned height, size_t pitch)
{
	bool exact;

	if (!state->frame_active || !state->offered || state->data != data)
		return VIDEO_DIRECT_EXTERNAL;

	exact = state->width == width && state->height == height &&
		state->pitch == pitch;
	if (!exact)
		return VIDEO_DIRECT_OFFERED_MISMATCH;
	if (state->matched)
		return VIDEO_DIRECT_REPEAT;

	state->matched = true;
	return VIDEO_DIRECT_ACCEPT;
}

void video_direct_end(struct video_direct_state *state)
{
	memset(state, 0, sizeof(*state));
}

void video_direct_validity_offer(struct video_direct_validity *state)
{
	state->valid = false;
}

void video_direct_validity_accept(struct video_direct_validity *state)
{
	state->valid = true;
}

void video_direct_validity_upload(struct video_direct_validity *state)
{
	state->valid = true;
}

void video_direct_validity_reset(struct video_direct_validity *state)
{
	state->valid = false;
}

bool video_direct_validity_can_dupe(const struct video_direct_validity *state)
{
	return state->valid;
}
