/*
 * kvfs_stdio.c
 */

#ifdef __linux__
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include <kvfs/kvfs.h>
#include <kvfs/chunk.h>

typedef struct kvfs_read_cookie_t {
	kvfs_store_t*				store;
	uint8_t*					buffer;
	const uint8_t*				key;
	struct kvfs_read_cookie_t*	next;
	chunk_t*					chunk;
	uint16_t					length;
	uint16_t					offset;
} kvfs_read_cookie_t;

typedef struct kvfs_write_cookie_t {
	kvfs_store_t*				store;
	uint8_t*					buffer;
	uint8_t*					keybuffer;
	size_t						keybuffer_length;
	size_t						keybuffer_offset;
	uint16_t					offset;
	uint8_t						depth;
} kvfs_write_cookie_t;

static kvfs_read_cookie_t*
				kvfs_stdio_reader_alloc(kvfs_store_t* store, const uint8_t* key);
static void		kvfs_stdio_reader_free(void* cookie);
static int		kvfs_stdio_reader_read(void* cookie, char* buf, int size);
static ssize_t	kvfs_stdio_reader_wrapper(void* cookie, char* buf, size_t size);
static int		kvfs_stdio_reader_close(void* cookie);

static kvfs_write_cookie_t*
				kvfs_stdio_writer_alloc(kvfs_store_t* store, uint8_t depth);
static void		kvfs_stdio_writer_free(void* cookie);
static int		kvfs_stdio_writer_write(void* cookie, const char* buf, int size);
static ssize_t	kvfs_stdio_writer_wrapper(void* cookie, const char* buf, size_t size);
static int		kvfs_stdio_writer_close(void* cookie);

//---------------------------------------------------------------------

FILE* kvfs_fopen_read(kvfs_store_t* store, const uint8_t* key)
{
	if (!key) {
		errno = EINVAL;
		return NULL;
	}

	kvfs_read_cookie_t* cookie = kvfs_stdio_reader_alloc(store, key);
	if (!cookie) {
		return NULL;
	}

#ifdef __linux__
	static cookie_io_functions_t funcs = {
		.read  = kvfs_stdio_reader_wrapper,
		.write = NULL,
		.seek  = NULL,
		.close = kvfs_stdio_reader_close
	};

	return fopencookie(cookie, "r", funcs);
#else
	return funopen(cookie, kvfs_stdio_reader_read, NULL, NULL, kvfs_stdio_reader_close);
#endif
}

FILE* kvfs_fopen_write(kvfs_store_t* store)
{
	if (!store) {
		errno = EINVAL;
		return NULL;
	}

	kvfs_write_cookie_t* cookie = kvfs_stdio_writer_alloc(store, 0);
	if (!cookie) {
		return NULL;
	}

#ifdef __linux__
	static cookie_io_functions_t funcs = {
		.read  = NULL,
		.write = kvfs_stdio_writer_wrapper,
		.seek  = NULL,
		.close = kvfs_stdio_writer_close
	};
	return fopencookie(cookie, "w", funcs);
#else
	return funopen(cookie, NULL, kvfs_stdio_writer_write, NULL, kvfs_stdio_writer_close);
#endif
}

//---------------------------------------------------------------------

static kvfs_read_cookie_t* kvfs_stdio_reader_alloc(kvfs_store_t* store, const uint8_t* key)
{
	kvfs_read_cookie_t* cookie = malloc(sizeof *cookie);
	uint8_t* buffer = malloc(chunk_maxlength);

	if (!cookie || !buffer) {
		goto error;
	}

	cookie->store = store;
	cookie->buffer = buffer;
	cookie->key = key;
	cookie->next = NULL;
	cookie->chunk = kvfs_get(cookie->store, cookie->key);
	if (!cookie->chunk) {
		goto error;
	}
	cookie->length = chunk_length(cookie->chunk);
	cookie->offset = 0;

	return cookie;

error:
	free(buffer);
	free(cookie);
	return NULL;
}

static void	kvfs_stdio_reader_free(void* _cookie)
{
	kvfs_read_cookie_t* cookie = _cookie;
	if (cookie) {
		if (cookie->chunk) {
			chunk_free(cookie->chunk);
		}
		free(cookie->buffer);
		free(cookie);
	}
}

static int kvfs_stdio_reader_branch(kvfs_read_cookie_t* cookie, char *buf, int size)
{
	while (cookie->next != NULL || cookie->offset < cookie->length) {

		if (!cookie->next) {
			const uint8_t* key = chunk_data(cookie->chunk) + cookie->offset;
			cookie->next = kvfs_stdio_reader_alloc(cookie->store, key);
			cookie->offset += chunk_keylength;
		}

		if (cookie->next) {
			int r = kvfs_stdio_reader_read(cookie->next, buf, size);
			if (cookie->next->offset == cookie->next->length || r == 0) {
				kvfs_stdio_reader_free(cookie->next);
				cookie->next = 0;
			}
			if (r > 0) {
				return r;
			} else if (r < 0) {
				return -1;
			}
		} else {
			return -1;
		}
	}

	return 0;
}

static int kvfs_stdio_reader_leaf(kvfs_read_cookie_t* cookie, char *buf, int size)
{
	if (cookie->offset == cookie->length) {
		return 0;
	} else if (cookie->offset > cookie->length) {
		return -1;
	} else  {
		/* for leaf nodes, copy the smaller of the
		 * data available or the destination buffer */
		int avail = cookie->length - cookie->offset;
		int tocopy = avail < size ? avail : size;

		assert(tocopy > 0);
		memcpy(buf, chunk_data(cookie->chunk) + cookie->offset, tocopy);
		cookie->offset += tocopy;
		return tocopy;
	}
}

static int kvfs_stdio_reader_read(void* _cookie, char* buf, int size)
{
	kvfs_read_cookie_t* cookie = _cookie;

	if (chunk_depth(cookie->chunk) == 0) {
		return kvfs_stdio_reader_leaf(cookie, buf, size);
	} else {
		return kvfs_stdio_reader_branch(cookie, buf, size);
	}
}

static ssize_t kvfs_stdio_reader_wrapper(void* cookie, char* buf, size_t size)
{
	size_t read = 0;

	do {
		ssize_t n = kvfs_stdio_reader_read(cookie, buf + read, size - read);
		if (n == 0) {
			break;
		} else if (n < 0) {
			return -1;
		}
		read += n;
	} while (read < size);

	return read;
}

static int kvfs_stdio_reader_close(void* cookie)
{
	kvfs_stdio_reader_free(cookie);

	return 0;
}

//---------------------------------------------------------------------

static kvfs_write_cookie_t*
				kvfs_stdio_writer_alloc(kvfs_store_t* store, uint8_t depth)
{
	kvfs_write_cookie_t* cookie = malloc(sizeof *cookie);
	uint8_t* buffer = malloc(chunk_maxlength);
	uint8_t* keybuffer = malloc(chunk_maxlength);

	if (!cookie || !buffer || !keybuffer) {
		goto error;
	}

	cookie->store = store;
	cookie->buffer = buffer;
	cookie->keybuffer = keybuffer;
	cookie->keybuffer_length = chunk_maxlength;
	cookie->keybuffer_offset = 0;
	cookie->offset = 0;
	cookie->depth = depth;

	return cookie;

error:
	free(keybuffer);
	free(buffer);
	free(cookie);
	return NULL;
}

static void	kvfs_stdio_writer_free(void* _cookie)
{
	kvfs_write_cookie_t* cookie = _cookie;

	free(cookie->keybuffer);
	free(cookie->buffer);
	free(cookie);
}

static int kvfs_stdio_writer_save_key(kvfs_write_cookie_t* cookie, const uint8_t* key)
{
	uint8_t* buf = cookie->keybuffer;
	if (cookie->keybuffer_offset + chunk_keylength > cookie->keybuffer_length) {
		cookie->keybuffer_length *= 2;
		buf = realloc(buf, cookie->keybuffer_length);
		if (!buf) {
			free(cookie->keybuffer);
			return -1;
		}
		cookie->keybuffer = buf;
	}

	memcpy(cookie->keybuffer + cookie->keybuffer_offset, key, chunk_keylength);
	cookie->keybuffer_offset += chunk_keylength;

	return 0;
}

int kvfs_stdio_writer_emit(kvfs_write_cookie_t* cookie, const uint8_t* buffer)
{
	int r = 0;

	chunk_t* chunk = chunk_create(buffer, cookie->offset, cookie->depth, false, NULL);
	if (!chunk) {
		r = -1;
		goto cleanup;
	}

	r = kvfs_put(cookie->store, chunk);
	if (r < 0) {
		goto cleanup;
	}

	r = kvfs_stdio_writer_save_key(cookie, chunk_key(chunk));
	if (r < 0) {
		goto cleanup;
	}

	r = cookie->offset;
	cookie->offset = 0;

cleanup:
	if (chunk) {
		chunk_free(chunk);
	}

	return r;
}

static int kvfs_stdio_writer_write(void* _cookie, const char* buf, int size)
{
	kvfs_write_cookie_t* cookie = _cookie;

	/* how much fits in the current chunk */
	int avail = chunk_maxlength - cookie->offset;

	/* how much do we actually copy */
	int amount = avail < size ? avail : size;

	if (cookie->offset == 0 && amount == (int)chunk_maxlength) {
		/* if it's a whole chunk, avoid the copy */
		cookie->offset = chunk_maxlength;
		return kvfs_stdio_writer_emit(cookie, buf);
	} else {
		memcpy(cookie->buffer + cookie->offset, buf, amount);
		cookie->offset += amount;
		assert(cookie->offset <= chunk_maxlength);
		if (cookie->offset == chunk_maxlength) {
			return kvfs_stdio_writer_emit(cookie, cookie->buffer);
		} else {
			return amount;
		}
	}
}

/* required with glibc because fopencookie can't cope with short writes,
 * but also provides a handy helper for the close function since it does
 * output the whole buffer */
static ssize_t kvfs_stdio_writer_wrapper(void* cookie, const char* buf, size_t size)
{
	size_t written = 0;

	do {
		ssize_t n = kvfs_stdio_writer_write(cookie, buf + written, size - written);
		if (n <= 0) {
			return 0;
		}
		written += n;
	} while (written < size);

	return written;
}

static int kvfs_stdio_writer_close(void* _cookie)
{
	kvfs_write_cookie_t* cookie = _cookie;
	int r = 0;

	if (cookie->offset) {
		r = kvfs_stdio_writer_emit(cookie, cookie->buffer);
		if (r < 0) {
			return r;
		}
	}

	/* if there's more than one key in the keybuffer then
	 * the keybuffer needs to be serialised, too */
	if (cookie->keybuffer_offset > chunk_keylength) {
		// TODO: error checks
		kvfs_write_cookie_t* next = kvfs_stdio_writer_alloc(cookie->store, cookie->depth + 1);
		if (next) {
			r = kvfs_stdio_writer_wrapper(next, cookie->keybuffer, cookie->keybuffer_offset);
			r = kvfs_stdio_writer_close(next);
		} else {
			r = -1;
		}
	}

	kvfs_stdio_writer_free(cookie);

	return r;
}
