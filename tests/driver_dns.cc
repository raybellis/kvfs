#include <ldns/ldns.h>
#include <cerrno>

#include <kvfs/kvfs.h>
#include <kvfs/drivers/dns.h>

#include <UnitTest++/UnitTest++.h>

class KVFSDNSHelper {
	protected:
		ldns_resolver*	resolver;
		kvfs_store_t*	store;

	public:
		KVFSDNSHelper() : resolver(nullptr), store(nullptr) {
			resolver = ldns_resolver_new();
			CHECK(resolver);

			// set server address
			ldns_rdf* ns = ldns_rdf_new_frm_str(LDNS_RDF_TYPE_A, "54.165.45.170");
			ldns_resolver_push_nameserver(resolver, ns);
			ldns_rdf_deep_free(ns);

			// set default domain
			ldns_rdf* domain = ldns_dname_new_frm_str("rb.me.uk");
			ldns_resolver_set_domain(resolver, domain);

			// set TSIG
			ldns_resolver_set_tsig_algorithm(resolver, (char*)"hmac-sha256.");
			ldns_resolver_set_tsig_keyname(resolver, (char*)"kvfs-key");
			ldns_resolver_set_tsig_keydata(resolver, (char*)"kLc6QkUVzbPUj1BHGf4uCA==");

			CHECK(store = kvfs_create_dns(resolver));
		}

		~KVFSDNSHelper() {
			kvfs_free(store);
			ldns_resolver_deep_free(resolver);
		}
};

SUITE(DNS)
{
	TEST(PassingNullContextShouldFail)
	{
		kvfs_store_t* store = kvfs_create_dns(NULL);
		CHECK(!store);
		CHECK_EQUAL(EINVAL, errno);
	}

	TEST_FIXTURE(KVFSDNSHelper, Create)
	{
		CHECK(store);
	}

	TEST_FIXTURE(KVFSDNSHelper, Put)
	{
		uint8_t data[1024] = { 0, };
		chunk_t* chunk = chunk_create(data, sizeof data, 0, false, NULL);
		CHECK_EQUAL(0, kvfs_put(store, chunk));
		chunk_free(chunk);
	}

	TEST_FIXTURE(KVFSDNSHelper, Get)
	{
		uint8_t key[] = {
			0x00, 0x00, 0xbf, 0x18, 0xa0, 0x86, 0x00, 0x70,
			0x16, 0xe9, 0x48, 0xb0, 0x4a, 0xed, 0x3b, 0x82,
			0x10, 0x3a, 0x36, 0xbe, 0xa4, 0x17, 0x55, 0xb6,
			0xcd, 0xdf, 0xaf, 0x10, 0xac, 0xe3, 0xc6, 0xef
		};

		chunk_t* chunk = kvfs_get(store, key);
		CHECK(chunk);
		if (chunk) {
			CHECK_EQUAL(1024, chunk_length(chunk));
			CHECK_EQUAL(0, chunk_depth(chunk));
			const uint8_t* p = chunk_data(chunk);
			for (int i = 0; i < 1024; ++i) {
				CHECK_EQUAL(0, *p++);
			}
			chunk_free(chunk);
		}
	}
}
