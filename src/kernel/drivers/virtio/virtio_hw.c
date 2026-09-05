/*
 * Copyright (c) 2026, otsos team
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* !DEFINES!

$define %type u8 as 8 bit unsigned
$define %type u16 as 16 bit unsigned
$define %type u32 as 32 bit unsigned
$define %type u64 as 64 bit unsigned
$define %type pci_bar_t as struct with BAR info
$define %type virtio_pci_common_cfg_t as packed struct with common config
$define %type virtio_gpu_config_t as packed struct with GPU device config
$define %type virtio_hw_t as struct with resolved transport state

$define %func mmio_remap as function with args u64, u64
$define %func mmio_read8 as function with args volatile void *
$define %func mmio_read16 as function with args volatile void *
$define %func mmio_read32 as function with args volatile void *
$define %func mmio_read64 as function with args volatile void *
$define %func mmio_write8 as procedure with args volatile void *, u8
$define %func mmio_write16 as procedure with args volatile void *, u16
$define %func mmio_write32 as procedure with args volatile void *, u32
$define %func mmio_write64 as procedure with args volatile void *, u64
$define %func io_read8 as function with args u64, u32
$define %func io_read16 as function with args u64, u32
$define %func io_read32 as function with args u64, u32
$define %func io_write8 as procedure with args u64, u32, u8
$define %func io_write16 as procedure with args u64, u32, u16
$define %func io_write32 as procedure with args u64, u32, u32
$define %func parse_capability as procedure with args virtio_hw_t *, u8, u8, u32, u32, u32
$define %func walk_capabilities as procedure with args virtio_hw_t *
$define %func legacy_init as function with args virtio_hw_t *
$define %func common_reg as function with args virtio_hw_t *, u32
$define %func common_read32 as function with args virtio_hw_t *, u32
$define %func common_write32 as procedure with args virtio_hw_t *, u32, u32
$define %func common_read16 as function with args virtio_hw_t *, u32
$define %func common_write16 as procedure with args virtio_hw_t *, u32, u16
$define %func common_read8 as function with args virtio_hw_t *, u32
$define %func common_write8 as procedure with args virtio_hw_t *, u32, u8
$define %func common_write64 as procedure with args virtio_hw_t *, u32, u64
$define %func virtio_hw_init as function with args virtio_hw_t *, pci_device_t *
$define %func virtio_hw_shutdown as procedure with args virtio_hw_t *
$define %func virtio_hw_set_status as procedure with args virtio_hw_t *, u8
$define %func virtio_hw_get_status as function with args virtio_hw_t *
$define %func virtio_hw_get_features as function with args virtio_hw_t *
$define %func virtio_hw_get_features_hi as function with args virtio_hw_t *
$define %func virtio_hw_set_features as procedure with args virtio_hw_t *, u32
$define %func virtio_hw_set_features_hi as procedure with args virtio_hw_t *, u32
$define %func virtio_hw_get_num_queues as function with args virtio_hw_t *
$define %func virtio_hw_select_queue as procedure with args virtio_hw_t *, u16
$define %func virtio_hw_get_queue_size as function with args virtio_hw_t *
$define %func virtio_hw_set_queue_size as procedure with args virtio_hw_t *, u16
$define %func virtio_hw_set_queue_desc as procedure with args virtio_hw_t *, u64
$define %func virtio_hw_set_queue_driver as procedure with args virtio_hw_t *, u64
$define %func virtio_hw_set_queue_device as procedure with args virtio_hw_t *, u64
$define %func virtio_hw_get_queue_notify_off as function with args virtio_hw_t *
$define %func virtio_hw_enable_queue as procedure with args virtio_hw_t *
$define %func virtio_hw_notify_queue as procedure with args virtio_hw_t *, u16
$define %func virtio_hw_read_isr as function with args virtio_hw_t *
$define %func virtio_hw_read_gpu_config as procedure with args virtio_hw_t *, virtio_gpu_config_t *

*/

/* !SPACE!

$space %internal mmio_remap, mmio_read8, mmio_read16, mmio_read32
$space %internal mmio_read64, mmio_write8, mmio_write16, mmio_write32
$space %internal mmio_write64, io_read8, io_read16, io_read32
$space %internal io_write8, io_write16, io_write32
$space %internal parse_capability, walk_capabilities, legacy_init
$space %internal common_reg, common_read32, common_write32
$space %internal common_read16, common_write16, common_read8
$space %internal common_write8, common_write64
$space %export virtio_hw_init, virtio_hw_shutdown
$space %export virtio_hw_set_status, virtio_hw_get_status
$space %export virtio_hw_get_features, virtio_hw_set_features
$space %export virtio_hw_get_features_hi, virtio_hw_set_features_hi
$space %export virtio_hw_get_num_queues
$space %export virtio_hw_select_queue, virtio_hw_get_queue_size
$space %export virtio_hw_set_queue_size
$space %export virtio_hw_set_queue_desc, virtio_hw_set_queue_driver
$space %export virtio_hw_set_queue_device
$space %export virtio_hw_get_queue_notify_off
$space %export virtio_hw_enable_queue, virtio_hw_notify_queue
$space %export virtio_hw_read_isr, virtio_hw_read_gpu_config

*/

#include <kernel/drivers/virtio/virtio_hw.h>
#include <kernel/pci/utils/bar.h>
#include <kernel/pci/utils/io.h>
#include <kernel/mm/vm/pmap.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>

static u64
mmio_remap(u64 phys_base, u64 size)
{
	u64	phys_start, phys_end, vaddr, p;

	if (size == 0) {
		size = PAGE_SIZE;
	}
	phys_start = phys_base & ~((u64)PAGE_SIZE - 1);
	phys_end = (phys_base + size + PAGE_SIZE - 1) &
	    ~((u64)PAGE_SIZE - 1);
	vaddr = MMIO_VBASE + phys_start;
	for (p = phys_start; p < phys_end;
	    p += PAGE_SIZE, vaddr += PAGE_SIZE) {
		pmap_enter(vaddr, p, PTE_RW | PTE_PCD | PTE_PWT);
	}
	return (MMIO_VBASE + phys_base);
}

static u8
mmio_read8(volatile void *addr)
{
	return (*(volatile u8 *)addr);
}

static u16
mmio_read16(volatile void *addr)
{
	return (*(volatile u16 *)addr);
}

static u32
mmio_read32(volatile void *addr)
{
	return (*(volatile u32 *)addr);
}

static u64
mmio_read64(volatile void *addr)
{
	return (*(volatile u64 *)addr);
}

static void
mmio_write8(volatile void *addr, u8 val)
{
	*(volatile u8 *)addr = val;
}

static void
mmio_write16(volatile void *addr, u16 val)
{
	*(volatile u16 *)addr = val;
}

static void
mmio_write32(volatile void *addr, u32 val)
{
	*(volatile u32 *)addr = val;
}

static void
mmio_write64(volatile void *addr, u64 val)
{
	*(volatile u64 *)addr = val;
}

static u8
io_read8(u64 base, u32 offset)
{
	return (inb((u16)(base + offset)));
}

static u16
io_read16(u64 base, u32 offset)
{
	return (inw((u16)(base + offset)));
}

static u32
io_read32(u64 base, u32 offset)
{
	u32	value;

	__asm__ volatile("inl %1, %0"
	    : "=a"(value)
	    : "Nd"((u16)(base + offset)));
	return (value);
}

static void
io_write8(u64 base, u32 offset, u8 val)
{
	outb((u16)(base + offset), val);
}

static void
io_write16(u64 base, u32 offset, u16 val)
{
	outw((u16)(base + offset), val);
}

static void
io_write32(u64 base, u32 offset, u32 val)
{
	__asm__ volatile("outl %0, %1"
	    : : "a"(val), "Nd"((u16)(base + offset)));
}

static void
parse_capability(virtio_hw_t *hw, u8 cfg_type, u8 bar,
    u32 offset, u32 length, u32 notify_multiplier)
{
	pci_bar_t	bar_info;
	u64		struct_phys, vaddr;

	if (pci_read_bar(hw->pci_dev, bar, &bar_info) != 0 ||
	    bar_info.base == 0 || length == 0 || offset > bar_info.size ||
	    length > bar_info.size - offset) {
		drivers_log("[VIRTIO] cap type %u: BAR %u "
		    "unreadable\n", cfg_type, bar);
		return;
	}

	struct_phys = bar_info.base + offset;

	switch (cfg_type) {
	case VIRTIO_PCI_CAP_COMMON_CFG:
		hw->common_is_io = bar_info.is_io;
		if (!bar_info.is_io) {
			vaddr = mmio_remap(struct_phys, length);
			hw->common_base = vaddr - offset;
			hw->common_mmio =
			    (virtio_pci_common_cfg_t *)vaddr;
		} else {
			hw->common_base = bar_info.base;
		}
		hw->common_offset = offset;
		hw->common_length = length;
		drivers_log("[VIRTIO] common cfg: %s %p+0x%x\n",
		    bar_info.is_io ? "IO" : "MMIO",
		    (void *)(bar_info.is_io ? struct_phys :
		    (MMIO_VBASE + struct_phys)), length);
		break;

	case VIRTIO_PCI_CAP_NOTIFY_CFG:
		hw->notify_is_io = bar_info.is_io;
		if (!bar_info.is_io) {
			vaddr = mmio_remap(struct_phys, length);
			hw->notify_base = vaddr - offset;
		} else {
			hw->notify_base = bar_info.base;
		}
		hw->notify_offset = offset;
		hw->notify_multiplier = notify_multiplier;
		drivers_log("[VIRTIO] notify: %s %p+0x%x "
		    "(mult=%u)\n",
		    bar_info.is_io ? "IO" : "MMIO",
		    (void *)(bar_info.is_io ? struct_phys :
		    (MMIO_VBASE + struct_phys)), length,
		    notify_multiplier);
		break;

	case VIRTIO_PCI_CAP_ISR_CFG:
		hw->isr_is_io = bar_info.is_io;
		if (!bar_info.is_io) {
			vaddr = mmio_remap(struct_phys, length);
			hw->isr_base = vaddr - offset;
		} else {
			hw->isr_base = bar_info.base;
		}
		hw->isr_offset = offset;
		drivers_log("[VIRTIO] isr: %s %p+0x%x\n",
		    bar_info.is_io ? "IO" : "MMIO",
		    (void *)(bar_info.is_io ? struct_phys :
		    (MMIO_VBASE + struct_phys)), length);
		break;

	case VIRTIO_PCI_CAP_DEVICE_CFG:
		hw->dev_is_io = bar_info.is_io;
		if (!bar_info.is_io) {
			vaddr = mmio_remap(struct_phys, length);
			hw->dev_base = vaddr - offset;
			hw->dev_mmio =
			    (virtio_gpu_config_t *)vaddr;
		} else {
			hw->dev_base = bar_info.base;
		}
		hw->dev_offset = offset;
		hw->dev_length = length;
		drivers_log("[VIRTIO] device cfg: %s %p+0x%x\n",
		    bar_info.is_io ? "IO" : "MMIO",
		    (void *)(bar_info.is_io ? struct_phys :
		    (MMIO_VBASE + struct_phys)), length);
		break;

	default:
		break;
	}
}

static void
walk_capabilities(virtio_hw_t *hw)
{
	u8	cap_ptr, cap_vndr, cap_next, cap_len;
	u8	cfg_type, bar;
	u32	offset, length, notify_mult;
	int	count;

	cap_ptr = pci_cfg_read8(hw->pci_dev->bus,
	    hw->pci_dev->slot, hw->pci_dev->function,
	    PCI_CFG_CAPABILITIES);

	count = 0;
	while (cap_ptr >= 0x40 && (cap_ptr & 3) == 0 &&
	    count++ < 48) {
		cap_vndr = pci_cfg_read8(hw->pci_dev->bus,
		    hw->pci_dev->slot, hw->pci_dev->function,
		    cap_ptr);
		cap_next = pci_cfg_read8(hw->pci_dev->bus,
		    hw->pci_dev->slot, hw->pci_dev->function,
		    cap_ptr + 1);
		cap_len = pci_cfg_read8(hw->pci_dev->bus,
		    hw->pci_dev->slot, hw->pci_dev->function,
		    cap_ptr + 2);

		if (cap_vndr == PCI_CAP_ID_VNDR && cap_len >= 16) {
			cfg_type = pci_cfg_read8(hw->pci_dev->bus,
			    hw->pci_dev->slot,
			    hw->pci_dev->function, cap_ptr + 3);
			bar = pci_cfg_read8(hw->pci_dev->bus,
			    hw->pci_dev->slot,
			    hw->pci_dev->function, cap_ptr + 4);
			offset = pci_cfg_read32(hw->pci_dev->bus,
			    hw->pci_dev->slot,
			    hw->pci_dev->function, cap_ptr + 8);
			length = pci_cfg_read32(hw->pci_dev->bus,
			    hw->pci_dev->slot,
			    hw->pci_dev->function, cap_ptr + 12);

			notify_mult = 0;
			if (cfg_type ==
			    VIRTIO_PCI_CAP_NOTIFY_CFG &&
			    cap_len >= 20) {
				notify_mult =
				    pci_cfg_read32(hw->pci_dev->bus,
				    hw->pci_dev->slot,
				    hw->pci_dev->function,
				    cap_ptr + 16);
			}

			parse_capability(hw, cfg_type, bar,
			    offset, length, notify_mult);
		}

		cap_ptr = cap_next;
	}
	if (count >= 48) {
		drivers_log("[VIRTIO] malformed capability list\n");
	}
}

static volatile void *
common_reg(virtio_hw_t *hw, u32 field_offset)
{
	return ((volatile void *)(u64)(hw->common_base +
	    hw->common_offset + field_offset));
}

static u32
common_read32(virtio_hw_t *hw, u32 off)
{
	if (hw->common_is_io) {
		return (io_read32(hw->common_base,
		    hw->common_offset + off));
	}
	return (mmio_read32(common_reg(hw, off)));
}

static void
common_write32(virtio_hw_t *hw, u32 off, u32 val)
{
	if (hw->common_is_io) {
		io_write32(hw->common_base,
		    hw->common_offset + off, val);
	} else {
		mmio_write32(common_reg(hw, off), val);
	}
}

static u16
common_read16(virtio_hw_t *hw, u32 off)
{
	if (hw->common_is_io) {
		return (io_read16(hw->common_base,
		    hw->common_offset + off));
	}
	return (mmio_read16(common_reg(hw, off)));
}

static void
common_write16(virtio_hw_t *hw, u32 off, u16 val)
{
	if (hw->common_is_io) {
		io_write16(hw->common_base,
		    hw->common_offset + off, val);
	} else {
		mmio_write16(common_reg(hw, off), val);
	}
}

static u8
common_read8(virtio_hw_t *hw, u32 off)
{
	if (hw->common_is_io) {
		return (io_read8(hw->common_base,
		    hw->common_offset + off));
	}
	return (mmio_read8(common_reg(hw, off)));
}

static void
common_write8(virtio_hw_t *hw, u32 off, u8 val)
{
	if (hw->common_is_io) {
		io_write8(hw->common_base,
		    hw->common_offset + off, val);
	} else {
		mmio_write8(common_reg(hw, off), val);
	}
}

static void
common_write64(virtio_hw_t *hw, u32 off, u64 val)
{
	common_write32(hw, off, (u32)val);
	common_write32(hw, off + 4, (u32)(val >> 32));
}

#define	OFF_DEVICE_FEATURE_SELECT	0
#define	OFF_DEVICE_FEATURE		4
#define	OFF_DRIVER_FEATURE_SELECT	8
#define	OFF_DRIVER_FEATURE		12
#define	OFF_CONFIG_MSIX_VECTOR		16
#define	OFF_NUM_QUEUES			18
#define	OFF_DEVICE_STATUS		20
#define	OFF_CONFIG_GENERATION		21
#define	OFF_QUEUE_SELECT		22
#define	OFF_QUEUE_SIZE			24
#define	OFF_QUEUE_MSIX_VECTOR		26
#define	OFF_QUEUE_ENABLE		28
#define	OFF_QUEUE_NOTIFY_OFF		30
#define	OFF_QUEUE_DESC			32
#define	OFF_QUEUE_DRIVER		40
#define	OFF_QUEUE_DEVICE		48

static int
legacy_init(virtio_hw_t *hw)
{
	pci_bar_t	bar;

	if (hw->pci_dev->device_id < VIRTIO_PCI_LEGACY_DEVICE_MIN ||
	    hw->pci_dev->device_id > VIRTIO_PCI_LEGACY_DEVICE_MAX ||
	    pci_read_bar(hw->pci_dev, 0, &bar) != 0 || !bar.is_io ||
	    bar.base == 0 || bar.size < VIRTIO_PCI_LEGACY_DEVICE_CONFIG) {
		return (-1);
	}

	hw->transport = VIRTIO_TRANSPORT_LEGACY;
	hw->legacy_io_base = bar.base;
	hw->dev_is_io = 1;
	hw->dev_base = bar.base;
	hw->dev_offset = VIRTIO_PCI_LEGACY_DEVICE_CONFIG;
	hw->dev_length = (u32)(bar.size - VIRTIO_PCI_LEGACY_DEVICE_CONFIG);

	drivers_log("[VIRTIO] legacy PCI I/O transport at %p\n",
	    (void *)bar.base);
	return (0);
}

void
virtio_dma_tags_destroy(virtio_hw_t *hw)
{
	if (hw == NULL) {
		return;
	}
	if (hw->buf_tag != NULL) {
		dma_tag_destroy(hw->buf_tag);
		hw->buf_tag = NULL;
	}
	if (hw->ring_tag != NULL) {
		dma_tag_destroy(hw->ring_tag);
		hw->ring_tag = NULL;
	}
	if (hw->tag != NULL) {
		dma_tag_destroy(hw->tag);
		hw->tag = NULL;
	}
}

static int
virtio_dma_tags_create(virtio_hw_t *hw)
{
	u64	ring_max;

	ring_max = (hw->transport == VIRTIO_TRANSPORT_LEGACY) ?
	    VIRTIO_LEGACY_RING_MAX : DMA_HIGHADDR_ANY;
	if (dma_tag_create(dma_tag_root(), 1, DMA_BOUNDARY_NONE, 0,
	    DMA_HIGHADDR_ANY, 0, 1, DMA_SEGSZ_MAX, 0, "virtio",
	    &hw->tag) != 0) {
		return (-1);
	}

	if (dma_tag_create(hw->tag, PAGE_SIZE, DMA_BOUNDARY_NONE, 0, ring_max, 0,
	    1, DMA_SEGSZ_MAX, 0, "virtio-ring", &hw->ring_tag) != 0) {
		goto fail;
	}
	if (dma_tag_create(hw->tag, PAGE_SIZE, DMA_BOUNDARY_NONE, 0,
	    DMA_HIGHADDR_ANY, 0, 1, DMA_SEGSZ_MAX, 0, "virtio-buf",
	    &hw->buf_tag) != 0) {
		goto fail;
	}
	return (0);
fail:
	virtio_dma_tags_destroy(hw);
	return (-1);
}

int
virtio_hw_init(virtio_hw_t *hw, pci_device_t *dev)
{

	if (!hw || !dev) {
		return (-1);
	}

	memset(hw, 0, sizeof(*hw));
	hw->pci_dev = dev;

	pci_enable_memory_space(dev);
	pci_enable_bus_mastering(dev);
	pci_enable_io_space(dev);

	walk_capabilities(hw);

	if (hw->common_base == 0 || hw->notify_base == 0 ||
	    hw->isr_base == 0 || hw->dev_base == 0) {
		if (legacy_init(hw) != 0) {
			drivers_log("[VIRTIO] missing required "
			    "capabilities\n");
			return (-1);
		}
	}

	if (virtio_dma_tags_create(hw) != 0) {
		drivers_log("[VIRTIO] DMA tag setup failed\n");
		return (-1);
	}

	virtio_hw_set_status(hw, VIRTIO_STATUS_RESET);
	virtio_hw_set_status(hw, VIRTIO_STATUS_ACKNOWLEDGE);
	virtio_hw_set_status(hw, VIRTIO_STATUS_ACKNOWLEDGE |
	    VIRTIO_STATUS_DRIVER);

	drivers_log("[VIRTIO] %s transport discovered\n",
	    hw->transport == VIRTIO_TRANSPORT_LEGACY ? "legacy" : "modern");
	return (0);
}

void
virtio_hw_shutdown(virtio_hw_t *hw)
{
	if (!hw || (hw->common_base == 0 && hw->legacy_io_base == 0)) {
		return;
	}
	virtio_hw_set_status(hw, VIRTIO_STATUS_RESET);
	hw->ready = 0;
	virtio_dma_tags_destroy(hw);
}

void
virtio_hw_set_status(virtio_hw_t *hw, u8 status)
{
	if (hw->transport == VIRTIO_TRANSPORT_LEGACY) {
		status &= ~VIRTIO_STATUS_FEATURES_OK;
		io_write8(hw->legacy_io_base,
		    VIRTIO_PCI_LEGACY_STATUS, status);
		return;
	}
	common_write8(hw, OFF_DEVICE_STATUS, status);
}

u8
virtio_hw_get_status(virtio_hw_t *hw)
{
	if (hw->transport == VIRTIO_TRANSPORT_LEGACY) {
		return (io_read8(hw->legacy_io_base,
		    VIRTIO_PCI_LEGACY_STATUS));
	}
	return (common_read8(hw, OFF_DEVICE_STATUS));
}

u32
virtio_hw_get_features(virtio_hw_t *hw)
{
	if (hw->transport == VIRTIO_TRANSPORT_LEGACY) {
		return (io_read32(hw->legacy_io_base,
		    VIRTIO_PCI_LEGACY_HOST_FEATURES));
	}
	common_write32(hw, OFF_DEVICE_FEATURE_SELECT, 0);
	return (common_read32(hw, OFF_DEVICE_FEATURE));
}

u32
virtio_hw_get_features_hi(virtio_hw_t *hw)
{
	if (hw->transport == VIRTIO_TRANSPORT_LEGACY) {
		return (0);
	}
	common_write32(hw, OFF_DEVICE_FEATURE_SELECT, 1);
	return (common_read32(hw, OFF_DEVICE_FEATURE));
}

void
virtio_hw_set_features(virtio_hw_t *hw, u32 features)
{
	if (hw->transport == VIRTIO_TRANSPORT_LEGACY) {
		io_write32(hw->legacy_io_base,
		    VIRTIO_PCI_LEGACY_GUEST_FEATURES, features);
		return;
	}
	common_write32(hw, OFF_DRIVER_FEATURE_SELECT, 0);
	common_write32(hw, OFF_DRIVER_FEATURE, features);
}

void
virtio_hw_set_features_hi(virtio_hw_t *hw, u32 features)
{
	if (hw->transport == VIRTIO_TRANSPORT_LEGACY) {
		return;
	}
	common_write32(hw, OFF_DRIVER_FEATURE_SELECT, 1);
	common_write32(hw, OFF_DRIVER_FEATURE, features);
}

u16
virtio_hw_get_num_queues(virtio_hw_t *hw)
{
	if (hw->transport == VIRTIO_TRANSPORT_LEGACY) {
		return (0);
	}
	return (common_read16(hw, OFF_NUM_QUEUES));
}

void
virtio_hw_select_queue(virtio_hw_t *hw, u16 index)
{
	if (hw->transport == VIRTIO_TRANSPORT_LEGACY) {
		io_write16(hw->legacy_io_base,
		    VIRTIO_PCI_LEGACY_QUEUE_SELECT, index);
		return;
	}
	common_write16(hw, OFF_QUEUE_SELECT, index);
}

u16
virtio_hw_get_queue_size(virtio_hw_t *hw)
{
	if (hw->transport == VIRTIO_TRANSPORT_LEGACY) {
		return (io_read16(hw->legacy_io_base,
		    VIRTIO_PCI_LEGACY_QUEUE_SIZE));
	}
	return (common_read16(hw, OFF_QUEUE_SIZE));
}

void
virtio_hw_set_queue_size(virtio_hw_t *hw, u16 size)
{
	if (hw->transport == VIRTIO_TRANSPORT_LEGACY) {
		return;
	}
	common_write16(hw, OFF_QUEUE_SIZE, size);
}

void
virtio_hw_set_queue_desc(virtio_hw_t *hw, u64 addr)
{
	if (hw->transport == VIRTIO_TRANSPORT_LEGACY) {
		hw->legacy_queue_desc = addr;
		return;
	}
	common_write64(hw, OFF_QUEUE_DESC, addr);
}

void
virtio_hw_set_queue_driver(virtio_hw_t *hw, u64 addr)
{
	if (hw->transport == VIRTIO_TRANSPORT_LEGACY) {
		return;
	}
	common_write64(hw, OFF_QUEUE_DRIVER, addr);
}

void
virtio_hw_set_queue_device(virtio_hw_t *hw, u64 addr)
{
	if (hw->transport == VIRTIO_TRANSPORT_LEGACY) {
		return;
	}
	common_write64(hw, OFF_QUEUE_DEVICE, addr);
}

u16
virtio_hw_get_queue_notify_off(virtio_hw_t *hw)
{
	if (hw->transport == VIRTIO_TRANSPORT_LEGACY) {
		return (0);
	}
	return (common_read16(hw, OFF_QUEUE_NOTIFY_OFF));
}

void
virtio_hw_enable_queue(virtio_hw_t *hw)
{
	if (hw->transport == VIRTIO_TRANSPORT_LEGACY) {
		io_write32(hw->legacy_io_base,
		    VIRTIO_PCI_LEGACY_QUEUE_PFN,
		    (u32)(hw->legacy_queue_desc >> 12));
		return;
	}
	common_write16(hw, OFF_QUEUE_ENABLE, 1);
}

void
virtio_hw_notify_queue(virtio_hw_t *hw, u16 queue_index)
{
	u16	notify_off;
	u64	addr;

	if (hw->transport == VIRTIO_TRANSPORT_LEGACY) {
		io_write16(hw->legacy_io_base,
		    VIRTIO_PCI_LEGACY_QUEUE_NOTIFY, queue_index);
		return;
	}

	virtio_hw_select_queue(hw, queue_index);
	notify_off = virtio_hw_get_queue_notify_off(hw);

	addr = hw->notify_base + hw->notify_offset +
	    (u64)notify_off * hw->notify_multiplier;

	if (hw->notify_is_io) {
		io_write16(addr, 0, queue_index);
	} else {
		mmio_write16((volatile void *)(u64)addr,
		    queue_index);
	}
}

u8
virtio_hw_read_isr(virtio_hw_t *hw)
{
	u64	addr;

	if (hw->transport == VIRTIO_TRANSPORT_LEGACY) {
		return (io_read8(hw->legacy_io_base,
		    VIRTIO_PCI_LEGACY_ISR));
	}

	addr = hw->isr_base + hw->isr_offset;
	if (hw->isr_is_io) {
		return (io_read8(addr, 0));
	}
	return (mmio_read8((volatile void *)(u64)addr));
}

u8
virtio_hw_get_config_generation(virtio_hw_t *hw)
{
	if (hw->transport == VIRTIO_TRANSPORT_LEGACY) {
		return (0);
	}
	return (common_read8(hw, OFF_CONFIG_GENERATION));
}

int
virtio_hw_read_device_config(virtio_hw_t *hw, u32 offset, void *buf,
    u32 len)
{
	u8	*dst;
	u32	i;

	if (!hw || !buf || len == 0 || offset > hw->dev_length ||
	    len > hw->dev_length - offset) {
		return (-1);
	}

	dst = buf;
	for (i = 0; i < len; i++) {
		if (hw->dev_is_io) {
			dst[i] = io_read8(hw->dev_base,
			    hw->dev_offset + offset + i);
		} else {
			dst[i] = mmio_read8((volatile void *)(u64)
			    (hw->dev_base + hw->dev_offset + offset + i));
		}
	}
	return (0);
}

void
virtio_hw_read_gpu_config(virtio_hw_t *hw, virtio_gpu_config_t *out)
{
	if (!out || virtio_hw_read_device_config(hw, 0, out,
	    sizeof(*out)) != 0) {
		if (out) {
			memset(out, 0, sizeof(*out));
		}
	}
}
