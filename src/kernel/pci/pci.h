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

#ifndef PCI_H
#define PCI_H

#include <mlibc/mlibc.h>

#define PCI_MAX_DEVICES 256
#define PCI_MAX_DRIVERS 64

#define PCI_ANY_ID 0xFFFF
#define PCI_VENDOR_INVALID 0xFFFF
#define PCI_ANY_CLASS 0xFF
#define PCI_ANY_SUBCLASS 0xFF
#define PCI_ANY_PROGIF 0xFF

#define PCI_CFG_VENDOR_ID 0x00
#define PCI_CFG_DEVICE_ID 0x02
#define PCI_CFG_COMMAND 0x04
#define PCI_CFG_STATUS 0x06
#define PCI_CFG_REVISION_ID 0x08
#define PCI_CFG_PROG_IF 0x09
#define PCI_CFG_SUBCLASS 0x0A
#define PCI_CFG_CLASS_CODE 0x0B
#define PCI_CFG_CACHE_LINE_SIZE 0x0C
#define PCI_CFG_LATENCY_TIMER 0x0D
#define PCI_CFG_HEADER_TYPE 0x0E
#define PCI_CFG_BIST 0x0F
#define PCI_CFG_BAR0 0x10
#define PCI_CFG_SUBSYSTEM_VENDOR_ID 0x2C
#define PCI_CFG_SUBSYSTEM_ID 0x2E
#define PCI_CFG_IRQ_LINE 0x3C
#define PCI_CFG_IRQ_PIN 0x3D

#define PCI_HEADER_TYPE_MULTI_FUNCTION 0x80

#define PCI_CLASS_BRIDGE 0x06
#define PCI_SUBCLASS_PCI_TO_PCI 0x04

#define PCI_COMMAND_IO 0x1
#define PCI_COMMAND_MEMORY 0x2
#define PCI_COMMAND_BUS_MASTER 0x4

#define PCI_BAR_IO_SPACE 0x1
#define PCI_BAR_MEM_TYPE_64 0x2
#define PCI_BAR_MEM_PREFETCH 0x8
#define PCI_BAR_IO_MASK 0xFFFFFFFC
#define PCI_BAR_MEM_MASK 0xFFFFFFF0

typedef struct pci_match {
  u16 vendor_id;
  u16 device_id;
  u8 class_code;
  u8 subclass;
  u8 prog_if;
} pci_match_t;

typedef struct pci_driver pci_driver_t;

typedef struct pci_device {
  u8 bus;
  u8 slot;
  u8 function;

  u16 vendor_id;
  u16 device_id;

  u8 class_code;
  u8 subclass;
  u8 prog_if;
  u8 revision_id;

  u8 header_type;
  u8 cache_line_size;
  u8 latency_timer;

  u16 subsystem_vendor_id;
  u16 subsystem_id;

  u8 irq_line;
  u8 irq_pin;

  pci_driver_t *driver;
  void *driver_data;
} pci_device_t;

typedef struct pci_driver {
  const char *name;
  const pci_match_t *matches;
  u32 match_count;
  int (*probe)(pci_device_t *dev, const pci_match_t *match);
  void (*remove)(pci_device_t *dev);
} pci_driver_t;

typedef struct pci_bar {
  u64 base;
  u64 size;
  u8 index;
  u8 is_io;
  u8 is_64;
  u8 prefetchable;
} pci_bar_t;

void pci_init(void);
int pci_is_initialized(void);

int pci_register_driver(pci_driver_t *driver);
int pci_unregister_driver(pci_driver_t *driver);

int pci_scan(void);
int pci_device_count(void);
pci_device_t *pci_get_device(int index);
void pci_set_verbose_scan(int enabled);

u32 pci_cfg_read32(u8 bus, u8 slot, u8 function, u8 offset);
u16 pci_cfg_read16(u8 bus, u8 slot, u8 function, u8 offset);
u8 pci_cfg_read8(u8 bus, u8 slot, u8 function, u8 offset);
void pci_cfg_write32(u8 bus, u8 slot, u8 function, u8 offset, u32 value);
void pci_cfg_write16(u8 bus, u8 slot, u8 function, u8 offset, u16 value);
void pci_cfg_write8(u8 bus, u8 slot, u8 function, u8 offset, u8 value);

u16 pci_read_command(const pci_device_t *dev);
void pci_write_command(const pci_device_t *dev, u16 command);
void pci_enable_io_space(const pci_device_t *dev);
void pci_enable_memory_space(const pci_device_t *dev);
void pci_enable_bus_mastering(const pci_device_t *dev);

#endif
