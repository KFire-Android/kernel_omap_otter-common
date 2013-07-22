/*
 * dr7-evm.c  --  SoC audio for TI DRA7 EVM
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

#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/pcm_params.h>
#include <sound/soc-dapm.h>

struct dra7_snd_data {
	unsigned int media_mclk_freq;
	int media_slots;
};

static int dra7_mcasp_reparent(struct snd_soc_card *card,
			       const char *fclk_name,
			       const char *parent_name)
{
	struct clk *gfclk, *parent_clk;
	int ret;

	gfclk = clk_get(card->dev, fclk_name);
	if (IS_ERR(gfclk)) {
		dev_err(card->dev, "failed to get %s\n", fclk_name);
		return PTR_ERR(gfclk);
	}

	parent_clk = clk_get(card->dev, parent_name);
	if (IS_ERR(parent_clk)) {
		dev_err(card->dev, "failed to get new parent clock %s\n",
			parent_name);
		ret = PTR_ERR(parent_clk);
		goto err1;
	}

	ret = clk_set_parent(gfclk, parent_clk);
	if (ret) {
		dev_err(card->dev, "failed to reparent %s\n", fclk_name);
		goto err2;
	}

err2:
	clk_put(parent_clk);
err1:
	clk_put(gfclk);
	return ret;
}

static unsigned int dra7_get_bclk(struct snd_pcm_hw_params *params, int slots)
{
	int sample_size = snd_pcm_format_width(params_format(params));
	int rate = params_rate(params);

	return sample_size * slots * rate;
}

static int dra7_snd_media_hw_params(struct snd_pcm_substream *substream,
				    struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_card *card = rtd->card;
	struct dra7_snd_data *card_data = snd_soc_card_get_drvdata(card);
	unsigned int fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_CBS_CFS;
	unsigned int bclk_freq;
	int ret;

	bclk_freq = dra7_get_bclk(params, card_data->media_slots);
	if (card_data->media_mclk_freq % bclk_freq) {
		dev_err(card->dev, "can't produce exact sample freq\n");
		return -EPERM;
	}

	/* McASP driver requires inverted frame for I2S */
	ret = snd_soc_dai_set_fmt(cpu_dai, fmt | SND_SOC_DAIFMT_NB_IF);
	if (ret < 0) {
		dev_err(card->dev, "can't set CPU DAI format %d\n", ret);
		return ret;
	}

	/* Set McASP BCLK divider (clkid = 1) */
	ret = snd_soc_dai_set_clkdiv(cpu_dai, 1,
				     card_data->media_mclk_freq / bclk_freq);
	if (ret < 0) {
		dev_err(card->dev, "can't set CPU DAI clock divider %d\n", ret);
		return ret;
	}

	/* Set McASP sysclk from AHCLKX sourced from ATL */
	ret = snd_soc_dai_set_sysclk(cpu_dai, 0,
				     card_data->media_mclk_freq,
				     SND_SOC_CLOCK_IN);
	if (ret < 0) {
		dev_err(card->dev, "can't set CPU DAI sysclk %d\n", ret);
		return ret;
	}

	ret = snd_soc_dai_set_fmt(codec_dai, fmt | SND_SOC_DAIFMT_NB_NF);
	if (ret < 0) {
		dev_err(card->dev, "can't set CODEC DAI format %d\n", ret);
		return ret;
	}

	/* Set MCLK as clock source for tlv320aic3106 */
	ret = snd_soc_dai_set_sysclk(codec_dai, 0,
				     card_data->media_mclk_freq,
				     SND_SOC_CLOCK_IN);
	if (ret < 0)
		dev_err(card->dev, "can't set CODEC sysclk %d\n", ret);

	return ret;
}

static struct snd_soc_ops dra7_snd_media_ops = {
	.hw_params = dra7_snd_media_hw_params,
};

static struct snd_soc_dai_link dra7_snd_dai[] = {
	{
		/* Media: McASP3 + tlv320aic3106 */
		.name = "Media",
		.codec_dai_name = "tlv320aic3x-hifi",
		.platform_name = "omap-pcm-audio",
		.ops = &dra7_snd_media_ops,
	},
};

static int dra7_snd_add_dai_link(struct snd_soc_card *card,
				 struct snd_soc_dai_link *dai_link,
				 const char *prefix)
{
	struct dra7_snd_data *card_data = snd_soc_card_get_drvdata(card);
	struct device_node *node = card->dev->of_node;
	struct device_node *dai_node;
	char prop[32];
	int ret;

	if (!node) {
		dev_err(card->dev, "card node is invalid\n");
		return -EINVAL;
	}

	snprintf(prop, sizeof(prop), "%s-mclk-freq", prefix);
	of_property_read_u32(node, prop,
			     &card_data->media_mclk_freq);

	snprintf(prop, sizeof(prop), "%s-cpu", prefix);
	dai_node = of_parse_phandle(node, prop, 0);
	if (!dai_node) {
		dev_err(card->dev, "cpu dai node is invalid\n");
		return -EINVAL;
	}

	dai_link->cpu_of_node = dai_node;

	snprintf(prop, sizeof(prop), "%s-codec", prefix);
	dai_node = of_parse_phandle(node, prop, 0);
	if (!dai_node) {
		dev_err(card->dev, "codec dai node is invalid\n");
		return -EINVAL;
	}

	dai_link->codec_of_node = dai_node;

	snprintf(prop, sizeof(prop), "%s-slots", prefix);
	of_property_read_u32(node, prop, &card_data->media_slots);
	if ((card_data->media_slots < 1) ||
	    (card_data->media_slots > 32)) {
		dev_err(card->dev, "invalid media slot count %d\n",
			card_data->media_slots);
		return -EINVAL;
	}

	ret = snd_soc_card_new_dai_links(card, dai_link, 1);
	if (ret < 0) {
		dev_err(card->dev, "failed to add dai link %s\n",
			dai_link->name);
		return ret;
	}

	return 0;
}

/* DRA7 CPU board widgets */
static const struct snd_soc_dapm_widget dra7_snd_dapm_widgets[] = {
	/* CPU board input */
	SND_SOC_DAPM_MIC("Main Mic", NULL),
	SND_SOC_DAPM_LINE("Line In", NULL),

	/* CPU board outputs */
	SND_SOC_DAPM_HP("Headphone", NULL),
	SND_SOC_DAPM_LINE("Line Out", NULL),
};

/* Audio machine driver */
static struct snd_soc_card dra7_snd_card = {
	.owner = THIS_MODULE,
	.dapm_widgets = dra7_snd_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(dra7_snd_dapm_widgets),
};

static int dra7_snd_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct snd_soc_card *card = &dra7_snd_card;
	struct dra7_snd_data *card_data;
	int ret;

	/*
	 * HACK: DMA CROSSBAR
	 * CTRL_CORE_DMA_SYSTEM_DREQ_62_63
	 *    McASP6 TX: DREQ_139 -> sDMA_62
	 *    McASP6 RX: DREQ_138 -> sDMA_63
	 * CTRL_CORE_DMA_SYSTEM_DREQ_78_79
	 *    McASP3 TX: DREQ_133 -> sDMA_78
	 *    McASP3 RX: DREQ_132 -> sDMA_79
	 */
	void __iomem *dma_sys_dreq = ioremap(0x4A002B78, SZ_1K);
	__raw_writel(0x008a008b, dma_sys_dreq + 0x7c); /* DREQ_62_63 */
	__raw_writel(0x00840085, dma_sys_dreq + 0x9c); /* DREQ_78_79 */
	iounmap(dma_sys_dreq);

	card->dev = &pdev->dev;

	card_data = devm_kzalloc(&pdev->dev, sizeof(*card_data), GFP_KERNEL);
	if (card_data == NULL)
		return -ENOMEM;

	if (!node) {
		dev_err(card->dev, "missing of_node\n");
		return -ENODEV;
	}

	ret = snd_soc_of_parse_card_name(card, "ti,model");
	if (ret) {
		dev_err(card->dev, "card name is not provided\n");
		return -ENODEV;
	}

	ret = snd_soc_of_parse_audio_routing(card,
					     "ti,audio-routing");
	if (ret) {
		dev_err(card->dev, "failed to parse DAPM routing\n");
		return ret;
	}

	snd_soc_card_set_drvdata(card, card_data);

	ret = dra7_snd_add_dai_link(card, &dra7_snd_dai[0], "ti,media");
	if (ret) {
		dev_err(card->dev, "failed to add media dai link %d\n", ret);
		return ret;
	}

	ret = snd_soc_register_card(card);
	if (ret) {
		dev_err(card->dev, "failed to register sound card %d\n", ret);
		goto err_card;
	}

	ret = dra7_mcasp_reparent(card, "mcasp3_ahclkx_mux", "atl_clkin2_ck");
	if (ret) {
		dev_err(card->dev, "failed to reparent McASP3 %d\n", ret);
		goto err_reparent;
	}

	return 0;

err_reparent:
	snd_soc_unregister_card(card);
err_card:
	snd_soc_card_reset_dai_links(card);
	return ret;
}
static int dra7_snd_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);

	snd_soc_unregister_card(card);
	snd_soc_card_reset_dai_links(card);

	return 0;
}

static const struct of_device_id dra7_snd_of_match[] = {
	{.compatible = "ti,dra7-evm-sound", },
	{ },
};
MODULE_DEVICE_TABLE(of, dra7_snd_of_match);

static struct platform_driver dra7_snd_driver = {
	.driver = {
		.name = "dra7-evm-sound",
		.owner = THIS_MODULE,
		.pm = &snd_soc_pm_ops,
		.of_match_table = dra7_snd_of_match,
	},
	.probe = dra7_snd_probe,
	.remove = dra7_snd_remove,
};

module_platform_driver(dra7_snd_driver);

MODULE_AUTHOR("Misael Lopez Cruz <misael.lopez@ti.com>");
MODULE_DESCRIPTION("ALSA SoC for DRA7 EVM");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:dra7-evm-sound");
