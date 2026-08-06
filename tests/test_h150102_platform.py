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
    def test_build_target_reuses_h150101_platform(self):
        makefile = (ROOT / "Makefile").read_text(encoding="utf-8")
        branch = make_branch(makefile, "h150102")

        self.assertTrue(branch)
        for expected in (
            "plat_h150101.c",
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
        self.assertNotIn("plat_h150102.c", branch)
        self.assertNotIn("plat_h150102_sdl2_input.c", branch)

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
            r"#define SCREEN_WIDTH 1280\s+"
            r"#define SCREEN_HEIGHT 720",
        )


if __name__ == "__main__":
    unittest.main()
