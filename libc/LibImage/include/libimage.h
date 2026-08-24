/* !DEFINES!

$define %type li_image as decoded raster image in ARGB8888
$define %type uint32_t as 32 bit unsigned
$define %func li_probe as function with args const void *, size_t
$define %func li_load_memory as function with args const void *, size_t, li_image **
$define %func li_load_file as function with args const char *, li_image **
$define %func li_free as procedure with args li_image *
$define %func li_strerror as function with args int
$define %func li_png_load as function with args const void *, size_t, li_image **

*/

/* !SPACE!

$space %export li_image_t
$space %export li_probe, li_load_memory, li_load_file, li_free, li_strerror
$space %export li_png_load

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

#ifndef LIBIMAGE_H
#define LIBIMAGE_H

#include <stddef.h>
#include <stdint.h>

#define LI_VERSION_MAJOR	0
#define LI_VERSION_MINOR	1

#define LI_OK			0
#define LI_ERR_INVAL		-1
#define LI_ERR_NOMEM		-2
#define LI_ERR_FORMAT		-3
#define LI_ERR_UNSUPPORTED	-4
#define LI_ERR_CORRUPT		-5
#define LI_ERR_TOO_LARGE	-6
#define LI_ERR_IO		-7

#define LI_FORMAT_UNKNOWN	0
#define LI_FORMAT_PNG		1
#define LI_MAX_DIM		8192u
#define LI_MAX_PIXELS		(16u * 1024u * 1024u)
#define LI_MAX_FILE_BYTES	(32u * 1024u * 1024u)

typedef struct li_image {
	uint32_t	*pixels;
	uint32_t	width;
	uint32_t	height;
	uint32_t	pitch;
	int		format;
	int		has_alpha;
} li_image_t;

int	li_probe(const void *data, size_t len);
int	li_load_memory(const void *data, size_t len, li_image_t **out);
int	li_load_file(const char *path, li_image_t **out);
void	li_free(li_image_t *img);
const char *li_strerror(int err);
int	li_png_load(const void *data, size_t len, li_image_t **out);


#endif
