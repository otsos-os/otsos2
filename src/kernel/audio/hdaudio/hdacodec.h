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

$define %type hda_codec_graph_t as one codec with its fully discovered widget graph
$define %type hda_bus_t as one Intel HDA controller bus instance
$define %type hda_widget_t as one codec node with its full capability set
$define %type ks_device_t as one audio device in the streaming graph

$const HDA_CODEC_MAX_WIDGETS as widgets one codec may have
$const HDA_CODEC_MAX_FORMATS as canonical formats one pin may express
$const HDA_CODEC_MAX_PINS as pins a codec may publish

$define %func hda_codec_discover as function with args hda_bus_t *, u8, hda_codec_graph_t **
$define %func hda_codec_release as procedure with args hda_codec_graph_t *
$define %func hda_codec_verb as function with args hda_bus_t *, u8, u32, u32, u32 *
$define %func hda_codec_widget as function with args hda_codec_graph_t *, u32
$define %func hda_codec_publish as function with args hda_codec_graph_t *, ks_device_t **
*/

/* !SPACE!

$space %internal hda_codec_verb
$space %export hda_codec_discover, hda_codec_release
$space %export hda_codec_widget, hda_codec_publish

*/

#ifndef KERNEL_AUDIO_HDAUDIO_HDACODEC_H
#define KERNEL_AUDIO_HDAUDIO_HDACODEC_H

#include <mlibc/mlibc.h>
#include <kernel/audio/hdaudio/hdabus.h>
#include <kernel/audio/hdaudio/hdawidget.h>
#include <kernel/audio/ks/ksobj.h>
#include <kernel/audio/ks/kstypes.h>

#define	HDA_CODEC_MAX_WIDGETS	HDA_MAX_WIDGETS
#define	HDA_CODEC_MAX_FORMATS	16
#define	HDA_CODEC_MAX_PINS	16

typedef struct hda_codec_graph {
	hda_bus_t		*bus;
	u8			addr;
	u32			vendor_id;
	u32			revision_id;
	u32			subsystem_id;
	u32			node_start;
	u32			node_count;
	u32			group_count;
	hda_widget_t		*widgets[HDA_CODEC_MAX_WIDGETS];
	u32			widget_count;
	u32			pin_count;
	u32			pin_ids[HDA_CODEC_MAX_PINS];
	u32			pin_flows[HDA_CODEC_MAX_PINS];
} hda_codec_graph_t;

int
hda_codec_discover(hda_bus_t *bus, u8 addr, hda_codec_graph_t **outcodec);

void
hda_codec_release(hda_codec_graph_t *codec);

int
hda_codec_verb(hda_bus_t *bus, u8 addr, u32 node, u32 command, u32 *resp);

hda_widget_t *
hda_codec_widget(hda_codec_graph_t *codec, u32 nid);

int
hda_codec_publish(hda_codec_graph_t *codec, ks_device_t **outdev);

#endif