#!/usr/bin/env python3
"""Reject frontend-owned English UI literals outside the generated catalog."""

import ast
from pathlib import Path
import re
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


def _translation_active_source(source: str) -> str:
    output = []
    active = True
    stack = []

    for line in source.splitlines(keepends=True):
        directive = re.match(
            r"\s*#\s*(ifdef|ifndef|if|else|elif|endif)\b(.*)", line
        )
        if directive:
            kind, expression = directive.groups()
            if kind in ("ifdef", "ifndef", "if"):
                parent = active
                if kind == "ifdef" and expression.strip() == "MENU_TRANSLATION_IDS":
                    condition = True
                    known = True
                elif kind == "ifndef" and expression.strip() == "MENU_TRANSLATION_IDS":
                    condition = False
                    known = True
                elif kind == "if" and re.fullmatch(
                    r"\s*defined\s*\(\s*MENU_TRANSLATION_IDS\s*\)\s*",
                    expression,
                ):
                    condition = True
                    known = True
                else:
                    condition = True
                    known = False
                stack.append((parent, condition, known))
                active = parent and condition
            elif kind in ("else", "elif") and stack:
                parent, condition, known = stack[-1]
                active = parent and (not condition if known else True)
            elif kind == "endif" and stack:
                parent, _, _ = stack.pop()
                active = parent
            output.append("\n" if line.endswith("\n") else "")
        elif active:
            output.append(line)
        else:
            output.append("\n" if line.endswith("\n") else "")
    return "".join(output)


def _skip_separators(source: str, position: int) -> int:
    length = len(source)
    while position < length:
        if source[position].isspace():
            position += 1
        elif source.startswith("//", position):
            newline = source.find("\n", position + 2)
            position = length if newline < 0 else newline
        elif source.startswith("/*", position):
            end = source.find("*/", position + 2)
            position = length if end < 0 else end + 2
        else:
            break
    return position


def _read_string(source: str, position: int):
    if position >= len(source) or source[position] != '"':
        return None
    end = position + 1
    while end < len(source):
        if source[end] == "\\":
            end += 2
        elif source[end] == '"':
            token = source[position : end + 1]
            try:
                return ast.literal_eval(token), end + 1
            except (SyntaxError, ValueError):
                return None
        else:
            end += 1
    return None


def find_prohibited_literals(source: str):
    source = _translation_active_source(source)
    failures = []
    position = 0

    while position < len(source):
        if source.startswith("//", position) or source.startswith("/*", position):
            position = _skip_separators(source, position)
            continue
        if source[position] == "'":
            position += 1
            while position < len(source):
                if source[position] == "\\":
                    position += 2
                elif source[position] == "'":
                    position += 1
                    break
                else:
                    position += 1
            continue

        parsed = _read_string(source, position)
        if parsed is None:
            position += 1
            continue

        start = position
        value, position = parsed
        while True:
            next_position = _skip_separators(source, position)
            parsed = _read_string(source, next_position)
            if parsed is None:
                break
            suffix, position = parsed
            value += suffix

        if value in LITERALS:
            failures.append((source.count("\n", 0, start) + 1, value))
    return failures


def non_sdl_forces_english(source: str) -> bool:
    return re.search(
        r"#\s*ifndef\s+USE_SDL2\s+"
        r"ui_language_set\s*\(\s*UI_LANG_EN\s*\)\s*;\s+"
        r"#\s*endif",
        source,
    ) is not None


def background_copy_preserves_alt_preview(source: str) -> bool:
    return re.search(
        r"if\s*\(\s*menu_sdl2_initialized\s*&&\s*!drew_alt_bg\s*\)"
        r"\s*\{\s*menu_sdl2_copy_background\s*\(",
        source,
    ) is not None


def menu_message_uses_utf8_fitting(source: str) -> bool:
    function = re.search(
        r"void\s+menu_update_msg\s*\([^)]*\)\s*\{(?P<body>.*?)\n\}",
        source,
        re.DOTALL,
    )
    return function is not None and re.search(
        r"\bmenu_text_fit\s*\(\s*msg\s*,", function.group("body")
    ) is not None


def main() -> int:
    failures = []
    for path in SOURCES:
        source = path.read_text(encoding="utf-8")
        for lineno, literal in find_prohibited_literals(source):
            failures.append(f"{path.relative_to(ROOT)}:{lineno}: {literal}")

    if failures:
        print("frontend UI literals must use catalog IDs:", file=sys.stderr)
        print("\n".join(failures), file=sys.stderr)
        return 1
    menu_source = SOURCES[0].read_text(encoding="utf-8")
    if not non_sdl_forces_english(menu_source):
        print("menu.c: non-SDL2 builds must force UI_LANG_EN", file=sys.stderr)
        return 1
    if not background_copy_preserves_alt_preview(menu_source):
        print("menu.c: SDL2 background must preserve alternate preview", file=sys.stderr)
        return 1
    if not menu_message_uses_utf8_fitting(menu_source):
        print("menu.c: menu messages must use UTF-8-safe fitting", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
