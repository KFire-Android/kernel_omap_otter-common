/*
 * linux/drivers/video/omap2/dss/hdmi.c
 *
 * Copyright (C) 2009 Texas Instruments
 * Author: Yong Zhi
 *
 * HDMI settings from TI's DSS driver
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
 * History:
 * Mythripk <mythripk@ti.com>	Apr 2010 Modified for EDID reading and adding
 *				OMAP related timing
 *				May 2010 Added support of Hot Plug Detect
 *				July 2010 Redesigned HDMI EDID for Auto-detect
 *				of timing
 *				August 2010 Char device user space control for
 *				HDMI
 * Munish <munish@ti.com>	Sep 2010 Added support for Mandatory S3D formats
 */

#define DSS_SUBSYS_NAME "HDMI"

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/platform_device.h>
#include <plat/display.h>
#include <plat/cpu.h>
#include <plat/hdmi_lib.h>
#include <plat/gpio.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/fs.h>

#include "dss.h"
#include <plat/edid.h>

static int hdmi_enable_video(struct omap_dss_device *dssdev);
static void hdmi_disable_video(struct omap_dss_device *dssdev);
static int hdmi_suspend_video(struct omap_dss_device *dssdev);
static int hdmi_resume_video(struct omap_dss_device *dssdev);
static void hdmi_get_timings(struct omap_dss_device *dssdev,
			struct omap_video_timings *timings);
static void hdmi_set_timings(struct omap_dss_device *dssdev,
			struct omap_video_timings *timings);
static void hdmi_set_custom_edid_timing_code(struct omap_dss_device *dssdev,
							int code , int mode);
static void hdmi_get_edid(struct omap_dss_device *dssdev);
static int hdmi_check_timings(struct omap_dss_device *dssdev,
			struct omap_video_timings *timings);
static int hdmi_read_edid(struct omap_video_timings *);
static int get_edid_timing_data(struct HDMI_EDID *edid);
static irqreturn_t hdmi_irq_handler(int irq, void *arg);
static int hdmi_enable_hpd(struct omap_dss_device *dssdev);
static int hdmi_set_power(struct omap_dss_device *dssdev,
		enum hdmi_power_state v_power, bool suspend, bool audio_on);
static int set_video_power(struct omap_dss_device *dssdev,
		enum hdmi_power_state v_power);
static void hdmi_power_off(struct omap_dss_device *dssdev);
static int hdmi_open(struct inode *inode, struct file *filp);
static int hdmi_release(struct inode *inode, struct file *filp);
static int hdmi_ioctl(struct inode *inode, struct file *file,
					  unsigned int cmd, unsigned long arg);
static bool hdmi_get_s3d_enabled(struct omap_dss_device *dssdev);
static int hdmi_enable_s3d(struct omap_dss_device *dssdev, bool enable);
static int hdmi_set_s3d_disp_type(struct omap_dss_device *dssdev,
		struct s3d_disp_info *info);
static void hdmi_notify_queue(struct work_struct *work);
static int hdmi_reset(struct omap_dss_device *dssdev,
					enum omap_dss_reset_phase phase);
static struct hdmi_cm hdmi_get_code(struct omap_video_timings *timing);

/* Structures for chardevice move this to panel */
static int hdmi_major;
static struct cdev hdmi_cdev;
static dev_t hdmi_dev_id;

/* Read and write ioctls will be added to configure parameters */
static const struct file_operations hdmi_fops = {
	.owner = THIS_MODULE,
	.open = hdmi_open,
	.release = hdmi_release,
	.ioctl = hdmi_ioctl,
};

static enum hdmi_power_state hdmi_power;	/* current power */
static enum hdmi_power_state video_power;	/* video power state */
bool v_suspended;				/* video part is suspended */
static bool audio_on;				/* audio power on/off */
static bool user_hpd_state;	/* user hpd state - last notification state */

struct workqueue_struct *irq_wq;
static DECLARE_WAIT_QUEUE_HEAD(audio_wq);
static bool hot_plug_notify_canceled;
static DECLARE_DELAYED_WORK(hot_plug_notify_work, hdmi_notify_queue);
static DEFINE_SPINLOCK(irqstatus_lock);

struct hdmi_work_struct {
	struct work_struct work;
	int r;
};
static bool hdmi_opt_clk_state;
static int in_reset;
#define HDMI_IN_RESET		0x1000
static bool hdmi_connected;

#define HDMI_PLLCTRL		0x58006200
#define HDMI_PHY		0x58006300

static u8 edid[HDMI_EDID_MAX_LENGTH] = {0};
static u8 edid_set;
static u8 header[8] = {0x0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x0};
static u8 custom_set;

enum hdmi_ioctl_cmds {
	HDMI_ENABLE,
	HDMI_DISABLE,
	HDMI_READ_EDID,
};

enum hdmi_s3d_frame_structure {
	HDMI_S3D_FRAME_PACKING          = 0,
	HDMI_S3D_FIELD_ALTERNATIVE      = 1,
	HDMI_S3D_LINE_ALTERNATIVE       = 2,
	HDMI_S3D_SIDE_BY_SIDE_FULL      = 3,
	HDMI_S3D_L_DEPTH                = 4,
	HDMI_S3D_L_DEPTH_GP_GP_DEPTH    = 5,
	HDMI_S3D_SIDE_BY_SIDE_HALF      = 8
};

/* Subsampling types used for Sterioscopic 3D over HDMI. Below HOR
stands for Horizontal, QUI for Quinxcunx Subsampling, O for odd fields,
E for Even fields, L for left view and R for Right view*/
enum hdmi_s3d_subsampling_type {
	HDMI_S3D_HOR_OL_OR = 0,/*horizontal subsampling with odd fields
		from left view and even fields from the right view*/
	HDMI_S3D_HOR_OL_ER = 1,
	HDMI_S3D_HOR_EL_OR = 2,
	HDMI_S3D_HOR_EL_ER = 3,
	HDMI_S3D_QUI_OL_OR = 4,
	HDMI_S3D_QUI_OL_ER = 5,
	HDMI_S3D_QUI_EL_OR = 6,
	HDMI_S3D_QUI_EL_ER = 7
};

/* PLL */
#define PLLCTRL_PLL_CONTROL				0x0ul
#define PLLCTRL_PLL_STATUS				0x4ul
#define PLLCTRL_PLL_GO					0x8ul
#define PLLCTRL_CFG1					0xCul
#define PLLCTRL_CFG2					0x10ul
#define PLLCTRL_CFG3					0x14ul
#define PLLCTRL_CFG4					0x20ul

/* HDMI PHY */
#define HDMI_TXPHY_TX_CTRL				0x0ul
#define HDMI_TXPHY_DIGITAL_CTRL			0x4ul
#define HDMI_TXPHY_POWER_CTRL			0x8ul
#define HDMI_TXPHY_PAD_CFG_CTRL			0xCul

struct hdmi_hvsync_pol {
	int vsync_pol;
	int hsync_pol;
};

/* All supported timing values that OMAP4 supports */
static const struct omap_video_timings all_timings_direct[] = {
	{640, 480, 25200, 96, 16, 48, 2, 10, 33},
	{1280, 720, 74250, 40, 440, 220, 5, 5, 20},
	{1280, 720, 74250, 40, 110, 220, 5, 5, 20},
	{720, 480, 27027, 62, 16, 60, 6, 9, 30},
	{2880, 576, 108000, 256, 48, 272, 5, 5, 39},
	{1440, 240, 27027, 124, 38, 114, 3, 4, 15},
	{1440, 288, 27000, 126, 24, 138, 3, 2, 19},
	{1920, 540, 74250, 44, 528, 148, 5, 2, 15},
	{1920, 540, 74250, 44, 88, 148, 5, 2, 15},
	{1920, 1080, 148500, 44, 88, 148, 5, 4, 36},
	{720, 576, 27000, 64, 12, 68, 5, 5, 39},
	{1440, 576, 54000, 128, 24, 136, 5, 5, 39},
	{1920, 1080, 148500, 44, 528, 148, 5, 4, 36},
	{2880, 480, 108108, 248, 64, 240, 6, 9, 30},
	{1920, 1080, 74250, 44, 638, 148, 5, 4, 36},
	/* Vesa frome here */
	{640, 480, 25175, 96, 8, 40, 2, 2, 25},
	{800, 600, 40000, 128, 40, 88, 4 , 1, 23},
	{848, 480, 33750, 112, 16, 112, 8 , 6, 23},
	{1280, 768, 79500, 128, 64, 192, 7 , 3, 20},
	{1280, 800, 83500, 128, 72, 200, 6 , 3, 22},
	{1360, 768, 85500, 112, 64, 256, 6 , 3, 18},
	{1280, 960, 108000, 112, 96, 312, 3 , 1, 36},
	{1280, 1024, 108000, 112, 48, 248, 3 , 1, 38},
	{1024, 768, 65000, 136, 24, 160, 6, 3, 29},
	{1400, 1050, 121750, 144, 88, 232, 4, 3, 32},
	{1440, 900, 106500, 152, 80, 232, 6, 3, 25},
	{1680, 1050, 146250, 176 , 104, 280, 6, 3, 30},
	{1366, 768, 85500, 143, 70, 213, 3, 3, 24},
	{1920, 1080, 148500, 44, 88, 148, 5, 4, 36},
	{1280, 768, 68250, 32, 48, 80, 7, 3, 12},
	{1400, 1050, 101000, 32, 48, 80, 4, 3, 23},
	{1680, 1050, 119000, 32, 48, 80, 6, 3, 21},
	{1280, 800, 79500, 32, 48, 80, 6, 3, 14},
	{1280, 720, 74250, 40, 110, 220, 5, 5, 20},
	/* supported 3d timings UNDEROVER full frame */
	{1280, 1470, 148350, 40, 110, 220, 5, 5, 20},
	{1280, 1470, 148500, 40, 110, 220, 5, 5, 20},
	{1280, 1470, 148500, 40, 440, 220, 5, 5, 20}
};

/* Array which maps the timing values with corresponding CEA / VESA code */
static int code_index[ARRAY_SIZE(all_timings_direct)] = {
	1,  19,  4,  2, 37,  6, 21, 20,  5, 16, 17,
	29, 31, 35, 32,
	/* <--15 CEA 22--> vesa*/
	4, 9, 0xE, 0x17, 0x1C, 0x27, 0x20, 0x23, 0x10, 0x2A,
	0X2F, 0x3A, 0X51, 0X52, 0x16, 0x29, 0x39, 0x1B, 0x55, 4,
	4, 19
};

/* Mapping the Timing values with the corresponding Vsync and Hsync polarity */
static const
struct hdmi_hvsync_pol hvpol_mapping[ARRAY_SIZE(all_timings_direct)] = {
	{0, 0}, {1, 1}, {1, 1}, {0, 0},
	{0, 0}, {0, 0}, {0, 0}, {1, 1},
	{1, 1}, {1, 1}, {0, 0}, {0, 0},
	{1, 1}, {0, 0}, {1, 1}, /* VESA */
	{0, 0}, {1, 1}, {1, 1}, {1, 0},
	{1, 0}, {1, 1}, {1, 1}, {1, 1},
	{0, 0}, {1, 0}, {1, 0}, {1, 0},
	{1, 1}, {1, 1}, {0, 1}, {0, 1},
	{0, 1}, {0, 1}, {1, 1}, {1, 1},
	{1, 1}, {1, 1}
};

/* Map CEA code to the corresponding timing values (10 entries/line) */
static int code_cea[39] = {
	-1,  0,  3,  3,  2,  8,  5,  5, -1, -1,
	-1, -1, -1, -1, -1, -1,  9, 10, 10,  1,
	7,   6,  6, -1, -1, -1, -1, -1, -1, 11,
	11, 12, 14, -1, -1, 13, 13,  4,  4
};

/* Map CEA code to the corresponding 3D timing values */
static int s3d_code_cea[39] = {
	-1, -1, -1, -1, 35, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, 36,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1
};

/* Map VESA code to the corresponding timing values */
static int code_vesa[86] = {
	-1, -1, -1, -1, 15, -1, -1, -1, -1, 16,
	-1, -1, -1, -1, 17, -1, 23, -1, -1, -1,
	-1, -1, 29, 18, -1, -1, -1, 32, 19, -1,
	-1, -1, 21, -1, -1, 22, -1, -1, -1, 20,
	-1, 30, 24, -1, -1, -1, -1, 25, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, 31, 26, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, 27, 28, -1, -1, 33
};

static struct hdmi_s3d_info {
	bool subsamp;
	int  structure;
	int  subsamp_pos;
} hdmi_s3d;

struct hdmi {
	struct kobject kobj;
	void __iomem *base_phy;
	void __iomem *base_pll;
	struct mutex lock;
	struct mutex lock_aux;
	int code;
	int mode;
	int deep_color;
	int lr_fr;
	int force_set;
	bool s3d_enabled;
	struct hdmi_config cfg;
	struct omap_display_platform_data *pdata;
	struct platform_device *pdev;
	void (*hdmi_start_frame_cb)(void);
	void (*hdmi_stop_frame_cb)(void);
} hdmi;

struct hdmi_cm {
	int code;
	int mode;
};
struct omap_video_timings edid_timings;

static void update_cfg(struct hdmi_config *cfg,
					struct omap_video_timings *timings)
{
	cfg->ppl = timings->x_res;
	cfg->lpp = timings->y_res;
	cfg->hbp = timings->hbp;
	cfg->hfp = timings->hfp;
	cfg->hsw = timings->hsw;
	cfg->vbp = timings->vbp;
	cfg->vfp = timings->vfp;
	cfg->vsw = timings->vsw;
	cfg->pixel_clock = timings->pixel_clock;
}

static void update_cfg_pol(struct hdmi_config *cfg, int  code)
{
	cfg->v_pol = hvpol_mapping[code].vsync_pol;
	cfg->h_pol = hvpol_mapping[code].hsync_pol;
}

static inline void hdmi_write_reg(u32 base, u16 idx, u32 val)
{
	void __iomem *b;

	switch (base) {
	case HDMI_PHY:
		b = hdmi.base_phy;
		break;
	case HDMI_PLLCTRL:
		b = hdmi.base_pll;
		break;
	default:
		BUG();
	}
	__raw_writel(val, b + idx);
	/* DBG("write = 0x%x idx =0x%x\r\n", val, idx); */
}

static inline u32 hdmi_read_reg(u32 base, u16 idx)
{
	void __iomem *b;
	u32 l;

	switch (base) {
	case HDMI_PHY:
		b = hdmi.base_phy;
		break;
	case HDMI_PLLCTRL:
		b = hdmi.base_pll;
		break;
	default:
		BUG();
	}
	l = __raw_readl(b + idx);

	/* DBG("addr = 0x%p rd = 0x%x idx = 0x%x\r\n", (b+idx), l, idx); */
	return l;
}

#define FLD_GET(val, start, end) (((val) & FLD_MASK(start, end)) >> (end))
#define FLD_MOD(orig, val, start, end) \
	(((orig) & ~FLD_MASK(start, end)) | FLD_VAL(val, start, end))

#define REG_FLD_MOD(b, i, v, s, e) \
	hdmi_write_reg(b, i, FLD_MOD(hdmi_read_reg(b, i), v, s, e))

static ssize_t hdmi_edid_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	memcpy(buf, edid, HDMI_EDID_MAX_LENGTH);
	return HDMI_EDID_MAX_LENGTH;
}

static ssize_t hdmi_edid_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t size)
{
	return 0;
}

static ssize_t hdmi_yuv_supported(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	bool enabled = hdmi_tv_yuv_supported(edid);
	return snprintf(buf, PAGE_SIZE, "%s\n", enabled ? "true" : "false");
}

static ssize_t hdmi_yuv_set(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t size)
{
	unsigned long yuv;
	int r = strict_strtoul(buf, 0, &yuv);
	if (r == 0)
		hdmi_configure_csc(yuv ? RGB_TO_YUV : RGB);
	return r ? : size;
}

static ssize_t hdmi_deepcolor_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", hdmi.deep_color);
}

static ssize_t hdmi_deepcolor_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t size)
{
	unsigned long deep_color;
	int r = strict_strtoul(buf, 0, &deep_color);
	if (r || deep_color > 2)
		return -EINVAL;
	hdmi.deep_color = deep_color;
	return size;
}

/*
 * This function is used to configure Limited range/full range
 * with RGB format , with YUV format Full range is not supported
 * Please refer to section 6.6 Video quantization ranges in HDMI 1.3a
 * specification for more details.
 * Now conversion to Full range or limited range can either be done at
 * display controller or HDMI IP ,This function allows to select either
 * Please note : To convert to full range it is better to convert the video
 * in the dispc to full range as there will be no loss of data , if a
 * limited range data is sent ot HDMI and converted to Full range in HDMI
 * the data quality would not be good.
 */
static void hdmi_configure_lr_fr(void)
{
	int ret = 0;
	if (hdmi.mode == 0 || (hdmi.mode == 1 && hdmi.code == 1)) {
		ret = hdmi_configure_lrfr(HDMI_FULL_RANGE, 0);
		if (!ret)
			dispc_setup_color_fr_lr(1);
		return;
	}
	if (hdmi.lr_fr) {
		ret = hdmi_configure_lrfr(HDMI_FULL_RANGE, hdmi.force_set);
		if (!ret && !hdmi.force_set)
			dispc_setup_color_fr_lr(1);
	} else {
		ret = hdmi_configure_lrfr(HDMI_LIMITED_RANGE, hdmi.force_set);
		if (!ret && !hdmi.force_set)
			dispc_setup_color_fr_lr(0);
	}
}

static ssize_t hdmi_lr_fr_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d:%d\n", hdmi.lr_fr, hdmi.force_set);
}

static ssize_t hdmi_lr_fr_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t size)
{
	int lr_fr, force_set = 0;
	if (!*buf || !strchr("yY1nN0", *buf))
		return -EINVAL;
	lr_fr = !!strchr("yY1", *buf++);
	if (*buf && *buf != '\n') {
		if (!strchr(" \t,:", *buf++) ||
		    !*buf || !strchr("yY1nN0", *buf))
			return -EINVAL;
		force_set = !!strchr("yY1", *buf++);
	}
	if (*buf && strcmp(buf, "\n"))
		return -EINVAL;
	hdmi.lr_fr = lr_fr;
	hdmi.force_set = force_set;
	hdmi_configure_lr_fr();
	return size;
}

static ssize_t audio_power_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", audio_on);
}

static ssize_t audio_power_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t size)
{
	int on;
	if (!*buf || !strchr("yY1nN0", *buf))
		return -EINVAL;
	on = !!strchr("yY1", *buf++);
	if (*buf && strcmp(buf, "\n"))
		return -EINVAL;

	hdmi_set_audio_power(on);
	return size;
}

static ssize_t hdmi_code_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct omap_dss_device *dssdev = to_dss_device(dev);
	struct hdmi_cm cm;

	cm = hdmi_get_code(&dssdev->panel.timings);

	return snprintf(buf, PAGE_SIZE, "%s:%u\n",
			cm.mode ? "CEA" : "VESA", cm.code);
}

static DEVICE_ATTR(edid, S_IRUGO, hdmi_edid_show, hdmi_edid_store);
static DEVICE_ATTR(yuv, S_IRUGO | S_IWUSR, hdmi_yuv_supported, hdmi_yuv_set);
static DEVICE_ATTR(deepcolor, S_IRUGO | S_IWUSR, hdmi_deepcolor_show,
							hdmi_deepcolor_store);
static DEVICE_ATTR(lr_fr, S_IRUGO | S_IWUSR, hdmi_lr_fr_show, hdmi_lr_fr_store);
static DEVICE_ATTR(audio_power, S_IRUGO | S_IWUSR, audio_power_show,
							audio_power_store);
static DEVICE_ATTR(code, S_IRUGO, hdmi_code_show, NULL);

static int set_hdmi_hot_plug_status(struct omap_dss_device *dssdev, bool onoff)
{
	int ret = 0;

	if (onoff != user_hpd_state) {
		hdmi_notify_hpd(onoff);
		DSSINFO("hot plug event %d", onoff);
		ret = kobject_uevent(&dssdev->dev.kobj,
					onoff ? KOBJ_ADD : KOBJ_REMOVE);
		if (ret)
			DSSWARN("error sending hot plug event %d (%d)",
								onoff, ret);
		/*
		 * TRICKY: we update status here as kobject_uevent seems to
		 * always return an error for now.
		 */
		user_hpd_state = onoff;
	}

	return ret;
}

static int hdmi_open(struct inode *inode, struct file *filp)
{
	return 0;
}

static int hdmi_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static struct omap_dss_device *get_hdmi_device(void)
{
	int match(struct omap_dss_device *dssdev, void *arg)
	{
		return sysfs_streq(dssdev->name , "hdmi");
	}

	return omap_dss_find_device(NULL, match);
}

static int hdmi_ioctl(struct inode *inode, struct file *file,
					  unsigned int cmd, unsigned long arg)
{
	struct omap_dss_device *dssdev = get_hdmi_device();
	int r = 0;

	if (dssdev == NULL)
		return -EIO;

	switch (cmd) {
	case HDMI_ENABLE:
		hdmi_enable_video(dssdev);
		break;
	case HDMI_DISABLE:
		hdmi_disable_video(dssdev);
		break;
	case HDMI_READ_EDID:
		hdmi_get_edid(dssdev);
		break;
	default:
		r = -EINVAL;
		DSSDBG("Un-recoganized command");
		break;
	}

	return r;
}
/*
 * refclk = (sys_clk/(highfreq+1))/(n+1)
 * so refclk = 38.4/2/(n+1) = 19.2/(n+1)
 * choose n = 15, makes refclk = 1.2
 *
 * m = tclk/cpf*refclk = tclk/2*1.2
 *
 *	for clkin = 38.2/2 = 192
 *	    phy = 2520
 *
 *	m = 2520*16/2* 192 = 105;
 *
 *	for clkin = 38.4
 *	    phy = 2520
 *
 */

struct hdmi_pll_info {
	u16 regn;
	u16 regm;
	u32 regmf;
	u16 regm2;
	u16 regsd;
	u16 dcofreq;
};

static inline void print_omap_video_timings(struct omap_video_timings *timings)
{
	extern unsigned int dss_debug;
	if (dss_debug) {
		printk(KERN_INFO "Timing Info:\n");
		printk(KERN_INFO "  pixel_clk = %d\n", timings->pixel_clock);
		printk(KERN_INFO "  x_res     = %d\n", timings->x_res);
		printk(KERN_INFO "  y_res     = %d\n", timings->y_res);
		printk(KERN_INFO "  hfp       = %d\n", timings->hfp);
		printk(KERN_INFO "  hsw       = %d\n", timings->hsw);
		printk(KERN_INFO "  hbp       = %d\n", timings->hbp);
		printk(KERN_INFO "  vfp       = %d\n", timings->vfp);
		printk(KERN_INFO "  vsw       = %d\n", timings->vsw);
		printk(KERN_INFO "  vbp       = %d\n", timings->vbp);
	}
}

static void compute_pll(int clkin, int phy,
	int n, struct hdmi_pll_info *pi)
{
	int refclk;
	u32 temp, mf;

	refclk = clkin / (n + 1);

	temp = phy * 100/(refclk);

	pi->regn = n;
	pi->regm = temp/100;
	pi->regm2 = 1;

	mf = (phy - pi->regm * refclk) * 262144;
	pi->regmf = mf/(refclk);

	if (phy > 1000 * 100) {
		pi->dcofreq = 1;
	} else {
		pi->dcofreq = 0;
	}

	pi->regsd = ((pi->regm * clkin / 10) / ((n + 1) * 250) + 5) / 10;

	DSSDBG("M = %d Mf = %d\n", pi->regm, pi->regmf);
	DSSDBG("range = %d sd = %d\n", pi->dcofreq, pi->regsd);
}

static int hdmi_pll_init(int refsel, int dcofreq, struct hdmi_pll_info *fmt,
									u16 sd)
{
	u32 r;
	unsigned t = 500000;
	u32 pll = HDMI_PLLCTRL;

	/* PLL start always use manual mode */
	REG_FLD_MOD(pll, PLLCTRL_PLL_CONTROL, 0x0, 0, 0);

	r = hdmi_read_reg(pll, PLLCTRL_CFG1);
	r = FLD_MOD(r, fmt->regm, 20, 9); /* CFG1__PLL_REGM */
	r = FLD_MOD(r, fmt->regn, 8, 1);  /* CFG1__PLL_REGN */

	hdmi_write_reg(pll, PLLCTRL_CFG1, r);

	r = hdmi_read_reg(pll, PLLCTRL_CFG2);

	r = FLD_MOD(r, 0x0, 12, 12); /* PLL_HIGHFREQ divide by 2 */
	r = FLD_MOD(r, 0x1, 13, 13); /* PLL_REFEN */
	r = FLD_MOD(r, 0x0, 14, 14); /* PHY_CLKINEN de-assert during locking */

	if (dcofreq) {
		/* divider programming for 1080p */
		REG_FLD_MOD(pll, PLLCTRL_CFG3, sd, 17, 10);
		r = FLD_MOD(r, 0x4, 3, 1); /* 1000MHz and 2000MHz */
	} else {
		r = FLD_MOD(r, 0x2, 3, 1); /* 500MHz and 1000MHz */
	}

	hdmi_write_reg(pll, PLLCTRL_CFG2, r);

	r = hdmi_read_reg(pll, PLLCTRL_CFG4);
	r = FLD_MOD(r, fmt->regm2, 24, 18);
	r = FLD_MOD(r, fmt->regmf, 17, 0);

	hdmi_write_reg(pll, PLLCTRL_CFG4, r);

	/* go now */
	REG_FLD_MOD(pll, PLLCTRL_PLL_GO, 0x1ul, 0, 0);

	/* wait for bit change */
	while (FLD_GET(hdmi_read_reg(pll, PLLCTRL_PLL_GO), 0, 0))
		;

	/* Wait till the lock bit is set */
	/* read PLL status */
	while (0 == FLD_GET(hdmi_read_reg(pll, PLLCTRL_PLL_STATUS), 1, 1)) {
		udelay(1);
		if (!--t) {
			printk(KERN_WARNING "HDMI: cannot lock PLL\n");
			DSSDBG("CFG1 0x%x\n", hdmi_read_reg(pll, PLLCTRL_CFG1));
			DSSDBG("CFG2 0x%x\n", hdmi_read_reg(pll, PLLCTRL_CFG2));
			DSSDBG("CFG4 0x%x\n", hdmi_read_reg(pll, PLLCTRL_CFG4));
			return -EIO;
		}
	}

	DSSDBG("PLL locked!\n");

	return 0;
}

static int hdmi_pll_reset(void)
{
	int t = 0;

	/* SYSREEST  controled by power FSM*/
	REG_FLD_MOD(HDMI_PLLCTRL, PLLCTRL_PLL_CONTROL, 0x0, 3, 3);

	/* READ 0x0 reset is in progress */
	while (!FLD_GET(hdmi_read_reg(HDMI_PLLCTRL,
			PLLCTRL_PLL_STATUS), 0, 0)) {
		udelay(1);
		if (t++ > 1000) {
			ERR("Failed to sysrest PLL\n");
			return -ENODEV;
		}
	}

	return 0;
}

static int hdmi_pll_program(struct hdmi_pll_info *fmt)
{
	u32 r;
	int refsel;

	HDMI_PllPwr_t PllPwrWaitParam;

	/* wait for wrapper rest */
	HDMI_W1_SetWaitSoftReset();

	/* power off PLL */
	PllPwrWaitParam = HDMI_PLLPWRCMD_ALLOFF;
	r = HDMI_W1_SetWaitPllPwrState(HDMI_WP, PllPwrWaitParam);
	if (r)
		return r;

	/* power on PLL */
	PllPwrWaitParam = HDMI_PLLPWRCMD_BOTHON_ALLCLKS;
	r = HDMI_W1_SetWaitPllPwrState(HDMI_WP, PllPwrWaitParam);
	if (r)
		return r;

	hdmi_pll_reset();

	refsel = 0x3; /* select SYSCLK reference */

	r = hdmi_pll_init(refsel, fmt->dcofreq, fmt, fmt->regsd);

	return r;
}

/* double check the order */
static int hdmi_phy_init(u32 w1,
		u32 phy, int tmds)
{
	int r;

	/* wait till PHY_PWR_STATUS=LDOON */
	/* HDMI_PHYPWRCMD_LDOON = 1 */
	r = HDMI_W1_SetWaitPhyPwrState(w1, 1);
	if (r)
		return r;

	/* wait till PHY_PWR_STATUS=TXON */
	r = HDMI_W1_SetWaitPhyPwrState(w1, 2);
	if (r)
		return r;

	/* read address 0 in order to get the SCPreset done completed */
	/* Dummy access performed to solve resetdone issue */
	hdmi_read_reg(phy, HDMI_TXPHY_TX_CTRL);

	/* write to phy address 0 to configure the clock */
	/* use HFBITCLK write HDMI_TXPHY_TX_CONTROL__FREQOUT field */
	REG_FLD_MOD(phy, HDMI_TXPHY_TX_CTRL, tmds, 31, 30);

	/* write to phy address 1 to start HDMI line (TXVALID and TMDSCLKEN) */
	hdmi_write_reg(phy, HDMI_TXPHY_DIGITAL_CTRL, 0xF0000000);

	/*  write to phy address 3 to change the polarity control  */
	REG_FLD_MOD(phy, HDMI_TXPHY_PAD_CFG_CTRL, 0x1, 27, 27);

	return 0;
}

static int hdmi_phy_off(u32 name)
{
	int r = 0;

	/* wait till PHY_PWR_STATUS=OFF */
	/* HDMI_PHYPWRCMD_OFF = 0 */
	r = HDMI_W1_SetWaitPhyPwrState(name, 0);
	if (r)
		return r;

	return 0;
}

static int get_s3d_timings_index(void)
{
	int code;

	code = s3d_code_cea[hdmi.code];

	if (code == -1) {
		hdmi.s3d_enabled = false;
		code = 9;
		hdmi.code = 16;
		hdmi.mode = 1;
	}
	return code;
}
/* driver */
static int get_timings_index(void)
{
	int code;

	if (hdmi.mode == 0)
		code = code_vesa[hdmi.code];
	else
		code = code_cea[hdmi.code];

	if (code == -1 || all_timings_direct[code].x_res >= 2048) {
		code = 9;
		hdmi.code = 16;
		hdmi.mode = 1;
	}
	return code;
}

static int hdmi_panel_probe(struct omap_dss_device *dssdev)
{
	int code;
	printk(KERN_DEBUG "ENTER hdmi_panel_probe()\n");

	dssdev->panel.config = OMAP_DSS_LCD_TFT |
			OMAP_DSS_LCD_IVS | OMAP_DSS_LCD_IHS;
	hdmi.deep_color = 0;
	hdmi.lr_fr = HDMI_LIMITED_RANGE;
	code = get_timings_index();

	dssdev->panel.timings = all_timings_direct[code];
	printk(KERN_INFO "hdmi_panel_probe x_res= %d y_res = %d", \
		dssdev->panel.timings.x_res, dssdev->panel.timings.y_res);

	mdelay(50);

	return 0;
}

static void hdmi_panel_remove(struct omap_dss_device *dssdev)
{
}

static bool hdmi_panel_is_enabled(struct omap_dss_device *dssdev)
{
	return video_power == HDMI_POWER_FULL;
}

static int hdmi_panel_enable(struct omap_dss_device *dssdev)
{
	hdmi_enable_video(dssdev);
	return 0;
}

static void hdmi_panel_disable(struct omap_dss_device *dssdev)
{
	hdmi_disable_video(dssdev);
}

static int hdmi_panel_suspend(struct omap_dss_device *dssdev)
{
	hdmi_suspend_video(dssdev);
	return 0;
}

static int hdmi_panel_resume(struct omap_dss_device *dssdev)
{
	hdmi_resume_video(dssdev);
	return 0;
}

static void hdmi_enable_clocks(int enable)
{
	if (enable)
		dss_clk_enable(DSS_CLK_ICK | DSS_CLK_FCK1 | DSS_CLK_54M |
				DSS_CLK_96M);
	else
		dss_clk_disable(DSS_CLK_ICK | DSS_CLK_FCK1 | DSS_CLK_54M |
				DSS_CLK_96M);
}

static struct omap_dss_driver hdmi_driver = {
	.probe		= hdmi_panel_probe,
	.remove		= hdmi_panel_remove,

	.disable	= hdmi_panel_disable,
	.smart_enable	= hdmi_panel_enable,
	.smart_is_enabled	= hdmi_panel_is_enabled,
	.suspend	= hdmi_panel_suspend,
	.resume		= hdmi_panel_resume,
	.get_timings	= hdmi_get_timings,
	.set_timings	= hdmi_set_timings,
	.check_timings	= hdmi_check_timings,
	.get_edid	= hdmi_get_edid,
	.set_custom_edid_timing_code	= hdmi_set_custom_edid_timing_code,
	.hpd_enable	=	hdmi_enable_hpd,
	.reset		= hdmi_reset,

	.enable_s3d = hdmi_enable_s3d,
	.get_s3d_enabled = hdmi_get_s3d_enabled,
	.set_s3d_disp_type = hdmi_set_s3d_disp_type,

	.driver			= {
		.name   = "hdmi_panel",
		.owner  = THIS_MODULE,
	},
};
/* driver end */

int hdmi_init(struct platform_device *pdev)
{
	int r = 0, hdmi_irq;
	struct resource *hdmi_mem;
	printk(KERN_INFO "Enter hdmi_init()\n");

	hdmi_s3d.structure = HDMI_S3D_FRAME_PACKING;
	hdmi_s3d.subsamp = false;
	hdmi_s3d.subsamp_pos = 0;
	hdmi.s3d_enabled = false;

	hdmi.pdata = pdev->dev.platform_data;
	hdmi.pdev = pdev;
	mutex_init(&hdmi.lock);
	mutex_init(&hdmi.lock_aux);

	hdmi_mem = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	hdmi.base_pll = ioremap(hdmi_mem->start + 0x200,
						resource_size(hdmi_mem));
	if (!hdmi.base_pll) {
		ERR("can't ioremap pll\n");
		return -ENOMEM;
	}
	hdmi.base_phy = ioremap((hdmi_mem->start + 0x300), 64);

	if (!hdmi.base_phy) {
		ERR("can't ioremap phy\n");
		return -ENOMEM;
	}

	hdmi_enable_clocks(1);

	hdmi_lib_init();

	hdmi.hdmi_start_frame_cb = 0;
	hdmi.hdmi_stop_frame_cb = 0;

	hdmi_enable_clocks(0);
	/* Get the major number for this module */
	r = alloc_chrdev_region(&hdmi_dev_id, 0, 1, "hdmi_panel");
	if (r) {
		printk(KERN_WARNING "HDMI: Cound not register character device\n");
		return -ENOMEM;
	}

	hdmi_major = MAJOR(hdmi_dev_id);

	/* initialize character device */
	cdev_init(&hdmi_cdev, &hdmi_fops);

	hdmi_cdev.owner = THIS_MODULE;
	hdmi_cdev.ops = &hdmi_fops;

	/* add char driver */
	r = cdev_add(&hdmi_cdev, hdmi_dev_id, 1);
	if (r) {
		printk(KERN_WARNING "HDMI: Could not add hdmi char driver\n");
		unregister_chrdev_region(hdmi_dev_id, 1);
		return -ENOMEM;
	}

	irq_wq = create_singlethread_workqueue("HDMI WQ");

	hdmi_irq = platform_get_irq(pdev, 0);
	r = request_irq(hdmi_irq, hdmi_irq_handler, 0, "OMAP HDMI", (void *)0);

	return omap_dss_register_driver(&hdmi_driver);
}

void hdmi_exit(void)
{
	hdmi_lib_exit();
	destroy_workqueue(irq_wq);
	free_irq(OMAP44XX_IRQ_DSS_HDMI, NULL);
	iounmap(hdmi.base_pll);
	iounmap(hdmi.base_phy);
}

static int hdmi_power_on(struct omap_dss_device *dssdev)
{
	int r = 0;
	int code = 0;
	int dirty;
	struct omap_video_timings *p;
	struct hdmi_pll_info pll_data;
	struct deep_color *vsdb_format = NULL;
	int clkin, n, phy = 0, max_tmds = 0, temp = 0, tmds_freq;

	hdmi_power = HDMI_POWER_FULL;
	code = get_timings_index();
	dssdev->panel.timings = all_timings_direct[code];

	hdmi_enable_clocks(1);

	p = &dssdev->panel.timings;

	if (!custom_set) {
		code = get_timings_index();

		DSSDBG("No edid set thus will be calling hdmi_read_edid");
		r = hdmi_read_edid(p);
		if (r) {
			r = -EIO;
			goto err;
		}

		vsdb_format = kzalloc(sizeof(*vsdb_format), GFP_KERNEL);
		hdmi_deep_color_support_info(edid, vsdb_format);
		DSSINFO("deep_color_bit30=%d bit36=%d max_tmds_freq=%d\n",
			vsdb_format->bit_30, vsdb_format->bit_36,
			vsdb_format->max_tmds_freq);
		max_tmds = vsdb_format->max_tmds_freq * 500;

		dirty = get_timings_index() != code;
	} else {
		dirty = true;
	}

	update_cfg(&hdmi.cfg, p);

	if (hdmi.s3d_enabled && (hdmi_s3d.structure == HDMI_S3D_FRAME_PACKING))
		code = get_s3d_timings_index();
	else
		code = get_timings_index();
	update_cfg_pol(&hdmi.cfg, code);

	dssdev->panel.timings = all_timings_direct[code];

	DSSINFO("%s:%d res=%dx%d ", hdmi.mode ? "CEA" : "VESA", hdmi.code,
		dssdev->panel.timings.x_res, dssdev->panel.timings.y_res);

	clkin = 3840; /* 38.4 mHz */
	n = 15; /* this is a constant for our math */

	switch (hdmi.deep_color) {
	case 1:
		temp = (p->pixel_clock * 125) / 100 ;
		if (!custom_set) {
			if (vsdb_format->bit_30) {
				if (max_tmds != 0 && max_tmds >= temp)
					phy = temp;
			} else {
				printk(KERN_ERR "TV does not support Deep color");
				goto err;
			}
		} else {
			phy = temp;
		}
		hdmi.cfg.deep_color = 1;
		break;
	case 2:
		if (p->pixel_clock == 148500) {
			printk(KERN_ERR"36 bit deep color not supported");
			goto err;
		}

		temp = (p->pixel_clock * 150) / 100;
		if (!custom_set) {
			if (vsdb_format->bit_36) {
				if (max_tmds != 0 && max_tmds >= temp)
					phy = temp;
			} else {
				printk(KERN_ERR "TV does not support Deep color");
				goto err;
			}
		} else {
			phy = temp;
		}
		hdmi.cfg.deep_color = 2;
		break;
	case 0:
	default:
		phy = p->pixel_clock;
		hdmi.cfg.deep_color = 0;
		break;
	}

	compute_pll(clkin, phy, n, &pll_data);

	HDMI_W1_StopVideoFrame(HDMI_WP);

	dispc_enable_digit_out(0);

	if (dirty)
		omap_dss_notify(dssdev, OMAP_DSS_SIZE_CHANGE);

	/* config the PLL and PHY first */
	r = hdmi_pll_program(&pll_data);
	if (r) {
		DSSERR("Failed to lock PLL\n");
		r = -EIO;
		goto err;
	}

	/* TMDS freq_out in the PHY should be set based on the TMDS clock */
	if (phy <= 50000)
		tmds_freq = 0x0;
	else if ((phy > 50000) && (phy <= 100000))
		tmds_freq = 0x1;
	else
		tmds_freq = 0x2;

	r = hdmi_phy_init(HDMI_WP, HDMI_PHY, tmds_freq);
	if (r) {
		DSSERR("Failed to start PHY\n");
		r = -EIO;
		goto err;
	}

	if (hdmi.s3d_enabled) {
		hdmi.cfg.vsi_enabled = true;
		hdmi.cfg.s3d_structure = hdmi_s3d.structure;
		hdmi.cfg.subsamp_pos = hdmi_s3d.subsamp_pos;
	} else {
		hdmi.cfg.vsi_enabled = false;
	}

	hdmi.cfg.hdmi_dvi = hdmi.mode;
	hdmi.cfg.video_format = hdmi.code;
	hdmi.cfg.supports_ai = hdmi_ai_supported(edid);

	if ((hdmi.mode)) {
		switch (hdmi.code) {
		case 20:
		case 5:
		case 6:
		case 21:
			hdmi.cfg.interlace = 1;
			break;
		default:
			hdmi.cfg.interlace = 0;
			break;
		}
	}

	hdmi_lib_enable(&hdmi.cfg);

	hdmi_configure_lr_fr();

	/* these settings are independent of overlays */
	dss_switch_tv_hdmi(1);

	/* bypass TV gamma table */
	dispc_enable_gamma_table(0);

	/* allow idle mode */
	dispc_set_idle_mode();

#ifndef CONFIG_OMAP4_ES1
	/*The default reset value for DISPC.DIVISOR1 LCD is 4
	* in ES2.0 and the clock will run at 1/4th the speed
	* resulting in the sync_lost_digit */
	dispc_set_tv_divisor();
#endif

	/* tv size */
	dispc_set_digit_size(dssdev->panel.timings.x_res,
			dssdev->panel.timings.y_res);

	HDMI_W1_StartVideoFrame(HDMI_WP);

	dispc_enable_digit_out(1);
	hdmi_set_irqs(0);

	if (hdmi.hdmi_start_frame_cb)
		(*hdmi.hdmi_start_frame_cb)();

	kfree(vsdb_format);

	return 0;
err:
	kfree(vsdb_format);
	return r;
}

static int hdmi_min_enable(void)
{
	int r;
	DSSDBG("hdmi_min_enable");

	hdmi_set_irqs(1);
	hdmi_enable_clocks(0);
	hdmi_power = HDMI_POWER_MIN;
	r = hdmi_phy_init(HDMI_WP, HDMI_PHY, 0);
	if (r)
		DSSERR("Failed to start PHY\n");

	if (hdmi.s3d_enabled) {
		hdmi.cfg.vsi_enabled = true;
		hdmi.cfg.s3d_structure = hdmi_s3d.structure;
		hdmi.cfg.subsamp_pos = hdmi_s3d.subsamp_pos;
	} else {
		hdmi.cfg.vsi_enabled = false;
	}

	hdmi.cfg.hdmi_dvi = hdmi.mode;
	hdmi.cfg.video_format = hdmi.code;
	hdmi_lib_enable(&hdmi.cfg);
	return 0;
}

static int hdmi_audio_power_off(void)
{
	int res;
retry:
	/* wait for audio to suspend */
	if (audio_on) {
		mutex_unlock(&hdmi.lock_aux);

		/* signal suspend request to audio */
		hdmi_notify_pwrchange(HDMI_EVENT_POWEROFF);

		/* allow audio power to go off */
		res = wait_event_interruptible_timeout(audio_wq,
			!audio_on, msecs_to_jiffies(1000));
		mutex_lock(&hdmi.lock_aux);
		if (res <= 0) {
			printk(KERN_ERR "HDMI audio power is not released within 1sec\n");
			return -EBUSY;
		}
		goto retry;
	}

	return 0;
}

static int hdmi_reconfigure(struct omap_dss_device *dssdev)
{
	/* don't reconfigure in power off state */
	if (hdmi_power == HDMI_POWER_OFF)
		return -EINVAL;

	hdmi_enable_clocks(1);  /* ??? */

	/* force a new power-up to read EDID */
	if (hdmi_power == HDMI_POWER_FULL)
		hdmi_power = HDMI_POWER_MIN;

	return set_video_power(dssdev, HDMI_POWER_FULL);
}

static void hdmi_notify_queue(struct work_struct *work)
{
	struct omap_dss_device *dssdev = NULL;
	dssdev = get_hdmi_device();

	mutex_lock(&hdmi.lock);
	mutex_lock(&hdmi.lock_aux);
	if (!hot_plug_notify_canceled && !edid_set) {
		/* force EDID read */
		DSSINFO("Read EDID\n");
		hdmi_reconfigure(dssdev);

		/* force hpd event if edid is read the first time */
		if (edid_set)
			user_hpd_state = false;
	}

	if (!hot_plug_notify_canceled && !user_hpd_state)
		set_hdmi_hot_plug_status(dssdev, true);
	mutex_unlock(&hdmi.lock_aux);
	mutex_unlock(&hdmi.lock);
}

static void cancel_hot_plug_notify_work(void)
{
	if (!delayed_work_pending(&hot_plug_notify_work))
		return;

	/* cancel auto-notify */
	hot_plug_notify_canceled = true;
	if (cancel_delayed_work(&hot_plug_notify_work) != 1)
		flush_delayed_work(&hot_plug_notify_work);
	hot_plug_notify_canceled = false;
}

static int hdmi_is_connected(void)
{
	return hdmi_rxdet();
}

static void hdmi_work_queue(struct work_struct *ws)
{
	struct hdmi_work_struct *work =
			container_of(ws, struct hdmi_work_struct, work);
	struct omap_dss_device *dssdev = get_hdmi_device();
	int r = work->r;
	unsigned long time;
	static ktime_t last_connect, last_disconnect;
	bool notify;
	int action = 0;

	mutex_lock(&hdmi.lock);
	mutex_lock(&hdmi.lock_aux);
	if (dssdev == NULL || dssdev->state != OMAP_DSS_DISPLAY_ACTIVE)
		/* HDMI is disabled, there cannot be any HDMI irqs */
		goto done;

	DSSDBG("irqstatus=%08x\n hdmi/vid_power=%d/%d",
		r, hdmi_power, video_power);

	DSSDBG("irqstatus=%08x\n dssdev->state = %d, "
		"hdmi_power = %d", r, dssdev->state, hdmi_power);
	if (r & (HDMI_CONNECT | HDMI_DISCONNECT))
		action = (hdmi_is_connected()) ? HDMI_CONNECT : HDMI_DISCONNECT;

	if (action & HDMI_DISCONNECT) {
		/* cancel auto-notify */
		mutex_unlock(&hdmi.lock_aux);
		mutex_unlock(&hdmi.lock);
		cancel_hot_plug_notify_work();
		mutex_lock(&hdmi.lock);
		mutex_lock(&hdmi.lock_aux);
	}

	if ((action & HDMI_DISCONNECT) && !(r & HDMI_IN_RESET) &&
	    (hdmi_power == HDMI_POWER_FULL)) {
		/*
		 * Wait at least 100ms after HDMI_CONNECT to decide if
		 * cable is really disconnected
		 */
		last_disconnect = ktime_get();
		time = ktime_to_ms(ktime_sub(last_disconnect, last_connect));
		if (time < 100)
			mdelay(100 - time);
		if (hdmi_connected)
			goto done;

		/* turn audio power off */
		notify = !audio_on; /* notification is sent if audio is on */
		hdmi_audio_power_off();

		set_hdmi_hot_plug_status(dssdev, false);
		/* ignore return value for now */

		mutex_unlock(&hdmi.lock_aux);
		mutex_unlock(&hdmi.lock);
		mdelay(100);
		mutex_lock(&hdmi.lock);
		if (notify)
			hdmi_notify_pwrchange(HDMI_EVENT_POWEROFF);
		mutex_lock(&hdmi.lock_aux);

		if (dssdev->state != OMAP_DSS_DISPLAY_ACTIVE)
			/* HDMI is disabled, no need to process */
			goto done;

		DSSINFO("Display disabled\n");
		HDMI_W1_StopVideoFrame(HDMI_WP);
		if (dssdev->platform_disable)
			dssdev->platform_disable(dssdev);
			dispc_enable_digit_out(0);

		if (hdmi.hdmi_stop_frame_cb)
			(*hdmi.hdmi_stop_frame_cb)();

		HDMI_W1_SetWaitPllPwrState(HDMI_WP, HDMI_PLLPWRCMD_ALLOFF);
		edid_set = custom_set = false;
		set_video_power(dssdev, HDMI_POWER_MIN);
	}

	/* read connect timestamp */
	if (action & HDMI_CONNECT)
		last_connect = ktime_get();

	if (action & HDMI_CONNECT) {
		if (!edid_set && !custom_set) {
			/*
			 * Schedule hot-plug-notify in 1 sec in case no HPD
			 * event gets generated
			 */
			if (!delayed_work_pending(&hot_plug_notify_work))
				queue_delayed_work(irq_wq,
						&hot_plug_notify_work,
						msecs_to_jiffies(1000));
		} else {
			if (custom_set)
				user_hpd_state = false;

			if (!user_hpd_state && hdmi_power == HDMI_POWER_FULL)
				set_hdmi_hot_plug_status(dssdev, true);
		}
	}

	if ((action & HDMI_CONNECT) && (video_power == HDMI_POWER_MIN) &&
		(hdmi_power != HDMI_POWER_FULL)) {

		DSSINFO("Physical Connect\n");

		/* turn on clocks on connect */
		hdmi_reconfigure(dssdev);
		mutex_unlock(&hdmi.lock_aux);
		hdmi_notify_pwrchange(HDMI_EVENT_POWERON);
		mutex_unlock(&hdmi.lock);
		mdelay(100);
		mutex_lock(&hdmi.lock);
		mutex_lock(&hdmi.lock_aux);
		if (dssdev->state != OMAP_DSS_DISPLAY_ACTIVE)
			/* HDMI is disabled, no need to process */
			goto done;
	}

done:

	if ((r & HDMI_FIRST_HPD) && (!edid_set) && (!custom_set)) {
		mutex_unlock(&hdmi.lock_aux);
		mutex_unlock(&hdmi.lock);
		/* cancel auto-notify - must be done outside mutex */
		cancel_hot_plug_notify_work();

		mdelay(100);
		mutex_lock(&hdmi.lock);
		mutex_lock(&hdmi.lock_aux);

		if (hdmi_power != HDMI_POWER_FULL || !hdmi_connected) {
			DSSINFO("irqstatus=0x%08x ignoring FIRST_HPD when "
				"hdmi_connected = %d, hdmi_power = %d\n",
				r, hdmi_connected, hdmi_power);
			goto done;
		}
		/*
		 * HDMI should already be full on. We use this to read EDID
		 * the first time we enable HDMI via HPD.
		 */
		DSSINFO("Connect 1 - Enabling display\n");
		hdmi_reconfigure(dssdev);

		set_hdmi_hot_plug_status(dssdev, true);
		/* ignore return value for now */
	}

	if (r & HDMI_HPD_MODIFY) {
		/*
		 * HDMI HPD state low followed by a HPD state high
		 * with more than 100ms duration is recieved so EDID should
		 * reread and HDMI reconfigued.
		 */
		DSSINFO("HDMI HPD  display\n");
		/* force a new power-up to read EDID */
		edid_set = false;
		custom_set = false;
		hdmi_reconfigure(dssdev);
		set_hdmi_hot_plug_status(dssdev, true);
		/* ignore return value for now */
	}

	mutex_unlock(&hdmi.lock_aux);
	mutex_unlock(&hdmi.lock);
	kfree(work);
}

static inline void hdmi_handle_irq_work(int r)
{
	struct hdmi_work_struct *work;
	if (r) {
		work = kmalloc(sizeof(*work), GFP_ATOMIC);

		if (work) {
			INIT_WORK(&work->work, hdmi_work_queue);
			work->r = r;
			queue_work(irq_wq, &work->work);
		} else {
			printk(KERN_ERR "Cannot allocate memory to create work");
		}
	}
}

static irqreturn_t hdmi_irq_handler(int irq, void *arg)
{
	unsigned long flags;
	int r = 0;

	/* process interrupt in critical section to handle conflicts */
	spin_lock_irqsave(&irqstatus_lock, flags);

	HDMI_W1_HPD_handler(&r);
	DSSDBG("Received IRQ r=%08x\n", r);

	if (((r & HDMI_CONNECT) || (r & HDMI_FIRST_HPD)) &&
						video_power == HDMI_POWER_MIN)
		hdmi_enable_clocks(1);
	if (r & HDMI_CONNECT)
		hdmi_connected = true;
	if (r & HDMI_DISCONNECT)
		hdmi_connected = false;

	spin_unlock_irqrestore(&irqstatus_lock, flags);

	hdmi_handle_irq_work(r | in_reset);

	return IRQ_HANDLED;
}

static void hdmi_power_off_phy(struct omap_dss_device *dssdev)
{
	edid_set = false;

	HDMI_W1_StopVideoFrame(HDMI_WP);
	hdmi_set_irqs(1);

	dispc_enable_digit_out(0);

	if (hdmi.hdmi_stop_frame_cb)
		(*hdmi.hdmi_stop_frame_cb)();

	hdmi_phy_off(HDMI_WP);

	HDMI_W1_SetWaitPllPwrState(HDMI_WP, HDMI_PLLPWRCMD_ALLOFF);
}

static void hdmi_power_off(struct omap_dss_device *dssdev)
{
	set_hdmi_hot_plug_status(dssdev, false);
	/* ignore return value for now */

	/*
	 * WORKAROUND: wait before turning off HDMI.  This may give
	 * audio/video enough time to stop operations.  However, if
	 * user reconnects HDMI, response will be delayed.
	 */
	mdelay(100); /* Tunning the delay*/

	dssdev->state = OMAP_DSS_DISPLAY_DISABLED;
	hdmi_power_off_phy(dssdev);

	if (dssdev->platform_disable)
		dssdev->platform_disable(dssdev);

	hdmi_enable_clocks(0);

	/* cut clock(s) */
	dss_mainclk_state_disable(true);

	/* reset to default */
}

static int hdmi_start_display(struct omap_dss_device *dssdev)
{
	/* the tv overlay manager is shared*/
	int r = omap_dss_start_device(dssdev);
	if (r) {
		DSSERR("failed to start device\n");
		goto err;
	}

	/* PAD0_HDMI_HPD_PAD1_HDMI_CEC */
	omap_writel(0x01100110, 0x4A100098);
	/* PAD0_HDMI_DDC_SCL_PAD1_HDMI_DDC_SDA */
	omap_writel(0x01100110 , 0x4A10009C);
	/* CONTROL_HDMI_TX_PHY */
	omap_writel(0x10000000, 0x4A100610);

	if (dssdev->platform_enable)
		dssdev->platform_enable(dssdev);

	/* enable clock(s) */
	dssdev->state = OMAP_DSS_DISPLAY_ACTIVE;
	dss_mainclk_state_enable();
	if (cpu_is_omap44xx()) {
		if (!hdmi_opt_clk_state) {
			r = hdmi_opt_clock_enable();
			if (!r)
				hdmi_opt_clk_state = true;
		}
	}
err:
	return r;
}

/*
 * set power state depending on device state and HDMI state.
 *
 * v_power, v_state is the desired video power/device state
 * a_power, a_state is the desired audio power/device state
 *
 * v_state and a_state is automatically adjusted to the
 * correct state unless it is HDMI_SUSPENDED.  This can be
 * used to keep the current suspend state by passing the old
 * v_state and/or a_state values in.
 */
static int hdmi_set_power(struct omap_dss_device *dssdev,
	enum hdmi_power_state v_power, bool suspend, bool _audio_on)
{
	int r = 0, old_power;
	enum hdmi_power_state power_need;
	enum hdmi_dev_state state_need;

	/* calculate HDMI combined power and state */
	if (_audio_on)
		power_need = HDMI_POWER_FULL;
	else
		power_need = suspend ? HDMI_POWER_OFF : v_power;
	state_need = power_need ? HDMI_ACTIVE :
				suspend ? HDMI_SUSPENDED : HDMI_DISABLED;

	DSSINFO("powerchg (a=%d,v=%d,s=%d/%d)->(a=%d,v=%d,s=%d) = (%d:%c)\n",
		audio_on, video_power, v_suspended,
		dssdev->state == HDMI_SUSPENDED, _audio_on, v_power, suspend,
		power_need, (state_need == HDMI_ACTIVE ? 'A' :
			     state_need == HDMI_SUSPENDED ? 'S' : 'D'));

	old_power = hdmi_power;

	/* turn on HDMI */
	if (dssdev->state != OMAP_DSS_DISPLAY_ACTIVE && power_need)
		r = hdmi_start_display(dssdev);

	video_power = v_power;
	v_suspended = suspend;
	audio_on = _audio_on;

	if (hdmi_power != power_need && r == 0) {
		if (power_need == HDMI_POWER_FULL) {
			r = hdmi_power_on(dssdev);
		} else if (power_need == HDMI_POWER_MIN) {
			r = hdmi_min_enable();
		} else {
			omap_dss_stop_device(dssdev);
			hdmi_power_off(dssdev);
		}
		if (!r)
			hdmi_power = power_need;
	}

	if (r == 0) {
		dssdev->state = state_need;
		dssdev->activate_after_resume = v_power || audio_on;
	}

	DSSINFO("pwrchanged => (%d,%c) = %d\n",
		power_need, (state_need == HDMI_ACTIVE ? 'A' :
			     state_need == HDMI_SUSPENDED ? 'S' : 'D'), r);

	if (cpu_is_omap44xx()) {
		if (power_need == HDMI_POWER_FULL) {
			if (!hdmi_opt_clk_state) {
				r = hdmi_opt_clock_enable();
				if (!r)
					hdmi_opt_clk_state = true;
			}
		} else {
			if (hdmi_opt_clk_state) {
				hdmi_opt_clock_disable();
				hdmi_opt_clk_state = false;
			}
		}
	}

	return r;
}

static int set_video_power(struct omap_dss_device *dssdev,
						enum hdmi_power_state v_power)
{
	return hdmi_set_power(dssdev, v_power, v_suspended, audio_on);
}

int hdmi_set_audio_power(bool _audio_on)
{
	struct omap_dss_device *dssdev = get_hdmi_device();
	bool notify;
	int r;

	if (!dssdev)
		return -EPERM;

	mutex_lock(&hdmi.lock_aux);
	/*
	 * For now we don't broadcase HDMI_EVENT_POWEROFF if audio is the last
	 * one turning off HDMI to prevent a possibility for an infinite loop.
	 */
	notify = _audio_on && (hdmi_power != HDMI_POWER_FULL);
	r = hdmi_set_power(dssdev, video_power, v_suspended, _audio_on);
	mutex_unlock(&hdmi.lock_aux);
	if (notify)
		hdmi_notify_pwrchange(HDMI_EVENT_POWERON);
	wake_up_interruptible(&audio_wq);

	return r;
}

static int hdmi_enable_hpd(struct omap_dss_device *dssdev)
{
	int r;
	DSSDBG("ENTER hdmi_enable_hpd()\n");

	/* use at least min power, keep suspended state if set */
	mutex_lock(&hdmi.lock);
	mutex_lock(&hdmi.lock_aux);
	r = set_video_power(dssdev, video_power ? : HDMI_POWER_MIN);
	mutex_unlock(&hdmi.lock_aux);
	mutex_unlock(&hdmi.lock);
	return r;
}


static int hdmi_enable_video(struct omap_dss_device *dssdev)
{
	int ret;
	bool notify;
	DSSDBG("ENTER hdmi_enable_video()\n");

	mutex_lock(&hdmi.lock);
	mutex_lock(&hdmi.lock_aux);
	notify = hdmi_power != HDMI_POWER_FULL;
	ret = set_video_power(dssdev, HDMI_POWER_FULL);

	/* notify power change */
	mutex_unlock(&hdmi.lock_aux);
	if (notify && !ret)
		hdmi_notify_pwrchange(HDMI_EVENT_POWERON);
	mutex_unlock(&hdmi.lock);
	return ret;
}

static void hdmi_disable_video(struct omap_dss_device *dssdev)
{
	DSSDBG("Enter hdmi_disable_video()\n");

	mutex_lock(&hdmi.lock);
	in_reset = HDMI_IN_RESET;
	/* notify power change */
	if (!audio_on)
		hdmi_notify_pwrchange(HDMI_EVENT_POWEROFF);

	mutex_lock(&hdmi.lock_aux);
	set_video_power(dssdev, HDMI_POWER_OFF);
	mutex_unlock(&hdmi.lock_aux);
	in_reset = 0;
	mutex_unlock(&hdmi.lock);

	/* setting to default only in case of disable and not suspend */
	edid_set = custom_set = false;
	hdmi.code = 16;
	hdmi.mode = 1;
}

/*
 * For timings change to take effect, phy needs to be off. Interrupts will be
 * ignored during this sequence to avoid queuing work items that generate
 * hotplug events.
 */
static int hdmi_reset(struct omap_dss_device *dssdev,
					enum omap_dss_reset_phase phase)
{
	int r = 0;

	if (phase & OMAP_DSS_RESET_OFF) {
		mutex_lock(&hdmi.lock);
		mutex_lock(&hdmi.lock_aux);

		/* don't reset PHY unless in full power state */
		if (hdmi_power == HDMI_POWER_FULL) {
			in_reset = HDMI_IN_RESET;
			hdmi_notify_pwrchange(HDMI_EVENT_POWERPHYOFF);
			hdmi_power_off_phy(dssdev);
		}
		hdmi_power = HDMI_POWER_MIN;
	}
	if (phase & OMAP_DSS_RESET_ON) {
		if (hdmi_power == HDMI_POWER_MIN) {
			r = hdmi_reconfigure(dssdev);
			hdmi_notify_pwrchange(HDMI_EVENT_POWERPHYON);
			in_reset = 0;
		} else if (hdmi_power == HDMI_POWER_OFF) {
			r = -EINVAL;
		}
		/* succeed in HDMI_POWER_MIN state */
		mutex_unlock(&hdmi.lock_aux);
		mutex_unlock(&hdmi.lock);
	}
	return r;
}

static int hdmi_suspend_video(struct omap_dss_device *dssdev)
{
	int ret = 0;

	DSSDBG("hdmi_suspend_video\n");

	mutex_lock(&hdmi.lock);
	mutex_lock(&hdmi.lock_aux);

	if (dssdev->state != OMAP_DSS_DISPLAY_ACTIVE) {
		ret = -EINVAL;
	} else {
		/* (attempt to=ignore result) turn audio power off */
		hdmi_audio_power_off();
		ret = hdmi_set_power(dssdev, video_power, true, audio_on);
	}

	mutex_unlock(&hdmi.lock_aux);
	mutex_unlock(&hdmi.lock);

	return ret;
}

static int hdmi_resume_video(struct omap_dss_device *dssdev)
{
	int ret = 0;

	DSSDBG("hdmi_resume_video\n");

	mutex_lock(&hdmi.lock);
	mutex_lock(&hdmi.lock_aux);

	if (dssdev->state != OMAP_DSS_DISPLAY_SUSPENDED) {
		mutex_unlock(&hdmi.lock_aux);
		ret = -EINVAL;
	} else {
		/* resume video */
		ret = hdmi_set_power(dssdev, video_power, false, audio_on);

		/* signal power change */
		mutex_unlock(&hdmi.lock_aux);
		if (!ret)
			hdmi_notify_pwrchange(HDMI_EVENT_POWERON);

		/* if edid is already read, give hot plug notification */
		if (edid_set)
			set_hdmi_hot_plug_status(dssdev, true);
	}
	mutex_unlock(&hdmi.lock);

	return ret;
}

static void hdmi_get_timings(struct omap_dss_device *dssdev,
			struct omap_video_timings *timings)
{
	*timings = dssdev->panel.timings;
}

static void hdmi_set_timings(struct omap_dss_device *dssdev,
			struct omap_video_timings *timings)
{
	DSSDBG("hdmi_set_timings\n");

	dssdev->panel.timings = *timings;

	if (dssdev->state == OMAP_DSS_DISPLAY_ACTIVE)
		/* turn the phy off and on to get new timings to use */
		hdmi_reset(dssdev, OMAP_DSS_RESET_BOTH);
}

static void hdmi_set_custom_edid_timing_code(struct omap_dss_device *dssdev,
							int code, int mode)
{
	/* for now only set this while on or on HPD */
	if (dssdev->state != OMAP_DSS_DISPLAY_ACTIVE)
		return;

	/* ignore invalid codes */
	if ((!mode && code < ARRAY_SIZE(code_vesa) && code_vesa[code] >= 0) ||
	    (mode == 1 && code < ARRAY_SIZE(code_cea) && code_cea[code] >= 0)) {
		/* turn the hdmi off and on to use new timings */
		hdmi.code = code;
		hdmi.mode = mode;
		custom_set = 1;
		hdmi_reset(dssdev, OMAP_DSS_RESET_BOTH);
	}
}

static struct hdmi_cm hdmi_get_code(struct omap_video_timings *timing)
{
	int i = 0, code = -1, temp_vsync = 0, temp_hsync = 0;
	int timing_vsync = 0, timing_hsync = 0;
	struct omap_video_timings temp;
	struct hdmi_cm cm = {-1};
	DSSDBG("hdmi_get_code");

	for (i = 0; i < ARRAY_SIZE(all_timings_direct); i++) {
		temp = all_timings_direct[i];
		if (temp.pixel_clock != timing->pixel_clock ||
		    temp.x_res != timing->x_res ||
		    temp.y_res != timing->y_res)
			continue;

		temp_hsync = temp.hfp + temp.hsw + temp.hbp;
		timing_hsync = timing->hfp + timing->hsw + timing->hbp;
		temp_vsync = temp.vfp + temp.vsw + temp.vbp;
		timing_vsync = timing->vfp + timing->vsw + timing->vbp;

		DSSDBG("Temp_hsync = %d, temp_vsync = %d, "
			"timing_hsync = %d, timing_vsync = %d",
			temp_hsync, temp_vsync, timing_hsync, timing_vsync);

		if (temp_hsync == timing_hsync && temp_vsync == timing_vsync) {
			code = i;
			cm.code = code_index[i];
			cm.mode = code < 14;
			DSSDBG("Video code = %d mode = %s\n",
			cm.code, cm.mode ? "CEA" : "VESA");
			break;
		}
	}
	return cm;
}

static void hdmi_get_edid(struct omap_dss_device *dssdev)
{
	u8 i = 0, mark = 0, *e;
	int offset, addr;
	struct HDMI_EDID *edid_st = (struct HDMI_EDID *)edid;
	struct image_format *img_format;
	struct audio_format *aud_format;
	struct deep_color *vsdb_format;
	struct latency *lat;
	struct omap_video_timings timings;
	struct hdmi_cm cm;
	int no_timing_info = 0;

	img_format = kzalloc(sizeof(*img_format), GFP_KERNEL);
	if (!img_format) {
		WARN_ON(1);
		return;
	}
	aud_format = kzalloc(sizeof(*aud_format), GFP_KERNEL);
	if (!aud_format) {
		WARN_ON(1);
		goto hdmi_get_err1;
	}
	vsdb_format = kzalloc(sizeof(*vsdb_format), GFP_KERNEL);
	if (!vsdb_format) {
		WARN_ON(1);
		goto hdmi_get_err2;
	}
	lat = kzalloc(sizeof(*lat), GFP_KERNEL);
	if (!lat) {
		WARN_ON(1);
		goto hdmi_get_err3;
	}

	if (!edid_set) {
		printk(KERN_WARNING "Display doesn't seem to be enabled invalid read\n");
		if (HDMI_CORE_DDC_READEDID(HDMI_CORE_SYS, edid,
						HDMI_EDID_MAX_LENGTH) != 0)
			printk(KERN_WARNING "HDMI failed to read E-EDID\n");
		edid_set = !memcmp(edid, header, sizeof(header));
	}

	mdelay(100); /* We should investigate why the delay is required ??*/

	e = edid;
	printk(KERN_INFO "\nHeader:\n%02x\t%02x\t%02x\t%02x\t%02x\t%02x\t"
		"%02x\t%02x\n", e[0], e[1], e[2], e[3], e[4], e[5], e[6], e[7]);
	e += 8;
	printk(KERN_INFO "Vendor & Product:\n%02x\t%02x\t%02x\t%02x\t%02x\t"
		"%02x\t%02x\t%02x\t%02x\t%02x\n",
		e[0], e[1], e[2], e[3], e[4], e[5], e[6], e[7], e[8], e[9]);
	e += 10;
	printk(KERN_INFO "EDID Structure:\n%02x\t%02x\n",
		e[0], e[1]);
	e += 2;
	printk(KERN_INFO "Basic Display Parameter:\n%02x\t%02x\t%02x\t%02x\t%02x\n",
		e[0], e[1], e[2], e[3], e[4]);
	e += 5;
	printk(KERN_INFO "Color Characteristics:\n%02x\t%02x\t%02x\t%02x\t"
		"%02x\t%02x\t%02x\t%02x\t%02x\t%02x\n",
		e[0], e[1], e[2], e[3], e[4], e[5], e[6], e[7], e[8], e[9]);
	e += 10;
	printk(KERN_INFO "Established timings:\n%02x\t%02x\t%02x\n",
		e[0], e[1], e[2]);
	e += 3;
	printk(KERN_INFO "Standard timings:\n%02x\t%02x\t%02x\t%02x\t%02x\t"
			 "%02x\t%02x\t%02x\n",
		e[0], e[1], e[2], e[3], e[4], e[5], e[6], e[7]);
	e += 8;
	printk(KERN_INFO "%02x\t%02x\t%02x\t%02x\t%02x\t%02x\t%02x\t%02x\n",
		e[0], e[1], e[2], e[3], e[4], e[5], e[6], e[7]);
	e += 8;

	for (i = 0; i < EDID_SIZE_BLOCK0_TIMING_DESCRIPTOR; i++) {
		printk(KERN_INFO "Extension 0 Block %d\n", i);
		no_timing_info = get_edid_timing_info(&edid_st->DTD[i],
				&timings);
		mark = dss_debug;
		dss_debug = 1;
		if (!no_timing_info) {
			cm = hdmi_get_code(&timings);
			print_omap_video_timings(&timings);
			printk(KERN_INFO "Video code: %d video mode %d",
				cm.code, cm.mode);
		}
		dss_debug = mark;
	}
	if (edid[0x7e] != 0x00) {
		offset = edid[EDID_DESCRIPTOR_BLOCK1_ADDRESS + 2];
		printk(KERN_INFO "offset %x\n", offset);
		if (offset != 0) {
			addr = EDID_DESCRIPTOR_BLOCK1_ADDRESS + offset;
			/*to determine the number of descriptor blocks */
			for (i = 0; i < EDID_SIZE_BLOCK1_TIMING_DESCRIPTOR;
									i++) {
				printk(KERN_INFO "Extension 1 Block %d", i);
				get_eedid_timing_info(addr, edid, &timings);
				addr += EDID_TIMING_DESCRIPTOR_SIZE;
				mark = dss_debug;
				dss_debug = 1;
				print_omap_video_timings(&timings);
				cm = hdmi_get_code(&timings);
				printk(KERN_INFO "Video code: %d video mode %d",
					cm.code, cm.mode);
				dss_debug = mark;
			}
		}
		printk(KERN_INFO "Supports basic audio: %s",
				(edid[EDID_DESCRIPTOR_BLOCK1_ADDRESS + 3]
			& HDMI_AUDIO_BASIC_MASK) ? "YES" : "NO");

	}

	printk(KERN_INFO "Has IEEE HDMI ID: %s",
		hdmi_has_ieee_id(edid) ? "YES" : "NO");
	printk(KERN_INFO "Supports AI: %s", hdmi.cfg.supports_ai ?
		"YES" : "NO");

	hdmi_get_image_format(edid, img_format);
	printk(KERN_INFO "%d audio length\n", img_format->length);
	for (i = 0 ; i < img_format->length ; i++)
		printk(KERN_INFO "%d %d pref code\n",
			img_format->fmt[i].pref, img_format->fmt[i].code);

	hdmi_get_audio_format(edid, aud_format);
	printk(KERN_INFO "%d audio length\n", aud_format->length);
	for (i = 0 ; i < aud_format->length ; i++)
		printk(KERN_INFO "%d %d format num_of_channels\n",
			aud_format->fmt[i].format,
			aud_format->fmt[i].num_of_ch);

	hdmi_deep_color_support_info(edid, vsdb_format);
	printk(KERN_INFO "%d deep color bit 30 %d  deep color 36 bit "
		"%d max tmds freq", vsdb_format->bit_30, vsdb_format->bit_36,
		vsdb_format->max_tmds_freq);

	hdmi_get_av_delay(edid, lat);
	printk(KERN_INFO "%d vid_latency %d aud_latency "
		"%d interlaced vid latency %d interlaced aud latency",
		lat->vid_latency, lat->aud_latency,
		lat->int_vid_latency, lat->int_aud_latency);

	printk(KERN_INFO "YUV supported %d", hdmi_tv_yuv_supported(edid));

	kfree(lat);
hdmi_get_err3:
	kfree(vsdb_format);
hdmi_get_err2:
	kfree(aud_format);
hdmi_get_err1:
	kfree(img_format);
}

static int hdmi_check_timings(struct omap_dss_device *dssdev,
			struct omap_video_timings *timings)
{
	DSSDBG("hdmi_check_timings\n");

	if (memcmp(&dssdev->panel.timings, timings, sizeof(*timings)) == 0)
		return 0;

	return -EINVAL;
}

static int hdmi_set_s3d_disp_type(struct omap_dss_device *dssdev,
						struct s3d_disp_info *info)
{
	int r = -EINVAL;
	struct hdmi_s3d_info tinfo;

	tinfo.structure = 0;
	tinfo.subsamp = false;
	tinfo.subsamp_pos = 0;

	switch (info->type) {
	case S3D_DISP_OVERUNDER:
		if (info->sub_samp == S3D_DISP_SUB_SAMPLE_NONE) {
			tinfo.structure = HDMI_S3D_FRAME_PACKING;
			r = 0;
		} else {
			goto err;
		}
		break;
	case S3D_DISP_SIDEBYSIDE:
		if (info->sub_samp == S3D_DISP_SUB_SAMPLE_H) {
			tinfo.structure = HDMI_S3D_SIDE_BY_SIDE_HALF;
			tinfo.subsamp = true;
			tinfo.subsamp_pos = HDMI_S3D_HOR_EL_ER;
			r = 0;
		} else {
			goto err;
		}
		break;
	default:
		goto err;
	}
	hdmi_s3d = tinfo;
err:
	return r;
}

static int hdmi_enable_s3d(struct omap_dss_device *dssdev, bool enable)
{
	int r = 0;

	/* Timings change between S3D and 2D mode, hence phy needs to reset */
	mutex_lock(&hdmi.lock);
	mutex_lock(&hdmi.lock_aux);
	if (hdmi.s3d_enabled == enable)
		goto done;

	dssdev->panel.s3d_info.type = S3D_DISP_NONE;
	hdmi.s3d_enabled = enable;
	r = hdmi_reset(dssdev, OMAP_DSS_RESET_BOTH);

	/* hdmi.s3d_enabled will be updated when powering display up */
	/* if there's no S3D support it will be reset to false */
	if (r == 0 && hdmi.s3d_enabled) {
		switch (hdmi_s3d.structure) {
		case HDMI_S3D_FRAME_PACKING:
			dssdev->panel.s3d_info.type = S3D_DISP_OVERUNDER;
			dssdev->panel.s3d_info.gap = 30;
			if (hdmi_s3d.subsamp)
				dssdev->panel.s3d_info.sub_samp =
							S3D_DISP_SUB_SAMPLE_V;
			break;
		case HDMI_S3D_SIDE_BY_SIDE_HALF:
			dssdev->panel.s3d_info.type = S3D_DISP_SIDEBYSIDE;
			dssdev->panel.s3d_info.gap = 0;
			if (hdmi_s3d.subsamp)
				dssdev->panel.s3d_info.sub_samp =
							S3D_DISP_SUB_SAMPLE_H;
			break;
		default:
			dssdev->panel.s3d_info.type = S3D_DISP_OVERUNDER;
			dssdev->panel.s3d_info.gap = 0;
			dssdev->panel.s3d_info.sub_samp = S3D_DISP_SUB_SAMPLE_V;
			break;
		}
		dssdev->panel.s3d_info.order = S3D_DISP_ORDER_L;
	} else if (r) {
		DSSDBG("hdmi_enable_s3d: error enabling HDMI(%d)\n", r);
		hdmi.s3d_enabled = false;
	} else if (!hdmi.s3d_enabled && enable) {
		DSSDBG("hdmi_enable_s3d: No S3D support\n");
		/* Fallback to subsampled side-by-side packing */
		dssdev->panel.s3d_info.type = S3D_DISP_SIDEBYSIDE;
		dssdev->panel.s3d_info.gap = 0;
		dssdev->panel.s3d_info.sub_samp = S3D_DISP_SUB_SAMPLE_H;
		dssdev->panel.s3d_info.order = S3D_DISP_ORDER_L;
	}
done:
	mutex_unlock(&hdmi.lock_aux);
	mutex_unlock(&hdmi.lock);

	return r;
}

static bool hdmi_get_s3d_enabled(struct omap_dss_device *dssdev)
{
	return hdmi.s3d_enabled;
}

int hdmi_init_display(struct omap_dss_device *dssdev)
{
	printk(KERN_DEBUG "init_display\n");

	/* register HDMI specific sysfs files */
	/* note: custom_edid_timing should perhaps be moved here too,
	 * instead of generic code?  Or edid sysfs file should be moved
	 * to generic code.. either way they should be in same place..
	 */
	if (device_create_file(&dssdev->dev, &dev_attr_edid))
		DSSERR("failed to create sysfs file\n");
	if (device_create_file(&dssdev->dev, &dev_attr_yuv))
		DSSERR("failed to create sysfs file\n");
	if (device_create_file(&dssdev->dev, &dev_attr_deepcolor))
		DSSERR("failed to create sysfs file\n");
	if (device_create_file(&dssdev->dev, &dev_attr_lr_fr))
		DSSERR("failed to create sysfs file\n");
	if (device_create_file(&dssdev->dev, &dev_attr_audio_power))
		DSSERR("failed to create sysfs file\n");
	if (device_create_file(&dssdev->dev, &dev_attr_code))
		DSSERR("failed to create sysfs file\n");

	return 0;
}

static int hdmi_read_edid(struct omap_video_timings *dp)
{
	int r = 0, ret = 0, code = 0;

	memset(edid, 0, HDMI_EDID_MAX_LENGTH);
	if (!edid_set)
		ret = HDMI_CORE_DDC_READEDID(HDMI_CORE_SYS, edid,
							HDMI_EDID_MAX_LENGTH);
	if (ret != 0) {
		printk(KERN_WARNING "HDMI failed to read E-EDID\n");
	} else {
		if (!memcmp(edid, header, sizeof(header))) {
			if (hdmi.s3d_enabled) {
				/* Update flag to convey if sink supports 3D */
				hdmi.s3d_enabled = hdmi_s3d_supported(edid);
			}
			/* search for timings of default resolution */
			if (get_edid_timing_data((struct HDMI_EDID *) edid))
				edid_set = true;
		}
	}

	if (!edid_set) {
		DSSDBG("fallback to VGA\n");
		hdmi.code = 4; /*setting default value of 640 480 VGA*/
		hdmi.mode = 0;
	}
	if (hdmi.s3d_enabled && hdmi_s3d.structure == HDMI_S3D_FRAME_PACKING)
		code = get_s3d_timings_index();
	else
		code = get_timings_index();

	*dp = all_timings_direct[code];

	DSSDBG(KERN_INFO"hdmi read EDID:\n");
	print_omap_video_timings(dp);

	return r;
}

/*
 *------------------------------------------------------------------------------
 * Function    : get_edid_timing_data
 *------------------------------------------------------------------------------
 * Description : This function gets the resolution information from EDID
 *
 * Parameters  : void
 *
 * Returns     : void
 *------------------------------------------------------------------------------
 */
static int get_edid_timing_data(struct HDMI_EDID *edid)
{
	u8 i, j, code, offset = 0, addr = 0;
	struct hdmi_cm cm;
	bool audio_support = false;
	int svd_base, svd_length, svd_code, svd_native;

	/*
	 *  Verify if the sink supports audio
	 */
	/* check if EDID has CEA extension block */
	if ((edid->extension_edid != 0x00))
		/* check if CEA extension block is version 3 */
		if (edid->extention_rev == 3)
			/* check if extension block has the IEEE HDMI ID*/
			if (hdmi_has_ieee_id((u8 *)edid))
				/* check if sink supports basic audio */
				if (edid->num_dtd & HDMI_AUDIO_BASIC_MASK)
					audio_support = true;

	/* Seach block 0, there are 4 DTDs arranged in priority order */
	for (i = 0; i < EDID_SIZE_BLOCK0_TIMING_DESCRIPTOR; i++) {
		get_edid_timing_info(&edid->DTD[i], &edid_timings);
		DSSDBG("Block0 [%d] timings:", i);
		print_omap_video_timings(&edid_timings);
		cm = hdmi_get_code(&edid_timings);
		DSSDBG("Block0[%d] value matches code = %d , mode = %d",
			i, cm.code, cm.mode);
		if (cm.code == -1)
			continue;
		if (hdmi.s3d_enabled && s3d_code_cea[cm.code] == -1)
			continue;
		/* if sink supports audio, use CEA video timing */
		if (audio_support && !cm.mode)
			continue;
		hdmi.code = cm.code;
		hdmi.mode = cm.mode;
		DSSDBG("code = %d , mode = %d", hdmi.code, hdmi.mode);
		return 1;
	}
	/* Search SVDs in block 1 twice: first natives and then all */
	if (edid->extension_edid != 0x00) {
		hdmi_get_video_svds((u8 *)edid, &svd_base, &svd_length);
		for (j = 1; j >= 0; j--) {
			for (i = 0; i < svd_length; i++) {
				svd_native = ((u8 *)edid)[svd_base+i]
					& HDMI_EDID_EX_VIDEO_NATIVE;
				svd_code = ((u8 *)edid)[svd_base+i]
					& HDMI_EDID_EX_VIDEO_MASK;
				if (svd_code >= ARRAY_SIZE(code_cea))
					continue;
				/* Check if this SVD is native*/
				if (!svd_native && j)
					continue;
				/* Check if this 3D CEA timing is supported*/
				if (hdmi.s3d_enabled &&
					s3d_code_cea[svd_code] == -1)
					continue;
				/* Check if this CEA timing is supported*/
				if (code_cea[svd_code] == -1)
					continue;
				hdmi.code = svd_code;
				hdmi.mode = 1;
				return 1;
			}
		}
	}
	/* Search DTDs in block1 */
	if (edid->extension_edid != 0x00) {
		offset = edid->offset_dtd;
		if (offset != 0)
			addr = EDID_DESCRIPTOR_BLOCK1_ADDRESS + offset;
		for (i = 0; i < EDID_SIZE_BLOCK1_TIMING_DESCRIPTOR; i++) {
			get_eedid_timing_info(addr, (u8 *)edid, &edid_timings);
			addr += EDID_TIMING_DESCRIPTOR_SIZE;
			cm = hdmi_get_code(&edid_timings);
			DSSDBG("Block1[%d] value matches code = %d , mode = %d",
				i, cm.code, cm.mode);
			if (cm.code == -1)
				continue;
			if (hdmi.s3d_enabled && s3d_code_cea[cm.code] == -1)
				continue;
			/* if sink supports audio, use CEA video timing */
			if (audio_support && !cm.mode)
				continue;
			hdmi.code = cm.code;
			hdmi.mode = cm.mode;
			DSSDBG("code = %d , mode = %d", hdmi.code, hdmi.mode);
			return 1;
		}
	}
	/*As last resort, check for best standard timing supported:*/
	if (edid->timing_1 & 0x01) {
		DSSDBG("800x600@60Hz\n");
		hdmi.mode = 0;
		hdmi.code = 9;
		return 1;
	}
	if (edid->timing_2 & 0x08) {
		DSSDBG("1024x768@60Hz\n");
		hdmi.mode = 0;
		hdmi.code = 16;
		return 1;
	}

	hdmi.code = 4; /*setting default value of 640 480 VGA*/
	hdmi.mode = 0;
	code = code_vesa[hdmi.code];
	edid_timings = all_timings_direct[code];
	return 1;
}

bool is_hdmi_interlaced(void)
{
	return hdmi.cfg.interlace;
}

const struct omap_video_timings *hdmi_get_omap_timing(int ix)
{
	if (ix < 0 || ix >= ARRAY_SIZE(all_timings_direct))
		return NULL;
	return all_timings_direct + ix;
}

int hdmi_register_hdcp_callbacks(void (*hdmi_start_frame_cb)(void),
				 void (*hdmi_stop_frame_cb)(void))
{
	hdmi.hdmi_start_frame_cb = hdmi_start_frame_cb;
	hdmi.hdmi_stop_frame_cb  = hdmi_stop_frame_cb;

	return hdmi_w1_get_video_state();
}
EXPORT_SYMBOL(hdmi_register_hdcp_callbacks);

void hdmi_restart(void)
{
	struct omap_dss_device *dssdev = get_hdmi_device();

	hdmi_disable_video(dssdev);
	hdmi_enable_video(dssdev);
}
EXPORT_SYMBOL(hdmi_restart);
