/* !DEFINES!

$define %type ioapic_state_t as mapped I/O APIC controller
$define %type ioapic_discover_ctx_t as MADT discovery context

$define %func ioapic_init as function with args void
$define %func ioapic_route_gsi as function with args u32, u8, u8, int, int
$define %func ioapic_mask_gsi as function with args u32
$define %func ioapic_unmask_gsi as function with args u32
$define %func ioapic_is_initialized as function with args void

*/

/* !SPACE!

$space %internal ioapic_read, ioapic_write, ioapic_find_gsi
$space %internal ioapic_discover_cb
$space %export ioapic_init, ioapic_route_gsi
$space %export ioapic_mask_gsi, ioapic_unmask_gsi
$space %export ioapic_is_initialized

*/

#include <kernel/drivers/acpi/acpi.h>
#include <kernel/interrupts/apic/ioapic.h>
#include <kernel/interrupts/apic/lapic.h>
#include <kernel/interrupts/irq.h>
#include <mm/vm/pmap.h>
#include <mlibc/mlibc.h>
#include <mlibc/stdio.h>

typedef struct ioapic_state {
	u64	base;
	u32	gsi_base;
	u32	count;
	u8	id;
} ioapic_state_t;

typedef struct ioapic_discover_ctx {
	int	count;
} ioapic_discover_ctx_t;

static ioapic_state_t	ioapics[IOAPIC_MAX_CONTROLLERS];
static int		ioapic_count;
static int		ioapic_ready;

static u32
ioapic_read(ioapic_state_t *ioapic, u8 reg)
{
	*(volatile u32 *)((u8 *)ioapic->base + IOAPIC_IOREGSEL) = reg;
	return (*(volatile u32 *)((u8 *)ioapic->base + IOAPIC_IOWIN));
}

static void
ioapic_write(ioapic_state_t *ioapic, u8 reg, u32 value)
{
	*(volatile u32 *)((u8 *)ioapic->base + IOAPIC_IOREGSEL) = reg;
	*(volatile u32 *)((u8 *)ioapic->base + IOAPIC_IOWIN) = value;
}

static ioapic_state_t *
ioapic_find_gsi(u32 gsi)
{
	int	i;

	for (i = 0; i < ioapic_count; i++) {
		if (gsi >= ioapics[i].gsi_base &&
		    gsi < ioapics[i].gsi_base + ioapics[i].count) {
			return (&ioapics[i]);
		}
	}
	return (NULL);
}

static void
ioapic_discover_cb(acpi_madt_entry_header_t *entry, void *arg)
{
	acpi_madt_io_apic_t	*madt_ioapic;
	ioapic_discover_ctx_t	*ctx;
	ioapic_state_t		*ioapic;
	u64			vaddr;
	u32			version, pin;

	ctx = arg;
	if (ctx->count >= IOAPIC_MAX_CONTROLLERS) {
		return;
	}
	madt_ioapic = (acpi_madt_io_apic_t *)entry;
	ioapic = &ioapics[ctx->count];
	vaddr = APIC_MMIO_VBASE + madt_ioapic->io_apic_address;
	pmap_enter(vaddr, madt_ioapic->io_apic_address,
	    PTE_RW | PTE_PCD | PTE_PWT);
	ioapic->base = vaddr;
	ioapic->gsi_base = madt_ioapic->global_system_interrupt_base;
	ioapic->id = madt_ioapic->io_apic_id;
	version = ioapic_read(ioapic, 1);
	ioapic->count = ((version >> 16) & 0xFF) + 1;
	for (pin = 0; pin < ioapic->count; pin++) {
		ioapic_write(ioapic, (u8)(0x10 + pin * 2),
		    IOAPIC_RTE_MASK);
		ioapic_write(ioapic, (u8)(0x11 + pin * 2), 0);
	}
	printk("[IOAPIC] id=%u gsi=%u-%u pins=%u\n", ioapic->id,
	    ioapic->gsi_base, ioapic->gsi_base + ioapic->count - 1,
	    ioapic->count);
	ctx->count++;
}

int
ioapic_init(void)
{
	ioapic_discover_ctx_t	ctx;
	int			i;

	memset(ioapics, 0, sizeof(ioapics));
	memset(&ctx, 0, sizeof(ctx));
	if (acpi_madt_foreach(ACPI_MADT_IO_APIC, ioapic_discover_cb,
	    &ctx) <= 0 || ctx.count == 0) {
		return (-1);
	}
	ioapic_count = ctx.count;
	for (i = 0; i < ioapic_count; i++) {
		if (irq_ioapic_online(ioapics[i].gsi_base,
		    ioapics[i].count) != 0) {
			return (-1);
		}
	}
	ioapic_ready = 1;
	return (0);
}

int
ioapic_route_gsi(u32 gsi, u8 vector, u8 dest, int level, int active_low)
{
	ioapic_state_t	*ioapic;
	u32		low, pin;
	u8		reg;

	ioapic = ioapic_find_gsi(gsi);
	if (!ioapic_ready || ioapic == NULL) {
		return (-1);
	}
	pin = gsi - ioapic->gsi_base;
	reg = (u8)(0x10 + pin * 2);
	low = vector | IOAPIC_RTE_MASK;
	if (level) {
		low |= IOAPIC_RTE_LEVEL;
	}
	if (active_low) {
		low |= IOAPIC_RTE_ACTIVE_LOW;
	}
	ioapic_write(ioapic, reg, low);
	ioapic_write(ioapic, (u8)(reg + 1), ((u32)dest) << 24);
	return (0);
}

int
ioapic_mask_gsi(u32 gsi)
{
	ioapic_state_t	*ioapic;
	u32		low, pin;
	u8		reg;

	ioapic = ioapic_find_gsi(gsi);
	if (ioapic == NULL) {
		return (-1);
	}
	pin = gsi - ioapic->gsi_base;
	reg = (u8)(0x10 + pin * 2);
	low = ioapic_read(ioapic, reg);
	ioapic_write(ioapic, reg, low | IOAPIC_RTE_MASK);
	return (0);
}

int
ioapic_unmask_gsi(u32 gsi)
{
	ioapic_state_t	*ioapic;
	u32		low, pin;
	u8		reg;

	ioapic = ioapic_find_gsi(gsi);
	if (ioapic == NULL) {
		return (-1);
	}
	pin = gsi - ioapic->gsi_base;
	reg = (u8)(0x10 + pin * 2);
	low = ioapic_read(ioapic, reg);
	ioapic_write(ioapic, reg, low & ~IOAPIC_RTE_MASK);
	return (0);
}

int
ioapic_is_initialized(void)
{
	return (ioapic_ready);
}
