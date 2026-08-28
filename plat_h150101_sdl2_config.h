#ifndef PLAT_H150101_SDL2_CONFIG_H
#define PLAT_H150101_SDL2_CONFIG_H

#include <string.h>

#define IN_H150101_SDL2_PREFIX "h150101-sdl2:"

static inline int h150101_sdl2_config_match(const char *configured_name,
	const char *device_name)
{
	size_t prefix_len = strlen(IN_H150101_SDL2_PREFIX);
	const char *p2_prefix = IN_H150101_SDL2_PREFIX "p2:";
	size_t p2_len = strlen(p2_prefix);

	if (strcmp(configured_name, device_name) == 0)
		return 1;
	if (strncmp(configured_name, IN_H150101_SDL2_PREFIX, prefix_len) != 0 ||
	    strncmp(device_name, p2_prefix, p2_len) != 0)
		return 0;
	if (strncmp(configured_name + prefix_len, "p2:", 3) == 0)
		return 0;

	return strcmp(configured_name + prefix_len, device_name + p2_len) == 0;
}

#endif
