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

#include <kernel/console.h>
#include <kernel/other/kusr.h>
#include <kernel/drivers/fs/chainFS/chainfs.h>
#include <kernel/drivers/keyboard/keyboard.h>
#include <kernel/drivers/tty.h>
#include <lib/com1.h>
#include <mlibc/memory.h>
#include <mlibc/mlibc.h>
#include <mlibc/toml.h>

#define KUSR_CONFIG_PATH "/conf/kernel.toml"
#define KUSR_PASS_MIN_LEN 4

static int g_kusr_authenticated = 0;

int kusr_is_authenticated(void) { return g_kusr_authenticated; }
void kusr_set_authenticated(int auth) { g_kusr_authenticated = auth ? 1 : 0; }

static void kusr_flush(void) {
  tty_flush_kernel();
}

static u64 fnv1a_64(const char *data, int len) {
  u64 hash = 0xcbf29ce484222325ULL;
  for (int i = 0; i < len; i++) {
    hash ^= (u8)data[i];
    hash *= 0x100000001b3ULL;
  }
  return hash;
}

static void hash_to_hex(u64 hash, char *out) {
  const char *hex = "0123456789abcdef";
  for (int i = 0; i < 16; i++)
    out[i] = hex[(hash >> (60 - i * 4)) & 0xF];
  out[16] = '\0';
}

static void kusr_hash_password(const char *pass, char *hash_out) {
  u64 h = fnv1a_64(pass, strlen(pass));
  hash_to_hex(h, hash_out);
}

static void kusr_wipe(char *buf, int len) {
  for (int i = 0; i < len; i++) buf[i] = 0;
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
  kusr_flush();

  for (;;) {
    char pass1[128], pass2[128];

    kusr_read_password(pass1, sizeof(pass1), "New kusr password: ");
    if (strlen(pass1) < KUSR_PASS_MIN_LEN) {
      printf("\033[31mPassword must be at least %d chars. Try again.\033[0m\n\n",
             KUSR_PASS_MIN_LEN);
      kusr_flush();
      kusr_wipe(pass1, sizeof(pass1));
      continue;
    }

    kusr_read_password(pass2, sizeof(pass2), "Confirm kusr password: ");
    if (strcmp(pass1, pass2) != 0) {
      printf("\033[31mPasswords do not match. Try again.\033[0m\n\n");
      kusr_flush();
      kusr_wipe(pass1, sizeof(pass1));
      kusr_wipe(pass2, sizeof(pass2));
      continue;
    }

    char hash[17];
    kusr_hash_password(pass1, hash);
    kusr_wipe(pass1, sizeof(pass1));
    kusr_wipe(pass2, sizeof(pass2));

    chainfs_mkdir("/conf");

    toml_doc_t *doc = toml_new();
    if (!doc) {
      printf("\033[31mFailed to create config. Retrying...\033[0m\n\n");
      kusr_flush();
      continue;
    }

    toml_set(doc, "kusr", "password_hash", hash);
    toml_set(doc, "kusr", "created", "1");

    if (toml_save(doc, KUSR_CONFIG_PATH) != 0) {
      printf("\033[31mFailed to save config to disk. Retrying...\033[0m\n\n");
      kusr_flush();
      toml_free(doc);
      continue;
    }

    toml_free(doc);
    g_kusr_authenticated = 1;
    printf("\n\033[32mSetup complete. kusr configured.\033[0m\n");
    kusr_flush();
    return 1;
  }
}

void kusr_init(void) {
  com1_printf("[KUSR] Initializing...\n");

  chainfs_file_entry_t entry;
  u32 entry_block, entry_offset;
  if (chainfs_find_file(KUSR_CONFIG_PATH, &entry, &entry_block, &entry_offset) == 0) {
    com1_printf("[KUSR] Config exists, skipping first-boot setup\n");
    g_kusr_authenticated = 0;
    return;
  }

  kusr_first_boot_setup();
  if (g_kusr_authenticated)
    com1_printf("[KUSR] First-boot setup complete\n");
  else
    com1_printf("[KUSR] First-boot setup was cancelled\n");
}
