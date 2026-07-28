import tempfile
import unittest
from pathlib import Path

from tools import gen_ui_catalog


class CatalogValidationTest(unittest.TestCase):
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


if __name__ == "__main__":
	unittest.main()
