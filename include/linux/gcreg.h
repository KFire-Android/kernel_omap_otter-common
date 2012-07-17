/*
 * gcreg.h
 *
 * Copyright (C) 2010-2011 Vivante Corporation.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef __gcreg_h
#define __gcreg_h

/*******************************************************************************
** Register access macros.
*/

#define GCREGSTART(reg_field) \
( \
	0 ? reg_field \
)

#define GCREGEND(reg_field) \
( \
	1 ? reg_field \
)

#define GCREGSIZE(reg_field) \
( \
	GCREGEND(reg_field) - GCREGSTART(reg_field) + 1 \
)

#define GCREGALIGN(data, reg_field) \
( \
	((unsigned int) (data)) << GCREGSTART(reg_field) \
)

#define GCREGMASK(reg_field) \
( \
	GCREGALIGN(~0UL >> (32 - GCREGSIZE(reg_field)), reg_field) \
)

#define GCSETFIELDVAL(data, reg, field, value) \
( \
	(((unsigned int) (data)) & ~GCREGMASK(reg##_##field)) \
	| (GCREGALIGN(reg##_##field##_##value, reg##_##field) \
		& GCREGMASK(reg##_##field)) \
)

#define GCSETFIELD(data, reg, field, value) \
( \
	(((unsigned int) (data)) & ~GCREGMASK(reg##_##field)) \
	| (GCREGALIGN((unsigned int) (value), reg##_##field) \
		& GCREGMASK(reg##_##field)) \
)

#define GCGETFIELD(data, reg, field) \
( \
	(((unsigned int) (data)) & GCREGMASK(reg##_##field)) \
	>> GCREGSTART(reg##_##field) \
)

#define GCREGVALUE(reg, field, val) \
( \
	reg##_##field##_##val \
)

/*******************************************************************************
** Register gcregHiClockControl
*/

#define GCREG_HI_CLOCK_CONTROL_Address                                   0x00000
#define GCREG_HI_CLOCK_CONTROL_MSB                                            15
#define GCREG_HI_CLOCK_CONTROL_LSB                                             0
#define GCREG_HI_CLOCK_CONTROL_BLK                                             0
#define GCREG_HI_CLOCK_CONTROL_Count                                           1
#define GCREG_HI_CLOCK_CONTROL_FieldMask                              0x000A17FE
#define GCREG_HI_CLOCK_CONTROL_ReadMask                               0x000A17FE
#define GCREG_HI_CLOCK_CONTROL_WriteMask                              0x000817FE
#define GCREG_HI_CLOCK_CONTROL_ResetValue                             0x00000100

/* Disable 3D clock. */
#define GCREG_HI_CLOCK_CONTROL_CLK3D_DIS                                   0 : 0
#define GCREG_HI_CLOCK_CONTROL_CLK3D_DIS_End                                   0
#define GCREG_HI_CLOCK_CONTROL_CLK3D_DIS_Start                                 0
#define GCREG_HI_CLOCK_CONTROL_CLK3D_DIS_Type                                U01

/* Disable 2D clock. */
#define GCREG_HI_CLOCK_CONTROL_CLK2D_DIS                                   1 : 1
#define GCREG_HI_CLOCK_CONTROL_CLK2D_DIS_End                                   1
#define GCREG_HI_CLOCK_CONTROL_CLK2D_DIS_Start                                 1
#define GCREG_HI_CLOCK_CONTROL_CLK2D_DIS_Type                                U01

#define GCREG_HI_CLOCK_CONTROL_FSCALE_VAL                                  8 : 2
#define GCREG_HI_CLOCK_CONTROL_FSCALE_VAL_End                                  8
#define GCREG_HI_CLOCK_CONTROL_FSCALE_VAL_Start                                2
#define GCREG_HI_CLOCK_CONTROL_FSCALE_VAL_Type                               U07

#define GCREG_HI_CLOCK_CONTROL_FSCALE_CMD_LOAD                             9 : 9
#define GCREG_HI_CLOCK_CONTROL_FSCALE_CMD_LOAD_End                             9
#define GCREG_HI_CLOCK_CONTROL_FSCALE_CMD_LOAD_Start                           9
#define GCREG_HI_CLOCK_CONTROL_FSCALE_CMD_LOAD_Type                          U01

/* Disables clock gating for rams. */
#define GCREG_HI_CLOCK_CONTROL_DISABLE_RAM_CLK_GATING                    10 : 10
#define GCREG_HI_CLOCK_CONTROL_DISABLE_RAM_CLK_GATING_End                     10
#define GCREG_HI_CLOCK_CONTROL_DISABLE_RAM_CLK_GATING_Start                   10
#define GCREG_HI_CLOCK_CONTROL_DISABLE_RAM_CLK_GATING_Type                   U01

/* Disable debug registers. If this bit is 1, debug regs are clock gated. */
#define GCREG_HI_CLOCK_CONTROL_DISABLE_DEBUG_REGISTERS                   11 : 11
#define GCREG_HI_CLOCK_CONTROL_DISABLE_DEBUG_REGISTERS_End                    11
#define GCREG_HI_CLOCK_CONTROL_DISABLE_DEBUG_REGISTERS_Start                  11
#define GCREG_HI_CLOCK_CONTROL_DISABLE_DEBUG_REGISTERS_Type                  U01

/* Soft resets the IP. */
#define GCREG_HI_CLOCK_CONTROL_SOFT_RESET                                12 : 12
#define GCREG_HI_CLOCK_CONTROL_SOFT_RESET_End                                 12
#define GCREG_HI_CLOCK_CONTROL_SOFT_RESET_Start                               12
#define GCREG_HI_CLOCK_CONTROL_SOFT_RESET_Type                               U01

/* 3D pipe is idle. */
#define GCREG_HI_CLOCK_CONTROL_IDLE_3D                                   16 : 16
#define GCREG_HI_CLOCK_CONTROL_IDLE_3D_End                                    16
#define GCREG_HI_CLOCK_CONTROL_IDLE_3D_Start                                  16
#define GCREG_HI_CLOCK_CONTROL_IDLE_3D_Type                                  U01

/* 2D pipe is idle. */
#define GCREG_HI_CLOCK_CONTROL_IDLE_2D                                   17 : 17
#define GCREG_HI_CLOCK_CONTROL_IDLE_2D_End                                    17
#define GCREG_HI_CLOCK_CONTROL_IDLE_2D_Start                                  17
#define GCREG_HI_CLOCK_CONTROL_IDLE_2D_Type                                  U01

/* VG pipe is idle. */
#define GCREG_HI_CLOCK_CONTROL_IDLE_VG                                   18 : 18
#define GCREG_HI_CLOCK_CONTROL_IDLE_VG_End                                    18
#define GCREG_HI_CLOCK_CONTROL_IDLE_VG_Start                                  18
#define GCREG_HI_CLOCK_CONTROL_IDLE_VG_Type                                  U01

/* Isolate GPU bit */
#define GCREG_HI_CLOCK_CONTROL_ISOLATE_GPU                               19 : 19
#define GCREG_HI_CLOCK_CONTROL_ISOLATE_GPU_End                                19
#define GCREG_HI_CLOCK_CONTROL_ISOLATE_GPU_Start                              19
#define GCREG_HI_CLOCK_CONTROL_ISOLATE_GPU_Type                              U01

union gcclockcontrol {
	struct {
		/* gcregHiClockControl:
			GCREG_HI_CLOCK_CONTROL_CLK3D_DIS */
		unsigned int disable3d:1;

		/* gcregHiClockControl:
			GCREG_HI_CLOCK_CONTROL_CLK2D_DIS */
		unsigned int disable2d:1;

		/* gcregHiClockControl:
			GCREG_HI_CLOCK_CONTROL_FSCALE_VAL */
		unsigned int pulsecount:7;

		/* gcregHiClockControl:
			GCREG_HI_CLOCK_CONTROL_FSCALE_CMD_LOAD */
		unsigned int pulseset:1;

		/* gcregHiClockControl:
			GCREG_HI_CLOCK_CONTROL_DISABLE_RAM_CLK_GATING */
		unsigned int ramgate:1;

		/* gcregHiClockControl:
			GCREG_HI_CLOCK_CONTROL_DISABLE_DEBUG_REGISTERS */
		unsigned int disabledbg:1;

		/* gcregHiClockControl:
			GCREG_HI_CLOCK_CONTROL_SOFT_RESET */
		unsigned int reset:1;

		/* gcregHiClockControl:
			reserved */
		unsigned int _reserved_13_15:3;

		/* gcregHiClockControl:
			GCREG_HI_CLOCK_CONTROL_IDLE_3D */
		unsigned int idle3d:1;

		/* gcregHiClockControl:
			GCREG_HI_CLOCK_CONTROL_IDLE_2D */
		unsigned int idle2d:1;

		/* gcregHiClockControl:
			GCREG_HI_CLOCK_CONTROL_IDLE_VG */
		unsigned int idlevg:1;

		/* gcregHiClockControl:
			GCREG_HI_CLOCK_CONTROL_ISOLATE_GPU */
		unsigned int isolate:1;

		/* gcregHiClockControl:
			reserved */
		unsigned int _reserved_20_31:12;
	} reg;

	unsigned int raw;
};

/*******************************************************************************
** Register gcregHiIdle
*/

#define GCREG_HI_IDLE_Address                                            0x00004
#define GCREG_HI_IDLE_MSB                                                     15
#define GCREG_HI_IDLE_LSB                                                      0
#define GCREG_HI_IDLE_BLK                                                      0
#define GCREG_HI_IDLE_Count                                                    1
#define GCREG_HI_IDLE_FieldMask                                       0x80000007
#define GCREG_HI_IDLE_ReadMask                                        0x80000007
#define GCREG_HI_IDLE_WriteMask                                       0x00000000
#define GCREG_HI_IDLE_ResetValue                                      0x00000007

/* FE is idle. */
#define GCREG_HI_IDLE_IDLE_FE                                              0 : 0
#define GCREG_HI_IDLE_IDLE_FE_End                                              0
#define GCREG_HI_IDLE_IDLE_FE_Start                                            0
#define GCREG_HI_IDLE_IDLE_FE_Type                                           U01

/* DE is idle. */
#define GCREG_HI_IDLE_IDLE_DE                                              1 : 1
#define GCREG_HI_IDLE_IDLE_DE_End                                              1
#define GCREG_HI_IDLE_IDLE_DE_Start                                            1
#define GCREG_HI_IDLE_IDLE_DE_Type                                           U01

/* PE is idle. */
#define GCREG_HI_IDLE_IDLE_PE                                              2 : 2
#define GCREG_HI_IDLE_IDLE_PE_End                                              2
#define GCREG_HI_IDLE_IDLE_PE_Start                                            2
#define GCREG_HI_IDLE_IDLE_PE_Type                                           U01

/* AXI is in low power mode. */
#define GCREG_HI_IDLE_AXI_LP                                             31 : 31
#define GCREG_HI_IDLE_AXI_LP_End                                              31
#define GCREG_HI_IDLE_AXI_LP_Start                                            31
#define GCREG_HI_IDLE_AXI_LP_Type                                            U01

union gcidle {
	struct {
		/* gcregHiIdle: GCREG_HI_IDLE_IDLE_FE */
		unsigned int fe:1;

		/* gcregHiIdle: GCREG_HI_IDLE_IDLE_DE */
		unsigned int de:1;

		/* gcregHiIdle: GCREG_HI_IDLE_IDLE_PE */
		unsigned int pe:1;

		/* gcregHiIdle: reserved */
		unsigned int _reserved_3_30:28;

		/* gcregHiIdle: GCREG_HI_IDLE_AXI_LP */
		unsigned int axilp:1;
	} reg;

	unsigned int raw;
};

/*******************************************************************************
** Register gcregAxiConfig
*/

#define GCREG_AXI_CONFIG_Address                                         0x00008
#define GCREG_AXI_CONFIG_MSB                                                  15
#define GCREG_AXI_CONFIG_LSB                                                   0
#define GCREG_AXI_CONFIG_BLK                                                   0
#define GCREG_AXI_CONFIG_Count                                                 1
#define GCREG_AXI_CONFIG_FieldMask                                    0x0000FFFF
#define GCREG_AXI_CONFIG_ReadMask                                     0x0000FFFF
#define GCREG_AXI_CONFIG_WriteMask                                    0x0000FFFF
#define GCREG_AXI_CONFIG_ResetValue                                   0x00000000

#define GCREG_AXI_CONFIG_AWID                                              3 : 0
#define GCREG_AXI_CONFIG_AWID_End                                              3
#define GCREG_AXI_CONFIG_AWID_Start                                            0
#define GCREG_AXI_CONFIG_AWID_Type                                           U04

#define GCREG_AXI_CONFIG_ARID                                              7 : 4
#define GCREG_AXI_CONFIG_ARID_End                                              7
#define GCREG_AXI_CONFIG_ARID_Start                                            4
#define GCREG_AXI_CONFIG_ARID_Type                                           U04

#define GCREG_AXI_CONFIG_AWCACHE                                          11 : 8
#define GCREG_AXI_CONFIG_AWCACHE_End                                          11
#define GCREG_AXI_CONFIG_AWCACHE_Start                                         8
#define GCREG_AXI_CONFIG_AWCACHE_Type                                        U04

#define GCREG_AXI_CONFIG_ARCACHE                                         15 : 12
#define GCREG_AXI_CONFIG_ARCACHE_End                                          15
#define GCREG_AXI_CONFIG_ARCACHE_Start                                        12
#define GCREG_AXI_CONFIG_ARCACHE_Type                                        U04

/*******************************************************************************
** Register gcregAxiStatus
*/

#define GCREG_AXI_STATUS_Address                                         0x0000C
#define GCREG_AXI_STATUS_MSB                                                  15
#define GCREG_AXI_STATUS_LSB                                                   0
#define GCREG_AXI_STATUS_BLK                                                   0
#define GCREG_AXI_STATUS_Count                                                 1
#define GCREG_AXI_STATUS_FieldMask                                    0x000003FF
#define GCREG_AXI_STATUS_ReadMask                                     0x000003FF
#define GCREG_AXI_STATUS_WriteMask                                    0x00000000
#define GCREG_AXI_STATUS_ResetValue                                   0x00000000

#define GCREG_AXI_STATUS_DET_RD_ERR                                        9 : 9
#define GCREG_AXI_STATUS_DET_RD_ERR_End                                        9
#define GCREG_AXI_STATUS_DET_RD_ERR_Start                                      9
#define GCREG_AXI_STATUS_DET_RD_ERR_Type                                     U01

#define GCREG_AXI_STATUS_DET_WR_ERR                                        8 : 8
#define GCREG_AXI_STATUS_DET_WR_ERR_End                                        8
#define GCREG_AXI_STATUS_DET_WR_ERR_Start                                      8
#define GCREG_AXI_STATUS_DET_WR_ERR_Type                                     U01

#define GCREG_AXI_STATUS_RD_ERR_ID                                         7 : 4
#define GCREG_AXI_STATUS_RD_ERR_ID_End                                         7
#define GCREG_AXI_STATUS_RD_ERR_ID_Start                                       4
#define GCREG_AXI_STATUS_RD_ERR_ID_Type                                      U04

#define GCREG_AXI_STATUS_WR_ERR_ID                                         3 : 0
#define GCREG_AXI_STATUS_WR_ERR_ID_End                                         3
#define GCREG_AXI_STATUS_WR_ERR_ID_Start                                       0
#define GCREG_AXI_STATUS_WR_ERR_ID_Type                                      U04

/*******************************************************************************
** Register gcregIntrAcknowledge
*/

/* Interrupt acknowledge register. Each bit represents a corresponding event
** being triggered. Reading from this register clears the outstanding interrupt.
*/

#define GCREG_INTR_ACKNOWLEDGE_Address                                   0x00010
#define GCREG_INTR_ACKNOWLEDGE_MSB                                            15
#define GCREG_INTR_ACKNOWLEDGE_LSB                                             0
#define GCREG_INTR_ACKNOWLEDGE_BLK                                             0
#define GCREG_INTR_ACKNOWLEDGE_Count                                           1
#define GCREG_INTR_ACKNOWLEDGE_FieldMask                              0xFFFFFFFF
#define GCREG_INTR_ACKNOWLEDGE_ReadMask                               0xFFFFFFFF
#define GCREG_INTR_ACKNOWLEDGE_WriteMask                              0x00000000
#define GCREG_INTR_ACKNOWLEDGE_ResetValue                             0x00000000

#define GCREG_INTR_ACKNOWLEDGE_INTR_VEC                                   31 : 0
#define GCREG_INTR_ACKNOWLEDGE_INTR_VEC_End                                   31
#define GCREG_INTR_ACKNOWLEDGE_INTR_VEC_Start                                  0
#define GCREG_INTR_ACKNOWLEDGE_INTR_VEC_Type                                 U32

/*******************************************************************************
** Register gcregIntrEnbl
*/

/* Interrupt enable register. Each bit enables a corresponding event. */

#define GCREG_INTR_ENBL_Address                                          0x00014
#define GCREG_INTR_ENBL_MSB                                                   15
#define GCREG_INTR_ENBL_LSB                                                    0
#define GCREG_INTR_ENBL_BLK                                                    0
#define GCREG_INTR_ENBL_Count                                                  1
#define GCREG_INTR_ENBL_FieldMask                                     0xFFFFFFFF
#define GCREG_INTR_ENBL_ReadMask                                      0xFFFFFFFF
#define GCREG_INTR_ENBL_WriteMask                                     0xFFFFFFFF
#define GCREG_INTR_ENBL_ResetValue                                    0x00000000

#define GCREG_INTR_ENBL_INTR_ENBL_VEC                                     31 : 0
#define GCREG_INTR_ENBL_INTR_ENBL_VEC_End                                     31
#define GCREG_INTR_ENBL_INTR_ENBL_VEC_Start                                    0
#define GCREG_INTR_ENBL_INTR_ENBL_VEC_Type                                   U32

/*******************************************************************************
** Register GCFeatures
*/

/* Shows which features are enabled in this chip.  This register has no set
** reset value.  It varies with the implementation.
*/

#define GC_FEATURES_Address                                              0x0001C
#define GC_FEATURES_MSB                                                       15
#define GC_FEATURES_LSB                                                        0
#define GC_FEATURES_BLK                                                        0
#define GC_FEATURES_Count                                                      1
#define GC_FEATURES_FieldMask                                         0xFFFFFFFF
#define GC_FEATURES_ReadMask                                          0xFFFFFFFF
#define GC_FEATURES_WriteMask                                         0x00000000
#define GC_FEATURES_ResetValue                                        0x00000000

/* Fast clear. */
#define GC_FEATURES_FAST_CLEAR                                             0 : 0
#define GC_FEATURES_FAST_CLEAR_End                                             0
#define GC_FEATURES_FAST_CLEAR_Start                                           0
#define GC_FEATURES_FAST_CLEAR_Type                                          U01
#define   GC_FEATURES_FAST_CLEAR_NONE                                        0x0
#define   GC_FEATURES_FAST_CLEAR_AVAILABLE                                   0x1

/* Full-screen anti-aliasing. */
#define GC_FEATURES_SPECIAL_ANTI_ALIASING                                  1 : 1
#define GC_FEATURES_SPECIAL_ANTI_ALIASING_End                                  1
#define GC_FEATURES_SPECIAL_ANTI_ALIASING_Start                                1
#define GC_FEATURES_SPECIAL_ANTI_ALIASING_Type                               U01
#define   GC_FEATURES_SPECIAL_ANTI_ALIASING_NONE                             0x0
#define   GC_FEATURES_SPECIAL_ANTI_ALIASING_AVAILABLE                        0x1

/* 3D pipe. */
#define GC_FEATURES_PIPE_3D                                                2 : 2
#define GC_FEATURES_PIPE_3D_End                                                2
#define GC_FEATURES_PIPE_3D_Start                                              2
#define GC_FEATURES_PIPE_3D_Type                                             U01
#define   GC_FEATURES_PIPE_3D_NONE                                           0x0
#define   GC_FEATURES_PIPE_3D_AVAILABLE                                      0x1

/* DXT texture compression. */
#define GC_FEATURES_DXT_TEXTURE_COMPRESSION                                3 : 3
#define GC_FEATURES_DXT_TEXTURE_COMPRESSION_End                                3
#define GC_FEATURES_DXT_TEXTURE_COMPRESSION_Start                              3
#define GC_FEATURES_DXT_TEXTURE_COMPRESSION_Type                             U01
#define   GC_FEATURES_DXT_TEXTURE_COMPRESSION_NONE                           0x0
#define   GC_FEATURES_DXT_TEXTURE_COMPRESSION_AVAILABLE                      0x1

/* Debug registers. */
#define GC_FEATURES_DEBUG_MODE                                             4 : 4
#define GC_FEATURES_DEBUG_MODE_End                                             4
#define GC_FEATURES_DEBUG_MODE_Start                                           4
#define GC_FEATURES_DEBUG_MODE_Type                                          U01
#define   GC_FEATURES_DEBUG_MODE_NONE                                        0x0
#define   GC_FEATURES_DEBUG_MODE_AVAILABLE                                   0x1

/* Depth and color compression. */
#define GC_FEATURES_ZCOMPRESSION                                           5 : 5
#define GC_FEATURES_ZCOMPRESSION_End                                           5
#define GC_FEATURES_ZCOMPRESSION_Start                                         5
#define GC_FEATURES_ZCOMPRESSION_Type                                        U01
#define   GC_FEATURES_ZCOMPRESSION_NONE                                      0x0
#define   GC_FEATURES_ZCOMPRESSION_AVAILABLE                                 0x1

/* YUV 4:2:0 support in filter blit. */
#define GC_FEATURES_YUV420_FILTER                                          6 : 6
#define GC_FEATURES_YUV420_FILTER_End                                          6
#define GC_FEATURES_YUV420_FILTER_Start                                        6
#define GC_FEATURES_YUV420_FILTER_Type                                       U01
#define   GC_FEATURES_YUV420_FILTER_NONE                                     0x0
#define   GC_FEATURES_YUV420_FILTER_AVAILABLE                                0x1

/* MSAA support. */
#define GC_FEATURES_MSAA                                                   7 : 7
#define GC_FEATURES_MSAA_End                                                   7
#define GC_FEATURES_MSAA_Start                                                 7
#define GC_FEATURES_MSAA_Type                                                U01
#define   GC_FEATURES_MSAA_NONE                                              0x0
#define   GC_FEATURES_MSAA_AVAILABLE                                         0x1

/* Shows if there is a display controller in the IP. */
#define GC_FEATURES_DC                                                     8 : 8
#define GC_FEATURES_DC_End                                                     8
#define GC_FEATURES_DC_Start                                                   8
#define GC_FEATURES_DC_Type                                                  U01
#define   GC_FEATURES_DC_NONE                                                0x0
#define   GC_FEATURES_DC_AVAILABLE                                           0x1

/* Shows if there is 2D engine. */
#define GC_FEATURES_PIPE_2D                                                9 : 9
#define GC_FEATURES_PIPE_2D_End                                                9
#define GC_FEATURES_PIPE_2D_Start                                              9
#define GC_FEATURES_PIPE_2D_Type                                             U01
#define   GC_FEATURES_PIPE_2D_NONE                                           0x0
#define   GC_FEATURES_PIPE_2D_AVAILABLE                                      0x1

/* ETC1 texture compression. */
#define GC_FEATURES_ETC1_TEXTURE_COMPRESSION                             10 : 10
#define GC_FEATURES_ETC1_TEXTURE_COMPRESSION_End                              10
#define GC_FEATURES_ETC1_TEXTURE_COMPRESSION_Start                            10
#define GC_FEATURES_ETC1_TEXTURE_COMPRESSION_Type                            U01
#define   GC_FEATURES_ETC1_TEXTURE_COMPRESSION_NONE                          0x0
#define   GC_FEATURES_ETC1_TEXTURE_COMPRESSION_AVAILABLE                     0x1

/* Shows if the IP has HD scaler. */
#define GC_FEATURES_FAST_SCALER                                          11 : 11
#define GC_FEATURES_FAST_SCALER_End                                           11
#define GC_FEATURES_FAST_SCALER_Start                                         11
#define GC_FEATURES_FAST_SCALER_Type                                         U01
#define   GC_FEATURES_FAST_SCALER_NONE                                       0x0
#define   GC_FEATURES_FAST_SCALER_AVAILABLE                                  0x1

/* Shows if the IP has HDR support. */
#define GC_FEATURES_HIGH_DYNAMIC_RANGE                                   12 : 12
#define GC_FEATURES_HIGH_DYNAMIC_RANGE_End                                    12
#define GC_FEATURES_HIGH_DYNAMIC_RANGE_Start                                  12
#define GC_FEATURES_HIGH_DYNAMIC_RANGE_Type                                  U01
#define   GC_FEATURES_HIGH_DYNAMIC_RANGE_NONE                                0x0
#define   GC_FEATURES_HIGH_DYNAMIC_RANGE_AVAILABLE                           0x1

/* YUV 4:2:0 tiler is available. */
#define GC_FEATURES_YUV420_TILER                                         13 : 13
#define GC_FEATURES_YUV420_TILER_End                                          13
#define GC_FEATURES_YUV420_TILER_Start                                        13
#define GC_FEATURES_YUV420_TILER_Type                                        U01
#define   GC_FEATURES_YUV420_TILER_NONE                                      0x0
#define   GC_FEATURES_YUV420_TILER_AVAILABLE                                 0x1

/* Second level clock gating is available. */
#define GC_FEATURES_MODULE_CG                                            14 : 14
#define GC_FEATURES_MODULE_CG_End                                             14
#define GC_FEATURES_MODULE_CG_Start                                           14
#define GC_FEATURES_MODULE_CG_Type                                           U01
#define   GC_FEATURES_MODULE_CG_NONE                                         0x0
#define   GC_FEATURES_MODULE_CG_AVAILABLE                                    0x1

/* IP is configured to have minimum area. */
#define GC_FEATURES_MIN_AREA                                             15 : 15
#define GC_FEATURES_MIN_AREA_End                                              15
#define GC_FEATURES_MIN_AREA_Start                                            15
#define GC_FEATURES_MIN_AREA_Type                                            U01
#define   GC_FEATURES_MIN_AREA_NONE                                          0x0
#define   GC_FEATURES_MIN_AREA_AVAILABLE                                     0x1

/* IP does not have early-Z. */
#define GC_FEATURES_NO_EZ                                                16 : 16
#define GC_FEATURES_NO_EZ_End                                                 16
#define GC_FEATURES_NO_EZ_Start                                               16
#define GC_FEATURES_NO_EZ_Type                                               U01
#define   GC_FEATURES_NO_EZ_NONE                                             0x0
#define   GC_FEATURES_NO_EZ_AVAILABLE                                        0x1

/* IP does not have 422 texture input format. */
#define GC_FEATURES_NO422_TEXTURE                                        17 : 17
#define GC_FEATURES_NO422_TEXTURE_End                                         17
#define GC_FEATURES_NO422_TEXTURE_Start                                       17
#define GC_FEATURES_NO422_TEXTURE_Type                                       U01
#define   GC_FEATURES_NO422_TEXTURE_NONE                                     0x0
#define   GC_FEATURES_NO422_TEXTURE_AVAILABLE                                0x1

/* IP supports interleaving depth and color buffers. */
#define GC_FEATURES_BUFFER_INTERLEAVING                                  18 : 18
#define GC_FEATURES_BUFFER_INTERLEAVING_End                                   18
#define GC_FEATURES_BUFFER_INTERLEAVING_Start                                 18
#define GC_FEATURES_BUFFER_INTERLEAVING_Type                                 U01
#define   GC_FEATURES_BUFFER_INTERLEAVING_NONE                               0x0
#define   GC_FEATURES_BUFFER_INTERLEAVING_AVAILABLE                          0x1

/* Supports byte write in 2D. */
#define GC_FEATURES_BYTE_WRITE_2D                                        19 : 19
#define GC_FEATURES_BYTE_WRITE_2D_End                                         19
#define GC_FEATURES_BYTE_WRITE_2D_Start                                       19
#define GC_FEATURES_BYTE_WRITE_2D_Type                                       U01
#define   GC_FEATURES_BYTE_WRITE_2D_NONE                                     0x0
#define   GC_FEATURES_BYTE_WRITE_2D_AVAILABLE                                0x1

/* IP does not have 2D scaler. */
#define GC_FEATURES_NO_SCALER                                            20 : 20
#define GC_FEATURES_NO_SCALER_End                                             20
#define GC_FEATURES_NO_SCALER_Start                                           20
#define GC_FEATURES_NO_SCALER_Type                                           U01
#define   GC_FEATURES_NO_SCALER_NONE                                         0x0
#define   GC_FEATURES_NO_SCALER_AVAILABLE                                    0x1

/* YUY2 averaging support in resolve. */
#define GC_FEATURES_YUY2_AVERAGING                                       21 : 21
#define GC_FEATURES_YUY2_AVERAGING_End                                        21
#define GC_FEATURES_YUY2_AVERAGING_Start                                      21
#define GC_FEATURES_YUY2_AVERAGING_Type                                      U01
#define   GC_FEATURES_YUY2_AVERAGING_NONE                                    0x0
#define   GC_FEATURES_YUY2_AVERAGING_AVAILABLE                               0x1

/* PE cache is half. */
#define GC_FEATURES_HALF_PE_CACHE                                        22 : 22
#define GC_FEATURES_HALF_PE_CACHE_End                                         22
#define GC_FEATURES_HALF_PE_CACHE_Start                                       22
#define GC_FEATURES_HALF_PE_CACHE_Type                                       U01
#define   GC_FEATURES_HALF_PE_CACHE_NONE                                     0x0
#define   GC_FEATURES_HALF_PE_CACHE_AVAILABLE                                0x1

/* TX cache is half. */
#define GC_FEATURES_HALF_TX_CACHE                                        23 : 23
#define GC_FEATURES_HALF_TX_CACHE_End                                         23
#define GC_FEATURES_HALF_TX_CACHE_Start                                       23
#define GC_FEATURES_HALF_TX_CACHE_Type                                       U01
#define   GC_FEATURES_HALF_TX_CACHE_NONE                                     0x0
#define   GC_FEATURES_HALF_TX_CACHE_AVAILABLE                                0x1

/* YUY2 support in PE and YUY2 to RGB conversion in resolve. */
#define GC_FEATURES_YUY2_RENDER_TARGET                                   24 : 24
#define GC_FEATURES_YUY2_RENDER_TARGET_End                                    24
#define GC_FEATURES_YUY2_RENDER_TARGET_Start                                  24
#define GC_FEATURES_YUY2_RENDER_TARGET_Type                                  U01
#define   GC_FEATURES_YUY2_RENDER_TARGET_NONE                                0x0
#define   GC_FEATURES_YUY2_RENDER_TARGET_AVAILABLE                           0x1

/* 32 bit memory address support. */
#define GC_FEATURES_MEM32_BIT_SUPPORT                                    25 : 25
#define GC_FEATURES_MEM32_BIT_SUPPORT_End                                     25
#define GC_FEATURES_MEM32_BIT_SUPPORT_Start                                   25
#define GC_FEATURES_MEM32_BIT_SUPPORT_Type                                   U01
#define   GC_FEATURES_MEM32_BIT_SUPPORT_NONE                                 0x0
#define   GC_FEATURES_MEM32_BIT_SUPPORT_AVAILABLE                            0x1

/* VG pipe is present. */
#define GC_FEATURES_PIPE_VG                                              26 : 26
#define GC_FEATURES_PIPE_VG_End                                               26
#define GC_FEATURES_PIPE_VG_Start                                             26
#define GC_FEATURES_PIPE_VG_Type                                             U01
#define   GC_FEATURES_PIPE_VG_NONE                                           0x0
#define   GC_FEATURES_PIPE_VG_AVAILABLE                                      0x1

/* VG tesselator is present. */
#define GC_FEATURES_VGTS                                                 27 : 27
#define GC_FEATURES_VGTS_End                                                  27
#define GC_FEATURES_VGTS_Start                                                27
#define GC_FEATURES_VGTS_Type                                                U01
#define   GC_FEATURES_VGTS_NONE                                              0x0
#define   GC_FEATURES_VGTS_AVAILABLE                                         0x1

/* FE 2.0 is present. */
#define GC_FEATURES_FE20                                                 28 : 28
#define GC_FEATURES_FE20_End                                                  28
#define GC_FEATURES_FE20_Start                                                28
#define GC_FEATURES_FE20_Type                                                U01
#define   GC_FEATURES_FE20_NONE                                              0x0
#define   GC_FEATURES_FE20_AVAILABLE                                         0x1

/* 3D PE has byte write capability. */
#define GC_FEATURES_BYTE_WRITE_3D                                        29 : 29
#define GC_FEATURES_BYTE_WRITE_3D_End                                         29
#define GC_FEATURES_BYTE_WRITE_3D_Start                                       29
#define GC_FEATURES_BYTE_WRITE_3D_Type                                       U01
#define   GC_FEATURES_BYTE_WRITE_3D_NONE                                     0x0
#define   GC_FEATURES_BYTE_WRITE_3D_AVAILABLE                                0x1

/* Supports resolveing into YUV target. */
#define GC_FEATURES_RS_YUV_TARGET                                        30 : 30
#define GC_FEATURES_RS_YUV_TARGET_End                                         30
#define GC_FEATURES_RS_YUV_TARGET_Start                                       30
#define GC_FEATURES_RS_YUV_TARGET_Type                                       U01
#define   GC_FEATURES_RS_YUV_TARGET_NONE                                     0x0
#define   GC_FEATURES_RS_YUV_TARGET_AVAILABLE                                0x1

/* Supports 20 bit index. */
#define GC_FEATURES_FE20_BIT_INDEX                                       31 : 31
#define GC_FEATURES_FE20_BIT_INDEX_End                                        31
#define GC_FEATURES_FE20_BIT_INDEX_Start                                      31
#define GC_FEATURES_FE20_BIT_INDEX_Type                                      U01
#define   GC_FEATURES_FE20_BIT_INDEX_NONE                                    0x0
#define   GC_FEATURES_FE20_BIT_INDEX_AVAILABLE                               0x1

/*******************************************************************************
** Register GCChipId
*/

/* Shows the ID for the chip in BCD.  This register has no set reset value.
** It varies with the implementation.
*/

#define GC_CHIP_ID_Address                                               0x00020
#define GC_CHIP_ID_MSB                                                        15
#define GC_CHIP_ID_LSB                                                         0
#define GC_CHIP_ID_BLK                                                         0
#define GC_CHIP_ID_Count                                                       1
#define GC_CHIP_ID_FieldMask                                          0xFFFFFFFF
#define GC_CHIP_ID_ReadMask                                           0xFFFFFFFF
#define GC_CHIP_ID_WriteMask                                          0x00000000
#define GC_CHIP_ID_ResetValue                                         0x00000000

/* Id. */
#define GC_CHIP_ID_ID                                                     31 : 0
#define GC_CHIP_ID_ID_End                                                     31
#define GC_CHIP_ID_ID_Start                                                    0
#define GC_CHIP_ID_ID_Type                                                   U32

/*******************************************************************************
** Register GCChipRev
*/

/* Shows the revision for the chip in BCD.  This register has no set reset
** value.  It varies with the implementation.
*/

#define GC_CHIP_REV_Address                                              0x00024
#define GC_CHIP_REV_MSB                                                       15
#define GC_CHIP_REV_LSB                                                        0
#define GC_CHIP_REV_BLK                                                        0
#define GC_CHIP_REV_Count                                                      1
#define GC_CHIP_REV_FieldMask                                         0xFFFFFFFF
#define GC_CHIP_REV_ReadMask                                          0xFFFFFFFF
#define GC_CHIP_REV_WriteMask                                         0x00000000
#define GC_CHIP_REV_ResetValue                                        0x00000000

/* Revision. */
#define GC_CHIP_REV_REV                                                   31 : 0
#define GC_CHIP_REV_REV_End                                                   31
#define GC_CHIP_REV_REV_Start                                                  0
#define GC_CHIP_REV_REV_Type                                                 U32

/*******************************************************************************
** Register GCChipDate
*/

/* Shows the release date for the IP.  This register has no set reset value.
** It varies with the implementation.
*/

#define GC_CHIP_DATE_Address                                             0x00028
#define GC_CHIP_DATE_MSB                                                      15
#define GC_CHIP_DATE_LSB                                                       0
#define GC_CHIP_DATE_BLK                                                       0
#define GC_CHIP_DATE_Count                                                     1
#define GC_CHIP_DATE_FieldMask                                        0xFFFFFFFF
#define GC_CHIP_DATE_ReadMask                                         0xFFFFFFFF
#define GC_CHIP_DATE_WriteMask                                        0x00000000
#define GC_CHIP_DATE_ResetValue                                       0x00000000

/* Date. */
#define GC_CHIP_DATE_DATE                                                 31 : 0
#define GC_CHIP_DATE_DATE_End                                                 31
#define GC_CHIP_DATE_DATE_Start                                                0
#define GC_CHIP_DATE_DATE_Type                                               U32

/*******************************************************************************
** Register GCChipTime
*/

/* Shows the release time for the IP.  This register has no set reset value.
** It varies with the implementation.
*/

#define GC_CHIP_TIME_Address                                             0x0002C
#define GC_CHIP_TIME_MSB                                                      15
#define GC_CHIP_TIME_LSB                                                       0
#define GC_CHIP_TIME_BLK                                                       0
#define GC_CHIP_TIME_Count                                                     1
#define GC_CHIP_TIME_FieldMask                                        0xFFFFFFFF
#define GC_CHIP_TIME_ReadMask                                         0xFFFFFFFF
#define GC_CHIP_TIME_WriteMask                                        0x00000000
#define GC_CHIP_TIME_ResetValue                                       0x00000000

/* Time. */
#define GC_CHIP_TIME_TIME                                                 31 : 0
#define GC_CHIP_TIME_TIME_End                                                 31
#define GC_CHIP_TIME_TIME_Start                                                0
#define GC_CHIP_TIME_TIME_Type                                               U32

/*******************************************************************************
** Register GCMinorFeatures0
*/

/* Shows which minor features are enabled in this chip.  This register has no
** set reset value.  It varies with the implementation.
*/

#define GC_MINOR_FEATURES0_Address                                       0x00034
#define GC_MINOR_FEATURES0_MSB                                                15
#define GC_MINOR_FEATURES0_LSB                                                 0
#define GC_MINOR_FEATURES0_BLK                                                 0
#define GC_MINOR_FEATURES0_Count                                               1
#define GC_MINOR_FEATURES0_FieldMask                                  0xFFFFFFFF
#define GC_MINOR_FEATURES0_ReadMask                                   0xFFFFFFFF
#define GC_MINOR_FEATURES0_WriteMask                                  0x00000000
#define GC_MINOR_FEATURES0_ResetValue                                 0x00000000

/* Y flipping capability is added to resolve. */
#define GC_MINOR_FEATURES0_FLIP_Y                                          0 : 0
#define GC_MINOR_FEATURES0_FLIP_Y_End                                          0
#define GC_MINOR_FEATURES0_FLIP_Y_Start                                        0
#define GC_MINOR_FEATURES0_FLIP_Y_Type                                       U01
#define   GC_MINOR_FEATURES0_FLIP_Y_NONE                                     0x0
#define   GC_MINOR_FEATURES0_FLIP_Y_AVAILABLE                                0x1

/* Dual Return Bus from HI to clients. */
#define GC_MINOR_FEATURES0_DUAL_RETURN_BUS                                 1 : 1
#define GC_MINOR_FEATURES0_DUAL_RETURN_BUS_End                                 1
#define GC_MINOR_FEATURES0_DUAL_RETURN_BUS_Start                               1
#define GC_MINOR_FEATURES0_DUAL_RETURN_BUS_Type                              U01
#define   GC_MINOR_FEATURES0_DUAL_RETURN_BUS_NONE                            0x0
#define   GC_MINOR_FEATURES0_DUAL_RETURN_BUS_AVAILABLE                       0x1

/* Configurable endianness support. */
#define GC_MINOR_FEATURES0_ENDIANNESS_CONFIG                               2 : 2
#define GC_MINOR_FEATURES0_ENDIANNESS_CONFIG_End                               2
#define GC_MINOR_FEATURES0_ENDIANNESS_CONFIG_Start                             2
#define GC_MINOR_FEATURES0_ENDIANNESS_CONFIG_Type                            U01
#define   GC_MINOR_FEATURES0_ENDIANNESS_CONFIG_NONE                          0x0
#define   GC_MINOR_FEATURES0_ENDIANNESS_CONFIG_AVAILABLE                     0x1

/* Supports 8Kx8K textures. */
#define GC_MINOR_FEATURES0_TEXTURE8_K                                      3 : 3
#define GC_MINOR_FEATURES0_TEXTURE8_K_End                                      3
#define GC_MINOR_FEATURES0_TEXTURE8_K_Start                                    3
#define GC_MINOR_FEATURES0_TEXTURE8_K_Type                                   U01
#define   GC_MINOR_FEATURES0_TEXTURE8_K_NONE                                 0x0
#define   GC_MINOR_FEATURES0_TEXTURE8_K_AVAILABLE                            0x1

/* Driver hack is not needed. */
#define GC_MINOR_FEATURES0_CORRECT_TEXTURE_CONVERTER                       4 : 4
#define GC_MINOR_FEATURES0_CORRECT_TEXTURE_CONVERTER_End                       4
#define GC_MINOR_FEATURES0_CORRECT_TEXTURE_CONVERTER_Start                     4
#define GC_MINOR_FEATURES0_CORRECT_TEXTURE_CONVERTER_Type                    U01
#define   GC_MINOR_FEATURES0_CORRECT_TEXTURE_CONVERTER_NONE                  0x0
#define   GC_MINOR_FEATURES0_CORRECT_TEXTURE_CONVERTER_AVAILABLE             0x1

/* Special LOD calculation when MSAA is on. */
#define GC_MINOR_FEATURES0_SPECIAL_MSAA_LOD                                5 : 5
#define GC_MINOR_FEATURES0_SPECIAL_MSAA_LOD_End                                5
#define GC_MINOR_FEATURES0_SPECIAL_MSAA_LOD_Start                              5
#define GC_MINOR_FEATURES0_SPECIAL_MSAA_LOD_Type                             U01
#define   GC_MINOR_FEATURES0_SPECIAL_MSAA_LOD_NONE                           0x0
#define   GC_MINOR_FEATURES0_SPECIAL_MSAA_LOD_AVAILABLE                      0x1

/* Proper flush is done in fast clear cache. */
#define GC_MINOR_FEATURES0_FAST_CLEAR_FLUSH                                6 : 6
#define GC_MINOR_FEATURES0_FAST_CLEAR_FLUSH_End                                6
#define GC_MINOR_FEATURES0_FAST_CLEAR_FLUSH_Start                              6
#define GC_MINOR_FEATURES0_FAST_CLEAR_FLUSH_Type                             U01
#define   GC_MINOR_FEATURES0_FAST_CLEAR_FLUSH_NONE                           0x0
#define   GC_MINOR_FEATURES0_FAST_CLEAR_FLUSH_AVAILABLE                      0x1

/* 2D PE 2.0 is present. */
#define GC_MINOR_FEATURES0_2DPE20                                          7 : 7
#define GC_MINOR_FEATURES0_2DPE20_End                                          7
#define GC_MINOR_FEATURES0_2DPE20_Start                                        7
#define GC_MINOR_FEATURES0_2DPE20_Type                                       U01
#define   GC_MINOR_FEATURES0_2DPE20_NONE                                     0x0
#define   GC_MINOR_FEATURES0_2DPE20_AVAILABLE                                0x1

/* Reserved. */
#define GC_MINOR_FEATURES0_CORRECT_AUTO_DISABLE                            8 : 8
#define GC_MINOR_FEATURES0_CORRECT_AUTO_DISABLE_End                            8
#define GC_MINOR_FEATURES0_CORRECT_AUTO_DISABLE_Start                          8
#define GC_MINOR_FEATURES0_CORRECT_AUTO_DISABLE_Type                         U01
#define   GC_MINOR_FEATURES0_CORRECT_AUTO_DISABLE_NONE                       0x0
#define   GC_MINOR_FEATURES0_CORRECT_AUTO_DISABLE_AVAILABLE                  0x1

/* Supports 8K render target. */
#define GC_MINOR_FEATURES0_RENDER_8K                                       9 : 9
#define GC_MINOR_FEATURES0_RENDER_8K_End                                       9
#define GC_MINOR_FEATURES0_RENDER_8K_Start                                     9
#define GC_MINOR_FEATURES0_RENDER_8K_Type                                    U01
#define   GC_MINOR_FEATURES0_RENDER_8K_NONE                                  0x0
#define   GC_MINOR_FEATURES0_RENDER_8K_AVAILABLE                             0x1

/* 2 bits are used instead of 4 bits for tile status. */
#define GC_MINOR_FEATURES0_TILE_STATUS_2BITS                             10 : 10
#define GC_MINOR_FEATURES0_TILE_STATUS_2BITS_End                              10
#define GC_MINOR_FEATURES0_TILE_STATUS_2BITS_Start                            10
#define GC_MINOR_FEATURES0_TILE_STATUS_2BITS_Type                            U01
#define   GC_MINOR_FEATURES0_TILE_STATUS_2BITS_NONE                          0x0
#define   GC_MINOR_FEATURES0_TILE_STATUS_2BITS_AVAILABLE                     0x1

/* Use 2 separate tile status buffers in interleaved mode. */
#define GC_MINOR_FEATURES0_SEPARATE_TILE_STATUS_WHEN_INTERLEAVED         11 : 11
#define GC_MINOR_FEATURES0_SEPARATE_TILE_STATUS_WHEN_INTERLEAVED_End          11
#define GC_MINOR_FEATURES0_SEPARATE_TILE_STATUS_WHEN_INTERLEAVED_Start        11
#define GC_MINOR_FEATURES0_SEPARATE_TILE_STATUS_WHEN_INTERLEAVED_Type        U01
#define   GC_MINOR_FEATURES0_SEPARATE_TILE_STATUS_WHEN_INTERLEAVED_NONE      0x0
#define   GC_MINOR_FEATURES0_SEPARATE_TILE_STATUS_WHEN_INTERLEAVED_AVAILABLE 0x1

/* 32x32 super tile is available. */
#define GC_MINOR_FEATURES0_SUPER_TILED_32X32                             12 : 12
#define GC_MINOR_FEATURES0_SUPER_TILED_32X32_End                              12
#define GC_MINOR_FEATURES0_SUPER_TILED_32X32_Start                            12
#define GC_MINOR_FEATURES0_SUPER_TILED_32X32_Type                            U01
#define   GC_MINOR_FEATURES0_SUPER_TILED_32X32_NONE                          0x0
#define   GC_MINOR_FEATURES0_SUPER_TILED_32X32_AVAILABLE                     0x1

/* Major updates to VG pipe (TS buffer tiling. State masking.). */
#define GC_MINOR_FEATURES0_VG_20                                         13 : 13
#define GC_MINOR_FEATURES0_VG_20_End                                          13
#define GC_MINOR_FEATURES0_VG_20_Start                                        13
#define GC_MINOR_FEATURES0_VG_20_Type                                        U01
#define   GC_MINOR_FEATURES0_VG_20_NONE                                      0x0
#define   GC_MINOR_FEATURES0_VG_20_AVAILABLE                                 0x1

/* New commands added to the tessellator. */
#define GC_MINOR_FEATURES0_TS_EXTENDED_COMMANDS                          14 : 14
#define GC_MINOR_FEATURES0_TS_EXTENDED_COMMANDS_End                           14
#define GC_MINOR_FEATURES0_TS_EXTENDED_COMMANDS_Start                         14
#define GC_MINOR_FEATURES0_TS_EXTENDED_COMMANDS_Type                         U01
#define   GC_MINOR_FEATURES0_TS_EXTENDED_COMMANDS_NONE                       0x0
#define   GC_MINOR_FEATURES0_TS_EXTENDED_COMMANDS_AVAILABLE                  0x1

/* If this bit is not set, the FIFO counter should be set to 50.  Else, the   **
** default should remain.                                                     */
#define GC_MINOR_FEATURES0_COMPRESSION_FIFO_FIXED                        15 : 15
#define GC_MINOR_FEATURES0_COMPRESSION_FIFO_FIXED_End                         15
#define GC_MINOR_FEATURES0_COMPRESSION_FIFO_FIXED_Start                       15
#define GC_MINOR_FEATURES0_COMPRESSION_FIFO_FIXED_Type                       U01
#define   GC_MINOR_FEATURES0_COMPRESSION_FIFO_FIXED_NONE                     0x0
#define   GC_MINOR_FEATURES0_COMPRESSION_FIFO_FIXED_AVAILABLE                0x1

/* Floor, ceil, and sign instructions are available.  */
#define GC_MINOR_FEATURES0_EXTRA_SHADER_INSTRUCTIONS0                    16 : 16
#define GC_MINOR_FEATURES0_EXTRA_SHADER_INSTRUCTIONS0_End                     16
#define GC_MINOR_FEATURES0_EXTRA_SHADER_INSTRUCTIONS0_Start                   16
#define GC_MINOR_FEATURES0_EXTRA_SHADER_INSTRUCTIONS0_Type                   U01
#define   GC_MINOR_FEATURES0_EXTRA_SHADER_INSTRUCTIONS0_NONE                 0x0
#define   GC_MINOR_FEATURES0_EXTRA_SHADER_INSTRUCTIONS0_AVAILABLE            0x1

/* VG filter is available.  */
#define GC_MINOR_FEATURES0_VG_FILTER                                     17 : 17
#define GC_MINOR_FEATURES0_VG_FILTER_End                                      17
#define GC_MINOR_FEATURES0_VG_FILTER_Start                                    17
#define GC_MINOR_FEATURES0_VG_FILTER_Type                                    U01
#define   GC_MINOR_FEATURES0_VG_FILTER_NONE                                  0x0
#define   GC_MINOR_FEATURES0_VG_FILTER_AVAILABLE                             0x1

/* Minor updates to VG pipe (Event generation from VG, TS, PE). Tiled image   **
** support.                                                                   */
#define GC_MINOR_FEATURES0_VG_21                                         18 : 18
#define GC_MINOR_FEATURES0_VG_21_End                                          18
#define GC_MINOR_FEATURES0_VG_21_Start                                        18
#define GC_MINOR_FEATURES0_VG_21_Type                                        U01
#define   GC_MINOR_FEATURES0_VG_21_NONE                                      0x0
#define   GC_MINOR_FEATURES0_VG_21_AVAILABLE                                 0x1

/* W is sent to SH from RA. */
#define GC_MINOR_FEATURES0_SHADER_GETS_W                                 19 : 19
#define GC_MINOR_FEATURES0_SHADER_GETS_W_End                                  19
#define GC_MINOR_FEATURES0_SHADER_GETS_W_Start                                19
#define GC_MINOR_FEATURES0_SHADER_GETS_W_Type                                U01
#define   GC_MINOR_FEATURES0_SHADER_GETS_W_NONE                              0x0
#define   GC_MINOR_FEATURES0_SHADER_GETS_W_AVAILABLE                         0x1

/* Sqrt, sin, cos instructions are available.  */
#define GC_MINOR_FEATURES0_EXTRA_SHADER_INSTRUCTIONS1                    20 : 20
#define GC_MINOR_FEATURES0_EXTRA_SHADER_INSTRUCTIONS1_End                     20
#define GC_MINOR_FEATURES0_EXTRA_SHADER_INSTRUCTIONS1_Start                   20
#define GC_MINOR_FEATURES0_EXTRA_SHADER_INSTRUCTIONS1_Type                   U01
#define   GC_MINOR_FEATURES0_EXTRA_SHADER_INSTRUCTIONS1_NONE                 0x0
#define   GC_MINOR_FEATURES0_EXTRA_SHADER_INSTRUCTIONS1_AVAILABLE            0x1

/* Unavailable registers will return 0. */
#define GC_MINOR_FEATURES0_DEFAULT_REG0                                  21 : 21
#define GC_MINOR_FEATURES0_DEFAULT_REG0_End                                   21
#define GC_MINOR_FEATURES0_DEFAULT_REG0_Start                                 21
#define GC_MINOR_FEATURES0_DEFAULT_REG0_Type                                 U01
#define   GC_MINOR_FEATURES0_DEFAULT_REG0_NONE                               0x0
#define   GC_MINOR_FEATURES0_DEFAULT_REG0_AVAILABLE                          0x1

/* New style MC with separate paths for color and depth. */
#define GC_MINOR_FEATURES0_MC_20                                         22 : 22
#define GC_MINOR_FEATURES0_MC_20_End                                          22
#define GC_MINOR_FEATURES0_MC_20_Start                                        22
#define GC_MINOR_FEATURES0_MC_20_Type                                        U01
#define   GC_MINOR_FEATURES0_MC_20_NONE                                      0x0
#define   GC_MINOR_FEATURES0_MC_20_AVAILABLE                                 0x1

/* Put the MSAA data into sideband fifo. */
#define GC_MINOR_FEATURES0_SHADER_MSAA_SIDEBAND                          23 : 23
#define GC_MINOR_FEATURES0_SHADER_MSAA_SIDEBAND_End                           23
#define GC_MINOR_FEATURES0_SHADER_MSAA_SIDEBAND_Start                         23
#define GC_MINOR_FEATURES0_SHADER_MSAA_SIDEBAND_Type                         U01
#define   GC_MINOR_FEATURES0_SHADER_MSAA_SIDEBAND_NONE                       0x0
#define   GC_MINOR_FEATURES0_SHADER_MSAA_SIDEBAND_AVAILABLE                  0x1

#define GC_MINOR_FEATURES0_BUG_FIXES0                                    24 : 24
#define GC_MINOR_FEATURES0_BUG_FIXES0_End                                     24
#define GC_MINOR_FEATURES0_BUG_FIXES0_Start                                   24
#define GC_MINOR_FEATURES0_BUG_FIXES0_Type                                   U01
#define   GC_MINOR_FEATURES0_BUG_FIXES0_NONE                                 0x0
#define   GC_MINOR_FEATURES0_BUG_FIXES0_AVAILABLE                            0x1

/* VAA is available or not. */
#define GC_MINOR_FEATURES0_VAA                                           25 : 25
#define GC_MINOR_FEATURES0_VAA_End                                            25
#define GC_MINOR_FEATURES0_VAA_Start                                          25
#define GC_MINOR_FEATURES0_VAA_Type                                          U01
#define   GC_MINOR_FEATURES0_VAA_NONE                                        0x0
#define   GC_MINOR_FEATURES0_VAA_AVAILABLE                                   0x1

/* Shader supports bypass mode when MSAA is enabled. */
#define GC_MINOR_FEATURES0_BYPASS_IN_MSAA                                26 : 26
#define GC_MINOR_FEATURES0_BYPASS_IN_MSAA_End                                 26
#define GC_MINOR_FEATURES0_BYPASS_IN_MSAA_Start                               26
#define GC_MINOR_FEATURES0_BYPASS_IN_MSAA_Type                               U01
#define   GC_MINOR_FEATURES0_BYPASS_IN_MSAA_NONE                             0x0
#define   GC_MINOR_FEATURES0_BYPASS_IN_MSAA_AVAILABLE                        0x1

/* Hierarchiccal Z is supported. */
#define GC_MINOR_FEATURES0_HIERARCHICAL_Z                                27 : 27
#define GC_MINOR_FEATURES0_HIERARCHICAL_Z_End                                 27
#define GC_MINOR_FEATURES0_HIERARCHICAL_Z_Start                               27
#define GC_MINOR_FEATURES0_HIERARCHICAL_Z_Type                               U01
#define   GC_MINOR_FEATURES0_HIERARCHICAL_Z_NONE                             0x0
#define   GC_MINOR_FEATURES0_HIERARCHICAL_Z_AVAILABLE                        0x1

/* New texture unit is available. */
#define GC_MINOR_FEATURES0_NEW_TEXTURE                                   28 : 28
#define GC_MINOR_FEATURES0_NEW_TEXTURE_End                                    28
#define GC_MINOR_FEATURES0_NEW_TEXTURE_Start                                  28
#define GC_MINOR_FEATURES0_NEW_TEXTURE_Type                                  U01
#define   GC_MINOR_FEATURES0_NEW_TEXTURE_NONE                                0x0
#define   GC_MINOR_FEATURES0_NEW_TEXTURE_AVAILABLE                           0x1

/* 2D engine supports A8 target.  */
#define GC_MINOR_FEATURES0_A8_TARGET_SUPPORT                             29 : 29
#define GC_MINOR_FEATURES0_A8_TARGET_SUPPORT_End                              29
#define GC_MINOR_FEATURES0_A8_TARGET_SUPPORT_Start                            29
#define GC_MINOR_FEATURES0_A8_TARGET_SUPPORT_Type                            U01
#define   GC_MINOR_FEATURES0_A8_TARGET_SUPPORT_NONE                          0x0
#define   GC_MINOR_FEATURES0_A8_TARGET_SUPPORT_AVAILABLE                     0x1

/* Correct stencil behavior in depth only. */
#define GC_MINOR_FEATURES0_CORRECT_STENCIL                               30 : 30
#define GC_MINOR_FEATURES0_CORRECT_STENCIL_End                                30
#define GC_MINOR_FEATURES0_CORRECT_STENCIL_Start                              30
#define GC_MINOR_FEATURES0_CORRECT_STENCIL_Type                              U01
#define   GC_MINOR_FEATURES0_CORRECT_STENCIL_NONE                            0x0
#define   GC_MINOR_FEATURES0_CORRECT_STENCIL_AVAILABLE                       0x1

/* Enhance VR and add a mode to walk 16 pixels in 16-bit mode in Vertical     **
** pass to improve $ hit rate when rotating 90/270.                           */
#define GC_MINOR_FEATURES0_ENHANCE_VR                                    31 : 31
#define GC_MINOR_FEATURES0_ENHANCE_VR_End                                     31
#define GC_MINOR_FEATURES0_ENHANCE_VR_Start                                   31
#define GC_MINOR_FEATURES0_ENHANCE_VR_Type                                   U01
#define   GC_MINOR_FEATURES0_ENHANCE_VR_NONE                                 0x0
#define   GC_MINOR_FEATURES0_ENHANCE_VR_AVAILABLE                            0x1

/*******************************************************************************
** Register GCMinorFeatures1
*/

/* Shows which features are enabled in this chip. This register has no set
** reset value. It varies with the implementation.
*/

#define GC_MINOR_FEATURES1_Address                                       0x00074
#define GC_MINOR_FEATURES1_MSB                                                15
#define GC_MINOR_FEATURES1_LSB                                                 0
#define GC_MINOR_FEATURES1_BLK                                                 0
#define GC_MINOR_FEATURES1_Count                                               1
#define GC_MINOR_FEATURES1_FieldMask                                  0xFFFFFFFF
#define GC_MINOR_FEATURES1_ReadMask                                   0xFFFFFFFF
#define GC_MINOR_FEATURES1_WriteMask                                  0x00000000
#define GC_MINOR_FEATURES1_ResetValue                                 0x00000000

/* Resolve UV swizzle. */
#define GC_MINOR_FEATURES1_RSUV_SWIZZLE                                    0 : 0
#define GC_MINOR_FEATURES1_RSUV_SWIZZLE_End                                    0
#define GC_MINOR_FEATURES1_RSUV_SWIZZLE_Start                                  0
#define GC_MINOR_FEATURES1_RSUV_SWIZZLE_Type                                 U01
#define   GC_MINOR_FEATURES1_RSUV_SWIZZLE_NONE                               0x0
#define   GC_MINOR_FEATURES1_RSUV_SWIZZLE_AVAILABLE                          0x1

/* V2 compression. */
#define GC_MINOR_FEATURES1_V2_COMPRESSION                                  1 : 1
#define GC_MINOR_FEATURES1_V2_COMPRESSION_End                                  1
#define GC_MINOR_FEATURES1_V2_COMPRESSION_Start                                1
#define GC_MINOR_FEATURES1_V2_COMPRESSION_Type                               U01
#define   GC_MINOR_FEATURES1_V2_COMPRESSION_NONE                             0x0
#define   GC_MINOR_FEATURES1_V2_COMPRESSION_AVAILABLE                        0x1

/* Double buffering support for VG (second TS-->VG semaphore is present). */
#define GC_MINOR_FEATURES1_VG_DOUBLE_BUFFER                                2 : 2
#define GC_MINOR_FEATURES1_VG_DOUBLE_BUFFER_End                                2
#define GC_MINOR_FEATURES1_VG_DOUBLE_BUFFER_Start                              2
#define GC_MINOR_FEATURES1_VG_DOUBLE_BUFFER_Type                             U01
#define   GC_MINOR_FEATURES1_VG_DOUBLE_BUFFER_NONE                           0x0
#define   GC_MINOR_FEATURES1_VG_DOUBLE_BUFFER_AVAILABLE                      0x1

#define GC_MINOR_FEATURES1_BUG_FIXES1                                      3 : 3
#define GC_MINOR_FEATURES1_BUG_FIXES1_End                                      3
#define GC_MINOR_FEATURES1_BUG_FIXES1_Start                                    3
#define GC_MINOR_FEATURES1_BUG_FIXES1_Type                                   U01
#define   GC_MINOR_FEATURES1_BUG_FIXES1_NONE                                 0x0
#define   GC_MINOR_FEATURES1_BUG_FIXES1_AVAILABLE                            0x1

#define GC_MINOR_FEATURES1_BUG_FIXES2                                      4 : 4
#define GC_MINOR_FEATURES1_BUG_FIXES2_End                                      4
#define GC_MINOR_FEATURES1_BUG_FIXES2_Start                                    4
#define GC_MINOR_FEATURES1_BUG_FIXES2_Type                                   U01
#define   GC_MINOR_FEATURES1_BUG_FIXES2_NONE                                 0x0
#define   GC_MINOR_FEATURES1_BUG_FIXES2_AVAILABLE                            0x1

/* Texture has stride and memory addressing. */
#define GC_MINOR_FEATURES1_TEXTURE_STRIDE                                  5 : 5
#define GC_MINOR_FEATURES1_TEXTURE_STRIDE_End                                  5
#define GC_MINOR_FEATURES1_TEXTURE_STRIDE_Start                                5
#define GC_MINOR_FEATURES1_TEXTURE_STRIDE_Type                               U01
#define   GC_MINOR_FEATURES1_TEXTURE_STRIDE_NONE                             0x0
#define   GC_MINOR_FEATURES1_TEXTURE_STRIDE_AVAILABLE                        0x1

#define GC_MINOR_FEATURES1_BUG_FIXES3                                      6 : 6
#define GC_MINOR_FEATURES1_BUG_FIXES3_End                                      6
#define GC_MINOR_FEATURES1_BUG_FIXES3_Start                                    6
#define GC_MINOR_FEATURES1_BUG_FIXES3_Type                                   U01
#define   GC_MINOR_FEATURES1_BUG_FIXES3_NONE                                 0x0
#define   GC_MINOR_FEATURES1_BUG_FIXES3_AVAILABLE                            0x1

#define GC_MINOR_FEATURES1_CORRECT_AUTO_DISABLE                            7 : 7
#define GC_MINOR_FEATURES1_CORRECT_AUTO_DISABLE_End                            7
#define GC_MINOR_FEATURES1_CORRECT_AUTO_DISABLE_Start                          7
#define GC_MINOR_FEATURES1_CORRECT_AUTO_DISABLE_Type                         U01
#define   GC_MINOR_FEATURES1_CORRECT_AUTO_DISABLE_NONE                       0x0
#define   GC_MINOR_FEATURES1_CORRECT_AUTO_DISABLE_AVAILABLE                  0x1

#define GC_MINOR_FEATURES1_AUTO_RESTART_TS                                 8 : 8
#define GC_MINOR_FEATURES1_AUTO_RESTART_TS_End                                 8
#define GC_MINOR_FEATURES1_AUTO_RESTART_TS_Start                               8
#define GC_MINOR_FEATURES1_AUTO_RESTART_TS_Type                              U01
#define   GC_MINOR_FEATURES1_AUTO_RESTART_TS_NONE                            0x0
#define   GC_MINOR_FEATURES1_AUTO_RESTART_TS_AVAILABLE                       0x1

#define GC_MINOR_FEATURES1_BUG_FIXES4                                      9 : 9
#define GC_MINOR_FEATURES1_BUG_FIXES4_End                                      9
#define GC_MINOR_FEATURES1_BUG_FIXES4_Start                                    9
#define GC_MINOR_FEATURES1_BUG_FIXES4_Type                                   U01
#define   GC_MINOR_FEATURES1_BUG_FIXES4_NONE                                 0x0
#define   GC_MINOR_FEATURES1_BUG_FIXES4_AVAILABLE                            0x1

#define GC_MINOR_FEATURES1_L2_WINDOWING                                  10 : 10
#define GC_MINOR_FEATURES1_L2_WINDOWING_End                                   10
#define GC_MINOR_FEATURES1_L2_WINDOWING_Start                                 10
#define GC_MINOR_FEATURES1_L2_WINDOWING_Type                                 U01
#define   GC_MINOR_FEATURES1_L2_WINDOWING_NONE                               0x0
#define   GC_MINOR_FEATURES1_L2_WINDOWING_AVAILABLE                          0x1

#define GC_MINOR_FEATURES1_HALF_FLOAT_PIPE                               11 : 11
#define GC_MINOR_FEATURES1_HALF_FLOAT_PIPE_End                                11
#define GC_MINOR_FEATURES1_HALF_FLOAT_PIPE_Start                              11
#define GC_MINOR_FEATURES1_HALF_FLOAT_PIPE_Type                              U01
#define   GC_MINOR_FEATURES1_HALF_FLOAT_PIPE_NONE                            0x0
#define   GC_MINOR_FEATURES1_HALF_FLOAT_PIPE_AVAILABLE                       0x1

#define GC_MINOR_FEATURES1_PIXEL_DITHER                                  12 : 12
#define GC_MINOR_FEATURES1_PIXEL_DITHER_End                                   12
#define GC_MINOR_FEATURES1_PIXEL_DITHER_Start                                 12
#define GC_MINOR_FEATURES1_PIXEL_DITHER_Type                                 U01
#define   GC_MINOR_FEATURES1_PIXEL_DITHER_NONE                               0x0
#define   GC_MINOR_FEATURES1_PIXEL_DITHER_AVAILABLE                          0x1

#define GC_MINOR_FEATURES1_TWO_STENCIL_REFERENCE                         13 : 13
#define GC_MINOR_FEATURES1_TWO_STENCIL_REFERENCE_End                          13
#define GC_MINOR_FEATURES1_TWO_STENCIL_REFERENCE_Start                        13
#define GC_MINOR_FEATURES1_TWO_STENCIL_REFERENCE_Type                        U01
#define   GC_MINOR_FEATURES1_TWO_STENCIL_REFERENCE_NONE                      0x0
#define   GC_MINOR_FEATURES1_TWO_STENCIL_REFERENCE_AVAILABLE                 0x1

#define GC_MINOR_FEATURES1_EXTENDED_PIXEL_FORMAT                         14 : 14
#define GC_MINOR_FEATURES1_EXTENDED_PIXEL_FORMAT_End                          14
#define GC_MINOR_FEATURES1_EXTENDED_PIXEL_FORMAT_Start                        14
#define GC_MINOR_FEATURES1_EXTENDED_PIXEL_FORMAT_Type                        U01
#define   GC_MINOR_FEATURES1_EXTENDED_PIXEL_FORMAT_NONE                      0x0
#define   GC_MINOR_FEATURES1_EXTENDED_PIXEL_FORMAT_AVAILABLE                 0x1

/* EEZ and HZ are correct. */
#define GC_MINOR_FEATURES1_CORRECT_MIN_MAX_DEPTH                         15 : 15
#define GC_MINOR_FEATURES1_CORRECT_MIN_MAX_DEPTH_End                          15
#define GC_MINOR_FEATURES1_CORRECT_MIN_MAX_DEPTH_Start                        15
#define GC_MINOR_FEATURES1_CORRECT_MIN_MAX_DEPTH_Type                        U01
#define   GC_MINOR_FEATURES1_CORRECT_MIN_MAX_DEPTH_NONE                      0x0
#define   GC_MINOR_FEATURES1_CORRECT_MIN_MAX_DEPTH_AVAILABLE                 0x1

/* Dither and filter+alpha available. */
#define GC_MINOR_FEATURES1_DITHER_AND_FILTER_PLUS_ALPHA_2D               16 : 16
#define GC_MINOR_FEATURES1_DITHER_AND_FILTER_PLUS_ALPHA_2D_End                16
#define GC_MINOR_FEATURES1_DITHER_AND_FILTER_PLUS_ALPHA_2D_Start              16
#define GC_MINOR_FEATURES1_DITHER_AND_FILTER_PLUS_ALPHA_2D_Type              U01
#define   GC_MINOR_FEATURES1_DITHER_AND_FILTER_PLUS_ALPHA_2D_NONE            0x0
#define   GC_MINOR_FEATURES1_DITHER_AND_FILTER_PLUS_ALPHA_2D_AVAILABLE       0x1

#define GC_MINOR_FEATURES1_BUG_FIXES5                                    17 : 17
#define GC_MINOR_FEATURES1_BUG_FIXES5_End                                     17
#define GC_MINOR_FEATURES1_BUG_FIXES5_Start                                   17
#define GC_MINOR_FEATURES1_BUG_FIXES5_Type                                   U01
#define   GC_MINOR_FEATURES1_BUG_FIXES5_NONE                                 0x0
#define   GC_MINOR_FEATURES1_BUG_FIXES5_AVAILABLE                            0x1

#define GC_MINOR_FEATURES1_NEW_2D                                        18 : 18
#define GC_MINOR_FEATURES1_NEW_2D_End                                         18
#define GC_MINOR_FEATURES1_NEW_2D_Start                                       18
#define GC_MINOR_FEATURES1_NEW_2D_Type                                       U01
#define   GC_MINOR_FEATURES1_NEW_2D_NONE                                     0x0
#define   GC_MINOR_FEATURES1_NEW_2D_AVAILABLE                                0x1

#define GC_MINOR_FEATURES1_NEW_FLOATING_POINT_ARITHMETIC                 19 : 19
#define GC_MINOR_FEATURES1_NEW_FLOATING_POINT_ARITHMETIC_End                  19
#define GC_MINOR_FEATURES1_NEW_FLOATING_POINT_ARITHMETIC_Start                19
#define GC_MINOR_FEATURES1_NEW_FLOATING_POINT_ARITHMETIC_Type                U01
#define   GC_MINOR_FEATURES1_NEW_FLOATING_POINT_ARITHMETIC_NONE              0x0
#define   GC_MINOR_FEATURES1_NEW_FLOATING_POINT_ARITHMETIC_AVAILABLE         0x1

#define GC_MINOR_FEATURES1_TEXTURE_HORIZONTAL_ALIGNMENT_SELECT           20 : 20
#define GC_MINOR_FEATURES1_TEXTURE_HORIZONTAL_ALIGNMENT_SELECT_End            20
#define GC_MINOR_FEATURES1_TEXTURE_HORIZONTAL_ALIGNMENT_SELECT_Start          20
#define GC_MINOR_FEATURES1_TEXTURE_HORIZONTAL_ALIGNMENT_SELECT_Type          U01
#define   GC_MINOR_FEATURES1_TEXTURE_HORIZONTAL_ALIGNMENT_SELECT_NONE        0x0
#define   GC_MINOR_FEATURES1_TEXTURE_HORIZONTAL_ALIGNMENT_SELECT_AVAILABLE   0x1

#define GC_MINOR_FEATURES1_NON_POWER_OF_TWO                              21 : 21
#define GC_MINOR_FEATURES1_NON_POWER_OF_TWO_End                               21
#define GC_MINOR_FEATURES1_NON_POWER_OF_TWO_Start                             21
#define GC_MINOR_FEATURES1_NON_POWER_OF_TWO_Type                             U01
#define   GC_MINOR_FEATURES1_NON_POWER_OF_TWO_NONE                           0x0
#define   GC_MINOR_FEATURES1_NON_POWER_OF_TWO_AVAILABLE                      0x1

#define GC_MINOR_FEATURES1_LINEAR_TEXTURE_SUPPORT                        22 : 22
#define GC_MINOR_FEATURES1_LINEAR_TEXTURE_SUPPORT_End                         22
#define GC_MINOR_FEATURES1_LINEAR_TEXTURE_SUPPORT_Start                       22
#define GC_MINOR_FEATURES1_LINEAR_TEXTURE_SUPPORT_Type                       U01
#define   GC_MINOR_FEATURES1_LINEAR_TEXTURE_SUPPORT_NONE                     0x0
#define   GC_MINOR_FEATURES1_LINEAR_TEXTURE_SUPPORT_AVAILABLE                0x1

#define GC_MINOR_FEATURES1_HALTI0                                        23 : 23
#define GC_MINOR_FEATURES1_HALTI0_End                                         23
#define GC_MINOR_FEATURES1_HALTI0_Start                                       23
#define GC_MINOR_FEATURES1_HALTI0_Type                                       U01
#define   GC_MINOR_FEATURES1_HALTI0_NONE                                     0x0
#define   GC_MINOR_FEATURES1_HALTI0_AVAILABLE                                0x1

#define GC_MINOR_FEATURES1_CORRECT_OVERFLOW_VG                           24 : 24
#define GC_MINOR_FEATURES1_CORRECT_OVERFLOW_VG_End                            24
#define GC_MINOR_FEATURES1_CORRECT_OVERFLOW_VG_Start                          24
#define GC_MINOR_FEATURES1_CORRECT_OVERFLOW_VG_Type                          U01
#define   GC_MINOR_FEATURES1_CORRECT_OVERFLOW_VG_NONE                        0x0
#define   GC_MINOR_FEATURES1_CORRECT_OVERFLOW_VG_AVAILABLE                   0x1

#define GC_MINOR_FEATURES1_NEGATIVE_LOG_FIX                              25 : 25
#define GC_MINOR_FEATURES1_NEGATIVE_LOG_FIX_End                               25
#define GC_MINOR_FEATURES1_NEGATIVE_LOG_FIX_Start                             25
#define GC_MINOR_FEATURES1_NEGATIVE_LOG_FIX_Type                             U01
#define   GC_MINOR_FEATURES1_NEGATIVE_LOG_FIX_NONE                           0x0
#define   GC_MINOR_FEATURES1_NEGATIVE_LOG_FIX_AVAILABLE                      0x1

#define GC_MINOR_FEATURES1_RESOLVE_OFFSET                                26 : 26
#define GC_MINOR_FEATURES1_RESOLVE_OFFSET_End                                 26
#define GC_MINOR_FEATURES1_RESOLVE_OFFSET_Start                               26
#define GC_MINOR_FEATURES1_RESOLVE_OFFSET_Type                               U01
#define   GC_MINOR_FEATURES1_RESOLVE_OFFSET_NONE                             0x0
#define   GC_MINOR_FEATURES1_RESOLVE_OFFSET_AVAILABLE                        0x1

#define GC_MINOR_FEATURES1_OK_TO_GATE_AXI_CLOCK                          27 : 27
#define GC_MINOR_FEATURES1_OK_TO_GATE_AXI_CLOCK_End                           27
#define GC_MINOR_FEATURES1_OK_TO_GATE_AXI_CLOCK_Start                         27
#define GC_MINOR_FEATURES1_OK_TO_GATE_AXI_CLOCK_Type                         U01
#define   GC_MINOR_FEATURES1_OK_TO_GATE_AXI_CLOCK_NONE                       0x0
#define   GC_MINOR_FEATURES1_OK_TO_GATE_AXI_CLOCK_AVAILABLE                  0x1

#define GC_MINOR_FEATURES1_MMU                                           28 : 28
#define GC_MINOR_FEATURES1_MMU_End                                            28
#define GC_MINOR_FEATURES1_MMU_Start                                          28
#define GC_MINOR_FEATURES1_MMU_Type                                          U01
#define   GC_MINOR_FEATURES1_MMU_NONE                                        0x0
#define   GC_MINOR_FEATURES1_MMU_AVAILABLE                                   0x1

#define GC_MINOR_FEATURES1_WIDE_LINE                                     29 : 29
#define GC_MINOR_FEATURES1_WIDE_LINE_End                                      29
#define GC_MINOR_FEATURES1_WIDE_LINE_Start                                    29
#define GC_MINOR_FEATURES1_WIDE_LINE_Type                                    U01
#define   GC_MINOR_FEATURES1_WIDE_LINE_NONE                                  0x0
#define   GC_MINOR_FEATURES1_WIDE_LINE_AVAILABLE                             0x1

#define GC_MINOR_FEATURES1_BUG_FIXES6                                    30 : 30
#define GC_MINOR_FEATURES1_BUG_FIXES6_End                                     30
#define GC_MINOR_FEATURES1_BUG_FIXES6_Start                                   30
#define GC_MINOR_FEATURES1_BUG_FIXES6_Type                                   U01
#define   GC_MINOR_FEATURES1_BUG_FIXES6_NONE                                 0x0
#define   GC_MINOR_FEATURES1_BUG_FIXES6_AVAILABLE                            0x1

#define GC_MINOR_FEATURES1_FC_FLUSH_STALL                                31 : 31
#define GC_MINOR_FEATURES1_FC_FLUSH_STALL_End                                 31
#define GC_MINOR_FEATURES1_FC_FLUSH_STALL_Start                               31
#define GC_MINOR_FEATURES1_FC_FLUSH_STALL_Type                               U01
#define   GC_MINOR_FEATURES1_FC_FLUSH_STALL_NONE                             0x0
#define   GC_MINOR_FEATURES1_FC_FLUSH_STALL_AVAILABLE                        0x1

/*******************************************************************************
** Register GCResetMemCounters
*/

/* Writing 1 will reset the counters and stop counting. Write 0 to start
** counting again.  This register is write only so it has no reset value.
*/

#define GC_RESET_MEM_COUNTERS_Address                                    0x0003C
#define GC_RESET_MEM_COUNTERS_MSB                                             15
#define GC_RESET_MEM_COUNTERS_LSB                                              0
#define GC_RESET_MEM_COUNTERS_BLK                                              0
#define GC_RESET_MEM_COUNTERS_Count                                            1
#define GC_RESET_MEM_COUNTERS_FieldMask                               0x00000001
#define GC_RESET_MEM_COUNTERS_ReadMask                                0x00000000
#define GC_RESET_MEM_COUNTERS_WriteMask                               0x00000001
#define GC_RESET_MEM_COUNTERS_ResetValue                              0x00000000

#define GC_RESET_MEM_COUNTERS_RESET                                        0 : 0
#define GC_RESET_MEM_COUNTERS_RESET_End                                        0
#define GC_RESET_MEM_COUNTERS_RESET_Start                                      0
#define GC_RESET_MEM_COUNTERS_RESET_Type                                     U01

/*******************************************************************************
** Register gcTotalReads
*/

/* Total reads in terms of 64bits. */

#define GC_TOTAL_READS_Address                                           0x00040
#define GC_TOTAL_READS_MSB                                                    15
#define GC_TOTAL_READS_LSB                                                     0
#define GC_TOTAL_READS_BLK                                                     0
#define GC_TOTAL_READS_Count                                                   1
#define GC_TOTAL_READS_FieldMask                                      0xFFFFFFFF
#define GC_TOTAL_READS_ReadMask                                       0xFFFFFFFF
#define GC_TOTAL_READS_WriteMask                                      0x00000000
#define GC_TOTAL_READS_ResetValue                                     0x00000000

#define GC_TOTAL_READS_COUNT                                              31 : 0
#define GC_TOTAL_READS_COUNT_End                                              31
#define GC_TOTAL_READS_COUNT_Start                                             0
#define GC_TOTAL_READS_COUNT_Type                                            U32

/*******************************************************************************
** Register gcTotalWrites
*/

/* Total writes in terms of 64bits. */

#define GC_TOTAL_WRITES_Address                                          0x00044
#define GC_TOTAL_WRITES_MSB                                                   15
#define GC_TOTAL_WRITES_LSB                                                    0
#define GC_TOTAL_WRITES_BLK                                                    0
#define GC_TOTAL_WRITES_Count                                                  1
#define GC_TOTAL_WRITES_FieldMask                                     0xFFFFFFFF
#define GC_TOTAL_WRITES_ReadMask                                      0xFFFFFFFF
#define GC_TOTAL_WRITES_WriteMask                                     0x00000000
#define GC_TOTAL_WRITES_ResetValue                                    0x00000000

#define GC_TOTAL_WRITES_COUNT                                             31 : 0
#define GC_TOTAL_WRITES_COUNT_End                                             31
#define GC_TOTAL_WRITES_COUNT_Start                                            0
#define GC_TOTAL_WRITES_COUNT_Type                                           U32

/*******************************************************************************
** Register gcTotalWriteBursts
*/

/* Total write Data Count in terms of 64bits value. */

#define GC_TOTAL_WRITE_BURSTS_Address                                    0x0004C
#define GC_TOTAL_WRITE_BURSTS_MSB                                             15
#define GC_TOTAL_WRITE_BURSTS_LSB                                              0
#define GC_TOTAL_WRITE_BURSTS_BLK                                              0
#define GC_TOTAL_WRITE_BURSTS_Count                                            1
#define GC_TOTAL_WRITE_BURSTS_FieldMask                               0xFFFFFFFF
#define GC_TOTAL_WRITE_BURSTS_ReadMask                                0xFFFFFFFF
#define GC_TOTAL_WRITE_BURSTS_WriteMask                               0x00000000
#define GC_TOTAL_WRITE_BURSTS_ResetValue                              0x00000000

#define GC_TOTAL_WRITE_BURSTS_COUNT                                       31 : 0
#define GC_TOTAL_WRITE_BURSTS_COUNT_End                                       31
#define GC_TOTAL_WRITE_BURSTS_COUNT_Start                                      0
#define GC_TOTAL_WRITE_BURSTS_COUNT_Type                                     U32

/*******************************************************************************
** Register gcTotalWriteReqs
*/

/* Total write Request Count. */

#define GC_TOTAL_WRITE_REQS_Address                                      0x00050
#define GC_TOTAL_WRITE_REQS_MSB                                               15
#define GC_TOTAL_WRITE_REQS_LSB                                                0
#define GC_TOTAL_WRITE_REQS_BLK                                                0
#define GC_TOTAL_WRITE_REQS_Count                                              1
#define GC_TOTAL_WRITE_REQS_FieldMask                                 0xFFFFFFFF
#define GC_TOTAL_WRITE_REQS_ReadMask                                  0xFFFFFFFF
#define GC_TOTAL_WRITE_REQS_WriteMask                                 0x00000000
#define GC_TOTAL_WRITE_REQS_ResetValue                                0x00000000

#define GC_TOTAL_WRITE_REQS_COUNT                                         31 : 0
#define GC_TOTAL_WRITE_REQS_COUNT_End                                         31
#define GC_TOTAL_WRITE_REQS_COUNT_Start                                        0
#define GC_TOTAL_WRITE_REQS_COUNT_Type                                       U32

/*******************************************************************************
** Register gcTotalReadBursts
*/

/* Total Read Data Count in terms of 64bits. */

#define GC_TOTAL_READ_BURSTS_Address                                     0x00058
#define GC_TOTAL_READ_BURSTS_MSB                                              15
#define GC_TOTAL_READ_BURSTS_LSB                                               0
#define GC_TOTAL_READ_BURSTS_BLK                                               0
#define GC_TOTAL_READ_BURSTS_Count                                             1
#define GC_TOTAL_READ_BURSTS_FieldMask                                0xFFFFFFFF
#define GC_TOTAL_READ_BURSTS_ReadMask                                 0xFFFFFFFF
#define GC_TOTAL_READ_BURSTS_WriteMask                                0x00000000
#define GC_TOTAL_READ_BURSTS_ResetValue                               0x00000000

#define GC_TOTAL_READ_BURSTS_COUNT                                        31 : 0
#define GC_TOTAL_READ_BURSTS_COUNT_End                                        31
#define GC_TOTAL_READ_BURSTS_COUNT_Start                                       0
#define GC_TOTAL_READ_BURSTS_COUNT_Type                                      U32

/*******************************************************************************
** Register gcTotalReadReqs
*/

/* Total Read Request Count. */

#define GC_TOTAL_READ_REQS_Address                                       0x0005C
#define GC_TOTAL_READ_REQS_MSB                                                15
#define GC_TOTAL_READ_REQS_LSB                                                 0
#define GC_TOTAL_READ_REQS_BLK                                                 0
#define GC_TOTAL_READ_REQS_Count                                               1
#define GC_TOTAL_READ_REQS_FieldMask                                  0xFFFFFFFF
#define GC_TOTAL_READ_REQS_ReadMask                                   0xFFFFFFFF
#define GC_TOTAL_READ_REQS_WriteMask                                  0x00000000
#define GC_TOTAL_READ_REQS_ResetValue                                 0x00000000

#define GC_TOTAL_READ_REQS_COUNT                                          31 : 0
#define GC_TOTAL_READ_REQS_COUNT_End                                          31
#define GC_TOTAL_READ_REQS_COUNT_Start                                         0
#define GC_TOTAL_READ_REQS_COUNT_Type                                        U32

/*******************************************************************************
** Register gcTotalReadLasts
*/

/* Total RLAST Count. This is used to match with GCTotalReadReqs. */

#define GC_TOTAL_READ_LASTS_Address                                      0x00060
#define GC_TOTAL_READ_LASTS_MSB                                               15
#define GC_TOTAL_READ_LASTS_LSB                                                0
#define GC_TOTAL_READ_LASTS_BLK                                                0
#define GC_TOTAL_READ_LASTS_Count                                              1
#define GC_TOTAL_READ_LASTS_FieldMask                                 0xFFFFFFFF
#define GC_TOTAL_READ_LASTS_ReadMask                                  0xFFFFFFFF
#define GC_TOTAL_READ_LASTS_WriteMask                                 0x00000000
#define GC_TOTAL_READ_LASTS_ResetValue                                0x00000000

#define GC_TOTAL_READ_LASTS_COUNT                                         31 : 0
#define GC_TOTAL_READ_LASTS_COUNT_End                                         31
#define GC_TOTAL_READ_LASTS_COUNT_Start                                        0
#define GC_TOTAL_READ_LASTS_COUNT_Type                                       U32

/*******************************************************************************
** Register gcGpOut0
*/

/* General Purpose output register0. R/W but not connected to anywhere. */

#define GC_GP_OUT0_Address                                               0x00064
#define GC_GP_OUT0_MSB                                                        15
#define GC_GP_OUT0_LSB                                                         0
#define GC_GP_OUT0_BLK                                                         0
#define GC_GP_OUT0_Count                                                       1
#define GC_GP_OUT0_FieldMask                                          0xFFFFFFFF
#define GC_GP_OUT0_ReadMask                                           0xFFFFFFFF
#define GC_GP_OUT0_WriteMask                                          0xFFFFFFFF
#define GC_GP_OUT0_ResetValue                                         0x00000000

#define GC_GP_OUT0_COUNT                                                  31 : 0
#define GC_GP_OUT0_COUNT_End                                                  31
#define GC_GP_OUT0_COUNT_Start                                                 0
#define GC_GP_OUT0_COUNT_Type                                                U32

/*******************************************************************************
** Register gcGpOut1
*/

/* General Purpose output register1. R/W but not connected to anywhere. */

#define GC_GP_OUT1_Address                                               0x00068
#define GC_GP_OUT1_MSB                                                        15
#define GC_GP_OUT1_LSB                                                         0
#define GC_GP_OUT1_BLK                                                         0
#define GC_GP_OUT1_Count                                                       1
#define GC_GP_OUT1_FieldMask                                          0xFFFFFFFF
#define GC_GP_OUT1_ReadMask                                           0xFFFFFFFF
#define GC_GP_OUT1_WriteMask                                          0xFFFFFFFF
#define GC_GP_OUT1_ResetValue                                         0x00000000

#define GC_GP_OUT1_COUNT                                                  31 : 0
#define GC_GP_OUT1_COUNT_End                                                  31
#define GC_GP_OUT1_COUNT_Start                                                 0
#define GC_GP_OUT1_COUNT_Type                                                U32

/*******************************************************************************
** Register gcGpOut2
*/

/* General Purpose output register2. R/W but not connected to anywhere. */

#define GC_GP_OUT2_Address                                               0x0006C
#define GC_GP_OUT2_MSB                                                        15
#define GC_GP_OUT2_LSB                                                         0
#define GC_GP_OUT2_BLK                                                         0
#define GC_GP_OUT2_Count                                                       1
#define GC_GP_OUT2_FieldMask                                          0xFFFFFFFF
#define GC_GP_OUT2_ReadMask                                           0xFFFFFFFF
#define GC_GP_OUT2_WriteMask                                          0xFFFFFFFF
#define GC_GP_OUT2_ResetValue                                         0x00000000

#define GC_GP_OUT2_COUNT                                                  31 : 0
#define GC_GP_OUT2_COUNT_End                                                  31
#define GC_GP_OUT2_COUNT_Start                                                 0
#define GC_GP_OUT2_COUNT_Type                                                U32

/*******************************************************************************
** Register gcAxiControl
*/

/* Special Handling on AXI Bus */

#define GC_AXI_CONTROL_Address                                           0x00070
#define GC_AXI_CONTROL_MSB                                                    15
#define GC_AXI_CONTROL_LSB                                                     0
#define GC_AXI_CONTROL_BLK                                                     0
#define GC_AXI_CONTROL_Count                                                   1
#define GC_AXI_CONTROL_FieldMask                                      0x00000001
#define GC_AXI_CONTROL_ReadMask                                       0x00000001
#define GC_AXI_CONTROL_WriteMask                                      0x00000001
#define GC_AXI_CONTROL_ResetValue                                     0x00000000

#define GC_AXI_CONTROL_WR_FULL_BURST_MODE                                  0 : 0
#define GC_AXI_CONTROL_WR_FULL_BURST_MODE_End                                  0
#define GC_AXI_CONTROL_WR_FULL_BURST_MODE_Start                                0
#define GC_AXI_CONTROL_WR_FULL_BURST_MODE_Type                               U01
#define   GC_AXI_CONTROL_WR_FULL_BURST_MODE_NO_BURST_RESET_VALUE             0x0
#define   GC_AXI_CONTROL_WR_FULL_BURST_MODE_BURST_RESET_VALUE                0x1

/*******************************************************************************
** Register GCMinorFeatures1
*/

/* Shows which features are enabled in this chip. This register has no set
   reset value. It varies with the implementation. */

#define GC_MINOR_FEATURES1_Address                                       0x00074
#define GC_MINOR_FEATURES1_MSB                                                15
#define GC_MINOR_FEATURES1_LSB                                                 0
#define GC_MINOR_FEATURES1_BLK                                                 0
#define GC_MINOR_FEATURES1_Count                                               1
#define GC_MINOR_FEATURES1_FieldMask                                  0xFFFFFFFF
#define GC_MINOR_FEATURES1_ReadMask                                   0xFFFFFFFF
#define GC_MINOR_FEATURES1_WriteMask                                  0x00000000
#define GC_MINOR_FEATURES1_ResetValue                                 0x00000000

/* Resolve UV swizzle. */
#define GC_MINOR_FEATURES1_RSUV_SWIZZLE                                    0 : 0
#define GC_MINOR_FEATURES1_RSUV_SWIZZLE_End                                    0
#define GC_MINOR_FEATURES1_RSUV_SWIZZLE_Start                                  0
#define GC_MINOR_FEATURES1_RSUV_SWIZZLE_Type                                 U01
#define   GC_MINOR_FEATURES1_RSUV_SWIZZLE_NONE                               0x0
#define   GC_MINOR_FEATURES1_RSUV_SWIZZLE_AVAILABLE                          0x1

/* V2 compression. */
#define GC_MINOR_FEATURES1_V2_COMPRESSION                                  1 : 1
#define GC_MINOR_FEATURES1_V2_COMPRESSION_End                                  1
#define GC_MINOR_FEATURES1_V2_COMPRESSION_Start                                1
#define GC_MINOR_FEATURES1_V2_COMPRESSION_Type                               U01
#define   GC_MINOR_FEATURES1_V2_COMPRESSION_NONE                             0x0
#define   GC_MINOR_FEATURES1_V2_COMPRESSION_AVAILABLE                        0x1

/* Double buffering support for VG (second TS-->VG semaphore is present). */
#define GC_MINOR_FEATURES1_VG_DOUBLE_BUFFER                                2 : 2
#define GC_MINOR_FEATURES1_VG_DOUBLE_BUFFER_End                                2
#define GC_MINOR_FEATURES1_VG_DOUBLE_BUFFER_Start                              2
#define GC_MINOR_FEATURES1_VG_DOUBLE_BUFFER_Type                             U01
#define   GC_MINOR_FEATURES1_VG_DOUBLE_BUFFER_NONE                           0x0
#define   GC_MINOR_FEATURES1_VG_DOUBLE_BUFFER_AVAILABLE                      0x1

#define GC_MINOR_FEATURES1_BUG_FIXES1                                      3 : 3
#define GC_MINOR_FEATURES1_BUG_FIXES1_End                                      3
#define GC_MINOR_FEATURES1_BUG_FIXES1_Start                                    3
#define GC_MINOR_FEATURES1_BUG_FIXES1_Type                                   U01
#define   GC_MINOR_FEATURES1_BUG_FIXES1_NONE                                 0x0
#define   GC_MINOR_FEATURES1_BUG_FIXES1_AVAILABLE                            0x1

#define GC_MINOR_FEATURES1_BUG_FIXES2                                      4 : 4
#define GC_MINOR_FEATURES1_BUG_FIXES2_End                                      4
#define GC_MINOR_FEATURES1_BUG_FIXES2_Start                                    4
#define GC_MINOR_FEATURES1_BUG_FIXES2_Type                                   U01
#define   GC_MINOR_FEATURES1_BUG_FIXES2_NONE                                 0x0
#define   GC_MINOR_FEATURES1_BUG_FIXES2_AVAILABLE                            0x1

/* Texture has stride and memory addressing. */
#define GC_MINOR_FEATURES1_TEXTURE_STRIDE                                  5 : 5
#define GC_MINOR_FEATURES1_TEXTURE_STRIDE_End                                  5
#define GC_MINOR_FEATURES1_TEXTURE_STRIDE_Start                                5
#define GC_MINOR_FEATURES1_TEXTURE_STRIDE_Type                               U01
#define   GC_MINOR_FEATURES1_TEXTURE_STRIDE_NONE                             0x0
#define   GC_MINOR_FEATURES1_TEXTURE_STRIDE_AVAILABLE                        0x1

#define GC_MINOR_FEATURES1_BUG_FIXES3                                      6 : 6
#define GC_MINOR_FEATURES1_BUG_FIXES3_End                                      6
#define GC_MINOR_FEATURES1_BUG_FIXES3_Start                                    6
#define GC_MINOR_FEATURES1_BUG_FIXES3_Type                                   U01
#define   GC_MINOR_FEATURES1_BUG_FIXES3_NONE                                 0x0
#define   GC_MINOR_FEATURES1_BUG_FIXES3_AVAILABLE                            0x1

#define GC_MINOR_FEATURES1_CORRECT_AUTO_DISABLE                            7 : 7
#define GC_MINOR_FEATURES1_CORRECT_AUTO_DISABLE_End                            7
#define GC_MINOR_FEATURES1_CORRECT_AUTO_DISABLE_Start                          7
#define GC_MINOR_FEATURES1_CORRECT_AUTO_DISABLE_Type                         U01
#define   GC_MINOR_FEATURES1_CORRECT_AUTO_DISABLE_NONE                       0x0
#define   GC_MINOR_FEATURES1_CORRECT_AUTO_DISABLE_AVAILABLE                  0x1

#define GC_MINOR_FEATURES1_AUTO_RESTART_TS                                 8 : 8
#define GC_MINOR_FEATURES1_AUTO_RESTART_TS_End                                 8
#define GC_MINOR_FEATURES1_AUTO_RESTART_TS_Start                               8
#define GC_MINOR_FEATURES1_AUTO_RESTART_TS_Type                              U01
#define   GC_MINOR_FEATURES1_AUTO_RESTART_TS_NONE                            0x0
#define   GC_MINOR_FEATURES1_AUTO_RESTART_TS_AVAILABLE                       0x1

#define GC_MINOR_FEATURES1_BUG_FIXES4                                      9 : 9
#define GC_MINOR_FEATURES1_BUG_FIXES4_End                                      9
#define GC_MINOR_FEATURES1_BUG_FIXES4_Start                                    9
#define GC_MINOR_FEATURES1_BUG_FIXES4_Type                                   U01
#define   GC_MINOR_FEATURES1_BUG_FIXES4_NONE                                 0x0
#define   GC_MINOR_FEATURES1_BUG_FIXES4_AVAILABLE                            0x1

#define GC_MINOR_FEATURES1_L2_WINDOWING                                  10 : 10
#define GC_MINOR_FEATURES1_L2_WINDOWING_End                                   10
#define GC_MINOR_FEATURES1_L2_WINDOWING_Start                                 10
#define GC_MINOR_FEATURES1_L2_WINDOWING_Type                                 U01
#define   GC_MINOR_FEATURES1_L2_WINDOWING_NONE                               0x0
#define   GC_MINOR_FEATURES1_L2_WINDOWING_AVAILABLE                          0x1

#define GC_MINOR_FEATURES1_HALF_FLOAT_PIPE                               11 : 11
#define GC_MINOR_FEATURES1_HALF_FLOAT_PIPE_End                                11
#define GC_MINOR_FEATURES1_HALF_FLOAT_PIPE_Start                              11
#define GC_MINOR_FEATURES1_HALF_FLOAT_PIPE_Type                              U01
#define   GC_MINOR_FEATURES1_HALF_FLOAT_PIPE_NONE                            0x0
#define   GC_MINOR_FEATURES1_HALF_FLOAT_PIPE_AVAILABLE                       0x1

#define GC_MINOR_FEATURES1_PIXEL_DITHER                                  12 : 12
#define GC_MINOR_FEATURES1_PIXEL_DITHER_End                                   12
#define GC_MINOR_FEATURES1_PIXEL_DITHER_Start                                 12
#define GC_MINOR_FEATURES1_PIXEL_DITHER_Type                                 U01
#define   GC_MINOR_FEATURES1_PIXEL_DITHER_NONE                               0x0
#define   GC_MINOR_FEATURES1_PIXEL_DITHER_AVAILABLE                          0x1

#define GC_MINOR_FEATURES1_TWO_STENCIL_REFERENCE                         13 : 13
#define GC_MINOR_FEATURES1_TWO_STENCIL_REFERENCE_End                          13
#define GC_MINOR_FEATURES1_TWO_STENCIL_REFERENCE_Start                        13
#define GC_MINOR_FEATURES1_TWO_STENCIL_REFERENCE_Type                        U01
#define   GC_MINOR_FEATURES1_TWO_STENCIL_REFERENCE_NONE                      0x0
#define   GC_MINOR_FEATURES1_TWO_STENCIL_REFERENCE_AVAILABLE                 0x1

#define GC_MINOR_FEATURES1_EXTENDED_PIXEL_FORMAT                         14 : 14
#define GC_MINOR_FEATURES1_EXTENDED_PIXEL_FORMAT_End                          14
#define GC_MINOR_FEATURES1_EXTENDED_PIXEL_FORMAT_Start                        14
#define GC_MINOR_FEATURES1_EXTENDED_PIXEL_FORMAT_Type                        U01
#define   GC_MINOR_FEATURES1_EXTENDED_PIXEL_FORMAT_NONE                      0x0
#define   GC_MINOR_FEATURES1_EXTENDED_PIXEL_FORMAT_AVAILABLE                 0x1

/* EEZ and HZ are correct. */
#define GC_MINOR_FEATURES1_CORRECT_MIN_MAX_DEPTH                         15 : 15
#define GC_MINOR_FEATURES1_CORRECT_MIN_MAX_DEPTH_End                          15
#define GC_MINOR_FEATURES1_CORRECT_MIN_MAX_DEPTH_Start                        15
#define GC_MINOR_FEATURES1_CORRECT_MIN_MAX_DEPTH_Type                        U01
#define   GC_MINOR_FEATURES1_CORRECT_MIN_MAX_DEPTH_NONE                      0x0
#define   GC_MINOR_FEATURES1_CORRECT_MIN_MAX_DEPTH_AVAILABLE                 0x1

/* Dither and filter+alpha available. */
#define GC_MINOR_FEATURES1_DITHER_AND_FILTER_PLUS_ALPHA_2D               16 : 16
#define GC_MINOR_FEATURES1_DITHER_AND_FILTER_PLUS_ALPHA_2D_End                16
#define GC_MINOR_FEATURES1_DITHER_AND_FILTER_PLUS_ALPHA_2D_Start              16
#define GC_MINOR_FEATURES1_DITHER_AND_FILTER_PLUS_ALPHA_2D_Type              U01
#define   GC_MINOR_FEATURES1_DITHER_AND_FILTER_PLUS_ALPHA_2D_NONE            0x0
#define   GC_MINOR_FEATURES1_DITHER_AND_FILTER_PLUS_ALPHA_2D_AVAILABLE       0x1

#define GC_MINOR_FEATURES1_BUG_FIXES5                                    17 : 17
#define GC_MINOR_FEATURES1_BUG_FIXES5_End                                     17
#define GC_MINOR_FEATURES1_BUG_FIXES5_Start                                   17
#define GC_MINOR_FEATURES1_BUG_FIXES5_Type                                   U01
#define   GC_MINOR_FEATURES1_BUG_FIXES5_NONE                                 0x0
#define   GC_MINOR_FEATURES1_BUG_FIXES5_AVAILABLE                            0x1

#define GC_MINOR_FEATURES1_NEW_2D                                        18 : 18
#define GC_MINOR_FEATURES1_NEW_2D_End                                         18
#define GC_MINOR_FEATURES1_NEW_2D_Start                                       18
#define GC_MINOR_FEATURES1_NEW_2D_Type                                       U01
#define   GC_MINOR_FEATURES1_NEW_2D_NONE                                     0x0
#define   GC_MINOR_FEATURES1_NEW_2D_AVAILABLE                                0x1

#define GC_MINOR_FEATURES1_NEW_FLOATING_POINT_ARITHMETIC                 19 : 19
#define GC_MINOR_FEATURES1_NEW_FLOATING_POINT_ARITHMETIC_End                  19
#define GC_MINOR_FEATURES1_NEW_FLOATING_POINT_ARITHMETIC_Start                19
#define GC_MINOR_FEATURES1_NEW_FLOATING_POINT_ARITHMETIC_Type                U01
#define   GC_MINOR_FEATURES1_NEW_FLOATING_POINT_ARITHMETIC_NONE              0x0
#define   GC_MINOR_FEATURES1_NEW_FLOATING_POINT_ARITHMETIC_AVAILABLE         0x1

#define GC_MINOR_FEATURES1_TEXTURE_HORIZONTAL_ALIGNMENT_SELECT           20 : 20
#define GC_MINOR_FEATURES1_TEXTURE_HORIZONTAL_ALIGNMENT_SELECT_End            20
#define GC_MINOR_FEATURES1_TEXTURE_HORIZONTAL_ALIGNMENT_SELECT_Start          20
#define GC_MINOR_FEATURES1_TEXTURE_HORIZONTAL_ALIGNMENT_SELECT_Type          U01
#define   GC_MINOR_FEATURES1_TEXTURE_HORIZONTAL_ALIGNMENT_SELECT_NONE        0x0
#define   GC_MINOR_FEATURES1_TEXTURE_HORIZONTAL_ALIGNMENT_SELECT_AVAILABLE   0x1

#define GC_MINOR_FEATURES1_NON_POWER_OF_TWO                              21 : 21
#define GC_MINOR_FEATURES1_NON_POWER_OF_TWO_End                               21
#define GC_MINOR_FEATURES1_NON_POWER_OF_TWO_Start                             21
#define GC_MINOR_FEATURES1_NON_POWER_OF_TWO_Type                             U01
#define   GC_MINOR_FEATURES1_NON_POWER_OF_TWO_NONE                           0x0
#define   GC_MINOR_FEATURES1_NON_POWER_OF_TWO_AVAILABLE                      0x1

#define GC_MINOR_FEATURES1_LINEAR_TEXTURE_SUPPORT                        22 : 22
#define GC_MINOR_FEATURES1_LINEAR_TEXTURE_SUPPORT_End                         22
#define GC_MINOR_FEATURES1_LINEAR_TEXTURE_SUPPORT_Start                       22
#define GC_MINOR_FEATURES1_LINEAR_TEXTURE_SUPPORT_Type                       U01
#define   GC_MINOR_FEATURES1_LINEAR_TEXTURE_SUPPORT_NONE                     0x0
#define   GC_MINOR_FEATURES1_LINEAR_TEXTURE_SUPPORT_AVAILABLE                0x1

#define GC_MINOR_FEATURES1_HALTI0                                        23 : 23
#define GC_MINOR_FEATURES1_HALTI0_End                                         23
#define GC_MINOR_FEATURES1_HALTI0_Start                                       23
#define GC_MINOR_FEATURES1_HALTI0_Type                                       U01
#define   GC_MINOR_FEATURES1_HALTI0_NONE                                     0x0
#define   GC_MINOR_FEATURES1_HALTI0_AVAILABLE                                0x1

#define GC_MINOR_FEATURES1_CORRECT_OVERFLOW_VG                           24 : 24
#define GC_MINOR_FEATURES1_CORRECT_OVERFLOW_VG_End                            24
#define GC_MINOR_FEATURES1_CORRECT_OVERFLOW_VG_Start                          24
#define GC_MINOR_FEATURES1_CORRECT_OVERFLOW_VG_Type                          U01
#define   GC_MINOR_FEATURES1_CORRECT_OVERFLOW_VG_NONE                        0x0
#define   GC_MINOR_FEATURES1_CORRECT_OVERFLOW_VG_AVAILABLE                   0x1

#define GC_MINOR_FEATURES1_NEGATIVE_LOG_FIX                              25 : 25
#define GC_MINOR_FEATURES1_NEGATIVE_LOG_FIX_End                               25
#define GC_MINOR_FEATURES1_NEGATIVE_LOG_FIX_Start                             25
#define GC_MINOR_FEATURES1_NEGATIVE_LOG_FIX_Type                             U01
#define   GC_MINOR_FEATURES1_NEGATIVE_LOG_FIX_NONE                           0x0
#define   GC_MINOR_FEATURES1_NEGATIVE_LOG_FIX_AVAILABLE                      0x1

#define GC_MINOR_FEATURES1_RESOLVE_OFFSET                                26 : 26
#define GC_MINOR_FEATURES1_RESOLVE_OFFSET_End                                 26
#define GC_MINOR_FEATURES1_RESOLVE_OFFSET_Start                               26
#define GC_MINOR_FEATURES1_RESOLVE_OFFSET_Type                               U01
#define   GC_MINOR_FEATURES1_RESOLVE_OFFSET_NONE                             0x0
#define   GC_MINOR_FEATURES1_RESOLVE_OFFSET_AVAILABLE                        0x1

#define GC_MINOR_FEATURES1_OK_TO_GATE_AXI_CLOCK                          27 : 27
#define GC_MINOR_FEATURES1_OK_TO_GATE_AXI_CLOCK_End                           27
#define GC_MINOR_FEATURES1_OK_TO_GATE_AXI_CLOCK_Start                         27
#define GC_MINOR_FEATURES1_OK_TO_GATE_AXI_CLOCK_Type                         U01
#define   GC_MINOR_FEATURES1_OK_TO_GATE_AXI_CLOCK_NONE                       0x0
#define   GC_MINOR_FEATURES1_OK_TO_GATE_AXI_CLOCK_AVAILABLE                  0x1

#define GC_MINOR_FEATURES1_MMU                                           28 : 28
#define GC_MINOR_FEATURES1_MMU_End                                            28
#define GC_MINOR_FEATURES1_MMU_Start                                          28
#define GC_MINOR_FEATURES1_MMU_Type                                          U01
#define   GC_MINOR_FEATURES1_MMU_NONE                                        0x0
#define   GC_MINOR_FEATURES1_MMU_AVAILABLE                                   0x1

#define GC_MINOR_FEATURES1_WIDE_LINE                                     29 : 29
#define GC_MINOR_FEATURES1_WIDE_LINE_End                                      29
#define GC_MINOR_FEATURES1_WIDE_LINE_Start                                    29
#define GC_MINOR_FEATURES1_WIDE_LINE_Type                                    U01
#define   GC_MINOR_FEATURES1_WIDE_LINE_NONE                                  0x0
#define   GC_MINOR_FEATURES1_WIDE_LINE_AVAILABLE                             0x1

#define GC_MINOR_FEATURES1_BUG_FIXES6                                    30 : 30
#define GC_MINOR_FEATURES1_BUG_FIXES6_End                                     30
#define GC_MINOR_FEATURES1_BUG_FIXES6_Start                                   30
#define GC_MINOR_FEATURES1_BUG_FIXES6_Type                                   U01
#define   GC_MINOR_FEATURES1_BUG_FIXES6_NONE                                 0x0
#define   GC_MINOR_FEATURES1_BUG_FIXES6_AVAILABLE                            0x1

#define GC_MINOR_FEATURES1_FC_FLUSH_STALL                                31 : 31
#define GC_MINOR_FEATURES1_FC_FLUSH_STALL_End                                 31
#define GC_MINOR_FEATURES1_FC_FLUSH_STALL_Start                               31
#define GC_MINOR_FEATURES1_FC_FLUSH_STALL_Type                               U01
#define   GC_MINOR_FEATURES1_FC_FLUSH_STALL_NONE                             0x0
#define   GC_MINOR_FEATURES1_FC_FLUSH_STALL_AVAILABLE                        0x1

/*******************************************************************************
** Register gcTotalCycles
*/

/* Total cycles. This register is a free running counter.  It can be reset by
** writing 0 to it.
*/

#define GC_TOTAL_CYCLES_Address                                          0x00078
#define GC_TOTAL_CYCLES_MSB                                                   15
#define GC_TOTAL_CYCLES_LSB                                                    0
#define GC_TOTAL_CYCLES_BLK                                                    0
#define GC_TOTAL_CYCLES_Count                                                  1
#define GC_TOTAL_CYCLES_FieldMask                                     0xFFFFFFFF
#define GC_TOTAL_CYCLES_ReadMask                                      0xFFFFFFFF
#define GC_TOTAL_CYCLES_WriteMask                                     0xFFFFFFFF
#define GC_TOTAL_CYCLES_ResetValue                                    0x00000000

#define GC_TOTAL_CYCLES_CYCLES                                            31 : 0
#define GC_TOTAL_CYCLES_CYCLES_End                                            31
#define GC_TOTAL_CYCLES_CYCLES_Start                                           0
#define GC_TOTAL_CYCLES_CYCLES_Type                                          U32

/*******************************************************************************
** Register gcTotalIdleCycles
*/

/* Total cycles where the GPU is idle. It is reset when  gcTotalCycles register
** is written to. It looks at all the blocks but FE when determining the IP is
** idle.
*/

#define GC_TOTAL_IDLE_CYCLES_Address                                     0x0007C
#define GC_TOTAL_IDLE_CYCLES_MSB                                              15
#define GC_TOTAL_IDLE_CYCLES_LSB                                               0
#define GC_TOTAL_IDLE_CYCLES_BLK                                               0
#define GC_TOTAL_IDLE_CYCLES_Count                                             1
#define GC_TOTAL_IDLE_CYCLES_FieldMask                                0xFFFFFFFF
#define GC_TOTAL_IDLE_CYCLES_ReadMask                                 0xFFFFFFFF
#define GC_TOTAL_IDLE_CYCLES_WriteMask                                0xFFFFFFFF
#define GC_TOTAL_IDLE_CYCLES_ResetValue                               0x00000000

#define GC_TOTAL_IDLE_CYCLES_CYCLES                                       31 : 0
#define GC_TOTAL_IDLE_CYCLES_CYCLES_End                                       31
#define GC_TOTAL_IDLE_CYCLES_CYCLES_Start                                      0
#define GC_TOTAL_IDLE_CYCLES_CYCLES_Type                                     U32

/*******************************************************************************
** Command opcodes.
*/

#define GCREG_COMMAND_OPCODE_LOAD_STATE                                     0x01
#define GCREG_COMMAND_OPCODE_END                                            0x02
#define GCREG_COMMAND_OPCODE_NOP                                            0x03
#define GCREG_COMMAND_OPCODE_STARTDE                                        0x04
#define GCREG_COMMAND_OPCODE_WAIT                                           0x07
#define GCREG_COMMAND_OPCODE_LINK                                           0x08
#define GCREG_COMMAND_OPCODE_STALL                                          0x09
#define GCREG_COMMAND_OPCODE_CALL                                           0x0A
#define GCREG_COMMAND_OPCODE_RETURN                                         0x0B

/*******************************************************************************
** Command gcregCommandLoadState
*/

/* When enabled, convert 16.16 fixed point into 32-bit floating point. */
#define GCREG_COMMAND_LOAD_STATE_FLOAT                                   26 : 26
#define GCREG_COMMAND_LOAD_STATE_FLOAT_End                                    26
#define GCREG_COMMAND_LOAD_STATE_FLOAT_Start                                  26
#define GCREG_COMMAND_LOAD_STATE_FLOAT_Type                                  U01
#define   GCREG_COMMAND_LOAD_STATE_FLOAT_NORMAL                              0x0
#define   GCREG_COMMAND_LOAD_STATE_FLOAT_FIXED16_DOT16                       0x1

/* Number of states. 0 = 1024. */
#define GCREG_COMMAND_LOAD_STATE_COUNT                                   25 : 16
#define GCREG_COMMAND_LOAD_STATE_COUNT_End                                    25
#define GCREG_COMMAND_LOAD_STATE_COUNT_Start                                  16
#define GCREG_COMMAND_LOAD_STATE_COUNT_Type                                  U10

/* Starting state address. */
#define GCREG_COMMAND_LOAD_STATE_ADDRESS                                  15 : 0
#define GCREG_COMMAND_LOAD_STATE_ADDRESS_End                                  15
#define GCREG_COMMAND_LOAD_STATE_ADDRESS_Start                                 0
#define GCREG_COMMAND_LOAD_STATE_ADDRESS_Type                                U16

#define GCREG_COMMAND_LOAD_STATE_OPCODE                                  31 : 27
#define GCREG_COMMAND_LOAD_STATE_OPCODE_End                                   31
#define GCREG_COMMAND_LOAD_STATE_OPCODE_Start                                 27
#define GCREG_COMMAND_LOAD_STATE_OPCODE_Type                                 U05

struct gccmdldstate {
	/* gcregCommandLoadState:GCREG_COMMAND_LOAD_STATE_ADDRESS */
	unsigned int address:16;

	/* gcregCommandLoadState:GCREG_COMMAND_LOAD_STATE_COUNT */
	unsigned int count:10;

	/* gcregCommandLoadState:GCREG_COMMAND_LOAD_STATE_FLOAT */
	unsigned int fixed:1;

	/* gcregCommandLoadState:GCREG_COMMAND_LOAD_STATE_OPCODE */
	unsigned int opcode:5;
};

#define GCLDSTATE(Address, Count) \
{ \
	/* gcregCommandLoadState:GCREG_COMMAND_LOAD_STATE_ADDRESS */ \
	Address, \
	\
	/* gcregCommandLoadState:GCREG_COMMAND_LOAD_STATE_COUNT */ \
	Count, \
	\
	/* gcregCommandLoadState:GCREG_COMMAND_LOAD_STATE_FLOAT */ \
	GCREG_COMMAND_LOAD_STATE_FLOAT_NORMAL, \
	\
	/* gcregCommandLoadState:GCREG_COMMAND_LOAD_STATE_OPCODE */ \
	GCREG_COMMAND_OPCODE_LOAD_STATE \
}

/*******************************************************************************
** Command gcregCommandEnd
*/

/* Send event when END is completed. */
#define GCREG_COMMAND_END_EVENT                                            8 : 8
#define GCREG_COMMAND_END_EVENT_End                                            8
#define GCREG_COMMAND_END_EVENT_Start                                          8
#define GCREG_COMMAND_END_EVENT_Type                                         U01
#define   GCREG_COMMAND_END_EVENT_DISABLE                                    0x0
#define   GCREG_COMMAND_END_EVENT_ENABLE                                     0x1

/* Event ID to be send. */
#define GCREG_COMMAND_END_EVENT_ID                                         4 : 0
#define GCREG_COMMAND_END_EVENT_ID_End                                         4
#define GCREG_COMMAND_END_EVENT_ID_Start                                       0
#define GCREG_COMMAND_END_EVENT_ID_Type                                      U05

#define GCREG_COMMAND_END_OPCODE                                         31 : 27
#define GCREG_COMMAND_END_OPCODE_End                                          31
#define GCREG_COMMAND_END_OPCODE_Start                                        27
#define GCREG_COMMAND_END_OPCODE_Type                                        U05

struct gcfldend {
	/* gcregCommandEnd:GCREG_COMMAND_END_EVENT_ID */
	unsigned int signalid:5;

	/* gcregCommandEnd:reserved */
	unsigned int _reserved_5_7:3;

	/* gcregCommandEnd:GCREG_COMMAND_END_EVENT_ENABLE */
	unsigned int signal:1;

	/* gcregCommandEnd:reserved */
	unsigned int _reserved_9_26:18;

	/* gcregCommandEnd:GCREG_COMMAND_END_OPCODE */
	unsigned int opcode:5;
};

struct gccmdend {
	union {
		struct gcfldend fld;
		unsigned int raw;
	}
	cmd;

	/* Alignment filler. */
	unsigned int _filler;
};

static const struct gccmdend gccmdend_const = {
	/* cmd */
	{
		/* fld */
		{
			/* gcregCommandEnd:GCREG_COMMAND_END_EVENT_ID */
			0,

			/* gcregCommandEnd:reserved */
			0,

			/* gcregCommandEnd:GCREG_COMMAND_END_EVENT */
			GCREG_COMMAND_END_EVENT_DISABLE,

			/* gcregCommandEnd:reserved */
			0,

			/* gcregCommandEnd:GCREG_COMMAND_END_OPCODE */
			GCREG_COMMAND_OPCODE_END
		}
	},

	/* Alignment filler. */
	0
};

/*******************************************************************************
** Command gcregCommandNop
*/

#define GCREG_COMMAND_NOP_OPCODE                                         31 : 27
#define GCREG_COMMAND_NOP_OPCODE_End                                          31
#define GCREG_COMMAND_NOP_OPCODE_Start                                        27
#define GCREG_COMMAND_NOP_OPCODE_Type                                        U05

struct gcfldnop {
	/* gcregCommandNop:reserve */
	unsigned int _reserved_0_26:27;

	/* gcregCommandNop:GCREG_COMMAND_NOP_OPCODE */
	unsigned int opcode:5;
};

struct gccmdnop {
	union {
		struct gcfldnop fld;
		unsigned int raw;
	}
	cmd;

	/* Alignment filler. */
	unsigned int _filler;
};

static const struct gccmdnop gccmdnop_const = {
	/* cmd */
	{
		/* fld */
		{
			/* gcregCommandNop:reserve */
			0,

			/* gcregCommandNop:GCREG_COMMAND_NOP_OPCODE */
			GCREG_COMMAND_OPCODE_NOP
		}
	},

	/* Alignment filler. */
	0
};

/*******************************************************************************
** Command gcregCommandStartDE
*/

/* Offset Command
** ~~~~~~~~~~~~~~ */

/* Number of 32-bit data words to send.
** The data follows the rectangles, aligned at 64-bit.
*/
#define GCREG_COMMAND_STARTDE_DATA_COUNT                                 26 : 16
#define GCREG_COMMAND_STARTDE_DATA_COUNT_End                                  26
#define GCREG_COMMAND_STARTDE_DATA_COUNT_Start                                16
#define GCREG_COMMAND_STARTDE_DATA_COUNT_Type                                U11

/* Number of rectangles to send.
** The rectangles follow the command, aligned at 64-bit.
*/
#define GCREG_COMMAND_STARTDE_COUNT                                       15 : 8
#define GCREG_COMMAND_STARTDE_COUNT_End                                       15
#define GCREG_COMMAND_STARTDE_COUNT_Start                                      8
#define GCREG_COMMAND_STARTDE_COUNT_Type                                     U08

#define GCREG_COMMAND_STARTDE_OPCODE                                     31 : 27
#define GCREG_COMMAND_STARTDE_OPCODE_End                                      31
#define GCREG_COMMAND_STARTDE_OPCODE_Start                                    27
#define GCREG_COMMAND_STARTDE_OPCODE_Type                                    U05

struct gcfldstartde {
	/* gcregCommandStartDE:reserved */
	unsigned int _reserved_0_7:8;

	/* gcregCommandStartDE:GCREG_COMMAND_STARTDE_COUNT */
	unsigned int rectcount:8;

	/* gcregCommandStartDE:GCREG_COMMAND_STARTDE_DATA_COUNT */
	unsigned int datacount:11;

	/* gcregCommandStartDE:GCREG_COMMAND_STARTDE_OPCODE */
	unsigned int opcode:5;
};

struct gccmdstartde {
	union {
		struct gcfldstartde fld;
		unsigned int raw;
	} cmd;

	/* Alignment filler. */
	unsigned int _filler;
};

static const struct gcfldstartde gcfldstartde = {
	/* gcregCommandStartDE:reserved */
	0,

	/* gcregCommandStartDE:GCREG_COMMAND_STARTDE_COUNT */
	1,

	/* gcregCommandStartDE:GCREG_COMMAND_STARTDE_DATA_COUNT */
	0,

	/* gcregCommandStartDE:GCREG_COMMAND_STARTDE_OPCODE */
	GCREG_COMMAND_OPCODE_STARTDE
};

/* Offset TopLeft
** ~~~~~~~~~~~~~~ */

#define GCREG_COMMAND_TOP_LEFT_Y                                         31 : 16
#define GCREG_COMMAND_TOP_LEFT_Y_End                                          31
#define GCREG_COMMAND_TOP_LEFT_Y_Start                                        16
#define GCREG_COMMAND_TOP_LEFT_Y_Type                                        U16

#define GCREG_COMMAND_TOP_LEFT_X                                          15 : 0
#define GCREG_COMMAND_TOP_LEFT_X_End                                          15
#define GCREG_COMMAND_TOP_LEFT_X_Start                                         0
#define GCREG_COMMAND_TOP_LEFT_X_Type                                        U16

/* Offset BottomRight
** ~~~~~~~~~~~~~~~~~~ */

#define GCREG_COMMAND_BOTTOM_RIGHT_Y                                     31 : 16
#define GCREG_COMMAND_BOTTOM_RIGHT_Y_End                                      31
#define GCREG_COMMAND_BOTTOM_RIGHT_Y_Start                                    16
#define GCREG_COMMAND_BOTTOM_RIGHT_Y_Type                                    U16

#define GCREG_COMMAND_BOTTOM_RIGHT_X                                      15 : 0
#define GCREG_COMMAND_BOTTOM_RIGHT_X_End                                      15
#define GCREG_COMMAND_BOTTOM_RIGHT_X_Start                                     0
#define GCREG_COMMAND_BOTTOM_RIGHT_X_Type                                    U16

struct gccmdstartderect {
	/* GCREG_COMMAND_TOP_LEFT_X */
	unsigned int left:16;

	/* GCREG_COMMAND_TOP_LEFT_Y */
	unsigned int top:16;

	/* GCREG_COMMAND_BOTTOM_RIGHT_X */
	unsigned int right:16;

	/* GCREG_COMMAND_BOTTOM_RIGHT_Y */
	unsigned int bottom:16;
};

/*******************************************************************************
** Command gcregCommandWait
*/

/* Number of cycles to wait until the next command gets fetched. */
#define GCREG_COMMAND_WAIT_DELAY                                          15 : 0
#define GCREG_COMMAND_WAIT_DELAY_End                                          15
#define GCREG_COMMAND_WAIT_DELAY_Start                                         0
#define GCREG_COMMAND_WAIT_DELAY_Type                                        U16

#define GCREG_COMMAND_WAIT_OPCODE                                        31 : 27
#define GCREG_COMMAND_WAIT_OPCODE_End                                         31
#define GCREG_COMMAND_WAIT_OPCODE_Start                                       27
#define GCREG_COMMAND_WAIT_OPCODE_Type                                       U05

struct gcfldwait {
	/* gcregCommandWait:GCREG_COMMAND_WAIT_DELAY */
	unsigned int delay:16;

	/* gcregCommandWait:reserved */
	unsigned int _reserved_16_26:11;

	/* gcregCommandWait:GCREG_COMMAND_WAIT_OPCODE */
	unsigned int opcode:5;
};

struct gccmdwait {
	union {
		struct gcfldwait fld;
		unsigned int raw;
	} cmd;

	/* Alignment filler. */
	unsigned int _filler;
};

static const struct gcfldwait gcfldwait200 = {
	/* gcregCommandWait:GCREG_COMMAND_WAIT_DELAY */
	200,

	/* gcregCommandWait:reserved */
	0,

	/* gcregCommandWait:GCREG_COMMAND_WAIT_OPCODE */
	GCREG_COMMAND_OPCODE_WAIT
};

/*******************************************************************************
** Command gcregCommandLink
*/

/* Number of 64-bit words to fetch.  Make sure this number is not too low,
** nothing else will be fetched.  So, make sure that the last command in the
** new command buffer is either an END, a LINK, a CALL, or a RETURN.
*/
#define GCREG_COMMAND_LINK_PREFETCH                                       15 : 0
#define GCREG_COMMAND_LINK_PREFETCH_End                                       15
#define GCREG_COMMAND_LINK_PREFETCH_Start                                      0
#define GCREG_COMMAND_LINK_PREFETCH_Type                                     U16

#define GCREG_COMMAND_LINK_OPCODE                                        31 : 27
#define GCREG_COMMAND_LINK_OPCODE_End                                         31
#define GCREG_COMMAND_LINK_OPCODE_Start                                       27
#define GCREG_COMMAND_LINK_OPCODE_Type                                       U05

/* Offset Address
** ~~~~~~~~~~~~~~ */
#define GCREG_COMMAND_LINK_ADDRESS_Index                                       1
#define GCREG_COMMAND_LINK_ADDRESS_CmdAddrs                               0x0F0D

#define GCREG_COMMAND_LINK_ADDRESS_ADDRESS                                31 : 0
#define GCREG_COMMAND_LINK_ADDRESS_ADDRESS_End                                30
#define GCREG_COMMAND_LINK_ADDRESS_ADDRESS_Start                               0
#define GCREG_COMMAND_LINK_ADDRESS_ADDRESS_Type                              U31

struct gcfldlink {
	/* gcregCommandLink:GCREG_COMMAND_LINK_PREFETCH */
	unsigned int count:16;

	/* gcregCommandLink:reserved */
	unsigned int _reserved_16_26:11;

	/* gcregCommandLink:GCREG_COMMAND_LINK_OPCODE */
	unsigned int opcode:5;
};

struct gccmdlink {
	union {
		struct gcfldlink fld;
		unsigned int raw;
	} cmd;

	/* gcregCommandLink:GCREG_COMMAND_LINK_ADDRESS_ADDRESS */
	unsigned int address;
};

static const struct gcfldlink gcfldlink2 = {
	/* gcregCommandLink:GCREG_COMMAND_LINK_PREFETCH */
	2,

	/* gcregCommandLink:reserved */
	0,

	/* gcregCommandLink:GCREG_COMMAND_LINK_OPCODE */
	GCREG_COMMAND_OPCODE_LINK
};

static const struct gcfldlink gcfldlink4 = {
	/* gcregCommandLink:GCREG_COMMAND_LINK_PREFETCH */
	4,

	/* gcregCommandLink:reserved */
	0,

	/* gcregCommandLink:GCREG_COMMAND_LINK_OPCODE */
	GCREG_COMMAND_OPCODE_LINK
};

/*******************************************************************************
** Command gcregCommandStall
*/

/* Offset Command
** ~~~~~~~~~~~~~~ */
#define GCREG_COMMAND_STALL_OPCODE                                       31 : 27
#define GCREG_COMMAND_STALL_OPCODE_End                                        31
#define GCREG_COMMAND_STALL_OPCODE_Start                                      27
#define GCREG_COMMAND_STALL_OPCODE_Type                                      U05

/* Offset Stall
** ~~~~~~~~~~~~ */
#define GCREG_COMMAND_STALL_STALL_SOURCE                                   4 : 0
#define GCREG_COMMAND_STALL_STALL_SOURCE_End                                   4
#define GCREG_COMMAND_STALL_STALL_SOURCE_Start                                 0
#define GCREG_COMMAND_STALL_STALL_SOURCE_Type                                U05
#define   GCREG_COMMAND_STALL_STALL_SOURCE_FRONT_END                        0x01
#define   GCREG_COMMAND_STALL_STALL_SOURCE_PIXEL_ENGINE                     0x07
#define   GCREG_COMMAND_STALL_STALL_SOURCE_DRAWING_ENGINE                   0x0B

#define GCREG_COMMAND_STALL_STALL_DESTINATION                             12 : 8
#define GCREG_COMMAND_STALL_STALL_DESTINATION_End                             12
#define GCREG_COMMAND_STALL_STALL_DESTINATION_Start                            8
#define GCREG_COMMAND_STALL_STALL_DESTINATION_Type                           U05
#define   GCREG_COMMAND_STALL_STALL_DESTINATION_FRONT_END                   0x01
#define   GCREG_COMMAND_STALL_STALL_DESTINATION_PIXEL_ENGINE                0x07
#define   GCREG_COMMAND_STALL_STALL_DESTINATION_DRAWING_ENGINE              0x0B

struct gcfldstall {
	/* gcregCommandStall:reserved */
	unsigned int _reserved_0_26:27;

	/* gcregCommandStall:GCREG_COMMAND_STALL_OPCODE */
	unsigned int opcode:5;
};

struct gcfldstallarg {
	/* gcregCommandStall:GCREG_COMMAND_STALL_STALL_SOURCE */
	unsigned int src:5;

	/* gcregCommandStall:reserved */
	unsigned int _reserved_5_7:3;

	/* gcregCommandStall:GCREG_COMMAND_STALL_STALL_DESTINATION */
	unsigned int dst:5;

	/* gcregCommandStall:reserved */
	unsigned int _reserved_13_31:19;
};

struct gccmdstall {
	union {
		struct gcfldstall fld;
		unsigned int raw;
	} cmd;

	union {
		struct gcfldstallarg fld;
		unsigned int raw;
	} arg;
};

static const struct gcfldstall gcfldstall = {
	/* gcregCommandStall:reserved */
	0,

	/* gcregCommandStall:GCREG_COMMAND_STALL_OPCODE */
	GCREG_COMMAND_OPCODE_STALL
};

static const struct gcfldstallarg gcfldstall_fe_pe = {
	/* gcregCommandStall:GCREG_COMMAND_STALL_STALL_SOURCE */
	GCREG_COMMAND_STALL_STALL_SOURCE_FRONT_END,

	/* gcregCommandStall:reserved */
	0,

	/* gcregCommandStall:GCREG_COMMAND_STALL_STALL_DESTINATION */
	GCREG_COMMAND_STALL_STALL_DESTINATION_PIXEL_ENGINE,

	/* gcregCommandStall:reserved */
	0
};

/*******************************************************************************
** Command gcregCommandCall
*/

/* Offset Command
** ~~~~~~~~~~~~~~ */

/* Number of 64-bit words to fetch.  Make sure this number is not too low,
** nothing else will be fetched.  So, make sure that the last command in the
** new command buffer is either an END, a LINK, a CALL, or a RETURN.
*/
#define GCREG_COMMAND_CALL_PREFETCH                                       15 : 0
#define GCREG_COMMAND_CALL_PREFETCH_End                                       15
#define GCREG_COMMAND_CALL_PREFETCH_Start                                      0
#define GCREG_COMMAND_CALL_PREFETCH_Type                                     U16

#define GCREG_COMMAND_CALL_OPCODE                                        31 : 27
#define GCREG_COMMAND_CALL_OPCODE_End                                         31
#define GCREG_COMMAND_CALL_OPCODE_Start                                       27
#define GCREG_COMMAND_CALL_OPCODE_Type                                       U05

/* Offset Address
** ~~~~~~~~~~~~~~ */

#define GCREG_COMMAND_CALL_ADDRESS_ADDRESS                                31 : 0
#define GCREG_COMMAND_CALL_ADDRESS_ADDRESS_End                                30
#define GCREG_COMMAND_CALL_ADDRESS_ADDRESS_Start                               0
#define GCREG_COMMAND_CALL_ADDRESS_ADDRESS_Type                              U31

/* Offset ReturnPrefetch
** ~~~~~~~~~~~~~~~~~~~~~ */

/* Number of 64-bit words to fetch after a Return has been issued.  Make sure **
** this number if not too low nothing else will be fetched.  So, make sure    **
** the last command in this prefetch block is either an END, a LINK, a CALL,  **
** or a RETURN.                                                               */
#define GCREG_COMMAND_CALL_RETURN_PREFETCH_PREFETCH                       15 : 0
#define GCREG_COMMAND_CALL_RETURN_PREFETCH_PREFETCH_End                       15
#define GCREG_COMMAND_CALL_RETURN_PREFETCH_PREFETCH_Start                      0
#define GCREG_COMMAND_CALL_RETURN_PREFETCH_PREFETCH_Type                     U16

/* Offset ReturnAddress
** ~~~~~~~~~~~~~~~~~~~~ */

#define GCREG_COMMAND_CALL_RETURN_ADDRESS_ADDRESS                         31 : 0
#define GCREG_COMMAND_CALL_RETURN_ADDRESS_ADDRESS_End                         30
#define GCREG_COMMAND_CALL_RETURN_ADDRESS_ADDRESS_Start                        0
#define GCREG_COMMAND_CALL_RETURN_ADDRESS_ADDRESS_Type                       U31

struct gccmdcall {
	/* gcregCommandCall:GCREG_COMMAND_CALL_PREFETCH */
	unsigned int count:16;

	/* gcregCommandCall:reserved */
	unsigned int _reserved_16_26:11;

	/* gcregCommandCall:GCREG_COMMAND_CALL_OPCODE */
	unsigned int opcode:5;

	/* gcregCommandCall:GCREG_COMMAND_CALL_ADDRESS_ADDRESS */
	unsigned int address;

	/* gcregCommandCall:GCREG_COMMAND_CALL_RETURN_PREFETCH_PREFETCH */
	unsigned int retcount;

	/* gcregCommandCall:GCREG_COMMAND_CALL_RETURN_ADDRESS_ADDRESS */
	unsigned int retaddress;
};

/*******************************************************************************
** Command gccmdCommandReturn
*/

#define GCREG_COMMAND_RETURN_OPCODE                                      31 : 27
#define GCREG_COMMAND_RETURN_OPCODE_End                                       31
#define GCREG_COMMAND_RETURN_OPCODE_Start                                     27
#define GCREG_COMMAND_RETURN_OPCODE_Type                                     U05

struct gcfldret {
	/* gccmdCommandReturn:reserve */
	unsigned int _reserved_0_26:27;

	/* gccmdCommandReturn:GCREG_COMMAND_RETURN_OPCODE */
	unsigned int opcode:5;
};

struct gccmdret {
	union {
		struct gcfldret fld;
		unsigned int raw;
	}
	cmd;

	/* Alignment filler. */
	unsigned int _filler;
};

static const struct gcfldret gcfldret = {
	/* gccmdCommandReturn:reserve */
	0,

	/* gccmdCommandReturn:GCREG_COMMAND_RETURN_OPCODE */
	GCREG_COMMAND_OPCODE_RETURN
};

/*******************************************************************************
** State gcregStall
*/

#define gcregStallRegAddrs                                                0x0F00
#define GCREG_STALL_Count                                                      1
#define GCREG_STALL_ResetValue                                        0x00000000

#define GCREG_STALL_FLIP0                                                30 : 30
#define GCREG_STALL_FLIP0_End                                                 30
#define GCREG_STALL_FLIP0_Start                                               30
#define GCREG_STALL_FLIP0_Type                                               U01

#define GCREG_STALL_FLIP1                                                31 : 31
#define GCREG_STALL_FLIP1_End                                                 31
#define GCREG_STALL_FLIP1_Start                                               31
#define GCREG_STALL_FLIP1_Type                                               U01

#define GCREG_STALL_SOURCE                                                 4 : 0
#define GCREG_STALL_SOURCE_End                                                 4
#define GCREG_STALL_SOURCE_Start                                               0
#define GCREG_STALL_SOURCE_Type                                              U05
#define   GCREG_STALL_SOURCE_FRONT_END                                      0x01
#define   GCREG_STALL_SOURCE_PIXEL_ENGINE                                   0x07
#define   GCREG_STALL_SOURCE_DRAWING_ENGINE                                 0x0B

#define GCREG_STALL_DESTINATION                                           12 : 8
#define GCREG_STALL_DESTINATION_End                                           12
#define GCREG_STALL_DESTINATION_Start                                          8
#define GCREG_STALL_DESTINATION_Type                                         U05
#define   GCREG_STALL_DESTINATION_FRONT_END                                 0x01
#define   GCREG_STALL_DESTINATION_PIXEL_ENGINE                              0x07
#define   GCREG_STALL_DESTINATION_DRAWING_ENGINE                            0x0B

/*******************************************************************************
** State gcregPipeSelect
*/

/* Select the current graphics pipe. */

#define gcregPipeSelectRegAddrs                                           0x0E00
#define GCREG_PIPE_SELECT_MSB                                                 15
#define GCREG_PIPE_SELECT_LSB                                                  0
#define GCREG_PIPE_SELECT_BLK                                                  0
#define GCREG_PIPE_SELECT_Count                                                1
#define GCREG_PIPE_SELECT_FieldMask                                   0x00000001
#define GCREG_PIPE_SELECT_ReadMask                                    0x00000001
#define GCREG_PIPE_SELECT_WriteMask                                   0x00000001
#define GCREG_PIPE_SELECT_ResetValue                                  0x00000000

/* Selects the pipe to send states and data to.  Make  sure the PE is idle    **
** before you switch pipes.                                                   */
#define GCREG_PIPE_SELECT_PIPE                                             0 : 0
#define GCREG_PIPE_SELECT_PIPE_End                                             0
#define GCREG_PIPE_SELECT_PIPE_Start                                           0
#define GCREG_PIPE_SELECT_PIPE_Type                                          U01
#define   GCREG_PIPE_SELECT_PIPE_PIPE3D                                      0x0
#define   GCREG_PIPE_SELECT_PIPE_PIPE2D                                      0x1

struct gcregpipeselect {
	/* gcregPipeSelectRegAddrs:GCREG_PIPE_SELECT_PIPE */
	unsigned int pipe:1;

	/* gcregPipeSelectRegAddrs:reserved */
	unsigned int _reserved_1_31:31;
};

static const struct gcregpipeselect gcregpipeselect_2D = {
	/* gcregPipeSelectRegAddrs:GCREG_PIPE_SELECT_PIPE */
	GCREG_PIPE_SELECT_PIPE_PIPE2D,

	/* gcregPipeSelectRegAddrs:reserved */
	0
};

static const struct gcregpipeselect gcregpipeselect_3D = {
	/* gcregPipeSelectRegAddrs:GCREG_PIPE_SELECT_PIPE */
	GCREG_PIPE_SELECT_PIPE_PIPE3D,

	/* gcregPipeSelectRegAddrs:reserved */
	0
};

/*******************************************************************************
** State gcregEvent
*/

/* Send an event. */

#define gcregEventRegAddrs                                                0x0E01
#define GCREG_EVENT_MSB                                                       15
#define GCREG_EVENT_LSB                                                        0
#define GCREG_EVENT_BLK                                                        0
#define GCREG_EVENT_Count                                                      1
#define GCREG_EVENT_FieldMask                                         0x0000007F
#define GCREG_EVENT_ReadMask                                          0x0000007F
#define GCREG_EVENT_WriteMask                                         0x0000007F
#define GCREG_EVENT_ResetValue                                        0x00000000

/* 5-bit event ID to send. */
#define GCREG_EVENT_EVENT_ID                                               4 : 0
#define GCREG_EVENT_EVENT_ID_End                                               4
#define GCREG_EVENT_EVENT_ID_Start                                             0
#define GCREG_EVENT_EVENT_ID_Type                                            U05

/* The event is sent by the FE. */
#define GCREG_EVENT_FE_SRC                                                 5 : 5
#define GCREG_EVENT_FE_SRC_End                                                 5
#define GCREG_EVENT_FE_SRC_Start                                               5
#define GCREG_EVENT_FE_SRC_Type                                              U01
#define   GCREG_EVENT_FE_SRC_DISABLE                                         0x0
#define   GCREG_EVENT_FE_SRC_ENABLE                                          0x1

/* The event is sent by the PE. */
#define GCREG_EVENT_PE_SRC                                                 6 : 6
#define GCREG_EVENT_PE_SRC_End                                                 6
#define GCREG_EVENT_PE_SRC_Start                                               6
#define GCREG_EVENT_PE_SRC_Type                                              U01
#define   GCREG_EVENT_PE_SRC_DISABLE                                         0x0
#define   GCREG_EVENT_PE_SRC_ENABLE                                          0x1

struct gcregevent {
	/* gcregEventRegAddrs:GCREG_EVENT_EVENT_ID */
	unsigned int id:5;

	/* gcregEventRegAddrs:GCREG_EVENT_FE_SRC */
	unsigned int fe:1;

	/* gcregEventRegAddrs:GCREG_EVENT_PE_SRC */
	unsigned int pe:1;

	/* gcregEventRegAddrs:reserved */
	unsigned int _reserved_7_31:25;
};

/*******************************************************************************
** State gcregSemaphore
*/

/* A sempahore state arms the semaphore in the destination. */

#define gcregSemaphoreRegAddrs                                            0x0E02
#define GCREG_SEMAPHORE_MSB                                                   15
#define GCREG_SEMAPHORE_LSB                                                    0
#define GCREG_SEMAPHORE_BLK                                                    0
#define GCREG_SEMAPHORE_Count                                                  1
#define GCREG_SEMAPHORE_FieldMask                                     0x00001F1F
#define GCREG_SEMAPHORE_ReadMask                                      0x00001F1F
#define GCREG_SEMAPHORE_WriteMask                                     0x00001F1F
#define GCREG_SEMAPHORE_ResetValue                                    0x00000000

#define GCREG_SEMAPHORE_SOURCE                                             4 : 0
#define GCREG_SEMAPHORE_SOURCE_End                                             4
#define GCREG_SEMAPHORE_SOURCE_Start                                           0
#define GCREG_SEMAPHORE_SOURCE_Type                                          U05
#define   GCREG_SEMAPHORE_SOURCE_FRONT_END                                  0x01
#define   GCREG_SEMAPHORE_SOURCE_PIXEL_ENGINE                               0x07
#define   GCREG_SEMAPHORE_SOURCE_DRAWING_ENGINE                             0x0B

#define GCREG_SEMAPHORE_DESTINATION                                       12 : 8
#define GCREG_SEMAPHORE_DESTINATION_End                                       12
#define GCREG_SEMAPHORE_DESTINATION_Start                                      8
#define GCREG_SEMAPHORE_DESTINATION_Type                                     U05
#define   GCREG_SEMAPHORE_DESTINATION_FRONT_END                             0x01
#define   GCREG_SEMAPHORE_DESTINATION_PIXEL_ENGINE                          0x07
#define   GCREG_SEMAPHORE_DESTINATION_DRAWING_ENGINE                        0x0B

struct gcregsemaphore {
	/* gcregSemaphoreRegAddrs:GCREG_SEMAPHORE_SOURCE */
	unsigned int src:5;

	/* gcregSemaphoreRegAddrs:reserved */
	unsigned int _reserved_5_7:3;

	/* gcregSemaphoreRegAddrs:GCREG_SEMAPHORE_DESTINATION */
	unsigned int dst:5;

	/* gcregSemaphoreRegAddrs:reserved */
	unsigned int _reserved_13_31:19;
};

static const struct gcregsemaphore gcregsema_fe_pe = {
	/* gcregSemaphoreRegAddrs:GCREG_SEMAPHORE_SOURCE */
	GCREG_SEMAPHORE_SOURCE_FRONT_END,

	/* gcregSemaphoreRegAddrs:reserved */
	0,

	/* gcregSemaphoreRegAddrs:GCREG_SEMAPHORE_DESTINATION */
	GCREG_SEMAPHORE_DESTINATION_PIXEL_ENGINE,

	/* gcregSemaphoreRegAddrs:reserved */
	0
};


/*******************************************************************************
** State gcregFlush
*/

/* Flush the current pipe. */

#define gcregFlushRegAddrs                                                0x0E03
#define GCREG_FLUSH_MSB                                                       15
#define GCREG_FLUSH_LSB                                                        0
#define GCREG_FLUSH_BLK                                                        0
#define GCREG_FLUSH_Count                                                      1
#define GCREG_FLUSH_FieldMask                                         0x00000008
#define GCREG_FLUSH_ReadMask                                          0x00000008
#define GCREG_FLUSH_WriteMask                                         0x00000008
#define GCREG_FLUSH_ResetValue                                        0x00000000

/* Flush the 2D pixel cache. */
#define GCREG_FLUSH_PE2D_CACHE                                             3 : 3
#define GCREG_FLUSH_PE2D_CACHE_End                                             3
#define GCREG_FLUSH_PE2D_CACHE_Start                                           3
#define GCREG_FLUSH_PE2D_CACHE_Type                                          U01
#define   GCREG_FLUSH_PE2D_CACHE_DISABLE                                     0x0
#define   GCREG_FLUSH_PE2D_CACHE_ENABLE                                      0x1

struct gcregflush {
	/* gcregFlushRegAddrs:reserved */
	unsigned int _reserved_0_2:3;

	/* gcregFlushRegAddrs:GCREG_FLUSH_PE2D_CACHE */
	unsigned int enable:1;

	/* gcregFlushRegAddrs:reserved */
	unsigned int _reserved_4_31:28;
};

static const struct gcregflush gcregflush_pe2D = {
	/* gcregFlushRegAddrs:reserved */
	0,

	/* gcregFlushRegAddrs:GCREG_FLUSH_PE2D_CACHE */
	GCREG_FLUSH_PE2D_CACHE_ENABLE,

	/* gcregFlushRegAddrs:reserved */
	0
};

/*******************************************************************************
** State gcregMMUFlush
*/

/* Flush the virtual addrses lookup cache inside the MC. */

#define gcregMMUFlushRegAddrs                                             0x0E04
#define gcregMMU_FLUSH_MSB                                                    15
#define gcregMMU_FLUSH_LSB                                                     0
#define gcregMMU_FLUSH_BLK                                                     0
#define gcregMMU_FLUSH_Count                                                   1
#define gcregMMU_FLUSH_FieldMask                                      0x00000009
#define gcregMMU_FLUSH_ReadMask                                       0x00000009
#define gcregMMU_FLUSH_WriteMask                                      0x00000009
#define gcregMMU_FLUSH_ResetValue                                     0x00000000

/* Flush the FE address translation caches. */
#define gcregMMU_FLUSH_FEMMU                                               0 : 0
#define gcregMMU_FLUSH_FEMMU_End                                               0
#define gcregMMU_FLUSH_FEMMU_Start                                             0
#define gcregMMU_FLUSH_FEMMU_Type                                            U01
#define   gcregMMU_FLUSH_FEMMU_DISABLE                                       0x0
#define   gcregMMU_FLUSH_FEMMU_ENABLE                                        0x1

/* Flush the PE render target address translation caches. */
#define gcregMMU_FLUSH_PEMMU                                               3 : 3
#define gcregMMU_FLUSH_PEMMU_End                                               3
#define gcregMMU_FLUSH_PEMMU_Start                                             3
#define gcregMMU_FLUSH_PEMMU_Type                                            U01
#define   gcregMMU_FLUSH_PEMMU_DISABLE                                       0x0
#define   gcregMMU_FLUSH_PEMMU_ENABLE                                        0x1

/*******************************************************************************
** Register gcregCmdBufferAddr
*/

/* Base address for the command buffer.  The address must be 64-bit aligned
** and it is always physical. This register cannot be read. To check the value
** of the current fetch address use gcregFEDebugCurCmdAdr. Since this is a write
** only register is has no reset value.
*/

#define GCREG_CMD_BUFFER_ADDR_Address                                    0x00654
#define GCREG_CMD_BUFFER_ADDR_MSB                                             15
#define GCREG_CMD_BUFFER_ADDR_LSB                                              0
#define GCREG_CMD_BUFFER_ADDR_BLK                                              0
#define GCREG_CMD_BUFFER_ADDR_Count                                            1
#define GCREG_CMD_BUFFER_ADDR_FieldMask                               0xFFFFFFFF
#define GCREG_CMD_BUFFER_ADDR_ReadMask                                0x00000000
#define GCREG_CMD_BUFFER_ADDR_WriteMask                               0xFFFFFFFC
#define GCREG_CMD_BUFFER_ADDR_ResetValue                              0x00000000

#define GCREG_CMD_BUFFER_ADDR_ADDRESS                                     31 : 0
#define GCREG_CMD_BUFFER_ADDR_ADDRESS_End                                     30
#define GCREG_CMD_BUFFER_ADDR_ADDRESS_Start                                    0
#define GCREG_CMD_BUFFER_ADDR_ADDRESS_Type                                   U31

/*******************************************************************************
** Register gcregCmdBufferCtrl
*/

/* Since this is a write only register is has no reset value. */

#define GCREG_CMD_BUFFER_CTRL_Address                                    0x00658
#define GCREG_CMD_BUFFER_CTRL_MSB                                             15
#define GCREG_CMD_BUFFER_CTRL_LSB                                              0
#define GCREG_CMD_BUFFER_CTRL_BLK                                              0
#define GCREG_CMD_BUFFER_CTRL_Count                                            1
#define GCREG_CMD_BUFFER_CTRL_FieldMask                               0x0001FFFF
#define GCREG_CMD_BUFFER_CTRL_ReadMask                                0x00010000
#define GCREG_CMD_BUFFER_CTRL_WriteMask                               0x0001FFFF
#define GCREG_CMD_BUFFER_CTRL_ResetValue                              0x00000000

/* Number of 64-bit words to fetch from the command buffer. */
#define GCREG_CMD_BUFFER_CTRL_PREFETCH                                    15 : 0
#define GCREG_CMD_BUFFER_CTRL_PREFETCH_End                                    15
#define GCREG_CMD_BUFFER_CTRL_PREFETCH_Start                                   0
#define GCREG_CMD_BUFFER_CTRL_PREFETCH_Type                                  U16

/* Enable the command parser. */
#define GCREG_CMD_BUFFER_CTRL_ENABLE                                     16 : 16
#define GCREG_CMD_BUFFER_CTRL_ENABLE_End                                      16
#define GCREG_CMD_BUFFER_CTRL_ENABLE_Start                                    16
#define GCREG_CMD_BUFFER_CTRL_ENABLE_Type                                    U01
#define   GCREG_CMD_BUFFER_CTRL_ENABLE_DISABLE                               0x0
#define   GCREG_CMD_BUFFER_CTRL_ENABLE_ENABLE                                0x1

/*******************************************************************************
** Register gcregFEDebugState
*/

#define GCREG_FE_DEBUG_STATE_Address                                     0x00660
#define GCREG_FE_DEBUG_STATE_MSB                                              15
#define GCREG_FE_DEBUG_STATE_LSB                                               0
#define GCREG_FE_DEBUG_STATE_BLK                                               0
#define GCREG_FE_DEBUG_STATE_Count                                             1
#define GCREG_FE_DEBUG_STATE_FieldMask                                0x0003FF1F
#define GCREG_FE_DEBUG_STATE_ReadMask                                 0x0003FF1F
#define GCREG_FE_DEBUG_STATE_WriteMask                                0x00000000
#define GCREG_FE_DEBUG_STATE_ResetValue                               0x00000000

#define GCREG_FE_DEBUG_STATE_CMD_STATE                                     4 : 0
#define GCREG_FE_DEBUG_STATE_CMD_STATE_End                                     4
#define GCREG_FE_DEBUG_STATE_CMD_STATE_Start                                   0
#define GCREG_FE_DEBUG_STATE_CMD_STATE_Type                                  U05

#define GCREG_FE_DEBUG_STATE_CMD_DMA_STATE                                 9 : 8
#define GCREG_FE_DEBUG_STATE_CMD_DMA_STATE_End                                 9
#define GCREG_FE_DEBUG_STATE_CMD_DMA_STATE_Start                               8
#define GCREG_FE_DEBUG_STATE_CMD_DMA_STATE_Type                              U02

#define GCREG_FE_DEBUG_STATE_CMD_FETCH_STATE                             11 : 10
#define GCREG_FE_DEBUG_STATE_CMD_FETCH_STATE_End                              11
#define GCREG_FE_DEBUG_STATE_CMD_FETCH_STATE_Start                            10
#define GCREG_FE_DEBUG_STATE_CMD_FETCH_STATE_Type                            U02

#define GCREG_FE_DEBUG_STATE_REQ_DMA_STATE                               13 : 12
#define GCREG_FE_DEBUG_STATE_REQ_DMA_STATE_End                                13
#define GCREG_FE_DEBUG_STATE_REQ_DMA_STATE_Start                              12
#define GCREG_FE_DEBUG_STATE_REQ_DMA_STATE_Type                              U02

#define GCREG_FE_DEBUG_STATE_CAL_STATE                                   15 : 14
#define GCREG_FE_DEBUG_STATE_CAL_STATE_End                                    15
#define GCREG_FE_DEBUG_STATE_CAL_STATE_Start                                  14
#define GCREG_FE_DEBUG_STATE_CAL_STATE_Type                                  U02

#define GCREG_FE_DEBUG_STATE_VE_REQ_STATE                                17 : 16
#define GCREG_FE_DEBUG_STATE_VE_REQ_STATE_End                                 17
#define GCREG_FE_DEBUG_STATE_VE_REQ_STATE_Start                               16
#define GCREG_FE_DEBUG_STATE_VE_REQ_STATE_Type                               U02

/*******************************************************************************
** Register gcregFEDebugCurCmdAdr
*/

/* This is the command decoder address.  The address is always physical so
** the MSB should always be 0.  It has no reset value.
*/

#define GCREG_FE_DEBUG_CUR_CMD_ADR_Address                               0x00664
#define GCREG_FE_DEBUG_CUR_CMD_ADR_MSB                                        15
#define GCREG_FE_DEBUG_CUR_CMD_ADR_LSB                                         0
#define GCREG_FE_DEBUG_CUR_CMD_ADR_BLK                                         0
#define GCREG_FE_DEBUG_CUR_CMD_ADR_Count                                       1
#define GCREG_FE_DEBUG_CUR_CMD_ADR_FieldMask                          0xFFFFFFF8
#define GCREG_FE_DEBUG_CUR_CMD_ADR_ReadMask                           0xFFFFFFF8
#define GCREG_FE_DEBUG_CUR_CMD_ADR_WriteMask                          0x00000000
#define GCREG_FE_DEBUG_CUR_CMD_ADR_ResetValue                         0x00000000

#define GCREG_FE_DEBUG_CUR_CMD_ADR_CUR_CMD_ADR                            31 : 3
#define GCREG_FE_DEBUG_CUR_CMD_ADR_CUR_CMD_ADR_End                            31
#define GCREG_FE_DEBUG_CUR_CMD_ADR_CUR_CMD_ADR_Start                           3
#define GCREG_FE_DEBUG_CUR_CMD_ADR_CUR_CMD_ADR_Type                          U29

/*******************************************************************************
** Register gcregFEDebugCmdLowReg
*/

#define GCREG_FE_DEBUG_CMD_LOW_REG_Address                               0x00668
#define GCREG_FE_DEBUG_CMD_LOW_REG_MSB                                        15
#define GCREG_FE_DEBUG_CMD_LOW_REG_LSB                                         0
#define GCREG_FE_DEBUG_CMD_LOW_REG_BLK                                         0
#define GCREG_FE_DEBUG_CMD_LOW_REG_Count                                       1
#define GCREG_FE_DEBUG_CMD_LOW_REG_FieldMask                          0xFFFFFFFF
#define GCREG_FE_DEBUG_CMD_LOW_REG_ReadMask                           0xFFFFFFFF
#define GCREG_FE_DEBUG_CMD_LOW_REG_WriteMask                          0x00000000
#define GCREG_FE_DEBUG_CMD_LOW_REG_ResetValue                         0x00000000

/* Command register used by CmdState. */
#define GCREG_FE_DEBUG_CMD_LOW_REG_CMD_LOW_REG                            31 : 0
#define GCREG_FE_DEBUG_CMD_LOW_REG_CMD_LOW_REG_End                            31
#define GCREG_FE_DEBUG_CMD_LOW_REG_CMD_LOW_REG_Start                           0
#define GCREG_FE_DEBUG_CMD_LOW_REG_CMD_LOW_REG_Type                          U32

/*******************************************************************************
** Register gcregFEDebugCmdHiReg
*/

#define GCREG_FE_DEBUG_CMD_HI_REG_Address                                0x0066C
#define GCREG_FE_DEBUG_CMD_HI_REG_MSB                                         15
#define GCREG_FE_DEBUG_CMD_HI_REG_LSB                                          0
#define GCREG_FE_DEBUG_CMD_HI_REG_BLK                                          0
#define GCREG_FE_DEBUG_CMD_HI_REG_Count                                        1
#define GCREG_FE_DEBUG_CMD_HI_REG_FieldMask                           0xFFFFFFFF
#define GCREG_FE_DEBUG_CMD_HI_REG_ReadMask                            0xFFFFFFFF
#define GCREG_FE_DEBUG_CMD_HI_REG_WriteMask                           0x00000000
#define GCREG_FE_DEBUG_CMD_HI_REG_ResetValue                          0x00000000

/* Command register used by CmdState. */
#define GCREG_FE_DEBUG_CMD_HI_REG_CMD_HI_REG                              31 : 0
#define GCREG_FE_DEBUG_CMD_HI_REG_CMD_HI_REG_End                              31
#define GCREG_FE_DEBUG_CMD_HI_REG_CMD_HI_REG_Start                             0
#define GCREG_FE_DEBUG_CMD_HI_REG_CMD_HI_REG_Type                            U32

/*******************************************************************************
** State gcregMMUSafeAddress
*/

/* A 64-byte address that will acts as a 'safe' zone.  Any address that would
** cause an exception is routed to this safe zone.  Reads will happend and
** writes will go to this address, but with a write-enable of 0. This
** register can only be programmed once after a reset - any attempt to write
** to this register after the initial write-after-reset will be ignored.
*/

#define gcregMMUSafeAddressRegAddrs                                       0x0060
#define GCREG_MMU_SAFE_ADDRESS_MSB                                            15
#define GCREG_MMU_SAFE_ADDRESS_LSB                                             0
#define GCREG_MMU_SAFE_ADDRESS_BLK                                             0
#define GCREG_MMU_SAFE_ADDRESS_Count                                           1
#define GCREG_MMU_SAFE_ADDRESS_FieldMask                              0xFFFFFFFF
#define GCREG_MMU_SAFE_ADDRESS_ReadMask                               0xFFFFFFC0
#define GCREG_MMU_SAFE_ADDRESS_WriteMask                              0xFFFFFFC0
#define GCREG_MMU_SAFE_ADDRESS_ResetValue                             0x00000000

#define GCREG_MMU_SAFE_ADDRESS_ADDRESS                                    31 : 0
#define GCREG_MMU_SAFE_ADDRESS_ADDRESS_End                                    31
#define GCREG_MMU_SAFE_ADDRESS_ADDRESS_Start                                   0
#define GCREG_MMU_SAFE_ADDRESS_ADDRESS_Type                                  U32

/*******************************************************************************
** State gcregMMUConfiguration
*/

/* This register controls the master TLB of the MMU. */

#define gcregMMUConfigurationRegAddrs                                     0x0061
#define GCREG_MMU_CONFIGURATION_MSB                                           15
#define GCREG_MMU_CONFIGURATION_LSB                                            0
#define GCREG_MMU_CONFIGURATION_BLK                                            0
#define GCREG_MMU_CONFIGURATION_Count                                          1
#define GCREG_MMU_CONFIGURATION_FieldMask                             0xFFFFFD99
#define GCREG_MMU_CONFIGURATION_ReadMask                              0xFFFFFD99
#define GCREG_MMU_CONFIGURATION_WriteMask                             0xFFFFFD99
#define GCREG_MMU_CONFIGURATION_ResetValue                            0x00000000

/* Upper bits of the page aligned (depending on the mode) master TLB. */
#define GCREG_MMU_CONFIGURATION_ADDRESS                                  31 : 10
#define GCREG_MMU_CONFIGURATION_ADDRESS_End                                   31
#define GCREG_MMU_CONFIGURATION_ADDRESS_Start                                 10
#define GCREG_MMU_CONFIGURATION_ADDRESS_Type                                 U22

/* Mask for Address field. */
#define GCREG_MMU_CONFIGURATION_MASK_ADDRESS                               8 : 8
#define GCREG_MMU_CONFIGURATION_MASK_ADDRESS_End                               8
#define GCREG_MMU_CONFIGURATION_MASK_ADDRESS_Start                             8
#define GCREG_MMU_CONFIGURATION_MASK_ADDRESS_Type                            U01
#define   GCREG_MMU_CONFIGURATION_MASK_ADDRESS_ENABLED                       0x0
#define   GCREG_MMU_CONFIGURATION_MASK_ADDRESS_MASKED                        0x1

/* Mask Flush field. */
#define GCREG_MMU_CONFIGURATION_MASK_FLUSH                                 7 : 7
#define GCREG_MMU_CONFIGURATION_MASK_FLUSH_End                                 7
#define GCREG_MMU_CONFIGURATION_MASK_FLUSH_Start                               7
#define GCREG_MMU_CONFIGURATION_MASK_FLUSH_Type                              U01
#define   GCREG_MMU_CONFIGURATION_MASK_FLUSH_ENABLED                         0x0
#define   GCREG_MMU_CONFIGURATION_MASK_FLUSH_MASKED                          0x1

/* Flush the MMU caches. */
#define GCREG_MMU_CONFIGURATION_FLUSH                                      4 : 4
#define GCREG_MMU_CONFIGURATION_FLUSH_End                                      4
#define GCREG_MMU_CONFIGURATION_FLUSH_Start                                    4
#define GCREG_MMU_CONFIGURATION_FLUSH_Type                                   U01
#define   GCREG_MMU_CONFIGURATION_FLUSH_FLUSH                                0x1

/* Mask Mode field. */
#define GCREG_MMU_CONFIGURATION_MASK_MODE                                  3 : 3
#define GCREG_MMU_CONFIGURATION_MASK_MODE_End                                  3
#define GCREG_MMU_CONFIGURATION_MASK_MODE_Start                                3
#define GCREG_MMU_CONFIGURATION_MASK_MODE_Type                               U01
#define   GCREG_MMU_CONFIGURATION_MASK_MODE_ENABLED                          0x0
#define   GCREG_MMU_CONFIGURATION_MASK_MODE_MASKED                           0x1

/* Set the mode for the Master TLB. */
#define GCREG_MMU_CONFIGURATION_MODE                                       0 : 0
#define GCREG_MMU_CONFIGURATION_MODE_End                                       0
#define GCREG_MMU_CONFIGURATION_MODE_Start                                     0
#define GCREG_MMU_CONFIGURATION_MODE_Type                                    U01
/* The Master TLB is 4kB in size and contains 1024 entries. Each page can be  **
** 4kB or 64kB in size.                                                       */
#define   GCREG_MMU_CONFIGURATION_MODE_MODE4_K                               0x0
/* The Master TLB is 1kB in size and contains 256 entries. Each page can be   **
** 4kB, 64kB, 1MB or 16MB in size.                                            */
#define   GCREG_MMU_CONFIGURATION_MODE_MODE1_K                               0x1

struct gcregmmuconfiguration {
	/* gcregMMUConfiguration:GCREG_MMU_CONFIGURATION_MODE */
	unsigned int master:1;

	/* gcregMMUConfiguration:reserved */
	unsigned int _reserved_1_2:2;

	/* gcregMMUConfiguration:GCREG_MMU_CONFIGURATION_MASK_MODE */
	unsigned int master_mask:1;

	/* gcregMMUConfiguration:GCREG_MMU_CONFIGURATION_FLUSH */
	unsigned int flush:1;

	/* gcregMMUConfiguration:reserved */
	unsigned int _reserved_5_6:2;

	/* gcregMMUConfiguration:GCREG_MMU_CONFIGURATION_MASK_FLUSH */
	unsigned int flush_mask:1;

	/* gcregMMUConfiguration:GCREG_MMU_CONFIGURATION_MASK_ADDRESS */
	unsigned int address_mask:1;

	/* gcregMMUConfiguration:reserved */
	unsigned int _reserved_9:1;

	/* gcregMMUConfiguration:GCREG_MMU_CONFIGURATION_ADDRESS */
	unsigned int address:22;
};

static const struct gcregmmuconfiguration gcregmmu_flush = {
	/* gcregMMUConfiguration:GCREG_MMU_CONFIGURATION_MODE */
	0,

	/* gcregMMUConfiguration:reserved */
	0,

	/* gcregMMUConfiguration:GCREG_MMU_CONFIGURATION_MASK_MODE */
	GCREG_MMU_CONFIGURATION_MASK_MODE_MASKED,

	/* gcregMMUConfiguration:GCREG_MMU_CONFIGURATION_FLUSH */
	GCREG_MMU_CONFIGURATION_FLUSH_FLUSH,

	/* gcregMMUConfiguration:reserved */
	0,

	/* gcregMMUConfiguration:GCREG_MMU_CONFIGURATION_MASK_FLUSH */
	GCREG_MMU_CONFIGURATION_MASK_FLUSH_ENABLED,

	/* gcregMMUConfiguration:GCREG_MMU_CONFIGURATION_MASK_ADDRESS */
	GCREG_MMU_CONFIGURATION_MASK_ADDRESS_MASKED,

	/* gcregMMUConfiguration:reserved */
	0,

	/* gcregMMUConfiguration:GCREG_MMU_CONFIGURATION_ADDRESS */
	0
};

/*******************************************************************************
** Register gcregMMUStatus
*/

/* Status register that holds which MMU generated an exception. */

#define GCREG_MMU_STATUS_Address                                         0x00188
#define GCREG_MMU_STATUS_MSB                                                  15
#define GCREG_MMU_STATUS_LSB                                                   0
#define GCREG_MMU_STATUS_BLK                                                   0
#define GCREG_MMU_STATUS_Count                                                 1
#define GCREG_MMU_STATUS_FieldMask                                    0x00003333
#define GCREG_MMU_STATUS_ReadMask                                     0x00003333
#define GCREG_MMU_STATUS_WriteMask                                    0x00000000
#define GCREG_MMU_STATUS_ResetValue                                   0x00000000

/* MMU 3 caused an exception and the fourth gcregMMUException register holds  **
** the offending address.                                                     */
#define GCREG_MMU_STATUS_EXCEPTION3                                      13 : 12
#define GCREG_MMU_STATUS_EXCEPTION3_End                                       13
#define GCREG_MMU_STATUS_EXCEPTION3_Start                                     12
#define GCREG_MMU_STATUS_EXCEPTION3_Type                                     U02
#define   GCREG_MMU_STATUS_EXCEPTION3_SLAVE_NOT_PRESENT                      0x1
#define   GCREG_MMU_STATUS_EXCEPTION3_PAGE_NOT_PRESENT                       0x2
#define   GCREG_MMU_STATUS_EXCEPTION3_WRITE_VIOLATION                        0x3

/* MMU 2 caused an exception and the third gcregMMUException register holds   **
** the offending address.                                                     */
#define GCREG_MMU_STATUS_EXCEPTION2                                        9 : 8
#define GCREG_MMU_STATUS_EXCEPTION2_End                                        9
#define GCREG_MMU_STATUS_EXCEPTION2_Start                                      8
#define GCREG_MMU_STATUS_EXCEPTION2_Type                                     U02
#define   GCREG_MMU_STATUS_EXCEPTION2_SLAVE_NOT_PRESENT                      0x1
#define   GCREG_MMU_STATUS_EXCEPTION2_PAGE_NOT_PRESENT                       0x2
#define   GCREG_MMU_STATUS_EXCEPTION2_WRITE_VIOLATION                        0x3

/* MMU 1 caused an exception and the second gcregMMUException register holds  **
** the offending address.                                                     */
#define GCREG_MMU_STATUS_EXCEPTION1                                        5 : 4
#define GCREG_MMU_STATUS_EXCEPTION1_End                                        5
#define GCREG_MMU_STATUS_EXCEPTION1_Start                                      4
#define GCREG_MMU_STATUS_EXCEPTION1_Type                                     U02
#define   GCREG_MMU_STATUS_EXCEPTION1_SLAVE_NOT_PRESENT                      0x1
#define   GCREG_MMU_STATUS_EXCEPTION1_PAGE_NOT_PRESENT                       0x2
#define   GCREG_MMU_STATUS_EXCEPTION1_WRITE_VIOLATION                        0x3

/* MMU 0 caused an exception and the first gcregMMUException register holds   **
** the offending address.                                                     */
#define GCREG_MMU_STATUS_EXCEPTION0                                        1 : 0
#define GCREG_MMU_STATUS_EXCEPTION0_End                                        1
#define GCREG_MMU_STATUS_EXCEPTION0_Start                                      0
#define GCREG_MMU_STATUS_EXCEPTION0_Type                                     U02
#define   GCREG_MMU_STATUS_EXCEPTION0_SLAVE_NOT_PRESENT                      0x1
#define   GCREG_MMU_STATUS_EXCEPTION0_PAGE_NOT_PRESENT                       0x2
#define   GCREG_MMU_STATUS_EXCEPTION0_WRITE_VIOLATION                        0x3

/*******************************************************************************
** Register gcregMMUControl
*/

/* Control register that enables the MMU (only time shot). */

#define GCREG_MMU_CONTROL_Address                                        0x0018C
#define GCREG_MMU_CONTROL_MSB                                                 15
#define GCREG_MMU_CONTROL_LSB                                                  0
#define GCREG_MMU_CONTROL_BLK                                                  0
#define GCREG_MMU_CONTROL_Count                                                1
#define GCREG_MMU_CONTROL_FieldMask                                   0x00000001
#define GCREG_MMU_CONTROL_ReadMask                                    0x00000000
#define GCREG_MMU_CONTROL_WriteMask                                   0x00000001
#define GCREG_MMU_CONTROL_ResetValue                                  0x00000000

/* Enable the MMU. For security reasons, once the MMU is  enabled it cannot   **
** be disabled anymore.                                                       */
#define GCREG_MMU_CONTROL_ENABLE                                           0 : 0
#define GCREG_MMU_CONTROL_ENABLE_End                                           0
#define GCREG_MMU_CONTROL_ENABLE_Start                                         0
#define GCREG_MMU_CONTROL_ENABLE_Type                                        U01
#define   GCREG_MMU_CONTROL_ENABLE_ENABLE                                    0x1

/*******************************************************************************
** State/Register gcregMMUException (4 in total)
*/

/* Up to 4 registers that will hold the original address that generated an
** exception. Use load state form for exception resolution.
*/

#define gcregMMUExceptionRegAddrs                                         0x0064
#define GCREG_MMU_EXCEPTION_Address                                      0x00190
#define GCREG_MMU_EXCEPTION_MSB                                               15
#define GCREG_MMU_EXCEPTION_LSB                                                2
#define GCREG_MMU_EXCEPTION_BLK                                                2
#define GCREG_MMU_EXCEPTION_Count                                              4
#define GCREG_MMU_EXCEPTION_FieldMask                                 0xFFFFFFFF
#define GCREG_MMU_EXCEPTION_ReadMask                                  0xFFFFFFFF
#define GCREG_MMU_EXCEPTION_WriteMask                                 0xFFFFFFFF
#define GCREG_MMU_EXCEPTION_ResetValue                                0x00000000

#define GCREG_MMU_EXCEPTION_ADDRESS                                       31 : 0
#define GCREG_MMU_EXCEPTION_ADDRESS_End                                       31
#define GCREG_MMU_EXCEPTION_ADDRESS_Start                                      0
#define GCREG_MMU_EXCEPTION_ADDRESS_Type                                     U32

/*******************************************************************************
** Register gcModulePowerControls
*/

/* Control register for module level power controls. */

#define GC_MODULE_POWER_CONTROLS_Address                                 0x00100
#define GC_MODULE_POWER_CONTROLS_MSB                                          15
#define GC_MODULE_POWER_CONTROLS_LSB                                           0
#define GC_MODULE_POWER_CONTROLS_BLK                                           0
#define GC_MODULE_POWER_CONTROLS_Count                                         1
#define GC_MODULE_POWER_CONTROLS_FieldMask                            0xFFFF00F7
#define GC_MODULE_POWER_CONTROLS_ReadMask                             0xFFFF00F7
#define GC_MODULE_POWER_CONTROLS_WriteMask                            0xFFFF00F7
#define GC_MODULE_POWER_CONTROLS_ResetValue                           0x00140020

/* Enables module level clock gating. */
#define GC_MODULE_POWER_CONTROLS_ENABLE_MODULE_CLOCK_GATING                0 : 0
#define GC_MODULE_POWER_CONTROLS_ENABLE_MODULE_CLOCK_GATING_End                0
#define GC_MODULE_POWER_CONTROLS_ENABLE_MODULE_CLOCK_GATING_Start              0
#define GC_MODULE_POWER_CONTROLS_ENABLE_MODULE_CLOCK_GATING_Type             U01

/* Disables module level clock gating for stall condition. */
#define GC_MODULE_POWER_CONTROLS_DISABLE_STALL_MODULE_CLOCK_GATING         1 : 1
#define GC_MODULE_POWER_CONTROLS_DISABLE_STALL_MODULE_CLOCK_GATING_End         1
#define GC_MODULE_POWER_CONTROLS_DISABLE_STALL_MODULE_CLOCK_GATING_Start       1
#define GC_MODULE_POWER_CONTROLS_DISABLE_STALL_MODULE_CLOCK_GATING_Type      U01

/* Disables module level clock gating for starve/idle condition. */
#define GC_MODULE_POWER_CONTROLS_DISABLE_STARVE_MODULE_CLOCK_GATING        2 : 2
#define GC_MODULE_POWER_CONTROLS_DISABLE_STARVE_MODULE_CLOCK_GATING_End        2
#define GC_MODULE_POWER_CONTROLS_DISABLE_STARVE_MODULE_CLOCK_GATING_Start      2
#define GC_MODULE_POWER_CONTROLS_DISABLE_STARVE_MODULE_CLOCK_GATING_Type     U01

/* Number of clock cycles to wait after turning on the clock. */
#define GC_MODULE_POWER_CONTROLS_TURN_ON_COUNTER                           7 : 4
#define GC_MODULE_POWER_CONTROLS_TURN_ON_COUNTER_End                           7
#define GC_MODULE_POWER_CONTROLS_TURN_ON_COUNTER_Start                         4
#define GC_MODULE_POWER_CONTROLS_TURN_ON_COUNTER_Type                        U04

/* Counter value for clock gating the module if the module is idle for  this  **
** amount of clock cycles.                                                    */
#define GC_MODULE_POWER_CONTROLS_TURN_OFF_COUNTER                        31 : 16
#define GC_MODULE_POWER_CONTROLS_TURN_OFF_COUNTER_End                         31
#define GC_MODULE_POWER_CONTROLS_TURN_OFF_COUNTER_Start                       16
#define GC_MODULE_POWER_CONTROLS_TURN_OFF_COUNTER_Type                       U16

/*******************************************************************************
** Register gcModulePowerModuleControl
*/

/* Module level control registers. */

#define GC_MODULE_POWER_MODULE_CONTROL_Address                           0x00104
#define GC_MODULE_POWER_MODULE_CONTROL_MSB                                    15
#define GC_MODULE_POWER_MODULE_CONTROL_LSB                                     0
#define GC_MODULE_POWER_MODULE_CONTROL_BLK                                     0
#define GC_MODULE_POWER_MODULE_CONTROL_Count                                   1
#define GC_MODULE_POWER_MODULE_CONTROL_FieldMask                      0x00000007
#define GC_MODULE_POWER_MODULE_CONTROL_ReadMask                       0x00000007
#define GC_MODULE_POWER_MODULE_CONTROL_WriteMask                      0x00000007
#define GC_MODULE_POWER_MODULE_CONTROL_ResetValue                     0x00000000

/* Disables module level clock gating for FE. */
#define GC_MODULE_POWER_MODULE_CONTROL_DISABLE_MODULE_CLOCK_GATING_FE      0 : 0
#define GC_MODULE_POWER_MODULE_CONTROL_DISABLE_MODULE_CLOCK_GATING_FE_End      0
#define GC_MODULE_POWER_MODULE_CONTROL_DISABLE_MODULE_CLOCK_GATING_FE_Start    0
#define GC_MODULE_POWER_MODULE_CONTROL_DISABLE_MODULE_CLOCK_GATING_FE_Type   U01

/* Disables module level clock gating for DE. */
#define GC_MODULE_POWER_MODULE_CONTROL_DISABLE_MODULE_CLOCK_GATING_DE      1 : 1
#define GC_MODULE_POWER_MODULE_CONTROL_DISABLE_MODULE_CLOCK_GATING_DE_End      1
#define GC_MODULE_POWER_MODULE_CONTROL_DISABLE_MODULE_CLOCK_GATING_DE_Start    1
#define GC_MODULE_POWER_MODULE_CONTROL_DISABLE_MODULE_CLOCK_GATING_DE_Type   U01

/* Disables module level clock gating for PE. */
#define GC_MODULE_POWER_MODULE_CONTROL_DISABLE_MODULE_CLOCK_GATING_PE      2 : 2
#define GC_MODULE_POWER_MODULE_CONTROL_DISABLE_MODULE_CLOCK_GATING_PE_End      2
#define GC_MODULE_POWER_MODULE_CONTROL_DISABLE_MODULE_CLOCK_GATING_PE_Start    2
#define GC_MODULE_POWER_MODULE_CONTROL_DISABLE_MODULE_CLOCK_GATING_PE_Type   U01

/*******************************************************************************
** Register gcModulePowerModuleStatus
*/

/* Module level control status. */

#define GC_MODULE_POWER_MODULE_STATUS_Address                            0x00108
#define GC_MODULE_POWER_MODULE_STATUS_MSB                                     15
#define GC_MODULE_POWER_MODULE_STATUS_LSB                                      0
#define GC_MODULE_POWER_MODULE_STATUS_BLK                                      0
#define GC_MODULE_POWER_MODULE_STATUS_Count                                    1
#define GC_MODULE_POWER_MODULE_STATUS_FieldMask                       0x00000007
#define GC_MODULE_POWER_MODULE_STATUS_ReadMask                        0x00000007
#define GC_MODULE_POWER_MODULE_STATUS_WriteMask                       0x00000000
#define GC_MODULE_POWER_MODULE_STATUS_ResetValue                      0x00000000

/* Module level clock gating is ON for FE. */
#define GC_MODULE_POWER_MODULE_STATUS_MODULE_CLOCK_GATED_FE                0 : 0
#define GC_MODULE_POWER_MODULE_STATUS_MODULE_CLOCK_GATED_FE_End                0
#define GC_MODULE_POWER_MODULE_STATUS_MODULE_CLOCK_GATED_FE_Start              0
#define GC_MODULE_POWER_MODULE_STATUS_MODULE_CLOCK_GATED_FE_Type             U01

/* Module level clock gating is ON for DE. */
#define GC_MODULE_POWER_MODULE_STATUS_MODULE_CLOCK_GATED_DE                1 : 1
#define GC_MODULE_POWER_MODULE_STATUS_MODULE_CLOCK_GATED_DE_End                1
#define GC_MODULE_POWER_MODULE_STATUS_MODULE_CLOCK_GATED_DE_Start              1
#define GC_MODULE_POWER_MODULE_STATUS_MODULE_CLOCK_GATED_DE_Type             U01

/* Module level clock gating is ON for PE. */
#define GC_MODULE_POWER_MODULE_STATUS_MODULE_CLOCK_GATED_PE                2 : 2
#define GC_MODULE_POWER_MODULE_STATUS_MODULE_CLOCK_GATED_PE_End                2
#define GC_MODULE_POWER_MODULE_STATUS_MODULE_CLOCK_GATED_PE_Start              2
#define GC_MODULE_POWER_MODULE_STATUS_MODULE_CLOCK_GATED_PE_Type             U01

/*******************************************************************************
** State gcregSrcAddress
*/

/* 32-bit aligned base address of the source surface. */

#define gcregSrcAddressRegAddrs                                           0x0480
#define GCREG_SRC_ADDRESS_MSB                                                 15
#define GCREG_SRC_ADDRESS_LSB                                                  0
#define GCREG_SRC_ADDRESS_BLK                                                  0
#define GCREG_SRC_ADDRESS_Count                                                1
#define GCREG_SRC_ADDRESS_FieldMask                                   0xFFFFFFFF
#define GCREG_SRC_ADDRESS_ReadMask                                    0xFFFFFFFC
#define GCREG_SRC_ADDRESS_WriteMask                                   0xFFFFFFFC
#define GCREG_SRC_ADDRESS_ResetValue                                  0x00000000

#define GCREG_SRC_ADDRESS_ADDRESS                                         31 : 0
#define GCREG_SRC_ADDRESS_ADDRESS_End                                         30
#define GCREG_SRC_ADDRESS_ADDRESS_Start                                        0
#define GCREG_SRC_ADDRESS_ADDRESS_Type                                       U31

/*******************************************************************************
** State gcregSrcStride
*/

/* Stride of the source surface in bytes. To calculate the stride multiply
** the surface width in pixels (8-pixel aligned) by the number of bytes per
** pixel.
*/

#define gcregSrcStrideRegAddrs                                            0x0481
#define GCREG_SRC_STRIDE_MSB                                                  15
#define GCREG_SRC_STRIDE_LSB                                                   0
#define GCREG_SRC_STRIDE_BLK                                                   0
#define GCREG_SRC_STRIDE_Count                                                 1
#define GCREG_SRC_STRIDE_FieldMask                                    0x0003FFFF
#define GCREG_SRC_STRIDE_ReadMask                                     0x0003FFFC
#define GCREG_SRC_STRIDE_WriteMask                                    0x0003FFFC
#define GCREG_SRC_STRIDE_ResetValue                                   0x00000000

#define GCREG_SRC_STRIDE_STRIDE                                           17 : 0
#define GCREG_SRC_STRIDE_STRIDE_End                                           17
#define GCREG_SRC_STRIDE_STRIDE_Start                                          0
#define GCREG_SRC_STRIDE_STRIDE_Type                                         U18

/*******************************************************************************
** State gcregSrcRotationConfig
*/

/* 90 degree rotation configuration for the source surface. Width field
** specifies the width of the surface in pixels.
*/

#define gcregSrcRotationConfigRegAddrs                                    0x0482
#define GCREG_SRC_ROTATION_CONFIG_MSB                                         15
#define GCREG_SRC_ROTATION_CONFIG_LSB                                          0
#define GCREG_SRC_ROTATION_CONFIG_BLK                                          0
#define GCREG_SRC_ROTATION_CONFIG_Count                                        1
#define GCREG_SRC_ROTATION_CONFIG_FieldMask                           0x0001FFFF
#define GCREG_SRC_ROTATION_CONFIG_ReadMask                            0x0001FFFF
#define GCREG_SRC_ROTATION_CONFIG_WriteMask                           0x0001FFFF
#define GCREG_SRC_ROTATION_CONFIG_ResetValue                          0x00000000

#define GCREG_SRC_ROTATION_CONFIG_WIDTH                                   15 : 0
#define GCREG_SRC_ROTATION_CONFIG_WIDTH_End                                   15
#define GCREG_SRC_ROTATION_CONFIG_WIDTH_Start                                  0
#define GCREG_SRC_ROTATION_CONFIG_WIDTH_Type                                 U16

#define GCREG_SRC_ROTATION_CONFIG_ROTATION                               16 : 16
#define GCREG_SRC_ROTATION_CONFIG_ROTATION_End                                16
#define GCREG_SRC_ROTATION_CONFIG_ROTATION_Start                              16
#define GCREG_SRC_ROTATION_CONFIG_ROTATION_Type                              U01
#define   GCREG_SRC_ROTATION_CONFIG_ROTATION_DISABLE                         0x0
#define   GCREG_SRC_ROTATION_CONFIG_ROTATION_ENABLE                          0x1

struct gcregsrcrotationconfig {
	/* gcregSrcRotationConfigRegAddrs:GCREG_SRC_ROTATION_CONFIG_WIDTH */
	unsigned int surf_width:16;

	/* gcregSrcRotationConfigRegAddrs:GCREG_SRC_ROTATION_CONFIG_ROTATION */
	unsigned int enable:1;

	/* gcregSrcRotationConfigRegAddrs:reserved */
	unsigned int _reserved_17_31:15;
};

/*******************************************************************************
** State gcregSrcConfig
*/

/* Source surface configuration register. */

#define gcregSrcConfigRegAddrs                                            0x0483
#define GCREG_SRC_CONFIG_MSB                                                  15
#define GCREG_SRC_CONFIG_LSB                                                   0
#define GCREG_SRC_CONFIG_BLK                                                   0
#define GCREG_SRC_CONFIG_Count                                                 1
#define GCREG_SRC_CONFIG_FieldMask                                    0xFF31B1FF
#define GCREG_SRC_CONFIG_ReadMask                                     0xFF31B1FF
#define GCREG_SRC_CONFIG_WriteMask                                    0xFF31B1FF
#define GCREG_SRC_CONFIG_ResetValue                                   0x00000000

/* Control source endianess. */
#define GCREG_SRC_CONFIG_ENDIAN_CONTROL                                  31 : 30
#define GCREG_SRC_CONFIG_ENDIAN_CONTROL_End                                   31
#define GCREG_SRC_CONFIG_ENDIAN_CONTROL_Start                                 30
#define GCREG_SRC_CONFIG_ENDIAN_CONTROL_Type                                 U02
#define   GCREG_SRC_CONFIG_ENDIAN_CONTROL_NO_SWAP                            0x0
#define   GCREG_SRC_CONFIG_ENDIAN_CONTROL_SWAP_WORD                          0x1
#define   GCREG_SRC_CONFIG_ENDIAN_CONTROL_SWAP_DWORD                         0x2

/* Disable 420 L2 cache  NOTE: the field is valid for chips with 420 L2 cache **
** defined.                                                                   */
#define GCREG_SRC_CONFIG_DISABLE420_L2_CACHE                             29 : 29
#define GCREG_SRC_CONFIG_DISABLE420_L2_CACHE_End                              29
#define GCREG_SRC_CONFIG_DISABLE420_L2_CACHE_Start                            29
#define GCREG_SRC_CONFIG_DISABLE420_L2_CACHE_Type                            U01
#define   GCREG_SRC_CONFIG_DISABLE420_L2_CACHE_ENABLED                       0x0
#define   GCREG_SRC_CONFIG_DISABLE420_L2_CACHE_DISABLED                      0x1

/* Defines the pixel format of the source surface. */
#define GCREG_SRC_CONFIG_SOURCE_FORMAT                                   28 : 24
#define GCREG_SRC_CONFIG_SOURCE_FORMAT_End                                    28
#define GCREG_SRC_CONFIG_SOURCE_FORMAT_Start                                  24
#define GCREG_SRC_CONFIG_SOURCE_FORMAT_Type                                  U05
#define   GCREG_SRC_CONFIG_SOURCE_FORMAT_X4R4G4B4                           0x00
#define   GCREG_SRC_CONFIG_SOURCE_FORMAT_A4R4G4B4                           0x01
#define   GCREG_SRC_CONFIG_SOURCE_FORMAT_X1R5G5B5                           0x02
#define   GCREG_SRC_CONFIG_SOURCE_FORMAT_A1R5G5B5                           0x03
#define   GCREG_SRC_CONFIG_SOURCE_FORMAT_R5G6B5                             0x04
#define   GCREG_SRC_CONFIG_SOURCE_FORMAT_X8R8G8B8                           0x05
#define   GCREG_SRC_CONFIG_SOURCE_FORMAT_A8R8G8B8                           0x06
#define   GCREG_SRC_CONFIG_SOURCE_FORMAT_YUY2                               0x07
#define   GCREG_SRC_CONFIG_SOURCE_FORMAT_UYVY                               0x08
#define   GCREG_SRC_CONFIG_SOURCE_FORMAT_INDEX8                             0x09
#define   GCREG_SRC_CONFIG_SOURCE_FORMAT_MONOCHROME                         0x0A
#define   GCREG_SRC_CONFIG_SOURCE_FORMAT_YV12                               0x0F
#define   GCREG_SRC_CONFIG_SOURCE_FORMAT_A8                                 0x10
#define   GCREG_SRC_CONFIG_SOURCE_FORMAT_NV12                               0x11
#define   GCREG_SRC_CONFIG_SOURCE_FORMAT_NV16                               0x12
#define   GCREG_SRC_CONFIG_SOURCE_FORMAT_RG16                               0x13

/* Color channel swizzles. */
#define GCREG_SRC_CONFIG_SWIZZLE                                         21 : 20
#define GCREG_SRC_CONFIG_SWIZZLE_End                                          21
#define GCREG_SRC_CONFIG_SWIZZLE_Start                                        20
#define GCREG_SRC_CONFIG_SWIZZLE_Type                                        U02
#define   GCREG_SRC_CONFIG_SWIZZLE_ARGB                                      0x0
#define   GCREG_SRC_CONFIG_SWIZZLE_RGBA                                      0x1
#define   GCREG_SRC_CONFIG_SWIZZLE_ABGR                                      0x2
#define   GCREG_SRC_CONFIG_SWIZZLE_BGRA                                      0x3

/* Mono expansion: if 0, transparency color will be 0, otherwise transparency **
** color will be 1.                                                           */
#define GCREG_SRC_CONFIG_MONO_TRANSPARENCY                               15 : 15
#define GCREG_SRC_CONFIG_MONO_TRANSPARENCY_End                                15
#define GCREG_SRC_CONFIG_MONO_TRANSPARENCY_Start                              15
#define GCREG_SRC_CONFIG_MONO_TRANSPARENCY_Type                              U01
#define   GCREG_SRC_CONFIG_MONO_TRANSPARENCY_BACKGROUND                      0x0
#define   GCREG_SRC_CONFIG_MONO_TRANSPARENCY_FOREGROUND                      0x1

/* Mono expansion or masked blit: stream packing in pixels. Determines how    **
** many horizontal pixels are there per each 32-bit chunk. For example, if    **
** set to Packed8, each 32-bit chunk is 8-pixel wide, which also means that   **
** it defines 4 vertical lines of pixels.                                     */
#define GCREG_SRC_CONFIG_PACK                                            13 : 12
#define GCREG_SRC_CONFIG_PACK_End                                             13
#define GCREG_SRC_CONFIG_PACK_Start                                           12
#define GCREG_SRC_CONFIG_PACK_Type                                           U02
#define   GCREG_SRC_CONFIG_PACK_PACKED8                                      0x0
#define   GCREG_SRC_CONFIG_PACK_PACKED16                                     0x1
#define   GCREG_SRC_CONFIG_PACK_PACKED32                                     0x2
#define   GCREG_SRC_CONFIG_PACK_UNPACKED                                     0x3

/* Source data location: set to STREAM for mono expansion blits or masked     **
** blits. For mono expansion blits the complete bitmap comes from the command **
** stream. For masked blits the source data comes from the memory and the     **
** mask from the command stream.                                              */
#define GCREG_SRC_CONFIG_LOCATION                                          8 : 8
#define GCREG_SRC_CONFIG_LOCATION_End                                          8
#define GCREG_SRC_CONFIG_LOCATION_Start                                        8
#define GCREG_SRC_CONFIG_LOCATION_Type                                       U01
#define   GCREG_SRC_CONFIG_LOCATION_MEMORY                                   0x0
#define   GCREG_SRC_CONFIG_LOCATION_STREAM                                   0x1

/* Source linear/tiled address computation control. */
#define GCREG_SRC_CONFIG_TILED                                             7 : 7
#define GCREG_SRC_CONFIG_TILED_End                                             7
#define GCREG_SRC_CONFIG_TILED_Start                                           7
#define GCREG_SRC_CONFIG_TILED_Type                                          U01
#define   GCREG_SRC_CONFIG_TILED_DISABLED                                    0x0
#define   GCREG_SRC_CONFIG_TILED_ENABLED                                     0x1

/* If set to ABSOLUTE, the source coordinates are treated as absolute         **
** coordinates inside the source surface. If set to RELATIVE, the source      **
** coordinates are treated as the offsets from the destination coordinates    **
** with the source size equal to the size of the destination.                 */
#define GCREG_SRC_CONFIG_SRC_RELATIVE                                      6 : 6
#define GCREG_SRC_CONFIG_SRC_RELATIVE_End                                      6
#define GCREG_SRC_CONFIG_SRC_RELATIVE_Start                                    6
#define GCREG_SRC_CONFIG_SRC_RELATIVE_Type                                   U01
#define   GCREG_SRC_CONFIG_SRC_RELATIVE_ABSOLUTE                             0x0
#define   GCREG_SRC_CONFIG_SRC_RELATIVE_RELATIVE                             0x1

struct gcregsrcconfig {
	/* gcregSrcConfigRegAddrs:reserved */
	unsigned int _reserved_0_5:6;

	/* gcregSrcConfigRegAddrs:GCREG_SRC_CONFIG_SRC_RELATIVE */
	unsigned int relative:1;

	/* gcregSrcConfigRegAddrs:GCREG_SRC_CONFIG_TILED */
	unsigned int tiled:1;

	/* gcregSrcConfigRegAddrs:GCREG_SRC_CONFIG_LOCATION */
	unsigned int stream:1;

	/* gcregSrcConfigRegAddrs:reserved */
	unsigned int _reserved_9_11:3;

	/* gcregSrcConfigRegAddrs:GCREG_SRC_CONFIG_PACK */
	unsigned int monopack:2;

	/* gcregSrcConfigRegAddrs:reserved */
	unsigned int _reserved_14:1;

	/* gcregSrcConfigRegAddrs:GCREG_SRC_CONFIG_MONO_TRANSPARENCY */
	unsigned int monotransp:1;

	/* gcregSrcConfigRegAddrs:reserved */
	unsigned int _reserved_16_19:4;

	/* gcregSrcConfigRegAddrs:GCREG_SRC_CONFIG_SWIZZLE */
	unsigned int swizzle:2;

	/* gcregSrcConfigRegAddrs:reserved */
	unsigned int _reserved_22_23:2;

	/* gcregSrcConfigRegAddrs:GCREG_SRC_CONFIG_SOURCE_FORMAT */
	unsigned int format:5;

	/* gcregSrcConfigRegAddrs:GCREG_SRC_CONFIG_DISABLE420_L2_CACHE */
	unsigned int disable420L2cache:1;

	/* gcregSrcConfigRegAddrs:GCREG_SRC_CONFIG_ENDIAN_CONTROL */
	unsigned int endian:2;
};

/*******************************************************************************
** State gcregSrcOrigin
*/

/* Absolute or relative (see SRC_RELATIVE field of gcregSrcConfig register) X
** and Y coordinates in pixels of the top left corner of the source rectangle
** within the source surface.
*/

#define gcregSrcOriginRegAddrs                                            0x0484
#define GCREG_SRC_ORIGIN_MSB                                                  15
#define GCREG_SRC_ORIGIN_LSB                                                   0
#define GCREG_SRC_ORIGIN_BLK                                                   0
#define GCREG_SRC_ORIGIN_Count                                                 1
#define GCREG_SRC_ORIGIN_FieldMask                                    0xFFFFFFFF
#define GCREG_SRC_ORIGIN_ReadMask                                     0xFFFFFFFF
#define GCREG_SRC_ORIGIN_WriteMask                                    0xFFFFFFFF
#define GCREG_SRC_ORIGIN_ResetValue                                   0x00000000

#define GCREG_SRC_ORIGIN_Y                                               31 : 16
#define GCREG_SRC_ORIGIN_Y_End                                                31
#define GCREG_SRC_ORIGIN_Y_Start                                              16
#define GCREG_SRC_ORIGIN_Y_Type                                              U16

#define GCREG_SRC_ORIGIN_X                                                15 : 0
#define GCREG_SRC_ORIGIN_X_End                                                15
#define GCREG_SRC_ORIGIN_X_Start                                               0
#define GCREG_SRC_ORIGIN_X_Type                                              U16

struct gcregsrcorigin {
	/* gcregSrcOriginRegAddrs:GCREG_SRC_ORIGIN_X */
	unsigned int x:16;

	/* gcregSrcOriginRegAddrs:GCREG_SRC_ORIGIN_Y */
	unsigned int y:16;
};

static const struct gcregsrcorigin gcregsrcorigin_min = {
	/* gcregSrcOriginRegAddrs:GCREG_SRC_ORIGIN_X */
	0,

	/* gcregSrcOriginRegAddrs:GCREG_SRC_ORIGIN_Y */
	0
};

/*******************************************************************************
** State gcregSrcSize
*/

/* Width and height of the source rectangle in pixels. If the source is
** relative (see SRC_RELATIVE field of gcregSrcConfig register) or a regular
** bitblt is being performed without stretching, this register is ignored and
** the source size is assumed to be the same as the destination.
*/

#define gcregSrcSizeRegAddrs                                              0x0485
#define GCREG_SRC_SIZE_MSB                                                    15
#define GCREG_SRC_SIZE_LSB                                                     0
#define GCREG_SRC_SIZE_BLK                                                     0
#define GCREG_SRC_SIZE_Count                                                   1
#define GCREG_SRC_SIZE_FieldMask                                      0xFFFFFFFF
#define GCREG_SRC_SIZE_ReadMask                                       0xFFFFFFFF
#define GCREG_SRC_SIZE_WriteMask                                      0xFFFFFFFF
#define GCREG_SRC_SIZE_ResetValue                                     0x00000000

#define GCREG_SRC_SIZE_Y                                                 31 : 16
#define GCREG_SRC_SIZE_Y_End                                                  31
#define GCREG_SRC_SIZE_Y_Start                                                16
#define GCREG_SRC_SIZE_Y_Type                                                U16

#define GCREG_SRC_SIZE_X                                                  15 : 0
#define GCREG_SRC_SIZE_X_End                                                  15
#define GCREG_SRC_SIZE_X_Start                                                 0
#define GCREG_SRC_SIZE_X_Type                                                U16

struct gcregsrcsize {
	/* gcregSrcOriginRegAddrs:GCREG_SRC_SIZE_X */
	unsigned int width:16;

	/* gcregSrcOriginRegAddrs:GCREG_SRC_SIZE_Y */
	unsigned int height:16;
};

static const struct gcregsrcsize gcregsrcsize_max = {
	/* gcregSrcOriginRegAddrs:GCREG_SRC_SIZE_X */
	32767,

	/* gcregSrcOriginRegAddrs:GCREG_SRC_SIZE_Y */
	32767
};

/*******************************************************************************
** State gcregSrcColorBg
*/

/* In mono expansion defines the source color if the mono pixel is 0. The color
** must be set in A8R8G8B8 format. In color blits defines the source
** transparency color and must be of the same format as the source surface.
*/

#define gcregSrcColorBgRegAddrs                                           0x0486
#define GCREG_SRC_COLOR_BG_MSB                                                15
#define GCREG_SRC_COLOR_BG_LSB                                                 0
#define GCREG_SRC_COLOR_BG_BLK                                                 0
#define GCREG_SRC_COLOR_BG_Count                                               1
#define GCREG_SRC_COLOR_BG_FieldMask                                  0xFFFFFFFF
#define GCREG_SRC_COLOR_BG_ReadMask                                   0xFFFFFFFF
#define GCREG_SRC_COLOR_BG_WriteMask                                  0xFFFFFFFF
#define GCREG_SRC_COLOR_BG_ResetValue                                 0x00000000

#define GCREG_SRC_COLOR_BG_ALPHA                                         31 : 24
#define GCREG_SRC_COLOR_BG_ALPHA_End                                          31
#define GCREG_SRC_COLOR_BG_ALPHA_Start                                        24
#define GCREG_SRC_COLOR_BG_ALPHA_Type                                        U08

#define GCREG_SRC_COLOR_BG_RED                                           23 : 16
#define GCREG_SRC_COLOR_BG_RED_End                                            23
#define GCREG_SRC_COLOR_BG_RED_Start                                          16
#define GCREG_SRC_COLOR_BG_RED_Type                                          U08

#define GCREG_SRC_COLOR_BG_GREEN                                          15 : 8
#define GCREG_SRC_COLOR_BG_GREEN_End                                          15
#define GCREG_SRC_COLOR_BG_GREEN_Start                                         8
#define GCREG_SRC_COLOR_BG_GREEN_Type                                        U08

#define GCREG_SRC_COLOR_BG_BLUE                                            7 : 0
#define GCREG_SRC_COLOR_BG_BLUE_End                                            7
#define GCREG_SRC_COLOR_BG_BLUE_Start                                          0
#define GCREG_SRC_COLOR_BG_BLUE_Type                                         U08

/*******************************************************************************
** State gcregSrcColorFg
*/

/* In mono expansion defines the source color if the mono pixel is 1. The color
** must be set in A8R8G8B8.
*/

#define gcregSrcColorFgRegAddrs                                           0x0487
#define GCREG_SRC_COLOR_FG_MSB                                                15
#define GCREG_SRC_COLOR_FG_LSB                                                 0
#define GCREG_SRC_COLOR_FG_BLK                                                 0
#define GCREG_SRC_COLOR_FG_Count                                               1
#define GCREG_SRC_COLOR_FG_FieldMask                                  0xFFFFFFFF
#define GCREG_SRC_COLOR_FG_ReadMask                                   0xFFFFFFFF
#define GCREG_SRC_COLOR_FG_WriteMask                                  0xFFFFFFFF
#define GCREG_SRC_COLOR_FG_ResetValue                                 0x00000000

#define GCREG_SRC_COLOR_FG_ALPHA                                         31 : 24
#define GCREG_SRC_COLOR_FG_ALPHA_End                                          31
#define GCREG_SRC_COLOR_FG_ALPHA_Start                                        24
#define GCREG_SRC_COLOR_FG_ALPHA_Type                                        U08

#define GCREG_SRC_COLOR_FG_RED                                           23 : 16
#define GCREG_SRC_COLOR_FG_RED_End                                            23
#define GCREG_SRC_COLOR_FG_RED_Start                                          16
#define GCREG_SRC_COLOR_FG_RED_Type                                          U08

#define GCREG_SRC_COLOR_FG_GREEN                                          15 : 8
#define GCREG_SRC_COLOR_FG_GREEN_End                                          15
#define GCREG_SRC_COLOR_FG_GREEN_Start                                         8
#define GCREG_SRC_COLOR_FG_GREEN_Type                                        U08

#define GCREG_SRC_COLOR_FG_BLUE                                            7 : 0
#define GCREG_SRC_COLOR_FG_BLUE_End                                            7
#define GCREG_SRC_COLOR_FG_BLUE_Start                                          0
#define GCREG_SRC_COLOR_FG_BLUE_Type                                         U08

/*******************************************************************************
** State gcregStretchFactorLow
*/

#define gcregStretchFactorLowRegAddrs                                     0x0488
#define GCREG_STRETCH_FACTOR_LOW_MSB                                          15
#define GCREG_STRETCH_FACTOR_LOW_LSB                                           0
#define GCREG_STRETCH_FACTOR_LOW_BLK                                           0
#define GCREG_STRETCH_FACTOR_LOW_Count                                         1
#define GCREG_STRETCH_FACTOR_LOW_FieldMask                            0x7FFFFFFF
#define GCREG_STRETCH_FACTOR_LOW_ReadMask                             0x7FFFFFFF
#define GCREG_STRETCH_FACTOR_LOW_WriteMask                            0x7FFFFFFF
#define GCREG_STRETCH_FACTOR_LOW_ResetValue                           0x00000000

/* Horizontal stretch factor in 15.16 fixed point format. The value is        **
** calculated using the following formula: factor = ((srcWidth - 1) << 16) /  **
** (dstWidth - 1). Stretch blit uses only the integer part of the value,      **
** while Filter blit uses all 31 bits.                                        */
#define GCREG_STRETCH_FACTOR_LOW_X                                        30 : 0
#define GCREG_STRETCH_FACTOR_LOW_X_End                                        30
#define GCREG_STRETCH_FACTOR_LOW_X_Start                                       0
#define GCREG_STRETCH_FACTOR_LOW_X_Type                                      U31

/*******************************************************************************
** State gcregStretchFactorHigh
*/

#define gcregStretchFactorHighRegAddrs                                    0x0489
#define GCREG_STRETCH_FACTOR_HIGH_MSB                                         15
#define GCREG_STRETCH_FACTOR_HIGH_LSB                                          0
#define GCREG_STRETCH_FACTOR_LOW_HIGH_BLK                                      0
#define GCREG_STRETCH_FACTOR_HIGH_Count                                        1
#define GCREG_STRETCH_FACTOR_HIGH_FieldMask                           0x7FFFFFFF
#define GCREG_STRETCH_FACTOR_HIGH_ReadMask                            0x7FFFFFFF
#define GCREG_STRETCH_FACTOR_HIGH_WriteMask                           0x7FFFFFFF
#define GCREG_STRETCH_FACTOR_HIGH_ResetValue                          0x00000000

/* Vertical stretch factor in 15.16 fixed point format. The value is          **
** calculated using the following formula: factor = ((srcHeight - 1) << 16) / **
** (dstHeight - 1). Stretch blit uses only the integer part of the value,     **
** while Filter blit uses all 31 bits.                                        */
#define GCREG_STRETCH_FACTOR_HIGH_Y                                       30 : 0
#define GCREG_STRETCH_FACTOR_HIGH_Y_End                                       30
#define GCREG_STRETCH_FACTOR_HIGH_Y_Start                                      0
#define GCREG_STRETCH_FACTOR_HIGH_Y_Type                                     U31

/*******************************************************************************
** State gcregDestAddress
*/

/* 32-bit aligned base address of the destination surface. */

#define gcregDestAddressRegAddrs                                          0x048A
#define GCREG_DEST_ADDRESS_MSB                                                15
#define GCREG_DEST_ADDRESS_LSB                                                 0
#define GCREG_DEST_ADDRESS_BLK                                                 0
#define GCREG_DEST_ADDRESS_Count                                               1
#define GCREG_DEST_ADDRESS_FieldMask                                  0xFFFFFFFF
#define GCREG_DEST_ADDRESS_ReadMask                                   0xFFFFFFFC
#define GCREG_DEST_ADDRESS_WriteMask                                  0xFFFFFFFC
#define GCREG_DEST_ADDRESS_ResetValue                                 0x00000000

#define GCREG_DEST_ADDRESS_ADDRESS                                        31 : 0
#define GCREG_DEST_ADDRESS_ADDRESS_End                                        30
#define GCREG_DEST_ADDRESS_ADDRESS_Start                                       0
#define GCREG_DEST_ADDRESS_ADDRESS_Type                                      U31

/*******************************************************************************
** State gcregDestStride
*/

/* Stride of the destination surface in bytes. To calculate the stride
** multiply the surface width in pixels (8-pixel aligned) by the number of
** bytes per pixel.
*/

#define gcregDestStrideRegAddrs                                           0x048B
#define GCREG_DEST_STRIDE_MSB                                                 15
#define GCREG_DEST_STRIDE_LSB                                                  0
#define GCREG_DEST_STRIDE_BLK                                                  0
#define GCREG_DEST_STRIDE_Count                                                1
#define GCREG_DEST_STRIDE_FieldMask                                   0x0003FFFF
#define GCREG_DEST_STRIDE_ReadMask                                    0x0003FFFC
#define GCREG_DEST_STRIDE_WriteMask                                   0x0003FFFC
#define GCREG_DEST_STRIDE_ResetValue                                  0x00000000

#define GCREG_DEST_STRIDE_STRIDE                                          17 : 0
#define GCREG_DEST_STRIDE_STRIDE_End                                          17
#define GCREG_DEST_STRIDE_STRIDE_Start                                         0
#define GCREG_DEST_STRIDE_STRIDE_Type                                        U18

/*******************************************************************************
** State gcregDestRotationConfig
*/

/* 90 degree rotation configuration for the destination surface. Width field
** specifies the width of the surface in pixels.
*/

#define gcregDestRotationConfigRegAddrs                                   0x048C
#define GCREG_DEST_ROTATION_CONFIG_MSB                                        15
#define GCREG_DEST_ROTATION_CONFIG_LSB                                         0
#define GCREG_DEST_ROTATION_CONFIG_BLK                                         0
#define GCREG_DEST_ROTATION_CONFIG_Count                                       1
#define GCREG_DEST_ROTATION_CONFIG_FieldMask                          0x0001FFFF
#define GCREG_DEST_ROTATION_CONFIG_ReadMask                           0x0001FFFF
#define GCREG_DEST_ROTATION_CONFIG_WriteMask                          0x0001FFFF
#define GCREG_DEST_ROTATION_CONFIG_ResetValue                         0x00000000

#define GCREG_DEST_ROTATION_CONFIG_WIDTH                                  15 : 0
#define GCREG_DEST_ROTATION_CONFIG_WIDTH_End                                  15
#define GCREG_DEST_ROTATION_CONFIG_WIDTH_Start                                 0
#define GCREG_DEST_ROTATION_CONFIG_WIDTH_Type                                U16

#define GCREG_DEST_ROTATION_CONFIG_ROTATION                              16 : 16
#define GCREG_DEST_ROTATION_CONFIG_ROTATION_End                               16
#define GCREG_DEST_ROTATION_CONFIG_ROTATION_Start                             16
#define GCREG_DEST_ROTATION_CONFIG_ROTATION_Type                             U01
#define   GCREG_DEST_ROTATION_CONFIG_ROTATION_DISABLE                        0x0
#define   GCREG_DEST_ROTATION_CONFIG_ROTATION_ENABLE                         0x1

struct gcregdstrotationconfig {
	/* gcregDestRotationConfigRegAddrs:GCREG_DEST_ROTATION_CONFIG_WIDTH */
	unsigned int surf_width:16;

	/* gcregDestRotationConfigRegAddrs:GCREG_DEST_ROTATION_CONFIG_ROTATION*/
	unsigned int enable:1;

	/* gcregDestRotationConfigRegAddrs:reserved */
	unsigned int _reserved_17_31:15;
};

/*******************************************************************************
** State gcregDestConfig
*/

/* Destination surface configuration register. */

#define gcregDestConfigRegAddrs                                           0x048D
#define GCREG_DEST_CONFIG_MSB                                                 15
#define GCREG_DEST_CONFIG_LSB                                                  0
#define GCREG_DEST_CONFIG_BLK                                                  0
#define GCREG_DEST_CONFIG_Count                                                1
#define GCREG_DEST_CONFIG_FieldMask                                   0x0733F11F
#define GCREG_DEST_CONFIG_ReadMask                                    0x0733F11F
#define GCREG_DEST_CONFIG_WriteMask                                   0x0733F11F
#define GCREG_DEST_CONFIG_ResetValue                                  0x00000000

/* MinorTile. */
#define GCREG_DEST_CONFIG_MINOR_TILED                                    26 : 26
#define GCREG_DEST_CONFIG_MINOR_TILED_End                                     26
#define GCREG_DEST_CONFIG_MINOR_TILED_Start                                   26
#define GCREG_DEST_CONFIG_MINOR_TILED_Type                                   U01
#define   GCREG_DEST_CONFIG_MINOR_TILED_DISABLED                             0x0
#define   GCREG_DEST_CONFIG_MINOR_TILED_ENABLED                              0x1

/* Performance fix for de. */
#define GCREG_DEST_CONFIG_INTER_TILE_PER_FIX                             25 : 25
#define GCREG_DEST_CONFIG_INTER_TILE_PER_FIX_End                              25
#define GCREG_DEST_CONFIG_INTER_TILE_PER_FIX_Start                            25
#define GCREG_DEST_CONFIG_INTER_TILE_PER_FIX_Type                            U01
#define   GCREG_DEST_CONFIG_INTER_TILE_PER_FIX_DISABLED                      0x1
#define   GCREG_DEST_CONFIG_INTER_TILE_PER_FIX_ENABLED                       0x0

/* Control GDI Strecth Blit. */
#define GCREG_DEST_CONFIG_GDI_STRE                                       24 : 24
#define GCREG_DEST_CONFIG_GDI_STRE_End                                        24
#define GCREG_DEST_CONFIG_GDI_STRE_Start                                      24
#define GCREG_DEST_CONFIG_GDI_STRE_Type                                      U01
#define   GCREG_DEST_CONFIG_GDI_STRE_DISABLED                                0x0
#define   GCREG_DEST_CONFIG_GDI_STRE_ENABLED                                 0x1

/* Control destination endianess. */
#define GCREG_DEST_CONFIG_ENDIAN_CONTROL                                 21 : 20
#define GCREG_DEST_CONFIG_ENDIAN_CONTROL_End                                  21
#define GCREG_DEST_CONFIG_ENDIAN_CONTROL_Start                                20
#define GCREG_DEST_CONFIG_ENDIAN_CONTROL_Type                                U02
#define   GCREG_DEST_CONFIG_ENDIAN_CONTROL_NO_SWAP                           0x0
#define   GCREG_DEST_CONFIG_ENDIAN_CONTROL_SWAP_WORD                         0x1
#define   GCREG_DEST_CONFIG_ENDIAN_CONTROL_SWAP_DWORD                        0x2

/* Color channel swizzles. */
#define GCREG_DEST_CONFIG_SWIZZLE                                        17 : 16
#define GCREG_DEST_CONFIG_SWIZZLE_End                                         17
#define GCREG_DEST_CONFIG_SWIZZLE_Start                                       16
#define GCREG_DEST_CONFIG_SWIZZLE_Type                                       U02
#define   GCREG_DEST_CONFIG_SWIZZLE_ARGB                                     0x0
#define   GCREG_DEST_CONFIG_SWIZZLE_RGBA                                     0x1
#define   GCREG_DEST_CONFIG_SWIZZLE_ABGR                                     0x2
#define   GCREG_DEST_CONFIG_SWIZZLE_BGRA                                     0x3

/* Determines the type of primitive to be rendered. BIT_BLT_REVERSED and      **
** INVALID_COMMAND values are defined for internal use and should not be      **
** used.                                                                      */
#define GCREG_DEST_CONFIG_COMMAND                                        15 : 12
#define GCREG_DEST_CONFIG_COMMAND_End                                         15
#define GCREG_DEST_CONFIG_COMMAND_Start                                       12
#define GCREG_DEST_CONFIG_COMMAND_Type                                       U04
#define   GCREG_DEST_CONFIG_COMMAND_CLEAR                                    0x0
#define   GCREG_DEST_CONFIG_COMMAND_LINE                                     0x1
#define   GCREG_DEST_CONFIG_COMMAND_BIT_BLT                                  0x2
#define   GCREG_DEST_CONFIG_COMMAND_BIT_BLT_REVERSED                         0x3
#define   GCREG_DEST_CONFIG_COMMAND_STRETCH_BLT                              0x4
#define   GCREG_DEST_CONFIG_COMMAND_HOR_FILTER_BLT                           0x5
#define   GCREG_DEST_CONFIG_COMMAND_VER_FILTER_BLT                           0x6
#define   GCREG_DEST_CONFIG_COMMAND_ONE_PASS_FILTER_BLT                      0x7
#define   GCREG_DEST_CONFIG_COMMAND_MULTI_SOURCE_BLT                         0x8

/* Destination linear/tiled address computation control. Reserved field for   **
** future expansion. */
#define GCREG_DEST_CONFIG_TILED                                            8 : 8
#define GCREG_DEST_CONFIG_TILED_End                                            8
#define GCREG_DEST_CONFIG_TILED_Start                                          8
#define GCREG_DEST_CONFIG_TILED_Type                                         U01
#define   GCREG_DEST_CONFIG_TILED_DISABLED                                   0x0
#define   GCREG_DEST_CONFIG_TILED_ENABLED                                    0x1

/* Defines the pixel format of the destination surface. */
#define GCREG_DEST_CONFIG_FORMAT                                           4 : 0
#define GCREG_DEST_CONFIG_FORMAT_End                                           4
#define GCREG_DEST_CONFIG_FORMAT_Start                                         0
#define GCREG_DEST_CONFIG_FORMAT_Type                                        U05
#define   GCREG_DEST_CONFIG_FORMAT_X4R4G4B4                                 0x00
#define   GCREG_DEST_CONFIG_FORMAT_A4R4G4B4                                 0x01
#define   GCREG_DEST_CONFIG_FORMAT_X1R5G5B5                                 0x02
#define   GCREG_DEST_CONFIG_FORMAT_A1R5G5B5                                 0x03
#define   GCREG_DEST_CONFIG_FORMAT_R5G6B5                                   0x04
#define   GCREG_DEST_CONFIG_FORMAT_X8R8G8B8                                 0x05
#define   GCREG_DEST_CONFIG_FORMAT_A8R8G8B8                                 0x06
#define   GCREG_DEST_CONFIG_FORMAT_YUY2                                     0x07
#define   GCREG_DEST_CONFIG_FORMAT_UYVY                                     0x08
#define   GCREG_DEST_CONFIG_FORMAT_INDEX8                                   0x09
#define   GCREG_DEST_CONFIG_FORMAT_MONOCHROME                               0x0A
#define   GCREG_DEST_CONFIG_FORMAT_YV12                                     0x0F
#define   GCREG_DEST_CONFIG_FORMAT_A8                                       0x10
#define   GCREG_DEST_CONFIG_FORMAT_NV12                                     0x11
#define   GCREG_DEST_CONFIG_FORMAT_NV16                                     0x12
#define   GCREG_DEST_CONFIG_FORMAT_RG16                                     0x13

struct gcregdstconfig {
	/* gcregDestConfigRegAddrs:GCREG_DEST_CONFIG_FORMAT */
	unsigned int format:5;

	/* gcregDestConfigRegAddrs:reserved */
	unsigned int _reserved_5_7:3;

	/* gcregDestConfigRegAddrs:GCREG_DEST_CONFIG_TILED */
	unsigned int tiled:1;

	/* gcregDestConfigRegAddrs:reserved */
	unsigned int _reserved_9_11:3;

	/* gcregDestConfigRegAddrs:GCREG_DEST_CONFIG_COMMAND */
	unsigned int command:4;

	/* gcregDestConfigRegAddrs:GCREG_DEST_CONFIG_SWIZZLE */
	unsigned int swizzle:2;

	/* gcregDestConfigRegAddrs:reserved */
	unsigned int _reserved_18_19:2;

	/* gcregDestConfigRegAddrs:GCREG_DEST_CONFIG_ENDIAN_CONTROL */
	unsigned int endian:2;

	/* gcregDestConfigRegAddrs:reserved */
	unsigned int _reserved_22_23:2;

	/* gcregDestConfigRegAddrs:GCREG_DEST_CONFIG_GDI_STRE */
	unsigned int gdi:1;

	/* gcregDestConfigRegAddrs:GCREG_DEST_CONFIG_INTER_TILE_PER_FIX */
	unsigned int inner_tile_fix:1;

	/* gcregDestConfigRegAddrs:GCREG_DEST_CONFIG_MINOR_TILED */
	unsigned int minor_tile:1;

	/* gcregDestConfigRegAddrs:reserved */
	unsigned int _reserved_27_31:5;
};

/*******************************************************************************
** State gcregFilterKernel
*/

/* Filter blit coefficient table. The algorithm uses 5 bits of pixel
** coordinate's fraction to index the kernel array, which makes it a 32-entry
** array. Each entry consists of 9 kernel values. In practice we store only a
** half of the table, because the other half is a mirror of the first,
** therefore:
**     rows_to_store    = 32 / 2 + 1 = 17
**     values_to_store  = rows_to_store * 9 = 153
**     even_value_count = (values_to_store + 1) & ~1 = 154
**     dword_count      = even_value_count / 2 = 77
*/

#define gcregFilterKernelRegAddrs                                         0x0600
#define GCREG_FILTER_KERNEL_MSB                                               15
#define GCREG_FILTER_KERNEL_LSB                                                7
#define GCREG_FILTER_KERNEL_BLK                                                7
#define GCREG_FILTER_KERNEL_Count                                            128
#define GCREG_FILTER_KERNEL_FieldMask                                 0xFFFFFFFF
#define GCREG_FILTER_KERNEL_ReadMask                                  0xFFFFFFFF
#define GCREG_FILTER_KERNEL_WriteMask                                 0xFFFFFFFF
#define GCREG_FILTER_KERNEL_ResetValue                                0x00000000

#define GCREG_FILTER_KERNEL_COEFFICIENT0                                  15 : 0
#define GCREG_FILTER_KERNEL_COEFFICIENT0_End                                  15
#define GCREG_FILTER_KERNEL_COEFFICIENT0_Start                                 0
#define GCREG_FILTER_KERNEL_COEFFICIENT0_Type                                U16

#define GCREG_FILTER_KERNEL_COEFFICIENT1                                 31 : 16
#define GCREG_FILTER_KERNEL_COEFFICIENT1_End                                  31
#define GCREG_FILTER_KERNEL_COEFFICIENT1_Start                                16
#define GCREG_FILTER_KERNEL_COEFFICIENT1_Type                                U16

/*******************************************************************************
** State gcregHoriFilterKernel
*/

/* One Pass filter Filter blit hori coefficient table. */

#define gcregHoriFilterKernelRegAddrs                                     0x0A00
#define GCREG_HORI_FILTER_KERNEL_MSB                                          15
#define GCREG_HORI_FILTER_KERNEL_LSB                                           7
#define GCREG_HORI_FILTER_KERNEL_BLK                                           7
#define GCREG_HORI_FILTER_KERNEL_Count                                       128
#define GCREG_HORI_FILTER_KERNEL_FieldMask                            0xFFFFFFFF
#define GCREG_HORI_FILTER_KERNEL_ReadMask                             0xFFFFFFFF
#define GCREG_HORI_FILTER_KERNEL_WriteMask                            0xFFFFFFFF
#define GCREG_HORI_FILTER_KERNEL_ResetValue                           0x00000000

#define GCREG_HORI_FILTER_KERNEL_COEFFICIENT0                             15 : 0
#define GCREG_HORI_FILTER_KERNEL_COEFFICIENT0_End                             15
#define GCREG_HORI_FILTER_KERNEL_COEFFICIENT0_Start                            0
#define GCREG_HORI_FILTER_KERNEL_COEFFICIENT0_Type                           U16

#define GCREG_HORI_FILTER_KERNEL_COEFFICIENT1                            31 : 16
#define GCREG_HORI_FILTER_KERNEL_COEFFICIENT1_End                             31
#define GCREG_HORI_FILTER_KERNEL_COEFFICIENT1_Start                           16
#define GCREG_HORI_FILTER_KERNEL_COEFFICIENT1_Type                           U16

/*******************************************************************************
** State gcregVertiFilterKernel
*/

/* One Pass Filter blit vertical coefficient table. */

#define gcregVertiFilterKernelRegAddrs                                    0x0A80
#define GCREG_VERTI_FILTER_KERNEL_MSB                                         15
#define GCREG_VERTI_FILTER_KERNEL_LSB                                          7
#define GCREG_VERTI_FILTER_KERNEL_BLK                                          7
#define GCREG_VERTI_FILTER_KERNEL_Count                                      128
#define GCREG_VERTI_FILTER_KERNEL_FieldMask                           0xFFFFFFFF
#define GCREG_VERTI_FILTER_KERNEL_ReadMask                            0xFFFFFFFF
#define GCREG_VERTI_FILTER_KERNEL_WriteMask                           0xFFFFFFFF
#define GCREG_VERTI_FILTER_KERNEL_ResetValue                          0x00000000

#define GCREG_VERTI_FILTER_KERNEL_COEFFICIENT0                            15 : 0
#define GCREG_VERTI_FILTER_KERNEL_COEFFICIENT0_End                            15
#define GCREG_VERTI_FILTER_KERNEL_COEFFICIENT0_Start                           0
#define GCREG_VERTI_FILTER_KERNEL_COEFFICIENT0_Type                          U16

#define GCREG_VERTI_FILTER_KERNEL_COEFFICIENT1                           31 : 16
#define GCREG_VERTI_FILTER_KERNEL_COEFFICIENT1_End                            31
#define GCREG_VERTI_FILTER_KERNEL_COEFFICIENT1_Start                          16
#define GCREG_VERTI_FILTER_KERNEL_COEFFICIENT1_Type                          U16

/*******************************************************************************
** State gcregVRSourceImageLow
*/

/* Bounding box of the source image. */

#define gcregVRSourceImageLowRegAddrs                                     0x04A6
#define GCREG_VR_SOURCE_IMAGE_LOW_Address                                0x01298
#define GCREG_VR_SOURCE_IMAGE_LOW_MSB                                         15
#define GCREG_VR_SOURCE_IMAGE_LOW_LSB                                          0
#define GCREG_VR_SOURCE_IMAGE_LOW_BLK                                          0
#define GCREG_VR_SOURCE_IMAGE_LOW_Count                                        1
#define GCREG_VR_SOURCE_IMAGE_LOW_FieldMask                           0xFFFFFFFF
#define GCREG_VR_SOURCE_IMAGE_LOW_ReadMask                            0xFFFFFFFF
#define GCREG_VR_SOURCE_IMAGE_LOW_WriteMask                           0xFFFFFFFF
#define GCREG_VR_SOURCE_IMAGE_LOW_ResetValue                          0x00000000

#define GCREG_VR_SOURCE_IMAGE_LOW_LEFT                                    15 : 0
#define GCREG_VR_SOURCE_IMAGE_LOW_LEFT_End                                    15
#define GCREG_VR_SOURCE_IMAGE_LOW_LEFT_Start                                   0
#define GCREG_VR_SOURCE_IMAGE_LOW_LEFT_Type                                  U16

#define GCREG_VR_SOURCE_IMAGE_LOW_TOP                                    31 : 16
#define GCREG_VR_SOURCE_IMAGE_LOW_TOP_End                                     31
#define GCREG_VR_SOURCE_IMAGE_LOW_TOP_Start                                   16
#define GCREG_VR_SOURCE_IMAGE_LOW_TOP_Type                                   U16

/*******************************************************************************
** State gcregVRSourceImageHigh
*/

#define gcregVRSourceImageHighRegAddrs                                    0x04A7
#define GCREG_VR_SOURCE_IMAGE_HIGH_MSB                                        15
#define GCREG_VR_SOURCE_IMAGE_HIGH_LSB                                         0
#define GCREG_VR_SOURCE_IMAGE_LOW_HIGH_BLK                                     0
#define GCREG_VR_SOURCE_IMAGE_HIGH_Count                                       1
#define GCREG_VR_SOURCE_IMAGE_HIGH_FieldMask                          0xFFFFFFFF
#define GCREG_VR_SOURCE_IMAGE_HIGH_ReadMask                           0xFFFFFFFF
#define GCREG_VR_SOURCE_IMAGE_HIGH_WriteMask                          0xFFFFFFFF
#define GCREG_VR_SOURCE_IMAGE_HIGH_ResetValue                         0x00000000

#define GCREG_VR_SOURCE_IMAGE_HIGH_RIGHT                                  15 : 0
#define GCREG_VR_SOURCE_IMAGE_HIGH_RIGHT_End                                  15
#define GCREG_VR_SOURCE_IMAGE_HIGH_RIGHT_Start                                 0
#define GCREG_VR_SOURCE_IMAGE_HIGH_RIGHT_Type                                U16

#define GCREG_VR_SOURCE_IMAGE_HIGH_BOTTOM                                31 : 16
#define GCREG_VR_SOURCE_IMAGE_HIGH_BOTTOM_End                                 31
#define GCREG_VR_SOURCE_IMAGE_HIGH_BOTTOM_Start                               16
#define GCREG_VR_SOURCE_IMAGE_HIGH_BOTTOM_Type                               U16

/*******************************************************************************
** State gcregVRSourceOriginLow
*/

/* Fractional origin of the source window to be rendered within the source
** image.
*/

#define gcregVRSourceOriginLowRegAddrs                                    0x04A8
#define GCREG_VR_SOURCE_ORIGIN_LOW_MSB                                        15
#define GCREG_VR_SOURCE_ORIGIN_LOW_LSB                                         0
#define GCREG_VR_SOURCE_ORIGIN_LOW_BLK                                         0
#define GCREG_VR_SOURCE_ORIGIN_LOW_Count                                       1
#define GCREG_VR_SOURCE_ORIGIN_LOW_FieldMask                          0xFFFFFFFF
#define GCREG_VR_SOURCE_ORIGIN_LOW_ReadMask                           0xFFFFFFFF
#define GCREG_VR_SOURCE_ORIGIN_LOW_WriteMask                          0xFFFFFFFF
#define GCREG_VR_SOURCE_ORIGIN_LOW_ResetValue                         0x00000000

#define GCREG_VR_SOURCE_ORIGIN_LOW_X                                      31 : 0
#define GCREG_VR_SOURCE_ORIGIN_LOW_X_End                                      31
#define GCREG_VR_SOURCE_ORIGIN_LOW_X_Start                                     0
#define GCREG_VR_SOURCE_ORIGIN_LOW_X_Type                                    U32

/*******************************************************************************
** State gcregVRSourceOriginHigh
*/

#define gcregVRSourceOriginHighRegAddrs                                   0x04A9
#define GCREG_VR_SOURCE_ORIGIN_HIGH_MSB                                       15
#define GCREG_VR_SOURCE_ORIGIN_HIGH_LSB                                        0
#define GCREG_VR_SOURCE_ORIGIN_LOW_HIGH_BLK                                    0
#define GCREG_VR_SOURCE_ORIGIN_HIGH_Count                                      1
#define GCREG_VR_SOURCE_ORIGIN_HIGH_FieldMask                         0xFFFFFFFF
#define GCREG_VR_SOURCE_ORIGIN_HIGH_ReadMask                          0xFFFFFFFF
#define GCREG_VR_SOURCE_ORIGIN_HIGH_WriteMask                         0xFFFFFFFF
#define GCREG_VR_SOURCE_ORIGIN_HIGH_ResetValue                        0x00000000

#define GCREG_VR_SOURCE_ORIGIN_HIGH_Y                                     31 : 0
#define GCREG_VR_SOURCE_ORIGIN_HIGH_Y_End                                     31
#define GCREG_VR_SOURCE_ORIGIN_HIGH_Y_Start                                    0
#define GCREG_VR_SOURCE_ORIGIN_HIGH_Y_Type                                   U32

/*******************************************************************************
** State gcregVRTargetWindowLow
*/

/* Bounding box of the destination window to be rendered within the
** destination image.
*/

#define gcregVRTargetWindowLowRegAddrs                                    0x04AA
#define GCREG_VR_TARGET_WINDOW_LOW_Address                               0x012A8
#define GCREG_VR_TARGET_WINDOW_LOW_MSB                                        15
#define GCREG_VR_TARGET_WINDOW_LOW_LSB                                         0
#define GCREG_VR_TARGET_WINDOW_LOW_BLK                                         0
#define GCREG_VR_TARGET_WINDOW_LOW_Count                                       1
#define GCREG_VR_TARGET_WINDOW_LOW_FieldMask                          0xFFFFFFFF
#define GCREG_VR_TARGET_WINDOW_LOW_ReadMask                           0xFFFFFFFF
#define GCREG_VR_TARGET_WINDOW_LOW_WriteMask                          0xFFFFFFFF
#define GCREG_VR_TARGET_WINDOW_LOW_ResetValue                         0x00000000

#define GCREG_VR_TARGET_WINDOW_LOW_LEFT                                   15 : 0
#define GCREG_VR_TARGET_WINDOW_LOW_LEFT_End                                   15
#define GCREG_VR_TARGET_WINDOW_LOW_LEFT_Start                                  0
#define GCREG_VR_TARGET_WINDOW_LOW_LEFT_Type                                 U16

#define GCREG_VR_TARGET_WINDOW_LOW_TOP                                   31 : 16
#define GCREG_VR_TARGET_WINDOW_LOW_TOP_End                                    31
#define GCREG_VR_TARGET_WINDOW_LOW_TOP_Start                                  16
#define GCREG_VR_TARGET_WINDOW_LOW_TOP_Type                                  U16

/*******************************************************************************
** State gcregVRTargetWindowHigh
*/

#define gcregVRTargetWindowHighRegAddrs                                   0x04AB
#define GCREG_VR_TARGET_WINDOW_HIGH_MSB                                       15
#define GCREG_VR_TARGET_WINDOW_HIGH_LSB                                        0
#define GCREG_VR_TARGET_WINDOW_LOW_HIGH_BLK                                    0
#define GCREG_VR_TARGET_WINDOW_HIGH_Count                                      1
#define GCREG_VR_TARGET_WINDOW_HIGH_FieldMask                         0xFFFFFFFF
#define GCREG_VR_TARGET_WINDOW_HIGH_ReadMask                          0xFFFFFFFF
#define GCREG_VR_TARGET_WINDOW_HIGH_WriteMask                         0xFFFFFFFF
#define GCREG_VR_TARGET_WINDOW_HIGH_ResetValue                        0x00000000

#define GCREG_VR_TARGET_WINDOW_HIGH_RIGHT                                 15 : 0
#define GCREG_VR_TARGET_WINDOW_HIGH_RIGHT_End                                 15
#define GCREG_VR_TARGET_WINDOW_HIGH_RIGHT_Start                                0
#define GCREG_VR_TARGET_WINDOW_HIGH_RIGHT_Type                               U16

#define GCREG_VR_TARGET_WINDOW_HIGH_BOTTOM                               31 : 16
#define GCREG_VR_TARGET_WINDOW_HIGH_BOTTOM_End                                31
#define GCREG_VR_TARGET_WINDOW_HIGH_BOTTOM_Start                              16
#define GCREG_VR_TARGET_WINDOW_HIGH_BOTTOM_Type                              U16

/*******************************************************************************
** State gcregVRConfig
*/

/* Video Rasterizer kick-off register. */

#define gcregVRConfigRegAddrs                                             0x04A5
#define GCREG_VR_CONFIG_MSB                                                   15
#define GCREG_VR_CONFIG_LSB                                                    0
#define GCREG_VR_CONFIG_BLK                                                    0
#define GCREG_VR_CONFIG_Count                                                  1
#define GCREG_VR_CONFIG_FieldMask                                     0x0000000B
#define GCREG_VR_CONFIG_ReadMask                                      0x0000000B
#define GCREG_VR_CONFIG_WriteMask                                     0x0000000B
#define GCREG_VR_CONFIG_ResetValue                                    0x00000000

/* Kick-off command. */
#define GCREG_VR_CONFIG_START                                              1 : 0
#define GCREG_VR_CONFIG_START_End                                              1
#define GCREG_VR_CONFIG_START_Start                                            0
#define GCREG_VR_CONFIG_START_Type                                           U02
#define   GCREG_VR_CONFIG_START_HORIZONTAL_BLIT                              0x0
#define   GCREG_VR_CONFIG_START_VERTICAL_BLIT                                0x1
#define   GCREG_VR_CONFIG_START_ONE_PASS_BLIT                                0x2

#define GCREG_VR_CONFIG_MASK_START                                         3 : 3
#define GCREG_VR_CONFIG_MASK_START_End                                         3
#define GCREG_VR_CONFIG_MASK_START_Start                                       3
#define GCREG_VR_CONFIG_MASK_START_Type                                      U01
#define   GCREG_VR_CONFIG_MASK_START_ENABLED                                 0x0
#define   GCREG_VR_CONFIG_MASK_START_MASKED                                  0x1

/*******************************************************************************
** State gcregVRConfigEx
*/

/* Video Rasterizer configuration register. */

#define gcregVRConfigExRegAddrs                                           0x04B9
#define GCREG_VR_CONFIG_EX_Address                                       0x012E4
#define GCREG_VR_CONFIG_EX_MSB                                                15
#define GCREG_VR_CONFIG_EX_LSB                                                 0
#define GCREG_VR_CONFIG_EX_BLK                                                 0
#define GCREG_VR_CONFIG_EX_Count                                               1
#define GCREG_VR_CONFIG_EX_FieldMask                                  0x000001FB
#define GCREG_VR_CONFIG_EX_ReadMask                                   0x000001FB
#define GCREG_VR_CONFIG_EX_WriteMask                                  0x000001FB
#define GCREG_VR_CONFIG_EX_ResetValue                                 0x00000000

/* Line width in pixels for vertical pass. */
#define GCREG_VR_CONFIG_EX_VERTICAL_LINE_WIDTH                             1 : 0
#define GCREG_VR_CONFIG_EX_VERTICAL_LINE_WIDTH_End                             1
#define GCREG_VR_CONFIG_EX_VERTICAL_LINE_WIDTH_Start                           0
#define GCREG_VR_CONFIG_EX_VERTICAL_LINE_WIDTH_Type                          U02
#define   GCREG_VR_CONFIG_EX_VERTICAL_LINE_WIDTH_AUTO                        0x0
#define   GCREG_VR_CONFIG_EX_VERTICAL_LINE_WIDTH_PIXELS16                    0x1
#define   GCREG_VR_CONFIG_EX_VERTICAL_LINE_WIDTH_PIXELS32                    0x2

#define GCREG_VR_CONFIG_EX_MASK_VERTICAL_LINE_WIDTH                        3 : 3
#define GCREG_VR_CONFIG_EX_MASK_VERTICAL_LINE_WIDTH_End                        3
#define GCREG_VR_CONFIG_EX_MASK_VERTICAL_LINE_WIDTH_Start                      3
#define GCREG_VR_CONFIG_EX_MASK_VERTICAL_LINE_WIDTH_Type                     U01
#define   GCREG_VR_CONFIG_EX_MASK_VERTICAL_LINE_WIDTH_ENABLED                0x0
#define   GCREG_VR_CONFIG_EX_MASK_VERTICAL_LINE_WIDTH_MASKED                 0x1

/* one pass filter tap. */
#define GCREG_VR_CONFIG_EX_FILTER_TAP                                      7 : 4
#define GCREG_VR_CONFIG_EX_FILTER_TAP_End                                      7
#define GCREG_VR_CONFIG_EX_FILTER_TAP_Start                                    4
#define GCREG_VR_CONFIG_EX_FILTER_TAP_Type                                   U04

#define GCREG_VR_CONFIG_EX_MASK_FILTER_TAP                                 8 : 8
#define GCREG_VR_CONFIG_EX_MASK_FILTER_TAP_End                                 8
#define GCREG_VR_CONFIG_EX_MASK_FILTER_TAP_Start                               8
#define GCREG_VR_CONFIG_EX_MASK_FILTER_TAP_Type                              U01
#define   GCREG_VR_CONFIG_EX_MASK_FILTER_TAP_ENABLED                         0x0
#define   GCREG_VR_CONFIG_EX_MASK_FILTER_TAP_MASKED                          0x1

/*******************************************************************************
** State gcregBWConfig
*/

#define gcregBWConfigRegAddrs                                             0x04BC
#define GCREG_BW_CONFIG_MSB                                                   15
#define GCREG_BW_CONFIG_LSB                                                    0
#define GCREG_BW_CONFIG_BLK                                                    0
#define GCREG_BW_CONFIG_Count                                                  1
#define GCREG_BW_CONFIG_FieldMask                                     0x00009999
#define GCREG_BW_CONFIG_ReadMask                                      0x00009999
#define GCREG_BW_CONFIG_WriteMask                                     0x00009999
#define GCREG_BW_CONFIG_ResetValue                                    0x00000000

/* One Pass Filter Block Config. */
#define GCREG_BW_CONFIG_BLOCK_CONFIG                                       0 : 0
#define GCREG_BW_CONFIG_BLOCK_CONFIG_End                                       0
#define GCREG_BW_CONFIG_BLOCK_CONFIG_Start                                     0
#define GCREG_BW_CONFIG_BLOCK_CONFIG_Type                                    U01
#define   GCREG_BW_CONFIG_BLOCK_CONFIG_AUTO                                  0x0
#define   GCREG_BW_CONFIG_BLOCK_CONFIG_CUSTOMIZE                             0x1

#define GCREG_BW_CONFIG_MASK_BLOCK_CONFIG                                  3 : 3
#define GCREG_BW_CONFIG_MASK_BLOCK_CONFIG_End                                  3
#define GCREG_BW_CONFIG_MASK_BLOCK_CONFIG_Start                                3
#define GCREG_BW_CONFIG_MASK_BLOCK_CONFIG_Type                               U01
#define   GCREG_BW_CONFIG_MASK_BLOCK_CONFIG_ENABLED                          0x0
#define   GCREG_BW_CONFIG_MASK_BLOCK_CONFIG_MASKED                           0x1

/* block walk direction in one pass filter blit. */
#define GCREG_BW_CONFIG_BLOCK_WALK_DIRECTION                               4 : 4
#define GCREG_BW_CONFIG_BLOCK_WALK_DIRECTION_End                               4
#define GCREG_BW_CONFIG_BLOCK_WALK_DIRECTION_Start                             4
#define GCREG_BW_CONFIG_BLOCK_WALK_DIRECTION_Type                            U01
#define   GCREG_BW_CONFIG_BLOCK_WALK_DIRECTION_RIGHT_BOTTOM                  0x0
#define   GCREG_BW_CONFIG_BLOCK_WALK_DIRECTION_BOTTOM_RIGHT                  0x1

#define GCREG_BW_CONFIG_MASK_BLOCK_WALK_DIRECTION                          7 : 7
#define GCREG_BW_CONFIG_MASK_BLOCK_WALK_DIRECTION_End                          7
#define GCREG_BW_CONFIG_MASK_BLOCK_WALK_DIRECTION_Start                        7
#define GCREG_BW_CONFIG_MASK_BLOCK_WALK_DIRECTION_Type                       U01
#define   GCREG_BW_CONFIG_MASK_BLOCK_WALK_DIRECTION_ENABLED                  0x0
#define   GCREG_BW_CONFIG_MASK_BLOCK_WALK_DIRECTION_MASKED                   0x1

/* block walk direction in one pass filter blit. */
#define GCREG_BW_CONFIG_TILE_WALK_DIRECTION                                8 : 8
#define GCREG_BW_CONFIG_TILE_WALK_DIRECTION_End                                8
#define GCREG_BW_CONFIG_TILE_WALK_DIRECTION_Start                              8
#define GCREG_BW_CONFIG_TILE_WALK_DIRECTION_Type                             U01
#define   GCREG_BW_CONFIG_TILE_WALK_DIRECTION_RIGHT_BOTTOM                   0x0
#define   GCREG_BW_CONFIG_TILE_WALK_DIRECTION_BOTTOM_RIGHT                   0x1

#define GCREG_BW_CONFIG_MASK_TILE_WALK_DIRECTION                         11 : 11
#define GCREG_BW_CONFIG_MASK_TILE_WALK_DIRECTION_End                          11
#define GCREG_BW_CONFIG_MASK_TILE_WALK_DIRECTION_Start                        11
#define GCREG_BW_CONFIG_MASK_TILE_WALK_DIRECTION_Type                        U01
#define   GCREG_BW_CONFIG_MASK_TILE_WALK_DIRECTION_ENABLED                   0x0
#define   GCREG_BW_CONFIG_MASK_TILE_WALK_DIRECTION_MASKED                    0x1

/* block walk direction in one pass filter blit. */
#define GCREG_BW_CONFIG_PIXEL_WALK_DIRECTION                             12 : 12
#define GCREG_BW_CONFIG_PIXEL_WALK_DIRECTION_End                              12
#define GCREG_BW_CONFIG_PIXEL_WALK_DIRECTION_Start                            12
#define GCREG_BW_CONFIG_PIXEL_WALK_DIRECTION_Type                            U01
#define   GCREG_BW_CONFIG_PIXEL_WALK_DIRECTION_RIGHT_BOTTOM                  0x0
#define   GCREG_BW_CONFIG_PIXEL_WALK_DIRECTION_BOTTOM_RIGHT                  0x1

#define GCREG_BW_CONFIG_MASK_PIXEL_WALK_DIRECTION                        15 : 15
#define GCREG_BW_CONFIG_MASK_PIXEL_WALK_DIRECTION_End                         15
#define GCREG_BW_CONFIG_MASK_PIXEL_WALK_DIRECTION_Start                       15
#define GCREG_BW_CONFIG_MASK_PIXEL_WALK_DIRECTION_Type                       U01
#define   GCREG_BW_CONFIG_MASK_PIXEL_WALK_DIRECTION_ENABLED                  0x0
#define   GCREG_BW_CONFIG_MASK_PIXEL_WALK_DIRECTION_MASKED                   0x1

/*******************************************************************************
** State gcregBWBlockSize
*/

/* Walker Block size. */

#define gcregBWBlockSizeRegAddrs                                          0x04BD
#define GCREG_BW_BLOCK_SIZE_MSB                                               15
#define GCREG_BW_BLOCK_SIZE_LSB                                                0
#define GCREG_BW_BLOCK_SIZE_BLK                                                0
#define GCREG_BW_BLOCK_SIZE_Count                                              1
#define GCREG_BW_BLOCK_SIZE_FieldMask                                 0xFFFFFFFF
#define GCREG_BW_BLOCK_SIZE_ReadMask                                  0xFFFFFFFF
#define GCREG_BW_BLOCK_SIZE_WriteMask                                 0xFFFFFFFF
#define GCREG_BW_BLOCK_SIZE_ResetValue                                0x00000000

#define GCREG_BW_BLOCK_SIZE_WIDTH                                         15 : 0
#define GCREG_BW_BLOCK_SIZE_WIDTH_End                                         15
#define GCREG_BW_BLOCK_SIZE_WIDTH_Start                                        0
#define GCREG_BW_BLOCK_SIZE_WIDTH_Type                                       U16

#define GCREG_BW_BLOCK_SIZE_HEIGHT                                       31 : 16
#define GCREG_BW_BLOCK_SIZE_HEIGHT_End                                        31
#define GCREG_BW_BLOCK_SIZE_HEIGHT_Start                                      16
#define GCREG_BW_BLOCK_SIZE_HEIGHT_Type                                      U16

/*******************************************************************************
** State gcregBWTileSize
*/

/* Walker tile size. */

#define gcregBWTileSizeRegAddrs                                           0x04BE
#define GCREG_BW_TILE_SIZE_MSB                                                15
#define GCREG_BW_TILE_SIZE_LSB                                                 0
#define GCREG_BW_TILE_SIZE_BLK                                                 0
#define GCREG_BW_TILE_SIZE_Count                                               1
#define GCREG_BW_TILE_SIZE_FieldMask                                  0xFFFFFFFF
#define GCREG_BW_TILE_SIZE_ReadMask                                   0xFFFFFFFF
#define GCREG_BW_TILE_SIZE_WriteMask                                  0xFFFFFFFF
#define GCREG_BW_TILE_SIZE_ResetValue                                 0x00000000

#define GCREG_BW_TILE_SIZE_WIDTH                                          15 : 0
#define GCREG_BW_TILE_SIZE_WIDTH_End                                          15
#define GCREG_BW_TILE_SIZE_WIDTH_Start                                         0
#define GCREG_BW_TILE_SIZE_WIDTH_Type                                        U16

#define GCREG_BW_TILE_SIZE_HEIGHT                                        31 : 16
#define GCREG_BW_TILE_SIZE_HEIGHT_End                                         31
#define GCREG_BW_TILE_SIZE_HEIGHT_Start                                       16
#define GCREG_BW_TILE_SIZE_HEIGHT_Type                                       U16

/*******************************************************************************
** State gcregBWBlockMask
*/

/* Walker Block Mask. */

#define gcregBWBlockMaskRegAddrs                                          0x04BF
#define GCREG_BW_BLOCK_MASK_MSB                                               15
#define GCREG_BW_BLOCK_MASK_LSB                                                0
#define GCREG_BW_BLOCK_MASK_BLK                                                0
#define GCREG_BW_BLOCK_MASK_Count                                              1
#define GCREG_BW_BLOCK_MASK_FieldMask                                 0xFFFFFFFF
#define GCREG_BW_BLOCK_MASK_ReadMask                                  0xFFFFFFFF
#define GCREG_BW_BLOCK_MASK_WriteMask                                 0xFFFFFFFF
#define GCREG_BW_BLOCK_MASK_ResetValue                                0x00000000

#define GCREG_BW_BLOCK_MASK_HORIZONTAL                                    15 : 0
#define GCREG_BW_BLOCK_MASK_HORIZONTAL_End                                    15
#define GCREG_BW_BLOCK_MASK_HORIZONTAL_Start                                   0
#define GCREG_BW_BLOCK_MASK_HORIZONTAL_Type                                  U16

#define GCREG_BW_BLOCK_MASK_VERTICAL                                     31 : 16
#define GCREG_BW_BLOCK_MASK_VERTICAL_End                                      31
#define GCREG_BW_BLOCK_MASK_VERTICAL_Start                                    16
#define GCREG_BW_BLOCK_MASK_VERTICAL_Type                                    U16

/*******************************************************************************
** State gcregIndexColorTable
*/

/* 256 color entries for the indexed color mode. Colors are assumed to be in
** the destination format and no color conversion is done on the values.
*/

#define gcregIndexColorTableRegAddrs                                      0x0700
#define GCREG_INDEX_COLOR_TABLE_MSB                                           15
#define GCREG_INDEX_COLOR_TABLE_LSB                                            8
#define GCREG_INDEX_COLOR_TABLE_BLK                                            8
#define GCREG_INDEX_COLOR_TABLE_Count                                        256
#define GCREG_INDEX_COLOR_TABLE_FieldMask                             0xFFFFFFFF
#define GCREG_INDEX_COLOR_TABLE_ReadMask                              0xFFFFFFFF
#define GCREG_INDEX_COLOR_TABLE_WriteMask                             0xFFFFFFFF
#define GCREG_INDEX_COLOR_TABLE_ResetValue                            0x00000000

#define GCREG_INDEX_COLOR_TABLE_ALPHA                                    31 : 24
#define GCREG_INDEX_COLOR_TABLE_ALPHA_End                                     31
#define GCREG_INDEX_COLOR_TABLE_ALPHA_Start                                   24
#define GCREG_INDEX_COLOR_TABLE_ALPHA_Type                                   U08

#define GCREG_INDEX_COLOR_TABLE_RED                                      23 : 16
#define GCREG_INDEX_COLOR_TABLE_RED_End                                       23
#define GCREG_INDEX_COLOR_TABLE_RED_Start                                     16
#define GCREG_INDEX_COLOR_TABLE_RED_Type                                     U08

#define GCREG_INDEX_COLOR_TABLE_GREEN                                     15 : 8
#define GCREG_INDEX_COLOR_TABLE_GREEN_End                                     15
#define GCREG_INDEX_COLOR_TABLE_GREEN_Start                                    8
#define GCREG_INDEX_COLOR_TABLE_GREEN_Type                                   U08

#define GCREG_INDEX_COLOR_TABLE_BLUE                                       7 : 0
#define GCREG_INDEX_COLOR_TABLE_BLUE_End                                       7
#define GCREG_INDEX_COLOR_TABLE_BLUE_Start                                     0
#define GCREG_INDEX_COLOR_TABLE_BLUE_Type                                    U08

/*******************************************************************************
** State gcregIndexColorTable32
*/

/* 256 color entries for the indexed color mode. Colors are assumed to be in
** the A8R8G8B8 format and no color conversion is done on the values. This
** register is used only with chips with PE20 feature available.
*/

#define gcregIndexColorTable32RegAddrs                                    0x0D00
#define GCREG_INDEX_COLOR_TABLE32_MSB                                         15
#define GCREG_INDEX_COLOR_TABLE32_LSB                                          8
#define GCREG_INDEX_COLOR_TABLE32_BLK                                          8
#define GCREG_INDEX_COLOR_TABLE32_Count                                      256
#define GCREG_INDEX_COLOR_TABLE32_FieldMask                           0xFFFFFFFF
#define GCREG_INDEX_COLOR_TABLE32_ReadMask                            0xFFFFFFFF
#define GCREG_INDEX_COLOR_TABLE32_WriteMask                           0xFFFFFFFF
#define GCREG_INDEX_COLOR_TABLE32_ResetValue                          0x00000000

#define GCREG_INDEX_COLOR_TABLE32_ALPHA                                  31 : 24
#define GCREG_INDEX_COLOR_TABLE32_ALPHA_End                                   31
#define GCREG_INDEX_COLOR_TABLE32_ALPHA_Start                                 24
#define GCREG_INDEX_COLOR_TABLE32_ALPHA_Type                                 U08

#define GCREG_INDEX_COLOR_TABLE32_RED                                    23 : 16
#define GCREG_INDEX_COLOR_TABLE32_RED_End                                     23
#define GCREG_INDEX_COLOR_TABLE32_RED_Start                                   16
#define GCREG_INDEX_COLOR_TABLE32_RED_Type                                   U08

#define GCREG_INDEX_COLOR_TABLE32_GREEN                                   15 : 8
#define GCREG_INDEX_COLOR_TABLE32_GREEN_End                                   15
#define GCREG_INDEX_COLOR_TABLE32_GREEN_Start                                  8
#define GCREG_INDEX_COLOR_TABLE32_GREEN_Type                                 U08

#define GCREG_INDEX_COLOR_TABLE32_BLUE                                     7 : 0
#define GCREG_INDEX_COLOR_TABLE32_BLUE_End                                     7
#define GCREG_INDEX_COLOR_TABLE32_BLUE_Start                                   0
#define GCREG_INDEX_COLOR_TABLE32_BLUE_Type                                  U08

/*******************************************************************************
** State gcregRop
*/

/* Raster operation foreground and background codes. Even though ROP is not
** used in CLEAR, HOR_FILTER_BLT, VER_FILTER_BLT and alpha-eanbled BIT_BLTs,
** ROP code still has to be programmed, because the engine makes the decision
** whether source, destination and pattern are involved in the current
** operation and the correct decision is essential for the engine to complete
** the operation as expected.
*/

#define gcregRopRegAddrs                                                  0x0497
#define GCREG_ROP_MSB                                                         15
#define GCREG_ROP_LSB                                                          0
#define GCREG_ROP_BLK                                                          0
#define GCREG_ROP_Count                                                        1
#define GCREG_ROP_FieldMask                                           0x0030FFFF
#define GCREG_ROP_ReadMask                                            0x0030FFFF
#define GCREG_ROP_WriteMask                                           0x0030FFFF
#define GCREG_ROP_ResetValue                                          0x00000000

/* ROP type: ROP2, ROP3 or ROP4 */
#define GCREG_ROP_TYPE                                                   21 : 20
#define GCREG_ROP_TYPE_End                                                    21
#define GCREG_ROP_TYPE_Start                                                  20
#define GCREG_ROP_TYPE_Type                                                  U02
#define   GCREG_ROP_TYPE_ROP2_PATTERN                                        0x0
#define   GCREG_ROP_TYPE_ROP2_SOURCE                                         0x1
#define   GCREG_ROP_TYPE_ROP3                                                0x2
#define   GCREG_ROP_TYPE_ROP4                                                0x3

/* Background ROP code is used for transparent pixels. */
#define GCREG_ROP_ROP_BG                                                  15 : 8
#define GCREG_ROP_ROP_BG_End                                                  15
#define GCREG_ROP_ROP_BG_Start                                                 8
#define GCREG_ROP_ROP_BG_Type                                                U08

/* Background ROP code is used for opaque pixels. */
#define GCREG_ROP_ROP_FG                                                   7 : 0
#define GCREG_ROP_ROP_FG_End                                                   7
#define GCREG_ROP_ROP_FG_Start                                                 0
#define GCREG_ROP_ROP_FG_Type                                                U08

struct gcregrop {
	unsigned int fg:8;
	unsigned int bg:8;
	unsigned int _reserved_16_19:4;
	unsigned int type:2;
	unsigned int _reserved_22_31:10;
};

/*******************************************************************************
** State gcregClipTopLeft
*/

/* Top left corner of the clipping rectangle defined in pixels. Clipping is
** always on and everything beyond the clipping rectangle will be clipped
** out. Clipping is not used with filter blits.
*/

#define gcregClipTopLeftRegAddrs                                          0x0498
#define GCREG_CLIP_TOP_LEFT_MSB                                               15
#define GCREG_CLIP_TOP_LEFT_LSB                                                0
#define GCREG_CLIP_TOP_LEFT_BLK                                                0
#define GCREG_CLIP_TOP_LEFT_Count                                              1
#define GCREG_CLIP_TOP_LEFT_FieldMask                                 0x7FFF7FFF
#define GCREG_CLIP_TOP_LEFT_ReadMask                                  0x7FFF7FFF
#define GCREG_CLIP_TOP_LEFT_WriteMask                                 0x7FFF7FFF
#define GCREG_CLIP_TOP_LEFT_ResetValue                                0x00000000

#define GCREG_CLIP_TOP_LEFT_Y                                            30 : 16
#define GCREG_CLIP_TOP_LEFT_Y_End                                             30
#define GCREG_CLIP_TOP_LEFT_Y_Start                                           16
#define GCREG_CLIP_TOP_LEFT_Y_Type                                           U15

#define GCREG_CLIP_TOP_LEFT_X                                             14 : 0
#define GCREG_CLIP_TOP_LEFT_X_End                                             14
#define GCREG_CLIP_TOP_LEFT_X_Start                                            0
#define GCREG_CLIP_TOP_LEFT_X_Type                                           U15

struct gcregcliplt {
	unsigned int left:15;
	unsigned int _reserved_15:1;
	unsigned int top:15;
	unsigned int _reserved_31:1;
};

/*******************************************************************************
** State gcregClipBottomRight
*/

/* Bottom right corner of the clipping rectangle defined in pixels. Clipping
** is always on and everything beyond the clipping rectangle will be clipped
** out. Clipping is not used with filter blits.
*/

#define gcregClipBottomRightRegAddrs                                      0x0499
#define GCREG_CLIP_BOTTOM_RIGHT_MSB                                           15
#define GCREG_CLIP_BOTTOM_RIGHT_LSB                                            0
#define GCREG_CLIP_BOTTOM_RIGHT_BLK                                            0
#define GCREG_CLIP_BOTTOM_RIGHT_Count                                          1
#define GCREG_CLIP_BOTTOM_RIGHT_FieldMask                             0x7FFF7FFF
#define GCREG_CLIP_BOTTOM_RIGHT_ReadMask                              0x7FFF7FFF
#define GCREG_CLIP_BOTTOM_RIGHT_WriteMask                             0x7FFF7FFF
#define GCREG_CLIP_BOTTOM_RIGHT_ResetValue                            0x00000000

#define GCREG_CLIP_BOTTOM_RIGHT_Y                                        30 : 16
#define GCREG_CLIP_BOTTOM_RIGHT_Y_End                                         30
#define GCREG_CLIP_BOTTOM_RIGHT_Y_Start                                       16
#define GCREG_CLIP_BOTTOM_RIGHT_Y_Type                                       U15

#define GCREG_CLIP_BOTTOM_RIGHT_X                                         14 : 0
#define GCREG_CLIP_BOTTOM_RIGHT_X_End                                         14
#define GCREG_CLIP_BOTTOM_RIGHT_X_Start                                        0
#define GCREG_CLIP_BOTTOM_RIGHT_X_Type                                       U15

struct gcregcliprb {
	unsigned int right:15;
	unsigned int _reserved_15:1;
	unsigned int bottom:15;
	unsigned int _reserved_31:1;
};

/*******************************************************************************
** State gcregConfig
*/

#define gcregConfigRegAddrs                                               0x049B
#define GCREG_CONFIG_MSB                                                      15
#define GCREG_CONFIG_LSB                                                       0
#define GCREG_CONFIG_BLK                                                       0
#define GCREG_CONFIG_Count                                                     1
#define GCREG_CONFIG_FieldMask                                        0x00370031
#define GCREG_CONFIG_ReadMask                                         0x00370031
#define GCREG_CONFIG_WriteMask                                        0x00370031
#define GCREG_CONFIG_ResetValue                                       0x00000000

#define GCREG_CONFIG_MIRROR_BLT_MODE                                       5 : 4
#define GCREG_CONFIG_MIRROR_BLT_MODE_End                                       5
#define GCREG_CONFIG_MIRROR_BLT_MODE_Start                                     4
#define GCREG_CONFIG_MIRROR_BLT_MODE_Type                                    U02
#define   GCREG_CONFIG_MIRROR_BLT_MODE_NORMAL                                0x0
#define   GCREG_CONFIG_MIRROR_BLT_MODE_HMIRROR                               0x1
#define   GCREG_CONFIG_MIRROR_BLT_MODE_VMIRROR                               0x2
#define   GCREG_CONFIG_MIRROR_BLT_MODE_FULL_MIRROR                           0x3

#define GCREG_CONFIG_MIRROR_BLT_ENABLE                                     0 : 0
#define GCREG_CONFIG_MIRROR_BLT_ENABLE_End                                     0
#define GCREG_CONFIG_MIRROR_BLT_ENABLE_Start                                   0
#define GCREG_CONFIG_MIRROR_BLT_ENABLE_Type                                  U01
#define   GCREG_CONFIG_MIRROR_BLT_ENABLE_OFF                                 0x0
#define   GCREG_CONFIG_MIRROR_BLT_ENABLE_ON                                  0x1

/* Source select for the old walkers. */
#define GCREG_CONFIG_SOURCE_SELECT                                       18 : 16
#define GCREG_CONFIG_SOURCE_SELECT_End                                        18
#define GCREG_CONFIG_SOURCE_SELECT_Start                                      16
#define GCREG_CONFIG_SOURCE_SELECT_Type                                      U03

/* Destination select for the old walkers. */
#define GCREG_CONFIG_DESTINATION_SELECT                                  21 : 20
#define GCREG_CONFIG_DESTINATION_SELECT_End                                   21
#define GCREG_CONFIG_DESTINATION_SELECT_Start                                 20
#define GCREG_CONFIG_DESTINATION_SELECT_Type                                 U02

/*******************************************************************************
** State gcregSrcOriginFraction
*/

/* Fraction for the source origin. Together with values in gcregSrcOrigin
** these values form signed 16.16 fixed point origin for the source
** rectangle. Fractions are only used in filter blit in split frame mode.
*/

#define gcregSrcOriginFractionRegAddrs                                    0x049E
#define GCREG_SRC_ORIGIN_FRACTION_MSB                                         15
#define GCREG_SRC_ORIGIN_FRACTION_LSB                                          0
#define GCREG_SRC_ORIGIN_FRACTION_BLK                                          0
#define GCREG_SRC_ORIGIN_FRACTION_Count                                        1
#define GCREG_SRC_ORIGIN_FRACTION_FieldMask                           0xFFFFFFFF
#define GCREG_SRC_ORIGIN_FRACTION_ReadMask                            0xFFFFFFFF
#define GCREG_SRC_ORIGIN_FRACTION_WriteMask                           0xFFFFFFFF
#define GCREG_SRC_ORIGIN_FRACTION_ResetValue                          0x00000000

#define GCREG_SRC_ORIGIN_FRACTION_Y                                      31 : 16
#define GCREG_SRC_ORIGIN_FRACTION_Y_End                                       31
#define GCREG_SRC_ORIGIN_FRACTION_Y_Start                                     16
#define GCREG_SRC_ORIGIN_FRACTION_Y_Type                                     U16

#define GCREG_SRC_ORIGIN_FRACTION_X                                       15 : 0
#define GCREG_SRC_ORIGIN_FRACTION_X_End                                       15
#define GCREG_SRC_ORIGIN_FRACTION_X_Start                                      0
#define GCREG_SRC_ORIGIN_FRACTION_X_Type                                     U16

/*******************************************************************************
** State gcregAlphaControl
*/

#define gcregAlphaControlRegAddrs                                         0x049F
#define GCREG_ALPHA_CONTROL_MSB                                               15
#define GCREG_ALPHA_CONTROL_LSB                                                0
#define GCREG_ALPHA_CONTROL_BLK                                                0
#define GCREG_ALPHA_CONTROL_Count                                              1
#define GCREG_ALPHA_CONTROL_FieldMask                                 0xFFFF0001
#define GCREG_ALPHA_CONTROL_ReadMask                                  0xFFFF0001
#define GCREG_ALPHA_CONTROL_WriteMask                                 0xFFFF0001
#define GCREG_ALPHA_CONTROL_ResetValue                                0x00000000

#define GCREG_ALPHA_CONTROL_ENABLE                                         0 : 0
#define GCREG_ALPHA_CONTROL_ENABLE_End                                         0
#define GCREG_ALPHA_CONTROL_ENABLE_Start                                       0
#define GCREG_ALPHA_CONTROL_ENABLE_Type                                      U01
#define   GCREG_ALPHA_CONTROL_ENABLE_OFF                                     0x0
#define   GCREG_ALPHA_CONTROL_ENABLE_ON                                      0x1

struct gcregalphacontrol {
	/* gcregAlphaControlRegAddrs:GCREG_ALPHA_CONTROL_ENABLE */
	unsigned int enable:1;

	/* gcregAlphaControlRegAddrs:reserved */
	unsigned int _reserved_1_31:31;
};

/*******************************************************************************
** State gcregAlphaModes
*/

#define gcregAlphaModesRegAddrs                                           0x04A0
#define GCREG_ALPHA_MODES_MSB                                                 15
#define GCREG_ALPHA_MODES_LSB                                                  0
#define GCREG_ALPHA_MODES_BLK                                                  0
#define GCREG_ALPHA_MODES_Count                                                1
#define GCREG_ALPHA_MODES_FieldMask                                   0xFF113311
#define GCREG_ALPHA_MODES_ReadMask                                    0xFF113311
#define GCREG_ALPHA_MODES_WriteMask                                   0xFF113311
#define GCREG_ALPHA_MODES_ResetValue                                  0x00000000

#define GCREG_ALPHA_MODES_SRC_ALPHA_MODE                                   0 : 0
#define GCREG_ALPHA_MODES_SRC_ALPHA_MODE_End                                   0
#define GCREG_ALPHA_MODES_SRC_ALPHA_MODE_Start                                 0
#define GCREG_ALPHA_MODES_SRC_ALPHA_MODE_Type                                U01
#define   GCREG_ALPHA_MODES_SRC_ALPHA_MODE_NORMAL                            0x0
#define   GCREG_ALPHA_MODES_SRC_ALPHA_MODE_INVERSED                          0x1

#define GCREG_ALPHA_MODES_DST_ALPHA_MODE                                   4 : 4
#define GCREG_ALPHA_MODES_DST_ALPHA_MODE_End                                   4
#define GCREG_ALPHA_MODES_DST_ALPHA_MODE_Start                                 4
#define GCREG_ALPHA_MODES_DST_ALPHA_MODE_Type                                U01
#define   GCREG_ALPHA_MODES_DST_ALPHA_MODE_NORMAL                            0x0
#define   GCREG_ALPHA_MODES_DST_ALPHA_MODE_INVERSED                          0x1

#define GCREG_ALPHA_MODES_GLOBAL_SRC_ALPHA_MODE                            9 : 8
#define GCREG_ALPHA_MODES_GLOBAL_SRC_ALPHA_MODE_End                            9
#define GCREG_ALPHA_MODES_GLOBAL_SRC_ALPHA_MODE_Start                          8
#define GCREG_ALPHA_MODES_GLOBAL_SRC_ALPHA_MODE_Type                         U02
#define   GCREG_ALPHA_MODES_GLOBAL_SRC_ALPHA_MODE_NORMAL                     0x0
#define   GCREG_ALPHA_MODES_GLOBAL_SRC_ALPHA_MODE_GLOBAL                     0x1
#define   GCREG_ALPHA_MODES_GLOBAL_SRC_ALPHA_MODE_SCALED                     0x2

#define GCREG_ALPHA_MODES_GLOBAL_DST_ALPHA_MODE                          13 : 12
#define GCREG_ALPHA_MODES_GLOBAL_DST_ALPHA_MODE_End                           13
#define GCREG_ALPHA_MODES_GLOBAL_DST_ALPHA_MODE_Start                         12
#define GCREG_ALPHA_MODES_GLOBAL_DST_ALPHA_MODE_Type                         U02
#define   GCREG_ALPHA_MODES_GLOBAL_DST_ALPHA_MODE_NORMAL                     0x0
#define   GCREG_ALPHA_MODES_GLOBAL_DST_ALPHA_MODE_GLOBAL                     0x1
#define   GCREG_ALPHA_MODES_GLOBAL_DST_ALPHA_MODE_SCALED                     0x2

#define GCREG_ALPHA_MODES_SRC_BLENDING_MODE                              26 : 24
#define GCREG_ALPHA_MODES_SRC_BLENDING_MODE_End                               26
#define GCREG_ALPHA_MODES_SRC_BLENDING_MODE_Start                             24
#define GCREG_ALPHA_MODES_SRC_BLENDING_MODE_Type                             U03
#define   GCREG_ALPHA_MODES_SRC_BLENDING_MODE_ZERO                           0x0
#define   GCREG_ALPHA_MODES_SRC_BLENDING_MODE_ONE                            0x1
#define   GCREG_ALPHA_MODES_SRC_BLENDING_MODE_NORMAL                         0x2
#define   GCREG_ALPHA_MODES_SRC_BLENDING_MODE_INVERSED                       0x3
#define   GCREG_ALPHA_MODES_SRC_BLENDING_MODE_COLOR                          0x4
#define   GCREG_ALPHA_MODES_SRC_BLENDING_MODE_COLOR_INVERSED                 0x5
#define   GCREG_ALPHA_MODES_SRC_BLENDING_MODE_SATURATED_ALPHA                0x6
#define   GCREG_ALPHA_MODES_SRC_BLENDING_MODE_SATURATED_DEST_ALPHA           0x7

/* Src Blending factor is calculate from Src alpha. */
#define GCREG_ALPHA_MODES_SRC_ALPHA_FACTOR                               27 : 27
#define GCREG_ALPHA_MODES_SRC_ALPHA_FACTOR_End                                27
#define GCREG_ALPHA_MODES_SRC_ALPHA_FACTOR_Start                              27
#define GCREG_ALPHA_MODES_SRC_ALPHA_FACTOR_Type                              U01
#define   GCREG_ALPHA_MODES_SRC_ALPHA_FACTOR_DISABLED                        0x0
#define   GCREG_ALPHA_MODES_SRC_ALPHA_FACTOR_ENABLED                         0x1

#define GCREG_ALPHA_MODES_DST_BLENDING_MODE                              30 : 28
#define GCREG_ALPHA_MODES_DST_BLENDING_MODE_End                               30
#define GCREG_ALPHA_MODES_DST_BLENDING_MODE_Start                             28
#define GCREG_ALPHA_MODES_DST_BLENDING_MODE_Type                             U03
#define   GCREG_ALPHA_MODES_DST_BLENDING_MODE_ZERO                           0x0
#define   GCREG_ALPHA_MODES_DST_BLENDING_MODE_ONE                            0x1
#define   GCREG_ALPHA_MODES_DST_BLENDING_MODE_NORMAL                         0x2
#define   GCREG_ALPHA_MODES_DST_BLENDING_MODE_INVERSED                       0x3
#define   GCREG_ALPHA_MODES_DST_BLENDING_MODE_COLOR                          0x4
#define   GCREG_ALPHA_MODES_DST_BLENDING_MODE_COLOR_INVERSED                 0x5
#define   GCREG_ALPHA_MODES_DST_BLENDING_MODE_SATURATED_ALPHA                0x6
#define   GCREG_ALPHA_MODES_DST_BLENDING_MODE_SATURATED_DEST_ALPHA           0x7

/* Dst Blending factor is calculate from Dst alpha. */
#define GCREG_ALPHA_MODES_DST_ALPHA_FACTOR                               31 : 31
#define GCREG_ALPHA_MODES_DST_ALPHA_FACTOR_End                                31
#define GCREG_ALPHA_MODES_DST_ALPHA_FACTOR_Start                              31
#define GCREG_ALPHA_MODES_DST_ALPHA_FACTOR_Type                              U01
#define   GCREG_ALPHA_MODES_DST_ALPHA_FACTOR_DISABLED                        0x0
#define   GCREG_ALPHA_MODES_DST_ALPHA_FACTOR_ENABLED                         0x1

struct gcregalphamodes {
	/* gcregAlphaModes:GCREG_ALPHA_MODES_SRC_ALPHA_MODE */
	unsigned int src_inverse:1;

	/* gcregAlphaModes:reserved */
	unsigned int _reserved_1_3:3;

	/* gcregAlphaModes:GCREG_ALPHA_MODES_DST_ALPHA_MODE */
	unsigned int dst_inverse:1;

	/* gcregAlphaModes:reserved */
	unsigned int _reserved_5_7:3;

	/* gcregAlphaModes:GCREG_ALPHA_MODES_GLOBAL_SRC_ALPHA_MODE */
	unsigned int src_global_alpha:2;

	/* gcregAlphaModes:reserved */
	unsigned int _reserved_10_11:2;

	/* gcregAlphaModes:GCREG_ALPHA_MODES_GLOBAL_DST_ALPHA_MODE */
	unsigned int dst_global_alpha:2;

	/* gcregAlphaModes:reserved */
	unsigned int _reserved_14_23:10;

	/* gcregAlphaModes:GCREG_ALPHA_MODES_SRC_BLENDING_MODE */
	unsigned int src_blend:3;

	/* gcregAlphaModes:GCREG_ALPHA_MODES_SRC_ALPHA_FACTOR */
	unsigned int src_color_reverse:1;

	/* gcregAlphaModes:GCREG_ALPHA_MODES_DST_BLENDING_MODE */
	unsigned int dst_blend:3;

	/* gcregAlphaModes:GCREG_ALPHA_MODES_DST_ALPHA_FACTOR */
	unsigned int dst_color_reverse:1;
};

/*******************************************************************************
** State UPlaneAddress
*/

/* 32-bit aligned base address of the source U plane. */

#define gcregUPlaneAddressRegAddrs                                        0x04A1
#define GCREG_UPLANE_ADDRESS_MSB                                              15
#define GCREG_UPLANE_ADDRESS_LSB                                               0
#define GCREG_UPLANE_ADDRESS_BLK                                               0
#define GCREG_UPLANE_ADDRESS_Count                                             1
#define GCREG_UPLANE_ADDRESS_FieldMask                                0xFFFFFFFF
#define GCREG_UPLANE_ADDRESS_ReadMask                                 0xFFFFFFFC
#define GCREG_UPLANE_ADDRESS_WriteMask                                0xFFFFFFFC
#define GCREG_UPLANE_ADDRESS_ResetValue                               0x00000000

#define GCREG_UPLANE_ADDRESS_ADDRESS                                      31 : 0
#define GCREG_UPLANE_ADDRESS_ADDRESS_End                                      30
#define GCREG_UPLANE_ADDRESS_ADDRESS_Start                                     0
#define GCREG_UPLANE_ADDRESS_ADDRESS_Type                                    U31

/*******************************************************************************
** State UPlaneStride
*/

/* Stride of the source U plane in bytes. */

#define gcregUPlaneStrideRegAddrs                                         0x04A2
#define GCREG_UPLANE_STRIDE_MSB                                               15
#define GCREG_UPLANE_STRIDE_LSB                                                0
#define GCREG_UPLANE_STRIDE_BLK                                                0
#define GCREG_UPLANE_STRIDE_Count                                              1
#define GCREG_UPLANE_STRIDE_FieldMask                                 0x0003FFFF
#define GCREG_UPLANE_STRIDE_ReadMask                                  0x0003FFFC
#define GCREG_UPLANE_STRIDE_WriteMask                                 0x0003FFFC
#define GCREG_UPLANE_STRIDE_ResetValue                                0x00000000

#define GCREG_UPLANE_STRIDE_STRIDE                                        17 : 0
#define GCREG_UPLANE_STRIDE_STRIDE_End                                        17
#define GCREG_UPLANE_STRIDE_STRIDE_Start                                       0
#define GCREG_UPLANE_STRIDE_STRIDE_Type                                      U18

/*******************************************************************************
** State VPlaneAddress
*/

/* 32-bit aligned base address of the source V plane. */

#define gcregVPlaneAddressRegAddrs                                        0x04A3
#define GCREG_VPLANE_ADDRESS_MSB                                              15
#define GCREG_VPLANE_ADDRESS_LSB                                               0
#define GCREG_VPLANE_ADDRESS_BLK                                               0
#define GCREG_VPLANE_ADDRESS_Count                                             1
#define GCREG_VPLANE_ADDRESS_FieldMask                                0xFFFFFFFF
#define GCREG_VPLANE_ADDRESS_ReadMask                                 0xFFFFFFFC
#define GCREG_VPLANE_ADDRESS_WriteMask                                0xFFFFFFFC
#define GCREG_VPLANE_ADDRESS_ResetValue                               0x00000000

#define GCREG_VPLANE_ADDRESS_ADDRESS                                      31 : 0
#define GCREG_VPLANE_ADDRESS_ADDRESS_End                                      30
#define GCREG_VPLANE_ADDRESS_ADDRESS_Start                                     0
#define GCREG_VPLANE_ADDRESS_ADDRESS_Type                                    U31

/*******************************************************************************
** State VPlaneStride
*/

/* Stride of the source V plane in bytes. */

#define gcregVPlaneStrideRegAddrs                                         0x04A4
#define GCREG_VPLANE_STRIDE_MSB                                               15
#define GCREG_VPLANE_STRIDE_LSB                                                0
#define GCREG_VPLANE_STRIDE_BLK                                                0
#define GCREG_VPLANE_STRIDE_Count                                              1
#define GCREG_VPLANE_STRIDE_FieldMask                                 0x0003FFFF
#define GCREG_VPLANE_STRIDE_ReadMask                                  0x0003FFFC
#define GCREG_VPLANE_STRIDE_WriteMask                                 0x0003FFFC
#define GCREG_VPLANE_STRIDE_ResetValue                                0x00000000

#define GCREG_VPLANE_STRIDE_STRIDE                                        17 : 0
#define GCREG_VPLANE_STRIDE_STRIDE_End                                        17
#define GCREG_VPLANE_STRIDE_STRIDE_Start                                       0
#define GCREG_VPLANE_STRIDE_STRIDE_Type                                      U18

/*******************************************************************************
** State gcregPEConfig
*/

/* PE debug register. */

#define gcregPEConfigRegAddrs                                             0x04AC
#define GCREG_PE_CONFIG_Address                                          0x012B0
#define GCREG_PE_CONFIG_MSB                                                   15
#define GCREG_PE_CONFIG_LSB                                                    0
#define GCREG_PE_CONFIG_BLK                                                    0
#define GCREG_PE_CONFIG_Count                                                  1
#define GCREG_PE_CONFIG_FieldMask                                     0x0000000B
#define GCREG_PE_CONFIG_ReadMask                                      0x0000000B
#define GCREG_PE_CONFIG_WriteMask                                     0x0000000B
#define GCREG_PE_CONFIG_ResetValue                                    0x00000000

#define GCREG_PE_CONFIG_DESTINATION_FETCH                                  1 : 0
#define GCREG_PE_CONFIG_DESTINATION_FETCH_End                                  1
#define GCREG_PE_CONFIG_DESTINATION_FETCH_Start                                0
#define GCREG_PE_CONFIG_DESTINATION_FETCH_Type                               U02
#define   GCREG_PE_CONFIG_DESTINATION_FETCH_DISABLE                          0x0
#define   GCREG_PE_CONFIG_DESTINATION_FETCH_DEFAULT                          0x1
#define   GCREG_PE_CONFIG_DESTINATION_FETCH_ALWAYS                           0x2

#define GCREG_PE_CONFIG_MASK_DESTINATION_FETCH                             3 : 3
#define GCREG_PE_CONFIG_MASK_DESTINATION_FETCH_End                             3
#define GCREG_PE_CONFIG_MASK_DESTINATION_FETCH_Start                           3
#define GCREG_PE_CONFIG_MASK_DESTINATION_FETCH_Type                          U01
#define   GCREG_PE_CONFIG_MASK_DESTINATION_FETCH_ENABLED                     0x0
#define   GCREG_PE_CONFIG_MASK_DESTINATION_FETCH_MASKED                      0x1

/*******************************************************************************
** State gcregDstRotationHeight
*/

/* 180/270 degree rotation configuration for the destination surface. Height
** field specifies the height of the surface in pixels.
*/

#define gcregDstRotationHeightRegAddrs                                    0x04AD
#define GCREG_DST_ROTATION_HEIGHT_MSB                                         15
#define GCREG_DST_ROTATION_HEIGHT_LSB                                          0
#define GCREG_DST_ROTATION_HEIGHT_BLK                                          0
#define GCREG_DST_ROTATION_HEIGHT_Count                                        1
#define GCREG_DST_ROTATION_HEIGHT_FieldMask                           0x0000FFFF
#define GCREG_DST_ROTATION_HEIGHT_ReadMask                            0x0000FFFF
#define GCREG_DST_ROTATION_HEIGHT_WriteMask                           0x0000FFFF
#define GCREG_DST_ROTATION_HEIGHT_ResetValue                          0x00000000

#define GCREG_DST_ROTATION_HEIGHT_HEIGHT                                  15 : 0
#define GCREG_DST_ROTATION_HEIGHT_HEIGHT_End                                  15
#define GCREG_DST_ROTATION_HEIGHT_HEIGHT_Start                                 0
#define GCREG_DST_ROTATION_HEIGHT_HEIGHT_Type                                U16

struct gcregdstrotationheight {
	/* gcregDstRotationHeightRegAddrs:GCREG_DST_ROTATION_HEIGHT_HEIGHT */
	unsigned int height:16;

	/* gcregDstRotationHeightRegAddrs:reserved */
	unsigned int _reserved_16_31:16;
};

/*******************************************************************************
** State gcregSrcRotationHeight
*/

/* 180/270 degree rotation configuration for the Source surface. Height field
** specifies the height of the surface in pixels.
*/

#define gcregSrcRotationHeightRegAddrs                                    0x04AE
#define GCREG_SRC_ROTATION_HEIGHT_MSB                                         15
#define GCREG_SRC_ROTATION_HEIGHT_LSB                                          0
#define GCREG_SRC_ROTATION_HEIGHT_BLK                                          0
#define GCREG_SRC_ROTATION_HEIGHT_Count                                        1
#define GCREG_SRC_ROTATION_HEIGHT_FieldMask                           0x0000FFFF
#define GCREG_SRC_ROTATION_HEIGHT_ReadMask                            0x0000FFFF
#define GCREG_SRC_ROTATION_HEIGHT_WriteMask                           0x0000FFFF
#define GCREG_SRC_ROTATION_HEIGHT_ResetValue                          0x00000000

#define GCREG_SRC_ROTATION_HEIGHT_HEIGHT                                  15 : 0
#define GCREG_SRC_ROTATION_HEIGHT_HEIGHT_End                                  15
#define GCREG_SRC_ROTATION_HEIGHT_HEIGHT_Start                                 0
#define GCREG_SRC_ROTATION_HEIGHT_HEIGHT_Type                                U16

struct gcregsrcrotationheight {
	/* gcregSrcRotationHeightRegAddrs:GCREG_SRC_ROTATION_HEIGHT_HEIGHT */
	unsigned int height:16;

	/* gcregSrcRotationHeightRegAddrs:reserved */
	unsigned int _reserved_16_31:16;
};

/*******************************************************************************
** State gcregRotAngle
*/

/* 0/90/180/270 degree rotation configuration for the Source surface. Height
** field specifies the height of the surface in pixels.
*/

#define gcregRotAngleRegAddrs                                             0x04AF
#define GCREG_ROT_ANGLE_MSB                                                   15
#define GCREG_ROT_ANGLE_LSB                                                    0
#define GCREG_ROT_ANGLE_BLK                                                    0
#define GCREG_ROT_ANGLE_Count                                                  1
#define GCREG_ROT_ANGLE_FieldMask                                     0x000BB33F
#define GCREG_ROT_ANGLE_ReadMask                                      0x000BB33F
#define GCREG_ROT_ANGLE_WriteMask                                     0x000BB33F
#define GCREG_ROT_ANGLE_ResetValue                                    0x00000000

#define GCREG_ROT_ANGLE_SRC                                                2 : 0
#define GCREG_ROT_ANGLE_SRC_End                                                2
#define GCREG_ROT_ANGLE_SRC_Start                                              0
#define GCREG_ROT_ANGLE_SRC_Type                                             U03
#define   GCREG_ROT_ANGLE_SRC_ROT0                                           0x0
#define   GCREG_ROT_ANGLE_SRC_FLIP_X                                         0x1
#define   GCREG_ROT_ANGLE_SRC_FLIP_Y                                         0x2
#define   GCREG_ROT_ANGLE_SRC_ROT90                                          0x4
#define   GCREG_ROT_ANGLE_SRC_ROT180                                         0x5
#define   GCREG_ROT_ANGLE_SRC_ROT270                                         0x6

#define GCREG_ROT_ANGLE_DST                                                5 : 3
#define GCREG_ROT_ANGLE_DST_End                                                5
#define GCREG_ROT_ANGLE_DST_Start                                              3
#define GCREG_ROT_ANGLE_DST_Type                                             U03
#define   GCREG_ROT_ANGLE_DST_ROT0                                           0x0
#define   GCREG_ROT_ANGLE_DST_FLIP_X                                         0x1
#define   GCREG_ROT_ANGLE_DST_FLIP_Y                                         0x2
#define   GCREG_ROT_ANGLE_DST_ROT90                                          0x4
#define   GCREG_ROT_ANGLE_DST_ROT180                                         0x5
#define   GCREG_ROT_ANGLE_DST_ROT270                                         0x6

#define GCREG_ROT_ANGLE_MASK_SRC                                           8 : 8
#define GCREG_ROT_ANGLE_MASK_SRC_End                                           8
#define GCREG_ROT_ANGLE_MASK_SRC_Start                                         8
#define GCREG_ROT_ANGLE_MASK_SRC_Type                                        U01
#define   GCREG_ROT_ANGLE_MASK_SRC_ENABLED                                   0x0
#define   GCREG_ROT_ANGLE_MASK_SRC_MASKED                                    0x1

#define GCREG_ROT_ANGLE_MASK_DST                                           9 : 9
#define GCREG_ROT_ANGLE_MASK_DST_End                                           9
#define GCREG_ROT_ANGLE_MASK_DST_Start                                         9
#define GCREG_ROT_ANGLE_MASK_DST_Type                                        U01
#define   GCREG_ROT_ANGLE_MASK_DST_ENABLED                                   0x0
#define   GCREG_ROT_ANGLE_MASK_DST_MASKED                                    0x1

#define GCREG_ROT_ANGLE_SRC_MIRROR                                       13 : 12
#define GCREG_ROT_ANGLE_SRC_MIRROR_End                                        13
#define GCREG_ROT_ANGLE_SRC_MIRROR_Start                                      12
#define GCREG_ROT_ANGLE_SRC_MIRROR_Type                                      U02
#define   GCREG_ROT_ANGLE_SRC_MIRROR_NONE                                    0x0
#define   GCREG_ROT_ANGLE_SRC_MIRROR_MIRROR_X                                0x1
#define   GCREG_ROT_ANGLE_SRC_MIRROR_MIRROR_Y                                0x2
#define   GCREG_ROT_ANGLE_SRC_MIRROR_MIRROR_XY                               0x3

#define GCREG_ROT_ANGLE_MASK_SRC_MIRROR                                  15 : 15
#define GCREG_ROT_ANGLE_MASK_SRC_MIRROR_End                                   15
#define GCREG_ROT_ANGLE_MASK_SRC_MIRROR_Start                                 15
#define GCREG_ROT_ANGLE_MASK_SRC_MIRROR_Type                                 U01
#define   GCREG_ROT_ANGLE_MASK_SRC_MIRROR_ENABLED                            0x0
#define   GCREG_ROT_ANGLE_MASK_SRC_MIRROR_MASKED                             0x1

#define GCREG_ROT_ANGLE_DST_MIRROR                                       17 : 16
#define GCREG_ROT_ANGLE_DST_MIRROR_End                                        17
#define GCREG_ROT_ANGLE_DST_MIRROR_Start                                      16
#define GCREG_ROT_ANGLE_DST_MIRROR_Type                                      U02
#define   GCREG_ROT_ANGLE_DST_MIRROR_NONE                                    0x0
#define   GCREG_ROT_ANGLE_DST_MIRROR_MIRROR_X                                0x1
#define   GCREG_ROT_ANGLE_DST_MIRROR_MIRROR_Y                                0x2
#define   GCREG_ROT_ANGLE_DST_MIRROR_MIRROR_XY                               0x3

#define GCREG_ROT_ANGLE_MASK_DST_MIRROR                                  19 : 19
#define GCREG_ROT_ANGLE_MASK_DST_MIRROR_End                                   19
#define GCREG_ROT_ANGLE_MASK_DST_MIRROR_Start                                 19
#define GCREG_ROT_ANGLE_MASK_DST_MIRROR_Type                                 U01
#define   GCREG_ROT_ANGLE_MASK_DST_MIRROR_ENABLED                            0x0
#define   GCREG_ROT_ANGLE_MASK_DST_MIRROR_MASKED                             0x1

struct gcregrotangle {
	/* gcregRotAngleRegAddrs:GCREG_ROT_ANGLE_SRC */
	unsigned int src:3;

	/* gcregRotAngleRegAddrs:GCREG_ROT_ANGLE_DST */
	unsigned int dst:3;

	/* gcregRotAngleRegAddrs:reserved */
	unsigned int _reserved_6_7:2;

	/* gcregRotAngleRegAddrs:GCREG_ROT_ANGLE_MASK_SRC */
	unsigned int src_mask:1;

	/* gcregRotAngleRegAddrs:GCREG_ROT_ANGLE_MASK_DST */
	unsigned int dst_mask:1;

	/* gcregRotAngleRegAddrs:reserved */
	unsigned int _reserved_10_11:2;

	/* gcregRotAngleRegAddrs:GCREG_ROT_ANGLE_SRC_MIRROR */
	unsigned int src_mirror:2;

	/* gcregRotAngleRegAddrs:reserved */
	unsigned int _reserved_14:1;

	/* gcregRotAngleRegAddrs:GCREG_ROT_ANGLE_MASK_SRC_MIRROR */
	unsigned int src_mirror_mask:1;

	/* gcregRotAngleRegAddrs:GCREG_ROT_ANGLE_DST_MIRROR */
	unsigned int dst_mirror:2;

	/* gcregRotAngleRegAddrs:reserved */
	unsigned int _reserved_18:1;

	/* gcregRotAngleRegAddrs:GCREG_ROT_ANGLE_MASK_DST_MIRROR */
	unsigned int dst_mirror_mask:1;

	/* gcregRotAngleRegAddrs:reserved */
	unsigned int _reserved_20_31:12;
};

/*******************************************************************************
** State gcregClearPixelValue32
*/

/* Clear color value in A8R8G8B8 format. */

#define gcregClearPixelValue32RegAddrs                                    0x04B0
#define GCREG_CLEAR_PIXEL_VALUE32_MSB                                         15
#define GCREG_CLEAR_PIXEL_VALUE32_LSB                                          0
#define GCREG_CLEAR_PIXEL_VALUE32_BLK                                          0
#define GCREG_CLEAR_PIXEL_VALUE32_Count                                        1
#define GCREG_CLEAR_PIXEL_VALUE32_FieldMask                           0xFFFFFFFF
#define GCREG_CLEAR_PIXEL_VALUE32_ReadMask                            0xFFFFFFFF
#define GCREG_CLEAR_PIXEL_VALUE32_WriteMask                           0xFFFFFFFF
#define GCREG_CLEAR_PIXEL_VALUE32_ResetValue                          0x00000000

#define GCREG_CLEAR_PIXEL_VALUE32_ALPHA                                  31 : 24
#define GCREG_CLEAR_PIXEL_VALUE32_ALPHA_End                                   31
#define GCREG_CLEAR_PIXEL_VALUE32_ALPHA_Start                                 24
#define GCREG_CLEAR_PIXEL_VALUE32_ALPHA_Type                                 U08

#define GCREG_CLEAR_PIXEL_VALUE32_RED                                    23 : 16
#define GCREG_CLEAR_PIXEL_VALUE32_RED_End                                     23
#define GCREG_CLEAR_PIXEL_VALUE32_RED_Start                                   16
#define GCREG_CLEAR_PIXEL_VALUE32_RED_Type                                   U08

#define GCREG_CLEAR_PIXEL_VALUE32_GREEN                                   15 : 8
#define GCREG_CLEAR_PIXEL_VALUE32_GREEN_End                                   15
#define GCREG_CLEAR_PIXEL_VALUE32_GREEN_Start                                  8
#define GCREG_CLEAR_PIXEL_VALUE32_GREEN_Type                                 U08

#define GCREG_CLEAR_PIXEL_VALUE32_BLUE                                     7 : 0
#define GCREG_CLEAR_PIXEL_VALUE32_BLUE_End                                     7
#define GCREG_CLEAR_PIXEL_VALUE32_BLUE_Start                                   0
#define GCREG_CLEAR_PIXEL_VALUE32_BLUE_Type                                  U08

struct gcregclearcolor {
	/* gcregClearPixelValue32RegAddrs:GCREG_CLEAR_PIXEL_VALUE32_BLUE */
	unsigned int b:8;

	/* gcregClearPixelValue32RegAddrs:GCREG_CLEAR_PIXEL_VALUE32_GREEN */
	unsigned int g:8;

	/* gcregClearPixelValue32RegAddrs:GCREG_CLEAR_PIXEL_VALUE32_RED */
	unsigned int r:8;

	/* gcregClearPixelValue32RegAddrs:GCREG_CLEAR_PIXEL_VALUE32_ALPHA */
	unsigned int a:8;
};

/*******************************************************************************
** State gcregDestColorKey
*/

/* Defines the destination transparency color in destination format. */

#define gcregDestColorKeyRegAddrs                                         0x04B1
#define GCREG_DEST_COLOR_KEY_MSB                                              15
#define GCREG_DEST_COLOR_KEY_LSB                                               0
#define GCREG_DEST_COLOR_KEY_BLK                                               0
#define GCREG_DEST_COLOR_KEY_Count                                             1
#define GCREG_DEST_COLOR_KEY_FieldMask                                0xFFFFFFFF
#define GCREG_DEST_COLOR_KEY_ReadMask                                 0xFFFFFFFF
#define GCREG_DEST_COLOR_KEY_WriteMask                                0xFFFFFFFF
#define GCREG_DEST_COLOR_KEY_ResetValue                               0x00000000

#define GCREG_DEST_COLOR_KEY_ALPHA                                       31 : 24
#define GCREG_DEST_COLOR_KEY_ALPHA_End                                        31
#define GCREG_DEST_COLOR_KEY_ALPHA_Start                                      24
#define GCREG_DEST_COLOR_KEY_ALPHA_Type                                      U08

#define GCREG_DEST_COLOR_KEY_RED                                         23 : 16
#define GCREG_DEST_COLOR_KEY_RED_End                                          23
#define GCREG_DEST_COLOR_KEY_RED_Start                                        16
#define GCREG_DEST_COLOR_KEY_RED_Type                                        U08

#define GCREG_DEST_COLOR_KEY_GREEN                                        15 : 8
#define GCREG_DEST_COLOR_KEY_GREEN_End                                        15
#define GCREG_DEST_COLOR_KEY_GREEN_Start                                       8
#define GCREG_DEST_COLOR_KEY_GREEN_Type                                      U08

#define GCREG_DEST_COLOR_KEY_BLUE                                          7 : 0
#define GCREG_DEST_COLOR_KEY_BLUE_End                                          7
#define GCREG_DEST_COLOR_KEY_BLUE_Start                                        0
#define GCREG_DEST_COLOR_KEY_BLUE_Type                                       U08

/*******************************************************************************
** State gcregGlobalSrcColor
*/

/* Defines the global source color and alpha values. */

#define gcregGlobalSrcColorRegAddrs                                       0x04B2
#define GCREG_GLOBAL_SRC_COLOR_MSB                                            15
#define GCREG_GLOBAL_SRC_COLOR_LSB                                             0
#define GCREG_GLOBAL_SRC_COLOR_BLK                                             0
#define GCREG_GLOBAL_SRC_COLOR_Count                                           1
#define GCREG_GLOBAL_SRC_COLOR_FieldMask                              0xFFFFFFFF
#define GCREG_GLOBAL_SRC_COLOR_ReadMask                               0xFFFFFFFF
#define GCREG_GLOBAL_SRC_COLOR_WriteMask                              0xFFFFFFFF
#define GCREG_GLOBAL_SRC_COLOR_ResetValue                             0x00000000

#define GCREG_GLOBAL_SRC_COLOR_ALPHA                                     31 : 24
#define GCREG_GLOBAL_SRC_COLOR_ALPHA_End                                      31
#define GCREG_GLOBAL_SRC_COLOR_ALPHA_Start                                    24
#define GCREG_GLOBAL_SRC_COLOR_ALPHA_Type                                    U08

#define GCREG_GLOBAL_SRC_COLOR_RED                                       23 : 16
#define GCREG_GLOBAL_SRC_COLOR_RED_End                                        23
#define GCREG_GLOBAL_SRC_COLOR_RED_Start                                      16
#define GCREG_GLOBAL_SRC_COLOR_RED_Type                                      U08

#define GCREG_GLOBAL_SRC_COLOR_GREEN                                      15 : 8
#define GCREG_GLOBAL_SRC_COLOR_GREEN_End                                      15
#define GCREG_GLOBAL_SRC_COLOR_GREEN_Start                                     8
#define GCREG_GLOBAL_SRC_COLOR_GREEN_Type                                    U08

#define GCREG_GLOBAL_SRC_COLOR_BLUE                                        7 : 0
#define GCREG_GLOBAL_SRC_COLOR_BLUE_End                                        7
#define GCREG_GLOBAL_SRC_COLOR_BLUE_Start                                      0
#define GCREG_GLOBAL_SRC_COLOR_BLUE_Type                                     U08

struct gcregglobalsrccolor {
	/* gcregGlobalSrcColorRegAddrs:GCREG_GLOBAL_SRC_COLOR_BLUE */
	unsigned int b:8;

	/* gcregGlobalSrcColorRegAddrs:GCREG_GLOBAL_SRC_COLOR_GREEN */
	unsigned int g:8;

	/* gcregGlobalSrcColorRegAddrs:GCREG_GLOBAL_SRC_COLOR_RED */
	unsigned int r:8;

	/* gcregGlobalSrcColorRegAddrs:GCREG_GLOBAL_SRC_COLOR_ALPHA */
	unsigned int a:8;
};

/*******************************************************************************
** State gcregGlobalDestColor
*/

/* Defines the global destination color and alpha values. */

#define gcregGlobalDestColorRegAddrs                                      0x04B3
#define GCREG_GLOBAL_DEST_COLOR_MSB                                           15
#define GCREG_GLOBAL_DEST_COLOR_LSB                                            0
#define GCREG_GLOBAL_DEST_COLOR_BLK                                            0
#define GCREG_GLOBAL_DEST_COLOR_Count                                          1
#define GCREG_GLOBAL_DEST_COLOR_FieldMask                             0xFFFFFFFF
#define GCREG_GLOBAL_DEST_COLOR_ReadMask                              0xFFFFFFFF
#define GCREG_GLOBAL_DEST_COLOR_WriteMask                             0xFFFFFFFF
#define GCREG_GLOBAL_DEST_COLOR_ResetValue                            0x00000000

#define GCREG_GLOBAL_DEST_COLOR_ALPHA                                    31 : 24
#define GCREG_GLOBAL_DEST_COLOR_ALPHA_End                                     31
#define GCREG_GLOBAL_DEST_COLOR_ALPHA_Start                                   24
#define GCREG_GLOBAL_DEST_COLOR_ALPHA_Type                                   U08

#define GCREG_GLOBAL_DEST_COLOR_RED                                      23 : 16
#define GCREG_GLOBAL_DEST_COLOR_RED_End                                       23
#define GCREG_GLOBAL_DEST_COLOR_RED_Start                                     16
#define GCREG_GLOBAL_DEST_COLOR_RED_Type                                     U08

#define GCREG_GLOBAL_DEST_COLOR_GREEN                                     15 : 8
#define GCREG_GLOBAL_DEST_COLOR_GREEN_End                                     15
#define GCREG_GLOBAL_DEST_COLOR_GREEN_Start                                    8
#define GCREG_GLOBAL_DEST_COLOR_GREEN_Type                                   U08

#define GCREG_GLOBAL_DEST_COLOR_BLUE                                       7 : 0
#define GCREG_GLOBAL_DEST_COLOR_BLUE_End                                       7
#define GCREG_GLOBAL_DEST_COLOR_BLUE_Start                                     0
#define GCREG_GLOBAL_DEST_COLOR_BLUE_Type                                    U08

struct gcregglobaldstcolor {
	/* gcregGlobalDestColorRegAddrs:GCREG_GLOBAL_DEST_COLOR_BLUE */
	unsigned int b:8;

	/* gcregGlobalDestColorRegAddrs:GCREG_GLOBAL_DEST_COLOR_GREEN */
	unsigned int g:8;

	/* gcregGlobalDestColorRegAddrs:GCREG_GLOBAL_DEST_COLOR_RED */
	unsigned int r:8;

	/* gcregGlobalDestColorRegAddrs:GCREG_GLOBAL_DEST_COLOR_ALPHA */
	unsigned int a:8;
};

/*******************************************************************************
** State gcregColorMultiplyModes
*/

/* Color modes to multiply Source or Destination pixel color by alpha
** channel. Alpha can be from global color source or current pixel.
*/

#define gcregColorMultiplyModesRegAddrs                                   0x04B4
#define GCREG_COLOR_MULTIPLY_MODES_MSB                                        15
#define GCREG_COLOR_MULTIPLY_MODES_LSB                                         0
#define GCREG_COLOR_MULTIPLY_MODES_BLK                                         0
#define GCREG_COLOR_MULTIPLY_MODES_Count                                       1
#define GCREG_COLOR_MULTIPLY_MODES_FieldMask                          0x00100311
#define GCREG_COLOR_MULTIPLY_MODES_ReadMask                           0x00100311
#define GCREG_COLOR_MULTIPLY_MODES_WriteMask                          0x00100311
#define GCREG_COLOR_MULTIPLY_MODES_ResetValue                         0x00000000

#define GCREG_COLOR_MULTIPLY_MODES_SRC_PREMULTIPLY                         0 : 0
#define GCREG_COLOR_MULTIPLY_MODES_SRC_PREMULTIPLY_End                         0
#define GCREG_COLOR_MULTIPLY_MODES_SRC_PREMULTIPLY_Start                       0
#define GCREG_COLOR_MULTIPLY_MODES_SRC_PREMULTIPLY_Type                      U01
#define   GCREG_COLOR_MULTIPLY_MODES_SRC_PREMULTIPLY_DISABLE                 0x0
#define   GCREG_COLOR_MULTIPLY_MODES_SRC_PREMULTIPLY_ENABLE                  0x1

#define GCREG_COLOR_MULTIPLY_MODES_DST_PREMULTIPLY                         4 : 4
#define GCREG_COLOR_MULTIPLY_MODES_DST_PREMULTIPLY_End                         4
#define GCREG_COLOR_MULTIPLY_MODES_DST_PREMULTIPLY_Start                       4
#define GCREG_COLOR_MULTIPLY_MODES_DST_PREMULTIPLY_Type                      U01
#define   GCREG_COLOR_MULTIPLY_MODES_DST_PREMULTIPLY_DISABLE                 0x0
#define   GCREG_COLOR_MULTIPLY_MODES_DST_PREMULTIPLY_ENABLE                  0x1

#define GCREG_COLOR_MULTIPLY_MODES_SRC_GLOBAL_PREMULTIPLY                  9 : 8
#define GCREG_COLOR_MULTIPLY_MODES_SRC_GLOBAL_PREMULTIPLY_End                  9
#define GCREG_COLOR_MULTIPLY_MODES_SRC_GLOBAL_PREMULTIPLY_Start                8
#define GCREG_COLOR_MULTIPLY_MODES_SRC_GLOBAL_PREMULTIPLY_Type               U02
#define   GCREG_COLOR_MULTIPLY_MODES_SRC_GLOBAL_PREMULTIPLY_DISABLE          0x0
#define   GCREG_COLOR_MULTIPLY_MODES_SRC_GLOBAL_PREMULTIPLY_ALPHA            0x1
#define   GCREG_COLOR_MULTIPLY_MODES_SRC_GLOBAL_PREMULTIPLY_COLOR            0x2

#define GCREG_COLOR_MULTIPLY_MODES_DST_DEMULTIPLY                        20 : 20
#define GCREG_COLOR_MULTIPLY_MODES_DST_DEMULTIPLY_End                         20
#define GCREG_COLOR_MULTIPLY_MODES_DST_DEMULTIPLY_Start                       20
#define GCREG_COLOR_MULTIPLY_MODES_DST_DEMULTIPLY_Type                       U01
#define   GCREG_COLOR_MULTIPLY_MODES_DST_DEMULTIPLY_DISABLE                  0x0
#define   GCREG_COLOR_MULTIPLY_MODES_DST_DEMULTIPLY_ENABLE                   0x1

struct gcregcolormultiplymodes {
	/* gcregColorMultiplyModesRegAddrs:
		GCREG_COLOR_MULTIPLY_MODES_SRC_PREMULTIPLY */
	unsigned int srcpremul:1;

	/* gcregColorMultiplyModesRegAddrs:
		reserved */
	unsigned int _reserved_1_3:3;

	/* gcregColorMultiplyModesRegAddrs:
		GCREG_COLOR_MULTIPLY_MODES_DST_PREMULTIPLY */
	unsigned int dstpremul:1;

	/* gcregColorMultiplyModesRegAddrs:
		reserved */
	unsigned int _reserved_5_7:3;

	/* gcregColorMultiplyModesRegAddrs:
		GCREG_COLOR_MULTIPLY_MODES_DST_PREMULTIPLY */
	unsigned int srcglobalpremul:2;

	/* gcregColorMultiplyModesRegAddrs:
		reserved */
	unsigned int _reserved_10_19:10;

	/* gcregColorMultiplyModesRegAddrs:
		GCREG_COLOR_MULTIPLY_MODES_DST_DEMULTIPLY */
	unsigned int dstdemul:1;

	/* gcregColorMultiplyModesRegAddrs:
		reserved */
	unsigned int _reserved_21_31:11;
};

/*******************************************************************************
** State gcregPETransparency
*/

#define gcregPETransparencyRegAddrs                                       0x04B5
#define GCREG_PE_TRANSPARENCY_MSB                                             15
#define GCREG_PE_TRANSPARENCY_LSB                                              0
#define GCREG_PE_TRANSPARENCY_BLK                                              0
#define GCREG_PE_TRANSPARENCY_Count                                            1
#define GCREG_PE_TRANSPARENCY_FieldMask                               0xB3331333
#define GCREG_PE_TRANSPARENCY_ReadMask                                0xB3331333
#define GCREG_PE_TRANSPARENCY_WriteMask                               0xB3331333
#define GCREG_PE_TRANSPARENCY_ResetValue                              0x00000000

/* Source transparency mode. */
#define GCREG_PE_TRANSPARENCY_SOURCE                                       1 : 0
#define GCREG_PE_TRANSPARENCY_SOURCE_End                                       1
#define GCREG_PE_TRANSPARENCY_SOURCE_Start                                     0
#define GCREG_PE_TRANSPARENCY_SOURCE_Type                                    U02
#define   GCREG_PE_TRANSPARENCY_SOURCE_OPAQUE                                0x0
#define   GCREG_PE_TRANSPARENCY_SOURCE_MASK                                  0x1
#define   GCREG_PE_TRANSPARENCY_SOURCE_KEY                                   0x2

/* Pattern transparency mode. KEY transparency mode is reserved. */
#define GCREG_PE_TRANSPARENCY_PATTERN                                      5 : 4
#define GCREG_PE_TRANSPARENCY_PATTERN_End                                      5
#define GCREG_PE_TRANSPARENCY_PATTERN_Start                                    4
#define GCREG_PE_TRANSPARENCY_PATTERN_Type                                   U02
#define   GCREG_PE_TRANSPARENCY_PATTERN_OPAQUE                               0x0
#define   GCREG_PE_TRANSPARENCY_PATTERN_MASK                                 0x1
#define   GCREG_PE_TRANSPARENCY_PATTERN_KEY                                  0x2

/* Destination transparency mode. MASK transparency mode is reserved. */
#define GCREG_PE_TRANSPARENCY_DESTINATION                                  9 : 8
#define GCREG_PE_TRANSPARENCY_DESTINATION_End                                  9
#define GCREG_PE_TRANSPARENCY_DESTINATION_Start                                8
#define GCREG_PE_TRANSPARENCY_DESTINATION_Type                               U02
#define   GCREG_PE_TRANSPARENCY_DESTINATION_OPAQUE                           0x0
#define   GCREG_PE_TRANSPARENCY_DESTINATION_MASK                             0x1
#define   GCREG_PE_TRANSPARENCY_DESTINATION_KEY                              0x2

/* Mask field for Source/Pattern/Destination fields. */
#define GCREG_PE_TRANSPARENCY_MASK_TRANSPARENCY                          12 : 12
#define GCREG_PE_TRANSPARENCY_MASK_TRANSPARENCY_End                           12
#define GCREG_PE_TRANSPARENCY_MASK_TRANSPARENCY_Start                         12
#define GCREG_PE_TRANSPARENCY_MASK_TRANSPARENCY_Type                         U01
#define   GCREG_PE_TRANSPARENCY_MASK_TRANSPARENCY_ENABLED                    0x0
#define   GCREG_PE_TRANSPARENCY_MASK_TRANSPARENCY_MASKED                     0x1

/* Source usage override. */
#define GCREG_PE_TRANSPARENCY_USE_SRC_OVERRIDE                           17 : 16
#define GCREG_PE_TRANSPARENCY_USE_SRC_OVERRIDE_End                            17
#define GCREG_PE_TRANSPARENCY_USE_SRC_OVERRIDE_Start                          16
#define GCREG_PE_TRANSPARENCY_USE_SRC_OVERRIDE_Type                          U02
#define   GCREG_PE_TRANSPARENCY_USE_SRC_OVERRIDE_DEFAULT                     0x0
#define   GCREG_PE_TRANSPARENCY_USE_SRC_OVERRIDE_USE_ENABLE                  0x1
#define   GCREG_PE_TRANSPARENCY_USE_SRC_OVERRIDE_USE_DISABLE                 0x2

/* Pattern usage override. */
#define GCREG_PE_TRANSPARENCY_USE_PAT_OVERRIDE                           21 : 20
#define GCREG_PE_TRANSPARENCY_USE_PAT_OVERRIDE_End                            21
#define GCREG_PE_TRANSPARENCY_USE_PAT_OVERRIDE_Start                          20
#define GCREG_PE_TRANSPARENCY_USE_PAT_OVERRIDE_Type                          U02
#define   GCREG_PE_TRANSPARENCY_USE_PAT_OVERRIDE_DEFAULT                     0x0
#define   GCREG_PE_TRANSPARENCY_USE_PAT_OVERRIDE_USE_ENABLE                  0x1
#define   GCREG_PE_TRANSPARENCY_USE_PAT_OVERRIDE_USE_DISABLE                 0x2

/* Destination usage override. */
#define GCREG_PE_TRANSPARENCY_USE_DST_OVERRIDE                           25 : 24
#define GCREG_PE_TRANSPARENCY_USE_DST_OVERRIDE_End                            25
#define GCREG_PE_TRANSPARENCY_USE_DST_OVERRIDE_Start                          24
#define GCREG_PE_TRANSPARENCY_USE_DST_OVERRIDE_Type                          U02
#define   GCREG_PE_TRANSPARENCY_USE_DST_OVERRIDE_DEFAULT                     0x0
#define   GCREG_PE_TRANSPARENCY_USE_DST_OVERRIDE_USE_ENABLE                  0x1
#define   GCREG_PE_TRANSPARENCY_USE_DST_OVERRIDE_USE_DISABLE                 0x2

/* 2D resource usage override mask field. */
#define GCREG_PE_TRANSPARENCY_MASK_RESOURCE_OVERRIDE                     28 : 28
#define GCREG_PE_TRANSPARENCY_MASK_RESOURCE_OVERRIDE_End                      28
#define GCREG_PE_TRANSPARENCY_MASK_RESOURCE_OVERRIDE_Start                    28
#define GCREG_PE_TRANSPARENCY_MASK_RESOURCE_OVERRIDE_Type                    U01
#define   GCREG_PE_TRANSPARENCY_MASK_RESOURCE_OVERRIDE_ENABLED               0x0
#define   GCREG_PE_TRANSPARENCY_MASK_RESOURCE_OVERRIDE_MASKED                0x1

/* DEB Color Key. */
#define GCREG_PE_TRANSPARENCY_DFB_COLOR_KEY                              29 : 29
#define GCREG_PE_TRANSPARENCY_DFB_COLOR_KEY_End                               29
#define GCREG_PE_TRANSPARENCY_DFB_COLOR_KEY_Start                             29
#define GCREG_PE_TRANSPARENCY_DFB_COLOR_KEY_Type                             U01
#define   GCREG_PE_TRANSPARENCY_DFB_COLOR_KEY_DISABLED                       0x0
#define   GCREG_PE_TRANSPARENCY_DFB_COLOR_KEY_ENABLED                        0x1

#define GCREG_PE_TRANSPARENCY_MASK_DFB_COLOR_KEY                         31 : 31
#define GCREG_PE_TRANSPARENCY_MASK_DFB_COLOR_KEY_End                          31
#define GCREG_PE_TRANSPARENCY_MASK_DFB_COLOR_KEY_Start                        31
#define GCREG_PE_TRANSPARENCY_MASK_DFB_COLOR_KEY_Type                        U01
#define   GCREG_PE_TRANSPARENCY_MASK_DFB_COLOR_KEY_ENABLED                   0x0
#define   GCREG_PE_TRANSPARENCY_MASK_DFB_COLOR_KEY_MASKED                    0x1

/*******************************************************************************
** State gcregPEControl
*/

/* General purpose control register. */

#define gcregPEControlRegAddrs                                            0x04B6
#define GCREG_PE_CONTROL_MSB                                                  15
#define GCREG_PE_CONTROL_LSB                                                   0
#define GCREG_PE_CONTROL_BLK                                                   0
#define GCREG_PE_CONTROL_Count                                                 1
#define GCREG_PE_CONTROL_FieldMask                                    0x00000999
#define GCREG_PE_CONTROL_ReadMask                                     0x00000999
#define GCREG_PE_CONTROL_WriteMask                                    0x00000999
#define GCREG_PE_CONTROL_ResetValue                                   0x00000000

#define GCREG_PE_CONTROL_YUV                                               0 : 0
#define GCREG_PE_CONTROL_YUV_End                                               0
#define GCREG_PE_CONTROL_YUV_Start                                             0
#define GCREG_PE_CONTROL_YUV_Type                                            U01
#define   GCREG_PE_CONTROL_YUV_601                                           0x0
#define   GCREG_PE_CONTROL_YUV_709                                           0x1

#define GCREG_PE_CONTROL_MASK_YUV                                          3 : 3
#define GCREG_PE_CONTROL_MASK_YUV_End                                          3
#define GCREG_PE_CONTROL_MASK_YUV_Start                                        3
#define GCREG_PE_CONTROL_MASK_YUV_Type                                       U01
#define   GCREG_PE_CONTROL_MASK_YUV_ENABLED                                  0x0
#define   GCREG_PE_CONTROL_MASK_YUV_MASKED                                   0x1

#define GCREG_PE_CONTROL_UV_SWIZZLE                                        4 : 4
#define GCREG_PE_CONTROL_UV_SWIZZLE_End                                        4
#define GCREG_PE_CONTROL_UV_SWIZZLE_Start                                      4
#define GCREG_PE_CONTROL_UV_SWIZZLE_Type                                     U01
#define   GCREG_PE_CONTROL_UV_SWIZZLE_UV                                     0x0
#define   GCREG_PE_CONTROL_UV_SWIZZLE_VU                                     0x1

#define GCREG_PE_CONTROL_MASK_UV_SWIZZLE                                   7 : 7
#define GCREG_PE_CONTROL_MASK_UV_SWIZZLE_End                                   7
#define GCREG_PE_CONTROL_MASK_UV_SWIZZLE_Start                                 7
#define GCREG_PE_CONTROL_MASK_UV_SWIZZLE_Type                                U01
#define   GCREG_PE_CONTROL_MASK_UV_SWIZZLE_ENABLED                           0x0
#define   GCREG_PE_CONTROL_MASK_UV_SWIZZLE_MASKED                            0x1

/* YUV to RGB convert enable */
#define GCREG_PE_CONTROL_YUVRGB                                            8 : 8
#define GCREG_PE_CONTROL_YUVRGB_End                                            8
#define GCREG_PE_CONTROL_YUVRGB_Start                                          8
#define GCREG_PE_CONTROL_YUVRGB_Type                                         U01
#define   GCREG_PE_CONTROL_YUVRGB_DISABLED                                   0x0
#define   GCREG_PE_CONTROL_YUVRGB_ENABLED                                    0x1

#define GCREG_PE_CONTROL_MASK_YUVRGB                                     11 : 11
#define GCREG_PE_CONTROL_MASK_YUVRGB_End                                      11
#define GCREG_PE_CONTROL_MASK_YUVRGB_Start                                    11
#define GCREG_PE_CONTROL_MASK_YUVRGB_Type                                    U01
#define   GCREG_PE_CONTROL_MASK_YUVRGB_ENABLED                               0x0
#define   GCREG_PE_CONTROL_MASK_YUVRGB_MASKED                                0x1

/*******************************************************************************
** State gcregSrcColorKeyHigh
*/

/* Defines the source transparency color in source format. */

#define gcregSrcColorKeyHighRegAddrs                                      0x04B7
#define GCREG_SRC_COLOR_KEY_HIGH_Address                                 0x012DC
#define GCREG_SRC_COLOR_KEY_HIGH_MSB                                          15
#define GCREG_SRC_COLOR_KEY_HIGH_LSB                                           0
#define GCREG_SRC_COLOR_KEY_HIGH_BLK                                           0
#define GCREG_SRC_COLOR_KEY_HIGH_Count                                         1
#define GCREG_SRC_COLOR_KEY_HIGH_FieldMask                            0xFFFFFFFF
#define GCREG_SRC_COLOR_KEY_HIGH_ReadMask                             0xFFFFFFFF
#define GCREG_SRC_COLOR_KEY_HIGH_WriteMask                            0xFFFFFFFF
#define GCREG_SRC_COLOR_KEY_HIGH_ResetValue                           0x00000000

#define GCREG_SRC_COLOR_KEY_HIGH_ALPHA                                   31 : 24
#define GCREG_SRC_COLOR_KEY_HIGH_ALPHA_End                                    31
#define GCREG_SRC_COLOR_KEY_HIGH_ALPHA_Start                                  24
#define GCREG_SRC_COLOR_KEY_HIGH_ALPHA_Type                                  U08

#define GCREG_SRC_COLOR_KEY_HIGH_RED                                     23 : 16
#define GCREG_SRC_COLOR_KEY_HIGH_RED_End                                      23
#define GCREG_SRC_COLOR_KEY_HIGH_RED_Start                                    16
#define GCREG_SRC_COLOR_KEY_HIGH_RED_Type                                    U08

#define GCREG_SRC_COLOR_KEY_HIGH_GREEN                                    15 : 8
#define GCREG_SRC_COLOR_KEY_HIGH_GREEN_End                                    15
#define GCREG_SRC_COLOR_KEY_HIGH_GREEN_Start                                   8
#define GCREG_SRC_COLOR_KEY_HIGH_GREEN_Type                                  U08

#define GCREG_SRC_COLOR_KEY_HIGH_BLUE                                      7 : 0
#define GCREG_SRC_COLOR_KEY_HIGH_BLUE_End                                      7
#define GCREG_SRC_COLOR_KEY_HIGH_BLUE_Start                                    0
#define GCREG_SRC_COLOR_KEY_HIGH_BLUE_Type                                   U08

/*******************************************************************************
** State gcregDestColorKeyHigh
*/

/* Defines the destination transparency color in destination format. */

#define gcregDestColorKeyHighRegAddrs                                     0x04B8
#define GCREG_DEST_COLOR_KEY_HIGH_MSB                                         15
#define GCREG_DEST_COLOR_KEY_HIGH_LSB                                          0
#define GCREG_DEST_COLOR_KEY_HIGH_BLK                                          0
#define GCREG_DEST_COLOR_KEY_HIGH_Count                                        1
#define GCREG_DEST_COLOR_KEY_HIGH_FieldMask                           0xFFFFFFFF
#define GCREG_DEST_COLOR_KEY_HIGH_ReadMask                            0xFFFFFFFF
#define GCREG_DEST_COLOR_KEY_HIGH_WriteMask                           0xFFFFFFFF
#define GCREG_DEST_COLOR_KEY_HIGH_ResetValue                          0x00000000

#define GCREG_DEST_COLOR_KEY_HIGH_ALPHA                                  31 : 24
#define GCREG_DEST_COLOR_KEY_HIGH_ALPHA_End                                   31
#define GCREG_DEST_COLOR_KEY_HIGH_ALPHA_Start                                 24
#define GCREG_DEST_COLOR_KEY_HIGH_ALPHA_Type                                 U08

#define GCREG_DEST_COLOR_KEY_HIGH_RED                                    23 : 16
#define GCREG_DEST_COLOR_KEY_HIGH_RED_End                                     23
#define GCREG_DEST_COLOR_KEY_HIGH_RED_Start                                   16
#define GCREG_DEST_COLOR_KEY_HIGH_RED_Type                                   U08

#define GCREG_DEST_COLOR_KEY_HIGH_GREEN                                   15 : 8
#define GCREG_DEST_COLOR_KEY_HIGH_GREEN_End                                   15
#define GCREG_DEST_COLOR_KEY_HIGH_GREEN_Start                                  8
#define GCREG_DEST_COLOR_KEY_HIGH_GREEN_Type                                 U08

#define GCREG_DEST_COLOR_KEY_HIGH_BLUE                                     7 : 0
#define GCREG_DEST_COLOR_KEY_HIGH_BLUE_End                                     7
#define GCREG_DEST_COLOR_KEY_HIGH_BLUE_Start                                   0
#define GCREG_DEST_COLOR_KEY_HIGH_BLUE_Type                                  U08

/*******************************************************************************
** State gcregPEDitherLow
*/

/* PE dither register.
** If you don't want dither, set all fields to their reset values.
*/

#define gcregPEDitherLowRegAddrs                                          0x04BA
#define GCREG_PE_DITHER_LOW_MSB                                               15
#define GCREG_PE_DITHER_LOW_LSB                                                0
#define GCREG_PE_DITHER_LOW_BLK                                                0
#define GCREG_PE_DITHER_LOW_Count                                              1
#define GCREG_PE_DITHER_LOW_FieldMask                                 0xFFFFFFFF
#define GCREG_PE_DITHER_LOW_ReadMask                                  0xFFFFFFFF
#define GCREG_PE_DITHER_LOW_WriteMask                                 0xFFFFFFFF
#define GCREG_PE_DITHER_LOW_ResetValue                                0xFFFFFFFF

/* X,Y = 0,0 */
#define GCREG_PE_DITHER_LOW_PIXEL_X0_Y0                                    3 : 0
#define GCREG_PE_DITHER_LOW_PIXEL_X0_Y0_End                                    3
#define GCREG_PE_DITHER_LOW_PIXEL_X0_Y0_Start                                  0
#define GCREG_PE_DITHER_LOW_PIXEL_X0_Y0_Type                                 U04

/* X,Y = 1,0 */
#define GCREG_PE_DITHER_LOW_PIXEL_X1_Y0                                    7 : 4
#define GCREG_PE_DITHER_LOW_PIXEL_X1_Y0_End                                    7
#define GCREG_PE_DITHER_LOW_PIXEL_X1_Y0_Start                                  4
#define GCREG_PE_DITHER_LOW_PIXEL_X1_Y0_Type                                 U04

/* X,Y = 2,0 */
#define GCREG_PE_DITHER_LOW_PIXEL_X2_Y0                                   11 : 8
#define GCREG_PE_DITHER_LOW_PIXEL_X2_Y0_End                                   11
#define GCREG_PE_DITHER_LOW_PIXEL_X2_Y0_Start                                  8
#define GCREG_PE_DITHER_LOW_PIXEL_X2_Y0_Type                                 U04

/* X,Y = 3,0 */
#define GCREG_PE_DITHER_LOW_PIXEL_X3_Y0                                  15 : 12
#define GCREG_PE_DITHER_LOW_PIXEL_X3_Y0_End                                   15
#define GCREG_PE_DITHER_LOW_PIXEL_X3_Y0_Start                                 12
#define GCREG_PE_DITHER_LOW_PIXEL_X3_Y0_Type                                 U04

/* X,Y = 0,1 */
#define GCREG_PE_DITHER_LOW_PIXEL_X0_Y1                                  19 : 16
#define GCREG_PE_DITHER_LOW_PIXEL_X0_Y1_End                                   19
#define GCREG_PE_DITHER_LOW_PIXEL_X0_Y1_Start                                 16
#define GCREG_PE_DITHER_LOW_PIXEL_X0_Y1_Type                                 U04

/* X,Y = 1,1 */
#define GCREG_PE_DITHER_LOW_PIXEL_X1_Y1                                  23 : 20
#define GCREG_PE_DITHER_LOW_PIXEL_X1_Y1_End                                   23
#define GCREG_PE_DITHER_LOW_PIXEL_X1_Y1_Start                                 20
#define GCREG_PE_DITHER_LOW_PIXEL_X1_Y1_Type                                 U04

/* X,Y = 2,1 */
#define GCREG_PE_DITHER_LOW_PIXEL_X2_Y1                                  27 : 24
#define GCREG_PE_DITHER_LOW_PIXEL_X2_Y1_End                                   27
#define GCREG_PE_DITHER_LOW_PIXEL_X2_Y1_Start                                 24
#define GCREG_PE_DITHER_LOW_PIXEL_X2_Y1_Type                                 U04

/* X,Y = 3,1 */
#define GCREG_PE_DITHER_LOW_PIXEL_X3_Y1                                  31 : 28
#define GCREG_PE_DITHER_LOW_PIXEL_X3_Y1_End                                   31
#define GCREG_PE_DITHER_LOW_PIXEL_X3_Y1_Start                                 28
#define GCREG_PE_DITHER_LOW_PIXEL_X3_Y1_Type                                 U04

/*******************************************************************************
** State gcregPEDitherHigh
*/

#define gcregPEDitherHighRegAddrs                                         0x04BB
#define GCREG_PE_DITHER_HIGH_MSB                                              15
#define GCREG_PE_DITHER_HIGH_LSB                                               0
#define GCREG_PE_DITHER_LOW_HIGH_BLK                                           0
#define GCREG_PE_DITHER_HIGH_Count                                             1
#define GCREG_PE_DITHER_HIGH_FieldMask                                0xFFFFFFFF
#define GCREG_PE_DITHER_HIGH_ReadMask                                 0xFFFFFFFF
#define GCREG_PE_DITHER_HIGH_WriteMask                                0xFFFFFFFF
#define GCREG_PE_DITHER_HIGH_ResetValue                               0xFFFFFFFF

/* X,Y = 0,2 */
#define GCREG_PE_DITHER_HIGH_PIXEL_X0_Y2                                   3 : 0
#define GCREG_PE_DITHER_HIGH_PIXEL_X0_Y2_End                                   3
#define GCREG_PE_DITHER_HIGH_PIXEL_X0_Y2_Start                                 0
#define GCREG_PE_DITHER_HIGH_PIXEL_X0_Y2_Type                                U04

/* X,Y = 1,2 */
#define GCREG_PE_DITHER_HIGH_PIXEL_X1_Y2                                   7 : 4
#define GCREG_PE_DITHER_HIGH_PIXEL_X1_Y2_End                                   7
#define GCREG_PE_DITHER_HIGH_PIXEL_X1_Y2_Start                                 4
#define GCREG_PE_DITHER_HIGH_PIXEL_X1_Y2_Type                                U04

/* X,Y = 2,2 */
#define GCREG_PE_DITHER_HIGH_PIXEL_X2_Y2                                  11 : 8
#define GCREG_PE_DITHER_HIGH_PIXEL_X2_Y2_End                                  11
#define GCREG_PE_DITHER_HIGH_PIXEL_X2_Y2_Start                                 8
#define GCREG_PE_DITHER_HIGH_PIXEL_X2_Y2_Type                                U04

/* X,Y = 0,3 */
#define GCREG_PE_DITHER_HIGH_PIXEL_X3_Y2                                 15 : 12
#define GCREG_PE_DITHER_HIGH_PIXEL_X3_Y2_End                                  15
#define GCREG_PE_DITHER_HIGH_PIXEL_X3_Y2_Start                                12
#define GCREG_PE_DITHER_HIGH_PIXEL_X3_Y2_Type                                U04

/* X,Y = 1,3 */
#define GCREG_PE_DITHER_HIGH_PIXEL_X0_Y3                                 19 : 16
#define GCREG_PE_DITHER_HIGH_PIXEL_X0_Y3_End                                  19
#define GCREG_PE_DITHER_HIGH_PIXEL_X0_Y3_Start                                16
#define GCREG_PE_DITHER_HIGH_PIXEL_X0_Y3_Type                                U04

/* X,Y = 2,3 */
#define GCREG_PE_DITHER_HIGH_PIXEL_X1_Y3                                 23 : 20
#define GCREG_PE_DITHER_HIGH_PIXEL_X1_Y3_End                                  23
#define GCREG_PE_DITHER_HIGH_PIXEL_X1_Y3_Start                                20
#define GCREG_PE_DITHER_HIGH_PIXEL_X1_Y3_Type                                U04

/* X,Y = 3,3 */
#define GCREG_PE_DITHER_HIGH_PIXEL_X2_Y3                                 27 : 24
#define GCREG_PE_DITHER_HIGH_PIXEL_X2_Y3_End                                  27
#define GCREG_PE_DITHER_HIGH_PIXEL_X2_Y3_Start                                24
#define GCREG_PE_DITHER_HIGH_PIXEL_X2_Y3_Type                                U04

/* X,Y = 3,2 */
#define GCREG_PE_DITHER_HIGH_PIXEL_X3_Y3                                 31 : 28
#define GCREG_PE_DITHER_HIGH_PIXEL_X3_Y3_End                                  31
#define GCREG_PE_DITHER_HIGH_PIXEL_X3_Y3_Start                                28
#define GCREG_PE_DITHER_HIGH_PIXEL_X3_Y3_Type                                U04

/*******************************************************************************
** State gcregSrcExConfig
*/

#define gcregSrcExConfigRegAddrs                                          0x04C0
#define GCREG_SRC_EX_CONFIG_MSB                                               15
#define GCREG_SRC_EX_CONFIG_LSB                                                0
#define GCREG_SRC_EX_CONFIG_BLK                                                0
#define GCREG_SRC_EX_CONFIG_Count                                              1
#define GCREG_SRC_EX_CONFIG_FieldMask                                 0x00000109
#define GCREG_SRC_EX_CONFIG_ReadMask                                  0x00000109
#define GCREG_SRC_EX_CONFIG_WriteMask                                 0x00000109
#define GCREG_SRC_EX_CONFIG_ResetValue                                0x00000000

/* Source multi tiled address computation control. */
#define GCREG_SRC_EX_CONFIG_MULTI_TILED                                    0 : 0
#define GCREG_SRC_EX_CONFIG_MULTI_TILED_End                                    0
#define GCREG_SRC_EX_CONFIG_MULTI_TILED_Start                                  0
#define GCREG_SRC_EX_CONFIG_MULTI_TILED_Type                                 U01
#define   GCREG_SRC_EX_CONFIG_MULTI_TILED_DISABLED                           0x0
#define   GCREG_SRC_EX_CONFIG_MULTI_TILED_ENABLED                            0x1

/* Source super tiled address computation control. */
#define GCREG_SRC_EX_CONFIG_SUPER_TILED                                    3 : 3
#define GCREG_SRC_EX_CONFIG_SUPER_TILED_End                                    3
#define GCREG_SRC_EX_CONFIG_SUPER_TILED_Start                                  3
#define GCREG_SRC_EX_CONFIG_SUPER_TILED_Type                                 U01
#define   GCREG_SRC_EX_CONFIG_SUPER_TILED_DISABLED                           0x0
#define   GCREG_SRC_EX_CONFIG_SUPER_TILED_ENABLED                            0x1

/* Source super tiled address computation control. */
#define GCREG_SRC_EX_CONFIG_MINOR_TILED                                    8 : 8
#define GCREG_SRC_EX_CONFIG_MINOR_TILED_End                                    8
#define GCREG_SRC_EX_CONFIG_MINOR_TILED_Start                                  8
#define GCREG_SRC_EX_CONFIG_MINOR_TILED_Type                                 U01
#define   GCREG_SRC_EX_CONFIG_MINOR_TILED_DISABLED                           0x0
#define   GCREG_SRC_EX_CONFIG_MINOR_TILED_ENABLED                            0x1

/*******************************************************************************
** State gcregSrcExAddress
*/

/* 32-bit aligned base address of the source extra surface. */

#define gcregSrcExAddressRegAddrs                                         0x04C1
#define GCREG_SRC_EX_ADDRESS_MSB                                              15
#define GCREG_SRC_EX_ADDRESS_LSB                                               0
#define GCREG_SRC_EX_ADDRESS_BLK                                               0
#define GCREG_SRC_EX_ADDRESS_Count                                             1
#define GCREG_SRC_EX_ADDRESS_FieldMask                                0xFFFFFFFF
#define GCREG_SRC_EX_ADDRESS_ReadMask                                 0xFFFFFFFC
#define GCREG_SRC_EX_ADDRESS_WriteMask                                0xFFFFFFFC
#define GCREG_SRC_EX_ADDRESS_ResetValue                               0x00000000

#define GCREG_SRC_EX_ADDRESS_ADDRESS                                      31 : 0
#define GCREG_SRC_EX_ADDRESS_ADDRESS_End                                      30
#define GCREG_SRC_EX_ADDRESS_ADDRESS_Start                                     0
#define GCREG_SRC_EX_ADDRESS_ADDRESS_Type                                    U31

/*******************************************************************************
** State gcregDEMultiSource
*/

/* MutiSource control register. */

#define gcregDEMultiSourceRegAddrs                                        0x04C2
#define GCREG_DE_MULTI_SOURCE_MSB                                             15
#define GCREG_DE_MULTI_SOURCE_LSB                                              0
#define GCREG_DE_MULTI_SOURCE_BLK                                              0
#define GCREG_DE_MULTI_SOURCE_Count                                            1
#define GCREG_DE_MULTI_SOURCE_FieldMask                               0x00070707
#define GCREG_DE_MULTI_SOURCE_ReadMask                                0x00070707
#define GCREG_DE_MULTI_SOURCE_WriteMask                               0x00070707
#define GCREG_DE_MULTI_SOURCE_ResetValue                              0x00000000

/* Number of source surfaces minus 1. */
#define GCREG_DE_MULTI_SOURCE_MAX_SOURCE                                   2 : 0
#define GCREG_DE_MULTI_SOURCE_MAX_SOURCE_End                                   2
#define GCREG_DE_MULTI_SOURCE_MAX_SOURCE_Start                                 0
#define GCREG_DE_MULTI_SOURCE_MAX_SOURCE_Type                                U03

/* Number of pixels for horizontal block walker. */
#define GCREG_DE_MULTI_SOURCE_HORIZONTAL_BLOCK                            10 : 8
#define GCREG_DE_MULTI_SOURCE_HORIZONTAL_BLOCK_End                            10
#define GCREG_DE_MULTI_SOURCE_HORIZONTAL_BLOCK_Start                           8
#define GCREG_DE_MULTI_SOURCE_HORIZONTAL_BLOCK_Type                          U03
#define   GCREG_DE_MULTI_SOURCE_HORIZONTAL_BLOCK_PIXEL16                     0x0
#define   GCREG_DE_MULTI_SOURCE_HORIZONTAL_BLOCK_PIXEL32                     0x1
#define   GCREG_DE_MULTI_SOURCE_HORIZONTAL_BLOCK_PIXEL64                     0x2
#define   GCREG_DE_MULTI_SOURCE_HORIZONTAL_BLOCK_PIXEL128                    0x3
#define   GCREG_DE_MULTI_SOURCE_HORIZONTAL_BLOCK_PIXEL256                    0x4
#define   GCREG_DE_MULTI_SOURCE_HORIZONTAL_BLOCK_PIXEL512                    0x5

/* Number of lines for vertical block walker. */
#define GCREG_DE_MULTI_SOURCE_VERTICAL_BLOCK                             18 : 16
#define GCREG_DE_MULTI_SOURCE_VERTICAL_BLOCK_End                              18
#define GCREG_DE_MULTI_SOURCE_VERTICAL_BLOCK_Start                            16
#define GCREG_DE_MULTI_SOURCE_VERTICAL_BLOCK_Type                            U03
#define   GCREG_DE_MULTI_SOURCE_VERTICAL_BLOCK_LINE1                         0x0
#define   GCREG_DE_MULTI_SOURCE_VERTICAL_BLOCK_LINE2                         0x1
#define   GCREG_DE_MULTI_SOURCE_VERTICAL_BLOCK_LINE4                         0x2
#define   GCREG_DE_MULTI_SOURCE_VERTICAL_BLOCK_LINE8                         0x3
#define   GCREG_DE_MULTI_SOURCE_VERTICAL_BLOCK_LINE16                        0x4
#define   GCREG_DE_MULTI_SOURCE_VERTICAL_BLOCK_LINE32                        0x5
#define   GCREG_DE_MULTI_SOURCE_VERTICAL_BLOCK_LINE64                        0x6
#define   GCREG_DE_MULTI_SOURCE_VERTICAL_BLOCK_LINE128                       0x7

struct gcregmultisource {
	/* gcregDEMultiSourceRegAddrs:GCREG_DE_MULTI_SOURCE_MAX_SOURCE */
	unsigned int srccount:3;

	/* gcregDEMultiSourceRegAddrs:reserved */
	unsigned int _reserved_3_7:5;

	/* gcregDEMultiSourceRegAddrs:GCREG_DE_MULTI_SOURCE_HORIZONTAL_BLOCK */
	unsigned int horblock:3;

	/* gcregDEMultiSourceRegAddrs:reserved */
	unsigned int _reserved_11_15:5;

	/* gcregDEMultiSourceRegAddrs:GCREG_DE_MULTI_SOURCE_VERTICAL_BLOCK */
	unsigned int verblock:3;

	/* gcregDEMultiSourceRegAddrs:reserved */
	unsigned int _reserved_19_31:13;
};

/*******************************************************************************
** State gcregDEYUVConversion
*/

/* Configure the YUV to YUV conversion. */

#define gcregDEYUVConversionRegAddrs                                      0x04C3
#define GCREG_DEYUV_CONVERSION_MSB                                            15
#define GCREG_DEYUV_CONVERSION_LSB                                             0
#define GCREG_DEYUV_CONVERSION_BLK                                             0
#define GCREG_DEYUV_CONVERSION_Count                                           1
#define GCREG_DEYUV_CONVERSION_FieldMask                              0xFFFFFFFF
#define GCREG_DEYUV_CONVERSION_ReadMask                               0xFFFFFFFF
#define GCREG_DEYUV_CONVERSION_WriteMask                              0xFFFFFFFF
#define GCREG_DEYUV_CONVERSION_ResetValue                             0x00000000

/* Select the number of planes we need to process. */
#define GCREG_DEYUV_CONVERSION_ENABLE                                      1 : 0
#define GCREG_DEYUV_CONVERSION_ENABLE_End                                      1
#define GCREG_DEYUV_CONVERSION_ENABLE_Start                                    0
#define GCREG_DEYUV_CONVERSION_ENABLE_Type                                   U02
/* YUV to YUV conversion is turned off. */
#define   GCREG_DEYUV_CONVERSION_ENABLE_OFF                                  0x0
/* YUV to YUV conversion is writing to 1 plane. */
#define   GCREG_DEYUV_CONVERSION_ENABLE_PLANE1                               0x1
/* YUV to YUV conversion is writing to 2 planes. */
#define   GCREG_DEYUV_CONVERSION_ENABLE_PLANE2                               0x2
/* YUV to YUV conversion is writing to 3 planes. */
#define   GCREG_DEYUV_CONVERSION_ENABLE_PLANE3                               0x3

/* Number of channels to process - 1 for plane 1. */
#define GCREG_DEYUV_CONVERSION_PLANE1_COUNT                                3 : 2
#define GCREG_DEYUV_CONVERSION_PLANE1_COUNT_End                                3
#define GCREG_DEYUV_CONVERSION_PLANE1_COUNT_Start                              2
#define GCREG_DEYUV_CONVERSION_PLANE1_COUNT_Type                             U02

/* Number of channels to process - 1 for plane 2. */
#define GCREG_DEYUV_CONVERSION_PLANE2_COUNT                                5 : 4
#define GCREG_DEYUV_CONVERSION_PLANE2_COUNT_End                                5
#define GCREG_DEYUV_CONVERSION_PLANE2_COUNT_Start                              4
#define GCREG_DEYUV_CONVERSION_PLANE2_COUNT_Type                             U02

/* Number of channels to process - 1 for plane 3. */
#define GCREG_DEYUV_CONVERSION_PLANE3_COUNT                                7 : 6
#define GCREG_DEYUV_CONVERSION_PLANE3_COUNT_End                                7
#define GCREG_DEYUV_CONVERSION_PLANE3_COUNT_Start                              6
#define GCREG_DEYUV_CONVERSION_PLANE3_COUNT_Type                             U02

/* Select which color channel to pick for B channel for plane 1. */
#define GCREG_DEYUV_CONVERSION_PLANE1_SWIZZLE_B                            9 : 8
#define GCREG_DEYUV_CONVERSION_PLANE1_SWIZZLE_B_End                            9
#define GCREG_DEYUV_CONVERSION_PLANE1_SWIZZLE_B_Start                          8
#define GCREG_DEYUV_CONVERSION_PLANE1_SWIZZLE_B_Type                         U02
#define   GCREG_DEYUV_CONVERSION_PLANE1_SWIZZLE_B_BLUE                       0x0
#define   GCREG_DEYUV_CONVERSION_PLANE1_SWIZZLE_B_GREEN                      0x1
#define   GCREG_DEYUV_CONVERSION_PLANE1_SWIZZLE_B_RED                        0x2
#define   GCREG_DEYUV_CONVERSION_PLANE1_SWIZZLE_B_ALPHA                      0x3

/* Select which color channel to pick for G channel for plane 1. */
#define GCREG_DEYUV_CONVERSION_PLANE1_SWIZZLE_G                          11 : 10
#define GCREG_DEYUV_CONVERSION_PLANE1_SWIZZLE_G_End                           11
#define GCREG_DEYUV_CONVERSION_PLANE1_SWIZZLE_G_Start                         10
#define GCREG_DEYUV_CONVERSION_PLANE1_SWIZZLE_G_Type                         U02
#define   GCREG_DEYUV_CONVERSION_PLANE1_SWIZZLE_G_BLUE                       0x0
#define   GCREG_DEYUV_CONVERSION_PLANE1_SWIZZLE_G_GREEN                      0x1
#define   GCREG_DEYUV_CONVERSION_PLANE1_SWIZZLE_G_RED                        0x2
#define   GCREG_DEYUV_CONVERSION_PLANE1_SWIZZLE_G_ALPHA                      0x3

/* Select which color channel to pick for R channel for plane 1. */
#define GCREG_DEYUV_CONVERSION_PLANE1_SWIZZLE_R                          13 : 12
#define GCREG_DEYUV_CONVERSION_PLANE1_SWIZZLE_R_End                           13
#define GCREG_DEYUV_CONVERSION_PLANE1_SWIZZLE_R_Start                         12
#define GCREG_DEYUV_CONVERSION_PLANE1_SWIZZLE_R_Type                         U02
#define   GCREG_DEYUV_CONVERSION_PLANE1_SWIZZLE_R_BLUE                       0x0
#define   GCREG_DEYUV_CONVERSION_PLANE1_SWIZZLE_R_GREEN                      0x1
#define   GCREG_DEYUV_CONVERSION_PLANE1_SWIZZLE_R_RED                        0x2
#define   GCREG_DEYUV_CONVERSION_PLANE1_SWIZZLE_R_ALPHA                      0x3

/* Select which color channel to pick for A channel for plane 1. */
#define GCREG_DEYUV_CONVERSION_PLANE1_SWIZZLE_A                          15 : 14
#define GCREG_DEYUV_CONVERSION_PLANE1_SWIZZLE_A_End                           15
#define GCREG_DEYUV_CONVERSION_PLANE1_SWIZZLE_A_Start                         14
#define GCREG_DEYUV_CONVERSION_PLANE1_SWIZZLE_A_Type                         U02
#define   GCREG_DEYUV_CONVERSION_PLANE1_SWIZZLE_A_BLUE                       0x0
#define   GCREG_DEYUV_CONVERSION_PLANE1_SWIZZLE_A_GREEN                      0x1
#define   GCREG_DEYUV_CONVERSION_PLANE1_SWIZZLE_A_RED                        0x2
#define   GCREG_DEYUV_CONVERSION_PLANE1_SWIZZLE_A_ALPHA                      0x3

/* Select which color channel to pick for B channel for plane 2. */
#define GCREG_DEYUV_CONVERSION_PLANE2_SWIZZLE_B                          17 : 16
#define GCREG_DEYUV_CONVERSION_PLANE2_SWIZZLE_B_End                           17
#define GCREG_DEYUV_CONVERSION_PLANE2_SWIZZLE_B_Start                         16
#define GCREG_DEYUV_CONVERSION_PLANE2_SWIZZLE_B_Type                         U02
#define   GCREG_DEYUV_CONVERSION_PLANE2_SWIZZLE_B_BLUE                       0x0
#define   GCREG_DEYUV_CONVERSION_PLANE2_SWIZZLE_B_GREEN                      0x1
#define   GCREG_DEYUV_CONVERSION_PLANE2_SWIZZLE_B_RED                        0x2
#define   GCREG_DEYUV_CONVERSION_PLANE2_SWIZZLE_B_ALPHA                      0x3

/* Select which color channel to pick for G channel for plane 2. */
#define GCREG_DEYUV_CONVERSION_PLANE2_SWIZZLE_G                          19 : 18
#define GCREG_DEYUV_CONVERSION_PLANE2_SWIZZLE_G_End                           19
#define GCREG_DEYUV_CONVERSION_PLANE2_SWIZZLE_G_Start                         18
#define GCREG_DEYUV_CONVERSION_PLANE2_SWIZZLE_G_Type                         U02
#define   GCREG_DEYUV_CONVERSION_PLANE2_SWIZZLE_G_BLUE                       0x0
#define   GCREG_DEYUV_CONVERSION_PLANE2_SWIZZLE_G_GREEN                      0x1
#define   GCREG_DEYUV_CONVERSION_PLANE2_SWIZZLE_G_RED                        0x2
#define   GCREG_DEYUV_CONVERSION_PLANE2_SWIZZLE_G_ALPHA                      0x3

/* Select which color channel to pick for R channel for plane 2. */
#define GCREG_DEYUV_CONVERSION_PLANE2_SWIZZLE_R                          21 : 20
#define GCREG_DEYUV_CONVERSION_PLANE2_SWIZZLE_R_End                           21
#define GCREG_DEYUV_CONVERSION_PLANE2_SWIZZLE_R_Start                         20
#define GCREG_DEYUV_CONVERSION_PLANE2_SWIZZLE_R_Type                         U02
#define   GCREG_DEYUV_CONVERSION_PLANE2_SWIZZLE_R_BLUE                       0x0
#define   GCREG_DEYUV_CONVERSION_PLANE2_SWIZZLE_R_GREEN                      0x1
#define   GCREG_DEYUV_CONVERSION_PLANE2_SWIZZLE_R_RED                        0x2
#define   GCREG_DEYUV_CONVERSION_PLANE2_SWIZZLE_R_ALPHA                      0x3

/* Select which color channel to pick for A channel for plane 2. */
#define GCREG_DEYUV_CONVERSION_PLANE2_SWIZZLE_A                          23 : 22
#define GCREG_DEYUV_CONVERSION_PLANE2_SWIZZLE_A_End                           23
#define GCREG_DEYUV_CONVERSION_PLANE2_SWIZZLE_A_Start                         22
#define GCREG_DEYUV_CONVERSION_PLANE2_SWIZZLE_A_Type                         U02
#define   GCREG_DEYUV_CONVERSION_PLANE2_SWIZZLE_A_BLUE                       0x0
#define   GCREG_DEYUV_CONVERSION_PLANE2_SWIZZLE_A_GREEN                      0x1
#define   GCREG_DEYUV_CONVERSION_PLANE2_SWIZZLE_A_RED                        0x2
#define   GCREG_DEYUV_CONVERSION_PLANE2_SWIZZLE_A_ALPHA                      0x3

/* Select which color channel to pick for B channel for plane 3. */
#define GCREG_DEYUV_CONVERSION_PLANE3_SWIZZLE_B                          25 : 24
#define GCREG_DEYUV_CONVERSION_PLANE3_SWIZZLE_B_End                           25
#define GCREG_DEYUV_CONVERSION_PLANE3_SWIZZLE_B_Start                         24
#define GCREG_DEYUV_CONVERSION_PLANE3_SWIZZLE_B_Type                         U02
#define   GCREG_DEYUV_CONVERSION_PLANE3_SWIZZLE_B_BLUE                       0x0
#define   GCREG_DEYUV_CONVERSION_PLANE3_SWIZZLE_B_GREEN                      0x1
#define   GCREG_DEYUV_CONVERSION_PLANE3_SWIZZLE_B_RED                        0x2
#define   GCREG_DEYUV_CONVERSION_PLANE3_SWIZZLE_B_ALPHA                      0x3

/* Select which color channel to pick for G channel for plane 3. */
#define GCREG_DEYUV_CONVERSION_PLANE3_SWIZZLE_G                          27 : 26
#define GCREG_DEYUV_CONVERSION_PLANE3_SWIZZLE_G_End                           27
#define GCREG_DEYUV_CONVERSION_PLANE3_SWIZZLE_G_Start                         26
#define GCREG_DEYUV_CONVERSION_PLANE3_SWIZZLE_G_Type                         U02
#define   GCREG_DEYUV_CONVERSION_PLANE3_SWIZZLE_G_BLUE                       0x0
#define   GCREG_DEYUV_CONVERSION_PLANE3_SWIZZLE_G_GREEN                      0x1
#define   GCREG_DEYUV_CONVERSION_PLANE3_SWIZZLE_G_RED                        0x2
#define   GCREG_DEYUV_CONVERSION_PLANE3_SWIZZLE_G_ALPHA                      0x3

/* Select which color channel to pick for R channel for plane 3. */
#define GCREG_DEYUV_CONVERSION_PLANE3_SWIZZLE_R                          29 : 28
#define GCREG_DEYUV_CONVERSION_PLANE3_SWIZZLE_R_End                           29
#define GCREG_DEYUV_CONVERSION_PLANE3_SWIZZLE_R_Start                         28
#define GCREG_DEYUV_CONVERSION_PLANE3_SWIZZLE_R_Type                         U02
#define   GCREG_DEYUV_CONVERSION_PLANE3_SWIZZLE_R_BLUE                       0x0
#define   GCREG_DEYUV_CONVERSION_PLANE3_SWIZZLE_R_GREEN                      0x1
#define   GCREG_DEYUV_CONVERSION_PLANE3_SWIZZLE_R_RED                        0x2
#define   GCREG_DEYUV_CONVERSION_PLANE3_SWIZZLE_R_ALPHA                      0x3

/* Select which color channel to pick for A channel for plane 3. */
#define GCREG_DEYUV_CONVERSION_PLANE3_SWIZZLE_A                          31 : 30
#define GCREG_DEYUV_CONVERSION_PLANE3_SWIZZLE_A_End                           31
#define GCREG_DEYUV_CONVERSION_PLANE3_SWIZZLE_A_Start                         30
#define GCREG_DEYUV_CONVERSION_PLANE3_SWIZZLE_A_Type                         U02
#define   GCREG_DEYUV_CONVERSION_PLANE3_SWIZZLE_A_BLUE                       0x0
#define   GCREG_DEYUV_CONVERSION_PLANE3_SWIZZLE_A_GREEN                      0x1
#define   GCREG_DEYUV_CONVERSION_PLANE3_SWIZZLE_A_RED                        0x2
#define   GCREG_DEYUV_CONVERSION_PLANE3_SWIZZLE_A_ALPHA                      0x3

/*******************************************************************************
** State gcregDEPlane2Address
*/

/* Address for plane 2 if gcregDEYUVConversion.
** Enable is set to Plane2 or Plane3.
*/

#define gcregDEPlane2AddressRegAddrs                                      0x04C4
#define GCREG_DE_PLANE2_ADDRESS_Address                                  0x01310
#define GCREG_DE_PLANE2_ADDRESS_MSB                                           15
#define GCREG_DE_PLANE2_ADDRESS_LSB                                            0
#define GCREG_DE_PLANE2_ADDRESS_BLK                                            0
#define GCREG_DE_PLANE2_ADDRESS_Count                                          1
#define GCREG_DE_PLANE2_ADDRESS_FieldMask                             0xFFFFFFFF
#define GCREG_DE_PLANE2_ADDRESS_ReadMask                              0xFFFFFFFC
#define GCREG_DE_PLANE2_ADDRESS_WriteMask                             0xFFFFFFFC
#define GCREG_DE_PLANE2_ADDRESS_ResetValue                            0x00000000

#define GCREG_DE_PLANE2_ADDRESS_ADDRESS                                   31 : 0
#define GCREG_DE_PLANE2_ADDRESS_ADDRESS_End                                   30
#define GCREG_DE_PLANE2_ADDRESS_ADDRESS_Start                                  0
#define GCREG_DE_PLANE2_ADDRESS_ADDRESS_Type                                 U31

/*******************************************************************************
** State gcregDEPlane2Stride
*/

/* Stride for plane 2 if gcregDEYUVConversion.
** Enable is set to Plane2 or Plane3.
*/

#define gcregDEPlane2StrideRegAddrs                                       0x04C5
#define GCREG_DE_PLANE2_STRIDE_MSB                                            15
#define GCREG_DE_PLANE2_STRIDE_LSB                                             0
#define GCREG_DE_PLANE2_STRIDE_BLK                                             0
#define GCREG_DE_PLANE2_STRIDE_Count                                           1
#define GCREG_DE_PLANE2_STRIDE_FieldMask                              0x0003FFFF
#define GCREG_DE_PLANE2_STRIDE_ReadMask                               0x0003FFFC
#define GCREG_DE_PLANE2_STRIDE_WriteMask                              0x0003FFFC
#define GCREG_DE_PLANE2_STRIDE_ResetValue                             0x00000000

#define GCREG_DE_PLANE2_STRIDE_STRIDE                                     17 : 0
#define GCREG_DE_PLANE2_STRIDE_STRIDE_End                                     17
#define GCREG_DE_PLANE2_STRIDE_STRIDE_Start                                    0
#define GCREG_DE_PLANE2_STRIDE_STRIDE_Type                                   U18

/*******************************************************************************
** State gcregDEPlane3Address
*/

/* Address for plane 3 if gcregDEYUVConversion.
** Enable is set to Plane3.
*/

#define gcregDEPlane3AddressRegAddrs                                      0x04C6
#define GCREG_DE_PLANE3_ADDRESS_MSB                                           15
#define GCREG_DE_PLANE3_ADDRESS_LSB                                            0
#define GCREG_DE_PLANE3_ADDRESS_BLK                                            0
#define GCREG_DE_PLANE3_ADDRESS_Count                                          1
#define GCREG_DE_PLANE3_ADDRESS_FieldMask                             0xFFFFFFFF
#define GCREG_DE_PLANE3_ADDRESS_ReadMask                              0xFFFFFFFC
#define GCREG_DE_PLANE3_ADDRESS_WriteMask                             0xFFFFFFFC
#define GCREG_DE_PLANE3_ADDRESS_ResetValue                            0x00000000

#define GCREG_DE_PLANE3_ADDRESS_ADDRESS                                   31 : 0
#define GCREG_DE_PLANE3_ADDRESS_ADDRESS_End                                   30
#define GCREG_DE_PLANE3_ADDRESS_ADDRESS_Start                                  0
#define GCREG_DE_PLANE3_ADDRESS_ADDRESS_Type                                 U31

/*******************************************************************************
** State gcregDEPlane3Stride
*/

/* Stride for plane 3 if gcregDEYUVConversion.
** Enable is set to Plane3.
*/

#define gcregDEPlane3StrideRegAddrs                                       0x04C7
#define GCREG_DE_PLANE3_STRIDE_MSB                                            15
#define GCREG_DE_PLANE3_STRIDE_LSB                                             0
#define GCREG_DE_PLANE3_STRIDE_BLK                                             0
#define GCREG_DE_PLANE3_STRIDE_Count                                           1
#define GCREG_DE_PLANE3_STRIDE_FieldMask                              0x0003FFFF
#define GCREG_DE_PLANE3_STRIDE_ReadMask                               0x0003FFFC
#define GCREG_DE_PLANE3_STRIDE_WriteMask                              0x0003FFFC
#define GCREG_DE_PLANE3_STRIDE_ResetValue                             0x00000000

#define GCREG_DE_PLANE3_STRIDE_STRIDE                                     17 : 0
#define GCREG_DE_PLANE3_STRIDE_STRIDE_End                                     17
#define GCREG_DE_PLANE3_STRIDE_STRIDE_Start                                    0
#define GCREG_DE_PLANE3_STRIDE_STRIDE_Type                                   U18

/*******************************************************************************
** State gcregDEStallDE
*/

#define gcregDEStallDERegAddrs                                            0x04C8
#define GCREG_DE_STALL_DE_MSB                                                 15
#define GCREG_DE_STALL_DE_LSB                                                  0
#define GCREG_DE_STALL_DE_BLK                                                  0
#define GCREG_DE_STALL_DE_Count                                                1
#define GCREG_DE_STALL_DE_FieldMask                                   0x00000001
#define GCREG_DE_STALL_DE_ReadMask                                    0x00000001
#define GCREG_DE_STALL_DE_WriteMask                                   0x00000001
#define GCREG_DE_STALL_DE_ResetValue                                  0x00000000

/* Stall de enable. */
#define GCREG_DE_STALL_DE_ENABLE                                           0 : 0
#define GCREG_DE_STALL_DE_ENABLE_End                                           0
#define GCREG_DE_STALL_DE_ENABLE_Start                                         0
#define GCREG_DE_STALL_DE_ENABLE_Type                                        U01
#define   GCREG_DE_STALL_DE_ENABLE_DISABLED                                  0x0
#define   GCREG_DE_STALL_DE_ENABLE_ENABLED                                   0x1

/*******************************************************************************
** State gcregBlock4SrcAddress
*/

/* 32-bit aligned base address of the source surface. */

#define gcregBlock4SrcAddressRegAddrs                                     0x4A00
#define GCREG_BLOCK4_SRC_ADDRESS_MSB                                          15
#define GCREG_BLOCK4_SRC_ADDRESS_LSB                                           2
#define GCREG_BLOCK4_SRC_ADDRESS_BLK                                           0
#define GCREG_BLOCK4_SRC_ADDRESS_Count                                         4
#define GCREG_BLOCK4_SRC_ADDRESS_FieldMask                            0xFFFFFFFF
#define GCREG_BLOCK4_SRC_ADDRESS_ReadMask                             0xFFFFFFFC
#define GCREG_BLOCK4_SRC_ADDRESS_WriteMask                            0xFFFFFFFC
#define GCREG_BLOCK4_SRC_ADDRESS_ResetValue                           0x00000000

#define GCREG_BLOCK4_SRC_ADDRESS_ADDRESS                                  31 : 0
#define GCREG_BLOCK4_SRC_ADDRESS_ADDRESS_End                                  30
#define GCREG_BLOCK4_SRC_ADDRESS_ADDRESS_Start                                 0
#define GCREG_BLOCK4_SRC_ADDRESS_ADDRESS_Type                                U31

/*******************************************************************************
** State gcregBlock4SrcStride
*/

/* Stride of the source surface in bytes. To calculate the stride multiply
** the surface width in pixels by the number of  bytes per pixel.
*/

#define gcregBlock4SrcStrideRegAddrs                                      0x4A04
#define GCREG_BLOCK4_SRC_STRIDE_MSB                                           15
#define GCREG_BLOCK4_SRC_STRIDE_LSB                                            2
#define GCREG_BLOCK4_SRC_STRIDE_BLK                                            0
#define GCREG_BLOCK4_SRC_STRIDE_Count                                          4
#define GCREG_BLOCK4_SRC_STRIDE_FieldMask                             0x0003FFFF
#define GCREG_BLOCK4_SRC_STRIDE_ReadMask                              0x0003FFFC
#define GCREG_BLOCK4_SRC_STRIDE_WriteMask                             0x0003FFFC
#define GCREG_BLOCK4_SRC_STRIDE_ResetValue                            0x00000000

#define GCREG_BLOCK4_SRC_STRIDE_STRIDE                                    17 : 0
#define GCREG_BLOCK4_SRC_STRIDE_STRIDE_End                                    17
#define GCREG_BLOCK4_SRC_STRIDE_STRIDE_Start                                   0
#define GCREG_BLOCK4_SRC_STRIDE_STRIDE_Type                                  U18

/*******************************************************************************
** State gcregBlock4SrcRotationConfig
*/

/* 90 degree rotation configuration for the source surface. Width field
** specifies the width of the surface in pixels.
*/

#define gcregBlock4SrcRotationConfigRegAddrs                              0x4A08
#define GCREG_BLOCK4_SRC_ROTATION_CONFIG_MSB                                  15
#define GCREG_BLOCK4_SRC_ROTATION_CONFIG_LSB                                   2
#define GCREG_BLOCK4_SRC_ROTATION_CONFIG_BLK                                   0
#define GCREG_BLOCK4_SRC_ROTATION_CONFIG_Count                                 4
#define GCREG_BLOCK4_SRC_ROTATION_CONFIG_FieldMask                    0x0001FFFF
#define GCREG_BLOCK4_SRC_ROTATION_CONFIG_ReadMask                     0x0001FFFF
#define GCREG_BLOCK4_SRC_ROTATION_CONFIG_WriteMask                    0x0001FFFF
#define GCREG_BLOCK4_SRC_ROTATION_CONFIG_ResetValue                   0x00000000

#define GCREG_BLOCK4_SRC_ROTATION_CONFIG_WIDTH                            15 : 0
#define GCREG_BLOCK4_SRC_ROTATION_CONFIG_WIDTH_End                            15
#define GCREG_BLOCK4_SRC_ROTATION_CONFIG_WIDTH_Start                           0
#define GCREG_BLOCK4_SRC_ROTATION_CONFIG_WIDTH_Type                          U16

#define GCREG_BLOCK4_SRC_ROTATION_CONFIG_ROTATION                        16 : 16
#define GCREG_BLOCK4_SRC_ROTATION_CONFIG_ROTATION_End                         16
#define GCREG_BLOCK4_SRC_ROTATION_CONFIG_ROTATION_Start                       16
#define GCREG_BLOCK4_SRC_ROTATION_CONFIG_ROTATION_Type                       U01
#define   GCREG_BLOCK4_SRC_ROTATION_CONFIG_ROTATION_DISABLE                  0x0
#define   GCREG_BLOCK4_SRC_ROTATION_CONFIG_ROTATION_ENABLE                   0x1

/*******************************************************************************
** State gcregBlock4SrcConfig
*/

/* Source surface configuration register. */

#define gcregBlock4SrcConfigRegAddrs                                      0x4A0C
#define GCREG_BLOCK4_SRC_CONFIG_MSB                                           15
#define GCREG_BLOCK4_SRC_CONFIG_LSB                                            2
#define GCREG_BLOCK4_SRC_CONFIG_BLK                                            0
#define GCREG_BLOCK4_SRC_CONFIG_Count                                          4
#define GCREG_BLOCK4_SRC_CONFIG_FieldMask                             0xDF30B1C0
#define GCREG_BLOCK4_SRC_CONFIG_ReadMask                              0xDF30B1C0
#define GCREG_BLOCK4_SRC_CONFIG_WriteMask                             0xDF30B1C0
#define GCREG_BLOCK4_SRC_CONFIG_ResetValue                            0x00000000

/* Control source endianess. */
#define GCREG_BLOCK4_SRC_CONFIG_ENDIAN_CONTROL                           31 : 30
#define GCREG_BLOCK4_SRC_CONFIG_ENDIAN_CONTROL_End                            31
#define GCREG_BLOCK4_SRC_CONFIG_ENDIAN_CONTROL_Start                          30
#define GCREG_BLOCK4_SRC_CONFIG_ENDIAN_CONTROL_Type                          U02
#define   GCREG_BLOCK4_SRC_CONFIG_ENDIAN_CONTROL_NO_SWAP                     0x0
#define   GCREG_BLOCK4_SRC_CONFIG_ENDIAN_CONTROL_SWAP_WORD                   0x1
#define   GCREG_BLOCK4_SRC_CONFIG_ENDIAN_CONTROL_SWAP_DWORD                  0x2

/* Defines the pixel format of the source surface. */
#define GCREG_BLOCK4_SRC_CONFIG_SOURCE_FORMAT                            28 : 24
#define GCREG_BLOCK4_SRC_CONFIG_SOURCE_FORMAT_End                             28
#define GCREG_BLOCK4_SRC_CONFIG_SOURCE_FORMAT_Start                           24
#define GCREG_BLOCK4_SRC_CONFIG_SOURCE_FORMAT_Type                           U05
#define   GCREG_BLOCK4_SRC_CONFIG_SOURCE_FORMAT_X4R4G4B4                    0x00
#define   GCREG_BLOCK4_SRC_CONFIG_SOURCE_FORMAT_A4R4G4B4                    0x01
#define   GCREG_BLOCK4_SRC_CONFIG_SOURCE_FORMAT_X1R5G5B5                    0x02
#define   GCREG_BLOCK4_SRC_CONFIG_SOURCE_FORMAT_A1R5G5B5                    0x03
#define   GCREG_BLOCK4_SRC_CONFIG_SOURCE_FORMAT_R5G6B5                      0x04
#define   GCREG_BLOCK4_SRC_CONFIG_SOURCE_FORMAT_X8R8G8B8                    0x05
#define   GCREG_BLOCK4_SRC_CONFIG_SOURCE_FORMAT_A8R8G8B8                    0x06
#define   GCREG_BLOCK4_SRC_CONFIG_SOURCE_FORMAT_YUY2                        0x07
#define   GCREG_BLOCK4_SRC_CONFIG_SOURCE_FORMAT_UYVY                        0x08
#define   GCREG_BLOCK4_SRC_CONFIG_SOURCE_FORMAT_INDEX8                      0x09
#define   GCREG_BLOCK4_SRC_CONFIG_SOURCE_FORMAT_MONOCHROME                  0x0A
#define   GCREG_BLOCK4_SRC_CONFIG_SOURCE_FORMAT_YV12                        0x0F
#define   GCREG_BLOCK4_SRC_CONFIG_SOURCE_FORMAT_A8                          0x10
#define   GCREG_BLOCK4_SRC_CONFIG_SOURCE_FORMAT_NV12                        0x11
#define   GCREG_BLOCK4_SRC_CONFIG_SOURCE_FORMAT_NV16                        0x12
#define   GCREG_BLOCK4_SRC_CONFIG_SOURCE_FORMAT_RG16                        0x13

/* Color channel swizzles. */
#define GCREG_BLOCK4_SRC_CONFIG_SWIZZLE                                  21 : 20
#define GCREG_BLOCK4_SRC_CONFIG_SWIZZLE_End                                   21
#define GCREG_BLOCK4_SRC_CONFIG_SWIZZLE_Start                                 20
#define GCREG_BLOCK4_SRC_CONFIG_SWIZZLE_Type                                 U02
#define   GCREG_BLOCK4_SRC_CONFIG_SWIZZLE_ARGB                               0x0
#define   GCREG_BLOCK4_SRC_CONFIG_SWIZZLE_RGBA                               0x1
#define   GCREG_BLOCK4_SRC_CONFIG_SWIZZLE_ABGR                               0x2
#define   GCREG_BLOCK4_SRC_CONFIG_SWIZZLE_BGRA                               0x3

/* Mono expansion: if 0, transparency color will be 0, otherwise transparency **
** color will be 1.                                                           */
#define GCREG_BLOCK4_SRC_CONFIG_MONO_TRANSPARENCY                        15 : 15
#define GCREG_BLOCK4_SRC_CONFIG_MONO_TRANSPARENCY_End                         15
#define GCREG_BLOCK4_SRC_CONFIG_MONO_TRANSPARENCY_Start                       15
#define GCREG_BLOCK4_SRC_CONFIG_MONO_TRANSPARENCY_Type                       U01
#define   GCREG_BLOCK4_SRC_CONFIG_MONO_TRANSPARENCY_BACKGROUND               0x0
#define   GCREG_BLOCK4_SRC_CONFIG_MONO_TRANSPARENCY_FOREGROUND               0x1

/* Mono expansion or masked blit: stream packing in pixels. Determines how    **
** many horizontal pixels are there per each 32-bit chunk. For example, if    **
** set to Packed8, each 32-bit chunk is 8-pixel wide, which also means that   **
** it defines 4 vertical lines of pixels.                                     */
#define GCREG_BLOCK4_SRC_CONFIG_PACK                                     13 : 12
#define GCREG_BLOCK4_SRC_CONFIG_PACK_End                                      13
#define GCREG_BLOCK4_SRC_CONFIG_PACK_Start                                    12
#define GCREG_BLOCK4_SRC_CONFIG_PACK_Type                                    U02
#define   GCREG_BLOCK4_SRC_CONFIG_PACK_PACKED8                               0x0
#define   GCREG_BLOCK4_SRC_CONFIG_PACK_PACKED16                              0x1
#define   GCREG_BLOCK4_SRC_CONFIG_PACK_PACKED32                              0x2
#define   GCREG_BLOCK4_SRC_CONFIG_PACK_UNPACKED                              0x3

/* Source data location: set to STREAM for mono expansion blits or masked     **
** blits. For mono expansion blits the complete bitmap comes from the command **
** stream. For masked blits the source data comes from the memory and the     **
** mask from the command stream.                                              */
#define GCREG_BLOCK4_SRC_CONFIG_LOCATION                                   8 : 8
#define GCREG_BLOCK4_SRC_CONFIG_LOCATION_End                                   8
#define GCREG_BLOCK4_SRC_CONFIG_LOCATION_Start                                 8
#define GCREG_BLOCK4_SRC_CONFIG_LOCATION_Type                                U01
#define   GCREG_BLOCK4_SRC_CONFIG_LOCATION_MEMORY                            0x0
#define   GCREG_BLOCK4_SRC_CONFIG_LOCATION_STREAM                            0x1

/* Source linear/tiled address computation control. */
#define GCREG_BLOCK4_SRC_CONFIG_TILED                                      7 : 7
#define GCREG_BLOCK4_SRC_CONFIG_TILED_End                                      7
#define GCREG_BLOCK4_SRC_CONFIG_TILED_Start                                    7
#define GCREG_BLOCK4_SRC_CONFIG_TILED_Type                                   U01
#define   GCREG_BLOCK4_SRC_CONFIG_TILED_DISABLED                             0x0
#define   GCREG_BLOCK4_SRC_CONFIG_TILED_ENABLED                              0x1

/* If set to ABSOLUTE, the source coordinates are treated as absolute         **
** coordinates inside the source surface. If set to RELATIVE, the source      **
** coordinates are treated as the offsets from the destination coordinates    **
** with the source size equal to the size of the destination.                 */
#define GCREG_BLOCK4_SRC_CONFIG_SRC_RELATIVE                               6 : 6
#define GCREG_BLOCK4_SRC_CONFIG_SRC_RELATIVE_End                               6
#define GCREG_BLOCK4_SRC_CONFIG_SRC_RELATIVE_Start                             6
#define GCREG_BLOCK4_SRC_CONFIG_SRC_RELATIVE_Type                            U01
#define   GCREG_BLOCK4_SRC_CONFIG_SRC_RELATIVE_ABSOLUTE                      0x0
#define   GCREG_BLOCK4_SRC_CONFIG_SRC_RELATIVE_RELATIVE                      0x1

/*******************************************************************************
** State gcregBlock4SrcOrigin
*/

/* Absolute or relative (see SRC_RELATIVE field of gcregBlock4SrcConfig
** register) X and Y coordinates in pixels of the top left corner of the
** source rectangle within the source surface.
*/

#define gcregBlock4SrcOriginRegAddrs                                      0x4A10
#define GCREG_BLOCK4_SRC_ORIGIN_MSB                                           15
#define GCREG_BLOCK4_SRC_ORIGIN_LSB                                            2
#define GCREG_BLOCK4_SRC_ORIGIN_BLK                                            0
#define GCREG_BLOCK4_SRC_ORIGIN_Count                                          4
#define GCREG_BLOCK4_SRC_ORIGIN_FieldMask                             0xFFFFFFFF
#define GCREG_BLOCK4_SRC_ORIGIN_ReadMask                              0xFFFFFFFF
#define GCREG_BLOCK4_SRC_ORIGIN_WriteMask                             0xFFFFFFFF
#define GCREG_BLOCK4_SRC_ORIGIN_ResetValue                            0x00000000

#define GCREG_BLOCK4_SRC_ORIGIN_Y                                        31 : 16
#define GCREG_BLOCK4_SRC_ORIGIN_Y_End                                         31
#define GCREG_BLOCK4_SRC_ORIGIN_Y_Start                                       16
#define GCREG_BLOCK4_SRC_ORIGIN_Y_Type                                       U16

#define GCREG_BLOCK4_SRC_ORIGIN_X                                         15 : 0
#define GCREG_BLOCK4_SRC_ORIGIN_X_End                                         15
#define GCREG_BLOCK4_SRC_ORIGIN_X_Start                                        0
#define GCREG_BLOCK4_SRC_ORIGIN_X_Type                                       U16

/*******************************************************************************
** State gcregBlock4SrcSize
*/

/* Width and height of the source rectangle in pixels. If the source is
** relative (see SRC_RELATIVE field of gcregBlock4SrcConfig register) or a
** regular bitblt is being performed without stretching, this register is
** ignored and the source size is assumed to be the same as the destination.
*/

#define gcregBlock4SrcSizeRegAddrs                                        0x4A14
#define GCREG_BLOCK4_SRC_SIZE_MSB                                             15
#define GCREG_BLOCK4_SRC_SIZE_LSB                                              2
#define GCREG_BLOCK4_SRC_SIZE_BLK                                              0
#define GCREG_BLOCK4_SRC_SIZE_Count                                            4
#define GCREG_BLOCK4_SRC_SIZE_FieldMask                               0xFFFFFFFF
#define GCREG_BLOCK4_SRC_SIZE_ReadMask                                0xFFFFFFFF
#define GCREG_BLOCK4_SRC_SIZE_WriteMask                               0xFFFFFFFF
#define GCREG_BLOCK4_SRC_SIZE_ResetValue                              0x00000000

#define GCREG_BLOCK4_SRC_SIZE_Y                                          31 : 16
#define GCREG_BLOCK4_SRC_SIZE_Y_End                                           31
#define GCREG_BLOCK4_SRC_SIZE_Y_Start                                         16
#define GCREG_BLOCK4_SRC_SIZE_Y_Type                                         U16

#define GCREG_BLOCK4_SRC_SIZE_X                                           15 : 0
#define GCREG_BLOCK4_SRC_SIZE_X_End                                           15
#define GCREG_BLOCK4_SRC_SIZE_X_Start                                          0
#define GCREG_BLOCK4_SRC_SIZE_X_Type                                         U16

/*******************************************************************************
** State gcregBlock4SrcColorBg
*/

/* Select the color where source becomes transparent. It must be programmed
** in A8R8G8B8 format.
*/

#define gcregBlock4SrcColorBgRegAddrs                                     0x4A18
#define GCREG_BLOCK4_SRC_COLOR_BG_MSB                                         15
#define GCREG_BLOCK4_SRC_COLOR_BG_LSB                                          2
#define GCREG_BLOCK4_SRC_COLOR_BG_BLK                                          0
#define GCREG_BLOCK4_SRC_COLOR_BG_Count                                        4
#define GCREG_BLOCK4_SRC_COLOR_BG_FieldMask                           0xFFFFFFFF
#define GCREG_BLOCK4_SRC_COLOR_BG_ReadMask                            0xFFFFFFFF
#define GCREG_BLOCK4_SRC_COLOR_BG_WriteMask                           0xFFFFFFFF
#define GCREG_BLOCK4_SRC_COLOR_BG_ResetValue                          0x00000000

#define GCREG_BLOCK4_SRC_COLOR_BG_ALPHA                                  31 : 24
#define GCREG_BLOCK4_SRC_COLOR_BG_ALPHA_End                                   31
#define GCREG_BLOCK4_SRC_COLOR_BG_ALPHA_Start                                 24
#define GCREG_BLOCK4_SRC_COLOR_BG_ALPHA_Type                                 U08

#define GCREG_BLOCK4_SRC_COLOR_BG_RED                                    23 : 16
#define GCREG_BLOCK4_SRC_COLOR_BG_RED_End                                     23
#define GCREG_BLOCK4_SRC_COLOR_BG_RED_Start                                   16
#define GCREG_BLOCK4_SRC_COLOR_BG_RED_Type                                   U08

#define GCREG_BLOCK4_SRC_COLOR_BG_GREEN                                   15 : 8
#define GCREG_BLOCK4_SRC_COLOR_BG_GREEN_End                                   15
#define GCREG_BLOCK4_SRC_COLOR_BG_GREEN_Start                                  8
#define GCREG_BLOCK4_SRC_COLOR_BG_GREEN_Type                                 U08

#define GCREG_BLOCK4_SRC_COLOR_BG_BLUE                                     7 : 0
#define GCREG_BLOCK4_SRC_COLOR_BG_BLUE_End                                     7
#define GCREG_BLOCK4_SRC_COLOR_BG_BLUE_Start                                   0
#define GCREG_BLOCK4_SRC_COLOR_BG_BLUE_Type                                  U08

/*******************************************************************************
** State gcregBlock4Rop
*/

/* Raster operation foreground and background codes. Even though ROP is not
** used in CLEAR, HOR_FILTER_BLT, VER_FILTER_BLT and alpha-eanbled BIT_BLTs,
** ROP code still has to be programmed, because the engine makes the decision
** whether source, destination and pattern are involved in the current
** operation and the correct decision is essential for the engine to complete
** the operation as expected.
*/

#define gcregBlock4RopRegAddrs                                            0x4A1C
#define GCREG_BLOCK4_ROP_MSB                                                  15
#define GCREG_BLOCK4_ROP_LSB                                                   2
#define GCREG_BLOCK4_ROP_BLK                                                   0
#define GCREG_BLOCK4_ROP_Count                                                 4
#define GCREG_BLOCK4_ROP_FieldMask                                    0x0030FFFF
#define GCREG_BLOCK4_ROP_ReadMask                                     0x0030FFFF
#define GCREG_BLOCK4_ROP_WriteMask                                    0x0030FFFF
#define GCREG_BLOCK4_ROP_ResetValue                                   0x00000000

/* ROP type: ROP2, ROP3 or ROP4 */
#define GCREG_BLOCK4_ROP_TYPE                                            21 : 20
#define GCREG_BLOCK4_ROP_TYPE_End                                             21
#define GCREG_BLOCK4_ROP_TYPE_Start                                           20
#define GCREG_BLOCK4_ROP_TYPE_Type                                           U02
#define   GCREG_BLOCK4_ROP_TYPE_ROP2_PATTERN                                 0x0
#define   GCREG_BLOCK4_ROP_TYPE_ROP2_SOURCE                                  0x1
#define   GCREG_BLOCK4_ROP_TYPE_ROP3                                         0x2
#define   GCREG_BLOCK4_ROP_TYPE_ROP4                                         0x3

/* Background ROP code is used for transparent pixels. */
#define GCREG_BLOCK4_ROP_ROP_BG                                           15 : 8
#define GCREG_BLOCK4_ROP_ROP_BG_End                                           15
#define GCREG_BLOCK4_ROP_ROP_BG_Start                                          8
#define GCREG_BLOCK4_ROP_ROP_BG_Type                                         U08

/* Background ROP code is used for opaque pixels. */
#define GCREG_BLOCK4_ROP_ROP_FG                                            7 : 0
#define GCREG_BLOCK4_ROP_ROP_FG_End                                            7
#define GCREG_BLOCK4_ROP_ROP_FG_Start                                          0
#define GCREG_BLOCK4_ROP_ROP_FG_Type                                         U08

/*******************************************************************************
** State gcregBlock4AlphaControl
*/

#define gcregBlock4AlphaControlRegAddrs                                   0x4A20
#define GCREG_BLOCK4_ALPHA_CONTROL_MSB                                        15
#define GCREG_BLOCK4_ALPHA_CONTROL_LSB                                         2
#define GCREG_BLOCK4_ALPHA_CONTROL_BLK                                         0
#define GCREG_BLOCK4_ALPHA_CONTROL_Count                                       4
#define GCREG_BLOCK4_ALPHA_CONTROL_FieldMask                          0x00000001
#define GCREG_BLOCK4_ALPHA_CONTROL_ReadMask                           0x00000001
#define GCREG_BLOCK4_ALPHA_CONTROL_WriteMask                          0x00000001
#define GCREG_BLOCK4_ALPHA_CONTROL_ResetValue                         0x00000000

#define GCREG_BLOCK4_ALPHA_CONTROL_ENABLE                                  0 : 0
#define GCREG_BLOCK4_ALPHA_CONTROL_ENABLE_End                                  0
#define GCREG_BLOCK4_ALPHA_CONTROL_ENABLE_Start                                0
#define GCREG_BLOCK4_ALPHA_CONTROL_ENABLE_Type                               U01
#define   GCREG_BLOCK4_ALPHA_CONTROL_ENABLE_OFF                              0x0
#define   GCREG_BLOCK4_ALPHA_CONTROL_ENABLE_ON                               0x1

/*******************************************************************************
** State gcregBlock4AlphaModes
*/

#define gcregBlock4AlphaModesRegAddrs                                     0x4A24
#define GCREG_BLOCK4_ALPHA_MODES_MSB                                          15
#define GCREG_BLOCK4_ALPHA_MODES_LSB                                           2
#define GCREG_BLOCK4_ALPHA_MODES_BLK                                           0
#define GCREG_BLOCK4_ALPHA_MODES_Count                                         4
#define GCREG_BLOCK4_ALPHA_MODES_FieldMask                            0xFF003311
#define GCREG_BLOCK4_ALPHA_MODES_ReadMask                             0xFF003311
#define GCREG_BLOCK4_ALPHA_MODES_WriteMask                            0xFF003311
#define GCREG_BLOCK4_ALPHA_MODES_ResetValue                           0x00000000

#define GCREG_BLOCK4_ALPHA_MODES_SRC_ALPHA_MODE                            0 : 0
#define GCREG_BLOCK4_ALPHA_MODES_SRC_ALPHA_MODE_End                            0
#define GCREG_BLOCK4_ALPHA_MODES_SRC_ALPHA_MODE_Start                          0
#define GCREG_BLOCK4_ALPHA_MODES_SRC_ALPHA_MODE_Type                         U01
#define   GCREG_BLOCK4_ALPHA_MODES_SRC_ALPHA_MODE_NORMAL                     0x0
#define   GCREG_BLOCK4_ALPHA_MODES_SRC_ALPHA_MODE_INVERSED                   0x1

#define GCREG_BLOCK4_ALPHA_MODES_DST_ALPHA_MODE                            4 : 4
#define GCREG_BLOCK4_ALPHA_MODES_DST_ALPHA_MODE_End                            4
#define GCREG_BLOCK4_ALPHA_MODES_DST_ALPHA_MODE_Start                          4
#define GCREG_BLOCK4_ALPHA_MODES_DST_ALPHA_MODE_Type                         U01
#define   GCREG_BLOCK4_ALPHA_MODES_DST_ALPHA_MODE_NORMAL                     0x0
#define   GCREG_BLOCK4_ALPHA_MODES_DST_ALPHA_MODE_INVERSED                   0x1

#define GCREG_BLOCK4_ALPHA_MODES_GLOBAL_SRC_ALPHA_MODE                     9 : 8
#define GCREG_BLOCK4_ALPHA_MODES_GLOBAL_SRC_ALPHA_MODE_End                     9
#define GCREG_BLOCK4_ALPHA_MODES_GLOBAL_SRC_ALPHA_MODE_Start                   8
#define GCREG_BLOCK4_ALPHA_MODES_GLOBAL_SRC_ALPHA_MODE_Type                  U02
#define   GCREG_BLOCK4_ALPHA_MODES_GLOBAL_SRC_ALPHA_MODE_NORMAL              0x0
#define   GCREG_BLOCK4_ALPHA_MODES_GLOBAL_SRC_ALPHA_MODE_GLOBAL              0x1
#define   GCREG_BLOCK4_ALPHA_MODES_GLOBAL_SRC_ALPHA_MODE_SCALED              0x2

#define GCREG_BLOCK4_ALPHA_MODES_GLOBAL_DST_ALPHA_MODE                   13 : 12
#define GCREG_BLOCK4_ALPHA_MODES_GLOBAL_DST_ALPHA_MODE_End                    13
#define GCREG_BLOCK4_ALPHA_MODES_GLOBAL_DST_ALPHA_MODE_Start                  12
#define GCREG_BLOCK4_ALPHA_MODES_GLOBAL_DST_ALPHA_MODE_Type                  U02
#define   GCREG_BLOCK4_ALPHA_MODES_GLOBAL_DST_ALPHA_MODE_NORMAL              0x0
#define   GCREG_BLOCK4_ALPHA_MODES_GLOBAL_DST_ALPHA_MODE_GLOBAL              0x1
#define   GCREG_BLOCK4_ALPHA_MODES_GLOBAL_DST_ALPHA_MODE_SCALED              0x2

#define GCREG_BLOCK4_ALPHA_MODES_SRC_BLENDING_MODE                       26 : 24
#define GCREG_BLOCK4_ALPHA_MODES_SRC_BLENDING_MODE_End                        26
#define GCREG_BLOCK4_ALPHA_MODES_SRC_BLENDING_MODE_Start                      24
#define GCREG_BLOCK4_ALPHA_MODES_SRC_BLENDING_MODE_Type                      U03
#define   GCREG_BLOCK4_ALPHA_MODES_SRC_BLENDING_MODE_ZERO                    0x0
#define   GCREG_BLOCK4_ALPHA_MODES_SRC_BLENDING_MODE_ONE                     0x1
#define   GCREG_BLOCK4_ALPHA_MODES_SRC_BLENDING_MODE_NORMAL                  0x2
#define   GCREG_BLOCK4_ALPHA_MODES_SRC_BLENDING_MODE_INVERSED                0x3
#define   GCREG_BLOCK4_ALPHA_MODES_SRC_BLENDING_MODE_COLOR                   0x4
#define   GCREG_BLOCK4_ALPHA_MODES_SRC_BLENDING_MODE_COLOR_INVERSED          0x5
#define   GCREG_BLOCK4_ALPHA_MODES_SRC_BLENDING_MODE_SATURATED_ALPHA         0x6
#define   GCREG_BLOCK4_ALPHA_MODES_SRC_BLENDING_MODE_SATURATED_DEST_ALPHA    0x7

/* Src Blending factor is calculate from Src alpha. */
#define GCREG_BLOCK4_ALPHA_MODES_SRC_ALPHA_FACTOR                        27 : 27
#define GCREG_BLOCK4_ALPHA_MODES_SRC_ALPHA_FACTOR_End                         27
#define GCREG_BLOCK4_ALPHA_MODES_SRC_ALPHA_FACTOR_Start                       27
#define GCREG_BLOCK4_ALPHA_MODES_SRC_ALPHA_FACTOR_Type                       U01
#define   GCREG_BLOCK4_ALPHA_MODES_SRC_ALPHA_FACTOR_DISABLED                 0x0
#define   GCREG_BLOCK4_ALPHA_MODES_SRC_ALPHA_FACTOR_ENABLED                  0x1

#define GCREG_BLOCK4_ALPHA_MODES_DST_BLENDING_MODE                       30 : 28
#define GCREG_BLOCK4_ALPHA_MODES_DST_BLENDING_MODE_End                        30
#define GCREG_BLOCK4_ALPHA_MODES_DST_BLENDING_MODE_Start                      28
#define GCREG_BLOCK4_ALPHA_MODES_DST_BLENDING_MODE_Type                      U03
#define   GCREG_BLOCK4_ALPHA_MODES_DST_BLENDING_MODE_ZERO                    0x0
#define   GCREG_BLOCK4_ALPHA_MODES_DST_BLENDING_MODE_ONE                     0x1
#define   GCREG_BLOCK4_ALPHA_MODES_DST_BLENDING_MODE_NORMAL                  0x2
#define   GCREG_BLOCK4_ALPHA_MODES_DST_BLENDING_MODE_INVERSED                0x3
#define   GCREG_BLOCK4_ALPHA_MODES_DST_BLENDING_MODE_COLOR                   0x4
#define   GCREG_BLOCK4_ALPHA_MODES_DST_BLENDING_MODE_COLOR_INVERSED          0x5
#define   GCREG_BLOCK4_ALPHA_MODES_DST_BLENDING_MODE_SATURATED_ALPHA         0x6
#define   GCREG_BLOCK4_ALPHA_MODES_DST_BLENDING_MODE_SATURATED_DEST_ALPHA    0x7

/* Dst Blending factor is calculate from Dst alpha. */
#define GCREG_BLOCK4_ALPHA_MODES_DST_ALPHA_FACTOR                        31 : 31
#define GCREG_BLOCK4_ALPHA_MODES_DST_ALPHA_FACTOR_End                         31
#define GCREG_BLOCK4_ALPHA_MODES_DST_ALPHA_FACTOR_Start                       31
#define GCREG_BLOCK4_ALPHA_MODES_DST_ALPHA_FACTOR_Type                       U01
#define   GCREG_BLOCK4_ALPHA_MODES_DST_ALPHA_FACTOR_DISABLED                 0x0
#define   GCREG_BLOCK4_ALPHA_MODES_DST_ALPHA_FACTOR_ENABLED                  0x1

/*******************************************************************************
** State gcregBlock4AddressU
*/

/* 32-bit aligned base address of the source U plane. */

#define gcregBlock4AddressURegAddrs                                       0x4A28
#define GCREG_BLOCK4_ADDRESS_U_MSB                                            15
#define GCREG_BLOCK4_ADDRESS_U_LSB                                             2
#define GCREG_BLOCK4_ADDRESS_U_BLK                                             0
#define GCREG_BLOCK4_ADDRESS_U_Count                                           4
#define GCREG_BLOCK4_ADDRESS_U_FieldMask                              0xFFFFFFFF
#define GCREG_BLOCK4_ADDRESS_U_ReadMask                               0xFFFFFFFC
#define GCREG_BLOCK4_ADDRESS_U_WriteMask                              0xFFFFFFFC
#define GCREG_BLOCK4_ADDRESS_U_ResetValue                             0x00000000

#define GCREG_BLOCK4_ADDRESS_U_ADDRESS                                    31 : 0
#define GCREG_BLOCK4_ADDRESS_U_ADDRESS_End                                    30
#define GCREG_BLOCK4_ADDRESS_U_ADDRESS_Start                                   0
#define GCREG_BLOCK4_ADDRESS_U_ADDRESS_Type                                  U31

/*******************************************************************************
** State gcregBlock4StrideU
*/

/* Stride of the source U plane in bytes. */

#define gcregBlock4StrideURegAddrs                                        0x4A2C
#define GCREG_BLOCK4_STRIDE_U_MSB                                             15
#define GCREG_BLOCK4_STRIDE_U_LSB                                              2
#define GCREG_BLOCK4_STRIDE_U_BLK                                              0
#define GCREG_BLOCK4_STRIDE_U_Count                                            4
#define GCREG_BLOCK4_STRIDE_U_FieldMask                               0x0003FFFF
#define GCREG_BLOCK4_STRIDE_U_ReadMask                                0x0003FFFC
#define GCREG_BLOCK4_STRIDE_U_WriteMask                               0x0003FFFC
#define GCREG_BLOCK4_STRIDE_U_ResetValue                              0x00000000

#define GCREG_BLOCK4_STRIDE_U_STRIDE                                      17 : 0
#define GCREG_BLOCK4_STRIDE_U_STRIDE_End                                      17
#define GCREG_BLOCK4_STRIDE_U_STRIDE_Start                                     0
#define GCREG_BLOCK4_STRIDE_U_STRIDE_Type                                    U18

/*******************************************************************************
** State gcregBlock4AddressV
*/

/* 32-bit aligned base address of the source V plane. */

#define gcregBlock4AddressVRegAddrs                                       0x4A30
#define GCREG_BLOCK4_ADDRESS_V_MSB                                            15
#define GCREG_BLOCK4_ADDRESS_V_LSB                                             2
#define GCREG_BLOCK4_ADDRESS_V_BLK                                             0
#define GCREG_BLOCK4_ADDRESS_V_Count                                           4
#define GCREG_BLOCK4_ADDRESS_V_FieldMask                              0xFFFFFFFF
#define GCREG_BLOCK4_ADDRESS_V_ReadMask                               0xFFFFFFFC
#define GCREG_BLOCK4_ADDRESS_V_WriteMask                              0xFFFFFFFC
#define GCREG_BLOCK4_ADDRESS_V_ResetValue                             0x00000000

#define GCREG_BLOCK4_ADDRESS_V_ADDRESS                                    31 : 0
#define GCREG_BLOCK4_ADDRESS_V_ADDRESS_End                                    30
#define GCREG_BLOCK4_ADDRESS_V_ADDRESS_Start                                   0
#define GCREG_BLOCK4_ADDRESS_V_ADDRESS_Type                                  U31

/*******************************************************************************
** State gcregBlock4StrideV
*/

/* Stride of the source V plane in bytes. */

#define gcregBlock4StrideVRegAddrs                                        0x4A34
#define GCREG_BLOCK4_STRIDE_V_MSB                                             15
#define GCREG_BLOCK4_STRIDE_V_LSB                                              2
#define GCREG_BLOCK4_STRIDE_V_BLK                                              0
#define GCREG_BLOCK4_STRIDE_V_Count                                            4
#define GCREG_BLOCK4_STRIDE_V_FieldMask                               0x0003FFFF
#define GCREG_BLOCK4_STRIDE_V_ReadMask                                0x0003FFFC
#define GCREG_BLOCK4_STRIDE_V_WriteMask                               0x0003FFFC
#define GCREG_BLOCK4_STRIDE_V_ResetValue                              0x00000000

#define GCREG_BLOCK4_STRIDE_V_STRIDE                                      17 : 0
#define GCREG_BLOCK4_STRIDE_V_STRIDE_End                                      17
#define GCREG_BLOCK4_STRIDE_V_STRIDE_Start                                     0
#define GCREG_BLOCK4_STRIDE_V_STRIDE_Type                                    U18

/*******************************************************************************
** State gcregBlock4SrcRotationHeight
*/

/* 180/270 degree rotation configuration for the Source surface. Height field
** specifies the height of the surface in pixels.
*/

#define gcregBlock4SrcRotationHeightRegAddrs                              0x4A38
#define GCREG_BLOCK4_SRC_ROTATION_HEIGHT_MSB                                  15
#define GCREG_BLOCK4_SRC_ROTATION_HEIGHT_LSB                                   2
#define GCREG_BLOCK4_SRC_ROTATION_HEIGHT_BLK                                   0
#define GCREG_BLOCK4_SRC_ROTATION_HEIGHT_Count                                 4
#define GCREG_BLOCK4_SRC_ROTATION_HEIGHT_FieldMask                    0x0000FFFF
#define GCREG_BLOCK4_SRC_ROTATION_HEIGHT_ReadMask                     0x0000FFFF
#define GCREG_BLOCK4_SRC_ROTATION_HEIGHT_WriteMask                    0x0000FFFF
#define GCREG_BLOCK4_SRC_ROTATION_HEIGHT_ResetValue                   0x00000000

#define GCREG_BLOCK4_SRC_ROTATION_HEIGHT_HEIGHT                           15 : 0
#define GCREG_BLOCK4_SRC_ROTATION_HEIGHT_HEIGHT_End                           15
#define GCREG_BLOCK4_SRC_ROTATION_HEIGHT_HEIGHT_Start                          0
#define GCREG_BLOCK4_SRC_ROTATION_HEIGHT_HEIGHT_Type                         U16

/*******************************************************************************
** State gcregBlock4RotAngle
*/

/* 0/90/180/270 degree rotation configuration for the Source surface. Height
** field specifies the height of the surface in pixels.
*/

#define gcregBlock4RotAngleRegAddrs                                       0x4A3C
#define GCREG_BLOCK4_ROT_ANGLE_MSB                                            15
#define GCREG_BLOCK4_ROT_ANGLE_LSB                                             2
#define GCREG_BLOCK4_ROT_ANGLE_BLK                                             0
#define GCREG_BLOCK4_ROT_ANGLE_Count                                           4
#define GCREG_BLOCK4_ROT_ANGLE_FieldMask                              0x000BB33F
#define GCREG_BLOCK4_ROT_ANGLE_ReadMask                               0x000BB33F
#define GCREG_BLOCK4_ROT_ANGLE_WriteMask                              0x000BB33F
#define GCREG_BLOCK4_ROT_ANGLE_ResetValue                             0x00000000

#define GCREG_BLOCK4_ROT_ANGLE_SRC                                         2 : 0
#define GCREG_BLOCK4_ROT_ANGLE_SRC_End                                         2
#define GCREG_BLOCK4_ROT_ANGLE_SRC_Start                                       0
#define GCREG_BLOCK4_ROT_ANGLE_SRC_Type                                      U03
#define   GCREG_BLOCK4_ROT_ANGLE_SRC_ROT0                                    0x0
#define   GCREG_BLOCK4_ROT_ANGLE_SRC_FLIP_X                                  0x1
#define   GCREG_BLOCK4_ROT_ANGLE_SRC_FLIP_Y                                  0x2
#define   GCREG_BLOCK4_ROT_ANGLE_SRC_ROT90                                   0x4
#define   GCREG_BLOCK4_ROT_ANGLE_SRC_ROT180                                  0x5
#define   GCREG_BLOCK4_ROT_ANGLE_SRC_ROT270                                  0x6

#define GCREG_BLOCK4_ROT_ANGLE_DST                                         5 : 3
#define GCREG_BLOCK4_ROT_ANGLE_DST_End                                         5
#define GCREG_BLOCK4_ROT_ANGLE_DST_Start                                       3
#define GCREG_BLOCK4_ROT_ANGLE_DST_Type                                      U03
#define   GCREG_BLOCK4_ROT_ANGLE_DST_ROT0                                    0x0
#define   GCREG_BLOCK4_ROT_ANGLE_DST_FLIP_X                                  0x1
#define   GCREG_BLOCK4_ROT_ANGLE_DST_FLIP_Y                                  0x2
#define   GCREG_BLOCK4_ROT_ANGLE_DST_ROT90                                   0x4
#define   GCREG_BLOCK4_ROT_ANGLE_DST_ROT180                                  0x5
#define   GCREG_BLOCK4_ROT_ANGLE_DST_ROT270                                  0x6

#define GCREG_BLOCK4_ROT_ANGLE_MASK_SRC                                    8 : 8
#define GCREG_BLOCK4_ROT_ANGLE_MASK_SRC_End                                    8
#define GCREG_BLOCK4_ROT_ANGLE_MASK_SRC_Start                                  8
#define GCREG_BLOCK4_ROT_ANGLE_MASK_SRC_Type                                 U01
#define   GCREG_BLOCK4_ROT_ANGLE_MASK_SRC_ENABLED                            0x0
#define   GCREG_BLOCK4_ROT_ANGLE_MASK_SRC_MASKED                             0x1

#define GCREG_BLOCK4_ROT_ANGLE_MASK_DST                                    9 : 9
#define GCREG_BLOCK4_ROT_ANGLE_MASK_DST_End                                    9
#define GCREG_BLOCK4_ROT_ANGLE_MASK_DST_Start                                  9
#define GCREG_BLOCK4_ROT_ANGLE_MASK_DST_Type                                 U01
#define   GCREG_BLOCK4_ROT_ANGLE_MASK_DST_ENABLED                            0x0
#define   GCREG_BLOCK4_ROT_ANGLE_MASK_DST_MASKED                             0x1

#define GCREG_BLOCK4_ROT_ANGLE_SRC_MIRROR                                13 : 12
#define GCREG_BLOCK4_ROT_ANGLE_SRC_MIRROR_End                                 13
#define GCREG_BLOCK4_ROT_ANGLE_SRC_MIRROR_Start                               12
#define GCREG_BLOCK4_ROT_ANGLE_SRC_MIRROR_Type                               U02
#define   GCREG_BLOCK4_ROT_ANGLE_SRC_MIRROR_NONE                             0x0
#define   GCREG_BLOCK4_ROT_ANGLE_SRC_MIRROR_MIRROR_X                         0x1
#define   GCREG_BLOCK4_ROT_ANGLE_SRC_MIRROR_MIRROR_Y                         0x2
#define   GCREG_BLOCK4_ROT_ANGLE_SRC_MIRROR_MIRROR_XY                        0x3

#define GCREG_BLOCK4_ROT_ANGLE_MASK_SRC_MIRROR                           15 : 15
#define GCREG_BLOCK4_ROT_ANGLE_MASK_SRC_MIRROR_End                            15
#define GCREG_BLOCK4_ROT_ANGLE_MASK_SRC_MIRROR_Start                          15
#define GCREG_BLOCK4_ROT_ANGLE_MASK_SRC_MIRROR_Type                          U01
#define   GCREG_BLOCK4_ROT_ANGLE_MASK_SRC_MIRROR_ENABLED                     0x0
#define   GCREG_BLOCK4_ROT_ANGLE_MASK_SRC_MIRROR_MASKED                      0x1

#define GCREG_BLOCK4_ROT_ANGLE_DST_MIRROR                                17 : 16
#define GCREG_BLOCK4_ROT_ANGLE_DST_MIRROR_End                                 17
#define GCREG_BLOCK4_ROT_ANGLE_DST_MIRROR_Start                               16
#define GCREG_BLOCK4_ROT_ANGLE_DST_MIRROR_Type                               U02
#define   GCREG_BLOCK4_ROT_ANGLE_DST_MIRROR_NONE                             0x0
#define   GCREG_BLOCK4_ROT_ANGLE_DST_MIRROR_MIRROR_X                         0x1
#define   GCREG_BLOCK4_ROT_ANGLE_DST_MIRROR_MIRROR_Y                         0x2
#define   GCREG_BLOCK4_ROT_ANGLE_DST_MIRROR_MIRROR_XY                        0x3

#define GCREG_BLOCK4_ROT_ANGLE_MASK_DST_MIRROR                           19 : 19
#define GCREG_BLOCK4_ROT_ANGLE_MASK_DST_MIRROR_End                            19
#define GCREG_BLOCK4_ROT_ANGLE_MASK_DST_MIRROR_Start                          19
#define GCREG_BLOCK4_ROT_ANGLE_MASK_DST_MIRROR_Type                          U01
#define   GCREG_BLOCK4_ROT_ANGLE_MASK_DST_MIRROR_ENABLED                     0x0
#define   GCREG_BLOCK4_ROT_ANGLE_MASK_DST_MIRROR_MASKED                      0x1

/*******************************************************************************
** State gcregBlock4GlobalSrcColor
*/

/* Defines the global source color and alpha values. */

#define gcregBlock4GlobalSrcColorRegAddrs                                 0x4A40
#define GCREG_BLOCK4_GLOBAL_SRC_COLOR_MSB                                     15
#define GCREG_BLOCK4_GLOBAL_SRC_COLOR_LSB                                      2
#define GCREG_BLOCK4_GLOBAL_SRC_COLOR_BLK                                      0
#define GCREG_BLOCK4_GLOBAL_SRC_COLOR_Count                                    4
#define GCREG_BLOCK4_GLOBAL_SRC_COLOR_FieldMask                       0xFFFFFFFF
#define GCREG_BLOCK4_GLOBAL_SRC_COLOR_ReadMask                        0xFFFFFFFF
#define GCREG_BLOCK4_GLOBAL_SRC_COLOR_WriteMask                       0xFFFFFFFF
#define GCREG_BLOCK4_GLOBAL_SRC_COLOR_ResetValue                      0x00000000

#define GCREG_BLOCK4_GLOBAL_SRC_COLOR_ALPHA                              31 : 24
#define GCREG_BLOCK4_GLOBAL_SRC_COLOR_ALPHA_End                               31
#define GCREG_BLOCK4_GLOBAL_SRC_COLOR_ALPHA_Start                             24
#define GCREG_BLOCK4_GLOBAL_SRC_COLOR_ALPHA_Type                             U08

#define GCREG_BLOCK4_GLOBAL_SRC_COLOR_RED                                23 : 16
#define GCREG_BLOCK4_GLOBAL_SRC_COLOR_RED_End                                 23
#define GCREG_BLOCK4_GLOBAL_SRC_COLOR_RED_Start                               16
#define GCREG_BLOCK4_GLOBAL_SRC_COLOR_RED_Type                               U08

#define GCREG_BLOCK4_GLOBAL_SRC_COLOR_GREEN                               15 : 8
#define GCREG_BLOCK4_GLOBAL_SRC_COLOR_GREEN_End                               15
#define GCREG_BLOCK4_GLOBAL_SRC_COLOR_GREEN_Start                              8
#define GCREG_BLOCK4_GLOBAL_SRC_COLOR_GREEN_Type                             U08

#define GCREG_BLOCK4_GLOBAL_SRC_COLOR_BLUE                                 7 : 0
#define GCREG_BLOCK4_GLOBAL_SRC_COLOR_BLUE_End                                 7
#define GCREG_BLOCK4_GLOBAL_SRC_COLOR_BLUE_Start                               0
#define GCREG_BLOCK4_GLOBAL_SRC_COLOR_BLUE_Type                              U08

/*******************************************************************************
** State gcregBlock4GlobalDestColor
*/

/* Defines the global destination color and alpha values. */

#define gcregBlock4GlobalDestColorRegAddrs                                0x4A44
#define GCREG_BLOCK4_GLOBAL_DEST_COLOR_MSB                                    15
#define GCREG_BLOCK4_GLOBAL_DEST_COLOR_LSB                                     2
#define GCREG_BLOCK4_GLOBAL_DEST_COLOR_BLK                                     0
#define GCREG_BLOCK4_GLOBAL_DEST_COLOR_Count                                   4
#define GCREG_BLOCK4_GLOBAL_DEST_COLOR_FieldMask                      0xFFFFFFFF
#define GCREG_BLOCK4_GLOBAL_DEST_COLOR_ReadMask                       0xFFFFFFFF
#define GCREG_BLOCK4_GLOBAL_DEST_COLOR_WriteMask                      0xFFFFFFFF
#define GCREG_BLOCK4_GLOBAL_DEST_COLOR_ResetValue                     0x00000000

#define GCREG_BLOCK4_GLOBAL_DEST_COLOR_ALPHA                             31 : 24
#define GCREG_BLOCK4_GLOBAL_DEST_COLOR_ALPHA_End                              31
#define GCREG_BLOCK4_GLOBAL_DEST_COLOR_ALPHA_Start                            24
#define GCREG_BLOCK4_GLOBAL_DEST_COLOR_ALPHA_Type                            U08

#define GCREG_BLOCK4_GLOBAL_DEST_COLOR_RED                               23 : 16
#define GCREG_BLOCK4_GLOBAL_DEST_COLOR_RED_End                                23
#define GCREG_BLOCK4_GLOBAL_DEST_COLOR_RED_Start                              16
#define GCREG_BLOCK4_GLOBAL_DEST_COLOR_RED_Type                              U08

#define GCREG_BLOCK4_GLOBAL_DEST_COLOR_GREEN                              15 : 8
#define GCREG_BLOCK4_GLOBAL_DEST_COLOR_GREEN_End                              15
#define GCREG_BLOCK4_GLOBAL_DEST_COLOR_GREEN_Start                             8
#define GCREG_BLOCK4_GLOBAL_DEST_COLOR_GREEN_Type                            U08

#define GCREG_BLOCK4_GLOBAL_DEST_COLOR_BLUE                                7 : 0
#define GCREG_BLOCK4_GLOBAL_DEST_COLOR_BLUE_End                                7
#define GCREG_BLOCK4_GLOBAL_DEST_COLOR_BLUE_Start                              0
#define GCREG_BLOCK4_GLOBAL_DEST_COLOR_BLUE_Type                             U08

/*******************************************************************************
** State gcregBlock4ColorMultiplyModes
*/

/* Color modes to multiply Source or Destination pixel color by alpha
** channel. Alpha can be from global color source or current pixel.
*/

#define gcregBlock4ColorMultiplyModesRegAddrs                             0x4A48
#define GCREG_BLOCK4_COLOR_MULTIPLY_MODES_MSB                                 15
#define GCREG_BLOCK4_COLOR_MULTIPLY_MODES_LSB                                  2
#define GCREG_BLOCK4_COLOR_MULTIPLY_MODES_BLK                                  0
#define GCREG_BLOCK4_COLOR_MULTIPLY_MODES_Count                                4
#define GCREG_BLOCK4_COLOR_MULTIPLY_MODES_FieldMask                   0x00100311
#define GCREG_BLOCK4_COLOR_MULTIPLY_MODES_ReadMask                    0x00100311
#define GCREG_BLOCK4_COLOR_MULTIPLY_MODES_WriteMask                   0x00100311
#define GCREG_BLOCK4_COLOR_MULTIPLY_MODES_ResetValue                  0x00000000

#define GCREG_BLOCK4_COLOR_MULTIPLY_MODES_SRC_PREMULTIPLY                  0 : 0
#define GCREG_BLOCK4_COLOR_MULTIPLY_MODES_SRC_PREMULTIPLY_End                  0
#define GCREG_BLOCK4_COLOR_MULTIPLY_MODES_SRC_PREMULTIPLY_Start                0
#define GCREG_BLOCK4_COLOR_MULTIPLY_MODES_SRC_PREMULTIPLY_Type               U01
#define   GCREG_BLOCK4_COLOR_MULTIPLY_MODES_SRC_PREMULTIPLY_DISABLE          0x0
#define   GCREG_BLOCK4_COLOR_MULTIPLY_MODES_SRC_PREMULTIPLY_ENABLE           0x1

#define GCREG_BLOCK4_COLOR_MULTIPLY_MODES_DST_PREMULTIPLY                  4 : 4
#define GCREG_BLOCK4_COLOR_MULTIPLY_MODES_DST_PREMULTIPLY_End                  4
#define GCREG_BLOCK4_COLOR_MULTIPLY_MODES_DST_PREMULTIPLY_Start                4
#define GCREG_BLOCK4_COLOR_MULTIPLY_MODES_DST_PREMULTIPLY_Type               U01
#define   GCREG_BLOCK4_COLOR_MULTIPLY_MODES_DST_PREMULTIPLY_DISABLE          0x0
#define   GCREG_BLOCK4_COLOR_MULTIPLY_MODES_DST_PREMULTIPLY_ENABLE           0x1

#define GCREG_BLOCK4_COLOR_MULTIPLY_MODES_SRC_GLOBAL_PREMULTIPLY           9 : 8
#define GCREG_BLOCK4_COLOR_MULTIPLY_MODES_SRC_GLOBAL_PREMULTIPLY_End           9
#define GCREG_BLOCK4_COLOR_MULTIPLY_MODES_SRC_GLOBAL_PREMULTIPLY_Start         8
#define GCREG_BLOCK4_COLOR_MULTIPLY_MODES_SRC_GLOBAL_PREMULTIPLY_Type        U02
#define   GCREG_BLOCK4_COLOR_MULTIPLY_MODES_SRC_GLOBAL_PREMULTIPLY_DISABLE   0x0
#define   GCREG_BLOCK4_COLOR_MULTIPLY_MODES_SRC_GLOBAL_PREMULTIPLY_ALPHA     0x1
#define   GCREG_BLOCK4_COLOR_MULTIPLY_MODES_SRC_GLOBAL_PREMULTIPLY_COLOR     0x2

#define GCREG_BLOCK4_COLOR_MULTIPLY_MODES_DST_DEMULTIPLY                 20 : 20
#define GCREG_BLOCK4_COLOR_MULTIPLY_MODES_DST_DEMULTIPLY_End                  20
#define GCREG_BLOCK4_COLOR_MULTIPLY_MODES_DST_DEMULTIPLY_Start                20
#define GCREG_BLOCK4_COLOR_MULTIPLY_MODES_DST_DEMULTIPLY_Type                U01
#define   GCREG_BLOCK4_COLOR_MULTIPLY_MODES_DST_DEMULTIPLY_DISABLE           0x0
#define   GCREG_BLOCK4_COLOR_MULTIPLY_MODES_DST_DEMULTIPLY_ENABLE            0x1

/*******************************************************************************
** State gcregBlock4Transparency
*/

#define gcregBlock4TransparencyRegAddrs                                   0x4A4C
#define GCREG_BLOCK4_TRANSPARENCY_MSB                                         15
#define GCREG_BLOCK4_TRANSPARENCY_LSB                                          2
#define GCREG_BLOCK4_TRANSPARENCY_BLK                                          0
#define GCREG_BLOCK4_TRANSPARENCY_Count                                        4
#define GCREG_BLOCK4_TRANSPARENCY_FieldMask                           0xB3331333
#define GCREG_BLOCK4_TRANSPARENCY_ReadMask                            0xB3331333
#define GCREG_BLOCK4_TRANSPARENCY_WriteMask                           0xB3331333
#define GCREG_BLOCK4_TRANSPARENCY_ResetValue                          0x00000000

/* Source transparency mode. */
#define GCREG_BLOCK4_TRANSPARENCY_SOURCE                                   1 : 0
#define GCREG_BLOCK4_TRANSPARENCY_SOURCE_End                                   1
#define GCREG_BLOCK4_TRANSPARENCY_SOURCE_Start                                 0
#define GCREG_BLOCK4_TRANSPARENCY_SOURCE_Type                                U02
#define   GCREG_BLOCK4_TRANSPARENCY_SOURCE_OPAQUE                            0x0
#define   GCREG_BLOCK4_TRANSPARENCY_SOURCE_MASK                              0x1
#define   GCREG_BLOCK4_TRANSPARENCY_SOURCE_KEY                               0x2

/* Pattern transparency mode. KEY transparency mode is reserved. */
#define GCREG_BLOCK4_TRANSPARENCY_PATTERN                                  5 : 4
#define GCREG_BLOCK4_TRANSPARENCY_PATTERN_End                                  5
#define GCREG_BLOCK4_TRANSPARENCY_PATTERN_Start                                4
#define GCREG_BLOCK4_TRANSPARENCY_PATTERN_Type                               U02
#define   GCREG_BLOCK4_TRANSPARENCY_PATTERN_OPAQUE                           0x0
#define   GCREG_BLOCK4_TRANSPARENCY_PATTERN_MASK                             0x1
#define   GCREG_BLOCK4_TRANSPARENCY_PATTERN_KEY                              0x2

/* Destination transparency mode. MASK transparency mode is reserved. */
#define GCREG_BLOCK4_TRANSPARENCY_DESTINATION                              9 : 8
#define GCREG_BLOCK4_TRANSPARENCY_DESTINATION_End                              9
#define GCREG_BLOCK4_TRANSPARENCY_DESTINATION_Start                            8
#define GCREG_BLOCK4_TRANSPARENCY_DESTINATION_Type                           U02
#define   GCREG_BLOCK4_TRANSPARENCY_DESTINATION_OPAQUE                       0x0
#define   GCREG_BLOCK4_TRANSPARENCY_DESTINATION_MASK                         0x1
#define   GCREG_BLOCK4_TRANSPARENCY_DESTINATION_KEY                          0x2

/* Mask field for Source/Pattern/Destination fields. */
#define GCREG_BLOCK4_TRANSPARENCY_MASK_TRANSPARENCY                      12 : 12
#define GCREG_BLOCK4_TRANSPARENCY_MASK_TRANSPARENCY_End                       12
#define GCREG_BLOCK4_TRANSPARENCY_MASK_TRANSPARENCY_Start                     12
#define GCREG_BLOCK4_TRANSPARENCY_MASK_TRANSPARENCY_Type                     U01
#define   GCREG_BLOCK4_TRANSPARENCY_MASK_TRANSPARENCY_ENABLED                0x0
#define   GCREG_BLOCK4_TRANSPARENCY_MASK_TRANSPARENCY_MASKED                 0x1

/* Source usage override. */
#define GCREG_BLOCK4_TRANSPARENCY_USE_SRC_OVERRIDE                       17 : 16
#define GCREG_BLOCK4_TRANSPARENCY_USE_SRC_OVERRIDE_End                        17
#define GCREG_BLOCK4_TRANSPARENCY_USE_SRC_OVERRIDE_Start                      16
#define GCREG_BLOCK4_TRANSPARENCY_USE_SRC_OVERRIDE_Type                      U02
#define   GCREG_BLOCK4_TRANSPARENCY_USE_SRC_OVERRIDE_DEFAULT                 0x0
#define   GCREG_BLOCK4_TRANSPARENCY_USE_SRC_OVERRIDE_USE_ENABLE              0x1
#define   GCREG_BLOCK4_TRANSPARENCY_USE_SRC_OVERRIDE_USE_DISABLE             0x2

/* Pattern usage override. */
#define GCREG_BLOCK4_TRANSPARENCY_USE_PAT_OVERRIDE                       21 : 20
#define GCREG_BLOCK4_TRANSPARENCY_USE_PAT_OVERRIDE_End                        21
#define GCREG_BLOCK4_TRANSPARENCY_USE_PAT_OVERRIDE_Start                      20
#define GCREG_BLOCK4_TRANSPARENCY_USE_PAT_OVERRIDE_Type                      U02
#define   GCREG_BLOCK4_TRANSPARENCY_USE_PAT_OVERRIDE_DEFAULT                 0x0
#define   GCREG_BLOCK4_TRANSPARENCY_USE_PAT_OVERRIDE_USE_ENABLE              0x1
#define   GCREG_BLOCK4_TRANSPARENCY_USE_PAT_OVERRIDE_USE_DISABLE             0x2

/* Destination usage override. */
#define GCREG_BLOCK4_TRANSPARENCY_USE_DST_OVERRIDE                       25 : 24
#define GCREG_BLOCK4_TRANSPARENCY_USE_DST_OVERRIDE_End                        25
#define GCREG_BLOCK4_TRANSPARENCY_USE_DST_OVERRIDE_Start                      24
#define GCREG_BLOCK4_TRANSPARENCY_USE_DST_OVERRIDE_Type                      U02
#define   GCREG_BLOCK4_TRANSPARENCY_USE_DST_OVERRIDE_DEFAULT                 0x0
#define   GCREG_BLOCK4_TRANSPARENCY_USE_DST_OVERRIDE_USE_ENABLE              0x1
#define   GCREG_BLOCK4_TRANSPARENCY_USE_DST_OVERRIDE_USE_DISABLE             0x2

/* 2D resource usage override mask field. */
#define GCREG_BLOCK4_TRANSPARENCY_MASK_RESOURCE_OVERRIDE                 28 : 28
#define GCREG_BLOCK4_TRANSPARENCY_MASK_RESOURCE_OVERRIDE_End                  28
#define GCREG_BLOCK4_TRANSPARENCY_MASK_RESOURCE_OVERRIDE_Start                28
#define GCREG_BLOCK4_TRANSPARENCY_MASK_RESOURCE_OVERRIDE_Type                U01
#define   GCREG_BLOCK4_TRANSPARENCY_MASK_RESOURCE_OVERRIDE_ENABLED           0x0
#define   GCREG_BLOCK4_TRANSPARENCY_MASK_RESOURCE_OVERRIDE_MASKED            0x1

/* DFB Color Key. */
#define GCREG_BLOCK4_TRANSPARENCY_DFB_COLOR_KEY                          29 : 29
#define GCREG_BLOCK4_TRANSPARENCY_DFB_COLOR_KEY_End                           29
#define GCREG_BLOCK4_TRANSPARENCY_DFB_COLOR_KEY_Start                         29
#define GCREG_BLOCK4_TRANSPARENCY_DFB_COLOR_KEY_Type                         U01
#define   GCREG_BLOCK4_TRANSPARENCY_DFB_COLOR_KEY_DISABLED                   0x0
#define   GCREG_BLOCK4_TRANSPARENCY_DFB_COLOR_KEY_ENABLED                    0x1

#define GCREG_BLOCK4_TRANSPARENCY_MASK_DFB_COLOR_KEY                     31 : 31
#define GCREG_BLOCK4_TRANSPARENCY_MASK_DFB_COLOR_KEY_End                      31
#define GCREG_BLOCK4_TRANSPARENCY_MASK_DFB_COLOR_KEY_Start                    31
#define GCREG_BLOCK4_TRANSPARENCY_MASK_DFB_COLOR_KEY_Type                    U01
#define   GCREG_BLOCK4_TRANSPARENCY_MASK_DFB_COLOR_KEY_ENABLED               0x0
#define   GCREG_BLOCK4_TRANSPARENCY_MASK_DFB_COLOR_KEY_MASKED                0x1

/*******************************************************************************
** State gcregBlock4Control
*/

/* General purpose control register. */

#define gcregBlock4ControlRegAddrs                                        0x4A50
#define GCREG_BLOCK4_CONTROL_MSB                                              15
#define GCREG_BLOCK4_CONTROL_LSB                                               2
#define GCREG_BLOCK4_CONTROL_BLK                                               0
#define GCREG_BLOCK4_CONTROL_Count                                             4
#define GCREG_BLOCK4_CONTROL_FieldMask                                0x00000999
#define GCREG_BLOCK4_CONTROL_ReadMask                                 0x00000999
#define GCREG_BLOCK4_CONTROL_WriteMask                                0x00000999
#define GCREG_BLOCK4_CONTROL_ResetValue                               0x00000000

#define GCREG_BLOCK4_CONTROL_YUV                                           0 : 0
#define GCREG_BLOCK4_CONTROL_YUV_End                                           0
#define GCREG_BLOCK4_CONTROL_YUV_Start                                         0
#define GCREG_BLOCK4_CONTROL_YUV_Type                                        U01
#define   GCREG_BLOCK4_CONTROL_YUV_601                                       0x0
#define   GCREG_BLOCK4_CONTROL_YUV_709                                       0x1

#define GCREG_BLOCK4_CONTROL_MASK_YUV                                      3 : 3
#define GCREG_BLOCK4_CONTROL_MASK_YUV_End                                      3
#define GCREG_BLOCK4_CONTROL_MASK_YUV_Start                                    3
#define GCREG_BLOCK4_CONTROL_MASK_YUV_Type                                   U01
#define   GCREG_BLOCK4_CONTROL_MASK_YUV_ENABLED                              0x0
#define   GCREG_BLOCK4_CONTROL_MASK_YUV_MASKED                               0x1

#define GCREG_BLOCK4_CONTROL_UV_SWIZZLE                                    4 : 4
#define GCREG_BLOCK4_CONTROL_UV_SWIZZLE_End                                    4
#define GCREG_BLOCK4_CONTROL_UV_SWIZZLE_Start                                  4
#define GCREG_BLOCK4_CONTROL_UV_SWIZZLE_Type                                 U01
#define   GCREG_BLOCK4_CONTROL_UV_SWIZZLE_UV                                 0x0
#define   GCREG_BLOCK4_CONTROL_UV_SWIZZLE_VU                                 0x1

#define GCREG_BLOCK4_CONTROL_MASK_UV_SWIZZLE                               7 : 7
#define GCREG_BLOCK4_CONTROL_MASK_UV_SWIZZLE_End                               7
#define GCREG_BLOCK4_CONTROL_MASK_UV_SWIZZLE_Start                             7
#define GCREG_BLOCK4_CONTROL_MASK_UV_SWIZZLE_Type                            U01
#define   GCREG_BLOCK4_CONTROL_MASK_UV_SWIZZLE_ENABLED                       0x0
#define   GCREG_BLOCK4_CONTROL_MASK_UV_SWIZZLE_MASKED                        0x1

/* YUV to RGB convert enable */
#define GCREG_BLOCK4_CONTROL_YUVRGB                                        8 : 8
#define GCREG_BLOCK4_CONTROL_YUVRGB_End                                        8
#define GCREG_BLOCK4_CONTROL_YUVRGB_Start                                      8
#define GCREG_BLOCK4_CONTROL_YUVRGB_Type                                     U01
#define   GCREG_BLOCK4_CONTROL_YUVRGB_DISABLED                               0x0
#define   GCREG_BLOCK4_CONTROL_YUVRGB_ENABLED                                0x1

#define GCREG_BLOCK4_CONTROL_MASK_YUVRGB                                 11 : 11
#define GCREG_BLOCK4_CONTROL_MASK_YUVRGB_End                                  11
#define GCREG_BLOCK4_CONTROL_MASK_YUVRGB_Start                                11
#define GCREG_BLOCK4_CONTROL_MASK_YUVRGB_Type                                U01
#define   GCREG_BLOCK4_CONTROL_MASK_YUVRGB_ENABLED                           0x0
#define   GCREG_BLOCK4_CONTROL_MASK_YUVRGB_MASKED                            0x1

/*******************************************************************************
** State gcregBlock4SrcColorKeyHigh
*/

/* Defines the source transparency color in source format. */

#define gcregBlock4SrcColorKeyHighRegAddrs                                0x4A54
#define GCREG_BLOCK4_SRC_COLOR_KEY_HIGH_MSB                                   15
#define GCREG_BLOCK4_SRC_COLOR_KEY_HIGH_LSB                                    2
#define GCREG_BLOCK4_SRC_COLOR_KEY_HIGH_BLK                                    0
#define GCREG_BLOCK4_SRC_COLOR_KEY_HIGH_Count                                  4
#define GCREG_BLOCK4_SRC_COLOR_KEY_HIGH_FieldMask                     0xFFFFFFFF
#define GCREG_BLOCK4_SRC_COLOR_KEY_HIGH_ReadMask                      0xFFFFFFFF
#define GCREG_BLOCK4_SRC_COLOR_KEY_HIGH_WriteMask                     0xFFFFFFFF
#define GCREG_BLOCK4_SRC_COLOR_KEY_HIGH_ResetValue                    0x00000000

#define GCREG_BLOCK4_SRC_COLOR_KEY_HIGH_ALPHA                            31 : 24
#define GCREG_BLOCK4_SRC_COLOR_KEY_HIGH_ALPHA_End                             31
#define GCREG_BLOCK4_SRC_COLOR_KEY_HIGH_ALPHA_Start                           24
#define GCREG_BLOCK4_SRC_COLOR_KEY_HIGH_ALPHA_Type                           U08

#define GCREG_BLOCK4_SRC_COLOR_KEY_HIGH_RED                              23 : 16
#define GCREG_BLOCK4_SRC_COLOR_KEY_HIGH_RED_End                               23
#define GCREG_BLOCK4_SRC_COLOR_KEY_HIGH_RED_Start                             16
#define GCREG_BLOCK4_SRC_COLOR_KEY_HIGH_RED_Type                             U08

#define GCREG_BLOCK4_SRC_COLOR_KEY_HIGH_GREEN                             15 : 8
#define GCREG_BLOCK4_SRC_COLOR_KEY_HIGH_GREEN_End                             15
#define GCREG_BLOCK4_SRC_COLOR_KEY_HIGH_GREEN_Start                            8
#define GCREG_BLOCK4_SRC_COLOR_KEY_HIGH_GREEN_Type                           U08

#define GCREG_BLOCK4_SRC_COLOR_KEY_HIGH_BLUE                               7 : 0
#define GCREG_BLOCK4_SRC_COLOR_KEY_HIGH_BLUE_End                               7
#define GCREG_BLOCK4_SRC_COLOR_KEY_HIGH_BLUE_Start                             0
#define GCREG_BLOCK4_SRC_COLOR_KEY_HIGH_BLUE_Type                            U08

/*******************************************************************************
** State gcregBlock4SrcExConfig
*/

#define gcregBlock4SrcExConfigRegAddrs                                    0x4A58
#define GCREG_BLOCK4_SRC_EX_CONFIG_MSB                                        15
#define GCREG_BLOCK4_SRC_EX_CONFIG_LSB                                         2
#define GCREG_BLOCK4_SRC_EX_CONFIG_BLK                                         0
#define GCREG_BLOCK4_SRC_EX_CONFIG_Count                                       4
#define GCREG_BLOCK4_SRC_EX_CONFIG_FieldMask                          0x00000109
#define GCREG_BLOCK4_SRC_EX_CONFIG_ReadMask                           0x00000109
#define GCREG_BLOCK4_SRC_EX_CONFIG_WriteMask                          0x00000109
#define GCREG_BLOCK4_SRC_EX_CONFIG_ResetValue                         0x00000000

/* Source multi tiled address computation control. */
#define GCREG_BLOCK4_SRC_EX_CONFIG_MULTI_TILED                             0 : 0
#define GCREG_BLOCK4_SRC_EX_CONFIG_MULTI_TILED_End                             0
#define GCREG_BLOCK4_SRC_EX_CONFIG_MULTI_TILED_Start                           0
#define GCREG_BLOCK4_SRC_EX_CONFIG_MULTI_TILED_Type                          U01
#define   GCREG_BLOCK4_SRC_EX_CONFIG_MULTI_TILED_DISABLED                    0x0
#define   GCREG_BLOCK4_SRC_EX_CONFIG_MULTI_TILED_ENABLED                     0x1

/* Source super tiled address computation control. */
#define GCREG_BLOCK4_SRC_EX_CONFIG_SUPER_TILED                             3 : 3
#define GCREG_BLOCK4_SRC_EX_CONFIG_SUPER_TILED_End                             3
#define GCREG_BLOCK4_SRC_EX_CONFIG_SUPER_TILED_Start                           3
#define GCREG_BLOCK4_SRC_EX_CONFIG_SUPER_TILED_Type                          U01
#define   GCREG_BLOCK4_SRC_EX_CONFIG_SUPER_TILED_DISABLED                    0x0
#define   GCREG_BLOCK4_SRC_EX_CONFIG_SUPER_TILED_ENABLED                     0x1

/* Source super tiled address computation control. */
#define GCREG_BLOCK4_SRC_EX_CONFIG_MINOR_TILED                             8 : 8
#define GCREG_BLOCK4_SRC_EX_CONFIG_MINOR_TILED_End                             8
#define GCREG_BLOCK4_SRC_EX_CONFIG_MINOR_TILED_Start                           8
#define GCREG_BLOCK4_SRC_EX_CONFIG_MINOR_TILED_Type                          U01
#define   GCREG_BLOCK4_SRC_EX_CONFIG_MINOR_TILED_DISABLED                    0x0
#define   GCREG_BLOCK4_SRC_EX_CONFIG_MINOR_TILED_ENABLED                     0x1

/*******************************************************************************
** State gcregBlock4SrcExAddress
*/

/* 32-bit aligned base address of the source extra surface. */

#define gcregBlock4SrcExAddressRegAddrs                                   0x4A5C
#define GCREG_BLOCK4_SRC_EX_ADDRESS_MSB                                       15
#define GCREG_BLOCK4_SRC_EX_ADDRESS_LSB                                        2
#define GCREG_BLOCK4_SRC_EX_ADDRESS_BLK                                        0
#define GCREG_BLOCK4_SRC_EX_ADDRESS_Count                                      4
#define GCREG_BLOCK4_SRC_EX_ADDRESS_FieldMask                         0xFFFFFFFF
#define GCREG_BLOCK4_SRC_EX_ADDRESS_ReadMask                          0xFFFFFFFC
#define GCREG_BLOCK4_SRC_EX_ADDRESS_WriteMask                         0xFFFFFFFC
#define GCREG_BLOCK4_SRC_EX_ADDRESS_ResetValue                        0x00000000

#define GCREG_BLOCK4_SRC_EX_ADDRESS_ADDRESS                               31 : 0
#define GCREG_BLOCK4_SRC_EX_ADDRESS_ADDRESS_End                               30
#define GCREG_BLOCK4_SRC_EX_ADDRESS_ADDRESS_Start                              0
#define GCREG_BLOCK4_SRC_EX_ADDRESS_ADDRESS_Type                             U31

/*******************************************************************************
** State gcregBlock8SrcAddressEx
*/

/* 32-bit aligned base address of the source surface. */

#define gcregBlock8SrcAddressRegAddrs                                     0x4A80
#define GCREG_BLOCK8_SRC_ADDRESS_MSB                                          15
#define GCREG_BLOCK8_SRC_ADDRESS_LSB                                           3
#define GCREG_BLOCK8_SRC_ADDRESS_BLK                                           0
#define GCREG_BLOCK8_SRC_ADDRESS_Count                                         8
#define GCREG_BLOCK8_SRC_ADDRESS_FieldMask                            0xFFFFFFFF
#define GCREG_BLOCK8_SRC_ADDRESS_ReadMask                             0xFFFFFFFC
#define GCREG_BLOCK8_SRC_ADDRESS_WriteMask                            0xFFFFFFFC
#define GCREG_BLOCK8_SRC_ADDRESS_ResetValue                           0x00000000

#define GCREG_BLOCK8_SRC_ADDRESS_ADDRESS                                  31 : 0
#define GCREG_BLOCK8_SRC_ADDRESS_ADDRESS_End                                  30
#define GCREG_BLOCK8_SRC_ADDRESS_ADDRESS_Start                                 0
#define GCREG_BLOCK8_SRC_ADDRESS_ADDRESS_Type                                U31

/*******************************************************************************
** State gcregBlock8SrcStride
*/

/* Stride of the source surface in bytes. To calculate the stride multiply
** the surface width in pixels by the number of  bytes per pixel.
*/

#define gcregBlock8SrcStrideRegAddrs                                      0x4A88
#define GCREG_BLOCK8_SRC_STRIDE_MSB                                           15
#define GCREG_BLOCK8_SRC_STRIDE_LSB                                            3
#define GCREG_BLOCK8_SRC_STRIDE_BLK                                            0
#define GCREG_BLOCK8_SRC_STRIDE_Count                                          8
#define GCREG_BLOCK8_SRC_STRIDE_FieldMask                             0x0003FFFF
#define GCREG_BLOCK8_SRC_STRIDE_ReadMask                              0x0003FFFC
#define GCREG_BLOCK8_SRC_STRIDE_WriteMask                             0x0003FFFC
#define GCREG_BLOCK8_SRC_STRIDE_ResetValue                            0x00000000

#define GCREG_BLOCK8_SRC_STRIDE_STRIDE                                    17 : 0
#define GCREG_BLOCK8_SRC_STRIDE_STRIDE_End                                    17
#define GCREG_BLOCK8_SRC_STRIDE_STRIDE_Start                                   0
#define GCREG_BLOCK8_SRC_STRIDE_STRIDE_Type                                  U18

/*******************************************************************************
** State gcregBlock8SrcRotationConfig
*/

/* 90 degree rotation configuration for the source surface. Width field
** specifies the width of the surface in pixels.
*/

#define gcregBlock8SrcRotationConfigRegAddrs                              0x4A90
#define GCREG_BLOCK8_SRC_ROTATION_CONFIG_MSB                                  15
#define GCREG_BLOCK8_SRC_ROTATION_CONFIG_LSB                                   3
#define GCREG_BLOCK8_SRC_ROTATION_CONFIG_BLK                                   0
#define GCREG_BLOCK8_SRC_ROTATION_CONFIG_Count                                 8
#define GCREG_BLOCK8_SRC_ROTATION_CONFIG_FieldMask                    0x0001FFFF
#define GCREG_BLOCK8_SRC_ROTATION_CONFIG_ReadMask                     0x0001FFFF
#define GCREG_BLOCK8_SRC_ROTATION_CONFIG_WriteMask                    0x0001FFFF
#define GCREG_BLOCK8_SRC_ROTATION_CONFIG_ResetValue                   0x00000000

#define GCREG_BLOCK8_SRC_ROTATION_CONFIG_WIDTH                            15 : 0
#define GCREG_BLOCK8_SRC_ROTATION_CONFIG_WIDTH_End                            15
#define GCREG_BLOCK8_SRC_ROTATION_CONFIG_WIDTH_Start                           0
#define GCREG_BLOCK8_SRC_ROTATION_CONFIG_WIDTH_Type                          U16

#define GCREG_BLOCK8_SRC_ROTATION_CONFIG_ROTATION                        16 : 16
#define GCREG_BLOCK8_SRC_ROTATION_CONFIG_ROTATION_End                         16
#define GCREG_BLOCK8_SRC_ROTATION_CONFIG_ROTATION_Start                       16
#define GCREG_BLOCK8_SRC_ROTATION_CONFIG_ROTATION_Type                       U01
#define   GCREG_BLOCK8_SRC_ROTATION_CONFIG_ROTATION_NORMAL                   0x0
#define   GCREG_BLOCK8_SRC_ROTATION_CONFIG_ROTATION_ROTATED                  0x1

/*******************************************************************************
** State gcregBlock8SrcConfig
*/

/* Source surface configuration register. */

#define gcregBlock8SrcConfigRegAddrs                                      0x4A98
#define GCREG_BLOCK8_SRC_CONFIG_MSB                                           15
#define GCREG_BLOCK8_SRC_CONFIG_LSB                                            3
#define GCREG_BLOCK8_SRC_CONFIG_BLK                                            0
#define GCREG_BLOCK8_SRC_CONFIG_Count                                          8
#define GCREG_BLOCK8_SRC_CONFIG_FieldMask                             0xDF30B1C0
#define GCREG_BLOCK8_SRC_CONFIG_ReadMask                              0xDF30B1C0
#define GCREG_BLOCK8_SRC_CONFIG_WriteMask                             0xDF30B1C0
#define GCREG_BLOCK8_SRC_CONFIG_ResetValue                            0x00000000

/* Control source endianess. */
#define GCREG_BLOCK8_SRC_CONFIG_ENDIAN_CONTROL                           31 : 30
#define GCREG_BLOCK8_SRC_CONFIG_ENDIAN_CONTROL_End                            31
#define GCREG_BLOCK8_SRC_CONFIG_ENDIAN_CONTROL_Start                          30
#define GCREG_BLOCK8_SRC_CONFIG_ENDIAN_CONTROL_Type                          U02
#define   GCREG_BLOCK8_SRC_CONFIG_ENDIAN_CONTROL_NO_SWAP                     0x0
#define   GCREG_BLOCK8_SRC_CONFIG_ENDIAN_CONTROL_SWAP_WORD                   0x1
#define   GCREG_BLOCK8_SRC_CONFIG_ENDIAN_CONTROL_SWAP_DWORD                  0x2

/* Defines the pixel format of the source surface. */
#define GCREG_BLOCK8_SRC_CONFIG_SOURCE_FORMAT                            28 : 24
#define GCREG_BLOCK8_SRC_CONFIG_SOURCE_FORMAT_End                             28
#define GCREG_BLOCK8_SRC_CONFIG_SOURCE_FORMAT_Start                           24
#define GCREG_BLOCK8_SRC_CONFIG_SOURCE_FORMAT_Type                           U05
#define   GCREG_BLOCK8_SRC_CONFIG_SOURCE_FORMAT_X4R4G4B4                    0x00
#define   GCREG_BLOCK8_SRC_CONFIG_SOURCE_FORMAT_A4R4G4B4                    0x01
#define   GCREG_BLOCK8_SRC_CONFIG_SOURCE_FORMAT_X1R5G5B5                    0x02
#define   GCREG_BLOCK8_SRC_CONFIG_SOURCE_FORMAT_A1R5G5B5                    0x03
#define   GCREG_BLOCK8_SRC_CONFIG_SOURCE_FORMAT_R5G6B5                      0x04
#define   GCREG_BLOCK8_SRC_CONFIG_SOURCE_FORMAT_X8R8G8B8                    0x05
#define   GCREG_BLOCK8_SRC_CONFIG_SOURCE_FORMAT_A8R8G8B8                    0x06
#define   GCREG_BLOCK8_SRC_CONFIG_SOURCE_FORMAT_YUY2                        0x07
#define   GCREG_BLOCK8_SRC_CONFIG_SOURCE_FORMAT_UYVY                        0x08
#define   GCREG_BLOCK8_SRC_CONFIG_SOURCE_FORMAT_INDEX8                      0x09
#define   GCREG_BLOCK8_SRC_CONFIG_SOURCE_FORMAT_MONOCHROME                  0x0A
#define   GCREG_BLOCK8_SRC_CONFIG_SOURCE_FORMAT_YV12                        0x0F
#define   GCREG_BLOCK8_SRC_CONFIG_SOURCE_FORMAT_A8                          0x10
#define   GCREG_BLOCK8_SRC_CONFIG_SOURCE_FORMAT_NV12                        0x11
#define   GCREG_BLOCK8_SRC_CONFIG_SOURCE_FORMAT_NV16                        0x12
#define   GCREG_BLOCK8_SRC_CONFIG_SOURCE_FORMAT_RG16                        0x13

/* Color channel swizzles. */
#define GCREG_BLOCK8_SRC_CONFIG_SWIZZLE                                  21 : 20
#define GCREG_BLOCK8_SRC_CONFIG_SWIZZLE_End                                   21
#define GCREG_BLOCK8_SRC_CONFIG_SWIZZLE_Start                                 20
#define GCREG_BLOCK8_SRC_CONFIG_SWIZZLE_Type                                 U02
#define   GCREG_BLOCK8_SRC_CONFIG_SWIZZLE_ARGB                               0x0
#define   GCREG_BLOCK8_SRC_CONFIG_SWIZZLE_RGBA                               0x1
#define   GCREG_BLOCK8_SRC_CONFIG_SWIZZLE_ABGR                               0x2
#define   GCREG_BLOCK8_SRC_CONFIG_SWIZZLE_BGRA                               0x3

/* Mono expansion: if 0, transparency color will be 0, otherwise transparency **
** color will be 1.                                                           */
#define GCREG_BLOCK8_SRC_CONFIG_MONO_TRANSPARENCY                        15 : 15
#define GCREG_BLOCK8_SRC_CONFIG_MONO_TRANSPARENCY_End                         15
#define GCREG_BLOCK8_SRC_CONFIG_MONO_TRANSPARENCY_Start                       15
#define GCREG_BLOCK8_SRC_CONFIG_MONO_TRANSPARENCY_Type                       U01
#define   GCREG_BLOCK8_SRC_CONFIG_MONO_TRANSPARENCY_BACKGROUND               0x0
#define   GCREG_BLOCK8_SRC_CONFIG_MONO_TRANSPARENCY_FOREGROUND               0x1

/* Mono expansion or masked blit: stream packing in pixels. Determines how    **
** many horizontal pixels are there per each 32-bit chunk. For example, if    **
** set to Packed8, each 32-bit chunk is 8-pixel wide, which also means that   **
** it defines 4 vertical lines of pixels.                                     */
#define GCREG_BLOCK8_SRC_CONFIG_PACK                                     13 : 12
#define GCREG_BLOCK8_SRC_CONFIG_PACK_End                                      13
#define GCREG_BLOCK8_SRC_CONFIG_PACK_Start                                    12
#define GCREG_BLOCK8_SRC_CONFIG_PACK_Type                                    U02
#define   GCREG_BLOCK8_SRC_CONFIG_PACK_PACKED8                               0x0
#define   GCREG_BLOCK8_SRC_CONFIG_PACK_PACKED16                              0x1
#define   GCREG_BLOCK8_SRC_CONFIG_PACK_PACKED32                              0x2
#define   GCREG_BLOCK8_SRC_CONFIG_PACK_UNPACKED                              0x3

/* Source data location: set to STREAM for mono expansion blits or masked     **
** blits. For mono expansion blits the complete bitmap comes from the command **
** stream. For masked blits the source data comes from the memory and the     **
** mask from the command stream.                                              */
#define GCREG_BLOCK8_SRC_CONFIG_LOCATION                                   8 : 8
#define GCREG_BLOCK8_SRC_CONFIG_LOCATION_End                                   8
#define GCREG_BLOCK8_SRC_CONFIG_LOCATION_Start                                 8
#define GCREG_BLOCK8_SRC_CONFIG_LOCATION_Type                                U01
#define   GCREG_BLOCK8_SRC_CONFIG_LOCATION_MEMORY                            0x0
#define   GCREG_BLOCK8_SRC_CONFIG_LOCATION_STREAM                            0x1

/* Source linear/tiled address computation control. */
#define GCREG_BLOCK8_SRC_CONFIG_TILED                                      7 : 7
#define GCREG_BLOCK8_SRC_CONFIG_TILED_End                                      7
#define GCREG_BLOCK8_SRC_CONFIG_TILED_Start                                    7
#define GCREG_BLOCK8_SRC_CONFIG_TILED_Type                                   U01
#define   GCREG_BLOCK8_SRC_CONFIG_TILED_DISABLED                             0x0
#define   GCREG_BLOCK8_SRC_CONFIG_TILED_ENABLED                              0x1

/* If set to ABSOLUTE, the source coordinates are treated as absolute         **
** coordinates inside the source surface. If set to RELATIVE, the source      **
** coordinates are treated as the offsets from the destination coordinates    **
** with the source size equal to the size of the destination.                 */
#define GCREG_BLOCK8_SRC_CONFIG_SRC_RELATIVE                               6 : 6
#define GCREG_BLOCK8_SRC_CONFIG_SRC_RELATIVE_End                               6
#define GCREG_BLOCK8_SRC_CONFIG_SRC_RELATIVE_Start                             6
#define GCREG_BLOCK8_SRC_CONFIG_SRC_RELATIVE_Type                            U01
#define   GCREG_BLOCK8_SRC_CONFIG_SRC_RELATIVE_ABSOLUTE                      0x0
#define   GCREG_BLOCK8_SRC_CONFIG_SRC_RELATIVE_RELATIVE                      0x1

/*******************************************************************************
** State gcregBlock8SrcOrigin
*/

/* Absolute or relative (see SRC_RELATIVE field of gcregBlock8SrcConfig
** register) X and Y coordinates in pixels of the top left corner of the
** source rectangle within the source surface.
*/

#define gcregBlock8SrcOriginRegAddrs                                      0x4AA0
#define GCREG_BLOCK8_SRC_ORIGIN_MSB                                           15
#define GCREG_BLOCK8_SRC_ORIGIN_LSB                                            3
#define GCREG_BLOCK8_SRC_ORIGIN_BLK                                            0
#define GCREG_BLOCK8_SRC_ORIGIN_Count                                          8
#define GCREG_BLOCK8_SRC_ORIGIN_FieldMask                             0xFFFFFFFF
#define GCREG_BLOCK8_SRC_ORIGIN_ReadMask                              0xFFFFFFFF
#define GCREG_BLOCK8_SRC_ORIGIN_WriteMask                             0xFFFFFFFF
#define GCREG_BLOCK8_SRC_ORIGIN_ResetValue                            0x00000000

#define GCREG_BLOCK8_SRC_ORIGIN_Y                                        31 : 16
#define GCREG_BLOCK8_SRC_ORIGIN_Y_End                                         31
#define GCREG_BLOCK8_SRC_ORIGIN_Y_Start                                       16
#define GCREG_BLOCK8_SRC_ORIGIN_Y_Type                                       U16

#define GCREG_BLOCK8_SRC_ORIGIN_X                                         15 : 0
#define GCREG_BLOCK8_SRC_ORIGIN_X_End                                         15
#define GCREG_BLOCK8_SRC_ORIGIN_X_Start                                        0
#define GCREG_BLOCK8_SRC_ORIGIN_X_Type                                       U16

/*******************************************************************************
** State gcregBlock8SrcSize
*/

/* Width and height of the source rectangle in pixels. If the source is
** relative (see SRC_RELATIVE field of gcregBlock8SrcConfig register) or a
** regular bitblt is being performed without stretching, this register is
** ignored and the source size is assumed to be the same as the destination.
*/

#define gcregBlock8SrcSizeRegAddrs                                        0x4AA8
#define GCREG_BLOCK8_SRC_SIZE_MSB                                             15
#define GCREG_BLOCK8_SRC_SIZE_LSB                                              3
#define GCREG_BLOCK8_SRC_SIZE_BLK                                              0
#define GCREG_BLOCK8_SRC_SIZE_Count                                            8
#define GCREG_BLOCK8_SRC_SIZE_FieldMask                               0xFFFFFFFF
#define GCREG_BLOCK8_SRC_SIZE_ReadMask                                0xFFFFFFFF
#define GCREG_BLOCK8_SRC_SIZE_WriteMask                               0xFFFFFFFF
#define GCREG_BLOCK8_SRC_SIZE_ResetValue                              0x00000000

#define GCREG_BLOCK8_SRC_SIZE_Y                                          31 : 16
#define GCREG_BLOCK8_SRC_SIZE_Y_End                                           31
#define GCREG_BLOCK8_SRC_SIZE_Y_Start                                         16
#define GCREG_BLOCK8_SRC_SIZE_Y_Type                                         U16

#define GCREG_BLOCK8_SRC_SIZE_X                                           15 : 0
#define GCREG_BLOCK8_SRC_SIZE_X_End                                           15
#define GCREG_BLOCK8_SRC_SIZE_X_Start                                          0
#define GCREG_BLOCK8_SRC_SIZE_X_Type                                         U16

/*******************************************************************************
** State gcregBlock8SrcColorBg
*/

/* Select the color where source becomes transparent. It must be programmed
** in A8R8G8B8 format.
*/

#define gcregBlock8SrcColorBgRegAddrs                                     0x4AB0
#define GCREG_BLOCK8_SRC_COLOR_BG_MSB                                         15
#define GCREG_BLOCK8_SRC_COLOR_BG_LSB                                          3
#define GCREG_BLOCK8_SRC_COLOR_BG_BLK                                          0
#define GCREG_BLOCK8_SRC_COLOR_BG_Count                                        8
#define GCREG_BLOCK8_SRC_COLOR_BG_FieldMask                           0xFFFFFFFF
#define GCREG_BLOCK8_SRC_COLOR_BG_ReadMask                            0xFFFFFFFF
#define GCREG_BLOCK8_SRC_COLOR_BG_WriteMask                           0xFFFFFFFF
#define GCREG_BLOCK8_SRC_COLOR_BG_ResetValue                          0x00000000

#define GCREG_BLOCK8_SRC_COLOR_BG_ALPHA                                  31 : 24
#define GCREG_BLOCK8_SRC_COLOR_BG_ALPHA_End                                   31
#define GCREG_BLOCK8_SRC_COLOR_BG_ALPHA_Start                                 24
#define GCREG_BLOCK8_SRC_COLOR_BG_ALPHA_Type                                 U08

#define GCREG_BLOCK8_SRC_COLOR_BG_RED                                    23 : 16
#define GCREG_BLOCK8_SRC_COLOR_BG_RED_End                                     23
#define GCREG_BLOCK8_SRC_COLOR_BG_RED_Start                                   16
#define GCREG_BLOCK8_SRC_COLOR_BG_RED_Type                                   U08

#define GCREG_BLOCK8_SRC_COLOR_BG_GREEN                                   15 : 8
#define GCREG_BLOCK8_SRC_COLOR_BG_GREEN_End                                   15
#define GCREG_BLOCK8_SRC_COLOR_BG_GREEN_Start                                  8
#define GCREG_BLOCK8_SRC_COLOR_BG_GREEN_Type                                 U08

#define GCREG_BLOCK8_SRC_COLOR_BG_BLUE                                     7 : 0
#define GCREG_BLOCK8_SRC_COLOR_BG_BLUE_End                                     7
#define GCREG_BLOCK8_SRC_COLOR_BG_BLUE_Start                                   0
#define GCREG_BLOCK8_SRC_COLOR_BG_BLUE_Type                                  U08

/*******************************************************************************
** State gcregBlock8Rop
*/

/* Raster operation foreground and background codes. Even though ROP is not
** used in CLEAR, HOR_FILTER_BLT, VER_FILTER_BLT and alpha-eanbled BIT_BLTs,
** ROP code still has to be programmed, because the engine makes the decision
** whether source, destination and pattern are involved in the current
** operation and the correct decision is essential for the engine to complete
** the operation as expected.
*/

#define gcregBlock8RopRegAddrs                                            0x4AB8
#define GCREG_BLOCK8_ROP_MSB                                                  15
#define GCREG_BLOCK8_ROP_LSB                                                   3
#define GCREG_BLOCK8_ROP_BLK                                                   0
#define GCREG_BLOCK8_ROP_Count                                                 8
#define GCREG_BLOCK8_ROP_FieldMask                                    0x0030FFFF
#define GCREG_BLOCK8_ROP_ReadMask                                     0x0030FFFF
#define GCREG_BLOCK8_ROP_WriteMask                                    0x0030FFFF
#define GCREG_BLOCK8_ROP_ResetValue                                   0x00000000

/* ROP type: ROP2, ROP3 or ROP4 */
#define GCREG_BLOCK8_ROP_TYPE                                            21 : 20
#define GCREG_BLOCK8_ROP_TYPE_End                                             21
#define GCREG_BLOCK8_ROP_TYPE_Start                                           20
#define GCREG_BLOCK8_ROP_TYPE_Type                                           U02
#define   GCREG_BLOCK8_ROP_TYPE_ROP2_PATTERN                                 0x0
#define   GCREG_BLOCK8_ROP_TYPE_ROP2_SOURCE                                  0x1
#define   GCREG_BLOCK8_ROP_TYPE_ROP3                                         0x2
#define   GCREG_BLOCK8_ROP_TYPE_ROP4                                         0x3

/* Background ROP code is used for transparent pixels. */
#define GCREG_BLOCK8_ROP_ROP_BG                                           15 : 8
#define GCREG_BLOCK8_ROP_ROP_BG_End                                           15
#define GCREG_BLOCK8_ROP_ROP_BG_Start                                          8
#define GCREG_BLOCK8_ROP_ROP_BG_Type                                         U08

/* Background ROP code is used for opaque pixels. */
#define GCREG_BLOCK8_ROP_ROP_FG                                            7 : 0
#define GCREG_BLOCK8_ROP_ROP_FG_End                                            7
#define GCREG_BLOCK8_ROP_ROP_FG_Start                                          0
#define GCREG_BLOCK8_ROP_ROP_FG_Type                                         U08

/*******************************************************************************
** State gcregBlock8AlphaControl
*/

#define gcregBlock8AlphaControlRegAddrs                                   0x4AC0
#define GCREG_BLOCK8_ALPHA_CONTROL_MSB                                        15
#define GCREG_BLOCK8_ALPHA_CONTROL_LSB                                         3
#define GCREG_BLOCK8_ALPHA_CONTROL_BLK                                         0
#define GCREG_BLOCK8_ALPHA_CONTROL_Count                                       8
#define GCREG_BLOCK8_ALPHA_CONTROL_FieldMask                          0x00000001
#define GCREG_BLOCK8_ALPHA_CONTROL_ReadMask                           0x00000001
#define GCREG_BLOCK8_ALPHA_CONTROL_WriteMask                          0x00000001
#define GCREG_BLOCK8_ALPHA_CONTROL_ResetValue                         0x00000000

#define GCREG_BLOCK8_ALPHA_CONTROL_ENABLE                                  0 : 0
#define GCREG_BLOCK8_ALPHA_CONTROL_ENABLE_End                                  0
#define GCREG_BLOCK8_ALPHA_CONTROL_ENABLE_Start                                0
#define GCREG_BLOCK8_ALPHA_CONTROL_ENABLE_Type                               U01
#define   GCREG_BLOCK8_ALPHA_CONTROL_ENABLE_OFF                              0x0
#define   GCREG_BLOCK8_ALPHA_CONTROL_ENABLE_ON                               0x1

/*******************************************************************************
** State gcregBlock8AlphaModes
*/

#define gcregBlock8AlphaModesRegAddrs                                     0x4AC8
#define GCREG_BLOCK8_ALPHA_MODES_MSB                                          15
#define GCREG_BLOCK8_ALPHA_MODES_LSB                                           3
#define GCREG_BLOCK8_ALPHA_MODES_BLK                                           0
#define GCREG_BLOCK8_ALPHA_MODES_Count                                         8
#define GCREG_BLOCK8_ALPHA_MODES_FieldMask                            0xFF003311
#define GCREG_BLOCK8_ALPHA_MODES_ReadMask                             0xFF003311
#define GCREG_BLOCK8_ALPHA_MODES_WriteMask                            0xFF003311
#define GCREG_BLOCK8_ALPHA_MODES_ResetValue                           0x00000000

#define GCREG_BLOCK8_ALPHA_MODES_SRC_ALPHA_MODE                            0 : 0
#define GCREG_BLOCK8_ALPHA_MODES_SRC_ALPHA_MODE_End                            0
#define GCREG_BLOCK8_ALPHA_MODES_SRC_ALPHA_MODE_Start                          0
#define GCREG_BLOCK8_ALPHA_MODES_SRC_ALPHA_MODE_Type                         U01
#define   GCREG_BLOCK8_ALPHA_MODES_SRC_ALPHA_MODE_NORMAL                     0x0
#define   GCREG_BLOCK8_ALPHA_MODES_SRC_ALPHA_MODE_INVERSED                   0x1

#define GCREG_BLOCK8_ALPHA_MODES_DST_ALPHA_MODE                            4 : 4
#define GCREG_BLOCK8_ALPHA_MODES_DST_ALPHA_MODE_End                            4
#define GCREG_BLOCK8_ALPHA_MODES_DST_ALPHA_MODE_Start                          4
#define GCREG_BLOCK8_ALPHA_MODES_DST_ALPHA_MODE_Type                         U01
#define   GCREG_BLOCK8_ALPHA_MODES_DST_ALPHA_MODE_NORMAL                     0x0
#define   GCREG_BLOCK8_ALPHA_MODES_DST_ALPHA_MODE_INVERSED                   0x1

#define GCREG_BLOCK8_ALPHA_MODES_GLOBAL_SRC_ALPHA_MODE                     9 : 8
#define GCREG_BLOCK8_ALPHA_MODES_GLOBAL_SRC_ALPHA_MODE_End                     9
#define GCREG_BLOCK8_ALPHA_MODES_GLOBAL_SRC_ALPHA_MODE_Start                   8
#define GCREG_BLOCK8_ALPHA_MODES_GLOBAL_SRC_ALPHA_MODE_Type                  U02
#define   GCREG_BLOCK8_ALPHA_MODES_GLOBAL_SRC_ALPHA_MODE_NORMAL              0x0
#define   GCREG_BLOCK8_ALPHA_MODES_GLOBAL_SRC_ALPHA_MODE_GLOBAL              0x1
#define   GCREG_BLOCK8_ALPHA_MODES_GLOBAL_SRC_ALPHA_MODE_SCALED              0x2

#define GCREG_BLOCK8_ALPHA_MODES_GLOBAL_DST_ALPHA_MODE                   13 : 12
#define GCREG_BLOCK8_ALPHA_MODES_GLOBAL_DST_ALPHA_MODE_End                    13
#define GCREG_BLOCK8_ALPHA_MODES_GLOBAL_DST_ALPHA_MODE_Start                  12
#define GCREG_BLOCK8_ALPHA_MODES_GLOBAL_DST_ALPHA_MODE_Type                  U02
#define   GCREG_BLOCK8_ALPHA_MODES_GLOBAL_DST_ALPHA_MODE_NORMAL              0x0
#define   GCREG_BLOCK8_ALPHA_MODES_GLOBAL_DST_ALPHA_MODE_GLOBAL              0x1
#define   GCREG_BLOCK8_ALPHA_MODES_GLOBAL_DST_ALPHA_MODE_SCALED              0x2

#define GCREG_BLOCK8_ALPHA_MODES_SRC_BLENDING_MODE                       26 : 24
#define GCREG_BLOCK8_ALPHA_MODES_SRC_BLENDING_MODE_End                        26
#define GCREG_BLOCK8_ALPHA_MODES_SRC_BLENDING_MODE_Start                      24
#define GCREG_BLOCK8_ALPHA_MODES_SRC_BLENDING_MODE_Type                      U03
#define   GCREG_BLOCK8_ALPHA_MODES_SRC_BLENDING_MODE_ZERO                    0x0
#define   GCREG_BLOCK8_ALPHA_MODES_SRC_BLENDING_MODE_ONE                     0x1
#define   GCREG_BLOCK8_ALPHA_MODES_SRC_BLENDING_MODE_NORMAL                  0x2
#define   GCREG_BLOCK8_ALPHA_MODES_SRC_BLENDING_MODE_INVERSED                0x3
#define   GCREG_BLOCK8_ALPHA_MODES_SRC_BLENDING_MODE_COLOR                   0x4
#define   GCREG_BLOCK8_ALPHA_MODES_SRC_BLENDING_MODE_COLOR_INVERSED          0x5
#define   GCREG_BLOCK8_ALPHA_MODES_SRC_BLENDING_MODE_SATURATED_ALPHA         0x6
#define   GCREG_BLOCK8_ALPHA_MODES_SRC_BLENDING_MODE_SATURATED_DEST_ALPHA    0x7

/* Src Blending factor is calculate from Src alpha. */
#define GCREG_BLOCK8_ALPHA_MODES_SRC_ALPHA_FACTOR                        27 : 27
#define GCREG_BLOCK8_ALPHA_MODES_SRC_ALPHA_FACTOR_End                         27
#define GCREG_BLOCK8_ALPHA_MODES_SRC_ALPHA_FACTOR_Start                       27
#define GCREG_BLOCK8_ALPHA_MODES_SRC_ALPHA_FACTOR_Type                       U01
#define   GCREG_BLOCK8_ALPHA_MODES_SRC_ALPHA_FACTOR_DISABLED                 0x0
#define   GCREG_BLOCK8_ALPHA_MODES_SRC_ALPHA_FACTOR_ENABLED                  0x1

#define GCREG_BLOCK8_ALPHA_MODES_DST_BLENDING_MODE                       30 : 28
#define GCREG_BLOCK8_ALPHA_MODES_DST_BLENDING_MODE_End                        30
#define GCREG_BLOCK8_ALPHA_MODES_DST_BLENDING_MODE_Start                      28
#define GCREG_BLOCK8_ALPHA_MODES_DST_BLENDING_MODE_Type                      U03
#define   GCREG_BLOCK8_ALPHA_MODES_DST_BLENDING_MODE_ZERO                    0x0
#define   GCREG_BLOCK8_ALPHA_MODES_DST_BLENDING_MODE_ONE                     0x1
#define   GCREG_BLOCK8_ALPHA_MODES_DST_BLENDING_MODE_NORMAL                  0x2
#define   GCREG_BLOCK8_ALPHA_MODES_DST_BLENDING_MODE_INVERSED                0x3
#define   GCREG_BLOCK8_ALPHA_MODES_DST_BLENDING_MODE_COLOR                   0x4
#define   GCREG_BLOCK8_ALPHA_MODES_DST_BLENDING_MODE_COLOR_INVERSED          0x5
#define   GCREG_BLOCK8_ALPHA_MODES_DST_BLENDING_MODE_SATURATED_ALPHA         0x6
#define   GCREG_BLOCK8_ALPHA_MODES_DST_BLENDING_MODE_SATURATED_DEST_ALPHA    0x7

/* Dst Blending factor is calculate from Dst alpha. */
#define GCREG_BLOCK8_ALPHA_MODES_DST_ALPHA_FACTOR                        31 : 31
#define GCREG_BLOCK8_ALPHA_MODES_DST_ALPHA_FACTOR_End                         31
#define GCREG_BLOCK8_ALPHA_MODES_DST_ALPHA_FACTOR_Start                       31
#define GCREG_BLOCK8_ALPHA_MODES_DST_ALPHA_FACTOR_Type                       U01
#define   GCREG_BLOCK8_ALPHA_MODES_DST_ALPHA_FACTOR_DISABLED                 0x0
#define   GCREG_BLOCK8_ALPHA_MODES_DST_ALPHA_FACTOR_ENABLED                  0x1

/*******************************************************************************
** State gcregBlock8AddressU
*/

/* 32-bit aligned base address of the source U plane. */

#define gcregBlock8AddressURegAddrs                                       0x4AD0
#define GCREG_BLOCK8_ADDRESS_U_MSB                                            15
#define GCREG_BLOCK8_ADDRESS_U_LSB                                             3
#define GCREG_BLOCK8_ADDRESS_U_BLK                                             0
#define GCREG_BLOCK8_ADDRESS_U_Count                                           8
#define GCREG_BLOCK8_ADDRESS_U_FieldMask                              0xFFFFFFFF
#define GCREG_BLOCK8_ADDRESS_U_ReadMask                               0xFFFFFFFC
#define GCREG_BLOCK8_ADDRESS_U_WriteMask                              0xFFFFFFFC
#define GCREG_BLOCK8_ADDRESS_U_ResetValue                             0x00000000

#define GCREG_BLOCK8_ADDRESS_U_ADDRESS                                    31 : 0
#define GCREG_BLOCK8_ADDRESS_U_ADDRESS_End                                    30
#define GCREG_BLOCK8_ADDRESS_U_ADDRESS_Start                                   0
#define GCREG_BLOCK8_ADDRESS_U_ADDRESS_Type                                  U31

/*******************************************************************************
** State gcregBlock8StrideU
*/

/* Stride of the source U plane in bytes. */

#define gcregBlock8StrideURegAddrs                                        0x4AD8
#define GCREG_BLOCK8_STRIDE_U_MSB                                             15
#define GCREG_BLOCK8_STRIDE_U_LSB                                              3
#define GCREG_BLOCK8_STRIDE_U_BLK                                              0
#define GCREG_BLOCK8_STRIDE_U_Count                                            8
#define GCREG_BLOCK8_STRIDE_U_FieldMask                               0x0003FFFF
#define GCREG_BLOCK8_STRIDE_U_ReadMask                                0x0003FFFC
#define GCREG_BLOCK8_STRIDE_U_WriteMask                               0x0003FFFC
#define GCREG_BLOCK8_STRIDE_U_ResetValue                              0x00000000

#define GCREG_BLOCK8_STRIDE_U_STRIDE                                      17 : 0
#define GCREG_BLOCK8_STRIDE_U_STRIDE_End                                      17
#define GCREG_BLOCK8_STRIDE_U_STRIDE_Start                                     0
#define GCREG_BLOCK8_STRIDE_U_STRIDE_Type                                    U18

/*******************************************************************************
** State gcregBlock8AddressV
*/

/* 32-bit aligned base address of the source V plane. */

#define gcregBlock8AddressVRegAddrs                                       0x4AE0
#define GCREG_BLOCK8_ADDRESS_V_MSB                                            15
#define GCREG_BLOCK8_ADDRESS_V_LSB                                             3
#define GCREG_BLOCK8_ADDRESS_V_BLK                                             0
#define GCREG_BLOCK8_ADDRESS_V_Count                                           8
#define GCREG_BLOCK8_ADDRESS_V_FieldMask                              0xFFFFFFFF
#define GCREG_BLOCK8_ADDRESS_V_ReadMask                               0xFFFFFFFC
#define GCREG_BLOCK8_ADDRESS_V_WriteMask                              0xFFFFFFFC
#define GCREG_BLOCK8_ADDRESS_V_ResetValue                             0x00000000

#define GCREG_BLOCK8_ADDRESS_V_ADDRESS                                    31 : 0
#define GCREG_BLOCK8_ADDRESS_V_ADDRESS_End                                    30
#define GCREG_BLOCK8_ADDRESS_V_ADDRESS_Start                                   0
#define GCREG_BLOCK8_ADDRESS_V_ADDRESS_Type                                  U31

/*******************************************************************************
** State gcregBlock8StrideV
*/

/* Stride of the source V plane in bytes. */

#define gcregBlock8StrideVRegAddrs                                        0x4AE8
#define GCREG_BLOCK8_STRIDE_V_MSB                                             15
#define GCREG_BLOCK8_STRIDE_V_LSB                                              3
#define GCREG_BLOCK8_STRIDE_V_BLK                                              0
#define GCREG_BLOCK8_STRIDE_V_Count                                            8
#define GCREG_BLOCK8_STRIDE_V_FieldMask                               0x0003FFFF
#define GCREG_BLOCK8_STRIDE_V_ReadMask                                0x0003FFFC
#define GCREG_BLOCK8_STRIDE_V_WriteMask                               0x0003FFFC
#define GCREG_BLOCK8_STRIDE_V_ResetValue                              0x00000000

#define GCREG_BLOCK8_STRIDE_V_STRIDE                                      17 : 0
#define GCREG_BLOCK8_STRIDE_V_STRIDE_End                                      17
#define GCREG_BLOCK8_STRIDE_V_STRIDE_Start                                     0
#define GCREG_BLOCK8_STRIDE_V_STRIDE_Type                                    U18

/*******************************************************************************
** State gcregBlock8SrcRotationHeight
*/

/* 180/270 degree rotation configuration for the Source surface. Height field
** specifies the height of the surface in pixels.
*/

#define gcregBlock8SrcRotationHeightRegAddrs                              0x4AF0
#define GCREG_BLOCK8_SRC_ROTATION_HEIGHT_MSB                                  15
#define GCREG_BLOCK8_SRC_ROTATION_HEIGHT_LSB                                   3
#define GCREG_BLOCK8_SRC_ROTATION_HEIGHT_BLK                                   0
#define GCREG_BLOCK8_SRC_ROTATION_HEIGHT_Count                                 8
#define GCREG_BLOCK8_SRC_ROTATION_HEIGHT_FieldMask                    0x0000FFFF
#define GCREG_BLOCK8_SRC_ROTATION_HEIGHT_ReadMask                     0x0000FFFF
#define GCREG_BLOCK8_SRC_ROTATION_HEIGHT_WriteMask                    0x0000FFFF
#define GCREG_BLOCK8_SRC_ROTATION_HEIGHT_ResetValue                   0x00000000

#define GCREG_BLOCK8_SRC_ROTATION_HEIGHT_HEIGHT                           15 : 0
#define GCREG_BLOCK8_SRC_ROTATION_HEIGHT_HEIGHT_End                           15
#define GCREG_BLOCK8_SRC_ROTATION_HEIGHT_HEIGHT_Start                          0
#define GCREG_BLOCK8_SRC_ROTATION_HEIGHT_HEIGHT_Type                         U16

/*******************************************************************************
** State gcregBlock8RotAngle
*/

/* 0/90/180/270 degree rotation configuration for the Source surface. Height
** field specifies the height of the surface in pixels.
*/

#define gcregBlock8RotAngleRegAddrs                                       0x4AF8
#define GCREG_BLOCK8_ROT_ANGLE_MSB                                            15
#define GCREG_BLOCK8_ROT_ANGLE_LSB                                             3
#define GCREG_BLOCK8_ROT_ANGLE_BLK                                             0
#define GCREG_BLOCK8_ROT_ANGLE_Count                                           8
#define GCREG_BLOCK8_ROT_ANGLE_FieldMask                              0x000BB33F
#define GCREG_BLOCK8_ROT_ANGLE_ReadMask                               0x000BB33F
#define GCREG_BLOCK8_ROT_ANGLE_WriteMask                              0x000BB33F
#define GCREG_BLOCK8_ROT_ANGLE_ResetValue                             0x00000000

#define GCREG_BLOCK8_ROT_ANGLE_SRC                                         2 : 0
#define GCREG_BLOCK8_ROT_ANGLE_SRC_End                                         2
#define GCREG_BLOCK8_ROT_ANGLE_SRC_Start                                       0
#define GCREG_BLOCK8_ROT_ANGLE_SRC_Type                                      U03
#define   GCREG_BLOCK8_ROT_ANGLE_SRC_ROT0                                    0x0
#define   GCREG_BLOCK8_ROT_ANGLE_SRC_FLIP_X                                  0x1
#define   GCREG_BLOCK8_ROT_ANGLE_SRC_FLIP_Y                                  0x2
#define   GCREG_BLOCK8_ROT_ANGLE_SRC_ROT90                                   0x4
#define   GCREG_BLOCK8_ROT_ANGLE_SRC_ROT180                                  0x5
#define   GCREG_BLOCK8_ROT_ANGLE_SRC_ROT270                                  0x6

#define GCREG_BLOCK8_ROT_ANGLE_DST                                         5 : 3
#define GCREG_BLOCK8_ROT_ANGLE_DST_End                                         5
#define GCREG_BLOCK8_ROT_ANGLE_DST_Start                                       3
#define GCREG_BLOCK8_ROT_ANGLE_DST_Type                                      U03
#define   GCREG_BLOCK8_ROT_ANGLE_DST_ROT0                                    0x0
#define   GCREG_BLOCK8_ROT_ANGLE_DST_FLIP_X                                  0x1
#define   GCREG_BLOCK8_ROT_ANGLE_DST_FLIP_Y                                  0x2
#define   GCREG_BLOCK8_ROT_ANGLE_DST_ROT90                                   0x4
#define   GCREG_BLOCK8_ROT_ANGLE_DST_ROT180                                  0x5
#define   GCREG_BLOCK8_ROT_ANGLE_DST_ROT270                                  0x6

#define GCREG_BLOCK8_ROT_ANGLE_MASK_SRC                                    8 : 8
#define GCREG_BLOCK8_ROT_ANGLE_MASK_SRC_End                                    8
#define GCREG_BLOCK8_ROT_ANGLE_MASK_SRC_Start                                  8
#define GCREG_BLOCK8_ROT_ANGLE_MASK_SRC_Type                                 U01
#define   GCREG_BLOCK8_ROT_ANGLE_MASK_SRC_ENABLED                            0x0
#define   GCREG_BLOCK8_ROT_ANGLE_MASK_SRC_MASKED                             0x1

#define GCREG_BLOCK8_ROT_ANGLE_MASK_DST                                    9 : 9
#define GCREG_BLOCK8_ROT_ANGLE_MASK_DST_End                                    9
#define GCREG_BLOCK8_ROT_ANGLE_MASK_DST_Start                                  9
#define GCREG_BLOCK8_ROT_ANGLE_MASK_DST_Type                                 U01
#define   GCREG_BLOCK8_ROT_ANGLE_MASK_DST_ENABLED                            0x0
#define   GCREG_BLOCK8_ROT_ANGLE_MASK_DST_MASKED                             0x1

#define GCREG_BLOCK8_ROT_ANGLE_SRC_MIRROR                                13 : 12
#define GCREG_BLOCK8_ROT_ANGLE_SRC_MIRROR_End                                 13
#define GCREG_BLOCK8_ROT_ANGLE_SRC_MIRROR_Start                               12
#define GCREG_BLOCK8_ROT_ANGLE_SRC_MIRROR_Type                               U02
#define   GCREG_BLOCK8_ROT_ANGLE_SRC_MIRROR_NONE                             0x0
#define   GCREG_BLOCK8_ROT_ANGLE_SRC_MIRROR_MIRROR_X                         0x1
#define   GCREG_BLOCK8_ROT_ANGLE_SRC_MIRROR_MIRROR_Y                         0x2
#define   GCREG_BLOCK8_ROT_ANGLE_SRC_MIRROR_MIRROR_XY                        0x3

#define GCREG_BLOCK8_ROT_ANGLE_MASK_SRC_MIRROR                           15 : 15
#define GCREG_BLOCK8_ROT_ANGLE_MASK_SRC_MIRROR_End                            15
#define GCREG_BLOCK8_ROT_ANGLE_MASK_SRC_MIRROR_Start                          15
#define GCREG_BLOCK8_ROT_ANGLE_MASK_SRC_MIRROR_Type                          U01
#define   GCREG_BLOCK8_ROT_ANGLE_MASK_SRC_MIRROR_ENABLED                     0x0
#define   GCREG_BLOCK8_ROT_ANGLE_MASK_SRC_MIRROR_MASKED                      0x1

#define GCREG_BLOCK8_ROT_ANGLE_DST_MIRROR                                17 : 16
#define GCREG_BLOCK8_ROT_ANGLE_DST_MIRROR_End                                 17
#define GCREG_BLOCK8_ROT_ANGLE_DST_MIRROR_Start                               16
#define GCREG_BLOCK8_ROT_ANGLE_DST_MIRROR_Type                               U02
#define   GCREG_BLOCK8_ROT_ANGLE_DST_MIRROR_NONE                             0x0
#define   GCREG_BLOCK8_ROT_ANGLE_DST_MIRROR_MIRROR_X                         0x1
#define   GCREG_BLOCK8_ROT_ANGLE_DST_MIRROR_MIRROR_Y                         0x2
#define   GCREG_BLOCK8_ROT_ANGLE_DST_MIRROR_MIRROR_XY                        0x3

#define GCREG_BLOCK8_ROT_ANGLE_MASK_DST_MIRROR                           19 : 19
#define GCREG_BLOCK8_ROT_ANGLE_MASK_DST_MIRROR_End                            19
#define GCREG_BLOCK8_ROT_ANGLE_MASK_DST_MIRROR_Start                          19
#define GCREG_BLOCK8_ROT_ANGLE_MASK_DST_MIRROR_Type                          U01
#define   GCREG_BLOCK8_ROT_ANGLE_MASK_DST_MIRROR_ENABLED                     0x0
#define   GCREG_BLOCK8_ROT_ANGLE_MASK_DST_MIRROR_MASKED                      0x1

/*******************************************************************************
** State gcregBlock8GlobalSrcColor
*/

/* Defines the global source color and alpha values. */

#define gcregBlock8GlobalSrcColorRegAddrs                                 0x4B00
#define GCREG_BLOCK8_GLOBAL_SRC_COLOR_MSB                                     15
#define GCREG_BLOCK8_GLOBAL_SRC_COLOR_LSB                                      3
#define GCREG_BLOCK8_GLOBAL_SRC_COLOR_BLK                                      0
#define GCREG_BLOCK8_GLOBAL_SRC_COLOR_Count                                    8
#define GCREG_BLOCK8_GLOBAL_SRC_COLOR_FieldMask                       0xFFFFFFFF
#define GCREG_BLOCK8_GLOBAL_SRC_COLOR_ReadMask                        0xFFFFFFFF
#define GCREG_BLOCK8_GLOBAL_SRC_COLOR_WriteMask                       0xFFFFFFFF
#define GCREG_BLOCK8_GLOBAL_SRC_COLOR_ResetValue                      0x00000000

#define GCREG_BLOCK8_GLOBAL_SRC_COLOR_ALPHA                              31 : 24
#define GCREG_BLOCK8_GLOBAL_SRC_COLOR_ALPHA_End                               31
#define GCREG_BLOCK8_GLOBAL_SRC_COLOR_ALPHA_Start                             24
#define GCREG_BLOCK8_GLOBAL_SRC_COLOR_ALPHA_Type                             U08

#define GCREG_BLOCK8_GLOBAL_SRC_COLOR_RED                                23 : 16
#define GCREG_BLOCK8_GLOBAL_SRC_COLOR_RED_End                                 23
#define GCREG_BLOCK8_GLOBAL_SRC_COLOR_RED_Start                               16
#define GCREG_BLOCK8_GLOBAL_SRC_COLOR_RED_Type                               U08

#define GCREG_BLOCK8_GLOBAL_SRC_COLOR_GREEN                               15 : 8
#define GCREG_BLOCK8_GLOBAL_SRC_COLOR_GREEN_End                               15
#define GCREG_BLOCK8_GLOBAL_SRC_COLOR_GREEN_Start                              8
#define GCREG_BLOCK8_GLOBAL_SRC_COLOR_GREEN_Type                             U08

#define GCREG_BLOCK8_GLOBAL_SRC_COLOR_BLUE                                 7 : 0
#define GCREG_BLOCK8_GLOBAL_SRC_COLOR_BLUE_End                                 7
#define GCREG_BLOCK8_GLOBAL_SRC_COLOR_BLUE_Start                               0
#define GCREG_BLOCK8_GLOBAL_SRC_COLOR_BLUE_Type                              U08

/*******************************************************************************
** State gcregBlock8GlobalDestColor
*/

/* Defines the global destination color and alpha values. */

#define gcregBlock8GlobalDestColorRegAddrs                                0x4B08
#define GCREG_BLOCK8_GLOBAL_DEST_COLOR_MSB                                    15
#define GCREG_BLOCK8_GLOBAL_DEST_COLOR_LSB                                     3
#define GCREG_BLOCK8_GLOBAL_DEST_COLOR_BLK                                     0
#define GCREG_BLOCK8_GLOBAL_DEST_COLOR_Count                                   8
#define GCREG_BLOCK8_GLOBAL_DEST_COLOR_FieldMask                      0xFFFFFFFF
#define GCREG_BLOCK8_GLOBAL_DEST_COLOR_ReadMask                       0xFFFFFFFF
#define GCREG_BLOCK8_GLOBAL_DEST_COLOR_WriteMask                      0xFFFFFFFF
#define GCREG_BLOCK8_GLOBAL_DEST_COLOR_ResetValue                     0x00000000

#define GCREG_BLOCK8_GLOBAL_DEST_COLOR_ALPHA                             31 : 24
#define GCREG_BLOCK8_GLOBAL_DEST_COLOR_ALPHA_End                              31
#define GCREG_BLOCK8_GLOBAL_DEST_COLOR_ALPHA_Start                            24
#define GCREG_BLOCK8_GLOBAL_DEST_COLOR_ALPHA_Type                            U08

#define GCREG_BLOCK8_GLOBAL_DEST_COLOR_RED                               23 : 16
#define GCREG_BLOCK8_GLOBAL_DEST_COLOR_RED_End                                23
#define GCREG_BLOCK8_GLOBAL_DEST_COLOR_RED_Start                              16
#define GCREG_BLOCK8_GLOBAL_DEST_COLOR_RED_Type                              U08

#define GCREG_BLOCK8_GLOBAL_DEST_COLOR_GREEN                              15 : 8
#define GCREG_BLOCK8_GLOBAL_DEST_COLOR_GREEN_End                              15
#define GCREG_BLOCK8_GLOBAL_DEST_COLOR_GREEN_Start                             8
#define GCREG_BLOCK8_GLOBAL_DEST_COLOR_GREEN_Type                            U08

#define GCREG_BLOCK8_GLOBAL_DEST_COLOR_BLUE                                7 : 0
#define GCREG_BLOCK8_GLOBAL_DEST_COLOR_BLUE_End                                7
#define GCREG_BLOCK8_GLOBAL_DEST_COLOR_BLUE_Start                              0
#define GCREG_BLOCK8_GLOBAL_DEST_COLOR_BLUE_Type                             U08

/*******************************************************************************
** State gcregBlock8ColorMultiplyModes
*/

/* Color modes to multiply Source or Destination pixel color by alpha
** channel. Alpha can be from global color source or current pixel.
*/

#define gcregBlock8ColorMultiplyModesRegAddrs                             0x4B10
#define GCREG_BLOCK8_COLOR_MULTIPLY_MODES_MSB                                 15
#define GCREG_BLOCK8_COLOR_MULTIPLY_MODES_LSB                                  3
#define GCREG_BLOCK8_COLOR_MULTIPLY_MODES_BLK                                  0
#define GCREG_BLOCK8_COLOR_MULTIPLY_MODES_Count                                8
#define GCREG_BLOCK8_COLOR_MULTIPLY_MODES_FieldMask                   0x00100311
#define GCREG_BLOCK8_COLOR_MULTIPLY_MODES_ReadMask                    0x00100311
#define GCREG_BLOCK8_COLOR_MULTIPLY_MODES_WriteMask                   0x00100311
#define GCREG_BLOCK8_COLOR_MULTIPLY_MODES_ResetValue                  0x00000000

#define GCREG_BLOCK8_COLOR_MULTIPLY_MODES_SRC_PREMULTIPLY                  0 : 0
#define GCREG_BLOCK8_COLOR_MULTIPLY_MODES_SRC_PREMULTIPLY_End                  0
#define GCREG_BLOCK8_COLOR_MULTIPLY_MODES_SRC_PREMULTIPLY_Start                0
#define GCREG_BLOCK8_COLOR_MULTIPLY_MODES_SRC_PREMULTIPLY_Type               U01
#define   GCREG_BLOCK8_COLOR_MULTIPLY_MODES_SRC_PREMULTIPLY_DISABLE          0x0
#define   GCREG_BLOCK8_COLOR_MULTIPLY_MODES_SRC_PREMULTIPLY_ENABLE           0x1

#define GCREG_BLOCK8_COLOR_MULTIPLY_MODES_DST_PREMULTIPLY                  4 : 4
#define GCREG_BLOCK8_COLOR_MULTIPLY_MODES_DST_PREMULTIPLY_End                  4
#define GCREG_BLOCK8_COLOR_MULTIPLY_MODES_DST_PREMULTIPLY_Start                4
#define GCREG_BLOCK8_COLOR_MULTIPLY_MODES_DST_PREMULTIPLY_Type               U01
#define   GCREG_BLOCK8_COLOR_MULTIPLY_MODES_DST_PREMULTIPLY_DISABLE          0x0
#define   GCREG_BLOCK8_COLOR_MULTIPLY_MODES_DST_PREMULTIPLY_ENABLE           0x1

#define GCREG_BLOCK8_COLOR_MULTIPLY_MODES_SRC_GLOBAL_PREMULTIPLY           9 : 8
#define GCREG_BLOCK8_COLOR_MULTIPLY_MODES_SRC_GLOBAL_PREMULTIPLY_End           9
#define GCREG_BLOCK8_COLOR_MULTIPLY_MODES_SRC_GLOBAL_PREMULTIPLY_Start         8
#define GCREG_BLOCK8_COLOR_MULTIPLY_MODES_SRC_GLOBAL_PREMULTIPLY_Type        U02
#define   GCREG_BLOCK8_COLOR_MULTIPLY_MODES_SRC_GLOBAL_PREMULTIPLY_DISABLE   0x0
#define   GCREG_BLOCK8_COLOR_MULTIPLY_MODES_SRC_GLOBAL_PREMULTIPLY_ALPHA     0x1
#define   GCREG_BLOCK8_COLOR_MULTIPLY_MODES_SRC_GLOBAL_PREMULTIPLY_COLOR     0x2

#define GCREG_BLOCK8_COLOR_MULTIPLY_MODES_DST_DEMULTIPLY                 20 : 20
#define GCREG_BLOCK8_COLOR_MULTIPLY_MODES_DST_DEMULTIPLY_End                  20
#define GCREG_BLOCK8_COLOR_MULTIPLY_MODES_DST_DEMULTIPLY_Start                20
#define GCREG_BLOCK8_COLOR_MULTIPLY_MODES_DST_DEMULTIPLY_Type                U01
#define   GCREG_BLOCK8_COLOR_MULTIPLY_MODES_DST_DEMULTIPLY_DISABLE           0x0
#define   GCREG_BLOCK8_COLOR_MULTIPLY_MODES_DST_DEMULTIPLY_ENABLE            0x1

/*******************************************************************************
** State gcregBlock8Transparency
*/

#define gcregBlock8TransparencyRegAddrs                                   0x4B18
#define GCREG_BLOCK8_TRANSPARENCY_MSB                                         15
#define GCREG_BLOCK8_TRANSPARENCY_LSB                                          3
#define GCREG_BLOCK8_TRANSPARENCY_BLK                                          0
#define GCREG_BLOCK8_TRANSPARENCY_Count                                        8
#define GCREG_BLOCK8_TRANSPARENCY_FieldMask                           0xB3331333
#define GCREG_BLOCK8_TRANSPARENCY_ReadMask                            0xB3331333
#define GCREG_BLOCK8_TRANSPARENCY_WriteMask                           0xB3331333
#define GCREG_BLOCK8_TRANSPARENCY_ResetValue                          0x00000000

/* Source transparency mode. */
#define GCREG_BLOCK8_TRANSPARENCY_SOURCE                                   1 : 0
#define GCREG_BLOCK8_TRANSPARENCY_SOURCE_End                                   1
#define GCREG_BLOCK8_TRANSPARENCY_SOURCE_Start                                 0
#define GCREG_BLOCK8_TRANSPARENCY_SOURCE_Type                                U02
#define   GCREG_BLOCK8_TRANSPARENCY_SOURCE_OPAQUE                            0x0
#define   GCREG_BLOCK8_TRANSPARENCY_SOURCE_MASK                              0x1
#define   GCREG_BLOCK8_TRANSPARENCY_SOURCE_KEY                               0x2

/* Pattern transparency mode. KEY transparency mode is reserved. */
#define GCREG_BLOCK8_TRANSPARENCY_PATTERN                                  5 : 4
#define GCREG_BLOCK8_TRANSPARENCY_PATTERN_End                                  5
#define GCREG_BLOCK8_TRANSPARENCY_PATTERN_Start                                4
#define GCREG_BLOCK8_TRANSPARENCY_PATTERN_Type                               U02
#define   GCREG_BLOCK8_TRANSPARENCY_PATTERN_OPAQUE                           0x0
#define   GCREG_BLOCK8_TRANSPARENCY_PATTERN_MASK                             0x1
#define   GCREG_BLOCK8_TRANSPARENCY_PATTERN_KEY                              0x2

/* Destination transparency mode. MASK transparency mode is reserved. */
#define GCREG_BLOCK8_TRANSPARENCY_DESTINATION                              9 : 8
#define GCREG_BLOCK8_TRANSPARENCY_DESTINATION_End                              9
#define GCREG_BLOCK8_TRANSPARENCY_DESTINATION_Start                            8
#define GCREG_BLOCK8_TRANSPARENCY_DESTINATION_Type                           U02
#define   GCREG_BLOCK8_TRANSPARENCY_DESTINATION_OPAQUE                       0x0
#define   GCREG_BLOCK8_TRANSPARENCY_DESTINATION_MASK                         0x1
#define   GCREG_BLOCK8_TRANSPARENCY_DESTINATION_KEY                          0x2

/* Mask field for Source/Pattern/Destination fields. */
#define GCREG_BLOCK8_TRANSPARENCY_MASK_TRANSPARENCY                      12 : 12
#define GCREG_BLOCK8_TRANSPARENCY_MASK_TRANSPARENCY_End                       12
#define GCREG_BLOCK8_TRANSPARENCY_MASK_TRANSPARENCY_Start                     12
#define GCREG_BLOCK8_TRANSPARENCY_MASK_TRANSPARENCY_Type                     U01
#define   GCREG_BLOCK8_TRANSPARENCY_MASK_TRANSPARENCY_ENABLED                0x0
#define   GCREG_BLOCK8_TRANSPARENCY_MASK_TRANSPARENCY_MASKED                 0x1

/* Source usage override. */
#define GCREG_BLOCK8_TRANSPARENCY_USE_SRC_OVERRIDE                       17 : 16
#define GCREG_BLOCK8_TRANSPARENCY_USE_SRC_OVERRIDE_End                        17
#define GCREG_BLOCK8_TRANSPARENCY_USE_SRC_OVERRIDE_Start                      16
#define GCREG_BLOCK8_TRANSPARENCY_USE_SRC_OVERRIDE_Type                      U02
#define   GCREG_BLOCK8_TRANSPARENCY_USE_SRC_OVERRIDE_DEFAULT                 0x0
#define   GCREG_BLOCK8_TRANSPARENCY_USE_SRC_OVERRIDE_USE_ENABLE              0x1
#define   GCREG_BLOCK8_TRANSPARENCY_USE_SRC_OVERRIDE_USE_DISABLE             0x2

/* Pattern usage override. */
#define GCREG_BLOCK8_TRANSPARENCY_USE_PAT_OVERRIDE                       21 : 20
#define GCREG_BLOCK8_TRANSPARENCY_USE_PAT_OVERRIDE_End                        21
#define GCREG_BLOCK8_TRANSPARENCY_USE_PAT_OVERRIDE_Start                      20
#define GCREG_BLOCK8_TRANSPARENCY_USE_PAT_OVERRIDE_Type                      U02
#define   GCREG_BLOCK8_TRANSPARENCY_USE_PAT_OVERRIDE_DEFAULT                 0x0
#define   GCREG_BLOCK8_TRANSPARENCY_USE_PAT_OVERRIDE_USE_ENABLE              0x1
#define   GCREG_BLOCK8_TRANSPARENCY_USE_PAT_OVERRIDE_USE_DISABLE             0x2

/* Destination usage override. */
#define GCREG_BLOCK8_TRANSPARENCY_USE_DST_OVERRIDE                       25 : 24
#define GCREG_BLOCK8_TRANSPARENCY_USE_DST_OVERRIDE_End                        25
#define GCREG_BLOCK8_TRANSPARENCY_USE_DST_OVERRIDE_Start                      24
#define GCREG_BLOCK8_TRANSPARENCY_USE_DST_OVERRIDE_Type                      U02
#define   GCREG_BLOCK8_TRANSPARENCY_USE_DST_OVERRIDE_DEFAULT                 0x0
#define   GCREG_BLOCK8_TRANSPARENCY_USE_DST_OVERRIDE_USE_ENABLE              0x1
#define   GCREG_BLOCK8_TRANSPARENCY_USE_DST_OVERRIDE_USE_DISABLE             0x2

/* 2D resource usage override mask field. */
#define GCREG_BLOCK8_TRANSPARENCY_MASK_RESOURCE_OVERRIDE                 28 : 28
#define GCREG_BLOCK8_TRANSPARENCY_MASK_RESOURCE_OVERRIDE_End                  28
#define GCREG_BLOCK8_TRANSPARENCY_MASK_RESOURCE_OVERRIDE_Start                28
#define GCREG_BLOCK8_TRANSPARENCY_MASK_RESOURCE_OVERRIDE_Type                U01
#define   GCREG_BLOCK8_TRANSPARENCY_MASK_RESOURCE_OVERRIDE_ENABLED           0x0
#define   GCREG_BLOCK8_TRANSPARENCY_MASK_RESOURCE_OVERRIDE_MASKED            0x1

/* DFB Color Key. */
#define GCREG_BLOCK8_TRANSPARENCY_DFB_COLOR_KEY                          29 : 29
#define GCREG_BLOCK8_TRANSPARENCY_DFB_COLOR_KEY_End                           29
#define GCREG_BLOCK8_TRANSPARENCY_DFB_COLOR_KEY_Start                         29
#define GCREG_BLOCK8_TRANSPARENCY_DFB_COLOR_KEY_Type                         U01
#define   GCREG_BLOCK8_TRANSPARENCY_DFB_COLOR_KEY_DISABLED                   0x0
#define   GCREG_BLOCK8_TRANSPARENCY_DFB_COLOR_KEY_ENABLED                    0x1

#define GCREG_BLOCK8_TRANSPARENCY_MASK_DFB_COLOR_KEY                     31 : 31
#define GCREG_BLOCK8_TRANSPARENCY_MASK_DFB_COLOR_KEY_End                      31
#define GCREG_BLOCK8_TRANSPARENCY_MASK_DFB_COLOR_KEY_Start                    31
#define GCREG_BLOCK8_TRANSPARENCY_MASK_DFB_COLOR_KEY_Type                    U01
#define   GCREG_BLOCK8_TRANSPARENCY_MASK_DFB_COLOR_KEY_ENABLED               0x0
#define   GCREG_BLOCK8_TRANSPARENCY_MASK_DFB_COLOR_KEY_MASKED                0x1

/*******************************************************************************
** State gcregBlock8Control
*/

/* General purpose control register. */

#define gcregBlock8ControlRegAddrs                                        0x4B20
#define GCREG_BLOCK8_CONTROL_MSB                                              15
#define GCREG_BLOCK8_CONTROL_LSB                                               3
#define GCREG_BLOCK8_CONTROL_BLK                                               0
#define GCREG_BLOCK8_CONTROL_Count                                             8
#define GCREG_BLOCK8_CONTROL_FieldMask                                0x00000999
#define GCREG_BLOCK8_CONTROL_ReadMask                                 0x00000999
#define GCREG_BLOCK8_CONTROL_WriteMask                                0x00000999
#define GCREG_BLOCK8_CONTROL_ResetValue                               0x00000000

#define GCREG_BLOCK8_CONTROL_YUV                                           0 : 0
#define GCREG_BLOCK8_CONTROL_YUV_End                                           0
#define GCREG_BLOCK8_CONTROL_YUV_Start                                         0
#define GCREG_BLOCK8_CONTROL_YUV_Type                                        U01
#define   GCREG_BLOCK8_CONTROL_YUV_601                                       0x0
#define   GCREG_BLOCK8_CONTROL_YUV_709                                       0x1

#define GCREG_BLOCK8_CONTROL_MASK_YUV                                      3 : 3
#define GCREG_BLOCK8_CONTROL_MASK_YUV_End                                      3
#define GCREG_BLOCK8_CONTROL_MASK_YUV_Start                                    3
#define GCREG_BLOCK8_CONTROL_MASK_YUV_Type                                   U01
#define   GCREG_BLOCK8_CONTROL_MASK_YUV_ENABLED                              0x0
#define   GCREG_BLOCK8_CONTROL_MASK_YUV_MASKED                               0x1

#define GCREG_BLOCK8_CONTROL_UV_SWIZZLE                                    4 : 4
#define GCREG_BLOCK8_CONTROL_UV_SWIZZLE_End                                    4
#define GCREG_BLOCK8_CONTROL_UV_SWIZZLE_Start                                  4
#define GCREG_BLOCK8_CONTROL_UV_SWIZZLE_Type                                 U01
#define   GCREG_BLOCK8_CONTROL_UV_SWIZZLE_UV                                 0x0
#define   GCREG_BLOCK8_CONTROL_UV_SWIZZLE_VU                                 0x1

#define GCREG_BLOCK8_CONTROL_MASK_UV_SWIZZLE                               7 : 7
#define GCREG_BLOCK8_CONTROL_MASK_UV_SWIZZLE_End                               7
#define GCREG_BLOCK8_CONTROL_MASK_UV_SWIZZLE_Start                             7
#define GCREG_BLOCK8_CONTROL_MASK_UV_SWIZZLE_Type                            U01
#define   GCREG_BLOCK8_CONTROL_MASK_UV_SWIZZLE_ENABLED                       0x0
#define   GCREG_BLOCK8_CONTROL_MASK_UV_SWIZZLE_MASKED                        0x1

/* YUV to RGB convert enable */
#define GCREG_BLOCK8_CONTROL_YUVRGB                                        8 : 8
#define GCREG_BLOCK8_CONTROL_YUVRGB_End                                        8
#define GCREG_BLOCK8_CONTROL_YUVRGB_Start                                      8
#define GCREG_BLOCK8_CONTROL_YUVRGB_Type                                     U01
#define   GCREG_BLOCK8_CONTROL_YUVRGB_DISABLED                               0x0
#define   GCREG_BLOCK8_CONTROL_YUVRGB_ENABLED                                0x1

#define GCREG_BLOCK8_CONTROL_MASK_YUVRGB                                 11 : 11
#define GCREG_BLOCK8_CONTROL_MASK_YUVRGB_End                                  11
#define GCREG_BLOCK8_CONTROL_MASK_YUVRGB_Start                                11
#define GCREG_BLOCK8_CONTROL_MASK_YUVRGB_Type                                U01
#define   GCREG_BLOCK8_CONTROL_MASK_YUVRGB_ENABLED                           0x0
#define   GCREG_BLOCK8_CONTROL_MASK_YUVRGB_MASKED                            0x1

/*******************************************************************************
** State gcregBlock8SrcColorKeyHigh
*/

/* Defines the source transparency color in source format. */

#define gcregBlock8SrcColorKeyHighRegAddrs                                0x4B28
#define GCREG_BLOCK8_SRC_COLOR_KEY_HIGH_MSB                                   15
#define GCREG_BLOCK8_SRC_COLOR_KEY_HIGH_LSB                                    3
#define GCREG_BLOCK8_SRC_COLOR_KEY_HIGH_BLK                                    0
#define GCREG_BLOCK8_SRC_COLOR_KEY_HIGH_Count                                  8
#define GCREG_BLOCK8_SRC_COLOR_KEY_HIGH_FieldMask                     0xFFFFFFFF
#define GCREG_BLOCK8_SRC_COLOR_KEY_HIGH_ReadMask                      0xFFFFFFFF
#define GCREG_BLOCK8_SRC_COLOR_KEY_HIGH_WriteMask                     0xFFFFFFFF
#define GCREG_BLOCK8_SRC_COLOR_KEY_HIGH_ResetValue                    0x00000000

#define GCREG_BLOCK8_SRC_COLOR_KEY_HIGH_ALPHA                            31 : 24
#define GCREG_BLOCK8_SRC_COLOR_KEY_HIGH_ALPHA_End                             31
#define GCREG_BLOCK8_SRC_COLOR_KEY_HIGH_ALPHA_Start                           24
#define GCREG_BLOCK8_SRC_COLOR_KEY_HIGH_ALPHA_Type                           U08

#define GCREG_BLOCK8_SRC_COLOR_KEY_HIGH_RED                              23 : 16
#define GCREG_BLOCK8_SRC_COLOR_KEY_HIGH_RED_End                               23
#define GCREG_BLOCK8_SRC_COLOR_KEY_HIGH_RED_Start                             16
#define GCREG_BLOCK8_SRC_COLOR_KEY_HIGH_RED_Type                             U08

#define GCREG_BLOCK8_SRC_COLOR_KEY_HIGH_GREEN                             15 : 8
#define GCREG_BLOCK8_SRC_COLOR_KEY_HIGH_GREEN_End                             15
#define GCREG_BLOCK8_SRC_COLOR_KEY_HIGH_GREEN_Start                            8
#define GCREG_BLOCK8_SRC_COLOR_KEY_HIGH_GREEN_Type                           U08

#define GCREG_BLOCK8_SRC_COLOR_KEY_HIGH_BLUE                               7 : 0
#define GCREG_BLOCK8_SRC_COLOR_KEY_HIGH_BLUE_End                               7
#define GCREG_BLOCK8_SRC_COLOR_KEY_HIGH_BLUE_Start                             0
#define GCREG_BLOCK8_SRC_COLOR_KEY_HIGH_BLUE_Type                            U08

/*******************************************************************************
** State gcregBlock8SrcExConfig
*/

#define gcregBlock8SrcExConfigRegAddrs                                    0x4B30
#define GCREG_BLOCK8_SRC_EX_CONFIG_MSB                                        15
#define GCREG_BLOCK8_SRC_EX_CONFIG_LSB                                         3
#define GCREG_BLOCK8_SRC_EX_CONFIG_BLK                                         0
#define GCREG_BLOCK8_SRC_EX_CONFIG_Count                                       8
#define GCREG_BLOCK8_SRC_EX_CONFIG_FieldMask                          0x00000109
#define GCREG_BLOCK8_SRC_EX_CONFIG_ReadMask                           0x00000109
#define GCREG_BLOCK8_SRC_EX_CONFIG_WriteMask                          0x00000109
#define GCREG_BLOCK8_SRC_EX_CONFIG_ResetValue                         0x00000000

/* Source multi tiled address computation control. */
#define GCREG_BLOCK8_SRC_EX_CONFIG_MULTI_TILED                             0 : 0
#define GCREG_BLOCK8_SRC_EX_CONFIG_MULTI_TILED_End                             0
#define GCREG_BLOCK8_SRC_EX_CONFIG_MULTI_TILED_Start                           0
#define GCREG_BLOCK8_SRC_EX_CONFIG_MULTI_TILED_Type                          U01
#define   GCREG_BLOCK8_SRC_EX_CONFIG_MULTI_TILED_DISABLED                    0x0
#define   GCREG_BLOCK8_SRC_EX_CONFIG_MULTI_TILED_ENABLED                     0x1

/* Source super tiled address computation control. */
#define GCREG_BLOCK8_SRC_EX_CONFIG_SUPER_TILED                             3 : 3
#define GCREG_BLOCK8_SRC_EX_CONFIG_SUPER_TILED_End                             3
#define GCREG_BLOCK8_SRC_EX_CONFIG_SUPER_TILED_Start                           3
#define GCREG_BLOCK8_SRC_EX_CONFIG_SUPER_TILED_Type                          U01
#define   GCREG_BLOCK8_SRC_EX_CONFIG_SUPER_TILED_DISABLED                    0x0
#define   GCREG_BLOCK8_SRC_EX_CONFIG_SUPER_TILED_ENABLED                     0x1

/* Source super tiled address computation control. */
#define GCREG_BLOCK8_SRC_EX_CONFIG_MINOR_TILED                             8 : 8
#define GCREG_BLOCK8_SRC_EX_CONFIG_MINOR_TILED_End                             8
#define GCREG_BLOCK8_SRC_EX_CONFIG_MINOR_TILED_Start                           8
#define GCREG_BLOCK8_SRC_EX_CONFIG_MINOR_TILED_Type                          U01
#define   GCREG_BLOCK8_SRC_EX_CONFIG_MINOR_TILED_DISABLED                    0x0
#define   GCREG_BLOCK8_SRC_EX_CONFIG_MINOR_TILED_ENABLED                     0x1

/*******************************************************************************
** State gcregBlock8SrcExAddress
*/

/* 32-bit aligned base address of the source extra surface. */

#define gcregBlock8SrcExAddressRegAddrs                                   0x4B38
#define GCREG_BLOCK8_SRC_EX_ADDRESS_MSB                                       15
#define GCREG_BLOCK8_SRC_EX_ADDRESS_LSB                                        3
#define GCREG_BLOCK8_SRC_EX_ADDRESS_BLK                                        0
#define GCREG_BLOCK8_SRC_EX_ADDRESS_Count                                      8
#define GCREG_BLOCK8_SRC_EX_ADDRESS_FieldMask                         0xFFFFFFFF
#define GCREG_BLOCK8_SRC_EX_ADDRESS_ReadMask                          0xFFFFFFFC
#define GCREG_BLOCK8_SRC_EX_ADDRESS_WriteMask                         0xFFFFFFFC
#define GCREG_BLOCK8_SRC_EX_ADDRESS_ResetValue                        0x00000000

#define GCREG_BLOCK8_SRC_EX_ADDRESS_ADDRESS                               31 : 0
#define GCREG_BLOCK8_SRC_EX_ADDRESS_ADDRESS_End                               30
#define GCREG_BLOCK8_SRC_EX_ADDRESS_ADDRESS_Start                              0
#define GCREG_BLOCK8_SRC_EX_ADDRESS_ADDRESS_Type                             U31

/*******************************************************************************
** Generic defines
*/

#define GCREG_FORMAT_SUB_SAMPLE_MODE_YUV_MODE422                             0x0
#define GCREG_FORMAT_SUB_SAMPLE_MODE_YUV_MODE420                             0x1

#define GCREG_DE_SWIZZLE_ARGB                                                0x0
#define GCREG_DE_SWIZZLE_RGBA                                                0x1
#define GCREG_DE_SWIZZLE_ABGR                                                0x2
#define GCREG_DE_SWIZZLE_BGRA                                                0x3

#define GCREG_DE_FORMAT_X4R4G4B4                                            0x00
#define GCREG_DE_FORMAT_A4R4G4B4                                            0x01
#define GCREG_DE_FORMAT_X1R5G5B5                                            0x02
#define GCREG_DE_FORMAT_A1R5G5B5                                            0x03
#define GCREG_DE_FORMAT_R5G6B5                                              0x04
#define GCREG_DE_FORMAT_X8R8G8B8                                            0x05
#define GCREG_DE_FORMAT_A8R8G8B8                                            0x06
#define GCREG_DE_FORMAT_YUY2                                                0x07
#define GCREG_DE_FORMAT_UYVY                                                0x08
#define GCREG_DE_FORMAT_INDEX8                                              0x09
#define GCREG_DE_FORMAT_MONOCHROME                                          0x0A
#define GCREG_DE_FORMAT_YV12                                                0x0F
#define GCREG_DE_FORMAT_A8                                                  0x10
#define GCREG_DE_FORMAT_NV12                                                0x11
#define GCREG_DE_FORMAT_NV16                                                0x12
#define GCREG_DE_FORMAT_RG16                                                0x13

/* ~~~~~~~~~~~~~ */

#define GCREG_ALPHA_MODE_NORMAL                                              0x0
#define GCREG_ALPHA_MODE_INVERSED                                            0x1

#define GCREG_GLOBAL_ALPHA_MODE_NORMAL                                       0x0
#define GCREG_GLOBAL_ALPHA_MODE_GLOBAL                                       0x1
#define GCREG_GLOBAL_ALPHA_MODE_SCALED                                       0x2

#define GCREG_COLOR_MODE_NORMAL                                              0x0
#define GCREG_COLOR_MODE_MULTIPLY                                            0x1

#define GCREG_BLENDING_MODE_ZERO                                             0x0
#define GCREG_BLENDING_MODE_ONE                                              0x1
#define GCREG_BLENDING_MODE_NORMAL                                           0x2
#define GCREG_BLENDING_MODE_INVERSED                                         0x3
#define GCREG_BLENDING_MODE_COLOR                                            0x4
#define GCREG_BLENDING_MODE_COLOR_INVERSED                                   0x5
#define GCREG_BLENDING_MODE_SATURATED_ALPHA                                  0x6
#define GCREG_BLENDING_MODE_SATURATED_DEST_ALPHA                             0x7

/* ~~~~~~~~~~~~~ */

#define GCREG_FACTOR_INVERSE_DISABLE                                         0x0
#define GCREG_FACTOR_INVERSE_ENABLE                                          0x1

/* ~~~~~~~~~~~~~ */

#define GCREG_RESOURCE_USAGE_OVERRIDE_DEFAULT                                0x0
#define GCREG_RESOURCE_USAGE_OVERRIDE_USE_ENABLE                             0x1
#define GCREG_RESOURCE_USAGE_OVERRIDE_USE_DISABLE                            0x2

/*******************************************************************************
** Modular operations: pipesel
*/

static const struct gccmdldstate gcmopipesel_pipesel_ldst =
	GCLDSTATE(gcregPipeSelectRegAddrs, 1);

struct gcmopipesel {
	/* gcregPipeSelectRegAddrs */
	struct gccmdldstate pipesel_ldst;

		/* gcregPipeSelectRegAddrs */
		union {
			struct gcregpipeselect reg;
			unsigned int raw;
		} pipesel;
};

/*******************************************************************************
** Modular operations: signal
*/

static const struct gccmdldstate gcmosignal_signal_ldst =
	GCLDSTATE(gcregEventRegAddrs, 1);

struct gcmosignal {
	/* gcregEventRegAddrs */
	struct gccmdldstate signal_ldst;

		/* gcregEventRegAddrs */
		union {
			struct gcregevent reg;
			unsigned int raw;
		} signal;
};

/*******************************************************************************
** Modular operations: flush
*/

static const struct gccmdldstate gcmoflush_flush_ldst =
	GCLDSTATE(gcregFlushRegAddrs, 1);

struct gcmoflush {
	/* gcregFlushRegAddrs */
	struct gccmdldstate flush_ldst;

		/* gcregFlushRegAddrs */
		union {
			struct gcregflush reg;
			unsigned int raw;
		} flush;
};

/*******************************************************************************
** Modular operations: semaphore
*/

static const struct gccmdldstate gcmosema_sema_ldst =
	GCLDSTATE(gcregSemaphoreRegAddrs, 1);

struct gcmosema {
	/* gcregSemaphoreRegAddrs */
	struct gccmdldstate sema_ldst;

		/* gcregSemaphoreRegAddrs */
		union {
			struct gcregsemaphore reg;
			unsigned int raw;
		} sema;
};

/*******************************************************************************
** Modular operations: mmuinit
*/

struct gcmoterminator {
	union {
		struct gcmosignal done;
		struct gccmdnop nop;
	} u1;

	union {
		struct gccmdwait wait;
		struct gccmdlink linknext;
		struct gccmdend end;
	} u2;

	union {
		struct gccmdlink linkwait;
		struct gccmdnop nop;
	} u3;
};

/*******************************************************************************
** Modular operations: mmuinit
*/

static const struct gccmdldstate gcmommuinit_safe_ldst =
	GCLDSTATE(gcregMMUSafeAddressRegAddrs, 2);

struct gcmommuinit {
	/* gcregMMUSafeAddressRegAddrs */
	struct gccmdldstate safe_ldst;

		/* gcregMMUSafeAddressRegAddrs */
		unsigned int safe;

		/* gcregMMUConfigurationRegAddrs */
		unsigned int mtlb;

		/* Alignment filler. */
		unsigned int _filler;
};

/*******************************************************************************
** Modular operations: mmumaster
*/

static const struct gccmdldstate gcmommumaster_master_ldst =
	GCLDSTATE(gcregMMUConfigurationRegAddrs, 1);

struct gcmommumaster {
	/* gcregMMUConfigurationRegAddrs */
	struct gccmdldstate master_ldst;

		/* gcregMMUConfigurationRegAddrs */
		unsigned int master;
};

/*******************************************************************************
** Modular operations: mmuflush
*/

static const struct gccmdldstate gcmommuflush_mmuflush_ldst =
	GCLDSTATE(gcregMMUConfigurationRegAddrs, 1);

struct gcmommuflush {
	/* PE cache flush. */
	struct gcmoflush peflush;

	/* Semaphore/stall after PE flush. */
	struct gcmosema peflushsema;
	struct gccmdstall peflushstall;

	/* Link to flush FE FIFO. */
	struct gccmdlink feflush;

	/* MMU flush. */
	struct gccmdldstate mmuflush_ldst;

		/* gcregMMUConfigurationRegAddrs */
		union {
			struct gcregmmuconfiguration reg;
			unsigned int raw;
		} mmuflush;

	/* Semaphore/stall after MMU flush. */
	struct gcmosema mmuflushsema;
	struct gccmdstall mmuflushstall;

	/* Link to the user buffer. */
	struct gccmdlink link;
};

/*******************************************************************************
** Modular operations: dst
*/

static const struct gccmdldstate gcmodst_address_ldst =
	GCLDSTATE(gcregDestAddressRegAddrs, 3);

static const struct gccmdldstate gcmodst_rotationheight_ldst =
	GCLDSTATE(gcregDstRotationHeightRegAddrs, 1);

static const struct gccmdldstate gcmodst_clip_ldst =
	GCLDSTATE(gcregClipTopLeftRegAddrs, 2);

struct gcmodst {
	/* gcregDestAddressRegAddrs */
	struct gccmdldstate address_ldst;

		/* gcregDestAddressRegAddrs */
		unsigned int address;

		/* gcregDestStrideRegAddrs */
		unsigned int stride;

		/* gcregDestRotationConfigRegAddrs */
		union {
			struct gcregdstrotationconfig reg;
			unsigned int raw;
		} rotation;

	/* gcregDstRotationHeightRegAddrs */
	struct gccmdldstate rotationheight_ldst;

		/* gcregDstRotationHeightRegAddrs */
		union {
			struct gcregdstrotationheight reg;
			unsigned int raw;
		} rotationheight;

	/* gcregClipTopLeftRegAddrs */
	struct gccmdldstate clip_ldst;

		/* gcregClipTopLeftRegAddrs */
		union {
			struct gcregcliplt reg;
			unsigned int raw;
		} cliplt;

		/* gcregClipBottomRight */
		union {
			struct gcregcliprb reg;
			unsigned int raw;
		} cliprb;

		/* Alignment filler. */
		unsigned int _filler;
};

/*******************************************************************************
** Modular operations: src
*/

static const struct gccmdldstate gcmosrc_address_ldst[4] = {
	GCLDSTATE(gcregBlock4SrcAddressRegAddrs + 0, 1),
	GCLDSTATE(gcregBlock4SrcAddressRegAddrs + 1, 1),
	GCLDSTATE(gcregBlock4SrcAddressRegAddrs + 2, 1),
	GCLDSTATE(gcregBlock4SrcAddressRegAddrs + 3, 1),
};

static const struct gccmdldstate gcmosrc_stride_ldst[4] = {
	GCLDSTATE(gcregBlock4SrcStrideRegAddrs + 0, 1),
	GCLDSTATE(gcregBlock4SrcStrideRegAddrs + 1, 1),
	GCLDSTATE(gcregBlock4SrcStrideRegAddrs + 2, 1),
	GCLDSTATE(gcregBlock4SrcStrideRegAddrs + 3, 1),
};

static const struct gccmdldstate gcmosrc_uplaneaddress_ldst[4] = {
	GCLDSTATE(gcregUPlaneAddressRegAddrs + 0, 1),
	GCLDSTATE(gcregUPlaneAddressRegAddrs + 1, 1),
	GCLDSTATE(gcregUPlaneAddressRegAddrs + 2, 1),
	GCLDSTATE(gcregUPlaneAddressRegAddrs + 3, 1),
};

static const struct gccmdldstate gcmosrc_uplanestride_ldst[4] = {
	GCLDSTATE(gcregUPlaneStrideRegAddrs + 0, 1),
	GCLDSTATE(gcregUPlaneStrideRegAddrs + 1, 1),
	GCLDSTATE(gcregUPlaneStrideRegAddrs + 2, 1),
	GCLDSTATE(gcregUPlaneStrideRegAddrs + 3, 1),
};

static const struct gccmdldstate gcmosrc_vplaneaddress_ldst[4] = {
	GCLDSTATE(gcregVPlaneAddressRegAddrs + 0, 1),
	GCLDSTATE(gcregVPlaneAddressRegAddrs + 1, 1),
	GCLDSTATE(gcregVPlaneAddressRegAddrs + 2, 1),
	GCLDSTATE(gcregVPlaneAddressRegAddrs + 3, 1),
};

static const struct gccmdldstate gcmosrc_vplanestride_ldst[4] = {
	GCLDSTATE(gcregVPlaneStrideRegAddrs + 0, 1),
	GCLDSTATE(gcregVPlaneStrideRegAddrs + 1, 1),
	GCLDSTATE(gcregVPlaneStrideRegAddrs + 2, 1),
	GCLDSTATE(gcregVPlaneStrideRegAddrs + 3, 1),
};

static const struct gccmdldstate gcmosrc_rotation_ldst[4] = {
	GCLDSTATE(gcregBlock4SrcRotationConfigRegAddrs + 0, 1),
	GCLDSTATE(gcregBlock4SrcRotationConfigRegAddrs + 1, 1),
	GCLDSTATE(gcregBlock4SrcRotationConfigRegAddrs + 2, 1),
	GCLDSTATE(gcregBlock4SrcRotationConfigRegAddrs + 3, 1),
};

static const struct gccmdldstate gcmosrc_config_ldst[4] = {
	GCLDSTATE(gcregBlock4SrcConfigRegAddrs + 0, 1),
	GCLDSTATE(gcregBlock4SrcConfigRegAddrs + 1, 1),
	GCLDSTATE(gcregBlock4SrcConfigRegAddrs + 2, 1),
	GCLDSTATE(gcregBlock4SrcConfigRegAddrs + 3, 1),
};

static const struct gccmdldstate gcmosrc_origin_ldst[4] = {
	GCLDSTATE(gcregBlock4SrcOriginRegAddrs + 0, 1),
	GCLDSTATE(gcregBlock4SrcOriginRegAddrs + 1, 1),
	GCLDSTATE(gcregBlock4SrcOriginRegAddrs + 2, 1),
	GCLDSTATE(gcregBlock4SrcOriginRegAddrs + 3, 1),
};

static const struct gccmdldstate gcmosrc_size_ldst[4] = {
	GCLDSTATE(gcregBlock4SrcSizeRegAddrs + 0, 1),
	GCLDSTATE(gcregBlock4SrcSizeRegAddrs + 1, 1),
	GCLDSTATE(gcregBlock4SrcSizeRegAddrs + 2, 1),
	GCLDSTATE(gcregBlock4SrcSizeRegAddrs + 3, 1),
};

static const struct gccmdldstate gcmosrc_rotationheight_ldst[4] = {
	GCLDSTATE(gcregBlock4SrcRotationHeightRegAddrs + 0, 1),
	GCLDSTATE(gcregBlock4SrcRotationHeightRegAddrs + 1, 1),
	GCLDSTATE(gcregBlock4SrcRotationHeightRegAddrs + 2, 1),
	GCLDSTATE(gcregBlock4SrcRotationHeightRegAddrs + 3, 1),
};

static const struct gccmdldstate gcmosrc_rotationangle_ldst[4] = {
	GCLDSTATE(gcregBlock4RotAngleRegAddrs + 0, 1),
	GCLDSTATE(gcregBlock4RotAngleRegAddrs + 1, 1),
	GCLDSTATE(gcregBlock4RotAngleRegAddrs + 2, 1),
	GCLDSTATE(gcregBlock4RotAngleRegAddrs + 3, 1),
};

static const struct gccmdldstate gcmosrc_rop_ldst[4] = {
	GCLDSTATE(gcregBlock4RopRegAddrs + 0, 1),
	GCLDSTATE(gcregBlock4RopRegAddrs + 1, 1),
	GCLDSTATE(gcregBlock4RopRegAddrs + 2, 1),
	GCLDSTATE(gcregBlock4RopRegAddrs + 3, 1),
};

static const struct gccmdldstate gcmosrc_mult_ldst[4] = {
	GCLDSTATE(gcregBlock4ColorMultiplyModesRegAddrs + 0, 1),
	GCLDSTATE(gcregBlock4ColorMultiplyModesRegAddrs + 1, 1),
	GCLDSTATE(gcregBlock4ColorMultiplyModesRegAddrs + 2, 1),
	GCLDSTATE(gcregBlock4ColorMultiplyModesRegAddrs + 3, 1),
};

static const struct gccmdldstate gcmosrc_alphacontrol_ldst[4] = {
	GCLDSTATE(gcregBlock4AlphaControlRegAddrs + 0, 1),
	GCLDSTATE(gcregBlock4AlphaControlRegAddrs + 1, 1),
	GCLDSTATE(gcregBlock4AlphaControlRegAddrs + 2, 1),
	GCLDSTATE(gcregBlock4AlphaControlRegAddrs + 3, 1),
};

static const struct gccmdldstate gcmosrcalpha_alphamodes_ldst[4] = {
	GCLDSTATE(gcregBlock4AlphaModesRegAddrs + 0, 1),
	GCLDSTATE(gcregBlock4AlphaModesRegAddrs + 1, 1),
	GCLDSTATE(gcregBlock4AlphaModesRegAddrs + 2, 1),
	GCLDSTATE(gcregBlock4AlphaModesRegAddrs + 3, 1),
};

static const struct gccmdldstate gcmosrcalpha_srcglobal_ldst[4] = {
	GCLDSTATE(gcregBlock4GlobalSrcColorRegAddrs + 0, 1),
	GCLDSTATE(gcregBlock4GlobalSrcColorRegAddrs + 1, 1),
	GCLDSTATE(gcregBlock4GlobalSrcColorRegAddrs + 2, 1),
	GCLDSTATE(gcregBlock4GlobalSrcColorRegAddrs + 3, 1),
};

static const struct gccmdldstate gcmosrcalpha_dstglobal_ldst[4] = {
	GCLDSTATE(gcregBlock4GlobalDestColorRegAddrs + 0, 1),
	GCLDSTATE(gcregBlock4GlobalDestColorRegAddrs + 1, 1),
	GCLDSTATE(gcregBlock4GlobalDestColorRegAddrs + 2, 1),
	GCLDSTATE(gcregBlock4GlobalDestColorRegAddrs + 3, 1),
};

struct gcmosrc {
	/* gcregBlock4SrcAddressRegAddrs */
	struct gccmdldstate address_ldst;

		/* gcregBlock4SrcAddressRegAddrs */
		unsigned int address;

	/* gcregBlock4SrcStrideRegAddrs */
	struct gccmdldstate stride_ldst;

		/* gcregBlock4SrcStrideRegAddrs */
		unsigned int stride;

	/* gcregBlock4SrcRotationConfigRegAddrs */
	struct gccmdldstate rotation_ldst;

		/* gcregBlock4SrcRotationConfigRegAddrs */
		union {
			struct gcregsrcrotationconfig reg;
			unsigned int raw;
		} rotation;

	/* gcregBlock4SrcConfigRegAddrs */
	struct gccmdldstate config_ldst;

		/* gcregBlock4SrcConfigRegAddrs */
		union {
			struct gcregsrcconfig reg;
			unsigned int raw;
		} config;

	/* gcregBlock4SrcOriginRegAddrs */
	struct gccmdldstate origin_ldst;

		/* gcregBlock4SrcOriginRegAddrs */
		union {
			struct gcregsrcorigin reg;
			unsigned int raw;
		} origin;

	/* gcregBlock4SrcSizeRegAddrs */
	struct gccmdldstate size_ldst;

		/* gcregBlock4SrcSizeRegAddrs */
		union {
			struct gcregsrcsize reg;
			unsigned int raw;
		} size;

	/* gcregBlock4SrcRotationHeightRegAddrs */
	struct gccmdldstate rotationheight_ldst;

		/* gcregBlock4SrcRotationHeightRegAddrs */
		union {
			struct gcregsrcrotationheight reg;
			unsigned int raw;
		} rotationheight;

	/* gcregBlock4RotAngleRegAddrs */
	struct gccmdldstate rotationangle_ldst;

		/* gcregBlock4RotAngleRegAddrs */
		union {
			struct gcregrotangle reg;
			unsigned int raw;
		} rotationangle;

	/* gcregBlock4RopRegAddrs */
	struct gccmdldstate rop_ldst;

		/* gcregBlock4RopRegAddrs */
		union {
			struct gcregrop reg;
			unsigned int raw;
		} rop;

	/* gcregBlock4ColorMultiplyModesRegAddrs */
	struct gccmdldstate mult_ldst;

		/* gcregBlock4ColorMultiplyModesRegAddrs */
		union {
			struct gcregcolormultiplymodes reg;
			unsigned int raw;
		} mult;

	/* gcregBlock4AlphaControlRegAddrs */
	struct gccmdldstate alphacontrol_ldst;

		/* gcregBlock4AlphaControlRegAddrs */
		union {
			struct gcregalphacontrol reg;
			unsigned int raw;
		} alphacontrol;
};

struct gcmosrcplanaryuv {
	/* gcregUPlaneAddressRegAddrs */
	struct gccmdldstate uplaneaddress_ldst;
		unsigned int uplaneaddress;

	/* gcregUPlaneStrideRegAddrs */
	struct gccmdldstate uplanestride_ldst;
		unsigned int uplanestride;

	/* gcregVPlaneAddressRegAddrs */
	struct gccmdldstate vplaneaddress_ldst;
		unsigned int vplaneaddress;

	/* gcregVPlaneStrideRegAddrs */
	struct gccmdldstate vplanestride_ldst;
		unsigned int vplanestride;
};

struct gcmosrcalpha {
	/* gcregBlock4AlphaModesRegAddrs */
	struct gccmdldstate alphamodes_ldst;

		/* gcregBlock4AlphaModesRegAddrs */
		union {
			struct gcregalphamodes reg;
			unsigned int raw;
		} alphamodes;

	/* gcregBlock4GlobalSrcColorRegAddrs */
	struct gccmdldstate srcglobal_ldst;

		/* gcregBlock4GlobalSrcColorRegAddrs */
		union {
			struct gcregglobalsrccolor reg;
			unsigned int raw;
		} srcglobal;

	/* gcregBlock4GlobalDestColorRegAddrs */
	struct gccmdldstate dstglobal_ldst;

		/* gcregBlock4GlobalDestColorRegAddrs */
		union {
			struct gcregglobaldstcolor reg;
			unsigned int raw;
		} dstglobal;
};

/*******************************************************************************
** Modular operations: bltconfig
*/

static const struct gccmdldstate gcmobltconfig_multisource_ldst =
	GCLDSTATE(gcregDEMultiSourceRegAddrs, 1);

static const struct gccmdldstate gcmobltconfig_dstconfig_ldst =
	GCLDSTATE(gcregDestConfigRegAddrs, 1);

static const struct gccmdldstate gcmobltconfig_rop_ldst =
	GCLDSTATE(gcregRopRegAddrs, 1);

struct gcmobltconfig {
	/* gcregDEMultiSourceRegAddrs */
	struct gccmdldstate multisource_ldst;

		/* gcregDEMultiSourceRegAddrs */
		union {
			struct gcregmultisource reg;
			unsigned int raw;
		} multisource;

	/* gcregDestConfigRegAddrs */
	struct gccmdldstate dstconfig_ldst;

		/* gcregDestConfigRegAddrs */
		union {
			struct gcregdstconfig reg;
			unsigned int raw;
		} dstconfig;

	/* gcregRopRegAddrs */
	struct gccmdldstate rop_ldst;

		/* gcregRopRegAddrs */
		union {
			struct gcregrop reg;
			unsigned int raw;
		} rop;
};

/*******************************************************************************
** Modular operations: startde
*/

struct gcmostart {
	/* Start DE command. */
	struct gccmdstartde startde;
	struct gccmdstartderect rect;
};

/*******************************************************************************
** Modular operations: fillsrc
*/

static const struct gccmdldstate gcmofillsrc_rotation_ldst =
	GCLDSTATE(gcregSrcRotationConfigRegAddrs, 2);

static const struct gccmdldstate gcmofillsrc_rotationheight_ldst =
	GCLDSTATE(gcregSrcRotationHeightRegAddrs, 2);

static const struct gccmdldstate gcmofillsrc_alphacontrol_ldst =
	GCLDSTATE(gcregAlphaControlRegAddrs, 1);

struct gcmofillsrc {
	/* gcregSrcRotationConfigRegAddrs */
	struct gccmdldstate rotation_ldst;

		/* gcregSrcRotationConfigRegAddrs */
		union {
			struct gcregsrcrotationconfig reg;
			unsigned int raw;
		} rotation;

		/* gcregSrcConfigRegAddrs */
		union {
			struct gcregsrcconfig reg;
			unsigned int raw;
		} config;

		/* Alignment filler. */
		unsigned int _filler1;

	/* gcregSrcRotationHeightRegAddrs */
	struct gccmdldstate rotationheight_ldst;

		/* gcregSrcRotationHeightRegAddrs */
		union {
			struct gcregsrcrotationheight reg;
			unsigned int raw;
		} rotationheight;

		/* gcregRotAngleRegAddrs */
		union {
			struct gcregrotangle reg;
			unsigned int raw;
		} rotationangle;

		/* Alignment filler. */
		unsigned int _filler2;

	/* gcregAlphaControlRegAddrs */
	struct gccmdldstate alphacontrol_ldst;

		/* gcregAlphaControlRegAddrs */
		union {
			struct gcregalphacontrol reg;
			unsigned int raw;
		} alphacontrol;
};

/*******************************************************************************
** Modular operations: fill
*/

static const struct gccmdldstate gcmofill_clearcolor_ldst =
	GCLDSTATE(gcregClearPixelValue32RegAddrs, 1);

static const struct gccmdldstate gcmofill_dstconfig_ldst =
	GCLDSTATE(gcregDestConfigRegAddrs, 1);

static const struct gccmdldstate gcmofill_rop_ldst =
	GCLDSTATE(gcregRopRegAddrs, 1);

struct gcmofill {
	struct gcmofillsrc src;

	/* gcregClearPixelValue32RegAddrs */
	struct gccmdldstate clearcolor_ldst;

		/* gcregClearPixelValue32RegAddrs */
		union {
			struct gcregclearcolor reg;
			unsigned int raw;
		} clearcolor;

	/* gcregDestConfigRegAddrs */
	struct gccmdldstate dstconfig_ldst;

		/* gcregDestConfigRegAddrs */
		union {
			struct gcregdstconfig reg;
			unsigned int raw;
		} dstconfig;

	/* gcregRopRegAddrs */
	struct gccmdldstate rop_ldst;

		/* gcregRopRegAddrs */
		union {
			struct gcregrop reg;
			unsigned int raw;
		} rop;

	/* Start DE command. */
	struct gccmdstartde startde;
	struct gccmdstartderect rect;
};

#endif
