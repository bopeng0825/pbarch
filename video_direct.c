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
	if (!state->frame_active || !state->offered || state->matched)
		return false;

	if (state->data != data || state->width != width ||
	    state->height != height || state->pitch != pitch)
		return false;

	state->matched = true;
	return true;
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
