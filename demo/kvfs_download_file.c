#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <kvfs/kvfs.h>
#include <kvfs/drivers/file.h>

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

	kvfs_store_t* store = kvfs_create_file("/tmp");

	FILE *in = kvfs_fopen_read(store, key);
	while (!feof(in)) {
		char buffer[4096];
		size_t n = fread(buffer, 1, sizeof buffer, in);
		if (n > 0) {
			fwrite(buffer, 1, n, stdout);
		}
	}
	fclose(in);
	kvfs_free(store);
}
