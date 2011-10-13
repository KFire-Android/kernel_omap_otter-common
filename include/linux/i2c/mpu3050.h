/*
 * mpu3050.h
 * MPU-3050 Gyroscope driver
 *
 * Copyright (C) 2011 Texas Instruments
 * Author: Dan Murphy <Dmurphy@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Derived work from mpu3050_gyro.h from Jorge Bustamante <jbustamante@ti.com>
 */

#ifndef _LINUX_MPU3050_I2C_H
#define _LINUX_MPU3050_I2C_H

#define MPU3050_INT_STATUS_RAW_DATA_RDY		(1 << 0)
#define MPU3050_INT_STATUS_DMP_DONE		(1 << 1)
#define MPU3050_INT_STATUS_MPU_RDY		(1 << 2)

#define MPU3050_INT_CFG_RAW_RDY_EN	(1 << 0)
#define MPU3050_INT_CFG_DMP_DONE_EN	(1 << 1)
#define MPU3050_INT_CFG_MPU_RDY_EN	(1 << 2)
#define MPU3050_INT_CFG_ANY_RD_CLEAR	(1 << 4)
#define MPU3050_INT_CFG_LATCH_INT_EN	(1 << 5)
#define MPU3050_INT_CFG_OPEN		(1 << 6)
#define MPU3050_INT_CFG_ACTL		(1 << 7)

#define MPU3050_PWR_MGMT_RESET		(1 << 7)
#define MPU3050_PWR_MGMT_SLEEP		(1 << 6)
#define MPU3050_PWR_MGMT_STANDBY_X	(1 << 5)
#define MPU3050_PWR_MGMT_STANDBY_Y	(1 << 4)
#define MPU3050_PWR_MGMT_STANDBY_Z	(1 << 3)
#define MPU3050_PWR_MGMT_CLK_INTERNAL	0x00
#define MPU3050_PWR_MGMT_CLK_X_REF	0x01
#define MPU3050_PWR_MGMT_CLK_Y_REF	0x02
#define MPU3050_PWR_MGMT_CLK_Z_REF	0x03
#define MPU3050_PWR_MGMT_CLK_EXT_32K	0x04
#define MPU3050_PWR_MGMT_CLK_EXT_19K	0x05
#define MPU3050_PWR_MGMT_CLK_STOP_RESET	0x07

struct mpu3050gyro_platform_data {
	int irq_flags;

	int default_poll_rate;
	uint8_t slave_i2c_addr;
	uint8_t sample_rate_div;
	uint8_t dlpf_fs_sync;
	uint8_t interrupt_cfg;
};

#endif
