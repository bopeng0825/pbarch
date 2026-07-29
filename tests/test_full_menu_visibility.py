import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class FullMenuVisibilityTest(unittest.TestCase):
	def test_three_entries_share_full_menu_visibility(self):
		source = (ROOT / "menu.c").read_text(encoding="utf-8")
		self.assertRegex(source, r"MA_MAIN_OPTIONS,")
		for menu_id in (
			"MA_MAIN_OPTIONS",
			"MA_MAIN_CONTENT_SEL",
			"MA_MAIN_CREDITS",
		):
			self.assertRegex(
				source,
				r"me_enable\(e_menu_main,\s*" + menu_id +
				r",\s*full_menu_enabled\);",
			)

	def test_main_passes_parsed_flag_to_menu(self):
		source = (ROOT / "main.c").read_text(encoding="utf-8")
		self.assertRegex(
			source,
			r"menu_set_full_menu\(args\.full_menu\);",
		)


if __name__ == "__main__":
	unittest.main()
