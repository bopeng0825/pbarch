import re
import unittest
from pathlib import Path


MENU_SOURCE = (
	Path(__file__).resolve().parents[1] / "libpicofe" / "menu.c"
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
		self.assert_non_sdl_branch(
			"wt += 10 * me_mfont_w;",
			"wt += value_width;",
		)

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


if __name__ == "__main__":
	unittest.main()
