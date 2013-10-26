/*
 * arch/arm/mach-omap2/board-44xx-tablet-panel.c
 *
 * Copyright (C) 2011 Texas Instruments
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/leds-omap4430sdp-display.h>
#include <linux/platform_device.h>
#include <linux/omapfb.h>
#include <video/omapdss.h>
#include <video/omap-panel-generic.h>

#include <linux/i2c/twl.h>

#include <plat/android-display.h>
#include <plat/vram.h>
#include <plat/omap_apps_brd_id.h>

#include <plat/board-backlight.h>
#include "board-bowser.h"
#include "board-bowser-idme.h"

#if defined(CONFIG_BACKLIGHT_LP855X)
#include <linux/lp855x.h>
#endif

#ifdef CONFIG_PANEL_NT51012_LG
#include <video/omap-panel-nt51012.h>
#endif

#include "control.h"
#include "mux.h"

#define DP_4430_GPIO_59         59
#define LED_DISP_EN		102
#define DSI2_GPIO_59		59

#define HDMI_GPIO_CT_CP_HPD             60
#define HDMI_GPIO_HPD                   63  /* Hot plug pin for HDMI */

#define HDMI_GPIO_LS_OE 41 /* Level shifter for HDMI */
#define LCD_BL_GPIO		59 /*27*/	/* LCD Backlight GPIO */
/* PWM2 and TOGGLE3 register offsets */
#define LED_PWM2ON		0x03
#define LED_PWM2OFF		0x04
#define TWL6030_TOGGLE3		0x92

#define OMAP_HDMI_HPD_ADDR      0x4A100098
#define OMAP_HDMI_PULLTYPE_MASK 0x00000010

#define LED_SEC_DISP_GPIO 27

#define SYS_CLK                 38400000     /* same as ref clk which is 38.4M  */

#define GPIO_BACKLIGHT_EN_O2M         25  /* enabling O2Micro 9979 */
#define GPIO_BACKLIGHT_VOL_O2M        -1  /* no need to do for Jem */
#define GPIO_LCD_ENABLE               35  /* enabling LCD panel */
#define GPIO_LCD_ENABLE_FROM_EVT2     190 /* enabling LCD panel */
#define GPIO_D2L_RESET                40  /* D2L reset gpio */
#define GPIO_LED_TST                  41  /* LED_TST_GPIO41 */


#if defined (CONFIG_BACKLIGHT_LP855X)
#define GPIO_BACKLIGHT_EN_LP855X	(25)	/*Enable LP855x backlight*/
#define INITIAL_BRT			(0x64)
#define MAX_BRT				(0xFF)
#define LP8557_PLAT_CONFIG	(LP8557_COMB2_CONFIG | LP8557_PWM_FILTER)
#endif

#if defined(CONFIG_PANEL_NT71391_HYDIS)
#define OMAP4_BOWSER_LCD_FB_RAM_SIZE	(SZ_32M) /*1920*1200*4 * 4*/
#elif defined(CONFIG_PANEL_NT51012_LG)
#define OMAP4_BOWSER_LCD_FB_RAM_SIZE	(SZ_16M) /* 1280×800*4 * 4 */
#endif

#ifdef CONFIG_OMAP4_DSS_HDMI
#define OMAP4_BOWSER_HDMI_FB_RAM_SIZE	(20 * SZ_1M) /*1920*1080*4 * 2*/
#else
#define OMAP4_BOWSER_HDMI_FB_RAM_SIZE	(0)
#endif

#define OMAP4_BOWSER_FB_RAM_SIZE	(OMAP4_BOWSER_LCD_FB_RAM_SIZE + \
						OMAP4_BOWSER_HDMI_FB_RAM_SIZE)

#if defined (CONFIG_MACH_OMAP4_BOWSER_SUBTYPE_TATE)
#define GPIO_BACKLIGHT_CABC_EN        37  /* enabling backlight CABC (Content Adaptive Backlight Control) */
#elif defined(CONFIG_MACH_OMAP4_BOWSER_SUBTYPE_SOHO)
#define GPIO_BACKLIGHT_CABC_EN        35
#endif

#if 0
static struct gpio tablet_hdmi_gpios[] = {
	{HDMI_GPIO_CT_CP_HPD,  GPIOF_OUT_INIT_HIGH,    "hdmi_gpio_hpd"   },
	{HDMI_GPIO_LS_OE,      GPIOF_OUT_INIT_HIGH,    "hdmi_gpio_ls_oe" },
};
#endif

#if defined (CONFIG_PANEL_TC358765)
static struct panel_board_data bowser_dsi_panel = {
		.lcd_en_gpio = GPIO_LCD_ENABLE,
		.reset_gpio	= GPIO_D2L_RESET,
		.cabc_en_gpio = -1,
		.lcd_vsys_gpio = -1,
};
#elif defined (CONFIG_PANEL_NT71391_HYDIS)
static struct panel_board_data bowser_dsi_panel = {
		.lcd_en_gpio = GPIO_LCD_ENABLE,
		.reset_gpio = -1,
		.cabc_en_gpio = -1,
		.lcd_vsys_gpio = -1,
};
#elif defined (CONFIG_PANEL_NT51012_LG)
static struct panel_board_data bowser_dsi_panel = {
		.lcd_en_gpio = GPIO_LCD_ENABLE,
		.reset_gpio = -1,
		.lcd_vsys_gpio = -1,
		.cabc_en_gpio = GPIO_BACKLIGHT_CABC_EN,
};
#endif


#if defined (CONFIG_PANEL_TC358765)
static struct omap_dss_device bowser_lcd_device = {
	.name			= "lcd",
	.driver_name		= "tc358765",
	.type			= OMAP_DISPLAY_TYPE_DSI,
	.data			= &bowser_dsi_panel,
	.phy.dsi		= {
		.clk_lane	= 3,
		.clk_pol	= 0,
		.data1_lane	= 1,
		.data1_pol	= 0,
		.data2_lane	= 2,
		.data2_pol	= 0,
		.data3_lane	= 4,
		.data3_pol	= 0,
		.data4_lane	= 5,
		.data4_pol	= 0,
	},

#if defined (CONFIG_PANEL_SAMSUNG_LTL089CL01)
	.panel = {
		.acbi = 0,
		.acb = 40,
		.timings = {
			.x_res = 1920,
			.y_res = 1200,
		},
		.width_in_um = 191520,
		.height_in_um = 119700,
		.dsi_mode = OMAP_DSS_DSI_VIDEO_MODE,
	},
	.clocks = {
		.dispc = {
			 .channel = {
				.lck_div        = 1,	/* LCD */
				.pck_div        = 1,	/* PCD */
				.lcd_clk_src    = OMAP_DSS_CLK_SRC_DSI_PLL_HSDIV_DISPC,
			},
			.dispc_fclk_src = OMAP_DSS_CLK_SRC_FCK,
		},

		.dsi = {
			.regn           = 17,	/* DSI_PLL_REGN */
			.regm           = 287,	/* DSI_PLL_REGM */
			.regm_dispc     = 9,	/* PLL_CLK1 (M4) */
			.regm_dsi       = 7,	/* PLL_CLK2 (M5) */
			.lp_clk_div     = 19,	/* PLDIV */
			.offset_ddr_clk = 0,
			.dsi_fclk_src   = OMAP_DSS_CLK_SRC_DSI_PLL_HSDIV_DSI,
		},
	},
#else // For bowser LG panel
#error Need .panel timings, width & height_in_um
	.clocks = {
		.dispc = {
			 .channel = {
				.lck_div        = 1,	/* LCD */
				.pck_div        = 2,	/* PCD */
				.lcd_clk_src    = OMAP_DSS_CLK_SRC_DSI_PLL_HSDIV_DISPC,
			},
			.dispc_fclk_src = OMAP_DSS_CLK_SRC_FCK,
		},

		.dsi = {
			.regn           = 38,	/* DSI_PLL_REGN */
			.regm           = 394,	/* DSI_PLL_REGM */
			.regm_dispc     = 6,	/* PLL_CLK1 (M4) */
			.regm_dsi       = 9,	/* PLL_CLK2 (M5) */
			.lp_clk_div     = 5,	/* PLDIV */
			.offset_ddr_clk = 0,
			.dsi_fclk_src   = OMAP_DSS_CLK_SRC_DSI_PLL_HSDIV_DSI,
		},
	},

#endif
	.channel = OMAP_DSS_CHANNEL_LCD,
#ifdef CONFIG_FB_OMAP_BOOTLOADER_INIT
	.skip_init = true,
#else
	.skip_init = false,
#endif

	.platform_enable = NULL,
	.platform_disable = NULL,
};
#elif defined (CONFIG_PANEL_NT71391_HYDIS)
static struct omap_dss_device bowser_lcd_device = {
	.name			= "lcd",
	.driver_name		= "nt71391",
	.type			= OMAP_DISPLAY_TYPE_DSI,
	.data			= &bowser_dsi_panel,
	.phy.dsi		= {
		.clk_lane	= 3,
		.clk_pol	= 0,
		.data1_lane	= 1,
		.data1_pol	= 0,
		.data2_lane	= 2,
		.data2_pol	= 0,
		.data3_lane	= 4,
		.data3_pol	= 0,
		.data4_lane	= 5,
		.data4_pol	= 0,

		.module		= 0,
	},

	.panel = {
		.dsi_mode	= OMAP_DSS_DSI_VIDEO_MODE,
		.dsi_pix_fmt= OMAP_DSS_DSI_FMT_RGB666_PACKED,
		.acbi = 0,
		.acb = 40,
		.timings = {
			.x_res = 1920,
			.y_res = 1200,
		},
		.width_in_um = 191520,
		.height_in_um = 119700,
		.dsi_vm_data = {
			/* DSI video mode blanking data */
			/* Unit: byte clock cycles */
			.hsa				= 0,
			.hfp				= 27,
			.hbp				= 6,
			/* Unit: line clocks */
			.vsa				= 1,
			.vfp				= 10,
			.vbp				= 9,

			.ddr_clk_pre			= 39,
			.ddr_clk_post			= 17,

			/* DSI blanking modes */
			.blanking_mode			= 1,
			.hsa_blanking_mode		= 1,
			.hbp_blanking_mode		= 1,
			.hfp_blanking_mode		= 1,

			/* Video port sync events */
			.vp_de_pol			= 1,
			.vp_hsync_pol			= 0,
			.vp_vsync_pol			= 1,
			.vp_vsync_end			= 0,
			.vp_hsync_end			= 0,

			.ddr_clk_always_on		= 0,
			.window_sync			= 4,
		},
		.dsi_cio_data = {
			.ths_prepare			= 26,
			.ths_prepare_ths_zero		= 60,
			.ths_trail			= 25,
			.ths_exit			= 48,
			.tlpx_half			= 9,
			.tclk_trail			= 22,
			.tclk_zero			= 87,
			.tclk_prepare			= 22,
			.reg_ttaget			= 4,
		},
	},
	.clocks = {
		.dispc = {
			 .channel = {
				.lck_div        = 1,	/* LCD */
				.pck_div        = 1,	/* PCD */
				.lcd_clk_src    = OMAP_DSS_CLK_SRC_DSI_PLL_HSDIV_DISPC,
			},
			.dispc_fclk_src = OMAP_DSS_CLK_SRC_FCK,
		},

		.dsi = {
			.regn           = 16,	/* DSI_PLL_REGN */
			.regm           = 272,	/* DSI_PLL_REGM */
			.regm_dispc     = 9,	/* PLL_CLK1 (M4) */
			.regm_dsi       = 8,	/* PLL_CLK2 (M5) */
			.lp_clk_div     = 17,	/* PLDIV */
			.offset_ddr_clk = 0,
			.dsi_fclk_src   = OMAP_DSS_CLK_SRC_DSI_PLL_HSDIV_DSI,
		},
	},

	.channel = OMAP_DSS_CHANNEL_LCD,
#ifdef CONFIG_FB_OMAP_BOOTLOADER_INIT
	.skip_init = true,
#else
	.skip_init = false,
#endif

	.platform_enable = NULL,
	.platform_disable = NULL,
};
#elif defined (CONFIG_PANEL_NT51012_LG)
static struct omap_dss_device bowser_lcd_device = {
	.name			= "lcd",
	.driver_name		= "nt51012",
	.type			= OMAP_DISPLAY_TYPE_DSI,
	.data			= &bowser_dsi_panel,
	.phy.dsi		= {
		.clk_lane	= 1,
		.clk_pol	= 0,
		.data1_lane	= 2,
		.data1_pol	= 0,
		.data2_lane	= 3,
		.data2_pol	= 0,
		.data3_lane	= 4,
		.data3_pol	= 0,
		.data4_lane	= 5,
		.data4_pol	= 0,

		.module		= 0,
	},
	.panel = {
		.dsi_mode			= OMAP_DSS_DSI_VIDEO_MODE,
		.dsi_pix_fmt			= OMAP_DSS_DSI_FMT_RGB888,

		.timings = {
			.x_res = 800,
			.y_res = 1280,
		},
		.width_in_um = 94200,
		.height_in_um = 150720,

		.dsi_vm_data = {
			.hsa			= 0,
			.hfp			= 23,
			.hbp			= 58,
			/* unit: line clocks */
			.vsa			= 1,
			.vfp			= 10,
			.vbp			= 10,
			.ddr_clk_pre		= 31,
			.ddr_clk_post		= 14,

			/* DSI blanking modes */
			.blanking_mode		= 1,
			.hsa_blanking_mode	= 1,
			.hbp_blanking_mode	= 1,
			.hfp_blanking_mode	= 1,

			/* Video port sync event */
			.vp_de_pol		= 1,
			.vp_hsync_pol		= 0,
			.vp_vsync_pol		= 1,
			.vp_vsync_end		= 0,
			.vp_hsync_end		= 0,

			.ddr_clk_always_on	= 0,
			.window_sync		= 4,
		},
		/*From MR0*/
		.dsi_cio_data = {
			.ths_prepare		= 17,
			.ths_prepare_ths_zero	= 60, //enlarge to around 245ns
			.ths_trail		= 18,
			.ths_exit		= 31,
			.tlpx_half		= 6,
			.tclk_trail		= 15,
			.tclk_zero		= 65, //enlarge to around 330ns
			.tclk_prepare		= 14,
			.reg_ttaget		= 4,
		},
	},

	.clocks = {
		.dispc = {
			 .channel = {
				.lck_div        = 1,	/* LCD */
				.pck_div        = 2,	/* PCD */
				.lcd_clk_src    = OMAP_DSS_CLK_SRC_DSI_PLL_HSDIV_DISPC,
			},
			.dispc_fclk_src = OMAP_DSS_CLK_SRC_FCK,
		},

		.dsi = {
			.regn           = 16,	/* DSI_PLL_REGN */
			.regm           = 178,	/* DSI_PLL_REGM */
			.regm_dispc     = 6,	/* PLL_CLK1 (M4) */
			.regm_dsi       = 5,	/* PLL_CLK2 (M5) */
			.lp_clk_div     = 8,	/* PLDIV */
			.offset_ddr_clk = 0,
			.dsi_fclk_src   = OMAP_DSS_CLK_SRC_DSI_PLL_HSDIV_DSI,
		},
	},

	.channel = OMAP_DSS_CHANNEL_LCD,
#ifdef CONFIG_FB_OMAP_BOOTLOADER_INIT
	.skip_init = true,
#else
	.skip_init = false,
#endif
	.platform_enable = NULL,
	.platform_disable = NULL,
};

static struct nt51012_pmic_regs nt51012_pmic_enable_evt1[] = {
	NT51012_REGS(0x10, 0x01, 0x00, 0x00), /* reg[0x10] = 0x01 */
	NT51012_REGS(0xFF, 0xFF, 0xFF, 0xFF), /* Finish array */
};

static struct nt51012_pmic_regs nt51012_pmic_disable_evt1[] = {
	NT51012_REGS(0x0E, 0x00, 0x00, 0x00), /* r[0x0E] = 0x00 */
	NT51012_REGS(0x1F, 0x00, 0x80, 0x00), /* r[0x1F] = 0x00 */
	NT51012_REGS(0xFF, 0xFF, 0xFF, 0xFF), /* Finish array */
};

#if 0
static struct nt51012_pmic_regs nt51012_pmic_enable_evt2[] = {
	NT51012_REGS(0x1F, 0x00, 0x80, 0x00), /* r[0x1F] = r[0x1F] & ~0x80 */
	NT51012_REGS(0x10, 0x10, 0x00, 0x00), /* r[0x10] = 0x10 */
	NT51012_REGS(0xFF, 0xFF, 0xFF, 0xFF), /* Finish array   */
};
#endif

static struct nt51012_pmic_data nt51012_pmic_data = {
	.fw_enable = NULL,
	.fw_disable = NULL,
};

static struct i2c_board_info __initdata nt51012_pmic_i2c = {
	I2C_BOARD_INFO("nt51012_pmic", 0x4F),
	.platform_data = &nt51012_pmic_data,
};
#endif

#ifdef CONFIG_OMAP4_DSS_HDMI
static struct omap_dss_hdmi_data omap5panda_hdmi_data = {
	.hpd_gpio = HDMI_GPIO_HPD,
	.ct_cp_hpd_gpio = HDMI_GPIO_CT_CP_HPD,
	.ls_oe_gpio = HDMI_GPIO_LS_OE,
};

static struct omap_dss_device bowser_hdmi_device = {
	.name = "hdmi",
	.driver_name = "hdmi_panel",
	.type = OMAP_DISPLAY_TYPE_HDMI,
	.clocks	= {
		.dispc	= {
			.dispc_fclk_src	= OMAP_DSS_CLK_SRC_FCK,
		},
		.hdmi	= {
			.regn	= 15,
			.regm2	= 1,
		},
	},
	.hpd_gpio = HDMI_GPIO_HPD,
	.channel = OMAP_DSS_CHANNEL_DIGIT,
	.data = &omap5panda_hdmi_data,
};
#endif /* CONFIG_OMAP4_DSS_HDMI */

static struct omap_dss_device *bowser_dss_devices[] = {
	&bowser_lcd_device,
#ifdef CONFIG_OMAP4_DSS_HDMI
	&bowser_hdmi_device,
#endif /* CONFIG_OMAP4_DSS_HDMI */
};

static struct omap_dss_board_info bowser_dss_data = {
	.num_devices	=	ARRAY_SIZE(bowser_dss_devices),
	.devices	=	bowser_dss_devices,
	.default_device	=	&bowser_lcd_device,
};

static struct dsscomp_platform_data dsscomp_config_bowser = {
#if defined (CONFIG_PANEL_NT51012_LG)
	.tiler1d_slotsz = ( 24 * SZ_1M ),
#elif defined (CONFIG_PANEL_NT71391_HYDIS)
    .tiler1d_slotsz = ( 36 * SZ_1M ),
#else
	.tiler1d_slotsz = ( 16 * SZ_1M ),
#endif
};

static struct sgx_omaplfb_config omaplfb_config_bowser[] = {
	{
	.vram_buffers = 4,
	.swap_chain_length = 2,
	},
#if defined(CONFIG_OMAP4_DSS_HDMI)
	{
	.vram_buffers = 2,
	.swap_chain_length = 2,
	},
#endif
};

static struct sgx_omaplfb_platform_data omaplfb_plat_data_bowser = {
	.num_configs = ARRAY_SIZE(omaplfb_config_bowser),
	.configs = omaplfb_config_bowser,
};

static struct omapfb_platform_data bowser_fb_pdata = {
	.mem_desc = {
		.region_cnt = ARRAY_SIZE(omaplfb_config_bowser),
	},
};

#if defined (CONFIG_BACKLIGHT_LP855X)
static struct lp855x_rom_data lp8552_eeprom_arr[] = {
	{0xa1, 0xf0}, /* SLOPE=0 */
	{0xa5, 0x4f}, /* EN_VSYNC=0 */
};

static struct lp855x_rom_data lp8557_eeprom_arr[] = {
	{0x11, 0x05}, /* Set the maxcurr to 20ma */
	{0x14, 0xDF}, /* 4V OV, 5 LED string enabled */
};

static struct lp855x_platform_data lp8552_pdata = {
	.name = "lcd-backlight",
	.mode = REGISTER_BASED,
	.device_control = I2C_CONFIG(LP8552),
	.initial_brightness = INITIAL_BRT,
	.max_brightness = MAX_BRT,
	.load_new_rom_data = 1,
	.size_program = ARRAY_SIZE(lp8552_eeprom_arr),
	.rom_data = lp8552_eeprom_arr,
	.gpio_en = GPIO_BACKLIGHT_EN_LP855X,
};

static struct lp855x_platform_data lp8557_pdata = {
	.name = "bowser",
	.mode = REGISTER_BASED,
	.device_control = LP8557_PLAT_CONFIG,
	.initial_brightness = INITIAL_BRT,
	.max_brightness = MAX_BRT,
	.load_new_rom_data = 1,
	.size_program = ARRAY_SIZE(lp8557_eeprom_arr),
	.rom_data = lp8557_eeprom_arr,
	.gpio_en = GPIO_BACKLIGHT_EN_LP855X,
};

static struct i2c_board_info __initdata bl_lp8552_i2c_boardinfo[] = {
	{
	 I2C_BOARD_INFO("lp8552", 0x2C),
	 .platform_data = &lp8552_pdata,
	},
};

static struct i2c_board_info __initdata bl_lp8557_i2c_boardinfo[] = {
	{
	 I2C_BOARD_INFO("lp8557", 0x2C),
	 .platform_data = &lp8557_pdata,
	},
};

#else	/*CONFIG_BACKLIGHT_LP855X*/

static struct bowser_backlight_platform_data bowser_backlight_data = {
	.gpio_en_o2m = GPIO_BACKLIGHT_EN_O2M,
	.gpio_vol_o2m = GPIO_BACKLIGHT_VOL_O2M,
#if defined (CONFIG_MACH_OMAP4_BOWSER_SUBTYPE_TATE)
	.gpio_cabc_en = GPIO_BACKLIGHT_CABC_EN,
#else
	.gpio_cabc_en = -1,
#endif
	.timer       = 10,        /* use GPTimer 10 for backlight */
	.sysclk      = SYS_CLK,   /* input frequency to the timer, 38.4M */
	.pwmfreq     = 10000,     /* output freqeuncy from timer, 10k */
#if defined (CONFIG_MACH_OMAP4_BOWSER_SUBTYPE_TATE)
	.totalsteps  = 255,       /* how many backlight steps for the user, also the max brightness */
        .initialstep = 200        /* initial brightness */

#else
	.totalsteps  = 256,       /* how many backlight steps for the user, also the max brightness */
	.initialstep = 100        /* initial brightness */
#endif
};

static struct platform_device bowser_backlight_device = {
	.name   = "backlight",
	.id     = -1,
	.dev	= {
		.platform_data = &bowser_backlight_data,
	},
};

static struct platform_device *bowser_devices[] __initdata = {
	&bowser_backlight_device,
};

#endif  /*CONFIG_BACKLIGHT_LP855X*/

#if defined(CONFIG_FB_OMAP2_NUM_FBS)
#define OMAPLFB_NUM_DEV CONFIG_FB_OMAP2_NUM_FBS
#else
#define OMAPLFB_NUM_DEV 1
#endif


#ifdef CONFIG_OMAP4_DSS_HDMI
static void tablet_hdmi_mux_init(void)
{
	void __iomem *phymux_base = NULL;
	u32 val = 0xFFFFC000;
	u32 r;
	int status;
	/* PAD0_HDMI_HPD_PAD1_HDMI_CEC */
	omap_mux_init_signal("hdmi_hpd.hdmi_hpd",
				OMAP_PIN_INPUT_PULLDOWN);
	omap_mux_init_signal("gpmc_wait2.gpio_100",
			OMAP_PIN_INPUT_PULLDOWN);
	omap_mux_init_signal("hdmi_cec.hdmi_cec",
			OMAP_PIN_INPUT_PULLUP);
	/* PAD0_HDMI_DDC_SCL_PAD1_HDMI_DDC_SDA */
	omap_mux_init_signal("hdmi_ddc_scl.hdmi_ddc_scl",
			OMAP_PIN_INPUT_PULLUP);
	omap_mux_init_signal("hdmi_ddc_sda.hdmi_ddc_sda",
			OMAP_PIN_INPUT_PULLUP);

	/* strong pullup on DDC lines using unpublished register */
	r = ((1 << 24) | (1 << 28)) ;
	omap4_ctrl_pad_writel(r, OMAP4_CTRL_MODULE_PAD_CORE_CONTROL_I2C_1);

	phymux_base = ioremap(0x4A100000, 0x1000);
	gpio_request(HDMI_GPIO_HPD, NULL);
	omap_mux_init_gpio(HDMI_GPIO_HPD, OMAP_PIN_INPUT | OMAP_PULL_ENA);
	gpio_direction_input(HDMI_GPIO_HPD);

	/* This debouncing interval brings more stability where it manages the HPD interrupt response time */
	status = gpio_set_debounce(HDMI_GPIO_HPD, 5 * 1000);
	if (status)
		pr_err("%s: Cannot set debounce to hdmi_gpio %x \n", __func__, status);

	/* Turning on DSI PHY Mux*/
	__raw_writel(val, phymux_base + 0x618);

	if (idme_query_board_type(IDME_BOARD_TYPE_JEM_PVT_WIFI) ||
	    idme_query_board_type(IDME_BOARD_TYPE_JEM_PVT_WAN)) {
		/* Disabling TPD12S015's DCDC and it's level shifter during off-mode
		* by configuring off-mode state of HDMI_GPIO_CT_CP_HPD's and HDMI_GPIO_LS_OE's GPIOs*/
		omap_mux_init_signal("gpmc_nbe1.gpio_60", OMAP_PIN_OUTPUT | OMAP_PIN_OFF_OUTPUT_LOW);
		omap_mux_init_signal("gpmc_a17.gpio_41", OMAP_PIN_OUTPUT | OMAP_PIN_OFF_OUTPUT_LOW);
	}

#if 0
	/* Request HDMI_GPIO_CT_CP_HPD and HDMI_GPIO_LS_OE GPIOs */
	status = gpio_request_array(tablet_hdmi_gpios,
			ARRAY_SIZE(tablet_hdmi_gpios));
	if (status)
		pr_err("%s: Cannot request HDMI GPIOs %x \n", __func__, status);
#endif
	iounmap(phymux_base);
}
#endif /* CONFIG_OMAP4_DSS_HDMI */

extern int idme_get_board_type(void);
extern int idme_get_board_revision(void);

static void __init bowser_lcd_init(void)
{
	u32 reg;
	int status;

	/* Enable 5 lanes in DSI1 module, disable pull down */
	reg = omap4_ctrl_pad_readl(OMAP4_CTRL_MODULE_PAD_CORE_CONTROL_DSIPHY);
	reg &= ~OMAP4_DSI1_LANEENABLE_MASK;
	reg |= 0x1f << OMAP4_DSI1_LANEENABLE_SHIFT;
	reg &= ~OMAP4_DSI1_PIPD_MASK;
	reg |= 0x1f << OMAP4_DSI1_PIPD_SHIFT;
	omap4_ctrl_pad_writel(reg, OMAP4_CTRL_MODULE_PAD_CORE_CONTROL_DSIPHY);

#ifdef CONFIG_PANEL_NT51012_LG
	if (!(idme_query_board_type(IDME_BOARD_TYPE_SOHO_PROTO)) &&
	    !(idme_query_board_type(IDME_BOARD_TYPE_SOHO_PreEVT1)) &&
	    !(idme_query_board_type(IDME_BOARD_TYPE_SOHO_EVT1))) {
		bowser_dsi_panel.lcd_en_gpio = GPIO_LCD_ENABLE_FROM_EVT2;
	}
	if ((idme_get_board_type() == BOARD_TYPE_SOHO) &&
				(idme_get_board_revision() == 8)) {
		bowser_dsi_panel.lcd_vsys_gpio = GPIO_LED_TST;
		omap_mux_init_gpio(bowser_dsi_panel.lcd_vsys_gpio, OMAP_PIN_OUTPUT);
		status = gpio_request(bowser_dsi_panel.lcd_vsys_gpio, "lcd_vsys_gpio");
		if (status) {
			pr_err("%s: lcd_vsys_gpio request failed\n", __func__);
			bowser_dsi_panel.lcd_vsys_gpio = -1;
		} else {
			gpio_direction_output(bowser_dsi_panel.lcd_vsys_gpio, 1);
			printk("LCD_VSYS_GPIO success\n");
		}
	}
#endif

	/* LCD enable GPIO init */
	if(bowser_dsi_panel.lcd_en_gpio > 0)
	{
		omap_mux_init_gpio(bowser_dsi_panel.lcd_en_gpio, OMAP_PIN_OUTPUT);

		status = gpio_request(bowser_dsi_panel.lcd_en_gpio, "lcd_en_gpio");

		if (status) {
			pr_err("%s: lcd_en_gpio request failed\n", __func__);
			bowser_dsi_panel.lcd_en_gpio = -1;
		}
		gpio_direction_output(bowser_dsi_panel.lcd_en_gpio, 1);
		printk("LCD_EN_GPIO success\n");
	}

	/* D2L reset GPIO init */
	if(bowser_dsi_panel.reset_gpio > 0)
	{
		status = gpio_request(bowser_dsi_panel.reset_gpio, "d2l_reset_gpio");

		if (status)
			pr_err("%s: Could not get lcd_reset_gpio\n", __func__);

		gpio_direction_output(bowser_dsi_panel.reset_gpio, 0);
		printk("LCD_EN_GPIO success\n");
	}

#ifdef CONFIG_PANEL_NT51012_LG
	if (idme_query_board_type(IDME_BOARD_TYPE_SOHO_PreEVT1) ||
	    idme_query_board_type(IDME_BOARD_TYPE_SOHO_EVT1)) {
		nt51012_pmic_data.fw_enable = nt51012_pmic_enable_evt1;
		nt51012_pmic_data.fw_disable = nt51012_pmic_disable_evt1;
		i2c_register_board_info(2, &nt51012_pmic_i2c, 1);
	}
#endif
}

#if defined (CONFIG_BACKLIGHT_LP855X)
static void lp855x_backlight_init(void)
{
	omap_mux_init_gpio(GPIO_BACKLIGHT_EN_LP855X, OMAP_PIN_OUTPUT);

#if defined (CONFIG_MACH_OMAP4_BOWSER_SUBTYPE_TATE)
		i2c_register_board_info(2, bl_lp8552_i2c_boardinfo,
					ARRAY_SIZE(bl_lp8552_i2c_boardinfo));
#else
	if(idme_query_board_type(IDME_BOARD_TYPE_SOHO_PROTO))
		i2c_register_board_info(2, bl_lp8552_i2c_boardinfo,
					ARRAY_SIZE(bl_lp8552_i2c_boardinfo));
	else
		i2c_register_board_info(2, bl_lp8557_i2c_boardinfo,
					ARRAY_SIZE(bl_lp8557_i2c_boardinfo));
#endif
}

#else /*CONFIG_BACKLIGHT_LP855X*/
static void bowser_backlight_init(void)
{
	/* backlight gpio */
	if (GPIO_BACKLIGHT_EN_O2M > 0)
		omap_mux_init_gpio(GPIO_BACKLIGHT_EN_O2M, OMAP_PIN_OUTPUT);
	if (GPIO_BACKLIGHT_VOL_O2M > 0)
		omap_mux_init_gpio(GPIO_BACKLIGHT_VOL_O2M, OMAP_PIN_OUTPUT);
#if defined (CONFIG_MACH_OMAP4_BOWSER_SUBTYPE_TATE)
        if (GPIO_BACKLIGHT_CABC_EN >= 0)
               omap_mux_init_gpio(GPIO_BACKLIGHT_CABC_EN, OMAP_PIN_OUTPUT); 
#endif
}
#endif  /*CONFIG_BACKLIGHT_LP855X*/

void __init bowser_android_display_setup(struct omap_ion_platform_data *ion)
{
	omap_android_display_setup(&bowser_dss_data,
				   &dsscomp_config_bowser,
				   &omaplfb_plat_data_bowser,
				   &bowser_fb_pdata);
}

int __init bowser_panel_init(void)
{
#if defined (CONFIG_BACKLIGHT_LP855X)
	lp855x_backlight_init();
#else
	bowser_backlight_init();
#endif
	bowser_lcd_init();
#ifdef CONFIG_OMAP4_DSS_HDMI
	tablet_hdmi_mux_init();
#endif /* CONFIG_OMAP4_DSS_HDMI */

	omapfb_set_platform_data(&bowser_fb_pdata);
	omap_vram_set_sdram_vram(OMAP4_BOWSER_FB_RAM_SIZE, 0);

	omap_display_init(&bowser_dss_data);
#if defined (CONFIG_BACKLIGHT_BOWSER)
	platform_add_devices(bowser_devices, ARRAY_SIZE(bowser_devices));
#endif
	return 0;
}
