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

$define %type hda_softc_t as per-controller device state (soft context)
$define %type hda_stream_t as one hardware DMA stream the controller owns
$define %type device_t as pointer to newbus device
$define %type pc_miniport_t as hardware ops the port-class drives
$define %type ks_format_t as canonical sample format descriptor
$define %type ks_state_t as run state of a streaming pin
$define %type ks_position_t as play/capture cursor position pair
$define %type dma_seg_t as one scatter/gather segment

$define %func hda_attach as function with args device_t
$define %func hda_detach as function with args device_t
$define %func hda_intr as function with args void *
$define %func hda_reg_read32 as function with args hda_softc_t *, u32
$define %func hda_miniport as function with args void
$define %func hda_miniport_probe_format as function with args const pc_miniport_t *, const ks_format_t *
$define %func hda_miniport_create_stream as function with args const pc_miniport_t *, const ks_format_t *, u32, pc_stream_t **
$define %func hda_miniport_destroy_stream as function with args const pc_miniport_t *, pc_stream_t *
$define %func hda_miniport_set_state as function with args const pc_miniport_t *, pc_stream_t *, ks_state_t
$define %func hda_miniport_position as function with args const pc_miniport_t *, pc_stream_t *, ks_position_t *
$define %func hda_miniport_set_volume as function with args const pc_miniport_t *, pc_stream_t *, uint
$define %func hda_miniport_get_volume as function with args const pc_miniport_t *, pc_stream_t *
$define %func hda_amp_step_for_volume as function with args const hda_stream_t *, uint
$define %func hda_stream_setup as function with args hda_softc_t *, hda_stream_t *
$define %func hda_stream_teardown as procedure with args hda_softc_t *, hda_stream_t *
$define %func hda_reset_stream as procedure with args hda_softc_t *, hda_stream_t *
$define %func hda_run_stream as procedure with args hda_softc_t *, hda_stream_t *, int
$define %func hda_codec_route_render as procedure with args hda_softc_t *, u32, const ks_format_t *
$define %func hda_codec_format_from_ks as function with args const ks_format_t *
$define %func hda_lookup_quirks as function with args u32
$define %func hda_stream_tag as function with args hda_softc_t *, const hda_stream_t *
*/

/* !SPACE!

$space %internal hda_stream_setup, hda_stream_teardown
$space %internal hda_reset_stream, hda_run_stream, hda_map_bar
$space %internal hda_poll
$space %internal hda_miniport_probe_format, hda_miniport_create_stream
$space %internal hda_miniport_destroy_stream, hda_miniport_set_state
$space %internal hda_miniport_position, hda_codec_route_render
$space %internal hda_miniport_set_volume, hda_miniport_get_volume
$space %internal hda_amp_step_for_volume
$space %internal hda_codec_format_from_ks, hda_lookup_quirks, hda_stream_tag
$space %export hda_attach, hda_detach, hda_miniport
$space %export hda_reg_read32, hda_reg_write32

*/

#include <kernel/drivers/audio/intel-hda/hda.h>
#include <kernel/audio/hdaudio/hdacodec.h>
#include <kernel/audio/portcls/pcport.h>
#include <kernel/audio/api/api_audio.h>
#include <kernel/audio/sysaudio/sysaudio.h>
#include <kernel/drivers/timer.h>
#include <mlibc/stdio.h>

u32
hda_reg_read32(hda_softc_t *sc, u32 off)
{
	if (sc == NULL || sc->mmio == NULL) {
		return (0);
	}
	return (hda_bus_reg_read32(&sc->bus, off));
}

u8
hda_reg_read8(hda_softc_t *sc, u32 off)
{
	if (sc == NULL || sc->mmio == NULL) {
		return (0);
	}
	return (hda_bus_reg_read8(&sc->bus, off));
}

u16
hda_reg_read16(hda_softc_t *sc, u32 off)
{
	if (sc == NULL || sc->mmio == NULL) {
		return (0);
	}
	return (hda_bus_reg_read16(&sc->bus, off));
}

void
hda_reg_write32(hda_softc_t *sc, u32 off, u32 val)
{
	if (sc != NULL && sc->mmio != NULL) {
		hda_bus_reg_write32(&sc->bus, off, val);
	}
}

void
hda_reg_write16(hda_softc_t *sc, u32 off, u16 val)
{
	if (sc != NULL && sc->mmio != NULL) {
		hda_bus_reg_write16(&sc->bus, off, val);
	}
}

void
hda_reg_write8(hda_softc_t *sc, u32 off, u8 val)
{
	if (sc != NULL && sc->mmio != NULL) {
		hda_bus_reg_write8(&sc->bus, off, val);
	}
}

static u32	hda_codec_format_from_ks(const ks_format_t *fmt);

static u32
hda_format_to_sdfmt(const ks_format_t *fmt)
{
	return (hda_codec_format_from_ks(fmt));
}



static const hda_quirk_t hda_quirk_table[] = {
	{ 0x1af4, 0x0022, HDA_QUIRK_NONE },		/* QEMU intel-hda */
	{ 0x8384, 0xffff, HDA_QUIRK_STREAM_TAG_SDO },	/* VB SigmaTel */
};

static u32
hda_lookup_quirks(u32 codec_vendor)
{
	u32	i;

	for (i = 0; i < sizeof(hda_quirk_table) / sizeof(hda_quirk_table[0]);
	    i++) {
		const hda_quirk_t	*q;

		q = &hda_quirk_table[i];
		if ((q->codec_vendor == 0xffff ||
		    q->codec_vendor == (u16)(codec_vendor >> 16)) &&
		    (q->codec_device == 0xffff ||
		    q->codec_device == (u16)codec_vendor)) {
			return (q->flags);
		}
	}
	return (HDA_QUIRK_NONE);
}

static u32
hda_stream_tag(hda_softc_t *sc, const hda_stream_t *stream)
{
	if ((sc->quirks & HDA_QUIRK_STREAM_TAG_SDO) != 0 &&
	    stream->direction == 0) {
		return ((u32)stream->desc - HDA_OUT_DESC_LO);
	}
	return ((u32)stream->desc + 1);
}


static void
hda_delay_us(u32 us)
{
	volatile u32	i;

	for (i = 0; i < us * 100; i++) {
		__asm__ volatile("pause");
	}
}

static void
hda_dump_desc(hda_softc_t *sc, u32 base, const char *tag)
{
	drivers_log("[HDA] desc[%s] base=0x%02x ctl=0x%08x sts=0x%02x "
	    "lpib=%u cbl=%u lvi=%u fifow=0x%02x fifos=0x%02x fmt=0x%04x "
	    "bdpl=0x%08x bdpu=0x%08x\n", tag, base,
	    hda_reg_read32(sc, base + 0x00),
	    hda_reg_read8(sc, base + 0x03),
	    hda_reg_read32(sc, base + 0x04),
	    hda_reg_read32(sc, base + 0x08),
	    hda_reg_read16(sc, base + 0x0C),
	    hda_reg_read8(sc, base + 0x0E),
	    hda_reg_read8(sc, base + 0x10),
	    hda_reg_read16(sc, base + 0x12),
	    hda_reg_read32(sc, base + 0x18),
	    hda_reg_read32(sc, base + 0x1C));
}

static void
hda_reset_stream(hda_softc_t *sc, hda_stream_t *stream)
{
	u64	spin;
	u32	base;
	u32	ctl;

	base = HDAC_SD_BASE + stream->desc * HDAC_SD_STRIDE;
	hda_dump_desc(sc, base, "before-reset");
	hda_reg_write32(sc, base + HDAC_SD_CTL, HDAC_SDCTL_SRST);
	hda_delay_us(HDA_RESET_POLL_US);
	hda_reg_write32(sc, base + HDAC_SD_CTL, 0);
	ctl = 0;
	spin = 0;
	for (spin = 0; spin < HDA_RESET_POLLS; spin++) {
		ctl = hda_reg_read32(sc, base + HDAC_SD_CTL);
		if ((ctl & HDAC_SDCTL_SRST) == 0) {
			break;
		}
		hda_delay_us(HDA_RESET_POLL_US);
	}
	drivers_log("[HDA] reset stream %u: srst cleared=%u polls=%llu "
	    "ctl=0x%08x\n", stream->desc,
	    (ctl & HDAC_SDCTL_SRST) == 0,
	    (unsigned long long)spin, ctl);
	stream->active = 0;
	hda_dump_desc(sc, base, "after-reset");
}

static void
hda_run_stream(hda_softc_t *sc, hda_stream_t *stream, int run)
{
	u32	base;
	u32	ctl;

	base = HDAC_SD_BASE + stream->desc * HDAC_SD_STRIDE;
	ctl = hda_reg_read32(sc, base + HDAC_SD_CTL);
	if (run) {
		ctl |= HDAC_SDCTL_RUN;
	} else {
		ctl &= ~HDAC_SDCTL_RUN;
	}
	hda_reg_write32(sc, base + HDAC_SD_CTL, ctl);
	stream->active = run ? 1 : 0;
	drivers_log("[HDA] run stream %u run=%u ctl=0x%08x sts=0x%02x "
	    "lpib=%u\n", stream->desc, run,
	    hda_reg_read32(sc, base + HDAC_SD_CTL),
	    hda_reg_read8(sc, base + HDAC_SD_STS),
	    hda_reg_read32(sc, base + HDAC_SD_LPIB));
	if (run) {
		drivers_log("[HDA] bdl[0] addr=0x%llx len=%u ioc=%u | "
		    "bdl[1] addr=0x%llx len=%u ioc=%u\n",
		    (unsigned long long)stream->bdl[0].address,
		    stream->bdl[0].length, stream->bdl[0].ioc,
		    (unsigned long long)stream->bdl[1].address,
		    stream->bdl[1].length, stream->bdl[1].ioc);
	}
}


static int
hda_stream_setup(hda_softc_t *sc, hda_stream_t *stream)
{
	u64	phys;
	u32	base;
	u32	i;
	u32	byte_off;
	u32	ctl;

	if (sc == NULL || stream == NULL) {
		return (-1);
	}
	base = HDAC_SD_BASE + stream->desc * HDAC_SD_STRIDE;
	hda_reset_stream(sc, stream);

	{
		u16	want, got;

		want = (u16)hda_format_to_sdfmt(&stream->format);
		hda_reg_write16(sc, base + HDAC_SD_FMT, want);
		got = hda_reg_read16(sc, base + HDAC_SD_FMT);
		{
			u32	raw32;

			raw32 = hda_reg_read32(sc, base + 0x10);
			drivers_log("[HDA] fmt raw: dword@0x10=0x%08x "
			    "fifos8=0x%02x fifow8=0x%02x fmt16=0x%04x "
			    "want16=0x%04x\n", raw32,
			    hda_reg_read8(sc, base + 0x10),
			    hda_reg_read8(sc, base + 0x0E),
			    hda_reg_read16(sc, base + 0x12), want);
		}
		drivers_log("[HDA] fmt probe: container=0x%04x bits=%u "
		    "ch=%u rate=%u wrote=0x%04x read=0x%04x base=0x%02x\n",
		    stream->format.container, stream->format.valid_bits,
		    stream->format.channels, stream->format.rate,
		    want, got, base);
	}
	hda_reg_write32(sc, base + HDAC_SD_CBL, stream->total_bytes);
	hda_reg_write16(sc, base + HDAC_SD_LVI,
	    stream->nfrags - 1);

	hda_reg_write8(sc, base + HDAC_SD_FIFOW, HDA_FIFO_SIZE);
	hda_reg_write8(sc, base + HDAC_SD_FIFOS, HDA_FIFO_SIZE);
	hda_dump_desc(sc, base, "after-fmt-cbl-lvi-fifo");

	phys = stream->buf_mem.phys;
	byte_off = 0;
	for (i = 0; i < stream->nfrags; i++) {
		stream->bdl[i].address = phys + byte_off;
		stream->bdl[i].length = stream->frag_bytes;
		stream->bdl[i].ioc = HDAC_BDL_IOC;
		byte_off += stream->frag_bytes;
	}
	__sync_synchronize();


	hda_reg_write32(sc, base + HDAC_SD_BDPL,
	    (u32)stream->bdl_mem.phys);
	hda_reg_write32(sc, base + HDAC_SD_BDPL_BASE + 4,
	    (u32)(stream->bdl_mem.phys >> 32));
	hda_dump_desc(sc, base, "after-bdl");


	ctl = 0;
	if (stream->direction != 0) {
		ctl |= HDAC_SDCTL_DIR;
	}
	ctl |= (hda_stream_tag(sc, stream) << HDAC_SDCTL_STRM_SHIFT) &
	    HDAC_SDCTL_STRM_MASK;
	ctl |= HDAC_SDCTL_IOCE;
	hda_reg_write32(sc, base + HDAC_SD_CTL, ctl);
	hda_reg_write32(sc, HDAC_DPLBASE, 0);
	hda_reg_write32(sc, HDAC_DPUBASE, 0);

	drivers_log("[HDA] setup stream %u: cbl=%u lvi=%u frag=%u "
	    "nfrags=%u bdl=0x%llx buf=0x%llx sdfmt=0x%04x\n",
	    stream->desc, stream->total_bytes, stream->nfrags - 1,
	    stream->frag_bytes, stream->nfrags,
	    (unsigned long long)stream->bdl_mem.phys,
	    (unsigned long long)stream->buf_mem.phys,
	    hda_format_to_sdfmt(&stream->format));
	hda_dump_desc(sc, base, "after-ctl");

	{
		u32	k;
		u64	sum;

		sum = 0;
		for (k = 0; k < 64 && k < stream->total_bytes; k++) {
			sum += ((const u8 *)stream->buf_mem.virt)[k];
		}
		drivers_log("[HDA] buffer check: virt=%p phys=0x%llx "
		    "bytes=%u sum(first64)=%llu\n", stream->buf_mem.virt,
		    (unsigned long long)stream->buf_mem.phys,
		    stream->total_bytes, (unsigned long long)sum);
	}
	return (0);
}

static void
hda_stream_teardown(hda_softc_t *sc, hda_stream_t *stream)
{
	if (sc == NULL || stream == NULL) {
		return;
	}
	hda_reset_stream(sc, stream);
	dma_mem_free(&stream->bdl_mem);
	dma_mem_free(&stream->buf_mem);
	memset(stream, 0, sizeof(*stream));
}

static int
hda_miniport_probe_format(const pc_miniport_t *mp, const ks_format_t *fmt)
{
	(void)mp;
	if (fmt == NULL) {
		return (PC_FAILURE_INVALID_FORMAT);
	}
	if (!ks_format_validate(fmt)) {
		return (PC_FAILURE_INVALID_FORMAT);
	}
	switch (fmt->container) {
	case KS_DATARANGE_PCM_S8:
	case KS_DATARANGE_PCM_U8:
	case KS_DATARANGE_PCM_S16LE:
	case KS_DATARANGE_PCM_S24LE:
	case KS_DATARANGE_PCM_S32LE:
		return (0);
	default:
		return (PC_FAILURE_INVALID_FORMAT);
	}
}

static int
hda_miniport_create_stream(const pc_miniport_t *mp,
    const ks_format_t *fmt, u32 flags, mp_stream_t **handle)
{
	hda_softc_t	*sc;
	hda_stream_t	*stream;
	u32		i;
	u32		framesz;

	if (mp == NULL || fmt == NULL || handle == NULL) {
		return (PC_FAILURE_NO_RESOURCE);
	}
	sc = (hda_softc_t *)mp->device_ctx;
	if (sc == NULL) {
		return (PC_FAILURE_NO_RESOURCE);
	}
	if (hda_miniport_probe_format(mp, fmt) != 0) {
		return (PC_FAILURE_INVALID_FORMAT);
	}
	framesz = ks_format_bytes_per_frame(fmt);
	if (framesz == 0) {
		return (PC_FAILURE_INVALID_FORMAT);
	}

	if (flags & PC_STREAM_CAPTURE) {
		stream = NULL;
		for (i = HDA_IN_DESC_LO; i <= HDA_IN_DESC_HI; i++) {
			stream = &sc->streams[i];
			if (stream->buf_mem.virt == NULL) {
				break;
			}
			stream = NULL;
		}
	} else {
		stream = NULL;
		for (i = HDA_OUT_DESC_LO; i <= HDA_OUT_DESC_HI; i++) {
			stream = &sc->streams[i];
			if (stream->buf_mem.virt == NULL) {
				break;
			}
			stream = NULL;
		}
	}
	if (stream == NULL) {
		return (PC_FAILURE_NO_RESOURCE);
	}
	memset(stream, 0, sizeof(*stream));
	stream->desc = i;
	stream->direction = (flags & PC_STREAM_CAPTURE) != 0;
	stream->format = *fmt;
	stream->volume = PC_VOLUME_UNITY;

	stream->frag_bytes = HDA_DEFAULT_BUFFER_BYTES / HDA_BDL_ENTRIES;
	stream->frag_bytes -= stream->frag_bytes % framesz;
	stream->nfrags = HDA_BDL_ENTRIES;
	stream->total_bytes = stream->frag_bytes * stream->nfrags;
	if (dma_mem_alloc(sc->tag, stream->total_bytes, 0,
	    &stream->buf_mem) != 0) {
		return (PC_FAILURE_NO_RESOURCE);
	}
	if (dma_mem_alloc(sc->tag,
	    HDA_BDL_ENTRIES * HDAC_BDL_ENTRY_SIZE, 0,
	    &stream->bdl_mem) != 0) {
		dma_mem_free(&stream->buf_mem);
		return (PC_FAILURE_NO_RESOURCE);
	}
	stream->bdl = (hda_bdl_entry_t *)stream->bdl_mem.virt;
	if (hda_stream_setup(sc, stream) != 0) {
		hda_stream_teardown(sc, stream);
		return (PC_FAILURE_NO_RESOURCE);
	}
	*handle = (mp_stream_t *)stream;
	sc->stream_count++;
	return (0);
}

static int
hda_miniport_destroy_stream(const pc_miniport_t *mp, mp_stream_t *handle)
{
	hda_softc_t	*sc;
	hda_stream_t	*stream;

	if (mp == NULL || handle == NULL) {
		return (PC_FAILURE_NOT_SUPPORTED);
	}
	sc = (hda_softc_t *)mp->device_ctx;
	stream = (hda_stream_t *)handle;
	if (sc == NULL || stream < &sc->streams[0] ||
	    stream >= &sc->streams[HDA_MAX_STREAM_INSTANCES]) {
		return (PC_FAILURE_NOT_SUPPORTED);
	}
	hda_stream_teardown(sc, stream);
	if (sc->stream_count != 0) {
		sc->stream_count--;
	}
	return (0);
}

static u32
hda_codec_format_from_ks(const ks_format_t *fmt)
{
	u32	val;

	if (fmt == NULL) {
		return (0);
	}
	val = HDAC_FMT_TYPE_PCM;
	switch (fmt->valid_bits) {
	case 8:
		val |= HDAC_FMT_BITS_8;
		break;
	case 20:
		val |= HDAC_FMT_BITS_20;
		break;
	case 24:
		val |= HDAC_FMT_BITS_24;
		break;
	case 32:
		val |= HDAC_FMT_BITS_32;
		break;
	case 16:
	default:
		val |= HDAC_FMT_BITS_16;
		break;
	}
	if (fmt->rate == 44100) {
		val |= HDAC_FMT_BASE_44K;
	}
	val |= ((fmt->channels - 1) & 0xF) << HDAC_FMT_CHAN_SHIFT;
	return (val);
}


static void
hda_codec_route_render(hda_softc_t *sc, u32 stream_id, const ks_format_t *fmt,
    hda_stream_t *stream)
{
	hda_codec_graph_t	*codec;
	hda_widget_t		*pin;
	hda_widget_t		*conv;
	u32			i;
	u32			j;
	u32			conv_idx;
	u32			resp;
	u32			conv_fmt;

	if (sc == NULL) {
		return;
	}
	codec = sc->codec;
	if (codec == NULL) {
		return;
	}
	for (i = 0; i < codec->pin_count; i++) {
		pin = hda_codec_widget(codec, codec->pin_ids[i]);
		if (pin == NULL || pin->type != HDA_WIDGET_PIN_COMPLEX ||
		    codec->pin_flows[i] != KS_DATAFLOW_OUT) {
			continue;
		}


		conv = NULL;
		conv_idx = 0;
		for (j = 0; j < pin->conn_count; j++) {
			hda_widget_t	*cand;

			cand = hda_codec_widget(codec, pin->conn_list[j]);
			if (cand == NULL) {
				continue;
			}
			if (cand->type == HDA_WIDGET_AUDIO_OUTPUT ||
			    cand->type == HDA_WIDGET_UNKNOWN) {
				conv = cand;
				conv_idx = j;
				break;
			}
		}
		if (conv == NULL) {
			continue;
		}

		if (pin->conn_count > 1) {
			(void)hda_codec_verb(codec->bus, codec->addr,
			    pin->nid,
			    (HDAC_VERB_SET_CONNSEL << 8) | conv_idx,
			    &resp);
		}


		(void)hda_codec_verb(codec->bus, codec->addr, conv->nid,
		    (HDAC_VERB_SET_STREAM << 8) |
		    (((stream_id & 0xF) << 4) | 0),
		    &resp);

		conv_fmt = hda_codec_format_from_ks(fmt);
		(void)hda_codec_verb(codec->bus, codec->addr, conv->nid,
		    (HDAC_VERB_SET_STREAM_FORMAT << 8) | conv_fmt,
		    &resp);
		drivers_log("[HDA] converter format stream %u conv=%u "
		    "payload=0x%04x\n", stream_id, conv->nid, conv_fmt);

		if (stream != NULL) {
			stream->amp_nid = conv->nid;
			stream->amp_num_steps = conv->amp_out.num_steps;
			stream->amp_step_size = conv->amp_out.step_size;
			stream->amp_offset = conv->amp_out.offset;
			stream->amp_mute_cap = conv->amp_out.mute_cap;
		}

		(void)hda_codec_verb(codec->bus, codec->addr,
		    conv->nid,
		    (HDAC_VERB_SET_AMP_GAIN_MUTE << 8) |
		    HDAC_AMP_SET_OUTPUT | HDAC_AMP_SET_LEFT |
		    HDAC_AMP_SET_RIGHT |
		    ((conv->amp_out.num_steps & 0x7F) << 8),
		    &resp);

		(void)hda_codec_verb(codec->bus, codec->addr, pin->nid,
		    (HDAC_VERB_SET_PIN_WIDGET_CTRL << 8) |
		    HDAC_PIN_CTRL_OUT_EN, &resp);

		if (pin->eapd) {
			(void)hda_codec_verb(codec->bus, codec->addr,
			    pin->nid,
			    (HDAC_VERB_SET_EAPD_BTLENABLE << 8) |
			    HDAC_EAPD_BTL_ENABLE, &resp);
		}

		drivers_log("[HDA] routed render stream %u pin=%u conv=%u\n",
		    stream_id, pin->nid, conv->nid);
		return;
	}
}

static int
hda_miniport_set_state(const pc_miniport_t *mp, mp_stream_t *handle,
    ks_state_t state)
{
	hda_softc_t	*sc;
	hda_stream_t	*stream;

	if (mp == NULL || handle == NULL) {
		return (PC_FAILURE_NOT_SUPPORTED);
	}
	sc = (hda_softc_t *)mp->device_ctx;
	stream = (hda_stream_t *)handle;
	if (sc == NULL || stream < &sc->streams[0] ||
	    stream >= &sc->streams[HDA_MAX_STREAM_INSTANCES]) {
		return (PC_FAILURE_NOT_SUPPORTED);
	}
	if (state == KS_STATE_RUN) {
		if (hda_stream_setup(sc, stream) != 0) {
			return (PC_FAILURE_NO_RESOURCE);
		}
		if (stream->direction == 0) {
			hda_codec_route_render(sc,
			    hda_stream_tag(sc, stream), &stream->format,
			    stream);
		}
		hda_run_stream(sc, stream, 1);
	} else if (state == KS_STATE_STOP ||
	    state == KS_STATE_PAUSE) {
		hda_run_stream(sc, stream, 0);
	}
	return (0);
}

static int
hda_miniport_position(const pc_miniport_t *mp, mp_stream_t *handle,
    ks_position_t *pos)
{
	hda_softc_t	*sc;
	hda_stream_t	*stream;
	u32		base;
	u32		lpib;

	if (mp == NULL || handle == NULL || pos == NULL) {
		return (PC_FAILURE_NOT_SUPPORTED);
	}
	sc = (hda_softc_t *)mp->device_ctx;
	stream = (hda_stream_t *)handle;
	if (sc == NULL || stream < &sc->streams[0] ||
	    stream >= &sc->streams[HDA_MAX_STREAM_INSTANCES]) {
		return (PC_FAILURE_NOT_SUPPORTED);
	}
	base = HDAC_SD_BASE + stream->desc * HDAC_SD_STRIDE;
	lpib = hda_reg_read32(sc, base + HDAC_SD_LPIB);
	stream->position = lpib;

	pos->play_offset = lpib % stream->total_bytes;
	pos->write_offset = lpib % stream->total_bytes;
	return (0);
}

static int
hda_miniport_get_buffer(const pc_miniport_t *mp, mp_stream_t *handle,
    u8 **virt, u64 *len)
{
	hda_softc_t	*sc;
	hda_stream_t	*stream;

	if (mp == NULL || handle == NULL || virt == NULL || len == NULL) {
		return (PC_FAILURE_NOT_SUPPORTED);
	}
	sc = (hda_softc_t *)mp->device_ctx;
	stream = (hda_stream_t *)handle;
	if (sc == NULL || stream < &sc->streams[0] ||
	    stream >= &sc->streams[HDA_MAX_STREAM_INSTANCES]) {
		return (PC_FAILURE_NOT_SUPPORTED);
	}
	*virt = (u8 *)stream->buf_mem.virt;
	*len = (u64)stream->total_bytes;
	return (0);
}

static int
hda_miniport_map_buffer(const pc_miniport_t *mp, mp_stream_t *handle,
    dma_seg_t *segs, u32 maxsegs, u32 *nsegs)
{
	hda_softc_t	*sc;
	hda_stream_t	*stream;

	(void)maxsegs;
	if (mp == NULL || handle == NULL || segs == NULL || nsegs == NULL) {
		return (PC_FAILURE_NOT_SUPPORTED);
	}
	sc = (hda_softc_t *)mp->device_ctx;
	stream = (hda_stream_t *)handle;
	if (sc == NULL || stream < &sc->streams[0] ||
	    stream >= &sc->streams[HDA_MAX_STREAM_INSTANCES]) {
		return (PC_FAILURE_NOT_SUPPORTED);
	}
	segs[0].phys = stream->buf_mem.phys;
	segs[0].len = stream->total_bytes;
	*nsegs = 1;
	return (0);
}

static u32
hda_amp_step_for_volume(const hda_stream_t *stream, u32 volume)
{
	u32	n;
	u32	step;

	if (stream->amp_num_steps == 0) {
		return (0);
	}
	n = stream->amp_num_steps;
	if (n > 0x7F) {
		n = 0x7F;
	}
	if (volume == 0) {
		step = 0;
	} else {
		u32	idx;

		idx = (u32)(((u64)volume * (u64)(n - stream->amp_offset))
		    / 0x8000U);
		step = stream->amp_offset + idx;
		if (step > n) {
			step = n;
		}
	}
	return (step);
}

static int
hda_miniport_set_volume(const pc_miniport_t *mp, mp_stream_t *handle,
    u32 volume)
{
	hda_softc_t	*sc;
	hda_stream_t	*stream;
	u32		step;
	u32		payload;
	u32		resp;

	if (mp == NULL || handle == NULL) {
		return (PC_FAILURE_NOT_SUPPORTED);
	}
	sc = (hda_softc_t *)mp->device_ctx;
	stream = (hda_stream_t *)handle;
	if (sc == NULL || stream < &sc->streams[0] ||
	    stream >= &sc->streams[HDA_MAX_STREAM_INSTANCES]) {
		return (PC_FAILURE_NOT_SUPPORTED);
	}
	if (volume > PC_VOLUME_MAX) {
		volume = PC_VOLUME_MAX;
	}
	stream->volume = volume;

	if (stream->amp_nid == 0 || stream->amp_num_steps == 0 ||
	    sc->codec == NULL) {
		return (PC_FAILURE_NOT_SUPPORTED);
	}

	step = hda_amp_step_for_volume(stream, volume);
	payload = HDAC_AMP_SET_OUTPUT | HDAC_AMP_SET_LEFT |
	    HDAC_AMP_SET_RIGHT | ((step & 0x7F) << 8);
	if (volume == 0 && stream->amp_mute_cap) {
		payload |= HDA_AMP_SET_MUTE;
	}
	if (hda_codec_verb(sc->codec->bus, sc->codec->addr,
	    stream->amp_nid,
	    (HDAC_VERB_SET_AMP_GAIN_MUTE << 8) | payload, &resp) != 0) {
		return (PC_FAILURE_NO_RESOURCE);
	}
	return (0);
}

static u32
hda_miniport_get_volume(const pc_miniport_t *mp, mp_stream_t *handle)
{
	hda_softc_t	*sc;
	hda_stream_t	*stream;

	if (mp == NULL || handle == NULL) {
		return (PC_VOLUME_UNITY);
	}
	sc = (hda_softc_t *)mp->device_ctx;
	stream = (hda_stream_t *)handle;
	if (sc == NULL || stream < &sc->streams[0] ||
	    stream >= &sc->streams[HDA_MAX_STREAM_INSTANCES]) {
		return (PC_VOLUME_UNITY);
	}
	return (stream->volume);
}

const pc_miniport_t *
hda_miniport(void)
{
	static const pc_miniport_t	miniport = {
		.probe_format = hda_miniport_probe_format,
		.create_stream = hda_miniport_create_stream,
		.destroy_stream = hda_miniport_destroy_stream,
		.set_state = hda_miniport_set_state,
		.position = hda_miniport_position,
		.map_buffer = hda_miniport_map_buffer,
		.get_buffer = hda_miniport_get_buffer,
		.set_volume = hda_miniport_set_volume,
		.get_volume = hda_miniport_get_volume,
		.capabilities = PC_CAP_HW_AMP | PC_CAP_SOFTVOL,
		.device_ctx = NULL,
	};
	return (&miniport);
}

static int
hda_map_bar(hda_softc_t *sc)
{
	int	rid;

	if (sc == NULL || sc->dev == NULL) {
		return (-1);
	}
	rid = 0;
	sc->mem_res = bus_alloc_resource(sc->dev, SYS_RES_MEMORY, &rid,
	    0, 0, RF_ACTIVE);
	if (sc->mem_res == NULL) {
		drivers_log("[HDA] failed to map BAR0\n");
		return (-1);
	}
	sc->mmio = (void *)(sc->mem_res->start + DMAP_BASE);
	return (0);
}


static void
hda_poll(void *arg)
{
	hda_softc_t	*sc;
	hda_stream_t	*stream;
	u32		lpib;
	u32		base;
	u32		i;

	sc = (hda_softc_t *)arg;
	if (sc == NULL || sc->port == NULL) {
		return;
	}

	for (i = 0; i < HDA_MAX_STREAM_INSTANCES; i++) {
		stream = &sc->streams[i];
		if (stream->buf_mem.virt == NULL) {
			continue;
		}
		base = HDAC_SD_BASE + stream->desc * HDAC_SD_STRIDE;
		lpib = hda_reg_read32(sc, base + HDAC_SD_LPIB);
		if (sc->poll_probe_count < 4 ||
		    (sc->poll_probe_count % 500) == 0) {
			u32	sts;
			u32	lpib_now;
			u32	ctl_now;

			ctl_now = hda_reg_read32(sc, base + HDAC_SD_CTL);
			sts = hda_reg_read8(sc, base + HDAC_SD_STS);
			lpib_now = hda_reg_read32(sc, base + HDAC_SD_LPIB);
			drivers_log("[HDA] poll#%u stream %u LPIB=%u(%u) "
			    "ctl=0x%08x sts=0x%02x cbl=%u(%u) lvi=%u "
			    "fmt=0x%04x(0x%04x) bdpl=0x%08x(0x%08x) "
			    "gcap=0x%08x gctl=0x%08x intsts=0x%08x "
			    "corbwp=0x%04x rirbwp=0x%04x\n",
			    sc->poll_probe_count, stream->desc, lpib,
			    lpib_now, ctl_now, sts,
			    hda_reg_read32(sc, base + HDAC_SD_CBL),
			    stream->total_bytes,
			    hda_reg_read16(sc, base + HDAC_SD_LVI),
			    hda_reg_read16(sc, base + HDAC_SD_FMT),
			    hda_format_to_sdfmt(&stream->format),
			    hda_reg_read32(sc, base + HDAC_SD_BDPL),
			    (u32)stream->bdl_mem.phys,
			    hda_reg_read32(sc, HDAC_GCAP),
			    hda_reg_read32(sc, HDAC_GCTL),
			    hda_reg_read32(sc, HDAC_INTSTS),
			    hda_reg_read16(sc, HDAC_CORBWP),
			    hda_reg_read16(sc, HDAC_RIRBWP));
		}
		stream->position = lpib;
	}
	sc->poll_probe_count++;
	pc_port_process(sc->port);
}

static int
hda_intr(void *arg)
{
	hda_softc_t	*sc;
	u32		intsts;
	u32		i;
	u32		base;

	sc = (hda_softc_t *)arg;
	if (sc == NULL) {
		return (-1);
	}
	intsts = hda_reg_read32(sc, HDAC_INTSTS);
	if ((intsts & HDAC_INTSTS_GIS) == 0) {
		return (-1);
	}

	for (i = 0; i < HDA_MAX_STREAM_INSTANCES; i++) {
		if ((intsts & (1u << i)) == 0) {
			continue;
		}
		base = HDAC_SD_BASE + i * HDAC_SD_STRIDE;
		hda_reg_write8(sc, base + HDAC_SD_STS,
		    hda_reg_read8(sc, base + HDAC_SD_STS));
	}
	hda_reg_write32(sc, HDAC_INTSTS,
	    intsts & HDAC_INTSTS_SIS_MASK);

	return (0);
}

static pci_match_t	hda_matches[] = {
	{ PCI_ANY_ID, PCI_ANY_ID, HDA_PCI_CLASS, HDA_PCI_SUBCLASS,
	    HDA_PCI_PROGIF },
};

static int
hda_pci_probe(pci_device_t *pdev, const pci_match_t *match)
{
	hda_softc_t	*sc;
	device_t	dev;
	dma_tag_t	tag;
	int		rid;

	(void)match;
	if (pdev == NULL || pdev->nb_device == NULL) {
		return (1);
	}
	dev = pdev->nb_device;

	sc = (hda_softc_t *)device_get_softc(dev);
	if (sc != NULL) {
		return (0);
	}
	sc = (hda_softc_t *)kmem_calloc(1, sizeof(*sc));
	if (sc == NULL) {
		return (1);
	}
	sc->dev = dev;
	sc->pci = pdev;
	sc->unit = (u32)device_get_unit(dev);

	tag = bus_get_dma_tag(dev);
	if (tag == NULL) {
		kmem_free(sc);
		return (1);
	}
	if (dma_tag_create(tag, 128, DMA_BOUNDARY_NONE, 0,
	    DMA_HIGHADDR_ANY, 0x40000, HDA_BDL_ENTRIES, DMA_SEGSZ_MAX, 0,
	    "hda", &sc->tag) != 0) {
		kmem_free(sc);
		return (1);
	}
	tag = sc->tag;

	if (hda_map_bar(sc) != 0) {
		dma_tag_destroy(sc->tag);
		kmem_free(sc);
		return (1);
	}

	if (dma_mem_alloc(tag, HDA_BUS_MAX_CORB * 4, 0,
	    &sc->corb_mem) != 0) {
		goto fail;
	}
	if (dma_mem_alloc(tag, HDA_BUS_MAX_RIRB * 8, 0,
	    &sc->rirb_mem) != 0) {
		dma_mem_free(&sc->corb_mem);
		goto fail;
	}
	sc->bus.corb = sc->corb_mem.virt;
	sc->bus.corb_phys = sc->corb_mem.phys;
	sc->bus.rirb = (volatile u32 *)sc->rirb_mem.virt;
	sc->bus.rirb_phys = sc->rirb_mem.phys;

	if (hda_bus_init(&sc->bus, sc->mmio, (u32)sc->mem_res->count) != 0) {
		dma_mem_free(&sc->rirb_mem);
		dma_mem_free(&sc->corb_mem);
		goto fail;
	}

	rid = 0;
	sc->irq_res = bus_alloc_resource_any(sc->dev, SYS_RES_IRQ, &rid,
	    RF_ACTIVE | RF_SHAREABLE);
	if (sc->irq_res != NULL) {
		if (bus_setup_intr(sc->dev, sc->irq_res, hda_intr, sc,
		    &sc->intr_cookie) != 0) {
			sc->intr_cookie = NULL;
		}
	}

	sc->miniport = *hda_miniport();
	sc->miniport.device_ctx = sc;

	bus_setup_poll(sc->dev, NB_POLL_TIMER, hda_poll, sc,
	    &sc->poll_cookie);

	{
		hda_codec_graph_t	*codec;
		ks_device_t		*ksdev;
		pc_port_t		*port;
		u32			codec_idx;
		u16			mask;

		mask = hda_bus_scan_codecs(&sc->bus);
		sc->codec_count = 0;
		drivers_log("[HDA] scan_codecs returned mask=0x%04x "
		    "(codec_count_states=%u)\n", mask, __builtin_popcount(mask));
		for (codec_idx = 0; codec_idx < HDA_BUS_MAX_CODECS;
		    codec_idx++) {
			if ((mask & (1u << codec_idx)) == 0) {
				continue;
			}
			if (hda_codec_discover(&sc->bus, (u8)codec_idx,
			    &codec) != 0) {
				drivers_log("[HDA] codec %u discover failed\n",
				    codec_idx);
				continue;
			}
			drivers_log("[HDA] codec %u discovered: "
			    "vendor=0x%08x nodes=%u start=%u pins=%u\n",
			    codec_idx, codec->vendor_id, codec->node_count,
			    codec->node_start, codec->pin_count);
			ksdev = NULL;
			if (hda_codec_publish(codec, &ksdev) != 0) {
				drivers_log("[HDA] codec %u publish failed\n",
				    codec_idx);
				hda_codec_release(codec);
				continue;
			}
			drivers_log("[HDA] codec %u published: name=%s "
			    "pins=%u\n", codec_idx,
			    ksdev->obj.name, ksdev->pin_count);
			port = pc_port_create(&sc->miniport,
			    device_get_nameunit(dev));
			if (port == NULL) {
				drivers_log("[HDA] port create failed\n");
				ks_device_destroy(ksdev);
				hda_codec_release(codec);
				continue;
			}
			sc->codec = codec;
			sc->port = port;
			sc->ksdev = ksdev;
			sc->codec_count++;
			sc->quirks = hda_lookup_quirks(codec->vendor_id);
			drivers_log("[HDA] codec vendor=0x%08x "
			    "quirks=0x%x\n", codec->vendor_id, sc->quirks);
			drivers_log("[HDA] publishing device via sysaudio\n");
			sysaudio_publish_device(ksdev, port);
		}
	}

	device_set_softc(dev, sc);
	drivers_log("[HDA] controller initialized unit %u\n", sc->unit);
	return (0);

fail:
	if (sc->mmio != NULL) {
		bus_release_resource(sc->dev, SYS_RES_MEMORY, 0,
		    sc->mem_res);
	}
	dma_tag_destroy(sc->tag);
	kmem_free(sc);
	return (1);
}

static pci_driver_t hda_pci_driver = {
	.name = "hda",
	.matches = hda_matches,
	.match_count = 1,
	.probe = hda_pci_probe,
	.remove = NULL,
};

static devclass_t hda_devclass = {
	.name = "hda",
	.maxunit = 4,
};

PCI_DRIVER_MODULE(hda, hda_pci_driver, hda_devclass,
    NEWBUS_PASS_AUDIO, NEWBUS_ORDER_MIDDLE);