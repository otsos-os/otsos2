# Memory Management

OTSOS implements a dynamic memory allocator (Heap) for the kernel, located in `src/mlibc/memory.c`.

## Architecture
The allocator uses a **Doubly-Linked List** of blocks. Each block has a header (`header_t`) that contains metadata about the block's size, status, and neighbors.

### Header Structure
```c
typedef struct header {
  unsigned int magic;    // 0x48454150 ("HEAP")
  unsigned int is_free;  // 1 if free, 0 if allocated
  unsigned long size;    // Size of the data area
  struct header *next;   // Next block in memory
  struct header *prev;   // Previous block in memory
} header_t;
```

## Public API

### `void *kmalloc(unsigned long size)`
Allocates `size` bytes of memory.
- Uses **First Fit** algorithm to find a free block.
- Automatically splits larger blocks to minimize waste.
- Returns a 16-byte aligned pointer.

### `void kfree(void *ptr)`
Frees the memory block pointed to by `ptr`.
- Performs **O(1) Coalescing**: Merges the freed block with its immediate neighbors if they are also free.
- Validates the block using the `magic` field to detect corruption or double-frees.

### `void *krealloc(void *ptr, unsigned long size)`
Resizes a previous allocation.
- Tries to expand the block "in-place" if the next block is free and large enough.
- Otherwise, performs a new allocation, copies data, and frees the old pointer.

### `void *kmalloc_aligned(unsigned long size, unsigned long align)`
Allocates memory with a specific alignment (e.g., 4096 for page tables).
- Correctly splits blocks to ensure the returned pointer satisfies the alignment requirement.

### `void *kcalloc(unsigned long nmemb, unsigned long size)`
Allocates memory for an array and initializes it to zero.

## Diagnostics

### `unsigned long kget_free_memory()`
Returns the total amount of free memory currently available in the heap.

### `void kheap_dump()`
Output a detailed map of all heap blocks to the serial port (COM1). Useful for debugging memory leaks and fragmentation.

## Internal Mechanisms
- **Alignment**: All allocations are aligned to 16 bytes.
- **Integrity**: Every block is guarded by the `HEAP_MAGIC` constant. If a header is overwritten, the system will report "HEAP CORRUPTION DETECTED".
