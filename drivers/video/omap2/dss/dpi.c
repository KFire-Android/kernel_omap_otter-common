/*
 * linux/drivers/video/omap2/dss/dpi.c
 *
 * Copyright (C) 2009 Nokia Corporation
 * Author: Tomi Valkeinen <tomi.valkeinen@nokia.com>
 *
 * Some code and ideas taken from drivers/video/omap/ driver
 * by Imre Deak.
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

#define DSS_SUBSYS_NAME "DPI"

#include <linux/kernel.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>

#include <plat/display.h>
#include <plat/cpu.h>

#include "dss.h"

static struct {
	struct regulator *vdds_dsi_reg;
} dpi;

#ifdef CONFIG_OMAP2_DSS_USE_DSI_PLL
static int dpi_set_dsi_clk(enum omap_channel channel, bool is_tft,
		unsigned long pck_req, unsigned long *pck)
{
	struct dsi_clock_info dsi_cinfo;
	struct dispc_clock_info dispc_cinfo;
	int r;
	enum omap_dsi_index ix;

	DSSDBG("DPI clk source is DSI PLL\n");

	ix = (channel == OMAP_DSS_CHANNEL_LCD) ? DSI1 : DSI2;

	if (!cpu_is_omap44xx()) {
		r = dsi_pll_calc_clock_div_pck(ix, is_tft,
			pck_req, &dsi_cinfo, &dispc_cinfo);
		if (r)
			return r;
	} else {
		dsi_cinfo.regn = 17;
		dsi_cinfo.regm = 150;
		dsi_cinfo.regm_dispc = 4;
		dsi_cinfo.regm_dsi = 4;
		dsi_cinfo.use_dss2_fck = true;
		r = dsi_calc_clock_rates(channel, &dsi_cinfo);
		if (r)
			return r;
		dispc_find_clk_divs(is_tft, pck_req,
			dsi_cinfo.dsi_pll_dispc_fclk, &dispc_cinfo);
	}

	r = dsi_pll_set_clock_div(ix, &dsi_cinfo);
	if (r)
		return r;

	if (cpu_is_omap44xx())
		dss_select_dispc_clk_source(ix, (ix == DSI1) ?
			DSS_SRC_PLL1_CLK1 : DSS_SRC_PLL2_CLK1);
	else
		dss_select_dispc_clk_source(ix, DSS_SRC_DSI1_PLL_FCLK);

	dispc_set_clock_div(channel, &dispc_cinfo);

	*pck = dispc_cinfo.pck;

	return 0;
}
#else /* #ifdef CONFIG_OMAP2_DSS_USE_DSI_PLL */
static int dpi_set_dispc_clk(enum omap_channel channel,
		bool is_tft, unsigned long pck_req, unsigned long *pck)
{
	struct dispc_clock_info dispc_cinfo;
	enum omap_dsi_index ix;

	DSSDBG("DPI clk source is DISPC\n");

	ix = (channel == OMAP_DSS_CHANNEL_LCD) ? DSI1 : DSI2;

	if (cpu_is_omap44xx())
		dispc_find_clk_divs(is_tft, pck_req,
			dss_clk_get_rate(DSS_CLK_FCK1), &dispc_cinfo);
	else {
		struct dss_clock_info dss_cinfo;
		int r;

		r = dss_calc_clock_div(is_tft, pck_req,
				&dss_cinfo, &dispc_cinfo);
		if (r)
			return r;

		r = dss_set_clock_div(&dss_cinfo);
		if (r)
			return r;
	}

	dss_select_dispc_clk_source(ix, DSS_SRC_DSS1_ALWON_FCLK);
	if (cpu_is_omap44xx())
		dss_select_lcd_clk_source(ix, DSS_SRC_DSS1_ALWON_FCLK);

	dispc_set_clock_div(channel, &dispc_cinfo);

	*pck = dispc_cinfo.pck;

	return 0;
}
#endif /* CONFIG_OMAP2_DSS_USE_DSI_PLL */

static int dpi_set_mode(struct omap_dss_device *dssdev)
{
	struct omap_video_timings *t = &dssdev->panel.timings;
	unsigned long pck = 0;
	unsigned long cache_req_pck = 0;
	bool is_tft;
	int r = 0;

	dispc_set_pol_freq(dssdev->channel, dssdev->panel.config,
				dssdev->panel.acbi, dssdev->panel.acb);

	is_tft = (dssdev->panel.config & OMAP_DSS_LCD_TFT) != 0;

#ifdef CONFIG_OMAP2_DSS_USE_DSI_PLL
	r = dpi_set_dsi_clk(dssdev->channel, is_tft,
			t->pixel_clock * 1000, &pck);
#else /* #ifdef CONFIG_OMAP2_DSS_USE_DSI_PLL */
	cache_req_pck = dss_get_cache_req_pck();
	if (cache_req_pck)
		r = dpi_set_dispc_clk(dssdev->channel, is_tft,
				cache_req_pck, &pck);
	else
		r = dpi_set_dispc_clk(dssdev->channel, is_tft,
				t->pixel_clock * 1000, &pck);
#endif /* CONFIG_OMAP2_DSS_USE_DSI_PLL */
	if (r)
		return r;

	pck /= 1000;

	if (pck != t->pixel_clock) {
		DSSWARN("Could not find exact pixel clock. "
				"Requested %d kHz, got %lu kHz\n",
				t->pixel_clock, pck);

		t->pixel_clock = pck;
	}

	dispc_set_lcd_timings(dssdev->channel, t);

	return 0;
}

static void dpi_basic_init(struct omap_dss_device *dssdev)
{
	bool is_tft;

	is_tft = (dssdev->panel.config & OMAP_DSS_LCD_TFT) != 0;

	dispc_set_parallel_interface_mode(dssdev->channel,
				OMAP_DSS_PARALLELMODE_BYPASS);
	dispc_set_lcd_display_type(dssdev->channel,
		is_tft ? OMAP_DSS_LCD_DISPLAY_TFT : OMAP_DSS_LCD_DISPLAY_STN);
	dispc_set_tft_data_lines(dssdev->channel, dssdev->phy.dpi.data_lines);
}

/*This one needs to be to set the ovl info to dirty*/
static void dpi_start_auto_update(struct omap_dss_device *dssdev)
{
	int i;

	for (i = 0; i < omap_dss_get_num_overlays(); ++i) {
		struct omap_overlay *ovl;

		ovl = omap_dss_get_overlay(i);

		if (ovl->manager == dssdev->manager)
			ovl->info_dirty = true;
	}
	dssdev->manager->apply(dssdev->manager);
}

int omapdss_dpi_display_enable(struct omap_dss_device *dssdev)
{
	int r;

	if (cpu_is_omap44xx() && dssdev->channel != OMAP_DSS_CHANNEL_LCD2) {
		/* Only LCD2 channel is connected to DPI on OMAP4 */
		return -EINVAL;
	}

	r = omap_dss_start_device(dssdev);
	if (r) {
		DSSERR("failed to start device\n");
		return r;
	}

	if (cpu_is_omap34xx() && !cpu_is_omap3630()) {
		r = regulator_enable(dpi.vdds_dsi_reg);
		if (r)
			goto err0;
	}

	/* turn on clock(s) */
	dssdev->state = OMAP_DSS_DISPLAY_ACTIVE;
	if (!cpu_is_omap44xx())
		dss_clk_enable(DSS_CLK_ICK | DSS_CLK_FCK1);
	dss_mainclk_state_enable();

	dpi_basic_init(dssdev);

#ifdef CONFIG_OMAP2_DSS_USE_DSI_PLL
	r = dsi_pll_init(dssdev, 0, 1);
	if (r)
		goto err1;
#endif /* CONFIG_OMAP2_DSS_USE_DSI_PLL */

	r = dpi_set_mode(dssdev);
	if (r)
		goto err2;

	mdelay(2);

	if (dssdev->manager) {
		if (cpu_is_omap44xx())
			dpi_start_auto_update(dssdev);

		dssdev->manager->enable(dssdev->manager);
	}

	return 0;

err2:
#ifdef CONFIG_OMAP2_DSS_USE_DSI_PLL
	dsi_pll_uninit(dssdev->channel == OMAP_DSS_CHANNEL_LCD ? DSI1 : DSI2);
err1:
#endif
	dssdev->state = OMAP_DSS_DISPLAY_DISABLED;
	if (!cpu_is_omap44xx())
		dss_clk_disable(DSS_CLK_ICK | DSS_CLK_FCK1);
	dss_mainclk_state_disable(true);
	if (cpu_is_omap34xx() && !cpu_is_omap3630())
		regulator_disable(dpi.vdds_dsi_reg);
err0:
	omap_dss_stop_device(dssdev);
	return r;
}
EXPORT_SYMBOL(omapdss_dpi_display_enable);

void omapdss_dpi_display_disable(struct omap_dss_device *dssdev)
{
	if (dssdev->manager)
		dssdev->manager->disable(dssdev->manager);

#ifdef HWMOD
#ifdef CONFIG_OMAP2_DSS_USE_DSI_PLL
	{
		enum omap_dsi_index ix;

		ix = (dssdev->channel == OMAP_DSS_CHANNEL_LCD) ? DSI1 : DSI2;
		dss_select_dispc_clk_source(ix, DSS_SRC_DSS1_ALWON_FCLK);
		dsi_pll_uninit(ix);
	}
#endif
#endif

	/* cut clock(s) */
	dssdev->state = OMAP_DSS_DISPLAY_DISABLED;
	if (!cpu_is_omap44xx())
		dss_clk_disable(DSS_CLK_ICK | DSS_CLK_FCK1);
	dss_mainclk_state_disable(true);

	if (cpu_is_omap34xx() && !cpu_is_omap3630())
		regulator_disable(dpi.vdds_dsi_reg);

	omap_dss_stop_device(dssdev);
}
EXPORT_SYMBOL(omapdss_dpi_display_disable);

void dpi_set_timings(struct omap_dss_device *dssdev,
			struct omap_video_timings *timings)
{
	dssdev->panel.timings = *timings;

	if (dssdev->state == OMAP_DSS_DISPLAY_ACTIVE) {
		dpi_set_mode(dssdev);
		dispc_go(dssdev->channel);
	}
}
EXPORT_SYMBOL(dpi_set_timings);

int dpi_check_timings(struct omap_dss_device *dssdev,
			struct omap_video_timings *timings)
{
	struct dispc_clock_info dispc_cinfo;
	bool is_tft;

	if (!dispc_lcd_timings_ok(timings))
		return -EINVAL;

	if (timings->pixel_clock == 0)
		return -EINVAL;

	is_tft = (dssdev->panel.config & OMAP_DSS_LCD_TFT) != 0;

#ifdef CONFIG_OMAP2_DSS_USE_DSI_PLL
	{
		struct dsi_clock_info dsi_cinfo;
		int r = 0;

		if (cpu_is_omap44xx()) {
			dsi_cinfo.regn = 17;
			dsi_cinfo.regm = 150;
			dsi_cinfo.regm_dispc = 4;
			dsi_cinfo.regm_dsi = 4;
			dsi_cinfo.use_dss2_fck = true;
			r = dsi_calc_clock_rates(dssdev->channel, &dsi_cinfo);
			if (r)
				return r;
			dispc_find_clk_divs(is_tft, timings->pixel_clock * 1000,
				dsi_cinfo.dsi_pll_dispc_fclk, &dispc_cinfo);
		} else {
			r = dsi_pll_calc_clock_div_pck(dssdev->channel ==
					OMAP_DSS_CHANNEL_LCD ? DSI1 : DSI2,
					is_tft, timings->pixel_clock * 1000,
					&dsi_cinfo, &dispc_cinfo);
			if (r)
				return r;
		}
	}
#else /* #ifdef CONFIG_OMAP2_DSS_USE_DSI_PLL */
	if (cpu_is_omap44xx())
		dispc_find_clk_divs(is_tft, timings->pixel_clock * 1000,
			dss_clk_get_rate(DSS_CLK_FCK1), &dispc_cinfo);
	else {
		struct dss_clock_info dss_cinfo;
		int r = 0;

		r = dss_calc_clock_div(is_tft, timings->pixel_clock * 1000,
				&dss_cinfo, &dispc_cinfo);
		if (r)
			return r;
	}
#endif /* CONFIG_OMAP2_DSS_USE_DSI_PLL */

	timings->pixel_clock = dispc_cinfo.pck / 1000;

	return 0;
}
EXPORT_SYMBOL(dpi_check_timings);

int dpi_init_display(struct omap_dss_device *dssdev)
{
	DSSDBG("init_display\n");

	return 0;
}

int dpi_init(struct platform_device *pdev)
{
	if (cpu_is_omap34xx() && !cpu_is_omap3630()) {
		dpi.vdds_dsi_reg = dss_get_vdds_dsi();
		if (IS_ERR(dpi.vdds_dsi_reg)) {
			DSSERR("can't get VDDS_DSI regulator\n");
			return PTR_ERR(dpi.vdds_dsi_reg);
		}
	}

	return 0;
}

void dpi_exit(void)
{
}
