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

/* !DEFINES!

$define %type u32 as 32 bit unsigned
$define %type u64 as 64 bit unsigned
$define %type int as 32 bit signed
$define %type dma_tag_t as opaque pointer to a device DMA constraint set
$define %type dma_tag as struct with one resolved constraint set

$define %func dma_pow2 as function with args u64
$define %func dma_tighter as function with args u64, u64
$define %func dma_tag_resolve as function with args dma_tag_t, u64, u64, u64, u64, u64, u32, u64, struct dma_tag *
$define %func dma_tag_root_create as function with args void
$define %func dma_tag_teardown as procedure with args void
$define %func dma_addr_ok as function with args const struct dma_tag *, u64, u64
$define %func dma_tag_root as function with args void
$define %func dma_tag_create as function with args dma_tag_t, u64, u64, u64, u64, u64, u32, u64, u32, const char *, dma_tag_t *
$define %func dma_tag_destroy as procedure with args dma_tag_t
$define %func dma_tag_name as function with args dma_tag_t

*/

/* !SPACE!

$space %internal dma_pow2, dma_tighter, dma_tag_resolve
$space %export dma_tag_root_create, dma_tag_teardown, dma_addr_ok
$space %export dma_tag_root, dma_tag_create, dma_tag_destroy, dma_tag_name

*/

#include <mm/dma/dma_internal.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>

static int
dma_pow2(u64 value)
{
	if (value == 0) {
		return (0);
	}
	return ((value & (value - 1)) == 0);
}


static u64
dma_tighter(u64 parent, u64 child)
{
	if (child == 0) {
		return (parent);
	}
	if (parent == 0) {
		return (child);
	}
	return (child < parent ? child : parent);
}

int
dma_tag_root_create(void)
{
	struct dma_tag	*root;

	root = &dma_g.root;
	memset(root, 0, sizeof(*root));
	root->alignment = 1;
	root->boundary = DMA_BOUNDARY_NONE;
	root->lowaddr = 0;
	root->highaddr = 0xFFFFFFFFFFFFFFFFULL;
	root->maxsize = 0xFFFFFFFFFFFFFFFFULL;
	root->maxsegsz = 0xFFFFFFFFFFFFFFFFULL;
	root->nsegments = DMA_MAX_SEGMENTS;
	root->parent = NULL;
	root->children = 0;
	root->live = 1;
	strncpy(root->name, "root", DMA_TAG_NAME_MAX - 1);
	root->name[DMA_TAG_NAME_MAX - 1] = '\0';
	return (0);
}

int
dma_addr_ok(const struct dma_tag *tag, u64 phys, u64 len)
{
	u64	last;

	if (tag == NULL || len == 0) {
		return (0);
	}
	if (phys > 0xFFFFFFFFFFFFFFFFULL - (len - 1)) {
		return (0);
	}
	last = phys + len - 1;
	if (phys < tag->lowaddr || last > tag->highaddr) {
		return (0);
	}
	return (1);
}

static int
dma_tag_resolve(dma_tag_t parent, u64 alignment, u64 boundary, u64 lowaddr,
    u64 highaddr, u64 maxsize, u32 nsegments, u64 maxsegsz,
    struct dma_tag *out)
{
	if (alignment == 0) {
		alignment = 1;
	}
	if (!dma_pow2(alignment)) {
		return (-1);
	}
	if (boundary != DMA_BOUNDARY_NONE && !dma_pow2(boundary)) {
		return (-1);
	}

	memset(out, 0, sizeof(*out));
	out->alignment = alignment > parent->alignment ? alignment :
	    parent->alignment;
	out->boundary = dma_tighter(parent->boundary, boundary);
	out->lowaddr = lowaddr > parent->lowaddr ? lowaddr : parent->lowaddr;
	if (highaddr == DMA_HIGHADDR_ANY) {
		out->highaddr = parent->highaddr;
	} else {
		out->highaddr = highaddr < parent->highaddr ? highaddr :
		    parent->highaddr;
	}
	out->maxsize = dma_tighter(parent->maxsize, maxsize);
	out->maxsegsz = dma_tighter(parent->maxsegsz, maxsegsz);
	out->nsegments = (u32)dma_tighter(parent->nsegments, nsegments);
	if (out->nsegments == 0 || out->nsegments > DMA_MAX_SEGMENTS) {
		out->nsegments = DMA_MAX_SEGMENTS;
	}

	
	if (out->lowaddr > out->highaddr || out->maxsize == 0 ||
	    out->maxsegsz == 0) {
		return (-1);
	}
	return (0);
}

dma_tag_t
dma_tag_root(void)
{
	if (!dma_g.ready) {
		return (NULL);
	}
	return (&dma_g.root);
}

int
dma_tag_create(dma_tag_t parent, u64 alignment, u64 boundary, u64 lowaddr,
    u64 highaddr, u64 maxsize, u32 nsegments, u64 maxsegsz, u32 flags,
    const char *name, dma_tag_t *tagp)
{
	struct dma_tag	resolved;
	struct dma_tag	*tag;
	u64		live;

	if (tagp == NULL || name == NULL) {
		return (-1);
	}
	*tagp = NULL;
	if (!dma_g.ready) {
		return (-1);
	}
	if (parent == NULL) {
		parent = &dma_g.root;
	}
	if (!parent->live) {
		return (-1);
	}
	if (dma_tag_resolve(parent, alignment, boundary, lowaddr, highaddr,
	    maxsize, nsegments, maxsegsz, &resolved) != 0) {
		__atomic_fetch_add(&dma_g.fail_constraint, 1,
		    __ATOMIC_RELAXED);
		drivers_log("[DMA] tag '%s': contradictory constraints "
		    "(align=%u boundary=%u low=%p high=%p)\n", name,
		    (u32)alignment, (u32)boundary, (void *)lowaddr,
		    (void *)highaddr);
		return (-1);
	}

	tag = kmem_alloc(sizeof(*tag));
	if (tag == NULL) {
		__atomic_fetch_add(&dma_g.fail_nomem, 1, __ATOMIC_RELAXED);
		return (-1);
	}
	*tag = resolved;
	tag->parent = parent;
	tag->flags = flags;
	tag->live = 1;
	strncpy(tag->name, name, DMA_TAG_NAME_MAX - 1);
	tag->name[DMA_TAG_NAME_MAX - 1] = '\0';

	spin_lock(&dma_g.lock);
	parent->children++;
	dma_g.tags_live++;
	live = dma_g.tags_live;
	spin_unlock(&dma_g.lock);
	dma_peak_update(&dma_g.tags_peak, live);

	*tagp = tag;
	return (0);
}

void
dma_tag_destroy(dma_tag_t tag)
{
	dma_tag_t	parent;
	u32		children;

	if (tag == NULL || tag == &dma_g.root) {
		return;
	}

	spin_lock(&dma_g.lock);
	children = tag->children;
	if (children != 0) {
		spin_unlock(&dma_g.lock);
		drivers_log("[DMA] tag '%s' destroy refused: %u live "
		    "children\n", tag->name, children);
		return;
	}
	parent = tag->parent;
	if (parent != NULL && parent->children != 0) {
		parent->children--;
	}
	if (dma_g.tags_live != 0) {
		dma_g.tags_live--;
	}
	tag->live = 0;
	spin_unlock(&dma_g.lock);

	kmem_free(tag);
}

const char *
dma_tag_name(dma_tag_t tag)
{
	if (tag == NULL) {
		return ("<null>");
	}
	return (tag->name);
}

void
dma_tag_teardown(void)
{
	spin_lock(&dma_g.lock);
	dma_g.root.children = 0;
	dma_g.root.live = 0;
	spin_unlock(&dma_g.lock);
}
