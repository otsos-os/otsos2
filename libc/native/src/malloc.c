/* !DEFINES!

$define %type malloc_block as heap block header
$define %func malloc as function with args size_t
$define %func free as procedure with args void *

*/

/* !SPACE!

$space %internal align_up, find_free, split_block, more_memory, coalesce
$space %export malloc, free, calloc, realloc

*/

#include <errno.h>
#include <native.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "private.h"

#define MALLOC_ALIGN	16UL
#define MALLOC_CHUNK	65536UL

struct malloc_block {
	size_t			size;
	int			free;
	struct malloc_block	*next;
	struct malloc_block	*prev;
};

static struct malloc_block	*heap_head;
static struct malloc_block	*heap_tail;

static size_t
align_up(size_t value)
{
	return ((value + MALLOC_ALIGN - 1) & ~(MALLOC_ALIGN - 1));
}

static struct malloc_block *
find_free(size_t size)
{
	struct malloc_block	*block;

	block = heap_head;
	while (block) {
		if (block->free && block->size >= size) {
			return (block);
		}
		block = block->next;
	}
	return (NULL);
}

static void
split_block(struct malloc_block *block, size_t size)
{
	struct malloc_block	*next;
	char			*base;

	if (block->size < size + sizeof(*next) + MALLOC_ALIGN) {
		return;
	}

	base = (char *)(block + 1);
	next = (struct malloc_block *)(base + size);
	next->size = block->size - size - sizeof(*next);
	next->free = 1;
	next->next = block->next;
	next->prev = block;
	if (next->next) {
		next->next->prev = next;
	} else {
		heap_tail = next;
	}
	block->next = next;
	block->size = size;
}

static struct malloc_block *
more_memory(size_t size)
{
	struct mem_map_args	args;
	struct malloc_block	*block;
	size_t			total;
	long			ret;

	total = align_up(size + sizeof(*block));
	if (total < MALLOC_CHUNK) {
		total = MALLOC_CHUNK;
	}

	memset(&args, 0, sizeof(args));
	args.length = total;
	args.prot = API_MAP_READ | API_MAP_WRITE;
	args.flags = API_MAP_PRIVATE | API_MAP_ANON;
	args.fd = -1;

	ret = __syscall1(CALL_MEM_MAP, (long)&args);
	if (ret < 0) {
		errno = (int)-ret;
		return (NULL);
	}

	block = (struct malloc_block *)ret;
	block->size = total - sizeof(*block);
	block->free = 1;
	block->next = NULL;
	block->prev = heap_tail;
	if (heap_tail) {
		heap_tail->next = block;
	} else {
		heap_head = block;
	}
	heap_tail = block;
	return (block);
}

static void
coalesce(struct malloc_block *block)
{
	char	*end;

	if (block->next && block->next->free) {
		end = (char *)(block + 1) + block->size;
		if (end == (char *)block->next) {
			block->size += sizeof(*block) + block->next->size;
			block->next = block->next->next;
			if (block->next) {
				block->next->prev = block;
			} else {
				heap_tail = block;
			}
		}
	}

	if (block->prev && block->prev->free) {
		coalesce(block->prev);
	}
}

void *
malloc(size_t size)
{
	struct malloc_block	*block;

	if (size == 0) {
		size = 1;
	}
	size = align_up(size);
	block = find_free(size);
	if (!block) {
		block = more_memory(size);
		if (!block) {
			return (NULL);
		}
	}
	split_block(block, size);
	block->free = 0;
	return ((void *)(block + 1));
}

void
free(void *ptr)
{
	struct malloc_block	*block;

	if (!ptr) {
		return;
	}
	block = ((struct malloc_block *)ptr) - 1;
	block->free = 1;
	coalesce(block);
}

void *
calloc(size_t nmemb, size_t size)
{
	void	*ptr;
	size_t	total;

	if (size != 0 && nmemb > (SIZE_MAX / size)) {
		errno = ENOMEM;
		return (NULL);
	}
	total = nmemb * size;
	ptr = malloc(total);
	if (!ptr) {
		return (NULL);
	}
	memset(ptr, 0, total);
	return (ptr);
}

void *
realloc(void *ptr, size_t size)
{
	struct malloc_block	*block;
	void			*next;
	size_t			copy;

	if (!ptr) {
		return (malloc(size));
	}
	if (size == 0) {
		free(ptr);
		return (NULL);
	}

	block = ((struct malloc_block *)ptr) - 1;
	if (block->size >= size) {
		split_block(block, align_up(size));
		return (ptr);
	}

	next = malloc(size);
	if (!next) {
		return (NULL);
	}
	copy = block->size;
	if (copy > size) {
		copy = size;
	}
	memcpy(next, ptr, copy);
	free(ptr);
	return (next);
}
