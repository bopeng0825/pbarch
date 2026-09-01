import os
import pathlib
import shutil
import subprocess
import tempfile
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]


class ContentPathTest(unittest.TestCase):
    def test_rom_relative_cheat_path_behavior(self):
        compiler = os.environ.get("HOST_CC") or next(
            (path for name in ("cc", "gcc", "clang")
             if (path := shutil.which(name))),
            None,
        )
        if compiler is None:
            self.skipTest("no C compiler available for behavioral harness")

        with tempfile.TemporaryDirectory() as tmpdir:
            executable = pathlib.Path(tmpdir) / "content_path_harness"
            env = os.environ.copy()
            env["PATH"] = (
                str(pathlib.Path(compiler).parent) + os.pathsep + env["PATH"]
            )
            command = [
                compiler,
                "-std=c99",
                "-D_POSIX_C_SOURCE=200809L",
                "-DPATH_MAX=4096",
                "-I", str(ROOT),
                "-I", str(ROOT / "libretro-common/include"),
                str(ROOT / "tests/content_path_harness.c"),
                str(ROOT / "content_path.c"),
                "-o", str(executable),
            ]
            subprocess.run(command, check=True, cwd=ROOT, env=env)
            subprocess.run([str(executable)], check=True, cwd=ROOT, env=env)


if __name__ == "__main__":
    unittest.main()
