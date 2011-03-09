/*
 * OMAP4 specific common source file.
 *
 * Copyright (C) 2010 Texas Instruments, Inc.
 * Author:
 *	Santosh Shilimkar <santosh.shilimkar@ti.com>
 *
 *
 * This program is free software,you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>

#include <asm/hardware/gic.h>
#include <asm/hardware/cache-l2x0.h>
#include <asm/cacheflush.h>

#include <mach/hardware.h>
#include <mach/omap4-common.h>

#include <plat/control.h>
#include <plat/clockdomain.h>
#include <plat/clockdomain.h>


#ifdef CONFIG_ENABLE_L3_ERRORS
/*
 * L3 register offsets
 */
#define L3_44XX_BASE_CLK1		0x44000000
#define L3_44XX_BASE_CLK2		0x44800000
#define L3_44XX_BASE_CLK3		0x45000000
#define L3_44XX_BASE_FIREWALL		0x4A204000
#define CUSTOM_ERROR			0x2
#define STANDARD_ERROR			0x0
#define INBAND_ERROR			0x0
#define CLEAR_STDERR_LOG		80000000
#define EMIF_KERRLOG_OFFSET		0x10
#define L3_SLAVE_ADDRESS_OFFSET		0x14
#define LOGICAL_ADDR_ERRORLOG		0x4

u32 l3_flagmux_regerr[3] = {
	0x50C,
	0x100C,
	0X020C
};

/*
 * L3 Target standard Error register offsets
 */
u32 l3_targ_stderrlog_main_clk1[5] = {
	0x148, /* DMM1 */
	0x248, /* DMM2 */
	0x348, /* ABE */
	0x448, /* L4CFG */
	0x648  /* CLK2 PWR DISC */
};

u32 l3_targ_stderrlog_main_clk2[18] = {
	0x548,		/* COREX M3 */
	0x348,		/* DSS */
	0x148,		/* GPMC */
	0x448,		/* ISS */
	0x748,		/* IVAHD */
	0xD48,		/* missing in TRM  corresponds to AES1*/
	0x948,		/* L4 PER0*/
	0x248,		/* OCMRAM */
	0x148,		/* missing in TRM corresponds to GPMC sERROR*/
	0x648,		/* SGX */
	0x848,		/* SL2 */
	0x1648,		/* C2C */
	0x1148,		/* missing in TRM corresponds PWR DISC CLK1*/
	0xF48,		/* missing in TRM corrsponds to SHA1*/
	0xE48,		/* missing in TRM corresponds to AES2*/
	0xC48,		/* L4 PER3 */
	0xA48,		/* L4 PER1*/
	0xB48		/* L4 PER2*/
};

/* Firewall Register Offsets  BASE ADDRES 0x4A204000 */
u32 kerrlog_firewall[15] = {
	0xE000,		/* L3RAM SOURCE = 1*/
	0xC000,		/* GPMC SOURCE = 2*/
	0x8000,		/* EMIF SOURCE = 3*/
	0x1C000,	/* IVAHD SOURCE = 4*/
	0x14000,	/* DUAL CORTEX M3 SOURCE = 5*/
	0x1A000,	/* SL2 SOURCE = 6*/
	0x0,		/* C2C Master SOURCE = 12*/
	0x10000,	/* SGX SOURCE = 13*/
	0x18000,	/* DSS SOURCE = 14*/
	0x12000,	/* ISS SOURCE = 15*/
	0x0,		/* L4 PER1. Missing in TRM  SOURCE = 16*/
	0x0,		/* L4 CONFIG. Missing in TRM SOURCE = 17*/
	0x0,		/* DEBUGSS. Missing in TRM SOURCE = 18*/
	0x24000,	/* L4 ABE SOURCE = 19*/
	0x2000		/* C2C SLAVE SOURCE = 20*/
};

/* Firewall Register source names */
char *kerrlog_firewall_sourcename[15] = {
	"L3RAM",
	"GPMC",
	"EMIF",
	"IVAHD",
	"DUAL CORTEX M3",
	"SL2",
	"C2C Master",
	"SGX",
	"DSS",
	"ISS",
	"L4 PER1",
	"L4 CONFIG",
	"DEBUGSS",
	"L4 ABE",
	"C2C SLAVE"
};


u32 l3_targ_stderrlog_main_clk3[1] = {
	0x0148	/* EMUSS */
};


char *l3_targ_stderrlog_main_clk1_name[5] = {
    "DMM1",
    "DMM2",
    "ABE",
    "L4CFG",
    "CLK2 PWR DISC"
};

char *l3_targ_stderrlog_main_clk2_name[18] = {
    "COREX M3" ,
    "DSS ",
    "GPMC ",
    "ISS ",
    "IVAHD ",
    "AES1",
    "L4 PER0",
    "OCMRAM ",
    "GPMC sERROR",
    "SGX ",
    "SL2 ",
    "C2C ",
    "PWR DISC CLK1",
    "SHA1",
    "AES2",
    "L4 PER3",
    "L4 PER1",
    "L4 PER2"
};

char *l3_targ_stderrlog_main_clk3_name[1] = {
    "EMUSS"
};

u32 *l3_targ_stderrlog_main[3] = {
	l3_targ_stderrlog_main_clk1,
	l3_targ_stderrlog_main_clk2,
	l3_targ_stderrlog_main_clk3,
};

u32 *l3_targ_stderrlog_main_sourcename[3] = {
	(u32 *)l3_targ_stderrlog_main_clk1_name,
	(u32 *)l3_targ_stderrlog_main_clk2_name,
	(u32 *)l3_targ_stderrlog_main_clk3_name
};

u32 ctrl_sec_err_stat[2] = {
	0x2D0,
	0x2D4
};

void __iomem *l3_base[4];
void __iomem *ctrl_base;
#endif

#ifdef CONFIG_CACHE_L2X0
void __iomem *l2cache_base;
#endif

void __iomem *gic_cpu_base_addr;
void __iomem *gic_dist_base_addr;

#ifndef CONFIG_TF_MSHIELD
static struct clockdomain *l4_secure_clkdm;
#endif

void __init gic_init_irq(void)
{
	/* Static mapping, never released */
	gic_dist_base_addr = ioremap(OMAP44XX_GIC_DIST_BASE, SZ_4K);
	BUG_ON(!gic_dist_base_addr);
	gic_dist_init(0, gic_dist_base_addr, 29);

	/* Static mapping, never released */
	gic_cpu_base_addr = ioremap(OMAP44XX_GIC_CPU_BASE, SZ_512);
	BUG_ON(!gic_cpu_base_addr);
	gic_cpu_init(0, gic_cpu_base_addr);
}

#ifdef CONFIG_CACHE_L2X0
static int __init omap_l2_cache_init(void)
{
	/*
	 * To avoid code running on other OMAPs in
	 * multi-omap builds
	 */
	if (!cpu_is_omap44xx())
		return -ENODEV;

	/* Static mapping, never released */
	l2cache_base = ioremap(OMAP44XX_L2CACHE_BASE, SZ_4K);
	BUG_ON(!l2cache_base);

	if (omap_rev() != OMAP4430_REV_ES1_0) {
		/* Set POR through PPA service only in EMU/HS devices */
		if (omap_type() != OMAP2_DEVICE_TYPE_GP)
			omap4_secure_dispatcher(
				PPA_SERVICE_PL310_POR, 0x7, 1,
				PL310_POR, 0, 0, 0);

		omap_smc1(0x109, OMAP4_L2X0_AUXCTL_VALUE);
	}

	/* Enable PL310 L2 Cache controller */
	omap_smc1(0x102, 0x1);

	/*
	 * 32KB way size, 16-way associativity,
	 * parity disabled
	 */
	if (omap_rev() == OMAP4430_REV_ES1_0)
		l2x0_init(l2cache_base, 0x0e050000, 0xc0000fff);
	else
		l2x0_init(l2cache_base, OMAP4_L2X0_AUXCTL_VALUE, 0xd0000fff);

	return 0;
}
early_initcall(omap_l2_cache_init);
#endif


#ifndef CONFIG_TF_MSHIELD
/*
 * omap4_sec_dispatcher: Routine to dispatch low power secure
 * service routines
 *
 * @idx: The HAL API index
 * @flag: The flag indicating criticality of operation
 * @nargs: Number of valid arguments out of four.
 * @arg1, arg2, arg3 args4: Parameters passed to secure API
 *
 * Return the error value on success/failure
 */
u32 omap4_secure_dispatcher(u32 idx, u32 flag, u32 nargs, u32 arg1, u32 arg2,
							 u32 arg3, u32 arg4)
{
	u32 ret;
	u32 param[5];

	param[0] = nargs;
	param[1] = arg1;
	param[2] = arg2;
	param[3] = arg3;
	param[4] = arg4;

	/* Look-up Only once */
	if (!l4_secure_clkdm)
		l4_secure_clkdm = clkdm_lookup("l4_secure_clkdm");

	/* Put l4 secure to SW_WKUP so that moduels are accessible */
	omap2_clkdm_wakeup(l4_secure_clkdm);

	/*
	 * Secure API needs physical address
	 * pointer for the parameters
	 */
	flush_cache_all();
	outer_clean_range(__pa(param), __pa(param + 5));

	ret = omap_smc2(idx, flag, __pa(param));

	/* Restore the HW_SUP so that module can idle */
	omap2_clkdm_allow_idle(l4_secure_clkdm);

	return ret;
}
#endif

#ifdef CONFIG_ENABLE_L3_ERRORS
static void omap_fw_error_handler(u32 ctrl_sec_err_status,
					u32 ctrl_sec_err_status_regval)
{
	u32 err_source_firewall = 0;
	u32 kerror_log_fw = 0;
	u32 kerror_log_val = 0;
	u32 j = 0;
	u32 k = 0;

	/* Identify the source */
	for (j = 0; !err_source_firewall; j++)
		err_source_firewall = ctrl_sec_err_status_regval & (1<<j);
	/*
	 * -1 Since array offset is from Zero.One more -1 since CONTROL SEC
	 * register starts from 1 and not 0.
	 */
	err_source_firewall = j-1-1;

	/*
	 * Get the details from KerrorLog registers and print the firewall
	 * error details
	 */
	if (err_source_firewall == 0x3) {
		/* only for EMIF there are three kerrlog registers*/
		for (k = 0; k < 3; k++) {
			kerror_log_fw = (u32)l3_base[3] +
				kerrlog_firewall[err_source_firewall] +
				(EMIF_KERRLOG_OFFSET * k);
			kerror_log_val = readl(kerror_log_fw);
			/*
			 * Print the error log and clear it . The Command is
			 * yet to be updated after checking with HARDWARE TEAM
			 */
			pr_crit("FireWall Error : SOURCE:%s at ADDRESS 0x%x\n",
			kerrlog_firewall_sourcename[err_source_firewall],
			readl((kerror_log_fw+LOGICAL_ADDR_ERRORLOG)));
			writel(kerror_log_val, kerror_log_fw);
			dump_stack();
		}
	} else {
		kerror_log_fw = (u32)l3_base[3] +
			kerrlog_firewall[err_source_firewall];
		kerror_log_val = readl(kerror_log_fw);
		/* Print the error log and clear it.*/
		pr_crit("FireWall Error: SOURCE :%s at ADDRESS 0x%x\n",	\
		kerrlog_firewall_sourcename[err_source_firewall],	\
		readl((kerror_log_fw+LOGICAL_ADDR_ERRORLOG)));		\
		writel(kerror_log_val, kerror_log_fw);
		dump_stack();
	}

	/* Clear control status bits . This has to be tested*/
	writel(ctrl_sec_err_status_regval, ctrl_sec_err_status);
	return;
}

/*
 * Interrupt Handler for L3 error detection
 *	1) Identify the clock domain to which the error belongs to
 *	2) Identify the slave where the error information is logged
 *	3) Print the logged information
 */
static irqreturn_t l3_interrupt_handler(int irq, void *dev_id)
{
	int inttype, i, j;
	int err_source = 0;
	u32 stderrlog_main, stderrlog_main_reg_val, error_source_reg;
	u32 ctrl_sec_err_status, ctrl_sec_err_status_regval, slave_addr;
	char *source_name;

	/*
	 * Get the Type of interrupt
	 * 0- Application
	 * 1 - Debug
	 */
	if (irq == OMAP44XX_IRQ_L3_APP)
		inttype = 0;
	else
		inttype = 1;

	for (i = 0; i < 3; i++) {
		/*
		 * Read the regerr register of the clock domain
		 * to determine the source
		 */
		error_source_reg =  readl((l3_base[i] + l3_flagmux_regerr[i]
							+ (inttype << 3)));
		/* Get the corresponding error and analyse */
		if (error_source_reg) {
			/* Identify the source from control status register */
			for (j = 0; !err_source; j++)
				err_source = error_source_reg & (1<<j);
			/* Since array offset is from Zero */
			err_source = j-1;
			/* Read the stderrlog_main_source from clk domain */
			stderrlog_main = (u32)l3_base[i] +
				(*(l3_targ_stderrlog_main[i] + err_source));
			stderrlog_main_reg_val =  readl(stderrlog_main);

			switch ((stderrlog_main_reg_val & CUSTOM_ERROR)) {
			case STANDARD_ERROR:
				/* check if this is a firewall violation */
				ctrl_sec_err_status = (u32)ctrl_base +
						ctrl_sec_err_stat[inttype];
				ctrl_sec_err_status_regval =
						readl(ctrl_sec_err_status);
				source_name =
				(char *)(*(l3_targ_stderrlog_main_sourcename[i]
					+ err_source));
				slave_addr = stderrlog_main +

						L3_SLAVE_ADDRESS_OFFSET;
				if (!ctrl_sec_err_status_regval) {
					/*
					 * get the details about the inband
					 * error as command etc and print
					 * details
					 */
					pr_crit("L3 standard error: SOURCE:%s"
						"at address 0x%x\n",
						source_name, readl(slave_addr));
					dump_stack();
				} else {
					/* Then this is a Fire Wall Error */
					if (omap_type() == OMAP2_DEVICE_TYPE_GP)
						omap_fw_error_handler(
								ctrl_sec_err_status,
								ctrl_sec_err_status_regval);
				}
				/* clear the stderr log */
				writel((stderrlog_main_reg_val |
					CLEAR_STDERR_LOG), stderrlog_main);
				break;
			case CUSTOM_ERROR:
				pr_crit("CUSTOM SRESP error with SOURCE:%s\n",
				(char *)(*(l3_targ_stderrlog_main_sourcename[i]
						+ err_source)));
				/* clear the std error log*/
				writel((stderrlog_main_reg_val |
					CLEAR_STDERR_LOG), stderrlog_main);
				dump_stack();
				break;
			default:
				/* Nothing to be handled here as of now */
				break;
			}
		/* Error found so break the for loop */
		break;
		}
	}
	return IRQ_HANDLED;
}

static int __init omap_l3_init(void)
{
	int ret;

	/* Static mapping. Never released map it for 1M*/
	l3_base[0] = ioremap(L3_44XX_BASE_CLK1, SZ_1M);
	l3_base[1] = ioremap(L3_44XX_BASE_CLK2, SZ_1M);
	l3_base[2] = ioremap(L3_44XX_BASE_CLK3, SZ_1M);
	l3_base[3] = ioremap(L3_44XX_BASE_FIREWALL, SZ_1M);
	if ((!l3_base[0]) || (!l3_base[1]) || (!l3_base[2]) || (!l3_base[3])) {
		pr_crit("Could not ioremap L3 address space\n");
		return -ENOMEM;
	}
	ctrl_base = omap_ctrl_base_get();

	/*
	 * Setup interrupt Handlers
	 */
	ret = request_irq(OMAP44XX_IRQ_L3_DBG,
			(irq_handler_t)l3_interrupt_handler,
			IRQF_DISABLED, "l3_debug_error", NULL);
	if (ret) {
		pr_crit("L3: request_irq failed to register for 0x%x\n",
					 OMAP44XX_IRQ_L3_DBG);
		return ret;
	}
	ret = request_irq(OMAP44XX_IRQ_L3_APP,
			(irq_handler_t)l3_interrupt_handler,
			IRQF_DISABLED, "l3_app_error", NULL);
	if (ret) {
		pr_crit("L3: request_irq failed to register for 0x%x\n",
					 OMAP44XX_IRQ_L3_APP);
		return ret;
	}
	return 0;
}
early_initcall(omap_l3_init);
#endif
