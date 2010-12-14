/*
 * drivers/media/video/lv8093.c
 *
 * LV8093 Piezo Motor (LENS) driver
 *
 * Copyright (C) 2008-2009 Texas Instruments.
 * Copyright (C) 2009 Hewlett-Packard.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <linux/mutex.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/device.h>

#include <media/v4l2-int-device.h>
#include <media/lv8093.h>
#include <mach/gpio.h>

#include "lv8093_regs.h"

static int
lv8093_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int __exit lv8093_remove(struct i2c_client *client);

struct lv8093_device {
	const struct lv8093_platform_data *pdata;
	struct v4l2_int_device *v4l2_int_device;
	struct i2c_client *i2c_client;
	int state;
	int power_state;
};

static const struct i2c_device_id lv8093_id[] = {
	{LV8093_NAME, 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, lv8093_id);

static struct i2c_driver lv8093_i2c_driver = {
	.driver = {
		   .name = LV8093_NAME,
		   .owner = THIS_MODULE,
		   },
	.probe = lv8093_probe,
	.remove = __exit_p(lv8093_remove),
	.id_table = lv8093_id,
};

static struct lv8093_device lv8093 = {
	.state = LENS_NOT_DETECTED,
};

static struct vcontrol {
	struct v4l2_queryctrl qc;
} video_control[] = {
	{
		{
		.id = V4L2_CID_FOCUS_RELATIVE,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Lens Relative Position",
		.minimum = 0,
		.maximum = 0,
		.step = LV8093_MAX_RELATIVE_STEP,
		.default_value = 0,
		}
	,}
};

static struct i2c_driver lv8093_i2c_driver;

static struct lv8093_lens_settings {
	u8 reg;
	u8 val;
} lens_settings[] = {
	{	/* Set control register */
		.reg = CAMAF_LV8093_CTL_REG,
		.val = CAMAF_LV8093_GATE0 |
				CAMAF_LV8093_ENIN |
				CAMAF_LV8093_CKSEL_ONE |
				CAMAF_LV8093_RET2 |
				CAMAF_LV8093_INIT_OFF,
	},
	{	/* Specify number of clocks per period */
		.reg = CAMAF_LV8093_RST_REG,
		.val = (LV8093_CLK_PER_PERIOD - 1),
	},
	{	/* Set the GATE_A pulse set value */
		.reg = CAMAF_LV8093_GTAS_REG,
		.val = (LV8093_TIME_GATEA + 1),
	},
	{	/* Set the GATE_B pulse reset value */
		.reg = CAMAF_LV8093_GTBR_REG,
		.val = (LV8093_TIME_GATEA + 1 + LV8093_TIME_OFF),
	},
	{	/* Set the GATE_B pulse set value */
		.reg = CAMAF_LV8093_GTBS_REG,
		.val = (LV8093_TIME_GATEA + 1 +
				LV8093_TIME_OFF + LV8093_TIME_GATEB),
	},
	{	/* Specific the number of output pulse steps */
		.reg = CAMAF_LV8093_STP_REG,
		.val = LV8093_STP,
	},
	{	/* Set the number of swing back of init sequence performed */
		.reg = CAMAF_LV8093_MOV_REG,
		.val = 0,
	},
};

/**
 * find_vctrl - Finds the requested ID in the video control structure array
 * @id: ID of control to search the video control array for
 *
 * Returns the index of the requested ID from the control structure array
 */
static int find_vctrl(int id)
{
	int i;

	if (id < V4L2_CID_BASE)
		return -EDOM;

	for (i = (ARRAY_SIZE(video_control) - 1); i >= 0; i--)
		if (video_control[i].qc.id == id)
			break;
	if (i < 0)
		i = -EINVAL;
	return i;
}

/**
 * lv8093_reg_read - Reads a value from a register in LV8093 Piezo
 * driver device.
 * @client: Pointer to structure of I2C client.
 * @value: Pointer to u16 for returning value of register to read.
 *
 * Returns zero if successful, or non-zero otherwise.
 **/
static int lv8093_reg_read(struct i2c_client *client, u8 *value)
{
	int err;
	struct i2c_msg msg[1];

	if (!client->adapter)
		return -ENODEV;

	msg->addr = client->addr;
	msg->flags = I2C_M_RD;
	msg->len = 1;
	msg->buf = value;

	err = i2c_transfer(client->adapter, msg, 1);

	if (err < 0)
		v4l_err(client, "i2c read failed with error %i", err);

	return err;
}

/**
 * lv8093_reg_write - Writes a value to a register in LV8093 Piezo
 * driver device.
 * @client: Pointer to structure of I2C client.
 * @reg: Register to write.
 * @value: Value of register to write.
 *
 * Returns zero or +ve if successful, -ve for error.
 **/
static int lv8093_reg_write(struct i2c_client *client, u8 reg, u8 value)
{
	int err;
	struct i2c_msg msg[1];
	unsigned char data[2];

	if (!client->adapter)
		return -ENODEV;

	msg->addr = client->addr;
	msg->flags = 0;
	msg->len = 2;
	msg->buf = data;

	data[0] = reg;
	data[1] = value;

	err = i2c_transfer(client->adapter, msg, 1);
	if (err < 0)
		v4l_err(client, "i2c write failed with error %i", err);

	return err;
}

/**
 * lv8093_detect - Detects LV8093 Piezo driver device.
 * @client: Pointer to structure of I2C client.
 *
 * Returns 0 if successful, -1 if camera is off or if test register value
 * wasn't stored properly, or return error from lv8093_reg_write function.
 **/
static int lv8093_detect(struct i2c_client *client)
{
	int err = 0;
	u8 rposn = 0;

	err = lv8093_reg_write(client, CAMAF_LV8093_CTL_REG,
				CAMAF_LV8093_GATE0 |
				CAMAF_LV8093_CKSEL_ONE |
				CAMAF_LV8093_RET2 |
				CAMAF_LV8093_INIT_OFF);

	if (err < 0) {
		v4l_err(client, "Unable to write LV8093\n");
		return err;
	}

	err = lv8093_reg_read(client, &rposn);
	if (err < 0) {
		v4l_err(client, "Unable to read LV8093\n");
		return err;
	}

	return err;
}

/**
 * lv8093_reginit - Initializes LV8093 Piezo driver device.
 * @client: Pointer to structure of I2C client.
 *
 * Returns 0 if successful, or returns errors from lv8093_reg_write.
 **/
static int lv8093_reginit(struct i2c_client *client)
{
	int i, err = 0;

	for (i = 0; i < ARRAY_SIZE(lens_settings); i++) {

		err = lv8093_reg_write(client,
				lens_settings[i].reg, lens_settings[i].val);

		if (err < 0) {
			v4l_err(client, "Unable to initialize LV8093\n");
			return err;
		}
	}

	return 0;
}

/**
 * lv8093_af_setfocus - Sets the desired focus.
 * @relpos: Relative focus position:
 * 			-ve - Direction INFINITY.
 * 			+ve - Direction MACRO.
 * 			abs(relpos) gives number of steps in desired direction.
 *
 * Returns 0 on success, -EINVAL if camera is off or returned errors
 * from lv8093_reg_write function.
 **/
int lv8093_af_setfocus(s16 relpos)
{
	struct lv8093_device *af_dev = &lv8093;
	struct i2c_client *client = af_dev->i2c_client;
	u8 num_pulses = abs(relpos);
	int ret = 0;

	if ((af_dev->power_state == V4L2_POWER_OFF) ||
	    (af_dev->power_state == V4L2_POWER_STANDBY))
		return -EINVAL;

	if (relpos >= 0) {
		/* Move lens in Macro direction */
		ret |= lv8093_reg_write(client, CAMAF_LV8093_DRVPLS_REG,
			0 & (~CAMAF_LV8093_MAC_DIR));
		ret |= lv8093_reg_write(client, CAMAF_LV8093_DRVPLS_REG,
			num_pulses | CAMAF_LV8093_MAC_DIR);

	} else {
		/* Move lens in Infinite direction */
		ret |= lv8093_reg_write(client, CAMAF_LV8093_DRVPLS_REG,
			0 | CAMAF_LV8093_MAC_DIR);
		ret |= lv8093_reg_write(client, CAMAF_LV8093_DRVPLS_REG,
			num_pulses & (~CAMAF_LV8093_MAC_DIR));

	}

	if (ret < 0) {
		v4l_err(client, "Unable to write " LV8093_NAME
			" lens HW\n");
		return -EINVAL;
	}

	return 0;
}

/**
 * lv8093_is_busy - Read busy bit.
 *
 * Returns:
 *  0 for READY, -EBUSY for device busy, -EINVAL on error.
 **/
int lv8093_is_busy(void)
{
	struct lv8093_device *af_dev = &lv8093;
	struct i2c_client *client = af_dev->i2c_client;
	int ret;
	u8  regval = 0;

	ret = lv8093_reg_read(client, &regval);

	if (ret < 0) {
		dev_err(&client->dev, "Unable to read " LV8093_NAME
			" lens HW\n");
		return -EINVAL;
	}

	if (regval & CAMAF_LV8093_BUSY)
		return -EBUSY;

	return 0;
}

/**
 * ioctl_queryctrl - V4L2 lens interface handler for VIDIOC_QUERYCTRL ioctl
 * @s: pointer to standard V4L2 device structure
 * @qc: standard V4L2 VIDIOC_QUERYCTRL ioctl structure
 *
 * If the requested control is supported, returns the control information
 * from the video_control[] array.  Otherwise, returns -EINVAL if the
 * control is not supported.
 */
static int ioctl_queryctrl(struct v4l2_int_device *s, struct v4l2_queryctrl *qc)
{
	int i;

	i = find_vctrl(qc->id);
	if (i == -EINVAL)
		qc->flags = V4L2_CTRL_FLAG_DISABLED;

	if (i < 0)
		return -EINVAL;

	*qc = video_control[i].qc;
	return 0;
}

/**
 * ioctl_s_ctrl - V4L2 LV8093 lens interface handler for VIDIOC_S_CTRL ioctl
 * @s: pointer to standard V4L2 device structure
 * @vc: standard V4L2 VIDIOC_S_CTRL ioctl structure
 *
 * If the requested control is supported, sets the control's current
 * value in HW.
 * Otherwise, returns -EINVAL if the control is not supported.
 */
static int ioctl_s_ctrl(struct v4l2_int_device *s, struct v4l2_control *vc)
{
	int retval = -EINVAL;
	int i;
	struct vcontrol *lvc;

	i = find_vctrl(vc->id);
	if (i < 0)
		return -EINVAL;
	lvc = &video_control[i];

	switch (vc->id) {
	case V4L2_CID_FOCUS_RELATIVE:
		retval = lv8093_af_setfocus((s16)vc->value);
		break;
	}

	return retval;
}

/**
 * ioctl_g_ctrl - V4L2 LV8093 lens interface handler for VIDIOC_G_CTRL ioctl
 * @s: pointer to standard V4L2 device structure
 * @vc: standard V4L2 VIDIOC_S_CTRL ioctl structure
 *
 * For V4L2_CID_FOCUS_RELATIVE control always returns the control's value
 * as zero. However, the return value is used to return whether the device
 * is busy still moving the lens. It will do this by returning -EBUSY (busy)
 * or 0 (ready).
 * Otherwise, returns -EINVAL if the control is not supported.
 */
static int ioctl_g_ctrl(struct v4l2_int_device *s, struct v4l2_control *vc)
{
	int i, retval = -EINVAL;
	struct vcontrol *lvc;

	i = find_vctrl(vc->id);
	if (i < 0)
		return -EINVAL;
	lvc = &video_control[i];

	switch (vc->id) {
	case V4L2_CID_FOCUS_RELATIVE:
		retval = lv8093_is_busy();
		vc->value = 0;
		break;
	}
	return retval;
}

/**
 * ioctl_g_priv - V4L2 sensor interface handler for vidioc_int_g_priv_num
 * @s: pointer to standard V4L2 device structure
 * @p: void pointer to hold sensor's private data address
 *
 * Returns device's (sensor's) private data area address in p parameter
 */
static int ioctl_g_priv(struct v4l2_int_device *s, void *p)
{
	struct lv8093_device *lens = s->priv;

	return lens->pdata->priv_data_set(p);
}

static int lv8093_power_on(struct lv8093_device *lens)
{
	struct i2c_client *c = lens->i2c_client;
	int rval = -EINVAL;

	if (lens->pdata->power_set)
		rval = lens->pdata->power_set(V4L2_POWER_ON);

	if (rval < 0) {
		v4l_err(c, "Unable to set the power state ON\n");
		return rval;
	}

	return 0;
}

static int lv8093_power_off(struct lv8093_device *lens)
{
	struct i2c_client *c = lens->i2c_client;
	int rval = -EINVAL;

	if (lens->pdata->power_set)
		rval = lens->pdata->power_set(V4L2_POWER_OFF);

	if (rval < 0) {
		v4l_err(c, "Unable to set the power state OFF\n");
		return rval;
	}

	return 0;
}

static int lv8093_power_standby(struct lv8093_device *lens)
{
	struct i2c_client *c = lens->i2c_client;
	int rval = -EINVAL;

	if (lens->pdata->power_set)
		rval = lens->pdata->power_set(V4L2_POWER_STANDBY);

	if (rval < 0) {
		v4l_err(c, "Unable to set the power state STANDBY\n");
		return rval;
	}

	return 0;
}

/**
 * ioctl_s_power - V4L2 sensor interface handler for vidioc_int_s_power_num
 * @s: pointer to standard V4L2 device structure
 * @on: power state to which device is to be set
 *
 * Sets devices power state to requested state, if possible.
 */
static int ioctl_s_power(struct v4l2_int_device *s, enum v4l2_power on)
{
	struct lv8093_device *lens = s->priv;
	struct i2c_client *c = lens->i2c_client;
	int rval = 0;

	switch (on) {
	case V4L2_POWER_ON:
		rval = lv8093_power_on(lens);
		if (rval)
			goto error;

		if (lens->power_state == V4L2_POWER_STANDBY) {
			rval = lv8093_reginit(c);
			if (rval < 0) {
				v4l_err(c, "Unable to initialize " LV8093_NAME
					" lens HW\n");
				lens->state = LENS_NOT_DETECTED;
				return rval;
			}
		}
		break;
	case V4L2_POWER_OFF:
		rval = lv8093_power_off(lens);
		break;
	case V4L2_POWER_STANDBY:
		rval = lv8093_power_standby(lens);
		break;
	}

	lens->power_state = on;
error:
	return rval;
}

/**
 * ioctl_dev_init - V4L2 sensor interface handler for vidioc_int_dev_init_num
 * @s: pointer to standard V4L2 device structure
 *
 * Initialise the device when slave attaches to the master.  Returns 0 if
 * lv8093 device could be found, otherwise returns appropriate error.
 */
static int ioctl_dev_init(struct v4l2_int_device *s)
{
	struct lv8093_device *lens = s->priv;
	struct i2c_client *c = lens->i2c_client;
	int err;

	err = lv8093_power_on(lens);
	if (err)
		return -ENODEV;

	err = lv8093_detect(c);
	if (err < 0) {
		v4l_err(c, "Unable to detect " LV8093_NAME
			" lens HW\n");
		lens->state = LENS_NOT_DETECTED;
		return err;
	}
	lens->state = LENS_DETECTED;
	pr_info(LV8093_NAME " Lens HW detected\n");

	err = lv8093_reginit(c);
	if (err < 0) {
		v4l_err(c, "Unable to initialize " LV8093_NAME
			" lens HW\n");
		lens->state = LENS_NOT_DETECTED;
		return err;
	}

	err = lv8093_power_off(lens);
	if (err)
		return -ENODEV;

	return 0;
}

static struct v4l2_int_ioctl_desc lv8093_ioctl_desc[] = {
	{.num = vidioc_int_s_power_num,
	 .func = (v4l2_int_ioctl_func *) ioctl_s_power},
	{ .num = vidioc_int_dev_init_num,
	  .func = (v4l2_int_ioctl_func *)ioctl_dev_init},
	{.num = vidioc_int_g_priv_num,
	 .func = (v4l2_int_ioctl_func *) ioctl_g_priv},
	{.num = vidioc_int_queryctrl_num,
	 .func = (v4l2_int_ioctl_func *) ioctl_queryctrl},
	{.num = vidioc_int_s_ctrl_num,
	 .func = (v4l2_int_ioctl_func *) ioctl_s_ctrl},
	{.num = vidioc_int_g_ctrl_num,
	 .func = (v4l2_int_ioctl_func *) ioctl_g_ctrl},
};

static struct v4l2_int_slave lv8093_slave = {
	.ioctls = lv8093_ioctl_desc,
	.num_ioctls = ARRAY_SIZE(lv8093_ioctl_desc),
};

static struct v4l2_int_device lv8093_int_device = {
	.module = THIS_MODULE,
	.name = LV8093_NAME,
	.priv = &lv8093,
	.type = v4l2_int_type_slave,
	.u = {
	      .slave = &lv8093_slave,
	      },
};

/**
 * lv8093_probe - Probes the driver for valid I2C attachment.
 * @client: Pointer to structure of I2C client.
 *
 * Returns 0 if successful, or -EBUSY if unable to get client attached data.
 **/
static int
lv8093_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct lv8093_device *lens = &lv8093;
	int err;

	if (i2c_get_clientdata(client)) {
		v4l_err(client, "DTA BUSY %s\n", client->name);
		return -EBUSY;
	}

	lens->pdata = client->dev.platform_data;

	if (!lens->pdata) {
		v4l_err(client, "no platform data?\n");
		return -ENODEV;
	}

	lens->v4l2_int_device = &lv8093_int_device;

	lens->i2c_client = client;
	i2c_set_clientdata(client, lens);

	err = v4l2_int_device_register(lens->v4l2_int_device);
	if (err) {
		v4l_err(client, "Failed to Register "
		       LV8093_NAME " as V4L2 device.\n");
		i2c_set_clientdata(client, NULL);
	} else {
		v4l_info(client, "Registered " LV8093_NAME
			" as V4L2 device.\n");
	}

	return err;
}

/**
 * lv8093_remove - Routine when device its unregistered from I2C
 * @client: Pointer to structure of I2C client.
 *
 * Returns 0 if successful, or -ENODEV if the client isn't attached.
 **/
static int __exit lv8093_remove(struct i2c_client *client)
{
	struct lv8093_device *lens = i2c_get_clientdata(client);

	if (!client->adapter)
		return -ENODEV;

	v4l2_int_device_unregister(lens->v4l2_int_device);
	i2c_set_clientdata(client, NULL);
	return 0;
}

/**
 * lv8093_init - Module initialisation.
 *
 * Returns 0 if successful, or -EINVAL if device couldn't be initialized, or
 * added as a character device.
 **/
static int __init lv8093_init(void)
{
	int err;

	err = i2c_add_driver(&lv8093_i2c_driver);
	if (err)
		goto fail;
	pr_info("Registered " LV8093_NAME " as i2c device.\n");

	return err;
fail:
	pr_err("Failed to register " LV8093_NAME " as i2c driver.\n");
	return err;
}

late_initcall(lv8093_init);

/**
 * lv8093_cleanup - Module cleanup.
 **/
static void __exit lv8093_cleanup(void)
{
	i2c_del_driver(&lv8093_i2c_driver);
}

module_exit(lv8093_cleanup);

MODULE_AUTHOR("Texas Instruments");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("LV8093 LENS driver");
