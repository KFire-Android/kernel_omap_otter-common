/*
 * common-board-devices.c
 *
 * Copyright (C) 2011 CompuLab, Ltd.
 * Author: Mike Rapoport <mike@compulab.co.il>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#include <linux/gpio.h>
#include <linux/spi/spi.h>
#include <linux/spi/ads7846.h>

#include <plat/mcspi.h>
#include <plat/nand.h>

#include "common-board-devices.h"

#if defined(CONFIG_TOUCHSCREEN_ADS7846) || \
	defined(CONFIG_TOUCHSCREEN_ADS7846_MODULE)
static struct omap2_mcspi_device_config ads7846_mcspi_config = {
	.turbo_mode	= 0,
};

static struct ads7846_platform_data ads7846_config = {
	.x_max			= 0x0fff,
	.y_max			= 0x0fff,
	.x_plate_ohms		= 180,
	.pressure_max		= 255,
	.debounce_max		= 10,
	.debounce_tol		= 3,
	.debounce_rep		= 1,
	.gpio_pendown		= -EINVAL,
	.keep_vref_on		= 1,
};

static struct spi_board_info ads7846_spi_board_info __initdata = {
	.modalias		= "ads7846",
	.bus_num		= -EINVAL,
	.chip_select		= 0,
	.max_speed_hz		= 1500000,
	.controller_data	= &ads7846_mcspi_config,
	.irq			= -EINVAL,
	.platform_data		= &ads7846_config,
};

void __init omap_ads7846_init(int bus_num, int gpio_pendown, int gpio_debounce,
			      struct ads7846_platform_data *board_pdata)
{
	struct spi_board_info *spi_bi = &ads7846_spi_board_info;
	int err;

	if (board_pdata && board_pdata->get_pendown_state) {
		err = gpio_request_one(gpio_pendown, GPIOF_IN, "TSPenDown");
		if (err) {
			pr_err("Couldn't obtain gpio for TSPenDown: %d\n", err);
			return;
		}
		gpio_export(gpio_pendown, 0);

		if (gpio_debounce)
			gpio_set_debounce(gpio_pendown, gpio_debounce);
	}

	spi_bi->bus_num	= bus_num;
	spi_bi->irq	= gpio_to_irq(gpio_pendown);

	if (board_pdata) {
		board_pdata->gpio_pendown = gpio_pendown;
		spi_bi->platform_data = board_pdata;
	} else {
		ads7846_config.gpio_pendown = gpio_pendown;
	}

	spi_register_board_info(&ads7846_spi_board_info, 1);
}
#else
void __init omap_ads7846_init(int bus_num, int gpio_pendown, int gpio_debounce,
			      struct ads7846_platform_data *board_pdata)
{
}
#endif

#if defined(CONFIG_MTD_NAND_OMAP2) || defined(CONFIG_MTD_NAND_OMAP2_MODULE)
static struct omap_nand_platform_data nand_data;

void __init omap_nand_flash_init(int options, struct mtd_partition *parts,
				 int nr_parts)
{
	u8 cs = 0;
	u8 nandcs = GPMC_CS_NUM + 1;

	/* find out the chip-select on which NAND exists */
	while (cs < GPMC_CS_NUM) {
		u32 ret = 0;
		ret = gpmc_cs_read_reg(cs, GPMC_CS_CONFIG1);

		if ((ret & 0xC00) == 0x800) {
			printk(KERN_INFO "Found NAND on CS%d\n", cs);
			if (nandcs > GPMC_CS_NUM)
				nandcs = cs;
		}
		cs++;
	}

	if (nandcs > GPMC_CS_NUM) {
		printk(KERN_INFO "NAND: Unable to find configuration "
				 "in GPMC\n ");
		return;
	}

	if (nandcs < GPMC_CS_NUM) {
		nand_data.cs = nandcs;
		nand_data.parts = parts;
		nand_data.nr_parts = nr_parts;
		nand_data.devsize = options;

		printk(KERN_INFO "Registering NAND on CS%d\n", nandcs);
		if (gpmc_nand_init(&nand_data) < 0)
			printk(KERN_ERR "Unable to register NAND device\n");
	}
}
#else
void __init omap_nand_flash_init(int options, struct mtd_partition *parts,
				 int nr_parts)
{
}
#endif

#if defined(CONFIG_TI_EMIF) || defined(CONFIG_TI_EMIF_MODULE)
/*
 * SDRAM memory data
 */
struct ddr_device_info lpddr2_elpida_2G_S4_x2_info = {
	.type		= DDR_TYPE_LPDDR2_S4,
	.density	= DDR_DENSITY_2Gb,
	.io_width	= DDR_IO_WIDTH_32,
	.cs1_used	= true,
	.cal_resistors_per_cs = false,
	.manufacturer	= "Elpida"
};

/*
 * AC timings for Elpida LPDDR2-s4 2Gb memory device
 */
struct lpddr2_timings lpddr2_elpida_2G_S4_timings[] = {
	/* Speed bin 800(400 MHz) */
	[0] = {
		.max_freq	= 400000000,
		.min_freq	= 10000000,
		.tRPab		= 21000,
		.tRCD		= 18000,
		.tWR		= 15000,
		.tRAS_min	= 42000,
		.tRRD		= 10000,
		.tWTR		= 7500,
		.tXP		= 7500,
		.tRTP		= 7500,
		.tCKESR		= 15000,
		.tDQSCK_max	= 5500,
		.tFAW		= 50000,
		.tZQCS		= 90000,
		.tZQCL		= 360000,
		.tZQinit	= 1000000,
		.tRAS_max_ns	= 70000,
		.tRTW		= 7500,
		.tAONPD		= 1000,
		.tDQSCK_max_derated = 6000,
	},
	/* Speed bin 400(200 MHz) */
	[1] = {
		.max_freq	= 200000000,
		.min_freq	= 10000000,
		.tRPab		= 21000,
		.tRCD		= 18000,
		.tWR		= 15000,
		.tRAS_min	= 42000,
		.tRRD		= 10000,
		.tWTR		= 10000,
		.tXP		= 7500,
		.tRTP		= 7500,
		.tCKESR		= 15000,
		.tDQSCK_max	= 5500,
		.tFAW		= 50000,
		.tZQCS		= 90000,
		.tZQCL		= 360000,
		.tZQinit	= 1000000,
		.tRAS_max_ns	= 70000,
		.tRTW		= 7500,
		.tAONPD		= 1000,
		.tDQSCK_max_derated = 6000,
	}
};

struct lpddr2_min_tck lpddr2_elpida_S4_min_tck = {
	.tRPab		= 3,
	.tRCD		= 3,
	.tWR		= 3,
	.tRASmin	= 3,
	.tRRD		= 2,
	.tWTR		= 2,
	.tXP		= 2,
	.tRTP		= 2,
	.tCKE		= 3,
	.tCKESR		= 3,
	.tFAW		= 8,
};

/*
 * AC timings for Elpida LPDDR2-s4 4Gb memory device
 */
struct lpddr2_timings lpddr2_elpida_4G_S4_timings[] = {
	/* Speed bin 1066(533 MHz) */
	[0] = {
		.max_freq	= 533333333,
		.min_freq	= 10000000,
		.tRPab		= 21000,
		.tRCD		= 18000,
		.tWR		= 15000,
		.tRAS_min	= 42000,
		.tRRD		= 10000,
		.tWTR		= 7500,
		.tXP		= 7500,
		.tRTP		= 7500,
		.tCKESR		= 15000,
		.tDQSCK_max	= 5500,
		.tFAW		= 50000,
		.tZQCS		= 90000,
		.tZQCL		= 360000,
		.tZQinit	= 1000000,
		.tRAS_max_ns	= 70000,
		.tRTW		= 7500,
		.tAONPD		= 1000,
		.tDQSCK_max_derated = 5620,
	},
	/* Speed bin 933(466 MHz) */
	[1] = {
		.max_freq	= 466666666,
		.min_freq	= 10000000,
		.tRPab		= 21000,
		.tRCD		= 18000,
		.tWR		= 15000,
		.tRAS_min	= 42000,
		.tRRD		= 10000,
		.tWTR		= 7500,
		.tXP		= 7500,
		.tRTP		= 7500,
		.tCKESR		= 15000,
		.tDQSCK_max	= 5500,
		.tFAW		= 50000,
		.tZQCS		= 90000,
		.tZQCL		= 360000,
		.tZQinit	= 1000000,
		.tRAS_max_ns	= 70000,
		.tRTW		= 7500,
		.tAONPD		= 1000,
		.tDQSCK_max_derated = 6000,
	},
	/* Speed bin 800(400 MHz) */
	[2] = {
		.max_freq	= 400000000,
		.min_freq	= 10000000,
		.tRPab		= 21000,
		.tRCD		= 18000,
		.tWR		= 15000,
		.tRAS_min	= 42000,
		.tRRD		= 10000,
		.tWTR		= 7500,
		.tXP		= 7500,
		.tRTP		= 7500,
		.tCKESR		= 15000,
		.tDQSCK_max	= 5500,
		.tFAW		= 50000,
		.tZQCS		= 90000,
		.tZQCL		= 360000,
		.tZQinit	= 1000000,
		.tRAS_max_ns	= 70000,
		.tRTW		= 7500,
		.tAONPD		= 1000,
		.tDQSCK_max_derated = 6000,
	},
	/* Speed bin 533(266 MHz) */
	[3] = {
		.max_freq	= 266666666,
		.min_freq	= 10000000,
		.tRPab		= 21000,
		.tRCD		= 18000,
		.tWR		= 15000,
		.tRAS_min	= 42000,
		.tRRD		= 10000,
		.tWTR		= 7500,
		.tXP		= 7500,
		.tRTP		= 7500,
		.tCKESR		= 15000,
		.tDQSCK_max	= 5500,
		.tFAW		= 50000,
		.tZQCS		= 90000,
		.tZQCL		= 360000,
		.tZQinit	= 1000000,
		.tRAS_max_ns	= 70000,
		.tRTW		= 7500,
		.tAONPD		= 1000,
		.tDQSCK_max_derated = 6000,
	},
	/* Speed bin 466(233 MHz) */
	[4] = {
		.max_freq	= 233333333,
		.min_freq	= 10000000,
		.tRPab		= 21000,
		.tRCD		= 18000,
		.tWR		= 15000,
		.tRAS_min	= 42000,
		.tRRD		= 10000,
		.tWTR		= 10000,
		.tXP		= 7500,
		.tRTP		= 7500,
		.tCKESR		= 15000,
		.tDQSCK_max	= 5500,
		.tFAW		= 50000,
		.tZQCS		= 90000,
		.tZQCL		= 360000,
		.tZQinit	= 1000000,
		.tRAS_max_ns	= 70000,
		.tRTW		= 7500,
		.tAONPD		= 1000,
		.tDQSCK_max_derated = 6000,
	},
	/* Speed bin 400(200 MHz) */
	[5] = {
		.max_freq	= 200000000,
		.min_freq	= 10000000,
		.tRPab		= 21000,
		.tRCD		= 18000,
		.tWR		= 15000,
		.tRAS_min	= 42000,
		.tRRD		= 10000,
		.tWTR		= 10000,
		.tXP		= 7500,
		.tRTP		= 7500,
		.tCKESR		= 15000,
		.tDQSCK_max	= 5500,
		.tFAW		= 50000,
		.tZQCS		= 90000,
		.tZQCL		= 360000,
		.tZQinit	= 1000000,
		.tRAS_max_ns	= 70000,
		.tRTW		= 7500,
		.tAONPD		= 1000,
		.tDQSCK_max_derated = 6000,
	}
};

/*
 * SDRAM memory data
 */
struct ddr_device_info lpddr2_elpida_4G_S4_x2_info = {
	.type		= DDR_TYPE_LPDDR2_S4,
	.density	= DDR_DENSITY_4Gb,
	.io_width	= DDR_IO_WIDTH_32,
	.cs1_used	= true,
	.cal_resistors_per_cs = false,
	.manufacturer	= "Elpida"
};

struct ddr_device_info lpddr2_elpida_4G_S4_info = {
	.type		= DDR_TYPE_LPDDR2_S4,
	.density	= DDR_DENSITY_4Gb,
	.io_width	= DDR_IO_WIDTH_32,
	.cs1_used	= false,
	.cal_resistors_per_cs = false,
	.manufacturer	= "Elpida"
};
#endif
