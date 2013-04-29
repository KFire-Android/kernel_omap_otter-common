/* revert
 * omap4_panda_aic31xx.c  --  SoC audio for TI OMAP4 Panda Board
 *
 *
 * Based on:
 * sdp3430.c by Misael Lopez Cruz <x0052729@ti.com>
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
 * Revision 0.1           Developed the Machine driver for AIC3110 Codec Chipset
 *
 * Revision 0.2           Updated the Headset jack insertion code-base.
 *
 * Revision 0.3           Updated the driver for ABE Support. Modified the
 *                        dai_link array to include both Front-End and back-end
 *                        DAI Declarations.
 *
 * Revision 0.4           Updated the McBSP threshold from 1 to 2 within the
 *			  mcbsp_be_hw_params_fixup() function. Also updated the
 *			  ABE Sample format to STEREO_RSHIFTED_16 from
 *                        STEREO_16_16
 *
 * Revision 0.5           Updated the omap4_hw_params() function to check for
 *			  the GPIO and the Jack Status.
 *
 * Revision 0.6           Updated the snd_soc_dai_link array for ABE to use the
 *			  Low-Power "MultiMedia1 LP" Mode
 *
 * Revision 0.7		  DAPM support implemented and switching between speaker
 *			  and headset is taken card DAPM itself
 *
 * Revision 0.8		  Ported to Linux 3.0 Kernel
 *
 * Revision 0.9		  Ported to Linux 3.4 Kernel
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/jack.h>
#include <sound/pcm_params.h>

#include <asm/mach-types.h>
#include <plat/hardware.h>
#include <plat/mux.h>
#include <plat/mcbsp.h>

#include "omap-mcpdm.h"
#include "omap-pcm.h"
#include "omap-mcbsp.h"
#include "mcbsp.h"
#include "omap-abe-priv.h"
#include "../codecs/tlv320aic31xx.h"

#include <linux/mfd/tlv320aic31xx-registers.h>
#include <linux/mfd/tlv320aic3xxx-registers.h>
#include <linux/mfd/tlv320aic3xxx-core.h>

// static struct snd_soc_codec *aic31xx_codec;

/* Forward Declaration */
static int Qoo_headset_jack_status_check(void);

/* Headset jack information structure */
static struct snd_soc_jack hs_jack;


static int omap_abe_mcbsp_hw_params(struct snd_pcm_substream *substream,
					   struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int ret = 0, err = 0;
	unsigned int channels;
#ifdef CONFIG_MACH_OMAP_4430_KC1
	void __iomem *phymux_base = NULL;
#endif


#ifdef AIC31XX_MCBSP_SLAVE

		pr_debug("\naic31xx: AIC31xx  MCBSP slave & master\n");
		/* Set codec DAI configuration */
		err = snd_soc_dai_set_fmt(codec_dai,
						SND_SOC_DAIFMT_I2S |
						SND_SOC_DAIFMT_NB_NF |
						SND_SOC_DAIFMT_CBM_CFM);
		if (err < 0) {
			pr_debug(KERN_ERR
				 "can't set codec DAI configuration\n");
			return err;
		}
		/* Set cpu DAI configuration */
		err = snd_soc_dai_set_fmt(cpu_dai,
						SND_SOC_DAIFMT_I2S |
						SND_SOC_DAIFMT_NB_NF |
						SND_SOC_DAIFMT_CBM_CFM);
		if (err < 0) {
			pr_debug(KERN_ERR "can't set cpu DAI configuration\n");
			return ret;
		}
		
#ifdef CONFIG_MACH_OMAP_4430_KC1
		/* Enabling the 19.2 Mhz Master Clock Output from OMAP4 for KC1 Board */
		phymux_base = ioremap(0x4a30a000, 0x1000);
		__raw_writel(0x00010100, phymux_base + 0x318);

		/* Added the test code to configure the McBSP4 CONTROL_MCBSP_LP
		 * register. This register ensures that the FSX and FSR on McBSP4 are
		 * internally short and both of them see the same signal from the
		 * External Audio Codec.
		 */
		phymux_base = ioremap(0x4a100000, 0x1000);
		__raw_writel(0xC0000000, phymux_base + 0x61c);
#endif

        	/* Set the codec system clock for DAC and ADC */

		ret = snd_soc_dai_set_pll(codec_dai, 0, AIC31XX_PLL_CLKIN_MCLK , 19200000, params_rate(params));
        	if (ret < 0) {
                	pr_debug(KERN_ERR "Can't set codec pll clock\n");
                	return ret;
        }
#else
		pr_debug("\naic31xx: AIC31xx SLAVE & MASTER\n");
		/* Set codec DAI configuration */
		err = snd_soc_dai_set_fmt(codec_dai,
						SND_SOC_DAIFMT_I2S |
						SND_SOC_DAIFMT_NB_NF |
						SND_SOC_DAIFMT_CBS_CFS);
		if (err < 0) {
			printk(KERN_INFO"\nInside %s cannot set codec"
				"dai configuration\n", __func__);
			return err;
		}

		/* Set cpu DAI configuration */
		err = snd_soc_dai_set_fmt(cpu_dai,
						SND_SOC_DAIFMT_I2S |
						SND_SOC_DAIFMT_NB_NF |
						SND_SOC_DAIFMT_CBS_CFS);
		if (err < 0) {
			printk("\nInside %s cannot set"
					"cpu dai configuration\n", __func__);
			return err;
		}

        /* Set McBSP clock to external */
        ret = snd_soc_dai_set_sysclk(cpu_dai, OMAP_MCBSP_SYSCLK_CLKS_FCLK,
                                     64 * params_rate(params),
                                     SND_SOC_CLOCK_IN);
        if (ret < 0) {
                pr_debug(KERN_ERR "can't set cpu system clock\n");
                return ret;
        }

	ret = snd_soc_dai_set_pll(codec_dai, 0, AIC31XX_PLL_CLKIN_MCLK , 19200000, params_rate(params));
        if (ret < 0) {
                pr_debug(KERN_ERR "Can't set codec pll clock\n");
                return ret;
        }
#endif
	
	
	if (params != NULL) {
		struct omap_mcbsp *mcbsp = snd_soc_dai_get_drvdata(cpu_dai);
		/* Configure McBSP internal buffer usage */
		/* this need to be done for playback and/or record */
		channels = params_channels(params);

		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			omap_mcbsp_set_tx_threshold(mcbsp, channels);
		else
			omap_mcbsp_set_rx_threshold(mcbsp, channels);
	} else
		pr_debug(" params in else statement is %p\n", params);

	if (ret < 0)
		pr_debug(KERN_ERR "can't set cpu system clock\n");

	return ret;
}

/*
 * @struct omap_abe_mcbsp_ops
 *
 * Structure for the Machine Driver Operations
 */
static struct snd_soc_ops omap_abe_mcbsp_ops = {
	.hw_params = omap_abe_mcbsp_hw_params,

};

static int mcbsp_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
			struct snd_pcm_hw_params *params)
{
	struct snd_interval *channels = hw_param_interval(params,
						SNDRV_PCM_HW_PARAM_CHANNELS);
	struct snd_interval *rate = hw_param_interval(params,
						SNDRV_PCM_HW_PARAM_RATE);
	unsigned int be_id = rtd->dai_link->be_id;

	switch (be_id) {
	case OMAP_ABE_DAI_BT_VX:
		channels->min = 2;
		rate->min = rate->max = 8000;
		break;
	case OMAP_ABE_DAI_MM_FM:
		channels->min = 2;
		rate->min = rate->max = 48000;
		break;
	case OMAP_ABE_DAI_MODEM:
		break;
	default:
		return -EINVAL;
	}

	snd_mask_set(&params->masks[SNDRV_PCM_HW_PARAM_FORMAT -
				SNDRV_PCM_HW_PARAM_FIRST_MASK],
				SNDRV_PCM_FORMAT_S16_LE);
	return 0;
}

/* @struct hs_jack_pins
 *
 * Headset jack detection DAPM pins
 *
 * @pin:    name of the pin to update
 * @mask:   bits to check for in reported jack status
 * @invert: if non-zero then pin is enabled when status is not reported
 */
static struct snd_soc_jack_pin hs_jack_pins[] = {
	{
		.pin = "HSMIC",
		.mask = SND_JACK_MICROPHONE,
	},
	{
		.pin = "Headphone Jack",
		.mask = SND_JACK_HEADPHONE,
	},
};

/*
 * @struct hs_jack_gpios
 *
 * Headset jack detection gpios
 *
 * @gpio:         Pin 49 on the Qoo Rev 1 Board
 * @name:         String Name "hsdet-gpio"
 * @report:       value to report when jack detected
 * @invert:       report presence in low state
 * @debouce_time: debouce time in ms
 */
static struct snd_soc_jack_gpio hs_jack_gpios[] = {
	{
		.gpio = Qoo_HEADSET_DETECT_GPIO_PIN,
		.name = "hsdet-gpio",
		.report = SND_JACK_HEADSET,
		.debounce_time = 200,
		.jack_status_check = Qoo_headset_jack_status_check,
	},
};

static int mic_power_up_event(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	u8 ret = 0;

	if (SND_SOC_DAPM_EVENT_ON(event)) {

		/* Power up control of MICBIAS */
		snd_soc_update_bits(codec,
			AIC31XX_MICBIAS_CTRL_REG, 0x03, 0x03);
	}
	if (SND_SOC_DAPM_EVENT_OFF(event)) {

		/* Power down control of MICBIAS */
		snd_soc_update_bits(codec,
			AIC31XX_MICBIAS_CTRL_REG, 0x03, 0x03);
	}
	return ret;
}

/* OMAP4 machine DAPM */
static const struct snd_soc_dapm_widget omap4_aic31xx_dapm_widgets[] = {
	SND_SOC_DAPM_HP("Headphone Jack", NULL),
	SND_SOC_DAPM_SPK("Speaker Jack", NULL),
	SND_SOC_DAPM_MIC("HSMIC", mic_power_up_event),
	SND_SOC_DAPM_MIC("Onboard Mic", mic_power_up_event),
};

static const struct snd_kcontrol_new omap4_aic31xx_controls[] = {
	SOC_DAPM_PIN_SWITCH("HSMIC"),
	SOC_DAPM_PIN_SWITCH("Speaker Jack"),
	SOC_DAPM_PIN_SWITCH("Headphone Jack"),
};
static const struct snd_soc_dapm_route audio_map[] = {
	/* External Speakers: HFL, HFR */
	{"Speaker Jack", NULL, "SPL"},
	{"Speaker Jack", NULL, "SPR"},

	/* Headset Mic: HSMIC with bias */
	{"MIC1LP", NULL, "HSMIC"},
	{"MIC1RP", NULL, "HSMIC"},
	{"MIC1LM", NULL, "HSMIC"},
	{"INTMIC", NULL, "Onboard Mic"},

	/* Headset Stereophone(Headphone): HSOL, HSOR */
	{"Headphone Jack", NULL, "HPL"},
	{"Headphone Jack", NULL, "HPR"},
};

/*
 * omap4_panda_soc_init
 * This function is used to initialize the machine Driver.
 */

static int omap4_aic31xx_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dapm_context *dapm = &codec->dapm;
	int ret;

	/* Adding aic31xx specific widgets */

	ret = snd_soc_dapm_new_controls(dapm, omap4_aic31xx_dapm_widgets,
			ARRAY_SIZE(omap4_aic31xx_dapm_widgets));
	if (ret)
		return ret;


	ret = snd_soc_add_codec_controls(codec, omap4_aic31xx_controls,
			ARRAY_SIZE(omap4_aic31xx_controls));
	if (ret)
		return ret;


	/* Setup aic31xx specific audio path audio map */
	ret = snd_soc_dapm_add_routes(dapm, audio_map, ARRAY_SIZE(audio_map));

	ret = snd_soc_dapm_sync(dapm);
	if (ret)
		return ret;
	/* Headset jack detection */

	ret = snd_soc_jack_new(codec, "Headset Jack", SND_JACK_HEADSET, &hs_jack);
	if (ret)
		return ret;

	ret = snd_soc_jack_add_pins(&hs_jack, ARRAY_SIZE(hs_jack_pins), hs_jack_pins);

	snd_soc_jack_add_gpios(&hs_jack, ARRAY_SIZE(hs_jack_gpios), hs_jack_gpios);
// FIXME-HASH: Skip check for GPIOs here -- it's failing
//	ret = snd_soc_jack_add_gpios(&hs_jack, ARRAY_SIZE(hs_jack_gpios), hs_jack_gpios);
//	if (ret)
//		return ret;

	Qoo_headset_jack_status_check();
	return ret;
}

/*
 * Qoo_headset_jack_status_check
 * This function is to check the Headset Jack Status
 */
static int Qoo_headset_jack_status_check(void)
{
	int gpio_status, ret = 0, hs_status = 0;
	struct snd_soc_codec *codec = hs_jack.codec;
	struct aic31xx_priv *priv = snd_soc_codec_get_drvdata(codec);
	struct snd_soc_dapm_context *dapm = &codec->dapm;
	gpio_status = gpio_get_value(Qoo_HEADSET_DETECT_GPIO_PIN);
	dev_info(codec->dev, "#Entered %s\n", __func__);
	if (hs_jack.codec != NULL) {

		dev_dbg(codec->dev, "codec is  not null\n");
		if (!gpio_status) {
			dev_info(codec->dev, "headset connected\n");
			snd_soc_dapm_disable_pin(dapm, "Speaker Jack");
			snd_soc_dapm_enable_pin(dapm, "Headphone Jack");
			if (aic31xx_mic_check(codec)) {
				if (hs_status) {
					dev_info(codec->dev, "Headset without"
						"MIC Detected Recording"
						"not possible.\n");
					hs_status = 1;
				}
				 else {
					dev_info(codec->dev, "Headset with MIC"
						"Inserted Recording possible\n");
					hs_status = (1<<1);
				}
			}
		} else {
			dev_info(codec->dev, "headset not connected\n");
			snd_soc_dapm_enable_pin(dapm,  "Speaker Jack");
			snd_soc_dapm_disable_pin(dapm, "HSMIC");
			snd_soc_dapm_disable_pin(dapm, "Headphone Jack");
			hs_status = 0;
		}
		priv->headset_connected = !gpio_status;
		dev_info(codec->dev, "##%s : switch state = %d\n",
				__func__, !gpio_status);

		dev_dbg(codec->dev, "%s: Exiting\n", __func__);
	}
	return ret;
}

static struct snd_soc_dai_link omap4_dai_abe[] = {
	{
		.name = "tlv320aic3110 Media1 LP",
		.stream_name = "Multimedia",
		/* ABE components - MM-DL (mmap) */
		.cpu_dai_name = "MultiMedia1 LP",
		.platform_name = "aess",
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.dynamic = 1, /* BE is dynamic */
		.trigger = {SND_SOC_DPCM_TRIGGER_BESPOKE, SND_SOC_DPCM_TRIGGER_BESPOKE},
	},
	{
		.name = "tlv320aic3110 Media",
		.stream_name = "Multimedia",
		/* ABE components - MM-UL & MM_DL */
		.cpu_dai_name = "MultiMedia1",
		.platform_name = "omap-pcm-audio",
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.dynamic = 1, /* BE is dynamic */
		.trigger = {SND_SOC_DPCM_TRIGGER_BESPOKE, SND_SOC_DPCM_TRIGGER_BESPOKE},
	},
	{
		.name = "tlv320aic3110 Media Capture",
		.stream_name = "Multimedia Capture",
		/* ABE components - MM-UL2 */
		.cpu_dai_name = "MultiMedia2",
		.platform_name = "omap-pcm-audio",
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.dynamic = 1, /* BE is dynamic */
		.trigger = {SND_SOC_DPCM_TRIGGER_BESPOKE, SND_SOC_DPCM_TRIGGER_BESPOKE},
	},
	{

		.name = "Legacy McBSP3",
		.stream_name = "MultiMedia",
		/* ABE components - MCBSP2 - MM-EXT */
		.cpu_dai_name = "omap-mcbsp.2",
		.codec_dai_name = "tlv320aic31xx-MM_EXT",
		.platform_name = "omap-pcm-audio",
		.codec_name = "tlv320aic31xx-codec",
		.init = omap4_aic31xx_init,
		.ops = &omap_abe_mcbsp_ops,
	},

/*
 * Backend DAIs - i.e. dynamically matched interfaces, invisible to userspace.
 * Matched to above interfaces at runtime, based upon use case.
 */
	{
		.name = OMAP_ABE_BE_MM_EXT0_UL,
		.stream_name = "FM Capture",

		/* ABE components - MCBSP2 - MM-EXT */
		.cpu_dai_name = "omap-mcbsp.2",
		.platform_name = "aess",

		/* FM */
		.codec_dai_name = "tlv320aic31xx-MM_EXT",
		.codec_name = "tlv320aic31xx-codec",

		.no_pcm = 1, /* don't create ALSA pcm for this */
		.be_hw_params_fixup = mcbsp_be_hw_params_fixup,
		.ops = &omap_abe_mcbsp_ops,
		.be_id = OMAP_ABE_DAI_MM_FM,
	},
	{
		.name = OMAP_ABE_BE_MM_EXT0_DL,
		.stream_name = "FM Playback",

		/* ABE components - MCBSP2 - MM-EXT */
		.cpu_dai_name = "omap-mcbsp.2",
		.platform_name = "aess",

		/* FM */
		.codec_dai_name = "tlv320aic31xx-MM_EXT",
		.codec_name = "tlv320aic31xx-codec",

		.no_pcm = 1, /* don't create ALSA pcm for this */
		.be_hw_params_fixup = mcbsp_be_hw_params_fixup,
		.ops = &omap_abe_mcbsp_ops,
		.be_id = OMAP_ABE_DAI_MM_FM,
	},
};

/* The below mentioend DAI LINK Structure is to be used in McBSP3 Legacy Mode
 * only when the AIC3110 Audio Codec Chipset is interfaced via McBSP3 Port
 */
/* Audio machine driver with ABE Support */

static struct snd_soc_card omap_abe_card = {
	.owner = THIS_MODULE,
	.name = "AIC31XX_OTTER",
	.long_name = "TI OMAP4 Kindle Fire",
	.dai_link = omap4_dai_abe,
	.num_links = ARRAY_SIZE(omap4_dai_abe),
};

static struct platform_device *omap4_snd_device;

static __devinit int sdp44xx_aic31xx_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &omap_abe_card;
	int ret;

	card->dev = &pdev->dev;

	pr_info(KERN_INFO "AIC31xx SoC init\n");

	ret = snd_soc_register_card(card);
	if (ret) {
		printk(KERN_ERR "snd_soc_register_card() failed: %d\n", ret);
		goto err_dev;
	}

	return 0;

err_dev:
	platform_device_put(omap4_snd_device);
	return ret;
}

static int __devexit sdp44xx_aic31xx_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);

	snd_soc_jack_free_gpios(&hs_jack, ARRAY_SIZE(hs_jack_gpios), hs_jack_gpios);
	platform_device_unregister(omap4_snd_device);
	snd_soc_unregister_card(card);

	return 0;
}

static void sdp44xx_aic31xx_shutdown(struct platform_device *pdev)
{
	snd_soc_poweroff(&pdev->dev);
}

static struct platform_driver sdp44xx_aic31xx_driver = {
	.driver = {
		.name = "omap4-panda-aic31xx",
		.owner = THIS_MODULE,
		.pm = &snd_soc_pm_ops,
	},
	.probe = sdp44xx_aic31xx_probe,
	.remove = __devexit_p(sdp44xx_aic31xx_remove),
	.shutdown = sdp44xx_aic31xx_shutdown,
};

module_platform_driver(sdp44xx_aic31xx_driver);

MODULE_AUTHOR("Santosh Sivaraj <santosh.s@mistralsolutions.com>");
MODULE_DESCRIPTION("ALSA SoC OMAP4 Panda");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:omap4-panda-aic31xx");
