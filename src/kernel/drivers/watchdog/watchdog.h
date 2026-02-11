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

#ifndef WATCHDOG_H
#define WATCHDOG_H

#include <kernel/pci/pci.h>
#include <mlibc/mlibc.h>

#define WATCHDOG_MAX_DEVICES 8
#define WATCHDOG_DEFAULT_TIMEOUT_SEC 30

typedef struct watchdog_device watchdog_device_t;

typedef struct watchdog_ops {
  int (*start)(watchdog_device_t *dev, u32 timeout_sec);
  int (*stop)(watchdog_device_t *dev);
  int (*ping)(watchdog_device_t *dev);
  int (*set_timeout)(watchdog_device_t *dev, u32 timeout_sec);
} watchdog_ops_t;

typedef struct watchdog_device {
  const char *name;
  u32 min_timeout_sec;
  u32 max_timeout_sec;
  u32 timeout_sec;
  int running;

  pci_device_t *pci_dev;
  void *priv;
  const watchdog_ops_t *ops;
} watchdog_device_t;

void watchdog_init(void);
int watchdog_is_initialized(void);

int watchdog_register_device(watchdog_device_t *dev);
int watchdog_device_count(void);
watchdog_device_t *watchdog_get_device(int index);

int watchdog_set_active(int index);
watchdog_device_t *watchdog_get_active(void);
const char *watchdog_get_active_name(void);

int watchdog_start(u32 timeout_sec);
int watchdog_stop(void);
int watchdog_ping(void);
int watchdog_set_timeout(u32 timeout_sec);
void watchdog_tick(void);

#endif
