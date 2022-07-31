#include <stdlib.h>
#include <stdio.h>

#include <kvfs/kvfs.h>
#include <kvfs/drivers/file.h>

int main(int argc, char *argv[])
{
	kvfs_store_t* store = kvfs_create_file("/tmp");

	for (int i = 1; i < argc; ++i) {
		char buffer[4096];

		FILE *in = fopen(argv[i], "rb");
		FILE *out = kvfs_fopen_write(store);

		while (!feof(in)) {
			size_t n = fread(buffer, 1, sizeof buffer, in);
			if (n > 0) {
				size_t offset = 0;
				while (offset < n) {
					size_t r = fwrite(buffer + offset, 1, n, out);
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
