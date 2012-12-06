/*
 * omap-abe.c  --  OMAP ALSA SoC DAI driver using Audio Backend
 *
 * Copyright (C) 2010 Texas Instruments
 *
 * Contact: Liam Girdwood <lrg@ti.com>
 *          Misael Lopez Cruz <misael.lopez@ti.com>
 *          Sebastien Guiriec <s-guiriec@ti.com>
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

#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/pm_runtime.h>
#include <linux/firmware.h>
#include <linux/debugfs.h>
#include <linux/opp.h>
#include <linux/dma-mapping.h>
#include <linux/of_device.h>

#include <sound/soc.h>
#include <sound/soc-fw.h>
#include <plat/cpu.h>
#include "../../../arch/arm/mach-omap2/omap-pm.h"

#include "omap-abe-priv.h"

int abe_opp_stream_event(struct snd_soc_dapm_context *dapm, int event);
int abe_pm_suspend(struct snd_soc_dai *dai);
int abe_pm_resume(struct snd_soc_dai *dai);

int abe_mixer_write(struct snd_soc_platform *platform, unsigned int reg,
		unsigned int val);
unsigned int abe_mixer_read(struct snd_soc_platform *platform,
		unsigned int reg);
irqreturn_t abe_irq_handler(int irq, void *dev_id);
void abe_init_debugfs(struct omap_abe *abe);
void abe_cleanup_debugfs(struct omap_abe *abe);
int abe_opp_init_initial_opp(struct omap_abe *abe);
extern struct snd_pcm_ops omap_aess_pcm_ops;
extern struct snd_soc_dai_driver omap_abe_dai[6];

static u64 omap_abe_dmamask = DMA_BIT_MASK(32);

static const char *abe_memory_bank[5] = {
	"dmem",
	"cmem",
	"smem",
	"pmem",
	"mpu"
};

static void abe_init_gains(struct omap_aess *abe)
{
	/* Uplink gains */
	omap_aess_mute_gain(abe, OMAP_AESS_MIXAUDUL_MM_DL);
	omap_aess_mute_gain(abe, OMAP_AESS_MIXAUDUL_TONES);
	omap_aess_mute_gain(abe, OMAP_AESS_MIXAUDUL_UPLINK);
	omap_aess_mute_gain(abe, OMAP_AESS_MIXAUDUL_VX_DL);
	omap_aess_mute_gain(abe, OMAP_AESS_MIXVXREC_TONES);
	omap_aess_mute_gain(abe, OMAP_AESS_MIXVXREC_VX_DL);
	omap_aess_mute_gain(abe, OMAP_AESS_MIXVXREC_MM_DL);
	omap_aess_mute_gain(abe, OMAP_AESS_MIXVXREC_VX_UL);
	omap_aess_mute_gain(abe, OMAP_AESS_GAIN_DMIC1_LEFT);
	omap_aess_mute_gain(abe, OMAP_AESS_GAIN_DMIC1_RIGHT);
	omap_aess_mute_gain(abe, OMAP_AESS_GAIN_DMIC2_LEFT);
	omap_aess_mute_gain(abe, OMAP_AESS_GAIN_DMIC2_RIGHT);
	omap_aess_mute_gain(abe, OMAP_AESS_GAIN_DMIC3_LEFT);
	omap_aess_mute_gain(abe, OMAP_AESS_GAIN_DMIC3_RIGHT);

	omap_aess_mute_gain(abe, OMAP_AESS_GAIN_AMIC_LEFT);
	omap_aess_mute_gain(abe, OMAP_AESS_GAIN_AMIC_RIGHT);

	omap_aess_mute_gain(abe, OMAP_AESS_GAIN_BTUL_LEFT);
	omap_aess_mute_gain(abe, OMAP_AESS_GAIN_BTUL_RIGHT);

	/* Downlink gains */
	omap_aess_write_gain(abe, OMAP_AESS_GAIN_DL1_LEFT, GAIN_0dB);
	omap_aess_write_gain(abe, OMAP_AESS_GAIN_DL1_RIGHT, GAIN_0dB);
	/*SEBG: Ramp RAMP_2MS */

	omap_aess_mute_gain(abe, OMAP_AESS_GAIN_DL1_LEFT);
	omap_aess_mute_gain(abe, OMAP_AESS_GAIN_DL1_RIGHT);

	omap_aess_write_gain(abe, OMAP_AESS_GAIN_DL2_LEFT, GAIN_M7dB);
	omap_aess_write_gain(abe, OMAP_AESS_GAIN_DL2_RIGHT, GAIN_M7dB);
	/*SEBG: Ramp RAMP_2MS */

	omap_aess_mute_gain(abe, OMAP_AESS_GAIN_DL2_LEFT);
	omap_aess_mute_gain(abe, OMAP_AESS_GAIN_DL2_RIGHT);
	omap_aess_mute_gain(abe, OMAP_AESS_MIXDL1_MM_DL);
	omap_aess_mute_gain(abe, OMAP_AESS_MIXDL1_MM_UL2);
	omap_aess_mute_gain(abe, OMAP_AESS_MIXDL1_VX_DL);
	omap_aess_mute_gain(abe, OMAP_AESS_MIXDL1_TONES);
	omap_aess_mute_gain(abe, OMAP_AESS_MIXDL2_TONES);
	omap_aess_mute_gain(abe, OMAP_AESS_MIXDL2_VX_DL);
	omap_aess_mute_gain(abe, OMAP_AESS_MIXDL2_MM_DL);
	omap_aess_mute_gain(abe, OMAP_AESS_MIXDL2_MM_UL2);
	omap_aess_mute_gain(abe, OMAP_AESS_MIXECHO_DL1);
	omap_aess_mute_gain(abe, OMAP_AESS_MIXECHO_DL2);

	/* Sidetone gains */
	omap_aess_mute_gain(abe, OMAP_AESS_MIXSDT_UL);
	omap_aess_mute_gain(abe, OMAP_AESS_MIXSDT_DL);
}

static int abe_load_coeffs(struct snd_soc_platform *platform,
	struct snd_soc_fw_hdr *hdr)
{
	struct omap_abe *abe = snd_soc_platform_get_drvdata(platform);
	const struct snd_soc_file_coeff_data *cd = snd_soc_fw_get_data(hdr);
	const void *coeff_data = cd + 1;

	dev_dbg(platform->dev,"coeff %d size 0x%x with %d elems\n",
		cd->id, cd->size, cd->count);

	switch (cd->id) {
	case OMAP_AESS_CMEM_DL1_COEFS_ID:
		abe->equ.dl1.profile_size = cd->size / cd->count;
		abe->equ.dl1.num_profiles = cd->count;
		abe->equ.dl1.coeff_data = kmalloc(cd->size, GFP_KERNEL);
		if (abe->equ.dl1.coeff_data == NULL)
			return -ENOMEM;
		memcpy(abe->equ.dl1.coeff_data, coeff_data, cd->size);
		break;
	case OMAP_AESS_CMEM_DL2_L_COEFS_ID:
		abe->equ.dl2l.profile_size = cd->size / cd->count;
		abe->equ.dl2l.num_profiles = cd->count;
		abe->equ.dl2l.coeff_data = kmalloc(cd->size, GFP_KERNEL);
		if (abe->equ.dl2l.coeff_data == NULL)
			return -ENOMEM;
		memcpy(abe->equ.dl2l.coeff_data, coeff_data, cd->size);
		break;
	case OMAP_AESS_CMEM_DL2_R_COEFS_ID:
		abe->equ.dl2r.profile_size = cd->size / cd->count;
		abe->equ.dl2r.num_profiles = cd->count;
		abe->equ.dl2r.coeff_data = kmalloc(cd->size, GFP_KERNEL);
		if (abe->equ.dl2r.coeff_data == NULL)
			return -ENOMEM;
		memcpy(abe->equ.dl2r.coeff_data, coeff_data, cd->size);
		break;
	case OMAP_AESS_CMEM_SDT_COEFS_ID:
		abe->equ.sdt.profile_size = cd->size / cd->count;
		abe->equ.sdt.num_profiles = cd->count;
		abe->equ.sdt.coeff_data = kmalloc(cd->size, GFP_KERNEL);
		if (abe->equ.sdt.coeff_data == NULL)
			return -ENOMEM;
		memcpy(abe->equ.sdt.coeff_data, coeff_data, cd->size);
		break;
	case OMAP_AESS_CMEM_96_48_AMIC_COEFS_ID:
		abe->equ.amic.profile_size = cd->size / cd->count;
		abe->equ.amic.num_profiles = cd->count;
		abe->equ.amic.coeff_data = kmalloc(cd->size, GFP_KERNEL);
		if (abe->equ.amic.coeff_data == NULL)
			return -ENOMEM;
		memcpy(abe->equ.amic.coeff_data, coeff_data, cd->size);
		break;
	case OMAP_AESS_CMEM_96_48_DMIC_COEFS_ID:
		abe->equ.dmic.profile_size = cd->size / cd->count;
		abe->equ.dmic.num_profiles = cd->count;
		abe->equ.dmic.coeff_data = kmalloc(cd->size, GFP_KERNEL);
		if (abe->equ.dmic.coeff_data == NULL)
			return -ENOMEM;
		memcpy(abe->equ.dmic.coeff_data, coeff_data, cd->size);
		break;
	default:
		dev_err(platform->dev, "invalid coefficient ID %d\n", cd->id);
		return -EINVAL;
	}

	return 0;
}

static int abe_load_fw(struct snd_soc_platform *platform,
	struct snd_soc_fw_hdr *hdr)
{
	struct omap_abe *abe = snd_soc_platform_get_drvdata(platform);
	const u8 *fw_data = snd_soc_fw_get_data(hdr);

	/* get firmware and coefficients header info */
	memcpy(&abe->hdr, fw_data, sizeof(struct fw_header));
	if (hdr->size > OMAP_ABE_MAX_FW_SIZE) {
		dev_err(abe->dev, "Firmware too large at %d bytes\n",
			hdr->size);
		return -ENOMEM;
	}
	dev_info(abe->dev, "ABE firmware size %d bytes\n", hdr->size);
	dev_info(abe->dev, "ABE mem P %d C %d D %d S %d bytes\n",
		abe->hdr.pmem_size, abe->hdr.cmem_size,
		abe->hdr.dmem_size, abe->hdr.smem_size);

	dev_info(abe->dev, "ABE Firmware version %x\n", abe->hdr.version);
#if 0
	if (omap_abe_get_supported_fw_version() <= abe->hdr.firmware_version) {
		dev_err(abe->dev, "firmware version too old. Need %x have %x\n",
			omap_abe_get_supported_fw_version(),
			abe->hdr.firmware_version);
		return -EINVAL;
	}
#endif
	/* store ABE firmware for later context restore */
	abe->fw_text = kzalloc(hdr->size, GFP_KERNEL);
	if (abe->fw_text == NULL)
		return -ENOMEM;

	memcpy(abe->fw_text, fw_data, hdr->size);

	return 0;
}

static int abe_load_config(struct snd_soc_platform *platform,
	struct snd_soc_fw_hdr *hdr)
{
	struct omap_abe *abe = snd_soc_platform_get_drvdata(platform);
	const u8 *fw_data = snd_soc_fw_get_data(hdr);

	/* store ABE config for later context restore */
	abe->fw_config = kzalloc(hdr->size, GFP_KERNEL);
	if (abe->fw_config == NULL)
		return -ENOMEM;

	dev_info(abe->dev, "ABE Config size %d bytes\n", hdr->size);

	memcpy(abe->fw_config, fw_data, hdr->size);

	return 0;
}

static void abe_free_fw(struct omap_abe *abe)
{
	kfree(abe->fw_text);
	kfree(abe->fw_config);

	/* This below should be done in HAL  - oposite of init_mem()*/
	if (!abe->aess)
		return;

	if (abe->aess->fw_info) {
		kfree(abe->aess->fw_info->init_table);
		kfree(abe->aess->fw_info);
	}
}

/* callback to handle vendor data */
static int abe_vendor_load(struct snd_soc_platform *platform,
	struct snd_soc_fw_hdr *hdr)
{

	switch (hdr->type) {
	case SND_SOC_FW_VENDOR_FW:
		return abe_load_fw(platform, hdr);
	case SND_SOC_FW_VENDOR_CONFIG:
		return abe_load_config(platform, hdr);
	case SND_SOC_FW_COEFF:
		return abe_load_coeffs(platform, hdr);
	case SND_SOC_FW_VENDOR_CODEC:
	default:
		dev_err(platform->dev, "vendor type %d:%d not supported\n",
			hdr->type, hdr->vendor_type);
		return 0;
	}
	return 0;
}

static struct snd_soc_fw_platform_ops soc_fw_ops = {
	.vendor_load	= abe_vendor_load,
	.io_ops		= abe_ops,
	.io_ops_count	= 7,
};

static int abe_probe(struct snd_soc_platform *platform)
{
	struct omap_abe *abe = snd_soc_platform_get_drvdata(platform);
	int ret = 0, i;

	pm_runtime_enable(abe->dev);
	pm_runtime_irq_safe(abe->dev);

	ret = snd_soc_fw_load_platform(platform, &soc_fw_ops, abe->fw, 0);
	if (ret < 0) {
		dev_err(platform->dev, "request for ABE FW failed %d\n", ret);
		goto err_fw;
	}

	ret = request_threaded_irq(abe->irq, NULL, abe_irq_handler,
				IRQF_ONESHOT, "ABE", (void *)abe);
	if (ret) {
		dev_err(platform->dev, "request for ABE IRQ %d failed %d\n",
				abe->irq, ret);
		goto err_irq;
	}

	ret = abe_opp_init_initial_opp(abe);
	if (ret < 0)
		goto err_opp;

	/* aess_clk has to be enabled to access hal register.
	 * Disable the clk after it has been used.
	 */
	pm_runtime_get_sync(abe->dev);

	/* lrg - rework for better init flow */
	abe->aess = omap_abe_port_mgr_get();
	omap_aess_init_mem(abe->aess, abe->dev, abe->io_base, abe->fw_config);

	omap_aess_reset_hal(abe->aess);

	/* ZERO_labelID should really be 0 */
	for (i = 0; i < OMAP_ABE_ROUTES_UL + 2; i++)
		abe->mixer.route_ul[i] = abe->aess->fw_info->label_id[OMAP_AESS_BUFFER_ZERO_ID];

	omap_aess_load_fw(abe->aess, abe->fw_text);

	/* "tick" of the audio engine */
	omap_aess_write_event_generator(abe->aess, EVENT_TIMER);
	abe_init_gains(abe->aess);

	/* Stop the engine */
	omap_aess_stop_event_generator(abe->aess);
	omap_aess_disable_irq(abe->aess);

	pm_runtime_put_sync(abe->dev);
	abe_init_debugfs(abe);

	return ret;

err_opp:
	free_irq(abe->irq, (void *)abe);
err_irq:
	abe_free_fw(abe);
err_fw:
	pm_runtime_disable(abe->dev);
	return ret;
}

static int abe_remove(struct snd_soc_platform *platform)
{
	struct omap_abe *abe = snd_soc_platform_get_drvdata(platform);

	abe_cleanup_debugfs(abe);
	free_irq(abe->irq, (void *)abe);
	abe_free_fw(abe);
	pm_runtime_disable(abe->dev);

	return 0;
}

static struct snd_soc_platform_driver omap_aess_platform = {
	.ops		= &omap_aess_pcm_ops,
	.probe		= abe_probe,
	.remove		= abe_remove,
	.suspend	= abe_pm_suspend,
	.resume		= abe_pm_resume,
	.read		= abe_mixer_read,
	.write		= abe_mixer_write,
	.stream_event	= abe_opp_stream_event,
};

static void abe_fw_ready(const struct firmware *fw, void *context)
{
	struct platform_device *pdev = (struct platform_device *)context;
	struct omap_abe *abe = dev_get_drvdata(&pdev->dev);
	int err;

	abe->fw = fw;

	err = snd_soc_register_platform(&pdev->dev, &omap_aess_platform);
	if (err < 0) {
		dev_err(&pdev->dev, "failed to register ABE platform %d\n", err);
		release_firmware(fw);
		return;
	}

	err = snd_soc_register_dais(&pdev->dev, omap_abe_dai,
			ARRAY_SIZE(omap_abe_dai));
	if (err < 0) {
		dev_err(&pdev->dev, "failed to register ABE DAIs %d\n", err);
		snd_soc_unregister_platform(&pdev->dev);
		release_firmware(fw);
	}
}

static int abe_engine_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct omap_abe *abe;
	int ret = -EINVAL, i;

	abe = devm_kzalloc(&pdev->dev, sizeof(struct omap_abe), GFP_KERNEL);
	if (abe == NULL)
		return -ENOMEM;
	dev_set_drvdata(&pdev->dev, abe);

	for (i = 0; i < OMAP_ABE_IO_RESOURCES; i++) {
		res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						   abe_memory_bank[i]);
		if (res == NULL) {
			dev_err(&pdev->dev, "no resource %s\n",
				abe_memory_bank[i]);
			goto err;
		}
		abe->io_base[i] = ioremap(res->start, resource_size(res));
		if (!abe->io_base[i]) {
			ret = -ENOMEM;
			goto err;
		}
	}

	abe->irq = platform_get_irq(pdev, 0);
	if (abe->irq < 0) {
		ret = abe->irq;
		goto err;
	}

#ifdef CONFIG_PM
	abe->get_context_lost_count = omap_pm_get_dev_context_loss_count;
	abe->device_scale = NULL;
#endif
	abe->dev = &pdev->dev;
	mutex_init(&abe->mutex);
	mutex_init(&abe->opp.mutex);
	mutex_init(&abe->opp.req_mutex);
	INIT_LIST_HEAD(&abe->opp.req);

	get_device(abe->dev);
	abe->dev->dma_mask = &omap_abe_dmamask;
	abe->dev->coherent_dma_mask = omap_abe_dmamask;
	put_device(abe->dev);

	ret = request_firmware_nowait(THIS_MODULE, 1, "omap4_abe_new", abe->dev,
		GFP_KERNEL, pdev, abe_fw_ready);
	if (ret != 0) {
		dev_err(abe->dev, "Failed to load firmware %d\n", ret);
		goto err;
	}

	return ret;

err:
	for (--i; i >= 0; i--)
		iounmap(abe->io_base[i]);

	return ret;
}

static int abe_engine_remove(struct platform_device *pdev)
{
	struct omap_abe *abe = dev_get_drvdata(&pdev->dev);
	int i;

	snd_soc_unregister_dais(&pdev->dev, ARRAY_SIZE(omap_abe_dai));
	snd_soc_unregister_platform(&pdev->dev);
	release_firmware(abe->fw);
	for (i = 0; i < OMAP_ABE_IO_RESOURCES; i++)
		iounmap(abe->io_base[i]);

	return 0;
}

static const struct of_device_id omap_aess_of_match[] = {
	{ .compatible = "ti,omap4-aess", },
	{ }
};
MODULE_DEVICE_TABLE(of, omap_aess_of_match);

static struct platform_driver omap_aess_driver = {
	.driver = {
		.name = "aess",
		.owner = THIS_MODULE,
		.of_match_table = omap_aess_of_match,
	},
	.probe = abe_engine_probe,
	.remove = abe_engine_remove,
};

module_platform_driver(omap_aess_driver);

MODULE_ALIAS("platform:omap-aess");
MODULE_DESCRIPTION("ASoC OMAP4 ABE");
MODULE_AUTHOR("Liam Girdwood <lrg@ti.com>");
MODULE_LICENSE("GPL");
