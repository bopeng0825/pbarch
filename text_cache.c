#include <stdlib.h>
#include <string.h>

#include "text_cache.h"

struct text_cache_entry {
	struct text_cache_entry *prev;
	struct text_cache_entry *next;
	unsigned font_role;
	unsigned color;
	char *utf8;
	void *value;
	size_t bytes;
};

static void unlink_entry(struct text_cache *cache,
			 struct text_cache_entry *entry)
{
	if (entry->prev)
		entry->prev->next = entry->next;
	else
		cache->head = entry->next;

	if (entry->next)
		entry->next->prev = entry->prev;
	else
		cache->tail = entry->prev;
}

static void link_at_head(struct text_cache *cache,
			 struct text_cache_entry *entry)
{
	entry->prev = NULL;
	entry->next = cache->head;
	if (cache->head)
		cache->head->prev = entry;
	else
		cache->tail = entry;
	cache->head = entry;
}

static void promote(struct text_cache *cache,
		    struct text_cache_entry *entry)
{
	if (cache->head == entry)
		return;
	unlink_entry(cache, entry);
	link_at_head(cache, entry);
}

static struct text_cache_entry *find_entry(struct text_cache *cache,
					    unsigned font_role,
					    unsigned color,
					    const char *utf8)
{
	struct text_cache_entry *entry;

	for (entry = cache->head; entry; entry = entry->next) {
		if (entry->font_role == font_role && entry->color == color &&
		    strcmp(entry->utf8, utf8) == 0)
			return entry;
	}
	return NULL;
}

static void remove_entry(struct text_cache *cache,
			 struct text_cache_entry *entry)
{
	unlink_entry(cache, entry);
	cache->bytes -= entry->bytes;
	cache->count--;
	if (cache->release)
		cache->release(entry->value);
	free(entry->utf8);
	free(entry);
}

void text_cache_init(struct text_cache *cache, size_t limit,
		     text_cache_release_fn release)
{
	cache->head = NULL;
	cache->tail = NULL;
	cache->bytes = 0;
	cache->count = 0;
	cache->limit = limit;
	cache->release = release;
}

void *text_cache_get(struct text_cache *cache, unsigned font_role,
		     unsigned color, const char *utf8)
{
	struct text_cache_entry *entry;

	if (!utf8)
		return NULL;
	entry = find_entry(cache, font_role, color, utf8);
	if (!entry)
		return NULL;
	promote(cache, entry);
	return entry->value;
}

int text_cache_put(struct text_cache *cache, unsigned font_role,
		   unsigned color, const char *utf8, void *value, size_t bytes)
{
	struct text_cache_entry *entry;
	size_t utf8_size;
	int is_new = 0;

	if (!utf8 || bytes > cache->limit)
		return -1;

	entry = find_entry(cache, font_role, color, utf8);
	if (entry) {
		cache->bytes -= entry->bytes;
		if (cache->release && entry->value != value)
			cache->release(entry->value);
		entry->value = value;
		entry->bytes = bytes;
		promote(cache, entry);
	} else {
		utf8_size = strlen(utf8) + 1;
		entry = malloc(sizeof(*entry));
		if (!entry)
			return -1;
		entry->utf8 = malloc(utf8_size);
		if (!entry->utf8) {
			free(entry);
			return -1;
		}
		memcpy(entry->utf8, utf8, utf8_size);
		entry->font_role = font_role;
		entry->color = color;
		entry->value = value;
		entry->bytes = bytes;
		is_new = 1;
	}

	while (cache->bytes > cache->limit - bytes)
		remove_entry(cache, cache->tail);
	if (is_new) {
		link_at_head(cache, entry);
		cache->count++;
	}
	cache->bytes += bytes;
	return 0;
}

void text_cache_clear(struct text_cache *cache)
{
	while (cache->tail)
		remove_entry(cache, cache->tail);
}

size_t text_cache_bytes(const struct text_cache *cache)
{
	return cache->bytes;
}

size_t text_cache_count(const struct text_cache *cache)
{
	return cache->count;
}
