/*
 * omap-abe-twl6040.c  --  SoC audio for TI OMAP based boards with ABE and
 *			   twl6040 codec
 *
 * Author: Misael Lopez Cruz <misael.lopez@ti.com>
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

#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/mfd/twl6040.h>
#include <linux/platform_data/omap-abe-twl6040.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/of.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/jack.h>
#include <sound/pcm_params.h>
#include <sound/soc-dapm.h>
#include <sound/soc-dpcm.h>

#include "omap-dmic.h"
#include "omap-mcpdm.h"
#include "omap-pcm.h"
#include "omap-abe-priv.h"
#include "mcbsp.h"
#include "omap-mcbsp.h"
#include "omap-dmic.h"
#include "../codecs/twl6040.h"

struct omap_abe_data {
	int	jack_detection;	/* board can detect jack events */
	int	mclk_freq;	/* MCLK frequency speed for twl6040 */
	int	has_abe;
	int	has_dmic;
	int twl6040_power_mode;
	int mcbsp_cfg;
	struct snd_soc_platform *abe_platform;
	struct snd_soc_codec *twl_codec;
	struct i2c_client *tps6130x;
	struct i2c_adapter *adapter;

	struct platform_device *dmic_codec_dev;
	struct platform_device *spdif_codec_dev;
};

static struct i2c_board_info tps6130x_hwmon_info = {
	I2C_BOARD_INFO("tps6130x", 0x33),
};

/* configure the TPS6130x Handsfree Boost Converter */
static int omap_abe_tps6130x_configure(struct omap_abe_data *sdp4403)
{
	struct i2c_client *tps6130x = sdp4403->tps6130x;
	u8 data[2];

	data[0] = 0x01;
	data[1] = 0x60;
	if (i2c_master_send(tps6130x, data, 2) != 2)
		dev_err(&tps6130x->dev, "I2C write to TPS6130x failed\n");

	data[0] = 0x02;
	if (i2c_master_send(tps6130x, data, 2) != 2)
		dev_err(&tps6130x->dev, "I2C write to TPS6130x failed\n");
	return 0;
}

static int mcpdm_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
			struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
			SNDRV_PCM_HW_PARAM_RATE);

	rate->min = rate->max = 96000;

	return 0;
}

static int omap_abe_mcpdm_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_card *card = rtd->card;
	struct omap_abe_data *card_data = snd_soc_card_get_drvdata(card);
	int clk_id, freq;
	int ret;

	clk_id = twl6040_get_clk_id(rtd->codec);
	if (clk_id == TWL6040_SYSCLK_SEL_HPPLL)
		freq = card_data->mclk_freq;
	else if (clk_id == TWL6040_SYSCLK_SEL_LPPLL)
		freq = 32768;
	else {
		dev_err(card->dev, "invalid clock\n");
		return -EINVAL;
	}

	/* set the codec mclk */
	ret = snd_soc_dai_set_sysclk(codec_dai, clk_id, freq,
				SND_SOC_CLOCK_IN);
	if (ret)
		dev_err(card->dev, "can't set codec system clock\n");

	return ret;
}

static struct snd_soc_ops omap_abe_mcpdm_ops = {
	.hw_params = omap_abe_mcpdm_hw_params,
};

static int omap_abe_mcbsp_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_card *card = rtd->card;
	int ret;
	unsigned int be_id, channels;

	be_id = rtd->dai_link->be_id;

	/* FM + MODEM + Bluetooth all use I2S config */
	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S |
			SND_SOC_DAIFMT_NB_IF | SND_SOC_DAIFMT_CBM_CFM);
	if (ret < 0) {
		dev_err(card->dev, "can't set DAI %d configuration\n", be_id);
		return ret;
	}

	if (params != NULL) {
		struct omap_mcbsp *mcbsp = snd_soc_dai_get_drvdata(cpu_dai);
		/* Configure McBSP internal buffer usage */
		/* this need to be done for playback and/or record */
		channels = params_channels(params);
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			omap_mcbsp_set_tx_threshold(mcbsp, channels);
		else
			omap_mcbsp_set_rx_threshold(mcbsp, channels);
	}

	/* Set McBSP clock to external */
	ret = snd_soc_dai_set_sysclk(cpu_dai, OMAP_MCBSP_SYSCLK_CLKS_FCLK,
				     64 * params_rate(params), SND_SOC_CLOCK_IN);
	if (ret < 0)
		dev_err(card->dev, "can't set cpu system clock\n");

	return ret;
}

static struct snd_soc_ops omap_abe_mcbsp_ops = {
	.hw_params = omap_abe_mcbsp_hw_params,
};

static int omap_abe_dmic_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_card *card = rtd->card;
	int ret = 0;

	ret = snd_soc_dai_set_sysclk(cpu_dai, OMAP_DMIC_SYSCLK_PAD_CLKS,
				     19200000, SND_SOC_CLOCK_IN);
	if (ret < 0) {
		dev_err(card->dev, "can't set DMIC in system clock\n");
		return ret;
	}
	ret = snd_soc_dai_set_sysclk(cpu_dai, OMAP_DMIC_ABE_DMIC_CLK, 2400000,
				     SND_SOC_CLOCK_OUT);
	if (ret < 0)
		dev_err(card->dev, "can't set DMIC output clock\n");

	return ret;
}

static struct snd_soc_ops omap_abe_dmic_ops = {
	.hw_params = omap_abe_dmic_hw_params,
};

static int mcbsp_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
			struct snd_pcm_hw_params *params)
{
	struct snd_interval *channels = hw_param_interval(params,
						SNDRV_PCM_HW_PARAM_CHANNELS);
	unsigned int be_id = rtd->dai_link->be_id;

	if (be_id == OMAP_ABE_DAI_MM_FM || be_id == OMAP_ABE_DAI_BT_VX)
		channels->min = 2;

	snd_mask_set(&params->masks[SNDRV_PCM_HW_PARAM_FORMAT -
				    SNDRV_PCM_HW_PARAM_FIRST_MASK],
				    SNDRV_PCM_FORMAT_S16_LE);
	return 0;
}

static int dmic_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
			struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
			SNDRV_PCM_HW_PARAM_RATE);

	/* The ABE will covert the FE rate to 96k */
	rate->min = rate->max = 96000;

	snd_mask_set(&params->masks[SNDRV_PCM_HW_PARAM_FORMAT -
				    SNDRV_PCM_HW_PARAM_FIRST_MASK],
				    SNDRV_PCM_FORMAT_S32_LE);
	return 0;
}
/* Headset jack */
static struct snd_soc_jack hs_jack;

/*Headset jack detection DAPM pins */
static struct snd_soc_jack_pin hs_jack_pins[] = {
	{
		.pin = "Headset Mic",
		.mask = SND_JACK_MICROPHONE,
	},
	{
		.pin = "Headset Stereophone",
		.mask = SND_JACK_HEADPHONE,
	},
};

static int omap_abe_get_power_mode(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card = snd_kcontrol_chip(kcontrol);
	struct omap_abe_data *card_data = snd_soc_card_get_drvdata(card);

	ucontrol->value.integer.value[0] = card_data->twl6040_power_mode;
	return 0;
}

static int omap_abe_set_power_mode(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card = snd_kcontrol_chip(kcontrol);
	struct omap_abe_data *card_data = snd_soc_card_get_drvdata(card);

	if (card_data->twl6040_power_mode == ucontrol->value.integer.value[0])
		return 0;

	card_data->twl6040_power_mode = ucontrol->value.integer.value[0];
	omap_abe_pm_set_mode(card_data->abe_platform,
			     card_data->twl6040_power_mode);

	return 1;
}

static const char *power_texts[] = {"Low-Power", "High-Performance"};

static const struct soc_enum omap_abe_enum[] = {
	SOC_ENUM_SINGLE_EXT(2, power_texts),
};

static const struct snd_kcontrol_new omap_abe_controls[] = {
	SOC_ENUM_EXT("TWL6040 Power Mode", omap_abe_enum[0],
		omap_abe_get_power_mode, omap_abe_set_power_mode),
};

/* OMAP ABE TWL6040 machine DAPM */
static const struct snd_soc_dapm_widget twl6040_dapm_widgets[] = {
	/* Outputs */
	SND_SOC_DAPM_HP("Headset Stereophone", NULL),
	SND_SOC_DAPM_SPK("Earphone Spk", NULL),
	SND_SOC_DAPM_SPK("Ext Spk", NULL),
	SND_SOC_DAPM_LINE("Line Out", NULL),

	/* Inputs */
	SND_SOC_DAPM_MIC("Headset Mic", NULL),
	SND_SOC_DAPM_MIC("Main Handset Mic", NULL),
	SND_SOC_DAPM_MIC("Sub Handset Mic", NULL),
	SND_SOC_DAPM_LINE("Line In", NULL),
};

static const struct snd_soc_dapm_route audio_map[] = {
	/* Routings for outputs */
	{"Headset Stereophone", NULL, "HSOL"},
	{"Headset Stereophone", NULL, "HSOR"},

	{"Earphone Spk", NULL, "EP"},

	{"Ext Spk", NULL, "HFL"},
	{"Ext Spk", NULL, "HFR"},

	{"Line Out", NULL, "AUXL"},
	{"Line Out", NULL, "AUXR"},

	/* Routings for inputs */
	{"HSMIC", NULL, "Headset Mic Bias"},
	{"Headset Mic Bias", NULL, "Headset Mic"},

	{"MAINMIC", NULL, "Main Mic Bias"},
	{"Main Mic Bias", NULL, "Main Handset Mic"},

	{"SUBMIC", NULL, "Main Mic Bias"},
	{"Main Mic Bias", NULL, "Sub Handset Mic"},

	{"AFML", NULL, "Line In"},
	{"AFMR", NULL, "Line In"},

	/* Connections between twl6040 and ABE */
	{"Headset Playback", NULL, "PDM_DL1"},
	{"Handsfree Playback", NULL, "PDM_DL2"},
	{"PDM_UL1", NULL, "Capture"},

	/* Bluetooth <--> ABE*/
	{"omap-mcbsp.1 Playback", NULL, "BT_VX_DL"},
	{"BT_VX_UL", NULL, "omap-mcbsp.1 Capture"},

	/* FM <--> ABE */
	{"omap-mcbsp.2 Playback", NULL, "MM_EXT_DL"},
	{"MM_EXT_UL", NULL, "omap-mcbsp.2 Capture"},
};

static inline void twl6040_disconnect_pin(struct snd_soc_dapm_context *dapm,
					  int connected, char *pin)
{
	if (!connected)
		snd_soc_dapm_disable_pin(dapm, pin);
}

static int omap_abe_stream_event(struct snd_soc_dapm_context *dapm, int event)
{
	struct snd_soc_card *card = dapm->card;
	struct omap_abe_data *card_data = snd_soc_card_get_drvdata(card);
	int gain;

	/*
	 * set DL1 gains dynamically according to the active output
	 * (Headset, Earpiece) and HSDAC power mode
	 */

	gain = twl6040_get_dl1_gain(card_data->twl_codec) * 100;

	omap_abe_set_dl1_gains(card_data->abe_platform, gain, gain);

	return 0;
}

static int omap_abe_twl6040_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_card *card = codec->card;
	struct snd_soc_platform *platform = rtd->platform;
	struct omap_abe_twl6040_data *pdata = dev_get_platdata(card->dev);
	struct omap_abe_data *card_data = snd_soc_card_get_drvdata(card);
	u32 hsotrim, left_offset, right_offset, step_mV;
	int ret = 0;

	card_data->twl_codec = codec;
	card->dapm.stream_event = omap_abe_stream_event;

	/* allow audio paths from the audio modem to run during suspend */
	snd_soc_dapm_ignore_suspend(&card->dapm, "Ext Spk");
	snd_soc_dapm_ignore_suspend(&codec->dapm, "AFML");
	snd_soc_dapm_ignore_suspend(&codec->dapm, "AFMR");
	snd_soc_dapm_ignore_suspend(&card->dapm, "Headset Mic");
	snd_soc_dapm_ignore_suspend(&card->dapm, "Headset Stereophone");

	/* DC offset cancellation computation only if ABE is enabled */
	if (card_data->has_abe) {
		hsotrim = twl6040_get_trim_value(codec, TWL6040_TRIM_HSOTRIM);
		right_offset = TWL6040_HSF_TRIM_RIGHT(hsotrim);
		left_offset = TWL6040_HSF_TRIM_LEFT(hsotrim);

		step_mV = twl6040_get_hs_step_size(codec);
		omap_abe_dc_set_hs_offset(platform, left_offset, right_offset,
					  step_mV);

		/* ABE power control */
		ret = snd_soc_add_card_controls(card, omap_abe_controls,
			ARRAY_SIZE(omap_abe_controls));
		if (ret)
			return ret;
	}

	/* Headset jack detection only if it is supported */
	if (card_data->jack_detection) {
		ret = snd_soc_jack_new(codec, "Headset Jack",
					SND_JACK_HEADSET, &hs_jack);
		if (ret)
			return ret;

		ret = snd_soc_jack_add_pins(&hs_jack, ARRAY_SIZE(hs_jack_pins),
					hs_jack_pins);

		twl6040_hs_jack_detect(codec, &hs_jack, SND_JACK_HEADSET);
	}

	/*
	 * Should be only for SDP4430...
	 * but machine_is_* has been gone, so do it always
	 */
	if (1) {
		card_data->adapter = i2c_get_adapter(1);
		if (!card_data->adapter) {
			dev_err(card->dev, "can't get i2c adapter\n");
			return -ENODEV;
		}

		card_data->tps6130x = i2c_new_device(card_data->adapter,
				&tps6130x_hwmon_info);
		if (!card_data->tps6130x) {
			dev_err(card->dev, "can't add i2c device\n");
			i2c_put_adapter(card_data->adapter);
			return -ENODEV;
		}

		omap_abe_tps6130x_configure(card_data);
	}

	/*
	 * NULL pdata means we booted with DT. In this case the routing is
	 * provided and the card is fully routed, no need to mark pins.
	 */

	if (!pdata)
		return ret;

	/* Disable not connected paths if not used */
	twl6040_disconnect_pin(&codec->dapm, pdata->has_hs, "Headset Stereophone");
	twl6040_disconnect_pin(&codec->dapm, pdata->has_hf, "Ext Spk");
	twl6040_disconnect_pin(&codec->dapm, pdata->has_ep, "Earphone Spk");
	twl6040_disconnect_pin(&codec->dapm, pdata->has_aux, "Line Out");
	twl6040_disconnect_pin(&codec->dapm, pdata->has_vibra, "Vibrator");
	twl6040_disconnect_pin(&codec->dapm, pdata->has_hsmic, "Headset Mic");
	twl6040_disconnect_pin(&codec->dapm, pdata->has_mainmic, "Main Handset Mic");
	twl6040_disconnect_pin(&codec->dapm, pdata->has_submic, "Sub Handset Mic");
	twl6040_disconnect_pin(&codec->dapm, pdata->has_afm, "Line In");

	return ret;
}

static const struct snd_soc_dapm_widget dmic_dapm_widgets[] = {
	SND_SOC_DAPM_MIC("Digital Mic 0", NULL),
	SND_SOC_DAPM_MIC("Digital Mic 1", NULL),
	SND_SOC_DAPM_MIC("Digital Mic 2", NULL),
};

static const struct snd_soc_dapm_route dmic_audio_map[] = {
	/* Digital Mics: DMic0, DMic1, DMic2 with bias */
	{"DMIC0", NULL, "omap-dmic-abe.0 Capture"},
	{"omap-dmic-abe.0 Capture", NULL, "Digital Mic1 Bias"},
	{"Digital Mic1 Bias", NULL, "Digital Mic 0"},

	{"DMIC1", NULL, "omap-dmic-abe.1 Capture"},
	{"omap-dmic-abe.1 Capture", NULL, "Digital Mic1 Bias"},
	{"Digital Mic1 Bias", NULL, "Digital Mic 1"},

	{"DMIC2", NULL, "omap-dmic-abe.2 Capture"},
	{"omap-dmic-abe.2 Capture", NULL, "Digital Mic1 Bias"},
	{"Digital Mic1 Bias", NULL, "Digital Mic 2"},
};

static int omap_abe_dmic_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_card *card = rtd->card;
	struct snd_soc_dapm_context *dapm = &card->dapm;
	int ret;

	ret = snd_soc_dapm_new_controls(dapm, dmic_dapm_widgets,
				ARRAY_SIZE(dmic_dapm_widgets));
	if (ret)
		return ret;

	ret = snd_soc_dapm_add_routes(dapm, dmic_audio_map,
				ARRAY_SIZE(dmic_audio_map));
	if (ret < 0)
		return ret;

	snd_soc_dapm_ignore_suspend(dapm, "Digital Mic 0");
	snd_soc_dapm_ignore_suspend(dapm, "Digital Mic 1");
	snd_soc_dapm_ignore_suspend(dapm, "Digital Mic 2");
	return 0;
}

static int omap_abe_twl6040_dl2_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_platform *platform = rtd->platform;
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_card *card = codec->card;
	struct omap_abe_data *card_data = snd_soc_card_get_drvdata(card);
	u32 hfotrim, left_offset, right_offset;

	/* DC offset cancellation computation only if ABE is enabled */
	if (card_data->has_abe) {
		/* DC offset cancellation computation */
		hfotrim = twl6040_get_trim_value(codec, TWL6040_TRIM_HFOTRIM);
		right_offset = TWL6040_HSF_TRIM_RIGHT(hfotrim);
		left_offset = TWL6040_HSF_TRIM_LEFT(hfotrim);

		omap_abe_dc_set_hf_offset(platform, left_offset, right_offset);
	}

	return 0;
}

static int omap_abe_twl6040_fe_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_platform *platform = rtd->platform;
	struct snd_soc_card *card = rtd->card;
	struct omap_abe_data *card_data = snd_soc_card_get_drvdata(card);

	card_data->abe_platform = platform;

	return 0;
}

/* Digital audio interface glue - connects codec <--> CPU */
static struct snd_soc_dai_link legacy_dmic_dai = {
	/* Legacy DMIC */
	SND_SOC_DAI_CONNECT("Legacy DMIC", "dmic-codec", "omap-pcm-audio",
			    "dmic-hifi", "omap-dmic"),
	SND_SOC_DAI_OPS(&omap_abe_dmic_ops, omap_abe_dmic_init),
};

static struct snd_soc_dai_link legacy_mcpdm_dai = {
	/* Legacy McPDM */
	SND_SOC_DAI_CONNECT("Legacy McPDM", "twl6040-codec", "omap-pcm-audio",
			    "twl6040-legacy", "mcpdm-legacy"),
	SND_SOC_DAI_OPS(&omap_abe_mcpdm_ops, NULL),
};

static struct snd_soc_dai_link legacy_mcbsp_dai = {
	/* Legacy McBSP */
	SND_SOC_DAI_CONNECT("Legacy McBSP", "snd-soc-dummy", "omap-pcm-audio",
			    "snd-soc-dummy-dai", "omap-mcbsp.2"),
	SND_SOC_DAI_OPS(&omap_abe_mcbsp_ops, NULL),
	SND_SOC_DAI_IGNORE_SUSPEND,
};

static struct snd_soc_dai_link legacy_mcasp_dai = {
	/* Legacy SPDIF */
	SND_SOC_DAI_CONNECT("Legacy SPDIF", "spdif-dit", "omap-pcm-audio",
			    "dit-hifi", "mcasp-legacy"),
	SND_SOC_DAI_IGNORE_SUSPEND,
};

static struct snd_soc_dai_link abe_fe_dai[] = {

/*
 * Frontend DAIs - i.e. userspace visible interfaces (ALSA PCMs)
 */
{
	/* ABE Media Capture */
	SND_SOC_DAI_FE_LINK("OMAP ABE Media1", "omap-pcm-audio", "MultiMedia1"),
	SND_SOC_DAI_FE_TRIGGER(SND_SOC_DPCM_TRIGGER_BESPOKE, SND_SOC_DPCM_TRIGGER_BESPOKE),
	SND_SOC_DAI_OPS(NULL, omap_abe_twl6040_fe_init),
	SND_SOC_DAI_IGNORE_PMDOWN,
},
{
	/* ABE Media Capture */
	SND_SOC_DAI_FE_LINK("OMAP ABE Media2", "omap-pcm-audio", "MultiMedia2"),
	SND_SOC_DAI_FE_TRIGGER(SND_SOC_DPCM_TRIGGER_BESPOKE, SND_SOC_DPCM_TRIGGER_BESPOKE),
},
{
	/* ABE Voice */
	SND_SOC_DAI_FE_LINK("OMAP ABE Voice", "omap-pcm-audio", "Voice"),
	SND_SOC_DAI_FE_TRIGGER(SND_SOC_DPCM_TRIGGER_BESPOKE, SND_SOC_DPCM_TRIGGER_BESPOKE),
},
{
	/* ABE Tones */
	SND_SOC_DAI_FE_LINK("OMAP ABE Tones", "omap-pcm-audio", "Tones"),
	SND_SOC_DAI_FE_TRIGGER(SND_SOC_DPCM_TRIGGER_BESPOKE, SND_SOC_DPCM_TRIGGER_BESPOKE),
},
{
	/* MODEM */
	SND_SOC_DAI_FE_LINK("OMAP ABE MODEM", "aess", "MODEM"),
	SND_SOC_DAI_FE_TRIGGER(SND_SOC_DPCM_TRIGGER_BESPOKE, SND_SOC_DPCM_TRIGGER_BESPOKE),
	SND_SOC_DAI_OPS(NULL, omap_abe_twl6040_fe_init),
	SND_SOC_DAI_IGNORE_SUSPEND, SND_SOC_DAI_IGNORE_PMDOWN,
	SND_SOC_DAI_LINK_NO_HOST,
},
{
	/* Low power ping - pong */
	SND_SOC_DAI_FE_LINK("OMAP ABE Media LP", "aess", "MultiMedia1 LP"),
	SND_SOC_DAI_FE_TRIGGER(SND_SOC_DPCM_TRIGGER_BESPOKE, SND_SOC_DPCM_TRIGGER_BESPOKE),
},
};

/*
 * Backend DAIs - i.e. dynamically matched interfaces, invisible to userspace.
 * Matched to above interfaces at runtime, based upon use case.
 */

static struct snd_soc_dai_link abe_be_mcpdm_dai[] = {
{
	/* McPDM DL1 - Headset */
	SND_SOC_DAI_CONNECT("McPDM-DL1", "twl6040-codec", "aess",
			    "twl6040-dl1", "mcpdm-dl1"),
	SND_SOC_DAI_BE_LINK(OMAP_ABE_DAI_PDM_DL1, mcpdm_be_hw_params_fixup),
	SND_SOC_DAI_OPS(&omap_abe_mcpdm_ops, omap_abe_twl6040_init),
	SND_SOC_DAI_IGNORE_SUSPEND, SND_SOC_DAI_IGNORE_PMDOWN,
},
{
	/* McPDM UL1 - Analog Capture */
	SND_SOC_DAI_CONNECT("McPDM-UL1", "twl6040-codec", "aess",
			    "twl6040-ul", "mcpdm-ul1"),
	SND_SOC_DAI_BE_LINK(OMAP_ABE_DAI_PDM_UL, mcpdm_be_hw_params_fixup),
	SND_SOC_DAI_OPS(&omap_abe_mcpdm_ops, NULL),
	SND_SOC_DAI_IGNORE_SUSPEND, SND_SOC_DAI_IGNORE_PMDOWN,
},
{
	/* McPDM DL2 - Handsfree */
	SND_SOC_DAI_CONNECT("McPDM-DL2", "twl6040-codec", "aess",
			    "twl6040-dl2", "mcpdm-dl2"),
	SND_SOC_DAI_BE_LINK(OMAP_ABE_DAI_PDM_DL2, mcpdm_be_hw_params_fixup),
	SND_SOC_DAI_OPS(&omap_abe_mcpdm_ops, omap_abe_twl6040_dl2_init),
	SND_SOC_DAI_IGNORE_SUSPEND, SND_SOC_DAI_IGNORE_PMDOWN,
},
};

static struct snd_soc_dai_link abe_be_mcbsp1_dai = {
	/* McBSP 1 - Bluetooth */
	SND_SOC_DAI_CONNECT("McBSP-1", "snd-soc-dummy", "aess",
			    "snd-soc-dummy-dai", "omap-mcbsp.1"),
	SND_SOC_DAI_BE_LINK(OMAP_ABE_DAI_BT_VX,	mcbsp_be_hw_params_fixup),
	SND_SOC_DAI_OPS(&omap_abe_mcbsp_ops, NULL),
	SND_SOC_DAI_IGNORE_SUSPEND, SND_SOC_DAI_IGNORE_PMDOWN,
};

static struct snd_soc_dai_link abe_be_mcbsp2_dai = {
	/* McBSP 2 - MODEM or FM */
	SND_SOC_DAI_CONNECT("McBSP-2", "snd-soc-dummy", "aess",
			    "snd-soc-dummy-dai", "omap-mcbsp.2"),
	SND_SOC_DAI_BE_LINK(OMAP_ABE_DAI_MM_FM,	mcbsp_be_hw_params_fixup),
	SND_SOC_DAI_OPS(&omap_abe_mcbsp_ops, NULL),
	SND_SOC_DAI_IGNORE_SUSPEND, SND_SOC_DAI_IGNORE_PMDOWN,
};

static struct snd_soc_dai_link abe_be_dmic_dai[] = {
{
	/* DMIC0 */
	SND_SOC_DAI_CONNECT("DMIC-0", "dmic-codec", "aess",
			    "dmic-hifi", "omap-dmic-abe-dai-0"),
	SND_SOC_DAI_BE_LINK(OMAP_ABE_DAI_DMIC0,	dmic_be_hw_params_fixup),
	SND_SOC_DAI_OPS(&omap_abe_dmic_ops, NULL),
},
{
	/* DMIC1 */
	SND_SOC_DAI_CONNECT("DMIC-1", "dmic-codec", "aess",
			    "dmic-hifi", "omap-dmic-abe-dai-1"),
	SND_SOC_DAI_BE_LINK(OMAP_ABE_DAI_DMIC1,	dmic_be_hw_params_fixup),
	SND_SOC_DAI_OPS(&omap_abe_dmic_ops, NULL),
},
{
	/* DMIC2 */
	SND_SOC_DAI_CONNECT("DMIC-2", "dmic-codec", "aess",
			    "dmic-hifi", "omap-dmic-abe-dai-2"),
	SND_SOC_DAI_BE_LINK(OMAP_ABE_DAI_DMIC2,	dmic_be_hw_params_fixup),
	SND_SOC_DAI_OPS(&omap_abe_dmic_ops, NULL),
},
};

/* TODO: Peter - this will need some logic for DTS DAI link creation */
static int omap_abe_add_dai_links(struct snd_soc_card *card,
	struct omap_abe_twl6040_data *pdata)
{
	struct omap_abe_data *card_data = snd_soc_card_get_drvdata(card);
	struct device_node *node = card->dev->of_node;
	int ret, i;


	if (node) {
		struct device_node *dai_node, *aess_node;

		aess_node = of_parse_phandle(node, "ti,aess", 0);
		if (!aess_node) {
			dev_err(card->dev, "AESS node is not provided\n");
			return -EINVAL;
		}

		for (i = 4; i < ARRAY_SIZE(abe_fe_dai); i++) {
			abe_fe_dai[i].platform_name  = NULL;
			abe_fe_dai[i].platform_of_node = aess_node;
		}

		dai_node = of_parse_phandle(node, "ti,mcpdm", 0);
		if (!dai_node) {
				dev_err(card->dev, "McPDM node is not provided\n");
				return -EINVAL;
		}

		for (i = 0; i < ARRAY_SIZE(abe_be_mcpdm_dai); i++) {
			abe_be_mcpdm_dai[i].platform_name  = NULL;
			abe_be_mcpdm_dai[i].platform_of_node = aess_node;
		}

		for (i = 0; i < ARRAY_SIZE(abe_be_dmic_dai); i++) {
			abe_be_dmic_dai[i].platform_name  = NULL;
			abe_be_dmic_dai[i].platform_of_node = aess_node;
		}

		dai_node = of_parse_phandle(node, "ti,mcbsp1", 0);
		if (!dai_node) {
			dev_err(card->dev,"McBSP1 node is not provided\n");
			return -EINVAL;
		}
		abe_be_mcbsp1_dai.cpu_dai_name  = NULL;
		abe_be_mcbsp1_dai.cpu_of_node = dai_node;
		abe_be_mcbsp1_dai.platform_name  = NULL;
		abe_be_mcbsp1_dai.platform_of_node = aess_node;

		dai_node = of_parse_phandle(node, "ti,mcbsp2", 0);
		if (!dai_node) {
			dev_err(card->dev,"McBSP2 node is not provided\n");
			return -EINVAL;
		}
		abe_be_mcbsp2_dai.cpu_dai_name  = NULL;
		abe_be_mcbsp2_dai.cpu_of_node = dai_node;
		abe_be_mcbsp2_dai.platform_name  = NULL;
		abe_be_mcbsp2_dai.platform_of_node = aess_node;
	}

	/* Add the ABE FEs */
	ret = snd_soc_card_new_dai_links(card, abe_fe_dai,
		ARRAY_SIZE(abe_fe_dai));
	if (ret < 0)
		return ret;

	/* McPDM BEs */
	ret = snd_soc_card_new_dai_links(card, abe_be_mcpdm_dai,
		ARRAY_SIZE(abe_be_mcpdm_dai));
	if (ret < 0)
		return ret;

	/* McBSP1 BEs */
	ret = snd_soc_card_new_dai_links(card, &abe_be_mcbsp1_dai, 1);
	if (ret < 0)
		return ret;

	/* McBSP2 BEs */
	ret = snd_soc_card_new_dai_links(card, &abe_be_mcbsp2_dai, 1);
	if (ret < 0)
		return ret;
	/* DMIC BEs */
	if (card_data->has_dmic) {
		ret = snd_soc_card_new_dai_links(card, abe_be_dmic_dai,
			ARRAY_SIZE(abe_be_dmic_dai));
		if (ret < 0)
			return ret;
	}

	return 0;
}

/* TODO: Peter - this will need some logic for DTS DAI link creation */
static int omap_abe_add_legacy_dai_links(struct snd_soc_card *card,
	struct omap_abe_twl6040_data *pdata)
{
	struct omap_abe_data *card_data = snd_soc_card_get_drvdata(card);
	struct device_node *node = card->dev->of_node;
	struct device_node *dai_node;
	int ret;

	if (node) {
		dai_node = of_parse_phandle(node, "ti,mcpdm", 0);
		if (!dai_node) {
				dev_err(card->dev, "McPDM node is not provided\n");
				return -EINVAL;
		}

		dai_node = of_parse_phandle(node, "ti,mcbsp2", 0);
		if (!dai_node) {
			dev_err(card->dev,"McBSP2 node is not provided\n");
			return -EINVAL;
		}
		legacy_mcbsp_dai.cpu_dai_name  = NULL;
		legacy_mcbsp_dai.cpu_of_node = dai_node;

	}
	/* Add the Legacy McPDM */
	ret = snd_soc_card_new_dai_links(card, &legacy_mcpdm_dai, 1);
	if (ret < 0)
		return ret;

	/* Add the Legacy McBSP */
	ret = snd_soc_card_new_dai_links(card, &legacy_mcbsp_dai, 1);
	if (ret < 0)
		return ret;

	/* Add the Legacy McASP */
	ret = snd_soc_card_new_dai_links(card, &legacy_mcasp_dai, 1);
	if (ret < 0)
		return ret;

	/* Add the Legacy DMICs */
	if (card_data->has_dmic) {
		ret = snd_soc_card_new_dai_links(card, &legacy_dmic_dai, 1);
		if (ret < 0)
			return ret;
	}

	return 0;
}

/* Audio machine driver */
static struct snd_soc_card omap_abe_card = {
	.owner = THIS_MODULE,

	.dapm_widgets = twl6040_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(twl6040_dapm_widgets),
	.dapm_routes = audio_map,
	.num_dapm_routes = ARRAY_SIZE(audio_map),

};

static int omap_abe_probe(struct platform_device *pdev)
{
	struct omap_abe_twl6040_data *pdata = dev_get_platdata(&pdev->dev);
	struct device_node *node = pdev->dev.of_node;
	struct snd_soc_card *card = &omap_abe_card;
	struct omap_abe_data *card_data;
	int ret;

	card->dev = &pdev->dev;

	card_data = devm_kzalloc(&pdev->dev, sizeof(*card_data), GFP_KERNEL);
	if (card_data == NULL)
		return -ENOMEM;

	card_data->dmic_codec_dev = ERR_PTR(-EINVAL);
	card_data->spdif_codec_dev = ERR_PTR(-EINVAL);

	if (node) {
		struct device_node *dai_node;

		if (snd_soc_of_parse_card_name(card, "ti,model")) {
			dev_err(&pdev->dev, "Card name is not provided\n");
			return -ENODEV;
		}

		ret = snd_soc_of_parse_audio_routing(card,
						"ti,audio-routing");
		if (ret) {
			dev_err(&pdev->dev,
				"Error while parsing DAPM routing\n");
			return ret;
		}

		dai_node = of_parse_phandle(node, "ti,mcpdm", 0);
		if (!dai_node) {
			dev_err(&pdev->dev, "McPDM node is not provided\n");
			return -EINVAL;
		}

		dai_node = of_parse_phandle(node, "ti,aess", 0);
		if (dai_node) {
			card_data->has_abe = 1;
		} else {
			dev_dbg(&pdev->dev, "AESS node is not provided\n");
			card_data->has_abe = 0;
		}

		dai_node = of_parse_phandle(node, "ti,dmic", 0);
		if (dai_node) {
			card_data->has_dmic = 1;

			card_data->dmic_codec_dev =
					platform_device_register_simple(
						"dmic-codec", -1, NULL, 0);
			if (IS_ERR(card_data->dmic_codec_dev)) {
				dev_err(&pdev->dev,
					"Can't instantiate dmic-codec\n");
				return PTR_ERR(card_data->dmic_codec_dev);
			}
		} else {
			dev_dbg(&pdev->dev, "DMIC node is not provided\n");
			card_data->has_dmic = 0;
		}

		card_data->spdif_codec_dev = platform_device_register_simple(
						"spdif-dit", -1, NULL, 0);
		if (IS_ERR(card_data->spdif_codec_dev)) {
			dev_err(&pdev->dev,
				"Can't instantiate spdif-dit\n");
			ret = PTR_ERR(card_data->spdif_codec_dev);
			goto err_dmic_unregister;
		}

		of_property_read_u32(node, "ti,jack-detection",
				     &card_data->jack_detection);
		of_property_read_u32(node, "ti,mclk-freq",
				     &card_data->mclk_freq);
		if (!card_data->mclk_freq) {
			dev_err(&pdev->dev, "MCLK frequency not provided\n");
			ret = -EINVAL;
			goto err_unregister;
		}

		omap_abe_card.fully_routed = 1;
	} else if (pdata) {
		if (pdata->card_name) {
			card->name = pdata->card_name;
		} else {
			dev_err(&pdev->dev, "Card name is not provided\n");
			return -ENODEV;
		}

		card_data->jack_detection = pdata->jack_detection;
		card_data->mclk_freq = pdata->mclk_freq;
		card_data->has_abe = pdata->has_abe;
		card_data->has_dmic = pdata->has_dmic;
	} else {
		dev_err(&pdev->dev, "Missing pdata\n");
		return -ENODEV;
	}

	if (!card_data->mclk_freq) {
		dev_err(&pdev->dev, "MCLK frequency missing\n");
		ret = -EINVAL;
		goto err_unregister;
	}

	snd_soc_card_set_drvdata(card, card_data);

	if (card_data->has_abe) {
		ret = omap_abe_add_dai_links(card, pdata);
		if (ret < 0)
			goto err_unregister;
	}

	ret = omap_abe_add_legacy_dai_links(card, pdata);
	if (ret < 0)
		goto err_unregister;

	ret = snd_soc_register_card(card);
	if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_card() failed: %d\n", ret);
		goto err_unregister;
	}

	return ret;

err_unregister:
	if (!IS_ERR(card_data->spdif_codec_dev))
		platform_device_unregister(card_data->spdif_codec_dev);
err_dmic_unregister:
	if (!IS_ERR(card_data->dmic_codec_dev))
		platform_device_unregister(card_data->dmic_codec_dev);

	snd_soc_card_reset_dai_links(card);
	return ret;
}

static int omap_abe_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	struct omap_abe_data *card_data = snd_soc_card_get_drvdata(card);

	snd_soc_unregister_card(card);
	i2c_unregister_device(card_data->tps6130x);
	i2c_put_adapter(card_data->adapter);

	if (!IS_ERR(card_data->dmic_codec_dev))
		platform_device_unregister(card_data->dmic_codec_dev);
	if (!IS_ERR(card_data->spdif_codec_dev))
		platform_device_unregister(card_data->spdif_codec_dev);
	snd_soc_card_reset_dai_links(card);

	return 0;
}

static const struct of_device_id omap_abe_of_match[] = {
	{.compatible = "ti,abe-twl6040", },
	{ },
};
MODULE_DEVICE_TABLE(of, omap_abe_of_match);

static struct platform_driver omap_abe_driver = {
	.driver = {
		.name = "omap-abe-twl6040",
		.owner = THIS_MODULE,
		.pm = &snd_soc_pm_ops,
		.of_match_table = omap_abe_of_match,
	},
	.probe = omap_abe_probe,
	.remove = omap_abe_remove,
};

module_platform_driver(omap_abe_driver);

MODULE_AUTHOR("Misael Lopez Cruz <misael.lopez@ti.com>");
MODULE_DESCRIPTION("ALSA SoC for OMAP boards with ABE and twl6040 codec");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:omap-abe-twl6040");
