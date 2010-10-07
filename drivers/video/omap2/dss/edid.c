/*
 * linux/drivers/video/omap2/dss/edid.c
 *
 * Copyright (C) 2009 Texas Instruments
 * Author: Mythri P K <mythripk@ti.com>
 * 		With EDID parsing for DVI Monitor from Rob Clark <rob@ti.com>
 *
 * EDID.c to read the EDID content , given the 256 Bytes EDID.
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
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/clk.h>
#include <linux/err.h>

#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/platform_device.h>

#include <plat/cpu.h>

#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <plat/edid.h>


/*This is the structure which has all supported timing values that OMAP4 supports*/
const struct omap_video_timings omap_hdmi_timings[31] = {
						{640, 480, 25200, 96, 16, 48, 2, 10, 33},
						{1280, 720, 74250, 40, 440, 220, 5, 5, 20},
						{1280, 720, 74250, 40, 110, 220, 5, 5, 20},
						{720, 480, 27000, 62, 16, 60, 6, 9, 30},
						{2880, 576, 108000, 256, 48, 272, 5, 5, 39},
						{1440, 240, 27000, 124, 38, 114, 3, 4, 15},
						{1440, 288, 27000, 126, 24, 138, 3, 2, 19},
						{1920, 540, 74250, 44, 528, 148, 5, 2, 15},
						{1920, 540, 74250, 44, 88, 148, 5, 2, 15},
						{1920, 1080, 148500, 44, 88, 148, 5, 4, 36},
						{720, 576, 27000, 64, 12, 68, 5, 5, 39},
						{1440, 576, 54000, 128, 24, 136, 5, 5, 39},
						{1920, 1080, 148500, 44, 528, 148, 5, 4, 36},
						{2880, 480, 108000, 248, 64, 240, 6, 9, 30},
						/*Vesa frome here*/
						{640, 480, 25175, 96, 16, 48, 2 , 11, 31},
						{800, 600, 40000, 128, 40, 88, 4 , 1, 23},
						{848, 480, 33750, 112, 16, 112, 8 , 6, 23},
						{1280, 768, 71000, 128, 64, 192, 7 , 3, 20},
						{1280, 800, 83500, 128, 72, 200, 6 , 3, 22},
						{1360, 768, 85500, 112, 64, 256, 6 , 3, 18},
						{1280, 960, 108000, 112, 96, 312, 3 , 1, 36},
						{1280, 1024, 108000, 112, 48, 248, 3 , 1, 38},
						{1024, 768, 65000, 136, 24, 160, 6, 3, 29},
						{1400, 1050, 121750, 144, 88, 232, 4, 3, 32},
						{1440, 900, 106500, 152, 80, 232, 6, 3, 25},
						{1680, 1050, 146250, 176 , 104, 280, 6, 3, 30},
						{1366, 768, 85500, 143, 70, 213, 3, 3, 24},
						{1920, 1080, 148500, 44, 88, 80, 5, 4, 36},
						{1280, 768, 68250, 32, 48, 80, 7, 3, 12},
						{1400, 1050, 101000, 32, 48, 80, 4, 3, 23},
						{1680, 1050, 119000, 32, 48, 80, 6, 3, 21} } ;

int get_edid_timing_info(union HDMI_EDID_DTD *edid_dtd, struct omap_video_timings *timings)
{

	if (edid_dtd->video.pixel_clock) {

		struct HDMI_EDID_DTD_VIDEO *vid = &edid_dtd->video;

		timings->pixel_clock = 10 * vid->pixel_clock;
		timings->x_res = vid->horiz_active |
				(((u16)vid->horiz_high & 0xf0) << 4);
		timings->y_res = vid->vert_active |
				(((u16)vid->vert_high & 0xf0) << 4);
		timings->hfp = vid->horiz_sync_offset |
				(((u16)vid->sync_pulse_high & 0xc0) << 2);
		timings->hsw = vid->horiz_sync_pulse |
				(((u16)vid->sync_pulse_high & 0x30) << 4);
		timings->hbp = (vid->horiz_blanking |
				(((u16)vid->horiz_high & 0x0f) << 8)) -
				(timings->hfp + timings->hsw);
		timings->vfp = ((vid->vert_sync_pulse & 0xf0) >> 4) |
				((vid->sync_pulse_high & 0x0f) << 2);
		timings->vsw = (vid->vert_sync_pulse & 0x0f) |
				((vid->sync_pulse_high & 0x03) << 4);
		timings->vbp = (vid->vert_blanking |
				(((u16)vid->vert_high & 0x0f) << 8)) -
				(timings->vfp + timings->vsw);
		return 0;
	} else {
		switch (edid_dtd->monitor_name.block_type) {
		case HDMI_EDID_DTD_TAG_STANDARD_TIMING_DATA:
			printk(KERN_INFO "standard timing data\n");
			return 1;
		case HDMI_EDID_DTD_TAG_COLOR_POINT_DATA:
			printk(KERN_INFO "color point data\n");
			return 1;
		case HDMI_EDID_DTD_TAG_MONITOR_NAME:
			printk(KERN_INFO "monitor name: %s\n", edid_dtd->monitor_name.text);
			return 1;
		case HDMI_EDID_DTD_TAG_MONITOR_LIMITS: {
			int i, max_area = 0, best_idx = -1;
			struct HDMI_EDID_DTD_MONITOR *limits = &edid_dtd->monitor_limits;

			printk(KERN_INFO "monitor limits\n");
			printk(KERN_INFO "  min_vert_freq=%d\n",
							limits->min_vert_freq);
			printk(KERN_INFO "  max_vert_freq=%d\n",
							limits->max_vert_freq);
			printk(KERN_INFO "  min_horiz_freq=%d\n",
							limits->min_horiz_freq);
			printk(KERN_INFO "  max_horiz_freq=%d\n",
							limits->max_horiz_freq);
			printk(KERN_INFO "  pixel_clock_mhz=%d\n",
							limits->pixel_clock_mhz * 10);

			/* loop through supported timings, and find the best matching
			* resolution.. where highest resolution (w*h) is considered
			* as best */

			/* XXX since this is mainly for DVI monitors, should we only
			* support VESA timings?  My monitor at home would pick 1920x1080 				* otherwise, but that seems to not work well (monitor blanks out and 				* comes back, and picture doesn't fill full screen, but leaves a black 				* bar on left (native res is 2048x1152). However if I only consider
			* VESA timings, it picks 1680x1050 and the picture is stable and fills 				* whole screen..
			*/
			for (i = 14; i < 31; i++) {
				const struct omap_video_timings *timings =
							 &omap_hdmi_timings[i];
				int hz, hscan, pixclock;
				int vtotal, htotal;
				htotal = timings->hbp + timings->hfp +
						timings->hsw + timings->x_res;
				vtotal = timings->vbp + timings->vfp +
						timings->vsw + timings->y_res;
				/* NOTE: We dont support interlaced mode for VESA */
				pixclock = timings->pixel_clock * 1000;
				hscan = (pixclock + htotal / 2) / htotal;
				hscan = (hscan + 500) / 1000 * 1000;
				hz = (hscan + vtotal / 2) / vtotal;
				hscan /= 1000;
				pixclock /= 1000000;
				printk(KERN_DEBUG "debug only pixclock=%d, hscan=%d, hz=%d\n",
						pixclock, hscan, hz);
				if ((pixclock < (limits->pixel_clock_mhz * 10)) &&
					(limits->min_horiz_freq <= hscan) &&
					(hscan <= limits->max_horiz_freq) &&
					(limits->min_vert_freq <= hz) &&
					(hz <= limits->max_vert_freq)) {
					int area = timings->x_res * timings->y_res;
					printk(KERN_INFO " -> %d: %dx%d\n", i, timings->x_res,
						timings->y_res);
					if (area > max_area) {
						max_area = area;
						best_idx = i;
					}
				}
			}
			if (best_idx > 0) {
				*timings = omap_hdmi_timings[best_idx];
				printk(KERN_INFO "found best resolution: %dx%d (%d)\n",
					timings->x_res, timings->y_res, best_idx);
			}
			return 0;
		}
		case HDMI_EDID_DTD_TAG_ASCII_STRING:
			printk(KERN_INFO "ascii string: %s\n", edid_dtd->ascii.text);
			return 1;
		case HDMI_EDID_DTD_TAG_MONITOR_SERIALNUM:
			printk(KERN_INFO "monitor serialnum: %s\n",
				edid_dtd->monitor_serial_number.text);
			return 1;
		default:
			printk(KERN_INFO "unsupported EDID descriptor block format\n");
			return 1;
		}
	}
}

void get_eedid_timing_info(int current_descriptor_addrs, u8 *edid ,
			struct omap_video_timings *timings)
{

	timings->x_res = (((edid[current_descriptor_addrs + 4] & 0xF0) << 4)
				| edid[current_descriptor_addrs + 2]);
	timings->y_res = (((edid[current_descriptor_addrs + 7] & 0xF0) << 4)
				| edid[current_descriptor_addrs + 5]);
	timings->pixel_clock = ((edid[current_descriptor_addrs + 1] << 8)
				| edid[current_descriptor_addrs]);
	timings->pixel_clock = 10 * timings->pixel_clock;
	timings->hfp = edid[current_descriptor_addrs + 8];
	timings->hsw = edid[current_descriptor_addrs + 9];
	timings->hbp = (((edid[current_descriptor_addrs + 4] & 0x0F) << 8)
				| edid[current_descriptor_addrs + 3]) -
				(timings->hfp + timings->hsw);
	timings->vfp = ((edid[current_descriptor_addrs + 10] & 0xF0) >> 4);
	timings->vsw = (edid[current_descriptor_addrs + 10] & 0x0F);
	timings->vbp = (((edid[current_descriptor_addrs + 7] & 	0x0F) << 8)
				| edid[current_descriptor_addrs + 6]) -
				(timings->vfp + timings->vsw);
}

int hdmi_get_datablock_offset(u8 *edid, enum extension_edid_db datablock, int *offset)
{
	int current_byte, disp, i = 0, length = 0;
	if (edid[0x7e] != 0x00) {
		printk(KERN_INFO "Extension block present");
		disp = edid[(0x80) + 2];
		printk("db %d %x ", datablock, disp);
		if (disp != 0x4) {
			i = 0x80 + 0x4;
			printk("%x", i);
			while (i < (0x80 + disp)) {
				current_byte = edid[i];
				printk("i = %x "
					"cur_byte = %x "
					" (current_byte & HDMI_EDID_EX_DATABLOCK_TAG_MASK)"
					" = %d",
					i, current_byte,
					(current_byte & HDMI_EDID_EX_DATABLOCK_TAG_MASK));
				if ((current_byte >> 5)	== datablock) {
					*offset = i;
					printk("datablock %d %d ", datablock, *offset);
					return 0;
				} else {
					length = (current_byte & 							HDMI_EDID_EX_DATABLOCK_LEN_MASK) + 1;
					i += length;
				}

			}
		}
	}
	return 1;
}

int hdmi_get_image_format(u8 *edid, struct image_format *format)
{
	int offset, current_byte, j = 0, length = 0;
	enum extension_edid_db vsdb =  DATABLOCK_VIDEO;
	format->length = 0;

	memset(format->fmt, 0, sizeof(struct img_edid) * HDMI_IMG_FORMAT_MAX_LENGTH);
	if (!hdmi_get_datablock_offset(edid, vsdb, &offset)) {
		current_byte = edid[offset];
		length = current_byte & HDMI_EDID_EX_DATABLOCK_LEN_MASK;
		if (length >= HDMI_IMG_FORMAT_MAX_LENGTH)
			format->length = HDMI_IMG_FORMAT_MAX_LENGTH;
		else
			format->length = length;
		for (j = 1 ; j < length ; j++) {
			current_byte = edid[offset+j];
			format->fmt[j-1].code = current_byte & 0x7F;
			format->fmt[j-1].pref = current_byte & 0x80;

		}
	}
	return 0;
}

int hdmi_get_audio_format(u8 *edid, struct audio_format *format)
{
	int offset, current_byte, j = 0, length = 0;
	enum extension_edid_db vsdb =  DATABLOCK_AUDIO;

	format->length = 0;
	memset(format->fmt, 0, sizeof(struct audio_edid) * HDMI_AUDIO_FORMAT_MAX_LENGTH);

	if (!hdmi_get_datablock_offset(edid, vsdb, &offset)) {
		current_byte = edid[offset];
		length = current_byte & HDMI_EDID_EX_DATABLOCK_LEN_MASK;

		if (length >= HDMI_AUDIO_FORMAT_MAX_LENGTH)
			format->length = HDMI_AUDIO_FORMAT_MAX_LENGTH;
		else
			format->length = length;

		for (j = 1 ; j < length ; j++) {
			if (j%3 == 1) {
				current_byte = edid[offset + j];
				format->fmt[j-1].format = current_byte & 0x78;
				format->fmt[j-1].num_of_ch = (current_byte & 0x07) + 1;
			}

		}
	}
	return 0;
}

void hdmi_get_av_delay(u8 *edid, struct latency *lat)
{
	int offset, current_byte, length = 0;
	enum extension_edid_db vsdb =  DATABLOCK_VENDOR;

	if (!hdmi_get_datablock_offset(edid, vsdb, &offset)) {
		current_byte = edid[offset];
		length = current_byte & HDMI_EDID_EX_DATABLOCK_LEN_MASK;
		if (length >= 8 && ((current_byte + 8) & 0x80)) {
			lat->vid_latency = ((current_byte + 8) - 1) * 2;
			lat->aud_latency = ((current_byte + 9) - 1) * 2;
		}
		if (length >= 8 && ((current_byte + 8) & 0xC0)) {
			lat->int_vid_latency = ((current_byte + 10) - 1) * 2;
			lat->int_aud_latency = ((current_byte + 11) - 1) * 2;
		}
	}
}

void hdmi_deep_color_support_info(u8 *edid, struct deep_color *format)
{
	int offset, current_byte, length = 0;
	enum extension_edid_db vsdb = DATABLOCK_VENDOR;
	memset(format, 0, sizeof(struct deep_color));
	if (!hdmi_get_datablock_offset(edid, vsdb, &offset)) {
		current_byte = edid[offset];
		length = current_byte & HDMI_EDID_EX_DATABLOCK_LEN_MASK;
		if (length >= 6) {
			format->bit_30 =
					((current_byte + 6) & 0x10);
			format->bit_36 =
					((current_byte + 6) & 0x20);
		}
		if (length >= 7) {
			format->max_tmds_freq =
					((current_byte + 7)) * 5;
		}
	}
}

int hdmi_tv_yuv_supported(u8 *edid)
{
	if (edid[0x7e] != 0x00) {
		if (edid[0x83] & 0x30) {
			printk(KERN_INFO "YUV supported");
			return 1;
		}
	}
	return 0;
}
