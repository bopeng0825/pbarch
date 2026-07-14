#ifndef PLAT_H150101_SDL2_INPUT_H
#define PLAT_H150101_SDL2_INPUT_H

#include <SDL/SDL.h>
#include "libpicofe/input.h"

#define H150101_SDL2_BUTTON_COUNT 32
#define H150101_SDL2_AXIS_COUNT 4

#define H150101_SDL2_BUTTON(button) (SDL_NUM_SCANCODES + (button))
#define H150101_SDL2_AXIS_NEG(axis) (SDL_NUM_SCANCODES + H150101_SDL2_BUTTON_COUNT + (axis) * 2)
#define H150101_SDL2_AXIS_POS(axis) (H150101_SDL2_AXIS_NEG(axis) + 1)
#define H150101_SDL2_KEY_COUNT (SDL_NUM_SCANCODES + H150101_SDL2_BUTTON_COUNT + H150101_SDL2_AXIS_COUNT * 2)

int in_h150101_sdl2_init(const struct in_pdata *pdata, void (*handler)(void *event));

#endif
