import pathlib
import re
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]


def make_branch(makefile, platform):
    pattern = (
        rf"else ifeq \(\$\(platform\), {re.escape(platform)}\)"
        rf"(?P<body>.*?)"
        rf"(?=else ifeq \(\$\(platform\),|\nendif)"
    )
    match = re.search(pattern, makefile, re.DOTALL)
    return match.group("body") if match else ""


class H150102PlatformTest(unittest.TestCase):
    def test_build_target_selects_h150102_platform_input(self):
        makefile = (ROOT / "Makefile").read_text(encoding="utf-8")
        branch = make_branch(makefile, "h150102")

        self.assertTrue(branch)
        for expected in (
            "plat_h150102.c",
            "plat_h150101_sdl2_input.c",
            "menu_sdl2.c",
            "text_cache.c",
            "-DUSE_SDL2",
            "-DH150102",
            "-march=mips32r2",
            "-mhard-float",
            "-lSDL2_ttf",
            'CONTENT_DIR=\'"/mnt"\'',
        ):
            self.assertIn(expected, branch)
        self.assertNotIn("plat_h150101.c", branch)
        self.assertNotIn("plat_h150102_sdl2_input.c", branch)

    def test_h150102_starts_with_h150101_input_layout(self):
        platform_sources = [
            (ROOT / filename).read_text(encoding="utf-8")
            for filename in ("plat_h150101.c", "plat_h150102.c")
        ]

        for source in platform_sources:
            for mapping in (
                "H150101_SDL2_BUTTON(0),    IN_BINDTYPE_PLAYER12, RETRO_DEVICE_ID_JOYPAD_B",
                "H150101_SDL2_BUTTON(1),    IN_BINDTYPE_PLAYER12, RETRO_DEVICE_ID_JOYPAD_A",
                "H150101_SDL2_BUTTON(0),    PBTN_MBACK",
                "H150101_SDL2_BUTTON(1),    PBTN_MOK",
            ):
                self.assertIn(mapping, source)

    def test_each_platform_supplies_its_select_start_menu_combo(self):
        h150101 = (ROOT / "plat_h150101.c").read_text(encoding="utf-8")
        h150102 = (ROOT / "plat_h150102.c").read_text(encoding="utf-8")

        self.assertRegex(
            h150101,
            r"h150101_sdl2_menu_combo\[2\]\s*=\s*\{\s*8,\s*9\s*\}",
        )
        self.assertRegex(
            h150102,
            r"h150101_sdl2_menu_combo\[2\]\s*=\s*\{\s*10,\s*11\s*\}",
        )

    def test_resolution_constants_are_device_specific(self):
        scale_h = (ROOT / "scale.h").read_text(encoding="utf-8")

        self.assertRegex(
            scale_h,
            r"#elif defined\(H150101\)\s+"
            r"#define SCREEN_WIDTH 640\s+"
            r"#define SCREEN_HEIGHT 480",
        )
        self.assertRegex(
            scale_h,
            r"#elif defined\(H150102\)\s+"
            r"#define SCREEN_WIDTH 640\s+"
            r"#define SCREEN_HEIGHT 360",
        )

    def test_sdl_runtime_behavior_is_shared_with_h150101(self):
        plat_sdl = (ROOT / "plat_sdl.c").read_text(encoding="utf-8")
        shared_guard = r"#if defined\(H150101\) \|\| defined\(H150102\)"

        for behavior in (
            r'if \(!getenv\("SDL_VIDEODRIVER"\)\)',
            r"if \(screen_renderer_vsync &&",
            r"SDL_WINDOW_SHOWN \| SDL_WINDOW_FULLSCREEN_DESKTOP",
        ):
            self.assertRegex(
                plat_sdl,
                shared_guard + r"(?:(?!#endif)[\s\S])*?" + behavior,
            )

    def test_menu_background_uses_height_times_pitch(self):
        menu = (ROOT / "menu.c").read_text(encoding="utf-8")

        self.assertIn(
            "calloc(g_menuscreen_h * g_menuscreen_pp, sizeof(uint16_t))",
            menu,
        )


if __name__ == "__main__":
    unittest.main()
