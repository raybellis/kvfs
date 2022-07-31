#include <cerrno>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

#include <kvfs/kvfs.h>
#include <kvfs/drivers/file.h>

#include <UnitTest++/UnitTest++.h>

class KVFSStdioHelper {
	protected:
		kvfs_store_t*	store;
	public:
		KVFSStdioHelper() : store(nullptr) {
			store = kvfs_create_file("/tmp");
			errno = 0;
		}
		~KVFSStdioHelper() {
			kvfs_free(store);
		}
};

SUITE(StdioFile)
{
	TEST(PassingNullStoreShouldFail)
	{
		FILE *fp = kvfs_fopen_write(NULL);
		CHECK(!fp);
		CHECK_EQUAL(EINVAL, errno);
	}

	TEST_FIXTURE(KVFSStdioHelper, PassingValidStoreShouldWork)
	{
		FILE *fp = kvfs_fopen_write(store);
		CHECK(fp);
		fclose(fp);
	}

	TEST_FIXTURE(KVFSStdioHelper, Write1024)
	{
		uint8_t data[1024] = { 0, };

		FILE *fp = kvfs_fopen_write(store);
		size_t count = fwrite(data, 1, sizeof data, fp);
		CHECK_EQUAL(0, ferror(fp));
		CHECK_EQUAL(sizeof data, count);
		fclose(fp);
	}

	TEST_FIXTURE(KVFSStdioHelper, Write2048)
	{
		uint8_t data[2048];
		memset(&data[0], 0x55, 1024);
		memset(&data[1024], 0xaa, 1024);

		FILE *fp = kvfs_fopen_write(store);
		size_t count = fwrite(data, 1, sizeof data, fp);
		CHECK_EQUAL(0, ferror(fp));
		CHECK_EQUAL(sizeof data, count);
		fclose(fp);
	}

	TEST_FIXTURE(KVFSStdioHelper, Read2048)
	{
		uint8_t key[] = {
			0x04, 0x40, 0x7d, 0x4b, 0x8f, 0x10, 0x15, 0xbf,
			0x93, 0x17, 0x42, 0x8b, 0x69, 0x10, 0x4a, 0x66,
			0x8a, 0x0a, 0x1b, 0x98, 0x23, 0xd4, 0x68, 0x50,
			0x61, 0xca, 0x85, 0xc2, 0xbc, 0x13, 0x36, 0x25
		};

		uint8_t data[2048];
		memset(data, 0, sizeof data);

		FILE *fp = kvfs_fopen_read(store, key);
		size_t count = fread(data, 1, sizeof data, fp);
		CHECK_EQUAL(sizeof data, count);
		CHECK_EQUAL(0, ferror(fp));
		fclose(fp);

		bool ok = true;
		for (int i = 0; i < 1024; ++i) {
			ok &= (data[i] == 0x55);
		}

		for (int i = 1024; i < 2048; ++i) {
			ok &= (data[i] == 0xaa);
		}

		CHECK(ok);
	}

	TEST_FIXTURE(KVFSStdioHelper, Write64k)
	{
		uint8_t data[65536];
		for (size_t i = 0; i < sizeof data; ++i) {
			data[i] = i & 0xff;
		}
		FILE *fp = kvfs_fopen_write(store);
		size_t count = fwrite(data, 1, sizeof data, fp);
		CHECK_EQUAL(0, ferror(fp));
		CHECK_EQUAL(sizeof data, count);
		fclose(fp);
	}

	TEST_FIXTURE(KVFSStdioHelper, Read64k)
	{
		const uint8_t* key = chunk_key_from_hex("084042e38dc8ce5220eef3f306d5efca1d37ecf6a2fbbb186e933c0ec72eb637");
		uint8_t data[65536];
		memset(data, 0, sizeof data);

		FILE *fp = kvfs_fopen_read(store, key);
		size_t n = fread(data, 1, sizeof data, fp);
		CHECK_EQUAL(sizeof data, n);
		CHECK_EQUAL(0, ferror(fp));
		fclose(fp);

		bool ok = true;
		for (size_t i = 0; i < sizeof data; ++i) {
			ok &= (data[i] == (i & 0xff));
		}

		CHECK(ok == true);
	}
}
