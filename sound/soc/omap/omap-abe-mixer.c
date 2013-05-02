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
 *
 */

#include <linux/export.h>
#include <linux/opp.h>

#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/tlv.h>
#include <sound/soc-fw.h>

#include "omap-abe-priv.h"

#define OMAP_ABE_HS_DC_OFFSET_STEP	(1800 / 8)
#define OMAP_ABE_HF_DC_OFFSET_STEP	(4600 / 8)

/* Gain value conversion */
#define OMAP_ABE_MAX_GAIN		12000	/* 120dB */
#define OMAP_ABE_GAIN_SCALE		100		/* 1dB */
#define abe_gain_to_val(gain) \
	((val + OMAP_ABE_MAX_GAIN) / OMAP_ABE_GAIN_SCALE)
#define abe_val_to_gain(val) \
	(-OMAP_ABE_MAX_GAIN + (val * OMAP_ABE_GAIN_SCALE))

void omap_abe_dc_set_hs_offset(struct snd_soc_platform *platform,
	int left, int right, int step_mV)
{
	struct omap_abe *abe = snd_soc_platform_get_drvdata(platform);

	if (left >= 8)
		left -= 16;
	abe->dc_offset.hsl = OMAP_ABE_HS_DC_OFFSET_STEP * left * step_mV;

	if (right >= 8)
		right -= 16;
	abe->dc_offset.hsr = OMAP_ABE_HS_DC_OFFSET_STEP * right * step_mV;
}
EXPORT_SYMBOL(omap_abe_dc_set_hs_offset);

void omap_abe_dc_set_hf_offset(struct snd_soc_platform *platform,
	int left, int right)
{
	struct omap_abe *abe = snd_soc_platform_get_drvdata(platform);

	if (left >= 8)
		left -= 16;
	abe->dc_offset.hfl = OMAP_ABE_HF_DC_OFFSET_STEP * left;

	if (right >= 8)
		right -= 16;
	abe->dc_offset.hfr = OMAP_ABE_HF_DC_OFFSET_STEP * right;
}
EXPORT_SYMBOL(omap_abe_dc_set_hf_offset);

void omap_abe_set_dl1_gains(struct snd_soc_platform *platform,
	int left, int right)
{
	struct omap_abe *abe = snd_soc_platform_get_drvdata(platform);

	omap_aess_write_gain(abe->aess, OMAP_AESS_GAIN_DL1_LEFT, left);
	omap_aess_write_gain(abe->aess, OMAP_AESS_GAIN_DL1_RIGHT, right);
}
EXPORT_SYMBOL(omap_abe_set_dl1_gains);


/* TODO: we have to use the shift value atm to represent register
 * id due to current HAL ID MACROS not being unique.
 */
static int abe_put_mixer(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget_list *wlist = snd_kcontrol_chip(kcontrol);
	struct snd_soc_dapm_widget *widget = wlist->widgets[0];
	struct snd_soc_platform *platform = widget->platform;
	struct omap_abe *abe = snd_soc_platform_get_drvdata(platform);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;

	if (ucontrol->value.integer.value[0]) {
		abe->opp.widget[mc->reg] |= ucontrol->value.integer.value[0]<<mc->shift;
		snd_soc_dapm_mixer_update_power(widget, kcontrol, 1);
		omap_aess_enable_gain(abe->aess, mc->reg);
	} else {
		abe->opp.widget[mc->reg] &= ~(0x1<<mc->shift);
		snd_soc_dapm_mixer_update_power(widget, kcontrol, 0);
		omap_aess_disable_gain(abe->aess, mc->reg);
	}

	return 1;
}

static int abe_get_mixer(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget_list *wlist = snd_kcontrol_chip(kcontrol);
	struct snd_soc_dapm_widget *widget = wlist->widgets[0];
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_platform *platform = widget->platform;
	struct omap_abe *abe = snd_soc_platform_get_drvdata(platform);

	ucontrol->value.integer.value[0] = (abe->opp.widget[mc->reg]>>mc->shift) & 0x1;

	return 0;
}

int abe_mixer_enable_mono(struct omap_abe *abe, int id, int enable)
{
	int mixer;

	switch (id) {
	case MIX_DL1_MONO:
		mixer = MIXDL1;
		break;
	case MIX_DL2_MONO:
		mixer = MIXDL2;
		break;
	case MIX_AUDUL_MONO:
		mixer = MIXAUDUL;
		break;
	default:
		return -EINVAL;
	}

	omap_aess_mono_mixer(abe->aess, mixer, enable);

	return 0;
}

static int abe_put_mono_mixer(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *platform = snd_kcontrol_chip(kcontrol);
	struct omap_abe *abe = snd_soc_platform_get_drvdata(platform);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	int id = mc->shift - MIX_DL1_MONO;

	abe->mixer.mono[id] = ucontrol->value.integer.value[0];
	abe_mixer_enable_mono(abe, mc->shift, abe->mixer.mono[id]);

	return 1;
}

static int abe_get_mono_mixer(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *platform = snd_kcontrol_chip(kcontrol);
	struct omap_abe *abe = snd_soc_platform_get_drvdata(platform);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	int id = mc->shift - MIX_DL1_MONO;

	ucontrol->value.integer.value[0] = abe->mixer.mono[id];
	return 0;
}

/* router IDs that match our mixer strings */
static const int router[] = {
		OMAP_AESS_BUFFER_ZERO_ID, /* strangely this is not 0 */
		OMAP_AESS_BUFFER_DMIC1_L_ID,
		OMAP_AESS_BUFFER_DMIC1_R_ID,
		OMAP_AESS_BUFFER_DMIC2_L_ID,
		OMAP_AESS_BUFFER_DMIC2_R_ID,
		OMAP_AESS_BUFFER_DMIC3_L_ID,
		OMAP_AESS_BUFFER_DMIC3_R_ID,
		OMAP_AESS_BUFFER_BT_UL_L_ID,
		OMAP_AESS_BUFFER_BT_UL_R_ID,
		OMAP_AESS_BUFFER_MM_EXT_IN_L_ID,
		OMAP_AESS_BUFFER_MM_EXT_IN_R_ID,
		OMAP_AESS_BUFFER_AMIC_L_ID,
		OMAP_AESS_BUFFER_AMIC_R_ID,
		OMAP_AESS_BUFFER_VX_REC_L_ID,
		OMAP_AESS_BUFFER_VX_REC_R_ID,
};

static int ul_mux_put_route(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget_list *wlist = snd_kcontrol_chip(kcontrol);
	struct snd_soc_dapm_widget *widget = wlist->widgets[0];
	struct snd_soc_platform *platform = widget->platform;
	struct omap_abe *abe = snd_soc_platform_get_drvdata(platform);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	int mux = ucontrol->value.enumerated.item[0];
	int reg = e->reg - OMAP_ABE_MUX(0);

	if (mux > OMAP_ABE_ROUTES_UL) {
		pr_err("inavlid mux %d\n", mux);
		return 0;
	}

	/* TODO: remove the gap */
	if (reg < 6) {
		/* 0  .. 5   = MM_UL */
		abe->mixer.route_ul[reg] = abe->aess->fw_info->label_id[router[mux]];
	} else if (reg < 10) {
		/* 10 .. 11  = MM_UL2 */
		/* 12 .. 13  = VX_UL */
		abe->mixer.route_ul[reg + 4] = abe->aess->fw_info->label_id[router[mux]];
	}

	omap_aess_set_router_configuration(abe->aess, (u32 *)abe->mixer.route_ul);

	if (router[mux] != OMAP_AESS_BUFFER_ZERO_ID)
		abe->opp.widget[e->reg] = e->shift_l;
	else
		abe->opp.widget[e->reg] = 0;

	snd_soc_dapm_mux_update_power(widget, kcontrol, mux, e);

	return 1;
}

static int ul_mux_get_route(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget_list *wlist = snd_kcontrol_chip(kcontrol);
	struct snd_soc_dapm_widget *widget = wlist->widgets[0];
	struct snd_soc_platform *platform = widget->platform;
	struct omap_abe *abe = snd_soc_platform_get_drvdata(platform);
	struct soc_enum *e =
		(struct soc_enum *)kcontrol->private_value;
	int reg = e->reg - OMAP_ABE_MUX(0), i, rval = 0;

	/* TODO: remove the gap */
	if (reg < 6) {
		/* 0  .. 5   = MM_UL */
		rval = abe->mixer.route_ul[reg];
	} else if (reg < 10) {
		/* 10 .. 11  = MM_UL2 */
		/* 12 .. 13  = VX_UL */
		rval = abe->mixer.route_ul[reg + 4];
	}

	for (i = 0; i < ARRAY_SIZE(router); i++) {
		if (abe->aess->fw_info->label_id[router[i]] == rval) {
			ucontrol->value.integer.value[0] = i;
			return 0;
		}
	}

	return 1;
}


static int abe_put_switch(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget_list *wlist = snd_kcontrol_chip(kcontrol);
	struct snd_soc_dapm_widget *widget = wlist->widgets[0];
	struct snd_soc_platform *platform = widget->platform;
	struct omap_abe *abe = snd_soc_platform_get_drvdata(platform);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;

	if (ucontrol->value.integer.value[0]) {
		abe->opp.widget[mc->reg] |= ucontrol->value.integer.value[0]<<mc->shift;
		snd_soc_dapm_mixer_update_power(widget, kcontrol, 1);
	} else {
		abe->opp.widget[mc->reg] &= ~(0x1<<mc->shift);
		snd_soc_dapm_mixer_update_power(widget, kcontrol, 0);
	}

	return 1;
}


static int volume_put_mixer(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *platform = snd_kcontrol_chip(kcontrol);
	struct omap_abe *abe = snd_soc_platform_get_drvdata(platform);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;

	omap_aess_write_mixer(abe->aess, mc->reg, abe_val_to_gain(ucontrol->value.integer.value[0]));

	return 1;
}

static int volume_put_gain(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *platform = snd_kcontrol_chip(kcontrol);
	struct omap_abe *abe = snd_soc_platform_get_drvdata(platform);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;

	omap_aess_write_gain(abe->aess, mc->shift,
		       abe_val_to_gain(ucontrol->value.integer.value[0]));
	omap_aess_write_gain(abe->aess, mc->rshift,
		       abe_val_to_gain(ucontrol->value.integer.value[1]));

	return 1;
}

static int volume_get_mixer(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *platform = snd_kcontrol_chip(kcontrol);
	struct omap_abe *abe = snd_soc_platform_get_drvdata(platform);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	u32 val;

	omap_aess_read_mixer(abe->aess, mc->reg, &val);
	ucontrol->value.integer.value[0] = abe_gain_to_val(val);

	return 0;
}


static int volume_get_gain(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *platform = snd_kcontrol_chip(kcontrol);
	struct omap_abe *abe = snd_soc_platform_get_drvdata(platform);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	u32 val;

	omap_aess_read_gain(abe->aess, mc->shift, &val);
	ucontrol->value.integer.value[0] = abe_gain_to_val(val);
	omap_aess_read_gain(abe->aess, mc->rshift, &val);
	ucontrol->value.integer.value[1] = abe_gain_to_val(val);

	return 0;
}

int abe_mixer_set_equ_profile(struct omap_abe *abe,
		unsigned int id, unsigned int profile)
{
	struct omap_aess_equ params;
	void *src_coeff;

	switch (id) {
	case OMAP_AESS_CMEM_DL1_COEFS_ID:
		abe->equ.dl1.profile = profile;
		params.equ_length = abe->equ.dl1.profile_size;
		src_coeff = abe->equ.dl1.coeff_data;
		break;
	case OMAP_AESS_CMEM_DL2_L_COEFS_ID:
		abe->equ.dl2l.profile = profile;
		params.equ_length = abe->equ.dl2l.profile_size;
		src_coeff = abe->equ.dl2l.coeff_data;
		break;
	case OMAP_AESS_CMEM_DL2_R_COEFS_ID:
		abe->equ.dl2r.profile = profile;
		params.equ_length = abe->equ.dl2r.profile_size;
		src_coeff = abe->equ.dl2r.coeff_data;
		break;
	case OMAP_AESS_CMEM_SDT_COEFS_ID:
		abe->equ.sdt.profile = profile;
		params.equ_length = abe->equ.sdt.profile_size;
		src_coeff = abe->equ.sdt.coeff_data;
		break;
	case OMAP_AESS_CMEM_96_48_AMIC_COEFS_ID:
		abe->equ.amic.profile = profile;
		params.equ_length = abe->equ.amic.profile_size;
		src_coeff = abe->equ.amic.coeff_data;
		break;
	case OMAP_AESS_CMEM_96_48_DMIC_COEFS_ID:
		abe->equ.dmic.profile = profile;
		params.equ_length = abe->equ.dmic.profile_size;
		src_coeff = abe->equ.dmic.coeff_data;
		break;
	default:
		return -EINVAL;
	}

	src_coeff += profile * params.equ_length;
	memcpy(params.coef.type1, src_coeff, params.equ_length);

	omap_aess_write_equalizer(abe->aess, id,
		(struct omap_aess_equ *)&params); //align types on this API

	return 0;
}

static int abe_get_equalizer(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *platform = snd_kcontrol_chip(kcontrol);
	struct omap_abe *abe = snd_soc_platform_get_drvdata(platform);
	struct soc_enum *eqc = (struct soc_enum *)kcontrol->private_value;

	switch (eqc->reg) {
	case OMAP_AESS_CMEM_DL1_COEFS_ID:
		ucontrol->value.integer.value[0] = abe->equ.dl1.profile;
		break;
	case OMAP_AESS_CMEM_DL2_L_COEFS_ID:
		ucontrol->value.integer.value[0] = abe->equ.dl2l.profile;
		break;
	case OMAP_AESS_CMEM_DL2_R_COEFS_ID:
		ucontrol->value.integer.value[0] = abe->equ.dl2r.profile;
		break;
	case OMAP_AESS_CMEM_SDT_COEFS_ID:
		ucontrol->value.integer.value[0] = abe->equ.sdt.profile;
		break;
	case OMAP_AESS_CMEM_96_48_AMIC_COEFS_ID:
		ucontrol->value.integer.value[0] = abe->equ.amic.profile;
		break;
	case OMAP_AESS_CMEM_96_48_DMIC_COEFS_ID:
		ucontrol->value.integer.value[0] = abe->equ.dmic.profile;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int abe_put_equalizer(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *platform = snd_kcontrol_chip(kcontrol);
	struct omap_abe *abe = snd_soc_platform_get_drvdata(platform);
	struct soc_enum *eqc = (struct soc_enum *)kcontrol->private_value;
	u16 val = ucontrol->value.enumerated.item[0];
	int ret;

	ret = abe_mixer_set_equ_profile(abe, eqc->reg, val);
	if (ret < 0)
		return ret;

	return 1;
}

const struct snd_soc_fw_kcontrol_ops abe_ops[] = {
	{OMAP_CONTROL_DEFAULT,	abe_get_mixer,	abe_put_mixer, NULL},
	{OMAP_CONTROL_VOLUME,	volume_get_mixer, volume_put_mixer, NULL},
	{OMAP_CONTROL_ROUTER,	ul_mux_get_route, ul_mux_put_route, NULL},
	{OMAP_CONTROL_EQU,	abe_get_equalizer, abe_put_equalizer, NULL},
	{OMAP_CONTROL_GAIN,	volume_get_gain, volume_put_gain, NULL},
	{OMAP_CONTROL_MONO,	abe_get_mono_mixer, abe_put_mono_mixer, NULL},
	{OMAP_CONTROL_SWITCH,	NULL, abe_put_switch, NULL},
};

