#include <SDL/SDL.h>
#include <linux/input.h>
#include "libretro.h"
#include "libpicofe/plat.h"
#include "libpicofe/input.h"
#include "libpicofe/in_sdl.h"
#include "main.h"
#include "util.h"

#define MAX_SAMPLE_RATE 48000

static const struct in_default_bind in_sdl_defbinds[] = {
    { BTN_DPAD_UP,      IN_BINDTYPE_PLAYER12, RETRO_DEVICE_ID_JOYPAD_UP },
    { BTN_DPAD_DOWN,    IN_BINDTYPE_PLAYER12, RETRO_DEVICE_ID_JOYPAD_DOWN },
    { BTN_DPAD_LEFT,    IN_BINDTYPE_PLAYER12, RETRO_DEVICE_ID_JOYPAD_LEFT },
    { BTN_DPAD_RIGHT,   IN_BINDTYPE_PLAYER12, RETRO_DEVICE_ID_JOYPAD_RIGHT },

    { BTN_SOUTH,   IN_BINDTYPE_PLAYER12, RETRO_DEVICE_ID_JOYPAD_B },
    { BTN_EAST,    IN_BINDTYPE_PLAYER12, RETRO_DEVICE_ID_JOYPAD_A },
    { BTN_WEST,    IN_BINDTYPE_PLAYER12, RETRO_DEVICE_ID_JOYPAD_Y },
    { BTN_NORTH,   IN_BINDTYPE_PLAYER12, RETRO_DEVICE_ID_JOYPAD_X },

    { BTN_TL,      IN_BINDTYPE_PLAYER12, RETRO_DEVICE_ID_JOYPAD_L },
    { BTN_TR,      IN_BINDTYPE_PLAYER12, RETRO_DEVICE_ID_JOYPAD_R },

    { BTN_START,   IN_BINDTYPE_PLAYER12, RETRO_DEVICE_ID_JOYPAD_START },
    { BTN_SELECT,  IN_BINDTYPE_PLAYER12, RETRO_DEVICE_ID_JOYPAD_SELECT },

    { BTN_MODE,    IN_BINDTYPE_EMU, EACTION_MENU },
    { 0, 0, 0 }
};

const struct menu_keymap in_sdl_key_map[] = {
    { BTN_DPAD_UP,      PBTN_UP },
    { BTN_DPAD_DOWN,    PBTN_DOWN },
    { BTN_DPAD_LEFT,    PBTN_LEFT },
    { BTN_DPAD_RIGHT,   PBTN_RIGHT },

    { BTN_SOUTH,   PBTN_MOK },
    { BTN_EAST,    PBTN_MBACK },
    { BTN_NORTH,   PBTN_MA2 },
    { BTN_WEST,    PBTN_MA3 },

    { BTN_TL,      PBTN_L },
    { BTN_TR,      PBTN_R },

    { BTN_MODE,    PBTN_MENU },
};

const struct menu_keymap in_sdl_joy_map[] =
{
    { BTN_DPAD_UP,      PBTN_UP },
    { BTN_DPAD_DOWN,    PBTN_DOWN },
    { BTN_DPAD_LEFT,    PBTN_LEFT },
    { BTN_DPAD_RIGHT,   PBTN_RIGHT },

    { BTN_SOUTH,   PBTN_MOK },
    { BTN_EAST,    PBTN_MBACK },
    { BTN_NORTH,   PBTN_MA2 },
    { BTN_WEST,    PBTN_MA3 },
};

static const struct mod_keymap in_sdl_mod_keymap[] = {
    { SDLK_TAB,       SDLK_q },
    { SDLK_BACKSPACE, SDLK_BACKSLASH },
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

#include "plat_sdl.c"
