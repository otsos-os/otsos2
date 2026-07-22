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

#include <kernel/drivers/input/i8042.h>
#include <mlibc/mlibc.h>

#define	I8042_DATA_PORT		0x60
#define	I8042_STATUS_PORT	0x64
#define	I8042_CMD_PORT		0x64
#define	I8042_RFLAGS_IF		0x200

u64
i8042_irq_save(void)
{
	u64	flags;

	__asm__ volatile("pushfq; popq %0; cli"
	    : "=r"(flags)
	    :
	    : "memory");
	return (flags);
}

void
i8042_irq_restore(u64 flags)
{
	if (flags & I8042_RFLAGS_IF) {
		__asm__ volatile("sti" ::: "memory");
	}
}

u8
i8042_status(void)
{
	return (inb(I8042_STATUS_PORT));
}

int
i8042_wait_input_clear(void)
{
	u32	i;

	for (i = 0; i < 100000; i++) {
		if ((i8042_status() & I8042_STATUS_IBF) == 0) {
			return (0);
		}
	}
	return (-1);
}

int
i8042_wait_output_full(void)
{
	u32	i;

	for (i = 0; i < 100000; i++) {
		if (i8042_status() & I8042_STATUS_OBF) {
			return (0);
		}
	}
	return (-1);
}

void
i8042_flush_output(void)
{
	u32	i;

	for (i = 0; i < 1024; i++) {
		if ((i8042_status() & I8042_STATUS_OBF) == 0) {
			return;
		}
		(void)inb(I8042_DATA_PORT);
	}
}

int
i8042_read_data(u8 *data)
{
	if (i8042_wait_output_full() != 0) {
		return (-1);
	}
	*data = inb(I8042_DATA_PORT);
	return (0);
}

int
i8042_read_aux(u8 *data)
{
	u32	i;
	u8	status;

	for (i = 0; i < 100000; i++) {
		status = i8042_status();
		if ((status & I8042_STATUS_OBF) == 0) {
			continue;
		}
		if ((status & I8042_STATUS_AUX) == 0) {
			(void)inb(I8042_DATA_PORT);
			continue;
		}
		*data = inb(I8042_DATA_PORT);
		return (0);
	}
	return (-1);
}

int
i8042_write_cmd(u8 cmd)
{
	if (i8042_wait_input_clear() != 0) {
		return (-1);
	}
	outb(I8042_CMD_PORT, cmd);
	return (0);
}

int
i8042_write_data(u8 data)
{
	if (i8042_wait_input_clear() != 0) {
		return (-1);
	}
	outb(I8042_DATA_PORT, data);
	return (0);
}

int
i8042_write_aux(u8 data)
{
	if (i8042_write_cmd(I8042_CMD_WRITE_AUX) != 0) {
		return (-1);
	}
	return (i8042_write_data(data));
}

int
i8042_read_config(u8 *config)
{
	if (i8042_write_cmd(I8042_CMD_READ_CONFIG) != 0) {
		return (-1);
	}
	return (i8042_read_data(config));
}

int
i8042_write_config(u8 config)
{
	if (i8042_write_cmd(I8042_CMD_WRITE_CONFIG) != 0) {
		return (-1);
	}
	return (i8042_write_data(config));
}

int
i8042_test_port2(u8 *result)
{
	if (i8042_write_cmd(I8042_CMD_TEST_PORT2) != 0) {
		return (-1);
	}
	return (i8042_read_data(result));
}
