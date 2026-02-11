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

#include <kernel/pci/utils/bar.h>

static int pci_bar_count_from_header(u8 header_type) {
  header_type &= 0x7F;
  if (header_type == 0x00) {
    return 6;
  }
  if (header_type == 0x01) {
    return 2;
  }
  return 0;
}

static void pci_bar_clear(pci_bar_t *bar, u8 index) {
  bar->base = 0;
  bar->size = 0;
  bar->index = index;
  bar->is_io = 0;
  bar->is_64 = 0;
  bar->prefetchable = 0;
}

int pci_get_bar_count(const pci_device_t *dev) {
  if (!dev) {
    return 0;
  }
  return pci_bar_count_from_header(dev->header_type);
}

int pci_read_bar(const pci_device_t *dev, u8 index, pci_bar_t *out) {
  if (!dev || !out) {
    return -1;
  }

  int max_bars = pci_bar_count_from_header(dev->header_type);
  if (index >= (u8)max_bars) {
    return -1;
  }

  pci_bar_clear(out, index);

  u8 offset = PCI_CFG_BAR0 + (index * 4);
  u32 original_low = pci_cfg_read32(dev->bus, dev->slot, dev->function, offset);

  if (original_low == 0) {
    return 0;
  }

  if (original_low & PCI_BAR_IO_SPACE) {
    out->is_io = 1;
    out->base = (u64)(original_low & PCI_BAR_IO_MASK);

    pci_cfg_write32(dev->bus, dev->slot, dev->function, offset, 0xFFFFFFFF);
    u32 mask_low = pci_cfg_read32(dev->bus, dev->slot, dev->function, offset);
    pci_cfg_write32(dev->bus, dev->slot, dev->function, offset, original_low);

    u32 size_mask = mask_low & PCI_BAR_IO_MASK;
    if (size_mask) {
      out->size = (u64)(~size_mask + 1);
    }

    return 0;
  }

  out->prefetchable = (original_low & PCI_BAR_MEM_PREFETCH) ? 1 : 0;

  u32 mem_type = (original_low >> 1) & 0x3;
  if (mem_type == PCI_BAR_MEM_TYPE_64) {
    if (index + 1 >= (u8)max_bars) {
      return -1;
    }

    u32 original_high = pci_cfg_read32(dev->bus, dev->slot, dev->function,
                                       offset + 4);

    pci_cfg_write32(dev->bus, dev->slot, dev->function, offset, 0xFFFFFFFF);
    pci_cfg_write32(dev->bus, dev->slot, dev->function, offset + 4, 0xFFFFFFFF);
    u32 mask_low = pci_cfg_read32(dev->bus, dev->slot, dev->function, offset);
    u32 mask_high =
        pci_cfg_read32(dev->bus, dev->slot, dev->function, offset + 4);
    pci_cfg_write32(dev->bus, dev->slot, dev->function, offset, original_low);
    pci_cfg_write32(dev->bus, dev->slot, dev->function, offset + 4,
                    original_high);

    u64 base = ((u64)original_high << 32) | (original_low & PCI_BAR_MEM_MASK);
    u64 mask = ((u64)mask_high << 32) | (mask_low & PCI_BAR_MEM_MASK);

    out->is_64 = 1;
    out->base = base;
    if (mask) {
      out->size = (~mask + 1);
    }

    return 0;
  }

  out->base = (u64)(original_low & PCI_BAR_MEM_MASK);

  pci_cfg_write32(dev->bus, dev->slot, dev->function, offset, 0xFFFFFFFF);
  u32 mask_low = pci_cfg_read32(dev->bus, dev->slot, dev->function, offset);
  pci_cfg_write32(dev->bus, dev->slot, dev->function, offset, original_low);

  u32 size_mask = mask_low & PCI_BAR_MEM_MASK;
  if (size_mask) {
    out->size = (u64)(~size_mask + 1);
  }

  return 0;
}
