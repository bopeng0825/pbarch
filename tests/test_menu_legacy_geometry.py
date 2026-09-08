import re
import unittest
from pathlib import Path


MENU_SOURCE = (
	Path(__file__).resolve().parents[1] / "libpicofe" / "menu.c"
).read_text(encoding="utf-8")
FRONTEND_MENU_SOURCE = (
	Path(__file__).resolve().parents[1] / "menu.c"
).read_text(encoding="utf-8")
SDL2_MENU_SOURCE = (
	Path(__file__).resolve().parents[1] / "menu_sdl2.c"
).read_text(encoding="utf-8")
MENU_LAYOUT_SOURCE = (
	Path(__file__).resolve().parents[1] / "menu_layout.c"
).read_text(encoding="utf-8")
MENU_STYLE_SOURCE = (
	Path(__file__).resolve().parents[1] / "menu_style.c"
).read_text(encoding="utf-8")
CHEAT_SOURCE = (
	Path(__file__).resolve().parents[1] / "cheat.c"
).read_text(encoding="utf-8")
INPUT_SOURCE = (
	Path(__file__).resolve().parents[1] / "libpicofe" / "input.c"
).read_text(encoding="utf-8")


class LegacyMenuGeometryTest(unittest.TestCase):
	def assert_non_sdl_branch(self, legacy_expression, sdl_expression):
		pattern = (
			r"#ifdef USE_SDL2(?:(?!#endif).)*"
			+ re.escape(sdl_expression)
			+ r"(?:(?!#endif).)*#else(?:(?!#endif).)*"
			+ re.escape(legacy_expression)
			+ r"(?:(?!#endif).)*#endif"
		)
		self.assertRegex(MENU_SOURCE, re.compile(pattern, re.DOTALL))

	def test_main_menu_uses_fixed_cells_outside_sdl2(self):
		self.assert_non_sdl_branch(
			"wt = strlen(name) * me_mfont_w;",
			"wt = menu_text_width(name, 0);",
		)

	def test_list_centering_stays_in_legacy_branch(self):
		self.assert_non_sdl_branch(
			"x = g_menuscreen_w / 2 - w / 2;",
			"x = layout.menu.x + me_mfont_w;",
		)
		self.assert_non_sdl_branch(
			"y = g_menuscreen_h / 2 - h / 2;",
			"y = menu_centered_block_y(layout.menu.y, layout.menu.h, h);",
		)
		self.assert_non_sdl_branch(
			"wt += 10 * me_mfont_w;",
			"wt += value_width;",
		)

	def test_sdl2_layout_is_available_before_entry_geometry(self):
		body = re.search(
			r"static void me_draw\(.*?\)\s*\{(?P<body>.*?)"
			r"\n\}\n\n#ifdef MENU_TEST",
			MENU_SOURCE,
			re.DOTALL,
		)
		self.assertIsNotNone(body)
		draw_body = body.group("body")
		self.assertLess(
			draw_body.index("menu_draw_begin(1, 0);"),
			draw_body.index("menu_get_responsive_layout(&layout);"),
		)

	def test_sdl2_main_menu_draws_translated_header(self):
		self.assertRegex(
			FRONTEND_MENU_SOURCE,
			re.compile(
				r"static int draw_main_menu_styled\(int selected_index\).*?"
				r"ui_text\(UI_TEXT_GAME_MENU\)",
				re.DOTALL,
			),
		)
		self.assertIn("menu_style_draw_selection(", FRONTEND_MENU_SOURCE)

	def test_sdl2_main_menu_does_not_use_the_legacy_draw_loop(self):
		loop = re.search(
			r"void menu_loop\(void\)\s*\{(?P<body>.*?)"
			r"\n\}\n\nint menu_init",
			FRONTEND_MENU_SOURCE,
			re.DOTALL,
		)
		self.assertIsNotNone(loop)
		sdl_branch = re.search(
			r"#ifdef USE_SDL2(?P<body>.*?)#else",
			loop.group("body"),
			re.DOTALL,
		)
		self.assertIsNotNone(sdl_branch)
		self.assertIn("menu_loop_main_styled();", sdl_branch.group("body"))
		self.assertNotIn("me_loop_d(", sdl_branch.group("body"))

	def test_styled_main_menu_initializes_the_frame_before_layout_lookup(self):
		draw = re.search(
			r"static int draw_main_menu_styled\(int selected_index\)\s*"
			r"\{(?P<body>.*?)\n\}",
			FRONTEND_MENU_SOURCE,
			re.DOTALL,
		)
		self.assertIsNotNone(draw)
		body = draw.group("body")
		self.assertLess(
			body.index("menu_draw_begin(1, 1);"),
			body.index("menu_get_responsive_layout(&layout)"),
		)

	def test_styled_savestate_initializes_the_frame_before_layout_lookup(self):
		draw = re.search(
			r"static int draw_savestate_menu_styled\(.*?\)\s*"
			r"\{(?P<body>.*?)\n\}",
			FRONTEND_MENU_SOURCE,
			re.DOTALL,
		)
		self.assertIsNotNone(draw)
		body = draw.group("body")
		self.assertLess(
			body.index("menu_draw_begin(1, 1);"),
			body.index("menu_get_responsive_layout(&layout)"),
		)

	def test_savestate_menu_exposes_eight_slots(self):
		self.assertIn("#define STATE_SLOT_COUNT 8", MENU_SOURCE)

	def test_savestate_usage_is_translated_without_timestamps(self):
		self.assertIn("UI_TEXT_SLOT_EMPTY", MENU_SOURCE)
		self.assertIn("UI_TEXT_SLOT_SAVED", MENU_SOURCE)
		self.assertIn("UI_TEXT_SLOT_EMPTY", FRONTEND_MENU_SOURCE)
		self.assertIn("UI_TEXT_SLOT_SAVED", FRONTEND_MENU_SOURCE)
		self.assertNotIn('strcpy(time_buf, "free")', MENU_SOURCE)
		self.assertNotIn("strftime(time_buf", MENU_SOURCE)
		self.assertNotIn('strcpy(time_buf, "free")', FRONTEND_MENU_SOURCE)
		self.assertNotIn("strftime(time_buf", FRONTEND_MENU_SOURCE)

	def test_cheat_menu_uses_styled_columns_and_marquee(self):
		self.assertIn("draw_cheats_menu_styled(", FRONTEND_MENU_SOURCE)
		self.assertIn("menu_loop_cheats_styled(", FRONTEND_MENU_SOURCE)
		self.assertIn("ui_text(UI_TEXT_CHEATS)", FRONTEND_MENU_SOURCE)
		self.assertIn("menu_style_option_columns(", FRONTEND_MENU_SOURCE)
		self.assertIn("menu_marquee_offset(", FRONTEND_MENU_SOURCE)

	def test_styled_menu_uses_reference_spacing_and_lower_list_origin(self):
		self.assertIn("spacing = (font_px * 3 + 2) / 5;", MENU_LAYOUT_SOURCE)
		self.assertIn(
			"title_height + 3 * line_height / 4",
			MENU_STYLE_SOURCE,
		)

	def test_selected_text_uses_unshadowed_renderer(self):
		self.assertIn(
			"int menu_sdl2_draw_text_clipped_unshadowed(",
			SDL2_MENU_SOURCE,
		)
		self.assertGreaterEqual(
			FRONTEND_MENU_SOURCE.count(
				"menu_sdl2_draw_text_clipped_unshadowed("
			),
			2,
		)

	def test_styled_menu_centers_body_text_and_keeps_title_regular(self):
		self.assertIn("int menu_sdl2_font_height(", SDL2_MENU_SOURCE)
		self.assertNotIn(
			"TTF_SetFontStyle(state.fonts[MENU_FONT_TITLE]",
			SDL2_MENU_SOURCE,
		)
		self.assertIn(
			"(line_height - text_height) / 2",
			MENU_STYLE_SOURCE,
		)

	def test_sdl2_ttf_text_draws_shadow_before_foreground(self):
		text_draw = re.search(
			r"int menu_sdl2_draw_text\(.*?\)\s*\{(?P<body>.*?)"
			r"\n\}",
			SDL2_MENU_SOURCE,
			re.DOTALL,
		)
		self.assertIsNotNone(text_draw)
		body = text_draw.group("body")
		self.assertIn("MENU_SDL2_SHADOW_COLOR", body)
		self.assertLess(
			body.index("MENU_SDL2_SHADOW_COLOR"),
			body.index("color, utf8"),
		)

	def test_sdl2_selection_uses_flat_solid_color(self):
		skin = Path(__file__).resolve().parents[1] / "skin" / "skin.txt"
		self.assertTrue(skin.is_file())
		settings = dict(
			line.split("=", 1)
			for line in skin.read_text(encoding="ascii").splitlines()
			if line and not line.startswith("#")
		)
		self.assertEqual(settings["selection_color"], "1e4f78")

	def test_sdl2_cheat_descriptions_are_not_byte_truncated(self):
		self.assertRegex(
			CHEAT_SOURCE,
			re.compile(
				r"#ifndef USE_SDL2\s+"
				r"string_truncate\(\(char \*\)cheat->name, MAX_DESC_LEN\);\s+"
				r"#endif"
			),
		)

	def test_marquee_resets_when_a_menu_is_entered(self):
		loop = re.search(
			r"static int me_loop_d\(.*?\)\s*\{(?P<body>.*?)"
			r"\n\}\n\nstatic int me_loop",
			MENU_SOURCE,
			re.DOTALL,
		)
		self.assertIsNotNone(loop)
		body = loop.group("body")
		self.assertLess(body.index("menu_marquee_reset();"),
				body.index("me_draw(menu, sel, NULL);"))

	def test_sdl2_menu_wait_returns_for_animation_frames(self):
		loop = re.search(
			r"static int me_loop_d\(.*?\)\s*\{(?P<body>.*?)"
			r"\n\}\n\nstatic int me_loop",
			MENU_SOURCE,
			re.DOTALL,
		)
		self.assertIsNotNone(loop)
		self.assertRegex(
			loop.group("body"),
			re.compile(
				r"#ifdef USE_SDL2\s+if \(marquee_active\)\s+"
				r"inp = in_menu_wait_with_callback\(.*?NULL, 70, 70,\s+"
				r"menu_idle_redraw, &redraw\);.*?"
				r"#else\s+inp = in_menu_wait\(.*?, NULL, 70\);\s+#endif",
				re.DOTALL,
			),
		)
		wait = re.search(
			r"static int in_menu_wait_any_with_callback\(.*?\)\s*"
			r"\{(?P<body>.*?)\n\}",
			INPUT_SOURCE,
			re.DOTALL,
		)
		self.assertIsNotNone(wait)
		self.assertIn("redraw(redraw_data);", wait.group("body"))
		self.assertIn("ret != menu_key_prev", wait.group("body"))

	def test_enum_alignment_uses_ten_cell_legacy_rule(self):
		self.assert_non_sdl_branch(
			"len = strlen(names[i]);",
			"int width = menu_text_width(names[i], 0);",
		)

	def test_savestate_geometry_uses_original_fixed_cells(self):
		self.assertRegex(
			MENU_SOURCE,
			re.compile(
				r"#ifdef USE_SDL2\s+"
				r"w = menu_text_width\(title, 0\);.*?"
				r"#else\s+w = \(13 \+ 2\) \* me_mfont_w;\s+#endif",
				re.DOTALL,
			),
		)
		self.assert_non_sdl_branch(
			"(23 + 2) * me_mfont_w + 4",
			"w + 4",
		)

	def test_key_config_geometry_uses_original_fixed_cells(self):
		self.assert_non_sdl_branch(
			"w = ((player_idx >= 0) ? 20 : 30) * me_mfont_w;",
			"w = 0;",
		)
		self.assert_non_sdl_branch(
			"w = strlen(dev_name) * me_mfont_w;",
			"w = menu_text_width(dev_name, 0);",
		)

	def test_bitmap_geometry_is_restored_before_font_data_is_built(self):
		init_body = re.search(
			r"void menu_init_base\(void\)\s*\{(?P<body>.*?)"
			r"menu_font_data = calloc",
			MENU_SOURCE,
			re.DOTALL,
		)
		self.assertIsNotNone(init_body)
		body = init_body.group("body")
		self.assertIn("me_mfont_w = MENU_X2 ? 16 : 8;", body)
		self.assertIn("me_mfont_h = MENU_X2 ? 20 : 10;", body)
		self.assertIn("me_sfont_w = MENU_X2 ? 12 : 6;", body)
		self.assertIn("me_sfont_h = MENU_X2 ? 20 : 10;", body)

	def test_legacy_message_width_counts_full_lines_beyond_255_bytes(self):
		body = re.search(
			r"static void draw_menu_message\(.*?\)\s*\{(?P<body>.*?)"
			r"\n\}\n\n// -------------- del confirm",
			MENU_SOURCE,
			re.DOTALL,
		)
		self.assertIsNotNone(body)
		self.assertRegex(
			body.group("body"),
			re.compile(
				r"#ifdef USE_SDL2.*?"
				r"if \(length >= sizeof\(line\)\).*?"
				r"wt = menu_text_width\(line, 0\);.*?"
				r"#else\s+"
				r"for \(wt = 0; \*p != 0 && \*p != '\\n'; p\+\+\)\s+"
				r"wt\+\+;\s+"
				r"#endif.*?"
				r"#ifdef USE_SDL2\s+"
				r"x = g_menuscreen_w / 2 - w / 2;\s+"
				r"#else\s+"
				r"x = g_menuscreen_w / 2 - w \* me_mfont_w / 2;\s+"
				r"#endif",
				re.DOTALL,
			),
		)
		long_line = "x" * 300
		self.assertEqual(len(long_line) * 8, 2400)
		self.assertGreater(len(long_line), 255)


if __name__ == "__main__":
	unittest.main()
