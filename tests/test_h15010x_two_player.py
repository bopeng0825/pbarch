import pathlib
import re
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]


class H15010xTwoPlayerTest(unittest.TestCase):
    def test_base_device_config_matches_identical_player_two(self):
        header = (ROOT / "libpicofe/input.h").read_text(encoding="utf-8")
        input_source = (ROOT / "libpicofe/input.c").read_text(
            encoding="utf-8"
        )
        config_source = (ROOT / "libpicofe/config_file.c").read_text(
            encoding="utf-8"
        )
        driver = (ROOT / "plat_h150101_sdl2_input.c").read_text(
            encoding="utf-8"
        )

        self.assertIn(
            "int (*config_match)(const char *configured_name, "
            "const char *device_name);",
            header,
        )
        self.assertIn(
            "int  in_config_parse_devs(const char *name, int *dev_ids, "
            "int max_ids);",
            header,
        )
        self.assertIn(
            "DRV(dev->drv_id).config_match(name, dev->name)",
            input_source,
        )
        self.assertIn(".config_match   = h150101_sdl2_config_match", driver)
        self.assertIn(
            "in_config_parse_devs(dev, dev_ids, IN_MAX_DEVS)",
            config_source,
        )
        self.assertRegex(
            config_source,
            r"for \(i = 0; i < dev_count; i\+\+\)\s+"
            r"in_unbind_all\(dev_ids\[i\], -1, -1\);",
        )
        self.assertRegex(
            config_source,
            r"for \(i = 0; i < dev_count; i\+\+\)\s+"
            r"in_config_bind_key\(dev_ids\[i\], key, bind, bindtype\);",
        )

    def test_shared_matcher_restricts_aliases_to_identical_names(self):
        driver = (ROOT / "plat_h150101_sdl2_input.c").read_text(
            encoding="utf-8"
        )
        self.assertIn("static int h150101_sdl2_config_match", driver)
        self.assertIn("strcmp(configured_name, device_name) == 0", driver)
        self.assertIn('IN_H150101_SDL2_PREFIX "p2:"', driver)
        self.assertIn(
            "strcmp(configured_name + prefix_len, device_name + p2_len) == 0",
            driver,
        )

    def test_multi_device_config_results_are_unique_and_bounded(self):
        source = (ROOT / "libpicofe/input.c").read_text(encoding="utf-8")
        self.assertIn("int in_config_parse_devs", source)
        self.assertIn("count < max_ids", source)
        self.assertIn("dev_ids[j] == i", source)

    def test_sdl2_supports_eight_axes_without_new_defaults(self):
        header = (ROOT / "plat_h150101_sdl2_input.h").read_text(
            encoding="utf-8"
        )
        input_source = (ROOT / "plat_h150101_sdl2_input.c").read_text(
            encoding="utf-8"
        )
        platform = (ROOT / "plat_h150101.c").read_text(encoding="utf-8")

        self.assertIn("#define H150101_SDL2_AXIS_COUNT 8", header)
        for axis in range(4, 8):
            self.assertIn(
                f'[H150101_SDL2_AXIS_NEG({axis})] = "axis {axis}-"',
                input_source,
            )
            self.assertIn(
                f'[H150101_SDL2_AXIS_POS({axis})] = "axis {axis}+"',
                input_source,
            )
            self.assertNotIn(f"H150101_SDL2_AXIS_NEG({axis}),", platform)
            self.assertNotIn(f"H150101_SDL2_AXIS_POS({axis}),", platform)

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

    def test_sdl_hat_directions_are_available_to_the_menu(self):
        driver = (ROOT / "plat_h150101_sdl2_input.c").read_text(
            encoding="utf-8"
        )
        platform = (ROOT / "plat_h150101.c").read_text(encoding="utf-8")
        update_keycode = driver[
            driver.index("static int h150101_sdl2_update_keycode"):
            driver.index("static int h150101_sdl2_menu_translate")
        ]

        self.assertIn("case SDL_JOYHATMOTION:", update_keycode)
        self.assertIn("H150101_SDL2_AXIS_NEG(0),  PBTN_LEFT", platform)
        self.assertIn("H150101_SDL2_AXIS_POS(0),  PBTN_RIGHT", platform)
        self.assertIn("H150101_SDL2_AXIS_NEG(1),  PBTN_UP", platform)
        self.assertIn("H150101_SDL2_AXIS_POS(1),  PBTN_DOWN", platform)

    def test_select_start_opens_menu_once_and_never_quits(self):
        source = (ROOT / "plat_h150101_sdl2_input.c").read_text(
            encoding="utf-8"
        )
        state = source[
            source.index("struct h150101_sdl2_state"):
            source.index("struct h150101_sdl2_pdata")
        ]
        update = source[
            source.index("static int h150101_sdl2_update("):
            source.index("static int h150101_sdl2_update_keycode")
        ]

        self.assertIn("int menu_combo_latched;", state)
        self.assertRegex(
            update,
            r"state->keys\[H150101_SDL2_BUTTON\(8\)\]\s*&&\s*"
            r"state->keys\[H150101_SDL2_BUTTON\(9\)\]",
        )
        self.assertIn("!state->menu_combo_latched", update)
        self.assertIn("1 << EACTION_MENU", update)
        self.assertIn("state->menu_combo_latched = 1", update)
        self.assertIn("state->menu_combo_latched = 0", update)
        self.assertNotIn("quit_count", source)
        self.assertNotIn("EACTION_QUIT", update)

    def test_select_start_state_is_resynced_before_combo_detection(self):
        source = (ROOT / "plat_h150101_sdl2_input.c").read_text(
            encoding="utf-8"
        )
        update = source[
            source.index("static int h150101_sdl2_update("):
            source.index("static int h150101_sdl2_update_keycode")
        ]

        joystick_update = update.index("SDL_JoystickUpdate();")
        self.assertIn("\tsync_button_key(state, 8);", update)
        self.assertIn("\tsync_button_key(state, 9);", update)
        sync_select = update.index("\tsync_button_key(state, 8);")
        sync_start = update.index("\tsync_button_key(state, 9);")
        combo_check = update.index(
            "state->keys[H150101_SDL2_BUTTON(8)] &&"
        )

        self.assertLess(joystick_update, sync_select)
        self.assertLess(sync_select, sync_start)
        self.assertLess(sync_start, combo_check)

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
