/* revert
 * omap4_panda_aic31xx.c  --  SoC audio for TI OMAP4 Panda Board
 *
 * Author: Santosh Sivaraj <santosh.s@mistralsolutions.com>
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
 * Revision 0.1           Developed the Machine driver for AIC3110 Codec Chipset.
 *
 * Revision 0.2           Updated the Headset jack insertion code-base.
 *
 * Revision 0.3           Updated the driver for ABE Support. Modified the
 *                        dai_link array to include both Front-End and back-end
 *                        DAI Declarations.
 *
 * Revision 0.4           Updated the McBSP threshold from 1 to 2 within the mcbsp_be_hw_params_fixup()
 *                        function. Also updated the ABE Sample format to STEREO_RSHIFTED_16 from
 *                        STEREO_16_16  
 *
 * Revision 0.5           Updated the omap4_hw_params() function to check for the GPIO and the 
 *                        Jack Status.
 *
 * Revision 0.6           Updated the snd_soc_dai_link array for ABE to use the Low-Power "MultiMedia1 LP" Mode
 * 
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/gpio.h>

#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/jack.h>

#include <asm/mach-types.h>
#include <mach/hardware.h>
#include <mach/gpio.h>
#include <plat/mcbsp.h>
#include <plat/board.h>

#include "omap-mcbsp.h"
#include "omap-pcm.h"
#include "omap-abe.h"
#include "omap-dmic.h"
#include "abe/abe_main.h"
#include "../codecs/tlv320aic31xx.h"

/* The following flag is used to enable/disable the DAPM support
 * for the driver 
 */
//#define DRIVER_DAPM_SUPPORT
#undef DRIVER_DAPM_SUPPORT

/* Global I2c Client Structure used for registering with I2C Bus */
static struct i2c_client *tlv320aic31xx_client;
/*
static struct i2c_board_info tlv320aic31xx_hwmon_info = {
	I2C_BOARD_INFO("tlv320aic3110", 0x18),
};
*/
/* Forward Declaration */
static int kc1_headset_jack_status_check (void);

/* Headset jack information structure */
static struct snd_soc_jack hs_jack;

/*
 * omap4_hw_params
 * This function is to configure the Machine Driver
 */
static int omap4_hw_params(struct snd_pcm_substream *substream,
			   struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_codec *codec = rtd->codec;

	void __iomem *phymux_base = NULL;
	int ret, gpio_status, val;

	DBG ("%s: Entered \n", __func__);

	/* Recording will not happen if headset is not inserted */
	gpio_status = gpio_get_value (KC1_HEADSET_DETECT_GPIO_PIN);

	/* first Check if the Jack is inserted or not by checking the GPIO Pin */
	if(gpio_status != 0 && substream->stream != SNDRV_PCM_STREAM_PLAYBACK) {
		printk (KERN_INFO "No Jack connected. Recording not possible..\n");
		return (-ENODEV);
	}

	/* Set codec DAI configuration */
	ret = snd_soc_dai_set_fmt(codec_dai,
				  SND_SOC_DAIFMT_I2S |
				  SND_SOC_DAIFMT_NB_NF |
				  SND_SOC_DAIFMT_CBM_CFM);
	if (ret < 0) {
		printk(KERN_ERR "can't set codec DAI configuration\n");
		return ret;
	}
	//DBG ("snd_soc_dai_set_fmt passed..\n");

	/* Set cpu DAI configuration */
	ret = snd_soc_dai_set_fmt(cpu_dai,
				  SND_SOC_DAIFMT_I2S |
				  SND_SOC_DAIFMT_NB_NF |
				  SND_SOC_DAIFMT_CBM_CFM);
	if (ret < 0) {
		printk(KERN_ERR "can't set cpu DAI configuration\n");
		return ret;
	}
	//DBG ("snd_soc_dai_set_fmt passed...\n");

	/* Enabling the 19.2 Mhz Master Clock Output from OMAP4 for KC1 Board */
	phymux_base = ioremap(0x4a30a000, 0x1000);
	__raw_writel(0x00010100, phymux_base + 0x318);

	/* Mistral added the test code to configure the McBSP4 CONTROL_MCBSP_LP
	 * register. This register ensures that the FSX and FSR on McBSP4 are
	 * internally short and both of them see the same signal from the
	 * External Audio Codec.
	 */
	phymux_base = ioremap (0x4a100000, 0x1000);
	__raw_writel (0xC0000000, phymux_base + 0x61c);

	/* Set the codec system clock for DAC and ADC. The
         * third argument is specific to the board being used.
         */
	ret = snd_soc_dai_set_sysclk(codec_dai, 0, AIC31XX_FREQ_19200000,
				     SND_SOC_CLOCK_IN);
	if (ret < 0) {
		printk(KERN_ERR "can't set codec system clock\n");
		return ret;
	}

	//DBG ("omap4_hw_params passed...\n");
	DBG ("%s: EXITING \n", __func__);

       return 0;
}

/* 
 * @struct omap4_ops
 *
 * Structure for the Machine Driver Operations
 */
static struct snd_soc_ops omap4_ops = {
	.hw_params = omap4_hw_params,

};


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
		.pin = "Headset Mic",
		.mask = SND_JACK_MICROPHONE,
	},
	{
		.pin = "Headset Stereophone",
		.mask = SND_JACK_HEADPHONE,
	},
};

/* 
 * @struct hs_jack_gpios
 * 
 * Headset jack detection gpios  
 *
 * @gpio:         Pin 49 on the KC1 Rev 1 Boardmcbsp_be_hw_params_fixup
 * @name:         String Name "hsdet-gpio"
 * @report:       value to report when jack detected
 * @invert:       report presence in low state
 * @debouce_time: debouce time in ms
 */
static struct snd_soc_jack_gpio hs_jack_gpios[] = {
	{
		.gpio = KC1_HEADSET_DETECT_GPIO_PIN,
		.name = "hsdet-gpio",
		.report = SND_JACK_HEADSET,
		.debounce_time = 200,
		.jack_status_check = kc1_headset_jack_status_check, 
	},
};

#ifdef DRIVER_DAPM_SUPPORT
/* OMAP4 machine DAPM */
static const struct snd_soc_dapm_widget omap4_aic31xx_dapm_widgets[] = {
	SND_SOC_DAPM_HP ("Headphone Jack", NULL),
	SND_SOC_DAPM_SPK ("Speaker Jack", NULL),
	SND_SOC_DAPM_MIC ("Ext Mic", NULL),
	SND_SOC_DAPM_MIC ("Onboard Mic", NULL),
};

static const struct snd_soc_dapm_route audio_map[] = {
	/* External Speakers: HFL, HFR */
	{"Speaker Jack", NULL, "SPL"},
	{"Speaker Jack", NULL, "SPR"},

	/* Headset Mic: HSMIC with bias */
	{"EXTMIC", NULL, "Ext Mic"},
	{"INTMIC", NULL, "Onboard Mic"},

	/* Headset Stereophone (Headphone): HSOL, HSOR */
	{"Headphone Jack", NULL, "HPL"},
	{"Headphone Jack", NULL, "HPR"},
};

#endif

/*
 * omap4_aic31xx_init
 * This function is to initialize the machine Driver.
 */
static int omap4_aic31xx_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_codec *codec = rtd->codec;
	int ret = 0;
	int gpiostatus;

	printk(KERN_INFO "entered the omap4_aic31xx_init function....\n");

#ifdef DRIVER_DAPM_SUPPORT
	/* Add OMAP4 specific widgets */
	ret = snd_soc_dapm_new_controls(codec->dapm, omap4_aic31xx_dapm_widgets,
					ARRAY_SIZE(omap4_aic31xx_dapm_widgets));
	if (ret) {
		printk (KERN_INFO "snd_soc_dapm_new_controls call failed. returning...\n");
		return ret;
	}
	DBG ("snd_soc_dapm_new_controls passed..\n");
	DBG ("Disabling the DAPM Routes for now...\n");



	/* Set up OMAP4 specific audio path audio_map */
	ret = snd_soc_dapm_add_routes(codec->dapm, audio_map, ARRAY_SIZE(audio_map));

	if (ret != 0) {
		printk (KERN_INFO "snd_soc_dapm_add_routes failed... %d\n", ret);
	}

	/* OMAP4 connected pins */
	snd_soc_dapm_enable_pin(codec->dapm, "Ext Mic");
	snd_soc_dapm_enable_pin(codec->dapm, "Ext Spk");
	snd_soc_dapm_disable_pin(codec->dapm, "Headset Mic");
	snd_soc_dapm_disable_pin(codec->dapm, "Headset Stereophone");


	/* AIC31XX not connected pins */
	snd_soc_dapm_nc_pin(codec->dapm, "AUXL");
	snd_soc_dapm_nc_pin(codec->dapm, "AUXR");
	snd_soc_dapm_nc_pin(codec->dapm, "CARKITMIC");
	snd_soc_dapm_nc_pin(codec->dapm, "DIGIMIC0");
	snd_soc_dapm_nc_pin(codec->dapm, "DIGIMIC1");mcbsp_be_hw_params_fixup

	snd_soc_dapm_nc_pin(codec->dapm, "OUTL");
	snd_soc_dapm_nc_pin(codec->dapm, "OUTR");
	snd_soc_dapm_nc_pin(codec->dapm, "EARPIECE");
	snd_soc_dapm_nc_pin(codec->dapm, "PREDRIVEL");
	snd_soc_dapm_nc_pin(codec->dapm, "PREDRIVER");
	snd_soc_dapm_nc_pin(codec->dapm, "CARKITL");
	snd_soc_dapm_nc_pin(codec->dapm, "CARKITR");

	ret = snd_soc_dapm_sync(codec->dapm);
	if (ret != 0) {
		printk (KERN_INFO "snd_soc_dapm_sync failed... %d\n", ret);
		return ret;
	}

	printk (KERN_INFO "snd_soc_dapm_sync passed..\n");mcbsp_be_hw_params_fixup

	/* Headset jack detection */
	ret = snd_soc_jack_new(codec, "Headset Jack",
			       SND_JACK_HEADSET, &hs_jack);
	if (ret != 0) {
		printk (KERN_INFO "snd_soc_jack_new failed...\n");
		return ret;
	}

	ret = snd_soc_jack_add_pins(&hs_jack, ARRAY_SIZE(hs_jack_pins),
				    hs_jack_pins);
	if (ret != 0) {
		printk (KERN_INFO "snd_soc_jack_add_pins failed... %d\n", ret);
		return ret;
	}

	ret = snd_soc_jack_add_gpios(&hs_jack, ARRAY_SIZE(hs_jack_gpios),
				     hs_jack_gpios);
#endif
	/* Headset jack detection */
	gpiostatus = snd_soc_jack_new(codec, "Headset Jack",
			       SND_JACK_HEADSET, &hs_jack);
	if (gpiostatus != 0) {
		printk (KERN_INFO "snd_soc_jack_new failed. %d..\n", gpiostatus);
	}

	gpiostatus = snd_soc_jack_add_pins(&hs_jack, ARRAY_SIZE(hs_jack_pins),
				    hs_jack_pins);
	if (gpiostatus != 0) {
		printk (KERN_INFO "snd_soc_jack_add_pins failed... %d\n", gpiostatus);
        }

	gpiostatus = snd_soc_jack_add_gpios(&hs_jack, ARRAY_SIZE(hs_jack_gpios),
				     hs_jack_gpios);

	if (gpiostatus != 0)
		printk (KERN_INFO "snd_soc_jack_add_gpios failed..%d\n", gpiostatus);

	DBG ("%s: Exiting \n", __func__);
	return ret;
}


/*
 * kc1_headset_jack_status_check
 * This function is to check the Headset Jack Status
 */
static int kc1_headset_jack_status_check (void)
{
	int gpio_status;

	DBG ("%s: Entered \n", __func__);
	gpio_status = gpio_get_value (KC1_HEADSET_DETECT_GPIO_PIN);
	//DBG ("##kc1_headset_jack_status_check invoked. GPIO %d\n", gpio_status);
	/* If the js_jack codec Member is not empty, Invoke the headset_Speaker_Path routine
	* Codec Driver.
	*/
	if (hs_jack.codec != NULL)
		aic31xx_headset_speaker_path (hs_jack.codec, gpio_status);

	DBG ("%s: Exiting \n", __func__);
	return 0;
}

/*
 * mcbsp_be_hw_params_fixup
 * This function will be invoked by the SOC-Core layer
 * during the back-end DAI initialization.
 */

static int mcbsp_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
			struct snd_pcm_hw_params *params)
{
	struct snd_interval *channels = hw_param_interval(params,
                                       SNDRV_PCM_HW_PARAM_CHANNELS);
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	unsigned int be_id;
	unsigned int threshold;
	unsigned int val, min_mask;
	DBG ("%s: Entered \n", __func__);
	//DBG ("%s: CPU DAI %s BE_ID %d\n", __func__, cpu_dai->name, rtd->dai_link->be_id);

	be_id = rtd->dai_link->be_id;

	switch (be_id) {
	case OMAP_ABE_DAI_MM_FM:
		channels->min = 2;
		threshold = 2;
                val = SNDRV_PCM_FORMAT_S16_LE;
		break;
	case OMAP_ABE_DAI_BT_VX:
		channels->min = 1;
		threshold = 1;
                val = SNDRV_PCM_FORMAT_S16_LE;
		break;
	default:
		threshold = 1;
                val = SNDRV_PCM_FORMAT_S16_LE;
		break;
	}
        
        min_mask = snd_mask_min(&params->masks[SNDRV_PCM_HW_PARAM_FORMAT -     
                                               SNDRV_PCM_HW_PARAM_FIRST_MASK]);

	//DBG ("%s: Returned min_mask 0x%x Format %x\n", __func__, min_mask, val);

        snd_mask_reset(&params->masks[SNDRV_PCM_HW_PARAM_FORMAT - 
                                      SNDRV_PCM_HW_PARAM_FIRST_MASK],
			              min_mask);

	//DBG ("%s: Returned min_mask 0x%x Format %x\n", __func__, min_mask, val);

        snd_mask_set(&params->masks[SNDRV_PCM_HW_PARAM_FORMAT -
                                    SNDRV_PCM_HW_PARAM_FIRST_MASK], val);	

	omap_mcbsp_set_tx_threshold(cpu_dai->id, threshold);
	omap_mcbsp_set_rx_threshold(cpu_dai->id, threshold);
	
	DBG ("%s: Exiting \n", __func__);
	return 0;
}

static const char *mm1_be[] = {
		OMAP_ABE_BE_MM_EXT0,
};

static const char *mm_lp_be[] = {
		OMAP_ABE_BE_MM_EXT0,
};
/* ABE Port configuration structure introduced within the
* DAI_LINK Structure as private_data void pointer member
*/
t_port_config mm_ext0_config = {
	/* uplink port configuration */
	.abe_port_id_ul = MM_EXT_IN_PORT,
	.serial_id_ul = MCBSP3_RX,
	.sample_format_ul = STEREO_RSHIFTED_16, 
#ifdef CONFIG_ABE_44100
	.sample_rate_ul = 44100,
#else
	.sample_rate_ul = 48000,
#endif	
	.bit_reorder_ul = 0,

	/* down link port configuration */
	.abe_port_id_dl = MM_EXT_OUT_PORT,
	.serial_id_dl = MCBSP3_TX,
	.sample_format_dl = STEREO_RSHIFTED_16, 
#ifdef CONFIG_ABE_44100
	.sample_rate_dl = 44100,
#else
	.sample_rate_dl = 48000,
#endif

	.bit_reorder_dl = 0,
};

/* DAI_LINK Structure definition with both Front-End and
 * Back-end DAI Declarations.
 */
static struct snd_soc_dai_link omap4_dai_abe[] = {
	{
		.name = "tlv320aic3110 Media LP",
		.stream_name = "Multimedia",

		/* ABE components - MM-DL (mmap) */
		.cpu_dai_name = "MultiMedia1 LP",
		.platform_name = "omap-aess-audio",

		.dynamic = 1, /* BE is dynamic */
		.supported_be = mm_lp_be,
		.num_be = ARRAY_SIZE(mm_lp_be),
		.fe_playback_channels = 2,
		.no_host_mode = SND_SOC_DAI_LINK_OPT_HOST,
	},
	{
		.name = "tlv320aic3110 Media",
		.stream_name = "Multimedia",

		/* ABE components - MM-UL & MM_DL */
		.cpu_dai_name = "MultiMedia2",
		.platform_name = "omap-pcm-audio",

		.dynamic = 1, /* BE is dynamic */
		.supported_be = mm1_be,
		.num_be = ARRAY_SIZE(mm1_be),
		.fe_playback_channels = 2,
		.fe_capture_channels = 8,
	},
/*
 * Backend DAIs - i.e. dynamically matched interfaces, invisible to userspace.
 * Matched to above interfaces at runtime, based upon use case.
 */

	{
		.name = OMAP_ABE_BE_MM_EXT0,
		.stream_name = "FM",

		/* ABE components - MCBSP3 - MM-EXT */
		.cpu_dai_name = "omap-mcbsp-dai.2",
		.platform_name = "omap-aess-audio",

		/* FM */
		.codec_dai_name = "tlv320aic3110-MM_EXT",
		.codec_name = "tlv320aic3110-codec",

		.no_pcm = 1, /* don't create ALSA pcm for this */
		.be_hw_params_fixup = mcbsp_be_hw_params_fixup,
		.ops = &omap4_ops,
		.be_id = OMAP_ABE_DAI_MM_FM,
		.private_data = &mm_ext0_config,
		.init = omap4_aic31xx_init,
	},
};

/* The below mentioend DAI LINK Structure is to be used in non-ABE Mode only 
 * when the AIC3110 Audio Codec Chipset is interfaced via McBSP4 Port
 */

/* Digital audio interface glue - connects codec <--> CPU */
static struct snd_soc_dai_link omap4_dai[] = {
	{
		/* .name = "AIC31XX", */
		.name = "tlv320aic3110",
		.stream_name = "AIC31XX Audio",
		.cpu_dai_name = "omap-mcbsp-dai.3",
		.codec_dai_name = "tlv320aic3110-MM_EXT",
		.platform_name = "omap-pcm-audio",
		.codec_name = "tlv320aic3110-codec",
		.init = omap4_aic31xx_init,
		.ops = &omap4_ops,
	},
};

/* Audio machine driver */
static struct snd_soc_card snd_soc_omap4_panda = {
	.name = "OMAP4_KC1",
	.long_name = "OMAP4_KC1_AIC3110",
	.dai_link = omap4_dai,
	.num_links = ARRAY_SIZE(omap4_dai),
};

/* Audio machine driver with ABE Support */
static struct snd_soc_card snd_soc_omap4_panda_abe = {
	.name = "OMAP4_KC1-ABE",
	.long_name = "OMAP4_KC1_AIC3110-ABE",
	.dai_link = omap4_dai_abe,
	.num_links = ARRAY_SIZE(omap4_dai_abe),
};

static struct platform_device *omap4_snd_device;

/*
 * omap4_panda_soc_init
 * This function is used to initialize the machine Driver.
 */
static int __init omap4_panda_soc_init(void)
{
	int ret = 0;
	/* struct i2c_adapter *adapter; */
	unsigned char board_ver = 0;

	printk(KERN_INFO "Entered omap4_evt_soc_init>...\n");

	printk(KERN_INFO "OMAP4 EVT SoC init\n");

	omap4_snd_device = platform_device_alloc("soc-audio", -1);

	if (!omap4_snd_device) {
		printk(KERN_ERR "Platform device allocation failed\n");
		return -ENOMEM;
	}
#ifdef CONFIG_ABE_44100
#warning "Configuring the ABE at 44.1 Khz."
#endif
	board_ver = quanta_get_mbid();
	//board_ver = 3;
	if(board_ver >= 3) {
		printk(KERN_INFO "Found Board EVT 2.1 then we use ABE support\n");
		platform_set_drvdata(omap4_snd_device, &snd_soc_omap4_panda_abe);
	} else {
		printk(KERN_INFO "Not found Board EVT 2.1 then we don't use ABE support\n");
		platform_set_drvdata(omap4_snd_device, &snd_soc_omap4_panda);
	}
	ret = platform_device_add(omap4_snd_device);
	if (ret) {
		printk(KERN_INFO "platform device add failed...\n");
		goto err1;
	}

	printk(KERN_INFO "OMAP4 EVT Soc Init success..\n");

	return 0;

err1:
	printk(KERN_ERR "Unable to add platform device\n");
	platform_device_put(omap4_snd_device);

	return ret;
}
module_init(omap4_panda_soc_init);

/*
 * omap4_soc_exit
 * This function is used to exit the machine Driver.
 */
static void __exit omap4_soc_exit(void)
{
	snd_soc_jack_free_gpios(&hs_jack, ARRAY_SIZE(hs_jack_gpios),
				hs_jack_gpios);

	platform_device_unregister(omap4_snd_device);
	i2c_unregister_device(tlv320aic31xx_client);
}
module_exit(omap4_soc_exit);

MODULE_AUTHOR("Santosh Sivaraj <santosh.s@mistralsolutions.com>");
MODULE_DESCRIPTION("ALSA SoC OMAP4 Panda");
MODULE_LICENSE("GPL");
