/*
 * Copyright (c) 2026, otsos team
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the conditions in the BSD
 * 2-Clause License are met. THIS SOFTWARE IS PROVIDED "AS IS".
 */

/* !DEFINES!

$define %func ehci_pci_register as function with args void

*/

/* !SPACE!

$space %export ehci_pci_register

*/

#ifndef KERNEL_DRIVERS_USB_EHCI_H
#define KERNEL_DRIVERS_USB_EHCI_H

int	ehci_pci_register(void);

#endif
