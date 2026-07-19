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

$define %func kusr_flush as procedure with args void
$define %func kusr_hash_password as procedure with args pass, len, salt, out
$define %func kusr_generate_salt as procedure with args salt, len
$define %func kusr_save_credentials as function with args hash, salt
$define %func kusr_read_password as function with args buf, max, prompt
$define %func kusr_first_boot_setup as function with args void
$define %func kusr_init as procedure with args void
$define %func kusr_is_authenticated as function with args void
$define %func kusr_set_authenticated as procedure with args int

*/

/* !SPACE!

$space %internal kusr_flush, kusr_hash_password, kusr_generate_salt
$space %internal kusr_save_credentials, kusr_read_password
$space %internal kusr_first_boot_setup
$space %export kusr_init, kusr_is_authenticated, kusr_set_authenticated

*/

#include <kernel/api/errno.h>
#include <kernel/cm/cm.h>
#include <kernel/console/console.h>
#include <kernel/console/terminal.h>
#include <kernel/crypto/crypto.h>
#include <kernel/drivers/keyboard/keyboard.h>
#include <kernel/other/kusr.h>
#include <mlibc/mlibc.h>
#include <mlibc/stdio.h>

#define	KUSR_REG_HIVE		"SECURITY"
#define	KUSR_REG_KEY		"Kusr"
#define	KUSR_KDF_NAME		"pbkdf2-hmac-sha256"
#define	KUSR_PASS_MIN_LEN	4
#define	KUSR_SALT_LEN		PBKDF2_SALT_DEFAULT
#define	KUSR_ITERATIONS		PBKDF2_DEFAULT_ITERS
#define	KUSR_HASH_LEN		PBKDF2_DIGEST_SIZE
#define	KUSR_HASH_HEX_LEN	(KUSR_HASH_LEN * 2 + 1)
#define	KUSR_SALT_HEX_LEN	(KUSR_SALT_LEN * 2 + 1)

static int	g_kusr_authenticated;

int
kusr_is_authenticated(void)
{
	return (g_kusr_authenticated);
}

void
kusr_set_authenticated(int auth)
{
	g_kusr_authenticated = auth ? 1 : 0;
}

static void
kusr_flush(void)
{
	terminal_flush_kernel();
}

static void
kusr_hash_password(const char *pass, u32 pass_len, const u8 *salt,
    u32 salt_len, u32 iterations, u8 *hash_out)
{
	pbkdf2_hmac_sha256((const u8 *)pass, pass_len, salt, salt_len,
	    iterations, hash_out, KUSR_HASH_LEN);
}

static void
kusr_generate_salt(u8 *salt, u32 len)
{
	crypto_rng_bytes(salt, len);
}

static int
kusr_save_credentials(const char *hash_hex, const char *salt_hex)
{
	int	ret;

	ret = cm_create_key(KUSR_REG_HIVE, KUSR_REG_KEY);
	if (ret != 0 && ret != -API_ERR_EXISTS) {
		return (ret);
	}
	ret = cm_set_string(KUSR_REG_HIVE, KUSR_REG_KEY,
	    "PasswordHash", hash_hex);
	if (ret != 0) {
		return (ret);
	}
	ret = cm_set_string(KUSR_REG_HIVE, KUSR_REG_KEY,
	    "PasswordSalt", salt_hex);
	if (ret != 0) {
		return (ret);
	}
	ret = cm_set_u32(KUSR_REG_HIVE, KUSR_REG_KEY,
	    "Iterations", KUSR_ITERATIONS);
	if (ret != 0) {
		return (ret);
	}
	ret = cm_set_string(KUSR_REG_HIVE, KUSR_REG_KEY,
	    "Kdf", KUSR_KDF_NAME);
	if (ret != 0) {
		return (ret);
	}
	return (cm_set_bool(KUSR_REG_HIVE, KUSR_REG_KEY,
	    "Configured", 1));
}

static int
kusr_read_password(char *buf, int max, const char *prompt)
{
	char	c;
	int	pos;

	pos = 0;
	keyboard_start_direct_input();
	printf("%s", prompt);
	kusr_flush();
	while (1) {
		c = keyboard_getchar();
		if (c == 0) {
			__asm__ volatile("hlt");
			continue;
		}
		if (c == '\r' || c == '\n') {
			printf("\n");
			kusr_flush();
			buf[pos] = '\0';
			keyboard_stop_direct_input();
			break;
		}
		if (c == '\b' || c == 0x7F) {
			if (pos > 0) {
				pos--;
			}
			continue;
		}
		if (c >= 32 && c < 127 && pos < max - 1) {
			buf[pos++] = c;
		}
	}
	return (pos);
}

static int
kusr_first_boot_setup(void)
{
	char	pass1[128], pass2[128];
	char	salt_hex[KUSR_SALT_HEX_LEN];
	char	hash_hex[KUSR_HASH_HEX_LEN];
	u8	salt[KUSR_SALT_LEN];
	u8	hash[KUSR_HASH_LEN];
	int	ret;

	printf("\n\033[36m=== OTSOS First Boot Setup ===\033[0m\n");
	printf("Create kernel user (kusr) password.\n");
	printf("This protects dangerous syscalls (drm, raw disk, etc).\n");
	printf("Password is hashed with PBKDF2-HMAC-SHA256 (%d iterations).\n",
	    KUSR_ITERATIONS);
	kusr_flush();

	for (;;) {
		memset(pass1, 0, sizeof(pass1));
		memset(pass2, 0, sizeof(pass2));
		memset(salt_hex, 0, sizeof(salt_hex));
		memset(hash_hex, 0, sizeof(hash_hex));
		memset(salt, 0, sizeof(salt));
		memset(hash, 0, sizeof(hash));

		kusr_read_password(pass1, sizeof(pass1),
		    "New kusr password: ");
		if (strlen(pass1) < KUSR_PASS_MIN_LEN) {
			printf("\033[31mPassword must be at least %d "
			    "chars. Try again.\033[0m\n\n",
			    KUSR_PASS_MIN_LEN);
			kusr_flush();
			crypto_secure_wipe(pass1, sizeof(pass1));
			continue;
		}

		kusr_read_password(pass2, sizeof(pass2),
		    "Confirm kusr password: ");
		if (strcmp(pass1, pass2) != 0) {
			printf("\033[31mPasswords do not match. "
			    "Try again.\033[0m\n\n");
			kusr_flush();
			crypto_secure_wipe(pass1, sizeof(pass1));
			crypto_secure_wipe(pass2, sizeof(pass2));
			continue;
		}

		kusr_generate_salt(salt, KUSR_SALT_LEN);
		kusr_hash_password(pass1, (u32)strlen(pass1), salt,
		    KUSR_SALT_LEN, KUSR_ITERATIONS, hash);
		crypto_secure_wipe(pass1, sizeof(pass1));
		crypto_secure_wipe(pass2, sizeof(pass2));

		crypto_hex_encode(salt, KUSR_SALT_LEN, salt_hex);
		crypto_hex_encode(hash, KUSR_HASH_LEN, hash_hex);
		crypto_secure_wipe(salt, sizeof(salt));
		crypto_secure_wipe(hash, sizeof(hash));

		ret = kusr_save_credentials(hash_hex, salt_hex);
		crypto_secure_wipe(salt_hex, sizeof(salt_hex));
		crypto_secure_wipe(hash_hex, sizeof(hash_hex));
		if (ret != 0) {
			printf("\033[31mFailed to save registry "
			    "(err %d). Retrying...\033[0m\n\n", ret);
			kusr_flush();
			continue;
		}

		g_kusr_authenticated = 1;
		printf("\n\033[32mSetup complete. kusr configured "
		    "(PBKDF2).\033[0m\n");
		kusr_flush();
		return (1);
	}
}

void
kusr_init(void)
{
	int	configured, ret;

	printk("[KUSR] Initializing...\n");
	crypto_rng_init();
	g_kusr_authenticated = 0;

	if (!cm_is_initialized()) {
		printk("[KUSR] registry unavailable\n");
		return;
	}

	configured = 0;
	ret = cm_get_bool(KUSR_REG_HIVE, KUSR_REG_KEY,
	    "Configured", &configured);
	if (ret == 0 && configured) {
		printk("[KUSR] Registry credentials exist, "
		    "skipping first-boot setup\n");
		return;
	}

	kusr_first_boot_setup();
}
