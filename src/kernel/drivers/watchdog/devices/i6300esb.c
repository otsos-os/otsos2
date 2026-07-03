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

$define %type u8 as 8 bit unsigned
$define %type u16 as 16 bit unsigned
$define %type u32 as 32 bit unsigned
$define %type u64 as 64 bit unsigned
$define %type int as 32 bit signed
$define %type i6300esb_priv_t as struct with mmio/io mode, base addr, size

$define %func i6300esb_mmio_map as procedure with args u64, u64
$define %func i6300esb_write as procedure with args i6300esb_priv_t *, u16, u32
$define %func i6300esb_read as function with args i6300esb_priv_t *, u16
$define %func i6300esb_set_timeout as function with args watchdog_device_t *, u32
$define %func i6300esb_start as function with args watchdog_device_t *, u32
$define %func i6300esb_stop as function with args watchdog_device_t *
$define %func i6300esb_ping as function with args watchdog_device_t *
$define %func i6300esb_probe as function with args pci_device_t *, const pci_match_t *
$define %func watchdog_i6300esb_init as function with args void

*/

/* !SPACE!

$space %internal i6300esb_mmio_map, i6300esb_write, i6300esb_read
$space %internal i6300esb_set_timeout, i6300esb_start
$space %internal i6300esb_stop, i6300esb_ping, i6300esb_probe
$space %export watchdog_i6300esb_init

*/

#include <kernel/drivers/watchdog/watchdog.h>
#include <mm/vm/pmap.h>
#include <kernel/pci/utils/bar.h>
#include <kernel/pci/utils/io.h>
#include <mlibc/stdio.h>

#define	I6300ESB_VENDOR_ID	0x8086
#define	I6300ESB_DEVICE_ID	0x25AB

#define	ESB_REG_WDTCSR		0x00
#define	ESB_REG_WDTVAL		0x04
#define	ESB_REG_RELOAD		0x0C

#define	ESB_WDTCSR_ENABLE	0x00000001

typedef struct i6300esb_priv {
	u8		use_mmio;
	u16		io_base;
	volatile u32	*mmio_base;
	u64		mmio_size;
} i6300esb_priv_t;

static i6300esb_priv_t	i6300esb_priv;

static void
i6300esb_mmio_map(u64 base, u64 size)
{
	u64	start, end, addr;

	if (size == 0) {
		size = PAGE_SIZE;
	}

	start = base & ~(PAGE_SIZE - 1);
	end = (base + size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

	for (addr = start; addr < end; addr += PAGE_SIZE) {
		pmap_enter(addr, addr, PTE_RW | PTE_PCD | PTE_PWT);
	}
}

static void
i6300esb_write(i6300esb_priv_t *priv, u16 offset, u32 value)
{
	volatile u32	*reg;

	if (priv->use_mmio) {
		reg = (volatile u32 *)
		    ((u8 *)priv->mmio_base + offset);
		*reg = value;
		return;
	}
	pci_outl((u16)(priv->io_base + offset), value);
}

static u32
i6300esb_read(i6300esb_priv_t *priv, u16 offset)
{
	volatile u32	*reg;

	if (priv->use_mmio) {
		reg = (volatile u32 *)
		    ((u8 *)priv->mmio_base + offset);
		return (*reg);
	}
	return (pci_inl((u16)(priv->io_base + offset)));
}

static int
i6300esb_set_timeout(watchdog_device_t *dev, u32 timeout_sec)
{
	i6300esb_priv_t	*priv;

	if (!dev || !dev->priv) {
		return (-1);
	}

	priv = (i6300esb_priv_t *)dev->priv;
	i6300esb_write(priv, ESB_REG_WDTVAL, timeout_sec);
	i6300esb_write(priv, ESB_REG_RELOAD, 0x01);
	return (0);
}

static int
i6300esb_start(watchdog_device_t *dev, u32 timeout_sec)
{
	i6300esb_priv_t	*priv;
	u32		ctrl;

	if (!dev || !dev->priv) {
		return (-1);
	}

	priv = (i6300esb_priv_t *)dev->priv;
	i6300esb_write(priv, ESB_REG_WDTVAL, timeout_sec);

	ctrl = i6300esb_read(priv, ESB_REG_WDTCSR);
	ctrl |= ESB_WDTCSR_ENABLE;
	i6300esb_write(priv, ESB_REG_WDTCSR, ctrl);

	i6300esb_write(priv, ESB_REG_RELOAD, 0x01);
	return (0);
}

static int
i6300esb_stop(watchdog_device_t *dev)
{
	i6300esb_priv_t	*priv;
	u32		ctrl;

	if (!dev || !dev->priv) {
		return (-1);
	}

	priv = (i6300esb_priv_t *)dev->priv;
	ctrl = i6300esb_read(priv, ESB_REG_WDTCSR);
	ctrl &= ~ESB_WDTCSR_ENABLE;
	i6300esb_write(priv, ESB_REG_WDTCSR, ctrl);
	return (0);
}

static int
i6300esb_ping(watchdog_device_t *dev)
{
	i6300esb_priv_t	*priv;

	if (!dev || !dev->priv) {
		return (-1);
	}

	priv = (i6300esb_priv_t *)dev->priv;
	i6300esb_write(priv, ESB_REG_RELOAD, 0x01);
	return (0);
}

static const watchdog_ops_t i6300esb_ops = {
	.start		= i6300esb_start,
	.stop		= i6300esb_stop,
	.ping		= i6300esb_ping,
	.set_timeout	= i6300esb_set_timeout,
};

static watchdog_device_t i6300esb_device = {
	.name		= "Intel 6300ESB",
	.min_timeout_sec	= 1,
	.max_timeout_sec	= 65535,
	.timeout_sec	= 0,
	.running		= 0,
	.pci_dev		= NULL,
	.priv		= &i6300esb_priv,
	.ops		= &i6300esb_ops,
};

static int
i6300esb_probe(pci_device_t *dev, const pci_match_t *match)
{
	pci_bar_t	bar0;

	(void)match;
	if (!dev) {
		return (-1);
	}

	if (pci_read_bar(dev, 0, &bar0) != 0 || bar0.base == 0) {
		drivers_log("[WDT] i6300esb: BAR0 missing\n");
		return (-1);
	}

	if (bar0.is_io) {
		pci_enable_io_space(dev);
		i6300esb_priv.use_mmio = 0;
		i6300esb_priv.io_base = (u16)bar0.base;
	} else {
		pci_enable_memory_space(dev);
		i6300esb_priv.use_mmio = 1;
		i6300esb_priv.mmio_base =
		    (volatile u32 *)(u64)bar0.base;
		i6300esb_priv.mmio_size = bar0.size;
		i6300esb_mmio_map(bar0.base, bar0.size);
	}
	i6300esb_device.pci_dev = dev;

	return (watchdog_register_device(&i6300esb_device));
}

static pci_match_t i6300esb_matches[] = {
	{
		.vendor_id	= I6300ESB_VENDOR_ID,
		.device_id	= I6300ESB_DEVICE_ID,
		.class_code	= PCI_ANY_CLASS,
		.subclass	= PCI_ANY_SUBCLASS,
		.prog_if	= PCI_ANY_PROGIF,
	},
};

static pci_driver_t i6300esb_pci_driver = {
	.name		= "watchdog-i6300esb",
	.matches	= i6300esb_matches,
	.match_count	= 1,
	.probe		= i6300esb_probe,
	.remove		= NULL,
};

int
watchdog_i6300esb_init(void)
{
	return (pci_register_driver(&i6300esb_pci_driver));
}
