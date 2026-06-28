/*
 * Copyright (c) 2026, otsos team
 *
 * [.BSD-2-clause license text...]
 */

/* !DEFINES!

$define %type u8 as 8 bit unsigned
$define %type pci_device_t as struct with PCI device info
$define %type pci_bar_t as struct with BAR info

$define %func pci_get_bar_count as function with args const pci_device_t *
$define %func pci_read_bar as function with args const pci_device_t *, u8, pci_bar_t *

*/

/* !SPACE!

$space %export pci_get_bar_count, pci_read_bar

*/

#ifndef PCI_BAR_H
#define PCI_BAR_H

#include <kernel/pci/pci.h>

int	pci_get_bar_count(const pci_device_t *dev);
int	pci_read_bar(const pci_device_t *dev, u8 index,
    pci_bar_t *out);

#endif
