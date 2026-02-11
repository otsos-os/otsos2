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

#include <kernel/drivers/watchdog/watchdog.h>
#include <lib/com1.h>
#include <mlibc/mlibc.h>

#define ICH_VENDOR_ID 0x8086
#define ICH_CLASS_BRIDGE 0x06
#define ICH_SUBCLASS_ISA 0x01

#define ICH_PMBASE 0x40
#define ICH_ACPI_CNTL 0x44
#define ICH_ACPI_EN 0x01

#define TCO_BASE_OFFSET 0x60
#define TCO_RLD 0x00
#define TCO1_CNT 0x08
#define TCO_TMR 0x12
#define TCO1_CNT_TMR_HLT (1 << 11)

typedef struct ich_tco_priv {
  u16 tco_base;
} ich_tco_priv_t;

static ich_tco_priv_t ich_tco_priv;

static u16 ich_tco_read16(u16 base, u16 offset) {
  return inw((u16)(base + offset));
}

static void ich_tco_write16(u16 base, u16 offset, u16 value) {
  outw((u16)(base + offset), value);
}

static int ich_tco_set_timeout(watchdog_device_t *dev, u32 timeout_sec) {
  if (!dev || !dev->priv) {
    return -1;
  }

  ich_tco_priv_t *priv = (ich_tco_priv_t *)dev->priv;
  ich_tco_write16(priv->tco_base, TCO_TMR, (u16)timeout_sec);
  ich_tco_write16(priv->tco_base, TCO_RLD, 0x01);
  return 0;
}

static int ich_tco_start(watchdog_device_t *dev, u32 timeout_sec) {
  if (!dev || !dev->priv) {
    return -1;
  }

  ich_tco_priv_t *priv = (ich_tco_priv_t *)dev->priv;
  u16 ctrl = ich_tco_read16(priv->tco_base, TCO1_CNT);
  ctrl &= (u16)(~TCO1_CNT_TMR_HLT);
  ich_tco_write16(priv->tco_base, TCO1_CNT, ctrl);

  ich_tco_write16(priv->tco_base, TCO_TMR, (u16)timeout_sec);
  ich_tco_write16(priv->tco_base, TCO_RLD, 0x01);
  return 0;
}

static int ich_tco_stop(watchdog_device_t *dev) {
  if (!dev || !dev->priv) {
    return -1;
  }

  ich_tco_priv_t *priv = (ich_tco_priv_t *)dev->priv;
  u16 ctrl = ich_tco_read16(priv->tco_base, TCO1_CNT);
  ctrl |= TCO1_CNT_TMR_HLT;
  ich_tco_write16(priv->tco_base, TCO1_CNT, ctrl);
  return 0;
}

static int ich_tco_ping(watchdog_device_t *dev) {
  if (!dev || !dev->priv) {
    return -1;
  }

  ich_tco_priv_t *priv = (ich_tco_priv_t *)dev->priv;
  ich_tco_write16(priv->tco_base, TCO_RLD, 0x01);
  return 0;
}

static const watchdog_ops_t ich_tco_ops = {
    .start = ich_tco_start,
    .stop = ich_tco_stop,
    .ping = ich_tco_ping,
    .set_timeout = ich_tco_set_timeout,
};

static watchdog_device_t ich_tco_device = {
    .name = "Intel ICH TCO",
    .min_timeout_sec = 4,
    .max_timeout_sec = 1023,
    .timeout_sec = 0,
    .running = 0,
    .pci_dev = NULL,
    .priv = &ich_tco_priv,
    .ops = &ich_tco_ops,
};

static int ich_tco_probe(pci_device_t *dev, const pci_match_t *match) {
  (void)match;
  if (!dev) {
    return -1;
  }

  u32 pmbase = pci_cfg_read32(dev->bus, dev->slot, dev->function, ICH_PMBASE);
  pmbase &= 0xFF80;
  if (pmbase == 0) {
    com1_printf("[WDT] ich_tco: PMBASE not set\n");
    return -1;
  }

  u8 acpi_ctl =
      pci_cfg_read8(dev->bus, dev->slot, dev->function, ICH_ACPI_CNTL);
  if ((acpi_ctl & ICH_ACPI_EN) == 0) {
    pci_cfg_write8(dev->bus, dev->slot, dev->function, ICH_ACPI_CNTL,
                   acpi_ctl | ICH_ACPI_EN);
  }

  ich_tco_priv.tco_base = (u16)(pmbase + TCO_BASE_OFFSET);
  ich_tco_device.pci_dev = dev;

  return watchdog_register_device(&ich_tco_device);
}

static pci_match_t ich_tco_matches[] = {
    {.vendor_id = ICH_VENDOR_ID,
     .device_id = PCI_ANY_ID,
     .class_code = ICH_CLASS_BRIDGE,
     .subclass = ICH_SUBCLASS_ISA,
     .prog_if = PCI_ANY_PROGIF},
};

static pci_driver_t ich_tco_pci_driver = {
    .name = "watchdog-ich-tco",
    .matches = ich_tco_matches,
    .match_count = 1,
    .probe = ich_tco_probe,
    .remove = NULL,
};

int watchdog_ich_tco_init(void) {
  return pci_register_driver(&ich_tco_pci_driver);
}
