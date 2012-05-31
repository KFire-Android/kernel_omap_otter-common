#include <linux/platform_device.h>
#include <linux/dma-mapping.h>

#include <plat/usb.h>
#include <plat/omap_device.h>

static u64 dwc3_dmamask = DMA_BIT_MASK(32);

static struct omap_device_pm_latency omap_dwc3_latency[] = {
	{
		.deactivate_func        = omap_device_idle_hwmods,
		.activate_func          = omap_device_enable_hwmods,
		.flags                  = OMAP_DEVICE_LATENCY_AUTO_ADJUST,
	},
};

void __init usb_dwc3_init(void)
{
	struct omap_hwmod	*oh;
	struct platform_device	*pdev;
	int			bus_id = -1;
	const char		*oh_name, *name;

	oh_name = "usb_otg_ss";
	name = "omap-dwc3";

	oh = omap_hwmod_lookup(oh_name);
	if (!oh) {
		pr_err("Could not lookup hwmod for %s\n", oh_name);
		return;
	}

	pdev = omap_device_build(name, bus_id, oh,
			NULL, 0,
			omap_dwc3_latency, ARRAY_SIZE(omap_dwc3_latency),
			false);
	if (IS_ERR(pdev)) {
		pr_err("Could not build omap_device for %s %s\n",
				name, oh_name);
		return;
	}

	get_device(&pdev->dev);
	pdev->dev.dma_mask = &dwc3_dmamask;
	pdev->dev.coherent_dma_mask = dwc3_dmamask;
	put_device(&pdev->dev);
}
