/*
 * NEC panel support
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
#include <plat/gpio.h>
#include <plat/mux.h>
#include <asm/mach-types.h>
#include <plat/control.h>
#include <plat/display.h>

#define LCD_XRES		800
#define LCD_YRES		480

#define LCD_PIXCLOCK_MIN	21800 /* NEC MIN PIX Clock is 21.8MHz */
#define LCD_PIXCLOCK_TYP	23800 /* Typical PIX clock is 23.8MHz */
#define LCD_PIXCLOCK_MAX	25700 /* Maximum is 25.7MHz */

/* Current Pixel clock */
#define LCD_PIXEL_CLOCK		LCD_PIXCLOCK_MIN

/* NEC NL8048HL11-01B  Manual
 * defines HFB, HSW, HBP, VFP, VSW, VBP as shown below
 */

static struct omap_video_timings nec_8048_panel_timings = {
	/* 800 x 480 @ 60 Hz  Reduced blanking VESA CVT 0.31M3-R */
	.x_res          = LCD_XRES,
	.y_res          = LCD_YRES,
	.pixel_clock    = LCD_PIXEL_CLOCK,
	.hfp            = 6,
	.hsw            = 1,
	.hbp            = 4,
	.vfp            = 3,
	.vsw            = 1,
	.vbp            = 4,
};

static int nec_8048_panel_probe(struct omap_dss_device *dssdev)
{
	dssdev->panel.config = OMAP_DSS_LCD_TFT | OMAP_DSS_LCD_IVS |
				OMAP_DSS_LCD_IHS | OMAP_DSS_LCD_RF |
				OMAP_DSS_LCD_ONOFF;
	dssdev->panel.timings = nec_8048_panel_timings;
	dssdev->ctrl.pixel_size = 16;

	return 0;
}

static void nec_8048_panel_remove(struct omap_dss_device *dssdev)
{
}

static int nec_8048_panel_enable(struct omap_dss_device *dssdev)
{
	int r = 0;

	r = omapdss_dpi_display_enable(dssdev);
	if (r)
		goto err0;

	/* Delay recommended by panel DATASHEET */
	mdelay(4);
	if (dssdev->platform_enable) {
		r = dssdev->platform_enable(dssdev);
		if (r)
			goto err1;
	}
	return r;
err1:
	omapdss_dpi_display_disable(dssdev);
err0:
	return r;
}

static void nec_8048_panel_disable(struct omap_dss_device *dssdev)
{
	if (dssdev->platform_disable)
		dssdev->platform_disable(dssdev);
	/* Delay recommended by panel DATASHEET */
	mdelay(4);

	omapdss_dpi_display_disable(dssdev);
}

static int nec_8048_panel_suspend(struct omap_dss_device *dssdev)
{
	nec_8048_panel_disable(dssdev);
	return 0;
}

static int nec_8048_panel_resume(struct omap_dss_device *dssdev)
{
	return nec_8048_panel_enable(dssdev);
}

static void nec_8048_panel_get_timings(struct omap_dss_device *dssdev,
				struct omap_video_timings *timings)
{
	*timings = dssdev->panel.timings;
}

static struct omap_dss_driver nec_8048_driver = {
	.probe          = nec_8048_panel_probe,
	.remove         = nec_8048_panel_remove,
	.enable         = nec_8048_panel_enable,
	.disable        = nec_8048_panel_disable,
	.suspend        = nec_8048_panel_suspend,
	.resume         = nec_8048_panel_resume,
	.get_recommended_bpp = omapdss_default_get_recommended_bpp,
	.get_timings    = nec_8048_panel_get_timings,

	.driver		= {
		.name	= "NEC_8048_panel",
		.owner	= THIS_MODULE,
	},
};

static int
spi_send(struct spi_device *spi, unsigned char reg_addr, unsigned char reg_data)
{
	int ret = 0;
	unsigned int cmd = 0;
	unsigned int data = 0;

	cmd = 0x0000 | reg_addr; /* register address write */
	data = 0x0100 | reg_data ; /* register data write */
	data = (cmd << 16) | data;
	if (spi_write(spi, (unsigned char *)&data, 4))
		printk(KERN_WARNING "error in spi_write %x\n", data);

	/* Delay part of init seqence recommended by panel DATASHEET */
	udelay(10);
	return ret;
}


static int init_nec_8048_wvga_lcd(struct spi_device *spi)
{
	/* Initialization Sequence */
	spi_send(spi, 3, 0x01);    /* R3 = 01h */
	spi_send(spi, 0, 0x00);    /* R0 = 00h */
	spi_send(spi, 1, 0x01);    /* R1 = 0x01 (normal), 0x03 (reversed) */
	spi_send(spi, 4, 0x00);    /* R4 = 00h */
	spi_send(spi, 5, 0x14);    /* R5 = 14h */
	spi_send(spi, 6, 0x24);    /* R6 = 24h */
	spi_send(spi, 16, 0xD7);   /* R16 = D7h */
	spi_send(spi, 17, 0x00);   /* R17 = 00h */
	spi_send(spi, 18, 0x00);   /* R18 = 00h */
	spi_send(spi, 19, 0x55);   /* R19 = 55h */
	spi_send(spi, 20, 0x01);   /* R20 = 01h */
	spi_send(spi, 21, 0x70);   /* R21 = 70h */
	spi_send(spi, 22, 0x1E);   /* R22 = 1Eh */
	spi_send(spi, 23, 0x25);   /* R23 = 25h */
	spi_send(spi, 24, 0x25);   /* R24 = 25h */
	spi_send(spi, 25, 0x02);   /* R25 = 02h */
	spi_send(spi, 26, 0x02);   /* R26 = 02h */
	spi_send(spi, 27, 0xA0);   /* R27 = A0h */
	spi_send(spi, 32, 0x2F);   /* R32 = 2Fh */
	spi_send(spi, 33, 0x0F);   /* R33 = 0Fh */
	spi_send(spi, 34, 0x0F);   /* R34 = 0Fh */
	spi_send(spi, 35, 0x0F);   /* R35 = 0Fh */
	spi_send(spi, 36, 0x0F);   /* R36 = 0Fh */
	spi_send(spi, 37, 0x0F);   /* R37 = 0Fh */
	spi_send(spi, 38, 0x0F);   /* R38 = 0Fh */
	spi_send(spi, 39, 0x00);   /* R39 = 00h */
	spi_send(spi, 40, 0x02);   /* R40 = 02h */
	spi_send(spi, 41, 0x02);   /* R41 = 02h */
	spi_send(spi, 42, 0x02);   /* R42 = 02h */
	spi_send(spi, 43, 0x0F);   /* R43 = 0Fh */
	spi_send(spi, 44, 0x0F);   /* R44 = 0Fh */
	spi_send(spi, 45, 0x0F);   /* R45 = 0Fh */
	spi_send(spi, 46, 0x0F);   /* R46 = 0Fh */
	spi_send(spi, 47, 0x0F);   /* R47 = 0Fh */
	spi_send(spi, 48, 0x0F);   /* R48 = 0Fh */
	spi_send(spi, 49, 0x0F);   /* R49 = 0Fh */
	spi_send(spi, 50, 0x00);   /* R50 = 00h */
	spi_send(spi, 51, 0x02);   /* R51 = 02h */
	spi_send(spi, 52, 0x02);   /* R52 = 02h */
	spi_send(spi, 53, 0x02);   /* R53 = 02h */
	spi_send(spi, 80, 0x0C);   /* R80 = 0Ch */
	spi_send(spi, 83, 0x42);   /* R83 = 42h */
	spi_send(spi, 84, 0x42);   /* R84 = 42h */
	spi_send(spi, 85, 0x41);   /* R85 = 41h */
	spi_send(spi, 86, 0x14);   /* R86 = 14h */
	spi_send(spi, 89, 0x88);   /* R89 = 88h */
	spi_send(spi, 90, 0x01);   /* R90 = 01h */
	spi_send(spi, 91, 0x00);   /* R91 = 00h */
	spi_send(spi, 92, 0x02);   /* R92 = 02h */
	spi_send(spi, 93, 0x0C);   /* R93 = 0Ch */
	spi_send(spi, 94, 0x1C);   /* R94 = 1Ch */
	spi_send(spi, 95, 0x27);   /* R95 = 27h */
	spi_send(spi, 98, 0x49);   /* R98 = 49h */
	spi_send(spi, 99, 0x27);   /* R99 = 27h */
	spi_send(spi, 102, 0x76);  /* R102 = 76h */
	spi_send(spi, 103, 0x27);  /* R103 = 27h */
	spi_send(spi, 112, 0x01);  /* R112 = 01h */
	spi_send(spi, 113, 0x0E);  /* R113 = 0Eh */
	spi_send(spi, 114, 0x02);  /* R114 = 02h */
	spi_send(spi, 115, 0x0C);  /* R115 = 0Ch */
	spi_send(spi, 118, 0x0C);  /* R118 = 0Ch */
	spi_send(spi, 121, 0x30); /* R121 = 0x30 (normal), 0x10 (reversed) */
	spi_send(spi, 130, 0x00);  /* R130 = 00h */
	spi_send(spi, 131, 0x00);  /* R131 = 00h */
	spi_send(spi, 132, 0xFC);  /* R132 = FCh */
	spi_send(spi, 134, 0x00);  /* R134 = 00h */
	spi_send(spi, 136, 0x00);  /* R136 = 00h */
	spi_send(spi, 138, 0x00);  /* R138 = 00h */
	spi_send(spi, 139, 0x00);  /* R139 = 00h */
	spi_send(spi, 140, 0x00);  /* R140 = 00h */
	spi_send(spi, 141, 0xFC);  /* R141 = FCh */
	spi_send(spi, 143, 0x00);  /* R143 = 00h */
	spi_send(spi, 145, 0x00);  /* R145 = 00h */
	spi_send(spi, 147, 0x00);  /* R147 = 00h */
	spi_send(spi, 148, 0x00);  /* R148 = 00h */
	spi_send(spi, 149, 0x00);  /* R149 = 00h */
	spi_send(spi, 150, 0xFC);  /* R150 = FCh */
	spi_send(spi, 152, 0x00);  /* R152 = 00h */
	spi_send(spi, 154, 0x00);  /* R154 = 00h */
	spi_send(spi, 156, 0x00);  /* R156 = 00h */
	spi_send(spi, 157, 0x00);  /* R157 = 00h */
	udelay(20);
	spi_send(spi, 2, 0x00);    /* R2 = 00h */
	return 0;
}

static int nec_8048_spi_probe(struct spi_device *spi)
{
	spi->mode = SPI_MODE_0;
	spi->bits_per_word = 32;
	spi_setup(spi);

	init_nec_8048_wvga_lcd(spi);

	omap_dss_register_driver(&nec_8048_driver);
	return 0;
}

static int nec_8048_spi_remove(struct spi_device *spi)
{
	omap_dss_unregister_driver(&nec_8048_driver);

	return 0;
}

static int nec_8048_spi_suspend(struct spi_device *spi, pm_message_t mesg)
{
	spi_send(spi, 2, 0x01);  /* R2 = 01h */
	mdelay(40);

	return 0;
}

static int nec_8048_spi_resume(struct spi_device *spi)
{
	/* reinitialize the panel */
	spi_setup(spi);
	spi_send(spi, 2, 0x00);  /* R2 = 00h */
	init_nec_8048_wvga_lcd(spi);

	return 0;
}

static struct spi_driver nec_8048_spi_driver = {
	.probe           = nec_8048_spi_probe,
	.remove	= __devexit_p(nec_8048_spi_remove),
	.suspend         = nec_8048_spi_suspend,
	.resume          = nec_8048_spi_resume,
	.driver         = {
		.name   = "nec_8048_spi",
		.bus    = &spi_bus_type,
		.owner  = THIS_MODULE,
	},
};

static int __init nec_8048_lcd_init(void)
{
	return spi_register_driver(&nec_8048_spi_driver);
}

static void __exit nec_8048_lcd_exit(void)
{
	return spi_unregister_driver(&nec_8048_spi_driver);
}

module_init(nec_8048_lcd_init);
module_exit(nec_8048_lcd_exit);
MODULE_LICENSE("GPL");

