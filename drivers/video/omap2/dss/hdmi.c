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
#include <linux/clk.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/of_gpio.h>
#include <linux/of_i2c.h>
#include <video/omapdss.h>
#include <linux/i2c.h>
#include <linux/i2c-algo-bit.h>

#include "ti_hdmi.h"
#include "dss.h"
#include "dss_features.h"

/* HDMI EDID Length move this */
#define HDMI_EDID_MAX_LENGTH			256
#define EDID_TIMING_DESCRIPTOR_SIZE		0x12
#define EDID_DESCRIPTOR_BLOCK0_ADDRESS		0x36
#define EDID_DESCRIPTOR_BLOCK1_ADDRESS		0x80
#define EDID_SIZE_BLOCK0_TIMING_DESCRIPTOR	4
#define EDID_SIZE_BLOCK1_TIMING_DESCRIPTOR	4

static struct {
	struct mutex lock;
	struct platform_device *pdev;
#if defined(CONFIG_OMAP4_DSS_HDMI_AUDIO) || \
	defined(CONFIG_OMAP5_DSS_HDMI_AUDIO)
	struct platform_device *audio_pdev;
#endif

	struct hdmi_ip_data ip_data;
	int hdmi_irq;

	struct clk *sys_clk;
	struct regulator *vdda_hdmi_dac_reg;

	/* voltage required by the ip*/
	u32 microvolt_min;
	u32 microvolt_max;

	/* GPIO pins */
	int ct_cp_hpd_gpio;
	int ls_oe_gpio;
	int hpd_gpio;

	/* level shifter state */
	enum level_shifter_state ls_state;

	/*
	 * i2c adapter info(this could be either a bitbanged adapter, or a
	 * 'real' i2c adapter
	 */
	struct i2c_adapter *adap;

	/* these are needed in case it's a bitbanged adapter */
	struct i2c_algo_bit_data bit_data;
	int scl_pin;
	int sda_pin;

	struct omap_dss_output output;
} hdmi;

/*
 * Logic for the below structure :
 * user enters the CEA or VESA timings by specifying the HDMI/DVI code.
 * There is a correspondence between CEA/VESA timing and code, please
 * refer to section 6.3 in HDMI 1.3 specification for timing code.
 *
 * In the below structure, cea_vesa_timings corresponds to all OMAP4
 * supported CEA and VESA timing values.code_cea corresponds to the CEA
 * code, It is used to get the timing from cea_vesa_timing array.Similarly
 * with code_vesa. Code_index is used for back mapping, that is once EDID
 * is read from the TV, EDID is parsed to find the timing values and then
 * map it to corresponding CEA or VESA index.
 */

static const struct hdmi_config cea_timings[] = {
	{
		{ 640, 480, 25200, 96, 16, 48, 2, 10, 33,
			OMAPDSS_SIG_ACTIVE_LOW, OMAPDSS_SIG_ACTIVE_LOW,
			false, },
		{ 1, HDMI_HDMI },
	},
	{
		{ 720, 480, 27027, 62, 16, 60, 6, 9, 30,
			OMAPDSS_SIG_ACTIVE_LOW, OMAPDSS_SIG_ACTIVE_LOW,
			false, },
		{ 2, HDMI_HDMI },
	},
	{
		{ 1280, 720, 74250, 40, 110, 220, 5, 5, 20,
			OMAPDSS_SIG_ACTIVE_HIGH, OMAPDSS_SIG_ACTIVE_HIGH,
			false, },
		{ 4, HDMI_HDMI },
	},
	{
		{ 1920, 540, 74250, 44, 88, 148, 5, 2, 15,
			OMAPDSS_SIG_ACTIVE_HIGH, OMAPDSS_SIG_ACTIVE_HIGH,
			true, },
		{ 5, HDMI_HDMI },
	},
	{
		{ 1440, 240, 27027, 124, 38, 114, 3, 4, 15,
			OMAPDSS_SIG_ACTIVE_LOW, OMAPDSS_SIG_ACTIVE_LOW,
			true, },
		{ 6, HDMI_HDMI },
	},
	{
		{ 1920, 1080, 148500, 44, 88, 148, 5, 4, 36,
			OMAPDSS_SIG_ACTIVE_HIGH, OMAPDSS_SIG_ACTIVE_HIGH,
			false, },
		{ 16, HDMI_HDMI },
	},
	{
		{ 720, 576, 27000, 64, 12, 68, 5, 5, 39,
			OMAPDSS_SIG_ACTIVE_LOW, OMAPDSS_SIG_ACTIVE_LOW,
			false, },
		{ 17, HDMI_HDMI },
	},
	{
		{ 1280, 720, 74250, 40, 440, 220, 5, 5, 20,
			OMAPDSS_SIG_ACTIVE_HIGH, OMAPDSS_SIG_ACTIVE_HIGH,
			false, },
		{ 19, HDMI_HDMI },
	},
	{
		{ 1920, 540, 74250, 44, 528, 148, 5, 2, 15,
			OMAPDSS_SIG_ACTIVE_HIGH, OMAPDSS_SIG_ACTIVE_HIGH,
			true, },
		{ 20, HDMI_HDMI },
	},
	{
		{ 1440, 288, 27000, 126, 24, 138, 3, 2, 19,
			OMAPDSS_SIG_ACTIVE_LOW, OMAPDSS_SIG_ACTIVE_LOW,
			true, },
		{ 21, HDMI_HDMI },
	},
	{
		{ 1440, 576, 54000, 128, 24, 136, 5, 5, 39,
			OMAPDSS_SIG_ACTIVE_LOW, OMAPDSS_SIG_ACTIVE_LOW,
			false, },
		{ 29, HDMI_HDMI },
	},
	{
		{ 1920, 1080, 148500, 44, 528, 148, 5, 4, 36,
			OMAPDSS_SIG_ACTIVE_HIGH, OMAPDSS_SIG_ACTIVE_HIGH,
			false, },
		{ 31, HDMI_HDMI },
	},
	{
		{ 1920, 1080, 74250, 44, 638, 148, 5, 4, 36,
			OMAPDSS_SIG_ACTIVE_HIGH, OMAPDSS_SIG_ACTIVE_HIGH,
			false, },
		{ 32, HDMI_HDMI },
	},
	{
		{ 2880, 480, 108108, 248, 64, 240, 6, 9, 30,
			OMAPDSS_SIG_ACTIVE_LOW, OMAPDSS_SIG_ACTIVE_LOW,
			false, },
		{ 35, HDMI_HDMI },
	},
	{
		{ 2880, 576, 108000, 256, 48, 272, 5, 5, 39,
			OMAPDSS_SIG_ACTIVE_LOW, OMAPDSS_SIG_ACTIVE_LOW,
			false, },
		{ 37, HDMI_HDMI },
	},
};

static const struct hdmi_config vesa_timings[] = {
/* VESA From Here */
	{
		{ 640, 480, 25175, 96, 16, 48, 2, 11, 31,
			OMAPDSS_SIG_ACTIVE_LOW, OMAPDSS_SIG_ACTIVE_LOW,
			false, },
		{ 4, HDMI_DVI },
	},
	{
		{ 800, 600, 40000, 128, 40, 88, 4, 1, 23,
			OMAPDSS_SIG_ACTIVE_HIGH, OMAPDSS_SIG_ACTIVE_HIGH,
			false, },
		{ 9, HDMI_DVI },
	},
	{
		{ 848, 480, 33750, 112, 16, 112, 8, 6, 23,
			OMAPDSS_SIG_ACTIVE_HIGH, OMAPDSS_SIG_ACTIVE_HIGH,
			false, },
		{ 0xE, HDMI_DVI },
	},
	{
		{ 1280, 768, 79500, 128, 64, 192, 7, 3, 20,
			OMAPDSS_SIG_ACTIVE_HIGH, OMAPDSS_SIG_ACTIVE_LOW,
			false, },
		{ 0x17, HDMI_DVI },
	},
	{
		{ 1280, 800, 83500, 128, 72, 200, 6, 3, 22,
			OMAPDSS_SIG_ACTIVE_HIGH, OMAPDSS_SIG_ACTIVE_LOW,
			false, },
		{ 0x1C, HDMI_DVI },
	},
	{
		{ 1360, 768, 85500, 112, 64, 256, 6, 3, 18,
			OMAPDSS_SIG_ACTIVE_HIGH, OMAPDSS_SIG_ACTIVE_HIGH,
			false, },
		{ 0x27, HDMI_DVI },
	},
	{
		{ 1280, 960, 108000, 112, 96, 312, 3, 1, 36,
			OMAPDSS_SIG_ACTIVE_HIGH, OMAPDSS_SIG_ACTIVE_HIGH,
			false, },
		{ 0x20, HDMI_DVI },
	},
	{
		{ 1280, 1024, 108000, 112, 48, 248, 3, 1, 38,
			OMAPDSS_SIG_ACTIVE_HIGH, OMAPDSS_SIG_ACTIVE_HIGH,
			false, },
		{ 0x23, HDMI_DVI },
	},
	{
		{ 1024, 768, 65000, 136, 24, 160, 6, 3, 29,
			OMAPDSS_SIG_ACTIVE_LOW, OMAPDSS_SIG_ACTIVE_LOW,
			false, },
		{ 0x10, HDMI_DVI },
	},
	{
		{ 1400, 1050, 121750, 144, 88, 232, 4, 3, 32,
			OMAPDSS_SIG_ACTIVE_HIGH, OMAPDSS_SIG_ACTIVE_LOW,
			false, },
		{ 0x2A, HDMI_DVI },
	},
	{
		{ 1440, 900, 106500, 152, 80, 232, 6, 3, 25,
			OMAPDSS_SIG_ACTIVE_HIGH, OMAPDSS_SIG_ACTIVE_LOW,
			false, },
		{ 0x2F, HDMI_DVI },
	},
	{
		{ 1680, 1050, 146250, 176 , 104, 280, 6, 3, 30,
			OMAPDSS_SIG_ACTIVE_HIGH, OMAPDSS_SIG_ACTIVE_LOW,
			false, },
		{ 0x3A, HDMI_DVI },
	},
	{
		{ 1366, 768, 85500, 143, 70, 213, 3, 3, 24,
			OMAPDSS_SIG_ACTIVE_HIGH, OMAPDSS_SIG_ACTIVE_HIGH,
			false, },
		{ 0x51, HDMI_DVI },
	},
	{
		{ 1920, 1080, 148500, 44, 148, 80, 5, 4, 36,
			OMAPDSS_SIG_ACTIVE_HIGH, OMAPDSS_SIG_ACTIVE_HIGH,
			false, },
		{ 0x52, HDMI_DVI },
	},
	{
		{ 1280, 768, 68250, 32, 48, 80, 7, 3, 12,
			OMAPDSS_SIG_ACTIVE_LOW, OMAPDSS_SIG_ACTIVE_HIGH,
			false, },
		{ 0x16, HDMI_DVI },
	},
	{
		{ 1400, 1050, 101000, 32, 48, 80, 4, 3, 23,
			OMAPDSS_SIG_ACTIVE_LOW, OMAPDSS_SIG_ACTIVE_HIGH,
			false, },
		{ 0x29, HDMI_DVI },
	},
	{
		{ 1680, 1050, 119000, 32, 48, 80, 6, 3, 21,
			OMAPDSS_SIG_ACTIVE_LOW, OMAPDSS_SIG_ACTIVE_HIGH,
			false, },
		{ 0x39, HDMI_DVI },
	},
	{
		{ 1280, 800, 79500, 32, 48, 80, 6, 3, 14,
			OMAPDSS_SIG_ACTIVE_LOW, OMAPDSS_SIG_ACTIVE_HIGH,
			false, },
		{ 0x1B, HDMI_DVI },
	},
	{
		{ 1280, 720, 74250, 40, 110, 220, 5, 5, 20,
			OMAPDSS_SIG_ACTIVE_HIGH, OMAPDSS_SIG_ACTIVE_HIGH,
			false, },
		{ 0x55, HDMI_DVI },
	},
	{
		{ 1920, 1200, 154000, 32, 48, 80, 6, 3, 26,
			OMAPDSS_SIG_ACTIVE_LOW, OMAPDSS_SIG_ACTIVE_HIGH,
			false, },
		{ 0x44, HDMI_DVI },
	},
};

static const struct hdmi_config s3d_timings[] = {
	{
		{ 1280, 1470, 148500, 40, 110, 220, 5, 5, 20,
			OMAPDSS_SIG_ACTIVE_HIGH, OMAPDSS_SIG_ACTIVE_HIGH,
			false, },
		{ 4, HDMI_HDMI },
	},
	{
		{ 1280, 1470, 148500, 40, 440, 220, 5, 5, 20,
			OMAPDSS_SIG_ACTIVE_HIGH, OMAPDSS_SIG_ACTIVE_HIGH,
			false, },
		{ 19, HDMI_HDMI },
	},
	{
		{ 1920, 2205, 148500, 44, 638, 148, 5, 4, 36,
			OMAPDSS_SIG_ACTIVE_HIGH, OMAPDSS_SIG_ACTIVE_HIGH,
			false, },
		{ 32, HDMI_HDMI },
	},
};

static void hdmi_set_ls_state(enum level_shifter_state state)
{
	bool hpd_enable = false;
	bool ls_enable = false;

	/* return early if we have nothing to do */
	if (state == hdmi.ls_state)
		return;

	sel_i2c();

	switch (state) {
	case LS_HPD_ON:
		hpd_enable = true;
		break;

	case LS_ENABLED:
		hpd_enable = true;
		ls_enable = true;
		break;

	case LS_DISABLED:
	default:
		break;
	}

	gpio_set_value_cansleep(hdmi.ct_cp_hpd_gpio, hpd_enable);

	gpio_set_value_cansleep(hdmi.ls_oe_gpio, ls_enable);

	/* wait 300us after asserting CT_CP_HPD for the 5V rail to reach 90% */
	if (hdmi.ls_state == LS_DISABLED)
		udelay(300);

	hdmi.ls_state = state;

	sel_hdmi();
}

static int hdmi_runtime_get(void)
{
	int r;

	DSSDBG("hdmi_runtime_get\n");

	r = pm_runtime_get_sync(&hdmi.pdev->dev);
	WARN_ON(r < 0);
	if (r < 0)
		return r;

	return 0;
}

static void hdmi_runtime_put(void)
{
	int r;

	DSSDBG("hdmi_runtime_put\n");

	r = pm_runtime_put_sync(&hdmi.pdev->dev);
	WARN_ON(r < 0 && r != -ENOSYS);
}

static int __init hdmi_init_display(struct omap_dss_device *dssdev)
{
	int r;

	struct gpio gpios[] = {
		{ hdmi.ct_cp_hpd_gpio, GPIOF_OUT_INIT_LOW, "hdmi_ct_cp_hpd" },
		{ hdmi.ls_oe_gpio, GPIOF_OUT_INIT_LOW, "hdmi_ls_oe" },
		{ hdmi.hpd_gpio, GPIOF_DIR_IN, "hdmi_hpd" },
	};

	DSSDBG("init_display\n");

	if (hdmi.vdda_hdmi_dac_reg == NULL) {
		struct regulator *reg;

		reg = devm_regulator_get(&hdmi.pdev->dev, "vdda_hdmi_dac");

		/* DT HACK: try VDAC to make omapdss work for o4 sdp/panda */
		if (IS_ERR(reg))
			reg = devm_regulator_get(&hdmi.pdev->dev, "VDAC");

		if (IS_ERR(reg)) {
			DSSERR("can't get VDDA_HDMI_DAC regulator\n");
			return PTR_ERR(reg);
		}

		r = regulator_set_voltage(reg, hdmi.microvolt_min, hdmi.microvolt_max);
		if(r) {
			DSSERR("can't set the voltage regulator");
		}

		hdmi.vdda_hdmi_dac_reg = reg;
	}

	r = gpio_request_array(gpios, ARRAY_SIZE(gpios));
	if (r)
		return r;

	return 0;
}

static void hdmi_uninit_display(struct omap_dss_device *dssdev)
{
	DSSDBG("uninit_display\n");

	gpio_free(hdmi.ct_cp_hpd_gpio);
	gpio_free(hdmi.ls_oe_gpio);
	gpio_free(hdmi.hpd_gpio);
}

static const struct hdmi_config *hdmi_find_timing(
					const struct hdmi_config *timings_arr,
					int len)
{
	int i;

	for (i = 0; i < len; i++) {
		if (timings_arr[i].cm.code == hdmi.ip_data.cfg.cm.code)
			return &timings_arr[i];
	}
	return NULL;
}

static const struct hdmi_config *hdmi_get_timings(void)
{
       const struct hdmi_config *arr;
       int len;

	if (!hdmi.ip_data.cfg.s3d_enabled) {
		if (hdmi.ip_data.cfg.cm.mode == HDMI_DVI) {
			arr = vesa_timings;
			len = ARRAY_SIZE(vesa_timings);
		} else {
			arr = cea_timings;
			len = ARRAY_SIZE(cea_timings);
		}
	} else {
		arr = s3d_timings;
		len = ARRAY_SIZE(s3d_timings);
	}

	return hdmi_find_timing(arr, len);
}

static bool hdmi_timings_compare(struct omap_video_timings *timing1,
				const struct omap_video_timings *timing2)
{
	int timing1_vsync, timing1_hsync, timing2_vsync, timing2_hsync;

	if ((DIV_ROUND_CLOSEST(timing2->pixel_clock, 1000) ==
			DIV_ROUND_CLOSEST(timing1->pixel_clock, 1000)) &&
		(timing2->x_res == timing1->x_res) &&
		(timing2->y_res == timing1->y_res)) {

		timing2_hsync = timing2->hfp + timing2->hsw + timing2->hbp;
		timing1_hsync = timing1->hfp + timing1->hsw + timing1->hbp;
		timing2_vsync = timing2->vfp + timing2->vsw + timing2->vbp;
		timing1_vsync = timing2->vfp + timing2->vsw + timing2->vbp;

		DSSDBG("timing1_hsync = %d timing1_vsync = %d"\
			"timing2_hsync = %d timing2_vsync = %d\n",
			timing1_hsync, timing1_vsync,
			timing2_hsync, timing2_vsync);

		if ((timing1_hsync == timing2_hsync) &&
			(timing1_vsync == timing2_vsync)) {
			return true;
		}
	}
	return false;
}

static struct hdmi_cm hdmi_get_code(struct omap_video_timings *timing)
{
	int i;
	struct hdmi_cm cm = {-1};
	DSSDBG("hdmi_get_code\n");

	for (i = 0; i < ARRAY_SIZE(cea_timings); i++) {
		if (hdmi_timings_compare(timing, &cea_timings[i].timings)) {
			cm = cea_timings[i].cm;
			goto end;
		}
	}
	for (i = 0; i < ARRAY_SIZE(vesa_timings); i++) {
		if (hdmi_timings_compare(timing, &vesa_timings[i].timings)) {
			cm = vesa_timings[i].cm;
			goto end;
		}
	}
	for (i = 0; i < ARRAY_SIZE(s3d_timings); i++) {
		if (hdmi_timings_compare(timing, &s3d_timings[i].timings)) {
			cm = s3d_timings[i].cm;
			goto end;
		}
	}

end:	return cm;

}

unsigned long hdmi_get_pixel_clock(void)
{
	/* HDMI Pixel Clock in Mhz */
	return hdmi.ip_data.cfg.timings.pixel_clock * 1000;
}

static int hdmi_compute_pll(struct omap_dss_device *dssdev, int phy,
		struct hdmi_pll_info *pi)
{
	unsigned long clkin, refclk;
	int phy_calc;
	unsigned long regn_max, regn_min, regm_min, regm_max;
	unsigned long fint_min, fint_max;
	unsigned long dco_low_min, dco_high_min;
	bool found = false;
	u32 mf;

	clkin = clk_get_rate(hdmi.sys_clk) / 10000;

	fint_min = dss_feat_get_param_min(FEAT_PARAM_HDMIPLL_FINT) / 10000;
	fint_max = dss_feat_get_param_max(FEAT_PARAM_HDMIPLL_FINT) / 10000;

	/* clkin limits */
	/* .62 MHz < CLKIN/REGN < 2.5MHz */
	regn_min = clkin / fint_max + 1;
	regn_max = clkin / fint_min;

	/* Fractional limits on REGM */
	regm_min = dss_feat_get_param_min(FEAT_PARAM_HDMIPLL_REGM);
	regm_max = dss_feat_get_param_max(FEAT_PARAM_HDMIPLL_REGM);

	/* DCO frequency ranges */

	/* DCO lowest frequency supported */
	dco_low_min = dss_feat_get_param_min(FEAT_PARAM_DCOFREQ_LOW) / 10000;

	/* Starting frequency of high frequency range(in Mhz) */
	dco_high_min = dss_feat_get_param_min(FEAT_PARAM_DCOFREQ_HIGH);

	/* set dcofreq to 1 if required clock is > 1.25GHz */
	pi->dcofreq = phy > (dco_high_min / 10000);

	if (phy < dco_low_min) {
		/* Calculate CLKOUTLDO - low frequency */
		for (pi->regn = regn_min; pi->regn < regn_max; pi->regn++) {
			refclk = clkin / pi->regn;

			regm_min = ((dco_low_min / refclk) < regm_min) ?
					regm_min : (dco_low_min / refclk);

			for (pi->regm2 = 3; pi->regm2 <= 127; pi->regm2++) {
				pi->regm = phy * pi->regm2 / refclk;
				if (pi->regm < regm_min || pi->regm > regm_max)
					continue;

				pi->regsd = DIV_ROUND_UP((pi->regm * clkin / 100),
							pi->regn * 250);
				phy_calc = clkin * pi->regm / pi->regn /
						pi->regm2;

				if (pi->regsd && pi->regsd < 255 &&
					phy_calc <= phy) {
					found = true;
					break;
				}
			}

			if (found)
				break;
		}
	} else {
		pi->regm2 = 1;

		/* CLKDCOLDO - high frequency */
		for (pi->regn = regn_min; pi->regn < regn_max; pi->regn++) {
			refclk = clkin / pi->regn;
			pi->regm = phy / refclk;

			if (pi->regm < regm_min || pi->regm > regm_max)
				continue;

			pi->regsd = DIV_ROUND_UP((pi->regm * clkin / 100),
						pi->regn * 250);

			phy_calc = clkin * pi->regm / pi->regn;

			if (pi->regsd < 255 && phy_calc <= phy) {
				found = true;
				break;
			}
		}
	}

	if (!found) {
		DSSERR("Failed to find pll settings\n");
		return 1;
	}

	/*
	 * fractional multiplier is remainder of the difference between
	 * multiplier and actual phy(required pixel clock thus should be
	 * multiplied by 2^18(262144) divided by the reference clock
	 */
	mf = (phy - refclk * pi->regm / pi->regm2) * 262144;
	pi->regmf = pi->regm2 * mf / refclk;

	if (pi->regmf > 262144)
		pi->regmf = 0;

	/* Set the reference clock to sysclk reference */
	pi->refsel = HDMI_REFSEL_SYSCLK;

	DSSERR("M = %d Mf = %d\n", pi->regm, pi->regmf);
	DSSERR("range = %d sd = %d\n", pi->dcofreq, pi->regsd);

	return 0;
}

static int hdmi_power_on_core(struct omap_dss_device *dssdev)
{
	int r;

	hdmi_set_ls_state(LS_ENABLED);

	r = regulator_enable(hdmi.vdda_hdmi_dac_reg);
	if (r)
		goto err_vdac_enable;

	r = hdmi_runtime_get();
	if (r)
		goto err_runtime_get;

	/* Make selection of HDMI in DSS */
	dss_select_hdmi_venc_clk_source(DSS_HDMI_M_PCLK);

	return 0;

err_runtime_get:
	regulator_disable(hdmi.vdda_hdmi_dac_reg);
err_vdac_enable:
	hdmi_set_ls_state(LS_HPD_ON);
	return r;
}

static void hdmi_power_off_core(struct omap_dss_device *dssdev)
{
	hdmi_runtime_put();
	regulator_disable(hdmi.vdda_hdmi_dac_reg);

	hdmi_set_ls_state(LS_HPD_ON);
}

static int hdmi_power_on_full(struct omap_dss_device *dssdev)
{
	int r;
	struct omap_video_timings *p;
	struct omap_overlay_manager *mgr = dssdev->output->manager;
	unsigned long phy;

	r = hdmi_power_on_core(dssdev);
	if (r)
		return r;

	dss_mgr_disable(mgr);

	p = &hdmi.ip_data.cfg.timings;

	DSSDBG("hdmi_power_on x_res= %d y_res = %d\n", p->x_res, p->y_res);

	switch (hdmi.ip_data.cfg.deep_color) {
	case HDMI_DEEP_COLOR_30BIT:
		phy = (p->pixel_clock * 125) / 100 ;
		break;
	case HDMI_DEEP_COLOR_36BIT:
		phy = (p->pixel_clock * 150) / 100;

		if (phy >= dss_feat_get_param_max(FEAT_PARAM_HDMI_PCLK)) {
			DSSERR("36 bit deep color not supported for the pixel clock %d\n",
				p->pixel_clock);
			goto err_deep_color;
		}
		break;
	case HDMI_DEEP_COLOR_24BIT:
	default:
		phy = p->pixel_clock;
		break;
	}

	if (hdmi_compute_pll(dssdev, phy, &hdmi.ip_data.pll_data))
		goto err_pll_compute;

	hdmi.ip_data.ops->video_disable(&hdmi.ip_data);

	/* config the PLL and PHY hdmi_set_pll_pwrfirst */
	r = hdmi.ip_data.ops->pll_enable(&hdmi.ip_data);
	if (r) {
		DSSDBG("Failed to lock PLL\n");
		goto err_pll_enable;
	}

	r = hdmi.ip_data.ops->phy_enable(&hdmi.ip_data);
	/*
	 * DRA7xx doesn't show the correct PHY transition changes in the
	 * WP_PWR_CTRL register, need to investigate
	 */
	if (omapdss_get_version() == OMAPDSS_VER_DRA7xx)
		r = 0;

	if (r) {
		DSSDBG("Failed to start PHY\n");
		goto err_phy_enable;
	}

	hdmi.ip_data.ops->video_configure(&hdmi.ip_data);

	/* bypass TV gamma table */
	dispc_enable_gamma_table(0);

	/* tv size */
	dss_mgr_set_timings(mgr, p);

	r = hdmi.ip_data.ops->video_enable(&hdmi.ip_data);
	if (r)
		goto err_vid_enable;

	r = dss_mgr_enable(mgr);
	if (r)
		goto err_mgr_enable;

	return 0;

err_mgr_enable:
	hdmi.ip_data.ops->video_disable(&hdmi.ip_data);
err_vid_enable:
	hdmi.ip_data.ops->phy_disable(&hdmi.ip_data);
err_phy_enable:
	hdmi.ip_data.ops->pll_disable(&hdmi.ip_data);
err_pll_enable:
err_pll_compute:
err_deep_color:
	hdmi_power_off_core(dssdev);
	return -EIO;
}

static void hdmi_power_off_full(struct omap_dss_device *dssdev)
{
	struct omap_overlay_manager *mgr = dssdev->output->manager;

	dss_mgr_disable(mgr);

	hdmi.ip_data.ops->video_disable(&hdmi.ip_data);
	hdmi.ip_data.ops->phy_disable(&hdmi.ip_data);
	hdmi.ip_data.ops->pll_disable(&hdmi.ip_data);

	hdmi.ip_data.cfg.deep_color = HDMI_DEEP_COLOR_24BIT;

	hdmi_power_off_core(dssdev);
}

int omapdss_hdmi_set_deepcolor(struct omap_dss_device *dssdev, int val,
		bool hdmi_restart)
{
	int r;

	if (!hdmi_restart) {
		hdmi.ip_data.cfg.deep_color = val;
		return 0;
	}

	omapdss_hdmi_display_disable(dssdev);

	hdmi.ip_data.cfg.deep_color = val;

	r = omapdss_hdmi_display_enable(dssdev);
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

int omapdss_hdmi_display_check_timing(struct omap_dss_device *dssdev,
					struct omap_video_timings *timings)
{
	struct hdmi_cm cm;

	cm = hdmi_get_code(timings);
	if (cm.code == -1) {
		return -EINVAL;
	}

	return 0;

}

int omapdss_hdmi_display_3d_enable(struct omap_dss_device *dssdev,
					struct s3d_disp_info *info, int code)
{
	struct omap_dss_output *out = dssdev->output;
	int r = 0;

	DSSDBG("ENTER hdmi_display_3d_enable\n");

	mutex_lock(&hdmi.lock);

	if (out == NULL || out->manager == NULL) {
		DSSERR("failed to enable display: no output/manager\n");
		r = -ENODEV;
		goto err0;
	}

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
			hdmi.ip_data.cfg.s3d_info.frame_struct =
				HDMI_S3D_FRAME_PACKING;
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
			hdmi.ip_data.cfg.s3d_info.frame_struct =
				HDMI_S3D_SIDE_BY_SIDE_FULL;
			hdmi.ip_data.cfg.s3d_info.subsamp = true;
			hdmi.ip_data.cfg.s3d_info.subsamp_pos =
				HDMI_S3D_HOR_EL_ER;
			hdmi.ip_data.cfg.s3d_enabled = true;
			hdmi.ip_data.cfg.s3d_info.vsi_enabled = true;
		} else if (info->sub_samp == S3D_DISP_SUB_SAMPLE_H) {
			hdmi.ip_data.cfg.s3d_info.frame_struct =
				HDMI_S3D_SIDE_BY_SIDE_HALF;
			hdmi.ip_data.cfg.s3d_info.subsamp = true;
			hdmi.ip_data.cfg.s3d_info.subsamp_pos =
				HDMI_S3D_HOR_EL_ER;
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

	r = hdmi_power_on_full(dssdev);
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

void omapdss_hdmi_display_set_timing(struct omap_dss_device *dssdev,
		struct omap_video_timings *timings)
{
	struct hdmi_cm cm;
	const struct hdmi_config *t;

	mutex_lock(&hdmi.lock);

	cm = hdmi_get_code(timings);
	hdmi.ip_data.cfg.cm = cm;

	t = hdmi_get_timings();
	if (t != NULL)
		hdmi.ip_data.cfg = *t;

	mutex_unlock(&hdmi.lock);
}

static void hdmi_dump_regs(struct seq_file *s)
{
	mutex_lock(&hdmi.lock);

	if (hdmi_runtime_get()) {
		mutex_unlock(&hdmi.lock);
		return;
	}

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
	enum level_shifter_state restore_state = hdmi.ls_state;

	/* skip if no monitor attached */
	if (!gpio_get_value(hdmi.hpd_gpio))
		return -ENODEV;

	mutex_lock(&hdmi.lock);

	r = hdmi_runtime_get();
	BUG_ON(r);


	hdmi_set_ls_state(LS_ENABLED);

	r = hdmi.ip_data.ops->read_edid(&hdmi.ip_data, buf, len);

	/* restore level shifter state */
	hdmi_set_ls_state(restore_state);

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

int omapdss_hdmi_display_enable(struct omap_dss_device *dssdev)
{
	struct omap_dss_output *out = dssdev->output;
	int r = 0;

	DSSDBG("ENTER hdmi_display_enable\n");

	mutex_lock(&hdmi.lock);

	if (out == NULL || out->manager == NULL) {
		DSSERR("failed to enable display: no output/manager\n");
		r = -ENODEV;
		goto err0;
	}

	hdmi.ip_data.hpd_gpio = hdmi.hpd_gpio;

	r = omap_dss_start_device(dssdev);
	if (r) {
		DSSERR("failed to start device\n");
		goto err0;
	}

	r = hdmi_power_on_full(dssdev);
	if (r) {
		DSSERR("failed to power on device\n");
		goto err1;
	}

	mutex_unlock(&hdmi.lock);
	return 0;

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

	hdmi_power_off_full(dssdev);

	omap_dss_stop_device(dssdev);

	mutex_unlock(&hdmi.lock);
}

int omapdss_hdmi_core_enable(struct omap_dss_device *dssdev)
{
	int r = 0;

	DSSDBG("ENTER omapdss_hdmi_core_enable\n");

	mutex_lock(&hdmi.lock);

	hdmi.ip_data.hpd_gpio = hdmi.hpd_gpio;

	r = hdmi_power_on_core(dssdev);
	if (r) {
		DSSERR("failed to power on device\n");
		goto err0;
	}

	mutex_unlock(&hdmi.lock);
	return 0;

err0:
	mutex_unlock(&hdmi.lock);
	return r;
}

void omapdss_hdmi_core_disable(struct omap_dss_device *dssdev)
{
	DSSDBG("Enter omapdss_hdmi_core_disable\n");

	mutex_lock(&hdmi.lock);

	hdmi_power_off_core(dssdev);

	mutex_unlock(&hdmi.lock);
}

static irqreturn_t hdmi_irq_handler(int irq, void *arg)
{
	int r = 0;

	r = hdmi.ip_data.ops->irq_handler(&hdmi.ip_data);
	DSSDBG("Received HDMI IRQ = %08x\n", r);

	r = hdmi.ip_data.ops->irq_core_handler(&hdmi.ip_data);
	DSSDBG("Received HDMI core IRQ = %08x\n", r);

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

	return 0;
}

static void hdmi_put_clocks(void)
{
	if (hdmi.sys_clk)
		clk_put(hdmi.sys_clk);
}

#if defined(CONFIG_OMAP4_DSS_HDMI_AUDIO) || \
	defined(CONFIG_OMAP5_DSS_HDMI_AUDIO)
static int hdmi_probe_audio(struct platform_device *pdev)
{
	struct resource *res;
	struct platform_device *aud_pdev;
	u32 port_offset, port_size;
	struct resource aud_res[2] = {
		DEFINE_RES_MEM(-1, -1),
		DEFINE_RES_DMA(-1),
	};

	res = platform_get_resource(hdmi.pdev, IORESOURCE_MEM, 0);
	if (!res) {
		DSSERR("can't get IORESOURCE_MEM HDMI\n");
		return -EINVAL;
	}

	/*
	 * Pass DMA audio port to audio drivers.
	 * Audio drivers should not ioremap it.
	 */
	hdmi.ip_data.ops->audio_get_dma_port(&port_offset, &port_size);

	aud_res[0].start = res->start + port_offset;
	aud_res[0].end =  aud_res[0].start + port_size - 1;

	res = platform_get_resource(hdmi.pdev, IORESOURCE_DMA, 0);
	if (!res) {
		DSSERR("can't get IORESOURCE_DMA HDMI\n");
		return -EINVAL;
	}

	/* Pass the audio DMA request resource to audio drivers. */
	aud_res[1].start = res->start;

	/* create platform device for HDMI audio driver */
	aud_pdev = platform_device_register_simple("omap-hdmi-audio",
						   pdev->id, aud_res,
						   ARRAY_SIZE(aud_res));
	if (IS_ERR(aud_pdev)) {
		DSSERR("Can't instantiate hdmi-audio\n");
		return -ENODEV;
	}

	hdmi.audio_pdev = aud_pdev;

	return 0;
}

int hdmi_compute_acr(u32 sample_freq, u32 *n, u32 *cts)
{
	int r;
	u32 deep_color;
	u32 pclk = hdmi.ip_data.cfg.timings.pixel_clock;

	if (n == NULL || cts == NULL || sample_freq == 0)
		return -EINVAL;

	/* TODO: When implemented, query deep color mode here. */
	deep_color = 100;

	switch (sample_freq) {
	case 32000:
		if (deep_color == 125 && pclk == 74250) {
			*n = 8192;
			break;
		}

		if (deep_color == 125 && pclk == 27027) {
			/*
			 * For this specific configuration, no value within the
			 * allowed interval of N (as per the HDMI spec) will
			 * produce an integer value of CTS. The value we use
			 * here will produce CTS = 11587.000427246, which is
			 * slightly larger than the integer. This difference
			 * could cause the audio clock at the sink to slowly
			 * drift. The true solution requires alternating between
			 * two CTS relevant values with careful timing in order
			 * to, on average, obtain the true CTS float value.
			*/
			*n = 13529;
			break;
		}

		if (deep_color == 150 && pclk == 27027) {
			*n = 8192;
			break;
		}

		*n = 4096;
		break;
	case 44100:
		if (deep_color == 125 && pclk == 27027) {
			*n = 12544;
			break;
		}

		*n = 6272;
		break;
	case 48000:
		if (deep_color == 125 && (pclk == 27027 || pclk == 74250)) {
			*n = 8192;
			break;
		}

		if (deep_color == 150 && pclk == 27027) {
			*n = 8192;
			break;
		}

		*n = 6144;
		break;
	case 88200:
		r = hdmi_compute_acr(44100, n, cts);
		*n *= 2;
		return r;
	case 96000:
		r = hdmi_compute_acr(48000, n, cts);
		*n *= 2;
		return r;
	case 176400:
		r = hdmi_compute_acr(44100, n, cts);
		*n *= 4;
		return r;
	case 192000:
		r = hdmi_compute_acr(48000, n, cts);
		*n *= 4;
		return r;
	default:
		return -EINVAL;
	}

	/*
	 * Calculate CTS. See HDMI 1.3a or 1.4a specifications. Preserve the
	 * remainder in case N is not a multiple of 128.
	 */
	*cts = (*n / 128) * pclk * deep_color;
	*cts += (*n % 128) * pclk * deep_color / 128;
	*cts /= (sample_freq / 10);

	if ((pclk * (*n / 128) * deep_color) % (sample_freq / 10))
		DSSWARN("CTS is not integer fs[%u]pclk[%u]N[%u]\n",
			sample_freq, pclk, *n);

	return 0;
}

int hdmi_audio_enable(void)
{
	DSSDBG("audio_enable\n");

	return hdmi.ip_data.ops->audio_enable(&hdmi.ip_data);
}

void hdmi_audio_disable(void)
{
	DSSDBG("audio_disable\n");

	hdmi.ip_data.ops->audio_disable(&hdmi.ip_data);
}

int hdmi_audio_start(void)
{
	DSSDBG("audio_start\n");

	return hdmi.ip_data.ops->audio_start(&hdmi.ip_data);
}

void hdmi_audio_stop(void)
{
	DSSDBG("audio_stop\n");

	hdmi.ip_data.ops->audio_stop(&hdmi.ip_data);
}

bool hdmi_mode_has_audio(void)
{
	if (hdmi.ip_data.cfg.cm.mode == HDMI_HDMI)
		return true;
	else
		return false;
}

int hdmi_audio_config(struct omap_dss_audio *audio)
{
	return hdmi.ip_data.ops->audio_config(&hdmi.ip_data, audio);
}

#endif

static struct omap_dss_device * __init hdmi_find_dssdev(struct platform_device *pdev)
{
	struct omap_dss_board_info *pdata = pdev->dev.platform_data;
	const char *def_disp_name = omapdss_get_default_display_name();
	struct omap_dss_device *def_dssdev;
	int i;

	def_dssdev = NULL;

	for (i = 0; i < pdata->num_devices; ++i) {
		struct omap_dss_device *dssdev = pdata->devices[i];

		if (dssdev->type != OMAP_DISPLAY_TYPE_HDMI)
			continue;

		if (def_dssdev == NULL)
			def_dssdev = dssdev;

		if (def_disp_name != NULL &&
				strcmp(dssdev->name, def_disp_name) == 0) {
			def_dssdev = dssdev;
			break;
		}
	}

	return def_dssdev;
}

static void __init hdmi_probe_pdata(struct platform_device *pdev)
{
	struct omap_dss_device *plat_dssdev;
	struct omap_dss_device *dssdev;
	struct omap_dss_hdmi_data *priv;
	int r;

	plat_dssdev = hdmi_find_dssdev(pdev);

	if (!plat_dssdev)
		return;

	dssdev = dss_alloc_and_init_device(&pdev->dev);
	if (!dssdev)
		return;

	dss_copy_device_pdata(dssdev, plat_dssdev);

	priv = dssdev->data;

	hdmi.ct_cp_hpd_gpio = priv->ct_cp_hpd_gpio;
	hdmi.ls_oe_gpio = priv->ls_oe_gpio;
	hdmi.hpd_gpio = priv->hpd_gpio;

	dssdev->channel = OMAP_DSS_CHANNEL_DIGIT;

	r = hdmi_init_display(dssdev);
	if (r) {
		DSSERR("device %s init failed: %d\n", dssdev->name, r);
		dss_put_device(dssdev);
		return;
	}

	r = omapdss_output_set_device(&hdmi.output, dssdev);
	if (r) {
		DSSERR("failed to connect output to new device: %s\n",
				dssdev->name);
		dss_put_device(dssdev);
		return;
	}

	r = dss_add_device(dssdev);
	if (r) {
		DSSERR("device %s register failed: %d\n", dssdev->name, r);
		omapdss_output_unset_device(&hdmi.output);
		hdmi_uninit_display(dssdev);
		dss_put_device(dssdev);
		return;
	}
}

struct i2c_adapter *omapdss_hdmi_adapter(void)
{
	return hdmi.adap;
}

static void ddc_set_sda(void *data, int state)
{
	if (state)
		gpio_direction_input(hdmi.sda_pin);
	else
		gpio_direction_output(hdmi.sda_pin, 0);
}

static void ddc_set_scl(void *data, int state)
{
	if (state)
		gpio_direction_input(hdmi.scl_pin);
	else
		gpio_direction_output(hdmi.scl_pin, 0);
}

static int ddc_get_sda(void *data)
{
	return gpio_get_value(hdmi.sda_pin);
}

static int ddc_get_scl(void *data)
{
	return gpio_get_value(hdmi.scl_pin);
}

static int ddc_pre_xfer(struct i2c_adapter *adap)
{
	/* don't read if no hdmi connected */
	if (!gpio_get_value(hdmi.hpd_gpio))
		return -ENODEV;

	gpio_set_value_cansleep(hdmi.ls_oe_gpio, 1);

	return 0;
}
static void ddc_post_xfer(struct i2c_adapter *adap)
{
	hdmi_set_ls_state(hdmi.ls_state);
}

static void ddc_i2c_init(struct platform_device *pdev)
{

	hdmi.adap = kzalloc(sizeof(*hdmi.adap), GFP_KERNEL);

	if (!hdmi.adap) {
		pr_err("Failed to allocate i2c adapter\n");
		return;
	}

	hdmi.adap->owner = THIS_MODULE;
	hdmi.adap->class = I2C_CLASS_DDC;
	hdmi.adap->dev.parent = &pdev->dev;
	hdmi.adap->algo_data = &hdmi.bit_data;
	hdmi.adap->algo = &i2c_bit_algo;
	hdmi.bit_data.udelay = 2;
	hdmi.bit_data.timeout = HZ/10;
	hdmi.bit_data.setsda = ddc_set_sda;
	hdmi.bit_data.setscl = ddc_set_scl;
	hdmi.bit_data.getsda = ddc_get_sda;
	hdmi.bit_data.getscl = ddc_get_scl;
	hdmi.bit_data.pre_xfer = ddc_pre_xfer;
	hdmi.bit_data.post_xfer = ddc_post_xfer;

	gpio_request(hdmi.sda_pin, "DDC SDA");
	gpio_request(hdmi.scl_pin, "DDC SCL");
	snprintf(hdmi.adap->name, sizeof(hdmi.adap->name),
		"DSS DDC-EDID adapter");
	if (i2c_add_adapter(hdmi.adap)) {
		DSSERR("Cannot initialize DDC I2c\n");
		kfree(hdmi.adap);
		hdmi.adap = NULL;
	}
}

static void init_sel_i2c_hdmi(void)
{
	void __iomem *clk_base = ioremap(0x4A009000, SZ_4K);
	void __iomem *mcasp2_base = ioremap(0x48464000, SZ_1K);
	void __iomem *pinmux = ioremap(0x4a003600, SZ_1K);
	u32 val;
	
	if (omapdss_get_version() != OMAPDSS_VER_DRA7xx)
		goto err;

	if (!clk_base || !mcasp2_base || !pinmux)
		DSSERR("couldn't ioremap for clk or mcasp2\n");

	__raw_writel(0x40000, pinmux + 0xfc);
	/* sw supervised wkup */
	__raw_writel(0x2, clk_base + 0x8fc);

	/* enable clock domain */
	__raw_writel(0x2, clk_base + 0x860);

	/* see what status looks like */
	val = __raw_readl(clk_base + 0x8fc);
	printk("CM_L4PER2_CLKSTCTRL %x\n", val);

	/*
	 * mcasp2 regs should be hopefully accessible, make mcasp2_aclkr
	 * a gpio, write necessary stuff to MCASP_PFUNC and PDIR
	 */
	__raw_writel(0x1 << 29, mcasp2_base + 0x10);
	__raw_writel(0x1 << 29, mcasp2_base + 0x14);

err:
	iounmap(clk_base);
	iounmap(mcasp2_base);
	iounmap(pinmux);
}

/* use this to configure the pcf8575@22 to set LS_OE and CT_HPD */
void sel_i2c(void)
{
	void __iomem *base = ioremap(0x48464000, SZ_1K);

	if (omapdss_get_version() != OMAPDSS_VER_DRA7xx)
		goto err;

	/* PDOUT */
	__raw_writel(0x0, base + 0x18);

	DSSDBG("PDOUT sel_i2c  %x\n", __raw_readl(base + 0x18));

err:
	iounmap(base);
}

/* use this to read edid and detect hpd ? */
void sel_hdmi(void)
{
	void __iomem *base = ioremap(0x48464000, SZ_1K);

	if (omapdss_get_version() != OMAPDSS_VER_DRA7xx)
		goto err;

	/* PDOUT */
	__raw_writel(0x20000000, base + 0x18);

	DSSDBG("PDOUT sel_hdmi %x\n", __raw_readl(base + 0x18));

err:
	iounmap(base);
}

static void __init hdmi_probe_of(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct device_node *child;
	struct omap_dss_device *dssdev;
	struct device_node *adapter_node;
	struct i2c_adapter *adapter = NULL;
	int r, gpio;
	enum omap_channel channel;
	u32 v, volt;
	int gpio_count;

	r = of_property_read_u32(node, "video-source", &v);
	if (r) {
		DSSERR("parsing channel failed\n");
		return;
	}

	channel = v;

	r = of_property_read_u32(node, "vdda_hdmi_microvolt_min", &volt);
	if (r) {
		DSSERR("parsing microvolt_min failed\n");
		return;
	}
	hdmi.microvolt_min = volt;

	r = of_property_read_u32(node, "vdda_hdmi_microvolt_max", &volt);
	if (r) {
		DSSERR("parsing microvolt_max failed\n");
		return;
	}
	hdmi.microvolt_max = volt;

	node = of_find_compatible_node(node, NULL, "ti,tpd12s015");
	if (!node)
		return;

	child = of_get_next_available_child(node, NULL);
	if (!child)
		return;

	gpio_count = of_gpio_count(node);

	/* OMAP4 derivatives have 3 pins defined, OMAP5 derivatives have 5 */
	if (gpio_count != 5 && gpio_count != 3) {
		DSSERR("wrong number of GPIOs\n");
		return;
	}

	gpio = of_get_gpio(node, 0);
	if (gpio_is_valid(gpio)) {
		hdmi.ct_cp_hpd_gpio = gpio;
	} else {
		DSSERR("failed to parse CT CP HPD gpio\n");
		return;
	}

	gpio = of_get_gpio(node, 1);
	if (gpio_is_valid(gpio)) {
		hdmi.ls_oe_gpio = gpio;
	} else {
		DSSERR("failed to parse LS OE gpio\n");
		return;
	}

	gpio = of_get_gpio(node, 2);
	if (gpio_is_valid(gpio)) {
		hdmi.hpd_gpio = gpio;
	} else {
		DSSERR("failed to parse HPD gpio\n");
		return;
	}

	adapter_node = of_parse_phandle(node, "hdmi_ddc", 0);
	if (adapter_node)
		adapter = of_find_i2c_adapter_by_node(adapter_node);

	/*
	 * if I2C SCL and SDA pins are defined, parse them, if an adapter is
	 * present, use the i2c adapter rather than bitbanging i2c. If there
	 * isn't an adapter either, assume that we are using the hdmi core IP's
	 * ddc.
	 */
	if (gpio_count == 5) {
		gpio = of_get_gpio(node, 3);
		if (gpio_is_valid(gpio)) {
			hdmi.scl_pin = gpio;
		} else {
			DSSERR("failed to parse SCL gpio\n");
			return;
		}

		gpio = of_get_gpio(node, 4);
		if (gpio_is_valid(gpio)) {
			hdmi.sda_pin = gpio;
		} else {
			DSSERR("failed to parse SDA gpio\n");
			return;
		}
	} else if (adapter != NULL) {
		hdmi.adap = adapter;

		/*
		 * we have SEL_I2C_HDMI pin which acts as a control line to
		 * a demux which choses the i2c lines to go either to hdmi
		 * or to the other i2c2 slaves. This line is used as a mcasp2
		 * gpio. Init the gpio pin so that it can be used to control
		 * the demux.
		 */
		init_sel_i2c_hdmi();
		sel_i2c();
	}

	dssdev = dss_alloc_and_init_device(&pdev->dev);
	if (!dssdev)
		return;

	dssdev->dev.of_node = child;
	dssdev->type = OMAP_DISPLAY_TYPE_HDMI;
	dssdev->name = child->name;
	dssdev->channel = channel;

	r = hdmi_init_display(dssdev);
	if (r) {
		DSSERR("device %s init failed: %d\n", dssdev->name, r);
		dss_put_device(dssdev);
		return;
	}

	r = omapdss_output_set_device(&hdmi.output, dssdev);
	if (r) {
		DSSERR("failed to connect output to new device: %s\n",
				dssdev->name);
		dss_put_device(dssdev);
		return;
	}

	r = dss_add_device(dssdev);
	if (r) {
		DSSERR("dss_add_device failed %d\n", r);
		dss_put_device(dssdev);
		return;
	}
}

static void __init hdmi_init_output(struct platform_device *pdev)
{
	struct omap_dss_output *out = &hdmi.output;

	out->pdev = pdev;
	out->id = OMAP_DSS_OUTPUT_HDMI;
	out->type = OMAP_DISPLAY_TYPE_HDMI;

	dss_register_output(out);
}

static void __exit hdmi_uninit_output(struct platform_device *pdev)
{
	struct omap_dss_output *out = &hdmi.output;

	dss_unregister_output(out);
}

/* HDMI HW IP initialisation */
static int __init omapdss_hdmihw_probe(struct platform_device *pdev)
{
	struct resource *res;
	int r;

	hdmi.pdev = pdev;

	mutex_init(&hdmi.lock);
	mutex_init(&hdmi.ip_data.lock);

	dss_init_hdmi_ip_ops(&hdmi.ip_data, omapdss_get_version());

	/* HDMI wrapper memory remap */
	res = platform_get_resource_byname(hdmi.pdev,
					   IORESOURCE_MEM, "hdmi_wp");
	if (!res) {
		DSSERR("can't get WP IORESOURCE_MEM HDMI\n");
		return -EINVAL;
	}

	/* Base address taken from platform */
	hdmi.ip_data.base_wp = devm_request_and_ioremap(&pdev->dev, res);
	if (!hdmi.ip_data.base_wp) {
		DSSERR("can't ioremap WP\n");
		return -ENOMEM;
	}

	/* HDMI PLLCTRL memory remap */
	res = platform_get_resource_byname(hdmi.pdev,
					   IORESOURCE_MEM, "pllctrl");
	if (!res) {
		DSSERR("can't get PLL CTRL IORESOURCE_MEM HDMI\n");
		return -EINVAL;
	}

	hdmi.ip_data.base_pllctrl = devm_request_and_ioremap(&pdev->dev, res);
	if (!hdmi.ip_data.base_pllctrl) {
		DSSERR("can't ioremap PLL ctrl\n");
		return -ENOMEM;
	}

	/* HDMI TXPHYCTRL memory remap */
	res = platform_get_resource_byname(hdmi.pdev,
					   IORESOURCE_MEM, "hdmitxphy");
	if (!res) {
		DSSERR("can't get TXPHY CTRL IORESOURCE_MEM HDMI\n");
		return -EINVAL;
	}

	hdmi.ip_data.base_txphyctrl = devm_request_and_ioremap(&pdev->dev, res);
	if (!hdmi.ip_data.base_txphyctrl) {
		DSSERR("can't ioremap TXPHY ctrl\n");
		return -ENOMEM;
	}

	/* HDMI core memory remap */
	res = platform_get_resource_byname(hdmi.pdev,
					   IORESOURCE_MEM, "hdmi_core");
	if (!res) {
		DSSERR("can't get core IORESOURCE_MEM HDMI\n");
		return -EINVAL;
	}

	hdmi.ip_data.base_core = devm_request_and_ioremap(&pdev->dev, res);
	if (!hdmi.ip_data.base_core) {
		DSSERR("can't ioremap core\n");
		return -ENOMEM;
	}

	r = hdmi_get_clocks(pdev);
	if (r) {
		DSSERR("can't get clocks\n");
		return r;
	}

	pm_runtime_enable(&pdev->dev);

	hdmi.hdmi_irq = platform_get_irq(pdev, 0);
	r = request_irq(hdmi.hdmi_irq, hdmi_irq_handler, 0, "OMAP HDMI", NULL);
	if (r < 0) {
		pr_err("hdmi: request_irq %s failed\n", pdev->name);
		return -EINVAL;
	}

	r = hdmi_panel_init();
	if (r) {
		DSSERR("can't init panel\n");
		goto err_panel_init;
	}

	dss_debugfs_create_file("hdmi", hdmi_dump_regs);

	hdmi_init_output(pdev);

	if (pdev->dev.of_node)
		hdmi_probe_of(pdev);
	else if (pdev->dev.platform_data)
		hdmi_probe_pdata(pdev);

	/* if i2c pins defined, setup I2C adapter */
	if (hdmi.scl_pin && hdmi.sda_pin)
		ddc_i2c_init(pdev);

#if defined(CONFIG_OMAP4_DSS_HDMI_AUDIO) || \
	defined(CONFIG_OMAP5_DSS_HDMI_AUDIO)
	r = hdmi_probe_audio(pdev);
	if (r)
		DSSWARN("could not create platform device for audio");
#endif

	return 0;

err_panel_init:
	hdmi_put_clocks();
	return r;
}

static int __exit hdmi_remove_child(struct device *dev, void *data)
{
	struct omap_dss_device *dssdev = to_dss_device(dev);
	hdmi_uninit_display(dssdev);
	return 0;
}

static int __exit omapdss_hdmihw_remove(struct platform_device *pdev)
{
#if defined(CONFIG_OMAP4_DSS_HDMI_AUDIO) || \
	defined(CONFIG_OMAP5_DSS_HDMI_AUDIO)
	if (hdmi.audio_pdev != NULL)
		platform_device_unregister(hdmi.audio_pdev);
#endif

	kfree(hdmi.adap);

	device_for_each_child(&pdev->dev, NULL, hdmi_remove_child);

	dss_unregister_child_devices(&pdev->dev);

	hdmi_panel_exit();

	hdmi_uninit_output(pdev);

	pm_runtime_disable(&pdev->dev);

	hdmi_put_clocks();

	return 0;
}

static int hdmi_runtime_suspend(struct device *dev)
{
	clk_disable_unprepare(hdmi.sys_clk);

	dispc_runtime_put();

	return 0;
}

static int hdmi_runtime_resume(struct device *dev)
{
	int r;

	r = dispc_runtime_get();
	if (r < 0)
		return r;

	clk_prepare_enable(hdmi.sys_clk);

	return 0;
}

static const struct dev_pm_ops hdmi_pm_ops = {
	.runtime_suspend = hdmi_runtime_suspend,
	.runtime_resume = hdmi_runtime_resume,
};

#if defined(CONFIG_OF)
static const struct of_device_id hdmi_of_match[] = {
	{
		.compatible = "ti,omap4-hdmi",
	},
	{},
};
#else
#define hdmi_of_match NULL
#endif

static struct platform_driver omapdss_hdmihw_driver = {
	.remove         = __exit_p(omapdss_hdmihw_remove),
	.driver         = {
		.name   = "omapdss_hdmi",
		.owner  = THIS_MODULE,
		.pm	= &hdmi_pm_ops,
		.of_match_table = hdmi_of_match,
	},
};

int __init hdmi_init_platform_driver(void)
{
	return platform_driver_probe(&omapdss_hdmihw_driver, omapdss_hdmihw_probe);
}

void __exit hdmi_uninit_platform_driver(void)
{
	platform_driver_unregister(&omapdss_hdmihw_driver);
}
