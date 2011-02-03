/*
 * drivers/media/video/omap/sil9022.c
 *
 * Copyright (C) 2009 Texas Instruments
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2. This program is licensed "as is" without any warranty of any
 * kind, whether express or implied.
 *
 * SIL9022
 * Owner: kiran Chitriki
 *
 */

/***********************************/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/io.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <mach/sil9022.h>

#include <plat/display.h>
#include <plat/io.h>
#include <plat/omap-pm.h>
#include <linux/slab.h>

u16 current_descriptor_addrs;

static struct i2c_client *sil9022_client;

static struct omap_video_timings omap_dss_hdmi_timings = {
	.x_res          = HDMI_XRES,
	.y_res          = HDMI_YRES,
	.pixel_clock    = HDMI_PIXCLOCK_MAX,
	.hfp            = 110,
	.hbp            = 220,
	.hsw            = 40,
	.vfp            = 5,
	.vbp            = 20,
	.vsw            = 5,
};

static struct hdmi_reg_data  hdmi_tpi_audio_config_data[] = {
	/* Transmitter is brought to Full operation when value of power
	 * state register is 0x0 */
	{ HDMI_TPI_POWER_STATE_CTRL_REG, TPI_AVI_POWER_STATE_D0		},
	/* TMDS output lines active. bit 3 1:TMDS inactive, 0: TMDS active */
	{ HDMI_SYS_CTRL_DATA_REG,  0x01					},
	/*HDCP Enable - Disable */
	{ HDMI_TPI_HDCP_CONTROLDATA_REG, 0				},
	/* I2S mode , Mute Enabled , PCM */
	{ HDMI_TPI_AUDIO_CONFIG_BYTE2_REG, TPI_AUDIO_INTERFACE_I2S |
					    TPI_AUDIO_MUTE_ENABLE |
					    TPI_AUDIO_CODING_PCM	},
	/* I2S Input configuration register */
	{ HDMI_TPI_I2S_INPUT_CONFIG_REG, TPI_I2S_SCK_EDGE_RISING |
					TPI_I2S_MCLK_MULTIPLIER_256 |
					TPI_I2S_WS_POLARITY_HIGH |
					TPI_I2S_SD_JUSTIFY_LEFT |
					TPI_I2S_SD_DIRECTION_MSB_FIRST |
					TPI_I2S_FIRST_BIT_SHIFT_YES	 },
	/* I2S Enable ad Mapping Register */
	{ HDMI_TPI_I2S_ENABLE_MAPPING_REG, TPI_I2S_SD_CHANNEL_ENABLE |
					    TPI_I2S_SD_FIFO_0 |
					    TPI_I2S_DOWNSAMPLE_DISABLE |
					    TPI_I2S_LF_RT_SWAP_NO |
					    TPI_I2S_SD_CONFIG_SELECT_SD0 },
	{ HDMI_TPI_I2S_ENABLE_MAPPING_REG, TPI_I2S_SD_CHANNEL_DISABLE |
					    TPI_I2S_SD_FIFO_1 |
					    TPI_I2S_DOWNSAMPLE_DISABLE |
					    TPI_I2S_LF_RT_SWAP_NO |
					    TPI_I2S_SD_CONFIG_SELECT_SD1 },
	{ HDMI_TPI_I2S_ENABLE_MAPPING_REG, TPI_I2S_SD_CHANNEL_DISABLE |
					    TPI_I2S_SD_FIFO_2 |
					    TPI_I2S_DOWNSAMPLE_DISABLE |
					    TPI_I2S_LF_RT_SWAP_NO |
					    TPI_I2S_SD_CONFIG_SELECT_SD2 },
	{ HDMI_TPI_I2S_ENABLE_MAPPING_REG, TPI_I2S_SD_CHANNEL_DISABLE |
					    TPI_I2S_SD_FIFO_3 |
					    TPI_I2S_DOWNSAMPLE_DISABLE |
					    TPI_I2S_LF_RT_SWAP_NO |
					    TPI_I2S_SD_CONFIG_SELECT_SD3 },
	{ HDMI_TPI_AUDIO_CONFIG_BYTE3_REG, TPI_AUDIO_SAMPLE_SIZE_16 |
					     TPI_AUDIO_FREQ_44KHZ |
					     TPI_AUDIO_2_CHANNEL	 },
	/* Speaker Configuration  refer CEA Specification*/
	{ HDMI_TPI_AUDIO_CONFIG_BYTE4_REG, (0x0 << 0)},
	/* Stream Header Settings */
	{ HDMI_TPI_I2S_STRM_HDR_0_REG, I2S_CHAN_STATUS_MODE		 },
	{ HDMI_TPI_I2S_STRM_HDR_1_REG, I2S_CHAN_STATUS_CAT_CODE	},
	{ HDMI_TPI_I2S_STRM_HDR_2_REG, I2S_CHAN_SOURCE_CHANNEL_NUM	 },
	{ HDMI_TPI_I2S_STRM_HDR_3_REG, I2S_CHAN_ACCURACY_N_44_SAMPLING_FS },
	{ HDMI_TPI_I2S_STRM_HDR_4_REG, I2S_CHAN_ORIGIN_FS_N_SAMP_LENGTH  },
	/*     Infoframe data Select  */
	{ HDMI_CPI_MISC_IF_SELECT_REG, HDMI_INFOFRAME_TX_ENABLE |
					HDMI_INFOFRAME_TX_REPEAT |
					HDMI_AUDIO_INFOFRAME		 },
};

static u8 misc_audio_info_frame_data[] = {
	MISC_INFOFRAME_TYPE | MISC_INFOFRAME_ALWAYS_SET,
	MISC_INFOFRAME_VERSION,
	MISC_INFOFRAME_LENGTH,
	0,				/* Checksum byte*/
	HDMI_SH_PCM | HDMI_SH_TWO_CHANNELS,
	HDMI_SH_44KHz | HDMI_SH_16BIT,	/* 44.1 KHz*/
	0x0,   /* Default 0*/
	HDMI_SH_SPKR_FLFR,
	HDMI_SH_0dB_ATUN | 0x1,		/* 0 dB  Attenuation*/
	0x0,
	0x0,
	0x0,
	0x0,
	0x0
};

static u8 avi_info_frame_data[] = {
	0x00,
	0x00,
	0xA8,
	0x00,
	0x04,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00
};

void get_horz_vert_timing_info(u8 *edid)
{
	/*HORIZONTAL FRONT PORCH */
	omap_dss_hdmi_timings.hfp = edid[current_descriptor_addrs + 8];
	/*HORIZONTAL SYNC WIDTH */
	omap_dss_hdmi_timings.hsw = edid[current_descriptor_addrs + 9];
	/*HORIZONTAL BACK PORCH */
	omap_dss_hdmi_timings.hbp = (((edid[current_descriptor_addrs + 4]
					  & 0x0F) << 8) |
					edid[current_descriptor_addrs + 3]) -
		(omap_dss_hdmi_timings.hfp + omap_dss_hdmi_timings.hsw);
	/*VERTICAL FRONT PORCH */
	omap_dss_hdmi_timings.vfp = ((edid[current_descriptor_addrs + 10] &
				       0xF0) >> 4);
	/*VERTICAL SYNC WIDTH */
	omap_dss_hdmi_timings.vsw = (edid[current_descriptor_addrs + 10] &
				      0x0F);
	/*VERTICAL BACK PORCH */
	omap_dss_hdmi_timings.vbp = (((edid[current_descriptor_addrs + 7] &
					0x0F) << 8) |
				      edid[current_descriptor_addrs + 6]) -
		(omap_dss_hdmi_timings.vfp + omap_dss_hdmi_timings.vsw);

	dev_dbg(&sil9022_client->dev, "<%s>\n"
				       "hfp			= %d\n"
				       "hsw			= %d\n"
				       "hbp			= %d\n"
				       "vfp			= %d\n"
				       "vsw			= %d\n"
				       "vbp			= %d\n",
		 __func__,
		 omap_dss_hdmi_timings.hfp,
		 omap_dss_hdmi_timings.hsw,
		 omap_dss_hdmi_timings.hbp,
		 omap_dss_hdmi_timings.vfp,
		 omap_dss_hdmi_timings.vsw,
		 omap_dss_hdmi_timings.vbp
		 );

}

/*------------------------------------------------------------------------------
 | Function    : get_edid_timing_data
 +------------------------------------------------------------------------------
 | Description : This function gets the resolution information from EDID
 |
 | Parameters  : void
 |
 | Returns     : void
 +----------------------------------------------------------------------------*/
void get_edid_timing_data(u8 *edid, u16 *pixel_clk, u16 *horizontal_res,
			  u16 *vertical_res)
{
	u8 offset, effective_addrs;
	u8 count;
	u8 i;
	u8 flag = false;
	/*check for 720P timing in block0 */
	for (count = 0; count < EDID_SIZE_BLOCK0_TIMING_DESCRIPTOR; count++) {
		current_descriptor_addrs =
			EDID_DESCRIPTOR_BLOCK0_ADDRESS +
			count * EDID_TIMING_DESCRIPTOR_SIZE;
		*horizontal_res =
			(((edid[EDID_DESCRIPTOR_BLOCK0_ADDRESS + 4 +
			   count * EDID_TIMING_DESCRIPTOR_SIZE] & 0xF0) << 4) |
			 edid[EDID_DESCRIPTOR_BLOCK0_ADDRESS + 2 +
			 count * EDID_TIMING_DESCRIPTOR_SIZE]);
		*vertical_res =
			(((edid[EDID_DESCRIPTOR_BLOCK0_ADDRESS + 7 +
			   count * EDID_TIMING_DESCRIPTOR_SIZE] & 0xF0) << 4) |
			 edid[EDID_DESCRIPTOR_BLOCK0_ADDRESS + 5 +
			 count * EDID_TIMING_DESCRIPTOR_SIZE]);

		dev_dbg(&sil9022_client->dev,
			"<%s> ***Block-0-Timing-descriptor[%d]***\n",
			__func__, count);
		for (i = current_descriptor_addrs;
		      i <
		      (current_descriptor_addrs+EDID_TIMING_DESCRIPTOR_SIZE);
		      i++)
			dev_dbg(&sil9022_client->dev,
				"%x ==>		%x\n", i, edid[i]);

			dev_dbg(&sil9022_client->dev,
				 "<%s>\n"
				 "E-EDID Buffer Index	= %d\n"
				 "horizontal_res	= %d\n"
				 "vertical_res		= %d\n",
				 __func__,
				 current_descriptor_addrs,
				 *horizontal_res,
				 *vertical_res
				 );

		if (*horizontal_res == HDMI_XRES &&
		    *vertical_res == HDMI_YRES) {
			dev_info(&sil9022_client->dev, "<%s>\nFound EDID Data "
						       "for %d x %dp\n",
				 __func__, *horizontal_res, *vertical_res);
			flag = true;
			break;
			}
	}

	/*check for the Timing in block1 */
	if (flag != true) {
		offset = edid[EDID_DESCRIPTOR_BLOCK1_ADDRESS + 2];
		if (offset != 0) {
			effective_addrs = EDID_DESCRIPTOR_BLOCK1_ADDRESS
				+ offset;
			/*to determine the number of descriptor blocks */
			for (count = 0;
			      count < EDID_SIZE_BLOCK1_TIMING_DESCRIPTOR;
			      count++) {
				current_descriptor_addrs = effective_addrs +
					count * EDID_TIMING_DESCRIPTOR_SIZE;
				*horizontal_res =
					(((edid[effective_addrs + 4 +
					   count*EDID_TIMING_DESCRIPTOR_SIZE] &
					   0xF0) << 4) |
					 edid[effective_addrs + 2 +
					 count * EDID_TIMING_DESCRIPTOR_SIZE]);
				*vertical_res =
					(((edid[effective_addrs + 7 +
					   count*EDID_TIMING_DESCRIPTOR_SIZE] &
					   0xF0) << 4) |
					 edid[effective_addrs + 5 +
					 count * EDID_TIMING_DESCRIPTOR_SIZE]);

				dev_dbg(&sil9022_client->dev,
					 "<%s> Block1-Timing-descriptor[%d]\n",
					 __func__, count);

				for (i = current_descriptor_addrs;
				      i < (current_descriptor_addrs+
					   EDID_TIMING_DESCRIPTOR_SIZE); i++)
					dev_dbg(&sil9022_client->dev,
						"%x ==>		%x\n",
						   i, edid[i]);

				dev_dbg(&sil9022_client->dev, "<%s>\n"
						"current_descriptor	= %d\n"
						"horizontal_res		= %d\n"
						"vertical_res		= %d\n",
					 __func__, current_descriptor_addrs,
					 *horizontal_res, *vertical_res);

				if (*horizontal_res == HDMI_XRES &&
				    *vertical_res == HDMI_YRES) {
					dev_info(&sil9022_client->dev,
						 "<%s> Found EDID Data for "
						 "%d x %dp\n",
						 __func__,
						 *horizontal_res,
						 *vertical_res
						 );
					flag = true;
					break;
					}
			}
		}
	}

	if (flag == true) {
		*pixel_clk = ((edid[current_descriptor_addrs + 1] << 8) |
			     edid[current_descriptor_addrs]);

		omap_dss_hdmi_timings.x_res = *horizontal_res;
		omap_dss_hdmi_timings.y_res = *vertical_res;
		omap_dss_hdmi_timings.pixel_clock = *pixel_clk*10;
		dev_dbg(&sil9022_client->dev,
			 "EDID TIMING DATA supported by zoom2 FOUND\n"
			 "EDID DTD block address	= %d\n"
			 "pixel_clk			= %d\n"
			 "horizontal res		= %d\n"
			 "vertical res			= %d\n",
			 current_descriptor_addrs,
			 omap_dss_hdmi_timings.pixel_clock,
			 omap_dss_hdmi_timings.x_res,
			 omap_dss_hdmi_timings.y_res
			 );

		get_horz_vert_timing_info(edid);
	} else {

		dev_info(&sil9022_client->dev,
			 "<%s>\n"
			 "EDID TIMING DATA supported by zoom2 NOT FOUND\n"
			 "setting default timing values for 720p\n"
			 "pixel_clk		= %d\n"
			 "horizontal res	= %d\n"
			 "vertical res		= %d\n",
			 __func__,
			 omap_dss_hdmi_timings.pixel_clock,
			 omap_dss_hdmi_timings.x_res,
			 omap_dss_hdmi_timings.y_res
			 );

		*pixel_clk = omap_dss_hdmi_timings.pixel_clock;
		*horizontal_res = omap_dss_hdmi_timings.x_res;
		*vertical_res = omap_dss_hdmi_timings.y_res;
	}


}


static int
sil9022_blockwrite_reg(struct i2c_client *client,
				  u8 reg, u16 alength, u8 *val, u16 *out_len)
{
	int err = 0, i;
	struct i2c_msg msg[1];
	u8 data[2];

	if (!client->adapter) {
		dev_err(&client->dev, "<%s> ERROR: No HDMI Device\n", __func__);
		return -ENODEV;
	}

	msg->addr = client->addr;
	msg->flags = I2C_M_WR;
	msg->len = 2;
	msg->buf = data;

	/* high byte goes out first */
	data[0] = reg >> 8;

	for (i = 0; i < alength - 1; i++) {
		data[1] = val[i];
		err = i2c_transfer(client->adapter, msg, 1);
		udelay(50);
		dev_err(&client->dev, "<%s> i2c Block write at 0x%x, "
				      "*val=%d flags=%d byte[%d] err=%d\n",
			__func__, data[0], data[1], msg->flags, i, err);
		if (err < 0)
			break;
	}
	/* set the number of bytes written*/
	*out_len = i;

	if (err < 0) {
		dev_err(&client->dev, "<%s> ERROR:  i2c Block Write at 0x%x, "
				      "*val=%d flags=%d bytes written=%d "
				      "err=%d\n",
			__func__, data[0], data[1], msg->flags, i, err);
		return err;
	}
	return 0;
}

static int
sil9022_blockread_reg(struct i2c_client *client,
		      u16 data_length, u16 alength,
		      u8 reg, u8 *val, u16 *out_len)
{
	int err = 0, i;
	struct i2c_msg msg[1];
	u8 data[2] = {0};

	if (!client->adapter) {
		dev_err(&client->dev, "<%s> ERROR: No HDMI Device\n", __func__);
		return -ENODEV;
	}

	msg->addr = client->addr;
	msg->flags = I2C_M_WR;
	msg->len = 1;
	msg->buf = data;

	/* High byte goes out first */
	data[0] = reg;
	err = i2c_transfer(client->adapter, msg, 1);
	dev_dbg(&client->dev, "<%s> i2c Block Read1 at 0x%x, "
			       "*val=%d flags=%d err=%d\n",
		 __func__, data[0], data[1], msg->flags, err);

	for (i = 0; i < alength; i++) {
		if (err >= 0) {
			mdelay(3);
			msg->flags = I2C_M_RD;
			msg->len = data_length;
			err = i2c_transfer(client->adapter, msg, 1);
		} else
			break;
		if (err >= 0) {
			val[i] = 0;
			/* High byte comes first */
			if (data_length == 1)
				val[i] = data[0];
			else if (data_length == 2)
				val[i] = data[1] + (data[0] << 8);
			dev_dbg(&client->dev, "<%s> i2c Block Read2 at 0x%x, "
					       "*val=%d flags=%d byte=%d "
					       "err=%d\n",
				 __func__, reg, val[i], msg->flags, i, err);
		} else
			break;
	}
	*out_len = i;
	dev_dbg(&client->dev, "<%s> i2c Block Read at 0x%x, bytes read = %d\n",
		__func__, client->addr, *out_len);

	if (err < 0) {
		dev_err(&client->dev, "<%s> ERROR:  i2c Read at 0x%x, "
				      "*val=%d flags=%d bytes read=%d err=%d\n",
			__func__, reg, *val, msg->flags, i, err);
		return err;
	}
	return 0;
}


/* Write a value to a register in sil9022 device.
 * @client: i2c driver client structure.
 * @reg: Address of the register to read value from.
 * @val: Value to be written to a specific register.
 * Returns zero if successful, or non-zero otherwise.
 */
static int
sil9022_write_reg(struct i2c_client *client, u8 reg, u8 val)
{
	int err = 0;
	struct i2c_msg msg[1];
	u8 data[2];
	int retries = 0;

	if (!client->adapter) {
		dev_err(&client->dev, "<%s> ERROR: No HDMI Device\n", __func__);
		return -ENODEV;
	}

retry:
	msg->addr = client->addr;
	msg->flags = I2C_M_WR;
	msg->len = 2;
	msg->buf = data;

	data[0] = reg;
	data[1] = val;

	err = i2c_transfer(client->adapter, msg, 1);

	dev_dbg(&client->dev, "<%s> i2c write at=%x "
			       "val=%x flags=%d err=%d\n",
		__func__, data[0], data[1], msg->flags, err);
	udelay(50);

	if (err >= 0)
		return 0;

	dev_err(&client->dev, "<%s> ERROR: i2c write at=%x "
			       "val=%x flags=%d err=%d\n",
		__func__, data[0], data[1], msg->flags, err);
	if (retries <= 5) {
		dev_info(&client->dev, "Retrying I2C... %d\n", retries);
		retries++;
		set_current_state(TASK_UNINTERRUPTIBLE);
		schedule_timeout(msecs_to_jiffies(20));
		goto retry;
	}
	return err;
}

/*
 * Read a value from a register in sil9022 device.
 * The value is returned in 'val'.
 * Returns zero if successful, or non-zero otherwise.
 */
static int
sil9022_read_reg(struct i2c_client *client, u16 data_length, u8 reg, u8 *val)
{
	int err = 0;
	struct i2c_msg msg[1];
	u8 data[2] = {0};

	if (!client->adapter) {
		dev_err(&client->dev, "<%s> ERROR: No HDMI Device\n", __func__);
		return -ENODEV;
	}

	msg->addr = client->addr;
	msg->flags = I2C_M_WR;
	msg->len = 1;
	msg->buf = data;

	data[0] = reg;
	err = i2c_transfer(client->adapter, msg, 1);
	dev_dbg(&client->dev, "<%s> i2c Read1 reg=%x val=%d "
			       "flags=%d err=%d\n",
		__func__, reg, data[1], msg->flags, err);

	if (err >= 0) {
		mdelay(3);
		msg->flags = I2C_M_RD;
		msg->len = data_length;
		err = i2c_transfer(client->adapter, msg, 1);
	}

	if (err >= 0) {
		*val = 0;
		if (data_length == 1)
			*val = data[0];
		else if (data_length == 2)
			*val = data[1] + (data[0] << 8);
		dev_dbg(&client->dev, "<%s> i2c Read2 at 0x%x, *val=%d "
				       "flags=%d err=%d\n",
			 __func__, reg, *val, msg->flags, err);
		return 0;
	}

	dev_err(&client->dev, "<%s> ERROR: i2c Read at 0x%x, "
			      "*val=%d flags=%d err=%d\n",
		__func__, reg, *val, msg->flags, err);
	return err;
 }

static int
hdmi_read_edid(struct i2c_client *client, u16 len,
	       char *p_buffer, u16 *out_len)
{
	int err =  0;
	u8 val = 0;
	int retries = 0;
	int i = 0;
	int k = 0;

	len = (len < HDMI_EDID_MAX_LENGTH) ? len : HDMI_EDID_MAX_LENGTH;

	/* Request DDC bus access to read EDID info from HDTV */
	dev_info(&client->dev, "<%s> Reading HDMI EDID\n", __func__);

	/* Bring transmitter to low-Power state */
	val = TPI_AVI_POWER_STATE_D2;
	err = sil9022_write_reg(client, HDMI_TPI_DEVICE_POWER_STATE_DATA, val);
	if (err < 0) {
		dev_err(&client->dev,
			"<%s> ERROR: Failed during bring power state - low.\n",
			 __func__);
		return err;
	}

	/* Disable TMDS clock */
	val = 0x11;
	err = sil9022_write_reg(client, HDMI_SYS_CTRL_DATA_REG, val);
	if (err < 0) {
		dev_err(&client->dev,
			"<%s> ERROR: Failed during bring power state - low.\n",
			 __func__);
		return err;
	}

	val = 0;
	/* Read TPI system control register*/
	err = sil9022_read_reg(client, 1, HDMI_SYS_CTRL_DATA_REG, &val);
	if (err < 0) {
		dev_err(&client->dev,
			"<%s> ERROR: Reading DDC BUS REQUEST\n", __func__);
		return err;
	}

	/* The host writes 0x1A[2]=1 to request the
	 * DDC(Display Data Channel) bus
	 */
	val |= TPI_SYS_CTRL_DDC_BUS_REQUEST;
	err = sil9022_write_reg(client, HDMI_SYS_CTRL_DATA_REG, val);
	if (err < 0) {
		dev_err(&client->dev,
			"<%s> ERROR: Writing DDC BUS REQUEST\n", __func__);
		return err;
	}

	 /*  Poll for bus DDC Bus control to be granted */
	dev_info(&client->dev, "<%s> Poll for DDC bus access\n", __func__);
	val = 0;
	do {
		err = sil9022_read_reg(client, 1, HDMI_SYS_CTRL_DATA_REG, &val);
		if (retries++ > 100)
			return err;

	} while ((val & TPI_SYS_CTRL_DDC_BUS_GRANTED) == 0);

	/*  Close the switch to the DDC */
	val |= TPI_SYS_CTRL_DDC_BUS_REQUEST | TPI_SYS_CTRL_DDC_BUS_GRANTED;
	err = sil9022_write_reg(client, HDMI_SYS_CTRL_DATA_REG, val);
	if (err < 0) {
		dev_err(&client->dev,
			"<%s> ERROR: Close switch to DDC BUS REQUEST\n",
			__func__);
		return err;
	}

	memset(p_buffer, 0, len);
	/* change I2C SetSlaveAddress to HDMI_I2C_MONITOR_ADDRESS */
	/*  Read the EDID structure from the monitor I2C address  */
	client->addr = HDMI_I2C_MONITOR_ADDRESS;
	err = sil9022_blockread_reg(client, 1, len,
				    0x00, p_buffer, out_len);
	if (err < 0 || *out_len <= 0) {
		dev_err(&client->dev,
			"<%s> ERROR: Reading EDID from "
			"HDMI_I2C_MONITOR_ADDRESS\n", __func__);
		return err;
	}

	for (i = 0; i < *out_len; i++) {
		if ((i / 18) < 3) {
			dev_dbg(&client->dev, "byte->%02x	%x\n",
				i, p_buffer[i]);
			continue;
		}
		if ((i/18 >= 3 && i/18 <= 6) && (i%18 == 0))
			dev_dbg(&client->dev, "\n DTD Block %d\n", k++);

		if ((i/18 == 7) && (i%18 == 0))
			dev_dbg(&client->dev, "\n");

		dev_dbg(&client->dev, "byte->%02x	%x\n", i, p_buffer[i]);
	}

	/* Release DDC bus access */
	client->addr = SI9022_I2CSLAVEADDRESS;
	val &= ~(TPI_SYS_CTRL_DDC_BUS_REQUEST | TPI_SYS_CTRL_DDC_BUS_GRANTED);

	err = sil9022_write_reg(client, HDMI_SYS_CTRL_DATA_REG, val);

	if (err < 0) {
		dev_err(&client->dev,
			"<%s> ERROR: Releasing DDC  Bus Access\n",
			__func__);
		return err;
	}

	/*  Success */
	return 0;
}

static int
hdmi_enable_audio(struct i2c_client *client)
{
	int err = 0;
	u8  val = 0;
	u8  crc = 0;
	u32 count = 0;
	int index = 0;

	for (index = 0;
	      index < sizeof(hdmi_tpi_audio_config_data) /
	      sizeof(struct hdmi_reg_data);
	      index++) {
		err = sil9022_write_reg(
			client,
			hdmi_tpi_audio_config_data[index].reg_offset,
			hdmi_tpi_audio_config_data[index].value);
		if (err != 0) {
			dev_err(&client->dev,
				"<%s> ERROR: Writing "
				"tpi_audio_config_data[%d]={ %d, %d }\n",
				__func__, index,
				hdmi_tpi_audio_config_data[index].reg_offset,
				hdmi_tpi_audio_config_data[index].value);
			return err;
			}
		}

	/* Fill the checksum byte for Infoframe data*/
	count = 0;
	while (count < MISC_INFOFRAME_SIZE_MEMORY) {
		crc += misc_audio_info_frame_data[count];
		count++;
	}
	crc = 0x100 - crc;

	/* Fill CRC Byte*/
	misc_audio_info_frame_data[0x3] = crc;

	for (count = 0; count < MISC_INFOFRAME_SIZE_MEMORY; count++) {
		err = sil9022_write_reg(client,
					(HDMI_CPI_MISC_IF_OFFSET + count),
					misc_audio_info_frame_data[count]);
		if (err < 0) {
			dev_err(&client->dev,
				"<%s> ERROR: writing audio info frame"
				" CRC data: %d\n", __func__, count);
			return err;
		}
	}

	/* Decode Level 0 Packets */
	val = 0x2;
	sil9022_write_reg(client, 0xBC, val);
	if (err < 0) {
		dev_err(&client->dev,
			"<%s> ERROR: writing level 0 packets to 0xBC\n",
			__func__);
		return err;
	}

	val = 0x24;
	err = sil9022_write_reg(client, 0xBD, val);
	if (err < 0) {
		dev_err(&client->dev,
			"<%s> ERROR: writing level 0 packets to 0xBD\n",
			__func__);
		return err;
	}

	val = 0x2;
	err = sil9022_write_reg(client, 0xBE, val);
	if (err < 0) {
		dev_err(&client->dev,
			"<%s> ERROR: writing level 0 packets to 0xBE\n",
			__func__);
		return err;
	}

	/* Disable Mute */
	val = TPI_AUDIO_INTERFACE_I2S |
		  TPI_AUDIO_MUTE_DISABLE |
		  TPI_AUDIO_CODING_PCM;
	err = sil9022_write_reg(client, HDMI_TPI_AUDIO_CONFIG_BYTE2_REG, val);
	if (err < 0) {
		dev_err(&client->dev,
			"<%s> ERROR: Disabling mute\n",
			__func__);
		return err;
	}

	dev_info(&client->dev, "<%s> hdmi audio enabled\n",
		__func__);
	return 0;

}

static int
hdmi_disable_audio(struct i2c_client *client)
{
	u8 val = 0;
	int err = 0;
	/* Disable Audio */
	val = TPI_AUDIO_INTERFACE_DISABLE;
	err = sil9022_write_reg(client, HDMI_TPI_AUDIO_CONFIG_BYTE2_REG, val);
	if (err < 0)
		dev_err(&client->dev,
			"<%s> ERROR: Disisable audio interface", __func__);

	dev_dbg(&client->dev, "<%s> hdmi audio disabled\n", __func__);
	return err;
}

static int
hdmi_enable(struct omap_dss_device *dssdev)
{
	int		err;
	u8		val, vals[14];
	int		i;
	u16		out_len = 0;
	u8		edid[HDMI_EDID_MAX_LENGTH];
	u16		horizontal_res;
	u16		vertical_res;
	u16		pixel_clk;


	memset(edid, 0, HDMI_EDID_MAX_LENGTH);
	memset(vals, 0, 14);

	err = hdmi_read_edid(sil9022_client, HDMI_EDID_MAX_LENGTH,
			     edid, &out_len);
	if (err < 0 || out_len == 0) {
		dev_err(&sil9022_client->dev,
			"<%s> Unable to read EDID for monitor\n", __func__);
		return err;
	}

	get_edid_timing_data(edid,
			     &pixel_clk,
			     &horizontal_res,
			     &vertical_res
			     );

	/*  Fill the TPI Video Mode Data structure */
	vals[0] = (pixel_clk & 0xFF);                  /* Pixel clock */
	vals[1] = ((pixel_clk & 0xFF00) >> 8);
	vals[2] = VERTICAL_FREQ;                    /* Vertical freq */
	vals[3] = 0x00;
	vals[4] = (horizontal_res & 0xFF);         /* Horizontal pixels*/
	vals[5] = ((horizontal_res & 0xFF00) >> 8);
	vals[6] = (vertical_res & 0xFF);           /* Vertical pixels */
	vals[7] = ((vertical_res & 0xFF00) >> 8);


	dev_info(&sil9022_client->dev, "<%s>\nHDMI Monitor E-EDID Timing Data\n"
					"horizontal_res	= %d\n"
					"vertical_res	= %d\n"
					"pixel_clk	= %d\n"
					"hfp		= %d\n"
					"hsw		= %d\n"
					"hbp		= %d\n"
					"vfp		= %d\n"
					"vsw		= %d\n"
					"vbp		= %d\n",
		 __func__,
		 omap_dss_hdmi_timings.x_res,
		 omap_dss_hdmi_timings.y_res,
		 omap_dss_hdmi_timings.pixel_clock,
		 omap_dss_hdmi_timings.hfp,
		 omap_dss_hdmi_timings.hsw,
		 omap_dss_hdmi_timings.hbp,
		 omap_dss_hdmi_timings.vfp,
		 omap_dss_hdmi_timings.vsw,
		 omap_dss_hdmi_timings.vbp
		 );

	dssdev->panel.timings = omap_dss_hdmi_timings;
	/*  Write out the TPI Video Mode Data */
	out_len = 0;
	err = sil9022_blockwrite_reg(sil9022_client,
				     HDMI_TPI_VIDEO_DATA_BASE_REG,
				     8, vals, &out_len);
	if (err < 0) {
		dev_err(&sil9022_client->dev,
			"<%s> ERROR: writing TPI video mode data\n", __func__);
		return err;
	}

	/* Write out the TPI Pixel Repetition Data (24 bit wide bus,
	falling edge, no pixel replication) */
	val = TPI_AVI_PIXEL_REP_BUS_24BIT |
		TPI_AVI_PIXEL_REP_FALLING_EDGE |
		TPI_AVI_PIXEL_REP_NONE;
	err = sil9022_write_reg(sil9022_client,
				HDMI_TPI_PIXEL_REPETITION_REG,
				val);
	if (err < 0) {
		dev_err(&sil9022_client->dev,
			"<%s> ERROR: writing TPI pixel repetition data\n",
			__func__);
		return err;
	}

	 /*  Write out the TPI AVI Input Format */
	val = TPI_AVI_INPUT_BITMODE_8BIT |
		TPI_AVI_INPUT_RANGE_AUTO |
		TPI_AVI_INPUT_COLORSPACE_RGB;
	err = sil9022_write_reg(sil9022_client,
				HDMI_TPI_AVI_IN_FORMAT_REG,
				val);
	if (err < 0) {
		dev_err(&sil9022_client->dev,
			"<%s> ERROR: writing TPI AVI Input format\n", __func__);
		return err;
	}

	/*  Write out the TPI AVI Output Format */
	val = TPI_AVI_OUTPUT_CONV_BT709 |
		TPI_AVI_OUTPUT_RANGE_AUTO |
		TPI_AVI_OUTPUT_COLORSPACE_RGBHDMI;
	err = sil9022_write_reg(sil9022_client,
				HDMI_TPI_AVI_OUT_FORMAT_REG, val);
	if (err < 0) {
		dev_err(&sil9022_client->dev,
			"<%s> ERROR: writing TPI AVI output format\n",
			__func__);
		return err;
	}

	/* Write out the TPI System Control Data to power down */
	val = TPI_SYS_CTRL_POWER_DOWN;
	err = sil9022_write_reg(sil9022_client, HDMI_SYS_CTRL_DATA_REG, val);
	if (err < 0) {
		dev_err(&sil9022_client->dev,
			"<%s> ERROR: writing TPI power down control data\n",
			__func__);
		return err;
	}

	/* Write out the TPI AVI InfoFrame Data (all defaults) */
	/* Compute CRC*/
	val = 0x82 + 0x02 + 13;

	for (i = 0; i < sizeof(avi_info_frame_data); i++)
		val += avi_info_frame_data[i];

	avi_info_frame_data[0] = 0x100 - val;

	out_len = 0;
	err = sil9022_blockwrite_reg(sil9022_client,
				     HDMI_TPI_AVI_DBYTE_BASE_REG,
				     sizeof(avi_info_frame_data),
				     avi_info_frame_data, &out_len);
	if (err < 0) {
		dev_err(&sil9022_client->dev,
			"<%s> ERROR: writing TPI AVI infoframe data\n",
			__func__);
		return err;
	}

	/*  Audio Configuration  */
	err = hdmi_enable_audio(sil9022_client);
	if (err < 0) {
		dev_err(&sil9022_client->dev,
			"<%s> ERROR: Unable set audio configuration\n",
			__func__);
		return err;
	}

	/*  Write out the TPI Device Power State (D0) */
	val = TPI_AVI_POWER_STATE_D0;
	err = sil9022_write_reg(sil9022_client,
				HDMI_TPI_POWER_STATE_CTRL_REG, val);
	if (err < 0) {
		dev_err(&sil9022_client->dev,
			"<%s> ERROR: Setting device power state to D0\n",
			__func__);
		return err;
	}

	/* Write out the TPI System Control Data to power up and
	 * select output mode
	 */
	val = TPI_SYS_CTRL_POWER_ACTIVE | TPI_SYS_CTRL_OUTPUT_MODE_HDMI;
	err = sil9022_write_reg(sil9022_client, HDMI_SYS_CTRL_DATA_REG, val);
	if (err < 0) {
		dev_err(&sil9022_client->dev,
			"<%s> ERROR: Writing system control data\n", __func__);
		return err;
	}

	/*  Read back TPI System Control Data to latch settings */
	msleep(10);
	err = sil9022_read_reg(sil9022_client, 1, HDMI_SYS_CTRL_DATA_REG, &val);
	if (err < 0) {
		dev_err(&sil9022_client->dev,
			"<%s> ERROR: Writing system control data\n",
			__func__);
		return err;
	}

	/* HDCP Enable - Disable */
	val = 0;
	err = sil9022_write_reg(sil9022_client,
				HDMI_TPI_HDCP_CONTROLDATA_REG, val);
	if (err < 0) {
		dev_err(&sil9022_client->dev,
			"<%s> ERROR: Enable (1) / Disable (0) => HDCP: %d\n",
			__func__, val);
		return err;
	}

	dev_info(&sil9022_client->dev, "<%s> hdmi enabled\n", __func__);

	return 0;

}

static int
hdmi_disable(void)
{
	u8 val = 0;
	int err = 0;

	err = hdmi_disable_audio(sil9022_client);
	if (err < 0) {
		dev_err(&sil9022_client->dev,
			"<%s> ERROR: failed to disable audio\n", __func__);
		return err;
	}

	/*  Write out the TPI System Control Data to power down  */
	val = TPI_SYS_CTRL_POWER_DOWN;
	err = sil9022_write_reg(sil9022_client, HDMI_SYS_CTRL_DATA_REG, val);
	if (err < 0) {
		dev_err(&sil9022_client->dev,
			"<%s> ERROR: writing control data - power down\n",
			__func__);
		return err;
	}

	/*  Write out the TPI Device Power State (D2) */
	val = TPI_AVI_POWER_STATE_D2;
	err = sil9022_write_reg(sil9022_client,
			  HDMI_TPI_DEVICE_POWER_STATE_DATA, val);
	if (err < 0) {
		dev_err(&sil9022_client->dev,
			"<%s> ERROR: Setting device power state to D2\n",
			__func__);
		return err;
	}

	/*  Read back TPI System Control Data to latch settings */
	mdelay(10);
	err = sil9022_read_reg(sil9022_client, 1, HDMI_SYS_CTRL_DATA_REG, &val);
	if (err < 0) {
		dev_err(&sil9022_client->dev,
			"<%s> ERROR:  Reading System control data "
			"- latch settings\n", __func__);
		return err;
	}

	dev_dbg(&sil9022_client->dev, "<%s> hdmi disabled\n", __func__);

	return 0;

}

static int sil9022_set_cm_clkout_ctrl(struct i2c_client *client)
{
	int err = 0;
	u8 ver;
	u32 clkout_ctrl = 0;
	/* from TRM*/
	/* intitialize the CM_CLKOUT_CTRL register*/
	clkout_ctrl = CLKOUT2_EN |	/* sys_clkout2 is enabled*/
			CLKOUT2_DIV |	/* sys_clkout2 / 16*/
			CLKOUT2SOURCE;	/* CM_96M_FCLK */

	omap_writel(clkout_ctrl, CM_CLKOUT_CTRL); /*CM_CLKOUT_CTRL*/

	/* probe for sil9022 chip version*/
	err = sil9022_write_reg(client, SI9022_REG_TPI_RQB, 0x00);
	if (err < 0) {
		dev_err(&client->dev,
			"<%s> ERROR: Writing HDMI configuration to "
			"reg - SI9022_REG_TPI_RQB\n", __func__);
		err = -ENODEV;
		goto ERROR1;
	}

	err = sil9022_read_reg(client, 1, SI9022_REG_CHIPID0, &ver);
	if (err < 0) {
		dev_err(&client->dev,
			"<%s> ERROR: Reading HDMI version Id\n", __func__);
		err = -ENODEV;
		goto ERROR1;
	} else if (ver != SI9022_CHIPID_902x) {
		dev_err(&client->dev,
			"<%s> Not a valid verId: 0x%x\n", __func__, ver);
		err = -ENODEV;
		goto ERROR1;
	} else
		dev_info(&client->dev,
			 "<%s> sil9022 HDMI Chip version = %x\n",
			 __func__, ver);

	return 0;
ERROR1:
	return err;
}

/* omap_dss_hdmi_driver */
static int hdmi_panel_probe(struct omap_dss_device *dssdev)
{

	dssdev->panel.config = OMAP_DSS_LCD_TFT | OMAP_DSS_LCD_IVS |
				OMAP_DSS_LCD_IHS;

	dssdev->panel.timings = omap_dss_hdmi_timings;

	return 0;
}

static void hdmi_panel_remove(struct omap_dss_device *dssdev)
{
	omap_dss_unregister_driver(dssdev->driver);
}

static int hdmi_panel_enable(struct omap_dss_device *dssdev)
{
	int r = 0;

#ifdef CONFIG_PM
	struct hdmi_platform_data *pdata = dssdev->dev.platform_data;
	if (pdata->set_min_bus_tput) {
		if (cpu_is_omap3630()) {
			pdata->set_min_bus_tput(&sil9022_client->dev,
						OCP_INITIATOR_AGENT,
						200 * 1000 * 4);
			pdata->set_max_mpu_wakeup_lat(&sil9022_client->dev,
						      260);
		} else
			pdata->set_min_bus_tput(&sil9022_client->dev,
						OCP_INITIATOR_AGENT,
						166 * 1000 * 4);
	}
#endif

	if (dssdev->platform_enable)
		r = dssdev->platform_enable(dssdev);

	r = sil9022_set_cm_clkout_ctrl(sil9022_client);
	if (r)
		goto ERROR0;

	r = hdmi_enable(dssdev);
	if (r)
		goto ERROR0;
	/* wait couple of vsyncs until enabling the LCD */
	msleep(50);

	return 0;
ERROR0:
	return r;
}

static void hdmi_panel_disable(struct omap_dss_device *dssdev)
{
#ifdef CONFIG_PM
	struct hdmi_platform_data *pdata = dssdev->dev.platform_data;
#endif
	hdmi_disable();

	/* wait couple of vsyncs until enabling the hdmi */
	msleep(50);

	if (dssdev->platform_disable)
		dssdev->platform_disable(dssdev);
#ifdef CONFIG_PM
	if (pdata->set_min_bus_tput) {
		pdata->set_min_bus_tput(&sil9022_client->dev,
					OCP_INITIATOR_AGENT,
					0);
		if (cpu_is_omap3630())
			pdata->set_max_mpu_wakeup_lat(&sil9022_client->dev, -1);
	}
#endif
}

static int hdmi_panel_suspend(struct omap_dss_device *dssdev)
{
	hdmi_panel_disable(dssdev);
	return 0;
}

static int hdmi_panel_resume(struct omap_dss_device *dssdev)
{
	return hdmi_panel_enable(dssdev);
}

static struct omap_dss_driver hdmi_driver = {
	.probe		= hdmi_panel_probe,
	.remove		= hdmi_panel_remove,

	.enable		= hdmi_panel_enable,
	.disable	= hdmi_panel_disable,
	.suspend	= hdmi_panel_suspend,
	.resume		= hdmi_panel_resume,

	.driver         = {
		.name   = "hdmi_panel",
		.owner  = THIS_MODULE,
	},
};
/* hdmi omap_dss_driver end */


static int __devinit
sil9022_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int err = 0;

	sil9022_client = kmalloc(sizeof(struct i2c_client), GFP_KERNEL);
	if (!sil9022_client) {
		err = -ENOMEM;
		goto ERROR0;
	}
	memset(sil9022_client, 0, sizeof(struct i2c_client));

	strncpy(sil9022_client->name, SIL9022_DRV_NAME, I2C_NAME_SIZE);
	sil9022_client->addr = SI9022_I2CSLAVEADDRESS;
	sil9022_client->adapter = client->adapter;

	i2c_set_clientdata(client, sil9022_client);

	err = sil9022_set_cm_clkout_ctrl(client);
	if (err)
		goto ERROR1;

	omap_dss_register_driver(&hdmi_driver);

	return 0;
ERROR1:
	kfree(sil9022_client);
ERROR0:
	return err;
}


static int
sil9022_remove(struct i2c_client *client)

{
	int err = 0;

	if (!client->adapter) {
		dev_err(&sil9022_client->dev, "<%s> No HDMI Device\n",
			__func__);
		return -ENODEV;
	}
	kfree(sil9022_client);

	return err;
}

static const struct i2c_device_id sil9022_id[] = {
	{ SIL9022_DRV_NAME, 0 },
	{ },
};

MODULE_DEVICE_TABLE(i2c, sil9022_id);

static struct i2c_driver sil9022_driver = {
	.driver = {
		.name  = SIL9022_DRV_NAME,
		.owner = THIS_MODULE,
		},
	.probe		= sil9022_probe,
	.remove		= sil9022_remove,
	.id_table	= sil9022_id,
};

static int __init
sil9022_init(void)
{
	int err = 0;

	err = i2c_add_driver(&sil9022_driver);
	if (err < 0) {
		printk(KERN_ERR "<%s> Driver registration failed\n", __func__);
		err = -ENODEV;
		goto ERROR0;
	}

	if (sil9022_client == NULL) {
		printk(KERN_ERR "<%s> sil9022_client not allocated,\n"
				"<%s> No HDMI Device\n", __func__, __func__);
		err = -ENODEV;
		goto ERROR0;
	}

	return 0;
ERROR0:
	return err;
}

static void __exit
sil9022_exit(void)
{
	i2c_del_driver(&sil9022_driver);
}

/*late_initcall(sil9022_init);*/
module_init(sil9022_init);
module_exit(sil9022_exit);

MODULE_AUTHOR("Texas Instruments");
MODULE_DESCRIPTION("SIL9022 HDMI Driver");
MODULE_LICENSE("GPL");



