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
$define %type int as 32 bit signed

$define %func pci_find_capability as function with args const pci_device_t *, u8
$define %func pci_msi_supported as function with args const pci_device_t *
$define %func pci_msi_enable as function with args const pci_device_t *, u8, u8
$define %func pci_msi_disable as procedure with args const pci_device_t *
$define %func pci_msi_policy_enabled as function with args void
$define %func pci_msix_supported as function with args const pci_device_t *
$define %func pci_msix_vector_count as function with args const pci_device_t *
$define %func pci_msix_table_map as function with args const pci_device_t *, u8, u16 *
$define %func pci_msix_enable as function with args const pci_device_t *, u16, u8, u8
$define %func pci_msix_mask as function with args const pci_device_t *, u16, int
$define %func pci_msix_disable as procedure with args const pci_device_t *
$define %func pci_msix_policy_enabled as function with args void
$define %func pci_intr_msi_supported as function with args const pci_device_t *

*/

/* !SPACE!

$space %export pci_find_capability, pci_msi_supported
$space %export pci_msi_enable, pci_msi_disable
$space %export pci_msi_policy_enabled
$space %internal pci_msix_table_map
$space %export pci_msix_supported, pci_msix_vector_count
$space %export pci_msix_enable, pci_msix_mask, pci_msix_disable
$space %export pci_msix_policy_enabled, pci_intr_msi_supported

*/

#include <kernel/cm/cm.h>
#include <kernel/mm/vm/pmap.h>
#include <kernel/pci/pci.h>
#include <kernel/pci/utils/bar.h>
#include <mlibc/mlibc.h>

#define	PCI_CAP_WALK_MAX	48
#define	PCI_CAP_OFFSET_MIN	0x40
#define	PCI_CAP_NEXT_MASK	0xFC

u8
pci_find_capability(const pci_device_t *dev, u8 cap_id)
{
	u16	status;
	u8	offset, id, next;
	int	guard;

	if (dev == NULL) {
		return (0);
	}
	status = pci_cfg_read16(dev->bus, dev->slot, dev->function,
	    PCI_CFG_STATUS);
	if ((status & PCI_STATUS_CAP_LIST) == 0) {
		return (0);
	}
	offset = pci_cfg_read8(dev->bus, dev->slot, dev->function,
	    PCI_CFG_CAP_PTR) & PCI_CAP_NEXT_MASK;
	for (guard = 0; guard < PCI_CAP_WALK_MAX; guard++) {
		if (offset < PCI_CAP_OFFSET_MIN || offset == 0xFF) {
			return (0);
		}
		id = pci_cfg_read8(dev->bus, dev->slot, dev->function, offset);
		if (id == 0xFF) {
			return (0);
		}
		if (id == cap_id) {
			return (offset);
		}
		next = pci_cfg_read8(dev->bus, dev->slot, dev->function,
		    (u8)(offset + 1)) & PCI_CAP_NEXT_MASK;
		if (next == offset) {
			return (0);
		}
		offset = next;
	}
	return (0);
}

int
pci_msi_supported(const pci_device_t *dev)
{
	return (pci_find_capability(dev, PCI_CAP_ID_MSI) != 0);
}

int
pci_msi_enable(const pci_device_t *dev, u8 vector, u8 dest_apic_id)
{
	u32	address;
	u16	control, command;
	u8	cap, data_offset;

	if (dev == NULL || vector < 32) {
		return (-1);
	}
	cap = pci_find_capability(dev, PCI_CAP_ID_MSI);
	if (cap == 0) {
		return (-1);
	}
	control = pci_cfg_read16(dev->bus, dev->slot, dev->function,
	    (u8)(cap + PCI_MSI_CTRL));
	control &= (u16)~PCI_MSI_CTRL_ENABLE;
	pci_cfg_write16(dev->bus, dev->slot, dev->function,
	    (u8)(cap + PCI_MSI_CTRL), control);

	address = PCI_MSI_ADDR_BASE |
	    ((u32)dest_apic_id << PCI_MSI_ADDR_DEST_SHIFT);
	pci_cfg_write32(dev->bus, dev->slot, dev->function,
	    (u8)(cap + PCI_MSI_ADDR_LO), address);
	if ((control & PCI_MSI_CTRL_ADDR_64) != 0) {
		pci_cfg_write32(dev->bus, dev->slot, dev->function,
		    (u8)(cap + PCI_MSI_ADDR_HI), 0);
		data_offset = PCI_MSI_DATA_64;
	} else {
		data_offset = PCI_MSI_DATA_32;
	}
	pci_cfg_write16(dev->bus, dev->slot, dev->function,
	    (u8)(cap + data_offset), vector);

	control &= (u16)~PCI_MSI_CTRL_MULTI_ENABLE;
	control |= PCI_MSI_CTRL_ENABLE;
	pci_cfg_write16(dev->bus, dev->slot, dev->function,
	    (u8)(cap + PCI_MSI_CTRL), control);

	command = pci_read_command(dev);
	command |= PCI_COMMAND_INTX_DISABLE;
	pci_write_command(dev, command);

	control = pci_cfg_read16(dev->bus, dev->slot, dev->function,
	    (u8)(cap + PCI_MSI_CTRL));
	if ((control & PCI_MSI_CTRL_ENABLE) == 0) {
		command = pci_read_command(dev);
		command &= (u16)~PCI_COMMAND_INTX_DISABLE;
		pci_write_command(dev, command);
		return (-1);
	}
	return (0);
}

void
pci_msi_disable(const pci_device_t *dev)
{
	u16	control, command;
	u8	cap;

	if (dev == NULL) {
		return;
	}
	cap = pci_find_capability(dev, PCI_CAP_ID_MSI);
	if (cap == 0) {
		return;
	}
	control = pci_cfg_read16(dev->bus, dev->slot, dev->function,
	    (u8)(cap + PCI_MSI_CTRL));
	control &= (u16)~PCI_MSI_CTRL_ENABLE;
	pci_cfg_write16(dev->bus, dev->slot, dev->function,
	    (u8)(cap + PCI_MSI_CTRL), control);
	command = pci_read_command(dev);
	command &= (u16)~PCI_COMMAND_INTX_DISABLE;
	pci_write_command(dev, command);
}

int
pci_msi_policy_enabled(void)
{
	if (!cm_is_initialized()) {
		return (1);
	}
	return (cm_get_bool_default("SYSTEM", "IRQ", "MsiEnabled", 1));
}

int
pci_msix_policy_enabled(void)
{
	if (!cm_is_initialized()) {
		return (1);
	}
	return (cm_get_bool_default("SYSTEM", "IRQ", "MsixEnabled", 1));
}

int
pci_msix_supported(const pci_device_t *dev)
{
	return (pci_find_capability(dev, PCI_CAP_ID_MSIX) != 0);
}

int
pci_intr_msi_supported(const pci_device_t *dev)
{
	if (pci_msix_supported(dev) && pci_msix_policy_enabled()) {
		return (1);
	}
	return (pci_msi_supported(dev) && pci_msi_policy_enabled());
}

int
pci_msix_vector_count(const pci_device_t *dev)
{
	u16	control;
	u8	cap;

	cap = pci_find_capability(dev, PCI_CAP_ID_MSIX);
	if (cap == 0) {
		return (0);
	}
	control = pci_cfg_read16(dev->bus, dev->slot, dev->function,
	    (u8)(cap + PCI_MSIX_CTRL));
	return ((int)(control & PCI_MSIX_CTRL_TABLE_SIZE) + 1);
}

static volatile u32 *
pci_msix_table_map(const pci_device_t *dev, u8 cap, u16 *count)
{
	pci_bar_t	bar;
	u64		span;
	u32		table;
	u32		offset;
	u16		control;
	u8		bir;

	control = pci_cfg_read16(dev->bus, dev->slot, dev->function,
	    (u8)(cap + PCI_MSIX_CTRL));
	*count = (u16)((control & PCI_MSIX_CTRL_TABLE_SIZE) + 1);
	if (*count > PCI_MSIX_MAX_VECTORS) {
		return (NULL);
	}
	table = pci_cfg_read32(dev->bus, dev->slot, dev->function,
	    (u8)(cap + PCI_MSIX_TABLE));
	if (table == 0xFFFFFFFFU) {
		return (NULL);
	}
	bir = (u8)(table & PCI_MSIX_BIR_MASK);
	offset = table & PCI_MSIX_OFFSET_MASK;
	if (pci_read_bar(dev, bir, &bar) != 0) {
		return (NULL);
	}
	if (bar.is_io || bar.base == 0 || bar.size == 0) {
		return (NULL);
	}
	span = (u64)*count * PCI_MSIX_ENTRY_SIZE;
	if ((u64)offset + span > bar.size) {
		return (NULL);
	}
	return ((volatile u32 *)pmap_map_mmio(bar.base + offset, span));
}

int
pci_msix_enable(const pci_device_t *dev, u16 entry, u8 vector,
    u8 dest_apic_id)
{
	volatile u32	*table;
	u32		address;
	u16		control, command, count;
	u8		cap;

	if (dev == NULL || vector < 32) {
		return (-1);
	}
	cap = pci_find_capability(dev, PCI_CAP_ID_MSIX);
	if (cap == 0) {
		return (-1);
	}
	pci_enable_memory_space(dev);
	table = pci_msix_table_map(dev, cap, &count);
	if (table == NULL || entry >= count) {
		return (-1);
	}
	table += (u32)entry * (PCI_MSIX_ENTRY_SIZE / sizeof(u32));

	table[PCI_MSIX_ENTRY_VECTOR_CTRL / sizeof(u32)] =
	    PCI_MSIX_VECTOR_CTRL_MASK;

	address = PCI_MSI_ADDR_BASE |
	    ((u32)dest_apic_id << PCI_MSI_ADDR_DEST_SHIFT);
	table[PCI_MSIX_ENTRY_ADDR_LO / sizeof(u32)] = address;
	table[PCI_MSIX_ENTRY_ADDR_HI / sizeof(u32)] = 0;
	table[PCI_MSIX_ENTRY_DATA / sizeof(u32)] = vector;
	table[PCI_MSIX_ENTRY_VECTOR_CTRL / sizeof(u32)] = 0;

	control = pci_cfg_read16(dev->bus, dev->slot, dev->function,
	    (u8)(cap + PCI_MSIX_CTRL));
	control &= (u16)~PCI_MSIX_CTRL_FUNCTION_MASK;
	control |= PCI_MSIX_CTRL_ENABLE;
	pci_cfg_write16(dev->bus, dev->slot, dev->function,
	    (u8)(cap + PCI_MSIX_CTRL), control);

	command = pci_read_command(dev);
	command |= PCI_COMMAND_INTX_DISABLE;
	pci_write_command(dev, command);

	control = pci_cfg_read16(dev->bus, dev->slot, dev->function,
	    (u8)(cap + PCI_MSIX_CTRL));
	if ((control & PCI_MSIX_CTRL_ENABLE) == 0) {
		table[PCI_MSIX_ENTRY_VECTOR_CTRL / sizeof(u32)] =
		    PCI_MSIX_VECTOR_CTRL_MASK;
		command = pci_read_command(dev);
		command &= (u16)~PCI_COMMAND_INTX_DISABLE;
		pci_write_command(dev, command);
		return (-1);
	}
	return (0);
}

int
pci_msix_mask(const pci_device_t *dev, u16 entry, int masked)
{
	volatile u32	*table;
	u16		count;
	u8		cap;

	if (dev == NULL) {
		return (-1);
	}
	cap = pci_find_capability(dev, PCI_CAP_ID_MSIX);
	if (cap == 0) {
		return (-1);
	}
	table = pci_msix_table_map(dev, cap, &count);
	if (table == NULL || entry >= count) {
		return (-1);
	}
	table += (u32)entry * (PCI_MSIX_ENTRY_SIZE / sizeof(u32));
	table[PCI_MSIX_ENTRY_VECTOR_CTRL / sizeof(u32)] =
	    (masked != 0) ? PCI_MSIX_VECTOR_CTRL_MASK : 0;
	return (0);
}

void
pci_msix_disable(const pci_device_t *dev)
{
	volatile u32	*table;
	u32		i;
	u16		control, command, count;
	u8		cap;

	if (dev == NULL) {
		return;
	}
	cap = pci_find_capability(dev, PCI_CAP_ID_MSIX);
	if (cap == 0) {
		return;
	}
	control = pci_cfg_read16(dev->bus, dev->slot, dev->function,
	    (u8)(cap + PCI_MSIX_CTRL));
	control |= PCI_MSIX_CTRL_FUNCTION_MASK;
	pci_cfg_write16(dev->bus, dev->slot, dev->function,
	    (u8)(cap + PCI_MSIX_CTRL), control);

	table = pci_msix_table_map(dev, cap, &count);
	if (table != NULL) {
		for (i = 0; i < count; i++) {
			table[(i * (PCI_MSIX_ENTRY_SIZE / sizeof(u32))) +
			    (PCI_MSIX_ENTRY_VECTOR_CTRL / sizeof(u32))] =
			    PCI_MSIX_VECTOR_CTRL_MASK;
		}
	}

	control &= (u16)~PCI_MSIX_CTRL_ENABLE;
	pci_cfg_write16(dev->bus, dev->slot, dev->function,
	    (u8)(cap + PCI_MSIX_CTRL), control);

	command = pci_read_command(dev);
	command &= (u16)~PCI_COMMAND_INTX_DISABLE;
	pci_write_command(dev, command);
}
