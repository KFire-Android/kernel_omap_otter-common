/*
 * omap-abe-dsp.c
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
 * Copyright (C) 2010 Texas Instruments Inc.
 *
 * Author : Liam Girdwood <lrg@slimlogic.co.uk>
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/i2c/twl.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/pm_runtime.h>

#include <plat/omap_hwmod.h>
#include <plat/omap_device.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include <sound/omap-abe-dsp.h>

#include "omap-abe-dsp.h"
#include "omap-abe.h"
#include "abe/abe_main.h"

// TODO: change to S16 and use ARM SIMD to re-format to S32
#define ABE_FORMATS	 (SNDRV_PCM_FMTBIT_S32_LE)

// TODO: make all these into virtual registers or similar */
#define ABE_NUM_MIXERS		21
#define ABE_NUM_MUXES		12
#define ABE_NUM_WIDGETS	41	/* TODO - refine this val */
#define ABE_NUM_DAPM_REG		\
	(ABE_NUM_MIXERS + ABE_NUM_MUXES + ABE_NUM_WIDGETS)
#define ABE_WIDGET_START	(ABE_NUM_MIXERS + ABE_NUM_MUXES)
#define ABE_WIDGET_END	(ABE_WIDGET_START + ABE_NUM_WIDGETS)
#define ABE_BE_START		(ABE_WIDGET_START + 7)
#define ABE_BE_END	(ABE_BE_START + 10)

/* Uplink MUX path identifiers from ROUTE_UL */
#define ABE_MM_UL1(x)		(x + ABE_NUM_MIXERS)
#define ABE_MM_UL2(x)		(x + ABE_NUM_MIXERS + 8)
#define ABE_VX_UL(x)			(x + ABE_NUM_MIXERS + 10)
#define ABE_WIDGET(x)		(x + ABE_NUM_MIXERS + ABE_NUM_MUXES)
//#define ABE_BE_WIDGET(x)		(x + ABE_NUM_MIXERS + ABE_NUM_MUXES)

#define VIRT_SWITCH	0

// TODO: OPP bitmask - Use HAL version after update
#define ABE_OPP_25		0
#define ABE_OPP_50		1
#define ABE_OPP_100		2

/*
 * ABE driver
 *
 * TODO: tasks 1,2,3,4,5,6,7,8 below
 *
 * This driver is responsible for :-
 * 1) ABE init - including loading ABE firmware
 * 2) ABE coefficient mgmt - inc user space re-calcs
 * 8) Process ABE platform data
 *
 */

/* ABE private data */
struct abe_data {
	struct omap4_abe_dsp_pdata *abe_pdata;
	struct platform_device *pdev;
	struct snd_soc_platform *platform;

	struct delayed_work delayed_work;
	struct mutex mutex;

	int fe_id;
	struct omap_aess_fe fe[5];

	/* DAPM mixer config - TODO: some of this can be replaced with HAL update */
	u32 dapm[ABE_NUM_DAPM_REG];

	int mm_dl1_volume;
	int tones_dl1_volume;
	int voice_dl1_volume;
	int capture_dl1_volume;

	int mm_dl2_volume;
	int tones_dl2_volume;
	int voice_dl2_volume;
	int capture_dl2_volume;
};

static struct abe_data *abe;

/* TODO: use int - do we need to reload these over time ? */
static const s32 DL2_COEF [25] =	{
		-7554223, 708210, -708206, 7554225,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 6802833, -682266, 731554
};

// TODO: map to the new version of HAL
static unsigned int abe_dsp_read(struct snd_soc_platform *platform,
		unsigned int reg)
{
	struct abe_data *priv = snd_soc_platform_get_drvdata(platform);
	return priv->dapm[reg];
}

static int abe_dsp_write(struct snd_soc_platform *platform, unsigned int reg,
		unsigned int val)
{
	struct abe_data *priv = snd_soc_platform_get_drvdata(platform);
	priv->dapm[reg] = val;

	printk("fe: %d widget %d %s\n", abe->fe_id,
			reg - ABE_WIDGET_START, val ? "on" : "off");

	if (reg >= ABE_BE_START && reg < ABE_BE_END)
		abe->fe[abe->fe_id].active[reg - ABE_BE_START] = val;

	return 0;
}

static void abe_init_engine(struct snd_soc_platform *platform)
{
	struct abe_data *priv = snd_soc_platform_get_drvdata(platform);
#ifdef CONFIG_PM_RUNTIME
	struct omap4_abe_dsp_pdata *pdata = priv->abe_pdata;
#endif
	struct platform_device *pdev = priv->pdev;
	abe_opp_t OPP = ABE_OPP100;
	abe_equ_t dl2_eq;

	dl2_eq.equ_length = 25;

	/* build the coefficient parameter for the equalizer api */
	memcpy(dl2_eq.coef.type1, DL2_COEF, sizeof(DL2_COEF));

	abe_init_mem();

	/* aess_clk has to be enabled to access hal register.
	 * Disable the clk after it has been used.
	 */
	pm_runtime_get_sync(&pdev->dev);
#ifdef CONFIG_PM_RUNTIME
	if (pdata->device_enable)
		pdata->device_enable(pdev);
#endif

	abe_reset_hal();

	abe_load_fw();	// TODO: use fw API here


	/* Config OPP 100 for now */
	abe_set_opp_processing(OPP);

	/* "tick" of the audio engine */
	abe_write_event_generator(EVENT_TIMER);

	/* TODO: make these ALSA kcontrols */
	abe_write_mixer(MIXDL1, MUTE_GAIN, RAMP_0MS, MIX_DL1_INPUT_MM_DL);
	abe_write_mixer(MIXDL1, MUTE_GAIN, RAMP_0MS, MIX_DL1_INPUT_MM_UL2);
	abe_write_mixer(MIXDL1, GAIN_M6dB, RAMP_0MS, MIX_DL1_INPUT_VX_DL);
	abe_write_mixer(MIXDL1, MUTE_GAIN, RAMP_0MS, MIX_DL1_INPUT_TONES);

	abe_write_mixer(MIXDL2, GAIN_M6dB, RAMP_0MS, MIX_DL2_INPUT_TONES);
	abe_write_mixer(MIXDL2, MUTE_GAIN, RAMP_0MS, MIX_DL2_INPUT_VX_DL);
	abe_write_mixer(MIXDL2, GAIN_M6dB, RAMP_0MS, MIX_DL2_INPUT_MM_DL);
	abe_write_mixer(MIXDL2, MUTE_GAIN, RAMP_0MS, MIX_DL2_INPUT_MM_UL2);

	abe_write_mixer(MIXSDT, MUTE_GAIN, RAMP_0MS, MIX_SDT_INPUT_UP_MIXER);
	abe_write_mixer(MIXSDT, GAIN_0dB, RAMP_0MS, MIX_SDT_INPUT_DL1_MIXER);

	abe_write_mixer(MIXECHO, MUTE_GAIN, RAMP_0MS, GAIN_LEFT_OFFSET);
	abe_write_mixer(MIXECHO, MUTE_GAIN, RAMP_0MS, GAIN_RIGHT_OFFSET);

	abe_write_mixer(MIXAUDUL, MUTE_GAIN, RAMP_0MS, MIX_AUDUL_INPUT_TONES);
	abe_write_mixer(MIXAUDUL, GAIN_M6dB, RAMP_0MS, MIX_AUDUL_INPUT_UPLINK);
	abe_write_mixer(MIXAUDUL, MUTE_GAIN, RAMP_0MS, MIX_AUDUL_INPUT_MM_DL);
	abe_write_mixer(MIXAUDUL, MUTE_GAIN, RAMP_0MS, MIX_AUDUL_INPUT_VX_DL);

	abe_write_mixer(MIXVXREC, MUTE_GAIN, RAMP_0MS, MIX_VXREC_INPUT_TONES);
	abe_write_mixer(MIXVXREC, MUTE_GAIN, RAMP_0MS, MIX_VXREC_INPUT_VX_DL);
	abe_write_mixer(MIXVXREC, MUTE_GAIN, RAMP_0MS, MIX_VXREC_INPUT_MM_DL);
	abe_write_mixer(MIXVXREC, MUTE_GAIN, RAMP_0MS, MIX_VXREC_INPUT_VX_UL);

	/* load the high-pass coefficient of IHF-Right */
	abe_write_equalizer(EQ2L, &dl2_eq);

	/* load the high-pass coefficient of IHF-Left */
	abe_write_equalizer(EQ2R, &dl2_eq);

	pm_runtime_get_sync(&pdev->dev);
#ifdef CONFIG_PM_RUNTIME
	if (pdata->device_idle)
		pdata->device_idle(pdev);
#endif
}

/* Media DL1 volume control from -50 to 30 dB in 6 dB steps */
static DECLARE_TLV_DB_SCALE(mm_dl1_tlv, -5000, 600, 3000);

/* Media DL1 volume control from -50 to 30 dB in 6 dB steps */
static DECLARE_TLV_DB_SCALE(tones_dl1_tlv, -5000, 600, 3000);

/* Media DL1 volume control from -50 to 30 dB in 6 dB steps */
static DECLARE_TLV_DB_SCALE(voice_dl1_tlv, -5000, 600, 3000);

/* Media DL1 volume control from -50 to 30 dB in 6 dB steps */
static DECLARE_TLV_DB_SCALE(capture_dl1_tlv, -5000, 600, 3000);

/* Media DL2 volume control from -50 to 30 dB in 6 dB steps */
static DECLARE_TLV_DB_SCALE(mm_dl2_tlv, -5000, 600, 3000);

/* Media DL2 volume control from -50 to 30 dB in 6 dB steps */
static DECLARE_TLV_DB_SCALE(tones_dl2_tlv, -5000, 600, 3000);

/* Media DL2 volume control from -50 to 30 dB in 6 dB steps */
static DECLARE_TLV_DB_SCALE(voice_dl2_tlv, -5000, 600, 3000);

/* Media DL2 volume control from -50 to 30 dB in 6 dB steps */
static DECLARE_TLV_DB_SCALE(capture_dl2_tlv, -5000, 600, 3000);

static const struct snd_kcontrol_new abe_snd_controls[] = {

};

//TODO: we have to use the shift value atm to represent register id due to current HAL
static int dl1_put_mixer(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget *widget = snd_kcontrol_chip(kcontrol);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;

	// TODO: restore old gain values after mute
	if (ucontrol->value.integer.value[0]) {
		abe_write_mixer(MIXDL1, GAIN_M6dB, RAMP_0MS, mc->reg);
		abe->dapm[mc->shift] = ucontrol->value.integer.value[0];
		snd_soc_dapm_mixer_update_power(widget, kcontrol, 1);
	} else {
		abe_write_mixer(MIXDL1, MUTE_GAIN, RAMP_0MS, mc->reg);
		abe->dapm[mc->shift] = ucontrol->value.integer.value[0];
		snd_soc_dapm_mixer_update_power(widget, kcontrol, 0);
	}

	return 1;
}

static int dl2_put_mixer(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget *widget = snd_kcontrol_chip(kcontrol);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;

	// TODO: restore old gain values after mute
	if (ucontrol->value.integer.value[0]) {
		abe_write_mixer(MIXDL2, GAIN_M6dB, RAMP_0MS, mc->reg);
		abe->dapm[mc->shift] = ucontrol->value.integer.value[0];
		snd_soc_dapm_mixer_update_power(widget, kcontrol, 1);
	} else {
		abe_write_mixer(MIXDL2, MUTE_GAIN, RAMP_0MS, mc->reg);
		abe->dapm[mc->shift] = ucontrol->value.integer.value[0];
		snd_soc_dapm_mixer_update_power(widget, kcontrol, 0);
	}

	return 1;
}

static int audio_ul_put_mixer(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget *widget = snd_kcontrol_chip(kcontrol);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;

	// TODO: restore old gain values after mute
	if (ucontrol->value.integer.value[0]) {
		abe_write_mixer(MIXAUDUL, GAIN_M6dB, RAMP_0MS, mc->reg);
		abe->dapm[mc->shift] = ucontrol->value.integer.value[0];
		snd_soc_dapm_mixer_update_power(widget, kcontrol, 1);
	} else {
		abe_write_mixer(MIXAUDUL, MUTE_GAIN, RAMP_0MS, mc->reg);
		abe->dapm[mc->shift] = ucontrol->value.integer.value[0];
		snd_soc_dapm_mixer_update_power(widget, kcontrol, 0);
	}

	return 1;
}

static int vxrec_put_mixer(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget *widget = snd_kcontrol_chip(kcontrol);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;

	// TODO: restore old gain values after mute
	if (ucontrol->value.integer.value[0]) {
		abe_write_mixer(MIXVXREC, GAIN_M6dB, RAMP_0MS, mc->reg);
		abe->dapm[mc->shift] = ucontrol->value.integer.value[0];
		snd_soc_dapm_mixer_update_power(widget, kcontrol, 1);
	} else {
		abe_write_mixer(MIXVXREC, MUTE_GAIN, RAMP_0MS, mc->reg);
		abe->dapm[mc->shift] = ucontrol->value.integer.value[0];
		snd_soc_dapm_mixer_update_power(widget, kcontrol, 0);
	}

	return 1;
}

static int sdt_put_mixer(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget *widget = snd_kcontrol_chip(kcontrol);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;

	// TODO: restore old gain values after mute
	if (ucontrol->value.integer.value[0]) {
		abe_write_mixer(MIXSDT, GAIN_M6dB, RAMP_0MS, mc->reg);
		abe->dapm[mc->shift] = ucontrol->value.integer.value[0];
		snd_soc_dapm_mixer_update_power(widget, kcontrol, 1);
	} else {
		abe_write_mixer(MIXSDT, MUTE_GAIN, RAMP_0MS, mc->reg);
		abe->dapm[mc->shift] = ucontrol->value.integer.value[0];
		snd_soc_dapm_mixer_update_power(widget, kcontrol, 0);
	}

	return 1;
}

static int abe_get_mixer(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;

	ucontrol->value.integer.value[0] = abe->dapm[mc->shift];
	return 0;
}

static int ul_mux_put_route(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget *widget = snd_kcontrol_chip(kcontrol);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	int mux = ucontrol->value.enumerated.item[0];

	// TODO: set mux via HAL
	abe->dapm[e->reg] = ucontrol->value.integer.value[0];
	snd_soc_dapm_mux_update_power(widget, kcontrol, abe->dapm[e->reg], mux, e);
	return 1;
}

static int ul_mux_get_route(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_enum *e =
		(struct soc_enum *)kcontrol->private_value;

	// TODO: get mux via HAL

	ucontrol->value.integer.value[0] = abe->dapm[e->reg];
	return 0;
}


static int abe_put_switch(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget *widget = snd_kcontrol_chip(kcontrol);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;

	if (ucontrol->value.integer.value[0]) {
		abe->dapm[mc->shift] = ucontrol->value.integer.value[0];
		snd_soc_dapm_mixer_update_power(widget, kcontrol, 1);
	} else {
		abe->dapm[mc->shift] = ucontrol->value.integer.value[0];
		snd_soc_dapm_mixer_update_power(widget, kcontrol, 0);
	}
	return 1;
}

static int volume_put_dl1_mixer(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;

	// TODO get ramp times from kcontrol
	switch (mc->reg) {
	case MIX_DL1_INPUT_MM_DL:
		abe_write_mixer(MIXDL1, -5000 + (ucontrol->value.integer.value[0] * 600),
				RAMP_0MS, mc->reg);
		abe->mm_dl1_volume = ucontrol->value.integer.value[0];
		return 1;
	case MIX_DL1_INPUT_VX_DL:
		abe_write_mixer(MIXDL1, -5000 + (ucontrol->value.integer.value[0] * 600),
				RAMP_0MS, mc->reg);
		abe->voice_dl1_volume = ucontrol->value.integer.value[0];
		return 1;
	case MIX_DL1_INPUT_TONES:
		abe_write_mixer(MIXDL1, -5000 + (ucontrol->value.integer.value[0] * 600),
				RAMP_0MS, mc->reg);
		abe->tones_dl1_volume = ucontrol->value.integer.value[0];
		return 1;
	case MIX_DL1_INPUT_MM_UL2:
		abe_write_mixer(MIXDL1, -5000 + (ucontrol->value.integer.value[0] * 600),
				RAMP_0MS, mc->reg);
		abe->capture_dl1_volume = ucontrol->value.integer.value[0];
		return 1;

	}

	return 0;
}

static int volume_put_dl2_mixer(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;

	switch(mc->reg) {
		case MIX_DL2_INPUT_MM_DL:
		abe_write_mixer(MIXDL2, -5000 + (ucontrol->value.integer.value[0] * 600),
				RAMP_0MS, mc->reg);
		abe->mm_dl2_volume = ucontrol->value.integer.value[0];
		return 1;
	case MIX_DL2_INPUT_VX_DL:
		abe_write_mixer(MIXDL2, -5000 + (ucontrol->value.integer.value[0] * 600),
				RAMP_0MS, mc->reg);
		abe->voice_dl2_volume = ucontrol->value.integer.value[0];
		return 1;
	case MIX_DL2_INPUT_TONES:
		abe_write_mixer(MIXDL2, -5000 + (ucontrol->value.integer.value[0] * 600),
				RAMP_0MS, mc->reg);
		abe->tones_dl2_volume = ucontrol->value.integer.value[0];
		return 1;
	case MIX_DL2_INPUT_MM_UL2:
		abe_write_mixer(MIXDL2, -5000 + (ucontrol->value.integer.value[0] * 600),
				RAMP_0MS, mc->reg);
		abe->capture_dl2_volume = ucontrol->value.integer.value[0];
		return 1;
	}
	return 0;
}

static int volume_get_dl1_mixer(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;

	switch (mc->reg) {
	case MIX_DL1_INPUT_MM_DL:
		ucontrol->value.integer.value[0] = abe->mm_dl1_volume;
		return 0;
	case MIX_DL1_INPUT_MM_UL2:
		ucontrol->value.integer.value[0] = abe->capture_dl1_volume;
		return 0;
	case MIX_DL1_INPUT_TONES:
		ucontrol->value.integer.value[0] = abe->tones_dl1_volume;
		return 0;
	case MIX_DL1_INPUT_VX_DL:
		ucontrol->value.integer.value[0] = abe->voice_dl1_volume;
		return 0;
	}

	return 1;
}

static int volume_get_dl2_mixer(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;

	switch (mc->reg) {
	case MIX_DL2_INPUT_MM_DL:
		ucontrol->value.integer.value[0] = abe->mm_dl2_volume;
		return 0;
	case MIX_DL2_INPUT_MM_UL2:
		ucontrol->value.integer.value[0] = abe->capture_dl2_volume;
		return 0;
	case MIX_DL2_INPUT_TONES:
		ucontrol->value.integer.value[0] = abe->tones_dl2_volume;
		return 0;
	case MIX_DL2_INPUT_VX_DL:
		ucontrol->value.integer.value[0] = abe->voice_dl2_volume;
		return 0;
	}

	return 1;
}

static const char *route_ul_texts[] =
	{"None", "DMic0L", "DMic0R", "DMic1L", "DMic1R", "DMic2L", "DMic2R",
	"BT Left", "BT Right", "AMic0", "AMic1", "VX Left", "Vx Right"};

/* ROUTE_UL Mux table */
static const struct soc_enum abe_enum[] = {
		SOC_ENUM_SINGLE(ABE_MM_UL1(0), 0, 13, route_ul_texts),
		SOC_ENUM_SINGLE(ABE_MM_UL1(1), 0, 13, route_ul_texts),
		SOC_ENUM_SINGLE(ABE_MM_UL1(2), 0, 13, route_ul_texts),
		SOC_ENUM_SINGLE(ABE_MM_UL1(3), 0, 13, route_ul_texts),
		SOC_ENUM_SINGLE(ABE_MM_UL1(4), 0, 13, route_ul_texts),
		SOC_ENUM_SINGLE(ABE_MM_UL1(5), 0, 13, route_ul_texts),
		SOC_ENUM_SINGLE(ABE_MM_UL1(6), 0, 13, route_ul_texts),
		SOC_ENUM_SINGLE(ABE_MM_UL1(7), 0, 13, route_ul_texts),
		SOC_ENUM_SINGLE(ABE_MM_UL2(0), 0, 13, route_ul_texts),
		SOC_ENUM_SINGLE(ABE_MM_UL2(1), 0, 13, route_ul_texts),
		SOC_ENUM_SINGLE(ABE_VX_UL(0), 0, 13, route_ul_texts),
		SOC_ENUM_SINGLE(ABE_VX_UL(1), 0, 13, route_ul_texts),
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
	SOC_SINGLE_EXT("Tones", MIX_DL2_INPUT_TONES, 0, 1, 0,
		abe_get_mixer, dl1_put_mixer),
	SOC_SINGLE_EXT("Voice", MIX_DL2_INPUT_VX_DL, 1, 1, 0,
		abe_get_mixer, dl1_put_mixer),
	SOC_SINGLE_EXT("Capture", MIX_DL1_INPUT_MM_UL2, 2, 1, 0,
		abe_get_mixer, dl1_put_mixer),
	SOC_SINGLE_EXT("Multimedia", MIX_DL1_INPUT_MM_DL, 3, 1, 0,
		abe_get_mixer, dl1_put_mixer),
};

/* DL2 mixer paths */
static const struct snd_kcontrol_new dl2_mixer_controls[] = {
	SOC_SINGLE_EXT("Tones", MIX_DL2_INPUT_TONES, 4, 1, 0,
		abe_get_mixer, dl2_put_mixer),
	SOC_SINGLE_EXT("Voice", MIX_DL2_INPUT_VX_DL, 5, 1, 0,
		abe_get_mixer, dl2_put_mixer),
	SOC_SINGLE_EXT("Capture", MIX_DL2_INPUT_MM_UL2, 6, 1, 0,
		abe_get_mixer, dl2_put_mixer),
	SOC_SINGLE_EXT("Multimedia", MIX_DL2_INPUT_MM_DL, 7, 1, 0,
		abe_get_mixer, dl2_put_mixer),
};

/* AUDUL ("Voice Capture Mixer") mixer paths */
static const struct snd_kcontrol_new audio_ul_mixer_controls[] = {
	SOC_SINGLE_EXT("Tones Playback", MIX_AUDUL_INPUT_TONES, 8, 1, 0,
		abe_get_mixer, audio_ul_put_mixer),
	SOC_SINGLE_EXT("Media Playback", MIX_AUDUL_INPUT_MM_DL, 9, 1, 0,
		abe_get_mixer, audio_ul_put_mixer),
	SOC_SINGLE_EXT("Capture", MIX_AUDUL_INPUT_UPLINK, 10, 1, 0,
		abe_get_mixer, audio_ul_put_mixer),
};

/* VXREC ("Capture Mixer")  mixer paths */
static const struct snd_kcontrol_new vx_rec_mixer_controls[] = {
	SOC_SINGLE_EXT("Tones", MIX_VXREC_INPUT_TONES, 11, 1, 0,
		abe_get_mixer, vxrec_put_mixer),
	SOC_SINGLE_EXT("Voice Playback", MIX_VXREC_INPUT_VX_DL, 12, 1, 0,
		abe_get_mixer, vxrec_put_mixer),
	SOC_SINGLE_EXT("Voice Capture", MIX_VXREC_INPUT_VX_UL, 13, 1, 0,
		abe_get_mixer, vxrec_put_mixer),
	SOC_SINGLE_EXT("Media Playback", MIX_VXREC_INPUT_MM_DL, 14, 1, 0,
		abe_get_mixer, vxrec_put_mixer),
};

/* SDT ("Sidetone Mixer") mixer paths */
static const struct snd_kcontrol_new sdt_mixer_controls[] = {
	SOC_SINGLE_EXT("Capture", MIX_SDT_INPUT_UP_MIXER, 15, 1, 0,
		abe_get_mixer, sdt_put_mixer),
	SOC_SINGLE_EXT("Playback", MIX_SDT_INPUT_DL1_MIXER, 16, 1, 0,
		abe_get_mixer, sdt_put_mixer),
};

/* Virtual PDM_DL Switch */
static const struct snd_kcontrol_new pdm_dl1_switch_controls =
	SOC_SINGLE_EXT("Switch", VIRT_SWITCH, 17, 1, 0,
			abe_get_mixer, abe_put_switch);

/* Virtual BT_VX_DL Switch */
static const struct snd_kcontrol_new bt_vx_dl_switch_controls =
	SOC_SINGLE_EXT("Switch", VIRT_SWITCH, 18, 1, 0,
			abe_get_mixer, abe_put_switch);

/* Virtual MM_EXT_DL Switch */
static const struct snd_kcontrol_new mm_ext_dl_switch_controls =
	SOC_SINGLE_EXT("Switch", VIRT_SWITCH, 19, 1, 0,
			abe_get_mixer, abe_put_switch);

/* Virtual MM_EXT_UL Switch */
static const struct snd_kcontrol_new mm_ext_ul_switch_controls =
	SOC_SINGLE_EXT("Switch", VIRT_SWITCH, 20, 1, 0,
			abe_get_mixer, abe_put_switch);

/* Virtual PDM_UL Switch */
static const struct snd_kcontrol_new pdm_ul1_switch_controls =
	SOC_SINGLE_EXT("Switch", VIRT_SWITCH, 21, 1, 0,
			abe_get_mixer, abe_put_switch);

static const struct snd_kcontrol_new abe_controls[] = {
	/* DL1 mixer gains */
	SOC_SINGLE_EXT_TLV("DL1 Media Playback Volume",
		MIX_DL1_INPUT_MM_DL, 0, 14, 0,
		volume_get_dl1_mixer, volume_put_dl1_mixer, mm_dl1_tlv),
	SOC_SINGLE_EXT_TLV("DL1 Tones Playback Volume",
		MIX_DL1_INPUT_TONES, 0, 14, 0,
		volume_get_dl1_mixer, volume_put_dl1_mixer, tones_dl1_tlv),
	SOC_SINGLE_EXT_TLV("DL1 Voice Playback Volume",
		MIX_DL1_INPUT_VX_DL, 0, 14, 0,
		volume_get_dl1_mixer, volume_put_dl1_mixer, voice_dl1_tlv),
	SOC_SINGLE_EXT_TLV("DL1 Capture Playback Volume",
		MIX_DL1_INPUT_MM_UL2, 0, 14, 0,
		volume_get_dl1_mixer, volume_put_dl1_mixer, capture_dl1_tlv),

	/* DL2 mixer gains */
	SOC_SINGLE_EXT_TLV("DL2 Media Playback Volume",
		MIX_DL2_INPUT_MM_DL, 0, 14, 0,
		volume_get_dl2_mixer, volume_put_dl2_mixer, mm_dl2_tlv),
	SOC_SINGLE_EXT_TLV("DL2 Tones Playback Volume",
		MIX_DL2_INPUT_TONES, 0, 14, 0,
		volume_get_dl2_mixer, volume_put_dl2_mixer, tones_dl2_tlv),
	SOC_SINGLE_EXT_TLV("DL2 Voice Playback Volume",
		MIX_DL2_INPUT_VX_DL, 0, 14, 0,
		volume_get_dl2_mixer, volume_put_dl2_mixer, voice_dl2_tlv),
	SOC_SINGLE_EXT_TLV("DL2 Capture Playback Volume",
		MIX_DL2_INPUT_MM_UL2, 0, 14, 0,
		volume_get_dl2_mixer, volume_put_dl2_mixer, capture_dl2_tlv),
};

static const struct snd_soc_dapm_widget abe_dapm_widgets[] = {

	/* Frontend AIFs */
	SND_SOC_DAPM_AIF_IN("TONES_DL", "Tones Playback", 0,
			ABE_WIDGET(0), ABE_OPP_25, 0),
	SND_SOC_DAPM_AIF_IN("VX_DL", "Voice Playback", 0,
			ABE_WIDGET(1), ABE_OPP_50, 0),
	SND_SOC_DAPM_AIF_OUT("VX_UL", "Voice Capture", 0,
			ABE_WIDGET(2), ABE_OPP_50, 0),
	SND_SOC_DAPM_AIF_OUT("MM_UL1", "MultiMedia1 Capture", 0,
			ABE_WIDGET(3), ABE_OPP_50, 0),
	SND_SOC_DAPM_AIF_OUT("MM_UL2", "MultiMedia2 Capture", 0,
			ABE_WIDGET(4), ABE_OPP_50, 0),
	SND_SOC_DAPM_AIF_IN("MM_DL", " MultiMedia1 Playback", 0,
			ABE_WIDGET(5), ABE_OPP_25, 0),
	SND_SOC_DAPM_AIF_IN("VIB_DL", "Vibra Playback", 0,
			ABE_WIDGET(6), ABE_OPP_100, 0),

	/* Backend DAIs  - TODO: maybe should match stream names of TWL6040 etc ?? */
	// FIXME: must match BE order in abe_dai.h
	SND_SOC_DAPM_AIF_IN("PDM_UL1", "Analog Capture", 0,
			ABE_WIDGET(7), ABE_OPP_50, 0),
	SND_SOC_DAPM_AIF_OUT("PDM_DL1", "HS Playback", 0,
			ABE_WIDGET(8), ABE_OPP_25, 0),
	SND_SOC_DAPM_AIF_OUT("PDM_DL2", "HF Playback", 0,
			ABE_WIDGET(9), ABE_OPP_100, 0),
	SND_SOC_DAPM_AIF_OUT("PDM_VIB", "Vibra Playback", 0,
			ABE_WIDGET(10), ABE_OPP_100, 0),
	SND_SOC_DAPM_AIF_IN("BT_VX_UL", "Capture", 0,
			ABE_WIDGET(11), ABE_OPP_50, 0),
	SND_SOC_DAPM_AIF_OUT("BT_VX_DL", "Playback", 0,
			ABE_WIDGET(12), ABE_OPP_50, 0),
	SND_SOC_DAPM_AIF_IN("MM_EXT_UL", "Capture", 0,
			ABE_WIDGET(13), ABE_OPP_50, 0),
	SND_SOC_DAPM_AIF_OUT("MM_EXT_DL", "Playback", 0,
			ABE_WIDGET(14), ABE_OPP_25, 0),
	SND_SOC_DAPM_AIF_IN("DMIC0", "Capture", 0,
			ABE_WIDGET(15), ABE_OPP_50, 0),
	SND_SOC_DAPM_AIF_IN("DMIC1", "Capture", 0,
			ABE_WIDGET(16), ABE_OPP_50, 0),
	SND_SOC_DAPM_AIF_IN("DMIC2", "Capture", 0,
			ABE_WIDGET(17), ABE_OPP_50, 0),

	/* ROUTE_UL Capture Muxes */
	SND_SOC_DAPM_MUX("MUX_UL00",
			ABE_WIDGET(18), ABE_OPP_50, 0, &mm_ul00_control),
	SND_SOC_DAPM_MUX("MUX_UL01",
			ABE_WIDGET(19), ABE_OPP_50, 0, &mm_ul01_control),
	SND_SOC_DAPM_MUX("MUX_UL02",
			ABE_WIDGET(20), ABE_OPP_50, 0, &mm_ul02_control),
	SND_SOC_DAPM_MUX("MUX_UL03",
			ABE_WIDGET(21), ABE_OPP_50, 0, &mm_ul03_control),
	SND_SOC_DAPM_MUX("MUX_UL04",
			ABE_WIDGET(22), ABE_OPP_50, 0, &mm_ul04_control),
	SND_SOC_DAPM_MUX("MUX_UL05",
			ABE_WIDGET(23), ABE_OPP_50, 0, &mm_ul05_control),
	SND_SOC_DAPM_MUX("MUX_UL06",
			ABE_WIDGET(24), ABE_OPP_50, 0, &mm_ul06_control),
	SND_SOC_DAPM_MUX("MUX_UL07",
			ABE_WIDGET(25), ABE_OPP_50, 0, &mm_ul07_control),
	SND_SOC_DAPM_MUX("MUX_UL10",
			ABE_WIDGET(26), ABE_OPP_50, 0, &mm_ul10_control),
	SND_SOC_DAPM_MUX("MUX_UL11",
			ABE_WIDGET(27), ABE_OPP_50, 0, &mm_ul11_control),
	SND_SOC_DAPM_MUX("MUX_VX0",
			ABE_WIDGET(28), ABE_OPP_50, 0, &mm_vx0_control),
	SND_SOC_DAPM_MUX("MUX_VX1",
			ABE_WIDGET(29), ABE_OPP_50, 0, &mm_vx1_control),

	/* DL1 & DL2 Playback Mixers */
	SND_SOC_DAPM_MIXER("DL1 Mixer",
			ABE_WIDGET(30), ABE_OPP_25, 0, dl1_mixer_controls,
			ARRAY_SIZE(dl1_mixer_controls)),
	SND_SOC_DAPM_MIXER("DL2 Mixer",
			ABE_WIDGET(31), ABE_OPP_100, 0, dl2_mixer_controls,
			ARRAY_SIZE(dl2_mixer_controls)),

	/* DL1 Mixer Input volumes ?????*/
	SND_SOC_DAPM_PGA("DL1 Media Volume",
			ABE_WIDGET(32), 0, 0, NULL, 0),

	/* AUDIO_UL_MIXER */
	SND_SOC_DAPM_MIXER("Voice Capture Mixer",
			ABE_WIDGET(33), ABE_OPP_50, 0, audio_ul_mixer_controls,
			ARRAY_SIZE(audio_ul_mixer_controls)),

	/* VX_REC_MIXER */
	SND_SOC_DAPM_MIXER("Capture Mixer",
			ABE_WIDGET(34), ABE_OPP_50, 0, vx_rec_mixer_controls,
			ARRAY_SIZE(vx_rec_mixer_controls)),

	/* SDT_MIXER  - TODO: shoult this not be OPP25 ??? */
	SND_SOC_DAPM_MIXER("Sidetone Mixer",
			ABE_WIDGET(35), ABE_OPP_50, 0, sdt_mixer_controls,
			ARRAY_SIZE(sdt_mixer_controls)),

	/* Virtual PDM_DL1 Switch */
	SND_SOC_DAPM_MIXER("PDM_DL Playback",
			ABE_WIDGET(36), ABE_OPP_25, 0, &pdm_dl1_switch_controls, 1),

	/* Virtual BT_VX_DL Switch */
	SND_SOC_DAPM_MIXER("BT_VX_DL Playback",
			ABE_WIDGET(37), ABE_OPP_50, 0, &bt_vx_dl_switch_controls, 1),

	/* Virtual MM_EXT_DL Switch TODO: confrm OPP level here */
	SND_SOC_DAPM_MIXER("MM_EXT_DL Playback",
			ABE_WIDGET(38), ABE_OPP_50, 0, &mm_ext_dl_switch_controls, 1),

	/* Virtual MM_EXT_UL Switch */
	SND_SOC_DAPM_MIXER("MM_EXT_UL Capture",
			ABE_WIDGET(39), ABE_OPP_50, 0, &mm_ext_ul_switch_controls, 1),

	/* Virtual PDM_UL1 Switch */
	SND_SOC_DAPM_MIXER("PDM_UL Capture",
			ABE_WIDGET(40), ABE_OPP_50, 0, &pdm_ul1_switch_controls, 1),

	/* Virtual Pins to force backends ON atm */
	SND_SOC_DAPM_OUTPUT("BE_OUT"),
	SND_SOC_DAPM_INPUT("BE_IN"),
};

static const struct snd_soc_dapm_route intercon[] = {

	/* MUX_UL00 - ROUTE_UL */
	{"MM_UL1", "DMic0L", "MUX_UL00"},
	{"MM_UL1", "DMic0R", "MUX_UL00"},
	{"MM_UL1", "DMic1L", "MUX_UL00"},
	{"MM_UL1", "DMic1R", "MUX_UL00"},
	{"MM_UL1", "DMic2L", "MUX_UL00"},
	{"MM_UL1", "DMic2R", "MUX_UL00"},
	{"MM_UL1", "BT Left", "MUX_UL00"},
	{"MM_UL1", "BT Right", "MUX_UL00"},
	{"MM_UL1", "AMic0", "MUX_UL00"},
	{"MM_UL1", "AMic1", "MUX_UL00"},
	{"MM_UL1", "VX Left", "MUX_UL00"},
	{"MM_UL1", "VX Right", "MUX_UL00"},

	/* MUX_UL01 - ROUTE_UL */
	{"MM_UL1", "DMic0L", "MUX_UL01"},
	{"MM_UL1", "DMic0R", "MUX_UL01"},
	{"MM_UL1", "DMic1L", "MUX_UL01"},
	{"MM_UL1", "DMic1R", "MUX_UL01"},
	{"MM_UL1", "DMic2L", "MUX_UL01"},
	{"MM_UL1", "DMic2R", "MUX_UL01"},
	{"MM_UL1", "BT Left", "MUX_UL01"},
	{"MM_UL1", "BT Right", "MUX_UL01"},
	{"MM_UL1", "AMic0", "MUX_UL01"},
	{"MM_UL1", "AMic1", "MUX_UL01"},
	{"MM_UL1", "VX Left", "MUX_UL01"},
	{"MM_UL1", "VX Right", "MUX_UL01"},

	/* MUX_UL02 - ROUTE_UL */
	{"MM_UL1", "DMic0L", "MUX_UL02"},
	{"MM_UL1", "DMic0R", "MUX_UL02"},
	{"MM_UL1", "DMic1L", "MUX_UL02"},
	{"MM_UL1", "DMic1R", "MUX_UL02"},
	{"MM_UL1", "DMic2L", "MUX_UL02"},
	{"MM_UL1", "DMic2R", "MUX_UL02"},
	{"MM_UL1", "BT Left", "MUX_UL02"},
	{"MM_UL1", "BT Right", "MUX_UL02"},
	{"MM_UL1", "AMic0", "MUX_UL02"},
	{"MM_UL1", "AMic1", "MUX_UL02"},
	{"MM_UL1", "VX Left", "MUX_UL02"},
	{"MM_UL1", "VX Right", "MUX_UL02"},

	/* MUX_UL03 - ROUTE_UL */
	{"MM_UL1", "DMic0L", "MUX_UL03"},
	{"MM_UL1", "DMic0R", "MUX_UL03"},
	{"MM_UL1", "DMic1L", "MUX_UL03"},
	{"MM_UL1", "DMic1R", "MUX_UL03"},
	{"MM_UL1", "DMic2L", "MUX_UL03"},
	{"MM_UL1", "DMic2R", "MUX_UL03"},
	{"MM_UL1", "BT Left", "MUX_UL03"},
	{"MM_UL1", "BT Right", "MUX_UL03"},
	{"MM_UL1", "AMic0", "MUX_UL03"},
	{"MM_UL1", "AMic1", "MUX_UL03"},
	{"MM_UL1", "VX Left", "MUX_UL03"},
	{"MM_UL1", "VX Right", "MUX_UL03"},

	/* MUX_UL04 - ROUTE_UL */
	{"MM_UL1", "DMic0L", "MUX_UL04"},
	{"MM_UL1", "DMic0R", "MUX_UL04"},
	{"MM_UL1", "DMic1L", "MUX_UL04"},
	{"MM_UL1", "DMic1R", "MUX_UL04"},
	{"MM_UL1", "DMic2L", "MUX_UL04"},
	{"MM_UL1", "DMic2R", "MUX_UL04"},
	{"MM_UL1", "BT Left", "MUX_UL04"},
	{"MM_UL1", "BT Right", "MUX_UL04"},
	{"MM_UL1", "AMic0", "MUX_UL04"},
	{"MM_UL1", "AMic1", "MUX_UL04"},
	{"MM_UL1", "VX Left", "MUX_UL04"},
	{"MM_UL1", "VX Right", "MUX_UL04"},

	/* MUX_UL05 - ROUTE_UL */
	{"MM_UL1", "DMic0L", "MUX_UL05"},
	{"MM_UL1", "DMic0R", "MUX_UL05"},
	{"MM_UL1", "DMic1L", "MUX_UL05"},
	{"MM_UL1", "DMic1R", "MUX_UL05"},
	{"MM_UL1", "DMic2L", "MUX_UL05"},
	{"MM_UL1", "DMic2R", "MUX_UL05"},
	{"MM_UL1", "BT Left", "MUX_UL05"},
	{"MM_UL1", "BT Right", "MUX_UL05"},
	{"MM_UL1", "AMic0", "MUX_UL05"},
	{"MM_UL1", "AMic1", "MUX_UL05"},
	{"MM_UL1", "VX Left", "MUX_UL05"},
	{"MM_UL1", "VX Right", "MUX_UL05"},

	/* MUX_UL06 - ROUTE_UL */
	{"MM_UL1", "DMic0L", "MUX_UL06"},
	{"MM_UL1", "DMic0R", "MUX_UL06"},
	{"MM_UL1", "DMic1L", "MUX_UL06"},
	{"MM_UL1", "DMic1R", "MUX_UL06"},
	{"MM_UL1", "DMic2L", "MUX_UL06"},
	{"MM_UL1", "DMic2R", "MUX_UL06"},
	{"MM_UL1", "BT Left", "MUX_UL06"},
	{"MM_UL1", "BT Right", "MUX_UL06"},
	{"MM_UL1", "AMic0", "MUX_UL06"},
	{"MM_UL1", "AMic1", "MUX_UL06"},
	{"MM_UL1", "VX Left", "MUX_UL06"},
	{"MM_UL1", "VX Right", "MUX_UL06"},

	/* MUX_UL07 - ROUTE_UL */
	{"MM_UL1", "DMic0L", "MUX_UL07"},
	{"MM_UL1", "DMic0R", "MUX_UL07"},
	{"MM_UL1", "DMic1L", "MUX_UL07"},
	{"MM_UL1", "DMic1R", "MUX_UL07"},
	{"MM_UL1", "DMic2L", "MUX_UL07"},
	{"MM_UL1", "DMic2R", "MUX_UL07"},
	{"MM_UL1", "BT Left", "MUX_UL07"},
	{"MM_UL1", "BT Right", "MUX_UL07"},
	{"MM_UL1", "AMic0", "MUX_UL07"},
	{"MM_UL1", "AMic1", "MUX_UL07"},
	{"MM_UL1", "VX Left", "MUX_UL07"},
	{"MM_UL1", "VX Right", "MUX_UL07"},

	/* MUX_UL10 - ROUTE_UL */
	{"MM_UL1", "DMic0L", "MUX_UL10"},
	{"MM_UL1", "DMic0R", "MUX_UL10"},
	{"MM_UL1", "DMic1L", "MUX_UL10"},
	{"MM_UL1", "DMic1R", "MUX_UL10"},
	{"MM_UL1", "DMic2L", "MUX_UL10"},
	{"MM_UL1", "DMic2R", "MUX_UL10"},
	{"MM_UL1", "BT Left", "MUX_UL10"},
	{"MM_UL1", "BT Right", "MUX_UL10"},
	{"MM_UL1", "AMic0", "MUX_UL10"},
	{"MM_UL1", "AMic1", "MUX_UL10"},
	{"MM_UL1", "VX Left", "MUX_UL10"},
	{"MM_UL1", "VX Right", "MUX_UL10"},

	/* MUX_UL11 - ROUTE_UL */
	{"MM_UL1", "DMic0L", "MUX_UL11"},
	{"MM_UL1", "DMic0R", "MUX_UL11"},
	{"MM_UL1", "DMic1L", "MUX_UL11"},
	{"MM_UL1", "DMic1R", "MUX_UL11"},
	{"MM_UL1", "DMic2L", "MUX_UL11"},
	{"MM_UL1", "DMic2R", "MUX_UL11"},
	{"MM_UL1", "BT Left", "MUX_UL11"},
	{"MM_UL1", "BT Right", "MUX_UL11"},
	{"MM_UL1", "AMic0", "MUX_UL11"},
	{"MM_UL1", "AMic1", "MUX_UL11"},
	{"MM_UL1", "VX Left", "MUX_UL11"},
	{"MM_UL1", "VX Right", "MUX_UL11"},

	/* MUX_VX0 - ROUTE_UL */
	{"VX_UL", "DMic0L", "MUX_VX0"},
	{"VX_UL", "DMic0R", "MUX_VX0"},
	{"VX_UL", "DMic1L", "MUX_VX0"},
	{"VX_UL", "DMic1R", "MUX_VX0"},
	{"VX_UL", "DMic2L", "MUX_VX0"},
	{"VX_UL", "DMic2R", "MUX_VX0"},
	{"VX_UL", "BT Left", "MUX_VX0"},
	{"VX_UL", "BT Right", "MUX_VX0"},
	{"VX_UL", "AMic0", "MUX_VX0"},
	{"VX_UL", "AMic1", "MUX_VX0"},
	{"VX_UL", "VX Left", "MUX_VX0"},
	{"VX_UL", "VX Right", "MUX_VX0"},

	/* MUX_VX1 - ROUTE_UL */
	{"VX_UL", "DMic0L", "MUX_VX1"},
	{"VX_UL", "DMic0R", "MUX_VX1"},
	{"VX_UL", "DMic1L", "MUX_VX1"},
	{"VX_UL", "DMic1R", "MUX_VX1"},
	{"VX_UL", "DMic2L", "MUX_VX1"},
	{"VX_UL", "DMic2R", "MUX_VX1"},
	{"VX_UL", "BT Left", "MUX_VX1"},
	{"VX_UL", "BT Right", "MUX_VX1"},
	{"VX_UL", "AMic0", "MUX_VX1"},
	{"VX_UL", "AMic1", "MUX_VX1"},
	{"VX_UL", "VX Left", "MUX_VX1"},
	{"VX_UL", "VX Right", "MUX_VX1"},

	/* AMIC_UL */
	{"MM_EXT_UL Capture", "Switch", "MM_EXT_UL"},
	{"MUX_UL00", NULL, "MM_EXT_UL Capture"},
	{"MUX_UL01", NULL, "MM_EXT_UL Capture"},
	{"MUX_UL02", NULL, "MM_EXT_UL Capture"},
	{"MUX_UL03", NULL, "MM_EXT_UL Capture"},
	{"MUX_UL04", NULL, "MM_EXT_UL Capture"},
	{"MUX_UL05", NULL, "MM_EXT_UL Capture"},
	{"MUX_UL06", NULL, "MM_EXT_UL Capture"},
	{"MUX_UL07", NULL, "MM_EXT_UL Capture"},
	{"MUX_UL10", NULL, "MM_EXT_UL Capture"},
	{"MUX_UL11", NULL, "MM_EXT_UL Capture"},
	{"MUX_VX0", NULL, "MM_EXT_UL Capture"},
	{"MUX_VX1", NULL, "MM_EXT_UL Capture"},

	/* BT_VX_UL */
	{"MUX_UL00", NULL, "BT_VX_UL"},
	{"MUX_UL01", NULL, "BT_VX_UL"},
	{"MUX_UL02", NULL, "BT_VX_UL"},
	{"MUX_UL03", NULL, "BT_VX_UL"},
	{"MUX_UL04", NULL, "BT_VX_UL"},
	{"MUX_UL05", NULL, "BT_VX_UL"},
	{"MUX_UL06", NULL, "BT_VX_UL"},
	{"MUX_UL07", NULL, "BT_VX_UL"},
	{"MUX_UL10", NULL, "BT_VX_UL"},
	{"MUX_UL11", NULL, "BT_VX_UL"},
	{"MUX_VX0", NULL, "BT_VX_UL"},
	{"MUX_VX1", NULL, "BT_VX_UL"},

	/* DMIC0 */
	{"MUX_UL00", NULL, "DMIC0"},
	{"MUX_UL01", NULL, "DMIC0"},
	{"MUX_UL02", NULL, "DMIC0"},
	{"MUX_UL03", NULL, "DMIC0"},
	{"MUX_UL04", NULL, "DMIC0"},
	{"MUX_UL05", NULL, "DMIC0"},
	{"MUX_UL06", NULL, "DMIC0"},
	{"MUX_UL07", NULL, "DMIC0"},
	{"MUX_UL10", NULL, "DMIC0"},
	{"MUX_UL11", NULL, "DMIC0"},
	{"MUX_VX0", NULL, "DMIC0"},
	{"MUX_VX1", NULL, "DMIC0"},

	/* DMIC1 */
	{"MUX_UL00", NULL, "DMIC1"},
	{"MUX_UL01", NULL, "DMIC1"},
	{"MUX_UL02", NULL, "DMIC1"},
	{"MUX_UL03", NULL, "DMIC1"},
	{"MUX_UL04", NULL, "DMIC1"},
	{"MUX_UL05", NULL, "DMIC1"},
	{"MUX_UL06", NULL, "DMIC1"},
	{"MUX_UL07", NULL, "DMIC1"},
	{"MUX_UL10", NULL, "DMIC1"},
	{"MUX_UL11", NULL, "DMIC1"},
	{"MUX_VX0", NULL, "DMIC1"},
	{"MUX_VX1", NULL, "DMIC1"},

	/* DMIC2 */
	{"MUX_UL00", NULL, "DMIC2"},
	{"MUX_UL01", NULL, "DMIC2"},
	{"MUX_UL02", NULL, "DMIC2"},
	{"MUX_UL03", NULL, "DMIC2"},
	{"MUX_UL04", NULL, "DMIC2"},
	{"MUX_UL05", NULL, "DMIC2"},
	{"MUX_UL06", NULL, "DMIC2"},
	{"MUX_UL07", NULL, "DMIC2"},
	{"MUX_UL10", NULL, "DMIC2"},
	{"MUX_UL11", NULL, "DMIC2"},
	{"MUX_VX0", NULL, "DMIC2"},
	{"MUX_VX1", NULL, "DMIC2"},

	/* Headset (DL1)  playback path */
	{"DL1 Mixer", "Tones", "TONES_DL"},
	{"DL1 Mixer", "Voice", "VX_DL"},
	{"DL1 Mixer", "Capture", "MUX_UL10"},
	{"DL1 Mixer", "Multimedia", "MM_DL"},

	/* Sidetone Mixer */
	{"Sidetone Mixer", "Playback", "DL1 Mixer"},
	{"Sidetone Mixer", "Capture", "MUX_VX0"},

	{"BT_VX_DL Playback", "Switch", "Sidetone Mixer"},
	{"MM_EXT_DL Playback", "Switch", "Sidetone Mixer"},
	{"PDM_DL Playback", "Switch", "Sidetone Mixer"},

	{"PDM_DL1", NULL, "PDM_DL Playback"},
	{"BT_VX_DL", NULL, "BT_VX_DL Playback"},
	{"MM_EXT_DL", NULL, "MM_EXT_DL Playback"},

	/* Handsfree (DL2) playback path */
	{"DL2 Mixer", "Tones", "TONES_DL"},
	{"DL2 Mixer", "Voice", "VX_DL"},
	{"DL2 Mixer", "Capture", "MUX_UL10"},
	{"DL2 Mixer", "Multimedia", "MM_DL"},

	{"PDM_DL2", NULL, "DL2 Mixer"},

	/* VxREC Mixer */
	{"Capture Mixer", "Tones", "TONES_DL"},
	{"Capture Mixer", "Voice Playback", "VX_DL"},
	{"Capture Mixer", "Voice Capture", "VX_UL"},
	{"Capture Mixer", "Media Playback", "MM_DL"},
	{"MUX_UL00", NULL, "Capture Mixer"},
	{"MUX_UL01", NULL, "Capture Mixer"},
	{"MUX_UL02", NULL, "Capture Mixer"},
	{"MUX_UL03", NULL, "Capture Mixer"},
	{"MUX_UL04", NULL, "Capture Mixer"},
	{"MUX_UL05", NULL, "Capture Mixer"},
	{"MUX_UL06", NULL, "Capture Mixer"},
	{"MUX_UL07", NULL, "Capture Mixer"},
	{"MUX_UL10", NULL, "Capture Mixer"},
	{"MUX_UL11", NULL, "Capture Mixer"},
	{"MUX_VX0", NULL, "Capture Mixer"},
	{"MUX_VX1", NULL, "Capture Mixer"},

	/* Audio UL mixer */
	{"Voice Capture Mixer", "Tones Playback", "TONES_DL"},
	{"Voice Capture Mixer", "Media Playback", "MM_DL"},
	{"Voice Capture Mixer", "Capture", "MUX_VX0"},

	{"VX_UL", NULL, "Voice Capture Mixer"},

	/* Vibra */
	{"PDM_VIB", NULL, "VIB_DL"},

	/* Backend Enablement - TODO: maybe re-work*/
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

static int abe_add_widgets(struct snd_soc_platform *platform)
{
	snd_soc_add_platform_controls(platform, abe_controls,
				ARRAY_SIZE(abe_controls));

	snd_soc_dapm_new_controls(platform->dapm, abe_dapm_widgets,
				 ARRAY_SIZE(abe_dapm_widgets));

	snd_soc_dapm_add_routes(platform->dapm, intercon, ARRAY_SIZE(intercon));

	snd_soc_dapm_new_widgets(platform->dapm);

	return 0;
}

static int abe_probe(struct snd_soc_platform *platform)
{
	abe_init_engine(platform);
	abe_add_widgets(platform);
	abe->platform = platform;
	return 0;
}

static int  abe_remove(struct snd_soc_platform *platform)
{
	return 0;
}

static int aess_open(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *dai = rtd->cpu_dai;

	mutex_lock(&abe->mutex);

	abe->fe_id = dai->id;

	switch (dai->id) {
	case ABE_FRONTEND_DAI_MEDIA:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			snd_soc_dapm_stream_event(rtd, "Multimedia Playback",
					SND_SOC_DAPM_STREAM_START);
		else
			snd_soc_dapm_stream_event(rtd, "Multimedia Capture2",
					SND_SOC_DAPM_STREAM_START);
		break;
	case ABE_FRONTEND_DAI_MEDIA_CAPTURE:
		snd_soc_dapm_stream_event(rtd, "Multimedia Capture1",
				SND_SOC_DAPM_STREAM_START);
		break;
	case ABE_FRONTEND_DAI_VOICE:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			snd_soc_dapm_stream_event(rtd, "Voice Playback",
					SND_SOC_DAPM_STREAM_START);
		else
			snd_soc_dapm_stream_event(rtd, "Voice Capture",
					SND_SOC_DAPM_STREAM_START);
		break;
	case ABE_FRONTEND_DAI_TONES:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			snd_soc_dapm_stream_event(rtd, "Tones Playback",
					SND_SOC_DAPM_STREAM_START);
		break;
	case ABE_FRONTEND_DAI_VIBRA:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			snd_soc_dapm_stream_event(rtd, "Vibra Playback",
					SND_SOC_DAPM_STREAM_START);
		break;
	}

	mutex_unlock(&abe->mutex);
	return 0;
}

static int aess_prepare(struct snd_pcm_substream *substream)
{
	int i, opp = 0;

	mutex_lock(&abe->mutex);

	/* now calculate OPP level based upon DAPM widget status */
	for (i = ABE_WIDGET_START; i < ABE_WIDGET_END; i++)
		opp |= abe->dapm[i];
	printk("OPP level at start is %d\n", (1 << (fls(opp) - 1)) * 25);

	mutex_unlock(&abe->mutex);
	return 0;
}

static int aess_close(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime  *rtd = substream->private_data;
	struct snd_soc_dai *dai = rtd->cpu_dai;
	int i, opp = 0;

	mutex_lock(&abe->mutex);

	abe->fe_id = dai->id;

	switch (dai->id) {
	case ABE_FRONTEND_DAI_MEDIA:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			snd_soc_dapm_stream_event(rtd, "Multimedia Playback",
					SND_SOC_DAPM_STREAM_STOP);
		else
			snd_soc_dapm_stream_event(rtd, "Multimedia Capture2",
					SND_SOC_DAPM_STREAM_STOP);
		break;
	case ABE_FRONTEND_DAI_MEDIA_CAPTURE:
		snd_soc_dapm_stream_event(rtd, "Multimedia Capture1",
				SND_SOC_DAPM_STREAM_STOP);
		break;
	case ABE_FRONTEND_DAI_VOICE:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			snd_soc_dapm_stream_event(rtd, "Voice Playback",
					SND_SOC_DAPM_STREAM_STOP);
		else
			snd_soc_dapm_stream_event(rtd, "Voice Capture",
					SND_SOC_DAPM_STREAM_STOP);
		break;
	case ABE_FRONTEND_DAI_TONES:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			snd_soc_dapm_stream_event(rtd, "Tones Playback",
					SND_SOC_DAPM_STREAM_STOP);
		break;
	case ABE_FRONTEND_DAI_VIBRA:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			snd_soc_dapm_stream_event(rtd, "Vibra Playback",
					SND_SOC_DAPM_STREAM_STOP);
		break;
	}

	/* now calculate OPP level based upon DAPM widget status */
	for (i = ABE_WIDGET_START; i < ABE_WIDGET_END; i++)
		opp |= abe->dapm[i];
	printk("OPP level at close is %d\n", (1 << (fls(opp) - 1)) * 25);

	mutex_unlock(&abe->mutex);
	return 0;
}

struct omap_aess_fe *omap_aess_get_be_status(int fe)
{
	return &abe->fe[fe];
}
EXPORT_SYMBOL_GPL(omap_aess_get_be_status);

static struct snd_pcm_ops omap_aess_pcm_ops = {
	.open = aess_open,
	.prepare	= aess_prepare,
	.close	= aess_close,
};

static struct snd_soc_platform_driver omap_aess_platform = {
	.ops	= &omap_aess_pcm_ops,
	.probe = abe_probe,
	.remove = abe_remove,
	.read = abe_dsp_read,
	.write = abe_dsp_write,
};

static struct omap_device_pm_latency omap_aess_latency[] = {
	{
		.deactivate_func = omap_device_idle_hwmods,
		.activate_func = omap_device_enable_hwmods,
		.flags = OMAP_DEVICE_LATENCY_AUTO_ADJUST,
	},
};

static int __devinit abe_engine_probe(struct platform_device *pdev)
{
	struct omap4_abe_dsp_pdata *pdata = dev_get_platdata(&pdev->dev);
	struct omap_hwmod *oh;
	struct omap_device *od;
	int ret;

	abe = kzalloc(sizeof(struct abe_data), GFP_KERNEL);
	if (abe == NULL)
		return -ENOMEM;
	dev_set_drvdata(&pdev->dev, abe);

	pm_runtime_enable(&pdev->dev);

	abe->abe_pdata = pdev->dev.platform_data;
	abe->pdev = pdev;

	mutex_init(&abe->mutex);

	ret = snd_soc_register_platform(&pdev->dev,
			&omap_aess_platform);
	if (ret == 0)
		return 0;
#if 0
err:
#endif
	kfree(abe);
	return ret;
}

static int __devexit abe_engine_remove(struct platform_device *pdev)
{
	struct abe_data *priv = dev_get_drvdata(&pdev->dev);

	snd_soc_unregister_platform(&pdev->dev);

	kfree(priv);
	return 0;
}

static struct platform_driver omap_aess_driver = {
	.driver = {
		.name = "omap-dsp-audio",
		.owner = THIS_MODULE,
	},
	.probe = abe_engine_probe,
	.remove = __devexit_p(abe_engine_remove),
};

static int __init abe_engine_init(void)
{
	return platform_driver_register(&omap_aess_driver);
}
module_init(abe_engine_init);

static void __exit abe_engine_exit(void)
{
	platform_driver_unregister(&omap_aess_driver);
}
module_exit(abe_engine_exit);

MODULE_DESCRIPTION("ASoC OMAP4 ABE");
MODULE_AUTHOR("Liam Girdwood <lrg@slimlogic.co.uk>");
MODULE_LICENSE("GPL");
