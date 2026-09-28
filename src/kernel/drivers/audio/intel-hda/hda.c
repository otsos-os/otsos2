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
$define %func hda_stream_setup as function with args hda_softc_t *, hda_stream_t *
$define %func hda_stream_teardown as procedure with args hda_softc_t *, hda_stream_t *
$define %func hda_reset_stream as procedure with args hda_softc_t *, hda_stream_t *
$define %func hda_run_stream as procedure with args hda_softc_t *, hda_stream_t *, int
*/

/* !SPACE!

$space %internal hda_stream_setup, hda_stream_teardown
$space %internal hda_reset_stream, hda_run_stream, hda_map_bar
$space %internal hda_poll
$space %internal hda_miniport_probe_format, hda_miniport_create_stream
$space %internal hda_miniport_destroy_stream, hda_miniport_set_state
$space %internal hda_miniport_position
$space %export hda_attach, hda_detach, hda_miniport
$space %export hda_reg_read32, hda_reg_write32

*/

#include <kernel/drivers/audio/intel-hda/hda.h>
#include <kernel/audio/hdaudio/hdacodec.h>
#include <kernel/audio/portcls/pcport.h>
#include <kernel/audio/api/api_audio.h>
#include <kernel/audio/sysaudio/sysaudio.h>
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


static u32
hda_format_to_sdfmt(const ks_format_t *fmt)
{
	u32	val;

	val = HDAC_SDFMT_BASE_44K | HDAC_SDFMT_MULT_NO | HDAC_SDFMT_DIV_NO;
	switch (fmt->rate) {
	case 44100:
		val = HDAC_SDFMT_BASE_44K | HDAC_SDFMT_MULT_NO |
		    HDAC_SDFMT_DIV_NO;
		break;
	case 48000:
		val = HDAC_SDFMT_MULT_NO | HDAC_SDFMT_DIV_NO;
		break;
	case 96000:
		val = HDAC_SDFMT_MULT_2 | HDAC_SDFMT_DIV_NO;
		break;
	case 192000:
		val = HDAC_SDFMT_MULT_4 | HDAC_SDFMT_DIV_NO;
		break;
	default:
		break;
	}
	switch (fmt->container) {
	case KS_DATARANGE_PCM_S8:
	case KS_DATARANGE_PCM_U8:
		val |= HDAC_SDFMT_BITS_8;
		break;
	case KS_DATARANGE_PCM_S16LE:
		val |= HDAC_SDFMT_BITS_16;
		break;
	case KS_DATARANGE_PCM_S24LE:
		val |= HDAC_SDFMT_BITS_24;
		break;
	case KS_DATARANGE_PCM_S32LE:
		val |= HDAC_SDFMT_BITS_32;
		break;
	default:
		break;
	}
	val |= ((fmt->channels - 1) & 0xF) << HDAC_SDFMT_CHAN_SHIFT;
	return (val);
}


static void
hda_reset_stream(hda_softc_t *sc, hda_stream_t *stream)
{
	u32	base;
	u32	ctl;
	u32	i;

	base = HDAC_SD_BASE + stream->desc * HDAC_SD_STRIDE;
	ctl = hda_reg_read32(sc, base + HDAC_SD_CTL);
	ctl &= ~HDAC_SDCTL_RUN;
	ctl |= HDAC_SDCTL_SRST;
	hda_reg_write32(sc, base + HDAC_SD_CTL, ctl);
	for (i = 0; i < 1000; i++) {
		if ((hda_reg_read32(sc, base + HDAC_SD_CTL) &
		    HDAC_SDCTL_SRST) == 0) {
			break;
		}
		__asm__ volatile("pause");
	}
	ctl = hda_reg_read32(sc, base + HDAC_SD_CTL);
	ctl &= ~HDAC_SDCTL_SRST;
	hda_reg_write32(sc, base + HDAC_SD_CTL, ctl);
	stream->active = 0;
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
}


static int
hda_stream_setup(hda_softc_t *sc, hda_stream_t *stream)
{
	u32	base;
	u32	i;
	u32	byte_off;
	u64	phys;

	if (sc == NULL || stream == NULL) {
		return (-1);
	}
	base = HDAC_SD_BASE + stream->desc * HDAC_SD_STRIDE;
	hda_reset_stream(sc, stream);

	hda_reg_write16(sc, base + HDAC_SD_FMT,
	    hda_format_to_sdfmt(&stream->format));
	hda_reg_write32(sc, base + HDAC_SD_CBL, stream->total_bytes);
	hda_reg_write16(sc, base + HDAC_SD_LVI,
	    stream->nfrags - 1);
	hda_reg_write16(sc, base + HDAC_SD_FIFOS, HDA_FIFO_SIZE);
	hda_reg_write16(sc, base + HDAC_SD_FIFOW, HDA_FIFO_SIZE);

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
	hda_reg_write32(sc, base + HDAC_SD_BDPU_BASE,
	    (u32)(stream->bdl_mem.phys >> 32));

	if (stream->direction != 0) {
		hda_reg_write32(sc, base + HDAC_SD_CTL,
		    hda_reg_read32(sc, base + HDAC_SD_CTL) |
		    HDAC_SDCTL_DIR);
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
	for (i = 0; i < HDA_MAX_STREAM_INSTANCES; i++) {
		stream = &sc->streams[i];
		if (stream->buf_mem.virt != NULL) {
			continue;
		}
		break;
	}
	if (i >= HDA_MAX_STREAM_INSTANCES) {
		return (PC_FAILURE_NO_RESOURCE);
	}
	memset(stream, 0, sizeof(*stream));
	stream->desc = i;
	stream->direction = (flags & PC_STREAM_CAPTURE) != 0;
	stream->format = *fmt;

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
	pos->play_offset = lpib;
	pos->write_offset = (lpib + stream->frag_bytes) %
	    stream->total_bytes;
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

	sc = (hda_softc_t *)arg;
	if (sc == NULL || sc->port == NULL) {
		return;
	}
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
		return (0);
	}
	intsts = hda_reg_read32(sc, HDAC_INTSTS);
	if ((intsts & HDAC_INTSTS_GIS) == 0) {
		return (0);
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
	return (1);
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

	rid = PCI_MSI_RID;
	sc->irq_res = bus_alloc_resource(sc->dev, SYS_RES_IRQ, &rid,
	    0, 1, RF_ACTIVE | RF_SHAREABLE);
	if (sc->irq_res != NULL) {
		if (bus_setup_intr(sc->dev, sc->irq_res, hda_intr, sc,
		    &sc->intr_cookie) != 0) {
			sc->intr_cookie = NULL;
		}
	}
	if (sc->intr_cookie == NULL) {
		rid = 0;
		sc->irq_res = bus_alloc_resource(sc->dev, SYS_RES_IRQ,
		    &rid, 0, 1, RF_ACTIVE);
		if (sc->irq_res != NULL) {
			bus_setup_intr(sc->dev, sc->irq_res, hda_intr, sc,
			    &sc->intr_cookie);
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