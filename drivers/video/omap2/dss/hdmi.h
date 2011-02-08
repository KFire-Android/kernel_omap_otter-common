/*
 * drivers/media/video/omap2/dss/hdmi.h
 *
 * Copyright (C) 2009 Texas Instruments
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2. This program is licensed "as is" without any warranty of any
 * kind, whether express or implied.
 *
 * hdmi driver
 */
#ifndef _OMAP4_HDMI_H_
#define _OMAP4_HDMI_H_

#define HDMI_EDID_DETAILED_TIMING_OFFSET  0x36 /*EDID Detailed Timing
							Info 0 begin offset*/
#define HDMI_EDID_PIX_CLK_OFFSET	0
#define HDMI_EDID_H_ACTIVE_OFFSET	2
#define HDMI_EDID_H_BLANKING_OFFSET	3
#define HDMI_EDID_V_ACTIVE_OFFSET	5
#define HDMI_EDID_V_BLANKING_OFFSET	6
#define HDMI_EDID_H_SYNC_OFFSET		8
#define HDMI_EDID_H_SYNC_PW_OFFSET	9
#define HDMI_EDID_V_SYNC_OFFSET		10
#define HDMI_EDID_V_SYNC_PW_OFFSET	10
#define HDMI_EDID_H_IMAGE_SIZE_OFFSET	12
#define HDMI_EDID_V_IMAGE_SIZE_OFFSET	13
#define HDMI_EDID_H_BORDER_OFFSET	15
#define HDMI_EDID_V_BORDER_OFFSET	16
#define HDMI_EDID_FLAGS_OFFSET		17

#define EDID_TIMING_DESCRIPTOR_SIZE		0x12
#define EDID_DESCRIPTOR_BLOCK0_ADDRESS		0x36
#define EDID_DESCRIPTOR_BLOCK1_ADDRESS		0x80
#define EDID_SIZE_BLOCK0_TIMING_DESCRIPTOR	4
#define EDID_SIZE_BLOCK1_TIMING_DESCRIPTOR	4

/* HDMI Connected States */
#define HDMI_STATE_NOMONITOR	0 /* No HDMI monitor connected*/
#define HDMI_STATE_CONNECTED	1 /* HDMI monitor connected but powered off*/
#define HDMI_STATE_ON		2 /* HDMI monitor connected and powered on*/


/* HDMI EDID Length */
#define HDMI_EDID_MAX_LENGTH			256

/* HDMI EDID DTDs */
#define HDMI_EDID_MAX_DTDS			4

/* HDMI EDID DTD Tags */
#define HDMI_EDID_DTD_TAG_MONITOR_NAME		0xFC
#define HDMI_EDID_DTD_TAG_MONITOR_SERIALNUM	0xFF
#define HDMI_EDID_DTD_TAG_MONITOR_LIMITS	0xFD

/* HDMI EDID Extension Data Block Tags */
#define HDMI_EDID_EX_DATABLOCK_TAG_MASK		0xE0
#define HDMI_EDID_EX_DATABLOCK_LEN_MASK		0x1F

#define HDMI_EDID_EX_DATABLOCK_AUDIO		0x20
#define HDMI_EDID_EX_DATABLOCK_VIDEO		0x40
#define HDMI_EDID_EX_DATABLOCK_VENDOR		0x60
#define HDMI_EDID_EX_DATABLOCK_SPEAKERS		0x80

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

/*---------------------------------------------------------------------  */

#ifdef __cplusplus
extern "C" {
#endif

/* Video Descriptor Block */
typedef struct {
	u8 pixel_clock[2];	/* 54-55 */
	u8 horiz_active;	/* 56 */
	u8 horiz_blanking;	/* 57 */
	u8 horiz_high;		/* 58 */
	u8 vert_active;		/* 59 */
	u8 vert_blanking;	/* 60 */
	u8 vert_high;		/* 61 */
	u8 horiz_sync_offset;	/* 62 */
	u8 horiz_sync_pulse;	/* 63 */
	u8 vert_sync_pulse;	/* 64 */
	u8 sync_pulse_high;	/* 65 */
	u8 horiz_image_size;	/* 66 */
	u8 vert_image_size;	/* 67 */
	u8 image_size_high;	/* 68 */
	u8 horiz_border;	/* 69 */
	u8 vert_border;		/* 70 */
	u8 misc_settings;	/* 71 */
}
HDMI_EDID_DTD_VIDEO;

/* Monitor Limits Descriptor Block */
typedef struct {
	u8 pixel_clock[2];	/* 54-55*/
	u8 _reserved1;		/* 56 */
	u8 block_type;		/* 57 */
	u8 _reserved2;		/* 58 */
	u8 min_vert_freq;	/* 59 */
	u8 max_vert_freq;	/* 60 */
	u8 min_horiz_freq;	/* 61 */
	u8 max_horiz_freq;	/* 62 */
	u8 pixel_clock_mhz;	/* 63 */

	u8 GTF[2];		/* 64 -65 */
	u8 start_horiz_freq;	/* 66	*/
	u8 C;			/* 67 */
	u8 M[2];		/* 68-69 */
	u8 K;			/* 70 */
	u8 J;			/* 71 */
}
HDMI_EDID_DTD_MONITOR;

/* Text Descriptor Block */
typedef struct {
	u8 pixel_clock[2];	/* 54-55 */
	u8 _reserved1;		/* 56 */
	u8 block_type;		/* 57 */
	u8 _reserved2;		/* 58 */

	u8 text[13];		/* 59-71 */
}
HDMI_EDID_DTD_TEXT;

/* DTD Union */
typedef union {
	HDMI_EDID_DTD_VIDEO	video;
	HDMI_EDID_DTD_TEXT	monitor_name;
	HDMI_EDID_DTD_TEXT	monitor_serial_number;
	HDMI_EDID_DTD_MONITOR	monitor_limits;
}
HDMI_EDID_DTD;

/* EDID struct */
typedef struct {
	u8 header[8];		/* 00-07 */
	u8 manufacturerID[2];	/* 08-09 */
	u8 product_id[2];	/* 10-11 */
	u8 serial_number[4];	/* 12-15 */
	u8 week_manufactured;	/* 16 */
	u8 year_manufactured;	/* 17 */
	u8 edid_version;	/* 18 */
	u8 edid_revision;	/* 19 */

	u8 video_in_definition;	/* 20 */
	u8 max_horiz_image_size;/* 21 */
	u8 max_vert_image_size;	/* 22 */
	u8 display_gamma;	/* 23 */
	u8 power_features;	/* 24 */
	u8 chroma_info[10];	/* 25-34 */
	u8 timing_1;		/* 35 */
	u8 timing_2;		/* 36 */
	u8 timing_3;		/* 37 */
	u8 std_timings[16];	/* 38-53 */

	HDMI_EDID_DTD DTD[4];	/* 72-125 */

	u8 extension_edid;	/* 126 */
	u8 checksum;		/* 127 */

	u8 extension_tag;	/* 00 (extensions follow EDID) */
	u8 extention_rev;	/* 01 */
	u8 offset_dtd;		/* 02 */
	u8 num_dtd;		/* 03 */

	u8 data_block[123];	/* 04 - 126 */
	u8 extension_checksum;	/* 127 */
 }
HDMI_EDID;

void show_horz_vert_timing_info(u8 *edid);
int hdmi_get_image_format(void);
int hdmi_get_audio_format(void);
int hdmi_register_hdcp_callbacks(void (*hdmi_start_frame_cb)(void),
				 void (*hdmi_stop_frame_cb)(void));
void hdmi_restart(void);

enum hdmi_ioctl_cmds {
	HDMI_ENABLE,
	HDMI_DISABLE,
	HDMI_READ_EDID,
};

#ifdef __cplusplus
};
#endif

#endif
