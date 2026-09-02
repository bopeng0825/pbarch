#include <assert.h>
#include <stddef.h>
#include <stdarg.h>

#include "libpicofe/input.h"

static unsigned int fake_now;
static int held_state;
static int event_at;
static int event_state;
static int redraw_count;

static int fake_wait(int timeout_ms)
{
	if (timeout_ms > 0)
		fake_now += (unsigned int)timeout_ms;
	if (event_at >= 0 && fake_now >= (unsigned int)event_at)
		held_state = event_state;
	return held_state;
}

static unsigned int fake_ticks(void)
{
	return fake_now;
}

unsigned int plat_get_ticks_ms(void)
{
	return fake_ticks();
}

void plat_sleep_ms(int ms)
{
	(void)ms;
}

int plat_wait_event(int *fds_hnds, int count, int timeout_ms)
{
	(void)fds_hnds;
	(void)count;
	(void)timeout_ms;
	return -1;
}

void lprintf(const char *fmt, ...)
{
	(void)fmt;
}

static void redraw(void *data)
{
	(void)data;
	redraw_count++;
}

static void setup(int initial_state, int change_at, int changed_state)
{
	fake_now = 0;
	held_state = initial_state;
	event_at = change_at;
	event_state = changed_state;
	redraw_count = 0;
	in_menu_wait_test_setup(initial_state, fake_wait, fake_ticks);
}

static void test_held_key_keeps_original_repeat_delays(void)
{
	int result;

	setup(PBTN_DOWN, -1, PBTN_DOWN);
	result = in_menu_wait_with_callback(PBTN_DOWN, NULL, 70, 70,
					    redraw, NULL);
	assert(result == PBTN_DOWN);
	assert(fake_now == 450);
	assert(redraw_count >= 6);

	redraw_count = 0;
	result = in_menu_wait_with_callback(PBTN_DOWN, NULL, 70, 70,
					    redraw, NULL);
	assert(result == PBTN_DOWN);
	assert(fake_now == 520);
	assert(redraw_count == 1);
}

static void test_idle_and_uninteresting_states_keep_redrawing(void)
{
	int result;

	setup(0, 700, PBTN_DOWN);
	result = in_menu_wait_with_callback(PBTN_DOWN, NULL, 70, 70,
					    redraw, NULL);
	assert(result == PBTN_DOWN);
	assert(fake_now >= 700 && fake_now <= 770);
	assert(redraw_count >= 9);

	setup(PBTN_MOK, 210, PBTN_MOK | PBTN_DOWN);
	result = in_menu_wait_with_callback(PBTN_DOWN, NULL, 70, 70,
					    redraw, NULL);
	assert(result & PBTN_DOWN);
	assert(fake_now == 210);
	assert(redraw_count == 2);
}

int main(void)
{
	test_held_key_keeps_original_repeat_delays();
	test_idle_and_uninteresting_states_keep_redrawing();
	return 0;
}
