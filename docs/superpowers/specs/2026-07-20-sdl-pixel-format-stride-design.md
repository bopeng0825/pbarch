# SDL Pixel Format and Stride Design

## Goal

Render libretro video correctly when a core supplies rows with padding, such as
SNES9x2005 reporting a 256-pixel RGB565 frame with a 1024-byte pitch.

## Root Cause

The SDL2 hardware-scaling path currently infers pixel format from the relation
between active width and pitch. A padded RGB565 row can therefore look like a
tightly packed XRGB8888 row. SNES9x2005's 256-pixel RGB565 output uses a
1024-byte stride, so the frontend incorrectly creates an XRGB8888 texture and
interprets pairs of 16-bit pixels as 32-bit pixels.

Pitch describes the distance between adjacent rows. It does not identify the
pixel format.

## Design

Expose the pixel format already negotiated through
`RETRO_ENVIRONMENT_SET_PIXEL_FORMAT` with a read-only
`video_get_pixel_format()` function in `video.c` and `video.h`.

The SDL2 video path will:

1. Select `SDL_PIXELFORMAT_RGB565` for `RETRO_PIXEL_FORMAT_RGB565`.
2. Select `SDL_PIXELFORMAT_XRGB8888` for
   `RETRO_PIXEL_FORMAT_XRGB8888`.
3. Permit hardware scaling when pitch is at least `width * bytes_per_pixel`.
4. Continue passing the core-provided pitch to `SDL_UpdateTexture()`, preserving
   padded rows without copying them.
5. Fall back to the existing software scaling path for unsupported formats,
   invalid dimensions, rotation, or an undersized pitch.

No core-specific SNES workaround will be added.

## Compatibility

Tightly packed RGB565 and XRGB8888 frames retain their existing accelerated
path. Padded RGB565 frames become eligible for the same path without being
misidentified. Existing non-SDL2 conversion behavior remains unchanged.

## Validation

Add a focused test for SDL format selection and pitch validation covering:

- 256-pixel RGB565 with a 1024-byte pitch selects RGB565.
- Tightly packed RGB565 selects RGB565.
- Tightly packed XRGB8888 selects XRGB8888.
- A pitch smaller than the active row is rejected.

Then build the Unix SDL2 target available in this repository. Device validation
should confirm that SNES9x2005 logs `format=RGB565` for the reported
`256x224 pitch=1024` frame and displays the complete image.
