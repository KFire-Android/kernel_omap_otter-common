/*
 * imx046_regs.h
 *
 * Register definitions for the IMX046 Sensor.
 *
 * Leverage MT9P012.h
 *
 * Copyright (C) 2008 Hewlett Packard.
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2. This program is licensed "as is" without any warranty of any
 * kind, whether express or implied.
 */

#ifndef IMX046_REGS_H
#define IMX046_REGS_H

/* The ID values we are looking for */
#define IMX046_MOD_ID			0x0046
#define IMX046_MFR_ID			0x000B

/* IMX046 has 8/16/32 I2C registers */
#define I2C_8BIT			1
#define I2C_16BIT			2
#define I2C_32BIT			4

/* Terminating list entry for reg */
#define I2C_REG_TERM		0xFFFF
/* Terminating list entry for val */
#define I2C_VAL_TERM		0xFFFFFFFF
/* Terminating list entry for len */
#define I2C_LEN_TERM		0xFFFF

/* terminating token for reg list */
#define IMX046_TOK_TERM 		0xFF

/* delay token for reg list */
#define IMX046_TOK_DELAY		100

/* CSI2 Virtual ID */
#define IMX046_CSI2_VIRTUAL_ID	0x0

#define IMX046_CLKRC			0x11

/* Used registers */
#define IMX046_REG_MODEL_ID				0x0000
#define IMX046_REG_REV_NUMBER			0x0002
#define IMX046_REG_MFR_ID				0x0003

#define IMX046_REG_MODE_SELECT			0x0100
#define IMX046_REG_IMAGE_ORIENTATION	0x0101
#define IMX046_REG_SW_RESET				0x0103
#define IMX046_REG_GROUPED_PAR_HOLD		0x0104
#define IMX046_REG_CCP2_CHANNEL_ID		0x0110

#define IMX046_REG_FINE_INT_TIME		0x0200
#define IMX046_REG_COARSE_INT_TIME		0x0202

#define IMX046_REG_ANALOG_GAIN_GLOBAL	0x0204
#define IMX046_REG_ANALOG_GAIN_GREENR	0x0206
#define IMX046_REG_ANALOG_GAIN_RED		0x0208
#define IMX046_REG_ANALOG_GAIN_BLUE		0x020A
#define IMX046_REG_ANALOG_GAIN_GREENB	0x020C
#define IMX046_REG_DIGITAL_GAIN_GREENR	0x020E
#define IMX046_REG_DIGITAL_GAIN_RED		0x0210
#define IMX046_REG_DIGITAL_GAIN_BLUE	0x0212
#define IMX046_REG_DIGITAL_GAIN_GREENB	0x0214

#define IMX046_REG_VT_PIX_CLK_DIV		0x0300
#define IMX046_REG_VT_SYS_CLK_DIV		0x0302
#define IMX046_REG_PRE_PLL_CLK_DIV		0x0304
#define IMX046_REG_PLL_MULTIPLIER		0x0306
#define IMX046_REG_OP_PIX_CLK_DIV		0x0308
#define IMX046_REG_OP_SYS_CLK_DIV		0x030A

#define IMX046_REG_FRAME_LEN_LINES		0x0340
#define IMX046_REG_LINE_LEN_PCK			0x0342

#define IMX046_REG_X_ADDR_START			0x0344
#define IMX046_REG_Y_ADDR_START			0x0346
#define IMX046_REG_X_ADDR_END			0x0348
#define IMX046_REG_Y_ADDR_END			0x034A
#define IMX046_REG_X_OUTPUT_SIZE		0x034C
#define IMX046_REG_Y_OUTPUT_SIZE		0x034E
#define IMX046_REG_X_EVEN_INC			0x0380
#define IMX046_REG_X_ODD_INC			0x0382
#define IMX046_REG_Y_EVEN_INC			0x0384
#define IMX046_REG_Y_ODD_INC			0x0386

#define IMX046_REG_HMODEADD				0x3001
#define HMODEADD_SHIFT					7
#define HMODEADD_MASK  					(0x1 << HMODEADD_SHIFT)
#define IMX046_REG_OPB_CTRL				0x300C
#define IMX046_REG_Y_OPBADDR_START_DI	0x3014
#define IMX046_REG_Y_OPBADDR_END_DI		0x3015
#define IMX046_REG_PGACUR_VMODEADD		0x3016
#define VMODEADD_SHIFT					6
#define VMODEADD_MASK  					(0x1 << VMODEADD_SHIFT)
#define IMX046_REG_CHCODE_OUTCHSINGLE	0x3017
#define IMX046_REG_OUTIF				0x301C
#define IMX046_REG_RGPLTD_RGCLKEN		0x3022
#define RGPLTD_MASK						0x3
#define IMX046_REG_RGPOF_RGPOFV2		0x3031
#define IMX046_REG_CPCKAUTOEN			0x3040
#define IMX046_REG_RGCPVFB				0x3041
#define IMX046_REG_RGAZPDRV				0x3051
#define IMX046_REG_RGAZTEST				0x3053
#define IMX046_REG_RGVSUNLV				0x3055
#define IMX046_REG_CLPOWER				0x3060
#define IMX046_REG_CLPOWERSP			0x3065
#define IMX046_REG_ACLDIRV_TVADDCLP		0x30AA
#define IMX046_REG_TESTDI				0x30E5
#define IMX046_REG_HADDAVE				0x30E8
#define HADDAVE_SHIFT					7
#define HADDAVE_MASK  					(0x1 << HADDAVE_SHIFT)

#define IMX046_REG_RAW10CH2V2P_LO		0x31A4
#define IMX046_REG_RAW10CH2V2D_LO		0x31A6
#define IMX046_REG_COMP8CH1V2P_LO		0x31AC
#define IMX046_REG_COMP8CH1V2D_LO		0x31AE
#define IMX046_REG_RAW10CH1V2P_LO		0x31B4
#define IMX046_REG_RAW10CH1V2D_LO		0x31B6

#define IMX046_REG_RGOUTSEL1			0x3300
#define IMX046_REG_RGLANESEL			0x3301
#define IMX046_REG_RGTLPX 				0x3304
#define IMX046_REG_RGTCLKPREPARE 		0x3305
#define IMX046_REG_RGTCLKZERO 			0x3306
#define IMX046_REG_RGTCLKPRE 			0x3307
#define IMX046_REG_RGTCLKPOST 			0x3308
#define IMX046_REG_RGTCLKTRAIL 			0x3309
#define IMX046_REG_RGTHSEXIT 			0x330A
#define IMX046_REG_RGTHSPREPARE			0x330B
#define IMX046_REG_RGTHSZERO			0x330C
#define IMX046_REG_RGTHSTRAIL 			0x330D

#define IMX046_REG_TESBYPEN		0x30D8
#define IMX046_REG_TEST_PATT_MODE	0x0600
#define IMX046_REG_TEST_PATT_RED	0x0602
#define IMX046_REG_TEST_PATT_GREENR	0x0604
#define IMX046_REG_TEST_PATT_BLUE	0x0606
#define IMX046_REG_TEST_PATT_GREENB	0x0608

/*
 * The nominal xclk input frequency of the IMX046 is 18MHz, maximum
 * frequency is 45MHz, and minimum frequency is 6MHz.
 */
#define IMX046_XCLK_MIN   	6000000
#define IMX046_XCLK_MAX   	45000000
#define IMX046_XCLK_NOM_1 	18000000
#define IMX046_XCLK_NOM_2 	18000000

/* FPS Capabilities */
#define IMX046_MIN_FPS		7
#define IMX046_DEF_FPS		15
#define IMX046_MAX_FPS		30

#define I2C_RETRY_COUNT		5

/* Still capture 8 MP */
#define IMX046_IMAGE_WIDTH_MAX	3280
#define IMX046_IMAGE_HEIGHT_MAX	2464

/* Analog gain values */
#define IMX046_EV_MIN_GAIN		0
#define IMX046_EV_MAX_GAIN		30
#define IMX046_EV_DEF_GAIN		21
#define IMX046_EV_GAIN_STEP		1
/* maximum index in the gain EVT */
#define IMX046_EV_TABLE_GAIN_MAX	30

/* Exposure time values */
#define IMX046_MIN_EXPOSURE		250
#define IMX046_MAX_EXPOSURE		128000
#define IMX046_DEF_EXPOSURE	    33000
#define IMX046_EXPOSURE_STEP	50

/* Test Pattern Values */
#define IMX046_MIN_TEST_PATT_MODE	0
#define IMX046_MAX_TEST_PATT_MODE	4
#define IMX046_MODE_TEST_PATT_STEP	1

#define IMX046_TEST_PATT_SOLID_COLOR 	1
#define IMX046_TEST_PATT_COLOR_BAR	2
#define IMX046_TEST_PATT_PN9		4

#define IMX046_MAX_FRAME_LENGTH_LINES	0xFFFF

#define SENSOR_DETECTED		1
#define SENSOR_NOT_DETECTED	0

/**
 * struct imx046_reg - imx046 register format
 * @reg: 16-bit offset to register
 * @val: 8/16/32-bit register value
 * @length: length of the register
 *
 * Define a structure for IMX046 register initialization values
 */
struct imx046_reg {
	u16 	reg;
	u32 	val;
	u16	length;
};

/**
 * struct struct clk_settings - struct for storage of sensor
 * clock settings
 */
struct imx046_clk_settings {
	u16	pre_pll_div;
	u16	pll_mult;
	u16	post_pll_div;
	u16	vt_pix_clk_div;
	u16	vt_sys_clk_div;
};

/**
 * struct struct mipi_settings - struct for storage of sensor
 * mipi settings
 */
struct imx046_mipi_settings {
	u16	data_lanes;
	u16	ths_prepare;
	u16	ths_zero;
	u16	ths_settle_lower;
	u16	ths_settle_upper;
};

/**
 * struct struct frame_settings - struct for storage of sensor
 * frame settings
 */
struct imx046_frame_settings {
	u16	frame_len_lines_min;
	u16	frame_len_lines;
	u16	line_len_pck;
	u16	x_addr_start;
	u16	x_addr_end;
	u16	y_addr_start;
	u16	y_addr_end;
	u16	x_output_size;
	u16	y_output_size;
	u16	x_even_inc;
	u16	x_odd_inc;
	u16	y_even_inc;
	u16	y_odd_inc;
	u16	v_mode_add;
	u16	h_mode_add;
	u16	h_add_ave;
};

/**
 * struct struct imx046_sensor_settings - struct for storage of
 * sensor settings.
 */
struct imx046_sensor_settings {
	struct imx046_clk_settings clk;
	struct imx046_mipi_settings mipi;
	struct imx046_frame_settings frame;
};

/**
 * struct struct imx046_clock_freq - struct for storage of sensor
 * clock frequencies
 */
struct imx046_clock_freq {
	u32 vco_clk;
	u32 mipi_clk;
	u32 ddr_clk;
	u32 vt_pix_clk;
};

#endif /* ifndef IMX046_REGS_H */
