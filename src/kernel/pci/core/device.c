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

u16 pci_read_command(const pci_device_t *dev) {
  if (!dev) {
    return 0;
  }
  return pci_cfg_read16(dev->bus, dev->slot, dev->function, PCI_CFG_COMMAND);
}

void pci_write_command(const pci_device_t *dev, u16 command) {
  if (!dev) {
    return;
  }
  pci_cfg_write16(dev->bus, dev->slot, dev->function, PCI_CFG_COMMAND, command);
}

void pci_enable_io_space(const pci_device_t *dev) {
  u16 command = pci_read_command(dev);
  command |= PCI_COMMAND_IO;
  pci_write_command(dev, command);
}

void pci_enable_memory_space(const pci_device_t *dev) {
  u16 command = pci_read_command(dev);
  command |= PCI_COMMAND_MEMORY;
  pci_write_command(dev, command);
}

void pci_enable_bus_mastering(const pci_device_t *dev) {
  u16 command = pci_read_command(dev);
  command |= PCI_COMMAND_BUS_MASTER;
  pci_write_command(dev, command);
}
