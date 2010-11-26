/*
 * board-zoom-flash.c
 *
 * Copyright (C) 2009 Texas Instruments Inc.
 * Vimal Singh <vimalsingh@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/mtd/nand.h>
#include <linux/types.h>
#include <linux/io.h>

#include <asm/mach/flash.h>
#include <plat/board.h>
#include <plat/gpmc.h>
#include <plat/nand.h>

#include <mach/board-zoom.h>

#if defined(CONFIG_MTD_NAND_OMAP2) || \
		defined(CONFIG_MTD_NAND_OMAP2_MODULE)
/* NAND chip access: 16 bit */
static struct omap_nand_platform_data zoom_nand_data = {
	.nand_setup	= NULL,
	.dma_channel	= -1,	/* disable DMA in OMAP NAND driver */
	.dev_ready	= NULL,
	.devsize	= 1,	/* '0' for 8-bit, '1' for 16-bit device */
};

/**
 * zoom_flash_init - Identify devices connected to GPMC and register.
 *
 * @return - void.
 */
void __init zoom_flash_init(struct flash_partitions zoom_nand_parts[], int cs)
{
	u32 gpmc_base_add = OMAP34XX_GPMC_VIRT;

	zoom_nand_data.cs		= cs;
	zoom_nand_data.parts		= zoom_nand_parts[0].parts;
	zoom_nand_data.nr_parts		= zoom_nand_parts[0].nr_parts;
	zoom_nand_data.ecc_opt		= 0x2; /* HW ECC in romcode layout */
	zoom_nand_data.gpmc_baseaddr	= (void *)(gpmc_base_add);
	zoom_nand_data.gpmc_cs_baseaddr	= (void *)(gpmc_base_add +
						GPMC_CS0_BASE +
						cs * GPMC_CS_SIZE);
	gpmc_nand_init(&zoom_nand_data);
}
#else
void __init zoom_flash_init(struct flash_partitions zoom_nand_parts[], int cs)
{
}
#endif /* CONFIG_MTD_NAND_OMAP2 || CONFIG_MTD_NAND_OMAP2_MODULE */
