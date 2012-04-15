/*
 * Boxer (Kindle Fire Version) panel support
 *
 * Copyright (C) 2008 Nokia Corporation
 * Author: Tomi Valkeinen <tomi.valkeinen@nokia.com>
 *
 * Copyright (c) 2010 Barnes & Noble
 * David Bolcsfoldi <dbolcsfoldi@intrinsyc.com>
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
#include <linux/platform_device.h>
#include <linux/spi/spi.h>
#include <asm/mach-types.h>
#include <video/omapdss.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>

/* Delay between Panel configuration and Panel enabling */
#define LCD_RST_DELAY		100
#define LCD_INIT_DELAY		200

static struct workqueue_struct *boxer_panel_wq;
static struct omap_dss_device *boxer_panel_dssdev;
static struct regulator *boxer_panel_regulator;
static struct spi_device *boxer_spi_device;
static atomic_t boxer_panel_is_enabled = ATOMIC_INIT(0);

extern u8 quanta_get_mbid(void);

/* Get FT i2c adapter for lock/unlock it */
struct i2c_adapter *g_ft_i2c_adapter = NULL;

extern void register_ft_i2c_adapter(struct i2c_adapter *adapter)
{
	g_ft_i2c_adapter = adapter;
}

extern void unregister_ft_i2c_adapter(struct i2c_adapter *adapter)
{
	g_ft_i2c_adapter = NULL;
}

static int spi_send(struct spi_device *spi,
		    unsigned char reg_addr, unsigned char reg_data)
{
	uint16_t msg = 0;
	int result = 0;
	msg = (reg_addr << 10) | reg_data;
	result = spi_write(spi, (unsigned char *) &msg, 2);
	if (result != 0)
		printk(KERN_ERR "error in spi_write %x\n", msg);
	else 
		udelay(10);
	
	return result;
}

static ssize_t set_cabc (struct device *dev, struct device_attribute *attr,
                         const char *buffer, size_t count)
{
	int mode = 0;
	sscanf(buffer, "%d", &mode);
	printk("the cabc value is %d\n", mode);
	switch(mode)
	{
	case 0:
		printk("set the cabc value to 0b00\n");
		spi_send(boxer_spi_device, 0x01, 0x30);
		break;
	case 1:
		printk("set the cabc value to 0b01\n");
		spi_send(boxer_spi_device, 0x01, 0x70);
		break;
	case 2:
		printk("set the cabc value to 0b10\n");
		spi_send(boxer_spi_device, 0x01, 0xb0);
		break;
	case 3:
		printk("set the cabc value to 0b11\n");
		spi_send(boxer_spi_device, 0x01, 0xf0);
		break;
	default:
		printk("set the cabc value to 0b00\n");
		spi_send(boxer_spi_device, 0x01, 0x30);
		break;
	}
		return count;
}

DEVICE_ATTR(cabc,      S_IRUGO | S_IWUGO, NULL, set_cabc);

static struct attribute *otter1_panel_attributes[] = {
       &dev_attr_cabc.attr,
        NULL
};

static struct attribute_group otter1_panel_attribute_group = {
       .attrs = otter1_panel_attributes
};

static void boxer_init_panel(void) {
	int vendor0=1, vendor1=1;
	int result = 0;
	mdelay(1);

	vendor1 = gpio_get_value(175); //LCD_ID PIN#17(Vendor[1])
	vendor0 = gpio_get_value(176); //LCD_ID PIN#15(Vendor[0])

	//Before DVT
	if (quanta_get_mbid() < 4) {
		if((vendor1 == 0) && (vendor0 == 0)) {
			printk("***************************************************\n");
			printk("******************  Panel of LG  ******************\n");
			printk("***************************************************\n");
			result |= spi_send(boxer_spi_device, 0x00, 0x21);
			result |= spi_send(boxer_spi_device, 0x00, 0xa5);
			result |= spi_send(boxer_spi_device, 0x01, 0x30);
			result |= spi_send(boxer_spi_device, 0x02, 0x40);
			result |= spi_send(boxer_spi_device, 0x0e, 0x5f);
			result |= spi_send(boxer_spi_device, 0x0f, 0xa4);
			result |= spi_send(boxer_spi_device, 0x0d, 0x00);
			result |= spi_send(boxer_spi_device, 0x02, 0x43);
			result |= spi_send(boxer_spi_device, 0x0a, 0x28);
			result |= spi_send(boxer_spi_device, 0x10, 0x41);
			result |= spi_send(boxer_spi_device, 0x00, 0xad);
		}
		else if((vendor1 == 1) && (vendor0 == 0))
		{
			printk("***************************************************\n");
			printk("******************  Panel of HYDIS  ***************\n");
			printk("***************************************************\n");
			result |= spi_send(boxer_spi_device, 0x00, 0x29);
			result |= spi_send(boxer_spi_device, 0x00, 0x25);
			result |= spi_send(boxer_spi_device, 0x01, 0x30);
			result |= spi_send(boxer_spi_device, 0x02, 0x40);
			result |= spi_send(boxer_spi_device, 0x0e, 0x5f);
			result |= spi_send(boxer_spi_device, 0x0f, 0xa4);
			result |= spi_send(boxer_spi_device, 0x0d, 0x01);
			result |= spi_send(boxer_spi_device, 0x00, 0x2d);
		}
		else if((vendor1 == 0) && (vendor0 == 1))
		{
			printk("****************************************************\n");
			printk("******************  Panel of CMO  ******************\n");
			printk("****************************************************\n");
			result |= spi_send(boxer_spi_device, 0x00, 0xad);
			result |= spi_send(boxer_spi_device, 0x01, 0x10);
			result |= spi_send(boxer_spi_device, 0x02, 0x40);
			result |= spi_send(boxer_spi_device, 0x03, 0x04);
		}
		else
		{
			printk("********************************************************************************\n");
			printk("******************  undefined panel, use default LGD settings ******************\n");
			printk("********************************************************************************\n");
			result |= spi_send(boxer_spi_device, 0x00, 0x21);
			result |= spi_send(boxer_spi_device, 0x00, 0xa5);
			result |= spi_send(boxer_spi_device, 0x01, 0x30);
			result |= spi_send(boxer_spi_device, 0x02, 0x40);
			result |= spi_send(boxer_spi_device, 0x0e, 0x5f);
			result |= spi_send(boxer_spi_device, 0x0f, 0xa4);
			result |= spi_send(boxer_spi_device, 0x0d, 0x00);
			result |= spi_send(boxer_spi_device, 0x02, 0x43);
			result |= spi_send(boxer_spi_device, 0x0a, 0x28);
			result |= spi_send(boxer_spi_device, 0x10, 0x41);
			result |= spi_send(boxer_spi_device, 0x00, 0xad);
		}
	} else {
		//After DVT
		if((vendor1 == 0) && (vendor0 == 0))
		{
			printk("***************************************************\n");
			printk("******************  Panel of LG  ******************\n");
			printk("***************************************************\n");
			result |= spi_send(boxer_spi_device, 0x00, 0x21);
			result |= spi_send(boxer_spi_device, 0x00, 0xa5);
			result |= spi_send(boxer_spi_device, 0x01, 0x30);
			result |= spi_send(boxer_spi_device, 0x02, 0x40);
			result |= spi_send(boxer_spi_device, 0x0e, 0x5f);
			result |= spi_send(boxer_spi_device, 0x0f, 0xa4);
			result |= spi_send(boxer_spi_device, 0x0d, 0x00);
			result |= spi_send(boxer_spi_device, 0x02, 0x43);
			result |= spi_send(boxer_spi_device, 0x0a, 0x28);
			result |= spi_send(boxer_spi_device, 0x10, 0x41);
			result |= spi_send(boxer_spi_device, 0x00, 0xad);
		}
		else if((vendor1 == 0) && (vendor0 == 1))
		{
			printk("***************************************************\n");
			printk("******************  Panel of HYDIS  ***************\n");
			printk("***************************************************\n");
			result |= spi_send(boxer_spi_device, 0x00, 0x29);
			result |= spi_send(boxer_spi_device, 0x00, 0x25);
			result |= spi_send(boxer_spi_device, 0x01, 0x30);
			result |= spi_send(boxer_spi_device, 0x02, 0x40);
			result |= spi_send(boxer_spi_device, 0x0e, 0x5f);
			result |= spi_send(boxer_spi_device, 0x0f, 0xa4);
			result |= spi_send(boxer_spi_device, 0x0d, 0x01);
			result |= spi_send(boxer_spi_device, 0x00, 0x2d);
		}
		else
		{
			printk("********************************************************************************\n");
			printk("******************  undefined panel, use default LGD settings ******************\n");
			printk("********************************************************************************\n");
			result |= spi_send(boxer_spi_device, 0x00, 0x21);
			result |= spi_send(boxer_spi_device, 0x00, 0xa5);
			result |= spi_send(boxer_spi_device, 0x01, 0x30);
			result |= spi_send(boxer_spi_device, 0x02, 0x40);
			result |= spi_send(boxer_spi_device, 0x0e, 0x5f);
			result |= spi_send(boxer_spi_device, 0x0f, 0xa4);
			result |= spi_send(boxer_spi_device, 0x0d, 0x00);
			result |= spi_send(boxer_spi_device, 0x02, 0x43);
			result |= spi_send(boxer_spi_device, 0x0a, 0x28);
			result |= spi_send(boxer_spi_device, 0x10, 0x41);
			result |= spi_send(boxer_spi_device, 0x00, 0xad);
		}
	}
}

static void boxer_panel_work_func(struct work_struct *work)
{
	if (!regulator_is_enabled(boxer_panel_regulator)){
		regulator_enable(boxer_panel_regulator);
	}

	msleep(LCD_RST_DELAY);

	boxer_spi_device->mode = SPI_MODE_0;
	boxer_spi_device->bits_per_word = 16;
	spi_setup(boxer_spi_device);

	boxer_init_panel();

	msleep(LCD_INIT_DELAY);

	if (boxer_panel_dssdev->platform_enable){
		boxer_panel_dssdev->platform_enable(boxer_panel_dssdev);
	}
}

static DECLARE_WORK(boxer_panel_work, boxer_panel_work_func);

int boxer_panel_power_on(void)
{
	printk(KERN_INFO " >>>> BOXER POWER ON");
	// int vendor0=1, vendor1=1;
	gpio_direction_output(47, 0);
	gpio_direction_output(37, 0);
	mdelay(LCD_RST_DELAY);

	gpio_direction_output(47, 1);
	mdelay(LCD_RST_DELAY);

	boxer_init_panel();

	mdelay(LCD_INIT_DELAY);

	gpio_direction_output(37, 1);
	return 0;
}

static void boxer_get_resolution(struct omap_dss_device *dssdev,
				 u16 *xres, u16 *yres)
{

	*xres = dssdev->panel.timings.x_res;
	*yres = dssdev->panel.timings.y_res;
}

static int boxer_panel_probe(struct omap_dss_device *dssdev)
{
	printk(KERN_INFO " boxer : %s called , line %d\n", __FUNCTION__ , __LINE__);

	omap_writel(0x00020000,0x4a1005cc); //PCLK impedance
	gpio_request(175, "LCD_VENDOR0");
	gpio_request(176, "LCD_VENDOR1");
	gpio_direction_input(175);
	gpio_direction_input(176);
	//gpio_request(47, "vdd_lcd");
	gpio_request(37, "OMAP_RGB_SHTDN");

	//Already done in uboot
	boxer_panel_power_on();
	return 0;
}

static void boxer_panel_remove(struct omap_dss_device *dssdev)
{
}

static int boxer_panel_start(struct omap_dss_device *dssdev)
{
	int r = 0;

	printk(KERN_INFO " boxer : %s called , line %d\n", __FUNCTION__ , __LINE__);

	if (dssdev->state == OMAP_DSS_DISPLAY_ACTIVE)
		return 0;

	if (atomic_add_unless(&boxer_panel_is_enabled, 1, 1)) {
		boxer_panel_dssdev = dssdev;
		queue_work(boxer_panel_wq, &boxer_panel_work);
	}

	r = omapdss_dpi_display_enable(dssdev);
	printk(KERN_INFO " omapdss_dpi_display_enable == %d", r);
	if (r) goto err0;

	msleep(LCD_RST_DELAY);

	return 0;
err0:
	return r;
}

static void boxer_panel_stop(struct omap_dss_device *dssdev)
{
	printk(KERN_INFO " boxer : %s called , line %d\n", __FUNCTION__ , __LINE__);

	if (dssdev->state != OMAP_DSS_DISPLAY_ACTIVE)
		return;

	if (atomic_dec_and_test(&boxer_panel_is_enabled)) {
		cancel_work_sync(&boxer_panel_work);

		if (dssdev->platform_disable){
			dssdev->platform_disable(dssdev);
		}
		if (regulator_is_enabled(boxer_panel_regulator)){
			//regulator_disable(boxer_panel_regulator);
		}
	} else {
		printk(KERN_WARNING "%s: attempting to disable panel twice!\n",
		       __func__);
		WARN_ON(1);
	}

	omapdss_dpi_display_disable(dssdev);
}

static int boxer_panel_enable(struct omap_dss_device *dssdev)
{
	int r = 0;

	printk(KERN_INFO " boxer : %s called , line %d\n", __FUNCTION__ , __LINE__);
	r = boxer_panel_start(dssdev);
	if (r)
		return r;

	dssdev->state = OMAP_DSS_DISPLAY_ACTIVE;

	return 0;
}

static void boxer_panel_disable(struct omap_dss_device *dssdev)
{
	printk(KERN_INFO " boxer : %s called , line %d\n", __FUNCTION__ , __LINE__);
	boxer_panel_stop(dssdev);
	dssdev->state = OMAP_DSS_DISPLAY_DISABLED;
}

extern int Light_Sensor_Exist;

static int boxer_panel_suspend(struct omap_dss_device *dssdev)
{
	printk(KERN_INFO " boxer : %s called , line %d\n", __FUNCTION__ , __LINE__);
    	gpio_direction_output(37, 0);
	msleep(LCD_RST_DELAY);
	gpio_direction_output(47, 0);
	boxer_panel_stop(dssdev);
	dssdev->state = OMAP_DSS_DISPLAY_SUSPENDED;
	return 0;
}

static int boxer_panel_resume(struct omap_dss_device *dssdev)
{
	int r = 0;
	printk(KERN_INFO " boxer : %s called , line %d\n", __FUNCTION__ , __LINE__);

	boxer_panel_start(dssdev);
	dssdev->state = OMAP_DSS_DISPLAY_ACTIVE;
	r = boxer_panel_power_on();
	return r;
}

static void boxer_panel_set_timings(struct omap_dss_device *dssdev,
		struct omap_video_timings *timings)
{
	dpi_set_timings(dssdev, timings);
}

static void boxer_panel_get_timings(struct omap_dss_device *dssdev,
                        struct omap_video_timings *timings)
{
        *timings = dssdev->panel.timings;
}

static int boxer_panel_check_timings(struct omap_dss_device *dssdev,
		struct omap_video_timings *timings)
{
	return dpi_check_timings(dssdev, timings);
}

static struct omap_dss_driver boxer_driver = {
	.probe		= boxer_panel_probe,
	.remove		= boxer_panel_remove,

	.enable		= boxer_panel_enable,
	.disable	= boxer_panel_disable,
	.suspend	= boxer_panel_suspend,
	.resume		= boxer_panel_resume,

	.get_resolution	= boxer_get_resolution,

	.get_recommended_bpp = omapdss_default_get_recommended_bpp,

	.set_timings	= boxer_panel_set_timings,
	.get_timings	= boxer_panel_get_timings,
	.check_timings	= boxer_panel_check_timings,

	.driver		= {
		.name	= "otter1_panel_drv",
		.owner	= THIS_MODULE,
	},
};

static int boxer_spi_probe(struct spi_device *spi)
{
	int ret = 0;

	boxer_panel_regulator = regulator_get(&spi->dev, "vlcd");
	if (!regulator_is_enabled(boxer_panel_regulator)) {
		if (g_ft_i2c_adapter) {
			i2c_lock_adapter(g_ft_i2c_adapter);
		}
		ret = regulator_enable(boxer_panel_regulator);
		mdelay(2);
		if (g_ft_i2c_adapter) {
			i2c_unlock_adapter(g_ft_i2c_adapter);
		}
		if (ret) {
			regulator_put(boxer_panel_regulator);
			goto out;
		}
	}

	spi->mode = SPI_MODE_0;
	spi->bits_per_word = 16;
	spi->chip_select=0;
	boxer_spi_device = spi;
	spi_setup(boxer_spi_device);
	boxer_init_panel();

	sysfs_create_group(&spi->dev.kobj, &otter1_panel_attribute_group);

	return omap_dss_register_driver(&boxer_driver);
out:
	return ret;
}

static int boxer_spi_remove(struct spi_device *spi)
{
	printk(KERN_INFO " boxer : %s called , line %d\n", __FUNCTION__ , __LINE__);
	sysfs_remove_group(&spi->dev.kobj, &otter1_panel_attribute_group);
	omap_dss_unregister_driver(&boxer_driver);
	regulator_disable(boxer_panel_regulator);
	regulator_put(boxer_panel_regulator);
	return 0;
}


static struct spi_driver boxer_spi_driver = {
	.probe		= boxer_spi_probe,
	.remove		= __devexit_p(boxer_spi_remove),
	.driver		= {
		.name	= "otter1_disp_spi",
		.bus	= &spi_bus_type,
		.owner	= THIS_MODULE,
	},
};

static int __init boxer_lcd_init(void)
{
	int ret = 0;

#if 0
	twl_i2c_write_u8(TWL_MODULE_PWM, 0xFF, LED_PWM2ON);
	twl_i2c_write_u8(TWL_MODULE_PWM, 0x7F, LED_PWM2OFF);
	//twl_i2c_write_u8(TWL6030_MODULE_ID1, 0x30, TWL6030_TOGGLE3);
	twl_i2c_write_u8(TWL6030_MODULE_ID1, 0x08, TWL6030_TOGGLE3);
	twl_i2c_write_u8(TWL6030_MODULE_ID1, 0x38, TWL6030_TOGGLE3);
#endif
	boxer_panel_wq = create_singlethread_workqueue("boxer-panel-wq");

	if (IS_ERR(boxer_panel_regulator)) {
		printk(KERN_ERR "Unable to get vlcd regulator, reason: %ld!\n",
		       IS_ERR(boxer_panel_regulator));
		ret = -ENODEV;
		goto out;
	}

	return spi_register_driver(&boxer_spi_driver);
out:
	return ret;
}

static void __exit boxer_lcd_exit(void)
{
	spi_unregister_driver(&boxer_spi_driver);
}

module_init(boxer_lcd_init);
module_exit(boxer_lcd_exit);

MODULE_LICENSE("GPL");

