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
#include <kernel/pci/utils/io.h>
#include <mlibc/stdio.h>
#include <kernel/panic.h>

#define PCI_CONFIG_ADDRESS 0xCF8

static int pci_config_present(void) {
  u32 saved = pci_inl(PCI_CONFIG_ADDRESS);
  pci_outl(PCI_CONFIG_ADDRESS, 0x80000000U);
  u32 read_back = pci_inl(PCI_CONFIG_ADDRESS);
  pci_outl(PCI_CONFIG_ADDRESS, saved);
  return (read_back == 0x80000000U);
}

static int pci_initialized = 0;

static int
pci_match_device(const pci_device_t *dev, const pci_match_t *match)
{
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

static const pci_match_t *
pci_find_driver_match(pci_device_t *dev, pci_driver_t *driver)
{
  u32 i;

  if (!driver || !driver->matches || driver->match_count == 0) {
    return NULL;
  }

  for (i = 0; i < driver->match_count; i++) {
    const pci_match_t *match;

    match = &driver->matches[i];
    if (!pci_match_device(dev, match)) {
      continue;
    }
    return match;
  }

  return NULL;
}

static void
pci_bus_add_resources(device_t child, pci_device_t *pdev)
{
  pci_bar_t bar;
  int i;

  for (i = 0; i < pci_get_bar_count(pdev); i++) {
    if (pci_read_bar(pdev, (u8)i, &bar) != 0 || bar.base == 0 ||
        bar.size == 0) {
      continue;
    }
    if (bar.is_io) {
      bus_set_resource(child, SYS_RES_IOPORT, i, bar.base, bar.size, 0);
    } else {
      bus_set_resource(child, SYS_RES_MEMORY, i, bar.base, bar.size, 0);
    }
  }
  if (pdev->irq_pin != 0 && pdev->irq_line != 0xFF) {
    bus_set_resource(child, SYS_RES_IRQ, 0, pdev->irq_line, 1,
        RF_SHAREABLE | RF_IRQ_LEVEL | RF_IRQ_ACTIVE_LOW | RF_IRQ_ISA);
  }
}

static void
pci_bus_identify(driver_t *driver, device_t parent)
{
  (void)driver;
  if (device_find_child(parent, "pci", 0) == NULL) {
    device_add_child(parent, "pci", 0);
  }
}

static int
pci_bus_probe(device_t dev)
{
  (void)dev;
  if (!pci_config_present()) {
    return (-1);
  }
  return (100);
}

static int
pci_bus_attach(device_t dev)
{
  pci_device_t *pdev;
  device_t child;
  int devices;
  int i;

  if (!pci_config_present()) {
    return (-1);
  }
  devices = pci_scan();
  printk("[PCI] scan complete: %d device(s)\n", devices);
  for (i = 0; i < devices; i++) {
    pdev = pci_get_device(i);
    child = device_add_child(dev, "pcifn", -1);
    if (child == NULL) {
      continue;
    }
    pdev->nb_device = child;
    device_set_ivars(child, pdev);
    pci_bus_add_resources(child, pdev);
  }
  pci_initialized = 1;
  return (0);
}

int
pci_newbus_probe(device_t dev, pci_driver_t *driver)
{
  pci_device_t *pdev;

  if (dev == NULL || driver == NULL) {
    return (-1);
  }
  pdev = (pci_device_t *)device_get_ivars(dev);
  if (pdev == NULL || pci_find_driver_match(pdev, driver) == NULL) {
    return (-1);
  }
  return (100);
}

int
pci_newbus_attach(device_t dev, pci_driver_t *driver)
{
  const pci_match_t *match;
  pci_device_t *pdev;

  if (dev == NULL || driver == NULL) {
    return (-1);
  }
  pdev = (pci_device_t *)device_get_ivars(dev);
  if (pdev == NULL) {
    return (-1);
  }
  match = pci_find_driver_match(pdev, driver);
  if (match == NULL) {
    return (-1);
  }
  if (driver->probe != NULL && driver->probe(pdev, match) != 0) {
    return (-1);
  }
  pdev->driver = driver;
  if (driver->name != NULL) {
    printk("[PCI] %s bound to %02x:%02x.%u\n", driver->name,
        pdev->bus, pdev->slot, pdev->function);
  }
  return (0);
}

int
pci_newbus_detach(device_t dev, pci_driver_t *driver)
{
  pci_device_t *pdev;

  if (dev == NULL || driver == NULL) {
    return (-1);
  }
  pdev = (pci_device_t *)device_get_ivars(dev);
  if (pdev == NULL || pdev->driver != driver) {
    return (-1);
  }
  if (driver->remove != NULL) {
    driver->remove(pdev);
  }
  pdev->driver = NULL;
  pdev->driver_data = NULL;
  return (0);
}

int
pci_register_driver(pci_driver_t *driver)
{
  (void)driver;
  drivers_log("[PCI] pci_register_driver is deprecated; "
      "use PCI_DRIVER_MODULE\n");
  return (-1);
}

int
pci_unregister_driver(pci_driver_t *driver)
{
  (void)driver;
  return (-1);
}

void
pci_init(void)
{
  if (pci_initialized) {
    return;
  }
  if (!pci_config_present()) {
    pci_initialized = 0;
    panic("[PCI] config access not available, pci disabled\n");
    return;
  }

  drivers_log("[PCI] pci_init is deprecated; pci0 is a newbus device\n");
}

int pci_is_initialized(void) { return pci_initialized; }

static devclass_t pci_bus_devclass = {
  .name = "pci",
  .maxunit = 1,
};

static driver_t pci_bus_driver = {
  .name = "pci",
  .identify = pci_bus_identify,
  .probe = pci_bus_probe,
  .attach = pci_bus_attach,
  .detach = NULL,
  .suspend = NULL,
  .resume = NULL,
  .shutdown = NULL,
  .priv = NULL,
};

DRIVER_MODULE(pci, platform, pci_bus_driver, pci_bus_devclass,
    NEWBUS_PASS_BUS, NEWBUS_ORDER_MIDDLE);
