/*
 * Copyright (c) 2022, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */


#include <stdint.h>

#include <plat/common/platform.h>
#include <services/drtm_svc.h>
#include <drivers/auth/crypto_mod.h>
#include "../common/qemu_private.h"

/*
 * This file contains DRTM platform functions which don't really do anything on
 * FVP but are needed for DRTM to function.
 */

uint64_t plat_drtm_get_min_size_normal_world_dce(void)
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

uint64_t plat_drtm_get_tcb_hash_table_size(void)
{
	return 0ULL;
}

// SMMU functions

unsigned int plat_get_total_smmus(void)
{
	return 0;
}

void plat_enumerate_smmus(const uintptr_t **smmus_out,
													size_t *smmu_count_out)
{
	*smmus_out = NULL;
	*smmu_count_out = 0;
}

//

const mmap_region_t *plat_get_addr_mmap(void)
{
	return plat_qemu_get_mmap();
}

/* Note-LPT:
 * From looking at the docs and source code, I have not found clues whether this
 * platform has DMA-capable devices, and if it does, whether their memory
 * accesses necessarily go through IOMMU translation (SMMUv3).  Therefore,
 * for now assume there are no DMA-capable devices and therefore no (useful)
 * SMMUs.  This is a supported DRTM case whereby full DMA protection is still
 * advertised.
 */
bool plat_has_unmanaged_dma_peripherals(void)
{
	return false;
}

/* DRTM DMA Protection Features */
static const plat_drtm_dma_prot_features_t dma_prot_features = {
	.max_num_mem_prot_regions = 0, /* No protection regions are present */
	.dma_protection_support = 0x1 /* Complete DMA protection only */
};

const plat_drtm_dma_prot_features_t *plat_drtm_get_dma_prot_features(void)
{
	return &dma_prot_features;
}

uint64_t plat_drtm_dma_prot_get_max_table_bytes(void)
{
	return 0U;
}

bool plat_has_non_host_platforms(void)
{
	/* QEMU base platforms typically have GPU, as per FVP Reference guide */
	return true;
}

/* DRTM TPM Features */
static const plat_drtm_tpm_features_t tpm_features = {
	/* No TPM-based hashing supported. */
	.tpm_based_hash_support = false,

	/* Set to decided algorithm by Event Log driver */
	.firmware_hash_algorithm = 0x000B

};

const plat_drtm_tpm_features_t *plat_drtm_get_tpm_features(void)
{
	return &tpm_features;
}

int plat_drtm_validate_ns_region(uintptr_t region_start,
				 size_t region_size)
{
	return 0; // TODO?
}


int plat_set_drtm_error(uint64_t error_code)
{
	/* TODO: Set DRTM error in NV-storage */
	return 0;
}

int plat_get_drtm_error(uint64_t *error_code)
{
	/* TODO: Get DRTM error from NV-storage */
	*error_code = 0;
	return 0;
}

void __dead2 plat_system_reset(void)
{
	while(1) {};
}



