#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>

#include <kvfs/drivers/file.h>
#include <kvfs/chunk.h>
#include <kvfs/private.h>

typedef struct kvfs_file_context_t {
	char		*path;
	size_t		path_length;
} kvfs_file_context_t;

/* buffer must be at least _POSIX_PATH_MAX long */
static void hex_path(kvfs_file_context_t* context, const uint8_t* key, char* buffer)
{
	snprintf(buffer, _POSIX_PATH_MAX, "%s/%s.kvfs", context->path, chunk_hex_from_key(key));
}

static chunk_t* kvfs_file_get(kvfs_store_t* store, const uint8_t* key)
{
	char path[_POSIX_PATH_MAX];
	uint8_t* buffer;
	uint8_t depth;
	ssize_t length;
	int fd;

	buffer = malloc(chunk_maxlength);
	if (!buffer) {
		goto error;
	}

	hex_path(store->context, key, path);
	fd = open(path, O_RDONLY);
	if (fd == -1) {
		goto error;
	}

	length = read(fd, buffer, chunk_maxlength);
	if (length < 0) {
		// TODO better error / short read check
		goto error;
	}

	depth = chunk_depth_from_key(key);
	return chunk_create(buffer, length, depth, true, key);

error:
	free(buffer);
	return NULL;
}

static int kvfs_file_put(kvfs_store_t* store, chunk_t* chunk)
{
	char path[_POSIX_PATH_MAX];
	const uint8_t* buffer;
	ssize_t length;
	int fd;

	kvfs_file_context_t* context = store->context;
	hex_path(context, chunk_key(chunk), path);
	fd = open(path, O_WRONLY | O_CREAT, 0644);
	if (fd == -1) {
		return -1;
	}

	buffer = chunk_data(chunk);
	length = chunk_length(chunk);

	// TODO better short write check
	if (write(fd, buffer, length) != length) {
		return -1;
	} else {
		return 0;
	}
}

static void kvfs_file_free(kvfs_store_t* store)
{
	if (store->context) {
		kvfs_file_context_t* context = store->context;
		free(context->path);
		free(context);
	}
	free(store);
}

static const char *kvfs_file_error(kvfs_store_t* store)
{
	(void)store;
	return NULL;
}

kvfs_store_t* kvfs_create_file(const char* path)
{
	kvfs_store_t* store = NULL;
	kvfs_file_context_t* context = NULL;
	char *path_copy = NULL;

	if (!path) {
		errno = EINVAL;
		return NULL;
	}

	store = malloc(sizeof *store);
	context = malloc(sizeof *context);
	path_copy = strdup(path);

	if (!store || !context || !path_copy) {
		free(path_copy);
		free(context);
		free(store);
		return NULL;
	}

	store->context = context;
	context->path = path_copy;
	context->path_length = strlen(path);
	store->get = kvfs_file_get;
	store->put = kvfs_file_put;
	store->free = kvfs_file_free;
	store->error = kvfs_file_error;

	return store;
}
