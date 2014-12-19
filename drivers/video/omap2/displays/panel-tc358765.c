/*
 * Toshiba TC358765 DSI-to-LVDS chip driver
 *
 * Copyright (C) Texas Instruments
 * Author: Tomi Valkeinen <tomi.valkeinen@ti.com>
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

#define DEBUG

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
#include <linux/pm_runtime.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>

#include <video/omapdss.h>
#include <video/omap-panel-tc358765.h>

#include "panel-tc358765.h"

#define A_RO 0x1
#define A_WO 0x2
#define A_RW (A_RO|A_WO)

#define FLD_MASK(start, end)	(((1 << ((start) - (end) + 1)) - 1) << (end))
#define FLD_VAL(val, start, end) (((val) << (end)) & FLD_MASK(start, end))
#define FLD_GET(val, start, end) (((val) & FLD_MASK(start, end)) >> (end))
#define FLD_MOD(orig, val, start, end) \
	(((orig) & ~FLD_MASK(start, end)) | FLD_VAL(val, start, end))

static struct omap_video_timings tc358765_timings;
static struct tc358765_board_data *get_board_data(struct omap_dss_device
					*dssdev) __attribute__ ((unused));

/* device private data structure */
struct tc358765_data {
	struct mutex lock;

	struct omap_dss_device *dssdev;

	int channel0;
	int channel1;
};

struct tc358765_i2c {
	struct i2c_client *client;
	struct mutex xfer_lock;
} *sd1;


#ifdef CONFIG_TC358765_DEBUG

struct {
	struct device *dev;
	struct dentry *dir;
} tc358765_debug;

struct tc358765_reg {
	const char *name;
	u16 reg;
	u8 perm:2;
} tc358765_regs[] = {
	/* DSI D-PHY Layer Registers */
	{ "D0W_DPHYCONTTX", D0W_DPHYCONTTX, A_RW },
	{ "CLW_DPHYCONTRX", CLW_DPHYCONTRX, A_RW },
	{ "D0W_DPHYCONTRX", D0W_DPHYCONTRX, A_RW },
	{ "D1W_DPHYCONTRX", D1W_DPHYCONTRX, A_RW },
	{ "D2W_DPHYCONTRX", D2W_DPHYCONTRX, A_RW },
	{ "D3W_DPHYCONTRX", D3W_DPHYCONTRX, A_RW },
	{ "COM_DPHYCONTRX", COM_DPHYCONTRX, A_RW },
	{ "CLW_CNTRL", CLW_CNTRL, A_RW },
	{ "D0W_CNTRL", D0W_CNTRL, A_RW },
	{ "D1W_CNTRL", D1W_CNTRL, A_RW },
	{ "D2W_CNTRL", D2W_CNTRL, A_RW },
	{ "D3W_CNTRL", D3W_CNTRL, A_RW },
	{ "DFTMODE_CNTRL", DFTMODE_CNTRL, A_RW },
	/* DSI PPI Layer Registers */
	{ "PPI_STARTPPI", PPI_STARTPPI, A_RW },
	{ "PPI_BUSYPPI", PPI_BUSYPPI, A_RO },
	{ "PPI_LINEINITCNT", PPI_LINEINITCNT, A_RW },
	{ "PPI_LPTXTIMECNT", PPI_LPTXTIMECNT, A_RW },
	{ "PPI_LANEENABLE", PPI_LANEENABLE, A_RW },
	{ "PPI_TX_RX_TA", PPI_TX_RX_TA, A_RW },
	{ "PPI_CLS_ATMR", PPI_CLS_ATMR, A_RW },
	{ "PPI_D0S_ATMR", PPI_D0S_ATMR, A_RW },
	{ "PPI_D1S_ATMR", PPI_D1S_ATMR, A_RW },
	{ "PPI_D2S_ATMR", PPI_D2S_ATMR, A_RW },
	{ "PPI_D3S_ATMR", PPI_D3S_ATMR, A_RW },
	{ "PPI_D0S_CLRSIPOCOUNT", PPI_D0S_CLRSIPOCOUNT, A_RW },
	{ "PPI_D1S_CLRSIPOCOUNT", PPI_D1S_CLRSIPOCOUNT, A_RW },
	{ "PPI_D2S_CLRSIPOCOUNT", PPI_D2S_CLRSIPOCOUNT, A_RW },
	{ "PPI_D3S_CLRSIPOCOUNT", PPI_D3S_CLRSIPOCOUNT, A_RW },
	{ "CLS_PRE", CLS_PRE, A_RW },
	{ "D0S_PRE", D0S_PRE, A_RW },
	{ "D1S_PRE", D1S_PRE, A_RW },
	{ "D2S_PRE", D2S_PRE, A_RW },
	{ "D3S_PRE", D3S_PRE, A_RW },
	{ "CLS_PREP", CLS_PREP, A_RW },
	{ "D0S_PREP", D0S_PREP, A_RW },
	{ "D1S_PREP", D1S_PREP, A_RW },
	{ "D2S_PREP", D2S_PREP, A_RW },
	{ "D3S_PREP", D3S_PREP, A_RW },
	{ "CLS_ZERO", CLS_ZERO, A_RW },
	{ "D0S_ZERO", D0S_ZERO, A_RW },
	{ "D1S_ZERO", D1S_ZERO, A_RW },
	{ "D2S_ZERO", D2S_ZERO, A_RW  },
	{ "D3S_ZERO", D3S_ZERO, A_RW },
	{ "PPI_CLRFLG", PPI_CLRFLG, A_RW },
	{ "PPI_CLRSIPO", PPI_CLRSIPO, A_RW },
	{ "PPI_HSTimeout", PPI_HSTimeout, A_RW },
	{ "PPI_HSTimeoutEnable", PPI_HSTimeoutEnable, A_RW },
	/* DSI Protocol Layer Registers */
	{ "DSI_STARTDSI", DSI_STARTDSI, A_WO },
	{ "DSI_BUSYDSI", DSI_BUSYDSI, A_RO },
	{ "DSI_LANEENABLE", DSI_LANEENABLE, A_RW },
	{ "DSI_LANESTATUS0", DSI_LANESTATUS0, A_RO },
	{ "DSI_LANESTATUS1", DSI_LANESTATUS1, A_RO },
	{ "DSI_INTSTATUS", DSI_INTSTATUS, A_RO },
	{ "DSI_INTMASK", DSI_INTMASK, A_RW },
	{ "DSI_INTCLR", DSI_INTCLR, A_WO },
	{ "DSI_LPTXTO", DSI_LPTXTO, A_RW },
	/* DSI General Registers */
	{ "DSIERRCNT", DSIERRCNT, A_RW },
	/* DSI Application Layer Registers */
	{ "APLCTRL", APLCTRL, A_RW },
	{ "RDPKTLN", RDPKTLN, A_RW },
	/* Video Path Registers */
	{ "VPCTRL", VPCTRL, A_RW },
	{ "HTIM1", HTIM1, A_RW },
	{ "HTIM2",  HTIM2, A_RW },
	{ "VTIM1", VTIM1, A_RW },
	{ "VTIM2", VTIM2, A_RW },
	{ "VFUEN", VFUEN, A_RW },
	/* LVDS Registers */
	{ "LVMX0003", LVMX0003, A_RW },
	{ "LVMX0407", LVMX0407, A_RW },
	{ "LVMX0811", LVMX0811, A_RW },
	{ "LVMX1215", LVMX1215, A_RW },
	{ "LVMX1619", LVMX1619, A_RW },
	{ "LVMX2023", LVMX2023, A_RW },
	{ "LVMX2427", LVMX2427, A_RW },
	{ "LVCFG", LVCFG, A_RW },
	{ "LVPHY0", LVPHY0, A_RW },
	{ "LVPHY1", LVPHY1, A_RW },
	/* System Registers */
	{ "SYSSTAT", SYSSTAT, A_RO },
	{ "SYSRST", SYSRST, A_WO },
	/* GPIO Registers */
	{ "GPIOC", GPIOC, A_RW },
	{ "GPIOO", GPIOO, A_RW },
	{ "GPIOI", GPIOI, A_RO },
	/* I2C Registers */
	{ "I2CTIMCTRL", I2CTIMCTRL, A_RW },
	{ "I2CMADDR", I2CMADDR, A_RW },
	{ "WDATAQ", WDATAQ, A_WO },
	{ "RDATAQ", RDATAQ, A_WO },
	/* Chip/Rev Registers */
	{ "IDREG", IDREG, A_RO },
	/* Debug Registers */
	{ "DEBUG00", DEBUG00, A_RW },
	{ "DEBUG01", DEBUG01, A_RW },
};
#endif

static int tc358765_read_block(u16 reg, u8 *data, int len)
{
	unsigned char wb[2];
	struct i2c_msg msg[2];
	int r;
	mutex_lock(&sd1->xfer_lock);
	wb[0] = (reg & 0xff00) >> 8;
	wb[1] = reg & 0xff;
	msg[0].addr = sd1->client->addr;
	msg[0].len = 2;
	msg[0].flags = 0;
	msg[0].buf = wb;
	msg[1].addr = sd1->client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = len;
	msg[1].buf = data;

	r = i2c_transfer(sd1->client->adapter, msg, 2);
	mutex_unlock(&sd1->xfer_lock);

	if (r == 2)
		return len;

	return r;
}

static int tc358765_i2c_read(u16 reg, u32 *val)
{
	int r;
	u8 data[4];
	data[0] = data[1] = data[2] = data[3] = 0;

	r = tc358765_read_block(reg, data, 4);
	if (r != 4)
		return -EIO;

	*val = ((int)data[3] << 24) | ((int)(data[2]) << 16) |
	    ((int)(data[1]) << 8) | ((int)(data[0]));
	return 0;
}

static int tc358765_dsi_read(struct omap_dss_device *dssdev, u16 reg, u32 *val)
{
	struct tc358765_data *d2d = dev_get_drvdata(&dssdev->dev);
	u8 buf[4];
	int r;

	r = dsi_vc_gen_read_2(dssdev, d2d->channel1, reg, buf, 4);
	if (r < 0) {
		dev_err(&dssdev->dev, "0x%x read failed with status %d\n",
								reg, r);
		return r;
	}

	*val = buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);
	return 0;
}

static int tc358765_read_register(struct omap_dss_device *dssdev,
					u16 reg, u32 *val)
{
	int ret = 0;
	pm_runtime_get_sync(&dssdev->dev);
	/* I2C is preferred way of reading, but fall back to DSI
	 * if I2C didn't got initialized
	*/
	if (sd1)
		ret = tc358765_i2c_read(reg, val);
	else
		ret = tc358765_dsi_read(dssdev, reg, val);
	pm_runtime_put_sync(&dssdev->dev);
	return ret;
}

static int tc358765_write_register(struct omap_dss_device *dssdev, u16 reg,
		u32 value)
{
	struct tc358765_data *d2d = dev_get_drvdata(&dssdev->dev);
	u8 buf[6];
	int r;

	buf[0] = (reg >> 0) & 0xff;
	buf[1] = (reg >> 8) & 0xff;
	buf[2] = (value >> 0) & 0xff;
	buf[3] = (value >> 8) & 0xff;
	buf[4] = (value >> 16) & 0xff;
	buf[5] = (value >> 24) & 0xff;

	r = dsi_vc_gen_write_nosync(dssdev, d2d->channel1, buf, 6);
	if (r)
		dev_err(&dssdev->dev, "reg write reg(%x) val(%x) failed: %d\n",
			       reg, value, r);
	return r;
}

/****************************
********* DEBUG *************
****************************/
#ifdef CONFIG_TC358765_DEBUG
static int tc358765_write_register_i2c(u16 reg, u32 val)
{
	int ret = -ENODEV;
	unsigned char buf[6];
	struct i2c_msg msg;

	if (!sd1) {
		dev_err(&sd1->client->dev, "%s: I2C not initilized\n",
							__func__);
		return ret;
	}

	buf[0] = (reg >> 8) & 0xff;
	buf[1] = (reg >> 0) & 0xff;
	buf[2] = (val >> 0) & 0xff;
	buf[3] = (val >> 8) & 0xff;
	buf[4] = (val >> 16) & 0xff;
	buf[5] = (val >> 24) & 0xff;
	msg.addr = sd1->client->addr;
	msg.len = sizeof(buf);
	msg.flags = 0;
	msg.buf = buf;

	mutex_lock(&sd1->xfer_lock);
	ret = i2c_transfer(sd1->client->adapter, &msg, 1);
	mutex_unlock(&sd1->xfer_lock);

	if (ret != 1)
		return ret;
	return 0;
}


static int tc358765_registers_show(struct seq_file *seq, void *pos)
{
	struct device *dev = tc358765_debug.dev;
	unsigned i, reg_count;
	uint value;

	if (!sd1) {
		dev_warn(dev,
			"failed to read register: I2C not initialized\n");
		return -ENODEV;
	}

	reg_count = sizeof(tc358765_regs) / sizeof(tc358765_regs[0]);
	pm_runtime_get_sync(dev);
	for (i = 0; i < reg_count; i++) {
		if (tc358765_regs[i].perm & A_RO) {
			tc358765_i2c_read(tc358765_regs[i].reg, &value);
			seq_printf(seq, "%-20s = 0x%02X\n",
					tc358765_regs[i].name, value);
		}
	}

	pm_runtime_put_sync(dev);
	return 0;
}
static ssize_t tc358765_seq_write(struct file *filp, const char __user *ubuf,
						size_t size, loff_t *ppos)
{
	struct device *dev = tc358765_debug.dev;
	unsigned i, reg_count;
	u32 value = 0;
	int error = 0;
	/* kids, don't use register names that long */
	char name[30];
	char buf[50];

	if (size >= sizeof(buf))
		size = sizeof(buf);

	if (copy_from_user(&buf, ubuf, size))
		return -EFAULT;

	buf[size-1] = '\0';
	if (sscanf(buf, "%s %x", name, &value) != 2) {
		dev_err(dev, "%s: unable to parse input\n", __func__);
		return -1;
	}

	if (!sd1) {
		dev_warn(dev,
			"failed to write register: I2C not initialized\n");
		return -ENODEV;
	}

	reg_count = sizeof(tc358765_regs) / sizeof(tc358765_regs[0]);
	for (i = 0; i < reg_count; i++) {
		if (!strcmp(name, tc358765_regs[i].name)) {
			if (!(tc358765_regs[i].perm & A_WO)) {
				dev_err(dev, "%s is write-protected\n", name);
				return -EACCES;
			}

			error = tc358765_write_register_i2c(
				tc358765_regs[i].reg, value);
			if (error) {
				dev_err(dev, "%s: failed to write %s\n",
					__func__, name);
				return -1;
			}

			return size;
		}
	}

	dev_err(dev, "%s: no such register %s\n", __func__, name);

	return size;
}

static ssize_t tc358765_seq_open(struct inode *inode, struct file *file)
{
	return single_open(file, tc358765_registers_show, inode->i_private);
}

static const struct file_operations tc358765_debug_fops = {
	.open		= tc358765_seq_open,
	.read		= seq_read,
	.write		= tc358765_seq_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int tc358765_initialize_debugfs(struct omap_dss_device *dssdev)
{
	tc358765_debug.dir = debugfs_create_dir("tc358765", NULL);
	if (IS_ERR(tc358765_debug.dir)) {
		int ret = PTR_ERR(tc358765_debug.dir);
		tc358765_debug.dir = NULL;
		return ret;
	}

	tc358765_debug.dev = &dssdev->dev;
	debugfs_create_file("registers", S_IRWXU, tc358765_debug.dir,
			dssdev, &tc358765_debug_fops);
	return 0;
}

static void tc358765_uninitialize_debugfs(void)
{
	if (tc358765_debug.dir)
		debugfs_remove_recursive(tc358765_debug.dir);
	tc358765_debug.dir = NULL;
	tc358765_debug.dev = NULL;
}

#else
static int tc358765_initialize_debugfs(struct omap_dss_device *dssdev)
{
	return 0;
}

static void tc358765_uninitialize_debugfs(void)
{
}
#endif

static struct tc358765_board_data *get_board_data(struct omap_dss_device
								*dssdev)
{
	return (struct tc358765_board_data *)dssdev->data;
}

static void tc358765_get_timings(struct omap_dss_device *dssdev,
		struct omap_video_timings *timings)
{
	*timings = dssdev->panel.timings;
}

static void tc358765_set_timings(struct omap_dss_device *dssdev,
		struct omap_video_timings *timings)
{
	dev_info(&dssdev->dev, "set_timings() not implemented\n");
}

static int tc358765_check_timings(struct omap_dss_device *dssdev,
		struct omap_video_timings *timings)
{
	if (tc358765_timings.x_res != timings->x_res ||
			tc358765_timings.y_res != timings->y_res ||
			tc358765_timings.pixel_clock != timings->pixel_clock ||
			tc358765_timings.hsw != timings->hsw ||
			tc358765_timings.hfp != timings->hfp ||
			tc358765_timings.hbp != timings->hbp ||
			tc358765_timings.vsw != timings->vsw ||
			tc358765_timings.vfp != timings->vfp ||
			tc358765_timings.vbp != timings->vbp)
		return -EINVAL;

	return 0;
}

static void tc358765_get_resolution(struct omap_dss_device *dssdev,
		u16 *xres, u16 *yres)
{
	*xres = tc358765_timings.x_res;
	*yres = tc358765_timings.y_res;
}

static int tc358765_hw_reset(struct omap_dss_device *dssdev)
{
	if (dssdev == NULL || dssdev->reset_gpio == -1)
		return 0;

	gpio_set_value(dssdev->reset_gpio, 1);
	udelay(100);
	/* reset the panel */
	gpio_set_value(dssdev->reset_gpio, 0);
	/* assert reset */
	udelay(100);
	gpio_set_value(dssdev->reset_gpio, 1);

	/* wait after releasing reset */
	msleep(100);

	return 0;
}

static int tc358765_probe(struct omap_dss_device *dssdev)
{
	struct tc358765_data *d2d;
	int r = 0;

	dev_dbg(&dssdev->dev, "tc358765_probe\n");

	dssdev->panel.config = OMAP_DSS_LCD_TFT;
	tc358765_timings = dssdev->panel.timings;
	dssdev->panel.acbi = 0;
	dssdev->panel.acb = 40;

	d2d = kzalloc(sizeof(*d2d), GFP_KERNEL);
	if (!d2d) {
		r = -ENOMEM;
		goto err;
	}

	d2d->dssdev = dssdev;

	mutex_init(&d2d->lock);

	dev_set_drvdata(&dssdev->dev, d2d);

	r = omap_dsi_request_vc(dssdev, &d2d->channel0);
	if (r) {
		dev_err(&dssdev->dev, "failed to get virtual channel0\n");
		goto err;
	}

	r = omap_dsi_set_vc_id(dssdev, d2d->channel0, 0);
	if (r) {
		dev_err(&dssdev->dev, "failed to set VC_ID0\n");
		goto err;
	}

	r = omap_dsi_request_vc(dssdev, &d2d->channel1);
	if (r) {
		dev_err(&dssdev->dev, "failed to get virtual channel1\n");
		goto err;
	}

	r = omap_dsi_set_vc_id(dssdev, d2d->channel1, 0);
	if (r) {
		dev_err(&dssdev->dev, "failed to set VC_ID1\n");
		goto err;
	}

	r = tc358765_initialize_debugfs(dssdev);
	if (r)
		dev_warn(&dssdev->dev, "failed to create sysfs files\n");

	dev_dbg(&dssdev->dev, "tc358765_probe done\n");
	return 0;
err:
	kfree(d2d);
	return r;
}

static void tc358765_remove(struct omap_dss_device *dssdev)
{
	struct tc358765_data *d2d = dev_get_drvdata(&dssdev->dev);

	tc358765_uninitialize_debugfs();

	omap_dsi_release_vc(dssdev, d2d->channel0);
	omap_dsi_release_vc(dssdev, d2d->channel1);

	kfree(d2d);
}

static int tc358765_init_ppi(struct omap_dss_device *dssdev)
{
	u32 go_cnt, sure_cnt, val = 0;
	u8 lanes = 0;
	int ret = 0;
	struct tc358765_board_data *board_data = get_board_data(dssdev);

	/* this register setting is required only if host wishes to
	 * perform DSI read transactions
	 */
	go_cnt = (board_data->lp_time * 5 - 3) / 4;
	sure_cnt = DIV_ROUND_UP(board_data->lp_time * 3, 2);
	val = FLD_MOD(val, go_cnt, 26, 16);
	val = FLD_MOD(val, sure_cnt, 10, 0);
	ret |= tc358765_write_register(dssdev, PPI_TX_RX_TA, val);

	/* SYSLPTX Timing Generation Counter */
	ret |= tc358765_write_register(dssdev, PPI_LPTXTIMECNT,
					board_data->lp_time);

	/* D*S_CLRSIPOCOUNT = [(THS-SETTLE + THS-ZERO) /
					HS_byte_clock_period ] */

	if (dssdev->phy.dsi.clk_lane)
		lanes |= (1 << 0);

	if (dssdev->phy.dsi.data1_lane) {
		lanes |= (1 << 1);
		ret |= tc358765_write_register(dssdev, PPI_D0S_CLRSIPOCOUNT,
							board_data->clrsipo);
	}
	if (dssdev->phy.dsi.data2_lane) {
		lanes |= (1 << 2);
		ret |= tc358765_write_register(dssdev, PPI_D1S_CLRSIPOCOUNT,
							board_data->clrsipo);
	}
	if (dssdev->phy.dsi.data3_lane) {
		lanes |= (1 << 3);
		ret |= tc358765_write_register(dssdev, PPI_D2S_CLRSIPOCOUNT,
							board_data->clrsipo);
	}
	if (dssdev->phy.dsi.data4_lane) {
		lanes |= (1 << 4);
		ret |= tc358765_write_register(dssdev, PPI_D3S_CLRSIPOCOUNT,
							board_data->clrsipo);
	}

	ret |= tc358765_write_register(dssdev, PPI_LANEENABLE, lanes);
	ret |= tc358765_write_register(dssdev, DSI_LANEENABLE, lanes);

	return ret;
}

static int tc358765_init_video_timings(struct omap_dss_device *dssdev)
{
	u32 val;
	struct tc358765_board_data *board_data = get_board_data(dssdev);
	int ret;
	ret = tc358765_read_register(dssdev, VPCTRL, &val);
	if (ret < 0) {
		dev_warn(&dssdev->dev,
			"couldn't access VPCTRL, going on with reset value\n");
		val = 0;
	}

	if (dssdev->ctrl.pixel_size == 18) {
		/* Magic Square FRC available for RGB666 only */
		val = FLD_MOD(val, board_data->msf, 0, 0);
		val = FLD_MOD(val, 0, 8, 8);
	} else {
		val = FLD_MOD(val, 1, 8, 8);
	}

	val = FLD_MOD(val, board_data->vtgen, 4, 4);
	val = FLD_MOD(val, board_data->evtmode, 5, 5);
	val = FLD_MOD(val, board_data->vsdelay, 31, 20);

	ret = tc358765_write_register(dssdev, VPCTRL, val);

	ret |= tc358765_write_register(dssdev, HTIM1,
		(tc358765_timings.hbp << 16) | tc358765_timings.hsw);
	ret |= tc358765_write_register(dssdev, HTIM2,
		((tc358765_timings.hfp << 16) | tc358765_timings.x_res));
	ret |= tc358765_write_register(dssdev, VTIM1,
		((tc358765_timings.vbp << 16) |	tc358765_timings.vsw));
	ret |= tc358765_write_register(dssdev, VTIM2,
		((tc358765_timings.vfp << 16) |	tc358765_timings.y_res));
	return ret;
}

static int tc358765_write_init_config(struct omap_dss_device *dssdev)
{
	struct tc358765_board_data *board_data = get_board_data(dssdev);
	u32 val;
	int r;

	/* HACK: dummy read: if we read via DSI, first reads always fail */
	tc358765_read_register(dssdev, DSI_INTSTATUS, &val);

	r = tc358765_init_ppi(dssdev);
	if (r) {
		dev_err(&dssdev->dev, "failed to initialize PPI layer\n");
		return r;
	}

	r = tc358765_write_register(dssdev, PPI_STARTPPI, 0x1);
	if (r) {
		dev_err(&dssdev->dev, "failed to start PPI-TX\n");
		return r;
	}

	r = tc358765_write_register(dssdev, DSI_STARTDSI, 0x1);
	if (r) {
		dev_err(&dssdev->dev, "failed to start DSI-RX\n");
		return r;
	}

	/* reset LVDS-PHY */
	tc358765_write_register(dssdev, LVPHY0, (1 << 22));
	mdelay(2);

	r = tc358765_read_register(dssdev, LVPHY0, &val);
	if (r < 0) {
		dev_warn(&dssdev->dev, "couldn't access LVPHY0, going on with reset value\n");
		val = 0;
	}
	val = FLD_MOD(val, 0, 22, 22);
	val = FLD_MOD(val, board_data->lv_is, 15, 14);
	val = FLD_MOD(val, board_data->lv_nd, 4, 0);
	r = tc358765_write_register(dssdev, LVPHY0, val);
	if (r) {
		dev_err(&dssdev->dev, "failed to initialize LVDS-PHY\n");
		return r;
	}
	r = tc358765_init_video_timings(dssdev);
	if (r) {
		dev_err(&dssdev->dev, "failed to initialize video path layer\n");
		return r;
	}

	r = tc358765_read_register(dssdev, LVCFG, &val);
	if (r < 0) {
		dev_warn(&dssdev->dev,
			"couldn't access LVCFG, going on with reset value\n");
		val = 0;
	}

	val = FLD_MOD(val, board_data->pclkdiv, 9, 8);
	val = FLD_MOD(val, board_data->pclksel, 11, 10);
	val = FLD_MOD(val, board_data->lvdlink, 1, 1);
	/* enable LVDS transmitter */
	val = FLD_MOD(val, 1, 0, 0);
	r = tc358765_write_register(dssdev, LVCFG, val);
	if (r) {
		dev_err(&dssdev->dev, "failed to start LVDS transmitter\n");
		return r;
	}

	/* Issue a soft reset to LCD Controller for a clean start */
	r = tc358765_write_register(dssdev, SYSRST, (1 << 2));
	/* commit video configuration */
	r |= tc358765_write_register(dssdev, VFUEN, 0x1);
	if (r)
		dev_err(&dssdev->dev, "failed to latch video timings\n");
	return r;
}

static int tc358765_power_on(struct omap_dss_device *dssdev)
{
	struct tc358765_data *d2d = dev_get_drvdata(&dssdev->dev);
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

	omapdss_dsi_vc_enable_hs(dssdev, d2d->channel0, true);

	/* reset tc358765 bridge */
	tc358765_hw_reset(dssdev);

	/* We need to wait at least 100ms after power-on and before
	 * turning on DDR clock. Otherwise display can be not initialithed
	 * after resume, time to time. Delay in tc358765_hw_reset() function
	 * also plays role. */
	msleep(100);

	/* do extra job to match kozio registers (???) */
	dsi_videomode_panel_preinit(dssdev);

	/* Need to wait a certain time - Toshiba Bridge Constraint */
	/* msleep(400); */

	/* configure D2L chip DSI-RX configuration registers */
	r = tc358765_write_init_config(dssdev);
	if (r)
		goto err_write_init;

	omapdss_dsi_vc_enable_hs(dssdev, d2d->channel1, true);

	/* 0x0e - 16bit
	 * 0x1e - packed 18bit
	 * 0x2e - unpacked 18bit
	 * 0x3e - 24bit
	 */
	switch (dssdev->ctrl.pixel_size) {
	case 18:
		dsi_video_mode_enable(dssdev, 0x1e);
		break;
	case 24:
		dsi_video_mode_enable(dssdev, 0x3e);
		break;
	default:
		dev_warn(&dssdev->dev, "not expected pixel size: %d\n",
					dssdev->ctrl.pixel_size);
	}

	dev_dbg(&dssdev->dev, "power_on done\n");

	return r;

err_write_init:
	omapdss_dsi_display_disable(dssdev, false, false);
err_disp_enable:
	if (dssdev->platform_disable)
		dssdev->platform_disable(dssdev);

	return r;
}

static void tc358765_power_off(struct omap_dss_device *dssdev)
{
	dsi_video_mode_disable(dssdev);

	omapdss_dsi_display_disable(dssdev, false, false);

	if (dssdev->platform_disable)
		dssdev->platform_disable(dssdev);

	gpio_set_value(dssdev->reset_gpio, 0);
}

static void tc358765_disable(struct omap_dss_device *dssdev)
{
	struct tc358765_data *d2d = dev_get_drvdata(&dssdev->dev);

	dev_dbg(&dssdev->dev, "disable\n");

	if (dssdev->state == OMAP_DSS_DISPLAY_ACTIVE) {
		mutex_lock(&d2d->lock);
		dsi_bus_lock(dssdev);

		tc358765_power_off(dssdev);

		dsi_bus_unlock(dssdev);
		mutex_unlock(&d2d->lock);
	}

	dssdev->state = OMAP_DSS_DISPLAY_DISABLED;
}

static int tc358765_enable(struct omap_dss_device *dssdev)
{
	struct tc358765_data *d2d = dev_get_drvdata(&dssdev->dev);
	int r = 0;

	dev_dbg(&dssdev->dev, "enable\n");

	if (dssdev->state != OMAP_DSS_DISPLAY_DISABLED)
		return -EINVAL;

	mutex_lock(&d2d->lock);
	dsi_bus_lock(dssdev);

	r = tc358765_power_on(dssdev);

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

static int tc358765_suspend(struct omap_dss_device *dssdev)
{
	/* Disable the panel and return 0;*/
	tc358765_disable(dssdev);
	return 0;
}

static struct omap_dss_driver tc358765_driver = {
	.probe		= tc358765_probe,
	.remove		= tc358765_remove,

	.enable		= tc358765_enable,
	.disable	= tc358765_disable,
	.suspend	= tc358765_suspend,
	.resume		= NULL,

	.get_resolution	= tc358765_get_resolution,
	.get_recommended_bpp = omapdss_default_get_recommended_bpp,

	.get_timings	= tc358765_get_timings,
	.set_timings	= tc358765_set_timings,
	.check_timings	= tc358765_check_timings,

	.driver         = {
		.name   = "tc358765",
		.owner  = THIS_MODULE,
	},
};

static int __devinit tc358765_i2c_probe(struct i2c_client *client,
				   const struct i2c_device_id *id)
{
	sd1 = kzalloc(sizeof(struct tc358765_i2c), GFP_KERNEL);
	if (sd1 == NULL)
		return -ENOMEM;

	/* store i2c_client pointer on private data structure */
	sd1->client = client;

	/* store private data structure pointer on i2c_client structure */
	i2c_set_clientdata(client, sd1);

	/* init mutex */
	mutex_init(&sd1->xfer_lock);
	dev_err(&client->dev, "D2L i2c initialized\n");
	return 0;
}

/* driver remove function */
static int __devexit tc358765_i2c_remove(struct i2c_client *client)
{
	struct tc358765_i2c *sd1 = i2c_get_clientdata(client);

	/* remove client data */
	i2c_set_clientdata(client, NULL);

	/* free private data memory */
	kfree(sd1);

	return 0;
}

static const struct i2c_device_id tc358765_i2c_idtable[] = {
	{"tc358765_i2c_driver", 0},
	{},
};

static struct i2c_driver tc358765_i2c_driver = {
	.probe = tc358765_i2c_probe,
	.remove = __exit_p(tc358765_i2c_remove),
	.id_table = tc358765_i2c_idtable,
	.driver = {
		   .name  = "d2l",
		   .owner = THIS_MODULE,
	},
};


static int __init tc358765_init(void)
{
	int r;
	sd1 = NULL;
	r = i2c_add_driver(&tc358765_i2c_driver);
	if (r < 0) {
		printk(KERN_WARNING "d2l i2c driver registration failed\n");
		return r;
	}

	omap_dss_register_driver(&tc358765_driver);
	return 0;
}

static void __exit tc358765_exit(void)
{
	omap_dss_unregister_driver(&tc358765_driver);
	i2c_del_driver(&tc358765_i2c_driver);
}

module_init(tc358765_init);
module_exit(tc358765_exit);

MODULE_AUTHOR("Tomi Valkeinen <tomi.valkeinen@ti.com>");
MODULE_DESCRIPTION("TC358765 DSI-2-LVDS Driver");
MODULE_LICENSE("GPL");
