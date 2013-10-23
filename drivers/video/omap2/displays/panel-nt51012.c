/*
 * Novatek NT51012 MIPI DSI driver
 *
 * Copyright (C) Texas Instruments
 * Author: Subbaraman Narayanamurthy <x0164410@ti.com>
 *
 * Based on original version from Jerry Alexander <x0135174@ti.com>
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


#include <linux/module.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/jiffies.h>
#include <linux/sched.h>
#include <linux/backlight.h>
#include <linux/fb.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/i2c.h>

#include <video/omapdss.h>
#include <video/omap-panel-generic.h>
#include <video/omap-panel-nt51012.h>
#ifdef CONFIG_AMAZON_METRICS_LOG
#include <linux/metricslog.h>
#endif
#include "../dss/dss.h"

#include "panel-nt51012.h"

static unsigned char first_suspend = 1;
static unsigned char color_sat_enabled;

static unsigned char gamma_ctrl;
static unsigned char pwm_step_ctrl;
static unsigned char pwm_frame_ctrl;
static struct i2c_client *nt51012_pmic_client;
static char *board_type_str;

char *idme_get_board_type_string(void);

DEFINE_SPINLOCK(lcd_disable_lock);

#ifdef CONFIG_MACH_OMAP4_BOWSER_SUBTYPE_SOHO
	/* Used to specifiy LCD_EN (GPIO 190) off period in ms*/
	#define NT51012_EN_GPIO_TIME	13
#endif

static struct omap_video_timings nt51012_timings = {
	.x_res		= NT51012_WIDTH,
	.y_res		= NT51012_HEIGHT,
	.pixel_clock	= NT51012_PCLK,
	.hfp		= NT51012_HFP,
	.hsw            = NT51012_HSW,
	.hbp            = NT51012_HBP,
	.vfp            = NT51012_VFP,
	.vsw            = NT51012_VSW,
	.vbp            = NT51012_VBP,
};

/* device private data structure */
struct nt51012_data {
	struct mutex lock;

	struct omap_dss_device *dssdev;

	int channel0;
	int channel1;
	int cabc_mode;
};

static const char * const cabc_modes[] = {
	"moving-image",
	"still-image",
	"ui",
	"off",
};

static int nt51012_pmic_read(struct i2c_client *client, u8 reg, u8 *val)
{
	struct i2c_msg msg[2];
	unsigned char data;
	int ret;

	if (!client || !client->adapter)
		return -ENODEV;

	if (!val)
		return -EINVAL;

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].buf = &reg;

	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 1;
	msg[1].buf = &data;

	ret = i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg));
	if (ret != ARRAY_SIZE(msg))
		return ret;

	*val = data;

	return 0;
}

static int nt51012_pmic_write(struct i2c_client *client, u8 reg, u8 val)
{
	struct i2c_msg msg[1];
	unsigned char data[2];
	int ret;

	if (!client || !client->adapter)
		return -ENODEV;

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = 2;
	msg[0].buf = data;
	data[0] = reg;
	data[1] = val;

	ret = i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg));

	return (ret == ARRAY_SIZE(msg)) ? 0 : ret;
}

static int nt51012_pmic_upload(struct i2c_client *client,
	struct nt51012_pmic_regs *regs)
{
	if (!client || !regs)
		return -EINVAL;

	while (!(regs->clr == 0xFF && regs->set == 0xFF)) {
		u8 val = 0;
		if (regs->clr || regs->set)
			nt51012_pmic_read(client, regs->reg, &val);

		val = regs->val | (val & !regs->clr) | regs->set;
		nt51012_pmic_write(client, regs->reg, val);

		regs++;
	}

	return 0;
}

static int nt51012_pmic_enable(struct i2c_client *client)
{
	struct nt51012_pmic_data *data;

	if (!client)
		return -EINVAL;

	/* Get I2C device platform data */
	data = dev_get_platdata(&client->dev);
	if (!data)
		return -EINVAL;

	return nt51012_pmic_upload(client, data->fw_enable);
}

static int nt51012_pmic_disable(struct i2c_client *client)
{
	struct nt51012_pmic_data *data;

	if (!client)
		return -EINVAL;

	/* Get I2C device platform data */
	data = dev_get_platdata(&client->dev);
	if (!data)
		return -EINVAL;

	return nt51012_pmic_upload(client, data->fw_disable);
}

static int nt51012_pmic_probe(struct i2c_client *client,
	const struct i2c_device_id *dev_id)
{
	nt51012_pmic_enable(client);
	nt51012_pmic_client = client;

	return 0;
}

static int nt51012_pmic_remove(struct i2c_client *client)
{
	nt51012_pmic_client = NULL;

	return 0;
}

#define DRIVER_NAME		"nt51012_pmic"

static const struct i2c_device_id nt51012_pmic_i2c_id_table[] = {
	{ DRIVER_NAME, 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, nt51012_pmic_i2c_id_table);

static struct i2c_driver nt51012_pmic_i2c_driver = {
	.driver = {
		.name = DRIVER_NAME,
		.owner = THIS_MODULE,
	},
	.probe = nt51012_pmic_probe,
	.remove = nt51012_pmic_remove,
	.id_table = nt51012_pmic_i2c_id_table,
};

static ssize_t nt51012_show_cabc_mode(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct nt51012_data *d2d = dev_get_drvdata(dev);
	const char *mode_str;
	int mode;
	int len;

	mode = d2d->cabc_mode;

	if (mode >= 0 && mode < ARRAY_SIZE(cabc_modes))
		mode_str = cabc_modes[mode];
	else
		mode_str = "unknown";

	len = snprintf(buf, PAGE_SIZE, "%s\n", mode_str);

	return len < PAGE_SIZE - 1 ? len : PAGE_SIZE - 1;
}

static ssize_t nt51012_set_cabc_mode(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct nt51012_data *d2d = dev_get_drvdata(dev);
	int i;
	int r = 0;

	for (i = 0; i < ARRAY_SIZE(cabc_modes); i++) {
		if (sysfs_streq(cabc_modes[i], buf))
			break;
	}

	if (i == ARRAY_SIZE(cabc_modes))
		return -EINVAL;

	if (d2d->cabc_mode == i)
		return -EINVAL;
	else {
		mutex_lock(&d2d->lock);

		if (d2d->dssdev->state == OMAP_DSS_DISPLAY_SUSPENDED) {
			d2d->cabc_mode = i;
			mutex_unlock(&d2d->lock);
			return r;
		}

		dsi_bus_lock(d2d->dssdev);

		/* NT51012 control register 0xB0 has CABC_ENB[7:6] */
		r = dsi_vc_dcs_write_1(d2d->dssdev, 1, 0xB0, (i<<6)|0x3E);

		dsi_bus_unlock(d2d->dssdev);

		if (r) {
			mutex_unlock(&d2d->lock);
			return -EINVAL;
		} else
			d2d->cabc_mode = i;

		mutex_unlock(&d2d->lock);
	}

	return count;
}

static DEVICE_ATTR(cabc_mode, S_IRUGO | S_IWUSR, nt51012_show_cabc_mode,
		nt51012_set_cabc_mode);

static ssize_t nt51012_show_cabc_modes(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	int len;
	int i;

	for (i = 0, len = 0;
	     len < PAGE_SIZE && i < ARRAY_SIZE(cabc_modes); i++)
		len += snprintf(&buf[len], PAGE_SIZE - len, "%s%s%s",
			i ? " " : "", cabc_modes[i],
			i == ARRAY_SIZE(cabc_modes) - 1 ? "\n" : "");

	return len < PAGE_SIZE ? len : PAGE_SIZE - 1;
}

static DEVICE_ATTR(cabc_modes, S_IRUGO,
		nt51012_show_cabc_modes, NULL);

static ssize_t nt51012_set_gamma_ctrl(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct nt51012_data *d2d = dev_get_drvdata(dev);
	int value;
	int r = 0;

	if (sscanf(buf, "%d", &value) != 1)
		return -EINVAL;

	mutex_lock(&d2d->lock);

	if (d2d->dssdev->state == OMAP_DSS_DISPLAY_SUSPENDED) {
		mutex_unlock(&d2d->lock);
		return r;
	}

	dsi_bus_lock(d2d->dssdev);

	gamma_ctrl = value & 0xFF;

	r = dsi_vc_dcs_write_1(d2d->dssdev, 1, 0xF5, gamma_ctrl);

	dsi_bus_unlock(d2d->dssdev);

	if (r) {
		mutex_unlock(&d2d->lock);
		return -EINVAL;
	}

	mutex_unlock(&d2d->lock);

	return count;
}

static ssize_t nt51012_show_gamma_ctrl(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	int len;
	len = snprintf(buf, PAGE_SIZE, "%x\n", gamma_ctrl);
	return len < PAGE_SIZE - 1 ? len : PAGE_SIZE - 1;
}

static DEVICE_ATTR(gamma_ctrl, S_IRUGO | S_IWUSR, nt51012_show_gamma_ctrl,
	nt51012_set_gamma_ctrl);

static ssize_t nt51012_set_pwm_step_ctrl(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct nt51012_data *d2d = dev_get_drvdata(dev);
	int value;
	int r = 0;

	if (sscanf(buf, "%d", &value) != 1)
		return -EINVAL;

	mutex_lock(&d2d->lock);

	if (d2d->dssdev->state == OMAP_DSS_DISPLAY_SUSPENDED) {
		mutex_unlock(&d2d->lock);
		return r;
	}

	dsi_bus_lock(d2d->dssdev);

	pwm_step_ctrl = value & 0xFF;

	r = dsi_vc_dcs_write_1(d2d->dssdev, 1, 0xC3, pwm_step_ctrl);

	dsi_bus_unlock(d2d->dssdev);

	if (r) {
		mutex_unlock(&d2d->lock);
		return -EINVAL;
	}

	mutex_unlock(&d2d->lock);

	return count;
}

static ssize_t nt51012_show_pwm_step_ctrl(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	int len;
	len = snprintf(buf, PAGE_SIZE, "%x\n", pwm_step_ctrl);
	return len < PAGE_SIZE - 1 ? len : PAGE_SIZE - 1;
}

static DEVICE_ATTR(pwm_step_ctrl, S_IRUGO | S_IWUSR, nt51012_show_pwm_step_ctrl,
	nt51012_set_pwm_step_ctrl);

static ssize_t nt51012_set_pwm_frame_ctrl(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct nt51012_data *d2d = dev_get_drvdata(dev);
	int value;
	int r = 0;

	if (sscanf(buf, "%d", &value) != 1)
		return -EINVAL;

	mutex_lock(&d2d->lock);

	if (d2d->dssdev->state == OMAP_DSS_DISPLAY_SUSPENDED) {
		mutex_unlock(&d2d->lock);
		return r;
	}

	dsi_bus_lock(d2d->dssdev);

	pwm_frame_ctrl = value & 0xFF;

	r = dsi_vc_dcs_write_1(d2d->dssdev, 1, 0xC6, pwm_frame_ctrl);

	dsi_bus_unlock(d2d->dssdev);

	if (r) {
		mutex_unlock(&d2d->lock);
		return -EINVAL;
	}

	mutex_unlock(&d2d->lock);

	return count;
}

static ssize_t nt51012_show_pwm_frame_ctrl(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	int len;
	len = snprintf(buf, PAGE_SIZE, "%x\n", pwm_frame_ctrl);
	return len < PAGE_SIZE - 1 ? len : PAGE_SIZE - 1;
}

static DEVICE_ATTR(pwm_frame_ctrl, S_IRUGO | S_IWUSR,
	nt51012_show_pwm_frame_ctrl, nt51012_set_pwm_frame_ctrl);

static struct attribute *nt51012_attrs[] = {
	&dev_attr_cabc_mode.attr,
	&dev_attr_cabc_modes.attr,
	&dev_attr_gamma_ctrl.attr,
	&dev_attr_pwm_step_ctrl.attr,
	&dev_attr_pwm_frame_ctrl.attr,
	NULL,
};

static struct attribute_group nt51012_attr_group = {
	.attrs = nt51012_attrs,
};

static struct panel_board_data *get_board_data(struct omap_dss_device *dssdev)
{
	return (struct panel_board_data *)dssdev->data;
}

static void nt51012_get_timings(struct omap_dss_device *dssdev,
		struct omap_video_timings *timings)
{
	*timings = dssdev->panel.timings;
}

static void nt51012_set_timings(struct omap_dss_device *dssdev,
		struct omap_video_timings *timings)
{
}

static int nt51012_check_timings(struct omap_dss_device *dssdev,
		struct omap_video_timings *timings)
{
	if (nt51012_timings.x_res != timings->x_res ||
	    nt51012_timings.y_res != timings->y_res ||
	    nt51012_timings.pixel_clock != timings->pixel_clock ||
	    nt51012_timings.hsw != timings->hsw ||
	    nt51012_timings.hfp != timings->hfp ||
	    nt51012_timings.hbp != timings->hbp ||
	    nt51012_timings.vsw != timings->vsw ||
	    nt51012_timings.vfp != timings->vfp ||
	    nt51012_timings.vbp != timings->vbp)
		return -EINVAL;

	return 0;
}

static void nt51012_get_resolution(struct omap_dss_device *dssdev,
		u16 *xres, u16 *yres)
{
	*xres = nt51012_timings.x_res;
	*yres = nt51012_timings.y_res;
}

static int nt51012_lcd_enable(struct omap_dss_device *dssdev)
{
	struct panel_board_data *board_data = get_board_data(dssdev);
	unsigned long irq_flags;

	if (board_data->lcd_en_gpio == -1) {
		dev_err(&dssdev->dev, "lcd_en_gpio == -1\n");
		return -1;
	}

	spin_lock_irqsave(&lcd_disable_lock, irq_flags);

	gpio_set_value(board_data->lcd_en_gpio, 1);
	if (board_data->lcd_vsys_gpio > 0) {
		gpio_set_value(board_data->lcd_vsys_gpio, 1);
	}

	spin_unlock_irqrestore(&lcd_disable_lock, irq_flags);

	/* Make sure lcd vsys has enough time to ramp up
	before issuing dsi commands later on */
	usleep_range(2000, 2500);
	gpio_set_value(board_data->cabc_en_gpio, 1);
	nt51012_pmic_enable(nt51012_pmic_client);
	dev_dbg(&dssdev->dev, "nt51012_lcd_enable\n");

	return 0;
}

static int nt51012_lcd_disable(struct omap_dss_device *dssdev)
{
	struct panel_board_data *board_data = get_board_data(dssdev);
	unsigned long irq_flags;

	if (board_data->lcd_en_gpio == -1)
		return -1;

	/* This suspend is part of fb_blank from HWC? Ignore this as it
	interrupts the panel init process */
	if (first_suspend) {
		dev_dbg(&dssdev->dev, "not nt51012_lcd_disable now\n");
		first_suspend = 0;
		return 0;
	}

	gpio_set_value(board_data->cabc_en_gpio, 0);
	msleep(100);
	nt51012_pmic_disable(nt51012_pmic_client);
	
	spin_lock_irqsave(&lcd_disable_lock, irq_flags);

	gpio_set_value(board_data->lcd_en_gpio, 0);
	if (board_data->lcd_vsys_gpio > 0)
		gpio_set_value(board_data->lcd_vsys_gpio, 0);

	spin_unlock_irqrestore(&lcd_disable_lock, irq_flags);

#ifdef CONFIG_MACH_OMAP4_BOWSER_SUBTYPE_SOHO
	usleep_range((NT51012_EN_GPIO_TIME * 1000),
			(NT51012_EN_GPIO_TIME * 1000) + 500);
#endif
	dev_dbg(&dssdev->dev, "nt51012_lcd_disable\n");

	return 0;
}

static int nt51012_probe(struct omap_dss_device *dssdev)
{
	struct nt51012_data *d2d;
	int r = 0;

	dev_dbg(&dssdev->dev, "nt51012_probe\n");

	dssdev->panel.config = OMAP_DSS_LCD_TFT;
	dssdev->panel.timings = nt51012_timings;
	dssdev->ctrl.pixel_size = 24;

	d2d = kzalloc(sizeof(*d2d), GFP_KERNEL);
	if (!d2d) {
		r = -ENOMEM;
		return r;
	}

	d2d->dssdev = dssdev;

#if defined(CONFIG_BACKLIGHT_LP855X)
	d2d->cabc_mode = 3; /* CABC off */
#else
	d2d->cabc_mode = 0; /* Moving image */
#endif

	mutex_init(&d2d->lock);

	dev_set_drvdata(&dssdev->dev, d2d);

	r = omap_dsi_request_vc(dssdev, &d2d->channel0);
	if (r)
		dev_err(&dssdev->dev, "failed to get virtual channel0\n");

	r = omap_dsi_set_vc_id(dssdev, d2d->channel0, 0);
	if (r)
		dev_err(&dssdev->dev, "failed to set VC_ID0\n");

	r = omap_dsi_request_vc(dssdev, &d2d->channel1);
	if (r)
		dev_err(&dssdev->dev, "failed to get virtual channel1\n");

	r = omap_dsi_set_vc_id(dssdev, d2d->channel1, 0);
	if (r)
		dev_err(&dssdev->dev, "failed to set VC_ID1\n");

	r = sysfs_create_group(&dssdev->dev.kobj, &nt51012_attr_group);
	if (r)
		dev_err(&dssdev->dev, "failed to create sysfs file\n");

	dev_dbg(&dssdev->dev, "nt51012_probe done\n");

	/* do I need an err and kfree(d2d) */
	return r;
}

static void nt51012_remove(struct omap_dss_device *dssdev)
{
	struct nt51012_data *d2d = dev_get_drvdata(&dssdev->dev);

	omap_dsi_release_vc(dssdev, d2d->channel0);
	omap_dsi_release_vc(dssdev, d2d->channel1);

	sysfs_remove_group(&dssdev->dev.kobj, &nt51012_attr_group);

	kfree(d2d);
}

static void nt51012_config(struct omap_dss_device *dssdev)
{
	struct nt51012_data *d2d = dev_get_drvdata(&dssdev->dev);
	u8 term_res_value = 0;
	u8 data = 0;
	int r = 0;

	/*
	 * Termination resistance configuration @ TCON
	 * 0x0F - 100ohm/100ohm
	 * 0x07 - open/100ohm
	 * 0x0B - 100ohm/open
	 * 0x0D - 200ohm/200ohm
	 */

	/* set it to open_100ohm */
	term_res_value = 0x0B;

	/* soft reset */
	dsi_vc_dcs_write_1(dssdev, 0, 0x01, 0x00);
	/* need atleast 20ms after reset */
	msleep(20);

	/* Read vendor id to set parameters for different panels */
	r = dsi_vc_dcs_read(dssdev, 0, 0xFC, &data, 1);
	if (r < 0)
		dev_err(&dssdev->dev, "dsi read err\n");
	else
		dev_dbg(&dssdev->dev, "data read: %x\n", data);

	/* Read bits [5:4] for vendor id */
	data = (data >> 4) & 0x03;

	dev_err(&dssdev->dev, "Vendor id: %d\n", data);

	switch (data) {
	case 1:
		/* CMI */
		/* set it to 200ohm_200ohm */
		term_res_value = 0x0D;
		break;
	case 2:
		/* LGD */
		/* set it to open_100ohm */
		term_res_value = 0x07;
		break;
	case 3:
		/* PLD */
		/* set it to 200ohm_200ohm */
		term_res_value = 0x0D;
		break;
	case 0:
	default:
		/* set it to 100ohm_open */
		term_res_value = 0x0B;
		break;
	}

	/* Differential impedance selection */
	dsi_vc_dcs_write_1(dssdev, 0, 0xAE, term_res_value);

	dsi_vc_dcs_write_1(dssdev, 0, 0xEE, 0xEA);
	dsi_vc_dcs_write_1(dssdev, 0, 0xEF, 0x5F);
	dsi_vc_dcs_write_1(dssdev, 0, 0xF2, 0x68);

	switch (data) {
	case 1:
		/* CMI */
		/* TCLV_OE timing adjust for Gate Pulse
		 * Modulation Function
		 */
		dsi_vc_dcs_write_1(dssdev, 0, 0xB2, 0x7F);
		break;
	case 2:
		/* LGD */
		/* TCLV_OE timing adjust for Gate Pulse
		 * Modulation Function
		 */
		dsi_vc_dcs_write_1(dssdev, 0, 0xB2, 0x7D);
		/* Gate OE width control which secures data
		 * charging time margin
		 */
		dsi_vc_dcs_write_1(dssdev, 0, 0xB6, 0x18);
		/* AVDDG off */
		dsi_vc_dcs_write_1(dssdev, 0, 0xD2, 0x64);
		break;
	case 3:
		/* PLD */
		/* Selection of amplifier */
		dsi_vc_dcs_write_1(dssdev, 0, 0xBE, 0x02);
		/* Adjust drive timing 1 */
		dsi_vc_dcs_write_1(dssdev, 0, 0xB5, 0x90);
		/* Adjust drive timing 2 */
		dsi_vc_dcs_write_1(dssdev, 0, 0xB6, 0x09);
		break;
	case 0:
	default:
		/* TCLV_OE timing adjust for Gate Pulse
		 * Modulation Function
		 */
		dsi_vc_dcs_write_1(dssdev, 0, 0xB2, 0x7D);
		/* Gate OE width control which secures data
		 * charging time margin
		 */
		dsi_vc_dcs_write_1(dssdev, 0, 0xB6, 0x18);
		/* AVDDG off */
		dsi_vc_dcs_write_1(dssdev, 0, 0xD2, 0x64);
		break;
	}

	/* Color saturation */
	if (color_sat_enabled) {
		dsi_vc_dcs_write_1(dssdev, 0, 0xB3, 0x03);

		dsi_vc_dcs_write_1(dssdev, 0, 0xC8, 0x04);
	}

	dsi_vc_dcs_write_1(dssdev, 0, 0xEE, 0x00);

	dsi_vc_dcs_write_1(dssdev, 0, 0xEF, 0x00);

	/* CABC settings suggested by Rajeev and Novatek */
	/* Gamma control */
	dsi_vc_dcs_write_1(dssdev, 0, 0xF5, 0xF0);

	/* PWM step control */
	dsi_vc_dcs_write_1(dssdev, 0, 0xC3, 0x41);

	/* PWM frame control */
	dsi_vc_dcs_write_1(dssdev, 0, 0xC6, 0x01);

	/* Read back values and store it*/

		r = dsi_vc_dcs_read(dssdev, 0, 0xF5, &gamma_ctrl, 1);
		if (r < 0)
			dev_err(&dssdev->dev, "dsi read err\n");
		else {
			r = dsi_vc_dcs_read(dssdev, 0, 0xC3, &pwm_step_ctrl, 1);
			if (r < 0)
				dev_err(&dssdev->dev, "dsi read err\n");

			r = dsi_vc_dcs_read(dssdev, 0, 0xC6,
					    &pwm_frame_ctrl, 1);
			if (r < 0)
				dev_err(&dssdev->dev,"dsi read err\n");
		}

	/* NT51012 control register 0xB0 has CABC_ENB[7:6] */
	dsi_vc_dcs_write_1(dssdev, 0, 0xB0, (d2d->cabc_mode<<6)|0x3E);
}

static int nt51012_power_on(struct omap_dss_device *dssdev)
{
	struct nt51012_data *d2d = dev_get_drvdata(&dssdev->dev);
	int r;

	/* At power on the first vsync has not been received yet */
	dssdev->first_vsync = false;

	dev_dbg(&dssdev->dev, "power_on\n");

	if (dssdev->platform_enable)
		dssdev->platform_enable(dssdev);

	r = omapdss_dsi_display_enable(dssdev);
	if (r) {
		dev_err(&dssdev->dev, "failed to enable DSI\n");
		goto err_disp_enable;
	}

	/* first_suspend flag is to ignore suspend one time during init process
	we use this flag to re-init the TCON once by disable/enable panel
	voltage. From the next time suspend will help doing this */

	if (!dssdev->skip_init) {

		/* enable lcd */
		r = nt51012_lcd_enable(dssdev);

		/* we would suggest having more than 70ms
		 * for T3 (PMIC_EN to MIPI_IN),
		 * to make sure power will be ready before MIPI data in.*/
		msleep(100);

		/* do extra job to match kozio registers (???) */
		dsi_videomode_panel_preinit(dssdev);

		/* On EVT1 the LCD power is permanent and there is no need
		 * to reinitialize the LCD every time.
		 * From EVT2 the LCD power (3V3&1V8) will off and on by toggling EN pin,
		 * so need to reinitialize the LCD every time.
		 */
		if(!(strcmp(board_type_str, "Soho PreEVT")) || !(strcmp(board_type_str, "Soho EVT1")))
			if (first_suspend)
				nt51012_config(dssdev);
			else
				dsi_vc_dcs_write_1(dssdev, 1, 0x10, 0x01);
		else
			nt51012_config(dssdev);

		/* Go to HS mode after sending all the DSI commands in
		 * LP mode
		 */
		omapdss_dsi_vc_enable_hs(dssdev, d2d->channel0, true);
		omapdss_dsi_vc_enable_hs(dssdev, d2d->channel1, true);

		dsi_enable_video_output(dssdev, d2d->channel0);
	} else {
		r = dss_mgr_enable(dssdev->manager);
		dssdev->skip_init = false;
	}

	dev_dbg(&dssdev->dev, "power_on done\n");

	return r;

err_disp_enable:
	if (dssdev->platform_disable)
		dssdev->platform_disable(dssdev);

	return r;
}

static void nt51012_power_off(struct omap_dss_device *dssdev)
{
	int r;
	struct nt51012_data *d2d = dev_get_drvdata(&dssdev->dev);

	dsi_disable_video_output(dssdev, d2d->channel0);

	/* Send sleep in command to the TCON */
	dsi_vc_dcs_write_1(dssdev, 1, 0x11, 0x01);

	omapdss_dsi_display_disable(dssdev, false, false);

	if (dssdev->platform_disable)
		dssdev->platform_disable(dssdev);

	/* disable lcd */
	r = nt51012_lcd_disable(dssdev);
	if (r)
		dev_err(&dssdev->dev, "failed to disable LCD\n");
}

static void nt51012_disable(struct omap_dss_device *dssdev)
{
	struct nt51012_data *d2d = dev_get_drvdata(&dssdev->dev);

#ifdef CONFIG_AMAZON_METRICS_LOG
	char buf[128];
	snprintf(buf, sizeof(buf), "%s:lcd:disable=1;CT;1:NR", __func__);
	log_to_metrics(ANDROID_LOG_INFO, "LCDEvent", buf);
#endif
	dev_dbg(&dssdev->dev, "disable\n");

	if (dssdev->state == OMAP_DSS_DISPLAY_ACTIVE) {
		mutex_lock(&d2d->lock);
		dsi_bus_lock(dssdev);

		nt51012_power_off(dssdev);

		dsi_bus_unlock(dssdev);
		mutex_unlock(&d2d->lock);
	}

	dssdev->state = OMAP_DSS_DISPLAY_DISABLED;
}

static int nt51012_enable(struct omap_dss_device *dssdev)
{
	struct nt51012_data *d2d = dev_get_drvdata(&dssdev->dev);
	int r = 0;
#ifdef CONFIG_AMAZON_METRICS_LOG
	char buf[128];
	snprintf(buf, sizeof(buf), "%s:lcd:enable=1;CT;1:NR", __func__);
	log_to_metrics(ANDROID_LOG_INFO, "LCDEvent", buf);
#endif

	dev_dbg(&dssdev->dev, "enable\n");

	if (dssdev->state != OMAP_DSS_DISPLAY_DISABLED)
		return -EINVAL;

	mutex_lock(&d2d->lock);
	dsi_bus_lock(dssdev);

	r = nt51012_power_on(dssdev);

	dsi_bus_unlock(dssdev);

	if (r) {
		dev_dbg(&dssdev->dev, "enable failed\n");
		dssdev->state = OMAP_DSS_DISPLAY_DISABLED;
	} else {
		dssdev->state = OMAP_DSS_DISPLAY_ACTIVE;
	}

	mutex_unlock(&d2d->lock);

	return r;
}

static int nt51012_suspend(struct omap_dss_device *dssdev)
{
	struct nt51012_data *nd = dev_get_drvdata(&dssdev->dev);
	int r;
#ifdef CONFIG_AMAZON_METRICS_LOG
	char buf[128];
	snprintf(buf, sizeof(buf), "%s:lcd:suspend=1;CT;1:NR", __func__);
	log_to_metrics(ANDROID_LOG_INFO, "LCDEvent", buf);
#endif

	dev_dbg(&dssdev->dev, "suspend\n");

	mutex_lock(&nd->lock);

	if (dssdev->state != OMAP_DSS_DISPLAY_ACTIVE) {
		r = -EINVAL;
		goto err;
	}

	dsi_bus_lock(dssdev);

	nt51012_power_off(dssdev);

	dsi_bus_unlock(dssdev);

	dssdev->state = OMAP_DSS_DISPLAY_SUSPENDED;

	mutex_unlock(&nd->lock);

return 0;
err:
	mutex_unlock(&nd->lock);
	return r;
}


static int nt51012_resume(struct omap_dss_device *dssdev)
{
	struct nt51012_data *nd = dev_get_drvdata(&dssdev->dev);
	int r;
#ifdef CONFIG_AMAZON_METRICS_LOG
	char buf[128];
	snprintf(buf, sizeof(buf), "%s:lcd:resume=1;CT;1:NR", __func__);
	log_to_metrics(ANDROID_LOG_INFO, "LCDEvent", buf);
#endif

	if (first_suspend) {
		dev_dbg(&dssdev->dev, "not resuming now\n");
		first_suspend = 0;
		return 0;
	}
	dev_dbg(&dssdev->dev, "resume\n");

	mutex_lock(&nd->lock);

	if (dssdev->state != OMAP_DSS_DISPLAY_SUSPENDED) {
		r = -EINVAL;
		goto err;
	}

	dsi_bus_lock(dssdev);

	r = nt51012_power_on(dssdev);

	dsi_bus_unlock(dssdev);

	if (r)
		dssdev->state = OMAP_DSS_DISPLAY_DISABLED;
	else
		dssdev->state = OMAP_DSS_DISPLAY_ACTIVE;

	mutex_unlock(&nd->lock);

	return r;
err:
	mutex_unlock(&nd->lock);
	return r;
}

static struct omap_dss_driver nt51012_driver = {
	.probe		= nt51012_probe,
	.remove		= nt51012_remove,

	.enable		= nt51012_enable,
	.disable	= nt51012_disable,
	.suspend	= nt51012_suspend,
	.resume		= nt51012_resume,

	.get_resolution	= nt51012_get_resolution,
	.get_recommended_bpp = omapdss_default_get_recommended_bpp,

	.get_timings	= nt51012_get_timings,
	.set_timings	= nt51012_set_timings,
	.check_timings	= nt51012_check_timings,

	.driver         = {
		.name   = "nt51012",
		.owner  = THIS_MODULE,
	},
};

static int __init nt51012_init(void)
{
	int r;
	board_type_str = idme_get_board_type_string();

	r = i2c_add_driver(&nt51012_pmic_i2c_driver);
	if (r)
		printk(KERN_ERR "Unable to register NT51012 PMIC i2c driver\n");

	r = omap_dss_register_driver(&nt51012_driver);
	if (r < 0) {
		printk(KERN_ERR"nt51012 driver registration failed\n");
		return r;
	}

	return 0;
}

static void __exit nt51012_exit(void)
{
	omap_dss_unregister_driver(&nt51012_driver);
	i2c_del_driver(&nt51012_pmic_i2c_driver);
}

module_init(nt51012_init);
module_exit(nt51012_exit);

MODULE_AUTHOR("Subbaraman Narayanamurthy <x0164410@ti.com>");
MODULE_DESCRIPTION("NT51012 MIPI DSI Driver");
MODULE_LICENSE("GPL");
