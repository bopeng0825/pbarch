import pathlib
import re
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]


class H15010xTwoPlayerTest(unittest.TestCase):
    def test_input_has_a_separate_player_two_result_slot(self):
        header = (ROOT / "libpicofe/input.h").read_text(encoding="utf-8")

        self.assertRegex(
            header,
            r"IN_BINDTYPE_PLAYER12\s*,\s*"
            r"IN_BINDTYPE_PLAYER2\s*,\s*"
            r"IN_BINDTYPE_COUNT",
        )

    def test_sdl_driver_assigns_only_indices_zero_and_one(self):
        source = (ROOT / "plat_h150101_sdl2_input.c").read_text(
            encoding="utf-8"
        )

        self.assertIn("int player;", source)
        self.assertRegex(source, r"if \(joycount > 2\)\s+joycount = 2;")
        self.assertIn("state->player = i;", source)
        self.assertIn('IN_H150101_SDL2_PREFIX "p2:%s"', source)

    def test_player_one_keeps_legacy_registration_name(self):
        source = (ROOT / "plat_h150101_sdl2_input.c").read_text(
            encoding="utf-8"
        )

        self.assertRegex(
            source,
            r"(?s)state->player == 0\).*?"
            r'IN_H150101_SDL2_PREFIX "%s"',
        )

    def test_sdl_driver_routes_second_device_to_player_two(self):
        source = (ROOT / "plat_h150101_sdl2_input.c").read_text(
            encoding="utf-8"
        )

        self.assertRegex(
            source,
            r"state->player == 1 &&\s*"
            r"b == IN_BINDTYPE_PLAYER12",
        )
        self.assertRegex(
            source,
            r"result\[IN_BINDTYPE_PLAYER2\]\s*\|=\s*"
            r"binds\[IN_BIND_OFFS\(i, b\)\]",
        )

    def test_sdl_driver_preserves_events_for_the_other_player(self):
        source = (ROOT / "plat_h150101_sdl2_input.c").read_text(
            encoding="utf-8"
        )

        self.assertRegex(
            source,
            r"(?s)!handle_event\(state, &event\).*?"
            r"skipped\[skipped_count\+\+\] = event",
        )
        update_keycode = source[source.index("h150101_sdl2_update_keycode"):]
        self.assertIn("skipped[skipped_count++] = event", update_keycode)
        self.assertIn("SDL_PushEvent(&skipped[i])", update_keycode)

    def test_core_routes_joypad_queries_by_libretro_port(self):
        source = (ROOT / "core.c").read_text(encoding="utf-8")

        self.assertRegex(source, r"static uint32_t buttons\[2\]")
        self.assertRegex(
            source,
            r"buttons\[0\]\s*=\s*actions\[IN_BINDTYPE_PLAYER12\]",
        )
        self.assertRegex(
            source,
            r"buttons\[1\]\s*=\s*actions\[IN_BINDTYPE_PLAYER2\]",
        )
        self.assertRegex(source, r"port < 2 && device == RETRO_DEVICE_JOYPAD")
        self.assertIn("buttons[port]", source)


if __name__ == "__main__":
    unittest.main()
