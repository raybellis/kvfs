/*
 * kvfs_memcache.h
 */

#ifndef __kvfs_memcache_h
#define __kvfs_memcache_h

#include <libmemcached/memcached.h>
#include <kvfs/kvfs.h>

#ifdef __cplusplus
extern "C" {
#endif

kvfs_store_t*	kvfs_create_memcache(memcached_st* memc);

#ifdef __cplusplus
}
#endif

#endif // __kvfs_memcache_h
