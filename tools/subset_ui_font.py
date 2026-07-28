#!/usr/bin/env python3
"""Build and verify the small font and background used by the SDL2 menu."""

import argparse
import csv
import hashlib
import shutil
import struct
import subprocess
import sys
import zlib
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
CATALOG = ROOT / "locales" / "ui.tsv"
SKIN_DIR = ROOT / "skin"
FONT = SKIN_DIR / "picoarch-ui.ttf"
LICENSE = SKIN_DIR / "OFL.txt"
BACKGROUND = SKIN_DIR / "background.png"
GLYPH_LIST = ROOT / "build" / "ui-glyphs.txt"
FAMILY_NAME = "Picoarch UI"
FONTTOOLS_VERSION = "4.63.0"
SOURCE_SHA256 = "9fe57a71eb48e50cccb77903123d08857edefcf289c7b309462c4c3d82208126"
BACKGROUND_RGB = (24, 28, 24)
PNG_SIZE = (8, 8)
TRUETYPE_SFNT_VERSION = "\x00\x01\x00\x00"
VARIABLE_OR_CFF_TABLES = {
	"fvar", "gvar", "avar", "HVAR", "VVAR", "MVAR", "cvar", "CFF ", "CFF2"
}


def catalog_codepoints():
	"""Return catalog characters plus dynamic UI ASCII and non-breaking space."""
	characters = {chr(codepoint) for codepoint in range(0x20, 0x7f)}
	characters.add("\u00a0")
	with CATALOG.open("r", encoding="utf-8", newline="") as catalog_file:
		for row in csv.reader(catalog_file, delimiter="\t"):
			for cell in row:
				characters.update(cell)
	return characters


def load_fonttools():
	try:
		import fontTools
		from fontTools.ttLib import TTFont
		from fontTools.varLib.instancer import instantiateVariableFont
	except ImportError as error:
		raise RuntimeError(
			"fonttools is required (install with: python3 -m pip install fonttools)"
		) from error
	if fontTools.__version__ != FONTTOOLS_VERSION:
		raise RuntimeError(
			f"fonttools {FONTTOOLS_VERSION} is required, got {fontTools.__version__}"
		)
	return TTFont, instantiateVariableFont


def font_family_names(font):
	names = set()
	for record in font["name"].names:
		if record.nameID == 1:
			try:
				names.add(record.toUnicode())
			except UnicodeDecodeError:
				continue
	return names


def read_png(path):
	data = path.read_bytes()
	if data[:8] != b"\x89PNG\r\n\x1a\n":
		raise ValueError("invalid PNG signature")

	offset = 8
	ihdr = None
	idat = bytearray()
	while offset < len(data):
		if offset + 12 > len(data):
			raise ValueError("truncated PNG chunk")
		length = struct.unpack(">I", data[offset:offset + 4])[0]
		chunk_type = data[offset + 4:offset + 8]
		chunk_data = data[offset + 8:offset + 8 + length]
		if len(chunk_data) != length:
			raise ValueError("truncated PNG data")
		if zlib.crc32(chunk_type + chunk_data) & 0xffffffff != struct.unpack(
			">I", data[offset + 8 + length:offset + 12 + length]
		)[0]:
			raise ValueError("invalid PNG chunk CRC")
		if chunk_type == b"IHDR":
			ihdr = struct.unpack(">IIBBBBB", chunk_data)
		elif chunk_type == b"IDAT":
			idat.extend(chunk_data)
		elif chunk_type == b"IEND":
			break
		offset += 12 + length

	if ihdr is None:
		raise ValueError("missing PNG IHDR")
	width, height, depth, color_type, compression, filtering, interlace = ihdr
	if depth != 8 or color_type not in (2, 6):
		raise ValueError("PNG must be 8-bit RGB or RGBA")
	if compression != 0 or filtering != 0 or interlace != 0:
		raise ValueError("unsupported PNG encoding")
	return width, height, color_type, zlib.decompress(idat)


def check_assets():
	missing = [path for path in (FONT, LICENSE, BACKGROUND) if not path.is_file()]
	if missing:
		raise RuntimeError("missing asset(s): " + ", ".join(str(path) for path in missing))

	TTFont, _ = load_fonttools()
	with TTFont(FONT, lazy=True) as font:
		if font.reader.sfntVersion != TRUETYPE_SFNT_VERSION:
			raise RuntimeError("font must use TrueType outlines (sfnt version 0x00010000)")
		if not {"glyf", "loca"}.issubset(font.keys()):
			raise RuntimeError("font must contain TrueType glyf and loca tables")
		disallowed_tables = sorted(VARIABLE_OR_CFF_TABLES.intersection(font.keys()))
		if disallowed_tables:
			raise RuntimeError(
				"font must be static TrueType; found table(s): "
				+ ", ".join(disallowed_tables)
			)
		codepoints = set()
		for table in font["cmap"].tables:
			codepoints.update(table.cmap)
		required = {ord(character) for character in catalog_codepoints()}
		missing_codepoints = sorted(required - codepoints)
		if missing_codepoints:
			values = ", ".join(f"U+{codepoint:04X}" for codepoint in missing_codepoints)
			raise RuntimeError(f"font is missing required code points: {values}")
		if font_family_names(font) != {FAMILY_NAME}:
			raise RuntimeError(
				f"font family must be {FAMILY_NAME!r}, got {sorted(font_family_names(font))}"
			)

	if "SIL OPEN FONT LICENSE Version 1.1" not in LICENSE.read_text(encoding="utf-8"):
		raise RuntimeError("skin/OFL.txt is not the SIL Open Font License Version 1.1")

	width, height, color_type, pixels = read_png(BACKGROUND)
	channels = 3 if color_type == 2 else 4
	row_size = width * channels
	if len(pixels) != height * (row_size + 1):
		raise RuntimeError("background PNG has an unexpected decoded size")
	for row_index in range(height):
		row = pixels[row_index * (row_size + 1):(row_index + 1) * (row_size + 1)]
		if row[0] != 0:
			raise RuntimeError("background PNG must use unfiltered scanlines")
		for offset in range(1, len(row), channels):
			if tuple(row[offset:offset + 3]) != BACKGROUND_RGB:
				raise RuntimeError(f"background PNG must be solid RGB {BACKGROUND_RGB}")

	print(
		f"assets OK: {len(required)} code points, "
		f"{width}x{height} {'RGBA' if color_type == 6 else 'RGB'} background"
	)


def png_chunk(chunk_type, data):
	return (
		struct.pack(">I", len(data))
		+ chunk_type
		+ data
		+ struct.pack(">I", zlib.crc32(chunk_type + data) & 0xffffffff)
	)


def write_background():
	width, height = PNG_SIZE
	row = b"\0" + bytes(BACKGROUND_RGB) * width
	data = (
		b"\x89PNG\r\n\x1a\n"
		+ png_chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
		+ png_chunk(b"IDAT", zlib.compress(row * height, level=9))
		+ png_chunk(b"IEND", b"")
	)
	BACKGROUND.write_bytes(data)


def rename_font():
	TTFont, _ = load_fonttools()
	with TTFont(FONT, recalcTimestamp=False) as font:
		for record in font["name"].names:
			if record.nameID in (1, 4, 6):
				value = FAMILY_NAME.replace(" ", "") if record.nameID == 6 else FAMILY_NAME
				record.string = value.encode(
					"utf-16-be" if record.isUnicode() else "ascii"
				)
		font["head"].modified = font["head"].created
		font.save(FONT)


def build_assets(source, license_path):
	if not source.is_file():
		raise RuntimeError(f"font source does not exist: {source}")
	source_sha256 = hashlib.sha256(source.read_bytes()).hexdigest()
	if source_sha256 != SOURCE_SHA256:
		raise RuntimeError(
			f"font source SHA-256 must be {SOURCE_SHA256}, got {source_sha256}"
		)
	if license_path is None:
		license_path = source.with_name("LICENSE")
	if not license_path.is_file():
		raise RuntimeError(
			f"license does not exist: {license_path} (pass it with --license)"
		)
	TTFont, instantiate_variable_font = load_fonttools()

	SKIN_DIR.mkdir(parents=True, exist_ok=True)
	GLYPH_LIST.parent.mkdir(parents=True, exist_ok=True)
	GLYPH_LIST.write_text(
		"".join(sorted(catalog_codepoints(), key=ord)),
		encoding="utf-8",
	)
	subset_source = source
	with TTFont(source) as source_font:
		if "fvar" in source_font:
			regular_source = GLYPH_LIST.with_name("ui-font-regular.ttf")
			instantiate_variable_font(source_font, {"wght": 400}, inplace=True)
			source_font.save(regular_source)
			subset_source = regular_source
	subprocess.run(
		[
			sys.executable,
			"-m",
			"fontTools.subset",
			str(subset_source),
			f"--text-file={GLYPH_LIST}",
			f"--output-file={FONT}",
			"--layout-features=*",
			"--name-IDs=*",
			"--name-languages=*",
			"--notdef-glyph",
			"--recommended-glyphs",
		],
		check=True,
	)
	rename_font()
	shutil.copyfile(license_path, LICENSE)
	write_background()
	check_assets()


def parse_args():
	parser = argparse.ArgumentParser(description=__doc__)
	mode = parser.add_mutually_exclusive_group(required=True)
	mode.add_argument("--check", action="store_true", help="verify bundled assets")
	mode.add_argument("--source", type=Path, help="build from an upstream Noto font")
	parser.add_argument(
		"--license",
		dest="license_path",
		type=Path,
		help="upstream OFL file (default: LICENSE beside --source)",
	)
	return parser.parse_args()


def main():
	args = parse_args()
	try:
		if args.check:
			check_assets()
		else:
			build_assets(args.source.resolve(), args.license_path)
	except (OSError, RuntimeError, ValueError, subprocess.CalledProcessError) as error:
		print(f"error: {error}", file=sys.stderr)
		return 1
	return 0


if __name__ == "__main__":
	sys.exit(main())
