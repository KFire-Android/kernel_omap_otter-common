/*
 * bowser.c  --  SoC audio for TI OMAP4430 SDP
 *
 * Author: Misael Lopez Cruz <x0052729@ti.com>
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
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/input.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/jack.h>
#include <sound/pcm_params.h>
#include <sound/soc-dapm.h>
#include <sound/soc-dpcm.h>

#include <asm/mach-types.h>
#include <plat/hardware.h>
#include <plat/mux.h>

#include "omap-dmic.h"
#include "omap-mcpdm.h"
#include "omap-pcm.h"
#include "omap-abe-priv.h"
#include "mcbsp.h"
#include "omap-mcbsp.h"
#include "omap-dmic.h"

#include <sound/max98090.h>
#include <sound/max97236.h>

#ifdef CONFIG_SND_OMAP_SOC_HDMI
#include "omap-hdmi.h"
#endif

#define DEBUG
#define ALSA_DEBUG
#include "bowser_alsa_debug.h"

#define MCLK_RATE 19200000
#define SYS_CLK_RATE	MCLK_RATE

static struct clk *mclk;
static unsigned int fll_clk = SYS_CLK_RATE;
static unsigned int sys_clk = SYS_CLK_RATE;

/*static enum snd_soc_bias_level bias_level = SND_SOC_BIAS_OFF;*/
static struct snd_soc_jack bowser_jack;

static int bowser_suspend_post(struct snd_soc_card *card)
{

	if (mclk != NULL)
		clk_disable(mclk);

	return 0;
}

static int bowser_resume_pre(struct snd_soc_card *card)
{
	int ret = 0;

	if (mclk != NULL)
		clk_enable(mclk);

	return ret;
}

static int bowser_max98090_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	int ret = 0;
	struct snd_soc_dai *codec_dai = rtd->codec_dai, *cpu_dai = rtd->cpu_dai;

	dev_info(card->dev, "%s() - enter\n", __func__);

	sys_clk = fll_clk = SYS_CLK_RATE;

	ret = snd_soc_dai_set_sysclk(codec_dai, 0, sys_clk, 0);
	if (ret < 0) {
		dev_err(codec_dai->dev,
			"Failed to set CODEC SYSCLK=%d; err=%d\n",
			sys_clk, ret);
		return ret;
	}
	ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S
			| SND_SOC_DAIFMT_CBM_CFM | SND_SOC_DAIFMT_NB_NF);
	if (ret < 0) {
		dev_err(codec_dai->dev,
			"Failed to set CODEC DAI format: %d\n", ret);
		return ret;
	}

	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S
			| SND_SOC_DAIFMT_CBM_CFM | SND_SOC_DAIFMT_NB_NF);
	if (ret < 0) {
		dev_err(cpu_dai->dev,
			"Failed to set CPU DAI format: %d\n", ret);
		return ret;
	}

	dev_dbg(card->dev, "%s() - exit\n", __func__);

	return 0;
}

static struct snd_soc_ops bowser_max98090_abe_ops = {
	.hw_params = bowser_max98090_hw_params,
};

static int bowser_max98090_mcbsp_hw_params(struct snd_pcm_substream *substream,
					struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct omap_mcbsp *mcbsp = snd_soc_dai_get_drvdata(cpu_dai);
	int ret = 0;

	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_DSP_C |
					   SND_SOC_DAIFMT_NB_NF |
					   SND_SOC_DAIFMT_CBM_CFM);

	if (ret < 0) {
		dev_err(cpu_dai->dev, "Can't set cpu DAI configuration\n");
		return ret;
	}

	omap_mcbsp_set_tx_threshold(mcbsp, 1);

	return ret;
}

static struct snd_soc_ops bowser_max98090_bt_ops = {
	.hw_params = bowser_max98090_mcbsp_hw_params,
};

static const struct snd_soc_dapm_widget bowser_dapm_widgets[] = {
	SND_SOC_DAPM_HP("HP", NULL),
	SND_SOC_DAPM_SPK("SPK", NULL),
	SND_SOC_DAPM_MIC("MIC", NULL),
};

static const struct snd_kcontrol_new bowser_controls[] = {
	SOC_DAPM_PIN_SWITCH("HP"),
	SOC_DAPM_PIN_SWITCH("SPK"),
	SOC_DAPM_PIN_SWITCH("MIC"),
};

static const struct snd_soc_dapm_route bowser_dapm_routes[] = {
	/* Define HP / SPK output path */
	/* HP switch -> max97237 HP output pin */
	{ "HP", NULL, "AMP_HPL" },
	{ "HP", NULL, "AMP_HPR" },

	/*max98090 output pin -> max97236 HP input pin */
	{ "AMP_INL", NULL, "HPL"},
	{ "AMP_INR", NULL, "HPR"},

	/* SPK switch -> max98090 SPK output pin */
	{ "SPK", NULL, "SPKL" },
	{ "SPK", NULL, "SPKR" },

	{ "AIFINL", NULL, "MM_EXT_DL" },
	{ "AIFINR", NULL, "MM_EXT_DL" },

	/* FM <--> ABE */
	{"omap-mcbsp.2 Playback", NULL, "MM_EXT_DL"},
	{"MM_EXT_UL", NULL, "omap-mcbsp.2 Capture"},

	/* Bluetooth <--> ABE*/
	{"omap-mcbsp.3 Playback", NULL, "BT_VX_DL"},
	{"BT_VX_UL", NULL, "omap-mcbsp.3 Capture"},

	/* Define microphone input path */
	/* MIC switch -> max97236 input */
	{ "AMP_MIC", NULL, "MIC" },

	/* max97237 mic output -> max98090 mic input */
	{ "IN34", NULL, "AMP_MOUT" },
};

static int bowser_jack_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_codec *codec = rtd->codec;
	int ret;

	/* Disable folowing pins at init time for power save
	 * They will be enabled if is requared later */
	snd_soc_dapm_disable_pin(&codec->dapm, "AMP_HPL");
	snd_soc_dapm_disable_pin(&codec->dapm, "AMP_HPR");
	snd_soc_dapm_disable_pin(&codec->dapm, "AMP_MIC");

	snd_soc_dapm_sync(&codec->dapm);

	max97236_set_key_div(codec, MCLK_RATE);

	ret = snd_soc_jack_new(codec, "h2w",
			SND_JACK_BTN_0 | SND_JACK_BTN_1 | SND_JACK_BTN_2 |
			SND_JACK_BTN_3 | SND_JACK_BTN_4 | SND_JACK_BTN_5 |
			SND_JACK_HEADSET | SND_JACK_LINEOUT, &bowser_jack);
	if (ret) {
		dev_err(codec->dev, "Failed to create jack: %d\n", ret);
		return ret;
	}

	snd_jack_set_key(bowser_jack.jack, SND_JACK_BTN_0, KEY_MEDIA);

	max97236_set_jack(codec, &bowser_jack);

	max97236_detect_jack(codec);

	return 0;
}

static int mcbsp_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
		struct snd_pcm_hw_params *params) {
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_interval *channels = hw_param_interval(params,
			SNDRV_PCM_HW_PARAM_CHANNELS);
	struct snd_interval *rate = hw_param_interval(params,
			SNDRV_PCM_HW_PARAM_RATE);
	unsigned int be_id = rtd->dai_link->be_id;
	struct omap_mcbsp *mcbsp = snd_soc_dai_get_drvdata(cpu_dai);
	unsigned int threshold;

	switch (be_id) {
	case OMAP_ABE_DAI_MM_FM:
		channels->min = 2;
		threshold = 2;
		break;
	case OMAP_ABE_DAI_BT_VX:
		channels->min = 1;
		rate->min = rate->max = 8000;
		threshold = 1;
		break;
	default:
		threshold = 1;
		break;
	}

	snd_mask_set(&params->masks[SNDRV_PCM_HW_PARAM_FORMAT -
				SNDRV_PCM_HW_PARAM_FIRST_MASK],
				SNDRV_PCM_FORMAT_S16_LE);

	omap_mcbsp_set_tx_threshold(mcbsp, threshold);
	omap_mcbsp_set_rx_threshold(mcbsp, threshold);

	return 0;
}

/* Digital audio interface glue - connects codec <--> CPU */
static struct snd_soc_dai_link bowser_dai[] = {
	{
		.name = "max98090-lp",
		.stream_name = "Multimedia",
		/* ABE components - MM-DL (mmap) */
		.cpu_dai_name = "MultiMedia1 LP",
		.platform_name = "aess",
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_pmdown_time = true,

		.dynamic = 1, /* BE is dynamic */
		.trigger = {
				SND_SOC_DPCM_TRIGGER_BESPOKE,
				SND_SOC_DPCM_TRIGGER_BESPOKE
			   },
	},
	{
		.name = "max98090-mm",
		.stream_name = "Multimedia",
		/* ABE components - MM-UL & MM_DL */
		.cpu_dai_name = "MultiMedia1",
		.platform_name = "omap-pcm-audio",
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",

		.dynamic = 1, /* BE is dynamic */
		.trigger = {
				SND_SOC_DPCM_TRIGGER_BESPOKE,
				SND_SOC_DPCM_TRIGGER_BESPOKE
			   },
	},
	{
		.name = "max98090-capture",
		.stream_name = "Multimedia Capture",

		/* ABE components - MM-UL2 */
		.cpu_dai_name = "MultiMedia2",
		.platform_name = "omap-pcm-audio",
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",

		.dynamic = 1, /* BE is dynamic */
		.trigger = {
				SND_SOC_DPCM_TRIGGER_BESPOKE,
				SND_SOC_DPCM_TRIGGER_BESPOKE
			   },
	},
	{
		.name = "max98090-tone",
		.stream_name = "Tone Playback",

		/* ABE components - TONES_DL */
		.cpu_dai_name = "Tones",
		.platform_name = "omap-pcm-audio",
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_pmdown_time = true,

		.dynamic = 1, /* BE is dynamic */
		.trigger = {
				SND_SOC_DPCM_TRIGGER_BESPOKE,
				SND_SOC_DPCM_TRIGGER_BESPOKE
			   },
	},
	{
		.name = "max98090-noabe",
		.stream_name = "max98090",
		.cpu_dai_name = "omap-mcbsp.2", /* McBSP2 */
		.platform_name = "omap-pcm-audio",
		.codec_dai_name = "HiFi",
		.codec_name = "max98090.3-0010",
		.ops = &bowser_max98090_abe_ops,
	},
	/*
	 * Backend DAIs - i.e. dynamically matched interfaces, invisible
	 * touserspace. Matched to above interfaces at runtime,
	 * based upon use case.
	 */
	{
		.name = OMAP_ABE_BE_MM_EXT0_DL,
		.stream_name = "FM Playback",

		/* ABE components - MCBSP2 - MM-EXT */
		.cpu_dai_name = "omap-mcbsp.2",
		.platform_name = "aess",

		/* FM */
		.codec_dai_name = "HiFi",
		.codec_name = "max98090.3-0010",

		.no_pcm = 1, /* don't create ALSA pcm for this */
		.be_hw_params_fixup = mcbsp_be_hw_params_fixup,
		.ops = &bowser_max98090_abe_ops,
		.be_id = OMAP_ABE_DAI_MM_FM,
		.ignore_pmdown_time = true,
	},
	{
		.name = OMAP_ABE_BE_MM_EXT0_UL,
		.stream_name = "FM Capture",
		/* ABE components - MCBSP2 - MM-EXT */
		.cpu_dai_name = "omap-mcbsp.2",
		.platform_name = "aess",

		/* FM */
		.codec_dai_name = "HiFi",
		.codec_name = "max98090.3-0010",

		.no_pcm = 1, /* don't create ALSA pcm for this */
		.be_hw_params_fixup = mcbsp_be_hw_params_fixup,
		.ops = &bowser_max98090_abe_ops,
		.be_id = OMAP_ABE_DAI_MM_FM,
	},
	{
		.name = OMAP_ABE_BE_BT_VX_UL,
		.stream_name = "BT Capture",

		/* ABE components - MCBSP3 - BT-VX */
		.cpu_dai_name = "omap-mcbsp.3",
		.platform_name = "aess",

		/* Bluetooth */
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",

		.no_pcm = 1, /* don't create ALSA pcm for this */
		.be_hw_params_fixup = mcbsp_be_hw_params_fixup,
		.ops = &bowser_max98090_bt_ops,
		.be_id = OMAP_ABE_DAI_BT_VX,
		.ignore_suspend = 1,
	},
	{
		.name = OMAP_ABE_BE_BT_VX_DL,
		.stream_name = "BT Playback",

		/* ABE components - MCBSP3 - BT-VX */
		.cpu_dai_name = "omap-mcbsp.3",
		.platform_name = "aess",

		/* Bluetooth */
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",

		.no_pcm = 1, /* don't create ALSA pcm for this */
		.ignore_pmdown_time = 1, /* Power down without delay */
		.be_hw_params_fixup = mcbsp_be_hw_params_fixup,
		.ops = &bowser_max98090_bt_ops,
		.be_id = OMAP_ABE_DAI_BT_VX,
		.ignore_suspend = 1,
	},
	{
		.name = "max97236-hpamp",
		.stream_name = "max97236",

		.cpu_dai_name = "snd-soc-dummy-dai",
		.platform_name = "snd-soc-dummy",

		.codec_dai_name = "max97236-hifi",
		.codec_name = "max97236.3-0040",

		.init = &bowser_jack_init,

		.no_pcm = 1, /* don't create ALSA pcm for this */
	},
};

/* Audio machine driver */
static struct snd_soc_card bowser_maxim_card = {
	.owner = THIS_MODULE,

	.name = "bowser",
	.long_name = "TI OMAP4 bowser Board",

	.dai_link = bowser_dai,
	.num_links = ARRAY_SIZE(bowser_dai),

	.suspend_post = bowser_suspend_post,
	.resume_pre = bowser_resume_pre,

	.dapm_widgets = bowser_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(bowser_dapm_widgets),
	.dapm_routes = bowser_dapm_routes,
	.num_dapm_routes = ARRAY_SIZE(bowser_dapm_routes),
	.controls = bowser_controls,
	.num_controls = ARRAY_SIZE(bowser_controls),
};

static __devinit int bowser_maxim_probe(struct platform_device *pdev)
{
	struct max98090_pdata *pdata = dev_get_platdata(&pdev->dev);
	struct snd_soc_card *card = &bowser_maxim_card;
	int ret;

	card->dev = &pdev->dev;

	dev_dbg(&pdev->dev,
		"%s: [AP] codec platform driver init: enter\n", __func__);

	mclk = clk_get(NULL, "auxclk0_ck");
	if (IS_ERR(mclk)) {
		dev_err(&pdev->dev, "Failed to get AUXCLK0 CLK: %ld\n",
				PTR_ERR(mclk));
		ret = -ENODEV;
		goto done;
	}

	dev_dbg(&pdev->dev, "Old codec mclk rate = %lu\n", clk_get_rate(mclk));

	ret = clk_set_rate(mclk, MCLK_RATE);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to set MCLK rate: %d\n", ret);
		goto clk_err;
	}

	dev_dbg(&pdev->dev, "New codec mclk rate = %lu\n", clk_get_rate(mclk));

	ret = clk_enable(mclk);

	dev_dbg(&pdev->dev, "MCLK enable: ret=%d\n", ret);

	if (!pdata) {
		dev_err(&pdev->dev, "Missing pdata\n");
		return -ENODEV;
	}

	ret = snd_soc_register_card(card);
	if (ret)
		dev_err(&pdev->dev,
			"%s: snd_soc_register_card() failed: %d\n",
			__func__, ret);

	goto done;

clk_err: clk_put(mclk);
done:
	dev_err(&pdev->dev, "codec platfrom driver init done: err=%d\n", ret);

	return ret;
}

static int __devexit bowser_maxim_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	snd_soc_unregister_card(card);
	return 0;
}

static void bowser_maxim_shutdown(struct platform_device *pdev)
{
	snd_soc_poweroff(&pdev->dev);
}

static struct platform_driver bowser_maxim_driver = {
	.driver = {
		.name = "bowser-maxim",
		.owner = THIS_MODULE,
		.pm = &snd_soc_pm_ops,
	},
	.probe = bowser_maxim_probe,
	.remove = __devexit_p(bowser_maxim_remove),
	.shutdown = bowser_maxim_shutdown,
};

module_platform_driver(bowser_maxim_driver);

MODULE_AUTHOR("Misael Lopez Cruz <misael.lopez@ti.com>");
MODULE_DESCRIPTION("ALSA SoC for Bowser7+ boards with ABE and max98090 codec");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:bowser-max98090");

