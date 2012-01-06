/*
 * Otter1 panel support
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
#include <plat/display.h>
#include <linux/gpio.h>

static struct spi_device *otter1_spi;
static struct mutex lock_power_on;
static struct mutex lock_panel;
static struct omap_video_timings otter1_panel_timings = {
	.x_res          = 1024,
	.y_res          = 600,
	.pixel_clock    = 46000, /* in kHz */
	.hfp            = 160,   /* HFP fix 160 */
	.hsw            = 10,    /* HSW = 1~140 */
	.hbp            = 150,   /* HSW + HBP = 160 */
	.vfp            = 12,    /* VFP fix 12 */
	.vsw            = 3,     /* VSW = 1~20 */
	.vbp            = 20,    /* VSW + VBP = 23 */
};
extern u8 quanta_get_mbid(void);

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
static ssize_t set_cabc (struct device *dev, struct device_attribute *attr, const char *buffer, size_t count)
{
	int mode = 0;
	sscanf(buffer, "%d", &mode);
	printk("the cabc value is %d\n", mode);
	switch(mode)
	{
	case 0:
		printk("set the cabc value to 0b00\n");
		spi_send(otter1_spi, 0x01, 0x30);
		break;
	case 1:
		printk("set the cabc value to 0b01\n");
		spi_send(otter1_spi, 0x01, 0x70);
		break;
	case 2:
		printk("set the cabc value to 0b10\n");
		spi_send(otter1_spi, 0x01, 0xb0);
		break;
	case 3:
		printk("set the cabc value to 0b11\n");
		spi_send(otter1_spi, 0x01, 0xf0);
		break;
	default:
		printk("set the cabc value to 0b00\n");
		spi_send(otter1_spi, 0x01, 0x30);
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

static void otter1_get_timings(struct omap_dss_device *dssdev,
                        struct omap_video_timings *timings)
{
        *timings = dssdev->panel.timings;
}

int otter1_panel_power_on(void)
{
	int vendor0=1, vendor1=1;
	int result = 0;
	mutex_lock(&lock_power_on);
    gpio_direction_output(47, 0);		
    gpio_direction_output(37, 0);
    gpio_direction_output(47, 1);
    mdelay(1);

	vendor1 = gpio_get_value(175); //LCD_ID PIN#17(Vendor[1])
	vendor0 = gpio_get_value(176); //LCD_ID PIN#15(Vendor[0])

    //Before DVT
    if (quanta_get_mbid() < 4) {
        if((vendor1 == 0) && (vendor0 == 0))
        {
            //printk("***************************************************\n");
            //printk("******************  Panel of LG  ******************\n");
            //printk("***************************************************\n");
            result |= spi_send(otter1_spi, 0x00, 0x21);
            result |= spi_send(otter1_spi, 0x00, 0xa5);
            result |= spi_send(otter1_spi, 0x01, 0x30);
            result |= spi_send(otter1_spi, 0x02, 0x40);
            result |= spi_send(otter1_spi, 0x0e, 0x5f);
            result |= spi_send(otter1_spi, 0x0f, 0xa4);
            result |= spi_send(otter1_spi, 0x0d, 0x00);
            result |= spi_send(otter1_spi, 0x02, 0x43);
            result |= spi_send(otter1_spi, 0x0a, 0x28);
            result |= spi_send(otter1_spi, 0x10, 0x41);
            result |= spi_send(otter1_spi, 0x00, 0xad);
        }
        else if((vendor1 == 1) && (vendor0 == 0))
        {
            printk("***************************************************\n");
            printk("******************  Panel of HYDIS  ***************\n");
            printk("***************************************************\n");
            result |= spi_send(otter1_spi, 0x00, 0x29);
            result |= spi_send(otter1_spi, 0x00, 0x25);
            result |= spi_send(otter1_spi, 0x01, 0x30);
            result |= spi_send(otter1_spi, 0x02, 0x40);
            result |= spi_send(otter1_spi, 0x0e, 0x5f);
            result |= spi_send(otter1_spi, 0x0f, 0xa4);
            result |= spi_send(otter1_spi, 0x0d, 0x01);
            result |= spi_send(otter1_spi, 0x00, 0x2d);
        }
        else if((vendor1 == 0) && (vendor0 == 1))
        {
            printk("****************************************************\n");
            printk("******************  Panel of CMO  ******************\n");
            printk("****************************************************\n");
            result |= spi_send(otter1_spi, 0x00, 0xad);
            result |= spi_send(otter1_spi, 0x01, 0x10);
            result |= spi_send(otter1_spi, 0x02, 0x40);
            result |= spi_send(otter1_spi, 0x03, 0x04);
        }
        else
        {
            printk("********************************************************************************\n");
            printk("******************  undefined panel, use default LGD settings ******************\n");
            printk("********************************************************************************\n");
            result |= spi_send(otter1_spi, 0x00, 0x21);
            result |= spi_send(otter1_spi, 0x00, 0xa5);
            result |= spi_send(otter1_spi, 0x01, 0x30);
            result |= spi_send(otter1_spi, 0x02, 0x40);
            result |= spi_send(otter1_spi, 0x0e, 0x5f);
            result |= spi_send(otter1_spi, 0x0f, 0xa4);
            result |= spi_send(otter1_spi, 0x0d, 0x00);
            result |= spi_send(otter1_spi, 0x02, 0x43);
            result |= spi_send(otter1_spi, 0x0a, 0x28);
            result |= spi_send(otter1_spi, 0x10, 0x41);
            result |= spi_send(otter1_spi, 0x00, 0xad);
        }
    } else {
        //After DVT
        if((vendor1 == 0) && (vendor0 == 0))
        {
            //printk("***************************************************\n");
            //printk("******************  Panel of LG  ******************\n");
            //printk("***************************************************\n");
            result |= spi_send(otter1_spi, 0x00, 0x21);
            result |= spi_send(otter1_spi, 0x00, 0xa5);
            result |= spi_send(otter1_spi, 0x01, 0x30);
            result |= spi_send(otter1_spi, 0x02, 0x40);
            result |= spi_send(otter1_spi, 0x0e, 0x5f);
            result |= spi_send(otter1_spi, 0x0f, 0xa4);
            result |= spi_send(otter1_spi, 0x0d, 0x00);
            result |= spi_send(otter1_spi, 0x02, 0x43);
            result |= spi_send(otter1_spi, 0x0a, 0x28);
            result |= spi_send(otter1_spi, 0x10, 0x41);
            result |= spi_send(otter1_spi, 0x00, 0xad);
        }
        else if((vendor1 == 0) && (vendor0 == 1))
        {
            printk("***************************************************\n");
            printk("******************  Panel of HYDIS  ***************\n");
            printk("***************************************************\n");
            result |= spi_send(otter1_spi, 0x00, 0x29);
            result |= spi_send(otter1_spi, 0x00, 0x25);
            result |= spi_send(otter1_spi, 0x01, 0x30);
            result |= spi_send(otter1_spi, 0x02, 0x40);
            result |= spi_send(otter1_spi, 0x0e, 0x5f);
            result |= spi_send(otter1_spi, 0x0f, 0xa4);
            result |= spi_send(otter1_spi, 0x0d, 0x01);
            result |= spi_send(otter1_spi, 0x00, 0x2d);
        }
        else
        {
            printk("********************************************************************************\n");
            printk("******************  undefined panel, use default LGD settings ******************\n");
            printk("********************************************************************************\n");
            result |= spi_send(otter1_spi, 0x00, 0x21);
            result |= spi_send(otter1_spi, 0x00, 0xa5);
            result |= spi_send(otter1_spi, 0x01, 0x30);
            result |= spi_send(otter1_spi, 0x02, 0x40);
            result |= spi_send(otter1_spi, 0x0e, 0x5f);
            result |= spi_send(otter1_spi, 0x0f, 0xa4);
            result |= spi_send(otter1_spi, 0x0d, 0x00);
            result |= spi_send(otter1_spi, 0x02, 0x43);
            result |= spi_send(otter1_spi, 0x0a, 0x28);
            result |= spi_send(otter1_spi, 0x10, 0x41);
            result |= spi_send(otter1_spi, 0x00, 0xad);
        }
    }
	//mdelay(1);

	gpio_direction_output(37, 1);
	mutex_unlock(&lock_power_on);
    return result;
}
EXPORT_SYMBOL(otter1_panel_power_on);

static int otter1_panel_probe(struct omap_dss_device *dssdev)
{
	printk(KERN_INFO "Otter1 LCD probe called\n");
	dssdev->panel.config	= OMAP_DSS_LCD_TFT | OMAP_DSS_LCD_IVS |
				  OMAP_DSS_LCD_IHS;
	dssdev->panel.timings	= otter1_panel_timings;

	omap_writel(0x00020000,0x4a1005cc); //PCLK impedance
	gpio_request(175, "LCD_VENDOR0");
	gpio_request(176, "LCD_VENDOR1");
	gpio_direction_input(175);
	gpio_direction_input(176);
    gpio_request(47, "3V_ENABLE");
    gpio_request(37, "OMAP_RGB_SHTDN");

    //Already done in uboot
    //otter1_panel_power_on();
	mutex_init(&lock_power_on);
	mutex_init(&lock_panel);
	return 0;
}

static void otter1_panel_remove(struct omap_dss_device *dssdev)
{
	mutex_destroy(&lock_power_on);
	mutex_destroy(&lock_panel);
}

static int otter1_panel_start(struct omap_dss_device *dssdev)
{
	int r = 0;

	if (dssdev->platform_enable) {
		r = dssdev->platform_enable(dssdev);
		if (r)
			return r;
	}

	r = omapdss_dpi_display_enable(dssdev);
	if (r && dssdev->platform_disable)
		dssdev->platform_disable(dssdev);

	return r;
}

static void otter1_panel_stop(struct omap_dss_device *dssdev)
{
	mutex_lock(&lock_panel);

	omapdss_dpi_display_disable(dssdev);

	if (dssdev->platform_disable)
		dssdev->platform_disable(dssdev);

	mutex_unlock(&lock_panel);
}

static int otter1_panel_enable(struct omap_dss_device *dssdev)
{
	if (dssdev->state != OMAP_DSS_DISPLAY_DISABLED)
		return -EINVAL;

	return otter1_panel_start(dssdev);
}

static void otter1_panel_disable(struct omap_dss_device *dssdev)
{
	otter1_panel_stop(dssdev);
	dssdev->state = OMAP_DSS_DISPLAY_DISABLED;
}

extern int Light_Sensor_Exist;

static int otter1_panel_suspend(struct omap_dss_device *dssdev)
{
	printk(KERN_INFO "!!!!!!!%s state:%d!!!!!!!!!!\n",__func__, dssdev->state);
#if 1 //RGB to LVDS SHTDN pin off
    	gpio_direction_output(37, 0);
#endif
mdelay(1);
    //printk("%s Light_Sensor_Exist = %d\n", __func__, Light_Sensor_Exist);
//#if 1 //3V LDO off when light sensor does not exist
//if(Light_Sensor_Exist == 0) {
	gpio_direction_output(47, 0);	
//}
//#endif
//gpio_direction_output(175, 0);
//gpio_direction_output(176, 0);
	otter1_panel_stop(dssdev);
	dssdev->state = OMAP_DSS_DISPLAY_SUSPENDED;
	return 0;
}

static int otter1_panel_resume(struct omap_dss_device *dssdev)
{
	int result = 0;
	printk(KERN_INFO "!!!!!!!%s state:%d!!!!!!!!!!\n",__func__, dssdev->state);
	if (dssdev->state == OMAP_DSS_DISPLAY_ACTIVE) {
		printk(KERN_INFO "%s panel is already active",__func__);
		dump_stack ();
		return 0;
	}

	otter1_panel_start(dssdev);
        result = otter1_panel_power_on();
	if (result != 0) {
		printk (KERN_ERR "!!!!%s failed\n!!!", __func__);
	}
	else {
		dssdev->state = OMAP_DSS_DISPLAY_ACTIVE;
	}
	/* Return 0. If any issue then next power button press
	will power it up */
	return 0;
}

static struct omap_dss_driver otter1_driver = {
	.probe		= otter1_panel_probe,
	.remove		= otter1_panel_remove,
	.enable		= otter1_panel_enable,
	.disable	= otter1_panel_disable,
	.suspend	= otter1_panel_suspend,
	.resume		= otter1_panel_resume,
	.get_timings    = otter1_get_timings,
	.driver		= {
		.name	= "otter1_panel_drv",
		.owner	= THIS_MODULE,
	},
};

static int otter1_spi_probe(struct spi_device *spi)
{
	spi->mode = SPI_MODE_0;
	spi->bits_per_word = 16;
	spi_setup(spi);

	otter1_spi = spi;
	sysfs_create_group(&spi->dev.kobj, &otter1_panel_attribute_group);

	return omap_dss_register_driver(&otter1_driver);
}

static int otter1_spi_remove(struct spi_device *spi)
{
	omap_dss_unregister_driver(&otter1_driver);
	return 0;
}


static struct spi_driver otter1_spi_driver = {
	.probe		= otter1_spi_probe,
	.remove		= __devexit_p(otter1_spi_remove),
	.driver		= {
		.name	= "otter1_disp_spi",
		.bus	= &spi_bus_type,
		.owner	= THIS_MODULE,
	},
};

static int __init otter1_lcd_init(void)
{
	return spi_register_driver(&otter1_spi_driver);
}

static void __exit otter1_lcd_exit(void)
{
	spi_unregister_driver(&otter1_spi_driver);
}

module_init(otter1_lcd_init);
module_exit(otter1_lcd_exit);

