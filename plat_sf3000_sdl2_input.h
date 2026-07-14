#ifndef PLAT_SF3000_SDL2_INPUT_H
#define PLAT_SF3000_SDL2_INPUT_H

#include <SDL/SDL.h>
#include "libpicofe/input.h"

#define SF3000_SDL2_BUTTON_COUNT 32
#define SF3000_SDL2_AXIS_COUNT 4

#define SF3000_SDL2_BUTTON(button) (SDL_NUM_SCANCODES + (button))
#define SF3000_SDL2_AXIS_NEG(axis) (SDL_NUM_SCANCODES + SF3000_SDL2_BUTTON_COUNT + (axis) * 2)
#define SF3000_SDL2_AXIS_POS(axis) (SF3000_SDL2_AXIS_NEG(axis) + 1)
#define SF3000_SDL2_KEY_COUNT (SDL_NUM_SCANCODES + SF3000_SDL2_BUTTON_COUNT + SF3000_SDL2_AXIS_COUNT * 2)

int in_sf3000_sdl2_init(const struct in_pdata *pdata, void (*handler)(void *event));

#endif
