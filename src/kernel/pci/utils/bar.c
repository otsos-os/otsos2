/*
 * Copyright (c) 2026, otsos team
 *
 * [.BSD-2-clause license text...]
 */

/* !DEFINES!

$define %type u8 as 8 bit unsigned
$define %type u16 as 16 bit unsigned
$define %type u32 as 32 bit unsigned
$define %type u64 as 64 bit unsigned
$define %type int as 32 bit signed
$define %type pci_device_t as struct with PCI device info
$define %type pci_bar_t as struct with BAR info

$define %func pci_bar_count_from_header as function with args u8
$define %func pci_bar_clear as procedure with args pci_bar_t *, u8
$define %func pci_get_bar_count as function with args const pci_device_t *
$define %func pci_read_bar as function with args const pci_device_t *, u8, pci_bar_t *

*/

/* !SPACE!

$space %internal pci_bar_count_from_header, pci_bar_clear
$space %export pci_get_bar_count, pci_read_bar

*/

#include <kernel/pci/utils/bar.h>

static int
pci_bar_count_from_header(u8 header_type)
{
	header_type &= 0x7F;
	if (header_type == 0x00) {
		return (6);
	}
	if (header_type == 0x01) {
		return (2);
	}
	return (0);
}

static void
pci_bar_clear(pci_bar_t *bar, u8 index)
{
	bar->base = 0;
	bar->size = 0;
	bar->index = index;
	bar->is_io = 0;
	bar->is_64 = 0;
	bar->prefetchable = 0;
}

int
pci_get_bar_count(const pci_device_t *dev)
{
	if (!dev) {
		return (0);
	}
	return (pci_bar_count_from_header(dev->header_type));
}

int
pci_read_bar(const pci_device_t *dev, u8 index, pci_bar_t *out)
{
	int	max_bars;
	u8	offset;
	u32	original_low;

	if (!dev || !out) {
		return (-1);
	}

	max_bars = pci_bar_count_from_header(dev->header_type);
	if (index >= (u8)max_bars) {
		return (-1);
	}

	pci_bar_clear(out, index);

	offset = PCI_CFG_BAR0 + (index * 4);
	original_low = pci_cfg_read32(dev->bus, dev->slot,
	    dev->function, offset);

	if (original_low == 0) {
		return (0);
	}

	if (original_low & PCI_BAR_IO_SPACE) {
		u32	mask_low, size_mask;

		out->is_io = 1;
		out->base = (u64)(original_low & PCI_BAR_IO_MASK);

		pci_cfg_write32(dev->bus, dev->slot,
		    dev->function, offset, 0xFFFFFFFF);
		mask_low = pci_cfg_read32(dev->bus, dev->slot,
		    dev->function, offset);
		pci_cfg_write32(dev->bus, dev->slot,
		    dev->function, offset, original_low);

		size_mask = mask_low & PCI_BAR_IO_MASK;
		if (size_mask) {
			out->size = (u64)(~size_mask + 1);
		}
		return (0);
	}

	out->prefetchable = (original_low & PCI_BAR_MEM_PREFETCH) ?
	    1 : 0;

	{
		u32	mem_type;

		mem_type = (original_low >> 1) & 0x3;
		if (mem_type == PCI_BAR_MEM_TYPE_64) {
			u32	original_high, mask_low;
			u32	mask_high;
			u64	base, mask;

			if (index + 1 >= (u8)max_bars) {
				return (-1);
			}

			original_high = pci_cfg_read32(dev->bus,
			    dev->slot, dev->function, offset + 4);

			pci_cfg_write32(dev->bus, dev->slot,
			    dev->function, offset, 0xFFFFFFFF);
			pci_cfg_write32(dev->bus, dev->slot,
			    dev->function, offset + 4,
			    0xFFFFFFFF);
			mask_low = pci_cfg_read32(dev->bus,
			    dev->slot, dev->function, offset);
			mask_high = pci_cfg_read32(dev->bus,
			    dev->slot, dev->function, offset + 4);
			pci_cfg_write32(dev->bus, dev->slot,
			    dev->function, offset, original_low);
			pci_cfg_write32(dev->bus, dev->slot,
			    dev->function, offset + 4,
			    original_high);

			base = ((u64)original_high << 32) |
			    (original_low & PCI_BAR_MEM_MASK);
			mask = ((u64)mask_high << 32) |
			    (mask_low & PCI_BAR_MEM_MASK);

			out->is_64 = 1;
			out->base = base;
			if (mask) {
				out->size = (~mask + 1);
			}
			return (0);
		}
	}

	out->base = (u64)(original_low & PCI_BAR_MEM_MASK);

	{
		u32	mask_low, size_mask;

		pci_cfg_write32(dev->bus, dev->slot,
		    dev->function, offset, 0xFFFFFFFF);
		mask_low = pci_cfg_read32(dev->bus, dev->slot,
		    dev->function, offset);
		pci_cfg_write32(dev->bus, dev->slot,
		    dev->function, offset, original_low);

		size_mask = mask_low & PCI_BAR_MEM_MASK;
		if (size_mask) {
			out->size = (u64)(~size_mask + 1);
		}
	}

	return (0);
}
