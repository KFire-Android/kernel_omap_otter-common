/*
 * User space cooling intofication driver file
 *
 * Copyright (C) 2012 Texas Instruments Incorporated - http://www.ti.com/
 * Author: Radhesh Fadnis <radhesh.fadnis@ti.com>
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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/types.h>

#include <linux/thermal_framework.h>

/*
 * user_cooling_dev structure
 * @pdev - Platform device pointer
 * @dev - device pointer
 */
struct user_cooling_dev {
	struct device *dev;
	struct thermal_dev *therm_fw;
	int cur_cooling_level;
};

/*
 * user_apply_cooling: based on requested cooling level, update the userspace
 * @param cooling_level: percentage of required cooling at the moment
 *
 * The requested cooling level is updated to user space. And it is upto the
 * listener/policy at user space to take approriate action on the cooling_level.
*/
static int user_apply_cooling(struct thermal_dev *tdev, int cooling_level)
{
	struct platform_device *pdev = to_platform_device(tdev->dev);
	struct user_cooling_dev *cooling_dev = platform_get_drvdata(pdev);

	cooling_dev->cur_cooling_level = cooling_level;

	pr_debug("%s: Unthrottle cool level %i curr cool %i\n",
		 __func__, cooling_level, cooling_dev->cur_cooling_level);
	kobject_uevent(&cooling_dev->dev->kobj, KOBJ_CHANGE);

	return 0;
}

static struct thermal_dev_ops user_cooling_ops = {
	.cool_device = user_apply_cooling,
};

static ssize_t
level_show(struct device *dev, struct device_attribute *attr,
	   char *buf)
{
	struct user_cooling_dev *cooling_dev = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", cooling_dev->cur_cooling_level);
}
static DEVICE_ATTR(level, 0444, level_show, NULL);

static int __devinit user_cooling_dev_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct user_cooling_dev *cooling_dev;
	const char *domain_name = (const char *)pdev->dev.platform_data;
	int ret = 0;

	cooling_dev = devm_kzalloc(&pdev->dev, sizeof(struct user_cooling_dev),
				   GFP_KERNEL);
	if (!cooling_dev)
		return -ENOMEM;

	cooling_dev->dev = dev;
	platform_set_drvdata(pdev, cooling_dev);

	cooling_dev->therm_fw = devm_kzalloc(&pdev->dev,
					     sizeof(struct thermal_dev),
					     GFP_KERNEL);
	if (cooling_dev->therm_fw) {
		cooling_dev->therm_fw->name = "user_cooling_dev";
		cooling_dev->therm_fw->domain_name = domain_name;
		cooling_dev->therm_fw->dev = cooling_dev->dev;
		cooling_dev->therm_fw->dev_ops = &user_cooling_ops;
		thermal_cooling_dev_register(cooling_dev->therm_fw);
	} else {
		dev_err(&pdev->dev, "%s:Cannot alloc memory for thermal fw\n",
			__func__);
		ret = -ENOMEM;
		goto free_user_cooling;
	}

	cooling_dev->cur_cooling_level = 0;

	ret = device_create_file(cooling_dev->dev, &dev_attr_level);
	if (ret)
		goto free_user_cooling;

	kobject_uevent(&pdev->dev.kobj, KOBJ_ADD);

	dev_info(&pdev->dev, "%s: Initialised\n", cooling_dev->therm_fw->name);

	return 0;

free_user_cooling:
	kobject_uevent(&cooling_dev->dev->kobj, KOBJ_REMOVE);

	return ret;
}

static int __exit user_cooling_dev_remove(struct platform_device *pdev)
{
	struct user_cooling_dev *cooling_dev = platform_get_drvdata(pdev);

	thermal_cooling_dev_unregister(cooling_dev->therm_fw);

	kobject_uevent(&cooling_dev->dev->kobj, KOBJ_REMOVE);

	platform_set_drvdata(pdev, NULL);

	return 0;
}

static struct platform_driver user_cooling_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name = "user_cooling_device",
	},
	.remove = __exit_p(user_cooling_dev_remove),
};

static int __init user_cooling_dev_init(void)
{
	return platform_driver_probe(&user_cooling_driver,
					user_cooling_dev_probe);
}

static void __exit user_cooling_dev_exit(void)
{
	platform_driver_unregister(&user_cooling_driver);
}

module_init(user_cooling_dev_init);
module_exit(user_cooling_dev_exit);

MODULE_DESCRIPTION("User Space Dummy Cooling Driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:user_cooling_device");
MODULE_AUTHOR("Texas Instruments Inc");
