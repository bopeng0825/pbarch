#include <SDL/SDL.h>
#include <linux/input.h>
#include "libretro.h"
#include "libpicofe/plat.h"
#include "libpicofe/input.h"
#include "libpicofe/in_sdl.h"
#include "libpicofe/linux/in_evdev.h"
#include "main.h"
#include "util.h"

#define MAX_SAMPLE_RATE 48000

static const struct in_default_bind in_sdl_defbinds[] = {
    { SDLK_UP,                  IN_BINDTYPE_PLAYER12, RETRO_DEVICE_ID_JOYPAD_UP },
    { SDLK_DOWN,                IN_BINDTYPE_PLAYER12, RETRO_DEVICE_ID_JOYPAD_DOWN },
    { SDLK_LEFT,                IN_BINDTYPE_PLAYER12, RETRO_DEVICE_ID_JOYPAD_LEFT },
    { SDLK_RIGHT,               IN_BINDTYPE_PLAYER12, RETRO_DEVICE_ID_JOYPAD_RIGHT },

    { SDL_JOY_BUTTON(0),        IN_BINDTYPE_PLAYER12, RETRO_DEVICE_ID_JOYPAD_B },
    { SDL_JOY_BUTTON(1),        IN_BINDTYPE_PLAYER12, RETRO_DEVICE_ID_JOYPAD_A },
    { SDL_JOY_BUTTON(2),        IN_BINDTYPE_PLAYER12, RETRO_DEVICE_ID_JOYPAD_Y },
    { SDL_JOY_BUTTON(3),        IN_BINDTYPE_PLAYER12, RETRO_DEVICE_ID_JOYPAD_X },

    { SDL_JOY_BUTTON(4),        IN_BINDTYPE_PLAYER12, RETRO_DEVICE_ID_JOYPAD_L },
    { SDL_JOY_BUTTON(5),        IN_BINDTYPE_PLAYER12, RETRO_DEVICE_ID_JOYPAD_R },

    { SDL_JOY_BUTTON(8),        IN_BINDTYPE_PLAYER12, RETRO_DEVICE_ID_JOYPAD_SELECT },
    { SDL_JOY_BUTTON(9),        IN_BINDTYPE_PLAYER12, RETRO_DEVICE_ID_JOYPAD_START },

    { SDL_JOY_BUTTON(10),       IN_BINDTYPE_EMU, EACTION_MENU },
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

const struct menu_keymap in_sdl_key_map[] = {
    { SDLK_UP,                  PBTN_UP },
    { SDLK_DOWN,                PBTN_DOWN },
    { SDLK_LEFT,                PBTN_LEFT },
    { SDLK_RIGHT,               PBTN_RIGHT },

    { SDLK_RETURN,              PBTN_MOK },
    { SDLK_BACKSPACE,           PBTN_MBACK },
    { SDLK_SPACE,               PBTN_MA2 },
    { SDLK_TAB,                 PBTN_MA3 },
};

const struct menu_keymap in_sdl_joy_map[] = {
    { SDLK_UP,                  PBTN_UP },
    { SDLK_DOWN,                PBTN_DOWN },
    { SDLK_LEFT,                PBTN_LEFT },
    { SDLK_RIGHT,               PBTN_RIGHT },

    { SDL_JOY_BUTTON(0),        PBTN_MOK },
    { SDL_JOY_BUTTON(1),        PBTN_MBACK },
    { SDL_JOY_BUTTON(2),        PBTN_MA2 },
    { SDL_JOY_BUTTON(3),        PBTN_MA3 },
    { SDL_JOY_BUTTON(4),        PBTN_L },
    { SDL_JOY_BUTTON(5),        PBTN_R },
    { SDL_JOY_BUTTON(10),       PBTN_MENU },
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

static const struct mod_keymap in_sdl_mod_keymap[] = {
    { SDLK_TAB,                 SDLK_q },
    { SDLK_BACKSPACE,           SDLK_BACKSLASH },
};

static const struct in_pdata in_sdl_platform_data = {
	.defbinds     = in_sdl_defbinds,
	.key_map      = in_sdl_key_map,
	.kmap_size    = array_size(in_sdl_key_map),
	.joy_map      = in_sdl_joy_map,
	.jmap_size    = array_size(in_sdl_joy_map),
	.mod_key      = SDLK_ESCAPE,
	.mod_keymap   = in_sdl_mod_keymap,
	.modmap_size  = array_size(in_sdl_mod_keymap),
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

	if (in_sdl_init(&in_sdl_platform_data, handler)) {
		PA_ERROR("SDL input failed to init: %s\n", SDL_GetError());
		return -1;
	}

	//if (in_evdev_init(&in_evdev_platform_data))
	//	PA_WARN("evdev input fallback failed to init\n");

	return 0;
}

#define in_sdl_init plat_input_init

#include "plat_sdl.c"
