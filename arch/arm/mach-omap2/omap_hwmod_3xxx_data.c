/*
 * omap_hwmod_3xxx_data.c - hardware modules present on the OMAP3xxx chips
 *
 * Copyright (C) 2009-2010 Nokia Corporation
 * Paul Walmsley
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * The data in this file should be completely autogeneratable from
 * the TI hardware database or other technical documentation.
 *
 * XXX these should be marked initdata for multi-OMAP kernels
 */
#include <plat/omap_hwmod.h>
#include <mach/irqs.h>
#include <plat/cpu.h>
#include <plat/dma.h>
#include <plat/l4_3xxx.h>
#include <plat/i2c.h>
#include <plat/omap34xx.h>
#include <plat/serial.h>
#include <plat/gpio.h>
#include <plat/mcspi.h>
#include <plat/mmc.h>
#include <plat/dmtimer.h>
#include <plat/control.h>
#include <plat/smartreflex.h>

#include "omap_hwmod_common_data.h"

#include "prm-regbits-34xx.h"
#include "cm-regbits-34xx.h"

/*
 * OMAP3xxx hardware module integration data
 *
 * ALl of the data in this section should be autogeneratable from the
 * TI hardware database or other technical documentation.  Data that
 * is driver-specific or driver-kernel integration-specific belongs
 * elsewhere.
 */

static struct omap_hwmod omap3xxx_dss_hwmod;
static struct omap_hwmod omap3xxx_dss_dispc_hwmod;
static struct omap_hwmod omap3xxx_dss_dsi1_hwmod;
static struct omap_hwmod omap3xxx_dss_rfbi_hwmod;
static struct omap_hwmod omap3xxx_dss_venc_hwmod;
static struct omap_hwmod omap3xxx_mpu_hwmod;
static struct omap_hwmod omap3xxx_ispmmu_hwmod;
static struct omap_hwmod omap3xxx_iva_hwmod;
static struct omap_hwmod omap3xxx_l3_main_hwmod;
static struct omap_hwmod omap3xxx_l4_core_hwmod;
static struct omap_hwmod omap3xxx_l4_per_hwmod;
static struct omap_hwmod omap3xxx_i2c1_hwmod;
static struct omap_hwmod omap3xxx_i2c2_hwmod;
static struct omap_hwmod omap3xxx_i2c3_hwmod;
static struct omap_hwmod omap3xxx_gpio1_hwmod;
static struct omap_hwmod omap3xxx_gpio2_hwmod;
static struct omap_hwmod omap3xxx_gpio3_hwmod;
static struct omap_hwmod omap3xxx_gpio4_hwmod;
static struct omap_hwmod omap3xxx_gpio5_hwmod;
static struct omap_hwmod omap3xxx_gpio6_hwmod;
static struct omap_hwmod omap3xxx_wd_timer2_hwmod;
static struct omap_hwmod omap3xxx_dma_system_hwmod;
static struct omap_hwmod omap34xx_mcspi1;
static struct omap_hwmod omap34xx_mcspi2;
static struct omap_hwmod omap34xx_mcspi3;
static struct omap_hwmod omap34xx_mcspi4;
static struct omap_hwmod omap34xx_sr1_hwmod;
static struct omap_hwmod omap34xx_sr2_hwmod;
static struct omap_hwmod omap3xxx_mcbsp1_hwmod;
static struct omap_hwmod omap3xxx_mcbsp2_hwmod;
static struct omap_hwmod omap3xxx_mcbsp3_hwmod;
static struct omap_hwmod omap3xxx_mcbsp4_hwmod;
static struct omap_hwmod omap3xxx_mcbsp5_hwmod;
static struct omap_hwmod omap3xxx_mcbsp2_sidetone_hwmod;
static struct omap_hwmod omap3xxx_mcbsp3_sidetone_hwmod;
static struct omap_hwmod omap3xxx_gpu_hwmod;

/* L3 -> L4_CORE interface */
static struct omap_hwmod_ocp_if omap3xxx_l3_main__l4_core = {
	.master	= &omap3xxx_l3_main_hwmod,
	.slave	= &omap3xxx_l4_core_hwmod,
	.user	= OCP_USER_MPU | OCP_USER_SDMA,
};

/* L3 -> L4_PER interface */
static struct omap_hwmod_ocp_if omap3xxx_l3_main__l4_per = {
	.master = &omap3xxx_l3_main_hwmod,
	.slave	= &omap3xxx_l4_per_hwmod,
	.user	= OCP_USER_MPU | OCP_USER_SDMA,
};

/* MPU -> L3 interface */
static struct omap_hwmod_ocp_if omap3xxx_mpu__l3_main = {
	.master = &omap3xxx_mpu_hwmod,
	.slave	= &omap3xxx_l3_main_hwmod,
	.user	= OCP_USER_MPU,
};

/* DSS -> l3 */
static struct omap_hwmod_ocp_if omap3xxx_dss__l3 = {
	.master		= &omap3xxx_dss_hwmod,
	.slave		= &omap3xxx_l3_main_hwmod,
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* GPU -> l3_main */
static struct omap_hwmod_ocp_if omap3xxx_gpu__l3_main = {
	.master         = &omap3xxx_gpu_hwmod,
	.slave          = &omap3xxx_l3_main_hwmod,
	.clk		= "core_l3_ick",
	.user           = OCP_USER_MPU | OCP_USER_SDMA,
};

/* Slave interfaces on the L3 interconnect */
static struct omap_hwmod_ocp_if *omap3xxx_l3_main_slaves[] = {
	&omap3xxx_mpu__l3_main,
	&omap3xxx_gpu__l3_main,
};

/* Master interfaces on the L3 interconnect */
static struct omap_hwmod_ocp_if *omap3xxx_l3_main_masters[] = {
	&omap3xxx_l3_main__l4_core,
	&omap3xxx_l3_main__l4_per,
};

/* L3 */
static struct omap_hwmod omap3xxx_l3_main_hwmod = {
	.name		= "l3_main",
	.class		= &l3_hwmod_class,
	.vdd_name	= "core",
	.masters	= omap3xxx_l3_main_masters,
	.masters_cnt	= ARRAY_SIZE(omap3xxx_l3_main_masters),
	.slaves		= omap3xxx_l3_main_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap3xxx_l3_main_slaves),
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430),
	.flags		= HWMOD_NO_IDLEST,
};

static struct omap_hwmod omap3xxx_l4_wkup_hwmod;
static struct omap_hwmod omap3xxx_uart1_hwmod;
static struct omap_hwmod omap3xxx_uart2_hwmod;
static struct omap_hwmod omap3xxx_uart3_hwmod;
static struct omap_hwmod omap3xxx_mmc1_hwmod;
static struct omap_hwmod omap3xxx_mmc2_hwmod;
static struct omap_hwmod omap3xxx_mmc3_hwmod;
static struct omap_hwmod omap3xxx_usbhsotg_hwmod;

/* L3 <- USBHSOTG interface */
static struct omap_hwmod_ocp_if omap3xxx_usbhsotg__l3 = {
	.master		= &omap3xxx_usbhsotg_hwmod,
	.slave		= &omap3xxx_l3_main_hwmod,
	.clk		= "core_l3_ick",
	.user		= OCP_USER_MPU,
};

/* L4_CORE -> L4_WKUP interface */
static struct omap_hwmod_ocp_if omap3xxx_l4_core__l4_wkup = {
	.master	= &omap3xxx_l4_core_hwmod,
	.slave	= &omap3xxx_l4_wkup_hwmod,
	.user	= OCP_USER_MPU | OCP_USER_SDMA,
};

/* L4 CORE -> UART1 interface */
static struct omap_hwmod_addr_space omap3xxx_uart1_addr_space[] = {
	{
		.pa_start	= OMAP3_UART1_BASE,
		.pa_end		= OMAP3_UART1_BASE + SZ_8K - 1,
		.flags		= ADDR_MAP_ON_INIT | ADDR_TYPE_RT,
	},
};

static struct omap_hwmod_ocp_if omap3_l4_core__uart1 = {
	.master		= &omap3xxx_l4_core_hwmod,
	.slave		= &omap3xxx_uart1_hwmod,
	.clk		= "uart1_ick",
	.addr		= omap3xxx_uart1_addr_space,
	.addr_cnt	= ARRAY_SIZE(omap3xxx_uart1_addr_space),
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* L4 CORE -> UART2 interface */
static struct omap_hwmod_addr_space omap3xxx_uart2_addr_space[] = {
	{
		.pa_start	= OMAP3_UART2_BASE,
		.pa_end		= OMAP3_UART2_BASE + SZ_1K - 1,
		.flags		= ADDR_MAP_ON_INIT | ADDR_TYPE_RT,
	},
};

static struct omap_hwmod_ocp_if omap3_l4_core__uart2 = {
	.master		= &omap3xxx_l4_core_hwmod,
	.slave		= &omap3xxx_uart2_hwmod,
	.clk		= "uart2_ick",
	.addr		= omap3xxx_uart2_addr_space,
	.addr_cnt	= ARRAY_SIZE(omap3xxx_uart2_addr_space),
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* L4 PER -> UART3 interface */
static struct omap_hwmod_addr_space omap3xxx_uart3_addr_space[] = {
	{
		.pa_start	= OMAP3_UART3_BASE,
		.pa_end		= OMAP3_UART3_BASE + SZ_1K - 1,
		.flags		= ADDR_MAP_ON_INIT | ADDR_TYPE_RT,
	},
};

static struct omap_hwmod_ocp_if omap3_l4_per__uart3 = {
	.master		= &omap3xxx_l4_per_hwmod,
	.slave		= &omap3xxx_uart3_hwmod,
	.clk		= "uart3_ick",
	.addr		= omap3xxx_uart3_addr_space,
	.addr_cnt	= ARRAY_SIZE(omap3xxx_uart3_addr_space),
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* I2C IP block address space length (in bytes) */
#define OMAP2_I2C_AS_LEN		128

/* L4 CORE -> I2C1 interface */
static struct omap_hwmod_addr_space omap3xxx_i2c1_addr_space[] = {
	{
		.pa_start	= OMAP34XX_I2C1_BASE,
		.pa_end		= OMAP34XX_I2C1_BASE + OMAP2_I2C_AS_LEN - 1,
		.flags		= ADDR_TYPE_RT,
	},
};

static struct omap_hwmod_ocp_if omap3_l4_core__i2c1 = {
	.master		= &omap3xxx_l4_core_hwmod,
	.slave		= &omap3xxx_i2c1_hwmod,
	.clk		= "i2c1_ick",
	.addr		= omap3xxx_i2c1_addr_space,
	.addr_cnt	= ARRAY_SIZE(omap3xxx_i2c1_addr_space),
	.fw = {
		.omap2 = {
			.l4_fw_region  = OMAP3_L4_CORE_FW_I2C1_REGION,
			.l4_prot_group = 7,
			.flags	= OMAP_FIREWALL_L4,
		}
	},
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* L4 CORE -> I2C2 interface */
static struct omap_hwmod_addr_space omap3xxx_i2c2_addr_space[] = {
	{
		.pa_start	= OMAP34XX_I2C2_BASE,
		.pa_end		= OMAP34XX_I2C2_BASE + OMAP2_I2C_AS_LEN - 1,
		.flags		= ADDR_TYPE_RT,
	},
};

static struct omap_hwmod_ocp_if omap3_l4_core__i2c2 = {
	.master		= &omap3xxx_l4_core_hwmod,
	.slave		= &omap3xxx_i2c2_hwmod,
	.clk		= "i2c2_ick",
	.addr		= omap3xxx_i2c2_addr_space,
	.addr_cnt	= ARRAY_SIZE(omap3xxx_i2c2_addr_space),
	.fw = {
		.omap2 = {
			.l4_fw_region  = OMAP3_L4_CORE_FW_I2C2_REGION,
			.l4_prot_group = 7,
			.flags = OMAP_FIREWALL_L4,
		}
	},
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* L4 CORE -> I2C3 interface */
static struct omap_hwmod_addr_space omap3xxx_i2c3_addr_space[] = {
	{
		.pa_start	= OMAP34XX_I2C3_BASE,
		.pa_end		= OMAP34XX_I2C3_BASE + OMAP2_I2C_AS_LEN - 1,
		.flags		= ADDR_TYPE_RT,
	},
};

static struct omap_hwmod_ocp_if omap3_l4_core__i2c3 = {
	.master		= &omap3xxx_l4_core_hwmod,
	.slave		= &omap3xxx_i2c3_hwmod,
	.clk		= "i2c3_ick",
	.addr		= omap3xxx_i2c3_addr_space,
	.addr_cnt	= ARRAY_SIZE(omap3xxx_i2c3_addr_space),
	.fw = {
		.omap2 = {
			.l4_fw_region  = OMAP3_L4_CORE_FW_I2C3_REGION,
			.l4_prot_group = 7,
			.flags = OMAP_FIREWALL_L4,
		}
	},
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/*
* USBHSOTG interface data
*/

static struct omap_hwmod_addr_space omap3xxx_usbhsotg_addrs[] = {
	{
		.pa_start	= OMAP34XX_HSUSB_OTG_BASE,
		.pa_end		= OMAP34XX_HSUSB_OTG_BASE + SZ_4K - 1,
		.flags		= ADDR_TYPE_RT
	},
};

/* USBHSOTG <- L4_CORE interface */
static struct omap_hwmod_ocp_if omap3xxx_l4_core__usbhsotg = {
	.master		= &omap3xxx_l4_core_hwmod,
	.slave		= &omap3xxx_usbhsotg_hwmod,
	.clk		= "l4_ick",
	.addr		= omap3xxx_usbhsotg_addrs,
	.addr_cnt	= ARRAY_SIZE(omap3xxx_usbhsotg_addrs),
	.user		= OCP_USER_MPU,

};

static struct omap_hwmod_ocp_if *omap3xxx_usbhsotg_masters[] = {
	&omap3xxx_usbhsotg__l3,
};

static struct omap_hwmod_ocp_if *omap3xxx_usbhsotg_slaves[] = {
	&omap3xxx_l4_core__usbhsotg,
};


/* L4 CORE -> MCSPI1 interface */
static struct omap_hwmod_addr_space omap34xx_mcspi1_addr_space[] = {
	{
		.pa_start	= 0x48098000,
		.pa_end		= 0x480980ff,
		.flags		= ADDR_TYPE_RT,
	},
};

static struct omap_hwmod_ocp_if omap34xx_l4_core__mcspi1 = {
	.master		= &omap3xxx_l4_core_hwmod,
	.slave		= &omap34xx_mcspi1,
	.clk		= "mcspi1_ick",
	.addr		= omap34xx_mcspi1_addr_space,
	.addr_cnt	= ARRAY_SIZE(omap34xx_mcspi1_addr_space),
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* L4 CORE -> MCSPI2 interface */
static struct omap_hwmod_addr_space omap34xx_mcspi2_addr_space[] = {
	{
		.pa_start	= 0x4809a000,
		.pa_end		= 0x4809a0ff,
		.flags		= ADDR_TYPE_RT,
	},
};

static struct omap_hwmod_ocp_if omap34xx_l4_core__mcspi2 = {
	.master		= &omap3xxx_l4_core_hwmod,
	.slave		= &omap34xx_mcspi2,
	.clk		= "mcspi2_ick",
	.addr		= omap34xx_mcspi2_addr_space,
	.addr_cnt	= ARRAY_SIZE(omap34xx_mcspi2_addr_space),
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* L4 CORE -> MCSPI3 interface */
static struct omap_hwmod_addr_space omap34xx_mcspi3_addr_space[] = {
	{
		.pa_start	= 0x480b8000,
		.pa_end		= 0x480b80ff,
		.flags		= ADDR_TYPE_RT,
	},
};

static struct omap_hwmod_ocp_if omap34xx_l4_core__mcspi3 = {
	.master		= &omap3xxx_l4_core_hwmod,
	.slave		= &omap34xx_mcspi3,
	.clk		= "mcspi3_ick",
	.addr		= omap34xx_mcspi3_addr_space,
	.addr_cnt	= ARRAY_SIZE(omap34xx_mcspi3_addr_space),
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* L4 CORE -> MCSPI4 interface */
static struct omap_hwmod_addr_space omap34xx_mcspi4_addr_space[] = {
	{
		.pa_start	= 0x480ba000,
		.pa_end		= 0x480ba0ff,
		.flags		= ADDR_TYPE_RT,
	},
};

static struct omap_hwmod_ocp_if omap34xx_l4_core__mcspi4 = {
	.master		= &omap3xxx_l4_core_hwmod,
	.slave		= &omap34xx_mcspi4,
	.clk		= "mcspi4_ick",
	.addr		= omap34xx_mcspi4_addr_space,
	.addr_cnt	= ARRAY_SIZE(omap34xx_mcspi4_addr_space),
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* L4 CORE -> MMC1 interface */
static struct omap_hwmod_addr_space omap3xxx_mmc1_addr_space[] = {
	{
		.pa_start	= OMAP2_MMC1_BASE,
		.pa_end		= OMAP2_MMC1_BASE + 512 - 1,
		.flags		= ADDR_TYPE_RT,
	},
};

static struct omap_hwmod_ocp_if omap3xxx_l4_core__mmc1 = {
	.master		= &omap3xxx_l4_core_hwmod,
	.slave		= &omap3xxx_mmc1_hwmod,
	.clk		= "mmchs1_ick",
	.addr		= omap3xxx_mmc1_addr_space,
	.addr_cnt	= ARRAY_SIZE(omap3xxx_mmc1_addr_space),
	.fw = {
		.omap2 = {
			.l4_fw_region = OMAP3_L4_CORE_FW_MMC1_REGION,
			.l4_prot_group = 7,
		}
	},
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
	.flags		= OMAP_FIREWALL_L4
};

/* L4 CORE -> MMC2 interface */
static struct omap_hwmod_addr_space omap3xxx_mmc2_addr_space[] = {
	{
		.pa_start	= OMAP2_MMC2_BASE,
		.pa_end		= OMAP2_MMC2_BASE + 512 - 1,
		.flags		= ADDR_TYPE_RT,
	},
};

static struct omap_hwmod_ocp_if omap3xxx_l4_core__mmc2 = {
	.master		= &omap3xxx_l4_core_hwmod,
	.slave		= &omap3xxx_mmc2_hwmod,
	.clk		= "mmchs2_ick",
	.addr		= omap3xxx_mmc2_addr_space,
	.addr_cnt	= ARRAY_SIZE(omap3xxx_mmc2_addr_space),
	.fw = {
		.omap2 = {
			.l4_fw_region = OMAP3_L4_CORE_FW_MMC2_REGION,
			.l4_prot_group = 7,
		}
	},
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
	.flags		= OMAP_FIREWALL_L4
};

/* L4 CORE -> MMC3 interface */
static struct omap_hwmod_addr_space omap3xxx_mmc3_addr_space[] = {
	{
		.pa_start	= OMAP3_MMC3_BASE,
		.pa_end		= OMAP3_MMC3_BASE + 512 - 1,
		.flags		= ADDR_TYPE_RT,
	},
};

static struct omap_hwmod_ocp_if omap3xxx_l4_core__mmc3 = {
	.master		= &omap3xxx_l4_core_hwmod,
	.slave		= &omap3xxx_mmc3_hwmod,
	.clk		= "mmchs3_ick",
	.addr		= omap3xxx_mmc3_addr_space,
	.addr_cnt	= ARRAY_SIZE(omap3xxx_mmc3_addr_space),
	.fw = {
		.omap2 = {
			.l4_fw_region  = OMAP3_L4_CORE_FW_MMC3_REGION,
			.l4_prot_group = 7,
		}
	},
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
	.flags		= OMAP_FIREWALL_L4
};

/* L4 CORE -> SR1 interface */
static struct omap_hwmod_addr_space omap3_sr1_addr_space[] = {
	{
		.pa_start	= OMAP34XX_SR1_BASE,
		.pa_end		= OMAP34XX_SR1_BASE + SZ_1K - 1,
		.flags		= ADDR_TYPE_RT,
	},
};

static struct omap_hwmod_ocp_if omap3_l4_core__sr1 = {
	.master		= &omap3xxx_l4_core_hwmod,
	.slave		= &omap34xx_sr1_hwmod,
	.clk		= "sr_l4_ick",
	.addr		= omap3_sr1_addr_space,
	.addr_cnt	= ARRAY_SIZE(omap3_sr1_addr_space),
	.user		= OCP_USER_MPU,
};

/* L4 CORE -> SR1 interface */
static struct omap_hwmod_addr_space omap3_sr2_addr_space[] = {
	{
		.pa_start	= OMAP34XX_SR2_BASE,
		.pa_end		= OMAP34XX_SR2_BASE + SZ_1K - 1,
		.flags		= ADDR_TYPE_RT,
	},
};

static struct omap_hwmod_ocp_if omap3_l4_core__sr2 = {
	.master		= &omap3xxx_l4_core_hwmod,
	.slave		= &omap34xx_sr2_hwmod,
	.clk		= "sr_l4_ick",
	.addr		= omap3_sr2_addr_space,
	.addr_cnt	= ARRAY_SIZE(omap3_sr2_addr_space),
	.user		= OCP_USER_MPU,
};

/* Slave interfaces on the L4_CORE interconnect */
static struct omap_hwmod_ocp_if *omap3xxx_l4_core_slaves[] = {
	&omap3xxx_l3_main__l4_core,
	&omap3_l4_core__sr1,
	&omap3_l4_core__sr2,
};

/* Master interfaces on the L4_CORE interconnect */
static struct omap_hwmod_ocp_if *omap3xxx_l4_core_masters[] = {
	&omap3xxx_l4_core__l4_wkup,
	&omap3_l4_core__i2c1,
	&omap3_l4_core__i2c2,
	&omap3_l4_core__i2c3,
	&omap3_l4_core__uart1,
	&omap3_l4_core__uart2,
};

/* WDTIMER2 <- L4_WKUP interface */
static struct omap_hwmod_addr_space omap3xxx_wd_timer2_addrs[] = {
	{
		.pa_start	= 0x48314000,
		.pa_end		= 0x48314000 + SZ_4K - 1,
		.flags		= ADDR_TYPE_RT
	},
};

static struct omap_hwmod_ocp_if omap3xxx_l4_wkup__wd_timer2 = {
	.master		= &omap3xxx_l4_wkup_hwmod,
	.slave		= &omap3xxx_wd_timer2_hwmod,
	.clk		= "wdt2_ick",
	.addr		= omap3xxx_wd_timer2_addrs,
	.addr_cnt	= ARRAY_SIZE(omap3xxx_wd_timer2_addrs),
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};
/* timer class */

static struct omap_hwmod_class_sysconfig omap3xxx_timer_1ms_sysc = {
	.rev_offs	= 0x0000,
	.sysc_offs	= 0x0010,
	.syss_offs	= 0x0014,
	.sysc_flags	= (SYSC_HAS_SIDLEMODE | SYSC_HAS_CLOCKACTIVITY |
				SYSC_HAS_ENAWAKEUP | SYSC_HAS_SOFTRESET |
				SYSC_HAS_EMUFREE | SYSC_HAS_AUTOIDLE |
				SYSS_HAS_RESET_STATUS),
	.idlemodes	= (SIDLE_FORCE | SIDLE_NO | SIDLE_SMART),
	.sysc_fields	= &omap_hwmod_sysc_type1,
};

static struct omap_hwmod_class omap3xxx_timer_1ms_hwmod_class = {
	.name = "timer",
	.sysc = &omap3xxx_timer_1ms_sysc,
	.rev = OMAP_TIMER_IP_VERSION_1,
};


static struct omap_hwmod_class_sysconfig omap3xxx_timer_sysc = {
	.rev_offs	= 0x0000,
	.sysc_offs	= 0x0010,
	.syss_offs	= 0x0014,
	.sysc_flags	= (SYSC_HAS_SIDLEMODE | SYSC_HAS_ENAWAKEUP |
			   SYSC_HAS_SOFTRESET | SYSC_HAS_AUTOIDLE |
			   SYSS_HAS_RESET_STATUS),
	.idlemodes	= (SIDLE_FORCE | SIDLE_NO | SIDLE_SMART),
	.sysc_fields	= &omap_hwmod_sysc_type2,
};

static struct omap_hwmod_class omap3xxx_timer_hwmod_class = {
	.name = "timer",
	.sysc = &omap3xxx_timer_sysc,
	.rev =  OMAP_TIMER_IP_VERSION_1,
};

/* timer10 */
static struct omap_hwmod omap3xxx_timer10_hwmod;
static struct omap_hwmod_irq_info omap3xxx_timer10_mpu_irqs[] = {
	{ .irq = INT_24XX_GPTIMER10, },
};

static struct omap_hwmod_addr_space omap3xxx_timer10_addrs[] = {
	{
		.pa_start	= 0x48086000,
		.pa_end		= 0x48086000 + SZ_1K - 1,
		.flags		= ADDR_TYPE_RT
	},
};

/* l4_core -> timer10 */
static struct omap_hwmod_ocp_if omap3xxx_l4_core__timer10 = {
	.master		= &omap3xxx_l4_core_hwmod,
	.slave		= &omap3xxx_timer10_hwmod,
	.clk		= "gpt10_ick",
	.addr		= omap3xxx_timer10_addrs,
	.addr_cnt	= ARRAY_SIZE(omap3xxx_timer10_addrs),
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* timer10 master port */
static struct omap_hwmod_ocp_if *omap3xxx_timer10_masters[] = {
	&omap3xxx_l4_core__timer10,
};

/* timer10 slave port */
static struct omap_hwmod_ocp_if *omap3xxx_timer10_slaves[] = {
	&omap3xxx_l4_core__timer10,
};

/* timer10 hwmod */
static struct omap_hwmod omap3xxx_timer10_hwmod = {
	.name		= "timer10",
	.mpu_irqs	= omap3xxx_timer10_mpu_irqs,
	.mpu_irqs_cnt	= ARRAY_SIZE(omap3xxx_timer10_mpu_irqs),
	.main_clk	= "gpt10_fck",
	.prcm		= {
		.omap2 = {
			.prcm_reg_id = 1,
			.module_bit = OMAP24XX_EN_GPT10_SHIFT,
			.module_offs = CORE_MOD,
			.idlest_reg_id = 1,
			.idlest_idle_bit = OMAP24XX_EN_GPT10_SHIFT,
		},
	},
	.masters	= omap3xxx_timer10_masters,
	.masters_cnt	= ARRAY_SIZE(omap3xxx_timer10_masters),
	.slaves		= omap3xxx_timer10_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap3xxx_timer10_slaves),
	.class		= &omap3xxx_timer_1ms_hwmod_class,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430)
};

/* timer11 */
static struct omap_hwmod omap3xxx_timer11_hwmod;
static struct omap_hwmod_irq_info omap3xxx_timer11_mpu_irqs[] = {
	{ .irq = INT_24XX_GPTIMER11, },
};

static struct omap_hwmod_addr_space omap3xxx_timer11_addrs[] = {
	{
		.pa_start	= 0x48088000,
		.pa_end		= 0x48088000 + SZ_1K - 1,
		.flags		= ADDR_TYPE_RT
	},
};

/* l4_core -> timer11 */
static struct omap_hwmod_ocp_if omap3xxx_l4_core__timer11 = {
	.master		= &omap3xxx_l4_core_hwmod,
	.slave		= &omap3xxx_timer11_hwmod,
	.clk		= "gpt11_ick",
	.addr		= omap3xxx_timer11_addrs,
	.addr_cnt	= ARRAY_SIZE(omap3xxx_timer11_addrs),
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* timer11 master port */
static struct omap_hwmod_ocp_if *omap3xxx_timer11_masters[] = {
	&omap3xxx_l4_core__timer11,
};

/* timer11 slave port */
static struct omap_hwmod_ocp_if *omap3xxx_timer11_slaves[] = {
	&omap3xxx_l4_core__timer11,
};

/* timer11 hwmod */
static struct omap_hwmod omap3xxx_timer11_hwmod = {
	.name		= "timer11",
	.mpu_irqs	= omap3xxx_timer11_mpu_irqs,
	.mpu_irqs_cnt	= ARRAY_SIZE(omap3xxx_timer11_mpu_irqs),
	.main_clk	= "gpt11_fck",
	.prcm		= {
		.omap2 = {
			.prcm_reg_id = 1,
			.module_bit = OMAP24XX_EN_GPT11_SHIFT,
			.module_offs = CORE_MOD,
			.idlest_reg_id = 1,
			.idlest_idle_bit = OMAP24XX_EN_GPT11_SHIFT,
		},
	},
	.masters	= omap3xxx_timer11_masters,
	.masters_cnt	= ARRAY_SIZE(omap3xxx_timer11_masters),
	.slaves		= omap3xxx_timer11_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap3xxx_timer11_slaves),
	.class		= &omap3xxx_timer_hwmod_class,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430)
};

/* L4 CORE */
static struct omap_hwmod omap3xxx_l4_core_hwmod = {
	.name		= "l4_core",
	.class		= &l4_hwmod_class,
	.masters	= omap3xxx_l4_core_masters,
	.masters_cnt	= ARRAY_SIZE(omap3xxx_l4_core_masters),
	.slaves		= omap3xxx_l4_core_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap3xxx_l4_core_slaves),
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430),
	.flags		= HWMOD_NO_IDLEST,
};

/* timer2 */
static struct omap_hwmod omap3xxx_timer2_hwmod;
static struct omap_hwmod_irq_info omap3xxx_timer2_mpu_irqs[] = {
	{ .irq = INT_24XX_GPTIMER2, },
};

static struct omap_hwmod_addr_space omap3xxx_timer2_addrs[] = {
	{
		.pa_start	= 0x49032000,
		.pa_end		= 0x49032000 + SZ_1K - 1,
		.flags		= ADDR_TYPE_RT
	},
};

/* l4_per -> timer2 */
static struct omap_hwmod_ocp_if omap3xxx_l4_per__timer2 = {
	.master		= &omap3xxx_l4_per_hwmod,
	.slave		= &omap3xxx_timer2_hwmod,
	.clk		= "gpt2_ick",
	.addr		= omap3xxx_timer2_addrs,
	.addr_cnt	= ARRAY_SIZE(omap3xxx_timer2_addrs),
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* timer2 master port */
static struct omap_hwmod_ocp_if *omap3xxx_timer2_masters[] = {
	&omap3xxx_l4_per__timer2,
};

/* timer2 slave port */
static struct omap_hwmod_ocp_if *omap3xxx_timer2_slaves[] = {
	&omap3xxx_l4_per__timer2,
};

/* timer2 hwmod */
static struct omap_hwmod omap3xxx_timer2_hwmod = {
	.name		= "timer2",
	.mpu_irqs	= omap3xxx_timer2_mpu_irqs,
	.mpu_irqs_cnt	= ARRAY_SIZE(omap3xxx_timer2_mpu_irqs),
	.main_clk	= "gpt2_fck",
	.prcm		= {
		.omap2 = {
			.prcm_reg_id = 1,
			.module_bit = OMAP3430_EN_GPT2_SHIFT,
			.module_offs = OMAP3430_PER_MOD,
			.idlest_reg_id = 1,
			.idlest_idle_bit = OMAP3430_EN_GPT2_SHIFT,
		},
	},
	.masters	= omap3xxx_timer2_masters,
	.masters_cnt	= ARRAY_SIZE(omap3xxx_timer2_masters),
	.slaves		= omap3xxx_timer2_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap3xxx_timer2_slaves),
	.class		= &omap3xxx_timer_1ms_hwmod_class,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430)
};


/* timer3 */
static struct omap_hwmod omap3xxx_timer3_hwmod;
static struct omap_hwmod_irq_info omap3xxx_timer3_mpu_irqs[] = {
	{ .irq = INT_24XX_GPTIMER3, },
};

static struct omap_hwmod_addr_space omap3xxx_timer3_addrs[] = {
	{
		.pa_start	= 0x49034000,
		.pa_end		= 0x49034000 + SZ_1K - 1,
		.flags		= ADDR_TYPE_RT
	},
};

/* l4_per -> timer3 */
static struct omap_hwmod_ocp_if omap3xxx_l4_per__timer3 = {
	.master		= &omap3xxx_l4_per_hwmod,
	.slave		= &omap3xxx_timer3_hwmod,
	.clk		= "gpt3_ick",
	.addr		= omap3xxx_timer3_addrs,
	.addr_cnt	= ARRAY_SIZE(omap3xxx_timer3_addrs),
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};
/* timer3 master port */
static struct omap_hwmod_ocp_if *omap3xxx_timer3_masters[] = {
	&omap3xxx_l4_per__timer3,
};

/* timer3 slave port */
static struct omap_hwmod_ocp_if *omap3xxx_timer3_slaves[] = {
	&omap3xxx_l4_per__timer3,
};

/* timer3 hwmod */
static struct omap_hwmod omap3xxx_timer3_hwmod = {
	.name		= "timer3",
	.mpu_irqs	= omap3xxx_timer3_mpu_irqs,
	.mpu_irqs_cnt	= ARRAY_SIZE(omap3xxx_timer3_mpu_irqs),
	.main_clk	= "gpt3_fck",
	.prcm		= {
		.omap2 = {
			.prcm_reg_id = 1,
			.module_bit = OMAP3430_EN_GPT3_SHIFT,
			.module_offs = OMAP3430_PER_MOD,
			.idlest_reg_id = 1,
			.idlest_idle_bit = OMAP3430_EN_GPT3_SHIFT,
		},
	},
	.masters	= omap3xxx_timer3_masters,
	.masters_cnt	= ARRAY_SIZE(omap3xxx_timer3_masters),
	.slaves		= omap3xxx_timer3_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap3xxx_timer3_slaves),
	.class		= &omap3xxx_timer_hwmod_class,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430)
};

/* timer4 */
static struct omap_hwmod omap3xxx_timer4_hwmod;
static struct omap_hwmod_irq_info omap3xxx_timer4_mpu_irqs[] = {
	{ .irq = INT_24XX_GPTIMER4, },
};

static struct omap_hwmod_addr_space omap3xxx_timer4_addrs[] = {
	{
		.pa_start	= 0x49036000,
		.pa_end		= 0x49036000 + SZ_1K - 1,
		.flags		= ADDR_TYPE_RT
	},
};

/* l4_per -> timer4 */
static struct omap_hwmod_ocp_if omap3xxx_l4_per__timer4 = {
	.master		= &omap3xxx_l4_per_hwmod,
	.slave		= &omap3xxx_timer4_hwmod,
	.clk		= "gpt4_ick",
	.addr		= omap3xxx_timer4_addrs,
	.addr_cnt	= ARRAY_SIZE(omap3xxx_timer4_addrs),
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* timer4 master port */
static struct omap_hwmod_ocp_if *omap3xxx_timer4_masters[] = {
	&omap3xxx_l4_per__timer4,
};

/* timer4 slave port */
static struct omap_hwmod_ocp_if *omap3xxx_timer4_slaves[] = {
	&omap3xxx_l4_per__timer4,
};

/* timer4 hwmod */
static struct omap_hwmod omap3xxx_timer4_hwmod = {
	.name		= "timer4",
	.mpu_irqs	= omap3xxx_timer4_mpu_irqs,
	.mpu_irqs_cnt	= ARRAY_SIZE(omap3xxx_timer4_mpu_irqs),
	.main_clk	= "gpt4_fck",
	.prcm		= {
		.omap2 = {
			.prcm_reg_id = 1,
			.module_bit = OMAP3430_EN_GPT4_SHIFT,
			.module_offs = OMAP3430_PER_MOD,
			.idlest_reg_id = 1,
			.idlest_idle_bit = OMAP3430_EN_GPT4_SHIFT,
		},
	},
	.masters	= omap3xxx_timer4_masters,
	.masters_cnt	= ARRAY_SIZE(omap3xxx_timer4_masters),
	.slaves		= omap3xxx_timer4_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap3xxx_timer4_slaves),
	.class		= &omap3xxx_timer_hwmod_class,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430)
};

/* timer5 */
static struct omap_hwmod omap3xxx_timer5_hwmod;
static struct omap_hwmod_irq_info omap3xxx_timer5_mpu_irqs[] = {
	{ .irq = INT_24XX_GPTIMER5, },
};

static struct omap_hwmod_addr_space omap3xxx_timer5_addrs[] = {
	{
		.pa_start	= 0x49038000,
		.pa_end		= 0x49038000 + SZ_1K - 1,
		.flags		= ADDR_TYPE_RT
	},
};

/* l4_per -> timer5 */
static struct omap_hwmod_ocp_if omap3xxx_l4_per__timer5 = {
	.master		= &omap3xxx_l4_per_hwmod,
	.slave		= &omap3xxx_timer5_hwmod,
	.clk		= "gpt5_ick",
	.addr		= omap3xxx_timer5_addrs,
	.addr_cnt	= ARRAY_SIZE(omap3xxx_timer5_addrs),
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* timer5 master port */
static struct omap_hwmod_ocp_if *omap3xxx_timer5_masters[] = {
	&omap3xxx_l4_per__timer5,
};

/* timer5 slave port */
static struct omap_hwmod_ocp_if *omap3xxx_timer5_slaves[] = {
	&omap3xxx_l4_per__timer5,
};

/* timer5 hwmod */
static struct omap_hwmod omap3xxx_timer5_hwmod = {
	.name		= "timer5",
	.mpu_irqs	= omap3xxx_timer5_mpu_irqs,
	.mpu_irqs_cnt	= ARRAY_SIZE(omap3xxx_timer5_mpu_irqs),
	.main_clk	= "gpt5_fck",
	.prcm		= {
		.omap2 = {
			.prcm_reg_id = 1,
			.module_bit = OMAP3430_EN_GPT5_SHIFT,
			.module_offs = OMAP3430_PER_MOD,
			.idlest_reg_id = 1,
			.idlest_idle_bit = OMAP3430_EN_GPT5_SHIFT,
		},
	},
	.masters	= omap3xxx_timer5_masters,
	.masters_cnt	= ARRAY_SIZE(omap3xxx_timer5_masters),
	.slaves		= omap3xxx_timer5_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap3xxx_timer5_slaves),
	.class		= &omap3xxx_timer_hwmod_class,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430)
};

/* timer6 */
static struct omap_hwmod omap3xxx_timer6_hwmod;
static struct omap_hwmod_irq_info omap3xxx_timer6_mpu_irqs[] = {
	{ .irq = INT_24XX_GPTIMER6, },
};

static struct omap_hwmod_addr_space omap3xxx_timer6_addrs[] = {
	{
		.pa_start	= 0x4903A000,
		.pa_end		= 0x4903A000 + SZ_1K - 1,
		.flags		= ADDR_TYPE_RT
	},
};

/* l4_per -> timer6 */
static struct omap_hwmod_ocp_if omap3xxx_l4_per__timer6 = {
	.master		= &omap3xxx_l4_per_hwmod,
	.slave		= &omap3xxx_timer6_hwmod,
	.clk		= "gpt6_ick",
	.addr		= omap3xxx_timer6_addrs,
	.addr_cnt	= ARRAY_SIZE(omap3xxx_timer6_addrs),
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* timer6 master port */
static struct omap_hwmod_ocp_if *omap3xxx_timer6_masters[] = {
	&omap3xxx_l4_per__timer6,
};
/* timer6 slave port */
static struct omap_hwmod_ocp_if *omap3xxx_timer6_slaves[] = {
	&omap3xxx_l4_per__timer6,
};

/* timer6 hwmod */
static struct omap_hwmod omap3xxx_timer6_hwmod = {
	.name		= "timer6",
	.mpu_irqs	= omap3xxx_timer6_mpu_irqs,
	.mpu_irqs_cnt	= ARRAY_SIZE(omap3xxx_timer6_mpu_irqs),
	.main_clk	= "gpt6_fck",
	.prcm		= {
		.omap2 = {
			.prcm_reg_id = 1,
			.module_bit = OMAP3430_EN_GPT6_SHIFT,
			.module_offs = OMAP3430_PER_MOD,
			.idlest_reg_id = 1,
			.idlest_idle_bit = OMAP3430_EN_GPT6_SHIFT,
		},
	},
	.masters	= omap3xxx_timer6_masters,
	.masters_cnt	= ARRAY_SIZE(omap3xxx_timer6_masters),
	.slaves		= omap3xxx_timer6_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap3xxx_timer6_slaves),
	.class		= &omap3xxx_timer_hwmod_class,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430)
};

/* timer7 */
static struct omap_hwmod omap3xxx_timer7_hwmod;
static struct omap_hwmod_irq_info omap3xxx_timer7_mpu_irqs[] = {
	{ .irq = INT_24XX_GPTIMER7, },
};

static struct omap_hwmod_addr_space omap3xxx_timer7_addrs[] = {
	{
		.pa_start	= 0x4903C000,
		.pa_end		= 0x4903C000 + SZ_1K - 1,
		.flags		= ADDR_TYPE_RT
	},
};

/* l4_per -> timer7 */
static struct omap_hwmod_ocp_if omap3xxx_l4_per__timer7 = {
	.master		= &omap3xxx_l4_per_hwmod,
	.slave		= &omap3xxx_timer7_hwmod,
	.clk		= "gpt7_ick",
	.addr		= omap3xxx_timer7_addrs,
	.addr_cnt	= ARRAY_SIZE(omap3xxx_timer7_addrs),
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* timer7 master port */
static struct omap_hwmod_ocp_if *omap3xxx_timer7_masters[] = {
	&omap3xxx_l4_per__timer7,
};

/* timer7 slave port */
static struct omap_hwmod_ocp_if *omap3xxx_timer7_slaves[] = {
	&omap3xxx_l4_per__timer7,
};

/* timer7 hwmod */
static struct omap_hwmod omap3xxx_timer7_hwmod = {
	.name		= "timer7",
	.mpu_irqs	= omap3xxx_timer7_mpu_irqs,
	.mpu_irqs_cnt	= ARRAY_SIZE(omap3xxx_timer7_mpu_irqs),
	.main_clk	= "gpt7_fck",
	.prcm		= {
		.omap2 = {
			.prcm_reg_id = 1,
			.module_bit = OMAP3430_EN_GPT7_SHIFT,
			.module_offs = OMAP3430_PER_MOD,
			.idlest_reg_id = 1,
			.idlest_idle_bit = OMAP3430_EN_GPT7_SHIFT,
		},
	},
	.masters	= omap3xxx_timer7_masters,
	.masters_cnt	= ARRAY_SIZE(omap3xxx_timer7_masters),
	.slaves		= omap3xxx_timer7_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap3xxx_timer7_slaves),
	.class		= &omap3xxx_timer_hwmod_class,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430)
};

/* timer8 */
static struct omap_hwmod omap3xxx_timer8_hwmod;
static struct omap_hwmod_irq_info omap3xxx_timer8_mpu_irqs[] = {
	{ .irq = INT_24XX_GPTIMER8, },
};

static struct omap_hwmod_addr_space omap3xxx_timer8_addrs[] = {
	{
		.pa_start	= 0x4903E000,
		.pa_end		= 0x4903E000 + SZ_1K - 1,
		.flags		= ADDR_TYPE_RT
	},
};

/* l4_per -> timer8 */
static struct omap_hwmod_ocp_if omap3xxx_l4_per__timer8 = {
	.master		= &omap3xxx_l4_per_hwmod,
	.slave		= &omap3xxx_timer8_hwmod,
	.clk		= "gpt8_ick",
	.addr		= omap3xxx_timer8_addrs,
	.addr_cnt	= ARRAY_SIZE(omap3xxx_timer8_addrs),
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* timer8 master port */
static struct omap_hwmod_ocp_if *omap3xxx_timer8_masters[] = {
	&omap3xxx_l4_per__timer8,
};

/* timer8 slave port */
static struct omap_hwmod_ocp_if *omap3xxx_timer8_slaves[] = {
	&omap3xxx_l4_per__timer8,
};

/* timer8 hwmod */
static struct omap_hwmod omap3xxx_timer8_hwmod = {
	.name		= "timer8",
	.mpu_irqs	= omap3xxx_timer8_mpu_irqs,
	.mpu_irqs_cnt	= ARRAY_SIZE(omap3xxx_timer8_mpu_irqs),
	.main_clk	= "gpt8_fck",
	.prcm		= {
		.omap2 = {
			.prcm_reg_id = 1,
			.module_bit = OMAP3430_EN_GPT8_SHIFT,
			.module_offs = OMAP3430_PER_MOD,
			.idlest_reg_id = 1,
			.idlest_idle_bit = OMAP3430_EN_GPT8_SHIFT,
		},
	},
	.masters	= omap3xxx_timer8_masters,
	.masters_cnt	= ARRAY_SIZE(omap3xxx_timer8_masters),
	.slaves		= omap3xxx_timer8_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap3xxx_timer8_slaves),
	.class		= &omap3xxx_timer_hwmod_class,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430)
};


/* timer9 */
static struct omap_hwmod omap3xxx_timer9_hwmod;
static struct omap_hwmod_irq_info omap3xxx_timer9_mpu_irqs[] = {
	{ .irq = INT_24XX_GPTIMER9, },
};

static struct omap_hwmod_addr_space omap3xxx_timer9_addrs[] = {
	{
		.pa_start	= 0x49040000,
		.pa_end		= 0x49040000 + SZ_1K - 1,
		.flags		= ADDR_TYPE_RT
	},
};

/* l4_per -> timer9 */
static struct omap_hwmod_ocp_if omap3xxx_l4_per__timer9 = {
	.master		= &omap3xxx_l4_per_hwmod,
	.slave		= &omap3xxx_timer9_hwmod,
	.clk		= "gpt9_ick",
	.addr		= omap3xxx_timer9_addrs,
	.addr_cnt	= ARRAY_SIZE(omap3xxx_timer9_addrs),
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* timer9 master port */
static struct omap_hwmod_ocp_if *omap3xxx_timer9_masters[] = {
	&omap3xxx_l4_per__timer9,
};

/* timer9 slave port */
static struct omap_hwmod_ocp_if *omap3xxx_timer9_slaves[] = {
	&omap3xxx_l4_per__timer9,
};

/* timer9 hwmod */
static struct omap_hwmod omap3xxx_timer9_hwmod = {
	.name		= "timer9",
	.mpu_irqs	= omap3xxx_timer9_mpu_irqs,
	.mpu_irqs_cnt	= ARRAY_SIZE(omap3xxx_timer9_mpu_irqs),
	.main_clk	= "gpt9_fck",
	.prcm		= {
		.omap2 = {
			.prcm_reg_id = 1,
			.module_bit = OMAP3430_EN_GPT9_SHIFT,
			.module_offs = OMAP3430_PER_MOD,
			.idlest_reg_id = 1,
			.idlest_idle_bit = OMAP3430_EN_GPT9_SHIFT,
		},
	},
	.masters	= omap3xxx_timer9_masters,
	.masters_cnt	= ARRAY_SIZE(omap3xxx_timer9_masters),
	.slaves		= omap3xxx_timer9_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap3xxx_timer9_slaves),
	.class		= &omap3xxx_timer_hwmod_class,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430)
};


/* Slave interfaces on the L4_PER interconnect */
static struct omap_hwmod_ocp_if *omap3xxx_l4_per_slaves[] = {
	&omap3xxx_l3_main__l4_per,
};

/* Master interfaces on the L4_PER interconnect */
static struct omap_hwmod_ocp_if *omap3xxx_l4_per_masters[] = {
	&omap3_l4_per__uart3,
};

/* L4 PER */
static struct omap_hwmod omap3xxx_l4_per_hwmod = {
	.name		= "l4_per",
	.class		= &l4_hwmod_class,
	.masters	= omap3xxx_l4_per_masters,
	.masters_cnt	= ARRAY_SIZE(omap3xxx_l4_per_masters),
	.slaves		= omap3xxx_l4_per_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap3xxx_l4_per_slaves),
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430),
	.flags		= HWMOD_NO_IDLEST,
};

/* timer1 */
static struct omap_hwmod omap3xxx_timer1_hwmod;
static struct omap_hwmod_irq_info omap3xxx_timer1_mpu_irqs[] = {
	{ .irq = INT_24XX_GPTIMER1, },
};

static struct omap_hwmod_addr_space omap3xxx_timer1_addrs[] = {
	{
		.pa_start	= 0x48318000,
		.pa_end		= 0x48318000 + SZ_1K - 1,
		.flags		= ADDR_TYPE_RT
	},
};

/* l4_wkup -> timer1 */
static struct omap_hwmod_ocp_if omap3xxx_l4_wkup__timer1 = {
	.master		= &omap3xxx_l4_wkup_hwmod,
	.slave		= &omap3xxx_timer1_hwmod,
	.clk		= "gpt1_ick",
	.addr		= omap3xxx_timer1_addrs,
	.addr_cnt	= ARRAY_SIZE(omap3xxx_timer1_addrs),
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* timer1 master port */
static struct omap_hwmod_ocp_if *omap3xxx_timer1_masters[] = {
	&omap3xxx_l4_wkup__timer1,
};

/* timer1 slave port */
static struct omap_hwmod_ocp_if *omap3xxx_timer1_slaves[] = {
	&omap3xxx_l4_wkup__timer1,
};

/* timer1 hwmod */
static struct omap_hwmod omap3xxx_timer1_hwmod = {
	.name		= "timer1",
	.mpu_irqs	= omap3xxx_timer1_mpu_irqs,
	.mpu_irqs_cnt	= ARRAY_SIZE(omap3xxx_timer1_mpu_irqs),
	.main_clk	= "gpt1_fck",
	.prcm		= {
		.omap2 = {
			.prcm_reg_id = 1,
			.module_bit = OMAP3430_EN_GPT1_SHIFT,
			.module_offs = WKUP_MOD,
			.idlest_reg_id = 1,
			.idlest_idle_bit = OMAP3430_EN_GPT1_SHIFT,
		},
	},
	.masters	= omap3xxx_timer1_masters,
	.masters_cnt	= ARRAY_SIZE(omap3xxx_timer1_masters),
	.slaves		= omap3xxx_timer1_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap3xxx_timer1_slaves),
	.class		= &omap3xxx_timer_1ms_hwmod_class,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430)
};
/* Slave interfaces on the L4_WKUP interconnect */
static struct omap_hwmod_ocp_if *omap3xxx_l4_wkup_slaves[] = {
	&omap3xxx_l4_core__l4_wkup,
	&omap34xx_l4_core__mcspi1,
	&omap34xx_l4_core__mcspi2,
	&omap34xx_l4_core__mcspi3,
	&omap34xx_l4_core__mcspi4,
	&omap3xxx_l4_wkup__timer1,
};

/* Master interfaces on the L4_WKUP interconnect */
static struct omap_hwmod_ocp_if *omap3xxx_l4_wkup_masters[] = {
};

/* L4 WKUP */
static struct omap_hwmod omap3xxx_l4_wkup_hwmod = {
	.name		= "l4_wkup",
	.class		= &l4_hwmod_class,
	.masters	= omap3xxx_l4_wkup_masters,
	.masters_cnt	= ARRAY_SIZE(omap3xxx_l4_wkup_masters),
	.slaves		= omap3xxx_l4_wkup_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap3xxx_l4_wkup_slaves),
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430),
	.flags		= HWMOD_NO_IDLEST,
};

/* Master interfaces on the MPU device */
static struct omap_hwmod_ocp_if *omap3xxx_mpu_masters[] = {
	&omap3xxx_mpu__l3_main,
};

/* MPU */
static struct omap_hwmod omap3xxx_mpu_hwmod = {
	.name		= "mpu",
	.class		= &mpu_hwmod_class,
	.main_clk	= "arm_fck",
	.vdd_name	= "mpu",
	.masters	= omap3xxx_mpu_masters,
	.masters_cnt	= ARRAY_SIZE(omap3xxx_mpu_masters),
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430),
};

/*
 * ISP MMU interface data
 */

static struct omap_hwmod_class_sysconfig omap3xxx_ispmmu_sysc = {
	.rev_offs	= 0x00,
	.sysc_offs	= 0x10,
	.syss_offs	= 0x14,
	.sysc_flags	= (SYSC_HAS_CLOCKACTIVITY | SYSC_HAS_SIDLEMODE |
			   SYSC_HAS_SOFTRESET | SYSC_HAS_AUTOIDLE |
			   SYSS_HAS_RESET_STATUS),
	.idlemodes	= (SIDLE_FORCE | SIDLE_NO | SIDLE_SMART),
	.sysc_fields	= &omap_hwmod_sysc_type1,
};

static struct omap_hwmod_class omap3xxx_ispmmu_hwmod_class = {
	.name = "isp",
	.sysc = &omap3xxx_ispmmu_sysc,
};

static struct omap_hwmod_irq_info omap3xxx_ispmmu_irqs[] = {
	{ .irq = INT_34XX_CAM_IRQ, },
};

static struct omap_hwmod_addr_space omap3xxx_ispmmu_addrs[] = {
	{
		.pa_start	= 0x480bd400,
		.pa_end		= 0x480bd4ff,
		.flags		= ADDR_TYPE_RT
	},
};

/* ISP <- L3 interface */
static struct omap_hwmod_ocp_if omap3xxx_l3__ispmmu = {
	.master		= &omap3xxx_l3_main_hwmod,
	.slave		= &omap3xxx_ispmmu_hwmod,
	.clk		= "cam_ick",
	.addr		= omap3xxx_ispmmu_addrs,
	.addr_cnt	= ARRAY_SIZE(omap3xxx_ispmmu_addrs),
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};
static struct omap_hwmod_ocp_if *omap3xxx_ispmmu_slaves[] = {
	&omap3xxx_l3__ispmmu,
};

/*
 * ISP
 */

static struct omap_hwmod omap3xxx_ispmmu_hwmod = {
	.name		= "isp",
	.class		= &omap3xxx_ispmmu_hwmod_class,
	.vdd_name	= "mpu",
	.mpu_irqs	= omap3xxx_ispmmu_irqs,
	.mpu_irqs_cnt	= ARRAY_SIZE(omap3xxx_ispmmu_irqs),
	.flags		= (HWMOD_NO_IDLEST | HWMOD_INIT_NO_RESET),
	.slaves		= omap3xxx_ispmmu_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap3xxx_ispmmu_slaves),
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3630ES1_2)
};

/*
 * IVA2_2 interface data
 */

/* IVA2 <- L3 interface */
static struct omap_hwmod_ocp_if omap3xxx_l3__iva = {
	.master		= &omap3xxx_l3_main_hwmod,
	.slave		= &omap3xxx_iva_hwmod,
	.clk		= "iva2_ck",
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

static struct omap_hwmod_ocp_if *omap3xxx_iva_masters[] = {
	&omap3xxx_l3__iva,
};

/*
 * IVA2 (IVA2)
 */

static struct omap_hwmod omap3xxx_iva_hwmod = {
	.name		= "iva",
	.class          = &iva_hwmod_class,
	.vdd_name	= "mpu",
	.masters	= omap3xxx_iva_masters,
	.masters_cnt	= ARRAY_SIZE(omap3xxx_iva_masters),
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430)
};

/* I2C common */
static struct omap_hwmod_class_sysconfig i2c_sysc = {
	.rev_offs	= 0x00,
	.sysc_offs	= 0x20,
	.syss_offs	= 0x10,
	.sysc_flags	= (SYSC_HAS_CLOCKACTIVITY | SYSC_HAS_SIDLEMODE |
			   SYSC_HAS_ENAWAKEUP | SYSC_HAS_SOFTRESET |
			   SYSC_HAS_AUTOIDLE | SYSS_HAS_RESET_STATUS),
	.idlemodes	= (SIDLE_FORCE | SIDLE_NO | SIDLE_SMART),
	.sysc_fields    = &omap_hwmod_sysc_type1,
};

static struct omap_hwmod_class i2c_class = {
	.name = "i2c",
	.sysc = &i2c_sysc,
};

/* I2C1 */

static struct omap_i2c_dev_attr i2c1_dev_attr = {
	.fifo_depth	= 8, /* bytes */
};

static struct omap_hwmod_irq_info i2c1_mpu_irqs[] = {
	{ .irq = INT_24XX_I2C1_IRQ, },
};

static struct omap_hwmod_dma_info i2c1_sdma_reqs[] = {
	{ .name = "tx", .dma_req = OMAP24XX_DMA_I2C1_TX },
	{ .name = "rx", .dma_req = OMAP24XX_DMA_I2C1_RX },
};

static struct omap_hwmod_ocp_if *omap3xxx_i2c1_slaves[] = {
	&omap3_l4_core__i2c1,
};

static struct omap_hwmod omap3xxx_i2c1_hwmod = {
	.name		= "i2c1",
	.mpu_irqs	= i2c1_mpu_irqs,
	.mpu_irqs_cnt	= ARRAY_SIZE(i2c1_mpu_irqs),
	.sdma_reqs	= i2c1_sdma_reqs,
	.sdma_reqs_cnt	= ARRAY_SIZE(i2c1_sdma_reqs),
	.main_clk	= "i2c1_fck",
	.prcm		= {
		.omap2 = {
			.prcm_reg_id = 1,
			.module_bit = OMAP3430_GRPSEL_I2C1_SHIFT,
			.idlest_reg_id = 1,
			.idlest_idle_bit = OMAP3430_GRPSEL_I2C1_SHIFT,
		},
	},
	.slaves		= omap3xxx_i2c1_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap3xxx_i2c1_slaves),
	.class		= &i2c_class,
	.dev_attr	= &i2c1_dev_attr,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430),
};

/* I2C2 */

static struct omap_i2c_dev_attr i2c2_dev_attr = {
	.fifo_depth	= 8, /* bytes */
};

static struct omap_hwmod_irq_info i2c2_mpu_irqs[] = {
	{ .irq = INT_24XX_I2C2_IRQ, },
};

static struct omap_hwmod_dma_info i2c2_sdma_reqs[] = {
	{ .name = "tx", .dma_req = OMAP24XX_DMA_I2C2_TX },
	{ .name = "rx", .dma_req = OMAP24XX_DMA_I2C2_RX },
};

static struct omap_hwmod_ocp_if *omap3xxx_i2c2_slaves[] = {
	&omap3_l4_core__i2c2,
};

static struct omap_hwmod omap3xxx_i2c2_hwmod = {
	.name		= "i2c2",
	.mpu_irqs	= i2c2_mpu_irqs,
	.mpu_irqs_cnt	= ARRAY_SIZE(i2c2_mpu_irqs),
	.sdma_reqs	= i2c2_sdma_reqs,
	.sdma_reqs_cnt	= ARRAY_SIZE(i2c2_sdma_reqs),
	.main_clk	= "i2c2_fck",
	.prcm		= {
		.omap2 = {
			.prcm_reg_id = 1,
			.module_bit = OMAP3430_GRPSEL_I2C2_SHIFT,
			.idlest_reg_id = 1,
			.idlest_idle_bit = OMAP3430_GRPSEL_I2C2_SHIFT,
		},
	},
	.slaves		= omap3xxx_i2c2_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap3xxx_i2c2_slaves),
	.class		= &i2c_class,
	.dev_attr	= &i2c2_dev_attr,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430),
};

/* I2C3 */

static struct omap_i2c_dev_attr i2c3_dev_attr = {
	.fifo_depth	= 64, /* bytes */
};

static struct omap_hwmod_irq_info i2c3_mpu_irqs[] = {
	{ .irq = INT_34XX_I2C3_IRQ, },
};

static struct omap_hwmod_dma_info i2c3_sdma_reqs[] = {
	{ .name = "tx", .dma_req = OMAP34XX_DMA_I2C3_TX },
	{ .name = "rx", .dma_req = OMAP34XX_DMA_I2C3_RX },
};

static struct omap_hwmod_ocp_if *omap3xxx_i2c3_slaves[] = {
	&omap3_l4_core__i2c3,
};

static struct omap_hwmod omap3xxx_i2c3_hwmod = {
	.name		= "i2c3",
	.mpu_irqs	= i2c3_mpu_irqs,
	.mpu_irqs_cnt	= ARRAY_SIZE(i2c3_mpu_irqs),
	.sdma_reqs	= i2c3_sdma_reqs,
	.sdma_reqs_cnt	= ARRAY_SIZE(i2c3_sdma_reqs),
	.main_clk	= "i2c3_fck",
	.prcm		= {
		.omap2 = {
			.prcm_reg_id = 1,
			.module_bit = OMAP3430_GRPSEL_I2C3_SHIFT,
			.idlest_reg_id = 1,
			.idlest_idle_bit = OMAP3430_GRPSEL_I2C3_SHIFT,
		},
	},
	.slaves		= omap3xxx_i2c3_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap3xxx_i2c3_slaves),
	.class		= &i2c_class,
	.dev_attr	= &i2c3_dev_attr,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430),
};

/* L4 WKUP -> GPIO1 interface */
static struct omap_hwmod_addr_space omap3xxx_gpio1_addrs[] = {
	{
		.pa_start	= 0x48310000,
		.pa_end		= 0x483101ff,
	.flags		= ADDR_TYPE_RT
	},
};

static struct omap_hwmod_ocp_if omap3xxx_l4_wkup__gpio1 = {
	.master		= &omap3xxx_l4_wkup_hwmod,
	.slave		= &omap3xxx_gpio1_hwmod,
	.clk		= "gpio1_ick",
	.addr		= omap3xxx_gpio1_addrs,
	.addr_cnt	= ARRAY_SIZE(omap3xxx_gpio1_addrs),
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* L4 PER -> GPIO2 interface */
static struct omap_hwmod_addr_space omap3xxx_gpio2_addrs[] = {
	{
		.pa_start	= 0x49050000,
		.pa_end		= 0x490501ff,
		.flags		= ADDR_TYPE_RT
	},
};

static struct omap_hwmod_ocp_if omap3xxx_l4_per__gpio2 = {
	.master		= &omap3xxx_l4_per_hwmod,
	.slave		= &omap3xxx_gpio2_hwmod,
	.clk		= "gpio2_ick",
	.addr		= omap3xxx_gpio2_addrs,
	.addr_cnt	= ARRAY_SIZE(omap3xxx_gpio2_addrs),
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* L4 PER -> GPIO3 interface */
static struct omap_hwmod_addr_space omap3xxx_gpio3_addrs[] = {
	{
		.pa_start	= 0x49052000,
		.pa_end		= 0x490521ff,
		.flags		= ADDR_TYPE_RT
	},
};

static struct omap_hwmod_ocp_if omap3xxx_l4_per__gpio3 = {
	.master		= &omap3xxx_l4_per_hwmod,
	.slave		= &omap3xxx_gpio3_hwmod,
	.clk		= "gpio3_ick",
	.addr		= omap3xxx_gpio3_addrs,
	.addr_cnt	= ARRAY_SIZE(omap3xxx_gpio3_addrs),
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* L4 PER -> GPIO4 interface */
static struct omap_hwmod_addr_space omap3xxx_gpio4_addrs[] = {
	{
		.pa_start	= 0x49054000,
		.pa_end		= 0x490541ff,
		.flags		= ADDR_TYPE_RT
	},
};

static struct omap_hwmod_ocp_if omap3xxx_l4_per__gpio4 = {
	.master		= &omap3xxx_l4_per_hwmod,
	.slave		= &omap3xxx_gpio4_hwmod,
	.clk		= "gpio4_ick",
	.addr		= omap3xxx_gpio4_addrs,
	.addr_cnt	= ARRAY_SIZE(omap3xxx_gpio4_addrs),
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* L4 PER -> GPIO5 interface */
static struct omap_hwmod_addr_space omap3xxx_gpio5_addrs[] = {
	{
		.pa_start	= 0x49056000,
		.pa_end		= 0x490561ff,
		.flags		= ADDR_TYPE_RT
	},
};

static struct omap_hwmod_ocp_if omap3xxx_l4_per__gpio5 = {
	.master		= &omap3xxx_l4_per_hwmod,
	.slave		= &omap3xxx_gpio5_hwmod,
	.clk		= "gpio5_ick",
	.addr		= omap3xxx_gpio5_addrs,
	.addr_cnt	= ARRAY_SIZE(omap3xxx_gpio5_addrs),
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* L4 PER -> GPIO6 interface */
static struct omap_hwmod_addr_space omap3xxx_gpio6_addrs[] = {
	{
		.pa_start	= 0x49058000,
		.pa_end		= 0x490581ff,
		.flags		= ADDR_TYPE_RT
	},
};

static struct omap_hwmod_ocp_if omap3xxx_l4_per__gpio6 = {
	.master		= &omap3xxx_l4_per_hwmod,
	.slave		= &omap3xxx_gpio6_hwmod,
	.clk		= "gpio6_ick",
	.addr		= omap3xxx_gpio6_addrs,
	.addr_cnt	= ARRAY_SIZE(omap3xxx_gpio6_addrs),
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* GPIO common*/
static struct omap_hwmod_class_sysconfig omap3xxx_gpio_sysc = {
	.rev_offs	= 0x0000,
	.sysc_offs	= 0x0010,
	.syss_offs	= 0x0014,
	.sysc_flags	= (SYSC_HAS_ENAWAKEUP | SYSC_HAS_SIDLEMODE |
			   SYSC_HAS_SOFTRESET | SYSC_HAS_AUTOIDLE |
			   SYSS_HAS_RESET_STATUS),
	.idlemodes	= (SIDLE_FORCE | SIDLE_NO | SIDLE_SMART),
	.sysc_fields    = &omap_hwmod_sysc_type1,
};

static struct omap_hwmod_class omap3xxx_gpio_hwmod_class = {
	.name = "gpio",
	.sysc = &omap3xxx_gpio_sysc,
	.rev = 1,
};

/* GPIO dev_attr common for gpio2-6*/

static struct omap_gpio_dev_attr gpio_dev_attr = {
	.bank_width = 32,
	.dbck_flag = true,
	.off_mode_support = true,
};

/* GPIO1 */

static struct omap_gpio_dev_attr gpio1_dev_attr = {
	.bank_width = 32,
	.dbck_flag = true,
	.off_mode_support = false,
};

static struct omap_hwmod_irq_info omap3xxx_gpio1_irqs[] = {
	{ .name = "gpio_mpu_irq", .irq = INT_34XX_GPIO_BANK1 },
};

static struct omap_hwmod_opt_clk gpio1_opt_clks[] = {
	{ .role = "dbclk", .clk = "gpio1_dbck", },
};

static struct omap_hwmod_ocp_if *omap3xxx_gpio1_slaves[] = {
	&omap3xxx_l4_wkup__gpio1,
};

static struct omap_hwmod omap3xxx_gpio1_hwmod = {
	.name		= "gpio1",
	.mpu_irqs	= omap3xxx_gpio1_irqs,
	.mpu_irqs_cnt	= ARRAY_SIZE(omap3xxx_gpio1_irqs),
	.main_clk	= NULL,
	.opt_clks	= gpio1_opt_clks,
	.opt_clks_cnt	= ARRAY_SIZE(gpio1_opt_clks),
	.prcm		= {
		.omap2 = {
			.prcm_reg_id = 1,
			.module_bit = OMAP3430_EN_GPIO1_SHIFT,
			.module_offs = WKUP_MOD,
			.idlest_reg_id = 1,
			.idlest_idle_bit = 3,
		},
	},
	.slaves		= omap3xxx_gpio1_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap3xxx_gpio1_slaves),
	.class		= &omap3xxx_gpio_hwmod_class,
	.dev_attr	= &gpio1_dev_attr,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430),
};

/* GPIO2 */

static struct omap_hwmod_irq_info omap3xxx_gpio2_irqs[] = {
	{ .name = "gpio_mpu_irq", .irq = INT_34XX_GPIO_BANK2 },
};

static struct omap_hwmod_opt_clk gpio2_opt_clks[] = {
	{ .role = "dbclk", .clk = "gpio2_dbck", },
};

static struct omap_hwmod_ocp_if *omap3xxx_gpio2_slaves[] = {
	&omap3xxx_l4_per__gpio2,
};

static struct omap_hwmod omap3xxx_gpio2_hwmod = {
	.name		= "gpio2",
	.mpu_irqs	= omap3xxx_gpio2_irqs,
	.mpu_irqs_cnt	= ARRAY_SIZE(omap3xxx_gpio2_irqs),
	.main_clk	= NULL,
	.opt_clks	= gpio2_opt_clks,
	.opt_clks_cnt	= ARRAY_SIZE(gpio2_opt_clks),
	.prcm		= {
		.omap2 = {
			.prcm_reg_id = 1,
			.module_bit = OMAP3430_EN_GPIO2_SHIFT,
			.module_offs = OMAP3430_PER_MOD,
			.idlest_reg_id = 1,
			.idlest_idle_bit = 13,
		},
	},
	.slaves		= omap3xxx_gpio2_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap3xxx_gpio2_slaves),
	.class		= &omap3xxx_gpio_hwmod_class,
	.dev_attr	= &gpio_dev_attr,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430),
};

/* GPIO3 */

static struct omap_hwmod_irq_info omap3xxx_gpio3_irqs[] = {
	{ .name = "gpio_mpu_irq", .irq = INT_34XX_GPIO_BANK3 },
};

static struct omap_hwmod_opt_clk gpio3_opt_clks[] = {
	{ .role = "dbclk", .clk = "gpio3_dbck", },
};

static struct omap_hwmod_ocp_if *omap3xxx_gpio3_slaves[] = {
	&omap3xxx_l4_per__gpio3,
};

static struct omap_hwmod omap3xxx_gpio3_hwmod = {
	.name		= "gpio3",
	.mpu_irqs	= omap3xxx_gpio3_irqs,
	.mpu_irqs_cnt	= ARRAY_SIZE(omap3xxx_gpio3_irqs),
	.main_clk	= NULL,
	.opt_clks	= gpio3_opt_clks,
	.opt_clks_cnt	= ARRAY_SIZE(gpio3_opt_clks),
	.prcm		= {
		.omap2 = {
			.prcm_reg_id = 1,
			.module_bit = OMAP3430_EN_GPIO3_SHIFT,
			.module_offs = OMAP3430_PER_MOD,
			.idlest_reg_id = 1,
			.idlest_idle_bit = 14,
		},
	},
	.slaves		= omap3xxx_gpio3_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap3xxx_gpio3_slaves),
	.class		= &omap3xxx_gpio_hwmod_class,
	.dev_attr	= &gpio_dev_attr,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430),
};

/* GPIO4 */

static struct omap_hwmod_irq_info omap3xxx_gpio4_irqs[] = {
	{ .name = "gpio_mpu_irq", .irq = INT_34XX_GPIO_BANK4 },
};

static struct omap_hwmod_opt_clk gpio4_opt_clks[] = {
	{ .role = "dbclk", .clk = "gpio4_dbck", },
};

static struct omap_hwmod_ocp_if *omap3xxx_gpio4_slaves[] = {
	&omap3xxx_l4_per__gpio4,
};

static struct omap_hwmod omap3xxx_gpio4_hwmod = {
	.name		= "gpio4",
	.mpu_irqs	= omap3xxx_gpio4_irqs,
	.mpu_irqs_cnt	= ARRAY_SIZE(omap3xxx_gpio4_irqs),
	.main_clk	= NULL,
	.opt_clks	= gpio4_opt_clks,
	.opt_clks_cnt	= ARRAY_SIZE(gpio4_opt_clks),
	.prcm		= {
		.omap2 = {
			.prcm_reg_id = 1,
			.module_bit = OMAP3430_EN_GPIO4_SHIFT,
			.module_offs = OMAP3430_PER_MOD,
			.idlest_reg_id = 1,
			.idlest_idle_bit = 15,
		},
	},
	.slaves		= omap3xxx_gpio4_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap3xxx_gpio4_slaves),
	.class		= &omap3xxx_gpio_hwmod_class,
	.dev_attr	= &gpio_dev_attr,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430),
};


/* GPIO5 */

static struct omap_hwmod_irq_info omap3xxx_gpio5_irqs[] = {
	{ .name = "gpio_mpu_irq", .irq = INT_34XX_GPIO_BANK5 },
};

static struct omap_hwmod_opt_clk gpio5_opt_clks[] = {
	{ .role = "dbclk", .clk = "gpio5_dbck", },
};

static struct omap_hwmod_ocp_if *omap3xxx_gpio5_slaves[] = {
	&omap3xxx_l4_per__gpio5,
};

static struct omap_hwmod omap3xxx_gpio5_hwmod = {
	.name		= "gpio5",
	.mpu_irqs	= omap3xxx_gpio5_irqs,
	.mpu_irqs_cnt	= ARRAY_SIZE(omap3xxx_gpio5_irqs),
	.main_clk	= NULL,
	.opt_clks	= gpio5_opt_clks,
	.opt_clks_cnt	= ARRAY_SIZE(gpio5_opt_clks),
	.prcm		= {
		.omap2 = {
			.prcm_reg_id = 1,
			.module_bit = OMAP3430_EN_GPIO5_SHIFT,
			.module_offs = OMAP3430_PER_MOD,
			.idlest_reg_id = 1,
			.idlest_idle_bit = 16,
		},
	},
	.slaves		= omap3xxx_gpio5_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap3xxx_gpio5_slaves),
	.class		= &omap3xxx_gpio_hwmod_class,
	.dev_attr	= &gpio_dev_attr,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430),
};

/* GPIO6 */

static struct omap_hwmod_irq_info omap3xxx_gpio6_irqs[] = {
	{ .name = "gpio_mpu_irq", .irq = INT_34XX_GPIO_BANK6 },
};

static struct omap_hwmod_opt_clk gpio6_opt_clks[] = {
	{ .role = "dbclk", .clk = "gpio6_dbck", },
};

static struct omap_hwmod_ocp_if *omap3xxx_gpio6_slaves[] = {
	&omap3xxx_l4_per__gpio6,
};

static struct omap_hwmod omap3xxx_gpio6_hwmod = {
	.name		= "gpio6",
	.mpu_irqs	= omap3xxx_gpio6_irqs,
	.mpu_irqs_cnt	= ARRAY_SIZE(omap3xxx_gpio6_irqs),
	.main_clk	= NULL,
	.opt_clks	= gpio6_opt_clks,
	.opt_clks_cnt	= ARRAY_SIZE(gpio6_opt_clks),
	.prcm		= {
		.omap2 = {
			.prcm_reg_id = 1,
			.module_bit = OMAP3430_EN_GPIO6_SHIFT,
			.module_offs = OMAP3430_PER_MOD,
			.idlest_reg_id = 1,
			.idlest_idle_bit = 17,
		},
	},
	.slaves		= omap3xxx_gpio6_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap3xxx_gpio6_slaves),
	.class		= &omap3xxx_gpio_hwmod_class,
	.dev_attr	= &gpio_dev_attr,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430),
};

/*
 * 'gpu' class
 * 2d/3d graphics accelerator
 */

static struct omap_hwmod_class omap3xxx_gpu_hwmod_class = {
	.name = "gpu",
};

/* gpu */
static struct omap_hwmod_irq_info omap3xxx_gpu_irqs[] = {
	{ .irq = INT_24XX_GPU_IRQ },
};

/* gpu master ports */
static struct omap_hwmod_ocp_if *omap3xxx_gpu_masters[] = {
	&omap3xxx_gpu__l3_main,
};

static struct omap_hwmod_addr_space omap3xxx_gpu_addrs[] = {
	{
		.pa_start       = 0x50000000,
		.pa_end         = 0x50003fff,
		.flags          = ADDR_TYPE_RT
	},
};

/* l3_main -> gpu */
static struct omap_hwmod_ocp_if omap3xxx_l3_main__gpu = {
	.master         = &omap3xxx_l3_main_hwmod,
	.slave          = &omap3xxx_gpu_hwmod,
	.clk		= "core_l3_ick",
	.addr           = omap3xxx_gpu_addrs,
	.addr_cnt       = ARRAY_SIZE(omap3xxx_gpu_addrs),
	.user           = OCP_USER_MPU | OCP_USER_SDMA,
};

/* gpu slave ports */
static struct omap_hwmod_ocp_if *omap3xxx_gpu_slaves[] = {
	&omap3xxx_l3_main__gpu,
};

static struct omap_hwmod omap3xxx_gpu_hwmod = {
	.name           = "gpu",
	.class          = &omap3xxx_gpu_hwmod_class,
	.mpu_irqs       = omap3xxx_gpu_irqs,
	.mpu_irqs_cnt   = ARRAY_SIZE(omap3xxx_gpu_irqs),
	.flags		= HWMOD_NO_IDLEST,
	.vdd_name       = "core",
	.slaves         = omap3xxx_gpu_slaves,
	.slaves_cnt     = ARRAY_SIZE(omap3xxx_gpu_slaves),
	.masters        = omap3xxx_gpu_masters,
	.masters_cnt    = ARRAY_SIZE(omap3xxx_gpu_masters),
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430),
};



/* WDTIMER common */

static struct omap_hwmod_class_sysconfig omap3xxx_wd_timer_sysc = {
	.rev_offs	= 0x0000,
	.sysc_offs	= 0x0010,
	.syss_offs	= 0x0014,
	.sysc_flags	= (SYSC_HAS_SIDLEMODE | SYSC_HAS_EMUFREE |
			   SYSC_HAS_ENAWAKEUP | SYSC_HAS_SOFTRESET |
			   SYSC_HAS_AUTOIDLE | SYSS_HAS_RESET_STATUS),
	.idlemodes	= (SIDLE_FORCE | SIDLE_NO | SIDLE_SMART),
	.sysc_fields    = &omap_hwmod_sysc_type1,
};

static struct omap_hwmod_class omap3xxx_wd_timer_hwmod_class = {
	.name = "wd_timer",
	.sysc = &omap3xxx_wd_timer_sysc,
};

/* WDTIMER2 */
static struct omap_hwmod_ocp_if *omap3xxx_wd_timer2_slaves[] = {
	&omap3xxx_l4_wkup__wd_timer2,
};

static struct omap_hwmod omap3xxx_wd_timer2_hwmod = {
	.name		= "wd_timer2",
	.class		= &omap3xxx_wd_timer_hwmod_class,
	.main_clk	= "wdt2_fck",
	.prcm		= {
		.omap2 = {
			.prcm_reg_id = 1,
			.module_bit = OMAP3430_EN_WDT2_SHIFT,
			.module_offs = WKUP_MOD,
			.idlest_reg_id = 1,
			.idlest_idle_bit = OMAP3430_EN_WDT2_SHIFT,
		},
	},
	.slaves		= omap3xxx_wd_timer2_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap3xxx_wd_timer2_slaves),
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430),
};

/* UART common */

static struct omap_hwmod_class_sysconfig uart_sysc = {
	.rev_offs	= 0x50,
	.sysc_offs	= 0x54,
	.syss_offs	= 0x58,
	.sysc_flags	= (SYSC_HAS_SIDLEMODE |
			   SYSC_HAS_ENAWAKEUP | SYSC_HAS_SOFTRESET |
			   SYSC_HAS_AUTOIDLE | SYSS_HAS_RESET_STATUS),
	.idlemodes	= (SIDLE_FORCE | SIDLE_NO | SIDLE_SMART),
	.sysc_fields    = &omap_hwmod_sysc_type1,
};

static struct omap_hwmod_class uart_class = {
	.name = "uart",
	.sysc = &uart_sysc,
};

/* UART1 */

static struct omap_hwmod_irq_info uart1_mpu_irqs[] = {
	{ .irq = INT_24XX_UART1_IRQ, },
};

static struct omap_hwmod_dma_info uart1_sdma_chs[] = {
	{ .name = "tx",	.dma_req = OMAP24XX_DMA_UART1_TX, },
	{ .name = "rx",	.dma_req = OMAP24XX_DMA_UART1_RX, },
};

static struct omap_hwmod_ocp_if *omap3xxx_uart1_slaves[] = {
	&omap3_l4_core__uart1,
};

static struct omap_hwmod omap3xxx_uart1_hwmod = {
	.name		= "uart1",
	.mpu_irqs	= uart1_mpu_irqs,
	.mpu_irqs_cnt	= ARRAY_SIZE(uart1_mpu_irqs),
	.sdma_reqs	= uart1_sdma_chs,
	.sdma_reqs_cnt	= ARRAY_SIZE(uart1_sdma_chs),
	.main_clk	= "uart1_fck",
	.prcm		= {
		.omap2 = {
			.module_offs = CORE_MOD,
			.prcm_reg_id = 1,
			.module_bit = OMAP3430_EN_UART1_SHIFT,
			.idlest_reg_id = 1,
			.idlest_idle_bit = OMAP3430_EN_UART1_SHIFT,
		},
	},
	.slaves		= omap3xxx_uart1_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap3xxx_uart1_slaves),
	.class		= &uart_class,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430),
};

/* UART2 */

static struct omap_hwmod_irq_info uart2_mpu_irqs[] = {
	{ .irq = INT_24XX_UART2_IRQ, },
};

static struct omap_hwmod_dma_info uart2_sdma_chs[] = {
	{ .name = "tx",	.dma_req = OMAP24XX_DMA_UART2_TX, },
	{ .name = "rx",	.dma_req = OMAP24XX_DMA_UART2_RX, },
};

static struct omap_hwmod_ocp_if *omap3xxx_uart2_slaves[] = {
	&omap3_l4_core__uart2,
};

static struct omap_hwmod omap3xxx_uart2_hwmod = {
	.name		= "uart2",
	.mpu_irqs	= uart2_mpu_irqs,
	.mpu_irqs_cnt	= ARRAY_SIZE(uart2_mpu_irqs),
	.sdma_reqs	= uart2_sdma_chs,
	.sdma_reqs_cnt	= ARRAY_SIZE(uart2_sdma_chs),
	.main_clk	= "uart2_fck",
	.prcm		= {
		.omap2 = {
			.module_offs = CORE_MOD,
			.prcm_reg_id = 1,
			.module_bit = OMAP3430_EN_UART2_SHIFT,
			.idlest_reg_id = 1,
			.idlest_idle_bit = OMAP3430_EN_UART2_SHIFT,
		},
	},
	.slaves		= omap3xxx_uart2_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap3xxx_uart2_slaves),
	.class		= &uart_class,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430),
};

/* UART3 */

static struct omap_hwmod_irq_info uart3_mpu_irqs[] = {
	{ .irq = INT_24XX_UART3_IRQ, },
};

static struct omap_hwmod_dma_info uart3_sdma_chs[] = {
	{ .name = "tx",	.dma_req = OMAP24XX_DMA_UART3_TX, },
	{ .name = "rx",	.dma_req = OMAP24XX_DMA_UART3_RX, },
};

static struct omap_hwmod_ocp_if *omap3xxx_uart3_slaves[] = {
	&omap3_l4_per__uart3,
};

static struct omap_hwmod omap3xxx_uart3_hwmod = {
	.name		= "uart3",
	.mpu_irqs	= uart3_mpu_irqs,
	.mpu_irqs_cnt	= ARRAY_SIZE(uart3_mpu_irqs),
	.sdma_reqs	= uart3_sdma_chs,
	.sdma_reqs_cnt	= ARRAY_SIZE(uart3_sdma_chs),
	.main_clk	= "uart3_fck",
	.prcm		= {
		.omap2 = {
			.module_offs = OMAP3430_PER_MOD,
			.prcm_reg_id = 1,
			.module_bit = OMAP3430_EN_UART3_SHIFT,
			.idlest_reg_id = 1,
			.idlest_idle_bit = OMAP3430_EN_UART3_SHIFT,
		},
	},
	.slaves		= omap3xxx_uart3_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap3xxx_uart3_slaves),
	.class		= &uart_class,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430),
};

/*
 * USBHSOTG (USBHS)
 */
static struct omap_hwmod_class_sysconfig omap3xxx_usbhsotg_sysc = {
	.rev_offs	= 0x0400,
	.sysc_offs	= 0x0404,
	.syss_offs	= 0x0408,
	.sysc_flags	= (SYSC_HAS_SIDLEMODE | SYSC_HAS_MIDLEMODE|
			  SYSC_HAS_ENAWAKEUP | SYSC_HAS_SOFTRESET |
			  SYSC_HAS_AUTOIDLE | SYSS_HAS_RESET_STATUS),
	.idlemodes	= SIDLE_FORCE | SIDLE_NO | SIDLE_SMART,
	.sysc_fields	= &omap_hwmod_sysc_type1,
};

static struct omap_hwmod_class usbotg_class = {
	.name = "usbotg",
	.sysc = &omap3xxx_usbhsotg_sysc,
};

/* usb_otg_hs */
static struct omap_hwmod_irq_info omap3xxx_usbhsotg_mpu_irqs[] = {

	{ .name = "mc", .irq = 92 },
	{ .name = "dma", .irq = 93 },

};

static struct omap_hwmod omap3xxx_usbhsotg_hwmod = {
	.name		= "usb_otg_hs",
	.mpu_irqs	= omap3xxx_usbhsotg_mpu_irqs,
	.mpu_irqs_cnt	= ARRAY_SIZE(omap3xxx_usbhsotg_mpu_irqs),
	.main_clk	= "hsotgusb_ick",
	.prcm		= {
		.omap2 = {
			.prcm_reg_id = 1,
			.module_bit = OMAP3430_GRPSEL_HSOTGUSB_MASK,
			.module_offs = CORE_MOD,
			.idlest_reg_id = 1,
			.idlest_idle_bit = OMAP3430ES2_ST_HSOTGUSB_IDLE_SHIFT,
			.idlest_stdby_bit = OMAP3430ES2_ST_HSOTGUSB_STDBY_SHIFT
		},
	},
	.masters	= omap3xxx_usbhsotg_masters,
	.masters_cnt	= ARRAY_SIZE(omap3xxx_usbhsotg_masters),
	.slaves		= omap3xxx_usbhsotg_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap3xxx_usbhsotg_slaves),
	.class		= &usbotg_class,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430)
};

/* dma_system -> L3 */
static struct omap_hwmod_ocp_if omap3xxx_dma_system__l3 = {
	.master		= &omap3xxx_dma_system_hwmod,
	.slave		= &omap3xxx_l3_main_hwmod,
	.clk		= "core_l3_ick",
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* dma attributes */
static struct omap_dma_dev_attr dma_dev_attr = {
	.dma_dev_attr = DMA_LINKED_LCH | GLOBAL_PRIORITY |
				IS_CSSA_32 | IS_CDSA_32 | IS_RW_PRIORIY,
	.dma_lch_count = OMAP_DMA4_LOGICAL_DMA_CH_COUNT,
};;

static struct omap_hwmod_class_sysconfig omap3xxx_dma_sysc = {
	.rev_offs	= 0x0000,
	.sysc_offs	= 0x002c,
	.syss_offs	= 0x0028,
	.sysc_flags	= (SYSC_HAS_SIDLEMODE | SYSC_HAS_SOFTRESET |
			   SYSC_HAS_MIDLEMODE | SYSC_HAS_CLOCKACTIVITY |
			   SYSC_HAS_EMUFREE | SYSC_HAS_AUTOIDLE |
			   SYSS_HAS_RESET_STATUS),
	.idlemodes	= (SIDLE_FORCE | SIDLE_NO | SIDLE_SMART |
			   MSTANDBY_FORCE | MSTANDBY_NO | MSTANDBY_SMART),
	.sysc_fields	= &omap_hwmod_sysc_type1,
};

static struct omap_hwmod_class omap3xxx_dma_hwmod_class = {
	.name = "dma",
	.sysc = &omap3xxx_dma_sysc,
};

/* dma_system */
static struct omap_hwmod_irq_info omap3xxx_dma_system_irqs[] = {
	{ .name = "dma_0", .irq = INT_24XX_SDMA_IRQ0 },
	{ .name = "dma_1", .irq = INT_24XX_SDMA_IRQ1 },
	{ .name = "dma_2", .irq = INT_24XX_SDMA_IRQ2 },
	{ .name = "dma_3", .irq = INT_24XX_SDMA_IRQ3 },
};

static struct omap_hwmod_addr_space omap3xxx_dma_system_addrs[] = {
	{
		.pa_start	= 0x48056000,
		.pa_end		= 0x4a0560ff,
		.flags		= ADDR_TYPE_RT
	},
};

/* dma_system master ports */
static struct omap_hwmod_ocp_if *omap3xxx_dma_system_masters[] = {
	&omap3xxx_dma_system__l3,
};

/* l4_cfg -> dma_system */
static struct omap_hwmod_ocp_if omap3xxx_l4_core__dma_system = {
	.master		= &omap3xxx_l4_core_hwmod,
	.slave		= &omap3xxx_dma_system_hwmod,
	.clk		= "core_l4_ick",
	.addr		= omap3xxx_dma_system_addrs,
	.addr_cnt	= ARRAY_SIZE(omap3xxx_dma_system_addrs),
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* dma_system slave ports */
static struct omap_hwmod_ocp_if *omap3xxx_dma_system_slaves[] = {
	&omap3xxx_l4_core__dma_system,
};

static struct omap_hwmod omap3xxx_dma_system_hwmod = {
	.name		= "dma",
	.class		= &omap3xxx_dma_hwmod_class,
	.mpu_irqs	= omap3xxx_dma_system_irqs,
	.mpu_irqs_cnt	= ARRAY_SIZE(omap3xxx_dma_system_irqs),
	.main_clk	= "core_l3_ick",
	.flags		= HWMOD_NO_IDLEST,
	.slaves		= omap3xxx_dma_system_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap3xxx_dma_system_slaves),
	.masters	= omap3xxx_dma_system_masters,
	.masters_cnt	= ARRAY_SIZE(omap3xxx_dma_system_masters),
	.dev_attr	= &dma_dev_attr,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430),
};

/* SPI common */
static struct omap_hwmod_class_sysconfig omap34xx_mcspi_sysc = {
	.rev_offs	= 0x0000,
	.sysc_offs	= 0x0010,
	.syss_offs	= 0x0014,
	.sysc_flags	= (SYSC_HAS_CLOCKACTIVITY | SYSC_HAS_SIDLEMODE |
				SYSC_HAS_ENAWAKEUP | SYSC_HAS_SOFTRESET |
				SYSC_HAS_AUTOIDLE | SYSS_HAS_RESET_STATUS),
	.idlemodes	= (SIDLE_FORCE | SIDLE_NO | SIDLE_SMART),
	.sysc_fields    = &omap_hwmod_sysc_type1,
};

static struct omap_hwmod_class omap34xx_mcspi_class = {
	.name = "mcspi",
	.sysc = &omap34xx_mcspi_sysc,
};

/* SPI1 */
static struct omap_hwmod_irq_info omap34xx_mcspi1_mpu_irqs[] = {
	{ .name = "irq", .irq = INT_24XX_SPI1_IRQ }, /* 65 */
};

static struct omap_hwmod_dma_info omap34xx_mcspi1_sdma_chs[] = {
	{ .name = "tx0", .dma_req = OMAP24XX_DMA_SPI1_TX0 }, /* 34 */
	{ .name = "tx2", .dma_req = OMAP24XX_DMA_SPI1_TX2 }, /* 38 */
	{ .name = "rx1", .dma_req = OMAP24XX_DMA_SPI1_RX1 }, /* 37 */
	{ .name = "tx1", .dma_req = OMAP24XX_DMA_SPI1_TX1 }, /* 36 */
	{ .name = "rx0", .dma_req = OMAP24XX_DMA_SPI1_RX0 }, /* 35 */
	{ .name = "rx2", .dma_req = OMAP24XX_DMA_SPI1_RX2 }, /* 39 */
	{ .name = "rx3", .dma_req = OMAP24XX_DMA_SPI1_RX3 }, /* 41 */
	{ .name = "tx3", .dma_req = OMAP24XX_DMA_SPI1_TX3 }, /* 40 */
};

static struct omap_hwmod_ocp_if *omap34xx_mcspi1_slaves[] = {
	&omap34xx_l4_core__mcspi1,
};

static struct omap_hwmod omap34xx_mcspi1 = {
	.name		= "mcspi1",
	.mpu_irqs	= omap34xx_mcspi1_mpu_irqs,
	.mpu_irqs_cnt	= ARRAY_SIZE(omap34xx_mcspi1_mpu_irqs),
	.sdma_reqs	= omap34xx_mcspi1_sdma_chs,
	.sdma_reqs_cnt	= ARRAY_SIZE(omap34xx_mcspi1_sdma_chs),
	.main_clk 	= "mcspi1_fck",
	.prcm		= {
		.omap2 = {
			.module_offs = CORE_MOD,
			.prcm_reg_id = 1,
			.module_bit = OMAP3430_EN_MCSPI1_SHIFT,
			.idlest_reg_id = 1,
			.idlest_idle_bit = OMAP3430_EN_MCSPI1_SHIFT,
		},
	},
	.slaves		= omap34xx_mcspi1_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap34xx_mcspi1_slaves),
	.class		= &omap34xx_mcspi_class,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430),
};

/* SPI2 */
static struct omap_hwmod_irq_info omap34xx_mcspi2_mpu_irqs[] = {
	{ .name = "irq", .irq = INT_24XX_SPI2_IRQ }, /* 66 */
};

static struct omap_hwmod_dma_info omap34xx_mcspi2_sdma_chs[] = {
	{ .name = "tx0", .dma_req = OMAP24XX_DMA_SPI2_TX0 }, /* 42 */
	{ .name = "tx1", .dma_req = OMAP24XX_DMA_SPI2_TX1 }, /* 44 */
	{ .name = "rx0", .dma_req = OMAP24XX_DMA_SPI2_RX0 }, /* 43 */
	{ .name = "rx1", .dma_req = OMAP24XX_DMA_SPI2_RX1 }, /* 45 */
};

static struct omap_hwmod_ocp_if *omap34xx_mcspi2_slaves[] = {
	&omap34xx_l4_core__mcspi2,
};

static struct omap_hwmod omap34xx_mcspi2 = {
	.name		= "mcspi2",
	.mpu_irqs	= omap34xx_mcspi2_mpu_irqs,
	.mpu_irqs_cnt	= ARRAY_SIZE(omap34xx_mcspi2_mpu_irqs),
	.sdma_reqs	= omap34xx_mcspi2_sdma_chs,
	.sdma_reqs_cnt	= ARRAY_SIZE(omap34xx_mcspi2_sdma_chs),
	.main_clk 	= "mcspi2_fck",
	.prcm		= {
		.omap2 = {
			.module_offs = CORE_MOD,
			.prcm_reg_id = 1,
			.module_bit = OMAP3430_EN_MCSPI2_SHIFT,
			.idlest_reg_id = 1,
			.idlest_idle_bit = OMAP3430_EN_MCSPI2_SHIFT,
		},
	},
	.slaves		= omap34xx_mcspi2_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap34xx_mcspi2_slaves),
	.class		= &omap34xx_mcspi_class,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430),
};

/* SPI3 */
static struct omap_hwmod_irq_info omap34xx_mcspi3_mpu_irqs[] = {
	{ .name = "irq", .irq = INT_24XX_SPI3_IRQ }, /* 91 */
};

static struct omap_hwmod_dma_info omap34xx_mcspi3_sdma_chs[] = {
	{ .name = "tx0", .dma_req = OMAP24XX_DMA_SPI3_TX0 }, /* 14 */
	{ .name = "tx1", .dma_req = OMAP24XX_DMA_SPI3_TX1 }, /* 22 */
	{ .name = "rx0", .dma_req = OMAP24XX_DMA_SPI3_RX0 }, /* 15 */
	{ .name = "rx1", .dma_req = OMAP24XX_DMA_SPI3_RX1 }, /* 23 */
};

static struct omap_hwmod_ocp_if *omap34xx_mcspi3_slaves[] = {
	&omap34xx_l4_core__mcspi3,
};

static struct omap_hwmod omap34xx_mcspi3 = {
	.name		= "mcspi3",
	.mpu_irqs	= omap34xx_mcspi3_mpu_irqs,
	.mpu_irqs_cnt	= ARRAY_SIZE(omap34xx_mcspi3_mpu_irqs),
	.sdma_reqs	= omap34xx_mcspi3_sdma_chs,
	.sdma_reqs_cnt	= ARRAY_SIZE(omap34xx_mcspi3_sdma_chs),
	.main_clk 	= "mcspi3_fck",
	.prcm		= {
		.omap2 = {
			.module_offs = CORE_MOD,
			.prcm_reg_id = 1,
			.module_bit = OMAP3430_EN_MCSPI3_SHIFT,
			.idlest_reg_id = 1,
			.idlest_idle_bit = OMAP3430_EN_MCSPI3_SHIFT,
		},
	},
	.slaves		= omap34xx_mcspi3_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap34xx_mcspi3_slaves),
	.class		= &omap34xx_mcspi_class,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430),
};

/* SPI4 */
static struct omap_hwmod_irq_info omap34xx_mcspi4_mpu_irqs[] = {
	{ .name = "irq", .irq = INT_34XX_SPI4_IRQ }, /* 48 */
};

static struct omap_hwmod_dma_info omap34xx_mcspi4_sdma_chs[] = {
	{ .name = "tx0", .dma_req = OMAP34XX_DMA_SPI4_TX0 }, /* 69 */
	{ .name = "rx0", .dma_req = OMAP34XX_DMA_SPI4_RX0 }, /* 70 */
};

static struct omap_hwmod_ocp_if *omap34xx_mcspi4_slaves[] = {
	&omap34xx_l4_core__mcspi4,
};

static struct omap_hwmod omap34xx_mcspi4 = {
	.name		= "mcspi4",
	.mpu_irqs	= omap34xx_mcspi4_mpu_irqs,
	.mpu_irqs_cnt	= ARRAY_SIZE(omap34xx_mcspi4_mpu_irqs),
	.sdma_reqs	= omap34xx_mcspi4_sdma_chs,
	.sdma_reqs_cnt	= ARRAY_SIZE(omap34xx_mcspi4_sdma_chs),
	.main_clk 	= "mcspi4_fck",
	.prcm		= {
		.omap2 = {
			.module_offs = CORE_MOD,
			.prcm_reg_id = 1,
			.module_bit = OMAP3430_EN_MCSPI4_SHIFT,
			.idlest_reg_id = 1,
			.idlest_idle_bit = OMAP3430_EN_MCSPI4_SHIFT,
		},
	},
	.slaves		= omap34xx_mcspi4_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap34xx_mcspi4_slaves),
	.class		= &omap34xx_mcspi_class,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430),
};

/* MMC/SD/SDIO common */

static struct omap_hwmod_class_sysconfig mmc_sysc = {
	.rev_offs	= 0x00,
	.sysc_offs	= 0x10,
	.syss_offs	= 0x14,
	.sysc_flags	= (SYSC_HAS_CLOCKACTIVITY | SYSC_HAS_SIDLEMODE |
			   SYSC_HAS_ENAWAKEUP | SYSC_HAS_SOFTRESET |
			   SYSC_HAS_AUTOIDLE | SYSS_HAS_RESET_STATUS),
	.idlemodes	= (SIDLE_FORCE | SIDLE_NO | SIDLE_SMART),
	.sysc_fields    = &omap_hwmod_sysc_type1,
};

static struct omap_hwmod_class mmc_class = {
	.name = "mmc",
	.sysc = &mmc_sysc,
};


/* MMC/SD/SDIO1 */
static struct omap_hwmod_irq_info mmc1_mpu_irqs[] = {
	{ .irq = INT_24XX_MMC_IRQ, },
};

static struct omap_hwmod_dma_info mmc1_sdma_chs[] = {
	{ .name = "tx",	.dma_req = OMAP24XX_DMA_MMC1_TX, },
	{ .name = "rx",	.dma_req = OMAP24XX_DMA_MMC1_RX, },
};

static struct omap_hwmod_opt_clk mmc1_opt_clks[] = {
	{ .role = "dbck", .clk = "omap_32k_fck", },
};

static struct omap_hwmod_ocp_if *omap3xxx_mmc1_slaves[] = {
	&omap3xxx_l4_core__mmc1,
};

static struct omap_hwmod omap3xxx_mmc1_hwmod = {
	.name		= "mmc1",
	.mpu_irqs	= mmc1_mpu_irqs,
	.mpu_irqs_cnt	= ARRAY_SIZE(mmc1_mpu_irqs),
	.sdma_reqs	= mmc1_sdma_chs,
	.sdma_reqs_cnt	= ARRAY_SIZE(mmc1_sdma_chs),
	.opt_clks	= mmc1_opt_clks,
	.opt_clks_cnt	= ARRAY_SIZE(mmc1_opt_clks),
	.main_clk	= "mmchs1_fck",
	.prcm		= {
		.omap2 = {
			.prcm_reg_id = 1,
			.module_bit = OMAP3430_GRPSEL_MMC1_SHIFT,
			.idlest_reg_id = 1,
			.idlest_idle_bit = OMAP3430_GRPSEL_MMC1_SHIFT,
		},
	},
	.slaves		= omap3xxx_mmc1_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap3xxx_mmc1_slaves),
	.class		= &mmc_class,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430),
};

/* MMC/SD/SDIO2 */

static struct omap_hwmod_irq_info mmc2_mpu_irqs[] = {
	{ .irq = INT_24XX_MMC2_IRQ, },
};

static struct omap_hwmod_dma_info mmc2_sdma_chs[] = {
	{ .name = "tx",	.dma_req = OMAP24XX_DMA_MMC2_TX, },
	{ .name = "rx",	.dma_req = OMAP24XX_DMA_MMC2_RX, },
};

static struct omap_hwmod_opt_clk mmc2_opt_clks[] = {
	{ .role = "dbck", .clk = "omap_32k_fck", },
};

static struct omap_hwmod_ocp_if *omap3xxx_mmc2_slaves[] = {
	&omap3xxx_l4_core__mmc2,
};

static struct omap_hwmod omap3xxx_mmc2_hwmod = {
	.name		= "mmc2",
	.mpu_irqs	= mmc2_mpu_irqs,
	.mpu_irqs_cnt	= ARRAY_SIZE(mmc2_mpu_irqs),
	.sdma_reqs	= mmc2_sdma_chs,
	.sdma_reqs_cnt	= ARRAY_SIZE(mmc2_sdma_chs),
	.opt_clks	= mmc2_opt_clks,
	.opt_clks_cnt	= ARRAY_SIZE(mmc2_opt_clks),
	.main_clk	= "mmchs2_fck",
	.prcm		= {
		.omap2 = {
			.prcm_reg_id = 1,
			.module_bit = OMAP3430_GRPSEL_MMC2_SHIFT,
			.idlest_reg_id = 1,
			.idlest_idle_bit = OMAP3430_GRPSEL_MMC2_SHIFT,
		},
	},
	.slaves		= omap3xxx_mmc2_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap3xxx_mmc2_slaves),
	.class		= &mmc_class,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430),
};

/* MMC/SD/SDIO3 */
static struct omap_hwmod_irq_info mmc3_mpu_irqs[] = {
	{ .irq = INT_34XX_MMC3_IRQ, },
};

static struct omap_hwmod_dma_info mmc3_sdma_chs[] = {
	{ .name = "tx",	.dma_req = OMAP34XX_DMA_MMC3_TX, },
	{ .name = "rx",	.dma_req = OMAP34XX_DMA_MMC3_RX, },
};

static struct omap_hwmod_opt_clk mmc3_opt_clks[] = {
	{ .role = "dbck", .clk = "omap_32k_fck", },
};

static struct omap_hwmod_ocp_if *omap3xxx_mmc3_slaves[] = {
	&omap3xxx_l4_core__mmc3,
};

static struct omap_hwmod omap3xxx_mmc3_hwmod = {
	.name		= "mmc3",
	.mpu_irqs	= mmc3_mpu_irqs,
	.mpu_irqs_cnt	= ARRAY_SIZE(mmc3_mpu_irqs),
	.sdma_reqs	= mmc3_sdma_chs,
	.sdma_reqs_cnt	= ARRAY_SIZE(mmc3_sdma_chs),
	.opt_clks	= mmc3_opt_clks,
	.opt_clks_cnt	= ARRAY_SIZE(mmc3_opt_clks),
	.main_clk	= "mmchs3_fck",
	.prcm		= {
		.omap2 = {
			.prcm_reg_id = 1,
			.module_bit = OMAP3430ES2_GRPSEL_MMC3_SHIFT,
			.idlest_reg_id = 1,
			.idlest_idle_bit = OMAP3430ES2_GRPSEL_MMC3_SHIFT,
		},
	},
	.slaves		= omap3xxx_mmc3_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap3xxx_mmc3_slaves),
	.class		= &mmc_class,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430),
};

/* SR common */
static struct omap_hwmod_sysc_fields omap34xx_sr_sysc_fields = {
	.clkact_shift	= 20,
};

static struct omap_hwmod_class_sysconfig omap34xx_sr_sysc = {
	.sysc_offs	= 0x24,
	.sysc_flags	= (SYSC_HAS_CLOCKACTIVITY | SYSC_NO_CACHE),
	.clockact	= CLOCKACT_TEST_ICLK,
	.sysc_fields	= &omap34xx_sr_sysc_fields,
};

static struct omap_hwmod_class omap34xx_smartreflex_hwmod_class = {
	.name = "smartreflex",
	.sysc = &omap34xx_sr_sysc,
	.rev  = 1,
};

static struct omap_hwmod_sysc_fields omap36xx_sr_sysc_fields = {
	.sidle_shift	= 24,
	.enwkup_shift	= 26
};

static struct omap_hwmod_class_sysconfig omap36xx_sr_sysc = {
	.sysc_offs	= 0x38,
	.idlemodes	= (SIDLE_FORCE | SIDLE_NO | SIDLE_SMART),
	.sysc_flags	= (SYSC_HAS_SIDLEMODE | SYSC_HAS_ENAWAKEUP |
			   SYSC_NO_CACHE),
	.sysc_fields	= &omap36xx_sr_sysc_fields,
};

static struct omap_hwmod_class omap36xx_smartreflex_hwmod_class = {
	.name = "smartreflex",
	.sysc = &omap36xx_sr_sysc,
	.rev  = 2,
};

/* SR1 */
static struct omap_hwmod_ocp_if *omap3_sr1_slaves[] = {
	&omap3_l4_core__sr1,
};

static u32 omap34xx_sr1_efuse_offs[] = {
	OMAP343X_CONTROL_FUSE_OPP1_VDD1, OMAP343X_CONTROL_FUSE_OPP2_VDD1,
	OMAP343X_CONTROL_FUSE_OPP3_VDD1, OMAP343X_CONTROL_FUSE_OPP4_VDD1,
	OMAP343X_CONTROL_FUSE_OPP5_VDD1,
};

static u32 omap34xx_sr1_test_nvalues[] = {
	0x9A90E6, 0xAABE9A, 0xBBF5C5, 0xBBB292, 0xBBF5C5,
};

static struct omap_sr_dev_data omap34xx_sr1_dev_attr = {
	.efuse_sr_control	= OMAP343X_CONTROL_FUSE_SR,
	.sennenable_shift	= OMAP343X_SR1_SENNENABLE_SHIFT,
	.senpenable_shift	= OMAP343X_SR1_SENPENABLE_SHIFT,
	.efuse_nvalues_offs	= omap34xx_sr1_efuse_offs,
	.test_sennenable	= 0x3,
	.test_senpenable	= 0x3,
	.test_nvalues		= omap34xx_sr1_test_nvalues,
	.vdd_name		= "mpu",
};

static struct omap_hwmod omap34xx_sr1_hwmod = {
	.name		= "sr1_hwmod",
	.class		= &omap34xx_smartreflex_hwmod_class,
	.main_clk	= "sr1_fck",
	.prcm		= {
		.omap2 = {
			.prcm_reg_id = 1,
			.module_bit = OMAP3430_EN_SR1_SHIFT,
			.module_offs = WKUP_MOD,
			.idlest_reg_id = 1,
			.idlest_idle_bit = OMAP3430_EN_SR1_SHIFT,
		},
	},
	.slaves		= omap3_sr1_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap3_sr1_slaves),
	.dev_attr	= &omap34xx_sr1_dev_attr,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430ES2 |
					CHIP_IS_OMAP3430ES3_0 |
					CHIP_IS_OMAP3430ES3_1),
	.flags		= HWMOD_SET_DEFAULT_CLOCKACT,
};

static u32 omap36xx_sr1_efuse_offs[] = {
	OMAP3630_CONTROL_FUSE_OPP50_VDD1, OMAP3630_CONTROL_FUSE_OPP100_VDD1,
	OMAP3630_CONTROL_FUSE_OPP120_VDD1, OMAP3630_CONTROL_FUSE_OPP1G_VDD1,
};

static u32 omap36xx_sr1_test_nvalues[] = {
	0x898beb, 0x999b83, 0xaac5a8, 0xaab197,
};

static struct omap_sr_dev_data omap36xx_sr1_dev_attr = {
	.efuse_nvalues_offs	= omap36xx_sr1_efuse_offs,
	.test_sennenable	= 0x1,
	.test_senpenable	= 0x1,
	.test_nvalues		= omap36xx_sr1_test_nvalues,
	.vdd_name		= "mpu",
};

static struct omap_hwmod omap36xx_sr1_hwmod = {
	.name		= "sr1_hwmod",
	.class		= &omap36xx_smartreflex_hwmod_class,
	.main_clk	= "sr1_fck",
	.prcm		= {
		.omap2 = {
			.prcm_reg_id = 1,
			.module_bit = OMAP3430_EN_SR1_SHIFT,
			.module_offs = WKUP_MOD,
			.idlest_reg_id = 1,
			.idlest_idle_bit = OMAP3430_EN_SR1_SHIFT,
		},
	},
	.slaves		= omap3_sr1_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap3_sr1_slaves),
	.dev_attr	= &omap36xx_sr1_dev_attr,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3630ES1),
};

/* SR2 */
static struct omap_hwmod_ocp_if *omap3_sr2_slaves[] = {
	&omap3_l4_core__sr2,
};

static u32 omap34xx_sr2_efuse_offs[] = {
	OMAP343X_CONTROL_FUSE_OPP1_VDD2, OMAP343X_CONTROL_FUSE_OPP2_VDD2,
	OMAP343X_CONTROL_FUSE_OPP3_VDD2,
};

static u32 omap34xx_sr2_test_nvalues[] = {
	0x0, 0xAAC098, 0xAB89D9
};

static struct omap_sr_dev_data omap34xx_sr2_dev_attr = {
	.efuse_sr_control	= OMAP343X_CONTROL_FUSE_SR,
	.sennenable_shift	= OMAP343X_SR2_SENNENABLE_SHIFT,
	.senpenable_shift	= OMAP343X_SR2_SENPENABLE_SHIFT,
	.efuse_nvalues_offs	= omap34xx_sr2_efuse_offs,
	.test_sennenable	= 0x3,
	.test_senpenable	= 0x3,
	.test_nvalues		= omap34xx_sr2_test_nvalues,
	.vdd_name		= "core",
};

static struct omap_hwmod omap34xx_sr2_hwmod = {
	.name		= "sr2_hwmod",
	.class		= &omap34xx_smartreflex_hwmod_class,
	.main_clk	= "sr2_fck",
	.prcm		= {
		.omap2 = {
			.prcm_reg_id = 1,
			.module_bit = OMAP3430_EN_SR2_SHIFT,
			.module_offs = WKUP_MOD,
			.idlest_reg_id = 1,
			.idlest_idle_bit = OMAP3430_EN_SR2_SHIFT,
		},
	},
	.slaves		= omap3_sr2_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap3_sr2_slaves),
	.dev_attr	= &omap34xx_sr2_dev_attr,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430ES2 |
					CHIP_IS_OMAP3430ES3_0 |
					CHIP_IS_OMAP3430ES3_1),
	.flags		= HWMOD_SET_DEFAULT_CLOCKACT,
};

static u32 omap36xx_sr2_efuse_offs[] = {
	OMAP3630_CONTROL_FUSE_OPP50_VDD2, OMAP3630_CONTROL_FUSE_OPP100_VDD2,
};

static u32 omap36xx_sr2_test_nvalues[] = {
	0x898beb, 0x9a8cee,
};

static struct omap_sr_dev_data omap36xx_sr2_dev_attr = {
	.efuse_nvalues_offs	= omap36xx_sr2_efuse_offs,
	.test_sennenable	= 0x1,
	.test_senpenable	= 0x1,
	.test_nvalues		= omap36xx_sr2_test_nvalues,
	.vdd_name		= "core",
};

static struct omap_hwmod omap36xx_sr2_hwmod = {
	.name		= "sr2_hwmod",
	.class		= &omap36xx_smartreflex_hwmod_class,
	.main_clk	= "sr2_fck",
	.prcm		= {
		.omap2 = {
			.prcm_reg_id = 1,
			.module_bit = OMAP3430_EN_SR2_SHIFT,
			.module_offs = WKUP_MOD,
			.idlest_reg_id = 1,
			.idlest_idle_bit = OMAP3430_EN_SR2_SHIFT,
		},
	},
	.slaves		= omap3_sr2_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap3_sr2_slaves),
	.dev_attr	= &omap36xx_sr2_dev_attr,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3630ES1),
};

/*
 * 'mcbsp' class
 * multi channel buffered serial port controller
 */

static struct omap_hwmod_class_sysconfig omap3xxx_mcbsp_sysc = {
	.sysc_offs	= 0x008c,
	.sysc_flags	= (SYSC_HAS_ENAWAKEUP | SYSC_HAS_SIDLEMODE |
			   SYSC_HAS_CLOCKACTIVITY | SYSC_HAS_SOFTRESET),
	.idlemodes	= (SIDLE_FORCE | SIDLE_NO | SIDLE_SMART),
	.sysc_fields	= &omap_hwmod_sysc_type1,
	.clockact	= 0x2,
};

static struct omap_hwmod_class omap3xxx_mcbsp_hwmod_class = {
	.name = "mcbsp",
	.sysc = &omap3xxx_mcbsp_sysc,
};

/* mcbsp1 */
static struct omap_hwmod_irq_info omap3xxx_mcbsp1_irqs[] = {
	{ .name = "irq", .irq = 16 },
	{ .name = "tx", .irq = 59 },
	{ .name = "rx", .irq = 60 },
};

static struct omap_hwmod_dma_info omap3xxx_mcbsp1_sdma_chs[] = {
	{ .name = "rx", .dma_req = 32 },
	{ .name = "tx", .dma_req = 31 },
};

static struct omap_hwmod_addr_space omap3xxx_mcbsp1_addrs[] = {
	{
		.pa_start	= 0x48074000,
		.pa_end		= 0x480740ff,
		.flags		= ADDR_TYPE_RT
	},
};

/* l4_core -> mcbsp1 */
static struct omap_hwmod_ocp_if omap3xxx_l4_core__mcbsp1 = {
	.master		= &omap3xxx_l4_core_hwmod,
	.slave		= &omap3xxx_mcbsp1_hwmod,
	.clk		= "mcbsp1_ick",
	.addr		= omap3xxx_mcbsp1_addrs,
	.addr_cnt	= ARRAY_SIZE(omap3xxx_mcbsp1_addrs),
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* mcbsp1 slave ports */
static struct omap_hwmod_ocp_if *omap3xxx_mcbsp1_slaves[] = {
	&omap3xxx_l4_core__mcbsp1,
};

static struct omap_hwmod omap3xxx_mcbsp1_hwmod = {
	.name		= "mcbsp1",
	.class		= &omap3xxx_mcbsp_hwmod_class,
	.mpu_irqs	= omap3xxx_mcbsp1_irqs,
	.mpu_irqs_cnt	= ARRAY_SIZE(omap3xxx_mcbsp1_irqs),
	.sdma_reqs	= omap3xxx_mcbsp1_sdma_chs,
	.sdma_reqs_cnt	= ARRAY_SIZE(omap3xxx_mcbsp1_sdma_chs),
	.main_clk	= "mcbsp1_fck",
	.prcm		= {
		.omap2 = {
			.prcm_reg_id = 1,
			.module_bit = OMAP3430_EN_MCBSP1_SHIFT,
			.module_offs = CORE_MOD,
			.idlest_reg_id = 1,
			.idlest_idle_bit = OMAP3430_EN_MCBSP1_SHIFT,
		},
	},
	.slaves		= omap3xxx_mcbsp1_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap3xxx_mcbsp1_slaves),
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430),
};

/* mcbsp2 */
static struct omap_hwmod_irq_info omap3xxx_mcbsp2_irqs[] = {
	{ .name = "irq", .irq = 17 },
	{ .name = "tx", .irq = 62 },
	{ .name = "rx", .irq = 63 },
};

static struct omap_hwmod_dma_info omap3xxx_mcbsp2_sdma_chs[] = {
	{ .name = "rx", .dma_req = 34 },
	{ .name = "tx", .dma_req = 33 },
};

static struct omap_hwmod_addr_space omap3xxx_mcbsp2_addrs[] = {
	{
		.pa_start	= 0x49022000,
		.pa_end		= 0x490220ff,
		.flags		= ADDR_TYPE_RT
	},
};

/* l4_per -> mcbsp2 */
static struct omap_hwmod_ocp_if omap3xxx_l4_per__mcbsp2 = {
	.master		= &omap3xxx_l4_per_hwmod,
	.slave		= &omap3xxx_mcbsp2_hwmod,
	.clk		= "mcbsp2_ick",
	.addr		= omap3xxx_mcbsp2_addrs,
	.addr_cnt	= ARRAY_SIZE(omap3xxx_mcbsp2_addrs),
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* mcbsp2 slave ports */
static struct omap_hwmod_ocp_if *omap3xxx_mcbsp2_slaves[] = {
	&omap3xxx_l4_per__mcbsp2,
};

static struct omap_hwmod omap3xxx_mcbsp2_hwmod = {
	.name		= "mcbsp2",
	.class		= &omap3xxx_mcbsp_hwmod_class,
	.mpu_irqs	= omap3xxx_mcbsp2_irqs,
	.mpu_irqs_cnt	= ARRAY_SIZE(omap3xxx_mcbsp2_irqs),
	.sdma_reqs	= omap3xxx_mcbsp2_sdma_chs,
	.sdma_reqs_cnt	= ARRAY_SIZE(omap3xxx_mcbsp2_sdma_chs),
	.main_clk	= "mcbsp2_fck",
	.prcm		= {
		.omap2 = {
			.prcm_reg_id = 1,
			.module_bit = OMAP3430_EN_MCBSP2_SHIFT,
			.module_offs = OMAP3430_PER_MOD,
			.idlest_reg_id = 1,
			.idlest_idle_bit = OMAP3430_EN_MCBSP2_SHIFT,
		},
	},
	.slaves		= omap3xxx_mcbsp2_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap3xxx_mcbsp2_slaves),
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430),
};

/* mcbsp3 */
static struct omap_hwmod_irq_info omap3xxx_mcbsp3_irqs[] = {
	{ .name = "irq", .irq = 22 },
	{ .name = "tx", .irq = 89 },
	{ .name = "rx", .irq = 90 },
};

static struct omap_hwmod_dma_info omap3xxx_mcbsp3_sdma_chs[] = {
	{ .name = "rx", .dma_req = 18 },
	{ .name = "tx", .dma_req = 17 },
};

static struct omap_hwmod_addr_space omap3xxx_mcbsp3_addrs[] = {
	{
		.pa_start	= 0x49024000,
		.pa_end		= 0x490240ff,
		.flags		= ADDR_TYPE_RT
	},
};

/* l4_per -> mcbsp3 */
static struct omap_hwmod_ocp_if omap3xxx_l4_per__mcbsp3 = {
	.master		= &omap3xxx_l4_per_hwmod,
	.slave		= &omap3xxx_mcbsp3_hwmod,
	.clk		= "mcbsp3_ick",
	.addr		= omap3xxx_mcbsp3_addrs,
	.addr_cnt	= ARRAY_SIZE(omap3xxx_mcbsp3_addrs),
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* mcbsp3 slave ports */
static struct omap_hwmod_ocp_if *omap3xxx_mcbsp3_slaves[] = {
	&omap3xxx_l4_per__mcbsp3,
};

static struct omap_hwmod omap3xxx_mcbsp3_hwmod = {
	.name		= "mcbsp3",
	.class		= &omap3xxx_mcbsp_hwmod_class,
	.mpu_irqs	= omap3xxx_mcbsp3_irqs,
	.mpu_irqs_cnt	= ARRAY_SIZE(omap3xxx_mcbsp3_irqs),
	.sdma_reqs	= omap3xxx_mcbsp3_sdma_chs,
	.sdma_reqs_cnt	= ARRAY_SIZE(omap3xxx_mcbsp3_sdma_chs),
	.main_clk	= "mcbsp3_fck",
	.prcm		= {
		.omap2 = {
			.prcm_reg_id = 1,
			.module_bit = OMAP3430_EN_MCBSP3_SHIFT,
			.module_offs = OMAP3430_PER_MOD,
			.idlest_reg_id = 1,
			.idlest_idle_bit = OMAP3430_EN_MCBSP3_SHIFT,
		},
	},
	.slaves		= omap3xxx_mcbsp3_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap3xxx_mcbsp3_slaves),
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430),
};

/* mcbsp4 */
static struct omap_hwmod_irq_info omap3xxx_mcbsp4_irqs[] = {
	{ .name = "irq", .irq = 23 },
	{ .name = "tx", .irq = 54 },
	{ .name = "rx", .irq = 55 },
};

static struct omap_hwmod_dma_info omap3xxx_mcbsp4_sdma_chs[] = {
	{ .name = "rx", .dma_req = 20 },
	{ .name = "tx", .dma_req = 19 },
};

static struct omap_hwmod_addr_space omap3xxx_mcbsp4_addrs[] = {
	{
		.pa_start	= 0x49026000,
		.pa_end		= 0x490260ff,
		.flags		= ADDR_TYPE_RT
	},
};

/* l4_per -> mcbsp4 */
static struct omap_hwmod_ocp_if omap3xxx_l4_per__mcbsp4 = {
	.master		= &omap3xxx_l4_per_hwmod,
	.slave		= &omap3xxx_mcbsp4_hwmod,
	.clk		= "mcbsp4_ick",
	.addr		= omap3xxx_mcbsp4_addrs,
	.addr_cnt	= ARRAY_SIZE(omap3xxx_mcbsp4_addrs),
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* mcbsp4 slave ports */
static struct omap_hwmod_ocp_if *omap3xxx_mcbsp4_slaves[] = {
	&omap3xxx_l4_per__mcbsp4,
};

static struct omap_hwmod omap3xxx_mcbsp4_hwmod = {
	.name		= "mcbsp4",
	.class		= &omap3xxx_mcbsp_hwmod_class,
	.mpu_irqs	= omap3xxx_mcbsp4_irqs,
	.mpu_irqs_cnt	= ARRAY_SIZE(omap3xxx_mcbsp4_irqs),
	.sdma_reqs	= omap3xxx_mcbsp4_sdma_chs,
	.sdma_reqs_cnt	= ARRAY_SIZE(omap3xxx_mcbsp4_sdma_chs),
	.main_clk	= "mcbsp4_fck",
	.prcm		= {
		.omap2 = {
			.prcm_reg_id = 1,
			.module_bit = OMAP3430_EN_MCBSP4_SHIFT,
			.module_offs = OMAP3430_PER_MOD,
			.idlest_reg_id = 1,
			.idlest_idle_bit = OMAP3430_EN_MCBSP4_SHIFT,
		},
	},
	.slaves		= omap3xxx_mcbsp4_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap3xxx_mcbsp4_slaves),
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430),
};

/* mcbsp5 */
static struct omap_hwmod_irq_info omap3xxx_mcbsp5_irqs[] = {
	{ .name = "irq", .irq = 27 },
	{ .name = "tx", .irq = 81 },
	{ .name = "rx", .irq = 82 },
};

static struct omap_hwmod_dma_info omap3xxx_mcbsp5_sdma_chs[] = {
	{ .name = "rx", .dma_req = 22 },
	{ .name = "tx", .dma_req = 21 },
};

static struct omap_hwmod_addr_space omap3xxx_mcbsp5_addrs[] = {
	{
		.pa_start	= 0x48096000,
		.pa_end		= 0x480960ff,
		.flags		= ADDR_TYPE_RT
	},
};

/* l4_core -> mcbsp5 */
static struct omap_hwmod_ocp_if omap3xxx_l4_core__mcbsp5 = {
	.master		= &omap3xxx_l4_core_hwmod,
	.slave		= &omap3xxx_mcbsp5_hwmod,
	.clk		= "mcbsp5_ick",
	.addr		= omap3xxx_mcbsp5_addrs,
	.addr_cnt	= ARRAY_SIZE(omap3xxx_mcbsp5_addrs),
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* mcbsp5 slave ports */
static struct omap_hwmod_ocp_if *omap3xxx_mcbsp5_slaves[] = {
	&omap3xxx_l4_core__mcbsp5,
};

static struct omap_hwmod omap3xxx_mcbsp5_hwmod = {
	.name		= "mcbsp5",
	.class		= &omap3xxx_mcbsp_hwmod_class,
	.mpu_irqs	= omap3xxx_mcbsp5_irqs,
	.mpu_irqs_cnt	= ARRAY_SIZE(omap3xxx_mcbsp5_irqs),
	.sdma_reqs	= omap3xxx_mcbsp5_sdma_chs,
	.sdma_reqs_cnt	= ARRAY_SIZE(omap3xxx_mcbsp5_sdma_chs),
	.main_clk	= "mcbsp5_fck",
	.prcm		= {
		.omap2 = {
			.prcm_reg_id = 1,
			.module_bit = OMAP3430_EN_MCBSP5_SHIFT,
			.module_offs = CORE_MOD,
			.idlest_reg_id = 1,
			.idlest_idle_bit = OMAP3430_EN_MCBSP5_SHIFT,
		},
	},
	.slaves		= omap3xxx_mcbsp5_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap3xxx_mcbsp5_slaves),
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430),
};
/* 'mcbsp sidetone' class */

static struct omap_hwmod_class_sysconfig omap3xxx_mcbsp_sidetone_sysc = {
	.sysc_offs	= 0x0010,
	.sysc_flags	= SYSC_HAS_AUTOIDLE,
	.sysc_fields	= &omap_hwmod_sysc_type1,
};

static struct omap_hwmod_class omap3xxx_mcbsp_sidetone_hwmod_class = {
	.name = "mcbsp_sidetone",
	.sysc = &omap3xxx_mcbsp_sidetone_sysc,
};

/* mcbsp2_sidetone */
static struct omap_hwmod_irq_info omap3xxx_mcbsp2_sidetone_irqs[] = {
	{ .name = "irq", .irq = 4 },
};

static struct omap_hwmod_addr_space omap3xxx_mcbsp2_sidetone_addrs[] = {
	{
		.pa_start	= 0x49028000,
		.pa_end		= 0x490280ff,
		.flags		= ADDR_TYPE_RT
	},
};

/* l4_per -> mcbsp2_sidetone */
static struct omap_hwmod_ocp_if omap3xxx_l4_per__mcbsp2_sidetone = {
	.master		= &omap3xxx_l4_per_hwmod,
	.slave		= &omap3xxx_mcbsp2_sidetone_hwmod,
	.clk		= "mcbsp2_ick",
	.addr		= omap3xxx_mcbsp2_sidetone_addrs,
	.addr_cnt	= ARRAY_SIZE(omap3xxx_mcbsp2_sidetone_addrs),
	.user		= OCP_USER_MPU,
};

/* mcbsp2_sidetone slave ports */
static struct omap_hwmod_ocp_if *omap3xxx_mcbsp2_sidetone_slaves[] = {
	&omap3xxx_l4_per__mcbsp2_sidetone,
};

static struct omap_hwmod omap3xxx_mcbsp2_sidetone_hwmod = {
	.name		= "mcbsp2_sidetone",
	.class		= &omap3xxx_mcbsp_sidetone_hwmod_class,
	.mpu_irqs	= omap3xxx_mcbsp2_sidetone_irqs,
	.mpu_irqs_cnt	= ARRAY_SIZE(omap3xxx_mcbsp2_sidetone_irqs),
	.main_clk	= "mcbsp2_fck",
	.prcm		= {
		.omap2 = {
			.prcm_reg_id = 1,
			 .module_bit = OMAP3430_EN_MCBSP2_SHIFT,
			.module_offs = OMAP3430_PER_MOD,
			.idlest_reg_id = 1,
			.idlest_idle_bit = OMAP3430_EN_MCBSP2_SHIFT,
		},
	},
	.slaves		= omap3xxx_mcbsp2_sidetone_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap3xxx_mcbsp2_sidetone_slaves),
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430),
};

/* mcbsp3_sidetone */
static struct omap_hwmod_irq_info omap3xxx_mcbsp3_sidetone_irqs[] = {
	{ .name = "irq", .irq = 5 },
};

static struct omap_hwmod_addr_space omap3xxx_mcbsp3_sidetone_addrs[] = {
	{
		.pa_start	= 0x4902A000,
		.pa_end		= 0x4902A0ff,
		.flags		= ADDR_TYPE_RT
	},
};

/* l4_per -> mcbsp3_sidetone */
static struct omap_hwmod_ocp_if omap3xxx_l4_per__mcbsp3_sidetone = {
	.master		= &omap3xxx_l4_per_hwmod,
	.slave		= &omap3xxx_mcbsp3_sidetone_hwmod,
	.clk		= "mcbsp3_ick",
	.addr		= omap3xxx_mcbsp3_sidetone_addrs,
	.addr_cnt	= ARRAY_SIZE(omap3xxx_mcbsp3_sidetone_addrs),
	.user		= OCP_USER_MPU,
};

/* mcbsp3_sidetone slave ports */
static struct omap_hwmod_ocp_if *omap3xxx_mcbsp3_sidetone_slaves[] = {
	&omap3xxx_l4_per__mcbsp3_sidetone,
};

static struct omap_hwmod omap3xxx_mcbsp3_sidetone_hwmod = {
	.name		= "mcbsp3_sidetone",
	.class		= &omap3xxx_mcbsp_sidetone_hwmod_class,
	.mpu_irqs	= omap3xxx_mcbsp3_sidetone_irqs,
	.mpu_irqs_cnt	= ARRAY_SIZE(omap3xxx_mcbsp3_sidetone_irqs),
	.main_clk	= "mcbsp3_fck",
	.prcm		= {
		.omap2 = {
			.prcm_reg_id = 1,
			.module_bit = OMAP3430_EN_MCBSP3_SHIFT,
			.module_offs = OMAP3430_PER_MOD,
			.idlest_reg_id = 1,
			.idlest_idle_bit = OMAP3430_EN_MCBSP3_SHIFT,
		},
	},
	.slaves		= omap3xxx_mcbsp3_sidetone_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap3xxx_mcbsp3_sidetone_slaves),
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430),
};

/*
 * 'dss' class
 * display sub-system
 */

static struct omap_hwmod_class_sysconfig omap3xxx_dss_sysc = {
	.rev_offs	= 0x0000,
	.sysc_offs	= 0x0010,
	.syss_offs	= 0x0014,
	.sysc_flags	= (SYSC_HAS_SOFTRESET | SYSC_HAS_AUTOIDLE),
	.sysc_fields	= &omap_hwmod_sysc_type1,
};

static struct omap_hwmod_class omap3xxx_dss_hwmod_class = {
	.name = "dss",
	.sysc = &omap3xxx_dss_sysc,
};


/* dss */
static struct omap_hwmod_irq_info omap3xxx_dss_irqs[] = {
	{ .irq = 25 },
};

static struct omap_hwmod_dma_info omap3xxx_dss_sdma_chs[] = {
	{ .name = "dispc", .dma_req = 5 },
	{ .name = "dsi1", .dma_req = 74 },
};

/* dss */
/* dss master ports */
static struct omap_hwmod_ocp_if *omap3xxx_dss_masters[] = {
	&omap3xxx_dss__l3,
};

static struct omap_hwmod_addr_space omap3xxx_dss_addrs[] = {
	{
		.pa_start	= 0x48050000,
		.pa_end		= 0x480503FF,
		.flags		= ADDR_TYPE_RT
	},
};

/* l4_core -> dss */
static struct omap_hwmod_ocp_if omap3xxx_l4_core__dss = {
	.master		= &omap3xxx_l4_core_hwmod,
	.slave		= &omap3xxx_dss_hwmod,
	.clk		= "dss_ick",
	.addr		= omap3xxx_dss_addrs,
	.addr_cnt	= ARRAY_SIZE(omap3xxx_dss_addrs),
	.user		= OCP_USER_MPU,
};


/* dss slave ports */
static struct omap_hwmod_ocp_if *omap3xxx_dss_slaves[] = {
	&omap3xxx_l4_core__dss,
};

static struct omap_hwmod_opt_clk dss_opt_clks[] = {
	{ .role = "tv_fck", .clk = "dss_tv_fck" },
	{ .role = "video_fck", .clk = "dss_96m_fck" },
	{ .role = "dss2_fck", .clk = "dss2_alwon_fck" },
};

static struct omap_hwmod omap3xxx_dss_hwmod = {
	.name		= "dss",
	.class		= &omap3xxx_dss_hwmod_class,
	.main_clk	= "dss1_alwon_fck", /* instead of dss_fck */
	.mpu_irqs	= omap3xxx_dss_irqs,
	.mpu_irqs_cnt	= ARRAY_SIZE(omap3xxx_dss_irqs),
	.sdma_reqs	= omap3xxx_dss_sdma_chs,
	.sdma_reqs_cnt	= ARRAY_SIZE(omap3xxx_dss_sdma_chs),

	.prcm		= {
		.omap2 = {
			.prcm_reg_id = 1,
			.module_bit = OMAP3430_EN_DSS1_SHIFT,
			.module_offs = OMAP3430_DSS_MOD,
			.idlest_reg_id = 1,
			.idlest_idle_bit = OMAP3430ES2_ST_DSS_IDLE_SHIFT,
		},
	},
	.opt_clks	= dss_opt_clks,
	.opt_clks_cnt = ARRAY_SIZE(dss_opt_clks),
	.slaves		= omap3xxx_dss_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap3xxx_dss_slaves),
	.masters	= omap3xxx_dss_masters,
	.masters_cnt	= ARRAY_SIZE(omap3xxx_dss_masters),
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430),
};

/*
 * 'dispc' class
 * display controller
 */

static struct omap_hwmod_class_sysconfig omap3xxx_dispc_sysc = {
	.rev_offs	= 0x0000,
	.sysc_offs	= 0x0010,
	.syss_offs	= 0x0014,
	.sysc_flags	= (SYSC_HAS_SIDLEMODE | SYSC_HAS_CLOCKACTIVITY |
			   SYSC_HAS_MIDLEMODE | SYSC_HAS_ENAWAKEUP |
			   SYSC_HAS_SOFTRESET | SYSC_HAS_AUTOIDLE),
	.idlemodes	= (SIDLE_FORCE | SIDLE_NO | SIDLE_SMART |
			   MSTANDBY_FORCE | MSTANDBY_NO | MSTANDBY_SMART),
	.sysc_fields	= &omap_hwmod_sysc_type1,
};

static struct omap_hwmod_class omap3xxx_dispc_hwmod_class = {
	.name = "dispc",
	.sysc = &omap3xxx_dispc_sysc,
};


static struct omap_hwmod_addr_space omap3xxx_dss_dispc_addrs[] = {
	{
		.pa_start	= 0x48050400,
		.pa_end		= 0x480507FF,
		.flags		= ADDR_TYPE_RT
	},
};

/* l4_core -> dss_dispc */
static struct omap_hwmod_ocp_if omap3xxx_l4_core__dss_dispc = {
	.master		= &omap3xxx_l4_core_hwmod,
	.slave		= &omap3xxx_dss_dispc_hwmod,
	.clk		= "dss_ick",
	.addr		= omap3xxx_dss_dispc_addrs,
	.addr_cnt	= ARRAY_SIZE(omap3xxx_dss_dispc_addrs),
	.user		= OCP_USER_MPU,
};

/* dss_dispc slave ports */
static struct omap_hwmod_ocp_if *omap3xxx_dss_dispc_slaves[] = {
	&omap3xxx_l4_core__dss_dispc,
};

static struct omap_hwmod omap3xxx_dss_dispc_hwmod = {
	.name		= "dss_dispc",
	.class		= &omap3xxx_dispc_hwmod_class,
	.main_clk	= "dss1_alwon_fck",
	.prcm		= {
		.omap2 = {
			.prcm_reg_id = 1,
			.module_bit = OMAP3430_EN_DSS1_SHIFT,
			.module_offs = OMAP3430_DSS_MOD,
			.idlest_reg_id = 1,
			.idlest_idle_bit = OMAP3430ES2_ST_DSS_IDLE_SHIFT,
		},
	},
	.slaves		= omap3xxx_dss_dispc_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap3xxx_dss_dispc_slaves),
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430),
};

/*
 * 'dsi' class
 * display serial interface controller
 */

static struct omap_hwmod_class omap3xxx_dsi_hwmod_class = {
	.name = "dsi",
};

/* dss_dsi1 */

static struct omap_hwmod_addr_space omap3xxx_dss_dsi1_addrs[] = {
	{
		.pa_start	= 0x4804FBFF ,
		.pa_end		= 0x4804FFFF,
		.flags		= ADDR_TYPE_RT
	},
};

/* l4_core -> dss_dsi1 */
static struct omap_hwmod_ocp_if omap3xxx_l4_core__dss_dsi1 = {
	.master		= &omap3xxx_l4_core_hwmod,
	.slave		= &omap3xxx_dss_dsi1_hwmod,
	.addr		= omap3xxx_dss_dsi1_addrs,
	.addr_cnt	= ARRAY_SIZE(omap3xxx_dss_dsi1_addrs),
	.user		= OCP_USER_MPU,
};

static struct omap_hwmod_addr_space omap3xxx_dss_dsi1_dma_addrs[] = {
	{
		.pa_start       = 0x4804FC00,
		.pa_end         = 0x4804FCff,
		.flags          = ADDR_TYPE_RT
	},
};

/* L3 -> dss_dsi1 */
static struct omap_hwmod_ocp_if omap3xxx_l3_main__dss_dsi1 = {
	.master         = &omap3xxx_l3_main_hwmod,
	.slave          = &omap3xxx_dss_dsi1_hwmod,
	.addr           = omap3xxx_dss_dsi1_dma_addrs,
	.addr_cnt       = ARRAY_SIZE(omap3xxx_dss_dsi1_dma_addrs),
	.user           = OCP_USER_SDMA,
};


/* dss_dsi1 slave ports */
static struct omap_hwmod_ocp_if *omap3xxx_dss_dsi1_slaves[] = {
	&omap3xxx_l4_core__dss_dsi1,
	&omap3xxx_l3_main__dss_dsi1,
};

static struct omap_hwmod omap3xxx_dss_dsi1_hwmod = {
	.name		= "dss_dsi1",
	.class		= &omap3xxx_dsi_hwmod_class,
	.main_clk	= "dss1_alwon_fck",
	.prcm		= {
		.omap2 = {
			.prcm_reg_id = 1,
			.module_bit = OMAP3430_EN_DSS1_SHIFT,
			.module_offs = OMAP3430_DSS_MOD,
			.idlest_reg_id = 1,
			.idlest_idle_bit = OMAP3430ES2_ST_DSS_IDLE_SHIFT,
		},
	},
	.slaves		= omap3xxx_dss_dsi1_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap3xxx_dss_dsi1_slaves),
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430),
};
/*
 * 'rfbi' class
 * remote frame buffer interface
 */

static struct omap_hwmod_class_sysconfig omap3xxx_rfbi_sysc = {
	.rev_offs	= 0x0000,
	.sysc_offs	= 0x0010,
	.syss_offs	= 0x0014,
	.sysc_flags	= (SYSC_HAS_SIDLEMODE | SYSC_HAS_SOFTRESET |
			   SYSC_HAS_AUTOIDLE),
	.idlemodes	= (SIDLE_FORCE | SIDLE_NO | SIDLE_SMART),
	.sysc_fields	= &omap_hwmod_sysc_type1,
};

static struct omap_hwmod_class omap3xxx_rfbi_hwmod_class = {
	.name = "rfbi",
	.sysc = &omap3xxx_rfbi_sysc,
};


static struct omap_hwmod_addr_space omap3xxx_dss_rfbi_addrs[] = {
	{
		.pa_start	= 0x48050800,
		.pa_end		= 0x48050BFF,
		.flags		= ADDR_TYPE_RT
	},
};

/* l4_core -> dss_rfbi */
static struct omap_hwmod_ocp_if omap3xxx_l4_core__dss_rfbi = {
	.master		= &omap3xxx_l4_core_hwmod,
	.slave		= &omap3xxx_dss_rfbi_hwmod,
	.clk		= "dss_ick",
	.addr		= omap3xxx_dss_rfbi_addrs,
	.addr_cnt	= ARRAY_SIZE(omap3xxx_dss_rfbi_addrs),
	.user		= OCP_USER_MPU,
};


/* dss_rfbi slave ports */
static struct omap_hwmod_ocp_if *omap3xxx_dss_rfbi_slaves[] = {
	&omap3xxx_l4_core__dss_rfbi,
};

static struct omap_hwmod omap3xxx_dss_rfbi_hwmod = {
	.name		= "dss_rfbi",
	.class		= &omap3xxx_rfbi_hwmod_class,
	.main_clk	= "dss1_alwon_fck",
	.prcm		= {
		.omap2 = {
			.prcm_reg_id = 1,
			.module_bit = OMAP3430_EN_DSS1_SHIFT,
			.module_offs = OMAP3430_DSS_MOD,
			.idlest_reg_id = 1,
			.idlest_idle_bit = OMAP3430ES2_ST_DSS_IDLE_SHIFT,
		},
	},
	.slaves		= omap3xxx_dss_rfbi_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap3xxx_dss_rfbi_slaves),
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430),
};

/*
 * 'venc' class
 * video encoder
 */

static struct omap_hwmod_class_sysconfig omap3xxx_venc_sysc = {
	.sysc_flags	= SYSS_MISSING,
	.idlemodes	= (SIDLE_FORCE | SIDLE_NO | SIDLE_SMART),
};

static struct omap_hwmod_class omap3xxx_venc_hwmod_class = {
	.name = "venc",
	.sysc = &omap3xxx_venc_sysc,
};

/* dss_venc */
static struct omap_hwmod_addr_space omap3xxx_dss_venc_addrs[] = {
	{
		.pa_start	= 0x48050C00,
		.pa_end		= 0x48050FFF,
		.flags		= ADDR_TYPE_RT
	},
};

/* l4_core -> dss_venc */
static struct omap_hwmod_ocp_if omap3xxx_l4_core__dss_venc = {
	.master		= &omap3xxx_l4_core_hwmod,
	.slave		= &omap3xxx_dss_venc_hwmod,
	.clk		= "dss_tv_fck",
	.addr		= omap3xxx_dss_venc_addrs,
	.addr_cnt	= ARRAY_SIZE(omap3xxx_dss_venc_addrs),
	.user		= OCP_USER_MPU,
};


/* dss_venc slave ports */
static struct omap_hwmod_ocp_if *omap3xxx_dss_venc_slaves[] = {
	&omap3xxx_l4_core__dss_venc,
};

static struct omap_hwmod omap3xxx_dss_venc_hwmod = {
	.name		= "dss_venc",
	.class		= &omap3xxx_venc_hwmod_class,
	.main_clk	= "dss1_alwon_fck",
	.prcm		= {
		.omap2 = {
			.prcm_reg_id = 1,
			.module_bit = OMAP3430_EN_DSS1_SHIFT,
			.module_offs = OMAP3430_DSS_MOD,
			.idlest_reg_id = 1,
			.idlest_idle_bit = OMAP3430ES2_ST_DSS_IDLE_SHIFT,
		},
	},
	.slaves		= omap3xxx_dss_venc_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap3xxx_dss_venc_slaves),
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430),
};
static __initdata struct omap_hwmod *omap3xxx_hwmods[] = {
	&omap3xxx_l3_main_hwmod,
	&omap3xxx_l4_core_hwmod,
	&omap3xxx_l4_per_hwmod,
	&omap3xxx_l4_wkup_hwmod,
	&omap3xxx_mmc1_hwmod,
	&omap3xxx_mmc2_hwmod,
	&omap3xxx_mmc3_hwmod,
	&omap3xxx_mpu_hwmod,
	&omap3xxx_ispmmu_hwmod,
	&omap3xxx_iva_hwmod,
	&omap3xxx_gpu_hwmod,
	&omap3xxx_i2c1_hwmod,
	&omap3xxx_i2c2_hwmod,
	&omap3xxx_i2c3_hwmod,
	&omap3xxx_uart1_hwmod,
	&omap3xxx_uart2_hwmod,
	&omap3xxx_uart3_hwmod,
	&omap3xxx_usbhsotg_hwmod,
	&omap3xxx_gpio1_hwmod,
	&omap3xxx_gpio2_hwmod,
	&omap3xxx_gpio3_hwmod,
	&omap3xxx_gpio4_hwmod,
	&omap3xxx_gpio5_hwmod,
	&omap3xxx_gpio6_hwmod,
	&omap3xxx_wd_timer2_hwmod,
	&omap3xxx_dma_system_hwmod,
	&omap34xx_mcspi1,
	&omap34xx_mcspi2,
	&omap34xx_mcspi3,
	&omap34xx_mcspi4,
	&omap3xxx_timer1_hwmod,
	&omap3xxx_timer2_hwmod,
	&omap3xxx_timer3_hwmod,
	&omap3xxx_timer4_hwmod,
	&omap3xxx_timer5_hwmod,
	&omap3xxx_timer6_hwmod,
	&omap3xxx_timer7_hwmod,
	&omap3xxx_timer8_hwmod,
	&omap3xxx_timer9_hwmod,
	&omap3xxx_timer10_hwmod,
	&omap3xxx_timer11_hwmod,
	&omap34xx_sr1_hwmod,
	&omap34xx_sr2_hwmod,
	&omap36xx_sr1_hwmod,
	&omap36xx_sr2_hwmod,
	&omap3xxx_mcbsp1_hwmod,
	&omap3xxx_mcbsp2_hwmod,
	&omap3xxx_mcbsp3_hwmod,
	&omap3xxx_mcbsp4_hwmod,
	&omap3xxx_mcbsp5_hwmod,
	&omap3xxx_mcbsp2_sidetone_hwmod,
	&omap3xxx_mcbsp3_sidetone_hwmod,
	&omap3xxx_dss_hwmod,
	&omap3xxx_dss_dispc_hwmod,
	&omap3xxx_dss_dsi1_hwmod,
	&omap3xxx_dss_rfbi_hwmod,
	&omap3xxx_dss_venc_hwmod,
	NULL,
};

int __init omap3xxx_hwmod_init(void)
{
	return omap_hwmod_init(omap3xxx_hwmods);
}
