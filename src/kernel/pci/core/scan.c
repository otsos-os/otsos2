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
#include <kernel/pci/utils/bar.h>
#include <lib/com1.h>

typedef struct pci_class_info {
  u8 class_code;
  u8 subclass;
  u8 prog_if;
  const char *class_name;
  const char *subclass_name;
} pci_class_info_t;

static const pci_class_info_t pci_class_table[] = {
    {0x01, 0x00, PCI_ANY_PROGIF, "Storage", "SCSI"},
    {0x01, 0x01, PCI_ANY_PROGIF, "Storage", "IDE"},
    {0x01, 0x06, PCI_ANY_PROGIF, "Storage", "SATA"},
    {0x02, 0x00, PCI_ANY_PROGIF, "Network", "Ethernet"},
    {0x03, 0x00, PCI_ANY_PROGIF, "Display", "VGA"},
    {0x03, 0x02, PCI_ANY_PROGIF, "Display", "3D"},
    {0x06, 0x00, PCI_ANY_PROGIF, "Bridge", "Host"},
    {0x06, 0x01, PCI_ANY_PROGIF, "Bridge", "ISA"},
    {0x06, 0x04, PCI_ANY_PROGIF, "Bridge", "PCI-to-PCI"},
};

static void pci_lookup_class(const pci_device_t *dev, const char **class_name,
                             const char **subclass_name) {
  *class_name = "Unknown";
  *subclass_name = "Unknown";

  for (u32 i = 0; i < sizeof(pci_class_table) / sizeof(pci_class_table[0]);
       i++) {
    const pci_class_info_t *info = &pci_class_table[i];
    if (info->class_code != dev->class_code) {
      continue;
    }
    if (info->subclass != dev->subclass) {
      continue;
    }
    if (info->prog_if != PCI_ANY_PROGIF && info->prog_if != dev->prog_if) {
      continue;
    }
    *class_name = info->class_name;
    *subclass_name = info->subclass_name;
    return;
  }
}

static void pci_print_size(u64 size) {
  const u64 gb = 1024ULL * 1024ULL * 1024ULL;
  const u64 mb = 1024ULL * 1024ULL;
  const u64 kb = 1024ULL;

  if (size >= gb && (size % gb) == 0) {
    com1_write_dec(size / gb);
    com1_write_string("GB");
  } else if (size >= mb && (size % mb) == 0) {
    com1_write_dec(size / mb);
    com1_write_string("MB");
  } else if (size >= kb && (size % kb) == 0) {
    com1_write_dec(size / kb);
    com1_write_string("KB");
  } else {
    com1_write_dec(size);
    com1_write_string("B");
  }
}

static void pci_write_hex_u64(u64 value) {
  const char hex[] = "0123456789ABCDEF";
  int started = 0;

  for (int i = 15; i >= 0; i--) {
    u8 nibble = (value >> (i * 4)) & 0xF;
    if (!started && nibble == 0 && i != 0) {
      continue;
    }
    started = 1;
    com1_write_byte(hex[nibble]);
  }
}

static void pci_debug_device(const pci_device_t *dev) {
  const char *class_name = NULL;
  const char *subclass_name = NULL;
  pci_lookup_class(dev, &class_name, &subclass_name);

  com1_write_string("[PCI] ");
  com1_write_hex_byte(dev->bus);
  com1_write_string(":");
  com1_write_hex_byte(dev->slot);
  com1_write_string(".");
  com1_write_dec(dev->function);
  com1_write_string(" [");
  com1_write_hex_word(dev->vendor_id);
  com1_write_string(":");
  com1_write_hex_word(dev->device_id);
  com1_write_string("] class ");
  com1_write_hex_byte(dev->class_code);
  com1_write_string(":");
  com1_write_hex_byte(dev->subclass);
  com1_write_string(" (");
  com1_write_string(class_name);
  com1_write_string("/");
  com1_write_string(subclass_name);
  com1_write_string(")");

  pci_bar_t bar0;
  int has_bar0 = 0;
  if (pci_read_bar(dev, 0, &bar0) == 0 && bar0.base != 0 && bar0.size != 0) {
    has_bar0 = 1;
  }

  int has_irq = 0;
  if (dev->irq_pin != 0 && dev->irq_line != 0xFF) {
    has_irq = 1;
  }

  if (has_bar0 || has_irq) {
    com1_printf(" -> ");
  }

  if (has_bar0) {
    com1_write_string("BAR0: 0x");
    pci_write_hex_u64(bar0.base);
    com1_write_string(" (");
    if (bar0.is_io) {
      com1_write_string("IO, ");
    }
    pci_print_size(bar0.size);
    com1_write_string(")");
    if (has_irq) {
      com1_write_string(", ");
    }
  }

  if (has_irq) {
    com1_write_string("IRQ ");
    com1_write_dec(dev->irq_line);
  }

  com1_newline();
}

static pci_device_t pci_devices[PCI_MAX_DEVICES];
static int pci_device_count_val = 0;

static void pci_clear_devices(void) {
  for (int i = 0; i < PCI_MAX_DEVICES; i++) {
    pci_devices[i].vendor_id = 0;
    pci_devices[i].device_id = 0;
    pci_devices[i].driver = NULL;
    pci_devices[i].driver_data = NULL;
  }
  pci_device_count_val = 0;
}

static void pci_fill_device(pci_device_t *dev, u8 bus, u8 slot, u8 function) {
  dev->bus = bus;
  dev->slot = slot;
  dev->function = function;

  dev->vendor_id =
      pci_cfg_read16(bus, slot, function, PCI_CFG_VENDOR_ID);
  dev->device_id =
      pci_cfg_read16(bus, slot, function, PCI_CFG_DEVICE_ID);

  dev->revision_id =
      pci_cfg_read8(bus, slot, function, PCI_CFG_REVISION_ID);
  dev->prog_if = pci_cfg_read8(bus, slot, function, PCI_CFG_PROG_IF);
  dev->subclass = pci_cfg_read8(bus, slot, function, PCI_CFG_SUBCLASS);
  dev->class_code = pci_cfg_read8(bus, slot, function, PCI_CFG_CLASS_CODE);

  dev->cache_line_size =
      pci_cfg_read8(bus, slot, function, PCI_CFG_CACHE_LINE_SIZE);
  dev->latency_timer =
      pci_cfg_read8(bus, slot, function, PCI_CFG_LATENCY_TIMER);
  dev->header_type =
      pci_cfg_read8(bus, slot, function, PCI_CFG_HEADER_TYPE);

  dev->subsystem_vendor_id =
      pci_cfg_read16(bus, slot, function, PCI_CFG_SUBSYSTEM_VENDOR_ID);
  dev->subsystem_id =
      pci_cfg_read16(bus, slot, function, PCI_CFG_SUBSYSTEM_ID);

  dev->irq_line = pci_cfg_read8(bus, slot, function, PCI_CFG_IRQ_LINE);
  dev->irq_pin = pci_cfg_read8(bus, slot, function, PCI_CFG_IRQ_PIN);

  dev->driver = NULL;
  dev->driver_data = NULL;
}

static void pci_add_device(u8 bus, u8 slot, u8 function) {
  if (pci_device_count_val >= PCI_MAX_DEVICES) {
    com1_printf("[PCI] device limit reached (%d), skipping %u:%u.%u\n",
                PCI_MAX_DEVICES, bus, slot, function);
    return;
  }

  pci_fill_device(&pci_devices[pci_device_count_val], bus, slot, function);
  pci_debug_device(&pci_devices[pci_device_count_val]);
  pci_device_count_val++;
}

int pci_device_count(void) { return pci_device_count_val; }

pci_device_t *pci_get_device(int index) {
  if (index < 0 || index >= pci_device_count_val) {
    return NULL;
  }
  return &pci_devices[index];
}

int pci_scan(void) {
  pci_clear_devices();

  for (u16 bus = 0; bus < 256; bus++) {
    for (u8 slot = 0; slot < 32; slot++) {
      u16 vendor_id = pci_cfg_read16(bus, slot, 0, PCI_CFG_VENDOR_ID);
      if (vendor_id == PCI_VENDOR_INVALID) {
        continue;
      }

      u8 header_type = pci_cfg_read8(bus, slot, 0, PCI_CFG_HEADER_TYPE);
      u8 functions = (header_type & PCI_HEADER_TYPE_MULTI_FUNCTION) ? 8 : 1;

      for (u8 function = 0; function < functions; function++) {
        vendor_id =
            pci_cfg_read16(bus, slot, function, PCI_CFG_VENDOR_ID);
        if (vendor_id == PCI_VENDOR_INVALID) {
          continue;
        }
        pci_add_device((u8)bus, slot, function);
      }
    }
  }

  return pci_device_count_val;
}
