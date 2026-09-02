import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]


class StopInputTest(unittest.TestCase):
    def test_sigcont_discards_pending_input_before_next_core_frame(self):
        main = (ROOT / "main.c").read_text(encoding="utf-8")
        input_source = (ROOT / "plat_h150101_sdl2_input.c").read_text(
            encoding="utf-8"
        )

        self.assertIn("static void handle_signal_cont", main)
        handler = main[
            main.index("static void handle_signal_cont"):
            main.index("static void install_signal_handlers")
        ]
        self.assertIn("plat_input_resume_notify()", handler)
        self.assertIn("signal(SIGCONT, handle_signal_cont)", main)
        update = input_source[
            input_source.index("static int h150101_sdl2_update("):
            input_source.index("static int h150101_sdl2_update_keycode")
        ]
        keycode = input_source[
            input_source.index("static int h150101_sdl2_update_keycode"):
            input_source.index("static int h150101_sdl2_menu_translate")
        ]
        self.assertIn("plat_discard_pending_input();", update)
        self.assertIn("plat_discard_pending_input();", keycode)

    def test_resume_cleanup_does_not_reprobe_devices(self):
        source = (ROOT / "plat_sdl.c").read_text(encoding="utf-8")
        cleanup = source[
            source.index("void plat_discard_pending_input"):
            source.index("\n}", source.index("void plat_discard_pending_input"))
        ]

        self.assertNotIn("in_probe();", cleanup)

    def test_sdl_cleanup_pumps_then_flushes_only_input_events(self):
        source = (ROOT / "plat_sdl.c").read_text(encoding="utf-8")
        self.assertIn("void plat_discard_pending_input", source)
        cleanup = source[
            source.index("void plat_discard_pending_input"):
            source.index("\n}", source.index("void plat_discard_pending_input"))
        ]

        self.assertIn("if (!input_resume_requested)", cleanup)
        self.assertIn("input_resume_requested = 0", cleanup)
        self.assertLess(
            cleanup.index("SDL_PumpEvents();"),
            cleanup.index("SDL_FlushEvents(SDL_KEYDOWN, SDL_KEYUP);"),
        )
        self.assertIn(
            "SDL_FlushEvents(SDL_JOYAXISMOTION, SDL_JOYDEVICEREMOVED);",
            cleanup,
        )
        self.assertNotIn("SDL_FlushEvents(SDL_FIRSTEVENT", cleanup)


if __name__ == "__main__":
    unittest.main()
