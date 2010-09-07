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
#include <plat/gpio.h>

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
static struct omap_hwmod omap2420_gpio1_hwmod;
static struct omap_hwmod omap2420_gpio2_hwmod;
static struct omap_hwmod omap2420_gpio3_hwmod;
static struct omap_hwmod omap2420_gpio4_hwmod;
static struct omap_hwmod omap2420_dma_system_hwmod;

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
static struct omap_hwmod omap2420_wd_timer2_hwmod;

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

/* L4 WKUP -> GPIO1 interface */
static struct omap_hwmod_addr_space omap2420_gpio1_addr_space[] = {
	{
		.pa_start	= 0x48018000,
		.pa_end		= 0x480181ff,
		.flags		= ADDR_TYPE_RT
	},
};

static struct omap_hwmod_ocp_if omap2420_l4_wkup__gpio1 = {
	.master		= &omap2420_l4_wkup_hwmod,
	.slave		= &omap2420_gpio1_hwmod,
	.clk		= "gpios_ick",
	.addr		= omap2420_gpio1_addr_space,
	.addr_cnt	= ARRAY_SIZE(omap2420_gpio1_addr_space),
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* L4 WKUP -> GPIO2 interface */
static struct omap_hwmod_addr_space omap2420_gpio2_addr_space[] = {
	{
		.pa_start	= 0x4801a000,
		.pa_end		= 0x4801a1ff,
		.flags		= ADDR_TYPE_RT
	},
};

static struct omap_hwmod_ocp_if omap2420_l4_wkup__gpio2 = {
	.master		= &omap2420_l4_wkup_hwmod,
	.slave		= &omap2420_gpio2_hwmod,
	.clk		= "gpios_ick",
	.addr		= omap2420_gpio2_addr_space,
	.addr_cnt	= ARRAY_SIZE(omap2420_gpio2_addr_space),
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* L4 WKUP -> GPIO3 interface */
static struct omap_hwmod_addr_space omap2420_gpio3_addr_space[] = {
	{
		.pa_start	= 0x4801c000,
		.pa_end		= 0x4801c1ff,
		.flags		= ADDR_TYPE_RT
	},
};

static struct omap_hwmod_ocp_if omap2420_l4_wkup__gpio3 = {
	.master		= &omap2420_l4_wkup_hwmod,
	.slave		= &omap2420_gpio3_hwmod,
	.clk		= "gpios_ick",
	.addr		= omap2420_gpio3_addr_space,
	.addr_cnt	= ARRAY_SIZE(omap2420_gpio3_addr_space),
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* L4 WKUP -> GPIO4 interface */
static struct omap_hwmod_addr_space omap2420_gpio4_addr_space[] = {
	{
		.pa_start	= 0x4801e000,
		.pa_end		= 0x4801e1ff,
		.flags		= ADDR_TYPE_RT
	},
};

static struct omap_hwmod_ocp_if omap2420_l4_wkup__gpio4 = {
	.master		= &omap2420_l4_wkup_hwmod,
	.slave		= &omap2420_gpio4_hwmod,
	.clk		= "gpios_ick",
	.addr		= omap2420_gpio4_addr_space,
	.addr_cnt	= ARRAY_SIZE(omap2420_gpio4_addr_space),
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
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

/* WDTIMER2 <- L4_WKUP interface */
static struct omap_hwmod_addr_space omap2420_wd_timer2_addrs[] = {
	{
		.pa_start	= 0x48022000,
		.pa_end		= 0x48022000 + SZ_4K - 1,
		.flags		= ADDR_TYPE_RT
	},
};

static struct omap_hwmod_ocp_if omap2420_l4_wkup__wd_timer2 = {
	.master		= &omap2420_l4_wkup_hwmod,
	.slave		= &omap2420_wd_timer2_hwmod,
	.clk		= "mpu_wdt_ick",
	.addr		= omap2420_wd_timer2_addrs,
	.addr_cnt	= ARRAY_SIZE(omap2420_wd_timer2_addrs),
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* WDTIMER common */

static struct omap_hwmod_class_sysconfig omap2420_wd_timer_sysc = {
	.rev_offs	= 0x0000,
	.sysc_offs	= 0x0010,
	.syss_offs	= 0x0014,
	.sysc_flags	= (SYSC_HAS_EMUFREE | SYSC_HAS_SOFTRESET |
			   SYSC_HAS_AUTOIDLE),
	.sysc_fields    = &omap_hwmod_sysc_type1,
};

static struct omap_hwmod_class omap2420_wd_timer_hwmod_class = {
	.name = "wd_timer",
	.sysc = &omap2420_wd_timer_sysc,
};

/* WDTIMER2 */
static struct omap_hwmod_ocp_if *omap2420_wd_timer2_slaves[] = {
	&omap2420_l4_wkup__wd_timer2,
};

static struct omap_hwmod omap2420_wd_timer2_hwmod = {
	.name		= "wd_timer2",
	.class		= &omap2420_wd_timer_hwmod_class,
	.main_clk	= "mpu_wdt_fck",
	.prcm		= {
		.omap2 = {
			.prcm_reg_id = 1,
			.module_bit = OMAP24XX_EN_WDT2_SHIFT,
			.module_offs = WKUP_MOD,
			.idlest_reg_id = 1,
			.idlest_idle_bit = OMAP24XX_EN_WDT2_SHIFT,
		},
	},
	.slaves		= omap2420_wd_timer2_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap2420_wd_timer2_slaves),
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP2420),
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

/* GPIO common */

static struct omap_gpio_dev_attr gpio_dev_attr = {
	.bank_width = 32,
	.dbck_flag = false,
	/*
	 * off_mode is supported by GPIO, but it is not
	 * supported by software due to leakage current problem.
	 * Hence making off_mode_support flag as false
	 */
	.off_mode_support = false,
};

static struct omap_hwmod_class_sysconfig omap242x_gpio_sysc = {
	.rev_offs	= 0x0000,
	.sysc_offs	= 0x0010,
	.syss_offs	= 0x0014,
	.sysc_flags	= (SYSC_HAS_ENAWAKEUP | SYSC_HAS_SIDLEMODE |
			   SYSC_HAS_SOFTRESET | SYSC_HAS_AUTOIDLE),
	.idlemodes	= (SIDLE_FORCE | SIDLE_NO | SIDLE_SMART),
	.sysc_fields    = &omap_hwmod_sysc_type1,
};

static struct omap_hwmod_class omap242x_gpio_hwmod_class = {
	.name = "gpio",
	.sysc = &omap242x_gpio_sysc,
	.rev = 0,
};

/* GPIO1 */

static struct omap_hwmod_irq_info omap242x_gpio1_irqs[] = {
	{ .name = "gpio_mpu_irq", .irq = INT_24XX_GPIO_BANK1 },
};

static struct omap_hwmod_ocp_if *omap2420_gpio1_slaves[] = {
	&omap2420_l4_wkup__gpio1,
};

static struct omap_hwmod omap2420_gpio1_hwmod = {
	.name		= "gpio1",
	.mpu_irqs	= omap242x_gpio1_irqs,
	.mpu_irqs_cnt	= ARRAY_SIZE(omap242x_gpio1_irqs),
	.main_clk	= "gpios_fck",
	.prcm		= {
		.omap2 = {
			.prcm_reg_id = 1,
			.module_bit = OMAP24XX_EN_GPIOS_SHIFT,
			.module_offs = WKUP_MOD,
			.idlest_reg_id = 1,
			.idlest_idle_bit = OMAP24XX_EN_GPIOS_SHIFT,
		},
	},
	.slaves		= omap2420_gpio1_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap2420_gpio1_slaves),
	.class		= &omap242x_gpio_hwmod_class,
	.dev_attr	= &gpio_dev_attr,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP2420),
};

/* GPIO2 */

static struct omap_hwmod_irq_info omap242x_gpio2_irqs[] = {
	{ .name = "gpio_mpu_irq", .irq = INT_24XX_GPIO_BANK2 },
};

static struct omap_hwmod_ocp_if *omap2420_gpio2_slaves[] = {
	&omap2420_l4_wkup__gpio2,
};

static struct omap_hwmod omap2420_gpio2_hwmod = {
	.name		= "gpio2",
	.mpu_irqs	= omap242x_gpio2_irqs,
	.mpu_irqs_cnt	= ARRAY_SIZE(omap242x_gpio2_irqs),
	.main_clk	= "gpios_fck",
	.prcm		= {
		.omap2 = {
			.prcm_reg_id = 1,
			.module_bit = OMAP24XX_EN_GPIOS_SHIFT,
			.module_offs = WKUP_MOD,
			.idlest_reg_id = 1,
			.idlest_idle_bit = OMAP24XX_EN_GPIOS_SHIFT,
		},
	},
	.slaves		= omap2420_gpio2_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap2420_gpio2_slaves),
	.class		= &omap242x_gpio_hwmod_class,
	.dev_attr	= &gpio_dev_attr,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP2420),
};

/* GPIO3 */

static struct omap_hwmod_irq_info omap242x_gpio3_irqs[] = {
	{ .name = "gpio_mpu_irq", .irq = INT_24XX_GPIO_BANK3 },
};

static struct omap_hwmod_ocp_if *omap2420_gpio3_slaves[] = {
	&omap2420_l4_wkup__gpio3,
};

static struct omap_hwmod omap2420_gpio3_hwmod = {
	.name		= "gpio3",
	.mpu_irqs	= omap242x_gpio3_irqs,
	.mpu_irqs_cnt	= ARRAY_SIZE(omap242x_gpio3_irqs),
	.main_clk	= "gpios_fck",
	.prcm		= {
		.omap2 = {
			.prcm_reg_id = 1,
			.module_bit = OMAP24XX_EN_GPIOS_SHIFT,
			.module_offs = WKUP_MOD,
			.idlest_reg_id = 1,
			.idlest_idle_bit = OMAP24XX_EN_GPIOS_SHIFT,
		},
	},
	.slaves		= omap2420_gpio3_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap2420_gpio3_slaves),
	.class		= &omap242x_gpio_hwmod_class,
	.dev_attr	= &gpio_dev_attr,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP2420),
};

/* GPIO4 */

static struct omap_hwmod_irq_info omap242x_gpio4_irqs[] = {
	{ .name = "gpio_mpu_irq", .irq = INT_24XX_GPIO_BANK4 },
};

static struct omap_hwmod_ocp_if *omap2420_gpio4_slaves[] = {
	&omap2420_l4_wkup__gpio4,
};

static struct omap_hwmod omap2420_gpio4_hwmod = {
	.name		= "gpio4",
	.mpu_irqs	= omap242x_gpio4_irqs,
	.mpu_irqs_cnt	= ARRAY_SIZE(omap242x_gpio4_irqs),
	.main_clk	= "gpios_fck",
	.prcm		= {
		.omap2 = {
			.prcm_reg_id = 1,
			.module_bit = OMAP24XX_EN_GPIOS_SHIFT,
			.module_offs = WKUP_MOD,
			.idlest_reg_id = 1,
			.idlest_idle_bit = OMAP24XX_EN_GPIOS_SHIFT,
		},
	},
	.slaves		= omap2420_gpio4_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap2420_gpio4_slaves),
	.class		= &omap242x_gpio_hwmod_class,
	.dev_attr	= &gpio_dev_attr,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP2420),
};

/* system dma */
static struct omap_hwmod_class_sysconfig omap2420_dma_sysc = {
	.rev_offs	= 0x0000,
	.sysc_offs	= 0x002c,
	.syss_offs	= 0x0028,
	.sysc_flags	= (SYSC_HAS_SIDLEMODE | SYSC_HAS_SOFTRESET |
			   SYSC_HAS_MIDLEMODE | SYSC_HAS_CLOCKACTIVITY |
			   SYSC_HAS_EMUFREE | SYSC_HAS_AUTOIDLE),
	.idlemodes	= (SIDLE_FORCE | SIDLE_NO | SIDLE_SMART |
			   MSTANDBY_FORCE | MSTANDBY_NO | MSTANDBY_SMART),
	.sysc_fields	= &omap_hwmod_sysc_type1,
};

static struct omap_hwmod_class omap2420_dma_hwmod_class = {
	.name = "dma",
	.sysc = &omap2420_dma_sysc,
};

/* dma attributes */
static struct omap_dma_dev_attr dma_dev_attr = {
	.dma_dev_attr = DMA_LINKED_LCH | GLOBAL_PRIORITY |
				IS_CSSA_32 | IS_CDSA_32,
	.dma_lch_count = OMAP_DMA4_LOGICAL_DMA_CH_COUNT,
};

static struct omap_hwmod_irq_info omap2420_dma_system_irqs[] = {
	{ .name = "dma_0", .irq = INT_24XX_SDMA_IRQ0 },
	{ .name = "dma_1", .irq = INT_24XX_SDMA_IRQ1 },
	{ .name = "dma_2", .irq = INT_24XX_SDMA_IRQ2 },
	{ .name = "dma_3", .irq = INT_24XX_SDMA_IRQ3 },
};

static struct omap_hwmod_addr_space omap2420_dma_system_addrs[] = {
	{
		.pa_start	= 0x48056000,
		.pa_end		= 0x4a0560ff,
		.flags		= ADDR_TYPE_RT
	},
};

/* dma_system -> L3 */
static struct omap_hwmod_ocp_if omap2420_dma_system__l3 = {
	.master		= &omap2420_dma_system_hwmod,
	.slave		= &omap2420_l3_main_hwmod,
	.clk		= "l3_div_ck",
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* dma_system master ports */
static struct omap_hwmod_ocp_if *omap2420_dma_system_masters[] = {
	&omap2420_dma_system__l3,
};

/* l4_cfg -> dma_system */
static struct omap_hwmod_ocp_if omap2420_l4_core__dma_system = {
	.master		= &omap2420_l4_core_hwmod,
	.slave		= &omap2420_dma_system_hwmod,
	.clk		= "l4_div_ck",
	.addr		= omap2420_dma_system_addrs,
	.addr_cnt	= ARRAY_SIZE(omap2420_dma_system_addrs),
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* dma_system slave ports */
static struct omap_hwmod_ocp_if *omap2420_dma_system_slaves[] = {
	&omap2420_l4_core__dma_system,
};

static struct omap_hwmod omap2420_dma_system_hwmod = {
	.name		= "dma",
	.class		= &omap2420_dma_hwmod_class,
	.mpu_irqs	= omap2420_dma_system_irqs,
	.mpu_irqs_cnt	= ARRAY_SIZE(omap2420_dma_system_irqs),
	.main_clk	= "l3_div_ck",
	.prcm = {
		.omap2 = {
			/* .clkctrl_reg = NULL, */
		},
	},
	.slaves		= omap2420_dma_system_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap2420_dma_system_slaves),
	.masters	= omap2420_dma_system_masters,
	.masters_cnt	= ARRAY_SIZE(omap2420_dma_system_masters),
	.dev_attr	= &dma_dev_attr,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP2420),
};

static struct omap_hwmod omap2420_mcbsp1_hwmod;
static struct omap_hwmod omap2420_mcbsp2_hwmod;

static struct omap_hwmod_class omap2420_mcbsp_hwmod_class = {
	.name = "mcbsp",
};

/* mcbsp1 */
static struct omap_hwmod_irq_info omap2420_mcbsp1_irqs[] = {
	{ .name = "tx", .irq = 59 },
	{ .name = "rx", .irq = 60 },
};

static struct omap_hwmod_dma_info omap2420_mcbsp1_sdma_chs[] = {
	{ .name = "rx", .dma_req = 31 },
	{ .name = "tx", .dma_req = 30 },
};

static struct omap_hwmod_addr_space omap2420_mcbsp1_addrs[] = {
	{
		.pa_start	= 0x48074000,
		.pa_end		= 0x480740ff,
		.flags		= ADDR_TYPE_RT
	},
};

/* l4_core -> mcbsp1 */
static struct omap_hwmod_ocp_if omap2420_l4_core__mcbsp1 = {
	.master		= &omap2420_l4_core_hwmod,
	.slave		= &omap2420_mcbsp1_hwmod,
	.clk		= "mcbsp1_ick",
	.addr		= omap2420_mcbsp1_addrs,
	.addr_cnt	= ARRAY_SIZE(omap2420_mcbsp1_addrs),
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* mcbsp1 slave ports */
static struct omap_hwmod_ocp_if *omap2420_mcbsp1_slaves[] = {
	&omap2420_l4_core__mcbsp1,
};

static struct omap_hwmod omap2420_mcbsp1_hwmod = {
	.name		= "mcbsp1",
	.class		= &omap2420_mcbsp_hwmod_class,
	.mpu_irqs	= omap2420_mcbsp1_irqs,
	.mpu_irqs_cnt	= ARRAY_SIZE(omap2420_mcbsp1_irqs),
	.sdma_reqs	= omap2420_mcbsp1_sdma_chs,
	.sdma_reqs_cnt	= ARRAY_SIZE(omap2420_mcbsp1_sdma_chs),
	.main_clk	= "mcbsp1_fck",
	.prcm		= {
		.omap2 = {
			.prcm_reg_id = 1,
			.module_bit = OMAP24XX_EN_MCBSP1_SHIFT,
			.module_offs = CORE_MOD,
			.idlest_reg_id = 1,
			.idlest_idle_bit = OMAP24XX_EN_MCBSP1_SHIFT,
		},
	},
	.slaves		= omap2420_mcbsp1_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap2420_mcbsp1_slaves),
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP2420),
};

/* mcbsp2 */
static struct omap_hwmod_irq_info omap2420_mcbsp2_irqs[] = {
	{ .name = "tx", .irq = 62 },
	{ .name = "rx", .irq = 63 },
};

static struct omap_hwmod_dma_info omap2420_mcbsp2_sdma_chs[] = {
	{ .name = "rx", .dma_req = 33 },
	{ .name = "tx", .dma_req = 32 },
};

static struct omap_hwmod_addr_space omap2420_mcbsp2_addrs[] = {
	{
		.pa_start	= 0x48076000,
		.pa_end		= 0x480760ff,
		.flags		= ADDR_TYPE_RT
	},
};

/* l4_core -> mcbsp2 */
static struct omap_hwmod_ocp_if omap2420_l4_core__mcbsp2 = {
	.master		= &omap2420_l4_core_hwmod,
	.slave		= &omap2420_mcbsp2_hwmod,
	.clk		= "mcbsp2_ick",
	.addr		= omap2420_mcbsp2_addrs,
	.addr_cnt	= ARRAY_SIZE(omap2420_mcbsp2_addrs),
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* mcbsp2 slave ports */
static struct omap_hwmod_ocp_if *omap2420_mcbsp2_slaves[] = {
	&omap2420_l4_core__mcbsp2,
};

static struct omap_hwmod omap2420_mcbsp2_hwmod = {
	.name		= "mcbsp2",
	.class		= &omap2420_mcbsp_hwmod_class,
	.mpu_irqs	= omap2420_mcbsp2_irqs,
	.mpu_irqs_cnt	= ARRAY_SIZE(omap2420_mcbsp2_irqs),
	.sdma_reqs	= omap2420_mcbsp2_sdma_chs,
	.sdma_reqs_cnt	= ARRAY_SIZE(omap2420_mcbsp2_sdma_chs),
	.main_clk	= "mcbsp2_fck",
	.prcm		= {
		.omap2 = {
			.prcm_reg_id = 1,
			.module_bit = OMAP24XX_EN_MCBSP2_SHIFT,
			.module_offs = CORE_MOD,
			.idlest_reg_id = 1,
			.idlest_idle_bit = OMAP24XX_EN_MCBSP2_SHIFT,
		},
	},
	.slaves		= omap2420_mcbsp2_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap2420_mcbsp2_slaves),
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP2420),
};

static __initdata struct omap_hwmod *omap2420_hwmods[] = {
	&omap2420_l3_main_hwmod,
	&omap2420_l4_core_hwmod,
	&omap2420_l4_wkup_hwmod,
	&omap2420_mpu_hwmod,
	&omap2420_iva_hwmod,
	&omap2420_gpio1_hwmod,
	&omap2420_gpio2_hwmod,
	&omap2420_gpio3_hwmod,
	&omap2420_gpio4_hwmod,
	&omap2420_wd_timer2_hwmod,
	&omap2420_dma_system_hwmod,
	&omap2420_mcbsp1_hwmod,
	&omap2420_mcbsp2_hwmod,
	NULL,
};

int __init omap2420_hwmod_init(void)
{
	return omap_hwmod_init(omap2420_hwmods);
}
