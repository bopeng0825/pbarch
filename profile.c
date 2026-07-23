#include <stdlib.h>
#include <string.h>
#include "profile.h"

static int profile_enabled = -1;

bool profile_is_enabled(void)
{
	const char *value;

	if (profile_enabled >= 0)
		return profile_enabled != 0;
	value = getenv("PICOARCH_PROFILE");
	profile_enabled = value && value[0] && strcmp(value, "0") ? 1 : 0;
	return profile_enabled != 0;
}

void profile_reset_enabled_cache(void)
{
	profile_enabled = -1;
}
