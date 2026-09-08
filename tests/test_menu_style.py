import os
import pathlib
import shutil
import subprocess
import tempfile
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]


class MenuStyleTest(unittest.TestCase):
    def test_geometry_and_flat_rgb565_rendering(self):
        compiler = os.environ.get("HOST_CC") or next(
            (path for name in ("cc", "gcc", "clang")
             if (path := shutil.which(name))),
            None,
        )
        if compiler is None:
            self.skipTest("no C compiler available for behavioral harness")

        with tempfile.TemporaryDirectory() as tmpdir:
            executable = pathlib.Path(tmpdir) / "menu_style_harness"
            env = os.environ.copy()
            env["PATH"] = (
                str(pathlib.Path(compiler).parent) + os.pathsep + env["PATH"]
            )
            command = [
                compiler,
                "-std=c99",
                "-Wall",
                "-Wextra",
                "-Werror",
                "-I", str(ROOT),
                str(ROOT / "tests/menu_style_harness.c"),
                str(ROOT / "menu_style.c"),
                str(ROOT / "menu_layout.c"),
                "-o", str(executable),
            ]
            try:
                subprocess.run(
                    command, check=True, cwd=ROOT, env=env,
                    capture_output=True, text=True,
                )
            except subprocess.CalledProcessError as error:
                self.fail(error.stderr)
            subprocess.run([str(executable)], check=True, cwd=ROOT, env=env)


if __name__ == "__main__":
    unittest.main()
