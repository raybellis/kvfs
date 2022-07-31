/*
 * kvfs_dns.h
 */

#ifndef __kvfs_dns_h
#define __kvfs_dns_h

#include <ldns/ldns.h>
#include <kvfs/kvfs.h>

#ifdef __cplusplus
extern "C" {
#endif

kvfs_store_t*	kvfs_create_dns(ldns_resolver* resolver);

#ifdef __cplusplus
}
#endif

#endif // __kvfs_dns_h
