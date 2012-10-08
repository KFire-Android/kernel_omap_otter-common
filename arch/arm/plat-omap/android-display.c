/*
 * Android display memory setup for OMAP4+ displays
 *
 * Copyright (C) 2012 Texas Instruments Incorporated - http://www.ti.com/
 *
 * Author: Lajos Molnar
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/platform_device.h>

#include <video/omapdss.h>

#include <plat/android-display.h>
#include <plat/dsscomp.h>
#include <plat/vram.h>

struct omap_android_display_data {
	/* members with default values */
	u32 width;
	u32 height;
	u32 bpp;		/* must be 2 or 4 */

	/* members with no default value */
	u32 tiler1d_mem;
};

/*
 * We need to peek at omapdss settings so that we have enough memory for swap
 * chain and vram.  While this could be done by omapdss, omapdss could be
 * compiled as a module, which is too late to get this information.
 */
static char default_display[16];
static int __init get_default_display(char *str)
{
	strncpy(default_display, str, sizeof(default_display));
	if (strlen(str) >= sizeof(default_display))
		pr_warn("android_display: cannot set default display larger "
			"than %d characters", sizeof(default_display) - 1);
	default_display[sizeof(default_display) - 1] = '\0';
	return 0;
}
early_param("omapdss.def_disp", get_default_display);

bool omap_android_display_is_default(struct omap_dss_device *device)
{
	if (!strcmp(default_display, device->name))
		return true;
	else
		return false;
}

static unsigned int hdmi_width, hdmi_height;
static int __init get_hdmi_options(char *str)
{
	unsigned int width, height;
	char dummy;
	if (sscanf(str, "%ux%u%c", &width, &height, &dummy) == 2) {
		hdmi_width = width;
		hdmi_height = height;
	}
	return 0;
}
early_param("omapdss.hdmi_options", get_hdmi_options);

static void get_display_size(struct omap_dss_device *device,
				struct omap_android_display_data *mem)
{
	if (!device)
		goto done;
	if (device->type == OMAP_DISPLAY_TYPE_HDMI &&
	    hdmi_width && hdmi_height) {
		mem->width = hdmi_width;
		mem->height = hdmi_height;
	} else if (device->panel.timings.x_res && device->panel.timings.y_res) {
		mem->width = device->panel.timings.x_res;
		mem->height = device->panel.timings.y_res;
	}
	if (device->ctrl.pixel_size)
		mem->bpp = ALIGN(device->ctrl.pixel_size, 16) >> 3;

	pr_info("android_display: setting %s resolution to %u*%u, bpp=%u\n",
		device->name, mem->width, mem->height, mem->bpp);
done:
	return;
}

static void set_tiler1d_slot_size(struct dsscomp_platform_data *dsscomp,
				  struct omap_android_display_data *mem)
{
	struct dsscomp_platform_data data = {
		.tiler1d_slotsz = 0,
	};

	if (dsscomp)
		data = *dsscomp;

	/* do not change board specified value if given */
	if (data.tiler1d_slotsz)
		goto done;

	/*
	 * 4 bytes per pixel, and ICS factor of 4.  The ICS factor
	 * is chosen somewhat arbitrarily to support the home screen layers
	 * to be displayed by DSS.  The size of the home screen layers is
	 * roughly (1 + 2.5 + 0.1 + 0.1) * size_of_the_screen
	 * for the icons, wallpaper, status bar and navigation bar.  Boards
	 * that wish to use a different factor should supply their tiler1D
	 * slot size directly.
	 */
	data.tiler1d_slotsz =
		PAGE_ALIGN(mem->width * mem->height * 4 * 4);

done:
	if (dsscomp)
		*dsscomp = data;
	dsscomp_set_platform_data(&data);

	/* remember setting for ion carveouts */
	mem->tiler1d_mem =
		NUM_ANDROID_TILER1D_SLOTS * data.tiler1d_slotsz;
	pr_info("android_display: tiler1d %u\n", mem->tiler1d_mem);
}

static u32 vram_size(struct omap_android_display_data *mem)
{
	/* calculate required VRAM */
	return PAGE_ALIGN(ALIGN(mem->width, 64) * mem->height * mem->bpp);
}

static void set_vram_sizes(struct sgx_omaplfb_config *sgx_config,
			   struct omapfb_mem_region *region,
			   struct omap_android_display_data *mem,
			   unsigned fbnum)
{
	u32 num_vram_buffers;
	u32 vram = 0;

	if (!sgx_config || !region || !mem)
		return;

	/* Need at least 1 VRAM buffer for fb0 */
	num_vram_buffers = max(sgx_config->vram_buffers, 1u);
	vram += sgx_config->vram_reserve;
	vram += num_vram_buffers * vram_size(mem);

	/* set fb vram needs */
	region->size = vram;
	pr_info("android_display: setting fb%u.vram to %u\n", fbnum, vram);
}

/* coordinate between sgx, omapdss, dsscomp and ion needs */
int __init omap_android_display_setup(struct omap_dss_board_info *dss,
			       struct dsscomp_platform_data *dsscomp,
			       struct sgx_omaplfb_platform_data *sgx,
			       struct omapfb_platform_data *fb)
{
	struct sgx_omaplfb_config *p_sgx_config = NULL;
	int i;
	u32 omapfb_vram = 0;

	struct omap_android_display_data mem = {
		.bpp = 4,
		.width = 1920,
		.height = 1080,
	};

	if (!sgx || !sgx->num_configs)
		return -ENODEV;

	for (i = 0; i < sgx->num_configs; ++i) {
		p_sgx_config = &(sgx->configs[i]);

		if (i < dss->num_devices)
			get_display_size(dss->devices[i], &mem);

		set_tiler1d_slot_size(dsscomp, &mem);

		/* skip region if no sgx_config associated with it
		 * or it has size pre-set by board configuration
		*/
		if (i < fb->mem_desc.region_cnt &&
				!fb->mem_desc.region[i].size)
			set_vram_sizes(p_sgx_config, &fb->mem_desc.region[i],
								&mem, i);
		sgx_omaplfb_set(i, p_sgx_config);
	}

	/* set global vram needs incl. additional regions specified */
	for (i = 0; i < fb->mem_desc.region_cnt; i++)
		if (!fb->mem_desc.region[i].paddr)
			omapfb_vram += fb->mem_desc.region[i].size;

	pr_info("android_display: setting vram to %u\n", omapfb_vram);
	omap_vram_set_sdram_vram(omapfb_vram, 0);

	return 0;
}
