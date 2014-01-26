/*
 * hdmi.h
 *
 * HDMI interface driver definition
 *
 * Copyright (C) 2010-2011 Texas Instruments Incorporated - http://www.ti.com/
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

#ifndef _HDMI_H
#define _HDMI_H

#include <linux/fb.h>

enum level_shifter_state {
	LS_DISABLED = 0,	/* HPD off, LS off */
	LS_HPD_ON,		/* HPD on, LS off */
	LS_ENABLED,		/* HPD on, LS on */
};

extern struct device_attribute dev_attr_hdmi_timings;

int omapdss_hdmi_display_set_mode2(struct omap_dss_device *dssdev,
				   struct fb_videomode *vm,
				   int code, int mode);
int hdmi_panel_set_mode(struct fb_videomode *vm, int code, int mode);
int hdmi_panel_hpd_handler(int hpd);
int hdmi_get_current_hpd(void);
int hdmi_notify_hpd(struct omap_dss_device *dssdev, bool hpd);
void hdmi_set_ls_state(enum level_shifter_state state);
int hdmi_runtime_get(void);
void hdmi_runtime_put(void);
struct hdmi_ip_data *get_hdmi_ip_data(void);
void omapdss_hdmi_register_hdcp_callbacks(void (*hdmi_start_frame_cb)(void),
					bool (*hdmi_power_on_cb)(void),
					void (*hdmi_hdcp_irq_cb)(int));
bool omapdss_hdmi_get_force_timings(void);
void omapdss_hdmi_reset_force_timings(void);

#ifdef CONFIG_USE_FB_MODE_DB
int omapdss_hdmi_display_set_mode(struct omap_dss_device *dssdev,
					struct fb_videomode *mode);
void hdmi_get_monspecs(struct omap_dss_device *dssdev);
#endif
u8 *hdmi_read_valid_edid(void);
void omapdss_hdmi_clear_edid(void);
ssize_t omapdss_get_edid(char *buf);

#endif
