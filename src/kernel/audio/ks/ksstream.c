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

$define %type ks_ring_t as ring buffer with wrap-aware cursor arithmetic
$define %type u64 as 64 bit unsigned

$define %func ks_ring_init as procedure with args ks_ring_t *, u64
$define %func ks_ring_reset as procedure with args ks_ring_t *
$define %func ks_ring_bytes_available as function with args const ks_ring_t *
$define %func ks_ring_bytes_free as function with args const ks_ring_t *
$define %func ks_ring_write as function with args ks_ring_t *, const u8 *, u64
$define %func ks_ring_read as function with args ks_ring_t *, u8 *, u64
*/

/* !SPACE!

$space %export ks_ring_init, ks_ring_set_length, ks_ring_reset
$space %export ks_ring_bytes_available, ks_ring_bytes_free
$space %export ks_ring_write, ks_ring_read

*/

#include <kernel/audio/ks/ksstream.h>

void
ks_ring_init(ks_ring_t *ring, u64 length)
{
	if (ring == NULL) {
		return;
	}
	memset(ring, 0, sizeof(*ring));
	ks_ring_set_length(ring, length);
}

void
ks_ring_set_length(ks_ring_t *ring, u64 length)
{
	if (ring == NULL) {
		return;
	}
	ring->length = length;
	if (ring->write_cursor >= ring->length) {
		ring->write_cursor = 0;
	}
	if (ring->read_cursor >= ring->length) {
		ring->read_cursor = 0;
	}
}

void
ks_ring_reset(ks_ring_t *ring)
{
	if (ring == NULL) {
		return;
	}
	ring->write_cursor = 0;
	ring->read_cursor = 0;
}

u64
ks_ring_bytes_available(const ks_ring_t *ring)
{
	if (ring == NULL || ring->length == 0) {
		return (0);
	}
	return (ring->write_cursor - ring->read_cursor);
}

u64
ks_ring_bytes_free(const ks_ring_t *ring)
{
	if (ring == NULL || ring->length == 0) {
		return (0);
	}
	return (ring->length - ks_ring_bytes_available(ring));
}

u64
ks_ring_write(ks_ring_t *ring, const u8 *src, u64 count)
{
	u64	free_count, part, index, written;

	if (ring == NULL || ring->base == NULL || src == NULL ||
	    ring->length == 0 || count == 0) {
		return (0);
	}
	free_count = ks_ring_bytes_free(ring);
	if (count > free_count) {
		count = free_count;
	}
	written = 0;
	while (written < count) {
		index = ring->write_cursor % ring->length;
		part = count - written;
		if (part > ring->length - index) {
			part = ring->length - index;
		}
		memcpy(ring->base + index, src + written, part);
		ring->write_cursor += part;
		written += part;
	}
	return (written);
}

u64
ks_ring_read(ks_ring_t *ring, u8 *dst, u64 count)
{
	u64	avail, part, index, consumed;

	if (ring == NULL || ring->base == NULL || dst == NULL ||
	    ring->length == 0 || count == 0) {
		return (0);
	}
	avail = ks_ring_bytes_available(ring);
	if (count > avail) {
		count = avail;
	}
	consumed = 0;
	while (consumed < count) {
		index = ring->read_cursor % ring->length;
		part = count - consumed;
		if (part > ring->length - index) {
			part = ring->length - index;
		}
		memcpy(dst + consumed, ring->base + index, part);
		ring->read_cursor += part;
		consumed += part;
	}
	return (consumed);
}
