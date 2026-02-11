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

#include <kernel/pci/pci.h>
#include <kernel/pci/utils/io.h>

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA 0xCFC

static u32 pci_make_config_address(u8 bus, u8 slot, u8 function, u8 offset) {
  u32 aligned = (u32)(offset & 0xFC);
  return 0x80000000U | ((u32)bus << 16) | ((u32)slot << 11) |
         ((u32)function << 8) | aligned;
}

u32 pci_cfg_read32(u8 bus, u8 slot, u8 function, u8 offset) {
  u32 address = pci_make_config_address(bus, slot, function, offset);
  pci_outl(PCI_CONFIG_ADDRESS, address);
  return pci_inl(PCI_CONFIG_DATA);
}

u16 pci_cfg_read16(u8 bus, u8 slot, u8 function, u8 offset) {
  u32 value = pci_cfg_read32(bus, slot, function, offset);
  u32 shift = (offset & 2) * 8;
  return (u16)((value >> shift) & 0xFFFF);
}

u8 pci_cfg_read8(u8 bus, u8 slot, u8 function, u8 offset) {
  u32 value = pci_cfg_read32(bus, slot, function, offset);
  u32 shift = (offset & 3) * 8;
  return (u8)((value >> shift) & 0xFF);
}

void pci_cfg_write32(u8 bus, u8 slot, u8 function, u8 offset, u32 value) {
  u32 address = pci_make_config_address(bus, slot, function, offset);
  pci_outl(PCI_CONFIG_ADDRESS, address);
  pci_outl(PCI_CONFIG_DATA, value);
}

void pci_cfg_write16(u8 bus, u8 slot, u8 function, u8 offset, u16 value) {
  u32 current = pci_cfg_read32(bus, slot, function, offset);
  u32 shift = (offset & 2) * 8;
  u32 mask = 0xFFFFU << shift;
  u32 next = (current & ~mask) | ((u32)value << shift);
  pci_cfg_write32(bus, slot, function, offset, next);
}

void pci_cfg_write8(u8 bus, u8 slot, u8 function, u8 offset, u8 value) {
  u32 current = pci_cfg_read32(bus, slot, function, offset);
  u32 shift = (offset & 3) * 8;
  u32 mask = 0xFFU << shift;
  u32 next = (current & ~mask) | ((u32)value << shift);
  pci_cfg_write32(bus, slot, function, offset, next);
}
