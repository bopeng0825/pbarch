#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <SDL/SDL.h>
#include "libpicofe/input.h"
#include "plat_sf3000_sdl2_input.h"

#define IN_SF3000_SDL2_PREFIX "sf3000-sdl2:"
#define AXIS_DEADZONE 16384

struct sf3000_sdl2_state {
	const in_drv_t *drv;
	const struct in_pdata *pdata;
	SDL_Joystick *joy;
	SDL_JoystickID joy_id;
	int joy_index;
	uint8_t keys[SF3000_SDL2_KEY_COUNT];
	void (*event_handler)(void *event);
};

struct sf3000_sdl2_pdata {
	const struct in_pdata *pdata;
	void (*handler)(void *event);
};

static struct sf3000_sdl2_pdata sf3000_sdl2_pdata;

static const char * const sf3000_sdl2_key_names[SF3000_SDL2_KEY_COUNT] = {
	[SF3000_SDL2_BUTTON(0)] = "joy 0",
	[SF3000_SDL2_BUTTON(1)] = "joy 1",
	[SF3000_SDL2_BUTTON(2)] = "joy 2",
	[SF3000_SDL2_BUTTON(3)] = "joy 3",
	[SF3000_SDL2_BUTTON(4)] = "joy 4",
	[SF3000_SDL2_BUTTON(5)] = "joy 5",
	[SF3000_SDL2_BUTTON(6)] = "joy 6",
	[SF3000_SDL2_BUTTON(7)] = "joy 7",
	[SF3000_SDL2_BUTTON(8)] = "joy 8",
	[SF3000_SDL2_BUTTON(9)] = "joy 9",
	[SF3000_SDL2_BUTTON(10)] = "joy 10",
	[SF3000_SDL2_BUTTON(11)] = "joy 11",
	[SF3000_SDL2_BUTTON(12)] = "joy 12",
	[SF3000_SDL2_BUTTON(13)] = "joy 13",
	[SF3000_SDL2_BUTTON(14)] = "joy 14",
	[SF3000_SDL2_BUTTON(15)] = "joy 15",
	[SF3000_SDL2_BUTTON(16)] = "joy 16",
	[SF3000_SDL2_BUTTON(17)] = "joy 17",
	[SF3000_SDL2_BUTTON(18)] = "joy 18",
	[SF3000_SDL2_BUTTON(19)] = "joy 19",
	[SF3000_SDL2_BUTTON(20)] = "joy 20",
	[SF3000_SDL2_BUTTON(21)] = "joy 21",
	[SF3000_SDL2_BUTTON(22)] = "joy 22",
	[SF3000_SDL2_BUTTON(23)] = "joy 23",
	[SF3000_SDL2_BUTTON(24)] = "joy 24",
	[SF3000_SDL2_BUTTON(25)] = "joy 25",
	[SF3000_SDL2_BUTTON(26)] = "joy 26",
	[SF3000_SDL2_BUTTON(27)] = "joy 27",
	[SF3000_SDL2_BUTTON(28)] = "joy 28",
	[SF3000_SDL2_BUTTON(29)] = "joy 29",
	[SF3000_SDL2_BUTTON(30)] = "joy 30",
	[SF3000_SDL2_BUTTON(31)] = "joy 31",
	[SF3000_SDL2_AXIS_NEG(0)] = "axis 0-",
	[SF3000_SDL2_AXIS_POS(0)] = "axis 0+",
	[SF3000_SDL2_AXIS_NEG(1)] = "axis 1-",
	[SF3000_SDL2_AXIS_POS(1)] = "axis 1+",
	[SF3000_SDL2_AXIS_NEG(2)] = "axis 2-",
	[SF3000_SDL2_AXIS_POS(2)] = "axis 2+",
	[SF3000_SDL2_AXIS_NEG(3)] = "axis 3-",
	[SF3000_SDL2_AXIS_POS(3)] = "axis 3+",
};

static int is_joy_event(Uint32 type)
{
	return type == SDL_JOYAXISMOTION ||
	       type == SDL_JOYBALLMOTION ||
	       type == SDL_JOYHATMOTION ||
	       type == SDL_JOYBUTTONDOWN ||
	       type == SDL_JOYBUTTONUP ||
	       type == SDL_JOYDEVICEADDED ||
	       type == SDL_JOYDEVICEREMOVED;
}

static int event_matches_joy(struct sf3000_sdl2_state *state, SDL_Event *event)
{
	SDL_JoystickID which;

	switch (event->type) {
	case SDL_JOYAXISMOTION:
		which = event->jaxis.which;
		break;
	case SDL_JOYBUTTONDOWN:
	case SDL_JOYBUTTONUP:
		which = event->jbutton.which;
		break;
	case SDL_JOYHATMOTION:
		which = event->jhat.which;
		break;
	default:
		return 0;
	}

	return which == state->joy_id || which == state->joy_index;
}

static void set_key(struct sf3000_sdl2_state *state, int key, int down)
{
	if ((unsigned int)key >= SF3000_SDL2_KEY_COUNT)
		return;

	state->keys[key] = down ? 1 : 0;
}

static int axis_key(int axis, int positive)
{
	if ((unsigned int)axis >= SF3000_SDL2_AXIS_COUNT)
		return -1;
	return positive ? SF3000_SDL2_AXIS_POS(axis) : SF3000_SDL2_AXIS_NEG(axis);
}

static void handle_axis(struct sf3000_sdl2_state *state, int axis, int value)
{
	int neg = axis_key(axis, 0);
	int pos = axis_key(axis, 1);

	if (neg < 0 || pos < 0)
		return;

	set_key(state, neg, value < -AXIS_DEADZONE);
	set_key(state, pos, value > AXIS_DEADZONE);
}

static void handle_hat(struct sf3000_sdl2_state *state, uint8_t value)
{
	set_key(state, SF3000_SDL2_AXIS_NEG(0), value & SDL_HAT_LEFT);
	set_key(state, SF3000_SDL2_AXIS_POS(0), value & SDL_HAT_RIGHT);
	set_key(state, SF3000_SDL2_AXIS_NEG(1), value & SDL_HAT_UP);
	set_key(state, SF3000_SDL2_AXIS_POS(1), value & SDL_HAT_DOWN);
}

static int handle_event(struct sf3000_sdl2_state *state, SDL_Event *event)
{
	switch (event->type) {
	case SDL_JOYAXISMOTION:
		if (!event_matches_joy(state, event))
			return 0;
		handle_axis(state, event->jaxis.axis, event->jaxis.value);
		return 1;
	case SDL_JOYBUTTONDOWN:
	case SDL_JOYBUTTONUP:
		if (!event_matches_joy(state, event))
			return 0;
		if (event->jbutton.button >= SF3000_SDL2_BUTTON_COUNT)
			return 1;
		set_key(state, SF3000_SDL2_BUTTON(event->jbutton.button),
			event->jbutton.state == SDL_PRESSED);
		return 1;
	case SDL_JOYHATMOTION:
		if (!event_matches_joy(state, event))
			return 0;
		handle_hat(state, event->jhat.value);
		return 1;
	default:
		return 0;
	}
}

static void poll_events(struct sf3000_sdl2_state *state)
{
	SDL_Event event;
	SDL_Event skipped[16];
	int i, skipped_count = 0;
	int count = 0;

	SDL_PumpEvents();

	while (count++ < 32 && SDL_PollEvent(&event)) {
		if (is_joy_event(event.type))
			handle_event(state, &event);
		else {
			if (state->event_handler)
				state->event_handler(&event);
			else if (skipped_count < (int)(sizeof(skipped) / sizeof(skipped[0])))
				skipped[skipped_count++] = event;
		}
	}

	for (i = 0; i < skipped_count; i++)
		SDL_PushEvent(&skipped[i]);
}

static void sf3000_sdl2_probe(const in_drv_t *drv)
{
	const struct sf3000_sdl2_pdata *pdata = drv->pdata;
	struct sf3000_sdl2_state *state;
	SDL_Joystick *joy;
	const char *joy_name;
	int i, joycount;
	char name[256];

	if (SDL_InitSubSystem(SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) != 0) {
		return;
	}

	SDL_JoystickEventState(SDL_ENABLE);
	joycount = SDL_NumJoysticks();

	for (i = 0; i < joycount; i++) {
		joy = SDL_JoystickOpen(i);
		if (!joy) {
			continue;
		}

		state = calloc(1, sizeof(*state));
		if (!state) {
			SDL_JoystickClose(joy);
			break;
		}

		state->drv = drv;
		state->pdata = pdata->pdata;
		state->joy = joy;
		state->joy_index = i;
		state->joy_id = SDL_JoystickInstanceID(joy);
		state->event_handler = pdata->handler;

		joy_name = SDL_JoystickNameForIndex(i);
		if (!joy_name)
			joy_name = "joystick";
		snprintf(name, sizeof(name), IN_SF3000_SDL2_PREFIX "%s", joy_name);
		in_register(name, -1, state, SF3000_SDL2_KEY_COUNT,
			sf3000_sdl2_key_names, 0);
	}
}

static void sf3000_sdl2_free(void *drv_data)
{
	struct sf3000_sdl2_state *state = drv_data;

	if (state) {
		if (state->joy)
			SDL_JoystickClose(state->joy);
		free(state);
	}
}

static const char * const *sf3000_sdl2_get_key_names(const in_drv_t *drv, int *count)
{
	(void)drv;
	*count = SF3000_SDL2_KEY_COUNT;
	return sf3000_sdl2_key_names;
}

static int sf3000_sdl2_update(void *drv_data, const int *binds, int *result)
{
	struct sf3000_sdl2_state *state = drv_data;
	int i, b;

	poll_events(state);

	for (i = 0; i < SF3000_SDL2_KEY_COUNT; i++) {
		if (!state->keys[i])
			continue;
		for (b = 0; b < IN_BINDTYPE_COUNT; b++)
			result[b] |= binds[IN_BIND_OFFS(i, b)];
	}

	return 0;
}

static int sf3000_sdl2_update_keycode(void *drv_data, int *is_down)
{
	struct sf3000_sdl2_state *state = drv_data;
	SDL_Event event;
	int key = -1;

	SDL_PumpEvents();

	while (SDL_PollEvent(&event)) {
		if (!is_joy_event(event.type)) {
			if (state->event_handler)
				state->event_handler(&event);
			continue;
		}

		switch (event.type) {
		case SDL_JOYBUTTONDOWN:
		case SDL_JOYBUTTONUP:
			if (!event_matches_joy(state, &event) ||
			    event.jbutton.button >= SF3000_SDL2_BUTTON_COUNT)
				continue;
			key = SF3000_SDL2_BUTTON(event.jbutton.button);
			if (is_down)
				*is_down = event.jbutton.state == SDL_PRESSED;
			set_key(state, key, is_down ? *is_down : event.jbutton.state == SDL_PRESSED);
			return key;
		case SDL_JOYAXISMOTION:
			if (!event_matches_joy(state, &event) ||
			    event.jaxis.axis >= SF3000_SDL2_AXIS_COUNT)
				continue;
			{
				int neg = SF3000_SDL2_AXIS_NEG(event.jaxis.axis);
				int pos = SF3000_SDL2_AXIS_POS(event.jaxis.axis);
				int old_neg = state->keys[neg];
				int old_pos = state->keys[pos];

				handle_axis(state, event.jaxis.axis, event.jaxis.value);
				if (state->keys[neg] != old_neg) {
					if (is_down)
						*is_down = state->keys[neg];
					return neg;
				}
				if (state->keys[pos] != old_pos) {
					if (is_down)
						*is_down = state->keys[pos];
					return pos;
				}
			}
			continue;
		default:
			break;
		}
	}

	return -1;
}

static int sf3000_sdl2_menu_translate(void *drv_data, int keycode, char *charcode)
{
	struct sf3000_sdl2_state *state = drv_data;
	const struct in_pdata *pdata = state->pdata;
	const struct menu_keymap *map = pdata->joy_map;
	int i;

	(void)charcode;

	if (keycode < 0) {
		keycode = -keycode;
		for (i = 0; i < (int)pdata->jmap_size; i++)
			if (map[i].pbtn == keycode)
				return map[i].key;
		return 0;
	}

	for (i = 0; i < (int)pdata->jmap_size; i++)
		if (map[i].key == keycode)
			return map[i].pbtn;
	return 0;
}

static int sf3000_sdl2_clean_binds(void *drv_data, int *binds, int *def_binds)
{
	int i, t, cnt = 0;

	(void)drv_data;
	(void)def_binds;

	for (t = 0; t < IN_BINDTYPE_COUNT; t++) {
		for (i = 0; i < SF3000_SDL2_KEY_COUNT; i++) {
			if (binds[IN_BIND_OFFS(i, t)])
				cnt++;
		}
	}

	return cnt;
}

static const in_drv_t sf3000_sdl2_drv = {
	.prefix         = IN_SF3000_SDL2_PREFIX,
	.probe          = sf3000_sdl2_probe,
	.free           = sf3000_sdl2_free,
	.get_key_names  = sf3000_sdl2_get_key_names,
	.update         = sf3000_sdl2_update,
	.update_keycode = sf3000_sdl2_update_keycode,
	.menu_translate = sf3000_sdl2_menu_translate,
	.clean_binds    = sf3000_sdl2_clean_binds,
};

int in_sf3000_sdl2_init(const struct in_pdata *pdata, void (*handler)(void *event))
{
	if (!pdata)
		return -1;

	sf3000_sdl2_pdata.pdata = pdata;
	sf3000_sdl2_pdata.handler = handler;
	in_register_driver(&sf3000_sdl2_drv, pdata->defbinds, &sf3000_sdl2_pdata);
	return 0;
}
