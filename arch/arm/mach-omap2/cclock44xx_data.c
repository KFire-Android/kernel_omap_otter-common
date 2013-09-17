/*
 * OMAP4 Clock data
 *
 * Copyright (C) 2009-2012 Texas Instruments, Inc.
 * Copyright (C) 2009-2010 Nokia Corporation
 *
 * Paul Walmsley (paul@pwsan.com)
 * Rajendra Nayak (rnayak@ti.com)
 * Benoit Cousson (b-cousson@ti.com)
 * Mike Turquette (mturquette@ti.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * XXX Some of the ES1 clocks have been removed/changed; once support
 * is added for discriminating clocks by ES level, these should be added back
 * in.
 */

#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/clk-private.h>
#include <linux/clkdev.h>
#include <linux/io.h>

#include "soc.h"
#include "iomap.h"
#include "clock.h"
#include "clock44xx.h"
#include "cm1_44xx.h"
#include "cm2_44xx.h"
#include "cm-regbits-44xx.h"
#include "prm44xx.h"
#include "prm-regbits-44xx.h"
#include "control.h"
#include "scrm44xx.h"

/* OMAP4 modulemode control */
#define OMAP4430_MODULEMODE_HWCTRL_SHIFT		0
#define OMAP4430_MODULEMODE_SWCTRL_SHIFT		1

/*
 * OMAP4 ABE DPLL default frequency. In OMAP4460 TRM version V, section
 * "3.6.3.2.3 CM1_ABE Clock Generator" states that the "DPLL_ABE_X2_CLK
 * must be set to 196.608 MHz" and hence, the DPLL locked frequency is
 * half of this value.
 */
#define OMAP4_DPLL_ABE_DEFFREQ				98304000

/*
 * OMAP4450 TRM Rev X, section "3.6.3.9.5 DPLL_USB Preferred Settings"
 * states it must be at 960MHz
 */
#define OMAP4_DPLL_USB_DEFFREQ				960000000

/*
 * OMAP4 IVA DPLL frequency settings. The below values are defined
 * based on the OPP100 values according to OMAP4430 ES2.x Public TRM
 * version AN, section "3.6.3.8.7 DPLL_IVA Preferred Settings". The
 * DPLL locked frequency is 1862.4 MHz (value for DPLL_IVA_X2_CLK),
 * so the DPLL_IVA_DEFFREQ is half of this value.
 */
#define OMAP4_DPLL_IVA_DEFFREQ				931200000
#define OMAP4_DSP_ROOT_CLK_NOMFREQ			465600000
#define OMAP4_IVAHD_ROOT_CLK_NOMFREQ			266100000

/* Root clocks */

DEFINE_CLK_FIXED_RATE(extalt_clkin_ck, CLK_IS_ROOT, 59000000, 0x0);

DEFINE_CLK_FIXED_RATE(pad_clks_src_ck, CLK_IS_ROOT, 12000000, 0x0);

DEFINE_CLK_GATE(pad_clks_ck, "pad_clks_src_ck", &pad_clks_src_ck, 0x0,
		OMAP4430_CM_CLKSEL_ABE, OMAP4430_PAD_CLKS_GATE_SHIFT,
		0x0, NULL);

DEFINE_CLK_FIXED_RATE(pad_slimbus_core_clks_ck, CLK_IS_ROOT, 12000000, 0x0);

DEFINE_CLK_FIXED_RATE(secure_32k_clk_src_ck, CLK_IS_ROOT, 32768, 0x0);

DEFINE_CLK_FIXED_RATE(slimbus_src_clk, CLK_IS_ROOT, 12000000, 0x0);

DEFINE_CLK_GATE(slimbus_clk, "slimbus_src_clk", &slimbus_src_clk, 0x0,
		OMAP4430_CM_CLKSEL_ABE, OMAP4430_SLIMBUS_CLK_GATE_SHIFT,
		0x0, NULL);

DEFINE_CLK_FIXED_RATE(sys_32k_ck, CLK_IS_ROOT, 32768, 0x0);

DEFINE_CLK_FIXED_RATE(virt_12000000_ck, CLK_IS_ROOT, 12000000, 0x0);

DEFINE_CLK_FIXED_RATE(virt_13000000_ck, CLK_IS_ROOT, 13000000, 0x0);

DEFINE_CLK_FIXED_RATE(virt_16800000_ck, CLK_IS_ROOT, 16800000, 0x0);

DEFINE_CLK_FIXED_RATE(virt_19200000_ck, CLK_IS_ROOT, 19200000, 0x0);

DEFINE_CLK_FIXED_RATE(virt_26000000_ck, CLK_IS_ROOT, 26000000, 0x0);

DEFINE_CLK_FIXED_RATE(virt_27000000_ck, CLK_IS_ROOT, 27000000, 0x0);

DEFINE_CLK_FIXED_RATE(virt_38400000_ck, CLK_IS_ROOT, 38400000, 0x0);

static const char *sys_clkin_ck_parents[] = {
	"virt_12000000_ck", "virt_13000000_ck", "virt_16800000_ck",
	"virt_19200000_ck", "virt_26000000_ck", "virt_27000000_ck",
	"virt_38400000_ck",
};

DEFINE_CLK_MUX(sys_clkin_ck, sys_clkin_ck_parents, NULL, 0x0,
	       OMAP4430_CM_SYS_CLKSEL, OMAP4430_SYS_CLKSEL_SHIFT,
	       OMAP4430_SYS_CLKSEL_WIDTH, CLK_MUX_INDEX_ONE, NULL);

DEFINE_CLK_FIXED_RATE(tie_low_clock_ck, CLK_IS_ROOT, 0, 0x0);

DEFINE_CLK_FIXED_RATE(utmi_phy_clkout_ck, CLK_IS_ROOT, 60000000, 0x0);

DEFINE_CLK_FIXED_RATE(xclk60mhsp1_ck, CLK_IS_ROOT, 60000000, 0x0);

DEFINE_CLK_FIXED_RATE(xclk60mhsp2_ck, CLK_IS_ROOT, 60000000, 0x0);

DEFINE_CLK_FIXED_RATE(xclk60motg_ck, CLK_IS_ROOT, 60000000, 0x0);

/* Module clocks and DPLL outputs */

static const char *abe_dpll_bypass_clk_mux_ck_parents[] = {
	"sys_clkin_ck", "sys_32k_ck",
};

DEFINE_CLK_MUX(abe_dpll_bypass_clk_mux_ck, abe_dpll_bypass_clk_mux_ck_parents,
	       NULL, 0x0, OMAP4430_CM_L4_WKUP_CLKSEL, OMAP4430_CLKSEL_SHIFT,
	       OMAP4430_CLKSEL_WIDTH, 0x0, NULL);

DEFINE_CLK_MUX(abe_dpll_refclk_mux_ck, abe_dpll_bypass_clk_mux_ck_parents, NULL,
	       0x0, OMAP4430_CM_ABE_PLL_REF_CLKSEL, OMAP4430_CLKSEL_0_0_SHIFT,
	       OMAP4430_CLKSEL_0_0_WIDTH, 0x0, NULL);

/* DPLL_ABE */
static struct dpll_data dpll_abe_dd = {
	.mult_div1_reg	= OMAP4430_CM_CLKSEL_DPLL_ABE,
	.clk_bypass	= &abe_dpll_bypass_clk_mux_ck,
	.clk_ref	= &abe_dpll_refclk_mux_ck,
	.control_reg	= OMAP4430_CM_CLKMODE_DPLL_ABE,
	.modes		= (1 << DPLL_LOW_POWER_BYPASS) | (1 << DPLL_LOCKED),
	.autoidle_reg	= OMAP4430_CM_AUTOIDLE_DPLL_ABE,
	.idlest_reg	= OMAP4430_CM_IDLEST_DPLL_ABE,
	.mult_mask	= OMAP4430_DPLL_MULT_MASK,
	.div1_mask	= OMAP4430_DPLL_DIV_MASK,
	.enable_mask	= OMAP4430_DPLL_EN_MASK,
	.autoidle_mask	= OMAP4430_AUTO_DPLL_MODE_MASK,
	.idlest_mask	= OMAP4430_ST_DPLL_CLK_MASK,
	.m4xen_mask	= OMAP4430_DPLL_REGM4XEN_MASK,
	.lpmode_mask	= OMAP4430_DPLL_LPMODE_EN_MASK,
	.max_multiplier	= 2047,
	.max_divider	= 128,
	.min_divider	= 1,
};


static const char *dpll_abe_ck_parents[] = {
	"abe_dpll_refclk_mux_ck",
};

static struct clk dpll_abe_ck;

static const struct clk_ops dpll_abe_ck_ops = {
	.enable		= &omap3_noncore_dpll_enable,
	.disable	= &omap3_noncore_dpll_disable,
	.recalc_rate	= &omap4_dpll_regm4xen_recalc,
	.round_rate	= &omap4_dpll_regm4xen_round_rate,
	.set_rate	= &omap3_noncore_dpll_set_rate,
	.get_parent	= &omap2_init_dpll_parent,
};

static struct clk_hw_omap dpll_abe_ck_hw = {
	.hw = {
		.clk = &dpll_abe_ck,
	},
	.dpll_data	= &dpll_abe_dd,
	.ops		= &clkhwops_omap3_dpll,
};

DEFINE_STRUCT_CLK(dpll_abe_ck, dpll_abe_ck_parents, dpll_abe_ck_ops);

static const char *dpll_abe_x2_ck_parents[] = {
	"dpll_abe_ck",
};

static struct clk dpll_abe_x2_ck;

static const struct clk_ops dpll_abe_x2_ck_ops = {
	.recalc_rate	= &omap3_clkoutx2_recalc,
};

static struct clk_hw_omap dpll_abe_x2_ck_hw = {
	.hw = {
		.clk = &dpll_abe_x2_ck,
	},
	.flags		= CLOCK_CLKOUTX2,
	.clksel_reg	= OMAP4430_CM_DIV_M2_DPLL_ABE,
	.ops		= &clkhwops_omap4_dpllmx,
};

DEFINE_STRUCT_CLK(dpll_abe_x2_ck, dpll_abe_x2_ck_parents, dpll_abe_x2_ck_ops);

static const struct clk_ops omap_hsdivider_ops = {
	.set_rate	= &omap2_clksel_set_rate,
	.recalc_rate	= &omap2_clksel_recalc,
	.round_rate	= &omap2_clksel_round_rate,
};

DEFINE_CLK_OMAP_HSDIVIDER(dpll_abe_m2x2_ck, "dpll_abe_x2_ck", &dpll_abe_x2_ck,
			  0x0, OMAP4430_CM_DIV_M2_DPLL_ABE,
			  OMAP4430_DPLL_CLKOUT_DIV_MASK);

DEFINE_CLK_FIXED_FACTOR(abe_24m_fclk, "dpll_abe_m2x2_ck", &dpll_abe_m2x2_ck,
			0x0, 1, 8);

DEFINE_CLK_DIVIDER(abe_clk, "dpll_abe_m2x2_ck", &dpll_abe_m2x2_ck, 0x0,
		   OMAP4430_CM_CLKSEL_ABE, OMAP4430_CLKSEL_OPP_SHIFT,
		   OMAP4430_CLKSEL_OPP_WIDTH, CLK_DIVIDER_POWER_OF_TWO, NULL);

DEFINE_CLK_DIVIDER(aess_fclk, "abe_clk", &abe_clk, 0x0,
		   OMAP4430_CM1_ABE_AESS_CLKCTRL,
		   OMAP4430_CLKSEL_AESS_FCLK_SHIFT,
		   OMAP4430_CLKSEL_AESS_FCLK_WIDTH,
		   0x0, NULL);

DEFINE_CLK_OMAP_HSDIVIDER(dpll_abe_m3x2_ck, "dpll_abe_x2_ck", &dpll_abe_x2_ck,
			  0x0, OMAP4430_CM_DIV_M3_DPLL_ABE,
			  OMAP4430_DPLL_CLKOUTHIF_DIV_MASK);

static const char *core_hsd_byp_clk_mux_ck_parents[] = {
	"sys_clkin_ck", "dpll_abe_m3x2_ck",
};

DEFINE_CLK_MUX(core_hsd_byp_clk_mux_ck, core_hsd_byp_clk_mux_ck_parents, NULL,
	       0x0, OMAP4430_CM_CLKSEL_DPLL_CORE,
	       OMAP4430_DPLL_BYP_CLKSEL_SHIFT, OMAP4430_DPLL_BYP_CLKSEL_WIDTH,
	       0x0, NULL);

/* DPLL_CORE */
static struct dpll_data dpll_core_dd = {
	.mult_div1_reg	= OMAP4430_CM_CLKSEL_DPLL_CORE,
	.clk_bypass	= &core_hsd_byp_clk_mux_ck,
	.clk_ref	= &sys_clkin_ck,
	.control_reg	= OMAP4430_CM_CLKMODE_DPLL_CORE,
	.modes		= (1 << DPLL_LOW_POWER_BYPASS) | (1 << DPLL_LOCKED),
	.autoidle_reg	= OMAP4430_CM_AUTOIDLE_DPLL_CORE,
	.idlest_reg	= OMAP4430_CM_IDLEST_DPLL_CORE,
	.mult_mask	= OMAP4430_DPLL_MULT_MASK,
	.div1_mask	= OMAP4430_DPLL_DIV_MASK,
	.enable_mask	= OMAP4430_DPLL_EN_MASK,
	.autoidle_mask	= OMAP4430_AUTO_DPLL_MODE_MASK,
	.idlest_mask	= OMAP4430_ST_DPLL_CLK_MASK,
	.max_multiplier	= 2047,
	.max_divider	= 128,
	.min_divider	= 1,
};


static const char *dpll_core_ck_parents[] = {
	"sys_clkin_ck", "core_hsd_byp_clk_mux_ck"
};

static struct clk dpll_core_ck;

static const struct clk_ops dpll_core_ck_ops = {
	.recalc_rate	= &omap3_dpll_recalc,
	.get_parent	= &omap2_init_dpll_parent,
};

static struct clk_hw_omap dpll_core_ck_hw = {
	.hw = {
		.clk = &dpll_core_ck,
	},
	.dpll_data	= &dpll_core_dd,
	.ops		= &clkhwops_omap3_dpll,
};

DEFINE_STRUCT_CLK(dpll_core_ck, dpll_core_ck_parents, dpll_core_ck_ops);

static const char *dpll_core_x2_ck_parents[] = {
	"dpll_core_ck",
};

static struct clk dpll_core_x2_ck;

static struct clk_hw_omap dpll_core_x2_ck_hw = {
	.hw = {
		.clk = &dpll_core_x2_ck,
	},
};

DEFINE_STRUCT_CLK(dpll_core_x2_ck, dpll_core_x2_ck_parents, dpll_abe_x2_ck_ops);

DEFINE_CLK_OMAP_HSDIVIDER(dpll_core_m6x2_ck, "dpll_core_x2_ck",
			  &dpll_core_x2_ck, 0x0, OMAP4430_CM_DIV_M6_DPLL_CORE,
			  OMAP4430_HSDIVIDER_CLKOUT3_DIV_MASK);

DEFINE_CLK_OMAP_HSDIVIDER(dpll_core_m2_ck, "dpll_core_ck", &dpll_core_ck, 0x0,
			  OMAP4430_CM_DIV_M2_DPLL_CORE,
			  OMAP4430_DPLL_CLKOUT_DIV_MASK);

DEFINE_CLK_FIXED_FACTOR(ddrphy_ck, "dpll_core_m2_ck", &dpll_core_m2_ck, 0x0, 1,
			2);

DEFINE_CLK_OMAP_HSDIVIDER(dpll_core_m5x2_ck, "dpll_core_x2_ck",
			  &dpll_core_x2_ck, 0x0, OMAP4430_CM_DIV_M5_DPLL_CORE,
			  OMAP4430_HSDIVIDER_CLKOUT2_DIV_MASK);

DEFINE_CLK_DIVIDER(div_core_ck, "dpll_core_m5x2_ck", &dpll_core_m5x2_ck, 0x0,
		   OMAP4430_CM_CLKSEL_CORE, OMAP4430_CLKSEL_CORE_SHIFT,
		   OMAP4430_CLKSEL_CORE_WIDTH, 0x0, NULL);

DEFINE_CLK_DIVIDER(div_iva_hs_clk, "dpll_core_m5x2_ck", &dpll_core_m5x2_ck,
		   0x0, OMAP4430_CM_BYPCLK_DPLL_IVA, OMAP4430_CLKSEL_0_1_SHIFT,
		   OMAP4430_CLKSEL_0_1_WIDTH, CLK_DIVIDER_POWER_OF_TWO, NULL);

DEFINE_CLK_DIVIDER(div_mpu_hs_clk, "dpll_core_m5x2_ck", &dpll_core_m5x2_ck,
		   0x0, OMAP4430_CM_BYPCLK_DPLL_MPU, OMAP4430_CLKSEL_0_1_SHIFT,
		   OMAP4430_CLKSEL_0_1_WIDTH, CLK_DIVIDER_POWER_OF_TWO, NULL);

DEFINE_CLK_OMAP_HSDIVIDER(dpll_core_m4x2_ck, "dpll_core_x2_ck",
			  &dpll_core_x2_ck, 0x0, OMAP4430_CM_DIV_M4_DPLL_CORE,
			  OMAP4430_HSDIVIDER_CLKOUT1_DIV_MASK);

DEFINE_CLK_FIXED_FACTOR(dll_clk_div_ck, "dpll_core_m4x2_ck", &dpll_core_m4x2_ck,
			0x0, 1, 2);

DEFINE_CLK_DIVIDER(dpll_abe_m2_ck, "dpll_abe_ck", &dpll_abe_ck, 0x0,
		   OMAP4430_CM_DIV_M2_DPLL_ABE, OMAP4430_DPLL_CLKOUT_DIV_SHIFT,
		   OMAP4430_DPLL_CLKOUT_DIV_WIDTH, CLK_DIVIDER_ONE_BASED, NULL);

static const struct clk_ops dmic_fck_ops = {
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
	.is_enabled	= &omap2_dflt_clk_is_enabled,
	.recalc_rate	= &omap2_clksel_recalc,
	.get_parent	= &omap2_clksel_find_parent_index,
	.set_parent	= &omap2_clksel_set_parent,
	.init		= &omap2_init_clk_clkdm,
};

static const char *dpll_core_m3x2_ck_parents[] = {
	"dpll_core_x2_ck",
};

static const struct clksel dpll_core_m3x2_div[] = {
	{ .parent = &dpll_core_x2_ck, .rates = div31_1to31_rates },
	{ .parent = NULL },
};

/* XXX Missing round_rate, set_rate in ops */
DEFINE_CLK_OMAP_MUX_GATE(dpll_core_m3x2_ck, NULL, dpll_core_m3x2_div,
			 OMAP4430_CM_DIV_M3_DPLL_CORE,
			 OMAP4430_DPLL_CLKOUTHIF_DIV_MASK,
			 OMAP4430_CM_DIV_M3_DPLL_CORE,
			 OMAP4430_DPLL_CLKOUTHIF_GATE_CTRL_SHIFT, NULL,
			 dpll_core_m3x2_ck_parents, dmic_fck_ops);

DEFINE_CLK_OMAP_HSDIVIDER(dpll_core_m7x2_ck, "dpll_core_x2_ck",
			  &dpll_core_x2_ck, 0x0, OMAP4430_CM_DIV_M7_DPLL_CORE,
			  OMAP4430_HSDIVIDER_CLKOUT4_DIV_MASK);

static const char *iva_hsd_byp_clk_mux_ck_parents[] = {
	"sys_clkin_ck", "div_iva_hs_clk",
};

DEFINE_CLK_MUX(iva_hsd_byp_clk_mux_ck, iva_hsd_byp_clk_mux_ck_parents, NULL,
	       0x0, OMAP4430_CM_CLKSEL_DPLL_IVA, OMAP4430_DPLL_BYP_CLKSEL_SHIFT,
	       OMAP4430_DPLL_BYP_CLKSEL_WIDTH, 0x0, NULL);

/* DPLL_IVA */
static struct dpll_data dpll_iva_dd = {
	.mult_div1_reg	= OMAP4430_CM_CLKSEL_DPLL_IVA,
	.clk_bypass	= &iva_hsd_byp_clk_mux_ck,
	.clk_ref	= &sys_clkin_ck,
	.control_reg	= OMAP4430_CM_CLKMODE_DPLL_IVA,
	.modes		= (1 << DPLL_LOW_POWER_BYPASS) | (1 << DPLL_LOCKED),
	.autoidle_reg	= OMAP4430_CM_AUTOIDLE_DPLL_IVA,
	.idlest_reg	= OMAP4430_CM_IDLEST_DPLL_IVA,
	.mult_mask	= OMAP4430_DPLL_MULT_MASK,
	.div1_mask	= OMAP4430_DPLL_DIV_MASK,
	.enable_mask	= OMAP4430_DPLL_EN_MASK,
	.autoidle_mask	= OMAP4430_AUTO_DPLL_MODE_MASK,
	.idlest_mask	= OMAP4430_ST_DPLL_CLK_MASK,
	.max_multiplier	= 2047,
	.max_divider	= 128,
	.min_divider	= 1,
};

static const char *dpll_iva_ck_parents[] = {
	"sys_clkin_ck", "iva_hsd_byp_clk_mux_ck"
};

static struct clk dpll_iva_ck;

static const struct clk_ops dpll_ck_ops = {
	.enable		= &omap3_noncore_dpll_enable,
	.disable	= &omap3_noncore_dpll_disable,
	.recalc_rate	= &omap3_dpll_recalc,
	.round_rate	= &omap2_dpll_round_rate,
	.set_rate	= &omap3_noncore_dpll_set_rate,
	.get_parent	= &omap2_init_dpll_parent,
};

static struct clk_hw_omap dpll_iva_ck_hw = {
	.hw = {
		.clk = &dpll_iva_ck,
	},
	.dpll_data	= &dpll_iva_dd,
	.ops		= &clkhwops_omap3_dpll,
};

DEFINE_STRUCT_CLK(dpll_iva_ck, dpll_iva_ck_parents, dpll_ck_ops);

static const char *dpll_iva_x2_ck_parents[] = {
	"dpll_iva_ck",
};

static struct clk dpll_iva_x2_ck;

static struct clk_hw_omap dpll_iva_x2_ck_hw = {
	.hw = {
		.clk = &dpll_iva_x2_ck,
	},
};

DEFINE_STRUCT_CLK(dpll_iva_x2_ck, dpll_iva_x2_ck_parents, dpll_abe_x2_ck_ops);

DEFINE_CLK_OMAP_HSDIVIDER(dpll_iva_m4x2_ck, "dpll_iva_x2_ck", &dpll_iva_x2_ck,
			  0x0, OMAP4430_CM_DIV_M4_DPLL_IVA,
			  OMAP4430_HSDIVIDER_CLKOUT1_DIV_MASK);

DEFINE_CLK_OMAP_HSDIVIDER(dpll_iva_m5x2_ck, "dpll_iva_x2_ck", &dpll_iva_x2_ck,
			  0x0, OMAP4430_CM_DIV_M5_DPLL_IVA,
			  OMAP4430_HSDIVIDER_CLKOUT2_DIV_MASK);

/* DPLL_MPU */
static struct dpll_data dpll_mpu_dd = {
	.mult_div1_reg	= OMAP4430_CM_CLKSEL_DPLL_MPU,
	.clk_bypass	= &div_mpu_hs_clk,
	.clk_ref	= &sys_clkin_ck,
	.control_reg	= OMAP4430_CM_CLKMODE_DPLL_MPU,
	.modes		= (1 << DPLL_LOW_POWER_BYPASS) | (1 << DPLL_LOCKED),
	.autoidle_reg	= OMAP4430_CM_AUTOIDLE_DPLL_MPU,
	.idlest_reg	= OMAP4430_CM_IDLEST_DPLL_MPU,
	.mult_mask	= OMAP4430_DPLL_MULT_MASK,
	.div1_mask	= OMAP4430_DPLL_DIV_MASK,
	.enable_mask	= OMAP4430_DPLL_EN_MASK,
	.autoidle_mask	= OMAP4430_AUTO_DPLL_MODE_MASK,
	.idlest_mask	= OMAP4430_ST_DPLL_CLK_MASK,
	.max_multiplier	= 2047,
	.max_divider	= 128,
	.min_divider	= 1,
};

static const char *dpll_mpu_ck_parents[] = {
	"sys_clkin_ck", "div_mpu_hs_clk"
};

static struct clk dpll_mpu_ck;

static struct clk_hw_omap dpll_mpu_ck_hw = {
	.hw = {
		.clk = &dpll_mpu_ck,
	},
	.dpll_data	= &dpll_mpu_dd,
	.ops		= &clkhwops_omap3_dpll,
};

DEFINE_STRUCT_CLK(dpll_mpu_ck, dpll_mpu_ck_parents, dpll_ck_ops);

DEFINE_CLK_FIXED_FACTOR(mpu_periphclk, "dpll_mpu_ck", &dpll_mpu_ck, 0x0, 1, 2);

DEFINE_CLK_OMAP_HSDIVIDER(dpll_mpu_m2_ck, "dpll_mpu_ck", &dpll_mpu_ck, 0x0,
			  OMAP4430_CM_DIV_M2_DPLL_MPU,
			  OMAP4430_DPLL_CLKOUT_DIV_MASK);

DEFINE_CLK_FIXED_FACTOR(per_hs_clk_div_ck, "dpll_abe_m3x2_ck",
			&dpll_abe_m3x2_ck, 0x0, 1, 2);

static const char *per_hsd_byp_clk_mux_ck_parents[] = {
	"sys_clkin_ck", "per_hs_clk_div_ck",
};

DEFINE_CLK_MUX(per_hsd_byp_clk_mux_ck, per_hsd_byp_clk_mux_ck_parents, NULL,
	       0x0, OMAP4430_CM_CLKSEL_DPLL_PER, OMAP4430_DPLL_BYP_CLKSEL_SHIFT,
	       OMAP4430_DPLL_BYP_CLKSEL_WIDTH, 0x0, NULL);

/* DPLL_PER */
static struct dpll_data dpll_per_dd = {
	.mult_div1_reg	= OMAP4430_CM_CLKSEL_DPLL_PER,
	.clk_bypass	= &per_hsd_byp_clk_mux_ck,
	.clk_ref	= &sys_clkin_ck,
	.control_reg	= OMAP4430_CM_CLKMODE_DPLL_PER,
	.modes		= (1 << DPLL_LOW_POWER_BYPASS) | (1 << DPLL_LOCKED),
	.autoidle_reg	= OMAP4430_CM_AUTOIDLE_DPLL_PER,
	.idlest_reg	= OMAP4430_CM_IDLEST_DPLL_PER,
	.mult_mask	= OMAP4430_DPLL_MULT_MASK,
	.div1_mask	= OMAP4430_DPLL_DIV_MASK,
	.enable_mask	= OMAP4430_DPLL_EN_MASK,
	.autoidle_mask	= OMAP4430_AUTO_DPLL_MODE_MASK,
	.idlest_mask	= OMAP4430_ST_DPLL_CLK_MASK,
	.max_multiplier	= 2047,
	.max_divider	= 128,
	.min_divider	= 1,
};

static const char *dpll_per_ck_parents[] = {
	"sys_clkin_ck", "per_hsd_byp_clk_mux_ck"
};

static struct clk dpll_per_ck;

static struct clk_hw_omap dpll_per_ck_hw = {
	.hw = {
		.clk = &dpll_per_ck,
	},
	.dpll_data	= &dpll_per_dd,
	.ops		= &clkhwops_omap3_dpll,
};

DEFINE_STRUCT_CLK(dpll_per_ck, dpll_per_ck_parents, dpll_ck_ops);

DEFINE_CLK_DIVIDER(dpll_per_m2_ck, "dpll_per_ck", &dpll_per_ck, 0x0,
		   OMAP4430_CM_DIV_M2_DPLL_PER, OMAP4430_DPLL_CLKOUT_DIV_SHIFT,
		   OMAP4430_DPLL_CLKOUT_DIV_WIDTH, CLK_DIVIDER_ONE_BASED, NULL);

static const char *dpll_per_x2_ck_parents[] = {
	"dpll_per_ck",
};

static struct clk dpll_per_x2_ck;

static struct clk_hw_omap dpll_per_x2_ck_hw = {
	.hw = {
		.clk = &dpll_per_x2_ck,
	},
	.flags		= CLOCK_CLKOUTX2,
	.clksel_reg	= OMAP4430_CM_DIV_M2_DPLL_PER,
	.ops		= &clkhwops_omap4_dpllmx,
};

DEFINE_STRUCT_CLK(dpll_per_x2_ck, dpll_per_x2_ck_parents, dpll_abe_x2_ck_ops);

DEFINE_CLK_OMAP_HSDIVIDER(dpll_per_m2x2_ck, "dpll_per_x2_ck", &dpll_per_x2_ck,
			  0x0, OMAP4430_CM_DIV_M2_DPLL_PER,
			  OMAP4430_DPLL_CLKOUT_DIV_MASK);

static const char *dpll_per_m3x2_ck_parents[] = {
	"dpll_per_x2_ck",
};

static const struct clksel dpll_per_m3x2_div[] = {
	{ .parent = &dpll_per_x2_ck, .rates = div31_1to31_rates },
	{ .parent = NULL },
};

/* XXX Missing round_rate, set_rate in ops */
DEFINE_CLK_OMAP_MUX_GATE(dpll_per_m3x2_ck, NULL, dpll_per_m3x2_div,
			 OMAP4430_CM_DIV_M3_DPLL_PER,
			 OMAP4430_DPLL_CLKOUTHIF_DIV_MASK,
			 OMAP4430_CM_DIV_M3_DPLL_PER,
			 OMAP4430_DPLL_CLKOUTHIF_GATE_CTRL_SHIFT, NULL,
			 dpll_per_m3x2_ck_parents, dmic_fck_ops);

DEFINE_CLK_OMAP_HSDIVIDER(dpll_per_m4x2_ck, "dpll_per_x2_ck", &dpll_per_x2_ck,
			  0x0, OMAP4430_CM_DIV_M4_DPLL_PER,
			  OMAP4430_HSDIVIDER_CLKOUT1_DIV_MASK);

DEFINE_CLK_OMAP_HSDIVIDER(dpll_per_m5x2_ck, "dpll_per_x2_ck", &dpll_per_x2_ck,
			  0x0, OMAP4430_CM_DIV_M5_DPLL_PER,
			  OMAP4430_HSDIVIDER_CLKOUT2_DIV_MASK);

DEFINE_CLK_OMAP_HSDIVIDER(dpll_per_m6x2_ck, "dpll_per_x2_ck", &dpll_per_x2_ck,
			  0x0, OMAP4430_CM_DIV_M6_DPLL_PER,
			  OMAP4430_HSDIVIDER_CLKOUT3_DIV_MASK);

DEFINE_CLK_OMAP_HSDIVIDER(dpll_per_m7x2_ck, "dpll_per_x2_ck", &dpll_per_x2_ck,
			  0x0, OMAP4430_CM_DIV_M7_DPLL_PER,
			  OMAP4430_HSDIVIDER_CLKOUT4_DIV_MASK);

DEFINE_CLK_FIXED_FACTOR(usb_hs_clk_div_ck, "dpll_abe_m3x2_ck",
			&dpll_abe_m3x2_ck, 0x0, 1, 3);

/* DPLL_USB */
static struct dpll_data dpll_usb_dd = {
	.mult_div1_reg	= OMAP4430_CM_CLKSEL_DPLL_USB,
	.clk_bypass	= &usb_hs_clk_div_ck,
	.flags		= DPLL_J_TYPE,
	.clk_ref	= &sys_clkin_ck,
	.control_reg	= OMAP4430_CM_CLKMODE_DPLL_USB,
	.modes		= (1 << DPLL_LOW_POWER_BYPASS) | (1 << DPLL_LOCKED),
	.autoidle_reg	= OMAP4430_CM_AUTOIDLE_DPLL_USB,
	.idlest_reg	= OMAP4430_CM_IDLEST_DPLL_USB,
	.mult_mask	= OMAP4430_DPLL_MULT_USB_MASK,
	.div1_mask	= OMAP4430_DPLL_DIV_0_7_MASK,
	.enable_mask	= OMAP4430_DPLL_EN_MASK,
	.autoidle_mask	= OMAP4430_AUTO_DPLL_MODE_MASK,
	.idlest_mask	= OMAP4430_ST_DPLL_CLK_MASK,
	.sddiv_mask	= OMAP4430_DPLL_SD_DIV_MASK,
	.max_multiplier	= 4095,
	.max_divider	= 256,
	.min_divider	= 1,
};

static const char *dpll_usb_ck_parents[] = {
	"sys_clkin_ck", "usb_hs_clk_div_ck"
};

static struct clk dpll_usb_ck;

static struct clk_hw_omap dpll_usb_ck_hw = {
	.hw = {
		.clk = &dpll_usb_ck,
	},
	.dpll_data	= &dpll_usb_dd,
	.ops		= &clkhwops_omap3_dpll,
};

DEFINE_STRUCT_CLK(dpll_usb_ck, dpll_usb_ck_parents, dpll_ck_ops);

static const char *dpll_usb_clkdcoldo_ck_parents[] = {
	"dpll_usb_ck",
};

static struct clk dpll_usb_clkdcoldo_ck;

static const struct clk_ops dpll_usb_clkdcoldo_ck_ops = {
};

static struct clk_hw_omap dpll_usb_clkdcoldo_ck_hw = {
	.hw = {
		.clk = &dpll_usb_clkdcoldo_ck,
	},
	.clksel_reg	= OMAP4430_CM_CLKDCOLDO_DPLL_USB,
	.ops		= &clkhwops_omap4_dpllmx,
};

DEFINE_STRUCT_CLK(dpll_usb_clkdcoldo_ck, dpll_usb_clkdcoldo_ck_parents,
		  dpll_usb_clkdcoldo_ck_ops);

DEFINE_CLK_OMAP_HSDIVIDER(dpll_usb_m2_ck, "dpll_usb_ck", &dpll_usb_ck, 0x0,
			  OMAP4430_CM_DIV_M2_DPLL_USB,
			  OMAP4430_DPLL_CLKOUT_DIV_0_6_MASK);

static const char *ducati_clk_mux_ck_parents[] = {
	"div_core_ck", "dpll_per_m6x2_ck",
};

DEFINE_CLK_MUX(ducati_clk_mux_ck, ducati_clk_mux_ck_parents, NULL, 0x0,
	       OMAP4430_CM_CLKSEL_DUCATI_ISS_ROOT, OMAP4430_CLKSEL_0_0_SHIFT,
	       OMAP4430_CLKSEL_0_0_WIDTH, 0x0, NULL);

DEFINE_CLK_FIXED_FACTOR(func_12m_fclk, "dpll_per_m2x2_ck", &dpll_per_m2x2_ck,
			0x0, 1, 16);

DEFINE_CLK_FIXED_FACTOR(func_24m_clk, "dpll_per_m2_ck", &dpll_per_m2_ck, 0x0,
			1, 4);

DEFINE_CLK_FIXED_FACTOR(func_24mc_fclk, "dpll_per_m2x2_ck", &dpll_per_m2x2_ck,
			0x0, 1, 8);

static const struct clk_div_table func_48m_fclk_rates[] = {
	{ .div = 4, .val = 0 },
	{ .div = 8, .val = 1 },
	{ .div = 0 },
};
DEFINE_CLK_DIVIDER_TABLE(func_48m_fclk, "dpll_per_m2x2_ck", &dpll_per_m2x2_ck,
			 0x0, OMAP4430_CM_SCALE_FCLK, OMAP4430_SCALE_FCLK_SHIFT,
			 OMAP4430_SCALE_FCLK_WIDTH, 0x0, func_48m_fclk_rates,
			 NULL);

DEFINE_CLK_FIXED_FACTOR(func_48mc_fclk,	"dpll_per_m2x2_ck", &dpll_per_m2x2_ck,
			0x0, 1, 4);

static const struct clk_div_table func_64m_fclk_rates[] = {
	{ .div = 2, .val = 0 },
	{ .div = 4, .val = 1 },
	{ .div = 0 },
};
DEFINE_CLK_DIVIDER_TABLE(func_64m_fclk, "dpll_per_m4x2_ck", &dpll_per_m4x2_ck,
			 0x0, OMAP4430_CM_SCALE_FCLK, OMAP4430_SCALE_FCLK_SHIFT,
			 OMAP4430_SCALE_FCLK_WIDTH, 0x0, func_64m_fclk_rates,
			 NULL);

static const struct clk_div_table func_96m_fclk_rates[] = {
	{ .div = 2, .val = 0 },
	{ .div = 4, .val = 1 },
	{ .div = 0 },
};
DEFINE_CLK_DIVIDER_TABLE(func_96m_fclk, "dpll_per_m2x2_ck", &dpll_per_m2x2_ck,
			 0x0, OMAP4430_CM_SCALE_FCLK, OMAP4430_SCALE_FCLK_SHIFT,
			 OMAP4430_SCALE_FCLK_WIDTH, 0x0, func_96m_fclk_rates,
			 NULL);

static const struct clk_div_table init_60m_fclk_rates[] = {
	{ .div = 1, .val = 0 },
	{ .div = 8, .val = 1 },
	{ .div = 0 },
};
DEFINE_CLK_DIVIDER_TABLE(init_60m_fclk, "dpll_usb_m2_ck", &dpll_usb_m2_ck,
			 0x0, OMAP4430_CM_CLKSEL_USB_60MHZ,
			 OMAP4430_CLKSEL_0_0_SHIFT, OMAP4430_CLKSEL_0_0_WIDTH,
			 0x0, init_60m_fclk_rates, NULL);

DEFINE_CLK_DIVIDER(l3_div_ck, "div_core_ck", &div_core_ck, 0x0,
		   OMAP4430_CM_CLKSEL_CORE, OMAP4430_CLKSEL_L3_SHIFT,
		   OMAP4430_CLKSEL_L3_WIDTH, 0x0, NULL);

DEFINE_CLK_DIVIDER(l4_div_ck, "l3_div_ck", &l3_div_ck, 0x0,
		   OMAP4430_CM_CLKSEL_CORE, OMAP4430_CLKSEL_L4_SHIFT,
		   OMAP4430_CLKSEL_L4_WIDTH, 0x0, NULL);

DEFINE_CLK_FIXED_FACTOR(lp_clk_div_ck, "dpll_abe_m2x2_ck", &dpll_abe_m2x2_ck,
			0x0, 1, 16);

static const char *l4_wkup_clk_mux_ck_parents[] = {
	"sys_clkin_ck", "lp_clk_div_ck",
};

DEFINE_CLK_MUX(l4_wkup_clk_mux_ck, l4_wkup_clk_mux_ck_parents, NULL, 0x0,
	       OMAP4430_CM_L4_WKUP_CLKSEL, OMAP4430_CLKSEL_0_0_SHIFT,
	       OMAP4430_CLKSEL_0_0_WIDTH, 0x0, NULL);

static const struct clk_div_table ocp_abe_iclk_rates[] = {
	{ .div = 2, .val = 0 },
	{ .div = 1, .val = 1 },
	{ .div = 0 },
};
DEFINE_CLK_DIVIDER_TABLE(ocp_abe_iclk, "aess_fclk", &aess_fclk, 0x0,
			 OMAP4430_CM1_ABE_AESS_CLKCTRL,
			 OMAP4430_CLKSEL_AESS_FCLK_SHIFT,
			 OMAP4430_CLKSEL_AESS_FCLK_WIDTH,
			 0x0, ocp_abe_iclk_rates, NULL);

DEFINE_CLK_FIXED_FACTOR(per_abe_24m_fclk, "dpll_abe_m2_ck", &dpll_abe_m2_ck,
			0x0, 1, 4);

DEFINE_CLK_DIVIDER(per_abe_nc_fclk, "dpll_abe_m2_ck", &dpll_abe_m2_ck, 0x0,
		   OMAP4430_CM_SCALE_FCLK, OMAP4430_SCALE_FCLK_SHIFT,
		   OMAP4430_SCALE_FCLK_WIDTH, 0x0, NULL);

DEFINE_CLK_DIVIDER(syc_clk_div_ck, "sys_clkin_ck", &sys_clkin_ck, 0x0,
		   OMAP4430_CM_ABE_DSS_SYS_CLKSEL, OMAP4430_CLKSEL_0_0_SHIFT,
		   OMAP4430_CLKSEL_0_0_WIDTH, 0x0, NULL);

static const char *dbgclk_mux_ck_parents[] = {
	"sys_clkin_ck"
};

static struct clk dbgclk_mux_ck;
DEFINE_STRUCT_CLK_HW_OMAP(dbgclk_mux_ck, NULL);
DEFINE_STRUCT_CLK(dbgclk_mux_ck, dbgclk_mux_ck_parents,
		  dpll_usb_clkdcoldo_ck_ops);

/* Leaf clocks controlled by modules */

DEFINE_CLK_GATE(aes1_fck, "l3_div_ck", &l3_div_ck, 0x0,
		OMAP4430_CM_L4SEC_AES1_CLKCTRL,
		OMAP4430_MODULEMODE_SWCTRL_SHIFT, 0x0, NULL);

DEFINE_CLK_GATE(aes2_fck, "l3_div_ck", &l3_div_ck, 0x0,
		OMAP4430_CM_L4SEC_AES2_CLKCTRL,
		OMAP4430_MODULEMODE_SWCTRL_SHIFT, 0x0, NULL);

DEFINE_CLK_GATE(aess_fck, "aess_fclk", &aess_fclk, 0x0,
		OMAP4430_CM1_ABE_AESS_CLKCTRL, OMAP4430_MODULEMODE_SWCTRL_SHIFT,
		0x0, NULL);

DEFINE_CLK_GATE(bandgap_fclk, "sys_32k_ck", &sys_32k_ck, 0x0,
		OMAP4430_CM_WKUP_BANDGAP_CLKCTRL,
		OMAP4430_OPTFCLKEN_BGAP_32K_SHIFT, 0x0, NULL);

static const struct clk_div_table div_ts_ck_rates[] = {
	{ .div = 8, .val = 0 },
	{ .div = 16, .val = 1 },
	{ .div = 32, .val = 2 },
	{ .div = 0 },
};
DEFINE_CLK_DIVIDER_TABLE(div_ts_ck, "l4_wkup_clk_mux_ck", &l4_wkup_clk_mux_ck,
			 0x0, OMAP4430_CM_WKUP_BANDGAP_CLKCTRL,
			 OMAP4430_CLKSEL_24_25_SHIFT,
			 OMAP4430_CLKSEL_24_25_WIDTH, 0x0, div_ts_ck_rates,
			 NULL);

DEFINE_CLK_GATE(bandgap_ts_fclk, "div_ts_ck", &div_ts_ck, 0x0,
		OMAP4430_CM_WKUP_BANDGAP_CLKCTRL,
		OMAP4460_OPTFCLKEN_TS_FCLK_SHIFT,
		0x0, NULL);

DEFINE_CLK_GATE(des3des_fck, "l4_div_ck", &l4_div_ck, 0x0,
		OMAP4430_CM_L4SEC_DES3DES_CLKCTRL,
		OMAP4430_MODULEMODE_SWCTRL_SHIFT,
		0x0, NULL);

static const char *dmic_sync_mux_ck_parents[] = {
	"abe_24m_fclk", "syc_clk_div_ck", "func_24m_clk",
};

DEFINE_CLK_MUX(dmic_sync_mux_ck, dmic_sync_mux_ck_parents, NULL,
	       0x0, OMAP4430_CM1_ABE_DMIC_CLKCTRL,
	       OMAP4430_CLKSEL_INTERNAL_SOURCE_SHIFT,
	       OMAP4430_CLKSEL_INTERNAL_SOURCE_WIDTH, 0x0, NULL);

static const struct clksel func_dmic_abe_gfclk_sel[] = {
	{ .parent = &dmic_sync_mux_ck, .rates = div_1_0_rates },
	{ .parent = &pad_clks_ck, .rates = div_1_1_rates },
	{ .parent = &slimbus_clk, .rates = div_1_2_rates },
	{ .parent = NULL },
};

static const char *dmic_fck_parents[] = {
	"dmic_sync_mux_ck", "pad_clks_ck", "slimbus_clk",
};

/* Merged func_dmic_abe_gfclk into dmic */
static struct clk dmic_fck;

DEFINE_CLK_OMAP_MUX_GATE(dmic_fck, "abe_clkdm", func_dmic_abe_gfclk_sel,
			 OMAP4430_CM1_ABE_DMIC_CLKCTRL,
			 OMAP4430_CLKSEL_SOURCE_MASK,
			 OMAP4430_CM1_ABE_DMIC_CLKCTRL,
			 OMAP4430_MODULEMODE_SWCTRL_SHIFT, NULL,
			 dmic_fck_parents, dmic_fck_ops);

DEFINE_CLK_GATE(dsp_fck, "dpll_iva_m4x2_ck", &dpll_iva_m4x2_ck, 0x0,
		OMAP4430_CM_TESLA_TESLA_CLKCTRL,
		OMAP4430_MODULEMODE_HWCTRL_SHIFT, 0x0, NULL);

DEFINE_CLK_GATE(dss_sys_clk, "syc_clk_div_ck", &syc_clk_div_ck, 0x0,
		OMAP4430_CM_DSS_DSS_CLKCTRL,
		OMAP4430_OPTFCLKEN_SYS_CLK_SHIFT, 0x0, NULL);

DEFINE_CLK_GATE(dss_tv_clk, "extalt_clkin_ck", &extalt_clkin_ck, 0x0,
		OMAP4430_CM_DSS_DSS_CLKCTRL,
		OMAP4430_OPTFCLKEN_TV_CLK_SHIFT, 0x0, NULL);

DEFINE_CLK_GATE(dss_dss_clk, "dpll_per_m5x2_ck", &dpll_per_m5x2_ck, 0x0,
		OMAP4430_CM_DSS_DSS_CLKCTRL, OMAP4430_OPTFCLKEN_DSSCLK_SHIFT,
		0x0, NULL);

DEFINE_CLK_GATE(dss_48mhz_clk, "func_48mc_fclk", &func_48mc_fclk, 0x0,
		OMAP4430_CM_DSS_DSS_CLKCTRL, OMAP4430_OPTFCLKEN_48MHZ_CLK_SHIFT,
		0x0, NULL);

DEFINE_CLK_GATE(dss_fck, "l3_div_ck", &l3_div_ck, 0x0,
		OMAP4430_CM_DSS_DSS_CLKCTRL, OMAP4430_MODULEMODE_SWCTRL_SHIFT,
		0x0, NULL);

DEFINE_CLK_GATE(efuse_ctrl_cust_fck, "sys_clkin_ck", &sys_clkin_ck, 0x0,
		OMAP4430_CM_CEFUSE_CEFUSE_CLKCTRL,
		OMAP4430_MODULEMODE_SWCTRL_SHIFT, 0x0, NULL);

DEFINE_CLK_GATE(emif1_fck, "ddrphy_ck", &ddrphy_ck, 0x0,
		OMAP4430_CM_MEMIF_EMIF_1_CLKCTRL,
		OMAP4430_MODULEMODE_HWCTRL_SHIFT, 0x0, NULL);

DEFINE_CLK_GATE(emif2_fck, "ddrphy_ck", &ddrphy_ck, 0x0,
		OMAP4430_CM_MEMIF_EMIF_2_CLKCTRL,
		OMAP4430_MODULEMODE_HWCTRL_SHIFT, 0x0, NULL);

DEFINE_CLK_DIVIDER(fdif_fck, "dpll_per_m4x2_ck", &dpll_per_m4x2_ck, 0x0,
		   OMAP4430_CM_CAM_FDIF_CLKCTRL, OMAP4430_CLKSEL_FCLK_SHIFT,
		   OMAP4430_CLKSEL_FCLK_WIDTH, CLK_DIVIDER_POWER_OF_TWO, NULL);

DEFINE_CLK_GATE(fpka_fck, "l4_div_ck", &l4_div_ck, 0x0,
		OMAP4430_CM_L4SEC_PKAEIP29_CLKCTRL,
		OMAP4430_MODULEMODE_SWCTRL_SHIFT, 0x0, NULL);

DEFINE_CLK_GATE(gpio1_dbclk, "sys_32k_ck", &sys_32k_ck, 0x0,
		OMAP4430_CM_WKUP_GPIO1_CLKCTRL,
		OMAP4430_OPTFCLKEN_DBCLK_SHIFT,	0x0, NULL);

DEFINE_CLK_GATE(gpio1_ick, "l4_wkup_clk_mux_ck", &l4_wkup_clk_mux_ck, 0x0,
		OMAP4430_CM_WKUP_GPIO1_CLKCTRL,
		OMAP4430_MODULEMODE_HWCTRL_SHIFT, 0x0, NULL);

DEFINE_CLK_GATE(gpio2_dbclk, "sys_32k_ck", &sys_32k_ck, 0x0,
		OMAP4430_CM_L4PER_GPIO2_CLKCTRL, OMAP4430_OPTFCLKEN_DBCLK_SHIFT,
		0x0, NULL);

DEFINE_CLK_GATE(gpio2_ick, "l4_div_ck", &l4_div_ck, 0x0,
		OMAP4430_CM_L4PER_GPIO2_CLKCTRL,
		OMAP4430_MODULEMODE_HWCTRL_SHIFT, 0x0, NULL);

DEFINE_CLK_GATE(gpio3_dbclk, "sys_32k_ck", &sys_32k_ck, 0x0,
		OMAP4430_CM_L4PER_GPIO3_CLKCTRL,
		OMAP4430_OPTFCLKEN_DBCLK_SHIFT, 0x0, NULL);

DEFINE_CLK_GATE(gpio3_ick, "l4_div_ck", &l4_div_ck, 0x0,
		OMAP4430_CM_L4PER_GPIO3_CLKCTRL,
		OMAP4430_MODULEMODE_HWCTRL_SHIFT, 0x0, NULL);

DEFINE_CLK_GATE(gpio4_dbclk, "sys_32k_ck", &sys_32k_ck, 0x0,
		OMAP4430_CM_L4PER_GPIO4_CLKCTRL, OMAP4430_OPTFCLKEN_DBCLK_SHIFT,
		0x0, NULL);

DEFINE_CLK_GATE(gpio4_ick, "l4_div_ck", &l4_div_ck, 0x0,
		OMAP4430_CM_L4PER_GPIO4_CLKCTRL,
		OMAP4430_MODULEMODE_HWCTRL_SHIFT, 0x0, NULL);

DEFINE_CLK_GATE(gpio5_dbclk, "sys_32k_ck", &sys_32k_ck, 0x0,
		OMAP4430_CM_L4PER_GPIO5_CLKCTRL, OMAP4430_OPTFCLKEN_DBCLK_SHIFT,
		0x0, NULL);

DEFINE_CLK_GATE(gpio5_ick, "l4_div_ck", &l4_div_ck, 0x0,
		OMAP4430_CM_L4PER_GPIO5_CLKCTRL,
		OMAP4430_MODULEMODE_HWCTRL_SHIFT, 0x0, NULL);

DEFINE_CLK_GATE(gpio6_dbclk, "sys_32k_ck", &sys_32k_ck, 0x0,
		OMAP4430_CM_L4PER_GPIO6_CLKCTRL, OMAP4430_OPTFCLKEN_DBCLK_SHIFT,
		0x0, NULL);

DEFINE_CLK_GATE(gpio6_ick, "l4_div_ck", &l4_div_ck, 0x0,
		OMAP4430_CM_L4PER_GPIO6_CLKCTRL,
		OMAP4430_MODULEMODE_HWCTRL_SHIFT, 0x0, NULL);

DEFINE_CLK_GATE(gpmc_ick, "l3_div_ck", &l3_div_ck, 0x0,
		OMAP4430_CM_L3_2_GPMC_CLKCTRL, OMAP4430_MODULEMODE_HWCTRL_SHIFT,
		0x0, NULL);

static const struct clksel sgx_clk_mux_sel[] = {
	{ .parent = &dpll_core_m7x2_ck, .rates = div_1_0_rates },
	{ .parent = &dpll_per_m7x2_ck, .rates = div_1_1_rates },
	{ .parent = NULL },
};

static const char *gpu_fck_parents[] = {
	"dpll_core_m7x2_ck", "dpll_per_m7x2_ck",
};

/* Merged sgx_clk_mux into gpu */
DEFINE_CLK_OMAP_MUX_GATE(gpu_fck, "l3_gfx_clkdm", sgx_clk_mux_sel,
			 OMAP4430_CM_GFX_GFX_CLKCTRL,
			 OMAP4430_CLKSEL_SGX_FCLK_MASK,
			 OMAP4430_CM_GFX_GFX_CLKCTRL,
			 OMAP4430_MODULEMODE_SWCTRL_SHIFT, NULL,
			 gpu_fck_parents, dmic_fck_ops);

DEFINE_CLK_GATE(hdq1w_fck, "func_12m_fclk", &func_12m_fclk, 0x0,
		OMAP4430_CM_L4PER_HDQ1W_CLKCTRL,
		OMAP4430_MODULEMODE_SWCTRL_SHIFT, 0x0, NULL);

DEFINE_CLK_DIVIDER(hsi_fck, "dpll_per_m2x2_ck", &dpll_per_m2x2_ck, 0x0,
		   OMAP4430_CM_L3INIT_HSI_CLKCTRL, OMAP4430_CLKSEL_24_25_SHIFT,
		   OMAP4430_CLKSEL_24_25_WIDTH, CLK_DIVIDER_POWER_OF_TWO,
		   NULL);

DEFINE_CLK_GATE(i2c1_fck, "func_96m_fclk", &func_96m_fclk, 0x0,
		OMAP4430_CM_L4PER_I2C1_CLKCTRL,
		OMAP4430_MODULEMODE_SWCTRL_SHIFT, 0x0, NULL);

DEFINE_CLK_GATE(i2c2_fck, "func_96m_fclk", &func_96m_fclk, 0x0,
		OMAP4430_CM_L4PER_I2C2_CLKCTRL,
		OMAP4430_MODULEMODE_SWCTRL_SHIFT, 0x0, NULL);

DEFINE_CLK_GATE(i2c3_fck, "func_96m_fclk", &func_96m_fclk, 0x0,
		OMAP4430_CM_L4PER_I2C3_CLKCTRL,
		OMAP4430_MODULEMODE_SWCTRL_SHIFT, 0x0, NULL);

DEFINE_CLK_GATE(i2c4_fck, "func_96m_fclk", &func_96m_fclk, 0x0,
		OMAP4430_CM_L4PER_I2C4_CLKCTRL,
		OMAP4430_MODULEMODE_SWCTRL_SHIFT, 0x0, NULL);

DEFINE_CLK_GATE(ipu_fck, "ducati_clk_mux_ck", &ducati_clk_mux_ck, 0x0,
		OMAP4430_CM_DUCATI_DUCATI_CLKCTRL,
		OMAP4430_MODULEMODE_HWCTRL_SHIFT, 0x0, NULL);

DEFINE_CLK_GATE(iss_ctrlclk, "func_96m_fclk", &func_96m_fclk, 0x0,
		OMAP4430_CM_CAM_ISS_CLKCTRL, OMAP4430_OPTFCLKEN_CTRLCLK_SHIFT,
		0x0, NULL);

DEFINE_CLK_GATE(iss_fck, "ducati_clk_mux_ck", &ducati_clk_mux_ck, 0x0,
		OMAP4430_CM_CAM_ISS_CLKCTRL, OMAP4430_MODULEMODE_SWCTRL_SHIFT,
		0x0, NULL);

DEFINE_CLK_GATE(iva_fck, "dpll_iva_m5x2_ck", &dpll_iva_m5x2_ck, 0x0,
		OMAP4430_CM_IVAHD_IVAHD_CLKCTRL,
		OMAP4430_MODULEMODE_HWCTRL_SHIFT, 0x0, NULL);

DEFINE_CLK_GATE(kbd_fck, "sys_32k_ck", &sys_32k_ck, 0x0,
		OMAP4430_CM_WKUP_KEYBOARD_CLKCTRL,
		OMAP4430_MODULEMODE_SWCTRL_SHIFT, 0x0, NULL);

static struct clk l3_instr_ick;

static const char *l3_instr_ick_parent_names[] = {
	"l3_div_ck",
};

static const struct clk_ops l3_instr_ick_ops = {
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
	.is_enabled	= &omap2_dflt_clk_is_enabled,
	.init		= &omap2_init_clk_clkdm,
};

static struct clk_hw_omap l3_instr_ick_hw = {
	.hw = {
		.clk = &l3_instr_ick,
	},
	.enable_reg	= OMAP4430_CM_L3INSTR_L3_INSTR_CLKCTRL,
	.enable_bit	= OMAP4430_MODULEMODE_HWCTRL_SHIFT,
	.clkdm_name	= "l3_instr_clkdm",
};

DEFINE_STRUCT_CLK(l3_instr_ick, l3_instr_ick_parent_names, l3_instr_ick_ops);

static struct clk l3_main_3_ick;
static struct clk_hw_omap l3_main_3_ick_hw = {
	.hw = {
		.clk = &l3_main_3_ick,
	},
	.enable_reg	= OMAP4430_CM_L3INSTR_L3_3_CLKCTRL,
	.enable_bit	= OMAP4430_MODULEMODE_HWCTRL_SHIFT,
	.clkdm_name	= "l3_instr_clkdm",
};

DEFINE_STRUCT_CLK(l3_main_3_ick, l3_instr_ick_parent_names, l3_instr_ick_ops);

DEFINE_CLK_MUX(mcasp_sync_mux_ck, dmic_sync_mux_ck_parents, NULL, 0x0,
	       OMAP4430_CM1_ABE_MCASP_CLKCTRL,
	       OMAP4430_CLKSEL_INTERNAL_SOURCE_SHIFT,
	       OMAP4430_CLKSEL_INTERNAL_SOURCE_WIDTH, 0x0, NULL);

static const struct clksel func_mcasp_abe_gfclk_sel[] = {
	{ .parent = &mcasp_sync_mux_ck, .rates = div_1_0_rates },
	{ .parent = &pad_clks_ck, .rates = div_1_1_rates },
	{ .parent = &slimbus_clk, .rates = div_1_2_rates },
	{ .parent = NULL },
};

static const char *mcasp_fck_parents[] = {
	"mcasp_sync_mux_ck", "pad_clks_ck", "slimbus_clk",
};

/* Merged func_mcasp_abe_gfclk into mcasp */
DEFINE_CLK_OMAP_MUX_GATE(mcasp_fck, "abe_clkdm", func_mcasp_abe_gfclk_sel,
			 OMAP4430_CM1_ABE_MCASP_CLKCTRL,
			 OMAP4430_CLKSEL_SOURCE_MASK,
			 OMAP4430_CM1_ABE_MCASP_CLKCTRL,
			 OMAP4430_MODULEMODE_SWCTRL_SHIFT, NULL,
			 mcasp_fck_parents, dmic_fck_ops);

DEFINE_CLK_MUX(mcbsp1_sync_mux_ck, dmic_sync_mux_ck_parents, NULL, 0x0,
	       OMAP4430_CM1_ABE_MCBSP1_CLKCTRL,
	       OMAP4430_CLKSEL_INTERNAL_SOURCE_SHIFT,
	       OMAP4430_CLKSEL_INTERNAL_SOURCE_WIDTH, 0x0, NULL);

static const struct clksel func_mcbsp1_gfclk_sel[] = {
	{ .parent = &mcbsp1_sync_mux_ck, .rates = div_1_0_rates },
	{ .parent = &pad_clks_ck, .rates = div_1_1_rates },
	{ .parent = &slimbus_clk, .rates = div_1_2_rates },
	{ .parent = NULL },
};

static const char *mcbsp1_fck_parents[] = {
	"mcbsp1_sync_mux_ck", "pad_clks_ck", "slimbus_clk",
};

/* Merged func_mcbsp1_gfclk into mcbsp1 */
DEFINE_CLK_OMAP_MUX_GATE(mcbsp1_fck, "abe_clkdm", func_mcbsp1_gfclk_sel,
			 OMAP4430_CM1_ABE_MCBSP1_CLKCTRL,
			 OMAP4430_CLKSEL_SOURCE_MASK,
			 OMAP4430_CM1_ABE_MCBSP1_CLKCTRL,
			 OMAP4430_MODULEMODE_SWCTRL_SHIFT, NULL,
			 mcbsp1_fck_parents, dmic_fck_ops);

DEFINE_CLK_MUX(mcbsp2_sync_mux_ck, dmic_sync_mux_ck_parents, NULL, 0x0,
	       OMAP4430_CM1_ABE_MCBSP2_CLKCTRL,
	       OMAP4430_CLKSEL_INTERNAL_SOURCE_SHIFT,
	       OMAP4430_CLKSEL_INTERNAL_SOURCE_WIDTH, 0x0, NULL);

static const struct clksel func_mcbsp2_gfclk_sel[] = {
	{ .parent = &mcbsp2_sync_mux_ck, .rates = div_1_0_rates },
	{ .parent = &pad_clks_ck, .rates = div_1_1_rates },
	{ .parent = &slimbus_clk, .rates = div_1_2_rates },
	{ .parent = NULL },
};

static const char *mcbsp2_fck_parents[] = {
	"mcbsp2_sync_mux_ck", "pad_clks_ck", "slimbus_clk",
};

/* Merged func_mcbsp2_gfclk into mcbsp2 */
DEFINE_CLK_OMAP_MUX_GATE(mcbsp2_fck, "abe_clkdm", func_mcbsp2_gfclk_sel,
			 OMAP4430_CM1_ABE_MCBSP2_CLKCTRL,
			 OMAP4430_CLKSEL_SOURCE_MASK,
			 OMAP4430_CM1_ABE_MCBSP2_CLKCTRL,
			 OMAP4430_MODULEMODE_SWCTRL_SHIFT, NULL,
			 mcbsp2_fck_parents, dmic_fck_ops);

DEFINE_CLK_MUX(mcbsp3_sync_mux_ck, dmic_sync_mux_ck_parents, NULL, 0x0,
	       OMAP4430_CM1_ABE_MCBSP3_CLKCTRL,
	       OMAP4430_CLKSEL_INTERNAL_SOURCE_SHIFT,
	       OMAP4430_CLKSEL_INTERNAL_SOURCE_WIDTH, 0x0, NULL);

static const struct clksel func_mcbsp3_gfclk_sel[] = {
	{ .parent = &mcbsp3_sync_mux_ck, .rates = div_1_0_rates },
	{ .parent = &pad_clks_ck, .rates = div_1_1_rates },
	{ .parent = &slimbus_clk, .rates = div_1_2_rates },
	{ .parent = NULL },
};

static const char *mcbsp3_fck_parents[] = {
	"mcbsp3_sync_mux_ck", "pad_clks_ck", "slimbus_clk",
};

/* Merged func_mcbsp3_gfclk into mcbsp3 */
DEFINE_CLK_OMAP_MUX_GATE(mcbsp3_fck, "abe_clkdm", func_mcbsp3_gfclk_sel,
			 OMAP4430_CM1_ABE_MCBSP3_CLKCTRL,
			 OMAP4430_CLKSEL_SOURCE_MASK,
			 OMAP4430_CM1_ABE_MCBSP3_CLKCTRL,
			 OMAP4430_MODULEMODE_SWCTRL_SHIFT, NULL,
			 mcbsp3_fck_parents, dmic_fck_ops);

static const char *mcbsp4_sync_mux_ck_parents[] = {
	"func_96m_fclk", "per_abe_nc_fclk",
};

DEFINE_CLK_MUX(mcbsp4_sync_mux_ck, mcbsp4_sync_mux_ck_parents, NULL, 0x0,
	       OMAP4430_CM_L4PER_MCBSP4_CLKCTRL,
	       OMAP4430_CLKSEL_INTERNAL_SOURCE_SHIFT,
	       OMAP4430_CLKSEL_INTERNAL_SOURCE_WIDTH, 0x0, NULL);

static const struct clksel per_mcbsp4_gfclk_sel[] = {
	{ .parent = &mcbsp4_sync_mux_ck, .rates = div_1_0_rates },
	{ .parent = &pad_clks_ck, .rates = div_1_1_rates },
	{ .parent = NULL },
};

static const char *mcbsp4_fck_parents[] = {
	"mcbsp4_sync_mux_ck", "pad_clks_ck",
};

/* Merged per_mcbsp4_gfclk into mcbsp4 */
DEFINE_CLK_OMAP_MUX_GATE(mcbsp4_fck, "l4_per_clkdm", per_mcbsp4_gfclk_sel,
			 OMAP4430_CM_L4PER_MCBSP4_CLKCTRL,
			 OMAP4430_CLKSEL_SOURCE_24_24_MASK,
			 OMAP4430_CM_L4PER_MCBSP4_CLKCTRL,
			 OMAP4430_MODULEMODE_SWCTRL_SHIFT, NULL,
			 mcbsp4_fck_parents, dmic_fck_ops);

DEFINE_CLK_GATE(mcpdm_fck, "pad_clks_ck", &pad_clks_ck, 0x0,
		OMAP4430_CM1_ABE_PDM_CLKCTRL, OMAP4430_MODULEMODE_SWCTRL_SHIFT,
		0x0, NULL);

DEFINE_CLK_GATE(mcspi1_fck, "func_48m_fclk", &func_48m_fclk, 0x0,
		OMAP4430_CM_L4PER_MCSPI1_CLKCTRL,
		OMAP4430_MODULEMODE_SWCTRL_SHIFT, 0x0, NULL);

DEFINE_CLK_GATE(mcspi2_fck, "func_48m_fclk", &func_48m_fclk, 0x0,
		OMAP4430_CM_L4PER_MCSPI2_CLKCTRL,
		OMAP4430_MODULEMODE_SWCTRL_SHIFT, 0x0, NULL);

DEFINE_CLK_GATE(mcspi3_fck, "func_48m_fclk", &func_48m_fclk, 0x0,
		OMAP4430_CM_L4PER_MCSPI3_CLKCTRL,
		OMAP4430_MODULEMODE_SWCTRL_SHIFT, 0x0, NULL);

DEFINE_CLK_GATE(mcspi4_fck, "func_48m_fclk", &func_48m_fclk, 0x0,
		OMAP4430_CM_L4PER_MCSPI4_CLKCTRL,
		OMAP4430_MODULEMODE_SWCTRL_SHIFT, 0x0, NULL);

static const struct clksel hsmmc1_fclk_sel[] = {
	{ .parent = &func_64m_fclk, .rates = div_1_0_rates },
	{ .parent = &func_96m_fclk, .rates = div_1_1_rates },
	{ .parent = NULL },
};

static const char *mmc1_fck_parents[] = {
	"func_64m_fclk", "func_96m_fclk",
};

/* Merged hsmmc1_fclk into mmc1 */
DEFINE_CLK_OMAP_MUX_GATE(mmc1_fck, "l3_init_clkdm", hsmmc1_fclk_sel,
			 OMAP4430_CM_L3INIT_MMC1_CLKCTRL, OMAP4430_CLKSEL_MASK,
			 OMAP4430_CM_L3INIT_MMC1_CLKCTRL,
			 OMAP4430_MODULEMODE_SWCTRL_SHIFT, NULL,
			 mmc1_fck_parents, dmic_fck_ops);

/* Merged hsmmc2_fclk into mmc2 */
DEFINE_CLK_OMAP_MUX_GATE(mmc2_fck, "l3_init_clkdm", hsmmc1_fclk_sel,
			 OMAP4430_CM_L3INIT_MMC2_CLKCTRL, OMAP4430_CLKSEL_MASK,
			 OMAP4430_CM_L3INIT_MMC2_CLKCTRL,
			 OMAP4430_MODULEMODE_SWCTRL_SHIFT, NULL,
			 mmc1_fck_parents, dmic_fck_ops);

DEFINE_CLK_GATE(mmc3_fck, "func_48m_fclk", &func_48m_fclk, 0x0,
		OMAP4430_CM_L4PER_MMCSD3_CLKCTRL,
		OMAP4430_MODULEMODE_SWCTRL_SHIFT, 0x0, NULL);

DEFINE_CLK_GATE(mmc4_fck, "func_48m_fclk", &func_48m_fclk, 0x0,
		OMAP4430_CM_L4PER_MMCSD4_CLKCTRL,
		OMAP4430_MODULEMODE_SWCTRL_SHIFT, 0x0, NULL);

DEFINE_CLK_GATE(mmc5_fck, "func_48m_fclk", &func_48m_fclk, 0x0,
		OMAP4430_CM_L4PER_MMCSD5_CLKCTRL,
		OMAP4430_MODULEMODE_SWCTRL_SHIFT, 0x0, NULL);

DEFINE_CLK_GATE(ocp2scp_usb_phy_phy_48m, "func_48m_fclk", &func_48m_fclk, 0x0,
		OMAP4430_CM_L3INIT_USBPHYOCP2SCP_CLKCTRL,
		OMAP4430_OPTFCLKEN_PHY_48M_SHIFT, 0x0, NULL);

DEFINE_CLK_GATE(ocp2scp_usb_phy_ick, "l4_div_ck", &l4_div_ck, 0x0,
		OMAP4430_CM_L3INIT_USBPHYOCP2SCP_CLKCTRL,
		OMAP4430_MODULEMODE_HWCTRL_SHIFT, 0x0, NULL);

static struct clk ocp_wp_noc_ick;

static struct clk_hw_omap ocp_wp_noc_ick_hw = {
	.hw = {
		.clk = &ocp_wp_noc_ick,
	},
	.enable_reg	= OMAP4430_CM_L3INSTR_OCP_WP1_CLKCTRL,
	.enable_bit	= OMAP4430_MODULEMODE_HWCTRL_SHIFT,
	.clkdm_name	= "l3_instr_clkdm",
};

DEFINE_STRUCT_CLK(ocp_wp_noc_ick, l3_instr_ick_parent_names, l3_instr_ick_ops);

DEFINE_CLK_GATE(rng_ick, "l4_div_ck", &l4_div_ck, 0x0,
		OMAP4430_CM_L4SEC_RNG_CLKCTRL, OMAP4430_MODULEMODE_HWCTRL_SHIFT,
		0x0, NULL);

DEFINE_CLK_GATE(sha2md5_fck, "l3_div_ck", &l3_div_ck, 0x0,
		OMAP4430_CM_L4SEC_SHA2MD51_CLKCTRL,
		OMAP4430_MODULEMODE_SWCTRL_SHIFT, 0x0, NULL);

DEFINE_CLK_GATE(sl2if_ick, "dpll_iva_m5x2_ck", &dpll_iva_m5x2_ck, 0x0,
		OMAP4430_CM_IVAHD_SL2_CLKCTRL, OMAP4430_MODULEMODE_HWCTRL_SHIFT,
		0x0, NULL);

DEFINE_CLK_GATE(slimbus1_fclk_1, "func_24m_clk", &func_24m_clk, 0x0,
		OMAP4430_CM1_ABE_SLIMBUS_CLKCTRL,
		OMAP4430_OPTFCLKEN_FCLK1_SHIFT, 0x0, NULL);

DEFINE_CLK_GATE(slimbus1_fclk_0, "abe_24m_fclk", &abe_24m_fclk, 0x0,
		OMAP4430_CM1_ABE_SLIMBUS_CLKCTRL,
		OMAP4430_OPTFCLKEN_FCLK0_SHIFT, 0x0, NULL);

DEFINE_CLK_GATE(slimbus1_fclk_2, "pad_clks_ck", &pad_clks_ck, 0x0,
		OMAP4430_CM1_ABE_SLIMBUS_CLKCTRL,
		OMAP4430_OPTFCLKEN_FCLK2_SHIFT, 0x0, NULL);

DEFINE_CLK_GATE(slimbus1_slimbus_clk, "slimbus_clk", &slimbus_clk, 0x0,
		OMAP4430_CM1_ABE_SLIMBUS_CLKCTRL,
		OMAP4430_OPTFCLKEN_SLIMBUS_CLK_11_11_SHIFT, 0x0, NULL);

DEFINE_CLK_GATE(slimbus1_fck, "ocp_abe_iclk", &ocp_abe_iclk, 0x0,
		OMAP4430_CM1_ABE_SLIMBUS_CLKCTRL,
		OMAP4430_MODULEMODE_SWCTRL_SHIFT, 0x0, NULL);

DEFINE_CLK_GATE(slimbus2_fclk_1, "per_abe_24m_fclk", &per_abe_24m_fclk, 0x0,
		OMAP4430_CM_L4PER_SLIMBUS2_CLKCTRL,
		OMAP4430_OPTFCLKEN_PERABE24M_GFCLK_SHIFT, 0x0, NULL);

DEFINE_CLK_GATE(slimbus2_fclk_0, "func_24mc_fclk", &func_24mc_fclk, 0x0,
		OMAP4430_CM_L4PER_SLIMBUS2_CLKCTRL,
		OMAP4430_OPTFCLKEN_PER24MC_GFCLK_SHIFT, 0x0, NULL);

DEFINE_CLK_GATE(slimbus2_slimbus_clk, "pad_slimbus_core_clks_ck",
		&pad_slimbus_core_clks_ck, 0x0,
		OMAP4430_CM_L4PER_SLIMBUS2_CLKCTRL,
		OMAP4430_OPTFCLKEN_SLIMBUS_CLK_SHIFT, 0x0, NULL);

DEFINE_CLK_GATE(slimbus2_fck, "l4_div_ck", &l4_div_ck, 0x0,
		OMAP4430_CM_L4PER_SLIMBUS2_CLKCTRL,
		OMAP4430_MODULEMODE_SWCTRL_SHIFT, 0x0, NULL);

DEFINE_CLK_GATE(smartreflex_core_fck, "l4_wkup_clk_mux_ck", &l4_wkup_clk_mux_ck,
		0x0, OMAP4430_CM_ALWON_SR_CORE_CLKCTRL,
		OMAP4430_MODULEMODE_SWCTRL_SHIFT, 0x0, NULL);

DEFINE_CLK_GATE(smartreflex_iva_fck, "l4_wkup_clk_mux_ck", &l4_wkup_clk_mux_ck,
		0x0, OMAP4430_CM_ALWON_SR_IVA_CLKCTRL,
		OMAP4430_MODULEMODE_SWCTRL_SHIFT, 0x0, NULL);

DEFINE_CLK_GATE(smartreflex_mpu_fck, "l4_wkup_clk_mux_ck", &l4_wkup_clk_mux_ck,
		0x0, OMAP4430_CM_ALWON_SR_MPU_CLKCTRL,
		OMAP4430_MODULEMODE_SWCTRL_SHIFT, 0x0, NULL);

static const struct clksel dmt1_clk_mux_sel[] = {
	{ .parent = &sys_clkin_ck, .rates = div_1_0_rates },
	{ .parent = &sys_32k_ck, .rates = div_1_1_rates },
	{ .parent = NULL },
};

/* Merged dmt1_clk_mux into timer1 */
DEFINE_CLK_OMAP_MUX_GATE(timer1_fck, "l4_wkup_clkdm", dmt1_clk_mux_sel,
			 OMAP4430_CM_WKUP_TIMER1_CLKCTRL, OMAP4430_CLKSEL_MASK,
			 OMAP4430_CM_WKUP_TIMER1_CLKCTRL,
			 OMAP4430_MODULEMODE_SWCTRL_SHIFT, NULL,
			 abe_dpll_bypass_clk_mux_ck_parents, dmic_fck_ops);

/* Merged cm2_dm10_mux into timer10 */
DEFINE_CLK_OMAP_MUX_GATE(timer10_fck, "l4_per_clkdm", dmt1_clk_mux_sel,
			 OMAP4430_CM_L4PER_DMTIMER10_CLKCTRL,
			 OMAP4430_CLKSEL_MASK,
			 OMAP4430_CM_L4PER_DMTIMER10_CLKCTRL,
			 OMAP4430_MODULEMODE_SWCTRL_SHIFT, NULL,
			 abe_dpll_bypass_clk_mux_ck_parents, dmic_fck_ops);

/* Merged cm2_dm11_mux into timer11 */
DEFINE_CLK_OMAP_MUX_GATE(timer11_fck, "l4_per_clkdm", dmt1_clk_mux_sel,
			 OMAP4430_CM_L4PER_DMTIMER11_CLKCTRL,
			 OMAP4430_CLKSEL_MASK,
			 OMAP4430_CM_L4PER_DMTIMER11_CLKCTRL,
			 OMAP4430_MODULEMODE_SWCTRL_SHIFT, NULL,
			 abe_dpll_bypass_clk_mux_ck_parents, dmic_fck_ops);

/* Merged cm2_dm2_mux into timer2 */
DEFINE_CLK_OMAP_MUX_GATE(timer2_fck, "l4_per_clkdm", dmt1_clk_mux_sel,
			 OMAP4430_CM_L4PER_DMTIMER2_CLKCTRL,
			 OMAP4430_CLKSEL_MASK,
			 OMAP4430_CM_L4PER_DMTIMER2_CLKCTRL,
			 OMAP4430_MODULEMODE_SWCTRL_SHIFT, NULL,
			 abe_dpll_bypass_clk_mux_ck_parents, dmic_fck_ops);

/* Merged cm2_dm3_mux into timer3 */
DEFINE_CLK_OMAP_MUX_GATE(timer3_fck, "l4_per_clkdm", dmt1_clk_mux_sel,
			 OMAP4430_CM_L4PER_DMTIMER3_CLKCTRL,
			 OMAP4430_CLKSEL_MASK,
			 OMAP4430_CM_L4PER_DMTIMER3_CLKCTRL,
			 OMAP4430_MODULEMODE_SWCTRL_SHIFT, NULL,
			 abe_dpll_bypass_clk_mux_ck_parents, dmic_fck_ops);

/* Merged cm2_dm4_mux into timer4 */
DEFINE_CLK_OMAP_MUX_GATE(timer4_fck, "l4_per_clkdm", dmt1_clk_mux_sel,
			 OMAP4430_CM_L4PER_DMTIMER4_CLKCTRL,
			 OMAP4430_CLKSEL_MASK,
			 OMAP4430_CM_L4PER_DMTIMER4_CLKCTRL,
			 OMAP4430_MODULEMODE_SWCTRL_SHIFT, NULL,
			 abe_dpll_bypass_clk_mux_ck_parents, dmic_fck_ops);

static const struct clksel timer5_sync_mux_sel[] = {
	{ .parent = &syc_clk_div_ck, .rates = div_1_0_rates },
	{ .parent = &sys_32k_ck, .rates = div_1_1_rates },
	{ .parent = NULL },
};

static const char *timer5_fck_parents[] = {
	"syc_clk_div_ck", "sys_32k_ck",
};

/* Merged timer5_sync_mux into timer5 */
DEFINE_CLK_OMAP_MUX_GATE(timer5_fck, "abe_clkdm", timer5_sync_mux_sel,
			 OMAP4430_CM1_ABE_TIMER5_CLKCTRL, OMAP4430_CLKSEL_MASK,
			 OMAP4430_CM1_ABE_TIMER5_CLKCTRL,
			 OMAP4430_MODULEMODE_SWCTRL_SHIFT, NULL,
			 timer5_fck_parents, dmic_fck_ops);

/* Merged timer6_sync_mux into timer6 */
DEFINE_CLK_OMAP_MUX_GATE(timer6_fck, "abe_clkdm", timer5_sync_mux_sel,
			 OMAP4430_CM1_ABE_TIMER6_CLKCTRL, OMAP4430_CLKSEL_MASK,
			 OMAP4430_CM1_ABE_TIMER6_CLKCTRL,
			 OMAP4430_MODULEMODE_SWCTRL_SHIFT, NULL,
			 timer5_fck_parents, dmic_fck_ops);

/* Merged timer7_sync_mux into timer7 */
DEFINE_CLK_OMAP_MUX_GATE(timer7_fck, "abe_clkdm", timer5_sync_mux_sel,
			 OMAP4430_CM1_ABE_TIMER7_CLKCTRL, OMAP4430_CLKSEL_MASK,
			 OMAP4430_CM1_ABE_TIMER7_CLKCTRL,
			 OMAP4430_MODULEMODE_SWCTRL_SHIFT, NULL,
			 timer5_fck_parents, dmic_fck_ops);

/* Merged timer8_sync_mux into timer8 */
DEFINE_CLK_OMAP_MUX_GATE(timer8_fck, "abe_clkdm", timer5_sync_mux_sel,
			 OMAP4430_CM1_ABE_TIMER8_CLKCTRL, OMAP4430_CLKSEL_MASK,
			 OMAP4430_CM1_ABE_TIMER8_CLKCTRL,
			 OMAP4430_MODULEMODE_SWCTRL_SHIFT, NULL,
			 timer5_fck_parents, dmic_fck_ops);

/* Merged cm2_dm9_mux into timer9 */
DEFINE_CLK_OMAP_MUX_GATE(timer9_fck, "l4_per_clkdm", dmt1_clk_mux_sel,
			 OMAP4430_CM_L4PER_DMTIMER9_CLKCTRL,
			 OMAP4430_CLKSEL_MASK,
			 OMAP4430_CM_L4PER_DMTIMER9_CLKCTRL,
			 OMAP4430_MODULEMODE_SWCTRL_SHIFT, NULL,
			 abe_dpll_bypass_clk_mux_ck_parents, dmic_fck_ops);

DEFINE_CLK_GATE(uart1_fck, "func_48m_fclk", &func_48m_fclk, 0x0,
		OMAP4430_CM_L4PER_UART1_CLKCTRL,
		OMAP4430_MODULEMODE_SWCTRL_SHIFT, 0x0, NULL);

DEFINE_CLK_GATE(uart2_fck, "func_48m_fclk", &func_48m_fclk, 0x0,
		OMAP4430_CM_L4PER_UART2_CLKCTRL,
		OMAP4430_MODULEMODE_SWCTRL_SHIFT, 0x0, NULL);

DEFINE_CLK_GATE(uart3_fck, "func_48m_fclk", &func_48m_fclk, 0x0,
		OMAP4430_CM_L4PER_UART3_CLKCTRL,
		OMAP4430_MODULEMODE_SWCTRL_SHIFT, 0x0, NULL);

DEFINE_CLK_GATE(uart4_fck, "func_48m_fclk", &func_48m_fclk, 0x0,
		OMAP4430_CM_L4PER_UART4_CLKCTRL,
		OMAP4430_MODULEMODE_SWCTRL_SHIFT, 0x0, NULL);

static struct clk usb_host_fs_fck;

static const char *usb_host_fs_fck_parent_names[] = {
	"func_48mc_fclk",
};

static const struct clk_ops usb_host_fs_fck_ops = {
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
	.is_enabled	= &omap2_dflt_clk_is_enabled,
};

static struct clk_hw_omap usb_host_fs_fck_hw = {
	.hw = {
		.clk = &usb_host_fs_fck,
	},
	.enable_reg	= OMAP4430_CM_L3INIT_USB_HOST_FS_CLKCTRL,
	.enable_bit	= OMAP4430_MODULEMODE_SWCTRL_SHIFT,
	.clkdm_name	= "l3_init_clkdm",
};

DEFINE_STRUCT_CLK(usb_host_fs_fck, usb_host_fs_fck_parent_names,
		  usb_host_fs_fck_ops);

static const char *utmi_p1_gfclk_parents[] = {
	"init_60m_fclk", "xclk60mhsp1_ck",
};

DEFINE_CLK_MUX(utmi_p1_gfclk, utmi_p1_gfclk_parents, NULL, 0x0,
	       OMAP4430_CM_L3INIT_USB_HOST_CLKCTRL,
	       OMAP4430_CLKSEL_UTMI_P1_SHIFT, OMAP4430_CLKSEL_UTMI_P1_WIDTH,
	       0x0, NULL);

DEFINE_CLK_GATE(usb_host_hs_utmi_p1_clk, "utmi_p1_gfclk", &utmi_p1_gfclk, 0x0,
		OMAP4430_CM_L3INIT_USB_HOST_CLKCTRL,
		OMAP4430_OPTFCLKEN_UTMI_P1_CLK_SHIFT, 0x0, NULL);

static const char *utmi_p2_gfclk_parents[] = {
	"init_60m_fclk", "xclk60mhsp2_ck",
};

DEFINE_CLK_MUX(utmi_p2_gfclk, utmi_p2_gfclk_parents, NULL, 0x0,
	       OMAP4430_CM_L3INIT_USB_HOST_CLKCTRL,
	       OMAP4430_CLKSEL_UTMI_P2_SHIFT, OMAP4430_CLKSEL_UTMI_P2_WIDTH,
	       0x0, NULL);

DEFINE_CLK_GATE(usb_host_hs_utmi_p2_clk, "utmi_p2_gfclk", &utmi_p2_gfclk, 0x0,
		OMAP4430_CM_L3INIT_USB_HOST_CLKCTRL,
		OMAP4430_OPTFCLKEN_UTMI_P2_CLK_SHIFT, 0x0, NULL);

DEFINE_CLK_GATE(usb_host_hs_utmi_p3_clk, "init_60m_fclk", &init_60m_fclk, 0x0,
		OMAP4430_CM_L3INIT_USB_HOST_CLKCTRL,
		OMAP4430_OPTFCLKEN_UTMI_P3_CLK_SHIFT, 0x0, NULL);

DEFINE_CLK_GATE(usb_host_hs_hsic480m_p1_clk, "dpll_usb_m2_ck",
		&dpll_usb_m2_ck, 0x0,
		OMAP4430_CM_L3INIT_USB_HOST_CLKCTRL,
		OMAP4430_OPTFCLKEN_HSIC480M_P1_CLK_SHIFT, 0x0, NULL);

DEFINE_CLK_GATE(usb_host_hs_hsic60m_p1_clk, "init_60m_fclk",
		&init_60m_fclk, 0x0,
		OMAP4430_CM_L3INIT_USB_HOST_CLKCTRL,
		OMAP4430_OPTFCLKEN_HSIC60M_P1_CLK_SHIFT, 0x0, NULL);

DEFINE_CLK_GATE(usb_host_hs_hsic60m_p2_clk, "init_60m_fclk",
		&init_60m_fclk, 0x0,
		OMAP4430_CM_L3INIT_USB_HOST_CLKCTRL,
		OMAP4430_OPTFCLKEN_HSIC60M_P2_CLK_SHIFT, 0x0, NULL);

DEFINE_CLK_GATE(usb_host_hs_hsic480m_p2_clk, "dpll_usb_m2_ck",
		&dpll_usb_m2_ck, 0x0,
		OMAP4430_CM_L3INIT_USB_HOST_CLKCTRL,
		OMAP4430_OPTFCLKEN_HSIC480M_P2_CLK_SHIFT, 0x0, NULL);

DEFINE_CLK_GATE(usb_host_hs_func48mclk, "func_48mc_fclk", &func_48mc_fclk, 0x0,
		OMAP4430_CM_L3INIT_USB_HOST_CLKCTRL,
		OMAP4430_OPTFCLKEN_FUNC48MCLK_SHIFT, 0x0, NULL);

DEFINE_CLK_GATE(usb_host_hs_fck, "init_60m_fclk", &init_60m_fclk, 0x0,
		OMAP4430_CM_L3INIT_USB_HOST_CLKCTRL,
		OMAP4430_MODULEMODE_SWCTRL_SHIFT, 0x0, NULL);

static const char *otg_60m_gfclk_parents[] = {
	"utmi_phy_clkout_ck", "xclk60motg_ck",
};

DEFINE_CLK_MUX(otg_60m_gfclk, otg_60m_gfclk_parents, NULL, 0x0,
	       OMAP4430_CM_L3INIT_USB_OTG_CLKCTRL, OMAP4430_CLKSEL_60M_SHIFT,
	       OMAP4430_CLKSEL_60M_WIDTH, 0x0, NULL);

DEFINE_CLK_GATE(usb_otg_hs_xclk, "otg_60m_gfclk", &otg_60m_gfclk, 0x0,
		OMAP4430_CM_L3INIT_USB_OTG_CLKCTRL,
		OMAP4430_OPTFCLKEN_XCLK_SHIFT, 0x0, NULL);

DEFINE_CLK_GATE(usb_otg_hs_ick, "l3_div_ck", &l3_div_ck, 0x0,
		OMAP4430_CM_L3INIT_USB_OTG_CLKCTRL,
		OMAP4430_MODULEMODE_HWCTRL_SHIFT, 0x0, NULL);

DEFINE_CLK_GATE(usb_phy_cm_clk32k, "sys_32k_ck", &sys_32k_ck, 0x0,
		OMAP4430_CM_ALWON_USBPHY_CLKCTRL,
		OMAP4430_OPTFCLKEN_CLK32K_SHIFT, 0x0, NULL);

DEFINE_CLK_GATE(usb_tll_hs_usb_ch2_clk, "init_60m_fclk", &init_60m_fclk, 0x0,
		OMAP4430_CM_L3INIT_USB_TLL_CLKCTRL,
		OMAP4430_OPTFCLKEN_USB_CH2_CLK_SHIFT, 0x0, NULL);

DEFINE_CLK_GATE(usb_tll_hs_usb_ch0_clk, "init_60m_fclk", &init_60m_fclk, 0x0,
		OMAP4430_CM_L3INIT_USB_TLL_CLKCTRL,
		OMAP4430_OPTFCLKEN_USB_CH0_CLK_SHIFT, 0x0, NULL);

DEFINE_CLK_GATE(usb_tll_hs_usb_ch1_clk, "init_60m_fclk", &init_60m_fclk, 0x0,
		OMAP4430_CM_L3INIT_USB_TLL_CLKCTRL,
		OMAP4430_OPTFCLKEN_USB_CH1_CLK_SHIFT, 0x0, NULL);

DEFINE_CLK_GATE(usb_tll_hs_ick, "l4_div_ck", &l4_div_ck, 0x0,
		OMAP4430_CM_L3INIT_USB_TLL_CLKCTRL,
		OMAP4430_MODULEMODE_HWCTRL_SHIFT, 0x0, NULL);

static const struct clk_div_table usim_ck_rates[] = {
	{ .div = 14, .val = 0 },
	{ .div = 18, .val = 1 },
	{ .div = 0 },
};
DEFINE_CLK_DIVIDER_TABLE(usim_ck, "dpll_per_m4x2_ck", &dpll_per_m4x2_ck, 0x0,
			 OMAP4430_CM_WKUP_USIM_CLKCTRL,
			 OMAP4430_CLKSEL_DIV_SHIFT, OMAP4430_CLKSEL_DIV_WIDTH,
			 0x0, usim_ck_rates, NULL);

DEFINE_CLK_GATE(usim_fclk, "usim_ck", &usim_ck, 0x0,
		OMAP4430_CM_WKUP_USIM_CLKCTRL, OMAP4430_OPTFCLKEN_FCLK_SHIFT,
		0x0, NULL);

DEFINE_CLK_GATE(usim_fck, "sys_32k_ck", &sys_32k_ck, 0x0,
		OMAP4430_CM_WKUP_USIM_CLKCTRL, OMAP4430_MODULEMODE_HWCTRL_SHIFT,
		0x0, NULL);

DEFINE_CLK_GATE(wd_timer2_fck, "sys_32k_ck", &sys_32k_ck, 0x0,
		OMAP4430_CM_WKUP_WDT2_CLKCTRL, OMAP4430_MODULEMODE_SWCTRL_SHIFT,
		0x0, NULL);

DEFINE_CLK_GATE(wd_timer3_fck, "sys_32k_ck", &sys_32k_ck, 0x0,
		OMAP4430_CM1_ABE_WDT3_CLKCTRL, OMAP4430_MODULEMODE_SWCTRL_SHIFT,
		0x0, NULL);

/* Remaining optional clocks */
static const char *pmd_stm_clock_mux_ck_parents[] = {
	"sys_clkin_ck", "dpll_core_m6x2_ck", "tie_low_clock_ck",
};

DEFINE_CLK_MUX(pmd_stm_clock_mux_ck, pmd_stm_clock_mux_ck_parents, NULL, 0x0,
	       OMAP4430_CM_EMU_DEBUGSS_CLKCTRL, OMAP4430_PMD_STM_MUX_CTRL_SHIFT,
	       OMAP4430_PMD_STM_MUX_CTRL_WIDTH, 0x0, NULL);

DEFINE_CLK_MUX(pmd_trace_clk_mux_ck, pmd_stm_clock_mux_ck_parents, NULL, 0x0,
	       OMAP4430_CM_EMU_DEBUGSS_CLKCTRL,
	       OMAP4430_PMD_TRACE_MUX_CTRL_SHIFT,
	       OMAP4430_PMD_TRACE_MUX_CTRL_WIDTH, 0x0, NULL);

DEFINE_CLK_DIVIDER(stm_clk_div_ck, "pmd_stm_clock_mux_ck",
		   &pmd_stm_clock_mux_ck, 0x0, OMAP4430_CM_EMU_DEBUGSS_CLKCTRL,
		   OMAP4430_CLKSEL_PMD_STM_CLK_SHIFT,
		   OMAP4430_CLKSEL_PMD_STM_CLK_WIDTH, CLK_DIVIDER_POWER_OF_TWO,
		   NULL);

static const char *trace_clk_div_ck_parents[] = {
	"pmd_trace_clk_mux_ck",
};

static const struct clksel trace_clk_div_div[] = {
	{ .parent = &pmd_trace_clk_mux_ck, .rates = div3_1to4_rates },
	{ .parent = NULL },
};

static struct clk trace_clk_div_ck;

static const struct clk_ops trace_clk_div_ck_ops = {
	.recalc_rate	= &omap2_clksel_recalc,
	.set_rate	= &omap2_clksel_set_rate,
	.round_rate	= &omap2_clksel_round_rate,
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_clkops_enable_clkdm,
	.disable	= &omap2_clkops_disable_clkdm,
};

static struct clk_hw_omap trace_clk_div_ck_hw = {
	.hw = {
		.clk = &trace_clk_div_ck,
	},
	.clkdm_name	= "emu_sys_clkdm",
	.clksel		= trace_clk_div_div,
	.clksel_reg	= OMAP4430_CM_EMU_DEBUGSS_CLKCTRL,
	.clksel_mask	= OMAP4430_CLKSEL_PMD_TRACE_CLK_MASK,
};

DEFINE_STRUCT_CLK(trace_clk_div_ck, trace_clk_div_ck_parents,
		  trace_clk_div_ck_ops);

/* SCRM aux clk nodes */

static const struct clksel auxclk_src_sel[] = {
	{ .parent = &sys_clkin_ck, .rates = div_1_0_rates },
	{ .parent = &dpll_core_m3x2_ck, .rates = div_1_1_rates },
	{ .parent = &dpll_per_m3x2_ck, .rates = div_1_2_rates },
	{ .parent = NULL },
};

static const char *auxclk_src_ck_parents[] = {
	"sys_clkin_ck", "dpll_core_m3x2_ck", "dpll_per_m3x2_ck",
};

static const struct clk_ops auxclk_src_ck_ops = {
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
	.is_enabled	= &omap2_dflt_clk_is_enabled,
	.recalc_rate	= &omap2_clksel_recalc,
	.get_parent	= &omap2_clksel_find_parent_index,
};

DEFINE_CLK_OMAP_MUX_GATE(auxclk0_src_ck, NULL, auxclk_src_sel,
			 OMAP4_SCRM_AUXCLK0, OMAP4_SRCSELECT_MASK,
			 OMAP4_SCRM_AUXCLK0, OMAP4_ENABLE_SHIFT, NULL,
			 auxclk_src_ck_parents, auxclk_src_ck_ops);

DEFINE_CLK_DIVIDER(auxclk0_ck, "auxclk0_src_ck", &auxclk0_src_ck, 0x0,
		   OMAP4_SCRM_AUXCLK0, OMAP4_CLKDIV_SHIFT, OMAP4_CLKDIV_WIDTH,
		   0x0, NULL);

DEFINE_CLK_OMAP_MUX_GATE(auxclk1_src_ck, NULL, auxclk_src_sel,
			 OMAP4_SCRM_AUXCLK1, OMAP4_SRCSELECT_MASK,
			 OMAP4_SCRM_AUXCLK1, OMAP4_ENABLE_SHIFT, NULL,
			 auxclk_src_ck_parents, auxclk_src_ck_ops);

DEFINE_CLK_DIVIDER(auxclk1_ck, "auxclk1_src_ck", &auxclk1_src_ck, 0x0,
		   OMAP4_SCRM_AUXCLK1, OMAP4_CLKDIV_SHIFT, OMAP4_CLKDIV_WIDTH,
		   0x0, NULL);

DEFINE_CLK_OMAP_MUX_GATE(auxclk2_src_ck, NULL, auxclk_src_sel,
			 OMAP4_SCRM_AUXCLK2, OMAP4_SRCSELECT_MASK,
			 OMAP4_SCRM_AUXCLK2, OMAP4_ENABLE_SHIFT, NULL,
			 auxclk_src_ck_parents, auxclk_src_ck_ops);

DEFINE_CLK_DIVIDER(auxclk2_ck, "auxclk2_src_ck", &auxclk2_src_ck, 0x0,
		   OMAP4_SCRM_AUXCLK2, OMAP4_CLKDIV_SHIFT, OMAP4_CLKDIV_WIDTH,
		   0x0, NULL);

DEFINE_CLK_OMAP_MUX_GATE(auxclk3_src_ck, NULL, auxclk_src_sel,
			 OMAP4_SCRM_AUXCLK3, OMAP4_SRCSELECT_MASK,
			 OMAP4_SCRM_AUXCLK3, OMAP4_ENABLE_SHIFT, NULL,
			 auxclk_src_ck_parents, auxclk_src_ck_ops);

DEFINE_CLK_DIVIDER(auxclk3_ck, "auxclk3_src_ck", &auxclk3_src_ck, 0x0,
		   OMAP4_SCRM_AUXCLK3, OMAP4_CLKDIV_SHIFT, OMAP4_CLKDIV_WIDTH,
		   0x0, NULL);

DEFINE_CLK_OMAP_MUX_GATE(auxclk4_src_ck, NULL, auxclk_src_sel,
			 OMAP4_SCRM_AUXCLK4, OMAP4_SRCSELECT_MASK,
			 OMAP4_SCRM_AUXCLK4, OMAP4_ENABLE_SHIFT, NULL,
			 auxclk_src_ck_parents, auxclk_src_ck_ops);

DEFINE_CLK_DIVIDER(auxclk4_ck, "auxclk4_src_ck", &auxclk4_src_ck, 0x0,
		   OMAP4_SCRM_AUXCLK4, OMAP4_CLKDIV_SHIFT, OMAP4_CLKDIV_WIDTH,
		   0x0, NULL);

DEFINE_CLK_OMAP_MUX_GATE(auxclk5_src_ck, NULL, auxclk_src_sel,
			 OMAP4_SCRM_AUXCLK5, OMAP4_SRCSELECT_MASK,
			 OMAP4_SCRM_AUXCLK5, OMAP4_ENABLE_SHIFT, NULL,
			 auxclk_src_ck_parents, auxclk_src_ck_ops);

DEFINE_CLK_DIVIDER(auxclk5_ck, "auxclk5_src_ck", &auxclk5_src_ck, 0x0,
		   OMAP4_SCRM_AUXCLK5, OMAP4_CLKDIV_SHIFT, OMAP4_CLKDIV_WIDTH,
		   0x0, NULL);

static const char *auxclkreq_ck_parents[] = {
	"auxclk0_ck", "auxclk1_ck", "auxclk2_ck", "auxclk3_ck", "auxclk4_ck",
	"auxclk5_ck",
};

DEFINE_CLK_MUX(auxclkreq0_ck, auxclkreq_ck_parents, NULL, 0x0,
	       OMAP4_SCRM_AUXCLKREQ0, OMAP4_MAPPING_SHIFT, OMAP4_MAPPING_WIDTH,
	       0x0, NULL);

DEFINE_CLK_MUX(auxclkreq1_ck, auxclkreq_ck_parents, NULL, 0x0,
	       OMAP4_SCRM_AUXCLKREQ1, OMAP4_MAPPING_SHIFT, OMAP4_MAPPING_WIDTH,
	       0x0, NULL);

DEFINE_CLK_MUX(auxclkreq2_ck, auxclkreq_ck_parents, NULL, 0x0,
	       OMAP4_SCRM_AUXCLKREQ2, OMAP4_MAPPING_SHIFT, OMAP4_MAPPING_WIDTH,
	       0x0, NULL);

DEFINE_CLK_MUX(auxclkreq3_ck, auxclkreq_ck_parents, NULL, 0x0,
	       OMAP4_SCRM_AUXCLKREQ3, OMAP4_MAPPING_SHIFT, OMAP4_MAPPING_WIDTH,
	       0x0, NULL);

DEFINE_CLK_MUX(auxclkreq4_ck, auxclkreq_ck_parents, NULL, 0x0,
	       OMAP4_SCRM_AUXCLKREQ4, OMAP4_MAPPING_SHIFT, OMAP4_MAPPING_WIDTH,
	       0x0, NULL);

DEFINE_CLK_MUX(auxclkreq5_ck, auxclkreq_ck_parents, NULL, 0x0,
	       OMAP4_SCRM_AUXCLKREQ5, OMAP4_MAPPING_SHIFT, OMAP4_MAPPING_WIDTH,
	       0x0, NULL);

/*
 * clkdev
 */

static struct omap_clk omap44xx_clks[] = {
	CLK(NULL,	"extalt_clkin_ck",		&extalt_clkin_ck,	CK_443X),
	CLK(NULL,	"pad_clks_src_ck",		&pad_clks_src_ck,	CK_443X),
	CLK(NULL,	"pad_clks_ck",			&pad_clks_ck,	CK_443X),
	CLK(NULL,	"pad_slimbus_core_clks_ck",	&pad_slimbus_core_clks_ck,	CK_443X),
	CLK(NULL,	"secure_32k_clk_src_ck",	&secure_32k_clk_src_ck,	CK_443X),
	CLK(NULL,	"slimbus_src_clk",		&slimbus_src_clk,	CK_443X),
	CLK(NULL,	"slimbus_clk",			&slimbus_clk,	CK_443X),
	CLK(NULL,	"sys_32k_ck",			&sys_32k_ck,	CK_443X),
	CLK(NULL,	"virt_12000000_ck",		&virt_12000000_ck,	CK_443X),
	CLK(NULL,	"virt_13000000_ck",		&virt_13000000_ck,	CK_443X),
	CLK(NULL,	"virt_16800000_ck",		&virt_16800000_ck,	CK_443X),
	CLK(NULL,	"virt_19200000_ck",		&virt_19200000_ck,	CK_443X),
	CLK(NULL,	"virt_26000000_ck",		&virt_26000000_ck,	CK_443X),
	CLK(NULL,	"virt_27000000_ck",		&virt_27000000_ck,	CK_443X),
	CLK(NULL,	"virt_38400000_ck",		&virt_38400000_ck,	CK_443X),
	CLK(NULL,	"sys_clkin_ck",			&sys_clkin_ck,	CK_443X),
	CLK(NULL,	"tie_low_clock_ck",		&tie_low_clock_ck,	CK_443X),
	CLK(NULL,	"utmi_phy_clkout_ck",		&utmi_phy_clkout_ck,	CK_443X),
	CLK(NULL,	"xclk60mhsp1_ck",		&xclk60mhsp1_ck,	CK_443X),
	CLK(NULL,	"xclk60mhsp2_ck",		&xclk60mhsp2_ck,	CK_443X),
	CLK(NULL,	"xclk60motg_ck",		&xclk60motg_ck,	CK_443X),
	CLK(NULL,	"abe_dpll_bypass_clk_mux_ck",	&abe_dpll_bypass_clk_mux_ck,	CK_443X),
	CLK(NULL,	"abe_dpll_refclk_mux_ck",	&abe_dpll_refclk_mux_ck,	CK_443X),
	CLK(NULL,	"dpll_abe_ck",			&dpll_abe_ck,	CK_443X),
	CLK(NULL,	"dpll_abe_x2_ck",		&dpll_abe_x2_ck,	CK_443X),
	CLK(NULL,	"dpll_abe_m2x2_ck",		&dpll_abe_m2x2_ck,	CK_443X),
	CLK(NULL,	"abe_24m_fclk",			&abe_24m_fclk,	CK_443X),
	CLK(NULL,	"abe_clk",			&abe_clk,	CK_443X),
	CLK(NULL,	"aess_fclk",			&aess_fclk,	CK_443X),
	CLK(NULL,	"dpll_abe_m3x2_ck",		&dpll_abe_m3x2_ck,	CK_443X),
	CLK(NULL,	"core_hsd_byp_clk_mux_ck",	&core_hsd_byp_clk_mux_ck,	CK_443X),
	CLK(NULL,	"dpll_core_ck",			&dpll_core_ck,	CK_443X),
	CLK(NULL,	"dpll_core_x2_ck",		&dpll_core_x2_ck,	CK_443X),
	CLK(NULL,	"dpll_core_m6x2_ck",		&dpll_core_m6x2_ck,	CK_443X),
	CLK(NULL,	"dbgclk_mux_ck",		&dbgclk_mux_ck,	CK_443X),
	CLK(NULL,	"dpll_core_m2_ck",		&dpll_core_m2_ck,	CK_443X),
	CLK(NULL,	"ddrphy_ck",			&ddrphy_ck,	CK_443X),
	CLK(NULL,	"dpll_core_m5x2_ck",		&dpll_core_m5x2_ck,	CK_443X),
	CLK(NULL,	"div_core_ck",			&div_core_ck,	CK_443X),
	CLK(NULL,	"div_iva_hs_clk",		&div_iva_hs_clk,	CK_443X),
	CLK(NULL,	"div_mpu_hs_clk",		&div_mpu_hs_clk,	CK_443X),
	CLK(NULL,	"dpll_core_m4x2_ck",		&dpll_core_m4x2_ck,	CK_443X),
	CLK(NULL,	"dll_clk_div_ck",		&dll_clk_div_ck,	CK_443X),
	CLK(NULL,	"dpll_abe_m2_ck",		&dpll_abe_m2_ck,	CK_443X),
	CLK(NULL,	"dpll_core_m3x2_ck",		&dpll_core_m3x2_ck,	CK_443X),
	CLK(NULL,	"dpll_core_m7x2_ck",		&dpll_core_m7x2_ck,	CK_443X),
	CLK(NULL,	"iva_hsd_byp_clk_mux_ck",	&iva_hsd_byp_clk_mux_ck,	CK_443X),
	CLK(NULL,	"dpll_iva_ck",			&dpll_iva_ck,	CK_443X),
	CLK(NULL,	"dpll_iva_x2_ck",		&dpll_iva_x2_ck,	CK_443X),
	CLK(NULL,	"dpll_iva_m4x2_ck",		&dpll_iva_m4x2_ck,	CK_443X),
	CLK(NULL,	"dpll_iva_m5x2_ck",		&dpll_iva_m5x2_ck,	CK_443X),
	CLK(NULL,	"dpll_mpu_ck",			&dpll_mpu_ck,	CK_443X),
	CLK(NULL,	"dpll_mpu_m2_ck",		&dpll_mpu_m2_ck,	CK_443X),
	CLK(NULL,	"per_hs_clk_div_ck",		&per_hs_clk_div_ck,	CK_443X),
	CLK(NULL,	"per_hsd_byp_clk_mux_ck",	&per_hsd_byp_clk_mux_ck,	CK_443X),
	CLK(NULL,	"dpll_per_ck",			&dpll_per_ck,	CK_443X),
	CLK(NULL,	"dpll_per_m2_ck",		&dpll_per_m2_ck,	CK_443X),
	CLK(NULL,	"dpll_per_x2_ck",		&dpll_per_x2_ck,	CK_443X),
	CLK(NULL,	"dpll_per_m2x2_ck",		&dpll_per_m2x2_ck,	CK_443X),
	CLK(NULL,	"dpll_per_m3x2_ck",		&dpll_per_m3x2_ck,	CK_443X),
	CLK(NULL,	"dpll_per_m4x2_ck",		&dpll_per_m4x2_ck,	CK_443X),
	CLK(NULL,	"dpll_per_m5x2_ck",		&dpll_per_m5x2_ck,	CK_443X),
	CLK(NULL,	"dpll_per_m6x2_ck",		&dpll_per_m6x2_ck,	CK_443X),
	CLK(NULL,	"dpll_per_m7x2_ck",		&dpll_per_m7x2_ck,	CK_443X),
	CLK(NULL,	"usb_hs_clk_div_ck",		&usb_hs_clk_div_ck,	CK_443X),
	CLK(NULL,	"dpll_usb_ck",			&dpll_usb_ck,	CK_443X),
	CLK(NULL,	"dpll_usb_clkdcoldo_ck",	&dpll_usb_clkdcoldo_ck,	CK_443X),
	CLK(NULL,	"dpll_usb_m2_ck",		&dpll_usb_m2_ck,	CK_443X),
	CLK(NULL,	"ducati_clk_mux_ck",		&ducati_clk_mux_ck,	CK_443X),
	CLK(NULL,	"func_12m_fclk",		&func_12m_fclk,	CK_443X),
	CLK(NULL,	"func_24m_clk",			&func_24m_clk,	CK_443X),
	CLK(NULL,	"func_24mc_fclk",		&func_24mc_fclk,	CK_443X),
	CLK(NULL,	"func_48m_fclk",		&func_48m_fclk,	CK_443X),
	CLK(NULL,	"func_48mc_fclk",		&func_48mc_fclk,	CK_443X),
	CLK(NULL,	"func_64m_fclk",		&func_64m_fclk,	CK_443X),
	CLK(NULL,	"func_96m_fclk",		&func_96m_fclk,	CK_443X),
	CLK(NULL,	"init_60m_fclk",		&init_60m_fclk,	CK_443X),
	CLK(NULL,	"l3_div_ck",			&l3_div_ck,	CK_443X),
	CLK(NULL,	"l4_div_ck",			&l4_div_ck,	CK_443X),
	CLK(NULL,	"lp_clk_div_ck",		&lp_clk_div_ck,	CK_443X),
	CLK(NULL,	"l4_wkup_clk_mux_ck",		&l4_wkup_clk_mux_ck,	CK_443X),
	CLK("smp_twd",	NULL,				&mpu_periphclk,	CK_443X),
	CLK(NULL,	"ocp_abe_iclk",			&ocp_abe_iclk,	CK_443X),
	CLK(NULL,	"per_abe_24m_fclk",		&per_abe_24m_fclk,	CK_443X),
	CLK(NULL,	"per_abe_nc_fclk",		&per_abe_nc_fclk,	CK_443X),
	CLK(NULL,	"syc_clk_div_ck",		&syc_clk_div_ck,	CK_443X),
	CLK(NULL,	"aes1_fck",			&aes1_fck,	CK_443X),
	CLK(NULL,	"aes2_fck",			&aes2_fck,	CK_443X),
	CLK(NULL,	"aess_fck",			&aess_fck,	CK_443X),
	CLK(NULL,	"bandgap_fclk",			&bandgap_fclk,	CK_443X),
	CLK(NULL,	"div_ts_ck",			&div_ts_ck,	CK_446X),
	CLK(NULL,	"bandgap_ts_fclk",		&bandgap_ts_fclk,	CK_446X),
	CLK(NULL,	"des3des_fck",			&des3des_fck,	CK_443X),
	CLK(NULL,	"dmic_sync_mux_ck",		&dmic_sync_mux_ck,	CK_443X),
	CLK(NULL,	"dmic_fck",			&dmic_fck,	CK_443X),
	CLK(NULL,	"dsp_fck",			&dsp_fck,	CK_443X),
	CLK(NULL,	"dss_sys_clk",			&dss_sys_clk,	CK_443X),
	CLK(NULL,	"dss_tv_clk",			&dss_tv_clk,	CK_443X),
	CLK(NULL,	"dss_dss_clk",			&dss_dss_clk,	CK_443X),
	CLK(NULL,	"dss_48mhz_clk",		&dss_48mhz_clk,	CK_443X),
	CLK(NULL,	"dss_fck",			&dss_fck,	CK_443X),
	CLK("omapdss_dss",	"ick",			&dss_fck,	CK_443X),
	CLK(NULL,	"efuse_ctrl_cust_fck",		&efuse_ctrl_cust_fck,	CK_443X),
	CLK(NULL,	"emif1_fck",			&emif1_fck,	CK_443X),
	CLK(NULL,	"emif2_fck",			&emif2_fck,	CK_443X),
	CLK(NULL,	"fdif_fck",			&fdif_fck,	CK_443X),
	CLK(NULL,	"fpka_fck",			&fpka_fck,	CK_443X),
	CLK(NULL,	"gpio1_dbclk",			&gpio1_dbclk,	CK_443X),
	CLK(NULL,	"gpio1_ick",			&gpio1_ick,	CK_443X),
	CLK(NULL,	"gpio2_dbclk",			&gpio2_dbclk,	CK_443X),
	CLK(NULL,	"gpio2_ick",			&gpio2_ick,	CK_443X),
	CLK(NULL,	"gpio3_dbclk",			&gpio3_dbclk,	CK_443X),
	CLK(NULL,	"gpio3_ick",			&gpio3_ick,	CK_443X),
	CLK(NULL,	"gpio4_dbclk",			&gpio4_dbclk,	CK_443X),
	CLK(NULL,	"gpio4_ick",			&gpio4_ick,	CK_443X),
	CLK(NULL,	"gpio5_dbclk",			&gpio5_dbclk,	CK_443X),
	CLK(NULL,	"gpio5_ick",			&gpio5_ick,	CK_443X),
	CLK(NULL,	"gpio6_dbclk",			&gpio6_dbclk,	CK_443X),
	CLK(NULL,	"gpio6_ick",			&gpio6_ick,	CK_443X),
	CLK(NULL,	"gpmc_ick",			&gpmc_ick,	CK_443X),
	CLK(NULL,	"gpu_fck",			&gpu_fck,	CK_443X),
	CLK(NULL,	"hdq1w_fck",			&hdq1w_fck,	CK_443X),
	CLK(NULL,	"hsi_fck",			&hsi_fck,	CK_443X),
	CLK(NULL,	"i2c1_fck",			&i2c1_fck,	CK_443X),
	CLK(NULL,	"i2c2_fck",			&i2c2_fck,	CK_443X),
	CLK(NULL,	"i2c3_fck",			&i2c3_fck,	CK_443X),
	CLK(NULL,	"i2c4_fck",			&i2c4_fck,	CK_443X),
	CLK(NULL,	"ipu_fck",			&ipu_fck,	CK_443X),
	CLK(NULL,	"iss_ctrlclk",			&iss_ctrlclk,	CK_443X),
	CLK(NULL,	"iss_fck",			&iss_fck,	CK_443X),
	CLK(NULL,	"iva_fck",			&iva_fck,	CK_443X),
	CLK(NULL,	"kbd_fck",			&kbd_fck,	CK_443X),
	CLK(NULL,	"l3_instr_ick",			&l3_instr_ick,	CK_443X),
	CLK(NULL,	"l3_main_3_ick",		&l3_main_3_ick,	CK_443X),
	CLK(NULL,	"mcasp_sync_mux_ck",		&mcasp_sync_mux_ck,	CK_443X),
	CLK(NULL,	"mcasp_fck",			&mcasp_fck,	CK_443X),
	CLK(NULL,	"mcbsp1_sync_mux_ck",		&mcbsp1_sync_mux_ck,	CK_443X),
	CLK(NULL,	"mcbsp1_fck",			&mcbsp1_fck,	CK_443X),
	CLK(NULL,	"mcbsp2_sync_mux_ck",		&mcbsp2_sync_mux_ck,	CK_443X),
	CLK(NULL,	"mcbsp2_fck",			&mcbsp2_fck,	CK_443X),
	CLK(NULL,	"mcbsp3_sync_mux_ck",		&mcbsp3_sync_mux_ck,	CK_443X),
	CLK(NULL,	"mcbsp3_fck",			&mcbsp3_fck,	CK_443X),
	CLK(NULL,	"mcbsp4_sync_mux_ck",		&mcbsp4_sync_mux_ck,	CK_443X),
	CLK(NULL,	"mcbsp4_fck",			&mcbsp4_fck,	CK_443X),
	CLK(NULL,	"mcpdm_fck",			&mcpdm_fck,	CK_443X),
	CLK(NULL,	"mcspi1_fck",			&mcspi1_fck,	CK_443X),
	CLK(NULL,	"mcspi2_fck",			&mcspi2_fck,	CK_443X),
	CLK(NULL,	"mcspi3_fck",			&mcspi3_fck,	CK_443X),
	CLK(NULL,	"mcspi4_fck",			&mcspi4_fck,	CK_443X),
	CLK(NULL,	"mmc1_fck",			&mmc1_fck,	CK_443X),
	CLK(NULL,	"mmc2_fck",			&mmc2_fck,	CK_443X),
	CLK(NULL,	"mmc3_fck",			&mmc3_fck,	CK_443X),
	CLK(NULL,	"mmc4_fck",			&mmc4_fck,	CK_443X),
	CLK(NULL,	"mmc5_fck",			&mmc5_fck,	CK_443X),
	CLK(NULL,	"ocp2scp_usb_phy_phy_48m",	&ocp2scp_usb_phy_phy_48m,	CK_443X),
	CLK(NULL,	"ocp2scp_usb_phy_ick",		&ocp2scp_usb_phy_ick,	CK_443X),
	CLK(NULL,	"ocp_wp_noc_ick",		&ocp_wp_noc_ick,	CK_443X),
	CLK(NULL,	"rng_ick",			&rng_ick,	CK_443X),
	CLK("omap_rng",	"ick",				&rng_ick,	CK_443X),
	CLK(NULL,	"sha2md5_fck",			&sha2md5_fck,	CK_443X),
	CLK(NULL,	"sl2if_ick",			&sl2if_ick,	CK_443X),
	CLK(NULL,	"slimbus1_fclk_1",		&slimbus1_fclk_1,	CK_443X),
	CLK(NULL,	"slimbus1_fclk_0",		&slimbus1_fclk_0,	CK_443X),
	CLK(NULL,	"slimbus1_fclk_2",		&slimbus1_fclk_2,	CK_443X),
	CLK(NULL,	"slimbus1_slimbus_clk",		&slimbus1_slimbus_clk,	CK_443X),
	CLK(NULL,	"slimbus1_fck",			&slimbus1_fck,	CK_443X),
	CLK(NULL,	"slimbus2_fclk_1",		&slimbus2_fclk_1,	CK_443X),
	CLK(NULL,	"slimbus2_fclk_0",		&slimbus2_fclk_0,	CK_443X),
	CLK(NULL,	"slimbus2_slimbus_clk",		&slimbus2_slimbus_clk,	CK_443X),
	CLK(NULL,	"slimbus2_fck",			&slimbus2_fck,	CK_443X),
	CLK(NULL,	"smartreflex_core_fck",		&smartreflex_core_fck,	CK_443X),
	CLK(NULL,	"smartreflex_iva_fck",		&smartreflex_iva_fck,	CK_443X),
	CLK(NULL,	"smartreflex_mpu_fck",		&smartreflex_mpu_fck,	CK_443X),
	CLK(NULL,	"timer1_fck",			&timer1_fck,	CK_443X),
	CLK(NULL,	"timer10_fck",			&timer10_fck,	CK_443X),
	CLK(NULL,	"timer11_fck",			&timer11_fck,	CK_443X),
	CLK(NULL,	"timer2_fck",			&timer2_fck,	CK_443X),
	CLK(NULL,	"timer3_fck",			&timer3_fck,	CK_443X),
	CLK(NULL,	"timer4_fck",			&timer4_fck,	CK_443X),
	CLK(NULL,	"timer5_fck",			&timer5_fck,	CK_443X),
	CLK(NULL,	"timer6_fck",			&timer6_fck,	CK_443X),
	CLK(NULL,	"timer7_fck",			&timer7_fck,	CK_443X),
	CLK(NULL,	"timer8_fck",			&timer8_fck,	CK_443X),
	CLK(NULL,	"timer9_fck",			&timer9_fck,	CK_443X),
	CLK(NULL,	"uart1_fck",			&uart1_fck,	CK_443X),
	CLK(NULL,	"uart2_fck",			&uart2_fck,	CK_443X),
	CLK(NULL,	"uart3_fck",			&uart3_fck,	CK_443X),
	CLK(NULL,	"uart4_fck",			&uart4_fck,	CK_443X),
	CLK(NULL,	"usb_host_fs_fck",		&usb_host_fs_fck,	CK_443X),
	CLK("usbhs_omap",	"fs_fck",		&usb_host_fs_fck,	CK_443X),
	CLK(NULL,	"utmi_p1_gfclk",		&utmi_p1_gfclk,	CK_443X),
	CLK(NULL,	"usb_host_hs_utmi_p1_clk",	&usb_host_hs_utmi_p1_clk,	CK_443X),
	CLK(NULL,	"utmi_p2_gfclk",		&utmi_p2_gfclk,	CK_443X),
	CLK(NULL,	"usb_host_hs_utmi_p2_clk",	&usb_host_hs_utmi_p2_clk,	CK_443X),
	CLK(NULL,	"usb_host_hs_utmi_p3_clk",	&usb_host_hs_utmi_p3_clk,	CK_443X),
	CLK(NULL,	"usb_host_hs_hsic480m_p1_clk",	&usb_host_hs_hsic480m_p1_clk,	CK_443X),
	CLK(NULL,	"usb_host_hs_hsic60m_p1_clk",	&usb_host_hs_hsic60m_p1_clk,	CK_443X),
	CLK(NULL,	"usb_host_hs_hsic60m_p2_clk",	&usb_host_hs_hsic60m_p2_clk,	CK_443X),
	CLK(NULL,	"usb_host_hs_hsic480m_p2_clk",	&usb_host_hs_hsic480m_p2_clk,	CK_443X),
	CLK(NULL,	"usb_host_hs_func48mclk",	&usb_host_hs_func48mclk,	CK_443X),
	CLK(NULL,	"usb_host_hs_fck",		&usb_host_hs_fck,	CK_443X),
	CLK("usbhs_omap",	"hs_fck",		&usb_host_hs_fck,	CK_443X),
	CLK(NULL,	"otg_60m_gfclk",		&otg_60m_gfclk,	CK_443X),
	CLK(NULL,	"usb_otg_hs_xclk",		&usb_otg_hs_xclk,	CK_443X),
	CLK(NULL,	"usb_otg_hs_ick",		&usb_otg_hs_ick,	CK_443X),
	CLK("musb-omap2430",	"ick",			&usb_otg_hs_ick,	CK_443X),
	CLK(NULL,	"usb_phy_cm_clk32k",		&usb_phy_cm_clk32k,	CK_443X),
	CLK(NULL,	"usb_tll_hs_usb_ch2_clk",	&usb_tll_hs_usb_ch2_clk,	CK_443X),
	CLK(NULL,	"usb_tll_hs_usb_ch0_clk",	&usb_tll_hs_usb_ch0_clk,	CK_443X),
	CLK(NULL,	"usb_tll_hs_usb_ch1_clk",	&usb_tll_hs_usb_ch1_clk,	CK_443X),
	CLK(NULL,	"usb_tll_hs_ick",		&usb_tll_hs_ick,	CK_443X),
	CLK("usbhs_omap",	"usbtll_ick",		&usb_tll_hs_ick,	CK_443X),
	CLK("usbhs_tll",	"usbtll_ick",		&usb_tll_hs_ick,	CK_443X),
	CLK(NULL,	"usim_ck",			&usim_ck,	CK_443X),
	CLK(NULL,	"usim_fclk",			&usim_fclk,	CK_443X),
	CLK(NULL,	"usim_fck",			&usim_fck,	CK_443X),
	CLK(NULL,	"wd_timer2_fck",		&wd_timer2_fck,	CK_443X),
	CLK(NULL,	"wd_timer3_fck",		&wd_timer3_fck,	CK_443X),
	CLK(NULL,	"pmd_stm_clock_mux_ck",		&pmd_stm_clock_mux_ck,	CK_443X),
	CLK(NULL,	"pmd_trace_clk_mux_ck",		&pmd_trace_clk_mux_ck,	CK_443X),
	CLK(NULL,	"stm_clk_div_ck",		&stm_clk_div_ck,	CK_443X),
	CLK(NULL,	"trace_clk_div_ck",		&trace_clk_div_ck,	CK_443X),
	CLK(NULL,	"auxclk0_src_ck",		&auxclk0_src_ck,	CK_443X),
	CLK(NULL,	"auxclk0_ck",			&auxclk0_ck,	CK_443X),
	CLK(NULL,	"auxclkreq0_ck",		&auxclkreq0_ck,	CK_443X),
	CLK(NULL,	"auxclk1_src_ck",		&auxclk1_src_ck,	CK_443X),
	CLK(NULL,	"auxclk1_ck",			&auxclk1_ck,	CK_443X),
	CLK(NULL,	"auxclkreq1_ck",		&auxclkreq1_ck,	CK_443X),
	CLK(NULL,	"auxclk2_src_ck",		&auxclk2_src_ck,	CK_443X),
	CLK(NULL,	"auxclk2_ck",			&auxclk2_ck,	CK_443X),
	CLK(NULL,	"auxclkreq2_ck",		&auxclkreq2_ck,	CK_443X),
	CLK(NULL,	"auxclk3_src_ck",		&auxclk3_src_ck,	CK_443X),
	CLK(NULL,	"auxclk3_ck",			&auxclk3_ck,	CK_443X),
	CLK(NULL,	"auxclkreq3_ck",		&auxclkreq3_ck,	CK_443X),
	CLK(NULL,	"auxclk4_src_ck",		&auxclk4_src_ck,	CK_443X),
	CLK(NULL,	"auxclk4_ck",			&auxclk4_ck,	CK_443X),
	CLK(NULL,	"auxclkreq4_ck",		&auxclkreq4_ck,	CK_443X),
	CLK(NULL,	"auxclk5_src_ck",		&auxclk5_src_ck,	CK_443X),
	CLK(NULL,	"auxclk5_ck",			&auxclk5_ck,	CK_443X),
	CLK(NULL,	"auxclkreq5_ck",		&auxclkreq5_ck,	CK_443X),
	CLK("omap-gpmc",	"fck",			&dummy_ck,	CK_443X),
	CLK("omap_i2c.1",	"ick",			&dummy_ck,	CK_443X),
	CLK("omap_i2c.2",	"ick",			&dummy_ck,	CK_443X),
	CLK("omap_i2c.3",	"ick",			&dummy_ck,	CK_443X),
	CLK("omap_i2c.4",	"ick",			&dummy_ck,	CK_443X),
	CLK(NULL,	"mailboxes_ick",		&dummy_ck,	CK_443X),
	CLK("omap_hsmmc.0",	"ick",			&dummy_ck,	CK_443X),
	CLK("omap_hsmmc.1",	"ick",			&dummy_ck,	CK_443X),
	CLK("omap_hsmmc.2",	"ick",			&dummy_ck,	CK_443X),
	CLK("omap_hsmmc.3",	"ick",			&dummy_ck,	CK_443X),
	CLK("omap_hsmmc.4",	"ick",			&dummy_ck,	CK_443X),
	CLK("omap-mcbsp.1",	"ick",			&dummy_ck,	CK_443X),
	CLK("omap-mcbsp.2",	"ick",			&dummy_ck,	CK_443X),
	CLK("omap-mcbsp.3",	"ick",			&dummy_ck,	CK_443X),
	CLK("omap-mcbsp.4",	"ick",			&dummy_ck,	CK_443X),
	CLK("omap2_mcspi.1",	"ick",			&dummy_ck,	CK_443X),
	CLK("omap2_mcspi.2",	"ick",			&dummy_ck,	CK_443X),
	CLK("omap2_mcspi.3",	"ick",			&dummy_ck,	CK_443X),
	CLK("omap2_mcspi.4",	"ick",			&dummy_ck,	CK_443X),
	CLK(NULL,	"uart1_ick",			&dummy_ck,	CK_443X),
	CLK(NULL,	"uart2_ick",			&dummy_ck,	CK_443X),
	CLK(NULL,	"uart3_ick",			&dummy_ck,	CK_443X),
	CLK(NULL,	"uart4_ick",			&dummy_ck,	CK_443X),
	CLK("usbhs_omap",	"usbhost_ick",		&dummy_ck,		CK_443X),
	CLK("usbhs_omap",	"usbtll_fck",		&dummy_ck,	CK_443X),
	CLK("usbhs_tll",	"usbtll_fck",		&dummy_ck,	CK_443X),
	CLK("omap_wdt",	"ick",				&dummy_ck,	CK_443X),
	CLK(NULL,	"timer_32k_ck",	&sys_32k_ck,	CK_443X),
	/* TODO: Remove "omap_timer.X" aliases once DT migration is complete */
	CLK("omap_timer.1",	"timer_sys_ck",	&sys_clkin_ck,	CK_443X),
	CLK("omap_timer.2",	"timer_sys_ck",	&sys_clkin_ck,	CK_443X),
	CLK("omap_timer.3",	"timer_sys_ck",	&sys_clkin_ck,	CK_443X),
	CLK("omap_timer.4",	"timer_sys_ck",	&sys_clkin_ck,	CK_443X),
	CLK("omap_timer.9",	"timer_sys_ck",	&sys_clkin_ck,	CK_443X),
	CLK("omap_timer.10",	"timer_sys_ck",	&sys_clkin_ck,	CK_443X),
	CLK("omap_timer.11",	"timer_sys_ck",	&sys_clkin_ck,	CK_443X),
	CLK("omap_timer.5",	"timer_sys_ck",	&syc_clk_div_ck,	CK_443X),
	CLK("omap_timer.6",	"timer_sys_ck",	&syc_clk_div_ck,	CK_443X),
	CLK("omap_timer.7",	"timer_sys_ck",	&syc_clk_div_ck,	CK_443X),
	CLK("omap_timer.8",	"timer_sys_ck",	&syc_clk_div_ck,	CK_443X),
	CLK("4a318000.timer",	"timer_sys_ck",	&sys_clkin_ck,	CK_443X),
	CLK("48032000.timer",	"timer_sys_ck",	&sys_clkin_ck,	CK_443X),
	CLK("48034000.timer",	"timer_sys_ck",	&sys_clkin_ck,	CK_443X),
	CLK("48036000.timer",	"timer_sys_ck",	&sys_clkin_ck,	CK_443X),
	CLK("4803e000.timer",	"timer_sys_ck",	&sys_clkin_ck,	CK_443X),
	CLK("48086000.timer",	"timer_sys_ck",	&sys_clkin_ck,	CK_443X),
	CLK("48088000.timer",	"timer_sys_ck",	&sys_clkin_ck,	CK_443X),
	CLK("40138000.timer",	"timer_sys_ck",	&syc_clk_div_ck,	CK_443X),
	CLK("4013a000.timer",	"timer_sys_ck",	&syc_clk_div_ck,	CK_443X),
	CLK("4013c000.timer",	"timer_sys_ck",	&syc_clk_div_ck,	CK_443X),
	CLK("4013e000.timer",	"timer_sys_ck",	&syc_clk_div_ck,	CK_443X),
	CLK(NULL,	"cpufreq_ck",	&dpll_mpu_ck,	CK_443X),
};

static const char *enable_init_clks[] = {
	"emif1_fck",
	"emif2_fck",
	"gpmc_ick",
	"l3_instr_ick",
	"l3_main_3_ick",
	"ocp_wp_noc_ick",
};

static struct rate_init_clks rate_clks[] = {
	{ .name = "dpll_abe_ck", .rate = OMAP4_DPLL_ABE_DEFFREQ },
	{ .name = "dpll_usb_ck", .rate = OMAP4_DPLL_USB_DEFFREQ },
	{ .name = "dpll_usb_m2_ck", .rate = OMAP4_DPLL_USB_DEFFREQ/2 },
	{ .name = "dpll_iva_ck", .rate = OMAP4_DPLL_IVA_DEFFREQ },
	{ .name = "dpll_iva_m4x2_ck", .rate = OMAP4_DSP_ROOT_CLK_NOMFREQ },
	{ .name = "dpll_iva_m5x2_ck", .rate = OMAP4_IVAHD_ROOT_CLK_NOMFREQ },
};

static struct reparent_init_clks reparent_clks[] = {
	{ .name = "abe_dpll_refclk_mux_ck", .parent = "sys_32k_ck" },
};

int __init omap4xxx_clk_init(void)
{
	u32 cpu_clkflg;
	struct omap_clk *c;

	if (cpu_is_omap443x()) {
		cpu_mask = RATE_IN_4430;
		cpu_clkflg = CK_443X;
	} else if (cpu_is_omap446x() || cpu_is_omap447x()) {
		cpu_mask = RATE_IN_4460 | RATE_IN_4430;
		cpu_clkflg = CK_446X | CK_443X;

		if (cpu_is_omap447x())
			pr_warn("WARNING: OMAP4470 clock data incomplete!\n");
	} else {
		return 0;
	}

	for (c = omap44xx_clks; c < omap44xx_clks + ARRAY_SIZE(omap44xx_clks);
									c++) {
		if (c->cpu & cpu_clkflg) {
			clkdev_add(&c->lk);
			if (!__clk_init(NULL, c->lk.clk))
				omap2_init_clk_hw_omap_clocks(c->lk.clk);
		}
	}

	omap2_clk_disable_autoidle_all();

	omap2_clk_reparent_init_clocks(reparent_clks, ARRAY_SIZE(reparent_clks));
	omap2_clk_rate_init_clocks(rate_clks, ARRAY_SIZE(rate_clks));
	omap2_clk_enable_init_clocks(enable_init_clks,
				     ARRAY_SIZE(enable_init_clks));

	return 0;
}
