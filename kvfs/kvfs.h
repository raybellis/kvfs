/*
 * kvfs.h
 */

#ifndef __kvfs_h
#define __kvfs_h

#include <stdio.h>
#include <stdint.h>
#include <kvfs/chunk.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct kvfs_store_t kvfs_store_t;

chunk_t*		kvfs_get(kvfs_store_t* store, const uint8_t* key);
int				kvfs_put(kvfs_store_t* store, chunk_t* chunk);
void			kvfs_free(kvfs_store_t* store);
const uint8_t*	kvfs_last(kvfs_store_t* store);
const char*		kvfs_error(kvfs_store_t* store);

FILE*			kvfs_fopen_read(kvfs_store_t* store, const uint8_t* key);
FILE*			kvfs_fopen_write(kvfs_store_t* store);

enum {
	KVFS_ERRNO_BASE	= 0x1000,
	KVFS_BAD_DATA_LENGTH = 0x1000,
	KVFS_BAD_INDIRECT_DEPTH,
	KVFS_BAD_INDIRECT_LENGTH,
	KVFS_KEY_NOT_VALID,
	KVFS_DRIVER_ERROR,
	KVFS_ERRNO_LAST
};

#ifdef __cplusplus
}
#endif

#endif // __kvfs_h
