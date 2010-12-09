/*
 * OMAP4 Save Restore source file
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
#include <linux/clk.h>

#include <mach/omap4-common.h>
#include <plat/clockdomain.h>
#include <plat/control.h>

#include "omap4-sar-layout.h"
#include "cm-regbits-44xx.h"

/*
 * These SECUTE control registers are used to work-around
 * DDR corruption on the second chip select.
 */
#define OMAP4_CTRL_SECURE_EMIF1_SDRAM_CONFIG2_REG	0x0114
#define OMAP4_CTRL_SECURE_EMIF2_SDRAM_CONFIG2_REG	0x011c

void __iomem *sar_ram_base;
static void __iomem *omap4_sar_modules[MAX_SAR_MODULES];
static struct powerdomain *l3init_pwrdm;
static struct clk *usb_host_ck, *usb_tll_ck;

/*
 * omap_sar_save :
 * common routine to save the registers to  SAR RAM with the
 * given parameters
 * @nb_regs - Number of registers to saved
 * @sar_bank_offset - where to backup
 * @sar_layout - constant table containing the backup info
 */
static void sar_save(u32 nb_regs, u32 sar_bank,
					const u32 sar_layout_table[][4])
{
	u32 reg_val, size, i, j;
	void __iomem *reg_read_addr, *sar_wr_addr;

	for (i = 0; i < nb_regs; i++) {
		if (omap4_sar_modules[(sar_layout_table[i][MODULE_ADDR_IDX])]) {
			size = sar_layout_table[i][MODULE_NB_REGS_IDX];
			reg_read_addr =
			omap4_sar_modules[sar_layout_table[i][MODULE_ADDR_IDX]]
			+ sar_layout_table[i][MODULE_OFFSET_IDX];
			sar_wr_addr = sar_ram_base + sar_bank +
				sar_layout_table[i][SAR_RAM_OFFSET_IDX];
			for (j = 0; j < size; j++) {
				reg_val = __raw_readl(reg_read_addr + j * 4);
				__raw_writel(reg_val, sar_wr_addr + j * 4);
			}
		}
	}
}

static void save_sar_bank3(void)
{
	struct clockdomain *l4_secure_clkdm;

	/*
	 * Not supported on ES1.0 silicon
	 */
	if (omap_rev() == OMAP4430_REV_ES1_0) {
		WARN_ONCE(1, "omap4: SAR backup not supported on ES1.0 ..\n");
		return;
	}

	l4_secure_clkdm = clkdm_lookup("l4_secure_clkdm");
	omap2_clkdm_wakeup(l4_secure_clkdm);

	sar_save(NB_REGS_CONST_SETS_RAM3_HW, SAR_BANK3_OFFSET, sar_ram3_layout);

	omap2_clkdm_allow_idle(l4_secure_clkdm);
}

 /*
  * omap4_sar_save -
  * Save the context to SAR_RAM1 and SAR_RAM2 as per
  * sar_ram1_layout and sar_ram2_layout for the device OFF mode
  */
void omap4_sar_save(void)
{
	/*
	 * Not supported on ES1.0 silicon
	 */
	if (omap_rev() == OMAP4430_REV_ES1_0) {
		WARN_ONCE(1, "omap4: SAR backup not supported on ES1.0 ..\n");
		return;
	}

	/*
	 * SAR bits and clocks needs to be enabled
	 */
	pwrdm_enable_hdwr_sar(l3init_pwrdm);
	clk_enable(usb_host_ck);
	clk_enable(usb_tll_ck);

	/* Save SAR BANK1 */
	sar_save(NB_REGS_CONST_SETS_RAM1_HW, SAR_BANK1_OFFSET, sar_ram1_layout);

	pwrdm_disable_hdwr_sar(l3init_pwrdm);
	clk_disable(usb_host_ck);
	clk_disable(usb_tll_ck);

	/* Save SAR BANK2 */
	sar_save(NB_REGS_CONST_SETS_RAM2_HW, SAR_BANK2_OFFSET, sar_ram2_layout);
}
/**
 * omap4_sar_overwrite :
 * This API overwrite some of the SAR locations as a special cases
 * The register content to be saved can be the register value before
 * going into OFF-mode or a value that is required on wake up. This means
 * that the restored register value can be different from the last value
 * of the register before going into OFF-mode
 *	- CM1 and CM2 configuration
 *		Bits 0 of the CM_SHADOW_FREQ_CONFIG1 regiser and the
 *		CM_SHADOW_FREQ_CONFIG2 register are self-clearing and must
 *		 be set at restore time. Thus, these data must always be
 *		overwritten in the SAR RAM.
 *	- Because USBHOSTHS and USBTLL restore needs a particular
 *		sequencing, the software must overwrite data read from
 *		the following registers implied in phase2a and phase 2b
 */
void omap4_sar_overwrite(void)
{
	u32 val = 0;


	/* Overwriting Phase1 data to be restored */
	/* CM2 MEMIF_CLKTRCTRL = SW_WKUP, before FREQ UPDATE*/
	__raw_writel(0x2, sar_ram_base + SAR_BANK1_OFFSET + 0xd0);
	/* CM1 CM_SHADOW_FREQ_CONFIG2, Enable FREQ UPDATE */
	val = __raw_readl(OMAP4430_CM_SHADOW_FREQ_CONFIG2);
	/*
	 * FIXME: Implement FREQ UPDATE for L#/M5 before enabling this
	 * val |= 1 << OMAP4430_FREQ_UPDATE_SHIFT;
	 */
	__raw_writel(val, sar_ram_base + SAR_BANK1_OFFSET + 0x100);
	/* CM1 CM_SHADOW_FREQ_CONFIG1, Enable FREQ UPDATE */
	val = __raw_readl(OMAP4430_CM_SHADOW_FREQ_CONFIG1);
	val |= 1 << OMAP4430_FREQ_UPDATE_SHIFT;
	val &= ~OMAP4430_DLL_OVERRIDE_MASK;
	__raw_writel(val, sar_ram_base + SAR_BANK1_OFFSET + 0x104);
	/* CM2 MEMIF_CLKTRCTRL = HW_AUTO, after FREQ UPDATE*/
	__raw_writel(0x3, sar_ram_base + SAR_BANK1_OFFSET + 0x124);

	/* Overwriting Phase2a data to be restored */
	/* CM_L3INIT_USB_HOST_CLKCTRL: SAR_MODE = 1, MODULEMODE = 2 */
	__raw_writel(0x00000012, sar_ram_base + SAR_BANK1_OFFSET + 0x2ec);
	/* CM_L3INIT_USB_TLL_CLKCTRL: SAR_MODE = 1, MODULEMODE = 1 */
	__raw_writel(0x00000011, sar_ram_base + SAR_BANK1_OFFSET + 0x2f0);
	/* CM2 CM_SDMA_STATICDEP : Enable static depedency for SAR modules */
	__raw_writel(0x000090e8, sar_ram_base + SAR_BANK1_OFFSET + 0x2f4);

	/* Overwriting Phase2b data to be restored */
	/* CM_L3INIT_USB_HOST_CLKCTRL: SAR_MODE = 0, MODULEMODE = 0 */
	val = __raw_readl(OMAP4430_CM_L3INIT_USB_HOST_CLKCTRL);
	val &= (OMAP4430_CLKSEL_UTMI_P1_MASK | OMAP4430_CLKSEL_UTMI_P2_MASK);
	__raw_writel(val, sar_ram_base + SAR_BANK1_OFFSET + 0x91c);
	/* CM_L3INIT_USB_TLL_CLKCTRL: SAR_MODE = 0, MODULEMODE = 0 */
	__raw_writel(0x0000000, sar_ram_base + SAR_BANK1_OFFSET + 0x920);
	/* CM2 CM_SDMA_STATICDEP : Clear the static depedency */
	__raw_writel(0x00000040, sar_ram_base + SAR_BANK1_OFFSET + 0x924);

	/* readback to ensure data reaches to SAR RAM */
	barrier();
	val = __raw_readl(sar_ram_base + SAR_BANK1_OFFSET + 0x924);
}

/*
 * SAR RAM used to save and restore the HW
 * context in low power modes
 */
static int __init omap4_sar_ram_init(void)
{
	void __iomem *secure_ctrl_mod;

	/*
	 * To avoid code running on other OMAPs in
	 * multi-omap builds
	 */
	if (!cpu_is_omap44xx())
		return -ENODEV;

	/* Static mapping, never released */
	sar_ram_base = ioremap(OMAP44XX_SAR_RAM_BASE, SZ_8K);
	BUG_ON(!sar_ram_base);

	/*
	 *  All these are static mappings so ioremap() will
	 *  just return with mapped VA
	 */
	omap4_sar_modules[EMIF1_INDEX] = ioremap(OMAP44XX_EMIF1, SZ_1M);
	BUG_ON(!omap4_sar_modules[EMIF1_INDEX]);
	omap4_sar_modules[EMIF2_INDEX] = ioremap(OMAP44XX_EMIF2, SZ_1M);
	BUG_ON(!omap4_sar_modules[EMIF2_INDEX]);
	omap4_sar_modules[DMM_INDEX] = ioremap(OMAP44XX_DMM_BASE, SZ_1M);
	BUG_ON(!omap4_sar_modules[DMM_INDEX]);
	omap4_sar_modules[CM1_INDEX] = ioremap(OMAP4430_CM1_BASE, SZ_8K);
	BUG_ON(!omap4_sar_modules[CM1_INDEX]);
	omap4_sar_modules[CM2_INDEX] = ioremap(OMAP4430_CM2_BASE, SZ_8K);
	BUG_ON(!omap4_sar_modules[CM2_INDEX]);
	omap4_sar_modules[C2C_INDEX] = ioremap(OMAP44XX_C2C_BASE, SZ_1M);
	BUG_ON(!omap4_sar_modules[C2C_INDEX]);
	omap4_sar_modules[CTRL_MODULE_PAD_CORE_INDEX] =
					ioremap(OMAP443X_CTRL_BASE, SZ_4K);
	BUG_ON(!omap4_sar_modules[CTRL_MODULE_PAD_CORE_INDEX]);
	omap4_sar_modules[L3_CLK1_INDEX] = ioremap(L3_44XX_BASE_CLK1, SZ_1M);
	BUG_ON(!omap4_sar_modules[L3_CLK1_INDEX]);
	omap4_sar_modules[L3_CLK2_INDEX] = ioremap(L3_44XX_BASE_CLK2, SZ_1M);
	BUG_ON(!omap4_sar_modules[L3_CLK2_INDEX]);
	omap4_sar_modules[L3_CLK3_INDEX] = ioremap(L3_44XX_BASE_CLK3, SZ_1M);
	BUG_ON(!omap4_sar_modules[L3_CLK3_INDEX]);
	omap4_sar_modules[USBTLL_INDEX] = ioremap(OMAP44XX_USBTLL_BASE, SZ_1M);
	BUG_ON(!omap4_sar_modules[USBTLL_INDEX]);
	omap4_sar_modules[UHH_INDEX] = ioremap(OMAP44XX_UHH_CONFIG_BASE, SZ_1M);
	BUG_ON(!omap4_sar_modules[UHH_INDEX]);
	omap4_sar_modules[L4CORE_INDEX] = ioremap(L4_44XX_PHYS, SZ_4M);
	BUG_ON(!omap4_sar_modules[L4CORE_INDEX]);
	omap4_sar_modules[L4PER_INDEX] = ioremap(L4_PER_44XX_PHYS, SZ_4M);
	BUG_ON(!omap4_sar_modules[L4PER_INDEX]);

	/*
	 * SAR BANK3 contains all firewall settings and it's saved through
	 * secure API on HS device. On GP device these registers are
	 * meaningless but still needs to be saved. Otherwise Auto-restore
	 * phase DMA takes an abort. Hence save these conents only once
	 * in init to avoid the issue while waking up from device OFF
	 */
	if (omap_type() == OMAP2_DEVICE_TYPE_GP)
		save_sar_bank3();
	/*
	 * Overwrite EMIF1/EMIF2
	 * SECURE_EMIF1_SDRAM_CONFIG2_REG
	 * SECURE_EMIF2_SDRAM_CONFIG2_REG
	 */
	secure_ctrl_mod = ioremap(OMAP4_CTRL_MODULE_WKUP, SZ_4K);
	BUG_ON(!secure_ctrl_mod);
	__raw_writel(0x10,
		secure_ctrl_mod + OMAP4_CTRL_SECURE_EMIF1_SDRAM_CONFIG2_REG);
	__raw_writel(0x10,
	secure_ctrl_mod + OMAP4_CTRL_SECURE_EMIF2_SDRAM_CONFIG2_REG);
	wmb();
	iounmap(secure_ctrl_mod);

	/*
	 * L3INIT PD and clocks are needed for SAR save phase
	 */
	l3init_pwrdm = pwrdm_lookup("l3init_pwrdm");
	if (!l3init_pwrdm)
		pr_err("Failed to get l3init_pwrdm\n");

	usb_host_ck = clk_get(NULL, "usb_host_hs_fck");
	if (!usb_host_ck)
		pr_err("Could not get usb_host_ck\n");

	usb_tll_ck = clk_get(NULL, "usb_tll_hs_ick");
	if (!usb_tll_ck)
		pr_err("Could not get usb_tll_ck\n");

	return 0;
}
early_initcall(omap4_sar_ram_init);
