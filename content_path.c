#include <libgen.h>
#include <stdio.h>
#include <string.h>

#include "content.h"

void content_based_name(const struct content *content,
			char *buf, size_t len,
			const char *basedir, const char *subdir,
			const char *new_extension)
{
	char filename[MAX_PATH];
	char path[MAX_PATH];
	char *name;
	char *dot;
	size_t path_prefix_len = 0;

	strncpy(path, content->path, sizeof(path));
	path[sizeof(path) - 1] = 0;

	if (basedir) {
		if (!subdir)
			subdir = "";

		name = basename(path);
	} else {
		basedir = "";
		if (subdir) {
			name = strrchr(path, '/');
			name = name ? name + 1 : path;
			path_prefix_len = name - path;
		} else {
			subdir = "";
			name = path;
		}
	}
	strncpy(filename, name, sizeof(filename));

	filename[sizeof(filename) - 1] = 0;

	dot = strrchr(filename, '.');
	if (dot)
		*dot = 0;

	if (path_prefix_len) {
		snprintf(buf, len, "%.*s%s%s%s", (int)path_prefix_len, path,
			 subdir, filename, new_extension);
	} else {
		snprintf(buf, len, "%s%s%s%s", basedir, subdir, filename,
			 new_extension);
	}
}
