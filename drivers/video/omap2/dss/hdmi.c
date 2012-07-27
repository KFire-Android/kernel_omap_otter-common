/*
 * hdmi.c
 *
 * HDMI interface DSS driver setting for TI's OMAP4 family of processor.
 * Copyright (C) 2010-2011 Texas Instruments Incorporated - http://www.ti.com/
 * Authors: Yong Zhi
 *	Mythri pk <mythripk@ti.com>
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

#define DSS_SUBSYS_NAME "HDMI"

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>
#include <linux/clk.h>
#include <linux/fb.h>
#include <linux/omapfb.h>
#include <video/omapdss.h>

#include "ti_hdmi.h"
#include "dss.h"
#include "dss_features.h"

#define HDMI_WP			0x0
#define HDMI_PLLCTRL		0x200
#define HDMI_PHY		0x300

#define HDMI_CORE_AV		0x900

/* HDMI EDID Length move this */
#define HDMI_EDID_MAX_LENGTH			512
#define EDID_TIMING_DESCRIPTOR_SIZE		0x12
#define EDID_DESCRIPTOR_BLOCK0_ADDRESS		0x36
#define EDID_DESCRIPTOR_BLOCK1_ADDRESS		0x80
#define EDID_HDMI_VENDOR_SPECIFIC_DATA_BLOCK	128
#define EDID_SIZE_BLOCK0_TIMING_DESCRIPTOR	4
#define EDID_SIZE_BLOCK1_TIMING_DESCRIPTOR	4

#define HDMI_DEFAULT_REGN 16
#define HDMI_DEFAULT_REGM2 1

static struct {
	struct mutex lock;
	struct platform_device *pdev;
	struct hdmi_ip_data ip_data;
	struct omap_dss_device *dssdev;
	int code;
	int mode;
	u8 edid[HDMI_EDID_MAX_LENGTH];
	bool edid_set;
	bool custom_set;
	int hdmi_irq;
	bool hdcp;
	bool can_do_hdmi;

	struct clk *sys_clk;
	struct clk *dss_32k_clk;

	struct regulator *vdds_hdmi;
	bool enabled;
	bool force_timings;
	int source_physical_address;
	void (*hdmi_cec_enable_cb)(int status);
	void (*hdmi_cec_irq_cb)(void);
	void (*hdmi_cec_hpd)(int phy_addr, int status);
	void (*hdmi_start_frame_cb)(void);
	bool (*hdmi_power_on_cb)(void);
} hdmi;

static const u8 edid_header[8] = {0x0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x0};

int hdmi_runtime_get(void)
{
	int r;

	DSSDBG("hdmi_runtime_get\n");

	r = pm_runtime_get_sync(&hdmi.pdev->dev);
	WARN_ON(r < 0);
	if (r < 0)
		return r;

	return 0;
}

void hdmi_runtime_put(void)
{
	int r;

	DSSDBG("hdmi_runtime_put\n");

	r = pm_runtime_put_sync(&hdmi.pdev->dev);
	WARN_ON(r < 0);
}

int hdmi_init_display(struct omap_dss_device *dssdev)
{
	DSSDBG("init_display\n");

	hdmi.dssdev = dssdev;
	dss_init_hdmi_ip_ops(&hdmi.ip_data);
	return 0;
}

static int relaxed_fb_mode_is_equal(const struct fb_videomode *mode1,
					const struct fb_videomode *mode2)
{
	u32 ratio1 = mode1->flag & (FB_FLAG_RATIO_4_3 | FB_FLAG_RATIO_16_9);
	u32 ratio2 = mode2->flag & (FB_FLAG_RATIO_4_3 | FB_FLAG_RATIO_16_9);
	return (mode1->xres         == mode2->xres &&
		mode1->yres         == mode2->yres &&
		mode1->pixclock     <= mode2->pixclock * 201 / 200 &&
		mode1->pixclock     >= mode2->pixclock * 200 / 201 &&
		mode1->hsync_len + mode1->left_margin + mode1->right_margin ==
		mode2->hsync_len + mode2->left_margin + mode2->right_margin &&
		mode1->vsync_len + mode1->upper_margin + mode1->lower_margin ==
		mode2->vsync_len + mode2->upper_margin + mode2->lower_margin &&
		(!ratio1 || !ratio2 || ratio1 == ratio2) &&
		(mode1->vmode & FB_VMODE_INTERLACED) ==
		(mode2->vmode & FB_VMODE_INTERLACED));

}

static int hdmi_set_timings(struct fb_videomode *vm, bool check_only)
{
	int i = 0;
	int r = 0;
	DSSDBG("hdmi_set_timings\n");

	if (!vm->xres || !vm->yres || !vm->pixclock)
		goto fail;

	for (i = 0; i < CEA_MODEDB_SIZE; i++) {
		if (relaxed_fb_mode_is_equal(cea_modes + i, vm)) {
			*vm = cea_modes[i];
			if (check_only)
				return 1;
			hdmi.ip_data.cfg.cm.code = i;
			hdmi.ip_data.cfg.cm.mode = HDMI_HDMI;
			hdmi.ip_data.cfg.timings =
			cea_modes[hdmi.ip_data.cfg.cm.code];
			goto done;
		}
	}
	for (i = 0; i < VESA_MODEDB_SIZE; i++) {
		if (relaxed_fb_mode_is_equal(vesa_modes + i, vm)) {
			*vm = vesa_modes[i];
			if (check_only)
				return 1;
			hdmi.ip_data.cfg.cm.code = i;
			hdmi.ip_data.cfg.cm.mode = HDMI_DVI;
			hdmi.ip_data.cfg.timings =
			vesa_modes[hdmi.ip_data.cfg.cm.code];
			goto done;
		}
	}
fail:
	if (check_only)
		return 0;
	hdmi.ip_data.cfg.cm.code = 1;
	hdmi.ip_data.cfg.cm.mode = HDMI_HDMI;
	hdmi.ip_data.cfg.timings = cea_modes[hdmi.ip_data.cfg.cm.code];
	i = -1;
done:
	DSSDBG("%s-%d\n", hdmi.ip_data.cfg.cm.mode ? "CEA" : "VESA",
		hdmi.ip_data.cfg.cm.code);

	r = i >= 0 ? 1 : 0;
	return r;

}

void hdmi_get_monspecs(struct omap_dss_device *dssdev)
{
	int i, j;
	char *edid = (char *)hdmi.edid;
	struct fb_monspecs *specs = &dssdev->panel.monspecs;
	u32 fclk = dispc_fclk_rate() / 1000;
	u32 max_pclk = dssdev->clocks.hdmi.max_pixclk_khz;

	if (max_pclk && max_pclk < fclk)
		fclk = max_pclk;

	memset(specs, 0x0, sizeof(*specs));
	if (!hdmi.edid_set)
		return;
	fb_edid_to_monspecs(edid, specs);
	if (specs->modedb == NULL)
		return;

	for (i = 1; i <= edid[0x7e] && i * 128 < HDMI_EDID_MAX_LENGTH; i++) {
		if (edid[i * 128] == 0x2)
			fb_edid_add_monspecs(edid + i * 128, specs);
	}
	if (hdmi.force_timings) {
		for (i = 0; i < specs->modedb_len; i++) {
			specs->modedb[i++] = hdmi.ip_data.cfg.timings;
			break;
		}
		specs->modedb_len = i;
		hdmi.force_timings = false;
		return;
	}

	hdmi.can_do_hdmi = specs->misc & FB_MISC_HDMI;

	/* filter out resolutions we don't support */
	for (i = j = 0; i < specs->modedb_len; i++) {
		if (!hdmi_set_timings(&specs->modedb[i], true))
			continue;
		if (fclk < PICOS2KHZ(specs->modedb[i].pixclock))
			continue;
		if (specs->modedb[i].flag & FB_FLAG_PIXEL_REPEAT)
			continue;
		specs->modedb[j++] = specs->modedb[i];
	}
	specs->modedb_len = j;

	/* Find out the Source Physical address for the CEC
	CEC physical address will be part of VSD block from
	TV Physical address is 2 bytes after 24 bit IEEE
	registration identifier (0x000C03)
	*/
	i = EDID_HDMI_VENDOR_SPECIFIC_DATA_BLOCK;
	while (i < (HDMI_EDID_MAX_LENGTH - 5)) {
		if ((edid[i] == 0x03) && (edid[i+1] == 0x0c) &&
			(edid[i+2] == 0x00)) {
			hdmi.source_physical_address = (edid[i+3] << 8) |
				edid[i+4];
			break;
		}
		i++;

	}
}

void hdmi_inform_hpd_to_cec(int status)
{
	if (!status)
		hdmi.source_physical_address = 0;

	if (hdmi.hdmi_cec_hpd)
		(*hdmi.hdmi_cec_hpd)(hdmi.source_physical_address,
			status);
}

void hdmi_inform_power_on_to_cec(int status)
{
	if (hdmi.hdmi_cec_enable_cb)
		(*hdmi.hdmi_cec_enable_cb)(status);
}

u8 *hdmi_read_valid_edid(void)
{
	int ret, i;

	if (hdmi.edid_set)
		return hdmi.edid;

	memset(hdmi.edid, 0, HDMI_EDID_MAX_LENGTH);

	ret = hdmi.ip_data.ops->read_edid(&hdmi.ip_data, hdmi.edid,
						  HDMI_EDID_MAX_LENGTH);

	for (i = 0; i < HDMI_EDID_MAX_LENGTH; i += 16)
		pr_info("edid[%03x] = %02x %02x %02x %02x %02x %02x %02x %02x "\
			"%02x %02x %02x %02x %02x %02x %02x %02x\n", i,
			hdmi.edid[i], hdmi.edid[i + 1], hdmi.edid[i + 2],
			hdmi.edid[i + 3], hdmi.edid[i + 4], hdmi.edid[i + 5],
			hdmi.edid[i + 6], hdmi.edid[i + 7], hdmi.edid[i + 8],
			hdmi.edid[i + 9], hdmi.edid[i + 10], hdmi.edid[i + 11],
			hdmi.edid[i + 12], hdmi.edid[i + 13], hdmi.edid[i + 14],
			hdmi.edid[i + 15]);

	if (ret) {
		DSSWARN("failed to read E-EDID\n");
		return NULL;
	}
	if (memcmp(hdmi.edid, edid_header, sizeof(edid_header))) {
		DSSWARN("failed to read E-EDID: wrong header\n");
		return NULL;
	}
	hdmi.edid_set = true;
	return hdmi.edid;
}

unsigned long hdmi_get_pixel_clock(void)
{
	/* HDMI Pixel Clock in Mhz */
	return PICOS2KHZ(hdmi.ip_data.cfg.timings.pixclock) * 1000;
}

static void hdmi_compute_pll(struct omap_dss_device *dssdev, int phy,
		struct hdmi_pll_info *pi)
{
	unsigned long clkin, refclk;
	u32 mf;

	clkin = clk_get_rate(hdmi.sys_clk) / 10000;
	/*
	 * Input clock is predivided by N + 1
	 * out put of which is reference clk
	 */
	if (dssdev->clocks.hdmi.regn == 0)
		pi->regn = HDMI_DEFAULT_REGN;
	else
		pi->regn = dssdev->clocks.hdmi.regn;

	refclk = clkin / pi->regn;

	if (dssdev->clocks.hdmi.regm2 == 0) {
		if (cpu_is_omap44xx()) {
			pi->regm2 = HDMI_DEFAULT_REGM2;
		} else if (cpu_is_omap54xx()) {
			if (phy <= 50000)
				pi->regm2 = 2;
			else
				pi->regm2 = 1;
		}
	} else {
		pi->regm2 = dssdev->clocks.hdmi.regm2;
	}

	/*
	 * multiplier is pixel_clk/ref_clk
	 * Multiplying by 100 to avoid fractional part removal
	 */
	pi->regm = phy * pi->regm2 / refclk;

	/*
	 * fractional multiplier is remainder of the difference between
	 * multiplier and actual phy(required pixel clock thus should be
	 * multiplied by 2^18(262144) divided by the reference clock
	 */
	mf = (phy - pi->regm / pi->regm2 * refclk) * 262144;
	pi->regmf = pi->regm2 * mf / refclk;

	/*
	 * Dcofreq should be set to 1 if required pixel clock
	 * is greater than 1000MHz
	 */
	pi->dcofreq = phy > 1000 * 100;
	pi->regsd = ((pi->regm * clkin / 10) / (pi->regn * 250) + 5) / 10;

	/* Set the reference clock to sysclk reference */
	pi->refsel = HDMI_REFSEL_SYSCLK;

	DSSDBG("M = %d Mf = %d\n", pi->regm, pi->regmf);
	DSSDBG("range = %d sd = %d\n", pi->dcofreq, pi->regsd);
}

static void hdmi_load_hdcp_keys(struct omap_dss_device *dssdev)
{
	DSSDBG("hdmi_load_hdcp_keys\n");
	/* load the keys and reset the wrapper to populate the AKSV registers*/
	if (hdmi.hdmi_power_on_cb()) {
		hdmi_ti_4xxx_set_wait_soft_reset(&hdmi.ip_data);
		DSSINFO("HDMI_WRAPPER RESET DONE\n");
	}
}

static int hdmi_power_on(struct omap_dss_device *dssdev)
{
	int r;
	struct omap_video_timings *p;
	unsigned long phy;

	r = hdmi_runtime_get();
	if (r)
		return r;

	if (cpu_is_omap54xx()) {
		r = regulator_enable(hdmi.vdds_hdmi);
		if (r)
			goto err;
	}

	dss_mgr_disable(dssdev->manager);

	p = &dssdev->panel.timings;

	DSSDBG("hdmi_power_on x_res= %d y_res = %d\n",
		dssdev->panel.timings.x_res,
		dssdev->panel.timings.y_res);

	if (!hdmi.custom_set) {
		struct fb_videomode vesa_vga = vesa_modes[4];
		hdmi_set_timings(&vesa_vga, false);
	}

	omapfb_fb2dss_timings(&hdmi.ip_data.cfg.timings,
					&dssdev->panel.timings);

	switch (hdmi.ip_data.cfg.deep_color) {
	case HDMI_DEEP_COLOR_30BIT:
		phy = (p->pixel_clock * 125) / 100 ;
		break;
	case HDMI_DEEP_COLOR_36BIT:
		if (p->pixel_clock >= 148500) {
			DSSERR("36 bit deep color not supported for the \
				pixel clock %d\n", p->pixel_clock);
			goto err;
		}
		phy = (p->pixel_clock * 150) / 100;
		break;
	case HDMI_DEEP_COLOR_24BIT:
	default:
		phy = p->pixel_clock;
		break;
	}

	hdmi_compute_pll(dssdev, phy, &hdmi.ip_data.pll_data);

	hdmi.ip_data.ops->video_enable(&hdmi.ip_data, 0);

	hdmi_load_hdcp_keys(dssdev);

	/* config the PLL and PHY hdmi_set_pll_pwrfirst */
	r = hdmi.ip_data.ops->pll_enable(&hdmi.ip_data);
	if (r) {
		DSSDBG("Failed to lock PLL\n");
		goto err;
	}

	r = hdmi.ip_data.ops->phy_enable(&hdmi.ip_data);
	if (r) {
		DSSDBG("Failed to start PHY\n");
		goto err;
	}

	hdmi.ip_data.cfg.cm.mode = hdmi.can_do_hdmi ? hdmi.mode : HDMI_DVI;

	hdmi.ip_data.ops->video_configure(&hdmi.ip_data);

	/* Make selection of HDMI in DSS */
	dss_select_hdmi_venc_clk_source(DSS_HDMI_M_PCLK);

	/* Select the dispc clock source as PRCM clock, to ensure that it is not
	 * DSI PLL source as the clock selected by DSI PLL might not be
	 * sufficient for the resolution selected / that can be changed
	 * dynamically by user. This can be moved to single location , say
	 * Boardfile.
	 */
	dss_select_dispc_clk_source(dssdev->clocks.dispc.dispc_fclk_src);

	/* bypass TV gamma table */
	dispc_enable_gamma_table(0);

	/* tv size */
	dispc_set_digit_size(dssdev->panel.timings.x_res,
			dssdev->panel.timings.y_res);

	hdmi.ip_data.ops->video_enable(&hdmi.ip_data, 1);

	if (hdmi.hdcp && hdmi.hdmi_start_frame_cb)
		(*hdmi.hdmi_start_frame_cb)();

	r = dss_mgr_enable(dssdev->manager);
	if (r)
		goto err_mgr_enable;

	return 0;

err_mgr_enable:
	hdmi.ip_data.ops->video_enable(&hdmi.ip_data, 0);
	hdmi.ip_data.set_mode = false;
	hdmi.ip_data.ops->phy_disable(&hdmi.ip_data);
	hdmi.ip_data.ops->pll_disable(&hdmi.ip_data);
err:
	hdmi_runtime_put();
	return -EIO;
}

static void hdmi_power_off(struct omap_dss_device *dssdev)
{
	dss_mgr_disable(dssdev->manager);

	hdmi.ip_data.ops->hdcp_disable(&hdmi.ip_data);
	hdmi.ip_data.ops->video_enable(&hdmi.ip_data, 0);
	hdmi.ip_data.ops->phy_disable(&hdmi.ip_data);
	hdmi.ip_data.ops->pll_disable(&hdmi.ip_data);

	if (cpu_is_omap54xx())
		regulator_disable(hdmi.vdds_hdmi);

	hdmi_runtime_put();

	hdmi.ip_data.cfg.deep_color = HDMI_DEEP_COLOR_24BIT;
}

void omapdss_hdmi_register_hdcp_callbacks(void (*hdmi_start_frame_cb)(void),
				 bool (*hdmi_power_on_cb)(void))
{
	hdmi.hdmi_start_frame_cb = hdmi_start_frame_cb;
	hdmi.hdmi_power_on_cb = hdmi_power_on_cb;
}

struct hdmi_ip_data *get_hdmi_ip_data(void)
{
	return &hdmi.ip_data;
}

int omapdss_hdmi_set_deepcolor(struct omap_dss_device *dssdev, int val,
		bool hdmi_restart)
{
	int r;

	if (!hdmi_restart) {
		hdmi.ip_data.cfg.deep_color = val;
		return 0;
	}

	dssdev->driver->disable(dssdev);
	hdmi.custom_set = true;
	hdmi.ip_data.cfg.deep_color = val;
	r = dssdev->driver->enable(dssdev);
	if (r)
		return r;

	return 0;
}

int omapdss_hdmi_get_deepcolor(void)
{
	return hdmi.ip_data.cfg.deep_color;
}

int omapdss_hdmi_set_range(int range)
{
	int r = 0;
	enum hdmi_range old_range;

	old_range = hdmi.ip_data.cfg.range;
	hdmi.ip_data.cfg.range = range;

	/* HDMI 1.3 section 6.6 VGA (640x480) format requires Full Range */
	if ((range == 0) &&
		((hdmi.ip_data.cfg.cm.code == 4 &&
		hdmi.ip_data.cfg.cm.mode == HDMI_DVI) ||
		(hdmi.ip_data.cfg.cm.code == 1 &&
		hdmi.ip_data.cfg.cm.mode == HDMI_HDMI)))
			return -EINVAL;

	r = hdmi.ip_data.ops->configure_range(&hdmi.ip_data);
	if (r)
		hdmi.ip_data.cfg.range = old_range;

	return r;
}

int omapdss_hdmi_get_range(void)
{
	return hdmi.ip_data.cfg.range;
}

int omapdss_hdmi_register_cec_callbacks(void (*hdmi_cec_enable_cb)(int status),
					void (*hdmi_cec_irq_cb)(void),
					void (*hdmi_cec_hpd)(int phy_addr,
					int status))
{
	hdmi.hdmi_cec_enable_cb = hdmi_cec_enable_cb;
	hdmi.hdmi_cec_irq_cb = hdmi_cec_irq_cb;
	hdmi.hdmi_cec_hpd = hdmi_cec_hpd;
	return 0;
}
EXPORT_SYMBOL(omapdss_hdmi_register_cec_callbacks);

int hdmi_get_ipdata(struct hdmi_ip_data *ip_data)
{
	*ip_data = hdmi.ip_data;
	return 0;
}
EXPORT_SYMBOL(hdmi_get_ipdata);

int omapdss_hdmi_unregister_cec_callbacks(void)
{
	hdmi.hdmi_cec_enable_cb = NULL;
	hdmi.hdmi_cec_irq_cb = NULL;
	hdmi.hdmi_cec_hpd = NULL;
	return 0;
}

int omapdss_hdmi_display_check_timing(struct omap_dss_device *dssdev,
					struct omap_video_timings *timings)
{
	struct fb_videomode t;

	omapfb_dss2fb_timings(timings, &t);

	/* also check interlaced timings */
	if (!hdmi_set_timings(&t, true)) {
		t.yres *= 2;
		t.vmode |= FB_VMODE_INTERLACED;
	}
	if (!hdmi_set_timings(&t, true))
		return -EINVAL;

	return 0;
}

int omapdss_hdmi_display_set_mode2(struct omap_dss_device *dssdev,
				   struct fb_videomode *vm,
				   int code, int mode)
{
	/* turn the hdmi off and on to get new timings to use */
	hdmi.ip_data.set_mode = true;
	dssdev->driver->disable(dssdev);
	hdmi.ip_data.set_mode = false;
	hdmi.ip_data.cfg.timings = *vm;
	hdmi.custom_set = 1;
	hdmi.hdcp = true;
	hdmi.code = code;
	hdmi.mode = mode;
	return dssdev->driver->enable(dssdev);
}

int omapdss_hdmi_display_set_mode(struct omap_dss_device *dssdev,
				  struct fb_videomode *vm)
{
	int r1, r2;
	/* turn the hdmi off and on to get new timings to use */
	hdmi.ip_data.set_mode = true;
	dssdev->driver->disable(dssdev);
	hdmi.ip_data.set_mode = false;
	r1 = hdmi_set_timings(vm, false) ? 0 : -EINVAL;
	hdmi.custom_set = true;
	hdmi.hdcp = true;
	hdmi.code = hdmi.ip_data.cfg.cm.code;
	hdmi.mode = hdmi.ip_data.cfg.cm.mode;
	r2 = dssdev->driver->enable(dssdev);
	return r1 ? : r2;
}

int hdmi_notify_hpd(struct omap_dss_device *dssdev, bool hpd)
{
	return hdmi.ip_data.ops->set_phy(&hdmi.ip_data, hpd);
}

int omapdss_hdmi_display_3d_enable(struct omap_dss_device *dssdev,
					struct s3d_disp_info *info, int code)
{
	int r = 0;

	DSSDBG("ENTER hdmi_display_3d_enable\n");

	mutex_lock(&hdmi.lock);

	if (dssdev->manager == NULL) {
		DSSERR("failed to enable display: no manager\n");
		r = -ENODEV;
		goto err0;
	}

	if (hdmi.enabled)
		goto err0;

	r = omap_dss_start_device(dssdev);
	if (r) {
		DSSERR("failed to start device\n");
		goto err0;
	}

	if (dssdev->platform_enable) {
		r = dssdev->platform_enable(dssdev);
		if (r) {
			DSSERR("failed to enable GPIO's\n");
			goto err1;
		}
	}

	/* hdmi.s3d_enabled will be updated when powering display up */
	/* if there's no S3D support it will be reset to false */
	switch (info->type) {
	case S3D_DISP_OVERUNDER:
		if (info->sub_samp == S3D_DISP_SUB_SAMPLE_NONE) {
			dssdev->panel.s3d_info = *info;
			hdmi.ip_data.cfg.s3d_info.frame_struct = HDMI_S3D_FRAME_PACKING;
			hdmi.ip_data.cfg.s3d_info.subsamp = false;
			hdmi.ip_data.cfg.s3d_info.subsamp_pos = 0;
			hdmi.ip_data.cfg.s3d_enabled = true;
			hdmi.ip_data.cfg.s3d_info.vsi_enabled = true;
		} else {
			goto err2;
		}
		break;
	case S3D_DISP_SIDEBYSIDE:
		dssdev->panel.s3d_info = *info;
		if (info->sub_samp == S3D_DISP_SUB_SAMPLE_NONE) {
			hdmi.ip_data.cfg.s3d_info.frame_struct = HDMI_S3D_SIDE_BY_SIDE_FULL;
			hdmi.ip_data.cfg.s3d_info.subsamp = true;
			hdmi.ip_data.cfg.s3d_info.subsamp_pos = HDMI_S3D_HOR_EL_ER;
			hdmi.ip_data.cfg.s3d_enabled = true;
			hdmi.ip_data.cfg.s3d_info.vsi_enabled = true;
		} else if (info->sub_samp == S3D_DISP_SUB_SAMPLE_H) {
			hdmi.ip_data.cfg.s3d_info.frame_struct = HDMI_S3D_SIDE_BY_SIDE_HALF;
			hdmi.ip_data.cfg.s3d_info.subsamp = true;
			hdmi.ip_data.cfg.s3d_info.subsamp_pos = HDMI_S3D_HOR_EL_ER;
			hdmi.ip_data.cfg.s3d_info.vsi_enabled = true;
		} else {
			goto err2;
		}
		break;
	default:
		goto err2;
	}
	if (hdmi.ip_data.cfg.s3d_enabled) {
		hdmi.ip_data.cfg.cm.code = code;
		hdmi.ip_data.cfg.cm.mode = HDMI_HDMI;
	}

	r = hdmi_power_on(dssdev);
	if (r) {
		DSSERR("failed to power on device\n");
		goto err2;
	}

	mutex_unlock(&hdmi.lock);
	return 0;

err2:
	if (dssdev->platform_disable)
		dssdev->platform_disable(dssdev);
err1:
	omap_dss_stop_device(dssdev);
err0:
	mutex_unlock(&hdmi.lock);
	return r;
}

void omapdss_hdmi_display_set_timing(struct omap_dss_device *dssdev)
{
	struct fb_videomode t;
	omapfb_dss2fb_timings(&dssdev->panel.timings, &t);
	/* also check interlaced timings */
	if (!hdmi_set_timings(&t, true)) {
		t.yres *= 2;
		t.vmode |= FB_VMODE_INTERLACED;
	}
	omapdss_hdmi_display_set_mode(dssdev, &t);

}

void hdmi_dump_regs(struct seq_file *s)
{
	mutex_lock(&hdmi.lock);

	if (hdmi_runtime_get())
		return;

	hdmi.ip_data.ops->dump_wrapper(&hdmi.ip_data, s);
	hdmi.ip_data.ops->dump_pll(&hdmi.ip_data, s);
	hdmi.ip_data.ops->dump_phy(&hdmi.ip_data, s);
	hdmi.ip_data.ops->dump_core(&hdmi.ip_data, s);

	hdmi_runtime_put();
	mutex_unlock(&hdmi.lock);
}

int omapdss_hdmi_read_edid(u8 *buf, int len)
{
	int r;

	mutex_lock(&hdmi.lock);

	r = hdmi_runtime_get();
	BUG_ON(r);

	r = hdmi.ip_data.ops->read_edid(&hdmi.ip_data, buf, len);

	hdmi_runtime_put();
	mutex_unlock(&hdmi.lock);

	return r;
}

bool omapdss_hdmi_detect(void)
{
	int r;

	mutex_lock(&hdmi.lock);

	r = hdmi_runtime_get();
	BUG_ON(r);

	r = hdmi.ip_data.ops->detect(&hdmi.ip_data);

	hdmi_runtime_put();
	mutex_unlock(&hdmi.lock);

	return r == 1;
}

static ssize_t hdmi_timings_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct fb_videomode *t = &hdmi.ip_data.cfg.timings;
	return snprintf(buf, PAGE_SIZE,
			"%u,%u/%u/%u/%u,%u/%u/%u/%u,%c/%c,%s-%u\n",
			t->pixclock ? (u32)PICOS2KHZ(t->pixclock) : 0,
			t->xres, t->right_margin, t->left_margin, t->hsync_len,
			t->yres, t->lower_margin, t->upper_margin, t->vsync_len,
			(t->sync & FB_SYNC_HOR_HIGH_ACT) ? '+' : '-',
			(t->sync & FB_SYNC_VERT_HIGH_ACT) ? '+' : '-',
			hdmi.ip_data.cfg.cm.mode == HDMI_HDMI ? "CEA" : "VESA",
			hdmi.ip_data.cfg.cm.code);
}

static ssize_t hdmi_timings_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t size)
{
	struct fb_videomode t = { .pixclock = 0 }, c;
	u32 code, x, y, old_rate, new_rate = 0;
	int mode = -1, pos = 0, pos2 = 0;
	char hsync, vsync, ilace;
	int hpd;

	/* check for timings */
	if (sscanf(buf, "%u,%u/%u/%u/%u,%u/%u/%u/%u,%c/%c%n",
		&t.pixclock,
		&t.xres, &t.right_margin, &t.left_margin, &t.hsync_len,
		&t.yres, &t.lower_margin, &t.upper_margin, &t.vsync_len,
		&hsync, &vsync, &pos) >= 11 &&
		(hsync == '+' || hsync == '-') &&
		(vsync == '+' || vsync == '-') && t.pixclock) {
		t.sync = (hsync == '+' ? FB_SYNC_HOR_HIGH_ACT : 0) |
			(vsync == '+' ? FB_SYNC_VERT_HIGH_ACT : 0);
		t.pixclock = KHZ2PICOS(t.pixclock);
		buf += pos;
		if (*buf == ',')
			buf++;
	} else {
		t.pixclock = 0;
	}

	/* check for CEA/VESA code/mode */
	pos = 0;
	if (sscanf(buf, "CEA-%u%n", &code, &pos) >= 1 &&
	    code < CEA_MODEDB_SIZE) {
		mode = HDMI_HDMI;
		if (t.pixclock)
			t.flag = cea_modes[code].flag;
		else
			t = cea_modes[code];
	} else if (sscanf(buf, "VESA-%u%n", &code, &pos) >= 1 &&
		   code < VESA_MODEDB_SIZE) {
		mode = HDMI_DVI;
		if (!t.pixclock)
			t = vesa_modes[code];
	} else if (!t.pixclock &&
		   sscanf(buf, "%u*%u%c,%uHz%n",
			  &t.xres, &t.yres, &ilace, &t.refresh, &pos) >= 4 &&
		   (ilace == 'p' || ilace == 'i')) {

		/* optional aspect ratio (defaults to 16:9) for 720p */
		if (sscanf(buf + pos, ",%u:%u%n", &x, &y, &pos2) >= 2 &&
		      (x * 9 == y * 16 || x * 3 == y * 4) && x) {
			pos += pos2;
		} else {
			x = t.yres >= 720 ? 16 : 4;
			y = t.yres >= 720 ? 9 : 3;
		}

		pr_err("looking for %u*%u%c,%uHz,%u:%u\n",
		       t.xres, t.yres, ilace, t.refresh, x, y);
		/* CEA shorthand */
#define RATE(x) ((x) + ((x) % 6 == 5))
		t.flag = (x * 9 == y * 16) ? FB_FLAG_RATIO_16_9 :
							FB_FLAG_RATIO_4_3;
		t.vmode = (ilace == 'i') ? FB_VMODE_INTERLACED :
							FB_VMODE_NONINTERLACED;
		for (code = 0; code < CEA_MODEDB_SIZE; code++) {
			c = cea_modes[code];
			if (t.xres == c.xres &&
			    t.yres == c.yres &&
			    RATE(t.refresh) == RATE(c.refresh) &&
			    t.vmode == (c.vmode & FB_VMODE_MASK) &&
			    t.flag == (c.flag &
				(FB_FLAG_RATIO_16_9 | FB_FLAG_RATIO_4_3)))
				break;
		}
		if (code >= CEA_MODEDB_SIZE)
			return -EINVAL;
		mode = HDMI_HDMI;
		if (t.refresh != c.refresh)
			new_rate = t.refresh;
		t = c;
	} else {
		mode = HDMI_DVI;
		code = 0;
	}

	if (!t.pixclock)
		return -EINVAL;

	pos2 = 0;
	if (new_rate || sscanf(buf + pos, ",%uHz%n", &new_rate, &pos2) == 1) {
		u64 temp;
		pos += pos2;
		new_rate = RATE(new_rate) * 1000000 /
					(1000 + ((new_rate % 6) == 5));
		old_rate = RATE(t.refresh) * 1000000 /
					(1000 + ((t.refresh % 6) == 5));
		pr_err("%u mHz => %u mHz (%u", old_rate, new_rate, t.pixclock);
		temp = (u64) t.pixclock * old_rate;
		do_div(temp, new_rate);
		t.pixclock = temp;
		pr_err("=>%u)\n", t.pixclock);
	}

	pr_info("setting %u,%u/%u/%u/%u,%u/%u/%u/%u,%c/%c,%s-%u\n",
			t.pixclock ? (u32) PICOS2KHZ(t.pixclock) : 0,
			t.xres, t.right_margin, t.left_margin, t.hsync_len,
			t.yres, t.lower_margin, t.upper_margin, t.vsync_len,
			(t.sync & FB_SYNC_HOR_HIGH_ACT) ? '+' : '-',
			(t.sync & FB_SYNC_VERT_HIGH_ACT) ? '+' : '-',
			mode == HDMI_HDMI ? "CEA" : "VESA",
			code);

	hpd = !strncmp(buf + pos, "+hpd", 4);
	if (hpd) {
		hdmi.force_timings = true;
		hdmi_panel_hpd_handler(0);
		msleep(500);
		hdmi_panel_set_mode(&t, code, mode);
		hdmi_panel_hpd_handler(1);
	} else {
		size = hdmi_panel_set_mode(&t,
						code, mode) ? : size;
	}
	return size;
}

DEVICE_ATTR(hdmi_timings, S_IRUGO | S_IWUSR, hdmi_timings_show,
							hdmi_timings_store);

int omapdss_hdmi_display_enable(struct omap_dss_device *dssdev)
{
	struct omap_dss_hdmi_data *priv = dssdev->data;
	int r = 0;

	DSSDBG("ENTER hdmi_display_enable\n");

	mutex_lock(&hdmi.lock);

	if (dssdev->manager == NULL) {
		DSSERR("failed to enable display: no manager\n");
		r = -ENODEV;
		goto err0;
	}

	hdmi.ip_data.hpd_gpio = priv->hpd_gpio;

	r = omap_dss_start_device(dssdev);
	if (r) {
		DSSERR("failed to start device\n");
		goto err0;
	}

	if (dssdev->platform_enable) {
		r = dssdev->platform_enable(dssdev);
		if (r) {
			DSSERR("failed to enable GPIO's\n");
			goto err1;
		}
	}

	r = hdmi_power_on(dssdev);
	if (r) {
		DSSERR("failed to power on device\n");
		goto err2;
	}

	hdmi.enabled = true;

	mutex_unlock(&hdmi.lock);
	return 0;

err2:
	if (dssdev->platform_disable)
		dssdev->platform_disable(dssdev);
err1:
	omap_dss_stop_device(dssdev);
err0:
	mutex_unlock(&hdmi.lock);
	return r;
}

void omapdss_hdmi_display_disable(struct omap_dss_device *dssdev)
{
	DSSDBG("Enter hdmi_display_disable\n");

	mutex_lock(&hdmi.lock);

	if (!hdmi.enabled) {
		DSSERR("HDMI display not enabled\n");
		goto done;
	}

	hdmi.enabled = false;
	hdmi.hdcp = false;

	if (!dssdev->sync_lost_error
		&& (dssdev->state != OMAP_DSS_DISPLAY_SUSPENDED)) {
		/* clear EDID and mode on disable only */
		hdmi.edid_set = false;
		hdmi.custom_set = false;
		pr_info("hdmi: clearing EDID info\n");
	}

	hdmi_power_off(dssdev);

	if (dssdev->platform_disable)
		dssdev->platform_disable(dssdev);

	omap_dss_stop_device(dssdev);
done:
	mutex_unlock(&hdmi.lock);
}

static irqreturn_t hdmi_irq_handler(int irq, void *arg)
{
	int r = 0;

	r = hdmi.ip_data.ops->irq_handler(&hdmi.ip_data);
	DSSDBG("Received HDMI IRQ = %08x\n", r);

	if (hdmi.hdmi_cec_irq_cb && (r & HDMI_CEC_INT))
		hdmi.hdmi_cec_irq_cb();

	r = hdmi.ip_data.ops->irq_process(&hdmi.ip_data);
	return IRQ_HANDLED;
}

static int hdmi_get_clocks(struct platform_device *pdev)
{
	struct clk *clk;

	clk = clk_get(&pdev->dev, "sys_clk");
	if (IS_ERR(clk)) {
		DSSERR("can't get sys_clk\n");
		return PTR_ERR(clk);
	}

	hdmi.sys_clk = clk;

	clk = clk_get(&pdev->dev, "dss_32khz_clk");
	if (IS_ERR(clk)) {
		DSSERR("can't get dss_32khz_clk\n");
		return PTR_ERR(clk);
	}
	hdmi.dss_32k_clk = clk;

	return 0;
}

static void hdmi_put_clocks(void)
{
	if (hdmi.sys_clk)
		clk_put(hdmi.sys_clk);
	if (hdmi.dss_32k_clk)
		clk_put(hdmi.dss_32k_clk);
}

#if defined(CONFIG_OMAP4_DSS_HDMI_AUDIO)
int hdmi_compute_acr(u32 sample_freq, u32 *n, u32 *cts)
{
	int dc;
	u32 deep_color;
	bool deep_color_correct = false;
	u32 pclk = hdmi.dssdev->panel.timings.pixel_clock;

	if (n == NULL || cts == NULL)
		return -EINVAL;

	dc = omapdss_hdmi_get_deepcolor();
	switch (dc) {
	case HDMI_DEEP_COLOR_36BIT:
		deep_color = 150;
		break;
	case HDMI_DEEP_COLOR_30BIT:
		deep_color = 125;
		break;
	case HDMI_DEEP_COLOR_24BIT:
	default:
		deep_color = 100;
		break;
	}

	/*
	 * When using deep color, the default N value (as in the HDMI
	 * specification) yields to an non-integer CTS. Hence, we
	 * modify it while keeping the restrictions described in
	 * section 7.2.1 of the HDMI 1.4a specification.
	 */
	switch (sample_freq) {
	case 32000:
	case 48000:
	case 96000:
	case 192000:
		if (deep_color == 125)
			if (pclk == 27027 || pclk == 74250)
				deep_color_correct = true;
		if (deep_color == 150)
			if (pclk == 27027)
				deep_color_correct = true;
		break;
	case 44100:
	case 88200:
	case 176400:
		if (deep_color == 125)
			if (pclk == 27027)
				deep_color_correct = true;
		break;
	default:
		return -EINVAL;
	}

	if (deep_color_correct) {
		switch (sample_freq) {
		case 32000:
			*n = 8192;
			break;
		case 44100:
			*n = 12544;
			break;
		case 48000:
			*n = 8192;
			break;
		case 88200:
			*n = 25088;
			break;
		case 96000:
			*n = 16384;
			break;
		case 176400:
			*n = 50176;
			break;
		case 192000:
			*n = 32768;
			break;
		default:
			return -EINVAL;
		}
	} else {
		switch (sample_freq) {
		case 32000:
			*n = 4096;
			break;
		case 44100:
			*n = 6272;
			break;
		case 48000:
			*n = 6144;
			break;
		case 88200:
			*n = 12544;
			break;
		case 96000:
			*n = 12288;
			break;
		case 176400:
			*n = 25088;
			break;
		case 192000:
			*n = 24576;
			break;
		default:
			return -EINVAL;
		}
	}
	/* Calculate CTS. See HDMI 1.3a or 1.4a specifications */
	*cts = pclk * (*n / 128) * deep_color / (sample_freq / 10);

	return 0;
}

int hdmi_audio_enable(bool enable)
{
	DSSDBG("audio_enable\n");

	hdmi.ip_data.ops->audio_enable(&hdmi.ip_data, enable);

	return 0;
}

int hdmi_audio_start(bool start)
{
	DSSDBG("audio_enable\n");

	hdmi.ip_data.ops->audio_start(&hdmi.ip_data, start);

	return 0;
}

bool hdmi_mode_has_audio(void)
{
	if (hdmi.ip_data.cfg.cm.mode == HDMI_HDMI)
		return true;
	else
		return false;
}

int hdmi_audio_config(struct snd_aes_iec958 *iec,
		struct snd_cea_861_aud_if *aud_if)
{
	return hdmi.ip_data.ops->audio_config(&hdmi.ip_data, iec, aud_if);
}

#endif

/* HDMI HW IP initialisation */
static int omapdss_hdmihw_probe(struct platform_device *pdev)
{
	struct resource *hdmi_mem;
	struct regulator *vdds_hdmi;
	int r;

	hdmi.pdev = pdev;

	mutex_init(&hdmi.lock);

	hdmi_mem = platform_get_resource(hdmi.pdev, IORESOURCE_MEM, 0);
	if (!hdmi_mem) {
		DSSERR("can't get IORESOURCE_MEM HDMI\n");
		return -EINVAL;
	}

	/* Base address taken from platform */
	hdmi.ip_data.base_wp = ioremap(hdmi_mem->start,
						resource_size(hdmi_mem));
	if (!hdmi.ip_data.base_wp) {
		DSSERR("can't ioremap WP\n");
		return -ENOMEM;
	}

	r = hdmi_get_clocks(pdev);
	if (r) {
		iounmap(hdmi.ip_data.base_wp);
		return r;
	}

	pm_runtime_enable(&pdev->dev);

	hdmi.hdmi_irq = platform_get_irq(pdev, 0);
	r = request_irq(hdmi.hdmi_irq, hdmi_irq_handler, 0, "OMAP HDMI", NULL);
	if (r < 0) {
		pr_err("hdmi: request_irq %s failed\n", pdev->name);
		return -EINVAL;
	}

	if (cpu_is_omap54xx()) {
		/* Request for regulator supply required by HDMI PHY */
		vdds_hdmi = regulator_get(&pdev->dev, "vdds_hdmi");
		if (IS_ERR(vdds_hdmi)) {
			DSSERR("can't get VDDS_HDMI regulator\n");
			return PTR_ERR(vdds_hdmi);
		}
	hdmi.vdds_hdmi = vdds_hdmi;
	}

	hdmi.ip_data.core_sys_offset = dss_feat_get_hdmi_core_sys_offset();
	hdmi.ip_data.core_av_offset = HDMI_CORE_AV;
	hdmi.ip_data.pll_offset = HDMI_PLLCTRL;
	hdmi.ip_data.phy_offset = HDMI_PHY;

	hdmi_panel_init();

	if (hdmi_get_current_hpd())
		hdmi_panel_hpd_handler(1);
	return 0;
}

static int omapdss_hdmihw_remove(struct platform_device *pdev)
{
	hdmi_panel_exit();

	pm_runtime_disable(&pdev->dev);

	hdmi_put_clocks();

	if (cpu_is_omap54xx()) {
		regulator_put(hdmi.vdds_hdmi);
		hdmi.vdds_hdmi = NULL;
	}

	iounmap(hdmi.ip_data.base_wp);

	return 0;
}

static int hdmi_runtime_suspend(struct device *dev)
{
	clk_disable(hdmi.sys_clk);

	clk_disable(hdmi.dss_32k_clk);

	dispc_runtime_put();

	return 0;
}

static int hdmi_runtime_resume(struct device *dev)
{
	int r;

	r = dispc_runtime_get();
	if (r < 0)
		return r;

	clk_enable(hdmi.sys_clk);
	clk_enable(hdmi.dss_32k_clk);

	return 0;
}

static const struct dev_pm_ops hdmi_pm_ops = {
	.runtime_suspend = hdmi_runtime_suspend,
	.runtime_resume = hdmi_runtime_resume,
};

static struct platform_driver omapdss_hdmihw_driver = {
	.probe          = omapdss_hdmihw_probe,
	.remove         = omapdss_hdmihw_remove,
	.driver         = {
		.name   = "omapdss_hdmi",
		.owner  = THIS_MODULE,
		.pm	= &hdmi_pm_ops,
	},
};

int hdmi_init_platform_driver(void)
{
	return platform_driver_register(&omapdss_hdmihw_driver);
}

void hdmi_uninit_platform_driver(void)
{
	return platform_driver_unregister(&omapdss_hdmihw_driver);
}
