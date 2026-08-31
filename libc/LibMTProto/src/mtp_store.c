/* !DEFINES!

$define %type mtp_store as persisted authorization key and session record
$define %func mtp_store_load as function with args client
$define %func mtp_store_save as function with args client
$define %func mtp_store_forget as procedure with args client

*/

/* !SPACE!

$space %internal store_put_i64, store_get_i64
$space %export mtp_store_load, mtp_store_save, mtp_store_forget

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
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */



#include <errno.h>
#include <native.h>
#include <string.h>

#include "mtp_internal.h"

#define STORE_MAGIC	0x4F544D50u
#define STORE_VERSION	2
#define STORE_OFF_MAGIC	0
#define STORE_OFF_VERSION	4
#define STORE_OFF_DC	8
#define STORE_OFF_FLAGS	12
#define STORE_OFF_KEY	16
#define STORE_OFF_KEY_ID	(STORE_OFF_KEY + MTP_AUTH_KEY_SIZE)
#define STORE_OFF_SESSION	(STORE_OFF_KEY_ID + 8)
#define STORE_OFF_SALT	(STORE_OFF_SESSION + 8)
#define STORE_OFF_SELF	(STORE_OFF_SALT + 8)
#define STORE_OFF_TIME	(STORE_OFF_SELF + 8)
#define STORE_OFF_HASH	(STORE_OFF_TIME + 8)
#define STORE_LEN	(STORE_OFF_HASH + LC_SHA256_DIGEST_SIZE)

static void
store_put_i64(uint8_t *p, int64_t v)
{
	uint64_t	u;
	int		i;

	u = (uint64_t)v;
	for (i = 0; i < 8; i++) {
		p[i] = (uint8_t)((u >> (i * 8)) & 0xFFu);
	}
}

static int64_t
store_get_i64(const uint8_t *p)
{
	uint64_t	u;
	int		i;

	u = 0;
	for (i = 0; i < 8; i++) {
		u |= (uint64_t)p[i] << (i * 8);
	}
	return ((int64_t)u);
}

int
mtp_store_load(mtp_client_t *c)
{
	uint8_t		rec[STORE_LEN];
	uint8_t		digest[LC_SHA256_DIGEST_SIZE];
	uint32_t	magic, version;
	int32_t		dc;
	int		dc_slot;
	int		fd;

	if (c->auth_path[0] == '\0') {
		mtp_logf(MTP_LOG_ERROR, "store: no auth_path configured");
		return (MTP_ERR_STORE);
	}
	fd = dataOpen(c->auth_path, API_OPEN_READ);
	if (fd < 0) {
		if (errno == ENOENT) {
			mtp_logf(MTP_LOG_INFO, "store: %s absent, a fresh "
			    "handshake is required", c->auth_path);
			return (MTP_OK);
		}
		mtp_logf(MTP_LOG_ERROR, "store: cannot open %s: errno %d",
		    c->auth_path, errno);
		return (MTP_ERR_STORE);
	}
	if (dataReadFull(fd, rec, sizeof(rec)) != 0) {
		dataClose(fd);
		lc_wipe(rec, sizeof(rec));
		mtp_logf(MTP_LOG_ERROR, "store: %s is shorter than the %u-byte "
		    "record (truncated write)", c->auth_path,
		    (unsigned int)sizeof(rec));
		return (MTP_ERR_STORE);
	}
	dataClose(fd);

	magic = (uint32_t)rec[STORE_OFF_MAGIC] |
	    ((uint32_t)rec[STORE_OFF_MAGIC + 1] << 8) |
	    ((uint32_t)rec[STORE_OFF_MAGIC + 2] << 16) |
	    ((uint32_t)rec[STORE_OFF_MAGIC + 3] << 24);
	version = (uint32_t)rec[STORE_OFF_VERSION] |
	    ((uint32_t)rec[STORE_OFF_VERSION + 1] << 8) |
	    ((uint32_t)rec[STORE_OFF_VERSION + 2] << 16) |
	    ((uint32_t)rec[STORE_OFF_VERSION + 3] << 24);
	dc = (int32_t)((uint32_t)rec[STORE_OFF_DC] |
	    ((uint32_t)rec[STORE_OFF_DC + 1] << 8) |
	    ((uint32_t)rec[STORE_OFF_DC + 2] << 16) |
	    ((uint32_t)rec[STORE_OFF_DC + 3] << 24));
	lc_sha256(rec, STORE_OFF_HASH, digest);
	if (magic != STORE_MAGIC) {
		mtp_logf(MTP_LOG_ERROR, "store: %s has magic %08x, expected "
		    "%08x (not an auth record)", c->auth_path,
		    (unsigned int)magic, (unsigned int)STORE_MAGIC);
		goto reject;
	}
	if (version != STORE_VERSION) {
		mtp_logf(MTP_LOG_ERROR, "store: %s is version %u, this build "
		    "reads %u; delete it to re-key", c->auth_path,
		    (unsigned int)version, (unsigned int)STORE_VERSION);
		goto reject;
	}
	dc_slot = mtp_dc_index_of((int)dc);
	if (dc_slot < 0) {
		mtp_logf(MTP_LOG_ERROR, "store: %s names DC%d, which this build "
		    "has no address for", c->auth_path, (int)dc);
		goto reject;
	}
	if (!lc_memeq(digest, rec + STORE_OFF_HASH, LC_SHA256_DIGEST_SIZE)) {
		mtp_logf(MTP_LOG_ERROR, "store: %s failed its checksum "
		    "(interrupted write); delete it to re-key", c->auth_path);
		goto reject;
	}

	memcpy(c->auth_key, rec + STORE_OFF_KEY, MTP_AUTH_KEY_SIZE);
	c->auth_key_id = store_get_i64(rec + STORE_OFF_KEY_ID);
	c->session_id = store_get_i64(rec + STORE_OFF_SESSION);
	c->server_salt = store_get_i64(rec + STORE_OFF_SALT);
	c->self_id = store_get_i64(rec + STORE_OFF_SELF);
	c->time_offset = (int32_t)store_get_i64(rec + STORE_OFF_TIME);
	c->dc_index = dc_slot;
	c->auth_key_valid = 1;
	c->authorized = (rec[STORE_OFF_FLAGS] & 1u) != 0;

	mtp_logf(MTP_LOG_INFO, "store: loaded %s -- DC%d, auth_key_id=%016llx, "
	    "authorized=%s, self_id=%lld, clock offset %+d s", c->auth_path,
	    (int)dc, (unsigned long long)c->auth_key_id,
	    c->authorized ? "yes" : "no", (long long)c->self_id,
	    (int)c->time_offset);
	lc_wipe(rec, sizeof(rec));
	lc_wipe(digest, sizeof(digest));
	return (MTP_OK);
reject:
	lc_wipe(rec, sizeof(rec));
	lc_wipe(digest, sizeof(digest));
	return (MTP_ERR_STORE);
}

int
mtp_store_save(const mtp_client_t *c)
{
	uint8_t	rec[STORE_LEN];
	int	fd, ret;

	if (c->auth_path[0] == '\0') {
		mtp_logf(MTP_LOG_ERROR, "store: cannot save, no auth_path");
		return (MTP_ERR_STORE);
	}
	if (!c->auth_key_valid) {
		mtp_logf(MTP_LOG_ERROR, "store: refusing to save without a valid "
		    "auth_key");
		return (MTP_ERR_STORE);
	}
	memset(rec, 0, sizeof(rec));
	rec[STORE_OFF_MAGIC] = (uint8_t)(STORE_MAGIC & 0xFFu);
	rec[STORE_OFF_MAGIC + 1] = (uint8_t)((STORE_MAGIC >> 8) & 0xFFu);
	rec[STORE_OFF_MAGIC + 2] = (uint8_t)((STORE_MAGIC >> 16) & 0xFFu);
	rec[STORE_OFF_MAGIC + 3] = (uint8_t)((STORE_MAGIC >> 24) & 0xFFu);
	rec[STORE_OFF_VERSION] = STORE_VERSION;
	rec[STORE_OFF_DC] = (uint8_t)(mtp_dc_id(c->dc_index) & 0xFFu);
	rec[STORE_OFF_FLAGS] = (uint8_t)(c->authorized ? 1 : 0);
	memcpy(rec + STORE_OFF_KEY, c->auth_key, MTP_AUTH_KEY_SIZE);
	store_put_i64(rec + STORE_OFF_KEY_ID, c->auth_key_id);
	store_put_i64(rec + STORE_OFF_SESSION, c->session_id);
	store_put_i64(rec + STORE_OFF_SALT, c->server_salt);
	store_put_i64(rec + STORE_OFF_SELF, c->self_id);
	store_put_i64(rec + STORE_OFF_TIME, c->time_offset);
	lc_sha256(rec, STORE_OFF_HASH, rec + STORE_OFF_HASH);

	fd = dataOpen(c->auth_path, API_OPEN_WRITE | API_OPEN_CREATE |
	    API_OPEN_TRUNC);
	if (fd < 0) {
		lc_wipe(rec, sizeof(rec));
		mtp_logf(MTP_LOG_ERROR, "store: cannot create %s: errno %d",
		    c->auth_path, errno);
		return (MTP_ERR_STORE);
	}
	ret = dataWriteFull(fd, rec, sizeof(rec)) == 0 ? MTP_OK : MTP_ERR_STORE;
	dataClose(fd);
	lc_wipe(rec, sizeof(rec));
	if (ret != MTP_OK) {
		mtp_logf(MTP_LOG_ERROR, "store: short write to %s (errno %d); "
		    "the record is now incomplete and will be rejected on load",
		    c->auth_path, errno);
		return (ret);
	}
	mtp_logf(MTP_LOG_INFO, "store: saved %s -- DC%d, auth_key_id=%016llx, "
	    "authorized=%s", c->auth_path, mtp_dc_id(c->dc_index),
	    (unsigned long long)c->auth_key_id, c->authorized ? "yes" : "no");
	return (MTP_OK);
}


void
mtp_store_forget(mtp_client_t *c)
{
	uint8_t	zero[STORE_LEN];
	int	fd;

	lc_wipe(c->auth_key, sizeof(c->auth_key));
	c->auth_key_valid = 0;
	c->auth_key_id = 0;
	c->authorized = 0;
	c->self_id = 0;
	c->code_hash[0] = '\0';
	mtp_srp_reset(c);
	c->password_needed = 0;
	if (c->auth_path[0] == '\0') {
		return;
	}
	memset(zero, 0, sizeof(zero));
	fd = dataOpen(c->auth_path, API_OPEN_WRITE | API_OPEN_TRUNC);
	if (fd < 0) {

		mtp_logf(MTP_LOG_ERROR, "store: cannot overwrite %s on logout "
		    "(errno %d); the old key is still on disk", c->auth_path,
		    errno);
		return;
	}
	if (dataWriteFull(fd, zero, sizeof(zero)) != 0) {
		mtp_logf(MTP_LOG_ERROR, "store: short zero-fill of %s on logout "
		    "(errno %d)", c->auth_path, errno);
	} else {
		mtp_logf(MTP_LOG_INFO, "store: %s zeroed on logout",
		    c->auth_path);
	}
	dataClose(fd);
}
