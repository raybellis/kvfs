#include <stdio.h>
#include <errno.h>

#include <kvfs/drivers/memcache.h>
#include <kvfs/chunk.h>
#include <kvfs/private.h>

static chunk_t* kvfs_memcache_get(kvfs_store_t* store, const uint8_t* key)
{
	char keybuf[chunk_keylength * 2];
	size_t length;
	uint32_t flags;
	uint8_t depth;
	memcached_return r;

	chunk_hex_from_key_r(key, keybuf);
	char* data = memcached_get(store->context,
		keybuf, sizeof keybuf,
		&length, &flags, &r);

	if (r == MEMCACHED_SUCCESS) {
		if (data) {
			depth = chunk_depth_from_key(key);
			return chunk_create(data, length, depth, true, key);
		} else {
			errno = ENOENT;
		}
	} else if (r == MEMCACHED_NOTFOUND) {
		errno = ENOENT;
	} else {
		errno = KVFS_DRIVER_ERROR;
	}

	return NULL;
}

static int kvfs_memcache_put(kvfs_store_t* store, chunk_t* chunk)
{
	char keybuf[chunk_keylength * 2];

	chunk_hex_from_key_r(chunk_key(chunk), keybuf);
	memcached_return r = memcached_set(store->context,
		keybuf, sizeof keybuf,
		chunk_data(chunk), chunk_length(chunk),
		0, 0);

	if (r == MEMCACHED_SUCCESS) {
		return 0;
	}  else {
		errno = KVFS_DRIVER_ERROR;
	}

	return -1;
}

static void kvfs_memcache_free(kvfs_store_t* store)
{
	free(store);
}

static const char* kvfs_memcache_error(kvfs_store_t* store)
{
	return memcached_last_error_message(store->context);
}

kvfs_store_t* kvfs_create_memcache(memcached_st* memc)
{
	if (!memc) {
		errno = EINVAL;
		return NULL;
	}

	kvfs_store_t* store = malloc(sizeof *store);
	if (!store) {
		return NULL;
	}

	store->context = memc;
	store->get = kvfs_memcache_get;
	store->put = kvfs_memcache_put;
	store->free = kvfs_memcache_free;
	store->error = kvfs_memcache_error;

	return store;
}
