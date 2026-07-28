#include <string.h>

#include "ui_language.h"
#include "ui_catalog_data.h"

static enum ui_language current_language = UI_LANG_EN;

enum ui_language ui_language_parse(const char *code)
{
	if (code == NULL)
		return UI_LANG_EN;
	if (strcmp(code, "zh_CN") == 0 || strcmp(code, "zh-CN") == 0)
		return UI_LANG_ZH_CN;
	if (strcmp(code, "zh_TW") == 0 || strcmp(code, "zh-TW") == 0)
		return UI_LANG_ZH_TW;
	return UI_LANG_EN;
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
