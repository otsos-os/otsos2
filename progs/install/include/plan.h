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

/* !DEFINES!

$define %type char as 8 bit signed
$define %type uint32_t as 32 bit unsigned
$define %type uint64_t as 64 bit unsigned

*/

/* !SPACE!

$space %export INST_ROLE_KERNEL, INST_ROLE_CMSEED
$space %export INST_ROLE_STAGE1, INST_ROLE_STAGE2, INST_ROLE_UEFI

*/

#ifndef INSTALL_PLAN_H
#define INSTALL_PLAN_H

#include <stdint.h>


#define INST_REG_HIVE		"BOOT"
#define INST_REG_MODULES	"Modules"
#define INST_REG_DEST		"Dest"
#define INST_REG_ROLE		"Role"
#define INST_ROLE_KERNEL	"kernel"
#define INST_ROLE_CMSEED	"cmseed"
#define INST_ROLE_STAGE1	"bios-stage1"
#define INST_ROLE_STAGE2	"bios-stage2"
#define INST_ROLE_UEFI		"uefi-loader"
#define INST_MODULES_MAX	256
#define INST_TREES_MAX		32
#define INST_BIOS_BYTES		(1ULL * 1024ULL * 1024ULL)
#define INST_ESP_BYTES		(128ULL * 1024ULL * 1024ULL)
#define INST_MIN_BYTES		(512ULL * 1024ULL * 1024ULL)
#define INST_ROOT_MAX_FILES	8192U
#define INST_NAME_BIOS		"otsos-boot"
#define INST_NAME_ESP		"otsos-esp"
#define INST_NAME_ROOT		"otsos-root"
#define INST_LABEL_ESP		"OTSOSESP"
#define INST_ESP_DIRS		{ "/EFI", "/EFI/BOOT" }
#define INST_ESP_DIRS_MAX	4
#define INST_ESP_BOOTX64	"/EFI/BOOT/BOOTX64.EFI"
#define INST_CHUNK_CAP		(256U * 1024U)

#endif
