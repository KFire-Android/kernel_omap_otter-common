/*
 * arch/arm/mach-omap2/board-54xx-sevm-touch.c
 *
 * Copyright (C) 2012 Texas Instruments
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

#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/qtouch_obp_ts.h>

#include <plat/i2c.h>

#include "board-54xx-sevm.h"
#include "mux.h"

#define OMAP5_TOUCH_IRQ_1              179
#define OMAP5_TOUCH_RESET              230

static int sevm_touch_reset(void)
{
	gpio_direction_output(OMAP5_TOUCH_RESET, 1);
	mdelay(100);
	gpio_direction_output(OMAP5_TOUCH_RESET, 0);
	mdelay(100);
	gpio_direction_output(OMAP5_TOUCH_RESET, 1);
	mdelay(100);
	return 0;
}

static struct qtm_touch_keyarray_cfg omap5evm_key_array_data[] = {
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
	.irqflags       = (IRQF_TRIGGER_FALLING | IRQF_TRIGGER_LOW),
	.flags          = (QTOUCH_USE_MULTITOUCH | QTOUCH_FLIP_X |
			   QTOUCH_FLIP_Y | QTOUCH_CFG_BACKUPNV),
	.abs_min_x      = 0,
	.abs_max_x      = 1280,
	.abs_min_y      = 0,
	.abs_max_y      = 1024,
	.abs_min_p      = 0,
	.abs_max_p      = 255,
	.abs_min_w      = 0,
	.abs_max_w      = 15,
	.x_delta        = 0x00,
	.y_delta        = 0x00,
	.nv_checksum    = 0x187a,
	.fuzz_x         = 0,
	.fuzz_y         = 0,
	.fuzz_p         = 2,
	.fuzz_w         = 2,
	.hw_reset       = NULL,
	.gen_cmd_proc = {
		.reset  = 0x00,
		.backupnv = 0x00,
		.calibrate = 0x01,
		.reportall = 0x00,
	},
	.power_cfg      = {
		.idle_acq_int   = 0xff,
		.active_acq_int = 0xff,
		.active_idle_to = 0x0a,
	},
	.acquire_cfg    = {
		.charge_time    = 0x07,
		.atouch_drift   = 0x05,
		.touch_drift    = 0x14,
		.drift_susp     = 0x14,
		.touch_autocal  = 0x00,
		.sync           = 0x00,
		.cal_suspend_time = 0x0a,
		.cal_suspend_thresh = 0x00,
	},
	.multi_touch_cfg        = {
		.ctrl           = 0x83,
		.x_origin       = 0x00,
		.y_origin       = 0x00,
		.x_size         = 0x12,
		.y_size         = 0x0c,
		.aks_cfg        = 0x00,
		.burst_len      = 0x14,
		.tch_det_thr    = 0x28,
		.tch_det_int    = 0x02,
		.mov_hyst_init  = 0x0a,
		.mov_hyst_next  = 0x0a,
		.mov_filter     = 0x00,
		.num_touch      = 10,
		.orient         = 0x00,
		.mrg_timeout    = 0x00,
		.merge_hyst     = 0x0a,
		.merge_thresh   = 0x0a,
		.amp_hyst       = 0x00,
		.x_res          = 0x0fff,
		.y_res          = 0x0500,
		.x_low_clip     = 0x00,
		.x_high_clip    = 0x00,
		.y_low_clip     = 0x00,
		.y_high_clip    = 0x00,
		.x_edge_ctrl    = 0xD4,
		.x_edge_dist    = 0x42,
		.y_edge_ctrl    = 0xD4,
		.y_edge_dist    = 0x64,
		.jumplimit      = 0x3E,
	},
	.key_array      = {
		.cfg            = omap5evm_key_array_data,
		.num_keys   = ARRAY_SIZE(omap5evm_key_array_data),
	},
	.grip_suppression_cfg = {
		.ctrl           = 0x00,
		.xlogrip        = 0x00,
		.xhigrip        = 0x00,
		.ylogrip        = 0x00,
		.yhigrip        = 0x00,
		.maxtchs        = 0x00,
		.reserve0       = 0x00,
		.szthr1         = 0x00,
		.szthr2         = 0x00,
		.shpthr1        = 0x00,
		.shpthr2        = 0x00,
		.supextto       = 0x00,
	},
	.noise0_suppression_cfg = {
		.ctrl           = 0x07,
		.reserved       = 0x0000,
		.gcaf_upper_limit = 0x0019,
		.gcaf_lower_limit = 0xffe7,
		.gcaf_valid     = 0x04,
		.noise_thresh   = 0x14,
		.reserved1      = 0x00,
		.freq_hop_scale = 0x00,
		.burst_freq_0   = 0x0a,
		.burst_freq_1 = 0x07,
		.burst_freq_2 = 0x0c,
		.burst_freq_3 = 0x11,
		.burst_freq_4 = 0x14,
		.num_of_gcaf_samples = 0x04,
	},
	.touch_proximity_cfg = {
		.ctrl           = 0x00,
		.xorigin        = 0x00,
		.yorigin        = 0x00,
		.xsize          = 0x00,
		.ysize          = 0x00,
		.reserved       = 0x00,
		.blen           = 0x00,
		.fxddthr        = 0x0000,
		.fxddi          = 0x00,
		.average        = 0x00,
		.mvnullrate     = 0x0000,
		.mvdthr         = 0x0000,
	},
	.spt_commsconfig_cfg = {
		.ctrl           = 0x00,
		.command        = 0x00,
	},
	.spt_gpiopwm_cfg = {
		.ctrl           = 0x00,
		.reportmask     = 0x00,
		.dir            = 0x00,
		.intpullup      = 0x00,
		.out            = 0x00,
		.wake           = 0x00,
		.pwm            = 0x00,
		.period         = 0x00,
		.duty0          = 0x00,
		.duty1          = 0x00,
		.duty2          = 0x00,
		.duty3          = 0x00,
		.trigger0       = 0x00,
		.trigger1       = 0x00,
		.trigger2       = 0x00,
		.trigger3       = 0x00,
	},
	.onetouchgestureprocessor_cfg = {
		.ctrl   =       0x00,
		.numgest =      0x00,
		.gesten =       0x00,
		.process =      0x00,
		.tapto =        0x00,
		.flickto =      0x00,
		.dragto =       0x00,
		.spressto =     0x00,
		.lpressto =     0x00,
		.reppressto =   0x00,
		.flickthr =     0x00,
		.dragthr =      0x00,
		.tapthr =       0x00,
		.throwthr =     0x00,
	},
	.spt_selftest_cfg = {
		.ctrl = 0x03,
		.cmd = 0x00,
		.hisiglim0 = 0x36b0,
		.losiglim0 = 0x1b58,
		.hisiglim1 = 0x0000,
		.losiglim1 = 0x0000,
		.hisiglim2 = 0x0000,
		.losiglim2 = 0x0000,
	},
	.twotouchgestureprocessor_cfg = {
		.ctrl   =       0x03,
		.numgest =      0x01,
		.reserved =     0x00,
		.gesten =       0xe0,
		.rotatethr =    0x03,
		.zoomthr =      0x0063,
	},
	.spt_cte_cfg = {
		.ctrl = 0x00,
		.command = 0x00,
		.mode = 0x02,
		.gcaf_idle_mode = 0x20,
		.gcaf_actv_mode = 0x20,
	},
};

static struct i2c_board_info __initdata sevm_i2c_bus4_touch_info[] = {
	{
		I2C_BOARD_INFO(QTOUCH_TS_NAME, 0x4a),
		.platform_data = &atmel_mxt224_ts_platform_data,
		.irq = OMAP5_TOUCH_IRQ_1,
	},
};

int __init sevm_touch_init(void)
{
	gpio_request(OMAP5_TOUCH_RESET, "atmel reset");

	i2c_register_board_info(4, sevm_i2c_bus4_touch_info,
		ARRAY_SIZE(sevm_i2c_bus4_touch_info));

	return 0;
}
