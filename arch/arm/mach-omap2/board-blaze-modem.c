/*
 * arch/arm/mach-omap2/board-blaze-modem.c
 *
 * Copyright (C) 2011 Texas Instruments
 *
 * Author: Patrick Combes <p-combes@ti.com>
 * Based on a patch from Cedric Baudelet
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/gpio.h>
#include <linux/delay.h>
#include "board-blaze.h"
#include "mux.h"
#include <plat/omap_hsi.h>

#if defined(CONFIG_USB_EHCI_HCD_OMAP) || defined(CONFIG_USB_OHCI_HCD_OMAP3)
#include <plat/usb.h>
#endif

#define BLAZE_MDM_ONSW 187
#define BLAZE_MDM_PWRSTATE 189
#define MODEM_PWRSTATE_POLLING_MS 10
#define MODEM_PWRSTATE_TIMEOUT (100 / MODEM_PWRSTATE_POLLING_MS)

static int modem_detected;

/*
 * Switch-on or switch-off the modem and return modem status
 * Pre-requisites:
 * - Modem Vbat supplied with
 * - BLAZE_MDM_ONSW and BLAZE_MDM_PWRSTATE gpio reserved and PAD configured
 */
static int blaze_modem_switch(int new_state)
{
	int modem_pwrrst_ca;
	int state;
	int i;

	/* Control Modem power switch */
	gpio_direction_output(BLAZE_MDM_ONSW, new_state);
	state = new_state ? 1 : 0;

	/* Monitor Modem power state */
	i = 0;
	while (i < MODEM_PWRSTATE_TIMEOUT) {
		modem_pwrrst_ca = gpio_get_value(BLAZE_MDM_PWRSTATE);
		if (unlikely(new_state == modem_pwrrst_ca)) {
			pr_info("Modem state properly updated to state %d\n",
									state);
			break;
		}
		msleep(MODEM_PWRSTATE_POLLING_MS);
		i++;
	}
	return modem_pwrrst_ca;
}

static void blaze_hsi_pad_conf(void)
{
	/*
	 * HSI pad conf: hsi1_ca/ac_wake/flag/data/ready
	 */

	/* hsi1_cawake */
	omap_mux_init_signal("usbb1_ulpitll_clk.hsi1_cawake", \
		OMAP_PIN_INPUT_PULLDOWN | \
		OMAP_PIN_OFF_NONE | \
		OMAP_PIN_OFF_WAKEUPENABLE);
	/* hsi1_caflag */
	omap_mux_init_signal("usbb1_ulpitll_dir.hsi1_caflag", \
		OMAP_PIN_INPUT | \
		OMAP_PIN_OFF_NONE);
	/* hsi1_cadata */
	omap_mux_init_signal("usbb1_ulpitll_stp.hsi1_cadata", \
		OMAP_PIN_INPUT | \
		OMAP_PIN_OFF_NONE);
	/* hsi1_acready */
	omap_mux_init_signal("usbb1_ulpitll_nxt.hsi1_acready", \
		OMAP_PIN_OUTPUT | \
		OMAP_PIN_OFF_OUTPUT_LOW);
	/* hsi1_acwake */
	omap_mux_init_signal("usbb1_ulpitll_dat0.hsi1_acwake", \
		OMAP_PIN_OUTPUT | \
		OMAP_PIN_OFF_NONE);
	/* hsi1_acdata */
	omap_mux_init_signal("usbb1_ulpitll_dat1.hsi1_acdata", \
		OMAP_PIN_OUTPUT | \
		OMAP_PIN_OFF_NONE);
	/* hsi1_acflag */
	omap_mux_init_signal("usbb1_ulpitll_dat2.hsi1_acflag", \
		OMAP_PIN_OUTPUT | \
		OMAP_PIN_OFF_NONE);
	/* hsi1_caready */
	omap_mux_init_signal("usbb1_ulpitll_dat3.hsi1_caready", \
		OMAP_PIN_INPUT_PULLDOWN | \
		OMAP_PIN_OFF_NONE);
}

static void blaze_modem_pad_conf(void)
{
	pr_info("Update PAD for modem connection\n");

	/* Configure PADconf for HSI */
	blaze_hsi_pad_conf();

	/* gpio_95 */
	omap_mux_init_signal("usbb1_ulpitll_dat7.gpio_95", \
		OMAP_PIN_INPUT_PULLDOWN | \
		OMAP_PIN_OFF_NONE | \
		OMAP_PIN_OFF_WAKEUPENABLE);

	/* Ensure UART3 Tx will not drive until Modem is up and running */
	omap_mux_init_signal("uart3_tx_irtx.uart3_tx_irtx", OMAP_MUX_MODE7);

	/*mcbsp2_fsx*/
	omap_mux_init_signal("abe_mcbsp2_fsx.abe_mcbsp2_fsx", \
						OMAP_PIN_INPUT_PULLDOWN | \
						OMAP_PIN_OFF_INPUT_PULLDOWN | \
						OMAP_MUX_MODE0);
	/*mcbsp2_clkx*/
	omap_mux_init_signal("abe_mcbcp2_clkx.abe_mcbsp2_clkx", \
						OMAP_PIN_INPUT_PULLDOWN | \
						OMAP_PIN_OFF_INPUT_PULLDOWN | \
						OMAP_MUX_MODE0);
	/*mcbsp2_dr*/
	omap_mux_init_signal("abe_mcbsp2_dr.abe_mcbsp2_dr", \
						OMAP_PIN_INPUT_PULLDOWN | \
						OMAP_PIN_OFF_OUTPUT_LOW | \
						OMAP_MUX_MODE0);
}

/*
 * Modem is tentatively switched ON to detect it then switched back to OFF
 *
 * As OMAP4 muxes HSI and USB signals, when HSI is used (for instance HSI
 * modem is plugged) HSI pad conf is configured and some USB
 * configurations are disabled.
 */
void __init blaze_modem_init(bool force_mux)
{
	int modem_enabled;

	/* Prepare MDM VBAT */
	omap_mux_init_signal("usbb2_ulpitll_clk.gpio_157", \
			OMAP_PIN_OUTPUT | \
			OMAP_PIN_OFF_NONE);

	/* Power on 3V3 Buck/Boost Power Supply for Modem */
	if (unlikely(!gpio_is_valid(BLAZE_MDM_PWR_EN_GPIO) ||
		gpio_request(BLAZE_MDM_PWR_EN_GPIO, "MODEM VBAT") < 0)) {
		pr_err("Cannot control gpio %d\n", BLAZE_MDM_PWR_EN_GPIO);
		goto err_pwr;
	}
	gpio_direction_output(BLAZE_MDM_PWR_EN_GPIO, 1);

	/* Prepare MDM_STATE to get modem status */
	omap_mux_init_signal("sys_boot5.gpio_189", \
			OMAP_PIN_INPUT_PULLDOWN | \
			OMAP_PIN_OFF_NONE);

	if (unlikely(!gpio_is_valid(BLAZE_MDM_PWRSTATE) ||
		gpio_request(BLAZE_MDM_PWRSTATE, "MODEM POWER STATE") < 0)) {
		pr_err("Cannot control gpio %d\n", BLAZE_MDM_PWRSTATE);
		goto err_pwrstate1;
	}
	gpio_direction_input(BLAZE_MDM_PWRSTATE);

	if (unlikely(gpio_get_value(BLAZE_MDM_PWRSTATE))) {
		pr_err("Invalid modem state\n");
		goto err_pwrstate2;
	}

	/* Prepare MDM_ONSW to control modem state */
	omap_mux_init_signal("sys_boot3.gpio_187", \
			OMAP_PIN_OUTPUT | \
			OMAP_PIN_OFF_NONE);

	/* Enable Modem ONSW */
	if (unlikely(!gpio_is_valid(BLAZE_MDM_ONSW) ||
			gpio_request(BLAZE_MDM_ONSW, "MODEM SWITCH ON") < 0)) {
		pr_err("Cannot control gpio %d\n", BLAZE_MDM_ONSW);
		goto err_onsw;
	}

	modem_detected = blaze_modem_switch(1);
	pr_info("Modem %sdetected\n", modem_detected ? "" : "NOT ");

	/* Disable Modem ONSW */
	modem_enabled = blaze_modem_switch(0);
	if (unlikely(modem_detected && modem_enabled))
		pr_err("Modem cannot be switched-off\n");

	/* Release PWRSTATE and ONSW */
	gpio_free(BLAZE_MDM_ONSW);
err_onsw:
err_pwrstate2:
	gpio_free(BLAZE_MDM_PWRSTATE);
err_pwrstate1:
	/* Configure omap4 pad for HSI if modem detected */
	if (modem_detected || force_mux) {
#if defined(CONFIG_USB_EHCI_HCD_OMAP) || defined(CONFIG_USB_OHCI_HCD_OMAP3)
		/* USBB1 I/O pads conflict with HSI1 port */
		usbhs_bdata.port_mode[0] = OMAP_USBHS_PORT_MODE_UNUSED;
		/* USBB2 I/O pads conflict with McBSP2 port */
		usbhs_bdata.port_mode[1] = OMAP_USBHS_PORT_MODE_UNUSED;
#endif
		/* Setup modem pad conf for blaze platform */
		blaze_modem_pad_conf();
		/* Allow HSI omap_device to be registered later */
		omap_hsi_allow_registration();

	} else {
		gpio_direction_output(BLAZE_MDM_PWR_EN_GPIO, 0);
		gpio_free(BLAZE_MDM_PWR_EN_GPIO);
	}
err_pwr:
	return;
}


#include "omap-rfkill-ste.h"

static struct omap_rfkill_ste_platform_data ste_pdata;

static struct platform_device modem_dev = {
	.name			= "ste-p5780",
	.id			= -1,
	.dev.platform_data	= &ste_pdata
};

static int late_blaze_modem_init(void)
{
	int ret;

	if (modem_detected) {
		ret =  platform_device_register(&modem_dev);
		if (ret)
			pr_err("Error registering modem dev: %d\n", ret);
	}

	return 0;
}

late_initcall(late_blaze_modem_init);
