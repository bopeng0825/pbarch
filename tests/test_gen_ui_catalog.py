import tempfile
import unittest
from pathlib import Path

from tools import gen_ui_catalog


class CatalogValidationTest(unittest.TestCase):
	def assert_invalid_translation(self, english, translation, message):
		with tempfile.TemporaryDirectory() as directory:
			source = Path(directory) / "ui.tsv"
			source.write_text(
				"key\ten\tzh_CN\tzh_TW\n"
				f"sample\t{english}\t{translation}\t{english}\n",
				encoding="utf-8",
			)
			original_source = gen_ui_catalog.SOURCE
			gen_ui_catalog.SOURCE = source
			try:
				with self.assertRaisesRegex(ValueError, message):
					gen_ui_catalog.read_catalog()
			finally:
				gen_ui_catalog.SOURCE = original_source

	def test_rejects_source_key_reserved_for_english_fallback(self):
		with tempfile.TemporaryDirectory() as directory:
			source = Path(directory) / "ui.tsv"
			source.write_text(
				"key\ten\tzh_CN\tzh_TW\n"
				"test_english_fallback\tSource text\t源文本\t來源文字\n",
				encoding="utf-8",
			)
			original_source = gen_ui_catalog.SOURCE
			gen_ui_catalog.SOURCE = source
			try:
				with self.assertRaisesRegex(
					ValueError,
					"duplicate catalog key: test_english_fallback",
				):
					gen_ui_catalog.read_catalog()
			finally:
				gen_ui_catalog.SOURCE = original_source

	def test_rejects_changed_printf_conversion_type(self):
		self.assert_invalid_translation(
			"Slot %i", "槽位 %s", "printf signature mismatch"
		)

	def test_rejects_missing_printf_conversion(self):
		self.assert_invalid_translation(
			"Slot %i", "槽位", "printf signature mismatch"
		)

	def test_rejects_extra_printf_conversion(self):
		self.assert_invalid_translation(
			"Slot", "槽位 %i", "printf signature mismatch"
		)

	def test_rejects_percent_n_even_when_signatures_match(self):
		self.assert_invalid_translation(
			"Count %n", "计数 %n", "forbidden printf conversion"
		)

	def test_escaped_percent_does_not_add_a_conversion(self):
		with tempfile.TemporaryDirectory() as directory:
			source = Path(directory) / "ui.tsv"
			source.write_text(
				"key\ten\tzh_CN\tzh_TW\n"
				"sample\tProgress 100%%: %i\t进度 100%%：%i\t進度 100%%：%i\n",
				encoding="utf-8",
			)
			original_source = gen_ui_catalog.SOURCE
			gen_ui_catalog.SOURCE = source
			try:
				self.assertEqual(gen_ui_catalog.read_catalog()[0][0], "sample")
			finally:
				gen_ui_catalog.SOURCE = original_source


if __name__ == "__main__":
	unittest.main()
