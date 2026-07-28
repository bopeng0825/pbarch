#include "menu_layout.h"

#include <string.h>

#define REPLACEMENT_CODEPOINT 0xfffd

struct utf8_decoded {
	unsigned int codepoint;
	size_t bytes;
	int valid;
};

static struct utf8_decoded decode_utf8(const unsigned char *text)
{
	struct utf8_decoded decoded = { REPLACEMENT_CODEPOINT, 1, 0 };
	unsigned int value;

	if (text[0] < 0x80) {
		decoded.codepoint = text[0];
		decoded.valid = 1;
		return decoded;
	}

	if (text[0] >= 0xc2 && text[0] <= 0xdf &&
	    text[1] >= 0x80 && text[1] <= 0xbf) {
		decoded.codepoint = ((unsigned int)(text[0] & 0x1f) << 6) |
			(text[1] & 0x3f);
		decoded.bytes = 2;
		decoded.valid = 1;
		return decoded;
	}

	if (text[0] >= 0xe0 && text[0] <= 0xef && text[1] != '\0' &&
	    text[2] != '\0' && text[1] >= 0x80 && text[1] <= 0xbf &&
	    text[2] >= 0x80 && text[2] <= 0xbf &&
	    (text[0] != 0xe0 || text[1] >= 0xa0) &&
	    (text[0] != 0xed || text[1] <= 0x9f)) {
		value = ((unsigned int)(text[0] & 0x0f) << 12) |
			((unsigned int)(text[1] & 0x3f) << 6) |
			(text[2] & 0x3f);
		decoded.codepoint = value;
		decoded.bytes = 3;
		decoded.valid = 1;
		return decoded;
	}

	if (text[0] >= 0xf0 && text[0] <= 0xf4 && text[1] != '\0' &&
	    text[2] != '\0' && text[3] != '\0' &&
	    text[1] >= 0x80 && text[1] <= 0xbf &&
	    text[2] >= 0x80 && text[2] <= 0xbf &&
	    text[3] >= 0x80 && text[3] <= 0xbf &&
	    (text[0] != 0xf0 || text[1] >= 0x90) &&
	    (text[0] != 0xf4 || text[1] <= 0x8f)) {
		value = ((unsigned int)(text[0] & 0x07) << 18) |
			((unsigned int)(text[1] & 0x3f) << 12) |
			((unsigned int)(text[2] & 0x3f) << 6) |
			(text[3] & 0x3f);
		decoded.codepoint = value;
		decoded.bytes = 4;
		decoded.valid = 1;
		return decoded;
	}

	return decoded;
}

static size_t codepoint_cells(unsigned int codepoint)
{
	if ((codepoint >= 0x3000 && codepoint <= 0x303f) ||
	    (codepoint >= 0x3040 && codepoint <= 0x30ff) ||
	    (codepoint >= 0x1100 && codepoint <= 0x11ff) ||
	    (codepoint >= 0x3130 && codepoint <= 0x318f) ||
	    (codepoint >= 0x31f0 && codepoint <= 0x31ff) ||
	    (codepoint >= 0x3400 && codepoint <= 0x4dbf) ||
	    (codepoint >= 0x4e00 && codepoint <= 0x9fff) ||
	    (codepoint >= 0xa960 && codepoint <= 0xa97f) ||
	    (codepoint >= 0xac00 && codepoint <= 0xd7a3) ||
	    (codepoint >= 0xd7b0 && codepoint <= 0xd7ff) ||
	    (codepoint >= 0xf900 && codepoint <= 0xfaff) ||
	    (codepoint >= 0xff01 && codepoint <= 0xff60) ||
	    (codepoint >= 0xffe0 && codepoint <= 0xffe6) ||
	    (codepoint >= 0x20000 && codepoint <= 0x2ffff) ||
	    (codepoint >= 0x30000 && codepoint <= 0x323af))
		return 2;

	return 1;
}

int menu_main_font_px(int menu_height)
{
	int pixels;

	if (menu_height <= 0)
		return 12;
	pixels = menu_height / 24;
	if (menu_height % 24 >= 12)
		pixels++;

	if (pixels < 12)
		pixels = 12;
	if (pixels > 32)
		pixels = 32;
	return pixels;
}

int menu_small_font_px(int main_px)
{
	return (main_px * 4 + 2) / 5;
}

size_t menu_utf8_cells(const char *text)
{
	const unsigned char *cursor = (const unsigned char *)text;
	size_t cells = 0;

	if (text == NULL)
		return 0;

	while (*cursor != '\0') {
		struct utf8_decoded decoded = decode_utf8(cursor);

		cursor += decoded.bytes;
		cells += codepoint_cells(decoded.codepoint);
	}
	return cells;
}

size_t menu_utf8_truncate_cells(const char *src, size_t max_cells,
				char *dst, size_t dst_size)
{
	const unsigned char replacement[] = { 0xef, 0xbf, 0xbd };
	const unsigned char *cursor = (const unsigned char *)src;
	size_t cells = 0;
	size_t written = 0;

	if (dst_size == 0)
		return 0;

	dst[0] = '\0';
	if (src == NULL)
		return 0;

	while (*cursor != '\0') {
		const unsigned char *bytes = cursor;
		struct utf8_decoded decoded = decode_utf8(cursor);
		size_t byte_count = decoded.bytes;
		size_t cell_count = codepoint_cells(decoded.codepoint);

		if (cells + cell_count > max_cells)
			break;
		if (!decoded.valid) {
			bytes = replacement;
			byte_count = sizeof(replacement);
		}
		if (byte_count >= dst_size - written)
			break;

		memcpy(dst + written, bytes, byte_count);
		written += byte_count;
		cells += cell_count;
		cursor += decoded.bytes;
	}
	dst[written] = '\0';
	return cells;
}

size_t menu_utf8_fit_width(const char *src, int max_width,
			   char *dst, size_t dst_size,
			   menu_utf8_width_fn width, void *opaque)
{
	size_t cells;
	char *newline;

	if (dst_size == 0)
		return 0;
	cells = menu_utf8_truncate_cells(src, (size_t)-1, dst, dst_size);
	newline = strchr(dst, '\n');
	if (newline != NULL) {
		*newline = '\0';
		cells = menu_utf8_cells(dst);
	}
	if (max_width < 0)
		max_width = 0;
	if (width == NULL)
		return cells;

	while (cells > 0 && width(dst, opaque) > max_width) {
		cells = menu_utf8_truncate_cells(src, cells - 1,
						dst, dst_size);
	}
	return cells;
}
