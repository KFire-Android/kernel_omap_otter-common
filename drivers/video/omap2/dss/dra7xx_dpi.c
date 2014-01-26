/*
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


#define DSS_SUBSYS_NAME "DRA7XX_DPI"

#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/export.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/of.h>

#include <video/omapdss.h>

#include "dss.h"
#include "dss_features.h"

struct dpi_data {
	enum dss_dpll dpll;

	struct mutex lock;

	u32 module_id;

	struct omap_video_timings timings;
	struct dss_lcd_mgr_config mgr_config;
	int data_lines;

	struct omap_dss_output output;
};

/*
 * On vayu, we will try to use the DPLL_VIDEOx PLLs, only if we can't get one,
 * we will try to modify the DSS_FCLK to get the pixel clock. Leave HDMI PLL out
 * for now
 */

enum dss_dpll dpi_get_dpll(struct platform_device *pdev)
{
	struct dpi_data *dpi = dev_get_drvdata(&pdev->dev);

	switch (dpi->module_id) {
	case 0:
#ifdef CONFIG_DISPLAY_SKIP_INIT
		if (omapdss_skipinit())
#else
		if (dss_dpll_disabled(DSS_DPLL_VIDEO1))
#endif
			return DSS_DPLL_VIDEO1;
		else
			return DSS_DPLL_NONE;
	case 1:
	case 2:
		if (dss_dpll_disabled(DSS_DPLL_VIDEO1))
			return DSS_DPLL_VIDEO1;
		else if (dss_dpll_disabled(DSS_DPLL_VIDEO2))
			return DSS_DPLL_VIDEO2;
		else
			return DSS_DPLL_NONE;
	default:
		return DSS_DPLL_NONE;
	}

	return DSS_DPLL_NONE;
}

static int dpi_set_dss_dpll_clk(struct dpi_data *dpi, enum dss_dpll dpll,
		unsigned long pck_req, unsigned long *fck, u16 *lck_div,
		u16 *pck_div)
{
	struct dss_dpll_cinfo dpll_cinfo;
	struct dispc_clock_info dispc_cinfo;
	int r;

	r = dss_dpll_calc_clock_div_pck(dpll, pck_req, &dpll_cinfo, &dispc_cinfo);
	if (r)
		return r;

	r = dss_dpll_set_clock_div(dpll, &dpll_cinfo);
	if (r)
		return r;

	dpi->mgr_config.clock_info = dispc_cinfo;

	*fck = dpll_cinfo.hsdiv_clk;
	*lck_div = dispc_cinfo.lck_div;
	*pck_div = dispc_cinfo.pck_div;

	return 0;
}

static int dpi_set_dispc_clk(struct dpi_data *dpi, unsigned long pck_req,
		unsigned long *fck, u16 *lck_div, u16 *pck_div)
{
	struct dss_clock_info dss_cinfo;
	struct dispc_clock_info dispc_cinfo;
	int r;

	r = dss_calc_clock_div(pck_req, &dss_cinfo, &dispc_cinfo);
	if (r)
		return r;

	r = dss_set_clock_div(&dss_cinfo);
	if (r)
		return r;

	dpi->mgr_config.clock_info = dispc_cinfo;

	*fck = dss_cinfo.fck;
	*lck_div = dispc_cinfo.lck_div;
	*pck_div = dispc_cinfo.pck_div;

	return 0;
}

static int dpi_set_mode(struct platform_device *pdev, enum dss_dpll dpll)
{
	struct dpi_data *dpi = dev_get_drvdata(&pdev->dev);
	struct omap_video_timings *t = &dpi->timings;
	struct omap_dss_output *out = &dpi->output;
	struct omap_overlay_manager *mgr = out->manager;
	unsigned long fck = 0;
	unsigned long pck;
	u16 lcd, pcd;
	int r = 0;

	if (dpll != DSS_DPLL_NONE)
		r = dpi_set_dss_dpll_clk(dpi, dpll, t->pixel_clock * 1000,
			&fck, &lcd, &pcd);
	else
		r = dpi_set_dispc_clk(dpi, t->pixel_clock * 1000, &fck,
			&lcd, &pcd);
	if (r)
		return r;

	pck = fck / lcd / pcd / 1000;

	if (pck != t->pixel_clock) {
		DSSWARN("Could not find exact pixel clock. "
				"Requested %d kHz, got %lu kHz\n",
				t->pixel_clock, pck);

		t->pixel_clock = pck;
	}

	dss_mgr_set_timings(mgr, t);

	return 0;
}

static void dpi_config_lcd_manager(struct platform_device *pdev)
{
	struct dpi_data *dpi = dev_get_drvdata(&pdev->dev);
	struct omap_dss_output *out = &dpi->output;

	dpi->mgr_config.io_pad_mode = DSS_IO_PAD_MODE_BYPASS;

	dpi->mgr_config.stallmode = false;
	dpi->mgr_config.fifohandcheck = false;

	dpi->mgr_config.video_port_width = dpi->data_lines;

	dpi->mgr_config.lcden_sig_polarity = 0;

	dss_mgr_set_lcd_config(out->manager, &dpi->mgr_config);
}

static int dra7xx_dpi_display_enable(struct omap_dss_device *dssdev)
{
	struct omap_dss_output *out = dssdev->output;
	struct platform_device *pdev = out->pdev;
	struct dpi_data *dpi = dev_get_drvdata(&pdev->dev);
	enum omap_channel channel = dssdev->channel;
	int r;

	mutex_lock(&dpi->lock);

	if (out == NULL || out->manager == NULL) {
		DSSERR("failed to enable display: no output/manager\n");
		r = -ENODEV;
		goto err_no_out_mgr;
	}

	r = omap_dss_start_device(dssdev);
	if (r) {
		DSSERR("failed to start device\n");
		goto err_start_dev;
	}

	r = dispc_runtime_get();
	if (r)
		goto err_get_dispc;

	r = dss_dpi_select_source(dpi->module_id, channel);
	if (r)
		goto err_src_sel;

	/* try to get a free dpll, if not, try to change DSS_FCLK */
	dpi->dpll = dpi_get_dpll(pdev);
	if (dpi->dpll != DSS_DPLL_NONE) {
		DSSDBG("using DPLL %d for DPI%d\n", dpi->dpll,
		       dpi->module_id);

		dss_dpll_activate(dpi->dpll);
	}

	r = dpi_set_mode(pdev, dpi->dpll);
	if (r)
		goto err_set_mode;

	dss_use_dpll_lcd(channel, dpi->dpll != DSS_DPLL_NONE);

	dss_dpll_set_control_mux(channel, dpi->dpll);

	dpi_config_lcd_manager(pdev);

	mdelay(2);

	r = dss_mgr_enable(out->manager);
	if (r)
		goto err_mgr_enable;

	mutex_unlock(&dpi->lock);

	return 0;

err_mgr_enable:
err_set_mode:
	if (dpi->dpll != DSS_DPLL_NONE)
		dss_dpll_disable(dpi->dpll);
err_src_sel:
	dispc_runtime_put();
err_get_dispc:
err_start_dev:
err_no_out_mgr:
	mutex_unlock(&dpi->lock);
	return r;
}

static void dra7xx_dpi_display_disable(struct omap_dss_device *dssdev)
{
	struct platform_device *pdev = dssdev->output->pdev;
	struct dpi_data *dpi = dev_get_drvdata(&pdev->dev);
	struct omap_overlay_manager *mgr = dssdev->output->manager;

	mutex_lock(&dpi->lock);

	dss_mgr_disable(mgr);

	if (dpi->dpll != DSS_DPLL_NONE) {
		dss_use_dpll_lcd(dssdev->channel, false);
		dss_dpll_disable(dpi->dpll);
		dpi->dpll = DSS_DPLL_NONE;
	}

	dispc_runtime_put();

	omap_dss_stop_device(dssdev);

	mutex_unlock(&dpi->lock);
}

static void dra7xx_dpi_set_timings(struct omap_dss_device *dssdev,
		struct omap_video_timings *timings)
{
	struct platform_device *pdev = dssdev->output->pdev;
	struct dpi_data *dpi = dev_get_drvdata(&pdev->dev);

	DSSDBG("set_timings\n");

	mutex_lock(&dpi->lock);

	dpi->timings = *timings;

	mutex_unlock(&dpi->lock);
}

static int dra7xx_dpi_check_timings(struct omap_dss_device *dssdev,
			struct omap_video_timings *timings)
{
	DSSDBG("check_timings\n");

	return 0;
}

static void dra7xx_dpi_set_data_lines(struct omap_dss_device *dssdev, int data_lines)
{
	struct platform_device *pdev = dssdev->output->pdev;
	struct dpi_data *dpi = dev_get_drvdata(&pdev->dev);

	mutex_lock(&dpi->lock);

	dpi->data_lines = data_lines;

	mutex_unlock(&dpi->lock);
}

static int __init dpi_init_display(struct omap_dss_device *dssdev)
{
	DSSDBG("init_display\n");

	return 0;
}

static struct omap_dss_device * __init dpi_find_dssdev(struct platform_device *pdev)
{
	struct omap_dss_board_info *pdata = pdev->dev.platform_data;
	const char *def_disp_name = omapdss_get_default_display_name();
	struct omap_dss_device *def_dssdev;
	int i;

	def_dssdev = NULL;

	for (i = 0; i < pdata->num_devices; ++i) {
		struct omap_dss_device *dssdev = pdata->devices[i];

		if (dssdev->type != OMAP_DISPLAY_TYPE_DPI)
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

static void __init dra7xx_dpi_probe_pdata(struct platform_device *pdev)
{
	struct dpi_data *dpi = dev_get_drvdata(&pdev->dev);
	struct omap_dss_device *plat_dssdev;
	struct omap_dss_device *dssdev;
	int r;

	plat_dssdev = dpi_find_dssdev(pdev);

	if (!plat_dssdev)
		return;

	dssdev = dss_alloc_and_init_device(&pdev->dev);
	if (!dssdev)
		return;

	dss_copy_device_pdata(dssdev, plat_dssdev);

	r = dpi_init_display(dssdev);
	if (r) {
		DSSERR("device %s init failed: %d\n", dssdev->name, r);
		dss_put_device(dssdev);
		return;
	}

	r = omapdss_output_set_device(&dpi->output, dssdev);
	if (r) {
		DSSERR("failed to connect output to new device: %s\n",
				dssdev->name);
		dss_put_device(dssdev);
		return;
	}

	r = dss_add_device(dssdev);
	if (r) {
		DSSERR("device %s register failed: %d\n", dssdev->name, r);
		omapdss_output_unset_device(&dpi->output);
		dss_put_device(dssdev);
		return;
	}
}

static void __init dra7xx_dpi_probe_of(struct platform_device *pdev)
{
	struct dpi_data *dpi = dev_get_drvdata(&pdev->dev);
	struct device_node *node = pdev->dev.of_node;
	struct device_node *child;
	struct omap_dss_device *dssdev;
	enum omap_channel channel;
	u32 v;
	int r;

	child = of_get_next_available_child(node, NULL);

	if (!child)
		return;

	r = of_property_read_u32(node, "video-source", &v);
	if (r) {
		DSSERR("parsing channel failed\n");
		return;
	}

	channel = v;

	dssdev = dss_alloc_and_init_device(&pdev->dev);
	if (!dssdev)
		return;

	dssdev->dev.of_node = child;
	dssdev->type = OMAP_DISPLAY_TYPE_DPI;
	dssdev->name = child->name;
	dssdev->channel = channel;

	r = dpi_init_display(dssdev);
	if (r) {
		DSSERR("device %s init failed: %d\n", dssdev->name, r);
		dss_put_device(dssdev);
		return;
	}

	r = omapdss_output_set_device(&dpi->output, dssdev);
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

static void __init dra7xx_dpi_init_output(struct platform_device *pdev)
{
	struct dpi_data *dpi = dev_get_drvdata(&pdev->dev);
	struct omap_dss_output *out = &dpi->output;

	out->pdev = pdev;

	switch (dpi->module_id) {
	case 0:
		out->id = OMAP_DSS_OUTPUT_DPI;
		break;
	case 1:
		out->id = OMAP_DSS_OUTPUT_DPI1;
		break;
	case 2:
		out->id = OMAP_DSS_OUTPUT_DPI2;
		break;
	};

	out->type = OMAP_DISPLAY_TYPE_DPI;

	dss_register_output(out);
}

static void __exit dra7xx_dpi_uninit_output(struct platform_device *pdev)
{
	struct dpi_data *dpi = dev_get_drvdata(&pdev->dev);
	struct omap_dss_output *out = &dpi->output;

	dss_unregister_output(out);
}

static struct dpi_common_ops ops = {
	.enable = dra7xx_dpi_display_enable,
	.disable = dra7xx_dpi_display_disable,
	.set_timings = dra7xx_dpi_set_timings,
	.check_timings = dra7xx_dpi_check_timings,
	.set_data_lines = dra7xx_dpi_set_data_lines,
};

static int __init dra7xx_dpi_probe(struct platform_device *pdev)
{
	int r;
	struct dpi_data *dpi;

	dpi = devm_kzalloc(&pdev->dev, sizeof(*dpi), GFP_KERNEL);
	if (!dpi)
		return -ENOMEM;

	dpi_common_set_ops(&ops);

	dev_set_drvdata(&pdev->dev, dpi);

	mutex_init(&dpi->lock);

	if (pdev->dev.of_node) {
		u32 id;
		r = of_property_read_u32(pdev->dev.of_node, "reg", &id);
		if (r) {
			DSSERR("failed to read DPI module ID\n");
			return r;
		}

		dpi->module_id = id;
	} else {
		dpi->module_id = pdev->id;
	}

	dra7xx_dpi_init_output(pdev);

	if (pdev->dev.of_node)
		dra7xx_dpi_probe_of(pdev);
	else if (pdev->dev.platform_data)
		dra7xx_dpi_probe_pdata(pdev);

	r = dispc_runtime_get();
	if (r)
		DSSERR("DPI error runtime_get\n");

	dispc_runtime_put();

	return 0;
}

static int __exit dra7xx_dpi_remove(struct platform_device *pdev)
{
	dss_unregister_child_devices(&pdev->dev);

	dra7xx_dpi_uninit_output(pdev);

	return 0;
}

#if defined(CONFIG_OF)
static const struct of_device_id dpi_of_match[] = {
	{
		.compatible = "ti,dra7xx-dpi",
	},
	{},
};
#else
#define dpi_of_match NULL
#endif

static struct platform_driver dra7xx_dpi_driver = {
	.remove         = __exit_p(dra7xx_dpi_remove),
	.driver         = {
		.name   = "omapdss_dra7xx_dpi",
		.owner  = THIS_MODULE,
		.of_match_table = dpi_of_match,
	},
};

int __init dra7xx_dpi_init_platform_driver(void)
{
	return platform_driver_probe(&dra7xx_dpi_driver, dra7xx_dpi_probe);
}

void __exit dra7xx_dpi_uninit_platform_driver(void)
{
	platform_driver_unregister(&dra7xx_dpi_driver);
}
