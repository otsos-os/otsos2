/*
 * Copyright (c) 2026, otsos team
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef MM_H
#define MM_H

/*
 * otsos2 kernel memory management subsystem.
 *
 * Architecture inspired by FreeBSD's VM system:
 *
 *   mm/
 *     mm.h              — this umbrella header
 *     kmem.h/.c         — kernel heap allocator (kmem_alloc/kmem_free/kmem_calloc/kmem_realloc)
 *     uma.h/.c          — UMA zone (slab) allocator with type tracking
 *     vm/
 *       pmap.h/.c       — physical-to-virtual mapping (x86_64 page tables)
 *       vm_page.h/.c    — physical page frame allocator
 *       vm_map.h/.c     — virtual memory map (per-process VMA tracking)
 *       vm_object.h/.c  — VM objects (anonymous / file-backed memory)
 *
 * Initialization order:
 *   1. kmem_init()      — kernel heap (called early in kmain)
 *   2. pmap_init()      — page table subsystem
 *   3. vm_page_init()   — physical page allocator
 *   4. uma_init()       — zone allocator (on top of kmem)
 */

#include <mm/kmem.h>
#include <mm/uma.h>
#include <mm/vm/pmap.h>
#include <mm/vm/vm_page.h>
#include <mm/vm/vm_map.h>
#include <mm/vm/vm_object.h>

#endif
