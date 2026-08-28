#include <assert.h>
#include <stdarg.h>
#include <stdio.h>

#include "libpicofe/config_file.h"
#include "libpicofe/input.h"
#include "libpicofe/menu.h"
#include "plat_h150101_sdl2_config.h"

#define BASE_NAME "h150101-sdl2:Pad"
#define P2_NAME "h150101-sdl2:p2:Pad"

me_bind_action me_ctrl_actions[] = {
	{ "A", 1 },
	{ "B", 2 },
	{ NULL, 0 },
};

me_bind_action emuctrl_actions[] = {
	{ NULL, 0 },
};

void lprintf(const char *fmt, ...)
{
	(void)fmt;
}

unsigned int plat_get_ticks_ms(void)
{
	return 0;
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

static const char * const key_names[] = { "joy 0" };

static const char * const *get_key_names(const in_drv_t *drv, int *count)
{
	(void)drv;
	*count = 1;
	return key_names;
}

static void probe(const in_drv_t *drv)
{
	(void)drv;
	in_register(BASE_NAME, -1, NULL, 1, key_names, 0);
	in_register(P2_NAME, -1, NULL, 1, key_names, 0);
}

static int update(void *drv_data, const int *binds, int *result)
{
	(void)drv_data;
	(void)binds;
	(void)result;
	return 0;
}

static const in_drv_t test_driver = {
	.prefix = "h150101-sdl2:",
	.probe = probe,
	.get_key_names = get_key_names,
	.update = update,
	.config_match = h150101_sdl2_config_match,
};

int main(void)
{
	const int *p1_binds, *p2_binds;
	int ids[IN_MAX_DEVS];
	int count;

	in_init();
	assert(in_register_driver(&test_driver, NULL, NULL) == 0);
	in_probe();

	count = in_config_parse_devs(BASE_NAME, ids, IN_MAX_DEVS);
	assert(count == 2);
	assert(ids[0] != ids[1]);
	assert(in_config_parse_devs(BASE_NAME, ids, 1) == 1);
	assert(in_config_parse_devs(P2_NAME, ids, IN_MAX_DEVS) == 1);
	assert(in_config_parse_devs("h150101-sdl2:Other", ids,
		IN_MAX_DEVS) == 1);

	config_read_keys(
		"binddev = " BASE_NAME "\n"
		"bind joy 0 = player1 A\n"
		"binddev = " P2_NAME "\n"
		"bind joy 0 = player1 B\n");

	p1_binds = in_get_dev_binds(0);
	p2_binds = in_get_dev_binds(1);
	assert(p1_binds != NULL);
	assert(p2_binds != NULL);
	assert(p1_binds[IN_BIND_OFFS(0, IN_BINDTYPE_PLAYER12)] == 1);
	assert(p2_binds[IN_BIND_OFFS(0, IN_BINDTYPE_PLAYER12)] == 2);

	return 0;
}
