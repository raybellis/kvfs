/*
 * kvfs.c
 */

#include <string.h>
#include <errno.h>

#include <kvfs/kvfs.h>
#include <kvfs/private.h>

static const char* errors[] = {
	"bad chunk data length",
	"bad indirection pointer depth",
	"bad indirection pointer length",
	"key not valid",
	"unspecified driver error"
};

chunk_t* kvfs_get(kvfs_store_t* store, const uint8_t* key)
{
	return store->get(store, key);
}

int kvfs_put(kvfs_store_t* store, chunk_t* chunk)
{
	int result = store->put(store, chunk);
	if (result >= 0) {
		memcpy(store->last, chunk_key(chunk), chunk_keylength);
	}
	return result;
}

void kvfs_free(kvfs_store_t* store)
{
	store->free(store);
}

const uint8_t *kvfs_last(kvfs_store_t* store)
{
	return store->last;
}

const char *kvfs_error(kvfs_store_t* store)
{
	if (errno < KVFS_ERRNO_BASE) {
		return strerror(errno);
	}

	if (errno == KVFS_DRIVER_ERROR) {
		const char *error = store->error(store);
		if (error) {
			return error;
		}
	}

	if (errno >= KVFS_ERRNO_BASE && errno < KVFS_ERRNO_LAST) {
		return errors[errno - KVFS_ERRNO_BASE];
	} else {
		return "unknown error";
	}
}
