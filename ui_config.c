#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "ui_config.h"

int plat_get_root_dir(char *dst, int len);

int ui_config_parse(const char *text, char *language, size_t language_size)
{
	const char *selected = NULL;
	size_t selected_len = 0;

	if (text == NULL || language == NULL || language_size == 0)
		return -1;

	while (*text != '\0') {
		const char *line_end = strchr(text, '\n');
		const char *start = text;
		const char *end;
		const char *equals;
		const char *key_end;
		const char *value;
		const char *value_end;

		if (line_end == NULL)
			line_end = text + strlen(text);
		end = line_end;
		while (start < end && isspace((unsigned char)*start))
			start++;
		while (end > start && isspace((unsigned char)end[-1]))
			end--;

		if (start != end && *start != '#' && *start != ';') {
			equals = memchr(start, '=', (size_t)(end - start));
			if (equals == NULL)
				return -1;

			key_end = equals;
			while (key_end > start &&
			       isspace((unsigned char)key_end[-1]))
				key_end--;
			value = equals + 1;
			while (value < end && isspace((unsigned char)*value))
				value++;
			value_end = end;
			while (value_end > value &&
			       isspace((unsigned char)value_end[-1]))
				value_end--;

			if ((size_t)(key_end - start) != strlen("language") ||
			    memcmp(start, "language", strlen("language")) != 0 ||
			    selected != NULL || value == value_end ||
			    (size_t)(value_end - value) >= language_size)
				return -1;

			selected = value;
			selected_len = (size_t)(value_end - value);
		}

		text = *line_end == '\0' ? line_end : line_end + 1;
	}

	if (selected == NULL)
		return -1;
	memcpy(language, selected, selected_len);
	language[selected_len] = '\0';
	return 0;
}

enum ui_config_load_status ui_config_load(char *language,
					  size_t language_size)
{
	char path[4096];
	char text[4096];
	size_t root_len;
	size_t bytes_read;
	FILE *file;
	int written;

	if (language == NULL || language_size == 0)
		return UI_CONFIG_INVALID;
	if (plat_get_root_dir(path, (int)sizeof(path)) < 0)
		return UI_CONFIG_INVALID;
	path[sizeof(path) - 1] = '\0';
	root_len = strlen(path);
	if (root_len >= sizeof(path))
		return UI_CONFIG_INVALID;
	written = snprintf(path + root_len, sizeof(path) - root_len, "%s",
			   "ui.cfg");
	if (written < 0 || (size_t)written >= sizeof(path) - root_len)
		return UI_CONFIG_INVALID;

	errno = 0;
	file = fopen(path, "rb");
	if (file == NULL)
		return errno == ENOENT ? UI_CONFIG_ABSENT : UI_CONFIG_INVALID;
	bytes_read = fread(text, 1, sizeof(text) - 1, file);
	if (ferror(file) || (!feof(file) && bytes_read == sizeof(text) - 1)) {
		fclose(file);
		return UI_CONFIG_INVALID;
	}
	fclose(file);
	text[bytes_read] = '\0';

	return ui_config_parse(text, language, language_size) == 0 ?
	       UI_CONFIG_LOADED : UI_CONFIG_INVALID;
}

enum ui_language_choice_status ui_language_resolve(
	enum ui_config_load_status config_status, const char *config_language,
	const char *language_override, int allow_non_english,
	enum ui_language *language)
{
	const char *effective;

	if (language == NULL)
		return UI_LANGUAGE_CHOICE_WARN_LANGUAGE;
	*language = UI_LANG_EN;

	if (language_override != NULL)
		effective = language_override;
	else if (config_status == UI_CONFIG_LOADED)
		effective = config_language;
	else if (config_status == UI_CONFIG_INVALID)
		return UI_LANGUAGE_CHOICE_WARN_CONFIG;
	else
		return UI_LANGUAGE_CHOICE_OK;

	if (!ui_language_parse_checked(effective, language))
		return UI_LANGUAGE_CHOICE_WARN_LANGUAGE;
	if (!allow_non_english)
		*language = UI_LANG_EN;
	return UI_LANGUAGE_CHOICE_OK;
}

int app_args_parse(int argc, char **argv, struct app_args *out)
{
	int positional_count = 0;
	int i;

	if (argc < 1 || argv == NULL || out == NULL)
		return -1;
	memset(out, 0, sizeof(*out));

	for (i = 1; i < argc; i++) {
		const char *arg = argv[i];

		if (arg == NULL)
			return -1;
		if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
			out->show_help = 1;
		} else if (strcmp(arg, "--language") == 0) {
			if (++i >= argc || argv[i] == NULL || argv[i][0] == '\0' ||
			    argv[i][0] == '-')
				return -1;
			out->language_override = argv[i];
		} else if (strncmp(arg, "--language=", strlen("--language=")) == 0) {
			if (arg[strlen("--language=")] == '\0')
				return -1;
			out->language_override = arg + strlen("--language=");
		} else if (strcmp(arg, "--key-config") == 0) {
			if (++i >= argc || argv[i] == NULL || argv[i][0] == '\0')
				return -1;
			out->key_config_path = argv[i];
		} else if (strncmp(arg, "--key-config=",
				   strlen("--key-config=")) == 0) {
			if (arg[strlen("--key-config=")] == '\0')
				return -1;
			out->key_config_path = arg + strlen("--key-config=");
		} else if (strcmp(arg, "--full-menu") == 0) {
			out->full_menu = 1;
		} else if (arg[0] == '-') {
			return -1;
		} else if (positional_count == 0) {
			out->core_path = arg;
			positional_count++;
		} else if (positional_count == 1) {
			out->content_path = arg;
			positional_count++;
		} else {
			return -1;
		}
	}

	return 0;
}
