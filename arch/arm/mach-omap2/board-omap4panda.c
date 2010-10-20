/*
 * Board support file for OMAP4430 based Panda.
 *
 * Copyright (C) 2010 Texas Instruments
 *
 * Author: David Anders <x0132446@ti.com>
 *
 * Based on mach-omap2/board-4430sdp.c
 *
 * Author: Santosh Shilimkar <santosh.shilimkar@ti.com>
 *
 * Based on mach-omap2/board-3430sdp.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/leds.h>
#include <linux/gpio.h>
#include <linux/usb/otg.h>
#include <linux/i2c/twl.h>
#include <linux/regulator/machine.h>
#include <linux/spi/spi.h>
#include <linux/interrupt.h>

#include <mach/hardware.h>
#include <mach/omap4-common.h>
#include <mach/emif.h>
#include <mach/lpddr2-elpida.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>

#include <plat/board.h>
#include <plat/common.h>
#include <plat/control.h>
#include <plat/timer-gp.h>
#include <plat/display.h>
#include <linux/delay.h>
#include <plat/usb.h>
#include <plat/omap_device.h>
#include <plat/omap_hwmod.h>
#include <plat/mmc.h>
#include <plat/hwspinlock.h>
#include "hsmmc.h"

#define GPIO_HUB_POWER 1
#define GPIO_HUB_NRESET_39 39
#define GPIO_HUB_NRESET_62 62
#define GPIO_BOARD_ID0 182
#define GPIO_BOARD_ID1 101
#define GPIO_BOARD_ID2 171

static int board_revision;

/* wl127x BT, FM, GPS connectivity chip */
static int gpios[] = {46, -1, -1};
static struct platform_device wl127x_device = {
       .name           = "kim",
       .id             = -1,
       .dev.platform_data = &gpios,
};

static struct gpio_led gpio_leds[] = {
	{
		.name			= "pandaboard::status1",
		.default_trigger	= "heartbeat",
		.gpio			= 7,
	},
	{
		.name			= "pandaboard::status2",
		.default_trigger	= "mmc0",
		.gpio			= 8,
	},
};

static struct gpio_led_platform_data gpio_led_info = {
	.leds		= gpio_leds,
	.num_leds	= ARRAY_SIZE(gpio_leds),
};

static struct platform_device leds_gpio = {
	.name	= "leds-gpio",
	.id	= -1,
	.dev	= {
		.platform_data	= &gpio_led_info,
	},
};

#ifdef CONFIG_OMAP2_DSS_HDMI
static struct platform_device sdp4430_hdmi_audio_device = {
	.name		= "hdmi-dai",
	.id		= -1,
};

static int panda_panel_enable_hdmi(struct omap_dss_device *dssdev)
{
	gpio_request(HDMI_GPIO_60 , "hdmi_gpio_60");
	gpio_request(HDMI_GPIO_41 , "hdmi_gpio_41");
	gpio_direction_output(HDMI_GPIO_60, 0);
	gpio_direction_output(HDMI_GPIO_41, 0);
	gpio_set_value(HDMI_GPIO_60, 1);
	gpio_set_value(HDMI_GPIO_41, 1);
	gpio_set_value(HDMI_GPIO_60, 0);
	gpio_set_value(HDMI_GPIO_41, 0);
	gpio_set_value(HDMI_GPIO_60, 1);
	gpio_set_value(HDMI_GPIO_41, 1);

	return 0;
}

static void panda_panel_disable_hdmi(struct omap_dss_device *dssdev)
{
	gpio_set_value(HDMI_GPIO_60, 1);
	gpio_set_value(HDMI_GPIO_41, 1);
}

static __attribute__ ((unused)) void __init panda_hdmi_init(void)
{
	return;
}

static struct omap_dss_device panda_hdmi_device = {
	.name = "hdmi",
	.driver_name = "hdmi_panel",
	.type = OMAP_DISPLAY_TYPE_HDMI,
	.phy.dpi.data_lines = 24,
	.platform_enable = panda_panel_enable_hdmi,
	.platform_disable = panda_panel_disable_hdmi,
	.channel = OMAP_DSS_CHANNEL_DIGIT,
};

static struct omap_dss_device *panda_dss_devices[] = {
	&panda_hdmi_device,
};

static struct omap_dss_board_info panda_dss_data = {
	.num_devices	=	ARRAY_SIZE(panda_dss_devices),
	.devices	=	panda_dss_devices,
	.default_device	=	&panda_hdmi_device,
};

static struct platform_device *panda_devices[] __initdata = {
	&leds_gpio,
	&sdp4430_hdmi_audio_device,
	&wl127x_device
};

static void __init omap4_display_init(void)
{
	void __iomem *phymux_base = NULL;
	unsigned int dsimux = 0xFFFFFFFF;
	phymux_base = ioremap(0x4A100000, 0x1000);
	/* Turning on DSI PHY Mux*/
	__raw_writel(dsimux, phymux_base+0x618);
	dsimux = __raw_readl(phymux_base+0x618);
}
#else

static struct platform_device *panda_devices[] __initdata = {
	&leds_gpio,
	&wl127x_device,
};

static void __init omap4_display_init(void) {}

#endif

static void __init omap_panda_init_irq(void)
{
	omap2_init_common_hw(NULL, NULL);
#ifdef CONFIG_OMAP_32K_TIMER
	omap2_gp_clockevent_set_gptimer(1);
#endif
	gic_init_irq();
}

static struct omap_musb_board_data musb_board_data = {
	.interface_type		= MUSB_INTERFACE_UTMI,
#ifdef CONFIG_USB_MUSB_OTG
	.mode			= MUSB_OTG,
#elif defined(CONFIG_USB_MUSB_HDRC_HCD)
	.mode			= MUSB_HOST,
#elif defined(CONFIG_USB_GADGET_MUSB_HDRC)
	.mode			= MUSB_PERIPHERAL,
#endif
	.power			= 100,
};

static const struct usbhs_omap_platform_data usbhs_pdata __initconst = {
	.port_mode[0] = OMAP_EHCI_PORT_MODE_PHY,
	.port_mode[1] = OMAP_USBHS_PORT_MODE_UNUSED,
	.port_mode[2] = OMAP_USBHS_PORT_MODE_UNUSED,
	.phy_reset  = false,
	.reset_gpio_port[0]  = -EINVAL,
	.reset_gpio_port[1]  = -EINVAL,
	.reset_gpio_port[2]  = -EINVAL
};


static struct omap2_hsmmc_info mmc[] = {
	{
		.mmc		= 1,
		.caps		= MMC_CAP_4_BIT_DATA | MMC_CAP_8_BIT_DATA,
		.gpio_wp	= -EINVAL,
	},
	{
		.mmc            = 5,
		.caps		= MMC_CAP_4_BIT_DATA | MMC_CAP_8_BIT_DATA,
		.gpio_cd        = -EINVAL,
		.gpio_wp        = 4,
		.ocr_mask       = MMC_VDD_165_195,
#ifdef CONFIG_PM_RUNTIME
		.power_saving   = true,
#endif
	},
	{}	/* Terminator */
};

static struct regulator_consumer_supply panda_vmmc_supply[] = {
	{
		.supply = "vmmc",
		.dev_name = "mmci-omap-hs.0",
	},
	{
		.supply = "vmmc",
		.dev_name = "mmci-omap-hs.5",
	},
};

static int omap4_twl6030_hsmmc_late_init(struct device *dev)
{
	int ret = 0;
	struct platform_device *pdev = container_of(dev,
				struct platform_device, dev);
	struct omap_mmc_platform_data *pdata = dev->platform_data;

	/* Setting MMC1 Card detect Irq */
	if (pdev->id == 0) {
		ret = twl6030_mmc_card_detect_config();
		if (ret)
			pr_err("Failed configuring MMC1 card detect\n");
		pdata->slots[0].card_detect_irq = TWL6030_IRQ_BASE +
						MMCDETECT_INTR_OFFSET;
		pdata->slots[0].card_detect = twl6030_mmc_card_detect;
	}
	return ret;
}

static __init void omap4_twl6030_hsmmc_set_late_init(struct device *dev)
{
	struct omap_mmc_platform_data *pdata;

	/* dev can be null if CONFIG_MMC_OMAP_HS is not set */
	if (!dev)
		return;

	pdata = dev->platform_data;
	pdata->init = omap4_twl6030_hsmmc_late_init;
}

static int __init omap4_twl6030_hsmmc_init(struct omap2_hsmmc_info *controllers)
{
	struct omap2_hsmmc_info *c;

	omap2_hsmmc_init(controllers);
	for (c = controllers; c->mmc; c++)
		omap4_twl6030_hsmmc_set_late_init(c->dev);

	return 0;
}

static struct regulator_init_data panda_vaux2 = {
	.constraints = {
		.min_uV			= 1200000,
		.max_uV			= 2800000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask	 = REGULATOR_CHANGE_VOLTAGE
					| REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
};

static struct regulator_init_data panda_vaux3 = {
	.constraints = {
		.min_uV			= 1000000,
		.max_uV			= 3000000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask	 = REGULATOR_CHANGE_VOLTAGE
					| REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
};

/* VMMC1 for MMC1 card */
static struct regulator_init_data panda_vmmc = {
	.constraints = {
		.min_uV			= 1200000,
		.max_uV			= 3000000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask	 = REGULATOR_CHANGE_VOLTAGE
					| REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies  = 2,
	.consumer_supplies      = panda_vmmc_supply,
};

static struct regulator_init_data panda_vpp = {
	.constraints = {
		.min_uV			= 1800000,
		.max_uV			= 2500000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask	 = REGULATOR_CHANGE_VOLTAGE
					| REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
};

static struct regulator_init_data panda_vana = {
	.constraints = {
		.min_uV			= 2100000,
		.max_uV			= 2100000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask	 = REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
};

static struct regulator_init_data panda_vcxio = {
	.constraints = {
		.min_uV			= 1800000,
		.max_uV			= 1800000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask	 = REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
};

static struct regulator_init_data panda_vdac = {
	.constraints = {
		.min_uV			= 1800000,
		.max_uV			= 1800000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask	 = REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
};

static struct regulator_init_data panda_vusb = {
	.constraints = {
		.min_uV			= 3300000,
		.max_uV			= 3300000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask	 =	REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
};

static struct twl4030_codec_audio_data twl6040_audio = {
	.audio_mclk	= 38400000,
	.audpwron_gpio  = 127,
	.naudint_irq    = OMAP44XX_IRQ_SYS_2N,
};

static struct twl4030_codec_data twl6040_codec = {
	.audio_mclk	= 38400000,
	.audio	= &twl6040_audio,
};

static struct twl4030_madc_platform_data panda_gpadc_data = {
	.irq_line	= 1,
};

static struct twl4030_platform_data panda_twldata = {
	.irq_base	= TWL6030_IRQ_BASE,
	.irq_end	= TWL6030_IRQ_END,

	/* Regulators */
	.vmmc		= &panda_vmmc,
	.vpp		= &panda_vpp,
	.vana		= &panda_vana,
	.vcxio		= &panda_vcxio,
	.vdac		= &panda_vdac,
	.vusb		= &panda_vusb,
	.vaux2		= &panda_vaux2,
	.vaux3		= &panda_vaux3,
	.madc           = &panda_gpadc_data,

	/* children */
	.codec		= &twl6040_codec,
};

static struct i2c_board_info __initdata panda_i2c_boardinfo[] = {
	{
		I2C_BOARD_INFO("twl6030", 0x48),
		.flags = I2C_CLIENT_WAKE,
		.irq = OMAP44XX_IRQ_SYS_1N,
		.platform_data = &panda_twldata,
	},
};

/*
 * LPDDR2 Configeration Data
 * The memory organisation is as below :
 *	EMIF1 - CS0 -	2 Gb
 *		CS1 -	2 Gb
 *	EMIF2 - CS0 -	2 Gb
 *		CS1 -	2 Gb
 *	--------------------
 *	TOTAL -		8 Gb
 *
 * Same devices installed on EMIF1 and EMIF2
 */
static __initdata struct emif_device_details emif_devices = {
	.cs0_device = &elpida_2G_S4,
	.cs1_device = &elpida_2G_S4
};

static struct omap_i2c_bus_board_data __initdata panda_i2c_bus_pdata;
static void __init omap_i2c_hwspinlock_init(int bus_id,
		unsigned int spinlock_id, struct omap_i2c_bus_board_data *pdata)
{
	pdata->handle = hwspinlock_request_specific(spinlock_id);
	if (pdata->handle != NULL) {
		pdata->hwspinlock_lock = hwspinlock_lock;
		pdata->hwspinlock_unlock = hwspinlock_unlock;
	} else {
		pr_err("I2C hwspinlock request failed for bus %d\n", bus_id);
	}
}

static int __init omap4_i2c_init(void)
{
	omap_i2c_hwspinlock_init(1, 0, &panda_i2c_bus_pdata);
	/* Phoenix Audio IC needs I2C1 to start with 400 KHz and less */
	omap_register_i2c_bus(1, 400, &panda_i2c_bus_pdata,
		panda_i2c_boardinfo, ARRAY_SIZE(panda_i2c_boardinfo));
	return 0;
}


static void __init omap4_ehci_init(void)
{
	int hub_nreset, ret;

	if (board_revision)
		hub_nreset = GPIO_HUB_NRESET_62;
	else
		hub_nreset = GPIO_HUB_NRESET_39;

	/* disable the power to the usb hub prior to init */
	ret = gpio_request(GPIO_HUB_POWER, "hub_power");
	if (ret) {
		pr_err("Cannot request GPIO %d\n", GPIO_HUB_POWER);
		goto error1;
	}
	gpio_export(GPIO_HUB_POWER, 0);
	gpio_direction_output(GPIO_HUB_POWER, 0);
	gpio_set_value(GPIO_HUB_POWER, 0);

	/* reset phy+hub */
	ret = gpio_request(hub_nreset, "hub_nreset");
	if (ret) {
		pr_err("Cannot request GPIO %d\n", hub_nreset);
		goto error2;
	}
	gpio_export(hub_nreset, 0);
	gpio_direction_output(hub_nreset, 0);
	gpio_set_value(hub_nreset, 0);
	gpio_set_value(hub_nreset, 1);

	usb_uhhtll_init(&usbhs_pdata);
	usb_ehci_init();

	/* enable power to hub */
	gpio_set_value(GPIO_HUB_POWER, 1);
	return;

error2:
	gpio_free(GPIO_HUB_POWER);
error1:
	pr_err("Unable to initialize EHCI power/reset\n");
	return;

}

#ifdef CONFIG_TIWLAN_SDIO
static void pad_config(unsigned long pad_addr, u32 andmask, u32 ormask)
{
	int val;
	u32 *addr;

	addr = (u32 *) ioremap(pad_addr, 4);
	if (!addr) {
		printk(KERN_ERR"OMAP_pad_config: ioremap failed with addr %lx\n",
		pad_addr);
	return;
	}

	val =  __raw_readl(addr);
	val &= andmask;
	val |= ormask;
	__raw_writel(val, addr);

	iounmap(addr);
}

void wlan_1273_config(void)
{
	pad_config(0x4A100078, 0xFFECFFFF, 0x00030000);
	pad_config(0x4A100064, 0xFFEFFFFF, 0x000B0000);
	if (gpio_request(43, NULL) != 0)
		printk(KERN_ERR "GPIO 43 request failed\n");
	gpio_direction_output(43, 0);
	return ;
}
#endif

static void __init panda_boardrev_init(void)
{
	int ret;

	ret = gpio_request(GPIO_BOARD_ID0, "board_id0");
	if (ret) {
		pr_err("Cannot request GPIO %d\n", GPIO_BOARD_ID0);
		goto error1;
	}

	ret = gpio_request(GPIO_BOARD_ID1, "board_id1");
	if (ret) {
		pr_err("Cannot request GPIO %d\n", GPIO_BOARD_ID1);
		goto error2;
	}

	ret = gpio_request(GPIO_BOARD_ID2, "board_id2");
	if (ret) {
		pr_err("Cannot request GPIO %d\n", GPIO_BOARD_ID2);
		goto error3;
	}
	gpio_export(GPIO_BOARD_ID0, 0);
	gpio_export(GPIO_BOARD_ID1, 0);
	gpio_export(GPIO_BOARD_ID2, 0);

	board_revision = gpio_get_value(GPIO_BOARD_ID0);
	board_revision |= (gpio_get_value(GPIO_BOARD_ID1)<<1);
	board_revision |= (gpio_get_value(GPIO_BOARD_ID2)<<2);

	pr_info("PandaBoard Revision: %03d\n", board_revision);
	return;

error3:
	gpio_free(GPIO_BOARD_ID1);
error2:
	gpio_free(GPIO_BOARD_ID0);
error1:
	board_revision = 0;
	pr_err("Unable to detemine PandaBoard Revision\n");
	return;

}

static void __init omap_panda_init(void)
{

	panda_boardrev_init();

	omap_emif_setup_device_details(&emif_devices, &emif_devices);
	omap_init_emif_timings();
	omap4_i2c_init();
	omap4_display_init();
	platform_add_devices(panda_devices, ARRAY_SIZE(panda_devices));
	omap_serial_init();
	omap4_twl6030_hsmmc_init(mmc);

#ifdef CONFIG_TIWLAN_SDIO
	wlan_1273_config();
#endif

	usb_musb_init(&musb_board_data);
	omap4_ehci_init();
#ifdef CONFIG_OMAP2_DSS_HDMI
	omap_display_init(&panda_dss_data);
#endif
}

static void __init omap_panda_map_io(void)
{
	omap2_set_globals_443x();
	omap44xx_map_common_io();
}

MACHINE_START(OMAP4_PANDA, "OMAP4430 Panda Board")
	.phys_io	= 0x48000000,
	.io_pg_offst	= ((0xfa000000) >> 18) & 0xfffc,
	.boot_params	= 0x80000100,
	.map_io		= omap_panda_map_io,
	.init_irq	= omap_panda_init_irq,
	.init_machine	= omap_panda_init,
	.timer		= &omap_timer,
MACHINE_END
