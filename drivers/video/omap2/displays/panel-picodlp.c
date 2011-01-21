/*
 * pico_i2c_driver.c
 * pico DLP driver
 *
 * Copyright (C) 2009 Texas Instruments
 * Author: mythripk <mythripk@ti.com>
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
#include <linux/input.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <plat/display.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/firmware.h>
#include "panel-picodlp.h"
#include <linux/slab.h>


#define DRIVER_NAME       "pico_i2c"
/* How much data we can put into single write block */
#define MAX_I2C_WRITE_BLOCK_SIZE 32
#define PICO_MAJOR     1 /* 2 bits */
#define PICO_MINOR     1 /* 2 bits */


static struct omap_video_timings pico_ls_timings = {
	.x_res		= 864,
	.y_res		= 480,
	.pixel_clock	= 26600,
	.hsw		= 7,
	.hfp		= 11,
	.hbp		= 7,
	.vsw		= 2,
	.vfp		= 3,
	.vbp		= 14,
};

struct pico {
	struct i2c_client *client;
	struct mutex xfer_lock;
} *sd;

static int dlp_read_block(int reg, u8 *data, int len);
static int pico_i2c_write(int reg, u32 value);

static int dlp_write_block(int reg, const u8 *data, int len)
{
	unsigned char wb[MAX_I2C_WRITE_BLOCK_SIZE + 1];
	struct i2c_msg msg;
	int r;
	int i;

	if (len < 1 ||
	    len > MAX_I2C_WRITE_BLOCK_SIZE) {
		dev_info(&sd->client->dev, "too long syn_write_block len %d\n",
			 len);
		return -EIO;
	}

	wb[0] = reg & 0xff;

	for (i = 0; i < len; i++)
		wb[i + 1] = data[i];

	mutex_lock(&sd->xfer_lock);

	msg.addr = sd->client->addr;
	msg.flags = 0;
	msg.len = len + 1;
	msg.buf = wb;

	r = i2c_transfer(sd->client->adapter, &msg, 1);
	mutex_unlock(&sd->xfer_lock);

	if (r == 1)
		return 0;

	return r;
}

static int pico_i2c_write(int reg, u32 value)
{
	u8 data[4];

	data[0] = (value & 0xFF000000) >> 24;
	data[1] = (value & 0x00FF0000) >> 16;
	data[2] = (value & 0x0000FF00) >> 8;
	data[3] = (value & 0x000000FF);

	return dlp_write_block(reg, data, 4);
}

static __attribute__ ((unused)) int dlp_read_block(int reg, u8 *data, int len)
{
	unsigned char wb[2];
	struct i2c_msg msg[2];
	int r;
	mutex_lock(&sd->xfer_lock);
	wb[0] = 0x15 & 0xff;
	wb[1] = reg & 0xff;
	msg[0].addr = sd->client->addr;
	msg[0].len = 2;
	msg[0].flags = 0;
	msg[0].buf = wb;
	msg[1].addr = sd->client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = len;
	msg[1].buf = data;

	r = i2c_transfer(sd->client->adapter, msg, 2);
	mutex_unlock(&sd->xfer_lock);

	if (r == 2) {
		int i;

		for (i = 0; i < len; i++)
			dev_info(&sd->client->dev,
				 "addr %x br 0x%02x[%d]: 0x%02x\n",
				sd->client->addr, reg + i, i, data[i]);
		return len;
	}

	return r;
}


static __attribute__ ((unused)) int pico_i2c_read(int reg)
{
	int r;
	u8 data[4];
	data[1] = data[2] = data[3] = data[0] = 0;

	r = dlp_read_block(reg, data, 4);
	return (int)data[3] | ((int)(data[2]) << 8) | ((int)(data[1]) << 16) | ((int)(data[0]) << 24);
}

/*
 * Configure datapath for splash image operation
 * @param   flash_address   - I - splash image to load from flash
 * @param   flash_num_bytes - I - splash image to load from flash
 * @param   CMT_SEQz        - I - select mailbox to load data to: 0=sequence/DRC, 1=CMT/splash
 * @param   table_number    - I - splash image to load from flash
 * @return  0 - no errors
 *          1 - invalid flash address specified
 *          2 - invalid mailbox specified
 *          3 - invalid table_number / mailbox combination
 */
int dpp2600_flash_dma(int flash_address, int flash_num_bytes, int CMT_SEQz, int table_number)

{
	int mailbox_address, mailbox_select;

	/* check argument validity */
	if (flash_address > 0x1fffff)
		return 1;
	if (CMT_SEQz > 1)
		return 2;
	if ((CMT_SEQz == 0 && table_number > 6) ||
		(CMT_SEQz == 1 && table_number > 5))
		return 3;
	/* set mailbox parameters */
	if (CMT_SEQz) {
		mailbox_address = CMT_SPLASH_LUT_START_ADDR;
		mailbox_select = CMT_SPLASH_LUT_DEST_SELECT;
	} else {
		mailbox_address = SEQ_RESET_LUT_START_ADDR;
		mailbox_select = SEQ_RESET_LUT_DEST_SELECT;
	}

	/* configure DMA from flash to LUT */
	pico_i2c_write(PBC_CONTROL, 0);
	pico_i2c_write(FLASH_START_ADDR, flash_address);
	pico_i2c_write(FLASH_READ_BYTES, flash_num_bytes);
	pico_i2c_write(mailbox_address, 0);
	pico_i2c_write(mailbox_select, table_number);
	/* transfer control to flash controller */
	pico_i2c_write(PBC_CONTROL, 1);
	mdelay(1000);
	/* return register access to I2c */
	pico_i2c_write(PBC_CONTROL, 0);
	/* close LUT access */
	pico_i2c_write(mailbox_select, 0);
	return 0;
}

/* Configure datapath for parallel RGB operation */
static void dpp2600_config_rgb(void)
{
	/* enable video board output drivers */
	pico_i2c_write(SEQ_CONTROL, 0);
	pico_i2c_write(ACTGEN_CONTROL, 0x10);
	pico_i2c_write(SEQUENCE_MODE, SEQ_LOCK);
	pico_i2c_write(DATA_FORMAT, RGB888);
	pico_i2c_write(INPUT_RESOLUTION, WVGA_864_LANDSCAPE);
	pico_i2c_write(INPUT_SOURCE, PARALLEL_RGB);
	pico_i2c_write(CPU_IF_SYNC_METHOD, 1);
	/* turn image back on */
	pico_i2c_write(SEQ_CONTROL, 1);
}

/*
 * Configure datapath for splash image operation
 * @param   image_number - I - splash image to load from flash
 * @return  0 - no errors
 *          1 - invalid image_number specified
 */
int dpp2600_config_splash(int image_number)
{
	int address, size, resolution;
	printk(KERN_INFO "pico DLP dpp2600 config splash\n");
	resolution = QWVGA_LANDSCAPE;
	switch (image_number) {
	case 0:
		address = SPLASH_0_START_ADDR;
		size = SPLASH_0_SIZE;
		break;
	case 1:
		address = SPLASH_1_START_ADDR;
		size = SPLASH_1_SIZE;
		break;
	case 2:
		address = SPLASH_2_START_ADDR;
		size = SPLASH_2_SIZE;
		break;
	case 3:
		address = SPLASH_3_START_ADDR;
		size = SPLASH_3_SIZE;
		break;
	case 4:
		address = OPT_SPLASH_0_START_ADDR;
		size = OPT_SPLASH_0_SIZE;
		resolution = WVGA_DMD_OPTICAL_TEST;
		break;
	default:
		return 1;
	};
	/* configure sequence, data format and resolution */
	pico_i2c_write(SEQ_CONTROL, 0);
	pico_i2c_write(SEQUENCE_MODE, SEQ_FREE_RUN);
	pico_i2c_write(DATA_FORMAT, RGB565);
	pico_i2c_write(INPUT_RESOLUTION, resolution);
	pico_i2c_write(INPUT_SOURCE, SPLASH_SCREEN);
	dpp2600_flash_dma(address, size, 1, SPLASH_LUT);
	/* turn image back on */
	pico_i2c_write(SEQ_CONTROL, 1);
	return 0;
}

/*
 * Configure datapath for test pattern generator operation
 *
 * @param   pattern_select - I - color table to load
 *
 * @return  0 - no errors
 *          1 - invalid pattern specified
 */
int dpp2600_config_tpg(int pattern_select)
{
	if (pattern_select > TPG_ANSI_CHECKERBOARD)
		return 1;
	pico_i2c_write(SEQ_CONTROL, 0);
	pico_i2c_write(INPUT_RESOLUTION, WVGA_854_LANDSCAPE);
	pico_i2c_write(SEQUENCE_MODE, SEQ_LOCK);
	pico_i2c_write(TEST_PAT_SELECT, pattern_select);
	pico_i2c_write(INPUT_SOURCE, 1);
	pico_i2c_write(SEQ_CONTROL, 1);
	return 0;
}

static int pico_i2c_initialize(void)
{

	mutex_init(&sd->xfer_lock);
	mdelay(100);
	/* pico Soft reset */
	pico_i2c_write(SOFT_RESET, 1);
	/*Front end reset*/
	pico_i2c_write(DMD_PARK_TRIGGER, 1);
	/* write the software version number to a spare register field */
	pico_i2c_write(MISC_REG, PICO_MAJOR<<2 | PICO_MINOR);
	pico_i2c_write(SEQ_CONTROL, 0);
	pico_i2c_write(SEQ_VECTOR, 0x100);
	pico_i2c_write(DMD_BLOCK_COUNT, 7);
	pico_i2c_write(DMD_VCC_CONTROL, 0x109);
	pico_i2c_write(DMD_PARK_PULSE_COUNT, 0xA);
	pico_i2c_write(DMD_PARK_PULSE_WIDTH, 0xB);
	pico_i2c_write(DMD_PARK_DELAY, 0x2ED);
	pico_i2c_write(DMD_SHADOW_ENABLE, 0);
	/* serial flash common config */
	pico_i2c_write(FLASH_OPCODE, 0xB);
	pico_i2c_write(FLASH_DUMMY_BYTES, 1);
	pico_i2c_write(FLASH_ADDR_BYTES, 3);
	/* configure DMA from flash to LUT */
	dpp2600_flash_dma(CMT_LUT_0_START_ADDR, CMT_LUT_0_SIZE, 1, CMT_LUT_ALL);
	/* SEQ and DRC look-up tables */
	dpp2600_flash_dma(SEQUENCE_0_START_ADDR, SEQUENCE_0_SIZE, 0, SEQ_SEQ_LUT);
	dpp2600_flash_dma(DRC_TABLE_0_START_ADDR, DRC_TABLE_0_SIZE, 0, SEQ_DRC_LUT_ALL);
	/* frame buffer memory controller enable */
	pico_i2c_write(SDC_ENABLE, 1);
	/* AGC control */
	pico_i2c_write(AGC_CTRL, 7);
	/*CCA */
	pico_i2c_write(CCA_C1A, 0x100);
	pico_i2c_write(CCA_C1B, 0x000);
	pico_i2c_write(CCA_C1C, 0x000);
	pico_i2c_write(CCA_C2A, 0x000);
	pico_i2c_write(CCA_C2B, 0x100);
	pico_i2c_write(CCA_C2C, 0x000);
	pico_i2c_write(CCA_C3A, 0x000);
	pico_i2c_write(CCA_C3B, 0x000);
	pico_i2c_write(CCA_C3C, 0x100);
	pico_i2c_write(CCA_C7A, 0x100);
	pico_i2c_write(CCA_C7B, 0x100);
	pico_i2c_write(CCA_C7C, 0x100);
	pico_i2c_write(CCA_ENABLE, 1);
	/* main datapath setup */
	pico_i2c_write(CPU_IF_MODE, 1);
	pico_i2c_write(SHORT_FLIP, 1);
	pico_i2c_write(CURTAIN_CONTROL, 0);
	/* display Logo splash image */
	dpp2600_config_splash(1);
	pico_i2c_write(DMD_PARK_TRIGGER, 0);
	/* LED PWM and enables */
	pico_i2c_write(R_DRIVE_CURRENT, 0x298);
	pico_i2c_write(G_DRIVE_CURRENT, 0x298);
	pico_i2c_write(B_DRIVE_CURRENT, 0x298);
	pico_i2c_write(RGB_DRIVER_ENABLE, 7);
	mdelay(3000);
	dpp2600_config_rgb();
	return 0;


}

static int pico_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	printk(KERN_INFO "pico DLP probe called\n");
	sd = kzalloc(sizeof(struct pico), GFP_KERNEL);
	if (sd == NULL)
		return -ENOMEM;
	i2c_set_clientdata(client, sd);
	sd->client = client;
	return 0;
}

static int __exit pico_remove(struct i2c_client *client)
{
	struct pico *sd1 = i2c_get_clientdata(client);
	kfree(sd1);
	i2c_set_clientdata(client, NULL);

	return 0;
}

static const struct i2c_device_id pico_id[] = {
	{ "picoDLP_i2c_driver", 0 },
	{ },
};

static int picoDLP_panel_start(struct omap_dss_device *dssdev)
{
	int r = 0;
	if (dssdev->platform_enable) {
		r = dssdev->platform_enable(dssdev);
		if (r)
			return r;
	}
	r = omapdss_dpi_display_enable(dssdev);
	if (r) {
		dev_err(&dssdev->dev, "failed to enable DPI\n");
		return r;
	}
	pico_i2c_initialize();
	return 0;
}

static int picoDLP_panel_enable(struct omap_dss_device *dssdev)
{
	printk(KERN_INFO "pico DLP init is called\n");
	if (dssdev->state != OMAP_DSS_DISPLAY_DISABLED)
		return -EINVAL;

	return picoDLP_panel_start(dssdev);
}

static void picoDLP_panel_get_timings(struct omap_dss_device *dssdev,
					struct omap_video_timings *timings)
{
	*timings = dssdev->panel.timings;
}

static void pico_get_resolution(struct omap_dss_device *dssdev,
		u16 *xres, u16 *yres)
{
		*xres = dssdev->panel.timings.x_res;
		*yres = dssdev->panel.timings.y_res;
}

static int picoDLP_panel_probe(struct omap_dss_device *dssdev)
{
	dssdev->panel.config &= ~((OMAP_DSS_LCD_IPC) | (OMAP_DSS_LCD_IEO));
	dssdev->panel.config =  (OMAP_DSS_LCD_TFT) | (OMAP_DSS_LCD_ONOFF) |
						(OMAP_DSS_LCD_IHS)  |
						(OMAP_DSS_LCD_IVS) ;
	dssdev->panel.acb = 0x0;
	dssdev->panel.timings = pico_ls_timings;

	return 0;
}

static void picoDLP_panel_remove(struct omap_dss_device *dssdev)
{
}

static void picoDLP_panel_stop(struct omap_dss_device *dssdev)
{
	omapdss_dpi_display_disable(dssdev);

	if (dssdev->platform_disable)
		dssdev->platform_disable(dssdev);
}

static void picoDLP_panel_disable(struct omap_dss_device *dssdev)
{
	/* Turn of DLP Power */
	if (dssdev->state == OMAP_DSS_DISPLAY_ACTIVE)
		picoDLP_panel_stop(dssdev);
}

static int picoDLP_panel_suspend(struct omap_dss_device *dssdev)
{
	/* Turn of DLP Power */
	if (dssdev->state != OMAP_DSS_DISPLAY_ACTIVE)
		return -EINVAL;

	picoDLP_panel_stop(dssdev);

	dssdev->state = OMAP_DSS_DISPLAY_SUSPENDED;

	return 0;
}

static int picoDLP_panel_resume(struct omap_dss_device *dssdev)
{
	printk(KERN_INFO "pico DLP resume is called\n");
	if (dssdev->state != OMAP_DSS_DISPLAY_SUSPENDED)
		return -EINVAL;

	return picoDLP_panel_start(dssdev);
}

static struct omap_dss_driver picoDLP_driver = {
	.probe		= picoDLP_panel_probe,
	.remove		= picoDLP_panel_remove,
	.enable		= picoDLP_panel_enable,
	.disable	= picoDLP_panel_disable,
	.get_timings	= picoDLP_panel_get_timings,
	.get_resolution	= pico_get_resolution,
	.suspend	= picoDLP_panel_suspend,
	.resume		= picoDLP_panel_resume,
	.driver         = {
		.name   = "picoDLP_panel",
	.owner  = THIS_MODULE,
	},
};

static struct i2c_driver pico_i2c_driver = {
	.driver = {
		.name           = "pico_i2c_driver",
	},
	.probe          = pico_probe,
	.remove         = __exit_p(pico_remove),
	.id_table       = pico_id,

};

static int __init pico_i2c_init(void)
{
	int r;
	r = i2c_add_driver(&pico_i2c_driver);
	if (r < 0) {
		printk(KERN_WARNING DRIVER_NAME
			" driver registration failed\n");
		return r;
	}
	omap_dss_register_driver(&picoDLP_driver);
	return 0;
}


static void __exit pico_i2c_exit(void)
{
	i2c_del_driver(&pico_i2c_driver);
	omap_dss_unregister_driver(&picoDLP_driver);
}


module_init(pico_i2c_init);
module_exit(pico_i2c_exit);


MODULE_DESCRIPTION("pico DLP driver");
MODULE_LICENSE("GPL");
