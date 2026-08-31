/* !DEFINES!

$define %type mtp_reader as bounded TL deserialisation cursor
$define %type mtp_object as one parsed TL constructor with field offsets
$define %func mtp_reader_init as procedure with args reader, buffer, length
$define %func mtp_read_u32 as function with args reader
$define %func mtp_reader_peek_u32 as function with args reader
$define %func mtp_read_i32 as function with args reader
$define %func mtp_read_i64 as function with args reader
$define %func mtp_read_double as function with args reader
$define %func mtp_read_raw as function with args reader, length
$define %func mtp_read_bytes as function with args reader, out length
$define %func mtp_read_string as function with args reader, out, capacity
$define %func mtp_skip_object as function with args reader, depth
$define %func mtp_object_parse as function with args reader, out object
$define %func mtp_object_at as function with args object, field, out reader
$define %func mtp_object_has as function with args object, field
$define %func mtp_object_i32 as function with args object, field, default
$define %func mtp_object_i64 as function with args object, field, default
$define %func mtp_object_str as function with args object, field, out, capacity
$define %func mtp_object_vector as function with args object, field, out reader, out count
$define %func mtp_reader_reason as function with args reader
$define %func mtp_reader_explain as function with args reader, out, capacity

*/

/* !SPACE!

$space %internal reader_room, reader_blame, reader_blame_where, skip_prim
$space %internal skip_vector_body, skip_fields
$space %internal field_index, field_reader
$space %export mtp_reader_init, mtp_read_u32, mtp_reader_peek_u32
$space %export mtp_read_i32, mtp_read_i64
$space %export mtp_read_double, mtp_read_raw, mtp_read_bytes, mtp_read_string
$space %export mtp_skip_object, mtp_object_parse, mtp_object_at
$space %export mtp_reader_reason, mtp_reader_explain
$space %export mtp_object_has, mtp_object_i32, mtp_object_i64
$space %export mtp_object_str, mtp_object_vector

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



#include <stdio.h>
#include <string.h>

#include "mtp_internal.h"

/*
 * Records the first failure only.  Later frames of an unwinding walk would
 * otherwise overwrite the innermost cause with the outermost one, which is the
 * least useful of the two: the caller wants the field that actually stopped it,
 * not the top-level constructor it was nested in.
 */
static void
reader_blame(mtp_reader_t *r, int reason, const char *ctor, const char *field,
    uint32_t bad_id)
{
	if (r->reason != MTP_RE_OK) {
		return;
	}
	r->reason = reason;
	r->bad_id = bad_id;
	r->ctor_name = ctor;
	r->field_name = field;
	r->fail_pos = r->pos;
}

/*
 * The frame that knows *why* is the innermost one; the frame that knows *where*
 * is its caller.  So names are filled in on the way out, and only if still
 * empty -- the deepest caller that has them wins, which is the one closest to
 * the actual failure.
 */
static void
reader_blame_where(mtp_reader_t *r, const char *ctor, const char *field)
{
	if (r->ctor_name == NULL) {
		r->ctor_name = ctor;
	}
	if (r->field_name == NULL) {
		r->field_name = field;
	}
}

static int
reader_room(mtp_reader_t *r, size_t need)
{
	if (r->error) {
		return (0);
	}
	if (need > r->len - r->pos) {
		r->error = 1;
		reader_blame(r, MTP_RE_SHORT, NULL, NULL, 0);
		return (0);
	}
	return (1);
}

void
mtp_reader_init(mtp_reader_t *r, const void *buf, size_t len)
{
	r->buf = (const uint8_t *)buf;
	r->len = len;
	r->pos = 0;
	r->error = 0;
	r->reason = MTP_RE_OK;
	r->bad_id = 0;
	r->ctor_name = NULL;
	r->field_name = NULL;
	r->fail_pos = 0;
}

const char *
mtp_reader_reason(const mtp_reader_t *r)
{
	static const char *const names[] = {
		"ok",
		"truncated",
		"unknown constructor",
		"expected a vector header",
		"nested too deep",
		"constructor has more fields than the walker holds",
		"field gated on a flags word this build does not read",
	};

	if (r == NULL || r->reason < 0 ||
	    (size_t)r->reason >= sizeof(names) / sizeof(names[0])) {
		return ("unknown");
	}
	return (names[r->reason]);
}

size_t
mtp_reader_explain(const mtp_reader_t *r, char *out, size_t cap)
{
	if (out == NULL || cap == 0) {
		return (0);
	}
	if (r == NULL) {
		out[0] = '\0';
		return (0);
	}
	if (r->reason == MTP_RE_UNKNOWN_CTOR) {
		return ((size_t)snprintf(out, cap, "%s at %s.%s (offset %u): "
		    "id %08x is not in the layer %d table",
		    mtp_reader_reason(r),
		    r->ctor_name != NULL ? r->ctor_name : "?",
		    r->field_name != NULL ? r->field_name : "?",
		    (unsigned int)r->fail_pos, r->bad_id, MTP_LAYER));
	}
	return ((size_t)snprintf(out, cap, "%s at %s.%s (offset %u of %u)",
	    mtp_reader_reason(r),
	    r->ctor_name != NULL ? r->ctor_name : "?",
	    r->field_name != NULL ? r->field_name : "?",
	    (unsigned int)r->fail_pos, (unsigned int)r->len));
}

uint32_t
mtp_read_u32(mtp_reader_t *r)
{
	uint32_t	v;

	if (!reader_room(r, 4)) {
		return (0);
	}
	v = (uint32_t)r->buf[r->pos] |
	    ((uint32_t)r->buf[r->pos + 1] << 8) |
	    ((uint32_t)r->buf[r->pos + 2] << 16) |
	    ((uint32_t)r->buf[r->pos + 3] << 24);
	r->pos += 4;
	return (v);
}

uint32_t
mtp_reader_peek_u32(const mtp_reader_t *r)
{
	if (r->error || r->pos > r->len || r->len - r->pos < 4) {
		return (0);
	}
	return ((uint32_t)r->buf[r->pos] |
	    ((uint32_t)r->buf[r->pos + 1] << 8) |
	    ((uint32_t)r->buf[r->pos + 2] << 16) |
	    ((uint32_t)r->buf[r->pos + 3] << 24));
}

int32_t
mtp_read_i32(mtp_reader_t *r)
{
	return ((int32_t)mtp_read_u32(r));
}

int64_t
mtp_read_i64(mtp_reader_t *r)
{
	uint64_t	lo, hi;

	lo = mtp_read_u32(r);
	hi = mtp_read_u32(r);
	return ((int64_t)(lo | (hi << 32)));
}

double
mtp_read_double(mtp_reader_t *r)
{
	uint64_t	lo, hi, bits;
	double		v;

	lo = mtp_read_u32(r);
	hi = mtp_read_u32(r);
	bits = lo | (hi << 32);
	memcpy(&v, &bits, sizeof(v));
	return (v);
}

const uint8_t *
mtp_read_raw(mtp_reader_t *r, size_t len)
{
	const uint8_t	*p;

	if (!reader_room(r, len)) {
		return (NULL);
	}
	p = r->buf + r->pos;
	r->pos += len;
	return (p);
}


const uint8_t *
mtp_read_bytes(mtp_reader_t *r, size_t *out_len)
{
	const uint8_t	*p;
	size_t		len, head, total, pad;

	if (out_len != NULL) {
		*out_len = 0;
	}
	if (!reader_room(r, 1)) {
		return (NULL);
	}
	len = r->buf[r->pos];
	if (len < 254) {
		head = 1;
	} else {
		if (!reader_room(r, 4)) {
			return (NULL);
		}
		len = (size_t)r->buf[r->pos + 1] |
		    ((size_t)r->buf[r->pos + 2] << 8) |
		    ((size_t)r->buf[r->pos + 3] << 16);
		head = 4;
	}

	total = head + len;
	pad = (4 - (total % 4)) % 4;
	if (!reader_room(r, total + pad)) {
		return (NULL);
	}
	p = r->buf + r->pos + head;
	r->pos += total + pad;
	if (out_len != NULL) {
		*out_len = len;
	}
	return (p);
}


size_t
mtp_read_string(mtp_reader_t *r, char *out, size_t cap)
{
	const uint8_t	*p;
	size_t		len, n;

	if (out == NULL || cap == 0) {
		(void)mtp_read_bytes(r, NULL);
		return (0);
	}
	out[0] = '\0';
	p = mtp_read_bytes(r, &len);
	if (p == NULL) {
		return (0);
	}

	n = (len < cap - 1) ? len : cap - 1;
	if (n < len) {
		while (n > 0 && (p[n] & 0xC0u) == 0x80u) {
			n--;
		}
	}
	memcpy(out, p, n);
	out[n] = '\0';
	return (n);
}

static size_t
skip_prim(uint8_t kind)
{
	switch (kind) {
	case MTP_F_INT:
	case MTP_F_FLAGS:
		return (4);
	case MTP_F_LONG:
	case MTP_F_DOUBLE:
		return (8);
	case MTP_F_INT128:
		return (16);
	case MTP_F_INT256:
		return (32);
	default:
		return (0);
	}
}

static int	skip_fields(mtp_reader_t *r, const struct mtp_ctor *ctor,
		    int depth, mtp_object_t *out);

static int
skip_vector_body(mtp_reader_t *r, uint8_t elem_kind, uint32_t bare_id,
    int depth)
{
	const struct mtp_ctor	*ctor;
	uint32_t		count, i;
	size_t			step;

	count = mtp_read_u32(r);
	if (r->error) {
		return (-1);
	}
	if ((size_t)count > r->len - r->pos) {
		r->error = 1;
		reader_blame(r, MTP_RE_SHORT, NULL, "vector count", 0);
		return (-1);
	}

	if (bare_id != 0) {
		ctor = mtp_schema_lookup(bare_id);
		if (ctor == NULL) {
			r->error = 1;
			reader_blame(r, MTP_RE_UNKNOWN_CTOR, NULL,
			    "bare vector element", bare_id);
			return (-1);
		}
		for (i = 0; i < count; i++) {
			if (skip_fields(r, ctor, depth + 1, NULL) != 0) {
				return (-1);
			}
		}
		return (0);
	}

	step = skip_prim(elem_kind);
	for (i = 0; i < count; i++) {
		if (step != 0) {
			if (mtp_read_raw(r, step) == NULL) {
				return (-1);
			}
			continue;
		}
		switch (elem_kind) {
		case MTP_F_STRING:
		case MTP_F_BYTES:
			if (mtp_read_bytes(r, NULL) == NULL) {
				return (-1);
			}
			break;
		case MTP_F_OBJECT:
			if (mtp_skip_object(r, depth + 1) != 0) {
				return (-1);
			}
			break;
		default:
			r->error = 1;
			reader_blame(r, MTP_RE_BAD_VECTOR, NULL,
			    "vector element", 0);
			return (-1);
		}
	}
	return (0);
}

static int
skip_fields(mtp_reader_t *r, const struct mtp_ctor *ctor, int depth,
    mtp_object_t *out)
{
	const struct mtp_field	*f;
	uint32_t		flags[2];
	size_t			step;
	uint16_t		i;
	int			flag_words;

	if (depth > MTP_MAX_DEPTH) {
		r->error = 1;
		reader_blame(r, MTP_RE_DEPTH, ctor->name, NULL, 0);
		return (-1);
	}
	if (out != NULL && ctor->nfields > MTP_MAX_FIELDS) {
		r->error = 1;
		reader_blame(r, MTP_RE_TOO_MANY_FIELDS, ctor->name, NULL, 0);
		return (-1);
	}

	flags[0] = 0;
	flags[1] = 0;
	flag_words = 0;

	for (i = 0; i < ctor->nfields; i++) {
		f = &ctor->fields[i];

		if (f->flag_word != MTP_F_UNCOND) {
			if (f->flag_word > 1) {
				r->error = 1;
				reader_blame(r, MTP_RE_BAD_FLAG_WORD, ctor->name,
				    f->name, 0);
				return (-1);
			}
			if ((flags[f->flag_word] & (1u << f->flag_bit)) == 0) {
				if (out != NULL) {
					out->off[i] = MTP_OFF_ABSENT;
				}
				continue;
			}
		}
		if (out != NULL) {
			out->off[i] = (f->kind == MTP_F_TRUE) ?
			    MTP_OFF_FLAG_TRUE : (uint32_t)r->pos;
		}
		if (f->kind == MTP_F_TRUE) {
			continue;
		}

		if (f->kind == MTP_F_FLAGS) {
			if (flag_words > 1) {
				r->error = 1;
				reader_blame(r, MTP_RE_BAD_FLAG_WORD, ctor->name,
				    f->name, 0);
				return (-1);
			}
			flags[flag_words] = mtp_read_u32(r);
			flag_words++;
			if (r->error) {
				reader_blame_where(r, ctor->name, f->name);
				return (-1);
			}
			continue;
		}

		step = skip_prim(f->kind);
		if (step != 0) {
			if (mtp_read_raw(r, step) == NULL) {
				reader_blame_where(r, ctor->name, f->name);
				return (-1);
			}
			continue;
		}

		switch (f->kind) {
		case MTP_F_STRING:
		case MTP_F_BYTES:
			if (mtp_read_bytes(r, NULL) == NULL) {
				reader_blame_where(r, ctor->name, f->name);
				return (-1);
			}
			break;
		case MTP_F_OBJECT:
			if (mtp_skip_object(r, depth + 1) != 0) {
				reader_blame_where(r, ctor->name, f->name);
				return (-1);
			}
			break;
		case MTP_F_VEC_OBJ:
			if (mtp_read_u32(r) != MTP_ID_vector) {
				r->error = 1;
				reader_blame(r, MTP_RE_BAD_VECTOR, ctor->name,
				    f->name, 0);
				return (-1);
			}
			if (skip_vector_body(r, MTP_F_OBJECT, 0,
			    depth + 1) != 0) {
				reader_blame_where(r, ctor->name, f->name);
				return (-1);
			}
			break;
		case MTP_F_VEC_PRIM:
			if (mtp_read_u32(r) != MTP_ID_vector) {
				r->error = 1;
				reader_blame(r, MTP_RE_BAD_VECTOR, ctor->name,
				    f->name, 0);
				return (-1);
			}
			if (skip_vector_body(r, f->elem_kind, 0,
			    depth + 1) != 0) {
				reader_blame_where(r, ctor->name, f->name);
				return (-1);
			}
			break;
		case MTP_F_VEC_BARE:
			if (r->len - r->pos >= 4 &&
			    (uint32_t)((uint32_t)r->buf[r->pos] |
			    ((uint32_t)r->buf[r->pos + 1] << 8) |
			    ((uint32_t)r->buf[r->pos + 2] << 16) |
			    ((uint32_t)r->buf[r->pos + 3] << 24)) ==
			    MTP_ID_vector) {
				(void)mtp_read_u32(r);
			}
			if (skip_vector_body(r, 0, f->elem_id, depth + 1) != 0) {
				reader_blame_where(r, ctor->name, f->name);
				return (-1);
			}
			break;
		default:
			r->error = 1;
			reader_blame(r, MTP_RE_BAD_VECTOR, ctor->name, f->name,
			    0);
			return (-1);
		}
	}

	if (out != NULL) {
		out->flags[0] = flags[0];
		out->flags[1] = flags[1];
	}
	return (r->error ? -1 : 0);
}

int
mtp_skip_object(mtp_reader_t *r, int depth)
{
	const struct mtp_ctor	*ctor;
	uint32_t		id;

	if (depth > MTP_MAX_DEPTH) {
		r->error = 1;
		reader_blame(r, MTP_RE_DEPTH, NULL, NULL, 0);
		return (-1);
	}
	id = mtp_read_u32(r);
	if (r->error) {
		return (-1);
	}

	if (id == MTP_ID_vector) {
		return (skip_vector_body(r, MTP_F_OBJECT, 0, depth + 1));
	}

	ctor = mtp_schema_lookup(id);
	if (ctor == NULL) {
		r->error = 1;
		reader_blame(r, MTP_RE_UNKNOWN_CTOR, NULL, NULL, id);
		return (-1);
	}
	return (skip_fields(r, ctor, depth, NULL));
}

int
mtp_object_parse(mtp_reader_t *r, mtp_object_t *out)
{
	uint32_t	id;

	memset(out, 0, sizeof(*out));
	id = mtp_read_u32(r);
	if (r->error) {
		return (-1);
	}
	out->ctor = mtp_schema_lookup(id);
	if (out->ctor == NULL) {
		r->error = 1;
		reader_blame(r, MTP_RE_UNKNOWN_CTOR, NULL, "top-level result",
		    id);
		return (-1);
	}
	out->base = r->buf;
	out->base_len = r->len;
	if (skip_fields(r, out->ctor, 0, out) != 0) {
		return (-1);
	}
	out->end = r->pos;
	return (0);
}

static int
field_index(const mtp_object_t *o, const char *field)
{
	uint16_t	i;

	if (o->ctor == NULL) {
		return (-1);
	}
	for (i = 0; i < o->ctor->nfields; i++) {
		if (strcmp(o->ctor->fields[i].name, field) == 0) {
			return ((int)i);
		}
	}
	return (-1);
}

static int
field_reader(const mtp_object_t *o, const char *field, mtp_reader_t *out)
{
	int	idx;

	idx = field_index(o, field);
	if (idx < 0) {
		return (-1);
	}
	if (o->off[idx] == MTP_OFF_ABSENT || o->off[idx] == MTP_OFF_FLAG_TRUE) {
		return (-1);
	}
	mtp_reader_init(out, o->base, o->base_len);
	out->pos = o->off[idx];
	return (0);
}

int
mtp_object_at(const mtp_object_t *o, const char *field, mtp_reader_t *out)
{
	return (field_reader(o, field, out));
}

int
mtp_object_has(const mtp_object_t *o, const char *field)
{
	int	idx;

	idx = field_index(o, field);
	if (idx < 0) {
		return (0);
	}
	return (o->off[idx] != MTP_OFF_ABSENT);
}

int32_t
mtp_object_i32(const mtp_object_t *o, const char *field, int32_t def)
{
	mtp_reader_t	r;
	int32_t		v;

	if (field_reader(o, field, &r) != 0) {
		return (def);
	}
	v = mtp_read_i32(&r);
	return (r.error ? def : v);
}

int64_t
mtp_object_i64(const mtp_object_t *o, const char *field, int64_t def)
{
	mtp_reader_t	r;
	int64_t		v;

	if (field_reader(o, field, &r) != 0) {
		return (def);
	}
	v = mtp_read_i64(&r);
	return (r.error ? def : v);
}

size_t
mtp_object_str(const mtp_object_t *o, const char *field, char *out, size_t cap)
{
	mtp_reader_t	r;

	if (out != NULL && cap != 0) {
		out[0] = '\0';
	}
	if (field_reader(o, field, &r) != 0) {
		return (0);
	}
	return (mtp_read_string(&r, out, cap));
}


int
mtp_object_vector(const mtp_object_t *o, const char *field, mtp_reader_t *out,
    uint32_t *out_count)
{
	uint32_t	count, peek;

	if (out_count != NULL) {
		*out_count = 0;
	}
	if (field_reader(o, field, out) != 0) {
		return (-1);
	}
	if (out->len - out->pos >= 4) {
		peek = (uint32_t)out->buf[out->pos] |
		    ((uint32_t)out->buf[out->pos + 1] << 8) |
		    ((uint32_t)out->buf[out->pos + 2] << 16) |
		    ((uint32_t)out->buf[out->pos + 3] << 24);
		if (peek == MTP_ID_vector) {
			(void)mtp_read_u32(out);
		}
	}
	count = mtp_read_u32(out);
	if (out->error || (size_t)count > out->len - out->pos) {
		out->error = 1;
		return (-1);
	}
	if (out_count != NULL) {
		*out_count = count;
	}
	return (0);
}
