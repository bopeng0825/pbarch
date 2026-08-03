#!/usr/bin/env python3

import sys
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tests"))

import check_ui_literals


class LiteralScannerTests(unittest.TestCase):
    def test_adjacent_literals_are_joined(self):
        source = 'const char *s = "About to " /* gap */\n "delete";\n'
        self.assertEqual(
            check_ui_literals.find_prohibited_literals(source),
            [(1, "About to delete")],
        )

    def test_backslash_lf_splicing_precedes_literal_joining(self):
        source = 'const char *s = "About to "\\\n"delete";\n'
        self.assertEqual(
            check_ui_literals.find_prohibited_literals(source),
            [(1, "About to delete")],
        )

    def test_backslash_crlf_splicing_precedes_literal_joining(self):
        source = 'const char *s = "About to "\\\r\n"delete";\r\n'
        self.assertEqual(
            check_ui_literals.find_prohibited_literals(source),
            [(1, "About to delete")],
        )

    def test_translation_inactive_legacy_literal_is_allowed(self):
        source = """
#ifdef MENU_TRANSLATION_IDS
return menu_translate(UI_TEXT_ABOUT_TO_DELETE);
#else
return "About to delete";
#endif
"""
        self.assertEqual(check_ui_literals.find_prohibited_literals(source), [])

    def test_active_translation_literal_is_rejected(self):
        source = """
#ifdef MENU_TRANSLATION_IDS
return "About to " "delete";
#else
return "legacy";
#endif
"""
        self.assertEqual(
            check_ui_literals.find_prohibited_literals(source),
            [(3, "About to delete")],
        )


class IntegrationGuardTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.menu_source = (ROOT / "menu.c").read_text(encoding="utf-8")
        cls.libpicofe_menu_source = (
            ROOT / "libpicofe" / "menu.c"
        ).read_text(encoding="utf-8")
        cls.plat_sdl_source = (ROOT / "plat_sdl.c").read_text(
            encoding="utf-8"
        )

    def test_non_sdl_build_forces_english(self):
        self.assertTrue(
            check_ui_literals.non_sdl_forces_english(self.menu_source)
        )

    def test_sdl_background_preserves_alt_preview(self):
        self.assertTrue(
            check_ui_literals.background_copy_preserves_alt_preview(
                self.menu_source
            )
        )

    def test_sdl_list_frame_uses_responsive_preview(self):
        self.assertTrue(
            check_ui_literals.sdl_list_frame_uses_responsive_preview(
                self.menu_source
            )
        )

    def test_ordinary_sdl_list_uses_responsive_geometry(self):
        self.assertTrue(
            check_ui_literals.ordinary_sdl_list_uses_responsive_geometry(
                self.libpicofe_menu_source
            )
        )

    def test_ordinary_sdl_list_omits_permanent_help(self):
        self.assertTrue(
            check_ui_literals.ordinary_sdl_list_omits_permanent_help(
                self.libpicofe_menu_source
            )
        )

    def test_sdl_menu_enter_keeps_readback_and_fallback_sources_separate(self):
        self.assertTrue(
            check_ui_literals.sdl_menu_enter_captures_completed_frame(
                self.plat_sdl_source
            )
        )

    def test_sdl_menu_capture_rebuilds_stable_frame_before_readback(self):
        self.assertTrue(
            check_ui_literals.sdl_menu_capture_rebuilds_before_readback(
                self.plat_sdl_source
            )
        )

    def test_ordinary_sdl_list_clips_long_text_and_rows(self):
        self.assertTrue(
            check_ui_literals.ordinary_sdl_list_clips_text_and_rows(
                self.libpicofe_menu_source
            )
        )

    def test_ordinary_sdl_message_is_independent_of_list_height(self):
        self.assertTrue(
            check_ui_literals.ordinary_sdl_message_is_independent_of_list_height(
                self.libpicofe_menu_source
            )
        )

    def test_menu_message_uses_utf8_fitting(self):
        self.assertTrue(
            check_ui_literals.menu_message_uses_utf8_fitting(self.menu_source)
        )

    def test_sdl_menu_message_uses_outer_margins(self):
        self.assertTrue(
            check_ui_literals.sdl_menu_message_uses_outer_margins(
                self.menu_source
            )
        )

    def test_sdl_unavailable_uses_bitmap_byte_width(self):
        self.assertTrue(
            check_ui_literals.sdl_unavailable_uses_bitmap_byte_width(
                self.libpicofe_menu_source
            )
        )


if __name__ == "__main__":
    unittest.main()
