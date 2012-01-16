/*
 * linux/arch/arm/mach-omap2/prcm.c
 *
 * OMAP 24xx Power Reset and Clock Management (PRCM) functions
 *
 * Copyright (C) 2005 Nokia Corporation
 *
 * Written by Tony Lindgren <tony.lindgren@nokia.com>
 *
 * Copyright (C) 2007 Texas Instruments, Inc.
 * Rajendra Nayak <rnayak@ti.com>
 *
 * Some pieces of code Copyright (C) 2005 Texas Instruments, Inc.
 * Upgraded with OMAP4 support by Abhijit Pagare <abhijitpagare@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/delay.h>

#include <plat/common.h>
#include <plat/prcm.h>
#include <plat/irqs.h>
#include <plat/control.h>

#include "clock.h"
#include "clock2xxx.h"
#include "cm.h"
#include "prm.h"
#include "prm-regbits-24xx.h"
#include "prm-regbits-44xx.h"
#include "cm-regbits-44xx.h"

static void __iomem *prm_base;
static void __iomem *prcm_mpu_base;
static void __iomem *cm_base;
static void __iomem *cm2_base;

#define MAX_MODULE_ENABLE_WAIT	100000
#define MAX_DPLL_WAIT_TRIES	1000000

struct omap3_prcm_regs {
	u32 control_padconf_sys_nirq;
	u32 iva2_cm_clksel1;
	u32 iva2_cm_clksel2;
	u32 cm_sysconfig;
	u32 sgx_cm_clksel;
	u32 dss_cm_clksel;
	u32 cam_cm_clksel;
	u32 per_cm_clksel;
	u32 emu_cm_clksel;
	u32 emu_cm_clkstctrl;
	u32 pll_cm_autoidle2;
	u32 pll_cm_clksel4;
	u32 pll_cm_clksel5;
	u32 pll_cm_clken2;
	u32 cm_polctrl;
	u32 iva2_cm_fclken;
	u32 iva2_cm_clken_pll;
	u32 core_cm_fclken1;
	u32 core_cm_fclken3;
	u32 sgx_cm_fclken;
	u32 wkup_cm_fclken;
	u32 dss_cm_fclken;
	u32 cam_cm_fclken;
	u32 per_cm_fclken;
	u32 usbhost_cm_fclken;
	u32 core_cm_iclken1;
	u32 core_cm_iclken2;
	u32 core_cm_iclken3;
	u32 sgx_cm_iclken;
	u32 wkup_cm_iclken;
	u32 dss_cm_iclken;
	u32 cam_cm_iclken;
	u32 per_cm_iclken;
	u32 usbhost_cm_iclken;
	u32 iva2_cm_autiidle2;
	u32 mpu_cm_autoidle2;
	u32 iva2_cm_clkstctrl;
	u32 mpu_cm_clkstctrl;
	u32 core_cm_clkstctrl;
	u32 sgx_cm_clkstctrl;
	u32 dss_cm_clkstctrl;
	u32 cam_cm_clkstctrl;
	u32 per_cm_clkstctrl;
	u32 neon_cm_clkstctrl;
	u32 usbhost_cm_clkstctrl;
	u32 core_cm_autoidle1;
	u32 core_cm_autoidle2;
	u32 core_cm_autoidle3;
	u32 wkup_cm_autoidle;
	u32 dss_cm_autoidle;
	u32 cam_cm_autoidle;
	u32 per_cm_autoidle;
	u32 usbhost_cm_autoidle;
	u32 sgx_cm_sleepdep;
	u32 dss_cm_sleepdep;
	u32 cam_cm_sleepdep;
	u32 per_cm_sleepdep;
	u32 usbhost_cm_sleepdep;
	u32 cm_clkout_ctrl;
	u32 prm_clkout_ctrl;
	u32 sgx_pm_wkdep;
	u32 dss_pm_wkdep;
	u32 cam_pm_wkdep;
	u32 per_pm_wkdep;
	u32 neon_pm_wkdep;
	u32 usbhost_pm_wkdep;
	u32 core_pm_mpugrpsel1;
	u32 iva2_pm_ivagrpsel1;
	u32 core_pm_mpugrpsel3;
	u32 core_pm_ivagrpsel3;
	u32 wkup_pm_mpugrpsel;
	u32 wkup_pm_ivagrpsel;
	u32 per_pm_mpugrpsel;
	u32 per_pm_ivagrpsel;
	u32 wkup_pm_wken;
};

#define MAX_CM_REGISTERS 51
#define NO_DPLL_RESTORE	5
#define NO_CM1_MODULES	5
#define NO_CM2_MODULES	11

struct tuple {
	u16 addr;
	u32 val;
};

struct omap4_dpll_regs {
	u32 mod_off;
	struct tuple clkmode;
	struct tuple autoidle;
	struct tuple idlest;
	struct tuple clksel;
	struct tuple div_m2;
	struct tuple div_m3;
	struct tuple div_m4;
	struct tuple div_m5;
	struct tuple div_m6;
	struct tuple div_m7;
	struct tuple clkdcoldo;
};

struct omap4_cm_regs {
	u32 mod_off;
	u32 no_reg;
	struct tuple reg[MAX_CM_REGISTERS];
};

#ifdef CONFIG_ARCH_OMAP3
static struct omap3_prcm_regs prcm_context;
#endif


#ifdef CONFIG_ARCH_OMAP4
static struct omap4_cm_regs cm1_regs[NO_CM1_MODULES] = {
	/* OMAP4430_CM1_OCP_SOCKET_MOD */
	{ OMAP4430_CM1_OCP_SOCKET_MOD, 1,
	{{OMAP4_CM_CM1_PROFILING_CLKCTRL_OFFSET, 0x0} },
	},
	/* OMAP4430_CM1_CKGEN_MOD */
	{ OMAP4430_CM1_CKGEN_MOD, 4,
	{{OMAP4_CM_CLKSEL_CORE_OFFSET, 0x0},
	 {OMAP4_CM_CLKSEL_ABE_OFFSET, 0x0},
	 {OMAP4_CM_DLL_CTRL_OFFSET, 0x0},
	 {OMAP4_CM_DYN_DEP_PRESCAL_OFFSET, 0x0} },
	},
	/* OMAP4430_CM1_MPU_MOD */
	{ OMAP4430_CM1_MPU_MOD, 4,
	{{OMAP4_CM_MPU_CLKSTCTRL_OFFSET, 0x0},
	 {OMAP4_CM_MPU_STATICDEP_OFFSET, 0x0},
	 {OMAP4_CM_MPU_DYNAMICDEP_OFFSET, 0x0},
	 {OMAP4_CM_MPU_MPU_CLKCTRL_OFFSET, 0x0} },
	},
	/* OMAP4430_CM1_TESLA_MOD */
	{ OMAP4430_CM1_TESLA_MOD, 4,
	{{OMAP4_CM_TESLA_CLKSTCTRL_OFFSET, 0x0},
	 {OMAP4_CM_TESLA_STATICDEP_OFFSET, 0x0},
	 {OMAP4_CM_TESLA_DYNAMICDEP_OFFSET, 0x0},
	 {OMAP4_CM_TESLA_TESLA_CLKCTRL_OFFSET, 0x0} },
	},
	/* OMAP4430_CM1_ABE_MOD */
	{ OMAP4430_CM1_ABE_MOD, 15,
	{{OMAP4_CM1_ABE_CLKSTCTRL_OFFSET, 0x0},
	 {OMAP4_CM1_ABE_L4ABE_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM1_ABE_AESS_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM1_ABE_PDM_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM1_ABE_DMIC_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM1_ABE_MCASP_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM1_ABE_MCBSP1_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM1_ABE_MCBSP2_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM1_ABE_MCBSP3_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM1_ABE_SLIMBUS_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM1_ABE_TIMER5_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM1_ABE_TIMER6_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM1_ABE_TIMER7_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM1_ABE_TIMER8_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM1_ABE_WDT3_CLKCTRL_OFFSET, 0x0} },
	},
};

static struct omap4_cm_regs cm2_regs[NO_CM2_MODULES] = {
	/* OMAP4430_CM2_OCP_SOCKET_MOD */
	{ OMAP4430_CM2_OCP_SOCKET_MOD, 1,
	{{OMAP4_CM_CM2_PROFILING_CLKCTRL_OFFSET, 0x0} },
	},
	/* OMAP4430_CM2_CKGEN_MOD */
	{ OMAP4430_CM2_CKGEN_MOD, 12,
	{{OMAP4_CM_CLKSEL_DUCATI_ISS_ROOT_OFFSET, 0x0},
	 {OMAP4_CM_CLKSEL_USB_60MHZ_OFFSET, 0x0},
	 {OMAP4_CM_SCALE_FCLK_OFFSET, 0x0},
	 {OMAP4_CM_CORE_DVFS_PERF1_OFFSET, 0x0},
	 {OMAP4_CM_CORE_DVFS_PERF2_OFFSET, 0x0},
	 {OMAP4_CM_CORE_DVFS_PERF3_OFFSET, 0x0},
	 {OMAP4_CM_CORE_DVFS_PERF4_OFFSET, 0x0},
	 {OMAP4_CM_CORE_DVFS_CURRENT_OFFSET, 0x0},
	 {OMAP4_CM_IVA_DVFS_PERF_TESLA_OFFSET, 0x0},
	 {OMAP4_CM_IVA_DVFS_PERF_IVAHD_OFFSET, 0x0},
	 {OMAP4_CM_IVA_DVFS_PERF_ABE_OFFSET, 0x0},
	 {OMAP4_CM_IVA_DVFS_CURRENT_OFFSET, 0x0} },
	},
	/* OMAP4430_CM2_ALWAYS_ON_MOD */
	{ OMAP4430_CM2_ALWAYS_ON_MOD, 6,
	{{OMAP4_CM_ALWON_CLKSTCTRL_OFFSET, 0x0},
	 {OMAP4_CM_ALWON_MDMINTC_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM_ALWON_SR_MPU_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM_ALWON_SR_IVA_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM_ALWON_SR_CORE_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM_ALWON_USBPHY_CLKCTRL_OFFSET, 0x0} },
	},
	/* OMAP4430_CM2_CORE_MOD */
	{ OMAP4430_CM2_CORE_MOD, 41,
	{{OMAP4_CM_L3_1_CLKSTCTRL_OFFSET, 0x0},
	 {OMAP4_CM_L3_1_DYNAMICDEP_OFFSET, 0x0},
	 {OMAP4_CM_L3_1_L3_1_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM_L3_2_CLKSTCTRL_OFFSET, 0x0},
	 {OMAP4_CM_L3_2_DYNAMICDEP_OFFSET, 0x0},
	 {OMAP4_CM_L3_2_L3_2_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM_L3_2_GPMC_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM_L3_2_OCMC_RAM_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM_DUCATI_CLKSTCTRL_OFFSET, 0x0},
	 {OMAP4_CM_DUCATI_STATICDEP_OFFSET, 0x0},
	 {OMAP4_CM_DUCATI_DYNAMICDEP_OFFSET, 0x0},
	 {OMAP4_CM_DUCATI_DUCATI_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM_SDMA_CLKSTCTRL_OFFSET, 0x0},
	 {OMAP4_CM_SDMA_STATICDEP_OFFSET, 0x0},
	 {OMAP4_CM_SDMA_DYNAMICDEP_OFFSET, 0x0},
	 {OMAP4_CM_SDMA_SDMA_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM_MEMIF_CLKSTCTRL_OFFSET, 0x0},
	 {OMAP4_CM_MEMIF_DMM_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM_MEMIF_EMIF_FW_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM_MEMIF_EMIF_1_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM_MEMIF_EMIF_2_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM_MEMIF_DLL_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM_MEMIF_EMIF_H1_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM_MEMIF_EMIF_H2_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM_MEMIF_DLL_H_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM_D2D_CLKSTCTRL_OFFSET, 0x0},
	 {OMAP4_CM_D2D_STATICDEP_OFFSET, 0x0},
	 {OMAP4_CM_D2D_DYNAMICDEP_OFFSET, 0x0},
	 {OMAP4_CM_D2D_SAD2D_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM_D2D_MODEM_ICR_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM_D2D_SAD2D_FW_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM_L4CFG_CLKSTCTRL_OFFSET, 0x0},
	 {OMAP4_CM_L4CFG_DYNAMICDEP_OFFSET, 0x0},
	 {OMAP4_CM_L4CFG_L4_CFG_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM_L4CFG_HW_SEM_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM_L4CFG_MAILBOX_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM_L4CFG_SAR_ROM_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM_L3INSTR_CLKSTCTRL_OFFSET, 0x0},
	 {OMAP4_CM_L3INSTR_L3_3_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM_L3INSTR_L3_INSTR_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM_L3INSTR_OCP_WP1_CLKCTRL_OFFSET, 0x0} },
	},
	/* OMAP4430_CM2_IVAHD_MOD */
	{ OMAP4430_CM2_IVAHD_MOD, 5,
	{{OMAP4_CM_IVAHD_CLKSTCTRL_OFFSET, 0x0},
	 {OMAP4_CM_IVAHD_STATICDEP_OFFSET, 0x0},
	 {OMAP4_CM_IVAHD_DYNAMICDEP_OFFSET, 0x0},
	 {OMAP4_CM_IVAHD_IVAHD_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM_IVAHD_SL2_CLKCTRL_OFFSET, 0x0} },
	},
	/* OMAP4430_CM2_CAM_MOD */
	{ OMAP4430_CM2_CAM_MOD, 5,
	{{OMAP4_CM_CAM_CLKSTCTRL_OFFSET, 0x0},
	 {OMAP4_CM_CAM_STATICDEP_OFFSET, 0x0},
	 {OMAP4_CM_CAM_DYNAMICDEP_OFFSET, 0x0},
	 {OMAP4_CM_CAM_ISS_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM_CAM_FDIF_CLKCTRL_OFFSET, 0x0} },
	},
	/* OMAP4430_CM2_DSS_MOD */
	{ OMAP4430_CM2_DSS_MOD, 5,
	{{OMAP4_CM_DSS_CLKSTCTRL_OFFSET, 0x0},
	 {OMAP4_CM_DSS_STATICDEP_OFFSET, 0x0},
	 {OMAP4_CM_DSS_DYNAMICDEP_OFFSET, 0x0},
	 {OMAP4_CM_DSS_DSS_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM_DSS_DEISS_CLKCTRL_OFFSET, 0x0} },
	},
	/* OMAP4430_CM2_GFX_MOD */
	{ OMAP4430_CM2_GFX_MOD, 4,
	{{OMAP4_CM_GFX_CLKSTCTRL_OFFSET, 0x0},
	 {OMAP4_CM_GFX_STATICDEP_OFFSET, 0x0},
	 {OMAP4_CM_GFX_DYNAMICDEP_OFFSET, 0x0},
	 {OMAP4_CM_GFX_GFX_CLKCTRL_OFFSET, 0x0} },
	},
	/* OMAP4430_CM2_L3INIT_MOD */
	{ OMAP4430_CM2_L3INIT_MOD, 20,
	{{OMAP4_CM_L3INIT_CLKSTCTRL_OFFSET, 0x0},
	 {OMAP4_CM_L3INIT_STATICDEP_OFFSET, 0x0},
	 {OMAP4_CM_L3INIT_DYNAMICDEP_OFFSET, 0x0},
	 {OMAP4_CM_L3INIT_MMC1_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM_L3INIT_MMC2_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM_L3INIT_HSI_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM_L3INIT_UNIPRO1_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM_L3INIT_USB_HOST_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM_L3INIT_USB_OTG_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM_L3INIT_USB_TLL_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM_L3INIT_P1500_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM_L3INIT_EMAC_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM_L3INIT_SATA_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM_L3INIT_TPPSS_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM_L3INIT_PCIESS_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM_L3INIT_CCPTX_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM_L3INIT_XHPI_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM_L3INIT_MMC6_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM_L3INIT_USB_HOST_FS_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM_L3INIT_USBPHYOCP2SCP_CLKCTRL_OFFSET, 0x0} },
	},
	/* OMAP4430_CM2_L4PER_MOD */
	{ OMAP4430_CM2_L4PER_MOD, 51,
	{{OMAP4_CM_L4PER_CLKSTCTRL_OFFSET, 0x0},
	 {OMAP4_CM_L4PER_DYNAMICDEP_OFFSET, 0x0},
	 {OMAP4_CM_L4PER_ADC_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM_L4PER_DMTIMER10_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM_L4PER_DMTIMER11_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM_L4PER_DMTIMER2_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM_L4PER_DMTIMER3_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM_L4PER_DMTIMER4_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM_L4PER_DMTIMER9_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM_L4PER_ELM_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM_L4PER_GPIO2_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM_L4PER_GPIO3_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM_L4PER_GPIO4_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM_L4PER_GPIO5_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM_L4PER_GPIO6_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM_L4PER_HDQ1W_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM_L4PER_HECC1_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM_L4PER_HECC2_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM_L4PER_I2C1_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM_L4PER_I2C2_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM_L4PER_I2C3_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM_L4PER_I2C4_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM_L4PER_L4PER_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM_L4PER_MCASP2_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM_L4PER_MCASP3_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM_L4PER_MCBSP4_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM_L4PER_MGATE_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM_L4PER_MCSPI1_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM_L4PER_MCSPI2_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM_L4PER_MCSPI3_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM_L4PER_MCSPI4_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM_L4PER_MMCSD3_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM_L4PER_MMCSD4_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM_L4PER_MSPROHG_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM_L4PER_SLIMBUS2_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM_L4PER_UART1_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM_L4PER_UART2_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM_L4PER_UART3_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM_L4PER_UART4_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM_L4PER_MMCSD5_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM_L4PER_I2C5_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM_L4SEC_CLKSTCTRL_OFFSET, 0x0},
	 {OMAP4_CM_L4SEC_STATICDEP_OFFSET, 0x0},
	 {OMAP4_CM_L4SEC_DYNAMICDEP_OFFSET, 0x0},
	 {OMAP4_CM_L4SEC_AES1_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM_L4SEC_AES2_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM_L4SEC_DES3DES_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM_L4SEC_PKAEIP29_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM_L4SEC_RNG_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM_L4SEC_SHA2MD51_CLKCTRL_OFFSET, 0x0},
	 {OMAP4_CM_L4SEC_CRYPTODMA_CLKCTRL_OFFSET, 0x0} },
	},
	/* OMAP4430_CM2_CEFUSE_MOD */
	{ OMAP4430_CM2_CEFUSE_MOD, 2,
	{{OMAP4_CM_CEFUSE_CLKSTCTRL_OFFSET, 0x0},
	 {OMAP4_CM_CEFUSE_CEFUSE_CLKCTRL_OFFSET, 0x0} },
	},
};

static struct omap4_dpll_regs dpll_regs[NO_DPLL_RESTORE] = {
	/* MPU DPLL */
	{ OMAP4430_CM1_CKGEN_MOD,
	 {OMAP4_CM_CLKMODE_DPLL_MPU_OFFSET, 0x0},
	 {OMAP4_CM_AUTOIDLE_DPLL_MPU_OFFSET, 0x0},
	 {OMAP4_CM_IDLEST_DPLL_MPU_OFFSET, 0x0},
	 {OMAP4_CM_CLKSEL_DPLL_MPU_OFFSET, 0x0},
	 {OMAP4_CM_DIV_M2_DPLL_MPU_OFFSET, 0x0},
	},
	/* IVA DPLL */
	{ OMAP4430_CM1_CKGEN_MOD,
	 {OMAP4_CM_CLKMODE_DPLL_IVA_OFFSET, 0x0},
	 {OMAP4_CM_AUTOIDLE_DPLL_IVA_OFFSET, 0x0},
	 {OMAP4_CM_IDLEST_DPLL_IVA_OFFSET, 0x0},
	 {OMAP4_CM_CLKSEL_DPLL_IVA_OFFSET, 0x0},
	 {0x0, 0x0},
	 {0x0, 0x0},
	 {OMAP4_CM_DIV_M4_DPLL_IVA_OFFSET, 0x0},
	 {OMAP4_CM_DIV_M5_DPLL_IVA_OFFSET, 0x0},
	},
	/* ABE DPLL */
	{ OMAP4430_CM1_CKGEN_MOD,
	 {OMAP4_CM_CLKMODE_DPLL_ABE_OFFSET, 0x0},
	 {OMAP4_CM_AUTOIDLE_DPLL_ABE_OFFSET, 0x0},
	 {OMAP4_CM_IDLEST_DPLL_ABE_OFFSET, 0x0},
	 {OMAP4_CM_CLKSEL_DPLL_ABE_OFFSET, 0x0},
	 {OMAP4_CM_DIV_M2_DPLL_ABE_OFFSET, 0x0},
	 {OMAP4_CM_DIV_M3_DPLL_ABE_OFFSET, 0x0},
	},
	/* USB DPLL */
	{ OMAP4430_CM2_CKGEN_MOD,
	 {OMAP4_CM_CLKMODE_DPLL_USB_OFFSET, 0x0},
	 {OMAP4_CM_AUTOIDLE_DPLL_USB_OFFSET, 0x0},
	 {OMAP4_CM_IDLEST_DPLL_USB_OFFSET, 0x0},
	 {OMAP4_CM_CLKSEL_DPLL_USB_OFFSET, 0x0},
	 {OMAP4_CM_DIV_M2_DPLL_USB_OFFSET, 0x0},
	 {0x0, 0x0},
	 {0x0, 0x0},
	 {0x0, 0x0},
	 {0x0, 0x0},
	 {0x0, 0x0},
	 {OMAP4_CM_CLKDCOLDO_DPLL_USB_OFFSET, 0x0},
	 },
	/* PER DPLL */
	{ OMAP4430_CM2_CKGEN_MOD,
	 {OMAP4_CM_CLKMODE_DPLL_PER_OFFSET, 0x0},
	 {OMAP4_CM_AUTOIDLE_DPLL_PER_OFFSET, 0x0},
	 {OMAP4_CM_IDLEST_DPLL_PER_OFFSET, 0x0},
	 {OMAP4_CM_CLKSEL_DPLL_PER_OFFSET, 0x0},
	 {OMAP4_CM_DIV_M2_DPLL_PER_OFFSET, 0x0},
	 {OMAP4_CM_DIV_M3_DPLL_PER_OFFSET, 0x0},
	 {OMAP4_CM_DIV_M4_DPLL_PER_OFFSET, 0x0},
	 {OMAP4_CM_DIV_M5_DPLL_PER_OFFSET, 0x0},
	 {OMAP4_CM_DIV_M6_DPLL_PER_OFFSET, 0x0},
	 {OMAP4_CM_DIV_M7_DPLL_PER_OFFSET, 0x0},
	},
};
#endif

u32 omap_prcm_get_reset_sources(void)
{
	/* XXX This presumably needs modification for 34XX */
	if (cpu_is_omap24xx() || cpu_is_omap34xx())
		return prm_read_mod_reg(WKUP_MOD, OMAP2_RM_RSTST) & 0x7f;
	if (cpu_is_omap44xx())
		return prm_read_mod_reg(WKUP_MOD, OMAP4_RM_RSTST) & 0x7f;

	return 0;
}
EXPORT_SYMBOL(omap_prcm_get_reset_sources);

#ifdef CONFIG_ARCH_OMAP4
extern void __iomem *sar_ram_base;
#define PUBLIC_SAR_RAM_1_FREE		((char *)(sar_ram_base + 0xA0C))
#else
#define PUBLIC_SAR_RAM_1_FREE		"null"
#endif

void omap4_prm_global_sw_reset(const char *cmd)
{
	u32 v = 0;

	v |= OMAP4430_RST_GLOBAL_COLD_SW_MASK;
	
        /* clear previous reboot status */
	prm_write_mod_reg(0xfff,
			OMAP4430_PRM_DEVICE_MOD,
			OMAP4_RM_RSTST);

	prm_write_mod_reg(v,
			OMAP4430_PRM_DEVICE_MOD,
			OMAP4_RM_RSTCTRL);

	/* OCP barrier */
	v = prm_read_mod_reg(WKUP_MOD, OMAP4_RM_RSTCTRL);
}

/* Resets clock rates and reboots the system. Only called from system.h */
void omap_prcm_arch_reset(char mode, const char *cmd)
{
	s16 prcm_offs = 0;

	if (cpu_is_omap24xx()) {
		omap2xxx_clk_prepare_for_reboot();

		prcm_offs = WKUP_MOD;
	} else if (cpu_is_omap34xx()) {
		u32 l;

		prcm_offs = OMAP3430_GR_MOD;
		l = ('B' << 24) | ('M' << 16) | (cmd ? (u8)*cmd : 0);
		/* Reserve the first word in scratchpad for communicating
		 * with the boot ROM. A pointer to a data structure
		 * describing the boot process can be stored there,
		 * cf. OMAP34xx TRM, Initialization / Software Booting
		 * Configuration. */
		omap_writel(l, OMAP343X_SCRATCHPAD + 4);
	} else if (cpu_is_omap44xx()) {
		omap4_clk_prepare_for_reboot();
		omap4_prm_global_sw_reset(cmd); /* never returns */
	} else
		WARN_ON(1);

	if (cpu_is_omap24xx() || cpu_is_omap34xx())
		prm_set_mod_reg_bits(OMAP_RST_DPLL3_MASK, prcm_offs,
						 OMAP2_RM_RSTCTRL);
}

static inline u32 __omap_prcm_read(void __iomem *base, s16 module, u16 reg)
{
	BUG_ON(!base);
	return __raw_readl(base + module + reg);
}

static inline void __omap_prcm_write(u32 value, void __iomem *base,
						s16 module, u16 reg)
{
	BUG_ON(!base);
	__raw_writel(value, base + module + reg);
}

/* Read a register in a PRM module */
u32 prm_read_mod_reg(s16 module, u16 idx)
{
	u32 base = 0;

	base = abs(module) >> BASE_ID_SHIFT;
	module &= MOD_MASK;

	switch (base) {
	case PRCM_MPU_BASE:
		return __omap_prcm_read(prcm_mpu_base, module, idx);
	case DEFAULT_BASE:
		return __omap_prcm_read(prm_base, module, idx);
	default:
		pr_err("Unknown PRM submodule base\n");
	}
	return 0;
}

/* Write into a register in a PRM module */
void prm_write_mod_reg(u32 val, s16 module, u16 idx)
{
	u32 base = 0;

	base = abs(module) >> BASE_ID_SHIFT;
	module &= MOD_MASK;

	switch (base) {
	case PRCM_MPU_BASE:
		__omap_prcm_write(val, prcm_mpu_base, module, idx);
		break;
	case DEFAULT_BASE:
		__omap_prcm_write(val, prm_base, module, idx);
		break;
	default:
		pr_err("Unknown PRM submodule base\n");
		break;
	}
}

/* Read-modify-write a register in a PRM module. Caller must lock */
u32 prm_rmw_mod_reg_bits(u32 mask, u32 bits, s16 module, s16 idx)
{
	u32 v;

	v = prm_read_mod_reg(module, idx);
	v &= ~mask;
	v |= bits;
	prm_write_mod_reg(v, module, idx);

	return v;
}

/* Read a PRM register, AND it, and shift the result down to bit 0 */
u32 prm_read_mod_bits_shift(s16 domain, s16 idx, u32 mask)
{
	u32 v;

	v = prm_read_mod_reg(domain, idx);
	v &= mask;
	v >>= __ffs(mask);

	return v;
}

/* Read a PRM register, AND it, and shift the result down to bit 0 */
u32 omap4_prm_read_bits_shift(void __iomem *reg, u32 mask)
{
	u32 v;

	v = __raw_readl(reg);
	v &= mask;
	v >>= __ffs(mask);

	return v;
}

/* Read-modify-write a register in a PRM module. Caller must lock */
u32 omap4_prm_rmw_reg_bits(u32 mask, u32 bits, void __iomem *reg)
{
	u32 v;

	v = __raw_readl(reg);
	v &= ~mask;
	v |= bits;
	__raw_writel(v, reg);

	return v;
}

/* Read a register in a CM module */
u32 cm_read_mod_reg(s16 module, u16 idx)
{
	u32 base = 0;

	base = abs(module) >> BASE_ID_SHIFT;
	module &= MOD_MASK;

	switch (base) {
	case PRM_BASE:
		return __omap_prcm_read(prm_base, module, idx);
	case CM2_BASE:
		return __omap_prcm_read(cm2_base, module, idx);
	case DEFAULT_BASE:
		return __omap_prcm_read(cm_base, module, idx);
	default:
		pr_err("Unknown CM submodule base\n");
	}
	return 0;
}

/* Write into a register in a CM module */
void cm_write_mod_reg(u32 val, s16 module, u16 idx)
{
	u32 base = 0;

	base = abs(module) >> BASE_ID_SHIFT;
	module &= MOD_MASK;

	switch (base) {
	case PRM_BASE:
		__omap_prcm_write(val, prm_base, module, idx);
		break;
	case CM2_BASE:
		__omap_prcm_write(val, cm2_base, module, idx);
		break;
	case DEFAULT_BASE:
		__omap_prcm_write(val, cm_base, module, idx);
		break;
	default:
		pr_err("Unknown CM submodule base\n");
		break;
	}
}

/* Read-modify-write a register in a CM module. Caller must lock */
u32 cm_rmw_mod_reg_bits(u32 mask, u32 bits, s16 module, s16 idx)
{
	u32 v;

	v = cm_read_mod_reg(module, idx);
	v &= ~mask;
	v |= bits;
	cm_write_mod_reg(v, module, idx);

	return v;
}

/**
 * omap2_cm_wait_idlest - wait for IDLEST bit to indicate module readiness
 * @reg: physical address of module IDLEST register
 * @mask: value to mask against to determine if the module is active
 * @idlest: idle state indicator (0 or 1) for the clock
 * @name: name of the clock (for printk)
 *
 * Returns 1 if the module indicated readiness in time, or 0 if it
 * failed to enable in roughly MAX_MODULE_ENABLE_WAIT microseconds.
 */
int omap2_cm_wait_idlest(void __iomem *reg, u32 mask, u8 idlest,
				const char *name)
{
	int i = 0;
	int ena = 0;

	if (idlest)
		ena = 0;
	else
		ena = mask;

	/* Wait for lock */
	omap_test_timeout(((__raw_readl(reg) & mask) == ena),
			  MAX_MODULE_ENABLE_WAIT, i);

	if (i < MAX_MODULE_ENABLE_WAIT)
		pr_debug("cm: Module associated with clock %s ready after %d "
			 "loops\n", name, i);
	else
		pr_err("cm: Module associated with clock %s didn't enable in "
		       "%d tries\n", name, MAX_MODULE_ENABLE_WAIT);

	return (i < MAX_MODULE_ENABLE_WAIT) ? 1 : 0;
};

void __init omap2_set_globals_prcm(struct omap_globals *omap2_globals)
{
	/* Static mapping, never released */
	if (omap2_globals->prm) {
		prm_base = ioremap(omap2_globals->prm, SZ_8K);
		WARN_ON(!prm_base);
	}
	if (omap2_globals->prcm_mpu) {
		prcm_mpu_base = ioremap(omap2_globals->prcm_mpu, SZ_8K);
		WARN_ON(!prcm_mpu_base);
	}
	if (omap2_globals->cm) {
		cm_base = ioremap(omap2_globals->cm, SZ_8K);
		WARN_ON(!cm_base);
	}
	if (omap2_globals->cm2) {
		cm2_base = ioremap(omap2_globals->cm2, SZ_8K);
		WARN_ON(!cm2_base);
	}
}

#ifdef CONFIG_ARCH_OMAP3
void omap3_prcm_save_context(void)
{
	prcm_context.control_padconf_sys_nirq =
			 omap_ctrl_readl(OMAP343X_CONTROL_PADCONF_SYSNIRQ);
	prcm_context.iva2_cm_clksel1 =
			 cm_read_mod_reg(OMAP3430_IVA2_MOD, CM_CLKSEL1);
	prcm_context.iva2_cm_clksel2 =
			 cm_read_mod_reg(OMAP3430_IVA2_MOD, CM_CLKSEL2);
	prcm_context.cm_sysconfig = __raw_readl(OMAP3430_CM_SYSCONFIG);
	prcm_context.sgx_cm_clksel =
			 cm_read_mod_reg(OMAP3430ES2_SGX_MOD, CM_CLKSEL);
	prcm_context.dss_cm_clksel =
			 cm_read_mod_reg(OMAP3430_DSS_MOD, CM_CLKSEL);
	prcm_context.cam_cm_clksel =
			 cm_read_mod_reg(OMAP3430_CAM_MOD, CM_CLKSEL);
	prcm_context.per_cm_clksel =
			 cm_read_mod_reg(OMAP3430_PER_MOD, CM_CLKSEL);
	prcm_context.emu_cm_clksel =
			 cm_read_mod_reg(OMAP3430_EMU_MOD, CM_CLKSEL1);
	prcm_context.emu_cm_clkstctrl =
			 cm_read_mod_reg(OMAP3430_EMU_MOD, OMAP2_CM_CLKSTCTRL);
	prcm_context.pll_cm_autoidle2 =
			 cm_read_mod_reg(PLL_MOD, CM_AUTOIDLE2);
	prcm_context.pll_cm_clksel4 =
			cm_read_mod_reg(PLL_MOD, OMAP3430ES2_CM_CLKSEL4);
	prcm_context.pll_cm_clksel5 =
			 cm_read_mod_reg(PLL_MOD, OMAP3430ES2_CM_CLKSEL5);
	prcm_context.pll_cm_clken2 =
			cm_read_mod_reg(PLL_MOD, OMAP3430ES2_CM_CLKEN2);
	prcm_context.cm_polctrl = __raw_readl(OMAP3430_CM_POLCTRL);
	prcm_context.iva2_cm_fclken =
			 cm_read_mod_reg(OMAP3430_IVA2_MOD, CM_FCLKEN);
	prcm_context.iva2_cm_clken_pll = cm_read_mod_reg(OMAP3430_IVA2_MOD,
			OMAP3430_CM_CLKEN_PLL);
	prcm_context.core_cm_fclken1 =
			 cm_read_mod_reg(CORE_MOD, CM_FCLKEN1);
	prcm_context.core_cm_fclken3 =
			 cm_read_mod_reg(CORE_MOD, OMAP3430ES2_CM_FCLKEN3);
	prcm_context.sgx_cm_fclken =
			 cm_read_mod_reg(OMAP3430ES2_SGX_MOD, CM_FCLKEN);
	prcm_context.wkup_cm_fclken =
			 cm_read_mod_reg(WKUP_MOD, CM_FCLKEN);
	prcm_context.dss_cm_fclken =
			 cm_read_mod_reg(OMAP3430_DSS_MOD, CM_FCLKEN);
	prcm_context.cam_cm_fclken =
			 cm_read_mod_reg(OMAP3430_CAM_MOD, CM_FCLKEN);
	prcm_context.per_cm_fclken =
			 cm_read_mod_reg(OMAP3430_PER_MOD, CM_FCLKEN);
	prcm_context.usbhost_cm_fclken =
			 cm_read_mod_reg(OMAP3430ES2_USBHOST_MOD, CM_FCLKEN);
	prcm_context.core_cm_iclken1 =
			 cm_read_mod_reg(CORE_MOD, CM_ICLKEN1);
	prcm_context.core_cm_iclken2 =
			 cm_read_mod_reg(CORE_MOD, CM_ICLKEN2);
	prcm_context.core_cm_iclken3 =
			 cm_read_mod_reg(CORE_MOD, CM_ICLKEN3);
	prcm_context.sgx_cm_iclken =
			 cm_read_mod_reg(OMAP3430ES2_SGX_MOD, CM_ICLKEN);
	prcm_context.wkup_cm_iclken =
			 cm_read_mod_reg(WKUP_MOD, CM_ICLKEN);
	prcm_context.dss_cm_iclken =
			 cm_read_mod_reg(OMAP3430_DSS_MOD, CM_ICLKEN);
	prcm_context.cam_cm_iclken =
			 cm_read_mod_reg(OMAP3430_CAM_MOD, CM_ICLKEN);
	prcm_context.per_cm_iclken =
			 cm_read_mod_reg(OMAP3430_PER_MOD, CM_ICLKEN);
	prcm_context.usbhost_cm_iclken =
			 cm_read_mod_reg(OMAP3430ES2_USBHOST_MOD, CM_ICLKEN);
	prcm_context.iva2_cm_autiidle2 =
			 cm_read_mod_reg(OMAP3430_IVA2_MOD, CM_AUTOIDLE2);
	prcm_context.mpu_cm_autoidle2 =
			 cm_read_mod_reg(MPU_MOD, CM_AUTOIDLE2);
	prcm_context.iva2_cm_clkstctrl =
			 cm_read_mod_reg(OMAP3430_IVA2_MOD, OMAP2_CM_CLKSTCTRL);
	prcm_context.mpu_cm_clkstctrl =
			 cm_read_mod_reg(MPU_MOD, OMAP2_CM_CLKSTCTRL);
	prcm_context.core_cm_clkstctrl =
			 cm_read_mod_reg(CORE_MOD, OMAP2_CM_CLKSTCTRL);
	prcm_context.sgx_cm_clkstctrl =
			 cm_read_mod_reg(OMAP3430ES2_SGX_MOD,
						OMAP2_CM_CLKSTCTRL);
	prcm_context.dss_cm_clkstctrl =
			 cm_read_mod_reg(OMAP3430_DSS_MOD, OMAP2_CM_CLKSTCTRL);
	prcm_context.cam_cm_clkstctrl =
			 cm_read_mod_reg(OMAP3430_CAM_MOD, OMAP2_CM_CLKSTCTRL);
	prcm_context.per_cm_clkstctrl =
			 cm_read_mod_reg(OMAP3430_PER_MOD, OMAP2_CM_CLKSTCTRL);
	prcm_context.neon_cm_clkstctrl =
			 cm_read_mod_reg(OMAP3430_NEON_MOD, OMAP2_CM_CLKSTCTRL);
	prcm_context.usbhost_cm_clkstctrl =
			 cm_read_mod_reg(OMAP3430ES2_USBHOST_MOD,
						OMAP2_CM_CLKSTCTRL);
	prcm_context.core_cm_autoidle1 =
			 cm_read_mod_reg(CORE_MOD, CM_AUTOIDLE1);
	prcm_context.core_cm_autoidle2 =
			 cm_read_mod_reg(CORE_MOD, CM_AUTOIDLE2);
	prcm_context.core_cm_autoidle3 =
			 cm_read_mod_reg(CORE_MOD, CM_AUTOIDLE3);
	prcm_context.wkup_cm_autoidle =
			 cm_read_mod_reg(WKUP_MOD, CM_AUTOIDLE);
	prcm_context.dss_cm_autoidle =
			 cm_read_mod_reg(OMAP3430_DSS_MOD, CM_AUTOIDLE);
	prcm_context.cam_cm_autoidle =
			 cm_read_mod_reg(OMAP3430_CAM_MOD, CM_AUTOIDLE);
	prcm_context.per_cm_autoidle =
			 cm_read_mod_reg(OMAP3430_PER_MOD, CM_AUTOIDLE);
	prcm_context.usbhost_cm_autoidle =
			 cm_read_mod_reg(OMAP3430ES2_USBHOST_MOD, CM_AUTOIDLE);
	prcm_context.sgx_cm_sleepdep =
		 cm_read_mod_reg(OMAP3430ES2_SGX_MOD, OMAP3430_CM_SLEEPDEP);
	prcm_context.dss_cm_sleepdep =
		 cm_read_mod_reg(OMAP3430_DSS_MOD, OMAP3430_CM_SLEEPDEP);
	prcm_context.cam_cm_sleepdep =
		 cm_read_mod_reg(OMAP3430_CAM_MOD, OMAP3430_CM_SLEEPDEP);
	prcm_context.per_cm_sleepdep =
		 cm_read_mod_reg(OMAP3430_PER_MOD, OMAP3430_CM_SLEEPDEP);
	prcm_context.usbhost_cm_sleepdep =
		 cm_read_mod_reg(OMAP3430ES2_USBHOST_MOD, OMAP3430_CM_SLEEPDEP);
	prcm_context.cm_clkout_ctrl = cm_read_mod_reg(OMAP3430_CCR_MOD,
		 OMAP3_CM_CLKOUT_CTRL_OFFSET);
	prcm_context.prm_clkout_ctrl = prm_read_mod_reg(OMAP3430_CCR_MOD,
		OMAP3_PRM_CLKOUT_CTRL_OFFSET);
	prcm_context.sgx_pm_wkdep =
		 prm_read_mod_reg(OMAP3430ES2_SGX_MOD, PM_WKDEP);
	prcm_context.dss_pm_wkdep =
		 prm_read_mod_reg(OMAP3430_DSS_MOD, PM_WKDEP);
	prcm_context.cam_pm_wkdep =
		 prm_read_mod_reg(OMAP3430_CAM_MOD, PM_WKDEP);
	prcm_context.per_pm_wkdep =
		 prm_read_mod_reg(OMAP3430_PER_MOD, PM_WKDEP);
	prcm_context.neon_pm_wkdep =
		 prm_read_mod_reg(OMAP3430_NEON_MOD, PM_WKDEP);
	prcm_context.usbhost_pm_wkdep =
		 prm_read_mod_reg(OMAP3430ES2_USBHOST_MOD, PM_WKDEP);
	prcm_context.core_pm_mpugrpsel1 =
		 prm_read_mod_reg(CORE_MOD, OMAP3430_PM_MPUGRPSEL1);
	prcm_context.iva2_pm_ivagrpsel1 =
		 prm_read_mod_reg(OMAP3430_IVA2_MOD, OMAP3430_PM_IVAGRPSEL1);
	prcm_context.core_pm_mpugrpsel3 =
		 prm_read_mod_reg(CORE_MOD, OMAP3430ES2_PM_MPUGRPSEL3);
	prcm_context.core_pm_ivagrpsel3 =
		 prm_read_mod_reg(CORE_MOD, OMAP3430ES2_PM_IVAGRPSEL3);
	prcm_context.wkup_pm_mpugrpsel =
		 prm_read_mod_reg(WKUP_MOD, OMAP3430_PM_MPUGRPSEL);
	prcm_context.wkup_pm_ivagrpsel =
		 prm_read_mod_reg(WKUP_MOD, OMAP3430_PM_IVAGRPSEL);
	prcm_context.per_pm_mpugrpsel =
		 prm_read_mod_reg(OMAP3430_PER_MOD, OMAP3430_PM_MPUGRPSEL);
	prcm_context.per_pm_ivagrpsel =
		 prm_read_mod_reg(OMAP3430_PER_MOD, OMAP3430_PM_IVAGRPSEL);
	prcm_context.wkup_pm_wken = prm_read_mod_reg(WKUP_MOD, PM_WKEN);
	return;
}

void omap3_prcm_restore_context(void)
{
	omap_ctrl_writel(prcm_context.control_padconf_sys_nirq,
					 OMAP343X_CONTROL_PADCONF_SYSNIRQ);
	cm_write_mod_reg(prcm_context.iva2_cm_clksel1, OMAP3430_IVA2_MOD,
					 CM_CLKSEL1);
	cm_write_mod_reg(prcm_context.iva2_cm_clksel2, OMAP3430_IVA2_MOD,
					 CM_CLKSEL2);
	__raw_writel(prcm_context.cm_sysconfig, OMAP3430_CM_SYSCONFIG);
	cm_write_mod_reg(prcm_context.sgx_cm_clksel, OMAP3430ES2_SGX_MOD,
					 CM_CLKSEL);
	cm_write_mod_reg(prcm_context.dss_cm_clksel, OMAP3430_DSS_MOD,
					 CM_CLKSEL);
	cm_write_mod_reg(prcm_context.cam_cm_clksel, OMAP3430_CAM_MOD,
					 CM_CLKSEL);
	cm_write_mod_reg(prcm_context.per_cm_clksel, OMAP3430_PER_MOD,
					 CM_CLKSEL);
	cm_write_mod_reg(prcm_context.emu_cm_clksel, OMAP3430_EMU_MOD,
					 CM_CLKSEL1);
	cm_write_mod_reg(prcm_context.emu_cm_clkstctrl, OMAP3430_EMU_MOD,
					 OMAP2_CM_CLKSTCTRL);
	cm_write_mod_reg(prcm_context.pll_cm_autoidle2, PLL_MOD,
					 CM_AUTOIDLE2);
	cm_write_mod_reg(prcm_context.pll_cm_clksel4, PLL_MOD,
					OMAP3430ES2_CM_CLKSEL4);
	cm_write_mod_reg(prcm_context.pll_cm_clksel5, PLL_MOD,
					 OMAP3430ES2_CM_CLKSEL5);
	cm_write_mod_reg(prcm_context.pll_cm_clken2, PLL_MOD,
					OMAP3430ES2_CM_CLKEN2);
	__raw_writel(prcm_context.cm_polctrl, OMAP3430_CM_POLCTRL);
	cm_write_mod_reg(prcm_context.iva2_cm_fclken, OMAP3430_IVA2_MOD,
					 CM_FCLKEN);
	cm_write_mod_reg(prcm_context.iva2_cm_clken_pll, OMAP3430_IVA2_MOD,
					OMAP3430_CM_CLKEN_PLL);
	cm_write_mod_reg(prcm_context.core_cm_fclken1, CORE_MOD, CM_FCLKEN1);
	cm_write_mod_reg(prcm_context.core_cm_fclken3, CORE_MOD,
					 OMAP3430ES2_CM_FCLKEN3);
	cm_write_mod_reg(prcm_context.sgx_cm_fclken, OMAP3430ES2_SGX_MOD,
					 CM_FCLKEN);
	cm_write_mod_reg(prcm_context.wkup_cm_fclken, WKUP_MOD, CM_FCLKEN);
	cm_write_mod_reg(prcm_context.dss_cm_fclken, OMAP3430_DSS_MOD,
					 CM_FCLKEN);
	cm_write_mod_reg(prcm_context.cam_cm_fclken, OMAP3430_CAM_MOD,
					 CM_FCLKEN);
	cm_write_mod_reg(prcm_context.per_cm_fclken, OMAP3430_PER_MOD,
					 CM_FCLKEN);
	cm_write_mod_reg(prcm_context.usbhost_cm_fclken,
					 OMAP3430ES2_USBHOST_MOD, CM_FCLKEN);
	cm_write_mod_reg(prcm_context.core_cm_iclken1, CORE_MOD, CM_ICLKEN1);
	cm_write_mod_reg(prcm_context.core_cm_iclken2, CORE_MOD, CM_ICLKEN2);
	cm_write_mod_reg(prcm_context.core_cm_iclken3, CORE_MOD, CM_ICLKEN3);
	cm_write_mod_reg(prcm_context.sgx_cm_iclken, OMAP3430ES2_SGX_MOD,
					CM_ICLKEN);
	cm_write_mod_reg(prcm_context.wkup_cm_iclken, WKUP_MOD, CM_ICLKEN);
	cm_write_mod_reg(prcm_context.dss_cm_iclken, OMAP3430_DSS_MOD,
					CM_ICLKEN);
	cm_write_mod_reg(prcm_context.cam_cm_iclken, OMAP3430_CAM_MOD,
					CM_ICLKEN);
	cm_write_mod_reg(prcm_context.per_cm_iclken, OMAP3430_PER_MOD,
					CM_ICLKEN);
	cm_write_mod_reg(prcm_context.usbhost_cm_iclken,
					OMAP3430ES2_USBHOST_MOD, CM_ICLKEN);
	cm_write_mod_reg(prcm_context.iva2_cm_autiidle2, OMAP3430_IVA2_MOD,
					CM_AUTOIDLE2);
	cm_write_mod_reg(prcm_context.mpu_cm_autoidle2, MPU_MOD, CM_AUTOIDLE2);
	cm_write_mod_reg(prcm_context.iva2_cm_clkstctrl, OMAP3430_IVA2_MOD,
					OMAP2_CM_CLKSTCTRL);
	cm_write_mod_reg(prcm_context.mpu_cm_clkstctrl, MPU_MOD,
					OMAP2_CM_CLKSTCTRL);
	cm_write_mod_reg(prcm_context.core_cm_clkstctrl, CORE_MOD,
					OMAP2_CM_CLKSTCTRL);
	cm_write_mod_reg(prcm_context.sgx_cm_clkstctrl, OMAP3430ES2_SGX_MOD,
					OMAP2_CM_CLKSTCTRL);
	cm_write_mod_reg(prcm_context.dss_cm_clkstctrl, OMAP3430_DSS_MOD,
					OMAP2_CM_CLKSTCTRL);
	cm_write_mod_reg(prcm_context.cam_cm_clkstctrl, OMAP3430_CAM_MOD,
					OMAP2_CM_CLKSTCTRL);
	cm_write_mod_reg(prcm_context.per_cm_clkstctrl, OMAP3430_PER_MOD,
					OMAP2_CM_CLKSTCTRL);
	cm_write_mod_reg(prcm_context.neon_cm_clkstctrl, OMAP3430_NEON_MOD,
					OMAP2_CM_CLKSTCTRL);
	cm_write_mod_reg(prcm_context.usbhost_cm_clkstctrl,
				OMAP3430ES2_USBHOST_MOD, OMAP2_CM_CLKSTCTRL);
	cm_write_mod_reg(prcm_context.core_cm_autoidle1, CORE_MOD,
					CM_AUTOIDLE1);
	cm_write_mod_reg(prcm_context.core_cm_autoidle2, CORE_MOD,
					CM_AUTOIDLE2);
	cm_write_mod_reg(prcm_context.core_cm_autoidle3, CORE_MOD,
					CM_AUTOIDLE3);
	cm_write_mod_reg(prcm_context.wkup_cm_autoidle, WKUP_MOD, CM_AUTOIDLE);
	cm_write_mod_reg(prcm_context.dss_cm_autoidle, OMAP3430_DSS_MOD,
					CM_AUTOIDLE);
	cm_write_mod_reg(prcm_context.cam_cm_autoidle, OMAP3430_CAM_MOD,
					CM_AUTOIDLE);
	cm_write_mod_reg(prcm_context.per_cm_autoidle, OMAP3430_PER_MOD,
					CM_AUTOIDLE);
	cm_write_mod_reg(prcm_context.usbhost_cm_autoidle,
					OMAP3430ES2_USBHOST_MOD, CM_AUTOIDLE);
	cm_write_mod_reg(prcm_context.sgx_cm_sleepdep, OMAP3430ES2_SGX_MOD,
					OMAP3430_CM_SLEEPDEP);
	cm_write_mod_reg(prcm_context.dss_cm_sleepdep, OMAP3430_DSS_MOD,
					OMAP3430_CM_SLEEPDEP);
	cm_write_mod_reg(prcm_context.cam_cm_sleepdep, OMAP3430_CAM_MOD,
					OMAP3430_CM_SLEEPDEP);
	cm_write_mod_reg(prcm_context.per_cm_sleepdep, OMAP3430_PER_MOD,
					OMAP3430_CM_SLEEPDEP);
	cm_write_mod_reg(prcm_context.usbhost_cm_sleepdep,
				OMAP3430ES2_USBHOST_MOD, OMAP3430_CM_SLEEPDEP);
	cm_write_mod_reg(prcm_context.cm_clkout_ctrl, OMAP3430_CCR_MOD,
					OMAP3_CM_CLKOUT_CTRL_OFFSET);
	prm_write_mod_reg(prcm_context.prm_clkout_ctrl, OMAP3430_CCR_MOD,
					OMAP3_PRM_CLKOUT_CTRL_OFFSET);
	prm_write_mod_reg(prcm_context.sgx_pm_wkdep, OMAP3430ES2_SGX_MOD,
					PM_WKDEP);
	prm_write_mod_reg(prcm_context.dss_pm_wkdep, OMAP3430_DSS_MOD,
					PM_WKDEP);
	prm_write_mod_reg(prcm_context.cam_pm_wkdep, OMAP3430_CAM_MOD,
					PM_WKDEP);
	prm_write_mod_reg(prcm_context.per_pm_wkdep, OMAP3430_PER_MOD,
					PM_WKDEP);
	prm_write_mod_reg(prcm_context.neon_pm_wkdep, OMAP3430_NEON_MOD,
					PM_WKDEP);
	prm_write_mod_reg(prcm_context.usbhost_pm_wkdep,
					OMAP3430ES2_USBHOST_MOD, PM_WKDEP);
	prm_write_mod_reg(prcm_context.core_pm_mpugrpsel1, CORE_MOD,
					OMAP3430_PM_MPUGRPSEL1);
	prm_write_mod_reg(prcm_context.iva2_pm_ivagrpsel1, OMAP3430_IVA2_MOD,
					OMAP3430_PM_IVAGRPSEL1);
	prm_write_mod_reg(prcm_context.core_pm_mpugrpsel3, CORE_MOD,
					OMAP3430ES2_PM_MPUGRPSEL3);
	prm_write_mod_reg(prcm_context.core_pm_ivagrpsel3, CORE_MOD,
					OMAP3430ES2_PM_IVAGRPSEL3);
	prm_write_mod_reg(prcm_context.wkup_pm_mpugrpsel, WKUP_MOD,
					OMAP3430_PM_MPUGRPSEL);
	prm_write_mod_reg(prcm_context.wkup_pm_ivagrpsel, WKUP_MOD,
					OMAP3430_PM_IVAGRPSEL);
	prm_write_mod_reg(prcm_context.per_pm_mpugrpsel, OMAP3430_PER_MOD,
					OMAP3430_PM_MPUGRPSEL);
	prm_write_mod_reg(prcm_context.per_pm_ivagrpsel, OMAP3430_PER_MOD,
					 OMAP3430_PM_IVAGRPSEL);
	prm_write_mod_reg(prcm_context.wkup_pm_wken, WKUP_MOD, PM_WKEN);
	return;
}
#endif

#ifdef CONFIG_ARCH_OMAP4
static void omap4_cm1_prepare_off(void)
{
	u32 i, j;

	for (i = 0; i < NO_CM1_MODULES; i++) {
		for (j = 0; j < cm1_regs[i].no_reg; j++) {
			cm1_regs[i].reg[j].val = cm_read_mod_reg(cm1_regs[i].mod_off,
				cm1_regs[i].reg[j].addr);
		}
	}
	return;
}

static void omap4_cm2_prepare_off(void)
{
	u32 i, j;

	for (i = 0; i < NO_CM2_MODULES; i++) {
		for (j = 0; j < cm2_regs[i].no_reg; j++) {
			cm2_regs[i].reg[j].val = cm_read_mod_reg(cm2_regs[i].mod_off,
				cm2_regs[i].reg[j].addr);
		}
	}
	return;
}

static void omap4_dpll_prepare_off(void)
{
	u32 i;

	for (i = 0; i < NO_DPLL_RESTORE; i++) {
		dpll_regs[i].clkmode.val = cm_read_mod_reg(dpll_regs[i].mod_off,
			dpll_regs[i].clkmode.addr);
		dpll_regs[i].autoidle.val = cm_read_mod_reg(dpll_regs[i].mod_off,
			dpll_regs[i].autoidle.addr);
		dpll_regs[i].clksel.val = cm_read_mod_reg(dpll_regs[i].mod_off,
			dpll_regs[i].clksel.addr);
		dpll_regs[i].div_m2.val = dpll_regs[i].div_m2.addr ?
			cm_read_mod_reg(dpll_regs[i].mod_off, dpll_regs[i].div_m2.addr) : 0;
		dpll_regs[i].div_m3.val = dpll_regs[i].div_m3.addr ?
			cm_read_mod_reg(dpll_regs[i].mod_off, dpll_regs[i].div_m3.addr) : 0;
		dpll_regs[i].div_m4.val = dpll_regs[i].div_m4.addr ?
			cm_read_mod_reg(dpll_regs[i].mod_off, dpll_regs[i].div_m4.addr) : 0;
		dpll_regs[i].div_m5.val = dpll_regs[i].div_m5.addr ?
			cm_read_mod_reg(dpll_regs[i].mod_off, dpll_regs[i].div_m5.addr) : 0;
		dpll_regs[i].div_m6.val = dpll_regs[i].div_m6.addr ?
			cm_read_mod_reg(dpll_regs[i].mod_off, dpll_regs[i].div_m6.addr) : 0;
		dpll_regs[i].div_m7.val = dpll_regs[i].div_m7.addr ?
			cm_read_mod_reg(dpll_regs[i].mod_off, dpll_regs[i].div_m7.addr) : 0;
		dpll_regs[i].clkdcoldo.val = dpll_regs[i].clkdcoldo.addr ?
			cm_read_mod_reg(dpll_regs[i].mod_off, dpll_regs[i].clkdcoldo.addr) : 0;
	}
	return;
}

void omap4_prcm_prepare_off(void)
{
	omap4_cm1_prepare_off();
	omap4_cm2_prepare_off();
	omap4_dpll_prepare_off();
}

static void omap4_cm1_resume_off(void)
{
	u32 i, j;

	for (i = 0; i < NO_CM1_MODULES; i++) {
		for (j = 0; j < cm1_regs[i].no_reg; j++) {
			cm_write_mod_reg(cm1_regs[i].reg[j].val,
				cm1_regs[i].mod_off, cm1_regs[i].reg[j].addr);
		}
	}
	return;
}

static void omap4_cm2_resume_off(void)
{
	u32 i, j;

	for (i = 0; i < NO_CM2_MODULES; i++) {
		for (j = 0; j < cm2_regs[i].no_reg; j++) {
			cm_write_mod_reg(cm2_regs[i].reg[j].val,
				cm2_regs[i].mod_off, cm2_regs[i].reg[j].addr);
		}
	}
	return;
}

static void omap4_dpll_resume_off(void)
{
	u32 i, j;

	for (i = 0; i < NO_DPLL_RESTORE; i++) {
		cm_write_mod_reg(dpll_regs[i].clksel.val, dpll_regs[i].mod_off,
			dpll_regs[i].clksel.addr);
		if (dpll_regs[i].div_m2.addr)
			cm_write_mod_reg(dpll_regs[i].div_m2.val,
			dpll_regs[i].mod_off, dpll_regs[i].div_m2.addr);
		if (dpll_regs[i].div_m3.addr)
			cm_write_mod_reg(dpll_regs[i].div_m3.val,
			dpll_regs[i].mod_off, dpll_regs[i].div_m3.addr);
		if (dpll_regs[i].div_m4.addr)
			cm_write_mod_reg(dpll_regs[i].div_m4.val,
			dpll_regs[i].mod_off, dpll_regs[i].div_m4.addr);
		if (dpll_regs[i].div_m5.addr)
			cm_write_mod_reg(dpll_regs[i].div_m5.val,
			dpll_regs[i].mod_off, dpll_regs[i].div_m5.addr);
		if (dpll_regs[i].div_m6.addr)
			cm_write_mod_reg(dpll_regs[i].div_m6.val,
			dpll_regs[i].mod_off, dpll_regs[i].div_m6.addr);
		if (dpll_regs[i].div_m7.addr)
			cm_write_mod_reg(dpll_regs[i].div_m7.val,
			dpll_regs[i].mod_off, dpll_regs[i].div_m7.addr);
		if (dpll_regs[i].clkdcoldo.addr)
			cm_write_mod_reg(dpll_regs[i].clkdcoldo.val,
			dpll_regs[i].mod_off, dpll_regs[i].clkdcoldo.addr);
		/* Restore clkmode only after all other registers are restored */
		cm_write_mod_reg(dpll_regs[i].clkmode.val, dpll_regs[i].mod_off,
			dpll_regs[i].clkmode.addr);

		j = 0;
		/* Should we wait for DPLL to get locked? */
		if ((dpll_regs[i].clkmode.val & OMAP4430_DPLL_EN_MASK) == OMAP4430_DPLL_EN_MASK) {
			while (((cm_read_mod_reg(dpll_regs[i].mod_off, dpll_regs[i].idlest.addr)
				 & OMAP4430_ST_DPLL_CLK_MASK) != 0x1) &&
					j < MAX_DPLL_WAIT_TRIES) {
						j++;
						udelay(1);
				}
		}

		/* restore autoidle settings after the dpll is locked */
		cm_write_mod_reg(dpll_regs[i].autoidle.val, dpll_regs[i].mod_off,
			dpll_regs[i].autoidle.addr);

	}
	return;
}

void omap4_prcm_resume_off(void)
{
	omap4_cm1_resume_off();
	omap4_cm2_resume_off();
	omap4_dpll_resume_off();
}
#endif
