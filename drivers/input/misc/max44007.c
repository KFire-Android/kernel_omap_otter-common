/**
 * Copyright (C) 2011 Maxim Integrated Products
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */

#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/leds.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>
#include <linux/spinlock.h>
#include <linux/input/max44007.h>
#ifndef CONFIG_MACH_OMAP4_BOWSER_SUBTYPE_TATE
#include <mach/bowser_idme_init.h>
#endif

static void max44007_shutdown(struct i2c_client *client);
static int max44007_suspend(struct i2c_client *client, pm_message_t mesg);
static int max44007_resume(struct i2c_client *client);
static int max44007_probe(struct i2c_client *client,
						  const struct i2c_device_id *id );
static int max44007_read_ALS(struct MAX44007Data *alsDevice,
							 int *adjusted_lux);
static int max44007_remove(struct i2c_client *client);
static int max44007_initialize_device(struct MAX44007Data *device,
									  bool use_defaults);
static int max44007_get_trims(struct MAX44007Data *device);
static int max44007_set_cal_trims(struct MAX44007Data *device, struct MAX44007CalData constant, bool reset);
static irqreturn_t max44007_irq_handler(int irq, void *device);
static void max44007_work_queue(struct work_struct *work);
static int max44007_sleep(struct MAX44007Data *device);
static int max44007_wake(struct MAX44007Data *device);


#undef DEBUGD
//#define DEBUGD
#ifdef DEBUGD
#define dprintk(x...) printk(x)
#else
#define dprintk(x...)
#endif

#define MAX44007_FTM_PORTING
struct MAX44007Data *Public_deviceData;
struct MAX44007CalData cal_constant;
int ALS_val=0;
int als_enable = 0;
int transmittance = 1;

//enable attribute
static ssize_t max44007_enable_show(struct device *dev,
struct  device_attribute *attr , char *buf)
{
	return sprintf(buf, "%d\n", als_enable);
}

static ssize_t max44007_enable_store(struct device *dev,
	struct  device_attribute *attr,
					const char *buf, size_t size)
{
	u8 value = 0;

	if (sscanf(buf, "%hhx", &value) <= 0) {
		printk(KERN_ERR "Error setting the value in the register\n");
		return -EINVAL;
	}
	if(Public_deviceData == NULL){
		return 0;
	}
	if(als_enable != value && value <= 1 && value >= 0){
		if(value == 0){
			max44007_sleep(Public_deviceData);
		}else{
			max44007_wake(Public_deviceData);
		}
	}
	return size;
}

//ALS_val
static ssize_t max44007_ALS_val_show(struct device *dev, struct  device_attribute *attr, char *buf)
{
	if(Public_deviceData == NULL){
		return 0;
	}
	max44007_read_ALS(Public_deviceData, &ALS_val);
	return sprintf(buf, "%d\n", ALS_val);
}

static ssize_t max44007_ALS_dump_show(struct device *dev, struct  device_attribute *attr, char *buf)
{
	u8 address=0;
	int value =0;
	int buf_size = 0;
	if(Public_deviceData == NULL){
		return 0;
	}
	for (address = 0; address < (MAX44007_OTP_REG+1) ;address ++) {
		u8 buffer = address;
		max44007_read_reg(Public_deviceData, &buffer, 1);
		value = sprintf(buf, "Address: 0x%2x, Value = 0x%2x\n", address, buffer);
		buf += value;
		buf_size += value;
		}
	return buf_size;
}

static ssize_t max44007_transmittance_show(struct device *dev, struct  device_attribute *attr, char *buf)
{
    int als_transmittance;
    als_transmittance = transmittance;
    return sprintf(buf, "%d\n", als_transmittance);
}

static ssize_t max44007_transmittance_store(struct device *dev,
	struct  device_attribute *attr,
					const char *buf, size_t size)
{
    unsigned long value = simple_strtoul(buf, NULL, 10);
    transmittance = value;
    return size;

}

static ssize_t max44007_greenconst_show(struct device *dev, struct  device_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", cal_constant.greenConstant);
}

static ssize_t max44007_greenconst_store(struct device *dev,
	struct  device_attribute *attr,
					const char *buf, size_t size)
{
    cal_constant.greenConstant = simple_strtoul(buf, NULL, 10);
    return size;
}

static ssize_t max44007_irconst_show(struct device *dev, struct  device_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", cal_constant.irConstant);
}

static ssize_t max44007_irconst_store(struct device *dev,
	struct  device_attribute *attr,
					const char *buf, size_t size)
{
    cal_constant.irConstant = simple_strtoul(buf, NULL, 10);
    return size;

}

static ssize_t max44007_calibration_store(struct device *dev,
	struct  device_attribute *attr,
					const char *buf, size_t size)
{
    if(Public_deviceData == NULL){
	return 0;
    }
#ifdef CONFIG_MACH_OMAP4_BOWSER_SUBTYPE_TATE
    max44007_get_trims(Public_deviceData);
#endif
    max44007_set_cal_trims(Public_deviceData, cal_constant, false);
    return size;
}

static  DEVICE_ATTR(enable, 0644, max44007_enable_show, max44007_enable_store);
static  DEVICE_ATTR(ALS_val, 0444, max44007_ALS_val_show, NULL);
static  DEVICE_ATTR(ALS_dump, 0444, max44007_ALS_dump_show, NULL);
static  DEVICE_ATTR(transmittance, 0644, max44007_transmittance_show, max44007_transmittance_store);
static  DEVICE_ATTR(greenconst, 0644, max44007_greenconst_show, max44007_greenconst_store);
static  DEVICE_ATTR(irconst, 0644, max44007_irconst_show, max44007_irconst_store);
static  DEVICE_ATTR(calibration, 0644, NULL, max44007_calibration_store);

static struct attribute *max44007_attributes[] = {
	&dev_attr_enable.attr,
	&dev_attr_ALS_val.attr,
	&dev_attr_ALS_dump.attr,
	&dev_attr_transmittance.attr,
	&dev_attr_greenconst.attr,
	&dev_attr_irconst.attr,
	&dev_attr_calibration.attr,
	NULL
};

static const struct attribute_group max44007_attr_group = {
	.attrs = max44007_attributes,
};

/**
 *  Performs a write of one or more bytes to a target MAX44007 device.
 *
 *  Will attempt to write to the device several times, pausing slightly
 *  between each writing attempt.
 *  Returns 0 upon success or a negative error code in the case of a error
 *  Please consult the MAX44007 datasheet for further information on
 *  communicating with the MAX44007 over I2C.
 *
 *  Recommended usage:
 *    Put the desired register address in buffer[0], then the data in buffer[1]
 *
 *  Author's note:
 *    The MAX44007 does **not** auto-incremement the register pointer when
 *    doing multiple reads or writes. There should not be an issue so long as
 *    you write only one register at a time, using the frame formatting below.
 *
 *    Frame format for single write:
 *    [S] [Slave Addr + 0] [ACK] [Register] [ACK] [Data] [ACK] [P]
 *                                  ^^buffer[0]     ^^buffer[1]
 *
 *	@param struct MAX44007Data *device
 *	  a descriptor representing the MAX44007 as an object
 *  @param u8 *buffer
 *	  points to data to transfer
 *  @param int length
 *	  number of bytes to transfer (capped at ~64k)
 *    Note: This is typically the length of the data + address,
 *          which is one more than expected by other functions.
 *	@return
 *	  An error code (0 on success)
 */
int max44007_write_reg(struct MAX44007Data *device,
					   u8 *buffer, int length)
{
	int err;		// error code
	int tries = 0;	// retries

	dprintk(KERN_INFO "max44007_write_reg: 0x%02x 0x%02x\n",
			buffer[0], buffer[1]);

	do
	{
		err = i2c_master_send(device->client, buffer, length);
		if (err != 2)
			msleep_interruptible(MAX44007_RETRY_DELAY);
	} while ((err != 2) && (++tries < MAX44007_MAX_RETRIES));

	if (err != 2)
	{
		printk("%s: write transfer error err=%d\n", __func__, err);
		err = -EIO;
	}
	else
		err = 0;

	return err;
} // max44007_write_reg


/**
 *  Performs a read of one or more bytes to a target MAX44007 device.
 *  Will attempt to read from the device several times, pausing slightly
 *  between each attempt.
 *  Returns 0 upon success or a negative error code in the case of a error
 *  Please consult the MAX44007 datasheet for further information on
 *  communicating with the MAX44007 over I2C.
 *
 *  Recommended usage:
 *    buffer[0] contains the register addr to read when the function is
 *    called and is then overwritten with the data in that register.
 *    Same goes for buffer[1]...buffer[n], so long as the frame format
 *    discussed below is used
 *    For example,
 *      Before:     buffer[] has {0x01,0x02}
 *      After:      buffer[] has {0x24,0x30}
 *
 *  Author's note:
 *    The MAX44007 does **not** auto-incremement the register pointer when
 *    doing multiple reads or writes. To do consecutive reads/writes, you must
 *    follow the frame format described on pages 14-16 of the datasheet.
 *    To accomplish this, you should ensure that the implementation of the I2C
 *    protocol that your codebase uses supports this format.
 *
 *    Frame format for single read:
 *    [S] [Slave Addr + 0] [ACK] [Register] [ACK] [Data] [ACK] [P]
 *
 *    Frame format for two byte read:
 *    [S] [Slave Addr + 0] [ACK] [Register 1] [ACK] [Sr] [Slave Addr + 1]
 *    [ACK] [Data 1] [NACK] [Sr] [S] [Slave Addr + 0] [ACK] [Register 2] [ACK]
 *    [Sr] [Slave Addr + 1] [ACK] [Data 2] [NACK] [P]
 *
 *  @param struct MAX44007Data *device
 *    a descriptor representing the MAX44007 as an object
 *  @param u8 *buffer
 *    points to data to transfer
 *  @param int length
 *    number of bytes to transfer (capped at ~64k)
 *  @return
 *    An error code (0 on success)
 */
int max44007_read_reg(struct MAX44007Data *device, u8 *buffer, int length)
{
	int err;		// error code
	int tries = 0;	// retries
#ifdef DEBUGD
	u8 addr = *buffer;
#endif

	struct i2c_msg msgs[] =
	{
		{
			.addr = device->client->addr,
			.flags = device->client->flags,
			.len = 1,
			.buf = buffer,
		}, // first message is the register address
		{
			.addr = device->client->addr,
			.flags = device->client->flags | I2C_M_RD,
			.len = length,
			.buf = buffer,
		}, // rest of the message
	};

	// perform the i2c transfer of two messages, one of length 1 and the
	// other of length 'len' (i.e. first send the register to be read, then
	// get all the data
	do
	{
		err = i2c_transfer(device->client->adapter, msgs, 2);
		if (err != 2)
			msleep_interruptible(MAX44007_RETRY_DELAY);
	} while ((err != 2) && (++tries < MAX44007_MAX_RETRIES));

	if (err != 2)
	{
		printk("%s: read transfer error err=%d\n", __func__, err);
		err = -EIO;
	}
	else
	{
		err = 0;
	}

	dprintk(KERN_INFO "max44007_read_reg 0x%02x 0x%02x\n",
		addr,
		buffer[0]);

	// return error code (0 if no error)
	return err;
} // MAX44007ReadReg


/**
 *  Performs a special read of two bytes to a target MAX44007 device.
 *  This follows the recommended method of reading the high and low lux
 *  values per the application note AN5033.pdf "The Importance of Being
 *  Earnest (About Reading from an ADC on the I2C Interface".
 *
 *  This special format for two byte read avoids the stop between bytes.
 *  Otherwise the stop would allow the MAX44007 to update the low lux byte,
 *  and the two bytes could be out of sync:
 *    [S] [Slave Addr + 0] [ACK] [Register 1] [ACK] [Sr] [Slave Addr + 1]
 *    [ACK] [Data 1] [NACK] [Sr] [S] [Slave Addr + 0] [ACK] [Register 2] [ACK]
 *    [Sr] [Slave Addr + 1] [ACK] [Data 2] [NACK] [P]
 *
 *  @param struct MAX44007Data *device
 *    a descriptor representing the MAX44007 as an object
 *  @param u8 *buffer
 *    points to data to transfer
 *    It will initially be the two register addresses.
 *    It will finally be the two register values.
 *  @return
 *    An error code (0 on success)
 */
int max44007_read_reg2(struct MAX44007Data *device, u8 *buffer)
{
	int err;		// error code
	int tries = 0;	// retries
#ifdef DEBUGD
	u8 addr = *buffer;
#endif

	struct i2c_msg msgs[] =
	{
		{
			.addr = device->client->addr,
			.flags = device->client->flags,
			.len = 1,
			.buf = buffer,
		}, // first message is the register address
		{
			.addr = device->client->addr,
			.flags = device->client->flags | I2C_M_RD,
			.len = 1,
			.buf = buffer,
		}, // second message is the read length
		{
			.addr = device->client->addr,
			.flags = device->client->flags,
			.len = 1,
			.buf = &(buffer[1]),
		}, // third message is the next register address
		{
			.addr = device->client->addr,
			.flags = device->client->flags | I2C_M_RD,
			.len = 1,
			.buf = &(buffer[1]),
		}, // fourth message is the next read length
	};

	// perform the i2c transfer of two messages, one of length 1 and the
	// other of length 'len' (i.e. first send the register to be read, then
	// get all the data
	do
	{
		err = i2c_transfer(device->client->adapter, msgs, 4);
		if (err != 4)
			msleep_interruptible(MAX44007_RETRY_DELAY);
	} while ((err != 4) && (++tries < MAX44007_MAX_RETRIES));

	if (err != 4)
	{
		printk("%s: read transfer error err=%d\n", __func__, err);
		err = -EIO;
	}
	else
	{
		err = 0;
	}

	dprintk(KERN_INFO "max44007_read_reg2 0x%02x 0x%02x 0x%02x\n",
		addr,
		buffer[0],
		buffer[1]);

	// return error code (0 if no error)
	return err;
} // MAX44007ReadReg2


/**
 *	Either sets or clears the state of the interrupt enable bit.
 *
 *	@param struct MAX44007Data *device
 *	  Points to some data describing an instance of a MAX44007 device.
 *	  The state of the interrupt is updated in here
 *
 *	@param bool enable
 *	  Desired state of the interrupt (true = enabled)
 *
 *	@return
 *	  0 on success, an error code otherwise
 */
int max44007_set_interrupt(struct MAX44007Data *device, bool enable)
{
	u8 configStatus[] = {MAX44007_CONFIG_ADDR, 0};
	bool currently_enabled; // is the device already enabled?

	currently_enabled = (device->interrupt_en) ? true : false;

	dprintk(KERN_INFO "max44007_set_interrupt 0x%02x 0x%02x\n",
		currently_enabled,
		device->interrupt_en);

	// if the current status of the device and the desired status are the same,
	// do nothing
	if (currently_enabled != enable)
	{
		if (enable)
			configStatus[1] |= 1; // set lowest bit
		if (max44007_write_reg(device, configStatus, 2))
		{
			printk("%s: couldn't update interrupt status\n", __func__);
			return -EINVAL;
		}
		// if write was successful, update the value in the data structure
		device->interrupt_en = enable ? 1 : 0;
	}

	return 0;
}


/**
 *   Reads the interrupt status register to clear it and return the value of
 *	the interrupt status bit
 *
 *	@param struct MAX44007Data *device
 *	  Points to some data describing an instance of a MAX44007 device.
 *	  The state of the interrupt is updated in here
 *
 *	@return
 *	  Interrupt status
 */
int max44007_read_int_status(struct MAX44007Data *device)
{
	u8 buf = MAX44007_INT_STATUS_ADDR;

	dprintk(KERN_INFO "max44007_read_int_status\n");

	if (max44007_read_reg(device, &buf, 1) != 0)
	{
		printk("%s: couldn't read interrupt status\n", __func__);
		return -EIO;
	}

	return ((buf & 1) == 0) ? 1 : 0;
} // max44007ReadIntStatus


/**
 *	Updates the integration time settings in register 0x02
 *
 *	@param struct MAX44007Data *device
 *	  Points to some data describing an instance of a MAX44007 device.
 *	  The state of the configuration register is updated in here
 *
 *	@param u8 time
 *	  New integration time settings (see datasheet for details)
 *
 *	@return
 *	  Error code, 0 on success
 */
int max44007_set_integration_time(struct MAX44007Data *device, u8 time)
{
	u8 config_reg[] = {MAX44007_CONFIG_ADDR, device->config};

	dprintk(KERN_INFO "max44007_set_integration_time\n");

	if (!(device->config & 0x40))
		return 0;

	config_reg[1] &= 0xF8;			// clear lowest three bits
	config_reg[1] |= (time & 0x7);	// set lowest three bits to
									// contents of time

	if (max44007_write_reg(device, config_reg, 2))
	{
		printk("%s: couldn't update interrupt status\n", __func__);
		return -EINVAL;
	}

	// update the stored settings on success and move along...
	device->config = config_reg[1];
	return 0;
}


/**
 *	Either enables or disables manual mode on the MAX44007
 *
 *	@param struct MAX44007Data *device
 *	  Points to some data describing an instance of a MAX44007 device.
 *	  The state of the configuration register is updated in here
 *
 *	@param bool enable
 *	  whether to enable or disable manual mode
 *
 *	@return
 *	  Error code, 0 on success
 */
int max44007_set_manual_mode(struct MAX44007Data *device, bool enable)
{
	u8 configStatus[] = {MAX44007_CONFIG_ADDR, 0};
	bool currently_manual; // is the device already in manual mode?

	dprintk(KERN_INFO "max44007_set_manual_mode\n");

	currently_manual = (device->config & 0x40) ? true : false;

	// if the current status of the device and the desired status are the same
	// do nothing
	if (currently_manual != enable)
	{
		if (enable)
			configStatus[1] = (device->config) | 0x40;
		else
			configStatus[1] = (device->config) & 0xBF;
		if (max44007_write_reg(device, configStatus, 2))
		{
			printk("%s: couldn't update auto/manual mode status\n", __func__);
			return -EINVAL;
		}
		// if write was successful, update the value in the data structure
		device->config = configStatus[1];
	}

	return 0;
}


/**
 *	Sets or clear the continuous mode bit
 *
 *	@param struct MAX44007Data *device
 *	  Points to some data describing an instance of a MAX44007 device.
 *	  The state of the configuration register is updated in here
 *
 *	@param bool enable
 *	  whether to enable or disable the current divisor
 *
 *	@return
 *	  Error code, 0 on success
 */
int max44007_set_continuous_mode(struct MAX44007Data *device, bool enable)
{
	u8 configStatus[] = {MAX44007_CONFIG_ADDR, 0};
	bool currently_cont; // is the device already in continuous mode?

	dprintk(KERN_INFO "max44007_set_continuous_mode\n");

	currently_cont = (device->config & 0x80) ? true : false;

	// if the current status of the device and the desired status are the same
	// do nothing
	if (currently_cont != enable)
	{
		if (enable)
			configStatus[1] = (device->config) | 0x80;
		else
			configStatus[1] = (device->config) & 0x7F;
		if (max44007_write_reg(device, configStatus, 2))
		{
			printk("%s: couldn't update continuous mode setting\n", __func__);
			return -EINVAL;
		}
		// if write was successful, update the value in the data structure
		device->config = configStatus[1];
	}

	return 0;
}


/**
 *	Sets or clears the current division ratio bit, if necessary
 *
 *	@param struct MAX44007Data *device
 *	  Points to some data describing an instance of a MAX44007 device.
 *	  The state of the configuration register is updated in here
 *
 *	@param bool enable
 *	  whether to enable or disable the current divisor
 *
 *	@return
 *	  Error code, 0 on success
 */
int max44007_set_current_div_ratio(struct MAX44007Data *device, bool enable)
{
	u8 configStatus[] = {MAX44007_CONFIG_ADDR, 0};
	bool status; // is the device already in CDR=1 mode?

	dprintk(KERN_INFO "max44007_set_current_div_ratio\n");

	// don't bother if not in manual mode
	if ( !(device->config & 0x40) )
		return 0;
	status = (device->config & 0x08) ? true : false;

	// if the current status of the device and the desired status are the same
	// do nothing
	if (status != enable)
	{
		if (enable)
			configStatus[1] = (device->config) | 0x08;
		else
			configStatus[1] = (device->config) & 0xF7;
		if (max44007_write_reg(device, configStatus, 2))
		{
			printk("%s: couldn't update current division ratio status\n",__func__);
			return -EINVAL;
		}
		// if write was successful, update the value in the data structure
		device->config = configStatus[1];
	}

	return 0;
}


/**
 *	Gets the lux reading from the light sensor
 *
 *  Does a consecutive two-byte read on the ADC register, then checks for
 *  the setting being in overflow. If it is, the counts are automatically set
 *  to their maximum possible value.
 *	Otherwise, the counts are converted to lux by the formula:
 *		Lux = 2^(exponent) * mantissa * 0.025
 *
 *	** Author's note **
 *	  This function can easily be modified to return the counts instead of the
 *    lux by removing the multiplication by 0.025 and changing adjusted_lux to
 *    int from the float type
 *
 *  @param struct MAX44007Data *device
 *	  describes a particular ALS object.
 *  @param float *adjusted_lux
 *    points to where the result will be stored
 *
 *  @return
 *    0 on success
 *    -EIO if a communication error, -EINVAL if overflow
 */
static int max44007_read_ALS(struct MAX44007Data *device, int *adjusted_lux)
{
	u8 data_buffer[] = {MAX44007_LUX_HIGH_ADDR, MAX44007_LUX_LOW_ADDR};
	u8 exponent;
	u8 mantissa;

	dprintk(KERN_INFO "max44007_read_ALS\n");

	// do a consecutive read of the ADC register (0x03 and 0x04)
	// throw an error if needed
//	if (max44007_read_reg(device, data_buffer, 2) != 0)
	if (max44007_read_reg2(device, data_buffer) != 0)
	{
		printk("%s: couldn't read als data\n", __func__);
		return -EIO;
	} // if there was a read error

	/**
	 *  Author's note:
	 *    data_buffer should now contain the exponent and mantissa of the result
	 *	per the datasheet's description of the lux readings
	 */
	// update the most recent reading inside the object pointer
	device->lux_high = data_buffer[0];
	device->lux_low = data_buffer[1];

	// calculate the lux value
	exponent = (data_buffer[0] >> 4) & 0x0F;
	if (exponent == 0x0F)
	{
		printk("%s: overload on light sensor!\n", __func__);
		*adjusted_lux = 104448; // maximum reading (per datasheet)
		return -EINVAL;
	}
	mantissa = (data_buffer[0] & 0x0F) << 4 | (data_buffer[1] & 0x0F);
#ifdef CONFIG_MACH_OMAP4_BOWSER_SUBTYPE_TATE
	*adjusted_lux = ((int)(1 << exponent)) * mantissa *100 / transmittance ;
#else
	*adjusted_lux = ((int)(1 << exponent)) * mantissa /transmittance ;
#endif

	// Lux can be as large as 0x19640
	dprintk(KERN_INFO "max44007_read_ALS lux=%d\n", *adjusted_lux);

	// add any other adjustments you want to make here (e.g. correct for a lens,
	// glass, other filtering, etc)

	return 0;
} // read data from ALS


/**
 *  Updates threshold timer register inside physical device to a new value
 *  and also updates the value in the object descriptor passed to it if
 *  successful
 *
 *  @param struct MAX44007Data *device
 *    describes a particular ALS object
 *  @param u8 new_tim_val
 *    desired setting of the integration time (must be in [0 3])
 *
 *  @return
 *    -EIO if there is a write error, 0 otherwise
 */
int max44007_set_thresh_tim(struct MAX44007Data *device, u8 new_tim_val)
{
	u8 buf[] = {MAX44007_THRESH_TIM_ADDR, new_tim_val};

	dprintk(KERN_INFO "max44007_set_thresh_tim\n");

	if (max44007_write_reg(device, buf, 2))
		return -EIO;

	// update stored value
	device->thresh_tim = new_tim_val;

	return 0;
} // max44007_set_thresh_tim

static int max44007_get_trims(struct MAX44007Data *device)
{
	u8 buf[2] = {MAX44007_CLK_COARSE_ADDR,MAX44007_CLK_FINE_ADDR};

	// read values and terminate on error

	if (max44007_read_reg2(device,buf) != 0)

	{

		printk("%s: couldn't read clock trims\n", __func__);

		return -EIO;

	}

	// you MUST complement values read from the trim registers
	device->clk_coarse = ~buf[0];
	device->clk_fine = ~buf[1];

	buf[0] = MAX44007_GREEN_TRIM_ADDR;
	buf[1] = MAX44007_IR_TRIM_ADDR;

	// read values and terminate on error
	if (max44007_read_reg2(device,buf) != 0)

	{

		printk("%s: couldn't read gain trims\n", __func__);

		return -EIO;

	}

	// you MUST complement values read from the trim registers
	device->green_trim = ~buf[0] & 0xff;
	device->ir_trim = ~buf[1] & 0xff;
	// successful!
	return 0;
}

static int max44007_set_cal_trims(struct MAX44007Data *device, struct MAX44007CalData constant, bool reset)
{
	u8 buf[2];


	// set OTPSEL
	buf[0] = MAX44007_OTP_REG;
	buf[1] = 0x81;
	if (max44007_write_reg(device, buf, 2))
	{
		printk("%s: couldn't set OTPSEL\n",__func__);
		return -EINVAL;
	}

	// first you must set the clock values
	buf[0] = MAX44007_CLK_COARSE_ADDR;
	buf[1] = device->clk_coarse;
	if (max44007_write_reg(device, buf, 2))
	{
		printk("%s: failed to set coarse clk trim\n",__func__);
		return -EINVAL;
	}

	buf[0] = MAX44007_CLK_FINE_ADDR;
	buf[1] = device->clk_fine;
	if (max44007_write_reg(device, buf, 2))
	{
		printk("%s: failed to set fine clk trim\n",__func__);
		return -EINVAL;
	}



	// now scale the gain values


	buf[0] = MAX44007_GREEN_TRIM_ADDR;
	if (!reset)
		buf[1] = (u8)(device->green_trim * constant.greenConstant/100);
	else
		buf[1] = (u8)(device->green_trim_backup);
	if (max44007_write_reg(device, buf, 2))
	{
		printk("%s: failed to set green channel gain to %x\n",__func__,buf[1]);
		return -EINVAL;
	}

	buf[0] = MAX44007_IR_TRIM_ADDR;
	if(!reset)
		buf[1] = (u8)(device->ir_trim * constant.irConstant/100);
	else
		buf[1] = (u8)(device->ir_trim_backup);
	if (max44007_write_reg(device, buf, 2))
	{
		printk("%s: failed to set IR channel gain to %x\n",__func__,buf[1]);
		return -EINVAL;
	}

	return 0;
}

/**
 *  Converts a lux value into a threshold mantissa and exponent.
 *
 *  @param int lux
 *	  the light value to be converted
 *
 *  Returns:
 *    threshold mantissa and exponent.
 *
 *  Note: An identical upper and lower limit will be coded as an
 *        identical threshold mantissa and exponent. But there
 *        is +15 hysteresis between them, per the MAX44007
 *        documentation:
 *
 *        Upper lux threshold = 2(exponent) x mantissa x 0.025
 *        Exponent = 8xUE3 + 4xUE2 + 2xUE1 + UE0
 *        Mantissa = 128xUM7 + 64xUM6 + 32xUM5 + 16xUM4 + 15
 *
 *        Lower lux threshold = 2(exponent) x mantissa x 0.025
 *        Exponent = 8xLE3 + 4xLE2 + 2xLE1 + LE0
 *        Mantissa = 128xLM7 + 64xLM6 + 32xLM5 + 16xLM4
 */
int max44007_lux_to_thresh(int lux)
{
	long result = 0;
	int mantissa = 0;
	int exponent = 0;

	if (lux == 0)
	{
		// 0 Has no Mantissa
		dprintk(KERN_INFO
				"max44007_lux_to_thresh lux=0x%04x result=0x%02x\n",
				 lux, 0);
		return 0;
	}

	// Lux can be as large as 0x19640
	// Add some for rounding
	// Range of the Mantissa is 8 to 15 so rounding
	// is half the average LSB.
	result = (lux + lux / 24);
	result = result * 40;

	// Shift right until Mantissa is properly placed
	while (result >= 0x100)
	{
		result = result >> 1;
		exponent++;
	}

	mantissa = (result >> 4) & 0xf;
	result = mantissa + (exponent << 4);

	dprintk(KERN_INFO
			"max44007_lux_to_thresh lux=0x%04x result=0x%02x\n",
			lux, (int)result);

	return (int)result;
}


/**
 *  Gets the ambient light level from the MAX44007 device by calling
 *  max44007_read_ALS(), and sets the reading as an input event on the
 *  associated input device if the reading is valid (i.e. not an error)
 *  Optionally, this will also read the interrupt flag register to clear
 *  the interrupt flag.
 *
 *  @param struct MAX44007Data *device
 *	  describes a particular ALS object
 *  @param bool clear_int_flag
 *	  should we clear the interrupt flag?
 *    set to true if using the interrupt functionality
 *
 *  Returns:
 *    0 upon success, otherwise an error flag
 */
int max44007_report_light_level(struct MAX44007Data *device,
								bool clear_int_flag)
{
	int result = 0;
	int err;
	int lux; // calculated lux value
	struct MAX44007ThreshZone zone;

	dprintk(KERN_INFO "max44007_report_light_level\n");

	err = max44007_read_ALS(device, &lux);
	// lux goes into the input device as an input event, where the system will
	// do something with it
	if (err == 0) // no error occurred
	{

		// Reset lux interrupt thresholds
		zone.upperThresh =
			max44007_lux_to_thresh(lux +
				((lux * CONFIG_INPUT_MAX44007_HYSTERESIS) >> 8));

		if ((lux - ((lux * CONFIG_INPUT_MAX44007_HYSTERESIS) >> 8)) < 0)
		{
			zone.lowerThresh = max44007_lux_to_thresh(0);
		}
		else
		{
			zone.lowerThresh =
				max44007_lux_to_thresh(lux -
					((lux * CONFIG_INPUT_MAX44007_HYSTERESIS) >> 8));
		}

		err = max44007_set_threshold_zone(device, &zone);

		dprintk(KERN_INFO
				"max44007_report_light_level to upper layer lux=%d\n",
				lux);

		dprintk(KERN_INFO
				"max44007_report_light_level delta=%d raw thresh=%d %d\n",
				(lux * CONFIG_INPUT_MAX44007_HYSTERESIS) >> 8,
				lux + ((lux * CONFIG_INPUT_MAX44007_HYSTERESIS) >> 8),
				lux - ((lux * CONFIG_INPUT_MAX44007_HYSTERESIS) >> 8));

		// Report event to upper layer
		//	input_event(device->idev, EV_MSC, MSC_RAW, lux);
		ALS_val=lux;

		/*Input system will drop the latest event when driver send two same value event . It will cause autobrightness no response in dark room */
		if(lux==0)
		{
			lux=1;
		input_report_abs(device->idev, ABS_MISC, lux);
		input_sync(device->idev);
			lux=0;
			input_report_abs(device->idev, ABS_MISC, lux);
			input_sync(device->idev);
		}else{
			input_report_abs(device->idev, ABS_MISC, lux);
			input_sync(device->idev);
		}
	}
	else
	{
		result = -EIO;
		printk("%s: problem getting lux reading from MAX44007\n", __func__);
	}

	// clear the input status register, if desired
	// this is skipped if there was a problem communicating with the device
	if (clear_int_flag && result==0)
	{
		result = max44007_read_int_status(device);
		if (result != 0)
		{
			// Interrupt register expected to be set at this point
			// indicating an interrupt.
			// This error indicates that the register is 0.
			printk("%s: couldn't read MAX44007 interrupt register, %d\n",
				   __func__, result);
			return result;
		}
		dprintk("max44007_report_light_level enabling IRQ\n");
		max44007_enable_IRQ(device, true);
	}

	return result;
} // max44007_report_light_level


/**
 *  Helps handle the work queue for Linux platform
 *
 *  @param struct work_struct *work
 *    pointer to a working queue
 */
static void max44007_work_queue(struct work_struct *work)
{
	struct MAX44007Data *data =
		container_of((struct delayed_work *)work,
		struct MAX44007Data, work_queue);

	dprintk(KERN_INFO "max44007_work_queue\n");

	max44007_report_light_level(data, true);
} // max44007_work_queue


/**
 *  Either enables or disables IRQ handling for the MAX44007 device passed to
 *  it.
 *
 *  @param struct MAX44007Data *device
 *	  describes a particular ALS object
 *  @param bool enable
 *    true if we want to enable IRQ, false otherwise
 */
void max44007_enable_IRQ(struct MAX44007Data *device, bool enable)
{
	unsigned long flags;

	dprintk(KERN_INFO "max44007_enable_IRQ %d\n", enable);

	spin_lock_irqsave(&device->irqLock, flags);

	if (device->irqState != enable)
	{
		if (enable)
			enable_irq(device->client->irq);
		else
			disable_irq_nosync(device->client->irq);
		device->irqState = enable;
	}
	spin_unlock_irqrestore(&device->irqLock, flags);

} // max44007_enable_IRQ


/**
 *  IRQ handler for this device
 *
 *  @param int irq
 *    irq number
 *  @param void *device
 *    points to the device for which the IRQ is handled
 *
 *  @return
 *    A code indicating that the function handled the IRQ
 */
static irqreturn_t max44007_irq_handler(int irq, void *device)
{
	struct MAX44007Data *als_device = device;

	dprintk(KERN_INFO "max44007_irq_handler\n");

	// do something
	max44007_enable_IRQ(als_device, false);
	schedule_delayed_work(&als_device->work_queue, 0);

	return IRQ_HANDLED;
} // max44007IRQHandler


/**
 *  Changes the threshold zone in the device by changing the values in
 *  registers 0x5 and 0x6 (see datasheet for more info).
 *  It is recommended, but not mandatory, to disable the interrupt while
 *  this is being done to avoid unexpected behavior.
 *
 *  @param struct MAX44007Data *device
 *	  describes a particular ALS object
 *  @param struct MAX44007ThreshZone *new_zone
 *    a new threshold zone to adopt
 *  @return
 *    Error code if something goes wrong, otherwise 0
 */
int max44007_set_threshold_zone(struct MAX44007Data *device,
								struct MAX44007ThreshZone *new_zone)
{
	u8 buf[2];

	dprintk(KERN_INFO "max44007_set_threshold_zone %d %d\n",
			((new_zone->upperThresh & 0xf0) + 0x0f) <<
			(new_zone->upperThresh & 0xf),
			(new_zone->lowerThresh & 0xf0) <<
			(new_zone->lowerThresh & 0xf));

	buf[0] = MAX44007_THRESH_HIGH_ADDR;
	buf[1] = new_zone->upperThresh;
	if (max44007_write_reg(device, buf, 2))
		goto thresholdSetError;

	buf[0] = MAX44007_THRESH_LOW_ADDR;
	buf[1] = new_zone->lowerThresh;
	if (max44007_write_reg(device, buf, 2))
		goto thresholdSetError;

	// update the register data
	device->thresh_high = new_zone->upperThresh;
	device->thresh_low = new_zone->lowerThresh;

	return 0;

thresholdSetError:
	printk("%s: couldn't update thresholds\n", __func__);
	return -EINVAL;
} // max44007_set_threshold_zone


/**
 *  Remove function for this I2C driver.
 *
 *  ***Author's Note***
 *    Please adapt this function to your particular platform.
 *
 *  @param struct i2c_client *client
 *		the associated I2C client for the device
 *
 *	@return
 *    0
 */
static int max44007_remove(struct i2c_client *client)
{
	struct MAX44007Data *deviceData = i2c_get_clientdata(client);

	dprintk(KERN_INFO "max44007_remove\n");

	max44007_set_cal_trims(deviceData, cal_constant, true);
	sysfs_remove_group(&client->dev.kobj, &max44007_attr_group);
	if (!IS_ERR_OR_NULL(deviceData->regulator))
		regulator_put(deviceData->regulator);
	free_irq(deviceData->client->irq, deviceData);
	cancel_delayed_work_sync(&deviceData->work_queue);
	input_unregister_device(deviceData->idev);
	input_free_device(deviceData->idev);
	kfree(deviceData);
	return 0;
} // max44007Remove


/**
 *	Sets all the settable user registers to known values, either what's
 *  supplied in the device pointer or to the power-on defaults as specified
 *  in the datasheet
 *
 *	@param struct MAX44007Data *device
 *	  points to a represenation of a MAX44007 device in memory
 *	@param bool use_defaults
 *	  set this to false if you want to set the registers to what's stored
 *    inside struct MAX44007Data *device
 *	@return
 *    0 if no error, error code otherwise
 *
 */
static int max44007_initialize_device(struct MAX44007Data *device,
									  bool use_defaults)
{
	u8 buf[2]; // write buffer

	dprintk(KERN_INFO "max44007_initialize_device\n");

	// initialize receiver configuration register

	buf[0] = MAX44007_CONFIG_ADDR; // 0x02
	// 0xbx lets you set the gain to any value
	// else currently automated gain control
	// It is better to set this before enabling the interrupt
	buf[1] = use_defaults ? 0x03 : (device->config);
	if (max44007_write_reg(device, buf, 2))
		goto max44007_failed_init;

	// initialize main configuration register
	buf[0] = MAX44007_INT_ENABLE_ADDR; // 0x01
//	buf[1] = use_defaults ? 0x00 : (device->interrupt_en);
	buf[1] = use_defaults ? 0x01 : (device->interrupt_en); // Enable Int

	if (max44007_write_reg(device, buf, 2))
		goto max44007_failed_init;

	// initialize upper threshold
	buf[0] = MAX44007_THRESH_HIGH_ADDR;	// 0x05
//	buf[1] = use_defaults ? 0xFF : (device->thresh_high);
//	buf[1] = use_defaults ? 0xFD : (device->thresh_high); // Flash/Sun Light
//	buf[1] = use_defaults ? 0x88 : (device->thresh_high); // Room Light
	buf[1] = use_defaults ? 0x00 : (device->thresh_high); // Hysteresis
	if (max44007_write_reg(device, buf, 2))
		goto max44007_failed_init;

	// initialize lower threshold
	buf[0] = MAX44007_THRESH_LOW_ADDR; // 0x06
//	buf[1] = use_defaults ? 0x00 : (device->thresh_low);
//	buf[1] = use_defaults ? 0x58 : (device->thresh_low); // Shadow
//	buf[1] = use_defaults ? 0x16 : (device->thresh_low); // Cover
	buf[1] = use_defaults ? 0xFF : (device->thresh_low); // Hysteresis
	if (max44007_write_reg(device, buf, 2))
		goto max44007_failed_init;

	// initialize threshold timer
	buf[0] = MAX44007_THRESH_TIM_ADDR; // 0x07
//	buf[1] = use_defaults ? 0xFF : (device->thresh_tim);
//	buf[1] = use_defaults ? 0x05 : (device->thresh_tim); // 0.5 sec
	buf[1] = use_defaults ? 0x01 : (device->thresh_tim); // 0.1 sec
//	buf[1] = use_defaults ? 0x00 : (device->thresh_tim); // 0.0 sec
	if (max44007_write_reg(device, buf, 2))
		goto max44007_failed_init;
	if(use_defaults == true ){
		als_enable = 1;
	}
	return 0;
max44007_failed_init:
	printk("%s: couldn't initialize register %x\n", __func__, buf[0]);
	return -EINVAL;
} // initialization


/**
 *	Forces the device to its lowest power settings.
 *
 *  In this mode, the device still makes lux measurements,
 *  but does not send any interrupts.
 *
 *	@param struct MAX44007Data *device
 *	  points to a represenation of a MAX44007 device in memory
 *	@return
 *    0 if no error, error code otherwise
 *
 */
static int max44007_sleep(struct MAX44007Data *device)
{
	u8 buf[2]; // write buffer

	dprintk(KERN_INFO "max44007_sleep\n");

	// initialize receiver configuration register
	buf[0] = MAX44007_CONFIG_ADDR; // 0x02
	buf[1] = 0x43; // Cont cleared, tim = 100ms conversion.
	if (max44007_write_reg(device, buf, 2))
		goto max44007_failed_sleep;

	// initialize upper threshold
	buf[0] = MAX44007_THRESH_HIGH_ADDR;	// 0x05
	buf[1] = 0xFF;
	if (max44007_write_reg(device, buf, 2))
		goto max44007_failed_sleep;

	// initialize lower threshold
	buf[0] = MAX44007_THRESH_LOW_ADDR; // 0x06
	buf[1] = 0x00;
	if (max44007_write_reg(device, buf, 2))
		goto max44007_failed_sleep;

	// initialize threshold timer
	buf[0] = MAX44007_THRESH_TIM_ADDR; // 0x07
	buf[1] = 0xFF;
	if (max44007_write_reg(device, buf, 2))
		goto max44007_failed_sleep;
	als_enable = 0;
	return 0;

max44007_failed_sleep:
	printk("%s: couldn't initialize register %x\n", __func__, buf[0]);
	return -EINVAL;

} // initialization


/**
 *	Wakes the device from its lowest power settings to run.
 *
 *	@param struct MAX44007Data *device
 *	  points to a represenation of a MAX44007 device in memory
 *	@return
 *    0 if no error, error code otherwise
 *
 */
static int max44007_wake(struct MAX44007Data *device)
{
	int err = 0;

	dprintk(KERN_INFO "max44007_wake\n");

	err = max44007_initialize_device(device, true);
	if (err < 0)
	{
		printk("%s: Register initialization failed: %d\n", __func__, err);
		err = -ENODEV;
		goto max44007_failed_wake;
	}

	return 0;

max44007_failed_wake:
	printk("%s: couldn't wake\n", __func__);
	return err;

} // initialization
//***********************************************************************FTM
static int max44007_open(struct inode *inode, struct file *file)
{
        dprintk(KERN_INFO "\n");
        return 0;
}

static ssize_t max44007_read(struct file *file, char __user *buffer, size_t size, loff_t *f_ops)
{
        dprintk(KERN_INFO "\n");
        return 0;

}



static ssize_t max44007_write(struct file *file, const char __user *buffer, size_t size, loff_t *f_ops)
{
        dprintk(KERN_INFO "\n");
        return 0;
}

static long max44007_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{

        int ret = -1;
        printk( "Light sensor in IOCTL\n");
        switch (cmd)
        {
								case ACTIVE_LS:
                        ret = max44007_initialize_device(Public_deviceData, false);
                        if (ret < 0)
                        {
                            printk("%s: Register initialization failed: %d\n", __func__, ret);
                            return -ENODEV;
                        }
                        ret = max44007_initialize_device(Public_deviceData, true);
                        if (ret < 0)
                        {
                            printk("%s: Register initialization failed: %d\n", __func__, ret);
                            return -ENODEV;
                        }
                        break;
                case READ_LS:

                        ret = copy_to_user((unsigned long __user *)arg, &ALS_val, sizeof(ALS_val));
                        printk( "Light sensor : READ_LS value=%d\n", (int)(ALS_val));
                        break;

                default:
                        printk(KERN_INFO "Light sensor : ioctl default : NO ACTION!!\n");
        }

        return 0;
}
static int max44007_release(struct inode *inode, struct file *filp)
{
        dprintk(KERN_INFO "\n");
        return 0;
}


static const struct file_operations max4407_dev_fops = {
        .owner = THIS_MODULE,
        .open = max44007_open,
        .read = max44007_read,
        .write = max44007_write,
//        .ioctl = max44007_ioctl,
	.unlocked_ioctl = max44007_ioctl,
        .release = max44007_release,
};

static struct miscdevice max44007_dev =
{
        .minor = MISC_DYNAMIC_MINOR,
        .name = "max4407",
        .fops = &max4407_dev_fops,
};

//************************************************************************
/**
 *  Shutdown function for this I2C driver.
 *
 *  @param struct i2c_client *client
 *            the associated I2C client for the device
 *
 *     @return
 *    void
 */
static void max44007_shutdown(struct i2c_client *client)
{
       printk(KERN_INFO "max44007_shutdown\n");

       max44007_remove(client);

       return;
} // max44007_shutdown


/**
 *  Suspend function for this I2C driver.
 *
 *  @param struct i2c_client *client
 *            the associated I2C client for the device
 *
 *     @return
 *    0
 */

static int max44007_suspend(struct i2c_client *client, pm_message_t mesg)
{
       struct MAX44007Data *deviceData = i2c_get_clientdata(client);

       printk(KERN_INFO "max44007_suspend\n");

       max44007_sleep(deviceData);

       return 0;
} // max44007_suspend


/**
 *  Resume function for this I2C driver.
 *
 *  @param struct i2c_client *client
 *            the associated I2C client for the device
 *
 *	@return
 *    0
 */
	static int max44007_resume(struct i2c_client *client)
	{
	       struct MAX44007Data *deviceData = i2c_get_clientdata(client);

	       printk(KERN_INFO "max44007_resume\n");

	       max44007_wake(deviceData);

	       return 0;
	} // max44007_resume

//************************************************************************

/**
 * 	Probes the adapter for a valid MAX44007 device, then initializes it.
 *
 *  ***Author's Note***
 *    You may need to modify this function to suit your system's specific needs
 *    I have a platform_data fetch in here, but you may not need it
 *
 *  @param struct i2c_client *client
 *	  points to an i2c client object
 *  @param const struct i2c_device_id *id
 *    device id
 *
 *  @return
 *    0 on success, otherwise an error code
 */

extern int idme_get_board_revision(void);

static int max44007_probe(struct i2c_client *client,
						  const struct i2c_device_id *id )
{
	struct MAX44007PlatformData *pdata = client->dev.platform_data;
	struct MAX44007Data *deviceData;
	int err = 0;


	printk(KERN_INFO "max44007_probe\n");

	// attempt to get platform data from the device
	if (pdata == NULL)
	{
		printk("%s: couldn't get platform data\n", __func__);
		return -ENODEV;
	}

	// check to make sure that the adapter supports I2C
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
	{
		printk("%s: I2C_FUNC_I2C not supported\n", __func__);
		return -ENODEV;
	}

	// allocate some memory for the device
	deviceData = kzalloc(sizeof(struct MAX44007Data), GFP_KERNEL);
	if (deviceData == NULL)
	{
		printk("%s: couldn't allocate memory\n", __func__);
		return -ENOMEM;
	}
  Public_deviceData=deviceData;
	// start initializing...
	deviceData->client = client;
	deviceData->pdata = pdata;
	deviceData->idev = input_allocate_device();

	// couldn't allocate input device?
	if (!deviceData->idev)
	{
		printk("%s: couldn't allocate input device\n", __func__);
		kfree(deviceData);
		return -ENOMEM;
	}

	// set up the input device
	deviceData->idev->name = "MAX44007";
	//input_set_capability(deviceData->idev, EV_MSC, MSC_RAW);	//leon modify to fit the sensor HAL
	input_set_capability(deviceData->idev, EV_ABS, ABS_MISC);
	input_set_abs_params(deviceData->idev, ABS_MISC, 0, 10000, 0, 0);

	atomic_set(&deviceData->enabled, 0);
	// initialize work queue
	INIT_DELAYED_WORK(&deviceData->work_queue, max44007_work_queue);

	// attempt to register the input device
	if ( (err = input_register_device(deviceData->idev)) )
	{
		printk("%s: input device register failed:%d\n", __func__, err);
		goto error_input_register_failed;
	}

	// attempt to initialize the device
	// set up the als_id
	deviceData->als_id = MAX44009;
	err = max44007_initialize_device(deviceData, true);
	if (err) {
		/* try with MAX44007 address (0x5a) */
		deviceData->client->addr = 0x5A;
		deviceData->als_id = MAX44007;

		err = max44007_initialize_device(deviceData, true);
		if (err) {
			pr_err("%s: Register initialization failed: %d\n",
					__func__, err);

			err = -ENODEV;
			goto errRegInitFailed;
		}

		/* MAX44007 tuning */
#ifdef CONFIG_MACH_OMAP4_BOWSER_SUBTYPE_TATE
		transmittance = 160;
#else
		transmittance = 4;
#endif
		cal_constant.greenConstant = 48;
		cal_constant.irConstant = 307;
	} else {
		/* MAX44009 tuning */
#ifdef CONFIG_MACH_OMAP4_BOWSER_SUBTYPE_TATE
		transmittance = 242;
		cal_constant.greenConstant = 156;
		cal_constant.irConstant = 133;
#else
		transmittance = 6;
		if(idme_get_board_revision() > IDME_BOARD_VERSION_EVT3){
			cal_constant.greenConstant = 174;
			cal_constant.irConstant = 146;
		}else{
			cal_constant.greenConstant = 179;
			cal_constant.irConstant = 176;
		}
#endif
	}
	//calibration add here
	max44007_get_trims(deviceData);
	deviceData->green_trim_backup = deviceData->green_trim;
	deviceData->ir_trim_backup = deviceData->ir_trim;
	max44007_set_cal_trims(deviceData, cal_constant, false);


	// set up an IRQ channel
	max44007_read_int_status(deviceData);	//leon add to clear the interrupt status
	spin_lock_init(&deviceData->irqLock);
	deviceData->irqState = 1;

	err = request_irq(deviceData->client->irq, max44007_irq_handler,
					  IRQF_TRIGGER_FALLING, MAX44007_NAME, deviceData);
	if (err != 0)
	{
		dprintk(KERN_INFO "**********************************\n");
		dprintk(KERN_INFO "max44007_probe irq request failed.\n");
		dprintk(KERN_INFO "**********************************\n");
		printk("%s: irq request failed: %d\n", __func__, err);
		err = -ENODEV;
		goto errReqIRQFailed;
	}

	dprintk(KERN_INFO "max44007_probe irq request succeeded.\n");

	i2c_set_clientdata(client, deviceData);

	deviceData->regulator = regulator_get(&client->dev, "vio");

	/* Anvoi, 2011/09/07 for ftm porting { */
	#ifdef MAX44007_FTM_PORTING
	if (0 != misc_register(&max44007_dev))
 	{
		dprintk(KERN_INFO "Light Sensor : max4407_dev register failed.\n");
		return 0;
	}else
	{
		dprintk(KERN_INFO "Light Sensor : max4407_dev register ok.\n");
	}
	#endif
#ifdef CONFIG_MACH_OMAP4_BOWSER_SUBTYPE_TATE
	max44007_sleep(deviceData);
#endif
	/* Anvoi, 2011/09/07 for ftm porting } */
	dprintk(KERN_INFO "max44007_probe completed.\n");

	//sysfs
	err = sysfs_create_group(&client->dev.kobj, &max44007_attr_group);
	if (err)
		goto sysdevFailed;

	return 0;

	// error handling
sysdevFailed:
	printk("sysdevFailed.\n");
	sysfs_remove_group(&client->dev.kobj, &max44007_attr_group);
errReqIRQFailed:
errRegInitFailed:
	input_unregister_device(deviceData->idev);
error_input_register_failed:
	input_free_device(deviceData->idev);
	kfree(deviceData);
	return err;
} // max44007Probe


// descriptor of the MAX44007 device ID
static const struct i2c_device_id max44007_id[] =
{
	{MAX44007_NAME, 0},
	{}
};

// descriptor of the MAX44007 I2C driver
static struct i2c_driver max44007_i2c_driver =
{
	.remove		= max44007_remove,
	.id_table	= max44007_id,
	.suspend        = max44007_suspend,
	.resume         = max44007_resume,
	.shutdown       = max44007_shutdown,
	.probe		= max44007_probe,
	.driver		=
	{
		.name	= MAX44007_NAME,
		.owner	= THIS_MODULE,
	},
};


// initialization and exit functions
int __init max44007_init(void)
{
	dprintk(KERN_INFO "*************\n");
	dprintk(KERN_INFO "*************\n");
	dprintk(KERN_INFO "*************\n");
	dprintk(KERN_INFO "max44007_init\n");
	dprintk(KERN_INFO "*************\n");
	dprintk(KERN_INFO "*************\n");
	dprintk(KERN_INFO "*************\n");
	return i2c_add_driver(&max44007_i2c_driver);
}


void __exit max44007_exit(void)
{
	dprintk(KERN_INFO "max44007_exit\n");
	i2c_del_driver(&max44007_i2c_driver);
}


module_init(max44007_init);
module_exit(max44007_exit);

MODULE_DESCRIPTION("ALS driver for Maxim MAX44007");
MODULE_AUTHOR("Ilya Veygman <ilya.veygman@maxim-ic.com>");
MODULE_LICENSE("GPL");

