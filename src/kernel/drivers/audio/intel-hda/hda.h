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
$define %type pc_miniport_t as hardware ops the port-class drives
$define %type ks_format_t as canonical sample format descriptor
$define %type dma_tag_t as device DMA constraint set
$define %type dma_mem_t as coherent device-visible allocation

$const HDA_MAX_STREAM_INSTANCES as simultaneus DMA streams one controller runs
$const HDA_FIFO_SIZE as minimum FIFO size in dwords programmed to SDn.FIFOS
$const HDA_DEFAULT_BUFFER_BYTES as default transfer buffer per stream
$const HDA_BDL_ENTRIES as BDL descriptors per stream ring
$const HDA_PCI_CLASS as PCI Multimedia class
$const HDA_PCI_SUBCLASS as PCI Audio device subclass
$const HDA_PCI_PROGIF as HD Audio programming interface

$define %func hda_intr as function with args void *
$define %func hda_miniport as function with args void

*/

/* !SPACE!

$space %internal hda_stream_setup, hda_stream_teardown
$space %internal hda_reset_stream, hda_run_stream
$space %export hda_miniport

*/

#ifndef KERNEL_DRIVERS_AUDIO_INTEL_HDA_HDA_H
#define KERNEL_DRIVERS_AUDIO_INTEL_HDA_HDA_H

#include <mlibc/mlibc.h>
#include <kernel/audio/hdaudio/hdabus.h>
#include <kernel/audio/portcls/pcminiport.h>
#include <kernel/drivers/newbus/newbus.h>
#include <kernel/pci/pci.h>
#include <kernel/mm/dma/dma.h>

#define	HDA_MAX_STREAM_INSTANCES	(HDAC_MAX_OUTSTREAMS + \
					    HDAC_MAX_INSTREAMS)
#define	HDA_FIFO_SIZE			4
#define	HDA_IN_DESC_LO			0
#define	HDA_IN_DESC_HI			3
#define	HDA_OUT_DESC_LO			4
#define	HDA_OUT_DESC_HI			7
#define	HDA_DEFAULT_BUFFER_BYTES	65536
#define	HDA_BDL_ENTRIES			16
#define	HDA_RESET_POLLS			2000
#define	HDA_RESET_POLL_US		1
#define	HDA_PCI_CLASS			0x04
#define	HDA_PCI_SUBCLASS		0x03
#define	HDA_PCI_PROGIF			0x00

typedef struct hda_stream {
	u32		desc;
	u32		direction;
	ks_format_t	format;
	dma_mem_t	buf_mem;
	dma_mem_t	bdl_mem;
	hda_bdl_entry_t	*bdl;
	u32		frag_bytes;
	u32		nfrags;
	u32		total_bytes;
	u32		active;
	u64		position;
} hda_stream_t;

struct hda_codec_graph;
struct pc_port;
struct ks_device;
struct sysaudio_device;

typedef struct hda_softc {
	device_t	dev;
	pci_device_t	*pci;
	dma_tag_t	tag;
	resource_t	*mem_res;
	void		*mmio;
	hda_bus_t	bus;
	dma_mem_t	corb_mem;
	dma_mem_t	rirb_mem;
	hda_stream_t	streams[HDA_MAX_STREAM_INSTANCES];
	u32		stream_count;
	u32		unit;
	void		*intr_cookie;
	resource_t	*irq_res;
	void		*poll_cookie;
	u32		poll_probe_count;
	pc_miniport_t	miniport;
	struct hda_codec_graph *codec;
	struct pc_port	*port;
	struct ks_device *ksdev;
	u32		codec_count;
} hda_softc_t;

const pc_miniport_t	*hda_miniport(void);

u32		hda_reg_read32(hda_softc_t *sc, u32 off);
u16		hda_reg_read16(hda_softc_t *sc, u32 off);
u8		hda_reg_read8(hda_softc_t *sc, u32 off);
void		hda_reg_write32(hda_softc_t *sc, u32 off, u32 val);
void		hda_reg_write16(hda_softc_t *sc, u32 off, u16 val);
void		hda_reg_write8(hda_softc_t *sc, u32 off, u8 val);

#endif
