/*
 * omap-hdmi.h
 *
 * Copyright (C) 2009 Texas Instruments
 *
 * Contact: Jorge Candelaria <x0107209@ti.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#ifndef __OMAP_HDMI_H__
#define __OMAP_HDMI_H__

#ifndef CONFIG_HDMI_NO_IP_MODULE

#define HDMI_WP			0x58006000
#define HDMI_WP_AUDIO_DATA	0x8Cul

struct hdmi_ip_driver {
	int (*stop_video)(u32);
	int (*start_video)(u32);
	int (*wrapper_enable)(u32);
	int (*wrapper_disable)(u32);
	int (*stop_audio)(u32);
	int (*start_audio)(u32);
	int (*config_video)(struct hdmi_timing_t, u32, u32);
	int (*set_wait_pll)(u32, enum hdmi_pllpwr_cmd);
	int (*set_wait_pwr)(u32, enum hdmi_phypwr_cmd);
	int (*set_wait_srst)(void);
	int (*read_edid)(u32, u8 *d);
	int (*ip_init)(void);
	void (*ip_exit)(void);
	int module_loaded;
};

extern void hdmi_audio_core_stub_init(void);
#endif

#endif	/* End of __OMAP_HDMI_H__ */
