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

$define %type newbus_poll_entry_t as polling hook registration
$define %type int as 32 bit signed

$define %func bus_setup_poll as function with args device_t, u32, newbus_poll_handler_t *, void *, void **
$define %func bus_teardown_poll as function with args device_t, void *
$define %func newbus_poll_dispatch as procedure with args u32

*/

/* !SPACE!

$space %export bus_setup_poll, bus_teardown_poll
$space %export newbus_poll_dispatch

*/

#include <kernel/drivers/newbus/newbus.h>

typedef struct newbus_poll_entry {
	device_t			dev;
	newbus_poll_handler_t	*handler;
	void			*arg;
	u32			event;
	int			used;
} newbus_poll_entry_t;

static newbus_poll_entry_t	newbus_polls[NEWBUS_MAX_POLL_HOOKS];

int
bus_setup_poll(device_t dev, u32 event,
    newbus_poll_handler_t *handler, void *arg, void **cookiep)
{
	int	i;

	if (dev == NULL || handler == NULL || event == 0) {
		return (-1);
	}
	for (i = 0; i < NEWBUS_MAX_POLL_HOOKS; i++) {
		if (newbus_polls[i].used) {
			continue;
		}
		newbus_polls[i].dev = dev;
		newbus_polls[i].handler = handler;
		newbus_polls[i].arg = arg;
		newbus_polls[i].event = event;
		newbus_polls[i].used = 1;
		if (cookiep != NULL) {
			*cookiep = &newbus_polls[i];
		}
		return (0);
	}
	return (-1);
}

int
bus_teardown_poll(device_t dev, void *cookie)
{
	newbus_poll_entry_t	*entry;

	if (dev == NULL || cookie == NULL) {
		return (-1);
	}
	entry = (newbus_poll_entry_t *)cookie;
	if (!entry->used || entry->dev != dev) {
		return (-1);
	}
	entry->used = 0;
	entry->dev = NULL;
	entry->handler = NULL;
	entry->arg = NULL;
	entry->event = 0;
	return (0);
}

void
newbus_poll_dispatch(u32 event)
{
	int	i;

	for (i = 0; i < NEWBUS_MAX_POLL_HOOKS; i++) {
		if (!newbus_polls[i].used ||
		    (newbus_polls[i].event & event) == 0) {
			continue;
		}
		newbus_polls[i].handler(newbus_polls[i].arg);
	}
}
