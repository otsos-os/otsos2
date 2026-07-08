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

#include <kernel/console/console.h>
#include <kernel/other/kusr.h>
#include <kernel/crypto/crypto.h>
#include <kernel/drivers/fs/vfs/vfs.h>
#include <kernel/drivers/keyboard/keyboard.h>
#include <kernel/console/terminal.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>
#include <mlibc/toml.h>

#define KUSR_CONFIG_PATH "/conf/kernel.toml"
#define KUSR_PASS_MIN_LEN 4
#define KUSR_SALT_LEN     PBKDF2_SALT_DEFAULT
#define KUSR_ITERATIONS   PBKDF2_DEFAULT_ITERS
#define KUSR_HASH_LEN     PBKDF2_DIGEST_SIZE
#define KUSR_HASH_HEX_LEN (KUSR_HASH_LEN * 2 + 1)
#define KUSR_SALT_HEX_LEN (KUSR_SALT_LEN * 2 + 1)

static int g_kusr_authenticated = 0;

int kusr_is_authenticated(void) { return g_kusr_authenticated; }
void kusr_set_authenticated(int auth) { g_kusr_authenticated = auth ? 1 : 0; }

static void kusr_flush(void) {
  terminal_flush_kernel();
}

static void kusr_hash_password(const char *pass, u32 pass_len,
                               const u8 *salt, u32 salt_len,
                               u32 iterations,
                               u8 *hash_out) {
  pbkdf2_hmac_sha256((const u8 *)pass, pass_len,
                     salt, salt_len,
                     iterations,
                     hash_out, KUSR_HASH_LEN);
}

static void kusr_generate_salt(u8 *salt, u32 len) {
  crypto_rng_bytes(salt, len);
}

static int kusr_read_password(char *buf, int max, const char *prompt) {
  int pos = 0;
  printf("%s", prompt);
  kusr_flush();
  while (1) {
    char c = keyboard_getchar();
    if (c == 0) {
      __asm__ volatile("hlt");
      continue;
    }
    if (c == '\r' || c == '\n') {
      printf("\n");
      kusr_flush();
      buf[pos] = '\0';
      break;
    }
    if (c == '\b' || c == 0x7F) {
      if (pos > 0) pos--;
      continue;
    }
    if (c >= 32 && c < 127 && pos < max - 1)
      buf[pos++] = c;
  }
  return pos;
}

static int kusr_first_boot_setup(void) {
  printf("\n\033[36m=== OTSOS First Boot Setup ===\033[0m\n");
  printf("Create kernel user (kusr) password.\n");
  printf("This protects dangerous syscalls (drm, raw disk, etc).\n");
  printf("Password is hashed with PBKDF2-HMAC-SHA256 (%d iterations).\n",
         KUSR_ITERATIONS);
  kusr_flush();

  for (;;) {
    char pass1[128], pass2[128];

    kusr_read_password(pass1, sizeof(pass1), "New kusr password: ");
    if (strlen(pass1) < KUSR_PASS_MIN_LEN) {
      printf("\033[31mPassword must be at least %d chars. Try again.\033[0m\n\n",
             KUSR_PASS_MIN_LEN);
      kusr_flush();
      crypto_secure_wipe(pass1, sizeof(pass1));
      continue;
    }

    kusr_read_password(pass2, sizeof(pass2), "Confirm kusr password: ");
    if (strcmp(pass1, pass2) != 0) {
      printf("\033[31mPasswords do not match. Try again.\033[0m\n\n");
      kusr_flush();
      crypto_secure_wipe(pass1, sizeof(pass1));
      crypto_secure_wipe(pass2, sizeof(pass2));
      continue;
    }

    u8 salt[KUSR_SALT_LEN];
    u8 hash[KUSR_HASH_LEN];
    char salt_hex[KUSR_SALT_HEX_LEN];
    char hash_hex[KUSR_HASH_HEX_LEN];

    kusr_generate_salt(salt, KUSR_SALT_LEN);
    kusr_hash_password(pass1, strlen(pass1),
                       salt, KUSR_SALT_LEN,
                       KUSR_ITERATIONS,
                       hash);
    crypto_secure_wipe(pass1, sizeof(pass1));
    crypto_secure_wipe(pass2, sizeof(pass2));

    crypto_hex_encode(salt, KUSR_SALT_LEN, salt_hex);
    crypto_hex_encode(hash, KUSR_HASH_LEN, hash_hex);

    crypto_secure_wipe(salt, sizeof(salt));
    crypto_secure_wipe(hash, sizeof(hash));

    vfs_mkdir("/conf");

    toml_doc_t *doc = toml_new();
    if (!doc) {
      printf("\033[31mFailed to create config. Retrying...\033[0m\n\n");
      kusr_flush();
      crypto_secure_wipe(salt_hex, sizeof(salt_hex));
      crypto_secure_wipe(hash_hex, sizeof(hash_hex));
      continue;
    }

    toml_set(doc, "kusr", "password_hash", hash_hex);
    toml_set(doc, "kusr", "password_salt", salt_hex);

    char iter_str[16];
    itoa((int)KUSR_ITERATIONS, iter_str, 10);
    toml_set(doc, "kusr", "password_iterations", iter_str);
    toml_set(doc, "kusr", "created", "1");
    toml_set(doc, "kusr", "kdf", "pbkdf2-hmac-sha256");

    if (toml_save(doc, KUSR_CONFIG_PATH) != 0) {
      printf("\033[31mFailed to save config to disk. Retrying...\033[0m\n\n");
      kusr_flush();
      toml_free(doc);
      crypto_secure_wipe(salt_hex, sizeof(salt_hex));
      crypto_secure_wipe(hash_hex, sizeof(hash_hex));
      continue;
    }

    toml_free(doc);
    crypto_secure_wipe(salt_hex, sizeof(salt_hex));
    crypto_secure_wipe(hash_hex, sizeof(hash_hex));

    g_kusr_authenticated = 1;
    printf("\n\033[32mSetup complete. kusr configured (PBKDF2).\033[0m\n");
    kusr_flush();
    return 1;
  }
}

void kusr_init(void) {
  printk("[KUSR] Initializing...\n");
  crypto_rng_init();

	vnode_t *vn;
	if (vfs_resolve(KUSR_CONFIG_PATH, &vn) == 0 && vn != NULL) {
		vnode_release(vn);
		printk("[KUSR] Config exists, skipping first-boot setup\n");
		g_kusr_authenticated = 0;
		return;
	}

	kusr_first_boot_setup();
}
