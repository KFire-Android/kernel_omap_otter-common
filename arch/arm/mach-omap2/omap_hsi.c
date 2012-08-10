/*
 * arch/arm/mach-omap2/hsi.c
 *
 * HSI device definition
 *
 * Copyright (C) 2011 Texas Instruments, Inc.
 * Original Author: Sebastien JAN <s-jan@ti.com>
 * Original Author: Djamil ELAIDI <d-elaidi@ti.com>
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
#include <plat/omap-pm.h>
#include <plat/dvfs.h>

#include <../drivers/omap_hsi/hsi_driver.h>
#include "clock.h"
#include "mux.h"
#include "control.h"
#include "pm.h"

#define OMAP_HSI_PLATFORM_DEVICE_DRIVER_NAME	"omap_hsi"
#define OMAP_HSI_PLATFORM_DEVICE_NAME		"omap_hsi.0"
#define OMAP_HSI_HWMOD_NAME			"hsi"
#define OMAP_HSI_HWMOD_CLASSNAME		"hsi"

#define HSI_SIGNALS_PER_PORT		8 /* 4 for C=>A and 4 for A=>C */

#ifdef CONFIG_OMAP_MUX
/* OMAP44xx MUX settings for HSI port 1 & 2 */
static struct omap_device_pad __initdata
			omap44xx_hsi_ports_pads[2][HSI_SIGNALS_PER_PORT] = {
	{
	/* OMAP44xx MUX settings for HSI port 1 */
		{
			.name = "hsi1_cawake",
			.flags  = OMAP_DEVICE_PAD_REMUX |
				  OMAP_DEVICE_PAD_WAKEUP,
			.enable = (OMAP_MUX_MODE1 | OMAP_PIN_INPUT_PULLDOWN) &
				  ~OMAP_WAKEUP_EN,
			.idle   = OMAP_MUX_MODE1 | OMAP_PIN_INPUT_PULLDOWN |
				  OMAP_WAKEUP_EN,
			.off    = OMAP_MUX_MODE1 | OMAP_PIN_OFF_NONE |
				  OMAP_WAKEUP_EN,
		},

		{
			.name = "hsi1_caflag",
			.enable = OMAP_MUX_MODE1 | \
				  OMAP_PIN_INPUT | \
				  OMAP_PIN_OFF_NONE,
		},

		{
			.name = "hsi1_cadata",
			.enable = OMAP_MUX_MODE1 | \
				  OMAP_PIN_INPUT_PULLDOWN | \
				  OMAP_PIN_OFF_NONE,
		},

		{
			.name = "hsi1_acready",
			.enable = OMAP_MUX_MODE1 | \
				  OMAP_PIN_OUTPUT | \
				  OMAP_PIN_OFF_OUTPUT_LOW,
		},

		{
			.name = "hsi1_acwake",
			.enable = OMAP_MUX_MODE1 | \
				  OMAP_PIN_OUTPUT | \
				  OMAP_PIN_OFF_NONE,
		},

		{
			.name = "hsi1_acdata",
			.enable = OMAP_MUX_MODE1 | \
				  OMAP_PIN_OUTPUT | \
				  OMAP_PIN_OFF_NONE,
		},

		{
			.name = "hsi1_acflag",
			.enable = OMAP_MUX_MODE1 | \
				  OMAP_PIN_OUTPUT | \
				  OMAP_PIN_OFF_NONE,
		},

		{
			.name = "hsi1_caready",
			.enable = OMAP_MUX_MODE1 | \
				  OMAP_PIN_INPUT | \
				  OMAP_PIN_OFF_NONE,
		},
	},

	/* OMAP44xx MUX settings for HSI port 2 */
	{
		{
			.name = "hsi2_cawake",
			.flags  = OMAP_DEVICE_PAD_REMUX |
				  OMAP_DEVICE_PAD_WAKEUP,
			.enable = (OMAP_MUX_MODE4 | OMAP_PIN_INPUT_PULLDOWN) &
				  ~OMAP_WAKEUP_EN,
			.idle   = OMAP_MUX_MODE4 | OMAP_PIN_INPUT_PULLDOWN |
				  OMAP_WAKEUP_EN,
			.off    = OMAP_MUX_MODE4 | OMAP_PIN_OFF_NONE |
				  OMAP_WAKEUP_EN,
		},

		{
			.name = "hsi2_caflag",
			.enable = OMAP_MUX_MODE4 | \
				  OMAP_PIN_INPUT | \
				  OMAP_PIN_OFF_NONE,
		},

		{
			.name = "hsi2_cadata",
			.enable = OMAP_MUX_MODE4 | \
				  OMAP_PIN_INPUT_PULLDOWN | \
				  OMAP_PIN_OFF_NONE,
		},

		{
			.name = "hsi2_acready",
			.enable = OMAP_MUX_MODE4 | \
				  OMAP_PIN_OUTPUT | \
				  OMAP_PIN_OFF_OUTPUT_LOW,
		},

		{
			.name = "hsi2_acwake",
			.enable = OMAP_MUX_MODE4 | \
				  OMAP_PIN_OUTPUT | \
				  OMAP_PIN_OFF_NONE,
		},

		{
			.name = "hsi2_acdata",
			.enable = OMAP_MUX_MODE4 | \
				  OMAP_PIN_OUTPUT | \
				  OMAP_PIN_OFF_NONE,
		},

		{
			.name = "hsi2_acflag",
			.enable = OMAP_MUX_MODE4 | \
				  OMAP_PIN_OUTPUT | \
				  OMAP_PIN_OFF_NONE,
		},

		{
			.name = "hsi2_caready",
			.enable = OMAP_MUX_MODE4 | \
				  OMAP_PIN_INPUT | \
				  OMAP_PIN_OFF_NONE,
		},
	},
};

/* OMAP54xx MUX settings for HSI port 1 & 2 */
/* CAUTION: Adding OMAP_DEVICE_PAD_REMUX flags for a pad will add it in the
 * dynamic pads list and will change the index of following cawake pads, so
 * increment the second parameter of the omap_hwmod_pad_route_irq() function
 * accordingly */
static struct omap_device_pad __initdata
			omap54xx_hsi_ports_pads[2][HSI_SIGNALS_PER_PORT] = {
	{
	/* OMAP54xx MUX settings for HSI port 1 */
		{
			.name = "hsi1_cawake",
			.flags  = OMAP_DEVICE_PAD_REMUX |
				  OMAP_DEVICE_PAD_WAKEUP,
			.enable = (OMAP_MUX_MODE0 | OMAP_PIN_INPUT_PULLDOWN) &
				  ~OMAP_WAKEUP_EN,
			.idle   = OMAP_MUX_MODE0 | OMAP_PIN_INPUT_PULLDOWN |
				  OMAP_WAKEUP_EN,
			.off    = OMAP_MUX_MODE0 | OMAP_PIN_OFF_NONE |
				  OMAP_WAKEUP_EN,
		},

		{
			.name = "hsi1_caflag",
			.enable = OMAP_MUX_MODE0 | \
				  OMAP_PIN_INPUT | \
				  OMAP_PIN_OFF_NONE,
		},

		{
			.name = "hsi1_cadata",
			.enable = OMAP_MUX_MODE0 | \
				  OMAP_PIN_INPUT_PULLDOWN | \
				  OMAP_PIN_OFF_NONE,
		},

		{
			.name = "hsi1_acready",
			.enable = OMAP_MUX_MODE0 | \
				  OMAP_PIN_OUTPUT | \
				  OMAP_PIN_OFF_OUTPUT_LOW,
		},

		{
			.name = "hsi1_acwake",
			.enable = OMAP_MUX_MODE0 | \
				  OMAP_PIN_OUTPUT | \
				  OMAP_PIN_OFF_NONE,
		},

		{
			.name = "hsi1_acdata",
			.enable = OMAP_MUX_MODE0 | \
				  OMAP_PIN_OUTPUT | \
				  OMAP_PIN_OFF_NONE,
		},

		{
			.name = "hsi1_acflag",
			.enable = OMAP_MUX_MODE0 | \
				  OMAP_PIN_OUTPUT | \
				  OMAP_PIN_OFF_NONE,
		},

		{
			.name = "hsi1_caready",
			.enable = OMAP_MUX_MODE0 | \
				  OMAP_PIN_INPUT | \
				  OMAP_PIN_OFF_NONE,
		},
	},

	/* OMAP54xx MUX settings for HSI port 2 */
	{
		{
			.name = "hsi2_cawake",
			.flags  = OMAP_DEVICE_PAD_REMUX |
				  OMAP_DEVICE_PAD_WAKEUP,
			.enable = (OMAP_MUX_MODE0 | OMAP_PIN_INPUT_PULLDOWN) &
				  ~OMAP_WAKEUP_EN,
			.idle   = OMAP_MUX_MODE0 | OMAP_PIN_INPUT_PULLDOWN |
				  OMAP_WAKEUP_EN,
			.off    = OMAP_MUX_MODE0 | OMAP_PIN_OFF_NONE |
				  OMAP_WAKEUP_EN,
		},

		{
			.name = "hsi2_caflag",
			.enable = OMAP_MUX_MODE0 | \
				  OMAP_PIN_INPUT | \
				  OMAP_PIN_OFF_NONE,
		},

		{
			.name = "hsi2_cadata",
			.enable = OMAP_MUX_MODE0 | \
				  OMAP_PIN_INPUT_PULLDOWN | \
				  OMAP_PIN_OFF_NONE,
		},

		{
			.name = "hsi2_acready",
			.enable = OMAP_MUX_MODE0 | \
				  OMAP_PIN_OUTPUT | \
				  OMAP_PIN_OFF_OUTPUT_LOW,
		},

		{
			.name = "hsi2_acwake",
			.enable = OMAP_MUX_MODE0 | \
				  OMAP_PIN_OUTPUT | \
				  OMAP_PIN_OFF_NONE,
		},

		{
			.name = "hsi2_acdata",
			.enable = OMAP_MUX_MODE0 | \
				  OMAP_PIN_OUTPUT | \
				  OMAP_PIN_OFF_NONE,
		},

		{
			.name = "hsi2_acflag",
			.enable = OMAP_MUX_MODE0 | \
				  OMAP_PIN_OUTPUT | \
				  OMAP_PIN_OFF_NONE,
		},

		{
			.name = "hsi2_caready",
			.enable = OMAP_MUX_MODE0 | \
				  OMAP_PIN_INPUT | \
				  OMAP_PIN_OFF_NONE,
		},
	},
};

static struct omap_hwmod_mux_info * __init
omap_hsi_fill_default_pads(struct hsi_platform_data *pdata)
{
	struct omap_device_pad	*pads_to_mux;
	struct omap_device_pad	(*omap_pads)[HSI_SIGNALS_PER_PORT];

	if ((pdata->ctx->pctx[0].port_number > 2) ||
	    ((pdata->num_ports == 2) &&
	     (pdata->ctx->pctx[1].port_number > 2)) ||
	     (pdata->num_ports > 2)) {
		pr_err("%s: invalid port info\n", __func__);
		return NULL;
	}

	if (cpu_is_omap44xx())
		omap_pads = omap44xx_hsi_ports_pads;
	else if (cpu_is_omap54xx())
		omap_pads = omap54xx_hsi_ports_pads;
	else
		return NULL;

	if (pdata->num_ports == 1) {
		/* Mux only port corresponding to first index */
		pads_to_mux =
			&omap_pads[pdata->ctx->pctx[0].port_number - 1][0];
		pr_info("%s: Muxed hsi port %d\n", __func__,
			pdata->ctx->pctx[0].port_number);
	} else if (pdata->num_ports == 2) {
		/* Mux all ports */
		pads_to_mux = &omap_pads[0][0];
		pr_info("%s: Muxed hsi ports 1 & 2\n", __func__);
	} else {
		pr_err("%s: invalid port number %d\n", __func__,
			pdata->num_ports);
		return NULL;
	}

	return omap_hwmod_mux_init(pads_to_mux, pdata->num_ports * \
							HSI_SIGNALS_PER_PORT);
}
#else
static struct omap_hwmod_mux_info * __init
omap_hsi_fill_default_pads(struct hsi_platform_data *pdata)
{
	return NULL;
}
#endif

/*
 * NOTE: We abuse a little bit the struct port_ctx to use it also for
 * initialization.
 */
static struct hsi_port_ctx omap_hsi_port_ctx[] = {
	[0] = {
	       .port_number = 2,
	       .hst.mode = HSI_MODE_FRAME,
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
		.pctx = omap_hsi_port_ctx,
};

static struct hsi_platform_data omap_hsi_platform_data = {
	.num_ports = ARRAY_SIZE(omap_hsi_port_ctx),
	.hsi_gdd_chan_count = HSI_HSI_DMA_CHANNEL_MAX,
	.default_hsi_fclk = HSI_DEFAULT_FCLK,
	.fifo_mapping_strategy = HSI_FIFO_MAPPING_ALL_PORT2,
	.ctx = &omap_hsi_ctrl_ctx,
	.ssi_cawake_gpio = -1, /* If using SSI, define CAWAKE GPIO used here */
	.device_enable = omap_device_enable,
	.device_idle = omap_device_idle,
	.device_shutdown = omap_device_shutdown,
	.device_scale = omap_device_scale,
	.get_context_loss_count = omap_pm_get_dev_context_loss_count,

};

static u32 omap_hsi_configure_errata(void)
{
	u32 errata = 0;

	if (cpu_is_omap44xx() ||
	    (cpu_is_omap54xx() && (omap_rev() <= OMAP5430_REV_ES1_0))) {
		SET_HSI_ERRATA(errata, HSI_ERRATUM_i696_SW_RESET_FSM_STUCK);
		SET_HSI_ERRATA(errata, HSI_ERRATUM_ixxx_3WIRES_NO_SWAKEUP);
		SET_HSI_ERRATA(errata, HSI_ERRATUM_i702_PM_HSI_SWAKEUP);
	}

	return errata;
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
	struct platform_device *od;
	struct hsi_platform_data *pdata = &omap_hsi_platform_data;
	int i, port;

	if (!oh) {
		pr_err("Could not look up %s omap_hwmod\n",
		       OMAP_HSI_HWMOD_NAME);
		return -EEXIST;
	}

	omap_hsi_platform_data.errata = omap_hsi_configure_errata();

	oh->mux = omap_hsi_fill_default_pads(pdata);

	for (i = 0; i < pdata->num_ports; i++) {
		port = pdata->ctx->pctx[i].port_number - 1;
		omap_hwmod_pad_route_irq(oh, i, port);
	}

	od = omap_device_build(OMAP_HSI_PLATFORM_DEVICE_DRIVER_NAME, 0, oh,
			       pdata, sizeof(*pdata), omap_hsi_latency,
			       ARRAY_SIZE(omap_hsi_latency), false);
	WARN(IS_ERR(od), "Can't build omap_device for %s:%s.\n",
	     OMAP_HSI_PLATFORM_DEVICE_DRIVER_NAME, oh->name);

	pr_info("HSI: device registered as omap_hwmod: %s\n", oh->name);
	return 0;
}

int __init omap_hsi_dev_init(void)
{
	/* Keep this for genericity, although there is only one hwmod for HSI */
	return omap_hwmod_for_each_by_class(OMAP_HSI_HWMOD_CLASSNAME,
					    omap_hsi_register, NULL);
}
