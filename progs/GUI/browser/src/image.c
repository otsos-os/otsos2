/* !DEFINES!

$define %type browser_image_entry as one cached raster image keyed by absolute URL
$define %type browser_state as whole browser instance state
$define %type libg_image_t as LibG decoded raster handle
$define %type libg_rect_t as integer rectangle
$define %func browser_image_cache_free as procedure with args browser_state *
$define %func browser_image_find as function with args browser_state *, const char *
$define %func browser_image_evict as procedure with args browser_state *
$define %func browser_image_note as procedure with args browser_state *, const char *, libg_image_t *
$define %func browser_image_cached as function with args const browser_state *, const char *
$define %func browser_image_key as procedure with args const browser_state *, const char *, char *, size_t
$define %func browser_html_escape as function with args char *, size_t, const char *
$define %func browser_image_document as function with args browser_state *, const void *, size_t
$define %func browser_image_size as function with args void *, const char *, int32_t *, int32_t *
$define %func browser_image_draw as function with args void *, libg_context *, libg_rect_t, const char *

*/

/* !SPACE!

$space %internal browser_image_find, browser_image_evict, browser_html_escape
$space %export browser_image_cache_free, browser_image_note, browser_image_key
$space %export browser_image_cached, browser_image_document
$space %export browser_image_size, browser_image_draw

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

#include <browser.h>
#include <libg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


static browser_image_entry_t *
browser_image_find(browser_state_t *st, const char *url)
{
	int	i;

	for (i = 0; i < st->image_count; i++) {
		if (st->image_cache[i].url != NULL &&
		    strcmp(st->image_cache[i].url, url) == 0) {
			return (&st->image_cache[i]);
		}
	}
	return (NULL);
}


static void
browser_image_evict(browser_state_t *st)
{
	if (st->image_count <= 0) {
		return;
	}
	free(st->image_cache[0].url);
	libgImageFree(st->image_cache[0].image);
	memmove(&st->image_cache[0], &st->image_cache[1],
	    sizeof(browser_image_entry_t) * (size_t)(st->image_count - 1));
	st->image_count--;
	memset(&st->image_cache[st->image_count], 0,
	    sizeof(browser_image_entry_t));
}

void
browser_image_note(browser_state_t *st, const char *url, libg_image_t *img)
{
	browser_image_entry_t	*e;

	if (st == NULL || url == NULL || url[0] == '\0') {
		libgImageFree(img);
		return;
	}

	e = browser_image_find(st, url);
	if (e != NULL) {
		libgImageFree(e->image);
		e->image = img;
		e->failed = (img == NULL);
		return;
	}

	if (st->image_count >= BROWSER_IMAGE_CACHE_MAX) {
		browser_image_evict(st);
	}
	e = &st->image_cache[st->image_count];
	e->url = strdup(url);
	if (e->url == NULL) {
		libgImageFree(img);
		return;
	}
	e->image = img;
	e->failed = (img == NULL);
	st->image_count++;
}


int
browser_image_cached(const browser_state_t *st, const char *url)
{
	int	i;

	if (st == NULL || url == NULL || url[0] == '\0') {
		return (0);
	}
	for (i = 0; i < st->image_count; i++) {
		if (st->image_cache[i].url != NULL &&
		    strcmp(st->image_cache[i].url, url) == 0) {
			return (1);
		}
	}
	return (0);
}

void
browser_image_cache_free(browser_state_t *st)
{
	int	i;

	if (st == NULL) {
		return;
	}
	for (i = 0; i < st->image_count; i++) {
		free(st->image_cache[i].url);
		libgImageFree(st->image_cache[i].image);
	}
	memset(st->image_cache, 0, sizeof(st->image_cache));
	st->image_count = 0;
}


void
browser_image_key(const browser_state_t *st, const char *ref, char *out,
    size_t max_out)
{
	if (out == NULL || max_out == 0) {
		return;
	}
	out[0] = '\0';
	if (st == NULL || ref == NULL || ref[0] == '\0') {
		return;
	}
	if (strncasecmp(ref, "http://", 7) != 0 &&
	    strncasecmp(ref, "https://", 8) != 0) {
		browser_url_resolve(st->current_url, ref, out, max_out);
	} else {
		browser_url_normalize(ref, out, max_out);
	}
}


static size_t
browser_html_escape(char *dst, size_t cap, const char *src)
{
	const char	*rep;
	size_t		 n, rlen;

	n = 0;
	if (dst == NULL || cap == 0) {
		return (0);
	}
	dst[0] = '\0';
	if (src == NULL) {
		return (0);
	}

	for (; *src != '\0'; src++) {
		switch (*src) {
		case '&':
			rep = "&amp;";
			break;
		case '"':
			rep = "&quot;";
			break;
		case '<':
			rep = "&lt;";
			break;
		case '>':
			rep = "&gt;";
			break;
		default:
			rep = NULL;
			break;
		}
		if (rep != NULL) {
			rlen = strlen(rep);
			if (n + rlen + 1 > cap) {
				break;
			}
			memcpy(dst + n, rep, rlen);
			n += rlen;
		} else {
			if (n + 2 > cap) {
				break;
			}
			dst[n++] = *src;
		}
	}
	dst[n] = '\0';
	return (n);
}


char *
browser_image_document(browser_state_t *st, const void *data, size_t len)
{
	libg_image_t	*img;
	char		*out;
	char		*esc;
	char		 key[BROWSER_MAX_URL];
	int		 n;

	if (st == NULL || data == NULL || len == 0) {
		return (NULL);
	}
	img = NULL;
	if (libgImageLoad(data, len, &img) != LIBG_OK || img == NULL) {
		return (NULL);
	}


	browser_image_key(st, st->current_url, key, sizeof(key));
	if (key[0] == '\0') {
		libgImageFree(img);
		return (NULL);
	}


	esc = (char *)malloc(BROWSER_MAX_URL * 6 + 1);
	out = (char *)malloc(BROWSER_MAX_URL * 12 + 512);
	if (esc == NULL || out == NULL) {
		free(esc);
		free(out);
		libgImageFree(img);
		return (NULL);
	}
	(void)browser_html_escape(esc, BROWSER_MAX_URL * 6 + 1, key);

	n = snprintf(out, BROWSER_MAX_URL * 12 + 512,
	    "<html><head><title>%s (%d\xc3\x97%d)</title></head>"
	    "<body><img src=\"%s\" width=\"%d\" height=\"%d\" alt=\"%s\">"
	    "</body></html>",
	    esc, (int)libgImageWidth(img), (int)libgImageHeight(img),
	    esc, (int)libgImageWidth(img), (int)libgImageHeight(img), esc);
	free(esc);
	if (n < 0) {
		free(out);
		libgImageFree(img);
		return (NULL);
	}

	browser_image_note(st, key, img);
	return (out);
}

int
browser_image_size(void *userdata, const char *ref, int32_t *out_w,
    int32_t *out_h)
{
	browser_image_entry_t	*e;
	browser_state_t		*st;
	char			 key[BROWSER_MAX_URL];

	st = (browser_state_t *)userdata;
	if (st == NULL || ref == NULL || out_w == NULL || out_h == NULL) {
		return (-1);
	}
	if (ref[0] == '<') {
		return (-1);
	}

	browser_image_key(st, ref, key, sizeof(key));
	if (key[0] == '\0') {
		return (-1);
	}
	e = browser_image_find(st, key);
	if (e == NULL || e->image == NULL) {
		return (-1);
	}
	*out_w = libgImageWidth(e->image);
	*out_h = libgImageHeight(e->image);
	if (*out_w <= 0 || *out_h <= 0) {
		return (-1);
	}
	return (0);
}

int
browser_image_draw(void *userdata, libg_context_t *ctx, libg_rect_t rect,
    const char *ref)
{
	browser_image_entry_t	*e;
	browser_state_t		*st;
	char			 key[BROWSER_MAX_URL];

	st = (browser_state_t *)userdata;
	if (st == NULL || ctx == NULL || ref == NULL || ref[0] == '\0') {
		return (-1);
	}
	if (ref[0] == '<') {
		return (browser_svg_draw(userdata, ctx, rect, ref));
	}

	browser_image_key(st, ref, key, sizeof(key));
	if (key[0] == '\0') {
		return (-1);
	}
	e = browser_image_find(st, key);
	if (e == NULL || e->image == NULL) {
		return (-1);
	}


	libgImageDrawScaled(ctx, e->image, rect);
	return (0);
}
