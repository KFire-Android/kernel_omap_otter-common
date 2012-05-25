/*
 * linux/arch/arm/plat-omap/i2c.c
 *
 * Helper module for board specific I2C bus registration
 *
 * Copyright (C) 2007 Nokia Corporation.
 *
 * Contact: Jarkko Nikula <jhnikula@gmail.com>
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

#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/module.h>

#include <mach/irqs.h>
#include "../../../../arch/arm/mach-omap2/control.h"
#include <plat/mux.h>
#include <plat/i2c.h>
#include <plat/omap_device.h>
#include <plat/omap-pm.h>

#define OMAP_I2C_SIZE		0x3f
#define OMAP1_I2C_BASE		0xfffb3800

static const char name[] = "omap_i2c";

#define I2C_RESOURCE_BUILDER(base, irq)			\
	{						\
		.start	= (base),			\
		.end	= (base) + OMAP_I2C_SIZE,	\
		.flags	= IORESOURCE_MEM,		\
	},						\
	{						\
		.start	= (irq),			\
		.flags	= IORESOURCE_IRQ,		\
	},

static struct resource i2c_resources[][2] = {
	{ I2C_RESOURCE_BUILDER(0, 0) },
};

#define I2C_DEV_BUILDER(bus_id, res, data)		\
	{						\
		.id	= (bus_id),			\
		.name	= name,				\
		.num_resources	= ARRAY_SIZE(res),	\
		.resource	= (res),		\
		.dev		= {			\
			.platform_data	= (data),	\
		},					\
	}

#define MAX_OMAP_I2C_HWMOD_NAME_LEN	16
#define OMAP_I2C_MAX_CONTROLLERS 5
static struct omap_i2c_bus_platform_data i2c_pdata[OMAP_I2C_MAX_CONTROLLERS];
static struct platform_device omap_i2c_devices[] = {
	I2C_DEV_BUILDER(1, i2c_resources[0], &i2c_pdata[0]),
};

#define OMAP_I2C_CMDLINE_SETUP	(BIT(31))

static int omap_i2c_nr_ports(void)
{
	int ports = 0;

	if (cpu_class_is_omap1())
		ports = 1;
	else if (cpu_is_omap24xx())
		ports = 2;
	else if (cpu_is_omap34xx())
		ports = 3;
	else if (cpu_is_omap44xx())
		ports = 4;
	else if (cpu_is_omap54xx())
		ports = 5;

	return ports;
}

static inline int omap1_i2c_add_bus(int bus_id)
{
	struct platform_device *pdev;
	struct omap_i2c_bus_platform_data *pdata;
	struct resource *res;

	omap1_i2c_mux_pins(bus_id);

	pdev = &omap_i2c_devices[bus_id - 1];
	res = pdev->resource;
	res[0].start = OMAP1_I2C_BASE;
	res[0].end = res[0].start + OMAP_I2C_SIZE;
	res[1].start = INT_I2C;
	pdata = &i2c_pdata[bus_id - 1];

	/* all OMAP1 have IP version 1 register set */
	pdata->rev = OMAP_I2C_IP_VERSION_1;

	/* all OMAP1 I2C are implemented like this */
	pdata->flags = OMAP_I2C_FLAG_NO_FIFO |
		       OMAP_I2C_FLAG_SIMPLE_CLOCK |
		       OMAP_I2C_FLAG_16BIT_DATA_REG |
		       OMAP_I2C_FLAG_ALWAYS_ARMXOR_CLK;

	/* how the cpu bus is wired up differs for 7xx only */

	if (cpu_is_omap7xx())
		pdata->flags |= OMAP_I2C_FLAG_BUS_SHIFT_1;
	else
		pdata->flags |= OMAP_I2C_FLAG_BUS_SHIFT_2;

	return platform_device_register(pdev);
}


#ifdef CONFIG_ARCH_OMAP2PLUS
static inline int omap2_i2c_add_bus(int bus_id)
{
	int l;
	struct omap_hwmod *oh;
	struct platform_device *pdev;
	char oh_name[MAX_OMAP_I2C_HWMOD_NAME_LEN];
	struct omap_i2c_bus_platform_data *pdata;
	struct omap_i2c_dev_attr *dev_attr;

	omap2_i2c_mux_pins(bus_id);

	l = snprintf(oh_name, MAX_OMAP_I2C_HWMOD_NAME_LEN, "i2c%d", bus_id);
	WARN(l >= MAX_OMAP_I2C_HWMOD_NAME_LEN,
		"String buffer overflow in I2C%d device setup\n", bus_id);
	oh = omap_hwmod_lookup(oh_name);
	if (!oh) {
			pr_err("Could not look up %s\n", oh_name);
			return -EEXIST;
	}

	pdata = &i2c_pdata[bus_id - 1];
	/*
	 * pass the hwmod class's CPU-specific knowledge of I2C IP revision in
	 * use, and functionality implementation flags, up to the OMAP I2C
	 * driver via platform data
	 */
	pdata->rev = oh->class->rev;

	dev_attr = (struct omap_i2c_dev_attr *)oh->dev_attr;
	pdata->flags = dev_attr->flags;

	pdata->get_context_loss_count = omap_pm_get_dev_context_loss_count;

	pdev = omap_device_build(name, bus_id, oh, pdata,
			sizeof(struct omap_i2c_bus_platform_data),
			NULL, 0, 0);
	WARN(IS_ERR(pdev), "Could not build omap_device for %s\n", name);

	return PTR_RET(pdev);
}
#else
static inline int omap2_i2c_add_bus(int bus_id)
{
	return 0;
}
#endif

static int __init omap_i2c_add_bus(int bus_id)
{
	if (cpu_class_is_omap1())
		return omap1_i2c_add_bus(bus_id);
	else
		return omap2_i2c_add_bus(bus_id);
}

/**
 * omap_i2c_bus_setup - Process command line options for the I2C bus speed
 * @str: String of options
 *
 * This function allow to override the default I2C bus speed for given I2C
 * bus with a command line option.
 *
 * Format: i2c_bus=bus_id,clkrate (in kHz)
 *
 * Returns 1 on success, 0 otherwise.
 */
static int __init omap_i2c_bus_setup(char *str)
{
	int ports;
	int ints[3];

	ports = omap_i2c_nr_ports();
	get_options(str, 3, ints);
	if (ints[0] < 2 || ints[1] < 1 || ints[1] > ports)
		return 0;
	i2c_pdata[ints[1] - 1].clkrate = ints[2];
	i2c_pdata[ints[1] - 1].clkrate |= OMAP_I2C_CMDLINE_SETUP;

	return 1;
}
__setup("i2c_bus=", omap_i2c_bus_setup);

/*
 * Register busses defined in command line but that are not registered with
 * omap_register_i2c_bus from board initialization code.
 */
static int __init omap_register_i2c_bus_cmdline(void)
{
	int i, err = 0;

	for (i = 0; i < ARRAY_SIZE(i2c_pdata); i++)
		if (i2c_pdata[i].clkrate & OMAP_I2C_CMDLINE_SETUP) {
			i2c_pdata[i].clkrate &= ~OMAP_I2C_CMDLINE_SETUP;
			err = omap_i2c_add_bus(i + 1);
			if (err)
				goto out;
		}

out:
	return err;
}
subsys_initcall(omap_register_i2c_bus_cmdline);

/**
 * omap_register_i2c_bus - register I2C bus with device descriptors
 * @bus_id: bus id counting from number 1
 * @clkrate: clock rate of the bus in kHz
 * @info: pointer into I2C device descriptor table or NULL
 * @len: number of descriptors in the table
 *
 * Returns 0 on success or an error code.
 */
int __init omap_register_i2c_bus(int bus_id, u32 clkrate,
			  struct i2c_board_info const *info,
			  unsigned len)
{
	int err;

	BUG_ON(bus_id < 1 || bus_id > omap_i2c_nr_ports());

	if (info) {
		err = i2c_register_board_info(bus_id, info, len);
		if (err)
			return err;
	}

	if (!i2c_pdata[bus_id - 1].clkrate)
		i2c_pdata[bus_id - 1].clkrate = clkrate;

	i2c_pdata[bus_id - 1].clkrate &= ~OMAP_I2C_CMDLINE_SETUP;

	return omap_i2c_add_bus(bus_id);
}

/**
 * omap_register_i2c_bus_board_data - register hwspinlock data
 * @bus_id: bus id counting from number 1
 * @pdata: pointer to the I2C bus board data
 */
void omap_register_i2c_bus_board_data(int bus_id,
				struct omap_i2c_bus_board_data *pdata)
{
	BUG_ON(bus_id < 1 || bus_id > omap_i2c_nr_ports());

	if ((pdata != NULL) && (pdata->handle != NULL)) {
		i2c_pdata[bus_id - 1].handle = pdata->handle;
		i2c_pdata[bus_id - 1].hwspin_lock_timeout =
					pdata->hwspin_lock_timeout;
		i2c_pdata[bus_id - 1].hwspin_unlock = pdata->hwspin_unlock;
	}
}

/**
 * OMAP5 I2C register pull-up definition has changed so create a new
 * API to handle the OMAP5 configuration
 *
 * omap5_i2c_pullup - setup pull-up resistors for I2C bus for OMAP5
 * @bus_id: bus id counting from number 1
 * @enable: Pull-up resistor for SDA and SCL pins
 * @glitch_free: glitch free operation for SDA and SCL pins
 *
 */
void omap5_i2c_pullup(int bus_id, int enable, int glitch_free)
{
	u32 val = 0;

	if (!cpu_is_omap54xx())
		return;

	if (bus_id < 1 || bus_id > omap_i2c_nr_ports()) {
		pr_err("%s:Wrong I2C port (%d)\n",
			__func__, bus_id);
		return;
	}

	if ((enable > 1) || (glitch_free > 1)) {
		pr_err("%s:Enable (%d) or glitch (%d) setting is wrong\n",
			__func__, enable, glitch_free);
		return;
	}

	val = omap4_ctrl_pad_readl(OMAP4_CTRL_MODULE_PAD_CORE_CONTROL_I2C_0);
	/*TODO: HACK away at the register values since there does not appear
	 * to be a header file to handle these offsets for OMAP5.  And the
	 * i2c pull-up and glitch deifinitions have changed.
	 */
	switch (bus_id) {
	case 1:
		val &= 0xfcfcffff;
		val |= enable << 16;
		val |= glitch_free << 17;
		val |= enable << 26;
		val |= glitch_free << 27;
		break;
	case 2:
		val &= 0xf3f3ffff;
		val |= enable << 18;
		val |= glitch_free << 19;
		val |= enable << 26;
		val |= glitch_free << 27;
		break;
	case 3:
		val &= 0xcfcfffff;
		val |= enable << 20;
		val |= glitch_free << 21;
		val |= enable << 28;
		val |= glitch_free << 29;
		break;
	case 4:
		val &= 0x3f3fffff;
		val |= enable << 22;
		val |= glitch_free << 23;
		val |= enable << 30;
		val |= glitch_free << 31;
		break;
	case 5:
		val &= 0xffff0fff;
		val |= enable << 12;
		val |= glitch_free << 13;
		val |= enable << 14;
		val |= glitch_free << 15;
		break;
	default:
		break;
	}

	omap4_ctrl_pad_writel(val, OMAP4_CTRL_MODULE_PAD_CORE_CONTROL_I2C_0);
}

/**
 * omap2_i2c_pullup - setup pull-up resistors for I2C bus
 * @bus_id: bus id counting from number 1
 * @sda_pullup: Pull-up resistor for SDA and SCL pins
 *
 */
void omap2_i2c_pullup(int bus_id, enum omap_i2c_pullup_values pullup)
{
	u32 val = 0;

	if (!cpu_is_omap44xx())
		return;

	if (bus_id < 1 || bus_id > omap_i2c_nr_ports() ||
			pullup > I2C_PULLUP_STD_NA_FAST_300_OM) {
		pr_err("%s:Wrong pullup (%d) or use wrong I2C port (%d)\n",
			__func__, pullup, bus_id);
		return;
	}

	val = omap4_ctrl_pad_readl(OMAP4_CTRL_MODULE_PAD_CORE_CONTROL_I2C_0);
	switch (bus_id) {
	case 1:
		/* Setup PULL-UP resistor for I2C-1 */
		val &= ~(OMAP4_I2C1_SDA_LOAD_BITS_MASK  |
			OMAP4_I2C1_SCL_LOAD_BITS_MASK  |
			OMAP4_I2C1_SDA_PULLUPRESX_MASK |
			OMAP4_I2C1_SCL_PULLUPRESX_MASK);
		val |= ((pullup << OMAP4_I2C1_SDA_LOAD_BITS_SHIFT) |
			(pullup << OMAP4_I2C1_SCL_LOAD_BITS_SHIFT));
		break;
	case 2:
		/* Setup PULL-UP resistor for I2C-2 */
		val &= ~(OMAP4_I2C2_SDA_LOAD_BITS_MASK  |
			OMAP4_I2C2_SCL_LOAD_BITS_MASK  |
			OMAP4_I2C2_SDA_PULLUPRESX_MASK |
			OMAP4_I2C2_SCL_PULLUPRESX_MASK);
		val |= ((pullup << OMAP4_I2C2_SDA_LOAD_BITS_SHIFT) |
			(pullup << OMAP4_I2C2_SCL_LOAD_BITS_SHIFT));
		break;
	case 3:
		/* Setup PULL-UP resistor for I2C-3 */
		val &= ~(OMAP4_I2C3_SDA_LOAD_BITS_MASK  |
			OMAP4_I2C3_SCL_LOAD_BITS_MASK  |
			OMAP4_I2C3_SDA_PULLUPRESX_MASK |
			OMAP4_I2C3_SCL_PULLUPRESX_MASK);
		val |= ((pullup << OMAP4_I2C3_SDA_LOAD_BITS_SHIFT) |
			(pullup << OMAP4_I2C3_SCL_LOAD_BITS_SHIFT));
		break;
	case 4:
		/* Setup PULL-UP resistor for I2C-4 */
		val &= ~(OMAP4_I2C4_SDA_LOAD_BITS_MASK  |
			OMAP4_I2C4_SCL_LOAD_BITS_MASK  |
			OMAP4_I2C4_SDA_PULLUPRESX_MASK |
			OMAP4_I2C4_SCL_PULLUPRESX_MASK);
		val |= ((pullup << OMAP4_I2C4_SDA_LOAD_BITS_SHIFT) |
			(pullup << OMAP4_I2C4_SCL_LOAD_BITS_SHIFT));
		break;
	default:
		return;
	}

	omap4_ctrl_pad_writel(val, OMAP4_CTRL_MODULE_PAD_CORE_CONTROL_I2C_0);
}

/**
 * omap_i2c_get_hwspinlockid - Get HWSPINLOCK ID for I2C device
 * @dev: I2C device
 *
 * returns the hwspinlock id or -1 if does not exist
 */
int omap_i2c_get_hwspinlockid(struct device *dev)
{
	struct omap_i2c_bus_platform_data *pdata;

	pdata = dev_get_platdata(dev);
	if (!pdata) {
		dev_err(dev, "%s: platform data is missing\n", __func__);
		return -EINVAL;
	}

	if (pdata->handle != NULL)
		return hwspin_lock_get_id(pdata->handle);
	else
		return -1;
}
EXPORT_SYMBOL_GPL(omap_i2c_get_hwspinlockid);

