/*
 * OMAP5 clock function prototypes and macros
 *
 * Copyright (C) 20011 Texas Instruments, Inc.
 */

#ifndef __ARCH_ARM_MACH_OMAP2_CLOCK54XX_H
#define __ARCH_ARM_MACH_OMAP2_CLOCK54XX_H

#define OMAP54XX_MAX_DPLL_MULT 2047
#define OMAP54XX_MAX_DPLL_DIV  128

int omap5xxx_clk_init(void);
int omap5xxx_custom_clk_init(void);
int omap_virt_l3_set_rate(struct clk *clk, unsigned long rate);
long omap_virt_l3_round_rate(struct clk *clk, unsigned long rate);
unsigned long omap_virt_l3_recalc(struct clk *clk);
int omap_virt_iva_set_rate(struct clk *clk, unsigned long rate);
long omap_virt_iva_round_rate(struct clk *clk, unsigned long rate);
int omap_virt_dsp_set_rate(struct clk *clk, unsigned long rate);
long omap_virt_dsp_round_rate(struct clk *clk, unsigned long rate);
#endif
