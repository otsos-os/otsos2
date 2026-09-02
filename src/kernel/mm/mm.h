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
 * otsos2 memory ownership flows strictly in one direction:
 *
 *   bootmem -> vm_phys -> vm_page -> UMA -> kmem -> vm_object/vm_map
 *
 * bootmem discovers RAM and remains available for boot-only metadata. vm_phys
 * owns segmented physical free runs; vm_page adds reference, wire and paging
 * queue policy; UMA creates slab zones directly from wired vm_page runs; kmem
 * selects UMA size classes or a direct contiguous VM run. vm_object and vm_map
 * are clients of that stack, never allocator backends.
 *
 * Initialization order:
 *   1. bootmem_init()
 *   2. vm_page_startup() (initializes vm_phys internally)
 *   3. uma_init()
 *   4. kmem_init()
 *   5. vm_object_init() and VM-map clients
 */

#include <mm/kmem.h>
#include <mm/uma.h>
#include <mm/vm/pmap.h>
#include <mm/vm/vm_page.h>
#include <mm/vm/vm_map.h>
#include <mm/vm/vm_object.h>

#endif
