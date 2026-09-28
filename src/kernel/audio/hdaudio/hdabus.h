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

$define %type hda_bus_t as one Intel HDA controller bus instance
$define %type hda_codec_t as one codec discovered on the bus
$define %type hda_command_t as one verb command assembled for the CORB
$define %type hda_response_t as one decoded RIRB response

$const HDA_BUS_MAX_CODECS as codecs one controller may address
$const HDA_BUS_MAX_CORB as maximum CORB ring entries supported by core
$const HDA_BUS_MAX_RIRB as maximum RIRB ring entries supported by core
$const HDA_CMD_TIMEOUT_US as budget for a CORB command completion
$const HDA_CODEC_ADDR_SHIFT as codec address field shift in a verb

$define %func hda_bus_init as function with args hda_bus_t *, void *, u32
$define %func hda_bus_reset as function with args hda_bus_t *
$define %func hda_bus_command as function with args hda_bus_t *, u32, u32 *
$define %func hda_bus_codec_attach as function with args hda_bus_t *, u8
$define %func hda_bus_scan_codecs as function with args hda_bus_t *
*/

/* !SPACE!

$space %export hda_bus_init, hda_bus_reset, hda_bus_command
$space %export hda_bus_codec_attach, hda_bus_scan_codecs
$space %export hda_bus_reg_read32, hda_bus_reg_write32

*/

#ifndef KERNEL_AUDIO_HDAUDIO_HDABUS_H
#define KERNEL_AUDIO_HDAUDIO_HDABUS_H

#include <mlibc/mlibc.h>
#include <kernel/drivers/audio/intel-hda/hdac_reg.h>

#define	HDA_BUS_MAX_CODECS	15
#define	HDA_BUS_MAX_CORB		256
#define	HDA_BUS_MAX_RIRB		256
#define	HDA_CMD_TIMEOUT_US		1000000ULL
#define	HDA_CODEC_ADDR_SHIFT		28

typedef struct hda_codec {
	u32	addr;
	u32	vendor_id;
	u32	revision_id;
	u32	subsystem_id;
	u32	node_count;
	u32	group_count;
	u8	present;
} hda_codec_t;

typedef struct hda_bus {
	volatile u8	*regs;
	void		*corb;
	volatile u32	*rirb;
	u64		corb_phys;
	u64		rirb_phys;
	u32		corb_entries;
	u32		rirb_entries;
	u32		corb_wp;
	u32		corb_rp;
	u32		corb_last_wp;
	u32		rirb_rp;
	u32		rirb_last_wp;
	u32		corb_size_code;
	u32		rirb_size_code;
	u16		codec_mask;
	u32		gcap;
	hda_codec_t	codecs[HDA_BUS_MAX_CODECS];
} hda_bus_t;

u32		hda_bus_reg_read32(hda_bus_t *bus, u32 off);
void		hda_bus_reg_write32(hda_bus_t *bus, u32 off, u32 val);
u16		hda_bus_reg_read16(hda_bus_t *bus, u32 off);
void		hda_bus_reg_write16(hda_bus_t *bus, u32 off, u16 val);
u8		hda_bus_reg_read8(hda_bus_t *bus, u32 off);
void		hda_bus_reg_write8(hda_bus_t *bus, u32 off, u8 val);
int		hda_bus_init(hda_bus_t *bus, void *mmio, u32 bar_size);
int		hda_bus_reset(hda_bus_t *bus);
int		hda_bus_command(hda_bus_t *bus, u32 verb, u32 *response);
u16		hda_bus_scan_codecs(hda_bus_t *bus);
int		hda_bus_codec_attach(hda_bus_t *bus, u8 addr);

#endif
