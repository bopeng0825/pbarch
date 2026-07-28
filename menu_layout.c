#include "menu_layout.h"

#include <string.h>

#define REPLACEMENT_CODEPOINT 0xfffd

static size_t decode_utf8(const unsigned char *text, unsigned int *codepoint)
{
	unsigned int value;

	if (text[0] < 0x80) {
		*codepoint = text[0];
		return 1;
	}

	if (text[0] >= 0xc2 && text[0] <= 0xdf &&
	    text[1] >= 0x80 && text[1] <= 0xbf) {
		*codepoint = ((unsigned int)(text[0] & 0x1f) << 6) |
			(text[1] & 0x3f);
		return 2;
	}

	if (text[0] >= 0xe0 && text[0] <= 0xef && text[1] != '\0' &&
	    text[2] != '\0' && text[1] >= 0x80 && text[1] <= 0xbf &&
	    text[2] >= 0x80 && text[2] <= 0xbf &&
	    (text[0] != 0xe0 || text[1] >= 0xa0) &&
	    (text[0] != 0xed || text[1] <= 0x9f)) {
		value = ((unsigned int)(text[0] & 0x0f) << 12) |
			((unsigned int)(text[1] & 0x3f) << 6) |
			(text[2] & 0x3f);
		*codepoint = value;
		return 3;
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
		*codepoint = value;
		return 4;
	}

	*codepoint = REPLACEMENT_CODEPOINT;
	return 1;
}

static size_t codepoint_cells(unsigned int codepoint)
{
	if ((codepoint >= 0x3000 && codepoint <= 0x303f) ||
	    (codepoint >= 0x3040 && codepoint <= 0x30ff) ||
	    (codepoint >= 0x31f0 && codepoint <= 0x31ff) ||
	    (codepoint >= 0x3400 && codepoint <= 0x4dbf) ||
	    (codepoint >= 0x4e00 && codepoint <= 0x9fff) ||
	    (codepoint >= 0xac00 && codepoint <= 0xd7af) ||
	    (codepoint >= 0xf900 && codepoint <= 0xfaff) ||
	    (codepoint >= 0xff00 && codepoint <= 0xffef) ||
	    (codepoint >= 0x20000 && codepoint <= 0x2ffff) ||
	    (codepoint >= 0x30000 && codepoint <= 0x323af))
		return 2;

	return 1;
}

int menu_main_font_px(int menu_height)
{
	int pixels = (menu_height + 12) / 24;

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
		unsigned int codepoint;

		cursor += decode_utf8(cursor, &codepoint);
		cells += codepoint_cells(codepoint);
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
		unsigned int codepoint;
		size_t byte_count = decode_utf8(cursor, &codepoint);
		size_t cell_count = codepoint_cells(codepoint);

		if (cells + cell_count > max_cells)
			break;
		if (codepoint == REPLACEMENT_CODEPOINT) {
			bytes = replacement;
			byte_count = sizeof(replacement);
		}
		if (byte_count >= dst_size - written)
			break;

		memcpy(dst + written, bytes, byte_count);
		written += byte_count;
		cells += cell_count;
		cursor += codepoint == REPLACEMENT_CODEPOINT ? 1 : byte_count;
	}
	dst[written] = '\0';
	return cells;
}
