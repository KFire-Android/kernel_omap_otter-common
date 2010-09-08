/*
 * omap-mcpdm.c  --  OMAP ALSA SoC DAI driver using McPDM port
 *
 * Copyright (C) 2009 Texas Instruments
 *
 * Author: Misael Lopez Cruz <x0052729@ti.com>
 * Contact: Jorge Eduardo Candelaria <x0107209@ti.com>
 *          Margarita Olaya <magi.olaya@ti.com>
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
#include <linux/wait.h>
#include <linux/interrupt.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/slab.h>
#include <linux/pm_runtime.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/initval.h>
#include <sound/soc.h>

#include <plat/control.h>
#include <plat/dma.h>
#include <plat/mcpdm.h>
#include <plat/mcbsp.h>
#include "omap-mcpdm.h"
#include "omap-pcm.h"

struct omap_mcpdm_data {
	struct omap_mcpdm_link *links;
	int active;
};

struct omap_mcpdm {
	struct device *dev;
	unsigned long phys_base;
	void __iomem *io_base;
	u8 free;
	int irq;

	spinlock_t lock;
	struct omap_mcpdm_platform_data *pdata;
	struct omap_mcpdm_link *downlink;
	struct omap_mcpdm_link *uplink;
	struct completion irq_completion;

	int dn_channels;
	int up_channels;
};

static struct omap_mcpdm_link omap_mcpdm_links[] = {
	/* downlink */
	{
		.irq_mask = MCPDM_DN_IRQ_EMPTY | MCPDM_DN_IRQ_FULL,
		.threshold = 2,
		.format = PDMOUTFORMAT_LJUST,
	},
	/* uplink */
	{
		.irq_mask = MCPDM_UP_IRQ_EMPTY | MCPDM_UP_IRQ_FULL,
		.threshold = 2,
		.format = PDMOUTFORMAT_LJUST,
	},
};

/*
 * Stream DMA parameters
 */
static struct omap_pcm_dma_data omap_mcpdm_dai_dma_params[] = {
	{
		.name = "Audio playback",
		.dma_req = OMAP44XX_DMA_MCPDM_DL,
		.data_type = OMAP_DMA_DATA_TYPE_S32,
		.sync_mode = OMAP_DMA_SYNC_PACKET,
		.packet_size = 16,
		.port_addr = OMAP44XX_MCPDM_L3_BASE + MCPDM_DN_DATA,
	},
	{
		.name = "Audio capture",
		.dma_req = OMAP44XX_DMA_MCPDM_UP,
		.data_type = OMAP_DMA_DATA_TYPE_S32,
		.sync_mode = OMAP_DMA_SYNC_PACKET,
		.packet_size = 16,
		.port_addr = OMAP44XX_MCPDM_L3_BASE + MCPDM_UP_DATA,
	},
};

static inline void omap_mcpdm_write(struct omap_mcpdm *mcpdm,
		u16 reg, u32 val)
{
	__raw_writel(val, mcpdm->io_base + reg);
}

static inline int omap_mcpdm_read(struct omap_mcpdm *mcpdm, u16 reg)
{
	return __raw_readl(mcpdm->io_base + reg);
}

#ifdef MCPDM_DEBUG
static void omap_mcpdm_reg_dump(struct omap_mcpdm *mcpdm)
{
	dev_dbg(mcpdm->dev, "***********************\n");
	dev_dbg(mcpdm->dev, "IRQSTATUS_RAW:  0x%04x\n",
			omap_mcpdm_read(mcpdm, MCPDM_IRQSTATUS_RAW));
	dev_dbg(mcpdm->dev, "IRQSTATUS:  0x%04x\n",
			omap_mcpdm_read(mcpdm, MCPDM_IRQSTATUS));
	dev_dbg(mcpdm->dev, "IRQENABLE_SET:  0x%04x\n",
			omap_mcpdm_read(mcpdm, MCPDM_IRQENABLE_SET));
	dev_dbg(mcpdm->dev, "IRQENABLE_CLR:  0x%04x\n",
			omap_mcpdm_read(mcpdm, MCPDM_IRQENABLE_CLR));
	dev_dbg(mcpdm->dev, "IRQWAKE_EN: 0x%04x\n",
			omap_mcpdm_read(mcpdm, MCPDM_IRQWAKE_EN));
	dev_dbg(mcpdm->dev, "DMAENABLE_SET: 0x%04x\n",
			omap_mcpdm_read(mcpdm, MCPDM_DMAENABLE_SET));
	dev_dbg(mcpdm->dev, "DMAENABLE_CLR:  0x%04x\n",
			omap_mcpdm_read(mcpdm, MCPDM_DMAENABLE_CLR));
	dev_dbg(mcpdm->dev, "DMAWAKEEN:  0x%04x\n",
			omap_mcpdm_read(mcpdm, MCPDM_DMAWAKEEN));
	dev_dbg(mcpdm->dev, "CTRL:  0x%04x\n",
			omap_mcpdm_read(mcpdm, MCPDM_CTRL));
	dev_dbg(mcpdm->dev, "DN_DATA:  0x%04x\n",
			omap_mcpdm_read(mcpdm, MCPDM_DN_DATA));
	dev_dbg(mcpdm->dev, "UP_DATA: 0x%04x\n",
			omap_mcpdm_read(mcpdm, MCPDM_UP_DATA));
	dev_dbg(mcpdm->dev, "FIFO_CTRL_DN: 0x%04x\n",
			omap_mcpdm_read(mcpdm, MCPDM_FIFO_CTRL_DN));
	dev_dbg(mcpdm->dev, "FIFO_CTRL_UP:  0x%04x\n",
			omap_mcpdm_read(mcpdm, MCPDM_FIFO_CTRL_UP));
	dev_dbg(mcpdm->dev, "DN_OFFSET:  0x%04x\n",
			omap_mcpdm_read(mcpdm, MCPDM_DN_OFFSET));
	dev_dbg(mcpdm->dev, "***********************\n");
}
#endif

/*
 * Takes the McPDM module in and out of reset state.
 * Uplink and downlink can be reset individually.
 */
static void omap_mcpdm_reset_capture(struct omap_mcpdm * mcpdm,
		int reset)
{
	int ctrl = omap_mcpdm_read(mcpdm, MCPDM_CTRL);

	if (reset)
		ctrl |= SW_UP_RST;
	else
		ctrl &= ~SW_UP_RST;

	omap_mcpdm_write(mcpdm, MCPDM_CTRL, ctrl);
}

static void omap_mcpdm_reset_playback(struct omap_mcpdm * mcpdm,
		int reset)
{
	int ctrl = omap_mcpdm_read(mcpdm, MCPDM_CTRL);

	if (reset)
		ctrl |= SW_DN_RST;
	else
		ctrl &= ~SW_DN_RST;

	omap_mcpdm_write(mcpdm, MCPDM_CTRL, ctrl);
}

/* HACK - FIXME:  */
static int users[2] = {0, 0};

/*
 * Enables the transfer through the PDM interface to/from the Phoenix
 * codec by enabling the corresponding UP or DN channels.
 */
void omap_mcpdm_start(struct omap_mcpdm * mcpdm, int stream)
{
	int ctrl = omap_mcpdm_read(mcpdm, MCPDM_CTRL);

	if (stream)
		ctrl |= mcpdm->up_channels;
	else
		ctrl |= mcpdm->dn_channels;

	if (users[stream]++ == 0)
		omap_mcpdm_write(mcpdm, MCPDM_CTRL, ctrl);
}

/*
 * Disables the transfer through the PDM interface to/from the Phoenix
 * codec by disabling the corresponding UP or DN channels.
 */
void omap_mcpdm_stop(struct omap_mcpdm *mcpdm, int stream)
{
	int ctrl = omap_mcpdm_read(mcpdm, MCPDM_CTRL);

	if (stream)
		ctrl &= ~mcpdm->up_channels;
	else
		ctrl &= ~mcpdm->dn_channels;

	if (--users[stream] == 0)
		omap_mcpdm_write(mcpdm, MCPDM_CTRL, ctrl);
}

/*
 * Configures McPDM uplink for audio recording.
 * This function should be called before omap_mcpdm_start.
 */
int omap_mcpdm_capture_open(struct omap_mcpdm *mcpdm,
		struct omap_mcpdm_link *uplink)
{
	int irq_mask = 0;
	int ctrl;

	/* Enable irq request generation */
	irq_mask |= uplink->irq_mask & MCPDM_UPLINK_IRQ_MASK;
	omap_mcpdm_write(mcpdm, MCPDM_IRQENABLE_SET, irq_mask);

	/* Configure uplink threshold */
	if (uplink->threshold > UP_THRES_MAX)
		uplink->threshold = UP_THRES_MAX;

	omap_mcpdm_write(mcpdm, MCPDM_FIFO_CTRL_UP, uplink->threshold);

	/* Configure DMA controller */
	omap_mcpdm_write(mcpdm, MCPDM_DMAENABLE_SET, DMA_UP_ENABLE);

	/* Set pdm out format */
	ctrl = omap_mcpdm_read(mcpdm, MCPDM_CTRL);
	ctrl &= ~PDMOUTFORMAT;
	ctrl |= uplink->format & PDMOUTFORMAT;

	/* Uplink channels */
	mcpdm->up_channels = uplink->channels & (PDM_UP_MASK | PDM_STATUS_MASK);

	omap_mcpdm_write(mcpdm, MCPDM_CTRL, ctrl);

	return 0;
}

/*
 * Configures McPDM downlink for audio playback.
 * This function should be called before omap_mcpdm_start.
 */
int omap_mcpdm_playback_open(struct omap_mcpdm *mcpdm,
		struct omap_mcpdm_link *downlink)
{
	int irq_mask = 0;
	int ctrl;

	/* Enable irq request generation */
	irq_mask |= downlink->irq_mask & MCPDM_DOWNLINK_IRQ_MASK;
	omap_mcpdm_write(mcpdm, MCPDM_IRQENABLE_SET, irq_mask);

	/* Configure uplink threshold */
	if (downlink->threshold > DN_THRES_MAX)
		downlink->threshold = DN_THRES_MAX;

	omap_mcpdm_write(mcpdm, MCPDM_FIFO_CTRL_DN, downlink->threshold);

	/* Enable DMA request generation */
	omap_mcpdm_write(mcpdm, MCPDM_DMAENABLE_SET, DMA_DN_ENABLE);

	/* Set pdm out format */
	ctrl = omap_mcpdm_read(mcpdm, MCPDM_CTRL);
	ctrl &= ~PDMOUTFORMAT;
	ctrl |= downlink->format & PDMOUTFORMAT;

	/* Downlink channels */
	mcpdm->dn_channels = (PDM_DN_MASK | PDM_CMD_MASK);

	omap_mcpdm_write(mcpdm, MCPDM_CTRL, ctrl);

	return 0;
}

/*
 * Cleans McPDM uplink configuration.
 * This function should be called when the stream is closed.
 */
int omap_mcpdm_capture_close(struct omap_mcpdm *mcpdm,
		struct omap_mcpdm_link *uplink)
{
	int irq_mask = 0;

	/* Disable irq request generation */
	irq_mask |= uplink->irq_mask & MCPDM_UPLINK_IRQ_MASK;
	omap_mcpdm_write(mcpdm, MCPDM_IRQENABLE_CLR, irq_mask);

	/* Disable DMA request generation */
	omap_mcpdm_write(mcpdm, MCPDM_DMAENABLE_CLR, DMA_UP_ENABLE);

	/* Clear Downlink channels */
	mcpdm->up_channels = 0;

	return 0;
}

/*
 * Cleans McPDM downlink configuration.
 * This function should be called when the stream is closed.
 */
int omap_mcpdm_playback_close(struct omap_mcpdm *mcpdm,
		struct omap_mcpdm_link *downlink)
{
	int irq_mask = 0;

	/* Disable irq request generation */
	irq_mask |= downlink->irq_mask & MCPDM_DOWNLINK_IRQ_MASK;
	omap_mcpdm_write(mcpdm, MCPDM_IRQENABLE_CLR, irq_mask);

	/* Disable DMA request generation */
	omap_mcpdm_write(mcpdm, MCPDM_DMAENABLE_CLR, DMA_DN_ENABLE);

	/* clear Downlink channels */
	mcpdm->dn_channels = 0;

	return 0;
}

static irqreturn_t omap_mcpdm_irq_handler(int irq, void *dev_id)
{
	struct omap_mcpdm *mcpdm = dev_id;
	int irq_status;

	irq_status = omap_mcpdm_read(mcpdm, MCPDM_IRQSTATUS);

	/* Acknowledge irq event */
	omap_mcpdm_write(mcpdm, MCPDM_IRQSTATUS, irq_status);

	if (irq & MCPDM_DN_IRQ_FULL) {
		dev_err(mcpdm->dev, "DN FIFO error %x\n", irq_status);
		omap_mcpdm_reset_playback(mcpdm, 1);
		omap_mcpdm_playback_open(mcpdm, mcpdm->downlink);
		omap_mcpdm_reset_playback(mcpdm, 0);
	}

	if (irq & MCPDM_DN_IRQ_EMPTY) {
		dev_err(mcpdm->dev, "DN FIFO error %x\n", irq_status);
		omap_mcpdm_reset_playback(mcpdm, 1);
		omap_mcpdm_playback_open(mcpdm, mcpdm->downlink);
		omap_mcpdm_reset_playback(mcpdm, 0);
	}

	if (irq & MCPDM_DN_IRQ) {
		dev_dbg(mcpdm->dev, "DN write request\n");
	}

	if (irq & MCPDM_UP_IRQ_FULL) {
		dev_err(mcpdm->dev, "UP FIFO error %x\n", irq_status);
		omap_mcpdm_reset_capture(mcpdm, 1);
		omap_mcpdm_capture_open(mcpdm, mcpdm->uplink);
		omap_mcpdm_reset_capture(mcpdm, 0);
	}

	if (irq & MCPDM_UP_IRQ_EMPTY) {
		dev_err(mcpdm->dev, "UP FIFO error %x\n", irq_status);
		omap_mcpdm_reset_capture(mcpdm, 1);
		omap_mcpdm_capture_open(mcpdm, mcpdm->uplink);
		omap_mcpdm_reset_capture(mcpdm, 0);
	}

	if (irq & MCPDM_UP_IRQ) {
		dev_dbg(mcpdm->dev, "UP write request\n");
	}

	return IRQ_HANDLED;
}

int omap_mcpdm_request(struct omap_mcpdm*mcpdm)
{
	struct platform_device *pdev;
	struct omap_mcpdm_platform_data *pdata;
	int ret;
	int ctrl;

	pdev = to_platform_device(mcpdm->dev);
	pdata = pdev->dev.platform_data;

	pm_runtime_get_sync(&pdev->dev);
#ifndef CONFIG_PM_RUNTIME
	if (pdata->device_enable)
		pdata->device_enable(pdev);
#endif

	spin_lock(&mcpdm->lock);

	if (!mcpdm->free) {
		dev_err(mcpdm->dev, "McPDM interface is in use\n");
		spin_unlock(&mcpdm->lock);
		ret = -EBUSY;
		goto err;
	}
	mcpdm->free = 0;

	spin_unlock(&mcpdm->lock);

	/* Disable lines while request is ongoing */
	omap_mcpdm_write(mcpdm, MCPDM_CTRL, 0x00);

	ret = request_irq(mcpdm->irq, omap_mcpdm_irq_handler,
				0, "McPDM", (void *)mcpdm);
	if (ret) {
		dev_err(mcpdm->dev, "Request for McPDM IRQ failed\n");
		goto err;
	}

	if (omap_rev() != OMAP4430_REV_ES1_0) {
		/* Enable McPDM watch dog for ES above ES 1.0 to avoid saturation */
		ctrl = omap_mcpdm_read(mcpdm, MCPDM_CTRL);
		ctrl |= WD_EN;
		omap_mcpdm_write(mcpdm, MCPDM_CTRL, ctrl);
	}

	return 0;

err:
	pm_runtime_put_sync(&pdev->dev);
#ifndef CONFIG_PM_RUNTIME
	if (pdata->device_idle)
		pdata->device_idle(pdev);
#endif
	return ret;
}

void omap_mcpdm_free(struct omap_mcpdm*mcpdm)
{
	struct platform_device *pdev;
	struct omap_mcpdm_platform_data *pdata;

	pdev = to_platform_device(mcpdm->dev);
	pdata = pdev->dev.platform_data;

	spin_lock(&mcpdm->lock);
	if (mcpdm->free) {
		dev_err(mcpdm->dev, "McPDM interface is already free\n");
		spin_unlock(&mcpdm->lock);
		return;
	}
	mcpdm->free = 1;
	spin_unlock(&mcpdm->lock);

	pm_runtime_put_sync(&pdev->dev);
#ifndef CONFIG_PM_RUNTIME
	if (pdata->device_idle)
		pdata->device_idle(pdev);
#endif

	free_irq(mcpdm->irq, (void *)mcpdm);
}

/* Enable/disable DC offset cancelation for the analog
 * headset path (PDM channels 1 and 2).
 */
int omap_mcpdm_set_offset(struct omap_mcpdm*mcpdm,
		int offset1, int offset2)
{
	int offset;

	if ((offset1 > DN_OFST_MAX) || (offset2 > DN_OFST_MAX))
		return -EINVAL;

	offset = (offset1 << DN_OFST_RX1) | (offset2 << DN_OFST_RX2);

	/* offset cancellation for channel 1 */
	if (offset1)
		offset |= DN_OFST_RX1_EN;
	else
		offset &= ~DN_OFST_RX1_EN;

	/* offset cancellation for channel 2 */
	if (offset2)
		offset |= DN_OFST_RX2_EN;
	else
		offset &= ~DN_OFST_RX2_EN;

	omap_mcpdm_write(mcpdm, MCPDM_DN_OFFSET, offset);

	return 0;
}

static int omap_mcpdm_dai_startup(struct snd_pcm_substream *substream,
				  struct snd_soc_dai *dai)
{
	struct omap_mcpdm*mcpdm = snd_soc_dai_get_drvdata(dai);
	int err = 0;

	if (!dai->active)
		err = omap_mcpdm_request(mcpdm);

	return err;
}

static void omap_mcpdm_dai_shutdown(struct snd_pcm_substream *substream,
				    struct snd_soc_dai *dai)
{
	struct omap_mcpdm*mcpdm = snd_soc_dai_get_drvdata(dai);

	if (!dai->active)
		omap_mcpdm_free(mcpdm);
}

static int omap_mcpdm_dai_hw_params(struct snd_pcm_substream *substream,
				    struct snd_pcm_hw_params *params,
				    struct snd_soc_dai *dai)
{
	struct omap_mcpdm *mcpdm = snd_soc_dai_get_drvdata(dai);
	int stream = substream->stream;
	int channels, err, link_mask = 0;

	snd_soc_dai_set_dma_data(dai, substream,
				 &omap_mcpdm_dai_dma_params[stream]);

	channels = params_channels(params);
	switch (channels) {
	case 4:
		if (stream == SNDRV_PCM_STREAM_CAPTURE)
			/* up to 2 channels for capture */
			return -EINVAL;
		link_mask |= 1 << 3;
	case 3:
		if (stream == SNDRV_PCM_STREAM_CAPTURE)
			/* up to 2 channels for capture */
			return -EINVAL;
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

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		mcpdm->downlink->channels = link_mask << 3;
		err = omap_mcpdm_playback_open(mcpdm, &omap_mcpdm_links[0]);
	} else {
		mcpdm->uplink->channels = link_mask << 0;
		err = omap_mcpdm_capture_open(mcpdm, &omap_mcpdm_links[1]);
	}

	return err;
}

static int omap_mcpdm_dai_hw_free(struct snd_pcm_substream *substream,
				  struct snd_soc_dai *dai)
{
	struct omap_mcpdm*mcpdm = snd_soc_dai_get_drvdata(dai);
	int err;

	if (substream->stream ==  SNDRV_PCM_STREAM_PLAYBACK)
		err = omap_mcpdm_playback_close(mcpdm, mcpdm->downlink);
	else
		err = omap_mcpdm_capture_close(mcpdm, mcpdm->uplink);

	return err;
}

static int omap_mcpdm_dai_trigger(struct snd_pcm_substream *substream,
				  int cmd, struct snd_soc_dai *dai)
{
	struct omap_mcpdm *mcpdm = snd_soc_dai_get_drvdata(dai);
	int stream = substream->stream;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		omap_mcpdm_start(mcpdm, stream);
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		break;
	case SNDRV_PCM_TRIGGER_STOP:
		omap_mcpdm_stop(mcpdm, stream);
		break;
	default:
		break;
	}
	return 0;
}

static struct snd_soc_dai_ops omap_mcpdm_dai_ops = {
	.startup	= omap_mcpdm_dai_startup,
	.shutdown	= omap_mcpdm_dai_shutdown,
	.hw_params	= omap_mcpdm_dai_hw_params,
	.hw_free	= omap_mcpdm_dai_hw_free,
	.trigger = omap_mcpdm_dai_trigger,
};

#define OMAP_MCPDM_RATES	(SNDRV_PCM_RATE_88200 | SNDRV_PCM_RATE_96000)
#define OMAP_MCPDM_FORMATS	(SNDRV_PCM_FMTBIT_S32_LE)

static struct snd_soc_dai_driver omap_mcpdm_dai = {
	.playback = {
		.channels_min = 1,
		.channels_max = 4,
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
};

static __devinit int asoc_mcpdm_probe(struct platform_device *pdev)
{
	struct omap_mcpdm *mcpdm;
	struct resource *res;
	int ret = 0;

	mcpdm = kzalloc(sizeof(struct omap_mcpdm), GFP_KERNEL);
	if (!mcpdm) {
		ret = -ENOMEM;
		goto exit;
	}
	platform_set_drvdata(pdev, mcpdm);
	mcpdm->downlink = &omap_mcpdm_links[0];
	mcpdm->uplink = &omap_mcpdm_links[1];

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		dev_err(&pdev->dev, "no resource\n");
		goto err_resource;
	}

	spin_lock_init(&mcpdm->lock);
	mcpdm->free = 1;

	mcpdm->io_base = ioremap(res->start, resource_size(res));
	if (!mcpdm->io_base) {
		ret = -ENOMEM;
		goto err_iomap;
	}

	mcpdm->irq = platform_get_irq(pdev, 0);
	if (!mcpdm->irq) {
		ret = -EINVAL;
		goto err_irq;
	}

	pm_runtime_enable(&pdev->dev);

	mcpdm->dev = &pdev->dev;

	ret = snd_soc_register_dai(&pdev->dev, &omap_mcpdm_dai);
	if (ret == 0)
		return 0;

err_irq:
	iounmap(mcpdm->io_base);
err_iomap:
	release_mem_region(res->start, resource_size(res));
err_resource:
	kfree(mcpdm);
exit:
	return ret;
}

static int __devexit asoc_mcpdm_remove(struct platform_device *pdev)
{
	struct omap_mcpdm *mcpdm = platform_get_drvdata(pdev);
	struct omap_mcpdm_platform_data *pdata;
	struct resource *res;

	pdata = pdev->dev.platform_data;

	snd_soc_unregister_dai(&pdev->dev);

	pm_runtime_put_sync(&pdev->dev);
#ifndef CONFIG_PM_RUNTIME
	if (pdata->device_shutdown)
		pdata->device_shutdown(pdev);
#endif
	iounmap(mcpdm->io_base);
	free_irq(mcpdm->irq, (void *)mcpdm);
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	release_mem_region(res->start, resource_size(res));
	kfree(mcpdm);
	return 0;
}

static struct platform_driver asoc_mcpdm_driver = {
	.driver = {
			.name = "omap-mcpdm-dai",
			.owner = THIS_MODULE,
	},

	.probe = asoc_mcpdm_probe,
	.remove = __devexit_p(asoc_mcpdm_remove),
};

static int __init snd_omap_mcpdm_init(void)
{
	return platform_driver_register(&asoc_mcpdm_driver);
}
module_init(snd_omap_mcpdm_init);

static void __exit snd_omap_mcpdm_exit(void)
{
	platform_driver_unregister(&asoc_mcpdm_driver);
}
module_exit(snd_omap_mcpdm_exit);

MODULE_AUTHOR("Misael Lopez Cruz <x0052729@ti.com>");
MODULE_DESCRIPTION("OMAP PDM SoC Interface");
MODULE_LICENSE("GPL");
