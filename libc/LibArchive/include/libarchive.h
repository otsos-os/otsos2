/* !DEFINES!

$define %type la_zip_entry_cb as callback for extracted zip entry
$define %type la_zip_options as zip extraction options
$define %func la_zip_extract as function with args zip path, options
$define %type la_read_fn as pull callback for compressed input
$define %type la_write_fn as push callback for decompressed output
$define %type la_inflate as DEFLATE decoder state, caller allocated
$define %func la_inflate as function with args const void *, size_t, void *, size_t, size_t *
$define %func la_zlib_inflate as function with args const void *, size_t, void *, size_t, size_t *
$define %func la_inflate_stream as function with args state, window, window size, read, read arg, write, write arg, out
$define %func la_inflate_strerror as function with args int
$define %func la_crc32 as function with args const void *, size_t
$define %func la_crc32_update as function with args uint32_t, const void *, size_t
$define %func la_adler32 as function with args const void *, size_t

*/

/* !SPACE!

$space %export la_zip_entry_cb, la_zip_options, la_zip_extract
$space %export la_read_fn, la_write_fn, la_inflate_t
$space %export la_inflate, la_zlib_inflate, la_inflate_stream
$space %export la_inflate_strerror
$space %export la_crc32, la_crc32_update, la_adler32

*/

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

#ifndef LIBARCHIVE_H
#define LIBARCHIVE_H

#include <stddef.h>
#include <stdint.h>

typedef int	(*la_zip_entry_cb)(const char *name, const char *path,
		    uint64_t size, void *arg);

struct la_zip_options {
	const char	*dest_dir;
	la_zip_entry_cb	on_entry;
	void		*arg;
};

int	la_zip_extract(const char *zip_path,
	    const struct la_zip_options *opts);

#define LA_INF_OK		0
#define LA_INF_INVAL		-1
#define LA_INF_UNSUPPORTED	-4
#define LA_INF_CORRUPT		-5
#define LA_INF_TOO_LARGE	-6
#define LA_INF_IO		-7
#define LA_INF_WINDOW		32768u
typedef long	(*la_read_fn)(void *arg, void *buf, size_t len);
typedef int	(*la_write_fn)(void *arg, const void *buf, size_t len);
typedef struct la_inflate {
	uint16_t	lit_count[16];
	uint16_t	lit_symbol[288];
	uint16_t	dist_count[16];
	uint16_t	dist_symbol[30];
	uint8_t		in_buf[512];
	const uint8_t	*in_data;
	size_t		in_len;
	size_t		in_pos;
	la_read_fn	read;
	void		*read_arg;
	uint8_t		*win;
	size_t		win_cap;
	size_t		win_pos;
	size_t		win_flushed;
	uint64_t	produced;
	la_write_fn	write;
	void		*write_arg;
	uint32_t	acc;
	uint32_t	nbits;
	int		eof;
	int		io_err;
	int		linear;
} la_inflate_t;


int	la_inflate(const void *in, size_t in_len, void *out, size_t out_cap,
	    size_t *out_len);
int	la_zlib_inflate(const void *in, size_t in_len, void *out,
	    size_t out_cap, size_t *out_len);
int	la_inflate_stream(la_inflate_t *st, void *window, size_t win_cap,
	    la_read_fn read, void *read_arg, la_write_fn write,
	    void *write_arg, uint64_t *out_len);
const char *la_inflate_strerror(int err);

uint32_t	la_crc32(const void *data, size_t len);
uint32_t	la_crc32_update(uint32_t crc, const void *data, size_t len);
uint32_t	la_adler32(const void *data, size_t len);

#endif
