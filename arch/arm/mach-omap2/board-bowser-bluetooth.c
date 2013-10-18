/*
 * Bluetooth Broadcomm  and low power control via GPIO
 *
 *  Copyright (C) 2011 Google, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/hrtimer.h>
#include <linux/irq.h>
#include <linux/rfkill.h>
#include <linux/platform_device.h>
#include <linux/wakelock.h>
#include <linux/delay.h>
#include <asm/mach-types.h>
#include <plat/serial.h>
#include <plat/omap-serial.h>
#include <plat/board-bowser-bluetooth.h>
#include <linux/regulator/driver.h>

#include "mux.h"

#define BT_REG_GPIO 46
#define BT_RESET_GPIO 53

#define BT_WAKE_GPIO 49
#define BT_HOST_WAKE_GPIO 82

//The BT UART is set to UART2 on this platform (defined in plat/omap-serial.h)
#define UART2 0x1

static struct rfkill *bt_rfkill;
static struct regulator *clk32kaudio_reg;
static bool bt_enabled;
static bool host_wake_uart_enabled;
static bool wake_uart_enabled;

unsigned long bt_wake_level = -1;

typedef struct bcm_bt_lpm {
	int wake;
	int host_wake;

	struct hrtimer enter_lpm_timer;
	ktime_t enter_lpm_delay;

	struct uart_port *uport;

	struct wake_lock wake_lock;
	char wake_lock_name[100];
} bcm_lpm_t;

bcm_lpm_t bt_lpm;

static int bowser_bt_rfkill_set_power(void *data, bool blocked)
{
	// rfkill_ops callback. Turn transmitter on when blocked is false
	if (!blocked) {
		printk("bowser_bt_rfkill_set_power(On)\n");
		if (clk32kaudio_reg && !bt_enabled)
			regulator_enable(clk32kaudio_reg);

		gpio_set_value(BT_REG_GPIO, 1);
		gpio_set_value(BT_RESET_GPIO, 1);
	} else {
		printk("bowser_bt_rfkill_set_power(Off)\n");
		gpio_set_value(BT_RESET_GPIO, 0);
		gpio_set_value(BT_REG_GPIO, 0);
		if (clk32kaudio_reg && bt_enabled)
			regulator_disable(clk32kaudio_reg);
	}

	bt_enabled = !blocked;

	return 0;
}

static const struct rfkill_ops bowser_bt_rfkill_ops = {
	.set_block = bowser_bt_rfkill_set_power,
};

static void set_wake_locked(int wake)
{
	bt_lpm.wake = wake;

	if (!wake)
		wake_unlock(&bt_lpm.wake_lock);

	if (!wake_uart_enabled && wake)
	{
		omap_serial_ext_uart_enable(UART2);

#if 0
		//Mux back GPIO PULL UP pin in RTS pin
// !ATTENTIION! Need TI/Lab's help to resolve this
		omap_rts_mux_write(0, UART2);
#endif
	}

	if (wake_uart_enabled && !wake)
	{
#if 0
		//Mux RTS pin into GPIO PULL UP pin (this flows off the BT Controller)
// !ATTENTIION! Need TI/Lab's help to resolve this
		omap_rts_mux_write(MUX_PULL_UP, UART2);
#endif
		pr_debug( "%s wake_uart_enabled && !wake, calling omap_serial_ext_uart_disable\n",__FUNCTION__);
		omap_serial_ext_uart_disable(UART2);
	}
	wake_uart_enabled = wake;
}

static enum hrtimer_restart enter_lpm(struct hrtimer *timer) {
	unsigned long flags;

	gpio_set_value(BT_WAKE_GPIO, 0);
	bt_wake_level = 0;
	pr_debug("-- Deassert BT_WAKE --\n");

	spin_lock_irqsave(&bt_lpm.uport->lock, flags);

	if (bt_lpm.host_wake)
	{
		hrtimer_start(&bt_lpm.enter_lpm_timer, bt_lpm.enter_lpm_delay,
			HRTIMER_MODE_REL);
	}
	else
	{
		set_wake_locked(0);
	}

	spin_unlock_irqrestore(&bt_lpm.uport->lock, flags);

	return HRTIMER_NORESTART;
}

void bcm_bt_lpm_exit_lpm(struct uart_port *uport, int exit_lpm) {
	bt_lpm.uport = uport;

	hrtimer_try_to_cancel(&bt_lpm.enter_lpm_timer);

	if (exit_lpm)
	{
		gpio_set_value(BT_WAKE_GPIO, 1);
		bt_wake_level = 1;
		set_wake_locked(1);
		pr_debug("++ Assert BT_WAKE ++\n");
	}
	else
	{
		hrtimer_start(&bt_lpm.enter_lpm_timer, bt_lpm.enter_lpm_delay,
			HRTIMER_MODE_REL);
	}
}
EXPORT_SYMBOL(bcm_bt_lpm_exit_lpm);

static void update_host_wake_locked(int host_wake)
{
	if (host_wake == bt_lpm.host_wake)
		return;

	bt_lpm.host_wake = host_wake;

	if (host_wake) {
		wake_lock(&bt_lpm.wake_lock);
		if (!host_wake_uart_enabled)
		{
			omap_serial_ext_uart_enable(UART2);
#if 0
			//Mux back GPIO PULL UP pin in RTS pin
// !ATTENTIION! Need TI/Lab's help to resolve this
			omap_rts_mux_write(0, UART2);
#endif
		}
	} else {
		if (host_wake_uart_enabled)
		{
#if 0
			//Mux RTS pin into GPIO PULL UP pin (this flows off the BT Controller)
// !ATTENTIION! Need TI/Lab's help to resolve this
			omap_rts_mux_write(MUX_PULL_UP, UART2);
#endif
			omap_serial_ext_uart_disable(UART2);
		}

		// Take a timed wakelock, so that upper layers can take it.
		// The chipset deasserts the hostwake lock, when there is no
		// more data to send.
		wake_lock_timeout(&bt_lpm.wake_lock, HZ/2);
	}

	host_wake_uart_enabled = host_wake;

}

static irqreturn_t host_wake_isr(int irq, void *dev)
{
	int host_wake;
	unsigned long flags;

	host_wake = gpio_get_value(BT_HOST_WAKE_GPIO);

	irq_set_irq_type(irq, host_wake ? IRQF_TRIGGER_LOW : IRQF_TRIGGER_HIGH);

	if (!bt_lpm.uport) {
		bt_lpm.host_wake = host_wake;
		return IRQ_HANDLED;
	}

	spin_lock_irqsave(&bt_lpm.uport->lock, flags);
	update_host_wake_locked(host_wake);
	spin_unlock_irqrestore(&bt_lpm.uport->lock, flags);

	return IRQ_HANDLED;
}

static int bcm_bt_lpm_init(struct platform_device *pdev)
{
	int irq;
	int ret;

#if 0
	int rc;

	// Already done in board init
	rc = gpio_request(BT_WAKE_GPIO, "bowser_wake_gpio");
	if (unlikely(rc)) {
		return rc;
	}

	rc = gpio_request(BT_HOST_WAKE_GPIO, "bowser_host_wake_gpio");
	if (unlikely(rc)) {
		gpio_free(BT_WAKE_GPIO);
		return rc;
	}
#endif

	hrtimer_init(&bt_lpm.enter_lpm_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	bt_lpm.enter_lpm_delay = ktime_set(1, 0);  /* 1 sec */
	bt_lpm.enter_lpm_timer.function = enter_lpm;

	bt_lpm.host_wake = 0;

	irq = gpio_to_irq(BT_HOST_WAKE_GPIO);
	ret = request_irq(irq, host_wake_isr, IRQF_TRIGGER_HIGH,
		"bt host_wake", NULL);

	if (ret) {
		printk("Failed to request_irq() for BT_HOST_WAKE_GPIO\n");
#if 0
		gpio_free(BT_WAKE_GPIO);
		gpio_free(BT_HOST_WAKE_GPIO);
#endif
		return ret;
	}

	ret = irq_set_irq_wake(irq, 1);
	if (ret) {
#if 0
		gpio_free(BT_WAKE_GPIO);
		gpio_free(BT_HOST_WAKE_GPIO);
#endif
		return ret;
	}

#if 0
	gpio_direction_output(BT_WAKE_GPIO, 0);
	gpio_direction_input(BT_HOST_WAKE_GPIO);
#endif

	snprintf(bt_lpm.wake_lock_name, sizeof(bt_lpm.wake_lock_name),
			"BTLowPower");
	wake_lock_init(&bt_lpm.wake_lock, WAKE_LOCK_SUSPEND,
			 bt_lpm.wake_lock_name);
	return 0;
}

static int bowser_bluetooth_probe(struct platform_device *pdev)
{
	int rc = 0;
	int ret = 0;

#if 0
	rc = gpio_request(BT_RESET_GPIO, "bowser_nreset_gpip");
	if (unlikely(rc)) {
		return rc;
	}

	rc = gpio_request(BT_REG_GPIO, "bowser_nshutdown_gpio");
	if (unlikely(rc)) {
		gpio_free(BT_RESET_GPIO);
		return rc;
	}

	// clk32kaudio always on. No need to call regulator_get() ?
	clk32kaudio_reg = regulator_get(0, "clk32kaudio");
	if (IS_ERR(clk32kaudio_reg)) {
		pr_err("clk32kaudio reg not found!\n");
		clk32kaudio_reg = NULL;
	}

	gpio_direction_output(BT_REG_GPIO, 1);
	gpio_direction_output(BT_RESET_GPIO, 1);
#endif

	bt_rfkill = rfkill_alloc("bowser Bluetooth", &pdev->dev,
				RFKILL_TYPE_BLUETOOTH, &bowser_bt_rfkill_ops,
				NULL);

	if (unlikely(!bt_rfkill)) {
#if 0
		gpio_free(BT_RESET_GPIO);
		gpio_free(BT_REG_GPIO);
#endif
		return -ENOMEM;
	}

	rfkill_set_states(bt_rfkill, true, false);
	rc = rfkill_register(bt_rfkill);

	if (unlikely(rc)) {
		rfkill_destroy(bt_rfkill);
#if 0
		gpio_free(BT_RESET_GPIO);
		gpio_free(BT_REG_GPIO);
#endif
		return -1;
	}

	ret = bcm_bt_lpm_init(pdev);
	if (ret) {
		rfkill_unregister(bt_rfkill);
		rfkill_destroy(bt_rfkill);
#if 0
		gpio_free(BT_RESET_GPIO);
		gpio_free(BT_REG_GPIO);
#endif
	}

	return ret;
}

static int bowser_bluetooth_remove(struct platform_device *pdev)
{
	rfkill_unregister(bt_rfkill);
	rfkill_destroy(bt_rfkill);
#if 0
	gpio_free(BT_REG_GPIO);
	gpio_free(BT_RESET_GPIO);
	gpio_free(BT_WAKE_GPIO);
	gpio_free(BT_HOST_WAKE_GPIO);
	// clk32kaudio always on. No need to call regulator_put() ?
	regulator_put(clk32kaudio_reg);
#endif
	wake_lock_destroy(&bt_lpm.wake_lock);
	return 0;
}

int bowser_bluetooth_suspend(struct platform_device *pdev, pm_message_t state)
{
#if 0
	int irq = gpio_to_irq(BT_HOST_WAKE_GPIO);
#endif
	int host_wake;

#if 0
	disable_irq(irq);
#endif
	host_wake = gpio_get_value(BT_HOST_WAKE_GPIO);

	if (host_wake) {
#if 0
		enable_irq(irq);
#endif
		return -EBUSY;
	}

#if 0
	// !ATTENTIION! Need TI/Lab's help to resolve this
	omap_rts_mux_write(MUX_PULL_UP, UART2);
#endif

	return 0;
}

int bowser_bluetooth_resume(struct platform_device *pdev)
{
#if 0
	int irq = gpio_to_irq(BT_HOST_WAKE_GPIO);
	enable_irq(irq);
#endif

#if 0
	// !ATTENTIION! Need TI/Lab's help to resolve this
	omap_rts_mux_write(0, UART2);
#endif

	return 0;
}

static struct platform_driver bowser_bluetooth_platform_driver = {
	.probe = bowser_bluetooth_probe,
	.remove = bowser_bluetooth_remove,
	.suspend = bowser_bluetooth_suspend,
	.resume = bowser_bluetooth_resume,
	.driver = {
		.name = "bowser_bluetooth",
		.owner = THIS_MODULE,
	},
};

static int __init bowser_bluetooth_init(void)
{
	bt_enabled = false;
	return platform_driver_register(&bowser_bluetooth_platform_driver);
}

static void __exit bowser_bluetooth_exit(void)
{
	platform_driver_unregister(&bowser_bluetooth_platform_driver);
}


module_init(bowser_bluetooth_init);
module_exit(bowser_bluetooth_exit);

MODULE_ALIAS("platform:bowser");
MODULE_DESCRIPTION("bowser_bluetooth");
MODULE_AUTHOR("Jaikumar Ganesh <jaikumar@google.com>");
MODULE_LICENSE("GPL");


static void bowser_bt_mux_init(void)
{
	omap_mux_init_gpio(BT_HOST_WAKE_GPIO,
			OMAP_PIN_INPUT | OMAP_PIN_INPUT_PULLUP | OMAP_PIN_OFF_WAKEUPENABLE | OMAP_WAKEUP_EVENT);
	omap_mux_init_gpio(BT_RESET_GPIO, OMAP_MUX_MODE3 | OMAP_PIN_OUTPUT);
	omap_mux_init_gpio(BT_REG_GPIO, OMAP_PIN_OUTPUT);
	omap_mux_init_gpio(BT_WAKE_GPIO, OMAP_PIN_OUTPUT);
}

static struct platform_device bowser_bluetooth_device = {
	.name = "bowser_bluetooth",
	.id = -1,
};

int __init bowser_bt_init(void)
{
	int ret = 0;

	bowser_bt_mux_init();

	if (gpio_request(BT_RESET_GPIO, "bowser_bt_reset_n_gpio") ||
		gpio_direction_output(BT_RESET_GPIO, 1))
		pr_err("Error in initializing Bluetooth chip reset gpio: Unable to set direction. \n");

	// BT_RESET_GPIO is not tied to BT_REG_GPIO on Soho
	gpio_set_value(BT_RESET_GPIO, 0);

	if (gpio_request(BT_REG_GPIO, "bowser_bt_reg_gpio") ||
		gpio_direction_output(BT_REG_GPIO, 1))
		pr_err("Error in initializing Bluetooth power gpio: Unable to set direction.\n");

	gpio_set_value(BT_REG_GPIO, 0);

	// Disable Internal Pull up after the required 100msec delay after BT_REG is asserted
	msleep(120);

	if (gpio_request(BT_HOST_WAKE_GPIO, "bowser_host_wake_gpio") ||
		gpio_direction_input(BT_HOST_WAKE_GPIO))
		pr_err("Error in initializing Bluetooth host wake up gpio: Unable to set direction.\n");

	omap_mux_init_gpio(BT_HOST_WAKE_GPIO,
			OMAP_PIN_INPUT | OMAP_PIN_OFF_WAKEUPENABLE | OMAP_WAKEUP_EVENT);

	if (gpio_request(BT_WAKE_GPIO, "bowser_bt_wake_gpio") ||
		gpio_direction_output(BT_WAKE_GPIO, 0))
		pr_err("Error in initializing Bluetooth reset gpio: unable to set direction.\n");

	gpio_set_value(BT_WAKE_GPIO, 1);
	bt_wake_level = 1;

	ret = platform_device_register(&bowser_bluetooth_device);
	if (ret)
	{
		pr_err("could not register bcm2076_bluetooth_device!\n");
	}

	return ret;
}

