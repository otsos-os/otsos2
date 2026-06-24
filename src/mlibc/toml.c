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

#include <mlibc/toml.h>
#include <mlibc/mlibc.h>
#include <kernel/drivers/fs/chainFS/chainfs.h>

toml_doc_t *toml_new(void) {
  toml_doc_t *doc = (toml_doc_t *)kmem_calloc(1, sizeof(toml_doc_t));
  return doc;
}

static toml_entry_t *toml_find_entry(toml_doc_t *doc, const char *section,
                                     const char *key) {
  if (!doc || !key) return NULL;
  for (toml_entry_t *e = doc->entries; e; e = e->next) {
    if (strcmp(e->key, key) != 0) continue;
    if (!section && !e->section) return e;
    if (section && e->section && strcmp(e->section, section) == 0) return e;
    if (!section && e->section) continue;
  }
  return NULL;
}

void toml_set(toml_doc_t *doc, const char *section, const char *key,
              const char *value) {
  if (!doc || !key) return;

  toml_entry_t *existing = toml_find_entry(doc, section, key);
  if (existing) {
    if (existing->value) kmem_free(existing->value);
    existing->value = NULL;
    if (value) {
      int vlen = strlen(value);
      existing->value = (char *)kmem_calloc(vlen + 1, 1);
      if (existing->value) memcpy(existing->value, value, vlen);
    }
    return;
  }

  toml_entry_t *e = (toml_entry_t *)kmem_calloc(1, sizeof(toml_entry_t));
  if (!e) return;

  int klen = strlen(key);
  e->key = (char *)kmem_calloc(klen + 1, 1);
  if (e->key) memcpy(e->key, key, klen);

  if (section && section[0]) {
    int slen = strlen(section);
    e->section = (char *)kmem_calloc(slen + 1, 1);
    if (e->section) memcpy(e->section, section, slen);
  }

  if (value) {
    int vlen = strlen(value);
    e->value = (char *)kmem_calloc(vlen + 1, 1);
    if (e->value) memcpy(e->value, value, vlen);
  }

  e->next = doc->entries;
  doc->entries = e;
  doc->count++;
}

const char *toml_get(toml_doc_t *doc, const char *section, const char *key) {
  toml_entry_t *e = toml_find_entry(doc, section, key);
  return e ? e->value : NULL;
}

static void skip_whitespace(const char **p, const char *end) {
  while (*p < end && (**p == ' ' || **p == '\t')) (*p)++;
}

static int toml_parse_line(toml_doc_t *doc, const char *line, u32 len,
                           const char **current_section) {
  const char *p = line;
  const char *end = line + len;

  skip_whitespace(&p, end);
  if (p >= end || *p == '#' || *p == '\r' || *p == '\n') return 0;

  if (*p == '[') {
    p++;
    const char *sec_start = p;
    while (p < end && *p != ']') p++;
    if (p >= end) return -1;
    int sec_len = (int)(p - sec_start);
    if (sec_len <= 0) return -1;

    if (*current_section) kmem_free((void *)*current_section);
    char *sec = (char *)kmem_calloc(sec_len + 1, 1);
    if (!sec) return -1;
    memcpy(sec, sec_start, sec_len);
    *current_section = sec;
    return 0;
  }

  const char *key_start = p;
  while (p < end && *p != ' ' && *p != '\t' && *p != '=' && *p != '\r' && *p != '\n') p++;
  int key_len = (int)(p - key_start);
  if (key_len <= 0) return -1;

  skip_whitespace(&p, end);
  if (p >= end || *p != '=') return -1;
  p++;
  skip_whitespace(&p, end);

  char *key = (char *)kmem_calloc(key_len + 1, 1);
  if (!key) return -1;
  memcpy(key, key_start, key_len);

  if (p >= end) {
    toml_set(doc, *current_section, key, "");
    kmem_free(key);
    return 0;
  }

  if (*p == '"') {
    p++;
    const char *val_start = p;
    while (p < end && *p != '"') p++;
    int val_len = (int)(p - val_start);
    char *val = (char *)kmem_calloc(val_len + 1, 1);
    if (val) {
      memcpy(val, val_start, val_len);
      toml_set(doc, *current_section, key, val);
      kmem_free(val);
    }
  } else {
    const char *val_start = p;
    while (p < end && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n' && *p != '#') p++;
    int val_len = (int)(p - val_start);
    if (val_len > 0) {
      char *val = (char *)kmem_calloc(val_len + 1, 1);
      if (val) {
        memcpy(val, val_start, val_len);
        toml_set(doc, *current_section, key, val);
        kmem_free(val);
      }
    } else {
      toml_set(doc, *current_section, key, "");
    }
  }

  kmem_free(key);
  return 0;
}

toml_doc_t *toml_parse(const char *data, u32 len) {
  if (!data || len == 0) return toml_new();

  toml_doc_t *doc = toml_new();
  if (!doc) return NULL;

  char *current_section = NULL;
  u32 line_start = 0;

  for (u32 i = 0; i <= len; i++) {
    if (i == len || data[i] == '\n') {
      toml_parse_line(doc, data + line_start, i - line_start,
                      (const char **)&current_section);
      line_start = i + 1;
    }
  }

  if (current_section) kmem_free(current_section);
  return doc;
}

toml_doc_t *toml_parse_file(const char *path) {
  chainfs_file_entry_t entry;
  u32 entry_block, entry_offset;
  if (chainfs_find_file(path, &entry, &entry_block, &entry_offset) != 0)
    return NULL;

  u32 size = entry.size;
  if (size == 0) return toml_new();

  u8 *buf = (u8 *)kmem_calloc(size + 1, 1);
  if (!buf) return NULL;

  u32 bytes_read = 0;
  if (chainfs_read_file(path, buf, size, &bytes_read) != 0 || bytes_read != size) {
    kmem_free(buf);
    return NULL;
  }
  buf[size] = '\0';

  toml_doc_t *doc = toml_parse((const char *)buf, size);
  kmem_free(buf);
  return doc;
}

static int toml_count_chars(toml_doc_t *doc) {
  int total = 0;
  for (toml_entry_t *e = doc->entries; e; e = e->next) {
    if (e->section) total += 2 + strlen(e->section) + 2; /* [X]\n */
    if (e->key) total += strlen(e->key) + 3;              /* key =  */
    if (e->value) total += strlen(e->value) + 2;           /* "X"\n or X\n */
  }
  return total + 32;
}

char *toml_serialize(toml_doc_t *doc) {
  if (!doc) return NULL;

  const char *last_section = NULL;
  int cap = toml_count_chars(doc);
  char *out = (char *)kmem_calloc(cap, 1);
  if (!out) return NULL;
  int pos = 0;

  for (toml_entry_t *e = doc->entries; e; e = e->next) {
    const char *sec = e->section;
    if ((!sec && last_section) || (sec && (!last_section || strcmp(sec, last_section) != 0))) {
      if (last_section && (!sec || strcmp(sec, last_section) != 0)) {
        out[pos++] = '\n';
      }
      if (sec) {
        out[pos++] = '[';
        memcpy(out + pos, sec, strlen(sec));
        pos += strlen(sec);
        out[pos++] = ']';
        out[pos++] = '\n';
      }
      last_section = sec;
    }

    memcpy(out + pos, e->key, strlen(e->key));
    pos += strlen(e->key);
    out[pos++] = ' ';
    out[pos++] = '=';
    out[pos++] = ' ';
    out[pos++] = '"';
    if (e->value) {
      memcpy(out + pos, e->value, strlen(e->value));
      pos += strlen(e->value);
    }
    out[pos++] = '"';
    out[pos++] = '\n';
  }

  out[pos] = '\0';
  return out;
}

int toml_save(toml_doc_t *doc, const char *path) {
  if (!doc || !path) return -1;
  char *data = toml_serialize(doc);
  if (!data) return -1;
  int len = strlen(data);
  int ret = chainfs_write_file(path, (const u8 *)data, (u32)len);
  kmem_free(data);
  return ret;
}

void toml_free(toml_doc_t *doc) {
  if (!doc) return;
  toml_entry_t *e = doc->entries;
  while (e) {
    toml_entry_t *next = e->next;
    if (e->section) kmem_free(e->section);
    if (e->key) kmem_free(e->key);
    if (e->value) kmem_free(e->value);
    kmem_free(e);
    e = next;
  }
  kmem_free(doc);
}
