#include <assert.h>
#include <stdlib.h>
#include "profile.h"

static void check(const char *value, bool expected)
{
	if (value)
		setenv("PICOARCH_PROFILE", value, 1);
	else
		unsetenv("PICOARCH_PROFILE");
	profile_reset_enabled_cache();
	assert(profile_is_enabled() == expected);
}

int main(void)
{
	check(NULL, false);
	check("", false);
	check("0", false);
	check("1", true);
	check("yes", true);
	return 0;
}
