#include <assert.h>
#include <string.h>
#include "ui_language.h"

int main(void)
{
	assert(ui_language_parse(NULL) == UI_LANG_EN);
	assert(ui_language_parse("en") == UI_LANG_EN);
	assert(ui_language_parse("zh_CN") == UI_LANG_ZH_CN);
	assert(ui_language_parse("zh-CN") == UI_LANG_ZH_CN);
	assert(ui_language_parse("zh_TW") == UI_LANG_ZH_TW);
	assert(ui_language_parse("zh-TW") == UI_LANG_ZH_TW);
	assert(ui_language_parse("fr") == UI_LANG_EN);

	ui_language_set(UI_LANG_ZH_CN);
	assert(strcmp(ui_language_code(), "zh_CN") == 0);
	assert(strcmp(ui_text(UI_TEXT_RESUME_GAME), "继续游戏") == 0);

	ui_language_set(UI_LANG_ZH_TW);
	assert(strcmp(ui_text(UI_TEXT_RESUME_GAME), "繼續遊戲") == 0);
	assert(strcmp(ui_text(UI_TEXT_TEST_ENGLISH_FALLBACK),
		      "English fallback") == 0);
	return 0;
}
