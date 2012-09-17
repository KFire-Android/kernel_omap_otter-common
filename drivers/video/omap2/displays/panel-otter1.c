/*
 * Generic DPI Panels support
 *
 * Copyright (C) 2010 Canonical Ltd.
 * Author: Bryan Wu <bryan.wu@canonical.com>
 *
 * LCD panel driver for Sharp LQ043T1DG01
 *
 * Copyright (C) 2009 Texas Instruments Inc
 * Author: Vaibhav Hiremath <hvaibhav@ti.com>
 *
 * LCD panel driver for Toppoly TDO35S
 *
 * Copyright (C) 2009 CompuLab, Ltd.
 * Author: Mike Rapoport <mike@compulab.co.il>
 *
 * Copyright (C) 2008 Nokia Corporation
 * Author: Tomi Valkeinen <tomi.valkeinen@nokia.com>
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
#include <linux/slab.h>
#include <video/omapdss.h>
#include <linux/spi/spi.h>

#include <video/omap-panel-generic-dpi.h>
#include <linux/gpio.h>

#define GPIO_3V_ENABLE		47
#define GPIO_OMAP_RGB_SHTDN	37

extern u8 quanta_get_panelid(void);
static unsigned int ignore_first_suspend = 1;
static struct spi_device *otter1_spi;
#if 0
static struct omap_video_timings otter1_panel_timings = {
	.x_res          = 1024,
	.y_res          = 600,
	.pixel_clock	= 51200,
	.hfp            = 160,   /* HFP fix 160 */
	.hsw            = 10,    /* HSW = 1~140 */
	.hbp            = 150,   /* HSW + HBP = 160 */
	.vfp            = 12,    /* VFP fix 12 */
	.vsw            = 3,     /* VSW = 1~20 */
	.vbp            = 20,    /* VSW + VBP = 23 */
};
#endif
static int spi_send(struct spi_device *spi,
		    unsigned char reg_addr, unsigned char reg_data)
{
	uint16_t msg = 0;
	msg = (reg_addr << 10) | reg_data;

	if (spi_write(spi, (unsigned char *) &msg, 2))
		printk(KERN_ERR "error in spi_write %x\n", msg);

	udelay(10);

	return 0;
}

static void lcd_power_on(void)
{
	gpio_direction_output(GPIO_3V_ENABLE, 0);
	gpio_direction_output(GPIO_OMAP_RGB_SHTDN, 0);
	gpio_direction_output(GPIO_3V_ENABLE, 1);
    mdelay(1);

    //After DVT
	if (quanta_get_panelid() == 0) {
		printk(KERN_INFO "***************************************************\n");
		printk(KERN_INFO "******************  Panel of LG  ******************\n");
		printk(KERN_INFO "***************************************************\n");
        spi_send(otter1_spi, 0x00, 0x21);
        spi_send(otter1_spi, 0x00, 0xa5);
        spi_send(otter1_spi, 0x01, 0x30);
        spi_send(otter1_spi, 0x02, 0x40);
        spi_send(otter1_spi, 0x0e, 0x5f);
        spi_send(otter1_spi, 0x0f, 0xa4);
        spi_send(otter1_spi, 0x0d, 0x00);
        spi_send(otter1_spi, 0x02, 0x43);
        spi_send(otter1_spi, 0x0a, 0x28);
        spi_send(otter1_spi, 0x10, 0x41);
        spi_send(otter1_spi, 0x00, 0xad);
	} else if (quanta_get_panelid() == 1) {
		printk(KERN_INFO "***************************************************\n");
		printk(KERN_INFO "******************  Panel of HYDIS  ***************\n");
		printk(KERN_INFO "***************************************************\n");
        spi_send(otter1_spi, 0x00, 0x29);
        spi_send(otter1_spi, 0x00, 0x25);
        spi_send(otter1_spi, 0x01, 0x30);
        spi_send(otter1_spi, 0x02, 0x40);
        spi_send(otter1_spi, 0x0e, 0x5f);
        spi_send(otter1_spi, 0x0f, 0xa4);
	spi_send(otter1_spi, 0x10, 0x41);
        spi_send(otter1_spi, 0x0d, 0x01);
        spi_send(otter1_spi, 0x00, 0x2d);
	} else if (quanta_get_panelid() == 2) {
		printk(KERN_INFO "***************************************************\n");
		printk(KERN_INFO "******************   Panel of CMO   ***************\n");
		printk(KERN_INFO "***************************************************\n");
		spi_send(otter1_spi, 0x00, 0xad);
		spi_send(otter1_spi, 0x01, 0x10);
		spi_send(otter1_spi, 0x02, 0x40);
		spi_send(otter1_spi, 0x03, 0x04);
	} else {
		printk(KERN_INFO "********************************************************************************\n");
		printk(KERN_INFO "******************  undefined panel, use default LGD settings ******************\n");
		printk(KERN_INFO "********************************************************************************\n");
        spi_send(otter1_spi, 0x00, 0x21);
        spi_send(otter1_spi, 0x00, 0xa5);
        spi_send(otter1_spi, 0x01, 0x30);
        spi_send(otter1_spi, 0x02, 0x40);
        spi_send(otter1_spi, 0x0e, 0x5f);
        spi_send(otter1_spi, 0x0f, 0xa4);
        spi_send(otter1_spi, 0x0d, 0x00);
        spi_send(otter1_spi, 0x02, 0x43);
        spi_send(otter1_spi, 0x0a, 0x28);
        spi_send(otter1_spi, 0x10, 0x41);
        spi_send(otter1_spi, 0x00, 0xad);
    }

	mdelay(1);
	gpio_direction_output(37, 1);
}

static void lcd_power_off(void)
{
	gpio_direction_output(GPIO_OMAP_RGB_SHTDN, 0);
	mdelay(1);
	gpio_direction_output(GPIO_3V_ENABLE, 0);
}

static int otter1_panel_power_on(struct omap_dss_device *dssdev)
{
	int r;

	if (dssdev->state == OMAP_DSS_DISPLAY_ACTIVE)
		return 0;

	r = omapdss_dpi_display_enable(dssdev);
	if (r)
		goto err0;

	if (dssdev->platform_enable) {
		r = dssdev->platform_enable(dssdev);
		if (r)
			goto err1;
	}

	return 0;
err1:
	omapdss_dpi_display_disable(dssdev);
err0:
	return r;
}

static void otter1_panel_power_off(struct omap_dss_device *dssdev)
{
	if (dssdev->state != OMAP_DSS_DISPLAY_ACTIVE)
		return;

	if (dssdev->platform_disable)
		dssdev->platform_disable(dssdev);

	omapdss_dpi_display_disable(dssdev);
}

static int otter1_panel_probe(struct omap_dss_device *dssdev)
{
	printk(KERN_INFO "Otter1 LCD probe called\n");
	dssdev->panel.config	= OMAP_DSS_LCD_TFT;
//	dssdev->panel.timings	= otter1_panel_timings;

	gpio_request(GPIO_3V_ENABLE, "3V_ENABLE");
	gpio_request(GPIO_OMAP_RGB_SHTDN, "OMAP_RGB_SHTDN");

	return 0;
}

static void otter1_panel_remove(struct omap_dss_device *dssdev)
{
}

static int otter1_panel_enable(struct omap_dss_device *dssdev)
{
	int r = 0;

	r = otter1_panel_power_on(dssdev);
	if (r)
		return r;

	dssdev->state = OMAP_DSS_DISPLAY_ACTIVE;

	return 0;
}

static void otter1_panel_disable(struct omap_dss_device *dssdev)
{
	otter1_panel_power_off(dssdev);

	dssdev->state = OMAP_DSS_DISPLAY_DISABLED;
}

static int otter1_panel_suspend(struct omap_dss_device *dssdev)
{
	if (ignore_first_suspend) {
		pr_info("not suspending now:%s\n", __func__);
		ignore_first_suspend = 0;
		return 0;
	}

	otter1_panel_power_off(dssdev);
	dssdev->state = OMAP_DSS_DISPLAY_SUSPENDED;
	lcd_power_off();
	return 0;
}

static int otter1_panel_resume(struct omap_dss_device *dssdev)
{
	int r = 0;
	if (ignore_first_suspend) {
		pr_info("not resuming now:%s\n", __func__);
		ignore_first_suspend = 0;
		return 0;
	}

	lcd_power_on();
	r = otter1_panel_power_on(dssdev);
	if (r)
		return r;
	dssdev->state = OMAP_DSS_DISPLAY_ACTIVE;

	return 0;
}

static void otter1_panel_set_timings(struct omap_dss_device *dssdev,
		struct omap_video_timings *timings)
{
	dpi_set_timings(dssdev, timings);
}

static void otter1_panel_get_timings(struct omap_dss_device *dssdev,
		struct omap_video_timings *timings)
{
	*timings = dssdev->panel.timings;
}

static int otter1_panel_check_timings(struct omap_dss_device *dssdev,
		struct omap_video_timings *timings)
{
	return dpi_check_timings(dssdev, timings);
}

static struct omap_dss_driver otter1_driver = {
	.probe		= otter1_panel_probe,
	.remove		= otter1_panel_remove,
	.enable		= otter1_panel_enable,
	.disable	= otter1_panel_disable,
	.suspend	= otter1_panel_suspend,
	.resume		= otter1_panel_resume,

	.set_timings	= otter1_panel_set_timings,
	.get_timings	= otter1_panel_get_timings,
	.check_timings	= otter1_panel_check_timings,

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


