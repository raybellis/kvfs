#include <cerrno>
#include <unistd.h>
#include <fcntl.h>

#include <kvfs/kvfs.h>
#include <kvfs/drivers/file.h>

#include <UnitTest++/UnitTest++.h>

class KVFSFileHelper {
	protected:
		kvfs_store_t*	store;
		const char* empty1024 = "/tmp/0000bf18a086007016e948b04aed3b82103a36bea41755b6cddfaf10ace3c6ef.kvfs";
	public:
		KVFSFileHelper() : store(nullptr) {
			store = kvfs_create_file("/tmp");
			errno = 0;
		}
		~KVFSFileHelper() {
			kvfs_free(store);
		}
};

SUITE(File)
{
	TEST(PassingNullContextShouldFail)
	{
		kvfs_store_t* store = kvfs_create_file(NULL);
		CHECK(!store);
		CHECK_EQUAL(EINVAL, errno);
	}

	TEST_FIXTURE(KVFSFileHelper, Create)
	{
		CHECK(store);
	}

	TEST_FIXTURE(KVFSFileHelper, Put)
	{
		uint8_t data[1024] = { 0, };
		unlink(empty1024);
		errno = 0;
		chunk_t* chunk = chunk_create(data, sizeof data, 0, false, NULL);
		CHECK_EQUAL(0, kvfs_put(store, chunk));
		CHECK_EQUAL(0, errno);
		CHECK_EQUAL(0, access(empty1024, R_OK));
		chunk_free(chunk);
	}

	TEST_FIXTURE(KVFSFileHelper, Get)
	{
		uint8_t key[] = {
			0x00, 0x00, 0xbf, 0x18, 0xa0, 0x86, 0x00, 0x70,
			0x16, 0xe9, 0x48, 0xb0, 0x4a, 0xed, 0x3b, 0x82,
			0x10, 0x3a, 0x36, 0xbe, 0xa4, 0x17, 0x55, 0xb6,
			0xcd, 0xdf, 0xaf, 0x10, 0xac, 0xe3, 0xc6, 0xef
		};

		chunk_t* chunk = kvfs_get(store, key);
		CHECK(chunk);
		CHECK_EQUAL(0, errno);
		CHECK_EQUAL(1024, chunk_length(chunk));
		CHECK_EQUAL(0, chunk_depth(chunk));
		chunk_free(chunk);
	}

	TEST_FIXTURE(KVFSFileHelper, GetMissing)
	{
		uint8_t key[] = {
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
		};

		chunk_t* chunk = kvfs_get(store, key);
		CHECK(!chunk);
		CHECK_EQUAL(ENOENT, errno);
	}
}
