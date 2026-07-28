#!/usr/bin/env python3
"""Reject frontend-owned English UI literals outside the generated catalog."""

from pathlib import Path
import sys


ROOT = Path(__file__).resolve().parents[1]
SOURCES = (ROOT / "menu.c", ROOT / "libpicofe" / "menu.c")
LITERALS = (
    "Resume game",
    "Save state",
    "Load state",
    "Disc control",
    "Cheats",
    "Options",
    "Reset game",
    "Load new game",
    "About",
    "Exit",
    "Audio and video",
    "Emulator options",
    "Player controls",
    "Emulator hotkeys",
    "Save config",
    "Show FPS",
    "Show CPU usage",
    "Display mode",
    "Zoom level",
    "Screen panning",
    "Screen rotation",
    "Scaling filter",
    "Audio buffer",
    "Audio adjustment",
    "Use .srm saves",
    "Save global config",
    "Save game config",
    "Delete game config",
    "Restore defaults",
    "About to delete",
    "Are you sure?",
    "Press a button to bind/unbind",
    "Press left/right for other devs",
)


def main() -> int:
    failures = []
    for path in SOURCES:
        for lineno, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
            for literal in LITERALS:
                if f'"{literal}"' in line:
                    failures.append(f"{path.relative_to(ROOT)}:{lineno}: {literal}")

    if failures:
        print("frontend UI literals must use catalog IDs:", file=sys.stderr)
        print("\n".join(failures), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
