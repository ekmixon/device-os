TARGET_AMBD_SDK_SRC_PATH = $(AMBD_SDK_MODULE_PATH)/ambd_sdk
TARGET_AMBD_SDK_SRC_SOC_PATH = $(TARGET_AMBD_SDK_PATH)/ambd_sdk/component/soc/realtek/amebad
TARGET_AMBD_SDK_SRC_OS_PATH = $(TARGET_AMBD_SDK_PATH)/ambd_sdk/component/os

#CSRC += $(TARGET_AMBD_SDK_SRC_SOC_PATH)/fwlib/ram_common/rtl8721d_qdec.c
CSRC += $(TARGET_AMBD_SDK_SRC_SOC_PATH)/fwlib/ram_common/rtl8721d_i2c.c
CSRC += $(TARGET_AMBD_SDK_SRC_SOC_PATH)/fwlib/ram_common/rtl8721d_uart.c
CSRC += $(TARGET_AMBD_SDK_SRC_SOC_PATH)/fwlib/ram_common/rtl8721d_captouch.c
CSRC += $(TARGET_AMBD_SDK_SRC_SOC_PATH)/fwlib/ram_common/rtl8721d_pll.c
CSRC += $(TARGET_AMBD_SDK_SRC_SOC_PATH)/fwlib/ram_common/rtl8721d_tim.c
CSRC += $(TARGET_AMBD_SDK_SRC_SOC_PATH)/fwlib/ram_common/rtl8721d_bor.c
CSRC += $(TARGET_AMBD_SDK_SRC_SOC_PATH)/fwlib/ram_common/rtl8721d_adc.c
CSRC += $(TARGET_AMBD_SDK_SRC_SOC_PATH)/fwlib/ram_common/rtl8721d_sgpio.c
#CSRC += $(TARGET_AMBD_SDK_SRC_SOC_PATH)/fwlib/ram_common/rtl8721d_gdma_memcpy.c
CSRC += $(TARGET_AMBD_SDK_SRC_SOC_PATH)/fwlib/ram_common/rtl8721d_keyscan.c
CSRC += $(TARGET_AMBD_SDK_SRC_SOC_PATH)/fwlib/ram_common/rtl8721d_ir.c
CSRC += $(TARGET_AMBD_SDK_SRC_SOC_PATH)/fwlib/ram_common/rtl8721d_efuse.c
#CSRC += $(TARGET_AMBD_SDK_SRC_SOC_PATH)/fwlib/ram_common/rtl8721d_ram_libc.c
CSRC += $(TARGET_AMBD_SDK_SRC_SOC_PATH)/fwlib/ram_common/rtl8721d_comparator.c
CSRC += $(TARGET_AMBD_SDK_SRC_SOC_PATH)/fwlib/ram_common/rtl8721d_rtc.c
CSRC += $(TARGET_AMBD_SDK_SRC_SOC_PATH)/fwlib/ram_common/rtl8721d_flash_ram.c
#CSRC += $(TARGET_AMBD_SDK_SRC_SOC_PATH)/fwlib/ram_common/rtl8721d_ipc_api.c
CSRC += $(TARGET_AMBD_SDK_SRC_SOC_PATH)/fwlib/ram_common/rtl8721d_wdg.c

ifneq ("$(ARM_CPU)","cortex-m23")
# KM4 (Cortex-M33)
CSRC += $(TARGET_AMBD_SDK_SRC_SOC_PATH)/fwlib/ram_hp/rtl8721dhp_ota_ram.c
CSRC += $(TARGET_AMBD_SDK_SRC_SOC_PATH)/fwlib/ram_hp/rtl8721dhp_audio.c
CSRC += $(TARGET_AMBD_SDK_SRC_SOC_PATH)/fwlib/ram_hp/rtl8721dhp_sdio_host.c
#CSRC += $(TARGET_AMBD_SDK_SRC_SOC_PATH)/fwlib/ram_hp/rtl8721dhp_simulation.c
CSRC += $(TARGET_AMBD_SDK_SRC_SOC_PATH)/fwlib/ram_hp/rtl8721dhp_system.c
CSRC += $(TARGET_AMBD_SDK_SRC_SOC_PATH)/fwlib/ram_hp/rtl8721dhp_startup.c
CSRC += $(TARGET_AMBD_SDK_SRC_SOC_PATH)/fwlib/ram_hp/rtl8721dhp_usi_ssi.c
CSRC += $(TARGET_AMBD_SDK_SRC_SOC_PATH)/fwlib/ram_hp/rtl8721dhp_psram.c
#CSRC += $(TARGET_AMBD_SDK_SRC_SOC_PATH)/fwlib/ram_hp/rtl8721dhp_sd.c
CSRC += $(TARGET_AMBD_SDK_SRC_SOC_PATH)/fwlib/ram_hp/rtl8721dhp_clk.c
CSRC += $(TARGET_AMBD_SDK_SRC_SOC_PATH)/fwlib/ram_hp/rtl8721dhp_sdio.c
CSRC += $(TARGET_AMBD_SDK_SRC_SOC_PATH)/fwlib/ram_hp/rtl8721dhp_ssi.c
CSRC += $(TARGET_AMBD_SDK_SRC_SOC_PATH)/fwlib/ram_hp/rtl8721dhp_usi_uart.c
CSRC += $(TARGET_AMBD_SDK_SRC_SOC_PATH)/fwlib/ram_hp/rtl8721dhp_usi_i2c.c
CSRC += $(TARGET_AMBD_SDK_SRC_SOC_PATH)/fwlib/ram_hp/rtl8721dhp_cpft.c
#CSRC += $(TARGET_AMBD_SDK_SRC_SOC_PATH)/fwlib/ram_hp/rtl8721dhp_app_start.c
#CSRC += $(TARGET_AMBD_SDK_SRC_SOC_PATH)/fwlib/ram_hp/rtl8721dhp_pmc.c
CSRC += $(TARGET_AMBD_SDK_SRC_SOC_PATH)/fwlib/ram_hp/rtl8721dhp_lcdc.c
CSRC += $(TARGET_AMBD_SDK_SRC_SOC_PATH)/fwlib/ram_hp/rtl8721dhp_i2s.c
ifneq ("$(BOOTLOADER_MODULE)","")
CSRC += $(TARGET_AMBD_SDK_SRC_SOC_PATH)/bootloader/boot_flash_hp.c
CSRC += $(TARGET_AMBD_SDK_SRC_SOC_PATH)/bootloader/boot_ram_hp.c
CSRC += $(TARGET_AMBD_SDK_SRC_SOC_PATH)/bootloader/boot_trustzone_hp.c
endif
else
# KM0 (Cortex-M23)
#CSRC += $(TARGET_AMBD_SDK_SRC_SOC_PATH)/fwlib/ram_lp/rtl8721dlp_simulation.c
CSRC += $(TARGET_AMBD_SDK_SRC_SOC_PATH)/fwlib/ram_lp/rtl8721dlp_cpft.c
CSRC += $(TARGET_AMBD_SDK_SRC_SOC_PATH)/fwlib/ram_lp/rtl8721dlp_clk.c
CSRC += $(TARGET_AMBD_SDK_SRC_SOC_PATH)/fwlib/ram_lp/rtl8721dlp_flashclk.c
CSRC += $(TARGET_AMBD_SDK_SRC_SOC_PATH)/fwlib/ram_lp/rtl8721dlp_pmc_ram.c
CSRC += $(TARGET_AMBD_SDK_SRC_SOC_PATH)/fwlib/ram_lp/rtl8721dlp_km4.c
#CSRC += $(TARGET_AMBD_SDK_SRC_SOC_PATH)/fwlib/ram_lp/rtl8721dlp_app_start.c
CSRC += $(TARGET_AMBD_SDK_SRC_SOC_PATH)/fwlib/ram_lp/rtl8721dlp_startup.c
CSRC += $(TARGET_AMBD_SDK_SRC_SOC_PATH)/fwlib/ram_lp/rtl8721dlp_pmc.c
CSRC += $(TARGET_AMBD_SDK_SRC_SOC_PATH)/fwlib/ram_lp/rtl8721dlp_pinmap.c
CSRC += $(TARGET_AMBD_SDK_SRC_SOC_PATH)/fwlib/ram_lp/rtl8721dlp_system.c
ifneq ("$(BOOTLOADER_MODULE)","")
CSRC += $(TARGET_AMBD_SDK_SRC_SOC_PATH)/bootloader/boot_flash_lp.c
CSRC += $(TARGET_AMBD_SDK_SRC_SOC_PATH)/bootloader/boot_ram_lp.c
endif
endif

# CSRC += $(TARGET_AMBD_SDK_SRC_SOC_PATH)/fwlib/usrcfg/rtl8721dlp_intfcfg.c
CSRC += $(TARGET_AMBD_SDK_SRC_SOC_PATH)/fwlib/usrcfg/rtl8721dlp_flashcfg.c
CSRC += $(TARGET_AMBD_SDK_SRC_SOC_PATH)/fwlib/usrcfg/rtl8721dlp_pinmapcfg.c
# CSRC += $(TARGET_AMBD_SDK_SRC_SOC_PATH)/fwlib/usrcfg/rtl8721dlp_pinmapcfg_qfn88_evb_v1.c
CSRC += $(TARGET_AMBD_SDK_SRC_SOC_PATH)/fwlib/usrcfg/rtl8721dhp_boot_trustzonecfg.c
# CSRC += $(TARGET_AMBD_SDK_SRC_SOC_PATH)/fwlib/usrcfg/rtl8721d_ipccfg.c
CSRC += $(TARGET_AMBD_SDK_SRC_SOC_PATH)/fwlib/usrcfg/rtl8721dlp_sleepcfg.c
# CSRC += $(TARGET_AMBD_SDK_SRC_SOC_PATH)/fwlib/usrcfg/rtl8721d_bootcfg.c
# CSRC += $(TARGET_AMBD_SDK_SRC_SOC_PATH)/fwlib/usrcfg/rtl8721dhp_intfcfg.c
CSRC += $(TARGET_AMBD_SDK_SRC_SOC_PATH)/fwlib/usrcfg/rtl8721d_wificfg.c


# Out of the box this requires a few headers to define some basic types required
# by osdep_service and the default implementation is for FreeRTOS.
# We do not really want to run FreeRTOS in our bootloader if we can, so avoid building
# it for now, until there is a strong need for it (e.g. USB)
ifeq ("$(BOOTLOADER_MODULE)","")
CSRC += $(TARGET_AMBD_SDK_SRC_OS_PATH)/freertos/freertos_v10.2.0/Source/portable/GCC/RTL8721D_HP/non_secure/port.c
CSRC += $(TARGET_AMBD_SDK_SRC_OS_PATH)/freertos/freertos_v10.2.0/Source/portable/GCC/RTL8721D_HP/non_secure/portasm.c
CSRC += $(TARGET_AMBD_SDK_SRC_OS_PATH)/os_dep/osdep_service.c
endif

CFLAGS += -Wno-error=deprecated