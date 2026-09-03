/*
 * Copyright (c) 2026, Microsoft Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <arch_helpers.h>
#include <common/runtime_svc.h>
#include <event_measure.h>
#include <lib/smccc.h>
#include <lib/xlat_tables/xlat_tables_v2.h>
#include <plat/common/platform.h>
#include <platform_def.h>
#include <smccc_helpers.h>

#define MAP_DEVICE0_DRTM \
	MAP_REGION_FLAT(DEVICE0_BASE, DEVICE0_SIZE, MT_DEVICE | MT_RW | EL3_PAS)
#define MAP_DEVICE1_DRTM \
	MAP_REGION_FLAT(DEVICE1_BASE, DEVICE1_SIZE, MT_DEVICE | MT_RW | EL3_PAS)
#define MAP_NS_DRAM0_DRTM \
	MAP_REGION_FLAT(NS_DRAM0_BASE, QEMU_DRTM_NS_DRAM_SIZE, \
			MT_MEMORY | MT_RW | MT_NS)

#define QEMU_DRTM_ACPI_BLOB_MAGIC	U(0x50434144)
#define QEMU_DRTM_ACPI_BLOB_VERSION	U(1)
#define QEMU_DRTM_ACPI_MAX_TABLES	U(5)
#define QEMU_DRTM_ACPI_REGION_SIZE	U(0x4000)
#define QEMU_DRTM_ACPI_TABLE_ALIGNMENT	U(8)
#define QEMU_DRTM_NS_DRAM_SIZE		ULL(0x200000000)

#define QEMU_SIP_SVC_REGISTER_DRTM_ACPI_TABLES	U(0xC200011A)

#define QEMU_SIP_INVALID_PARAMETERS	(-2)
#define QEMU_SIP_INTERNAL_ERROR		(-5)
#define QEMU_SIP_INVALID_DATA		(-9)

#define ACPI_APIC_SIGNATURE	U(0x43495041)
#define ACPI_MCFG_SIGNATURE	U(0x4746434D)
#define ACPI_GTDT_SIGNATURE	U(0x54445447)
#define ACPI_IORT_SIGNATURE	U(0x54524F49)
#define ACPI_TPM2_SIGNATURE	U(0x324D5054)

typedef struct {
	uint32_t magic;
	uint32_t version;
	uint32_t table_count;
	uint32_t total_size;
} __packed qemu_drtm_acpi_blob_header_t;

typedef struct {
	char signature[4];
	uint32_t length;
	uint8_t revision;
	uint8_t checksum;
	char oem_id[6];
	char oem_table_id[8];
	uint32_t oem_revision;
	uint32_t creator_id;
	uint32_t creator_revision;
} __packed qemu_acpi_table_header_t;

static const qemu_acpi_table_header_t qemu_drtm_xsdt = {
	.signature = { 'X', 'S', 'D', 'T' },
	.length = sizeof(qemu_acpi_table_header_t),
	.revision = 1U,
	.oem_id = { 'Q', 'E', 'M', 'U', ' ', ' ' },
	.oem_table_id = { 'D', 'R', 'T', 'M', 'X', 'S', 'D', 'T' },
	.oem_revision = 1U,
	.creator_revision = 1U
};

static uint8_t qemu_drtm_acpi_blob[QEMU_DRTM_ACPI_REGION_SIZE];
static size_t qemu_drtm_acpi_blob_size;

static bool qemu_drtm_acpi_checksum_valid(const uint8_t *table, size_t size)
{
	uint8_t checksum = 0U;
	size_t index;

	for (index = 0U; index < size; ++index) {
		checksum += table[index];
	}

	return checksum == 0U;
}

static const mmap_region_t qemu_drtm_mmap[] = {
	MAP_DEVICE0_DRTM,
	MAP_DEVICE1_DRTM,
	MAP_NS_DRAM0_DRTM,
	{0}
};

static const plat_drtm_dma_prot_features_t qemu_drtm_dma_prot_features = {
	.max_num_mem_prot_regions = 0,
	.dma_protection_support = 0x1
};

static const plat_drtm_tpm_features_t qemu_drtm_tpm_features = {
	.tpm_based_hash_support = false,
	.firmware_hash_algorithm = TPM_ALG_ID
};

const mmap_region_t *plat_get_addr_mmap(void)
{
	return qemu_drtm_mmap;
}

bool plat_has_non_host_platforms(void)
{
	return true;
}

bool plat_has_unmanaged_dma_peripherals(void)
{
	return false;
}

unsigned int plat_get_total_smmus(void)
{
	return 0U;
}

void plat_enumerate_smmus(const uintptr_t **smmus_out, size_t *smmu_count_out)
{
	*smmus_out = NULL;
	*smmu_count_out = 0U;
}

const plat_drtm_dma_prot_features_t *plat_drtm_get_dma_prot_features(void)
{
	return &qemu_drtm_dma_prot_features;
}

uint64_t plat_drtm_dma_prot_get_max_table_bytes(void)
{
	return 0ULL;
}

const plat_drtm_tpm_features_t *plat_drtm_get_tpm_features(void)
{
	return &qemu_drtm_tpm_features;
}

uint64_t plat_drtm_get_min_size_normal_world_dce(void)
{
	return 0ULL;
}

uint64_t plat_drtm_get_tcb_hash_table_size(void)
{
	return 0ULL;
}

uint64_t plat_drtm_get_imp_def_dlme_region_size(void)
{
	return 0ULL;
}

uint64_t plat_drtm_get_tcb_hash_features(void)
{
	return 0ULL;
}

uint64_t plat_drtm_get_acpi_tables_region_size(void)
{
	return QEMU_DRTM_ACPI_REGION_SIZE;
}

void plat_drtm_get_acpi_tables(void *acpi_tables_out,
			       size_t acpi_tables_size,
			       uintptr_t acpi_tables_paddr)
{
	qemu_acpi_table_header_t *xsdt = acpi_tables_out;
	const qemu_drtm_acpi_blob_header_t *blob;
	const qemu_acpi_table_header_t *source;
	uint8_t *xsdt_entries;
	uint8_t *destination;
	uint8_t checksum = 0U;
	uint8_t *current;
	uint64_t xsdt_entry;
	size_t index;
	size_t offset;
	size_t output_size;

	assert(acpi_tables_out != NULL);
	assert(acpi_tables_size == QEMU_DRTM_ACPI_REGION_SIZE);

	memset(acpi_tables_out, 0, acpi_tables_size);
	memcpy(xsdt, &qemu_drtm_xsdt, sizeof(*xsdt));
	if (qemu_drtm_acpi_blob_size != 0U) {
		blob = (const qemu_drtm_acpi_blob_header_t *)qemu_drtm_acpi_blob;
		xsdt->length = sizeof(*xsdt) +
			(blob->table_count * sizeof(uint64_t));
		xsdt_entries = (uint8_t *)(xsdt + 1);
		output_size = round_up(xsdt->length,
			QEMU_DRTM_ACPI_TABLE_ALIGNMENT);
		offset = sizeof(*blob);

		for (index = 0U; index < blob->table_count; ++index) {
			source = (const qemu_acpi_table_header_t *)
				(qemu_drtm_acpi_blob + offset);
			output_size = round_up(output_size,
				QEMU_DRTM_ACPI_TABLE_ALIGNMENT);
			destination = (uint8_t *)acpi_tables_out + output_size;
			xsdt_entry = acpi_tables_paddr + output_size;
			memcpy(xsdt_entries + (index * sizeof(xsdt_entry)),
			       &xsdt_entry, sizeof(xsdt_entry));
			memcpy(destination, source, source->length);
			output_size += source->length;
			offset += source->length;
		}
	}

	current = (uint8_t *)xsdt;
	for (index = 0U; index < xsdt->length; ++index) {
		checksum += current[index];
	}
	xsdt->checksum = (uint8_t)(0U - checksum);
}

static int qemu_drtm_register_acpi_tables(const void *acpi_tables,
					  size_t acpi_tables_size)
{
	const qemu_drtm_acpi_blob_header_t *blob = acpi_tables;
	const qemu_acpi_table_header_t *table;
	uint32_t signature;
	uint32_t signatures = 0U;
	size_t index;
	size_t offset;
	size_t output_size;

	if (acpi_tables == NULL ||
	    acpi_tables_size < sizeof(*blob) ||
	    acpi_tables_size > sizeof(qemu_drtm_acpi_blob) ||
	    blob->magic != QEMU_DRTM_ACPI_BLOB_MAGIC ||
	    blob->version != QEMU_DRTM_ACPI_BLOB_VERSION ||
	    blob->table_count < 4U ||
	    blob->table_count > QEMU_DRTM_ACPI_MAX_TABLES ||
	    blob->total_size != acpi_tables_size) {
		return -1;
	}

	offset = sizeof(*blob);
	output_size = round_up(sizeof(qemu_drtm_xsdt) +
		(blob->table_count * sizeof(uint64_t)),
		QEMU_DRTM_ACPI_TABLE_ALIGNMENT);
	for (index = 0U; index < blob->table_count; ++index) {
		if (offset > acpi_tables_size ||
		    acpi_tables_size - offset < sizeof(*table)) {
			return -1;
		}
		table = (const qemu_acpi_table_header_t *)
			((const uint8_t *)acpi_tables + offset);
		if (table->length < sizeof(*table) ||
		    table->length > acpi_tables_size - offset ||
		    !qemu_drtm_acpi_checksum_valid((const uint8_t *)table,
						 table->length)) {
			return -1;
		}
		output_size = round_up(output_size,
			QEMU_DRTM_ACPI_TABLE_ALIGNMENT);
		if (output_size > QEMU_DRTM_ACPI_REGION_SIZE ||
		    table->length > QEMU_DRTM_ACPI_REGION_SIZE - output_size) {
			return -1;
		}
		output_size += table->length;

		memcpy(&signature, table->signature, sizeof(signature));
		switch (signature) {
		case ACPI_APIC_SIGNATURE:
			signatures |= BIT(0);
			break;
		case ACPI_MCFG_SIGNATURE:
			signatures |= BIT(1);
			break;
		case ACPI_GTDT_SIGNATURE:
			signatures |= BIT(2);
			break;
		case ACPI_IORT_SIGNATURE:
			signatures |= BIT(3);
			break;
		case ACPI_TPM2_SIGNATURE:
			signatures |= BIT(4);
			break;
		default:
			return -1;
		}
		offset += table->length;
	}

	if (offset != acpi_tables_size ||
	    (signatures & U(0xF)) != U(0xF) ||
	    __builtin_popcount(signatures) != blob->table_count) {
		return -1;
	}

	memcpy(qemu_drtm_acpi_blob, acpi_tables, acpi_tables_size);
	qemu_drtm_acpi_blob_size = acpi_tables_size;
	return 0;
}

uint64_t plat_drtm_get_dlme_img_auth_features(void)
{
	return 0ULL;
}

int plat_set_drtm_error(uint64_t error_code)
{
	(void)error_code;
	return 0;
}

int plat_get_drtm_error(uint64_t *error_code)
{
	*error_code = 0ULL;
	return 0;
}

int plat_drtm_validate_ns_region(uintptr_t region_start, size_t region_size)
{
	uintptr_t region_end;

	if ((region_size == 0U) ||
	    __builtin_add_overflow(region_start, region_size - 1U, &region_end)) {
		return -1;
	}

	if ((region_start < NS_DRAM0_BASE) ||
	    (region_end >= (NS_DRAM0_BASE + NS_DRAM0_SIZE))) {
		return -1;
	}

	return 0;
}

static uintptr_t qemu_drtm_sip_handler(uint32_t smc_fid,
				       u_register_t x1,
				       u_register_t x2,
				       u_register_t x3,
				       u_register_t x4,
				       void *cookie,
				       void *handle,
				       u_register_t flags)
{
	uintptr_t va_mapping;
	size_t va_mapping_size;
	int rc;

	(void)x3;
	(void)x4;
	(void)cookie;

	if (!is_caller_non_secure(flags) ||
	    smc_fid != QEMU_SIP_SVC_REGISTER_DRTM_ACPI_TABLES) {
		SMC_RET1(handle, SMC_UNK);
	}

	if (((x1 % PAGE_SIZE_4KB) != 0U) || (x2 == 0U) ||
	    (x2 > QEMU_DRTM_ACPI_REGION_SIZE)) {
		SMC_RET1(handle, QEMU_SIP_INVALID_PARAMETERS);
	}

	va_mapping_size = round_up((size_t)x2, PAGE_SIZE_4KB);
	rc = plat_drtm_validate_ns_region(x1, va_mapping_size);
	if (rc != 0) {
		SMC_RET1(handle, QEMU_SIP_INVALID_PARAMETERS);
	}

	rc = mmap_add_dynamic_region_alloc_va(x1, &va_mapping,
		va_mapping_size, MT_NS | MT_RO_DATA | MT_SHAREABILITY_ISH);
	if (rc != 0) {
		SMC_RET1(handle, QEMU_SIP_INTERNAL_ERROR);
	}

	flush_dcache_range(va_mapping, va_mapping_size);
	rc = qemu_drtm_register_acpi_tables((const void *)va_mapping, x2);

	if (mmap_remove_dynamic_region(va_mapping, va_mapping_size) != 0) {
		panic();
	}

	SMC_RET1(handle, rc == 0 ? SMC_OK : QEMU_SIP_INVALID_DATA);
}

DECLARE_RT_SVC(
	qemu_drtm_sip_svc,
	OEN_SIP_START,
	OEN_SIP_END,
	SMC_TYPE_FAST,
	NULL,
	qemu_drtm_sip_handler
);
