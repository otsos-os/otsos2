/* !DEFINES!

$define %type irq_result_t as interrupt action result
$define %type irq_domain_t as interrupt hardware namespace
$define %type irq_handler_t as interrupt action callback
$define %type irq_source_t as hardware interrupt identity

$define %func irq_init as procedure with args void
$define %func irq_source_isa as function with args u32
$define %func irq_source_gsi as function with args u32, u32
$define %func irq_source_local as function with args u32
$define %func irq_source_msi as function with args irq_source_t *
$define %func irq_source_msi_release as procedure with args irq_source_t *
$define %func irq_request as function with args irq_source_t, irq_handler_t *, void *, const char *, void **
$define %func irq_release as function with args void *
$define %func irq_dispatch as function with args u32, registers_t *
$define %func irq_vector_is_software as function with args u32
$define %func irq_ioapic_online as function with args u32, u32
$define %func irq_stats_dump as procedure with args void
$define %func irq_vector_info as function with args u32, u32 *, u32 *

*/

/* !SPACE!

$space %export irq_init, irq_source_isa, irq_source_gsi
$space %export irq_source_local, irq_request, irq_release
$space %export irq_source_msi, irq_source_msi_release
$space %export irq_dispatch, irq_ioapic_online, irq_stats_dump
$space %export irq_vector_info, irq_vector_is_software

*/

#ifndef KERNEL_INTERRUPTS_IRQ_H
#define KERNEL_INTERRUPTS_IRQ_H

#include <kernel/interrupts/idt.h>
#include <mlibc/mlibc.h>

#define	IRQ_VECTOR_FIRST	32
#define	IRQ_VECTOR_LAST		254
#define	IRQ_VECTOR_DYNAMIC_FIRST	64
#define	IRQ_VECTOR_SYSCALL	128
#define	IRQ_VECTOR_LAPIC_TIMER	48
#define	IRQ_VECTOR_YIELD	49
#define	IRQ_VECTOR_SPURIOUS	255

#define	IRQF_SHARED		0x0001
#define	IRQF_LEVEL		0x0002
#define	IRQF_ACTIVE_LOW		0x0004
#define	IRQF_PERCPU		0x0008
#define	IRQF_FIXED_VECTOR	0x0010
#define	IRQF_PREALLOC_VECTOR	0x0020

typedef enum irq_result {
	IRQ_NONE = 0,
	IRQ_HANDLED = 1
} irq_result_t;

typedef enum irq_domain {
	IRQ_DOMAIN_PIC = 0,
	IRQ_DOMAIN_IOAPIC,
	IRQ_DOMAIN_LOCAL,
	IRQ_DOMAIN_MSI
} irq_domain_t;

typedef irq_result_t	(irq_handler_t)(registers_t *, void *);

typedef struct irq_source {
	irq_domain_t	domain;
	u32		hwirq;
	u32		flags;
	u8		vector;
} irq_source_t;

void		irq_init(void);
irq_source_t	irq_source_isa(u32 isa_irq);
irq_source_t	irq_source_gsi(u32 gsi, u32 flags);
irq_source_t	irq_source_local(u32 vector);
int		irq_source_msi(irq_source_t *source);
void		irq_source_msi_release(irq_source_t *source);
int		irq_request(irq_source_t source, irq_handler_t *handler,
		    void *arg, const char *name, void **cookiep);
int		irq_release(void *cookie);
int		irq_dispatch(u32 vector, registers_t *regs);
int		irq_vector_is_software(u32 vector);
int		irq_ioapic_online(u32 gsi_base, u32 count);
void		irq_stats_dump(void);
int		irq_vector_info(u32 vector, u32 *domain, u32 *hwirq);

#endif
