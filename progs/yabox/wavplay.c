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

$define %type audio_t as userspace handle to one opened endpoint
$define %type audio_pcm_t as decoded PCM header for one WAV stream
$define %type size_t as native object size

$define %func wavplay_file as function with args const char *, const char *
$define %func main as start with args int, char **, char **

*/

/* !SPACE!

$space %internal wavplay_file
$space %export main

*/

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <libaudio.h>
#include "yabox.h"

static int
wavplay_file(const char *path, const char *epname)
{
	uint8_t		*data;
	uint8_t		*pcm;
	audio_pcm_t	pcm_hdr;
	audio_format_t	fmt;
	audio_t		h;
	FILE		*fp;
	long		len;
	int		code;
	int		ret;

	fp = fopen(path, "rb");
	if (fp == NULL) {
		code = errno;
		ybx_error("wavplay", path, code);
		return (1);
	}
	if (fseek(fp, 0, SEEK_END) != 0) {
		code = errno;
		ybx_error("wavplay", path, code);
		fclose(fp);
		return (1);
	}
	len = ftell(fp);
	if (len <= 0 || fseek(fp, 0, SEEK_SET) != 0) {
		ybx_error("wavplay", path, EINVAL);
		fclose(fp);
		return (1);
	}
	data = (uint8_t *)malloc((size_t)len);
	if (data == NULL) {
		ybx_error("wavplay", path, ENOMEM);
		fclose(fp);
		return (1);
	}
	if (fread(data, 1, (size_t)len, fp) != (size_t)len) {
		code = errno;
		ybx_error("wavplay", path, code);
		free(data);
		fclose(fp);
		return (1);
	}
	fclose(fp);

	if (audioWavParse(data, (uint32_t)len, &pcm_hdr) != 0) {
		ybx_error("wavplay", path, EINVAL);
		free(data);
		return (1);
	}

	if (epname == NULL || epname[0] == '\0') {
		epname = "hda";
	}
	h = audioOpen(epname);
	if (h < 0) {
		code = errno;
		ybx_error("wavplay", epname, code);
		free(data);
		return (1);
	}

	fmt.container = pcm_hdr.container;
	fmt.valid_bits = pcm_hdr.valid_bits;
	fmt.channels = pcm_hdr.channels;
	fmt.rate = pcm_hdr.rate;
	fmt.channel_mask = (pcm_hdr.channels >= 2) ? 0x3 : 0x1;
	if (audioSetFormat(h, &fmt) != 0) {
		code = errno;
		ybx_error("wavplay", "audioSetFormat", code);
		audioClose(h);
		free(data);
		return (1);
	}

	pcm = data + pcm_hdr.data_offset;
	fprintf(stderr, "wavplay: %s %uHz %uc %ubit -> %s\n", path,
	    fmt.rate, fmt.channels, fmt.valid_bits, epname);

	if (audioStart(h) != 0) {
		code = errno;
		ybx_error("wavplay", "audioStart", code);
		audioClose(h);
		free(data);
		return (1);
	}

	ret = audioWrite(h, pcm, (size_t)pcm_hdr.data_length);

	audioStop(h);
	audioClose(h);
	free(data);
	if (ret < 0) {
		code = errno;
		ybx_error("wavplay", path, code);
		return (1);
	}
	return (0);
}

int
main(int argc, char **argv, char **envp)
{
	const char	*path;
	const char	*epname;

	(void)envp;
	if (argc < 2) {
		fprintf(stderr, "usage: wavplay file.wav [endpoint]\n");
		return (1);
	}
	path = argv[1];
	epname = (argc >= 3) ? argv[2] : NULL;
	return (wavplay_file(path, epname));
}
