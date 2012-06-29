/*
 * arch/arm/mach-omap2/board-44xx-tablet2-touch.c
 *
 * Copyright (C) 2011 Texas Instruments
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/qtouch_obp_ts.h>
#include <linux/input/touch_platform.h>

#include <plat/i2c.h>

#include "board-44xx-tablet.h"
#include "mux.h"

extern struct touch_platform_data cyttsp4_i2c_touch_platform_data;

#define OMAP4_TOUCH_IRQ_1		35

static struct qtm_touch_keyarray_cfg blaze_tablet_key_array_data[] = {
	{
		.ctrl = 0,
		.x_origin = 0,
		.y_origin = 0,
		.x_size = 0,
		.y_size = 0,
		.aks_cfg = 0,
		.burst_len = 0,
		.tch_det_thr = 0,
		.tch_det_int = 0,
		.rsvd1 = 0,
	},
};

static struct qtouch_ts_platform_data atmel_mxt224_ts_platform_data = {
	.irqflags	= (IRQF_TRIGGER_FALLING | IRQF_TRIGGER_LOW),
	.flags		= (QTOUCH_USE_MULTITOUCH | QTOUCH_FLIP_Y |
			   QTOUCH_CFG_BACKUPNV),
	.abs_min_x	= 0,
	.abs_max_x	= 768,
	.abs_min_y	= 0,
	.abs_max_y	= 1024,
	.abs_min_p	= 0,
	.abs_max_p	= 255,
	.abs_min_w	= 0,
	.abs_max_w	= 15,
	.x_delta	= 1024,
	.y_delta	= 768,
	.nv_checksum	= 0x187a,
	.fuzz_x		= 5,
	.fuzz_y		= 5,
	.fuzz_p		= 2,
	.fuzz_w		= 2,
	.hw_reset	= NULL,
	.power_cfg	= {
		.idle_acq_int	= 0x58,
		.active_acq_int	= 0xff,
		.active_idle_to	= 0x0a,
	},
	.acquire_cfg	= {
		.charge_time	= 0x20,
		.atouch_drift	= 0x05,
		.touch_drift	= 0x14,
		.drift_susp	= 0x14,
		.touch_autocal	= 0x4b,
		.sync		= 0,
		.cal_suspend_time = 0x09,
		.cal_suspend_thresh = 0x23,
	},
	.multi_touch_cfg	= {
		.ctrl		= 0x83,
		.x_origin	= 0,
		.y_origin	= 0,
		.x_size		= 0x11,
		.y_size		= 0x0d,
		.aks_cfg	= 0x0,
		.burst_len	= 0x01,
		.tch_det_thr	= 0x30,
		.tch_det_int	= 0x2,
		.mov_hyst_init	= 0x0,
		.mov_hyst_next	= 0x0,
		.mov_filter	= 0x30,
		.num_touch	= 10,
		.orient		= 0x00,
		.mrg_timeout	= 0x01,
		.merge_hyst	= 0x0a,
		.merge_thresh	= 0x0a,
		.amp_hyst 	= 0x0a,
		.x_res 		= 0x02ff,
		.y_res 		= 0x03ff,
		.x_low_clip	= 0x00,
		.x_high_clip 	= 0x00,
		.y_low_clip 	= 0x00,
		.y_high_clip 	= 0x00,
	},
	.key_array      = {
		.cfg		= blaze_tablet_key_array_data,
		.num_keys   = ARRAY_SIZE(blaze_tablet_key_array_data),
	},
	.grip_suppression_cfg = {
		.ctrl		= 0x01,
		.xlogrip	= 0x00,
		.xhigrip	= 0x00,
		.ylogrip	= 0x00,
		.yhigrip	= 0x00,
		.maxtchs	= 0x00,
		.reserve0	= 0x00,
		.szthr1		= 0x50,
		.szthr2		= 0x28,
		.shpthr1	= 0x04,
		.shpthr2	= 0x0f,
		.supextto	= 0x0a,
	},
	.noise0_suppression_cfg = {
		.ctrl		= 0x05,
		.reserved	= 0x0000,
		.gcaf_upper_limit = 0x000a,
		.gcaf_lower_limit = 0xfff6,
		.gcaf_valid	= 0x04,
		.noise_thresh	= 0x20,
		.reserved1	= 0x00,
		.freq_hop_scale = 0x01,
		.burst_freq_0	= 0x0a,
		.burst_freq_1 = 0x0f,
		.burst_freq_2 = 0x14,
		.burst_freq_3 = 0x19,
		.burst_freq_4 = 0x1e,
		.num_of_gcaf_samples = 0x04,
	},
	.spt_cte_cfg = {
		.ctrl = 0x00,
		.command = 0x00,
		.mode = 0x01,
		.gcaf_idle_mode = 0x04,
		.gcaf_actv_mode = 0x08,
	},
};


static struct i2c_board_info __initdata omap4xx_i2c_bus4_touch_info[] = {
	{
		I2C_BOARD_INFO(QTOUCH_TS_NAME, 0x4b),
		.platform_data = &atmel_mxt224_ts_platform_data,
		.irq = OMAP_GPIO_IRQ(OMAP4_TOUCH_IRQ_1),
	},
#ifdef CONFIG_TOUCHSCREEN_CYPRESS_TTSP
	{
		I2C_BOARD_INFO(CY_I2C_NAME, CY_I2C_TCH_ADR),
		.platform_data = &cyttsp4_i2c_touch_platform_data,
		.irq = OMAP_GPIO_IRQ(OMAP4_TOUCH_IRQ_1),
	},
#endif
};

int __init tablet_touch_init(void)
{
	gpio_request(OMAP4_TOUCH_IRQ_1, "atmel touch irq");
	gpio_direction_input(OMAP4_TOUCH_IRQ_1);
	omap_mux_init_signal("gpmc_ad11.gpio_35",
			OMAP_PULL_ENA | OMAP_PULL_UP | OMAP_MUX_MODE3 |
			OMAP_INPUT_EN | OMAP_PIN_OFF_INPUT_PULLUP);

	i2c_register_board_info(4, omap4xx_i2c_bus4_touch_info,
		ARRAY_SIZE(omap4xx_i2c_bus4_touch_info));

	return 0;
}
