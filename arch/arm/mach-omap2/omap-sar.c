/*
 * OMAP4 Save Restore source file
 *
 * Copyright (C) 2010 Texas Instruments, Inc.
 * Santosh Shilimkar <santosh.shilimkar@ti.com>
 * Tero Kristo <t-kristo@ti.com>
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
#include <linux/slab.h>

#include <mach/ctrl_module_wkup_44xx.h>

#include "iomap.h"
#include "pm.h"
#include "clockdomain.h"
#include "common.h"

#include "omap4-sar-layout.h"
#include "cm-regbits-44xx.h"
#include "cm-regbits-34xx.h"
#include "prcm44xx.h"
#include "cminst44xx.h"

#include <plat/usb.h>

static void __iomem *sar_ram_base;
static struct powerdomain *l3init_pwrdm;
static struct clockdomain *l3init_clkdm;
static struct omap_hwmod *uhh_hwm, *tll_hwm;

#define SZ_CM_USBHS	0xc
#define OMAP44XX_CM2_USBHS_OFFSET	0x1e54

#ifdef CONFIG_ARCH_OMAP5_ES1
#define OMAP55XX_CM_USBHS_OFFSET	0x1e60
#else
#define OMAP55XX_CM_USBHS_OFFSET	0x1e64
#endif

/**
 * struct sar_ram_entry - SAR RAM layout descriptor
 * @io_base: IO base address for the entry
 * @offset: IO offset from @io_base
 * @size: size of entry in words, size=0 marks end of descriptor array
 * @ram_addr: SAR RAM address to store the data to
 * @flags: flags for the entry
 * @mod_func: function pointer relevant when the flags had "SAR_SAVE_COND"
 */
struct sar_ram_entry {
	void __iomem *io_base;
	u32 offset;
	u32 size;
	u32 ram_addr;
	u32 flags;
	int (*mod_func)(void);
};

/**
 * struct sar_overwrite_entry - SAR RAM overwrite descriptor
 * @reg_addr: register address
 * @sar_offset: offset in SAR RAM
 * @valid: whether data for this entry was found or not from SAR ROM
 */
struct sar_overwrite_entry {
	u32 reg_addr;
	u32 sar_offset;
	bool valid;
};

enum {
	MEMIF_CLKSTCTRL_IDX,
	SHADOW_FREQ_CFG2_IDX,
	SHADOW_FREQ_CFG1_IDX,
	MEMIF_CLKSTCTRL_2_IDX,
	HSUSBHOST_CLKCTRL_IDX,
	HSUSBHOST_CLKCTRL_2_IDX,
	L3INIT_CLKSTCTRL_IDX,
	OW_IDX_SIZE
};

/* SAR flags, used with entries */
#define SAR_SAVE_COND	(1 << 0)
#define SAR_INVALID	(1 << 1)

static struct sar_ram_entry *sar_ram_layout[3];
static struct sar_overwrite_entry *sar_overwrite_data;

static struct sar_overwrite_entry omap4_sar_overwrite_data[OW_IDX_SIZE] = {
	[MEMIF_CLKSTCTRL_IDX] = { .reg_addr = 0x4a009e0c },
	[SHADOW_FREQ_CFG2_IDX] = { .reg_addr = 0x4a004e2c },
	[SHADOW_FREQ_CFG1_IDX] = { .reg_addr = 0x4a004e30 },
	[MEMIF_CLKSTCTRL_2_IDX] = { .reg_addr = 0x4a009e0c },
	[HSUSBHOST_CLKCTRL_IDX] = { .reg_addr = 0x4a009e54 },
	[HSUSBHOST_CLKCTRL_2_IDX] = { .reg_addr = 0x4a009e54 },
	[L3INIT_CLKSTCTRL_IDX] = { .reg_addr = 0x4a009300 },
};

static struct sar_overwrite_entry omap5_sar_overwrite_data[OW_IDX_SIZE] = {
#ifdef CONFIG_ARCH_OMAP5_ES1
	[MEMIF_CLKSTCTRL_IDX] = { .reg_addr = 0x4a009e24 },
	[MEMIF_CLKSTCTRL_2_IDX] = { .reg_addr = 0x4a009e24 },
	[SHADOW_FREQ_CFG1_IDX] = { .reg_addr = 0x4a004e38 },
	[HSUSBHOST_CLKCTRL_IDX] = { .reg_addr = 0x4a009e60 },
	[HSUSBHOST_CLKCTRL_2_IDX] = { .reg_addr = 0x4a009e60 },
	[L3INIT_CLKSTCTRL_IDX] = { .reg_addr = 0x4a009e2c },
#else
	[MEMIF_CLKSTCTRL_IDX] = { .reg_addr = 0x4a009e24 },
	[MEMIF_CLKSTCTRL_2_IDX] = { .reg_addr = 0x4a009e24 },
	[SHADOW_FREQ_CFG1_IDX] = { .reg_addr = 0x4a004e40 },
	[HSUSBHOST_CLKCTRL_IDX] = { .reg_addr = 0x4a009e64 },
	[HSUSBHOST_CLKCTRL_2_IDX] = { .reg_addr = 0x4a009e64 },
	[L3INIT_CLKSTCTRL_IDX] = { .reg_addr = 0x4a009e2c },
#endif
};

/**
 * check_overwrite_data - update matching SAR overwrite entry
 * @io_addr: IO address of the entry to lookup
 * @ram_addr: SAR RAM address of the entry to update
 * @size: size of the entry in words
 *
 * Checks a single SAR RAM entry against existing SAR RAM overwrite data.
 * If a matching overwrite location is found, update the pointers for
 * that entry. As each SAR RAM entry is of @size, the function must
 * go over the whole entry area in a for loop to find any possible
 * matching locations.
 */
static void check_overwrite_data(u32 io_addr, u32 ram_addr, int size)
{
	int i;

	while (size) {
		for (i = 0; i < OW_IDX_SIZE; i++) {
			if (sar_overwrite_data[i].reg_addr == io_addr &&
			    !sar_overwrite_data[i].valid) {
				sar_overwrite_data[i].sar_offset = ram_addr;
				sar_overwrite_data[i].valid = true;
				break;
			}
		}
		size--;
		io_addr += 4;
		ram_addr += 4;
	}
}

/**
 * omap_sar_save - saves a SAR RAM layout
 * @entry: first descriptor for the layout to save
 *
 * common routine to save the registers to SAR RAM with the
 * given parameters
 */
static void sar_save(struct sar_ram_entry *entry)
{
	u32 reg_val, size, offset;
	void __iomem *reg_read_addr, *sar_wr_addr;

	while (entry->size) {
		if (entry->flags & SAR_INVALID) {
			entry++;
			continue;
		}

		if (entry->flags & SAR_SAVE_COND) {
			if (!(entry->mod_func && entry->mod_func())) {
				entry++;
				continue;
			}
		}

		size = entry->size;
		reg_read_addr = entry->io_base + entry->offset;
		sar_wr_addr = sar_ram_base + entry->ram_addr;
		for (offset = 0; offset < size * 4; offset += 4) {
			reg_val = __raw_readl(reg_read_addr + offset);
			__raw_writel(reg_val, sar_wr_addr + offset);
		}
		entry++;
	}
}

/**
 * save_sar_bank3 - save SAR RAM bank 3
 *
 * Saves SAR RAM bank 3, this contains static data and should be saved
 * only once during boot.
 */
static void __init save_sar_bank3(void)
{
	struct clockdomain *l4_secure_clkdm;
	struct omap_hwmod *l3_main_3_oh;

	/*
	 * Not supported on ES1.0 silicon
	 */
	if (omap_rev() == OMAP4430_REV_ES1_0) {
		WARN_ONCE(1, "omap4: SAR backup not supported on ES1.0 ..\n");
		return;
	}

	l4_secure_clkdm = clkdm_lookup("l4_secure_clkdm");
	clkdm_wakeup(l4_secure_clkdm);

	omap_hwmod_setup_one("l3_main_3");
	l3_main_3_oh = omap_hwmod_lookup("l3_main_3");
	omap_hwmod_enable(l3_main_3_oh);

	sar_save(sar_ram_layout[2]);

	omap_hwmod_idle(l3_main_3_oh);

	clkdm_allow_idle(l4_secure_clkdm);
}

/**
 * omap4_sar_not_accessible - checks if all SAR IO areas are accessible
 *
 * Check if all SAR RAM IO locations are accessible or not. Namely,
 * USB host and TLL modules should be idle.
 */
static int omap4_sar_not_accessible(void)
{
	u32 usbhost_state, usbtll_state;
	s16 inst = cpu_is_omap44xx() ? OMAP4430_CM2_L3INIT_INST :
		   OMAP54XX_CM_CORE_L3INIT_INST;

	/*
	 * Make sure that USB host and TLL modules are not
	 * enabled before attempting to save the context
	 * registers, otherwise this will trigger an exception.
	 */
	usbhost_state = omap4_cminst_read_inst_reg(OMAP4430_CM2_PARTITION,
				inst,
				OMAP4_CM_L3INIT_USB_HOST_CLKCTRL_OFFSET);
	usbhost_state &= (OMAP4430_STBYST_MASK | OMAP4430_IDLEST_MASK);

	usbtll_state = omap4_cminst_read_inst_reg(OMAP4430_CM2_PARTITION,
				inst,
				OMAP4_CM_L3INIT_USB_TLL_CLKCTRL_OFFSET);
	usbtll_state &= OMAP4430_IDLEST_MASK;

	if ((usbhost_state == (OMAP4430_STBYST_MASK | OMAP4430_IDLEST_MASK)) &&
	    (usbtll_state == (OMAP4430_IDLEST_MASK)))
		return 0;
	else
		return -EBUSY;
}

/**
 * omap_sar_overwrite :
 * This API overwrites some of the SAR locations as a special cases
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
static void omap_sar_overwrite(void)
{
	u32 val = 0;
	u32 offset = 0;

	if (sar_overwrite_data[MEMIF_CLKSTCTRL_IDX].valid)
		__raw_writel(0x2, sar_ram_base +
			sar_overwrite_data[MEMIF_CLKSTCTRL_IDX].sar_offset);

	if (sar_overwrite_data[SHADOW_FREQ_CFG1_IDX].valid) {
		offset = sar_overwrite_data[SHADOW_FREQ_CFG1_IDX].sar_offset;
		val = __raw_readl(sar_ram_base + offset);
		val |= 1 << OMAP4430_FREQ_UPDATE_SHIFT;
		val |= 1 << OMAP4430_DLL_RESET_SHIFT;
		val &= ~OMAP4430_DLL_OVERRIDE_2_2_MASK;
		__raw_writel(val, sar_ram_base + offset);
	}

	if (sar_overwrite_data[SHADOW_FREQ_CFG2_IDX].valid) {
		offset = sar_overwrite_data[SHADOW_FREQ_CFG2_IDX].sar_offset;
		val = __raw_readl(sar_ram_base + offset);
		/*
		 * FIXME: Implement FREQ UPDATE for L#/M5 before enabling
		 * val |= 1 << OMAP4430_FREQ_UPDATE_SHIFT;
		 */
		__raw_writel(val, sar_ram_base + offset);
	}

	if (sar_overwrite_data[MEMIF_CLKSTCTRL_2_IDX].valid)
		__raw_writel(0x3, sar_ram_base +
			sar_overwrite_data[MEMIF_CLKSTCTRL_2_IDX].sar_offset);

	if (sar_overwrite_data[HSUSBHOST_CLKCTRL_IDX].valid) {
		offset = sar_overwrite_data[HSUSBHOST_CLKCTRL_IDX].sar_offset;

		/* Overwriting Phase2a data to be restored */
		/* CM_L3INIT_USB_HOST_CLKCTRL: SAR_MODE = 1, MODULEMODE = 2 */
		__raw_writel(OMAP4430_SAR_MODE_MASK | 0x2,
			     sar_ram_base + offset);
		/* CM_L3INIT_USB_TLL_CLKCTRL: SAR_MODE = 1, MODULEMODE = 1 */
		__raw_writel(OMAP4430_SAR_MODE_MASK | 0x1,
			     sar_ram_base + offset + 4);
		/*
		 * CM2 CM_SDMA_STATICDEP : Enable static depedency for
		 * SAR modules
		 */
		__raw_writel(OMAP4430_L4WKUP_STATDEP_MASK |
			     OMAP4430_L4CFG_STATDEP_MASK |
			     OMAP4430_L3INIT_STATDEP_MASK |
			     OMAP4430_L3_1_STATDEP_MASK |
			     OMAP4430_L3_2_STATDEP_MASK |
			     OMAP4430_ABE_STATDEP_MASK,
			     sar_ram_base + offset + 8);
	}

	if (sar_overwrite_data[HSUSBHOST_CLKCTRL_2_IDX].valid) {
		s16 inst = cpu_is_omap44xx() ? OMAP4430_CM2_L3INIT_INST :
			   OMAP54XX_CM_CORE_L3INIT_INST;

		offset = sar_overwrite_data[HSUSBHOST_CLKCTRL_2_IDX].sar_offset;

		/* Overwriting Phase2b data to be restored */
		/* CM_L3INIT_USB_HOST_CLKCTRL: SAR_MODE = 0, MODULEMODE = 0 */
		val = omap4_cminst_read_inst_reg(OMAP4430_CM2_PARTITION,
				inst,
				OMAP4_CM_L3INIT_USB_HOST_CLKCTRL_OFFSET);
		val &= (OMAP4430_CLKSEL_UTMI_P1_MASK |
			OMAP4430_CLKSEL_UTMI_P2_MASK);
		__raw_writel(val, sar_ram_base + offset);
		/* CM_L3INIT_USB_TLL_CLKCTRL: SAR_MODE = 0, MODULEMODE = 0 */
		__raw_writel(0, sar_ram_base + offset + 4);
		/* CM2 CM_SDMA_STATICDEP : Clear the static depedency */
		__raw_writel(OMAP4430_L3_2_STATDEP_MASK,
			     sar_ram_base + offset + 8);
	}

	if (sar_overwrite_data[L3INIT_CLKSTCTRL_IDX].valid) {
		offset = sar_overwrite_data[L3INIT_CLKSTCTRL_IDX].sar_offset;

		/*
		 * Set CM_L3INIT_CLKSTRCTRL to FORCE_SLEEP, it is enabled
		 * during SAR save and gets incorrect value of FORCE_WAKEUP.
		 */
		val = __raw_readl(sar_ram_base + offset);
		val &= ~(OMAP4430_CLKTRCTRL_MASK);
		val |= OMAP34XX_CLKSTCTRL_FORCE_SLEEP;
		__raw_writel(val, sar_ram_base + offset);
	}

	/* readback to ensure data reaches to SAR RAM */
	barrier();
	val = __raw_readl(sar_ram_base + offset);
}

/**
 * omap_sar_save - save sar context to SAR RAM
 *
 * Save the context to SAR_RAM1 and SAR_RAM2 as per the generated SAR
 * RAM layouts (sar_ram_layout[0] / [1]) and overwrite parts of the
 * area according to the overwrite data. This must be done before
 * attempting device off mode.
 */
int omap_sar_save(void)
{
	if (omap4_sar_not_accessible()) {
		pr_debug("%s: USB SAR CNTX registers are not accessible!\n",
			 __func__);
		return -EBUSY;
	}

	/*
	 * SAR bits and clocks needs to be enabled
	 */
	clkdm_wakeup(l3init_clkdm);
	if (omap_usbhs_update_sar())
		pwrdm_enable_hdwr_sar(l3init_pwrdm);
	omap_hwmod_enable_clocks(uhh_hwm);
	omap_hwmod_enable_clocks(tll_hwm);

	/* Save SAR BANK1 */
	sar_save(sar_ram_layout[0]);

	omap_hwmod_disable_clocks(uhh_hwm);
	omap_hwmod_disable_clocks(tll_hwm);
	if (omap_usbhs_update_sar()) {
		pwrdm_disable_hdwr_sar(l3init_pwrdm);
		/* For errata i719 */
		omap_usbhs_disable_update_sar();
	}
	clkdm_allow_idle(l3init_clkdm);

	/* Save SAR BANK2 */
	sar_save(sar_ram_layout[1]);

	omap_sar_overwrite();

	return 0;
}

void __iomem *omap4_get_sar_ram_base(void)
{
	return sar_ram_base;
}

/*
 * SAR ROM phases correspond to different restore phases executed
 * according to TRM chapter 16.4.21.4. "SAR ROM". Different phases
 * are executed according to different types of wakeups, and at
 * different points of wakeup chain. All the data must be stored
 * for the restore to be successful before device off entry. The
 * values in this array are offsets from the SAR ROM base address.
 */
static const u32 sar_rom_phases[] = {
	0, 0x30, 0x60
};

/**
 * struct sar_module - SAR IO module declarations
 * @io_base: IO base pointer for the module
 * @base: base physical address for the module
 * @size: size of the module
 * @flags: flags for the module
 * @mod_func: function pointer relevant when the flags had "SAR_SAVE_COND"
 */
struct sar_module {
	void __iomem *io_base;
	u32 base;
	u32 size;
	u32 flags;
	int (*mod_func)(void);
};

static struct sar_module *sar_modules;

/**
 * sar_ioremap_modules - map the IO address space for all modules
 *
 * Maps the whole IO address space required by SAR mechanism for
 * reading the registers from IO address space.
 */
static void sar_ioremap_modules(void)
{
	struct sar_module *mod;

	mod = sar_modules;

	while (mod->base) {
		if (!(mod->flags & SAR_INVALID)) {
			mod->io_base = ioremap(mod->base, mod->size);
			if (!mod->io_base)
				pr_err("%s: ioremap failed for %08x[%08x]\n",
				       __func__, mod->base, mod->size);
			WARN_ON(!mod->io_base);
		}
		mod++;
	}
}

/**
 * set_sar_io_addr - route SAR RAM entry to SAR IO module
 * @entry: SAR RAM entry to route
 * @addr: target IO address
 *
 * Scans available SAR IO modules for a matching one. If a match is
 * found, sets the entry base address to point the IO address space
 * and calculates the address offset from the beginning of the module.
 * Returns 0 if a matching IO module was found, -EINVAL otherwise.
 */
static int set_sar_io_addr(struct sar_ram_entry *entry, u32 addr)
{
	struct sar_module *mod;

	mod = sar_modules;

	while (mod->base) {
		if (addr >= mod->base && addr < mod->base + mod->size) {
			if (mod->flags & SAR_INVALID)
				break;
			entry->io_base = mod->io_base;
			entry->offset = addr - mod->base;
			entry->flags = mod->flags;
			entry->mod_func = mod->mod_func;
			return 0;
		}
		mod++;
	}
	pr_warn("%s: no matching sar_module for %08x\n", __func__, addr);
	return -EINVAL;
}

/**
 * sar_layout_generate - generates SAR RAM layout based on SAR ROM contents
 *
 * This function parses the contents of the SAR ROM to generate SAR RAM
 * layout entries. SAR ROM consists of following entries:
 *      u32 next        : pointer to next element on ROM
 *      u32 size        : number of words to restore
 *      u32 ram_address : SAR RAM address to restore from
 *      u32 io_address  : IO address to write SAR RAM contents to
 *
 * The function parses each SAR ROM entry, checks the validity of pointers
 * to both IO and SAR RAM, and adds the entry to the list if it is valid.
 * After adding the entry, next ROM entry is parsed, unless the target
 * pointer is outside of SAR ROM area which indicates end of restore chain.
 * Returns 0 on success, negative error value otherwise.
 */
static int sar_layout_generate(void)
{
	int phase;
	void __iomem *sarrom;
	u32 rombase, romend, rambase = 0, ramend = 0;
	u32 offset, next;
	u16 size;
	u32 ram_addr, io_addr;
	void *sarram;
	struct sar_ram_entry *entry[3];
	int bank;
	int ret = 0;

	pr_info("generating sar_ram layout...\n");

	if (cpu_is_omap44xx()) {
		rombase = OMAP44XX_SAR_ROM_BASE;
		romend = rombase + SZ_8K;
		rambase = OMAP44XX_SAR_RAM_BASE;
		ramend = rambase + SAR_BANK4_OFFSET - 1;
	} else if (cpu_is_omap54xx()) {
		rombase = OMAP54XX_SAR_ROM_BASE;
		romend = rombase + SZ_8K;
		rambase = OMAP54XX_SAR_RAM_BASE;
		ramend = rambase + SAR_BANK4_OFFSET - 1;
	}

	sarrom = ioremap(rombase, SZ_8K);

	/*
	 * Allocate temporary memory for sar ram layout. The amount
	 * allocated is exaggerated to make sure everything fits in,
	 * this memory is freed once SAR RAM layout generation is
	 * completed and actual sizes are known
	 */
	sarram = kmalloc(SAR_BANK4_OFFSET, GFP_KERNEL);
	for (bank = 0; bank < 3; bank++)
		entry[bank] = sarram + SAR_BANK2_OFFSET * bank;

	/* Go through each SAR ROM restore phase */
	for (phase = 0; phase < ARRAY_SIZE(sar_rom_phases); phase++) {
		offset = sar_rom_phases[phase];

		while (1) {
			next = __raw_readl(sarrom + offset);
			size = __raw_readl(sarrom + offset + 4) & 0xffff;
			ram_addr = __raw_readl(sarrom + offset + 8);
			io_addr = __raw_readl(sarrom + offset + 12);

			if (ram_addr >= rambase && ram_addr <= ramend) {
				/* Valid ram address, add entry */
				ram_addr -= rambase;

				/* Calculate which bank we are using */
				bank = ram_addr / SAR_BANK2_OFFSET;

				if (!set_sar_io_addr(entry[bank], io_addr)) {
					entry[bank]->size = size;
					entry[bank]->ram_addr = ram_addr;
					check_overwrite_data(io_addr, ram_addr,
							     size);
					entry[bank]++;
				}
			}

			/*
			 * If pointer to next SAR ROM address is invalid,
			 * stop. Indicates end of restore phase.
			 */
			if (next < rombase || next > romend)
				break;

			offset = next - rombase;
		}
	}

	/* Copy SAR RAM entry data to permanent location */
	for (bank = 0; bank < 3; bank++) {
		size = (u32)entry[bank] -
			(u32)(sarram + SAR_BANK2_OFFSET * bank);
		sar_ram_layout[bank] = kmalloc(size +
			sizeof(struct sar_ram_entry), GFP_KERNEL);
		if (!sar_ram_layout[bank]) {
			pr_err("%s: kmalloc failed\n", __func__);
			goto cleanup;
		}
		memcpy(sar_ram_layout[bank], sarram + SAR_BANK2_OFFSET * bank,
		       size);
		memset((void *)sar_ram_layout[bank] + size, 0,
		       sizeof(struct sar_ram_entry));
		entry[bank] = sar_ram_layout[bank];
	}

cleanup:
	kfree(sarram);
	iounmap(sarrom);
	pr_info("sar ram layout created\n");
	return ret;
}

static void __init omap_usb_clk_init(void)
{
	uhh_hwm = omap_hwmod_lookup("usb_host_hs");
	if (!uhh_hwm)
		pr_err("Could not look up usb_host_hs\n");

	tll_hwm = omap_hwmod_lookup("usb_tll_hs");
	if (!tll_hwm)
		pr_err("Could not look up usb_tll_hs\n");
}
/*
 * SAR IO module mapping. This is used to access the hardware registers
 * during SAR save.
 */
static struct sar_module omap44xx_sar_modules[] = {
	{ .base = OMAP44XX_EMIF1_BASE, .size = SZ_1M },
	{ .base = OMAP44XX_EMIF2_BASE, .size = SZ_1M },
	{ .base = OMAP44XX_DMM_BASE, .size = SZ_1M },
	{ .base = OMAP4430_CM1_BASE, .size = SZ_8K },
	{ .base = OMAP4430_CM2_BASE + OMAP44XX_CM2_USBHS_OFFSET,
	  .size = SZ_CM_USBHS, .flags = SAR_SAVE_COND,
	  .mod_func = &omap_usbhs_update_sar },
	{ .base = OMAP4430_CM2_BASE, .size = SZ_8K },
	{ .base = OMAP44XX_C2C_BASE, .size = SZ_1M },
	{ .base = OMAP443X_CTRL_BASE, .size = SZ_4K },
	{ .base = L3_44XX_BASE_CLK1, .size = SZ_1M },
	{ .base = L3_44XX_BASE_CLK2, .size = SZ_1M },
	{ .base = L3_44XX_BASE_CLK3, .size = SZ_1M },
	{ .base = OMAP44XX_USBTLL_BASE, .size = SZ_8K,
	  .flags = SAR_SAVE_COND, .mod_func = &omap_usbhs_update_sar },
	{ .base = OMAP44XX_UHH_CONFIG_BASE, .size = SZ_8K,
	  .flags = SAR_SAVE_COND, .mod_func = &omap_usbhs_update_sar },
	{ .base = L4_44XX_PHYS, .size = SZ_4M },
	{ .base = L4_PER_44XX_PHYS, .size = SZ_4M },
	{ .base = 0 },
};

/**
 * omap4_sar_ram_init - initializes SAR save mechanism
 *
 * This function allocates the IO maps, generates the SAR RAM layout
 * entries, detects the overwrite locations and stores the once per
 * boot type data (sar_bank3). Also L3INIT clockdomain / clocks
 * are allocated, which are needed during SAR save.
 */
static int __init omap4_sar_ram_init(void)
{
	/*
	 * To avoid code running on other OMAPs in
	 * multi-omap builds
	 */
	if (!cpu_is_omap44xx())
		return -ENODEV;

	/*
	 * Static mapping, never released Actual SAR area used is 8K it's
	 * spaced over 16K address with some part is reserved.
	 */
	sar_ram_base = ioremap(OMAP44XX_SAR_RAM_BASE, SZ_16K);
	if (WARN_ON(!sar_ram_base))
		return -ENOMEM;

	sar_modules = omap44xx_sar_modules;
	sar_overwrite_data = omap4_sar_overwrite_data;

	sar_ioremap_modules();

	sar_layout_generate();

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
	 * L3INIT PD and clocks are needed for SAR save phase
	 */
	l3init_pwrdm = pwrdm_lookup("l3init_pwrdm");
	if (!l3init_pwrdm)
		pr_err("Failed to get l3init_pwrdm\n");

	l3init_clkdm = clkdm_lookup("l3_init_clkdm");
	if (!l3init_clkdm)
		pr_err("Failed to get l3_init_clkdm\n");

	omap_usb_clk_init();

	return 0;
}
early_initcall(omap4_sar_ram_init);

static struct sar_module omap54xx_sar_modules[] = {
	{ .base = OMAP54XX_EMIF1_BASE, .size = SZ_1M },
	{ .base = OMAP54XX_EMIF2_BASE, .size = SZ_1M },
	{ .base = OMAP54XX_DMM_BASE, .size = SZ_1M },
	{ .base = OMAP54XX_CM_CORE_AON_BASE, .size = SZ_8K },
	{ .base = OMAP54XX_CM_CORE_BASE + OMAP55XX_CM_USBHS_OFFSET,
	  .size = SZ_CM_USBHS, .flags = SAR_SAVE_COND,
	  .mod_func = &omap_usbhs_update_sar },
	{ .base = OMAP54XX_CM_CORE_BASE, .size = SZ_8K },
	{ .base = OMAP54XX_C2C_BASE, .size = SZ_1M },
	{ .base = OMAP543x_CTRL_BASE, .size = SZ_4K },
	{ .base = OMAP543x_SCM_BASE, .size = SZ_4K },
	{ .base = L3_54XX_BASE_CLK1, .size = SZ_1M },
	{ .base = L3_54XX_BASE_CLK2, .size = SZ_1M },
	{ .base = L3_54XX_BASE_CLK3, .size = SZ_1M },
#ifndef CONFIG_MACH_OMAP_5430ZEBU
	{ .base = OMAP54XX_USBTLL_BASE, .size = SZ_8K,
	  .flags = SAR_SAVE_COND, .mod_func = &omap_usbhs_update_sar },
	{ .base = OMAP54XX_UHH_CONFIG_BASE, .size = SZ_8K,
	  .flags = SAR_SAVE_COND, .mod_func = &omap_usbhs_update_sar },
#else
	{ .base = OMAP54XX_USBTLL_BASE, .size = SZ_8K, .flags = SAR_INVALID },
	{ .base = OMAP54XX_UHH_CONFIG_BASE, .size = SZ_8K,
	   .flags = SAR_INVALID },
#endif
	{ .base = L4_54XX_PHYS, .size = SZ_4M },
	{ .base = L4_PER_54XX_PHYS, .size = SZ_4M },
	{ .base = OMAP54XX_LLIA_BASE, .size = SZ_64K },
	{ .base = OMAP54XX_LLIB_BASE, .size = SZ_64K },
	{ .base = 0 },
};

static int __init omap5_sar_ram_init(void)
{
	/*
	 * To avoid code running on other OMAPs in
	 * multi-omap builds
	 */
	if (!cpu_is_omap54xx())
		return -ENODEV;

	/*
	 * Static mapping, never released Actual SAR area used is 8K it's
	 * spaced over 16K address with some part is reserved.
	 */
	sar_ram_base = ioremap(OMAP54XX_SAR_RAM_BASE, SZ_16K);
	BUG_ON(!sar_ram_base);

	sar_modules = omap54xx_sar_modules;
	sar_overwrite_data = omap5_sar_overwrite_data;

	sar_ioremap_modules();

	sar_layout_generate();

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
	 * L3INIT PD and clocks are needed for SAR save phase
	 */
	l3init_pwrdm = pwrdm_lookup("l3init_pwrdm");
	if (!l3init_pwrdm)
		pr_err("Failed to get l3init_pwrdm\n");

	l3init_clkdm = clkdm_lookup("l3init_clkdm");
	if (!l3init_clkdm)
		pr_err("Failed to get l3init_clkdm\n");

	/*
	 * usbhost and tll clocks are needed for SAR save phase
	 */
	omap_usb_clk_init();

	return 0;
}
early_initcall(omap5_sar_ram_init);
