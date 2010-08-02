/*
 *  FM Driver for Connectivity chip of Texas Instruments.
 *  FM Common module header file
 *
 *  Copyright (C) 2010 Texas Instruments
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef _FMDRV_COMMON_H
#define _FMDRV_COMMON_H

#define FM_ST_REGISTER_TIMEOUT   msecs_to_jiffies(6000)	/* 6 sec */
#define FM_PKT_LOGICAL_CHAN_NUMBER  0x08   /* Logical channel 8 */

#define REG_RD       0x1
#define REG_WR      0x0

struct fm_reg_table {
	unsigned char opcode;
	unsigned char type;
	char *name;
};

/* FM register index */
enum fm_reg_index {
	/* FM RX registers */
	STEREO_GET,
	RSSI_LVL_GET,
	IF_COUNT_GET,
	FLAG_GET,
	RDS_SYNC_GET,
	RDS_DATA_GET,
	FREQ_SET,
	FREQ_GET,
	AF_FREQ_SET,
	AF_FREQ_GET,
	MOST_MODE_SET,
	MOST_MODE_GET,
	MOST_BLEND_SET,
	MOST_BLEND_GET,
	DEMPH_MODE_SET,
	DEMPH_MODE_GET,
	SEARCH_LVL_SET,
	SEARCH_LVL_GET,
	RX_BAND_SET,
	RX_BAND_GET,
	MUTE_STATUS_SET,
	MUTE_STATUS_GET,
	RDS_PAUSE_LVL_SET,
	RDS_PAUSE_LVL_GET,
	RDS_PAUSE_DUR_SET,
	RDS_PAUSE_DUR_GET,
	RDS_MEM_SET,
	RDS_MEM_GET,
	RDS_BLK_B_SET,
	RDS_BLK_B_GET,
	RDS_MSK_B_SET,
	RDS_MSK_B_GET,
	RDS_PI_MASK_SET,
	RDS_PI_MASK_GET,
	RDS_PI_SET,
	RDS_PI_GET,
	RDS_SYSTEM_SET,
	RDS_SYSTEM_GET,
	INT_MASK_SET,
	INT_MASK_GET,
	SEARCH_DIR_SET,
	SEARCH_DIR_GET,
	VOLUME_SET,
	VOLUME_GET,
	AUDIO_ENABLE_SET,
	AUDIO_ENABLE_GET,
	PCM_MODE_SET,
	PCM_MODE_GET,
	I2S_MODE_CONFIG_SET,
	I2S_MODE_CONFIG_GET,
	POWER_SET,
	POWER_GET,
	INTx_CONFIG_SET,
	INTx_CONFIG_GET,
	PULL_EN_SET,
	PULL_EN_GET,
	HILO_SET,
	HILO_GET,
	SWITCH2FREF,
	FREQ_DRIFT_REPORT,
	PCE_GET,
	FIRM_VER_GET,
	ASIC_VER_GET,
	ASIC_ID_GET,
	MAIN_ID_GET,
	TUNER_MODE_SET,
	STOP_SEARCH,
	RDS_CNTRL_SET,
	WRITE_HARDWARE_REG,
	CODE_DOWNLOAD,
	RESET,
	FM_POWER_MODE,
	FM_INTERRUPT,

	/* FM TX registers */
	CHANL_SET,
	CHANL_GET,
	CHANL_BW_SET,
	CHANL_BW_GET,
	REF_SET,
	REF_GET,
	POWER_ENB_SET,
	POWER_ATT_SET,
	POWER_ATT_GET,
	POWER_LEL_SET,
	POWER_LEL_GET,
	AUDIO_DEV_SET,
	AUDIO_DEV_GET,
	PILOT_DEV_SET,
	PILOT_DEV_GET,
	RDS_DEV_SET,
	RDS_DEV_GET,
	PUPD_SET,
	AUDIO_IO_SET,
	PREMPH_SET,
	PREMPH_GET,
	TX_BAND_SET,
	TX_BAND_GET,
	MONO_SET,
	MONO_GET,
	MUTE,
	MPX_LMT_ENABLE,
	LOCK_GET,
	REF_ERR_SET,
	PI_SET,
	PI_GET,
	TYPE_SET,
	TYPE_GET,
	PTY_SET,
	PTY_GET,
	AF_SET,
	AF_GET,
	DISPLAY_SIZE_SET,
	DISPLAY_SIZE_GET,
	RDS_MODE_SET,
	RDS_MODE_GET,
	DISPLAY_MODE_SET,
	DISPLAY_MODE_GET,
	LENGHT_SET,
	LENGHT_GET,
	TOGGLE_AB_SET,
	TOGGLE_AB_GET,
	RDS_REP_SET,
	RDS_REP_GET,
	RDS_DATA_SET,
	RDS_DATA_ENB,
	TA_SET,
	TA_GET,
	TP_SET,
	TP_GET,
	DI_SET,
	DI_GET,
	MS_SET,
	MS_GET,
	PS_SCROLL_SPEED_SET,
	PS_SCROLL_SPEED_GET,

	FM_REG_MAX_ENTRIES
};

/* SKB helpers */
struct fm_skb_cb {
	__u8 fm_opcode;
	struct completion *completion;
};

#define fm_cb(skb) ((struct fm_skb_cb *)(skb->cb))

/* FM Channel-8 command message format */
struct fm_cmd_msg_hdr {
	__u8 header;		/* Logical Channel-8 */
	__u8 len;		/* Number of bytes follows */
	__u8 fm_opcode;		/* FM Opcode */
	__u8 rd_wr;		/* Read/Write command */
	__u8 dlen;		/* Length of payload */
} __attribute__ ((packed));

#define FM_CMD_MSG_HDR_SIZE    5	/* sizeof(struct fm_cmd_msg_hdr) */

/* FM Channel-8 event messgage format */
struct fm_event_msg_hdr {
	__u8 header;		/* Logical Channel-8 */
	__u8 len;		/* Number of bytes follows */
	__u8 status;		/* Event status */
	__u8 num_fm_hci_cmds;	/* Number of pkts the host allowed to send */
	__u8 fm_opcode;		/* FM Opcode */
	__u8 rd_wr;		/* Read/Write command */
	__u8 dlen;		/* Length of payload */
} __attribute__ ((packed));

#define FM_EVT_MSG_HDR_SIZE     7	/* sizeof(struct fm_event_msg_hdr) */

/* TI's magic number in firmware file */
#define FM_FW_FILE_HEADER_MAGIC	     0x42535442

/* Firmware header */
struct bts_header {
	uint32_t magic;
	uint32_t version;
	uint8_t future[24];
	uint8_t actions[0];
} __attribute__ ((packed));

/* Firmware action */
struct bts_action {
	uint16_t type;
	uint16_t size;
	uint8_t data[0];
} __attribute__ ((packed));

/* Firmware delay */
struct bts_action_delay {
	uint32_t msec;
} __attribute__ ((packed));

#define ACTION_SEND_COMMAND	1
#define ACTION_WAIT_EVENT	2
#define ACTION_SERIAL		3
#define ACTION_DELAY		4
#define ACTION_REMARKS		6

/* Converts little endian to big endian */
#define FM_STORE_LE16_TO_BE16(data, value)   \
	(data = ((value >> 8) | ((value & 0xFF) << 8)))
#define FM_LE16_TO_BE16(value)   (((value >> 8) | ((value & 0xFF) << 8)))

/* Converts big endian to little endian */
#define FM_STORE_BE16_TO_LE16(data, value)   \
	(data = ((value & 0xFF) << 8) | ((value >> 8)))
#define FM_BE16_TO_LE16(value)   (((value & 0xFF) << 8) | ((value >> 8)))

#define FM_ENABLE   1
#define FM_DISABLE  0

/* FLAG_GET register bits */
#define FM_FR_EVENT		(1 << 0)
#define FM_BL_EVENT		(1 << 1)
#define FM_RDS_EVENT		(1 << 2)
#define FM_BBLK_EVENT		(1 << 3)
#define FM_LSYNC_EVENT		(1 << 4)
#define FM_LEV_EVENT		(1 << 5)
#define FM_IFFR_EVENT		(1 << 6)
#define FM_PI_EVENT		(1 << 7)
#define FM_PD_EVENT		(1 << 8)
#define FM_STIC_EVENT		(1 << 9)
#define FM_MAL_EVENT		(1 << 10)
#define FM_POW_ENB_EVENT	(1 << 11)

/* Firmware files of the FM, ASIC ID and
 * ASIC version will be appened to this later
 */
#define FM_FMC_FW_FILE_START      ("fmc_ch8")
#define FM_RX_FW_FILE_START       ("fm_rx_ch8")
#define FM_TX_FW_FILE_START       ("fm_tx_ch8")

#define FM_CHECK_SEND_CMD_STATUS(ret)  \
	if (ret < 0) {\
		return ret;\
	}
#define FM_UNDEFINED_FREQ		   0xFFFFFFFF

/* Band types */
#define FM_BAND_EUROPE_US	0
#define FM_BAND_JAPAN		1

/* Seek directions */
#define FM_SEARCH_DIRECTION_DOWN	0
#define FM_SEARCH_DIRECTION_UP		1

/* Tunner modes */
#define FM_TUNER_STOP_SEARCH_MODE	0
#define FM_TUNER_PRESET_MODE		1
#define FM_TUNER_AUTONOMOUS_SEARCH_MODE	2
#define FM_TUNER_AF_JUMP_MODE		3

/* Min and Max volume */
#define FM_RX_VOLUME_MIN	0
#define FM_RX_VOLUME_MAX	70

/* Volume gain step */
#define FM_RX_VOLUME_GAIN_STEP	0x370

/* Mute modes */
#define FM_MUTE_ON		0
#define	FM_MUTE_OFF		1
#define	FM_MUTE_ATTENUATE	2

#define FM_RX_MUTE_UNMUTE_MODE		0x00
#define FM_RX_MUTE_RF_DEP_MODE		0x01
#define FM_RX_MUTE_AC_MUTE_MODE		0x02
#define FM_RX_MUTE_HARD_MUTE_LEFT_MODE	0x04
#define FM_RX_MUTE_HARD_MUTE_RIGHT_MODE	0x08
#define FM_RX_MUTE_SOFT_MUTE_FORCE_MODE	0x10

/* RF dependent mute mode */
#define FM_RX_RF_DEPENDENT_MUTE_ON	1
#define FM_RX_RF_DEPENDENT_MUTE_OFF	0

/* RSSI threshold min and max */
#define FM_RX_RSSI_THRESHOLD_MIN	-128
#define FM_RX_RSSI_THRESHOLD_MAX	127

/* Stereo/Mono mode */
#define FM_STEREO_MODE		0
#define FM_MONO_MODE		1
#define FM_STEREO_SOFT_BLEND	1

/* FM RX De-emphasis filter modes */
#define FM_RX_EMPHASIS_FILTER_50_USEC	0
#define FM_RX_EMPHASIS_FILTER_75_USEC	1

/* FM RDS modes */
#define FM_RDS_DISABLE	0
#define FM_RDS_ENABLE	1

#define FM_NO_PI_CODE	0

/* FM and RX RDS block enable/disable  */
#define FM_RX_POWER_SET_FM_ON_RDS_OFF		0x1
#define FM_RX_POWET_SET_FM_AND_RDS_BLK_ON	0x3
#define FM_RX_POWET_SET_FM_AND_RDS_BLK_OFF	0x0

/* RX RDS */
#define FM_RX_RDS_FLUSH_FIFO		0x1
#define FM_RX_RDS_FIFO_THRESHOLD	64	/* tuples */
#define FM_RDS_BLOCK_SIZE		3	/* 3 bytes */

/* RDS block types */
#define FM_RDS_BLOCK_A		0
#define FM_RDS_BLOCK_B		1
#define FM_RDS_BLOCK_C		2
#define FM_RDS_BLOCK_Ctag	3
#define FM_RDS_BLOCK_D		4
#define FM_RDS_BLOCK_E		5

#define FM_RDS_BLOCK_INDEX_A		0
#define FM_RDS_BLOCK_INDEX_B		1
#define FM_RDS_BLOCK_INDEX_C		2
#define FM_RDS_BLOCK_INDEX_D		3
#define FM_RDS_BLOCK_INDEX_UNKNOWN	0xF0

#define FM_RDS_STATUS_ERROR_MASK	0x18

/* Represents an RDS group type & version.
 * There are 15 groups, each group has 2
 * versions: A and B.
 */
#define FM_RDS_GROUP_TYPE_MASK_0A	    ((unsigned long)1<<0)
#define FM_RDS_GROUP_TYPE_MASK_0B	    ((unsigned long)1<<1)
#define FM_RDS_GROUP_TYPE_MASK_1A	    ((unsigned long)1<<2)
#define FM_RDS_GROUP_TYPE_MASK_1B	    ((unsigned long)1<<3)
#define FM_RDS_GROUP_TYPE_MASK_2A	    ((unsigned long)1<<4)
#define FM_RDS_GROUP_TYPE_MASK_2B	    ((unsigned long)1<<5)
#define FM_RDS_GROUP_TYPE_MASK_3A	    ((unsigned long)1<<6)
#define FM_RDS_GROUP_TYPE_MASK_3B           ((unsigned long)1<<7)
#define FM_RDS_GROUP_TYPE_MASK_4A	    ((unsigned long)1<<8)
#define FM_RDS_GROUP_TYPE_MASK_4B	    ((unsigned long)1<<9)
#define FM_RDS_GROUP_TYPE_MASK_5A	    ((unsigned long)1<<10)
#define FM_RDS_GROUP_TYPE_MASK_5B	    ((unsigned long)1<<11)
#define FM_RDS_GROUP_TYPE_MASK_6A	    ((unsigned long)1<<12)
#define FM_RDS_GROUP_TYPE_MASK_6B	    ((unsigned long)1<<13)
#define FM_RDS_GROUP_TYPE_MASK_7A	    ((unsigned long)1<<14)
#define FM_RDS_GROUP_TYPE_MASK_7B	    ((unsigned long)1<<15)
#define FM_RDS_GROUP_TYPE_MASK_8A           ((unsigned long)1<<16)
#define FM_RDS_GROUP_TYPE_MASK_8B	    ((unsigned long)1<<17)
#define FM_RDS_GROUP_TYPE_MASK_9A	    ((unsigned long)1<<18)
#define FM_RDS_GROUP_TYPE_MASK_9B	    ((unsigned long)1<<19)
#define FM_RDS_GROUP_TYPE_MASK_10A	    ((unsigned long)1<<20)
#define FM_RDS_GROUP_TYPE_MASK_10B	    ((unsigned long)1<<21)
#define FM_RDS_GROUP_TYPE_MASK_11A	    ((unsigned long)1<<22)
#define FM_RDS_GROUP_TYPE_MASK_11B	    ((unsigned long)1<<23)
#define FM_RDS_GROUP_TYPE_MASK_12A	    ((unsigned long)1<<24)
#define FM_RDS_GROUP_TYPE_MASK_12B	    ((unsigned long)1<<25)
#define FM_RDS_GROUP_TYPE_MASK_13A	    ((unsigned long)1<<26)
#define FM_RDS_GROUP_TYPE_MASK_13B	    ((unsigned long)1<<27)
#define FM_RDS_GROUP_TYPE_MASK_14A	    ((unsigned long)1<<28)
#define FM_RDS_GROUP_TYPE_MASK_14B	    ((unsigned long)1<<29)
#define FM_RDS_GROUP_TYPE_MASK_15A	    ((unsigned long)1<<30)
#define FM_RDS_GROUP_TYPE_MASK_15B	    ((unsigned long)1<<31)

/* RX Alternate Frequency info */
#define FM_RDS_MIN_AF		          1
#define FM_RDS_MAX_AF		        204
#define FM_RDS_MAX_AF_JAPAN	        140
#define FM_RDS_1_AF_FOLLOWS	        225
#define FM_RDS_25_AF_FOLLOWS	        249

/* RDS system type (RDS/RBDS) */
#define FM_RDS_SYSTEM_RDS		0
#define FM_RDS_SYSTEM_RBDS		1

/* AF on/off */
#define FM_RX_RDS_AF_SWITCH_MODE_ON	1
#define FM_RX_RDS_AF_SWITCH_MODE_OFF	0

/* Retry count when interrupt process goes wrong */
#define FM_IRQ_TIMEOUT_RETRY_MAX	5	/* 5 times */

/* Audio IO set values */
#define FM_RX_FM_AUDIO_ENABLE_I2S	0x01
#define FM_RX_FM_AUDIO_ENABLE_ANALOG	0x02
#define FM_RX_FM_AUDIO_ENABLE_I2S_AND_ANALOG	0x03
#define FM_RX_FM_AUDIO_ENABLE_DISABLE	0x00

/* HI/LO set values */
#define FM_RX_IFFREQ_TO_HI_SIDE		0x0
#define FM_RX_IFFREQ_TO_LO_SIDE		0x1
#define FM_RX_IFFREQ_HILO_AUTOMATIC	0x2

/* Default RX mode configuration. Chip will be configured
 * with this default values after loading RX firmware.
 */
#define FM_DEFAULT_RX_VOLUME		10
#define FM_DEFAULT_RSSI_THRESHOLD	3

/* Functions exported by FM common sub-module */
int fmc_prepare(struct fmdrv_ops *);
int fmc_release(struct fmdrv_ops *);

void fmc_update_region_info(struct fmdrv_ops *, unsigned char);
int fmc_send_cmd(struct fmdrv_ops *, unsigned char, void *, int,
			struct completion *, void *, int *);
int fmc_is_rds_data_available(struct fmdrv_ops *, struct file *,
				struct poll_table_struct *);
int fmc_transfer_rds_from_internal_buff(struct fmdrv_ops *, struct file *,
					char __user *, size_t);

int fmc_set_frequency(struct fmdrv_ops *, unsigned int);
int fmc_set_mode(struct fmdrv_ops *, unsigned char);
int fmc_set_region(struct fmdrv_ops *, unsigned char);
int fmc_set_mute_mode(struct fmdrv_ops *, unsigned char);
int fmc_set_stereo_mono(struct fmdrv_ops *, unsigned short);
int fmc_set_rds_mode(struct fmdrv_ops *, unsigned char);

int fmc_get_frequency(struct fmdrv_ops *, unsigned int *);
int fmc_get_region(struct fmdrv_ops *, unsigned char *);
int fmc_get_mode(struct fmdrv_ops *, unsigned char *);

#endif

