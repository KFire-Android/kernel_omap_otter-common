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

#define __GCSTART(reg_field) \
( \
	0 ? reg_field \
)

#define __GCEND(reg_field) \
( \
	1 ? reg_field \
)

#define __GCGETSIZE(reg_field) \
( \
	__GCEND(reg_field) - __GCSTART(reg_field) + 1 \
)

#define __GCALIGN(data, reg_field) \
( \
	((unsigned int) (data)) << __GCSTART(reg_field) \
)

#define __GCMASK(reg_field) \
( \
	(unsigned int) ((__GCGETSIZE(reg_field) == 32) \
		?  ~0 : (~(~0 << __GCGETSIZE(reg_field)))) \
)

#define SETFIELDVAL(data, reg, field, value) \
( \
(((unsigned int) (data)) \
	& ~__GCALIGN(__GCMASK(reg##_##field), reg##_##field)) \
	|  __GCALIGN(reg##_##field##_##value \
	& __GCMASK(reg##_##field), reg##_##field) \
)

#define SETFIELD(data, reg, field, value) \
( \
(((unsigned int) (data)) \
	& ~__GCALIGN(__GCMASK(reg##_##field), reg##_##field)) \
	|  __GCALIGN((unsigned int) (value) \
	& __GCMASK(reg##_##field), reg##_##field) \
)

#define LS(x, y) \
( \
	SETFIELDVAL(0, AQ_COMMAND_LOAD_STATE_COMMAND, OPCODE, LOAD_STATE) \
	| SETFIELD(0, AQ_COMMAND_LOAD_STATE_COMMAND, COUNT, y) \
	| SETFIELD(0, AQ_COMMAND_LOAD_STATE_COMMAND, ADDRESS, x) \
)

#define CONFIG_GPU_PIPE(n) \
( \
	SETFIELDVAL(0, AQ_PIPE_SELECT, PIPE, n) \
)

#define CONFIG_DEST_CMD(n) \
( \
	SETFIELDVAL(0, AQDE_DEST_CONFIG, COMMAND, n) \
)

#define CONFIG_DEST_SWIZZLE(n) \
( \
	SETFIELD(0, AQDE_DEST_CONFIG, SWIZZLE, n) \
)

#define CONFIG_DEST_FMT(n) \
( \
	SETFIELD(0, AQDE_DEST_CONFIG, FORMAT, n) \
)

#define CONFIG_CLIP_LFTTOP(l, t) \
( \
	SETFIELD(0, AQDE_CLIP_TOP_LEFT, X, l) | \
	SETFIELD(0, AQDE_CLIP_TOP_LEFT, Y, t) \
)

#define CONFIG_CLIP_RHTBTM(r, b) \
( \
	SETFIELD(0, AQDE_CLIP_BOTTOM_RIGHT, X, r) | \
	SETFIELD(0, AQDE_CLIP_BOTTOM_RIGHT, Y, b) \
)

#define CONFIG_PE2DCACHE_FLUSH() \
( \
	SETFIELD(0, AQ_FLUSH, PE2D_CACHE, AQ_FLUSH_PE2D_CACHE_ENABLE) \
)

#define CONFIG_ROP_TYPE(n) \
( \
	SETFIELD(0, AQDE_ROP, TYPE, n) \
)

#define CONFIG_ROP_BCKGND(n) \
( \
	SETFIELD(0, AQDE_ROP, ROP_BG, n) \
)

#define CONFIG_MLTSRC_ROP_BCKGND(n) \
( \
	SETFIELD(0, GCREG_DE_ROP, ROP_BG, n) \
)

#define CONFIG_ROP_FORGND(n) \
( \
	SETFIELD(0, AQDE_ROP, ROP_FG, n) \
)

#define CONFIG_SRC_ORIGIN(l, t) \
( \
	SETFIELD(0, AQDE_SRC_ORIGIN, X, l) | \
	SETFIELD(0, AQDE_SRC_ORIGIN, Y, t) \
)

#define CONFIG_SRC_SIZE(w, h) \
( \
	SETFIELD(0, AQDE_SRC_SIZE, X, w) | \
	SETFIELD(0, AQDE_SRC_SIZE, Y, h) \
)

#define CONFIG_SRC_ROT_MODE(n) \
( \
	SETFIELDVAL(0, AQDE_SRC_ROTATION_CONFIG, ROTATION, n) \
)

#define CONFIG_SRC_ROT_WIDTH(w) \
( \
	SETFIELD(0, AQDE_SRC_ROTATION_CONFIG, WIDTH, w) \
)

#define CONFIG_SRC_ROT_HEIGHT(h) \
( \
	SETFIELD(0, AQDE_SRC_ROTATION_HEIGHT, HEIGHT, h) \
)

#define CONFIG_DEST_ROT_MODE(n) \
( \
	SETFIELDVAL(0, AQDE_SRC_ROTATION_CONFIG, ROTATION, n) \
)

#define CONFIG_DEST_ROT_WIDTH(w) \
( \
	SETFIELD(0, AQDE_SRC_ROTATION_CONFIG, WIDTH, w) \
)

#define CONFIG_SRC_LOCATION(n) \
( \
	SETFIELD(0, AQDE_SRC_CONFIG, LOCATION, n) \
)

#define CONFIG_SRC_FMT(n) \
( \
	SETFIELD(0, AQDE_SRC_CONFIG, SOURCE_FORMAT, n) \
)

#define CONFIG_SRC_TRANSPARENCY(n) \
( \
	SETFIELD(1, AQDE_SRC_CONFIG, TRANSPARENCY, n) \
)

#define CONFIG_SRC_SWIZZLE(n) \
( \
	SETFIELD(0, AQDE_SRC_CONFIG, SWIZZLE, n) \
)

#define CONFIG_SRC_PIX_CALC(n) \
( \
	SETFIELD(0, AQDE_SRC_CONFIG, SRC_RELATIVE, n) \
)

#define CONFIG_START_DE(n) \
( \
	SETFIELDVAL(0, AQ_COMMAND_START_DE_COMMAND, OPCODE, START_DE) | \
	SETFIELD(0, AQ_COMMAND_START_DE_COMMAND, COUNT, n) \
)

#define CONFIG_START_DE_LFTTOP(l, t) \
( \
	SETFIELD(0, AQ_COMMAND_TOP_LEFT, X, l) | \
	SETFIELD(0, AQ_COMMAND_TOP_LEFT, Y, t) \
)

#define CONFIG_START_DE_RGHBTM(r, b) \
( \
	SETFIELD(0, AQ_COMMAND_BOTTOM_RIGHT, X, r) | \
	SETFIELD(0, AQ_COMMAND_BOTTOM_RIGHT, Y, b) \
)

#define CONFIG_MLTSRC_ROP_TYPE(n) \
( \
	SETFIELD(0, GCREG_DE_ROP, TYPE, n) \
)

#define CONFIG_MLTSRC_ROP_FORGND(n) \
( \
	SETFIELD(0, GCREG_DE_ROP, ROP_FG, n) \
)

#define CONFIG_MLTSRC_ORIGIN(l, t) \
( \
	SETFIELD(0, GCREG_DE_SRC_ORIGIN, X, l) | \
	SETFIELD(0, GCREG_DE_SRC_ORIGIN, Y, t) \
)

#define CONFIG_MLTSRC_SIZE(r, b) \
( \
	SETFIELD(0, GCREG_DE_SRC_SIZE, X, r) | \
	SETFIELD(0, GCREG_DE_SRC_SIZE, Y, b) \
)

#define CONFIG_MLTSRC_ROT_MODE(n) \
( \
	SETFIELDVAL(0, GCREG_DE_SRC_ROTATION_CONFIG, ROTATION, n) \
)

#define CONFIG_MLTSRC_ROT_WIDTH(w) \
( \
	SETFIELD(0, GCREG_DE_SRC_ROTATION_CONFIG, WIDTH, w) \
)

#define CONFIG_MLTSRC_LOC(n) \
( \
	SETFIELDVAL(0, GCREG_DE_SRC_CONFIG, LOCATION, n) \
)

#define CONFIG_MLTSRC_FMT(n) \
( \
	SETFIELD(0, GCREG_DE_SRC_CONFIG, SOURCE_FORMAT, n) \
)

#define CONFIG_MLTSRC_SWIZZLE(n) \
( \
	SETFIELD(0, GCREG_DE_SRC_CONFIG, SWIZZLE, n) \
)

#define CONFIG_MLTSRC_CTRL(n) \
( \
	SETFIELD(0, GCREG_DE_MULTI_SOURCE, MAX_SOURCE, n) \
)

#define CONFIG_ALPHA_CTRL(n) \
( \
	SETFIELDVAL(0, AQDE_ALPHA_CONTROL, ENABLE, n) \
)

/*******************************************************************************
** Register AQHiClockControl
*/

#define AQ_HI_CLOCK_CONTROL_Address                                      0x00000
#define AQ_HI_CLOCK_CONTROL_MSB                                               15
#define AQ_HI_CLOCK_CONTROL_LSB                                                0
#define AQ_HI_CLOCK_CONTROL_BLK                                                0
#define AQ_HI_CLOCK_CONTROL_Count                                              1
#define AQ_HI_CLOCK_CONTROL_FieldMask                                 0x000A17FE
#define AQ_HI_CLOCK_CONTROL_ReadMask                                  0x000A17FE
#define AQ_HI_CLOCK_CONTROL_WriteMask                                 0x000817FE
#define AQ_HI_CLOCK_CONTROL_ResetValue                                0x00000100

/* Disable 2D clock. */
#define AQ_HI_CLOCK_CONTROL_CLK2D_DIS                                      1 : 1
#define AQ_HI_CLOCK_CONTROL_CLK2D_DIS_End                                      1
#define AQ_HI_CLOCK_CONTROL_CLK2D_DIS_Start                                    1
#define AQ_HI_CLOCK_CONTROL_CLK2D_DIS_Type                                   U01

#define AQ_HI_CLOCK_CONTROL_FSCALE_VAL                                     8 : 2
#define AQ_HI_CLOCK_CONTROL_FSCALE_VAL_End                                     8
#define AQ_HI_CLOCK_CONTROL_FSCALE_VAL_Start                                   2
#define AQ_HI_CLOCK_CONTROL_FSCALE_VAL_Type                                  U07

#define AQ_HI_CLOCK_CONTROL_FSCALE_CMD_LOAD                                9 : 9
#define AQ_HI_CLOCK_CONTROL_FSCALE_CMD_LOAD_End                                9
#define AQ_HI_CLOCK_CONTROL_FSCALE_CMD_LOAD_Start                              9
#define AQ_HI_CLOCK_CONTROL_FSCALE_CMD_LOAD_Type                             U01

/* Disables clock gating for rams. */
#define AQ_HI_CLOCK_CONTROL_DISABLE_RAM_CLOCK_GATING                     10 : 10
#define AQ_HI_CLOCK_CONTROL_DISABLE_RAM_CLOCK_GATING_End                      10
#define AQ_HI_CLOCK_CONTROL_DISABLE_RAM_CLOCK_GATING_Start                    10
#define AQ_HI_CLOCK_CONTROL_DISABLE_RAM_CLOCK_GATING_Type                    U01

/* Soft resets the IP. */
#define AQ_HI_CLOCK_CONTROL_SOFT_RESET                                   12 : 12
#define AQ_HI_CLOCK_CONTROL_SOFT_RESET_End                                    12
#define AQ_HI_CLOCK_CONTROL_SOFT_RESET_Start                                  12
#define AQ_HI_CLOCK_CONTROL_SOFT_RESET_Type                                  U01

/* 2D pipe is idle. */
#define AQ_HI_CLOCK_CONTROL_IDLE2_D                                      17 : 17
#define AQ_HI_CLOCK_CONTROL_IDLE2_D_End                                       17
#define AQ_HI_CLOCK_CONTROL_IDLE2_D_Start                                     17
#define AQ_HI_CLOCK_CONTROL_IDLE2_D_Type                                     U01

/* Isolate GPU bit */
#define AQ_HI_CLOCK_CONTROL_ISOLATE_GPU                                  19 : 19
#define AQ_HI_CLOCK_CONTROL_ISOLATE_GPU_End                                   19
#define AQ_HI_CLOCK_CONTROL_ISOLATE_GPU_Start                                 19
#define AQ_HI_CLOCK_CONTROL_ISOLATE_GPU_Type                                 U01

/*******************************************************************************
** Register AQHiIdle
*/

#define AQ_HI_IDLE_Address                                               0x00004
#define AQ_HI_IDLE_MSB                                                        15
#define AQ_HI_IDLE_LSB                                                         0
#define AQ_HI_IDLE_BLK                                                         0
#define AQ_HI_IDLE_Count                                                       1
#define AQ_HI_IDLE_FieldMask                                          0x80000007
#define AQ_HI_IDLE_ReadMask                                           0x80000007
#define AQ_HI_IDLE_WriteMask                                          0x00000000
#define AQ_HI_IDLE_ResetValue                                         0x00000007

/* FE is idle. */
#define AQ_HI_IDLE_IDLE_FE                                                 0 : 0
#define AQ_HI_IDLE_IDLE_FE_End                                                 0
#define AQ_HI_IDLE_IDLE_FE_Start                                               0
#define AQ_HI_IDLE_IDLE_FE_Type                                              U01

/* DE is idle. */
#define AQ_HI_IDLE_IDLE_DE                                                 1 : 1
#define AQ_HI_IDLE_IDLE_DE_End                                                 1
#define AQ_HI_IDLE_IDLE_DE_Start                                               1
#define AQ_HI_IDLE_IDLE_DE_Type                                              U01

/* PE is idle. */
#define AQ_HI_IDLE_IDLE_PE                                                 2 : 2
#define AQ_HI_IDLE_IDLE_PE_End                                                 2
#define AQ_HI_IDLE_IDLE_PE_Start                                               2
#define AQ_HI_IDLE_IDLE_PE_Type                                              U01

/* AXI is in low power mode. */
#define AQ_HI_IDLE_AXI_LP                                                31 : 31
#define AQ_HI_IDLE_AXI_LP_End                                                 31
#define AQ_HI_IDLE_AXI_LP_Start                                               31
#define AQ_HI_IDLE_AXI_LP_Type                                               U01

/*******************************************************************************
** Register AQAxiConfig
*/

#define AQ_AXI_CONFIG_Address                                            0x00008
#define AQ_AXI_CONFIG_MSB                                                     15
#define AQ_AXI_CONFIG_LSB                                                      0
#define AQ_AXI_CONFIG_BLK                                                      0
#define AQ_AXI_CONFIG_Count                                                    1
#define AQ_AXI_CONFIG_FieldMask                                       0x0000FFFF
#define AQ_AXI_CONFIG_ReadMask                                        0x0000FFFF
#define AQ_AXI_CONFIG_WriteMask                                       0x0000FFFF
#define AQ_AXI_CONFIG_ResetValue                                      0x00000000

#define AQ_AXI_CONFIG_AWID                                                 3 : 0
#define AQ_AXI_CONFIG_AWID_End                                                 3
#define AQ_AXI_CONFIG_AWID_Start                                               0
#define AQ_AXI_CONFIG_AWID_Type                                              U04

#define AQ_AXI_CONFIG_ARID                                                 7 : 4
#define AQ_AXI_CONFIG_ARID_End                                                 7
#define AQ_AXI_CONFIG_ARID_Start                                               4
#define AQ_AXI_CONFIG_ARID_Type                                              U04

#define AQ_AXI_CONFIG_AWCACHE                                             11 : 8
#define AQ_AXI_CONFIG_AWCACHE_End                                             11
#define AQ_AXI_CONFIG_AWCACHE_Start                                            8
#define AQ_AXI_CONFIG_AWCACHE_Type                                           U04

#define AQ_AXI_CONFIG_ARCACHE                                            15 : 12
#define AQ_AXI_CONFIG_ARCACHE_End                                             15
#define AQ_AXI_CONFIG_ARCACHE_Start                                           12
#define AQ_AXI_CONFIG_ARCACHE_Type                                           U04

/*******************************************************************************
** Register AQAxiStatus
*/

#define AQ_AXI_STATUS_Address                                            0x0000C
#define AQ_AXI_STATUS_MSB                                                     15
#define AQ_AXI_STATUS_LSB                                                      0
#define AQ_AXI_STATUS_BLK                                                      0
#define AQ_AXI_STATUS_Count                                                    1
#define AQ_AXI_STATUS_FieldMask                                       0x000003FF
#define AQ_AXI_STATUS_ReadMask                                        0x000003FF
#define AQ_AXI_STATUS_WriteMask                                       0x00000000
#define AQ_AXI_STATUS_ResetValue                                      0x00000000

#define AQ_AXI_STATUS_DET_RD_ERR                                           9 : 9
#define AQ_AXI_STATUS_DET_RD_ERR_End                                           9
#define AQ_AXI_STATUS_DET_RD_ERR_Start                                         9
#define AQ_AXI_STATUS_DET_RD_ERR_Type                                        U01

#define AQ_AXI_STATUS_DET_WR_ERR                                           8 : 8
#define AQ_AXI_STATUS_DET_WR_ERR_End                                           8
#define AQ_AXI_STATUS_DET_WR_ERR_Start                                         8
#define AQ_AXI_STATUS_DET_WR_ERR_Type                                        U01

#define AQ_AXI_STATUS_RD_ERR_ID                                            7 : 4
#define AQ_AXI_STATUS_RD_ERR_ID_End                                            7
#define AQ_AXI_STATUS_RD_ERR_ID_Start                                          4
#define AQ_AXI_STATUS_RD_ERR_ID_Type                                         U04

#define AQ_AXI_STATUS_WR_ERR_ID                                            3 : 0
#define AQ_AXI_STATUS_WR_ERR_ID_End                                            3
#define AQ_AXI_STATUS_WR_ERR_ID_Start                                          0
#define AQ_AXI_STATUS_WR_ERR_ID_Type                                         U04

/*******************************************************************************
** Register AQIntrAcknowledge
*/

/* Interrupt acknowledge register. Each bit represents a corresponding event
** being triggered. Reading from this register clears the outstanding interrupt.
*/

#define AQ_INTR_ACKNOWLEDGE_Address                                      0x00010
#define AQ_INTR_ACKNOWLEDGE_MSB                                               15
#define AQ_INTR_ACKNOWLEDGE_LSB                                                0
#define AQ_INTR_ACKNOWLEDGE_BLK                                                0
#define AQ_INTR_ACKNOWLEDGE_Count                                              1
#define AQ_INTR_ACKNOWLEDGE_FieldMask                                 0xFFFFFFFF
#define AQ_INTR_ACKNOWLEDGE_ReadMask                                  0xFFFFFFFF
#define AQ_INTR_ACKNOWLEDGE_WriteMask                                 0x00000000
#define AQ_INTR_ACKNOWLEDGE_ResetValue                                0x00000000

#define AQ_INTR_ACKNOWLEDGE_INTR_VEC                                      31 : 0
#define AQ_INTR_ACKNOWLEDGE_INTR_VEC_End                                      31
#define AQ_INTR_ACKNOWLEDGE_INTR_VEC_Start                                     0
#define AQ_INTR_ACKNOWLEDGE_INTR_VEC_Type                                    U32

/*******************************************************************************
** Register AQIntrEnbl
*/

/* Interrupt enable register. Each bit enables a corresponding event. */

#define AQ_INTR_ENBL_Address                                             0x00014
#define AQ_INTR_ENBL_MSB                                                      15
#define AQ_INTR_ENBL_LSB                                                       0
#define AQ_INTR_ENBL_BLK                                                       0
#define AQ_INTR_ENBL_Count                                                     1
#define AQ_INTR_ENBL_FieldMask                                        0xFFFFFFFF
#define AQ_INTR_ENBL_ReadMask                                         0xFFFFFFFF
#define AQ_INTR_ENBL_WriteMask                                        0xFFFFFFFF
#define AQ_INTR_ENBL_ResetValue                                       0x00000000

#define AQ_INTR_ENBL_INTR_ENBL_VEC                                        31 : 0
#define AQ_INTR_ENBL_INTR_ENBL_VEC_End                                        31
#define AQ_INTR_ENBL_INTR_ENBL_VEC_Start                                       0
#define AQ_INTR_ENBL_INTR_ENBL_VEC_Type                                      U32

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
** Command AQCommandLoadState
*/

/* When enabled, convert 16.16 fixed point into 32-bit floating point. */
#define AQ_COMMAND_LOAD_STATE_COMMAND_FLOAT                              26 : 26
#define AQ_COMMAND_LOAD_STATE_COMMAND_FLOAT_End                               26
#define AQ_COMMAND_LOAD_STATE_COMMAND_FLOAT_Start                             26
#define AQ_COMMAND_LOAD_STATE_COMMAND_FLOAT_Type                             U01
#define   AQ_COMMAND_LOAD_STATE_COMMAND_FLOAT_NORMAL                         0x0
#define   AQ_COMMAND_LOAD_STATE_COMMAND_FLOAT_FIXED16_DOT16                  0x1

/* Number of states. 0 = 1024. */
#define AQ_COMMAND_LOAD_STATE_COMMAND_COUNT                              25 : 16
#define AQ_COMMAND_LOAD_STATE_COMMAND_COUNT_End                               25
#define AQ_COMMAND_LOAD_STATE_COMMAND_COUNT_Start                             16
#define AQ_COMMAND_LOAD_STATE_COMMAND_COUNT_Type                             U10

/* Starting state address. */
#define AQ_COMMAND_LOAD_STATE_COMMAND_ADDRESS                             15 : 0
#define AQ_COMMAND_LOAD_STATE_COMMAND_ADDRESS_End                             15
#define AQ_COMMAND_LOAD_STATE_COMMAND_ADDRESS_Start                            0
#define AQ_COMMAND_LOAD_STATE_COMMAND_ADDRESS_Type                           U16

#define AQ_COMMAND_LOAD_STATE_COMMAND_OPCODE                             31 : 27
#define AQ_COMMAND_LOAD_STATE_COMMAND_OPCODE_End                              31
#define AQ_COMMAND_LOAD_STATE_COMMAND_OPCODE_Start                            27
#define AQ_COMMAND_LOAD_STATE_COMMAND_OPCODE_Type                            U05
#define   AQ_COMMAND_LOAD_STATE_COMMAND_OPCODE_LOAD_STATE                   0x01

/*******************************************************************************
** Command AQCommandEnd
*/

/* Send event when END is completed. */
#define AQ_COMMAND_END_COMMAND_EVENT_ENABLE                                8 : 8
#define AQ_COMMAND_END_COMMAND_EVENT_ENABLE_End                                8
#define AQ_COMMAND_END_COMMAND_EVENT_ENABLE_Start                              8
#define AQ_COMMAND_END_COMMAND_EVENT_ENABLE_Type                             U01

/* Event ID to be send. */
#define AQ_COMMAND_END_COMMAND_EVENT_ID                                    4 : 0
#define AQ_COMMAND_END_COMMAND_EVENT_ID_End                                    4
#define AQ_COMMAND_END_COMMAND_EVENT_ID_Start                                  0
#define AQ_COMMAND_END_COMMAND_EVENT_ID_Type                                 U05

#define AQ_COMMAND_END_COMMAND_OPCODE                                    31 : 27
#define AQ_COMMAND_END_COMMAND_OPCODE_End                                     31
#define AQ_COMMAND_END_COMMAND_OPCODE_Start                                   27
#define AQ_COMMAND_END_COMMAND_OPCODE_Type                                   U05
#define   AQ_COMMAND_END_COMMAND_OPCODE_END                                 0x02

/*******************************************************************************
** Command AQCommandNop
*/

#define AQ_COMMAND_NOP_COMMAND_OPCODE                                    31 : 27
#define AQ_COMMAND_NOP_COMMAND_OPCODE_End                                     31
#define AQ_COMMAND_NOP_COMMAND_OPCODE_Start                                   27
#define AQ_COMMAND_NOP_COMMAND_OPCODE_Type                                   U05
#define   AQ_COMMAND_NOP_COMMAND_OPCODE_NOP                                 0x03

/*******************************************************************************
** Command AQCommandStartDE
*/

/* Offset Command
** ~~~~~~~~~~~~~~ */

/* Number of 32-bit data words to send.
** The data follows the rectangles, aligned at 64-bit.
*/
#define AQ_COMMAND_START_DE_COMMAND_DATA_COUNT                           26 : 16
#define AQ_COMMAND_START_DE_COMMAND_DATA_COUNT_End                            26
#define AQ_COMMAND_START_DE_COMMAND_DATA_COUNT_Start                          16
#define AQ_COMMAND_START_DE_COMMAND_DATA_COUNT_Type                          U11

/* Number of rectangles to send.
** The rectangles follow the command, aligned at 64-bit.
*/
#define AQ_COMMAND_START_DE_COMMAND_COUNT                                 15 : 8
#define AQ_COMMAND_START_DE_COMMAND_COUNT_End                                 15
#define AQ_COMMAND_START_DE_COMMAND_COUNT_Start                                8
#define AQ_COMMAND_START_DE_COMMAND_COUNT_Type                               U08

#define AQ_COMMAND_START_DE_COMMAND_OPCODE                               31 : 27
#define AQ_COMMAND_START_DE_COMMAND_OPCODE_End                                31
#define AQ_COMMAND_START_DE_COMMAND_OPCODE_Start                              27
#define AQ_COMMAND_START_DE_COMMAND_OPCODE_Type                              U05
#define   AQ_COMMAND_START_DE_COMMAND_OPCODE_START_DE                       0x04

/* Offset TopLeft
** ~~~~~~~~~~~~~~ */

#define AQ_COMMAND_TOP_LEFT_Y                                            31 : 16
#define AQ_COMMAND_TOP_LEFT_Y_End                                             31
#define AQ_COMMAND_TOP_LEFT_Y_Start                                           16
#define AQ_COMMAND_TOP_LEFT_Y_Type                                           U16

#define AQ_COMMAND_TOP_LEFT_X                                             15 : 0
#define AQ_COMMAND_TOP_LEFT_X_End                                             15
#define AQ_COMMAND_TOP_LEFT_X_Start                                            0
#define AQ_COMMAND_TOP_LEFT_X_Type                                           U16

/* Offset BottomRight
** ~~~~~~~~~~~~~~~~~~ */

#define AQ_COMMAND_BOTTOM_RIGHT_Y                                        31 : 16
#define AQ_COMMAND_BOTTOM_RIGHT_Y_End                                         31
#define AQ_COMMAND_BOTTOM_RIGHT_Y_Start                                       16
#define AQ_COMMAND_BOTTOM_RIGHT_Y_Type                                       U16

#define AQ_COMMAND_BOTTOM_RIGHT_X                                         15 : 0
#define AQ_COMMAND_BOTTOM_RIGHT_X_End                                         15
#define AQ_COMMAND_BOTTOM_RIGHT_X_Start                                        0
#define AQ_COMMAND_BOTTOM_RIGHT_X_Type                                       U16

/*******************************************************************************
** Command AQCommandWait
*/

/* Number of cycles to wait until the next command gets fetched. */
#define AQ_COMMAND_WAIT_COMMAND_DELAY                                     15 : 0
#define AQ_COMMAND_WAIT_COMMAND_DELAY_End                                     15
#define AQ_COMMAND_WAIT_COMMAND_DELAY_Start                                    0
#define AQ_COMMAND_WAIT_COMMAND_DELAY_Type                                   U16

#define AQ_COMMAND_WAIT_COMMAND_OPCODE                                   31 : 27
#define AQ_COMMAND_WAIT_COMMAND_OPCODE_End                                    31
#define AQ_COMMAND_WAIT_COMMAND_OPCODE_Start                                  27
#define AQ_COMMAND_WAIT_COMMAND_OPCODE_Type                                  U05
#define   AQ_COMMAND_WAIT_COMMAND_OPCODE_WAIT                               0x07

/*******************************************************************************
** Command AQCommandLink
*/

/* Number of 64-bit words to fetch.  Make sure this number is not too low,
** nothing else will be fetched.  So, make sure that the last command in the
** new command buffer is either an END, a LINK, a CALL, or a RETURN.
*/
#define AQ_COMMAND_LINK_COMMAND_PREFETCH                                  15 : 0
#define AQ_COMMAND_LINK_COMMAND_PREFETCH_End                                  15
#define AQ_COMMAND_LINK_COMMAND_PREFETCH_Start                                 0
#define AQ_COMMAND_LINK_COMMAND_PREFETCH_Type                                U16

#define AQ_COMMAND_LINK_COMMAND_OPCODE                                   31 : 27
#define AQ_COMMAND_LINK_COMMAND_OPCODE_End                                    31
#define AQ_COMMAND_LINK_COMMAND_OPCODE_Start                                  27
#define AQ_COMMAND_LINK_COMMAND_OPCODE_Type                                  U05
#define   AQ_COMMAND_LINK_COMMAND_OPCODE_LINK                               0x08

/* Offset Address
** ~~~~~~~~~~~~~~ */
#define AQ_COMMAND_LINK_ADDRESS_Index                                          1
#define AQ_COMMAND_LINK_ADDRESS_CmdAddrs                                  0x0F0D

#define AQ_COMMAND_LINK_ADDRESS_ADDRESS                                   31 : 0
#define AQ_COMMAND_LINK_ADDRESS_ADDRESS_End                                   30
#define AQ_COMMAND_LINK_ADDRESS_ADDRESS_Start                                  0
#define AQ_COMMAND_LINK_ADDRESS_ADDRESS_Type                                 U31

/*******************************************************************************
** Command Stall
*/

/* Offset Command
** ~~~~~~~~~~~~~~ */
#define STALL_COMMAND_OPCODE                                             31 : 27
#define STALL_COMMAND_OPCODE_End                                              31
#define STALL_COMMAND_OPCODE_Start                                            27
#define STALL_COMMAND_OPCODE_Type                                            U05
#define   STALL_COMMAND_OPCODE_STALL                                        0x09

/* Offset Stall
** ~~~~~~~~~~~~ */
#define STALL_STALL_SOURCE                                                 4 : 0
#define STALL_STALL_SOURCE_End                                                 4
#define STALL_STALL_SOURCE_Start                                               0
#define STALL_STALL_SOURCE_Type                                              U05
#define   STALL_STALL_SOURCE_FRONT_END                                      0x01
#define   STALL_STALL_SOURCE_PIXEL_ENGINE                                   0x07
#define   STALL_STALL_SOURCE_DRAWING_ENGINE                                 0x0B

#define STALL_STALL_DESTINATION                                           12 : 8
#define STALL_STALL_DESTINATION_End                                           12
#define STALL_STALL_DESTINATION_Start                                          8
#define STALL_STALL_DESTINATION_Type                                         U05
#define   STALL_STALL_DESTINATION_FRONT_END                                 0x01
#define   STALL_STALL_DESTINATION_PIXEL_ENGINE                              0x07
#define   STALL_STALL_DESTINATION_DRAWING_ENGINE                            0x0B

/*******************************************************************************
** Command gccmdCall
*/

/* Offset Command
** ~~~~~~~~~~~~~~ */

/* Number of 64-bit words to fetch.  Make sure this number is not too low,
** nothing else will be fetched.  So, make sure that the last command in the
** new command buffer is either an END, a LINK, a CALL, or a RETURN.
*/
#define GCCMD_CALL_COMMAND_PREFETCH                                       15 : 0
#define GCCMD_CALL_COMMAND_PREFETCH_End                                       15
#define GCCMD_CALL_COMMAND_PREFETCH_Start                                      0
#define GCCMD_CALL_COMMAND_PREFETCH_Type                                     U16

#define GCCMD_CALL_COMMAND_OPCODE                                        31 : 27
#define GCCMD_CALL_COMMAND_OPCODE_End                                         31
#define GCCMD_CALL_COMMAND_OPCODE_Start                                       27
#define GCCMD_CALL_COMMAND_OPCODE_Type                                       U05
#define   GCCMD_CALL_COMMAND_OPCODE_CALL                                    0x0A

/* Offset Address
** ~~~~~~~~~~~~~~ */

#define GCCMD_CALL_ADDRESS_Index                                               1
#define GCCMD_CALL_ADDRESS_CmdAddrs                                       0x0F19

#define GCCMD_CALL_ADDRESS_ADDRESS                                        31 : 0
#define GCCMD_CALL_ADDRESS_ADDRESS_End                                        30
#define GCCMD_CALL_ADDRESS_ADDRESS_Start                                       0
#define GCCMD_CALL_ADDRESS_ADDRESS_Type                                      U31

/* Offset ReturnPrefetch
** ~~~~~~~~~~~~~~~~~~~~~ */

/* Number of 64-bit words to fetch after a Return has been issued.  Make sure **
** this number if not too low nothing else will be fetched.  So, make sure    **
** the last command in this prefetch block is either an END, a LINK, a CALL,  **
** or a RETURN.                                                               */
#define GCCMD_CALL_RETURN_PREFETCH_PREFETCH                               15 : 0
#define GCCMD_CALL_RETURN_PREFETCH_PREFETCH_End                               15
#define GCCMD_CALL_RETURN_PREFETCH_PREFETCH_Start                              0
#define GCCMD_CALL_RETURN_PREFETCH_PREFETCH_Type                             U16

/* Offset ReturnAddress
** ~~~~~~~~~~~~~~~~~~~~ */
#define GCCMD_CALL_RETURN_ADDRESS_Index                                        3
#define GCCMD_CALL_RETURN_ADDRESS_CmdAddrs                                0x0F1B

#define GCCMD_CALL_RETURN_ADDRESS_ADDRESS                                 31 : 0
#define GCCMD_CALL_RETURN_ADDRESS_ADDRESS_End                                 30
#define GCCMD_CALL_RETURN_ADDRESS_ADDRESS_Start                                0
#define GCCMD_CALL_RETURN_ADDRESS_ADDRESS_Type                               U31

/*******************************************************************************
** Command gccmdReturn
*/

#define GCCMD_RETURN_COMMAND_OPCODE                                      31 : 27
#define GCCMD_RETURN_COMMAND_OPCODE_End                                       31
#define GCCMD_RETURN_COMMAND_OPCODE_Start                                     27
#define GCCMD_RETURN_COMMAND_OPCODE_Type                                     U05
#define   GCCMD_RETURN_COMMAND_OPCODE_RETURN                                0x0B

/*******************************************************************************
** State AQStall
*/

#define AQStallRegAddrs                                                   0x0F00
#define AQ_STALL_Count                                                         1
#define AQ_STALL_ResetValue                                           0x00000000

#define AQ_STALL_FLIP0                                                   30 : 30
#define AQ_STALL_FLIP0_End                                                    30
#define AQ_STALL_FLIP0_Start                                                  30
#define AQ_STALL_FLIP0_Type                                                  U01

#define AQ_STALL_FLIP1                                                   31 : 31
#define AQ_STALL_FLIP1_End                                                    31
#define AQ_STALL_FLIP1_Start                                                  31
#define AQ_STALL_FLIP1_Type                                                  U01

#define AQ_STALL_SOURCE                                                    4 : 0
#define AQ_STALL_SOURCE_End                                                    4
#define AQ_STALL_SOURCE_Start                                                  0
#define AQ_STALL_SOURCE_Type                                                 U05
#define   AQ_STALL_SOURCE_FRONT_END                                         0x01
#define   AQ_STALL_SOURCE_PIXEL_ENGINE                                      0x07
#define   AQ_STALL_SOURCE_DRAWING_ENGINE                                    0x0B

#define AQ_STALL_DESTINATION                                              12 : 8
#define AQ_STALL_DESTINATION_End                                              12
#define AQ_STALL_DESTINATION_Start                                             8
#define AQ_STALL_DESTINATION_Type                                            U05
#define   AQ_STALL_DESTINATION_FRONT_END                                    0x01
#define   AQ_STALL_DESTINATION_PIXEL_ENGINE                                 0x07
#define   AQ_STALL_DESTINATION_DRAWING_ENGINE                               0x0B

/*******************************************************************************
** State AQPipeSelect
*/

/* Select the current graphics pipe. */

#define AQPipeSelectRegAddrs                                              0x0E00
#define AQ_PIPE_SELECT_MSB                                                    15
#define AQ_PIPE_SELECT_LSB                                                     0
#define AQ_PIPE_SELECT_BLK                                                     0
#define AQ_PIPE_SELECT_Count                                                   1
#define AQ_PIPE_SELECT_FieldMask                                      0x00000001
#define AQ_PIPE_SELECT_ReadMask                                       0x00000001
#define AQ_PIPE_SELECT_WriteMask                                      0x00000001
#define AQ_PIPE_SELECT_ResetValue                                     0x00000000

/* Selects the pipe to send states and data to.  Make  sure the PE is idle    **
** before you switch pipes.                                                   */
#define AQ_PIPE_SELECT_PIPE                                                0 : 0
#define AQ_PIPE_SELECT_PIPE_End                                                0
#define AQ_PIPE_SELECT_PIPE_Start                                              0
#define AQ_PIPE_SELECT_PIPE_Type                                             U01
#define   AQ_PIPE_SELECT_PIPE_PIPE3D                                         0x0
#define   AQ_PIPE_SELECT_PIPE_PIPE2D                                         0x1

/*******************************************************************************
** State AQEvent
*/

/* Send an event. */

#define AQEventRegAddrs                                                   0x0E01
#define AQ_EVENT_MSB                                                          15
#define AQ_EVENT_LSB                                                           0
#define AQ_EVENT_BLK                                                           0
#define AQ_EVENT_Count                                                         1
#define AQ_EVENT_FieldMask                                            0x0000007F
#define AQ_EVENT_ReadMask                                             0x0000007F
#define AQ_EVENT_WriteMask                                            0x0000007F
#define AQ_EVENT_ResetValue                                           0x00000000

/* 5-bit event ID to send. */
#define AQ_EVENT_EVENT_ID                                                  4 : 0
#define AQ_EVENT_EVENT_ID_End                                                  4
#define AQ_EVENT_EVENT_ID_Start                                                0
#define AQ_EVENT_EVENT_ID_Type                                               U05

/* The event is sent by the FE. */
#define AQ_EVENT_FE_SRC                                                    5 : 5
#define AQ_EVENT_FE_SRC_End                                                    5
#define AQ_EVENT_FE_SRC_Start                                                  5
#define AQ_EVENT_FE_SRC_Type                                                 U01
#define   AQ_EVENT_FE_SRC_DISABLE                                            0x0
#define   AQ_EVENT_FE_SRC_ENABLE                                             0x1

/* The event is sent by the PE. */
#define AQ_EVENT_PE_SRC                                                    6 : 6
#define AQ_EVENT_PE_SRC_End                                                    6
#define AQ_EVENT_PE_SRC_Start                                                  6
#define AQ_EVENT_PE_SRC_Type                                                 U01
#define   AQ_EVENT_PE_SRC_DISABLE                                            0x0
#define   AQ_EVENT_PE_SRC_ENABLE                                             0x1

/*******************************************************************************
** State AQSemaphore
*/

/* A sempahore state arms the semaphore in the destination. */

#define AQSemaphoreRegAddrs                                               0x0E02
#define AQ_SEMAPHORE_MSB                                                      15
#define AQ_SEMAPHORE_LSB                                                       0
#define AQ_SEMAPHORE_BLK                                                       0
#define AQ_SEMAPHORE_Count                                                     1
#define AQ_SEMAPHORE_FieldMask                                        0x00001F1F
#define AQ_SEMAPHORE_ReadMask                                         0x00001F1F
#define AQ_SEMAPHORE_WriteMask                                        0x00001F1F
#define AQ_SEMAPHORE_ResetValue                                       0x00000000

#define AQ_SEMAPHORE_SOURCE                                                4 : 0
#define AQ_SEMAPHORE_SOURCE_End                                                4
#define AQ_SEMAPHORE_SOURCE_Start                                              0
#define AQ_SEMAPHORE_SOURCE_Type                                             U05
#define   AQ_SEMAPHORE_SOURCE_FRONT_END                                     0x01
#define   AQ_SEMAPHORE_SOURCE_PIXEL_ENGINE                                  0x07
#define   AQ_SEMAPHORE_SOURCE_DRAWING_ENGINE                                0x0B

#define AQ_SEMAPHORE_DESTINATION                                          12 : 8
#define AQ_SEMAPHORE_DESTINATION_End                                          12
#define AQ_SEMAPHORE_DESTINATION_Start                                         8
#define AQ_SEMAPHORE_DESTINATION_Type                                        U05
#define   AQ_SEMAPHORE_DESTINATION_FRONT_END                                0x01
#define   AQ_SEMAPHORE_DESTINATION_PIXEL_ENGINE                             0x07
#define   AQ_SEMAPHORE_DESTINATION_DRAWING_ENGINE                           0x0B

/*******************************************************************************
** State AQFlush
*/

/* Flush the current pipe. */

#define AQFlushRegAddrs                                                   0x0E03
#define AQ_FLUSH_MSB                                                          15
#define AQ_FLUSH_LSB                                                           0
#define AQ_FLUSH_BLK                                                           0
#define AQ_FLUSH_Count                                                         1
#define AQ_FLUSH_FieldMask                                            0x00000008
#define AQ_FLUSH_ReadMask                                             0x00000008
#define AQ_FLUSH_WriteMask                                            0x00000008
#define AQ_FLUSH_ResetValue                                           0x00000000

/* Flush the 2D pixel cache. */
#define AQ_FLUSH_PE2D_CACHE                                                3 : 3
#define AQ_FLUSH_PE2D_CACHE_End                                                3
#define AQ_FLUSH_PE2D_CACHE_Start                                              3
#define AQ_FLUSH_PE2D_CACHE_Type                                             U01
#define   AQ_FLUSH_PE2D_CACHE_DISABLE                                        0x0
#define   AQ_FLUSH_PE2D_CACHE_ENABLE                                         0x1

/*******************************************************************************
** State AQMMUFlush
*/

/* Flush the virtual addrses lookup cache inside the MC. */

#define AQMMUFlushRegAddrs                                                0x0E04
#define AQMMU_FLUSH_MSB                                                       15
#define AQMMU_FLUSH_LSB                                                        0
#define AQMMU_FLUSH_BLK                                                        0
#define AQMMU_FLUSH_Count                                                      1
#define AQMMU_FLUSH_FieldMask                                         0x00000009
#define AQMMU_FLUSH_ReadMask                                          0x00000009
#define AQMMU_FLUSH_WriteMask                                         0x00000009
#define AQMMU_FLUSH_ResetValue                                        0x00000000

/* Flush the FE address translation caches. */
#define AQMMU_FLUSH_FEMMU                                                  0 : 0
#define AQMMU_FLUSH_FEMMU_End                                                  0
#define AQMMU_FLUSH_FEMMU_Start                                                0
#define AQMMU_FLUSH_FEMMU_Type                                               U01
#define   AQMMU_FLUSH_FEMMU_DISABLE                                          0x0
#define   AQMMU_FLUSH_FEMMU_ENABLE                                           0x1

/* Flush the PE render target address translation caches. */
#define AQMMU_FLUSH_PEMMU                                                  3 : 3
#define AQMMU_FLUSH_PEMMU_End                                                  3
#define AQMMU_FLUSH_PEMMU_Start                                                3
#define AQMMU_FLUSH_PEMMU_Type                                               U01
#define   AQMMU_FLUSH_PEMMU_DISABLE                                          0x0
#define   AQMMU_FLUSH_PEMMU_ENABLE                                           0x1

/*******************************************************************************
** Register AQCmdBufferAddr
*/

/* Base address for the command buffer.  The address must be 64-bit aligned
** and it is always physical. This register cannot be read. To check the value
** of the current fetch address use AQFEDebugCurCmdAdr. Since this is a write
** only register is has no reset value.
*/

#define AQ_CMD_BUFFER_ADDR_Address                                       0x00654
#define AQ_CMD_BUFFER_ADDR_MSB                                                15
#define AQ_CMD_BUFFER_ADDR_LSB                                                 0
#define AQ_CMD_BUFFER_ADDR_BLK                                                 0
#define AQ_CMD_BUFFER_ADDR_Count                                               1
#define AQ_CMD_BUFFER_ADDR_FieldMask                                  0xFFFFFFFF
#define AQ_CMD_BUFFER_ADDR_ReadMask                                   0x00000000
#define AQ_CMD_BUFFER_ADDR_WriteMask                                  0xFFFFFFFC
#define AQ_CMD_BUFFER_ADDR_ResetValue                                 0x00000000

#define AQ_CMD_BUFFER_ADDR_ADDRESS                                        31 : 0
#define AQ_CMD_BUFFER_ADDR_ADDRESS_End                                        30
#define AQ_CMD_BUFFER_ADDR_ADDRESS_Start                                       0
#define AQ_CMD_BUFFER_ADDR_ADDRESS_Type                                      U31

/*******************************************************************************
** Register AQCmdBufferCtrl
*/

/* Since this is a write only register is has no reset value. */

#define AQ_CMD_BUFFER_CTRL_Address                                       0x00658
#define AQ_CMD_BUFFER_CTRL_MSB                                                15
#define AQ_CMD_BUFFER_CTRL_LSB                                                 0
#define AQ_CMD_BUFFER_CTRL_BLK                                                 0
#define AQ_CMD_BUFFER_CTRL_Count                                               1
#define AQ_CMD_BUFFER_CTRL_FieldMask                                  0x0001FFFF
#define AQ_CMD_BUFFER_CTRL_ReadMask                                   0x00010000
#define AQ_CMD_BUFFER_CTRL_WriteMask                                  0x0001FFFF
#define AQ_CMD_BUFFER_CTRL_ResetValue                                 0x00000000

/* Number of 64-bit words to fetch from the command buffer. */
#define AQ_CMD_BUFFER_CTRL_PREFETCH                                       15 : 0
#define AQ_CMD_BUFFER_CTRL_PREFETCH_End                                       15
#define AQ_CMD_BUFFER_CTRL_PREFETCH_Start                                      0
#define AQ_CMD_BUFFER_CTRL_PREFETCH_Type                                     U16

/* Enable the command parser. */
#define AQ_CMD_BUFFER_CTRL_ENABLE                                        16 : 16
#define AQ_CMD_BUFFER_CTRL_ENABLE_End                                         16
#define AQ_CMD_BUFFER_CTRL_ENABLE_Start                                       16
#define AQ_CMD_BUFFER_CTRL_ENABLE_Type                                       U01
#define   AQ_CMD_BUFFER_CTRL_ENABLE_DISABLE                                  0x0
#define   AQ_CMD_BUFFER_CTRL_ENABLE_ENABLE                                   0x1

/*******************************************************************************
** Register AQFEDebugState
*/

#define AQFE_DEBUG_STATE_Address                                         0x00660
#define AQFE_DEBUG_STATE_MSB                                                  15
#define AQFE_DEBUG_STATE_LSB                                                   0
#define AQFE_DEBUG_STATE_BLK                                                   0
#define AQFE_DEBUG_STATE_Count                                                 1
#define AQFE_DEBUG_STATE_FieldMask                                    0x0003FF1F
#define AQFE_DEBUG_STATE_ReadMask                                     0x0003FF1F
#define AQFE_DEBUG_STATE_WriteMask                                    0x00000000
#define AQFE_DEBUG_STATE_ResetValue                                   0x00000000

#define AQFE_DEBUG_STATE_CMD_STATE                                         4 : 0
#define AQFE_DEBUG_STATE_CMD_STATE_End                                         4
#define AQFE_DEBUG_STATE_CMD_STATE_Start                                       0
#define AQFE_DEBUG_STATE_CMD_STATE_Type                                      U05

#define AQFE_DEBUG_STATE_CMD_DMA_STATE                                     9 : 8
#define AQFE_DEBUG_STATE_CMD_DMA_STATE_End                                     9
#define AQFE_DEBUG_STATE_CMD_DMA_STATE_Start                                   8
#define AQFE_DEBUG_STATE_CMD_DMA_STATE_Type                                  U02

#define AQFE_DEBUG_STATE_CMD_FETCH_STATE                                 11 : 10
#define AQFE_DEBUG_STATE_CMD_FETCH_STATE_End                                  11
#define AQFE_DEBUG_STATE_CMD_FETCH_STATE_Start                                10
#define AQFE_DEBUG_STATE_CMD_FETCH_STATE_Type                                U02

#define AQFE_DEBUG_STATE_REQ_DMA_STATE                                   13 : 12
#define AQFE_DEBUG_STATE_REQ_DMA_STATE_End                                    13
#define AQFE_DEBUG_STATE_REQ_DMA_STATE_Start                                  12
#define AQFE_DEBUG_STATE_REQ_DMA_STATE_Type                                  U02

#define AQFE_DEBUG_STATE_CAL_STATE                                       15 : 14
#define AQFE_DEBUG_STATE_CAL_STATE_End                                        15
#define AQFE_DEBUG_STATE_CAL_STATE_Start                                      14
#define AQFE_DEBUG_STATE_CAL_STATE_Type                                      U02

#define AQFE_DEBUG_STATE_VE_REQ_STATE                                    17 : 16
#define AQFE_DEBUG_STATE_VE_REQ_STATE_End                                     17
#define AQFE_DEBUG_STATE_VE_REQ_STATE_Start                                   16
#define AQFE_DEBUG_STATE_VE_REQ_STATE_Type                                   U02

/*******************************************************************************
** Register AQFEDebugCurCmdAdr
*/

/* This is the command decoder address.  The address is always physical so
** the MSB should always be 0.  It has no reset value.
*/

#define AQFE_DEBUG_CUR_CMD_ADR_Address                                   0x00664
#define AQFE_DEBUG_CUR_CMD_ADR_MSB                                            15
#define AQFE_DEBUG_CUR_CMD_ADR_LSB                                             0
#define AQFE_DEBUG_CUR_CMD_ADR_BLK                                             0
#define AQFE_DEBUG_CUR_CMD_ADR_Count                                           1
#define AQFE_DEBUG_CUR_CMD_ADR_FieldMask                              0xFFFFFFF8
#define AQFE_DEBUG_CUR_CMD_ADR_ReadMask                               0xFFFFFFF8
#define AQFE_DEBUG_CUR_CMD_ADR_WriteMask                              0x00000000
#define AQFE_DEBUG_CUR_CMD_ADR_ResetValue                             0x00000000

#define AQFE_DEBUG_CUR_CMD_ADR_CUR_CMD_ADR                                31 : 3
#define AQFE_DEBUG_CUR_CMD_ADR_CUR_CMD_ADR_End                                31
#define AQFE_DEBUG_CUR_CMD_ADR_CUR_CMD_ADR_Start                               3
#define AQFE_DEBUG_CUR_CMD_ADR_CUR_CMD_ADR_Type                              U29

/*******************************************************************************
** Register AQFEDebugCmdLowReg
*/

#define AQFE_DEBUG_CMD_LOW_REG_Address                                   0x00668
#define AQFE_DEBUG_CMD_LOW_REG_MSB                                            15
#define AQFE_DEBUG_CMD_LOW_REG_LSB                                             0
#define AQFE_DEBUG_CMD_LOW_REG_BLK                                             0
#define AQFE_DEBUG_CMD_LOW_REG_Count                                           1
#define AQFE_DEBUG_CMD_LOW_REG_FieldMask                              0xFFFFFFFF
#define AQFE_DEBUG_CMD_LOW_REG_ReadMask                               0xFFFFFFFF
#define AQFE_DEBUG_CMD_LOW_REG_WriteMask                              0x00000000
#define AQFE_DEBUG_CMD_LOW_REG_ResetValue                             0x00000000

/* Command register used by CmdState. */
#define AQFE_DEBUG_CMD_LOW_REG_CMD_LOW_REG                                31 : 0
#define AQFE_DEBUG_CMD_LOW_REG_CMD_LOW_REG_End                                31
#define AQFE_DEBUG_CMD_LOW_REG_CMD_LOW_REG_Start                               0
#define AQFE_DEBUG_CMD_LOW_REG_CMD_LOW_REG_Type                              U32

/*******************************************************************************
** Register AQFEDebugCmdHiReg
*/

#define AQFE_DEBUG_CMD_HI_REG_Address                                    0x0066C
#define AQFE_DEBUG_CMD_HI_REG_MSB                                             15
#define AQFE_DEBUG_CMD_HI_REG_LSB                                              0
#define AQFE_DEBUG_CMD_HI_REG_BLK                                              0
#define AQFE_DEBUG_CMD_HI_REG_Count                                            1
#define AQFE_DEBUG_CMD_HI_REG_FieldMask                               0xFFFFFFFF
#define AQFE_DEBUG_CMD_HI_REG_ReadMask                                0xFFFFFFFF
#define AQFE_DEBUG_CMD_HI_REG_WriteMask                               0x00000000
#define AQFE_DEBUG_CMD_HI_REG_ResetValue                              0x00000000

/* Command register used by CmdState. */
#define AQFE_DEBUG_CMD_HI_REG_CMD_HI_REG                                  31 : 0
#define AQFE_DEBUG_CMD_HI_REG_CMD_HI_REG_End                                  31
#define AQFE_DEBUG_CMD_HI_REG_CMD_HI_REG_Start                                 0
#define AQFE_DEBUG_CMD_HI_REG_CMD_HI_REG_Type                                U32

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
** State AQDESrcAddress
*/

/* 32-bit aligned base address of the source surface. */

#define AQDESrcAddressRegAddrs                                            0x0480
#define AQDE_SRC_ADDRESS_MSB                                                  15
#define AQDE_SRC_ADDRESS_LSB                                                   0
#define AQDE_SRC_ADDRESS_BLK                                                   0
#define AQDE_SRC_ADDRESS_Count                                                 1
#define AQDE_SRC_ADDRESS_FieldMask                                    0xFFFFFFFF
#define AQDE_SRC_ADDRESS_ReadMask                                     0xFFFFFFFC
#define AQDE_SRC_ADDRESS_WriteMask                                    0xFFFFFFFC
#define AQDE_SRC_ADDRESS_ResetValue                                   0x00000000

#define AQDE_SRC_ADDRESS_ADDRESS                                          31 : 0
#define AQDE_SRC_ADDRESS_ADDRESS_End                                          30
#define AQDE_SRC_ADDRESS_ADDRESS_Start                                         0
#define AQDE_SRC_ADDRESS_ADDRESS_Type                                        U31

/*******************************************************************************
** State AQDESrcStride
*/

/* Stride of the source surface in bytes. To calculate the stride multiply
** the surface width in pixels (8-pixel aligned) by the number of bytes per
** pixel.
*/

#define AQDESrcStrideRegAddrs                                             0x0481
#define AQDE_SRC_STRIDE_MSB                                                   15
#define AQDE_SRC_STRIDE_LSB                                                    0
#define AQDE_SRC_STRIDE_BLK                                                    0
#define AQDE_SRC_STRIDE_Count                                                  1
#define AQDE_SRC_STRIDE_FieldMask                                     0x0003FFFF
#define AQDE_SRC_STRIDE_ReadMask                                      0x0003FFFC
#define AQDE_SRC_STRIDE_WriteMask                                     0x0003FFFC
#define AQDE_SRC_STRIDE_ResetValue                                    0x00000000

#define AQDE_SRC_STRIDE_STRIDE                                            17 : 0
#define AQDE_SRC_STRIDE_STRIDE_End                                            17
#define AQDE_SRC_STRIDE_STRIDE_Start                                           0
#define AQDE_SRC_STRIDE_STRIDE_Type                                          U18

/*******************************************************************************
** State AQDESrcRotationConfig
*/

/* 90 degree rotation configuration for the source surface. Width field
** specifies the width of the surface in pixels.
*/

#define AQDESrcRotationConfigRegAddrs                                     0x0482
#define AQDE_SRC_ROTATION_CONFIG_MSB                                          15
#define AQDE_SRC_ROTATION_CONFIG_LSB                                           0
#define AQDE_SRC_ROTATION_CONFIG_BLK                                           0
#define AQDE_SRC_ROTATION_CONFIG_Count                                         1
#define AQDE_SRC_ROTATION_CONFIG_FieldMask                            0x0001FFFF
#define AQDE_SRC_ROTATION_CONFIG_ReadMask                             0x0001FFFF
#define AQDE_SRC_ROTATION_CONFIG_WriteMask                            0x0001FFFF
#define AQDE_SRC_ROTATION_CONFIG_ResetValue                           0x00000000

#define AQDE_SRC_ROTATION_CONFIG_WIDTH                                    15 : 0
#define AQDE_SRC_ROTATION_CONFIG_WIDTH_End                                    15
#define AQDE_SRC_ROTATION_CONFIG_WIDTH_Start                                   0
#define AQDE_SRC_ROTATION_CONFIG_WIDTH_Type                                  U16

#define AQDE_SRC_ROTATION_CONFIG_ROTATION                                16 : 16
#define AQDE_SRC_ROTATION_CONFIG_ROTATION_End                                 16
#define AQDE_SRC_ROTATION_CONFIG_ROTATION_Start                               16
#define AQDE_SRC_ROTATION_CONFIG_ROTATION_Type                               U01
#define   AQDE_SRC_ROTATION_CONFIG_ROTATION_NORMAL                           0x0
#define   AQDE_SRC_ROTATION_CONFIG_ROTATION_ROTATED                          0x1

/*******************************************************************************
** State AQDESrcConfig
*/

/* Source surface configuration register. */

#define AQDESrcConfigRegAddrs                                             0x0483
#define AQDE_SRC_CONFIG_MSB                                                   15
#define AQDE_SRC_CONFIG_LSB                                                    0
#define AQDE_SRC_CONFIG_BLK                                                    0
#define AQDE_SRC_CONFIG_Count                                                  1
#define AQDE_SRC_CONFIG_FieldMask                                     0xFF31B1FF
#define AQDE_SRC_CONFIG_ReadMask                                      0xFF31B1FF
#define AQDE_SRC_CONFIG_WriteMask                                     0xFF31B1FF
#define AQDE_SRC_CONFIG_ResetValue                                    0x00000000

/* Control source endianess. */
#define AQDE_SRC_CONFIG_ENDIAN_CONTROL                                   31 : 30
#define AQDE_SRC_CONFIG_ENDIAN_CONTROL_End                                    31
#define AQDE_SRC_CONFIG_ENDIAN_CONTROL_Start                                  30
#define AQDE_SRC_CONFIG_ENDIAN_CONTROL_Type                                  U02
#define   AQDE_SRC_CONFIG_ENDIAN_CONTROL_NO_SWAP                             0x0
#define   AQDE_SRC_CONFIG_ENDIAN_CONTROL_SWAP_WORD                           0x1
#define   AQDE_SRC_CONFIG_ENDIAN_CONTROL_SWAP_DWORD                          0x2

/* Disable 420 L2 cache  NOTE: the field is valid for chips with 420 L2 cache **
** defined.                                                                   */
#define AQDE_SRC_CONFIG_DISABLE420_L2_CACHE                              29 : 29
#define AQDE_SRC_CONFIG_DISABLE420_L2_CACHE_End                               29
#define AQDE_SRC_CONFIG_DISABLE420_L2_CACHE_Start                             29
#define AQDE_SRC_CONFIG_DISABLE420_L2_CACHE_Type                             U01
#define   AQDE_SRC_CONFIG_DISABLE420_L2_CACHE_ENABLED                        0x0
#define   AQDE_SRC_CONFIG_DISABLE420_L2_CACHE_DISABLED                       0x1

/* Defines the pixel format of the source surface. */
#define AQDE_SRC_CONFIG_SOURCE_FORMAT                                    28 : 24
#define AQDE_SRC_CONFIG_SOURCE_FORMAT_End                                     28
#define AQDE_SRC_CONFIG_SOURCE_FORMAT_Start                                   24
#define AQDE_SRC_CONFIG_SOURCE_FORMAT_Type                                   U05
#define   AQDE_SRC_CONFIG_SOURCE_FORMAT_X4R4G4B4                            0x00
#define   AQDE_SRC_CONFIG_SOURCE_FORMAT_A4R4G4B4                            0x01
#define   AQDE_SRC_CONFIG_SOURCE_FORMAT_X1R5G5B5                            0x02
#define   AQDE_SRC_CONFIG_SOURCE_FORMAT_A1R5G5B5                            0x03
#define   AQDE_SRC_CONFIG_SOURCE_FORMAT_R5G6B5                              0x04
#define   AQDE_SRC_CONFIG_SOURCE_FORMAT_X8R8G8B8                            0x05
#define   AQDE_SRC_CONFIG_SOURCE_FORMAT_A8R8G8B8                            0x06
#define   AQDE_SRC_CONFIG_SOURCE_FORMAT_YUY2                                0x07
#define   AQDE_SRC_CONFIG_SOURCE_FORMAT_UYVY                                0x08
#define   AQDE_SRC_CONFIG_SOURCE_FORMAT_INDEX8                              0x09
#define   AQDE_SRC_CONFIG_SOURCE_FORMAT_MONOCHROME                          0x0A
#define   AQDE_SRC_CONFIG_SOURCE_FORMAT_YV12                                0x0F
#define   AQDE_SRC_CONFIG_SOURCE_FORMAT_A8                                  0x10
#define   AQDE_SRC_CONFIG_SOURCE_FORMAT_NV12                                0x11
#define   AQDE_SRC_CONFIG_SOURCE_FORMAT_NV16                                0x12
#define   AQDE_SRC_CONFIG_SOURCE_FORMAT_RG16                                0x13

/* Color channel swizzles. */
#define AQDE_SRC_CONFIG_SWIZZLE                                          21 : 20
#define AQDE_SRC_CONFIG_SWIZZLE_End                                           21
#define AQDE_SRC_CONFIG_SWIZZLE_Start                                         20
#define AQDE_SRC_CONFIG_SWIZZLE_Type                                         U02
#define   AQDE_SRC_CONFIG_SWIZZLE_ARGB                                       0x0
#define   AQDE_SRC_CONFIG_SWIZZLE_RGBA                                       0x1
#define   AQDE_SRC_CONFIG_SWIZZLE_ABGR                                       0x2
#define   AQDE_SRC_CONFIG_SWIZZLE_BGRA                                       0x3

/* Mono expansion: if 0, transparency color will be 0, otherwise transparency **
** color will be 1.                                                           */
#define AQDE_SRC_CONFIG_MONO_TRANSPARENCY                                15 : 15
#define AQDE_SRC_CONFIG_MONO_TRANSPARENCY_End                                 15
#define AQDE_SRC_CONFIG_MONO_TRANSPARENCY_Start                               15
#define AQDE_SRC_CONFIG_MONO_TRANSPARENCY_Type                               U01
#define   AQDE_SRC_CONFIG_MONO_TRANSPARENCY_BACKGROUND                       0x0
#define   AQDE_SRC_CONFIG_MONO_TRANSPARENCY_FOREGROUND                       0x1

/* Mono expansion or masked blit: stream packing in pixels. Determines how    **
** many horizontal pixels are there per each 32-bit chunk. For example, if    **
** set to Packed8, each 32-bit chunk is 8-pixel wide, which also means that   **
** it defines 4 vertical lines of pixels.                                     */
#define AQDE_SRC_CONFIG_PACK                                             13 : 12
#define AQDE_SRC_CONFIG_PACK_End                                              13
#define AQDE_SRC_CONFIG_PACK_Start                                            12
#define AQDE_SRC_CONFIG_PACK_Type                                            U02
#define   AQDE_SRC_CONFIG_PACK_PACKED8                                       0x0
#define   AQDE_SRC_CONFIG_PACK_PACKED16                                      0x1
#define   AQDE_SRC_CONFIG_PACK_PACKED32                                      0x2
#define   AQDE_SRC_CONFIG_PACK_UNPACKED                                      0x3

/* Source data location: set to STREAM for mono expansion blits or masked     **
** blits. For mono expansion blits the complete bitmap comes from the command **
** stream. For masked blits the source data comes from the memory and the     **
** mask from the command stream.                                              */
#define AQDE_SRC_CONFIG_LOCATION                                           8 : 8
#define AQDE_SRC_CONFIG_LOCATION_End                                           8
#define AQDE_SRC_CONFIG_LOCATION_Start                                         8
#define AQDE_SRC_CONFIG_LOCATION_Type                                        U01
#define   AQDE_SRC_CONFIG_LOCATION_MEMORY                                    0x0
#define   AQDE_SRC_CONFIG_LOCATION_STREAM                                    0x1

/* Source linear/tiled address computation control. */
#define AQDE_SRC_CONFIG_TILED                                              7 : 7
#define AQDE_SRC_CONFIG_TILED_End                                              7
#define AQDE_SRC_CONFIG_TILED_Start                                            7
#define AQDE_SRC_CONFIG_TILED_Type                                           U01
#define   AQDE_SRC_CONFIG_TILED_DISABLED                                     0x0
#define   AQDE_SRC_CONFIG_TILED_ENABLED                                      0x1

/* If set to ABSOLUTE, the source coordinates are treated as absolute         **
** coordinates inside the source surface. If set to RELATIVE, the source      **
** coordinates are treated as the offsets from the destination coordinates    **
** with the source size equal to the size of the destination.                 */
#define AQDE_SRC_CONFIG_SRC_RELATIVE                                       6 : 6
#define AQDE_SRC_CONFIG_SRC_RELATIVE_End                                       6
#define AQDE_SRC_CONFIG_SRC_RELATIVE_Start                                     6
#define AQDE_SRC_CONFIG_SRC_RELATIVE_Type                                    U01
#define   AQDE_SRC_CONFIG_SRC_RELATIVE_ABSOLUTE                              0x0
#define   AQDE_SRC_CONFIG_SRC_RELATIVE_RELATIVE                              0x1

/*******************************************************************************
** State AQDESrcOrigin
*/

/* Absolute or relative (see SRC_RELATIVE field of AQDESrcConfig register) X
** and Y coordinates in pixels of the top left corner of the source rectangle
** within the source surface.
*/

#define AQDESrcOriginRegAddrs                                             0x0484
#define AQDE_SRC_ORIGIN_MSB                                                   15
#define AQDE_SRC_ORIGIN_LSB                                                    0
#define AQDE_SRC_ORIGIN_BLK                                                    0
#define AQDE_SRC_ORIGIN_Count                                                  1
#define AQDE_SRC_ORIGIN_FieldMask                                     0xFFFFFFFF
#define AQDE_SRC_ORIGIN_ReadMask                                      0xFFFFFFFF
#define AQDE_SRC_ORIGIN_WriteMask                                     0xFFFFFFFF
#define AQDE_SRC_ORIGIN_ResetValue                                    0x00000000

#define AQDE_SRC_ORIGIN_Y                                                31 : 16
#define AQDE_SRC_ORIGIN_Y_End                                                 31
#define AQDE_SRC_ORIGIN_Y_Start                                               16
#define AQDE_SRC_ORIGIN_Y_Type                                               U16

#define AQDE_SRC_ORIGIN_X                                                 15 : 0
#define AQDE_SRC_ORIGIN_X_End                                                 15
#define AQDE_SRC_ORIGIN_X_Start                                                0
#define AQDE_SRC_ORIGIN_X_Type                                               U16

/*******************************************************************************
** State AQDESrcSize
*/

/* Width and height of the source rectangle in pixels. If the source is
** relative (see SRC_RELATIVE field of AQDESrcConfig register) or a regular
** bitblt is being performed without stretching, this register is ignored and
** the source size is assumed to be the same as the destination.
*/

#define AQDESrcSizeRegAddrs                                               0x0485
#define AQDE_SRC_SIZE_MSB                                                     15
#define AQDE_SRC_SIZE_LSB                                                      0
#define AQDE_SRC_SIZE_BLK                                                      0
#define AQDE_SRC_SIZE_Count                                                    1
#define AQDE_SRC_SIZE_FieldMask                                       0xFFFFFFFF
#define AQDE_SRC_SIZE_ReadMask                                        0xFFFFFFFF
#define AQDE_SRC_SIZE_WriteMask                                       0xFFFFFFFF
#define AQDE_SRC_SIZE_ResetValue                                      0x00000000

#define AQDE_SRC_SIZE_Y                                                  31 : 16
#define AQDE_SRC_SIZE_Y_End                                                   31
#define AQDE_SRC_SIZE_Y_Start                                                 16
#define AQDE_SRC_SIZE_Y_Type                                                 U16

#define AQDE_SRC_SIZE_X                                                   15 : 0
#define AQDE_SRC_SIZE_X_End                                                   15
#define AQDE_SRC_SIZE_X_Start                                                  0
#define AQDE_SRC_SIZE_X_Type                                                 U16

/*******************************************************************************
** State AQDESrcColorBg
*/

/* In mono expansion defines the source color if the mono pixel is 0. The color
** must be set in A8R8G8B8 format. In color blits defines the source
** transparency color and must be of the same format as the source surface.
*/

#define AQDESrcColorBgRegAddrs                                            0x0486
#define AQDE_SRC_COLOR_BG_MSB                                                 15
#define AQDE_SRC_COLOR_BG_LSB                                                  0
#define AQDE_SRC_COLOR_BG_BLK                                                  0
#define AQDE_SRC_COLOR_BG_Count                                                1
#define AQDE_SRC_COLOR_BG_FieldMask                                   0xFFFFFFFF
#define AQDE_SRC_COLOR_BG_ReadMask                                    0xFFFFFFFF
#define AQDE_SRC_COLOR_BG_WriteMask                                   0xFFFFFFFF
#define AQDE_SRC_COLOR_BG_ResetValue                                  0x00000000

#define AQDE_SRC_COLOR_BG_ALPHA                                          31 : 24
#define AQDE_SRC_COLOR_BG_ALPHA_End                                           31
#define AQDE_SRC_COLOR_BG_ALPHA_Start                                         24
#define AQDE_SRC_COLOR_BG_ALPHA_Type                                         U08

#define AQDE_SRC_COLOR_BG_RED                                            23 : 16
#define AQDE_SRC_COLOR_BG_RED_End                                             23
#define AQDE_SRC_COLOR_BG_RED_Start                                           16
#define AQDE_SRC_COLOR_BG_RED_Type                                           U08

#define AQDE_SRC_COLOR_BG_GREEN                                           15 : 8
#define AQDE_SRC_COLOR_BG_GREEN_End                                           15
#define AQDE_SRC_COLOR_BG_GREEN_Start                                          8
#define AQDE_SRC_COLOR_BG_GREEN_Type                                         U08

#define AQDE_SRC_COLOR_BG_BLUE                                             7 : 0
#define AQDE_SRC_COLOR_BG_BLUE_End                                             7
#define AQDE_SRC_COLOR_BG_BLUE_Start                                           0
#define AQDE_SRC_COLOR_BG_BLUE_Type                                          U08

/*******************************************************************************
** State AQDESrcColorFg
*/

/* In mono expansion defines the source color if the mono pixel is 1. The color
** must be set in A8R8G8B8.
*/

#define AQDESrcColorFgRegAddrs                                            0x0487
#define AQDE_SRC_COLOR_FG_MSB                                                 15
#define AQDE_SRC_COLOR_FG_LSB                                                  0
#define AQDE_SRC_COLOR_FG_BLK                                                  0
#define AQDE_SRC_COLOR_FG_Count                                                1
#define AQDE_SRC_COLOR_FG_FieldMask                                   0xFFFFFFFF
#define AQDE_SRC_COLOR_FG_ReadMask                                    0xFFFFFFFF
#define AQDE_SRC_COLOR_FG_WriteMask                                   0xFFFFFFFF
#define AQDE_SRC_COLOR_FG_ResetValue                                  0x00000000

#define AQDE_SRC_COLOR_FG_ALPHA                                          31 : 24
#define AQDE_SRC_COLOR_FG_ALPHA_End                                           31
#define AQDE_SRC_COLOR_FG_ALPHA_Start                                         24
#define AQDE_SRC_COLOR_FG_ALPHA_Type                                         U08

#define AQDE_SRC_COLOR_FG_RED                                            23 : 16
#define AQDE_SRC_COLOR_FG_RED_End                                             23
#define AQDE_SRC_COLOR_FG_RED_Start                                           16
#define AQDE_SRC_COLOR_FG_RED_Type                                           U08

#define AQDE_SRC_COLOR_FG_GREEN                                           15 : 8
#define AQDE_SRC_COLOR_FG_GREEN_End                                           15
#define AQDE_SRC_COLOR_FG_GREEN_Start                                          8
#define AQDE_SRC_COLOR_FG_GREEN_Type                                         U08

#define AQDE_SRC_COLOR_FG_BLUE                                             7 : 0
#define AQDE_SRC_COLOR_FG_BLUE_End                                             7
#define AQDE_SRC_COLOR_FG_BLUE_Start                                           0
#define AQDE_SRC_COLOR_FG_BLUE_Type                                          U08

/*******************************************************************************
** State AQDEStretchFactorLow
*/

#define AQDEStretchFactorLowRegAddrs                                      0x0488
#define AQDE_STRETCH_FACTOR_LOW_MSB                                           15
#define AQDE_STRETCH_FACTOR_LOW_LSB                                            0
#define AQDE_STRETCH_FACTOR_LOW_BLK                                            0
#define AQDE_STRETCH_FACTOR_LOW_Count                                          1
#define AQDE_STRETCH_FACTOR_LOW_FieldMask                             0x7FFFFFFF
#define AQDE_STRETCH_FACTOR_LOW_ReadMask                              0x7FFFFFFF
#define AQDE_STRETCH_FACTOR_LOW_WriteMask                             0x7FFFFFFF
#define AQDE_STRETCH_FACTOR_LOW_ResetValue                            0x00000000

/* Horizontal stretch factor in 15.16 fixed point format. The value is        **
** calculated using the following formula: factor = ((srcWidth - 1) << 16) /  **
** (dstWidth - 1). Stretch blit uses only the integer part of the value,      **
** while Filter blit uses all 31 bits.                                        */
#define AQDE_STRETCH_FACTOR_LOW_X                                         30 : 0
#define AQDE_STRETCH_FACTOR_LOW_X_End                                         30
#define AQDE_STRETCH_FACTOR_LOW_X_Start                                        0
#define AQDE_STRETCH_FACTOR_LOW_X_Type                                       U31

/*******************************************************************************
** State AQDEStretchFactorHigh
*/

#define AQDEStretchFactorHighRegAddrs                                     0x0489
#define AQDE_STRETCH_FACTOR_HIGH_MSB                                          15
#define AQDE_STRETCH_FACTOR_HIGH_LSB                                           0
#define AQDE_STRETCH_FACTOR_LOW_HIGH_BLK                                       0
#define AQDE_STRETCH_FACTOR_HIGH_Count                                         1
#define AQDE_STRETCH_FACTOR_HIGH_FieldMask                            0x7FFFFFFF
#define AQDE_STRETCH_FACTOR_HIGH_ReadMask                             0x7FFFFFFF
#define AQDE_STRETCH_FACTOR_HIGH_WriteMask                            0x7FFFFFFF
#define AQDE_STRETCH_FACTOR_HIGH_ResetValue                           0x00000000

/* Vertical stretch factor in 15.16 fixed point format. The value is          **
** calculated using the following formula: factor = ((srcHeight - 1) << 16) / **
** (dstHeight - 1). Stretch blit uses only the integer part of the value,     **
** while Filter blit uses all 31 bits.                                        */
#define AQDE_STRETCH_FACTOR_HIGH_Y                                        30 : 0
#define AQDE_STRETCH_FACTOR_HIGH_Y_End                                        30
#define AQDE_STRETCH_FACTOR_HIGH_Y_Start                                       0
#define AQDE_STRETCH_FACTOR_HIGH_Y_Type                                      U31

/*******************************************************************************
** State AQDEDestAddress
*/

/* 32-bit aligned base address of the destination surface. */

#define AQDEDestAddressRegAddrs                                           0x048A
#define AQDE_DEST_ADDRESS_MSB                                                 15
#define AQDE_DEST_ADDRESS_LSB                                                  0
#define AQDE_DEST_ADDRESS_BLK                                                  0
#define AQDE_DEST_ADDRESS_Count                                                1
#define AQDE_DEST_ADDRESS_FieldMask                                   0xFFFFFFFF
#define AQDE_DEST_ADDRESS_ReadMask                                    0xFFFFFFFC
#define AQDE_DEST_ADDRESS_WriteMask                                   0xFFFFFFFC
#define AQDE_DEST_ADDRESS_ResetValue                                  0x00000000

#define AQDE_DEST_ADDRESS_ADDRESS                                         31 : 0
#define AQDE_DEST_ADDRESS_ADDRESS_End                                         30
#define AQDE_DEST_ADDRESS_ADDRESS_Start                                        0
#define AQDE_DEST_ADDRESS_ADDRESS_Type                                       U31

/*******************************************************************************
** State AQDEDestStride
*/

/* Stride of the destination surface in bytes. To calculate the stride
** multiply the surface width in pixels (8-pixel aligned) by the number of
** bytes per pixel.
*/

#define AQDEDestStrideRegAddrs                                            0x048B
#define AQDE_DEST_STRIDE_MSB                                                  15
#define AQDE_DEST_STRIDE_LSB                                                   0
#define AQDE_DEST_STRIDE_BLK                                                   0
#define AQDE_DEST_STRIDE_Count                                                 1
#define AQDE_DEST_STRIDE_FieldMask                                    0x0003FFFF
#define AQDE_DEST_STRIDE_ReadMask                                     0x0003FFFC
#define AQDE_DEST_STRIDE_WriteMask                                    0x0003FFFC
#define AQDE_DEST_STRIDE_ResetValue                                   0x00000000

#define AQDE_DEST_STRIDE_STRIDE                                           17 : 0
#define AQDE_DEST_STRIDE_STRIDE_End                                           17
#define AQDE_DEST_STRIDE_STRIDE_Start                                          0
#define AQDE_DEST_STRIDE_STRIDE_Type                                         U18

/*******************************************************************************
** State AQDEDestRotationConfig
*/

/* 90 degree rotation configuration for the destination surface. Width field
** specifies the width of the surface in pixels.
*/

#define AQDEDestRotationConfigRegAddrs                                    0x048C
#define AQDE_DEST_ROTATION_CONFIG_MSB                                         15
#define AQDE_DEST_ROTATION_CONFIG_LSB                                          0
#define AQDE_DEST_ROTATION_CONFIG_BLK                                          0
#define AQDE_DEST_ROTATION_CONFIG_Count                                        1
#define AQDE_DEST_ROTATION_CONFIG_FieldMask                           0x0001FFFF
#define AQDE_DEST_ROTATION_CONFIG_ReadMask                            0x0001FFFF
#define AQDE_DEST_ROTATION_CONFIG_WriteMask                           0x0001FFFF
#define AQDE_DEST_ROTATION_CONFIG_ResetValue                          0x00000000

#define AQDE_DEST_ROTATION_CONFIG_WIDTH                                   15 : 0
#define AQDE_DEST_ROTATION_CONFIG_WIDTH_End                                   15
#define AQDE_DEST_ROTATION_CONFIG_WIDTH_Start                                  0
#define AQDE_DEST_ROTATION_CONFIG_WIDTH_Type                                 U16

#define AQDE_DEST_ROTATION_CONFIG_ROTATION                               16 : 16
#define AQDE_DEST_ROTATION_CONFIG_ROTATION_End                                16
#define AQDE_DEST_ROTATION_CONFIG_ROTATION_Start                              16
#define AQDE_DEST_ROTATION_CONFIG_ROTATION_Type                              U01
#define   AQDE_DEST_ROTATION_CONFIG_ROTATION_NORMAL                          0x0
#define   AQDE_DEST_ROTATION_CONFIG_ROTATION_ROTATED                         0x1

/*******************************************************************************
** State AQDEDestConfig
*/

/* Destination surface configuration register. */

#define AQDEDestConfigRegAddrs                                            0x048D
#define AQDE_DEST_CONFIG_MSB                                                  15
#define AQDE_DEST_CONFIG_LSB                                                   0
#define AQDE_DEST_CONFIG_BLK                                                   0
#define AQDE_DEST_CONFIG_Count                                                 1
#define AQDE_DEST_CONFIG_FieldMask                                    0x0733F11F
#define AQDE_DEST_CONFIG_ReadMask                                     0x0733F11F
#define AQDE_DEST_CONFIG_WriteMask                                    0x0733F11F
#define AQDE_DEST_CONFIG_ResetValue                                   0x00000000

/* MinorTile. */
#define AQDE_DEST_CONFIG_MINOR_TILED                                     26 : 26
#define AQDE_DEST_CONFIG_MINOR_TILED_End                                      26
#define AQDE_DEST_CONFIG_MINOR_TILED_Start                                    26
#define AQDE_DEST_CONFIG_MINOR_TILED_Type                                    U01
#define   AQDE_DEST_CONFIG_MINOR_TILED_DISABLED                              0x0
#define   AQDE_DEST_CONFIG_MINOR_TILED_ENABLED                               0x1

/* Performance fix for de. */
#define AQDE_DEST_CONFIG_INTER_TILE_PER_FIX                              25 : 25
#define AQDE_DEST_CONFIG_INTER_TILE_PER_FIX_End                               25
#define AQDE_DEST_CONFIG_INTER_TILE_PER_FIX_Start                             25
#define AQDE_DEST_CONFIG_INTER_TILE_PER_FIX_Type                             U01
#define   AQDE_DEST_CONFIG_INTER_TILE_PER_FIX_DISABLED                       0x1
#define   AQDE_DEST_CONFIG_INTER_TILE_PER_FIX_ENABLED                        0x0

/* Control GDI Strecth Blit. */
#define AQDE_DEST_CONFIG_GDI_STRE                                        24 : 24
#define AQDE_DEST_CONFIG_GDI_STRE_End                                         24
#define AQDE_DEST_CONFIG_GDI_STRE_Start                                       24
#define AQDE_DEST_CONFIG_GDI_STRE_Type                                       U01
#define   AQDE_DEST_CONFIG_GDI_STRE_DISABLED                                 0x0
#define   AQDE_DEST_CONFIG_GDI_STRE_ENABLED                                  0x1

/* Control destination endianess. */
#define AQDE_DEST_CONFIG_ENDIAN_CONTROL                                  21 : 20
#define AQDE_DEST_CONFIG_ENDIAN_CONTROL_End                                   21
#define AQDE_DEST_CONFIG_ENDIAN_CONTROL_Start                                 20
#define AQDE_DEST_CONFIG_ENDIAN_CONTROL_Type                                 U02
#define   AQDE_DEST_CONFIG_ENDIAN_CONTROL_NO_SWAP                            0x0
#define   AQDE_DEST_CONFIG_ENDIAN_CONTROL_SWAP_WORD                          0x1
#define   AQDE_DEST_CONFIG_ENDIAN_CONTROL_SWAP_DWORD                         0x2

/* Color channel swizzles. */
#define AQDE_DEST_CONFIG_SWIZZLE                                         17 : 16
#define AQDE_DEST_CONFIG_SWIZZLE_End                                          17
#define AQDE_DEST_CONFIG_SWIZZLE_Start                                        16
#define AQDE_DEST_CONFIG_SWIZZLE_Type                                        U02
#define   AQDE_DEST_CONFIG_SWIZZLE_ARGB                                      0x0
#define   AQDE_DEST_CONFIG_SWIZZLE_RGBA                                      0x1
#define   AQDE_DEST_CONFIG_SWIZZLE_ABGR                                      0x2
#define   AQDE_DEST_CONFIG_SWIZZLE_BGRA                                      0x3

/* Determines the type of primitive to be rendered. BIT_BLT_REVERSED and      **
** INVALID_COMMAND values are defined for internal use and should not be      **
** used.                                                                      */
#define AQDE_DEST_CONFIG_COMMAND                                         15 : 12
#define AQDE_DEST_CONFIG_COMMAND_End                                          15
#define AQDE_DEST_CONFIG_COMMAND_Start                                        12
#define AQDE_DEST_CONFIG_COMMAND_Type                                        U04
#define   AQDE_DEST_CONFIG_COMMAND_CLEAR                                     0x0
#define   AQDE_DEST_CONFIG_COMMAND_LINE                                      0x1
#define   AQDE_DEST_CONFIG_COMMAND_BIT_BLT                                   0x2
#define   AQDE_DEST_CONFIG_COMMAND_BIT_BLT_REVERSED                          0x3
#define   AQDE_DEST_CONFIG_COMMAND_STRETCH_BLT                               0x4
#define   AQDE_DEST_CONFIG_COMMAND_HOR_FILTER_BLT                            0x5
#define   AQDE_DEST_CONFIG_COMMAND_VER_FILTER_BLT                            0x6
#define   AQDE_DEST_CONFIG_COMMAND_ONE_PASS_FILTER_BLT                       0x7
#define   AQDE_DEST_CONFIG_COMMAND_MULTI_SOURCE_BLT                          0x8

/* Destination linear/tiled address computation control. Reserved field for   **
** future expansion. */
#define AQDE_DEST_CONFIG_TILED                                             8 : 8
#define AQDE_DEST_CONFIG_TILED_End                                             8
#define AQDE_DEST_CONFIG_TILED_Start                                           8
#define AQDE_DEST_CONFIG_TILED_Type                                          U01
#define   AQDE_DEST_CONFIG_TILED_DISABLED                                    0x0
#define   AQDE_DEST_CONFIG_TILED_ENABLED                                     0x1

/* Defines the pixel format of the destination surface. */
#define AQDE_DEST_CONFIG_FORMAT                                            4 : 0
#define AQDE_DEST_CONFIG_FORMAT_End                                            4
#define AQDE_DEST_CONFIG_FORMAT_Start                                          0
#define AQDE_DEST_CONFIG_FORMAT_Type                                         U05
#define   AQDE_DEST_CONFIG_FORMAT_X4R4G4B4                                  0x00
#define   AQDE_DEST_CONFIG_FORMAT_A4R4G4B4                                  0x01
#define   AQDE_DEST_CONFIG_FORMAT_X1R5G5B5                                  0x02
#define   AQDE_DEST_CONFIG_FORMAT_A1R5G5B5                                  0x03
#define   AQDE_DEST_CONFIG_FORMAT_R5G6B5                                    0x04
#define   AQDE_DEST_CONFIG_FORMAT_X8R8G8B8                                  0x05
#define   AQDE_DEST_CONFIG_FORMAT_A8R8G8B8                                  0x06
#define   AQDE_DEST_CONFIG_FORMAT_YUY2                                      0x07
#define   AQDE_DEST_CONFIG_FORMAT_UYVY                                      0x08
#define   AQDE_DEST_CONFIG_FORMAT_INDEX8                                    0x09
#define   AQDE_DEST_CONFIG_FORMAT_MONOCHROME                                0x0A
#define   AQDE_DEST_CONFIG_FORMAT_YV12                                      0x0F
#define   AQDE_DEST_CONFIG_FORMAT_A8                                        0x10
#define   AQDE_DEST_CONFIG_FORMAT_NV12                                      0x11
#define   AQDE_DEST_CONFIG_FORMAT_NV16                                      0x12
#define   AQDE_DEST_CONFIG_FORMAT_RG16                                      0x13

/*******************************************************************************
** State AQDEFilterKernel
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

#define AQDEFilterKernelRegAddrs                                          0x0600
#define AQDE_FILTER_KERNEL_MSB                                                15
#define AQDE_FILTER_KERNEL_LSB                                                 7
#define AQDE_FILTER_KERNEL_BLK                                                 7
#define AQDE_FILTER_KERNEL_Count                                             128
#define AQDE_FILTER_KERNEL_FieldMask                                  0xFFFFFFFF
#define AQDE_FILTER_KERNEL_ReadMask                                   0xFFFFFFFF
#define AQDE_FILTER_KERNEL_WriteMask                                  0xFFFFFFFF
#define AQDE_FILTER_KERNEL_ResetValue                                 0x00000000

#define AQDE_FILTER_KERNEL_COEFFICIENT0                                   15 : 0
#define AQDE_FILTER_KERNEL_COEFFICIENT0_End                                   15
#define AQDE_FILTER_KERNEL_COEFFICIENT0_Start                                  0
#define AQDE_FILTER_KERNEL_COEFFICIENT0_Type                                 U16

#define AQDE_FILTER_KERNEL_COEFFICIENT1                                  31 : 16
#define AQDE_FILTER_KERNEL_COEFFICIENT1_End                                   31
#define AQDE_FILTER_KERNEL_COEFFICIENT1_Start                                 16
#define AQDE_FILTER_KERNEL_COEFFICIENT1_Type                                 U16

/*******************************************************************************
** State AQDEHoriFilterKernel
*/

/* One Pass filter Filter blit hori coefficient table. */

#define AQDEHoriFilterKernelRegAddrs                                      0x0A00
#define AQDE_HORI_FILTER_KERNEL_MSB                                           15
#define AQDE_HORI_FILTER_KERNEL_LSB                                            7
#define AQDE_HORI_FILTER_KERNEL_BLK                                            7
#define AQDE_HORI_FILTER_KERNEL_Count                                        128
#define AQDE_HORI_FILTER_KERNEL_FieldMask                             0xFFFFFFFF
#define AQDE_HORI_FILTER_KERNEL_ReadMask                              0xFFFFFFFF
#define AQDE_HORI_FILTER_KERNEL_WriteMask                             0xFFFFFFFF
#define AQDE_HORI_FILTER_KERNEL_ResetValue                            0x00000000

#define AQDE_HORI_FILTER_KERNEL_COEFFICIENT0                              15 : 0
#define AQDE_HORI_FILTER_KERNEL_COEFFICIENT0_End                              15
#define AQDE_HORI_FILTER_KERNEL_COEFFICIENT0_Start                             0
#define AQDE_HORI_FILTER_KERNEL_COEFFICIENT0_Type                            U16

#define AQDE_HORI_FILTER_KERNEL_COEFFICIENT1                             31 : 16
#define AQDE_HORI_FILTER_KERNEL_COEFFICIENT1_End                              31
#define AQDE_HORI_FILTER_KERNEL_COEFFICIENT1_Start                            16
#define AQDE_HORI_FILTER_KERNEL_COEFFICIENT1_Type                            U16

/*******************************************************************************
** State AQDEVertiFilterKernel
*/

/* One Pass Filter blit vertical coefficient table. */

#define AQDEVertiFilterKernelRegAddrs                                     0x0A80
#define AQDE_VERTI_FILTER_KERNEL_MSB                                          15
#define AQDE_VERTI_FILTER_KERNEL_LSB                                           7
#define AQDE_VERTI_FILTER_KERNEL_BLK                                           7
#define AQDE_VERTI_FILTER_KERNEL_Count                                       128
#define AQDE_VERTI_FILTER_KERNEL_FieldMask                            0xFFFFFFFF
#define AQDE_VERTI_FILTER_KERNEL_ReadMask                             0xFFFFFFFF
#define AQDE_VERTI_FILTER_KERNEL_WriteMask                            0xFFFFFFFF
#define AQDE_VERTI_FILTER_KERNEL_ResetValue                           0x00000000

#define AQDE_VERTI_FILTER_KERNEL_COEFFICIENT0                             15 : 0
#define AQDE_VERTI_FILTER_KERNEL_COEFFICIENT0_End                             15
#define AQDE_VERTI_FILTER_KERNEL_COEFFICIENT0_Start                            0
#define AQDE_VERTI_FILTER_KERNEL_COEFFICIENT0_Type                           U16

#define AQDE_VERTI_FILTER_KERNEL_COEFFICIENT1                            31 : 16
#define AQDE_VERTI_FILTER_KERNEL_COEFFICIENT1_End                             31
#define AQDE_VERTI_FILTER_KERNEL_COEFFICIENT1_Start                           16
#define AQDE_VERTI_FILTER_KERNEL_COEFFICIENT1_Type                           U16

/*******************************************************************************
** State AQVRSourceImageLow
*/

/* Bounding box of the source image. */

#define AQVRSourceImageLowRegAddrs                                        0x04A6
#define AQVR_SOURCE_IMAGE_LOW_Address                                    0x01298
#define AQVR_SOURCE_IMAGE_LOW_MSB                                             15
#define AQVR_SOURCE_IMAGE_LOW_LSB                                              0
#define AQVR_SOURCE_IMAGE_LOW_BLK                                              0
#define AQVR_SOURCE_IMAGE_LOW_Count                                            1
#define AQVR_SOURCE_IMAGE_LOW_FieldMask                               0xFFFFFFFF
#define AQVR_SOURCE_IMAGE_LOW_ReadMask                                0xFFFFFFFF
#define AQVR_SOURCE_IMAGE_LOW_WriteMask                               0xFFFFFFFF
#define AQVR_SOURCE_IMAGE_LOW_ResetValue                              0x00000000

#define AQVR_SOURCE_IMAGE_LOW_LEFT                                        15 : 0
#define AQVR_SOURCE_IMAGE_LOW_LEFT_End                                        15
#define AQVR_SOURCE_IMAGE_LOW_LEFT_Start                                       0
#define AQVR_SOURCE_IMAGE_LOW_LEFT_Type                                      U16

#define AQVR_SOURCE_IMAGE_LOW_TOP                                        31 : 16
#define AQVR_SOURCE_IMAGE_LOW_TOP_End                                         31
#define AQVR_SOURCE_IMAGE_LOW_TOP_Start                                       16
#define AQVR_SOURCE_IMAGE_LOW_TOP_Type                                       U16

/*******************************************************************************
** State AQVRSourceImageHigh
*/

#define AQVRSourceImageHighRegAddrs                                       0x04A7
#define AQVR_SOURCE_IMAGE_HIGH_MSB                                            15
#define AQVR_SOURCE_IMAGE_HIGH_LSB                                             0
#define AQVR_SOURCE_IMAGE_LOW_HIGH_BLK                                         0
#define AQVR_SOURCE_IMAGE_HIGH_Count                                           1
#define AQVR_SOURCE_IMAGE_HIGH_FieldMask                              0xFFFFFFFF
#define AQVR_SOURCE_IMAGE_HIGH_ReadMask                               0xFFFFFFFF
#define AQVR_SOURCE_IMAGE_HIGH_WriteMask                              0xFFFFFFFF
#define AQVR_SOURCE_IMAGE_HIGH_ResetValue                             0x00000000

#define AQVR_SOURCE_IMAGE_HIGH_RIGHT                                      15 : 0
#define AQVR_SOURCE_IMAGE_HIGH_RIGHT_End                                      15
#define AQVR_SOURCE_IMAGE_HIGH_RIGHT_Start                                     0
#define AQVR_SOURCE_IMAGE_HIGH_RIGHT_Type                                    U16

#define AQVR_SOURCE_IMAGE_HIGH_BOTTOM                                    31 : 16
#define AQVR_SOURCE_IMAGE_HIGH_BOTTOM_End                                     31
#define AQVR_SOURCE_IMAGE_HIGH_BOTTOM_Start                                   16
#define AQVR_SOURCE_IMAGE_HIGH_BOTTOM_Type                                   U16

/*******************************************************************************
** State AQVRSourceOriginLow
*/

/* Fractional origin of the source window to be rendered within the source
** image.
*/

#define AQVRSourceOriginLowRegAddrs                                       0x04A8
#define AQVR_SOURCE_ORIGIN_LOW_MSB                                            15
#define AQVR_SOURCE_ORIGIN_LOW_LSB                                             0
#define AQVR_SOURCE_ORIGIN_LOW_BLK                                             0
#define AQVR_SOURCE_ORIGIN_LOW_Count                                           1
#define AQVR_SOURCE_ORIGIN_LOW_FieldMask                              0xFFFFFFFF
#define AQVR_SOURCE_ORIGIN_LOW_ReadMask                               0xFFFFFFFF
#define AQVR_SOURCE_ORIGIN_LOW_WriteMask                              0xFFFFFFFF
#define AQVR_SOURCE_ORIGIN_LOW_ResetValue                             0x00000000

#define AQVR_SOURCE_ORIGIN_LOW_X                                          31 : 0
#define AQVR_SOURCE_ORIGIN_LOW_X_End                                          31
#define AQVR_SOURCE_ORIGIN_LOW_X_Start                                         0
#define AQVR_SOURCE_ORIGIN_LOW_X_Type                                        U32

/*******************************************************************************
** State AQVRSourceOriginHigh
*/

#define AQVRSourceOriginHighRegAddrs                                      0x04A9
#define AQVR_SOURCE_ORIGIN_HIGH_MSB                                           15
#define AQVR_SOURCE_ORIGIN_HIGH_LSB                                            0
#define AQVR_SOURCE_ORIGIN_LOW_HIGH_BLK                                        0
#define AQVR_SOURCE_ORIGIN_HIGH_Count                                          1
#define AQVR_SOURCE_ORIGIN_HIGH_FieldMask                             0xFFFFFFFF
#define AQVR_SOURCE_ORIGIN_HIGH_ReadMask                              0xFFFFFFFF
#define AQVR_SOURCE_ORIGIN_HIGH_WriteMask                             0xFFFFFFFF
#define AQVR_SOURCE_ORIGIN_HIGH_ResetValue                            0x00000000

#define AQVR_SOURCE_ORIGIN_HIGH_Y                                         31 : 0
#define AQVR_SOURCE_ORIGIN_HIGH_Y_End                                         31
#define AQVR_SOURCE_ORIGIN_HIGH_Y_Start                                        0
#define AQVR_SOURCE_ORIGIN_HIGH_Y_Type                                       U32

/*******************************************************************************
** State AQVRTargetWindowLow
*/

/* Bounding box of the destination window to be rendered within the
** destination image.
*/

#define AQVRTargetWindowLowRegAddrs                                       0x04AA
#define AQVR_TARGET_WINDOW_LOW_Address                                   0x012A8
#define AQVR_TARGET_WINDOW_LOW_MSB                                            15
#define AQVR_TARGET_WINDOW_LOW_LSB                                             0
#define AQVR_TARGET_WINDOW_LOW_BLK                                             0
#define AQVR_TARGET_WINDOW_LOW_Count                                           1
#define AQVR_TARGET_WINDOW_LOW_FieldMask                              0xFFFFFFFF
#define AQVR_TARGET_WINDOW_LOW_ReadMask                               0xFFFFFFFF
#define AQVR_TARGET_WINDOW_LOW_WriteMask                              0xFFFFFFFF
#define AQVR_TARGET_WINDOW_LOW_ResetValue                             0x00000000

#define AQVR_TARGET_WINDOW_LOW_LEFT                                       15 : 0
#define AQVR_TARGET_WINDOW_LOW_LEFT_End                                       15
#define AQVR_TARGET_WINDOW_LOW_LEFT_Start                                      0
#define AQVR_TARGET_WINDOW_LOW_LEFT_Type                                     U16

#define AQVR_TARGET_WINDOW_LOW_TOP                                       31 : 16
#define AQVR_TARGET_WINDOW_LOW_TOP_End                                        31
#define AQVR_TARGET_WINDOW_LOW_TOP_Start                                      16
#define AQVR_TARGET_WINDOW_LOW_TOP_Type                                      U16

/*******************************************************************************
** State AQVRTargetWindowHigh
*/

#define AQVRTargetWindowHighRegAddrs                                      0x04AB
#define AQVR_TARGET_WINDOW_HIGH_MSB                                           15
#define AQVR_TARGET_WINDOW_HIGH_LSB                                            0
#define AQVR_TARGET_WINDOW_LOW_HIGH_BLK                                        0
#define AQVR_TARGET_WINDOW_HIGH_Count                                          1
#define AQVR_TARGET_WINDOW_HIGH_FieldMask                             0xFFFFFFFF
#define AQVR_TARGET_WINDOW_HIGH_ReadMask                              0xFFFFFFFF
#define AQVR_TARGET_WINDOW_HIGH_WriteMask                             0xFFFFFFFF
#define AQVR_TARGET_WINDOW_HIGH_ResetValue                            0x00000000

#define AQVR_TARGET_WINDOW_HIGH_RIGHT                                     15 : 0
#define AQVR_TARGET_WINDOW_HIGH_RIGHT_End                                     15
#define AQVR_TARGET_WINDOW_HIGH_RIGHT_Start                                    0
#define AQVR_TARGET_WINDOW_HIGH_RIGHT_Type                                   U16

#define AQVR_TARGET_WINDOW_HIGH_BOTTOM                                   31 : 16
#define AQVR_TARGET_WINDOW_HIGH_BOTTOM_End                                    31
#define AQVR_TARGET_WINDOW_HIGH_BOTTOM_Start                                  16
#define AQVR_TARGET_WINDOW_HIGH_BOTTOM_Type                                  U16

/*******************************************************************************
** State AQVRConfig
*/

/* Video Rasterizer kick-off register. */

#define AQVRConfigRegAddrs                                                0x04A5
#define AQVR_CONFIG_MSB                                                       15
#define AQVR_CONFIG_LSB                                                        0
#define AQVR_CONFIG_BLK                                                        0
#define AQVR_CONFIG_Count                                                      1
#define AQVR_CONFIG_FieldMask                                         0x0000000B
#define AQVR_CONFIG_ReadMask                                          0x0000000B
#define AQVR_CONFIG_WriteMask                                         0x0000000B
#define AQVR_CONFIG_ResetValue                                        0x00000000

/* Kick-off command. */
#define AQVR_CONFIG_START                                                  1 : 0
#define AQVR_CONFIG_START_End                                                  1
#define AQVR_CONFIG_START_Start                                                0
#define AQVR_CONFIG_START_Type                                               U02
#define   AQVR_CONFIG_START_HORIZONTAL_BLIT                                  0x0
#define   AQVR_CONFIG_START_VERTICAL_BLIT                                    0x1
#define   AQVR_CONFIG_START_ONE_PASS_BLIT                                    0x2

#define AQVR_CONFIG_MASK_START                                             3 : 3
#define AQVR_CONFIG_MASK_START_End                                             3
#define AQVR_CONFIG_MASK_START_Start                                           3
#define AQVR_CONFIG_MASK_START_Type                                          U01
#define   AQVR_CONFIG_MASK_START_ENABLED                                     0x0
#define   AQVR_CONFIG_MASK_START_MASKED                                      0x1

/*******************************************************************************
** State AQVRConfigEx
*/

/* Video Rasterizer configuration register. */

#define AQVRConfigExRegAddrs                                              0x04B9
#define AQVR_CONFIG_EX_Address                                           0x012E4
#define AQVR_CONFIG_EX_MSB                                                    15
#define AQVR_CONFIG_EX_LSB                                                     0
#define AQVR_CONFIG_EX_BLK                                                     0
#define AQVR_CONFIG_EX_Count                                                   1
#define AQVR_CONFIG_EX_FieldMask                                      0x000001FB
#define AQVR_CONFIG_EX_ReadMask                                       0x000001FB
#define AQVR_CONFIG_EX_WriteMask                                      0x000001FB
#define AQVR_CONFIG_EX_ResetValue                                     0x00000000

/* Line width in pixels for vertical pass. */
#define AQVR_CONFIG_EX_VERTICAL_LINE_WIDTH                                 1 : 0
#define AQVR_CONFIG_EX_VERTICAL_LINE_WIDTH_End                                 1
#define AQVR_CONFIG_EX_VERTICAL_LINE_WIDTH_Start                               0
#define AQVR_CONFIG_EX_VERTICAL_LINE_WIDTH_Type                              U02
#define   AQVR_CONFIG_EX_VERTICAL_LINE_WIDTH_AUTO                            0x0
#define   AQVR_CONFIG_EX_VERTICAL_LINE_WIDTH_PIXELS16                        0x1
#define   AQVR_CONFIG_EX_VERTICAL_LINE_WIDTH_PIXELS32                        0x2

#define AQVR_CONFIG_EX_MASK_VERTICAL_LINE_WIDTH                            3 : 3
#define AQVR_CONFIG_EX_MASK_VERTICAL_LINE_WIDTH_End                            3
#define AQVR_CONFIG_EX_MASK_VERTICAL_LINE_WIDTH_Start                          3
#define AQVR_CONFIG_EX_MASK_VERTICAL_LINE_WIDTH_Type                         U01
#define   AQVR_CONFIG_EX_MASK_VERTICAL_LINE_WIDTH_ENABLED                    0x0
#define   AQVR_CONFIG_EX_MASK_VERTICAL_LINE_WIDTH_MASKED                     0x1

/* one pass filter tap. */
#define AQVR_CONFIG_EX_FILTER_TAP                                          7 : 4
#define AQVR_CONFIG_EX_FILTER_TAP_End                                          7
#define AQVR_CONFIG_EX_FILTER_TAP_Start                                        4
#define AQVR_CONFIG_EX_FILTER_TAP_Type                                       U04

#define AQVR_CONFIG_EX_MASK_FILTER_TAP                                     8 : 8
#define AQVR_CONFIG_EX_MASK_FILTER_TAP_End                                     8
#define AQVR_CONFIG_EX_MASK_FILTER_TAP_Start                                   8
#define AQVR_CONFIG_EX_MASK_FILTER_TAP_Type                                  U01
#define   AQVR_CONFIG_EX_MASK_FILTER_TAP_ENABLED                             0x0
#define   AQVR_CONFIG_EX_MASK_FILTER_TAP_MASKED                              0x1

/*******************************************************************************
** State AQBWConfig
*/

#define AQBWConfigRegAddrs                                                0x04BC
#define AQBW_CONFIG_MSB                                                       15
#define AQBW_CONFIG_LSB                                                        0
#define AQBW_CONFIG_BLK                                                        0
#define AQBW_CONFIG_Count                                                      1
#define AQBW_CONFIG_FieldMask                                         0x00009999
#define AQBW_CONFIG_ReadMask                                          0x00009999
#define AQBW_CONFIG_WriteMask                                         0x00009999
#define AQBW_CONFIG_ResetValue                                        0x00000000

/* One Pass Filter Block Config. */
#define AQBW_CONFIG_BLOCK_CONFIG                                           0 : 0
#define AQBW_CONFIG_BLOCK_CONFIG_End                                           0
#define AQBW_CONFIG_BLOCK_CONFIG_Start                                         0
#define AQBW_CONFIG_BLOCK_CONFIG_Type                                        U01
#define   AQBW_CONFIG_BLOCK_CONFIG_AUTO                                      0x0
#define   AQBW_CONFIG_BLOCK_CONFIG_CUSTOMIZE                                 0x1

#define AQBW_CONFIG_MASK_BLOCK_CONFIG                                      3 : 3
#define AQBW_CONFIG_MASK_BLOCK_CONFIG_End                                      3
#define AQBW_CONFIG_MASK_BLOCK_CONFIG_Start                                    3
#define AQBW_CONFIG_MASK_BLOCK_CONFIG_Type                                   U01
#define   AQBW_CONFIG_MASK_BLOCK_CONFIG_ENABLED                              0x0
#define   AQBW_CONFIG_MASK_BLOCK_CONFIG_MASKED                               0x1

/* block walk direction in one pass filter blit. */
#define AQBW_CONFIG_BLOCK_WALK_DIRECTION                                   4 : 4
#define AQBW_CONFIG_BLOCK_WALK_DIRECTION_End                                   4
#define AQBW_CONFIG_BLOCK_WALK_DIRECTION_Start                                 4
#define AQBW_CONFIG_BLOCK_WALK_DIRECTION_Type                                U01
#define   AQBW_CONFIG_BLOCK_WALK_DIRECTION_RIGHT_BOTTOM                      0x0
#define   AQBW_CONFIG_BLOCK_WALK_DIRECTION_BOTTOM_RIGHT                      0x1

#define AQBW_CONFIG_MASK_BLOCK_WALK_DIRECTION                              7 : 7
#define AQBW_CONFIG_MASK_BLOCK_WALK_DIRECTION_End                              7
#define AQBW_CONFIG_MASK_BLOCK_WALK_DIRECTION_Start                            7
#define AQBW_CONFIG_MASK_BLOCK_WALK_DIRECTION_Type                           U01
#define   AQBW_CONFIG_MASK_BLOCK_WALK_DIRECTION_ENABLED                      0x0
#define   AQBW_CONFIG_MASK_BLOCK_WALK_DIRECTION_MASKED                       0x1

/* block walk direction in one pass filter blit. */
#define AQBW_CONFIG_TILE_WALK_DIRECTION                                    8 : 8
#define AQBW_CONFIG_TILE_WALK_DIRECTION_End                                    8
#define AQBW_CONFIG_TILE_WALK_DIRECTION_Start                                  8
#define AQBW_CONFIG_TILE_WALK_DIRECTION_Type                                 U01
#define   AQBW_CONFIG_TILE_WALK_DIRECTION_RIGHT_BOTTOM                       0x0
#define   AQBW_CONFIG_TILE_WALK_DIRECTION_BOTTOM_RIGHT                       0x1

#define AQBW_CONFIG_MASK_TILE_WALK_DIRECTION                             11 : 11
#define AQBW_CONFIG_MASK_TILE_WALK_DIRECTION_End                              11
#define AQBW_CONFIG_MASK_TILE_WALK_DIRECTION_Start                            11
#define AQBW_CONFIG_MASK_TILE_WALK_DIRECTION_Type                            U01
#define   AQBW_CONFIG_MASK_TILE_WALK_DIRECTION_ENABLED                       0x0
#define   AQBW_CONFIG_MASK_TILE_WALK_DIRECTION_MASKED                        0x1

/* block walk direction in one pass filter blit. */
#define AQBW_CONFIG_PIXEL_WALK_DIRECTION                                 12 : 12
#define AQBW_CONFIG_PIXEL_WALK_DIRECTION_End                                  12
#define AQBW_CONFIG_PIXEL_WALK_DIRECTION_Start                                12
#define AQBW_CONFIG_PIXEL_WALK_DIRECTION_Type                                U01
#define   AQBW_CONFIG_PIXEL_WALK_DIRECTION_RIGHT_BOTTOM                      0x0
#define   AQBW_CONFIG_PIXEL_WALK_DIRECTION_BOTTOM_RIGHT                      0x1

#define AQBW_CONFIG_MASK_PIXEL_WALK_DIRECTION                            15 : 15
#define AQBW_CONFIG_MASK_PIXEL_WALK_DIRECTION_End                             15
#define AQBW_CONFIG_MASK_PIXEL_WALK_DIRECTION_Start                           15
#define AQBW_CONFIG_MASK_PIXEL_WALK_DIRECTION_Type                           U01
#define   AQBW_CONFIG_MASK_PIXEL_WALK_DIRECTION_ENABLED                      0x0
#define   AQBW_CONFIG_MASK_PIXEL_WALK_DIRECTION_MASKED                       0x1

/*******************************************************************************
** State AQBWBlockSize
*/

/* Walker Block size. */

#define AQBWBlockSizeRegAddrs                                             0x04BD
#define AQBW_BLOCK_SIZE_MSB                                                   15
#define AQBW_BLOCK_SIZE_LSB                                                    0
#define AQBW_BLOCK_SIZE_BLK                                                    0
#define AQBW_BLOCK_SIZE_Count                                                  1
#define AQBW_BLOCK_SIZE_FieldMask                                     0xFFFFFFFF
#define AQBW_BLOCK_SIZE_ReadMask                                      0xFFFFFFFF
#define AQBW_BLOCK_SIZE_WriteMask                                     0xFFFFFFFF
#define AQBW_BLOCK_SIZE_ResetValue                                    0x00000000

#define AQBW_BLOCK_SIZE_WIDTH                                             15 : 0
#define AQBW_BLOCK_SIZE_WIDTH_End                                             15
#define AQBW_BLOCK_SIZE_WIDTH_Start                                            0
#define AQBW_BLOCK_SIZE_WIDTH_Type                                           U16

#define AQBW_BLOCK_SIZE_HEIGHT                                           31 : 16
#define AQBW_BLOCK_SIZE_HEIGHT_End                                            31
#define AQBW_BLOCK_SIZE_HEIGHT_Start                                          16
#define AQBW_BLOCK_SIZE_HEIGHT_Type                                          U16

/*******************************************************************************
** State AQBWTileSize
*/

/* Walker tile size. */

#define AQBWTileSizeRegAddrs                                              0x04BE
#define AQBW_TILE_SIZE_MSB                                                    15
#define AQBW_TILE_SIZE_LSB                                                     0
#define AQBW_TILE_SIZE_BLK                                                     0
#define AQBW_TILE_SIZE_Count                                                   1
#define AQBW_TILE_SIZE_FieldMask                                      0xFFFFFFFF
#define AQBW_TILE_SIZE_ReadMask                                       0xFFFFFFFF
#define AQBW_TILE_SIZE_WriteMask                                      0xFFFFFFFF
#define AQBW_TILE_SIZE_ResetValue                                     0x00000000

#define AQBW_TILE_SIZE_WIDTH                                              15 : 0
#define AQBW_TILE_SIZE_WIDTH_End                                              15
#define AQBW_TILE_SIZE_WIDTH_Start                                             0
#define AQBW_TILE_SIZE_WIDTH_Type                                            U16

#define AQBW_TILE_SIZE_HEIGHT                                            31 : 16
#define AQBW_TILE_SIZE_HEIGHT_End                                             31
#define AQBW_TILE_SIZE_HEIGHT_Start                                           16
#define AQBW_TILE_SIZE_HEIGHT_Type                                           U16

/*******************************************************************************
** State AQBWBlockMask
*/

/* Walker Block Mask. */

#define AQBWBlockMaskRegAddrs                                             0x04BF
#define AQBW_BLOCK_MASK_MSB                                                   15
#define AQBW_BLOCK_MASK_LSB                                                    0
#define AQBW_BLOCK_MASK_BLK                                                    0
#define AQBW_BLOCK_MASK_Count                                                  1
#define AQBW_BLOCK_MASK_FieldMask                                     0xFFFFFFFF
#define AQBW_BLOCK_MASK_ReadMask                                      0xFFFFFFFF
#define AQBW_BLOCK_MASK_WriteMask                                     0xFFFFFFFF
#define AQBW_BLOCK_MASK_ResetValue                                    0x00000000

#define AQBW_BLOCK_MASK_HORIZONTAL                                        15 : 0
#define AQBW_BLOCK_MASK_HORIZONTAL_End                                        15
#define AQBW_BLOCK_MASK_HORIZONTAL_Start                                       0
#define AQBW_BLOCK_MASK_HORIZONTAL_Type                                      U16

#define AQBW_BLOCK_MASK_VERTICAL                                         31 : 16
#define AQBW_BLOCK_MASK_VERTICAL_End                                          31
#define AQBW_BLOCK_MASK_VERTICAL_Start                                        16
#define AQBW_BLOCK_MASK_VERTICAL_Type                                        U16

/*******************************************************************************
** State AQDEIndexColorTable
*/

/* 256 color entries for the indexed color mode. Colors are assumed to be in
** the destination format and no color conversion is done on the values.
*/

#define AQDEIndexColorTableRegAddrs                                       0x0700
#define AQDE_INDEX_COLOR_TABLE_MSB                                            15
#define AQDE_INDEX_COLOR_TABLE_LSB                                             8
#define AQDE_INDEX_COLOR_TABLE_BLK                                             8
#define AQDE_INDEX_COLOR_TABLE_Count                                         256
#define AQDE_INDEX_COLOR_TABLE_FieldMask                              0xFFFFFFFF
#define AQDE_INDEX_COLOR_TABLE_ReadMask                               0xFFFFFFFF
#define AQDE_INDEX_COLOR_TABLE_WriteMask                              0xFFFFFFFF
#define AQDE_INDEX_COLOR_TABLE_ResetValue                             0x00000000

#define AQDE_INDEX_COLOR_TABLE_ALPHA                                     31 : 24
#define AQDE_INDEX_COLOR_TABLE_ALPHA_End                                      31
#define AQDE_INDEX_COLOR_TABLE_ALPHA_Start                                    24
#define AQDE_INDEX_COLOR_TABLE_ALPHA_Type                                    U08

#define AQDE_INDEX_COLOR_TABLE_RED                                       23 : 16
#define AQDE_INDEX_COLOR_TABLE_RED_End                                        23
#define AQDE_INDEX_COLOR_TABLE_RED_Start                                      16
#define AQDE_INDEX_COLOR_TABLE_RED_Type                                      U08

#define AQDE_INDEX_COLOR_TABLE_GREEN                                      15 : 8
#define AQDE_INDEX_COLOR_TABLE_GREEN_End                                      15
#define AQDE_INDEX_COLOR_TABLE_GREEN_Start                                     8
#define AQDE_INDEX_COLOR_TABLE_GREEN_Type                                    U08

#define AQDE_INDEX_COLOR_TABLE_BLUE                                        7 : 0
#define AQDE_INDEX_COLOR_TABLE_BLUE_End                                        7
#define AQDE_INDEX_COLOR_TABLE_BLUE_Start                                      0
#define AQDE_INDEX_COLOR_TABLE_BLUE_Type                                     U08

/*******************************************************************************
** State AQDEIndexColorTable32
*/

/* 256 color entries for the indexed color mode. Colors are assumed to be in
** the A8R8G8B8 format and no color conversion is done on the values. This
** register is used only with chips with PE20 feature available.
*/

#define AQDEIndexColorTable32RegAddrs                                     0x0D00
#define AQDE_INDEX_COLOR_TABLE32_MSB                                          15
#define AQDE_INDEX_COLOR_TABLE32_LSB                                           8
#define AQDE_INDEX_COLOR_TABLE32_BLK                                           8
#define AQDE_INDEX_COLOR_TABLE32_Count                                       256
#define AQDE_INDEX_COLOR_TABLE32_FieldMask                            0xFFFFFFFF
#define AQDE_INDEX_COLOR_TABLE32_ReadMask                             0xFFFFFFFF
#define AQDE_INDEX_COLOR_TABLE32_WriteMask                            0xFFFFFFFF
#define AQDE_INDEX_COLOR_TABLE32_ResetValue                           0x00000000

#define AQDE_INDEX_COLOR_TABLE32_ALPHA                                   31 : 24
#define AQDE_INDEX_COLOR_TABLE32_ALPHA_End                                    31
#define AQDE_INDEX_COLOR_TABLE32_ALPHA_Start                                  24
#define AQDE_INDEX_COLOR_TABLE32_ALPHA_Type                                  U08

#define AQDE_INDEX_COLOR_TABLE32_RED                                     23 : 16
#define AQDE_INDEX_COLOR_TABLE32_RED_End                                      23
#define AQDE_INDEX_COLOR_TABLE32_RED_Start                                    16
#define AQDE_INDEX_COLOR_TABLE32_RED_Type                                    U08

#define AQDE_INDEX_COLOR_TABLE32_GREEN                                    15 : 8
#define AQDE_INDEX_COLOR_TABLE32_GREEN_End                                    15
#define AQDE_INDEX_COLOR_TABLE32_GREEN_Start                                   8
#define AQDE_INDEX_COLOR_TABLE32_GREEN_Type                                  U08

#define AQDE_INDEX_COLOR_TABLE32_BLUE                                      7 : 0
#define AQDE_INDEX_COLOR_TABLE32_BLUE_End                                      7
#define AQDE_INDEX_COLOR_TABLE32_BLUE_Start                                    0
#define AQDE_INDEX_COLOR_TABLE32_BLUE_Type                                   U08

/*******************************************************************************
** State AQDERop
*/

/* Raster operation foreground and background codes. Even though ROP is not
** used in CLEAR, HOR_FILTER_BLT, VER_FILTER_BLT and alpha-eanbled BIT_BLTs,
** ROP code still has to be programmed, because the engine makes the decision
** whether source, destination and pattern are involved in the current
** operation and the correct decision is essential for the engine to complete
** the operation as expected.
*/

#define AQDERopRegAddrs                                                   0x0497
#define AQDE_ROP_MSB                                                          15
#define AQDE_ROP_LSB                                                           0
#define AQDE_ROP_BLK                                                           0
#define AQDE_ROP_Count                                                         1
#define AQDE_ROP_FieldMask                                            0x0030FFFF
#define AQDE_ROP_ReadMask                                             0x0030FFFF
#define AQDE_ROP_WriteMask                                            0x0030FFFF
#define AQDE_ROP_ResetValue                                           0x00000000

/* ROP type: ROP2, ROP3 or ROP4 */
#define AQDE_ROP_TYPE                                                    21 : 20
#define AQDE_ROP_TYPE_End                                                     21
#define AQDE_ROP_TYPE_Start                                                   20
#define AQDE_ROP_TYPE_Type                                                   U02
#define   AQDE_ROP_TYPE_ROP2_PATTERN                                         0x0
#define   AQDE_ROP_TYPE_ROP2_SOURCE                                          0x1
#define   AQDE_ROP_TYPE_ROP3                                                 0x2
#define   AQDE_ROP_TYPE_ROP4                                                 0x3

/* Background ROP code is used for transparent pixels. */
#define AQDE_ROP_ROP_BG                                                   15 : 8
#define AQDE_ROP_ROP_BG_End                                                   15
#define AQDE_ROP_ROP_BG_Start                                                  8
#define AQDE_ROP_ROP_BG_Type                                                 U08

/* Background ROP code is used for opaque pixels. */
#define AQDE_ROP_ROP_FG                                                    7 : 0
#define AQDE_ROP_ROP_FG_End                                                    7
#define AQDE_ROP_ROP_FG_Start                                                  0
#define AQDE_ROP_ROP_FG_Type                                                 U08

/*******************************************************************************
** State AQDEClipTopLeft
*/

/* Top left corner of the clipping rectangle defined in pixels. Clipping is
** always on and everything beyond the clipping rectangle will be clipped
** out. Clipping is not used with filter blits.
*/

#define AQDEClipTopLeftRegAddrs                                           0x0498
#define AQDE_CLIP_TOP_LEFT_MSB                                                15
#define AQDE_CLIP_TOP_LEFT_LSB                                                 0
#define AQDE_CLIP_TOP_LEFT_BLK                                                 0
#define AQDE_CLIP_TOP_LEFT_Count                                               1
#define AQDE_CLIP_TOP_LEFT_FieldMask                                  0x7FFF7FFF
#define AQDE_CLIP_TOP_LEFT_ReadMask                                   0x7FFF7FFF
#define AQDE_CLIP_TOP_LEFT_WriteMask                                  0x7FFF7FFF
#define AQDE_CLIP_TOP_LEFT_ResetValue                                 0x00000000

#define AQDE_CLIP_TOP_LEFT_Y                                             30 : 16
#define AQDE_CLIP_TOP_LEFT_Y_End                                              30
#define AQDE_CLIP_TOP_LEFT_Y_Start                                            16
#define AQDE_CLIP_TOP_LEFT_Y_Type                                            U15

#define AQDE_CLIP_TOP_LEFT_X                                              14 : 0
#define AQDE_CLIP_TOP_LEFT_X_End                                              14
#define AQDE_CLIP_TOP_LEFT_X_Start                                             0
#define AQDE_CLIP_TOP_LEFT_X_Type                                            U15

/*******************************************************************************
** State AQDEClipBottomRight
*/

/* Bottom right corner of the clipping rectangle defined in pixels. Clipping
** is always on and everything beyond the clipping rectangle will be clipped
** out. Clipping is not used with filter blits.
*/

#define AQDEClipBottomRightRegAddrs                                       0x0499
#define AQDE_CLIP_BOTTOM_RIGHT_MSB                                            15
#define AQDE_CLIP_BOTTOM_RIGHT_LSB                                             0
#define AQDE_CLIP_BOTTOM_RIGHT_BLK                                             0
#define AQDE_CLIP_BOTTOM_RIGHT_Count                                           1
#define AQDE_CLIP_BOTTOM_RIGHT_FieldMask                              0x7FFF7FFF
#define AQDE_CLIP_BOTTOM_RIGHT_ReadMask                               0x7FFF7FFF
#define AQDE_CLIP_BOTTOM_RIGHT_WriteMask                              0x7FFF7FFF
#define AQDE_CLIP_BOTTOM_RIGHT_ResetValue                             0x00000000

#define AQDE_CLIP_BOTTOM_RIGHT_Y                                         30 : 16
#define AQDE_CLIP_BOTTOM_RIGHT_Y_End                                          30
#define AQDE_CLIP_BOTTOM_RIGHT_Y_Start                                        16
#define AQDE_CLIP_BOTTOM_RIGHT_Y_Type                                        U15

#define AQDE_CLIP_BOTTOM_RIGHT_X                                          14 : 0
#define AQDE_CLIP_BOTTOM_RIGHT_X_End                                          14
#define AQDE_CLIP_BOTTOM_RIGHT_X_Start                                         0
#define AQDE_CLIP_BOTTOM_RIGHT_X_Type                                        U15

/*******************************************************************************
** State AQDEConfig
*/

#define AQDEConfigRegAddrs                                                0x049B
#define AQDE_CONFIG_MSB                                                       15
#define AQDE_CONFIG_LSB                                                        0
#define AQDE_CONFIG_BLK                                                        0
#define AQDE_CONFIG_Count                                                      1
#define AQDE_CONFIG_FieldMask                                         0x00370031
#define AQDE_CONFIG_ReadMask                                          0x00370031
#define AQDE_CONFIG_WriteMask                                         0x00370031
#define AQDE_CONFIG_ResetValue                                        0x00000000

#define AQDE_CONFIG_MIRROR_BLT_MODE                                        5 : 4
#define AQDE_CONFIG_MIRROR_BLT_MODE_End                                        5
#define AQDE_CONFIG_MIRROR_BLT_MODE_Start                                      4
#define AQDE_CONFIG_MIRROR_BLT_MODE_Type                                     U02
#define   AQDE_CONFIG_MIRROR_BLT_MODE_NORMAL                                 0x0
#define   AQDE_CONFIG_MIRROR_BLT_MODE_HMIRROR                                0x1
#define   AQDE_CONFIG_MIRROR_BLT_MODE_VMIRROR                                0x2
#define   AQDE_CONFIG_MIRROR_BLT_MODE_FULL_MIRROR                            0x3

#define AQDE_CONFIG_MIRROR_BLT_ENABLE                                      0 : 0
#define AQDE_CONFIG_MIRROR_BLT_ENABLE_End                                      0
#define AQDE_CONFIG_MIRROR_BLT_ENABLE_Start                                    0
#define AQDE_CONFIG_MIRROR_BLT_ENABLE_Type                                   U01
#define   AQDE_CONFIG_MIRROR_BLT_ENABLE_OFF                                  0x0
#define   AQDE_CONFIG_MIRROR_BLT_ENABLE_ON                                   0x1

/* Source select for the old walkers. */
#define AQDE_CONFIG_SOURCE_SELECT                                        18 : 16
#define AQDE_CONFIG_SOURCE_SELECT_End                                         18
#define AQDE_CONFIG_SOURCE_SELECT_Start                                       16
#define AQDE_CONFIG_SOURCE_SELECT_Type                                       U03

/* Destination select for the old walkers. */
#define AQDE_CONFIG_DESTINATION_SELECT                                   21 : 20
#define AQDE_CONFIG_DESTINATION_SELECT_End                                    21
#define AQDE_CONFIG_DESTINATION_SELECT_Start                                  20
#define AQDE_CONFIG_DESTINATION_SELECT_Type                                  U02

/*******************************************************************************
** State AQDESrcOriginFraction
*/

/* Fraction for the source origin. Together with values in AQDESrcOrigin
** these values form signed 16.16 fixed point origin for the source
** rectangle. Fractions are only used in filter blit in split frame mode.
*/

#define AQDESrcOriginFractionRegAddrs                                     0x049E
#define AQDE_SRC_ORIGIN_FRACTION_MSB                                          15
#define AQDE_SRC_ORIGIN_FRACTION_LSB                                           0
#define AQDE_SRC_ORIGIN_FRACTION_BLK                                           0
#define AQDE_SRC_ORIGIN_FRACTION_Count                                         1
#define AQDE_SRC_ORIGIN_FRACTION_FieldMask                            0xFFFFFFFF
#define AQDE_SRC_ORIGIN_FRACTION_ReadMask                             0xFFFFFFFF
#define AQDE_SRC_ORIGIN_FRACTION_WriteMask                            0xFFFFFFFF
#define AQDE_SRC_ORIGIN_FRACTION_ResetValue                           0x00000000

#define AQDE_SRC_ORIGIN_FRACTION_Y                                       31 : 16
#define AQDE_SRC_ORIGIN_FRACTION_Y_End                                        31
#define AQDE_SRC_ORIGIN_FRACTION_Y_Start                                      16
#define AQDE_SRC_ORIGIN_FRACTION_Y_Type                                      U16

#define AQDE_SRC_ORIGIN_FRACTION_X                                        15 : 0
#define AQDE_SRC_ORIGIN_FRACTION_X_End                                        15
#define AQDE_SRC_ORIGIN_FRACTION_X_Start                                       0
#define AQDE_SRC_ORIGIN_FRACTION_X_Type                                      U16

/*******************************************************************************
** State AQDEAlphaControl
*/

#define AQDEAlphaControlRegAddrs                                          0x049F
#define AQDE_ALPHA_CONTROL_MSB                                                15
#define AQDE_ALPHA_CONTROL_LSB                                                 0
#define AQDE_ALPHA_CONTROL_BLK                                                 0
#define AQDE_ALPHA_CONTROL_Count                                               1
#define AQDE_ALPHA_CONTROL_FieldMask                                  0xFFFF0001
#define AQDE_ALPHA_CONTROL_ReadMask                                   0xFFFF0001
#define AQDE_ALPHA_CONTROL_WriteMask                                  0xFFFF0001
#define AQDE_ALPHA_CONTROL_ResetValue                                 0x00000000

#define AQDE_ALPHA_CONTROL_ENABLE                                          0 : 0
#define AQDE_ALPHA_CONTROL_ENABLE_End                                          0
#define AQDE_ALPHA_CONTROL_ENABLE_Start                                        0
#define AQDE_ALPHA_CONTROL_ENABLE_Type                                       U01
#define   AQDE_ALPHA_CONTROL_ENABLE_OFF                                      0x0
#define   AQDE_ALPHA_CONTROL_ENABLE_ON                                       0x1

/*******************************************************************************
** State AQDEAlphaModes
*/

#define AQDEAlphaModesRegAddrs                                            0x04A0
#define AQDE_ALPHA_MODES_MSB                                                  15
#define AQDE_ALPHA_MODES_LSB                                                   0
#define AQDE_ALPHA_MODES_BLK                                                   0
#define AQDE_ALPHA_MODES_Count                                                 1
#define AQDE_ALPHA_MODES_FieldMask                                    0xFF113311
#define AQDE_ALPHA_MODES_ReadMask                                     0xFF113311
#define AQDE_ALPHA_MODES_WriteMask                                    0xFF113311
#define AQDE_ALPHA_MODES_ResetValue                                   0x00000000

#define AQDE_ALPHA_MODES_SRC_ALPHA_MODE                                    0 : 0
#define AQDE_ALPHA_MODES_SRC_ALPHA_MODE_End                                    0
#define AQDE_ALPHA_MODES_SRC_ALPHA_MODE_Start                                  0
#define AQDE_ALPHA_MODES_SRC_ALPHA_MODE_Type                                 U01
#define   AQDE_ALPHA_MODES_SRC_ALPHA_MODE_NORMAL                             0x0
#define   AQDE_ALPHA_MODES_SRC_ALPHA_MODE_INVERSED                           0x1

#define AQDE_ALPHA_MODES_DST_ALPHA_MODE                                    4 : 4
#define AQDE_ALPHA_MODES_DST_ALPHA_MODE_End                                    4
#define AQDE_ALPHA_MODES_DST_ALPHA_MODE_Start                                  4
#define AQDE_ALPHA_MODES_DST_ALPHA_MODE_Type                                 U01
#define   AQDE_ALPHA_MODES_DST_ALPHA_MODE_NORMAL                             0x0
#define   AQDE_ALPHA_MODES_DST_ALPHA_MODE_INVERSED                           0x1

#define AQDE_ALPHA_MODES_GLOBAL_SRC_ALPHA_MODE                             9 : 8
#define AQDE_ALPHA_MODES_GLOBAL_SRC_ALPHA_MODE_End                             9
#define AQDE_ALPHA_MODES_GLOBAL_SRC_ALPHA_MODE_Start                           8
#define AQDE_ALPHA_MODES_GLOBAL_SRC_ALPHA_MODE_Type                          U02
#define   AQDE_ALPHA_MODES_GLOBAL_SRC_ALPHA_MODE_NORMAL                      0x0
#define   AQDE_ALPHA_MODES_GLOBAL_SRC_ALPHA_MODE_GLOBAL                      0x1
#define   AQDE_ALPHA_MODES_GLOBAL_SRC_ALPHA_MODE_SCALED                      0x2

#define AQDE_ALPHA_MODES_GLOBAL_DST_ALPHA_MODE                           13 : 12
#define AQDE_ALPHA_MODES_GLOBAL_DST_ALPHA_MODE_End                            13
#define AQDE_ALPHA_MODES_GLOBAL_DST_ALPHA_MODE_Start                          12
#define AQDE_ALPHA_MODES_GLOBAL_DST_ALPHA_MODE_Type                          U02
#define   AQDE_ALPHA_MODES_GLOBAL_DST_ALPHA_MODE_NORMAL                      0x0
#define   AQDE_ALPHA_MODES_GLOBAL_DST_ALPHA_MODE_GLOBAL                      0x1
#define   AQDE_ALPHA_MODES_GLOBAL_DST_ALPHA_MODE_SCALED                      0x2

#define AQDE_ALPHA_MODES_SRC_BLENDING_MODE                               26 : 24
#define AQDE_ALPHA_MODES_SRC_BLENDING_MODE_End                                26
#define AQDE_ALPHA_MODES_SRC_BLENDING_MODE_Start                              24
#define AQDE_ALPHA_MODES_SRC_BLENDING_MODE_Type                              U03
#define   AQDE_ALPHA_MODES_SRC_BLENDING_MODE_ZERO                            0x0
#define   AQDE_ALPHA_MODES_SRC_BLENDING_MODE_ONE                             0x1
#define   AQDE_ALPHA_MODES_SRC_BLENDING_MODE_NORMAL                          0x2
#define   AQDE_ALPHA_MODES_SRC_BLENDING_MODE_INVERSED                        0x3
#define   AQDE_ALPHA_MODES_SRC_BLENDING_MODE_COLOR                           0x4
#define   AQDE_ALPHA_MODES_SRC_BLENDING_MODE_COLOR_INVERSED                  0x5
#define   AQDE_ALPHA_MODES_SRC_BLENDING_MODE_SATURATED_ALPHA                 0x6
#define   AQDE_ALPHA_MODES_SRC_BLENDING_MODE_SATURATED_DEST_ALPHA            0x7

/* Src Blending factor is calculate from Src alpha. */
#define AQDE_ALPHA_MODES_SRC_ALPHA_FACTOR                                27 : 27
#define AQDE_ALPHA_MODES_SRC_ALPHA_FACTOR_End                                 27
#define AQDE_ALPHA_MODES_SRC_ALPHA_FACTOR_Start                               27
#define AQDE_ALPHA_MODES_SRC_ALPHA_FACTOR_Type                               U01
#define   AQDE_ALPHA_MODES_SRC_ALPHA_FACTOR_DISABLED                         0x0
#define   AQDE_ALPHA_MODES_SRC_ALPHA_FACTOR_ENABLED                          0x1

#define AQDE_ALPHA_MODES_DST_BLENDING_MODE                               30 : 28
#define AQDE_ALPHA_MODES_DST_BLENDING_MODE_End                                30
#define AQDE_ALPHA_MODES_DST_BLENDING_MODE_Start                              28
#define AQDE_ALPHA_MODES_DST_BLENDING_MODE_Type                              U03
#define   AQDE_ALPHA_MODES_DST_BLENDING_MODE_ZERO                            0x0
#define   AQDE_ALPHA_MODES_DST_BLENDING_MODE_ONE                             0x1
#define   AQDE_ALPHA_MODES_DST_BLENDING_MODE_NORMAL                          0x2
#define   AQDE_ALPHA_MODES_DST_BLENDING_MODE_INVERSED                        0x3
#define   AQDE_ALPHA_MODES_DST_BLENDING_MODE_COLOR                           0x4
#define   AQDE_ALPHA_MODES_DST_BLENDING_MODE_COLOR_INVERSED                  0x5
#define   AQDE_ALPHA_MODES_DST_BLENDING_MODE_SATURATED_ALPHA                 0x6
#define   AQDE_ALPHA_MODES_DST_BLENDING_MODE_SATURATED_DEST_ALPHA            0x7

/* Dst Blending factor is calculate from Dst alpha. */
#define AQDE_ALPHA_MODES_DST_ALPHA_FACTOR                                31 : 31
#define AQDE_ALPHA_MODES_DST_ALPHA_FACTOR_End                                 31
#define AQDE_ALPHA_MODES_DST_ALPHA_FACTOR_Start                               31
#define AQDE_ALPHA_MODES_DST_ALPHA_FACTOR_Type                               U01
#define   AQDE_ALPHA_MODES_DST_ALPHA_FACTOR_DISABLED                         0x0
#define   AQDE_ALPHA_MODES_DST_ALPHA_FACTOR_ENABLED                          0x1

/*******************************************************************************
** State UPlaneAddress
*/

/* 32-bit aligned base address of the source U plane. */

#define UPlaneAddressRegAddrs                                             0x04A1
#define UPLANE_ADDRESS_MSB                                                    15
#define UPLANE_ADDRESS_LSB                                                     0
#define UPLANE_ADDRESS_BLK                                                     0
#define UPLANE_ADDRESS_Count                                                   1
#define UPLANE_ADDRESS_FieldMask                                      0xFFFFFFFF
#define UPLANE_ADDRESS_ReadMask                                       0xFFFFFFFC
#define UPLANE_ADDRESS_WriteMask                                      0xFFFFFFFC
#define UPLANE_ADDRESS_ResetValue                                     0x00000000

#define UPLANE_ADDRESS_ADDRESS                                            31 : 0
#define UPLANE_ADDRESS_ADDRESS_End                                            30
#define UPLANE_ADDRESS_ADDRESS_Start                                           0
#define UPLANE_ADDRESS_ADDRESS_Type                                          U31

/*******************************************************************************
** State UPlaneStride
*/

/* Stride of the source U plane in bytes. */

#define UPlaneStrideRegAddrs                                              0x04A2
#define UPLANE_STRIDE_MSB                                                     15
#define UPLANE_STRIDE_LSB                                                      0
#define UPLANE_STRIDE_BLK                                                      0
#define UPLANE_STRIDE_Count                                                    1
#define UPLANE_STRIDE_FieldMask                                       0x0003FFFF
#define UPLANE_STRIDE_ReadMask                                        0x0003FFFC
#define UPLANE_STRIDE_WriteMask                                       0x0003FFFC
#define UPLANE_STRIDE_ResetValue                                      0x00000000

#define UPLANE_STRIDE_STRIDE                                              17 : 0
#define UPLANE_STRIDE_STRIDE_End                                              17
#define UPLANE_STRIDE_STRIDE_Start                                             0
#define UPLANE_STRIDE_STRIDE_Type                                            U18

/*******************************************************************************
** State VPlaneAddress
*/

/* 32-bit aligned base address of the source V plane. */

#define VPlaneAddressRegAddrs                                             0x04A3
#define VPLANE_ADDRESS_MSB                                                    15
#define VPLANE_ADDRESS_LSB                                                     0
#define VPLANE_ADDRESS_BLK                                                     0
#define VPLANE_ADDRESS_Count                                                   1
#define VPLANE_ADDRESS_FieldMask                                      0xFFFFFFFF
#define VPLANE_ADDRESS_ReadMask                                       0xFFFFFFFC
#define VPLANE_ADDRESS_WriteMask                                      0xFFFFFFFC
#define VPLANE_ADDRESS_ResetValue                                     0x00000000

#define VPLANE_ADDRESS_ADDRESS                                            31 : 0
#define VPLANE_ADDRESS_ADDRESS_End                                            30
#define VPLANE_ADDRESS_ADDRESS_Start                                           0
#define VPLANE_ADDRESS_ADDRESS_Type                                          U31

/*******************************************************************************
** State VPlaneStride
*/

/* Stride of the source V plane in bytes. */

#define VPlaneStrideRegAddrs                                              0x04A4
#define VPLANE_STRIDE_MSB                                                     15
#define VPLANE_STRIDE_LSB                                                      0
#define VPLANE_STRIDE_BLK                                                      0
#define VPLANE_STRIDE_Count                                                    1
#define VPLANE_STRIDE_FieldMask                                       0x0003FFFF
#define VPLANE_STRIDE_ReadMask                                        0x0003FFFC
#define VPLANE_STRIDE_WriteMask                                       0x0003FFFC
#define VPLANE_STRIDE_ResetValue                                      0x00000000

#define VPLANE_STRIDE_STRIDE                                              17 : 0
#define VPLANE_STRIDE_STRIDE_End                                              17
#define VPLANE_STRIDE_STRIDE_Start                                             0
#define VPLANE_STRIDE_STRIDE_Type                                            U18

/*******************************************************************************
** State AQPEConfig
*/

/* PE debug register. */

#define AQPEConfigRegAddrs                                                0x04AC
#define AQPE_CONFIG_Address                                              0x012B0
#define AQPE_CONFIG_MSB                                                       15
#define AQPE_CONFIG_LSB                                                        0
#define AQPE_CONFIG_BLK                                                        0
#define AQPE_CONFIG_Count                                                      1
#define AQPE_CONFIG_FieldMask                                         0x0000000B
#define AQPE_CONFIG_ReadMask                                          0x0000000B
#define AQPE_CONFIG_WriteMask                                         0x0000000B
#define AQPE_CONFIG_ResetValue                                        0x00000000

#define AQPE_CONFIG_DESTINATION_FETCH                                      1 : 0
#define AQPE_CONFIG_DESTINATION_FETCH_End                                      1
#define AQPE_CONFIG_DESTINATION_FETCH_Start                                    0
#define AQPE_CONFIG_DESTINATION_FETCH_Type                                   U02
#define   AQPE_CONFIG_DESTINATION_FETCH_DISABLE                              0x0
#define   AQPE_CONFIG_DESTINATION_FETCH_DEFAULT                              0x1
#define   AQPE_CONFIG_DESTINATION_FETCH_ALWAYS                               0x2

#define AQPE_CONFIG_MASK_DESTINATION_FETCH                                 3 : 3
#define AQPE_CONFIG_MASK_DESTINATION_FETCH_End                                 3
#define AQPE_CONFIG_MASK_DESTINATION_FETCH_Start                               3
#define AQPE_CONFIG_MASK_DESTINATION_FETCH_Type                              U01
#define   AQPE_CONFIG_MASK_DESTINATION_FETCH_ENABLED                         0x0
#define   AQPE_CONFIG_MASK_DESTINATION_FETCH_MASKED                          0x1

/*******************************************************************************
** State AQDEDstRotationHeight
*/

/* 180/270 degree rotation configuration for the destination surface. Height
** field specifies the height of the surface in pixels.
*/

#define AQDEDstRotationHeightRegAddrs                                     0x04AD
#define AQDE_DST_ROTATION_HEIGHT_MSB                                          15
#define AQDE_DST_ROTATION_HEIGHT_LSB                                           0
#define AQDE_DST_ROTATION_HEIGHT_BLK                                           0
#define AQDE_DST_ROTATION_HEIGHT_Count                                         1
#define AQDE_DST_ROTATION_HEIGHT_FieldMask                            0x0000FFFF
#define AQDE_DST_ROTATION_HEIGHT_ReadMask                             0x0000FFFF
#define AQDE_DST_ROTATION_HEIGHT_WriteMask                            0x0000FFFF
#define AQDE_DST_ROTATION_HEIGHT_ResetValue                           0x00000000

#define AQDE_DST_ROTATION_HEIGHT_HEIGHT                                   15 : 0
#define AQDE_DST_ROTATION_HEIGHT_HEIGHT_End                                   15
#define AQDE_DST_ROTATION_HEIGHT_HEIGHT_Start                                  0
#define AQDE_DST_ROTATION_HEIGHT_HEIGHT_Type                                 U16

/*******************************************************************************
** State AQDESrcRotationHeight
*/

/* 180/270 degree rotation configuration for the Source surface. Height field
** specifies the height of the surface in pixels.
*/

#define AQDESrcRotationHeightRegAddrs                                     0x04AE
#define AQDE_SRC_ROTATION_HEIGHT_MSB                                          15
#define AQDE_SRC_ROTATION_HEIGHT_LSB                                           0
#define AQDE_SRC_ROTATION_HEIGHT_BLK                                           0
#define AQDE_SRC_ROTATION_HEIGHT_Count                                         1
#define AQDE_SRC_ROTATION_HEIGHT_FieldMask                            0x0000FFFF
#define AQDE_SRC_ROTATION_HEIGHT_ReadMask                             0x0000FFFF
#define AQDE_SRC_ROTATION_HEIGHT_WriteMask                            0x0000FFFF
#define AQDE_SRC_ROTATION_HEIGHT_ResetValue                           0x00000000

#define AQDE_SRC_ROTATION_HEIGHT_HEIGHT                                   15 : 0
#define AQDE_SRC_ROTATION_HEIGHT_HEIGHT_End                                   15
#define AQDE_SRC_ROTATION_HEIGHT_HEIGHT_Start                                  0
#define AQDE_SRC_ROTATION_HEIGHT_HEIGHT_Type                                 U16

/*******************************************************************************
** State AQDERotAngle
*/

/* 0/90/180/270 degree rotation configuration for the Source surface. Height
** field specifies the height of the surface in pixels.
*/

#define AQDERotAngleRegAddrs                                              0x04AF
#define AQDE_ROT_ANGLE_MSB                                                    15
#define AQDE_ROT_ANGLE_LSB                                                     0
#define AQDE_ROT_ANGLE_BLK                                                     0
#define AQDE_ROT_ANGLE_Count                                                   1
#define AQDE_ROT_ANGLE_FieldMask                                      0x000BB33F
#define AQDE_ROT_ANGLE_ReadMask                                       0x000BB33F
#define AQDE_ROT_ANGLE_WriteMask                                      0x000BB33F
#define AQDE_ROT_ANGLE_ResetValue                                     0x00000000

#define AQDE_ROT_ANGLE_SRC                                                 2 : 0
#define AQDE_ROT_ANGLE_SRC_End                                                 2
#define AQDE_ROT_ANGLE_SRC_Start                                               0
#define AQDE_ROT_ANGLE_SRC_Type                                              U03
#define   AQDE_ROT_ANGLE_SRC_ROT0                                            0x0
#define   AQDE_ROT_ANGLE_SRC_FLIP_X                                          0x1
#define   AQDE_ROT_ANGLE_SRC_FLIP_Y                                          0x2
#define   AQDE_ROT_ANGLE_SRC_ROT90                                           0x4
#define   AQDE_ROT_ANGLE_SRC_ROT180                                          0x5
#define   AQDE_ROT_ANGLE_SRC_ROT270                                          0x6

#define AQDE_ROT_ANGLE_DST                                                 5 : 3
#define AQDE_ROT_ANGLE_DST_End                                                 5
#define AQDE_ROT_ANGLE_DST_Start                                               3
#define AQDE_ROT_ANGLE_DST_Type                                              U03
#define   AQDE_ROT_ANGLE_DST_ROT0                                            0x0
#define   AQDE_ROT_ANGLE_DST_FLIP_X                                          0x1
#define   AQDE_ROT_ANGLE_DST_FLIP_Y                                          0x2
#define   AQDE_ROT_ANGLE_DST_ROT90                                           0x4
#define   AQDE_ROT_ANGLE_DST_ROT180                                          0x5
#define   AQDE_ROT_ANGLE_DST_ROT270                                          0x6

#define AQDE_ROT_ANGLE_MASK_SRC                                            8 : 8
#define AQDE_ROT_ANGLE_MASK_SRC_End                                            8
#define AQDE_ROT_ANGLE_MASK_SRC_Start                                          8
#define AQDE_ROT_ANGLE_MASK_SRC_Type                                         U01
#define   AQDE_ROT_ANGLE_MASK_SRC_ENABLED                                    0x0
#define   AQDE_ROT_ANGLE_MASK_SRC_MASKED                                     0x1

#define AQDE_ROT_ANGLE_MASK_DST                                            9 : 9
#define AQDE_ROT_ANGLE_MASK_DST_End                                            9
#define AQDE_ROT_ANGLE_MASK_DST_Start                                          9
#define AQDE_ROT_ANGLE_MASK_DST_Type                                         U01
#define   AQDE_ROT_ANGLE_MASK_DST_ENABLED                                    0x0
#define   AQDE_ROT_ANGLE_MASK_DST_MASKED                                     0x1

#define AQDE_ROT_ANGLE_SRC_MIRROR                                        13 : 12
#define AQDE_ROT_ANGLE_SRC_MIRROR_End                                         13
#define AQDE_ROT_ANGLE_SRC_MIRROR_Start                                       12
#define AQDE_ROT_ANGLE_SRC_MIRROR_Type                                       U02
#define   AQDE_ROT_ANGLE_SRC_MIRROR_NONE                                     0x0
#define   AQDE_ROT_ANGLE_SRC_MIRROR_MIRROR_X                                 0x1
#define   AQDE_ROT_ANGLE_SRC_MIRROR_MIRROR_Y                                 0x2
#define   AQDE_ROT_ANGLE_SRC_MIRROR_MIRROR_XY                                0x3

#define AQDE_ROT_ANGLE_MASK_SRC_MIRROR                                   15 : 15
#define AQDE_ROT_ANGLE_MASK_SRC_MIRROR_End                                    15
#define AQDE_ROT_ANGLE_MASK_SRC_MIRROR_Start                                  15
#define AQDE_ROT_ANGLE_MASK_SRC_MIRROR_Type                                  U01
#define   AQDE_ROT_ANGLE_MASK_SRC_MIRROR_ENABLED                             0x0
#define   AQDE_ROT_ANGLE_MASK_SRC_MIRROR_MASKED                              0x1

#define AQDE_ROT_ANGLE_DST_MIRROR                                        17 : 16
#define AQDE_ROT_ANGLE_DST_MIRROR_End                                         17
#define AQDE_ROT_ANGLE_DST_MIRROR_Start                                       16
#define AQDE_ROT_ANGLE_DST_MIRROR_Type                                       U02
#define   AQDE_ROT_ANGLE_DST_MIRROR_NONE                                     0x0
#define   AQDE_ROT_ANGLE_DST_MIRROR_MIRROR_X                                 0x1
#define   AQDE_ROT_ANGLE_DST_MIRROR_MIRROR_Y                                 0x2
#define   AQDE_ROT_ANGLE_DST_MIRROR_MIRROR_XY                                0x3

#define AQDE_ROT_ANGLE_MASK_DST_MIRROR                                   19 : 19
#define AQDE_ROT_ANGLE_MASK_DST_MIRROR_End                                    19
#define AQDE_ROT_ANGLE_MASK_DST_MIRROR_Start                                  19
#define AQDE_ROT_ANGLE_MASK_DST_MIRROR_Type                                  U01
#define   AQDE_ROT_ANGLE_MASK_DST_MIRROR_ENABLED                             0x0
#define   AQDE_ROT_ANGLE_MASK_DST_MIRROR_MASKED                              0x1

/*******************************************************************************
** State AQDEClearPixelValue32
*/

/* Clear color value in A8R8G8B8 format. */

#define AQDEClearPixelValue32RegAddrs                                     0x04B0
#define AQDE_CLEAR_PIXEL_VALUE32_MSB                                          15
#define AQDE_CLEAR_PIXEL_VALUE32_LSB                                           0
#define AQDE_CLEAR_PIXEL_VALUE32_BLK                                           0
#define AQDE_CLEAR_PIXEL_VALUE32_Count                                         1
#define AQDE_CLEAR_PIXEL_VALUE32_FieldMask                            0xFFFFFFFF
#define AQDE_CLEAR_PIXEL_VALUE32_ReadMask                             0xFFFFFFFF
#define AQDE_CLEAR_PIXEL_VALUE32_WriteMask                            0xFFFFFFFF
#define AQDE_CLEAR_PIXEL_VALUE32_ResetValue                           0x00000000

#define AQDE_CLEAR_PIXEL_VALUE32_ALPHA                                   31 : 24
#define AQDE_CLEAR_PIXEL_VALUE32_ALPHA_End                                    31
#define AQDE_CLEAR_PIXEL_VALUE32_ALPHA_Start                                  24
#define AQDE_CLEAR_PIXEL_VALUE32_ALPHA_Type                                  U08

#define AQDE_CLEAR_PIXEL_VALUE32_RED                                     23 : 16
#define AQDE_CLEAR_PIXEL_VALUE32_RED_End                                      23
#define AQDE_CLEAR_PIXEL_VALUE32_RED_Start                                    16
#define AQDE_CLEAR_PIXEL_VALUE32_RED_Type                                    U08

#define AQDE_CLEAR_PIXEL_VALUE32_GREEN                                    15 : 8
#define AQDE_CLEAR_PIXEL_VALUE32_GREEN_End                                    15
#define AQDE_CLEAR_PIXEL_VALUE32_GREEN_Start                                   8
#define AQDE_CLEAR_PIXEL_VALUE32_GREEN_Type                                  U08

#define AQDE_CLEAR_PIXEL_VALUE32_BLUE                                      7 : 0
#define AQDE_CLEAR_PIXEL_VALUE32_BLUE_End                                      7
#define AQDE_CLEAR_PIXEL_VALUE32_BLUE_Start                                    0
#define AQDE_CLEAR_PIXEL_VALUE32_BLUE_Type                                   U08

/*******************************************************************************
** State AQDEDestColorKey
*/

/* Defines the destination transparency color in destination format. */

#define AQDEDestColorKeyRegAddrs                                          0x04B1
#define AQDE_DEST_COLOR_KEY_MSB                                               15
#define AQDE_DEST_COLOR_KEY_LSB                                                0
#define AQDE_DEST_COLOR_KEY_BLK                                                0
#define AQDE_DEST_COLOR_KEY_Count                                              1
#define AQDE_DEST_COLOR_KEY_FieldMask                                 0xFFFFFFFF
#define AQDE_DEST_COLOR_KEY_ReadMask                                  0xFFFFFFFF
#define AQDE_DEST_COLOR_KEY_WriteMask                                 0xFFFFFFFF
#define AQDE_DEST_COLOR_KEY_ResetValue                                0x00000000

#define AQDE_DEST_COLOR_KEY_ALPHA                                        31 : 24
#define AQDE_DEST_COLOR_KEY_ALPHA_End                                         31
#define AQDE_DEST_COLOR_KEY_ALPHA_Start                                       24
#define AQDE_DEST_COLOR_KEY_ALPHA_Type                                       U08

#define AQDE_DEST_COLOR_KEY_RED                                          23 : 16
#define AQDE_DEST_COLOR_KEY_RED_End                                           23
#define AQDE_DEST_COLOR_KEY_RED_Start                                         16
#define AQDE_DEST_COLOR_KEY_RED_Type                                         U08

#define AQDE_DEST_COLOR_KEY_GREEN                                         15 : 8
#define AQDE_DEST_COLOR_KEY_GREEN_End                                         15
#define AQDE_DEST_COLOR_KEY_GREEN_Start                                        8
#define AQDE_DEST_COLOR_KEY_GREEN_Type                                       U08

#define AQDE_DEST_COLOR_KEY_BLUE                                           7 : 0
#define AQDE_DEST_COLOR_KEY_BLUE_End                                           7
#define AQDE_DEST_COLOR_KEY_BLUE_Start                                         0
#define AQDE_DEST_COLOR_KEY_BLUE_Type                                        U08

/*******************************************************************************
** State AQDEGlobalSrcColor
*/

/* Defines the global source color and alpha values. */

#define AQDEGlobalSrcColorRegAddrs                                        0x04B2
#define AQDE_GLOBAL_SRC_COLOR_MSB                                             15
#define AQDE_GLOBAL_SRC_COLOR_LSB                                              0
#define AQDE_GLOBAL_SRC_COLOR_BLK                                              0
#define AQDE_GLOBAL_SRC_COLOR_Count                                            1
#define AQDE_GLOBAL_SRC_COLOR_FieldMask                               0xFFFFFFFF
#define AQDE_GLOBAL_SRC_COLOR_ReadMask                                0xFFFFFFFF
#define AQDE_GLOBAL_SRC_COLOR_WriteMask                               0xFFFFFFFF
#define AQDE_GLOBAL_SRC_COLOR_ResetValue                              0x00000000

#define AQDE_GLOBAL_SRC_COLOR_ALPHA                                      31 : 24
#define AQDE_GLOBAL_SRC_COLOR_ALPHA_End                                       31
#define AQDE_GLOBAL_SRC_COLOR_ALPHA_Start                                     24
#define AQDE_GLOBAL_SRC_COLOR_ALPHA_Type                                     U08

#define AQDE_GLOBAL_SRC_COLOR_RED                                        23 : 16
#define AQDE_GLOBAL_SRC_COLOR_RED_End                                         23
#define AQDE_GLOBAL_SRC_COLOR_RED_Start                                       16
#define AQDE_GLOBAL_SRC_COLOR_RED_Type                                       U08

#define AQDE_GLOBAL_SRC_COLOR_GREEN                                       15 : 8
#define AQDE_GLOBAL_SRC_COLOR_GREEN_End                                       15
#define AQDE_GLOBAL_SRC_COLOR_GREEN_Start                                      8
#define AQDE_GLOBAL_SRC_COLOR_GREEN_Type                                     U08

#define AQDE_GLOBAL_SRC_COLOR_BLUE                                         7 : 0
#define AQDE_GLOBAL_SRC_COLOR_BLUE_End                                         7
#define AQDE_GLOBAL_SRC_COLOR_BLUE_Start                                       0
#define AQDE_GLOBAL_SRC_COLOR_BLUE_Type                                      U08

/*******************************************************************************
** State AQDEGlobalDestColor
*/

/* Defines the global destination color and alpha values. */

#define AQDEGlobalDestColorRegAddrs                                       0x04B3
#define AQDE_GLOBAL_DEST_COLOR_MSB                                            15
#define AQDE_GLOBAL_DEST_COLOR_LSB                                             0
#define AQDE_GLOBAL_DEST_COLOR_BLK                                             0
#define AQDE_GLOBAL_DEST_COLOR_Count                                           1
#define AQDE_GLOBAL_DEST_COLOR_FieldMask                              0xFFFFFFFF
#define AQDE_GLOBAL_DEST_COLOR_ReadMask                               0xFFFFFFFF
#define AQDE_GLOBAL_DEST_COLOR_WriteMask                              0xFFFFFFFF
#define AQDE_GLOBAL_DEST_COLOR_ResetValue                             0x00000000

#define AQDE_GLOBAL_DEST_COLOR_ALPHA                                     31 : 24
#define AQDE_GLOBAL_DEST_COLOR_ALPHA_End                                      31
#define AQDE_GLOBAL_DEST_COLOR_ALPHA_Start                                    24
#define AQDE_GLOBAL_DEST_COLOR_ALPHA_Type                                    U08

#define AQDE_GLOBAL_DEST_COLOR_RED                                       23 : 16
#define AQDE_GLOBAL_DEST_COLOR_RED_End                                        23
#define AQDE_GLOBAL_DEST_COLOR_RED_Start                                      16
#define AQDE_GLOBAL_DEST_COLOR_RED_Type                                      U08

#define AQDE_GLOBAL_DEST_COLOR_GREEN                                      15 : 8
#define AQDE_GLOBAL_DEST_COLOR_GREEN_End                                      15
#define AQDE_GLOBAL_DEST_COLOR_GREEN_Start                                     8
#define AQDE_GLOBAL_DEST_COLOR_GREEN_Type                                    U08

#define AQDE_GLOBAL_DEST_COLOR_BLUE                                        7 : 0
#define AQDE_GLOBAL_DEST_COLOR_BLUE_End                                        7
#define AQDE_GLOBAL_DEST_COLOR_BLUE_Start                                      0
#define AQDE_GLOBAL_DEST_COLOR_BLUE_Type                                     U08

/*******************************************************************************
** State AQDEColorMultiplyModes
*/

/* Color modes to multiply Source or Destination pixel color by alpha
** channel. Alpha can be from global color source or current pixel.
*/

#define AQDEColorMultiplyModesRegAddrs                                    0x04B4
#define AQDE_COLOR_MULTIPLY_MODES_MSB                                         15
#define AQDE_COLOR_MULTIPLY_MODES_LSB                                          0
#define AQDE_COLOR_MULTIPLY_MODES_BLK                                          0
#define AQDE_COLOR_MULTIPLY_MODES_Count                                        1
#define AQDE_COLOR_MULTIPLY_MODES_FieldMask                           0x00100311
#define AQDE_COLOR_MULTIPLY_MODES_ReadMask                            0x00100311
#define AQDE_COLOR_MULTIPLY_MODES_WriteMask                           0x00100311
#define AQDE_COLOR_MULTIPLY_MODES_ResetValue                          0x00000000

#define AQDE_COLOR_MULTIPLY_MODES_SRC_PREMULTIPLY                          0 : 0
#define AQDE_COLOR_MULTIPLY_MODES_SRC_PREMULTIPLY_End                          0
#define AQDE_COLOR_MULTIPLY_MODES_SRC_PREMULTIPLY_Start                        0
#define AQDE_COLOR_MULTIPLY_MODES_SRC_PREMULTIPLY_Type                       U01
#define   AQDE_COLOR_MULTIPLY_MODES_SRC_PREMULTIPLY_DISABLE                  0x0
#define   AQDE_COLOR_MULTIPLY_MODES_SRC_PREMULTIPLY_ENABLE                   0x1

#define AQDE_COLOR_MULTIPLY_MODES_DST_PREMULTIPLY                          4 : 4
#define AQDE_COLOR_MULTIPLY_MODES_DST_PREMULTIPLY_End                          4
#define AQDE_COLOR_MULTIPLY_MODES_DST_PREMULTIPLY_Start                        4
#define AQDE_COLOR_MULTIPLY_MODES_DST_PREMULTIPLY_Type                       U01
#define   AQDE_COLOR_MULTIPLY_MODES_DST_PREMULTIPLY_DISABLE                  0x0
#define   AQDE_COLOR_MULTIPLY_MODES_DST_PREMULTIPLY_ENABLE                   0x1

#define AQDE_COLOR_MULTIPLY_MODES_SRC_GLOBAL_PREMULTIPLY                   9 : 8
#define AQDE_COLOR_MULTIPLY_MODES_SRC_GLOBAL_PREMULTIPLY_End                   9
#define AQDE_COLOR_MULTIPLY_MODES_SRC_GLOBAL_PREMULTIPLY_Start                 8
#define AQDE_COLOR_MULTIPLY_MODES_SRC_GLOBAL_PREMULTIPLY_Type                U02
#define   AQDE_COLOR_MULTIPLY_MODES_SRC_GLOBAL_PREMULTIPLY_DISABLE           0x0
#define   AQDE_COLOR_MULTIPLY_MODES_SRC_GLOBAL_PREMULTIPLY_ALPHA             0x1
#define   AQDE_COLOR_MULTIPLY_MODES_SRC_GLOBAL_PREMULTIPLY_COLOR             0x2

#define AQDE_COLOR_MULTIPLY_MODES_DST_DEMULTIPLY                         20 : 20
#define AQDE_COLOR_MULTIPLY_MODES_DST_DEMULTIPLY_End                          20
#define AQDE_COLOR_MULTIPLY_MODES_DST_DEMULTIPLY_Start                        20
#define AQDE_COLOR_MULTIPLY_MODES_DST_DEMULTIPLY_Type                        U01
#define   AQDE_COLOR_MULTIPLY_MODES_DST_DEMULTIPLY_DISABLE                   0x0
#define   AQDE_COLOR_MULTIPLY_MODES_DST_DEMULTIPLY_ENABLE                    0x1

/*******************************************************************************
** State AQPETransparency
*/

#define AQPETransparencyRegAddrs                                          0x04B5
#define AQPE_TRANSPARENCY_MSB                                                 15
#define AQPE_TRANSPARENCY_LSB                                                  0
#define AQPE_TRANSPARENCY_BLK                                                  0
#define AQPE_TRANSPARENCY_Count                                                1
#define AQPE_TRANSPARENCY_FieldMask                                   0xB3331333
#define AQPE_TRANSPARENCY_ReadMask                                    0xB3331333
#define AQPE_TRANSPARENCY_WriteMask                                   0xB3331333
#define AQPE_TRANSPARENCY_ResetValue                                  0x00000000

/* Source transparency mode. */
#define AQPE_TRANSPARENCY_SOURCE                                           1 : 0
#define AQPE_TRANSPARENCY_SOURCE_End                                           1
#define AQPE_TRANSPARENCY_SOURCE_Start                                         0
#define AQPE_TRANSPARENCY_SOURCE_Type                                        U02
#define   AQPE_TRANSPARENCY_SOURCE_OPAQUE                                    0x0
#define   AQPE_TRANSPARENCY_SOURCE_MASK                                      0x1
#define   AQPE_TRANSPARENCY_SOURCE_KEY                                       0x2

/* Pattern transparency mode. KEY transparency mode is reserved. */
#define AQPE_TRANSPARENCY_PATTERN                                          5 : 4
#define AQPE_TRANSPARENCY_PATTERN_End                                          5
#define AQPE_TRANSPARENCY_PATTERN_Start                                        4
#define AQPE_TRANSPARENCY_PATTERN_Type                                       U02
#define   AQPE_TRANSPARENCY_PATTERN_OPAQUE                                   0x0
#define   AQPE_TRANSPARENCY_PATTERN_MASK                                     0x1
#define   AQPE_TRANSPARENCY_PATTERN_KEY                                      0x2

/* Destination transparency mode. MASK transparency mode is reserved. */
#define AQPE_TRANSPARENCY_DESTINATION                                      9 : 8
#define AQPE_TRANSPARENCY_DESTINATION_End                                      9
#define AQPE_TRANSPARENCY_DESTINATION_Start                                    8
#define AQPE_TRANSPARENCY_DESTINATION_Type                                   U02
#define   AQPE_TRANSPARENCY_DESTINATION_OPAQUE                               0x0
#define   AQPE_TRANSPARENCY_DESTINATION_MASK                                 0x1
#define   AQPE_TRANSPARENCY_DESTINATION_KEY                                  0x2

/* Mask field for Source/Pattern/Destination fields. */
#define AQPE_TRANSPARENCY_MASK_TRANSPARENCY                              12 : 12
#define AQPE_TRANSPARENCY_MASK_TRANSPARENCY_End                               12
#define AQPE_TRANSPARENCY_MASK_TRANSPARENCY_Start                             12
#define AQPE_TRANSPARENCY_MASK_TRANSPARENCY_Type                             U01
#define   AQPE_TRANSPARENCY_MASK_TRANSPARENCY_ENABLED                        0x0
#define   AQPE_TRANSPARENCY_MASK_TRANSPARENCY_MASKED                         0x1

/* Source usage override. */
#define AQPE_TRANSPARENCY_USE_SRC_OVERRIDE                               17 : 16
#define AQPE_TRANSPARENCY_USE_SRC_OVERRIDE_End                                17
#define AQPE_TRANSPARENCY_USE_SRC_OVERRIDE_Start                              16
#define AQPE_TRANSPARENCY_USE_SRC_OVERRIDE_Type                              U02
#define   AQPE_TRANSPARENCY_USE_SRC_OVERRIDE_DEFAULT                         0x0
#define   AQPE_TRANSPARENCY_USE_SRC_OVERRIDE_USE_ENABLE                      0x1
#define   AQPE_TRANSPARENCY_USE_SRC_OVERRIDE_USE_DISABLE                     0x2

/* Pattern usage override. */
#define AQPE_TRANSPARENCY_USE_PAT_OVERRIDE                               21 : 20
#define AQPE_TRANSPARENCY_USE_PAT_OVERRIDE_End                                21
#define AQPE_TRANSPARENCY_USE_PAT_OVERRIDE_Start                              20
#define AQPE_TRANSPARENCY_USE_PAT_OVERRIDE_Type                              U02
#define   AQPE_TRANSPARENCY_USE_PAT_OVERRIDE_DEFAULT                         0x0
#define   AQPE_TRANSPARENCY_USE_PAT_OVERRIDE_USE_ENABLE                      0x1
#define   AQPE_TRANSPARENCY_USE_PAT_OVERRIDE_USE_DISABLE                     0x2

/* Destination usage override. */
#define AQPE_TRANSPARENCY_USE_DST_OVERRIDE                               25 : 24
#define AQPE_TRANSPARENCY_USE_DST_OVERRIDE_End                                25
#define AQPE_TRANSPARENCY_USE_DST_OVERRIDE_Start                              24
#define AQPE_TRANSPARENCY_USE_DST_OVERRIDE_Type                              U02
#define   AQPE_TRANSPARENCY_USE_DST_OVERRIDE_DEFAULT                         0x0
#define   AQPE_TRANSPARENCY_USE_DST_OVERRIDE_USE_ENABLE                      0x1
#define   AQPE_TRANSPARENCY_USE_DST_OVERRIDE_USE_DISABLE                     0x2

/* 2D resource usage override mask field. */
#define AQPE_TRANSPARENCY_MASK_RESOURCE_OVERRIDE                         28 : 28
#define AQPE_TRANSPARENCY_MASK_RESOURCE_OVERRIDE_End                          28
#define AQPE_TRANSPARENCY_MASK_RESOURCE_OVERRIDE_Start                        28
#define AQPE_TRANSPARENCY_MASK_RESOURCE_OVERRIDE_Type                        U01
#define   AQPE_TRANSPARENCY_MASK_RESOURCE_OVERRIDE_ENABLED                   0x0
#define   AQPE_TRANSPARENCY_MASK_RESOURCE_OVERRIDE_MASKED                    0x1

/* DEB Color Key. */
#define AQPE_TRANSPARENCY_DFB_COLOR_KEY                                  29 : 29
#define AQPE_TRANSPARENCY_DFB_COLOR_KEY_End                                   29
#define AQPE_TRANSPARENCY_DFB_COLOR_KEY_Start                                 29
#define AQPE_TRANSPARENCY_DFB_COLOR_KEY_Type                                 U01
#define   AQPE_TRANSPARENCY_DFB_COLOR_KEY_DISABLED                           0x0
#define   AQPE_TRANSPARENCY_DFB_COLOR_KEY_ENABLED                            0x1

#define AQPE_TRANSPARENCY_MASK_DFB_COLOR_KEY                             31 : 31
#define AQPE_TRANSPARENCY_MASK_DFB_COLOR_KEY_End                              31
#define AQPE_TRANSPARENCY_MASK_DFB_COLOR_KEY_Start                            31
#define AQPE_TRANSPARENCY_MASK_DFB_COLOR_KEY_Type                            U01
#define   AQPE_TRANSPARENCY_MASK_DFB_COLOR_KEY_ENABLED                       0x0
#define   AQPE_TRANSPARENCY_MASK_DFB_COLOR_KEY_MASKED                        0x1

/*******************************************************************************
** State AQPEControl
*/

/* General purpose control register. */

#define AQPEControlRegAddrs                                               0x04B6
#define AQPE_CONTROL_MSB                                                      15
#define AQPE_CONTROL_LSB                                                       0
#define AQPE_CONTROL_BLK                                                       0
#define AQPE_CONTROL_Count                                                     1
#define AQPE_CONTROL_FieldMask                                        0x00000999
#define AQPE_CONTROL_ReadMask                                         0x00000999
#define AQPE_CONTROL_WriteMask                                        0x00000999
#define AQPE_CONTROL_ResetValue                                       0x00000000

#define AQPE_CONTROL_YUV                                                   0 : 0
#define AQPE_CONTROL_YUV_End                                                   0
#define AQPE_CONTROL_YUV_Start                                                 0
#define AQPE_CONTROL_YUV_Type                                                U01
#define   AQPE_CONTROL_YUV_601                                               0x0
#define   AQPE_CONTROL_YUV_709                                               0x1

#define AQPE_CONTROL_MASK_YUV                                              3 : 3
#define AQPE_CONTROL_MASK_YUV_End                                              3
#define AQPE_CONTROL_MASK_YUV_Start                                            3
#define AQPE_CONTROL_MASK_YUV_Type                                           U01
#define   AQPE_CONTROL_MASK_YUV_ENABLED                                      0x0
#define   AQPE_CONTROL_MASK_YUV_MASKED                                       0x1

#define AQPE_CONTROL_UV_SWIZZLE                                            4 : 4
#define AQPE_CONTROL_UV_SWIZZLE_End                                            4
#define AQPE_CONTROL_UV_SWIZZLE_Start                                          4
#define AQPE_CONTROL_UV_SWIZZLE_Type                                         U01
#define   AQPE_CONTROL_UV_SWIZZLE_UV                                         0x0
#define   AQPE_CONTROL_UV_SWIZZLE_VU                                         0x1

#define AQPE_CONTROL_MASK_UV_SWIZZLE                                       7 : 7
#define AQPE_CONTROL_MASK_UV_SWIZZLE_End                                       7
#define AQPE_CONTROL_MASK_UV_SWIZZLE_Start                                     7
#define AQPE_CONTROL_MASK_UV_SWIZZLE_Type                                    U01
#define   AQPE_CONTROL_MASK_UV_SWIZZLE_ENABLED                               0x0
#define   AQPE_CONTROL_MASK_UV_SWIZZLE_MASKED                                0x1

/* YUV to RGB convert enable */
#define AQPE_CONTROL_YUVRGB                                                8 : 8
#define AQPE_CONTROL_YUVRGB_End                                                8
#define AQPE_CONTROL_YUVRGB_Start                                              8
#define AQPE_CONTROL_YUVRGB_Type                                             U01
#define   AQPE_CONTROL_YUVRGB_DISABLED                                       0x0
#define   AQPE_CONTROL_YUVRGB_ENABLED                                        0x1

#define AQPE_CONTROL_MASK_YUVRGB                                         11 : 11
#define AQPE_CONTROL_MASK_YUVRGB_End                                          11
#define AQPE_CONTROL_MASK_YUVRGB_Start                                        11
#define AQPE_CONTROL_MASK_YUVRGB_Type                                        U01
#define   AQPE_CONTROL_MASK_YUVRGB_ENABLED                                   0x0
#define   AQPE_CONTROL_MASK_YUVRGB_MASKED                                    0x1

/*******************************************************************************
** State AQDESrcColorKeyHigh
*/

/* Defines the source transparency color in source format. */

#define AQDESrcColorKeyHighRegAddrs                                       0x04B7
#define AQDE_SRC_COLOR_KEY_HIGH_Address                                  0x012DC
#define AQDE_SRC_COLOR_KEY_HIGH_MSB                                           15
#define AQDE_SRC_COLOR_KEY_HIGH_LSB                                            0
#define AQDE_SRC_COLOR_KEY_HIGH_BLK                                            0
#define AQDE_SRC_COLOR_KEY_HIGH_Count                                          1
#define AQDE_SRC_COLOR_KEY_HIGH_FieldMask                             0xFFFFFFFF
#define AQDE_SRC_COLOR_KEY_HIGH_ReadMask                              0xFFFFFFFF
#define AQDE_SRC_COLOR_KEY_HIGH_WriteMask                             0xFFFFFFFF
#define AQDE_SRC_COLOR_KEY_HIGH_ResetValue                            0x00000000

#define AQDE_SRC_COLOR_KEY_HIGH_ALPHA                                    31 : 24
#define AQDE_SRC_COLOR_KEY_HIGH_ALPHA_End                                     31
#define AQDE_SRC_COLOR_KEY_HIGH_ALPHA_Start                                   24
#define AQDE_SRC_COLOR_KEY_HIGH_ALPHA_Type                                   U08

#define AQDE_SRC_COLOR_KEY_HIGH_RED                                      23 : 16
#define AQDE_SRC_COLOR_KEY_HIGH_RED_End                                       23
#define AQDE_SRC_COLOR_KEY_HIGH_RED_Start                                     16
#define AQDE_SRC_COLOR_KEY_HIGH_RED_Type                                     U08

#define AQDE_SRC_COLOR_KEY_HIGH_GREEN                                     15 : 8
#define AQDE_SRC_COLOR_KEY_HIGH_GREEN_End                                     15
#define AQDE_SRC_COLOR_KEY_HIGH_GREEN_Start                                    8
#define AQDE_SRC_COLOR_KEY_HIGH_GREEN_Type                                   U08

#define AQDE_SRC_COLOR_KEY_HIGH_BLUE                                       7 : 0
#define AQDE_SRC_COLOR_KEY_HIGH_BLUE_End                                       7
#define AQDE_SRC_COLOR_KEY_HIGH_BLUE_Start                                     0
#define AQDE_SRC_COLOR_KEY_HIGH_BLUE_Type                                    U08

/*******************************************************************************
** State AQDEDestColorKeyHigh
*/

/* Defines the destination transparency color in destination format. */

#define AQDEDestColorKeyHighRegAddrs                                      0x04B8
#define AQDE_DEST_COLOR_KEY_HIGH_MSB                                          15
#define AQDE_DEST_COLOR_KEY_HIGH_LSB                                           0
#define AQDE_DEST_COLOR_KEY_HIGH_BLK                                           0
#define AQDE_DEST_COLOR_KEY_HIGH_Count                                         1
#define AQDE_DEST_COLOR_KEY_HIGH_FieldMask                            0xFFFFFFFF
#define AQDE_DEST_COLOR_KEY_HIGH_ReadMask                             0xFFFFFFFF
#define AQDE_DEST_COLOR_KEY_HIGH_WriteMask                            0xFFFFFFFF
#define AQDE_DEST_COLOR_KEY_HIGH_ResetValue                           0x00000000

#define AQDE_DEST_COLOR_KEY_HIGH_ALPHA                                   31 : 24
#define AQDE_DEST_COLOR_KEY_HIGH_ALPHA_End                                    31
#define AQDE_DEST_COLOR_KEY_HIGH_ALPHA_Start                                  24
#define AQDE_DEST_COLOR_KEY_HIGH_ALPHA_Type                                  U08

#define AQDE_DEST_COLOR_KEY_HIGH_RED                                     23 : 16
#define AQDE_DEST_COLOR_KEY_HIGH_RED_End                                      23
#define AQDE_DEST_COLOR_KEY_HIGH_RED_Start                                    16
#define AQDE_DEST_COLOR_KEY_HIGH_RED_Type                                    U08

#define AQDE_DEST_COLOR_KEY_HIGH_GREEN                                    15 : 8
#define AQDE_DEST_COLOR_KEY_HIGH_GREEN_End                                    15
#define AQDE_DEST_COLOR_KEY_HIGH_GREEN_Start                                   8
#define AQDE_DEST_COLOR_KEY_HIGH_GREEN_Type                                  U08

#define AQDE_DEST_COLOR_KEY_HIGH_BLUE                                      7 : 0
#define AQDE_DEST_COLOR_KEY_HIGH_BLUE_End                                      7
#define AQDE_DEST_COLOR_KEY_HIGH_BLUE_Start                                    0
#define AQDE_DEST_COLOR_KEY_HIGH_BLUE_Type                                   U08

/*******************************************************************************
** State AQPEDitherLow
*/

/* PE dither register.
** If you don't want dither, set all fields to their reset values.
*/

#define AQPEDitherLowRegAddrs                                             0x04BA
#define AQPE_DITHER_LOW_MSB                                                   15
#define AQPE_DITHER_LOW_LSB                                                    0
#define AQPE_DITHER_LOW_BLK                                                    0
#define AQPE_DITHER_LOW_Count                                                  1
#define AQPE_DITHER_LOW_FieldMask                                     0xFFFFFFFF
#define AQPE_DITHER_LOW_ReadMask                                      0xFFFFFFFF
#define AQPE_DITHER_LOW_WriteMask                                     0xFFFFFFFF
#define AQPE_DITHER_LOW_ResetValue                                    0xFFFFFFFF

/* X,Y = 0,0 */
#define AQPE_DITHER_LOW_PIXEL_X0_Y0                                        3 : 0
#define AQPE_DITHER_LOW_PIXEL_X0_Y0_End                                        3
#define AQPE_DITHER_LOW_PIXEL_X0_Y0_Start                                      0
#define AQPE_DITHER_LOW_PIXEL_X0_Y0_Type                                     U04

/* X,Y = 1,0 */
#define AQPE_DITHER_LOW_PIXEL_X1_Y0                                        7 : 4
#define AQPE_DITHER_LOW_PIXEL_X1_Y0_End                                        7
#define AQPE_DITHER_LOW_PIXEL_X1_Y0_Start                                      4
#define AQPE_DITHER_LOW_PIXEL_X1_Y0_Type                                     U04

/* X,Y = 2,0 */
#define AQPE_DITHER_LOW_PIXEL_X2_Y0                                       11 : 8
#define AQPE_DITHER_LOW_PIXEL_X2_Y0_End                                       11
#define AQPE_DITHER_LOW_PIXEL_X2_Y0_Start                                      8
#define AQPE_DITHER_LOW_PIXEL_X2_Y0_Type                                     U04

/* X,Y = 3,0 */
#define AQPE_DITHER_LOW_PIXEL_X3_Y0                                      15 : 12
#define AQPE_DITHER_LOW_PIXEL_X3_Y0_End                                       15
#define AQPE_DITHER_LOW_PIXEL_X3_Y0_Start                                     12
#define AQPE_DITHER_LOW_PIXEL_X3_Y0_Type                                     U04

/* X,Y = 0,1 */
#define AQPE_DITHER_LOW_PIXEL_X0_Y1                                      19 : 16
#define AQPE_DITHER_LOW_PIXEL_X0_Y1_End                                       19
#define AQPE_DITHER_LOW_PIXEL_X0_Y1_Start                                     16
#define AQPE_DITHER_LOW_PIXEL_X0_Y1_Type                                     U04

/* X,Y = 1,1 */
#define AQPE_DITHER_LOW_PIXEL_X1_Y1                                      23 : 20
#define AQPE_DITHER_LOW_PIXEL_X1_Y1_End                                       23
#define AQPE_DITHER_LOW_PIXEL_X1_Y1_Start                                     20
#define AQPE_DITHER_LOW_PIXEL_X1_Y1_Type                                     U04

/* X,Y = 2,1 */
#define AQPE_DITHER_LOW_PIXEL_X2_Y1                                      27 : 24
#define AQPE_DITHER_LOW_PIXEL_X2_Y1_End                                       27
#define AQPE_DITHER_LOW_PIXEL_X2_Y1_Start                                     24
#define AQPE_DITHER_LOW_PIXEL_X2_Y1_Type                                     U04

/* X,Y = 3,1 */
#define AQPE_DITHER_LOW_PIXEL_X3_Y1                                      31 : 28
#define AQPE_DITHER_LOW_PIXEL_X3_Y1_End                                       31
#define AQPE_DITHER_LOW_PIXEL_X3_Y1_Start                                     28
#define AQPE_DITHER_LOW_PIXEL_X3_Y1_Type                                     U04

/*******************************************************************************
** State AQPEDitherHigh
*/

#define AQPEDitherHighRegAddrs                                            0x04BB
#define AQPE_DITHER_HIGH_MSB                                                  15
#define AQPE_DITHER_HIGH_LSB                                                   0
#define AQPE_DITHER_LOW_HIGH_BLK                                               0
#define AQPE_DITHER_HIGH_Count                                                 1
#define AQPE_DITHER_HIGH_FieldMask                                    0xFFFFFFFF
#define AQPE_DITHER_HIGH_ReadMask                                     0xFFFFFFFF
#define AQPE_DITHER_HIGH_WriteMask                                    0xFFFFFFFF
#define AQPE_DITHER_HIGH_ResetValue                                   0xFFFFFFFF

/* X,Y = 0,2 */
#define AQPE_DITHER_HIGH_PIXEL_X0_Y2                                       3 : 0
#define AQPE_DITHER_HIGH_PIXEL_X0_Y2_End                                       3
#define AQPE_DITHER_HIGH_PIXEL_X0_Y2_Start                                     0
#define AQPE_DITHER_HIGH_PIXEL_X0_Y2_Type                                    U04

/* X,Y = 1,2 */
#define AQPE_DITHER_HIGH_PIXEL_X1_Y2                                       7 : 4
#define AQPE_DITHER_HIGH_PIXEL_X1_Y2_End                                       7
#define AQPE_DITHER_HIGH_PIXEL_X1_Y2_Start                                     4
#define AQPE_DITHER_HIGH_PIXEL_X1_Y2_Type                                    U04

/* X,Y = 2,2 */
#define AQPE_DITHER_HIGH_PIXEL_X2_Y2                                      11 : 8
#define AQPE_DITHER_HIGH_PIXEL_X2_Y2_End                                      11
#define AQPE_DITHER_HIGH_PIXEL_X2_Y2_Start                                     8
#define AQPE_DITHER_HIGH_PIXEL_X2_Y2_Type                                    U04

/* X,Y = 0,3 */
#define AQPE_DITHER_HIGH_PIXEL_X3_Y2                                     15 : 12
#define AQPE_DITHER_HIGH_PIXEL_X3_Y2_End                                      15
#define AQPE_DITHER_HIGH_PIXEL_X3_Y2_Start                                    12
#define AQPE_DITHER_HIGH_PIXEL_X3_Y2_Type                                    U04

/* X,Y = 1,3 */
#define AQPE_DITHER_HIGH_PIXEL_X0_Y3                                     19 : 16
#define AQPE_DITHER_HIGH_PIXEL_X0_Y3_End                                      19
#define AQPE_DITHER_HIGH_PIXEL_X0_Y3_Start                                    16
#define AQPE_DITHER_HIGH_PIXEL_X0_Y3_Type                                    U04

/* X,Y = 2,3 */
#define AQPE_DITHER_HIGH_PIXEL_X1_Y3                                     23 : 20
#define AQPE_DITHER_HIGH_PIXEL_X1_Y3_End                                      23
#define AQPE_DITHER_HIGH_PIXEL_X1_Y3_Start                                    20
#define AQPE_DITHER_HIGH_PIXEL_X1_Y3_Type                                    U04

/* X,Y = 3,3 */
#define AQPE_DITHER_HIGH_PIXEL_X2_Y3                                     27 : 24
#define AQPE_DITHER_HIGH_PIXEL_X2_Y3_End                                      27
#define AQPE_DITHER_HIGH_PIXEL_X2_Y3_Start                                    24
#define AQPE_DITHER_HIGH_PIXEL_X2_Y3_Type                                    U04

/* X,Y = 3,2 */
#define AQPE_DITHER_HIGH_PIXEL_X3_Y3                                     31 : 28
#define AQPE_DITHER_HIGH_PIXEL_X3_Y3_End                                      31
#define AQPE_DITHER_HIGH_PIXEL_X3_Y3_Start                                    28
#define AQPE_DITHER_HIGH_PIXEL_X3_Y3_Type                                    U04

/*******************************************************************************
** State AQDESrcExConfig
*/

#define AQDESrcExConfigRegAddrs                                           0x04C0
#define AQDE_SRC_EX_CONFIG_MSB                                                15
#define AQDE_SRC_EX_CONFIG_LSB                                                 0
#define AQDE_SRC_EX_CONFIG_BLK                                                 0
#define AQDE_SRC_EX_CONFIG_Count                                               1
#define AQDE_SRC_EX_CONFIG_FieldMask                                  0x00000109
#define AQDE_SRC_EX_CONFIG_ReadMask                                   0x00000109
#define AQDE_SRC_EX_CONFIG_WriteMask                                  0x00000109
#define AQDE_SRC_EX_CONFIG_ResetValue                                 0x00000000

/* Source multi tiled address computation control. */
#define AQDE_SRC_EX_CONFIG_MULTI_TILED                                     0 : 0
#define AQDE_SRC_EX_CONFIG_MULTI_TILED_End                                     0
#define AQDE_SRC_EX_CONFIG_MULTI_TILED_Start                                   0
#define AQDE_SRC_EX_CONFIG_MULTI_TILED_Type                                  U01
#define   AQDE_SRC_EX_CONFIG_MULTI_TILED_DISABLED                            0x0
#define   AQDE_SRC_EX_CONFIG_MULTI_TILED_ENABLED                             0x1

/* Source super tiled address computation control. */
#define AQDE_SRC_EX_CONFIG_SUPER_TILED                                     3 : 3
#define AQDE_SRC_EX_CONFIG_SUPER_TILED_End                                     3
#define AQDE_SRC_EX_CONFIG_SUPER_TILED_Start                                   3
#define AQDE_SRC_EX_CONFIG_SUPER_TILED_Type                                  U01
#define   AQDE_SRC_EX_CONFIG_SUPER_TILED_DISABLED                            0x0
#define   AQDE_SRC_EX_CONFIG_SUPER_TILED_ENABLED                             0x1

/* Source super tiled address computation control. */
#define AQDE_SRC_EX_CONFIG_MINOR_TILED                                     8 : 8
#define AQDE_SRC_EX_CONFIG_MINOR_TILED_End                                     8
#define AQDE_SRC_EX_CONFIG_MINOR_TILED_Start                                   8
#define AQDE_SRC_EX_CONFIG_MINOR_TILED_Type                                  U01
#define   AQDE_SRC_EX_CONFIG_MINOR_TILED_DISABLED                            0x0
#define   AQDE_SRC_EX_CONFIG_MINOR_TILED_ENABLED                             0x1

/*******************************************************************************
** State AQDESrcExAddress
*/

/* 32-bit aligned base address of the source extra surface. */

#define AQDESrcExAddressRegAddrs                                          0x04C1
#define AQDE_SRC_EX_ADDRESS_MSB                                               15
#define AQDE_SRC_EX_ADDRESS_LSB                                                0
#define AQDE_SRC_EX_ADDRESS_BLK                                                0
#define AQDE_SRC_EX_ADDRESS_Count                                              1
#define AQDE_SRC_EX_ADDRESS_FieldMask                                 0xFFFFFFFF
#define AQDE_SRC_EX_ADDRESS_ReadMask                                  0xFFFFFFFC
#define AQDE_SRC_EX_ADDRESS_WriteMask                                 0xFFFFFFFC
#define AQDE_SRC_EX_ADDRESS_ResetValue                                0x00000000

#define AQDE_SRC_EX_ADDRESS_ADDRESS                                       31 : 0
#define AQDE_SRC_EX_ADDRESS_ADDRESS_End                                       30
#define AQDE_SRC_EX_ADDRESS_ADDRESS_Start                                      0
#define AQDE_SRC_EX_ADDRESS_ADDRESS_Type                                     U31

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
** State gcregDESrcAddress
*/

/* 32-bit aligned base address of the source surface. */

#define gcregDESrcAddressRegAddrs                                         0x4A00
#define GCREG_DE_SRC_ADDRESS_MSB                                              15
#define GCREG_DE_SRC_ADDRESS_LSB                                               2
#define GCREG_DE_SRC_ADDRESS_BLK                                               0
#define GCREG_DE_SRC_ADDRESS_Count                                             4
#define GCREG_DE_SRC_ADDRESS_FieldMask                                0xFFFFFFFF
#define GCREG_DE_SRC_ADDRESS_ReadMask                                 0xFFFFFFFC
#define GCREG_DE_SRC_ADDRESS_WriteMask                                0xFFFFFFFC
#define GCREG_DE_SRC_ADDRESS_ResetValue                               0x00000000

#define GCREG_DE_SRC_ADDRESS_ADDRESS                                      31 : 0
#define GCREG_DE_SRC_ADDRESS_ADDRESS_End                                      30
#define GCREG_DE_SRC_ADDRESS_ADDRESS_Start                                     0
#define GCREG_DE_SRC_ADDRESS_ADDRESS_Type                                    U31

/*******************************************************************************
** State gcregDESrcStride
*/

/* Stride of the source surface in bytes. To calculate the stride multiply
** the surface width in pixels by the number of  bytes per pixel.
*/

#define gcregDESrcStrideRegAddrs                                          0x4A04
#define GCREG_DE_SRC_STRIDE_MSB                                               15
#define GCREG_DE_SRC_STRIDE_LSB                                                2
#define GCREG_DE_SRC_STRIDE_BLK                                                0
#define GCREG_DE_SRC_STRIDE_Count                                              4
#define GCREG_DE_SRC_STRIDE_FieldMask                                 0x0003FFFF
#define GCREG_DE_SRC_STRIDE_ReadMask                                  0x0003FFFC
#define GCREG_DE_SRC_STRIDE_WriteMask                                 0x0003FFFC
#define GCREG_DE_SRC_STRIDE_ResetValue                                0x00000000

#define GCREG_DE_SRC_STRIDE_STRIDE                                        17 : 0
#define GCREG_DE_SRC_STRIDE_STRIDE_End                                        17
#define GCREG_DE_SRC_STRIDE_STRIDE_Start                                       0
#define GCREG_DE_SRC_STRIDE_STRIDE_Type                                      U18

/*******************************************************************************
** State gcregDESrcRotationConfig
*/

/* 90 degree rotation configuration for the source surface. Width field
** specifies the width of the surface in pixels.
*/

#define gcregDESrcRotationConfigRegAddrs                                  0x4A08
#define GCREG_DE_SRC_ROTATION_CONFIG_MSB                                      15
#define GCREG_DE_SRC_ROTATION_CONFIG_LSB                                       2
#define GCREG_DE_SRC_ROTATION_CONFIG_BLK                                       0
#define GCREG_DE_SRC_ROTATION_CONFIG_Count                                     4
#define GCREG_DE_SRC_ROTATION_CONFIG_FieldMask                        0x0001FFFF
#define GCREG_DE_SRC_ROTATION_CONFIG_ReadMask                         0x0001FFFF
#define GCREG_DE_SRC_ROTATION_CONFIG_WriteMask                        0x0001FFFF
#define GCREG_DE_SRC_ROTATION_CONFIG_ResetValue                       0x00000000

#define GCREG_DE_SRC_ROTATION_CONFIG_WIDTH                                15 : 0
#define GCREG_DE_SRC_ROTATION_CONFIG_WIDTH_End                                15
#define GCREG_DE_SRC_ROTATION_CONFIG_WIDTH_Start                               0
#define GCREG_DE_SRC_ROTATION_CONFIG_WIDTH_Type                              U16

#define GCREG_DE_SRC_ROTATION_CONFIG_ROTATION                            16 : 16
#define GCREG_DE_SRC_ROTATION_CONFIG_ROTATION_End                             16
#define GCREG_DE_SRC_ROTATION_CONFIG_ROTATION_Start                           16
#define GCREG_DE_SRC_ROTATION_CONFIG_ROTATION_Type                           U01
#define   GCREG_DE_SRC_ROTATION_CONFIG_ROTATION_NORMAL                       0x0
#define   GCREG_DE_SRC_ROTATION_CONFIG_ROTATION_ROTATED                      0x1

/*******************************************************************************
** State gcregDESrcConfig
*/

/* Source surface configuration register. */

#define gcregDESrcConfigRegAddrs                                          0x4A0C
#define GCREG_DE_SRC_CONFIG_MSB                                               15
#define GCREG_DE_SRC_CONFIG_LSB                                                2
#define GCREG_DE_SRC_CONFIG_BLK                                                0
#define GCREG_DE_SRC_CONFIG_Count                                              4
#define GCREG_DE_SRC_CONFIG_FieldMask                                 0xDF30B1C0
#define GCREG_DE_SRC_CONFIG_ReadMask                                  0xDF30B1C0
#define GCREG_DE_SRC_CONFIG_WriteMask                                 0xDF30B1C0
#define GCREG_DE_SRC_CONFIG_ResetValue                                0x00000000

/* Control source endianess. */
#define GCREG_DE_SRC_CONFIG_ENDIAN_CONTROL                               31 : 30
#define GCREG_DE_SRC_CONFIG_ENDIAN_CONTROL_End                                31
#define GCREG_DE_SRC_CONFIG_ENDIAN_CONTROL_Start                              30
#define GCREG_DE_SRC_CONFIG_ENDIAN_CONTROL_Type                              U02
#define   GCREG_DE_SRC_CONFIG_ENDIAN_CONTROL_NO_SWAP                         0x0
#define   GCREG_DE_SRC_CONFIG_ENDIAN_CONTROL_SWAP_WORD                       0x1
#define   GCREG_DE_SRC_CONFIG_ENDIAN_CONTROL_SWAP_DWORD                      0x2

/* Defines the pixel format of the source surface. */
#define GCREG_DE_SRC_CONFIG_SOURCE_FORMAT                                28 : 24
#define GCREG_DE_SRC_CONFIG_SOURCE_FORMAT_End                                 28
#define GCREG_DE_SRC_CONFIG_SOURCE_FORMAT_Start                               24
#define GCREG_DE_SRC_CONFIG_SOURCE_FORMAT_Type                               U05
#define   GCREG_DE_SRC_CONFIG_SOURCE_FORMAT_X4R4G4B4                        0x00
#define   GCREG_DE_SRC_CONFIG_SOURCE_FORMAT_A4R4G4B4                        0x01
#define   GCREG_DE_SRC_CONFIG_SOURCE_FORMAT_X1R5G5B5                        0x02
#define   GCREG_DE_SRC_CONFIG_SOURCE_FORMAT_A1R5G5B5                        0x03
#define   GCREG_DE_SRC_CONFIG_SOURCE_FORMAT_R5G6B5                          0x04
#define   GCREG_DE_SRC_CONFIG_SOURCE_FORMAT_X8R8G8B8                        0x05
#define   GCREG_DE_SRC_CONFIG_SOURCE_FORMAT_A8R8G8B8                        0x06
#define   GCREG_DE_SRC_CONFIG_SOURCE_FORMAT_YUY2                            0x07
#define   GCREG_DE_SRC_CONFIG_SOURCE_FORMAT_UYVY                            0x08
#define   GCREG_DE_SRC_CONFIG_SOURCE_FORMAT_INDEX8                          0x09
#define   GCREG_DE_SRC_CONFIG_SOURCE_FORMAT_MONOCHROME                      0x0A
#define   GCREG_DE_SRC_CONFIG_SOURCE_FORMAT_YV12                            0x0F
#define   GCREG_DE_SRC_CONFIG_SOURCE_FORMAT_A8                              0x10
#define   GCREG_DE_SRC_CONFIG_SOURCE_FORMAT_NV12                            0x11
#define   GCREG_DE_SRC_CONFIG_SOURCE_FORMAT_NV16                            0x12
#define   GCREG_DE_SRC_CONFIG_SOURCE_FORMAT_RG16                            0x13

/* Color channel swizzles. */
#define GCREG_DE_SRC_CONFIG_SWIZZLE                                      21 : 20
#define GCREG_DE_SRC_CONFIG_SWIZZLE_End                                       21
#define GCREG_DE_SRC_CONFIG_SWIZZLE_Start                                     20
#define GCREG_DE_SRC_CONFIG_SWIZZLE_Type                                     U02
#define   GCREG_DE_SRC_CONFIG_SWIZZLE_ARGB                                   0x0
#define   GCREG_DE_SRC_CONFIG_SWIZZLE_RGBA                                   0x1
#define   GCREG_DE_SRC_CONFIG_SWIZZLE_ABGR                                   0x2
#define   GCREG_DE_SRC_CONFIG_SWIZZLE_BGRA                                   0x3

/* Mono expansion: if 0, transparency color will be 0, otherwise transparency **
** color will be 1.                                                           */
#define GCREG_DE_SRC_CONFIG_MONO_TRANSPARENCY                            15 : 15
#define GCREG_DE_SRC_CONFIG_MONO_TRANSPARENCY_End                             15
#define GCREG_DE_SRC_CONFIG_MONO_TRANSPARENCY_Start                           15
#define GCREG_DE_SRC_CONFIG_MONO_TRANSPARENCY_Type                           U01
#define   GCREG_DE_SRC_CONFIG_MONO_TRANSPARENCY_BACKGROUND                   0x0
#define   GCREG_DE_SRC_CONFIG_MONO_TRANSPARENCY_FOREGROUND                   0x1

/* Mono expansion or masked blit: stream packing in pixels. Determines how    **
** many horizontal pixels are there per each 32-bit chunk. For example, if    **
** set to Packed8, each 32-bit chunk is 8-pixel wide, which also means that   **
** it defines 4 vertical lines of pixels.                                     */
#define GCREG_DE_SRC_CONFIG_PACK                                         13 : 12
#define GCREG_DE_SRC_CONFIG_PACK_End                                          13
#define GCREG_DE_SRC_CONFIG_PACK_Start                                        12
#define GCREG_DE_SRC_CONFIG_PACK_Type                                        U02
#define   GCREG_DE_SRC_CONFIG_PACK_PACKED8                                   0x0
#define   GCREG_DE_SRC_CONFIG_PACK_PACKED16                                  0x1
#define   GCREG_DE_SRC_CONFIG_PACK_PACKED32                                  0x2
#define   GCREG_DE_SRC_CONFIG_PACK_UNPACKED                                  0x3

/* Source data location: set to STREAM for mono expansion blits or masked     **
** blits. For mono expansion blits the complete bitmap comes from the command **
** stream. For masked blits the source data comes from the memory and the     **
** mask from the command stream.                                              */
#define GCREG_DE_SRC_CONFIG_LOCATION                                       8 : 8
#define GCREG_DE_SRC_CONFIG_LOCATION_End                                       8
#define GCREG_DE_SRC_CONFIG_LOCATION_Start                                     8
#define GCREG_DE_SRC_CONFIG_LOCATION_Type                                    U01
#define   GCREG_DE_SRC_CONFIG_LOCATION_MEMORY                                0x0
#define   GCREG_DE_SRC_CONFIG_LOCATION_STREAM                                0x1

/* Source linear/tiled address computation control. */
#define GCREG_DE_SRC_CONFIG_TILED                                          7 : 7
#define GCREG_DE_SRC_CONFIG_TILED_End                                          7
#define GCREG_DE_SRC_CONFIG_TILED_Start                                        7
#define GCREG_DE_SRC_CONFIG_TILED_Type                                       U01
#define   GCREG_DE_SRC_CONFIG_TILED_DISABLED                                 0x0
#define   GCREG_DE_SRC_CONFIG_TILED_ENABLED                                  0x1

/* If set to ABSOLUTE, the source coordinates are treated as absolute         **
** coordinates inside the source surface. If set to RELATIVE, the source      **
** coordinates are treated as the offsets from the destination coordinates    **
** with the source size equal to the size of the destination.                 */
#define GCREG_DE_SRC_CONFIG_SRC_RELATIVE                                   6 : 6
#define GCREG_DE_SRC_CONFIG_SRC_RELATIVE_End                                   6
#define GCREG_DE_SRC_CONFIG_SRC_RELATIVE_Start                                 6
#define GCREG_DE_SRC_CONFIG_SRC_RELATIVE_Type                                U01
#define   GCREG_DE_SRC_CONFIG_SRC_RELATIVE_ABSOLUTE                          0x0
#define   GCREG_DE_SRC_CONFIG_SRC_RELATIVE_RELATIVE                          0x1

/*******************************************************************************
** State gcregDESrcOrigin
*/

/* Absolute or relative (see SRC_RELATIVE field of gcregDESrcConfig register)
** X and Y coordinates in pixels of the top left corner of the source
** rectangle within the source surface.
*/

#define gcregDESrcOriginRegAddrs                                          0x4A10
#define GCREG_DE_SRC_ORIGIN_MSB                                               15
#define GCREG_DE_SRC_ORIGIN_LSB                                                2
#define GCREG_DE_SRC_ORIGIN_BLK                                                0
#define GCREG_DE_SRC_ORIGIN_Count                                              4
#define GCREG_DE_SRC_ORIGIN_FieldMask                                 0xFFFFFFFF
#define GCREG_DE_SRC_ORIGIN_ReadMask                                  0xFFFFFFFF
#define GCREG_DE_SRC_ORIGIN_WriteMask                                 0xFFFFFFFF
#define GCREG_DE_SRC_ORIGIN_ResetValue                                0x00000000

#define GCREG_DE_SRC_ORIGIN_Y                                            31 : 16
#define GCREG_DE_SRC_ORIGIN_Y_End                                             31
#define GCREG_DE_SRC_ORIGIN_Y_Start                                           16
#define GCREG_DE_SRC_ORIGIN_Y_Type                                           U16

#define GCREG_DE_SRC_ORIGIN_X                                             15 : 0
#define GCREG_DE_SRC_ORIGIN_X_End                                             15
#define GCREG_DE_SRC_ORIGIN_X_Start                                            0
#define GCREG_DE_SRC_ORIGIN_X_Type                                           U16

/*******************************************************************************
** State gcregDESrcSize
*/

/* Width and height of the source rectangle in pixels. If the source is
** relative (see SRC_RELATIVE field of gcregDESrcConfig register) or a
** regular bitblt is being performed without stretching, this register is
** ignored and the source size is assumed to be the same as the destination.
*/

#define gcregDESrcSizeRegAddrs                                            0x4A14
#define GCREG_DE_SRC_SIZE_MSB                                                 15
#define GCREG_DE_SRC_SIZE_LSB                                                  2
#define GCREG_DE_SRC_SIZE_BLK                                                  0
#define GCREG_DE_SRC_SIZE_Count                                                4
#define GCREG_DE_SRC_SIZE_FieldMask                                   0xFFFFFFFF
#define GCREG_DE_SRC_SIZE_ReadMask                                    0xFFFFFFFF
#define GCREG_DE_SRC_SIZE_WriteMask                                   0xFFFFFFFF
#define GCREG_DE_SRC_SIZE_ResetValue                                  0x00000000

#define GCREG_DE_SRC_SIZE_Y                                              31 : 16
#define GCREG_DE_SRC_SIZE_Y_End                                               31
#define GCREG_DE_SRC_SIZE_Y_Start                                             16
#define GCREG_DE_SRC_SIZE_Y_Type                                             U16

#define GCREG_DE_SRC_SIZE_X                                               15 : 0
#define GCREG_DE_SRC_SIZE_X_End                                               15
#define GCREG_DE_SRC_SIZE_X_Start                                              0
#define GCREG_DE_SRC_SIZE_X_Type                                             U16

/*******************************************************************************
** State gcregDESrcColorBg
*/

/* Select the color where source becomes transparent. It must be programmed
** in A8R8G8B8 format.
*/

#define gcregDESrcColorBgRegAddrs                                         0x4A18
#define GCREG_DE_SRC_COLOR_BG_MSB                                             15
#define GCREG_DE_SRC_COLOR_BG_LSB                                              2
#define GCREG_DE_SRC_COLOR_BG_BLK                                              0
#define GCREG_DE_SRC_COLOR_BG_Count                                            4
#define GCREG_DE_SRC_COLOR_BG_FieldMask                               0xFFFFFFFF
#define GCREG_DE_SRC_COLOR_BG_ReadMask                                0xFFFFFFFF
#define GCREG_DE_SRC_COLOR_BG_WriteMask                               0xFFFFFFFF
#define GCREG_DE_SRC_COLOR_BG_ResetValue                              0x00000000

#define GCREG_DE_SRC_COLOR_BG_ALPHA                                      31 : 24
#define GCREG_DE_SRC_COLOR_BG_ALPHA_End                                       31
#define GCREG_DE_SRC_COLOR_BG_ALPHA_Start                                     24
#define GCREG_DE_SRC_COLOR_BG_ALPHA_Type                                     U08

#define GCREG_DE_SRC_COLOR_BG_RED                                        23 : 16
#define GCREG_DE_SRC_COLOR_BG_RED_End                                         23
#define GCREG_DE_SRC_COLOR_BG_RED_Start                                       16
#define GCREG_DE_SRC_COLOR_BG_RED_Type                                       U08

#define GCREG_DE_SRC_COLOR_BG_GREEN                                       15 : 8
#define GCREG_DE_SRC_COLOR_BG_GREEN_End                                       15
#define GCREG_DE_SRC_COLOR_BG_GREEN_Start                                      8
#define GCREG_DE_SRC_COLOR_BG_GREEN_Type                                     U08

#define GCREG_DE_SRC_COLOR_BG_BLUE                                         7 : 0
#define GCREG_DE_SRC_COLOR_BG_BLUE_End                                         7
#define GCREG_DE_SRC_COLOR_BG_BLUE_Start                                       0
#define GCREG_DE_SRC_COLOR_BG_BLUE_Type                                      U08

/*******************************************************************************
** State gcregDERop
*/

/* Raster operation foreground and background codes. Even though ROP is not
** used in CLEAR, HOR_FILTER_BLT, VER_FILTER_BLT and alpha-eanbled BIT_BLTs,
** ROP code still has to be programmed, because the engine makes the decision
** whether source, destination and pattern are involved in the current
** operation and the correct decision is essential for the engine to complete
** the operation as expected.
*/

#define gcregDERopRegAddrs                                                0x4A1C
#define GCREG_DE_ROP_MSB                                                      15
#define GCREG_DE_ROP_LSB                                                       2
#define GCREG_DE_ROP_BLK                                                       0
#define GCREG_DE_ROP_Count                                                     4
#define GCREG_DE_ROP_FieldMask                                        0x0030FFFF
#define GCREG_DE_ROP_ReadMask                                         0x0030FFFF
#define GCREG_DE_ROP_WriteMask                                        0x0030FFFF
#define GCREG_DE_ROP_ResetValue                                       0x00000000

/* ROP type: ROP2, ROP3 or ROP4 */
#define GCREG_DE_ROP_TYPE                                                21 : 20
#define GCREG_DE_ROP_TYPE_End                                                 21
#define GCREG_DE_ROP_TYPE_Start                                               20
#define GCREG_DE_ROP_TYPE_Type                                               U02
#define   GCREG_DE_ROP_TYPE_ROP2_PATTERN                                     0x0
#define   GCREG_DE_ROP_TYPE_ROP2_SOURCE                                      0x1
#define   GCREG_DE_ROP_TYPE_ROP3                                             0x2
#define   GCREG_DE_ROP_TYPE_ROP4                                             0x3

/* Background ROP code is used for transparent pixels. */
#define GCREG_DE_ROP_ROP_BG                                               15 : 8
#define GCREG_DE_ROP_ROP_BG_End                                               15
#define GCREG_DE_ROP_ROP_BG_Start                                              8
#define GCREG_DE_ROP_ROP_BG_Type                                             U08

/* Background ROP code is used for opaque pixels. */
#define GCREG_DE_ROP_ROP_FG                                                7 : 0
#define GCREG_DE_ROP_ROP_FG_End                                                7
#define GCREG_DE_ROP_ROP_FG_Start                                              0
#define GCREG_DE_ROP_ROP_FG_Type                                             U08

/*******************************************************************************
** State gcregDEAlphaControl
*/

#define gcregDEAlphaControlRegAddrs                                       0x4A20
#define GCREG_DE_ALPHA_CONTROL_MSB                                            15
#define GCREG_DE_ALPHA_CONTROL_LSB                                             2
#define GCREG_DE_ALPHA_CONTROL_BLK                                             0
#define GCREG_DE_ALPHA_CONTROL_Count                                           4
#define GCREG_DE_ALPHA_CONTROL_FieldMask                              0x00000001
#define GCREG_DE_ALPHA_CONTROL_ReadMask                               0x00000001
#define GCREG_DE_ALPHA_CONTROL_WriteMask                              0x00000001
#define GCREG_DE_ALPHA_CONTROL_ResetValue                             0x00000000

#define GCREG_DE_ALPHA_CONTROL_ENABLE                                      0 : 0
#define GCREG_DE_ALPHA_CONTROL_ENABLE_End                                      0
#define GCREG_DE_ALPHA_CONTROL_ENABLE_Start                                    0
#define GCREG_DE_ALPHA_CONTROL_ENABLE_Type                                   U01
#define   GCREG_DE_ALPHA_CONTROL_ENABLE_OFF                                  0x0
#define   GCREG_DE_ALPHA_CONTROL_ENABLE_ON                                   0x1

/*******************************************************************************
** State gcregDEAlphaModes
*/

#define gcregDEAlphaModesRegAddrs                                         0x4A24
#define GCREG_DE_ALPHA_MODES_MSB                                              15
#define GCREG_DE_ALPHA_MODES_LSB                                               2
#define GCREG_DE_ALPHA_MODES_BLK                                               0
#define GCREG_DE_ALPHA_MODES_Count                                             4
#define GCREG_DE_ALPHA_MODES_FieldMask                                0xFF003311
#define GCREG_DE_ALPHA_MODES_ReadMask                                 0xFF003311
#define GCREG_DE_ALPHA_MODES_WriteMask                                0xFF003311
#define GCREG_DE_ALPHA_MODES_ResetValue                               0x00000000

#define GCREG_DE_ALPHA_MODES_SRC_ALPHA_MODE                                0 : 0
#define GCREG_DE_ALPHA_MODES_SRC_ALPHA_MODE_End                                0
#define GCREG_DE_ALPHA_MODES_SRC_ALPHA_MODE_Start                              0
#define GCREG_DE_ALPHA_MODES_SRC_ALPHA_MODE_Type                             U01
#define   GCREG_DE_ALPHA_MODES_SRC_ALPHA_MODE_NORMAL                         0x0
#define   GCREG_DE_ALPHA_MODES_SRC_ALPHA_MODE_INVERSED                       0x1

#define GCREG_DE_ALPHA_MODES_DST_ALPHA_MODE                                4 : 4
#define GCREG_DE_ALPHA_MODES_DST_ALPHA_MODE_End                                4
#define GCREG_DE_ALPHA_MODES_DST_ALPHA_MODE_Start                              4
#define GCREG_DE_ALPHA_MODES_DST_ALPHA_MODE_Type                             U01
#define   GCREG_DE_ALPHA_MODES_DST_ALPHA_MODE_NORMAL                         0x0
#define   GCREG_DE_ALPHA_MODES_DST_ALPHA_MODE_INVERSED                       0x1

#define GCREG_DE_ALPHA_MODES_GLOBAL_SRC_ALPHA_MODE                         9 : 8
#define GCREG_DE_ALPHA_MODES_GLOBAL_SRC_ALPHA_MODE_End                         9
#define GCREG_DE_ALPHA_MODES_GLOBAL_SRC_ALPHA_MODE_Start                       8
#define GCREG_DE_ALPHA_MODES_GLOBAL_SRC_ALPHA_MODE_Type                      U02
#define   GCREG_DE_ALPHA_MODES_GLOBAL_SRC_ALPHA_MODE_NORMAL                  0x0
#define   GCREG_DE_ALPHA_MODES_GLOBAL_SRC_ALPHA_MODE_GLOBAL                  0x1
#define   GCREG_DE_ALPHA_MODES_GLOBAL_SRC_ALPHA_MODE_SCALED                  0x2

#define GCREG_DE_ALPHA_MODES_GLOBAL_DST_ALPHA_MODE                       13 : 12
#define GCREG_DE_ALPHA_MODES_GLOBAL_DST_ALPHA_MODE_End                        13
#define GCREG_DE_ALPHA_MODES_GLOBAL_DST_ALPHA_MODE_Start                      12
#define GCREG_DE_ALPHA_MODES_GLOBAL_DST_ALPHA_MODE_Type                      U02
#define   GCREG_DE_ALPHA_MODES_GLOBAL_DST_ALPHA_MODE_NORMAL                  0x0
#define   GCREG_DE_ALPHA_MODES_GLOBAL_DST_ALPHA_MODE_GLOBAL                  0x1
#define   GCREG_DE_ALPHA_MODES_GLOBAL_DST_ALPHA_MODE_SCALED                  0x2

#define GCREG_DE_ALPHA_MODES_SRC_BLENDING_MODE                           26 : 24
#define GCREG_DE_ALPHA_MODES_SRC_BLENDING_MODE_End                            26
#define GCREG_DE_ALPHA_MODES_SRC_BLENDING_MODE_Start                          24
#define GCREG_DE_ALPHA_MODES_SRC_BLENDING_MODE_Type                          U03
#define   GCREG_DE_ALPHA_MODES_SRC_BLENDING_MODE_ZERO                        0x0
#define   GCREG_DE_ALPHA_MODES_SRC_BLENDING_MODE_ONE                         0x1
#define   GCREG_DE_ALPHA_MODES_SRC_BLENDING_MODE_NORMAL                      0x2
#define   GCREG_DE_ALPHA_MODES_SRC_BLENDING_MODE_INVERSED                    0x3
#define   GCREG_DE_ALPHA_MODES_SRC_BLENDING_MODE_COLOR                       0x4
#define   GCREG_DE_ALPHA_MODES_SRC_BLENDING_MODE_COLOR_INVERSED              0x5
#define   GCREG_DE_ALPHA_MODES_SRC_BLENDING_MODE_SATURATED_ALPHA             0x6
#define   GCREG_DE_ALPHA_MODES_SRC_BLENDING_MODE_SATURATED_DEST_ALPHA        0x7

/* Src Blending factor is calculate from Src alpha. */
#define GCREG_DE_ALPHA_MODES_SRC_ALPHA_FACTOR                            27 : 27
#define GCREG_DE_ALPHA_MODES_SRC_ALPHA_FACTOR_End                             27
#define GCREG_DE_ALPHA_MODES_SRC_ALPHA_FACTOR_Start                           27
#define GCREG_DE_ALPHA_MODES_SRC_ALPHA_FACTOR_Type                           U01
#define   GCREG_DE_ALPHA_MODES_SRC_ALPHA_FACTOR_DISABLED                     0x0
#define   GCREG_DE_ALPHA_MODES_SRC_ALPHA_FACTOR_ENABLED                      0x1

#define GCREG_DE_ALPHA_MODES_DST_BLENDING_MODE                           30 : 28
#define GCREG_DE_ALPHA_MODES_DST_BLENDING_MODE_End                            30
#define GCREG_DE_ALPHA_MODES_DST_BLENDING_MODE_Start                          28
#define GCREG_DE_ALPHA_MODES_DST_BLENDING_MODE_Type                          U03
#define   GCREG_DE_ALPHA_MODES_DST_BLENDING_MODE_ZERO                        0x0
#define   GCREG_DE_ALPHA_MODES_DST_BLENDING_MODE_ONE                         0x1
#define   GCREG_DE_ALPHA_MODES_DST_BLENDING_MODE_NORMAL                      0x2
#define   GCREG_DE_ALPHA_MODES_DST_BLENDING_MODE_INVERSED                    0x3
#define   GCREG_DE_ALPHA_MODES_DST_BLENDING_MODE_COLOR                       0x4
#define   GCREG_DE_ALPHA_MODES_DST_BLENDING_MODE_COLOR_INVERSED              0x5
#define   GCREG_DE_ALPHA_MODES_DST_BLENDING_MODE_SATURATED_ALPHA             0x6
#define   GCREG_DE_ALPHA_MODES_DST_BLENDING_MODE_SATURATED_DEST_ALPHA        0x7

/* Dst Blending factor is calculate from Dst alpha. */
#define GCREG_DE_ALPHA_MODES_DST_ALPHA_FACTOR                            31 : 31
#define GCREG_DE_ALPHA_MODES_DST_ALPHA_FACTOR_End                             31
#define GCREG_DE_ALPHA_MODES_DST_ALPHA_FACTOR_Start                           31
#define GCREG_DE_ALPHA_MODES_DST_ALPHA_FACTOR_Type                           U01
#define   GCREG_DE_ALPHA_MODES_DST_ALPHA_FACTOR_DISABLED                     0x0
#define   GCREG_DE_ALPHA_MODES_DST_ALPHA_FACTOR_ENABLED                      0x1

/*******************************************************************************
** State gcregDEAddressU
*/

/* 32-bit aligned base address of the source U plane. */

#define gcregDEAddressURegAddrs                                           0x4A28
#define GCREG_DE_ADDRESS_U_MSB                                                15
#define GCREG_DE_ADDRESS_U_LSB                                                 2
#define GCREG_DE_ADDRESS_U_BLK                                                 0
#define GCREG_DE_ADDRESS_U_Count                                               4
#define GCREG_DE_ADDRESS_U_FieldMask                                  0xFFFFFFFF
#define GCREG_DE_ADDRESS_U_ReadMask                                   0xFFFFFFFC
#define GCREG_DE_ADDRESS_U_WriteMask                                  0xFFFFFFFC
#define GCREG_DE_ADDRESS_U_ResetValue                                 0x00000000

#define GCREG_DE_ADDRESS_U_ADDRESS                                        31 : 0
#define GCREG_DE_ADDRESS_U_ADDRESS_End                                        30
#define GCREG_DE_ADDRESS_U_ADDRESS_Start                                       0
#define GCREG_DE_ADDRESS_U_ADDRESS_Type                                      U31

/*******************************************************************************
** State gcregDEStrideU
*/

/* Stride of the source U plane in bytes. */

#define gcregDEStrideURegAddrs                                            0x4A2C
#define GCREG_DE_STRIDE_U_MSB                                                 15
#define GCREG_DE_STRIDE_U_LSB                                                  2
#define GCREG_DE_STRIDE_U_BLK                                                  0
#define GCREG_DE_STRIDE_U_Count                                                4
#define GCREG_DE_STRIDE_U_FieldMask                                   0x0003FFFF
#define GCREG_DE_STRIDE_U_ReadMask                                    0x0003FFFC
#define GCREG_DE_STRIDE_U_WriteMask                                   0x0003FFFC
#define GCREG_DE_STRIDE_U_ResetValue                                  0x00000000

#define GCREG_DE_STRIDE_U_STRIDE                                          17 : 0
#define GCREG_DE_STRIDE_U_STRIDE_End                                          17
#define GCREG_DE_STRIDE_U_STRIDE_Start                                         0
#define GCREG_DE_STRIDE_U_STRIDE_Type                                        U18

/*******************************************************************************
** State gcregDEAddressV
*/

/* 32-bit aligned base address of the source V plane. */

#define gcregDEAddressVRegAddrs                                           0x4A30
#define GCREG_DE_ADDRESS_V_MSB                                                15
#define GCREG_DE_ADDRESS_V_LSB                                                 2
#define GCREG_DE_ADDRESS_V_BLK                                                 0
#define GCREG_DE_ADDRESS_V_Count                                               4
#define GCREG_DE_ADDRESS_V_FieldMask                                  0xFFFFFFFF
#define GCREG_DE_ADDRESS_V_ReadMask                                   0xFFFFFFFC
#define GCREG_DE_ADDRESS_V_WriteMask                                  0xFFFFFFFC
#define GCREG_DE_ADDRESS_V_ResetValue                                 0x00000000

#define GCREG_DE_ADDRESS_V_ADDRESS                                        31 : 0
#define GCREG_DE_ADDRESS_V_ADDRESS_End                                        30
#define GCREG_DE_ADDRESS_V_ADDRESS_Start                                       0
#define GCREG_DE_ADDRESS_V_ADDRESS_Type                                      U31

/*******************************************************************************
** State gcregDEStrideV
*/

/* Stride of the source V plane in bytes. */

#define gcregDEStrideVRegAddrs                                            0x4A34
#define GCREG_DE_STRIDE_V_MSB                                                 15
#define GCREG_DE_STRIDE_V_LSB                                                  2
#define GCREG_DE_STRIDE_V_BLK                                                  0
#define GCREG_DE_STRIDE_V_Count                                                4
#define GCREG_DE_STRIDE_V_FieldMask                                   0x0003FFFF
#define GCREG_DE_STRIDE_V_ReadMask                                    0x0003FFFC
#define GCREG_DE_STRIDE_V_WriteMask                                   0x0003FFFC
#define GCREG_DE_STRIDE_V_ResetValue                                  0x00000000

#define GCREG_DE_STRIDE_V_STRIDE                                          17 : 0
#define GCREG_DE_STRIDE_V_STRIDE_End                                          17
#define GCREG_DE_STRIDE_V_STRIDE_Start                                         0
#define GCREG_DE_STRIDE_V_STRIDE_Type                                        U18

/*******************************************************************************
** State gcregDESrcRotationHeight
*/

/* 180/270 degree rotation configuration for the Source surface. Height field
** specifies the height of the surface in pixels.
*/

#define gcregDESrcRotationHeightRegAddrs                                  0x4A38
#define GCREG_DE_SRC_ROTATION_HEIGHT_MSB                                      15
#define GCREG_DE_SRC_ROTATION_HEIGHT_LSB                                       2
#define GCREG_DE_SRC_ROTATION_HEIGHT_BLK                                       0
#define GCREG_DE_SRC_ROTATION_HEIGHT_Count                                     4
#define GCREG_DE_SRC_ROTATION_HEIGHT_FieldMask                        0x0000FFFF
#define GCREG_DE_SRC_ROTATION_HEIGHT_ReadMask                         0x0000FFFF
#define GCREG_DE_SRC_ROTATION_HEIGHT_WriteMask                        0x0000FFFF
#define GCREG_DE_SRC_ROTATION_HEIGHT_ResetValue                       0x00000000

#define GCREG_DE_SRC_ROTATION_HEIGHT_HEIGHT                               15 : 0
#define GCREG_DE_SRC_ROTATION_HEIGHT_HEIGHT_End                               15
#define GCREG_DE_SRC_ROTATION_HEIGHT_HEIGHT_Start                              0
#define GCREG_DE_SRC_ROTATION_HEIGHT_HEIGHT_Type                             U16

/*******************************************************************************
** State gcregDERotAngle
*/

/* 0/90/180/270 degree rotation configuration for the Source surface. Height
** field specifies the height of the surface in pixels.
*/

#define gcregDERotAngleRegAddrs                                           0x4A3C
#define GCREG_DE_ROT_ANGLE_MSB                                                15
#define GCREG_DE_ROT_ANGLE_LSB                                                 2
#define GCREG_DE_ROT_ANGLE_BLK                                                 0
#define GCREG_DE_ROT_ANGLE_Count                                               4
#define GCREG_DE_ROT_ANGLE_FieldMask                                  0x000BB33F
#define GCREG_DE_ROT_ANGLE_ReadMask                                   0x000BB33F
#define GCREG_DE_ROT_ANGLE_WriteMask                                  0x000BB33F
#define GCREG_DE_ROT_ANGLE_ResetValue                                 0x00000000

#define GCREG_DE_ROT_ANGLE_SRC                                             2 : 0
#define GCREG_DE_ROT_ANGLE_SRC_End                                             2
#define GCREG_DE_ROT_ANGLE_SRC_Start                                           0
#define GCREG_DE_ROT_ANGLE_SRC_Type                                          U03
#define   GCREG_DE_ROT_ANGLE_SRC_ROT0                                        0x0
#define   GCREG_DE_ROT_ANGLE_SRC_FLIP_X                                      0x1
#define   GCREG_DE_ROT_ANGLE_SRC_FLIP_Y                                      0x2
#define   GCREG_DE_ROT_ANGLE_SRC_ROT90                                       0x4
#define   GCREG_DE_ROT_ANGLE_SRC_ROT180                                      0x5
#define   GCREG_DE_ROT_ANGLE_SRC_ROT270                                      0x6

#define GCREG_DE_ROT_ANGLE_DST                                             5 : 3
#define GCREG_DE_ROT_ANGLE_DST_End                                             5
#define GCREG_DE_ROT_ANGLE_DST_Start                                           3
#define GCREG_DE_ROT_ANGLE_DST_Type                                          U03
#define   GCREG_DE_ROT_ANGLE_DST_ROT0                                        0x0
#define   GCREG_DE_ROT_ANGLE_DST_FLIP_X                                      0x1
#define   GCREG_DE_ROT_ANGLE_DST_FLIP_Y                                      0x2
#define   GCREG_DE_ROT_ANGLE_DST_ROT90                                       0x4
#define   GCREG_DE_ROT_ANGLE_DST_ROT180                                      0x5
#define   GCREG_DE_ROT_ANGLE_DST_ROT270                                      0x6

#define GCREG_DE_ROT_ANGLE_MASK_SRC                                        8 : 8
#define GCREG_DE_ROT_ANGLE_MASK_SRC_End                                        8
#define GCREG_DE_ROT_ANGLE_MASK_SRC_Start                                      8
#define GCREG_DE_ROT_ANGLE_MASK_SRC_Type                                     U01
#define   GCREG_DE_ROT_ANGLE_MASK_SRC_ENABLED                                0x0
#define   GCREG_DE_ROT_ANGLE_MASK_SRC_MASKED                                 0x1

#define GCREG_DE_ROT_ANGLE_MASK_DST                                        9 : 9
#define GCREG_DE_ROT_ANGLE_MASK_DST_End                                        9
#define GCREG_DE_ROT_ANGLE_MASK_DST_Start                                      9
#define GCREG_DE_ROT_ANGLE_MASK_DST_Type                                     U01
#define   GCREG_DE_ROT_ANGLE_MASK_DST_ENABLED                                0x0
#define   GCREG_DE_ROT_ANGLE_MASK_DST_MASKED                                 0x1

#define GCREG_DE_ROT_ANGLE_SRC_MIRROR                                    13 : 12
#define GCREG_DE_ROT_ANGLE_SRC_MIRROR_End                                     13
#define GCREG_DE_ROT_ANGLE_SRC_MIRROR_Start                                   12
#define GCREG_DE_ROT_ANGLE_SRC_MIRROR_Type                                   U02
#define   GCREG_DE_ROT_ANGLE_SRC_MIRROR_NONE                                 0x0
#define   GCREG_DE_ROT_ANGLE_SRC_MIRROR_MIRROR_X                             0x1
#define   GCREG_DE_ROT_ANGLE_SRC_MIRROR_MIRROR_Y                             0x2
#define   GCREG_DE_ROT_ANGLE_SRC_MIRROR_MIRROR_XY                            0x3

#define GCREG_DE_ROT_ANGLE_MASK_SRC_MIRROR                               15 : 15
#define GCREG_DE_ROT_ANGLE_MASK_SRC_MIRROR_End                                15
#define GCREG_DE_ROT_ANGLE_MASK_SRC_MIRROR_Start                              15
#define GCREG_DE_ROT_ANGLE_MASK_SRC_MIRROR_Type                              U01
#define   GCREG_DE_ROT_ANGLE_MASK_SRC_MIRROR_ENABLED                         0x0
#define   GCREG_DE_ROT_ANGLE_MASK_SRC_MIRROR_MASKED                          0x1

#define GCREG_DE_ROT_ANGLE_DST_MIRROR                                    17 : 16
#define GCREG_DE_ROT_ANGLE_DST_MIRROR_End                                     17
#define GCREG_DE_ROT_ANGLE_DST_MIRROR_Start                                   16
#define GCREG_DE_ROT_ANGLE_DST_MIRROR_Type                                   U02
#define   GCREG_DE_ROT_ANGLE_DST_MIRROR_NONE                                 0x0
#define   GCREG_DE_ROT_ANGLE_DST_MIRROR_MIRROR_X                             0x1
#define   GCREG_DE_ROT_ANGLE_DST_MIRROR_MIRROR_Y                             0x2
#define   GCREG_DE_ROT_ANGLE_DST_MIRROR_MIRROR_XY                            0x3

#define GCREG_DE_ROT_ANGLE_MASK_DST_MIRROR                               19 : 19
#define GCREG_DE_ROT_ANGLE_MASK_DST_MIRROR_End                                19
#define GCREG_DE_ROT_ANGLE_MASK_DST_MIRROR_Start                              19
#define GCREG_DE_ROT_ANGLE_MASK_DST_MIRROR_Type                              U01
#define   GCREG_DE_ROT_ANGLE_MASK_DST_MIRROR_ENABLED                         0x0
#define   GCREG_DE_ROT_ANGLE_MASK_DST_MIRROR_MASKED                          0x1

/*******************************************************************************
** State gcregDEGlobalSrcColor
*/

/* Defines the global source color and alpha values. */

#define gcregDEGlobalSrcColorRegAddrs                                     0x4A40
#define GCREG_DE_GLOBAL_SRC_COLOR_MSB                                         15
#define GCREG_DE_GLOBAL_SRC_COLOR_LSB                                          2
#define GCREG_DE_GLOBAL_SRC_COLOR_BLK                                          0
#define GCREG_DE_GLOBAL_SRC_COLOR_Count                                        4
#define GCREG_DE_GLOBAL_SRC_COLOR_FieldMask                           0xFFFFFFFF
#define GCREG_DE_GLOBAL_SRC_COLOR_ReadMask                            0xFFFFFFFF
#define GCREG_DE_GLOBAL_SRC_COLOR_WriteMask                           0xFFFFFFFF
#define GCREG_DE_GLOBAL_SRC_COLOR_ResetValue                          0x00000000

#define GCREG_DE_GLOBAL_SRC_COLOR_ALPHA                                  31 : 24
#define GCREG_DE_GLOBAL_SRC_COLOR_ALPHA_End                                   31
#define GCREG_DE_GLOBAL_SRC_COLOR_ALPHA_Start                                 24
#define GCREG_DE_GLOBAL_SRC_COLOR_ALPHA_Type                                 U08

#define GCREG_DE_GLOBAL_SRC_COLOR_RED                                    23 : 16
#define GCREG_DE_GLOBAL_SRC_COLOR_RED_End                                     23
#define GCREG_DE_GLOBAL_SRC_COLOR_RED_Start                                   16
#define GCREG_DE_GLOBAL_SRC_COLOR_RED_Type                                   U08

#define GCREG_DE_GLOBAL_SRC_COLOR_GREEN                                   15 : 8
#define GCREG_DE_GLOBAL_SRC_COLOR_GREEN_End                                   15
#define GCREG_DE_GLOBAL_SRC_COLOR_GREEN_Start                                  8
#define GCREG_DE_GLOBAL_SRC_COLOR_GREEN_Type                                 U08

#define GCREG_DE_GLOBAL_SRC_COLOR_BLUE                                     7 : 0
#define GCREG_DE_GLOBAL_SRC_COLOR_BLUE_End                                     7
#define GCREG_DE_GLOBAL_SRC_COLOR_BLUE_Start                                   0
#define GCREG_DE_GLOBAL_SRC_COLOR_BLUE_Type                                  U08

/*******************************************************************************
** State gcregDEGlobalDestColor
*/

/* Defines the global destination color and alpha values. */

#define gcregDEGlobalDestColorRegAddrs                                    0x4A44
#define GCREG_DE_GLOBAL_DEST_COLOR_MSB                                        15
#define GCREG_DE_GLOBAL_DEST_COLOR_LSB                                         2
#define GCREG_DE_GLOBAL_DEST_COLOR_BLK                                         0
#define GCREG_DE_GLOBAL_DEST_COLOR_Count                                       4
#define GCREG_DE_GLOBAL_DEST_COLOR_FieldMask                          0xFFFFFFFF
#define GCREG_DE_GLOBAL_DEST_COLOR_ReadMask                           0xFFFFFFFF
#define GCREG_DE_GLOBAL_DEST_COLOR_WriteMask                          0xFFFFFFFF
#define GCREG_DE_GLOBAL_DEST_COLOR_ResetValue                         0x00000000

#define GCREG_DE_GLOBAL_DEST_COLOR_ALPHA                                 31 : 24
#define GCREG_DE_GLOBAL_DEST_COLOR_ALPHA_End                                  31
#define GCREG_DE_GLOBAL_DEST_COLOR_ALPHA_Start                                24
#define GCREG_DE_GLOBAL_DEST_COLOR_ALPHA_Type                                U08

#define GCREG_DE_GLOBAL_DEST_COLOR_RED                                   23 : 16
#define GCREG_DE_GLOBAL_DEST_COLOR_RED_End                                    23
#define GCREG_DE_GLOBAL_DEST_COLOR_RED_Start                                  16
#define GCREG_DE_GLOBAL_DEST_COLOR_RED_Type                                  U08

#define GCREG_DE_GLOBAL_DEST_COLOR_GREEN                                  15 : 8
#define GCREG_DE_GLOBAL_DEST_COLOR_GREEN_End                                  15
#define GCREG_DE_GLOBAL_DEST_COLOR_GREEN_Start                                 8
#define GCREG_DE_GLOBAL_DEST_COLOR_GREEN_Type                                U08

#define GCREG_DE_GLOBAL_DEST_COLOR_BLUE                                    7 : 0
#define GCREG_DE_GLOBAL_DEST_COLOR_BLUE_End                                    7
#define GCREG_DE_GLOBAL_DEST_COLOR_BLUE_Start                                  0
#define GCREG_DE_GLOBAL_DEST_COLOR_BLUE_Type                                 U08

/*******************************************************************************
** State gcregDEColorMultiplyModes
*/

/* Color modes to multiply Source or Destination pixel color by alpha
** channel. Alpha can be from global color source or current pixel.
*/

#define gcregDEColorMultiplyModesRegAddrs                                 0x4A48
#define GCREG_DE_COLOR_MULTIPLY_MODES_MSB                                     15
#define GCREG_DE_COLOR_MULTIPLY_MODES_LSB                                      2
#define GCREG_DE_COLOR_MULTIPLY_MODES_BLK                                      0
#define GCREG_DE_COLOR_MULTIPLY_MODES_Count                                    4
#define GCREG_DE_COLOR_MULTIPLY_MODES_FieldMask                       0x00100311
#define GCREG_DE_COLOR_MULTIPLY_MODES_ReadMask                        0x00100311
#define GCREG_DE_COLOR_MULTIPLY_MODES_WriteMask                       0x00100311
#define GCREG_DE_COLOR_MULTIPLY_MODES_ResetValue                      0x00000000

#define GCREG_DE_COLOR_MULTIPLY_MODES_SRC_PREMULTIPLY                      0 : 0
#define GCREG_DE_COLOR_MULTIPLY_MODES_SRC_PREMULTIPLY_End                      0
#define GCREG_DE_COLOR_MULTIPLY_MODES_SRC_PREMULTIPLY_Start                    0
#define GCREG_DE_COLOR_MULTIPLY_MODES_SRC_PREMULTIPLY_Type                   U01
#define   GCREG_DE_COLOR_MULTIPLY_MODES_SRC_PREMULTIPLY_DISABLE              0x0
#define   GCREG_DE_COLOR_MULTIPLY_MODES_SRC_PREMULTIPLY_ENABLE               0x1

#define GCREG_DE_COLOR_MULTIPLY_MODES_DST_PREMULTIPLY                      4 : 4
#define GCREG_DE_COLOR_MULTIPLY_MODES_DST_PREMULTIPLY_End                      4
#define GCREG_DE_COLOR_MULTIPLY_MODES_DST_PREMULTIPLY_Start                    4
#define GCREG_DE_COLOR_MULTIPLY_MODES_DST_PREMULTIPLY_Type                   U01
#define   GCREG_DE_COLOR_MULTIPLY_MODES_DST_PREMULTIPLY_DISABLE              0x0
#define   GCREG_DE_COLOR_MULTIPLY_MODES_DST_PREMULTIPLY_ENABLE               0x1

#define GCREG_DE_COLOR_MULTIPLY_MODES_SRC_GLOBAL_PREMULTIPLY               9 : 8
#define GCREG_DE_COLOR_MULTIPLY_MODES_SRC_GLOBAL_PREMULTIPLY_End               9
#define GCREG_DE_COLOR_MULTIPLY_MODES_SRC_GLOBAL_PREMULTIPLY_Start             8
#define GCREG_DE_COLOR_MULTIPLY_MODES_SRC_GLOBAL_PREMULTIPLY_Type            U02
#define   GCREG_DE_COLOR_MULTIPLY_MODES_SRC_GLOBAL_PREMULTIPLY_DISABLE       0x0
#define   GCREG_DE_COLOR_MULTIPLY_MODES_SRC_GLOBAL_PREMULTIPLY_ALPHA         0x1
#define   GCREG_DE_COLOR_MULTIPLY_MODES_SRC_GLOBAL_PREMULTIPLY_COLOR         0x2

#define GCREG_DE_COLOR_MULTIPLY_MODES_DST_DEMULTIPLY                     20 : 20
#define GCREG_DE_COLOR_MULTIPLY_MODES_DST_DEMULTIPLY_End                      20
#define GCREG_DE_COLOR_MULTIPLY_MODES_DST_DEMULTIPLY_Start                    20
#define GCREG_DE_COLOR_MULTIPLY_MODES_DST_DEMULTIPLY_Type                    U01
#define   GCREG_DE_COLOR_MULTIPLY_MODES_DST_DEMULTIPLY_DISABLE               0x0
#define   GCREG_DE_COLOR_MULTIPLY_MODES_DST_DEMULTIPLY_ENABLE                0x1

/*******************************************************************************
** State gcregPETransparency
*/

#define gcregPETransparencyRegAddrs                                       0x4A4C
#define GCREG_PE_TRANSPARENCY_MSB                                             15
#define GCREG_PE_TRANSPARENCY_LSB                                              2
#define GCREG_PE_TRANSPARENCY_BLK                                              0
#define GCREG_PE_TRANSPARENCY_Count                                            4
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

/* DFB Color Key. */
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

#define gcregPEControlRegAddrs                                            0x4A50
#define GCREG_PE_CONTROL_MSB                                                  15
#define GCREG_PE_CONTROL_LSB                                                   2
#define GCREG_PE_CONTROL_BLK                                                   0
#define GCREG_PE_CONTROL_Count                                                 4
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
** State gcregDESrcColorKeyHigh
*/

/* Defines the source transparency color in source format. */

#define gcregDESrcColorKeyHighRegAddrs                                    0x4A54
#define GCREG_DE_SRC_COLOR_KEY_HIGH_MSB                                       15
#define GCREG_DE_SRC_COLOR_KEY_HIGH_LSB                                        2
#define GCREG_DE_SRC_COLOR_KEY_HIGH_BLK                                        0
#define GCREG_DE_SRC_COLOR_KEY_HIGH_Count                                      4
#define GCREG_DE_SRC_COLOR_KEY_HIGH_FieldMask                         0xFFFFFFFF
#define GCREG_DE_SRC_COLOR_KEY_HIGH_ReadMask                          0xFFFFFFFF
#define GCREG_DE_SRC_COLOR_KEY_HIGH_WriteMask                         0xFFFFFFFF
#define GCREG_DE_SRC_COLOR_KEY_HIGH_ResetValue                        0x00000000

#define GCREG_DE_SRC_COLOR_KEY_HIGH_ALPHA                                31 : 24
#define GCREG_DE_SRC_COLOR_KEY_HIGH_ALPHA_End                                 31
#define GCREG_DE_SRC_COLOR_KEY_HIGH_ALPHA_Start                               24
#define GCREG_DE_SRC_COLOR_KEY_HIGH_ALPHA_Type                               U08

#define GCREG_DE_SRC_COLOR_KEY_HIGH_RED                                  23 : 16
#define GCREG_DE_SRC_COLOR_KEY_HIGH_RED_End                                   23
#define GCREG_DE_SRC_COLOR_KEY_HIGH_RED_Start                                 16
#define GCREG_DE_SRC_COLOR_KEY_HIGH_RED_Type                                 U08

#define GCREG_DE_SRC_COLOR_KEY_HIGH_GREEN                                 15 : 8
#define GCREG_DE_SRC_COLOR_KEY_HIGH_GREEN_End                                 15
#define GCREG_DE_SRC_COLOR_KEY_HIGH_GREEN_Start                                8
#define GCREG_DE_SRC_COLOR_KEY_HIGH_GREEN_Type                               U08

#define GCREG_DE_SRC_COLOR_KEY_HIGH_BLUE                                   7 : 0
#define GCREG_DE_SRC_COLOR_KEY_HIGH_BLUE_End                                   7
#define GCREG_DE_SRC_COLOR_KEY_HIGH_BLUE_Start                                 0
#define GCREG_DE_SRC_COLOR_KEY_HIGH_BLUE_Type                                U08

/*******************************************************************************
** State gcregDESrcExConfig
*/

#define gcregDESrcExConfigRegAddrs                                        0x4A58
#define GCREG_DE_SRC_EX_CONFIG_MSB                                            15
#define GCREG_DE_SRC_EX_CONFIG_LSB                                             2
#define GCREG_DE_SRC_EX_CONFIG_BLK                                             0
#define GCREG_DE_SRC_EX_CONFIG_Count                                           4
#define GCREG_DE_SRC_EX_CONFIG_FieldMask                              0x00000109
#define GCREG_DE_SRC_EX_CONFIG_ReadMask                               0x00000109
#define GCREG_DE_SRC_EX_CONFIG_WriteMask                              0x00000109
#define GCREG_DE_SRC_EX_CONFIG_ResetValue                             0x00000000

/* Source multi tiled address computation control. */
#define GCREG_DE_SRC_EX_CONFIG_MULTI_TILED                                 0 : 0
#define GCREG_DE_SRC_EX_CONFIG_MULTI_TILED_End                                 0
#define GCREG_DE_SRC_EX_CONFIG_MULTI_TILED_Start                               0
#define GCREG_DE_SRC_EX_CONFIG_MULTI_TILED_Type                              U01
#define   GCREG_DE_SRC_EX_CONFIG_MULTI_TILED_DISABLED                        0x0
#define   GCREG_DE_SRC_EX_CONFIG_MULTI_TILED_ENABLED                         0x1

/* Source super tiled address computation control. */
#define GCREG_DE_SRC_EX_CONFIG_SUPER_TILED                                 3 : 3
#define GCREG_DE_SRC_EX_CONFIG_SUPER_TILED_End                                 3
#define GCREG_DE_SRC_EX_CONFIG_SUPER_TILED_Start                               3
#define GCREG_DE_SRC_EX_CONFIG_SUPER_TILED_Type                              U01
#define   GCREG_DE_SRC_EX_CONFIG_SUPER_TILED_DISABLED                        0x0
#define   GCREG_DE_SRC_EX_CONFIG_SUPER_TILED_ENABLED                         0x1

/* Source super tiled address computation control. */
#define GCREG_DE_SRC_EX_CONFIG_MINOR_TILED                                 8 : 8
#define GCREG_DE_SRC_EX_CONFIG_MINOR_TILED_End                                 8
#define GCREG_DE_SRC_EX_CONFIG_MINOR_TILED_Start                               8
#define GCREG_DE_SRC_EX_CONFIG_MINOR_TILED_Type                              U01
#define   GCREG_DE_SRC_EX_CONFIG_MINOR_TILED_DISABLED                        0x0
#define   GCREG_DE_SRC_EX_CONFIG_MINOR_TILED_ENABLED                         0x1

/*******************************************************************************
** State gcregDESrcExAddress
*/

/* 32-bit aligned base address of the source extra surface. */

#define gcregDESrcExAddressRegAddrs                                       0x4A5C
#define GCREG_DE_SRC_EX_ADDRESS_MSB                                           15
#define GCREG_DE_SRC_EX_ADDRESS_LSB                                            2
#define GCREG_DE_SRC_EX_ADDRESS_BLK                                            0
#define GCREG_DE_SRC_EX_ADDRESS_Count                                          4
#define GCREG_DE_SRC_EX_ADDRESS_FieldMask                             0xFFFFFFFF
#define GCREG_DE_SRC_EX_ADDRESS_ReadMask                              0xFFFFFFFC
#define GCREG_DE_SRC_EX_ADDRESS_WriteMask                             0xFFFFFFFC
#define GCREG_DE_SRC_EX_ADDRESS_ResetValue                            0x00000000

#define GCREG_DE_SRC_EX_ADDRESS_ADDRESS                                   31 : 0
#define GCREG_DE_SRC_EX_ADDRESS_ADDRESS_End                                   30
#define GCREG_DE_SRC_EX_ADDRESS_ADDRESS_Start                                  0
#define GCREG_DE_SRC_EX_ADDRESS_ADDRESS_Type                                 U31

/*******************************************************************************
** State gcregDESrcAddressEx
*/

/* 32-bit aligned base address of the source surface. */

#define gcregDESrcAddressExRegAddrs                                       0x4A80
#define GCREG_DE_SRC_ADDRESS_EX_MSB                                           15
#define GCREG_DE_SRC_ADDRESS_EX_LSB                                            3
#define GCREG_DE_SRC_ADDRESS_EX_BLK                                            0
#define GCREG_DE_SRC_ADDRESS_EX_Count                                          8
#define GCREG_DE_SRC_ADDRESS_EX_FieldMask                             0xFFFFFFFF
#define GCREG_DE_SRC_ADDRESS_EX_ReadMask                              0xFFFFFFFC
#define GCREG_DE_SRC_ADDRESS_EX_WriteMask                             0xFFFFFFFC
#define GCREG_DE_SRC_ADDRESS_EX_ResetValue                            0x00000000

#define GCREG_DE_SRC_ADDRESS_EX_ADDRESS                                   31 : 0
#define GCREG_DE_SRC_ADDRESS_EX_ADDRESS_End                                   30
#define GCREG_DE_SRC_ADDRESS_EX_ADDRESS_Start                                  0
#define GCREG_DE_SRC_ADDRESS_EX_ADDRESS_Type                                 U31

/*******************************************************************************
** State gcregDESrcStrideEx
*/

/* Stride of the source surface in bytes. To calculate the stride multiply
** the surface width in pixels by the number of  bytes per pixel.
*/

#define gcregDESrcStrideExRegAddrs                                        0x4A88
#define GCREG_DE_SRC_STRIDE_EX_MSB                                            15
#define GCREG_DE_SRC_STRIDE_EX_LSB                                             3
#define GCREG_DE_SRC_STRIDE_EX_BLK                                             0
#define GCREG_DE_SRC_STRIDE_EX_Count                                           8
#define GCREG_DE_SRC_STRIDE_EX_FieldMask                              0x0003FFFF
#define GCREG_DE_SRC_STRIDE_EX_ReadMask                               0x0003FFFC
#define GCREG_DE_SRC_STRIDE_EX_WriteMask                              0x0003FFFC
#define GCREG_DE_SRC_STRIDE_EX_ResetValue                             0x00000000

#define GCREG_DE_SRC_STRIDE_EX_STRIDE                                     17 : 0
#define GCREG_DE_SRC_STRIDE_EX_STRIDE_End                                     17
#define GCREG_DE_SRC_STRIDE_EX_STRIDE_Start                                    0
#define GCREG_DE_SRC_STRIDE_EX_STRIDE_Type                                   U18

/*******************************************************************************
** State gcregDESrcRotationConfigEx
*/

/* 90 degree rotation configuration for the source surface. Width field
** specifies the width of the surface in pixels.
*/

#define gcregDESrcRotationConfigExRegAddrs                                0x4A90
#define GCREG_DE_SRC_ROTATION_CONFIG_EX_MSB                                   15
#define GCREG_DE_SRC_ROTATION_CONFIG_EX_LSB                                    3
#define GCREG_DE_SRC_ROTATION_CONFIG_EX_BLK                                    0
#define GCREG_DE_SRC_ROTATION_CONFIG_EX_Count                                  8
#define GCREG_DE_SRC_ROTATION_CONFIG_EX_FieldMask                     0x0001FFFF
#define GCREG_DE_SRC_ROTATION_CONFIG_EX_ReadMask                      0x0001FFFF
#define GCREG_DE_SRC_ROTATION_CONFIG_EX_WriteMask                     0x0001FFFF
#define GCREG_DE_SRC_ROTATION_CONFIG_EX_ResetValue                    0x00000000

#define GCREG_DE_SRC_ROTATION_CONFIG_EX_WIDTH                             15 : 0
#define GCREG_DE_SRC_ROTATION_CONFIG_EX_WIDTH_End                             15
#define GCREG_DE_SRC_ROTATION_CONFIG_EX_WIDTH_Start                            0
#define GCREG_DE_SRC_ROTATION_CONFIG_EX_WIDTH_Type                           U16

#define GCREG_DE_SRC_ROTATION_CONFIG_EX_ROTATION                         16 : 16
#define GCREG_DE_SRC_ROTATION_CONFIG_EX_ROTATION_End                          16
#define GCREG_DE_SRC_ROTATION_CONFIG_EX_ROTATION_Start                        16
#define GCREG_DE_SRC_ROTATION_CONFIG_EX_ROTATION_Type                        U01
#define   GCREG_DE_SRC_ROTATION_CONFIG_EX_ROTATION_NORMAL                    0x0
#define   GCREG_DE_SRC_ROTATION_CONFIG_EX_ROTATION_ROTATED                   0x1

/*******************************************************************************
** State gcregDESrcConfigEx
*/

/* Source surface configuration register. */

#define gcregDESrcConfigExRegAddrs                                        0x4A98
#define GCREG_DE_SRC_CONFIG_EX_MSB                                            15
#define GCREG_DE_SRC_CONFIG_EX_LSB                                             3
#define GCREG_DE_SRC_CONFIG_EX_BLK                                             0
#define GCREG_DE_SRC_CONFIG_EX_Count                                           8
#define GCREG_DE_SRC_CONFIG_EX_FieldMask                              0xDF30B1C0
#define GCREG_DE_SRC_CONFIG_EX_ReadMask                               0xDF30B1C0
#define GCREG_DE_SRC_CONFIG_EX_WriteMask                              0xDF30B1C0
#define GCREG_DE_SRC_CONFIG_EX_ResetValue                             0x00000000

/* Control source endianess. */
#define GCREG_DE_SRC_CONFIG_EX_ENDIAN_CONTROL                            31 : 30
#define GCREG_DE_SRC_CONFIG_EX_ENDIAN_CONTROL_End                             31
#define GCREG_DE_SRC_CONFIG_EX_ENDIAN_CONTROL_Start                           30
#define GCREG_DE_SRC_CONFIG_EX_ENDIAN_CONTROL_Type                           U02
#define   GCREG_DE_SRC_CONFIG_EX_ENDIAN_CONTROL_NO_SWAP                      0x0
#define   GCREG_DE_SRC_CONFIG_EX_ENDIAN_CONTROL_SWAP_WORD                    0x1
#define   GCREG_DE_SRC_CONFIG_EX_ENDIAN_CONTROL_SWAP_DWORD                   0x2

/* Defines the pixel format of the source surface. */
#define GCREG_DE_SRC_CONFIG_EX_SOURCE_FORMAT                             28 : 24
#define GCREG_DE_SRC_CONFIG_EX_SOURCE_FORMAT_End                              28
#define GCREG_DE_SRC_CONFIG_EX_SOURCE_FORMAT_Start                            24
#define GCREG_DE_SRC_CONFIG_EX_SOURCE_FORMAT_Type                            U05
#define   GCREG_DE_SRC_CONFIG_EX_SOURCE_FORMAT_X4R4G4B4                     0x00
#define   GCREG_DE_SRC_CONFIG_EX_SOURCE_FORMAT_A4R4G4B4                     0x01
#define   GCREG_DE_SRC_CONFIG_EX_SOURCE_FORMAT_X1R5G5B5                     0x02
#define   GCREG_DE_SRC_CONFIG_EX_SOURCE_FORMAT_A1R5G5B5                     0x03
#define   GCREG_DE_SRC_CONFIG_EX_SOURCE_FORMAT_R5G6B5                       0x04
#define   GCREG_DE_SRC_CONFIG_EX_SOURCE_FORMAT_X8R8G8B8                     0x05
#define   GCREG_DE_SRC_CONFIG_EX_SOURCE_FORMAT_A8R8G8B8                     0x06
#define   GCREG_DE_SRC_CONFIG_EX_SOURCE_FORMAT_YUY2                         0x07
#define   GCREG_DE_SRC_CONFIG_EX_SOURCE_FORMAT_UYVY                         0x08
#define   GCREG_DE_SRC_CONFIG_EX_SOURCE_FORMAT_INDEX8                       0x09
#define   GCREG_DE_SRC_CONFIG_EX_SOURCE_FORMAT_MONOCHROME                   0x0A
#define   GCREG_DE_SRC_CONFIG_EX_SOURCE_FORMAT_YV12                         0x0F
#define   GCREG_DE_SRC_CONFIG_EX_SOURCE_FORMAT_A8                           0x10
#define   GCREG_DE_SRC_CONFIG_EX_SOURCE_FORMAT_NV12                         0x11
#define   GCREG_DE_SRC_CONFIG_EX_SOURCE_FORMAT_NV16                         0x12
#define   GCREG_DE_SRC_CONFIG_EX_SOURCE_FORMAT_RG16                         0x13

/* Color channel swizzles. */
#define GCREG_DE_SRC_CONFIG_EX_SWIZZLE                                   21 : 20
#define GCREG_DE_SRC_CONFIG_EX_SWIZZLE_End                                    21
#define GCREG_DE_SRC_CONFIG_EX_SWIZZLE_Start                                  20
#define GCREG_DE_SRC_CONFIG_EX_SWIZZLE_Type                                  U02
#define   GCREG_DE_SRC_CONFIG_EX_SWIZZLE_ARGB                                0x0
#define   GCREG_DE_SRC_CONFIG_EX_SWIZZLE_RGBA                                0x1
#define   GCREG_DE_SRC_CONFIG_EX_SWIZZLE_ABGR                                0x2
#define   GCREG_DE_SRC_CONFIG_EX_SWIZZLE_BGRA                                0x3

/* Mono expansion: if 0, transparency color will be 0, otherwise transparency **
** color will be 1.                                                           */
#define GCREG_DE_SRC_CONFIG_EX_MONO_TRANSPARENCY                         15 : 15
#define GCREG_DE_SRC_CONFIG_EX_MONO_TRANSPARENCY_End                          15
#define GCREG_DE_SRC_CONFIG_EX_MONO_TRANSPARENCY_Start                        15
#define GCREG_DE_SRC_CONFIG_EX_MONO_TRANSPARENCY_Type                        U01
#define   GCREG_DE_SRC_CONFIG_EX_MONO_TRANSPARENCY_BACKGROUND                0x0
#define   GCREG_DE_SRC_CONFIG_EX_MONO_TRANSPARENCY_FOREGROUND                0x1

/* Mono expansion or masked blit: stream packing in pixels. Determines how    **
** many horizontal pixels are there per each 32-bit chunk. For example, if    **
** set to Packed8, each 32-bit chunk is 8-pixel wide, which also means that   **
** it defines 4 vertical lines of pixels.                                     */
#define GCREG_DE_SRC_CONFIG_EX_PACK                                      13 : 12
#define GCREG_DE_SRC_CONFIG_EX_PACK_End                                       13
#define GCREG_DE_SRC_CONFIG_EX_PACK_Start                                     12
#define GCREG_DE_SRC_CONFIG_EX_PACK_Type                                     U02
#define   GCREG_DE_SRC_CONFIG_EX_PACK_PACKED8                                0x0
#define   GCREG_DE_SRC_CONFIG_EX_PACK_PACKED16                               0x1
#define   GCREG_DE_SRC_CONFIG_EX_PACK_PACKED32                               0x2
#define   GCREG_DE_SRC_CONFIG_EX_PACK_UNPACKED                               0x3

/* Source data location: set to STREAM for mono expansion blits or masked     **
** blits. For mono expansion blits the complete bitmap comes from the command **
** stream. For masked blits the source data comes from the memory and the     **
** mask from the command stream.                                              */
#define GCREG_DE_SRC_CONFIG_EX_LOCATION                                    8 : 8
#define GCREG_DE_SRC_CONFIG_EX_LOCATION_End                                    8
#define GCREG_DE_SRC_CONFIG_EX_LOCATION_Start                                  8
#define GCREG_DE_SRC_CONFIG_EX_LOCATION_Type                                 U01
#define   GCREG_DE_SRC_CONFIG_EX_LOCATION_MEMORY                             0x0
#define   GCREG_DE_SRC_CONFIG_EX_LOCATION_STREAM                             0x1

/* Source linear/tiled address computation control. */
#define GCREG_DE_SRC_CONFIG_EX_TILED                                       7 : 7
#define GCREG_DE_SRC_CONFIG_EX_TILED_End                                       7
#define GCREG_DE_SRC_CONFIG_EX_TILED_Start                                     7
#define GCREG_DE_SRC_CONFIG_EX_TILED_Type                                    U01
#define   GCREG_DE_SRC_CONFIG_EX_TILED_DISABLED                              0x0
#define   GCREG_DE_SRC_CONFIG_EX_TILED_ENABLED                               0x1

/* If set to ABSOLUTE, the source coordinates are treated as absolute         **
** coordinates inside the source surface. If set to RELATIVE, the source      **
** coordinates are treated as the offsets from the destination coordinates    **
** with the source size equal to the size of the destination.                 */
#define GCREG_DE_SRC_CONFIG_EX_SRC_RELATIVE                                6 : 6
#define GCREG_DE_SRC_CONFIG_EX_SRC_RELATIVE_End                                6
#define GCREG_DE_SRC_CONFIG_EX_SRC_RELATIVE_Start                              6
#define GCREG_DE_SRC_CONFIG_EX_SRC_RELATIVE_Type                             U01
#define   GCREG_DE_SRC_CONFIG_EX_SRC_RELATIVE_ABSOLUTE                       0x0
#define   GCREG_DE_SRC_CONFIG_EX_SRC_RELATIVE_RELATIVE                       0x1

/*******************************************************************************
** State gcregDESrcOriginEx
*/

/* Absolute or relative (see SRC_RELATIVE field of gcregDESrcConfig register)
** X and Y coordinates in pixels of the top left corner of the source
** rectangle within the source surface.
*/

#define gcregDESrcOriginExRegAddrs                                        0x4AA0
#define GCREG_DE_SRC_ORIGIN_EX_MSB                                            15
#define GCREG_DE_SRC_ORIGIN_EX_LSB                                             3
#define GCREG_DE_SRC_ORIGIN_EX_BLK                                             0
#define GCREG_DE_SRC_ORIGIN_EX_Count                                           8
#define GCREG_DE_SRC_ORIGIN_EX_FieldMask                              0xFFFFFFFF
#define GCREG_DE_SRC_ORIGIN_EX_ReadMask                               0xFFFFFFFF
#define GCREG_DE_SRC_ORIGIN_EX_WriteMask                              0xFFFFFFFF
#define GCREG_DE_SRC_ORIGIN_EX_ResetValue                             0x00000000

#define GCREG_DE_SRC_ORIGIN_EX_Y                                         31 : 16
#define GCREG_DE_SRC_ORIGIN_EX_Y_End                                          31
#define GCREG_DE_SRC_ORIGIN_EX_Y_Start                                        16
#define GCREG_DE_SRC_ORIGIN_EX_Y_Type                                        U16

#define GCREG_DE_SRC_ORIGIN_EX_X                                          15 : 0
#define GCREG_DE_SRC_ORIGIN_EX_X_End                                          15
#define GCREG_DE_SRC_ORIGIN_EX_X_Start                                         0
#define GCREG_DE_SRC_ORIGIN_EX_X_Type                                        U16

/*******************************************************************************
** State gcregDESrcSizeEx
*/

/* Width and height of the source rectangle in pixels. If the source is
** relative (see SRC_RELATIVE field of gcregDESrcConfig register) or a
** regular bitblt is being performed without stretching, this register is
** ignored and the source size is assumed to be the same as the destination.
*/

#define gcregDESrcSizeExRegAddrs                                          0x4AA8
#define GCREG_DE_SRC_SIZE_EX_MSB                                              15
#define GCREG_DE_SRC_SIZE_EX_LSB                                               3
#define GCREG_DE_SRC_SIZE_EX_BLK                                               0
#define GCREG_DE_SRC_SIZE_EX_Count                                             8
#define GCREG_DE_SRC_SIZE_EX_FieldMask                                0xFFFFFFFF
#define GCREG_DE_SRC_SIZE_EX_ReadMask                                 0xFFFFFFFF
#define GCREG_DE_SRC_SIZE_EX_WriteMask                                0xFFFFFFFF
#define GCREG_DE_SRC_SIZE_EX_ResetValue                               0x00000000

#define GCREG_DE_SRC_SIZE_EX_Y                                           31 : 16
#define GCREG_DE_SRC_SIZE_EX_Y_End                                            31
#define GCREG_DE_SRC_SIZE_EX_Y_Start                                          16
#define GCREG_DE_SRC_SIZE_EX_Y_Type                                          U16

#define GCREG_DE_SRC_SIZE_EX_X                                            15 : 0
#define GCREG_DE_SRC_SIZE_EX_X_End                                            15
#define GCREG_DE_SRC_SIZE_EX_X_Start                                           0
#define GCREG_DE_SRC_SIZE_EX_X_Type                                          U16

/*******************************************************************************
** State gcregDESrcColorBgEx
*/

/* Select the color where source becomes transparent. It must be programmed
** in A8R8G8B8 format.
*/

#define gcregDESrcColorBgExRegAddrs                                       0x4AB0
#define GCREG_DE_SRC_COLOR_BG_EX_MSB                                          15
#define GCREG_DE_SRC_COLOR_BG_EX_LSB                                           3
#define GCREG_DE_SRC_COLOR_BG_EX_BLK                                           0
#define GCREG_DE_SRC_COLOR_BG_EX_Count                                         8
#define GCREG_DE_SRC_COLOR_BG_EX_FieldMask                            0xFFFFFFFF
#define GCREG_DE_SRC_COLOR_BG_EX_ReadMask                             0xFFFFFFFF
#define GCREG_DE_SRC_COLOR_BG_EX_WriteMask                            0xFFFFFFFF
#define GCREG_DE_SRC_COLOR_BG_EX_ResetValue                           0x00000000

#define GCREG_DE_SRC_COLOR_BG_EX_ALPHA                                   31 : 24
#define GCREG_DE_SRC_COLOR_BG_EX_ALPHA_End                                    31
#define GCREG_DE_SRC_COLOR_BG_EX_ALPHA_Start                                  24
#define GCREG_DE_SRC_COLOR_BG_EX_ALPHA_Type                                  U08

#define GCREG_DE_SRC_COLOR_BG_EX_RED                                     23 : 16
#define GCREG_DE_SRC_COLOR_BG_EX_RED_End                                      23
#define GCREG_DE_SRC_COLOR_BG_EX_RED_Start                                    16
#define GCREG_DE_SRC_COLOR_BG_EX_RED_Type                                    U08

#define GCREG_DE_SRC_COLOR_BG_EX_GREEN                                    15 : 8
#define GCREG_DE_SRC_COLOR_BG_EX_GREEN_End                                    15
#define GCREG_DE_SRC_COLOR_BG_EX_GREEN_Start                                   8
#define GCREG_DE_SRC_COLOR_BG_EX_GREEN_Type                                  U08

#define GCREG_DE_SRC_COLOR_BG_EX_BLUE                                      7 : 0
#define GCREG_DE_SRC_COLOR_BG_EX_BLUE_End                                      7
#define GCREG_DE_SRC_COLOR_BG_EX_BLUE_Start                                    0
#define GCREG_DE_SRC_COLOR_BG_EX_BLUE_Type                                   U08

/*******************************************************************************
** State gcregDERopEx
*/

/* Raster operation foreground and background codes. Even though ROP is not
** used in CLEAR, HOR_FILTER_BLT, VER_FILTER_BLT and alpha-eanbled BIT_BLTs,
** ROP code still has to be programmed, because the engine makes the decision
** whether source, destination and pattern are involved in the current
** operation and the correct decision is essential for the engine to complete
** the operation as expected.
*/

#define gcregDERopExRegAddrs                                              0x4AB8
#define GCREG_DE_ROP_EX_MSB                                                   15
#define GCREG_DE_ROP_EX_LSB                                                    3
#define GCREG_DE_ROP_EX_BLK                                                    0
#define GCREG_DE_ROP_EX_Count                                                  8
#define GCREG_DE_ROP_EX_FieldMask                                     0x0030FFFF
#define GCREG_DE_ROP_EX_ReadMask                                      0x0030FFFF
#define GCREG_DE_ROP_EX_WriteMask                                     0x0030FFFF
#define GCREG_DE_ROP_EX_ResetValue                                    0x00000000

/* ROP type: ROP2, ROP3 or ROP4 */
#define GCREG_DE_ROP_EX_TYPE                                             21 : 20
#define GCREG_DE_ROP_EX_TYPE_End                                              21
#define GCREG_DE_ROP_EX_TYPE_Start                                            20
#define GCREG_DE_ROP_EX_TYPE_Type                                            U02
#define   GCREG_DE_ROP_EX_TYPE_ROP2_PATTERN                                  0x0
#define   GCREG_DE_ROP_EX_TYPE_ROP2_SOURCE                                   0x1
#define   GCREG_DE_ROP_EX_TYPE_ROP3                                          0x2
#define   GCREG_DE_ROP_EX_TYPE_ROP4                                          0x3

/* Background ROP code is used for transparent pixels. */
#define GCREG_DE_ROP_EX_ROP_BG                                            15 : 8
#define GCREG_DE_ROP_EX_ROP_BG_End                                            15
#define GCREG_DE_ROP_EX_ROP_BG_Start                                           8
#define GCREG_DE_ROP_EX_ROP_BG_Type                                          U08

/* Background ROP code is used for opaque pixels. */
#define GCREG_DE_ROP_EX_ROP_FG                                             7 : 0
#define GCREG_DE_ROP_EX_ROP_FG_End                                             7
#define GCREG_DE_ROP_EX_ROP_FG_Start                                           0
#define GCREG_DE_ROP_EX_ROP_FG_Type                                          U08

/*******************************************************************************
** State gcregDEAlphaControlEx
*/

#define gcregDEAlphaControlExRegAddrs                                     0x4AC0
#define GCREG_DE_ALPHA_CONTROL_EX_MSB                                         15
#define GCREG_DE_ALPHA_CONTROL_EX_LSB                                          3
#define GCREG_DE_ALPHA_CONTROL_EX_BLK                                          0
#define GCREG_DE_ALPHA_CONTROL_EX_Count                                        8
#define GCREG_DE_ALPHA_CONTROL_EX_FieldMask                           0x00000001
#define GCREG_DE_ALPHA_CONTROL_EX_ReadMask                            0x00000001
#define GCREG_DE_ALPHA_CONTROL_EX_WriteMask                           0x00000001
#define GCREG_DE_ALPHA_CONTROL_EX_ResetValue                          0x00000000

#define GCREG_DE_ALPHA_CONTROL_EX_ENABLE                                   0 : 0
#define GCREG_DE_ALPHA_CONTROL_EX_ENABLE_End                                   0
#define GCREG_DE_ALPHA_CONTROL_EX_ENABLE_Start                                 0
#define GCREG_DE_ALPHA_CONTROL_EX_ENABLE_Type                                U01
#define   GCREG_DE_ALPHA_CONTROL_EX_ENABLE_OFF                               0x0
#define   GCREG_DE_ALPHA_CONTROL_EX_ENABLE_ON                                0x1

/*******************************************************************************
** State gcregDEAlphaModesEx
*/

#define gcregDEAlphaModesExRegAddrs                                       0x4AC8
#define GCREG_DE_ALPHA_MODES_EX_MSB                                           15
#define GCREG_DE_ALPHA_MODES_EX_LSB                                            3
#define GCREG_DE_ALPHA_MODES_EX_BLK                                            0
#define GCREG_DE_ALPHA_MODES_EX_Count                                          8
#define GCREG_DE_ALPHA_MODES_EX_FieldMask                             0xFF003311
#define GCREG_DE_ALPHA_MODES_EX_ReadMask                              0xFF003311
#define GCREG_DE_ALPHA_MODES_EX_WriteMask                             0xFF003311
#define GCREG_DE_ALPHA_MODES_EX_ResetValue                            0x00000000

#define GCREG_DE_ALPHA_MODES_EX_SRC_ALPHA_MODE                             0 : 0
#define GCREG_DE_ALPHA_MODES_EX_SRC_ALPHA_MODE_End                             0
#define GCREG_DE_ALPHA_MODES_EX_SRC_ALPHA_MODE_Start                           0
#define GCREG_DE_ALPHA_MODES_EX_SRC_ALPHA_MODE_Type                          U01
#define   GCREG_DE_ALPHA_MODES_EX_SRC_ALPHA_MODE_NORMAL                      0x0
#define   GCREG_DE_ALPHA_MODES_EX_SRC_ALPHA_MODE_INVERSED                    0x1

#define GCREG_DE_ALPHA_MODES_EX_DST_ALPHA_MODE                             4 : 4
#define GCREG_DE_ALPHA_MODES_EX_DST_ALPHA_MODE_End                             4
#define GCREG_DE_ALPHA_MODES_EX_DST_ALPHA_MODE_Start                           4
#define GCREG_DE_ALPHA_MODES_EX_DST_ALPHA_MODE_Type                          U01
#define   GCREG_DE_ALPHA_MODES_EX_DST_ALPHA_MODE_NORMAL                      0x0
#define   GCREG_DE_ALPHA_MODES_EX_DST_ALPHA_MODE_INVERSED                    0x1

#define GCREG_DE_ALPHA_MODES_EX_GLOBAL_SRC_ALPHA_MODE                      9 : 8
#define GCREG_DE_ALPHA_MODES_EX_GLOBAL_SRC_ALPHA_MODE_End                      9
#define GCREG_DE_ALPHA_MODES_EX_GLOBAL_SRC_ALPHA_MODE_Start                    8
#define GCREG_DE_ALPHA_MODES_EX_GLOBAL_SRC_ALPHA_MODE_Type                   U02
#define   GCREG_DE_ALPHA_MODES_EX_GLOBAL_SRC_ALPHA_MODE_NORMAL               0x0
#define   GCREG_DE_ALPHA_MODES_EX_GLOBAL_SRC_ALPHA_MODE_GLOBAL               0x1
#define   GCREG_DE_ALPHA_MODES_EX_GLOBAL_SRC_ALPHA_MODE_SCALED               0x2

#define GCREG_DE_ALPHA_MODES_EX_GLOBAL_DST_ALPHA_MODE                    13 : 12
#define GCREG_DE_ALPHA_MODES_EX_GLOBAL_DST_ALPHA_MODE_End                     13
#define GCREG_DE_ALPHA_MODES_EX_GLOBAL_DST_ALPHA_MODE_Start                   12
#define GCREG_DE_ALPHA_MODES_EX_GLOBAL_DST_ALPHA_MODE_Type                   U02
#define   GCREG_DE_ALPHA_MODES_EX_GLOBAL_DST_ALPHA_MODE_NORMAL               0x0
#define   GCREG_DE_ALPHA_MODES_EX_GLOBAL_DST_ALPHA_MODE_GLOBAL               0x1
#define   GCREG_DE_ALPHA_MODES_EX_GLOBAL_DST_ALPHA_MODE_SCALED               0x2

#define GCREG_DE_ALPHA_MODES_EX_SRC_BLENDING_MODE                        26 : 24
#define GCREG_DE_ALPHA_MODES_EX_SRC_BLENDING_MODE_End                         26
#define GCREG_DE_ALPHA_MODES_EX_SRC_BLENDING_MODE_Start                       24
#define GCREG_DE_ALPHA_MODES_EX_SRC_BLENDING_MODE_Type                       U03
#define   GCREG_DE_ALPHA_MODES_EX_SRC_BLENDING_MODE_ZERO                     0x0
#define   GCREG_DE_ALPHA_MODES_EX_SRC_BLENDING_MODE_ONE                      0x1
#define   GCREG_DE_ALPHA_MODES_EX_SRC_BLENDING_MODE_NORMAL                   0x2
#define   GCREG_DE_ALPHA_MODES_EX_SRC_BLENDING_MODE_INVERSED                 0x3
#define   GCREG_DE_ALPHA_MODES_EX_SRC_BLENDING_MODE_COLOR                    0x4
#define   GCREG_DE_ALPHA_MODES_EX_SRC_BLENDING_MODE_COLOR_INVERSED           0x5
#define   GCREG_DE_ALPHA_MODES_EX_SRC_BLENDING_MODE_SATURATED_ALPHA          0x6
#define   GCREG_DE_ALPHA_MODES_EX_SRC_BLENDING_MODE_SATURATED_DEST_ALPHA     0x7

/* Src Blending factor is calculate from Src alpha. */
#define GCREG_DE_ALPHA_MODES_EX_SRC_ALPHA_FACTOR                         27 : 27
#define GCREG_DE_ALPHA_MODES_EX_SRC_ALPHA_FACTOR_End                          27
#define GCREG_DE_ALPHA_MODES_EX_SRC_ALPHA_FACTOR_Start                        27
#define GCREG_DE_ALPHA_MODES_EX_SRC_ALPHA_FACTOR_Type                        U01
#define   GCREG_DE_ALPHA_MODES_EX_SRC_ALPHA_FACTOR_DISABLED                  0x0
#define   GCREG_DE_ALPHA_MODES_EX_SRC_ALPHA_FACTOR_ENABLED                   0x1

#define GCREG_DE_ALPHA_MODES_EX_DST_BLENDING_MODE                        30 : 28
#define GCREG_DE_ALPHA_MODES_EX_DST_BLENDING_MODE_End                         30
#define GCREG_DE_ALPHA_MODES_EX_DST_BLENDING_MODE_Start                       28
#define GCREG_DE_ALPHA_MODES_EX_DST_BLENDING_MODE_Type                       U03
#define   GCREG_DE_ALPHA_MODES_EX_DST_BLENDING_MODE_ZERO                     0x0
#define   GCREG_DE_ALPHA_MODES_EX_DST_BLENDING_MODE_ONE                      0x1
#define   GCREG_DE_ALPHA_MODES_EX_DST_BLENDING_MODE_NORMAL                   0x2
#define   GCREG_DE_ALPHA_MODES_EX_DST_BLENDING_MODE_INVERSED                 0x3
#define   GCREG_DE_ALPHA_MODES_EX_DST_BLENDING_MODE_COLOR                    0x4
#define   GCREG_DE_ALPHA_MODES_EX_DST_BLENDING_MODE_COLOR_INVERSED           0x5
#define   GCREG_DE_ALPHA_MODES_EX_DST_BLENDING_MODE_SATURATED_ALPHA          0x6
#define   GCREG_DE_ALPHA_MODES_EX_DST_BLENDING_MODE_SATURATED_DEST_ALPHA     0x7

/* Dst Blending factor is calculate from Dst alpha. */
#define GCREG_DE_ALPHA_MODES_EX_DST_ALPHA_FACTOR                         31 : 31
#define GCREG_DE_ALPHA_MODES_EX_DST_ALPHA_FACTOR_End                          31
#define GCREG_DE_ALPHA_MODES_EX_DST_ALPHA_FACTOR_Start                        31
#define GCREG_DE_ALPHA_MODES_EX_DST_ALPHA_FACTOR_Type                        U01
#define   GCREG_DE_ALPHA_MODES_EX_DST_ALPHA_FACTOR_DISABLED                  0x0
#define   GCREG_DE_ALPHA_MODES_EX_DST_ALPHA_FACTOR_ENABLED                   0x1

/*******************************************************************************
** State gcregDEAddressUEx
*/

/* 32-bit aligned base address of the source U plane. */

#define gcregDEAddressUExRegAddrs                                         0x4AD0
#define GCREG_DE_ADDRESS_UEX_MSB                                              15
#define GCREG_DE_ADDRESS_UEX_LSB                                               3
#define GCREG_DE_ADDRESS_UEX_BLK                                               0
#define GCREG_DE_ADDRESS_UEX_Count                                             8
#define GCREG_DE_ADDRESS_UEX_FieldMask                                0xFFFFFFFF
#define GCREG_DE_ADDRESS_UEX_ReadMask                                 0xFFFFFFFC
#define GCREG_DE_ADDRESS_UEX_WriteMask                                0xFFFFFFFC
#define GCREG_DE_ADDRESS_UEX_ResetValue                               0x00000000

#define GCREG_DE_ADDRESS_UEX_ADDRESS                                      31 : 0
#define GCREG_DE_ADDRESS_UEX_ADDRESS_End                                      30
#define GCREG_DE_ADDRESS_UEX_ADDRESS_Start                                     0
#define GCREG_DE_ADDRESS_UEX_ADDRESS_Type                                    U31

/*******************************************************************************
** State gcregDEStrideUEx
*/

/* Stride of the source U plane in bytes. */

#define gcregDEStrideUExRegAddrs                                          0x4AD8
#define GCREG_DE_STRIDE_UEX_MSB                                               15
#define GCREG_DE_STRIDE_UEX_LSB                                                3
#define GCREG_DE_STRIDE_UEX_BLK                                                0
#define GCREG_DE_STRIDE_UEX_Count                                              8
#define GCREG_DE_STRIDE_UEX_FieldMask                                 0x0003FFFF
#define GCREG_DE_STRIDE_UEX_ReadMask                                  0x0003FFFC
#define GCREG_DE_STRIDE_UEX_WriteMask                                 0x0003FFFC
#define GCREG_DE_STRIDE_UEX_ResetValue                                0x00000000

#define GCREG_DE_STRIDE_UEX_STRIDE                                        17 : 0
#define GCREG_DE_STRIDE_UEX_STRIDE_End                                        17
#define GCREG_DE_STRIDE_UEX_STRIDE_Start                                       0
#define GCREG_DE_STRIDE_UEX_STRIDE_Type                                      U18

/*******************************************************************************
** State gcregDEAddressVEx
*/

/* 32-bit aligned base address of the source V plane. */

#define gcregDEAddressVExRegAddrs                                         0x4AE0
#define GCREG_DE_ADDRESS_VEX_MSB                                              15
#define GCREG_DE_ADDRESS_VEX_LSB                                               3
#define GCREG_DE_ADDRESS_VEX_BLK                                               0
#define GCREG_DE_ADDRESS_VEX_Count                                             8
#define GCREG_DE_ADDRESS_VEX_FieldMask                                0xFFFFFFFF
#define GCREG_DE_ADDRESS_VEX_ReadMask                                 0xFFFFFFFC
#define GCREG_DE_ADDRESS_VEX_WriteMask                                0xFFFFFFFC
#define GCREG_DE_ADDRESS_VEX_ResetValue                               0x00000000

#define GCREG_DE_ADDRESS_VEX_ADDRESS                                      31 : 0
#define GCREG_DE_ADDRESS_VEX_ADDRESS_End                                      30
#define GCREG_DE_ADDRESS_VEX_ADDRESS_Start                                     0
#define GCREG_DE_ADDRESS_VEX_ADDRESS_Type                                    U31

/*******************************************************************************
** State gcregDEStrideVEx
*/

/* Stride of the source V plane in bytes. */

#define gcregDEStrideVExRegAddrs                                          0x4AE8
#define GCREG_DE_STRIDE_VEX_MSB                                               15
#define GCREG_DE_STRIDE_VEX_LSB                                                3
#define GCREG_DE_STRIDE_VEX_BLK                                                0
#define GCREG_DE_STRIDE_VEX_Count                                              8
#define GCREG_DE_STRIDE_VEX_FieldMask                                 0x0003FFFF
#define GCREG_DE_STRIDE_VEX_ReadMask                                  0x0003FFFC
#define GCREG_DE_STRIDE_VEX_WriteMask                                 0x0003FFFC
#define GCREG_DE_STRIDE_VEX_ResetValue                                0x00000000

#define GCREG_DE_STRIDE_VEX_STRIDE                                        17 : 0
#define GCREG_DE_STRIDE_VEX_STRIDE_End                                        17
#define GCREG_DE_STRIDE_VEX_STRIDE_Start                                       0
#define GCREG_DE_STRIDE_VEX_STRIDE_Type                                      U18

/*******************************************************************************
** State gcregDESrcRotationHeightEx
*/

/* 180/270 degree rotation configuration for the Source surface. Height field
** specifies the height of the surface in pixels.
*/

#define gcregDESrcRotationHeightExRegAddrs                                0x4AF0
#define GCREG_DE_SRC_ROTATION_HEIGHT_EX_MSB                                   15
#define GCREG_DE_SRC_ROTATION_HEIGHT_EX_LSB                                    3
#define GCREG_DE_SRC_ROTATION_HEIGHT_EX_BLK                                    0
#define GCREG_DE_SRC_ROTATION_HEIGHT_EX_Count                                  8
#define GCREG_DE_SRC_ROTATION_HEIGHT_EX_FieldMask                     0x0000FFFF
#define GCREG_DE_SRC_ROTATION_HEIGHT_EX_ReadMask                      0x0000FFFF
#define GCREG_DE_SRC_ROTATION_HEIGHT_EX_WriteMask                     0x0000FFFF
#define GCREG_DE_SRC_ROTATION_HEIGHT_EX_ResetValue                    0x00000000

#define GCREG_DE_SRC_ROTATION_HEIGHT_EX_HEIGHT                            15 : 0
#define GCREG_DE_SRC_ROTATION_HEIGHT_EX_HEIGHT_End                            15
#define GCREG_DE_SRC_ROTATION_HEIGHT_EX_HEIGHT_Start                           0
#define GCREG_DE_SRC_ROTATION_HEIGHT_EX_HEIGHT_Type                          U16

/*******************************************************************************
** State gcregDERotAngleEx
*/

/* 0/90/180/270 degree rotation configuration for the Source surface. Height
** field specifies the height of the surface in pixels.
*/

#define gcregDERotAngleExRegAddrs                                         0x4AF8
#define GCREG_DE_ROT_ANGLE_EX_MSB                                             15
#define GCREG_DE_ROT_ANGLE_EX_LSB                                              3
#define GCREG_DE_ROT_ANGLE_EX_BLK                                              0
#define GCREG_DE_ROT_ANGLE_EX_Count                                            8
#define GCREG_DE_ROT_ANGLE_EX_FieldMask                               0x000BB33F
#define GCREG_DE_ROT_ANGLE_EX_ReadMask                                0x000BB33F
#define GCREG_DE_ROT_ANGLE_EX_WriteMask                               0x000BB33F
#define GCREG_DE_ROT_ANGLE_EX_ResetValue                              0x00000000

#define GCREG_DE_ROT_ANGLE_EX_SRC                                          2 : 0
#define GCREG_DE_ROT_ANGLE_EX_SRC_End                                          2
#define GCREG_DE_ROT_ANGLE_EX_SRC_Start                                        0
#define GCREG_DE_ROT_ANGLE_EX_SRC_Type                                       U03
#define   GCREG_DE_ROT_ANGLE_EX_SRC_ROT0                                     0x0
#define   GCREG_DE_ROT_ANGLE_EX_SRC_FLIP_X                                   0x1
#define   GCREG_DE_ROT_ANGLE_EX_SRC_FLIP_Y                                   0x2
#define   GCREG_DE_ROT_ANGLE_EX_SRC_ROT90                                    0x4
#define   GCREG_DE_ROT_ANGLE_EX_SRC_ROT180                                   0x5
#define   GCREG_DE_ROT_ANGLE_EX_SRC_ROT270                                   0x6

#define GCREG_DE_ROT_ANGLE_EX_DST                                          5 : 3
#define GCREG_DE_ROT_ANGLE_EX_DST_End                                          5
#define GCREG_DE_ROT_ANGLE_EX_DST_Start                                        3
#define GCREG_DE_ROT_ANGLE_EX_DST_Type                                       U03
#define   GCREG_DE_ROT_ANGLE_EX_DST_ROT0                                     0x0
#define   GCREG_DE_ROT_ANGLE_EX_DST_FLIP_X                                   0x1
#define   GCREG_DE_ROT_ANGLE_EX_DST_FLIP_Y                                   0x2
#define   GCREG_DE_ROT_ANGLE_EX_DST_ROT90                                    0x4
#define   GCREG_DE_ROT_ANGLE_EX_DST_ROT180                                   0x5
#define   GCREG_DE_ROT_ANGLE_EX_DST_ROT270                                   0x6

#define GCREG_DE_ROT_ANGLE_EX_MASK_SRC                                     8 : 8
#define GCREG_DE_ROT_ANGLE_EX_MASK_SRC_End                                     8
#define GCREG_DE_ROT_ANGLE_EX_MASK_SRC_Start                                   8
#define GCREG_DE_ROT_ANGLE_EX_MASK_SRC_Type                                  U01
#define   GCREG_DE_ROT_ANGLE_EX_MASK_SRC_ENABLED                             0x0
#define   GCREG_DE_ROT_ANGLE_EX_MASK_SRC_MASKED                              0x1

#define GCREG_DE_ROT_ANGLE_EX_MASK_DST                                     9 : 9
#define GCREG_DE_ROT_ANGLE_EX_MASK_DST_End                                     9
#define GCREG_DE_ROT_ANGLE_EX_MASK_DST_Start                                   9
#define GCREG_DE_ROT_ANGLE_EX_MASK_DST_Type                                  U01
#define   GCREG_DE_ROT_ANGLE_EX_MASK_DST_ENABLED                             0x0
#define   GCREG_DE_ROT_ANGLE_EX_MASK_DST_MASKED                              0x1

#define GCREG_DE_ROT_ANGLE_EX_SRC_MIRROR                                 13 : 12
#define GCREG_DE_ROT_ANGLE_EX_SRC_MIRROR_End                                  13
#define GCREG_DE_ROT_ANGLE_EX_SRC_MIRROR_Start                                12
#define GCREG_DE_ROT_ANGLE_EX_SRC_MIRROR_Type                                U02
#define   GCREG_DE_ROT_ANGLE_EX_SRC_MIRROR_NONE                              0x0
#define   GCREG_DE_ROT_ANGLE_EX_SRC_MIRROR_MIRROR_X                          0x1
#define   GCREG_DE_ROT_ANGLE_EX_SRC_MIRROR_MIRROR_Y                          0x2
#define   GCREG_DE_ROT_ANGLE_EX_SRC_MIRROR_MIRROR_XY                         0x3

#define GCREG_DE_ROT_ANGLE_EX_MASK_SRC_MIRROR                            15 : 15
#define GCREG_DE_ROT_ANGLE_EX_MASK_SRC_MIRROR_End                             15
#define GCREG_DE_ROT_ANGLE_EX_MASK_SRC_MIRROR_Start                           15
#define GCREG_DE_ROT_ANGLE_EX_MASK_SRC_MIRROR_Type                           U01
#define   GCREG_DE_ROT_ANGLE_EX_MASK_SRC_MIRROR_ENABLED                      0x0
#define   GCREG_DE_ROT_ANGLE_EX_MASK_SRC_MIRROR_MASKED                       0x1

#define GCREG_DE_ROT_ANGLE_EX_DST_MIRROR                                 17 : 16
#define GCREG_DE_ROT_ANGLE_EX_DST_MIRROR_End                                  17
#define GCREG_DE_ROT_ANGLE_EX_DST_MIRROR_Start                                16
#define GCREG_DE_ROT_ANGLE_EX_DST_MIRROR_Type                                U02
#define   GCREG_DE_ROT_ANGLE_EX_DST_MIRROR_NONE                              0x0
#define   GCREG_DE_ROT_ANGLE_EX_DST_MIRROR_MIRROR_X                          0x1
#define   GCREG_DE_ROT_ANGLE_EX_DST_MIRROR_MIRROR_Y                          0x2
#define   GCREG_DE_ROT_ANGLE_EX_DST_MIRROR_MIRROR_XY                         0x3

#define GCREG_DE_ROT_ANGLE_EX_MASK_DST_MIRROR                            19 : 19
#define GCREG_DE_ROT_ANGLE_EX_MASK_DST_MIRROR_End                             19
#define GCREG_DE_ROT_ANGLE_EX_MASK_DST_MIRROR_Start                           19
#define GCREG_DE_ROT_ANGLE_EX_MASK_DST_MIRROR_Type                           U01
#define   GCREG_DE_ROT_ANGLE_EX_MASK_DST_MIRROR_ENABLED                      0x0
#define   GCREG_DE_ROT_ANGLE_EX_MASK_DST_MIRROR_MASKED                       0x1

/*******************************************************************************
** State gcregDEGlobalSrcColorEx
*/

/* Defines the global source color and alpha values. */

#define gcregDEGlobalSrcColorExRegAddrs                                   0x4B00
#define GCREG_DE_GLOBAL_SRC_COLOR_EX_MSB                                      15
#define GCREG_DE_GLOBAL_SRC_COLOR_EX_LSB                                       3
#define GCREG_DE_GLOBAL_SRC_COLOR_EX_BLK                                       0
#define GCREG_DE_GLOBAL_SRC_COLOR_EX_Count                                     8
#define GCREG_DE_GLOBAL_SRC_COLOR_EX_FieldMask                        0xFFFFFFFF
#define GCREG_DE_GLOBAL_SRC_COLOR_EX_ReadMask                         0xFFFFFFFF
#define GCREG_DE_GLOBAL_SRC_COLOR_EX_WriteMask                        0xFFFFFFFF
#define GCREG_DE_GLOBAL_SRC_COLOR_EX_ResetValue                       0x00000000

#define GCREG_DE_GLOBAL_SRC_COLOR_EX_ALPHA                               31 : 24
#define GCREG_DE_GLOBAL_SRC_COLOR_EX_ALPHA_End                                31
#define GCREG_DE_GLOBAL_SRC_COLOR_EX_ALPHA_Start                              24
#define GCREG_DE_GLOBAL_SRC_COLOR_EX_ALPHA_Type                              U08

#define GCREG_DE_GLOBAL_SRC_COLOR_EX_RED                                 23 : 16
#define GCREG_DE_GLOBAL_SRC_COLOR_EX_RED_End                                  23
#define GCREG_DE_GLOBAL_SRC_COLOR_EX_RED_Start                                16
#define GCREG_DE_GLOBAL_SRC_COLOR_EX_RED_Type                                U08

#define GCREG_DE_GLOBAL_SRC_COLOR_EX_GREEN                                15 : 8
#define GCREG_DE_GLOBAL_SRC_COLOR_EX_GREEN_End                                15
#define GCREG_DE_GLOBAL_SRC_COLOR_EX_GREEN_Start                               8
#define GCREG_DE_GLOBAL_SRC_COLOR_EX_GREEN_Type                              U08

#define GCREG_DE_GLOBAL_SRC_COLOR_EX_BLUE                                  7 : 0
#define GCREG_DE_GLOBAL_SRC_COLOR_EX_BLUE_End                                  7
#define GCREG_DE_GLOBAL_SRC_COLOR_EX_BLUE_Start                                0
#define GCREG_DE_GLOBAL_SRC_COLOR_EX_BLUE_Type                               U08

/*******************************************************************************
** State gcregDEGlobalDestColorEx
*/

/* Defines the global destination color and alpha values. */

#define gcregDEGlobalDestColorExRegAddrs                                  0x4B08
#define GCREG_DE_GLOBAL_DEST_COLOR_EX_MSB                                     15
#define GCREG_DE_GLOBAL_DEST_COLOR_EX_LSB                                      3
#define GCREG_DE_GLOBAL_DEST_COLOR_EX_BLK                                      0
#define GCREG_DE_GLOBAL_DEST_COLOR_EX_Count                                    8
#define GCREG_DE_GLOBAL_DEST_COLOR_EX_FieldMask                       0xFFFFFFFF
#define GCREG_DE_GLOBAL_DEST_COLOR_EX_ReadMask                        0xFFFFFFFF
#define GCREG_DE_GLOBAL_DEST_COLOR_EX_WriteMask                       0xFFFFFFFF
#define GCREG_DE_GLOBAL_DEST_COLOR_EX_ResetValue                      0x00000000

#define GCREG_DE_GLOBAL_DEST_COLOR_EX_ALPHA                              31 : 24
#define GCREG_DE_GLOBAL_DEST_COLOR_EX_ALPHA_End                               31
#define GCREG_DE_GLOBAL_DEST_COLOR_EX_ALPHA_Start                             24
#define GCREG_DE_GLOBAL_DEST_COLOR_EX_ALPHA_Type                             U08

#define GCREG_DE_GLOBAL_DEST_COLOR_EX_RED                                23 : 16
#define GCREG_DE_GLOBAL_DEST_COLOR_EX_RED_End                                 23
#define GCREG_DE_GLOBAL_DEST_COLOR_EX_RED_Start                               16
#define GCREG_DE_GLOBAL_DEST_COLOR_EX_RED_Type                               U08

#define GCREG_DE_GLOBAL_DEST_COLOR_EX_GREEN                               15 : 8
#define GCREG_DE_GLOBAL_DEST_COLOR_EX_GREEN_End                               15
#define GCREG_DE_GLOBAL_DEST_COLOR_EX_GREEN_Start                              8
#define GCREG_DE_GLOBAL_DEST_COLOR_EX_GREEN_Type                             U08

#define GCREG_DE_GLOBAL_DEST_COLOR_EX_BLUE                                 7 : 0
#define GCREG_DE_GLOBAL_DEST_COLOR_EX_BLUE_End                                 7
#define GCREG_DE_GLOBAL_DEST_COLOR_EX_BLUE_Start                               0
#define GCREG_DE_GLOBAL_DEST_COLOR_EX_BLUE_Type                              U08

/*******************************************************************************
** State gcregDEColorMultiplyModesEx
*/

/* Color modes to multiply Source or Destination pixel color by alpha
** channel. Alpha can be from global color source or current pixel.
*/

#define gcregDEColorMultiplyModesExRegAddrs                               0x4B10
#define GCREG_DE_COLOR_MULTIPLY_MODES_EX_MSB                                  15
#define GCREG_DE_COLOR_MULTIPLY_MODES_EX_LSB                                   3
#define GCREG_DE_COLOR_MULTIPLY_MODES_EX_BLK                                   0
#define GCREG_DE_COLOR_MULTIPLY_MODES_EX_Count                                 8
#define GCREG_DE_COLOR_MULTIPLY_MODES_EX_FieldMask                    0x00100311
#define GCREG_DE_COLOR_MULTIPLY_MODES_EX_ReadMask                     0x00100311
#define GCREG_DE_COLOR_MULTIPLY_MODES_EX_WriteMask                    0x00100311
#define GCREG_DE_COLOR_MULTIPLY_MODES_EX_ResetValue                   0x00000000

#define GCREG_DE_COLOR_MULTIPLY_MODES_EX_SRC_PREMULTIPLY                   0 : 0
#define GCREG_DE_COLOR_MULTIPLY_MODES_EX_SRC_PREMULTIPLY_End                   0
#define GCREG_DE_COLOR_MULTIPLY_MODES_EX_SRC_PREMULTIPLY_Start                 0
#define GCREG_DE_COLOR_MULTIPLY_MODES_EX_SRC_PREMULTIPLY_Type                U01
#define   GCREG_DE_COLOR_MULTIPLY_MODES_EX_SRC_PREMULTIPLY_DISABLE           0x0
#define   GCREG_DE_COLOR_MULTIPLY_MODES_EX_SRC_PREMULTIPLY_ENABLE            0x1

#define GCREG_DE_COLOR_MULTIPLY_MODES_EX_DST_PREMULTIPLY                   4 : 4
#define GCREG_DE_COLOR_MULTIPLY_MODES_EX_DST_PREMULTIPLY_End                   4
#define GCREG_DE_COLOR_MULTIPLY_MODES_EX_DST_PREMULTIPLY_Start                 4
#define GCREG_DE_COLOR_MULTIPLY_MODES_EX_DST_PREMULTIPLY_Type                U01
#define   GCREG_DE_COLOR_MULTIPLY_MODES_EX_DST_PREMULTIPLY_DISABLE           0x0
#define   GCREG_DE_COLOR_MULTIPLY_MODES_EX_DST_PREMULTIPLY_ENABLE            0x1

#define GCREG_DE_COLOR_MULTIPLY_MODES_EX_SRC_GLOBAL_PREMULTIPLY            9 : 8
#define GCREG_DE_COLOR_MULTIPLY_MODES_EX_SRC_GLOBAL_PREMULTIPLY_End            9
#define GCREG_DE_COLOR_MULTIPLY_MODES_EX_SRC_GLOBAL_PREMULTIPLY_Start          8
#define GCREG_DE_COLOR_MULTIPLY_MODES_EX_SRC_GLOBAL_PREMULTIPLY_Type         U02
#define   GCREG_DE_COLOR_MULTIPLY_MODES_EX_SRC_GLOBAL_PREMULTIPLY_DISABLE    0x0
#define   GCREG_DE_COLOR_MULTIPLY_MODES_EX_SRC_GLOBAL_PREMULTIPLY_ALPHA      0x1
#define   GCREG_DE_COLOR_MULTIPLY_MODES_EX_SRC_GLOBAL_PREMULTIPLY_COLOR      0x2

#define GCREG_DE_COLOR_MULTIPLY_MODES_EX_DST_DEMULTIPLY                  20 : 20
#define GCREG_DE_COLOR_MULTIPLY_MODES_EX_DST_DEMULTIPLY_End                   20
#define GCREG_DE_COLOR_MULTIPLY_MODES_EX_DST_DEMULTIPLY_Start                 20
#define GCREG_DE_COLOR_MULTIPLY_MODES_EX_DST_DEMULTIPLY_Type                 U01
#define   GCREG_DE_COLOR_MULTIPLY_MODES_EX_DST_DEMULTIPLY_DISABLE            0x0
#define   GCREG_DE_COLOR_MULTIPLY_MODES_EX_DST_DEMULTIPLY_ENABLE             0x1

/*******************************************************************************
** State gcregPETransparencyEx
*/

#define gcregPETransparencyExRegAddrs                                     0x4B18
#define GCREG_PE_TRANSPARENCY_EX_MSB                                          15
#define GCREG_PE_TRANSPARENCY_EX_LSB                                           3
#define GCREG_PE_TRANSPARENCY_EX_BLK                                           0
#define GCREG_PE_TRANSPARENCY_EX_Count                                         8
#define GCREG_PE_TRANSPARENCY_EX_FieldMask                            0xB3331333
#define GCREG_PE_TRANSPARENCY_EX_ReadMask                             0xB3331333
#define GCREG_PE_TRANSPARENCY_EX_WriteMask                            0xB3331333
#define GCREG_PE_TRANSPARENCY_EX_ResetValue                           0x00000000

/* Source transparency mode. */
#define GCREG_PE_TRANSPARENCY_EX_SOURCE                                    1 : 0
#define GCREG_PE_TRANSPARENCY_EX_SOURCE_End                                    1
#define GCREG_PE_TRANSPARENCY_EX_SOURCE_Start                                  0
#define GCREG_PE_TRANSPARENCY_EX_SOURCE_Type                                 U02
#define   GCREG_PE_TRANSPARENCY_EX_SOURCE_OPAQUE                             0x0
#define   GCREG_PE_TRANSPARENCY_EX_SOURCE_MASK                               0x1
#define   GCREG_PE_TRANSPARENCY_EX_SOURCE_KEY                                0x2

/* Pattern transparency mode. KEY transparency mode is reserved. */
#define GCREG_PE_TRANSPARENCY_EX_PATTERN                                   5 : 4
#define GCREG_PE_TRANSPARENCY_EX_PATTERN_End                                   5
#define GCREG_PE_TRANSPARENCY_EX_PATTERN_Start                                 4
#define GCREG_PE_TRANSPARENCY_EX_PATTERN_Type                                U02
#define   GCREG_PE_TRANSPARENCY_EX_PATTERN_OPAQUE                            0x0
#define   GCREG_PE_TRANSPARENCY_EX_PATTERN_MASK                              0x1
#define   GCREG_PE_TRANSPARENCY_EX_PATTERN_KEY                               0x2

/* Destination transparency mode. MASK transparency mode is reserved. */
#define GCREG_PE_TRANSPARENCY_EX_DESTINATION                               9 : 8
#define GCREG_PE_TRANSPARENCY_EX_DESTINATION_End                               9
#define GCREG_PE_TRANSPARENCY_EX_DESTINATION_Start                             8
#define GCREG_PE_TRANSPARENCY_EX_DESTINATION_Type                            U02
#define   GCREG_PE_TRANSPARENCY_EX_DESTINATION_OPAQUE                        0x0
#define   GCREG_PE_TRANSPARENCY_EX_DESTINATION_MASK                          0x1
#define   GCREG_PE_TRANSPARENCY_EX_DESTINATION_KEY                           0x2

/* Mask field for Source/Pattern/Destination fields. */
#define GCREG_PE_TRANSPARENCY_EX_MASK_TRANSPARENCY                       12 : 12
#define GCREG_PE_TRANSPARENCY_EX_MASK_TRANSPARENCY_End                        12
#define GCREG_PE_TRANSPARENCY_EX_MASK_TRANSPARENCY_Start                      12
#define GCREG_PE_TRANSPARENCY_EX_MASK_TRANSPARENCY_Type                      U01
#define   GCREG_PE_TRANSPARENCY_EX_MASK_TRANSPARENCY_ENABLED                 0x0
#define   GCREG_PE_TRANSPARENCY_EX_MASK_TRANSPARENCY_MASKED                  0x1

/* Source usage override. */
#define GCREG_PE_TRANSPARENCY_EX_USE_SRC_OVERRIDE                        17 : 16
#define GCREG_PE_TRANSPARENCY_EX_USE_SRC_OVERRIDE_End                         17
#define GCREG_PE_TRANSPARENCY_EX_USE_SRC_OVERRIDE_Start                       16
#define GCREG_PE_TRANSPARENCY_EX_USE_SRC_OVERRIDE_Type                       U02
#define   GCREG_PE_TRANSPARENCY_EX_USE_SRC_OVERRIDE_DEFAULT                  0x0
#define   GCREG_PE_TRANSPARENCY_EX_USE_SRC_OVERRIDE_USE_ENABLE               0x1
#define   GCREG_PE_TRANSPARENCY_EX_USE_SRC_OVERRIDE_USE_DISABLE              0x2

/* Pattern usage override. */
#define GCREG_PE_TRANSPARENCY_EX_USE_PAT_OVERRIDE                        21 : 20
#define GCREG_PE_TRANSPARENCY_EX_USE_PAT_OVERRIDE_End                         21
#define GCREG_PE_TRANSPARENCY_EX_USE_PAT_OVERRIDE_Start                       20
#define GCREG_PE_TRANSPARENCY_EX_USE_PAT_OVERRIDE_Type                       U02
#define   GCREG_PE_TRANSPARENCY_EX_USE_PAT_OVERRIDE_DEFAULT                  0x0
#define   GCREG_PE_TRANSPARENCY_EX_USE_PAT_OVERRIDE_USE_ENABLE               0x1
#define   GCREG_PE_TRANSPARENCY_EX_USE_PAT_OVERRIDE_USE_DISABLE              0x2

/* Destination usage override. */
#define GCREG_PE_TRANSPARENCY_EX_USE_DST_OVERRIDE                        25 : 24
#define GCREG_PE_TRANSPARENCY_EX_USE_DST_OVERRIDE_End                         25
#define GCREG_PE_TRANSPARENCY_EX_USE_DST_OVERRIDE_Start                       24
#define GCREG_PE_TRANSPARENCY_EX_USE_DST_OVERRIDE_Type                       U02
#define   GCREG_PE_TRANSPARENCY_EX_USE_DST_OVERRIDE_DEFAULT                  0x0
#define   GCREG_PE_TRANSPARENCY_EX_USE_DST_OVERRIDE_USE_ENABLE               0x1
#define   GCREG_PE_TRANSPARENCY_EX_USE_DST_OVERRIDE_USE_DISABLE              0x2

/* 2D resource usage override mask field. */
#define GCREG_PE_TRANSPARENCY_EX_MASK_RESOURCE_OVERRIDE                  28 : 28
#define GCREG_PE_TRANSPARENCY_EX_MASK_RESOURCE_OVERRIDE_End                   28
#define GCREG_PE_TRANSPARENCY_EX_MASK_RESOURCE_OVERRIDE_Start                 28
#define GCREG_PE_TRANSPARENCY_EX_MASK_RESOURCE_OVERRIDE_Type                 U01
#define   GCREG_PE_TRANSPARENCY_EX_MASK_RESOURCE_OVERRIDE_ENABLED            0x0
#define   GCREG_PE_TRANSPARENCY_EX_MASK_RESOURCE_OVERRIDE_MASKED             0x1

/* DFB Color Key. */
#define GCREG_PE_TRANSPARENCY_EX_DFB_COLOR_KEY                           29 : 29
#define GCREG_PE_TRANSPARENCY_EX_DFB_COLOR_KEY_End                            29
#define GCREG_PE_TRANSPARENCY_EX_DFB_COLOR_KEY_Start                          29
#define GCREG_PE_TRANSPARENCY_EX_DFB_COLOR_KEY_Type                          U01
#define   GCREG_PE_TRANSPARENCY_EX_DFB_COLOR_KEY_DISABLED                    0x0
#define   GCREG_PE_TRANSPARENCY_EX_DFB_COLOR_KEY_ENABLED                     0x1

#define GCREG_PE_TRANSPARENCY_EX_MASK_DFB_COLOR_KEY                      31 : 31
#define GCREG_PE_TRANSPARENCY_EX_MASK_DFB_COLOR_KEY_End                       31
#define GCREG_PE_TRANSPARENCY_EX_MASK_DFB_COLOR_KEY_Start                     31
#define GCREG_PE_TRANSPARENCY_EX_MASK_DFB_COLOR_KEY_Type                     U01
#define   GCREG_PE_TRANSPARENCY_EX_MASK_DFB_COLOR_KEY_ENABLED                0x0
#define   GCREG_PE_TRANSPARENCY_EX_MASK_DFB_COLOR_KEY_MASKED                 0x1

/*******************************************************************************
** State gcregPEControlEx
*/

/* General purpose control register. */

#define gcregPEControlExRegAddrs                                          0x4B20
#define GCREG_PE_CONTROL_EX_MSB                                               15
#define GCREG_PE_CONTROL_EX_LSB                                                3
#define GCREG_PE_CONTROL_EX_BLK                                                0
#define GCREG_PE_CONTROL_EX_Count                                              8
#define GCREG_PE_CONTROL_EX_FieldMask                                 0x00000999
#define GCREG_PE_CONTROL_EX_ReadMask                                  0x00000999
#define GCREG_PE_CONTROL_EX_WriteMask                                 0x00000999
#define GCREG_PE_CONTROL_EX_ResetValue                                0x00000000

#define GCREG_PE_CONTROL_EX_YUV                                            0 : 0
#define GCREG_PE_CONTROL_EX_YUV_End                                            0
#define GCREG_PE_CONTROL_EX_YUV_Start                                          0
#define GCREG_PE_CONTROL_EX_YUV_Type                                         U01
#define   GCREG_PE_CONTROL_EX_YUV_601                                        0x0
#define   GCREG_PE_CONTROL_EX_YUV_709                                        0x1

#define GCREG_PE_CONTROL_EX_MASK_YUV                                       3 : 3
#define GCREG_PE_CONTROL_EX_MASK_YUV_End                                       3
#define GCREG_PE_CONTROL_EX_MASK_YUV_Start                                     3
#define GCREG_PE_CONTROL_EX_MASK_YUV_Type                                    U01
#define   GCREG_PE_CONTROL_EX_MASK_YUV_ENABLED                               0x0
#define   GCREG_PE_CONTROL_EX_MASK_YUV_MASKED                                0x1

#define GCREG_PE_CONTROL_EX_UV_SWIZZLE                                     4 : 4
#define GCREG_PE_CONTROL_EX_UV_SWIZZLE_End                                     4
#define GCREG_PE_CONTROL_EX_UV_SWIZZLE_Start                                   4
#define GCREG_PE_CONTROL_EX_UV_SWIZZLE_Type                                  U01
#define   GCREG_PE_CONTROL_EX_UV_SWIZZLE_UV                                  0x0
#define   GCREG_PE_CONTROL_EX_UV_SWIZZLE_VU                                  0x1

#define GCREG_PE_CONTROL_EX_MASK_UV_SWIZZLE                                7 : 7
#define GCREG_PE_CONTROL_EX_MASK_UV_SWIZZLE_End                                7
#define GCREG_PE_CONTROL_EX_MASK_UV_SWIZZLE_Start                              7
#define GCREG_PE_CONTROL_EX_MASK_UV_SWIZZLE_Type                             U01
#define   GCREG_PE_CONTROL_EX_MASK_UV_SWIZZLE_ENABLED                        0x0
#define   GCREG_PE_CONTROL_EX_MASK_UV_SWIZZLE_MASKED                         0x1

/* YUV to RGB convert enable */
#define GCREG_PE_CONTROL_EX_YUVRGB                                         8 : 8
#define GCREG_PE_CONTROL_EX_YUVRGB_End                                         8
#define GCREG_PE_CONTROL_EX_YUVRGB_Start                                       8
#define GCREG_PE_CONTROL_EX_YUVRGB_Type                                      U01
#define   GCREG_PE_CONTROL_EX_YUVRGB_DISABLED                                0x0
#define   GCREG_PE_CONTROL_EX_YUVRGB_ENABLED                                 0x1

#define GCREG_PE_CONTROL_EX_MASK_YUVRGB                                  11 : 11
#define GCREG_PE_CONTROL_EX_MASK_YUVRGB_End                                   11
#define GCREG_PE_CONTROL_EX_MASK_YUVRGB_Start                                 11
#define GCREG_PE_CONTROL_EX_MASK_YUVRGB_Type                                 U01
#define   GCREG_PE_CONTROL_EX_MASK_YUVRGB_ENABLED                            0x0
#define   GCREG_PE_CONTROL_EX_MASK_YUVRGB_MASKED                             0x1

/*******************************************************************************
** State gcregDESrcColorKeyHighEx
*/

/* Defines the source transparency color in source format. */

#define gcregDESrcColorKeyHighExRegAddrs                                  0x4B28
#define GCREG_DE_SRC_COLOR_KEY_HIGH_EX_MSB                                    15
#define GCREG_DE_SRC_COLOR_KEY_HIGH_EX_LSB                                     3
#define GCREG_DE_SRC_COLOR_KEY_HIGH_EX_BLK                                     0
#define GCREG_DE_SRC_COLOR_KEY_HIGH_EX_Count                                   8
#define GCREG_DE_SRC_COLOR_KEY_HIGH_EX_FieldMask                      0xFFFFFFFF
#define GCREG_DE_SRC_COLOR_KEY_HIGH_EX_ReadMask                       0xFFFFFFFF
#define GCREG_DE_SRC_COLOR_KEY_HIGH_EX_WriteMask                      0xFFFFFFFF
#define GCREG_DE_SRC_COLOR_KEY_HIGH_EX_ResetValue                     0x00000000

#define GCREG_DE_SRC_COLOR_KEY_HIGH_EX_ALPHA                             31 : 24
#define GCREG_DE_SRC_COLOR_KEY_HIGH_EX_ALPHA_End                              31
#define GCREG_DE_SRC_COLOR_KEY_HIGH_EX_ALPHA_Start                            24
#define GCREG_DE_SRC_COLOR_KEY_HIGH_EX_ALPHA_Type                            U08

#define GCREG_DE_SRC_COLOR_KEY_HIGH_EX_RED                               23 : 16
#define GCREG_DE_SRC_COLOR_KEY_HIGH_EX_RED_End                                23
#define GCREG_DE_SRC_COLOR_KEY_HIGH_EX_RED_Start                              16
#define GCREG_DE_SRC_COLOR_KEY_HIGH_EX_RED_Type                              U08

#define GCREG_DE_SRC_COLOR_KEY_HIGH_EX_GREEN                              15 : 8
#define GCREG_DE_SRC_COLOR_KEY_HIGH_EX_GREEN_End                              15
#define GCREG_DE_SRC_COLOR_KEY_HIGH_EX_GREEN_Start                             8
#define GCREG_DE_SRC_COLOR_KEY_HIGH_EX_GREEN_Type                            U08

#define GCREG_DE_SRC_COLOR_KEY_HIGH_EX_BLUE                                7 : 0
#define GCREG_DE_SRC_COLOR_KEY_HIGH_EX_BLUE_End                                7
#define GCREG_DE_SRC_COLOR_KEY_HIGH_EX_BLUE_Start                              0
#define GCREG_DE_SRC_COLOR_KEY_HIGH_EX_BLUE_Type                             U08

/*******************************************************************************
** State gcregDESrcExConfigEx
*/

#define gcregDESrcExConfigExRegAddrs                                      0x4B30
#define GCREG_DE_SRC_EX_CONFIG_EX_MSB                                         15
#define GCREG_DE_SRC_EX_CONFIG_EX_LSB                                          3
#define GCREG_DE_SRC_EX_CONFIG_EX_BLK                                          0
#define GCREG_DE_SRC_EX_CONFIG_EX_Count                                        8
#define GCREG_DE_SRC_EX_CONFIG_EX_FieldMask                           0x00000109
#define GCREG_DE_SRC_EX_CONFIG_EX_ReadMask                            0x00000109
#define GCREG_DE_SRC_EX_CONFIG_EX_WriteMask                           0x00000109
#define GCREG_DE_SRC_EX_CONFIG_EX_ResetValue                          0x00000000

/* Source multi tiled address computation control. */
#define GCREG_DE_SRC_EX_CONFIG_EX_MULTI_TILED                              0 : 0
#define GCREG_DE_SRC_EX_CONFIG_EX_MULTI_TILED_End                              0
#define GCREG_DE_SRC_EX_CONFIG_EX_MULTI_TILED_Start                            0
#define GCREG_DE_SRC_EX_CONFIG_EX_MULTI_TILED_Type                           U01
#define   GCREG_DE_SRC_EX_CONFIG_EX_MULTI_TILED_DISABLED                     0x0
#define   GCREG_DE_SRC_EX_CONFIG_EX_MULTI_TILED_ENABLED                      0x1

/* Source super tiled address computation control. */
#define GCREG_DE_SRC_EX_CONFIG_EX_SUPER_TILED                              3 : 3
#define GCREG_DE_SRC_EX_CONFIG_EX_SUPER_TILED_End                              3
#define GCREG_DE_SRC_EX_CONFIG_EX_SUPER_TILED_Start                            3
#define GCREG_DE_SRC_EX_CONFIG_EX_SUPER_TILED_Type                           U01
#define   GCREG_DE_SRC_EX_CONFIG_EX_SUPER_TILED_DISABLED                     0x0
#define   GCREG_DE_SRC_EX_CONFIG_EX_SUPER_TILED_ENABLED                      0x1

/* Source super tiled address computation control. */
#define GCREG_DE_SRC_EX_CONFIG_EX_MINOR_TILED                              8 : 8
#define GCREG_DE_SRC_EX_CONFIG_EX_MINOR_TILED_End                              8
#define GCREG_DE_SRC_EX_CONFIG_EX_MINOR_TILED_Start                            8
#define GCREG_DE_SRC_EX_CONFIG_EX_MINOR_TILED_Type                           U01
#define   GCREG_DE_SRC_EX_CONFIG_EX_MINOR_TILED_DISABLED                     0x0
#define   GCREG_DE_SRC_EX_CONFIG_EX_MINOR_TILED_ENABLED                      0x1

/*******************************************************************************
** State gcregDESrcExAddressEx
*/

/* 32-bit aligned base address of the source extra surface. */

#define gcregDESrcExAddressExRegAddrs                                     0x4B38
#define GCREG_DE_SRC_EX_ADDRESS_EX_MSB                                        15
#define GCREG_DE_SRC_EX_ADDRESS_EX_LSB                                         3
#define GCREG_DE_SRC_EX_ADDRESS_EX_BLK                                         0
#define GCREG_DE_SRC_EX_ADDRESS_EX_Count                                       8
#define GCREG_DE_SRC_EX_ADDRESS_EX_FieldMask                          0xFFFFFFFF
#define GCREG_DE_SRC_EX_ADDRESS_EX_ReadMask                           0xFFFFFFFC
#define GCREG_DE_SRC_EX_ADDRESS_EX_WriteMask                          0xFFFFFFFC
#define GCREG_DE_SRC_EX_ADDRESS_EX_ResetValue                         0x00000000

#define GCREG_DE_SRC_EX_ADDRESS_EX_ADDRESS                                31 : 0
#define GCREG_DE_SRC_EX_ADDRESS_EX_ADDRESS_End                                30
#define GCREG_DE_SRC_EX_ADDRESS_EX_ADDRESS_Start                               0
#define GCREG_DE_SRC_EX_ADDRESS_EX_ADDRESS_Type                              U31

/*******************************************************************************
** Generic defines
*/

#define AQ_DRAWING_ENGINE_FORMAT_SUB_SAMPLE_MODE_YUV_MODE422                 0x0
#define AQ_DRAWING_ENGINE_FORMAT_SUB_SAMPLE_MODE_YUV_MODE420                 0x1

#define AQ_DRAWING_ENGINE_FORMAT_SWIZZLE_ARGB                                0x0
#define AQ_DRAWING_ENGINE_FORMAT_SWIZZLE_RGBA                                0x1
#define AQ_DRAWING_ENGINE_FORMAT_SWIZZLE_ABGR                                0x2
#define AQ_DRAWING_ENGINE_FORMAT_SWIZZLE_BGRA                                0x3

#define AQ_DRAWING_ENGINE_FORMAT_FORMAT_X4R4G4B4                            0x00
#define AQ_DRAWING_ENGINE_FORMAT_FORMAT_A4R4G4B4                            0x01
#define AQ_DRAWING_ENGINE_FORMAT_FORMAT_X1R5G5B5                            0x02
#define AQ_DRAWING_ENGINE_FORMAT_FORMAT_A1R5G5B5                            0x03
#define AQ_DRAWING_ENGINE_FORMAT_FORMAT_R5G6B5                              0x04
#define AQ_DRAWING_ENGINE_FORMAT_FORMAT_X8R8G8B8                            0x05
#define AQ_DRAWING_ENGINE_FORMAT_FORMAT_A8R8G8B8                            0x06
#define AQ_DRAWING_ENGINE_FORMAT_FORMAT_YUY2                                0x07
#define AQ_DRAWING_ENGINE_FORMAT_FORMAT_UYVY                                0x08
#define AQ_DRAWING_ENGINE_FORMAT_FORMAT_INDEX8                              0x09
#define AQ_DRAWING_ENGINE_FORMAT_FORMAT_MONOCHROME                          0x0A
#define AQ_DRAWING_ENGINE_FORMAT_FORMAT_YV12                                0x0F
#define AQ_DRAWING_ENGINE_FORMAT_FORMAT_A8                                  0x10
#define AQ_DRAWING_ENGINE_FORMAT_FORMAT_NV12                                0x11
#define AQ_DRAWING_ENGINE_FORMAT_FORMAT_NV16                                0x12
#define AQ_DRAWING_ENGINE_FORMAT_FORMAT_RG16                                0x13

/* ~~~~~~~~~~~~~ */

#define AQ_DRAWING_ENGINE_ALPHA_BLENDING_ALPHA_MODE_NORMAL                   0x0
#define AQ_DRAWING_ENGINE_ALPHA_BLENDING_ALPHA_MODE_INVERSED                 0x1

#define AQ_DRAWING_ENGINE_ALPHA_BLENDING_GLOBAL_ALPHA_MODE_NORMAL            0x0
#define AQ_DRAWING_ENGINE_ALPHA_BLENDING_GLOBAL_ALPHA_MODE_GLOBAL            0x1
#define AQ_DRAWING_ENGINE_ALPHA_BLENDING_GLOBAL_ALPHA_MODE_SCALED            0x2

#define AQ_DRAWING_ENGINE_ALPHA_BLENDING_COLOR_MODE_NORMAL                   0x0
#define AQ_DRAWING_ENGINE_ALPHA_BLENDING_COLOR_MODE_MULTIPLY                 0x1

#define AQ_DRAWING_ENGINE_ALPHA_BLENDING_BLENDING_MODE_ZERO                  0x0
#define AQ_DRAWING_ENGINE_ALPHA_BLENDING_BLENDING_MODE_ONE                   0x1
#define AQ_DRAWING_ENGINE_ALPHA_BLENDING_BLENDING_MODE_NORMAL                0x2
#define AQ_DRAWING_ENGINE_ALPHA_BLENDING_BLENDING_MODE_INVERSED              0x3
#define AQ_DRAWING_ENGINE_ALPHA_BLENDING_BLENDING_MODE_COLOR                 0x4
#define AQ_DRAWING_ENGINE_ALPHA_BLENDING_BLENDING_MODE_COLOR_INVERSED        0x5
#define AQ_DRAWING_ENGINE_ALPHA_BLENDING_BLENDING_MODE_SATURATED_ALPHA       0x6
#define AQ_DRAWING_ENGINE_ALPHA_BLENDING_BLENDING_MODE_SATURATED_DEST_ALPHA  0x7

/* ~~~~~~~~~~~~~ */

#define AQ_DRAWING_ENGINE_RESOURCE_USAGE_OVERRIDE_DEFAULT                    0x0
#define AQ_DRAWING_ENGINE_RESOURCE_USAGE_OVERRIDE_USE_ENABLE                 0x1
#define AQ_DRAWING_ENGINE_RESOURCE_USAGE_OVERRIDE_USE_DISABLE                0x2

/* ~~~~~~~~~~~~~ */

#define AQ_DRAWING_ENGINE_FACTOR_INVERSE_DISABLE                             0x0
#define AQ_DRAWING_ENGINE_FACTOR_INVERSE_ENABLE                              0x1

#endif
