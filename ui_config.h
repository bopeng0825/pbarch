#ifndef UI_CONFIG_H
#define UI_CONFIG_H

#include <stddef.h>

#include "ui_language.h"

enum ui_config_load_status {
	UI_CONFIG_ABSENT = 0,
	UI_CONFIG_LOADED,
	UI_CONFIG_INVALID
};

enum ui_language_choice_status {
	UI_LANGUAGE_CHOICE_OK = 0,
	UI_LANGUAGE_CHOICE_WARN_CONFIG,
	UI_LANGUAGE_CHOICE_WARN_LANGUAGE
};

struct app_args {
	const char *language_override;
	const char *core_path;
	const char *content_path;
	int show_help;
	int full_menu;
};

int ui_config_parse(const char *text, char *language, size_t language_size);
enum ui_config_load_status ui_config_load(char *language,
					  size_t language_size);
enum ui_language_choice_status ui_language_resolve(
	enum ui_config_load_status config_status, const char *config_language,
	const char *language_override, int allow_non_english,
	enum ui_language *language);
int app_args_parse(int argc, char **argv, struct app_args *out);

#endif
