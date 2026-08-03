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


def _function_body(source: str, signature: str):
    match = re.search(signature + r"\s*\{", source)
    if match is None:
        return None
    depth = 1
    position = match.end()
    start = position
    while position < len(source) and depth:
        if source.startswith("//", position):
            position = source.find("\n", position)
            if position < 0:
                return source[start:]
        elif source.startswith("/*", position):
            position = source.find("*/", position + 2)
            if position < 0:
                return None
            position += 2
        elif source[position] in ('"', "'"):
            quote = source[position]
            position += 1
            while position < len(source) and source[position] != quote:
                position += 2 if source[position] == "\\" else 1
            position += 1
        else:
            depth += source[position] == "{"
            depth -= source[position] == "}"
            position += 1
    return source[start : position - 1] if depth == 0 else None


def find_prohibited_literals(source: str):
    source = re.sub(r"\\\r?\n", "", source)
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
        r"\s*\{.*?menu_sdl2_copy_background\s*\(",
        source,
        re.DOTALL,
    ) is not None


def sdl_list_frame_uses_responsive_preview(source: str) -> bool:
    body = _function_body(source, r"void\s+menu_begin\s*\([^)]*\)")
    if body is None:
        return False
    return all(
        re.search(pattern, body, re.DOTALL) is not None
        for pattern in (
            r"menu_calculate_responsive_layout\s*\(\s*g_menuscreen_w\s*,\s*g_menuscreen_h\s*,",
            r"menu_sdl2_copy_background\s*\(",
            r"menu_sdl2_draw_preview\s*\(\s*g_menubg_ptr\s*,\s*g_menuscreen_w\s*,\s*g_menubg_src_ptr\s*,\s*g_menubg_src_w\s*,\s*g_menubg_src_h\s*,\s*g_menubg_src_pp\s*,",
            r"menu_set_responsive_layout\s*\(",
        )
    )


def ordinary_sdl_list_uses_responsive_geometry(source: str) -> bool:
    body = _function_body(source, r"static\s+void\s+me_draw\b[^{]*")
    if body is None:
        return False
    return all(token in body for token in (
        "menu_get_responsive_layout",
        "menu_centered_block_y",
        "layout.menu.x",
        "layout.menu.w",
    ))


def sdl_menu_enter_captures_completed_frame(source: str) -> bool:
    body = _function_body(source, r"void\s+plat_video_menu_enter\s*\([^)]*\)")
    if body is None:
        return False
    dimensions = _function_body(
        source, r"static\s+int\s+plat_sdl_menu_source_dimensions_match\s*\([^)]*\)"
    )
    return dimensions is not None and all(token in dimensions for token in (
        "g_menubg_src_w == SCREEN_WIDTH",
        "g_menubg_src_h == SCREEN_HEIGHT",
        "g_menubg_src_pp == SCREEN_WIDTH",
    )) and all(token in body for token in (
        "if (is_rom_loaded)",
        "menu_capture_ready",
        "plat_sdl_capture_menu_frame(g_menubg_src_ptr",
        "memcpy(g_menubg_src_ptr, screen_pixels",
    )) and re.search(
        r"else\s*\{\s*memset\s*\(\s*g_menubg_src_ptr\s*,\s*0\s*,",
        body,
        re.DOTALL,
    ) is not None


def sdl_menu_capture_rebuilds_before_readback(source: str) -> bool:
    body = _function_body(
        source, r"static\s+int\s+plat_sdl_capture_menu_frame\s*\([^)]*\)"
    )
    menu_enter = _function_body(
        source, r"void\s+plat_video_menu_enter\s*\([^)]*\)"
    )
    if body is None or menu_enter is None:
        return False

    clear = body.find("SDL_RenderClear(renderer)")
    copy = body.find("SDL_RenderCopy(renderer, screen_texture")
    readback = body.find("plat_sdl_readback_rgb565(pixels, pitch)")
    stable_geometry = all(token in body for token in (
        "screen_texture_ready",
        "screen_use_hw_scaling ? &screen_src_rect : NULL",
        "screen_use_hw_scaling ? &screen_dst_rect : NULL",
    ))
    return (
        stable_geometry
        and 0 <= clear < copy < readback
        and "SDL_RenderPresent" not in body
        and "plat_sdl_capture_menu_frame(g_menubg_src_ptr" in menu_enter
    )


def menu_action_captures_core_frame(main_source: str, plat_source: str) -> bool:
    action = _function_body(main_source, r"void\s+handle_emu_action\s*\([^)]*\)")
    process = _function_body(plat_source, r"void\s+plat_video_process\s*\([^)]*\)")
    enter = _function_body(plat_source, r"void\s+plat_video_menu_enter\s*\([^)]*\)")
    if action is None or process is None or enter is None:
        return False
    return (
        "plat_video_request_menu_capture" in action
        and "menu_capture_requested" in process
        and "plat_sdl_capture_core_frame" in process
        and "menu_capture_ready" in enter
    )


def key_config_overlay_is_last_before_menu_protection(
    source: str, menu_source: str
) -> bool:
    loader = _function_body(
        source, r"void\s+load_config_keys\s*\(\s*const\s+char\s*\*\s*key_config_path\s*\)"
    )
    main = _function_body(source, r"int\s+main\s*\([^)]*\)")
    if loader is None or main is None:
        return False

    normal = loader.find("config_read_keys(config)")
    overlay = loader.find("load_key_config_overlay(key_config_path)")
    protection = loader.find("1 << EACTION_MENU")
    return (
        0 <= normal < overlay < protection
        and "load_config_keys(args.key_config_path)" in main
        and "load_config_keys(NULL)" in menu_source
    )


def key_config_open_failure_preserves_normal_keys(source: str) -> bool:
    helper = _function_body(
        source,
        r"static\s+void\s+load_key_config_overlay\s*\(\s*const\s+char\s*\*\s*key_config_path\s*\)",
    )
    if helper is None:
        return False

    open_failure = re.search(
        r"if\s*\(\s*!config_file\s*\)\s*\{(?P<body>.*?)\}",
        helper,
        re.DOTALL,
    )
    return (
        "fopen(key_config_path" in helper
        and "Couldn't load key config %s\\n" in helper
        and "key_config_path" in helper
        and open_failure is not None
        and "return;" in open_failure.group("body")
        and "config_read_keys" in helper
    )


def key_config_size_is_checked_before_allocation(source: str) -> bool:
    helper = _function_body(
        source,
        r"static\s+void\s+load_key_config_overlay\s*\(\s*const\s+char\s*\*\s*key_config_path\s*\)",
    )
    if helper is None:
        return False

    bound = helper.find("(uintmax_t)length >= (uintmax_t)SIZE_MAX")
    size = helper.find("config_size = (size_t)length + 1")
    allocation = helper.find("calloc(1, config_size)")
    failure = helper[bound:size] if 0 <= bound < size else ""
    return (
        0 <= bound < size < allocation
        and "PA_WARN(" in failure
        and "fclose(config_file)" in failure
        and "return;" in failure
        and "config_read_keys" not in failure
    )


def ordinary_sdl_list_clips_text_and_rows(source: str) -> bool:
    body = _function_body(source, r"static\s+void\s+me_draw\b[^{]*")
    text_body = _function_body(source, r"void\s+text_out16\b\s*\([^)]*\)")
    if body is None or text_body is None:
        return False
    return all(token in body for token in (
        "menu_visible_window",
        "visible_first",
        "visible_count",
    )) and re.search(
        r"menu_text_clip_right\s*=\s*responsive\s*\?\s*"
        r"layout\.menu\.x\s*\+\s*layout\.menu\.w",
        body,
    ) is not None and "clip_right - x" in text_body


def ordinary_sdl_message_is_independent_of_list_height(source: str) -> bool:
    body = _function_body(source, r"static\s+void\s+me_draw\b[^{]*")
    if body is None:
        return False
    return all(token in body for token in (
        "int message_margin = responsive ? layout.outer_margin : 5",
        "g_menuscreen_h - me_mfont_h - message_margin",
    )) and re.search(
        r"#ifdef\s+USE_SDL2.*?text_out16\s*\(\s*message_margin,.*?"
        r"#else.*?if\s*\(h\s*>=",
        body,
        re.DOTALL,
    ) is not None


def ordinary_sdl_list_omits_permanent_help(source: str) -> bool:
    body = _function_body(source, r"static\s+void\s+me_draw\b[^{]*")
    if body is None:
        return False
    return (
        "menu_error_msg[0] != 0" in body
        and re.search(r"#ifndef\s+USE_SDL2.*?menu_entry_help", body, re.DOTALL)
        is not None
    )


def menu_message_uses_utf8_fitting(source: str) -> bool:
    function = re.search(
        r"void\s+menu_update_msg\s*\([^)]*\)\s*\{(?P<body>.*?)\n\}",
        source,
        re.DOTALL,
    )
    return function is not None and re.search(
        r"\bmenu_text_fit\s*\(\s*msg\s*,", function.group("body")
    ) is not None


def sdl_menu_message_uses_outer_margins(source: str) -> bool:
    body = _function_body(source, r"void\s+menu_update_msg\s*\([^)]*\)")
    return body is not None and all(token in body for token in (
        "menu_calculate_responsive_layout",
        "layout.outer_margin * 2",
    ))


def sdl_unavailable_uses_bitmap_byte_width(source: str) -> bool:
    function = re.search(
        r"static\s+int\s+menu_text_width\s*\([^)]*\)\s*\{"
        r"(?P<body>.*?)\n\}",
        source,
        re.DOTALL,
    )
    if function is None:
        return False
    return re.search(
        r"if\s*\(\s*menu_sdl2_available\s*\(\s*\)\s*\).*?"
        r"return\s+menu_sdl2_text_width\s*\(.*?;"
        r"\s*return\s+menu_bitmap_text_width\s*\(",
        function.group("body"),
        re.DOTALL,
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
    libpicofe_source = SOURCES[1].read_text(encoding="utf-8")
    if not sdl_unavailable_uses_bitmap_byte_width(libpicofe_source):
        print("libpicofe/menu.c: SDL fallback must use bitmap width", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
