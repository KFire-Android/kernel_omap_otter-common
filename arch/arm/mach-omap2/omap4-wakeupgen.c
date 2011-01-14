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
#include <mach/omap4-common.h>

/*
 * WakeUpGen save restore offset from SAR_BANK3
 */
#define WAKEUPGENENB_OFFSET_CPU0		0x684
#define WAKEUPGENENB_SECURE_OFFSET_CPU0		0x694
#define WAKEUPGENENB_OFFSET_CPU1		0x6A4
#define WAKEUPGENENB_SECURE_OFFSET_CPU1		0x6B4
#define AUXCOREBOOT0_OFFSET			0x6C4
#define AUXCOREBOOT1_OFFSET			0x6C8
#define PTMSYNCREQ_MASK_OFFSET			0x6CC
#define PTMSYNCREQ_EN_OFFSET			0x6D0
#define SAR_BACKUP_STATUS_OFFSET		0x500
#define SAR_BACKUP_STATUS_WAKEUPGEN		0x10
#define SECURE_L3FW_IRQ_MASK			0xfffffeff

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

	for (reg_index = 1; reg_index < 4; reg_index++) {
		if (cpu)
			writel(reg, wakeupgen_base + OMAP4_WKG_ENB_A_1
							+ (4 * reg_index));
		else
			writel(reg, wakeupgen_base + OMAP4_WKG_ENB_A_0
							+ (4 * reg_index));
	}

	/*
	 * Maintain the Secure and L3 interrupt state which can potentialy
	 * gate low power state
	 */
	if (cpu)
		writel((reg & SECURE_L3FW_IRQ_MASK),
				wakeupgen_base + OMAP4_WKG_ENB_A_1);
	else
		writel((reg & SECURE_L3FW_IRQ_MASK),
				wakeupgen_base + OMAP4_WKG_ENB_A_0);
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

	/*
	 * Disable Secure and L3 interrupts which can potentialy
	 * gate low power state
	 */
	writel(SECURE_L3FW_IRQ_MASK, wakeupgen_base + OMAP4_WKG_ENB_A_0);
	writel(SECURE_L3FW_IRQ_MASK, wakeupgen_base + OMAP4_WKG_ENB_A_1);

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

/*
 * Save WakewupGen context in SAR RAM3. Restore is done by ROM code.
 * WakeupGen is lost only when DEVICE hits OFF. Though the register
 * context is retained in MPU OFF state, hw recommondation is to
 * save/restore WakeupGen along with GIC to have consistent interrupt
 * state at both the blocks.
 * During normal operation, WakeUpGen delivers external interrupts
 * directly to the GIC. When the CPU asserts StandbyWFI, indicating
 * it wants to enter lowpower state, the Standby Controller checks
 * with the WakeUpGen unit using the idlereq/idleack handshake to make
 * sure there is no incoming interrupts.
 */

void omap4_wakeupgen_save(void)
{
	u32 reg_value, reg_index;
	void __iomem *sar_bank3_base;

	/*
	 * WakewupGen needs to be saved in SAR_BANK3
	 */
	sar_bank3_base = sar_ram_base + SAR_BANK3_OFFSET;

	for (reg_index = 0; reg_index < 0x4 ; reg_index++) {
		/*
		 * Save the CPU0 Wakeup Enable for Interrupts 0 to 127
		 */
		reg_value = readl(wakeupgen_base + OMAP4_WKG_ENB_A_0
							+ (4 * reg_index));
		writel(reg_value, sar_bank3_base +
				WAKEUPGENENB_OFFSET_CPU0 + (4 * reg_index));
		/*
		 * Force to 0x0 CPU0 isecure Wakeup Enable Interrupts
		 */
		writel(0x0, sar_bank3_base +
			WAKEUPGENENB_SECURE_OFFSET_CPU0 + (4 * reg_index));
		/*
		 * Save the CPU1 Wakeup Enable for Interrupts 0 to 127
		 */
		reg_value = readl(wakeupgen_base + OMAP4_WKG_ENB_A_1
							+ (4 * reg_index));
		writel(reg_value, sar_bank3_base +
				WAKEUPGENENB_OFFSET_CPU1 + (4 * reg_index));
		/*
		 * Force to 0x0 CPU1 secure Wakeup Enable Interrupts
		 */
		writel(0x0, sar_bank3_base +
			WAKEUPGENENB_SECURE_OFFSET_CPU1 + (4 * reg_index));
	}

	/*
	 * Save AuxBoot registers
	 */
	reg_value = readl(wakeupgen_base + OMAP4_AUX_CORE_BOOT_0);
	writel(reg_value, sar_bank3_base + AUXCOREBOOT0_OFFSET);
	reg_value = readl(wakeupgen_base + OMAP4_AUX_CORE_BOOT_0);
	writel(reg_value, sar_bank3_base + AUXCOREBOOT1_OFFSET);

	/*
	 * SyncReq generation logic
	 */
	reg_value = readl(wakeupgen_base + OMAP4_PTMSYNCREQ_MASK);
	writel(reg_value, sar_bank3_base + PTMSYNCREQ_MASK_OFFSET);
	reg_value = readl(wakeupgen_base + OMAP4_PTMSYNCREQ_EN);
	writel(reg_value, sar_bank3_base + PTMSYNCREQ_EN_OFFSET);

	/*
	 * Set the Backup Bit Mask status
	 */
	reg_value = readl(sar_bank3_base + SAR_BACKUP_STATUS_OFFSET);
	reg_value |= SAR_BACKUP_STATUS_WAKEUPGEN;
	writel(reg_value, sar_bank3_base + SAR_BACKUP_STATUS_OFFSET);
}

void omap4_wakeupgen_restore(void)
{
	u32 reg_value, reg_index;
	void __iomem *sar_bank3_base;

	/*
	 * WakewupGen needs to be saved in SAR_BANK3
	 */
	sar_bank3_base = sar_ram_base + SAR_BANK3_OFFSET;

	for (reg_index = 0; reg_index < 0x4 ; reg_index++) {
		/*
		 * Save the CPU0/CPU1 Wakeup Enable for Interrupts 0 to 127
		 */
		reg_value =  readl(sar_bank3_base +
				WAKEUPGENENB_OFFSET_CPU0 + (4 * reg_index));
		writel(reg_value, wakeupgen_base + OMAP4_WKG_ENB_A_0
							+ (4 * reg_index));
		reg_value = readl(sar_bank3_base +
				WAKEUPGENENB_OFFSET_CPU1 + (4 * reg_index));
		writel(reg_value, wakeupgen_base + OMAP4_WKG_ENB_A_1
							+ (4 * reg_index));
	}

	/*
	 * Save AuxBoot registers
	 */
	reg_value =  readl(sar_bank3_base + AUXCOREBOOT0_OFFSET);
	writel(reg_value, wakeupgen_base + OMAP4_AUX_CORE_BOOT_0);
	reg_value = readl(sar_bank3_base + AUXCOREBOOT1_OFFSET);
	writel(reg_value, wakeupgen_base + OMAP4_AUX_CORE_BOOT_0);

	/*
	 * SyncReq generation logic
	 */
	reg_value = readl(sar_bank3_base + PTMSYNCREQ_MASK_OFFSET);
	writel(reg_value, wakeupgen_base + OMAP4_PTMSYNCREQ_MASK);
	reg_value = readl(sar_bank3_base + PTMSYNCREQ_EN_OFFSET);
	writel(reg_value, wakeupgen_base + OMAP4_PTMSYNCREQ_EN);
}
