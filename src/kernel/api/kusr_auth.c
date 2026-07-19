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

$define %type u8 as 8 bit unsigned
$define %type u32 as 32 bit unsigned
$define %type int as 32 bit signed
$define %type char as 8 bit signed
$define %type process_t as process control block

$define %func api_kusr_auth as function with args const char *

*/

/* !SPACE!

$space %export api_kusr_auth

*/

#include <kernel/api/api.h>
#include <kernel/cm/cm.h>
#include <kernel/crypto/crypto.h>
#include <kernel/process.h>
#include <kernel/useraddr.h>
#include <mlibc/mlibc.h>

#define	KUSR_AUTH_REG_HIVE		"SECURITY"
#define	KUSR_AUTH_REG_KEY		"Kusr"
#define	KUSR_AUTH_HASH_LEN		PBKDF2_DIGEST_SIZE
#define	KUSR_AUTH_SALT_LEN		PBKDF2_SALT_DEFAULT
#define	KUSR_AUTH_HASH_HEX_LEN		(KUSR_AUTH_HASH_LEN * 2 + 1)
#define	KUSR_AUTH_SALT_HEX_LEN		(KUSR_AUTH_SALT_LEN * 2 + 1)
#define	KUSR_AUTH_DEFAULT_ITERS		PBKDF2_DEFAULT_ITERS

int
api_kusr_auth(const char *password)
{
	process_t	*proc;
	char		kpass[128];
	char		stored_hash[KUSR_AUTH_HASH_HEX_LEN];
	char		stored_salt[KUSR_AUTH_SALT_HEX_LEN];
	u8		salt[KUSR_AUTH_SALT_LEN];
	u8		expected_hash[KUSR_AUTH_HASH_LEN];
	u32		iterations, salt_decoded, hash_decoded;
	int		configured, pass_len, max_pass;
	int		ret, match;

	proc = process_current();
	if (!proc) {
		return (-API_ERR_BAD_VALUE);
	}
	if (!is_user_address(password, 1)) {
		return (-API_ERR_BAD_ADDR);
	}

	memset(kpass, 0, sizeof(kpass));
	max_pass = (int)sizeof(kpass) - 1;
	pass_len = 0;
	while (pass_len < max_pass) {
		if (!is_user_address(password + pass_len, 1)) {
			return (-API_ERR_BAD_ADDR);
		}
		if (password[pass_len] == '\0') {
			break;
		}
		kpass[pass_len] = password[pass_len];
		pass_len++;
	}
	if (pass_len == max_pass) {
		if (!is_user_address(password + pass_len, 1)) {
			crypto_secure_wipe(kpass, sizeof(kpass));
			return (-API_ERR_BAD_ADDR);
		}
		if (password[pass_len] != '\0') {
			crypto_secure_wipe(kpass, sizeof(kpass));
			return (-API_ERR_TOO_BIG);
		}
	}

	configured = 0;
	ret = cm_get_bool(KUSR_AUTH_REG_HIVE, KUSR_AUTH_REG_KEY,
	    "Configured", &configured);
	if (ret != 0 || !configured) {
		crypto_secure_wipe(kpass, sizeof(kpass));
		return (-API_ERR_NOT_FOUND);
	}

	ret = cm_get_string(KUSR_AUTH_REG_HIVE, KUSR_AUTH_REG_KEY,
	    "PasswordHash", stored_hash, sizeof(stored_hash));
	if (ret != 0) {
		crypto_secure_wipe(kpass, sizeof(kpass));
		return (ret);
	}
	ret = cm_get_string(KUSR_AUTH_REG_HIVE, KUSR_AUTH_REG_KEY,
	    "PasswordSalt", stored_salt, sizeof(stored_salt));
	if (ret != 0) {
		crypto_secure_wipe(kpass, sizeof(kpass));
		crypto_secure_wipe(stored_hash, sizeof(stored_hash));
		return (ret);
	}

	iterations = cm_get_u32_default(KUSR_AUTH_REG_HIVE,
	    KUSR_AUTH_REG_KEY, "Iterations", KUSR_AUTH_DEFAULT_ITERS);
	memset(salt, 0, sizeof(salt));
	memset(expected_hash, 0, sizeof(expected_hash));
	salt_decoded = 0;
	hash_decoded = 0;

	ret = crypto_hex_decode(stored_salt, salt, KUSR_AUTH_SALT_LEN,
	    &salt_decoded);
	if (ret != 0 || salt_decoded != KUSR_AUTH_SALT_LEN) {
		crypto_secure_wipe(kpass, sizeof(kpass));
		crypto_secure_wipe(stored_hash, sizeof(stored_hash));
		crypto_secure_wipe(stored_salt, sizeof(stored_salt));
		crypto_secure_wipe(salt, sizeof(salt));
		return (-API_ERR_BAD_VALUE);
	}
	ret = crypto_hex_decode(stored_hash, expected_hash,
	    KUSR_AUTH_HASH_LEN, &hash_decoded);
	if (ret != 0 || hash_decoded != KUSR_AUTH_HASH_LEN) {
		crypto_secure_wipe(kpass, sizeof(kpass));
		crypto_secure_wipe(stored_hash, sizeof(stored_hash));
		crypto_secure_wipe(stored_salt, sizeof(stored_salt));
		crypto_secure_wipe(salt, sizeof(salt));
		crypto_secure_wipe(expected_hash, sizeof(expected_hash));
		return (-API_ERR_BAD_VALUE);
	}

	match = pbkdf2_verify((const u8 *)kpass, (u32)pass_len,
	    salt, KUSR_AUTH_SALT_LEN, iterations, expected_hash,
	    KUSR_AUTH_HASH_LEN);
	crypto_secure_wipe(kpass, sizeof(kpass));
	crypto_secure_wipe(stored_hash, sizeof(stored_hash));
	crypto_secure_wipe(stored_salt, sizeof(stored_salt));
	crypto_secure_wipe(salt, sizeof(salt));
	crypto_secure_wipe(expected_hash, sizeof(expected_hash));

	if (match != 0) {
		return (-API_ERR_PERM);
	}
	proc->kusr_auth = 1;
	return (0);
}
