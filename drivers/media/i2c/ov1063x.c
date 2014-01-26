/*
 * OmniVision OV1063X Camera Driver
 *
 * Copyright (C) 2013 Phil Edworthy
 * Copyright (C) 2013 Renesas Electronics
 *
 * This driver has been tested at QVGA, VGA and 720p, and 1280x800 at up to
 * 30fps and it should work at any resolution in between and any frame rate
 * up to 30fps.
 *
 * FIXME:
 *  Horizontal flip (mirroring) does not work correctly. The image is flipped,
 *  but the colors are wrong.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/regulator/consumer.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/v4l2-mediabus.h>
#include <linux/videodev2.h>

#include <media/soc_camera.h>
#include <media/v4l2-async.h>
#include <media/v4l2-chip-ident.h>
#include <media/v4l2-common.h>
#include <media/v4l2-ctrls.h>

#include <linux/pm_runtime.h>

#include <linux/of_gpio.h>
#include <linux/of_i2c.h>
#include <linux/of_device.h>

/* Register definitions */
#define	OV1063X_VFLIP			0x381c
#define	 OV1063X_VFLIP_ON		(0x3 << 6)
#define	 OV1063X_VFLIP_SUBSAMPLE	0x1
#define	OV1063X_HMIRROR			0x381d
#define	 OV1063X_HMIRROR_ON		0x3
#define OV1063X_PID			0x300a
#define OV1063X_VER			0x300b

/* IDs */
#define OV10633_VERSION_REG		0xa630
#define OV10635_VERSION_REG		0xa635
#define OV1063X_VERSION(pid, ver)	(((pid) << 8) | ((ver) & 0xff))

#define OV1063X_SENSOR_WIDTH		1312
#define OV1063X_SENSOR_HEIGHT		814

#define OV1063X_MAX_WIDTH		1280
#define OV1063X_MAX_HEIGHT		800

struct ov1063x_color_format {
	enum v4l2_mbus_pixelcode code;
	enum v4l2_colorspace colorspace;
};

struct ov1063x_reg {
	u16	reg;
	u8	val;
};

struct ov1063x_priv {
	struct v4l2_subdev		subdev;
	struct v4l2_async_subdev	asd;
	struct v4l2_async_subdev_list	asdl;
	struct v4l2_ctrl_handler	hdl;
	int				power;
	int				model;
	int				revision;
	int				xvclk;
	int				fps_numerator;
	int				fps_denominator;
	const struct ov1063x_color_format	*cfmt;
	int				width;
	int				height;

	const char	*sensor_name;
	int			mux_gpio;
	int			mux1_sel0_gpio;
	int			mux1_sel1_gpio;
	int			mux2_sel0_gpio;
	int			mux2_sel1_gpio;
	int			cam_fpd_mux_s0_gpio;
	int			vin2_s0_gpio;
	int			ov_pwdn_gpio;
};

static const struct ov1063x_reg ov1063x_regs_default[] = {
	/* Register configuration for full resolution : 1280x720 */
		{0x103, 0x1},	 /** Software Reset */
		{0x301b, 0xff},  /** System Control Clock Reset #1 */
		{0x301c, 0xff},  /** System Control Clock Reset #2 */
		{0x301a, 0xff},  /** System Control Clock Reset #0 */
		{0x300c, 0x61},  /** Serial Camera Control Bus ID */
		{0x300c, 0x61},
		{0x300c, 0x61},
		{0x300c, 0x61},
		{0x300c, 0x61},
		{0x300c, 0x61},
		{0x300c, 0x61},
		{0x300c, 0x61},
		{0x300c, 0x61},
		{0x300c, 0x61},
		{0x300c, 0x61},
		{0x300c, 0x61},
		{0x300c, 0x61},
		{0x300c, 0x61},
		{0x300c, 0x61},
		{0x300c, 0x61},
		{0x300c, 0x61},
		{0x300c, 0x61},
		{0x300c, 0x61},
		{0x300c, 0x61},
		{0x300c, 0x61},
		{0x300c, 0x61},
		{0x300c, 0x61},
		{0x3021, 0x3},  /** System Control Misc */
		{0x3011, 0x2},
		{0x6900, 0xc},
		{0x6901, 0x1},
		{0x3033, 0x8},  /** System clock/4 */
		{0x3503, 0x10}, /** AEC Delay enabled */
		{0x302d, 0x2f}, /** Power Down Control */
		{0x3025, 0x3},  /** Debug Control enabled */
		/*
		 * FPS is computed as:
		 * XVCLK	   = 9MHz
		 * preDivide   = {/1,/1.5,/2,/3,/4,/5,/6,/7}
		 * decided based on (0x3004[6:4].
		 * numerator   = (0x3003[5:0] * XVCLK)/preDivide)
		 * denominator = (2 * (1+0x3004[2:0]))
		 * FPS = numerator/denominator.
		 */
		/* {0x3003, 0x1B}, */ /* fps = 30fps. */
		/* {0x3004, 0x03}, */
		{0x3003, 0x28}, /* fps = 15fps. */
		{0x3004, 0x31},
		{0x3005, 0x20},
		{0x3006, 0x91},
		{0x3600, 0x74},
		{0x3601, 0x2b},
		{0x3612, 0x0},
		{0x3611, 0x67},
		{0x3633, 0xba},
		{0x3602, 0x2f},
		{0x3603, 0x0},
		{0x3630, 0xa8},
		{0x3631, 0x16},
		{0x3714, 0x10},
		{0x371d, 0x1},
		{0x4300, 0x3A}, /* UYVY mode */
		{0x3007, 0x1},
		/*
		 * RAW mode and Pixel CLK selct.
		 * 0x3024[0] = 1 :: System CLK (0x3003,0x3004)
		 * 0x3024[0] = 0 :: secondary CLK (0x3005,0x3006)
		 */
		{0x3024, 0x1},
		{0x3020, 0xb},
		{0x3702, 0xd},
		{0x3703, 0x20},
		{0x3704, 0x15},
		{0x3709, 0x28},
		{0x370d, 0x0},
		{0x3712, 0x0},
		{0x3713, 0x20},
		{0x3715, 0x4},
		{0x381d, 0x40},
		{0x381c, 0x0},
		{0x3824, 0x10},
		{0x3815, 0x8c},
		{0x3804, 0x5},
		{0x3805, 0x1f},
		{0x3800, 0x0},
		{0x3801, 0x0},
		{0x3806, 0x3},
		{0x3807, 0x1},
		{0x3802, 0x0},
		{0x3803, 0x2e},
		{0x3808, 0x5},
		{0x3809, 0x0},
		{0x380a, 0x2},
		{0x380b, 0xd0},
		{0x380c, 0x6},
		{0x380d, 0xf6}, /* 1280x720 */
		{0x380e, 0x2},
		{0x380f, 0xec}, /* 1280x720 */
		{0x3811, 0x8},
		{0x381f, 0xc},
		{0x3621, 0x63},
		{0x5005, 0x8},
		{0x56d5, 0x0},
		{0x56d6, 0x80},
		{0x56d7, 0x0},
		{0x56d8, 0x0},
		{0x56d9, 0x0},
		{0x56da, 0x80},
		{0x56db, 0x0},
		{0x56dc, 0x0},
		{0x56e8, 0x0},
		{0x56e9, 0x7f},
		{0x56ea, 0x0},
		{0x56eb, 0x7f},
		{0x5100, 0x0},
		{0x5101, 0x80},
		{0x5102, 0x0},
		{0x5103, 0x80},
		{0x5104, 0x0},
		{0x5105, 0x80},
		{0x5106, 0x0},
		{0x5107, 0x80},
		{0x5108, 0x0},
		{0x5109, 0x0},
		{0x510a, 0x0},
		{0x510b, 0x0},
		{0x510c, 0x0},
		{0x510d, 0x0},
		{0x510e, 0x0},
		{0x510f, 0x0},
		{0x5110, 0x0},
		{0x5111, 0x80},
		{0x5112, 0x0},
		{0x5113, 0x80},
		{0x5114, 0x0},
		{0x5115, 0x80},
		{0x5116, 0x0},
		{0x5117, 0x80},
		{0x5118, 0x0},
		{0x5119, 0x0},
		{0x511a, 0x0},
		{0x511b, 0x0},
		{0x511c, 0x0},
		{0x511d, 0x0},
		{0x511e, 0x0},
		{0x511f, 0x0},
		{0x56d0, 0x0},
		{0x5006, 0x24},
		{0x5608, 0x0},
		{0x52d7, 0x6},
		{0x528d, 0x8},
		{0x5293, 0x12},
		{0x52d3, 0x12},
		{0x5288, 0x6},
		{0x5289, 0x20},
		{0x52c8, 0x6},
		{0x52c9, 0x20},
		{0x52cd, 0x4},
		{0x5381, 0x0},
		{0x5382, 0xff},
		{0x5589, 0x76},
		{0x558a, 0x47},
		{0x558b, 0xef},
		{0x558c, 0xc9},
		{0x558d, 0x49},
		{0x558e, 0x30},
		{0x558f, 0x67},
		{0x5590, 0x3f},
		{0x5591, 0xf0},
		{0x5592, 0x10},
		{0x55a2, 0x6d},
		{0x55a3, 0x55},
		{0x55a4, 0xc3},
		{0x55a5, 0xb5},
		{0x55a6, 0x43},
		{0x55a7, 0x38},
		{0x55a8, 0x5f},
		{0x55a9, 0x4b},
		{0x55aa, 0xf0},
		{0x55ab, 0x10},
		{0x5581, 0x52},
		{0x5300, 0x1},
		{0x5301, 0x0},
		{0x5302, 0x0},
		{0x5303, 0xe},
		{0x5304, 0x0},
		{0x5305, 0xe},
		{0x5306, 0x0},
		{0x5307, 0x36},
		{0x5308, 0x0},
		{0x5309, 0xd9},
		{0x530a, 0x0},
		{0x530b, 0xf},
		{0x530c, 0x0},
		{0x530d, 0x2c},
		{0x530e, 0x0},
		{0x530f, 0x59},
		{0x5310, 0x0},
		{0x5311, 0x7b},
		{0x5312, 0x0},
		{0x5313, 0x22},
		{0x5314, 0x0},
		{0x5315, 0xd5},
		{0x5316, 0x0},
		{0x5317, 0x13},
		{0x5318, 0x0},
		{0x5319, 0x18},
		{0x531a, 0x0},
		{0x531b, 0x26},
		{0x531c, 0x0},
		{0x531d, 0xdc},
		{0x531e, 0x0},
		{0x531f, 0x2},
		{0x5320, 0x0},
		{0x5321, 0x24},
		{0x5322, 0x0},
		{0x5323, 0x56},
		{0x5324, 0x0},
		{0x5325, 0x85},
		{0x5326, 0x0},
		{0x5327, 0x20},
		{0x5609, 0x1},
		{0x560a, 0x40},
		{0x560b, 0x1},
		{0x560c, 0x40},
		{0x560d, 0x0},
		{0x560e, 0xfa},
		{0x560f, 0x0},
		{0x5610, 0xfa},
		{0x5611, 0x2},
		{0x5612, 0x80},
		{0x5613, 0x2},
		{0x5614, 0x80},
		{0x5615, 0x1},
		{0x5616, 0x2c},
		{0x5617, 0x1},
		{0x5618, 0x2c},
		{0x563b, 0x1},
		{0x563c, 0x1},
		{0x563d, 0x1},
		{0x563e, 0x1},
		{0x563f, 0x3},
		{0x5640, 0x3},
		{0x5641, 0x3},
		{0x5642, 0x5},
		{0x5643, 0x9},
		{0x5644, 0x5},
		{0x5645, 0x5},
		{0x5646, 0x5},
		{0x5647, 0x5},
		{0x5651, 0x0},
		{0x5652, 0x80},
		{0x521a, 0x1},
		{0x521b, 0x3},
		{0x521c, 0x6},
		{0x521d, 0xa},
		{0x521e, 0xe},
		{0x521f, 0x12},
		{0x5220, 0x16},
		{0x5223, 0x2},
		{0x5225, 0x4},
		{0x5227, 0x8},
		{0x5229, 0xc},
		{0x522b, 0x12},
		{0x522d, 0x18},
		{0x522f, 0x1e},
		{0x5241, 0x4},
		{0x5242, 0x1},
		{0x5243, 0x3},
		{0x5244, 0x6},
		{0x5245, 0xa},
		{0x5246, 0xe},
		{0x5247, 0x12},
		{0x5248, 0x16},
		{0x524a, 0x3},
		{0x524c, 0x4},
		{0x524e, 0x8},
		{0x5250, 0xc},
		{0x5252, 0x12},
		{0x5254, 0x18},
		{0x5256, 0x1e},
		{0x4605, 0x00}, /* 8-bit YUV mode. */
		{0x4606, 0x7},
		{0x4607, 0x71},
		{0x460a, 0x2},
		{0x460b, 0x70},
		{0x460c, 0x0},
		{0x4620, 0xe},
		{0x4700, 0x4},
		{0x4701, 0x01},
		/* {0x4702, 0x1}, */
		{0x4702, 0x00},  /* 01 */
		{0x4703, 0x00},
		{0x4704, 0x00}, /* 01 */
		/*
		 * Non-overlapping HSYNC-VSYNC.
		 * Therefore do not set the VSYNC delay registers.
		 */
		{0x4705, 0x00}, /* Vsync delay high byte */
		{0x4706, 0x00}, /* Vsync delay middle byte */
		{0x4707, 0x00}, /* Vsync delay low byte{0x4708, 0x1}, */
		/* {0x4709, 0x50}, */
		{0x4004, 0x8},
		{0x4005, 0x18},
		{0x4001, 0x4},
		{0x4050, 0x20},
		{0x4051, 0x22},
		{0x4057, 0x9c},
		{0x405a, 0x0},
		{0x4202, 0x2},
		{0x3023, 0x10},
		{0x100, 0x1},
		{0x100, 0x1},
		{0x6f0e, 0x0},
		{0x6f0f, 0x0},
		{0x460e, 0x8},
		{0x460f, 0x1},
		{0x4610, 0x0},
		{0x4611, 0x1},
		{0x4612, 0x0},
		{0x4613, 0x1},
		{0x4605, 0x00},
		{0x4608, 0x0},
		{0x4609, 0x8},
		{0x6804, 0x0},
		{0x6805, 0x6},
		{0x6806, 0x0},
		{0x5120, 0x0},
		{0x3510, 0x0},
		{0x3504, 0x0},
		{0x6800, 0x0},
		{0x6f0d, 0x0},
		{0x5000, 0xff},
		{0x5001, 0xbf},
		{0x5002, 0xfe},
		{0x503d, 0x0},
		/* {0x503e, 0x00}, */
		{0xc450, 0x1},
		{0xc452, 0x4},
		{0xc453, 0x0},
		{0xc454, 0x0},
		{0xc455, 0x0},
		{0xc456, 0x0},
		{0xc457, 0x0},
		{0xc458, 0x0},
		{0xc459, 0x0},
		{0xc45b, 0x0},
		{0xc45c, 0x0},
		{0xc45d, 0x0},
		{0xc45e, 0x0},
		{0xc45f, 0x0},
		{0xc460, 0x0},
		{0xc461, 0x1},
		{0xc462, 0x1},
		{0xc464, 0x88},
		{0xc465, 0x0},
		{0xc466, 0x8a},
		{0xc467, 0x0},
		{0xc468, 0x86},
		{0xc469, 0x0},
		{0xc46a, 0x40},
		{0xc46b, 0x50},
		{0xc46c, 0x30},
		{0xc46d, 0x28},
		{0xc46e, 0x60},
		{0xc46f, 0x40},
		{0xc47c, 0x1},
		{0xc47d, 0x38},
		{0xc47e, 0x0},
		{0xc47f, 0x0},
		{0xc480, 0x0},
		{0xc481, 0xff},
		{0xc482, 0x0},
		{0xc483, 0x40},
		{0xc484, 0x0},
		{0xc485, 0x18},
		{0xc486, 0x0},
		{0xc487, 0x18},
		{0xc488, 0x2e},
		{0xc489, 0x80},
		{0xc48a, 0x2e},
		{0xc48b, 0x80},
		{0xc48c, 0x0},
		{0xc48d, 0x4},
		{0xc48e, 0x0},
		{0xc48f, 0x4},
		{0xc490, 0x7},
		{0xc492, 0x20},
		{0xc493, 0x8},
		{0xc498, 0x2},
		{0xc499, 0x0},
		{0xc49a, 0x2},
		{0xc49b, 0x0},
		{0xc49c, 0x2},
		{0xc49d, 0x0},
		{0xc49e, 0x2},
		{0xc49f, 0x60},
		{0xc4a0, 0x4},
		{0xc4a1, 0x0},
		{0xc4a2, 0x6},
		{0xc4a3, 0x0},
		{0xc4a4, 0x0},
		{0xc4a5, 0x10},
		{0xc4a6, 0x0},
		{0xc4a7, 0x40},
		{0xc4a8, 0x0},
		{0xc4a9, 0x80},
		{0xc4aa, 0xd},
		{0xc4ab, 0x0},
		{0xc4ac, 0xf},
		{0xc4ad, 0xc0},
		{0xc4b4, 0x1},
		{0xc4b5, 0x1},
		{0xc4b6, 0x0},
		{0xc4b7, 0x1},
		{0xc4b8, 0x0},
		{0xc4b9, 0x1},
		{0xc4ba, 0x1},
		{0xc4bb, 0x0},
		{0xc4be, 0x2},
		{0xc4bf, 0x33},
		{0xc4c8, 0x3},
		{0xc4c9, 0xd0},
		{0xc4ca, 0xe},
		{0xc4cb, 0x0},
		{0xc4cc, 0xe},
		{0xc4cd, 0x51},
		{0xc4ce, 0xe},
		{0xc4cf, 0x51},
		{0xc4d0, 0x4},
		{0xc4d1, 0x80},
		{0xc4e0, 0x4},
		{0xc4e1, 0x2},
		{0xc4e2, 0x1},
		{0xc4e4, 0x10},
		{0xc4e5, 0x20},
		{0xc4e6, 0x30},
		{0xc4e7, 0x40},
		{0xc4e8, 0x50},
		{0xc4e9, 0x60},
		{0xc4ea, 0x70},
		{0xc4eb, 0x80},
		{0xc4ec, 0x90},
		{0xc4ed, 0xa0},
		{0xc4ee, 0xb0},
		{0xc4ef, 0xc0},
		{0xc4f0, 0xd0},
		{0xc4f1, 0xe0},
		{0xc4f2, 0xf0},
		{0xc4f3, 0x80},
		{0xc4f4, 0x0},
		{0xc4f5, 0x20},
		{0xc4f6, 0x2},
		{0xc4f7, 0x0},
		{0xc4f8, 0x4},
		{0xc4f9, 0xb},
		{0xc4fa, 0x0},
		{0xc4fb, 0x1},
		{0xc4fc, 0x1},
		{0xc4fd, 0x1},
		{0xc4fe, 0x4},
		{0xc4ff, 0x2},
		{0xc500, 0x68},
		{0xc501, 0x74},
		{0xc502, 0x70},
		{0xc503, 0x80},
		{0xc504, 0x5},
		{0xc505, 0x80},
		{0xc506, 0x3},
		{0xc507, 0x80},
		{0xc508, 0x1},
		{0xc509, 0xc0},
		{0xc50a, 0x1},
		{0xc50b, 0xa0},
		{0xc50c, 0x1},
		{0xc50d, 0x2c},
		{0xc50e, 0x1},
		{0xc50f, 0xa},
		{0xc510, 0x0},
		{0xc511, 0x0},
		{0xc512, 0xe5},
		{0xc513, 0x14},
		{0xc514, 0x4},
		{0xc515, 0x0},
		{0xc518, 0x3},
		{0xc519, 0x48},
		{0xc51a, 0x7},
		{0xc51b, 0x70},
		{0xc2e0, 0x0},
		{0xc2e1, 0x51},
		{0xc2e2, 0x0},
		{0xc2e3, 0xd6},
		{0xc2e4, 0x1},
		{0xc2e5, 0x5e},
		{0xc2e9, 0x1},
		{0xc2ea, 0x7a},
		{0xc2eb, 0x90},
		{0xc2ed, 0x1},
		{0xc2ee, 0x7a},
		{0xc2ef, 0x64},
		{0xc308, 0x0},
		{0xc309, 0x0},
		{0xc30a, 0x0},
		{0xc30c, 0x0},
		{0xc30d, 0x1},
		{0xc30e, 0x0},
		{0xc30f, 0x0},
		{0xc310, 0x1},
		{0xc311, 0x60},
		{0xc312, 0xff},
		{0xc313, 0x8},
		{0xc314, 0x1},
		{0xc315, 0x7f},
		{0xc316, 0xff},
		{0xc317, 0xb},
		{0xc318, 0x0},
		{0xc319, 0xc},
		{0xc31a, 0x0},
		{0xc31b, 0xe0},
		{0xc31c, 0x0},
		{0xc31d, 0x14},
		{0xc31e, 0x0},
		{0xc31f, 0xc5},
		{0xc320, 0xff},
		{0xc321, 0x4b},
		{0xc322, 0xff},
		{0xc323, 0xf0},
		{0xc324, 0xff},
		{0xc325, 0xe8},
		{0xc326, 0x0},
		{0xc327, 0x46},
		{0xc328, 0xff},
		{0xc329, 0xd2},
		{0xc32a, 0xff},
		{0xc32b, 0xe4},
		{0xc32c, 0xff},
		{0xc32d, 0xbb},
		{0xc32e, 0x0},
		{0xc32f, 0x61},
		{0xc330, 0xff},
		{0xc331, 0xf9},
		{0xc332, 0x0},
		{0xc333, 0xd9},
		{0xc334, 0x0},
		{0xc335, 0x2e},
		{0xc336, 0x0},
		{0xc337, 0xb1},
		{0xc338, 0xff},
		{0xc339, 0x64},
		{0xc33a, 0xff},
		{0xc33b, 0xeb},
		{0xc33c, 0xff},
		{0xc33d, 0xe8},
		{0xc33e, 0x0},
		{0xc33f, 0x48},
		{0xc340, 0xff},
		{0xc341, 0xd0},
		{0xc342, 0xff},
		{0xc343, 0xed},
		{0xc344, 0xff},
		{0xc345, 0xad},
		{0xc346, 0x0},
		{0xc347, 0x66},
		{0xc348, 0x1},
		{0xc349, 0x0},
		{0x6700, 0x4},
		{0x6701, 0x7b},
		{0x6702, 0xfd},
		{0x6703, 0xf9},
		{0x6704, 0x3d},
		{0x6705, 0x71},
		/*
		 * 0x6706[3:0] :: XVCLK
		 * 0x6706[3:0] :: 0 = 6MHz
		 * 0x6706[3:0] :: 1 = 9MHz
		 * 0x6706[3:0] :: 8 = 24MHz
		 * 0x6706[3:0] :: 9 = 27MHz
		 */
		{0x6706, 0x71},
		{0x6708, 0x5},
		{0x3822, 0x50},
		{0x6f06, 0x6f},
		{0x6f07, 0x0},
		{0x6f0a, 0x6f},
		{0x6f0b, 0x0},
		{0x6f00, 0x3},
		{0x3042, 0xf0},
		{0x3042, 0xf0},
		{0x3042, 0xf0},
		{0x3042, 0xf0},
		{0x3042, 0xf0},
		{0x3042, 0xf0},
		{0x3042, 0xf0},
		{0x3042, 0xf0},
		{0x3042, 0xf0},
		{0x3042, 0xf0},
		{0x3042, 0xf0},
		{0x3042, 0xf0},
		{0x3042, 0xf0},
		{0x3042, 0xf0},
		{0x3042, 0xf0},
		{0x3042, 0xf0},
		{0x3042, 0xf0},
		{0x3042, 0xf0},
		{0x3042, 0xf0},
		{0x3042, 0xf0},
		{0x3042, 0xf0},
		{0x3042, 0xf0},
		{0x3042, 0xf0},
		{0x3042, 0xf0},
		{0x3042, 0xf0},
		{0x3042, 0xf0},
		{0x301b, 0xf0},
		{0x301c, 0xf0},
		{0x301a, 0xf0},
};

static const struct ov1063x_reg ov1063x_regs_change_mode[] = {
	{ 0x301b, 0xff }, { 0x301c, 0xff }, { 0x301a, 0xff }, { 0x5005, 0x08 },
	{ 0x3007, 0x01 }, { 0x381c, 0x00 }, { 0x381f, 0x0C }, { 0x4001, 0x04 },
	{ 0x4004, 0x08 }, { 0x4050, 0x20 }, { 0x4051, 0x22 }, { 0x6e47, 0x0C },
	{ 0x4610, 0x05 }, { 0x4613, 0x10 },
};

static const struct ov1063x_reg ov1063x_regs_bt656[] = {
	{ 0x4700, 0x02 }, { 0x4302, 0x03 }, { 0x4303, 0xf8 }, { 0x4304, 0x00 },
	{ 0x4305, 0x08 }, { 0x4306, 0x03 }, { 0x4307, 0xf8 }, { 0x4308, 0x00 },
	{ 0x4309, 0x08 },
};

static const struct ov1063x_reg ov1063x_regs_bt656_10bit[] = {
	{ 0x4700, 0x02 }, { 0x4302, 0x03 }, { 0x4303, 0xfe }, { 0x4304, 0x00 },
	{ 0x4305, 0x02 }, { 0x4306, 0x03 }, { 0x4307, 0xfe }, { 0x4308, 0x00 },
	{ 0x4309, 0x02 },
};

static const struct ov1063x_reg ov1063x_regs_vert_sub_sample[] = {
	{ 0x381f, 0x06 }, { 0x4001, 0x02 }, { 0x4004, 0x02 }, { 0x4050, 0x10 },
	{ 0x4051, 0x11 }, { 0x6e47, 0x06 }, { 0x4610, 0x03 }, { 0x4613, 0x0a },
};

static const struct ov1063x_reg ov1063x_regs_enable[] = {
	{ 0x3042, 0xf9 }, { 0x3042, 0xf9 }, { 0x3042, 0xf9 }, { 0x3042, 0xf9 },
	{ 0x3042, 0xf9 }, { 0x3042, 0xf9 }, { 0x3042, 0xf9 }, { 0x3042, 0xf9 },
	{ 0x3042, 0xf9 }, { 0x3042, 0xf9 }, { 0x3042, 0xf9 }, { 0x3042, 0xf9 },
	{ 0x3042, 0xf9 }, { 0x3042, 0xf9 }, { 0x3042, 0xf9 }, { 0x3042, 0xf9 },
	{ 0x3042, 0xf9 }, { 0x3042, 0xf9 }, { 0x3042, 0xf9 }, { 0x3042, 0xf9 },
	{ 0x3042, 0xf9 }, { 0x3042, 0xf9 }, { 0x3042, 0xf9 }, { 0x3042, 0xf9 },
	{ 0x3042, 0xf9 }, { 0x3042, 0xf9 }, { 0x301b, 0xf0 }, { 0x301c, 0xf0 },
	{ 0x301a, 0xf0 },
};

/*
 * supported color format list
 */
static const struct ov1063x_color_format ov1063x_cfmts[] = {
	{
		.code		= V4L2_MBUS_FMT_YUYV8_2X8,
		.colorspace	= V4L2_COLORSPACE_SMPTE170M,
	},
	{
		.code		= V4L2_MBUS_FMT_YUYV10_2X10,
		.colorspace	= V4L2_COLORSPACE_SMPTE170M,
	},
};

static struct ov1063x_priv *to_ov1063x(const struct i2c_client *client)
{
	return container_of(i2c_get_clientdata(client), struct ov1063x_priv,
			    subdev);
}

/* read a register */
static int ov1063x_reg_read(struct i2c_client *client, u16 reg, u8 *val)
{
	int ret;
	u8 data[2] = { reg >> 8, reg & 0xff };
	struct i2c_msg msg = {
		.addr	= client->addr,
		.flags	= 0,
		.len	= 2,
		.buf	= data,
	};

	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret < 0)
		goto err;

	msg.flags = I2C_M_RD;
	msg.len	= 1;
	msg.buf	= &data[0];

	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret < 0)
		goto err;

	*val = data[0];
	return 0;
err:
	dev_err(&client->dev, "Failed reading register 0x%04x!\n", reg);
	return ret;
}

/* write a register */
static int ov1063x_reg_write(struct i2c_client *client, u16 reg, u8 val)
{
	int ret;
	u8 data[3] = { reg >> 8, reg & 0xff, val };

	struct i2c_msg msg = {
		.addr	= client->addr,
		.flags	= 0,
		.len	= 3,
		.buf	= data,
	};

	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret < 0) {
		dev_err(&client->dev, "Failed writing register 0x%04x!\n", reg);
		return ret;
	}

	return 0;
}

static int ov1063x_reg_write16(struct i2c_client *client, u16 reg, u16 val)
{
	int ret;

	ret = ov1063x_reg_write(client, reg, val >> 8);
	if (ret)
		return ret;

	ret = ov1063x_reg_write(client, reg + 1, val & 0xff);
	if (ret)
		return ret;

	return 0;
}

/* Read a register, alter its bits, write it back */
static int ov1063x_reg_rmw(struct i2c_client *client, u16 reg, u8 set, u8 unset)
{
	u8 val;
	int ret;

	ret = ov1063x_reg_read(client, reg, &val);
	if (ret) {
		dev_err(&client->dev,
			"[Read]-Modify-Write of register %04x failed!\n", reg);
		return ret;
	}

	val |= set;
	val &= ~unset;

	ret = ov1063x_reg_write(client, reg, val);
	if (ret)
		dev_err(&client->dev,
			"Read-Modify-[Write] of register %04x failed!\n", reg);

	return ret;
}


/* Start/Stop streaming from the device */
static int ov1063x_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	ov1063x_reg_write(client, 0x0100, enable);

	return 0;
}

/* Set status of additional camera capabilities */
static int ov1063x_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct ov1063x_priv *priv = container_of(ctrl->handler,
					struct ov1063x_priv, hdl);
	struct i2c_client *client = v4l2_get_subdevdata(&priv->subdev);

	switch (ctrl->id) {
	case V4L2_CID_VFLIP:
		if (ctrl->val)
			return ov1063x_reg_rmw(client, OV1063X_VFLIP,
				OV1063X_VFLIP_ON, 0);
		else
			return ov1063x_reg_rmw(client, OV1063X_VFLIP,
				0, OV1063X_VFLIP_ON);
		break;
	case V4L2_CID_HFLIP:
		if (ctrl->val)
			return ov1063x_reg_rmw(client, OV1063X_HMIRROR,
				OV1063X_HMIRROR_ON, 0);
		else
			return ov1063x_reg_rmw(client, OV1063X_HMIRROR,
				0, OV1063X_HMIRROR_ON);
		break;
	}

	return -EINVAL;
}

/*
 * Get the best pixel clock (pclk) that meets minimum hts/vts requirements.
 * xvclk => pre-divider => clk1 => multiplier => clk2 => post-divider => pclk
 * We try all valid combinations of settings for the 3 blocks to get the pixel
 * clock, and from that calculate the actual hts/vts to use. The vts is
 * extended so as to achieve the required frame rate. The function also returns
 * the PLL register contents needed to set the pixel clock.
 */
static int ov1063x_get_pclk(int xvclk, int *htsmin, int *vtsmin,
			    int fps_numerator, int fps_denominator,
			    u8 *r3003, u8 *r3004)
{
	int pre_divs[] = { 2, 3, 4, 6, 8, 10, 12, 14 };
	int pclk;
	int best_pclk = INT_MAX;
	int best_hts = 0;
	int i, j, k;
	int best_i = 0, best_j = 0, best_k = 0;
	int clk1, clk2;
	int hts;

	/* Pre-div, reg 0x3004, bits 6:4 */
	for (i = 0; i < ARRAY_SIZE(pre_divs); i++) {
		clk1 = (xvclk / pre_divs[i]) * 2;

		if ((clk1 < 3000000) || (clk1 > 27000000))
			continue;

		/* Mult = reg 0x3003, bits 5:0 */
		for (j = 1; j < 32; j++) {
			clk2 = (clk1 * j);

			if ((clk2 < 200000000) || (clk2 > 500000000))
				continue;

			/* Post-div, reg 0x3004, bits 2:0 */
			for (k = 0; k < 8; k++) {
				pclk = clk2 / (2*(k+1));

				if (pclk > 96000000)
					continue;

				hts = *htsmin + 200 + pclk/300000;

				/* 2 clock cycles for every YUV422 pixel */
				if (pclk < (((hts * *vtsmin)/fps_denominator)
					* fps_numerator * 2))
					continue;

				if (pclk < best_pclk) {
					best_pclk = pclk;
					best_hts = hts;
					best_i = i;
					best_j = j;
					best_k = k;
				}
			}
		}
	}

	/* register contents */
	*r3003 = (u8)best_j;
	*r3004 = ((u8)best_i << 4) | (u8)best_k;

	/* Did we get a valid PCLK? */
	if (best_pclk == INT_MAX)
		return -1;

	*htsmin = best_hts;

	/* Adjust vts to get as close to the desired frame rate as we can */
	*vtsmin = best_pclk / ((best_hts/fps_denominator) * fps_numerator * 2);

	return best_pclk;
}

static int ov1063x_set_regs(struct i2c_client *client,
	const struct ov1063x_reg *regs, int nr_regs)
{
	int i, ret;

	for (i = 0; i < nr_regs; i++) {
		if (regs[i].reg == 0x300c) {
			ret = ov1063x_reg_write(client, regs[i].reg,
					((client->addr * 2) | 0x1));
			if (ret)
				return ret;
		} else {
			ret = ov1063x_reg_write(client, regs[i].reg,
					regs[i].val);
			if (ret)
				return ret;
		}
	}

	return 0;
}

/* Setup registers according to resolution and color encoding */
static int ov1063x_set_params(struct i2c_client *client, u32 *width,
			      u32 *height,
			      enum v4l2_mbus_pixelcode code)
{
	struct ov1063x_priv *priv = to_ov1063x(client);
	int ret = -EINVAL;
	int pclk;
	int hts, vts;
	u8 r3003, r3004;
	int tmp;
	u32 height_pre_subsample;
	u32 width_pre_subsample;
	u8 horiz_crop_mode;
	int i;
	int nr_isp_pixels;
	int vert_sub_sample = 0;
	int horiz_sub_sample = 0;
	int sensor_width;

	if ((*width > OV1063X_MAX_WIDTH) || (*height > OV1063X_MAX_HEIGHT))
		return ret;

	/* select format */
	priv->cfmt = NULL;
	for (i = 0; i < ARRAY_SIZE(ov1063x_cfmts); i++) {
		if (code == ov1063x_cfmts[i].code) {
			priv->cfmt = ov1063x_cfmts + i;
			break;
		}
	}
	if (!priv->cfmt)
		goto ov1063x_set_fmt_error;

	priv->width = *width;
	priv->height = *height;

	/* Vertical sub-sampling? */
	height_pre_subsample = priv->height;
	if (priv->height <= 400) {
		vert_sub_sample = 1;
		height_pre_subsample <<= 1;
	}

	/* Horizontal sub-sampling? */
	width_pre_subsample = priv->width;
	if (priv->width <= 640) {
		horiz_sub_sample = 1;
		width_pre_subsample <<= 1;
	}

	/* Horizontal cropping */
	if (width_pre_subsample > 768) {
		sensor_width = OV1063X_SENSOR_WIDTH;
		horiz_crop_mode = 0x63;
	} else if (width_pre_subsample > 656) {
		sensor_width = 768;
		horiz_crop_mode = 0x6b;
	} else {
		sensor_width = 656;
		horiz_crop_mode = 0x73;
	}

	/* minimum values for hts and vts */
	hts = sensor_width;
	vts = height_pre_subsample + 50;
	dev_dbg(&client->dev, "fps=(%d/%d), hts=%d, vts=%d\n",
		priv->fps_numerator, priv->fps_denominator, hts, vts);

	/* Get the best PCLK & adjust hts,vts accordingly */
	pclk = ov1063x_get_pclk(priv->xvclk, &hts, &vts, priv->fps_numerator,
				priv->fps_denominator, &r3003, &r3004);
	if (pclk < 0)
		return ret;
	dev_dbg(&client->dev, "pclk=%d, hts=%d, vts=%d\n", pclk, hts, vts);
	dev_dbg(&client->dev, "r3003=0x%X r3004=0x%X\n", r3003, r3004);

	/* Disable ISP & program all registers that we might modify */
	ret = ov1063x_set_regs(client, ov1063x_regs_change_mode,
		ARRAY_SIZE(ov1063x_regs_change_mode));
	if (ret)
		return ret;

	/* Set to 1280x720 */
	ret = ov1063x_reg_write(client, 0x380f, 0x80);
	if (ret)
		return ret;

	/* Set PLL */
	ret = ov1063x_reg_write(client, 0x3003, r3003);
	if (ret)
		return ret;
	ret = ov1063x_reg_write(client, 0x3004, r3004);
	if (ret)
		return ret;

	/* Set HSYNC */
	ret = ov1063x_reg_write(client, 0x4700, 0x00);
	if (ret)
		return ret;

	/* Set format to UYVY */
	ret = ov1063x_reg_write(client, 0x4300, 0x3a);
	if (ret)
		return ret;

	/* Set output to 8-bit yuv */
		ret = ov1063x_reg_write(client, 0x4605, 0x08);
		if (ret)
			return ret;
#if 0
	if (priv->cfmt->code == V4L2_MBUS_FMT_YUYV8_2X8) {
		/* Set mode to 8-bit BT.656 */
		ret = ov1063x_set_regs(client, ov1063x_regs_bt656,
			ARRAY_SIZE(ov1063x_regs_bt656));
		if (ret)
			return ret;

		/* Set output to 8-bit yuv */
		ret = ov1063x_reg_write(client, 0x4605, 0x08);
		if (ret)
			return ret;
	} else {
		/* V4L2_MBUS_FMT_YUYV10_2X10 */
		/* Set mode to 10-bit BT.656 */
		ret = ov1063x_set_regs(client, ov1063x_regs_bt656_10bit,
			ARRAY_SIZE(ov1063x_regs_bt656_10bit));
		if (ret)
			return ret;

		/* Set output to 10-bit yuv */
		ret = ov1063x_reg_write(client, 0x4605, 0x00);
		if (ret)
			return ret;
	}
#endif

	/* Horizontal cropping */
	ret = ov1063x_reg_write(client, 0x3621, horiz_crop_mode);
	if (ret)
		return ret;

	ret = ov1063x_reg_write(client, 0x3702, (pclk+1500000)/3000000);
	if (ret)
		return ret;
	ret = ov1063x_reg_write(client, 0x3703, (pclk+666666)/1333333);
	if (ret)
		return ret;
	ret = ov1063x_reg_write(client, 0x3704, (pclk+961500)/1923000);
	if (ret)
		return ret;

	/* Vertical cropping */
	tmp = ((OV1063X_SENSOR_HEIGHT - height_pre_subsample) / 2) & ~0x1;
	ret = ov1063x_reg_write16(client, 0x3802, tmp);
	if (ret)
		return ret;
	tmp = tmp + height_pre_subsample + 3;
	ret = ov1063x_reg_write16(client, 0x3806, tmp);
	if (ret)
		return ret;

	/* Output size */
	ret = ov1063x_reg_write16(client, 0x3808, priv->width);
	if (ret)
		return ret;
	ret = ov1063x_reg_write16(client, 0x380a, priv->height);
	if (ret)
		return ret;

	ret = ov1063x_reg_write16(client, 0x380c, hts);
	if (ret)
		return ret;

	ret = ov1063x_reg_write16(client, 0x380e, vts);
	if (ret)
		return ret;

	if (vert_sub_sample) {
		ret = ov1063x_reg_rmw(client, OV1063X_VFLIP,
					OV1063X_VFLIP_SUBSAMPLE, 0);
		if (ret)
			return ret;

		ret = ov1063x_set_regs(client, ov1063x_regs_vert_sub_sample,
			ARRAY_SIZE(ov1063x_regs_vert_sub_sample));
		if (ret)
			return ret;
	}

	ret = ov1063x_reg_write16(client, 0x4606, 2*hts);
	if (ret)
		return ret;
	ret = ov1063x_reg_write16(client, 0x460a, 2*(hts-width_pre_subsample));
	if (ret)
		return ret;

	tmp = (vts - 8) * 16;
	ret = ov1063x_reg_write16(client, 0xc488, tmp);
	if (ret)
		return ret;
	ret = ov1063x_reg_write16(client, 0xc48a, tmp);
	if (ret)
		return ret;

	nr_isp_pixels = sensor_width * (priv->height + 4);
	ret = ov1063x_reg_write16(client, 0xc4cc, nr_isp_pixels/256);
	if (ret)
		return ret;
	ret = ov1063x_reg_write16(client, 0xc4ce, nr_isp_pixels/256);
	if (ret)
		return ret;
	ret = ov1063x_reg_write16(client, 0xc512, nr_isp_pixels/16);
	if (ret)
		return ret;

	/* Horizontal sub-sampling */
	if (horiz_sub_sample) {
		ret = ov1063x_reg_write(client, 0x5005, 0x9);
		if (ret)
			return ret;

		ret = ov1063x_reg_write(client, 0x3007, 0x2);
		if (ret)
			return ret;
	}

	ret = ov1063x_reg_write16(client, 0xc518, vts);
	if (ret)
		return ret;
	ret = ov1063x_reg_write16(client, 0xc51a, hts);
	if (ret)
		return ret;

	/* Enable ISP blocks */
	ret = ov1063x_set_regs(client, ov1063x_regs_enable,
		ARRAY_SIZE(ov1063x_regs_enable));
	if (ret)
		return ret;

	/* Wait for settings to take effect */
	mdelay(500);

	if (priv->cfmt->code == V4L2_MBUS_FMT_YUYV8_2X8)
		dev_dbg(&client->dev, "Using 8-bit BT.656\n");
	else
		dev_dbg(&client->dev, "Using 10-bit BT.656\n");

	return 0;

ov1063x_set_fmt_error:
	dev_err(&client->dev, "Unsupported settings (%dx%d@(%d/%d)fps)\n",
		*width, *height, priv->fps_numerator, priv->fps_denominator);
	priv = NULL;
	priv->cfmt = NULL;

	return ret;
}

static int ov1063x_g_fmt(struct v4l2_subdev *sd,
			struct v4l2_mbus_framefmt *mf)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov1063x_priv *priv = to_ov1063x(client);
	u32 width = OV1063X_MAX_WIDTH, height = OV1063X_MAX_HEIGHT;
	enum v4l2_mbus_pixelcode code;
	int ret;

	if (priv->cfmt)
		code = priv->cfmt->code;
	else
		code = V4L2_MBUS_FMT_YUYV8_2X8;

	ret = ov1063x_set_params(client, &width, &height, code);
	if (ret < 0)
		return ret;

	mf->width	= priv->width;
	mf->height	= priv->height;
	mf->code	= priv->cfmt->code;
	mf->colorspace	= priv->cfmt->colorspace;
	mf->field	= V4L2_FIELD_NONE;

	return 0;
}

/* set the format we will capture in */
static int ov1063x_s_fmt(struct v4l2_subdev *sd,
			struct v4l2_mbus_framefmt *mf)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov1063x_priv *priv = to_ov1063x(client);
	int ret;

	ret = ov1063x_set_params(client, &mf->width, &mf->height,
				    mf->code);
	if (!ret)
		mf->colorspace = priv->cfmt->colorspace;

	return ret;
}

static int ov1063x_try_fmt(struct v4l2_subdev *sd,
			  struct v4l2_mbus_framefmt *mf)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov1063x_priv *priv = to_ov1063x(client);
	int i;

	/* Note: All sizes are good */

	mf->field = V4L2_FIELD_NONE;

	for (i = 0; i < ARRAY_SIZE(ov1063x_cfmts); i++)
		if (mf->code == ov1063x_cfmts[i].code) {
			mf->colorspace	= ov1063x_cfmts[i].colorspace;
			break;
		}

	if (i == ARRAY_SIZE(ov1063x_cfmts)) {
		/* Unsupported format requested. Propose either */
		if (priv->cfmt) {
			/* the current one or */
			mf->colorspace = priv->cfmt->colorspace;
			mf->code = priv->cfmt->code;
		} else {
			/* the default one */
			mf->colorspace = ov1063x_cfmts[0].colorspace;
			mf->code = ov1063x_cfmts[0].code;
		}
	}

	return 0;
}

static int ov1063x_enum_fmt(struct v4l2_subdev *sd, unsigned int index,
			   enum v4l2_mbus_pixelcode *code)
{
	if (index >= ARRAY_SIZE(ov1063x_cfmts))
		return -EINVAL;

	*code = ov1063x_cfmts[index].code;

	return 0;
}

static int ov1063x_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov1063x_priv *priv = to_ov1063x(client);
	struct v4l2_captureparm *cp = &parms->parm.capture;

	if (parms->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;

	memset(cp, 0, sizeof(struct v4l2_captureparm));
	cp->capability = V4L2_CAP_TIMEPERFRAME;
	cp->timeperframe.denominator = priv->fps_numerator;
	cp->timeperframe.numerator = priv->fps_denominator;

	return 0;
}

static int ov1063x_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov1063x_priv *priv = to_ov1063x(client);
	struct v4l2_captureparm *cp = &parms->parm.capture;
	enum v4l2_mbus_pixelcode code;
	int ret;

	if (parms->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;
	if (cp->extendedmode != 0)
		return -EINVAL;

	/* FIXME Check we can handle the requested framerate */
	priv->fps_denominator = cp->timeperframe.numerator;
	priv->fps_numerator = cp->timeperframe.denominator;

	if (priv->cfmt)
		code = priv->cfmt->code;
	else
		code = V4L2_MBUS_FMT_YUYV8_2X8;

	ret = ov1063x_set_params(client, &priv->width, &priv->height, code);
	if (ret < 0)
		return ret;

	return 0;
}

static int ov1063x_s_crop(struct v4l2_subdev *sd, const struct v4l2_crop *a)
{
	struct v4l2_crop a_writable = *a;
	struct v4l2_rect *rect = &a_writable.c;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov1063x_priv *priv = to_ov1063x(client);
	int ret = -EINVAL;
	enum v4l2_mbus_pixelcode code;

	if (priv->cfmt)
		code = priv->cfmt->code;
	else
		code = V4L2_MBUS_FMT_YUYV8_2X8;

	ret = ov1063x_set_params(client, &rect->width, &rect->height, code);
	if (!ret)
		return -EINVAL;

	rect->width = priv->width;
	rect->height = priv->height;
	rect->left = 0;
	rect->top = 0;

	return ret;
}

static int ov1063x_g_crop(struct v4l2_subdev *sd, struct v4l2_crop *a)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov1063x_priv *priv = to_ov1063x(client);

	if (priv) {
		a->c.width = priv->width;
		a->c.height = priv->height;
	} else {
		a->c.width = OV1063X_MAX_WIDTH;
		a->c.height = OV1063X_MAX_HEIGHT;
	}

	a->c.left = 0;
	a->c.top = 0;
	a->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	return 0;
}

static int ov1063x_cropcap(struct v4l2_subdev *sd, struct v4l2_cropcap *a)
{
	a->bounds.left = 0;
	a->bounds.top = 0;
	a->bounds.width = OV1063X_MAX_WIDTH;
	a->bounds.height = OV1063X_MAX_HEIGHT;
	a->defrect = a->bounds;
	a->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	a->pixelaspect.numerator = 1;
	a->pixelaspect.denominator = 1;

	return 0;
}

static int ov1063x_init_sensor(struct i2c_client *client)
{
	struct ov1063x_priv *priv = to_ov1063x(client);
	int r;

	if (strcmp(priv->sensor_name, "ov10633") == 0) {
		struct gpio gpios[] = {
			{ priv->cam_fpd_mux_s0_gpio, GPIOF_OUT_INIT_LOW,
				"cam_fpd_mux_s0_gpio"},
			};

		r = gpio_request_array(gpios, ARRAY_SIZE(gpios));
		if (r)
			return r;
	} else if (strcmp(priv->sensor_name, "ov10635") == 0) {
		struct gpio gpios[] = {
		{ priv->vin2_s0_gpio, GPIOF_OUT_INIT_LOW,
			"vin2_s0" },
		{ priv->cam_fpd_mux_s0_gpio, GPIOF_OUT_INIT_HIGH,
			"cam_fpd_mux_s0" },
		{ priv->mux1_sel0_gpio, GPIOF_OUT_INIT_HIGH,
			"mux1_sel0" },
		{ priv->mux1_sel1_gpio, GPIOF_OUT_INIT_LOW,
			"mux1_sel1" },
		{ priv->mux2_sel0_gpio, GPIOF_OUT_INIT_HIGH,
			"mux2_sel0" },
		{ priv->mux2_sel1_gpio, GPIOF_OUT_INIT_LOW,
			"mux2_sel1" },
		{ priv->ov_pwdn_gpio, GPIOF_OUT_INIT_LOW,
			"ov_pwdn" },
		};

		r = gpio_request_array(gpios, ARRAY_SIZE(gpios));
		if (r)
			return r;
	}

	return 0;
}

static void ov1063x_uninit_sensor(struct i2c_client *client)
{
	struct ov1063x_priv *priv = to_ov1063x(client);
	if (strcmp(priv->sensor_name, "ov10633") == 0) {
		gpio_free(priv->cam_fpd_mux_s0_gpio);
	} else if (strcmp(priv->sensor_name, "ov10635") == 0) {
		gpio_free(priv->vin2_s0_gpio);
		gpio_free(priv->cam_fpd_mux_s0_gpio);
		gpio_free(priv->mux1_sel0_gpio);
		gpio_free(priv->mux1_sel1_gpio);
		gpio_free(priv->mux2_sel0_gpio);
		gpio_free(priv->mux2_sel1_gpio);
		gpio_free(priv->ov_pwdn_gpio);
	}
}

static const struct of_device_id ov1063x_dt_id[];

static int sensor_get_gpios(struct device_node *node, struct i2c_client *client)
{
	struct ov1063x_priv *priv = to_ov1063x(client);
	const struct of_device_id *of_dev_id;
	const char *sensor;
	int gpio;
	int err;

	of_dev_id = of_match_device(ov1063x_dt_id, &client->dev);
	if (!of_dev_id) {
		dev_err(&client->dev, "%s: Unable to match device\n", __func__);
		return -ENODEV;
	}
	sensor = (const char *)of_dev_id->data;
	if (strcmp(sensor, "ov10635") == 0)
		node = of_find_compatible_node(node, NULL, "ti,camera-ov10635");
	else if (strcmp(sensor, "ov10633") == 0)
		node = of_find_compatible_node(node, NULL, "ti,camera-ov10633");
	else
		dev_err(&client->dev, "Invalid sensor\n");

	err = of_property_read_string(node, "sensor", &priv->sensor_name);
	if (err) {
		dev_err(&client->dev, "failed to parse sensor name\n");
		return err;
	}

	if (strcmp(priv->sensor_name, "ov10635") == 0) {
		gpio = of_get_gpio(node, 0);
		if (gpio_is_valid(gpio)) {
			priv->vin2_s0_gpio = gpio;
		} else {
			dev_err(&client->dev, "failed to parse VIN2_S0 gpio\n");
			return -EINVAL;
		}

		gpio = of_get_gpio(node, 1);
		if (gpio_is_valid(gpio)) {
			priv->cam_fpd_mux_s0_gpio = gpio;
		} else {
			dev_err(&client->dev, "failed to parse CAM_FPD_MUX_S0 gpio\n");
			return -EINVAL;
		}

		gpio = of_get_gpio(node, 2);
		if (gpio_is_valid(gpio)) {
			priv->mux1_sel0_gpio = gpio;
		} else {
			dev_err(&client->dev, "failed to parse MUX1_SEL0 gpio\n");
			return -EINVAL;
		}

		gpio = of_get_gpio(node, 3);
		if (gpio_is_valid(gpio)) {
			priv->mux1_sel1_gpio = gpio;
		} else {
			dev_err(&client->dev, "failed to parse MUX1_SEL1 gpio\n");
			return -EINVAL;
		}

		gpio = of_get_gpio(node, 4);
		if (gpio_is_valid(gpio)) {
			priv->mux2_sel0_gpio = gpio;
		} else {
			dev_err(&client->dev, "failed to parse MUX2_SEL0 gpio\n");
			return -EINVAL;
		}

		gpio = of_get_gpio(node, 5);
		if (gpio_is_valid(gpio)) {
			priv->mux2_sel1_gpio = gpio;
		} else {
			dev_err(&client->dev, "failed to parse MUX2_SEL1 gpio\n");
			return -EINVAL;
		}

		gpio = of_get_gpio(node, 6);
		if (gpio_is_valid(gpio)) {
			priv->ov_pwdn_gpio = gpio;
		} else {
			dev_err(&client->dev, "failed to parse OV_PWDN gpio\n");
			return -EINVAL;
		}
	} else if (strcmp(priv->sensor_name, "ov10633") == 0) {
		gpio = of_get_gpio(node, 1);
		if (gpio_is_valid(gpio)) {
			priv->cam_fpd_mux_s0_gpio = gpio;
		} else {
			dev_err(&client->dev, "failed to parse CAM_FPD_MUX_S0 gpio\n");
			return -EINVAL;
		}
	} else {
		dev_err(&client->dev, "Invalid sensor name\n");
		return -ENODEV;
	}

	return 0;
}

static int ov1063x_video_probe(struct i2c_client *client)
{
	struct ov1063x_priv *priv = to_ov1063x(client);
	u8 pid, ver;
	u16 id;
	int ret;

	ret = ov1063x_set_regs(client, ov1063x_regs_default,
		ARRAY_SIZE(ov1063x_regs_default));
	if (ret)
		return ret;

	udelay(500);

	/* check and show product ID and manufacturer ID */
	ret = ov1063x_reg_read(client, OV1063X_PID, &pid);
	if (ret)
		return ret;

	id = pid << 8;
	ret = ov1063x_reg_read(client, OV1063X_VER, &ver);
	if (ret)
		return ret;

	id |= ver;
	if (OV1063X_VERSION(pid, ver) == OV10635_VERSION_REG) {
		priv->model = V4L2_IDENT_OV10635;
		priv->revision = 1;
	} else if (OV1063X_VERSION(pid, ver) == OV10633_VERSION_REG) {
		priv->model = V4L2_IDENT_OV10633;
		priv->revision = 1;
	} else {
		dev_err(&client->dev, "Product ID error %x:%x\n", pid, ver);
				return -ENODEV;
	}

	dev_info(&client->dev, "ov1063x Product ID %x Manufacturer ID %x\n",
		 pid, ver);

	/* Program all the 'standard' registers */

	return v4l2_ctrl_handler_setup(&priv->hdl);
}

static const struct v4l2_ctrl_ops ov1063x_ctrl_ops = {
	.s_ctrl = ov1063x_s_ctrl,
};

static struct v4l2_subdev_video_ops ov1063x_video_ops = {
	.s_stream	= ov1063x_s_stream,
	.g_mbus_fmt	= ov1063x_g_fmt,
	.s_mbus_fmt	= ov1063x_s_fmt,
	.try_mbus_fmt	= ov1063x_try_fmt,
	.enum_mbus_fmt	= ov1063x_enum_fmt,
	.cropcap	= ov1063x_cropcap,
	.g_crop		= ov1063x_g_crop,
	.s_crop		= ov1063x_s_crop,
	.g_parm		= ov1063x_g_parm,
	.s_parm		= ov1063x_s_parm,
};

static struct v4l2_subdev_ops ov1063x_subdev_ops = {
	.video	= &ov1063x_video_ops,
};

/*
 * i2c_driver function
 */

static int ov1063x_probe(struct i2c_client *client,
			 const struct i2c_device_id *did)
{
	struct device_node *node = client->dev.of_node;
	struct ov1063x_priv *priv;
	struct v4l2_subdev *sd;
	int ret;

	priv = devm_kzalloc(&client->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	i2c_set_clientdata(client, priv);

	/* TODO External XVCLK is board specific */
	priv->xvclk = 24000000;

	/* Default framerate */
	priv->fps_numerator = 30;
	priv->fps_denominator = 1;
	priv->cfmt = &ov1063x_cfmts[0];

	sd = &priv->subdev;
	v4l2_i2c_subdev_init(sd, client, &ov1063x_subdev_ops);
	v4l2_ctrl_handler_init(&priv->hdl, 2);
	v4l2_ctrl_new_std(&priv->hdl, &ov1063x_ctrl_ops,
			V4L2_CID_VFLIP, 0, 1, 1, 0);
	v4l2_ctrl_new_std(&priv->hdl, &ov1063x_ctrl_ops,
			V4L2_CID_HFLIP, 0, 1, 1, 0);
	priv->subdev.ctrl_handler = &priv->hdl;
	if (priv->hdl.error)
		return priv->hdl.error;

	ret = ov1063x_video_probe(client);
	if (ret) {
		v4l2_ctrl_handler_free(&priv->hdl);
		return ret;
	}

	ret = sensor_get_gpios(node, client);
	if (ret) {
		dev_err(&client->dev, "Unable to get gpios\n");
		return ret;
	}

	ret = ov1063x_init_sensor(client);
	if (ret) {
		dev_err(&client->dev, "failed to initialize the sensor\n");
		return ret;
	}

	sd->dev = &client->dev;
	ret = v4l2_async_register_subdev(sd);
	if (!ret)
		v4l2_info(&priv->subdev, "%s camera sensor driver registered\n",
			(char *)&priv->subdev.name);

	pm_runtime_enable(&client->dev);

	return ret;
}

static int ov1063x_remove(struct i2c_client *client)
{
	struct ov1063x_priv *priv = i2c_get_clientdata(client);

	v4l2_device_unregister_subdev(&priv->subdev);
	v4l2_ctrl_handler_free(&priv->hdl);
	ov1063x_uninit_sensor(client);

	return 0;
}

static const struct i2c_device_id ov1063x_id[] = {
	{ "ov10635", 0 },
	{ "ov10633", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ov1063x_id);

#if defined(CONFIG_OF)
static const struct of_device_id ov1063x_dt_id[] = {
	{
	.compatible   = "omnivision,ov10635", .data = "ov10635"
	},
	{
	.compatible	  = "omnivision,ov10633", .data = "ov10633"
	},
	{
	}
};

#else
#define ov1063x_dt_id NULL
#endif

static struct i2c_driver ov1063x_i2c_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "ov1063x",
		.of_match_table = ov1063x_dt_id,
	},
	.probe    = ov1063x_probe,
	.remove   = ov1063x_remove,
	.id_table = ov1063x_id,
};

module_i2c_driver(ov1063x_i2c_driver);

MODULE_DESCRIPTION("SoC Camera driver for OmniVision OV1063X");
MODULE_AUTHOR("Phil Edworthy <phil.edworthy@renesas.com>");
MODULE_LICENSE("GPL v2");
