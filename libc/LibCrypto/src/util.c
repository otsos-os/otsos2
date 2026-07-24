/* !DEFINES!

$define %type size_t as object size
$define %func lc_wipe as procedure with args void *, size_t
$define %func lc_memeq as function with args const void *, const void *, size_t

*/

/* !SPACE!

$space %export lc_wipe, lc_memeq

*/

/*
 * Copyright (c) 2026, otsos team
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
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
 * SUBSTITUTE GOODS OR SERVICES; LOSS, USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <libcrypto.h>
#include <stddef.h>
#include <stdint.h>

void
lc_wipe(void *ptr, size_t len)
{
	volatile uint8_t	*p;

	if (!ptr) {
		return;
	}
	p = (volatile uint8_t *)ptr;
	while (len > 0) {
		*p = 0;
		p++;
		len--;
	}
}

int
lc_memeq(const void *a, const void *b, size_t len)
{
	const uint8_t	*pa;
	const uint8_t	*pb;
	uint8_t		diff;
	size_t		i;

	if (!a || !b) {
		return (0);
	}
	pa = (const uint8_t *)a;
	pb = (const uint8_t *)b;
	diff = 0;
	for (i = 0; i < len; i++) {
		diff |= pa[i] ^ pb[i];
	}
	return (diff == 0);
}
