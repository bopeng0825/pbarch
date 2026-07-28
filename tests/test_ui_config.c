#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "ui_config.h"

static const char *root_dir = "/tmp/test/";

int plat_get_root_dir(char *dst, int len)
{
	size_t root_len = strlen(root_dir);

	assert(len > (int)root_len);
	strcpy(dst, root_dir);
	return (int)root_len;
}

int main(void)
{
	struct app_args args;
	char language[16];
	char *argv1[] = {
		"picoarch", "--language", "zh_TW", "core.so", "game.rom"
	};
	char *argv2[] = {
		"picoarch", "--language=zh-CN", "core.so"
	};
	char *help_argv[] = { "picoarch", "--help" };
	char *missing_language_argv[] = { "picoarch", "--language" };
	char *unknown_option_argv[] = { "picoarch", "--bogus" };
	char *too_many_argv[] = {
		"picoarch", "core.so", "game.rom", "extra"
	};
	FILE *config_file;

	assert(ui_config_parse("language = zh_CN\n", language,
			       sizeof(language)) == 0);
	assert(strcmp(language, "zh_CN") == 0);
	assert(ui_config_parse("\n ; comment\n# comment\nlanguage=zh-TW\n",
			       language, sizeof(language)) == 0);
	assert(strcmp(language, "zh-TW") == 0);
	assert(ui_config_parse("# comment\nlanguage bad\n", language,
			       sizeof(language)) == -1);
	assert(ui_config_parse("unknown=value\n", language,
			       sizeof(language)) == -1);
	assert(ui_config_parse("language=en\nlanguage=zh_CN\n", language,
			       sizeof(language)) == -1);
	assert(ui_config_parse("language=0123456789abcdef\n", language,
			       sizeof(language)) == -1);

	root_dir = "tests/";
	config_file = fopen("tests/ui.cfg", "wb");
	assert(config_file != NULL);
	assert(fputs("language = zh_TW\n", config_file) >= 0);
	assert(fclose(config_file) == 0);
	assert(ui_config_load(language, sizeof(language)) == 0);
	assert(strcmp(language, "zh_TW") == 0);
	assert(remove("tests/ui.cfg") == 0);

	assert(app_args_parse(5, argv1, &args) == 0);
	assert(strcmp(args.language_override, "zh_TW") == 0);
	assert(strcmp(args.core_path, "core.so") == 0);
	assert(strcmp(args.content_path, "game.rom") == 0);

	assert(app_args_parse(3, argv2, &args) == 0);
	assert(strcmp(args.language_override, "zh-CN") == 0);
	assert(strcmp(args.core_path, "core.so") == 0);
	assert(args.content_path == NULL);

	assert(app_args_parse(2, help_argv, &args) == 0);
	assert(args.show_help);
	assert(app_args_parse(2, missing_language_argv, &args) == -1);
	assert(app_args_parse(2, unknown_option_argv, &args) == -1);
	assert(app_args_parse(4, too_many_argv, &args) == -1);
	return 0;
}
