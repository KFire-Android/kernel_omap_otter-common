/*
 * omap_hwmod_2420_data.c - hardware modules present on the OMAP2420 chips
 *
 * Copyright (C) 2009-2010 Nokia Corporation
 * Paul Walmsley
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * XXX handle crossbar/shared link difference for L3?
 * XXX these should be marked initdata for multi-OMAP kernels
 */
#include <plat/omap_hwmod.h>
#include <mach/irqs.h>
#include <plat/cpu.h>
#include <plat/dma.h>
#include <plat/i2c.h>
#include <plat/omap24xx.h>

#include "omap_hwmod_common_data.h"

#include "cm-regbits-24xx.h"
#include "prm-regbits-24xx.h"

/*
 * OMAP2420 hardware module integration data
 *
 * ALl of the data in this section should be autogeneratable from the
 * TI hardware database or other technical documentation.  Data that
 * is driver-specific or driver-kernel integration-specific belongs
 * elsewhere.
 */

static struct omap_hwmod omap2420_mpu_hwmod;
static struct omap_hwmod omap2420_iva_hwmod;
static struct omap_hwmod omap2420_l3_main_hwmod;
static struct omap_hwmod omap2420_l4_core_hwmod;

/* L3 -> L4_CORE interface */
static struct omap_hwmod_ocp_if omap2420_l3_main__l4_core = {
	.master	= &omap2420_l3_main_hwmod,
	.slave	= &omap2420_l4_core_hwmod,
	.user	= OCP_USER_MPU | OCP_USER_SDMA,
};

/* MPU -> L3 interface */
static struct omap_hwmod_ocp_if omap2420_mpu__l3_main = {
	.master = &omap2420_mpu_hwmod,
	.slave	= &omap2420_l3_main_hwmod,
	.user	= OCP_USER_MPU,
};

/* Slave interfaces on the L3 interconnect */
static struct omap_hwmod_ocp_if *omap2420_l3_main_slaves[] = {
	&omap2420_mpu__l3_main,
};

/* Master interfaces on the L3 interconnect */
static struct omap_hwmod_ocp_if *omap2420_l3_main_masters[] = {
	&omap2420_l3_main__l4_core,
};

/* L3 */
static struct omap_hwmod omap2420_l3_main_hwmod = {
	.name		= "l3_main",
	.class		= &l3_hwmod_class,
	.masters	= omap2420_l3_main_masters,
	.masters_cnt	= ARRAY_SIZE(omap2420_l3_main_masters),
	.slaves		= omap2420_l3_main_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap2420_l3_main_slaves),
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP2420),
	.flags		= HWMOD_NO_IDLEST,
};

static struct omap_hwmod omap2420_l4_wkup_hwmod;
static struct omap_hwmod omap2420_i2c1_hwmod;
static struct omap_hwmod omap2420_i2c2_hwmod;

/* L4_CORE -> L4_WKUP interface */
static struct omap_hwmod_ocp_if omap2420_l4_core__l4_wkup = {
	.master	= &omap2420_l4_core_hwmod,
	.slave	= &omap2420_l4_wkup_hwmod,
	.user	= OCP_USER_MPU | OCP_USER_SDMA,
};

/* I2C IP block address space length (in bytes) */
#define OMAP2_I2C_AS_LEN		128

/* L4 CORE -> I2C1 interface */
static struct omap_hwmod_addr_space omap2420_i2c1_addr_space[] = {
	{
		.pa_start	= OMAP24XX_I2C1_BASE,
		.pa_end		= OMAP24XX_I2C1_BASE + OMAP2_I2C_AS_LEN - 1,
		.flags		= ADDR_TYPE_RT,
	},
};

static struct omap_hwmod_ocp_if omap2420_l4_core__i2c1 = {
	.master		= &omap2420_l4_core_hwmod,
	.slave		= &omap2420_i2c1_hwmod,
	.clk		= "i2c1_ick",
	.addr		= omap2420_i2c1_addr_space,
	.addr_cnt	= ARRAY_SIZE(omap2420_i2c1_addr_space),
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* L4 CORE -> I2C2 interface */
static struct omap_hwmod_addr_space omap2420_i2c2_addr_space[] = {
	{
		.pa_start	= OMAP24XX_I2C2_BASE,
		.pa_end		= OMAP24XX_I2C2_BASE + OMAP2_I2C_AS_LEN - 1,
		.flags		= ADDR_TYPE_RT,
	},
};

static struct omap_hwmod_ocp_if omap2420_l4_core__i2c2 = {
	.master		= &omap2420_l4_core_hwmod,
	.slave		= &omap2420_i2c2_hwmod,
	.clk		= "i2c2_ick",
	.addr		= omap2420_i2c2_addr_space,
	.addr_cnt	= ARRAY_SIZE(omap2420_i2c2_addr_space),
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* Slave interfaces on the L4_CORE interconnect */
static struct omap_hwmod_ocp_if *omap2420_l4_core_slaves[] = {
	&omap2420_l3_main__l4_core,
};

/* Master interfaces on the L4_CORE interconnect */
static struct omap_hwmod_ocp_if *omap2420_l4_core_masters[] = {
	&omap2420_l4_core__l4_wkup,
	&omap2420_l4_core__i2c1,
	&omap2420_l4_core__i2c2
};

/* L4 CORE */
static struct omap_hwmod omap2420_l4_core_hwmod = {
	.name		= "l4_core",
	.class		= &l4_hwmod_class,
	.masters	= omap2420_l4_core_masters,
	.masters_cnt	= ARRAY_SIZE(omap2420_l4_core_masters),
	.slaves		= omap2420_l4_core_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap2420_l4_core_slaves),
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP2420),
	.flags		= HWMOD_NO_IDLEST,
};

/* Slave interfaces on the L4_WKUP interconnect */
static struct omap_hwmod_ocp_if *omap2420_l4_wkup_slaves[] = {
	&omap2420_l4_core__l4_wkup,
};

/* Master interfaces on the L4_WKUP interconnect */
static struct omap_hwmod_ocp_if *omap2420_l4_wkup_masters[] = {
};

/* L4 WKUP */
static struct omap_hwmod omap2420_l4_wkup_hwmod = {
	.name		= "l4_wkup",
	.class		= &l4_hwmod_class,
	.masters	= omap2420_l4_wkup_masters,
	.masters_cnt	= ARRAY_SIZE(omap2420_l4_wkup_masters),
	.slaves		= omap2420_l4_wkup_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap2420_l4_wkup_slaves),
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP2420),
	.flags		= HWMOD_NO_IDLEST,
};

/* Master interfaces on the MPU device */
static struct omap_hwmod_ocp_if *omap2420_mpu_masters[] = {
	&omap2420_mpu__l3_main,
};

/* MPU */
static struct omap_hwmod omap2420_mpu_hwmod = {
	.name		= "mpu",
	.class		= &mpu_hwmod_class,
	.main_clk	= "mpu_ck",
	.masters	= omap2420_mpu_masters,
	.masters_cnt	= ARRAY_SIZE(omap2420_mpu_masters),
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP2420),
};

/*
 * IVA1 interface data
 */

/* IVA <- L3 interface */
static struct omap_hwmod_ocp_if omap2420_l3__iva = {
	.master		= &omap2420_l3_main_hwmod,
	.slave		= &omap2420_iva_hwmod,
	.clk		= "iva1_ifck",
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

static struct omap_hwmod_ocp_if *omap2420_iva_masters[] = {
	&omap2420_l3__iva,
};

/*
 * IVA2 (IVA2)
 */

static struct omap_hwmod omap2420_iva_hwmod = {
	.name		= "iva",
	.class		= &iva_hwmod_class,
	.masters	= omap2420_iva_masters,
	.masters_cnt	= ARRAY_SIZE(omap2420_iva_masters),
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP2420)
};

/* I2C common */
static struct omap_hwmod_class_sysconfig i2c_sysc = {
	.rev_offs	= 0x00,
	.sysc_offs	= 0x20,
	.syss_offs	= 0x10,
	.sysc_flags	= SYSC_HAS_SOFTRESET,
	.sysc_fields	= &omap_hwmod_sysc_type1,
};

static struct omap_hwmod_class i2c_class = {
	.name		= "i2c",
	.sysc		= &i2c_sysc,
};

static struct omap_i2c_dev_attr i2c_dev_attr;

/* I2C1 */

static struct omap_hwmod_irq_info i2c1_mpu_irqs[] = {
	{ .irq = INT_24XX_I2C1_IRQ, },
};

static struct omap_hwmod_dma_info i2c1_sdma_reqs[] = {
	{ .name = "tx", .dma_req = OMAP24XX_DMA_I2C1_TX },
	{ .name = "rx", .dma_req = OMAP24XX_DMA_I2C1_RX },
};

static struct omap_hwmod_ocp_if *omap2420_i2c1_slaves[] = {
	&omap2420_l4_core__i2c1,
};

static struct omap_hwmod omap2420_i2c1_hwmod = {
	.name		= "i2c1",
	.mpu_irqs	= i2c1_mpu_irqs,
	.mpu_irqs_cnt	= ARRAY_SIZE(i2c1_mpu_irqs),
	.sdma_reqs	= i2c1_sdma_reqs,
	.sdma_reqs_cnt	= ARRAY_SIZE(i2c1_sdma_reqs),
	.main_clk	= "i2c1_fck",
	.prcm		= {
		.omap2 = {
			.prcm_reg_id = 1,
			.module_bit = OMAP2420_EN_I2C1_SHIFT,
		},
	},
	.slaves		= omap2420_i2c1_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap2420_i2c1_slaves),
	.class		= &i2c_class,
	.dev_attr	= &i2c_dev_attr,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP2420),
};

/* I2C2 */

static struct omap_hwmod_irq_info i2c2_mpu_irqs[] = {
	{ .irq = INT_24XX_I2C2_IRQ, },
};

static struct omap_hwmod_dma_info i2c2_sdma_reqs[] = {
	{ .name = "tx", .dma_req = OMAP24XX_DMA_I2C2_TX },
	{ .name = "rx", .dma_req = OMAP24XX_DMA_I2C2_RX },
};

static struct omap_hwmod_ocp_if *omap2420_i2c2_slaves[] = {
	&omap2420_l4_core__i2c2,
};

static struct omap_hwmod omap2420_i2c2_hwmod = {
	.name		= "i2c2",
	.mpu_irqs	= i2c2_mpu_irqs,
	.mpu_irqs_cnt	= ARRAY_SIZE(i2c2_mpu_irqs),
	.sdma_reqs	= i2c2_sdma_reqs,
	.sdma_reqs_cnt	= ARRAY_SIZE(i2c2_sdma_reqs),
	.main_clk	= "i2c2_fck",
	.prcm		= {
		.omap2 = {
			.prcm_reg_id = 1,
			.module_bit = OMAP2420_EN_I2C2_SHIFT,
		},
	},
	.slaves		= omap2420_i2c2_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap2420_i2c2_slaves),
	.class		= &i2c_class,
	.dev_attr	= &i2c_dev_attr,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP2420),
};

static __initdata struct omap_hwmod *omap2420_hwmods[] = {
	&omap2420_l3_main_hwmod,
	&omap2420_l4_core_hwmod,
	&omap2420_l4_wkup_hwmod,
	&omap2420_mpu_hwmod,
	&omap2420_iva_hwmod,
	NULL,
};

int __init omap2420_hwmod_init(void)
{
	return omap_hwmod_init(omap2420_hwmods);
}
