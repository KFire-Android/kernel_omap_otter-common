/*
 * Backlight driver for otter board, based on TI & O2Micro chips
 *
 * Copyright (C) Amazon Technologies Inc. All rights reserved.  
 * Feng Ye (fengy@lab126.com)  
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/fb.h>
#include <linux/backlight.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/o2micro_bl.h>

#include <plat/board.h>
#include <plat/dmtimer.h>

#ifdef CONFIG_FB_OMAP_BOOTLOADER_INIT
#include <linux/clk.h>
#include <plat/clock.h>
#endif /* CONFIG_FB_OMAP_BOOTLOADER_INIT */

#if 0
#include <linux/earlysuspend.h>
#endif

#define TIMERVALUE_OVERFLOW    0xFFFFFFFF
#define TIMERVALUE_RELOAD      (TIMERVALUE_OVERFLOW - ctx->timer_range) /* the reload value when timer overflows, the bigger the shorter the whole duty cycle */

DECLARE_WAIT_QUEUE_HEAD(lcd_fini_queue);

struct o2micro_backlight_ctx {
	struct omap_dm_timer *timer;
	int gpio_en_o2m;
	int gpio_vol_o2m;
	int gpio_en_lcd;
	int gpio_cabc_en;
	char *backlight_name;
	unsigned long timer_range;
	unsigned int brightness_max;
	unsigned int brightness_set; /* brightness will be set to */
	unsigned int brightness;     /* current brightness */
};

static void o2micro_backlight_set_brightness(struct o2micro_backlight_ctx *ctx)
{
#ifdef CONFIG_FB_OMAP_BOOTLOADER_INIT
	static int first_boot = 1;
	struct clk *tclk;
#endif /* CONFIG_FB_OMAP_BOOTLOADER_INIT */
	unsigned long cmp_value;

	pr_debug("brightness_set %x\n", ctx->brightness_set);

	if (ctx->brightness_set == ctx->brightness)
		return;

	/* Set the new brightness */
	if (ctx->brightness_set == ctx->brightness_max) {
		cmp_value = TIMERVALUE_OVERFLOW+1;
	}
	else {
		cmp_value = (ctx->timer_range * ctx->brightness_set)/ctx->brightness_max + TIMERVALUE_RELOAD;
	}

	pr_debug("o2micro backlight cmp val=%lx\n", cmp_value);

	if (ctx->brightness_set) {
		omap_dm_timer_enable(ctx->timer);
		omap_dm_timer_set_source(ctx->timer,  OMAP_TIMER_SRC_SYS_CLK);
		omap_dm_timer_set_prescaler(ctx->timer, 0);

		omap_dm_timer_start(ctx->timer);
		omap_dm_timer_set_match(ctx->timer, 1 , cmp_value);
		omap_dm_timer_set_load(ctx->timer, 1, TIMERVALUE_RELOAD);
		omap_dm_timer_set_pwm(ctx->timer, 0 , 1 , OMAP_TIMER_TRIGGER_OVERFLOW_AND_COMPARE);
		/* Enabling it as it's disabled after setting pwm */
		omap_dm_timer_enable(ctx->timer);
	}
	else {
#ifdef CONFIG_FB_OMAP_BOOTLOADER_INIT
		/*
		 * we have timer10_fck ENABLE_ON_INIT (enabled during boot) so
		 * it needs to be disabled manually by us
		 */
		if (unlikely(first_boot != 0)) {
			tclk = clk_get(&ctx->timer->pdev->dev, "fck");
			if (!IS_ERR(tclk)) {
				clk_disable(tclk);
				clk_put(tclk);
			}
			first_boot = 0;
		}
#endif /* CONFIG_FB_OMAP_BOOTLOADER_INIT */
		omap_dm_timer_stop(ctx->timer);
		omap_dm_timer_disable(ctx->timer);
	}

	ctx->brightness = ctx->brightness_set;
}

static void o2micro_backlight_work(struct o2micro_backlight_ctx *ctx)
{
	int power_up   = !ctx->brightness && ctx->brightness_set;
	int power_down = ctx->brightness && !ctx->brightness_set;

	if (power_up) {
		pr_info("backlight powering up\n");

		/* Wait for LCD to get turned ON first */
		if (ctx->gpio_en_lcd >= 0) {
			wait_event_timeout(lcd_fini_queue, gpio_get_value(ctx->gpio_en_lcd), msecs_to_jiffies(100));
		}
		msleep(100);

		/* Enable VIN */
		if (ctx->gpio_vol_o2m >= 0) {
			gpio_set_value(ctx->gpio_vol_o2m, 1);
		}

		/* Enable PWM */
		o2micro_backlight_set_brightness(ctx);

		/* Enable the O2M */
		if (ctx->gpio_en_o2m >= 0) {
			gpio_set_value(ctx->gpio_en_o2m, 1);
		}
	}
	else if (power_down) {
		pr_info("backlight powering down\n");

		/* Disable PWM */
		o2micro_backlight_set_brightness(ctx);

		/* Sleep for PWM to discharge data completely before disabling BL GPIO */
		msleep(150);

		/* Disable the O2M */
		if (ctx->gpio_en_o2m >= 0) {
			gpio_set_value(ctx->gpio_en_o2m, 0);
		}

		/* Disable VIN */
		if (ctx->gpio_vol_o2m >= 0) {
			gpio_set_value(ctx->gpio_vol_o2m, 0);
		}

	}
	else {
		o2micro_backlight_set_brightness(ctx);
	}

}

/* this function will be called during suspend and resume */
static int o2micro_backlight_update_status(struct backlight_device *bl)
{
	struct o2micro_backlight_ctx *ctx = bl_get_data(bl);
	int brightness = bl->props.brightness;

	if (bl->props.power == 0)
		brightness = 0;

	if (bl->props.state & (BL_CORE_SUSPENDED | BL_CORE_FBBLANK))
		brightness = 0;

	ctx->brightness_set = brightness;
	o2micro_backlight_work(ctx);

	return 0;
}

static int o2micro_backlight_get_brightness(struct backlight_device *bl)
{
	struct o2micro_backlight_ctx *ctx = bl_get_data(bl);
	return ctx->brightness;
}

static const struct backlight_ops o2micro_backlight_ops = {
	.options = BL_CORE_SUSPENDRESUME,
	.update_status	= o2micro_backlight_update_status,
	.get_brightness	= o2micro_backlight_get_brightness,
};

static int o2micro_backlight_probe(struct platform_device *pdev)
{
	struct o2micro_backlight_ctx *ctx;
	struct backlight_device *bl;
	struct backlight_properties props;
	struct o2micro_backlight_platform_data *pdata = pdev->dev.platform_data;
	int    ret = 0;


	ctx = kzalloc(sizeof(struct o2micro_backlight_ctx), GFP_KERNEL);
	if (ctx == NULL) {
		dev_err(&pdev->dev, "No memory for backlight device\n");
		return -ENOMEM;
	}

	/* timer request */
	ctx->timer = omap_dm_timer_request_specific(pdata->timer);
	if (ctx->timer == NULL) {
		kfree(ctx);
		dev_err(&pdev->dev, "failed to request pwm timer\n");
		return -ENODEV;
	}

	/* gpio request */
	ctx->gpio_en_o2m = pdata->gpio_en_o2m;
	if (pdata->gpio_en_o2m >= 0) {
		ret = gpio_request(ctx->gpio_en_o2m, "backlight_o2m_gpio");
		if (ret != 0) {  /* this GPIO is shared with HDMI so it could fail */
			printk("backlight gpio request failed\n");
			ctx->gpio_en_o2m = -EINVAL; /* serve as a flag as well */
		} else {
			gpio_direction_output(ctx->gpio_en_o2m, 1);
		}
	}

	ctx->gpio_vol_o2m = pdata->gpio_vol_o2m;
	if (pdata->gpio_vol_o2m >= 0) {
		ret = gpio_request(ctx->gpio_vol_o2m, "backlight_vol_gpio");
		if (ret != 0) {
			printk("backlight vol gpio request failed\n");
			ctx->gpio_vol_o2m = -EINVAL; /* serve as a flag as well */
		} else {
			gpio_direction_output(ctx->gpio_vol_o2m, 1);
		}
	}

	ctx->gpio_en_lcd = pdata->gpio_en_lcd;
	ctx->gpio_cabc_en = pdata->gpio_cabc_en;
	if (pdata->gpio_cabc_en >= 0) {
		ret = gpio_request(ctx->gpio_cabc_en, "backlight_cabc_gpio");
		if (ret != 0) {
			printk("backlight cabc gpio request failed\n");
			ctx->gpio_cabc_en = -EINVAL; /* serve as a flag as well */
		} else {
			gpio_direction_output(ctx->gpio_cabc_en, 1);
			gpio_set_value(ctx->gpio_cabc_en, 1); /* For CABC we enable it right away */
		}
	}

	if (pdata->name != NULL)
		ctx->backlight_name = pdata->name;
	else
		ctx->backlight_name = "backlight";

	/* timer count range value calculate, the bigger the longer the whole duty cycle */
	/* *2 because PWM high and low count as one cycle */
	ctx->timer_range = (pdata->sysclk / (pdata->pwmfreq * 2));

	/* backlight device register */
	ctx->brightness_max  = pdata->totalsteps;
	props.max_brightness = pdata->totalsteps;
	props.type = BACKLIGHT_RAW;
	bl = backlight_device_register(ctx->backlight_name, &pdev->dev, ctx, &o2micro_backlight_ops, &props);
	if (IS_ERR(bl)) {
		dev_err(&pdev->dev, "failed to register backlight device\n");
		kfree(ctx);
		return PTR_ERR(bl);
	}

	bl->props.power = 1;
	bl->props.brightness = pdata->initialstep; /* do we want to save the value user set previously? */

	platform_set_drvdata(pdev, bl);

	return 0;
}

static int o2micro_backlight_remove(struct platform_device *pdev)
{
	struct backlight_device *bl = platform_get_drvdata(pdev);
	struct o2micro_backlight_ctx *ctx = bl_get_data(bl);

	backlight_device_unregister(bl);
	kfree(ctx);
	return 0;
}

static void o2micro_backlight_shutdown(struct platform_device *pdev)
{
	struct backlight_device *bl = platform_get_drvdata(pdev);

	/* Turn OFF backlight GPIO before turning of LCD during power off (shutdown) sequence */
	bl->props.power = 0;
	o2micro_backlight_update_status(bl);
}

static struct platform_driver o2micro_backlight_driver = {
	.driver		= {
		.name	= "o2micro-bl",
		.owner	= THIS_MODULE,
	},
	.probe		= o2micro_backlight_probe,
	.remove		= o2micro_backlight_remove,
	.shutdown       = o2micro_backlight_shutdown,
};

static int __init o2micro_backlight_init(void)
{
	return platform_driver_register(&o2micro_backlight_driver);
}
module_init(o2micro_backlight_init);

static void __exit o2micro_backlight_exit(void)
{
	platform_driver_unregister(&o2micro_backlight_driver);
}
module_exit(o2micro_backlight_exit);

MODULE_DESCRIPTION("Backlight Driver for O2Micro");
MODULE_AUTHOR("Feng Ye <fengy@lab126.com");
MODULE_LICENSE("GPL");
