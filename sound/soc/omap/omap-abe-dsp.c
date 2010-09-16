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

#undef DEBUG

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
#include <linux/dma-mapping.h>

#include <plat/omap_hwmod.h>
#include <plat/omap_device.h>
#include <plat/dma.h>

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

// TODO: make all these into virtual registers or similar - split out by type */
#define ABE_NUM_MIXERS		22
#define ABE_NUM_MUXES		12
#define ABE_NUM_WIDGETS	44	/* TODO - refine this val */
#define ABE_NUM_DAPM_REG		\
	(ABE_NUM_MIXERS + ABE_NUM_MUXES + ABE_NUM_WIDGETS)
#define ABE_WIDGET_START	(ABE_NUM_MIXERS + ABE_NUM_MUXES)
#define ABE_WIDGET_END	(ABE_WIDGET_START + ABE_NUM_WIDGETS)
#define ABE_BE_START		(ABE_WIDGET_START + 7)
#define ABE_BE_END	(ABE_BE_START + 10)
#define ABE_MUX_BASE	ABE_NUM_MIXERS

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

#define ABE_ROUTES_UL		14

/* TODO: fine tune for ping pong - buffer is 2 periods of 12k each*/
static const struct snd_pcm_hardware omap_abe_hardware = {
	.info			= SNDRV_PCM_INFO_MMAP |
				  SNDRV_PCM_INFO_MMAP_VALID |
				  SNDRV_PCM_INFO_INTERLEAVED |
				  SNDRV_PCM_INFO_PAUSE |
				  SNDRV_PCM_INFO_RESUME,
	.formats		= SNDRV_PCM_FMTBIT_S16_LE |
				  SNDRV_PCM_FMTBIT_S32_LE,
	.period_bytes_min	= 12 * 1024,
	.period_bytes_max	= 12 * 1024,
	.periods_min		= 2,
	.periods_max		= 2,
	.buffer_bytes_max	= 12 * 1024 * 2,
};

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

	struct clk *clk;

	void __iomem *io_base;
	int irq;

	int fe_id;

	/* DAPM mixer config - TODO: some of this can be replaced with HAL update */
	u32 dapm[ABE_NUM_DAPM_REG];

	u16 router[16];
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

	//dev_dbg(platform->dev, "fe: %d widget %d %s\n", abe->fe_id,
	//		reg - ABE_WIDGET_START, val ? "on" : "off");
	return 0;
}

static irqreturn_t abe_irq_handler(int irq, void *dev_id)
{
	/* TODO: handle underruns/overruns/errors */
	return IRQ_HANDLED;
}

static int abe_init_engine(struct snd_soc_platform *platform)
{
	struct abe_data *priv = snd_soc_platform_get_drvdata(platform);
#ifndef CONFIG_PM_RUNTIME
	struct omap4_abe_dsp_pdata *pdata = priv->abe_pdata;
#endif
	struct platform_device *pdev = priv->pdev;
	abe_equ_t dl2_eq;
	int ret = 0;

	abe_init_mem(abe->io_base);

	/* aess_clk has to be enabled to access hal register.
	 * Disable the clk after it has been used.
	 */
	pm_runtime_get_sync(&pdev->dev);

	ret = request_irq(abe->irq, abe_irq_handler,
				0, "ABE", (void *)abe);
	if (ret) {
		dev_err(platform->dev, "request for ABE IRQ %d failed %d\n",
				abe->irq, ret);
		return ret;
	}

	abe_reset_hal();

	abe_load_fw();	// TODO: use fw API here

	/* Config OPP 100 for now */
	abe_set_opp_processing(ABE_OPP100);

	/* "tick" of the audio engine */
	abe_write_event_generator(EVENT_TIMER);

	dl2_eq.equ_length = ARRAY_SIZE(DL2_COEF);

	/* build the coefficient parameter for the equalizer api */
	memcpy(dl2_eq.coef.type1, DL2_COEF, sizeof(DL2_COEF));

	/* load the high-pass coefficient of IHF-Right */
	abe_write_equalizer(EQ2L, &dl2_eq);

	/* load the high-pass coefficient of IHF-Left */
	abe_write_equalizer(EQ2R, &dl2_eq);

	pm_runtime_put_sync(&pdev->dev);
	return ret;
}

void abe_dsp_enable_data_transfer(int port)
{
	/* TODO: do not use abe global structure to assign pdev */
	struct platform_device *pdev = abe->pdev;

	pm_runtime_get_sync(&pdev->dev);
	abe_enable_data_transfer(port);
}

void abe_dsp_disable_data_transfer(int port)
{
	/* TODO: do not use abe global structure to assign pdev */
	struct platform_device *pdev = abe->pdev;

	abe_enable_data_transfer(port);
	pm_runtime_put_sync(&pdev->dev);
}

/*
 * These TLV settings will need fine tuned for each individual control
 */

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

//TODO: we have to use the shift value atm to represent register id due to current HAL
static int dl1_put_mixer(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget *widget = snd_kcontrol_chip(kcontrol);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;

	/* TODO: do not use abe global structure to assign pdev */
	struct platform_device *pdev = abe->pdev;

	pm_runtime_get_sync(&pdev->dev);

	if (ucontrol->value.integer.value[0]) {
		abe->dapm[mc->shift] = ucontrol->value.integer.value[0];
		snd_soc_dapm_mixer_update_power(widget, kcontrol, 1);
	} else {
		abe->dapm[mc->shift] = ucontrol->value.integer.value[0];
		snd_soc_dapm_mixer_update_power(widget, kcontrol, 0);
	}
	pm_runtime_put_sync(&pdev->dev);

	return 1;
}

static int dl2_put_mixer(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget *widget = snd_kcontrol_chip(kcontrol);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;

	/* TODO: do not use abe global structure to assign pdev */
	struct platform_device *pdev = abe->pdev;

	pm_runtime_get_sync(&pdev->dev);

	if (ucontrol->value.integer.value[0]) {
		abe->dapm[mc->shift] = ucontrol->value.integer.value[0];
		snd_soc_dapm_mixer_update_power(widget, kcontrol, 1);
	} else {
		abe->dapm[mc->shift] = ucontrol->value.integer.value[0];
		snd_soc_dapm_mixer_update_power(widget, kcontrol, 0);
	}

	pm_runtime_put_sync(&pdev->dev);
	return 1;
}

static int audio_ul_put_mixer(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget *widget = snd_kcontrol_chip(kcontrol);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;

	/* TODO: do not use abe global structure to assign pdev */
	struct platform_device *pdev = abe->pdev;

	pm_runtime_get_sync(&pdev->dev);

	if (ucontrol->value.integer.value[0]) {
		abe->dapm[mc->shift] = ucontrol->value.integer.value[0];
		snd_soc_dapm_mixer_update_power(widget, kcontrol, 1);
	} else {
		abe->dapm[mc->shift] = ucontrol->value.integer.value[0];
		snd_soc_dapm_mixer_update_power(widget, kcontrol, 0);
	}
	pm_runtime_put_sync(&pdev->dev);

	return 1;
}

static int vxrec_put_mixer(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget *widget = snd_kcontrol_chip(kcontrol);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;

	/* TODO: do not use abe global structure to assign pdev */
	struct platform_device *pdev = abe->pdev;

	pm_runtime_get_sync(&pdev->dev);

	if (ucontrol->value.integer.value[0]) {
		abe->dapm[mc->shift] = ucontrol->value.integer.value[0];
		snd_soc_dapm_mixer_update_power(widget, kcontrol, 1);
	} else {
		abe->dapm[mc->shift] = ucontrol->value.integer.value[0];
		snd_soc_dapm_mixer_update_power(widget, kcontrol, 0);
	}
	pm_runtime_put_sync(&pdev->dev);

	return 1;
}

static int sdt_put_mixer(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget *widget = snd_kcontrol_chip(kcontrol);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;

	/* TODO: do not use abe global structure to assign pdev */
	struct platform_device *pdev = abe->pdev;

	pm_runtime_get_sync(&pdev->dev);

	if (ucontrol->value.integer.value[0]) {
		abe->dapm[mc->shift] = ucontrol->value.integer.value[0];
		snd_soc_dapm_mixer_update_power(widget, kcontrol, 1);
	} else {
		abe->dapm[mc->shift] = ucontrol->value.integer.value[0];
		snd_soc_dapm_mixer_update_power(widget, kcontrol, 0);
	}
	pm_runtime_put_sync(&pdev->dev);

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

/* router IDs that match our mixer strings */
static const abe_router_t router[] = {
		ZERO_labelID, /* strangely this is not 0 */
		DMIC1_L_labelID, DMIC1_R_labelID,
		DMIC2_L_labelID, DMIC2_R_labelID,
		DMIC3_L_labelID, DMIC3_R_labelID,
		BT_UL_L_labelID, BT_UL_R_labelID,
		AMIC_L_labelID, AMIC_R_labelID,
		VX_REC_L_labelID, VX_REC_R_labelID,
};

static int ul_mux_put_route(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget *widget = snd_kcontrol_chip(kcontrol);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	int mux = ucontrol->value.enumerated.item[0];
	int reg = e->reg - ABE_MUX_BASE, i;

	/* TODO: do not use abe global structure to assign pdev */
	struct platform_device *pdev = abe->pdev;

	pm_runtime_get_sync(&pdev->dev);

	if (mux >= ABE_ROUTES_UL)
		return 0;

	if (reg < 8) {
		/* 0  .. 9   = MM_UL */
		abe->router[reg] = router[mux];
	} else if (reg < 12) {
		/* 10 .. 11  = MM_UL2 */
		/* 12 .. 13  = VX_UL */
		abe->router[reg + 2] = router[mux];
	}

	/* there is a 2 slot gap in the table, making it ABE_ROUTES_UL + 2 in size */
	for (i = 0; i < ABE_ROUTES_UL + 2; i++)
		dev_dbg(widget->dapm->dev, "router table [%d] = %d\n", i, abe->router[i]);

	/* 2nd arg here is unused */
	abe_set_router_configuration(UPROUTE, 0, abe->router);

	abe->dapm[e->reg] = ucontrol->value.integer.value[0];
	snd_soc_dapm_mux_update_power(widget, kcontrol, abe->dapm[e->reg], mux, e);
	pm_runtime_put_sync(&pdev->dev);

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

	/* TODO: do not use abe global structure to assign pdev */
	struct platform_device *pdev = abe->pdev;

	pm_runtime_get_sync(&pdev->dev);

	if (ucontrol->value.integer.value[0]) {
		abe->dapm[mc->shift] = ucontrol->value.integer.value[0];
		snd_soc_dapm_mixer_update_power(widget, kcontrol, 1);
	} else {
		abe->dapm[mc->shift] = ucontrol->value.integer.value[0];
		snd_soc_dapm_mixer_update_power(widget, kcontrol, 0);
	}
	pm_runtime_put_sync(&pdev->dev);

	return 1;
}


static int volume_put_sdt_mixer(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;

	/* TODO: do not use abe global structure to assign pdev */
	struct platform_device *pdev = abe->pdev;

	pm_runtime_get_sync(&pdev->dev);

	abe_write_mixer(MIXSDT, -12000 + (ucontrol->value.integer.value[0] * 100),
				RAMP_0MS, mc->reg);
	pm_runtime_put_sync(&pdev->dev);

	return 1;
}

static int volume_put_audul_mixer(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;

	/* TODO: do not use abe global structure to assign pdev */
	struct platform_device *pdev = abe->pdev;

	pm_runtime_get_sync(&pdev->dev);
	abe_write_mixer(MIXAUDUL, -12000 + (ucontrol->value.integer.value[0] * 100),
				RAMP_0MS, mc->reg);
	pm_runtime_put_sync(&pdev->dev);

	return 1;
}

static int volume_put_vxrec_mixer(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	/* TODO: do not use abe global structure to assign pdev */
	struct platform_device *pdev = abe->pdev;

	pm_runtime_get_sync(&pdev->dev);
	abe_write_mixer(MIXVXREC, -12000 + (ucontrol->value.integer.value[0] * 100),
				RAMP_0MS, mc->reg);
	pm_runtime_put_sync(&pdev->dev);

	return 1;
}

static int volume_put_dl1_mixer(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	/* TODO: do not use abe global structure to assign pdev */
	struct platform_device *pdev = abe->pdev;

	pm_runtime_get_sync(&pdev->dev);
	abe_write_mixer(MIXDL1, -12000 + (ucontrol->value.integer.value[0] * 100),
				RAMP_0MS, mc->reg);
	pm_runtime_put_sync(&pdev->dev);

	return 1;
}

static int volume_put_dl2_mixer(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	/* TODO: do not use abe global structure to assign pdev */
	struct platform_device *pdev = abe->pdev;

	pm_runtime_get_sync(&pdev->dev);
	abe_write_mixer(MIXDL2, -12000 + (ucontrol->value.integer.value[0] * 100),
				RAMP_0MS, mc->reg);
	pm_runtime_put_sync(&pdev->dev);

	return 1;
}

static int volume_get_dl1_mixer(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	u32 val;
	/* TODO: do not use abe global structure to assign pdev */
	struct platform_device *pdev = abe->pdev;

	pm_runtime_get_sync(&pdev->dev);
	abe_read_mixer(MIXDL1, &val, mc->reg);
	ucontrol->value.integer.value[0] = (val + 12000) / 100;
	pm_runtime_put_sync(&pdev->dev);

	return 0;
}

static int volume_get_dl2_mixer(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	/* TODO: do not use abe global structure to assign pdev */
	struct platform_device *pdev = abe->pdev;
	u32 val;

	pm_runtime_get_sync(&pdev->dev);
	abe_read_mixer(MIXDL2, &val, mc->reg);
	ucontrol->value.integer.value[0] = (val + 12000) / 100;
	pm_runtime_put_sync(&pdev->dev);

	return 0;
}

static int volume_get_audul_mixer(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	/* TODO: do not use abe global structure to assign pdev */
	struct platform_device *pdev = abe->pdev;
	u32 val;

	pm_runtime_get_sync(&pdev->dev);
	abe_read_mixer(MIXAUDUL, &val, mc->reg);
	ucontrol->value.integer.value[0] = (val + 12000) / 100;
	pm_runtime_put_sync(&pdev->dev);

	return 0;
}

static int volume_get_vxrec_mixer(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	/* TODO: do not use abe global structure to assign pdev */
	struct platform_device *pdev = abe->pdev;
	u32 val;

	pm_runtime_get_sync(&pdev->dev);
	abe_read_mixer(MIXVXREC, &val, mc->reg);
	ucontrol->value.integer.value[0] = (val + 12000) / 100;
	pm_runtime_put_sync(&pdev->dev);

	return 0;
}

static int volume_get_sdt_mixer(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	/* TODO: do not use abe global structure to assign pdev */
	struct platform_device *pdev = abe->pdev;
	u32 val;

	pm_runtime_get_sync(&pdev->dev);
	abe_read_mixer(MIXSDT, &val, mc->reg);
	ucontrol->value.integer.value[0] = (val + 12000) / 100;
	pm_runtime_put_sync(&pdev->dev);

	return 0;
}

static const char *route_ul_texts[] =
	{"None", "DMic0L", "DMic0R", "DMic1L", "DMic1R", "DMic2L", "DMic2R",
	"BT Left", "BT Right", "AMic0", "AMic1", "VX Left", "VX Right"};

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
	SOC_SINGLE_EXT("Tones", MIX_DL1_INPUT_TONES, 0, 1, 0,
		abe_get_mixer, dl1_put_mixer),
	SOC_SINGLE_EXT("Voice", MIX_DL1_INPUT_VX_DL, 1, 1, 0,
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
		MIX_DL1_INPUT_MM_DL, 0, 149, 0,
		volume_get_dl1_mixer, volume_put_dl1_mixer, mm_dl1_tlv),
	SOC_SINGLE_EXT_TLV("DL1 Tones Playback Volume",
		MIX_DL1_INPUT_TONES, 0, 149, 0,
		volume_get_dl1_mixer, volume_put_dl1_mixer, tones_dl1_tlv),
	SOC_SINGLE_EXT_TLV("DL1 Voice Playback Volume",
		MIX_DL1_INPUT_VX_DL, 0, 149, 0,
		volume_get_dl1_mixer, volume_put_dl1_mixer, voice_dl1_tlv),
	SOC_SINGLE_EXT_TLV("DL1 Capture Playback Volume",
		MIX_DL1_INPUT_MM_UL2, 0, 149, 0,
		volume_get_dl1_mixer, volume_put_dl1_mixer, capture_dl1_tlv),

	/* DL2 mixer gains */
	SOC_SINGLE_EXT_TLV("DL2 Media Playback Volume",
		MIX_DL2_INPUT_MM_DL, 0, 149, 0,
		volume_get_dl2_mixer, volume_put_dl2_mixer, mm_dl2_tlv),
	SOC_SINGLE_EXT_TLV("DL2 Tones Playback Volume",
		MIX_DL2_INPUT_TONES, 0, 149, 0,
		volume_get_dl2_mixer, volume_put_dl2_mixer, tones_dl2_tlv),
	SOC_SINGLE_EXT_TLV("DL2 Voice Playback Volume",
		MIX_DL2_INPUT_VX_DL, 0, 149, 0,
		volume_get_dl2_mixer, volume_put_dl2_mixer, voice_dl2_tlv),
	SOC_SINGLE_EXT_TLV("DL2 Capture Playback Volume",
		MIX_DL2_INPUT_MM_UL2, 0, 149, 0,
		volume_get_dl2_mixer, volume_put_dl2_mixer, capture_dl2_tlv),

	/* VXREC mixer gains */
	SOC_SINGLE_EXT_TLV("VXREC Media Volume",
		MIX_VXREC_INPUT_MM_DL, 0, 149, 0,
		volume_get_vxrec_mixer, volume_put_vxrec_mixer, vxrec_mm_dl_tlv),
	SOC_SINGLE_EXT_TLV("VXREC Tones Volume",
		MIX_VXREC_INPUT_TONES, 0, 149, 0,
		volume_get_vxrec_mixer, volume_put_vxrec_mixer, vxrec_tones_tlv),
	SOC_SINGLE_EXT_TLV("VXREC Voice DL Volume",
		MIX_VXREC_INPUT_VX_UL, 0, 149, 0,
		volume_get_vxrec_mixer, volume_put_vxrec_mixer, vxrec_vx_dl_tlv),
	SOC_SINGLE_EXT_TLV("VXREC Voice UL Volume",
		MIX_VXREC_INPUT_VX_DL, 0, 149, 0,
		volume_get_vxrec_mixer, volume_put_vxrec_mixer, vxrec_vx_ul_tlv),

	/* AUDUL mixer gains */
	SOC_SINGLE_EXT_TLV("AUDUL Media Volume",
		MIX_AUDUL_INPUT_MM_DL, 0, 149, 0,
		volume_get_audul_mixer, volume_put_audul_mixer, audul_mm_tlv),
	SOC_SINGLE_EXT_TLV("AUDUL Tones Volume",
		MIX_AUDUL_INPUT_TONES, 0, 149, 0,
		volume_get_audul_mixer, volume_put_audul_mixer, audul_tones_tlv),
	SOC_SINGLE_EXT_TLV("AUDUL Voice UL Volume",
		MIX_AUDUL_INPUT_UPLINK, 0, 149, 0,
		volume_get_audul_mixer, volume_put_audul_mixer, audul_vx_ul_tlv),
	SOC_SINGLE_EXT_TLV("AUDUL Voice DL Volume",
		MIX_AUDUL_INPUT_VX_DL, 0, 149, 0,
		volume_get_audul_mixer, volume_put_audul_mixer, audul_vx_dl_tlv),

	/* SDT mixer gains */
	SOC_SINGLE_EXT_TLV("SDT UL Volume",
		MIX_SDT_INPUT_UP_MIXER, 0, 149, 0,
		volume_get_sdt_mixer, volume_put_sdt_mixer, sdt_ul_tlv),
	SOC_SINGLE_EXT_TLV("SDT DL Volume",
		MIX_SDT_INPUT_DL1_MIXER, 0, 149, 0,
		volume_get_sdt_mixer, volume_put_sdt_mixer, sdt_dl_tlv),
};

static const struct snd_soc_dapm_widget abe_dapm_widgets[] = {

	/* Frontend AIFs */
	SND_SOC_DAPM_AIF_IN("TONES_DL", "Tones Playback", 0,
			ABE_WIDGET(0), ABE_OPP_25, 0),
	SND_SOC_DAPM_AIF_IN("VX_DL", "Voice Playback", 0,
			ABE_WIDGET(1), ABE_OPP_50, 0),
	SND_SOC_DAPM_AIF_OUT("VX_UL", "Voice Capture", 0,
			ABE_WIDGET(2), ABE_OPP_50, 0),
	/* the MM_UL mapping is intentional */
	SND_SOC_DAPM_AIF_OUT("MM_UL1", "MultiMedia2 Capture", 0,
			ABE_WIDGET(3), ABE_OPP_50, 0),
	SND_SOC_DAPM_AIF_OUT("MM_UL2", "MultiMedia1 Capture", 0,
			ABE_WIDGET(4), ABE_OPP_50, 0),
	SND_SOC_DAPM_AIF_IN("MM_DL", " MultiMedia1 Playback", 0,
			ABE_WIDGET(5), ABE_OPP_25, 0),
	SND_SOC_DAPM_AIF_IN("VIB_DL", "Vibra Playback", 0,
			ABE_WIDGET(6), ABE_OPP_100, 0),
	SND_SOC_DAPM_AIF_IN("MODEM_DL", "MODEM Playback", 0,
			ABE_WIDGET(7), ABE_OPP_50, 0),
	SND_SOC_DAPM_AIF_OUT("MODEM_UL", "MODEM Capture", 0,
			ABE_WIDGET(8), ABE_OPP_50, 0),

	/* Backend DAIs  */
	// FIXME: must match BE order in abe_dai.h
	SND_SOC_DAPM_AIF_IN("PDM_UL1", "Analog Capture", 0,
			ABE_WIDGET(9), ABE_OPP_50, 0),
	SND_SOC_DAPM_AIF_OUT("PDM_DL1", "HS Playback", 0,
			ABE_WIDGET(10), ABE_OPP_25, 0),
	SND_SOC_DAPM_AIF_OUT("PDM_DL2", "HF Playback", 0,
			ABE_WIDGET(11), ABE_OPP_100, 0),
	SND_SOC_DAPM_AIF_OUT("PDM_VIB", "Vibra Playback", 0,
			ABE_WIDGET(12), ABE_OPP_100, 0),
	SND_SOC_DAPM_AIF_IN("BT_VX_UL", "BT Capture", 0,
			ABE_WIDGET(13), ABE_OPP_50, 0),
	SND_SOC_DAPM_AIF_OUT("BT_VX_DL", "BT Playback", 0,
			ABE_WIDGET(14), ABE_OPP_50, 0),
	SND_SOC_DAPM_AIF_IN("MM_EXT_UL", "FM Capture", 0,
			ABE_WIDGET(15), ABE_OPP_50, 0),
	SND_SOC_DAPM_AIF_OUT("MM_EXT_DL", "FM Playback", 0,
			ABE_WIDGET(16), ABE_OPP_25, 0),
	SND_SOC_DAPM_AIF_IN("DMIC0", "DMIC0 Capture", 0,
			ABE_WIDGET(17), ABE_OPP_50, 0),
	SND_SOC_DAPM_AIF_IN("DMIC1", "DMIC1 Capture", 0,
			ABE_WIDGET(18), ABE_OPP_50, 0),
	SND_SOC_DAPM_AIF_IN("DMIC2", "DMIC2 Capture", 0,
			ABE_WIDGET(19), ABE_OPP_50, 0),

	/* ROUTE_UL Capture Muxes */
	SND_SOC_DAPM_MUX("MUX_UL00",
			ABE_WIDGET(20), ABE_OPP_50, 0, &mm_ul00_control),
	SND_SOC_DAPM_MUX("MUX_UL01",
			ABE_WIDGET(21), ABE_OPP_50, 0, &mm_ul01_control),
	SND_SOC_DAPM_MUX("MUX_UL02",
			ABE_WIDGET(22), ABE_OPP_50, 0, &mm_ul02_control),
	SND_SOC_DAPM_MUX("MUX_UL03",
			ABE_WIDGET(23), ABE_OPP_50, 0, &mm_ul03_control),
	SND_SOC_DAPM_MUX("MUX_UL04",
			ABE_WIDGET(24), ABE_OPP_50, 0, &mm_ul04_control),
	SND_SOC_DAPM_MUX("MUX_UL05",
			ABE_WIDGET(25), ABE_OPP_50, 0, &mm_ul05_control),
	SND_SOC_DAPM_MUX("MUX_UL06",
			ABE_WIDGET(26), ABE_OPP_50, 0, &mm_ul06_control),
	SND_SOC_DAPM_MUX("MUX_UL07",
			ABE_WIDGET(27), ABE_OPP_50, 0, &mm_ul07_control),
	SND_SOC_DAPM_MUX("MUX_UL10",
			ABE_WIDGET(28), ABE_OPP_50, 0, &mm_ul10_control),
	SND_SOC_DAPM_MUX("MUX_UL11",
			ABE_WIDGET(29), ABE_OPP_50, 0, &mm_ul11_control),
	SND_SOC_DAPM_MUX("MUX_VX0",
			ABE_WIDGET(30), ABE_OPP_50, 0, &mm_vx0_control),
	SND_SOC_DAPM_MUX("MUX_VX1",
			ABE_WIDGET(31), ABE_OPP_50, 0, &mm_vx1_control),

	/* DL1 & DL2 Playback Mixers */
	SND_SOC_DAPM_MIXER("DL1 Mixer",
			ABE_WIDGET(32), ABE_OPP_25, 0, dl1_mixer_controls,
			ARRAY_SIZE(dl1_mixer_controls)),
	SND_SOC_DAPM_MIXER("DL2 Mixer",
			ABE_WIDGET(33), ABE_OPP_100, 0, dl2_mixer_controls,
			ARRAY_SIZE(dl2_mixer_controls)),

	/* DL1 Mixer Input volumes ?????*/
	SND_SOC_DAPM_PGA("DL1 Media Volume",
			ABE_WIDGET(34), 0, 0, NULL, 0),

	/* AUDIO_UL_MIXER */
	SND_SOC_DAPM_MIXER("Voice Capture Mixer",
			ABE_WIDGET(35), ABE_OPP_50, 0, audio_ul_mixer_controls,
			ARRAY_SIZE(audio_ul_mixer_controls)),

	/* VX_REC_MIXER */
	SND_SOC_DAPM_MIXER("Capture Mixer",
			ABE_WIDGET(36), ABE_OPP_50, 0, vx_rec_mixer_controls,
			ARRAY_SIZE(vx_rec_mixer_controls)),

	/* SDT_MIXER  - TODO: shoult this not be OPP25 ??? */
	SND_SOC_DAPM_MIXER("Sidetone Mixer",
			ABE_WIDGET(37), ABE_OPP_50, 0, sdt_mixer_controls,
			ARRAY_SIZE(sdt_mixer_controls)),

	/*
	 * The Following three are virtual switches to select the output port
	 * after DL1 Gain - HAL V0.6x
	 */

	/* Virtual PDM_DL1 Switch */
	SND_SOC_DAPM_MIXER("DL1 PDM",
			ABE_WIDGET(38), ABE_OPP_25, 0, &pdm_dl1_switch_controls, 1),

	/* Virtual BT_VX_DL Switch */
	SND_SOC_DAPM_MIXER("DL1 BT_VX",
			ABE_WIDGET(39), ABE_OPP_50, 0, &bt_vx_dl_switch_controls, 1),

	/* Virtual MM_EXT_DL Switch TODO: confrm OPP level here */
	SND_SOC_DAPM_MIXER("DL1 MM_EXT",
			ABE_WIDGET(40), ABE_OPP_50, 0, &mm_ext_dl_switch_controls, 1),

	/*
	 * The Following three are virtual switches to select the input port
	 * before AMIC_UL enters ROUTE_UL - HAL V0.6x
	 */

	/* Virtual MM_EXT_UL Switch */
	SND_SOC_DAPM_MIXER("AMIC_UL MM_EXT",
			ABE_WIDGET(41), ABE_OPP_50, 0, &mm_ext_ul_switch_controls, 1),

	/* Virtual PDM_UL1 Switch */
	SND_SOC_DAPM_MIXER("AMIC_UL PDM",
			ABE_WIDGET(42), ABE_OPP_50, 0, &pdm_ul1_switch_controls, 1),

	/* Virtual to join MM_EXT and PDM+UL1 switches */
	SND_SOC_DAPM_MIXER("AMIC_UL", SND_SOC_NOPM, 0, 0, NULL, 0),

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
	{"MUX_UL00", "AMic0", "AMIC_UL"},
	{"MUX_UL00", "AMic1", "AMIC_UL"},
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
	{"MUX_UL01", "AMic0", "AMIC_UL"},
	{"MUX_UL01", "AMic1", "AMIC_UL"},
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
	{"MUX_UL02", "AMic0", "AMIC_UL"},
	{"MUX_UL02", "AMic1", "AMIC_UL"},
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
	{"MUX_UL03", "AMic0", "AMIC_UL"},
	{"MUX_UL03", "AMic1", "AMIC_UL"},
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
	{"MUX_UL04", "AMic0", "AMIC_UL"},
	{"MUX_UL04", "AMic1", "AMIC_UL"},
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
	{"MUX_UL05", "AMic0", "AMIC_UL"},
	{"MUX_UL05", "AMic1", "AMIC_UL"},
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
	{"MUX_UL06", "AMic0", "AMIC_UL"},
	{"MUX_UL06", "AMic1", "AMIC_UL"},
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
	{"MUX_UL07", "AMic0", "AMIC_UL"},
	{"MUX_UL07", "AMic1", "AMIC_UL"},
	{"MUX_UL07", "VX Left", "Capture Mixer"},
	{"MUX_UL07", "VX Right", "Capture Mixer"},
	{"MM_UL1", NULL, "MUX_UL07"},

	/* MUX_UL10 - ROUTE_UL - Chan 10  */
	{"MUX_UL10", "DMic0L", "DMIC0"},
	{"MUX_UL10", "DMic0R", "DMIC0"},
	{"MUX_UL10", "DMic1L", "DMIC1"},
	{"MUX_UL10", "DMic1R", "DMIC1"},
	{"MUX_UL10", "DMic2L", "DMIC2"},
	{"MUX_UL10", "DMic2R", "DMIC2"},
	{"MUX_UL10", "BT Left", "BT_VX_UL"},
	{"MUX_UL10", "BT Right", "BT_VX_UL"},
	{"MUX_UL10", "AMic0", "AMIC_UL"},
	{"MUX_UL10", "AMic1", "AMIC_UL"},
	{"MUX_UL10", "VX Left", "Capture Mixer"},
	{"MUX_UL10", "VX Right", "Capture Mixer"},
	{"MM_UL2", NULL, "MUX_UL10"},

	/* MUX_UL11 - ROUTE_UL - Chan 11  */
	{"MUX_UL11", "DMic0L", "DMIC0"},
	{"MUX_UL11", "DMic0R", "DMIC0"},
	{"MUX_UL11", "DMic1L", "DMIC1"},
	{"MUX_UL11", "DMic1R", "DMIC1"},
	{"MUX_UL11", "DMic2L", "DMIC2"},
	{"MUX_UL11", "DMic2R", "DMIC2"},
	{"MUX_UL11", "BT Left", "BT_VX_UL"},
	{"MUX_UL11", "BT Right", "BT_VX_UL"},
	{"MUX_UL11", "AMic0", "AMIC_UL"},
	{"MUX_UL11", "AMic1", "AMIC_UL"},
	{"MUX_UL11", "VX Left", "Capture Mixer"},
	{"MUX_UL11", "VX Right", "Capture Mixer"},
	{"MM_UL2", NULL, "MUX_UL11"},

	/* MUX_VX0 - ROUTE_UL - Chan 20  */
	{"MUX_VX0", "DMic0L", "DMIC0"},
	{"MUX_VX0", "DMic0R", "DMIC0"},
	{"MUX_VX0", "DMic1L", "DMIC1"},
	{"MUX_VX0", "DMic1R", "DMIC1"},
	{"MUX_VX0", "DMic2L", "DMIC2"},
	{"MUX_VX0", "DMic2R", "DMIC2"},
	{"MUX_VX0", "BT Left", "BT_VX_UL"},
	{"MUX_VX0", "BT Right", "BT_VX_UL"},
	{"MUX_VX0", "AMic0", "AMIC_UL"},
	{"MUX_VX0", "AMic1", "AMIC_UL"},
	{"MUX_VX0", "VX Left", "Capture Mixer"},
	{"MUX_VX0", "VX Right", "Capture Mixer"},

	/* MUX_VX1 - ROUTE_UL - Chan 20  */
	{"MUX_VX1", "DMic0L", "DMIC0"},
	{"MUX_VX1", "DMic0R", "DMIC0"},
	{"MUX_VX1", "DMic1L", "DMIC1"},
	{"MUX_VX1", "DMic1R", "DMIC1"},
	{"MUX_VX1", "DMic2L", "DMIC2"},
	{"MUX_VX1", "DMic2R", "DMIC2"},
	{"MUX_VX1", "BT Left", "BT_VX_UL"},
	{"MUX_VX1", "BT Right", "BT_VX_UL"},
	{"MUX_VX1", "AMic0", "AMIC_UL"},
	{"MUX_VX1", "AMic1", "AMIC_UL"},
	{"MUX_VX1", "VX Left", "Capture Mixer"},
	{"MUX_VX1", "VX Right", "Capture Mixer"},

	/* Capture Input Selection  for AMIC_UL  */
	{"AMIC_UL MM_EXT", "Switch", "MM_EXT_UL"},
	{"AMIC_UL PDM", "Switch", "PDM_UL1"},
	{"AMIC_UL", NULL, "AMIC_UL MM_EXT"},
	{"AMIC_UL", NULL, "AMIC_UL PDM"},

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
	/* TODO: do not use abe global structure to assign pdev */
	struct platform_device *pdev = abe->pdev;

	mutex_lock(&abe->mutex);

	abe->fe_id = dai->id;
	dev_dbg(&rtd->dev, "%s ID %d\n", __func__, dai->id);

	pm_runtime_get_sync(&pdev->dev);
	switch (dai->id) {
	case ABE_FRONTEND_DAI_MODEM:
	case ABE_FRONTEND_DAI_LP_MEDIA:
		snd_soc_set_runtime_hwparams(substream, &omap_abe_hardware);
		break;
	default:
		break;
	}

	mutex_unlock(&abe->mutex);
	return 0;
}

static int aess_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *dai = rtd->cpu_dai;

	dev_dbg(&rtd->dev, "%s ID %d\n", __func__, dai->id);

	switch (dai->id) {
	case ABE_FRONTEND_DAI_MODEM:
	case ABE_FRONTEND_DAI_LP_MEDIA:
		snd_pcm_set_runtime_buffer(substream, &substream->dma_buffer);
		runtime->dma_bytes = params_buffer_bytes(params);
		break;
	default:
		break;
	}

	return 0;
}

static int aess_prepare(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime  *rtd = substream->private_data;
	int i, opp = 0;

	mutex_lock(&abe->mutex);

	dev_dbg(&rtd->dev, "%s ID %d\n", __func__, rtd->cpu_dai->id);

	/* now calculate OPP level based upon DAPM widget status */
	for (i = ABE_WIDGET_START; i < ABE_WIDGET_END; i++) {
	//	dev_dbg(&rtd->dev, "opp w %d val 0x%x\n", i, abe->dapm[i]);
		opp |= abe->dapm[i];
	}

	opp = (1 << (fls(opp) - 1)) * 25;
	dev_dbg(&rtd->dev, "OPP level at prepare is %d\n", opp);

	switch (opp) {
	case 25:
		//abe_set_opp_processing(ABE_OPP25);
		break;
	case 50:
		//abe_set_opp_processing(ABE_OPP50);
		break;
	case 100:
	default:
		abe_set_opp_processing(ABE_OPP100);
		break;
	}

	mutex_unlock(&abe->mutex);
	return 0;
}

static int aess_close(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime  *rtd = substream->private_data;
	struct snd_soc_dai *dai = rtd->cpu_dai;
	/* TODO: do not use abe global structure to assign pdev */
	struct platform_device *pdev = abe->pdev;

	int i, opp = 0;

	mutex_lock(&abe->mutex);

	abe->fe_id = dai->id;
	dev_dbg(&rtd->dev, "%s ID %d\n", __func__, dai->id);

	/* now calculate OPP level based upon DAPM widget status */
	for (i = ABE_WIDGET_START; i < ABE_WIDGET_END; i++) {
	//	dev_dbg(&rtd->dev, "opp w %d val 0x%x\n", i, abe->dapm[i]);
		opp |= abe->dapm[i];
	}
	opp = (1 << (fls(opp) - 1)) * 25;
	dev_dbg(&rtd->dev, "OPP level at close is %d\n", opp);

	switch (opp) {
	case 25:
		//abe_set_opp_processing(ABE_OPP25);
		break;
	case 50:
		//abe_set_opp_processing(ABE_OPP50);
		break;
	case 100:
	default:
		abe_set_opp_processing(ABE_OPP100);
		break;
	}
	pm_runtime_put_sync(&pdev->dev);

	mutex_unlock(&abe->mutex);
	return 0;
}

static int aess_mmap(struct snd_pcm_substream *substream,
	struct vm_area_struct *vma)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	int ret;

	/* TODO: we may need to check for underrun. */

	ret = dma_mmap_writecombine(substream->pcm->card->dev, vma,
				     runtime->dma_area,
				     runtime->dma_addr,
				     runtime->dma_bytes);
	if (ret < 0)
		return ret;

	/* TODO: tell ABE we have new period. */
	/* abe_blah(); */
	return 0;
}

static struct snd_pcm_ops omap_aess_pcm_ops = {
	.open = aess_open,
	.hw_params	= aess_hw_params,
	.prepare	= aess_prepare,
	.close	= aess_close,
	.mmap		= aess_mmap,
};

/* TODO: MODEM doesn't need this although low power mmap() does */
/* TODO: We need the buffer less IOCTL() to support MODEM */
static u64 omap_pcm_dmamask = DMA_BIT_MASK(64);

static int aess_pcm_preallocate_dma_buffer(struct snd_pcm *pcm,
	int stream)
{
	struct snd_pcm_substream *substream = pcm->streams[stream].substream;
	struct snd_dma_buffer *buf = &substream->dma_buffer;
	size_t size = omap_abe_hardware.buffer_bytes_max;

	buf->dev.type = SNDRV_DMA_TYPE_DEV;
	buf->dev.dev = pcm->card->dev;
	buf->private_data = NULL;
	buf->area = dma_alloc_writecombine(pcm->card->dev, size,
					   &buf->addr, GFP_KERNEL);

	if (!buf->area)
		return -ENOMEM;

	buf->bytes = size;

	return 0;
}

static void aess_pcm_free_dma_buffers(struct snd_pcm *pcm)
{
	struct snd_pcm_substream *substream;
	struct snd_dma_buffer *buf;
	int stream;

	for (stream = 0; stream < 2; stream++) {
		substream = pcm->streams[stream].substream;
		if (!substream)
			continue;

		buf = &substream->dma_buffer;
		if (!buf->area)
			continue;

		dma_free_writecombine(pcm->card->dev, buf->bytes,
				      buf->area, buf->addr);
		buf->area = NULL;
	}
}

static int aess_pcm_new(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_card *card = rtd->card->snd_card;
	struct snd_pcm *pcm = rtd->pcm;
	int ret = 0;

	if (!card->dev->dma_mask)
		card->dev->dma_mask = &omap_pcm_dmamask;
	if (!card->dev->coherent_dma_mask)
		card->dev->coherent_dma_mask = DMA_BIT_MASK(64);

	if (pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream) {
		ret = aess_pcm_preallocate_dma_buffer(pcm,
			SNDRV_PCM_STREAM_PLAYBACK);
		if (ret)
			goto out;
	}

	if (pcm->streams[SNDRV_PCM_STREAM_CAPTURE].substream) {
		ret = aess_pcm_preallocate_dma_buffer(pcm,
			SNDRV_PCM_STREAM_CAPTURE);
		if (ret)
			goto out;
	}

out:
	return ret;
}

static struct snd_soc_platform_driver omap_aess_platform = {
	.ops	= &omap_aess_pcm_ops,
	.probe = abe_probe,
	.remove = abe_remove,
	.pcm_new	= aess_pcm_new,
	.pcm_free	= aess_pcm_free_dma_buffers,
	.read = abe_dsp_read,
	.write = abe_dsp_write,
};

static int __devinit abe_engine_probe(struct platform_device *pdev)
{
	struct resource *res;
	int ret = -EINVAL, i;

	abe = kzalloc(sizeof(struct abe_data), GFP_KERNEL);
	if (abe == NULL)
		return -ENOMEM;
	dev_set_drvdata(&pdev->dev, abe);

	/* ZERO_labelID should really be 0 */
	for (i = 0; i < ABE_ROUTES_UL + 2; i++)
		abe->router[i] = ZERO_labelID;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		dev_err(&pdev->dev, "no resource\n");
		goto err;
	}

	abe->io_base = ioremap(res->start, resource_size(res));
	if (!abe->io_base) {
		ret = -ENOMEM;
		goto err;
	}

	abe->irq = platform_get_irq(pdev, 0);
	if (abe->irq < 0) {
		ret = abe->irq;
		goto err_irq;
	}

	pm_runtime_enable(&pdev->dev);

	abe->abe_pdata = pdev->dev.platform_data;
	abe->pdev = pdev;

	mutex_init(&abe->mutex);

	ret = snd_soc_register_platform(&pdev->dev,
			&omap_aess_platform);
	if (ret == 0)
		return 0;

err_irq:
	iounmap(abe->io_base);
err:
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
		.name = "omap-aess-audio",
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
