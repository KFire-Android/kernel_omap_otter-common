/**
 * sevm_dock_station.c - OMAP5 sEVM Dock Station
 *
 * Copyright (C) 2012 Texas Instrument Inc
 *
 * Author: Leed Aguilar <leed.aguilar@ti.com>
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License. See the file "COPYING" in the main directory of this
 * archive for more details.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/types.h>
#include <linux/switch.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/sevm_dock_station.h>

struct sevm_dockstation {
	struct sevm_dockstation_platform_data *pdata;
	struct switch_dev dock_switch;
	int gpio;
	int irq;
	int can_sleep;
};

static void report_dock_state(struct sevm_dockstation *data)
{
	int state;

	if (data->can_sleep)
		state = gpio_get_value_cansleep(data->gpio);
	else
		state = gpio_get_value(data->gpio);

	switch_set_state(&data->dock_switch, !state);

}

static irqreturn_t dock_irq(int irq, void *id)
{
	struct sevm_dockstation *data = id;

	report_dock_state(data);

	return IRQ_HANDLED;
}

static int __devinit sevm_dockstation_probe(struct platform_device *pdev)
{
	struct sevm_dockstation_platform_data *pdata;
	struct sevm_dockstation *data;
	int ret = 0;

	pdata = pdev->dev.platform_data;
	if (!pdata) {
		dev_err(&pdev->dev, "platform data not found\n");
		ret = -ENODEV;
		goto err_out;
	}

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data) {
		dev_err(&pdev->dev, "data memory allocation failed\n");
		ret = -ENOMEM;
		goto err_out;
	}

	data->gpio = pdata->gpio;
	data->dock_switch.name = "dock";

	ret = switch_dev_register(&data->dock_switch);
	if (ret) {
		dev_err(&pdev->dev, "error registering switch device\n");
		goto err_free_mem;
	}

	ret = gpio_request_one(data->gpio, GPIOF_IN, "dock-gpio");
	if (ret) {
		dev_err(&pdev->dev, "gpio request failure\n");
		goto err_free_switch_dev;
	}

	data->irq = gpio_to_irq(data->gpio);
	if (data->irq < 0) {
		ret = data->irq;
		goto err_free_gpio;
	}

	ret = request_threaded_irq(data->irq, NULL, dock_irq,
				pdata->irqflags, pdev->name, data);
	if (ret) {
		dev_err(&pdev->dev, "request_threaded_irq failed\n");
		goto err_free_gpio;
	}

	data->can_sleep = gpio_cansleep(data->gpio);
	platform_set_drvdata(pdev, data);

	/* Report initial Docking state */
	report_dock_state(data);

	return 0;

err_free_gpio:
	gpio_free(data->gpio);
err_free_switch_dev:
	switch_dev_unregister(&data->dock_switch);
err_free_mem:
	kfree(data);
err_out:
	return ret;
}

static int __exit sevm_dockstation_remove(struct platform_device *pdev)
{
	struct sevm_dockstation *data = platform_get_drvdata(pdev);

	switch_dev_unregister(&data->dock_switch);
	free_irq(data->irq, data);
	gpio_free(data->gpio);
	kfree(data);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

#ifdef CONFIG_PM
static int dockstation_suspend(struct device *dev)
{
	return 0;
}

static int dockstation_resume(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops dockstation_pm_ops = {
	.suspend	= dockstation_suspend,
	.resume		= dockstation_resume,
};
#endif

static struct platform_driver sevm_dockstation_driver = {
	.probe		= sevm_dockstation_probe,
	.remove		= __devexit_p(sevm_dockstation_remove),
	.driver		= {
		.name	= DOCK_STATION_NAME,
		.owner	= THIS_MODULE,
#ifdef CONFIG_PM
		.pm	= &dockstation_pm_ops,
#endif
	},
};

static int __init sevm_dockstation_init(void)
{
	return platform_driver_probe(&sevm_dockstation_driver,
						sevm_dockstation_probe);
}
module_init(sevm_dockstation_init);

static void __exit sevm_dockstation_exit(void)
{
	platform_driver_unregister(&sevm_dockstation_driver);
}
module_exit(sevm_dockstation_exit);

MODULE_ALIAS("platform:sevm_dock_station");
MODULE_DESCRIPTION("sEVM Docking Station");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Leed Aguilar <leed.aguilar@ti.com>");
