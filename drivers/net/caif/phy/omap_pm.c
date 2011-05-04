/*
 * Copyright (C) ST-Ericsson AB 2010
 * Author:	Daniel Martensson / daniel.martensson@stericsson.com
 * License terms: GNU General Public License (GPL) version 2
 */
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/tty.h>
#include <linux/notifier.h>
#include <linux/suspend.h>
#ifdef CONFIG_HAS_WAKELOCK
#include <linux/wakelock.h>
#endif /* CONFIG_HAS_WAKELOCK */

#if !defined(GPIO_MODEM_AWR)
/* ZOOM2 TI Adaptor board defaults */
#define GPIO_MODEM_AWR	23
#endif

#if !defined(GPIO_MODEM_CWR)
/* ZOOM2 TI Adaptor board defaults */
#define GPIO_MODEM_CWR	156
#endif

extern void power_notify(bool is_on);
extern void cwr_notify(struct tty_struct *tty, bool asserted);

#ifdef CONFIG_HAS_WAKELOCK
static struct wake_lock awr_wakelock;
static struct wake_lock cwr_wakelock;
#endif /* CONFIG_HAS_WAKELOCK */

static struct tty_struct *pm_ext_tty;

static irqreturn_t cwr_irq(int irq, void *arg)
{
	int val;

	val = gpio_get_value(GPIO_MODEM_CWR);

#ifdef CONFIG_HAS_WAKELOCK
	if (val) {
		wake_lock(&cwr_wakelock);
	} else {
		wake_unlock(&cwr_wakelock);
	}
#endif /* CONFIG_HAS_WAKELOCK */

	cwr_notify(pm_ext_tty, val == 0 ? false : true);

	return IRQ_HANDLED;
}

/* Get power management notifications */
static int pm_notify(struct notifier_block *self, unsigned long val, void *data)
{
	switch (val) {
	case PM_HIBERNATION_PREPARE:
	case PM_SUSPEND_PREPARE:
		power_notify(false);
		break;
	case PM_POST_RESTORE:
	case PM_POST_HIBERNATION:
	case PM_POST_SUSPEND:
		power_notify(true);
		break;
	case PM_RESTORE_PREPARE:
	default:
		break;
}
	return NOTIFY_DONE;
}

int pm_ext_init(struct tty_struct *tty)
{
	int res;

	pm_ext_tty = tty;

	/*
	 * NOTE: No need to request AWR since this is done by the board
	 * file.
	 */

	/* AWR should be low initially. */
	gpio_direction_output(GPIO_MODEM_AWR, 0);

	/* Request and configure CWR. */
	res = gpio_request(GPIO_MODEM_CWR, "cwr");
	if (res < 0) {
		printk(KERN_ERR "pm_ext_init: request_gpio_cwr_failed: %d.\n", res);
		goto request_gpio_cwr_failed;
	}

	gpio_direction_input(GPIO_MODEM_CWR);

	/* Report initial state if different from low (WARN). */
	if (gpio_get_value(GPIO_MODEM_CWR)) {
		printk(KERN_WARNING "pm_ext_init: CWR already asserted.\n");
		cwr_notify(pm_ext_tty, true);
	}

	/* Trigger on both falling and rising edges on CWR. */
	res = request_irq(gpio_to_irq(GPIO_MODEM_CWR), cwr_irq,
					  IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
					  "cwr_irq", NULL);
	if (res < 0) {
		printk(KERN_ERR "pm_ext_init: request_irq_failed: %d.\n", res);
		goto request_irq_failed;
	}

	/* Make IRQ wakeup capable. */
	res = enable_irq_wake(gpio_to_irq(GPIO_MODEM_CWR));
	if (res < 0) {
		printk(KERN_ERR "pm_ext_init: enable_irq_wake_failed: %d.\n", res);
		goto enable_irq_wake_failed;
	}

#ifdef CONFIG_HAS_WAKELOCK
	wake_lock_init(&awr_wakelock, WAKE_LOCK_SUSPEND, "caif_awr");
	wake_lock_init(&cwr_wakelock, WAKE_LOCK_SUSPEND, "caif_cwr");
#endif /* CONFIG_HAS_WAKELOCK */

	pm_notifier(pm_notify, 0);

	return 0;

 enable_irq_wake_failed:
 request_irq_failed:
	gpio_free(GPIO_MODEM_CWR);
 request_gpio_cwr_failed:
	gpio_free(GPIO_MODEM_AWR);

	return -ENODEV;
}

void pm_ext_deinit(struct tty_struct *tty)
{
#ifdef CONFIG_HAS_WAKELOCK
	wake_lock_destroy(&awr_wakelock);
	wake_lock_destroy(&cwr_wakelock);
#endif /* CONFIG_HAS_WAKELOCK */

	free_irq(gpio_to_irq(GPIO_MODEM_CWR), NULL);
	gpio_free(GPIO_MODEM_CWR);
	gpio_free(GPIO_MODEM_AWR);
}

void pm_ext_prep_down(struct tty_struct *tty)
{
	gpio_set_value(GPIO_MODEM_AWR, 0);
#ifdef CONFIG_HAS_WAKELOCK
	wake_unlock(&awr_wakelock);
#endif /* CONFIG_HAS_WAKELOCK */
}

void pm_ext_down(struct tty_struct *tty)
{

}

void pm_ext_up(struct tty_struct *tty)
{
#ifdef CONFIG_HAS_WAKELOCK
	wake_lock(&awr_wakelock);
#endif /* CONFIG_HAS_WAKELOCK */

	/* Assert AWR. */
	gpio_set_value(GPIO_MODEM_AWR, 1);
}
