#
# Copyright (c) 2019-2023, Linaro Limited and Contributors. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

PLAT_QEMU_PATH		:=	plat/qemu/qemu_sbsa
PLAT_QEMU_COMMON_PATH	:=	plat/qemu/common

CRASH_REPORTING		:= 1

# Disable the PSCI platform compatibility layer
ENABLE_PLAT_COMPAT	:= 0

SEPARATE_CODE_AND_RODATA := 1
ENABLE_STACK_PROTECTOR	 := 0

ifeq (${SPM_MM},1)
NEED_BL32		:=	yes
EL3_EXCEPTION_HANDLING	:=	1
endif

include plat/qemu/common/common.mk

# Enable new version of image loading on QEMU platforms
LOAD_IMAGE_V2		:=	1

ifeq ($(NEED_BL32),yes)
$(eval $(call add_define,QEMU_LOAD_BL32))
endif

ifeq (${SPMD_SPM_AT_SEL2}, 1)
FDT_SOURCES		+=  ${PLAT_QEMU_PATH}/fdts/${PLAT}_tb_fw_config.dts
BL32_CONFIG_DTS		:=	${PLAT_QEMU_PATH}/fdts/${PLAT}_spmc_sp_manifest.dts
FDT_SOURCES		+=	${BL32_CONFIG_DTS}
QEMU_SBSA_TOS_FW_CONFIG		:=	${BUILD_PLAT}/fdts/$(notdir $(basename ${BL32_CONFIG_DTS})).dtb
endif

# Include GICv3 driver files
include drivers/arm/gic/v3/gicv3.mk

QEMU_GIC_SOURCES	:=	${GICV3_SOURCES}				\
				plat/common/plat_gicv3.c

BL31_SOURCES		+=	${PLAT_QEMU_PATH}/sbsa_gic.c 			\
				${PLAT_QEMU_PATH}/sbsa_platform.c		\
				${PLAT_QEMU_PATH}/sbsa_pm.c			\
				${PLAT_QEMU_PATH}/sbsa_sip_svc.c		\
				${PLAT_QEMU_PATH}/sbsa_topology.c

BL31_SOURCES		+=	${FDT_WRAPPERS_SOURCES}

ifeq (${SPM_MM},1)
	BL31_SOURCES		+=	${PLAT_QEMU_COMMON_PATH}/qemu_spm.c
endif

ifeq ($(DRTM_SUPPORT), 1)
		BL31_SOURCES		+= \
				drivers/arm/smmu/smmu_v3.c	\
				plat/qemu/qemu/qemu_drtm_stub.c \
				drivers/delay_timer/delay_timer.c	\
				drivers/delay_timer/generic_delay_timer.c

		MEASURED_BOOT_MK := drivers/measured_boot/event_log/event_log.mk
    $(info Including ${MEASURED_BOOT_MK})
    include ${MEASURED_BOOT_MK}

		BL31_SOURCES	        += 	${EVENT_LOG_SOURCES}

		PLAT_INCLUDES		+=	-Iinclude/drivers/auth/mbedtls


    CRYPTO_SOURCES	:=	drivers/auth/crypto_mod.c 	\
				lib/fconf/fconf_tbbr_getter.c
    BL1_SOURCES		+=	drivers/auth/crypto_mod.c
    BL2_SOURCES		+=	drivers/auth/crypto_mod.c
		BL31_SOURCES	+=	drivers/auth/crypto_mod.c

    # We expect to locate the *.mk files under the directories specified below
    CRYPTO_LIB_MK := drivers/auth/mbedtls/mbedtls_crypto.mk

    $(info Including ${CRYPTO_LIB_MK})
    include ${CRYPTO_LIB_MK}
endif

# Include Measured Boot makefile before any Crypto library makefile.
# Crypto library makefile may need default definitions of Measured Boot build
# flags present in Measured Boot makefile.
ifeq (${MEASURED_BOOT},1)
    MEASURED_BOOT_MK := drivers/measured_boot/event_log/event_log.mk
    $(info Including ${MEASURED_BOOT_MK})
    include ${MEASURED_BOOT_MK}

    BL2_SOURCES		+=	plat/qemu/qemu/qemu_measured_boot.c	\
				plat/qemu/qemu/qemu_helpers.c		\
				${EVENT_LOG_SOURCES}

     BL1_SOURCES	+=      plat/qemu/qemu/qemu_bl1_measured_boot.c

endif

ifeq (${SPMD_SPM_AT_SEL2}, 1)
BL1_SOURCES += plat/common/plat_spmd_manifest.c

BL2_SOURCES += ${PLAT_QEMU_COMMON_PATH}/qemu_io_storage.c \
				common/uuid.c

BL31_SOURCES += plat/common/plat_spmd_manifest.c

TOS_FW_CONFIG		:=	${BUILD_PLAT}/fdts/qemu_sbsa_spmc_sp_manifest.dtb
$(eval $(call TOOL_ADD_PAYLOAD,${TOS_FW_CONFIG},--tos-fw-config,${TOS_FW_CONFIG}))
TB_FW_CONFIG		:=	${BUILD_PLAT}/fdts/qemu_sbsa_tb_fw_config.dtb
$(eval $(call TOOL_ADD_PAYLOAD,${TB_FW_CONFIG},--tb-fw-config,${TB_FW_CONFIG}))
endif

# Use known base for UEFI if not given from command line
# By default BL33 is at FLASH1 base
PRELOADED_BL33_BASE	?= 0x10000000

# Qemu SBSA plafrom only support SEC_SRAM
BL32_RAM_LOCATION_ID	= SEC_SRAM_ID
$(eval $(call add_define,BL32_RAM_LOCATION_ID))

# Don't have the Linux kernel as a BL33 image by default
ARM_LINUX_KERNEL_AS_BL33	:=	0
$(eval $(call assert_boolean,ARM_LINUX_KERNEL_AS_BL33))
$(eval $(call add_define,ARM_LINUX_KERNEL_AS_BL33))

ARM_PRELOADED_DTB_BASE := PLAT_QEMU_DRAM0_BASE
$(eval $(call add_define,ARM_PRELOADED_DTB_BASE))
