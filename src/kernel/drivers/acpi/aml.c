/*
 * Copyright (c) 2026, otsos team
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
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
$define %type acpi_sdt_header_t as packed struct with common SDT header
$define %type acpi_fadt_t as packed struct with FADT fields

$define %func aml_init as function with args void
$define %func aml_is_initialized as function with args void

*/

/* !SPACE!

$space %internal aml_table_body, aml_load_sdt, aml_load_extra
$space %internal aml_locate_dsdt, aml_run_ini
$space %export aml_init, aml_is_initialized

*/

#include <kernel/drivers/acpi/amlint.h>
#include <kernel/drivers/acpi/acpi.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>

#define	AML_FADT_MIN_LENGTH_X_DSDT	(__builtin_offsetof(acpi_fadt_t, \
					    x_dsdt) + sizeof(u64))
#define	AML_SDT_HEADER_SIZE		sizeof(acpi_sdt_header_t)

static int	aml_initialized;
static u32	aml_tables_loaded;

static int
aml_table_body(acpi_sdt_header_t *header, const u8 **aml, u32 *length)
{
	if (header == NULL || header->length <= AML_SDT_HEADER_SIZE) {
		return (AML_ERR);
	}
	*aml = (const u8 *)header + AML_SDT_HEADER_SIZE;
	*length = header->length - (u32)AML_SDT_HEADER_SIZE;
	return (AML_OK);
}

static int
aml_load_sdt(acpi_sdt_header_t *header, void *ctx)
{
	const u8	*aml;
	u32		length;
	int		status;

	(void)ctx;
	if (aml_table_body(header, &aml, &length) != AML_OK) {
		drivers_log("aml: table %c%c%c%c has no body\n",
		    header->signature[0], header->signature[1],
		    header->signature[2], header->signature[3]);
		return (0);
	}
	status = aml_load_table(aml, length);
	if (status != AML_OK) {
		drivers_log("aml: %c%c%c%c load failed (%d)\n",
		    header->signature[0], header->signature[1],
		    header->signature[2], header->signature[3], status);
		return (0);
	}
	aml_tables_loaded++;
	return (0);
}

static acpi_sdt_header_t *
aml_locate_dsdt(void)
{
	acpi_fadt_t		*fadt;
	acpi_sdt_header_t	*dsdt;

	dsdt = NULL;
	fadt = acpi_get_fadt();
	if (fadt != NULL) {
		if (fadt->header.length >= AML_FADT_MIN_LENGTH_X_DSDT &&
		    fadt->x_dsdt != 0) {
			dsdt = (acpi_sdt_header_t *)fadt->x_dsdt;
		} else if (fadt->dsdt != 0) {
			dsdt = (acpi_sdt_header_t *)(u64)fadt->dsdt;
		}
	}
	if (dsdt == NULL) {
		dsdt = acpi_find_table("DSDT");
	}
	if (dsdt == NULL) {
		return (NULL);
	}
	if (dsdt->signature[0] != 'D' || dsdt->signature[1] != 'S' ||
	    dsdt->signature[2] != 'D' || dsdt->signature[3] != 'T') {
		return (NULL);
	}
	return (dsdt);
}

static int
aml_run_ini(aml_node_t *node, void *ctx)
{
	aml_object_t	*result;
	aml_node_t	*method;
	u32		*count;

	count = ctx;
	if (node->object == NULL) {
		return (AML_OK);
	}
	if (node->object->type != AML_TYPE_DEVICE &&
	    node->object->type != AML_TYPE_THERMAL &&
	    node->object->type != AML_TYPE_PROCESSOR) {
		return (AML_OK);
	}
	method = aml_node_child(node, "_INI");
	if (method == NULL || method->object == NULL ||
	    method->object->type != AML_TYPE_METHOD) {
		return (AML_OK);
	}
	if ((aml_node_status(node) & AML_STA_PRESENT) == 0) {
		return (AML_OK);
	}
	result = NULL;
	if (aml_evaluate(method, NULL, 0, &result) == AML_OK) {
		(*count)++;
	}
	aml_object_unref(result);
	return (AML_OK);
}

int
aml_init(void)
{
	acpi_sdt_header_t	*dsdt;
	const u8		*aml;
	u32			length;
	u32			initialized;
	int			status;

	if (aml_initialized) {
		return (AML_OK);
	}
	if (!acpi_is_initialized()) {
		drivers_log("aml: ACPI tables not available\n");
		return (AML_ERR);
	}
	dsdt = aml_locate_dsdt();
	if (dsdt == NULL) {
		drivers_log("aml: DSDT not found\n");
		return (AML_ERR_NOT_FOUND);
	}
	if (aml_table_body(dsdt, &aml, &length) != AML_OK) {
		drivers_log("aml: DSDT body invalid\n");
		return (AML_ERR);
	}
	aml_set_integer_width(dsdt->revision);
	status = aml_namespace_init();
	if (status != AML_OK) {
		drivers_log("aml: namespace init failed (%d)\n", status);
		return (status);
	}
	status = aml_load_table(aml, length);
	if (status != AML_OK) {
		drivers_log("aml: DSDT load failed (%d)\n", status);
		return (status);
	}
	aml_tables_loaded = 1;
	(void)acpi_table_foreach("SSDT", aml_load_sdt, NULL);
	(void)acpi_table_foreach("PSDT", aml_load_sdt, NULL);
	aml_initialized = 1;
	initialized = 0;
	(void)aml_walk(NULL, aml_run_ini, &initialized);
	drivers_log("aml: %u table(s), rev %u, %u-bit integers, "
	    "%u _INI method(s)\n", aml_tables_loaded, dsdt->revision,
	    (u32)aml_get_integer_width() * 8, initialized);
	return (AML_OK);
}

int
aml_is_initialized(void)
{
	return (aml_initialized);
}
