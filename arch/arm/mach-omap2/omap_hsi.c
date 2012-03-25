/*
 * arch/arm/mach-omap2/hsi.c
 *
 * HSI device definition
 *
 * Copyright (C) 2011 Texas Instruments, Inc.
 * Original Author: Sebastien JAN <s-jan@ti.com>
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/jiffies.h>
#include <linux/notifier.h>
#include <linux/hsi_driver_if.h>

#include <plat/omap_hsi.h>
#include <plat/omap_hwmod.h>
#include <plat/omap_device.h>

#include <../drivers/omap_hsi/hsi_driver.h>
#include "clock.h"
#include "mux.h"
#include "control.h"
#include "pm.h"
#include "dvfs.h"

#define OMAP_HSI_PLATFORM_DEVICE_DRIVER_NAME	"omap_hsi"
#define OMAP_HSI_PLATFORM_DEVICE_NAME		"omap_hsi.0"
#define OMAP_HSI_HWMOD_NAME			"hsi"
#define OMAP_HSI_HWMOD_CLASSNAME		"hsi"

/* MUX settings for HSI port 1 pins */
static struct omap_device_pad hsi_port1_pads[] __initdata = {
	{
		.name = "usbb1_ulpitll_clk.hsi1_cawake",
		.flags  = OMAP_DEVICE_PAD_REMUX | OMAP_DEVICE_PAD_WAKEUP,
		.enable = (OMAP_MUX_MODE1 | OMAP_PIN_INPUT_PULLDOWN) &
			  ~OMAP_WAKEUP_EN,
		.idle   = OMAP_MUX_MODE1 | OMAP_PIN_INPUT_PULLDOWN |
			  OMAP_WAKEUP_EN,
		.off    = OMAP_MUX_MODE1 | OMAP_PIN_OFF_NONE |
			  OMAP_WAKEUP_EN,
	},
};

/*
 * NOTE: We abuse a little bit the struct port_ctx to use it also for
 * initialization.
 */
static struct hsi_port_ctx omap_hsi_port_ctx[] = {
	[0] = {
	       .port_number = 1,
	       .hst.mode = HSI_MODE_FRAME,
	       .hst.flow = HSI_FLOW_SYNCHRONIZED,
	       .hst.frame_size = HSI_FRAMESIZE_DEFAULT,
	       .hst.divisor = HSI_DIVISOR_DEFAULT,
	       .hst.channels = HSI_CHANNELS_DEFAULT,
	       .hst.arb_mode = HSI_ARBMODE_ROUNDROBIN,
	       .hsr.mode = HSI_MODE_FRAME,
	       .hsr.flow = HSI_FLOW_SYNCHRONIZED,
	       .hsr.frame_size = HSI_FRAMESIZE_DEFAULT,
	       .hsr.channels = HSI_CHANNELS_DEFAULT,
	       .hsr.divisor = HSI_DIVISOR_DEFAULT,
	       .hsr.counters = HSI_COUNTERS_FT_DEFAULT |
			       HSI_COUNTERS_TB_DEFAULT |
			       HSI_COUNTERS_FB_DEFAULT,
	       },
};

static struct hsi_ctrl_ctx omap_hsi_ctrl_ctx = {
		.sysconfig = 0,
		.gdd_gcr = 0,
		.dll = 0,
		.pctx = omap_hsi_port_ctx,
};

static struct hsi_platform_data omap_hsi_platform_data = {
	.num_ports = ARRAY_SIZE(omap_hsi_port_ctx),
	.hsi_gdd_chan_count = HSI_HSI_DMA_CHANNEL_MAX,
	.default_hsi_fclk = HSI_DEFAULT_FCLK,
	.fifo_mapping_strategy = HSI_FIFO_MAPPING_ALL_PORT1,
	.ctx = &omap_hsi_ctrl_ctx,
	.device_enable = omap_device_enable,
	.device_idle = omap_device_idle,
	.device_shutdown = omap_device_shutdown,
	.device_scale = omap_device_scale,
};

static struct omap_device *hsi_od;


static bool omap_hsi_registration_allowed;

void omap_hsi_allow_registration(void)
{
	omap_hsi_registration_allowed = true;
}

static u32 omap_hsi_configure_errata(void)
{
	u32 errata;

	if (cpu_is_omap44xx())
		SET_HSI_ERRATA(errata, HSI_ERRATUM_i696_SW_RESET_FSM_STUCK);

	if (cpu_is_omap44xx())
		SET_HSI_ERRATA(errata, HSI_ERRATUM_ixxx_3WIRES_NO_SWAKEUP);

	if (cpu_is_omap44xx())
		SET_HSI_ERRATA(errata, HSI_ERRATUM_i702_PM_HSI_SWAKEUP);

	return errata;
}

static struct platform_device *hsi_get_hsi_platform_device(void)
{
	struct device *dev;
	struct platform_device *pdev;

	/* HSI_TODO: handle platform device id (or port) (0/1) */
	dev = bus_find_device_by_name(&platform_bus_type, NULL,
					OMAP_HSI_PLATFORM_DEVICE_NAME);
	if (!dev) {
		pr_debug("Could not find platform device %s\n",
		       OMAP_HSI_PLATFORM_DEVICE_NAME);
		return 0;
	}

	if (!dev->driver) {
		/* Could not find driver for platform device. */
		return 0;
	}

	pdev = to_platform_device(dev);

	return pdev;
}

static struct hsi_dev *hsi_get_hsi_controller_data(struct platform_device *pd)
{
	struct hsi_dev *hsi_ctrl;

	if (!pd)
		return 0;

	hsi_ctrl = (struct hsi_dev *) platform_get_drvdata(pd);
	if (!hsi_ctrl) {
		pr_err("Could not find HSI controller data\n");
		return 0;
	}

	return hsi_ctrl;
}

/**
* omap_hsi_is_io_wakeup_from_hsi - Indicates an IO wakeup from HSI CAWAKE
*
* @hsi_port - returns port number which triggered wakeup. Range [1, 2].
*	      Only valid if return value is 1 (HSI wakeup detected)
*
* Return value :* false if CAWAKE Padconf has not been found or no IOWAKEUP
*		event occured for CAWAKE.
*		* true if HSI wakeup detected on port *hsi_port
*/
bool omap_hsi_is_io_wakeup_from_hsi(int *hsi_port)
{
	if (!hsi_od)
		return false;

	/* Check for IO pad wakeup */
	if (omap_hwmod_pad_get_wakeup_status(hsi_od->hwmods[0]) == true) {
		/* Only Port 1 is supported */
		*hsi_port = 1;

		return true;
	}

	return false;
}

/**
* omap_hsi_io_wakeup_check - Check if IO wakeup is from HSI and schedule HSI
*			     processing tasklet
*
* Return value : * 0 if HSI tasklet scheduled.
*		 * negative value else.
*/
int omap_hsi_io_wakeup_check(void)
{
	int hsi_port, ret = -1;

	/* Modem HSI wakeup */
	if (omap_hsi_is_io_wakeup_from_hsi(&hsi_port))
		ret = omap_hsi_wakeup(hsi_port);

	return ret;
}

/**
* omap_hsi_wakeup - Prepare HSI for wakeup from suspend mode (RET/OFF)
*
* @hsi_port - reference to the HSI port which triggered wakeup.
*	      Range [1, 2]
*
* Return value : * 0 if HSI tasklet scheduled.
*		 * negative value else.
*/
int omap_hsi_wakeup(int hsi_port)
{
	static struct platform_device *pdev;
	static struct hsi_dev *hsi_ctrl;
	int i;

	if (!pdev) {
		pdev = hsi_get_hsi_platform_device();
		if (!pdev)
			return -ENODEV;
	}

	if (!device_may_wakeup(&pdev->dev)) {
		dev_info(&pdev->dev, "Modem not allowed to wakeup platform\n");
		return -EPERM;
	}

	if (!hsi_ctrl) {
		hsi_ctrl = hsi_get_hsi_controller_data(pdev);
		if (!hsi_ctrl)
			return -ENODEV;
	}

	for (i = 0; i < omap_hsi_platform_data.num_ports; i++) {
		if (omap_hsi_platform_data.ctx->pctx[i].port_number == hsi_port)
			break;
	}

	if (i == omap_hsi_platform_data.num_ports)
		return -ENODEV;


	/* Check no other interrupt handler has already scheduled the tasklet */
	if (test_and_set_bit(HSI_FLAGS_TASKLET_LOCK,
			     &hsi_ctrl->hsi_port[i].flags))
		return -EBUSY;

	/* CAWAKE falling or rising edge detected */
	hsi_ctrl->hsi_port[i].cawake_off_event = true;
	tasklet_hi_schedule(&hsi_ctrl->hsi_port[i].hsi_tasklet);

	/* Disable interrupt until Bottom Half has cleared */
	/* the IRQ status register */
	disable_irq_nosync(hsi_ctrl->hsi_port[i].irq);

	return 0;
}

/* HSI_TODO : This requires some fine tuning & completion of
 * activate/deactivate latency values
 */
static struct omap_device_pm_latency omap_hsi_latency[] = {
	[0] = {
	       .deactivate_func = omap_device_idle_hwmods,
	       .activate_func = omap_device_enable_hwmods,
	       .flags = OMAP_DEVICE_LATENCY_AUTO_ADJUST,
	       },
};

/* HSI device registration */
static int __init omap_hsi_register(struct omap_hwmod *oh, void *user)
{
	struct hsi_platform_data *pdata = &omap_hsi_platform_data;

	if (!oh) {
		pr_err("Could not look up %s omap_hwmod\n",
		       OMAP_HSI_HWMOD_NAME);
		return -EEXIST;
	}

	omap_hsi_platform_data.errata = omap_hsi_configure_errata();
	/* Handle only port1 for now */
	oh->mux = omap_hwmod_mux_init(hsi_port1_pads,
				      ARRAY_SIZE(hsi_port1_pads));

	hsi_od = omap_device_build(OMAP_HSI_PLATFORM_DEVICE_DRIVER_NAME, 0, oh,
			       pdata, sizeof(*pdata), omap_hsi_latency,
			       ARRAY_SIZE(omap_hsi_latency), false);
	WARN(IS_ERR(hsi_od), "Can't build omap_device for %s:%s.\n",
	     OMAP_HSI_PLATFORM_DEVICE_DRIVER_NAME, oh->name);

	pr_info("HSI: device registered as omap_hwmod: %s\n", oh->name);
	return 0;
}

int __init omap_hsi_dev_init(void)
{
	if (!omap_hsi_registration_allowed) {
		pr_info("HSI: skipping omap_device registration\n");
		return 0;
	}
	/* Keep this for genericity, although there is only one hwmod for HSI */
	return omap_hwmod_for_each_by_class(OMAP_HSI_HWMOD_CLASSNAME,
					    omap_hsi_register, NULL);
}
arch_initcall(omap_hsi_dev_init);

