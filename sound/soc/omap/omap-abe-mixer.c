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
#include <linux/pm_runtime.h>
#include <linux/opp.h>

#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/tlv.h>

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

/* TODO: map IO directly into ABE memories */
unsigned int abe_mixer_read(struct snd_soc_platform *platform,
		unsigned int reg)
{
	struct omap_abe *abe = snd_soc_platform_get_drvdata(platform);

	BUG_ON(reg > OMAP_ABE_NUM_DAPM_REG);
	dev_dbg(platform->dev, "read R%d (Ox%x) = 0x%x\n",
			reg, reg, abe->opp.widget[reg]);
	return abe->opp.widget[reg];
}

int abe_mixer_write(struct snd_soc_platform *platform, unsigned int reg,
		unsigned int val)
{
	struct omap_abe *abe = snd_soc_platform_get_drvdata(platform);

	BUG_ON(reg > OMAP_ABE_NUM_DAPM_REG);
	abe->opp.widget[reg] = val;
	dev_dbg(platform->dev, "write R%d (Ox%x) = 0x%x\n", reg, reg, val);
	return 0;
}

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

/* Media DL1 volume control from -120 to 30 dB in 1 dB steps */
static DECLARE_TLV_DB_SCALE(mm_dl1_tlv, -12000, 100, 3000);

/* Media DL1 volume control from -120 to 30 dB in 1 dB steps */
static DECLARE_TLV_DB_SCALE(tones_dl1_tlv, -12000, 100, 3000);

/* Media DL1 volume control from -120 to 30 dB in 1 dB steps */
static DECLARE_TLV_DB_SCALE(voice_dl1_tlv, -12000, 100, 3000);

/* Media DL1 volume control from -120 to 30 dB in 1 dB steps */
static DECLARE_TLV_DB_SCALE(capture_dl1_tlv, -12000, 100, 3000);

/* Media DL2 volume control from -120 to 30 dB in 1 dB steps */
static DECLARE_TLV_DB_SCALE(mm_dl2_tlv, -12000, 100, 3000);

/* Media DL2 volume control from -120 to 30 dB in 1 dB steps */
static DECLARE_TLV_DB_SCALE(tones_dl2_tlv, -12000, 100, 3000);

/* Media DL2 volume control from -120 to 30 dB in 1 dB steps */
static DECLARE_TLV_DB_SCALE(voice_dl2_tlv, -12000, 100, 3000);

/* Media DL2 volume control from -120 to 30 dB in 1 dB steps */
static DECLARE_TLV_DB_SCALE(capture_dl2_tlv, -12000, 100, 3000);

/* SDT volume control from -120 to 30 dB in 1 dB steps */
static DECLARE_TLV_DB_SCALE(sdt_ul_tlv, -12000, 100, 3000);

/* SDT volume control from -120 to 30 dB in 1 dB steps */
static DECLARE_TLV_DB_SCALE(sdt_dl_tlv, -12000, 100, 3000);

/* AUDUL volume control from -120 to 30 dB in 1 dB steps */
static DECLARE_TLV_DB_SCALE(audul_mm_tlv, -12000, 100, 3000);

/* AUDUL volume control from -120 to 30 dB in 1 dB steps */
static DECLARE_TLV_DB_SCALE(audul_tones_tlv, -12000, 100, 3000);

/* AUDUL volume control from -120 to 30 dB in 1 dB steps */
static DECLARE_TLV_DB_SCALE(audul_vx_ul_tlv, -12000, 100, 3000);

/* AUDUL volume control from -120 to 30 dB in 1 dB steps */
static DECLARE_TLV_DB_SCALE(audul_vx_dl_tlv, -12000, 100, 3000);

/* VXREC volume control from -120 to 30 dB in 1 dB steps */
static DECLARE_TLV_DB_SCALE(vxrec_mm_dl_tlv, -12000, 100, 3000);

/* VXREC volume control from -120 to 30 dB in 1 dB steps */
static DECLARE_TLV_DB_SCALE(vxrec_tones_tlv, -12000, 100, 3000);

/* VXREC volume control from -120 to 30 dB in 1 dB steps */
static DECLARE_TLV_DB_SCALE(vxrec_vx_dl_tlv, -12000, 100, 3000);

/* VXREC volume control from -120 to 30 dB in 1 dB steps */
static DECLARE_TLV_DB_SCALE(vxrec_vx_ul_tlv, -12000, 100, 3000);

/* DMIC volume control from -120 to 30 dB in 1 dB steps */
static DECLARE_TLV_DB_SCALE(dmic_tlv, -12000, 100, 3000);

/* BT UL volume control from -120 to 30 dB in 1 dB steps */
static DECLARE_TLV_DB_SCALE(btul_tlv, -12000, 100, 3000);

/* AMIC volume control from -120 to 30 dB in 1 dB steps */
static DECLARE_TLV_DB_SCALE(amic_tlv, -12000, 100, 3000);

/* TODO: we have to use the shift value atm to represent register
 * id due to current HAL ID MACROS not being unique.
 */
static int put_mixer(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget_list *wlist = snd_kcontrol_chip(kcontrol);
	struct snd_soc_dapm_widget *widget = wlist->widgets[0];
	struct snd_soc_platform *platform = widget->platform;
	struct omap_abe *abe = snd_soc_platform_get_drvdata(platform);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;

	pm_runtime_get_sync(abe->dev);

	if (ucontrol->value.integer.value[0]) {
		abe->opp.widget[mc->reg] |= ucontrol->value.integer.value[0]<<mc->shift;
		snd_soc_dapm_mixer_update_power(widget, kcontrol, 1);
		omap_aess_enable_gain(abe->aess, mc->reg);
	} else {
		abe->opp.widget[mc->reg] &= ~(0x1<<mc->shift);
		snd_soc_dapm_mixer_update_power(widget, kcontrol, 0);
		omap_aess_disable_gain(abe->aess, mc->reg);
	}

	pm_runtime_put_sync(abe->dev);
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

	pm_runtime_get_sync(abe->dev);
	omap_aess_mono_mixer(abe->aess, mixer, enable);
	pm_runtime_put_sync(abe->dev);

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
static const abe_router_t router[] = {
		ZERO_labelID, /* strangely this is not 0 */
		DMIC1_L_labelID, DMIC1_R_labelID,
		DMIC2_L_labelID, DMIC2_R_labelID,
		DMIC3_L_labelID, DMIC3_R_labelID,
		BT_UL_L_labelID, BT_UL_R_labelID,
		MM_EXT_IN_L_labelID, MM_EXT_IN_R_labelID,
		AMIC_L_labelID, AMIC_R_labelID,
		VX_REC_L_labelID, VX_REC_R_labelID,
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

	pm_runtime_get_sync(abe->dev);

	/* TODO: remove the gap */
	if (reg < 8) {
		/* 0  .. 9   = MM_UL */
		abe->mixer.route_ul[reg] = router[mux];
	} else if (reg < 12) {
		/* 10 .. 11  = MM_UL2 */
		/* 12 .. 13  = VX_UL */
		abe->mixer.route_ul[reg + 2] = router[mux];
	}

	/* 2nd arg here is unused */
	omap_aess_set_router_configuration(abe->aess, UPROUTE, 0, (u32 *)abe->mixer.route_ul);

	if (router[mux] != ZERO_labelID)
		abe->opp.widget[e->reg] = e->shift_l;
	else
		abe->opp.widget[e->reg] = 0;

	snd_soc_dapm_mux_update_power(widget, kcontrol, mux, e);
	pm_runtime_put_sync(abe->dev);

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
	if (reg < 8) {
		/* 0  .. 9   = MM_UL */
		rval = abe->mixer.route_ul[reg];
	} else if (reg < 12) {
		/* 10 .. 11  = MM_UL2 */
		/* 12 .. 13  = VX_UL */
		rval = abe->mixer.route_ul[reg + 2];
	}

	for (i = 0; i < ARRAY_SIZE(router); i++) {
		if (router[i] == rval) {
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

	pm_runtime_get_sync(abe->dev);

	if (ucontrol->value.integer.value[0]) {
		abe->opp.widget[mc->reg] |= ucontrol->value.integer.value[0]<<mc->shift;
		snd_soc_dapm_mixer_update_power(widget, kcontrol, 1);
	} else {
		abe->opp.widget[mc->reg] &= ~(0x1<<mc->shift);
		snd_soc_dapm_mixer_update_power(widget, kcontrol, 0);
	}
	pm_runtime_put_sync(abe->dev);

	return 1;
}


static int volume_put_mixer(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *platform = snd_kcontrol_chip(kcontrol);
	struct omap_abe *abe = snd_soc_platform_get_drvdata(platform);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;

	pm_runtime_get_sync(abe->dev);

	omap_aess_write_mixer(abe->aess, mc->reg, abe_val_to_gain(ucontrol->value.integer.value[0]));

	pm_runtime_put_sync(abe->dev);

	return 1;
}

static int volume_put_gain(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *platform = snd_kcontrol_chip(kcontrol);
	struct omap_abe *abe = snd_soc_platform_get_drvdata(platform);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;

	pm_runtime_get_sync(abe->dev);
	/*SEBG: Ramp 2ms */
	omap_aess_write_gain(abe->aess, mc->shift,
		       abe_val_to_gain(ucontrol->value.integer.value[0]));
	omap_aess_write_gain(abe->aess, mc->rshift,
		       abe_val_to_gain(ucontrol->value.integer.value[1]));
	pm_runtime_put_sync(abe->dev);

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

	pm_runtime_get_sync(abe->dev);
	omap_aess_read_mixer(abe->aess, mc->reg, &val);
	ucontrol->value.integer.value[0] = abe_gain_to_val(val);
	pm_runtime_put_sync(abe->dev);

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

	pm_runtime_get_sync(abe->dev);
	omap_aess_read_gain(abe->aess, mc->shift, &val);
	ucontrol->value.integer.value[0] = abe_gain_to_val(val);
	omap_aess_read_gain(abe->aess, mc->rshift, &val);
	ucontrol->value.integer.value[1] = abe_gain_to_val(val);
	pm_runtime_put_sync(abe->dev);

	return 0;
}

int abe_mixer_set_equ_profile(struct omap_abe *abe,
		unsigned int id, unsigned int profile)
{
	struct omap_aess_equ equ_params;
	int len;

	if (id >= abe->hdr.num_equ)
		return -EINVAL;

	if (profile >= abe->equ.texts[id].count)
		return -EINVAL;

	len = abe->equ.texts[id].coeff;
	equ_params.equ_length = len;
	memcpy(equ_params.coef.type1, abe->equ.equ[id] + profile * len,
		len * sizeof(u32));
	abe->equ.profile[id] = profile;

	pm_runtime_get_sync(abe->dev);
	omap_aess_write_equalizer(abe->aess, id + 1, (struct omap_aess_equ *)&equ_params);
	pm_runtime_put_sync(abe->dev);

	return 0;
}

static int abe_get_equalizer(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *platform = snd_kcontrol_chip(kcontrol);
	struct omap_abe *abe = snd_soc_platform_get_drvdata(platform);
	struct soc_enum *eqc = (struct soc_enum *)kcontrol->private_value;

	ucontrol->value.integer.value[0] = abe->equ.profile[eqc->reg];
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

int snd_soc_info_enum_ext1(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_info *uinfo)
{
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;

	uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
	uinfo->count = 1;
	uinfo->value.enumerated.items = e->max;

	if (uinfo->value.enumerated.item > e->max - 1)
		uinfo->value.enumerated.item = e->max - 1;
	strcpy(uinfo->value.enumerated.name,
		snd_soc_get_enum_text(e, uinfo->value.enumerated.item));

	return 0;
}

static const char *route_ul_texts[] = {
	"None", "DMic0L", "DMic0R", "DMic1L", "DMic1R", "DMic2L", "DMic2R",
	"BT Left", "BT Right", "MMExt Left", "MMExt Right", "AMic0", "AMic1",
	"VX Left", "VX Right"
};

/* ROUTE_UL Mux table */
static const struct soc_enum abe_enum[] = {
		SOC_ENUM_SINGLE(MUX_MM_UL10, 0, 15, route_ul_texts),
		SOC_ENUM_SINGLE(MUX_MM_UL11, 0, 15, route_ul_texts),
		SOC_ENUM_SINGLE(MUX_MM_UL12, 0, 15, route_ul_texts),
		SOC_ENUM_SINGLE(MUX_MM_UL13, 0, 15, route_ul_texts),
		SOC_ENUM_SINGLE(MUX_MM_UL14, 0, 15, route_ul_texts),
		SOC_ENUM_SINGLE(MUX_MM_UL15, 0, 15, route_ul_texts),
		SOC_ENUM_SINGLE(MUX_MM_UL16, 0, 15, route_ul_texts),
		SOC_ENUM_SINGLE(MUX_MM_UL17, 0, 15, route_ul_texts),
		SOC_ENUM_SINGLE(MUX_MM_UL20, 0, 15, route_ul_texts),
		SOC_ENUM_SINGLE(MUX_MM_UL21, 0, 15, route_ul_texts),
		SOC_ENUM_SINGLE(MUX_VX_UL0, 0, 15, route_ul_texts),
		SOC_ENUM_SINGLE(MUX_VX_UL1, 0, 15, route_ul_texts),
};

static const struct snd_kcontrol_new mm_ul00_control =
	SOC_DAPM_ENUM_EXT("Route", abe_enum[0],
	ul_mux_get_route, ul_mux_put_route);

static const struct snd_kcontrol_new mm_ul01_control =
	SOC_DAPM_ENUM_EXT("Route", abe_enum[1],
	ul_mux_get_route, ul_mux_put_route);

static const struct snd_kcontrol_new mm_ul02_control =
	SOC_DAPM_ENUM_EXT("Route", abe_enum[2],
	ul_mux_get_route, ul_mux_put_route);

static const struct snd_kcontrol_new mm_ul03_control =
	SOC_DAPM_ENUM_EXT("Route", abe_enum[3],
	ul_mux_get_route, ul_mux_put_route);

static const struct snd_kcontrol_new mm_ul04_control =
	SOC_DAPM_ENUM_EXT("Route", abe_enum[4],
	ul_mux_get_route, ul_mux_put_route);

static const struct snd_kcontrol_new mm_ul05_control =
	SOC_DAPM_ENUM_EXT("Route", abe_enum[5],
	ul_mux_get_route, ul_mux_put_route);

static const struct snd_kcontrol_new mm_ul06_control =
	SOC_DAPM_ENUM_EXT("Route", abe_enum[6],
	ul_mux_get_route, ul_mux_put_route);

static const struct snd_kcontrol_new mm_ul07_control =
	SOC_DAPM_ENUM_EXT("Route", abe_enum[7],
	ul_mux_get_route, ul_mux_put_route);

static const struct snd_kcontrol_new mm_ul10_control =
	SOC_DAPM_ENUM_EXT("Route", abe_enum[8],
	ul_mux_get_route, ul_mux_put_route);

static const struct snd_kcontrol_new mm_ul11_control =
	SOC_DAPM_ENUM_EXT("Route", abe_enum[9],
	ul_mux_get_route, ul_mux_put_route);

static const struct snd_kcontrol_new mm_vx0_control =
	SOC_DAPM_ENUM_EXT("Route", abe_enum[10],
	ul_mux_get_route, ul_mux_put_route);

static const struct snd_kcontrol_new mm_vx1_control =
	SOC_DAPM_ENUM_EXT("Route", abe_enum[11],
	ul_mux_get_route, ul_mux_put_route);

/* DL1 mixer paths */
static const struct snd_kcontrol_new dl1_mixer_controls[] = {
	SOC_SINGLE_EXT("Tones", OMAP_AESS_MIXDL1_TONES, 0, 1, 0,
		abe_get_mixer, put_mixer),
	SOC_SINGLE_EXT("Voice", OMAP_AESS_MIXDL1_VX_DL, 0, 1, 0,
		abe_get_mixer, put_mixer),
	SOC_SINGLE_EXT("Capture", OMAP_AESS_MIXDL1_MM_UL2, 0, 1, 0,
		abe_get_mixer, put_mixer),
	SOC_SINGLE_EXT("Multimedia", OMAP_AESS_MIXDL1_MM_DL, 0, 1, 0,
		abe_get_mixer, put_mixer),
};

/* DL2 mixer paths */
static const struct snd_kcontrol_new dl2_mixer_controls[] = {
	SOC_SINGLE_EXT("Tones", OMAP_AESS_MIXDL2_TONES, 0, 1, 0,
		abe_get_mixer, put_mixer),
	SOC_SINGLE_EXT("Voice", OMAP_AESS_MIXDL2_VX_DL, 0, 1, 0,
		abe_get_mixer, put_mixer),
	SOC_SINGLE_EXT("Capture", OMAP_AESS_MIXDL2_MM_UL2, 0, 1, 0,
		abe_get_mixer, put_mixer),
	SOC_SINGLE_EXT("Multimedia", OMAP_AESS_MIXDL2_MM_DL, 0, 1, 0,
		abe_get_mixer, put_mixer),
};

/* AUDUL ("Voice Capture Mixer") mixer paths */
static const struct snd_kcontrol_new audio_ul_mixer_controls[] = {
	SOC_SINGLE_EXT("Tones Playback", OMAP_AESS_MIXAUDUL_TONES, 0, 1, 0,
		abe_get_mixer, put_mixer),
	SOC_SINGLE_EXT("Media Playback", OMAP_AESS_MIXAUDUL_MM_DL, 0, 1, 0,
		abe_get_mixer, put_mixer),
	SOC_SINGLE_EXT("Capture", OMAP_AESS_MIXAUDUL_UPLINK, 0, 1, 0,
		abe_get_mixer, put_mixer),
};

/* VXREC ("Capture Mixer")  mixer paths */
static const struct snd_kcontrol_new vx_rec_mixer_controls[] = {
	SOC_SINGLE_EXT("Tones", OMAP_AESS_MIXVXREC_TONES, 0, 1, 0,
		abe_get_mixer, put_mixer),
	SOC_SINGLE_EXT("Voice Playback", OMAP_AESS_MIXVXREC_VX_DL,
		0, 1, 0, abe_get_mixer, put_mixer),
	SOC_SINGLE_EXT("Voice Capture", OMAP_AESS_MIXVXREC_VX_UL,
		0, 1, 0, abe_get_mixer, put_mixer),
	SOC_SINGLE_EXT("Media Playback", OMAP_AESS_MIXVXREC_MM_DL,
		0, 1, 0, abe_get_mixer, put_mixer),
};

/* SDT ("Sidetone Mixer") mixer paths */
static const struct snd_kcontrol_new sdt_mixer_controls[] = {
	SOC_SINGLE_EXT("Capture", OMAP_AESS_MIXSDT_UL, 0, 1, 0,
		abe_get_mixer, put_mixer),
	SOC_SINGLE_EXT("Playback", OMAP_AESS_MIXSDT_DL, 0, 1, 0,
		abe_get_mixer, put_mixer),
};

/* Virtual PDM_DL Switch */
static const struct snd_kcontrol_new pdm_dl1_switch_controls =
	SOC_SINGLE_EXT("Switch", OMAP_ABE_VIRTUAL_SWITCH, MIX_SWITCH_PDM_DL, 1, 0,
			abe_get_mixer, abe_put_switch);

/* Virtual BT_VX_DL Switch */
static const struct snd_kcontrol_new bt_vx_dl_switch_controls =
	SOC_SINGLE_EXT("Switch", OMAP_ABE_VIRTUAL_SWITCH, MIX_SWITCH_BT_VX_DL, 1, 0,
			abe_get_mixer, abe_put_switch);

/* Virtual MM_EXT_DL Switch */
static const struct snd_kcontrol_new mm_ext_dl_switch_controls =
	SOC_SINGLE_EXT("Switch", OMAP_ABE_VIRTUAL_SWITCH, MIX_SWITCH_MM_EXT_DL, 1, 0,
			abe_get_mixer, abe_put_switch);

static const struct snd_kcontrol_new abe_controls[] = {
	/* DL1 mixer gains */
	SOC_SINGLE_EXT_TLV("DL1 Media Playback Volume",
		OMAP_AESS_MIXDL1_MM_DL, 0, 149, 0,
		volume_get_mixer, volume_put_mixer, mm_dl1_tlv),
	SOC_SINGLE_EXT_TLV("DL1 Tones Playback Volume",
		OMAP_AESS_MIXDL1_TONES, 0, 149, 0,
		volume_get_mixer, volume_put_mixer, tones_dl1_tlv),
	SOC_SINGLE_EXT_TLV("DL1 Voice Playback Volume",
		OMAP_AESS_MIXDL1_VX_DL, 0, 149, 0,
		volume_get_mixer, volume_put_mixer, voice_dl1_tlv),
	SOC_SINGLE_EXT_TLV("DL1 Capture Playback Volume",
		OMAP_AESS_MIXDL1_MM_UL2, 0, 149, 0,
		volume_get_mixer, volume_put_mixer, capture_dl1_tlv),

	/* DL2 mixer gains */
	SOC_SINGLE_EXT_TLV("DL2 Media Playback Volume",
		OMAP_AESS_MIXDL2_MM_DL, 0, 149, 0,
		volume_get_mixer, volume_put_mixer, mm_dl2_tlv),
	SOC_SINGLE_EXT_TLV("DL2 Tones Playback Volume",
		OMAP_AESS_MIXDL2_TONES, 0, 149, 0,
		volume_get_mixer, volume_put_mixer, tones_dl2_tlv),
	SOC_SINGLE_EXT_TLV("DL2 Voice Playback Volume",
		OMAP_AESS_MIXDL2_VX_DL, 0, 149, 0,
		volume_get_mixer, volume_put_mixer, voice_dl2_tlv),
	SOC_SINGLE_EXT_TLV("DL2 Capture Playback Volume",
		OMAP_AESS_MIXDL2_MM_UL2, 0, 149, 0,
		volume_get_mixer, volume_put_mixer, capture_dl2_tlv),

	/* VXREC mixer gains */
	SOC_SINGLE_EXT_TLV("VXREC Media Volume",
		OMAP_AESS_MIXVXREC_MM_DL, 0, 149, 0,
		volume_get_mixer, volume_put_mixer, vxrec_mm_dl_tlv),
	SOC_SINGLE_EXT_TLV("VXREC Tones Volume",
		OMAP_AESS_MIXVXREC_TONES, 0, 149, 0,
		volume_get_mixer, volume_put_mixer, vxrec_tones_tlv),
	SOC_SINGLE_EXT_TLV("VXREC Voice DL Volume",
		OMAP_AESS_MIXVXREC_VX_UL, 0, 149, 0,
		volume_get_mixer, volume_put_mixer, vxrec_vx_dl_tlv),
	SOC_SINGLE_EXT_TLV("VXREC Voice UL Volume",
		OMAP_AESS_MIXVXREC_VX_DL, 0, 149, 0,
		volume_get_mixer, volume_put_mixer, vxrec_vx_ul_tlv),

	/* AUDUL mixer gains */
	SOC_SINGLE_EXT_TLV("AUDUL Media Volume",
		OMAP_AESS_MIXAUDUL_MM_DL, 0, 149, 0,
		volume_get_mixer, volume_put_mixer, audul_mm_tlv),
	SOC_SINGLE_EXT_TLV("AUDUL Tones Volume",
		OMAP_AESS_MIXAUDUL_TONES, 0, 149, 0,
		volume_get_mixer, volume_put_mixer, audul_tones_tlv),
	SOC_SINGLE_EXT_TLV("AUDUL Voice UL Volume",
		OMAP_AESS_MIXAUDUL_UPLINK, 0, 149, 0,
		volume_get_mixer, volume_put_mixer, audul_vx_ul_tlv),
	SOC_SINGLE_EXT_TLV("AUDUL Voice DL Volume",
		OMAP_AESS_MIXAUDUL_VX_DL, 0, 149, 0,
		volume_get_mixer, volume_put_mixer, audul_vx_dl_tlv),

	/* SDT mixer gains */
	SOC_SINGLE_EXT_TLV("SDT UL Volume",
		OMAP_AESS_MIXSDT_UL, 0, 149, 0,
		volume_get_mixer, volume_put_mixer, sdt_ul_tlv),
	SOC_SINGLE_EXT_TLV("SDT DL Volume",
		OMAP_AESS_MIXSDT_DL, 0, 149, 0,
		volume_get_mixer, volume_put_mixer, sdt_dl_tlv),

	/* DMIC gains */
	SOC_DOUBLE_EXT_TLV("DMIC1 UL Volume",
		GAINS_DMIC1, OMAP_AESS_GAIN_DMIC1_LEFT, OMAP_AESS_GAIN_DMIC1_RIGHT, 149, 0,
		volume_get_gain, volume_put_gain, dmic_tlv),

	SOC_DOUBLE_EXT_TLV("DMIC2 UL Volume",
		GAINS_DMIC2, OMAP_AESS_GAIN_DMIC2_LEFT, OMAP_AESS_GAIN_DMIC2_RIGHT, 149, 0,
		volume_get_gain, volume_put_gain, dmic_tlv),

	SOC_DOUBLE_EXT_TLV("DMIC3 UL Volume",
		GAINS_DMIC3, OMAP_AESS_GAIN_DMIC3_LEFT, OMAP_AESS_GAIN_DMIC3_RIGHT, 149, 0,
		volume_get_gain, volume_put_gain, dmic_tlv),

	SOC_DOUBLE_EXT_TLV("AMIC UL Volume",
		GAINS_AMIC, OMAP_AESS_GAIN_AMIC_LEFT, OMAP_AESS_GAIN_AMIC_RIGHT, 149, 0,
		volume_get_gain, volume_put_gain, amic_tlv),

	SOC_DOUBLE_EXT_TLV("BT UL Volume",
		GAINS_BTUL, OMAP_AESS_GAIN_BTUL_LEFT, OMAP_AESS_GAIN_BTUL_RIGHT, 149, 0,
		volume_get_gain, volume_put_gain, btul_tlv),
	SOC_SINGLE_EXT("DL1 Mono Mixer", MIXDL1, MIX_DL1_MONO, 1, 0,
		abe_get_mono_mixer, abe_put_mono_mixer),
	SOC_SINGLE_EXT("DL2 Mono Mixer", MIXDL2, MIX_DL2_MONO, 1, 0,
		abe_get_mono_mixer, abe_put_mono_mixer),
	SOC_SINGLE_EXT("AUDUL Mono Mixer", MIXAUDUL, MIX_AUDUL_MONO, 1, 0,
		abe_get_mono_mixer, abe_put_mono_mixer),
};

static const struct snd_soc_dapm_widget abe_dapm_widgets[] = {

	/* Frontend AIFs */
	SND_SOC_DAPM_AIF_IN("TONES_DL", NULL, 0,
			OMAP_ABE_AIF_TONES_DL, OMAP_ABE_OPP_25, 0),

	SND_SOC_DAPM_AIF_IN("VX_DL", NULL, 0,
			OMAP_ABE_AIF_VX_DL, OMAP_ABE_OPP_50, 0),
	SND_SOC_DAPM_AIF_OUT("VX_UL", NULL, 0,
			OMAP_ABE_AIF_VX_UL, OMAP_ABE_OPP_50, 0),

	SND_SOC_DAPM_AIF_OUT("MM_UL1", NULL, 0,
			OMAP_ABE_AIF_MM_UL1, OMAP_ABE_OPP_100, 0),
	SND_SOC_DAPM_AIF_OUT("MM_UL2", NULL, 0,
			OMAP_ABE_AIF_MM_UL2, OMAP_ABE_OPP_50, 0),

	SND_SOC_DAPM_AIF_IN("MM_DL", NULL, 0,
			OMAP_ABE_AIF_MM_DL, OMAP_ABE_OPP_25, 0),

	SND_SOC_DAPM_AIF_IN("VIB_DL", NULL, 0,
			OMAP_ABE_AIF_VIB_DL, OMAP_ABE_OPP_100, 0),
	SND_SOC_DAPM_AIF_IN("MODEM_DL", NULL, 0,
			OMAP_ABE_AIF_MODEM_DL, OMAP_ABE_OPP_50, 0),
	SND_SOC_DAPM_AIF_OUT("MODEM_UL", NULL, 0,
			OMAP_ABE_AIF_MODEM_UL, OMAP_ABE_OPP_50, 0),

	/* Backend DAIs  */
	SND_SOC_DAPM_AIF_IN("PDM_UL1", NULL, 0,
			OMAP_ABE_AIF_PDM_UL1, OMAP_ABE_OPP_50, 0),
	SND_SOC_DAPM_AIF_OUT("PDM_DL1", NULL, 0,
			OMAP_ABE_AIF_PDM_DL1, OMAP_ABE_OPP_25, 0),
	SND_SOC_DAPM_AIF_OUT("PDM_DL2", NULL, 0,
			OMAP_ABE_AIF_PDM_DL2, OMAP_ABE_OPP_100, 0),
	SND_SOC_DAPM_AIF_OUT("PDM_VIB", NULL, 0,
			OMAP_ABE_AIF_PDM_VIB, OMAP_ABE_OPP_100, 0),
	SND_SOC_DAPM_AIF_IN("BT_VX_UL", "BT Capture", 0,
			OMAP_ABE_AIF_BT_VX_UL, OMAP_ABE_OPP_50, 0),
	SND_SOC_DAPM_AIF_OUT("BT_VX_DL", "BT Playback", 0,
			OMAP_ABE_AIF_BT_VX_DL, OMAP_ABE_OPP_50, 0),
	SND_SOC_DAPM_AIF_IN("MM_EXT_UL", "FM Capture", 0,
			OMAP_ABE_AIF_MM_EXT_UL, OMAP_ABE_OPP_50, 0),
	SND_SOC_DAPM_AIF_OUT("MM_EXT_DL", "FM Playback", 0,
			OMAP_ABE_AIF_MM_EXT_DL, OMAP_ABE_OPP_25, 0),
	SND_SOC_DAPM_AIF_IN("DMIC0", "DMIC0 Capture", 0,
			OMAP_ABE_AIF_DMIC0, OMAP_ABE_OPP_50, 0),
	SND_SOC_DAPM_AIF_IN("DMIC1", "DMIC1 Capture", 0,
			OMAP_ABE_AIF_DMIC1, OMAP_ABE_OPP_50, 0),
	SND_SOC_DAPM_AIF_IN("DMIC2", "DMIC2 Capture", 0,
			OMAP_ABE_AIF_DMIC2, OMAP_ABE_OPP_50, 0),

	/* ROUTE_UL Capture Muxes */
	SND_SOC_DAPM_MUX("MUX_UL00",
			OMAP_ABE_MUX_UL00, OMAP_ABE_OPP_50, 0, &mm_ul00_control),
	SND_SOC_DAPM_MUX("MUX_UL01",
			OMAP_ABE_MUX_UL01, OMAP_ABE_OPP_50, 0, &mm_ul01_control),
	SND_SOC_DAPM_MUX("MUX_UL02",
			OMAP_ABE_MUX_UL02, OMAP_ABE_OPP_50, 0, &mm_ul02_control),
	SND_SOC_DAPM_MUX("MUX_UL03",
			OMAP_ABE_MUX_UL03, OMAP_ABE_OPP_50, 0, &mm_ul03_control),
	SND_SOC_DAPM_MUX("MUX_UL04",
			OMAP_ABE_MUX_UL04, OMAP_ABE_OPP_50, 0, &mm_ul04_control),
	SND_SOC_DAPM_MUX("MUX_UL05",
			OMAP_ABE_MUX_UL05, OMAP_ABE_OPP_50, 0, &mm_ul05_control),
	SND_SOC_DAPM_MUX("MUX_UL06",
			OMAP_ABE_MUX_UL06, OMAP_ABE_OPP_50, 0, &mm_ul06_control),
	SND_SOC_DAPM_MUX("MUX_UL07",
			OMAP_ABE_MUX_UL07, OMAP_ABE_OPP_50, 0, &mm_ul07_control),
	SND_SOC_DAPM_MUX("MUX_UL10",
			OMAP_ABE_MUX_UL10, OMAP_ABE_OPP_50, 0, &mm_ul10_control),
	SND_SOC_DAPM_MUX("MUX_UL11",
			OMAP_ABE_MUX_UL11, OMAP_ABE_OPP_50, 0, &mm_ul11_control),
	SND_SOC_DAPM_MUX("MUX_VX0",
			OMAP_ABE_MUX_VX00, OMAP_ABE_OPP_50, 0, &mm_vx0_control),
	SND_SOC_DAPM_MUX("MUX_VX1",
			OMAP_ABE_MUX_VX01, OMAP_ABE_OPP_50, 0, &mm_vx1_control),

	/* DL1 & DL2 Playback Mixers */
	SND_SOC_DAPM_MIXER("DL1 Mixer",
			OMAP_ABE_MIXER_DL1, OMAP_ABE_OPP_25, 0, dl1_mixer_controls,
			ARRAY_SIZE(dl1_mixer_controls)),
	SND_SOC_DAPM_MIXER("DL2 Mixer",
			OMAP_ABE_MIXER_DL2, OMAP_ABE_OPP_100, 0, dl2_mixer_controls,
			ARRAY_SIZE(dl2_mixer_controls)),

	/* DL1 Mixer Input volumes ?????*/
	SND_SOC_DAPM_PGA("DL1 Media Volume",
			OMAP_ABE_VOLUME_DL1, 0, 0, NULL, 0),

	/* AUDIO_UL_MIXER */
	SND_SOC_DAPM_MIXER("Voice Capture Mixer",
			OMAP_ABE_MIXER_AUDIO_UL, OMAP_ABE_OPP_50, 0,
			audio_ul_mixer_controls, ARRAY_SIZE(audio_ul_mixer_controls)),

	/* VX_REC_MIXER */
	SND_SOC_DAPM_MIXER("Capture Mixer",
			OMAP_ABE_MIXER_VX_REC, OMAP_ABE_OPP_50, 0, vx_rec_mixer_controls,
			ARRAY_SIZE(vx_rec_mixer_controls)),

	/* SDT_MIXER  */
	SND_SOC_DAPM_MIXER("Sidetone Mixer",
			OMAP_ABE_MIXER_SDT, OMAP_ABE_OPP_25, 0, sdt_mixer_controls,
			ARRAY_SIZE(sdt_mixer_controls)),

	/*
	 * The Following three are virtual switches to select the output port
	 * after DL1 Gain.
	 */

	/* Virtual PDM_DL1 Switch */
	SND_SOC_DAPM_MIXER("DL1 PDM", OMAP_ABE_VSWITCH_DL1_PDM,
			OMAP_ABE_OPP_25, 0, &pdm_dl1_switch_controls, 1),

	/* Virtual BT_VX_DL Switch */
	SND_SOC_DAPM_MIXER("DL1 BT_VX", OMAP_ABE_VSWITCH_DL1_BT_VX,
			OMAP_ABE_OPP_50, 0, &bt_vx_dl_switch_controls, 1),

	/* Virtual MM_EXT_DL Switch */
	SND_SOC_DAPM_MIXER("DL1 MM_EXT", OMAP_ABE_VSWITCH_DL1_MM_EXT,
			OMAP_ABE_OPP_50, 0, &mm_ext_dl_switch_controls, 1),

	/* Virtuals to join our capture sources */
	SND_SOC_DAPM_MIXER("Sidetone Capture VMixer", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("Voice Capture VMixer", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("DL1 Capture VMixer", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("DL2 Capture VMixer", SND_SOC_NOPM, 0, 0, NULL, 0),

	/* Virtual MODEM and VX_UL mixer */
	SND_SOC_DAPM_MIXER("VX UL VMixer", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("VX DL VMixer", SND_SOC_NOPM, 0, 0, NULL, 0),

	/* Virtual Pins to force backends ON atm */
	SND_SOC_DAPM_OUTPUT("BE_OUT"),
	SND_SOC_DAPM_INPUT("BE_IN"),
};

static const struct snd_soc_dapm_route intercon[] = {

	/* MUX_UL00 - ROUTE_UL - Chan 0  */
	{"MUX_UL00", "DMic0L", "DMIC0"},
	{"MUX_UL00", "DMic0R", "DMIC0"},
	{"MUX_UL00", "DMic1L", "DMIC1"},
	{"MUX_UL00", "DMic1R", "DMIC1"},
	{"MUX_UL00", "DMic2L", "DMIC2"},
	{"MUX_UL00", "DMic2R", "DMIC2"},
	{"MUX_UL00", "BT Left", "BT_VX_UL"},
	{"MUX_UL00", "BT Right", "BT_VX_UL"},
	{"MUX_UL00", "MMExt Left", "MM_EXT_UL"},
	{"MUX_UL00", "MMExt Right", "MM_EXT_UL"},
	{"MUX_UL00", "AMic0", "PDM_UL1"},
	{"MUX_UL00", "AMic1", "PDM_UL1"},
	{"MUX_UL00", "VX Left", "Capture Mixer"},
	{"MUX_UL00", "VX Right", "Capture Mixer"},
	{"MM_UL1", NULL, "MUX_UL00"},

	/* MUX_UL01 - ROUTE_UL - Chan 1  */
	{"MUX_UL01", "DMic0L", "DMIC0"},
	{"MUX_UL01", "DMic0R", "DMIC0"},
	{"MUX_UL01", "DMic1L", "DMIC1"},
	{"MUX_UL01", "DMic1R", "DMIC1"},
	{"MUX_UL01", "DMic2L", "DMIC2"},
	{"MUX_UL01", "DMic2R", "DMIC2"},
	{"MUX_UL01", "BT Left", "BT_VX_UL"},
	{"MUX_UL01", "BT Right", "BT_VX_UL"},
	{"MUX_UL01", "MMExt Left", "MM_EXT_UL"},
	{"MUX_UL01", "MMExt Right", "MM_EXT_UL"},
	{"MUX_UL01", "AMic0", "PDM_UL1"},
	{"MUX_UL01", "AMic1", "PDM_UL1"},
	{"MUX_UL01", "VX Left", "Capture Mixer"},
	{"MUX_UL01", "VX Right", "Capture Mixer"},
	{"MM_UL1", NULL, "MUX_UL01"},

	/* MUX_UL02 - ROUTE_UL - Chan 2  */
	{"MUX_UL02", "DMic0L", "DMIC0"},
	{"MUX_UL02", "DMic0R", "DMIC0"},
	{"MUX_UL02", "DMic1L", "DMIC1"},
	{"MUX_UL02", "DMic1R", "DMIC1"},
	{"MUX_UL02", "DMic2L", "DMIC2"},
	{"MUX_UL02", "DMic2R", "DMIC2"},
	{"MUX_UL02", "BT Left", "BT_VX_UL"},
	{"MUX_UL02", "BT Right", "BT_VX_UL"},
	{"MUX_UL02", "MMExt Left", "MM_EXT_UL"},
	{"MUX_UL02", "MMExt Right", "MM_EXT_UL"},
	{"MUX_UL02", "AMic0", "PDM_UL1"},
	{"MUX_UL02", "AMic1", "PDM_UL1"},
	{"MUX_UL02", "VX Left", "Capture Mixer"},
	{"MUX_UL02", "VX Right", "Capture Mixer"},
	{"MM_UL1", NULL, "MUX_UL02"},

	/* MUX_UL03 - ROUTE_UL - Chan 3  */
	{"MUX_UL03", "DMic0L", "DMIC0"},
	{"MUX_UL03", "DMic0R", "DMIC0"},
	{"MUX_UL03", "DMic1L", "DMIC1"},
	{"MUX_UL03", "DMic1R", "DMIC1"},
	{"MUX_UL03", "DMic2L", "DMIC2"},
	{"MUX_UL03", "DMic2R", "DMIC2"},
	{"MUX_UL03", "BT Left", "BT_VX_UL"},
	{"MUX_UL03", "BT Right", "BT_VX_UL"},
	{"MUX_UL03", "MMExt Left", "MM_EXT_UL"},
	{"MUX_UL03", "MMExt Right", "MM_EXT_UL"},
	{"MUX_UL03", "AMic0", "PDM_UL1"},
	{"MUX_UL03", "AMic1", "PDM_UL1"},
	{"MUX_UL03", "VX Left", "Capture Mixer"},
	{"MUX_UL03", "VX Right", "Capture Mixer"},
	{"MM_UL1", NULL, "MUX_UL03"},

	/* MUX_UL04 - ROUTE_UL - Chan 4  */
	{"MUX_UL04", "DMic0L", "DMIC0"},
	{"MUX_UL04", "DMic0R", "DMIC0"},
	{"MUX_UL04", "DMic1L", "DMIC1"},
	{"MUX_UL04", "DMic1R", "DMIC1"},
	{"MUX_UL04", "DMic2L", "DMIC2"},
	{"MUX_UL04", "DMic2R", "DMIC2"},
	{"MUX_UL04", "BT Left", "BT_VX_UL"},
	{"MUX_UL04", "BT Right", "BT_VX_UL"},
	{"MUX_UL04", "MMExt Left", "MM_EXT_UL"},
	{"MUX_UL04", "MMExt Right", "MM_EXT_UL"},
	{"MUX_UL04", "AMic0", "PDM_UL1"},
	{"MUX_UL04", "AMic1", "PDM_UL1"},
	{"MUX_UL04", "VX Left", "Capture Mixer"},
	{"MUX_UL04", "VX Right", "Capture Mixer"},
	{"MM_UL1", NULL, "MUX_UL04"},

	/* MUX_UL05 - ROUTE_UL - Chan 5  */
	{"MUX_UL05", "DMic0L", "DMIC0"},
	{"MUX_UL05", "DMic0R", "DMIC0"},
	{"MUX_UL05", "DMic1L", "DMIC1"},
	{"MUX_UL05", "DMic1R", "DMIC1"},
	{"MUX_UL05", "DMic2L", "DMIC2"},
	{"MUX_UL05", "DMic2R", "DMIC2"},
	{"MUX_UL05", "BT Left", "BT_VX_UL"},
	{"MUX_UL05", "BT Right", "BT_VX_UL"},
	{"MUX_UL05", "MMExt Left", "MM_EXT_UL"},
	{"MUX_UL05", "MMExt Right", "MM_EXT_UL"},
	{"MUX_UL05", "AMic0", "PDM_UL1"},
	{"MUX_UL05", "AMic1", "PDM_UL1"},
	{"MUX_UL05", "VX Left", "Capture Mixer"},
	{"MUX_UL05", "VX Right", "Capture Mixer"},
	{"MM_UL1", NULL, "MUX_UL05"},

	/* MUX_UL06 - ROUTE_UL - Chan 6  */
	{"MUX_UL06", "DMic0L", "DMIC0"},
	{"MUX_UL06", "DMic0R", "DMIC0"},
	{"MUX_UL06", "DMic1L", "DMIC1"},
	{"MUX_UL06", "DMic1R", "DMIC1"},
	{"MUX_UL06", "DMic2L", "DMIC2"},
	{"MUX_UL06", "DMic2R", "DMIC2"},
	{"MUX_UL06", "BT Left", "BT_VX_UL"},
	{"MUX_UL06", "BT Right", "BT_VX_UL"},
	{"MUX_UL06", "MMExt Left", "MM_EXT_UL"},
	{"MUX_UL06", "MMExt Right", "MM_EXT_UL"},
	{"MUX_UL06", "AMic0", "PDM_UL1"},
	{"MUX_UL06", "AMic1", "PDM_UL1"},
	{"MUX_UL06", "VX Left", "Capture Mixer"},
	{"MUX_UL06", "VX Right", "Capture Mixer"},
	{"MM_UL1", NULL, "MUX_UL06"},

	/* MUX_UL07 - ROUTE_UL - Chan 7  */
	{"MUX_UL07", "DMic0L", "DMIC0"},
	{"MUX_UL07", "DMic0R", "DMIC0"},
	{"MUX_UL07", "DMic1L", "DMIC1"},
	{"MUX_UL07", "DMic1R", "DMIC1"},
	{"MUX_UL07", "DMic2L", "DMIC2"},
	{"MUX_UL07", "DMic2R", "DMIC2"},
	{"MUX_UL07", "BT Left", "BT_VX_UL"},
	{"MUX_UL07", "BT Right", "BT_VX_UL"},
	{"MUX_UL07", "MMExt Left", "MM_EXT_UL"},
	{"MUX_UL07", "MMExt Right", "MM_EXT_UL"},
	{"MUX_UL07", "AMic0", "PDM_UL1"},
	{"MUX_UL07", "AMic1", "PDM_UL1"},
	{"MUX_UL07", "VX Left", "Capture Mixer"},
	{"MUX_UL07", "VX Right", "Capture Mixer"},
	{"MM_UL1", NULL, "MUX_UL07"},

	/* MUX_UL10 - ROUTE_UL - Chan 0  */
	{"MUX_UL10", "DMic0L", "DMIC0"},
	{"MUX_UL10", "DMic0R", "DMIC0"},
	{"MUX_UL10", "DMic1L", "DMIC1"},
	{"MUX_UL10", "DMic1R", "DMIC1"},
	{"MUX_UL10", "DMic2L", "DMIC2"},
	{"MUX_UL10", "DMic2R", "DMIC2"},
	{"MUX_UL10", "BT Left", "BT_VX_UL"},
	{"MUX_UL10", "BT Right", "BT_VX_UL"},
	{"MUX_UL10", "MMExt Left", "MM_EXT_UL"},
	{"MUX_UL10", "MMExt Right", "MM_EXT_UL"},
	{"MUX_UL10", "AMic0", "PDM_UL1"},
	{"MUX_UL10", "AMic1", "PDM_UL1"},
	{"MUX_UL10", "VX Left", "Capture Mixer"},
	{"MUX_UL10", "VX Right", "Capture Mixer"},
	{"MM_UL2", NULL, "MUX_UL10"},

	/* MUX_UL11 - ROUTE_UL - Chan 1  */
	{"MUX_UL11", "DMic0L", "DMIC0"},
	{"MUX_UL11", "DMic0R", "DMIC0"},
	{"MUX_UL11", "DMic1L", "DMIC1"},
	{"MUX_UL11", "DMic1R", "DMIC1"},
	{"MUX_UL11", "DMic2L", "DMIC2"},
	{"MUX_UL11", "DMic2R", "DMIC2"},
	{"MUX_UL11", "BT Left", "BT_VX_UL"},
	{"MUX_UL11", "BT Right", "BT_VX_UL"},
	{"MUX_UL11", "MMExt Left", "MM_EXT_UL"},
	{"MUX_UL11", "MMExt Right", "MM_EXT_UL"},
	{"MUX_UL11", "AMic0", "PDM_UL1"},
	{"MUX_UL11", "AMic1", "PDM_UL1"},
	{"MUX_UL11", "VX Left", "Capture Mixer"},
	{"MUX_UL11", "VX Right", "Capture Mixer"},
	{"MM_UL2", NULL, "MUX_UL11"},

	/* MUX_VX0 - ROUTE_UL - Chan 0  */
	{"MUX_VX0", "DMic0L", "DMIC0"},
	{"MUX_VX0", "DMic0R", "DMIC0"},
	{"MUX_VX0", "DMic1L", "DMIC1"},
	{"MUX_VX0", "DMic1R", "DMIC1"},
	{"MUX_VX0", "DMic2L", "DMIC2"},
	{"MUX_VX0", "DMic2R", "DMIC2"},
	{"MUX_VX0", "BT Left", "BT_VX_UL"},
	{"MUX_VX0", "BT Right", "BT_VX_UL"},
	{"MUX_VX0", "MMExt Left", "MM_EXT_UL"},
	{"MUX_VX0", "MMExt Right", "MM_EXT_UL"},
	{"MUX_VX0", "AMic0", "PDM_UL1"},
	{"MUX_VX0", "AMic1", "PDM_UL1"},
	{"MUX_VX0", "VX Left", "Capture Mixer"},
	{"MUX_VX0", "VX Right", "Capture Mixer"},

	/* MUX_VX1 - ROUTE_UL - Chan 1 */
	{"MUX_VX1", "DMic0L", "DMIC0"},
	{"MUX_VX1", "DMic0R", "DMIC0"},
	{"MUX_VX1", "DMic1L", "DMIC1"},
	{"MUX_VX1", "DMic1R", "DMIC1"},
	{"MUX_VX1", "DMic2L", "DMIC2"},
	{"MUX_VX1", "DMic2R", "DMIC2"},
	{"MUX_VX1", "BT Left", "BT_VX_UL"},
	{"MUX_VX1", "BT Right", "BT_VX_UL"},
	{"MUX_VX1", "MMExt Left", "MM_EXT_UL"},
	{"MUX_VX1", "MMExt Right", "MM_EXT_UL"},
	{"MUX_VX1", "AMic0", "PDM_UL1"},
	{"MUX_VX1", "AMic1", "PDM_UL1"},
	{"MUX_VX1", "VX Left", "Capture Mixer"},
	{"MUX_VX1", "VX Right", "Capture Mixer"},

	/* Headset (DL1)  playback path */
	{"DL1 Mixer", "Tones", "TONES_DL"},
	{"DL1 Mixer", "Voice", "VX DL VMixer"},
	{"DL1 Mixer", "Capture", "DL1 Capture VMixer"},
	{"DL1 Capture VMixer", NULL, "MUX_UL10"},
	{"DL1 Capture VMixer", NULL, "MUX_UL11"},
	{"DL1 Mixer", "Multimedia", "MM_DL"},

	/* Sidetone Mixer */
	{"Sidetone Mixer", "Playback", "DL1 Mixer"},
	{"Sidetone Mixer", "Capture", "Sidetone Capture VMixer"},
	{"Sidetone Capture VMixer", NULL, "MUX_VX0"},
	{"Sidetone Capture VMixer", NULL, "MUX_VX1"},

	/* Playback Output selection after DL1 Gain */
	{"DL1 BT_VX", "Switch", "Sidetone Mixer"},
	{"DL1 MM_EXT", "Switch", "Sidetone Mixer"},
	{"DL1 PDM", "Switch", "Sidetone Mixer"},
	{"PDM_DL1", NULL, "DL1 PDM"},
	{"BT_VX_DL", NULL, "DL1 BT_VX"},
	{"MM_EXT_DL", NULL, "DL1 MM_EXT"},

	/* Handsfree (DL2) playback path */
	{"DL2 Mixer", "Tones", "TONES_DL"},
	{"DL2 Mixer", "Voice", "VX DL VMixer"},
	{"DL2 Mixer", "Capture", "DL2 Capture VMixer"},
	{"DL2 Capture VMixer", NULL, "MUX_UL10"},
	{"DL2 Capture VMixer", NULL, "MUX_UL11"},
	{"DL2 Mixer", "Multimedia", "MM_DL"},
	{"PDM_DL2", NULL, "DL2 Mixer"},

	/* VxREC Mixer */
	{"Capture Mixer", "Tones", "TONES_DL"},
	{"Capture Mixer", "Voice Playback", "VX DL VMixer"},
	{"Capture Mixer", "Voice Capture", "VX UL VMixer"},
	{"Capture Mixer", "Media Playback", "MM_DL"},

	/* Audio UL mixer */
	{"Voice Capture Mixer", "Tones Playback", "TONES_DL"},
	{"Voice Capture Mixer", "Media Playback", "MM_DL"},
	{"Voice Capture Mixer", "Capture", "Voice Capture VMixer"},
	{"Voice Capture VMixer", NULL, "MUX_VX0"},
	{"Voice Capture VMixer", NULL, "MUX_VX1"},

	/* BT */
	{"VX UL VMixer", NULL, "Voice Capture Mixer"},

	/* Vibra */
	{"PDM_VIB", NULL, "VIB_DL"},

	/* VX and MODEM */
	{"VX_UL", NULL, "VX UL VMixer"},
	{"MODEM_UL", NULL, "VX UL VMixer"},
	{"VX DL VMixer", NULL, "VX_DL"},
	{"VX DL VMixer", NULL, "MODEM_DL"},

	/* DAIs */
	{"MM1 Capture", NULL, "MM_UL1"},
	{"MM2 Capture", NULL, "MM_UL2"},
	{"TONES_DL", NULL, "Tones Playback"},
	{"VIB_DL", NULL, "Vibra Playback"},
	{"MM_DL", NULL, "MM1 Playback"},
	{"MM_DL", NULL, "MMLP Playback"},
	{"VX_DL", NULL, "Voice Playback"},
	{"Voice Capture", NULL, "VX_UL"},
	{"MODEM_DL", NULL, "Modem Playback"},
	{"Modem Capture", NULL, "MODEM_UL"},

	/* Backend Enablement */
	{"BE_OUT", NULL, "PDM_DL1"},
	{"BE_OUT", NULL, "PDM_DL2"},
	{"BE_OUT", NULL, "PDM_VIB"},
	{"BE_OUT", NULL, "MM_EXT_DL"},
	{"BE_OUT", NULL, "BT_VX_DL"},
	{"PDM_UL1", NULL, "BE_IN"},
	{"BT_VX_UL", NULL, "BE_IN"},
	{"MM_EXT_UL", NULL, "BE_IN"},
	{"DMIC0", NULL, "BE_IN"},
	{"DMIC1", NULL, "BE_IN"},
	{"DMIC2", NULL, "BE_IN"},
};

int abe_mixer_add_widgets(struct snd_soc_platform *platform)
{
	struct omap_abe *abe = snd_soc_platform_get_drvdata(platform);
	struct fw_header *hdr = &abe->hdr;
	int i, j;

	/* create equalizer controls */
	for (i = 0; i < hdr->num_equ; i++) {
		struct soc_enum *equ_enum = &abe->equ.senum[i];
		struct snd_kcontrol_new *equ_kcontrol = &abe->equ.kcontrol[i];

		equ_enum->reg = i;
		equ_enum->max = abe->equ.texts[i].count;
		for (j = 0; j < abe->equ.texts[i].count; j++)
			equ_enum->dtexts[j] = abe->equ.texts[i].texts[j];

		equ_kcontrol->name = abe->equ.texts[i].name;
		equ_kcontrol->private_value = (unsigned long)equ_enum;
		equ_kcontrol->get = abe_get_equalizer;
		equ_kcontrol->put = abe_put_equalizer;
		equ_kcontrol->info = snd_soc_info_enum_ext1;
		equ_kcontrol->iface = SNDRV_CTL_ELEM_IFACE_MIXER;

		dev_dbg(platform->dev, "added EQU mixer: %s profiles %d\n",
				abe->equ.texts[i].name, abe->equ.texts[i].count);

		for (j = 0; j < abe->equ.texts[i].count; j++)
			dev_dbg(platform->dev, " %s\n", equ_enum->dtexts[j]);
	}

	snd_soc_add_platform_controls(platform, abe->equ.kcontrol, hdr->num_equ);

	snd_soc_add_platform_controls(platform, abe_controls,
			ARRAY_SIZE(abe_controls));

	snd_soc_dapm_new_controls(&platform->dapm, abe_dapm_widgets,
				 ARRAY_SIZE(abe_dapm_widgets));

	snd_soc_dapm_add_routes(&platform->dapm, intercon, ARRAY_SIZE(intercon));

	snd_soc_dapm_new_widgets(&platform->dapm);

	return 0;
}
