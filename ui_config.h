#ifndef UI_CONFIG_H
#define UI_CONFIG_H

#include <stddef.h>

struct app_args {
	const char *language_override;
	const char *core_path;
	const char *content_path;
	int show_help;
};

int ui_config_parse(const char *text, char *language, size_t language_size);
int ui_config_load(char *language, size_t language_size);
int app_args_parse(int argc, char **argv, struct app_args *out);

#endif
