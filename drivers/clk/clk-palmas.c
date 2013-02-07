/*
 * PALMAS resource clock module driver
 *
 * Copyright (C) 2011 Texas Instruments Inc.
 * Graeme Gregory <gg@slimlogic.co.uk>
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

#include <linux/clk.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/mfd/palmas.h>
#include <linux/clk-provider.h>
#include <linux/of_platform.h>

struct palmas_clk {
	struct palmas *palmas;
	struct device *dev;
	struct clk_hw clk32kg;
	struct clk_hw clk32kgaudio;
	int clk32kgaudio_mode_sleep;
	int clk32kg_mode_sleep;
};

static int palmas_clock_setbits(struct palmas *palmas, unsigned int reg,
		unsigned int data)
{
	unsigned int addr;
	int slave;

	slave = PALMAS_BASE_TO_SLAVE(PALMAS_RESOURCE_BASE);
	addr = PALMAS_BASE_TO_REG(PALMAS_RESOURCE_BASE, reg);

	return regmap_update_bits(palmas->regmap[slave], addr, data, data);
}

static int palmas_clock_clrbits(struct palmas *palmas, unsigned int reg,
		unsigned int data)
{
	unsigned int addr;
	int slave;

	slave = PALMAS_BASE_TO_SLAVE(PALMAS_RESOURCE_BASE);
	addr = PALMAS_BASE_TO_REG(PALMAS_RESOURCE_BASE, reg);

	return regmap_update_bits(palmas->regmap[slave], addr, data, 0);
}

static int palmas_prepare_clk32kg(struct clk_hw *hw)
{
	struct palmas_clk *palmas_clk = container_of(hw, struct palmas_clk,
			clk32kg);
	int ret;

	ret = palmas_clock_setbits(palmas_clk->palmas,
			PALMAS_CLK32KG_CTRL, PALMAS_CLK32KG_CTRL_MODE_ACTIVE);
	if (ret)
		dev_err(palmas_clk->dev, "Failed to enable clk32kg: %d\n", ret);

	return ret;
}

static void palmas_unprepare_clk32kg(struct clk_hw *hw)
{
	struct palmas_clk *palmas_clk = container_of(hw, struct palmas_clk,
			clk32kg);
	int ret;

	ret = palmas_clock_clrbits(palmas_clk->palmas,
			PALMAS_CLK32KG_CTRL, PALMAS_CLK32KG_CTRL_MODE_ACTIVE);
	if (ret)
		dev_err(palmas_clk->dev, "Failed to enable clk32kg: %d\n", ret);

	return;
}

static const struct clk_ops palmas_clk32kg_ops = {
	.prepare = palmas_prepare_clk32kg,
	.unprepare = palmas_unprepare_clk32kg,
};

static int palmas_prepare_clk32kgaudio(struct clk_hw *hw)
{
	struct palmas_clk *palmas_clk = container_of(hw, struct palmas_clk,
			clk32kgaudio);
	int ret;

	ret = palmas_clock_setbits(palmas_clk->palmas,
		PALMAS_CLK32KGAUDIO_CTRL, PALMAS_CLK32KGAUDIO_CTRL_MODE_ACTIVE);
	if (ret)
		dev_err(palmas_clk->dev,
				"Failed to enable clk32kgaudio: %d\n", ret);

	return ret;
}

static void palmas_unprepare_clk32kgaudio(struct clk_hw *hw)
{
	struct palmas_clk *palmas_clk = container_of(hw, struct palmas_clk,
			clk32kgaudio);
	int ret;

	ret = palmas_clock_clrbits(palmas_clk->palmas,
		PALMAS_CLK32KGAUDIO_CTRL, PALMAS_CLK32KGAUDIO_CTRL_MODE_ACTIVE);
	if (ret)
		dev_err(palmas_clk->dev,
				"Failed to enable clk32kgaudio: %d\n", ret);

	return;
}

static const struct clk_ops palmas_clk32kgaudio_ops = {
	.prepare = palmas_prepare_clk32kgaudio,
	.unprepare = palmas_unprepare_clk32kgaudio,
};

static int palmas_initialise_clk(struct palmas_clk *palmas_clk)
{
	int ret;

	if (palmas_clk->clk32kg_mode_sleep) {
		ret = palmas_clock_setbits(palmas_clk->palmas,
			PALMAS_CLK32KG_CTRL, PALMAS_CLK32KG_CTRL_MODE_SLEEP);
		if (ret)
			return ret;
	}

	if (palmas_clk->clk32kgaudio_mode_sleep) {
		ret = palmas_clock_setbits(palmas_clk->palmas,
			PALMAS_CLK32KGAUDIO_CTRL,
			PALMAS_CLK32KGAUDIO_CTRL_MODE_SLEEP);
		if (ret)
			return ret;
	}

	return 0;
}

static int palmas_clk_probe(struct platform_device *pdev)
{
	struct palmas *palmas = dev_get_drvdata(pdev->dev.parent);
	struct palmas_clk_platform_data *pdata = pdev->dev.platform_data;
	struct device_node *node = pdev->dev.of_node;
	struct palmas_clk *palmas_clk;
	struct clk *clk;
	struct clk_init_data init_clk32g, init_clk32gaudio;
	int ret;
	u32 prop;

	palmas_clk = devm_kzalloc(&pdev->dev, sizeof(*palmas_clk), GFP_KERNEL);
	if (!palmas_clk)
		return -ENOMEM;

	if (node && !pdata) {
		ret = of_property_read_u32(node, "ti,clk32kg_mode_sleep",
						&prop);
		if (!ret)
			palmas_clk->clk32kg_mode_sleep = prop;
		ret = of_property_read_u32(node, "ti,clk32kgaudio_mode_sleep",
						&prop);
		if (!ret)
			palmas_clk->clk32kgaudio_mode_sleep = prop;
	}

	palmas_clk->palmas = palmas;
	palmas_clk->dev = &pdev->dev;

	init_clk32g.name = "clk32kg";
	init_clk32g.ops = &palmas_clk32kg_ops;
	init_clk32g.parent_names = NULL;
	init_clk32g.num_parents = 0;
	palmas_clk->clk32kg.init = &init_clk32g;

	clk = clk_register(palmas_clk->dev, &palmas_clk->clk32kg);
	if (IS_ERR(clk)) {
		dev_dbg(&pdev->dev, "clk32kg clock register failed %ld\n",
				PTR_ERR(clk));
		ret = PTR_ERR(clk);
		goto err_clk32kg;
	}

	init_clk32gaudio.name = "clk32kgaudio";
	init_clk32gaudio.ops = &palmas_clk32kgaudio_ops;
	init_clk32gaudio.parent_names = NULL;
	init_clk32gaudio.num_parents = 0;
	palmas_clk->clk32kgaudio.init = &init_clk32gaudio;

	clk = clk_register(palmas_clk->dev, &palmas_clk->clk32kgaudio);
	if (IS_ERR(clk)) {
		dev_dbg(&pdev->dev, "clk32kgaudio clock register failed %ld\n",
		PTR_ERR(clk));
		ret = PTR_ERR(clk);
		goto err_audio;
	}

	ret = palmas_initialise_clk(palmas_clk);
	if (ret)
		goto err;

	dev_set_drvdata(&pdev->dev, palmas_clk);

	return 0;

err:
	clk_unregister(palmas_clk->clk32kgaudio.clk);
err_audio:
	clk_unregister(palmas_clk->clk32kg.clk);
err_clk32kg:

	return ret;
}

static int palmas_clk_remove(struct platform_device *pdev)
{
	struct palmas_clk *palmas_clk = dev_get_drvdata(&pdev->dev);

	clk_unregister(palmas_clk->clk32kgaudio.clk);
	clk_unregister(palmas_clk->clk32kg.clk);

	return 0;
}

static struct of_device_id palmas_clk_of_match[] = {
	{ .compatible = "ti,palmas-clk", },
	{ /* end */ }
};
MODULE_DEVICE_TABLE(of, palmas_clk_of_match);

static struct platform_driver palmas_clk_driver = {
	.probe = palmas_clk_probe,
	.remove = palmas_clk_remove,
	.driver = {
		.name = "palmas-clk",
		.of_match_table = palmas_clk_of_match,
		.owner = THIS_MODULE,
	},
};

static int __init palmas_clk_init(void)
{
	return platform_driver_register(&palmas_clk_driver);
}
module_init(palmas_clk_init);

static void __exit palmas_clk_exit(void)
{
	platform_driver_unregister(&palmas_clk_driver);
}
module_exit(palmas_clk_exit);
module_platform_driver(palmas_clk_driver);

MODULE_AUTHOR("Graeme Gregory <gg@slimlogic.co.uk>");
MODULE_DESCRIPTION("PALMAS clock driver");
MODULE_ALIAS("platform:palmas-clk");
MODULE_LICENSE("GPL");
