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
#include <lib/com1.h>
#include <kernel/panic.h>

#define PCI_CONFIG_ADDRESS 0xCF8

static int pci_config_present(void) {
  u32 saved = pci_inl(PCI_CONFIG_ADDRESS);
  pci_outl(PCI_CONFIG_ADDRESS, 0x80000000U);
  u32 read_back = pci_inl(PCI_CONFIG_ADDRESS);
  pci_outl(PCI_CONFIG_ADDRESS, saved);
  return (read_back == 0x80000000U);
}

static pci_driver_t *pci_drivers[PCI_MAX_DRIVERS];
static int pci_driver_count_val = 0;
static int pci_initialized = 0;

static int pci_match_device(const pci_device_t *dev,
                            const pci_match_t *match) {
  if (match->vendor_id != PCI_ANY_ID && match->vendor_id != dev->vendor_id) {
    return 0;
  }
  if (match->device_id != PCI_ANY_ID && match->device_id != dev->device_id) {
    return 0;
  }
  if (match->class_code != PCI_ANY_CLASS &&
      match->class_code != dev->class_code) {
    return 0;
  }
  if (match->subclass != PCI_ANY_SUBCLASS &&
      match->subclass != dev->subclass) {
    return 0;
  }
  if (match->prog_if != PCI_ANY_PROGIF && match->prog_if != dev->prog_if) {
    return 0;
  }
  return 1;
}

static int pci_probe_with_driver(pci_device_t *dev, pci_driver_t *driver) {
  if (!driver || !driver->matches || driver->match_count == 0) {
    return -1;
  }

  for (u32 i = 0; i < driver->match_count; i++) {
    const pci_match_t *match = &driver->matches[i];
    if (!pci_match_device(dev, match)) {
      continue;
    }

    if (driver->probe) {
      if (driver->probe(dev, match) != 0) {
        continue;
      }
    }

    dev->driver = driver;
    return 0;
  }

  return -1;
}

static void pci_try_attach_device(pci_device_t *dev) {
  if (!dev || dev->driver) {
    return;
  }

  for (int i = 0; i < pci_driver_count_val; i++) {
    pci_driver_t *driver = pci_drivers[i];
    if (pci_probe_with_driver(dev, driver) == 0) {
      if (driver && driver->name) {
        com1_printf("[PCI] %s bound to %02x:%02x.%u\n", driver->name, dev->bus,
                    dev->slot, dev->function);
      }
      return;
    }
  }
}

static void pci_attach_all_devices(void) {
  int count = pci_device_count();
  for (int i = 0; i < count; i++) {
    pci_try_attach_device(pci_get_device(i));
  }
}

int pci_register_driver(pci_driver_t *driver) {
  if (!driver) {
    return -1;
  }
  for (int i = 0; i < pci_driver_count_val; i++) {
    if (pci_drivers[i] == driver) {
      return -1;
    }
  }
  if (pci_driver_count_val >= PCI_MAX_DRIVERS) {
    com1_printf("[PCI] driver limit reached (%d)\n", PCI_MAX_DRIVERS);
    return -1;
  }

  pci_drivers[pci_driver_count_val++] = driver;
  if (driver->name) {
    com1_printf("[PCI] driver registered: %s\n", driver->name);
  }

  if (pci_initialized) {
    pci_attach_all_devices();
  }

  return 0;
}

int pci_unregister_driver(pci_driver_t *driver) {
  if (!driver) {
    return -1;
  }

  int index = -1;
  for (int i = 0; i < pci_driver_count_val; i++) {
    if (pci_drivers[i] == driver) {
      index = i;
      break;
    }
  }
  if (index < 0) {
    return -1;
  }

  int dev_count = pci_device_count();
  for (int i = 0; i < dev_count; i++) {
    pci_device_t *dev = pci_get_device(i);
    if (dev && dev->driver == driver) {
      if (driver->remove) {
        driver->remove(dev);
      }
      dev->driver = NULL;
      dev->driver_data = NULL;
    }
  }

  for (int i = index; i < pci_driver_count_val - 1; i++) {
    pci_drivers[i] = pci_drivers[i + 1];
  }
  pci_driver_count_val--;

  if (driver->name) {
    com1_printf("[PCI] driver unregistered: %s\n", driver->name);
  }

  return 0;
}

void pci_init(void) {
  if (pci_initialized) {
    return;
  }
  if (!pci_config_present()) {
    pci_initialized = 0;
    panic("[PCI] config access not available, pci disabled\n");
    return;
  }

  int devices = pci_scan();
  com1_printf("[PCI] scan complete: %d device(s)\n", devices);
  pci_attach_all_devices();
  pci_initialized = 1;
}

int pci_is_initialized(void) { return pci_initialized; }
