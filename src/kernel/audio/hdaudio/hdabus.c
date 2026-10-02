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
$define %type u32 as 32 bit unsigned
$define %type u16 as 16 bit unsigned
$define %type u8 as 8 bit unsigned
$define %type u64 as 64 bit unsigned

$define %func hda_bus_reg_read32 as function with args hda_bus_t *, u32
$define %func hda_bus_reg_write32 as procedure with args hda_bus_t *, u32, u32
$define %func hda_bus_init as function with args hda_bus_t *, void *, u32
$define %func hda_bus_reset as function with args hda_bus_t *
$define %func hda_bus_command as function with args hda_bus_t *, u32, u32 *
$define %func hda_bus_scan_codecs as function with args hda_bus_t *
$define %func hda_bus_codec_attach as function with args hda_bus_t *, u8
$define %func hda_bus_wait_us as procedure with args u64
*/

/* !SPACE!

$space %internal hda_bus_wait_us, hda_bus_corb_start, hda_bus_rirb_start
$space %internal verb_is_silent
$space %export hda_bus_reg_read32, hda_bus_reg_write32
$space %export hda_bus_init, hda_bus_reset, hda_bus_command
$space %export hda_bus_scan_codecs, hda_bus_codec_attach

*/

#include <kernel/audio/hdaudio/hdabus.h>


u32
hda_bus_reg_read32(hda_bus_t *bus, u32 off)
{
	if (bus == NULL || bus->regs == NULL) {
		return (0);
	}
	return (*(volatile u32 *)(bus->regs + off));
}

void
hda_bus_reg_write32(hda_bus_t *bus, u32 off, u32 val)
{
	if (bus != NULL && bus->regs != NULL) {
		*(volatile u32 *)(bus->regs + off) = val;
	}
}

u16
hda_bus_reg_read16(hda_bus_t *bus, u32 off)
{
	if (bus == NULL || bus->regs == NULL) {
		return (0);
	}
	return (*(volatile u16 *)(bus->regs + off));
}

void
hda_bus_reg_write16(hda_bus_t *bus, u32 off, u16 val)
{
	if (bus != NULL && bus->regs != NULL) {
		*(volatile u16 *)(bus->regs + off) = val;
	}
}

u8
hda_bus_reg_read8(hda_bus_t *bus, u32 off)
{
	if (bus == NULL || bus->regs == NULL) {
		return (0);
	}
	return (*(volatile u8 *)(bus->regs + off));
}

void
hda_bus_reg_write8(hda_bus_t *bus, u32 off, u8 val)
{
	if (bus != NULL && bus->regs != NULL) {
		*(volatile u8 *)(bus->regs + off) = val;
	}
}

static void
hda_bus_wait_us(u64 us)
{
	volatile u64	i;

	for (i = 0; i < us; i++) {
		__asm__ volatile("pause");
	}
}

static int
hda_bus_corb_start(hda_bus_t *bus)
{
	u32	i;

	hda_bus_reg_write8(bus, HDAC_CORBCTL, 0);
	hda_bus_reg_write32(bus, HDAC_CORBLBASE, (u32)bus->corb_phys);
	hda_bus_reg_write32(bus, HDAC_CORBUBASE,
	    (u32)(bus->corb_phys >> 32));
	hda_bus_reg_write8(bus, HDAC_CORBSIZE, bus->corb_size_code);
	hda_bus_reg_write16(bus, HDAC_CORBWP, 0);
	hda_bus_reg_write16(bus, HDAC_CORBRP, 0);
	bus->corb_wp = 0;
	bus->corb_rp = 0;
	bus->corb_last_wp = 0;
	hda_bus_reg_write8(bus, HDAC_CORBSTS,
	    HDAC_CORBSTS_CMEI);
	hda_bus_reg_write8(bus, HDAC_CORBCTL, HDAC_CORBCTL_RUN);
	for (i = 0; i < 1000; i++) {
		if ((hda_bus_reg_read8(bus, HDAC_CORBCTL) &
		    HDAC_CORBCTL_RUN) != 0) {
			return (0);
		}
		hda_bus_wait_us(10);
	}
	return (-1);
}

int
hda_bus_rirb_start(hda_bus_t *bus)
{
	u32	i;

	hda_bus_reg_write8(bus, HDAC_RIRBCTL, 0);
	hda_bus_reg_write32(bus, HDAC_RIRBLBASE, (u32)bus->rirb_phys);
	hda_bus_reg_write32(bus, HDAC_RIRBUBASE,
	    (u32)(bus->rirb_phys >> 32));
	hda_bus_reg_write8(bus, HDAC_RIRBSIZE, bus->rirb_size_code);
	hda_bus_reg_write16(bus, HDAC_RIRBWP, 0);
	bus->rirb_rp = 0;
	bus->rirb_last_wp = 0;
	hda_bus_reg_write16(bus, HDAC_RINTCNT, 0xFF);
	hda_bus_reg_write8(bus, HDAC_RIRBSTS, HDAC_RIRBSTS_MASK);
	hda_bus_reg_write8(bus, HDAC_RIRBCTL,
	    HDAC_RIRBCTL_RIRBDMAEN);
	for (i = 0; i < 1000; i++) {
		if ((hda_bus_reg_read8(bus, HDAC_RIRBCTL) &
		    HDAC_RIRBCTL_RIRBDMAEN) != 0) {
			return (0);
		}
		hda_bus_wait_us(10);
	}
	return (-1);
}

static int
hda_bus_rirb_init(hda_bus_t *bus)
{
	if (bus->rirb != NULL) {
		memset((void *)bus->rirb, 0,
		    (size_t)bus->rirb_entries * 8);
	}
	return (hda_bus_rirb_start(bus));
}

int
hda_bus_init(hda_bus_t *bus, void *mmio, u32 bar_size)
{
	void		*corb;
	volatile u32	*rirb;
	u64		corb_phys;
	u64		rirb_phys;
	u32		gcap;

	if (bus == NULL || mmio == NULL) {
		return (-1);
	}

	corb = bus->corb;
	rirb = bus->rirb;
	corb_phys = bus->corb_phys;
	rirb_phys = bus->rirb_phys;
	memset(bus, 0, sizeof(*bus));
	bus->corb = corb;
	bus->rirb = rirb;
	bus->corb_phys = corb_phys;
	bus->rirb_phys = rirb_phys;
	bus->regs = (volatile u8 *)mmio;
	(void)bar_size;

	if (hda_bus_reset(bus) != 0) {
		return (-1);
	}
	gcap = hda_bus_reg_read32(bus, HDAC_GCAP);
	bus->gcap = gcap;
	drivers_log("[HDA] GCAP=0x%08x\n", gcap);
	bus->corb_size_code = HDAC_CORBSIZE_ENTRIES_256;
	bus->corb_entries = 256;
	bus->rirb_size_code = HDAC_RIRBSIZE_ENTRIES_256;
	bus->rirb_entries = 256;
	if (hda_bus_corb_start(bus) != 0) {
		return (-1);
	}
	if (hda_bus_rirb_init(bus) != 0) {
		return (-1);
	}

	hda_bus_reg_write32(bus, HDAC_WAKEEN, 0xFFFFFFFFU);
	hda_bus_reg_write32(bus, HDAC_INTCTL,
	    HDAC_INTCTL_GIE | HDAC_INTCTL_CIE | 0x3FFFFFFFU);

	drivers_log("[HDA] init: corblbase=0x%08x(phys 0x%llx) "
	    "corbsize=0x%02x corbctl=0x%02x corbwp=0x%04x | "
	    "rirblbase=0x%08x(phys 0x%llx) rirbsize=0x%02x "
	    "rirbctl=0x%02x rirbwp=0x%04x\n",
	    hda_bus_reg_read32(bus, HDAC_CORBLBASE),
	    (unsigned long long)bus->corb_phys,
	    hda_bus_reg_read8(bus, HDAC_CORBSIZE),
	    hda_bus_reg_read8(bus, HDAC_CORBCTL),
	    hda_bus_reg_read16(bus, HDAC_CORBWP),
	    hda_bus_reg_read32(bus, HDAC_RIRBLBASE),
	    (unsigned long long)bus->rirb_phys,
	    hda_bus_reg_read8(bus, HDAC_RIRBSIZE),
	    hda_bus_reg_read8(bus, HDAC_RIRBCTL),
	    hda_bus_reg_read16(bus, HDAC_RIRBWP));
	return (0);
}

int
hda_bus_reset(hda_bus_t *bus)
{
	u32	i, ctl;

	if (bus == NULL) {
		return (-1);
	}
	ctl = hda_bus_reg_read32(bus, HDAC_GCTL);
	ctl &= ~HDAC_GCTL_CRST;
	hda_bus_reg_write32(bus, HDAC_GCTL, ctl);
	for (i = 0; i < 100; i++) {
		if ((hda_bus_reg_read32(bus, HDAC_GCTL) &
		    HDAC_GCTL_CRST) == 0) {
			break;
		}
		hda_bus_wait_us(10);
	}
	hda_bus_wait_us(100);
	ctl = hda_bus_reg_read32(bus, HDAC_GCTL);
	ctl |= HDAC_GCTL_CRST;
	hda_bus_reg_write32(bus, HDAC_GCTL, ctl);
	for (i = 0; i < 1000; i++) {
		if ((hda_bus_reg_read32(bus, HDAC_GCTL) &
		    HDAC_GCTL_CRST) != 0) {
			break;
		}
		hda_bus_wait_us(10);
	}
	return ((hda_bus_reg_read32(bus, HDAC_GCTL) &
	    HDAC_GCTL_CRST) != 0 ? 0 : (-1));
}

static int
verb_is_silent(u32 verb)
{
	u32	cmd;

	cmd = (verb >> 8) & 0xFFF;
	switch (cmd) {
	case HDAC_VERB_SET_CONNSEL:
	case HDAC_VERB_SET_STREAM:
	case HDAC_VERB_SET_AMP_GAIN_MUTE:
	case HDAC_VERB_SET_PIN_WIDGET_CTRL:
	case HDAC_VERB_SET_EAPD_BTLENABLE:
	case HDAC_VERB_SET_POWER:
	case HDAC_VERB_SET_PROC_STATE:
	case HDAC_VERB_SET_CONFIG_DEFAULT:
		return (1);
	default:
		return (0);
	}
}

int
hda_bus_command(hda_bus_t *bus, u32 verb, u32 *response)
{
	u32	wp;
	u32	rirb_rp;
	u32	last_wp;
	u32	i;
	u64	response_qw;

	if (bus == NULL || bus->corb == NULL || bus->rirb == NULL) {
		return (-1);
	}
	wp = hda_bus_reg_read16(bus, HDAC_CORBWP) & 0xFF;
	((volatile u32 *)bus->corb)[(wp + 1) & 0xFF] = verb;
	__sync_synchronize();
	hda_bus_reg_write16(bus, HDAC_CORBWP, (u16)((wp + 1) & 0xFF));
	bus->corb_wp = (wp + 1) & 0xFF;
	bus->corb_last_wp = bus->corb_wp;
	last_wp = bus->rirb_last_wp;
	rirb_rp = bus->rirb_rp;
	for (i = 0; i < 100000; i++) {
		u32	now;

		now = hda_bus_reg_read16(bus, HDAC_RIRBWP) & 0xFF;
		if (now != last_wp) {
			break;
		}
		hda_bus_wait_us(1);
	}
	if (hda_bus_reg_read16(bus, HDAC_RIRBWP) == last_wp) {
		if (verb_is_silent(verb)) {
			return (0);
		}
		drivers_log("[HDA] command verb=0x%08x RIRB timeout "
		    "(wp=0x%04x unchanged)\n", verb,
		    hda_bus_reg_read16(bus, HDAC_RIRBWP));
		bus->rirb_last_wp = hda_bus_reg_read16(bus,
		    HDAC_RIRBWP) & 0xFF;
		return (-1);
	}

	wp = hda_bus_reg_read16(bus, HDAC_RIRBWP) & 0xFF;
	rirb_rp = wp & (bus->rirb_entries - 1);
	response_qw = ((volatile u64 *)bus->rirb)[rirb_rp];
	bus->rirb_rp = (rirb_rp + 1) & (bus->rirb_entries - 1);
	bus->rirb_last_wp = wp;
	if (response != NULL) {
		*response = (u32)response_qw;
	}
	drivers_log("[HDA] command verb=0x%08x(codec=%u node=%u cmd=0x%03x "
	    "pay=0x%03x) -> resp=0x%08x rp=%u wp=%u\n", verb,
	    (verb >> HDA_CODEC_ADDR_SHIFT) & 0xF,
	    (verb >> 20) & 0xFF, (verb >> 8) & 0xFFF, verb & 0xFF,
	    (u32)response_qw, rirb_rp, wp);
	return (0);
}

u16
hda_bus_scan_codecs(hda_bus_t *bus)
{
	u16	mask;
	u8	addr;

	if (bus == NULL) {
		return (0);
	}
	mask = hda_bus_reg_read16(bus, HDAC_STATESTS);
	bus->codec_mask = mask;
	drivers_log("[HDA] STATESTS=0x%04x codec mask=0x%04x\n",
	    hda_bus_reg_read16(bus, HDAC_STATESTS), mask);
	for (addr = 0; addr < HDA_BUS_MAX_CODECS; addr++) {
		if ((mask & (1u << addr)) != 0) {
			hda_bus_codec_attach(bus, addr);
		}
	}
	return (mask);
}

int
hda_bus_codec_attach(hda_bus_t *bus, u8 addr)
{
	hda_codec_t	*codec;
	u32		resp, verb;

	if (bus == NULL || addr >= HDA_BUS_MAX_CODECS) {
		return (-1);
	}
	codec = &bus->codecs[addr];
	codec->addr = addr;
	codec->present = 1;

	verb = ((u32)addr << HDA_CODEC_ADDR_SHIFT) |
	    (0 << 20) | (HDAC_VERB_GET_PARAMETER << 8) |
	    HDAC_PARAM_VENDOR_ID;
	resp = 0;
	if (hda_bus_command(bus, verb, &resp) == 0) {
		codec->vendor_id = resp;
	}
	verb = ((u32)addr << HDA_CODEC_ADDR_SHIFT) |
	    (0 << 20) | (HDAC_VERB_GET_PARAMETER << 8) |
	    HDAC_PARAM_REV_ID;
	if (hda_bus_command(bus, verb, &resp) == 0) {
		codec->revision_id = resp;
	}
	verb = ((u32)addr << HDA_CODEC_ADDR_SHIFT) |
	    (0 << 20) | (HDAC_VERB_GET_PARAMETER << 8) |
	    HDAC_PARAM_NODE_COUNT;
	if (hda_bus_command(bus, verb, &resp) == 0) {
		codec->node_count = (resp >> 16) & 0xFF;
		codec->group_count = (resp >> 8) & 0xFF;
	}
	return (0);
}
