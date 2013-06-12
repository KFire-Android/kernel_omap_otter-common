/*
 * omap-abe.c  --  OMAP ALSA SoC DAI driver using Audio Backend
 *
 * Copyright (C) 2010 Texas Instruments
 *
 * Contact: Liam Girdwood <lrg@ti.com>
 *          Misael Lopez Cruz <misael.lopez@ti.com>
 *          Sebastien Guiriec <s-guiriec@ti.com>
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

#include <linux/export.h>
#include <linux/delay.h>
#include <linux/pm_runtime.h>

#include <sound/soc.h>

#include "omap-abe-priv.h"

int abe_mixer_enable_mono(struct omap_abe *abe, int id, int enable);
int abe_mixer_set_equ_profile(struct omap_abe *abe,
		unsigned int id, unsigned int profile);

void omap_abe_pm_get(struct snd_soc_platform *platform)
{
	struct omap_abe *abe = snd_soc_platform_get_drvdata(platform);
	pm_runtime_get_sync(abe->dev);
}
EXPORT_SYMBOL_GPL(omap_abe_pm_get);

void omap_abe_pm_put(struct snd_soc_platform *platform)
{
	struct omap_abe *abe = snd_soc_platform_get_drvdata(platform);
	pm_runtime_put_sync(abe->dev);
}
EXPORT_SYMBOL_GPL(omap_abe_pm_put);

void omap_abe_pm_shutdown(struct snd_soc_platform *platform)
{
	struct omap_abe *abe = snd_soc_platform_get_drvdata(platform);
	int ret;

	if (abe->active && omap_aess_check_activity(abe->aess))
		return;

	omap_aess_set_opp_processing(abe->aess, ABE_OPP25);
	abe->opp.level = 25;

	omap_aess_stop_event_generator(abe->aess);
	udelay(250);
	if (abe->device_scale) {
		ret = abe->device_scale(abe->dev, abe->dev, abe->opp.freqs[0]);
		if (ret)
			dev_err(abe->dev, "failed to scale to lowest OPP\n");
	}
}
EXPORT_SYMBOL_GPL(omap_abe_pm_shutdown);

void omap_abe_pm_set_mode(struct snd_soc_platform *platform, int mode)
{
	struct omap_abe *abe = snd_soc_platform_get_drvdata(platform);

	abe->dc_offset.power_mode = mode;
}
EXPORT_SYMBOL(omap_abe_pm_set_mode);

int abe_pm_save_context(struct omap_abe *abe)
{
	/* mute gains not associated with FEs/BEs */
	omap_aess_mute_gain(abe->aess, OMAP_AESS_MIXAUDUL_MM_DL);
	omap_aess_mute_gain(abe->aess, OMAP_AESS_MIXAUDUL_TONES);
	omap_aess_mute_gain(abe->aess, OMAP_AESS_MIXAUDUL_VX_DL);
	omap_aess_mute_gain(abe->aess, OMAP_AESS_MIXVXREC_TONES);
	omap_aess_mute_gain(abe->aess, OMAP_AESS_MIXVXREC_VX_DL);
	omap_aess_mute_gain(abe->aess, OMAP_AESS_MIXVXREC_MM_DL);
	omap_aess_mute_gain(abe->aess, OMAP_AESS_MIXVXREC_VX_UL);
	omap_aess_mute_gain(abe->aess, OMAP_AESS_MIXECHO_DL1);
	omap_aess_mute_gain(abe->aess, OMAP_AESS_MIXECHO_DL2);

	/*
	 * mute gains associated with DL1 BE
	 * ideally, these gains should be muted/saved when BE is muted, but
	 * when ABE McPDM is started for DL1 or DL2, PDM_DL1 port gets enabled
	 * which prevents to mute these gains since two ports on DL1 path are
	 * active when mute is called for BT_VX_DL or MM_EXT_DL.
	 *
	 * These gains are not restored along with the context because they
	 * are properly unmuted/restored when any of the DL1 BEs is unmuted
	 */
	omap_aess_mute_gain(abe->aess, OMAP_AESS_GAIN_DL1_LEFT);
	omap_aess_mute_gain(abe->aess, OMAP_AESS_GAIN_DL1_RIGHT);
	omap_aess_mute_gain(abe->aess, OMAP_AESS_MIXSDT_DL);

	return 0;
}

int abe_pm_restore_context(struct omap_abe *abe)
{
	int i, ret;

	if (abe->device_scale) {
		ret = abe->device_scale(abe->dev, abe->dev,
				abe->opp.freqs[OMAP_ABE_OPP_50]);
		if (ret) {
			dev_err(abe->dev, "failed to scale to OPP 50\n");
			return ret;
		}
	}

	/* unmute gains not associated with FEs/BEs */
	omap_aess_unmute_gain(abe->aess, OMAP_AESS_MIXAUDUL_MM_DL);
	omap_aess_unmute_gain(abe->aess, OMAP_AESS_MIXAUDUL_TONES);
	omap_aess_unmute_gain(abe->aess, OMAP_AESS_MIXAUDUL_VX_DL);
	omap_aess_unmute_gain(abe->aess, OMAP_AESS_MIXVXREC_TONES);
	omap_aess_unmute_gain(abe->aess, OMAP_AESS_MIXVXREC_VX_DL);
	omap_aess_unmute_gain(abe->aess, OMAP_AESS_MIXVXREC_MM_DL);
	omap_aess_unmute_gain(abe->aess, OMAP_AESS_MIXVXREC_VX_UL);
	omap_aess_unmute_gain(abe->aess, OMAP_AESS_MIXECHO_DL1);
	omap_aess_unmute_gain(abe->aess, OMAP_AESS_MIXECHO_DL2);
	omap_aess_set_router_configuration(abe->aess, (u32 *)abe->mixer.route_ul);

	/* DC offset cancellation setting */
	if (abe->dc_offset.power_mode)
		omap_aess_write_pdmdl_offset(abe->aess, 1, abe->dc_offset.hsl * 2, abe->dc_offset.hsr * 2);
	else
		omap_aess_write_pdmdl_offset(abe->aess, 1, abe->dc_offset.hsl, abe->dc_offset.hsr);

	omap_aess_write_pdmdl_offset(abe->aess, 2, abe->dc_offset.hfl, abe->dc_offset.hfr);

	abe_mixer_set_equ_profile(abe, OMAP_AESS_CMEM_DL1_COEFS_ID, abe->equ.dl1.profile);
	abe_mixer_set_equ_profile(abe, OMAP_AESS_CMEM_DL2_L_COEFS_ID, abe->equ.dl2l.profile);
	abe_mixer_set_equ_profile(abe, OMAP_AESS_CMEM_DL2_R_COEFS_ID, abe->equ.dl2r.profile);
	abe_mixer_set_equ_profile(abe, OMAP_AESS_CMEM_SDT_COEFS_ID, abe->equ.sdt.profile);
	abe_mixer_set_equ_profile(abe, OMAP_AESS_CMEM_96_48_AMIC_COEFS_ID, abe->equ.amic.profile);
	abe_mixer_set_equ_profile(abe, OMAP_AESS_CMEM_96_48_DMIC_COEFS_ID, abe->equ.dmic.profile);

	for (i = 0; i < OMAP_ABE_NUM_MONO_MIXERS; i++)
		abe_mixer_enable_mono(abe, MIX_DL1_MONO + i, abe->mixer.mono[i]);

       return 0;
}

#ifdef CONFIG_PM
int abe_pm_suspend(struct snd_soc_dai *dai)
{
	struct omap_abe *abe = snd_soc_dai_get_drvdata(dai);
	int ret = 0;

	dev_dbg(dai->dev, "%s: %s active %d\n",
		__func__, dai->name, dai->active);

	if (!dai->active)
		return 0;

	pm_runtime_get_sync(abe->dev);

	switch (dai->id) {
	case OMAP_ABE_DAI_PDM_UL:
		omap_aess_mute_gain(abe->aess, OMAP_AESS_GAIN_AMIC_LEFT);
		omap_aess_mute_gain(abe->aess, OMAP_AESS_GAIN_AMIC_RIGHT);
		break;
	case OMAP_ABE_DAI_PDM_DL1:
	case OMAP_ABE_DAI_PDM_DL2:
		break;
	case OMAP_ABE_DAI_BT_VX:
		omap_aess_mute_gain(abe->aess, OMAP_AESS_GAIN_BTUL_LEFT);
		omap_aess_mute_gain(abe->aess, OMAP_AESS_GAIN_BTUL_RIGHT);
		break;
	case OMAP_ABE_DAI_MM_FM:
	case OMAP_ABE_DAI_MODEM:
		break;
	case OMAP_ABE_DAI_DMIC0:
		omap_aess_mute_gain(abe->aess, OMAP_AESS_GAIN_DMIC1_LEFT);
		omap_aess_mute_gain(abe->aess, OMAP_AESS_GAIN_DMIC1_RIGHT);
		break;
	case OMAP_ABE_DAI_DMIC1:
		omap_aess_mute_gain(abe->aess, OMAP_AESS_GAIN_DMIC2_LEFT);
		omap_aess_mute_gain(abe->aess, OMAP_AESS_GAIN_DMIC2_RIGHT);
		break;
	case OMAP_ABE_DAI_DMIC2:
		omap_aess_mute_gain(abe->aess, OMAP_AESS_GAIN_DMIC3_LEFT);
		omap_aess_mute_gain(abe->aess, OMAP_AESS_GAIN_DMIC3_RIGHT);
		break;
	default:
		dev_err(dai->dev, "%s: invalid DAI id %d\n",
				__func__, dai->id);
		break;
	}

	pm_runtime_put_sync(abe->dev);
	return ret;
}

int abe_pm_resume(struct snd_soc_dai *dai)
{
	struct omap_abe *abe = snd_soc_dai_get_drvdata(dai);
	int i, ret = 0;

	dev_dbg(dai->dev, "%s: %s active %d\n",
		__func__, dai->name, dai->active);

	if (!dai->active)
		return 0;

	/* context retained, no need to restore */
	if (abe->get_context_lost_count && abe->get_context_lost_count(abe->dev) == abe->context_lost)
		return 0;
	abe->context_lost = abe->get_context_lost_count(abe->dev);
	pm_runtime_get_sync(abe->dev);
	if (abe->device_scale) {
		ret = abe->device_scale(abe->dev, abe->dev,
				abe->opp.freqs[OMAP_ABE_OPP_50]);
		if (ret) {
			dev_err(abe->dev, "failed to scale to OPP 50\n");
			goto out;
		}
	}

	omap_aess_reload_fw(abe->aess, abe->fw_data);

	switch (dai->id) {
	case OMAP_ABE_DAI_PDM_UL:
		omap_aess_unmute_gain(abe->aess, OMAP_AESS_GAIN_AMIC_LEFT);
		omap_aess_unmute_gain(abe->aess, OMAP_AESS_GAIN_AMIC_RIGHT);
		break;
	case OMAP_ABE_DAI_PDM_DL1:
	case OMAP_ABE_DAI_PDM_DL2:
		break;
	case OMAP_ABE_DAI_BT_VX:
		omap_aess_unmute_gain(abe->aess, OMAP_AESS_GAIN_BTUL_LEFT);
		omap_aess_unmute_gain(abe->aess, OMAP_AESS_GAIN_BTUL_RIGHT);
		break;
	case OMAP_ABE_DAI_MM_FM:
	case OMAP_ABE_DAI_MODEM:
		break;
	case OMAP_ABE_DAI_DMIC0:
		omap_aess_unmute_gain(abe->aess, OMAP_AESS_GAIN_DMIC1_LEFT);
		omap_aess_unmute_gain(abe->aess, OMAP_AESS_GAIN_DMIC1_RIGHT);
		break;
	case OMAP_ABE_DAI_DMIC1:
		omap_aess_unmute_gain(abe->aess, OMAP_AESS_GAIN_DMIC2_LEFT);
		omap_aess_unmute_gain(abe->aess, OMAP_AESS_GAIN_DMIC2_RIGHT);
		break;
	case OMAP_ABE_DAI_DMIC2:
		omap_aess_unmute_gain(abe->aess, OMAP_AESS_GAIN_DMIC3_LEFT);
		omap_aess_unmute_gain(abe->aess, OMAP_AESS_GAIN_DMIC3_RIGHT);
		break;
	default:
		dev_err(dai->dev, "%s: invalid DAI id %d\n",
				__func__, dai->id);
		ret = -EINVAL;
		goto out;
	}

	omap_aess_set_router_configuration(abe->aess, (u32 *)abe->mixer.route_ul);

	if (abe->dc_offset.power_mode)
		omap_aess_write_pdmdl_offset(abe->aess, 1, abe->dc_offset.hsl * 2, abe->dc_offset.hsr * 2);
	else
		omap_aess_write_pdmdl_offset(abe->aess, 1, abe->dc_offset.hsl, abe->dc_offset.hsr);

	omap_aess_write_pdmdl_offset(abe->aess, 2, abe->dc_offset.hfl, abe->dc_offset.hfr);

	abe_mixer_set_equ_profile(abe, OMAP_AESS_CMEM_DL1_COEFS_ID, abe->equ.dl1.profile);
	abe_mixer_set_equ_profile(abe, OMAP_AESS_CMEM_DL2_L_COEFS_ID, abe->equ.dl2l.profile);
	abe_mixer_set_equ_profile(abe, OMAP_AESS_CMEM_DL2_R_COEFS_ID, abe->equ.dl2r.profile);
	abe_mixer_set_equ_profile(abe, OMAP_AESS_CMEM_SDT_COEFS_ID, abe->equ.sdt.profile);
	abe_mixer_set_equ_profile(abe, OMAP_AESS_CMEM_96_48_AMIC_COEFS_ID, abe->equ.amic.profile);
	abe_mixer_set_equ_profile(abe, OMAP_AESS_CMEM_96_48_DMIC_COEFS_ID, abe->equ.dmic.profile);

	for (i = 0; i < OMAP_ABE_NUM_MONO_MIXERS; i++)
		abe_mixer_enable_mono(abe, MIX_DL1_MONO + i, abe->mixer.mono[i]);
out:
	pm_runtime_put_sync(abe->dev);
	return ret;
}
#else
#define abe_suspend	NULL
#define abe_resume	NULL
#endif
