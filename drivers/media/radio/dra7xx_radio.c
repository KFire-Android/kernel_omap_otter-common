/*
 * TI DRA7xx Radio Helper Driver
 *
 * Copyright (C) 2013-2014 Texas Instruments Incorporated - http://www.ti.com/
 * Author: Ravikumar Kattekola <rk@ti.com>
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
#include <linux/of.h>
#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/of_gpio.h>
#include <linux/of_platform.h>

struct dra7_radio_priv {
	int gpio;
};

static int dra7xx_radio_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct device_node *child = NULL;
	struct dra7_radio_priv *priv;
	int ret = 0;

	if (!np)
		return -ENODEV;

	if (of_find_property(np, "gpios", NULL)) {

		priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
		if (!priv)
			return -ENOMEM;
		dev_set_drvdata(&pdev->dev, priv);

		priv->gpio = of_get_gpio(np, 0);
		if (gpio_is_valid(priv->gpio)) {
			ret = gpio_request_one(priv->gpio, GPIOF_OUT_INIT_HIGH,
					       "xref3_clk");
		} else {
			dev_err(&pdev->dev, "failed to parse gpio\n");
			return -EINVAL;
		}
	}
	do {
		child = of_get_next_available_child(np, child);
		if (child && of_device_is_compatible(child,
				"ti,dra7xx_radio_subdev"))
			of_platform_device_create(child, NULL, &pdev->dev);
	} while (child);

	pr_info("DRA7xx Radio probe done\n");
	return ret;

}

static int dra7xx_radio_remove(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct device_node *child = NULL;
	struct dra7_radio_priv *priv;

	if (of_find_property(np, "gpios", NULL)) {
		priv = dev_get_drvdata(&pdev->dev);
		gpio_free(priv->gpio);
	}
	do {
		child = of_get_next_available_child(np, child);
		if (child && of_device_is_compatible(child,
				"ti,dra7xx_radio_subdev"))
			of_find_device_by_node(child);
	} while (child);
	return 0;
}

static const struct of_device_id dra7xx_radio_of_match[] = {
	{ .compatible = "ti,dra7xx_radio", },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, dra7xx_radio_of_match);

static struct platform_driver dra7xx_radio_driver = {
	.driver	= {
		.of_match_table = of_match_ptr(dra7xx_radio_of_match),
		.name	= "dra7xx_radio",
		.owner	= THIS_MODULE,
	},
	.probe = dra7xx_radio_probe,
	.remove = dra7xx_radio_remove,
};

module_platform_driver(dra7xx_radio_driver);

MODULE_DESCRIPTION("DRA7xx Radio Helper Driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:dra7xx_radio");
MODULE_AUTHOR("Texas Instrument Inc.");
