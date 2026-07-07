#ifndef PICOARCH_SDL_COMPAT_H
#define PICOARCH_SDL_COMPAT_H

#ifdef USE_SDL2
#include <SDL2/SDL.h>
#define SDL_PEEP_EVENTS_NATIVE SDL_PeepEvents
#ifndef SDLK_LAST
#define SDLK_LAST SDL_NUM_SCANCODES
#endif
#ifndef SDL_DEFAULT_REPEAT_DELAY
#define SDL_DEFAULT_REPEAT_DELAY 500
#endif
#ifndef SDL_DEFAULT_REPEAT_INTERVAL
#define SDL_DEFAULT_REPEAT_INTERVAL 30
#endif
#ifndef SDL_ENABLE
#define SDL_ENABLE 1
#endif
#ifndef SDL_DISABLE
#define SDL_DISABLE 0
#endif
#ifndef SDL_FULLSCREEN
#define SDL_FULLSCREEN SDL_WINDOW_FULLSCREEN
#endif
#ifndef SDL_KEYDOWNMASK
#define SDL_KEYDOWNMASK SDL_EVENTMASK(SDL_KEYDOWN)
#endif
#ifndef SDL_KEYUPMASK
#define SDL_KEYUPMASK SDL_EVENTMASK(SDL_KEYUP)
#endif
#ifndef SDL_JOYAXISMOTIONMASK
#define SDL_JOYAXISMOTIONMASK SDL_EVENTMASK(SDL_JOYAXISMOTION)
#endif
#ifndef SDL_JOYBALLMOTIONMASK
#define SDL_JOYBALLMOTIONMASK SDL_EVENTMASK(SDL_JOYBALLMOTION)
#endif
#ifndef SDL_JOYHATMOTIONMASK
#define SDL_JOYHATMOTIONMASK SDL_EVENTMASK(SDL_JOYHATMOTION)
#endif
#ifndef SDL_JOYBUTTONDOWNMASK
#define SDL_JOYBUTTONDOWNMASK SDL_EVENTMASK(SDL_JOYBUTTONDOWN)
#endif
#ifndef SDL_JOYBUTTONUPMASK
#define SDL_JOYBUTTONUPMASK SDL_EVENTMASK(SDL_JOYBUTTONUP)
#endif
#ifndef SDL_ALLEVENTS
#define SDL_ALLEVENTS 0xffffffffu
#endif

#undef SDLK_UP
#undef SDLK_DOWN
#undef SDLK_LEFT
#undef SDLK_RIGHT
#undef SDLK_LCTRL
#undef SDLK_RCTRL
#undef SDLK_LSHIFT
#undef SDLK_LALT
#undef SDLK_RETURN
#undef SDLK_BACKSPACE
#undef SDLK_BACKSLASH
#undef SDLK_ESCAPE
#undef SDLK_SPACE
#undef SDLK_TAB
#undef SDLK_q
#define SDLK_UP SDL_SCANCODE_UP
#define SDLK_DOWN SDL_SCANCODE_DOWN
#define SDLK_LEFT SDL_SCANCODE_LEFT
#define SDLK_RIGHT SDL_SCANCODE_RIGHT
#define SDLK_LCTRL SDL_SCANCODE_LCTRL
#define SDLK_RCTRL SDL_SCANCODE_RCTRL
#define SDLK_LSHIFT SDL_SCANCODE_LSHIFT
#define SDLK_LALT SDL_SCANCODE_LALT
#define SDLK_RETURN SDL_SCANCODE_RETURN
#define SDLK_BACKSPACE SDL_SCANCODE_BACKSPACE
#define SDLK_BACKSLASH SDL_SCANCODE_BACKSLASH
#define SDLK_ESCAPE SDL_SCANCODE_ESCAPE
#define SDLK_SPACE SDL_SCANCODE_SPACE
#define SDLK_TAB SDL_SCANCODE_TAB
#define SDLK_q SDL_SCANCODE_Q
#define SDLK_WORLD_0 (SDL_NUM_SCANCODES - 4)
#define SDLK_WORLD_1 (SDL_NUM_SCANCODES - 3)
#define SDLK_WORLD_2 (SDL_NUM_SCANCODES - 2)
#define SDLK_WORLD_3 (SDL_NUM_SCANCODES - 1)

static inline int SDL_EnableKeyRepeat(int delay, int interval)
{
	(void)delay;
	(void)interval;
	return 0;
}

static inline int SDL_EnableUNICODE(int enable)
{
	(void)enable;
	return 0;
}

static inline void SDL_KeycodeToScancodeCompat(SDL_Event *event)
{
	if (event->type == SDL_KEYDOWN || event->type == SDL_KEYUP)
		event->key.keysym.sym = event->key.keysym.scancode;
}

static inline int SDL_PushEvent_compat(SDL_Event *event)
{
	SDL_KeycodeToScancodeCompat(event);
	return SDL_PushEvent(event);
}
#define SDL_PushEvent(event) SDL_PushEvent_compat(event)

static inline int SDL_PeepEvents_compat(SDL_Event *events, int numevents,
                                        SDL_eventaction action, Uint32 mask)
{
	Uint32 min_type = SDL_FIRSTEVENT;
	Uint32 max_type = SDL_LASTEVENT;
	int count;
	int i;

	if (mask == SDL_KEYDOWNMASK || mask == SDL_KEYUPMASK ||
	    mask == (SDL_KEYDOWNMASK | SDL_KEYUPMASK)) {
		min_type = SDL_KEYDOWN;
		max_type = SDL_KEYUP;
	}

	count = SDL_PEEP_EVENTS_NATIVE(events, numevents, action, min_type, max_type);
	if (events && count > 0) {
		for (i = 0; i < count; i++)
			SDL_KeycodeToScancodeCompat(&events[i]);
	}

	return count;
}
#define SDL_PeepEvents(events, numevents, action, mask) \
	SDL_PeepEvents_compat(events, numevents, action, mask)
#else
#include_next <SDL/SDL.h>
#endif

#endif /* PICOARCH_SDL_COMPAT_H */
