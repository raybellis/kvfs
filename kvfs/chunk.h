/*
 * chunk.h
 */

#ifndef __chunk_h
#define __chunk_h

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
	chunk_keylength = 32,
	chunk_maxlength = 1024
};

typedef struct chunk_t chunk_t;

chunk_t*			chunk_create(const uint8_t* data, uint16_t length, uint8_t depth, bool owner, const uint8_t* key);
chunk_t*			chunk_create_copy(const uint8_t* data, uint16_t length, uint8_t depth, const uint8_t* key);

void				chunk_free(chunk_t* chunk);

const uint8_t*		chunk_key(const chunk_t* chunk);
const uint8_t*		chunk_data(const chunk_t* chunk);
uint8_t				chunk_depth(const chunk_t* chunk);
uint16_t			chunk_length(const chunk_t* chunk);

uint8_t				chunk_depth_from_key(const uint8_t *key);
uint16_t			chunk_length_from_key(const uint8_t *key);

const uint8_t*		chunk_key_from_hex(const char* hex);
const char*			chunk_hex_from_key(const uint8_t* key);

uint8_t*			chunk_key_from_hex_r(const char *hex, uint8_t *key);
char*				chunk_hex_from_key_r(const uint8_t* key, char *hex);

bool				chunk_key_valid(const chunk_t* chunk, const uint8_t *key);

#ifdef __cplusplus
}
#endif

#endif // __chunk_h
