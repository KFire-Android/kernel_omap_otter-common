/*
 * /drivers/mfd/twl6030-poweroff.c
 *
 * Power off device
 *
 * Copyright (C) 2010 Texas Instruments Corporation
 *
 * Written by Rajeev Kulkarni <rajeevk@ti.com>
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
#include <linux/pm.h>
#include <linux/i2c/twl.h>
#include <linux/gpio.h>

#define TWL6030_PHONIX_DEV_ON	0x25
#define APP_DEVOFF	(1<<0)
#define CON_DEVOFF	(1<<1)
#define MOD_DEVOFF	(1<<2)
#define SW_RESET_DEVOFF (1<<6)

void twl6030_reboot(char str, const char *cmd)
{
	u8 uninitialized_var(val);
	int err, i;
//This is a work around to solve the OTA factory reset issue
omap_writel(0x29b18c80 , 0x4a307B94);//VCORE 0x29
omap_writel(0x37b78c80 , 0x4a307B98);//VMPU  0x37
omap_writel(0x1ba98c80 , 0x4a307B9C);//VIVA  0x1b
        
	pr_warning("Device is rebooting thanks to Phonix... \n", i);

	val = SW_RESET_DEVOFF;

	err = twl_i2c_write_u8(TWL6030_MODULE_ID0, val,
			       TWL6030_PHONIX_DEV_ON);

	if (err) {
		pr_warning("I2C error %d writing PHONIX_DEV_ON\n", err);
	}
}
EXPORT_SYMBOL(twl6030_reboot);

void twl6030_poweroff(void)
{
	u8 uninitialized_var(val);
	int err;

	err = twl_i2c_read_u8(TWL6030_MODULE_ID0, &val,
				  TWL6030_PHONIX_DEV_ON);
	if (err) {
		pr_warning("I2C error %d reading PHONIX_DEV_ON\n", err);
		return;
	}

	val |= APP_DEVOFF | CON_DEVOFF | MOD_DEVOFF;

	err = twl_i2c_write_u8(TWL6030_MODULE_ID0, val,
				   TWL6030_PHONIX_DEV_ON);

	if (err) {
		pr_warning("I2C error %d writing PHONIX_DEV_ON\n", err);
		return;
	}

	return;
}
EXPORT_SYMBOL(twl6030_poweroff);
static int __init twl6030_poweroff_init(void)
{
	pm_power_off   = twl6030_poweroff;
	arm_pm_restart = twl6030_reboot;
	return 0;
}

static void __exit twl6030_poweroff_exit(void)
{
	pm_power_off = NULL;
}

module_init(twl6030_poweroff_init);
module_exit(twl6030_poweroff_exit);

MODULE_DESCRIPTION("Triton2 device power off");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Rajeev Kulkarni");
