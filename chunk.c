/*
 * chunk.c
 */

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <ldns/sha2.h>

#include <kvfs/kvfs.h>
#include <kvfs/chunk.h>

/*
 * NB: the bottom bit of 'data' is used to indicate whether the
 * chunk owns data (and should eventually free it)
 */

typedef struct chunk_t {
	uint64_t	data;
	uint8_t		key[LDNS_SHA256_DIGEST_LENGTH];
} chunk_t;

static void chunk_calckey(const uint8_t* data, uint16_t length, uint8_t depth, uint8_t* key);
static int chunk_validate(const uint8_t* data, uint16_t length, uint8_t depth);

/*
 * creates a chunk, just copying the pointer to the passed data.
 *
 * the 'owner' flag is used in a tagged pointer to indicate whether
 * the chunk owns this data itself and should free it when the
 * chunk itself is free'd.
 */
chunk_t* chunk_create(const uint8_t* data, uint16_t length, uint8_t depth, bool owner, const uint8_t *key)
{
	chunk_t* chunk = NULL;

	/* can't pass a null pointer */
	if (data == NULL) {
		errno = EINVAL;
		return NULL;
	}

	/* make sure the chunk passes sanity check */
	if (chunk_validate(data, length, depth) != 0) {
		goto error;
	}

	/* allocate space for the chunk */
	chunk = malloc(sizeof *chunk);
	if (chunk == NULL) {
		return NULL;
	}

	/* calculate the key */
	chunk_calckey(data, length, depth, &chunk->key[0]);

	/* copy and tag the pointer (if required) */
	chunk->data = (uint64_t)data;
	if (!owner) {
		chunk->data |= 0x01;
	}

	/* optionally, check that the calculated key matches */
	if (key && !chunk_key_valid(chunk, key)) {
		goto error;
	}

	return chunk;

error:
	if (owner) {
		free((void *)data);
	}
	free(chunk);
	return NULL;
}

/*
 * creates a chunk as above but takes a copy of the data first
 */
chunk_t* chunk_create_copy(const uint8_t* data, uint16_t length, uint8_t depth, const uint8_t *key)
{
	/* short circuit this test */
	if (length > chunk_maxlength) {
		errno = EINVAL;
		return NULL;
	}

	uint8_t* copy = malloc(length);
	if (copy == NULL) {
		return NULL;
	}

	memcpy(copy, data, length);

	return chunk_create(copy, length, depth, true, key);
}

/*
 * frees a chunk and and the data within it
 */
void chunk_free(chunk_t* chunk)
{
	if (chunk) {
		if ((chunk->data & 0x01) == 0x00) {
			free((uint8_t*)chunk->data);
		}
	}
	free(chunk);
}

/*
 * returns a (const) pointer to the chunk's key
 */
const uint8_t* chunk_key(const chunk_t* chunk)
{
	assert(chunk && chunk->key);
	return &chunk->key[0];
}

/*
 * returns a (const) pointer to the chunk's data
 */
const uint8_t* chunk_data(const chunk_t* chunk)
{
	assert(chunk && chunk->data);
	return (const uint8_t*)(chunk->data & ~0x01);
}

/*
 * returns the chunk's depth
 */
uint8_t chunk_depth(const chunk_t* chunk)
{
	assert(chunk);
	return chunk_depth_from_key(chunk->key);
}

/*
 * returns the chunk's length
 */
uint16_t chunk_length(const chunk_t* chunk)
{
	assert(chunk);
	return chunk_length_from_key(chunk->key);
}

/*
 * checks that the chunk's key matches the passed key
 */
bool chunk_key_valid(const chunk_t* chunk, const uint8_t *key)
{
	assert(chunk && key);
	bool result = memcmp(chunk_key(chunk), key, chunk_keylength) == 0;
	if (!result) {
		errno = KVFS_KEY_NOT_VALID;
	}
	return result;
}

/*
 * extracts the chunk's depth from the encoded key
 */
uint8_t chunk_depth_from_key(const uint8_t *key)
{
	assert(key);
	return (key[0] >> 2) & 0x3f;
}

/*
 * extract the chunk's length from the encoded key
 */
uint16_t chunk_length_from_key(const uint8_t *key)
{
	assert(key);
	uint16_t length = (key[0] << 8 | key[1]) & (chunk_maxlength - 1);
	return length ? length : chunk_maxlength;
}

uint8_t* chunk_key_from_hex_r(const char *hex, uint8_t *buffer)
{
	uint8_t* p = buffer;
	const char *q = hex;

	for (int i = 0; i < chunk_keylength; ++i, q += 2) {
		if (sscanf(q, "%2hhx", p++) != 1) {
			return NULL;
		}
	}

	return &buffer[0];
}

char* chunk_hex_from_key_r(const uint8_t *key, char *hex)
{
	char *p = hex;
	static const char hexchars[] = "0123456789abcdef";
	for (int i = 0; i < chunk_keylength; ++i) {
		uint8_t c = *key++;
		*p++ = hexchars[(c >> 4) & 0x0f];
		*p++ = hexchars[(c >> 0) & 0x0f];
	}
	return p;
}

const uint8_t* chunk_key_from_hex(const char* hex)
{
	static uint8_t buffer[chunk_keylength];
	return chunk_key_from_hex_r(hex, buffer);
}

const char* chunk_hex_from_key(const uint8_t* key)
{
	static char buffer[chunk_keylength * 2 + 1];
	chunk_hex_from_key_r(key, buffer);
	buffer[chunk_keylength * 2] = '\0';
	return &buffer[0];
}

/*
 * calculates the SHA-256 key from the data, and
 * encodes the depth and length into that key
 */
static void chunk_calckey(const uint8_t* data, uint16_t length, uint8_t depth, uint8_t* key)
{
	ldns_sha256((unsigned char *)data, length, key);

	key[0] = (depth << 2) | ((length >> 8) & 0x03);
	key[1] = length & 0xff;
}

static int chunk_validate(const uint8_t* data, uint16_t length, uint8_t depth)
{
	/* chunks can't exceed the maxmimum size, nor be empty */
	if (length == 0 || length > chunk_maxlength) {
		errno = EINVAL;
		return -1;
	}

	/* branch nodes require lots more validation */
	if (depth > 0) {

		/* chunk must contain whole keys */
		if (length % chunk_keylength != 0) {
			errno = KVFS_BAD_DATA_LENGTH;
			return -1;
		}

		/* check the sub key data */
		for (uint16_t offset = 0; offset < length; ) {
			const uint8_t* key = data + offset;

			/* check that the depth value is correct */
			if (chunk_depth_from_key(key) != depth - 1) {
				errno = KVFS_BAD_INDIRECT_DEPTH;
				return -1;
			}

			/* check that the length is correct - only the last
			 * key may be for less than a full chunk */
			offset += chunk_keylength;
			if (offset < length) {
				if (chunk_length_from_key(key) != chunk_maxlength) {
					errno = KVFS_BAD_INDIRECT_LENGTH;
					return -1;
				}
			}
		}
	}

	return 0;
}
