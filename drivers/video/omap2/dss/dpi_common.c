/*
 * Copyright (C) 2013 Texas Instruments Ltd.
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

#include <linux/kernel.h>
#include <linux/export.h>

#include <video/omapdss.h>

#include "dss.h"
#include "dss_features.h"

static struct dpi_common_ops dpi_ops;

int omapdss_dpi_display_enable(struct omap_dss_device *dssdev)
{
	return dpi_ops.enable(dssdev);
}
EXPORT_SYMBOL(omapdss_dpi_display_enable);

void omapdss_dpi_display_disable(struct omap_dss_device *dssdev)
{
	dpi_ops.disable(dssdev);
}
EXPORT_SYMBOL(omapdss_dpi_display_disable);

void omapdss_dpi_set_timings(struct omap_dss_device *dssdev,
		struct omap_video_timings *timings)
{
	dpi_ops.set_timings(dssdev, timings);
}
EXPORT_SYMBOL(omapdss_dpi_set_timings);

int dpi_check_timings(struct omap_dss_device *dssdev,
			struct omap_video_timings *timings)
{
	return dpi_ops.check_timings(dssdev, timings);
}
EXPORT_SYMBOL(dpi_check_timings);

void omapdss_dpi_set_data_lines(struct omap_dss_device *dssdev, int data_lines)
{
	dpi_ops.set_data_lines(dssdev, data_lines);
}
EXPORT_SYMBOL(omapdss_dpi_set_data_lines);

void dpi_common_set_ops(struct dpi_common_ops *ops)
{
	dpi_ops = *ops;
}

