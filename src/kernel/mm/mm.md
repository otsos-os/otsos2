# Memory Management Subsystem (mm/)

The `mm/` directory implements the kernel's memory management.

## Structure

```
mm/
  mm.h              — umbrella header (includes all sub-headers)
  mm.md             — this documentation
  kmem.h/.c         — kernel heap allocator (kmem_alloc/free/calloc/realloc)
  uma.h/.c          — UMA zone (slab) allocator with type tracking
  vm/
    pmap.h/.c       — physical-to-virtual mapping (x86_64 page tables)
    vm_page.h/.c    — physical page frame allocator
    vm_map.h/.c     — virtual memory map (per-process VMA tracking)
    vm_object.h/.c  — VM objects (anonymous / file-backed memory)
```

## Modules

### kmem — Kernel Heap

The basic kernel heap allocator. Moved from `mlibc/memory.c`.

- `kmem_init()` — initialize heap (called early in kmain)
- `kmem_alloc(size)` / `kmem_free(ptr)` — basic allocation
- `kmem_calloc(nmemb, size)` — zeroed allocation
- `kmem_realloc(ptr, size)` — resize
- `kmem_alloc_aligned(size, align)` — aligned allocation (for page tables, DMA)
- Redzone + poison checks for debugging
- `kmem_dump()` — heap statistics

Backward-compatible aliases: `kmalloc`, `kfree`, `kcalloc`, `krealloc`,
`kmalloc_aligned`, `init_heap`, etc.

### uma — Zone (Slab) Allocator

UMA (Universal Memory Allocator). Provides type-tracked,
pre-sized allocation zones for frequently-allocated objects.

- `uma_zcreate(name, size, align, flags)` — create a zone
- `uma_zalloc(zone, flags)` — allocate from zone
- `uma_zfree(zone, item)` — free to zone
- `uma_zdestroy(zone)` — destroy zone
- Pre-created zones for common sizes (16–4096 bytes)
- `M_ZERO` flag for zeroed allocation

### vm/pmap — Page Table Management

x86_64 four-level page table operations. Moved from `kernel/mmu.c`.

- `pmap_init()` — enable NXE, save kernel CR3
- `pmap_enter(vaddr, paddr, flags)` — map a page
- `pmap_remove(vaddr)` — unmap a page
- `pmap_extract(vaddr)` — virtual-to-physical translation
- `pmap_create()` — create new address space (copies kernel mappings)
- `pmap_clone(src_cr3)` — deep-copy user space (for fork)
- `pmap_destroy(cr3)` — free all user pages and page tables
- `pmap_clear_user_range(start, end)` — strip USER bit from kernel pages

Backward-compatible aliases: `mmu_init`, `mmu_map_page`, `mmu_unmap_page`,
`mmu_virt_to_phys`, etc.

### vm/vm_page — Physical Page Allocator

Tracks physical pages and manages a free list.

- `vm_page_init(start, end)` — initialize from available physical memory range
- `vm_page_alloc(flags)` — allocate a physical page
- `vm_page_free(page)` — return page to free list
- `vm_page_ref(page)` / `vm_page_unref(page)` — reference counting
- Static page array (no heap allocation needed for early boot)

### vm/vm_map — Standalone Virtual Address Spaces

`vm_map_t` owns one address-space interval independently of `process_t`.
Entries are indexed by an address-ordered red-black tree and carry an in-order
thread for safe client iteration.  A process embeds one map, but pmap, fault,
fork and mapping mechanics all take `vm_map_t *` directly.

- `vm_map_init(map, min, max)` — initialise an embedded map
- `vm_map_find_free(map, length)` — choose a free page-aligned range
- `vm_map_insert(map, ...)` / `vm_map_remove_range(map, ...)` — modify ranges
- `vm_map_lookup(map, addr)` — O(log n) range lookup
- `vm_map_fork(parent, child)` — construct private shadows or shared aliases
- `vm_map_fault(map, addr, error)` — resolve pager and CoW faults

### vm/vm_object — VM Objects

Reference-counted backing objects use a fixed four-level, 512-way radix index
of `vm_page_t` ownership instead of a resizeable flat physical-address array.
Radix nodes come from a dedicated UMA zone, avoiding page-run waste.

- `vm_object_create(type, size, backing)` — create a backing object
- `vm_object_ref(obj)` / `vm_object_unref(obj)` — lifetime control
- Types: `VM_OBJ_ANON`, `VM_OBJ_FILE`, `VM_OBJ_GEM`, `VM_OBJ_SHM`

## Compatibility

Old headers (`kernel/mmu.h`, `mlibc/memory.h`) are now thin
wrappers that `#include` the new mm/ headers.
Existing code continues to work unchanged; new code should use the new headers
directly.

## Initialization Order

```
kmain() {
    kmem_init();     // kernel heap
    pmap_init();     // page tables
    vm_page_init();  // physical page allocator
    uma_init();      // zone allocator
}
```
