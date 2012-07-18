/*
 * This file is part of calibrator
 *
 * Copyright (C) 2011 Texas Instruments
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

#ifndef __WL18XX_PLT_H__
#define __WL18XX_PLT_H__

enum wl18xx_test_cmds {
	WL18XX_TEST_CMD_PD_BUFFER_CAL = 0x1,
	WL18XX_TEST_CMD_P2G_CAL,
	WL18XX_TEST_CMD_RX_PLT_ENTER,
	WL18XX_TEST_CMD_RX_PLT_CAL,
	WL18XX_TEST_CMD_RX_PLT_EXIT,
	WL18XX_TEST_CMD_RX_PLT_GET,
	WL18XX_TEST_CMD_FCC,
	WL18XX_TEST_CMD_TELEC,
	WL18XX_TEST_CMD_STOP_TX,
	WL18XX_TEST_CMD_PLT_TEMPLATE,
	WL18XX_TEST_CMD_PLT_GAIN_ADJUST,
	WL18XX_TEST_CMD_PLT_GAIN_GET,
	WL18XX_TEST_CMD_CHANNEL_TUNE_OLD,
	WL18XX_TEST_CMD_FREE_RUN_RSSI,
	WL18XX_TEST_CMD_DEBUG,
	WL18XX_TEST_CMD_CLPC_COMMANDS,
	WL18XX_TEST_CMD_RESERVED,
	WL18XX_TEST_CMD_RX_STAT_STOP,
	WL18XX_TEST_CMD_RX_STAT_START,
	WL18XX_TEST_CMD_RX_STAT_RESET,
	WL18XX_TEST_CMD_RX_STAT_GET,
	WL18XX_TEST_CMD_LOOPBACK_START,
	WL18XX_TEST_CMD_LOOPBACK_STOP,
	WL18XX_TEST_CMD_GET_FW_VERSIONS,
	WL18XX_TEST_CMD_INI_FILE_RADIO_PARAM,
	WL18XX_TEST_CMD_RUN_CALIBRATION_TYPE,
	WL18XX_TEST_CMD_TX_GAIN_ADJUST,
	WL18XX_TEST_CMD_UPDATE_PD_BUFFER_ERRORS,
	WL18XX_TEST_CMD_UPDATE_PD_REFERENCE_POINT,
	WL18XX_TEST_CMD_INI_FILE_GENERAL_PARAM,
	WL18XX_TEST_CMD_SET_EFUSE,
	WL18XX_TEST_CMD_GET_EFUSE,
	WL18XX_TEST_CMD_TEST_TONE,
	WL18XX_TEST_CMD_POWER_MODE,
	WL18XX_TEST_CMD_SMART_REFLEX,
	WL18XX_TEST_CMD_CHANNEL_RESPONSE,
	WL18XX_TEST_CMD_DCO_ITRIM_FEATURE,
	WL18XX_TEST_CMD_START_TX_SIMULATION,
	WL18XX_TEST_CMD_STOP_TX_SIMULATION,
	WL18XX_TEST_CMD_START_RX_SIMULATION,
	WL18XX_TEST_CMD_STOP_RX_SIMULATION,
	WL18XX_TEST_CMD_GET_RX_STATISTICS,
	WL18XX_TEST_CMD_SET_NVS_VERSION,
	WL18XX_TEST_CMD_CHANNEL_TUNE,
};

struct wl18xx_cmd_channel_tune {
	struct wl1271_cmd_header header;
	struct wl1271_cmd_test_header test;

	__le16	radio_status;
	__u8	channel;
	__u8	band;
	__u8	bandwidth;
	__u8	padding[3];
} __attribute__((packed));

struct wl18xx_cmd_start_rx {
	struct wl1271_cmd_header header;
	struct wl1271_cmd_test_header test;
} __attribute__((packed));

struct wl18xx_cmd_stop_rx {
	struct wl1271_cmd_header header;
	struct wl1271_cmd_test_header test;
} __attribute__((packed));

struct wl18xx_cmd_rx_stats {
	struct wl1271_cmd_header header;
	struct wl1271_cmd_test_header test;

	__le32 radio_status;

	__le32 total;
	__le32 errors;
	__le32 addr_mm;
	__le32 good;
} __attribute__((packed));

struct wl18xx_cmd_start_tx {
	struct wl1271_cmd_header header;
	struct wl1271_cmd_test_header test;

	__le32 radio_status;

	__le32 delay;
	__le32 rate;
	__le32 size;
	__le32 mode;
	__le32 data_type;
	__le32 gi;
	__le32 options1;
	__le32 options2;
	__u8   src_addr[MAC_ADDR_LEN];
	__u8   dst_addr[MAC_ADDR_LEN];
	__le32 bandwidth;
	__le32 padding;
} __attribute__((packed));

struct wl18xx_cmd_stop_tx {
	struct wl1271_cmd_header header;
	struct wl1271_cmd_test_header test;
} __attribute__((packed));

#endif /* __WL18XX_PLT_H__ */
