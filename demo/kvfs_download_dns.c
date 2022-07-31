#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <ldns/ldns.h>

#include <kvfs/kvfs.h>
#include <kvfs/drivers/dns.h>

void dump(FILE *in)
{
	char buffer[4096];
	while (!feof(in)) {
		ssize_t n = fread(buffer, 1, sizeof buffer, in);
		if (n > 0) {
			fwrite(buffer, 1, n, stdout);
		} else {
			fprintf(stderr, "received error %d on fread\n", errno);
		}
	}
}

int main(int argc, char *argv[])
{
	if (argc != 2) {
		fprintf(stderr, "usage: kvfs_download <key>\n");
		return EXIT_FAILURE;
	}

	if (strlen(argv[1]) != chunk_keylength * 2) {
		fprintf(stderr, "error: bad key length");
		return EXIT_FAILURE;
	}

	uint8_t key[chunk_keylength];
	uint8_t* q = key;
	char*p = &argv[1][0];
	for (int i = 0; i < chunk_keylength; ++i, p += 2) {
		if (sscanf(p, "%2hhx", q++) != 1) {
			fprintf(stderr, "error: bad key data");
		}
	}

	ldns_resolver* resolver = NULL;
	if (ldns_resolver_new_frm_file(&resolver, NULL) != LDNS_STATUS_OK) {
		fprintf(stderr, "couldn't create resolver context\n");
		return EXIT_FAILURE;
	};

	// set default domain
	ldns_rdf* domain = ldns_dname_new_frm_str("rb.me.uk");
	ldns_resolver_set_domain(resolver, domain);

	// create DNS store
	kvfs_store_t* store = kvfs_create_dns(resolver);

	FILE *in = kvfs_fopen_read(store, key);
	if (!in) {
		fprintf(stderr, "couldn't open %s\n", argv[1]);
		return EXIT_FAILURE;
	}

	dump(in);
	fclose(in);
	kvfs_free(store);

	return EXIT_SUCCESS;
}
