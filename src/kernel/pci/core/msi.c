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

$define %type pci_msi_slot_t as per function MSI capability state

$define %func pci_find_capability as function with args const pci_device_t *, u8
$define %func pci_msi_slot_find as function with args const pci_device_t *
$define %func pci_msi_slot_alloc as function with args void
$define %func pci_msi_program as function with args void *, u8, u8
$define %func pci_msi_mask as procedure with args void *, int
$define %func pci_msi_disable as procedure with args void *
$define %func pci_msi_supported as function with args const pci_device_t *
$define %func pci_msi_alloc as function with args pci_device_t *
$define %func pci_msi_release as procedure with args pci_device_t *

*/

/* !SPACE!

$space %internal pci_msi_slot_find, pci_msi_slot_alloc, pci_msi_program
$space %internal pci_msi_mask, pci_msi_disable
$space %export pci_find_capability, pci_msi_supported
$space %export pci_msi_alloc, pci_msi_release

*/

#include <kernel/interrupts/irq.h>
#include <kernel/pci/pci.h>
#include <mlibc/mlibc.h>
#include <mlibc/stdio.h>

#define	PCI_MSI_MAX_SLOTS		16

#define	PCI_CAP_WALK_MAX		48
#define	PCI_CAP_WINDOW_FIRST		0x40
#define	PCI_CAP_WINDOW_LAST		0xFC
#define	PCI_CAP_WALK_MAX		48
#define	PCI_CAP_WINDOW_FIRST		0x40
#define	PCI_CAP_WINDOW_LAST		0xFC

#define	PCI_STATUS_CAP_LIST		0x0010
#define	PCI_CFG_CAP_PTR			0x34
#define	PCI_COMMAND_INTX_DISABLE	0x0400

#define	PCI_CAP_ID_MSI			0x05

#define	PCI_MSI_CTRL			0x02
#define	PCI_MSI_ADDR_LO			0x04
#define	PCI_MSI_ADDR_HI			0x08
#define	PCI_MSI_DATA_32			0x08
#define	PCI_MSI_DATA_64			0x0C
#define	PCI_MSI_MASK_32			0x0C
#define	PCI_MSI_MASK_64			0x10

#define	PCI_MSI_CTRL_ENABLE		0x0001
#define	PCI_MSI_CTRL_MULTI_EN_MASK	0x0070
#define	PCI_MSI_CTRL_ADDR64		0x0080
#define	PCI_MSI_CTRL_PER_VECTOR_MASK	0x0100

#define	PCI_MSI_ADDR_BASE		0xFEE00000U
#define	PCI_MSI_ADDR_DEST_SHIFT		12

typedef struct pci_msi_slot {
	pci_device_t	*dev;
	u32		msi_id;
	u8		cap;
	u8		addr64;
	u8		per_vector_mask;
	u8		used;
} pci_msi_slot_t;

static pci_msi_slot_t	pci_msi_slots[PCI_MSI_MAX_SLOTS];

u8
pci_find_capability(const pci_device_t *dev, u8 cap_id)
{
	u16	status;
	u8	offset;
	u8	guard;
	u8	id;

	if (dev == NULL) {
		return (0);
	}
	status = pci_cfg_read16(dev->bus, dev->slot, dev->function,
	    PCI_CFG_STATUS);
	if ((status & PCI_STATUS_CAP_LIST) == 0) {
		return (0);
	}
	offset = pci_cfg_read8(dev->bus, dev->slot, dev->function,
	    PCI_CFG_CAP_PTR) & 0xFC;
	for (guard = 0; guard < PCI_CAP_WALK_MAX; guard++) {
		if (offset < PCI_CAP_WINDOW_FIRST ||
		    offset > PCI_CAP_WINDOW_LAST) {
			return (0);
		}
		id = pci_cfg_read8(dev->bus, dev->slot, dev->function, offset);
		if (id == 0xFF) {
			return (0);
		}
		if (id == cap_id) {
			return (offset);
		}
		offset = pci_cfg_read8(dev->bus, dev->slot, dev->function,
		    (u8)(offset + 1)) & 0xFC;
		if (offset == 0) {
			return (0);
		}
	}
	return (0);
}

static pci_msi_slot_t *
pci_msi_slot_find(const pci_device_t *dev)
{
	int	i;

	for (i = 0; i < PCI_MSI_MAX_SLOTS; i++) {
		if (pci_msi_slots[i].used && pci_msi_slots[i].dev == dev) {
			return (&pci_msi_slots[i]);
		}
	}
	return (NULL);
}

static pci_msi_slot_t *
pci_msi_slot_alloc(void)
{
	int	i;

	for (i = 0; i < PCI_MSI_MAX_SLOTS; i++) {
		if (!pci_msi_slots[i].used) {
			return (&pci_msi_slots[i]);
		}
	}
	return (NULL);
}

static int
pci_msi_program(void *arg, u8 vector, u8 apic_id)
{
	pci_msi_slot_t	*slot;
	u16		ctrl;
	u16		command;

	slot = arg;
	if (slot == NULL || !slot->used || slot->dev == NULL) {
		return (-1);
	}
	ctrl = pci_cfg_read16(slot->dev->bus, slot->dev->slot,
	    slot->dev->function, (u8)(slot->cap + PCI_MSI_CTRL));
	ctrl &= (u16)~PCI_MSI_CTRL_ENABLE;
	pci_cfg_write16(slot->dev->bus, slot->dev->slot, slot->dev->function,
	    (u8)(slot->cap + PCI_MSI_CTRL), ctrl);

	pci_cfg_write32(slot->dev->bus, slot->dev->slot, slot->dev->function,
	    (u8)(slot->cap + PCI_MSI_ADDR_LO),
	    PCI_MSI_ADDR_BASE | ((u32)apic_id << PCI_MSI_ADDR_DEST_SHIFT));
	if (slot->addr64) {
		pci_cfg_write32(slot->dev->bus, slot->dev->slot,
		    slot->dev->function, (u8)(slot->cap + PCI_MSI_ADDR_HI), 0);
		pci_cfg_write16(slot->dev->bus, slot->dev->slot,
		    slot->dev->function, (u8)(slot->cap + PCI_MSI_DATA_64),
		    vector);
	} else {
		pci_cfg_write16(slot->dev->bus, slot->dev->slot,
		    slot->dev->function, (u8)(slot->cap + PCI_MSI_DATA_32),
		    vector);
	}
	if (slot->per_vector_mask) {
		pci_cfg_write32(slot->dev->bus, slot->dev->slot,
		    slot->dev->function, (u8)(slot->cap + (slot->addr64 ?
		    PCI_MSI_MASK_64 : PCI_MSI_MASK_32)), 0);
	}
	ctrl &= (u16)~PCI_MSI_CTRL_MULTI_EN_MASK;
	ctrl |= PCI_MSI_CTRL_ENABLE;
	pci_cfg_write16(slot->dev->bus, slot->dev->slot, slot->dev->function,
	    (u8)(slot->cap + PCI_MSI_CTRL), ctrl);

	command = pci_read_command(slot->dev);
	if ((command & PCI_COMMAND_INTX_DISABLE) == 0) {
		pci_write_command(slot->dev,
		    (u16)(command | PCI_COMMAND_INTX_DISABLE));
	}
	return (0);
}

static void
pci_msi_mask(void *arg, int masked)
{
	pci_msi_slot_t	*slot;
	u8		offset;

	slot = arg;
	if (slot == NULL || !slot->used || slot->dev == NULL ||
	    !slot->per_vector_mask) {
		return;
	}
	offset = (u8)(slot->cap + (slot->addr64 ? PCI_MSI_MASK_64 :
	    PCI_MSI_MASK_32));
	pci_cfg_write32(slot->dev->bus, slot->dev->slot, slot->dev->function,
	    offset, masked ? 1U : 0U);
}

static void
pci_msi_disable(void *arg)
{
	pci_msi_slot_t	*slot;
	u16		ctrl;

	slot = arg;
	if (slot == NULL || !slot->used || slot->dev == NULL) {
		return;
	}
	ctrl = pci_cfg_read16(slot->dev->bus, slot->dev->slot,
	    slot->dev->function, (u8)(slot->cap + PCI_MSI_CTRL));
	ctrl &= (u16)~PCI_MSI_CTRL_ENABLE;
	pci_cfg_write16(slot->dev->bus, slot->dev->slot, slot->dev->function,
	    (u8)(slot->cap + PCI_MSI_CTRL), ctrl);
}

int
pci_msi_supported(const pci_device_t *dev)
{
	return (pci_find_capability(dev, PCI_CAP_ID_MSI) != 0 ? 1 : 0);
}
int
pci_msi_alloc(pci_device_t *dev)
{
	irq_msi_ops_t	ops;
	pci_msi_slot_t	*slot;
	u16		ctrl;
	int		msi_id;
	u8		cap;

	if (dev == NULL) {
		return (-1);
	}
	slot = pci_msi_slot_find(dev);
	if (slot != NULL) {
		return ((int)slot->msi_id);
	}
	cap = pci_find_capability(dev, PCI_CAP_ID_MSI);
	if (cap == 0) {
		return (-1);
	}
	slot = pci_msi_slot_alloc();
	if (slot == NULL) {
		return (-1);
	}
	ctrl = pci_cfg_read16(dev->bus, dev->slot, dev->function,
	    (u8)(cap + PCI_MSI_CTRL));
	memset(slot, 0, sizeof(*slot));
	slot->dev = dev;
	slot->cap = cap;
	slot->addr64 = (ctrl & PCI_MSI_CTRL_ADDR64) != 0 ? 1 : 0;
	slot->per_vector_mask =
	    (ctrl & PCI_MSI_CTRL_PER_VECTOR_MASK) != 0 ? 1 : 0;
	slot->used = 1;

	pci_cfg_write16(dev->bus, dev->slot, dev->function,
	    (u8)(cap + PCI_MSI_CTRL), (u16)(ctrl & ~PCI_MSI_CTRL_ENABLE));

	memset(&ops, 0, sizeof(ops));
	ops.program = pci_msi_program;
	ops.mask = slot->per_vector_mask ? pci_msi_mask : NULL;
	ops.disable = pci_msi_disable;
	ops.arg = slot;
	msi_id = irq_msi_ops_register(&ops);
	if (msi_id < 0) {
		memset(slot, 0, sizeof(*slot));
		return (-1);
	}
	slot->msi_id = (u32)msi_id;
	return (msi_id);
}

void
pci_msi_release(pci_device_t *dev)
{
	pci_msi_slot_t	*slot;

	slot = pci_msi_slot_find(dev);
	if (slot == NULL) {
		return;
	}
	pci_msi_disable(slot);
	irq_msi_ops_unregister(slot->msi_id);
	memset(slot, 0, sizeof(*slot));
}
