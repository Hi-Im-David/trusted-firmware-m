/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */
#include <stdint.h>
#include <errno.h>
#include <nrfx.h>
#include <helpers/nrfx_ram_ctrl.h>

#include "wicr_setup.h"

#ifndef BIT_MASK
/* Use Zephyr BIT_MASK for unasigned integers */
#define BIT_MASK(n) ((1UL << (n)) - 1UL)
#endif

#define FLASH_PAGE_ERASE_MAX_TIME_US 42000UL
#define FLASH_PAGE_MAX_CNT           381UL

#ifdef DT_HAS_COMPAT_STATUS_OKAY
#if DT_HAS_COMPAT_STATUS_OKAY(nordic_nrf_pwr_antswc)
#define PWR_ANTSWC_REG (0x5010F780UL)
#define PWR_ANTSWC_ENABLE (0x3UL)
#endif /* DT_HAS_COMPAT_STATUS_OKAY(nordic_nrf_pwr_antswc) */
#endif /* DT_HAS_COMPAT_STATUS_OKAY */

/* PORTCNF.PWRCTRL of GPIO port 4. It selects where the floating ground of the P4 pins sits, in
 * other words whether the pins run in 1.8 V or 3.3 V mode. The register is not exposed by the MDK
 * yet, so it is defined here.
 * P4 is powered on by default so that the pins work out of the box, but that costs roughly 40 uA
 * of constant current, also in System OFF. The port is therefore powered off again when P4 is not
 * enabled in devicetree.
 */
#define P4_PWRCTRL_REG (NRF_P4_S_BASE + 0x34UL)
#define P4_PWRCTRL_OFF (0x0UL)

/* This handler needs to be ported to the upstream TF-M project when Cracen is supported there.
 * The implementation of this is currently in sdk-nrf. We define it to avoid warnings when we build
 * the target_cfg.c file which is the same for both upsteam TF-M and sdk-nrf.
 * It is defined as weak to allow the sdk-nrf version to be used when available. */
void __attribute__((weak)) CRACEN_IRQHandler(void){};

#ifndef __NRF_TFM__
#if defined(CONFIG_SOC_NRF71_WIFI_BOOT)
void __attribute__((weak)) wifi_setup(void){
	/* Kickstart the LMAC processor */
	NRF_WIFICORE_LRCCONF_LRC0->POWERON =
		(LRCCONF_POWERON_MAIN_AlwaysOn << LRCCONF_POWERON_MAIN_Pos);
	NRF_WIFICORE_LMAC_VPR->INITPC = (uint32_t)(uintptr_t)NRF_WICR->FIRMWARE.LMACINITPC;
	NRF_WIFICORE_LMAC_VPR->CPURUN = (VPR_CPURUN_EN_Running << VPR_CPURUN_EN_Pos);
}
#endif

int soc_early_init_hook(void){
    nrfx_ram_ctrl_retention_enable_all_set(false);

#if defined (CONFIG_SOC_NRF7120_WICR_SETUP)
	if (wicr_setup() != 0) {
		return -EIO;
	}
#endif

#if defined(CONFIG_SOC_NRF71_WIFI_BOOT)
	wifi_setup();
#endif

#ifdef DT_HAS_COMPAT_STATUS_OKAY
#if DT_HAS_COMPAT_STATUS_OKAY(nordic_nrf_pwr_antswc)
	*(volatile uint32_t *)PWR_ANTSWC_REG |= PWR_ANTSWC_ENABLE;
#endif /* DT_HAS_COMPAT_STATUS_OKAY(nordic_nrf_pwr_antswc) */
#endif /* DT_HAS_COMPAT_STATUS_OKAY */

#ifdef DT_NODE_HAS_STATUS_OKAY
#if DT_NODE_EXISTS(DT_NODELABEL(gpio4)) && !DT_NODE_HAS_STATUS_OKAY(DT_NODELABEL(gpio4))
	/* P4 is powered on by default and draws roughly 40 uA when unused */
	*(volatile uint32_t *)P4_PWRCTRL_REG = P4_PWRCTRL_OFF;
#endif /* !DT_NODE_HAS_STATUS_OKAY(DT_NODELABEL(gpio4)) */
#endif /* DT_NODE_HAS_STATUS_OKAY */

	/* Configure LFXO capacitive load if internal load capacitors are used */
#ifdef DT_ENUM_HAS_VALUE
#if DT_ENUM_HAS_VALUE(LFXO_NODE, load_capacitors, internal)
	nrf_lfxo_cload_set(NRF_LFXO,
			(uint8_t)(DT_PROP(LFXO_NODE, load_capacitance_femtofarad) / 1000));
#endif /* DT_ENUM_HAS_VALUE(LFXO_NODE, load_capacitors, internal) */
#endif /* DT_ENUM_HAS_VALUE */

	return 0;
}
#endif /* __NRF_TFM__ */