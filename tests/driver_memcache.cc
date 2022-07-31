#include <cerrno>

#include <kvfs/kvfs.h>
#include <kvfs/drivers/memcache.h>

#include <UnitTest++/UnitTest++.h>

class KVFSMemcacheHelper {
	protected:
		memcached_st*	memc;
		kvfs_store_t*	store;

	public:
		KVFSMemcacheHelper() {
			CHECK(memc = memcached_create(NULL));
			memcached_server_add(memc, "localhost", 11211);
			CHECK(store = kvfs_create_memcache(memc));
		}
		~KVFSMemcacheHelper() {
			kvfs_free(store);
			memcached_free(memc);
		}
};

SUITE(Memcached)
{
	TEST(PassingNullContextShouldFail)
	{
		kvfs_store_t* store = kvfs_create_memcache(NULL);
		CHECK(!store);
		CHECK_EQUAL(EINVAL, errno);
	}

	TEST_FIXTURE(KVFSMemcacheHelper, Create)
	{
		CHECK(store);
	}

	TEST_FIXTURE(KVFSMemcacheHelper, Put)
	{
		uint8_t data[1024] = { 0, };
		chunk_t* chunk = chunk_create(data, sizeof data, 0, false, NULL);
		CHECK_EQUAL(0, kvfs_put(store, chunk));
		chunk_free(chunk);
	}

	TEST_FIXTURE(KVFSMemcacheHelper, Get)
	{
		uint8_t key[] = {
			0x00, 0x00, 0xbf, 0x18, 0xa0, 0x86, 0x00, 0x70,
			0x16, 0xe9, 0x48, 0xb0, 0x4a, 0xed, 0x3b, 0x82,
			0x10, 0x3a, 0x36, 0xbe, 0xa4, 0x17, 0x55, 0xb6,
			0xcd, 0xdf, 0xaf, 0x10, 0xac, 0xe3, 0xc6, 0xef
		};

		chunk_t* chunk = kvfs_get(store, key);
		CHECK(chunk);
		CHECK_EQUAL(1024, chunk_length(chunk));
		CHECK_EQUAL(0, chunk_depth(chunk));
		chunk_free(chunk);
	}

	TEST_FIXTURE(KVFSMemcacheHelper, GetMissing)
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
