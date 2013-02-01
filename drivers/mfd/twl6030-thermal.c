/*
 * drivers/mfd/twl6030-thermal.c
 *
 * Thermal Monitoring driver for TWL6030
 *
 * Copyright (C) 2013 Texas Instruments Corporation
 *
 * Written by Ruslan Ruslichenko <ruslan.ruslichenko@ti.com>
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
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/reboot.h>
#include <linux/syscalls.h>
#include <linux/i2c/twl.h>

#define TMP_CFG_REG		0x05
#define TMP_CFG_THERM_HD_MASK	0x10

static irqreturn_t twl6030_hotdie_interrupt(int irq, void *di)
{
	pr_emerg("%s: TWL overheat detected.\n"\
		 "Shutting down to avoid device malfunction...", __func__);
	if (pm_power_off) {
		kernel_power_off();
	} else {
		WARN(1, "FIXME: NO pm_power_off!!! trying restart\n");
		kernel_restart(NULL);
	}

	return IRQ_HANDLED;
}

static int twl6030_thermal_probe(struct platform_device *pdev)
{
	int err = 0;
	struct twl6030_thermal_data *pdata = pdev->dev.platform_data;
	int irq;

	if (!pdata) {
		dev_dbg(&pdev->dev, "platform_data not available\n");
		return -EINVAL;
	}

	err = twl_i2c_write_u8(TWL6030_MODULE_PM_MISC, pdata->hotdie_cfg,
			       TMP_CFG_REG);
	if (err) {
		dev_dbg(&pdev->dev, "failed to configure hotdie temp\n");
		goto exit;
	}

	irq = platform_get_irq(pdev, 0);
	if (IS_ERR_VALUE(irq)) {
		dev_dbg(&pdev->dev, "Can't get IRQ num for device: %d\n", err);
		goto exit;
	}

	err = request_threaded_irq(irq, NULL, twl6030_hotdie_interrupt,
				   IRQF_TRIGGER_RISING, "twl6030_hotdie", pdev);
	if (IS_ERR_VALUE(err)) {
		dev_dbg(&pdev->dev, "Can't allocate IRQ for hotdie: %d\n", err);
		goto exit;
	}

	twl6030_interrupt_unmask(TWL6030_HOTDIE_INT_MASK, REG_INT_MSK_LINE_A);
	twl6030_interrupt_unmask(TWL6030_HOTDIE_INT_MASK, REG_INT_MSK_STS_A);

exit:
	return err;
}

static int __devexit twl6030_thermal_remove(struct platform_device *pdev)
{
	int irq;

	twl6030_interrupt_mask(TWL6030_HOTDIE_INT_MASK, REG_INT_MSK_LINE_A);
	twl6030_interrupt_mask(TWL6030_HOTDIE_INT_MASK, REG_INT_MSK_STS_A);

	irq = platform_get_irq(pdev, 0);
	free_irq(irq, pdev);

	return 0;
}

static struct platform_driver twl6030_thermal_driver = {
	.probe		= twl6030_thermal_probe,
	.remove		= __devexit_p(twl6030_thermal_remove),
	.driver		= {
		.name	= "twl6030_thermal",
	},
};

static int __init twl6030_thremal_init(void)
{
	return platform_driver_register(&twl6030_thermal_driver);
}
module_init(twl6030_thremal_init);

static void __exit twl6030_thermal_exit(void)
{
	platform_driver_unregister(&twl6030_thermal_driver);
}
module_exit(twl6030_thermal_exit);

MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:twl6030_thremal");
MODULE_DESCRIPTION("Driver for TWL6030 thermal monitoring");
MODULE_AUTHOR("Texas Instruments, Inc.");
