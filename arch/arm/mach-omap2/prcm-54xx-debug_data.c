/*
 * OMAP5 PRCM Debugging
 *
 * Copyright (C) 2011 Google, Inc.
 *
 * Copyright (C) 2012 Texas Instruments Incorporated - http://www.ti.com/
 *	Madhusudhan Chittim
 *
 * Original code from Android Icecream sandwich source code for OMAP4,
 * Modified by TI for supporting OMAP5.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/io.h>

#include "prcm-debug.h"
#include "iomap.h"
/*
 * Defined below in prcm44xx.h
 * OMAP54XX_PRM_PARTITION,
 * OMAP4430_CM2_PARTITION,
 * OMAP4430_CM1_PARTITION
 */
#include "prcm44xx.h"
#include "cm44xx.h" /* OMAP4_CM_CLKSTCTRL */
#include "cm1_54xx.h"
#include "cm2_54xx.h"
#include "cm-regbits-54xx.h"
#include "prm54xx.h"
#include "prm-regbits-54xx.h"
#include "cminst44xx.h"
#include "prcm_mpu54xx.h"
#include "powerdomain.h"
#include "powerdomain-private.h"

/* DPLLs */

static struct d_dpll_derived derived_dpll_per_m2 = {
	.name = "DPLL_PER_M2",
	.gatereg = OMAP54XX_CM_DIV_M2_DPLL_PER,
	.gatemask = 0xa00,
};

static struct d_dpll_derived derived_dpll_per_m3 = {
	.name = "DPLL_PER_M3",
	.gatereg = OMAP54XX_CM_DIV_M3_DPLL_PER,
	.gatemask = 0x200,
};

static struct d_dpll_derived derived_dpll_per_h11 = {
	.name = "DPLL_PER_H11",
	.gatereg = OMAP54XX_CM_DIV_H11_DPLL_PER,
	.gatemask = 0x200,
};

static struct d_dpll_derived derived_dpll_per_h12 = {
	.name = "DPLL_PER_H12",
	.gatereg = OMAP54XX_CM_DIV_H12_DPLL_PER,
	.gatemask = 0x200,
};

static struct d_dpll_derived derived_dpll_per_h14 = {
	.name = "DPLL_PER_H14",
	.gatereg = OMAP54XX_CM_DIV_H14_DPLL_PER,
	.gatemask = 0x200,
};

static struct d_dpll_info dpll_per = {
	.name = "DPLL_PER",
	.clkmodereg = OMAP54XX_CM_CLKMODE_DPLL_PER,
	.autoidlereg = OMAP54XX_CM_AUTOIDLE_DPLL_PER,
	.derived = {&derived_dpll_per_m2, &derived_dpll_per_m3,
		    &derived_dpll_per_h11, &derived_dpll_per_h12,
		    &derived_dpll_per_h14, NULL},
};

static struct d_dpll_derived derived_dpll_core_m2 = {
	.name = "DPLL_CORE_M2",
	.gatereg = OMAP54XX_CM_DIV_M2_DPLL_CORE,
	.gatemask = 0x200,
};

static struct d_dpll_derived derived_dpll_core_m3 = {
	.name = "DPLL_CORE_M3",
	.gatereg = OMAP54XX_CM_DIV_M3_DPLL_CORE,
	.gatemask = 0x200,
};

static struct d_dpll_derived derived_dpll_core_h11 = {
	.name = "DPLL_CORE_H11",
	.gatereg = OMAP54XX_CM_DIV_H11_DPLL_CORE,
	.gatemask = 0x200,
};

static struct d_dpll_derived derived_dpll_core_h12 = {
	.name = "DPLL_CORE_H12",
	.gatereg = OMAP54XX_CM_DIV_H12_DPLL_CORE,
	.gatemask = 0x200,
};

static struct d_dpll_derived derived_dpll_core_h13 = {
	.name = "DPLL_CORE_H13",
	.gatereg = OMAP54XX_CM_DIV_H13_DPLL_CORE,
	.gatemask = 0x200,
};

static struct d_dpll_derived derived_dpll_core_h14 = {
	.name = "DPLL_CORE_H14",
	.gatereg = OMAP54XX_CM_DIV_H14_DPLL_CORE,
	.gatemask = 0x200,
};

static struct d_dpll_info dpll_core = {
	.name = "DPLL_CORE",
	.clkmodereg = OMAP54XX_CM_CLKMODE_DPLL_CORE,
	.autoidlereg = OMAP54XX_CM_AUTOIDLE_DPLL_CORE,
	.derived = {&derived_dpll_core_m2, &derived_dpll_core_m3,
		    &derived_dpll_core_h11, &derived_dpll_core_h12,
		    &derived_dpll_core_h13, &derived_dpll_core_h14,
		    NULL},
};

static struct d_dpll_info dpll_abe = {
	.name = "DPLL_ABE",
	.clkmodereg = OMAP54XX_CM_CLKMODE_DPLL_ABE,
	.autoidlereg = OMAP54XX_CM_AUTOIDLE_DPLL_ABE,
	.derived = {/* &derived_dpll_abe_m2, &derived_dpll_abe_m3,
		    &derived_dpll_abe_m4, &derived_dpll_abe_m5,
		    &derived_dpll_abe_m6, &derived_dpll_abe_m7,
		    */ NULL},
};

static struct d_dpll_info dpll_mpu = {
	.name = "DPLL_MPU",
	.clkmodereg = OMAP54XX_CM_CLKMODE_DPLL_MPU,
	.autoidlereg = OMAP54XX_CM_AUTOIDLE_DPLL_MPU,
	.derived = {/* &derived_dpll_mpu_m2, */ NULL},
};

static struct d_dpll_info dpll_iva = {
	.name = "DPLL_IVA",
	.clkmodereg = OMAP54XX_CM_CLKMODE_DPLL_IVA,
	.autoidlereg = OMAP54XX_CM_AUTOIDLE_DPLL_IVA,
	.derived = {/* &derived_dpll_iva_m4, &derived_dpll_iva_m5, */ NULL},
};

static struct d_dpll_info dpll_usb = {
	.name = "DPLL_USB",
	.clkmodereg = OMAP54XX_CM_CLKMODE_DPLL_USB,
	.autoidlereg = OMAP54XX_CM_AUTOIDLE_DPLL_USB,
	.idlestreg = OMAP54XX_CM_IDLEST_DPLL_USB,
	.derived = {/* &derived_dpll_usb_m2, */ NULL},
};


/* Other internal generators */

static struct d_intgen_info intgen_cm1_abe = {
	.name = "CM1_ABE",
	.gatereg = OMAP54XX_CM_CLKSEL_ABE,
	.gatemask = 0x500,
};

/* Modules */

static struct d_mod_info mod_debug = {
	.name = "DEBUGSS",
	.clkctrl = OMAP54XX_CM_EMU_DEBUGSS_CLKCTRL,
	.flags = MOD_MODE | MOD_MASTER | MOD_SLAVE,
	.optclk = 0x0,
};

#ifdef CONFIG_ARCH_OMAP5_ES1
static struct d_mod_info mod_bandgap = {
	.name = "BANDGAPTS",
	.clkctrl = OMAP54XX_CM_COREAON_BANDGAP_CLKCTRL,
	.flags = 0,
	.optclk = 0x100,
};
#endif

static struct d_mod_info mod_gpio1 = {
	.name = "GPIO1",
	.clkctrl = OMAP54XX_CM_WKUPAON_GPIO1_CLKCTRL,
	.flags = MOD_MODE | MOD_SLAVE,
	.optclk = 0x100,
};

static struct d_mod_info mod_keyboard = {
	.name = "KEYBOARD",
	.clkctrl = OMAP54XX_CM_WKUPAON_KBD_CLKCTRL,
	.flags = MOD_MODE | MOD_SLAVE,
	.optclk = 0x0,
};

static struct d_mod_info mod_sar_ram = {
	.name = "SAR_RAM",
	.clkctrl = OMAP54XX_CM_WKUPAON_SAR_RAM_CLKCTRL,
	.flags = MOD_MODE | MOD_SLAVE,
	.optclk = 0x0,
};

static struct d_mod_info mod_32ktimer = {
	.name = "32KTIMER",
	.clkctrl = OMAP54XX_CM_WKUPAON_COUNTER_32K_CLKCTRL,
	.flags = MOD_MODE | MOD_SLAVE,
	.optclk = 0x0,
};

static struct d_mod_info mod_gptimer1 = {
	.name = "GPTIMER1",
	.clkctrl = OMAP54XX_CM_WKUPAON_TIMER1_CLKCTRL,
	.flags = MOD_MODE | MOD_SLAVE,
	.optclk = 0x0,
};

static struct d_mod_info mod_wdtimer2 = {
	.name = "WDTIMER2",
	.clkctrl = OMAP54XX_CM_WKUPAON_WD_TIMER2_CLKCTRL,
	.flags = MOD_MODE | MOD_SLAVE,
	.optclk = 0x0,
};

static struct d_mod_info mod_l4_wkup = {
	.name = "L4_WKUP",
	.clkctrl = OMAP54XX_CM_WKUPAON_L4_WKUP_CLKCTRL,
	.flags = MOD_MODE | MOD_SLAVE,
	.optclk = 0x0,
};

static struct d_mod_info mod_sr_core = {
	.name = "SR_CORE",
	.clkctrl = OMAP54XX_CM_COREAON_SMARTREFLEX_CORE_CLKCTRL,
	.flags = MOD_MODE | MOD_SLAVE,
	.optclk = 0x0,
};

static struct d_mod_info mod_sr_mm = {
	.name = "SR_MM",
	.clkctrl = OMAP54XX_CM_COREAON_SMARTREFLEX_MM_CLKCTRL,
	.flags = MOD_MODE | MOD_SLAVE,
	.optclk = 0x0,
};

static struct d_mod_info mod_sr_mpu = {
	.name = "SR_MPU",
	.clkctrl = OMAP54XX_CM_COREAON_SMARTREFLEX_MPU_CLKCTRL,
	.flags = MOD_MODE | MOD_SLAVE,
	.optclk = 0x0,
};

static struct d_mod_info mod_fdif = {
	.name = "FACE DETECT",
	.clkctrl = OMAP54XX_CM_CAM_FDIF_CLKCTRL,
	.flags = MOD_MODE | MOD_MASTER | MOD_SLAVE,
	.optclk = 0x0,
};

static struct d_mod_info mod_iss = {
	.name = "ISS",
	.clkctrl = OMAP54XX_CM_CAM_ISS_CLKCTRL,
	.flags = MOD_MODE | MOD_MASTER | MOD_SLAVE,
	.optclk = 0x100,
};

static struct d_mod_info mod_spinlock = {
	.name = "SPINLOCK",
	.clkctrl = OMAP54XX_CM_L4CFG_SPINLOCK_CLKCTRL,
	.flags = MOD_MODE | MOD_SLAVE,
	.optclk = 0x0,
};

static struct d_mod_info mod_l4_cfg_interconnect = {
	.name = "L4_CFG interconnect",
	.clkctrl = OMAP54XX_CM_L4CFG_L4_CFG_CLKCTRL,
	.flags = MOD_MODE | MOD_SLAVE,
	.optclk = 0x0,
};

static struct d_mod_info mod_mailbox = {
	.name = "MAILBOX",
	.clkctrl = OMAP54XX_CM_L4CFG_MAILBOX_CLKCTRL,
	.flags = MOD_MODE | MOD_SLAVE,
	.optclk = 0x0,
};

static struct d_mod_info mod_sar_rom = {
	.name = "SAR_ROM",
	.clkctrl = OMAP54XX_CM_L4CFG_SAR_ROM_CLKCTRL,
	.flags = MOD_MODE | MOD_SLAVE,
	.optclk = 0x0,
};

static struct d_mod_info mod_dmm = {
	.name = "DMM",
	.clkctrl = OMAP54XX_CM_EMIF_DMM_CLKCTRL,
	.flags = MOD_MODE | MOD_SLAVE,
	.optclk = 0x0,
};

static struct d_mod_info mod_emif_1 = {
	.name = "EMIF_1",
	.clkctrl = OMAP54XX_CM_EMIF_EMIF1_CLKCTRL,
	.flags = MOD_MODE | MOD_SLAVE,
	.optclk = 0x0,
};

static struct d_mod_info mod_emif_2 = {
	.name = "EMIF_2",
	.clkctrl = OMAP54XX_CM_EMIF_EMIF2_CLKCTRL,
	.flags = MOD_MODE | MOD_SLAVE,
	.optclk = 0x0,
};

static struct d_mod_info mod_emif_fw = {
	.name = "EMIF_FW",
	.clkctrl = OMAP54XX_CM_EMIF_EMIF_OCP_FW_CLKCTRL,
	.flags = MOD_MODE | MOD_SLAVE,
	.optclk = 0x0,
};

static struct d_mod_info mod_dll = {
	.name = "DLL_EMIF",
	.clkctrl = OMAP54XX_CM_EMIF_EMIF_DLL_CLKCTRL,
	.flags = 0,
	.optclk = 0x100,
};

static struct d_mod_info mod_ipu = {
	.name = "IPU",
	.clkctrl = OMAP54XX_CM_IPU_IPU_CLKCTRL,
	.flags = 0,
	.optclk = 0x100,
};

static struct d_mod_info mod_gpmc = {
	.name = "GPMC",
	.clkctrl = OMAP54XX_CM_L3MAIN2_GPMC_CLKCTRL,
	.flags = MOD_MODE | MOD_SLAVE,
	.optclk = 0x0,
};

static struct d_mod_info mod_l3_2_interconnect = {
	.name = "L3_2 interconnect",
	.clkctrl = OMAP54XX_CM_L3MAIN2_L3_MAIN_2_CLKCTRL,
	.flags = MOD_MODE | MOD_SLAVE,
	.optclk = 0x0,
};

static struct d_mod_info mod_ocmc_ram = {
	.name = "OCMC_RAM",
	.clkctrl = OMAP54XX_CM_L3MAIN2_OCMC_RAM_CLKCTRL,
	.flags = MOD_MODE | MOD_SLAVE,
	.optclk = 0x0,
};

static struct d_mod_info mod_l3_3_interconnect = {
	.name = "L3_3 interconnect",
	.clkctrl = OMAP54XX_CM_L3INSTR_L3_MAIN_3_CLKCTRL,
	.flags = MOD_MODE | MOD_SLAVE,
	.optclk = 0x0,
};

static struct d_mod_info mod_l3_instr_interconnect = {
	.name = "L3_INSTR interconnect",
	.clkctrl = OMAP54XX_CM_L3INSTR_L3_INSTR_CLKCTRL,
	.flags = MOD_MODE | MOD_SLAVE,
	.optclk = 0x0,
};

static struct d_mod_info mod_wp1 = {
	.name = "WP1",
	.clkctrl = OMAP54XX_CM_L3INSTR_OCP_WP_NOC_CLKCTRL,
	.flags = MOD_MODE | MOD_SLAVE,
	.optclk = 0x0,
};

static struct d_mod_info mod_l3_1_interconnect = {
	.name = "L3_1 interconnect",
	.clkctrl = OMAP54XX_CM_L3MAIN1_L3_MAIN_1_CLKCTRL,
	.flags = MOD_MODE | MOD_SLAVE,
	.optclk = 0x0,
};

static struct d_mod_info mod_c2c = {
	.name = "C2C",
	.clkctrl = OMAP54XX_CM_C2C_C2C_CLKCTRL,
	.flags = MOD_MODE | MOD_MASTER | MOD_SLAVE,
	.optclk = 0x0,
};

static struct d_mod_info mod_c2c_fw = {
	.name = "C2C_FW",
	.clkctrl = OMAP54XX_CM_C2C_C2C_OCP_FW_CLKCTRL,
	.flags = MOD_MODE | MOD_SLAVE,
	.optclk = 0x0,
};

static struct d_mod_info mod_modem_icr = {
	.name = "MODEM_ICR",
	.clkctrl = OMAP54XX_CM_C2C_MODEM_ICR_CLKCTRL,
	.flags = MOD_MODE | MOD_SLAVE,
	.optclk = 0x0,
};

static struct d_mod_info mod_lli = {
	.name = "LLI",
	.clkctrl = OMAP54XX_CM_MIPIEXT_LLI_CLKCTRL,
	.flags = MOD_MODE | MOD_MASTER | MOD_SLAVE,
	.optclk = 0x0,
};

static struct d_mod_info mod_lli_ocp_fw = {
	.name = "LLI_OCP_FW",
	.clkctrl = OMAP54XX_CM_MIPIEXT_LLI_OCP_FW_CLKCTRL,
	.flags = MOD_MODE | MOD_SLAVE,
	.optclk = 0x0,
};

static struct d_mod_info mod_mphy = {
	.name = "MPHY_LLI",
	.clkctrl = OMAP54XX_CM_MIPIEXT_MPHY_CLKCTRL,
	.flags = MOD_MODE | MOD_SLAVE,
	.optclk = 0x0,
};

#ifdef CONFIG_ARCH_OMAP5_ES1
static struct d_mod_info mod_unipro1 = {
	.name = "UNIPRO1",
	.clkctrl = OMAP54XX_CM_MIPIEXT_UNIPRO1_CLKCTRL,
	.flags = MOD_MODE | MOD_MASTER | MOD_SLAVE,
	.optclk = 0x0,
};
#endif

static struct d_mod_info mod_sdma = {
	.name = "sDMA",
	.clkctrl = OMAP54XX_CM_DMA_DMA_SYSTEM_CLKCTRL,
	.flags = MOD_MODE | MOD_MASTER | MOD_SLAVE,
	.optclk = 0x0,
};

static struct d_mod_info mod_dss = {
	.name = "DSS",
	.clkctrl = OMAP54XX_CM_DSS_DSS_CLKCTRL,
	.flags = MOD_MODE | MOD_MASTER | MOD_SLAVE,
	.optclk = 0xf00,
};

static struct d_mod_info mod_gpu = {
	.name = "GPU",
	.clkctrl = OMAP54XX_CM_GPU_GPU_CLKCTRL,
	.flags = MOD_MODE | MOD_MASTER | MOD_SLAVE,
	.optclk = 0x0,
};

static struct d_mod_info mod_hsi = {
	.name = "HSI",
	.clkctrl = OMAP54XX_CM_L3INIT_HSI_CLKCTRL,
	.flags = MOD_MODE | MOD_MASTER | MOD_SLAVE,
	.optclk = 0x0,
};
static struct d_mod_info mod_ieee_ocp = {
	.name = "IEEE1500_2_OCP",
	.clkctrl = OMAP54XX_CM_L3INIT_IEEE1500_2_OCP_CLKCTRL,
	.flags = MOD_MODE | MOD_MASTER | MOD_SLAVE,
	.optclk = 0x0,
};

static struct d_mod_info mod_ocp2scp1 = {
	.name = "OCP2SCP1",
	.clkctrl = OMAP54XX_CM_L3INIT_OCP2SCP1_CLKCTRL,
	.flags = MOD_MODE | MOD_SLAVE,
	.optclk = 0x0,
};

static struct d_mod_info mod_unipro2 = {
	.name = "UNIPRO2",
	.clkctrl = OMAP54XX_CM_L3INIT_UNIPRO2_CLKCTRL,
	.flags = MOD_MODE | MOD_MASTER | MOD_SLAVE,
	.optclk = 0x0,
};

static struct d_mod_info mod_mphy_unipro2 = {
	.name = "MPHY_UNIPRO2",
	.clkctrl = OMAP54XX_CM_L3INIT_MPHY_UNIPRO2_CLKCTRL,
	.flags = MOD_MODE | MOD_SLAVE,
	.optclk = 0x0,
};

static struct d_mod_info mod_hsmmc1 = {
	.name = "HSMMC1",
	.clkctrl = OMAP54XX_CM_L3INIT_MMC1_CLKCTRL,
	.flags = MOD_MODE | MOD_MASTER | MOD_SLAVE,
	.optclk = 0x0,
};

static struct d_mod_info mod_hsmmc2 = {
	.name = "HSMMC2",
	.clkctrl = OMAP54XX_CM_L3INIT_MMC2_CLKCTRL,
	.flags = MOD_MODE | MOD_MASTER | MOD_SLAVE,
	.optclk = 0x0,
};

static struct d_mod_info mod_usbphy = {
	.name = "USBPHY",
	.clkctrl = OMAP54XX_CM_L3INIT_OCP2SCP3_CLKCTRL,
	.flags = MOD_MODE | MOD_SLAVE,
	.optclk = 0x100,
};

static struct d_mod_info mod_hsusbhost = {
	.name = "HSUSBHOST",
	.clkctrl = OMAP54XX_CM_L3INIT_USB_HOST_HS_CLKCTRL,
	.flags = MOD_MODE | MOD_MASTER | MOD_SLAVE,
	.optclk = 0xff00,
};

static struct d_mod_info mod_sata = {
	.name = "SATA",
	.clkctrl = OMAP54XX_CM_L3INIT_SATA_CLKCTRL,
	.flags = MOD_MODE | MOD_MASTER | MOD_SLAVE,
	.optclk = 0xff00,
};

static struct d_mod_info mod_hsusbotg = {
	.name = "HSUSBOTG",
	.clkctrl = OMAP54XX_CM_L3INIT_USB_OTG_SS_CLKCTRL,
	.flags = MOD_MODE | MOD_MASTER | MOD_SLAVE,
	.optclk = 0x100,
};

static struct d_mod_info mod_usbtll = {
	.name = "USBTLL",
	.clkctrl = OMAP54XX_CM_L3INIT_USB_TLL_HS_CLKCTRL,
	.flags = MOD_MODE | MOD_SLAVE,
	.optclk = 0x300,
};

static struct d_mod_info mod_gptimer10 = {
	.name = "GPTIMER10",
	.clkctrl = OMAP54XX_CM_L4PER_TIMER10_CLKCTRL,
	.flags = MOD_MODE | MOD_SLAVE,
	.optclk = 0x0,
};

static struct d_mod_info mod_gptimer11 = {
	.name = "GPTIMER11",
	.clkctrl = OMAP54XX_CM_L4PER_TIMER11_CLKCTRL,
	.flags = MOD_MODE | MOD_SLAVE,
	.optclk = 0x0,
};
static struct d_mod_info mod_gptimer2 = {
	.name = "GPTIMER2",
	.clkctrl = OMAP54XX_CM_L4PER_TIMER2_CLKCTRL,
	.flags = MOD_MODE | MOD_SLAVE,
	.optclk = 0x0,
};
static struct d_mod_info mod_gptimer3 = {
	.name = "GPTIMER3",
	.clkctrl = OMAP54XX_CM_L4PER_TIMER3_CLKCTRL,
	.flags = MOD_MODE | MOD_SLAVE,
	.optclk = 0x0,
};
static struct d_mod_info mod_gptimer4 = {
	.name = "GPTIMER4",
	.clkctrl = OMAP54XX_CM_L4PER_TIMER4_CLKCTRL,
	.flags = MOD_MODE | MOD_SLAVE,
	.optclk = 0x0,
};
static struct d_mod_info mod_gptimer9 = {
	.name = "GPTIMER9",
	.clkctrl = OMAP54XX_CM_L4PER_TIMER9_CLKCTRL,
	.flags = MOD_MODE | MOD_SLAVE,
	.optclk = 0x0,
};
static struct d_mod_info mod_elm = {
	.name = "ELM",
	.clkctrl = OMAP54XX_CM_L4PER_ELM_CLKCTRL,
	.flags = MOD_MODE | MOD_SLAVE,
	.optclk = 0x0,
};
static struct d_mod_info mod_gpio2 = {
	.name = "GPIO2",
	.clkctrl = OMAP54XX_CM_L4PER_GPIO2_CLKCTRL,
	.flags = MOD_MODE | MOD_SLAVE,
	.optclk = 0x100,
};
static struct d_mod_info mod_gpio3 = {
	.name = "GPIO3",
	.clkctrl = OMAP54XX_CM_L4PER_GPIO3_CLKCTRL,
	.flags = MOD_MODE | MOD_SLAVE,
	.optclk = 0x100,
};
static struct d_mod_info mod_gpio4 = {
	.name = "GPIO4",
	.clkctrl = OMAP54XX_CM_L4PER_GPIO4_CLKCTRL,
	.flags = MOD_MODE | MOD_SLAVE,
	.optclk = 0x100,
};
static struct d_mod_info mod_gpio5 = {
	.name = "GPIO5",
	.clkctrl = OMAP54XX_CM_L4PER_GPIO5_CLKCTRL,
	.flags = MOD_MODE | MOD_SLAVE,
	.optclk = 0x100,
};
static struct d_mod_info mod_gpio6 = {
	.name = "GPIO6",
	.clkctrl = OMAP54XX_CM_L4PER_GPIO6_CLKCTRL,
	.flags = MOD_MODE | MOD_SLAVE,
	.optclk = 0x100,
};
static struct d_mod_info mod_gpio7 = {
	.name = "GPIO7",
	.clkctrl = OMAP54XX_CM_L4PER_GPIO7_CLKCTRL,
	.flags = MOD_MODE | MOD_SLAVE,
	.optclk = 0x100,
};
static struct d_mod_info mod_gpio8 = {
	.name = "GPIO8",
	.clkctrl = OMAP54XX_CM_L4PER_GPIO8_CLKCTRL,
	.flags = MOD_MODE | MOD_SLAVE,
	.optclk = 0x100,
};

static struct d_mod_info mod_hdq = {
	.name = "HDQ",
	.clkctrl = OMAP54XX_CM_L4PER_HDQ1W_CLKCTRL,
	.flags = MOD_MODE | MOD_SLAVE,
	.optclk = 0x0,
};
static struct d_mod_info mod_i2c1 = {
	.name = "I2C1",
	.clkctrl =  OMAP54XX_CM_L4PER_I2C1_CLKCTRL,
	.flags = MOD_MODE | MOD_SLAVE,
	.optclk = 0x0,
};
static struct d_mod_info mod_i2c2 = {
	.name = "I2C2",
	.clkctrl =  OMAP54XX_CM_L4PER_I2C1_CLKCTRL,
	.flags = MOD_MODE | MOD_SLAVE,
	.optclk = 0x0,
};
static struct d_mod_info mod_i2c3 = {
	.name = "I2C3",
	.clkctrl =  OMAP54XX_CM_L4PER_I2C1_CLKCTRL,
	.flags = MOD_MODE | MOD_SLAVE,
	.optclk = 0x0,
};
static struct d_mod_info mod_i2c4 = {
	.name = "I2C4",
	.clkctrl =  OMAP54XX_CM_L4PER_I2C1_CLKCTRL,
	.flags = MOD_MODE | MOD_SLAVE,
	.optclk = 0x0,
};
static struct d_mod_info mod_i2c5 = {
	.name = "I2C5",
	.clkctrl =  OMAP54XX_CM_L4PER_I2C1_CLKCTRL,
	.flags = MOD_MODE | MOD_SLAVE,
	.optclk = 0x0,
};
static struct d_mod_info mod_l4_per_interconnect = {
	.name = "L4_PER interconnect",
	.clkctrl = OMAP54XX_CM_L4PER_L4_PER_CLKCTRL,
	.flags = MOD_MODE | MOD_SLAVE,
	.optclk = 0x0,
};
static struct d_mod_info mod_mcspi1 = {
	.name = "MCSPI1",
	.clkctrl = OMAP54XX_CM_L4PER_MCSPI1_CLKCTRL,
	.flags = MOD_MODE | MOD_SLAVE,
	.optclk = 0x0,
};
static struct d_mod_info mod_mcspi2 = {
	.name = "MCSPI2",
	.clkctrl = OMAP54XX_CM_L4PER_MCSPI2_CLKCTRL,
	.flags = MOD_MODE | MOD_SLAVE,
	.optclk = 0x0,
};
static struct d_mod_info mod_mcspi3 = {
	.name = "MCSPI3",
	.clkctrl = OMAP54XX_CM_L4PER_MCSPI3_CLKCTRL,
	.flags = MOD_MODE | MOD_SLAVE,
	.optclk = 0x0,
};
static struct d_mod_info mod_mcspi4 = {
	.name = "MCSPI4",
	.clkctrl = OMAP54XX_CM_L4PER_MCSPI4_CLKCTRL,
	.flags = MOD_MODE | MOD_SLAVE,
	.optclk = 0x0,
};
static struct d_mod_info mod_hsmmc3 = {
	.name = "HSMMC3",
	.clkctrl = OMAP54XX_CM_L4PER_MMC3_CLKCTRL,
	.flags = MOD_MODE | MOD_SLAVE,
	.optclk = 0x0,
};
static struct d_mod_info mod_hsmmc4 = {
	.name = "HSMMC4",
	.clkctrl = OMAP54XX_CM_L4PER_MMC4_CLKCTRL,
	.flags = MOD_MODE | MOD_SLAVE,
	.optclk = 0x0,
};
static struct d_mod_info mod_hsmmc5 = {
	.name = "HSMMC5",
	.clkctrl = OMAP54XX_CM_L4PER_MMC5_CLKCTRL,
	.flags = MOD_MODE | MOD_SLAVE,
	.optclk = 0x0,
};

#ifdef CONFIG_ARCH_OMAP5_ES1
static struct d_mod_info mod_slimbus2 = {
	.name = "SLIMBUS2",
	.clkctrl = OMAP54XX_CM_L4PER_SLIMBUS2_CLKCTRL,
	.flags = MOD_MODE | MOD_SLAVE,
	.optclk = 0x700,
};
#endif

static struct d_mod_info mod_uart1 = {
	.name = "UART1",
	.clkctrl = OMAP54XX_CM_L4PER_UART1_CLKCTRL,
	.flags = MOD_MODE | MOD_SLAVE,
	.optclk = 0x0,
};
static struct d_mod_info mod_uart2 = {
	.name = "UART2",
	.clkctrl = OMAP54XX_CM_L4PER_UART2_CLKCTRL,
	.flags = MOD_MODE | MOD_SLAVE,
	.optclk = 0x0,
};
static struct d_mod_info mod_uart3 = {
	.name = "UART3",
	.clkctrl = OMAP54XX_CM_L4PER_UART3_CLKCTRL,
	.flags = MOD_MODE | MOD_SLAVE,
	.optclk = 0x0,
};

static struct d_mod_info mod_uart4 = {
	.name = "UART4",
	.clkctrl = OMAP54XX_CM_L4PER_UART4_CLKCTRL,
	.flags = MOD_MODE | MOD_SLAVE,
	.optclk = 0x0,
};
static struct d_mod_info mod_uart5 = {
	.name = "UART5",
	.clkctrl = OMAP54XX_CM_L4PER_UART5_CLKCTRL,
	.flags = MOD_MODE | MOD_SLAVE,
	.optclk = 0x0,
};
static struct d_mod_info mod_uart6 = {
	.name = "UART6",
	.clkctrl = OMAP54XX_CM_L4PER_UART6_CLKCTRL,
	.flags = MOD_MODE | MOD_SLAVE,
	.optclk = 0x0,
};
static struct d_mod_info mod_audio_engine = {
	.name = "AUDIO ENGINE",
	.clkctrl = OMAP54XX_CM_ABE_AESS_CLKCTRL,
	.flags = MOD_MODE | MOD_MASTER | MOD_SLAVE,
	.optclk = 0x0,
};
static struct d_mod_info mod_dmic = {
	.name = "DMIC",
	.clkctrl = OMAP54XX_CM_ABE_DMIC_CLKCTRL,
	.flags = MOD_MODE | MOD_SLAVE,
	.optclk = 0x0,
};
static struct d_mod_info mod_l4_abe_interconnect = {
	.name = "L4_ABE interconnect",
	.clkctrl = OMAP54XX_CM_ABE_L4_ABE_CLKCTRL,
	.flags = MOD_MODE | MOD_SLAVE,
	.optclk = 0x0,
};
static struct d_mod_info mod_mcasp1 = {
	.name = "MCASP",
	.clkctrl = OMAP54XX_CM_ABE_MCASP_CLKCTRL,
	.flags = MOD_MODE | MOD_SLAVE,
	.optclk = 0x0,
};
static struct d_mod_info mod_mcbsp1 = {
	.name = "MCBSP1",
	.clkctrl = OMAP54XX_CM_ABE_MCBSP1_CLKCTRL,
	.flags = MOD_MODE | MOD_SLAVE,
	.optclk = 0x0,
};
static struct d_mod_info mod_mcbsp2 = {
	.name = "MCBSP2",
	.clkctrl = OMAP54XX_CM_ABE_MCBSP2_CLKCTRL,
	.flags = MOD_MODE | MOD_SLAVE,
	.optclk = 0x0,
};
static struct d_mod_info mod_mcbsp3 = {
	.name = "MCBSP3",
	.clkctrl = OMAP54XX_CM_ABE_MCBSP3_CLKCTRL,
	.flags = MOD_MODE | MOD_SLAVE,
	.optclk = 0x0,
};
static struct d_mod_info mod_mcpdm = {
	.name = "MCPDM",
	.clkctrl = OMAP54XX_CM_ABE_MCPDM_CLKCTRL,
	.flags = MOD_MODE | MOD_SLAVE,
	.optclk = 0x0,
};
static struct d_mod_info mod_slimbus1 = {
	.name = "SLIMBUS1",
	.clkctrl = OMAP54XX_CM_ABE_SLIMBUS1_CLKCTRL,
	.flags = MOD_MODE | MOD_SLAVE,
	.optclk = 0xf00,
};
static struct d_mod_info mod_gptimer5 = {
	.name = "GPTIMER5",
	.clkctrl = OMAP54XX_CM_ABE_TIMER5_CLKCTRL,
	.flags = MOD_MODE | MOD_SLAVE,
	.optclk = 0x0,
};
static struct d_mod_info mod_gptimer6 = {
	.name = "GPTIMER6",
	.clkctrl = OMAP54XX_CM_ABE_TIMER6_CLKCTRL,
	.flags = MOD_MODE | MOD_SLAVE,
	.optclk = 0x0,
};
static struct d_mod_info mod_gptimer7 = {
	.name = "GPTIMER7",
	.clkctrl = OMAP54XX_CM_ABE_TIMER7_CLKCTRL,
	.flags = MOD_MODE | MOD_SLAVE,
	.optclk = 0x0,
};
static struct d_mod_info mod_gptimer8 = {
	.name = "GPTIMER8",
	.clkctrl = OMAP54XX_CM_ABE_TIMER8_CLKCTRL,
	.flags = MOD_MODE | MOD_SLAVE,
	.optclk = 0x0,
};
static struct d_mod_info mod_wdtimer3 = {
	.name = "WDTIMER3",
	.clkctrl = OMAP54XX_CM_ABE_WD_TIMER3_CLKCTRL,
	.flags = MOD_MODE | MOD_SLAVE,
	.optclk = 0x0,
};

static struct d_mod_info mod_ivahd = {
	.name = "IVAHD",
	.clkctrl = OMAP54XX_CM_IVA_IVA_CLKCTRL,
	.flags = MOD_MODE | MOD_MASTER | MOD_SLAVE,
	.optclk = 0x0,
};
static struct d_mod_info mod_sl2 = {
	.name = "SL2",
	.clkctrl = OMAP54XX_CM_IVA_SL2_CLKCTRL,
	.flags = MOD_MODE | MOD_SLAVE,
	.optclk = 0x0,
};

static struct d_mod_info mod_dsp = {
	.name = "DSP",
	.clkctrl = OMAP54XX_CM_DSP_DSP_CLKCTRL,
	.flags = MOD_MODE | MOD_MASTER | MOD_SLAVE,
	.optclk = 0x0,
};

static struct d_mod_info mod_cortexa15 = {
	.name = "CORTEXA15",
	.clkctrl = OMAP54XX_CM_MPU_MPU_CLKCTRL,
	.flags = MOD_MODE | MOD_MASTER | MOD_SLAVE,
	.optclk = 0x0,
};

static struct d_mod_info mod_aes1 = {
	.name = "AES1",
	.clkctrl = OMAP54XX_CM_L4SEC_AES1_CLKCTRL,
	.flags = MOD_MODE | MOD_SLAVE,
	.optclk = 0x0,
};

static struct d_mod_info mod_aes2 = {
	.name = "AES2",
	.clkctrl = OMAP54XX_CM_L4SEC_AES2_CLKCTRL,
	.flags = MOD_MODE | MOD_SLAVE,
	.optclk = 0x0,
};

static struct d_mod_info mod_des3des = {
	.name = "DES3DES",
	.clkctrl = OMAP54XX_CM_L4SEC_DES3DES_CLKCTRL,
	.flags = MOD_MODE | MOD_SLAVE,
	.optclk = 0x0,
};

static struct d_mod_info mod_fpka = {
	.name = "FPKA",
	.clkctrl = OMAP54XX_CM_L4SEC_FPKA_CLKCTRL,
	.flags = MOD_MODE | MOD_SLAVE,
	.optclk = 0x0,
};

static struct d_mod_info mod_rng = {
	.name = "RNG",
	.clkctrl = OMAP54XX_CM_L4SEC_RNG_CLKCTRL,
	.flags = MOD_MODE | MOD_SLAVE,
	.optclk = 0x0,
};

static struct d_mod_info mod_sha2md5 = {
	.name = "SHA2MD5",
	.clkctrl = OMAP54XX_CM_L4SEC_SHA2MD5_CLKCTRL,
	.flags = MOD_MODE | MOD_SLAVE,
	.optclk = 0x0,
};

static struct d_mod_info mod_cryptodma = {
	.name = "CRYPTODMA",
	.clkctrl = OMAP54XX_CM_L4SEC_DMA_CRYPTO_CLKCTRL,
	.flags = MOD_MODE | MOD_SLAVE,
	.optclk = 0x0,
};

/* Clock domains */

static struct d_clkd_info cd_emu = {
	.name = "CD_EMU",
	.prcm_partition	  = OMAP54XX_PRM_PARTITION,
	.cm_inst	  = OMAP54XX_PRM_EMU_CM_INST,
	.clkdm_offs	  = OMAP54XX_PRM_EMU_CM_EMU_CDOFFS,
	.activity	  = 0x100,
	.mods = {&mod_debug, NULL},
	.intgens = {NULL},
	/*TBD: TRM mentions: CM1_EMU*/
};

static struct d_clkd_info cd_wkupaon = {
	.name = "CD_WKUPAON",
	.prcm_partition	  = OMAP54XX_PRM_PARTITION,
	.cm_inst	  = OMAP54XX_PRM_WKUPAON_INST,
	.clkdm_offs	  = OMAP54XX_PRM_WKUPAON_CM_WKUPAON_CDOFFS,
	.activity	  = 0x3b00,
	.mods = {&mod_gpio1, &mod_keyboard, &mod_sar_ram,
		 &mod_32ktimer, &mod_gptimer1, &mod_wdtimer2,
		 &mod_l4_wkup, NULL},
	.intgens = {NULL},
	/* TBD: TRM mentions: SYSCTRL_PADCONF_WKUP,
		SYSCTRL_GENERAL_WKUP, PRM, SCRM */
};

static struct d_clkd_info cd_l4_alwon_core = {
	.name = "CD_COREAON",
	.prcm_partition	  = OMAP4430_CM2_PARTITION,
	.cm_inst	  = OMAP54XX_CM_CORE_COREAON_INST,
	.clkdm_offs	  = OMAP54XX_CM_CORE_COREAON_COREAON_CDOFFS,
	.activity	  = 0x7f00,
	.mods = {&mod_sr_core, &mod_sr_mm, &mod_sr_mpu,
#ifdef CONFIG_ARCH_OMAP5_ES1
		 &mod_bandgap, NULL},
#else
		 NULL},
#endif
	.dplls = {&dpll_per, &dpll_core, &dpll_abe, NULL},
	.intgens = {NULL},
	/* TBD: TRM mentions: CM1, CORTEXM3_WKUPGEN,
		SDMA_WKUPGEN, SPINNER */
};

static struct d_clkd_info cd_cam = {
	.name = "CD_CAM",
	.prcm_partition	  = OMAP4430_CM2_PARTITION,
	.cm_inst	  = OMAP54XX_CM_CORE_CAM_INST,
	.clkdm_offs	  = OMAP54XX_CM_CORE_CAM_CAM_CDOFFS,
	.activity	  = 0x1f00,
	.mods = {&mod_fdif, &mod_iss, NULL},
	.intgens = {NULL},
};

static struct d_clkd_info cd_l4_cfg = {
	.name = "CD_L4_CFG",
	.prcm_partition	  = OMAP4430_CM2_PARTITION,
	.cm_inst	  = OMAP54XX_CM_CORE_CORE_INST,
	.clkdm_offs	  = OMAP54XX_CM_CORE_CORE_L4CFG_CDOFFS,
	.activity	  = 0x300,
	.mods = {&mod_spinlock, &mod_l4_cfg_interconnect, &mod_mailbox,
		 &mod_sar_rom, NULL},
	.intgens = {NULL},
	/* TBD: TRM mentions: SYSCTRL_PADCONF_CORE, SYSCTRL_GENERAL_CORE */
};

static struct d_clkd_info cd_emif = {
	.name = "CD_EMIF",
	.prcm_partition	  = OMAP4430_CM2_PARTITION,
	.cm_inst	  = OMAP54XX_CM_CORE_CORE_INST,
	.clkdm_offs	  = OMAP54XX_CM_CORE_CORE_EMIF_CDOFFS,
	.activity	  = 0x700,
	.mods = {&mod_dmm, &mod_emif_1, &mod_emif_2, &mod_emif_fw,
		 &mod_dll, NULL},
	.intgens = {NULL},
	/* TBD: TRM mentions: DDRPHY, DLL */
};

static struct d_clkd_info cd_ipu = {
	.name = "CD_IPU",
	.prcm_partition	  = OMAP4430_CM2_PARTITION,
	.cm_inst	  = OMAP54XX_CM_CORE_CORE_INST,
	.clkdm_offs	  = OMAP54XX_CM_CORE_CORE_IPU_CDOFFS,
	.activity	  = 0x100,
	.mods = {&mod_ipu, NULL},
	.intgens = {NULL},
	/* TBD: TRM mentions: DDRPHY, DLL */
};

static struct d_clkd_info cd_l3_2 = {
	.name = "CD_L3_MAIN2",
	.prcm_partition	  = OMAP4430_CM2_PARTITION,
	.cm_inst	  = OMAP54XX_CM_CORE_CORE_INST,
	.clkdm_offs	  = OMAP54XX_CM_CORE_CORE_L3MAIN2_CDOFFS,
	.activity	  = 0x100,
	.mods = {&mod_gpmc, &mod_l3_2_interconnect, &mod_ocmc_ram, NULL},
	.intgens = {NULL},
};

static struct d_clkd_info cd_l3_instr = {
	.name = "CD_L3_INSTR",
	.prcm_partition	  = OMAP4430_CM2_PARTITION,
	.cm_inst	  = OMAP54XX_CM_CORE_CORE_INST,
	.clkdm_offs	  = OMAP54XX_CM_CORE_CORE_L3INSTR_CDOFFS,
	.activity	  = 0x300,
	.mods = {&mod_l3_3_interconnect, &mod_l3_instr_interconnect,
		 &mod_wp1, NULL},
	.intgens = {NULL},
};

static struct d_clkd_info cd_l3_1 = {
	.name = "CD_L3_MAIN1",
	.prcm_partition	  = OMAP4430_CM2_PARTITION,
	.cm_inst	  = OMAP54XX_CM_CORE_CORE_INST,
	.clkdm_offs	  = OMAP54XX_CM_CORE_CORE_L3MAIN1_CDOFFS,
	.activity	  = 0x100,
	.mods = {&mod_l3_1_interconnect, NULL},
	.intgens = {NULL},
};

static struct d_clkd_info cd_c2c = {
	.name = "CD_C2C",
	.prcm_partition	  = OMAP4430_CM2_PARTITION,
	.cm_inst	  = OMAP54XX_CM_CORE_CORE_INST,
	.clkdm_offs	  = OMAP54XX_CM_CORE_CORE_C2C_CDOFFS,
	.activity	  = 0x300,
	.mods = {&mod_c2c, &mod_c2c_fw, &mod_modem_icr, NULL},
	.intgens = {NULL},
};

static struct d_clkd_info cd_mipiext = {
	.name = "CD_MIPIEXT",
	.prcm_partition	  = OMAP4430_CM2_PARTITION,
	.cm_inst	  = OMAP54XX_CM_CORE_CORE_INST,
	.clkdm_offs	  = OMAP54XX_CM_CORE_CORE_MIPIEXT_CDOFFS,
	.activity	  = 0x3d00,
	.mods = {&mod_lli, &mod_lli_ocp_fw, &mod_mphy,
#ifdef CONFIG_ARCH_OMAP5_ES1
		 &mod_unipro1, NULL},
#else
		 NULL},
#endif
	.intgens = {NULL},
};

static struct d_clkd_info cd_dma = {
	.name = "CD_DMA",
	.prcm_partition	  = OMAP4430_CM2_PARTITION,
	.cm_inst	  = OMAP54XX_CM_CORE_CORE_INST,
	.clkdm_offs	  = OMAP54XX_CM_CORE_CORE_DMA_CDOFFS,
	.activity	  = 0x100,
	.mods = {&mod_sdma, NULL},
	.intgens = {NULL},
};

static struct d_clkd_info cd_dss = {
	.name = "CD_DSS",
	.prcm_partition	  = OMAP4430_CM2_PARTITION,
	.cm_inst	  = OMAP54XX_CM_CORE_DSS_INST,
	.clkdm_offs	  = OMAP54XX_CM_CORE_DSS_DSS_CDOFFS,
	.activity	  = 0xf00,
	.mods = {&mod_dss, NULL},
	.intgens = {NULL},
};

static struct d_clkd_info cd_gpu = {
	.name = "CD_GPU",
	.prcm_partition	  = OMAP4430_CM2_PARTITION,
	.cm_inst	  = OMAP54XX_CM_CORE_GPU_INST,
	.clkdm_offs	  = OMAP54XX_CM_CORE_GPU_GPU_CDOFFS,
	.activity	  = 0x700,
	.mods = {&mod_gpu, NULL},
};

static struct d_clkd_info cd_l3_init = {
	.name = "CD_L3_INIT",
	.prcm_partition	  = OMAP4430_CM2_PARTITION,
	.cm_inst	  = OMAP54XX_CM_CORE_L3INIT_INST,
	.clkdm_offs	  = OMAP54XX_CM_CORE_L3INIT_L3INIT_CDOFFS,
	.activity	  = 0xfffffff8,
	.mods = {&mod_hsi, &mod_ieee_ocp, &mod_hsmmc1, &mod_hsmmc2,
		 &mod_ocp2scp1, &mod_usbphy, &mod_hsusbhost, &mod_sata,
		 &mod_unipro2, &mod_mphy_unipro2, &mod_hsusbotg,
		 &mod_usbtll, NULL},
	.dplls = {&dpll_usb, NULL},
	.intgens = {NULL},
	/* TBD: TRM mentions: CM1_USB */
};

static struct d_clkd_info cd_l4_per = {
	.name = "CD_L4_PER",
	.prcm_partition	  = OMAP4430_CM2_PARTITION,
#ifdef CONFIG_ARCH_OMAP5_ES1
	.cm_inst	  = OMAP54XX_CM_CORE_L4PER_INST,
	.clkdm_offs	  = OMAP54XX_CM_CORE_L4PER_L4PER_CDOFFS,
#else
	.cm_inst	  = OMAP54XX_CM_CORE_CORE_INST,
	.clkdm_offs	  = OMAP54XX_CM_CORE_CORE_L4PER_CDOFFS,
#endif
	.activity	  = 0x60fff00,
	.mods = {&mod_gptimer10, &mod_gptimer11, &mod_gptimer2,
		 &mod_gptimer3, &mod_gptimer4, &mod_gptimer9, &mod_elm,
		 &mod_gpio2, &mod_gpio3, &mod_gpio4, &mod_gpio5, &mod_gpio6,
		 &mod_gpio7, &mod_gpio8, &mod_hdq, &mod_i2c1,
		 &mod_i2c2, &mod_i2c3, &mod_i2c4, &mod_i2c5,
		 &mod_l4_per_interconnect, &mod_mcspi1,
		 &mod_mcspi2, &mod_mcspi3, &mod_mcspi4, &mod_hsmmc3,
#ifdef CONFIG_ARCH_OMAP5_ES1
		 &mod_hsmmc4, &mod_hsmmc5, &mod_slimbus2, &mod_uart1,
#else
		 &mod_hsmmc4, &mod_hsmmc5, &mod_uart1,
#endif
		 &mod_uart2, &mod_uart3, &mod_uart4, &mod_uart5,
		 &mod_uart6, NULL},
	/* TBD: Linux refs: I2C5 */
	.intgens = {NULL},
};

static struct d_clkd_info cd_abe = {
	.name = "CD_ABE",
	.prcm_partition	  = OMAP4430_CM1_PARTITION,
	.cm_inst	  = OMAP54XX_CM_CORE_AON_ABE_INST,
	.clkdm_offs	  = OMAP54XX_CM_CORE_AON_ABE_ABE_CDOFFS,
	.activity	  = 0xff00,
	.mods = {&mod_audio_engine, &mod_dmic, &mod_l4_abe_interconnect,
		 &mod_mcasp1, &mod_mcbsp1, &mod_mcbsp2, &mod_mcbsp3,
		 &mod_mcpdm, &mod_slimbus1, &mod_gptimer5, &mod_gptimer6,
		 &mod_gptimer7, &mod_gptimer8, &mod_wdtimer3, NULL},
	.intgens = {&intgen_cm1_abe, NULL},
};

static struct d_clkd_info cd_iva = {
	.name = "CD_IVA",
	.prcm_partition	  = OMAP4430_CM2_PARTITION,
	.cm_inst	  = OMAP54XX_CM_CORE_IVA_INST,
	.clkdm_offs	  = OMAP54XX_CM_CORE_IVA_IVA_CDOFFS,
	.activity	  = 0x100,
	.mods = {&mod_ivahd, &mod_sl2, NULL},
	.intgens = {NULL},
};

static struct d_clkd_info cd_dsp = {
	.name = "CD_DSP",
	.prcm_partition	  = OMAP4430_CM1_PARTITION,
	.cm_inst	  = OMAP54XX_CM_CORE_AON_DSP_INST,
	.clkdm_offs	  = OMAP54XX_CM_CORE_AON_DSP_DSP_CDOFFS,
	.activity	  = 0x100,
	.mods = {&mod_dsp, NULL},
	.intgens = {NULL},
};

static struct d_clkd_info cd_pd_alwon_mpu_fake = {
	.name = "N/A (clock generator)",
	.prcm_partition	  = -1,
	.cm_inst	  = -1,
	.clkdm_offs	  = -1,
	.activity	  = 0x0,
	.mods = {NULL},
	.dplls = {&dpll_mpu, NULL},
	.intgens = {NULL},
	/* TBD: TRM mentions: CORTEXA9_MPU_INTC */
};

static struct d_clkd_info cd_pd_alwon_dsp_fake = {
	.name = "N/A (clock generator)",
	.prcm_partition	  = -1,
	.cm_inst	  = -1,
	.clkdm_offs	  = -1,
	.activity	  = 0x0,
	.mods = {NULL},
	.dplls = {&dpll_iva, NULL},
	.intgens = {NULL},
	/* TBD: TRM mentions: DSP_WKUPGEN */
};

static struct d_clkd_info cd_cortexa15 = {
	.name = "CD_CORTEXA15",
	.prcm_partition	  = OMAP4430_CM1_PARTITION,
	.cm_inst	  = OMAP54XX_CM_CORE_AON_MPU_INST,
	.clkdm_offs	  = OMAP54XX_CM_CORE_AON_MPU_MPU_CDOFFS,
	.activity	  = 0x100,
	.mods = {&mod_cortexa15, NULL},
	.intgens = {NULL},
};

static struct d_clkd_info cd_l4sec = {
	.name = "CD_L4_SEC",
	.prcm_partition	  = OMAP4430_CM2_PARTITION,
#ifdef CONFIG_ARCH_OMAP5_ES1
	.cm_inst	  = OMAP54XX_CM_CORE_L4PER_INST,
	.clkdm_offs	  = OMAP54XX_CM_CORE_L4PER_L4PER_CDOFFS,
#else
	.cm_inst	  = OMAP54XX_CM_CORE_CORE_INST,
	.clkdm_offs	  = OMAP54XX_CM_CORE_CORE_L4PER_CDOFFS,
#endif
	.activity	  = 0x300,
	.mods = {&mod_aes1, &mod_aes2, &mod_des3des, &mod_fpka, &mod_rng,
		 &mod_sha2md5, &mod_cryptodma, NULL},
	.intgens = {NULL}, /* TBD: No docs */
};

/* Power domains */

static struct d_pwrd_info pd_emu = {
	.name = "PD_EMU",
	.prminst = OMAP54XX_PRM_EMU_CM_INST,
	.pwrst = OMAP54XX_PM_EMU_PWRSTST_OFFSET,
	.cds = {&cd_emu, NULL},
};

static struct d_pwrd_info pd_wkupaon = {
	.name = "PD_WKUP",
	.prminst = OMAP54XX_PRM_WKUPAON_INST,
	.pwrst = -1,
	.cds = {&cd_wkupaon, NULL},
};

static struct d_pwrd_info pd_alwon_core = {
	.name = "PD_ALWON_CORE",
	.prminst = OMAP54XX_PRM_WKUPAON_CM_INST,
	.pwrst = -1,
	.cds = {&cd_l4_alwon_core, NULL},
};

static struct d_pwrd_info pd_cam = {
	.name = "PD_CAM",
	.prminst = OMAP54XX_PRM_CAM_INST,
	.pwrst = OMAP54XX_PM_CAM_PWRSTST_OFFSET,
	.cds = {&cd_cam, NULL},
};

static struct d_pwrd_info pd_core = {
	.name = "PD_CORE",
	.prminst = OMAP54XX_PRM_CORE_INST,
	.pwrst = OMAP54XX_PM_CORE_PWRSTST_OFFSET,
	.cds = {&cd_l4_cfg, &cd_ipu, &cd_emif, &cd_l3_2, &cd_l3_instr,
#ifdef CONFIG_ARCH_OMAP5_ES1
		&cd_l3_1, &cd_c2c, &cd_dma, &cd_mipiext, NULL},
#else
		&cd_l3_1, &cd_c2c, &cd_dma, &cd_mipiext, &cd_l4_per, &cd_l4sec,
		NULL},
#endif
	/* TBD: TRM mentions: CM2 */
};

static struct d_pwrd_info pd_dss = {
	.name = "PD_DSS",
	.prminst = OMAP54XX_PRM_DSS_INST,
	.pwrst = OMAP54XX_PM_DSS_PWRSTST_OFFSET,
	.cds = {&cd_dss, NULL},
};

static struct d_pwrd_info pd_gpu = {
	.name = "PD_GPU",
	.prminst = OMAP54XX_PRM_GPU_INST,
	.pwrst = OMAP54XX_PM_GPU_PWRSTST_OFFSET,
	.cds = {&cd_gpu, NULL},
};

static struct d_pwrd_info pd_l3_init = {
	.name = "PD_L3_INIT",
	.prminst = OMAP54XX_PRM_L3INIT_INST,
	.pwrst = OMAP54XX_PM_L3INIT_PWRSTST_OFFSET,
	.cds = {&cd_l3_init, NULL},
};
#ifdef CONFIG_ARCH_OMAP5_ES1
static struct d_pwrd_info pd_l4_per = {
	.name = "PD_L4_PER",
	.prminst = OMAP54XX_PRM_L4PER_INST,
	.pwrst = OMAP54XX_PM_L4PER_PWRSTST_OFFSET,
	.cds = {&cd_l4_per, &cd_l4sec, NULL},
};
#endif
static struct d_pwrd_info pd_std_efuse = {
	.name = "PD_STD_EFUSE",
	.prminst = OMAP54XX_PRM_CUSTEFUSE_INST,
	.pwrst = OMAP54XX_PM_CUSTEFUSE_PWRSTST_OFFSET,
	.cds = {NULL},
	/* TBD: Linux: CEFUSE PD regs, CD regs, module regs? */
};

static struct d_pwrd_info pd_alwon_mm = {
	.name = "PD_MM_ALWON",
	.prminst = -1,
	.pwrst = -1,
	.cds = {&cd_pd_alwon_dsp_fake, NULL},
};

static struct d_pwrd_info pd_audio = {
	.name = "PD_AUDIO",
	.prminst = OMAP54XX_PRM_ABE_INST,
	.pwrst = OMAP54XX_PM_ABE_PWRSTST_OFFSET,
	.cds = {&cd_abe, NULL},
};

static struct d_pwrd_info pd_iva = {
	.name = "PD_IVA",
	.prminst = OMAP54XX_PRM_IVA_INST,
	.pwrst = OMAP54XX_PM_IVA_PWRSTST_OFFSET,
	.cds = {&cd_iva, NULL},
};

static struct d_pwrd_info pd_dsp = {
	.name = "PD_DSP",
	.prminst = OMAP54XX_PRM_DSP_INST,
	.pwrst = OMAP54XX_PM_DSP_PWRSTST_OFFSET,
	.cds = {&cd_dsp, NULL},
};

static struct d_pwrd_info pd_alwon_mpu = {
	.name = "PD_ALWON_MPU",
	.prminst = -1,
	.pwrst = -1,
	.cds = {&cd_pd_alwon_mpu_fake, NULL},
};

static struct d_pwrd_info pd_mpu = {
	.name = "PD_MPU",
	.prminst = OMAP54XX_PRM_MPU_INST,
	.pwrst = OMAP54XX_PM_MPU_PWRSTST_OFFSET,
	.cds = {&cd_cortexa15, NULL},
};

/* Voltage domains to power domains */

static struct d_pwrd_info *vdd_wkup_pds[] = {
		&pd_emu, &pd_wkupaon, NULL};

static struct d_pwrd_info *vdd_core_pds[] = {
		&pd_alwon_core, &pd_cam, &pd_core,
#ifdef CONFIG_ARCH_OMAP5_ES1
		&pd_dss, &pd_l3_init, &pd_l4_per,
#else
		&pd_dss, &pd_l3_init,
#endif
		&pd_std_efuse, &pd_audio, NULL};

static struct d_pwrd_info *vdd_mm_pds[] = {
		&pd_dsp, &pd_gpu, &pd_alwon_mm,
		&pd_iva, NULL};

static struct d_pwrd_info *vdd_mpu_pds[] = {
		&pd_alwon_mpu, &pd_mpu,
		/* &pd_cpu0, &pd_cpu1, */ NULL};

/* Voltage domains */

struct d_vdd_info d_vddinfo_omap5[N_VDDS] =  {
	{
		.name = "VDD_WKUP",
		.auto_ctrl_shift = -1,
		.auto_ctrl_mask = -1,
		.pds = vdd_wkup_pds,
	},
	{
		.name = "VDD_CORE",
		.auto_ctrl_shift = OMAP54XX_AUTO_CTRL_VDD_CORE_L_SHIFT,
		.auto_ctrl_mask = OMAP54XX_AUTO_CTRL_VDD_CORE_L_MASK,
		.pds = vdd_core_pds,
	},
	{
		.name = "VDD_MM",
		.auto_ctrl_shift = OMAP54XX_AUTO_CTRL_VDD_MM_L_SHIFT,
		.auto_ctrl_mask = OMAP54XX_AUTO_CTRL_VDD_MM_L_MASK,
		.pds = vdd_mm_pds,
	},
	{
		.name = "VDD_MPU",
		.auto_ctrl_shift = OMAP54XX_AUTO_CTRL_VDD_MPU_L_SHIFT,
		.auto_ctrl_mask = OMAP54XX_AUTO_CTRL_VDD_MPU_L_MASK,
		.pds = vdd_mpu_pds,
	},
};

#define OMAP5_CONTROL_PADCONF_WAKEUPEVENT_0		0x4A0029FC
#define OMAP5_CONTROL_WKUP_PADCONF_WAKEUPEVENT_0	0x4AE0C880

struct d_prcm_regs_info omap5_prcm_regs_data = {
	.dpll_en_mask = OMAP54XX_DPLL_EN_MASK,
	.dpll_autoidle_mask = OMAP54XX_AUTO_DPLL_MODE_MASK,
	.clkctrl_stbyst_mask = OMAP54XX_STBYST_MASK,
	.clkctrl_stbyst_shift = OMAP54XX_STBYST_SHIFT,
	.clkctrl_idlest_mask = OMAP54XX_IDLEST_MASK,
	.clkctrl_idlest_shift = OMAP54XX_IDLEST_SHIFT,
	.clkctrl_module_mode_mask = OMAP54XX_MODULEMODE_MASK,
	.clkctrl_module_mode_shift = OMAP54XX_MODULEMODE_SHIFT,
	.clkstctrl_offset = OMAP4_CM_CLKSTCTRL,
	.clktrctrl_mask = OMAP54XX_CLKTRCTRL_MASK,
	.clktrctrl_shift = OMAP54XX_CLKTRCTRL_SHIFT,
	.prm_pwrstatest_mask = OMAP54XX_POWERSTATEST_MASK,
	.prm_pwrstatest_shift = OMAP54XX_POWERSTATEST_SHIFT,
	.prm_lastpwrstatentered_mask = OMAP54XX_LASTPOWERSTATEENTERED_MASK,
	.prm_lastpwrstatentered_shift = OMAP54XX_LASTPOWERSTATEENTERED_SHIFT,
	.prm_logicstatest_mask = OMAP54XX_LOGICSTATEST_MASK,
	.prm_logicstatest_shift = OMAP54XX_LOGICSTATEST_SHIFT,
	.prm_dev_instance = OMAP54XX_PRM_DEVICE_INST,
	.prm_volctrl_offset = OMAP54XX_PRM_VOLTCTRL_OFFSET,
	.prm_io_st_mask = OMAP54XX_IO_ST_MASK,
	.control_padconf_wakeupevent0 = OMAP5_CONTROL_PADCONF_WAKEUPEVENT_0,
	.control_wkup_padconf_wakeupevent0 =
			OMAP5_CONTROL_WKUP_PADCONF_WAKEUPEVENT_0,
};

struct d_prcm_regs_info *d_prcm_regs_omap5 = &omap5_prcm_regs_data;
