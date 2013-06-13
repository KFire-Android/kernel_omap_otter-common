/*
 * omap-mcpdm.c  --  OMAP ALSA SoC DAI driver using McPDM port
 *
 * Copyright (C) 2009 - 2011 Texas Instruments
 *
 * Author: Misael Lopez Cruz <misael.lopez@ti.com>
 * Contact: Jorge Eduardo Candelaria <x0107209@ti.com>
 *          Margarita Olaya <magi.olaya@ti.com>
 *          Peter Ujfalusi <peter.ujfalusi@ti.com>
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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/slab.h>
#include <linux/pm_runtime.h>
#include <linux/of_device.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>

#include "omap-mcpdm.h"
#include "omap-pcm.h"

#include "omap-abe-priv.h"

#define OMAP_MCPDM_LEGACY_DAI	0
#define OMAP_MCPDM_ABE_DAI	1

struct omap_mcpdm {
	struct device *dev;
	unsigned long phys_base;
	void __iomem *io_base;
	int irq;

	struct mutex mutex;

	/* channel data */
	u32 dn_channels;
	u32 up_channels;

	/* McPDM FIFO thresholds */
	u32 dn_threshold;
	u32 up_threshold;

	/* McPDM dn offsets for rx1, and 2 channels */
	u32 dn_rx_offset;

	int active;
	int active_dai;

	struct omap_aess *aess;
	struct omap_abe_port *dl_port;
	struct omap_abe_port *ul_port;
};

/*
 * Stream DMA parameters
 */
static struct omap_pcm_dma_data omap_mcpdm_dai_dma_params[] = {
	{
		.name = "Audio playback",
	},
	{
		.name = "Audio capture",
	},
};

static inline void omap_mcpdm_write(struct omap_mcpdm *mcpdm, u16 reg, u32 val)
{
	__raw_writel(val, mcpdm->io_base + reg);
}

static inline int omap_mcpdm_read(struct omap_mcpdm *mcpdm, u16 reg)
{
	return __raw_readl(mcpdm->io_base + reg);
}

#ifdef DEBUG
static void omap_mcpdm_reg_dump(struct omap_mcpdm *mcpdm)
{
	dev_dbg(mcpdm->dev, "***********************\n");
	dev_dbg(mcpdm->dev, "IRQSTATUS_RAW:  0x%04x\n",
			omap_mcpdm_read(mcpdm, MCPDM_REG_IRQSTATUS_RAW));
	dev_dbg(mcpdm->dev, "IRQSTATUS:  0x%04x\n",
			omap_mcpdm_read(mcpdm, MCPDM_REG_IRQSTATUS));
	dev_dbg(mcpdm->dev, "IRQENABLE_SET:  0x%04x\n",
			omap_mcpdm_read(mcpdm, MCPDM_REG_IRQENABLE_SET));
	dev_dbg(mcpdm->dev, "IRQENABLE_CLR:  0x%04x\n",
			omap_mcpdm_read(mcpdm, MCPDM_REG_IRQENABLE_CLR));
	dev_dbg(mcpdm->dev, "IRQWAKE_EN: 0x%04x\n",
			omap_mcpdm_read(mcpdm, MCPDM_REG_IRQWAKE_EN));
	dev_dbg(mcpdm->dev, "DMAENABLE_SET: 0x%04x\n",
			omap_mcpdm_read(mcpdm, MCPDM_REG_DMAENABLE_SET));
	dev_dbg(mcpdm->dev, "DMAENABLE_CLR:  0x%04x\n",
			omap_mcpdm_read(mcpdm, MCPDM_REG_DMAENABLE_CLR));
	dev_dbg(mcpdm->dev, "DMAWAKEEN:  0x%04x\n",
			omap_mcpdm_read(mcpdm, MCPDM_REG_DMAWAKEEN));
	dev_dbg(mcpdm->dev, "CTRL:  0x%04x\n",
			omap_mcpdm_read(mcpdm, MCPDM_REG_CTRL));
	dev_dbg(mcpdm->dev, "DN_DATA:  0x%04x\n",
			omap_mcpdm_read(mcpdm, MCPDM_REG_DN_DATA));
	dev_dbg(mcpdm->dev, "UP_DATA: 0x%04x\n",
			omap_mcpdm_read(mcpdm, MCPDM_REG_UP_DATA));
	dev_dbg(mcpdm->dev, "FIFO_CTRL_DN: 0x%04x\n",
			omap_mcpdm_read(mcpdm, MCPDM_REG_FIFO_CTRL_DN));
	dev_dbg(mcpdm->dev, "FIFO_CTRL_UP:  0x%04x\n",
			omap_mcpdm_read(mcpdm, MCPDM_REG_FIFO_CTRL_UP));
	dev_dbg(mcpdm->dev, "***********************\n");
}
#else
static void omap_mcpdm_reg_dump(struct omap_mcpdm *mcpdm) {}
#endif

/*
 * Enables the transfer through the PDM interface to/from the Phoenix
 * codec by enabling the corresponding UP or DN channels.
 */
static void omap_mcpdm_start(struct omap_mcpdm *mcpdm)
{
	u32 ctrl = omap_mcpdm_read(mcpdm, MCPDM_REG_CTRL);

	ctrl |= (MCPDM_SW_DN_RST | MCPDM_SW_UP_RST);
	omap_mcpdm_write(mcpdm, MCPDM_REG_CTRL, ctrl);

	ctrl |= mcpdm->dn_channels | mcpdm->up_channels;
	omap_mcpdm_write(mcpdm, MCPDM_REG_CTRL, ctrl);

	ctrl &= ~(MCPDM_SW_DN_RST | MCPDM_SW_UP_RST);
	omap_mcpdm_write(mcpdm, MCPDM_REG_CTRL, ctrl);
}

/*
 * Disables the transfer through the PDM interface to/from the Phoenix
 * codec by disabling the corresponding UP or DN channels.
 */
static void omap_mcpdm_stop(struct omap_mcpdm *mcpdm)
{
	u32 ctrl = omap_mcpdm_read(mcpdm, MCPDM_REG_CTRL);

	ctrl |= (MCPDM_SW_DN_RST | MCPDM_SW_UP_RST);
	omap_mcpdm_write(mcpdm, MCPDM_REG_CTRL, ctrl);

	ctrl &= ~(mcpdm->dn_channels | mcpdm->up_channels);
	omap_mcpdm_write(mcpdm, MCPDM_REG_CTRL, ctrl);

	ctrl &= ~(MCPDM_SW_DN_RST | MCPDM_SW_UP_RST);
	omap_mcpdm_write(mcpdm, MCPDM_REG_CTRL, ctrl);

}

/*
 * Is the physical McPDM interface active.
 */
static inline int omap_mcpdm_active(struct omap_mcpdm *mcpdm)
{
	return omap_mcpdm_read(mcpdm, MCPDM_REG_CTRL) &
					(MCPDM_PDM_DN_MASK | MCPDM_PDM_UP_MASK);
}

/*
 * Configures McPDM uplink, and downlink for audio.
 * This function should be called before omap_mcpdm_start.
 */
static void omap_mcpdm_open_streams(struct omap_mcpdm *mcpdm)
{
	omap_mcpdm_write(mcpdm, MCPDM_REG_IRQENABLE_SET,
			MCPDM_DN_IRQ_EMPTY | MCPDM_DN_IRQ_FULL |
			MCPDM_UP_IRQ_EMPTY | MCPDM_UP_IRQ_FULL);

	/* Enable DN RX1/2 offset cancellation feature, if configured */
	if (mcpdm->dn_rx_offset) {
		u32 dn_offset = mcpdm->dn_rx_offset;

		omap_mcpdm_write(mcpdm, MCPDM_REG_DN_OFFSET, dn_offset);
		dn_offset |= (MCPDM_DN_OFST_RX1_EN | MCPDM_DN_OFST_RX2_EN);
		omap_mcpdm_write(mcpdm, MCPDM_REG_DN_OFFSET, dn_offset);
	}

	omap_mcpdm_write(mcpdm, MCPDM_REG_FIFO_CTRL_DN, mcpdm->dn_threshold);
	omap_mcpdm_write(mcpdm, MCPDM_REG_FIFO_CTRL_UP, mcpdm->up_threshold);

	omap_mcpdm_write(mcpdm, MCPDM_REG_DMAENABLE_SET,
			MCPDM_DMA_DN_ENABLE | MCPDM_DMA_UP_ENABLE);
}

/*
 * Cleans McPDM uplink, and downlink configuration.
 * This function should be called when the stream is closed.
 */
static void omap_mcpdm_close_streams(struct omap_mcpdm *mcpdm)
{
	/* Disable irq request generation for downlink */
	omap_mcpdm_write(mcpdm, MCPDM_REG_IRQENABLE_CLR,
			MCPDM_DN_IRQ_EMPTY | MCPDM_DN_IRQ_FULL);

	/* Disable DMA request generation for downlink */
	omap_mcpdm_write(mcpdm, MCPDM_REG_DMAENABLE_CLR, MCPDM_DMA_DN_ENABLE);

	/* Disable irq request generation for uplink */
	omap_mcpdm_write(mcpdm, MCPDM_REG_IRQENABLE_CLR,
			MCPDM_UP_IRQ_EMPTY | MCPDM_UP_IRQ_FULL);

	/* Disable DMA request generation for uplink */
	omap_mcpdm_write(mcpdm, MCPDM_REG_DMAENABLE_CLR, MCPDM_DMA_UP_ENABLE);

	/* Disable RX1/2 offset cancellation */
	if (mcpdm->dn_rx_offset)
		omap_mcpdm_write(mcpdm, MCPDM_REG_DN_OFFSET, 0);
}

static irqreturn_t omap_mcpdm_irq_handler(int irq, void *dev_id)
{
	struct omap_mcpdm *mcpdm = dev_id;
	int irq_status;

	irq_status = omap_mcpdm_read(mcpdm, MCPDM_REG_IRQSTATUS);

	/* Acknowledge irq event */
	omap_mcpdm_write(mcpdm, MCPDM_REG_IRQSTATUS, irq_status);

	if (irq_status & MCPDM_DN_IRQ_FULL)
		dev_dbg(mcpdm->dev, "DN (playback) FIFO Full\n");

	if (irq_status & MCPDM_DN_IRQ_EMPTY)
		dev_dbg(mcpdm->dev, "DN (playback) FIFO Empty\n");

	if (irq_status & MCPDM_DN_IRQ)
		dev_dbg(mcpdm->dev, "DN (playback) write request\n");

	if (irq_status & MCPDM_UP_IRQ_FULL)
		dev_dbg(mcpdm->dev, "UP (capture) FIFO Full\n");

	if (irq_status & MCPDM_UP_IRQ_EMPTY)
		dev_dbg(mcpdm->dev, "UP (capture) FIFO Empty\n");

	if (irq_status & MCPDM_UP_IRQ)
		dev_dbg(mcpdm->dev, "UP (capture) write request\n");

	return IRQ_HANDLED;
}

static int omap_mcpdm_dai_startup(struct snd_pcm_substream *substream,
				  struct snd_soc_dai *dai)
{
	struct omap_mcpdm *mcpdm = snd_soc_dai_get_drvdata(dai);
	int ret = 0;

	mutex_lock(&mcpdm->mutex);

	/* nothing to do if already active */
	if (mcpdm->active++)
		goto out;

	if (!dai->active) {
		u32 ctrl = omap_mcpdm_read(mcpdm, MCPDM_REG_CTRL);

		omap_mcpdm_write(mcpdm, MCPDM_REG_CTRL, ctrl | MCPDM_WD_EN);

		mcpdm->active_dai = dai->id;

		/* McPDM FIFO configuration */
		mcpdm->dn_threshold = 2;
		if (dai->id == OMAP_MCPDM_LEGACY_DAI)
			mcpdm->up_threshold = MCPDM_UP_THRES_MAX - 3;
		else
			mcpdm->up_threshold = 2;
	} else if (mcpdm->active_dai != dai->id) {
		dev_err(mcpdm->dev, "Trying %s, while McPDM is in %s.\n",
			dai->id ? "ABE mode" : "Legacy mode",
			mcpdm->active_dai ? "ABE mode" : "Legacy mode");
		ret = -EINVAL;
	}

out:
	mutex_unlock(&mcpdm->mutex);

	snd_soc_dai_set_dma_data(dai, substream,
				 &omap_mcpdm_dai_dma_params[substream->stream]);

	return ret;
}

static void omap_mcpdm_dai_shutdown(struct snd_pcm_substream *substream,
				  struct snd_soc_dai *dai)
{
	struct omap_mcpdm *mcpdm = snd_soc_dai_get_drvdata(dai);
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_platform *platform = rtd->platform;

	mutex_lock(&mcpdm->mutex);

	if (--mcpdm->active)
		goto out;

	if (!dai->active) {
		if (omap_mcpdm_active(mcpdm)) {
			if (dai->id == OMAP_MCPDM_LEGACY_DAI) {
				omap_mcpdm_stop(mcpdm);
				omap_mcpdm_close_streams(mcpdm);
			} else {
				omap_abe_port_disable(mcpdm->aess,
						      mcpdm->dl_port);
				omap_abe_port_disable(mcpdm->aess,
						      mcpdm->ul_port);
				usleep_range(250, 300);
				omap_mcpdm_stop(mcpdm);
				omap_mcpdm_close_streams(mcpdm);
				omap_abe_pm_shutdown(platform);
				omap_abe_pm_put(platform);
			}
		}
	}

out:
	mutex_unlock(&mcpdm->mutex);
}

static int omap_mcpdm_dai_hw_params(struct snd_pcm_substream *substream,
				    struct snd_pcm_hw_params *params,
				    struct snd_soc_dai *dai)
{
	struct omap_mcpdm *mcpdm = snd_soc_dai_get_drvdata(dai);
	int stream = substream->stream;
	struct omap_pcm_dma_data *dma_data;
	int channels;
	int link_mask = 0;

	dma_data = snd_soc_dai_get_dma_data(dai, substream);

	/* ABE DAIs have fixed channels */
	if (dai->id == OMAP_MCPDM_ABE_DAI) {
		mcpdm->dn_channels = MCPDM_PDM_DN_MASK | MCPDM_CMD_INT;
		mcpdm->up_channels = MCPDM_PDM_UPLINK_EN(1) |
					MCPDM_PDM_UPLINK_EN(2);
		dma_data->packet_size = 16;
		return 0;
	}

	channels = params_channels(params);
	switch (channels) {
	case 5:
		if (stream == SNDRV_PCM_STREAM_CAPTURE)
			/* up to 3 channels for capture */
			return -EINVAL;
		link_mask |= 1 << 4;
	case 4:
		if (stream == SNDRV_PCM_STREAM_CAPTURE)
			/* up to 3 channels for capture */
			return -EINVAL;
		link_mask |= 1 << 3;
	case 3:
		link_mask |= 1 << 2;
	case 2:
		link_mask |= 1 << 1;
	case 1:
		link_mask |= 1 << 0;
		break;
	default:
		/* unsupported number of channels */
		return -EINVAL;
	}

	/* Configure McPDM channels, and DMA packet size */
	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		mcpdm->dn_channels = link_mask << 3;
		dma_data->packet_size =
			(MCPDM_DN_THRES_MAX - mcpdm->dn_threshold) * channels;
	} else {
		mcpdm->up_channels = link_mask << 0;
		dma_data->packet_size = mcpdm->up_threshold * channels;
	}

	return 0;
}

static int omap_mcpdm_prepare(struct snd_pcm_substream *substream,
				  struct snd_soc_dai *dai)
{
	struct omap_mcpdm *mcpdm = snd_soc_dai_get_drvdata(dai);
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_platform *platform = rtd->platform;

	/*  */
	if (omap_mcpdm_active(mcpdm))
		return 0;

	if (dai->id == OMAP_MCPDM_ABE_DAI) {
		/* Check if ABE McPDM DL is already started */
		if ((omap_abe_port_is_enabled(mcpdm->aess, mcpdm->dl_port)) ||
			(omap_abe_port_is_enabled(mcpdm->aess, mcpdm->ul_port)))
			return 0;

		omap_abe_pm_get(platform);

		/* start ATC before McPDM IP */
		omap_abe_port_enable(mcpdm->aess, mcpdm->dl_port);
		omap_abe_port_enable(mcpdm->aess, mcpdm->ul_port);

		/* wait 250us for ABE tick */
		usleep_range(250, 300);
	}

	omap_mcpdm_open_streams(mcpdm);
	omap_mcpdm_start(mcpdm);

	omap_mcpdm_reg_dump(mcpdm);
	return 0;
}

static const struct snd_soc_dai_ops omap_mcpdm_dai_ops = {
	.startup	= omap_mcpdm_dai_startup,
	.shutdown	= omap_mcpdm_dai_shutdown,
	.hw_params	= omap_mcpdm_dai_hw_params,
	.prepare	= omap_mcpdm_prepare,
};

static int omap_mcpdm_probe(struct snd_soc_dai *dai)
{
	struct omap_mcpdm *mcpdm = snd_soc_dai_get_drvdata(dai);
	int ret;

	mcpdm->aess = omap_abe_port_mgr_get();

	mcpdm->dl_port = omap_abe_port_open(mcpdm->aess,
					    OMAP_ABE_BE_PORT_PDM_DL1);
	if (mcpdm->dl_port == NULL) {
		omap_abe_port_mgr_put(mcpdm->aess);
		return -EINVAL;
	}

	mcpdm->ul_port = omap_abe_port_open(mcpdm->aess,
					    OMAP_ABE_BE_PORT_PDM_UL1);
	if (mcpdm->ul_port == NULL) {
		omap_abe_port_close(mcpdm->aess, mcpdm->dl_port);
		omap_abe_port_mgr_put(mcpdm->aess);
		return -EINVAL;
	}

	pm_runtime_enable(mcpdm->dev);

	/* Disable lines while request is ongoing */
	pm_runtime_get_sync(mcpdm->dev);
	omap_mcpdm_write(mcpdm, MCPDM_REG_CTRL, 0x00);

	ret = devm_request_irq(mcpdm->dev, mcpdm->irq, omap_mcpdm_irq_handler,
				0, "McPDM", (void *)mcpdm);

	pm_runtime_put_sync(mcpdm->dev);

	if (ret) {
		dev_err(mcpdm->dev, "Request for IRQ failed\n");
		pm_runtime_disable(mcpdm->dev);

		omap_abe_port_close(mcpdm->aess, mcpdm->dl_port);
		omap_abe_port_close(mcpdm->aess, mcpdm->ul_port);
		omap_abe_port_mgr_put(mcpdm->aess);
	}

	/* Configure McPDM threshold values */
	mcpdm->dn_threshold = 2;
	mcpdm->up_threshold = MCPDM_UP_THRES_MAX - 3;
	return ret;
}

static int omap_mcpdm_remove(struct snd_soc_dai *dai)
{
	struct omap_mcpdm *mcpdm = snd_soc_dai_get_drvdata(dai);

	pm_runtime_disable(mcpdm->dev);

	omap_abe_port_close(mcpdm->aess, mcpdm->dl_port);
	omap_abe_port_close(mcpdm->aess, mcpdm->ul_port);
	omap_abe_port_mgr_put(mcpdm->aess);

	return 0;
}

#define OMAP_MCPDM_RATES	(SNDRV_PCM_RATE_88200 | SNDRV_PCM_RATE_96000)
#define OMAP_MCPDM_FORMATS	SNDRV_PCM_FMTBIT_S32_LE

static struct snd_soc_dai_driver omap_mcpdm_dai[] = {
{
	.name = "mcpdm-legacy",
	.id	= OMAP_MCPDM_LEGACY_DAI,
	.probe = omap_mcpdm_probe,
	.remove = omap_mcpdm_remove,
	.probe_order = SND_SOC_COMP_ORDER_LATE,
	.remove_order = SND_SOC_COMP_ORDER_EARLY,
	.playback = {
		.channels_min = 1,
		.channels_max = 5,
		.rates = OMAP_MCPDM_RATES,
		.formats = OMAP_MCPDM_FORMATS,
		.sig_bits = 24,
	},
	.capture = {
		.channels_min = 1,
		.channels_max = 3,
		.rates = OMAP_MCPDM_RATES,
		.formats = OMAP_MCPDM_FORMATS,
		.sig_bits = 24,
	},
	.ops = &omap_mcpdm_dai_ops,
},
{
	.name = "mcpdm-abe",
	.id	= OMAP_MCPDM_ABE_DAI,
	.probe_order = SND_SOC_COMP_ORDER_LATE,
	.remove_order = SND_SOC_COMP_ORDER_EARLY,
	.playback = {
		.channels_min = 1,
		.channels_max = 2,
		.rates = OMAP_MCPDM_RATES,
		.formats = OMAP_MCPDM_FORMATS,
	},
	.capture = {
		.channels_min = 1,
		.channels_max = 2,
		.rates = OMAP_MCPDM_RATES,
		.formats = OMAP_MCPDM_FORMATS,
	},
	.ops = &omap_mcpdm_dai_ops,
},
};

void omap_mcpdm_configure_dn_offsets(struct snd_soc_pcm_runtime *rtd,
				    u8 rx1, u8 rx2)
{
	struct omap_mcpdm *mcpdm = snd_soc_dai_get_drvdata(rtd->cpu_dai);

	mcpdm->dn_rx_offset = MCPDM_DNOFST_RX1(rx1) | MCPDM_DNOFST_RX2(rx2);
}
EXPORT_SYMBOL_GPL(omap_mcpdm_configure_dn_offsets);

static int asoc_mcpdm_probe(struct platform_device *pdev)
{
	struct omap_mcpdm *mcpdm;
	struct resource *res;
	int ret, nr_dai;

	mcpdm = devm_kzalloc(&pdev->dev, sizeof(struct omap_mcpdm), GFP_KERNEL);
	if (!mcpdm)
		return -ENOMEM;

	platform_set_drvdata(pdev, mcpdm);

	mutex_init(&mcpdm->mutex);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "dma");
	if (res == NULL)
		return -ENOMEM;

	omap_mcpdm_dai_dma_params[0].port_addr = res->start + MCPDM_REG_DN_DATA;
	omap_mcpdm_dai_dma_params[1].port_addr = res->start + MCPDM_REG_UP_DATA;

	res = platform_get_resource_byname(pdev, IORESOURCE_DMA, "dn_link");
	if (!res)
		return -ENODEV;

	omap_mcpdm_dai_dma_params[0].dma_req = res->start;

	res = platform_get_resource_byname(pdev, IORESOURCE_DMA, "up_link");
	if (!res)
		return -ENODEV;

	omap_mcpdm_dai_dma_params[1].dma_req = res->start;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mpu");
	if (res == NULL)
		return -ENOMEM;

	mcpdm->io_base = devm_request_and_ioremap(&pdev->dev, res);
	if (!mcpdm->io_base) {
		dev_err(&pdev->dev, "cannot remap\n");
		return -ENOMEM;
	}

	mcpdm->irq = platform_get_irq(pdev, 0);
	if (mcpdm->irq < 0)
		return mcpdm->irq;

	mcpdm->dev = &pdev->dev;

#if defined(CONFIG_SND_OMAP_SOC_ABE) ||\
	defined(CONFIG_SND_OMAP_SOC_ABE_MODULE)
	nr_dai = ARRAY_SIZE(omap_mcpdm_dai);
#else
	nr_dai = 1;
#endif
	ret = snd_soc_register_dais(&pdev->dev, omap_mcpdm_dai, nr_dai);

	return ret;
}

static int asoc_mcpdm_remove(struct platform_device *pdev)
{
	snd_soc_unregister_dai(&pdev->dev);

	return 0;
}

static const struct of_device_id omap_mcpdm_of_match[] = {
	{ .compatible = "ti,omap4-mcpdm", },
	{ }
};
MODULE_DEVICE_TABLE(of, omap_mcpdm_of_match);

static struct platform_driver asoc_mcpdm_driver = {
	.driver = {
		.name	= "omap-mcpdm",
		.owner	= THIS_MODULE,
		.of_match_table = omap_mcpdm_of_match,
	},

	.probe	= asoc_mcpdm_probe,
	.remove	= asoc_mcpdm_remove,
};

module_platform_driver(asoc_mcpdm_driver);

MODULE_ALIAS("platform:omap-mcpdm");
MODULE_AUTHOR("Misael Lopez Cruz <misael.lopez@ti.com>");
MODULE_DESCRIPTION("OMAP PDM SoC Interface");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:omap-mcpdm");
