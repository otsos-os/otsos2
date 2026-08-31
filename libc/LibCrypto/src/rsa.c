/* !DEFINES!

$define %type lc_bn as fixed capacity big unsigned integer
$define %type size_t as object size
$define %func lc_rsa_public as function with args modulus, exponent, in, out, len

*/

/* !SPACE!

$space %export lc_rsa_public

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
#include <string.h>

int
lc_rsa_public(const void *modulus, size_t modulus_len, const void *exponent,
    size_t exponent_len, const void *in, void *out, size_t len)
{
	lc_bn	n, e, m, c;
	int	ret;

	if (modulus == NULL || exponent == NULL || in == NULL || out == NULL) {
		return (-1);
	}
	if (len == 0 || len != modulus_len || len > LC_BN_MAX_BYTES) {
		return (-1);
	}

	lc_bn_zero(&n);
	lc_bn_zero(&e);
	lc_bn_zero(&m);
	lc_bn_zero(&c);

	ret = -1;
	if (lc_bn_from_bytes(&n, modulus, modulus_len) != 0 ||
	    lc_bn_from_bytes(&e, exponent, exponent_len) != 0 ||
	    lc_bn_from_bytes(&m, in, len) != 0) {
		goto done;
	}
	if (lc_bn_cmp(&m, &n) >= 0) {
		goto done;
	}
	if (lc_bn_mod_exp(&c, &m, &e, &n) != 0) {
		goto done;
	}
	if (lc_bn_to_bytes(&c, out, len) != 0) {
		goto done;
	}
	ret = 0;

done:
	lc_bn_wipe(&n);
	lc_bn_wipe(&e);
	lc_bn_wipe(&m);
	lc_bn_wipe(&c);
	return (ret);
}
