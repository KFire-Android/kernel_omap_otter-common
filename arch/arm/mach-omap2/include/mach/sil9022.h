/*
 * drivers/media/video/omap/sil9022.c
 *
 * Copyright (C) 2009 Texas Instruments
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2. This program is licensed "as is" without any warranty of any
 * kind, whether express or implied.
 *
 * sil9022 hdmi driver
 */
#ifndef _SI9022_H_
#define _SI9022_H_

#define SIL9022_DRV_NAME		"sil9022"

#define CLKOUT2_EN			(0x1 << 7)
#define CLKOUT2_DIV			(0x4 << 3)
#define CLKOUT2SOURCE			(0x2 << 0)
#define CM_CLKOUT_CTRL			0x48004D70

#define HDMI_XRES			1280
/*#define HDMI_XRES			1920*/
#define HDMI_YRES			720
/*#define HDMI_YRES			1080*/
#define HDMI_PIXCLOCK_MAX		74250
#define VERTICAL_FREQ			0x3C

#define I2C_M_WR			0

#define SI9022_USERST_PIN		1	/*Uncomment this if RST pin
						is to be used*/

#define SI9022_REG_PIX_CLK_LSB		0x00
#define SI9022_REG_PIX_CLK_MSB		0x01

#define SI9022_REG_PIX_REPETITION	0x08
#define SI9022_REG_INPUT_FORMAT		0x09
#define SI9022_REG_OUTPUT_FORMAT	0x0A
#define SI9022_REG_SYNC_GEN_CTRL	0x60

#define SI9022_REG_DE_CTRL		0x63
#define DE_DLY_MSB_BITPOS		0
#define HSYNCPOL_INVERT_BITPOS		4
#define VSYNCPOL_INVERT_BITPOS		5
#define DE_GENERATOR_EN_BITPOS		6

#define SI9022_REG_PWR_STATE		0x1E

#define SI9022_REG_TPI_RQB		0xC7

#define SI9022_REG_INT_PAGE		0xBC
#define SI9022_REG_OFFSET		0xBD
#define	SI9022_REG_VALUE		0xBE

#define SI9022_PLLMULT_BITPOS		0x05

#define SI9022_REG_TPI_SYSCTRL		0x1A
#define I2CDDCGRANT_BITPOS		1
#define I2DDCREQ_BITPOS			2
#define TMDS_ENABLE_BITPOS		4
#define HDMI_ENABLE_BITPOS		0

#define SI9022_REG_CHIPID0		0x1B
#define SI9022_REG_CHIPID1		0x1C
#define SI9022_REG_CHIPID2		0x1D
#define SI9022_REG_HDCPVER		0x30

#define SI9022_REG_INTSTATUS		0x3D
#define HOTPLUG_PENDING_BITPOS		0
#define RCV_SENSE_PENDING_BITPOS	1
#define HOTPLUG_SENSE_BITPOS		2
#define RX_SENSE_BITPOS			3
#define AUDIO_ERR_PENDING_BITPOS	4


#define SI9022_I2CSLAVEADDRESS		0x39
#define SI9022_EDIDI2CSLAVEADDRESS	0x50

#define SI9022_CHIPID_902x		0xB0


#define SI9022_CH1_I2CINTERFACE		0x03
#define SI9022_MAXRETRY			100

#define SI9022_RESETPADNUM		HALGPIO_GPIO6

#define SI9022_EDID_DETAILED_TIMING_OFFSET  0x36 /*EDID Detailed Timing
							Info 0 begin offset*/
#define SI9022_EDID_PIX_CLK_OFFSET          0
#define SI9022_EDID_H_ACTIVE_OFFSET         2
#define SI9022_EDID_H_BLANKING_OFFSET       3
#define SI9022_EDID_V_ACTIVE_OFFSET         5
#define SI9022_EDID_V_BLANKING_OFFSET       6
#define SI9022_EDID_H_SYNC_OFFSET           8
#define SI9022_EDID_H_SYNC_PW_OFFSET        9
#define SI9022_EDID_V_SYNC_OFFSET           10
#define SI9022_EDID_V_SYNC_PW_OFFSET        10
#define SI9022_EDID_H_IMAGE_SIZE_OFFSET     12
#define SI9022_EDID_V_IMAGE_SIZE_OFFSET     13
#define SI9022_EDID_H_BORDER_OFFSET         15
#define SI9022_EDID_V_BORDER_OFFSET         16
#define SI9022_EDID_FLAGS_OFFSET            17

#define SI9022_PLUG_DETECTED                0xF0
#define SI9022_UNPLUG_DETECTED              0xF1


/* ---------------------------------------------------------------------  */
#define EDID_TIMING_DESCRIPTOR_SIZE		0x12
#define EDID_DESCRIPTOR_BLOCK0_ADDRESS		0x36
#define EDID_DESCRIPTOR_BLOCK1_ADDRESS		0x80
#define EDID_SIZE_BLOCK0_TIMING_DESCRIPTOR	4
#define EDID_SIZE_BLOCK1_TIMING_DESCRIPTOR	4

/* HDMI Connected States  */
#define HDMI_STATE_NOMONITOR    0       /* No HDMI monitor connected*/
#define HDMI_STATE_CONNECTED    1  /* HDMI monitor connected but powered off*/
#define HDMI_STATE_ON           2  /* HDMI monitor connected and powered on*/


/* HDMI EDID Length  */
#define HDMI_EDID_MAX_LENGTH    256

/* HDMI EDID DTDs  */
#define HDMI_EDID_MAX_DTDS      4

/* HDMI EDID DTD Tags  */
#define HDMI_EDID_DTD_TAG_MONITOR_NAME              0xFC
#define HDMI_EDID_DTD_TAG_MONITOR_SERIALNUM         0xFF
#define HDMI_EDID_DTD_TAG_MONITOR_LIMITS            0xFD


/* HDMI EDID Extension Data Block Tags  */
#define HDMI_EDID_EX_DATABLOCK_TAG_MASK             0xE0
#define HDMI_EDID_EX_DATABLOCK_LEN_MASK             0x1F

#define HDMI_EDID_EX_DATABLOCK_AUDIO                0x20
#define HDMI_EDID_EX_DATABLOCK_VIDEO                0x40
#define HDMI_EDID_EX_DATABLOCK_VENDOR               0x60
#define HDMI_EDID_EX_DATABLOCK_SPEAKERS             0x80

/* HDMI EDID Extenion Data Block Values: Video  */
#define HDMI_EDID_EX_VIDEO_NATIVE                   0x80
#define HDMI_EDID_EX_VIDEO_MASK                     0x7F
#define HDMI_EDID_EX_VIDEO_MAX                      35

#define HDMI_EDID_EX_VIDEO_640x480p_60Hz_4_3        1
#define HDMI_EDID_EX_VIDEO_720x480p_60Hz_4_3        2
#define HDMI_EDID_EX_VIDEO_720x480p_60Hz_16_9       3
#define HDMI_EDID_EX_VIDEO_1280x720p_60Hz_16_9      4
#define HDMI_EDID_EX_VIDEO_1920x1080i_60Hz_16_9     5
#define HDMI_EDID_EX_VIDEO_720x480i_60Hz_4_3        6
#define HDMI_EDID_EX_VIDEO_720x480i_60Hz_16_9       7
#define HDMI_EDID_EX_VIDEO_720x240p_60Hz_4_3        8
#define HDMI_EDID_EX_VIDEO_720x240p_60Hz_16_9       9
#define HDMI_EDID_EX_VIDEO_2880x480i_60Hz_4_3       10
#define HDMI_EDID_EX_VIDEO_2880x480i_60Hz_16_9      11
#define HDMI_EDID_EX_VIDEO_2880x480p_60Hz_4_3       12
#define HDMI_EDID_EX_VIDEO_2880x480p_60Hz_16_9      13
#define HDMI_EDID_EX_VIDEO_1440x480p_60Hz_4_3       14
#define HDMI_EDID_EX_VIDEO_1440x480p_60Hz_16_9      15
#define HDMI_EDID_EX_VIDEO_1920x1080p_60Hz_16_9     16
#define HDMI_EDID_EX_VIDEO_720x576p_50Hz_4_3        17
#define HDMI_EDID_EX_VIDEO_720x576p_50Hz_16_9       18
#define HDMI_EDID_EX_VIDEO_1280x720p_50Hz_16_9      19
#define HDMI_EDID_EX_VIDEO_1920x1080i_50Hz_16_9     20
#define HDMI_EDID_EX_VIDEO_720x576i_50Hz_4_3        21
#define HDMI_EDID_EX_VIDEO_720x576i_50Hz_16_9       22
#define HDMI_EDID_EX_VIDEO_720x288p_50Hz_4_3        23
#define HDMI_EDID_EX_VIDEO_720x288p_50Hz_16_9       24
#define HDMI_EDID_EX_VIDEO_2880x576i_50Hz_4_3       25
#define HDMI_EDID_EX_VIDEO_2880x576i_50Hz_16_9      26
#define HDMI_EDID_EX_VIDEO_2880x288p_50Hz_4_3       27
#define HDMI_EDID_EX_VIDEO_2880x288p_50Hz_16_9      28
#define HDMI_EDID_EX_VIDEO_1440x576p_50Hz_4_3       29
#define HDMI_EDID_EX_VIDEO_1440x576p_50Hz_16_9      30
#define HDMI_EDID_EX_VIDEO_1920x1080p_50Hz_16_9     31
#define HDMI_EDID_EX_VIDEO_1920x1080p_24Hz_16_9     32
#define HDMI_EDID_EX_VIDEO_1920x1080p_25Hz_16_9     33
#define HDMI_EDID_EX_VIDEO_1920x1080p_30Hz_16_9     34





/*                      */
/*  HDMI TPI Registers  */
/*                      */
/*                         */
#define HDMI_TPI_VIDEO_DATA_BASE_REG     0x00
#define HDMI_TPI_PIXEL_CLK_LSB_REG       (HDMI_TPI_VIDEO_DATA_BASE_REG + 0x00)
#define HDMI_TPI_PIXEL_CLK_MSB_REG       (HDMI_TPI_VIDEO_DATA_BASE_REG + 0x01)
#define HDMI_TPI_VFREQ_LSB_REG           (HDMI_TPI_VIDEO_DATA_BASE_REG + 0x02)
#define HDMI_TPI_VFREQ_MSB_REG           (HDMI_TPI_VIDEO_DATA_BASE_REG + 0x03)
#define HDMI_TPI_PIXELS_LSB_REG          (HDMI_TPI_VIDEO_DATA_BASE_REG + 0x04)
#define HDMI_TPI_PIXELS_MSB_REG          (HDMI_TPI_VIDEO_DATA_BASE_REG + 0x05)
#define HDMI_TPI_LINES_LSB_REG           (HDMI_TPI_VIDEO_DATA_BASE_REG + 0x06)
#define HDMI_TPI_LINES_MSB_REG           (HDMI_TPI_VIDEO_DATA_BASE_REG + 0x07)

#define HDMI_TPI_PIXEL_REPETITION_REG    0x08

#define HDMI_TPI_AVI_INOUT_BASE_REG      0x09
#define HDMI_TPI_AVI_IN_FORMAT_REG       (HDMI_TPI_AVI_INOUT_BASE_REG + 0x00)
#define HDMI_TPI_AVI_OUT_FORMAT_REG      (HDMI_TPI_AVI_INOUT_BASE_REG + 0x01)

#define HDMI_SYS_CTRL_DATA_REG           0x1A

#define HDMI_TPI_SYN_GENERATOR_REG       0x60

#define HDMI_TPI_VIDEO_SYN_POLARITY_REG  0x61

#define HDMI_TPI_DE_BASE_REG                0x62
#define HDMI_TPI_DE_DLY_LSB_REG             (HDMI_TPI_DE_BASE_REG + 0x0)
#define HDMI_TPI_DE_DLY_MSB_REG             (HDMI_TPI_DE_BASE_REG + 0x1)
#define HDMI_TPI_DE_TOP_REG                 (HDMI_TPI_DE_BASE_REG + 0x2)
#define HDMI_TPI_DE_RSVD_REG                (HDMI_TPI_DE_BASE_REG + 0x3)
#define HDMI_TPI_DE_CNT_LSB_REG             (HDMI_TPI_DE_BASE_REG + 0x4)
#define HDMI_TPI_DE_CNT_MSB_REG             (HDMI_TPI_DE_BASE_REG + 0x5)
#define HDMI_TPI_DE_LIN_LSB_REG             (HDMI_TPI_DE_BASE_REG + 0x6)
#define HDMI_TPI_DE_LIN_MSB_REG             (HDMI_TPI_DE_BASE_REG + 0x7)

#define HDMI_TPI_HRES_LSB_REG               0x6A
#define HDMI_TPI_HRES_MSB_REG               0x6B

#define HDMI_TPI_VRES_LSB_REG               0x6C
#define HDMI_TPI_VRES_MSB_REG               0x6D

#define HDMI_TPI_RQB_REG                    0xC7
#define HDMI_TPI_DEVID_REG                  0x1B
#define HDMI_TPI_DEVREV_REG                 0x1C

#define HDMI_TPI_DEVICE_POWER_STATE_DATA    0x1E
#define HDMI_REQ_GRANT_BMODE_REG            0xC7
#define HDMI_TPI_DEVICE_ID_REG              0x1B
#define HDMI_TPI_REVISION_REG               0x1C
#define HDMI_TPI_ID_BYTE2_REG               0x1D
#define HDMI_TPI_POWER_STATE_CTRL_REG       0x1E

#define HDMI_TPI_INTERRUPT_ENABLE_REG       0x3C
#define HDMI_TPI_INTERRUPT_STATUS_REG       0x3D


/* AVI InfoFrames can be readed byte by byte but must be write in a burst  */
#define HDMI_TPI_AVI_DBYTE_BASE_REG      0x0C
#define HDMI_TPI_AVI_DBYTE0_CHKSUM_REG   (HDMI_TPI_AVI_DBYTE_BASE_REG +  0x00)
#define HDMI_TPI_AVI_DBYTE1_REG          (HDMI_TPI_AVI_DBYTE_BASE_REG +  0x01)
#define HDMI_TPI_AVI_DBYTE2_REG          (HDMI_TPI_AVI_DBYTE_BASE_REG +  0x02)
#define HDMI_TPI_AVI_DBYTE3_REG          (HDMI_TPI_AVI_DBYTE_BASE_REG +  0x03)
#define HDMI_TPI_AVI_DBYTE4_REG          (HDMI_TPI_AVI_DBYTE_BASE_REG +  0x04)
#define HDMI_TPI_AVI_DBYTE5_REG          (HDMI_TPI_AVI_DBYTE_BASE_REG +  0x05)
#define HDMI_TPI_AVI_ETB_LSB_REG         (HDMI_TPI_AVI_DBYTE_BASE_REG +  0x06)
#define HDMI_TPI_AVI_ETB_MSB_REG         (HDMI_TPI_AVI_DBYTE_BASE_REG +  0x07)
#define HDMI_TPI_AVI_SBB_LSB_REG         (HDMI_TPI_AVI_DBYTE_BASE_REG +  0x08)
#define HDMI_TPI_AVI_SBB_MSB_REG         (HDMI_TPI_AVI_DBYTE_BASE_REG +  0x09)
#define HDMI_TPI_AVI_ELB_LSB_REG         (HDMI_TPI_AVI_DBYTE_BASE_REG +  0x0A)
#define HDMI_TPI_AVI_ELB_MSB_REG         (HDMI_TPI_AVI_DBYTE_BASE_REG +  0x0B)
#define HDMI_TPI_AVI_SRB_LSB_REG         (HDMI_TPI_AVI_DBYTE_BASE_REG +  0x0C)
#define HDMI_TPI_AVI_SRB_MSB_REG         (HDMI_TPI_AVI_DBYTE_BASE_REG +  0x0D)

#define HDMI_CPI_MISC_IF_SELECT_REG         0xBF
#define HDMI_CPI_MISC_IF_OFFSET             0xC0

#define MISC_INFOFRAME_SIZE_MEMORY          14
#define MISC_INFOFRAME_TYPE_SUBOFFSET       0
#define MISC_INFOFRAME_VERSION_SUBOFFSET    1
#define MISC_INFOFRAME_LENGTH_SUBOFFSET     2
#define MISC_INFOFRAME_CHECKSUM_SUBOFFSET   3
#define MISC_INFOFRAME_DBYTE1_SUBOFFSET     4
#define MISC_INFOFRAME_DBYTE2_SUBOFFSET     5
#define MISC_INFOFRAME_DBYTE3_SUBOFFSET     6
#define MISC_INFOFRAME_DBYTE4_SUBOFFSET     7
#define MISC_INFOFRAME_DBYTE5_SUBOFFSET     8
#define MISC_INFOFRAME_DBYTE6_SUBOFFSET     9
#define MISC_INFOFRAME_DBYTE7_SUBOFFSET     10
#define MISC_INFOFRAME_DBYTE8_SUBOFFSET     11
#define MISC_INFOFRAME_DBYTE9_SUBOFFSET     12
#define MISC_INFOFRAME_DBYTE10_SUBOFFSET    13

#define HDMI_CPI_MISC_TYPE_REG		(HDMI_CPI_MISC_IF_OFFSET\
					+  MISC_INFOFRAME_TYPE_SUBOFFSET)
#define HDMI_CPI_MISC_VERSION_REG	(HDMI_CPI_MISC_IF_OFFSET\
					+  MISC_INFOFRAME_VERSION_SUBOFFSET)
#define HDMI_CPI_MISC_LENGTH_REG	(HDMI_CPI_MISC_IF_OFFSET\
					+  MISC_INFOFRAME_LENGTH_SUBOFFSET)
#define HDMI_CPI_MISC_CHECKSUM_REG	(HDMI_CPI_MISC_IF_OFFSET\
					+  MISC_INFOFRAME_CHECKSUM_SUBOFFSET)
#define HDMI_CPI_MISC_DBYTE1_REG        (HDMI_CPI_MISC_IF_OFFSET\
					+  MISC_INFOFRAME_DBYTE1_SUBOFFSET)
#define HDMI_CPI_MISC_DBYTE2_REG        (HDMI_CPI_MISC_IF_OFFSET\
					+  MISC_INFOFRAME_DBYTE2_SUBOFFSET)
#define HDMI_CPI_MISC_DBYTE3_REG        (HDMI_CPI_MISC_IF_OFFSET\
					+  MISC_INFOFRAME_DBYTE3_SUBOFFSET)
#define HDMI_CPI_MISC_DBYTE4_REG        (HDMI_CPI_MISC_IF_OFFSET\
					+  MISC_INFOFRAME_DBYTE4_SUBOFFSET)
#define HDMI_CPI_MISC_DBYTE5_REG        (HDMI_CPI_MISC_IF_OFFSET\
					+  MISC_INFOFRAME_DBYTE5_SUBOFFSET)
#define HDMI_CPI_MISC_DBYTE6_REG        (HDMI_CPI_MISC_IF_OFFSET\
					+  MISC_INFOFRAME_DBYTE6_SUBOFFSET)
#define HDMI_CPI_MISC_DBYTE7_REG        (HDMI_CPI_MISC_IF_OFFSET\
					+  MISC_INFOFRAME_DBYTE7_SUBOFFSET)
#define HDMI_CPI_MISC_DBYTE8_REG        (HDMI_CPI_MISC_IF_OFFSET\
					+  MISC_INFOFRAME_DBYTE8_SUBOFFSET)
#define HDMI_CPI_MISC_DBYTE9_REG        (HDMI_CPI_MISC_IF_OFFSET\
					+  MISC_INFOFRAME_DBYTE9_SUBOFFSET)
#define HDMI_CPI_MISC_DBYTE10_REG       (HDMI_CPI_MISC_IF_OFFSET\
					+  MISC_INFOFRAME_DBYTE10_SUBOFFSET)

/* Audio  */
#define HDMI_TPI_I2S_ENABLE_MAPPING_REG     0x1F
#define HDMI_TPI_I2S_INPUT_CONFIG_REG       0x20
#define HDMI_TPI_I2S_STRM_HDR_BASE          0x21
#define HDMI_TPI_I2S_STRM_HDR_0_REG         (HDMI_TPI_I2S_STRM_HDR_BASE + 0)
#define HDMI_TPI_I2S_STRM_HDR_1_REG         (HDMI_TPI_I2S_STRM_HDR_BASE + 1)
#define HDMI_TPI_I2S_STRM_HDR_2_REG         (HDMI_TPI_I2S_STRM_HDR_BASE + 2)
#define HDMI_TPI_I2S_STRM_HDR_3_REG         (HDMI_TPI_I2S_STRM_HDR_BASE + 3)
#define HDMI_TPI_I2S_STRM_HDR_4_REG         (HDMI_TPI_I2S_STRM_HDR_BASE + 4)
#define HDMI_TPI_AUDIO_CONFIG_BYTE2_REG     0x26
#define HDMI_TPI_AUDIO_CONFIG_BYTE3_REG     0x27
#define HDMI_TPI_AUDIO_CONFIG_BYTE4_REG     0x28

/* HDCP */
#define HDMI_TPI_HDCP_QUERYDATA_REG         0x29
#define HDMI_TPI_HDCP_CONTROLDATA_REG       0x2A


/*                   */
/*  HDMI TPI Values  */
/*                   */
/*                      */
/* HDMI_TPI_DEVICE_ID_REG  */
#define TPI_DEVICE_ID                       0xB0

/* HDMI_TPI_REVISION_REG  */
#define TPI_REVISION                        0x00

/* HDMI_TPI_ID_BYTE2_REG  */
#define TPI_ID_BYTE2_VALUE                  0x00

/* HDMI_SYS_CTRL_DATA_REG  */
#define TPI_SYS_CTRL_POWER_DOWN             (1 << 4)
#define TPI_SYS_CTRL_POWER_ACTIVE           (0 << 4)
#define TPI_SYS_CTRL_AV_MUTE                (1 << 3)
#define TPI_SYS_CTRL_DDC_BUS_REQUEST        (1 << 2)
#define TPI_SYS_CTRL_DDC_BUS_GRANTED        (1 << 1)
#define TPI_SYS_CTRL_OUTPUT_MODE_HDMI       (1 << 0)
#define TPI_SYS_CTRL_OUTPUT_MODE_DVI        (0 << 0)


/* HDMI Monitor I2C default address  */
#define HDMI_I2C_MONITOR_ADDRESS            0x50


/* HDMI_TPI_INTR_ENABLE  */
#define TPI_INTR_ENABLE_SECURITY_EVENT      (1 << 5)
#define TPI_INTR_ENABLE_AUDIO_EVENT         (1 << 4)
#define TPI_INTR_ENABLE_CPI_EVENT           (1 << 3)
#define TPI_INTR_ENABLE_RECEIVER_EVENT      (1 << 1)
#define TPI_INTR_ENABLE_HOTPLUG_EVENT       (1 << 0)

/* HDMI_TPI_INTR_STATUS  */
#define TPI_INTR_STATUS_SECURITY_EVENT      (1 << 5)
#define TPI_INTR_STATUS_AUDIO_EVENT         (1 << 4)
#define TPI_INTR_STATUS_POWERED_EVENT       (1 << 3)
#define TPI_INTR_STATUS_HOTPLUG_STATE       (1 << 2)
#define TPI_INTR_STATUS_RECEIVER_EVENT      (1 << 1)
#define TPI_INTR_STATUS_HOTPLUG_EVENT       (1 << 0)


/* HDMI_TPI_PIXEL_REPETITION  */
#define TPI_AVI_PIXEL_REP_BUS_24BIT         (1 << 5)
#define TPI_AVI_PIXEL_REP_BUS_12BIT         (0 << 5)
#define TPI_AVI_PIXEL_REP_RISING_EDGE       (1 << 4)
#define TPI_AVI_PIXEL_REP_FALLING_EDGE      (0 << 4)
#define TPI_AVI_PIXEL_REP_4X                (3 << 0)
#define TPI_AVI_PIXEL_REP_2X                (1 << 0)
#define TPI_AVI_PIXEL_REP_NONE              (0 << 0)

/* HDMI_TPI_AVI_INPUT_FORMAT  */
#define TPI_AVI_INPUT_BITMODE_12BIT         (1 << 7)
#define TPI_AVI_INPUT_BITMODE_8BIT          (0 << 7)
#define TPI_AVI_INPUT_DITHER                (1 << 6)
#define TPI_AVI_INPUT_RANGE_LIMITED         (2 << 2)
#define TPI_AVI_INPUT_RANGE_FULL            (1 << 2)
#define TPI_AVI_INPUT_RANGE_AUTO            (0 << 2)
#define TPI_AVI_INPUT_COLORSPACE_BLACK      (3 << 0)
#define TPI_AVI_INPUT_COLORSPACE_YUV422     (2 << 0)
#define TPI_AVI_INPUT_COLORSPACE_YUV444     (1 << 0)
#define TPI_AVI_INPUT_COLORSPACE_RGB        (0 << 0)


/* HDMI_TPI_AVI_OUTPUT_FORMAT  */
#define TPI_AVI_OUTPUT_CONV_BT709           (1 << 4)
#define TPI_AVI_OUTPUT_CONV_BT601           (0 << 4)
#define TPI_AVI_OUTPUT_RANGE_LIMITED        (2 << 2)
#define TPI_AVI_OUTPUT_RANGE_FULL           (1 << 2)
#define TPI_AVI_OUTPUT_RANGE_AUTO           (0 << 2)
#define TPI_AVI_OUTPUT_COLORSPACE_RGBDVI    (3 << 0)
#define TPI_AVI_OUTPUT_COLORSPACE_YUV422    (2 << 0)
#define TPI_AVI_OUTPUT_COLORSPACE_YUV444    (1 << 0)
#define TPI_AVI_OUTPUT_COLORSPACE_RGBHDMI   (0 << 0)


/* HDMI_TPI_DEVICE_POWER_STATE  */
#define TPI_AVI_POWER_STATE_D3              (3 << 0)
#define TPI_AVI_POWER_STATE_D2              (2 << 0)
#define TPI_AVI_POWER_STATE_D0              (0 << 0)

/* HDMI_TPI_AUDIO_CONFIG_BYTE2_REG  */
#define TPI_AUDIO_CODING_STREAM_HEADER      (0 << 0)
#define TPI_AUDIO_CODING_PCM                (1 << 0)
#define TPI_AUDIO_CODING_AC3                (2 << 0)
#define TPI_AUDIO_CODING_MPEG1              (3 << 0)
#define TPI_AUDIO_CODING_MP3                (4 << 0)
#define TPI_AUDIO_CODING_MPEG2              (5 << 0)
#define TPI_AUDIO_CODING_AAC                (6 << 0)
#define TPI_AUDIO_CODING_DTS                (7 << 0)
#define TPI_AUDIO_CODING_ATRAC              (8 << 0)
#define TPI_AUDIO_MUTE_DISABLE              (0 << 4)
#define TPI_AUDIO_MUTE_ENABLE               (1 << 4)
#define TPI_AUDIO_INTERFACE_DISABLE         (0 << 6)
#define TPI_AUDIO_INTERFACE_SPDIF           (1 << 6)
#define TPI_AUDIO_INTERFACE_I2S             (2 << 6)

/* HDMI_TPI_AUDIO_CONFIG_BYTE3_REG  */
#define TPI_AUDIO_CHANNEL_STREAM            (0 << 0)
#define TPI_AUDIO_2_CHANNEL                 (1 << 0)
#define TPI_AUDIO_8_CHANNEL                 (7 << 0)
#define TPI_AUDIO_FREQ_STREAM               (0 << 3)
#define TPI_AUDIO_FREQ_32KHZ                (1 << 3)
#define TPI_AUDIO_FREQ_44KHZ                (2 << 3)
#define TPI_AUDIO_FREQ_48KHZ                (3 << 3)
#define TPI_AUDIO_FREQ_88KHZ                (4 << 3)
#define TPI_AUDIO_FREQ_96KHZ                (5 << 3)
#define TPI_AUDIO_FREQ_176KHZ               (6 << 3)
#define TPI_AUDIO_FREQ_192KHZ               (7 << 3)
#define TPI_AUDIO_SAMPLE_SIZE_STREAM        (0 << 6)
#define TPI_AUDIO_SAMPLE_SIZE_16            (1 << 6)
#define TPI_AUDIO_SAMPLE_SIZE_20            (2 << 6)
#define TPI_AUDIO_SAMPLE_SIZE_24            (3 << 6)

/* HDMI_TPI_I2S_ENABLE_MAPPING_REG  */
#define TPI_I2S_SD_CONFIG_SELECT_SD0        (0 << 0)
#define TPI_I2S_SD_CONFIG_SELECT_SD1        (1 << 0)
#define TPI_I2S_SD_CONFIG_SELECT_SD2        (2 << 0)
#define TPI_I2S_SD_CONFIG_SELECT_SD3        (3 << 0)
#define TPI_I2S_LF_RT_SWAP_NO               (0 << 2)
#define TPI_I2S_LF_RT_SWAP_YES              (1 << 2)
#define TPI_I2S_DOWNSAMPLE_DISABLE          (0 << 3)
#define TPI_I2S_DOWNSAMPLE_ENABLE           (1 << 3)
#define TPI_I2S_SD_FIFO_0                   (0 << 4)
#define TPI_I2S_SD_FIFO_1                   (1 << 4)
#define TPI_I2S_SD_FIFO_2                   (2 << 4)
#define TPI_I2S_SD_FIFO_3                   (3 << 4)
#define TPI_I2S_SD_CHANNEL_DISABLE          (0 << 7)
#define TPI_I2S_SD_CHANNEL_ENABLE           (1 << 7)


/* HDMI_TPI_I2S_INPUT_CONFIG_REG  */
#define TPI_I2S_FIRST_BIT_SHIFT_YES         (0 << 0)
#define TPI_I2S_FIRST_BIT_SHIFT_NO          (1 << 0)
#define TPI_I2S_SD_DIRECTION_MSB_FIRST      (0 << 1)
#define TPI_I2S_SD_DIRECTION_LSB_FIRST      (1 << 1)
#define TPI_I2S_SD_JUSTIFY_LEFT             (0 << 2)
#define TPI_I2S_SD_JUSTIFY_RIGHT            (1 << 2)
#define TPI_I2S_WS_POLARITY_LOW             (0 << 3)
#define TPI_I2S_WS_POLARITY_HIGH            (1 << 3)
#define TPI_I2S_MCLK_MULTIPLIER_128         (0 << 4)
#define TPI_I2S_MCLK_MULTIPLIER_256         (1 << 4)
#define TPI_I2S_MCLK_MULTIPLIER_384         (2 << 4)
#define TPI_I2S_MCLK_MULTIPLIER_512         (3 << 4)
#define TPI_I2S_MCLK_MULTIPLIER_768         (4 << 4)
#define TPI_I2S_MCLK_MULTIPLIER_1024        (5 << 4)
#define TPI_I2S_MCLK_MULTIPLIER_1152        (6 << 4)
#define TPI_I2S_MCLK_MULTIPLIER_192         (7 << 4)
#define TPI_I2S_SCK_EDGE_FALLING            (0 << 7)
#define TPI_I2S_SCK_EDGE_RISING             (1 << 7)

/* HDMI_TPI_I2S_STRM_HDR_REG                       */
/* the reference of this values is in IEC 60958-3  */
#define I2S_CHAN_STATUS_MODE                0x00
#define I2S_CHAN_STATUS_CAT_CODE            0x00
#define I2S_CHAN_SOURCE_CHANNEL_NUM         0x00
#define I2S_CHAN_ACCURACY_N_44_SAMPLING_FS  0x20
#define I2S_CHAN_ACCURACY_N_48_SAMPLING_FS  0x22
#define I2S_CHAN_ORIGIN_FS_N_SAMP_LENGTH    0xD2


/* MISCELLANOUS INFOFRAME VALUES  */

#define HDMI_INFOFRAME_TX_ENABLE            (0x1 << 7)
#define HDMI_INFOFRAME_TX_REPEAT            (0x1 << 6)
#define HDMI_AUDIO_INFOFRAME                (0x2 << 0)

/* Stream Header Data  */
#define HDMI_SH_PCM                         (0x1 << 4)
#define HDMI_SH_TWO_CHANNELS                (0x1 << 0)
#define HDMI_SH_44KHz                       (0x2 << 2)
#define HDMI_SH_48KHz                       (0x3 << 2)
#define HDMI_SH_16BIT                       (0x1 << 0)
#define HDMI_SH_SPKR_FLFR                   0x0
#define HDMI_SH_0dB_ATUN                    0x0

/* MISC_TYPE  */
#define MISC_INFOFRAME_TYPE                 0x04  /* for Audio */
#define MISC_INFOFRAME_ALWAYS_SET           0x80

/* MISC_VERSION  */
#define MISC_INFOFRAME_VERSION              0x01

/* MISC_LENGTH  */
#define MISC_INFOFRAME_LENGTH               0x0A /*length for Audio infoframe*/
#define MISC_INFOFRAME_LENGTH_RSVD_BITS     0xE0

/* MISC_DBYTE1                */
/* Type, Encoding, Trandport  */
#define MISC_DBYTE1_CT_CHK_HEADER_STREAM    0x00

/* audio channel count  */
#define MISC_DBYTE1_CC_CHK_HEADER_STREAM    0x00
#define MISC_DBYTE1_CC_2_CHANNELS           0x01

/* MISC_DBYTE2  */
/*Sample Size   */
#define MISC_DBYTE2_SS_CHK_HEADER_STREAM    0x00  /* for hdmi by default */

/* Sampling Frequency  */
#define MISC_DBYTE2_SF_CHK_HEADER_STREAM    0x00  /* for hdmi by default */

/* MISC_DBYTE3     */
/* Code Extention  */
#define MISC_DBYTE3_CTX_TAKE_DBYTE1         0x00  /* for hdmi by default */

/* MISC_DBYTE4  */
#define MISC_DBYTE4                         0x00 /*for no multichannel( */
						 /* multichannel means more*/
						/* than 2 channels */

/* MISC_DBYTE5  */
#define MISC_DBYTE5           0x00  /* for no multichannel(multichannel  */
					/* means more than 2 channels */
/*---------------------------------------------------------------------  */

#ifdef __cplusplus
extern "C" {
#endif


enum SI9022_INTTYPE_T {
	SI9022_HOTPLUG_PENDING     = 0x01,
	SI9022_RCV_SENSE_PENDING   = 0x02,
	SI9022_HOTPLUG_SENSE       = 0x04,
	SI9022_RX_SENSE            = 0x08,
	SI9022_AUDIO_ERR_PENDING   = 0x10
};

enum SI9022_PLLMULT_T {
	SI9022_PLLMULT_05    =  0x00,
	SI9022_PLLMULT_1     =  0x01,
	SI9022_PLLMULT_2     =  0x02,
	SI9022_PLLMULT_4     =  0x03
};

struct SI9022_EDIDRAW_T {
	u8 header[8];
	u8 vendor_product_id[10];
	u8 edid_version[2];
	u8 basic_display_info[5];
	u8 color_info[10];
	u8 established_timings[3];
	u8 standard_timing_id[16];
	u8 detailed_timing_desc[4][18];
	u8 ext_block_num[1];
	u8 checksum[1];
};

struct SI9022_EDIDDETAILEDTIMING_T {
	u32 pixel_clk;
	u32 hactive_pixels;
	u32 hblanking_pixels;
	u32 vactive_lines;
	u32 vblanking_lines;
	u32 hsync_offset;
	u32 hsync_width;
	u32 vsync_offset;
	u32 vsync_width;
	u32 image_hsize;
	u32 image_vsize;
	u32 image_hborder;
	u32 image_vborder;
	u8 is_interlaced;
	u8 is_analog_composite;
	u8 is_bipolar_analog_composite;
	u8 is_digital_composite;
	u8 is_digital_separate;
};

/*  EDID - Extended Display ID Data structs  */


/*  Video Descriptor Block  */
struct HDMI_EDID_DTD_VIDEO {
	u8   pixel_clock[2];          /* 54-55 */
	u8   horiz_active;            /* 56 */
	u8   horiz_blanking;          /* 57 */
	u8   horiz_high;              /* 58 */
	u8   vert_active;             /* 59 */
	u8   vert_blanking;           /* 60 */
	u8   vert_high;               /* 61 */
	u8   horiz_sync_offset;       /* 62 */
	u8   horiz_sync_pulse;        /* 63 */
	u8   vert_sync_pulse;         /* 64 */
	u8   sync_pulse_high;         /* 65 */
	u8   horiz_image_size;        /* 66 */
	u8   vert_image_size;	      /* 67 */
	u8   image_size_high;         /* 68 */
	u8   horiz_border;            /* 69 */
	u8   vert_border;             /* 70 */
	u8   misc_settings;           /* 71 */
};


/*  Monitor Limits Descriptor Block  */
struct HDMI_EDID_DTD_MONITOR {
	u8   pixel_clock[2];          /* 54-55*/
	u8   _reserved1;              /* 56 */
	u8   block_type;              /* 57 */
	u8   _reserved2;              /* 58 */
	u8   min_vert_freq;           /* 59 */
	u8   max_vert_freq;           /* 60 */
	u8   min_horiz_freq;          /* 61 */
	u8   max_horiz_freq;          /* 62 */
	u8   pixel_clock_mhz;         /* 63 */

	u8   GTF[2];                  /* 64 -65 */
	u8   start_horiz_freq;        /* 66  */
	u8   C;	                      /* 67 */
	u8   M[2];                    /* 68-69 */
	u8   K;                       /* 70 */
	u8   J;                       /* 71 */
};


/*  Text Descriptor Block  */
struct HDMI_EDID_DTD_TEXT {
	u8   pixel_clock[2];          /* 54-55 */
	u8   _reserved1;              /* 56 */
	u8   block_type;              /* 57 */
	u8   _reserved2;              /* 58 */

	u8   text[13];                /* 59-71 */
};


/*  DTD Union  */
union HDMI_EDID_DTD {
	struct HDMI_EDID_DTD_VIDEO	video;
	struct HDMI_EDID_DTD_TEXT	monitor_name;
	struct HDMI_EDID_DTD_TEXT	monitor_serial_number;
	struct HDMI_EDID_DTD_MONITOR	monitor_limits;
};


/*  EDID struct  */
struct HDMI_EDID {
	u8   header[8];              /* 00-07 */
	u8   manufacturerID[2];      /* 08-09 */
	u8   product_id[2];          /* 10-11 */
	u8   serial_number[4];       /* 12-15 */
	u8   week_manufactured;      /* 16 */
	u8   year_manufactured;      /* 17 */
	u8   edid_version;           /* 18 */
	u8   edid_revision;          /* 19 */

	u8   video_in_definition;    /* 20 */
	u8   max_horiz_image_size;   /* 21 */
	u8   max_vert_image_size;    /* 22 */
	u8   display_gamma;          /* 23 */
	u8   power_features;         /* 24 */
	u8   chroma_info[10];        /* 25-34 */
	u8   timing_1;               /* 35 */
	u8   timing_2;               /* 36 */
	u8   timing_3;               /* 37 */
	u8   std_timings[16];        /* 38-53 */

	union HDMI_EDID_DTD   DTD[4];         /* 72-125 */

	u8   extension_edid;         /* 126 */
	u8   checksum;               /* 127 */

	u8   extension_tag;          /* 00 (extensions follow EDID) */
	u8   extention_rev;          /* 01 */
	u8   offset_dtd;             /* 02 */
	u8   num_dtd;                /* 03 */

	u8   data_block[123];        /* 04 - 126 */
	u8   extension_checksum;     /* 127 */
};

struct hdmi_reg_data {
	u8 reg_offset;
	u8 value;
};

struct hdmi_platform_data {
	void (*set_min_bus_tput)(struct device *dev,
				 u8 agent_id,
				 unsigned long r);
	void (*set_max_mpu_wakeup_lat)(struct device *dev, long t);
};

extern void config_hdmi_gpio(void);
extern void zoom_hdmi_reset_enable(int);
#endif

