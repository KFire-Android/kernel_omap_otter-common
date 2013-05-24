/*  Date: 2013/05/15
 *  Revision: 3.4.1
 *  Hashcode
 */

/*
 * This software program is licensed subject to the GNU General Public License
 * (GPL).Version 2,June 1991, available at http://www.fsf.org/copyleft/gpl.html

 * (C) Copyright 2011 Bosch Sensortec GmbH
 * All Rights Reserved
 */


/* file BMA250.c
   brief This file contains all function implementations for the BMA250 in linux

*/

#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <asm/uaccess.h>
#include <linux/earlysuspend.h>
#include <linux/wakelock.h>
#include <linux/regulator/consumer.h>
#include <linux/i2c/twl.h>
#include <linux/gpio.h>
#include <linux/irq.h>

#include <linux/input/bma250.h>

//#define BMA250_DEBUG 0
#ifdef BMA250_DEBUG
#define aprintk(fmt, args...)	printk(fmt, ##args)
#else
#define aprintk(fmt, args...)
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
static void bma250_early_suspend(struct early_suspend *h);
static void bma250_late_resume(struct early_suspend *h);
#endif

#define BMA250_CHIP_ID			3
#define BMA250_XYZ_DATA_SIZE		6
#define BMA250_READDATA_RETRY		5

/*
 *
 *      register definitions
 *
 */
#define BMA250_CHIP_ID_REG                      0x00
#define BMA250_VERSION_REG                      0x01
#define BMA250_X_AXIS_LSB_REG                   0x02
#define BMA250_X_AXIS_MSB_REG                   0x03
#define BMA250_Y_AXIS_LSB_REG                   0x04
#define BMA250_Y_AXIS_MSB_REG                   0x05
#define BMA250_Z_AXIS_LSB_REG                   0x06
#define BMA250_Z_AXIS_MSB_REG                   0x07
#define BMA250_TEMP_RD_REG                      0x08
#define BMA250_STATUS1_REG                      0x09
#define BMA250_STATUS2_REG                      0x0A
#define BMA250_STATUS_TAP_SLOPE_REG             0x0B
#define BMA250_STATUS_ORIENT_HIGH_REG           0x0C
#define BMA250_RANGE_SEL_REG                    0x0F
#define BMA250_BW_SEL_REG                       0x10
#define BMA250_MODE_CTRL_REG                    0x11
#define BMA250_LOW_NOISE_CTRL_REG               0x12
#define BMA250_DATA_CTRL_REG                    0x13
#define BMA250_RESET_REG                        0x14
#define BMA250_INT_ENABLE1_REG                  0x16
#define BMA250_INT_ENABLE2_REG                  0x17
#define BMA250_INT1_PAD_SEL_REG                 0x19
#define BMA250_INT_DATA_SEL_REG                 0x1A
#define BMA250_INT2_PAD_SEL_REG                 0x1B
#define BMA250_INT_SRC_REG                      0x1E
#define BMA250_INT_SET_REG                      0x20
#define BMA250_INT_CTRL_REG                     0x21
#define BMA250_LOW_DURN_REG                     0x22
#define BMA250_LOW_THRES_REG                    0x23
#define BMA250_LOW_HIGH_HYST_REG                0x24
#define BMA250_HIGH_DURN_REG                    0x25
#define BMA250_HIGH_THRES_REG                   0x26
#define BMA250_SLOPE_DURN_REG                   0x27
#define BMA250_SLOPE_THRES_REG                  0x28
#define BMA250_TAP_PARAM_REG                    0x2A
#define BMA250_TAP_THRES_REG                    0x2B
#define BMA250_ORIENT_PARAM_REG                 0x2C
#define BMA250_THETA_BLOCK_REG                  0x2D
#define BMA250_THETA_FLAT_REG                   0x2E
#define BMA250_FLAT_HOLD_TIME_REG               0x2F
#define BMA250_STATUS_LOW_POWER_REG             0x31
#define BMA250_SELF_TEST_REG                    0x32
#define BMA250_EEPROM_CTRL_REG                  0x33
#define BMA250_SERIAL_CTRL_REG                  0x34
#define BMA250_CTRL_UNLOCK_REG                  0x35
#define BMA250_OFFSET_CTRL_REG                  0x36
#define BMA250_OFFSET_PARAMS_REG                0x37
#define BMA250_OFFSET_FILT_X_REG                0x38
#define BMA250_OFFSET_FILT_Y_REG                0x39
#define BMA250_OFFSET_FILT_Z_REG                0x3A
#define BMA250_OFFSET_UNFILT_X_REG              0x3B
#define BMA250_OFFSET_UNFILT_Y_REG              0x3C
#define BMA250_OFFSET_UNFILT_Z_REG              0x3D
#define BMA250_SPARE_0_REG                      0x3E
#define BMA250_SPARE_1_REG                      0x3F




#define BMA250_ACC_X_LSB__POS              6
#define BMA250_ACC_X_LSB__LEN              2
#define BMA250_ACC_X_LSB__MSK              0xC0
#define BMA250_ACC_X_LSB__REG              BMA250_X_AXIS_LSB_REG

#define BMA250_ACC_X_MSB__POS              0
#define BMA250_ACC_X_MSB__LEN              8
#define BMA250_ACC_X_MSB__MSK              0xFF
#define BMA250_ACC_X_MSB__REG              BMA250_X_AXIS_MSB_REG

#define BMA250_ACC_Y_LSB__POS              6
#define BMA250_ACC_Y_LSB__LEN              2
#define BMA250_ACC_Y_LSB__MSK              0xC0
#define BMA250_ACC_Y_LSB__REG              BMA250_Y_AXIS_LSB_REG

#define BMA250_ACC_Y_MSB__POS              0
#define BMA250_ACC_Y_MSB__LEN              8
#define BMA250_ACC_Y_MSB__MSK              0xFF
#define BMA250_ACC_Y_MSB__REG              BMA250_Y_AXIS_MSB_REG

#define BMA250_ACC_Z_LSB__POS              6
#define BMA250_ACC_Z_LSB__LEN              2
#define BMA250_ACC_Z_LSB__MSK              0xC0
#define BMA250_ACC_Z_LSB__REG              BMA250_Z_AXIS_LSB_REG

#define BMA250_ACC_Z_MSB__POS              0
#define BMA250_ACC_Z_MSB__LEN              8
#define BMA250_ACC_Z_MSB__MSK              0xFF
#define BMA250_ACC_Z_MSB__REG              BMA250_Z_AXIS_MSB_REG

#define BMA250_RANGE_SEL__POS              0
#define BMA250_RANGE_SEL__LEN              4
#define BMA250_RANGE_SEL__MSK              0x0F
#define BMA250_RANGE_SEL__REG              BMA250_RANGE_SEL_REG

#define BMA250_BANDWIDTH__POS              0
#define BMA250_BANDWIDTH__LEN              5
#define BMA250_BANDWIDTH__MSK              0x1F
#define BMA250_BANDWIDTH__REG              BMA250_BW_SEL_REG

#define BMA250_EN_LOW_POWER__POS           6
#define BMA250_EN_LOW_POWER__LEN           1
#define BMA250_EN_LOW_POWER__MSK           0x40
#define BMA250_EN_LOW_POWER__REG           BMA250_MODE_CTRL_REG

#define BMA250_EN_SUSPEND__POS             7
#define BMA250_EN_SUSPEND__LEN             1
#define BMA250_EN_SUSPEND__MSK             0x80
#define BMA250_EN_SUSPEND__REG             BMA250_MODE_CTRL_REG


/* add for fast calibration*/
#define BMA250_EN_FAST_COMP__POS           5
#define BMA250_EN_FAST_COMP__LEN           2
#define BMA250_EN_FAST_COMP__MSK           0x60
#define BMA250_EN_FAST_COMP__REG           BMA250_OFFSET_CTRL_REG

#define BMA250_FAST_COMP_RDY_S__POS        4
#define BMA250_FAST_COMP_RDY_S__LEN        1
#define BMA250_FAST_COMP_RDY_S__MSK        0x10
#define BMA250_FAST_COMP_RDY_S__REG        BMA250_OFFSET_CTRL_REG

#define BMA250_NVM_RDY__POS                2
#define BMA250_NVM_RDY__LEN                1
#define BMA250_NVM_RDY__MSK                0x04
#define BMA250_NVM_RDY__REG                BMA250_EEPROM_CTRL_REG

#define BMA250_NVM_PROG_TRIG__POS          1
#define BMA250_NVM_PROG_TRIG__LEN          1
#define BMA250_NVM_PROG_TRIG__MSK          0x02
#define BMA250_NVM_PROG_TRIG__REG          BMA250_EEPROM_CTRL_REG

#define BMA250_NVM_PROG_MODE__POS          0
#define BMA250_NVM_PROG_MODE__LEN          1
#define BMA250_NVM_PROG_MODE__MSK          0x01
#define BMA250_NVM_PROG_MODE__REG          BMA250_EEPROM_CTRL_REG


#define BMA250_COMP_TARGET_OFFSET_X__POS   1
#define BMA250_COMP_TARGET_OFFSET_X__LEN   2
#define BMA250_COMP_TARGET_OFFSET_X__MSK   0x06
#define BMA250_COMP_TARGET_OFFSET_X__REG   BMA250_OFFSET_PARAMS_REG

#define BMA250_COMP_TARGET_OFFSET_Y__POS   3
#define BMA250_COMP_TARGET_OFFSET_Y__LEN   2
#define BMA250_COMP_TARGET_OFFSET_Y__MSK   0x18
#define BMA250_COMP_TARGET_OFFSET_Y__REG   BMA250_OFFSET_PARAMS_REG

#define BMA250_COMP_TARGET_OFFSET_Z__POS   5
#define BMA250_COMP_TARGET_OFFSET_Z__LEN   2
#define BMA250_COMP_TARGET_OFFSET_Z__MSK   0x60
#define BMA250_COMP_TARGET_OFFSET_Z__REG   BMA250_OFFSET_PARAMS_REG



#define BMA250_GET_BITSLICE(regvar, bitname)\
			((regvar & bitname##__MSK) >> bitname##__POS)


#define BMA250_SET_BITSLICE(regvar, bitname, val)\
	((regvar & ~bitname##__MSK) | ((val<<bitname##__POS)&bitname##__MSK))


/* self test*/
#define BMA250_SELFTEST_X	1
#define BMA250_SELFTEST_Y	2
#define BMA250_SELFTEST_Z	3

struct bma250acc {
	s16 x,
	    y,
	    z;
};

struct bma250_data {
	struct i2c_client *bma250_client;
	struct bma250_platform_data *pdata;
	struct input_dev *input;
	struct bma250acc reported_value;
	struct mutex value_mutex;
	struct mutex enable_mutex;
	struct mutex mode_mutex;
	struct delayed_work work;
	struct regulator *vdd_regulator;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif
	atomic_t enable;
	unsigned char mode;
};

static int bma250_smbus_read_byte(struct i2c_client *client,
		unsigned char reg_addr, unsigned char *data)
{
	s32 dummy;

	dummy = i2c_smbus_read_byte_data(client, reg_addr);
	if (dummy < 0)
		return -1;
	*data = dummy & 0x000000ff;

	return 0;
}

static int bma250_smbus_write_byte(struct i2c_client *client,
		unsigned char reg_addr, unsigned char *data)
{
	s32 dummy;
	dummy = i2c_smbus_write_byte_data(client, reg_addr, *data);
	if (dummy < 0)
		return -1;
	return 0;
}

static int bma250_smbus_read_byte_block(struct i2c_client *client,
		unsigned char reg_addr, unsigned char *data, unsigned char len)
{
	s32 dummy;
	dummy = i2c_smbus_read_i2c_block_data(client, reg_addr, len, data);
	if (dummy < 0)
		return -1;
	return 0;
}

static int bma250_get_cal_ready(struct i2c_client *client, unsigned char *calrdy )
{
	int ret = 0 ;
	unsigned char data;
	ret	= bma250_smbus_read_byte(client, BMA250_OFFSET_CTRL_REG, &data);
	data	= BMA250_GET_BITSLICE(data, BMA250_FAST_COMP_RDY_S);
	*calrdy	= data;
	return ret;
}

static int bma250_set_cal_trigger(struct i2c_client *client, unsigned char caltrigger)
{
	int ret = 0;
	unsigned char data;
	ret	= bma250_smbus_read_byte(client, BMA250_EN_FAST_COMP__REG, &data);
	data	= BMA250_SET_BITSLICE(data, BMA250_EN_FAST_COMP, caltrigger );
	ret	= bma250_smbus_write_byte(client, BMA250_EN_FAST_COMP__REG, &data);
	return ret;
}

static int bma250_write_reg(struct i2c_client *client, unsigned char addr, unsigned char *data)
{
	int ret = 0 ;
	ret = bma250_smbus_write_byte(client, addr, data);
	return ret;
}


static int bma250_set_offset_target_x(struct i2c_client *client, unsigned char offsettarget)
{
	int ret = 0;
	unsigned char data;
	ret	= bma250_smbus_read_byte(client, BMA250_COMP_TARGET_OFFSET_X__REG, &data);
	data	= BMA250_SET_BITSLICE(data, BMA250_COMP_TARGET_OFFSET_X, offsettarget );
	ret	= bma250_smbus_write_byte(client, BMA250_COMP_TARGET_OFFSET_X__REG, &data);
	return ret;
}

static int bma250_write_eeprom(struct i2c_client *client)
{
	int ret = 0;
	unsigned char data;

	ret	= bma250_smbus_read_byte(client, BMA250_EEPROM_CTRL_REG, &data);
	data	= BMA250_SET_BITSLICE(data, BMA250_NVM_PROG_MODE, 1 );
	ret	= bma250_smbus_write_byte(client, BMA250_EEPROM_CTRL_REG, &data);
	data	= BMA250_SET_BITSLICE(data, BMA250_NVM_PROG_TRIG, 1 );
	ret	= bma250_smbus_write_byte(client, BMA250_EEPROM_CTRL_REG, &data);
	do {
		mdelay(1);
		ret = bma250_smbus_read_byte(client, BMA250_EEPROM_CTRL_REG, &data);	
	}
	while (!BMA250_GET_BITSLICE(data, BMA250_NVM_RDY));

	data	= BMA250_SET_BITSLICE(data, BMA250_NVM_PROG_MODE, 0 );
	ret	= bma250_smbus_write_byte(client, BMA250_EEPROM_CTRL_REG, &data);
	return ret;
}

static int bma250_get_offset_target_x(struct i2c_client *client, unsigned char *offsettarget )
{
	int ret = 0 ;
	unsigned char data;
	ret	= bma250_smbus_read_byte(client, BMA250_OFFSET_PARAMS_REG, &data);
	data	= BMA250_GET_BITSLICE(data, BMA250_COMP_TARGET_OFFSET_X);
	*offsettarget = data;
	return ret;
}

static int bma250_set_offset_target_y(struct i2c_client *client, unsigned char offsettarget)
{
	int ret = 0;
	unsigned char data;
	ret	= bma250_smbus_read_byte(client, BMA250_COMP_TARGET_OFFSET_Y__REG, &data);
	data	= BMA250_SET_BITSLICE(data, BMA250_COMP_TARGET_OFFSET_Y, offsettarget );
	ret	= bma250_smbus_write_byte(client, BMA250_COMP_TARGET_OFFSET_Y__REG, &data);
	return ret;
}

static int bma250_get_offset_target_y(struct i2c_client *client, unsigned char *offsettarget )
{
	int ret = 0 ;
	unsigned char data;
	ret	= bma250_smbus_read_byte(client, BMA250_OFFSET_PARAMS_REG, &data);
	data	= BMA250_GET_BITSLICE(data, BMA250_COMP_TARGET_OFFSET_Y);
	*offsettarget = data;
	return ret;
}

static int bma250_set_offset_target_z(struct i2c_client *client, unsigned char offsettarget)
{
	int ret = 0;
	unsigned char data;
	ret	= bma250_smbus_read_byte(client, BMA250_COMP_TARGET_OFFSET_Z__REG, &data);
	data	= BMA250_SET_BITSLICE(data, BMA250_COMP_TARGET_OFFSET_Z, offsettarget );
	ret	= bma250_smbus_write_byte(client, BMA250_COMP_TARGET_OFFSET_Z__REG, &data);
	return ret;
}

static int bma250_get_offset_target_z(struct i2c_client *client, unsigned char *offsettarget )
{
	int ret = 0 ;
	unsigned char data;
	ret	= bma250_smbus_read_byte(client, BMA250_OFFSET_PARAMS_REG, &data);
	data	= BMA250_GET_BITSLICE(data, BMA250_COMP_TARGET_OFFSET_Z);
	*offsettarget = data;
	return ret;
}

static int bma250_get_filt(struct i2c_client *client, unsigned char *filt, unsigned char axis)
{
	int ret = 0 ;
	unsigned char data;
	ret	= bma250_smbus_read_byte(client, (BMA250_OFFSET_FILT_X_REG + axis), &data);
	*filt	= data;
	return ret;
}

static int bma250_get_unfilt(struct i2c_client *client, unsigned char *unfilt, unsigned char axis)
{
	int ret = 0 ;
	unsigned char data;
	ret	= bma250_smbus_read_byte(client, (BMA250_OFFSET_UNFILT_X_REG + axis), &data);
	*unfilt	= data;
	return ret;
}

static int bma250_set_mode(struct i2c_client *client, unsigned char Mode)
{
	int ret = -1;
	unsigned char data1;
	struct bma250_data *bma250 = i2c_get_clientdata(client);

	if (Mode < 3) {
		ret = bma250_smbus_read_byte(bma250->bma250_client, BMA250_EN_LOW_POWER__REG, &data1);
		switch (Mode) {
			case BMA250_MODE_NORMAL:
				data1 = BMA250_SET_BITSLICE(data1, BMA250_EN_LOW_POWER, 0);
				data1 = BMA250_SET_BITSLICE(data1, BMA250_EN_SUSPEND, 0);
				break;
			case BMA250_MODE_LOWPOWER:
				data1 = BMA250_SET_BITSLICE(data1, BMA250_EN_LOW_POWER, 1);
				data1 = BMA250_SET_BITSLICE(data1, BMA250_EN_SUSPEND, 0);
				break;
			case BMA250_MODE_SUSPEND:
				data1 = BMA250_SET_BITSLICE(data1, BMA250_EN_LOW_POWER, 0);
				data1 = BMA250_SET_BITSLICE(data1, BMA250_EN_SUSPEND, 1);
				break;
			default:
				break;
		}

		ret += bma250_smbus_write_byte(bma250->bma250_client, BMA250_EN_LOW_POWER__REG, &data1);
	}

	return ret;
}

static void bma250_set_enable(struct device *dev, int enable)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bma250_data *bma250 = i2c_get_clientdata(client);
	int pre_enable = atomic_read(&bma250->enable);

	mutex_lock(&bma250->enable_mutex);
	if (enable) {
		if (pre_enable == 0) {
			bma250_set_mode(client, BMA250_MODE_NORMAL);
			schedule_delayed_work(&bma250->work, msecs_to_jiffies(bma250->pdata->poll_interval));
			atomic_set(&bma250->enable, 1);
		}

	} else {
		if (pre_enable == 1) {
			bma250_set_mode(client, BMA250_MODE_SUSPEND);
			cancel_delayed_work_sync(&bma250->work);
			atomic_set(&bma250->enable, 0);
		}
	}
	mutex_unlock(&bma250->enable_mutex);
}

static int bma250_set_range(struct i2c_client *client, u8 Range)
{
	int ret = -1;
	unsigned char data1;

	ret	= bma250_smbus_read_byte(client, BMA250_RANGE_SEL_REG, &data1);
	data1	= BMA250_SET_BITSLICE(data1, BMA250_RANGE_SEL, Range);
	ret	+= bma250_smbus_write_byte(client, BMA250_RANGE_SEL_REG, &data1);

	return ret;
}

static inline int bma250_bandwidth_handler(struct i2c_client *client, struct bma250_platform_data *pdata, unsigned int poll_interval)
{
	int ret = 0;
	unsigned char data;

	if (poll_interval > 40) {
		printk(KERN_ERR "bandwidth: BMA250_BW_31_25HZ\n");
		pdata->bandwidth = BMA250_BW_31_25HZ;
	}
	else if (poll_interval > 20) {
		printk(KERN_ERR "bandwidth: BMA250_BW_62_50HZ\n");
		pdata->bandwidth = BMA250_BW_62_50HZ;
	}
	else if (poll_interval > 8) {
		printk(KERN_ERR "bandwidth: BMA250_BW_125HZ\n");
		pdata->bandwidth = BMA250_BW_125HZ;
	}
	else if (poll_interval > 4) {
		printk(KERN_ERR "bandwidth: BMA250_BW_250HZ\n");
		pdata->bandwidth = BMA250_BW_250HZ;
	}
	else if (poll_interval > 2) {
		printk(KERN_ERR "bandwidth: BMA250_BW_500HZ\n");
		pdata->bandwidth = BMA250_BW_500HZ;
	}
	else {
		printk(KERN_ERR "bandwidth: BMA250_BW_1000HZ\n");
		pdata->bandwidth = BMA250_BW_1000HZ;
	}

	ret	= bma250_smbus_read_byte(client, BMA250_BANDWIDTH__REG, &data);
	data	= BMA250_SET_BITSLICE(data, BMA250_BANDWIDTH, pdata->bandwidth);
	ret	+= bma250_smbus_write_byte(client, BMA250_BANDWIDTH__REG, &data);

	return ret;
}


static int bma250_set_selftest(struct i2c_client *client, unsigned char AXIS, unsigned char SIGN)
{
	int ret = -1;
	unsigned char data = 0;
	int Axis = 0;

	switch (AXIS) {
	case 'x':
		Axis = BMA250_SELFTEST_X;
		data = Axis | (SIGN<<2);
		break;
	case 'y':
		Axis = BMA250_SELFTEST_Y;
		data = Axis | (SIGN<<2);
		break;
	case 'z':
		Axis = BMA250_SELFTEST_Z;
		data = Axis | (SIGN<<2);
		break;
	default:
		data = 0;
		break;
	}
	ret = bma250_smbus_write_byte(client, BMA250_SELF_TEST_REG, &data);

	return ret;
}

static int bma250_get_acceleration_data(struct i2c_client *client, struct bma250_data *bma250, s16 *xyz)
{
	int ret = -1;
	unsigned char data[6];

	ret = i2c_smbus_read_i2c_block_data(client, BMA250_ACC_X_LSB__REG, BMA250_XYZ_DATA_SIZE, data);
	if (ret != BMA250_XYZ_DATA_SIZE) {
		printk(KERN_ERR "bma250_get_acceleration_data: sensor read(%d) != BMA250_XYZ_DATA_SIZE(%d)\n", ret, BMA250_XYZ_DATA_SIZE);
		return -1;
	}

	/* 10bit signed to 16bit signed */
	xyz[0] = ((data[1] << 8) | (data[0] & 0xC0));
	xyz[1] = ((data[3] << 8) | (data[2] & 0xC0));
	xyz[2] = ((data[5] << 8) | (data[4] & 0xC0));

	/* sensitivty 256lsb/g for all g-ranges */
	xyz[0] >>= bma250->pdata->shift_adj;
	xyz[1] >>= bma250->pdata->shift_adj;
	xyz[2] >>= bma250->pdata->shift_adj;

	return 0;
}

static void bma250_report_xyz(struct bma250_data *bma250, s16 *xyz)
{
	struct bma250acc acc;

	acc.x = xyz[bma250->pdata->axis_map_x];
	acc.y = xyz[bma250->pdata->axis_map_y];
	acc.z = xyz[bma250->pdata->axis_map_z];

	if ((abs(acc.x - bma250->reported_value.x) >= bma250->pdata->report_threshold) ||
	    (abs(acc.y - bma250->reported_value.y) >= bma250->pdata->report_threshold) ||
	    (abs(acc.z - bma250->reported_value.z) >= bma250->pdata->report_threshold)) {
		input_report_abs(bma250->input, ABS_X, acc.x);
		input_report_abs(bma250->input, ABS_Y, acc.y);
		input_report_abs(bma250->input, ABS_Z, acc.z);
		input_sync(bma250->input);
		bma250->reported_value = acc;
		aprintk("BMA250 REPORT: %d %d %d\n", acc.x, acc.y, acc.z);
	}
}

static void bma250_work_func(struct work_struct *work)
{
	struct bma250_data *bma250 = container_of((struct delayed_work *)work, struct bma250_data, work);
	s16 xyz[3] = { 0 };
	int err;

	mutex_lock(&bma250->value_mutex);
	err = bma250_get_acceleration_data(bma250->bma250_client, bma250, xyz);
	if (err == 0)
		bma250_report_xyz(bma250, xyz);

	schedule_delayed_work(&bma250->work, msecs_to_jiffies(bma250->pdata->poll_interval));
	mutex_unlock(&bma250->value_mutex);
}

static ssize_t bma250_register_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int address, value;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma250_data *bma250 = i2c_get_clientdata(client);

	sscanf(buf, "%d%d", &address, &value);

	if (bma250_write_reg(bma250->bma250_client, (unsigned char)address, (unsigned char *)&value) < 0)
		return -EINVAL;

	return count;
}

static ssize_t bma250_register_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{

	struct i2c_client *client = to_i2c_client(dev);
	struct bma250_data *bma250 = i2c_get_clientdata(client);

	size_t count = 0;
	u8 reg[0x3d];
	int i;

	for(i =0; i < 0x3d; i++) {
		bma250_smbus_read_byte(bma250->bma250_client,i,reg+i);
		count += sprintf(&buf[count], "0x%x: %d\n", i, reg[i]);
	}
	return count;
}

static ssize_t bma250_report_threshold_show(struct device *dev,
                struct device_attribute *attr, char *buf)
{
        unsigned int data;
        struct i2c_client *client = to_i2c_client(dev);
        struct bma250_data *bma250 = i2c_get_clientdata(client);

	data = bma250->pdata->report_threshold;

        return sprintf(buf, "%d\n", data);
}

static ssize_t bma250_report_threshold_store(struct device *dev,
                struct device_attribute *attr,
                const char *buf, size_t count)
{
        unsigned long data;
        int error;
        struct i2c_client *client = to_i2c_client(dev);
        struct bma250_data *bma250 = i2c_get_clientdata(client);

        error = strict_strtoul(buf, 10, &data);
        if (error)
                return error;

	bma250->pdata->report_threshold = data;

        return count;
}

static ssize_t bma250_value_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bma250_data *bma250 = input_get_drvdata(input);
	struct bma250acc acc_value;

	mutex_lock(&bma250->value_mutex);
	acc_value = bma250->reported_value;
	mutex_unlock(&bma250->value_mutex);

	return sprintf(buf, "%d %d %d\n", acc_value.x, acc_value.y, acc_value.z);
}

static ssize_t bma250_delay_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bma250_data *bma250 = i2c_get_clientdata(client);

	return sprintf(buf, "%d\n", bma250->pdata->poll_interval);
}

static ssize_t bma250_delay_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long interval;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma250_data *bma250 = i2c_get_clientdata(client);

	aprintk("ENTER bma250_delay_store: %s", buf);
	error = strict_strtoul(buf, 10, &interval);
	if (error)
		return error;
	if (interval < 0 || interval > 200)
		return -EINVAL;
	if (interval > bma250->pdata->max_interval)
		interval = (unsigned long)bma250->pdata->max_interval;
	if (interval < bma250->pdata->min_interval)
		interval = (unsigned long)bma250->pdata->min_interval;
	bma250->pdata->poll_interval = (unsigned int)interval;
	bma250_bandwidth_handler(bma250->bma250_client, bma250->pdata, bma250->pdata->poll_interval);

	return count;
}

static ssize_t bma250_selftest_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char value[2];
	s16 temp;
	int error;
	long positive_values = 0;
	long negative_values = 0;
	long delta = 0;
	unsigned char i;
	struct i2c_client *client =  to_i2c_client(dev);
	struct bma250_data *bma250 = i2c_get_clientdata(client);

	error = bma250_set_selftest(bma250->bma250_client, 'x', 0);
	mdelay(10);
	for (i =0; i< 15; i++) {
		temp = 0;
		error = bma250_smbus_read_byte_block(bma250->bma250_client, BMA250_ACC_X_LSB__REG, value, 2);
		temp = BMA250_GET_BITSLICE(value[0], BMA250_ACC_X_LSB) | (BMA250_GET_BITSLICE(value[1], BMA250_ACC_X_MSB) << BMA250_ACC_X_LSB__LEN);
		temp = temp << (sizeof(short)*8-(BMA250_ACC_X_LSB__LEN + BMA250_ACC_X_MSB__LEN));
		temp = temp >> (sizeof(short)*8-(BMA250_ACC_X_LSB__LEN + BMA250_ACC_X_MSB__LEN));
		if (i >= 5)
			positive_values += temp;
	}
	positive_values /= 10;

	error = bma250_set_selftest(bma250->bma250_client, 'x', 1);
	mdelay(10);

	for (i =0; i< 15; i++) {
		temp = 0;
		error = bma250_smbus_read_byte_block(bma250->bma250_client, BMA250_ACC_X_LSB__REG, value, 2);
		temp = BMA250_GET_BITSLICE(value[0], BMA250_ACC_X_LSB) | (BMA250_GET_BITSLICE(value[1], BMA250_ACC_X_MSB) << BMA250_ACC_X_LSB__LEN);
		temp = temp << (sizeof(short)*8-(BMA250_ACC_X_LSB__LEN + BMA250_ACC_X_MSB__LEN));
		temp = temp >> (sizeof(short)*8-(BMA250_ACC_X_LSB__LEN + BMA250_ACC_X_MSB__LEN));
		if (i >=5 )
			negative_values += temp;
	}
	negative_values /= 10;
	delta = positive_values - negative_values;

	error = bma250_set_selftest(bma250->bma250_client, 'a', 0);
	if (delta < 205)
		return sprintf(buf, "Fail\n");		

	error = bma250_set_selftest(bma250->bma250_client, 'y', 0);
	mdelay(10);

	for (i =0; i< 15; i++) {
		temp = 0;
		error = bma250_smbus_read_byte_block(bma250->bma250_client, BMA250_ACC_Y_LSB__REG, value, 2);
		temp = BMA250_GET_BITSLICE(value[0], BMA250_ACC_Y_LSB) | (BMA250_GET_BITSLICE(value[1], BMA250_ACC_Y_MSB)<<BMA250_ACC_Y_LSB__LEN);
		temp = temp << (sizeof(short)*8-(BMA250_ACC_Y_LSB__LEN + BMA250_ACC_Y_MSB__LEN));
		temp = temp >> (sizeof(short)*8-(BMA250_ACC_Y_LSB__LEN + BMA250_ACC_Y_MSB__LEN));
		if (i >= 5)
		positive_values += temp;
	}
	positive_values /= 10;

	error = bma250_set_selftest(bma250->bma250_client, 'y', 1);
	mdelay(10);

	for (i =0; i< 15; i++) {
		temp = 0;
		error = bma250_smbus_read_byte_block(bma250->bma250_client, BMA250_ACC_Y_LSB__REG, value, 2);
		temp = BMA250_GET_BITSLICE(value[0], BMA250_ACC_Y_LSB) | (BMA250_GET_BITSLICE(value[1], BMA250_ACC_Y_MSB) << BMA250_ACC_Y_LSB__LEN);
		temp = temp << (sizeof(short)*8-(BMA250_ACC_Y_LSB__LEN + BMA250_ACC_Y_MSB__LEN));
		temp = temp >> (sizeof(short)*8-(BMA250_ACC_Y_LSB__LEN + BMA250_ACC_Y_MSB__LEN));
		if (i >=5 )
			negative_values += temp;
	}
	negative_values /= 10;
	delta = positive_values - negative_values;	

	error = bma250_set_selftest(bma250->bma250_client, 'a', 0);
	if (delta < 205)
		return sprintf(buf, "Fail\n");

	error = bma250_set_selftest(bma250->bma250_client, 'z', 0);
	mdelay(10);

	for (i =0; i< 15; i++) {
		temp = 0;
		error = bma250_smbus_read_byte_block(bma250->bma250_client, BMA250_ACC_Z_LSB__REG, value, 2);
		temp = BMA250_GET_BITSLICE(value[0], BMA250_ACC_Z_LSB) | (BMA250_GET_BITSLICE(value[1], BMA250_ACC_Z_MSB)<<BMA250_ACC_Z_LSB__LEN);
		temp = temp << (sizeof(short)*8-(BMA250_ACC_Z_LSB__LEN + BMA250_ACC_Z_MSB__LEN));
		temp = temp >> (sizeof(short)*8-(BMA250_ACC_Z_LSB__LEN + BMA250_ACC_Z_MSB__LEN));
		if (i >=5 )
			positive_values += temp;
	}
	positive_values /= 10;

	error = bma250_set_selftest(bma250->bma250_client, 'z', 1);
	mdelay(10);

	for (i =0; i< 15; i++) {
		temp = 0;
		error = bma250_smbus_read_byte_block(bma250->bma250_client, BMA250_ACC_Z_LSB__REG, value, 2);
		temp = BMA250_GET_BITSLICE(value[0], BMA250_ACC_Z_LSB) | (BMA250_GET_BITSLICE(value[1], BMA250_ACC_Z_MSB)<<BMA250_ACC_Z_LSB__LEN);
		temp = temp << (sizeof(short)*8-(BMA250_ACC_Z_LSB__LEN + BMA250_ACC_Z_MSB__LEN));
		temp = temp >> (sizeof(short)*8-(BMA250_ACC_Z_LSB__LEN + BMA250_ACC_Z_MSB__LEN));
		if (i >=5 )
			negative_values += temp;
	}
	negative_values /= 10;
	delta = positive_values - negative_values;		

	error = bma250_set_selftest(bma250->bma250_client, 'a', 0);
	if (delta <= 102)
		return sprintf(buf, "Fail\n");
	if (error < 0)
		return sprintf(buf, "Unknown Error\n");
	return sprintf(buf, "Pass\n");
}

static ssize_t bma250_enable_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;

	aprintk("ENTER bma250_enable_store: %s", buf);
	error = strict_strtoul(buf, 10, &data);
	if (error)
		return error;
	if ((data == 0) || (data == 1)) {
		bma250_set_enable(dev, data);
	}

	return count;
}

static ssize_t bma250_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bma250_data *bma250 = i2c_get_clientdata(client);

	return sprintf(buf, "%d\n", atomic_read(&bma250->enable));
}


static ssize_t bma250_fast_calibration_x_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma250_data *bma250 = i2c_get_clientdata(client);

	if (bma250_get_offset_target_x(bma250->bma250_client, &data) < 0)
		return sprintf(buf, "Read error\n");

	return sprintf(buf, "%d\n", data);
}

static ssize_t bma250_fast_calibration_x_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	signed char tmp;
	unsigned char timeout = 0;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma250_data *bma250 = i2c_get_clientdata(client);

	error = strict_strtoul(buf, 10, &data);
	if (error)
		return error;

	if (bma250_set_offset_target_x(bma250->bma250_client, (unsigned char)data) < 0)
		return -EINVAL;

	if (bma250_set_cal_trigger(bma250->bma250_client, 1) < 0)
		return -EINVAL;

	do {
		mdelay(2);
		bma250_get_cal_ready(bma250->bma250_client, &tmp);
		printk(KERN_INFO "wait 2ms and got cal ready flag is %d\n",tmp);
		timeout++;
		if (timeout == 50) {
			printk(KERN_INFO "get fast calibration ready error\n");
			return -EINVAL;
		};

	} while (tmp == 0);
	printk(KERN_INFO "x axis fast calibration finished\n");
	return count;
}

static ssize_t bma250_fast_calibration_y_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma250_data *bma250 = i2c_get_clientdata(client);

	if (bma250_get_offset_target_y(bma250->bma250_client, &data) < 0)
		return sprintf(buf, "Read error\n");

	return sprintf(buf, "%d\n", data);
}

static ssize_t bma250_fast_calibration_y_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	signed char tmp;
	unsigned char timeout = 0;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma250_data *bma250 = i2c_get_clientdata(client);

	error = strict_strtoul(buf, 10, &data);
	if (error)
		return error;

	if (bma250_set_offset_target_y(bma250->bma250_client, (unsigned char)data) < 0)
		return -EINVAL;

	if (bma250_set_cal_trigger(bma250->bma250_client, 2) < 0)
		return -EINVAL;

	do {
		mdelay(2);
		bma250_get_cal_ready(bma250->bma250_client, &tmp);

		printk(KERN_INFO "wait 2ms and got cal ready flag is %d\n",tmp);
		timeout++;
		if(timeout==50) {
			printk(KERN_INFO "get fast calibration ready error\n");
			return -EINVAL;
		};

	} while (tmp == 0);

	printk(KERN_INFO "y axis fast calibration finished\n");
	return count;
}

static ssize_t bma250_fast_calibration_z_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma250_data *bma250 = i2c_get_clientdata(client);

	if (bma250_get_offset_target_z(bma250->bma250_client, &data) < 0)
		return sprintf(buf, "Read error\n");

	return sprintf(buf, "%d\n", data);
}

static ssize_t bma250_fast_calibration_z_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	signed char tmp;
	unsigned char timeout = 0;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma250_data *bma250 = i2c_get_clientdata(client);

	error = strict_strtoul(buf, 10, &data);
	if (error)
		return error;

	if (bma250_set_offset_target_z(bma250->bma250_client, (unsigned char)data) < 0)
		return -EINVAL;

	if (bma250_set_cal_trigger(bma250->bma250_client, 3) < 0)
		return -EINVAL;

	do {
		mdelay(2);
		bma250_get_cal_ready(bma250->bma250_client, &tmp);

		printk(KERN_INFO "wait 2ms and got cal ready flag is %d\n",tmp);
		timeout++;
		if (timeout == 50) {
			printk(KERN_INFO "get fast calibration ready error\n");
			return -EINVAL;
		};

	} while (tmp == 0);

	printk(KERN_INFO "z axis fast calibration finished\n");
	return count;
}

static ssize_t bma250_eeprom_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data[6];
	struct i2c_client *client = to_i2c_client(dev);
	struct bma250_data *bma250 = i2c_get_clientdata(client);

	if (bma250_get_filt(bma250->bma250_client, data, 0) < 0)
		return sprintf(buf, "Read error\n");
	if (bma250_get_filt(bma250->bma250_client, (data+1), 1) < 0)
		return sprintf(buf, "Read error\n");
	if (bma250_get_filt(bma250->bma250_client, (data+2), 2) < 0)
		return sprintf(buf, "Read error\n");

	if (bma250_get_unfilt(bma250->bma250_client, (data+3), 0) < 0)
		return sprintf(buf, "Read error\n");
	if (bma250_get_unfilt(bma250->bma250_client, (data+4), 1) < 0)
		return sprintf(buf, "Read error\n");
	if (bma250_get_unfilt(bma250->bma250_client, (data+5), 2) < 0)
		return sprintf(buf, "Read error\n");
	return sprintf(buf, "FIL X:%d Y:%d Z:%d\nUNFIL: X:%d Y:%d Z:%d\n", data[0], data[1], data[2], data[3], data[4], data[5]);
}

static ssize_t bma250_eeprom_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bma250_data *bma250 = i2c_get_clientdata(client);

	bma250_write_eeprom(bma250->bma250_client);
	return count;
}


static DEVICE_ATTR(report_threshold, S_IRUGO|S_IWUSR|S_IWGRP,
                bma250_report_threshold_show, bma250_report_threshold_store);
static DEVICE_ATTR(value, S_IRUGO|S_IWUSR|S_IWGRP,
		bma250_value_show, NULL);
static DEVICE_ATTR(delay, S_IRUGO|S_IWUSR|S_IWGRP,
		bma250_delay_show, bma250_delay_store);
static DEVICE_ATTR(selftest, S_IRUGO|S_IWUSR|S_IWGRP,
		bma250_selftest_show, NULL);
static DEVICE_ATTR(enable, S_IRUGO|S_IWUSR|S_IWGRP|S_IWOTH,
		bma250_enable_show, bma250_enable_store);
static DEVICE_ATTR(reg, S_IRUGO|S_IWUSR|S_IWGRP|S_IWOTH,
		bma250_register_show, bma250_register_store);
static DEVICE_ATTR(fast_calibration_x, S_IRUGO|S_IWUSR|S_IWGRP|S_IWOTH,
		bma250_fast_calibration_x_show, bma250_fast_calibration_x_store);
static DEVICE_ATTR(fast_calibration_y, S_IRUGO|S_IWUSR|S_IWGRP|S_IWOTH,
		bma250_fast_calibration_y_show, bma250_fast_calibration_y_store);
static DEVICE_ATTR(fast_calibration_z, S_IRUGO|S_IWUSR|S_IWGRP|S_IWOTH,
		bma250_fast_calibration_z_show, bma250_fast_calibration_z_store);
static DEVICE_ATTR(eeprom, S_IRUGO|S_IWUSR|S_IWGRP|S_IWOTH,
		bma250_eeprom_show, bma250_eeprom_store);
static struct attribute *bma250_attributes[] = {
	&dev_attr_report_threshold.attr,
	&dev_attr_value.attr,
	&dev_attr_delay.attr,
	&dev_attr_selftest.attr,
	&dev_attr_enable.attr,
	&dev_attr_reg.attr,
	&dev_attr_fast_calibration_x.attr,
	&dev_attr_fast_calibration_y.attr,
	&dev_attr_fast_calibration_z.attr,
	&dev_attr_eeprom.attr,	
	NULL
};

static struct attribute_group bma250_attribute_group = {
	.attrs = bma250_attributes
};

static int bma250_detect(struct i2c_client *client, struct i2c_board_info *info) {
	struct i2c_adapter *adapter = client->adapter;

	if (!i2c_check_functionality(adapter, I2C_FUNC_I2C))
		return -ENODEV;

	strlcpy(info->type, BMA250_DRIVER, I2C_NAME_SIZE);

	return 0;
}

static int bma250_probe(struct i2c_client *client, const struct i2c_device_id *id) {
	int err = 0;
	int tempvalue;
	struct regulator *temp;
	struct bma250_data *data;
	struct input_dev *dev;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		dev_err(&client->dev, "SMBUS Byte Data not Supported\n");
		return -EIO;
	}

	data = kzalloc(sizeof(struct bma250_data), GFP_KERNEL);
	if (!data) {
		dev_err(&client->dev, "failed to allocate memory for module data\n");
		err = -ENOMEM;
		goto exit;
	}
	if (client->dev.platform_data == NULL) {
		dev_err(&client->dev, "platform data is NULL .. exiting\n");
		err = -ENODEV;
		goto exit;
	}

	data->pdata = kmalloc(sizeof(*data->pdata), GFP_KERNEL);
	if (data->pdata == NULL) {
		dev_err(&client->dev, "failed to allocate memory for platform data\n");
		err = -ENOMEM;
		goto kfree_exit;
	}
	memcpy(data->pdata, client->dev.platform_data, sizeof(*data->pdata));

	/*For g sensor*/
	if (data->pdata->regulator_name) {
		data->vdd_regulator = regulator_get(NULL, data->pdata->regulator_name);
		if (IS_ERR(data->vdd_regulator)) {
			dev_err(&client->dev, "[%s] regulator_get error\n", __func__);
			goto pdatafree_exit;
		}
		err = regulator_set_voltage(data->vdd_regulator, data->pdata->min_voltage, data->pdata->max_voltage);
		if (err) {
			dev_err(&client->dev, "[%s]: regulator_set_voltage %d<-->%d error\n", __func__, data->pdata->min_voltage, data->pdata->max_voltage);
			goto pdatafree_exit;
		}
		regulator_enable(data->vdd_regulator);
		temp = data->vdd_regulator;
	}

	// Apply settings to get VAUX2 belonging to APP groug, set 0x1 to VAUX2_CFG_GRP
	twl_i2c_write_u8(TWL6030_MODULE_ID0, 0x01, 0x88);
	udelay(5);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		printk(KERN_INFO "i2c_check_functionality error\n");
		goto pdatafree_exit;
	}

	/* read chip id */
	tempvalue = 0;
	tempvalue = i2c_smbus_read_word_data(client, BMA250_CHIP_ID_REG);

	if ((tempvalue & 0x00FF) == BMA250_CHIP_ID) {
		printk(KERN_INFO "Bosch Sensortec Device detected\nBMA250 registered I2C driver!\n");
	} else{
		printk(KERN_INFO "Bosch Sensortec Device not found\ni2c error %d\n", tempvalue);
		err = -ENODEV;
		goto pdatafree_exit;
	}

	i2c_set_clientdata(client, data);
	data->bma250_client = client;
	mutex_init(&data->value_mutex);
	mutex_init(&data->mode_mutex);
	mutex_init(&data->enable_mutex);
	bma250_bandwidth_handler(client, data->pdata, data->pdata->poll_interval);
	bma250_set_range(client, data->pdata->range);

	INIT_DELAYED_WORK(&data->work, bma250_work_func);
	atomic_set(&data->enable, 0);
	dev = input_allocate_device();
	if (!dev) {
		dev_err(&client->dev, "[%s]: input_allocate_device failed\n", __func__);
		err = -ENOMEM;
		goto pdatafree_exit;
	}
	dev->name = BMA250_DRIVER;
	dev->id.bustype = BUS_I2C;

	__set_bit(EV_ABS, dev->evbit);
	__set_bit(ABS_X, dev->absbit);
	__set_bit(ABS_Y, dev->absbit);
	__set_bit(ABS_Z, dev->absbit);
	input_set_abs_params(dev, ABS_X, -4096, 4095, 0, 0);
	input_set_abs_params(dev, ABS_Y, -4096, 4095, 0, 0);
	input_set_abs_params(dev, ABS_Z, -4096, 4095, 0, 0);

	input_set_drvdata(dev, data);

	err = input_register_device(dev);
	if (err < 0) {
		input_free_device(dev);
		goto kfree_exit;
	}

	data->input = dev;

	err = sysfs_create_group(&client->dev.kobj, &bma250_attribute_group);
	if (err < 0)
		goto error_sysfs;

#ifdef CONFIG_HAS_EARLYSUSPEND
	data->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	data->early_suspend.suspend = bma250_early_suspend;
	data->early_suspend.resume = bma250_late_resume;
	register_early_suspend(&data->early_suspend);
#endif
	
	mutex_init(&data->value_mutex);
	mutex_init(&data->mode_mutex);
	mutex_init(&data->enable_mutex);
//	bma250_set_enable(&client->dev, 1);
	return 0;

error_sysfs:
	input_unregister_device(data->input);

pdatafree_exit:
	kfree(data->pdata);
kfree_exit:
	kfree(data);
exit:
	return err;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void bma250_early_suspend(struct early_suspend *h)
{
	struct bma250_data *data = container_of(h, struct bma250_data, early_suspend);

	mutex_lock(&data->enable_mutex);
	if (atomic_read(&data->enable)==1) {
		bma250_set_mode(data->bma250_client, BMA250_MODE_SUSPEND);
		cancel_delayed_work_sync(&data->work);
	}
	mutex_unlock(&data->enable_mutex);
}


static void bma250_late_resume(struct early_suspend *h)
{
	struct bma250_data *data = container_of(h, struct bma250_data, early_suspend);

	mutex_lock(&data->enable_mutex);
	if (atomic_read(&data->enable)==1) {
		bma250_set_mode(data->bma250_client, BMA250_MODE_NORMAL);
		schedule_delayed_work(&data->work, msecs_to_jiffies(data->pdata->poll_interval));
	}
	mutex_unlock(&data->enable_mutex);
}
#endif

static void bma250_shutdown(struct i2c_client *client)
{
	struct bma250_data *data = i2c_get_clientdata(client);
	printk(KERN_DEBUG "%s\n", __func__);

	mutex_lock(&data->enable_mutex);
	if (atomic_read(&data->enable)==1) {
		bma250_set_mode(data->bma250_client, BMA250_MODE_SUSPEND);
		cancel_delayed_work_sync(&data->work);
	}
	mutex_unlock(&data->enable_mutex);
}

static int __devexit bma250_remove(struct i2c_client *client)
{
	struct bma250_data *data = i2c_get_clientdata(client);

	bma250_set_enable(&client->dev, 0);
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&data->early_suspend);
#endif
	sysfs_remove_group(&data->input->dev.kobj, &bma250_attribute_group);
	input_unregister_device(data->input);
	kfree(data);

	return 0;
}


static const struct i2c_device_id bma250_id[] = {
	{ "bma250", 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, bma250_id);

static struct i2c_driver bma250_i2c_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= BMA250_DRIVER,
	},
	.probe		= bma250_probe,
	.remove		= __devexit_p(bma250_remove),
	.detect		= bma250_detect,
	.shutdown	= bma250_shutdown,
	.id_table	= bma250_id,
};

static int __init BMA250_init(void)
{
	return i2c_add_driver(&bma250_i2c_driver);
}

static void __exit BMA250_exit(void)
{
	i2c_del_driver(&bma250_i2c_driver);
}

module_init(BMA250_init);
module_exit(BMA250_exit);

MODULE_AUTHOR("Bosch Sensortec GmbH");
MODULE_DESCRIPTION("BMA250 driver");
MODULE_LICENSE("GPL");
