/*
 * kvfs_file.h
 */

#ifndef __kvfs_file_h
#define __kvfs_file_h

#include <kvfs/kvfs.h>

#ifdef __cplusplus
extern "C" {
#endif

kvfs_store_t*	kvfs_create_file(const char *path);

#ifdef __cplusplus
}
#endif

#endif // __kvfs_file_h
