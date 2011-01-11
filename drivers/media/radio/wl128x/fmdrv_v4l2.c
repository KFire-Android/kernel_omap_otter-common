/*
 *  FM Driver for Connectivity chip of Texas Instruments.
 *  This file provides interfaces to V4L2 subsystem.
 *
 *  This module registers with V4L2 subsystem as Radio
 *  data system interface (/dev/radio). During the registration,
 *  it will expose two set of function pointers.
 *
 *    1) File operation related API (open, close, read, write, poll...etc).
 *    2) Set of V4L2 IOCTL complaint API.
 *
 *  Copyright (C) 2011 Texas Instruments
 *  Author: Raja Mani <raja_mani@ti.com>
 *  Author: Manjunatha Halli <manjunatha_halli@ti.com>
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

#include "fmdrv.h"
#include "fmdrv_v4l2.h"
#include "fmdrv_common.h"
#include "fmdrv_rx.h"
#include "fmdrv_tx.h"

static struct video_device *gradio_dev;
static u8 radio_disconnected;

/* Query control */
static struct v4l2_queryctrl fmdrv_v4l2_queryctrl[] = {
	{
		.id = V4L2_CID_AUDIO_VOLUME,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Volume",
		.minimum = FM_RX_VOLUME_MIN,
		.maximum = FM_RX_VOLUME_MAX,
		.step = 1,
		.default_value = FM_DEFAULT_RX_VOLUME,
	},
	{
		.id = V4L2_CID_AUDIO_BALANCE,
		.flags = V4L2_CTRL_FLAG_DISABLED,
	},
	{
		.id = V4L2_CID_AUDIO_BASS,
		.flags = V4L2_CTRL_FLAG_DISABLED,
	},
	{
		.id = V4L2_CID_AUDIO_TREBLE,
		.flags = V4L2_CTRL_FLAG_DISABLED,
	},
	{
		.id = V4L2_CID_AUDIO_MUTE,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.name = "Mute",
		.minimum = 0,
		.maximum = 2,
		.step = 1,
		.default_value = FM_MUTE_OFF,
	},
	{
		.id = V4L2_CID_AUDIO_LOUDNESS,
		.flags = V4L2_CTRL_FLAG_DISABLED,
	},
};

/* -- V4L2 RADIO (/dev/radioX) device file operation interfaces --- */

/* Read RX RDS data */
static ssize_t fm_v4l2_fops_read(struct file *file, char __user * buf,
		size_t count, loff_t *ppos)
{
	u8 rds_mode;
	int ret;
	struct fmdev *fmdev;

	fmdev = video_drvdata(file);

	if (!radio_disconnected) {
		fmerr("FM device is already disconnected\n");
		return -EIO;
	}

	/* Turn on RDS mode , if it is disabled */
	ret = fm_rx_get_rds_mode(fmdev, &rds_mode);
	if (ret < 0) {
		fmerr("Unable to read current rds mode\n");
		return ret;
	}

	if (rds_mode == FM_RDS_DISABLE) {
		ret = fmc_set_rds_mode(fmdev, FM_RDS_ENABLE);
		if (ret < 0) {
			fmerr("Failed to enable rds mode\n");
			return ret;
		}
	}

	/* Copy RDS data from internal buffer to user buffer */
	return fmc_transfer_rds_from_internal_buff(fmdev, file, buf, count);
}

/* Write TX RDS data */
static ssize_t fm_v4l2_fops_write(struct file *file, const char __user * buf,
		size_t count, loff_t *ppos)
{
	struct tx_rds rds;
	int ret;
	struct fmdev *fmdev;

	ret = copy_from_user(&rds, buf, sizeof(rds));
	fmdbg("(%d)type: %d, text %s, af %d\n",
		   ret, rds.text_type, rds.text, rds.af_freq);

	fmdev = video_drvdata(file);
	fm_tx_set_radio_text(fmdev, rds.text, rds.text_type);
	fm_tx_set_af(fmdev, rds.af_freq);

	return 0;
}

static u32 fm_v4l2_fops_poll(struct file *file, struct poll_table_struct *pts)
{
	int ret;
	struct fmdev *fmdev;

	fmdev = video_drvdata(file);
	ret = fmc_is_rds_data_available(fmdev, file, pts);
	if (ret < 0)
		return POLLIN | POLLRDNORM;

	return 0;
}

/*
 * Handle open request for "/dev/radioX" device.
 * Start with FM RX mode as default.
 */
static int fm_v4l2_fops_open(struct file *file)
{
	int ret;
	struct fmdev *fmdev = NULL;

	/* Don't allow multiple open */
	if (radio_disconnected) {
		fmerr("FM device is already opened\n");
		return -EBUSY;
	}

	fmdev = video_drvdata(file);

	ret = fmc_prepare(fmdev);
	if (ret < 0) {
		fmerr("Unable to prepare FM CORE\n");
		return ret;
	}

	fmdbg("Load FM RX firmware..\n");

	ret = fmc_set_mode(fmdev, FM_MODE_RX);
	if (ret < 0) {
		fmerr("Unable to load FM RX firmware\n");
		return ret;
	}
	radio_disconnected = 1;

	return ret;
}

static int fm_v4l2_fops_release(struct file *file)
{
	int ret;
	struct fmdev *fmdev;

	fmdev = video_drvdata(file);
	if (!radio_disconnected) {
		fmdbg("FM device is already closed\n");
		return 0;
	}

	ret = fmc_set_mode(fmdev, FM_MODE_OFF);
	if (ret < 0) {
		fmerr("Unable to turn off the chip\n");
		return ret;
	}

	ret = fmc_release(fmdev);
	if (ret < 0) {
		fmerr("FM CORE release failed\n");
		return ret;
	}
	radio_disconnected = 0;

	return ret;
}

/* V4L2 RADIO (/dev/radioX) device IOCTL interfaces */
static int fm_v4l2_vidioc_querycap(struct file *file, void *priv,
		struct v4l2_capability *capability)
{
	strlcpy(capability->driver, FM_DRV_NAME, sizeof(capability->driver));
	strlcpy(capability->card, FM_DRV_CARD_SHORT_NAME,
			sizeof(capability->card));
	sprintf(capability->bus_info, "UART");
	capability->version = FM_DRV_RADIO_VERSION;
	capability->capabilities = V4L2_CAP_HW_FREQ_SEEK | V4L2_CAP_TUNER |
		V4L2_CAP_RADIO | V4L2_CAP_MODULATOR |
		V4L2_CAP_AUDIO | V4L2_CAP_READWRITE |
		V4L2_CAP_RDS_CAPTURE;

	return 0;
}

static int fm_v4l2_vidioc_queryctrl(struct file *file, void *priv,
		struct v4l2_queryctrl *qc)
{
	int index;
	int ret = -EINVAL;

	if (qc->id < V4L2_CID_BASE)
		return ret;

	/* Search control ID and copy its properties */
	for (index = 0; index < NO_OF_ENTRIES_IN_ARRAY(fmdrv_v4l2_queryctrl);\
			index++) {
		if (qc->id && qc->id == fmdrv_v4l2_queryctrl[index].id) {
			memcpy(qc, &(fmdrv_v4l2_queryctrl[index]), sizeof(*qc));
			ret = 0;
			break;
		}
	}
	return ret;
}

static int fm_v4l2_vidioc_g_ctrl(struct file *file, void *priv,
		struct v4l2_control *ctrl)
{
	int ret = -EINVAL;
	unsigned short curr_vol;
	unsigned char curr_mute_mode;
	struct fmdev *fmdev;

	fmdev = video_drvdata(file);

	switch (ctrl->id) {
	case V4L2_CID_AUDIO_MUTE:	/* get mute mode */
		ret = fm_rx_get_mute_mode(fmdev, &curr_mute_mode);
		if (ret < 0)
			return ret;
		ctrl->value = curr_mute_mode;
		break;

	case V4L2_CID_AUDIO_VOLUME:	/* get volume */
		ret = fm_rx_get_volume(fmdev, &curr_vol);
		if (ret < 0)
			return ret;
		ctrl->value = curr_vol;
		break;

	case  V4L2_CID_TUNE_ANTENNA_CAPACITOR:
		ctrl->value = fm_tx_get_tune_cap_val(fmdev);
		break;

	case V4L2_CID_TUNE_PREEMPHASIS:
		ctrl->value = fmdev->tx_data.preemph;
		break;
	}

	return ret;
}

/*
 * Change the value of specified control.
 * V4L2_CID_TUNE_POWER_LEVEL: Application will specify power level value in
 * units of dB/uV, whereas range and step are specific to FM chip. For TI's WL
 * chips, convert application specified power level value to chip specific
 * value by subtracting 122 from it. Refer to TI FM data sheet for details.
 **/
static int fm_v4l2_vidioc_s_ctrl(struct file *file, void *priv,
		struct v4l2_control *ctrl)
{
	struct fmdev *fmdev = video_drvdata(file);
	unsigned int emph_filter;
	int ret = -EINVAL;

	switch (ctrl->id) {
	case V4L2_CID_AUDIO_MUTE:	/* set mute */
		ret = fmc_set_mute_mode(fmdev, (unsigned char)ctrl->value);
		break;

	case V4L2_CID_AUDIO_VOLUME:	/* set volume */
		ret = fm_rx_set_volume(fmdev, (unsigned short)ctrl->value);
		break;

	case V4L2_CID_TUNE_POWER_LEVEL: /* set TX power level - ext control */
		if (ctrl->value >= FM_PWR_LVL_LOW &&
			ctrl->value <= FM_PWR_LVL_HIGH) {
			ctrl->value = FM_PWR_LVL_HIGH - ctrl->value;
			ret = fm_tx_set_pwr_lvl(fmdev,
					(unsigned char)ctrl->value);
		} else
			ret = -ERANGE;
		break;

	case V4L2_CID_TUNE_PREEMPHASIS:
		if (ctrl->value < V4L2_PREEMPHASIS_DISABLED ||
				ctrl->value > V4L2_PREEMPHASIS_75_uS) {
			ret = -EINVAL;
			break;
		}
		if (ctrl->value == V4L2_PREEMPHASIS_DISABLED)
			emph_filter = FM_TX_PREEMPH_OFF;
		else if (ctrl->value == V4L2_PREEMPHASIS_50_uS)
			emph_filter = FM_TX_PREEMPH_50US;
		else
			emph_filter = FM_TX_PREEMPH_75US;
		ret = fm_tx_set_preemph_filter(fmdev, emph_filter);
		break;
	}

	return ret;
}

static int fm_v4l2_vidioc_g_audio(struct file *file, void *priv,
		struct v4l2_audio *audio)
{
	memset(audio, 0, sizeof(*audio));
	strcpy(audio->name, "Radio");
	audio->capability = V4L2_AUDCAP_STEREO;

	return 0;
}

static int fm_v4l2_vidioc_s_audio(struct file *file, void *priv,
		struct v4l2_audio *audio)
{
	if (audio->index != 0)
		return -EINVAL;

	return 0;
}

/* Get tuner attributes. If current mode is NOT RX, return error */
static int fm_v4l2_vidioc_g_tuner(struct file *file, void *priv,
		struct v4l2_tuner *tuner)
{
	struct fmdev *fmdev = video_drvdata(file);
	u32 bottom_freq;
	u32 top_freq;
	u16 stereo_mono_mode;
	u16 rssilvl;
	int ret;

	if (tuner->index != 0)
		return -EINVAL;

	if (fmdev->curr_fmmode != FM_MODE_RX)
		return -EPERM;

	ret = fm_rx_get_band_freq_range(fmdev, &bottom_freq, &top_freq);
	if (ret != 0)
		return ret;

	ret = fm_rx_get_stereo_mono(fmdev, &stereo_mono_mode);
	if (ret != 0)
		return ret;

	ret = fm_rx_get_rssi_level(fmdev, &rssilvl);
	if (ret != 0)
		return ret;

	strcpy(tuner->name, "FM");
	tuner->type = V4L2_TUNER_RADIO;
	/* Store rangelow and rangehigh freq in unit of 62.5 Hz */
	tuner->rangelow = bottom_freq * 16;
	tuner->rangehigh = top_freq * 16;
	tuner->rxsubchans = V4L2_TUNER_SUB_MONO | V4L2_TUNER_SUB_STEREO |
	((fmdev->rx.rds.flag == FM_RDS_ENABLE) ? V4L2_TUNER_SUB_RDS : 0);
	tuner->capability = V4L2_TUNER_CAP_STEREO | V4L2_TUNER_CAP_RDS |
			    V4L2_TUNER_CAP_LOW;
	tuner->audmode = (stereo_mono_mode ?
			  V4L2_TUNER_MODE_MONO : V4L2_TUNER_MODE_STEREO);

	/*
	 * Actual rssi value lies in between -128 to +127.
	 * Convert this range from 0 to 255 by adding +128
	 */
	rssilvl += 128;

	/*
	 * Return signal strength value should be within 0 to 65535.
	 * Find out correct signal radio by multiplying (65535/255) = 257
	 */
	tuner->signal = rssilvl * 257;
	tuner->afc = 0;

	return ret;
}

/*
 * Set tuner attributes. If current mode is NOT RX, set to RX.
 * Currently, we set only audio mode (mono/stereo) and RDS state (on/off).
 * Should we set other tuner attributes, too?
 */
static int fm_v4l2_vidioc_s_tuner(struct file *file, void *priv,
		struct v4l2_tuner *tuner)
{
	struct fmdev *fmdev = video_drvdata(file);
	u16 aud_mode;
	u8 rds_mode;
	int ret;

	if (tuner->index != 0)
		return -EINVAL;

	aud_mode = (tuner->audmode == V4L2_TUNER_MODE_STEREO) ?
			FM_STEREO_MODE : FM_MONO_MODE;
	rds_mode = (tuner->rxsubchans & V4L2_TUNER_SUB_RDS) ?
			FM_RDS_ENABLE : FM_RDS_DISABLE;

	if (fmdev->curr_fmmode != FM_MODE_RX) {
		ret = fmc_set_mode(fmdev, FM_MODE_RX);
		if (ret < 0) {
			fmerr("Failed to set RX mode\n");
			return ret;
		}
	}

	ret = fmc_set_stereo_mono(fmdev, aud_mode);
	if (ret < 0) {
		fmerr("Failed to set RX stereo/mono mode\n");
		return ret;
	}

	ret = fmc_set_rds_mode(fmdev, rds_mode);
	if (ret < 0)
		fmerr("Failed to set RX RDS mode\n");

	return ret;
}

/* Get tuner or modulator radio frequency */
static int fm_v4l2_vidioc_g_freq(struct file *file, void *priv,
		struct v4l2_frequency *freq)
{
	struct fmdev *fmdev = video_drvdata(file);
	int ret;

	ret = fmc_get_freq(fmdev, &freq->frequency);
	if (ret < 0) {
		fmerr("Failed to get frequency\n");
		return ret;
	}

	/* Frequency unit of 62.5 Hz*/
	freq->frequency = (u32) freq->frequency * 16;

	return 0;
}

/* Set tuner or modulator radio frequency */
static int fm_v4l2_vidioc_s_freq(struct file *file, void *priv,
		struct v4l2_frequency *freq)
{
	struct fmdev *fmdev = video_drvdata(file);

	/*
	 * As V4L2_TUNER_CAP_LOW is set 1 user sends the frequency
	 * in units of 62.5 Hz.
	 */
	freq->frequency = (u32)(freq->frequency / 16);

	return fmc_set_freq(fmdev, freq->frequency);
}

/* Set hardware frequency seek. If current mode is NOT RX, set it RX. */
static int fm_v4l2_vidioc_s_hw_freq_seek(struct file *file, void *priv,
		struct v4l2_hw_freq_seek *seek)
{
	struct fmdev *fmdev = video_drvdata(file);
	int ret;

	if (fmdev->curr_fmmode != FM_MODE_RX) {
		ret = fmc_set_mode(fmdev, FM_MODE_RX);
		if (ret != 0) {
			fmerr("Failed to set RX mode\n");
			return ret;
		}
	}

	ret = fm_rx_seek(fmdev, seek->seek_upward, seek->wrap_around);
	if (ret < 0)
		fmerr("RX seek failed - %d\n", ret);

	return ret;
}

static int fm_v4l2_vidioc_g_ext_ctrls(struct file *file, void *priv,
		struct v4l2_ext_controls *ext_ctrls)
{
	struct v4l2_control ctrl;
	int index;
	int ret = -EINVAL;

	if (V4L2_CTRL_CLASS_FM_TX == ext_ctrls->ctrl_class) {
		for (index = 0; index < ext_ctrls->count; index++) {
			ctrl.id = ext_ctrls->controls[index].id;
			ctrl.value = ext_ctrls->controls[index].value;
			ret = fm_v4l2_vidioc_g_ctrl(file, priv, &ctrl);
			if (ret < 0) {
				ext_ctrls->error_idx = index;
				break;
			}
			ext_ctrls->controls[index].value = ctrl.value;
		}
	}

	return ret;
}

static int fm_v4l2_vidioc_s_ext_ctrls(struct file *file, void *priv,
		struct v4l2_ext_controls *ext_ctrls)
{
	struct v4l2_control ctrl;
	int index;
	int ret = -EINVAL;

	if (V4L2_CTRL_CLASS_FM_TX == ext_ctrls->ctrl_class) {
		for (index = 0; index < ext_ctrls->count; index++) {
			ctrl.id = ext_ctrls->controls[index].id;
			ctrl.value = ext_ctrls->controls[index].value;
			ret = fm_v4l2_vidioc_s_ctrl(file, priv, &ctrl);
			if (ret < 0) {
				ext_ctrls->error_idx = index;
				break;
			}
			ext_ctrls->controls[index].value = ctrl.value;
		}
	}

	return ret;
}

/* Get modulator attributes. If mode is not TX, return no attributes. */
static int fm_v4l2_vidioc_g_modulator(struct file *file, void *priv,
		struct v4l2_modulator *mod)
{
	struct fmdev *fmdev = video_drvdata(file);;

	if (mod->index != 0)
		return -EINVAL;

	if (fmdev->curr_fmmode != FM_MODE_TX)
		return -EPERM;

	mod->txsubchans = ((fmdev->tx_data.aud_mode == FM_STEREO_MODE) ?
			V4L2_TUNER_SUB_STEREO : V4L2_TUNER_SUB_MONO) |
		((fmdev->tx_data.rds.flag == FM_RDS_ENABLE) ?
		 V4L2_TUNER_SUB_RDS : 0);

	mod->capability = V4L2_TUNER_CAP_STEREO | V4L2_TUNER_CAP_RDS |
		V4L2_TUNER_CAP_LOW;

	return 0;
}

/* Set modulator attributes. If mode is not TX, set to TX. */
static int fm_v4l2_vidioc_s_modulator(struct file *file, void *priv,
		struct v4l2_modulator *mod)
{
	struct fmdev *fmdev = video_drvdata(file);
	u8 rds_mode;
	u16 aud_mode;
	int ret;

	if (mod->index != 0)
		return -EINVAL;

	if (fmdev->curr_fmmode != FM_MODE_TX) {
		ret = fmc_set_mode(fmdev, FM_MODE_TX);
		if (ret != 0) {
			fmerr("Failed to set TX mode\n");
			return ret;
		}
	}

	aud_mode = (mod->txsubchans & V4L2_TUNER_SUB_STEREO) ?
		FM_STEREO_MODE : FM_MONO_MODE;
	rds_mode = (mod->txsubchans & V4L2_TUNER_SUB_RDS) ?
		FM_RDS_ENABLE : FM_RDS_DISABLE;
	ret = fm_tx_set_stereo_mono(fmdev, aud_mode);
	if (ret < 0) {
		fmerr("Failed to set mono/stereo mode for TX\n");
		return ret;
	}
	ret = fm_tx_set_rds_mode(fmdev, rds_mode);
	if (ret < 0)
		fmerr("Failed to set rds mode for TX\n");

	return ret;
}

static const struct v4l2_file_operations fm_drv_fops = {
	.owner = THIS_MODULE,
	.read = fm_v4l2_fops_read,
	.write = fm_v4l2_fops_write,
	.poll = fm_v4l2_fops_poll,
	.ioctl = video_ioctl2,
	.open = fm_v4l2_fops_open,
	.release = fm_v4l2_fops_release,
};

static const struct v4l2_ioctl_ops fm_drv_ioctl_ops = {
	.vidioc_querycap = fm_v4l2_vidioc_querycap,
	.vidioc_queryctrl = fm_v4l2_vidioc_queryctrl,
	.vidioc_g_ctrl = fm_v4l2_vidioc_g_ctrl,
	.vidioc_s_ctrl = fm_v4l2_vidioc_s_ctrl,
	.vidioc_g_ext_ctrls = fm_v4l2_vidioc_g_ext_ctrls,
	.vidioc_s_ext_ctrls = fm_v4l2_vidioc_s_ext_ctrls,
	.vidioc_g_audio = fm_v4l2_vidioc_g_audio,
	.vidioc_s_audio = fm_v4l2_vidioc_s_audio,
	.vidioc_g_tuner = fm_v4l2_vidioc_g_tuner,
	.vidioc_s_tuner = fm_v4l2_vidioc_s_tuner,
	.vidioc_g_frequency = fm_v4l2_vidioc_g_freq,
	.vidioc_s_frequency = fm_v4l2_vidioc_s_freq,
	.vidioc_s_hw_freq_seek = fm_v4l2_vidioc_s_hw_freq_seek,
	.vidioc_g_modulator = fm_v4l2_vidioc_g_modulator,
	.vidioc_s_modulator = fm_v4l2_vidioc_s_modulator
};

/* V4L2 RADIO device parent structure */
static struct video_device fm_viddev_template = {
	.fops = &fm_drv_fops,
	.ioctl_ops = &fm_drv_ioctl_ops,
	.name = FM_DRV_NAME,
	.release = video_device_release,
};

int fm_v4l2_init_video_device(struct fmdev *fmdev, int radio_nr)
{
	/* Allocate new video device */
	gradio_dev = video_device_alloc();
	if (NULL == gradio_dev) {
		fmerr("Can't allocate video device\n");
		return -ENOMEM;
	}

	/* Setup FM driver's V4L2 properties */
	memcpy(gradio_dev, &fm_viddev_template, sizeof(fm_viddev_template));

	video_set_drvdata(gradio_dev, fmdev);

	/* Register with V4L2 subsystem as RADIO device */
	if (video_register_device(gradio_dev, VFL_TYPE_RADIO, radio_nr)) {
		video_device_release(gradio_dev);
		fmerr("Could not register video device\n");
		return -ENOMEM;
	}

	fmdev->radio_dev = gradio_dev;

	return 0;
}

void *fm_v4l2_deinit_video_device(void)
{
	struct fmdev *fmdev;

	fmdev = video_get_drvdata(gradio_dev);

	/* Unregister RADIO device from V4L2 subsystem */
	video_unregister_device(gradio_dev);

	return fmdev;
}
