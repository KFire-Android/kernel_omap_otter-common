/*
 * Board support file for OMAP44XX Tablet.
 *
 * Copyright (C) 2009 - 2012 Texas Instruments
 *
 * Authors:
 *	 Dan Murphy <dmurphy@ti.com>
 *	 Volodymyr Riazantsev <v.riazantsev@ti.com>
 *
 * Based on mach-omap2/board-4430sdp.c
 *   by Santosh Shilimkar <santosh.shilimkar@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/usb/otg.h>
#include <linux/spi/spi.h>
#include <linux/hwspinlock.h>
#include <linux/i2c/twl.h>
#include <linux/mfd/twl6040.h>
#include <linux/cdc_tcxo.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/fixed.h>
#include <linux/platform_data/omap-abe-twl6040.h>

#include <asm/hardware/gic.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>

#include <mach/hardware.h>

#include <plat/drm.h>
#include <plat/board.h>
#include <plat/usb.h>
#include <plat/mmc.h>
#include <plat/remoteproc.h>
#include <plat/omap_apps_brd_id.h>

#include "mux.h"
#include "hsmmc.h"
#include "common.h"
#include "control.h"
#include "common-board-devices.h"
#include "board-44xx-tablet.h"
#include "omap4_ion.h"
#include "omap_ram_console.h"

#define ETH_KS8851_IRQ			34
#define ETH_KS8851_POWER_ON		48
#define ETH_KS8851_QUART		138

#define FIXED_REG_VBAT_ID		0
#define FIXED_REG_VWLAN_ID		1
#define TPS62361_GPIO			7

#define OMAP4_MDM_PWR_EN_GPIO		157
#define OMAP4_GPIO_WK30			30

static struct spi_board_info tablet_spi_board_info[] __initdata = {
	{
		.modalias               = "ks8851",
		.bus_num                = 1,
		.chip_select            = 0,
		.max_speed_hz           = 24000000,
		/*
		 * .irq is set to gpio_to_irq(ETH_KS8851_IRQ)
		 * in omap_tablet_init
		 */
	},
};

static struct gpio tablet_eth_gpios[] __initdata = {
	{ ETH_KS8851_POWER_ON,	GPIOF_OUT_INIT_HIGH,	"eth_power"	},
	{ ETH_KS8851_QUART,	GPIOF_OUT_INIT_HIGH,	"quart"		},
	{ ETH_KS8851_IRQ,	GPIOF_IN,		"eth_irq"	},
};

static int __init omap_ethernet_init(void)
{
	int status;

	/* Request of GPIO lines */
	status = gpio_request_array(tablet_eth_gpios,
				    ARRAY_SIZE(tablet_eth_gpios));
	if (status)
		pr_err("Cannot request ETH GPIOs\n");

	return status;
}

static struct regulator_consumer_supply tablet_vbat_supply[] = {
	REGULATOR_SUPPLY("vddvibl", "twl6040-vibra"),
	REGULATOR_SUPPLY("vddvibr", "twl6040-vibra"),
};

static struct regulator_init_data tablet_vbat_data = {
	.constraints = {
		.always_on	= 1,
	},
	.num_consumer_supplies	= ARRAY_SIZE(tablet_vbat_supply),
	.consumer_supplies	= tablet_vbat_supply,
};

static struct fixed_voltage_config tablet_vbat_pdata = {
	.supply_name	= "VBAT",
	.microvolts	= 3750000,
	.init_data	= &tablet_vbat_data,
	.gpio		= -EINVAL,
};

static struct platform_device tablet_vbat = {
	.name		= "reg-fixed-voltage",
	.id		= FIXED_REG_VBAT_ID,
	.dev = {
		.platform_data = &tablet_vbat_pdata,
	},
};

static struct platform_device tablet_dmic_codec = {
	.name	= "dmic-codec",
	.id	= -1,
};

static struct platform_device tablet_spdif_dit_codec = {
	.name           = "spdif-dit",
	.id             = -1,
};

static struct platform_device tablet_hdmi_audio_codec = {
	.name	= "hdmi-audio-codec",
	.id	= -1,
};

static struct omap_abe_twl6040_data tablet_abe_audio_data = {
	.card_name = "tablet",
	.has_hs		= ABE_TWL6040_LEFT | ABE_TWL6040_RIGHT,
	.has_hf		= ABE_TWL6040_LEFT | ABE_TWL6040_RIGHT,
	.has_ep		= 1,
	.has_aux	= ABE_TWL6040_LEFT | ABE_TWL6040_RIGHT,
	.has_vibra	= ABE_TWL6040_LEFT | ABE_TWL6040_RIGHT,

	.has_abe	= 1,
	.has_dmic	= 1,
	.has_hsmic	= 1,
	.has_mainmic	= 1,
	.has_submic	= 1,
	.has_afm	= ABE_TWL6040_LEFT | ABE_TWL6040_RIGHT,

	.jack_detection = 1,
	/* MCLK input is 38.4MHz */
	.mclk_freq	= 38400000,
};

static struct platform_device tablet_abe_audio = {
	.name		= "omap-abe-twl6040",
	.id		= -1,
	.dev = {
		.platform_data = &tablet_abe_audio_data,
	},
};

static struct platform_device *tablet_devices[] __initdata = {
	&tablet_vbat,
	&tablet_dmic_codec,
	&tablet_spdif_dit_codec,
	&tablet_abe_audio,
	&tablet_hdmi_audio_codec,
};

static struct omap_musb_board_data musb_board_data = {
	.interface_type		= MUSB_INTERFACE_UTMI,
	.mode			= MUSB_OTG,
	.power			= 200,
};

static struct omap2_hsmmc_info mmc[] = {
	{
		.mmc		= 2,
		.caps		=  MMC_CAP_4_BIT_DATA | MMC_CAP_8_BIT_DATA,
		.gpio_cd	= -EINVAL,
		.gpio_wp	= -EINVAL,
		.nonremovable   = true,
		.ocr_mask	= MMC_VDD_29_30,
		.no_off_init	= true,
	},
	{
		.mmc		= 1,
		.caps		= MMC_CAP_4_BIT_DATA | MMC_CAP_8_BIT_DATA,
		.gpio_cd	= -EINVAL,
		.gpio_wp	= -EINVAL,
	},
	{
		.mmc		= 5,
		.caps		= MMC_CAP_4_BIT_DATA | MMC_CAP_POWER_OFF_CARD,
		.pm_caps	= MMC_PM_KEEP_POWER,
		.gpio_cd	= -EINVAL,
		.gpio_wp	= -EINVAL,
		.ocr_mask	= MMC_VDD_165_195,
		.built_in	= 1,
		.nonremovable	= true,
	},
	{}	/* Terminator */
};

static int omap4_twl6030_hsmmc_late_init(struct device *dev)
{
	int irq = 0;
	struct platform_device *pdev = container_of(dev,
				struct platform_device, dev);
	struct omap_mmc_platform_data *pdata = dev->platform_data;

	/* Setting MMC1 Card detect Irq */
	if (pdev->id == 0) {
		irq = twl6030_mmc_card_detect_config();
		if (irq < 0) {
			pr_err("Failed configuring MMC1 card detect\n");
			return irq;
		}
		pdata->slots[0].card_detect_irq = irq;
		pdata->slots[0].card_detect = twl6030_mmc_card_detect;
	}
	return 0;
}

static __init void omap4_twl6030_hsmmc_set_late_init(struct device *dev)
{
	struct omap_mmc_platform_data *pdata;

	/* dev can be null if CONFIG_MMC_OMAP_HS is not set */
	if (!dev) {
		pr_err("Failed %s\n", __func__);
		return;
	}
	pdata = dev->platform_data;
	pdata->init = omap4_twl6030_hsmmc_late_init;
}

static int __init omap4_twl6030_hsmmc_init(struct omap2_hsmmc_info *controllers)
{
	struct omap2_hsmmc_info *c;

	omap_hsmmc_init(controllers);
	for (c = controllers; c->mmc; c++)
		omap4_twl6030_hsmmc_set_late_init(&c->pdev->dev);

	return 0;
}

static struct twl6040_codec_data twl6040_codec = {
	/* single-step ramp for headset and handsfree */
	.hs_left_step	= 0x0f,
	.hs_right_step	= 0x0f,
	.hf_left_step	= 0x1d,
	.hf_right_step	= 0x1d,
};

static struct twl6040_vibra_data twl6040_vibra = {
	.vibldrv_res = 8,
	.vibrdrv_res = 3,
	.viblmotor_res = 10,
	.vibrmotor_res = 10,
	.vddvibl_uV = 0,	/* fixed volt supply - VBAT */
	.vddvibr_uV = 0,	/* fixed volt supply - VBAT */

	.max_timeout = 15000,
	.initial_vibrate = 0,
	.voltage_raise_speed = 0x26,
};

static struct twl6040_platform_data twl6040_data = {
	.codec		= &twl6040_codec,
	.vibra		= &twl6040_vibra,
	.audpwron_gpio	= 127,
};

static struct twl4030_platform_data tablet_twldata;

/*
 * The Clock Driver Chip (TCXO) on OMAP4 based SDP needs to
 * be programmed to output CLK1 based on REQ1 from OMAP.
 * By default CLK1 is driven based on an internal REQ1INT signal
 * which is always set to 1.
 * Doing this helps gate sysclk (from CLK1) to OMAP while OMAP
 * is in sleep states.
 */
static struct cdc_tcxo_platform_data tablet_cdc_data = {
	.buf = {
		CDC_TCXO_REQ4INT | CDC_TCXO_REQ1INT |
		CDC_TCXO_REQ4POL | CDC_TCXO_REQ3POL |
		CDC_TCXO_REQ2POL | CDC_TCXO_REQ1POL,
		CDC_TCXO_MREQ4 | CDC_TCXO_MREQ3 |
		CDC_TCXO_MREQ2 | CDC_TCXO_MREQ1,
		CDC_TCXO_LDOEN1,
		0,
	},
};

static struct i2c_board_info __initdata tablet_i2c_boardinfo[] = {
	{
		I2C_BOARD_INFO("cdc_tcxo_driver", 0x6c),
		.platform_data = &tablet_cdc_data,
	},
};

static void __init omap_i2c_hwspinlock_init(int bus_id, int spinlock_id,
				struct omap_i2c_bus_board_data *pdata)
{
	/* spinlock_id should be -1 for a generic lock request */
	if (spinlock_id < 0)
		pdata->handle = hwspin_lock_request(USE_MUTEX_LOCK);
	else
		pdata->handle = hwspin_lock_request_specific(spinlock_id,
							USE_MUTEX_LOCK);

	if (pdata->handle != NULL) {
		pdata->hwspin_lock_timeout = hwspin_lock_timeout;
		pdata->hwspin_unlock = hwspin_unlock;
	} else {
		pr_err("I2C hwspinlock request failed for bus %d\n", \
								bus_id);
	}
}

static struct omap_i2c_bus_board_data __initdata omap4_i2c_1_bus_pdata;
static struct omap_i2c_bus_board_data __initdata omap4_i2c_2_bus_pdata;
static struct omap_i2c_bus_board_data __initdata omap4_i2c_3_bus_pdata;
static struct omap_i2c_bus_board_data __initdata omap4_i2c_4_bus_pdata;

static int __init omap4_i2c_init(void)
{
	omap_i2c_hwspinlock_init(1, 0, &omap4_i2c_1_bus_pdata);
	omap_i2c_hwspinlock_init(2, 1, &omap4_i2c_2_bus_pdata);
	omap_i2c_hwspinlock_init(3, 2, &omap4_i2c_3_bus_pdata);
	omap_i2c_hwspinlock_init(4, 3, &omap4_i2c_4_bus_pdata);

	omap_register_i2c_bus_board_data(1, &omap4_i2c_1_bus_pdata);
	omap_register_i2c_bus_board_data(2, &omap4_i2c_2_bus_pdata);
	omap_register_i2c_bus_board_data(3, &omap4_i2c_3_bus_pdata);
	omap_register_i2c_bus_board_data(4, &omap4_i2c_4_bus_pdata);

	omap4_pmic_get_config(&tablet_twldata, TWL_COMMON_PDATA_USB |
			TWL_COMMON_PDATA_MADC,
			TWL_COMMON_REGULATOR_VDAC |
			TWL_COMMON_REGULATOR_VAUX1 |
			TWL_COMMON_REGULATOR_VAUX2 |
			TWL_COMMON_REGULATOR_VAUX3 |
			TWL_COMMON_REGULATOR_VMMC |
			TWL_COMMON_REGULATOR_VPP |
			TWL_COMMON_REGULATOR_VUSIM |
			TWL_COMMON_REGULATOR_VANA |
			TWL_COMMON_REGULATOR_VCXIO |
			TWL_COMMON_REGULATOR_VUSB |
			TWL_COMMON_REGULATOR_CLK32KG |
			TWL_COMMON_REGULATOR_V1V8 |
			TWL_COMMON_REGULATOR_V2V1 |
			TWL_COMMON_REGULATOR_SYSEN |
			TWL_COMMON_REGULATOR_CLK32KAUDIO |
			TWL_COMMON_REGULATOR_REGEN1);
	omap4_pmic_init("twl6030", &tablet_twldata,
			&twl6040_data, OMAP44XX_IRQ_SYS_2N);
	i2c_register_board_info(1, tablet_i2c_boardinfo,
				ARRAY_SIZE(tablet_i2c_boardinfo));
	omap_register_i2c_bus(2, 400, NULL, 0);
	omap_register_i2c_bus(3, 400, NULL, 0);
	omap_register_i2c_bus(4, 400, NULL, 0);
	/*
	 * Drive MSECURE high for TWL6030/6032 write access.
	 */
	omap_mux_init_signal("fref_clk0_out.gpio_wk6", OMAP_PIN_OUTPUT);
	gpio_request(6, "msecure");
	gpio_direction_output(6, 1);

	return 0;
}

#ifdef CONFIG_OMAP_MUX
static struct omap_board_mux board_mux[] __initdata = {
	OMAP4_MUX(USBB2_ULPITLL_CLK, OMAP_MUX_MODE3 | OMAP_PIN_OUTPUT),
	OMAP4_MUX(SYS_NIRQ2, OMAP_MUX_MODE0 | OMAP_PIN_INPUT_PULLUP |
				OMAP_PIN_OFF_WAKEUPENABLE),
	{ .reg_offset = OMAP_MUX_TERMINATOR },
};

#else
#define board_mux	NULL
#endif

#if defined(CONFIG_USB_EHCI_HCD_OMAP) || defined(CONFIG_USB_OHCI_HCD_OMAP3)
static const struct usbhs_omap_board_data usbhs_bdata __initconst = {
	.port_mode[0] = OMAP_EHCI_PORT_MODE_PHY,
	.port_mode[1] = OMAP_USBHS_PORT_MODE_UNUSED,
	.port_mode[2] = OMAP_USBHS_PORT_MODE_UNUSED,
	.phy_reset  = false,
	.reset_gpio_port[0]  = -EINVAL,
	.reset_gpio_port[1]  = -EINVAL,
	.reset_gpio_port[2]  = -EINVAL
};

static void __init omap4_ehci_ohci_init(void)
{
	omap_mux_init_signal("fref_clk3_req.gpio_wk30", \
		OMAP_PIN_OUTPUT | \
		OMAP_PIN_OFF_NONE | OMAP_PULL_ENA);

	/* Enable 5V,1A USB power on external HS-USB ports */
	if (gpio_is_valid(OMAP4_GPIO_WK30)) {
		gpio_request(OMAP4_GPIO_WK30, "USB POWER GPIO");
		gpio_direction_output(OMAP4_GPIO_WK30, 1);
		gpio_set_value(OMAP4_GPIO_WK30, 0);
	}

	omap_mux_init_signal("usbb2_ulpitll_clk.gpio_157", \
		OMAP_PIN_OUTPUT | \
		OMAP_PIN_OFF_NONE);

	/* Power on the ULPI PHY */
	if (gpio_is_valid(OMAP4_MDM_PWR_EN_GPIO)) {
		gpio_request(OMAP4_MDM_PWR_EN_GPIO, "USBB1 PHY VMDM_3V3");
		gpio_direction_output(OMAP4_MDM_PWR_EN_GPIO, 1);
	}

	usbhs_init(&usbhs_bdata);

	return;
}
#else
static void __init omap4_ehci_ohci_init(void){}
#endif

static void __init tablet_camera_mux_init(void)
{
	u32 r = 0;

	/* Enable CSI22 pads for 4460 and 4470 */
	if (cpu_is_omap446x() || cpu_is_omap447x()) {
		r = omap4_ctrl_pad_readl(OMAP4_CTRL_MODULE_PAD_CORE_CONTROL_CAMERA_RX);
		r |= (0x7 << OMAP4_CAMERARX_CSI22_LANEENABLE_SHIFT);
		omap4_ctrl_pad_writel(r,
		OMAP4_CTRL_MODULE_PAD_CORE_CONTROL_CAMERA_RX);

		omap_mux_init_signal("csi22_dx2.csi22_dx2",
				OMAP_PIN_INPUT | OMAP_MUX_MODE0);
		omap_mux_init_signal("csi22_dy2.csi22_dy2",
				OMAP_PIN_INPUT | OMAP_MUX_MODE0);
	}
}

static void __init omap_tablet_init(void)
{
	int status;
	int package = OMAP_PACKAGE_CBS;

#if defined(CONFIG_TI_EMIF) || defined(CONFIG_TI_EMIF_MODULE)
	if (cpu_is_omap447x()) {
		omap_emif_set_device_details(1, &lpddr2_elpida_4G_S4_info,
				lpddr2_elpida_4G_S4_timings,
				ARRAY_SIZE(lpddr2_elpida_4G_S4_timings),
				&lpddr2_elpida_S4_min_tck, NULL);
		omap_emif_set_device_details(2, &lpddr2_elpida_4G_S4_info,
				lpddr2_elpida_4G_S4_timings,
				ARRAY_SIZE(lpddr2_elpida_4G_S4_timings),
				&lpddr2_elpida_S4_min_tck, NULL);
	} else {
		omap_emif_set_device_details(1, &lpddr2_elpida_2G_S4_x2_info,
				lpddr2_elpida_2G_S4_timings,
				ARRAY_SIZE(lpddr2_elpida_2G_S4_timings),
				&lpddr2_elpida_S4_min_tck, NULL);
		omap_emif_set_device_details(2, &lpddr2_elpida_2G_S4_x2_info,
				lpddr2_elpida_2G_S4_timings,
				ARRAY_SIZE(lpddr2_elpida_2G_S4_timings),
				&lpddr2_elpida_S4_min_tck, NULL);
	}
#endif

	omap4_mux_init(board_mux, NULL, package);
	omap_create_board_props();
	omap4_i2c_init();
	platform_add_devices(tablet_devices, ARRAY_SIZE(tablet_devices));
	omap4_board_serial_init();
	omap_sdrc_init(NULL, NULL);
	omap4_twl6030_hsmmc_init(mmc);

	omap4_ehci_ohci_init();
	usb_musb_init(&musb_board_data);

	omap_init_dmm_tiler();
	omap4_register_ion();
	tablet_display_init();
	tablet_touch_init();
	tablet_camera_mux_init();
	tablet_sensor_init();
	tablet_button_init();
	omap4plus_connectivity_init(OMAP4_TABLET_2_0_ID);
	status = omap_ethernet_init();
	if (status) {
		pr_err("Ethernet initialization failed: %d\n", status);
	} else {
		tablet_spi_board_info[0].irq = gpio_to_irq(ETH_KS8851_IRQ);
		spi_register_board_info(tablet_spi_board_info,
				ARRAY_SIZE(tablet_spi_board_info));
	}

	if (cpu_is_omap446x()) {
		/* Vsel0 = gpio, vsel1 = gnd */
		status = omap_tps6236x_board_setup(true, TPS62361_GPIO, -1,
					OMAP_PIN_OFF_OUTPUT_HIGH, -1);
		if (status)
			pr_err("TPS62361 initialization failed: %d\n", status);
	}
}

static void __init omap_tablet_reserve(void)
{
	omap_ram_console_init(OMAP_RAM_CONSOLE_START_DEFAULT,
			OMAP_RAM_CONSOLE_SIZE_DEFAULT);
	omap_rproc_reserve_cma(RPROC_CMA_OMAP4);
	omap4_ion_init();
	omap_reserve();
}

MACHINE_START(OMAP_BLAZE, "OMAP4 Blaze board")
	.atag_offset	= 0x100,
	.reserve	= omap_tablet_reserve,
	.map_io		= omap4_map_io,
	.init_early	= omap4430_init_early,
	.init_irq	= gic_init_irq,
	.handle_irq	= gic_handle_irq,
	.init_machine	= omap_tablet_init,
	.timer		= &omap4_timer,
	.restart	= omap_prcm_restart,
MACHINE_END
