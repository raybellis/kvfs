/*
 * kvfs_impl.h
 */

#ifndef __kvfs_impl_h
#define __kvfs_impl_h

#include <stdio.h>
#include <kvfs/chunk.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct kvfs_store_t {
	void*			context;
	uint8_t			last[chunk_keylength];
	chunk_t*		(*get)(struct kvfs_store_t* store, const uint8_t* key);
	int				(*put)(struct kvfs_store_t* store, chunk_t* chunk);
	void			(*free)(struct kvfs_store_t* store);
	const char*		(*error)(struct kvfs_store_t* store);
} kvfs_store_t;

#ifdef __cplusplus
}
#endif

#endif // __kvfs_impl_h
