#include <cstring>
#include <cerrno>
#include <kvfs/chunk.h>

#include <UnitTest++/UnitTest++.h>

class ChunkZeroX1024 {
	protected:
		chunk_t*		chunk;
		uint8_t			buf[1024];

	public:
		ChunkZeroX1024() {
			memset(buf, 0, sizeof buf);
			chunk = chunk_create(buf, sizeof buf, 0, false, NULL);
		}
		~ChunkZeroX1024() {
			chunk_free(chunk);
		}
};

class ChunkZeroX32L1 {
	protected:
		chunk_t*		chunk;
		uint8_t			buf[32];

	public:
		ChunkZeroX32L1() {
			memset(buf, 0, sizeof buf);
			chunk = chunk_create(buf, sizeof buf, 1, false, NULL);
		}
		~ChunkZeroX32L1() {
			chunk_free(chunk);
		}
};

SUITE(Chunk)
{
	TEST(CreateZeroLengthShouldFail)
	{
		uint8_t data[1] = { 0, };
		chunk_t* chunk = chunk_create(data, 0, 0, false, NULL);
		CHECK(!chunk);
		CHECK_EQUAL(EINVAL, errno);
	}

	TEST(CreateTooLongShouldFail)
	{
		uint8_t data[1025] = { 0, };
		chunk_t* chunk = chunk_create(data, sizeof data, 0, false, NULL);
		CHECK(!chunk);
		CHECK_EQUAL(EINVAL, errno);
	}

	TEST(CreateNullDataShouldFail)
	{
		chunk_t* chunk = chunk_create(NULL, 0, 0, false, NULL);
		CHECK(!chunk);
		CHECK_EQUAL(EINVAL, errno);
	}

	TEST(HexFromKey)
	{
		const char *hex = "0000bf18a086007016e948b04aed3b82103a36bea41755b6cddfaf10ace3c6ef";
		const uint8_t key[] = {
			0x00, 0x00, 0xbf, 0x18, 0xa0, 0x86, 0x00, 0x70,
			0x16, 0xe9, 0x48, 0xb0, 0x4a, 0xed, 0x3b, 0x82,
			0x10, 0x3a, 0x36, 0xbe, 0xa4, 0x17, 0x55, 0xb6,
			0xcd, 0xdf, 0xaf, 0x10, 0xac, 0xe3, 0xc6, 0xef
		};

		CHECK(strcmp(hex, chunk_hex_from_key(key)) == 0);
		CHECK(memcmp(key, chunk_key_from_hex(hex), chunk_keylength) == 0);
	}

	TEST_FIXTURE(ChunkZeroX1024, Length)
	{
		CHECK_EQUAL(1024, chunk_length(chunk));
	}

	TEST_FIXTURE(ChunkZeroX1024, Depth)
	{
		CHECK_EQUAL(0, chunk_depth(chunk));
	}

	TEST_FIXTURE(ChunkZeroX1024, Key)
	{
		uint8_t key[] = {
			0x00, 0x00, 0xbf, 0x18, 0xa0, 0x86, 0x00, 0x70,
			0x16, 0xe9, 0x48, 0xb0, 0x4a, 0xed, 0x3b, 0x82,
			0x10, 0x3a, 0x36, 0xbe, 0xa4, 0x17, 0x55, 0xb6,
			0xcd, 0xdf, 0xaf, 0x10, 0xac, 0xe3, 0xc6, 0xef
		};

		CHECK(chunk_key_valid(chunk, key));
	}

	TEST_FIXTURE(ChunkZeroX32L1, Length)
	{
		CHECK_EQUAL(32, chunk_length(chunk));
	}

	TEST_FIXTURE(ChunkZeroX32L1, Depth)
	{
		CHECK_EQUAL(true, chunk_depth(chunk));
	}

	TEST_FIXTURE(ChunkZeroX32L1, Key)
	{
		uint8_t key[] = {
			0x04, 0x20, 0x7a, 0xad, 0xf8, 0x62, 0xbd, 0x77,
			0x6c, 0x8f, 0xc1, 0x8b, 0x8e, 0x9f, 0x8e, 0x20,
			0x08, 0x97, 0x14, 0x85, 0x6e, 0xe2, 0x33, 0xb3,
			0x90, 0x2a, 0x59, 0x1d, 0x0d, 0x5f, 0x29, 0x25
		};

		CHECK(chunk_key_valid(chunk, key));
	}
}
