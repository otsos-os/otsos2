/*
 * Copyright (c) 2026, otsos team
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
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

#ifndef KERNEL_DRIVERS_INPUT_I8042_H
#define KERNEL_DRIVERS_INPUT_I8042_H

#include <mlibc/mlibc.h>

#define	I8042_STATUS_OBF		0x01
#define	I8042_STATUS_IBF		0x02
#define	I8042_STATUS_AUX		0x20

#define	I8042_CMD_READ_CONFIG		0x20
#define	I8042_CMD_WRITE_CONFIG		0x60
#define	I8042_CMD_SELF_TEST		0xAA
#define	I8042_CMD_DISABLE_PORT1		0xAD
#define	I8042_CMD_ENABLE_PORT1		0xAE
#define	I8042_CMD_DISABLE_PORT2		0xA7
#define	I8042_CMD_ENABLE_PORT2		0xA8
#define	I8042_CMD_TEST_PORT2		0xA9
#define	I8042_CMD_WRITE_AUX		0xD4

#define	I8042_CONFIG_PORT1_IRQ		0x01
#define	I8042_CONFIG_PORT2_IRQ		0x02
#define	I8042_CONFIG_PORT1_CLOCK		0x10
#define	I8042_CONFIG_PORT2_CLOCK		0x20
#define	I8042_CONFIG_TRANSLATION		0x40
typedef void	(*i8042_sink_t)(u8 data);

u64	i8042_irq_save(void);
void	i8042_irq_restore(u64 flags);
u8	i8042_status(void);
void	i8042_set_kbd_sink(i8042_sink_t sink);
void	i8042_set_aux_sink(i8042_sink_t sink);
#define	I8042_DISPATCH_KBD_UNIT		0x10000U
u32	i8042_dispatch(void);
void	i8042_cmd_begin(void);
void	i8042_cmd_end(void);
int	i8042_wait_input_clear(void);
int	i8042_wait_output_full(void);
void	i8042_flush_output(void);
int	i8042_read_data(u8 *data);
int	i8042_read_aux(u8 *data);
int	i8042_write_cmd(u8 cmd);
int	i8042_write_data(u8 data);
int	i8042_write_aux(u8 data);
int	i8042_read_config(u8 *config);
int	i8042_write_config(u8 config);
int	i8042_test_port2(u8 *result);

#endif
