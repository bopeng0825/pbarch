#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "text_cache.h"

static void *released[16];
static size_t released_count;

#ifdef TEXT_CACHE_TEST_ALLOC
#undef malloc
static int allocations_before_failure = -1;

void *test_malloc(size_t size)
{
	if (allocations_before_failure == 0)
		return NULL;
	if (allocations_before_failure > 0)
		allocations_before_failure--;
	return calloc(1, size);
}
#endif

static void release_value(void *value)
{
	released[released_count++] = value;
}

static void reset_releases(void)
{
	released_count = 0;
}

static void test_lru_promotion_and_eviction(void)
{
	struct text_cache cache;
	int a, b, c;

	reset_releases();
	text_cache_init(&cache, 10, release_value);
	assert(text_cache_put(&cache, 0, 0xffff, "a", &a, 4) == 0);
	assert(text_cache_put(&cache, 0, 0xffff, "b", &b, 4) == 0);
	assert(text_cache_count(&cache) == 2);
	assert(text_cache_bytes(&cache) == 8);
	assert(text_cache_get(&cache, 0, 0xffff, "a") == &a);
	assert(text_cache_put(&cache, 0, 0xffff, "c", &c, 4) == 0);
	assert(text_cache_get(&cache, 0, 0xffff, "b") == NULL);
	assert(text_cache_get(&cache, 0, 0xffff, "a") == &a);
	assert(text_cache_get(&cache, 0, 0xffff, "c") == &c);
	assert(released_count == 1);
	assert(released[0] == &b);
	assert(text_cache_count(&cache) == 2);
	assert(text_cache_bytes(&cache) == 8);

	text_cache_clear(&cache);
	assert(released_count == 3);
	assert(text_cache_bytes(&cache) == 0);
	assert(text_cache_count(&cache) == 0);
}

static void test_duplicate_replaces_value_and_accounting(void)
{
	struct text_cache cache;
	int old_value, other, new_value;
	char key[] = "same";

	reset_releases();
	text_cache_init(&cache, 10, release_value);
	assert(text_cache_put(&cache, 3, 0x1234, key, &old_value, 6) == 0);
	key[0] = 'X';
	assert(text_cache_put(&cache, 4, 0x1234, "other", &other, 2) == 0);
	assert(text_cache_put(&cache, 3, 0x1234, "same", &new_value, 3) == 0);
	assert(text_cache_get(&cache, 3, 0x1234, "same") == &new_value);
	assert(text_cache_get(&cache, 4, 0x1234, "other") == &other);
	assert(released_count == 1);
	assert(released[0] == &old_value);
	assert(text_cache_count(&cache) == 2);
	assert(text_cache_bytes(&cache) == 5);

	text_cache_clear(&cache);
	assert(released_count == 3);
}

static void test_oversized_entry_is_rejected_without_mutation(void)
{
	struct text_cache cache;
	int kept, rejected;

	reset_releases();
	text_cache_init(&cache, 4, release_value);
	assert(text_cache_put(&cache, 0, 0, "kept", &kept, 4) == 0);
	assert(text_cache_put(&cache, 0, 0, "large", &rejected, 5) == -1);
	assert(text_cache_get(&cache, 0, 0, "kept") == &kept);
	assert(text_cache_get(&cache, 0, 0, "large") == NULL);
	assert(released_count == 0);
	assert(text_cache_count(&cache) == 1);
	assert(text_cache_bytes(&cache) == 4);

	text_cache_clear(&cache);
	assert(released_count == 1);
}

static void test_byte_accounting_does_not_overflow(void)
{
	struct text_cache cache;
	int old_value, new_value;

	reset_releases();
	text_cache_init(&cache, SIZE_MAX, release_value);
	assert(text_cache_put(&cache, 0, 0, "old", &old_value,
			      SIZE_MAX - 2) == 0);
	assert(text_cache_put(&cache, 0, 0, "new", &new_value, 4) == 0);
	assert(text_cache_get(&cache, 0, 0, "old") == NULL);
	assert(text_cache_get(&cache, 0, 0, "new") == &new_value);
	assert(released_count == 1);
	assert(released[0] == &old_value);
	assert(text_cache_count(&cache) == 1);
	assert(text_cache_bytes(&cache) == 4);

	text_cache_clear(&cache);
}

#ifdef TEXT_CACHE_TEST_ALLOC
static void test_allocation_failures_leave_cache_unchanged(void)
{
	struct text_cache cache;
	int kept, rejected;

	reset_releases();
	text_cache_init(&cache, 10, release_value);
	assert(text_cache_put(&cache, 0, 0, "kept", &kept, 3) == 0);

	allocations_before_failure = 0;
	assert(text_cache_put(&cache, 0, 0, "node", &rejected, 2) == -1);
	assert(text_cache_count(&cache) == 1);
	assert(text_cache_bytes(&cache) == 3);
	assert(released_count == 0);

	allocations_before_failure = 1;
	assert(text_cache_put(&cache, 0, 0, "key", &rejected, 2) == -1);
	assert(text_cache_count(&cache) == 1);
	assert(text_cache_bytes(&cache) == 3);
	assert(text_cache_get(&cache, 0, 0, "kept") == &kept);
	assert(released_count == 0);

	allocations_before_failure = -1;
	text_cache_clear(&cache);
	assert(released_count == 1);
}
#endif

int main(void)
{
	test_lru_promotion_and_eviction();
	test_duplicate_replaces_value_and_accounting();
	test_oversized_entry_is_rejected_without_mutation();
	test_byte_accounting_does_not_overflow();
#ifdef TEXT_CACHE_TEST_ALLOC
	test_allocation_failures_leave_cache_unchanged();
#endif
	return 0;
}
