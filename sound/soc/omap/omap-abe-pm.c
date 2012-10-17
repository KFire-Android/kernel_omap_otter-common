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
/* #include "abe/abe_main.h" */
#include "abe/abe_aess.h"

void omap_abe_pm_get(struct snd_soc_platform *platform)
{
	struct omap_abe *abe = snd_soc_platform_get_drvdata(platform);
	omap_abe_pm_runtime_get_sync(abe);
}
EXPORT_SYMBOL_GPL(omap_abe_pm_get);

void omap_abe_pm_put(struct snd_soc_platform *platform)
{
	struct omap_abe *abe = snd_soc_platform_get_drvdata(platform);
	omap_abe_pm_runtime_put_sync(abe);
}
EXPORT_SYMBOL_GPL(omap_abe_pm_put);

int omap_abe_pm_runtime_get_sync(struct omap_abe *abe)
{
	int ret;
	ret = pm_runtime_get_sync(abe->dev);
	if (!ret)
		omap_aess_set_auto_gating(abe->aess);
	return ret;
}
EXPORT_SYMBOL_GPL(omap_abe_pm_runtime_get_sync);

int omap_abe_pm_runtime_put_sync(struct omap_abe *abe)
{
	return pm_runtime_put_sync(abe->dev);
}
EXPORT_SYMBOL_GPL(omap_abe_pm_runtime_put_sync);

int omap_abe_pm_runtime_put_sync_suspend(struct omap_abe *abe)
{
	return pm_runtime_put_sync_suspend(abe->dev);
}
EXPORT_SYMBOL_GPL(omap_abe_pm_runtime_put_sync_suspend);

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
		ret = abe->device_scale(abe->dev, abe->opp.freqs[0]);
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
	int context_loss = 0;
	int i, ret;

	if (abe->device_scale) {
		ret = abe->device_scale(abe->dev,
				abe->opp.freqs[OMAP_ABE_OPP_50]);
		if (ret) {
			dev_err(abe->dev, "failed to scale to OPP 50\n");
			return ret;
		}
	}

	if (abe->get_context_loss_count) {
		context_loss = abe->get_context_loss_count(abe->dev);
		if (context_loss < 0)
			return context_loss;
	}

	dev_dbg(abe->dev, "%s: context is %slost\n", __func__,
		(context_loss != abe->context_loss) ? "" : "not ");

	if (context_loss != abe->context_loss) {
		ret = omap_aess_reload_fw(abe->aess, abe->firmware);
		if (ret) {
			dev_dbg(abe->dev, "failed to reload firmware\n");
			return ret;
		}
	}

	abe->context_loss = context_loss;

#if defined(CONFIG_SND_OMAP_SOC_ABE_DL2)
	omap_aess_select_pdm_output(abe->aess, OMAP_ABE_DL1_HS_DL2_HF);
#else
	omap_aess_select_pdm_output(abe->aess, OMAP_ABE_DL1_HS_HF);
#endif

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
	omap_aess_set_router_configuration(abe->aess, UPROUTE, 0, (u32 *)abe->mixer.route_ul);

	/* DC offset cancellation setting */
	if (abe->dc_offset.power_mode)
		omap_aess_write_pdmdl_offset(abe->aess, 1, abe->dc_offset.hsl * 2, abe->dc_offset.hsr * 2);
	else
		omap_aess_write_pdmdl_offset(abe->aess, 1, abe->dc_offset.hsl, abe->dc_offset.hsr);

	omap_aess_write_pdmdl_offset(abe->aess, 2, abe->dc_offset.hfl, abe->dc_offset.hfr);

	for (i = 0; i < abe->hdr.num_equ; i++)
		abe_mixer_set_equ_profile(abe, i, abe->equ.profile[i]);

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

	omap_abe_pm_runtime_get_sync(abe);

	switch (dai->id) {
	case OMAP_ABE_DAI_PDM_UL:
		omap_aess_mute_gain(abe->aess, OMAP_AESS_GAIN_AMIC_LEFT);
		omap_aess_mute_gain(abe->aess, OMAP_AESS_GAIN_AMIC_RIGHT);
		break;
	case OMAP_ABE_DAI_PDM_DL1:
	case OMAP_ABE_DAI_PDM_DL2:
	case OMAP_ABE_DAI_PDM_VIB:
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

	omap_abe_pm_runtime_put_sync(abe);
	return ret;
}

int abe_pm_resume(struct snd_soc_dai *dai)
{
	struct omap_abe *abe = snd_soc_dai_get_drvdata(dai);
	int context_loss = 0;
	int i, ret = 0;

	dev_dbg(dai->dev, "%s: %s active %d\n",
		__func__, dai->name, dai->active);

	if (!dai->active)
		return 0;

	if (abe->get_context_loss_count) {
		context_loss = abe->get_context_loss_count(abe->dev);
		if (context_loss < 0)
			return context_loss;
	}

	dev_dbg(abe->dev, "%s: context is %slost\n", __func__,
		(context_loss != abe->context_loss) ? "" : "not ");

	/* context retained, no need to restore */
	if (context_loss == abe->context_loss)
		return 0;

	abe->context_loss = context_loss;

	omap_abe_pm_runtime_get_sync(abe);

	if (abe->device_scale) {
		ret = abe->device_scale(abe->dev,
				abe->opp.freqs[OMAP_ABE_OPP_50]);
		if (ret) {
			dev_err(abe->dev, "failed to scale to OPP 50\n");
			goto out;
		}
	}

	ret = omap_aess_reload_fw(abe->aess, abe->firmware);
	if (ret) {
		dev_dbg(abe->dev, "failed to reload firmware\n");
		goto out;
	}

#if defined(CONFIG_SND_OMAP_SOC_ABE_DL2)
	omap_aess_select_pdm_output(abe->aess, OMAP_ABE_DL1_HS_DL2_HF);
#else
	omap_aess_select_pdm_output(abe->aess, OMAP_ABE_DL1_HS_HF);
#endif

	switch (dai->id) {
	case OMAP_ABE_DAI_PDM_UL:
		omap_aess_unmute_gain(abe->aess, OMAP_AESS_GAIN_AMIC_LEFT);
		omap_aess_unmute_gain(abe->aess, OMAP_AESS_GAIN_AMIC_RIGHT);
		break;
	case OMAP_ABE_DAI_PDM_DL1:
	case OMAP_ABE_DAI_PDM_DL2:
	case OMAP_ABE_DAI_PDM_VIB:
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

	omap_aess_set_router_configuration(abe->aess, UPROUTE, 0, (u32 *)abe->mixer.route_ul);

	if (abe->dc_offset.power_mode)
		omap_aess_write_pdmdl_offset(abe->aess, 1, abe->dc_offset.hsl * 2, abe->dc_offset.hsr * 2);
	else
		omap_aess_write_pdmdl_offset(abe->aess, 1, abe->dc_offset.hsl, abe->dc_offset.hsr);

	omap_aess_write_pdmdl_offset(abe->aess, 2, abe->dc_offset.hfl, abe->dc_offset.hfr);

	for (i = 0; i < abe->hdr.num_equ; i++)
		abe_mixer_set_equ_profile(abe, i, abe->equ.profile[i]);

	for (i = 0; i < OMAP_ABE_NUM_MONO_MIXERS; i++)
		abe_mixer_enable_mono(abe, MIX_DL1_MONO + i, abe->mixer.mono[i]);
out:
	omap_abe_pm_runtime_put_sync(abe);
	return ret;
}
#endif
