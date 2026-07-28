#ifndef TEXT_CACHE_H
#define TEXT_CACHE_H

#include <stddef.h>

typedef void (*text_cache_release_fn)(void *value);

struct text_cache_entry;

struct text_cache {
	struct text_cache_entry *head;
	struct text_cache_entry *tail;
	size_t bytes;
	size_t count;
	size_t limit;
	text_cache_release_fn release;
};

void text_cache_init(struct text_cache *cache, size_t limit,
		     text_cache_release_fn release);
void *text_cache_get(struct text_cache *cache, unsigned font_role,
		     unsigned color, const char *utf8);
int text_cache_put(struct text_cache *cache, unsigned font_role,
		   unsigned color, const char *utf8, void *value, size_t bytes);
void text_cache_clear(struct text_cache *cache);
size_t text_cache_bytes(const struct text_cache *cache);
size_t text_cache_count(const struct text_cache *cache);

#endif
