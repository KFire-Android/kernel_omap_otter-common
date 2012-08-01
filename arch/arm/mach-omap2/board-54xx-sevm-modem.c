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
#include "mux.h"
#include "board-omap5evm.h"
#include <plat/omap_hsi.h>

#define OMAP5_GPIO_MDM_ONSWC		70 /* GPIO_MOD_ON (ONSWC) */
#define OMAP5_GPIO_MDM_PWRSTATE		68 /* GPIO_MOD_PWR_STATUS_CA */
#define MODEM_PWRSTATE_POLLING_MS	1
#define MODEM_PWRSTATE_TIMEOUT		(100 / MODEM_PWRSTATE_POLLING_MS)

static int modem_detected;

/*
 * Switch-on or switch-off the modem and return modem status
 * Pre-requisites:
 * - Modem Vbat supplied with
 * - MDM_ONSWC and MDM_PWRSTATE gpio reserved and PAD configured
 */
static int __init omap5evm_modem_switch(int new_state)
{
	int modem_pwrrst_ca;
	int i = 0;

	/* Control Modem power switch */
	gpio_direction_output(OMAP5_GPIO_MDM_ONSWC, new_state);

	/* Monitor Modem power state */
	while (i < MODEM_PWRSTATE_TIMEOUT) {
		modem_pwrrst_ca = gpio_get_value(OMAP5_GPIO_MDM_PWRSTATE);
		if (unlikely(new_state == modem_pwrrst_ca)) {
			pr_debug("Modem powered %s\n", new_state ? "on" :
								   "off");
			break;
		}
		msleep(MODEM_PWRSTATE_POLLING_MS);
		i++;
	}
	return modem_pwrrst_ca;
}

static void __init omap5evm_modem_pad_conf(void)
{
	/*
	 * HSI pad conf: hsi2_ca/ac_wake/flag/data/ready
	 * Also configure gpio_66, input from modem
	 */

	pr_info("Update PADs for modem connection\n");

	omap_mux_init_signal("hsi2_cawake", \
		OMAP_PIN_INPUT_PULLDOWN | \
		OMAP_PIN_OFF_NONE | \
		OMAP_PIN_OFF_WAKEUPENABLE);

	omap_mux_init_signal("hsi2_caflag", \
		OMAP_PIN_INPUT | \
		OMAP_PIN_OFF_NONE);

	omap_mux_init_signal("hsi2_cadata", \
		OMAP_PIN_INPUT_PULLDOWN | \
		OMAP_PIN_OFF_NONE);

	omap_mux_init_signal("hsi2_acready", \
		OMAP_PIN_OUTPUT | \
		OMAP_PIN_OFF_OUTPUT_LOW);

	omap_mux_init_signal("hsi2_acwake", \
		OMAP_PIN_OUTPUT | \
		OMAP_PIN_OFF_NONE);

	omap_mux_init_signal("hsi2_acdata", \
		OMAP_PIN_OUTPUT | \
		OMAP_PIN_OFF_NONE);

	omap_mux_init_signal("hsi2_acflag", \
		OMAP_PIN_OUTPUT | \
		OMAP_PIN_OFF_NONE);

	omap_mux_init_signal("hsi2_caready", \
		OMAP_PIN_INPUT | \
		OMAP_PIN_OFF_NONE);

	omap_mux_init_signal("gpio3_66", \
		OMAP_PIN_INPUT_PULLDOWN | \
		OMAP_PIN_OFF_NONE | \
		OMAP_PIN_OFF_WAKEUPENABLE);

	omap_mux_init_signal("uart1_rx", \
		OMAP_PIN_INPUT | \
		OMAP_PIN_OFF_NONE);

	omap_mux_init_signal("uart1_tx", \
		OMAP_PIN_OUTPUT | \
		OMAP_PIN_OFF_NONE);
}

/*
 * Modem is tentatively switched ON to detect it then switched back to OFF
 */
void __init omap5evm_modem_init(bool force_mux)
{
	int modem_enabled, status;

	/* Prepare MDM_STATE to get modem status */
	omap_mux_init_signal("gpio3_68", \
			OMAP_PIN_INPUT_PULLDOWN | \
			OMAP_PIN_OFF_NONE);

	if (unlikely(!gpio_is_valid(OMAP5_GPIO_MDM_PWRSTATE) ||
		     gpio_request(OMAP5_GPIO_MDM_PWRSTATE, "MODEM POWER STATE")
		     < 0)) {
		pr_err("%s: Cannot control gpio %d\n",
			__func__, OMAP5_GPIO_MDM_PWRSTATE);
		goto err_pwrstate;
	}
	gpio_direction_input(OMAP5_GPIO_MDM_PWRSTATE);

	if (unlikely(gpio_get_value(OMAP5_GPIO_MDM_PWRSTATE)))
		pr_warn("%s: Modem MOD_PWR_STATUS is already ON\n", __func__);

	/* Prepare MDM_ONSWC to control modem state */
	omap_mux_init_signal("gpio3_70", \
			OMAP_PIN_OUTPUT | \
			OMAP_PIN_OFF_NONE);

	/* Enable Modem ONSWC */
	if (unlikely(!gpio_is_valid(OMAP5_GPIO_MDM_ONSWC) ||
		   gpio_request(OMAP5_GPIO_MDM_ONSWC, "MODEM SWITCH ON") < 0)) {
		pr_err("Cannot control gpio %d\n", OMAP5_GPIO_MDM_ONSWC);
		goto err_onswc;
	}

	modem_detected = omap5evm_modem_switch(1);
	pr_info("Modem %sdetected\n", modem_detected ? "" : "NOT ");

	/* Disable Modem ONSWC */
	modem_enabled = omap5evm_modem_switch(0);
	if (unlikely(modem_detected && modem_enabled))
		pr_err("%s: Modem cannot be switched-off\n", __func__);

	/* Release PWRSTATE and ONSWC */
	gpio_free(OMAP5_GPIO_MDM_ONSWC);
err_onswc:
	gpio_free(OMAP5_GPIO_MDM_PWRSTATE);
err_pwrstate:
	/* Configure OMAP5 pad for HSI if modem detected */
	if (modem_detected || force_mux) {
		/* Setup HSI pad conf for blaze platform */
		omap5evm_modem_pad_conf();

		status = omap_hsi_dev_init();
		if (status < 0)
			pr_err("%s: hsi device registration failed: %d\n",
				__func__, status);
	}
	return;
}

static struct platform_device modem_dev = {
	.name			= "ste-g7400",
	.id			= -1
};

static int late_omap5evm_modem_init(void)
{
	int ret;

	if (modem_detected) {
		ret =  platform_device_register(&modem_dev);
		if (ret)
			pr_err("Error registering modem dev: %d\n", ret);
	}

	return 0;
}

late_initcall(late_omap5evm_modem_init);
