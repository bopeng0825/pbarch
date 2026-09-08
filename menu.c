#include <sys/stat.h>
#include "core.h"
#include "main.h"
#include "menu.h"
#include "ui_language.h"
#ifdef USE_SDL2
#include "menu_sdl2.h"
#include "menu_style.h"
#endif
#include "options.h"
#include "overrides.h"
#include "plat.h"
#include "scale.h"
#include "util.h"

#ifdef FUNKEY_S
#include "funkey/fk_instant_play.h"
#endif

#define PXMAKE(r,g,b) ((((r)<<8) & 0xf800)|(((g)<<3) & 0x07e0)|((b)>>3))
#define MENU_STYLE_SELECTION PXMAKE(0x72, 0xa6, 0xce)
#define MENU_STYLE_SELECTED_TEXT PXMAKE(0x08, 0x18, 0x20)
#define MENU_STYLE_TITLE PXMAKE(0xe4, 0xe7, 0xe9)

static int drew_alt_bg = 0;
static int full_menu_enabled;
#ifdef USE_SDL2
static int menu_sdl2_initialized;
#endif

static char cores_path[MAX_PATH];
static struct dirent **corelist = NULL;
static int corelist_len = 0;

static const char *new_fname = NULL;

#define MENU_ALIGN_LEFT 0
#define MENU_X2 0

#define MENU_ITEMS_PER_PAGE 11

typedef enum
{
	MA_NONE = 1,
	MA_MAIN_RESUME_GAME,
	MA_MAIN_SAVE_STATE,
	MA_MAIN_LOAD_STATE,
	MA_MAIN_DISC_CTRL,
	MA_MAIN_CHEATS,
	MA_MAIN_CORE_SEL,
	MA_MAIN_CONTENT_SEL,
	MA_MAIN_OPTIONS,
	MA_MAIN_RESET_GAME,
	MA_MAIN_CREDITS,
	MA_MAIN_EXIT,
	MA_OPT_CORE_OPTS,
	MA_OPT_SAVECFG,
	MA_OPT_SAVECFG_GAME,
	MA_OPT_RMCFG_GAME,
	MA_CTRL_PLAYER1,
	MA_CTRL_EMU,
} menu_id;

me_bind_action me_ctrl_actions[] =
{
	{ "UP       ",  1 << RETRO_DEVICE_ID_JOYPAD_UP},
	{ "DOWN     ",  1 << RETRO_DEVICE_ID_JOYPAD_DOWN },
	{ "LEFT     ",  1 << RETRO_DEVICE_ID_JOYPAD_LEFT },
	{ "RIGHT    ",  1 << RETRO_DEVICE_ID_JOYPAD_RIGHT },
	{ "A BUTTON ",  1 << RETRO_DEVICE_ID_JOYPAD_A },
	{ "B BUTTON ",  1 << RETRO_DEVICE_ID_JOYPAD_B },
	{ "X BUTTON ",  1 << RETRO_DEVICE_ID_JOYPAD_X },
	{ "Y BUTTON ",  1 << RETRO_DEVICE_ID_JOYPAD_Y },
	{ "START    ",  1 << RETRO_DEVICE_ID_JOYPAD_START },
	{ "SELECT   ",  1 << RETRO_DEVICE_ID_JOYPAD_SELECT },
	{ "L BUTTON ",  1 << RETRO_DEVICE_ID_JOYPAD_L },
	{ "R BUTTON ",  1 << RETRO_DEVICE_ID_JOYPAD_R },
	{ "L2 BUTTON ", 1 << RETRO_DEVICE_ID_JOYPAD_L2 },
	{ "R2 BUTTON ", 1 << RETRO_DEVICE_ID_JOYPAD_R2 },
#ifndef FUNKEY_S
	{ "L3 BUTTON ", 1 << RETRO_DEVICE_ID_JOYPAD_L3 },
	{ "R3 BUTTON ", 1 << RETRO_DEVICE_ID_JOYPAD_R3 },
#endif
	{ NULL,       0 }
};

/* Must be a superset of all possible actions. This is used when
 * saving config, and if an entry isn't here, the saver won't see
 * it. */
me_bind_action emuctrl_actions[] =
{
	{ "Save State   ", 1 << EACTION_SAVE_STATE },
	{ "Load State   ", 1 << EACTION_LOAD_STATE },
	{ "Toggle HUD   ", 1 << EACTION_TOGGLE_HUD },
	{ "Fast Forward ", 1 << EACTION_TOGGLE_FF },
	{ "Screenshot   ", 1 << EACTION_SCREENSHOT },
#ifdef FUNKEY_S
	{ "Pan Left     ", 1 << EACTION_PAN_DISPLAY_LEFT },
	{ "Pan Right    ", 1 << EACTION_PAN_DISPLAY_RIGHT },
#endif
	{ NULL,            0 }
};

static int emu_check_save_file(int slot, int *time)
{
	return state_exists(slot);
}

static int emu_save_load_game(int load, int unused)
{
	int ret;

	if (load)
		ret = state_read();
	else
		ret = state_write();

	return ret;
}

// RGB565
static unsigned short fname2color(const char *fname)
{
	return 0xFFFF;
}

const char *menu_translate(unsigned short text_id)
{
	return ui_text((enum ui_text_id)text_id);
}

#include "libpicofe/menu.c"

static void draw_menu_message(const char *msg, void (*draw_more)(void))  __attribute__((unused));
#ifdef USE_SDL2
static int menu_loop_savestate_styled(int is_loading);
#endif

static const char *mgn_saveloadcfg(int id, int *offs)
{
	return "";
}

static int mh_restore_defaults(int id, int keys)
{
	set_defaults();
	menu_update_msg(ui_text(UI_TEXT_DEFAULTS_RESTORED));
	return 1;
}

static int mh_savecfg(int id, int keys)
{
	if (save_config(id == MA_OPT_SAVECFG_GAME ? CONFIG_TYPE_GAME : CONFIG_TYPE_CORE) == 0)
		menu_update_msg(ui_text(UI_TEXT_CONFIG_SAVED));
	else
		menu_update_msg(ui_text(UI_TEXT_CONFIG_WRITE_FAILED));

	return 1;
}

static int mh_rmcfg(int id, int keys)
{
	if (remove_config(id == MA_OPT_RMCFG_GAME ? 1 : 0) == 0)
		menu_update_msg("config removed");
	else
		menu_update_msg("failed to remove config");

	return 1;
}

#ifdef FUNKEY_S
static int mh_zoom_level(int id, int keys)
{
	if (keys & PBTN_LEFT)  zoom_level -= 10;
	if (keys & PBTN_RIGHT) zoom_level += 10;

	if (zoom_level < 0) zoom_level = 0;
	if (zoom_level > 100) zoom_level = 100;

	return 0;
}

static const char *mgn_zoom_level(int id, int *offs)
{
	static char buf[16];
	snprintf(buf, sizeof(buf), "%d%%", zoom_level);
	return buf;
}
#endif

static void draw_src_bg(void) {
	memset(g_menubg_ptr, 0,
	       g_menuscreen_w * g_menuscreen_h * sizeof(uint16_t));
}

static int mh_set_core(int id, int keys) {
	if (corelist && id < corelist_len)
		snprintf(core_path, sizeof(core_path), "%s/%s", cores_path, corelist[id]->d_name);

	return 1;
}

static int core_selector(const struct dirent *ent) {
	return has_suffix_i(ent->d_name, "_libretro.so");
}

static int menu_loop_core_page(int offset, int keys) {
	/* persistent per-page selection for core list */
	static int *core_sel_per_page = NULL;
	static int core_sel_pages_alloc = 0;

	/* total entries known from corelist_len */
	int total_entries = corelist_len > 0 ? corelist_len : 0;
	int needed_pages = (total_entries + MENU_ITEMS_PER_PAGE - 1) / MENU_ITEMS_PER_PAGE;
	if (needed_pages < 1) needed_pages = 1;

	if (core_sel_pages_alloc < needed_pages) {
		int old = core_sel_pages_alloc;
		int *tmp = realloc(core_sel_per_page, sizeof(int) * needed_pages);
		if (tmp) {
			core_sel_per_page = tmp;
			/* initialize new slots to -1 (unvisited) */
			for (int k = old; k < needed_pages; k++)
				core_sel_per_page[k] = -1;
			core_sel_pages_alloc = needed_pages;
		} else {
			/* fallback: ensure at least one page */
			if (!core_sel_per_page) {
				core_sel_per_page = malloc(sizeof(int));
				if (core_sel_per_page) {
					core_sel_per_page[0] = -1;
					core_sel_pages_alloc = 1;
				}
			}
		}
	}

	int page = offset / MENU_ITEMS_PER_PAGE;
	if (page < 0) page = 0;
	if (page >= core_sel_pages_alloc) page = core_sel_pages_alloc - 1;

	int sel;
	if (core_sel_per_page && core_sel_per_page[page] != -1)
		sel = core_sel_per_page[page];
	else
		sel = 0;

	menu_entry e_menu_cores[MENU_ITEMS_PER_PAGE + 2] = {0}; /* +2 for Next, NULL */
	size_t menu_idx = 0;
	int i;
	char names[MENU_ITEMS_PER_PAGE][MAX_PATH];

	for (i = offset, menu_idx = 0; i < corelist_len && menu_idx < MENU_ITEMS_PER_PAGE; i++) {
		menu_entry *option;
		struct dirent *ent = corelist[i];
		option = &e_menu_cores[menu_idx];
		core_extract_name(ent->d_name, names[menu_idx], sizeof(names[menu_idx]));

		option->name = names[menu_idx];
		option->beh = MB_OPT_CUSTOM;
		option->id = i;
		option->enabled = 1;
		option->selectable = 1;
		option->handler = mh_set_core;
		menu_idx++;
	}

	if (i < corelist_len) {
		menu_entry *option;
		option = &e_menu_cores[menu_idx];
		option->name = "Next page";
		option->beh = MB_OPT_CUSTOM;
		option->id = i;
		option->enabled = 1;
		option->selectable = 1;
		option->handler = menu_loop_core_page;
	}
	int ret = me_loop(e_menu_cores, &sel);
	/* save last sel for this page */
	if (core_sel_per_page && page >= 0 && page < core_sel_pages_alloc)
		core_sel_per_page[page] = sel;
	return ret;
}

int menu_select_core(void) {
	int ret = -1;
	getcwd(cores_path, MAX_PATH);

	corelist_len = scandir(cores_path, &corelist, core_selector, alphasort);
	if (!corelist_len) return -1;

	plat_video_menu_enter(1);

	if (menu_loop_core_page(0, 0) < 0)
		goto finish;

	if (core_path[0] == '\0')
		goto finish;

	ret = 0;
finish:
	/* wait until menu, ok, back is released */
	while (in_menu_wait_any(NULL, 50) & (PBTN_MENU|PBTN_MOK|PBTN_MBACK))
		;

	plat_video_menu_leave();

	if (corelist_len > 0) {
		while (corelist_len--)
			free(corelist[corelist_len]);
		free(corelist);
		corelist = NULL;
	}
	return ret;
}

int hidden_file_filter(struct dirent **namelist, int count, const char *basedir) {
	int newcount = 0;

	for (int i = 0; i < count; i++) {
		if (namelist[i]->d_name[0] == '.' && namelist[i]->d_name[1] != '.') {
			free(namelist[i]);
			namelist[i] = NULL;
		}
	}

	for (int i = 0; i < count; i++) {
		if (namelist[i] != NULL)
			namelist[newcount++] = namelist[i];
	}

	return newcount;
}

const char *select_content(void) {
	const char *fname = NULL;
	char content_path[MAX_PATH];
	const char **extensions = core_extensions();
	const char **exts_with_zip = NULL;
	int i = 0, size = 0;

	if (content && strlen(content->path)) {
		strncpy(content_path, content->path, sizeof(content_path) - 1);
	} else {
		core_load_last_opened(content_path, sizeof(content_path));
	}

	if (!content_path[0]) {
		if (getenv("CONTENT_DIR")) {
			strncpy(content_path, getenv("CONTENT_DIR"), sizeof(content_path) - 1);
#ifdef CONTENT_DIR
		} else {
			strncpy(content_path, CONTENT_DIR, sizeof(content_path) - 1);
#else
		} else if (getenv("HOME")) {
			strncpy(content_path, getenv("HOME"), sizeof(content_path) - 1);
#endif
		}
	}

	content_path[sizeof(content_path) - 1] = '\0';

	if (extensions) {
		for (size = 0; extensions[size]; size++)
			;
	}

	exts_with_zip = calloc(size + 2, sizeof (char *)); /* add 2 for "zip", NULL */

	if (exts_with_zip) {
		for (i = 0; extensions[i]; i++) {
			exts_with_zip[i] = extensions[i];
		}
		exts_with_zip[i] = "zip";
	} else {
		exts_with_zip = extensions;
	}

	fname = menu_loop_romsel(content_path, sizeof(content_path), exts_with_zip, hidden_file_filter);

	if (exts_with_zip != extensions)
		free(exts_with_zip);

	return fname;
}

int menu_select_content(char *filename, size_t len) {
	const char *fname = NULL;
	int ret = -1;

	plat_video_menu_enter(1);
	fname = select_content();
	if (!fname)
		goto finish;

	strncpy(filename, fname, len - 1);
	if (g_autostateld_opt)
		resume_slot = 0;
	ret = 0;

finish:
        /* wait until menu, ok, back is released */
	while (in_menu_wait_any(NULL, 50) & (PBTN_MENU|PBTN_MOK|PBTN_MBACK))
		;

	plat_video_menu_leave();
	return ret;
}

static int menu_loop_select_content(int id, int keys) {
	const char *fname = select_content();

	if (fname == NULL)
		return -1;

	new_fname = fname;

	return 1;
}

static void load_new_content(const char *fname) {
	const struct core_override *override = get_overrides();

	if (!override || override->needs_reopen) {
#ifdef FUNKEY_S
		FK_LoadNewGame(fname);
		/* Does not return */
#else
		core_close();
		core_open(core_path);
#endif
	} else {
#ifdef FUNKEY_S
		FK_Autosave();
#endif
		core_unload();
	}
	core_load();

	content = content_init(fname);
	if (!content) {
		PA_ERROR("Couldn't allocate memory for content\n");
		quit(-1);
	}

	set_defaults();

	if (core_load_content(content)) {
		quit(-1);
	}

	load_config();
	load_config_keys(NULL);

	if (g_autostateld_opt) {
		resume_slot = 0;
		state_resume();
	}

#ifdef FUNKEY_S
	FK_Resume();
#endif
}

static void draw_frame_credits(void)
{
	smalltext_out16(4, 1, "Build date: " __DATE__, PXMAKE(0xe0, 0xff, 0xe0));
}

static const char credits[] =
	"   PicoArch rev. " REVISION "\n\n\n\n"
	"      --- Credits ---\n\n\n"
	" neonloop: original author\n\n"
	" Hairo   : .sav/.srm option\n\n"
#ifdef FUNKEY_S
	" DrUm78  : screen rotation,\n"
	"           cropped mode,\n"
	"           manual mode,\n"
	"           screen panning,\n"
	"           bug fixes\n\n"
	" xikteny : panning ideas";
#else
	" DrUm78  : bug fixes";
#endif

static int menu_loop_disc(int id, int keys)
{
	static int sel = 0;
	menu_entry e_menu_disc_options[2] = {0};
	unsigned disc = disc_get_index() + 1;
	menu_entry *option = &e_menu_disc_options[0];

	option->name = "Disc";
	option->beh = MB_OPT_RANGE;
	option->var = &disc;
	option->min = 1;
	option->max = disc_get_count();
	option->enabled = 1;
	option->need_to_save = 1;
	option->selectable = 1;

	me_loop(e_menu_disc_options, &sel);

	if (disc_get_index() + 1 != disc)
		disc_switch_index(disc - 1);

	return 0;
}

#ifdef USE_SDL2
static void draw_cheat_name_styled(const char *name, int selected,
				   int x, int y, int row_y,
				   int clip_right, unsigned int elapsed_ms)
{
	struct menu_rect clip;
	int color = selected ? MENU_STYLE_SELECTED_TEXT : menu_text_color;
	int viewport_width = clip_right - x;
	int text_width;
	int offset;
	int second_x;

	if (name == NULL || viewport_width <= 0)
		return;
	clip.x = x;
	clip.y = row_y;
	clip.w = viewport_width;
	clip.h = me_mfont_h;
	text_width = menu_sdl2_text_width(MENU_FONT_MAIN, name);
	if (!selected || text_width <= viewport_width) {
		if (selected)
			menu_sdl2_draw_text_clipped_unshadowed(
				g_menuscreen_ptr, g_menuscreen_pp, MENU_FONT_MAIN,
				x, y, color, name, &clip);
		else
			menu_sdl2_draw_text_clipped(
				g_menuscreen_ptr, g_menuscreen_pp, MENU_FONT_MAIN,
				x, y, color, name, &clip);
		return;
	}

	offset = menu_marquee_offset(text_width, viewport_width,
				      me_mfont_w * 3, elapsed_ms);
	menu_sdl2_draw_text_clipped_unshadowed(
		g_menuscreen_ptr, g_menuscreen_pp, MENU_FONT_MAIN,
		x - offset, y, color, name, &clip);
	second_x = x - offset + text_width + me_mfont_w * 3;
	if (second_x < clip_right)
		menu_sdl2_draw_text_clipped_unshadowed(
			g_menuscreen_ptr, g_menuscreen_pp, MENU_FONT_MAIN,
			second_x, y, color, name, &clip);
}

static int draw_cheats_menu_styled(menu_entry *entries, int sel,
				    unsigned int elapsed_ms)
{
	struct menu_responsive_layout layout;
	struct menu_style_geometry geometry;
	struct menu_style_option_columns columns;
	struct menu_rect value_clip;
	const char *on_text = ui_text(UI_TEXT_ON);
	const char *off_text = ui_text(UI_TEXT_OFF);
	int value_width = menu_sdl2_text_width(MENU_FONT_MAIN, on_text);
	int off_width = menu_sdl2_text_width(MENU_FONT_MAIN, off_text);
	int text_height;
	int title_height;
	int selection_right;
	int count = me_count(entries);
	int first;
	int end;
	int i;

	if (off_width > value_width)
		value_width = off_width;
	menu_draw_begin(1, 1);
	if (!menu_get_responsive_layout(&layout) || count <= 0 ||
	    sel < 0 || sel >= count) {
		menu_draw_end();
		return 0;
	}
	text_height = menu_sdl2_font_height(MENU_FONT_MAIN);
	title_height = menu_sdl2_line_height(MENU_FONT_TITLE);
	if (!menu_style_main_geometry(&layout, me_mfont_h, text_height,
				      title_height, me_mfont_w, count, sel,
				      &geometry) ||
	    !menu_style_option_columns(&geometry, me_mfont_w, value_width,
				       &columns)) {
		menu_draw_end();
		return 0;
	}
	selection_right = geometry.selection.x + geometry.selection.w;
	menu_style_draw_selection(g_menuscreen_ptr, g_menuscreen_w,
				  g_menuscreen_h, g_menuscreen_pp,
				  &geometry.selection, MENU_STYLE_SELECTION);
	menu_sdl2_draw_text(g_menuscreen_ptr, g_menuscreen_pp,
			   MENU_FONT_TITLE, geometry.title_x, geometry.title_y,
			   MENU_STYLE_TITLE, ui_text(UI_TEXT_CHEATS));

	value_clip.x = columns.value_x;
	value_clip.w = selection_right - me_mfont_w - value_clip.x;
	value_clip.h = me_mfont_h;
	first = geometry.first_visible;
	end = first + geometry.visible_count;
	for (i = first; i < end; i++) {
		const menu_entry *entry = &entries[i];
		const char *name = menu_entry_name(entry);
		const char *value = NULL;
		int selected = i == sel;
		int row_y = geometry.list_y + (i - first) * me_mfont_h;
		int text_y = row_y + geometry.text_y - geometry.selection.y;
		int name_clip_right = selection_right - me_mfont_w;
		int color = selected ? MENU_STYLE_SELECTED_TEXT : menu_text_color;

		if (entry->beh == MB_OPT_ONOFF) {
			value = me_read_onoff(entry) ? on_text : off_text;
			name_clip_right = columns.name_clip_right;
		}
		draw_cheat_name_styled(name, selected, geometry.text_x, text_y,
					row_y, name_clip_right, elapsed_ms);
		if (value == NULL)
			continue;
		value_clip.y = row_y;
		if (selected)
			menu_sdl2_draw_text_clipped_unshadowed(
				g_menuscreen_ptr, g_menuscreen_pp, MENU_FONT_MAIN,
				columns.value_x, text_y, color, value, &value_clip);
		else
			menu_sdl2_draw_text_clipped(
				g_menuscreen_ptr, g_menuscreen_pp, MENU_FONT_MAIN,
				columns.value_x, text_y, color, value, &value_clip);
	}
	menu_draw_end();
	return 1;
}

struct cheats_menu_redraw {
	menu_entry *entries;
	int sel;
	unsigned int selected_since;
	int draw_ok;
};

static void draw_cheats_menu_idle(void *data)
{
	struct cheats_menu_redraw *redraw = data;

	redraw->draw_ok = draw_cheats_menu_styled(redraw->entries, redraw->sel,
		plat_get_ticks_ms() - redraw->selected_since);
}

static int menu_loop_cheats_styled(menu_entry *entries, int *menu_sel)
{
	unsigned int selected_since = plat_get_ticks_ms();
	int count = me_count(entries);
	int sel_max = count - 1;
	int sel = *menu_sel;
	int ret = 0;
	unsigned long inp;

	if (count <= 0)
		return 0;
	if (sel < 0 || sel > sel_max)
		sel = 0;
	while ((!entries[sel].enabled || !entries[sel].selectable) &&
	       sel < sel_max)
		sel++;
	while (in_menu_wait_any(NULL, 50) &
	       (PBTN_MOK|PBTN_MBACK|PBTN_MENU))
		;
	for (;;) {
		struct cheats_menu_redraw redraw = {
			.entries = entries,
			.sel = sel,
			.selected_since = selected_since,
			.draw_ok = 1,
		};
		int old_sel = sel;

		if (!draw_cheats_menu_styled(entries, sel,
				plat_get_ticks_ms() - selected_since))
			return -1;
		inp = in_menu_wait_with_callback(
			PBTN_UP|PBTN_DOWN|PBTN_LEFT|PBTN_RIGHT|
			PBTN_MOK|PBTN_MBACK|PBTN_MENU|PBTN_L|PBTN_R,
			NULL, 70, 33, draw_cheats_menu_idle, &redraw);
		if (!redraw.draw_ok)
			return -1;
		if (inp & (PBTN_MENU|PBTN_MBACK))
			break;
		if (inp & PBTN_UP) {
			do {
				sel--;
				if (sel < 0)
					sel = sel_max;
			}
			while (!entries[sel].enabled || !entries[sel].selectable);
		}
		if (inp & PBTN_DOWN) {
			do {
				sel++;
				if (sel > sel_max)
					sel = 0;
			}
			while (!entries[sel].enabled || !entries[sel].selectable);
		}
		if (sel != old_sel)
			selected_since = plat_get_ticks_ms();
		if ((inp & (PBTN_L|PBTN_R)) == (PBTN_L|PBTN_R))
			debug_menu_loop();
		if (inp & (PBTN_LEFT|PBTN_RIGHT|PBTN_L|PBTN_R)) {
			if (me_process(&entries[sel],
				       (inp & (PBTN_RIGHT|PBTN_R)) != 0,
				       (inp & (PBTN_L|PBTN_R)) != 0))
				continue;
		}
		if ((inp & (PBTN_MOK|PBTN_LEFT|PBTN_RIGHT|PBTN_L|PBTN_R)) &&
		    entries[sel].handler != NULL &&
		    (entries[sel].beh != MB_NONE || (inp & PBTN_MOK))) {
			ret = entries[sel].handler(entries[sel].id, inp);
			if (ret)
				break;
			count = me_count(entries);
			sel_max = count - 1;
		}
	}
	*menu_sel = sel;
	return ret;
}

#endif

static int menu_loop_cheats_page(int offset, int keys) {
	/* persistent per-page selection for cheats */
	static int *cheats_sel_per_page = NULL;
	static int cheats_sel_pages_alloc = 0;

	int total_entries = cheats ? cheats->count : 0;
	int needed_pages = (total_entries + MENU_ITEMS_PER_PAGE - 1) / MENU_ITEMS_PER_PAGE;
	if (needed_pages < 1) needed_pages = 1;

	if (cheats_sel_pages_alloc < needed_pages) {
		int old = cheats_sel_pages_alloc;
		int *tmp = realloc(cheats_sel_per_page, sizeof(int) * needed_pages);
		if (tmp) {
			cheats_sel_per_page = tmp;
			for (int k = old; k < needed_pages; k++)
				cheats_sel_per_page[k] = -1;
			cheats_sel_pages_alloc = needed_pages;
		} else {
			if (!cheats_sel_per_page) {
				cheats_sel_per_page = malloc(sizeof(int));
				if (cheats_sel_per_page) {
					cheats_sel_per_page[0] = -1;
					cheats_sel_pages_alloc = 1;
				}
			}
		}
	}

	int page = offset / MENU_ITEMS_PER_PAGE;
	if (page < 0) page = 0;
	if (page >= cheats_sel_pages_alloc) page = cheats_sel_pages_alloc - 1;

	int sel;
	if (cheats_sel_per_page && cheats_sel_per_page[page] != -1)
		sel = cheats_sel_per_page[page];
	else
		sel = 0;

	menu_entry *e_menu_cheats;
	size_t i, menu_idx;

	/* cheats + 2 for possible "Next page" +  NULL */
	e_menu_cheats = (menu_entry *)calloc(cheats->count + 2, sizeof(menu_entry));

	if (!e_menu_cheats) {
		PA_ERROR("Error allocating cheats\n");
		return 0;
	}

	for (i = offset, menu_idx = 0; i < cheats->count && menu_idx < MENU_ITEMS_PER_PAGE; i++) {
		struct cheat *cheat = &cheats->cheats[i];
		menu_entry *option;

		option = &e_menu_cheats[menu_idx];

		option->name = cheat->name;
		option->beh = MB_OPT_ONOFF;
		option->var = &cheat->enabled;
		option->enabled = 1;
		option->mask = 1;
		option->need_to_save = 1;
		option->selectable = 1;
		option->help = cheat->info;
		menu_idx++;
	}

	if (i < cheats->count) {
		menu_entry *option;
		option = &e_menu_cheats[menu_idx];
		option->name = "Next page";
		option->beh = MB_OPT_CUSTOM;
		option->id = i;
		option->enabled = 1;
		option->selectable = 1;
		option->handler = menu_loop_cheats_page;
	}
	int ret;
#ifdef USE_SDL2
	ret = menu_loop_cheats_styled(e_menu_cheats, &sel);
	if (ret < 0)
		ret = me_loop(e_menu_cheats, &sel);
#else
	ret = me_loop(e_menu_cheats, &sel);
#endif
	free(e_menu_cheats);
	if (cheats_sel_per_page && page >= 0 && page < cheats_sel_pages_alloc)
		cheats_sel_per_page[page] = sel;
	return ret;
}

static int menu_loop_cheats(int id, int keys)
{
	int ret = menu_loop_cheats_page(0, keys);
	core_apply_cheats(cheats);
	return ret;
}

static int menu_loop_core_options_page(int offset, int keys) {
	/* persistent per-page selection for core options */
	static int *coreopt_sel_per_page = NULL;
	static int coreopt_sel_pages_alloc = 0;

	int total_entries = core_options.visible_len;
	if (total_entries < 0) total_entries = 0;
	int needed_pages = (total_entries + MENU_ITEMS_PER_PAGE - 1) / MENU_ITEMS_PER_PAGE;
	if (needed_pages < 1) needed_pages = 1;

	if (coreopt_sel_pages_alloc < needed_pages) {
		int old = coreopt_sel_pages_alloc;
		int *tmp = realloc(coreopt_sel_per_page, sizeof(int) * needed_pages);
		if (tmp) {
			coreopt_sel_per_page = tmp;
			for (int k = old; k < needed_pages; k++)
				coreopt_sel_per_page[k] = -1;
			coreopt_sel_pages_alloc = needed_pages;
		} else {
			if (!coreopt_sel_per_page) {
				coreopt_sel_per_page = malloc(sizeof(int));
				if (coreopt_sel_per_page) {
					coreopt_sel_per_page[0] = -1;
					coreopt_sel_pages_alloc = 1;
				}
			}
		}
	}

	int page = offset / MENU_ITEMS_PER_PAGE;
	if (page < 0) page = 0;
	if (page >= coreopt_sel_pages_alloc) page = coreopt_sel_pages_alloc - 1;

	int sel;
	if (coreopt_sel_per_page && coreopt_sel_per_page[page] != -1)
		sel = coreopt_sel_per_page[page];
	else
		sel = 0;

	menu_entry *e_menu_core_options;
	size_t i, menu_idx;

	/* core_option + 2 for possible "Next page" +  NULL */
	e_menu_core_options = (menu_entry *)calloc(core_options.visible_len + 2, sizeof(menu_entry));

	if (!e_menu_core_options) {
		PA_ERROR("Error allocating core options\n");
		return 0;
	}

	for (i = offset, menu_idx = 0; i < core_options.len && menu_idx < MENU_ITEMS_PER_PAGE; i++) {
		struct core_option_entry *entry = &core_options.entries[i];
		menu_entry *option;
		const char *key = entry->key;

		if (entry->blocked || !entry->visible)
			continue;

		option = &e_menu_core_options[menu_idx];

		option->name = entry->desc;
		option->beh = MB_OPT_ENUM;
		option->var = options_get_value_ptr(key);
		option->enabled = 1;
		option->need_to_save = 1;
		option->selectable = 1;
		option->data = options_get_options(key);
		option->help = entry->info;
		menu_idx++;
	}

	for (; i < core_options.len; i++) {
		struct core_option_entry *entry = &core_options.entries[i];
		if (!entry->blocked && entry->visible)
			break;
	}

	if (i < core_options.len) {
		menu_entry *option;
		option = &e_menu_core_options[menu_idx];
		option->name = "Next page";
		option->beh = MB_OPT_CUSTOM;
		option->id = i;
		option->enabled = 1;
		option->selectable = 1;
		option->handler = menu_loop_core_options_page;
	}
	int ret = me_loop(e_menu_core_options, &sel);

	options_update_changed();

	free(e_menu_core_options);

	if (coreopt_sel_per_page && page >= 0 && page < coreopt_sel_pages_alloc)
		coreopt_sel_per_page[page] = sel;

	return ret;
}

static int menu_loop_core_options(int id, int keys)
{
	return menu_loop_core_options_page(0, keys);
}

static const char h_rm_config_game[]  = "Removes game-specific config file.";

static const char h_restore_def[]     = "Switches back to default settings.";

static const char h_show_fps[]        = "Shows frames and vsyncs per second.";
static const char h_show_cpu[]        = "Shows CPU usage (%).";

#if (SCREEN_WIDTH >= 320)
static const char h_enable_drc[]      = "Dynamically adjusts audio rate for smoother video.";

static const char h_audio_buffer_size[]        =
	"The size of the audio buffer, in frames. Higher\n"
	"values reduce the risk of audio crackling at the\n"
	"cost of delayed sound.";

static const char h_scale_size[]        =
	"How much to stretch the screen when scaling. NATIVE\n"
	"does no stretching. SCALED uses the correct aspect\n"
	"ratio. STRETCHED uses the whole screen.";

static const char h_scale_filter[]        =
	"When stretching, how missing pixels are filled.\n"
	"NEAREST copies the last pixel. SHARP keeps pixels\n"
	"aligned where possible. SMOOTH adds a blur effect.";

static const char h_use_srm[]        =
	"Use .srm files for SRAM saves, needed for\n"
	"compatibility with mainline RetroArch saves.\n"
	"Save file compression needs to be off in RetroArch.";

static const char *men_scale_size[] =
{
	"NATIVE",
	"SCALED",
	"STRETCHED",
	NULL
};
#else
static const char h_enable_drc[]      =
	"Dynamically adjusts audio rate for\n"
	"smoother video.";

static const char h_audio_buffer_size[]        =
	"The audio buffer size, in frames.\n"
	"Higher values reduce the risk of audio\n"
	"crackling at the cost of delayed sound.";

static const char h_scale_size[]        =
	"NATIVE does no stretching. SCALED keeps\n"
	"the correct aspect ratio. STRETCHED\n"
	"uses the whole screen. CROPPED allows\n"
	"resizing beyond the screen limit.\n"
	"MANUAL allows manual screen resizing.";

static const char h_zoom_level[]        =
	"Control the zoom level of the MANUAL\n"
	"mode. Hotkeys Fn+left/right can be used\n"
	"to change the zoom level (+/-10%) and\n"
	"switch automatically to MANUAL mode.";

static const char h_scale_filter[]        =
	"When stretching, how missing pixels\n"
	"are filled. NEAREST copies the last\n"
	"pixel. SHARP tries to keep pixels\n"
	"aligned. SMOOTH adds a blur effect.";

static const char h_use_srm[]        =
	"Use .srm files for SRAM saves,\n"
	"needed for compatibility with mainline\n"
	"RetroArch saves. Save file compression\n"
	"needs to be off in RetroArch.";

static const char h_rotate_display[] =
	"Screen orientation. Rotates the display\n"
	"by 90, 180 or 270 degrees CLOCKWISE.";

static const char h_pan_display[] =
	"Viewport position. Sets the focus on\n"
	"the LEFT or on the RIGHT part of the\n"
	"screen when the game width exceeds 240\n"
	"pixels.";

static const char *men_scale_size[] =
{
	"NATIVE",
	"SCALED",
	"STRETCHED",
	"CROPPED",
	"MANUAL",
	NULL
};

static const char *men_rotate_display[] =
{
	"OFF",
	"90CW",
	"180CW",
	"270CW",
	NULL
};

static const char *men_pan_display[] =
{
	"OFF",
	"LEFT",
	"RIGHT",
	NULL
};
#endif

static const char *men_scale_filter[] =
{
	"NEAREST",
	"SHARP",
	"SMOOTH",
	NULL
};

static menu_entry e_menu_video_options[] =
{
	mee_onoff_h_t    (UI_TEXT_SHOW_FPS,             0, show_fps, 1, h_show_fps),
	mee_onoff_h_t    (UI_TEXT_SHOW_CPU_USAGE,       0, show_cpu, 1, h_show_cpu),
	mee_enum_h_t     (UI_TEXT_DISPLAY_MODE,         0, scale_size, men_scale_size, h_scale_size),
#ifdef FUNKEY_S
	mee_cust_h_t     (UI_TEXT_ZOOM_LEVEL,            MB_OPT_CUSTOM, mh_zoom_level, mgn_zoom_level, h_zoom_level),
	mee_enum_h_t     (UI_TEXT_SCREEN_PANNING,        0, pan_display, men_pan_display, h_pan_display),
	mee_enum_h_t     (UI_TEXT_SCREEN_ROTATION,       0, rotate_display, men_rotate_display, h_rotate_display),
#endif
	mee_enum_h_t     (UI_TEXT_SCALING_FILTER,        0, scale_filter, men_scale_filter, h_scale_filter),
	mee_range_h_t    (UI_TEXT_AUDIO_BUFFER,          0, audio_buffer_size, 1, 15, h_audio_buffer_size),
	mee_onoff_h_t    (UI_TEXT_AUDIO_ADJUSTMENT,      0, enable_drc, 1, h_enable_drc),
	mee_end,
};

static int menu_loop_video_options(int id, int keys)
{
	static int sel = 0;

	me_loop(e_menu_video_options, &sel);
	plat_reinit();

	return 0;
}

static int key_config_loop_wrap(int id, int keys)
{
	const struct core_override *override = get_overrides();
	me_bind_action *actions = CORE_OVERRIDE(override, actions, me_ctrl_actions);
	size_t action_size = CORE_OVERRIDE(override, action_size, array_size(me_ctrl_actions));
	me_bind_action *emu_actions = CORE_OVERRIDE(override, emu_actions, emuctrl_actions);
	size_t emu_action_size = CORE_OVERRIDE(override, emu_action_size, array_size(emuctrl_actions));

	switch (id) {
	case MA_CTRL_PLAYER1:
		key_config_loop(actions, action_size - 1, 0);
		break;
	case MA_CTRL_EMU:
		key_config_loop(emu_actions, emu_action_size - 1, -1);
		break;
	default:
		break;
	}
	return 0;
}

const char *config_label(int id, int *offs) {
	return config_override ? "Loaded: game config" : "Loaded: global config";
}

static menu_entry e_menu_config_options[] =
{
	mee_onoff_h_t    (UI_TEXT_USE_SRM_SAVES,       0, use_srm, 1, h_use_srm),
	mee_label        (""),
	mee_cust_nosave_t(UI_TEXT_SAVE_GLOBAL_CONFIG, MA_OPT_SAVECFG,      mh_savecfg, mgn_saveloadcfg),
	mee_cust_nosave_t(UI_TEXT_SAVE_GAME_CONFIG,   MA_OPT_SAVECFG_GAME, mh_savecfg, mgn_saveloadcfg),
	mee_handler_id_h_t(UI_TEXT_DELETE_GAME_CONFIG, MA_OPT_RMCFG_GAME,  mh_rmcfg,   h_rm_config_game),
	mee_handler_h_t  (UI_TEXT_RESTORE_DEFAULTS,    mh_restore_defaults, h_restore_def),
	mee_label        (""),
	mee_label_mk     (0,                          config_label),
	mee_end,
};

static int menu_loop_config_options(int id, int keys)
{
	static int sel = 0;
	me_enable(e_menu_config_options, MA_OPT_RMCFG_GAME, config_override == 1);

	me_loop(e_menu_config_options, &sel);

	return 0;
}

static menu_entry e_menu_options[] =
{
	mee_handler_t   (UI_TEXT_AUDIO_VIDEO,       menu_loop_video_options),
	mee_handler_id_t(UI_TEXT_EMULATOR_OPTIONS,  MA_OPT_CORE_OPTS, menu_loop_core_options),
	mee_handler_id_t(UI_TEXT_PLAYER_CONTROLS,   MA_CTRL_PLAYER1,  key_config_loop_wrap),
	mee_handler_id_t(UI_TEXT_EMULATOR_HOTKEYS,  MA_CTRL_EMU,      key_config_loop_wrap),
	mee_handler_t   (UI_TEXT_SAVE_CONFIG,       menu_loop_config_options),
	mee_end,
};

static int menu_loop_options(int id, int keys)
{
	static int sel = 0;
	me_loop(e_menu_options, &sel);

	return 0;
}

static int main_menu_handler(int id, int keys)
{
	switch (id)
	{
	case MA_MAIN_RESUME_GAME:
		return 1;
	case MA_MAIN_SAVE_STATE:
#ifdef USE_SDL2
		return menu_loop_savestate_styled(0);
#else
		return menu_loop_savestate(0);
#endif
	case MA_MAIN_LOAD_STATE:
#ifdef USE_SDL2
		return menu_loop_savestate_styled(1);
#else
		return menu_loop_savestate(1);
#endif
	case MA_MAIN_RESET_GAME:
		current_core.retro_reset();
		return 1;
	case MA_MAIN_CREDITS:
		draw_menu_message(credits, draw_frame_credits);
		in_menu_wait(PBTN_MENU|PBTN_MBACK, NULL, 70);
		break;
	case MA_MAIN_EXIT:
		should_quit = 1;
		return 1;
	default:
		lprintf("%s: something unknown selected\n", __FUNCTION__);
		break;
	}

	return 0;
}

static menu_entry e_menu_main[] =
{
	mee_handler_id_t(UI_TEXT_RESUME_GAME,   MA_MAIN_RESUME_GAME, main_menu_handler),
	mee_handler_id_t(UI_TEXT_SAVE_STATE,    MA_MAIN_SAVE_STATE,  main_menu_handler),
	mee_handler_id_t(UI_TEXT_LOAD_STATE,    MA_MAIN_LOAD_STATE,  main_menu_handler),
	mee_handler_id_t(UI_TEXT_DISC_CONTROL,  MA_MAIN_DISC_CTRL,   menu_loop_disc),
	mee_handler_id_t(UI_TEXT_CHEATS,        MA_MAIN_CHEATS,      menu_loop_cheats),
	mee_handler_id_t(UI_TEXT_OPTIONS,        MA_MAIN_OPTIONS,     menu_loop_options),
	mee_handler_id_t(UI_TEXT_RESET_GAME,    MA_MAIN_RESET_GAME,  main_menu_handler),
	mee_handler_id_t(UI_TEXT_LOAD_NEW_GAME, MA_MAIN_CONTENT_SEL, menu_loop_select_content),
	mee_handler_id_t(UI_TEXT_ABOUT,         MA_MAIN_CREDITS,     main_menu_handler),
	mee_handler_id_t(UI_TEXT_EXIT,          MA_MAIN_EXIT,        main_menu_handler),
	mee_end,
};

#ifdef USE_SDL2
static int main_menu_sel;

static int draw_main_menu_styled(int selected_index)
{
	struct menu_responsive_layout layout;
	struct menu_style_geometry geometry;
	struct menu_rect clip;
	int enabled_count = 0;
	int selected_visible = -1;
	int visible_index = 0;
	int drawn_index = 0;
	int text_height;
	int title_height;
	int i;

	menu_draw_begin(1, 1);
	if (!menu_get_responsive_layout(&layout) || selected_index < 0) {
		menu_draw_end();
		return 0;
	}
	for (i = 0; menu_entry_present(&e_menu_main[i]); i++) {
		if (!e_menu_main[i].enabled)
			continue;
		if (i == selected_index)
			selected_visible = enabled_count;
		enabled_count++;
	}
	text_height = menu_sdl2_font_height(MENU_FONT_MAIN);
	title_height = menu_sdl2_line_height(MENU_FONT_TITLE);
	if (!menu_style_main_geometry(&layout, me_mfont_h, text_height,
				      title_height,
				      me_mfont_w,
				      enabled_count, selected_visible,
				      &geometry)) {
		menu_draw_end();
		return 0;
	}

	menu_style_draw_selection(g_menuscreen_ptr, g_menuscreen_w,
				  g_menuscreen_h, g_menuscreen_pp,
				  &geometry.selection, MENU_STYLE_SELECTION);
	menu_sdl2_draw_text(g_menuscreen_ptr, g_menuscreen_pp,
			   MENU_FONT_TITLE, geometry.title_x, geometry.title_y,
			   MENU_STYLE_TITLE,
			   ui_text(UI_TEXT_GAME_MENU));
	clip.x = geometry.text_x;
	clip.w = geometry.selection.x + geometry.selection.w - clip.x;
	clip.h = me_mfont_h;
	for (i = 0; menu_entry_present(&e_menu_main[i]); i++) {
		const char *name;
		int color;
		int row_y;
		int text_y;

		if (!e_menu_main[i].enabled)
			continue;
		if (visible_index++ < geometry.first_visible)
			continue;
		if (drawn_index >= geometry.visible_count)
			break;
		name = menu_entry_name(&e_menu_main[i]);
		row_y = geometry.list_y + drawn_index++ * me_mfont_h;
		text_y = row_y + geometry.text_y - geometry.selection.y;
		color = i == selected_index ? MENU_STYLE_SELECTED_TEXT :
			menu_text_color;
		clip.y = row_y;
		if (i == selected_index)
			menu_sdl2_draw_text_clipped_unshadowed(
				g_menuscreen_ptr, g_menuscreen_pp, MENU_FONT_MAIN,
				geometry.text_x, text_y, color, name, &clip);
		else
			menu_sdl2_draw_text_clipped(
				g_menuscreen_ptr, g_menuscreen_pp, MENU_FONT_MAIN,
				geometry.text_x, text_y, color, name, &clip);
	}
	if (menu_error_msg[0] != 0) {
		clip.x = layout.outer_margin;
		clip.y = g_menuscreen_h - me_mfont_h - layout.outer_margin;
		clip.w = g_menuscreen_w - 2 * layout.outer_margin;
		clip.h = me_mfont_h;
		menu_sdl2_draw_text_clipped(g_menuscreen_ptr, g_menuscreen_pp,
					    MENU_FONT_MAIN, clip.x, clip.y,
					    menu_text_color, menu_error_msg,
					    &clip);
		if (plat_get_ticks_ms() - menu_error_time > 2048)
			menu_error_msg[0] = 0;
	}
	menu_draw_end();
	return 1;
}

static void menu_loop_main_styled(void)
{
	unsigned long inp;
	int count = me_count(e_menu_main);
	int sel_max = count - 1;
	int sel = main_menu_sel;

	if (count <= 0)
		return;
	if (sel < 0 || sel > sel_max)
		sel = 0;
	while ((!e_menu_main[sel].enabled ||
	        !e_menu_main[sel].selectable) && sel < sel_max)
		sel++;

	while (in_menu_wait_any(NULL, 50) &
	       (PBTN_MOK|PBTN_MBACK|PBTN_MENU))
		;
	for (;;) {
		if (!draw_main_menu_styled(sel)) {
			me_loop(e_menu_main, &main_menu_sel);
			return;
		}
		inp = in_menu_wait(PBTN_UP|PBTN_DOWN|PBTN_LEFT|PBTN_RIGHT|
			PBTN_MOK|PBTN_MBACK|PBTN_MENU|PBTN_L|PBTN_R,
			NULL, 70);
		if (inp & (PBTN_MENU|PBTN_MBACK))
			break;
		if (inp & PBTN_UP) {
			do {
				sel--;
				if (sel < 0)
					sel = sel_max;
			}
			while (!e_menu_main[sel].enabled ||
			       !e_menu_main[sel].selectable);
		}
		if (inp & PBTN_DOWN) {
			do {
				sel++;
				if (sel > sel_max)
					sel = 0;
			}
			while (!e_menu_main[sel].enabled ||
			       !e_menu_main[sel].selectable);
		}
		if ((inp & (PBTN_L|PBTN_R)) == (PBTN_L|PBTN_R))
			debug_menu_loop();
		if ((inp & PBTN_MOK) && e_menu_main[sel].handler != NULL &&
		    e_menu_main[sel].handler(e_menu_main[sel].id, inp))
			break;
	}
	main_menu_sel = sel;
}

static int draw_savestate_menu_styled(int menu_sel, int is_loading)
{
	struct menu_responsive_layout layout;
	struct menu_style_geometry geometry;
	struct menu_rect clip;
	char row[96];
	const char *title;
	const char *empty_text = menu_translate(UI_TEXT_SLOT_EMPTY);
	const char *saved_text = menu_translate(UI_TEXT_SLOT_SAVED);
	int text_height;
	int title_height;
	int first;
	int end;
	int i;

	if (menu_sel < STATE_SLOT_COUNT &&
	    (state_slot_flags & (1 << menu_sel)))
		draw_savestate_bg(menu_sel);

	menu_draw_begin(1, 1);
	if (!menu_get_responsive_layout(&layout)) {
		menu_draw_end();
		return 0;
	}
	text_height = menu_sdl2_font_height(MENU_FONT_MAIN);
	title_height = menu_sdl2_line_height(MENU_FONT_TITLE);
	if (!menu_style_main_geometry(&layout, me_mfont_h, text_height,
				      title_height,
				      me_mfont_w,
				      STATE_SLOT_COUNT + 1, menu_sel,
				      &geometry)) {
		menu_draw_end();
		return 0;
	}
	menu_style_draw_selection(g_menuscreen_ptr, g_menuscreen_w,
				  g_menuscreen_h, g_menuscreen_pp,
				  &geometry.selection, MENU_STYLE_SELECTION);

	title = menu_translate(is_loading ? UI_TEXT_LOAD_STATE_TITLE :
			       UI_TEXT_SAVE_STATE_TITLE);
	menu_sdl2_draw_text(g_menuscreen_ptr, g_menuscreen_pp,
			   MENU_FONT_TITLE, geometry.title_x, geometry.title_y,
			   MENU_STYLE_TITLE, title);
	clip.x = geometry.text_x;
	clip.w = geometry.selection.x + geometry.selection.w - geometry.text_x;
	clip.h = me_mfont_h;
	first = geometry.first_visible;
	end = first + geometry.visible_count;
	for (i = first; i < end; i++) {
		int row_y = geometry.list_y + (i - first) * me_mfont_h;
		int text_y = row_y + geometry.text_y - geometry.selection.y;
		int color = i == menu_sel ? MENU_STYLE_SELECTED_TEXT :
			menu_text_color;

		if (i < STATE_SLOT_COUNT) {
			const char *status = state_slot_flags & (1 << i) ?
				saved_text : empty_text;
			snprintf(row, sizeof(row), menu_translate(UI_TEXT_SLOT_FMT),
				 i, status);
		}
		else
			snprintf(row, sizeof(row), "%s", menu_translate(UI_TEXT_BACK));

		clip.y = row_y;
		if (i == menu_sel)
			menu_sdl2_draw_text_clipped_unshadowed(
				g_menuscreen_ptr, g_menuscreen_pp, MENU_FONT_MAIN,
				geometry.text_x, text_y, color, row, &clip);
		else
			menu_sdl2_draw_text_clipped(
				g_menuscreen_ptr, g_menuscreen_pp, MENU_FONT_MAIN,
				geometry.text_x, text_y, color, row, &clip);
	}
	menu_draw_end();
	return 1;
}

static int menu_loop_savestate_styled(int is_loading)
{
	static int menu_sel = STATE_SLOT_COUNT;
	int menu_sel_max = STATE_SLOT_COUNT;
	unsigned long inp;
	int ret = 0;

	state_check_slots();
	if (!(state_slot_flags & (1 << menu_sel)) && is_loading)
		menu_sel = menu_sel_max;

	for (;;) {
		if (!draw_savestate_menu_styled(menu_sel, is_loading))
			return menu_loop_savestate(is_loading);
		inp = in_menu_wait(PBTN_UP|PBTN_DOWN|PBTN_MOK|PBTN_MBACK,
				   NULL, 100);
		if (inp & PBTN_UP)
			menu_sel = menu_style_next_savestate_slot(menu_sel, -1,
				is_loading, (unsigned int)state_slot_flags,
				STATE_SLOT_COUNT);
		if (inp & PBTN_DOWN)
			menu_sel = menu_style_next_savestate_slot(menu_sel, 1,
				is_loading, (unsigned int)state_slot_flags,
				STATE_SLOT_COUNT);
		if (inp & PBTN_MOK) {
			if (menu_sel < STATE_SLOT_COUNT) {
				state_slot = menu_sel;
				if (emu_save_load_game(is_loading, 0)) {
					menu_update_msg(menu_translate(is_loading ?
						UI_TEXT_LOAD_FAILED : UI_TEXT_SAVE_FAILED));
					break;
				}
				ret = 1;
			}
			break;
		}
		if (inp & PBTN_MBACK)
			break;
	}
	return ret;
}
#endif

void menu_set_full_menu(int enabled)
{
	full_menu_enabled = enabled != 0;
}

static void draw_savestate_bg(int slot)
{
	char filename[MAX_PATH];
	int w, h, bpp;
	size_t bufsize = SCREEN_PITCH * SCREEN_HEIGHT;
	void *buf = calloc(bufsize, sizeof(char));

	if (!buf) {
		PA_WARN("Couldn't allocate savestate background");
		goto finish;
	}
	state_file_name(filename, MAX_PATH, slot);

	if (plat_load_screen(filename, buf, bufsize, &w, &h, &bpp))
		goto finish;

	if (bpp == sizeof(uint16_t)) {
		menu_darken_bg(g_menubg_ptr, buf, w * h, 0);
		drew_alt_bg = 1;
	}

finish:
	if (buf)
		free(buf);
}

void menu_begin(void)
{
#ifdef USE_SDL2
	if (menu_sdl2_initialized) {
		struct menu_responsive_layout layout;

		menu_calculate_responsive_layout(g_menuscreen_w, g_menuscreen_h,
					 &layout);
		if (!drew_alt_bg) {
			menu_sdl2_copy_background(g_menubg_ptr, g_menuscreen_w);
			menu_sdl2_draw_preview(g_menubg_ptr, g_menuscreen_w,
					       g_menubg_src_ptr, g_menubg_src_w,
					       g_menubg_src_h, g_menubg_src_pp,
					       &layout.preview);
			drew_alt_bg = 1;
		}
		menu_set_responsive_layout(&layout);
	}
#endif
	if (!drew_alt_bg)
		draw_src_bg();
}

void menu_end(void)
{
#ifdef USE_SDL2
	menu_set_responsive_layout(NULL);
#endif
	drew_alt_bg = 0;
}

void menu_loop(void)
{
#ifndef USE_SDL2
	static int sel = 0;
#endif
	bool needs_disc_ctrl = disc_get_count() > 1;

	plat_video_menu_enter(1);

	me_enable(e_menu_options, MA_OPT_CORE_OPTS, core_options.visible_len > 0);

	me_enable(e_menu_main, MA_MAIN_SAVE_STATE, state_allowed());
	me_enable(e_menu_main, MA_MAIN_LOAD_STATE, state_allowed());
	me_enable(e_menu_main, MA_MAIN_CHEATS, cheats != NULL);

	me_enable(e_menu_main, MA_MAIN_DISC_CTRL, needs_disc_ctrl);
	me_enable(e_menu_main, MA_MAIN_OPTIONS, full_menu_enabled);
	me_enable(e_menu_main, MA_MAIN_CONTENT_SEL, full_menu_enabled);
	me_enable(e_menu_main, MA_MAIN_CREDITS, full_menu_enabled);

#ifdef MMENU
	if (state_allowed()) {
		me_enable(e_menu_main, MA_MAIN_SAVE_STATE, mmenu == NULL);
		me_enable(e_menu_main, MA_MAIN_LOAD_STATE, mmenu == NULL);
	}
#endif
#ifdef USE_SDL2
	menu_loop_main_styled();
#else
	me_loop(e_menu_main, &sel);
#endif

	/* wait until menu, ok, back is released */
	while (in_menu_wait_any(NULL, 50) & (PBTN_MENU|PBTN_MOK|PBTN_MBACK))
		;

	if (new_fname) {
		load_new_content(new_fname);
		new_fname = NULL;
	}

	/* Force the hud to clear */
	plat_video_set_msg(NULL, 0, 0);
	plat_video_menu_leave();
}

int menu_init(void)
{
#ifndef USE_SDL2
	ui_language_set(UI_LANG_EN);
#endif
#ifdef USE_SDL2
	char font_path[MAX_PATH];
	char background_path[MAX_PATH];
	int pos = plat_get_skin_dir(font_path, sizeof(font_path));

	if (pos >= 0 &&
	    pos + (int)sizeof("picoarch-ui.ttf") <= (int)sizeof(font_path) &&
	    pos + (int)sizeof("background.png") <= (int)sizeof(background_path)) {
		memcpy(background_path, font_path, (size_t)pos);
		memcpy(font_path + pos, "picoarch-ui.ttf",
		       sizeof("picoarch-ui.ttf"));
		memcpy(background_path + pos, "background.png",
		       sizeof("background.png"));
		if (menu_sdl2_init(font_path, background_path,
				   g_menuscreen_w, g_menuscreen_h) == 0)
			menu_sdl2_initialized = 1;
	}
	if (!menu_sdl2_available() && ui_language_current() != UI_LANG_EN)
		ui_language_set(UI_LANG_EN);
#endif
	menu_init_base();

	g_menubg_src_ptr = calloc(g_menubg_src_pp * g_menubg_src_h, sizeof(uint16_t));
	g_menubg_ptr = calloc(g_menuscreen_h * g_menuscreen_pp, sizeof(uint16_t));
	if (g_menubg_src_ptr == NULL || g_menubg_ptr == NULL) {
		fprintf(stderr, "OOM\n");
		return -1;
	}
	return 0;
}

void menu_finish(void)
{
#ifdef USE_SDL2
	menu_sdl2_finish();
	menu_sdl2_initialized = 0;
#endif
	if (g_menubg_src_ptr) {
		free(g_menubg_src_ptr);
		g_menubg_src_ptr = NULL;
	}

	if (g_menubg_ptr) {
		free(g_menubg_ptr);
		g_menubg_ptr = NULL;
	}
}

static void debug_menu_loop(void)
{
}

void menu_update_msg(const char *msg)
{
	int max_pixels = g_menuscreen_w - 10;
#ifdef USE_SDL2
	struct menu_responsive_layout layout;

	menu_calculate_responsive_layout(g_menuscreen_w, g_menuscreen_h,
					 &layout);
	max_pixels = g_menuscreen_w - layout.outer_margin * 2;
#endif

	if (max_pixels < 0)
		max_pixels = 0;
	menu_text_fit(msg, max_pixels, menu_error_msg,
		      sizeof(menu_error_msg), 0);

	menu_error_time = plat_get_ticks_ms();
	PA_INFO("%s\n", menu_error_msg);
}
