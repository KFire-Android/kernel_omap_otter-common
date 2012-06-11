/*
 * INVENSENSE MPU6050 driver
 *
 * Copyright (C) 2011 Texas Instruments
 * Author: Kishore Kadiyala <kishore.kadiyala@ti.com>
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
 */

#ifndef _LINUX_MPU6050X_H
#define _LINUX_MPU6050X_H

/* MPU6050 REGISTERS */
#define MPU6050_REG_AUX_VDDIO				0x01
#define MPU6050_REG_SMPLRT_DIV				0x19
#define MPU6050_REG_CONFIG				0x1a
#define MPU6050_REG_GYRO_CONFIG				0x1b
#define MPU6050_REG_ACCEL_CONFIG			0x1c
#define MPU6050_REG_ACCEL_FF_THR			0x1d
#define MPU6050_REG_ACCEL_FF_DUR			0x1e
#define MPU6050_REG_ACCEL_MOT_THR			0x1f
#define MPU6050_REG_ACCEL_MOT_DUR			0x20
#define MPU6050_REG_ACCEL_ZRMOT_THR			0x21
#define MPU6050_REG_ACCEL_ZRMOT_DUR			0x22
#define MPU6050_REG_FIFO_EN				0x23
#define MPU6050_REG_I2C_MST_CTRL			0x24
#define MPU6050_REG_I2C_SLV0_ADDR			0x25
#define MPU6050_REG_I2C_SLV0_REG			0x26
#define MPU6050_REG_I2C_SLV0_CTRL			0x27
#define MPU6050_REG_I2C_SLV1_ADDR			0x28
#define MPU6050_REG_I2C_SLV1_REG			0x29
#define MPU6050_REG_I2C_SLV1_CTRL			0x2a
#define MPU6050_REG_I2C_SLV2_ADDR			0x2B
#define MPU6050_REG_I2C_SLV2_REG			0x2c
#define MPU6050_REG_I2C_SLV2_CTRL			0x2d
#define MPU6050_REG_I2C_SLV3_ADDR			0x2E
#define MPU6050_REG_I2C_SLV3_REG			0x2f
#define MPU6050_REG_I2C_SLV3_CTRL			0x30
#define MPU6050_REG_I2C_SLV4_ADDR			0x31
#define MPU6050_REG_I2C_SLV4_REG			0x32
#define MPU6050_REG_I2C_SLV4_DO				0x33
#define MPU6050_REG_I2C_SLV4_CTRL			0x34
#define MPU6050_REG_I2C_SLV4_DI				0x35
#define MPU6050_REG_I2C_MST_STATUS			0x36
#define MPU6050_REG_INT_PIN_CFG				0x37
#define MPU6050_REG_INT_ENABLE				0x38
#define MPU6050_REG_INT_STATUS				0x3A
#define MPU6050_REG_ACCEL_XOUT_H			0x3B
#define MPU6050_REG_ACCEL_XOUT_L			0x3c
#define MPU6050_REG_ACCEL_YOUT_H			0x3d
#define MPU6050_REG_ACCEL_YOUT_L			0x3e
#define MPU6050_REG_ACCEL_ZOUT_H			0x3f
#define MPU6050_REG_ACCEL_ZOUT_L			0x40
#define MPU6050_REG_TEMP_OUT_H				0x41
#define MPU6050_REG_TEMP_OUT_L				0x42
#define MPU6050_REG_GYRO_XOUT_H				0x43
#define MPU6050_REG_GYRO_XOUT_L				0x44
#define MPU6050_REG_GYRO_YOUT_H				0x45
#define MPU6050_REG_GYRO_YOUT_L				0x46
#define MPU6050_REG_GYRO_ZOUT_H				0x47
#define MPU6050_REG_GYRO_ZOUT_L				0x48
#define MPU6050_REG_EXT_SLV_SENS_DATA_00		0x49
#define MPU6050_REG_EXT_SLV_SENS_DATA_01		0x4a
#define MPU6050_REG_EXT_SLV_SENS_DATA_02		0x4b
#define MPU6050_REG_EXT_SLV_SENS_DATA_03		0x4c
#define MPU6050_REG_EXT_SLV_SENS_DATA_04		0x4d
#define MPU6050_REG_EXT_SLV_SENS_DATA_05		0x4e
#define MPU6050_REG_EXT_SLV_SENS_DATA_06		0x4F
#define MPU6050_REG_EXT_SLV_SENS_DATA_07		0x50
#define MPU6050_REG_EXT_SLV_SENS_DATA_08		0x51
#define MPU6050_REG_EXT_SLV_SENS_DATA_09		0x52
#define MPU6050_REG_EXT_SLV_SENS_DATA_10		0x53
#define MPU6050_REG_EXT_SLV_SENS_DATA_11		0x54
#define MPU6050_REG_EXT_SLV_SENS_DATA_12		0x55
#define MPU6050_REG_EXT_SLV_SENS_DATA_13		0x56
#define MPU6050_REG_EXT_SLV_SENS_DATA_14		0x57
#define MPU6050_REG_EXT_SLV_SENS_DATA_15		0x58
#define MPU6050_REG_EXT_SLV_SENS_DATA_16		0x59
#define MPU6050_REG_EXT_SLV_SENS_DATA_17		0x5a
#define MPU6050_REG_EXT_SLV_SENS_DATA_18		0x5b
#define MPU6050_REG_EXT_SLV_SENS_DATA_19		0x5c
#define MPU6050_REG_EXT_SLV_SENS_DATA_20		0x5d
#define MPU6050_REG_EXT_SLV_SENS_DATA_21		0x5e
#define MPU6050_REG_EXT_SLV_SENS_DATA_22		0x5f
#define MPU6050_REG_EXT_SLV_SENS_DATA_23		0x60
#define MPU6050_REG_ACCEL_INTEL_STATUS			0x61
#define MPU6050_REG_62_RSVD				0x62
#define MPU6050_REG_I2C_SLV0_DO				0x63
#define MPU6050_REG_I2C_SLV1_DO				0x64
#define MPU6050_REG_I2C_SLV2_DO				0x65
#define MPU6050_REG_I2C_SLV3_DO				0x66
#define MPU6050_REG_I2C_MST_DELAY_CTRL			0x67
#define MPU6050_REG_SIGNAL_PATH_RESET			0x68
#define MPU6050_REG_ACCEL_INTEL_CTRL			0x69
#define MPU6050_REG_USER_CTRL				0x6a
#define MPU6050_REG_PWR_MGMT_1				0x6b
#define MPU6050_REG_PWR_MGMT_2				0x6c
#define MPU6050_REG_FIFO_COUNTH				0x72
#define MPU6050_REG_FIFO_COUNTL				0x73
#define MPU6050_REG_FIFO_R_W				0x74
#define MPU6050_REG_WHOAMI				0x75


/* MPU6050 REGISTER MASKS */
#define MPU6050_CHIP_I2C_ADDR			0x68
#define MPU6050_DEVICE_RESET			BIT(7)
#define MPU6050_DEVICE_SLEEP_MODE		BIT(6)
#define MPU6050_FF_MODE				BIT(7)
#define MPU6050_MD_MODE				BIT(6)
#define MPU6050_ZM_MODE				BIT(5)
#define MPU6050_PM1_SLEEP			BIT(6)
#define MPU6050_ACCEL_SP_RESET			BIT(1)
#define MPU6050_GYRO_SP_RESET			BIT(2)
#define MPU6050_STBY_XA				BIT(5)
#define MPU6050_STBY_YA				BIT(4)
#define MPU6050_STBY_ZA				BIT(3)
#define MPU6050_STBY_XG				BIT(2)
#define MPU6050_STBY_YG				BIT(1)
#define MPU6050_STBY_ZG				BIT(0)
#define MPU6050_FF_INT				BIT(7)
#define MPU6050_MOT_INT				BIT(6)
#define MPU6050_ZMOT_INT			BIT(5)
#define MPU6050_DATARDY_INT			BIT(0)
#define MPU6050_PASS_THROUGH			BIT(7)
#define MPU6050_I2C_BYPASS_EN			BIT(1)
#define MPU6050_I2C_MST_EN			BIT(5)

/* The sensor measurement is stored as 16-bit 2's complement
 * value, so the maximun absolute value that can be obtained
 * correspond to (2^15/2) = 16384
 */
#define MPU6050_ABS_READING			16384

#define MPU6050_READ(data, addr, reg, len, val, msg) \
	(data->bus_ops->read(data->dev, addr, reg, len, val, msg))
#define MPU6050_WRITE(data, addr, reg, val, msg) \
	((data)->bus_ops->write(data->dev, addr, reg, val, msg))

#define MPU6050_MAX_RW_RETRIES			5
#define MPU6050_I2C_RETRY_DELAY 		10

struct mpu6050_bus_ops {
	uint16_t bustype;
	int (*read)(struct device *, u8, u8, u8, u8 *, u8 *);
	int (*write)(struct device *, u8, u8, u8, u8 *);
};

struct mpu6050_gyro_data {
	const struct mpu6050_bus_ops *bus_ops;
	struct device *dev;
	struct delayed_work input_work;
	struct input_dev *input_dev;
	struct mutex mutex;
	uint32_t req_poll_rate;
	uint8_t enabled;
};

enum accel_op_mode {
	FREE_FALL_MODE = 0,
	MOTION_DET_MODE = 1,
	ZERO_MOT_DET_MODE = 2,
};

struct mpu6050_accel_data {
	const struct mpu6050_bus_ops *bus_ops;
	struct device *dev;
	struct input_dev *input_dev;
	int irq;
	int gpio;
	enum accel_op_mode mode;
	struct mutex mutex;
	uint8_t enabled;
};

struct mpu6050_data {
	struct i2c_client *client;
	struct device *dev;
	const struct mpu6050_bus_ops *bus_ops;
	struct mpu6050_platform_data *pdata;
	struct mpu6050_accel_data *accel_data;
	struct mpu6050_gyro_data *gyro_data;
	int irq;
	struct mutex mutex;
	bool suspended;
};


int mpu6050_init(struct mpu6050_data *data,
			const struct mpu6050_bus_ops *bops);
void mpu6050_exit(struct mpu6050_data *);
void mpu6050_suspend(struct mpu6050_data *);
void mpu6050_resume(struct mpu6050_data *);


void mpu6050_accel_suspend(struct mpu6050_accel_data *data);
void mpu6050_accel_resume(struct mpu6050_accel_data *data);
struct mpu6050_accel_data *mpu6050_accel_init(
			const struct mpu6050_data *mpu_data);
void mpu6050_accel_exit(struct mpu6050_accel_data *data);

void mpu6050_gyro_suspend(struct mpu6050_gyro_data *data);
void mpu6050_gyro_resume(struct mpu6050_gyro_data *data);
struct mpu6050_gyro_data *mpu6050_gyro_init(
			const struct mpu6050_data *mpu_data);
void mpu6050_gyro_exit(struct mpu6050_gyro_data *data);
#endif
