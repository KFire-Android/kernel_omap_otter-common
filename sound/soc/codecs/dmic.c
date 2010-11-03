/*
 * dmic.c  --  SoC audio for Generic Digital MICs
 *
 * Author: Liam Girdwood <lrg@slimlogic.co.uk>
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

#include <linux/platform_device.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>


struct dmic_data {
	void *data;
};

static int dmic_set_dai_sysclk(struct snd_soc_dai *codec_dai,
		int clk_id, unsigned int freq, int dir)
{
	return 0;
}

static int dmic_startup(struct snd_pcm_substream *substream,
			struct snd_soc_dai *dai)
{
	return 0;
}

static int dmic_trigger(struct snd_pcm_substream *substream,
			int cmd, struct snd_soc_dai *dai)
{
	return 0;
}

static int dmic_hw_params(struct snd_pcm_substream *substream,
			struct snd_pcm_hw_params *params,
			struct snd_soc_dai *dai)
{
	return 0;
}

static struct snd_soc_dai_ops dmic_dai_ops = {
	.startup	= dmic_startup,
	.hw_params	= dmic_hw_params,
	.trigger	= dmic_trigger,
	.set_sysclk	= dmic_set_dai_sysclk,
};

static struct snd_soc_dai_driver dmic_dai = {
	.name = "dmic-hifi",
	.capture = {
		.stream_name = "Capture",
		.channels_min = 2,
		.channels_max = 2,
		.rates = SNDRV_PCM_RATE_96000 | SNDRV_PCM_RATE_192000,
		.formats = SNDRV_PCM_FMTBIT_S32_LE,
	},
	.ops = &dmic_dai_ops,
};

#ifdef CONFIG_PM
static int dmic_suspend(struct snd_soc_codec *codec, pm_message_t state)
{
//	dmic_set_bias_level(codec, SND_SOC_BIAS_OFF);

	return 0;
}

static int dmic_resume(struct snd_soc_codec *codec)
{
	//dmic_set_bias_level(codec, SND_SOC_BIAS_STANDBY);
//	dmic_set_bias_level(codec, codec->suspend_bias_level);

	return 0;
}
#else
#define dmic_suspend NULL
#define dmic_resume NULL
#endif

static int dmic_probe(struct snd_soc_codec *codec)
{
//	struct dmic_platform_data *dmic_pdata = codec->dev->platform_data;
	struct dmic_data *priv;
//	int ret = 0;

	priv = kzalloc(sizeof(struct dmic_data), GFP_KERNEL);
	if (priv == NULL)
		return -ENOMEM;
	snd_soc_codec_set_drvdata(codec, priv);

#if 0
	/* power on device */
	ret = dmic_set_bias_level(codec, SND_SOC_BIAS_STANDBY);
	if (ret)
		goto irq_err;

	snd_soc_add_controls(codec, dmic_snd_controls,
				ARRAY_SIZE(dmic_snd_controls));
	dmic_add_widgets(codec);
#endif
	return 0;
}

static int dmic_remove(struct snd_soc_codec *codec)
{
	struct dmic_data *priv = snd_soc_codec_get_drvdata(codec);

//	dmic_set_bias_level(codec, SND_SOC_BIAS_OFF);
	kfree(priv);

	return 0;
}

struct snd_soc_codec_driver soc_dmic = {
	.probe = dmic_probe,
	.remove = dmic_remove,
	.suspend = dmic_suspend,
	.resume = dmic_resume,
//	.read = dmic_read_reg_cache,
//	.write = dmic_write,
//	.set_bias_level = dmic_set_bias_level,
//	.reg_cache_size = ARRAY_SIZE(dmic_reg),
//	.reg_cache_default = dmic_reg,
};

static int __devinit dmic_dev_probe(struct platform_device *pdev)
{
	return snd_soc_register_codec(&pdev->dev,
			&soc_dmic, &dmic_dai, 1);
}

static int __devexit dmic_dev_remove(struct platform_device *pdev)
{
	snd_soc_unregister_codec(&pdev->dev);
	return 0;
}

static struct platform_driver dmic_driver = {
	.driver = {
		.name = "dmic-codec",
		.owner = THIS_MODULE,
	},
	.probe = dmic_dev_probe,
	.remove = __devexit_p(dmic_dev_remove),
};

static int __init dmic_init(void)
{
	return platform_driver_register(&dmic_driver);
}
module_init(dmic_init);

static void __exit dmic_exit(void)
{
	platform_driver_unregister(&dmic_driver);
}
module_exit(dmic_exit);

MODULE_DESCRIPTION("Generic DMIC driver");
MODULE_AUTHOR("Liam Girdwood <lrg@slimlogic.co.uk>");
MODULE_LICENSE("GPL");
