/*
 *  FM Driver for Connectivity chip of Texas Instruments.
 *  This sub-module of FM driver implements FM RX functionality.
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

#include "fmdrv.h"
#include "fmdrv_common.h"
#include "fmdrv_rx.h"

#ifndef DEBUG
#ifdef pr_info
#undef pr_info
#define pr_info(fmt, arg...)
#endif
#endif

void fm_rx_reset_rds_cache(struct fmdrv_ops *fmdev)
{
	fmdev->rx.rds.flag = FM_RDS_DISABLE;
	fmdev->rx.rds.last_block_index = 0;
	fmdev->rx.rds.wr_index = 0;
	fmdev->rx.rds.rd_index = 0;

	if (fmdev->rx.af_mode == FM_RX_RDS_AF_SWITCH_MODE_ON)
		fmdev->irq_info.mask |= FM_LEV_EVENT;

}

void fm_rx_reset_curr_station_info(struct fmdrv_ops *fmdev)
{
	fmdev->rx.cur_station_info.picode = FM_NO_PI_CODE;
	fmdev->rx.cur_station_info.no_of_items_in_afcache = 0;
	fmdev->rx.cur_station_info.af_list_max = 0;
}

int fm_rx_set_frequency(struct fmdrv_ops *fmdev, unsigned int freq_to_set)
{
	unsigned long timeleft;
	unsigned short payload, curr_frq, frq_index;
	unsigned int curr_frq_in_khz;
	int ret, resp_len;

	if (fmdev->curr_fmmode != FM_MODE_RX) {
		ret = -EPERM;
		goto exit;
	}

	if (freq_to_set < fmdev->rx.region.bottom_frequency ||
		freq_to_set > fmdev->rx.region.top_frequency) {
		pr_err("(fmdrv): Invalid frequency %d", freq_to_set);
		ret = -EINVAL;
		goto exit;
	}
	/* Set audio enable */
	FM_STORE_LE16_TO_BE16(payload, FM_RX_FM_AUDIO_ENABLE_I2S_AND_ANALOG);
	ret = fmc_send_cmd(fmdev, AUDIO_ENABLE_SET, &payload, sizeof(payload),
				&fmdev->maintask_completion, NULL, NULL);
	FM_CHECK_SEND_CMD_STATUS(ret);

	/* Set hilo to automatic selection */
	FM_STORE_LE16_TO_BE16(payload, FM_RX_IFFREQ_HILO_AUTOMATIC);
	ret = fmc_send_cmd(fmdev, HILO_SET, &payload, sizeof(payload),
				&fmdev->maintask_completion, NULL, NULL);
	FM_CHECK_SEND_CMD_STATUS(ret);

	/* Calculate frequency index to write */
	frq_index = (freq_to_set - fmdev->rx.region.bottom_frequency) /
		fmdev->rx.region.channel_spacing;

	/* Set frequency index */
	FM_STORE_LE16_TO_BE16(payload, frq_index);
	ret = fmc_send_cmd(fmdev, FREQ_SET, &payload, sizeof(payload),
				&fmdev->maintask_completion, NULL, NULL);
	FM_CHECK_SEND_CMD_STATUS(ret);

	/* Read flags - just to clear any pending interrupts if we had */
	ret = fmc_send_cmd(fmdev, FLAG_GET, NULL, 2,
				&fmdev->maintask_completion, NULL, NULL);
	FM_CHECK_SEND_CMD_STATUS(ret);

	/* Enable FR, BL interrupts */
	fmdev->irq_info.mask |= (FM_FR_EVENT | FM_BL_EVENT);
	FM_STORE_LE16_TO_BE16(payload, fmdev->irq_info.mask);
	ret = fmc_send_cmd(fmdev, INT_MASK_SET, &payload, sizeof(payload),
				&fmdev->maintask_completion, NULL, NULL);
	FM_CHECK_SEND_CMD_STATUS(ret);

	/* Start tune */
	FM_STORE_LE16_TO_BE16(payload, FM_TUNER_PRESET_MODE);
	ret = fmc_send_cmd(fmdev, TUNER_MODE_SET, &payload, sizeof(payload),
			       &fmdev->maintask_completion, NULL, NULL);
	FM_CHECK_SEND_CMD_STATUS(ret);

	/* Wait for tune ended interrupt */
	init_completion(&fmdev->maintask_completion);
	timeleft = wait_for_completion_timeout(&fmdev->maintask_completion,
					       FM_DRV_TX_TIMEOUT);
	if (!timeleft) {
		pr_err("(fmdrv): Timeout(%d sec),didn't get tune ended int",
			   jiffies_to_msecs(FM_DRV_TX_TIMEOUT) / 1000);
		ret = -ETIMEDOUT;
		goto exit;
	}

	/* Read freq back to confirm */
	ret = fmc_send_cmd(fmdev, FREQ_GET, NULL, 2,
		&fmdev->maintask_completion, &curr_frq, &resp_len);
	FM_CHECK_SEND_CMD_STATUS(ret);

	curr_frq = FM_BE16_TO_LE16(curr_frq);
	curr_frq_in_khz = (fmdev->rx.region.bottom_frequency
		+ ((unsigned int)curr_frq * fmdev->rx.region.channel_spacing));

	/* Re-enable default FM interrupts */
	fmdev->irq_info.mask &= ~(FM_FR_EVENT | FM_BL_EVENT);
	FM_STORE_LE16_TO_BE16(payload, fmdev->irq_info.mask);
	ret = fmc_send_cmd(fmdev, INT_MASK_SET, &payload, sizeof(payload),
				&fmdev->maintask_completion, NULL, NULL);
	FM_CHECK_SEND_CMD_STATUS(ret);

	if (curr_frq_in_khz != freq_to_set) {
		pr_err("(fmdrv): Current chip frequency(%d) is not matching"
			   " with requested frequency(%d)", curr_frq_in_khz,
			   freq_to_set);
		ret = -EAGAIN;
		goto exit;
	}

	/* Update local cache  */
	fmdev->rx.curr_freq = curr_frq_in_khz;

	/* Reset RDS cache and current station pointers */
	fm_rx_reset_rds_cache(fmdev);
	fm_rx_reset_curr_station_info(fmdev);

	/* Do we need to reset anything else? */
exit:
	return ret;
}

int fm_rx_seek(struct fmdrv_ops *fmdev, unsigned int seek_upward,
			unsigned int wrap_around)
{
	int resp_len;
	unsigned short curr_frq, next_frq, last_frq;
	unsigned short payload, int_reason;
	char offset;
	unsigned long timeleft;
	int ret;

	if (fmdev->curr_fmmode != FM_MODE_RX) {
		ret = -EPERM;
		goto exit;
	}

	/* Read the current frequency from chip */
	ret = fmc_send_cmd(fmdev, FREQ_GET, NULL, sizeof(curr_frq),
			&fmdev->maintask_completion, &curr_frq, &resp_len);
	FM_CHECK_SEND_CMD_STATUS(ret);

	curr_frq = FM_BE16_TO_LE16(curr_frq);

	last_frq =
	(fmdev->rx.region.top_frequency - fmdev->rx.region.bottom_frequency)
	/ fmdev->rx.region.channel_spacing;

	/* Check the offset in order to be aligned to the 100KHz steps */
	offset = curr_frq % 2;

	next_frq = seek_upward ? curr_frq + 2 /* Seek Up */ :
	    curr_frq - 2 /* Seek Down */ ;

	/* Add or subtract offset (0/1) in order
	 * to stay aligned to the 100KHz steps
	 */
	if ((short)next_frq < 0)
		next_frq = last_frq - offset;
	else if (next_frq > last_frq)
		next_frq = 0 + offset;

	/* Set calculated next frequency to perform seek */
	FM_STORE_LE16_TO_BE16(payload, next_frq);
	ret = fmc_send_cmd(fmdev, FREQ_SET, &payload, sizeof(payload),
				&fmdev->maintask_completion, NULL, NULL);
	FM_CHECK_SEND_CMD_STATUS(ret);

	/* Set search direction (0:Seek Down, 1:Seek Up) */
	FM_STORE_LE16_TO_BE16(payload, (seek_upward ? FM_SEARCH_DIRECTION_UP :
					FM_SEARCH_DIRECTION_DOWN));
	ret = fmc_send_cmd(fmdev, SEARCH_DIR_SET, &payload, sizeof(payload),
				&fmdev->maintask_completion, NULL, NULL);
	FM_CHECK_SEND_CMD_STATUS(ret);

	/* Read flags - just to clear any pending interrupts if we had */
	ret = fmc_send_cmd(fmdev, FLAG_GET, NULL, 2,
				&fmdev->maintask_completion, NULL, NULL);
	FM_CHECK_SEND_CMD_STATUS(ret);

	/* Enable FR, BL interrupts */
	fmdev->irq_info.mask |= (FM_FR_EVENT | FM_BL_EVENT);
	FM_STORE_LE16_TO_BE16(payload, fmdev->irq_info.mask);
	ret = fmc_send_cmd(fmdev, INT_MASK_SET, &payload, sizeof(payload),
				&fmdev->maintask_completion, NULL, NULL);
	FM_CHECK_SEND_CMD_STATUS(ret);

	/* Start seek */
	FM_STORE_LE16_TO_BE16(payload, FM_TUNER_AUTONOMOUS_SEARCH_MODE);
	ret = fmc_send_cmd(fmdev, TUNER_MODE_SET, &payload, sizeof(payload),
				&fmdev->maintask_completion, NULL, NULL);
	FM_CHECK_SEND_CMD_STATUS(ret);

	/* Wait for tune ended/band limit reached interrupt */
	init_completion(&fmdev->maintask_completion);
	timeleft = wait_for_completion_timeout(&fmdev->maintask_completion,
					       FM_DRV_RX_SEEK_TIMEOUT);
	if (!timeleft) {
		pr_err("(fmdrv): Timeout(%d sec),didn't get tune ended int",
			   jiffies_to_msecs(FM_DRV_RX_SEEK_TIMEOUT) / 1000);
		ret = -ETIMEDOUT;
		goto exit;
	}
	int_reason = fmdev->irq_info.flag & 0x3;

	/* Re-enable default FM interrupts */
	fmdev->irq_info.mask &= ~(FM_FR_EVENT | FM_BL_EVENT);
	FM_STORE_LE16_TO_BE16(payload, fmdev->irq_info.mask);
	ret = fmc_send_cmd(fmdev, INT_MASK_SET, &payload, sizeof(payload),
				&fmdev->maintask_completion, NULL, NULL);
	FM_CHECK_SEND_CMD_STATUS(ret);

	/* Read freq to know where operation tune operation stopped */
	ret = fmc_send_cmd(fmdev, FREQ_GET, NULL, 2,
		&fmdev->maintask_completion, &curr_frq, &resp_len);
	FM_CHECK_SEND_CMD_STATUS(ret);

	curr_frq = FM_BE16_TO_LE16(curr_frq);
	fmdev->rx.curr_freq = (fmdev->rx.region.bottom_frequency +
			       ((unsigned int)curr_frq *
				fmdev->rx.region.channel_spacing));

	/* Reset RDS cache and current station pointers */
	fm_rx_reset_rds_cache(fmdev);
	fm_rx_reset_curr_station_info(fmdev);

	/* Return error if band limit is reached */
	if (int_reason & FM_BL_EVENT) {
		pr_info("(fmdrv): band limit reached\n");
		ret = -EAGAIN;
	}
exit:
	return ret;
}

int fm_rx_set_volume(struct fmdrv_ops *fmdev, unsigned short vol_to_set)
{
	unsigned short payload;
	int ret;

	if (fmdev->curr_fmmode != FM_MODE_RX)
		return -EPERM;

	if (vol_to_set > FM_RX_VOLUME_MAX) {
		pr_err("(fmdrv): Volume is not within(%d-%d) range",
			   FM_RX_VOLUME_MIN, FM_RX_VOLUME_MAX);
		return -EINVAL;
	}
	vol_to_set *= FM_RX_VOLUME_GAIN_STEP;

	FM_STORE_LE16_TO_BE16(payload, vol_to_set);
	ret = fmc_send_cmd(fmdev, VOLUME_SET, &payload, sizeof(payload),
				&fmdev->maintask_completion, NULL, NULL);
	FM_CHECK_SEND_CMD_STATUS(ret);

	fmdev->rx.curr_volume = vol_to_set;
	return ret;
}

/* Get volume */
int fm_rx_get_volume(struct fmdrv_ops *fmdev, unsigned short *curr_vol)
{
	if (fmdev->curr_fmmode != FM_MODE_RX)
		return -EPERM;

	if (curr_vol == NULL) {
		pr_err("(fmdrv): Invalid memory");
		return -ENOMEM;
	}
	*curr_vol = fmdev->rx.curr_volume / FM_RX_VOLUME_GAIN_STEP;
	return 0;
}

/* To get current band's bottom and top frequency */
int fm_rx_get_currband_lowhigh_freq(struct fmdrv_ops *fmdev,
					unsigned int *bottom_frequency,
					unsigned int *top_frequency)
{
	if (bottom_frequency != NULL)
		*bottom_frequency = fmdev->rx.region.bottom_frequency;

	if (top_frequency != NULL)
		*top_frequency = fmdev->rx.region.top_frequency;

	return 0;
}

/* Sets band (0-Europe/US; 1-Japan) */
int fm_rx_set_region(struct fmdrv_ops *fmdev,
			unsigned char region_to_set)
{
	unsigned short payload;
	unsigned int new_frq;
	int ret = -EPERM;

	if (fmdev->curr_fmmode != FM_MODE_RX)
		goto exit;

	if (region_to_set != FM_BAND_EUROPE_US &&
	    region_to_set != FM_BAND_JAPAN) {
		pr_err("(fmdrv): Invalid band\n");
		ret = -EINVAL;
		goto exit;
	}
	if (fmdev->rx.region.region_index == region_to_set) {
		pr_err("(fmdrv): Requested band is already configured\n");
		goto exit;
	}
	/* Send cmd to set the band  */
	FM_STORE_LE16_TO_BE16(payload, (unsigned short)region_to_set);
	ret = fmc_send_cmd(fmdev, RX_BAND_SET, &payload, sizeof(payload),
				&fmdev->maintask_completion, NULL, NULL);
	FM_CHECK_SEND_CMD_STATUS(ret);

	fmc_update_region_info(fmdev, region_to_set);

	/* Check whether current RX frequency is within band boundary */
	if (fmdev->curr_fmmode == FM_MODE_RX) {
		new_frq = 0;
		if (fmdev->rx.curr_freq < fmdev->rx.region.bottom_frequency)
			new_frq = fmdev->rx.region.bottom_frequency;
		else if (fmdev->rx.curr_freq > fmdev->rx.region.top_frequency)
			new_frq = fmdev->rx.region.top_frequency;

		if (new_frq) {
			pr_info("(fmdrv): "
			     "Current freq is not within band limit boundary,"
			     "switching to %d KHz", new_frq);
			/* Current RX frequency is not
			 * within boundary. So, update it
			 */
			ret = fm_rx_set_frequency(fmdev, new_frq);
		}
	}
exit:
	return ret;
}

/* Reads current mute mode (Mute Off/On/Attenuate)*/
int fm_rx_get_mute_mode(struct fmdrv_ops *fmdev,
			unsigned char *curr_mute_mode)
{
	if (fmdev->curr_fmmode != FM_MODE_RX)
		return -EPERM;

	if (curr_mute_mode == NULL) {
		pr_err("(fmdrv): Invalid memory");
		return -ENOMEM;
	}
	*curr_mute_mode = fmdev->rx.curr_mute_mode;
	return 0;
}

static int __fm_config_rx_mute_reg(struct fmdrv_ops *fmdev)
{
	unsigned short payload, muteval;
	int ret;

	muteval = 0;
	switch (fmdev->rx.curr_mute_mode) {
	case FM_MUTE_ON:
		muteval = FM_RX_MUTE_AC_MUTE_MODE;
		break;
	case FM_MUTE_OFF:
		muteval = FM_RX_MUTE_UNMUTE_MODE;
		break;
	case FM_MUTE_ATTENUATE:
		muteval = FM_RX_MUTE_SOFT_MUTE_FORCE_MODE;
		break;
	}
	if (fmdev->rx.curr_rf_depend_mute == FM_RX_RF_DEPENDENT_MUTE_ON)
		muteval |= FM_RX_MUTE_RF_DEP_MODE;
	else
		muteval &= ~FM_RX_MUTE_RF_DEP_MODE;

	FM_STORE_LE16_TO_BE16(payload, muteval);
	ret = fmc_send_cmd(fmdev, MUTE_STATUS_SET, &payload, sizeof(payload),
				&fmdev->maintask_completion, NULL, NULL);
	FM_CHECK_SEND_CMD_STATUS(ret);
	return 0;
}

/* Configures mute mode (Mute Off/On/Attenuate) */
int fm_rx_set_mute_mode(struct fmdrv_ops *fmdev,
			unsigned char mute_mode_toset)
{
	unsigned char org_state;
	int ret;

	if (fmdev->curr_fmmode != FM_MODE_RX)
		return -EPERM;

	if (fmdev->rx.curr_mute_mode == mute_mode_toset)
		return 0;

	org_state = fmdev->rx.curr_mute_mode;
	fmdev->rx.curr_mute_mode = mute_mode_toset;

	ret = __fm_config_rx_mute_reg(fmdev);
	if (ret < 0) {
		fmdev->rx.curr_mute_mode = org_state;
		return ret;
	}
	return 0;
}

/* Gets RF dependent soft mute mode enable/disable status */
int fm_rx_get_rfdepend_softmute(struct fmdrv_ops *fmdev,
				unsigned char *curr_mute_mode)
{
	if (fmdev->curr_fmmode != FM_MODE_RX)
		return -EPERM;


	if (curr_mute_mode == NULL) {
		pr_err("(fmdrv): Invalid memory");
		return -ENOMEM;
	}
	*curr_mute_mode = fmdev->rx.curr_rf_depend_mute;
	return 0;
}

/* Sets RF dependent soft mute mode */
int fm_rx_set_rfdepend_softmute(struct fmdrv_ops *fmdev,
				unsigned char rfdepend_mute)
{
	unsigned char org_state;
	int ret;

	if (fmdev->curr_fmmode != FM_MODE_RX)
		return -EPERM;


	if (rfdepend_mute != FM_RX_RF_DEPENDENT_MUTE_ON &&
	    rfdepend_mute != FM_RX_RF_DEPENDENT_MUTE_OFF) {
		pr_err("(fmdrv): Invalid RF dependent soft mute");
		return -EINVAL;
	}
	if (fmdev->rx.curr_rf_depend_mute == rfdepend_mute)
		return 0;

	org_state = fmdev->rx.curr_rf_depend_mute;
	fmdev->rx.curr_rf_depend_mute = rfdepend_mute;

	ret = __fm_config_rx_mute_reg(fmdev);
	if (ret < 0) {
		fmdev->rx.curr_rf_depend_mute = org_state;
		return ret;
	}
	return 0;
}

/* Returns the signal strength level of current channel */
int fm_rx_get_rssi_level(struct fmdrv_ops *fmdev,
				unsigned short *rssilvl)
{
	unsigned short curr_rssi_lel;
	int resp_len;
	int ret;

	if (fmdev->curr_fmmode != FM_MODE_RX)
		return -EPERM;

	if (rssilvl == NULL) {
		pr_err("(fmdrv): Invalid memory");
		return -ENOMEM;
	}
	/* Read current RSSI level */
	ret = fmc_send_cmd(fmdev, RSSI_LVL_GET, NULL, 2,
		&fmdev->maintask_completion, &curr_rssi_lel, &resp_len);
	FM_CHECK_SEND_CMD_STATUS(ret);

	*rssilvl = FM_BE16_TO_LE16(curr_rssi_lel);

	return 0;
}

/* Sets the signal strength level that once reached
 * will stop the auto search process
 */
int fm_rx_set_rssi_threshold(struct fmdrv_ops *fmdev,
				short rssi_lvl_toset)
{
	unsigned short payload;
	int ret;

	if (fmdev->curr_fmmode != FM_MODE_RX)
		return -EPERM;

	if (rssi_lvl_toset < FM_RX_RSSI_THRESHOLD_MIN ||
	    rssi_lvl_toset > FM_RX_RSSI_THRESHOLD_MAX) {
		pr_err("(fmdrv): Invalid RSSI threshold level");
		return -EINVAL;
	}
	FM_STORE_LE16_TO_BE16(payload, (unsigned short)rssi_lvl_toset);
	ret = fmc_send_cmd(fmdev, SEARCH_LVL_SET, &payload, sizeof(payload),
				&fmdev->maintask_completion, NULL, NULL);
	FM_CHECK_SEND_CMD_STATUS(ret);

	fmdev->rx.curr_rssi_threshold = rssi_lvl_toset;
	return 0;
}

/* Returns current RX RSSI threshold value */
int fm_rx_get_rssi_threshold(struct fmdrv_ops *fmdev, short *curr_rssi_lvl)
{
	if (fmdev->curr_fmmode != FM_MODE_RX)
		return -EPERM;

	if (curr_rssi_lvl == NULL) {
		pr_err("(fmdrv): Invalid memory");
		return -ENOMEM;
	}
	*curr_rssi_lvl = fmdev->rx.curr_rssi_threshold;
	return 0;
}

/* Sets RX stereo/mono modes */
int fm_rx_set_stereo_mono(struct fmdrv_ops *fmdev, unsigned short mode)
{
	unsigned short payload;
	int ret;

	if (fmdev->curr_fmmode != FM_MODE_RX)
		return -EPERM;

	if (mode != FM_STEREO_MODE && mode != FM_MONO_MODE) {
		pr_err("(fmdrv): Invalid mode");
		return -EINVAL;
	}
	/* Set stereo/mono mode */
	FM_STORE_LE16_TO_BE16(payload, (unsigned short)mode);
	ret = fmc_send_cmd(fmdev, MOST_MODE_SET, &payload, sizeof(payload),
				&fmdev->maintask_completion, NULL, NULL);
	FM_CHECK_SEND_CMD_STATUS(ret);

	/* Set stereo blending mode */
	FM_STORE_LE16_TO_BE16(payload, FM_STEREO_SOFT_BLEND);
	ret = fmc_send_cmd(fmdev, MOST_BLEND_SET, &payload, sizeof(payload),
				&fmdev->maintask_completion, NULL, NULL);
	FM_CHECK_SEND_CMD_STATUS(ret);

	return 0;
}

/* Gets current RX stereo/mono mode */
int fm_rx_get_stereo_mono(struct fmdrv_ops *fmdev, unsigned short *mode)
{
	unsigned short curr_mode;
	int ret, resp_len;

	if (fmdev->curr_fmmode != FM_MODE_RX)
		return -EPERM;

	if (mode == NULL) {
		pr_err("(fmdrv): Invalid memory");
		return -ENOMEM;
	}
	ret = fmc_send_cmd(fmdev, MOST_MODE_GET, NULL, 2,
			&fmdev->maintask_completion, &curr_mode, &resp_len);
	FM_CHECK_SEND_CMD_STATUS(ret);

	*mode = FM_BE16_TO_LE16(curr_mode);
	return 0;
}

/* Choose RX de-emphasis filter mode (50us/75us) */
int fm_rx_set_deemphasis_mode(struct fmdrv_ops *fmdev, unsigned short mode)
{
	unsigned short payload;
	int ret;

	if (fmdev->curr_fmmode != FM_MODE_RX)
		return -EPERM;

	if (mode != FM_RX_EMPHASIS_FILTER_50_USEC &&
	    mode != FM_RX_EMPHASIS_FILTER_75_USEC) {
		pr_err("(fmdrv): Invalid rx de-emphasis mode");
		return -EINVAL;
	}
	FM_STORE_LE16_TO_BE16(payload, mode);
	ret = fmc_send_cmd(fmdev, DEMPH_MODE_SET, &payload, sizeof(payload),
				&fmdev->maintask_completion, NULL, NULL);
	FM_CHECK_SEND_CMD_STATUS(ret);

	return 0;
}

/* Gets current RX de-emphasis filter mode */
int fm_rx_get_deemphasis_mode(struct fmdrv_ops *fmdev,
				unsigned short *curr_deemphasis_mode)
{
	unsigned short curr_mode;
	int ret, resp_len;

	if (fmdev->curr_fmmode != FM_MODE_RX)
		return -EPERM;

	if (curr_deemphasis_mode == NULL) {
		pr_err("(fmdrv): Invalid memory");
		return -ENOMEM;
	}
	ret = fmc_send_cmd(fmdev, DEMPH_MODE_GET, NULL, 2,
			&fmdev->maintask_completion, &curr_mode, &resp_len);
	FM_CHECK_SEND_CMD_STATUS(ret);

	*curr_deemphasis_mode = FM_BE16_TO_LE16(curr_mode);
	return 0;
}

/* Enable/Disable RX RDS */
int fm_rx_set_rds_mode(struct fmdrv_ops *fmdev, unsigned char rds_en_dis)
{
	unsigned short payload;
	int ret;

	if (fmdev->curr_fmmode != FM_MODE_RX)
		return -EPERM;

	if (rds_en_dis != FM_RDS_ENABLE && rds_en_dis != FM_RDS_DISABLE) {
		pr_err("(fmdrv): Invalid rds option");
		return -EINVAL;
	}
	if (rds_en_dis == FM_RDS_ENABLE
	    && fmdev->rx.rds.flag == FM_RDS_DISABLE) {
		/* Turn on RX RDS */
		/* Turn on RDS circuit */
		FM_STORE_LE16_TO_BE16(payload,
				      FM_RX_POWET_SET_FM_AND_RDS_BLK_ON);
		ret = fmc_send_cmd(fmdev, POWER_SET, &payload, sizeof(payload),
				&fmdev->maintask_completion, NULL, NULL);
		FM_CHECK_SEND_CMD_STATUS(ret);

		/* Clear and rest RDS FIFO */
		FM_STORE_LE16_TO_BE16(payload, FM_RX_RDS_FLUSH_FIFO);
		ret = fmc_send_cmd(fmdev, RDS_CNTRL_SET, &payload,
		sizeof(payload), &fmdev->maintask_completion, NULL, NULL);
		FM_CHECK_SEND_CMD_STATUS(ret);

		/* Read flags - just to clear any
		 * pending interrupts if we had
		 */
		ret = fmc_send_cmd(fmdev, FLAG_GET, NULL, 2,
				&fmdev->maintask_completion, NULL, NULL);
		FM_CHECK_SEND_CMD_STATUS(ret);

		/* Set RDS FIFO threshold value */
		FM_STORE_LE16_TO_BE16(payload, FM_RX_RDS_FIFO_THRESHOLD);
		ret = fmc_send_cmd(fmdev, RDS_MEM_SET, &payload,
		sizeof(payload), &fmdev->maintask_completion, NULL, NULL);
		FM_CHECK_SEND_CMD_STATUS(ret);

		/* Enable RDS interrupt */
		fmdev->irq_info.mask |= FM_RDS_EVENT;
		FM_STORE_LE16_TO_BE16(payload, fmdev->irq_info.mask);
		ret = fmc_send_cmd(fmdev, INT_MASK_SET, &payload,
		sizeof(payload), &fmdev->maintask_completion, NULL, NULL);
		FM_CHECK_SEND_CMD_STATUS(ret);

		/* Update our local flag */
		fmdev->rx.rds.flag = FM_RDS_ENABLE;
	} else if (rds_en_dis == FM_RDS_DISABLE
		   && fmdev->rx.rds.flag == FM_RDS_ENABLE) {
		/* Turn off RX RDS */
		/* Turn off RDS circuit */
		FM_STORE_LE16_TO_BE16(payload, FM_RX_POWER_SET_FM_ON_RDS_OFF);
		ret = fmc_send_cmd(fmdev, POWER_SET, &payload, sizeof(payload),
				&fmdev->maintask_completion, NULL, NULL);
		FM_CHECK_SEND_CMD_STATUS(ret);

		/* Reset RDS pointers */
		fmdev->rx.rds.last_block_index = 0;
		fmdev->rx.rds.wr_index = 0;
		fmdev->rx.rds.rd_index = 0;
		fm_rx_reset_curr_station_info(fmdev);

		/* Update RDS local cache */
		fmdev->irq_info.mask &= ~(FM_RDS_EVENT);
		fmdev->rx.rds.flag = FM_RDS_DISABLE;
	}
	return 0;
}

/* Returns current RX RDS enable/disable status */
int fm_rx_get_rds_mode(struct fmdrv_ops *fmdev,
			unsigned char *curr_rds_en_dis)
{
	if (fmdev->curr_fmmode != FM_MODE_RX)
		return -EPERM;

	if (curr_rds_en_dis == NULL) {
		pr_err("(fmdrv): Invalid memory");
		return -ENOMEM;
	}
	*curr_rds_en_dis = fmdev->rx.rds.flag;
	return 0;
}

/* Sets RDS operation mode (RDS/RDBS) */
int fm_rx_set_rds_system(struct fmdrv_ops *fmdev, unsigned char rds_mode)
{
	unsigned short payload;
	int ret;

	if (fmdev->curr_fmmode != FM_MODE_RX)
		return -EPERM;

	if (rds_mode != FM_RDS_SYSTEM_RDS && rds_mode != FM_RDS_SYSTEM_RBDS) {
		pr_err("(fmdrv): Invalid rds mode");
		return -EINVAL;
	}
	/* Set RDS operation mode */
	FM_STORE_LE16_TO_BE16(payload, (unsigned short)rds_mode);
	ret = fmc_send_cmd(fmdev, RDS_SYSTEM_SET, &payload, sizeof(payload),
				&fmdev->maintask_completion, NULL, NULL);
	FM_CHECK_SEND_CMD_STATUS(ret);

	fmdev->rx.rds_mode = rds_mode;
	return 0;
}

/* Returns current RDS operation mode */
int fm_rx_get_rds_system(struct fmdrv_ops *fmdev,
				unsigned char *rds_mode)
{
	if (fmdev->curr_fmmode != FM_MODE_RX)
		return -EPERM;

	if (rds_mode == NULL) {
		pr_err("(fmdrv): Invalid memory");
		return -ENOMEM;
	}
	*rds_mode = fmdev->rx.rds_mode;
	return 0;
}

/* Configures Alternate Frequency switch mode */
int fm_rx_set_af_switch(struct fmdrv_ops *fmdev, unsigned char af_mode)
{
	unsigned short payload;
	int ret;

	if (fmdev->curr_fmmode != FM_MODE_RX)
		return -EPERM;

	if (af_mode != FM_RX_RDS_AF_SWITCH_MODE_ON &&
	    af_mode != FM_RX_RDS_AF_SWITCH_MODE_OFF) {
		pr_err("(fmdrv): Invalid af mode");
		return -EINVAL;
	}
	/* Enable/disable low RSSI interrupt based on af_mode */
	if (af_mode == FM_RX_RDS_AF_SWITCH_MODE_ON)
		fmdev->irq_info.mask |= FM_LEV_EVENT;
	else
		fmdev->irq_info.mask &= ~FM_LEV_EVENT;

	FM_STORE_LE16_TO_BE16(payload, fmdev->irq_info.mask);
	ret = fmc_send_cmd(fmdev, INT_MASK_SET, &payload, sizeof(payload),
				&fmdev->maintask_completion, NULL, NULL);
	FM_CHECK_SEND_CMD_STATUS(ret);

	fmdev->rx.af_mode = af_mode;
	return 0;
}

/* Returns Alternate Frequency switch status */
int fm_rx_get_af_switch(struct fmdrv_ops *fmdev, unsigned char *af_mode)
{
	if (fmdev->curr_fmmode != FM_MODE_RX)
		return -EPERM;

	if (af_mode == NULL) {
		pr_err("(fmdrv): Invalid memory");
		return -ENOMEM;
	}
	*af_mode = fmdev->rx.af_mode;
	return 0;
}
