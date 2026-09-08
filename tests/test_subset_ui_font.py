import unittest

from tools import subset_ui_font


class BundledFontCoverageTest(unittest.TestCase):
	def test_bundled_font_contains_every_catalog_character(self):
		available = subset_ui_font.sfnt_codepoints(subset_ui_font.FONT)
		required = {
			ord(character) for character in subset_ui_font.catalog_codepoints()
		}
		self.assertEqual(required - available, set())


if __name__ == "__main__":
	unittest.main()
