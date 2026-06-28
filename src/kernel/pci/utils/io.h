/*
 * Copyright (c) 2026, otsos team
 *
 * [.BSD-2-clause license text...]
 */

/* !DEFINES!

$define %type u16 as 16 bit unsigned
$define %type u32 as 32 bit unsigned

$define %func pci_outl as procedure with args u16, u32
$define %func pci_inl as function with args u16

*/

/* !SPACE!

$space %export pci_outl, pci_inl

*/

#ifndef PCI_IO_H
#define PCI_IO_H

#include <mlibc/mlibc.h>

static inline void
pci_outl(u16 port, u32 value)
{
	__asm__ volatile("outl %0, %1"
	    : : "a"(value), "Nd"(port));
}

static inline u32
pci_inl(u16 port)
{
	u32	value;

	__asm__ volatile("inl %1, %0"
	    : "=a"(value) : "Nd"(port));
	return (value);
}

#endif
