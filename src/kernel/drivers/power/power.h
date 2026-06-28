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

$define %type u32 as 32 bit unsigned
$define %type u16 as 16 bit unsigned
$define %type u8 as 8 bit unsigned
$define %type int as 32 bit signed
$define %type power_state_t as enum with power states S0-S5
$define %type power_method_t as enum with shutdown/reboot methods
$define %type power_info_t as struct with PM register info and flags

$define %func power_init as function with args void
$define %func power_shutdown as procedure with args void
$define %func power_reboot as procedure with args void
$define %func power_acpi_enable as function with args void
$define %func power_get_info as function with args void
$define %func power_is_initialized as function with args void
$define %func power_register_shutdown_hook as function with args void (*)(void)
$define %func power_controller_shutdown as procedure with args void
$define %func power_controller_reboot as procedure with args void
$define %func power_controller_status as procedure with args void

*/

/* !SPACE!

$space %export power_init, power_shutdown, power_reboot
$space %export power_acpi_enable, power_get_info, power_is_initialized
$space %export power_register_shutdown_hook
$space %export power_controller_shutdown, power_controller_reboot
$space %export power_controller_status

*/

#ifndef POWER_H
#define POWER_H

#include <mlibc/mlibc.h>

typedef enum {
	POWER_STATE_S0 = 0,
	POWER_STATE_S1 = 1,
	POWER_STATE_S3 = 3,
	POWER_STATE_S4 = 4,
	POWER_STATE_S5 = 5,
} power_state_t;

typedef enum {
	POWER_METHOD_ACPI,
	POWER_METHOD_KEYBOARD,
	POWER_METHOD_TRIPLE,
	POWER_METHOD_RESET_REG,
} power_method_t;

typedef struct {
	int	initialized;
	int	acpi_available;
	int	reset_reg_available;
	u16	pm1a_control;
	u16	pm1b_control;
	u16	slp_typa_s5;
	u16	slp_typb_s5;
	u32	smi_command_port;
	u8	acpi_enable_value;
} power_info_t;

int	power_init(void);
void	power_shutdown(void);
void	power_reboot(void);
int	power_acpi_enable(void);
power_info_t power_get_info(void);
int	power_is_initialized(void);

int	power_register_shutdown_hook(void (*fn)(void));
void	power_controller_shutdown(void);
void	power_controller_reboot(void);
void	power_controller_status(void);

#endif
