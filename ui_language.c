#include <string.h>

#include "ui_language.h"
#include "ui_catalog_data.h"

static enum ui_language current_language = UI_LANG_EN;

int ui_language_parse_checked(const char *code, enum ui_language *language)
{
	if (language == NULL)
		return 0;
	*language = UI_LANG_EN;
	if (code == NULL)
		return 0;
	if (strcmp(code, "en") == 0)
		return 1;
	if (strcmp(code, "zh_CN") == 0 || strcmp(code, "zh-CN") == 0) {
		*language = UI_LANG_ZH_CN;
		return 1;
	}
	if (strcmp(code, "zh_TW") == 0 || strcmp(code, "zh-TW") == 0) {
		*language = UI_LANG_ZH_TW;
		return 1;
	}
	return 0;
}

enum ui_language ui_language_parse(const char *code)
{
	enum ui_language language;

	ui_language_parse_checked(code, &language);
	return language;
}

void ui_language_set(enum ui_language language)
{
	if (language < UI_LANG_EN || language >= UI_LANG_COUNT)
		language = UI_LANG_EN;
	current_language = language;
}

enum ui_language ui_language_current(void)
{
	return current_language;
}

const char *ui_language_code(void)
{
	static const char *const codes[UI_LANG_COUNT] = {
		"en",
		"zh_CN",
		"zh_TW",
	};

	return codes[current_language];
}

const char *ui_text(enum ui_text_id id)
{
	const char *text;

	if (id < UI_TEXT_ON || id >= UI_TEXT_COUNT)
		return "";

	text = ui_catalog[id][current_language];
	if (text == NULL || text[0] == '\0')
		text = ui_catalog[id][UI_LANG_EN];
	if (text == NULL)
		return "";
	return text;
}
