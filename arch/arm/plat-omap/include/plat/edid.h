/*
 * linux/drivers/video/omap2/dss/edid.c
 *
 * Copyright (C) 2009 Texas Instruments
 * Author: Mythri P K <mythripk@ti.com>
 *		with some of the ENUM's and structure derived from Yong Zhi's
 *		hdmi.h(Now obsolete)
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
 */

#ifndef _EDID_H_
#define _EDID_H_

#include <plat/display.h>

/* HDMI EDID Extension Data Block Tags  */
#define HDMI_EDID_EX_DATABLOCK_TAG_MASK		0xE0
#define HDMI_EDID_EX_DATABLOCK_LEN_MASK		0x1F
#define HDMI_EDID_EX_SUPPORTS_AI_MASK			0x80

#define EDID_TIMING_DESCRIPTOR_SIZE		0x12
#define EDID_DESCRIPTOR_BLOCK0_ADDRESS		0x36
#define EDID_DESCRIPTOR_BLOCK1_ADDRESS		0x80
#define EDID_SIZE_BLOCK0_TIMING_DESCRIPTOR	4
#define EDID_SIZE_BLOCK1_TIMING_DESCRIPTOR	4

/* EDID Detailed Timing	Info 0 begin offset */
#define HDMI_EDID_DETAILED_TIMING_OFFSET	0x36

#define HDMI_EDID_PIX_CLK_OFFSET		0
#define HDMI_EDID_H_ACTIVE_OFFSET		2
#define HDMI_EDID_H_BLANKING_OFFSET		3
#define HDMI_EDID_V_ACTIVE_OFFSET		5
#define HDMI_EDID_V_BLANKING_OFFSET		6
#define HDMI_EDID_H_SYNC_OFFSET			8
#define HDMI_EDID_H_SYNC_PW_OFFSET		9
#define HDMI_EDID_V_SYNC_OFFSET			10
#define HDMI_EDID_V_SYNC_PW_OFFSET		11
#define HDMI_EDID_H_IMAGE_SIZE_OFFSET		12
#define HDMI_EDID_V_IMAGE_SIZE_OFFSET		13
#define HDMI_EDID_H_BORDER_OFFSET		15
#define HDMI_EDID_V_BORDER_OFFSET		16
#define HDMI_EDID_FLAGS_OFFSET			17

#define HDMI_IEEE_REGISTRATION_ID		0x000c03

/* HDMI Connected States */
#define HDMI_STATE_NOMONITOR	0 /* No HDMI monitor connected*/
#define HDMI_STATE_CONNECTED	1 /* HDMI monitor connected but powered off */
#define HDMI_STATE_ON		2 /* HDMI monitor connected and powered on*/

/* HDMI EDID Length */
#define HDMI_EDID_MAX_LENGTH			512

/* HDMI EDID DTDs */
#define HDMI_EDID_MAX_DTDS			4

/* HDMI EDID DTD Tags */
#define HDMI_EDID_DTD_TAG_MONITOR_NAME		0xFC
#define HDMI_EDID_DTD_TAG_MONITOR_SERIALNUM	0xFF
#define HDMI_EDID_DTD_TAG_MONITOR_LIMITS	0xFD
#define HDMI_EDID_DTD_TAG_STANDARD_TIMING_DATA	0xFA
#define HDMI_EDID_DTD_TAG_COLOR_POINT_DATA	0xFB
#define HDMI_EDID_DTD_TAG_ASCII_STRING		0xFE

#define HDMI_IMG_FORMAT_MAX_LENGTH		20
#define HDMI_AUDIO_FORMAT_MAX_LENGTH		10
#define HDMI_AUDIO_BASIC_MASK			0x40

/* HDMI EDID Extenion Data Block Values: Video */
#define HDMI_EDID_EX_VIDEO_NATIVE		0x80
#define HDMI_EDID_EX_VIDEO_MASK			0x7F
#define HDMI_EDID_EX_VIDEO_MAX			35

#define HDMI_EDID_EX_VIDEO_640x480p_60Hz_4_3	1
#define HDMI_EDID_EX_VIDEO_720x480p_60Hz_4_3	2
#define HDMI_EDID_EX_VIDEO_720x480p_60Hz_16_9	3
#define HDMI_EDID_EX_VIDEO_1280x720p_60Hz_16_9	4
#define HDMI_EDID_EX_VIDEO_1920x1080i_60Hz_16_9	5
#define HDMI_EDID_EX_VIDEO_720x480i_60Hz_4_3	6
#define HDMI_EDID_EX_VIDEO_720x480i_60Hz_16_9	7
#define HDMI_EDID_EX_VIDEO_720x240p_60Hz_4_3	8
#define HDMI_EDID_EX_VIDEO_720x240p_60Hz_16_9	9
#define HDMI_EDID_EX_VIDEO_2880x480i_60Hz_4_3	10
#define HDMI_EDID_EX_VIDEO_2880x480i_60Hz_16_9	11
#define HDMI_EDID_EX_VIDEO_2880x480p_60Hz_4_3	12
#define HDMI_EDID_EX_VIDEO_2880x480p_60Hz_16_9	13
#define HDMI_EDID_EX_VIDEO_1440x480p_60Hz_4_3	14
#define HDMI_EDID_EX_VIDEO_1440x480p_60Hz_16_9	15
#define HDMI_EDID_EX_VIDEO_1920x1080p_60Hz_16_9	16
#define HDMI_EDID_EX_VIDEO_720x576p_50Hz_4_3	17
#define HDMI_EDID_EX_VIDEO_720x576p_50Hz_16_9	18
#define HDMI_EDID_EX_VIDEO_1280x720p_50Hz_16_9	19
#define HDMI_EDID_EX_VIDEO_1920x1080i_50Hz_16_9	20
#define HDMI_EDID_EX_VIDEO_720x576i_50Hz_4_3	21
#define HDMI_EDID_EX_VIDEO_720x576i_50Hz_16_9	22
#define HDMI_EDID_EX_VIDEO_720x288p_50Hz_4_3	23
#define HDMI_EDID_EX_VIDEO_720x288p_50Hz_16_9	24
#define HDMI_EDID_EX_VIDEO_2880x576i_50Hz_4_3	25
#define HDMI_EDID_EX_VIDEO_2880x576i_50Hz_16_9	26
#define HDMI_EDID_EX_VIDEO_2880x288p_50Hz_4_3	27
#define HDMI_EDID_EX_VIDEO_2880x288p_50Hz_16_9	28
#define HDMI_EDID_EX_VIDEO_1440x576p_50Hz_4_3	29
#define HDMI_EDID_EX_VIDEO_1440x576p_50Hz_16_9	30
#define HDMI_EDID_EX_VIDEO_1920x1080p_50Hz_16_9	31
#define HDMI_EDID_EX_VIDEO_1920x1080p_24Hz_16_9	32
#define HDMI_EDID_EX_VIDEO_1920x1080p_25Hz_16_9	33
#define HDMI_EDID_EX_VIDEO_1920x1080p_30Hz_16_9	34

#define OMAP_HDMI_TIMINGS_NB			37
#define OMAP_HDMI_TIMINGS_VESA_START		15

#ifdef __cplusplus
extern "C" {
#endif

enum extension_edid_db {
	DATABLOCK_AUDIO	= 1,
	DATABLOCK_VIDEO	= 2,
	DATABLOCK_VENDOR = 3,
	DATABLOCK_SPEAKERS = 4,
};

struct img_edid {
	bool pref;
	int code;
};

struct image_format {
	int length;
	struct img_edid fmt[HDMI_IMG_FORMAT_MAX_LENGTH];
};

struct audio_edid {
	int num_of_ch;
	int format;
};

struct audio_format {
	int length;
	struct audio_edid fmt[HDMI_AUDIO_FORMAT_MAX_LENGTH];
};

struct latency {
	/* vid: if indicated, value=1+ms/2 with a max of 251 meaning 500ms */
	int vid_latency;

	int aud_latency;
	int int_vid_latency;
	int int_aud_latency;
};

struct deep_color {
	bool bit_30;
	bool bit_36;
	int max_tmds_freq;
};

/*  Video Descriptor Block  */
struct HDMI_EDID_DTD_VIDEO {
	u16	pixel_clock;		/* 54-55 */
	u8	horiz_active;		/* 56 */
	u8	horiz_blanking;		/* 57 */
	u8	horiz_high;		/* 58 */
	u8	vert_active;		/* 59 */
	u8	vert_blanking;		/* 60 */
	u8	vert_high;		/* 61 */
	u8	horiz_sync_offset;	/* 62 */
	u8	horiz_sync_pulse;	/* 63 */
	u8	vert_sync_pulse;	/* 64 */
	u8	sync_pulse_high;	/* 65 */
	u8	horiz_image_size;	/* 66 */
	u8	vert_image_size;	/* 67 */
	u8	image_size_high;	/* 68 */
	u8	horiz_border;		/* 69 */
	u8	vert_border;		/* 70 */
	u8	misc_settings;		/* 71 */
};

/*	Monitor Limits Descriptor Block	*/
struct HDMI_EDID_DTD_MONITOR {
	u16	pixel_clock;		/* 54-55*/
	u8	_reserved1;		/* 56 */
	u8	block_type;		/* 57 */
	u8	_reserved2;		/* 58 */
	u8	min_vert_freq;		/* 59 */
	u8	max_vert_freq;		/* 60 */
	u8	min_horiz_freq;		/* 61 */
	u8	max_horiz_freq;		/* 62 */
	u8	pixel_clock_mhz;	/* 63 */
	u8	GTF[2];			/* 64 -65 */
	u8	start_horiz_freq;	/* 66	*/
	u8	C;			/* 67 */
	u8	M[2];			/* 68-69 */
	u8	K;			/* 70 */
	u8	J;			/* 71 */

} __attribute__ ((packed));

/* Text Descriptor Block */
struct HDMI_EDID_DTD_TEXT {
	u16	pixel_clock;		/* 54-55 */
	u8	_reserved1;		/* 56 */
	u8	block_type;		/* 57 */
	u8	_reserved2;		/* 58 */
	u8	text[13];		/* 59-71 */
} __attribute__ ((packed));

/* DTD Union */
union HDMI_EDID_DTD {
	struct HDMI_EDID_DTD_VIDEO	video;
	struct HDMI_EDID_DTD_TEXT	monitor_name;
	struct HDMI_EDID_DTD_TEXT	monitor_serial_number;
	struct HDMI_EDID_DTD_TEXT	ascii;
	struct HDMI_EDID_DTD_MONITOR	monitor_limits;
} __attribute__ ((packed));

/*	EDID struct	*/
struct HDMI_EDID {
	u8	header[8];		/* 00-07 */
	u16	manufacturerID;		/* 08-09 */
	u16	product_id;		/* 10-11 */
	u32	serial_number;		/* 12-15 */
	u8	week_manufactured;	/* 16 */
	u8	year_manufactured;	/* 17 */
	u8	edid_version;		/* 18 */
	u8	edid_revision;		/* 19 */
	u8	video_in_definition;	/* 20 */
	u8	max_horiz_image_size;	/* 21 */
	u8	max_vert_image_size;	/* 22 */
	u8	display_gamma;		/* 23 */
	u8	power_features;		/* 24 */
	u8	chroma_info[10];	/* 25-34 */
	u8	timing_1;		/* 35 */
	u8	timing_2;		/* 36 */
	u8	timing_3;		/* 37 */
	u8	std_timings[16];	/* 38-53 */

	union HDMI_EDID_DTD DTD[4];	/* 54-125 */

	u8	extension_edid;		/* 126 */
	u8	checksum;		/* 127 */
	u8	extension_tag;		/* 00 (extensions follow EDID) */
	u8	extention_rev;		/* 01 */
	u8	offset_dtd;		/* 02 */
	u8	num_dtd;		/* 03 */

	u8	data_block[123];	/* 04 - 126 */
	u8	extension_checksum;	/* 127 */

	u8	ext_datablock[256];
} __attribute__ ((packed));

int get_edid_timing_info(union HDMI_EDID_DTD *edid_dtd,
			struct omap_video_timings *timings);
void get_eedid_timing_info(int current_descriptor_addrs, u8 *edid ,
			struct omap_video_timings *timings);
int hdmi_get_datablock_offset(u8 *edid, enum extension_edid_db datablock,
								int *offset);
int hdmi_get_image_format(u8 *edid, struct image_format *format);
int hdmi_get_audio_format(u8 *edid, struct audio_format *format);
bool hdmi_has_ieee_id(u8 *edid);
int hdmi_get_video_svds(u8 *edid, int *offset, int *length);
void hdmi_get_av_delay(u8 *edid, struct latency *lat);
void hdmi_deep_color_support_info(u8 *edid, struct deep_color *format);
int hdmi_tv_yuv_supported(u8 *edid);
bool hdmi_s3d_supported(u8 *edid);
bool hdmi_ai_supported(u8 *edid);
const struct omap_video_timings *hdmi_get_omap_timing(int ix);

#ifdef __cplusplus
};
#endif

#endif

