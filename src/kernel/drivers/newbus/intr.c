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

/* !DEFINES!

$define %type newbus_intr_entry_t as interrupt handler registration
$define %type int as 32 bit signed

$define %func bus_setup_intr as function with args device_t, resource_t *, newbus_intr_handler_t *, void *, void **
$define %func bus_teardown_intr as function with args device_t, resource_t *, void *
$define %func newbus_intr_invoke as function with args void *

*/

/* !SPACE!

$space %internal newbus_intr_invoke
$space %export bus_setup_intr, bus_teardown_intr

*/

#include <kernel/drivers/newbus/newbus.h>
#include <kernel/interrupts/irq.h>

typedef struct newbus_intr_entry {
	device_t			dev;
	resource_t		*res;
	newbus_intr_handler_t	*handler;
	void			*arg;
	u8			irq;
	void			*irq_cookie;
	irq_source_t		source;
	int			used;
} newbus_intr_entry_t;

static irq_result_t
newbus_intr_invoke(registers_t *regs, void *arg)
{
	newbus_intr_entry_t	*entry;

	(void)regs;
	entry = arg;
	if (entry == NULL || !entry->used || entry->handler == NULL) {
		return (IRQ_NONE);
	}
	return (entry->handler(entry->arg) == 0 ? IRQ_HANDLED : IRQ_NONE);
}

static newbus_intr_entry_t	newbus_intrs[NEWBUS_MAX_INTR_HANDLERS];

int
bus_setup_intr(device_t dev, resource_t *res,
    newbus_intr_handler_t *handler, void *arg, void **cookiep)
{
	u32	irq_flags;
	int	i;

	if (dev == NULL || res == NULL || handler == NULL ||
	    res->type != SYS_RES_IRQ || res->owner != dev ||
	    (res->flags & RF_ACTIVE) == 0 ||
	    (res->flags & RF_BUSY) == 0) {
		return (-1);
	}
	for (i = 0; i < NEWBUS_MAX_INTR_HANDLERS; i++) {
		if (newbus_intrs[i].used) {
			continue;
		}
		newbus_intrs[i].dev = dev;
		newbus_intrs[i].res = res;
		newbus_intrs[i].handler = handler;
		newbus_intrs[i].arg = arg;
		newbus_intrs[i].irq = (u8)res->start;
		newbus_intrs[i].used = 1;
		irq_flags = 0;
		if (res->flags & RF_SHAREABLE)
			irq_flags |= IRQF_SHARED;
		if (res->flags & RF_IRQ_LEVEL)
			irq_flags |= IRQF_LEVEL;
		if (res->flags & RF_IRQ_ACTIVE_LOW)
			irq_flags |= IRQF_ACTIVE_LOW;
		if (res->flags & RF_IRQ_MSI) {
			newbus_intrs[i].source = irq_source_msi(
			    (u32)res->start);
		} else if (res->flags & RF_IRQ_GSI) {
			newbus_intrs[i].source = irq_source_gsi(
			    (u32)res->start, irq_flags);
		} else {
			newbus_intrs[i].source = irq_source_isa(
			    (u32)res->start);
			newbus_intrs[i].source.flags |= irq_flags;
		}
		if (irq_request(newbus_intrs[i].source,
		    newbus_intr_invoke, &newbus_intrs[i],
		    device_get_nameunit(dev), &newbus_intrs[i].irq_cookie) != 0) {
			memset(&newbus_intrs[i], 0, sizeof(newbus_intrs[i]));
			return (-1);
		}
		if (cookiep != NULL) {
			*cookiep = &newbus_intrs[i];
		}
		return (0);
	}
	return (-1);
}

int
bus_teardown_intr(device_t dev, resource_t *res, void *cookie)
{
	newbus_intr_entry_t	*entry;

	if (dev == NULL || res == NULL || cookie == NULL) {
		return (-1);
	}
	entry = (newbus_intr_entry_t *)cookie;
	if (!entry->used || entry->dev != dev || entry->res != res) {
		return (-1);
	}
	if (irq_release(entry->irq_cookie) != 0) {
		return (-1);
	}
	entry->used = 0;
	entry->dev = NULL;
	entry->res = NULL;
	entry->handler = NULL;
	entry->arg = NULL;
	entry->irq_cookie = NULL;
	return (0);
}
