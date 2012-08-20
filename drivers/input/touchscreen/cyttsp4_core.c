/*
 * Core Source for:
 * Cypress TrueTouch(TM) Standard Product (TTSP) touchscreen drivers.
 * For use with Cypress Gen4 and Solo parts.
 * Supported parts include:
 * CY8CTMA398
 * CY8CTMA884
 *
 * Copyright (C) 2009-2011 Cypress Semiconductor, Inc.
 * Copyright (C) 2011 Motorola Mobility, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, and only version 2, as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contact Cypress Semiconductor at www.cypress.com <kev@cypress.com>
 *
 */
#include "cyttsp4_core.h"

#include <linux/delay.h>
#include <linux/input.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/input/touch_platform.h>
#include <linux/firmware.h>	/* This enables firmware class loader code */
#include <linux/input/cyttsp4_params.h>

/* platform address lookup offsets */
#define CY_TCH_ADDR_OFS		0
#define CY_LDR_ADDR_OFS		1

/* helpers */
#define GET_NUM_TOUCHES(x)          ((x) & 0x1F)
#define IS_LARGE_AREA(x)            (((x) & 0x20) >> 5)
#define IS_BAD_PKT(x)               ((x) & 0x20)
#define IS_VALID_APP(x)             ((x) & 0x01)
#define IS_OPERATIONAL_ERR(x)       ((x) & 0x3F)
#define GET_HSTMODE(reg)            ((reg & 0x70) >> 4)
#define GET_BOOTLOADERMODE(reg)     ((reg & 0x10) >> 4)

/* maximum number of concurrent tracks */
#define CY_NUM_TCH_ID               10
/* maximum number of track IDs */
#define CY_NUM_TRK_ID               16
/* maximum number of command data bytes */
#define CY_NUM_DAT                  6
/* maximum number of config block read data */
#define CY_NUM_CONFIG_BYTES        128

#define CY_REG_BASE                 0x00
#define CY_DELAY_DFLT               20 /* ms */
#define CY_DELAY_MAX                (500/CY_DELAY_DFLT) /* half second */
#define CY_HNDSHK_BIT               0x80
/* power mode select bits */
#define CY_SOFT_RESET_MODE          0x01
#define CY_DEEP_SLEEP_MODE          0x02
#define CY_LOW_POWER_MODE           0x04
/* device mode bits */
#define CY_MODE_CHANGE              0x08 /* rd/wr hst_mode */
#define CY_OPERATE_MODE             0x00 /* rd/wr hst_mode */
#define CY_SYSINFO_MODE             0x10 /* rd/wr hst_mode */
#define CY_CONFIG_MODE              0x20 /* rd/wr hst_mode */
#define CY_BL_MODE                  0x01 /*
					  * wr hst mode == soft reset
					  * was 0X10 to rep_stat for LTS
					  */
#define CY_IGNORE_VALUE             0xFFFF
#define CY_CMD_RDY_BIT		0x40

#define CY_REG_OP_START             0
#define CY_REG_SI_START             0
#define CY_REG_OP_END               0x20
#define CY_REG_SI_END               0x20

/* register field lengths */
#define CY_NUM_REVCTRL              8
#define CY_NUM_MFGID                8
#define CY_NUM_TCHREC               10
#define CY_NUM_DDATA                32
#define CY_NUM_MDATA                64
#define CY_TMA884_MAX_BYTES         255 /*
					  * max reg access for TMA884
					  * in config mode
					  */
#define CY_TMA400_MAX_BYTES         512 /*
					  * max reg access for TMA400
					  * in config mode
					  */

/* touch event id codes */
#define CY_GET_EVENTID(reg)         ((reg & 0x60) >> 5)
#define CY_GET_TRACKID(reg)         (reg & 0x1F)
#define CY_NOMOVE                   0
#define CY_TOUCHDOWN                1
#define CY_MOVE                     2
#define CY_LIFTOFF                  3

#define CY_CFG_BLK_SIZE             126

#define CY_BL_VERS_SIZE             12
#define CY_MAX_PRBUF_SIZE           PIPE_BUF
#ifdef CONFIG_TOUCHSCREEN_DEBUG
#define CY_BL_TXT_FW_IMG_SIZE       128261
#define CY_BL_BIN_FW_IMG_SIZE       128261
#define CY_BL_FW_NAME_SIZE          NAME_MAX
#define CY_RW_REGID_MAX             0x1F
#define CY_RW_REG_DATA_MAX          0xFF
#define CY_NUM_PKG_PKT              4
#define CY_NUM_PKT_DATA             32
#define CY_MAX_PKG_DATA             (CY_NUM_PKG_PKT * CY_NUM_PKT_DATA)
#define CY_MAX_IC_BUF               256
#endif

/* abs settings */
#define CY_NUM_ABS_SET  5 /* number of abs values per setting */
/* abs value offset */
#define CY_SIGNAL_OST   0
#define CY_MIN_OST      1
#define CY_MAX_OST      2
#define CY_FUZZ_OST     3
#define CY_FLAT_OST     4
/* axis signal offset */
#define CY_ABS_X_OST    0
#define CY_ABS_Y_OST    1
#define CY_ABS_P_OST    2
#define CY_ABS_W_OST    3
#define CY_ABS_ID_OST   4

enum cyttsp4_powerstate {
	CY_IDLE_STATE,		/* IC cannot be reached */
	CY_READY_STATE,		/* pre-operational; ready to go to ACTIVE */
	CY_ACTIVE_STATE,	/* app is running, IC is scanning */
	CY_LOW_PWR_STATE,	/* not currently used  */
	CY_SLEEP_STATE,		/* app is running, IC is idle */
	CY_BL_STATE,		/* bootloader is running */
	CY_LDR_STATE,		/* loader is running */
	CY_SYSINFO_STATE,	/* switching to sysinfo mode */
	CY_CMD_STATE,		/* command initiation mode */
	CY_INVALID_STATE	/* always last in the list */
};

static char *cyttsp4_powerstate_string[] = {
	/* Order must match enum cyttsp4_powerstate above */
	"IDLE",
	"READY",
	"ACTIVE",
	"LOW_PWR",
	"SLEEP",
	"BOOTLOADER",
	"LOADER",
	"SYSINFO",
	"CMD",
	"INVALID"
};

enum cyttsp4_controller_mode {
	CY_MODE_BOOTLOADER,
	CY_MODE_SYSINFO,
	CY_MODE_OPERATIONAL,
	CY_MODE_CONFIG,
	CY_MODE_NUM
};

enum cyttsp4_ic_grpnum {
	CY_IC_GRPNUM_RESERVED = 0,
	CY_IC_GRPNUM_CMD_REGS,
	CY_IC_GRPNUM_TCH_REP,
	CY_IC_GRPNUM_DATA_REC,
	CY_IC_GRPNUM_TEST_REC,
	CY_IC_GRPNUM_PCFG_REC,
	CY_IC_GRPNUM_TCH_PARM_VAL,
	CY_IC_GRPNUM_TCH_PARM_SIZ,
	CY_IC_GRPNUM_RESERVED1,
	CY_IC_GRPNUM_RESERVED2,
	CY_IC_GRPNUM_OPCFG_REC,
	CY_IC_GRPNUM_DDATA_REC,
	CY_IC_GRPNUM_MDATA_REC,
	CY_IC_GRPNUM_TEST_DATA,
	CY_IC_GRPNUM_NUM
};

enum cyttsp4_ic_command {
	CY_GET_PARAM_CMD = 0x02,
	CY_SET_PARAM_CMD = 0x03,
	CY_GET_CFG_BLK_CRC = 0x05,
};

enum cyttsp4_ic_cfg_blkid {
	CY_TCH_PARM_BLKID = 0x00,
	CY_DDATA_BLKID = 0x05,
	CY_MDATA_BLKID = 0x06,
};

enum cyttsp4_flags {
	CY_FLAG_TMA400 = 0x01,
};

/* GEN4/SOLO Operational interface definitions */
struct cyttsp4_touch {
	u8 xh;
	u8 xl;
	u8 yh;
	u8 yl;
	u8 z;
	u8 t;
	u8 size;
} __packed;

struct cyttsp4_xydata {
	u8 hst_mode;
	u8 reserved;
	u8 cmd;
	u8 dat[CY_NUM_DAT];
	u8 rep_len;
	u8 rep_stat;
	u8 tt_stat;
	struct cyttsp4_touch tch[CY_NUM_TCH_ID];
} __packed;

/* TTSP System Information interface definitions */
struct cyttsp4_cydata {
	u8 ttpidh;
	u8 ttpidl;
	u8 fw_ver_major;
	u8 fw_ver_minor;
	u8 revctrl[CY_NUM_REVCTRL];
	u8 blver_major;
	u8 blver_minor;
	u8 jtag_si_id3;
	u8 jtag_si_id2;
	u8 jtag_si_id1;
	u8 jtag_si_id0;
	u8 mfgid_sz;
	u8 mfg_id[CY_NUM_MFGID];
	u8 cyito_idh;
	u8 cyito_idl;
	u8 cyito_verh;
	u8 cyito_verl;
	u8 ttsp_ver_major;
	u8 ttsp_ver_minor;
	u8 device_info;
} __attribute__((packed));

struct cyttsp4_test {
	u8 post_codeh;
	u8 post_codel;
	u8 reserved[2];	/* was bist_code */
} __packed;

struct cyttsp4_pcfg {
	u8 electrodes_x;
	u8 electrodes_y;
	u8 len_xh;
	u8 len_xl;
	u8 len_yh;
	u8 len_yl;
	u8 axis_xh;
	u8 axis_xl;
	u8 axis_yh;
	u8 axis_yl;
	u8 max_zh;
	u8 max_zl;
} __packed;

struct cyttsp4_opcfg {
	u8 cmd_ofs;
	u8 rep_ofs;
	u8 rep_szh;
	u8 rep_szl;
	u8 num_btns;
	u8 tt_stat_ofs;
	u8 obj_cfg0;
	u8 max_tchs;
	u8 tch_rec_siz;
	u8 tch_rec[CY_NUM_TCHREC];
	u8 reserved[4];
} __packed;

struct cyttsp4_ddata {
	u8 ddata[CY_NUM_DDATA];
} __packed;

struct cyttsp4_mdata {
	u8 mdata[CY_NUM_MDATA];
} __packed;

struct cyttsp4_sysinfo_data {
	u8 hst_mode;
	u8 reserved;
	u8 map_szh;
	u8 map_szl;
	u8 cydata_ofsh;
	u8 cydata_ofsl;
	u8 test_ofsh;
	u8 test_ofsl;
	u8 pcfg_ofsh;
	u8 pcfg_ofsl;
	u8 opcfg_ofsh;
	u8 opcfg_ofsl;
	u8 ddata_ofsh;
	u8 ddata_ofsl;
	u8 mdata_ofsh;
	u8 mdata_ofsl;
	struct cyttsp4_cydata cydata;
	struct cyttsp4_test cytest;
	struct cyttsp4_pcfg pcfg;
	struct cyttsp4_opcfg opcfg;
	struct cyttsp4_ddata ddata;
	struct cyttsp4_mdata mdata;
} __packed;

struct cyttsp4_sysinfo_ofs {
	size_t cmd_ofs;
	size_t rep_ofs;
	size_t rep_sz;
	size_t tt_stat_ofs;
	size_t tch_rec_siz;
	size_t map_sz;
	size_t cydata_ofs;
	size_t test_ofs;
	size_t pcfg_ofs;
	size_t opcfg_ofs;
	size_t ddata_ofs;
	size_t mdata_ofs;
};

/* driver context structure definitions */
#ifdef CONFIG_TOUCHSCREEN_DEBUG
struct cyttsp4_dbg_pkg {
	bool ready;
	int cnt;
	u8 data[CY_MAX_PKG_DATA];
};
#endif

struct cyttsp4 {
	struct device *dev;
	int irq;
	struct input_dev *input;
	struct mutex data_lock;		/* prevent concurrent accesses */
	struct mutex startup_mutex;	/* protect power on sequence */
	struct workqueue_struct		*cyttsp4_wq;
	struct work_struct		cyttsp4_resume_startup_work;
	char phys[32];
	const struct bus_type *bus_type;
	const struct touch_platform_data *platform_data;
	struct cyttsp4_bus_ops *bus_ops;
	struct cyttsp4_sysinfo_data sysinfo_data;
	struct cyttsp4_sysinfo_ofs si_ofs;
	struct completion bl_int_running;
	struct completion si_int_running;
	enum cyttsp4_powerstate power_state;
	enum cyttsp4_controller_mode current_mode;
	bool powered;
	bool cmd_rdy;
	bool hndshk_enabled;
	bool was_suspended;
	u16 flags;
	size_t max_config_bytes;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif
	bool bl_ready_flag;
#ifdef CONFIG_TOUCHSCREEN_DEBUG
	bool waiting_for_fw;
	bool debug_upgrade;
	char *fwname;
	bool irq_enabled;
	int ic_grpnum;
	int ic_grpoffset;
	bool ic_grptest;
#endif
};

static int _cyttsp4_load_app(struct cyttsp4 *ts, const u8 *fw, int fw_size);
static int _cyttsp4_startup(struct cyttsp4 *ts);
static int _cyttsp4_calc_data_crc(struct cyttsp4 *ts,
	size_t ndata, u8 *pdata, u8 *crc_h, u8 *crc_l, const char *name);
static int _cyttsp4_get_ic_crc(struct cyttsp4 *ts,
	u8 blkid, u8 *crc_h, u8 *crc_l);
static int _cyttsp4_set_config_mode(struct cyttsp4 *ts);
static int _cyttsp4_ldr_exit(struct cyttsp4 *ts);
static irqreturn_t cyttsp4_irq(int irq, void *handle);

static void cyttsp4_pr_state(struct cyttsp4 *ts)
{
	pr_info("%s: %s\n", __func__,
		ts->power_state < CY_INVALID_STATE ?
		cyttsp4_powerstate_string[ts->power_state] :
		"INVALID");
}

static int cyttsp4_read_block_data(struct cyttsp4 *ts, u16 command,
	u8 length, void *buf, int i2c_addr, bool use_subaddr)
{
	int retval;
	int tries;

	if (!buf || !length)
		return -EIO;

	for (tries = 0, retval = -1;
		tries < CY_NUM_RETRY && (retval < 0);
		tries++) {
		retval = ts->bus_ops->read(ts->bus_ops, command, length, buf,
			i2c_addr, use_subaddr);
		if (retval)
			udelay(500);
	}

	if (retval < 0) {
		pr_err("%s: I2C read block data fail (ret=%d)\n",
			__func__, retval);
	}
	return retval;
}

static int cyttsp4_write_block_data(struct cyttsp4 *ts, u16 command,
	u8 length, const void *buf, int i2c_addr, bool use_subaddr)
{
	int retval;
	int tries;

	if (!buf || !length)
		return -EIO;

	for (tries = 0, retval = -1;
		tries < CY_NUM_RETRY && (retval < 0);
		tries++) {
		retval = ts->bus_ops->write(ts->bus_ops, command, length, buf,
			i2c_addr, use_subaddr);
		if (retval)
			udelay(500);
	}

	if (retval < 0) {
		pr_err("%s: I2C write block data fail (ret=%d)\n",
			__func__, retval);
	}
	return retval;
}

static void _cyttsp4_pr_buf(struct cyttsp4 *ts,
	u8 *dptr, int size, const char *data_name)
{
#ifdef CONFIG_TOUCHSCREEN_DEBUG
	int i;
	int max = (CY_MAX_PRBUF_SIZE - 1) - sizeof(" truncated...");
	char *pbuf;

	if (ts->bus_ops->tsdebug >= CY_DBG_LVL_2) {
		pbuf = kzalloc(CY_MAX_PRBUF_SIZE, GFP_KERNEL);
		if (pbuf) {
			for (i = 0; i < size && i < max; i++)
				sprintf(pbuf, "%s %02X", pbuf, dptr[i]);
			pr_info("%s:  %s[0..%d]=%s%s\n", __func__,
				data_name, size-1, pbuf,
				size <= max ?
				"" : " truncated...");
			kfree(pbuf);
		} else
			pr_err("%s: buf allocation error\n", __func__);
	}
#endif
	return;
}

static int cyttsp4_hndshk(struct cyttsp4 *ts, u8 hst_mode)
{
	int retval = 0;
	u8 cmd;

	if (ts->hndshk_enabled) {
		cmd = hst_mode & CY_HNDSHK_BIT ?
			hst_mode & ~CY_HNDSHK_BIT :
			hst_mode | CY_HNDSHK_BIT;

		retval = cyttsp4_write_block_data(ts, CY_REG_BASE,
			sizeof(cmd), (u8 *)&cmd,
			ts->platform_data->addr[CY_TCH_ADDR_OFS], true);

		if (retval < 0) {
			pr_err("%s: I2C write fail on handshake (ret=%d)\n",
				__func__, retval);
		}
	}

	return retval;
}

static int _cyttsp4_hndshk_enable(struct cyttsp4 *ts)
{
	int retval = 0;
	int tries = 0;
	int tmp_state = 0;
	u8 cmd[CY_NUM_DAT+1];

	memset(cmd, 0, sizeof(cmd));
	cmd[0] = 0x26;	/* handshake enable operational command */
	cmd[1] = 0x02;	/* synchronous edge handshake */

	tmp_state = ts->power_state;
	ts->power_state = CY_CMD_STATE;
	cyttsp4_pr_state(ts);
	ts->cmd_rdy = false;
	retval = cyttsp4_write_block_data(ts, ts->si_ofs.cmd_ofs,
		sizeof(cmd), &cmd,
		ts->platform_data->addr[CY_TCH_ADDR_OFS], true);
	if (retval) {
		pr_err("%s: Error writing enable handshake cmd (ret=%d)\n",
			__func__, retval);
		return retval;
	}

	tries = 0;
	while (tries++ < 10000) {
		if (mutex_is_locked(&ts->data_lock)) {
			mutex_unlock(&ts->data_lock);
			udelay(1000);
			mutex_lock(&ts->data_lock);
		} else {
			udelay(1000);
		}

		if (ts->cmd_rdy) {
			retval = cyttsp4_read_block_data(ts, CY_REG_BASE +
				offsetof(struct cyttsp4_xydata, hst_mode),
				sizeof(cmd), cmd,
				ts->platform_data->addr[CY_TCH_ADDR_OFS], true);
			if (retval < 0) {
				pr_err("%s: fail read host mode"
					"r=%d\n", __func__, retval);
				goto _cyttsp4_set_hndshk_enable_exit;
			} else if (!(cmd[0] & CY_MODE_CHANGE)) {
				break;
			} else {
				/* not our interrupt */
				ts->cmd_rdy = false;
			}
		}
	}

	if (tries >= 10000) {
		pr_err("%s: timeout waiting for cmd ready interrupt\n",
			__func__);
		retval = -ETIMEDOUT;
		goto _cyttsp4_set_hndshk_enable_exit;
	}

	cyttsp4_dbg(ts, CY_DBG_LVL_1,
		"%s: check cmd ready tries=%d ret=%d cmd[0]=%02X\n",
		__func__, tries, retval, cmd[0]);

	ts->hndshk_enabled = true;

_cyttsp4_set_hndshk_enable_exit:
	ts->power_state = tmp_state;
	cyttsp4_pr_state(ts);
	return retval;
}

static int _cyttsp4_set_operational_mode(struct cyttsp4 *ts)
{
	int retval = 0;
	int tries = 0;
	int tmp_state = 0;
	u8 cmd = 0;

	cmd = CY_OPERATE_MODE + CY_MODE_CHANGE;

	tmp_state = ts->power_state;
	ts->power_state = CY_CMD_STATE;
	cyttsp4_pr_state(ts);
	ts->cmd_rdy = false;
	retval = cyttsp4_write_block_data(ts,
		CY_REG_BASE + offsetof(struct cyttsp4_xydata, hst_mode),
		sizeof(cmd), &cmd,
		ts->platform_data->addr[CY_TCH_ADDR_OFS], true);
	if (retval) {
		pr_err("%s: Error writing hst_mode reg (ret=%d)\n",
			__func__, retval);
		return retval;
	}

	tries = 0;
	cmd = 0;
	while (tries++ < 10000) {
		if (mutex_is_locked(&ts->data_lock)) {
			mutex_unlock(&ts->data_lock);
			udelay(1000);
			mutex_lock(&ts->data_lock);
		} else {
			udelay(1000);
		}

		if (ts->cmd_rdy) {
			retval = cyttsp4_read_block_data(ts, CY_REG_BASE +
				offsetof(struct cyttsp4_xydata, hst_mode),
				sizeof(cmd), &cmd,
				ts->platform_data->addr[CY_TCH_ADDR_OFS], true);
			if (retval < 0) {
				pr_err("%s: fail read host mode"
					"r=%d\n", __func__, retval);
				goto _cyttsp4_set_operational_mode_exit;
			} else if (!(cmd & CY_MODE_CHANGE)) {
				break;
			} else {
				/* not our interrupt */
				ts->cmd_rdy = false;
			}
		}
	}

	if (tries >= 10000) {
		/* try another read in case we missed our interrupt */
		retval = cyttsp4_read_block_data(ts, CY_REG_BASE +
			offsetof(struct cyttsp4_xydata, hst_mode),
			sizeof(cmd), &cmd,
			ts->platform_data->addr[CY_TCH_ADDR_OFS], true);
		if (retval < 0) {
			pr_err("%s: fail read host mode after time out"
				"r=%d\n", __func__, retval);
			goto _cyttsp4_set_operational_mode_exit;
		}
		if (cmd & CY_MODE_CHANGE) {
			/* the device did not clear the command change bit */
			pr_err("%s: timeout waiting for op mode"
				" ready interrupt\n", __func__);
			retval = -ETIMEDOUT;
			goto _cyttsp4_set_operational_mode_exit;
		}
	}

	if (cmd != CY_OPERATE_MODE) {
		pr_err("%s: failed to switch to operational mode\n", __func__);
		retval = -EIO;
	} else
		ts->current_mode = CY_MODE_OPERATIONAL;

	cyttsp4_dbg(ts, CY_DBG_LVL_1,
		"%s: check op ready tries=%d ret=%d host_mode=%02X\n",
		__func__, tries, retval, cmd);

_cyttsp4_set_operational_mode_exit:
	ts->power_state = tmp_state;
	cyttsp4_pr_state(ts);
	return retval;
}

static int _cyttsp4_set_sysinfo_mode(struct cyttsp4 *ts)
{
	int retval = 0;
	int tries = 0;
	int tmp_state = 0;
	u8 cmd = 0;

	cmd = CY_SYSINFO_MODE + CY_MODE_CHANGE;

	tmp_state = ts->power_state;
	ts->power_state = CY_CMD_STATE;
	cyttsp4_pr_state(ts);
	ts->cmd_rdy = false;
	retval = cyttsp4_write_block_data(ts,
		CY_REG_BASE + offsetof(struct cyttsp4_xydata, hst_mode),
		sizeof(cmd), &cmd,
		ts->platform_data->addr[CY_TCH_ADDR_OFS], true);
	if (retval) {
		pr_err("%s: Error writing hst_mode reg (ret=%d)\n",
			__func__, retval);
		return retval;
	}

	tries = 0;
	cmd = 0;
	while (tries++ < 10000) {
		if (mutex_is_locked(&ts->data_lock)) {
			mutex_unlock(&ts->data_lock);
			udelay(1000);
			mutex_lock(&ts->data_lock);
		} else {
			udelay(1000);
		}

		if (ts->cmd_rdy) {
			retval = cyttsp4_read_block_data(ts, CY_REG_BASE +
				offsetof(struct cyttsp4_xydata, hst_mode),
				sizeof(cmd), &cmd,
				ts->platform_data->addr[CY_TCH_ADDR_OFS], true);
			if (retval < 0) {
				pr_err("%s: fail read host mode"
					"r=%d\n", __func__, retval);
				goto _cyttsp4_set_sysinfo_mode_exit;
			} else if (!(cmd & CY_MODE_CHANGE)) {
				break;
			} else {
				/* not our interrupt */
				ts->cmd_rdy = false;
			}
		}
	}

	if (tries >= 10000) {
		/* try another read in case we missed our interrupt */
		retval = cyttsp4_read_block_data(ts, CY_REG_BASE +
			offsetof(struct cyttsp4_xydata, hst_mode),
			sizeof(cmd), &cmd,
			ts->platform_data->addr[CY_TCH_ADDR_OFS], true);
		if (retval < 0) {
			pr_err("%s: fail read host mode after time out"
				"r=%d\n", __func__, retval);
			goto _cyttsp4_set_sysinfo_mode_exit;
		}
		if (cmd & CY_MODE_CHANGE) {
			pr_err("%s: timeout waiting for sysinfo mode"
				" ready interrupt\n", __func__);
			retval = -ETIMEDOUT;
			goto _cyttsp4_set_sysinfo_mode_exit;
		}
	}

	if (cmd != CY_SYSINFO_MODE) {
		pr_err("%s: failed to switch to sysinfo mode\n", __func__);
		retval = -EIO;
	} else
		ts->current_mode = CY_MODE_SYSINFO;

	cyttsp4_dbg(ts, CY_DBG_LVL_1,
		"%s: check sysinfo ready tries=%d ret=%d host_mode=%02X\n",
		__func__, tries, retval, cmd);

_cyttsp4_set_sysinfo_mode_exit:
	ts->power_state = tmp_state;
	cyttsp4_pr_state(ts);
	return retval;
}

static int _cyttsp4_set_config_mode(struct cyttsp4 *ts)
{
	int retval = 0;
	int tries = 0;
	int tmp_state = 0;
	u8 cmd = 0;

	cmd = CY_CONFIG_MODE + CY_MODE_CHANGE;

	tmp_state = ts->power_state;
	ts->power_state = CY_CMD_STATE;
	cyttsp4_pr_state(ts);
	ts->cmd_rdy = false;
	retval = cyttsp4_write_block_data(ts,
		CY_REG_BASE + offsetof(struct cyttsp4_xydata, hst_mode),
		sizeof(cmd), &cmd,
		ts->platform_data->addr[CY_TCH_ADDR_OFS], true);
	if (retval) {
		pr_err("%s: Error writing hst_mode reg (ret=%d)\n",
			__func__, retval);
		return retval;
	}

	tries = 0;
	cmd = 0;
	while (tries++ < 10000) {
		if (mutex_is_locked(&ts->data_lock)) {
			mutex_unlock(&ts->data_lock);
			udelay(1000);
			mutex_lock(&ts->data_lock);
		} else {
			udelay(1000);
		}

		if (ts->cmd_rdy) {
			retval = cyttsp4_read_block_data(ts, CY_REG_BASE +
				offsetof(struct cyttsp4_xydata, hst_mode),
				sizeof(cmd), &cmd,
				ts->platform_data->addr[CY_TCH_ADDR_OFS], true);
			if (retval < 0) {
				pr_err("%s: fail read host mode"
					"r=%d\n", __func__, retval);
				goto _cyttsp4_set_config_mode_exit;
			} else if (!(cmd & CY_MODE_CHANGE)) {
				break;
			} else {
				/* not our interrupt */
				ts->cmd_rdy = false;
			}
		}
	}

	if (tries >= 10000) {
		/* try another read in case we missed our interrupt */
		retval = cyttsp4_read_block_data(ts, CY_REG_BASE +
			offsetof(struct cyttsp4_xydata, hst_mode),
			sizeof(cmd), &cmd,
			ts->platform_data->addr[CY_TCH_ADDR_OFS], true);
		if (retval < 0) {
			pr_err("%s: fail read host mode after time out"
				"r=%d\n", __func__, retval);
			goto _cyttsp4_set_config_mode_exit;
		}
		if (cmd & CY_MODE_CHANGE) {
			pr_err("%s: timeout waiting for config mode"
				" ready interrupt\n", __func__);
			retval = -ETIMEDOUT;
			goto _cyttsp4_set_config_mode_exit;
		}
	}

	if (cmd != CY_CONFIG_MODE) {
		pr_err("%s: failed to switch to config mode\n", __func__);
		retval = -EIO;
	} else
		ts->current_mode = CY_MODE_CONFIG;

	cyttsp4_dbg(ts, CY_DBG_LVL_1,
		"%s: check config ready tries=%d ret=%d host_mode=%02X\n",
		__func__, tries, retval, cmd);

_cyttsp4_set_config_mode_exit:
	ts->power_state = tmp_state;
	cyttsp4_pr_state(ts);
	return retval;
}

static int _cyttsp4_write_config_block(struct cyttsp4 *ts, u8 blockid,
	const u8 *pdata, size_t ndata, u8 crc_h, u8 crc_l, const char *name)
{
	int retval = 0;
	uint8_t *buf = NULL;
	size_t buf_size;
	int tmp_state = 0;
	int tries = 0;
	uint8_t response[2] = {0x00, 0x00};

	tmp_state = ts->power_state;
	ts->power_state = CY_CMD_STATE;
	cyttsp4_pr_state(ts);

	ts->cmd_rdy = false;

	/* pre-amble (10) + data (122) + crc (2) + key (8) */
	buf_size = sizeof(uint8_t) * 142;
	buf = kzalloc(buf_size, GFP_KERNEL);
	if (buf == NULL) {
		pr_err("%s: Failed to allocate buffer for %s\n",
			__func__, name);
		retval = -ENOMEM;
		goto _cyttsp4_write_config_block_exit;
	}

	if (pdata == NULL) {
		pr_err("%s: bad data pointer\n", __func__);
		retval = -ENXIO;
		goto _cyttsp4_write_config_block_exit;
	}

	if (ndata > 122) {
		pr_err("%s: %s is too large n=%d size=%d\n",
			__func__, name, ndata, 122);
		retval = -EOVERFLOW;
		goto _cyttsp4_write_config_block_exit;
	}

	/* Set command bytes */
	buf[0] = 0x04; /* cmd */
	buf[1] = 0x00; /* row offset high */
	buf[2] = 0x00; /* row offset low */
	buf[3] = 0x00; /* write block length high */
	buf[4] = 0x80; /* write block length low */
	buf[5] = blockid; /* write block id */
	buf[6] = 0x00; /* num of config bytes + 4 high */
	buf[7] = 0x7E; /* num of config bytes + 4 low */
	buf[8] = 0x00; /* max block size w/o crc high */
	buf[9] = 0x7E; /* max block size w/o crc low */

	/* Copy platform data */
	memcpy(&(buf[10]), pdata, ndata);

	/* Copy block CRC */
	buf[132] = crc_h;
	buf[133] = crc_l;

	/* Set key bytes */
	buf[134] = 0x45;
	buf[135] = 0x63;
	buf[136] = 0x36;
	buf[137] = 0x6F;
	buf[138] = 0x34;
	buf[139] = 0x38;
	buf[140] = 0x73;
	buf[141] = 0x77;

	/* Write config block */
	_cyttsp4_pr_buf(ts, buf, buf_size, name);

	retval = cyttsp4_write_block_data(ts, 0x03, 141, &(buf[1]),
		ts->platform_data->addr[CY_TCH_ADDR_OFS], true);
	if (retval < 0) {
		pr_err("%s: Failed to write config %s r=%d\n",
			__func__, name, retval);
		goto _cyttsp4_write_config_block_exit;
	}

	/* Write command */
	retval = cyttsp4_write_block_data(ts, 0x02, 1, &(buf[0]),
		ts->platform_data->addr[CY_TCH_ADDR_OFS], true);
	if (retval < 0) {
		pr_err("%s: Failed to write config command r=%d\n",
			__func__, retval);
		goto _cyttsp4_write_config_block_exit;
	}

	/* Wait for cmd rdy interrupt */
	while (tries++ < 10000) {
		if (mutex_is_locked(&ts->data_lock)) {
			mutex_unlock(&ts->data_lock);
			udelay(1000);
			mutex_lock(&ts->data_lock);
		} else {
			udelay(1000);
		}

		if (ts->cmd_rdy) {
			memset(response, 0, sizeof(response));
			retval = cyttsp4_read_block_data(ts,
				CY_REG_BASE + offsetof
				(struct cyttsp4_xydata, cmd),
				sizeof(response), &(response[0]),
				ts->platform_data->addr[CY_TCH_ADDR_OFS],
				true);
			if (retval < 0) {
				pr_err("%s: fail read cmd status"
					" r=%d\n", __func__, retval);
				goto _cyttsp4_write_config_block_exit;
			} else if (response[0] & CY_CMD_RDY_BIT) {
				break;
			} else {
				/* not our interrupt */
				ts->cmd_rdy = false;
			}
		}
	}

	if (tries >= 10000) {
		pr_err("%s: command write timeout\n", __func__);
		retval = -ETIMEDOUT;
		goto _cyttsp4_write_config_block_exit;
	}

	if (response[1] != 0x00) {
		pr_err("%s: Write config block command failed"
			" response=%02X %02X tries=%d\n",
			__func__, response[0], response[1], tries);
		retval = -EIO;
	}

_cyttsp4_write_config_block_exit:
	kfree(buf);
	ts->power_state = tmp_state;
	cyttsp4_pr_state(ts);
	return retval;
}

#ifdef CONFIG_TOUCHSCREEN_DEBUG
static int _cyttsp4_read_config_block(struct cyttsp4 *ts, u8 blockid,
	u8 *pdata, size_t ndata, const char *name)
{
	int retval = 0;
	int tmp_state = 0;
	int tries = 0;
	u8 cmd[CY_NUM_DAT+1];

	tmp_state = ts->power_state;
	ts->power_state = CY_CMD_STATE;
	cyttsp4_pr_state(ts);


	/* Set command bytes */
	cmd[0] = 0x03; /* cmd */
	cmd[1] = 0x00; /* row offset high */
	cmd[2] = 0x00; /* row offset low */
	cmd[3] = ndata / 256; /* write block length high */
	cmd[4] = ndata % 256; /* write block length low */
	cmd[5] = blockid; /* read block id */
	cmd[6] = 0x00; /* blank fill */

	/* Write config block */
	_cyttsp4_pr_buf(ts, cmd, sizeof(cmd), name);

	/* write the read config block command */
	ts->cmd_rdy = false;
	retval = cyttsp4_write_block_data(ts, CY_REG_BASE + offsetof
		(struct cyttsp4_xydata, cmd), sizeof(cmd), cmd,
		ts->platform_data->addr[CY_TCH_ADDR_OFS], true);
	if (retval < 0) {
		pr_err("%s: Failed to write read config command%s r=%d\n",
			__func__, name, retval);
		goto _cyttsp4_read_config_block_exit;
	}

	/* Wait for cmd rdy interrupt */
	tries = 0;
	while (tries++ < 10000) {
		if (mutex_is_locked(&ts->data_lock)) {
			mutex_unlock(&ts->data_lock);
			udelay(1000);
			mutex_lock(&ts->data_lock);
		} else {
			udelay(1000);
		}

		if (ts->cmd_rdy) {
			memset(pdata, 0, ndata);
			retval = cyttsp4_read_block_data(ts,
				CY_REG_BASE + offsetof
				(struct cyttsp4_xydata, cmd),
				ndata, pdata,
				ts->platform_data->addr[CY_TCH_ADDR_OFS],
				true);
			if (retval < 0) {
				pr_err("%s: fail read cmd status"
					" r=%d\n", __func__, retval);
				goto _cyttsp4_read_config_block_exit;
			} else if (pdata[0] & CY_CMD_RDY_BIT) {
				break;
			} else {
				/* not our interrupt */
				ts->cmd_rdy = false;
			}
		}
	}

	if (tries >= 10000) {
		pr_err("%s: command write timeout\n", __func__);
		retval = -ETIMEDOUT;
		goto _cyttsp4_read_config_block_exit;
	}

	/* write the returned raw read config block data */
	_cyttsp4_pr_buf(ts, pdata, ndata, name);

	if (pdata[1] != 0x00) {
		pr_err("%s: Read config block command failed"
			" response=%02X %02X tries=%d\n",
			__func__, pdata[0], pdata[1], tries);
		retval = -EIO;
	}

_cyttsp4_read_config_block_exit:
	ts->power_state = tmp_state;
	cyttsp4_pr_state(ts);
	return retval;
}
#endif

static int _cyttsp4_set_op_params(struct cyttsp4 *ts, u8 crc_h, u8 crc_l)
{
	int retval = 0;

	if (ts->platform_data->sett[CY_IC_GRPNUM_TCH_PARM_VAL] == NULL) {
		pr_err("%s: Missing Platform Touch Parameter"
			" values table\n", __func__);
		retval = -ENXIO;
		goto _cyttsp4_set_op_params_exit;
	}

	if ((ts->platform_data->sett
		[CY_IC_GRPNUM_TCH_PARM_VAL]->data == NULL) ||
		(ts->platform_data->sett
		[CY_IC_GRPNUM_TCH_PARM_VAL]->size == 0)) {
		pr_err("%s: Missing Platform Touch Parameter"
			" values table data\n", __func__);
		retval = -ENXIO;
		goto _cyttsp4_set_op_params_exit;
	}

	/* Change to Config Mode */
	retval = _cyttsp4_set_config_mode(ts);
	if (retval < 0) {
		pr_err("%s: Failed to switch to config mode"
			" for touch params\n", __func__);
		goto _cyttsp4_set_op_params_exit;
	}
	retval = _cyttsp4_write_config_block(ts, CY_TCH_PARM_BLKID,
		ts->platform_data->sett[CY_IC_GRPNUM_TCH_PARM_VAL]->data,
		ts->platform_data->sett[CY_IC_GRPNUM_TCH_PARM_VAL]->size,
		crc_h, crc_l, "platform_touch_param_data");

_cyttsp4_set_op_params_exit:
	return retval;
}

static int _cyttsp4_set_data_block(struct cyttsp4 *ts, u8 blkid, u8 *pdata,
	size_t ndata, const char *name, bool force, bool *data_updated)
{
	u8 data_crc[2];
	u8 ic_crc[2];
	int retval = 0;

	memset(data_crc, 0, sizeof(data_crc));
	memset(ic_crc, 0, sizeof(ic_crc));
	*data_updated = false;

	_cyttsp4_pr_buf(ts, pdata, ndata, name);

	cyttsp4_dbg(ts, CY_DBG_LVL_3,
		"%s: calc %s crc\n", __func__, name);
	retval = _cyttsp4_calc_data_crc(ts, ndata, pdata,
		&data_crc[0], &data_crc[1],
		name);
	if (retval < 0) {
		pr_err("%s: fail calc crc for %s (0x%02X%02X) r=%d\n",
			__func__, name,
			data_crc[0], data_crc[1],
			retval);
		goto _cyttsp_set_data_block_exit;
	}

	cyttsp4_dbg(ts, CY_DBG_LVL_3,
		"%s: get ic %s crc\n", __func__, name);
	retval = _cyttsp4_set_operational_mode(ts);
	if (retval < 0) {
		pr_err("%s: Failed to switch to operational mode\n", __func__);
		goto _cyttsp_set_data_block_exit;
	}
	retval = _cyttsp4_get_ic_crc(ts, blkid,
		&ic_crc[0], &ic_crc[1]);
	if (retval < 0) {
		pr_err("%s: fail get ic crc for %s (0x%02X%02X) r=%d\n",
			__func__, name,
			ic_crc[0], ic_crc[1],
			retval);
		goto _cyttsp_set_data_block_exit;
	}

	cyttsp4_dbg(ts, CY_DBG_LVL_3,
		"%s: %s calc_crc=0x%02X%02X ic_crc=0x%02X%02X\n",
		__func__, name,
		data_crc[0], data_crc[1],
		ic_crc[0], ic_crc[1]);
	if ((data_crc[0] != ic_crc[0]) || (data_crc[1] != ic_crc[1]) || force) {
		/* Change to Config Mode */
		retval = _cyttsp4_set_config_mode(ts);
		if (retval < 0) {
			pr_err("%s: Failed to switch to config mode"
				" for sysinfo regs\n", __func__);
			goto _cyttsp_set_data_block_exit;
		}
		retval = _cyttsp4_write_config_block(ts, blkid, pdata,
			ndata, data_crc[0], data_crc[1], name);
		if (retval < 0) {
			pr_err("%s: fail write %s config block r=%d\n",
				__func__, name, retval);
			goto _cyttsp_set_data_block_exit;
		}

		cyttsp4_dbg(ts, CY_DBG_LVL_3,
			"%s: write %s config block ok\n", __func__, name);
		*data_updated = true;
	}

_cyttsp_set_data_block_exit:
	return retval;
}

static int _cyttsp4_set_sysinfo_regs(struct cyttsp4 *ts, bool *updated)
{
	bool ddata_updated = false;
	bool mdata_updated = false;
	size_t num_data;
	u8 *pdata;
	int retval = 0;

	pdata = kzalloc(CY_NUM_MDATA, GFP_KERNEL);
	if (pdata == NULL) {
		pr_err("%s: fail allocate set sysinfo regs buffer\n", __func__);
		retval = -ENOMEM;
		goto _cyttsp4_set_sysinfo_regs_err;
	}

	/* check for missing DDATA */
	if (ts->platform_data->sett[CY_IC_GRPNUM_DDATA_REC] == NULL) {
		cyttsp4_dbg(ts, CY_DBG_LVL_3,
			"%s: No platform_ddata table\n", __func__);
		cyttsp4_dbg(ts, CY_DBG_LVL_3,
			"%s: Use a zero filled array to compare with device\n",
			__func__);
		goto _cyttsp4_set_sysinfo_regs_set_ddata_block;
	}
	if ((ts->platform_data->sett[CY_IC_GRPNUM_DDATA_REC]->data == NULL) ||
		(ts->platform_data->sett[CY_IC_GRPNUM_DDATA_REC]->size == 0)) {
		cyttsp4_dbg(ts, CY_DBG_LVL_3,
			"%s: No platform_ddata table data\n", __func__);
		cyttsp4_dbg(ts, CY_DBG_LVL_3,
			"%s: Use a zero filled array to compare with device\n",
			__func__);
		goto _cyttsp4_set_sysinfo_regs_set_ddata_block;
	}

	/* copy platform data design data to the device eeprom */
	num_data = ts->platform_data->sett
		[CY_IC_GRPNUM_DDATA_REC]->size < CY_NUM_DDATA ?
		ts->platform_data->sett
		[CY_IC_GRPNUM_DDATA_REC]->size : CY_NUM_DDATA;
	cyttsp4_dbg(ts, CY_DBG_LVL_3,
		"%s: copy %d bytes from platform data to ddata array\n",
		__func__, num_data);
	memcpy(pdata, ts->platform_data->sett[CY_IC_GRPNUM_DDATA_REC]->data,
		num_data);

_cyttsp4_set_sysinfo_regs_set_ddata_block:
	/* set data block will check CRC match/nomatch */
	retval = _cyttsp4_set_data_block(ts, CY_DDATA_BLKID, pdata,
		CY_NUM_DDATA, "platform_ddata", false, &ddata_updated);
	if (retval < 0) {
		pr_err("%s: Fail while writing platform_ddata"
			" block to ic r=%d\n", __func__, retval);
	}

	/* check for missing MDATA */
	if (ts->platform_data->sett[CY_IC_GRPNUM_MDATA_REC] == NULL) {
		cyttsp4_dbg(ts, CY_DBG_LVL_3,
			"%s: No platform_mdata table\n", __func__);
		cyttsp4_dbg(ts, CY_DBG_LVL_3,
			"%s: Use a zero filled array to compare with device\n",
			__func__);
		goto _cyttsp4_set_sysinfo_regs_set_mdata_block;
	}
	if ((ts->platform_data->sett[CY_IC_GRPNUM_MDATA_REC]->data == NULL) ||
		(ts->platform_data->sett[CY_IC_GRPNUM_MDATA_REC]->size == 0)) {
		cyttsp4_dbg(ts, CY_DBG_LVL_3,
			"%s: No platform_mdata table data\n", __func__);
		cyttsp4_dbg(ts, CY_DBG_LVL_3,
			"%s: Use a zero filled array to compare with device\n",
			__func__);
		goto _cyttsp4_set_sysinfo_regs_set_mdata_block;
	}

	/* copy platform manufacturing data to the device eeprom */
	num_data = ts->platform_data->sett
		[CY_IC_GRPNUM_MDATA_REC]->size < CY_NUM_MDATA ?
		ts->platform_data->sett
		[CY_IC_GRPNUM_MDATA_REC]->size : CY_NUM_MDATA;
	cyttsp4_dbg(ts, CY_DBG_LVL_3,
		"%s: copy %d bytes from platform data to mdata array\n",
		__func__, num_data);
	memcpy(pdata, ts->platform_data->sett[CY_IC_GRPNUM_MDATA_REC]->data,
		num_data);

_cyttsp4_set_sysinfo_regs_set_mdata_block:
	/* set data block will check CRC match/nomatch */
	retval = _cyttsp4_set_data_block(ts, CY_MDATA_BLKID, pdata,
		CY_NUM_MDATA, "platform_mdata", false, &mdata_updated);
	if (retval < 0) {
		pr_err("%s: Fail while writing platform_mdata"
			" block to ic r=%d\n", __func__, retval);
	}

	kfree(pdata);
_cyttsp4_set_sysinfo_regs_err:
	*updated = ddata_updated || mdata_updated;
	return retval;
}

static int _cyttsp4_get_sysinfo_regs(struct cyttsp4 *ts)
{
	int retval;

	retval = cyttsp4_read_block_data(ts, CY_REG_BASE,
		sizeof(ts->sysinfo_data), &(ts->sysinfo_data),
		ts->platform_data->addr[CY_TCH_ADDR_OFS], true);

	/* Write sysinfo data */
	_cyttsp4_pr_buf(ts, (u8 *)&ts->sysinfo_data,
		sizeof(ts->sysinfo_data), "sysinfo_data");

	ts->si_ofs.cmd_ofs = ts->sysinfo_data.opcfg.cmd_ofs;
	ts->si_ofs.rep_ofs = ts->sysinfo_data.opcfg.rep_ofs;
	ts->si_ofs.rep_sz = (ts->sysinfo_data.opcfg.rep_szh * 256) +
		ts->sysinfo_data.opcfg.rep_szl;
	ts->si_ofs.tt_stat_ofs = ts->sysinfo_data.opcfg.tt_stat_ofs;
	ts->si_ofs.tch_rec_siz = ts->sysinfo_data.opcfg.tch_rec_siz;
	ts->si_ofs.map_sz = (ts->sysinfo_data.map_szh * 256) +
		ts->sysinfo_data.map_szl;
	ts->si_ofs.cydata_ofs = (ts->sysinfo_data.cydata_ofsh * 256) +
		ts->sysinfo_data.cydata_ofsl;
	ts->si_ofs.test_ofs = (ts->sysinfo_data.test_ofsh * 256) +
		ts->sysinfo_data.test_ofsl;
	ts->si_ofs.pcfg_ofs = (ts->sysinfo_data.pcfg_ofsh * 256) +
		ts->sysinfo_data.pcfg_ofsl;
	ts->si_ofs.opcfg_ofs = (ts->sysinfo_data.opcfg_ofsh * 256) +
		ts->sysinfo_data.opcfg_ofsl;
	ts->si_ofs.ddata_ofs = (ts->sysinfo_data.ddata_ofsh * 256) +
		ts->sysinfo_data.ddata_ofsl;
	ts->si_ofs.mdata_ofs = (ts->sysinfo_data.mdata_ofsh * 256) +
		ts->sysinfo_data.mdata_ofsl;

	cyttsp4_dbg(ts, CY_DBG_LVL_3,
		"%s: cmd_ofs=%d\n", __func__, ts->si_ofs.cmd_ofs);
	cyttsp4_dbg(ts, CY_DBG_LVL_3,
		"%s: rep_ofs=%d\n", __func__, ts->si_ofs.rep_ofs);
	cyttsp4_dbg(ts, CY_DBG_LVL_3,
		"%s: rep_sz=%d\n", __func__, ts->si_ofs.rep_sz);
	cyttsp4_dbg(ts, CY_DBG_LVL_3,
		"%s: tt_stat_ofs=%d\n", __func__, ts->si_ofs.tt_stat_ofs);
	cyttsp4_dbg(ts, CY_DBG_LVL_3,
		"%s: tch_rec_siz=%d\n", __func__, ts->si_ofs.tch_rec_siz);
	cyttsp4_dbg(ts, CY_DBG_LVL_3,
		"%s: map_sz=%d\n", __func__, ts->si_ofs.map_sz);
	cyttsp4_dbg(ts, CY_DBG_LVL_3,
		"%s: cydata_ofs=%d\n", __func__, ts->si_ofs.cydata_ofs);
	cyttsp4_dbg(ts, CY_DBG_LVL_3,
		"%s: test_ofs=%d\n", __func__, ts->si_ofs.test_ofs);
	cyttsp4_dbg(ts, CY_DBG_LVL_3,
		"%s: pcfg_ofs=%d\n", __func__, ts->si_ofs.pcfg_ofs);
	cyttsp4_dbg(ts, CY_DBG_LVL_3,
		"%s: opcfg_ofs=%d\n", __func__, ts->si_ofs.opcfg_ofs);
	cyttsp4_dbg(ts, CY_DBG_LVL_3,
		"%s: ddata_ofs=%d\n", __func__, ts->si_ofs.ddata_ofs);
	cyttsp4_dbg(ts, CY_DBG_LVL_3,
		"%s: mdata_ofs=%d\n", __func__, ts->si_ofs.mdata_ofs);

	return retval;
}

#define CY_IRQ_DEASSERT	1
#define CY_IRQ_ASSERT	0

/* read xy_data for all current touches */
static int _cyttsp4_xy_worker(struct cyttsp4 *ts)
{
	struct cyttsp4_xydata xy_data;
	u8 num_cur_tch = 0;
	int i;
	int signal;
	int retval;

	/*
	 * Get event data from CYTTSP device.
	 * The event data includes all data
	 * for all active touches.
	 */
	memset(&xy_data, 0, sizeof(xy_data));
	retval = cyttsp4_read_block_data(ts, CY_REG_BASE,
		sizeof(struct cyttsp4_xydata), &xy_data,
		ts->platform_data->addr[CY_TCH_ADDR_OFS], true);
	if (retval < 0) {
		/* I2C bus failure implies bootloader running */
		pr_err("%s: read fail on touch regs r=%d\n",
			__func__, retval);
		queue_work(ts->cyttsp4_wq,
			&ts->cyttsp4_resume_startup_work);
		pr_err("%s: startup queued\n", __func__);
		retval = IRQ_HANDLED;
		goto _cyttsp4_xy_worker_exit;
	}

	cyttsp4_dbg(ts, CY_DBG_LVL_1,
		"%s: hst_mode=%02X rep_stat=%02X tt_stat=%02X"
		" x=%d y=%d p=%02X id=%02X\n",
		__func__, xy_data.hst_mode,
		xy_data.rep_stat, xy_data.tt_stat,
		(xy_data.tch[0].xh * 256) + xy_data.tch[0].xl,
		(xy_data.tch[0].yh * 256) + xy_data.tch[0].yl,
		xy_data.tch[0].z, xy_data.tch[0].t);

	_cyttsp4_pr_buf(ts, (u8 *)&xy_data,
		ts->si_ofs.rep_sz + ts->si_ofs.rep_ofs, "xy_data");

	/* provide flow control handshake */
	if (cyttsp4_hndshk(ts, xy_data.hst_mode)) {
		pr_err("%s: handshake fail on operational reg\n",
			__func__);
		ts->power_state = CY_IDLE_STATE;
		cyttsp4_pr_state(ts);
		retval = -EIO;
		goto _cyttsp4_xy_worker_exit;
	}

	/* if in System Information mode, then switch to operational mode */
	if (GET_HSTMODE(xy_data.hst_mode) == GET_HSTMODE(CY_SYSINFO_MODE)) {
		pr_err("%s: Host mode detected in op state (hm=0x%02X)\n",
			__func__, xy_data.hst_mode);
		retval = _cyttsp4_set_operational_mode(ts);
		if (retval < 0) {
			ts->power_state = CY_IDLE_STATE;
			cyttsp4_pr_state(ts);
			pr_err("%s: Fail set operational mode (r=%d)\n",
				__func__, retval);
		} else {
			ts->power_state = CY_ACTIVE_STATE;
			cyttsp4_pr_state(ts);
		}
		goto _cyttsp4_xy_worker_exit;
	}

	/* determine number of currently active touches */
	num_cur_tch = GET_NUM_TOUCHES(xy_data.tt_stat);

	/* check for any error conditions */
	if (ts->power_state == CY_IDLE_STATE) {
		pr_err("%s: IDLE STATE detected\n", __func__);
		retval = 0;
		goto _cyttsp4_xy_worker_exit;
	} else if (IS_LARGE_AREA(xy_data.tt_stat) == 1) {
		/* terminate all active tracks */
		num_cur_tch = 0;
		pr_err("%s: Large area detected\n", __func__);
		retval = -EIO;
		goto _cyttsp4_xy_worker_exit;
	} else if (num_cur_tch > CY_NUM_TCH_ID) {
		if (num_cur_tch == 0x1F) {
			/* terminate all active tracks */
			pr_err("%s: Num touch err detected (n=%d)\n",
				__func__, num_cur_tch);
			num_cur_tch = 0;
		} else {
			pr_err("%s: too many tch; set to max tch (n=%d c=%d)\n",
				__func__, num_cur_tch, CY_NUM_TCH_ID);
			num_cur_tch = CY_NUM_TCH_ID;
		}
	} else if (IS_BAD_PKT(xy_data.rep_stat)) {
		/* terminate all active tracks */
		num_cur_tch = 0;
		pr_err("%s: Invalid buffer detected\n", __func__);
	}

	cyttsp4_dbg(ts, CY_DBG_LVL_3,
		"%s: num_cur_tch=%d\n", __func__, num_cur_tch);

	/* extract xy_data for all currently reported touches */
	if (num_cur_tch) {
		for (i = 0; i < num_cur_tch; i++) {
			struct cyttsp4_touch *tch = &xy_data.tch[i];
#ifdef CONFIG_TOUCHSCREEN_DEBUG
			int e = CY_GET_EVENTID(tch->t);
#endif
			int t = CY_GET_TRACKID(tch->t);
			int x = (tch->xh * 256) + tch->xl;
			int y = (tch->yh * 256) + tch->yl;
			int p = tch->z;
			int w = tch->size;
#if defined(CONFIG_MACH_OMAP4_BOWSER_SUBTYPE_TATE)
			/* ey: change the orientation to portrait */
			y = (tch->xh * 256) + tch->xl;
			x = (tch->yh * 256) + tch->yl;
			y = CY_MAXY - y;
#endif
			if (t)
				t--;	/* use 0 based track id's */
			signal = ts->platform_data->frmwrk->abs
				[(CY_ABS_ID_OST*CY_NUM_ABS_SET)+0];
			if (signal != CY_IGNORE_VALUE)
				input_report_abs(ts->input, signal, t);

			signal = ts->platform_data->frmwrk->abs
				[(CY_ABS_X_OST*CY_NUM_ABS_SET)+0];
			if (signal != CY_IGNORE_VALUE)
				input_report_abs(ts->input, signal, x);

			signal = ts->platform_data->frmwrk->abs
				[(CY_ABS_Y_OST*CY_NUM_ABS_SET)+0];
			if (signal != CY_IGNORE_VALUE)
				input_report_abs(ts->input, signal, y);

			signal = ts->platform_data->frmwrk->abs
				[(CY_ABS_P_OST*CY_NUM_ABS_SET)+0];
			if (signal != CY_IGNORE_VALUE)
				input_report_abs(ts->input, signal, p);

			signal = ts->platform_data->frmwrk->abs
				[(CY_ABS_W_OST*CY_NUM_ABS_SET)+0];
			if (signal != CY_IGNORE_VALUE)
				input_report_abs(ts->input, signal, w);
			input_mt_sync(ts->input);
			cyttsp4_dbg(ts, CY_DBG_LVL_1,
				"%s: t=%d x=%04X y=%04X z=%02X"
				" size=%02X e=%02X\n",
				__func__, t, x, y, p, w, e);
		}
		input_report_key(ts->input, BTN_TOUCH, num_cur_tch > 0);
		input_sync(ts->input);
	} else {
		input_mt_sync(ts->input);
		input_report_key(ts->input, BTN_TOUCH, num_cur_tch > 0);
		input_sync(ts->input);
	}

	cyttsp4_dbg(ts, CY_DBG_LVL_1, "%s:\n", __func__);
	retval = 0;
_cyttsp4_xy_worker_exit:
	return retval;
}

static int _cyttsp4_soft_reset(struct cyttsp4 *ts)
{
	u8 cmd = CY_SOFT_RESET_MODE;

	return cyttsp4_write_block_data(ts,
		offsetof(struct cyttsp4_xydata, hst_mode),
		sizeof(cmd), &cmd,
		ts->platform_data->addr[CY_TCH_ADDR_OFS], true);
}

static int _cyttsp4_reset(struct cyttsp4 *ts)
{
	bool soft_reset = false;
	int retval = 0;

	if (ts->platform_data->hw_reset) {
		retval = ts->platform_data->hw_reset();
		if (retval == -ENOSYS) {
			soft_reset = true;
			retval = _cyttsp4_soft_reset(ts);
		}
	} else {
		soft_reset = true;
		retval = _cyttsp4_soft_reset(ts);
	}

	if (soft_reset) {
		cyttsp4_dbg(ts, CY_DBG_LVL_2,
			"%s: Used Soft Reset - %s (r=%d)\n", __func__,
			retval ? "Fail" : "Ok", retval);
	} else {
		cyttsp4_dbg(ts, CY_DBG_LVL_2,
			"%s: Used Hard Reset - %s (r=%d)\n", __func__,
			retval ? "Fail" : "Ok", retval);
	}

	if (retval < 0)
		return retval;
	else {
		ts->current_mode = CY_MODE_BOOTLOADER;
		return retval;
	}
}

static void cyttsp4_ts_work_func(struct work_struct *work)
{
	struct cyttsp4 *ts =
		container_of(work, struct cyttsp4, cyttsp4_resume_startup_work);
	int retval = 0;

	mutex_lock(&ts->data_lock);

	retval = _cyttsp4_startup(ts);
	if (retval < 0) {
		pr_err("%s: Startup failed with error code %d\n",
			__func__, retval);
		ts->power_state = CY_IDLE_STATE;
		cyttsp4_pr_state(ts);
	}

	mutex_unlock(&ts->data_lock);

	return;
}

#if defined(CONFIG_PM) || defined(CONFIG_HAS_EARLYSUSPEND)
int cyttsp4_resume(void *handle)
{
	struct cyttsp4 *ts = handle;
	int tries = 0;
	int retval = 0;
	u8 dummy = 0x00;

	mutex_lock(&ts->data_lock);

	cyttsp4_dbg(ts, CY_DBG_LVL_2, "%s: Resuming...\n", __func__);

	switch (ts->power_state) {
	case CY_SLEEP_STATE:
		/* Any I2C line traffic will wake the part */
		/*
		 * This write should always fail because the part is not
		 * ready to respond.
		 */
		ts->cmd_rdy = false;
		ts->power_state = CY_CMD_STATE;
		retval = cyttsp4_read_block_data(ts, CY_REG_BASE,
			sizeof(dummy), &dummy,
			ts->platform_data->addr[CY_TCH_ADDR_OFS], false);
		if (retval < 0) {
			pr_info("%s: Read failure -121 is normal here--%s\n",
				__func__, "the I2C lines just need to toggle");
		} else {
			/* The IC is already awake, so resume is finished. */
			ts->power_state = CY_ACTIVE_STATE;
			cyttsp4_pr_state(ts);
			break;
		}
		/* Wait for cmd rdy interrupt to signal device wake */
		tries = 0;
		do {
			mutex_unlock(&ts->data_lock);
			msleep(CY_DELAY_DFLT);
			mutex_lock(&ts->data_lock);
		} while (!ts->cmd_rdy && (tries++ < CY_DELAY_MAX));

		if (ts->cmd_rdy) {
			if (!(ts->flags & CY_FLAG_TMA400)) {
				retval = cyttsp4_read_block_data(ts,
					CY_REG_BASE,
					sizeof(dummy), &dummy,
					ts->platform_data->addr
					[CY_TCH_ADDR_OFS],
					false);
			}
		} else
			retval = -EIO;

		if (retval < 0) {
			pr_err("%s: failed to resume or in bootloader (r=%d)\n",
				__func__, retval);
			ts->power_state = CY_BL_STATE;
			cyttsp4_pr_state(ts);
			mutex_unlock(&ts->data_lock);
			queue_work(ts->cyttsp4_wq,
				&ts->cyttsp4_resume_startup_work);
			pr_info("%s: startup queued\n", __func__);
			retval = 0;
			goto cyttsp4_resume_exit;
		} else {
			cyttsp4_dbg(ts, CY_DBG_LVL_3,
				"%s: resume in tries=%d\n", __func__, tries);
			retval = cyttsp4_hndshk(ts, dummy);
			if (retval < 0) {
				pr_err("%s: fail resume INT handshake (r=%d)\n",
					__func__, retval);
				/* continue */
			}
			ts->power_state = CY_ACTIVE_STATE;
			cyttsp4_pr_state(ts);
		}
		break;
	case CY_IDLE_STATE:
	case CY_READY_STATE:
	case CY_ACTIVE_STATE:
	case CY_LOW_PWR_STATE:
	case CY_BL_STATE:
	case CY_LDR_STATE:
	case CY_SYSINFO_STATE:
	case CY_CMD_STATE:
	case CY_INVALID_STATE:
	default:
		pr_err("%s: Already in %s state\n", __func__,
			cyttsp4_powerstate_string[ts->power_state]);
		break;
	}

	mutex_unlock(&ts->data_lock);
cyttsp4_resume_exit:
	cyttsp4_dbg(ts, CY_DBG_LVL_3,
		"%s: exit Resume r=%d\n", __func__, retval);
	return retval;
}
EXPORT_SYMBOL_GPL(cyttsp4_resume);

int cyttsp4_suspend(void *handle)
{
	int retval = 0;
	struct cyttsp4 *ts = handle;
	uint8_t sleep;

	switch (ts->power_state) {
	case CY_ACTIVE_STATE:
		mutex_lock(&ts->data_lock);

		cyttsp4_dbg(ts, CY_DBG_LVL_2, "%s: Suspending...\n", __func__);

		sleep = CY_DEEP_SLEEP_MODE;
		retval = cyttsp4_write_block_data(ts, CY_REG_BASE +
			offsetof(struct cyttsp4_xydata, hst_mode),
			sizeof(sleep), &sleep,
			ts->platform_data->addr[CY_TCH_ADDR_OFS], true);
		if (retval < 0) {
			pr_err("%s: Failed to write sleep bit\n", __func__);
		} else {
			ts->power_state = CY_SLEEP_STATE;
			cyttsp4_pr_state(ts);
		}

		mutex_unlock(&ts->data_lock);
		break;
	case CY_SLEEP_STATE:
		pr_err("%s: already in Sleep state\n", __func__);
		break;
	case CY_LDR_STATE:
	case CY_CMD_STATE:
	case CY_SYSINFO_STATE:
	case CY_READY_STATE:
		retval = -EBUSY;
		pr_err("%s: Suspend Blocked while in %s state\n", __func__,
			cyttsp4_powerstate_string[ts->power_state]);
		break;
	case CY_IDLE_STATE:
	case CY_LOW_PWR_STATE:
	case CY_BL_STATE:
	case CY_INVALID_STATE:
	default:
		pr_err("%s: Cannot enter suspend from %s state\n", __func__,
			cyttsp4_powerstate_string[ts->power_state]);
		break;
	}

	return retval;
}
EXPORT_SYMBOL_GPL(cyttsp4_suspend);
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
void cyttsp4_early_suspend(struct early_suspend *h)
{
	struct cyttsp4 *ts = container_of(h, struct cyttsp4, early_suspend);
	int retval = 0;

	retval = cyttsp4_suspend(ts);
	if (retval < 0) {
		pr_err("%s: Early suspend failed with error code %d\n",
			__func__, retval);
	}
}

void cyttsp4_late_resume(struct early_suspend *h)
{
	struct cyttsp4 *ts = container_of(h, struct cyttsp4, early_suspend);
	int retval = 0;

	retval = cyttsp4_resume(ts);
	if (retval < 0) {
		pr_err("%s: Late resume failed with error code %d\n",
			__func__, retval);
	}
}
#endif

static int _cyttsp4_boot_loader(struct cyttsp4 *ts, bool *upgraded)
{
	bool new_vers = false;
	int retval = 0;
	int i = 0;
	u32 fw_vers_platform = 0;
	u32 fw_vers_img = 0;
	u32 fw_revctrl_platform_h = 0;
	u32 fw_revctrl_platform_l = 0;
	u32 fw_revctrl_img_h = 0;
	u32 fw_revctrl_img_l = 0;
	bool new_fw_vers = false;
	bool new_fw_revctrl = false;

	*upgraded = false;
	if (ts->power_state == CY_SLEEP_STATE) {
		pr_err("%s: cannot load firmware in sleep state\n",
			__func__);
		retval = 0;
	} else if ((ts->platform_data->fw->ver == NULL) ||
		(ts->platform_data->fw->img == NULL)) {
		pr_err("%s: empty version list or no image\n",
			__func__);
		retval = 0;
	} else if (ts->platform_data->fw->vsize != CY_BL_VERS_SIZE) {
		pr_err("%s: bad fw version list size=%d\n",
			__func__, ts->platform_data->fw->vsize);
		retval = 0;
	} else {
		/* automatically update firmware if new version detected */
		new_vers = false;
		fw_vers_img = (ts->sysinfo_data.cydata.fw_ver_major * 256);
		fw_vers_img += ts->sysinfo_data.cydata.fw_ver_minor;
		fw_vers_platform = ts->platform_data->fw->ver[2] * 256;
		fw_vers_platform += ts->platform_data->fw->ver[3];
		if (fw_vers_platform > fw_vers_img)
			new_fw_vers = true;
		else
			new_fw_vers = false;
		cyttsp4_dbg(ts, CY_DBG_LVL_3,
			"%s: fw_vers_platform=%04X fw_vers_img=%04X\n",
			__func__, fw_vers_platform, fw_vers_img);

		fw_revctrl_img_h = ts->sysinfo_data.cydata.revctrl[0];
		fw_revctrl_img_l = ts->sysinfo_data.cydata.revctrl[4];
		fw_revctrl_platform_h = ts->platform_data->fw->ver[4];
		fw_revctrl_platform_l = ts->platform_data->fw->ver[8];
		for (i = 1; i < 4; i++) {
			fw_revctrl_img_h = (fw_revctrl_img_h * 256) +
				ts->sysinfo_data.cydata.revctrl[0+i];
			fw_revctrl_img_l = (fw_revctrl_img_l * 256) +
				ts->sysinfo_data.cydata.revctrl[4+i];
			fw_revctrl_platform_h = (fw_revctrl_platform_h * 256) +
				ts->platform_data->fw->ver[4+i];
			fw_revctrl_platform_l = (fw_revctrl_platform_l * 256) +
				ts->platform_data->fw->ver[8+i];
		}
		if (fw_revctrl_platform_h > fw_revctrl_img_h)
			new_fw_revctrl = true;
		else if (fw_revctrl_platform_h == fw_revctrl_img_h) {
			if (fw_revctrl_platform_l > fw_revctrl_img_l)
				new_fw_revctrl = true;
			else
				new_fw_revctrl = false;
		} else
			new_fw_revctrl = false;

		if (new_fw_vers || new_fw_revctrl)
			new_vers = true;

		cyttsp4_dbg(ts, CY_DBG_LVL_3,
			"%s: fw_revctrl_platform_h=%08X"
			" fw_revctrl_img_h=%08X\n", __func__,
			fw_revctrl_platform_h, fw_revctrl_img_h);
		cyttsp4_dbg(ts, CY_DBG_LVL_3,
			"%s: fw_revctrl_platform_l=%08X"
			" fw_revctrl_img_l=%08X\n", __func__,
			fw_revctrl_platform_l, fw_revctrl_img_l);
		cyttsp4_dbg(ts, CY_DBG_LVL_3,
			"%s: new_fw_vers=%d new_fw_revctrl=%d new_vers=%d\n",
			__func__,
			(int)new_fw_vers, (int)new_fw_revctrl, (int)new_vers);

		if (new_vers) {
			pr_info("%s: upgrading firmware...\n", __func__);
			*upgraded = true;
			retval = _cyttsp4_reset(ts);
			if (retval < 0) {
				pr_err("%s: fail to reset to bootloader r=%d\n",
					__func__, retval);
				goto _cyttsp4_boot_loader_exit;
			}
			retval = _cyttsp4_load_app(ts,
				ts->platform_data->fw->img,
				ts->platform_data->fw->size);
			if (retval) {
				pr_err("%s: I2C fail on load fw (r=%d)\n",
					__func__, retval);
				ts->power_state = CY_IDLE_STATE;
				retval = -EIO;
				cyttsp4_pr_state(ts);
			} else {
				/* reset TTSP Device back to bootloader mode */
				retval = _cyttsp4_reset(ts);
			}
		} else {
			cyttsp4_dbg(ts, CY_DBG_LVL_2,
				"%s: No auto firmware upgrade required\n",
				__func__);
		}
	}

_cyttsp4_boot_loader_exit:
	return retval;
}

static ssize_t cyttsp4_ic_ver_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct cyttsp4 *ts = dev_get_drvdata(dev);

	return sprintf(buf, "%s: 0x%02X 0x%02X\n%s: 0x%02X\n%s: 0x%02X\n%s: "
		"0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X\n",
		"TrueTouch Product ID",
		ts->sysinfo_data.cydata.ttpidh,
		ts->sysinfo_data.cydata.ttpidl,
		"Firmware Major Version", ts->sysinfo_data.cydata.fw_ver_major,
		"Firmware Minor Version", ts->sysinfo_data.cydata.fw_ver_minor,
		"Revision Control Number", ts->sysinfo_data.cydata.revctrl[0],
		ts->sysinfo_data.cydata.revctrl[1],
		ts->sysinfo_data.cydata.revctrl[2],
		ts->sysinfo_data.cydata.revctrl[3],
		ts->sysinfo_data.cydata.revctrl[4],
		ts->sysinfo_data.cydata.revctrl[5],
		ts->sysinfo_data.cydata.revctrl[6],
		ts->sysinfo_data.cydata.revctrl[7]);
}
static DEVICE_ATTR(ic_ver, S_IRUSR | S_IWUSR,
	cyttsp4_ic_ver_show, NULL);

/* Driver version */
static ssize_t cyttsp4_drv_ver_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, CY_MAX_PRBUF_SIZE,
		"Driver: %s\nVersion: %s\nDate: %s\n",
		CY_I2C_NAME, CY_DRIVER_VERSION, CY_DRIVER_DATE);
}
static DEVICE_ATTR(drv_ver, S_IRUGO, cyttsp4_drv_ver_show, NULL);


/* Driver status */
static ssize_t cyttsp4_drv_stat_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct cyttsp4 *ts = dev_get_drvdata(dev);

	return snprintf(buf, CY_MAX_PRBUF_SIZE,
		"Driver state is %s\n",
		cyttsp4_powerstate_string[ts->power_state]);
}
static DEVICE_ATTR(drv_stat, S_IRUGO, cyttsp4_drv_stat_show, NULL);

#ifdef CONFIG_TOUCHSCREEN_DEBUG
static ssize_t cyttsp_ic_irq_stat_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int retval;
	struct cyttsp4 *ts = dev_get_drvdata(dev);

	if (ts->platform_data->irq_stat) {
		retval = ts->platform_data->irq_stat();
		switch (retval) {
		case 0:
			return snprintf(buf, CY_MAX_PRBUF_SIZE,
				"Interrupt line is LOW.\n");
		case 1:
			return snprintf(buf, CY_MAX_PRBUF_SIZE,
				"Interrupt line is HIGH.\n");
		default:
			return snprintf(buf, CY_MAX_PRBUF_SIZE,
				"Function irq_stat() returned %d.\n", retval);
		}
	} else {
		return snprintf(buf, CY_MAX_PRBUF_SIZE,
			"Function irq_stat() undefined.\n");
	}
}
static DEVICE_ATTR(hw_irqstat, S_IRUSR | S_IWUSR,
	cyttsp_ic_irq_stat_show, NULL);
#endif

#ifdef CONFIG_TOUCHSCREEN_DEBUG
/* Disable Driver IRQ */
static ssize_t cyttsp4_drv_irq_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	static const char *fmt_disabled = "Driver interrupt is DISABLED\n";
	static const char *fmt_enabled = "Driver interrupt is ENABLED\n";
	struct cyttsp4 *ts = dev_get_drvdata(dev);

	if (ts->irq_enabled == false)
		return snprintf(buf, strlen(fmt_disabled)+1, fmt_disabled);
	else
		return snprintf(buf, strlen(fmt_enabled)+1, fmt_enabled);
}
static ssize_t cyttsp4_drv_irq_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int retval = 0;
	struct cyttsp4 *ts = dev_get_drvdata(dev);
	unsigned long value;

	mutex_lock(&(ts->data_lock));

	if (size > 2) {
		pr_err("%s: Err, data too large\n", __func__);
		retval = -EOVERFLOW;
		goto cyttsp4_drv_irq_store_error_exit;
	}

	retval = strict_strtoul(buf, 10, &value);
	if (retval < 0) {
		pr_err("%s: Failed to convert value\n", __func__);
		goto cyttsp4_drv_irq_store_error_exit;
	}

	if (ts->irq_enabled == false) {
		if (value == 1) {
			/* Enable IRQ */
			enable_irq(ts->irq);
			pr_info("%s: Driver IRQ now enabled\n", __func__);
			ts->irq_enabled = true;
		} else {
			pr_info("%s: Driver IRQ already disabled\n", __func__);
		}
	} else {
		if (value == 0) {
			/* Disable IRQ */
			disable_irq_nosync(ts->irq);
			pr_info("%s: Driver IRQ now disabled\n", __func__);
			ts->irq_enabled = false;
		} else {
			pr_info("%s: Driver IRQ already enabled\n", __func__);
		}
	}

	retval = size;

cyttsp4_drv_irq_store_error_exit:
	mutex_unlock(&(ts->data_lock));
	return retval;
}
static DEVICE_ATTR(drv_irq, S_IRUSR | S_IWUSR,
	cyttsp4_drv_irq_show, cyttsp4_drv_irq_store);
#endif

#ifdef CONFIG_TOUCHSCREEN_DEBUG
/* Driver debugging */
static ssize_t cyttsp4_drv_debug_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct cyttsp4 *ts = dev_get_drvdata(dev);

	return snprintf(buf, CY_MAX_PRBUF_SIZE,
		"Debug Setting: %u\n", ts->bus_ops->tsdebug);
}
static ssize_t cyttsp4_drv_debug_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct cyttsp4 *ts = dev_get_drvdata(dev);
	int retval = 0;
	unsigned long value = 0;
	u8 dummy = 0;

	retval = strict_strtoul(buf, 10, &value);
	if (retval < 0) {
		pr_err("%s: Failed to convert value\n", __func__);
		goto cyttsp4_drv_debug_store_exit;
	}

	switch (value) {
	case CY_DBG_LVL_0:
	case CY_DBG_LVL_1:
	case CY_DBG_LVL_2:
	case CY_DBG_LVL_3:
		pr_info("%s: Debug setting=%d\n", __func__, (int)value);
		ts->bus_ops->tsdebug = value;
		break;
	default:
		pr_err("%s: INVALID debug setting=%d\n",
			__func__, (int)value);
		break;
	}

	retval = size;

cyttsp4_drv_debug_store_exit:
	return retval;
}
static DEVICE_ATTR(drv_debug, S_IWUSR | S_IRUGO,
	cyttsp4_drv_debug_show, cyttsp4_drv_debug_store);
#endif

#ifdef CONFIG_TOUCHSCREEN_DEBUG
/* Group Number */
static ssize_t cyttsp4_ic_grpnum_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct cyttsp4 *ts = dev_get_drvdata(dev);

	return snprintf(buf, CY_MAX_PRBUF_SIZE,
		"Current Group: %d\n", ts->ic_grpnum);
}
static ssize_t cyttsp4_ic_grpnum_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct cyttsp4 *ts = dev_get_drvdata(dev);
	unsigned long value = 0;
	int retval = 0;

	mutex_lock(&(ts->data_lock));
	retval = strict_strtoul(buf, 10, &value);
	if (retval < 0) {
		pr_err("%s: Failed to convert value\n", __func__);
		goto cyttsp4_ic_grpnum_store_error_exit;
	}

	if (value > 0xFF) {
		value = 0xFF;
		pr_err("%s: value is greater than max;"
			" set to %d\n", __func__, (int)value);
	}
	ts->ic_grpnum = value;

	cyttsp4_dbg(ts, CY_DBG_LVL_2,
		"%s: grpnum=%d\n", __func__, ts->ic_grpnum);

cyttsp4_ic_grpnum_store_error_exit:
	retval = size;
	mutex_unlock(&(ts->data_lock));
	return retval;
}
static DEVICE_ATTR(ic_grpnum, S_IRUSR | S_IWUSR,
	cyttsp4_ic_grpnum_show, cyttsp4_ic_grpnum_store);

/* Group Offset */
static ssize_t cyttsp4_ic_grpoffset_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct cyttsp4 *ts = dev_get_drvdata(dev);

	return snprintf(buf, CY_MAX_PRBUF_SIZE,
		"Current Offset: %u\n", ts->ic_grpoffset);
}
static ssize_t cyttsp4_ic_grpoffset_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct cyttsp4 *ts = dev_get_drvdata(dev);
	unsigned long value;
	int retval = 0;

	mutex_lock(&(ts->data_lock));
	retval = strict_strtoul(buf, 10, &value);
	if (retval < 0) {
		pr_err("%s: Failed to convert value\n", __func__);
		goto cyttsp4_ic_grpoffset_store_error_exit;
	}

	if (value > 0xFF) {
		value = 0xFF;
		pr_err("%s: value is greater than max;"
			" set to %d\n", __func__, (int)value);
	}
	ts->ic_grpoffset = value;

	cyttsp4_dbg(ts, CY_DBG_LVL_2,
		"%s: grpoffset=%d\n", __func__, ts->ic_grpoffset);

cyttsp4_ic_grpoffset_store_error_exit:
	retval = size;
	mutex_unlock(&(ts->data_lock));
	return retval;
}
static DEVICE_ATTR(ic_grpoffset, S_IRUSR | S_IWUSR,
	cyttsp4_ic_grpoffset_show, cyttsp4_ic_grpoffset_store);

/* Group Data */
static ssize_t _cyttsp4_ic_grpdata_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct cyttsp4 *ts = dev_get_drvdata(dev);
	int i;
	size_t ndata = 0;
	int retval = 0;
	size_t num_read = 0;
	u8 *ic_buf;
	u8 *pdata;
	u8 blockid = 0;

	ic_buf = kzalloc(CY_MAX_PRBUF_SIZE, GFP_KERNEL);
	if (ic_buf == NULL) {
		pr_err("%s: Failed to allocate buffer for %s\n",
			__func__, "ic_grpdata_show");
		return snprintf(buf, CY_MAX_PRBUF_SIZE,
			"Group %d buffer allocation error.\n",
			ts->ic_grpnum);
	}
	cyttsp4_dbg(ts, CY_DBG_LVL_3,
		"%s: grpnum=%d grpoffset=%u\n",
		__func__, ts->ic_grpnum, ts->ic_grpoffset);

	if (ts->ic_grpnum >= CY_IC_GRPNUM_NUM) {
		pr_err("%s: Group %d does not exist.\n",
			__func__, ts->ic_grpnum);
		kfree(ic_buf);
		return snprintf(buf, CY_MAX_PRBUF_SIZE,
			"Group %d does not exist.\n",
			ts->ic_grpnum);
	}

	switch (ts->ic_grpnum) {
	case CY_IC_GRPNUM_RESERVED:
		goto cyttsp4_ic_grpdata_show_grperr;
		break;
	case CY_IC_GRPNUM_CMD_REGS:
		num_read = ts->si_ofs.rep_ofs - ts->si_ofs.cmd_ofs;
		cyttsp4_dbg(ts, CY_DBG_LVL_3,
			"%s: GRP=CMD_REGS: num_read=%d at ofs=%d + grpofs%d\n",
			__func__, num_read,
			ts->si_ofs.cmd_ofs, ts->ic_grpoffset);
		if (ts->ic_grpoffset >= num_read)
			goto cyttsp4_ic_grpdata_show_ofserr;
		else {
			num_read -= ts->ic_grpoffset;
			retval = cyttsp4_read_block_data(ts, ts->ic_grpoffset +
				ts->si_ofs.cmd_ofs, num_read, ic_buf,
				ts->platform_data->addr[CY_TCH_ADDR_OFS], true);
			if (retval < 0)
				goto cyttsp4_ic_grpdata_show_prerr;
		}
		break;
	case CY_IC_GRPNUM_TCH_REP:
		num_read = ts->si_ofs.rep_sz;
		cyttsp4_dbg(ts, CY_DBG_LVL_3,
			"%s: GRP=TCH_REP: num_read=%d at ofs=%d + grpofs%d\n",
			__func__, num_read,
			ts->si_ofs.rep_ofs, ts->ic_grpoffset);
		if (ts->ic_grpoffset >= num_read)
			goto cyttsp4_ic_grpdata_show_ofserr;
		else {
			num_read -= ts->ic_grpoffset;
			retval = cyttsp4_read_block_data(ts, ts->ic_grpoffset +
				ts->si_ofs.rep_ofs, num_read, ic_buf,
				ts->platform_data->addr[CY_TCH_ADDR_OFS], true);
			if (retval < 0)
				goto cyttsp4_ic_grpdata_show_prerr;
		}
		break;
	case CY_IC_GRPNUM_DATA_REC:
		num_read = ts->si_ofs.test_ofs - ts->si_ofs.cydata_ofs;
		cyttsp4_dbg(ts, CY_DBG_LVL_3,
			"%s: GRP=DATA_REC: num_read=%d at ofs=%d + grpofs%d\n",
			__func__, num_read,
			ts->si_ofs.cydata_ofs, ts->ic_grpoffset);
		if (ts->ic_grpoffset >= num_read)
			goto cyttsp4_ic_grpdata_show_ofserr;
		else {
			num_read -= ts->ic_grpoffset;
			retval = _cyttsp4_set_sysinfo_mode(ts);
			if (retval < 0) {
				pr_err("%s: Fail enter Sysinfo mode r=%d\n",
					__func__, retval);
				goto cyttsp4_ic_grpdata_show_data_rderr;
			}
			retval = cyttsp4_read_block_data(ts, ts->ic_grpoffset +
				ts->si_ofs.cydata_ofs, num_read, ic_buf,
				ts->platform_data->addr[CY_TCH_ADDR_OFS], true);
			if (retval < 0) {
				pr_err("%s: Fail read Sysinfo ddata r=%d\n",
					__func__, retval);
				goto cyttsp4_ic_grpdata_show_data_rderr;
			}
			retval = _cyttsp4_set_operational_mode(ts);
			if (retval < 0) {
				pr_err("%s: Fail enter Operational mode r=%d\n",
					__func__, retval);
			}
		}
		break;
cyttsp4_ic_grpdata_show_data_rderr:
		pr_err("%s: Fail read cydata record\n", __func__);
		goto cyttsp4_ic_grpdata_show_prerr;
		break;
	case CY_IC_GRPNUM_TEST_REC:
		num_read = ts->si_ofs.pcfg_ofs - ts->si_ofs.test_ofs;
		cyttsp4_dbg(ts, CY_DBG_LVL_3,
			"%s: GRP=TEST_REC: num_read=%d at ofs=%d + grpofs%d\n",
			__func__, num_read,
			ts->si_ofs.test_ofs, ts->ic_grpoffset);
		if (ts->ic_grpoffset >= num_read)
			goto cyttsp4_ic_grpdata_show_ofserr;
		else {
			num_read -= ts->ic_grpoffset;
			retval = _cyttsp4_set_sysinfo_mode(ts);
			if (retval < 0) {
				pr_err("%s: Fail enter Sysinfo mode r=%d\n",
					__func__, retval);
				goto cyttsp4_ic_grpdata_show_test_rderr;
			}
			retval = cyttsp4_read_block_data(ts, ts->ic_grpoffset +
				ts->si_ofs.test_ofs, num_read, ic_buf,
				ts->platform_data->addr[CY_TCH_ADDR_OFS], true);
			if (retval < 0) {
				pr_err("%s: Fail read Sysinfo ddata r=%d\n",
					__func__, retval);
				goto cyttsp4_ic_grpdata_show_test_rderr;
			}
			retval = _cyttsp4_set_operational_mode(ts);
			if (retval < 0) {
				pr_err("%s: Fail enter Operational mode r=%d\n",
					__func__, retval);
			}
		}
		break;
cyttsp4_ic_grpdata_show_test_rderr:
		pr_err("%s: Fail read test record\n", __func__);
		goto cyttsp4_ic_grpdata_show_prerr;
		break;
	case CY_IC_GRPNUM_PCFG_REC:
		num_read = ts->si_ofs.opcfg_ofs - ts->si_ofs.pcfg_ofs;
		cyttsp4_dbg(ts, CY_DBG_LVL_3,
			"%s: GRP=PCFG_REC: num_read=%d at ofs=%d + grpofs%d\n",
			__func__, num_read,
			ts->si_ofs.pcfg_ofs, ts->ic_grpoffset);
		if (ts->ic_grpoffset >= num_read)
			goto cyttsp4_ic_grpdata_show_ofserr;
		else {
			num_read -= ts->ic_grpoffset;
			retval = _cyttsp4_set_sysinfo_mode(ts);
			if (retval < 0) {
				pr_err("%s: Fail enter Sysinfo mode r=%d\n",
					__func__, retval);
				goto cyttsp4_ic_grpdata_show_pcfg_rderr;
			}
			retval = cyttsp4_read_block_data(ts, ts->ic_grpoffset +
				ts->si_ofs.pcfg_ofs, num_read, ic_buf,
				ts->platform_data->addr[CY_TCH_ADDR_OFS], true);
			if (retval < 0) {
				pr_err("%s: Fail read Sysinfo ddata r=%d\n",
					__func__, retval);
				goto cyttsp4_ic_grpdata_show_pcfg_rderr;
			}
			retval = _cyttsp4_set_operational_mode(ts);
			if (retval < 0) {
				pr_err("%s: Fail enter Operational mode r=%d\n",
					__func__, retval);
			}
		}
		break;
cyttsp4_ic_grpdata_show_pcfg_rderr:
		pr_err("%s: Fail read pcfg record\n", __func__);
		goto cyttsp4_ic_grpdata_show_prerr;
		break;
	case CY_IC_GRPNUM_OPCFG_REC:
		num_read = ts->si_ofs.ddata_ofs - ts->si_ofs.opcfg_ofs;
		cyttsp4_dbg(ts, CY_DBG_LVL_3,
			"%s: GRP=OPCFG_REC: num_read=%d at ofs=%d + grpofs%d\n",
			__func__, num_read,
			ts->si_ofs.opcfg_ofs, ts->ic_grpoffset);
		if (ts->ic_grpoffset >= num_read)
			goto cyttsp4_ic_grpdata_show_ofserr;
		else {
			num_read -= ts->ic_grpoffset;
			retval = _cyttsp4_set_sysinfo_mode(ts);
			if (retval < 0) {
				pr_err("%s: Fail enter Sysinfo mode r=%d\n",
					__func__, retval);
				goto cyttsp4_ic_grpdata_show_opcfg_rderr;
			}
			retval = cyttsp4_read_block_data(ts, ts->ic_grpoffset +
				ts->si_ofs.opcfg_ofs, num_read, ic_buf,
				ts->platform_data->addr[CY_TCH_ADDR_OFS], true);
			if (retval < 0) {
				pr_err("%s: Fail read Sysinfo ddata r=%d\n",
					__func__, retval);
				goto cyttsp4_ic_grpdata_show_opcfg_rderr;
			}
			retval = _cyttsp4_set_operational_mode(ts);
			if (retval < 0) {
				pr_err("%s: Fail enter Operational mode r=%d\n",
					__func__, retval);
			}
		}
		break;
cyttsp4_ic_grpdata_show_opcfg_rderr:
		pr_err("%s: Fail read opcfg record\n", __func__);
		goto cyttsp4_ic_grpdata_show_prerr;
		break;
	case CY_IC_GRPNUM_TCH_PARM_VAL:
		ndata = CY_NUM_CONFIG_BYTES;
		/* do not show cmd, block size and end of block bytes */
		num_read = ndata - (6+4+6);
		cyttsp4_dbg(ts, CY_DBG_LVL_3,
			"%s: GRP=PARM_VAL: num_read=%d at ofs=%d + grpofs%d\n",
			__func__, num_read,
			0, ts->ic_grpoffset);
		if (ts->ic_grpoffset >= num_read)
			goto cyttsp4_ic_grpdata_show_ofserr;
		else {
			blockid = CY_TCH_PARM_BLKID;
			pdata = kzalloc(ndata, GFP_KERNEL);
			if (pdata == NULL) {
				pr_err("%s: Failed to allocate read buffer"
					" for %s\n",
					__func__, "platform_touch_param_data");
				retval = -ENOMEM;
				goto cyttsp4_ic_grpdata_show_tch_rderr;
			}
			cyttsp4_dbg(ts, CY_DBG_LVL_3,
				"%s: read config block=0x%02X\n",
				__func__, blockid);
			retval = _cyttsp4_set_config_mode(ts);
			if (retval < 0) {
				pr_err("%s: Failed to switch to config mode\n",
					__func__);
				goto cyttsp4_ic_grpdata_show_tch_rderr;
			}
			retval = _cyttsp4_read_config_block(ts,
				blockid, pdata, ndata,
				"platform_touch_param_data");
			if (retval < 0) {
				pr_err("%s: Failed read config block %s r=%d\n",
					__func__, "platform_touch_param_data",
					retval);
				goto cyttsp4_ic_grpdata_show_tch_rderr;
			}
			retval = _cyttsp4_set_operational_mode(ts);
			if (retval < 0) {
				ts->power_state = CY_IDLE_STATE;
				cyttsp4_pr_state(ts);
				pr_err("%s: Fail set operational mode (r=%d)\n",
					__func__, retval);
				goto cyttsp4_ic_grpdata_show_tch_rderr;
			}
			cyttsp4_dbg(ts, CY_DBG_LVL_3,
				"%s: memcpy config block=0x%02X\n",
				__func__, blockid);
			num_read -= ts->ic_grpoffset;
			/*
			 * cmd+rdy_bit, status, ebid, lenh, lenl, reserved,
			 * data[0] .. data[ndata-6]
			 * skip data[0] .. data[3] - block size bytes
			 */
			memcpy(ic_buf,
				&pdata[6+4] + ts->ic_grpoffset, num_read);
			kfree(pdata);
		}
		break;
cyttsp4_ic_grpdata_show_tch_rderr:
		kfree(pdata);
		goto cyttsp4_ic_grpdata_show_prerr;
		break;
	case CY_IC_GRPNUM_TCH_PARM_SIZ:
		if (ts->platform_data->sett
			[CY_IC_GRPNUM_TCH_PARM_SIZ] == NULL) {
			pr_err("%s: Missing platform data"
				" Touch Parameters Sizes table\n", __func__);
			goto cyttsp4_ic_grpdata_show_prerr;
		}
		if (ts->platform_data->sett
			[CY_IC_GRPNUM_TCH_PARM_SIZ]->data == NULL) {
			pr_err("%s: Missing platform data"
				" Touch Parameters Sizes table data\n",
				__func__);
			goto cyttsp4_ic_grpdata_show_prerr;
		}
		num_read = ts->platform_data->sett
			[CY_IC_GRPNUM_TCH_PARM_SIZ]->size;
		cyttsp4_dbg(ts, CY_DBG_LVL_2,
			"%s: GRP=PARM_SIZ: num_read=%d at ofs=%d + grpofs%d\n",
			__func__, num_read,
			0, ts->ic_grpoffset);
		if (ts->ic_grpoffset >= num_read)
			goto cyttsp4_ic_grpdata_show_ofserr;
		else {
			num_read -= ts->ic_grpoffset;
			memcpy(ic_buf, (u8 *)ts->platform_data->sett
				[CY_IC_GRPNUM_TCH_PARM_SIZ]->data +
				ts->ic_grpoffset, num_read);
		}
		break;
	case CY_IC_GRPNUM_DDATA_REC:
		num_read = ts->si_ofs.mdata_ofs - ts->si_ofs.ddata_ofs;
		cyttsp4_dbg(ts, CY_DBG_LVL_3,
			"%s: GRP=DDATA_REC: num_read=%d at ofs=%d + grpofs%d\n",
			__func__, num_read,
			ts->si_ofs.ddata_ofs, ts->ic_grpoffset);
		if (ts->ic_grpoffset >= num_read)
			goto cyttsp4_ic_grpdata_show_ofserr;
		else {
			num_read -= ts->ic_grpoffset;
			retval = _cyttsp4_set_sysinfo_mode(ts);
			if (retval < 0) {
				pr_err("%s: Fail enter Sysinfo mode r=%d\n",
					__func__, retval);
				goto cyttsp4_ic_grpdata_show_ddata_rderr;
			}
			retval = cyttsp4_read_block_data(ts, ts->ic_grpoffset +
				ts->si_ofs.ddata_ofs, num_read, ic_buf,
				ts->platform_data->addr[CY_TCH_ADDR_OFS], true);
			if (retval < 0) {
				pr_err("%s: Fail read Sysinfo ddata r=%d\n",
					__func__, retval);
				goto cyttsp4_ic_grpdata_show_ddata_rderr;
			}
			retval = _cyttsp4_set_operational_mode(ts);
			if (retval < 0) {
				pr_err("%s: Fail enter Operational mode r=%d\n",
					__func__, retval);
			}
		}
		break;
cyttsp4_ic_grpdata_show_ddata_rderr:
		pr_err("%s: Fail read ddata\n", __func__);
		goto cyttsp4_ic_grpdata_show_prerr;
		break;
	case CY_IC_GRPNUM_MDATA_REC:
		num_read = ts->si_ofs.map_sz - ts->si_ofs.mdata_ofs;
		cyttsp4_dbg(ts, CY_DBG_LVL_3,
			"%s: GRP=MDATA_REC: num_read=%d at ofs=%d + grpofs%d\n",
			__func__, num_read,
			ts->si_ofs.mdata_ofs, ts->ic_grpoffset);
		if (ts->ic_grpoffset >= num_read)
			goto cyttsp4_ic_grpdata_show_ofserr;
		else {
			num_read -= ts->ic_grpoffset;
			retval = _cyttsp4_set_sysinfo_mode(ts);
			if (retval < 0) {
				pr_err("%s: Fail enter Sysinfo mode r=%d\n",
					__func__, retval);
				goto cyttsp4_ic_grpdata_show_mdata_rderr;
			}
			retval = cyttsp4_read_block_data(ts, ts->ic_grpoffset +
				ts->si_ofs.mdata_ofs, num_read, ic_buf,
				ts->platform_data->addr[CY_TCH_ADDR_OFS], true);
			if (retval < 0) {
				pr_err("%s: Fail read Sysinfo regs r=%d\n",
					__func__, retval);
				goto cyttsp4_ic_grpdata_show_mdata_rderr;
			}
			retval = _cyttsp4_set_operational_mode(ts);
			if (retval < 0) {
				pr_err("%s: Fail enter Operational mode r=%d\n",
					__func__, retval);
			}
		}
		break;
cyttsp4_ic_grpdata_show_mdata_rderr:
		pr_err("%s: Fail read mdata\n", __func__);
		goto cyttsp4_ic_grpdata_show_prerr;
		break;
	default:
		goto cyttsp4_ic_grpdata_show_grperr;
		break;
	}

	snprintf(buf, CY_MAX_PRBUF_SIZE,
		"Group %d, Offset %u:\n", ts->ic_grpnum, ts->ic_grpoffset);
	for (i = 0; i < num_read; i++) {
		snprintf(buf, CY_MAX_PRBUF_SIZE,
			"%s0x%02X\n", buf, ic_buf[i]);
	}
	kfree(ic_buf);
	return snprintf(buf, CY_MAX_PRBUF_SIZE,
		"%s(%d bytes)\n", buf, num_read);

cyttsp4_ic_grpdata_show_ofserr:
	pr_err("%s: Group Offset=%d exceeds Group Read Length=%d\n",
		__func__, ts->ic_grpoffset, num_read);
	kfree(ic_buf);
	snprintf(buf, CY_MAX_PRBUF_SIZE,
		"Cannot read Group %d Data.\n",
		ts->ic_grpnum);
	return snprintf(buf, CY_MAX_PRBUF_SIZE,
		"%sGroup Offset=%d exceeds Group Read Length=%d\n",
		buf, ts->ic_grpoffset, num_read);
cyttsp4_ic_grpdata_show_prerr:
	pr_err("%s: Cannot read Group %d Data.\n",
		__func__, ts->ic_grpnum);
	kfree(ic_buf);
	return snprintf(buf, CY_MAX_PRBUF_SIZE,
		"Cannot read Group %d Data.\n",
		ts->ic_grpnum);
cyttsp4_ic_grpdata_show_grperr:
	pr_err("%s: Group %d does not exist.\n",
		__func__, ts->ic_grpnum);
	kfree(ic_buf);
	return snprintf(buf, CY_MAX_PRBUF_SIZE,
		"Group %d does not exist.\n",
		ts->ic_grpnum);
}
static ssize_t cyttsp4_ic_grpdata_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct cyttsp4 *ts = dev_get_drvdata(dev);
	ssize_t retval = 0;

	mutex_lock(&ts->data_lock);
	if (ts->power_state == CY_SLEEP_STATE) {
		pr_err("%s: Group Show Test blocked: IC suspended\n", __func__);
		retval = snprintf(buf, CY_MAX_PRBUF_SIZE,
			"Group %d Show Test blocked: IC suspended\n",
			ts->ic_grpnum);
	} else
		retval = _cyttsp4_ic_grpdata_show(dev, attr, buf);
	mutex_unlock(&ts->data_lock);

	return retval;
}

static int _cyttsp4_write_mddata(struct cyttsp4 *ts, size_t write_length,
	size_t mddata_length, u8 blkid, size_t mddata_ofs,
	u8 *ic_buf, const char *mddata_name)
{
	bool mddata_updated = false;
	u8 *pdata;
	int retval = 0;

	pdata = kzalloc(CY_MAX_PRBUF_SIZE, GFP_KERNEL);
	if (pdata == NULL) {
		pr_err("%s: Fail allocate data buffer\n", __func__);
		retval = -ENOMEM;
		goto cyttsp4_write_mddata_exit;
	}
	if (ts->current_mode != CY_MODE_OPERATIONAL) {
		pr_err("%s: Must be in operational mode to start write of"
			" %s (current mode=%d)\n",
			__func__, mddata_name, ts->current_mode);
		retval = -EPERM;
		goto cyttsp4_write_mddata_exit;
	}
	if ((write_length + ts->ic_grpoffset) > mddata_length) {
		pr_err("%s: Requested length(%d) is greater than"
			" %s size(%d)\n", __func__,
			write_length, mddata_name, mddata_length);
		retval = -EINVAL;
		goto cyttsp4_write_mddata_exit;
	}
	retval = _cyttsp4_set_sysinfo_mode(ts);
	if (retval < 0) {
		pr_err("%s: Fail to enter Sysinfo mode r=%d\n",
			__func__, retval);
		goto cyttsp4_write_mddata_exit;
	}
	cyttsp4_dbg(ts, CY_DBG_LVL_3,
		"%s: blkid=%02X mddata_ofs=%d mddata_length=%d"
		" mddata_name=%s write_length=%d grpofs=%d\n",
		__func__, blkid, mddata_ofs, mddata_length, mddata_name,
		write_length, ts->ic_grpoffset);
	cyttsp4_read_block_data(ts, mddata_ofs, mddata_length, pdata,
		ts->platform_data->addr[CY_TCH_ADDR_OFS], true);
	if (retval < 0) {
		pr_err("%s: Fail to read %s regs r=%d\n",
			__func__, mddata_name, retval);
		goto cyttsp4_write_mddata_exit;
	}
	memcpy(pdata + ts->ic_grpoffset, ic_buf, write_length);
	_cyttsp4_set_data_block(ts, blkid, pdata,
		mddata_length, mddata_name, true, &mddata_updated);
	if ((retval < 0) || !mddata_updated) {
		pr_err("%s: Fail while writing %s block r=%d updated=%d\n",
			__func__, mddata_name, retval, (int)mddata_updated);
	}
	retval = _cyttsp4_set_operational_mode(ts);
	if (retval < 0) {
		pr_err("%s: Fail to enter Operational mode r=%d\n",
			__func__, retval);
	}

cyttsp4_write_mddata_exit:
	kfree(pdata);
	return retval;
}
static ssize_t _cyttsp4_ic_grpdata_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct cyttsp4 *ts = dev_get_drvdata(dev);
	unsigned long value;
	int retval = 0;
	const char *pbuf = buf;
	int i, j;
	char *scan_buf = NULL;
	u8 *ic_buf = NULL;
	u8 *pdata = NULL;
	size_t length;
	size_t mddata_length, ndata;
	u8 blockid = 0;
	bool mddata_updated = false;
	const char *mddata_name = "invalid name";

	scan_buf = kzalloc(CY_MAX_PRBUF_SIZE, GFP_KERNEL);
	if (scan_buf == NULL) {
		pr_err("%s: Failed to allocate scan buffer for"
			" Group Data store\n", __func__);
		goto cyttsp4_ic_grpdata_store_exit;
	}
	ic_buf = kzalloc(CY_MAX_PRBUF_SIZE, GFP_KERNEL);
	if (ic_buf == NULL) {
		pr_err("%s: Failed to allocate ic buffer for"
			" Group Data store\n", __func__);
		goto cyttsp4_ic_grpdata_store_exit;
	}
	cyttsp4_dbg(ts, CY_DBG_LVL_3,
		"%s: grpnum=%d grpoffset=%u\n",
		__func__, ts->ic_grpnum, ts->ic_grpoffset);

	if (ts->ic_grpnum >= CY_IC_GRPNUM_NUM) {
		pr_err("%s: Group %d does not exist.\n",
			__func__, ts->ic_grpnum);
		retval = size;
		goto cyttsp4_ic_grpdata_store_exit;
	}
	cyttsp4_dbg(ts, CY_DBG_LVL_3,
		"%s: pbuf=%p buf=%p size=%d sizeof(scan_buf)=%d buf=%s\n",
		__func__, pbuf, buf, size, sizeof(scan_buf), buf);

	i = 0;
	while (pbuf <= (buf + size)) {
		while (((*pbuf == ' ') || (*pbuf == ',')) &&
			(pbuf < (buf + size)))
			pbuf++;
		if (pbuf < (buf + size)) {
			memset(scan_buf, 0, CY_MAX_PRBUF_SIZE);
			for (j = 0; j < sizeof("0xHH") &&
				*pbuf != ' ' && *pbuf != ','; j++)
				scan_buf[j] = *pbuf++;
			retval = strict_strtoul(scan_buf, 16, &value);
			if (retval < 0) {
				pr_err("%s: Invalid data format. "
					"Use \"0xHH,...,0xHH\" instead.\n",
					__func__);
				retval = size;
				goto cyttsp4_ic_grpdata_store_exit;
			} else {
				if (i >= ts->max_config_bytes) {
					pr_err("%s: Max command size exceeded"
					" (size=%d max=%d)\n", __func__,
					i, ts->max_config_bytes);
					goto cyttsp4_ic_grpdata_store_exit;
				}
				ic_buf[i] = value;
				cyttsp4_dbg(ts, CY_DBG_LVL_3,
					"%s: ic_buf[%d] = 0x%02X\n",
					__func__, i, ic_buf[i]);
				i++;
			}
		} else
			break;
	}
	length = i;

	/* write ic_buf to log */
	_cyttsp4_pr_buf(ts, ic_buf, length, "ic_buf");

	switch (ts->ic_grpnum) {
	case CY_IC_GRPNUM_CMD_REGS:
		if ((length + ts->ic_grpoffset + ts->si_ofs.cmd_ofs) >
			ts->si_ofs.rep_ofs) {
			pr_err("%s: Length(%d) + offset(%d) + cmd_offset(%d)"
				" is beyond cmd reg space[%d..%d]\n", __func__,
				length, ts->ic_grpoffset, ts->si_ofs.cmd_ofs,
				ts->si_ofs.cmd_ofs, ts->si_ofs.rep_ofs - 1);
			goto cyttsp4_ic_grpdata_store_exit;
		}
		retval = cyttsp4_write_block_data(ts, ts->ic_grpoffset +
			ts->si_ofs.cmd_ofs, length, ic_buf,
			ts->platform_data->addr[CY_TCH_ADDR_OFS], true);
		if (retval < 0) {
			pr_err("%s: Fail write command regs r=%d\n",
				__func__, retval);
		}
		if (!ts->ic_grptest) {
			pr_info("%s: Disabled settings checksum verifications"
				" until next boot.\n", __func__);
			ts->ic_grptest = true;
		}
		break;
	case CY_IC_GRPNUM_TCH_PARM_VAL:
		mddata_name = "Touch Parameters";
		ndata = CY_NUM_CONFIG_BYTES;
		blockid = CY_TCH_PARM_BLKID;
		/* do not show cmd, block size and end of block bytes */
		mddata_length = ndata - (6+4+6);
		cyttsp4_dbg(ts, CY_DBG_LVL_3,
			"%s: GRP=PARM_VAL: write length=%d at ofs=%d +"
			" grpofs=%d\n", __func__, length,
			0, ts->ic_grpoffset);
		if ((length + ts->ic_grpoffset) > mddata_length) {
			pr_err("%s: Requested length(%d) is greater than"
				" %s size(%d)\n", __func__,
				length, mddata_name, mddata_length);
			retval = -EINVAL;
			goto cyttsp4_ic_grpdata_store_tch_wrerr;
		}
		pdata = kzalloc(ndata, GFP_KERNEL);
		if (pdata == NULL) {
			pr_err("%s: Failed to allocate read/write buffer"
				" for %s\n",
				__func__, "platform_touch_param_data");
			retval = -ENOMEM;
			goto cyttsp4_ic_grpdata_store_tch_wrerr;
		}
		cyttsp4_dbg(ts, CY_DBG_LVL_3,
			"%s: read config block=0x%02X\n",
			__func__, blockid);
		retval = _cyttsp4_set_config_mode(ts);
		if (retval < 0) {
			pr_err("%s: Failed to switch to config mode\n",
				__func__);
			goto cyttsp4_ic_grpdata_store_tch_wrerr;
		}
		retval = _cyttsp4_read_config_block(ts,
			blockid, pdata, ndata,
			"platform_touch_param_data");
		if (retval < 0) {
			pr_err("%s: Failed read config block %s r=%d\n",
				__func__, "platform_touch_param_data",
				retval);
			goto cyttsp4_ic_grpdata_store_tch_wrerr;
		}
		/*
		 * cmd+rdy_bit, status, ebid, lenh, lenl, reserved,
		 * data[0] .. data[ndata-6]
		 * skip data[0] .. data[3] - block size bytes
		 */
		memcpy(&pdata[6+4+ts->ic_grpoffset], ic_buf, length);
		_cyttsp4_set_data_block(ts, blockid, &pdata[6+4],
			mddata_length, mddata_name, true, &mddata_updated);
		if ((retval < 0) || !mddata_updated) {
			pr_err("%s: Fail while writing %s block r=%d"
				" updated=%d\n", __func__,
				mddata_name, retval, (int)mddata_updated);
		}
		if (!ts->ic_grptest) {
			pr_info("%s: Disabled settings checksum verifications"
				" until next boot.\n", __func__);
			ts->ic_grptest = true;
		}
		retval = _cyttsp4_startup(ts);
		if (retval < 0) {
			pr_err("%s: Fail restart after writing params r=%d\n",
				__func__, retval);
		}
cyttsp4_ic_grpdata_store_tch_wrerr:
		kfree(pdata);
		break;
	case CY_IC_GRPNUM_DDATA_REC:
		mddata_length =
			ts->si_ofs.mdata_ofs - ts->si_ofs.ddata_ofs;
		cyttsp4_dbg(ts, CY_DBG_LVL_3,
			"%s: DDATA_REC length=%d mddata_length=%d blkid=%02X"
			" ddata_ofs=%d name=%s\n", __func__, length,
			mddata_length, CY_DDATA_BLKID, ts->si_ofs.ddata_ofs,
			"Design Data");
		_cyttsp4_pr_buf(ts, ic_buf, length, "Design Data");
		retval = _cyttsp4_write_mddata(ts, length, mddata_length,
			CY_DDATA_BLKID, ts->si_ofs.ddata_ofs, ic_buf,
			"Design Data");
		if (retval < 0) {
			pr_err("%s: Fail writing Design Data\n",
				__func__);
		} else if (!ts->ic_grptest) {
			pr_info("%s: Disabled settings checksum verifications"
				" until next boot.\n", __func__);
			ts->ic_grptest = true;
		}
		break;
	case CY_IC_GRPNUM_MDATA_REC:
		mddata_length =
			ts->si_ofs.map_sz - ts->si_ofs.mdata_ofs;
		cyttsp4_dbg(ts, CY_DBG_LVL_3,
			"%s: MDATA_REC length=%d mddata_length=%d blkid=%02X"
			" ddata_ofs=%d name=%s\n", __func__, length,
			mddata_length, CY_MDATA_BLKID, ts->si_ofs.mdata_ofs,
			"Manufacturing Data");
		_cyttsp4_pr_buf(ts, ic_buf, length, "Manufacturing Data");
		retval = _cyttsp4_write_mddata(ts, length, mddata_length,
			CY_MDATA_BLKID, ts->si_ofs.mdata_ofs, ic_buf,
			"Manufacturing Data");
		if (retval < 0) {
			pr_err("%s: Fail writing Manufacturing Data\n",
				__func__);
		} else if (!ts->ic_grptest) {
			pr_info("%s: Disabled settings checksum verifications"
				" until next boot.\n", __func__);
			ts->ic_grptest = true;
		}
		break;
	default:
		pr_err("%s: Group=%d is read only\n",
			__func__, ts->ic_grpnum);
		break;
	}

cyttsp4_ic_grpdata_store_exit:
	kfree(scan_buf);
	kfree(ic_buf);
	return size;
}
static ssize_t cyttsp4_ic_grpdata_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct cyttsp4 *ts = dev_get_drvdata(dev);
	ssize_t retval = 0;

	mutex_lock(&ts->data_lock);
	if (ts->power_state == CY_SLEEP_STATE) {
		pr_err("%s: Group Store Test blocked: IC suspended\n",
			__func__);
		retval = size;
	} else
		retval = _cyttsp4_ic_grpdata_store(dev, attr, buf, size);
	mutex_unlock(&ts->data_lock);

	return retval;
}
static DEVICE_ATTR(ic_grpdata, S_IRUSR | S_IWUSR,
	cyttsp4_ic_grpdata_show, cyttsp4_ic_grpdata_store);

static ssize_t cyttsp4_drv_flags_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct cyttsp4 *ts = dev_get_drvdata(dev);

	return snprintf(buf, CY_MAX_PRBUF_SIZE,
		"Current Driver Flags: 0x%04X\n", ts->flags);
}
static ssize_t cyttsp4_drv_flags_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct cyttsp4 *ts = dev_get_drvdata(dev);
	unsigned long value = 0;
	ssize_t retval = 0;

	mutex_lock(&(ts->data_lock));
	retval = strict_strtoul(buf, 16, &value);
	if (retval < 0) {
		pr_err("%s: Failed to convert value\n", __func__);
		goto cyttsp4_drv_flags_store_error_exit;
	}

	if (value > 0xFFFF) {
		pr_err("%s: value=%lu is greater than max;"
			" drv_flags=0x%04X\n", __func__, value, ts->flags);
	} else {
		ts->flags = value;
	}

	cyttsp4_dbg(ts, CY_DBG_LVL_3,
		"%s: drv_flags=0x%04X\n", __func__, ts->flags);

cyttsp4_drv_flags_store_error_exit:
	retval = size;
	mutex_unlock(&(ts->data_lock));
	return retval;
}
static DEVICE_ATTR(drv_flags, S_IRUSR | S_IWUSR,
	cyttsp4_drv_flags_show, cyttsp4_drv_flags_store);

static ssize_t cyttsp4_hw_reset_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct cyttsp4 *ts = dev_get_drvdata(dev);
	ssize_t retval = 0;

	mutex_lock(&(ts->data_lock));
	retval = _cyttsp4_startup(ts);
	mutex_unlock(&(ts->data_lock));
	if (retval < 0) {
		pr_err("%s: fail hw_reset device restart r=%d\n",
			__func__, retval);
	}

	retval = size;
	return retval;
}
static DEVICE_ATTR(hw_reset, S_IWUSR, NULL, cyttsp4_hw_reset_store);

static ssize_t cyttsp4_hw_recov_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct cyttsp4 *ts = dev_get_drvdata(dev);
	unsigned long value = 0;
	ssize_t retval = 0;

	mutex_lock(&(ts->data_lock));
	retval = strict_strtoul(buf, 10, &value);
	if (retval < 0) {
		pr_err("%s: Failed to convert value\n", __func__);
		goto cyttsp4_hw_recov_store_error_exit;
	}

	if (ts->platform_data->hw_recov == NULL) {
		pr_err("%s: no hw_recov function\n", __func__);
		goto cyttsp4_hw_recov_store_error_exit;
	}

	retval = ts->platform_data->hw_recov((int)value);
	if (retval < 0) {
		pr_err("%s: fail hw_recov(value=%d) function r=%d\n",
			__func__, (int)value, retval);
	}

cyttsp4_hw_recov_store_error_exit:
	retval = size;
	mutex_unlock(&(ts->data_lock));
	return retval;
}
static DEVICE_ATTR(hw_recov, S_IWUSR, NULL, cyttsp4_hw_recov_store);
#endif

#define CY_CMD_I2C_ADDR					0
#define CY_STATUS_SIZE_BYTE				1
#define CY_STATUS_TYP_DELAY				2
#define CY_CMD_TAIL_LEN					3
#define CY_CMD_BYTE					1
#define CY_STATUS_BYTE					1
#define CY_MAX_STATUS_SIZE				32
#define CY_MIN_STATUS_SIZE				5
#define CY_START_OF_PACKET				0x01
#define CY_END_OF_PACKET				0x17
#define CY_DATA_ROW_SIZE				288
#define CY_PACKET_DATA_LEN				96
#define CY_MAX_PACKET_LEN				512
#define CY_COMM_BUSY					0xFF
#define CY_CMD_BUSY					0xFE
#define CY_SEPARATOR_OFFSET				0
#define CY_ARRAY_ID_OFFSET				0
#define CY_ROW_NUM_OFFSET				1
#define CY_ROW_SIZE_OFFSET				3
#define CY_ROW_DATA_OFFSET				5
#define CY_FILE_SILICON_ID_OFFSET			0
#define CY_FILE_REV_ID_OFFSET				4
#define CY_CMD_LDR_ENTER				0x38
#define CY_CMD_LDR_ENTER_STAT_SIZE			15
#define CY_CMD_LDR_ENTER_DLY				20
#define CY_CMD_LDR_ERASE_ROW				0x34
#define CY_CMD_LDR_ERASE_ROW_STAT_SIZE			7
#define CY_CMD_LDR_ERASE_ROW_DLY			20
#define CY_CMD_LDR_SEND_DATA				0x37
#define CY_CMD_LDR_SEND_DATA_STAT_SIZE			7
#define CY_CMD_LDR_SEND_DATA_DLY			5
#define CY_CMD_LDR_PROG_ROW				0x39
#define CY_CMD_LDR_PROG_ROW_STAT_SIZE			7
#define CY_CMD_LDR_PROG_ROW_DLY				30
#define CY_CMD_LDR_VERIFY_ROW				0x3A
#define CY_CMD_LDR_VERIFY_ROW_STAT_SIZE			8
#define CY_CMD_LDR_VERIFY_ROW_DLY			20
#define CY_CMD_LDR_VERIFY_CHKSUM			0x31
#define CY_CMD_LDR_VERIFY_CHKSUM_STAT_SIZE		8
#define CY_CMD_LDR_VERIFY_CHKSUM_DLY			20
#define CY_CMD_LDR_EXIT					0x3B
#define CY_CMD_LDR_EXIT_STAT_SIZE			7
#define CY_CMD_LDR_EXIT_DLY				0
/* {i2c_addr, status_size, status delay, cmd...} */
static u8 ldr_enter_cmd[] = {
	0x01, CY_CMD_LDR_ENTER,
	0x00, 0x00, 0xC7, 0xFF, 0x17
};
static u8 ldr_erase_row_cmd[] = {
	0x01, CY_CMD_LDR_ERASE_ROW,
	0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x17
};
static u8 ldr_send_data_cmd[] = {
	0x01, CY_CMD_LDR_SEND_DATA
};
static u8 ldr_prog_row_cmd[] = {
	0x01, CY_CMD_LDR_PROG_ROW
};
static u8 ldr_verify_row_cmd[] = {
	0x01, CY_CMD_LDR_VERIFY_ROW,
	0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x17
};
static u8 ldr_verify_chksum_cmd[] = {
	0x01, CY_CMD_LDR_VERIFY_CHKSUM,
	0x00, 0x00, 0xCE, 0xFF, 0x17
};
static u8 ldr_exit_cmd[] = {
	0x01, CY_CMD_LDR_EXIT,
	0x00, 0x00, 0xC4, 0xFF, 0x17
};

enum ldr_status {
	ERROR_SUCCESS = 0,
	ERROR_COMMAND = 1,
	ERROR_FLASH_ARRAY = 2,
	ERROR_PACKET_DATA = 3,
	ERROR_PACKET_LEN = 4,
	ERROR_PACKET_CHECKSUM = 5,
	ERROR_FLASH_PROTECTION = 6,
	ERROR_FLASH_CHECKSUM = 7,
	ERROR_VERIFY_IMAGE = 8,
	ERROR_UKNOWN1 = 9,
	ERROR_UKNOWN2 = 10,
	ERROR_UKNOWN3 = 11,
	ERROR_UKNOWN4 = 12,
	ERROR_UKNOWN5 = 13,
	ERROR_UKNOWN6 = 14,
	ERROR_INVALID_COMMAND = 15,
	ERROR_INVALID
};

#ifdef CONFIG_TOUCHSCREEN_DEBUG
static const char * const ldr_status_string[] = {
	/* Order must match enum ldr_status above */
	"Error Success",
	"Error Command",
	"Error Flash Array",
	"Error Packet Data",
	"Error Packet Length",
	"Error Packet Checksum",
	"Error Flash Protection",
	"Error Flash Checksum",
	"Error Verify Image",
	"Error Invalid Command",
	"Error Invalid Command",
	"Error Invalid Command",
	"Error Invalid Command",
	"Error Invalid Command",
	"Error Invalid Command",
	"Error Invalid Command",
	"Invalid Error Code"
};
#endif

#ifdef CONFIG_TOUCHSCREEN_DEBUG
static void cyttsp4_pr_status(struct cyttsp4 *ts, int level, int status)
{
	if (status > ERROR_INVALID)
		status = ERROR_INVALID;
	cyttsp4_dbg(ts, level,
		"%s: status error(%d)=%s\n",
		__func__, status, ldr_status_string[status]);
}
#endif

static u16 cyttsp4_get_short(u8 *buf)
{
	return ((u16)(*buf) << 8) + *(buf+1);
}

static u8 *cyttsp4_get_row(struct cyttsp4 *ts,
	u8 *row_buf, u8 *image_buf, int size)
{
	int i;
	for (i = 0; i < size; i++) {
		/* copy a row from the image */
		row_buf[i] = image_buf[i];
	}

	image_buf = image_buf + size;
	return image_buf;
}

static u16 cyttsp4_compute_crc(struct cyttsp4 *ts, u8 *buf, int size)
{
	u16 crc = 0xffff;
	u16 tmp;
	int i;

	/* RUN CRC */

	if (size == 0) {
		crc = ~crc;
		goto cyttsp4_compute_crc_exit;
	}

	do {
		for (i = 0, tmp = 0x00ff & *buf++; i < 8;
			i++, tmp >>= 1) {
			if ((crc & 0x0001) ^ (tmp & 0x0001))
				crc = (crc >> 1) ^ 0x8408;
			else
				crc >>= 1;
		}
	} while (--size);

	crc = ~crc;
	tmp = crc;
	crc = (crc << 8) | (tmp >> 8 & 0xFF);

cyttsp4_compute_crc_exit:
	return crc;
}

static u8 ok_status[] = {
	CY_START_OF_PACKET, ERROR_SUCCESS,
	0, 0, 0xFF, 0xFF, CY_END_OF_PACKET
};

static int cyttsp4_get_status(struct cyttsp4 *ts, u8 *buf, int size, int delay)
{
	int tries;
	u16 crc = 0;
	int retval = 0;

	if (!delay) {
		memcpy(buf, ok_status, sizeof(ok_status));
		crc = cyttsp4_compute_crc(ts, ldr_enter_cmd,
			sizeof(ldr_enter_cmd) - CY_CMD_TAIL_LEN);
		ok_status[4] = (u8)crc;
		ok_status[5] = (u8)(crc >> 8);
		retval = 0;
		goto cyttsp4_get_status_exit;
	}

	/* wait until status ready interrupt or timeout occurs */
	tries = 0;
	while (!ts->bl_ready_flag && (tries-- < (delay*2))) {
		if (mutex_is_locked(&ts->data_lock)) {
			mutex_unlock(&ts->data_lock);
			udelay(1000);
			mutex_lock(&ts->data_lock);
		} else
			udelay(1000);
	};

	/* read the status packet */
	retval = cyttsp4_read_block_data(ts, CY_REG_BASE,
		size, buf, ts->platform_data->addr[CY_LDR_ADDR_OFS], false);
	/* retry if bus read error or status byte shows not ready */
	tries = 0;
	while (((retval < 0) ||
		(!(retval < 0) &&
		((buf[1] == CY_COMM_BUSY) ||
		(buf[1] == CY_CMD_BUSY)))) &&
		(tries++ < CY_DELAY_DFLT)) {
		mdelay(delay);
		retval = cyttsp4_read_block_data(ts, CY_REG_BASE,
			size, buf,
			ts->platform_data->addr[CY_LDR_ADDR_OFS], false);
	}
	cyttsp4_dbg(ts, CY_DBG_LVL_3,
		"%s: tries=%d ret=%d status=%02X\n",
		__func__, tries, retval, buf[1]);

cyttsp4_get_status_exit:
	return retval;
}

static u8 *cyttsp4_send_cmd(struct cyttsp4 *ts,	const u8 *cmd_buf, int cmd_size,
	int status_size, int status_delay)
{
	int retval;
	u8 *status_buf = kzalloc(CY_MAX_STATUS_SIZE, GFP_KERNEL);
	if (status_buf == NULL) {
		pr_err("%s: Fail alloc status buffer=%p\n",
			__func__, status_buf);
		goto cyttsp4_send_cmd_exit;
	}

	cyttsp4_dbg(ts, CY_DBG_LVL_3,
		"%s: cmd=%02X %02X %02X %02X %02X\n", __func__,
		cmd_buf[0], cmd_buf[1], cmd_buf[2], cmd_buf[3], cmd_buf[4]);

	if (!cmd_size) {
		pr_err("%s: bad command size=%d\n", __func__, cmd_size);
		goto cyttsp4_send_cmd_exit;
	}

	/* write the command */
	ts->bl_ready_flag = false;
	retval = cyttsp4_write_block_data(ts, CY_REG_BASE,
		cmd_size, cmd_buf,
		ts->platform_data->addr[CY_LDR_ADDR_OFS], false);
	if (retval < 0) {
		pr_err("%s: Fail writing command=%02X\n",
			__func__, cmd_buf[CY_CMD_BYTE]);
		kfree(status_buf);
		status_buf = NULL;
		goto cyttsp4_send_cmd_exit;
	}

	/* get the status */
	retval = cyttsp4_get_status(ts, status_buf, status_size, status_delay);
	if ((retval < 0) || (status_buf[0] != CY_START_OF_PACKET)) {
		pr_err("%s: Error getting status r=%d status_buf[0]=%02X\n",
			__func__, retval, status_buf[0]);
		kfree(status_buf);
		status_buf = NULL;
		goto cyttsp4_send_cmd_exit;
	}

cyttsp4_send_cmd_exit:
	return status_buf;
}

struct cyttsp4_dev_id {
	u32 silicon_id;
	u8 rev_id;
	u32 bl_ver;
};

static int cyttsp4_ldr_enter(struct cyttsp4 *ts, struct cyttsp4_dev_id *dev_id)
{
	u16 crc;
	u8 status_buf[CY_MAX_STATUS_SIZE];
	u8 status = 0;
	int retval = 0;

	memset(status_buf, 0, sizeof(status_buf));
	dev_id->bl_ver = 0;
	dev_id->rev_id = 0;
	dev_id->silicon_id = 0;

	/* write the command */
	crc = cyttsp4_compute_crc(ts, ldr_enter_cmd,
		sizeof(ldr_enter_cmd) - CY_CMD_TAIL_LEN);
	ldr_enter_cmd[4] = (u8)crc;
	ldr_enter_cmd[5] = (u8)(crc >> 8);
	ts->bl_ready_flag = false;
	retval = cyttsp4_write_block_data(ts, CY_REG_BASE,
		sizeof(ldr_enter_cmd), ldr_enter_cmd,
		ts->platform_data->addr[CY_LDR_ADDR_OFS], false);
	if (retval < 0) {
		pr_err("%s: write block failed %d\n", __func__, retval);
		goto cyttsp4_ldr_enter_exit;
	}

	retval = cyttsp4_get_status(ts, status_buf,
		CY_CMD_LDR_ENTER_STAT_SIZE, CY_CMD_LDR_ENTER_DLY);

	if (retval < 0) {
		pr_err("%s: Fail get status to Enter Loader command r=%d\n",
			__func__, retval);
	} else {
		status = status_buf[CY_STATUS_BYTE];
		if (status == ERROR_SUCCESS) {
			dev_id->bl_ver =
				status_buf[11] << 16 |
				status_buf[10] <<  8 |
				status_buf[9] <<  0;
			dev_id->rev_id =
				status_buf[8] <<  0;
			dev_id->silicon_id =
				status_buf[7] << 24 |
				status_buf[6] << 16 |
				status_buf[5] <<  8 |
				status_buf[4] <<  0;
		}
#ifdef CONFIG_TOUCHSCREEN_DEBUG
		cyttsp4_pr_status(ts, CY_DBG_LVL_3, status);
#endif
		cyttsp4_dbg(ts, CY_DBG_LVL_3,
			"%s: status=%d "
			"bl_ver=%08X rev_id=%02X silicon_id=%08X\n",
			__func__, status,
			dev_id->bl_ver, dev_id->rev_id, dev_id->silicon_id);
		retval = status;
	}

cyttsp4_ldr_enter_exit:
	return retval;
}

struct cyttsp4_hex_image {
	u8 array_id;
	u16 row_num;
	u16 row_size;
	u8 row_data[CY_DATA_ROW_SIZE];
} __packed;

static int cyttsp4_ldr_erase_row(struct cyttsp4 *ts,
	struct cyttsp4_hex_image *row_image)
{
	u16 crc;
	u8 *status_buf;
	u8 status;
	int retval = 0;

	ldr_erase_row_cmd[4] = row_image->array_id;
	ldr_erase_row_cmd[5] = (u8)row_image->row_num;
	ldr_erase_row_cmd[6] = (u8)(row_image->row_num >> 8);
	crc = cyttsp4_compute_crc(ts, ldr_erase_row_cmd,
		sizeof(ldr_erase_row_cmd) - CY_CMD_TAIL_LEN);
	ldr_erase_row_cmd[7] = (u8)crc;
	ldr_erase_row_cmd[8] = (u8)(crc >> 8);

	status_buf = cyttsp4_send_cmd(ts, ldr_erase_row_cmd,
		sizeof(ldr_erase_row_cmd),
		CY_CMD_LDR_ERASE_ROW_STAT_SIZE,
		CY_CMD_LDR_ERASE_ROW_DLY);

	if (status_buf == NULL) {
		status = ERROR_INVALID;
		retval = -EIO;
	} else {
		status = status_buf[CY_STATUS_BYTE];
		kfree(status_buf);
		if (status == ERROR_SUCCESS)
			retval = 0;
		else
			retval = -EIO;
	}

	return retval;
}

static int cyttsp4_ldr_parse_row(u8 *row_buf,
	struct cyttsp4_hex_image *row_image)
{
	u16 i, j;
	int retval = 0;

	if (!row_buf) {
		pr_err("%s parse row error - buf is null\n", __func__);
		retval = -EINVAL;
		goto cyttsp4_ldr_parse_row_exit;
	}

	row_image->array_id = row_buf[CY_ARRAY_ID_OFFSET];
	row_image->row_num = cyttsp4_get_short(&row_buf[CY_ROW_NUM_OFFSET]);
	row_image->row_size = cyttsp4_get_short(&row_buf[CY_ROW_SIZE_OFFSET]);

	if (row_image->row_size > ARRAY_SIZE(row_image->row_data)) {
		pr_err("%s: row data buffer overflow\n", __func__);
		retval = -EOVERFLOW;
		goto cyttsp4_ldr_parse_row_exit;
	}

	for (i = 0, j = CY_ROW_DATA_OFFSET;
		i < row_image->row_size; i++)
		row_image->row_data[i] = row_buf[j++];

	retval = 0;

cyttsp4_ldr_parse_row_exit:
	return retval;
}

static u16 g_row_checksum;

static int cyttsp4_ldr_prog_row(struct cyttsp4 *ts,
	struct cyttsp4_hex_image *row_image)
{
	u8 *status_buf;
	int status;
	u16 crc;
	u16 i, j, k, l, m;
	u16 row_sum = 0;
	int retval = 0;

	u8 *cmd = kzalloc(CY_MAX_PACKET_LEN, GFP_KERNEL);

	g_row_checksum = 0;

	if (cmd) {
		i = 0;
		status = 0;
		row_sum = 0;

		for (l = 0; l < (CY_DATA_ROW_SIZE/CY_PACKET_DATA_LEN)-1; l++) {
			cmd[0] = ldr_send_data_cmd[0];
			cmd[1] = ldr_send_data_cmd[1];
			cmd[2] = (u8)CY_PACKET_DATA_LEN;
			cmd[3] = (u8)(CY_PACKET_DATA_LEN >> 8);
			j = 4;
			m = 4;

			for (k = 0; k < CY_PACKET_DATA_LEN; k++) {
				cmd[j] = row_image->row_data[i];
				row_sum += cmd[j];
				i++;
				j++;
			}

			crc = cyttsp4_compute_crc(ts, cmd,
				CY_PACKET_DATA_LEN+m);
			cmd[CY_PACKET_DATA_LEN+m+0] = (u8)crc;
			cmd[CY_PACKET_DATA_LEN+m+1] = (u8)(crc >> 8);
			cmd[CY_PACKET_DATA_LEN+m+2] = CY_END_OF_PACKET;

			status_buf = cyttsp4_send_cmd(ts, cmd,
				CY_PACKET_DATA_LEN+m+3,
				CY_CMD_LDR_SEND_DATA_STAT_SIZE,
				CY_CMD_LDR_SEND_DATA_DLY);

			if (status_buf == NULL) {
				status = ERROR_INVALID;
				retval = -EIO;
			} else {
				status = status_buf[CY_STATUS_BYTE];
				kfree(status_buf);
				if (status == ERROR_SUCCESS)
					retval = 0;
				else
					retval = -EIO;
			}
			if (retval < 0) {
				pr_err("%s: send row segment %d"
					" fail status=%d\n",
					__func__, l, status);
				goto cyttsp4_ldr_prog_row_exit;
			}
		}

		cmd[0] = ldr_prog_row_cmd[0];
		cmd[1] = ldr_prog_row_cmd[1];
		/*
		 * include array id size and row id size in CY_PACKET_DATA_LEN
		 */
		cmd[2] = (u8)(CY_PACKET_DATA_LEN+3);
		cmd[3] = (u8)((CY_PACKET_DATA_LEN+3) >> 8);
		cmd[4] = row_image->array_id;
		cmd[5] = (u8)row_image->row_num;
		cmd[6] = (u8)(row_image->row_num >> 8);
		j = 7;
		m = 7;

		for (k = 0; k < CY_PACKET_DATA_LEN; k++) {
			cmd[j] = row_image->row_data[i];
			row_sum += cmd[j];
			i++;
			j++;
		}

		crc = cyttsp4_compute_crc(ts, cmd,
			CY_PACKET_DATA_LEN+m);
		cmd[CY_PACKET_DATA_LEN+m+0] = (u8)crc;
		cmd[CY_PACKET_DATA_LEN+m+1] = (u8)(crc >> 8);
		cmd[CY_PACKET_DATA_LEN+m+2] = CY_END_OF_PACKET;

		status_buf = cyttsp4_send_cmd(ts, cmd,
			CY_PACKET_DATA_LEN+m+3,
			CY_CMD_LDR_PROG_ROW_STAT_SIZE,
			CY_CMD_LDR_PROG_ROW_DLY);

		g_row_checksum = 1 + ~row_sum;

		if (status_buf == NULL) {
			status = ERROR_INVALID;
			retval = -EIO;
		} else {
			status = status_buf[CY_STATUS_BYTE];
			kfree(status_buf);
			if (status == ERROR_SUCCESS)
				retval = 0;
			else
				retval = -EIO;
		}
		if (retval < 0) {
			pr_err("%s: prog row fail status=%d\n",
				__func__, status);
			goto cyttsp4_ldr_prog_row_exit;
		}

	} else {
		pr_err("%s prog row error - cmd buf is NULL\n", __func__);
		status = -EIO;
	}

cyttsp4_ldr_prog_row_exit:
	if (cmd != NULL)
		kfree(cmd);
	return retval;
}

static u8 g_verify_checksum;

static int cyttsp4_ldr_verify_row(struct cyttsp4 *ts,
	struct cyttsp4_hex_image *row_image)
{
	u16 crc;
	u8 *status_buf;
	u8 status;
	int retval = 0;

	g_verify_checksum = 0;

	ldr_verify_row_cmd[4] = row_image->array_id;
	ldr_verify_row_cmd[5] = (u8)row_image->row_num;
	ldr_verify_row_cmd[6] = (u8)(row_image->row_num >> 8);
	crc = cyttsp4_compute_crc(ts, ldr_verify_row_cmd,
		sizeof(ldr_verify_row_cmd) - CY_CMD_TAIL_LEN);
	ldr_verify_row_cmd[7] = (u8)crc;
	ldr_verify_row_cmd[8] = (u8)(crc >> 8);

	status_buf = cyttsp4_send_cmd(ts, ldr_verify_row_cmd,
		sizeof(ldr_verify_row_cmd),
		CY_CMD_LDR_VERIFY_ROW_STAT_SIZE,
		CY_CMD_LDR_VERIFY_ROW_DLY);

	if (status_buf == NULL) {
		status = ERROR_INVALID;
		retval = -EIO;
	} else {
		status = status_buf[CY_STATUS_BYTE];
		g_verify_checksum = status_buf[4];
		kfree(status_buf);
		if (status == ERROR_SUCCESS)
			retval = 0;
		else
			retval = -EIO;
	}
	if (retval < 0) {
		pr_err("%s: verify row fail status=%d\n",
			__func__, status);
	}

	return retval;
}

static int cyttsp4_ldr_verify_chksum(struct cyttsp4 *ts, u8 *app_chksum)
{
	u16 crc;
	u8 *status_buf;
	u8 status;
	int retval = 0;

	*app_chksum = 0;

	crc = cyttsp4_compute_crc(ts, ldr_verify_chksum_cmd,
		sizeof(ldr_verify_chksum_cmd) - CY_CMD_TAIL_LEN);
	ldr_verify_chksum_cmd[4] = (u8)crc;
	ldr_verify_chksum_cmd[5] = (u8)(crc >> 8);
	status_buf = cyttsp4_send_cmd(ts, ldr_verify_chksum_cmd,
		sizeof(ldr_verify_chksum_cmd),
		CY_CMD_LDR_VERIFY_CHKSUM_STAT_SIZE,
		CY_CMD_LDR_VERIFY_CHKSUM_DLY);

	if (status_buf == NULL) {
		status = ERROR_INVALID;
		retval = -EIO;
	} else {
		status = status_buf[CY_STATUS_BYTE];
		*app_chksum = status_buf[4];
		kfree(status_buf);
		if (status == ERROR_SUCCESS)
			retval = 0;
		else
			retval = -EIO;
	}
	if (retval < 0) {
		pr_err("%s: verify checksum fail status=%d\n",
			__func__, status);
	}

	return retval;
}

static int _cyttsp4_ldr_exit(struct cyttsp4 *ts)
{
	u16 crc;
	u8 *status_buf;
	u8 status;
	int retval = 0;

	crc = cyttsp4_compute_crc(ts, ldr_exit_cmd,
		sizeof(ldr_exit_cmd) - CY_CMD_TAIL_LEN);
	ldr_exit_cmd[4] = (u8)crc;
	ldr_exit_cmd[5] = (u8)(crc >> 8);
	status_buf = cyttsp4_send_cmd(ts, ldr_exit_cmd,
		sizeof(ldr_exit_cmd),
		CY_CMD_LDR_EXIT_STAT_SIZE,
		CY_CMD_LDR_EXIT_DLY);

	if (status_buf == NULL) {
		status = ERROR_INVALID;
		retval = -EIO;
	} else {
		status = status_buf[CY_STATUS_BYTE];
		kfree(status_buf);
		if (status == ERROR_SUCCESS)
			retval = 0;
		else
			retval = -EIO;
	}
	if (retval < 0) {
		pr_err("%s: loader exit fail status=%d\n",
			__func__, status);
	}

	cyttsp4_dbg(ts, CY_DBG_LVL_3,
		"%s: Exit Bootloader status=%d\n",
		__func__, status);

	return retval;
}


static int _cyttsp4_load_app(struct cyttsp4 *ts, const u8 *fw, int fw_size)
{
	u8 *p;
	u8 tries;
	int ret;
	int retval;	/* need separate return value at exit stage */
	struct cyttsp4_dev_id *file_id = NULL;
	struct cyttsp4_dev_id *dev_id = NULL;
	struct cyttsp4_hex_image *row_image = NULL;
	u8 app_chksum;
	u8 *row_buf = NULL;
	size_t row_buf_size = 1024 > CY_MAX_PRBUF_SIZE ?
		1024 : CY_MAX_PRBUF_SIZE;
#ifdef CONFIG_TOUCHSCREEN_DEBUG
	int row_count = 0;
#endif

	if (!fw_size || (fw_size % sizeof(struct cyttsp4_hex_image) != 0)) {
		pr_err("%s: Firmware image is misaligned\n", __func__);
		retval = -EINVAL;
		goto _cyttsp4_load_app_exit;
	}

	pr_info("%s: start load app\n", __func__);
	ts->power_state = CY_BL_STATE;
	cyttsp4_pr_state(ts);

	row_buf = kzalloc(row_buf_size, GFP_KERNEL);
	row_image = kzalloc(sizeof(struct cyttsp4_hex_image), GFP_KERNEL);
	file_id = kzalloc(sizeof(struct cyttsp4_dev_id), GFP_KERNEL);
	dev_id = kzalloc(sizeof(struct cyttsp4_dev_id), GFP_KERNEL);
	if ((row_buf == NULL) || (row_image == NULL) ||
		(file_id == NULL) || (dev_id == NULL)) {
		pr_err("%s: Unable to alloc row buffers(%p %p %p %p)\n",
			__func__, row_buf, row_image, file_id, dev_id);
		retval = -ENOMEM;
		goto _cyttsp4_load_app_error_exit;
	}

	p = (u8 *)fw;
	/* Enter Loader and return Silicon ID and Rev */

	INIT_COMPLETION(ts->bl_int_running);
	_cyttsp4_reset(ts);


	if (mutex_is_locked(&ts->data_lock)) {
		mutex_unlock(&ts->data_lock);
		retval = wait_for_completion_interruptible_timeout(
			&ts->bl_int_running,
			msecs_to_jiffies(CY_DELAY_DFLT * CY_DELAY_MAX * 10));
		mutex_lock(&ts->data_lock);
	} else {
		retval = wait_for_completion_interruptible_timeout(&ts->
			bl_int_running,
			msecs_to_jiffies(CY_DELAY_DFLT * CY_DELAY_MAX * 10));
	}
	if (retval == 0) {
		pr_err("%s: time out waiting for bootloader interrupt\n",
			__func__);
		retval = -ETIMEDOUT;
		goto _cyttsp4_load_app_exit;
	}

	ts->power_state = CY_LDR_STATE;
	retval = cyttsp4_ldr_enter(ts, dev_id);
	if (retval) {
		pr_err("%s: Error cannot start PSOC3 Loader (ret=%d)\n",
			__func__, retval);

		goto _cyttsp4_load_app_error_exit;
	} else {
		cyttsp4_dbg(ts, CY_DBG_LVL_3,
			"%s: dev: silicon id=%08X rev=%02X bl=%08X\n",
			__func__,
			dev_id->silicon_id, dev_id->rev_id, dev_id->bl_ver);
	}

	while (p < (fw + fw_size)) {
		/* Get row */
		cyttsp4_dbg(ts, CY_DBG_LVL_1,
			"%s: read row=%d\n", __func__, ++row_count);
		memset(row_buf, 0, row_buf_size);
		p = cyttsp4_get_row(ts, row_buf, p,
			sizeof(struct cyttsp4_hex_image));

		/* Parse row */
		cyttsp4_dbg(ts, CY_DBG_LVL_3,
			"%s: p=%p buf=%p buf[0]=%02X\n", __func__,
			p, row_buf, row_buf[0]);
		retval = cyttsp4_ldr_parse_row(row_buf, row_image);
		cyttsp4_dbg(ts, CY_DBG_LVL_2,
			"%s: array_id=%02X row_num=%04X(%d)"
				" row_size=%04X(%d)\n", __func__,
			row_image->array_id,
			row_image->row_num, row_image->row_num,
			row_image->row_size, row_image->row_size);
		if (retval) {
			pr_err("%s: Parse Row Error "
				"(a=%d r=%d ret=%d\n",
				__func__, row_image->array_id,
				row_image->row_num,
				retval);
			goto bl_exit;
		} else {
			cyttsp4_dbg(ts, CY_DBG_LVL_3,
				"%s: Parse Row "
				"(a=%d r=%d ret=%d\n",
				__func__, row_image->array_id,
				row_image->row_num, retval);
		}

		/* erase row */
		tries = 0;
		do {
			retval = cyttsp4_ldr_erase_row(ts, row_image);
			if (retval) {
				pr_err("%s: Erase Row Error "
					"(array=%d row=%d ret=%d try=%d)\n",
					__func__, row_image->array_id,
					row_image->row_num, retval, tries);
			}
		} while (retval && tries++ < 5);
		if (retval)
			goto _cyttsp4_load_app_error_exit;

		/* program row */
		retval = cyttsp4_ldr_prog_row(ts, row_image);
		if (retval) {
			pr_err("%s: Program Row Error "
				"(array=%d row=%d ret=%d)\n",
				__func__, row_image->array_id,
				row_image->row_num, retval);
		}
		if (retval)
			goto _cyttsp4_load_app_error_exit;

		/* verify row */
		retval = cyttsp4_ldr_verify_row(ts, row_image);
		if (retval) {
			pr_err("%s: Verify Row Error "
				"(array=%d row=%d ret=%d)\n",
				__func__, row_image->array_id,
				row_image->row_num, retval);
			goto _cyttsp4_load_app_error_exit;
		}

		/* verify flash checksum */
		cyttsp4_dbg(ts, CY_DBG_LVL_3,
			"%s: row=%d flashchk=%02X\n",
			__func__, row_count, g_verify_checksum);
		cyttsp4_dbg(ts, CY_DBG_LVL_3,
			"%s: array=%d row_cnt=%d row_num=%04X "
			"row_chksum=%04X ver_chksum=%02X\n",
			__func__, row_image->array_id, row_count,
			row_image->row_num,
			g_row_checksum, g_verify_checksum);
		if ((u8)g_row_checksum !=
			g_verify_checksum) {
			pr_err("%s: Verify Checksum Error "
				"(array=%d row=%d "
				"rchk=%04X vchk=%02X)\n",
				__func__,
				row_image->array_id, row_image->row_num,
				g_row_checksum, g_verify_checksum);
			retval = -EIO;
			goto _cyttsp4_load_app_error_exit;
		}
	}

	/* verify app checksum */
	retval = cyttsp4_ldr_verify_chksum(ts, &app_chksum);
	cyttsp4_dbg(ts, CY_DBG_LVL_1,
		"%s: Application Checksum = %02X r=%d\n",
		__func__, app_chksum, retval);
	if (retval) {
		pr_err("%s: ldr_verify_chksum fail r=%d\n", __func__, retval);
		retval = 0;
	}

	/* exit loader */
bl_exit:
	ret = _cyttsp4_ldr_exit(ts);
	if (ret) {
		pr_err("%s: Error on exit Loader (ret=%d)\n",
			__func__, ret);
		retval = ret;
		goto _cyttsp4_load_app_error_exit;
	}

	/*
	 * this is a temporary parking state;
	 * the driver will always run startup
	 * after the loader has completed
	 */
	ts->power_state = CY_READY_STATE;
	goto _cyttsp4_load_app_exit;

_cyttsp4_load_app_error_exit:
	ts->power_state = CY_BL_STATE;
_cyttsp4_load_app_exit:
	kfree(row_buf);
	kfree(row_image);
	kfree(file_id);
	kfree(dev_id);
	return retval;
}

#ifdef CONFIG_TOUCHSCREEN_DEBUG
/* Force firmware upgrade */
static void cyttsp4_firmware_cont(const struct firmware *fw, void *context)
{
	int retval = 0;
	struct device *dev = context;
	struct cyttsp4 *ts = dev_get_drvdata(dev);
	u8 header_size = 0;

	mutex_lock(&ts->data_lock);

	if (fw == NULL) {
		pr_err("%s: Firmware not found\n", __func__);
		goto cyttsp4_firmware_cont_exit;
	}

	if ((fw->data == NULL) || (fw->size == 0)) {
		pr_err("%s: No firmware received\n", __func__);
		goto cyttsp4_firmware_cont_release_exit;
	}

	header_size = fw->data[0];
	if (header_size >= (fw->size + 1)) {
		pr_err("%s: Firmware format is invalid\n", __func__);
		goto cyttsp4_firmware_cont_release_exit;
	}

	retval = _cyttsp4_load_app(ts, &(fw->data[header_size + 1]),
		fw->size - (header_size + 1));
	if (retval) {
		pr_err("%s: Firmware update failed with error code %d\n",
			__func__, retval);
		ts->power_state = CY_IDLE_STATE;
		cyttsp4_pr_state(ts);
		retval = -EIO;
		goto cyttsp4_firmware_cont_release_exit;
	}

	ts->debug_upgrade = true;

	retval = _cyttsp4_startup(ts);
	if (retval < 0) {
		pr_err("%s: Failed to restart IC with error code %d\n",
			__func__, retval);
		ts->power_state = CY_IDLE_STATE;
		cyttsp4_pr_state(ts);
	}

cyttsp4_firmware_cont_release_exit:
	release_firmware(fw);

cyttsp4_firmware_cont_exit:
	ts->waiting_for_fw = false;
	mutex_unlock(&ts->data_lock);
	return;
}
static ssize_t cyttsp4_ic_reflash_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	static const char *wait_fw_ld = "Driver is waiting for firmware load\n";
	static const char *no_fw_ld = "No firmware loading in progress\n";
	struct cyttsp4 *ts = dev_get_drvdata(dev);

	if (ts->waiting_for_fw)
		return snprintf(buf, strlen(wait_fw_ld)+1, wait_fw_ld);
	else
		return snprintf(buf, strlen(no_fw_ld)+1, no_fw_ld);
}
static ssize_t cyttsp4_ic_reflash_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int i;
	int retval = 0;
	struct cyttsp4 *ts = dev_get_drvdata(dev);

	if (ts->waiting_for_fw) {
		pr_err("%s: Driver is already waiting for firmware\n",
			__func__);
		retval = -EALREADY;
		goto cyttsp4_ic_reflash_store_exit;
	}

	/*
	 * must configure FW_LOADER in .config file
	 * CONFIG_HOTPLUG=y
	 * CONFIG_FW_LOADER=y
	 * CONFIG_FIRMWARE_IN_KERNEL=y
	 * CONFIG_EXTRA_FIRMWARE=""
	 * CONFIG_EXTRA_FIRMWARE_DIR=""
	 */

	if (size > CY_BL_FW_NAME_SIZE) {
		pr_err("%s: Filename too long\n", __func__);
		retval = -ENAMETOOLONG;
		goto cyttsp4_ic_reflash_store_exit;
	} else {
		/*
		 * name string must be in alloc() memory
		 * or is lost on context switch
		 * strip off any line feed character(s)
		 * at the end of the buf string
		 */
		for (i = 0; buf[i]; i++) {
			if (buf[i] < ' ')
				ts->fwname[i] = 0;
			else
				ts->fwname[i] = buf[i];
		}
	}

	cyttsp4_dbg(ts, CY_DBG_LVL_3,
		"%s: Enabling firmware class loader\n", __func__);

	retval = request_firmware_nowait(THIS_MODULE,
		FW_ACTION_NOHOTPLUG, (const char *)ts->fwname, ts->dev,
		GFP_KERNEL, ts->dev, cyttsp4_firmware_cont);
	if (retval) {
		pr_err("%s: Fail request firmware class file load\n",
			__func__);
		ts->waiting_for_fw = false;
		goto cyttsp4_ic_reflash_store_exit;
	} else {
		ts->waiting_for_fw = true;
		retval = size;
	}

cyttsp4_ic_reflash_store_exit:
	return retval;
}
static DEVICE_ATTR(ic_reflash, S_IRUSR | S_IWUSR,
	cyttsp4_ic_reflash_show, cyttsp4_ic_reflash_store);
#endif

static int _cyttsp4_calc_data_crc(struct cyttsp4 *ts, size_t ndata, u8 *pdata,
	u8 *crc_h, u8 *crc_l, const char *name)
{
	int retval = 0;
	u16 crc = 0x0000;
	u8 *buf = NULL;
	int i = 0;
	int j = 0;

	*crc_h = 0;
	*crc_l = 0;

	buf = kzalloc(sizeof(uint8_t) * 126, GFP_KERNEL);
	if (buf == NULL) {
		pr_err("%s: Failed to allocate buf\n", __func__);
		retval = -ENOMEM;
		goto _cyttsp4_calc_data_crc_exit;
	}

	if (pdata == NULL) {
		pr_err("%s: bad data pointer\n", __func__);
		retval = -ENXIO;
		goto _cyttsp4_calc_data_crc_exit;
	}

	if (ndata > 122) {
		pr_err("%s: %s is too large n=%d size=%d\n",
			__func__, name, ndata, 126);
		retval = -EOVERFLOW;
		goto _cyttsp4_calc_data_crc_exit;
	}

	buf[0] = 0x00; /* num of config bytes + 4 high */
	buf[1] = 0x7E; /* num of config bytes + 4 low */
	buf[2] = 0x00; /* max block size w/o crc high */
	buf[3] = 0x7E; /* max block size w/o crc low */

	/* Copy platform data */
	memcpy(&(buf[4]), pdata, ndata);

	/* Calculate CRC */
	crc = 0xFFFF;
	for (i = 0; i < 126; i++) {
		crc ^= (buf[i] << 8);

		for (j = 8; j > 0; --j) {
			if (crc & 0x8000)
				crc = (crc << 1) ^ 0x1021;
			else
				crc = crc << 1;
		}
	}

	*crc_h = crc / 256;
	*crc_l = crc % 256;

	cyttsp4_dbg(ts, CY_DBG_LVL_3,
		"%s: crc=%02X%02X\n", __func__, *crc_h, *crc_l);

_cyttsp4_calc_data_crc_exit:
	kfree(buf);
	return retval;
}

static int _cyttsp4_calc_settings_crc(struct cyttsp4 *ts, u8 *crc_h, u8 *crc_l)
{
	int retval = 0;
	u16 crc = 0x0000;
	u8 *buf = NULL;
	int i = 0;
	int j = 0;
	u8 size = 0;

	buf = kzalloc(sizeof(uint8_t) * 126, GFP_KERNEL);
	if (buf == NULL) {
		pr_err("%s: Failed to allocate buf\n", __func__);
		retval = -ENOMEM;
		goto _cyttsp4_calc_settings_crc_exit;
	}

	if (ts->platform_data->sett[CY_IC_GRPNUM_TCH_PARM_VAL] == NULL) {
		pr_err("%s: Missing Platform Touch Parameter"
			" values table\n",  __func__);
		retval = -ENXIO;
		goto _cyttsp4_calc_settings_crc_exit;
	}
	if ((ts->platform_data->sett
		[CY_IC_GRPNUM_TCH_PARM_VAL]->data == NULL) ||
		(ts->platform_data->sett
		[CY_IC_GRPNUM_TCH_PARM_VAL]->size == 0)) {
		pr_err("%s: Missing Platform Touch Parameter"
			" values table data\n", __func__);
		retval = -ENXIO;
		goto _cyttsp4_calc_settings_crc_exit;
	}

	size = ts->platform_data->sett[CY_IC_GRPNUM_TCH_PARM_VAL]->size;

	if (size > 122) {
		pr_err("%s: Platform data is too large\n", __func__);
		retval = -EOVERFLOW;
		goto _cyttsp4_calc_settings_crc_exit;
	}

	buf[0] = 0x00; /* num of config bytes + 4 high */
	buf[1] = 0x7E; /* num of config bytes + 4 low */
	buf[2] = 0x00; /* max block size w/o crc high */
	buf[3] = 0x7E; /* max block size w/o crc low */

	/* Copy platform data */
	memcpy(&(buf[4]),
		ts->platform_data->sett[CY_IC_GRPNUM_TCH_PARM_VAL]->data,
		size);

	/* Calculate CRC */
	crc = 0xFFFF;
	for (i = 0; i < 126; i++) {
		crc ^= (buf[i] << 8);

		for (j = 8; j > 0; --j) {
			if (crc & 0x8000)
				crc = (crc << 1) ^ 0x1021;
			else
				crc = crc << 1;
		}
	}

	*crc_h = crc / 256;
	*crc_l = crc % 256;

_cyttsp4_calc_settings_crc_exit:
	kfree(buf);
	return retval;
}

static int _cyttsp4_get_ic_crc(struct cyttsp4 *ts,
	u8 blkid, u8 *crc_h, u8 *crc_l)
{
	int retval = 0;
	u8 cmd_dat[CY_NUM_DAT + 1];	/* +1 for cmd byte */
	int tries;
	int tmp_state;

	tmp_state = ts->power_state;
	ts->power_state = CY_CMD_STATE;
	cyttsp4_pr_state(ts);

	memset(cmd_dat, 0, sizeof(cmd_dat));
	/* pack cmd */
	cmd_dat[0] = CY_GET_CFG_BLK_CRC;
	/* pack blockid */
	cmd_dat[1] = blkid;

	/* send cmd */
	cyttsp4_dbg(ts, CY_DBG_LVL_2, "%s: Get CRC cmd=%02X\n",
		__func__, cmd_dat[0]);
	/* wait rdy */
	ts->cmd_rdy = false;
	tries = 0;

	retval = cyttsp4_write_block_data(ts, CY_REG_BASE +
		offsetof(struct cyttsp4_xydata, cmd),
		sizeof(cmd_dat), cmd_dat,
		ts->platform_data->addr[CY_TCH_ADDR_OFS], true);
	if (retval < 0) {
		pr_err("%s: write get crc err r=%d\n",
			__func__, retval);
		goto _cyttsp4_get_ic_crc_exit;
	}

	/* wait for cmd rdy interrupt */
	memset(cmd_dat, 0, sizeof(cmd_dat));
	while (tries++ < 10000) {
		if (mutex_is_locked(&ts->data_lock)) {
			mutex_unlock(&ts->data_lock);
			udelay(1000);
			mutex_lock(&ts->data_lock);
		} else {
			udelay(1000);
		}

		if (ts->cmd_rdy) {
			retval = cyttsp4_read_block_data(ts, CY_REG_BASE +
				offsetof(struct cyttsp4_xydata, cmd),
				sizeof(cmd_dat), cmd_dat,
				ts->platform_data->addr
					[CY_TCH_ADDR_OFS], true);
			if (retval < 0) {
				pr_err("%s: fail read cmd status"
					"r=%d\n", __func__, retval);
				goto _cyttsp4_get_ic_crc_exit;
			} else if (cmd_dat[0] & CY_CMD_RDY_BIT) {
				break;
			} else {
				/* not our interrupt */
				ts->cmd_rdy = false;
			}
		}
	}

	if (tries >= 10000) {
		pr_err("%s: command write timeout for crc request\n",
			__func__);
		retval = -ETIMEDOUT;
		goto _cyttsp4_get_ic_crc_exit;
	}

	cyttsp4_dbg(ts, CY_DBG_LVL_3,
		"%s: get ic crc cmd[0..%d]="
		"%02X %02X %02X %02X %02X %02X %02X\n",
		__func__, sizeof(cmd_dat), cmd_dat[0],
		cmd_dat[1], cmd_dat[2], cmd_dat[3],
		cmd_dat[4], cmd_dat[5], cmd_dat[6]);

	/* Check CRC status and assign values */
	if (cmd_dat[1] != 0) {
		pr_err("%s: Get CRC command failed with code 0x%02X\n",
			__func__, cmd_dat[1]);
		retval = -EIO;
		goto _cyttsp4_get_ic_crc_exit;
	}

	*crc_h = cmd_dat[2];
	*crc_l = cmd_dat[3];

_cyttsp4_get_ic_crc_exit:
	ts->power_state = tmp_state;
	cyttsp4_pr_state(ts);
	return retval;
}

static int _cyttsp4_resume_sleep(struct cyttsp4 *ts)
{
	int retval = 0;
	uint8_t sleep = CY_DEEP_SLEEP_MODE;

	cyttsp4_dbg(ts, CY_DBG_LVL_3,
		"%s: Put the part back to sleep\n", __func__);

	retval = cyttsp4_write_block_data(ts, CY_REG_BASE +
		offsetof(struct cyttsp4_xydata, hst_mode),
		sizeof(sleep), &sleep,
		ts->platform_data->addr[CY_TCH_ADDR_OFS], true);
	if (retval < 0) {
		pr_err("%s: Failed to write sleep bit r=%d\n",
			__func__, retval);
	}

	ts->power_state = CY_SLEEP_STATE;
	cyttsp4_pr_state(ts);

	return retval;
}

static int _cyttsp4_startup(struct cyttsp4 *ts)
{
	int retval = 0;
	int i = 0;
	u8 pdata_crc[2];
	u8 ic_crc[2];
	bool upgraded = false;
	bool mddata_updated = false;
	bool wrote_sysinfo_regs = false;
	bool wrote_settings = false;
	int tries = 0;

_cyttsp4_startup_start:
	memset(pdata_crc, 0, sizeof(pdata_crc));
	memset(ic_crc, 0, sizeof(ic_crc));
	cyttsp4_dbg(ts, CY_DBG_LVL_3,
		"%s: enter power_state=%d\n", __func__, ts->power_state);
	ts->power_state = CY_BL_STATE;
	cyttsp4_pr_state(ts);

	_cyttsp4_reset(ts);

	/* wait for interrupt to set ready completion */
	tries = 0;
_cyttsp4_startup_wait_bl_irq:
	INIT_COMPLETION(ts->bl_int_running);
	if (mutex_is_locked(&ts->data_lock)) {
		mutex_unlock(&ts->data_lock);
		retval = wait_for_completion_interruptible_timeout(
			&ts->bl_int_running,
			msecs_to_jiffies(CY_DELAY_DFLT * CY_DELAY_MAX * 10));
		mutex_lock(&ts->data_lock);
	} else {
		retval = wait_for_completion_interruptible_timeout(
			&ts->bl_int_running,
			msecs_to_jiffies(CY_DELAY_DFLT * CY_DELAY_MAX * 10));
	}
	if (retval == 0) {
		pr_err("%s: timeout waiting for bootloader interrupt\n",
			__func__);
		retval = -ETIMEDOUT;
		goto _cyttsp4_startup_exit;
	}

	retval = _cyttsp4_ldr_exit(ts);
	if (retval) {
		if (tries == 0) {
			tries++;
			goto _cyttsp4_startup_wait_bl_irq;
		}
		pr_err("%s: Cannot exit bl mode r=%d\n",
			__func__, retval);
		ts->power_state = CY_BL_STATE;
		cyttsp4_pr_state(ts);
		goto _cyttsp4_startup_exit;
	}

#ifdef CONFIG_TOUCHSCREEN_DEBUG
	if (retval) {
		cyttsp4_pr_status(ts, CY_DBG_LVL_3, retval);
		retval = 0;
	}
#endif

	/* wait for first interrupt: switch to application interrupt */
	cyttsp4_dbg(ts, CY_DBG_LVL_3,
		"%s: wait for first interrupt\n", __func__);
	ts->power_state = CY_SYSINFO_STATE;
	ts->current_mode = CY_SYSINFO_MODE;
	cyttsp4_pr_state(ts);
	INIT_COMPLETION(ts->si_int_running);
	if (mutex_is_locked(&ts->data_lock)) {
		mutex_unlock(&ts->data_lock);
		retval = wait_for_completion_interruptible_timeout(
			&ts->si_int_running,
			msecs_to_jiffies(CY_DELAY_DFLT * CY_DELAY_MAX * 10));
		mutex_lock(&ts->data_lock);
	} else {
		retval = wait_for_completion_interruptible_timeout(
			&ts->si_int_running,
			msecs_to_jiffies(CY_DELAY_DFLT * CY_DELAY_MAX * 10));
	}
	if (retval == 0) {
		pr_err("%s: timeout waiting for application interrupt\n",
			__func__);
		retval = -ETIMEDOUT;
		goto _cyttsp4_startup_exit;
	}

	/*
	 * TODO: remove this wait for toggle high when
	 * startup from ES10 firmware is no longer required
	 */
	/* Wait for IRQ to toggle high */
	cyttsp4_dbg(ts, CY_DBG_LVL_3,
		"%s: wait for irq toggle high\n", __func__);
	retval = -ETIMEDOUT;
	for (i = 0; i < CY_DELAY_MAX * 10 * 5; i++) {
		if (ts->platform_data->irq_stat() == CY_IRQ_DEASSERT) {
			retval = 0;
			break;
		}
		mdelay(CY_DELAY_DFLT);
	}
	if (retval < 0) {
		pr_err("%s: timeout waiting for irq to de-assert\n",
			__func__);
		goto _cyttsp4_startup_exit;
	}

	cyttsp4_dbg(ts, CY_DBG_LVL_3,
		"%s: read sysinfo 1\n", __func__);
	memset(&ts->sysinfo_data, 0,
		sizeof(struct cyttsp4_sysinfo_data));
	retval = cyttsp4_read_block_data(ts, CY_REG_BASE,
		sizeof(struct cyttsp4_sysinfo_data),
		&ts->sysinfo_data,
		ts->platform_data->addr[CY_TCH_ADDR_OFS], true);
	if (retval < 0) {
		pr_err("%s: Fail to switch from Bootloader "
			"to Application r=%d\n",
			__func__, retval);

		ts->power_state = CY_BL_STATE;
		cyttsp4_pr_state(ts);

		if (upgraded) {
			pr_err("%s: app failed to launch after"
				" platform firmware upgrade\n", __func__);
			retval = -EIO;
			goto _cyttsp4_startup_exit;
		}

		pr_info("%s: attempting to reflash IC...\n", __func__);
		if (ts->platform_data->fw->img == NULL ||
			ts->platform_data->fw->size == 0) {
			pr_err("%s: no platform firmware available"
				" for reflashing\n", __func__);
			retval = -ENODATA;
			ts->power_state = CY_INVALID_STATE;
			cyttsp4_pr_state(ts);
			goto _cyttsp4_startup_exit;
		}

		retval = _cyttsp4_load_app(ts,
			ts->platform_data->fw->img,
			ts->platform_data->fw->size);
		if (retval) {
			pr_err("%s: failed to reflash IC (r=%d)\n",
				__func__, retval);
			ts->power_state = CY_INVALID_STATE;
			cyttsp4_pr_state(ts);
			retval = -EIO;
			goto _cyttsp4_startup_exit;
		}

		pr_info("%s: resetting IC after reflashing\n", __func__);
		goto _cyttsp4_startup_start; /* Reset the part */
	}

	/*
	 * read system information registers
	 * get version numbers and fill sysinfo regs
	 */
	cyttsp4_dbg(ts, CY_DBG_LVL_3,
		"%s: Read Sysinfo regs and get version numbers\n", __func__);
	retval = _cyttsp4_get_sysinfo_regs(ts);
	if (retval < 0) {
		pr_err("%s: Read Block fail -get sys regs (r=%d)\n",
			__func__, retval);
		ts->power_state = CY_IDLE_STATE;
		cyttsp4_pr_state(ts);
		goto _cyttsp4_startup_exit;
	}

#ifdef CONFIG_TOUCHSCREEN_DEBUG
	if (!ts->ic_grptest && !(ts->debug_upgrade)) {
		retval = _cyttsp4_boot_loader(ts, &upgraded);
		if (retval < 0) {
			pr_err("%s: fail boot loader r=%d)\n",
				__func__, retval);
			ts->power_state = CY_IDLE_STATE;
			cyttsp4_pr_state(ts);
			goto _cyttsp4_startup_exit;
		}
		if (upgraded)
			goto _cyttsp4_startup_start;
	}
#else
	retval = _cyttsp4_boot_loader(ts, &upgraded);
	if (retval < 0) {
		pr_err("%s: fail boot loader r=%d)\n",
			__func__, retval);
		ts->power_state = CY_IDLE_STATE;
		cyttsp4_pr_state(ts);
		goto _cyttsp4_startup_exit;
	}
	if (upgraded)
		goto _cyttsp4_startup_start;
#endif

	if (!wrote_sysinfo_regs) {
#ifdef CONFIG_TOUCHSCREEN_DEBUG
		if (ts->ic_grptest)
			goto _cyttsp4_startup_set_sysinfo_done;
#endif
		cyttsp4_dbg(ts, CY_DBG_LVL_3,
			"%s: Set Sysinfo regs\n", __func__);
		retval = _cyttsp4_set_sysinfo_mode(ts);
		if (retval < 0) {
			pr_err("%s: Set SysInfo Mode fail r=%d\n",
				__func__, retval);
			ts->power_state = CY_IDLE_STATE;
			cyttsp4_pr_state(ts);
			goto _cyttsp4_startup_exit;
		}
		retval = _cyttsp4_set_sysinfo_regs(ts, &mddata_updated);
		if (retval < 0) {
			pr_err("%s: Set SysInfo Regs fail r=%d\n",
				__func__, retval);
			ts->power_state = CY_IDLE_STATE;
			cyttsp4_pr_state(ts);
			goto _cyttsp4_startup_exit;
		} else
			wrote_sysinfo_regs = true;
	}

#ifdef CONFIG_TOUCHSCREEN_DEBUG
_cyttsp4_startup_set_sysinfo_done:
#endif
	cyttsp4_dbg(ts, CY_DBG_LVL_3,
		"%s: enter operational mode\n", __func__);
	retval = _cyttsp4_set_operational_mode(ts);
	if (retval < 0) {
		ts->power_state = CY_IDLE_STATE;
		cyttsp4_pr_state(ts);
		pr_err("%s: Fail set operational mode (r=%d)\n",
			__func__, retval);
		goto _cyttsp4_startup_exit;
	} else {
#ifdef CONFIG_TOUCHSCREEN_DEBUG
		if (ts->ic_grptest)
			goto _cyttsp4_startup_settings_valid;
#endif
		/* Calculate settings CRC from platform settings */
		cyttsp4_dbg(ts, CY_DBG_LVL_3,
			"%s: Calculate settings CRC and get IC CRC\n",
			__func__);
		retval = _cyttsp4_calc_settings_crc(ts,
			&pdata_crc[0], &pdata_crc[1]);
		if (retval < 0) {
			pr_err("%s: Unable to calculate settings CRC\n",
				__func__);
			goto _cyttsp4_startup_exit;
		}

		/* Get settings CRC from touch IC */
		retval = _cyttsp4_get_ic_crc(ts, CY_TCH_PARM_BLKID,
			&ic_crc[0], &ic_crc[1]);
		if (retval < 0) {
			pr_err("%s: Unable to get settings CRC\n", __func__);
			goto _cyttsp4_startup_exit;
		}

		/* Compare CRC values */
		cyttsp4_dbg(ts, CY_DBG_LVL_3,
			"%s: PDATA CRC = 0x%02X%02X, IC CRC = 0x%02X%02X\n",
			__func__, pdata_crc[0], pdata_crc[1],
			ic_crc[0], ic_crc[1]);

		if ((pdata_crc[0] == ic_crc[0]) &&
			(pdata_crc[1] == ic_crc[1]))
			goto _cyttsp4_startup_settings_valid;

		/* Update settings */
		pr_info("%s: Updating IC settings...\n", __func__);

		if (wrote_settings) {
			pr_err("%s: Already updated IC settings\n",
				__func__);
			goto _cyttsp4_startup_settings_valid;
		}

		retval = _cyttsp4_set_op_params(ts, pdata_crc[0], pdata_crc[1]);
		if (retval < 0) {
			pr_err("%s: Set Operational Params fail r=%d\n",
				__func__, retval);
			goto _cyttsp4_startup_exit;
		}

		wrote_settings = true;
	}

_cyttsp4_startup_settings_valid:
	if (mddata_updated || wrote_settings) {
		pr_info("%s: Resetting IC after writing settings\n",
			__func__);
			mddata_updated = false;
			wrote_settings = false;
		goto _cyttsp4_startup_start;
	}
	cyttsp4_dbg(ts, CY_DBG_LVL_3,
		"%s: enable handshake\n", __func__);
	retval = _cyttsp4_hndshk_enable(ts);
	if (retval < 0)
		pr_err("%s: fail enable handshake r=%d", __func__, retval);

	ts->power_state = CY_ACTIVE_STATE;
	cyttsp4_pr_state(ts);

	if (ts->was_suspended) {
		ts->was_suspended = false;
		retval = _cyttsp4_resume_sleep(ts);
		if (retval < 0) {
			pr_err("%s: fail resume sleep r=%d\n",
				__func__, retval);
		}
	}

_cyttsp4_startup_exit:
	return retval;
}

static irqreturn_t cyttsp4_irq(int irq, void *handle)
{
	struct cyttsp4 *ts = handle;
	int retval;

	cyttsp4_dbg(ts, CY_DBG_LVL_3,
		"%s: GOT IRQ ps=%d\n", __func__, ts->power_state);
	mutex_lock(&ts->data_lock);

	cyttsp4_dbg(ts, CY_DBG_LVL_3,
		"%s: DO IRQ ps=%d\n", __func__, ts->power_state);
	switch (ts->power_state) {
	case CY_BL_STATE:
		complete(&ts->bl_int_running);
		break;
	case CY_SYSINFO_STATE:
		complete(&ts->si_int_running);
		break;
	case CY_LDR_STATE:
		ts->bl_ready_flag = true;
		break;
	case CY_CMD_STATE:
		ts->cmd_rdy = true;
		break;
	case CY_SLEEP_STATE:
		pr_err("%s: IRQ in sleep state\n", __func__);
		/* Service the interrupt as if active (if possible) */
		ts->power_state = CY_ACTIVE_STATE;
		cyttsp4_pr_state(ts);
		retval = _cyttsp4_xy_worker(ts);
		if (retval == IRQ_HANDLED) {
			ts->was_suspended = true;
			break;
		} else if (retval < 0) {
			pr_err("%s: Sleep XY Worker fail r=%d\n",
				__func__, retval);
			_cyttsp4_startup(ts);
		}
		/* Put the part back to sleep */
		retval = _cyttsp4_resume_sleep(ts);
		if (retval < 0) {
			pr_err("%s: fail resume sleep r=%d\n",
				__func__, retval);
		}
		break;
	case CY_IDLE_STATE:
		break;
	case CY_ACTIVE_STATE:
		/* process the touches */
		retval = _cyttsp4_xy_worker(ts);
		if (retval < 0) {
			pr_err("%s: XY Worker fail r=%d\n",
				__func__, retval);
			_cyttsp4_startup(ts);
		}
		break;
	default:
		break;
	}

	mutex_unlock(&ts->data_lock);
	return IRQ_HANDLED;
}

static void cyttsp4_ldr_init(struct cyttsp4 *ts)
{
#ifdef CONFIG_TOUCHSCREEN_DEBUG
	if (device_create_file(ts->dev, &dev_attr_drv_debug))
		pr_err("%s: Error, could not create drv_debug\n", __func__);

	if (device_create_file(ts->dev, &dev_attr_drv_flags))
		pr_err("%s: Error, could not create drv_flags\n", __func__);

	if (device_create_file(ts->dev, &dev_attr_drv_irq))
		pr_err("%s: Error, could not create drv_irq\n", __func__);
#endif

	if (device_create_file(ts->dev, &dev_attr_drv_stat))
		pr_err("%s: Error, could not create drv_stat\n", __func__);

	if (device_create_file(ts->dev, &dev_attr_drv_ver))
		pr_err("%s: Error, could not create drv_ver\n", __func__);

#ifdef CONFIG_TOUCHSCREEN_DEBUG
	if (device_create_file(ts->dev, &dev_attr_hw_irqstat))
		pr_err("%s: Error, could not create hw_irqstat\n", __func__);

	if (device_create_file(ts->dev, &dev_attr_hw_reset))
		pr_err("%s: Error, could not create hw_reset\n", __func__);

	if (device_create_file(ts->dev, &dev_attr_hw_recov))
		pr_err("%s: Error, could not create hw_recov\n", __func__);

	if (device_create_file(ts->dev, &dev_attr_ic_grpnum))
		pr_err("%s: Error, could not create ic_grpnum\n", __func__);

	if (device_create_file(ts->dev, &dev_attr_ic_grpoffset))
		pr_err("%s: Error, could not create ic_grpoffset\n", __func__);

	if (device_create_file(ts->dev, &dev_attr_ic_grpdata))
		pr_err("%s: Error, could not create ic_grpdata\n", __func__);

	if (device_create_file(ts->dev, &dev_attr_ic_reflash))
		pr_err("%s: Error, could not create ic_reflash\n", __func__);
#endif

	if (device_create_file(ts->dev, &dev_attr_ic_ver))
		pr_err("%s: Cannot create ic_ver\n", __func__);

	return;
}

static void cyttsp4_ldr_free(struct cyttsp4 *ts)
{
	device_remove_file(ts->dev, &dev_attr_drv_ver);
	device_remove_file(ts->dev, &dev_attr_drv_stat);
	device_remove_file(ts->dev, &dev_attr_ic_ver);
#ifdef CONFIG_TOUCHSCREEN_DEBUG
	device_remove_file(ts->dev, &dev_attr_ic_grpnum);
	device_remove_file(ts->dev, &dev_attr_ic_grpoffset);
	device_remove_file(ts->dev, &dev_attr_ic_grpdata);
	device_remove_file(ts->dev, &dev_attr_hw_irqstat);
	device_remove_file(ts->dev, &dev_attr_drv_irq);
	device_remove_file(ts->dev, &dev_attr_drv_debug);
	device_remove_file(ts->dev, &dev_attr_ic_reflash);
	device_remove_file(ts->dev, &dev_attr_drv_flags);
	device_remove_file(ts->dev, &dev_attr_hw_reset);
	device_remove_file(ts->dev, &dev_attr_hw_recov);
#endif
}

static int cyttsp4_power_on(struct cyttsp4 *ts)
{
	int retval = 0;

	mutex_lock(&ts->data_lock);
	retval = _cyttsp4_startup(ts);
	mutex_unlock(&ts->data_lock);
	if (retval < 0) {
		pr_err("%s: startup fail at power on r=%d\n", __func__, retval);
		ts->power_state = CY_IDLE_STATE;
		cyttsp4_pr_state(ts);
	}

	return retval;
}

static int cyttsp4_open(struct input_dev *dev)
{
	int retval = 0;

	struct cyttsp4 *ts = input_get_drvdata(dev);
	mutex_lock(&ts->startup_mutex);
	if (!ts->powered) {
		retval = cyttsp4_power_on(ts);

		/* powered if no hard failure */
		if (retval < 0)
			ts->powered = false;
		else
			ts->powered = true;
		pr_info("%s: Powered ON(%d) r=%d\n",
			__func__, (int)ts->powered, retval);
	}
	mutex_unlock(&ts->startup_mutex);
	return 0;
}

static void cyttsp4_close(struct input_dev *dev)
{
	/*
	 * close() normally powers down the device
	 * this call simply returns unless power
	 * to the device can be controlled by the driver
	 */
	return;
}

void cyttsp4_core_release(void *handle)
{
	struct cyttsp4 *ts = handle;

	if (ts) {
#ifdef CONFIG_HAS_EARLYSUSPEND
		unregister_early_suspend(&ts->early_suspend);
#endif
		cyttsp4_ldr_free(ts);
		mutex_destroy(&ts->data_lock);
		mutex_destroy(&ts->startup_mutex);
		free_irq(ts->irq, ts);
		input_unregister_device(ts->input);
		if (ts->cyttsp4_wq) {
			destroy_workqueue(ts->cyttsp4_wq);
			ts->cyttsp4_wq = NULL;
		}

		kfree(ts);
	}
}
EXPORT_SYMBOL_GPL(cyttsp4_core_release);

void *cyttsp4_core_init(struct cyttsp4_bus_ops *bus_ops,
	struct device *dev, int irq, char *name)
{
	int i;
	u16 signal;
	int retval = 0;
	struct input_dev *input_device;
	struct cyttsp4 *ts = kzalloc(sizeof(*ts), GFP_KERNEL);

	if (!ts) {
		pr_err("%s: Error, kzalloc\n", __func__);
		goto error_alloc_data;
	}

	ts->cyttsp4_wq =
		create_singlethread_workqueue("cyttsp4_resume_startup_wq");
	if (ts->cyttsp4_wq == NULL) {
		pr_err("%s: No memory for cyttsp4_resume_startup_wq\n",
			__func__);
		retval = -ENOMEM;
		goto err_alloc_wq_failed;
	}

#ifdef CONFIG_TOUCHSCREEN_DEBUG
	ts->fwname = kzalloc(CY_BL_FW_NAME_SIZE, GFP_KERNEL);
	if ((ts->fwname == NULL) || (dev == NULL) || (bus_ops == NULL)) {
		pr_err("%s: Error, dev, bus_ops, or fwname null\n",
			__func__);
		kfree(ts);
		ts = NULL;
		goto error_alloc_data;
	}
	ts->waiting_for_fw = false;
	ts->debug_upgrade = false;

#endif

	ts->powered = false;
	ts->cmd_rdy = false;
	ts->hndshk_enabled = false;
	ts->was_suspended = false;


#ifdef CONFIG_TOUCHSCREEN_DEBUG
	ts->ic_grpnum = CY_IC_GRPNUM_RESERVED;
	ts->ic_grpoffset = 0;
	ts->ic_grptest = false;
#endif

	mutex_init(&ts->data_lock);
	mutex_init(&ts->startup_mutex);
	ts->power_state = CY_INVALID_STATE;
	ts->current_mode = CY_MODE_BOOTLOADER;
	ts->dev = dev;
	ts->platform_data = dev->platform_data;
	ts->bus_ops = bus_ops;
#ifdef CONFIG_TOUCHSCREEN_DEBUG
	ts->bus_ops->tsdebug = CY_DBG_LVL_0;
#endif

	cyttsp4_dbg(ts, CY_DBG_LVL_3,
		"%s: Initialize irq flags\n", __func__);

	init_completion(&ts->bl_int_running);
	init_completion(&ts->si_int_running);
	ts->flags = ts->platform_data->flags;

	if (ts->flags & CY_FLAG_TMA400)
		ts->max_config_bytes = CY_TMA400_MAX_BYTES;
	else
		ts->max_config_bytes = CY_TMA884_MAX_BYTES;

	ts->irq = irq;
	if (ts->irq <= 0) {
		cyttsp4_dbg(ts, CY_DBG_LVL_3,
			"%s: Error, failed to allocate irq\n", __func__);
			goto error_init;
	}

	ts->bl_ready_flag = false;

	/* Create the input device and register it. */
	cyttsp4_dbg(ts, CY_DBG_LVL_3,
		"%s: Create the input device and register it\n", __func__);
	input_device = input_allocate_device();
	if (!input_device) {
		pr_err("%s: Error, failed to allocate input device\n",
			__func__);
		goto error_input_allocate_device;
	}

	ts->input = input_device;
	input_device->name = name;
	snprintf(ts->phys, sizeof(ts->phys), "%s", dev_name(dev));
	input_device->phys = ts->phys;
	input_device->dev.parent = ts->dev;
	ts->bus_type = bus_ops->dev->bus;

	input_device->open = cyttsp4_open;
	input_device->close = cyttsp4_close;
	input_set_drvdata(input_device, ts);
	dev_set_drvdata(dev, ts);

	cyttsp4_dbg(ts, CY_DBG_LVL_3,
		"%s: Initialize event signals\n", __func__);
	__set_bit(EV_ABS, input_device->evbit);

	set_bit(BTN_TOUCH, input_device->keybit);
	input_set_abs_params(input_device, ABS_MT_POSITION_X, 0, CY_MAXX, 0, 0);
	input_set_abs_params(input_device, ABS_MT_POSITION_Y, 0, CY_MAXY, 0, 0);

	input_set_abs_params(input_device, ABS_X, 0, CY_MAXX, 0, 0);
	input_set_abs_params(input_device, ABS_Y, 0, CY_MAXY, 0, 0);

	for (i = 0; i < (ts->platform_data->frmwrk->size/CY_NUM_ABS_SET); i++) {
		signal = ts->platform_data->frmwrk->abs[
			(i*CY_NUM_ABS_SET)+CY_SIGNAL_OST];
		if (signal != CY_IGNORE_VALUE) {
			input_set_abs_params(input_device,
				signal,
				ts->platform_data->frmwrk->abs[
					(i*CY_NUM_ABS_SET)+CY_MIN_OST],
				ts->platform_data->frmwrk->abs[
					(i*CY_NUM_ABS_SET)+CY_MAX_OST],
				ts->platform_data->frmwrk->abs[
					(i*CY_NUM_ABS_SET)+CY_FUZZ_OST],
				ts->platform_data->frmwrk->abs[
					(i*CY_NUM_ABS_SET)+CY_FLAT_OST]);
		}
	}

	input_set_events_per_packet(input_device, 6 * CY_NUM_TCH_ID);

	if (ts->platform_data->frmwrk->enable_vkeys)
		input_set_capability(input_device, EV_KEY, KEY_PROG1);

	cyttsp4_dbg(ts, CY_DBG_LVL_3,
		"%s: Initialize irq\n", __func__);

	/* FIXME! ey: changed the hardcoded gpio */
	omap_writel(omap_readl(0x4a1001c8) | 0x011b, 0x4a1001c8);
	gpio_request(24, "touchscreen_cypress_ttsp");
	gpio_direction_input(24);

	retval = request_threaded_irq(ts->irq, NULL, cyttsp4_irq,
		IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
		ts->input->name, ts);
	if (retval < 0) {
#ifdef CONFIG_TOUCHSCREEN_DEBUG
		ts->irq_enabled = false;
#endif
		pr_err("%s: failed to init irq r=%d name=%s\n",
			__func__, retval, ts->input->name);
		goto error_input_register_device;
	} else {
#ifdef CONFIG_TOUCHSCREEN_DEBUG
		ts->irq_enabled = true;
#endif
	}

	if (input_register_device(input_device)) {
		pr_err("%s: Error, failed to register input device\n",
			__func__);
		goto error_input_register_device;
	}

	/* add /sys files */
	cyttsp4_ldr_init(ts);

#ifdef CONFIG_HAS_EARLYSUSPEND
	ts->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	ts->early_suspend.suspend = cyttsp4_early_suspend;
	ts->early_suspend.resume = cyttsp4_late_resume;
	register_early_suspend(&ts->early_suspend);
#endif

	INIT_WORK(&ts->cyttsp4_resume_startup_work, cyttsp4_ts_work_func);

	goto no_error;

error_input_register_device:
	input_free_device(input_device);
error_input_allocate_device:

error_init:
	mutex_destroy(&ts->data_lock);
	mutex_destroy(&ts->startup_mutex);
	if (ts->cyttsp4_wq) {
		destroy_workqueue(ts->cyttsp4_wq);
		ts->cyttsp4_wq = NULL;
	}
#ifdef CONFIG_TOUCHSCREEN_DEBUG
	kfree(ts->fwname);
#endif
err_alloc_wq_failed:
	kfree(ts);
	ts = NULL;
error_alloc_data:
	pr_err("%s: Failed Initialization\n", __func__);
no_error:
	return ts;
}
EXPORT_SYMBOL_GPL(cyttsp4_core_init);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Cypress TrueTouch(R) Standard touchscreen driver core");
MODULE_AUTHOR("Cypress");

