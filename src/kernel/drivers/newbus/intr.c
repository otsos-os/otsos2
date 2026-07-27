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
$define %func newbus_irq_dispatch as function with args u8

*/

/* !SPACE!

$space %export bus_setup_intr, bus_teardown_intr
$space %export newbus_irq_dispatch

*/

#include <kernel/drivers/newbus/newbus.h>

extern void	pic_unmask_irq(unsigned char irq);
extern void	ioapic_unmask_irq(u8 irq);

typedef struct newbus_intr_entry {
	device_t			dev;
	resource_t		*res;
	newbus_intr_handler_t	*handler;
	void			*arg;
	u8			irq;
	int			used;
} newbus_intr_entry_t;

static newbus_intr_entry_t	newbus_intrs[NEWBUS_MAX_INTR_HANDLERS];

int
bus_setup_intr(device_t dev, resource_t *res,
    newbus_intr_handler_t *handler, void *arg, void **cookiep)
{
	int	i;

	if (dev == NULL || res == NULL || handler == NULL ||
	    res->type != SYS_RES_IRQ) {
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
		if (cookiep != NULL) {
			*cookiep = &newbus_intrs[i];
		}
		if (newbus_intrs[i].irq < 16) {
			pic_unmask_irq(newbus_intrs[i].irq);
		}
		ioapic_unmask_irq(newbus_intrs[i].irq);
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
	entry->used = 0;
	entry->dev = NULL;
	entry->res = NULL;
	entry->handler = NULL;
	entry->arg = NULL;
	return (0);
}

int
newbus_irq_dispatch(u8 irq)
{
	int	handled, i;

	handled = 0;
	for (i = 0; i < NEWBUS_MAX_INTR_HANDLERS; i++) {
		if (!newbus_intrs[i].used || newbus_intrs[i].irq != irq) {
			continue;
		}
		if (newbus_intrs[i].handler(newbus_intrs[i].arg) == 0) {
			handled = 1;
		}
	}
	return (handled);
}
