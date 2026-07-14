#include <SDL/SDL.h>
#include <linux/input.h>
#include "libretro.h"
#include "libpicofe/plat.h"
#include "libpicofe/input.h"
#include "libpicofe/linux/in_evdev.h"
#include "main.h"
#include "plat_h150101_sdl2_input.h"
#include "util.h"

#define MAX_SAMPLE_RATE 48000

static const struct in_default_bind in_h150101_sdl2_defbinds[] = {
    { H150101_SDL2_AXIS_NEG(1),  IN_BINDTYPE_PLAYER12, RETRO_DEVICE_ID_JOYPAD_UP },
    { H150101_SDL2_AXIS_POS(1),  IN_BINDTYPE_PLAYER12, RETRO_DEVICE_ID_JOYPAD_DOWN },
    { H150101_SDL2_AXIS_NEG(0),  IN_BINDTYPE_PLAYER12, RETRO_DEVICE_ID_JOYPAD_LEFT },
    { H150101_SDL2_AXIS_POS(0),  IN_BINDTYPE_PLAYER12, RETRO_DEVICE_ID_JOYPAD_RIGHT },
    { H150101_SDL2_AXIS_NEG(3),  IN_BINDTYPE_PLAYER12, RETRO_DEVICE_ID_JOYPAD_UP },
    { H150101_SDL2_AXIS_POS(3),  IN_BINDTYPE_PLAYER12, RETRO_DEVICE_ID_JOYPAD_DOWN },
    { H150101_SDL2_AXIS_NEG(2),  IN_BINDTYPE_PLAYER12, RETRO_DEVICE_ID_JOYPAD_LEFT },
    { H150101_SDL2_AXIS_POS(2),  IN_BINDTYPE_PLAYER12, RETRO_DEVICE_ID_JOYPAD_RIGHT },

    { H150101_SDL2_BUTTON(0),    IN_BINDTYPE_PLAYER12, RETRO_DEVICE_ID_JOYPAD_B },
    { H150101_SDL2_BUTTON(1),    IN_BINDTYPE_PLAYER12, RETRO_DEVICE_ID_JOYPAD_A },
    { H150101_SDL2_BUTTON(2),    IN_BINDTYPE_PLAYER12, RETRO_DEVICE_ID_JOYPAD_Y },
    { H150101_SDL2_BUTTON(3),    IN_BINDTYPE_PLAYER12, RETRO_DEVICE_ID_JOYPAD_X },

    { H150101_SDL2_BUTTON(4),    IN_BINDTYPE_PLAYER12, RETRO_DEVICE_ID_JOYPAD_L },
    { H150101_SDL2_BUTTON(5),    IN_BINDTYPE_PLAYER12, RETRO_DEVICE_ID_JOYPAD_R },

    { H150101_SDL2_BUTTON(8),    IN_BINDTYPE_PLAYER12, RETRO_DEVICE_ID_JOYPAD_SELECT },
    { H150101_SDL2_BUTTON(9),    IN_BINDTYPE_PLAYER12, RETRO_DEVICE_ID_JOYPAD_START },

    { H150101_SDL2_BUTTON(10),   IN_BINDTYPE_EMU, EACTION_MENU },
    { 0, 0, 0 }
};

static const struct in_default_bind in_evdev_defbinds[] = {
    { BTN_DPAD_UP,              IN_BINDTYPE_PLAYER12, RETRO_DEVICE_ID_JOYPAD_UP },
    { BTN_DPAD_DOWN,            IN_BINDTYPE_PLAYER12, RETRO_DEVICE_ID_JOYPAD_DOWN },
    { BTN_DPAD_LEFT,            IN_BINDTYPE_PLAYER12, RETRO_DEVICE_ID_JOYPAD_LEFT },
    { BTN_DPAD_RIGHT,           IN_BINDTYPE_PLAYER12, RETRO_DEVICE_ID_JOYPAD_RIGHT },

    { BTN_SOUTH,                IN_BINDTYPE_PLAYER12, RETRO_DEVICE_ID_JOYPAD_B },
    { BTN_EAST,                 IN_BINDTYPE_PLAYER12, RETRO_DEVICE_ID_JOYPAD_A },
    { BTN_WEST,                 IN_BINDTYPE_PLAYER12, RETRO_DEVICE_ID_JOYPAD_Y },
    { BTN_NORTH,                IN_BINDTYPE_PLAYER12, RETRO_DEVICE_ID_JOYPAD_X },

    { BTN_TL,                   IN_BINDTYPE_PLAYER12, RETRO_DEVICE_ID_JOYPAD_L },
    { BTN_TR,                   IN_BINDTYPE_PLAYER12, RETRO_DEVICE_ID_JOYPAD_R },

    { BTN_START,                IN_BINDTYPE_PLAYER12, RETRO_DEVICE_ID_JOYPAD_START },
    { BTN_SELECT,               IN_BINDTYPE_PLAYER12, RETRO_DEVICE_ID_JOYPAD_SELECT },

    { BTN_MODE,                 IN_BINDTYPE_EMU, EACTION_MENU },
    { 0, 0, 0 }
};

const struct menu_keymap in_h150101_sdl2_joy_map[] = {
    { H150101_SDL2_AXIS_NEG(1),  PBTN_UP },
    { H150101_SDL2_AXIS_POS(1),  PBTN_DOWN },
    { H150101_SDL2_AXIS_NEG(0),  PBTN_LEFT },
    { H150101_SDL2_AXIS_POS(0),  PBTN_RIGHT },
    { H150101_SDL2_AXIS_NEG(3),  PBTN_UP },
    { H150101_SDL2_AXIS_POS(3),  PBTN_DOWN },
    { H150101_SDL2_AXIS_NEG(2),  PBTN_LEFT },
    { H150101_SDL2_AXIS_POS(2),  PBTN_RIGHT },

    { H150101_SDL2_BUTTON(0),    PBTN_MOK },
    { H150101_SDL2_BUTTON(1),    PBTN_MBACK },
    { H150101_SDL2_BUTTON(2),    PBTN_MA2 },
    { H150101_SDL2_BUTTON(3),    PBTN_MA3 },
    { H150101_SDL2_BUTTON(4),    PBTN_L },
    { H150101_SDL2_BUTTON(5),    PBTN_R },
    { H150101_SDL2_BUTTON(10),   PBTN_MENU },
};

const struct menu_keymap in_evdev_key_map[] = {
    { BTN_DPAD_UP,              PBTN_UP },
    { BTN_DPAD_DOWN,            PBTN_DOWN },
    { BTN_DPAD_LEFT,            PBTN_LEFT },
    { BTN_DPAD_RIGHT,           PBTN_RIGHT },

    { BTN_SOUTH,                PBTN_MOK },
    { BTN_EAST,                 PBTN_MBACK },
    { BTN_NORTH,                PBTN_MA2 },
    { BTN_WEST,                 PBTN_MA3 },

    { BTN_TL,                   PBTN_L },
    { BTN_TR,                   PBTN_R },

    { BTN_MODE,                 PBTN_MENU },
};

static const struct in_pdata in_sdl_platform_data = {
	.defbinds     = in_h150101_sdl2_defbinds,
	.joy_map      = in_h150101_sdl2_joy_map,
	.jmap_size    = array_size(in_h150101_sdl2_joy_map),
};

static const struct in_pdata in_evdev_platform_data = {
	.defbinds     = in_evdev_defbinds,
	.key_map      = in_evdev_key_map,
	.kmap_size    = array_size(in_evdev_key_map),
	.joy_map      = in_evdev_key_map,
	.jmap_size    = array_size(in_evdev_key_map),
	.mod_key      = BTN_MODE,
};

static int plat_input_init(const struct in_pdata *pdata, void (*handler)(void *event))
{
	(void)pdata;

	if (in_h150101_sdl2_init(&in_sdl_platform_data, handler)) {
		PA_ERROR("H150101 SDL2 input failed to init: %s\n", SDL_GetError());
		return -1;
	}

	//if (in_evdev_init(&in_evdev_platform_data))
	//	PA_WARN("evdev input fallback failed to init\n");

	return 0;
}

#define in_sdl_init plat_input_init

#include "plat_sdl.c"
