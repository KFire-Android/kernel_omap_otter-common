/*
 * arch/arm/mach-omap2/board-54xx-sevm-sensors.c
 *
 * Copyright (C) 2012 Texas Instruments
 *
 * Author: Leed Aguilar <leed.aguilar@ti.com>
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
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/io.h>
#include <linux/init.h>
#include <linux/i2c/tsl2771.h>
#include <linux/i2c/drv2667.h>
#include <linux/input/mpu6050.h>
#include <linux/akm8975.h>

#include <plat/i2c.h>

#include "board-54xx-sevm.h"

#define OMAP5_TSL2771_INT_GPIO		149
#define OMAP5_MPU6050_INT_GPIO		150
#define OMAP5_AK8975_INT_GPIO		165

static struct mpu6050_platform_data mpu6050_platform_data = {
	.aux_i2c_supply = 0,
	.sample_rate_div = 0,
	.config = 0,
	.fifo_mode = 0,
	.flags = (MPU6050_USE_ACCEL | MPU6050_USE_GYRO |
		  MPU6050_PASS_THROUGH_EN),
	.mpu6050_accel = {
		.x_axis = 2,
		.y_axis = 2,
		.z_axis = 2,
		.fsr = MPU6050_RANGE_2G,
		.hpf = 4, /* HPF ON and cut off 0.63HZ */
		.ctrl_mode = MPU605_MODE_MD,
		.mode_thr_val = 1, /* Threshold value */
		.mode_thr_dur = 2, /* Threshold duration */
		.irqflags = IRQF_TRIGGER_HIGH,
	},
	.mpu6050_gyro = {
		.def_poll_rate = 200,
		.x_axis = 2,
		.y_axis = 2,
		.z_axis = 2,
		.fsr = MPU6050_GYRO_FSR_250,
		.config = 0,
	},
};

static struct tsl2771_platform_data tsl2771_data = {
	.irq_flags      = (IRQF_TRIGGER_LOW | IRQF_ONESHOT),
	.flags          = (TSL2771_USE_ALS | TSL2771_USE_PROX),
	.def_enable                     = 0x0,
	.als_adc_time                   = 0xdb,
	.prox_adc_time                  = 0xff,
	.wait_time                      = 0x00,
	.als_low_thresh_low_byte        = 0x0,
	.als_low_thresh_high_byte       = 0x0,
	.als_high_thresh_low_byte       = 0x0,
	.als_high_thresh_high_byte      = 0x0,
	.prox_low_thresh_low_byte       = 0x0,
	.prox_low_thresh_high_byte      = 0x0,
	.prox_high_thresh_low_byte      = 0x0,
	.prox_high_thresh_high_byte     = 0x0,
	.interrupt_persistence          = 0xf6,
	.config                         = 0x00,
	.prox_pulse_count               = 0x03,
	.gain_control                   = 0xE0,
	.glass_attn                     = 0x01,
	.device_factor                  = 0x34,
};

static struct drv2667_pattern_data omap5evm_vib_array_data[] = {
	{
		.wave_seq = 0x01,
		.page_num = 0x01,
		.hdr_size = 0x05,
		.start_addr_upper = 0x80,
		.start_addr_lower = 0x06,
		.stop_addr_upper = 0x00,
		.stop_addr_lower = 0x09,
		.repeat_count = 0x01,
		.amplitude = 0x60,
		.frequency = 0x19,
		.duration = 0x05,
		.envelope = 0x0,
	},
};

static struct drv2667_platform_data omap5evm_drv2667_plat_data = {
	.drv2667_pattern_data = {
		.pattern_data = omap5evm_vib_array_data,
		.num_of_patterns = ARRAY_SIZE(omap5evm_vib_array_data),
	},
	.initial_vibrate = 10,
};

static struct akm8975_platform_data ak8975_data = {
	.intr = OMAP5_AK8975_INT_GPIO,
};

static struct i2c_board_info __initdata omap5evm_sensor_i2c2_boardinfo[] = {
	{
		I2C_BOARD_INFO("bmp085", 0x77),
	},
	{
		I2C_BOARD_INFO("tsl2771", 0x39),
		.platform_data = &tsl2771_data,
		.irq = OMAP5_TSL2771_INT_GPIO,
	},
	{
		I2C_BOARD_INFO("mpu6050", 0x68),
		.platform_data = &mpu6050_platform_data,
		.irq = OMAP5_MPU6050_INT_GPIO,
	},
	{
		I2C_BOARD_INFO("akm8975", 0x0C),
		.platform_data = &ak8975_data,
	},
};

static struct i2c_board_info __initdata omap5evm_sensor_i2c5_boardinfo[] = {
	{
		I2C_BOARD_INFO(DRV2667_NAME, 0x59),
		.platform_data = &omap5evm_drv2667_plat_data,
	},
};

int __init sevm_sensor_init(void)
{
	i2c_register_board_info(2, omap5evm_sensor_i2c2_boardinfo,
		ARRAY_SIZE(omap5evm_sensor_i2c2_boardinfo));

	i2c_register_board_info(5, omap5evm_sensor_i2c5_boardinfo,
		ARRAY_SIZE(omap5evm_sensor_i2c5_boardinfo));

	return 0;
}
