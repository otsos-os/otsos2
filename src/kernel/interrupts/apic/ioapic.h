/* !DEFINES!

$define %type u8 as 8 bit unsigned
$define %type u32 as 32 bit unsigned
$define %type int as 32 bit signed

$define %func ioapic_init as function with args void
$define %func ioapic_route_gsi as function with args u32, u8, u8, int, int
$define %func ioapic_mask_gsi as function with args u32
$define %func ioapic_unmask_gsi as function with args u32
$define %func ioapic_is_initialized as function with args void

*/

/* !SPACE!

$space %export ioapic_init, ioapic_route_gsi
$space %export ioapic_mask_gsi, ioapic_unmask_gsi
$space %export ioapic_is_initialized

*/

#ifndef KERNEL_INTERRUPTS_APIC_IOAPIC_H
#define KERNEL_INTERRUPTS_APIC_IOAPIC_H

#include <mlibc/mlibc.h>

#define	IOAPIC_IOREGSEL		0x00
#define	IOAPIC_IOWIN		0x10
#define	IOAPIC_MAX_CONTROLLERS	8
#define	IOAPIC_RTE_VECTOR_MASK	0x000000FF
#define	IOAPIC_RTE_ACTIVE_LOW	(1U << 13)
#define	IOAPIC_RTE_LEVEL	(1U << 15)
#define	IOAPIC_RTE_MASK		(1U << 16)

int	ioapic_init(void);
int	ioapic_route_gsi(u32 gsi, u8 vector, u8 dest, int level,
	    int active_low);
int	ioapic_mask_gsi(u32 gsi);
int	ioapic_unmask_gsi(u32 gsi);
int	ioapic_is_initialized(void);

#endif
