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
#include <linux/i2c.h>
#include <linux/i2c/twl.h>
#include <linux/regulator/consumer.h>
#include <linux/delay.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/jack.h>
#include <asm/mach-types.h>
#include <plat/hardware.h>
#include <plat/mux.h>

#include "mcbsp.h"
#include "omap-mcpdm.h"
#include "omap-abe-priv.h"

#include "omap-pcm.h"
#include "omap-mcbsp.h"
#include "omap-dmic.h"

#include "../codecs/wm8962.h"
/*
#ifdef CONFIG_SND_OMAP_SOC_HDMI
#include "omap-hdmi.h"
#endif
*/

#define ALSA_DEBUG

#define WM8962_MCLK_RATE 19200000

#ifdef CONFIG_ABE_44100
#define WM8962_SYS_CLK_RATE	(44100 * 512)
#else
#define WM8962_SYS_CLK_RATE	(48000 * 512)
#endif


struct bowser_abe_data {
	struct snd_soc_platform *abe_platform;
	struct snd_soc_jack bowser_jack;
	struct snd_soc_dai *codec_dai;
	struct clk *wm8962_mclk;
	enum snd_soc_bias_level bias_level;
};

static unsigned int fll_clk = WM8962_SYS_CLK_RATE;
static unsigned int sys_clk = WM8962_SYS_CLK_RATE;

static int bowser_set_bias_level(struct snd_soc_card *card,
				 struct snd_soc_dapm_context *dapm,
				 enum snd_soc_bias_level level)
{
	struct bowser_abe_data *bowser_data = snd_soc_card_get_drvdata(card);
	struct snd_soc_dai *codec_dai = bowser_data->codec_dai;
	int ret;

	if (codec_dai == NULL) {
		pr_debug("codec_dai not initialized yet\n");
		return 0;
	}

	if (codec_dai->dev == NULL) {
		pr_debug("no run time codec_dai initialized yet\n");
		return 0;
	}

	dev_dbg(codec_dai->dev, "Setting bias %d\n", level);

	if (dapm->dev != codec_dai->dev) {
		dev_dbg(dapm->dev, "dapm->dev!=codec_dai->dev\n");
		return 0;
	}

	switch (level) {
	case SND_SOC_BIAS_ON:
		dev_dbg(dapm->dev, "%s() - SND_SOC_BIAS_ON\n", __func__);
		break;
	case SND_SOC_BIAS_PREPARE:
		dev_dbg(dapm->dev, "%s() - SND_SOC_BIAS_PREPARE\n", __func__);
		break;
	case SND_SOC_BIAS_STANDBY:
		dev_dbg(dapm->dev, "%s() - SND_SOC_BIAS_STANDBY\n", __func__);
		if (bowser_data->bias_level == SND_SOC_BIAS_OFF) {
			dev_dbg(dapm->dev,
			"%s() - SND_SOC_BIAS_STANDBY and enable all clocks\n",
			__func__);

			ret = clk_enable(bowser_data->wm8962_mclk);
			if (ret < 0) {
				dev_err(codec_dai->dev,
					"Failed to enable MCLK: %d\n", ret);
				return ret;
			}

			ret = snd_soc_dai_set_pll(codec_dai, WM8962_FLL,
							WM8962_FLL_MCLK,
							WM8962_MCLK_RATE,
							fll_clk);
			if (ret < 0) {
				dev_err(codec_dai->dev,
					"Failed to start CODEC FLL: %d\n",
					ret);
				return ret;
			}

			ret = snd_soc_dai_set_sysclk(codec_dai,
						     WM8962_SYSCLK_FLL,
						     sys_clk, 0);
			if (ret < 0) {
				dev_err(codec_dai->dev,
					"Failed to set CODEC SYSCLK: %d\n",
					ret);
				return ret;
			}
		}
		break;
	default:
		break;
	}

	return 0;
}

static int bowser_set_bias_level_post(struct snd_soc_card *card,
				      struct snd_soc_dapm_context *dapm,
				      enum snd_soc_bias_level level)
{
	struct bowser_abe_data *bowser_data = snd_soc_card_get_drvdata(card);
	struct snd_soc_dai *codec_dai = bowser_data->codec_dai;

	int ret;

	if (codec_dai == NULL) {
		pr_debug("codec_dai not initialized yet\n");
		return 0;
	}
	if (codec_dai->dev == NULL) {
		pr_debug("no run time codec_dai initialized yet\n");
		return 0;
	}
	dev_dbg(codec_dai->dev, "Setting bias post %d\n", level);

	if (dapm->dev != codec_dai->dev) {
		dev_dbg(dapm->dev, "dapm->dev!=codec_dai->dev\n");
		return 0;
	}

	switch (level) {
	case SND_SOC_BIAS_ON:
		dev_dbg(dapm->dev, "%s() - SND_SOC_BIAS_ON\n", __func__);
		break;
	case SND_SOC_BIAS_PREPARE:
		dev_dbg(dapm->dev, "%s() - SND_SOC_BIAS_PREPARE\n", __func__);
		break;
	case SND_SOC_BIAS_STANDBY:
		dev_dbg(dapm->dev, "%s() - SND_SOC_BIAS_STANDBY\n", __func__);
		break;
	case SND_SOC_BIAS_OFF:
		dev_dbg(dapm->dev, "%s() - SND_SOC_BIAS_OFF\n", __func__);
		ret = snd_soc_dai_set_sysclk(codec_dai,
					     WM8962_SYSCLK_MCLK,
					     sys_clk, 0);
		if (ret < 0) {
			dev_err(codec_dai->dev,
				"Failed to set CODEC SYSCLK: %d\n",
				ret);
			return ret;
		}

		ret = snd_soc_dai_set_pll(codec_dai, WM8962_FLL,
					  WM8962_FLL_MCLK, 0, 0);

		if (ret < 0) {
			dev_err(codec_dai->dev,
				"Failed to stop CODEC FLL: %d\n", ret);
			return ret;
		}

		clk_disable(bowser_data->wm8962_mclk);
		break;

	default:
		break;
	}

	bowser_data->bias_level = level;

	return 0;
}

static int bowser_suspend_pre(struct snd_soc_card *card)
{
	struct bowser_abe_data *bowser_data = snd_soc_card_get_drvdata(card);
	struct snd_soc_dai *codec_dai = bowser_data->codec_dai;

	if (codec_dai == NULL) {
		pr_err("codec_dai not initialized yet\n");
		return -EINVAL;
	}

	dev_crit(codec_dai->dev, "%s\n", __func__);

	if (codec_dai->dev == NULL) {
		dev_err(codec_dai->dev, "no run time codec_dai initialized yet\n");
		return -EINVAL;
	}

	snd_soc_dapm_disable_pin(&codec_dai->codec->dapm, "SYSCLK");
	snd_soc_dapm_sync(&codec_dai->codec->dapm);
	msleep(10);
	snd_soc_dapm_disable_pin(&codec_dai->codec->dapm, "MICBIAS");
	snd_soc_dapm_sync(&codec_dai->codec->dapm);

	return 0;
}

static int bowser_resume_post(struct snd_soc_card *card)
{
	struct bowser_abe_data *bowser_data = snd_soc_card_get_drvdata(card);
	struct snd_soc_dai *codec_dai = bowser_data->codec_dai;
	int ret = 0;

	if (codec_dai == NULL) {
		pr_err("codec_dai not initialized yet\n");
		return -EINVAL;
	}

	dev_dbg(codec_dai->dev, "%s is calling mic_detect\n", __func__);

	ret = wm8962_mic_detect(codec_dai->codec, &bowser_data->bowser_jack);

	return ret;
}

static int bowser_wm8962_hw_params(struct snd_pcm_substream *substream,
				   struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct omap_mcbsp *mcbsp = snd_soc_dai_get_drvdata(cpu_dai);
	int ret;

	dev_dbg(codec_dai->dev, "%s() - enter\n", __func__);

	sys_clk = fll_clk =  WM8962_SYS_CLK_RATE;

	ret = snd_soc_dai_set_pll(codec_dai, WM8962_FLL, WM8962_FLL_MCLK,
				  WM8962_MCLK_RATE, fll_clk);
	if (ret < 0) {
		dev_err(codec_dai->dev, "Failed to start CODEC FLL: %d\n",
			ret);
		return ret;
	}

	ret = snd_soc_dai_set_sysclk(codec_dai, WM8962_SYSCLK_FLL,
				     sys_clk, 0);
	if (ret < 0) {
		dev_err(codec_dai->dev, "Failed to set CODEC SYSCLK: %d\n",
			ret);
		return ret;
	}
	ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_DSP_B |
				  SND_SOC_DAIFMT_CBM_CFM |
				  SND_SOC_DAIFMT_NB_NF);
	if (ret < 0) {
		dev_err(codec_dai->dev, "Failed to set CODEC DAI format: %d\n",
			ret);
		return ret;
	}

	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_DSP_B |
				  SND_SOC_DAIFMT_CBM_CFM |
				  SND_SOC_DAIFMT_NB_NF);
	if (ret < 0) {
		dev_err(cpu_dai->dev, "Failed to set CPU DAI format: %d\n",
			ret);
		return ret;
	}

	omap_mcbsp_set_tx_threshold(mcbsp, params_channels(params));

	dev_dbg(codec_dai->dev, "%s() - exit\n", __func__);

	return 0;
}

static struct snd_soc_ops bowser_abe_ops = {
	.hw_params = bowser_wm8962_hw_params,
};

static int bowser_bt_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int ret = 0;

	struct omap_mcbsp *mcbsp = snd_soc_dai_get_drvdata(cpu_dai);

	ret = snd_soc_dai_set_fmt(cpu_dai,
			  SND_SOC_DAIFMT_DSP_C |
			  SND_SOC_DAIFMT_NB_NF |
			  SND_SOC_DAIFMT_CBM_CFM);

	if (ret < 0) {
		printk(KERN_ERR "can't set cpu DAI configuration\n");
		return ret;
	}

	omap_mcbsp_set_tx_threshold(mcbsp, 1);

	return ret;
}

static struct snd_soc_ops bowser_bt_ops = {
	.hw_params = bowser_bt_hw_params,
};

static const struct snd_soc_dapm_widget bowser_dapm_widgets[] = {
	SND_SOC_DAPM_HP("HP", NULL),
	SND_SOC_DAPM_SPK("SPK", NULL),
};

static const struct snd_kcontrol_new bowser_controls[] = {
	SOC_DAPM_PIN_SWITCH("DMICDAT"),
	SOC_DAPM_PIN_SWITCH("HP"),
	SOC_DAPM_PIN_SWITCH("SPK"),
};

static const struct snd_soc_dapm_route bowser_dapm_routes[] = {
	{ "HP", NULL, "HPOUTL" },
	{ "HP", NULL, "HPOUTR" },

	{ "SPK", NULL, "SPKOUTL" },
	{ "SPK", NULL, "SPKOUTR" },

	{ "DACL", NULL, "MM_EXT_DL" },
	{ "DACR", NULL, "MM_EXT_DL" },

	/* FM <--> ABE */
	{"omap-mcbsp.2 Playback", NULL, "MM_EXT_DL"},
	{"MM_EXT_UL", NULL, "omap-mcbsp.2 Capture"},

	/* Bluetooth <--> ABE*/
	{"omap-mcbsp.3 Playback", NULL, "BT_VX_DL"},
	{"BT_VX_UL", NULL, "omap-mcbsp.3 Capture"},
};

static int bowser_wm8962_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_card *card = codec->card;
	struct bowser_abe_data *bowser_data = snd_soc_card_get_drvdata(card);
	int ret;

	bowser_data->codec_dai = rtd->codec_dai;

	ret = snd_soc_jack_new(codec, "h2w",
				SND_JACK_HEADSET | SND_JACK_HEADPHONE,
				&bowser_data->bowser_jack);
	if (ret) {
		pr_err("Failed to create jack: %d\n", ret);
		return ret;
	}

	snd_jack_set_key(bowser_data->bowser_jack.jack,
			SND_JACK_BTN_0, KEY_MEDIA);

	ret = wm8962_get_jack(codec, &bowser_data->bowser_jack);
	if (ret) {
		pr_err("Failed to get jack: %d\n", ret);
		return ret;
	}

	return 0;
}

static int mcbsp_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
				    struct snd_pcm_hw_params *params)
{
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_interval *channels = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_CHANNELS);
	struct snd_interval *rate = hw_param_interval(params,
						SNDRV_PCM_HW_PARAM_RATE);
	unsigned int be_id = rtd->dai_link->be_id;
	unsigned int threshold;
	struct omap_mcbsp *mcbsp = snd_soc_dai_get_drvdata(cpu_dai);

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
		.name = "wm8962-lp",
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
		.name = "wm8962-mm",
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
		.name = "wm8962-capture",
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
		.name = "wm8962-tone",
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
		.name = "wm8962-noabe",
		.stream_name = "wm8962",
		.cpu_dai_name = "omap-mcbsp.2",   /* McBSP2 */
		.platform_name = "omap-pcm-audio",
		.codec_dai_name = "wm8962",
		.codec_name = "wm8962.3-001a",
		.ops = &bowser_abe_ops,
		.init = &bowser_wm8962_init,
	},
/*
 * Backend DAIs - i.e. dynamically matched interfaces, invisible to userspace.
 * Matched to above interfaces at runtime, based upon use case.
 */
	{
		.name = OMAP_ABE_BE_MM_EXT0_DL,
		.stream_name = "FM Playback",

		/* ABE components - MCBSP2 - MM-EXT */
		.cpu_dai_name = "omap-mcbsp.2",
		.platform_name = "aess",

		/* FM */
		.codec_dai_name = "wm8962",
		.codec_name = "wm8962.3-001a",

		.no_pcm = 1, /* don't create ALSA pcm for this */
		.be_hw_params_fixup = mcbsp_be_hw_params_fixup,
		.ops = &bowser_abe_ops,
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
		.codec_dai_name = "wm8962",
		.codec_name = "wm8962.3-001a",

		.no_pcm = 1, /* don't create ALSA pcm for this */
		.be_hw_params_fixup = mcbsp_be_hw_params_fixup,
		.ops = &bowser_abe_ops,
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
		.ops = &bowser_bt_ops,
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
		.ops = &bowser_bt_ops,
		.be_id = OMAP_ABE_DAI_BT_VX,
		.ignore_suspend = 1,
	},
};

static struct snd_soc_card bowser_snd_soc_card = {
	.owner = THIS_MODULE,
	.name = "Bowser",
	.long_name = "TI OMAP4 bowser Board",

	.dapm_widgets = bowser_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(bowser_dapm_widgets),

	.dapm_routes = bowser_dapm_routes,
	.num_dapm_routes = ARRAY_SIZE(bowser_dapm_routes),

	.dai_link = bowser_dai,
	.num_links = ARRAY_SIZE(bowser_dai),

	.controls = bowser_controls,
	.num_controls = ARRAY_SIZE(bowser_controls),

	.set_bias_level = bowser_set_bias_level,
	.set_bias_level_post = bowser_set_bias_level_post,

	.suspend_pre = bowser_suspend_pre,
	.resume_post = bowser_resume_post,
};

static __devinit int bowser_omap_abe_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &bowser_snd_soc_card;
	struct bowser_abe_data *bowser_data = NULL;
	int ret;

	card->dev = &pdev->dev;

	bowser_data = devm_kzalloc(&pdev->dev,
			sizeof(*bowser_data), GFP_KERNEL);
	if (bowser_data == NULL)
		return -ENOMEM;

	bowser_data->wm8962_mclk = clk_get(NULL, "auxclk0_ck");
	if (IS_ERR(bowser_data->wm8962_mclk)) {
		dev_err(&pdev->dev, "Failed to get WM8962 MCLK: %ld\n",
			PTR_ERR(bowser_data->wm8962_mclk));
		return -ENODEV;
	}

	dev_dbg(&pdev->dev, "Codec mclk rate = %lu\n",
				clk_get_rate(bowser_data->wm8962_mclk));

	ret = clk_set_rate(bowser_data->wm8962_mclk, WM8962_MCLK_RATE);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to set MCLK rate: %d\n", ret);
		clk_put(bowser_data->wm8962_mclk);
		return ret;
	}

	dev_dbg(&pdev->dev, "Adjusted codec mclk rate = %lu\n",
				clk_get_rate(bowser_data->wm8962_mclk));

	bowser_data->bias_level = SND_SOC_BIAS_OFF;

	snd_soc_card_set_drvdata(card, bowser_data);

	ret = snd_soc_register_card(card);
	if (ret)
		dev_err(&pdev->dev, "snd_soc_register_card() failed: %d\n",
				ret);

	return ret;
}

static int __devexit bowser_omap_abe_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	struct bowser_abe_data *bowser_data = snd_soc_card_get_drvdata(card);

	snd_soc_unregister_card(card);

	if (!IS_ERR(bowser_data->wm8962_mclk)) {
		clk_disable(bowser_data->wm8962_mclk);
		clk_put(bowser_data->wm8962_mclk);
	}

	return 0;
}

static void bowser_omap_abe_shutdown(struct platform_device *pdev)
{
	snd_soc_poweroff(&pdev->dev);
}

static struct platform_driver bowser_omap_abe_driver = {
	.driver = {
		.name = "bowser-wolfson",
		.owner = THIS_MODULE,
		.pm = &snd_soc_pm_ops,
	},
	.probe = bowser_omap_abe_probe,
	.remove = __devexit_p(bowser_omap_abe_remove),
	.shutdown = bowser_omap_abe_shutdown,
};

module_platform_driver(bowser_omap_abe_driver);


MODULE_AUTHOR("Misael Lopez Cruz <x0052729@ti.com>");
MODULE_DESCRIPTION("ALSA SoC bowser");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:bowser-wolfson");

