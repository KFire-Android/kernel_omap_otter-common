/*
 * Copyright (C) 2009 Texas Instruments Inc.
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/gpio.h>
#include <linux/delay.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/delay.h>

#include <mach/board-zoom.h>

#include <plat/common.h>
#include <plat/board.h>
#include <plat/usb.h>
#include <plat/opp_twl_tps.h>
#include <plat/timer-gp.h>

#include "mux.h"
#include "sdram-hynix-h8mbx00u0mer-0em.h"
#include "smartreflex-class3.h"
#include "board-zoom2-wifi.h"
#include <linux/skbuff.h>
#include <linux/ti_wilink_st.h>

#define McBSP3_BT_GPIO 164

#ifdef CONFIG_PM
static struct omap_volt_vc_data vc_config = {
	/* MPU */
	.vdd0_on	= 1200000, /* 1.2v */
	.vdd0_onlp	= 1000000, /* 1.0v */
	.vdd0_ret	=  975000, /* 0.975v */
	.vdd0_off	=  600000, /* 0.6v */
	/* CORE */
	.vdd1_on	= 1150000, /* 1.15v */
	.vdd1_onlp	= 1000000, /* 1.0v */
	.vdd1_ret	=  975000, /* 0.975v */
	.vdd1_off	=  600000, /* 0.6v */

	.clksetup	= 0xff,
	.voltoffset	= 0xff,
	.voltsetup2	= 0xff,
	.voltsetup_time1 = 0xfff,
	.voltsetup_time2 = 0xfff,
};

#ifdef CONFIG_TWL4030_CORE
static struct omap_volt_pmic_info omap_pmic_mpu = { /* and iva */
	.name = "twl",
	.slew_rate = 4000,
	.step_size = 12500,
	.i2c_addr = 0x12,
	.i2c_vreg = 0x0, /* (vdd0) VDD1 -> VDD1_CORE -> VDD_MPU */
	.vsel_to_uv = omap_twl_vsel_to_uv,
	.uv_to_vsel = omap_twl_uv_to_vsel,
	.onforce_cmd = omap_twl_onforce_cmd,
	.on_cmd = omap_twl_on_cmd,
	.sleepforce_cmd = omap_twl_sleepforce_cmd,
	.sleep_cmd = omap_twl_sleep_cmd,
	.vp_config_erroroffset = 0,
	.vp_vstepmin_vstepmin = 0x01,
	.vp_vstepmax_vstepmax = 0x04,
	.vp_vlimitto_timeout_us = 0x200,
	.vp_vlimitto_vddmin = 0x14,
	.vp_vlimitto_vddmax = 0x44,
};

static struct omap_volt_pmic_info omap_pmic_core = {
	.name = "twl",
	.slew_rate = 4000,
	.step_size = 12500,
	.i2c_addr = 0x12,
	.i2c_vreg = 0x1, /* (vdd1) VDD2 -> VDD2_CORE -> VDD_CORE */
	.vsel_to_uv = omap_twl_vsel_to_uv,
	.uv_to_vsel = omap_twl_uv_to_vsel,
	.onforce_cmd = omap_twl_onforce_cmd,
	.on_cmd = omap_twl_on_cmd,
	.sleepforce_cmd = omap_twl_sleepforce_cmd,
	.sleep_cmd = omap_twl_sleep_cmd,
	.vp_config_erroroffset = 0,
	.vp_vstepmin_vstepmin = 0x01,
	.vp_vstepmax_vstepmax = 0x04,
	.vp_vlimitto_timeout_us = 0x200,
	.vp_vlimitto_vddmin = 0x18,
	.vp_vlimitto_vddmax = 0x42,
};
#endif /* CONFIG_TWL4030_CORE */
#endif /* CONFIG_PM */

static void __init omap_zoom_map_io(void)
{
	omap2_set_globals_36xx();
	omap34xx_map_common_io();
}

static struct omap_board_config_kernel zoom_config[] __initdata = {
};

/*
 * Note! The addresses which appear in this table must agree with the
 * addresses in the U-Boot environment variables.
 */
static struct mtd_partition zoom_nand_partitions[] = {
	/* All the partition sizes are listed in terms of NAND block size */
	{
		.name           = "X-Loader-NAND",
		.offset         = 0,
		.size           = 4 * (64 * 2048),
		.mask_flags     = MTD_WRITEABLE,        /* force read-only */
	},
	{
		.name           = "U-Boot-NAND",
		.offset         = 0x0080000,
		.size           = 0x0180000, /* 1.5 M */
		.mask_flags     = MTD_WRITEABLE,        /* force read-only */
	},
	{
		.name           = "Boot Env-NAND",
		.offset         = 0x01C0000,
		.size           = 0x0040000,
	},
	{
		.name           = "Kernel-NAND",
		.offset         = 0x0200000,
		.size           = 0x1C00000,   /* 30M */
	},
#ifdef CONFIG_ANDROID
	{
		.name           = "system",
		.offset         = 0x2000000,
		.size           = 0xB400000,   /* 180M */
	},
	{
		.name           = "userdata",
		.offset         = 0xD400000,
		.size           = 0x4000000,    /* 64M */
	},
	{
		.name           = "cache",
		.offset         = 0x11400000,
		.size           = 0x2000000,    /* 32M */
	},
#endif
};

static struct flash_partitions zoom_flash_partitions[] = {
	{
		.parts = zoom_nand_partitions,
		.nr_parts = ARRAY_SIZE(zoom_nand_partitions),
	},
};

static void __init omap_zoom_init_irq(void)
{
	omap_board_config = zoom_config;
	omap_board_config_size = ARRAY_SIZE(zoom_config);
	omap2_init_common_hw(h8mbx00u0mer0em_sdrc_params,
			h8mbx00u0mer0em_sdrc_params);
	omap2_gp_clockevent_set_gptimer(1);
	omap_init_irq();
}

#ifdef CONFIG_OMAP_MUX
static struct omap_board_mux board_mux[] __initdata = {
	{ .reg_offset = OMAP_MUX_TERMINATOR },
};
#else
#define board_mux	NULL
#endif

static const struct usbhs_omap_platform_data usbhs_pdata __initconst = {
	.port_mode[0]		= OMAP_USBHS_PORT_MODE_UNUSED,
	.port_mode[1]		= OMAP_EHCI_PORT_MODE_PHY,
	.port_mode[2]		= OMAP_USBHS_PORT_MODE_UNUSED,
	.phy_reset		= true,
	.reset_gpio_port[0]	= -EINVAL,
	.reset_gpio_port[1]	= 64,
	.reset_gpio_port[2]	= -EINVAL,
};
int plat_kim_suspend(struct platform_device *pdev, pm_message_t state)
{
	/* TODO: wait for HCI-LL sleep */
	return 0;
}
int plat_kim_resume(struct platform_device *pdev)
{
	return 0;
}

/* wl128x BT, FM, GPS connectivity chip */
static int gpios[] = {109, 161, -1};

struct ti_st_plat_data wilink_pdata = {
	.nshutdown_gpio = 109,
	.dev_name = "/dev/ttyO1",
	.flow_cntrl = 1,
	.baud_rate = 3000000,
	.suspend = plat_kim_suspend,
	.resume = plat_kim_resume,
};
static struct platform_device wl127x_device = {
	.name           = "kim",
	.id             = -1,
	.dev.platform_data = &wilink_pdata,
};
static struct platform_device btwilink_device = {
	.name = "btwilink",
	.id = -1,
};

static struct platform_device *zoom_devices[] __initdata = {

	&wl127x_device,
	&btwilink_device,
};
/* Fix to prevent VIO leakage on wl127x */
static int wl127x_vio_leakage_fix(void)
{
	int ret = 0;

	pr_info(" wl127x_vio_leakage_fix\n");

	ret = gpio_request(gpios[0], "wl127x_bten");
	if (ret < 0) {
		pr_err("wl127x_bten gpio_%d request fail",
			gpios[0]);
		goto fail;
	}

	gpio_direction_output(gpios[0], 1);
	mdelay(10);
	gpio_direction_output(gpios[0], 0);
	udelay(64);

	gpio_free(gpios[0]);
fail:
	return ret;
}



static void __init omap_zoom_init(void)
{
	omap3_mux_init(board_mux, OMAP_PACKAGE_CBP);
	config_wlan_mux();

	zoom_peripherals_init();
	zoom_flash_init(zoom_flash_partitions, ZOOM_NAND_CS);
	zoom_debugboard_init();
	zoom_display_init(OMAP_DSS_VENC_TYPE_COMPOSITE);

	omap_mux_init_gpio(64, OMAP_PIN_OUTPUT);
	omap_mux_init_gpio(109, OMAP_PIN_OUTPUT);
	omap_mux_init_gpio(161, OMAP_PIN_OUTPUT);
	omap_mux_init_gpio(McBSP3_BT_GPIO, OMAP_PIN_OUTPUT);
	usb_uhhtll_init(&usbhs_pdata);
	sr_class3_init();

#ifdef CONFIG_PM
#ifdef CONFIG_TWL4030_CORE
	omap_voltage_register_pmic(&omap_pmic_core, "core");
	omap_voltage_register_pmic(&omap_pmic_mpu, "mpu");
#endif
	omap_voltage_init_vc(&vc_config);
#endif
	platform_add_devices(zoom_devices, ARRAY_SIZE(zoom_devices));
	wl127x_vio_leakage_fix();
}

MACHINE_START(OMAP_ZOOM3, "OMAP3630 Zoom3 board")
	.phys_io	= ZOOM_UART_BASE,
	.io_pg_offst	= (ZOOM_UART_VIRT >> 18) & 0xfffc,
	.boot_params	= 0x80000100,
	.map_io		= omap_zoom_map_io,
	.init_irq	= omap_zoom_init_irq,
	.init_machine	= omap_zoom_init,
	.timer		= &omap_timer,
MACHINE_END
