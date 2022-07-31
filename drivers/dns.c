#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <ldns/ldns.h>
#include <ldns/rr.h>

#include <kvfs/drivers/dns.h>
#include <kvfs/chunk.h>
#include <kvfs/private.h>

typedef struct kvfs_dns_context_t {
	ldns_resolver*	resolver;
	ldns_status		status;
} kvfs_dns_context_t;

static chunk_t* kvfs_dns_get(kvfs_store_t* store, const uint8_t* key);
static int kvfs_dns_put(kvfs_store_t* store, chunk_t* chunk);
static void kvfs_dns_free(kvfs_store_t* store);
static const char* kvfs_dns_error(kvfs_store_t* store);

static ldns_rdf* hex_domain(kvfs_dns_context_t* context, const uint8_t* key);
static chunk_t* kvfs_dns_query(kvfs_dns_context_t* context, ldns_rdf* qname, const uint8_t* key);
static int kvfs_dns_update(kvfs_dns_context_t* context, ldns_rdf* qname, const chunk_t* chunk);

static const ldns_rr_type rrtype = LDNS_RR_TYPE_NULL;

/* --------------------------------------------------------------------
 * public interface
 */

kvfs_store_t* kvfs_create_dns(ldns_resolver* resolver)
{
	kvfs_store_t* store = NULL;
	kvfs_dns_context_t* context = NULL;

	if (!resolver) {
		errno = EINVAL;
		return NULL;
	}

	store = malloc(sizeof *store);
	context = malloc(sizeof *context);

	if (!store || !context) {
		free(context);
		free(store);
		return NULL;
	}

	store->context = context;
	context->resolver = resolver;

	store->get = kvfs_dns_get;
	store->put = kvfs_dns_put;
	store->free = kvfs_dns_free;
	store->error = kvfs_dns_error;

	return store;
}

/* --------------------------------------------------------------------
 * hidden interface
 */

static chunk_t* kvfs_dns_get(kvfs_store_t* store, const uint8_t* key)
{
	chunk_t* chunk = NULL;

	ldns_rdf* domain = hex_domain(store->context, key);
	if (!domain) {
		goto error;
	}
	chunk = kvfs_dns_query(store->context, domain, key);
	ldns_rdf_deep_free(domain);

error:
	return chunk;
}

static int kvfs_dns_put(kvfs_store_t* store, chunk_t* chunk)
{
	int r;

	if (!chunk) {
		errno = EINVAL;
		return -1;
	}

	ldns_rdf* domain = hex_domain(store->context, chunk_key(chunk));
	if (!domain) {
		r = -1;
		goto cleanup;
	}

	r = kvfs_dns_update(store->context, domain, chunk);

cleanup:
	ldns_rdf_deep_free(domain);
	return r;
}

static void kvfs_dns_free(kvfs_store_t* store)
{
	if (store->context) {
		kvfs_dns_context_t* context = store->context;
		free(context);
	}
	free(store);
}

static const char* kvfs_dns_error(kvfs_store_t* store)
{
	kvfs_dns_context_t* context = store->context;
	return ldns_get_errorstr_by_id(context->status);
}

/* --------------------------------------------------------------------
 * helper functions
 */

/* produces a relative qname representing the key */
static ldns_rdf* hex_domain(kvfs_dns_context_t* context, const uint8_t* key)
{
	static const char* hexchars = "0123456789abcdef";
	const int label_length = 4;			// should be a factor of chunk_keylength
	char buffer[4 * chunk_keylength + 5];
	char *p = buffer;

	for (size_t i = 0, j = 0; i < chunk_keylength; ++i) {
		uint8_t b = *key++;
		if (j++ % label_length == 0) {
			*p++ = label_length;
		}
		*p++ = hexchars[(b >> 4) & 0x0f];
		if (j++ % label_length == 0) {
			*p++ = label_length;
		}
		*p++ = hexchars[b & 0x0f];
	}
	*p++ = 4;
	*p++ = 'k'; *p++ = 'v'; *p++ = 'f'; *p++ = 's';
	*p++ = '\0';

	ldns_rdf* prefix = ldns_dname_new_frm_data(p - buffer, buffer); 
	ldns_rdf* suffix = ldns_resolver_domain(context->resolver);
	if (prefix && suffix) {
		if (ldns_dname_cat(prefix, suffix) != LDNS_STATUS_OK) {
			ldns_rdf_free(prefix);
			prefix = NULL;
		}
	}

	return prefix;
}

static chunk_t* kvfs_dns_query(kvfs_dns_context_t* context, ldns_rdf* qname, const uint8_t* key)
{
	ldns_resolver* resolver = context->resolver;
	chunk_t* chunk = NULL;

	ldns_resolver_set_recursive(resolver, true);
	ldns_resolver_set_edns_udp_size(resolver, 2048);
	ldns_resolver_set_defnames(resolver, false);
	ldns_pkt* resp = ldns_resolver_query(context->resolver, qname, rrtype, LDNS_RR_CLASS_IN, LDNS_RD);
	if (resp == NULL) {
		return NULL;
	}

	ldns_rr_list* answer = ldns_pkt_answer(resp);
	for (size_t i = 0; !chunk && i < ldns_rr_list_rr_count(answer); ++i) {
		ldns_rr* rr = ldns_rr_list_rr(answer, i);
		if (ldns_rr_get_type(rr) != rrtype) {
			continue;
		}

		/* take ownership of the RR data */
		ldns_rdf* rdf = ldns_rr_rdf(rr, 0);
		uint8_t* data = ldns_rdf_data(rdf);
		size_t length = ldns_rdf_size(rdf);

		/* create the chunk, has the side effect of validating the data against the key */
		uint8_t depth = chunk_depth_from_key(key);
		chunk = chunk_create(data, length, depth, true, key);

		/* fixup the RR to avoid double-free */
		ldns_rdf_set_data(rdf, NULL);
		ldns_rdf_set_size(rdf, 0);
	}

	ldns_pkt_free(resp);
	return chunk;
}

static ldns_pkt* kvfs_dns_make_update(kvfs_dns_context_t* context, ldns_rdf* qname, const chunk_t* chunk)
{
	ldns_resolver* resolver = context->resolver;

	ldns_rr_list* updates = ldns_rr_list_new();
	ldns_rr* rr = ldns_rr_new();
	ldns_rdf* owner = ldns_rdf_clone(qname);
	ldns_rdf* rdata = ldns_rdf_new_frm_data(LDNS_RDF_TYPE_NONE, chunk_length(chunk), (void *)chunk_data(chunk));
	ldns_rdf* zone = ldns_rdf_clone(ldns_resolver_domain(resolver));

	if (!updates || !rr || !owner || !rdata || !zone) {
		return NULL;
	}

	ldns_rr_set_type(rr, rrtype);
	ldns_rr_set_ttl(rr, 86400);
	ldns_rr_set_owner(rr, owner);
	ldns_rr_push_rdf(rr, rdata);
	ldns_rr_list_push_rr(updates, rr);

	ldns_pkt *pkt = ldns_update_pkt_new(zone, LDNS_RR_CLASS_IN, NULL, updates, NULL);
	ldns_rr_list_free(updates);

	if (pkt && ldns_resolver_tsig_keyname(resolver)) {
		context->status = ldns_update_pkt_tsig_add(pkt, resolver);
		if (context->status != LDNS_STATUS_OK) {
			goto error;
		}
	}

	return pkt;

error:
	errno = KVFS_DRIVER_ERROR;
	ldns_pkt_free(pkt);
	return NULL;
}

static int kvfs_dns_update(kvfs_dns_context_t* context, ldns_rdf* qname, const chunk_t* chunk)
{
	ldns_pkt* r_pkt = NULL;
	ldns_pkt* u_pkt = kvfs_dns_make_update(context, qname, chunk);
	if (!u_pkt) {
		return -1;
	}

	errno = 0;
	ldns_resolver* resolver = context->resolver;
	ldns_pkt_set_random_id(u_pkt);
	context->status = ldns_resolver_send_pkt(&r_pkt, resolver, u_pkt);
	if (context->status == LDNS_STATUS_OK) {
		if (ldns_pkt_get_rcode(r_pkt) != LDNS_RCODE_NOERROR) {
			errno = ENOENT;
		}
		ldns_pkt_free(r_pkt);
	} else {
		errno = KVFS_DRIVER_ERROR;
	}
	ldns_pkt_free(u_pkt);

	return (errno == 0) ? 0 : -1;
}
