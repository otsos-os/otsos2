/* !DEFINES!

$define %type irq_action_t as registered interrupt action
$define %type irq_desc_t as allocated logical interrupt descriptor
$define %type irq_source_t as hardware interrupt identity

$define %func irq_vector_alloc as function with args void
$define %func irq_desc_find as function with args irq_source_t
$define %func irq_desc_alloc as function with args irq_source_t
$define %func irq_request as function with args irq_source_t, irq_handler_t *, void *, const char *, void **
$define %func irq_release as function with args void *
$define %func irq_dispatch as function with args u32, registers_t *

*/

/* !SPACE!

$space %internal irq_vector_alloc, irq_vector_free, irq_desc_find
$space %internal irq_desc_alloc, irq_route, irq_mask, irq_unmask, irq_eoi
$space %export irq_init, irq_source_isa, irq_source_gsi
$space %export irq_source_local, irq_request, irq_release
$space %export irq_dispatch, irq_ioapic_online, irq_stats_dump

*/

#include <kernel/drivers/acpi/acpi.h>
#include <kernel/interrupts/apic/ioapic.h>
#include <kernel/interrupts/apic/lapic.h>
#include <kernel/interrupts/irq.h>
#include <mlibc/mlibc.h>
#include <mlibc/stdio.h>

#define	IRQ_MAX_DESCS		128
#define	IRQ_MAX_ACTIONS		128
#define	IRQ_STORM_THRESHOLD	100000
#define	IRQ_UNHANDLED_LOG_INTERVAL	1024

extern void	pic_mask_irq(unsigned char irq);
extern int	pic_is_spurious(unsigned char irq);
extern void	pic_send_eoi(unsigned char irq);
extern void	pic_unmask_irq(unsigned char irq);

typedef struct irq_desc irq_desc_t;

typedef struct irq_action {
	irq_desc_t	*desc;
	irq_handler_t	*handler;
	void		*arg;
	const char	*name;
	u32		flags;
	int		used;
} irq_action_t;

struct irq_desc {
	irq_action_t	*actions[IRQ_MAX_ACTIONS];
	u64		count;
	u64		handled;
	u64		unhandled;
	irq_source_t	source;
	u32		consecutive_unhandled;
	u16		action_count;
	int		masked_by_storm;
	int		used;
};

static irq_desc_t	irq_descs[IRQ_MAX_DESCS];
static irq_desc_t	*irq_vectors[256];
static irq_action_t	irq_actions[IRQ_MAX_ACTIONS];
static u8		irq_vector_used[256];
static int		irq_ioapic_ready;

static int
irq_vector_alloc(void)
{
	u32	vector;

	for (vector = IRQ_VECTOR_FIRST; vector <= IRQ_VECTOR_LAST; vector++) {
		if (!irq_vector_used[vector]) {
			irq_vector_used[vector] = 1;
			return ((int)vector);
		}
	}
	return (-1);
}

static void
irq_vector_free(u8 vector)
{
	if (vector >= IRQ_VECTOR_FIRST && vector <= IRQ_VECTOR_LAST &&
	    vector != IRQ_VECTOR_LAPIC_TIMER) {
		irq_vector_used[vector] = 0;
	}
}

static irq_desc_t *
irq_desc_find(irq_source_t source)
{
	u32	i;

	for (i = 0; i < IRQ_MAX_DESCS; i++) {
		if (irq_descs[i].used &&
		    irq_descs[i].source.domain == source.domain &&
		    irq_descs[i].source.hwirq == source.hwirq) {
			return (&irq_descs[i]);
		}
	}
	return (NULL);
}

static irq_desc_t *
irq_desc_alloc(irq_source_t source)
{
	irq_desc_t	*desc;
	int		vector;
	u32		i;

	vector = source.vector;
	if ((source.flags & IRQF_FIXED_VECTOR) == 0) {
		vector = irq_vector_alloc();
	} else if (vector > IRQ_VECTOR_LAST || irq_vectors[vector] != NULL ||
	    (irq_vector_used[vector] &&
	    vector != IRQ_VECTOR_LAPIC_TIMER)) {
		return (NULL);
	} else {
		irq_vector_used[vector] = 1;
	}
	if (vector < 0) {
		return (NULL);
	}
	for (i = 0; i < IRQ_MAX_DESCS; i++) {
		if (!irq_descs[i].used) {
			desc = &irq_descs[i];
			memset(desc, 0, sizeof(*desc));
			desc->source = source;
			desc->source.vector = (u8)vector;
			desc->used = 1;
			irq_vectors[vector] = desc;
			return (desc);
		}
	}
	irq_vector_free((u8)vector);
	return (NULL);
}

static int
irq_route(irq_desc_t *desc)
{
	irq_source_t	*source;

	source = &desc->source;
	if (source->domain == IRQ_DOMAIN_IOAPIC) {
		return (ioapic_route_gsi(source->hwirq, source->vector,
		    lapic_get_id(), (source->flags & IRQF_LEVEL) != 0,
		    (source->flags & IRQF_ACTIVE_LOW) != 0));
	}
	return (0);
}

static void
irq_mask(irq_desc_t *desc)
{
	if (desc->source.domain == IRQ_DOMAIN_IOAPIC) {
		(void)ioapic_mask_gsi(desc->source.hwirq);
	} else if (desc->source.domain == IRQ_DOMAIN_PIC) {
		pic_mask_irq((u8)desc->source.hwirq);
	}
}

static void
irq_unmask(irq_desc_t *desc)
{
	if (desc->source.domain == IRQ_DOMAIN_IOAPIC) {
		(void)ioapic_unmask_gsi(desc->source.hwirq);
	} else if (desc->source.domain == IRQ_DOMAIN_PIC) {
		pic_unmask_irq((u8)desc->source.hwirq);
	}
}

static void
irq_eoi(irq_desc_t *desc)
{
	if (desc->source.domain == IRQ_DOMAIN_IOAPIC ||
	    desc->source.domain == IRQ_DOMAIN_LOCAL) {
		lapic_eoi();
	} else if (desc->source.domain == IRQ_DOMAIN_PIC) {
		pic_send_eoi((u8)desc->source.hwirq);
	}
}

void
irq_init(void)
{
	memset(irq_descs, 0, sizeof(irq_descs));
	memset(irq_vectors, 0, sizeof(irq_vectors));
	memset(irq_actions, 0, sizeof(irq_actions));
	memset(irq_vector_used, 0, sizeof(irq_vector_used));
	irq_vector_used[IRQ_VECTOR_SYSCALL] = 1;
	irq_vector_used[IRQ_VECTOR_LAPIC_TIMER] = 1;
	irq_vector_used[IRQ_VECTOR_SPURIOUS] = 1;
}

int
irq_ioapic_online(u32 gsi_base, u32 count)
{
	(void)gsi_base;
	if (count == 0) {
		return (-1);
	}
	irq_ioapic_ready = 1;
	return (0);
}

irq_source_t
irq_source_isa(u32 isa_irq)
{
	irq_source_t	source;
	u32		gsi, flags;

	gsi = isa_irq;
	flags = 0;
	(void)acpi_resolve_isa_irq(isa_irq, &gsi, &flags);
	memset(&source, 0, sizeof(source));
	if (irq_ioapic_ready) {
		source.domain = IRQ_DOMAIN_IOAPIC;
		source.hwirq = gsi;
		source.flags = flags;
	} else {
		source.domain = IRQ_DOMAIN_PIC;
		source.hwirq = isa_irq;
		source.flags = IRQF_FIXED_VECTOR;
		source.vector = (u8)(IRQ_VECTOR_FIRST + isa_irq);
	}
	return (source);
}

irq_source_t
irq_source_gsi(u32 gsi, u32 flags)
{
	irq_source_t	source;

	memset(&source, 0, sizeof(source));
	source.domain = IRQ_DOMAIN_IOAPIC;
	source.hwirq = gsi;
	source.flags = flags;
	return (source);
}

irq_source_t
irq_source_local(u32 vector)
{
	irq_source_t	source;

	memset(&source, 0, sizeof(source));
	source.domain = IRQ_DOMAIN_LOCAL;
	source.hwirq = vector;
	source.vector = (u8)vector;
	source.flags = IRQF_PERCPU | IRQF_FIXED_VECTOR;
	return (source);
}

int
irq_request(irq_source_t source, irq_handler_t *handler, void *arg,
    const char *name, void **cookiep)
{
	irq_action_t	*action;
	irq_desc_t	*desc;
	u32		i;

	if (handler == NULL || source.hwirq > 0xFFFFFF) {
		return (-1);
	}
	desc = irq_desc_find(source);
	if (desc != NULL && ((desc->source.flags & IRQF_SHARED) == 0 ||
	    (source.flags & IRQF_SHARED) == 0)) {
		return (-1);
	}
	if (desc != NULL && ((desc->source.flags ^ source.flags) &
	    (IRQF_LEVEL | IRQF_ACTIVE_LOW | IRQF_PERCPU)) != 0) {
		return (-1);
	}
	if (desc == NULL) {
		desc = irq_desc_alloc(source);
		if (desc == NULL) {
			return (-1);
		}
		if (irq_route(desc) != 0) {
			irq_vectors[desc->source.vector] = NULL;
			irq_vector_free(desc->source.vector);
			memset(desc, 0, sizeof(*desc));
			return (-1);
		}
	}
	action = NULL;
	for (i = 0; i < IRQ_MAX_ACTIONS; i++) {
		if (!irq_actions[i].used) {
			action = &irq_actions[i];
			break;
		}
	}
	if (action == NULL || desc->action_count >= IRQ_MAX_ACTIONS) {
		if (desc->action_count == 0) {
			irq_mask(desc);
			irq_vectors[desc->source.vector] = NULL;
			irq_vector_free(desc->source.vector);
			memset(desc, 0, sizeof(*desc));
		}
		return (-1);
	}
	action->desc = desc;
	action->handler = handler;
	action->arg = arg;
	action->name = name;
	action->flags = source.flags;
	action->used = 1;
	desc->actions[desc->action_count++] = action;
	if (desc->action_count == 1) {
		irq_unmask(desc);
	}
	if (cookiep != NULL) {
		*cookiep = action;
	}
	return (0);
}

int
irq_release(void *cookie)
{
	irq_action_t	*action;
	irq_desc_t	*desc;
	u32		i;
	int		found;

	action = cookie;
	if (action == NULL || !action->used || action->desc == NULL) {
		return (-1);
	}
	desc = action->desc;
	found = 0;
	for (i = 0; i < desc->action_count; i++) {
		if (desc->actions[i] == action) {
			desc->action_count--;
			desc->actions[i] = desc->actions[desc->action_count];
			desc->actions[desc->action_count] = NULL;
			found = 1;
			break;
		}
	}
	if (!found) {
		return (-1);
	}
	memset(action, 0, sizeof(*action));
	if (desc->action_count == 0) {
		irq_mask(desc);
		irq_vectors[desc->source.vector] = NULL;
		irq_vector_free(desc->source.vector);
		memset(desc, 0, sizeof(*desc));
	}
	return (0);
}

int
irq_dispatch(u32 vector, registers_t *regs)
{
	irq_desc_t	*desc;
	irq_result_t	result;
	u32		i;
	int		handled;

	(void)regs;
	if (vector > IRQ_VECTOR_SPURIOUS ||
	    (desc = irq_vectors[vector]) == NULL) {
		return (0);
	}
	desc->count++;
	if (desc->source.domain == IRQ_DOMAIN_PIC &&
	    pic_is_spurious((u8)desc->source.hwirq)) {
		desc->unhandled++;
		return (0);
	}
	handled = 0;
	for (i = 0; i < desc->action_count; i++) {
		result = desc->actions[i]->handler(regs,
		    desc->actions[i]->arg);
		if (result == IRQ_HANDLED) {
			handled = 1;
		}
	}
	if (handled) {
		desc->handled++;
		desc->consecutive_unhandled = 0;
	} else {
		desc->unhandled++;
		desc->consecutive_unhandled++;
		if ((desc->unhandled % IRQ_UNHANDLED_LOG_INTERVAL) == 1) {
			printk("[IRQ] unhandled domain=%u hwirq=%u vector=%u "
			    "count=%lu\n", desc->source.domain,
			    desc->source.hwirq, vector, desc->unhandled);
		}
		if (desc->consecutive_unhandled >= IRQ_STORM_THRESHOLD &&
		    (desc->source.flags & IRQF_PERCPU) == 0) {
			irq_mask(desc);
			desc->masked_by_storm = 1;
			printk("[IRQ] masked storm domain=%u hwirq=%u vector=%u\n",
			    desc->source.domain, desc->source.hwirq, vector);
		}
	}
	irq_eoi(desc);
	return (handled);
}

void
irq_stats_dump(void)
{
	irq_desc_t	*desc;
	u32		i;

	for (i = 0; i < IRQ_MAX_DESCS; i++) {
		desc = &irq_descs[i];
		if (!desc->used) {
			continue;
		}
		printk("[IRQ] domain=%u hwirq=%u vector=%u actions=%u "
		    "total=%lu handled=%lu unhandled=%lu storm=%d\n",
		    desc->source.domain, desc->source.hwirq,
		    desc->source.vector, desc->action_count, desc->count,
		    desc->handled, desc->unhandled, desc->masked_by_storm);
	}
}

int
irq_vector_info(u32 vector, u32 *domain, u32 *hwirq)
{
	irq_desc_t	*desc;

	if (vector > IRQ_VECTOR_SPURIOUS || domain == NULL || hwirq == NULL) {
		return (-1);
	}
	desc = irq_vectors[vector];
	if (desc == NULL) {
		return (-1);
	}
	*domain = (u32)desc->source.domain;
	*hwirq = desc->source.hwirq;
	return (0);
}
