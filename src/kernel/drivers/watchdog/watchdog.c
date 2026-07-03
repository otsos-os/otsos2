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

$define %type u8 as 8 bit unsigned
$define %type u32 as 32 bit unsigned
$define %type u64 as 64 bit unsigned
$define %type int as 32 bit signed
$define %type watchdog_device_t as struct with name, timeouts, pci_dev, ops

$define %func watchdog_clamp_timeout as function with args const watchdog_device_t *, u32 *
$define %func watchdog_update_interval as procedure with args u32
$define %func watchdog_init as procedure with args void
$define %func watchdog_is_initialized as function with args void
$define %func watchdog_register_device as function with args watchdog_device_t *
$define %func watchdog_device_count as function with args void
$define %func watchdog_get_device as function with args int
$define %func watchdog_set_active as function with args int
$define %func watchdog_get_active as function with args void
$define %func watchdog_get_active_name as function with args void
$define %func watchdog_set_timeout as function with args u32
$define %func watchdog_start as function with args u32
$define %func watchdog_stop as function with args void
$define %func watchdog_ping as function with args void
$define %func watchdog_tick as procedure with args void

*/

/* !SPACE!

$space %internal watchdog_clamp_timeout, watchdog_update_interval
$space %export watchdog_init, watchdog_is_initialized
$space %export watchdog_register_device, watchdog_device_count
$space %export watchdog_get_device, watchdog_set_active
$space %export watchdog_get_active, watchdog_get_active_name
$space %export watchdog_set_timeout, watchdog_start, watchdog_stop
$space %export watchdog_ping, watchdog_tick

*/

#include <kernel/drivers/timer.h>
#include <kernel/drivers/watchdog/watchdog.h>
#include <kernel/panic.h>
#include <mlibc/stdio.h>

int	watchdog_i6300esb_init(void);
int	watchdog_ich_tco_init(void);

static watchdog_device_t	*watchdog_devices[WATCHDOG_MAX_DEVICES];
static int			watchdog_device_count_val;
static watchdog_device_t	*watchdog_active;
static int			watchdog_initialized;
static int			watchdog_auto_feed;
static u64			watchdog_last_ping_tick;
static u64			watchdog_ping_interval_ticks;

static int
watchdog_clamp_timeout(const watchdog_device_t *dev, u32 *timeout)
{
	if (!dev || !timeout) {
		return (-1);
	}

	if (*timeout < dev->min_timeout_sec) {
		*timeout = dev->min_timeout_sec;
	}
	if (dev->max_timeout_sec != 0 &&
	    *timeout > dev->max_timeout_sec) {
		*timeout = dev->max_timeout_sec;
	}

	return (0);
}

static void
watchdog_update_interval(u32 timeout_sec)
{
	u32	frequency, feed_sec;

	frequency = timer_get_frequency();
	if (frequency == 0) {
		watchdog_ping_interval_ticks = 0;
		return;
	}

	feed_sec = timeout_sec / 2;
	if (feed_sec < 1) {
		feed_sec = 1;
	}

	watchdog_ping_interval_ticks =
	    (u64)frequency * (u64)feed_sec;
}

void
watchdog_init(void)
{
	if (watchdog_initialized) {
		return;
	}

	watchdog_i6300esb_init();
	watchdog_ich_tco_init();

	watchdog_initialized = 1;
}

int
watchdog_is_initialized(void)
{
	return (watchdog_initialized);
}

int
watchdog_register_device(watchdog_device_t *dev)
{
	int	i;

	if (!dev || !dev->ops) {
		return (-1);
	}

	for (i = 0; i < watchdog_device_count_val; i++) {
		if (watchdog_devices[i] == dev) {
			return (-1);
		}
	}

	if (watchdog_device_count_val >= WATCHDOG_MAX_DEVICES) {
		drivers_log("[WDT] device limit reached (%d)\n",
		    WATCHDOG_MAX_DEVICES);
		return (-1);
	}

	watchdog_devices[watchdog_device_count_val++] = dev;

	drivers_log("[WDT] registered: %s\n",
	    dev->name ? dev->name : "unknown");

	if (!watchdog_active) {
		watchdog_active = dev;
	}

	return (0);
}

int
watchdog_device_count(void)
{
	return (watchdog_device_count_val);
}

watchdog_device_t *
watchdog_get_device(int index)
{
	if (index < 0 || index >= watchdog_device_count_val) {
		return (NULL);
	}
	return (watchdog_devices[index]);
}

int
watchdog_set_active(int index)
{
	watchdog_device_t	*dev;

	dev = watchdog_get_device(index);
	if (!dev) {
		return (-1);
	}
	watchdog_active = dev;
	return (0);
}

watchdog_device_t *
watchdog_get_active(void)
{
	return (watchdog_active);
}

const char *
watchdog_get_active_name(void)
{
	if (!watchdog_active) {
		return (NULL);
	}
	return (watchdog_active->name);
}

int
watchdog_set_timeout(u32 timeout_sec)
{
	if (!watchdog_active || !watchdog_active->ops) {
		return (-1);
	}

	watchdog_clamp_timeout(watchdog_active, &timeout_sec);

	if (watchdog_active->ops->set_timeout) {
		if (watchdog_active->ops->set_timeout(
		    watchdog_active, timeout_sec) != 0) {
			return (-1);
		}
	}

	watchdog_active->timeout_sec = timeout_sec;
	if (watchdog_active->running) {
		watchdog_update_interval(timeout_sec);
	}
	return (0);
}

int
watchdog_start(u32 timeout_sec)
{
	if (!watchdog_active || !watchdog_active->ops) {
		return (-1);
	}

	watchdog_clamp_timeout(watchdog_active, &timeout_sec);

	if (watchdog_active->ops->start) {
		if (watchdog_active->ops->start(
		    watchdog_active, timeout_sec) != 0) {
			return (-1);
		}
	} else if (watchdog_active->ops->set_timeout) {
		if (watchdog_active->ops->set_timeout(
		    watchdog_active, timeout_sec) != 0) {
			return (-1);
		}
	}

	watchdog_active->timeout_sec = timeout_sec;
	watchdog_active->running = 1;
	watchdog_update_interval(timeout_sec);
	watchdog_last_ping_tick = timer_get_ticks();
	watchdog_auto_feed = 1;
	return (0);
}

int
watchdog_stop(void)
{
	if (!watchdog_active || !watchdog_active->ops) {
		return (-1);
	}
	if (watchdog_active->ops->stop) {
		if (watchdog_active->ops->stop(
		    watchdog_active) != 0) {
			return (-1);
		}
	}
	watchdog_active->running = 0;
	watchdog_auto_feed = 0;
	return (0);
}

int
watchdog_ping(void)
{
	if (!watchdog_active || !watchdog_active->ops ||
	    !watchdog_active->ops->ping) {
		return (-1);
	}
	return (watchdog_active->ops->ping(watchdog_active));
}

void
watchdog_tick(void)
{
	u64	now;

	if (!watchdog_active || !watchdog_active->running ||
	    !watchdog_auto_feed) {
		return;
	}

	if (watchdog_ping_interval_ticks == 0) {
		return;
	}

	now = timer_get_ticks();
	if (now - watchdog_last_ping_tick <
	    watchdog_ping_interval_ticks) {
		return;
	}

	if (watchdog_ping() != 0) {
		panic("[WDT] ping failed (device: %s)\n",
		    watchdog_active->name ?
		    watchdog_active->name : "unknown");
	}

	watchdog_last_ping_tick = now;
}
