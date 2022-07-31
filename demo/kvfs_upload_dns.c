#include <stdlib.h>
#include <stdio.h>
#include <ldns/ldns.h>

#include <kvfs/kvfs.h>
#include <kvfs/drivers/dns.h>

int main(int argc, char *argv[])
{
	ldns_resolver* resolver = ldns_resolver_new();

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

	// create DNS store
	kvfs_store_t* store = kvfs_create_dns(resolver);

	for (int i = 1; i < argc; ++i) {
		char buffer[4096];

		FILE *in = fopen(argv[i], "rb");
		FILE *out = kvfs_fopen_write(store);

		while (!feof(in)) {
			ssize_t n = fread(buffer, 1, sizeof buffer, in);
			if (n > 0) {
				size_t offset = 0;
				while (offset < n) {
					ssize_t r = fwrite(buffer + offset, 1, n, out);
					if (r < 0) {
						printf(stderr, "error writing file %d\n", i);
						break;
					} 
					offset += r;
					n -= r;
				}
			}
		}
		fclose(in);
		fclose(out);

		// get the key
		const uint8_t* key = kvfs_last(store);
		for (int i = 0; i < chunk_keylength; ++i) {
			fprintf(stdout, "%02x", key[i]);
		}

		fprintf(stdout, ": %s\n", argv[i]);
	}

	kvfs_free(store);
}
