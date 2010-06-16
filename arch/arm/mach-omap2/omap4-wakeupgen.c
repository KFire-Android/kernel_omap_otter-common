/*
 * OMAP4 Wakeupgen Source file
 *
 * The WakeUpGen unit is responsible for generating wakeup event
 * from the incoming interrupts and enable bits. The WakeUpGen
 * is implemented in MPU Always-On power domain. Only SPI
 * interrupts are wakeup capabale
 *
 * Copyright (C) 2010 Texas Instruments, Inc.
 * Written by Santosh Shilimkar <santosh.shilimkar@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/platform_device.h>

#include <mach/omap4-wakeupgen.h>

/* Wakeupgen Base addres */
void __iomem *wakeupgen_base;

/*
 * Static helper functions
 */
static int __wakeupgen_irq(unsigned int cpu, unsigned int irq, unsigned int set)
{
	unsigned int reg_index, reg_value, spi_irq;

	/*
	 * Subtract the GIC offset
	 */
	spi_irq = irq - 32;

	/*
	 * Not supported on ES1.0 silicon
	 */
	if ((cpu > NR_CPUS) || (omap_rev() == OMAP4430_REV_ES1_0))
		return -EPERM;

	/*
	 * Each wakeup gen register controls 32
	 * interrupts. i.e 1 bit per SPI IRQ
	 */
	switch (spi_irq >> 5) {
	case 0:
		/* IRQ32 to IRQ64 */
		reg_index = 0;
		break;
	case 1:
		/* IRQ64 to IRQ96 */
		reg_index = 1;
		spi_irq -= 32;
		break;
	case 2:
		/* IRQ96 to IRQ128 */
		reg_index = 2;
		spi_irq -= 64;
		break;
	case 3:
		/* IRQ128 to IRQ160 */
		reg_index = 3;
		spi_irq -= 96;
		break;
	default:
		/* Invalid IRQ */
		return -EPERM;
	}

	if (cpu) {
		reg_value = readl(wakeupgen_base + OMAP4_WKG_ENB_A_1
							+ (4 * reg_index));
		if (set)
			reg_value |= (1 << spi_irq);
		else
			reg_value &= ~(1 << spi_irq);
		writel(reg_value, wakeupgen_base + OMAP4_WKG_ENB_A_1
							+ (4 * reg_index));
	} else {
		reg_value = readl(wakeupgen_base + OMAP4_WKG_ENB_A_0
							+ (4 * reg_index));
		if (set)
			reg_value |= (1 << spi_irq);
		else
			reg_value &= ~(1 << spi_irq);
		writel(reg_value, wakeupgen_base + OMAP4_WKG_ENB_A_0
							+ (4 * reg_index));
	}

	return 0;
}

static int __wakeupgen_irq_all(unsigned int cpu, unsigned int reg)
{
	unsigned int reg_index;

	/*
	 * Not supported on ES1.0 silicon
	 */
	if ((cpu > NR_CPUS) || (omap_rev() == OMAP4430_REV_ES1_0))
		return -EPERM;

	for (reg_index = 0; reg_index < 4; reg_index++) {
		if (cpu)
			writel(reg, wakeupgen_base + OMAP4_WKG_ENB_A_1
							+ (4 * reg_index));
		else
			writel(reg, wakeupgen_base + OMAP4_WKG_ENB_A_0
							+ (4 * reg_index));
	}

	return 0;
}

/*
 * Initialse the wakeupgen module
 */
static int __init omap4_wakeupgen_init(void)
{
	/*
	 * To avoid code running on other OMAPs in
	 * multi-omap builds
	 */
	if (!cpu_is_omap44xx())
		return -ENODEV;

	/* Static mapping, never released */
	wakeupgen_base = ioremap(OMAP44XX_WKUPGEN_BASE, SZ_4K);
	BUG_ON(!wakeupgen_base);

	return 0;
}
early_initcall(omap4_wakeupgen_init);


/*
 * Disable the wakeup on 'cpu' from a specific 'irq'
 */
void omap4_wakeupgen_clear_interrupt(unsigned int cpu, unsigned int irq)
{
	if (__wakeupgen_irq(cpu, irq, 0))
		pr_warning("OMAP4: WakeUpGen not supported..\n");
}

/*
 * Enable the wakeup on 'cpu' from a specific 'irq'
 */
void omap4_wakeupgen_set_interrupt(unsigned int cpu, unsigned int irq)
{
	if (__wakeupgen_irq(cpu, irq, 1))
		pr_warning("OMAP4: WakeUpGen not supported..\n");
}

/*
 * Disable the wakeup on 'cpu' from all interrupts
 */
void omap4_wakeupgen_clear_all(unsigned int cpu)
{
	if (__wakeupgen_irq_all(cpu, 0x00000000))
		pr_warning("OMAP4: WakeUpGen not supported..\n");
}

/*
 * Enable the wakeup on 'cpu' from all interrupts
 */
void omap4_wakeupgen_set_all(unsigned int cpu)
{

	if (__wakeupgen_irq_all(cpu, 0xffffffff))
		pr_warning("OMAP4: WakeUpGen not supported..\n");
}

