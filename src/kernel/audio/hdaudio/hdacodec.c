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
$define %type ks_pin_t as one directional pin on a device
$define %type u32 as 32 bit unsigned
$define %type u8 as 8 bit unsigned

$define %func hda_codec_discover as function with args hda_bus_t *, u8, hda_codec_graph_t **
$define %func hda_codec_release as procedure with args hda_codec_graph_t *
$define %func hda_codec_verb as function with args hda_bus_t *, u8, u32, u32, u32 *
$define %func hda_codec_widget as function with args hda_codec_graph_t *, u32
$define %func hda_codec_publish as function with args hda_codec_graph_t *, ks_device_t **
$define %func hda_codec_param as function with args hda_bus_t *, u8, u32, u32, u32 *
$define %func hda_codec_node_count as function with args hda_bus_t *, u8, u32 *, u32 *
$define %func hda_codec_fill_widget as procedure with args hda_codec_graph_t *, u32, hda_widget_t *
*/

/* !SPACE!

$space %internal hda_codec_param, hda_codec_node_count
$space %internal hda_codec_fill_widget
$space %export hda_codec_discover, hda_codec_release, hda_codec_verb
$space %export hda_codec_widget, hda_codec_publish

*/

#include <kernel/audio/hdaudio/hdacodec.h>

static u32
hda_codec_param(hda_bus_t *bus, u8 addr, u32 node, u32 param_id, u32 *out)
{
	u32	resp;

	if (hda_codec_verb(bus, addr, node,
	    (HDAC_VERB_GET_PARAMETER << 8) | param_id, &resp) != 0) {
		return (0);
	}
	if (out != NULL) {
		*out = resp;
	}
	return (1);
}

int
hda_codec_verb(hda_bus_t *bus, u8 addr, u32 node, u32 command, u32 *resp)
{
	u32	verb;
	u32	r;

	if (bus == NULL) {
		return (-1);
	}
	if (resp == NULL) {
		resp = &r;
	}

	verb = ((u32)addr << HDA_CODEC_ADDR_SHIFT) |
	    ((node & 0xFF) << 20) | (command & 0xFFFFF);
	return (hda_bus_command(bus, verb, resp));
}


static u32
hda_codec_node_count(hda_bus_t *bus, u8 addr, u32 *groups, u32 *start)
{
	u32	param;

	if (!hda_codec_param(bus, addr, 0, HDAC_PARAM_NODE_COUNT, &param)) {
		return (0);
	}
	if (groups != NULL) {
		*groups = (param >> 8) & 0xFF;
	}
	if (start != NULL) {
		*start = param & 0xFF;
	}
	return ((param >> 16) & 0xFF);
}


static void
hda_codec_fill_conns(hda_codec_graph_t *codec, hda_widget_t *widget)
{
	u32	max_conn;
	u32	response;
	u32	long_form;
	u32	length;
	u32	i;

	if ((widget->caps & HDA_WCAP_CONN_LIST) == 0) {
		widget->conn_count = 0;
		return;
	}
	if (!hda_codec_param(codec->bus, codec->addr, widget->nid,
	    HDAC_PARAM_CONNLIST_LEN, &length)) {
		widget->conn_count = 0;
		return;
	}
	long_form = (length & 0x80) != 0 ? 1 : 0;
	if (long_form) {
		max_conn = length & 0x7F;
	} else {
		max_conn = length & 0x1F;
	}
	if (max_conn > HDA_MAX_CONN) {
		max_conn = HDA_MAX_CONN;
	}
	widget->conn_count = 0;
	response = 0;
	for (i = 0; i < max_conn && widget->conn_count < HDA_MAX_CONN;
	    i++) {
		if (hda_codec_verb(codec->bus, codec->addr, widget->nid,
		    (HDAC_VERB_GET_CONNLIST << 8) | i, &response) != 0) {
			break;
		}
		hda_widget_parse_conns(widget, response, long_form);
	}
}

static void
hda_codec_fill_widget(hda_codec_graph_t *codec, u32 nid, hda_widget_t *widget)
{
	u32	param;

	memset(widget, 0, sizeof(*widget));
	widget->nid = nid;
	if (hda_codec_param(codec->bus, codec->addr, nid,
	    HDAC_PARAM_AUDIO_WIDGET, &param)) {
		hda_widget_parse_caps(widget, param);
	}
	if (widget->type == HDA_WIDGET_AUDIO_OUTPUT ||
	    widget->type == HDA_WIDGET_AUDIO_INPUT) {
		if (hda_codec_param(codec->bus, codec->addr, nid,
		    HDAC_PARAM_PCM, &param)) {
			hda_widget_parse_pcm(&widget->pcm, param);
		}
	}
	if ((widget->caps & HDA_WCAP_IN_AMP) ||
	    widget->type == HDA_WIDGET_UNKNOWN) {
		if (hda_codec_param(codec->bus, codec->addr, nid,
		    HDAC_PARAM_IN_AMP_CAP, &param)) {
			hda_widget_parse_amp(&widget->amp_in, param);
		}
	}
	if ((widget->caps & HDA_WCAP_OUT_AMP) ||
	    widget->type == HDA_WIDGET_UNKNOWN) {
		if (hda_codec_param(codec->bus, codec->addr, nid,
		    HDAC_PARAM_OUT_AMP_CAP, &param)) {
			hda_widget_parse_amp(&widget->amp_out, param);
		}
	}
	if (widget->type == HDA_WIDGET_PIN_COMPLEX) {
		if (hda_codec_param(codec->bus, codec->addr, nid,
		    HDAC_PARAM_PIN_CAP, &param)) {
			widget->pin_caps = param;
			widget->eapd = (param & HDA_PIN_CAP_EAPD) != 0;
		}
		if (hda_codec_verb(codec->bus, codec->addr, nid,
		    (HDAC_VERB_GET_PIN_SENSE << 8), &param) == 0) {
			widget->power_state = param;
		}
	}
	hda_codec_fill_conns(codec, widget);
}

static const ks_format_t hda_rate_formats[] = {
	{ KS_DATARANGE_PCM_S16LE, 16, 2, 44100, 0x3 },
	{ KS_DATARANGE_PCM_S16LE, 16, 2, 48000, 0x3 },
	{ KS_DATARANGE_PCM_S24LE, 24, 2, 48000, 0x3 },
	{ KS_DATARANGE_PCM_S32LE, 32, 2, 48000, 0x3 },
};

int
hda_codec_discover(hda_bus_t *bus, u8 addr, hda_codec_graph_t **outcodec)
{
	hda_codec_graph_t	*codec;
	hda_widget_t	*widget;
	u32		groups;
	u32		nodes;
	u32		start;
	u32		last;
	u32		nid;

	if (bus == NULL || addr >= HDA_BUS_MAX_CODECS ||
	    outcodec == NULL) {
		return (-1);
	}
	codec = (hda_codec_graph_t *)kmem_calloc(1, sizeof(*codec));
	if (codec == NULL) {
		return (-1);
	}
	codec->bus = bus;
	codec->addr = addr;
	if (hda_codec_param(bus, addr, 0, HDAC_PARAM_VENDOR_ID,
	    &codec->vendor_id)) {
		hda_codec_param(bus, addr, 0, HDAC_PARAM_REV_ID,
		    &codec->revision_id);
	}
	nodes = hda_codec_node_count(bus, addr, &groups, &start);
	codec->node_count = nodes;
	codec->group_count = groups;
	codec->node_start = start;

	last = HDA_MAX_WIDGETS;
	for (nid = start; nid < last &&
	    codec->widget_count < HDA_CODEC_MAX_WIDGETS; nid++) {
		widget = (hda_widget_t *)kmem_calloc(1, sizeof(*widget));
		if (widget == NULL) {
			break;
		}
		if (!hda_codec_param(codec->bus, codec->addr, nid,
		    HDAC_PARAM_AUDIO_WIDGET, &widget->caps)) {
			kmem_free(widget);
			break;
		}
		if (widget->caps == 0) {
			if (codec->widget_count != 0) {
				kmem_free(widget);
				break;
			}
			kmem_free(widget);
			continue;
		}
		hda_codec_fill_widget(codec, nid, widget);
		codec->widgets[codec->widget_count++] = widget;
		if (widget->type == HDA_WIDGET_PIN_COMPLEX &&
		    codec->pin_count < HDA_CODEC_MAX_PINS) {
			codec->pin_ids[codec->pin_count] = nid;
			if (widget->pin_caps & HDA_PIN_CAP_OUT) {
				codec->pin_flows[codec->pin_count] =
				    KS_DATAFLOW_OUT;
			} else if (widget->pin_caps & HDA_PIN_CAP_IN) {
				codec->pin_flows[codec->pin_count] =
				    KS_DATAFLOW_IN;
			} else {
				codec->pin_flows[codec->pin_count] = 0;
			}
			codec->pin_count++;
		}
	}
	*outcodec = codec;
	return (0);
}

void
hda_codec_release(hda_codec_graph_t *codec)
{
	u32	i;

	if (codec == NULL) {
		return;
	}
	for (i = 0; i < codec->widget_count; i++) {
		kmem_free(codec->widgets[i]);
		codec->widgets[i] = NULL;
	}
	kmem_free(codec);
}

hda_widget_t *
hda_codec_widget(hda_codec_graph_t *codec, u32 nid)
{
	u32	i;

	if (codec == NULL) {
		return (NULL);
	}
	for (i = 0; i < codec->widget_count; i++) {
		if (codec->widgets[i] != NULL &&
		    codec->widgets[i]->nid == nid) {
			return (codec->widgets[i]);
		}
	}
	return (NULL);
}

int
hda_codec_publish(hda_codec_graph_t *codec, ks_device_t **outdev)
{
	ks_device_t	*dev;
	ks_pin_t	*pin;
	char		name[KS_DEVICE_NAME_MAX];
	u32		i;

	if (codec == NULL || outdev == NULL) {
		return (-1);
	}
	snprintf(name, sizeof(name), "hda-%x-%08lx",
	    codec->addr, (unsigned long)codec->vendor_id);
	dev = ks_device_create(name, codec);
	if (dev == NULL) {
		return (-1);
	}
	for (i = 0; i < codec->pin_count; i++) {
		if (codec->pin_flows[i] == 0) {
			continue;
		}
		pin = (ks_pin_t *)kmem_calloc(1, sizeof(*pin));
		if (pin == NULL) {
			break;
		}
		pin->flow = (ks_dataflow_t)codec->pin_flows[i];
		if (i < (sizeof(hda_rate_formats) /
		    sizeof(hda_rate_formats[0]))) {
			pin->formats = hda_rate_formats;
			pin->formats_count = 4;
		}
		if (ks_device_add_pin(dev, pin) != 0) {
			kmem_free(pin);
		}
	}
	*outdev = dev;
	return (0);
}