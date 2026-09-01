#include <stdio.h>
#include <string.h>

#include "content.h"

static int check_path(const char *content_path, const char *basedir,
		      const char *subdir, const char *expected)
{
	struct content content = {0};
	char actual[MAX_PATH];
	int failed = 0;

	strncpy((char *)content.path, content_path, sizeof(content.path) - 1);
	content_based_name(&content, actual, sizeof(actual), basedir, subdir, ".cht");
	if (strcmp(actual, expected)) {
		fprintf(stderr, "expected %s, got %s\n", expected, actual);
		failed = 1;
	}

	return failed;
}

int main(void)
{
	int failed = 0;

	failed |= check_path("/mnt/GBA/Dragon Ball.gba", NULL, "cheats/",
			     "/mnt/GBA/cheats/Dragon Ball.cht");
	failed |= check_path("/mnt/GBA/Dragon Ball.zip", NULL, "cheats/",
			     "/mnt/GBA/cheats/Dragon Ball.cht");
	failed |= check_path("/mnt/GBA/Dragon Ball.gba", "/save/gpsp/", "cheats/",
			     "/save/gpsp/cheats/Dragon Ball.cht");

	return failed;
}
