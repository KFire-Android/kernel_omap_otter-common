/*
 * omap-dmic.c  --  OMAP ASoC DMIC DAI driver
 *
 * Copyright (C) 2010 Texas Instruments
 *
 * Author: Liam Girdwood <lrg@slimlogic.co.uk>
 *	   David Lambert <dlambert@ti.com>
 *	   Misael Lopez Cruz <misael.lopez@ti.com>
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

#undef DEBUG

#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/wait.h>
#include <linux/interrupt.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/i2c/twl.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/slab.h>
#include <linux/pm_runtime.h>

#include <plat/control.h>
#include <plat/dma.h>
#include <plat/omap_hwmod.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/initval.h>
#include <sound/soc.h>

#include "omap-dmic.h"
#include "omap-pcm.h"

#define OMAP_DMIC_RATES		(SNDRV_PCM_RATE_96000 | SNDRV_PCM_RATE_192000)
#define OMAP_DMIC_FORMATS	SNDRV_PCM_FMTBIT_S32_LE

struct omap_dmic {
	struct device *dev;
	void __iomem *io_base;
	int irq;
	int clk_freq;
	int sysclk;
	int active;
	spinlock_t lock;
	struct omap_dmic_link *link;
};

static struct omap_dmic_link omap_dmic_link = {
	.irq_mask	= DMIC_IRQ_EMPTY | DMIC_IRQ_FULL,
	.threshold	= 2,
	.format		= DMICOUTFORMAT_LJUST,
	.polar		= DMIC_POLAR1 | DMIC_POLAR2 | DMIC_POLAR3,
};

/*
 * Stream DMA parameters
 */
static struct omap_pcm_dma_data omap_dmic_dai_dma_params = {
	.name		= "DMIC capture",
	.data_type	= OMAP_DMA_DATA_TYPE_S32,
	.sync_mode	= OMAP_DMA_SYNC_PACKET,
	.packet_size	= 2,
	.port_addr	= OMAP44XX_DMIC_L3_BASE + DMIC_DATA,
};


static inline void omap_dmic_write(struct omap_dmic *dmic,
		u16 reg, u32 val)
{
	__raw_writel(val, dmic->io_base + reg);
}

static inline int omap_dmic_read(struct omap_dmic *dmic, u16 reg)
{
	return __raw_readl(dmic->io_base + reg);
}

#ifdef DEBUG
static void omap_dmic_reg_dump(struct omap_dmic *dmic)
{
	dev_dbg(dmic->dev, "***********************\n");
	dev_dbg(dmic->dev, "DMIC_IRQSTATUS_RAW:  0x%04x\n",
		omap_dmic_read(dmic, DMIC_IRQSTATUS_RAW));
	dev_dbg(dmic->dev, "DMIC_IRQSTATUS:  0x%04x\n",
		omap_dmic_read(dmic, DMIC_IRQSTATUS));
	dev_dbg(dmic->dev, "DMIC_IRQENABLE_SET:  0x%04x\n",
		omap_dmic_read(dmic, DMIC_IRQENABLE_SET));
	dev_dbg(dmic->dev, "DMIC_IRQENABLE_CLR:  0x%04x\n",
		omap_dmic_read(dmic, DMIC_IRQENABLE_CLR));
	dev_dbg(dmic->dev, "DMIC_IRQWAKE_EN:  0x%04x\n",
		omap_dmic_read(dmic, DMIC_IRQWAKE_EN));
	dev_dbg(dmic->dev, "DMIC_DMAENABLE_SET:  0x%04x\n",
		omap_dmic_read(dmic, DMIC_DMAENABLE_SET));
	dev_dbg(dmic->dev, "DMIC_DMAENABLE_CLR:  0x%04x\n",
		omap_dmic_read(dmic, DMIC_DMAENABLE_CLR));
	dev_dbg(dmic->dev, "DMIC_DMAWAKEEN:  0x%04x\n",
		omap_dmic_read(dmic, DMIC_DMAWAKEEN));
	dev_dbg(dmic->dev, "DMIC_CTRL:  0x%04x\n",
		omap_dmic_read(dmic, DMIC_CTRL));
	dev_dbg(dmic->dev, "DMIC_DATA:  0x%04x\n",
		omap_dmic_read(dmic, DMIC_DATA));
	dev_dbg(dmic->dev, "DMIC_FIFO_CTRL:  0x%04x\n",
		omap_dmic_read(dmic, DMIC_FIFO_CTRL));
	dev_dbg(dmic->dev, "DMIC_FIFO_DMIC1R_DATA:  0x%08x\n",
		omap_dmic_read(dmic, DMIC_FIFO_DMIC1R_DATA));
	dev_dbg(dmic->dev, "DMIC_FIFO_DMIC1L_DATA:  0x%08x\n",
		omap_dmic_read(dmic, DMIC_FIFO_DMIC1L_DATA));
	dev_dbg(dmic->dev, "DMIC_FIFO_DMIC2R_DATA:  0x%08x\n",
		omap_dmic_read(dmic, DMIC_FIFO_DMIC2R_DATA));
	dev_dbg(dmic->dev, "DMIC_FIFO_DMIC2L_DATA:  0x%08x\n",
		omap_dmic_read(dmic, DMIC_FIFO_DMIC2L_DATA));
	dev_dbg(dmic->dev, "DMIC_FIFO_DMIC3R_DATA:  0x%08x\n",
		omap_dmic_read(dmic, DMIC_FIFO_DMIC3R_DATA));
	dev_dbg(dmic->dev, "DMIC_FIFO_DMIC3L_DATA:  0x%08x\n",
		omap_dmic_read(dmic, DMIC_FIFO_DMIC3L_DATA));
	dev_dbg(dmic->dev, "***********************\n");
}
#else
static void omap_dmic_reg_dump(struct omap_dmic *dmic) {}
#endif

/*
 * Enables the transfer through the DMIC interface
 */
static void omap_dmic_start(struct omap_dmic *dmic, int channels)
{
	int ctrl = omap_dmic_read(dmic, DMIC_CTRL);
	omap_dmic_write(dmic, DMIC_CTRL, ctrl | channels);
}

/*
 * Disables the transfer through the DMIC interface
 */
static void omap_dmic_stop(struct omap_dmic *dmic, int channels)
{
	int ctrl = omap_dmic_read(dmic, DMIC_CTRL);
	omap_dmic_write(dmic, DMIC_CTRL, ctrl & ~channels);
}

/*
 * Configures DMIC for audio recording.
 * This function should be called before omap_dmic_start.
 */
static int omap_dmic_open(struct omap_dmic *dmic)
{
	struct omap_dmic_link *link = dmic->link;
	int ctrl;

	/* Enable irq request generation */
	omap_dmic_write(dmic, DMIC_IRQENABLE_SET,
			link->irq_mask & DMIC_IRQ_MASK);

	/* Configure uplink threshold */
	if (link->threshold > DMIC_THRES_MAX)
		link->threshold = DMIC_THRES_MAX;

	omap_dmic_write(dmic, DMIC_FIFO_CTRL, link->threshold);

	/* Configure DMA controller */
	omap_dmic_write(dmic, DMIC_DMAENABLE_SET, DMIC_DMA_ENABLE);

	/* Set dmic out format */
	ctrl = omap_dmic_read(dmic, DMIC_CTRL)
		& ~(DMIC_FORMAT | DMIC_POLAR_MASK);
	omap_dmic_write(dmic, DMIC_CTRL,
			ctrl | link->format | link->polar);

	return 0;
}

/*
 * Cleans DMIC uplink configuration.
 * This function should be called when the stream is closed.
 */
static int omap_dmic_close(struct omap_dmic *dmic)
{
	struct omap_dmic_link *link = dmic->link;

	/* Disable irq request generation */
	omap_dmic_write(dmic, DMIC_IRQENABLE_CLR,
			link->irq_mask & DMIC_IRQ_MASK);

	/* Disable DMA request generation */
	omap_dmic_write(dmic, DMIC_DMAENABLE_CLR, DMIC_DMA_ENABLE);

	return 0;
}

static irqreturn_t omap_dmic_irq_handler(int irq, void *dev_id)
{
	struct omap_dmic *dmic = dev_id;
	int irq_status;

	irq_status = omap_dmic_read(dmic, DMIC_IRQSTATUS);

	/* Acknowledge irq event */
	omap_dmic_write(dmic, DMIC_IRQSTATUS, irq_status);
	if (irq_status & DMIC_IRQ_FULL)
		dev_dbg(dmic->dev, "DMIC FIFO error %x\n", irq_status);

	if (irq_status & DMIC_IRQ_EMPTY)
		dev_dbg(dmic->dev, "DMIC FIFO error %x\n", irq_status);

	if (irq_status & DMIC_IRQ)
		dev_dbg(dmic->dev, "DMIC write request\n");

	return IRQ_HANDLED;
}

static int omap_dmic_dai_startup(struct snd_pcm_substream *substream,
				  struct snd_soc_dai *dai)
{
	struct omap_dmic *dmic = snd_soc_dai_get_drvdata(dai);

	if (!dmic->active++) {
		pm_runtime_get_sync(dmic->dev);
		/* Enable DMIC bias */
		/* TODO: convert this over to DAPM */
		twl_i2c_write_u8(TWL_MODULE_AUDIO_VOICE, 0x55, 0x0B);
	}

	return 0;
}

static void omap_dmic_dai_shutdown(struct snd_pcm_substream *substream,
				    struct snd_soc_dai *dai)
{
	struct omap_dmic *dmic = snd_soc_dai_get_drvdata(dai);

	if (!--dmic->active) {
		/* Disable DMIC bias */
		/* TODO: convert this over to DAPM */
		twl_i2c_write_u8(TWL_MODULE_AUDIO_VOICE, 0x44, 0x0B);
		pm_runtime_put_sync(dmic->dev);
	}
}

static int omap_dmic_dai_hw_params(struct snd_pcm_substream *substream,
				    struct snd_pcm_hw_params *params,
				    struct snd_soc_dai *dai)
{
	struct omap_dmic *dmic = snd_soc_dai_get_drvdata(dai);
	struct omap_dmic_link *link = dmic->link;
	int channels;
	int ret = 0;

	channels = params_channels(params);
	switch (channels) {
	case 1:
	case 2:
		link->channels = 2;
		break;
	default:
		dev_err(dmic->dev, "invalid number of channels\n");
		return -EINVAL;
	}

	omap_dmic_dai_dma_params.packet_size = link->threshold * link->channels;
	snd_soc_dai_set_dma_data(dai, substream, &omap_dmic_dai_dma_params);

	if (dmic->active == 1)
		ret = omap_dmic_open(dmic);

	return ret;
}

static int omap_dmic_dai_hw_free(struct snd_pcm_substream *substream,
				  struct snd_soc_dai *dai)
{
	struct omap_dmic *dmic = snd_soc_dai_get_drvdata(dai);
	struct omap_dmic_link *link = dmic->link;
	int ret = 0;

	if (dmic->active == 1) {
		ret = omap_dmic_close(dmic);
		link->channels = 0;
	}

	return ret;
}

static int omap_dmic_dai_trigger(struct snd_pcm_substream *substream,
				  int cmd, struct snd_soc_dai *dai)
{
	struct omap_dmic *dmic = snd_soc_dai_get_drvdata(dai);
	int dmic_id = 1 << dai->id;

	omap_dmic_reg_dump(dmic);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		omap_dmic_start(dmic, dmic_id);
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		break;
	case SNDRV_PCM_TRIGGER_STOP:
		omap_dmic_stop(dmic, dmic_id);
		break;
	default:
		break;
	}

	return 0;
}

static int omap_dmic_set_dai_sysclk(struct snd_soc_dai *dai,
				    int clk_id, unsigned int freq,
				    int dir)
{
	struct omap_dmic *dmic = snd_soc_dai_get_drvdata(dai);
	struct clk *dmic_clk, *parent_clk;
	int ret = 0;

	dmic_clk = clk_get(NULL, "dmic_fck");
	if (IS_ERR(dmic_clk))
		return -ENODEV;

	switch (clk_id) {
	case OMAP_DMIC_SYSCLK_PAD_CLKS:
		parent_clk = clk_get(NULL, "pad_clks_ck");
		if (IS_ERR(parent_clk)) {
			ret = -ENODEV;
			goto err_par;
		}
		break;
	case OMAP_DMIC_SYSCLK_SLIMBLUS_CLKS:
		parent_clk = clk_get(NULL, "slimbus_clk");
		if (IS_ERR(parent_clk)) {
			ret = -ENODEV;
			goto err_par;
		}
		break;
	case OMAP_DMIC_SYSCLK_SYNC_MUX_CLKS:
		parent_clk = clk_get(NULL, "dmic_sync_mux_ck");
		if (IS_ERR(parent_clk)) {
			ret = -ENODEV;
			goto err_par;
		}
		break;
	default:
		dev_err(dai->dev, "clk_id not supported %d\n", clk_id);
		ret = -EINVAL;
		goto err_par;
	}

	switch (freq) {
	case 19200000:
	case 24000000:
	case 24576000:
	case 12000000:
		dmic->clk_freq = freq;
		break;
	default:
		dev_err(dai->dev, "clk freq not supported %d\n", freq);
		ret = -EINVAL;
		goto err_freq;
	}

	if (dmic->sysclk != clk_id) {
		/* reparent not allowed if a stream is ongoing */
		if (dmic->active > 1)
			return -EBUSY;

		/* disable clock while reparenting */
		if (dmic->active == 1)
			pm_runtime_put_sync(dmic->dev);

		ret = clk_set_parent(dmic_clk, parent_clk);

		if (dmic->active == 1)
			pm_runtime_get_sync(dmic->dev);

		dmic->sysclk = clk_id;
	}

err_freq:
	clk_put(parent_clk);
err_par:
	clk_put(dmic_clk);

	return ret;
}

static int omap_dmic_set_clkdiv(struct snd_soc_dai *dai,
				int div_id, int div)
{
	struct omap_dmic *dmic = snd_soc_dai_get_drvdata(dai);
	int ctrl, div_sel = -EINVAL;

	if (div_id != OMAP_DMIC_CLKDIV)
		return -ENODEV;

	switch (dmic->clk_freq) {
	case 19200000:
		if (div == 5)
			div_sel = 0x1;
		else if (div == 8)
			div_sel = 0x0;
		break;
	case 24000000:
		if (div == 10)
			div_sel = 0x2;
		break;
	case 24576000:
		if (div == 8)
			div_sel = 0x3;
		else if (div == 16)
			div_sel = 0x4;
		break;
	case 12000000:
		if (div == 5)
			div_sel = 0x5;
		break;
	default:
		dev_err(dai->dev, "invalid freq %d\n", dmic->clk_freq);
		return -EINVAL;
	}

	if (div_sel < 0) {
		dev_err(dai->dev, "divider not supported %d\n", div);
		return -EINVAL;
	}

	ctrl = omap_dmic_read(dmic, DMIC_CTRL) & ~DMIC_CLK_DIV_MASK;
	omap_dmic_write(dmic, DMIC_CTRL,
			ctrl | (div_sel << DMIC_CLK_DIV_SHIFT));

	return 0;
}

static struct snd_soc_dai_ops omap_dmic_dai_ops = {
	.startup	= omap_dmic_dai_startup,
	.shutdown	= omap_dmic_dai_shutdown,
	.hw_params	= omap_dmic_dai_hw_params,
	.trigger	= omap_dmic_dai_trigger,
	.hw_free	= omap_dmic_dai_hw_free,
	.set_sysclk	= omap_dmic_set_dai_sysclk,
	.set_clkdiv	= omap_dmic_set_clkdiv,
};

#if defined(CONFIG_SND_OMAP_SOC_ABE_DSP) || \
    defined(CONFIG_SND_OMAP_SOC_ABE_DSP_MODULE)
static int omap_dmic_abe_dai_trigger(struct snd_pcm_substream *substream,
				  int cmd, struct snd_soc_dai *dai)
{
	struct omap_dmic *dmic = snd_soc_dai_get_drvdata(dai);
	int dmic_id = DMIC_UP1_ENABLE | DMIC_UP2_ENABLE | DMIC_UP3_ENABLE;

	omap_dmic_reg_dump(dmic);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		if (dmic->active == 1)
			omap_dmic_start(dmic, dmic_id);
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		break;
	case SNDRV_PCM_TRIGGER_STOP:
		if (dmic->active == 1)
			omap_dmic_stop(dmic, dmic_id);
		break;
	default:
		break;
	}

	return 0;
}

static struct snd_soc_dai_ops omap_dmic_abe_dai_ops = {
	.startup	= omap_dmic_dai_startup,
	.shutdown	= omap_dmic_dai_shutdown,
	.hw_params	= omap_dmic_dai_hw_params,
	.trigger	= omap_dmic_abe_dai_trigger,
	.hw_free	= omap_dmic_dai_hw_free,
	.set_sysclk	= omap_dmic_set_dai_sysclk,
	.set_clkdiv	= omap_dmic_set_clkdiv,
};
#endif

static struct snd_soc_dai_driver omap_dmic_dai[] = {
{
	.name = "omap-dmic-dai-0",
	.capture = {
		.channels_min = 2,
		.channels_max = 2,
		.rates = OMAP_DMIC_RATES,
		.formats = OMAP_DMIC_FORMATS,
	},
	.ops = &omap_dmic_dai_ops,
},
{
	.name = "omap-dmic-dai-1",
	.capture = {
		.channels_min = 2,
		.channels_max = 2,
		.rates = OMAP_DMIC_RATES,
		.formats = OMAP_DMIC_FORMATS,
	},
	.ops = &omap_dmic_abe_dai_ops,
},
{
	.name = "omap-dmic-dai-2",
	.capture = {
		.channels_min = 2,
		.channels_max = 2,
		.rates = OMAP_DMIC_RATES,
		.formats = OMAP_DMIC_FORMATS,
	},
	.ops = &omap_dmic_abe_dai_ops,
},
#if defined(CONFIG_SND_OMAP_SOC_ABE_DSP) || \
    defined(CONFIG_SND_OMAP_SOC_ABE_DSP_MODULE)
{
	.name = "omap-dmic-abe-dai-0",
	.capture = {
		.channels_min = 2,
		.channels_max = 2,
		.rates = OMAP_DMIC_RATES,
		.formats = OMAP_DMIC_FORMATS,
	},
	.ops = &omap_dmic_abe_dai_ops,
},
{
	.name = "omap-dmic-abe-dai-1",
	.capture = {
		.channels_min = 2,
		.channels_max = 2,
		.rates = OMAP_DMIC_RATES,
		.formats = OMAP_DMIC_FORMATS,
	},
	.ops = &omap_dmic_abe_dai_ops,
},
{
	.name = "omap-dmic-abe-dai-2",
	.capture = {
		.channels_min = 2,
		.channels_max = 2,
		.rates = OMAP_DMIC_RATES,
		.formats = OMAP_DMIC_FORMATS,
	},
	.ops = &omap_dmic_abe_dai_ops,
},
#endif
};

static __devinit int asoc_dmic_probe(struct platform_device *pdev)
{
	struct omap_dmic *dmic;
	struct resource *res;
	int ret;

	dmic = kzalloc(sizeof(struct omap_dmic), GFP_KERNEL);
	if (!dmic)
		return -ENOMEM;

	platform_set_drvdata(pdev, dmic);
	dmic->dev = &pdev->dev;
	dmic->link = &omap_dmic_link;

	spin_lock_init(&dmic->lock);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(dmic->dev, "invalid memory resource\n");
		ret = -ENODEV;
		goto err_res;
	}

	dmic->io_base = ioremap(res->start, resource_size(res));
	if (!dmic->io_base) {
		ret = -ENOMEM;
		goto err_res;
	}

	dmic->irq = platform_get_irq(pdev, 0);
	if (dmic->irq < 0) {
		ret = dmic->irq;
		goto err_irq;
	}

	res = platform_get_resource(pdev, IORESOURCE_DMA, 0);
	if (!res) {
		dev_err(dmic->dev, "invalid dma resource\n");
		ret = -ENODEV;
		goto err_irq;
	}
	omap_dmic_dai_dma_params.dma_req = res->start;

	pm_runtime_enable(dmic->dev);

	/* Disable lines while request is ongoing */
	omap_dmic_write(dmic, DMIC_CTRL, 0x00);

	ret = request_threaded_irq(dmic->irq, NULL, omap_dmic_irq_handler,
				   IRQF_ONESHOT, "DMIC", (void *)dmic);
	if (ret) {
		dev_err(dmic->dev, "irq request failed\n");
		goto err_irq;
	}

	ret = snd_soc_register_dais(&pdev->dev, omap_dmic_dai,
				    ARRAY_SIZE(omap_dmic_dai));
	if (ret)
		goto err_dai;

	return 0;

err_dai:
	free_irq(dmic->irq, (void *)dmic);
err_irq:
	iounmap(dmic->io_base);
err_res:
	kfree(dmic);
	return ret;
}

static int __devexit asoc_dmic_remove(struct platform_device *pdev)
{
	struct omap_dmic *dmic = platform_get_drvdata(pdev);

	snd_soc_unregister_dai(&pdev->dev);
	iounmap(dmic->io_base);
	free_irq(dmic->irq, (void *)dmic);
	kfree(dmic);

	return 0;
}

static struct platform_driver asoc_dmic_driver = {
	.driver = {
		.name = "omap-dmic-dai",
		.owner = THIS_MODULE,
	},
	.probe = asoc_dmic_probe,
	.remove = __devexit_p(asoc_dmic_remove),
};

static int __init snd_omap_dmic_init(void)
{
	return platform_driver_register(&asoc_dmic_driver);
}
module_init(snd_omap_dmic_init);

static void __exit snd_omap_dmic_exit(void)
{
	platform_driver_unregister(&asoc_dmic_driver);
}
module_exit(snd_omap_dmic_exit);

MODULE_AUTHOR("David Lambert <dlambert@ti.com>");
MODULE_DESCRIPTION("OMAP DMIC SoC Interface");
MODULE_LICENSE("GPL");
