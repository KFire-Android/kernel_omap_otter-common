/*
 * ==========================================================================
 *               Texas Instruments OMAP(TM) Platform Firmware
 * (c) Copyright 2009, Texas Instruments Incorporated.  All Rights Reserved.
 *
 *  Use of this firmware is controlled by the terms and conditions found
 *  in the license agreement under which this firmware has been supplied.
 * ==========================================================================
 */

#ifndef _ABE_DAT_H_
#define _ABE_DAT_H_

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Callbacks
 */
abe_subroutine2 callbacks[MAXCALLBACK];	/* 2 parameters subroutine pointers */

abe_port_t abe_port [LAST_PORT_ID];	/* list of ABE ports */
const abe_port_t abe_port_init [LAST_PORT_ID] = {
/* status, data format, drift, callback, io-task buffer 1, io-task buffer 2,
 * protocol, dma offset, features, name
 * - Features reseted at start
 */

	/* DMIC */
	{
		IDLE_P,
		{96000, SIX_MSB},
		NODRIFT,
		NOCALLBACK,
		0,
		0,
		{
			SNK_P,
			DMIC_PORT_PROT,
			{{
				dmem_dmic,
				dmem_dmic_size,
				DMIC_ITER
			}},
		},
		{0, 0},
		{EQMIC, 0},
		"DMIC",
	},
	/* PDM_UL */
	{
		IDLE_P,
		{96000, STEREO_MSB},
		NODRIFT,
		NOCALLBACK,
		smem_amic,
		0,
		{
			SNK_P,
			MCPDMUL_PORT_PROT,
			{{
				dmem_amic,
				dmem_amic_size,
				MCPDM_UL_ITER,
			}},
		},
		{0, 0},
		{EQMIC, 0},
		"PDM_UL",
	},
	/* BT_VX_UL */
	{
		IDLE_P,
		{8000, STEREO_MSB},
		NODRIFT,
		NOCALLBACK,
		smem_bt_vx_ul,
		0,
		{
			SNK_P,
			SERIAL_PORT_PROT,
			{{
				MCBSP1_DMA_TX * ATC_SIZE,
				dmem_bt_vx_ul,
				dmem_bt_vx_ul_size,
				1 * SCHED_LOOP_8kHz,
			}},
		},
		{0, 0},
		{0},
		"BT_VX_UL",
	},
	/* MM_UL */
	{
		IDLE_P,
		{48000, STEREO_MSB},
		NODRIFT,
		NOCALLBACK,
		smem_mm_ul,
		0,
		{
			SRC_P,
			DMAREQ_PORT_PROT,
			{{
				CBPr_DMA_RTX3 * ATC_SIZE,
				dmem_mm_ul,
				dmem_mm_ul_size,
				10 * SCHED_LOOP_48kHz,
				ABE_DMASTATUS_RAW,
				1 << 3,
			}},
		},
		{CIRCULAR_BUFFER_PERIPHERAL_R__3, 120},
		{UPROUTE, 0},
		"MM_UL",
	},
	/* MM_UL2 */
	{
		IDLE_P,
		{48000, STEREO_MSB},
		NODRIFT,
		NOCALLBACK,
		smem_mm_ul2,
		0,
		{
			SRC_P,
			DMAREQ_PORT_PROT,
			{{
				CBPr_DMA_RTX4 * ATC_SIZE,
				dmem_mm_ul2,
				dmem_mm_ul2_size,
				2 * SCHED_LOOP_48kHz,
				ABE_DMASTATUS_RAW,
				1 << 4,
			}},
		},
		{CIRCULAR_BUFFER_PERIPHERAL_R__4, 24},
		{UPROUTE, 0},
		"MM_UL2",
	},
	/* VX_UL */
	{
		IDLE_P,
		{8000, MONO_MSB},
		NODRIFT,
		NOCALLBACK,
		smem_vx_ul,
		0,
		{
			SRC_P,
			DMAREQ_PORT_PROT,
			{{
				CBPr_DMA_RTX2*ATC_SIZE,
				dmem_vx_ul,
				dmem_vx_ul_size / 2,
				1 * SCHED_LOOP_8kHz,
				ABE_DMASTATUS_RAW,
				1 << 2,
			}},
		},
		{CIRCULAR_BUFFER_PERIPHERAL_R__2, 2},
		{ASRC2, 0},
		"VX_UL",
	},
	/* MM_DL */
	{
		IDLE_P,
		{48000, STEREO_MSB},
		NODRIFT,
		NOCALLBACK,
		smem_mm_dl_opp100,
		0,
		{
			SNK_P,
			PINGPONG_PORT_PROT,
			{{
				CBPr_DMA_RTX0 * ATC_SIZE,
				dmem_mm_dl,
				dmem_mm_dl_size,
				2 * SCHED_LOOP_48kHz,
				ABE_DMASTATUS_RAW,
				1 << 0,
			}},
		},
		{CIRCULAR_BUFFER_PERIPHERAL_R__0, 24},
		{ASRC3, 0},
		"MM_DL",
	},
	/* VX_DL */
	{
		IDLE_P,
		{8000, MONO_MSB},
		NODRIFT,
		NOCALLBACK,
		smem_vx_dl,
		0,
		{
			SNK_P,
			DMAREQ_PORT_PROT,
			{{
				CBPr_DMA_RTX1 * ATC_SIZE,
				dmem_vx_dl,
				dmem_vx_dl_size,
				1 * SCHED_LOOP_8kHz,
				ABE_DMASTATUS_RAW,
				1 << 1,
			}},
		},
		{CIRCULAR_BUFFER_PERIPHERAL_R__1, 2},
		{ASRC1, 0},
		"VX_DL",
	},
	/* TONES_DL */
	{
		IDLE_P,
		{48000, STEREO_MSB},
		NODRIFT,
		NOCALLBACK,
		smem_tones_dl,
		0,
		{
			SNK_P,
			DMAREQ_PORT_PROT,
			{{
				CBPr_DMA_RTX5 * ATC_SIZE,
				dmem_tones_dl,
				dmem_tones_dl_size,
				2 * SCHED_LOOP_48kHz,
				ABE_DMASTATUS_RAW,
				1 << 5,
			}},
		},
		{CIRCULAR_BUFFER_PERIPHERAL_R__5, 24},
		{0},
		"TONES_DL",
	},
	/* VIB_DL */
	{
		IDLE_P,
		{24000, STEREO_MSB},
		NODRIFT,
		NOCALLBACK,
		smem_vib,
		0,
		{
			SNK_P,
			DMAREQ_PORT_PROT,
			{{
				CBPr_DMA_RTX6 * ATC_SIZE,
				dmem_vib_dl,
				dmem_vib_dl_size,
				2 * SCHED_LOOP_24kHz,
				ABE_DMASTATUS_RAW,
				1 << 6,
			}},
		},
		{CIRCULAR_BUFFER_PERIPHERAL_R__6, 12},
		{0},
		"VIB_DL",
	},
	/* BT_VX_DL */
	{
		IDLE_P,
		{8000, MONO_MSB},
		NODRIFT,
		NOCALLBACK,
		smem_bt_vx_dl,
		0,
		{
			SRC_P,
			SERIAL_PORT_PROT,
			{{
				MCBSP1_DMA_RX * ATC_SIZE,
				dmem_bt_vx_dl,
				dmem_bt_vx_dl_size,
				1 * SCHED_LOOP_8kHz,
			}},
		},
		{0, 0},
		{0},
		"BT_VX_DL",
	},
	/* PDM_DL1 */
	{
		IDLE_P,
		{96000, SIX_MSB},
		NODRIFT,
		NOCALLBACK,
		0,
		0,
		{
			SRC_P,
			MCPDMDL_PORT_PROT,
			{{
				dmem_mcpdm,
				dmem_mcpdm_size,
			}},
		},
		{0, 0},
		{MIXDL1, EQ1, APS1, MIXDL2, EQ2L, EQ2R, APS2L, APS2R, 0},
		"PDM_DL",
	},
	/* MM_EXT_OUT */
	{
		IDLE_P,
		{48000, STEREO_MSB},
		NODRIFT,
		NOCALLBACK,
		smem_mm_ext_out,
		0,
		{
			SRC_P,
			SERIAL_PORT_PROT,
			{{
				MCBSP1_DMA_TX * ATC_SIZE,
				dmem_mm_ext_out,
				dmem_mm_ext_out_size,
				2 * SCHED_LOOP_48kHz,
			}},
		},
		{0, 0},
		{0},
		"MM_EXT_OUT",
	},
	/* MM_EXT_IN */
	{
		IDLE_P,
		{48000, STEREO_MSB},
		NODRIFT,
		NOCALLBACK,
		smem_mm_ext_in,
		0,
		{
			SRC_P,
			SERIAL_PORT_PROT,
			{{
				MCBSP1_DMA_RX * ATC_SIZE,
				dmem_mm_ext_in,
				dmem_mm_ext_in_size,
				2 * SCHED_LOOP_48kHz,
			}},
		},
		{0, 0},
		{0},
		"MM_EXT_IN",
	},
	/* PCM3_TX */
	{
		IDLE_P,
		{48000, STEREO_MSB},
		NODRIFT,
		NOCALLBACK,
		0,
		0,
		{
			SRC_P,
			TDM_SERIAL_PORT_PROT,
			{{
				MCBSP3_DMA_TX*ATC_SIZE,
				dmem_mm_ext_out,
				dmem_mm_ext_out_size,
				2 * SCHED_LOOP_48kHz,
			}}
		},
		{0, 0},
		{0},
		"MM_EXT_OUT",
	},
	/* PCM3_RX */
	{
		IDLE_P,
		{48000, STEREO_MSB},
		NODRIFT,
		NOCALLBACK,
		0,
		0,
		{
			SRC_P,
			TDM_SERIAL_PORT_PROT,
			{{
				MCBSP3_DMA_RX*ATC_SIZE,
				dmem_mm_ext_in,
				dmem_mm_ext_in_size,
				2 * SCHED_LOOP_48kHz
			}}
		},
		{0, 0},
		{0},
		"MM_EXT_IN"
	},
	/* SCHD_DBG_PORT */
	{
		IDLE_P,
		{48000, MONO_MSB},
		NODRIFT,
		NOCALLBACK,
		0,
		0,
		{
			SRC_P,
			DMAREQ_PORT_PROT,
			{{
				CBPr_DMA_RTX7*ATC_SIZE,
				dmem_mm_trace,
				dmem_mm_trace_size,
				2 * SCHED_LOOP_48kHz,
				ABE_DMASTATUS_RAW,
				(1<<4)
			}}
		},
		{
			CIRCULAR_BUFFER_PERIPHERAL_R__7,
			24
		},
		{
			SEQUENCE,
			CONTROL,
			GAINS,
			0
		},
		"SCHD_DBG"
	},
};

const abe_port_info_t abe_port_info [LAST_PORT_ID] = {
	/* DMIC */
	{
		ABE_OPP50,
		{SUB_WRITE_PORT_GAIN, {DMIC_PORT, MUTE_GAIN, 0, 0}},
		{0, {0, 0, 0, 0}}
	},
	/* PDM_UL */
	{
		ABE_OPP50,
		{SUB_WRITE_PORT_GAIN, {DMIC_PORT, MUTE_GAIN, 0, 0}},
		{0, {0, 0, 0, 0}}
	},
	/* BT_VX_UL */
	{
		ABE_OPP50,
		{SUB_WRITE_PORT_GAIN, {DMIC_PORT, MUTE_GAIN, 0, 0}},
		{0, {0, 0, 0, 0}}
	},
	/* MM_UL */
	{
		ABE_OPP50,
		{SUB_WRITE_PORT_GAIN, {DMIC_PORT, MUTE_GAIN, 0, 0}},
		{0, {0, 0, 0, 0}}
	},
	/* MM_UL2 */
	{
		ABE_OPP50,
		{SUB_WRITE_PORT_GAIN, {DMIC_PORT, MUTE_GAIN, 0, 0}},
		{0, {0, 0, 0, 0}}
	},
	/* VX_UL */
	{
		ABE_OPP50,
		{SUB_WRITE_PORT_GAIN, {DMIC_PORT, MUTE_GAIN, 0, 0}},
		{0, {0, 0, 0, 0}}
	},
	/* MM_DL */
	{
		ABE_OPP50,
		{SUB_WRITE_MIXER, {MM_DL_PORT, MUTE_GAIN, 0, 0}},
		{0, {0, 0, 0, 0}}
	},
	/* VX_DL */
	{
		ABE_OPP50,
		{SUB_WRITE_PORT_GAIN, {DMIC_PORT, MUTE_GAIN, 0, 0}},
		{0, {0, 0, 0, 0}}
	},
	/* TONES_DL */
	{
		ABE_OPP50,
		{SUB_WRITE_PORT_GAIN, {DMIC_PORT, MUTE_GAIN, 0, 0}},
		{0, {0, 0, 0, 0}}
	},
	/* VIB_DL */
	{
		ABE_OPP50,
		{SUB_WRITE_PORT_GAIN, {DMIC_PORT, MUTE_GAIN, 0, 0}},
		{0, {0, 0, 0, 0}}
	},
	/* BT_VX_DL */
	{
		ABE_OPP50,
		{SUB_WRITE_PORT_GAIN, {DMIC_PORT, MUTE_GAIN, 0, 0}},
		{0, {0, 0, 0, 0}}
	},
	/* PDM_DL */
	{
		ABE_OPP50,
		{SUB_WRITE_PORT_GAIN, {DMIC_PORT, MUTE_GAIN, 0, 0}},
		{0, {0, 0, 0, 0}}
	},
	/* MM_EXT_OUT */
	{
		ABE_OPP50,
		{SUB_WRITE_PORT_GAIN, {DMIC_PORT, MUTE_GAIN, 0, 0}},
		{0, {0, 0, 0, 0}}
	},
	/* MM_EXT_IN */
	{
		ABE_OPP50,
		{SUB_WRITE_PORT_GAIN, {DMIC_PORT, MUTE_GAIN, 0, 0}},
		{0, {0, 0, 0, 0}}
	},
	/*
	SCHD_DBG_PORT
	{
		ABE_OPP25,
		{SUB_WRITE_PORT_GAIN, {DMIC_PORT, MUTE_GAIN, 0, 0}},
		{0, {0, 0, 0, 0}}
	},
	*/
};

/*
 * Firmware features
 */
abe_feature_t all_feature[MAXNBFEATURE];

const abe_feature_t all_feature_init[] = {
/* on_reset, off, read, write, status, input, output, slots, opp, name */
	/* EQ1: equalizer downlink path headset + earphone */
	{
		c_feat_init_eq,
		c_feat_init_eq,
		c_feat_read_eq1,
		c_write_eq1,
		0,
		0x1000,
		0x1010,
		2,
		0,
		ABE_OPP25,
		"  DLEQ1",
	},
	/* EQ2L: equalizer downlink path integrated handsfree left */
	{
		c_feat_init_eq,
		c_feat_init_eq,
		c_feat_read_eq2,
		c_write_eq2,
		0,
		0x1000,
		0x1010,
		2,
		0,
		ABE_OPP100,
		"  DLEQ2L",
	},
	/* EQ2R: equalizer downlink path integrated handsfree right */
	{
		c_feat_init_eq,
		c_feat_init_eq,
		c_feat_read_eq3,
		c_write_eq3,
		0,
		0x1000,
		0x1010,
		2,
		0,
		ABE_OPP100,
		"  DLEQ2R",
	},
	/* EQSDT: equalizer downlink path side-tone */
	{
		c_feat_init_eq,
		c_feat_init_eq,
		c_feat_read_eq3,
		c_write_eq3,
		0,
		0x1000,
		0x1010,
		2,
		0,
		ABE_OPP50,
		"  EQSDT",
	},
	/* EQDMIC1: SRC+equalizer uplink DMIC 1st pair */
	{
		c_feat_init_eq,
		c_feat_init_eq,
		c_feat_read_eq3,
		c_write_eq3,
		0,
		0x1000,
		0x1010,
		2,
		0,
		ABE_OPP50,
		"  EQDMIC1",
	},
	/* EQDMIC2: SRC+equalizer uplink DMIC 2nd pair */
	{
		c_feat_init_eq,
		c_feat_init_eq,
		c_feat_read_eq3,
		c_write_eq3,
		0,
		0x1000,
		0x1010,
		2,
		0,
		ABE_OPP50,
		"  EQDMIC2",
	},
	/* EQDMIC3: SRC+equalizer uplink DMIC 3rd pair */
	{
		c_feat_init_eq,
		c_feat_init_eq,
		c_feat_read_eq3,
		c_write_eq3,
		0,
		0x1000,
		0x1010,
		2,
		0,
		ABE_OPP50,
		"  EQDMIC3",
	},
	/* EQAMIC: SRC+equalizer uplink AMIC */
	{
		c_feat_init_eq,
		c_feat_init_eq,
		c_feat_read_eq3,
		c_write_eq3,
		0,
		0x1000,
		0x1010,
		2,
		0,
		ABE_OPP50,
		"  EQAMIC",
	},
	/* APS1: Acoustic protection for headset */
	{
		c_feat_init_eq,
		c_feat_init_eq,
		c_feat_read_eq3,
		c_write_eq3,
		0,
		0x1000,
		0x1010,
		2,
		0,
		ABE_OPP25,
		"  APS1",
	},
	/* APS2: acoustic protection high-pass filter for handsfree left */
	{
		c_feat_init_eq,
		c_feat_init_eq,
		c_feat_read_eq3,
		c_write_eq3,
		0,
		0x1000,
		0x1010,
		2,
		0,
		ABE_OPP100,
		"  APS2",
	},
	/* APS3: acoustic protection high-pass filter for handsfree right */
	{
		c_feat_init_eq,
		c_feat_init_eq,
		c_feat_read_eq3,
		c_write_eq3,
		0,
		0x1000,
		0x1010,
		2,
		0,
		ABE_OPP100,
		"  APS3",
	},
	/* ASRC1: asynchronous sample-rate-converter for the downlink voice path */
	{
		c_feat_init_eq,
		c_feat_init_eq,
		c_feat_read_eq3,
		c_write_eq3,
		0,
		0x1000,
		0x1010,
		2,
		0,
		ABE_OPP50,
		"  ASRC_VXDL"
	},
	/* ASRC2: asynchronous sample-rate-converter for the uplink voice path */
	{
		c_feat_init_eq,
		c_feat_init_eq,
		c_feat_read_eq3,
		c_write_eq3,
		0,
		0x1000,
		0x1010,
		2,
		0,
		ABE_OPP50,
		"  ASRC_VXUL",
	},
	/* ASRC3: asynchronous sample-rate-converter for the multimedia player */
	{
		c_feat_init_eq,
		c_feat_init_eq,
		c_feat_read_eq3,
		c_write_eq3,
		0,
		0x1000,
		0x1010,
		2,
		0,
		ABE_OPP100,
		"  ASRC_MMDL",
	},
	/* ASRC4: asynchronous sample-rate-converter for the echo reference */
	{
		c_feat_init_eq,
		c_feat_init_eq,
		c_feat_read_eq3,
		c_write_eq3,
		0,
		0x1000,
		0x1010,
		2,
		0,
		ABE_OPP50,
		"  ASRC_ECHO",
	},
	/* MXDL1: mixer of the headset and earphone path */
	{
		c_feat_init_eq,
		c_feat_init_eq,
		c_feat_read_eq3,
		c_write_eq3,
		0,
		0x1000,
		0x1010,
		2,
		0,
		ABE_OPP25,
		"  MIX_DL1",
	},
	/* MXDL2: mixer of the hands-free path */
	{
		c_feat_init_eq,
		c_feat_init_eq,
		c_feat_read_eq3,
		c_write_eq3,
		0,
		0x1000,
		0x1010,
		2,
		0,
		ABE_OPP100,
		"  MIX_DL2",
	},
	/* MXAUDUL: mixer for uplink tone mixer */
	{
		c_feat_init_eq,
		c_feat_init_eq,
		c_feat_read_eq3,
		c_write_eq3,
		0,
		0x1000,
		0x1010,
		2,
		0,
		ABE_OPP50,
		"  MXSAUDUL",
	},
	/* MXVXREC: mixer for voice recording */
	{
		c_feat_init_eq,
		c_feat_init_eq,
		c_feat_read_eq3,
		c_write_eq3,
		0,
		0x1000,
		0x1010,
		2,
		0,
		ABE_OPP50,
		"  MXVXREC",
	},
	/* MXSDT: mixer for side-tone */
	{
		c_feat_init_eq,
		c_feat_init_eq,
		c_feat_read_eq3,
		c_write_eq3,
		0,
		0x1000,
		0x1010,
		2,
		0,
		ABE_OPP50,
		"  MIX_SDT",
	},
	/* MXECHO: mixer for echo reference */
	{
		c_feat_init_eq,
		c_feat_init_eq,
		c_feat_read_eq3,
		c_write_eq3,
		0,
		0x1000,
		0x1010,
		2,
		0,
		ABE_OPP50,
		"  MIX_ECHO",
	},
	/* UPROUTE: router of the uplink path */
	{
		c_feat_init_eq,
		c_feat_init_eq,
		c_feat_read_eq3,
		c_write_eq3,
		0,
		0x1000,
		0x1010,
		2,
		0,
		ABE_OPP50,
		"  DLEQ3",
	},
	/* GAINS: all gains */
	{
		c_feat_init_eq,
		c_feat_init_eq,
		c_feat_read_eq3,
		c_write_eq3,
		0,
		0x1000,
		0x1010,
		2,
		0,
		ABE_OPP25,
		"  DLEQ3",
	},
	/* EANC: active noise canceller */
	{
		c_feat_init_eq,
		c_feat_init_eq,
		c_feat_read_eq3,
		c_write_eq3,
		0,
		0x1000,
		0x1010,
		2,
		0,
		ABE_OPP100,
		"  DLEQ3",
	},
	/* SEQ: sequencing queue of micro tasks */
	{
		c_feat_init_eq,
		c_feat_init_eq,
		c_feat_read_eq3,
		c_write_eq3,
		0,
		0x1000,
		0x1010,
		2,
		0,
		ABE_OPP25,
		"  DLEQ3",
	},
	/* CTL: Phoenix control queue through McPDM */
	{
		c_feat_init_eq,
		c_feat_init_eq,
		c_feat_read_eq3,
		c_write_eq3,
		0,
		0x1000,
		0x1010,
		2,
		0,
		ABE_OPP25,
		"  DLEQ3",
	},
};

/*
 * Memory mapping of DMEM FIFOs
 */
abe_uint32 abe_map_dmem[LAST_PORT_ID];			/* DMEM port map */
abe_uint32 abe_map_dmem_secondary[LAST_PORT_ID];
abe_uint32 abe_map_dmem_size[LAST_PORT_ID];		/* DMEM port buffer sizes */

/*
 * AESS/ATC destination and source address translation
 * (except McASPs) from the original 64bits words address
 */
const abe_uint32 abe_atc_dstid[ABE_ATC_DESC_SIZE >> 3] = {
	/* DMA_0 DMIC  PDM_DL  PDM_UL  McB1TX  McB1RX  McB2TX  McB2RX  0-7 */
	0,      0,      12,     0,      1,      0,      2,      0,
	/* McB3TX  McB3RX  SLIMT0  SLIMT1  SLIMT2  SLIMT3  SLIMT4  SLIMT5	 8-15 */
	3,      0,      4,      5,      6,      7,      8,      9,
	/* SLIMT6  SLIMT7  SLIMR0  SLIMR1  SLIMR2  SLIMR3  SLIMR4  SLIMR5	16-23 */
	10,     11,     0,      0,      0,      0,      0,      0,
	/* SLIMR6  SLIMR7  McASP1X ------  ------  McASP1R -----   ------  24-31 */
	0,      0,      14,     0,      0,      0,      0,      0,
	/* CBPrT0  CBPrT1  CBPrT2  CBPrT3  CBPrT4  CBPrT5  CBPrT6  CBPrT7  32-39 */
	63,     63,     63,     63,     63,     63,     63,     63,
	/* CBP_T0  CBP_T1  CBP_T2  CBP_T3  CBP_T4  CBP_T5  CBP_T6  CBP_T7	40-47 */
	0,      0,      0,      0,      0,      0,      0,      0,
	/* CBP_T8  CBP_T9  CBP_T10 CBP_T11 CBP_T12 CBP_T13 CBP_T14 CBP_T15	48-63 */
	0,      0,      0,      0,      0,      0,      0,      0,
};

const abe_uint32 abe_atc_srcid[ABE_ATC_DESC_SIZE >> 3] = {
	/* DMA_0   DMIC    PDM_DL  PDM_UL  McB1TX  McB1RX  McB2TX  McB2RX   0-7 */
	0,      12,     0,      13,     0,      1,      0,      2,
	/* McB3TX  McB3RX  SLIMT0  SLIMT1  SLIMT2  SLIMT3  SLIMT4  SLIMT5   8-15 */
	0,      3,      0,      0,      0,      0,      0,      0,
	/* SLIMT6  SLIMT7  SLIMR0  SLIMR1  SLIMR2  SLIMR3  SLIMR4  SLIMR5  16-23 */
	0,      0,      4,      5,      6,      7,      8,      9,
	/* SLIMR6  SLIMR7  McASP1X ------  ------  McASP1R ------  ------  24-31 */
	10,     11,     0,      0,      0,      14,     0,      0,
	/* CBPrT0  CBPrT1  CBPrT2  CBPrT3  CBPrT4  CBPrT5  CBPrT6  CBPrT7  32-39 */
	63,     63,     63,     63,     63,     63,     63,     63,
	/* CBP_T0  CBP_T1  CBP_T2  CBP_T3  CBP_T4  CBP_T5  CBP_T6  CBP_T7  40-47 */
	0,      0,      0,      0,      0,      0,      0,      0,
	/* CBP_T8  CBP_T9  CBP_T10 CBP_T11 CBP_T12 CBP_T13 CBP_T14 CBP_T15 48-63 */
	0,      0,      0,      0,      0,      0,      0,      0,
};

/*
 * preset default routing configurations
 * This is given as implementation EXAMPLES
 * the programmer uses "abe_set_router_configuration" with its own tables
*/
const abe_router_t abe_router_ul_table_preset[NBROUTE_CONFIG][NBROUTE_UL] = {
	/* Voice uplink with Phoenix microphones - Uproute config_dmic1 */
	{
		/* 0  .. 9   = MM_UL */
		DMIC1_L_labelID,
		DMIC1_R_labelID,
		DMIC2_L_labelID,
		DMIC2_R_labelID,
		MM_EXT_IN_L_labelID,
		MM_EXT_IN_R_labelID,
		ZERO_labelID,
		ZERO_labelID,
		ZERO_labelID,
		ZERO_labelID,
		/* 10 .. 11  = MM_UL2 */
		AMIC_L_labelID,
		AMIC_R_labelID,
		/* 12 .. 13  = VX_UL */
		AMIC_L_labelID,
		AMIC_R_labelID,
		/* 14 .. 15  = RESERVED */
		ZERO_labelID,
		ZERO_labelID,
	},
	/* Voice uplink with the first DMIC pair - Uproute config_dmic2 */
	{
		/* 0  .. 9   = MM_UL */
		DMIC2_L_labelID,
		DMIC2_R_labelID,
		DMIC3_L_labelID,
		DMIC3_R_labelID,
		DMIC1_L_labelID,
		DMIC1_R_labelID,
		ZERO_labelID,
		ZERO_labelID,
		ZERO_labelID,
		ZERO_labelID,
		/* 10 .. 11  = MM_UL2 */
		DMIC1_L_labelID,
		DMIC1_R_labelID,
		/* 12 .. 13  = VX_UL */
		DMIC1_L_labelID,
		DMIC1_R_labelID,
		/* 14 .. 15  = RESERVED */
		ZERO_labelID,
		ZERO_labelID,
	},
	/* Voice uplink with the second DMIC pair - Uproute config_dmic3 */
	{
		/* 0  .. 9   = MM_UL */
		DMIC3_L_labelID,
		DMIC3_R_labelID,
		DMIC1_L_labelID,
		DMIC1_R_labelID,
		DMIC2_L_labelID,
		DMIC2_R_labelID,
		ZERO_labelID,
		ZERO_labelID,
		ZERO_labelID,
		ZERO_labelID,
		/* 10 .. 11  = MM_UL2 */
		DMIC2_L_labelID,
		DMIC2_R_labelID,
		/* 12 .. 13  = VX_UL */
		DMIC2_L_labelID,
		DMIC2_R_labelID,
		/* 14 .. 15  = RESERVED */
		ZERO_labelID,
		ZERO_labelID,
	},
	/* VOICE UPLINK WITH THE LAST DMIC PAIR - UPROUTE_CONFIG_DMIC3 */
	{
		AMIC_L_labelID,	/* 0  .. 9 = MM_UL */
		AMIC_R_labelID,
		DMIC2_L_labelID,
		DMIC2_R_labelID,
		DMIC3_L_labelID,
		DMIC3_R_labelID,
		ZERO_labelID,
		ZERO_labelID,
		ZERO_labelID,
		ZERO_labelID,
		DMIC3_L_labelID,
		DMIC3_R_labelID,	/* 10 .. 11  = MM_UL2 */
		DMIC3_L_labelID,
		DMIC3_R_labelID,	/* 12 .. 13  = VX_UL */
		ZERO_labelID,
		ZERO_labelID,		/* 14 .. 15  = RESERVED */
	},
	/* VOICE UPLINK WITH THE BT - UPROUTE_CONFIG_BT */
	{
		BT_UL_L_labelID,
		BT_UL_R_labelID,
		DMIC2_L_labelID,
		DMIC2_R_labelID,	/* 0  .. 9	 = MM_UL */
		DMIC3_L_labelID,
		DMIC3_R_labelID,
		DMIC1_L_labelID,
		DMIC1_R_labelID,
		ZERO_labelID,
		ZERO_labelID,
		AMIC_L_labelID,
		AMIC_R_labelID,		/* 10 .. 11  = MM_UL2 */
		BT_UL_L_labelID,
		BT_UL_R_labelID,	/* 12 .. 13  = VX_UL */
		ZERO_labelID,
		ZERO_labelID,		/* 14 .. 15  = RESERVED */
	},
};

/* all default routing configurations */
abe_router_t abe_router_ul_table[NBROUTE_CONFIG_MAX][NBROUTE_UL];

/*
 * ABE_GLOBAL DATA
 */
/* flag, indicates the allowed control of Phoenix through McPDM slot 6 */
abe_uint32 abe_global_mcpdm_control;
abe_event_id abe_current_event_id;

/*
 * ABE SUBROUTINES AND SEQUENCES
 */

/*
const abe_seq_t abe_seq_array [MAXNBSEQUENCE] [MAXSEQUENCESTEPS] =
	   {{0,      0,      0,      0}, {-1, 0, 0, 0}},
	   {{0,      0,      0,      0}, {-1, 0, 0, 0}},
const seq_t setup_hw_sequence2 [ ] = { 0, C_AE_FUNC1,  0, 0, 0, 0,
				      -1, C_CALLBACK1, 0, 0, 0, 0 };

const abe_subroutine2 abe_sub_array [MAXNBSUBROUTINE] =
	    abe_init_atc,   0,      0,
	    abe_init_atc,   0,      0,

 typedef double (*PtrFun) (double);
PtrFun pFun;
pFun = sin;
y = (* pFun) (x);
*/

const abe_sequence_t seq_null =	{
	NOMASK,
	{
		CL_M1,
		0,
		{0, 0, 0, 0},
		0,
	},
	{
		CL_M1,
		0,
		{0, 0, 0, 0},
		0,
	},
};

/* table of new subroutines called in the sequence */
abe_subroutine2 abe_all_subsubroutine[MAXNBSUBROUTINE];
/* number of parameters per calls */
abe_uint32 abe_all_subsubroutine_nparam[MAXNBSUBROUTINE];
/* index of the subroutine */
abe_uint32 abe_subroutine_id[MAXNBSUBROUTINE];
abe_uint32* abe_all_subroutine_params[MAXNBSUBROUTINE];
abe_uint32 abe_subroutine_write_pointer;

/* table of all sequences */
abe_sequence_t abe_all_sequence[MAXNBSEQUENCE];

abe_uint32 abe_sequence_write_pointer;

/* current number of pending sequences (avoids to look in the table) */
abe_uint32 abe_nb_pending_sequences;

/* pending sequences due to ressource collision */
abe_uint32 abe_pending_sequences[MAXNBSEQUENCE];

/* mask of unsharable ressources among other sequences */
abe_uint32 abe_global_sequence_mask;

/* table of active sequences */
abe_seq_t abe_active_sequence[MAXACTIVESEQUENCE][MAXSEQUENCESTEPS];

/* index of the plugged subroutine doing ping-pong cache-flush DMEM accesses */
abe_uint32 abe_irq_pingpong_player_id;
/* index of the plugged subroutine doing acoustics protection adaptation */
abe_uint32 abe_irq_aps_adaptation_id;

/* base addresses of the ping pong buffers in bytes addresses */
abe_uint32 abe_base_address_pingpong[MAX_PINGPONG_BUFFERS];

/* size of each ping/pong buffers */
abe_uint32 abe_size_pingpong;

/* number of ping/pong buffer being used */
abe_uint32 abe_nb_pingpong;

/*
 * ABE CONST AREA FOR PARAMETERS TRANSLATION
 */
const abe_uint32 abe_db2lin_table [sizeof_db2lin_table] = {
	0x00000000,	/* SMEM coding of -120 dB */
	0x00000000,	/* SMEM coding of -119 dB */
	0x00000000,	/* SMEM coding of -118 dB */
	0x00000000,	/* SMEM coding of -117 dB */
	0x00000000,	/* SMEM coding of -116 dB */
	0x00000000,	/* SMEM coding of -115 dB */
	0x00000000,	/* SMEM coding of -114 dB */
	0x00000000,	/* SMEM coding of -113 dB */
	0x00000000,	/* SMEM coding of -112 dB */
	0x00000000,	/* SMEM coding of -111 dB */
	0x00000000,	/* SMEM coding of -110 dB */
	0x00000000,	/* SMEM coding of -109 dB */
	0x00000001,	/* SMEM coding of -108 dB */
	0x00000001,	/* SMEM coding of -107 dB */
	0x00000001,	/* SMEM coding of -106 dB */
	0x00000001,	/* SMEM coding of -105 dB */
	0x00000001,	/* SMEM coding of -104 dB */
	0x00000001,	/* SMEM coding of -103 dB */
	0x00000002,	/* SMEM coding of -102 dB */
	0x00000002,	/* SMEM coding of -101 dB */
	0x00000002,	/* SMEM coding of -100 dB */
	0x00000002,	/* SMEM coding of  -99 dB */
	0x00000003,	/* SMEM coding of  -98 dB */
	0x00000003,	/* SMEM coding of  -97 dB */
	0x00000004,	/* SMEM coding of  -96 dB */
	0x00000004,	/* SMEM coding of  -95 dB */
	0x00000005,	/* SMEM coding of  -94 dB */
	0x00000005,	/* SMEM coding of  -93 dB */
	0x00000006,	/* SMEM coding of  -92 dB */
	0x00000007,	/* SMEM coding of  -91 dB */
	0x00000008,	/* SMEM coding of  -90 dB */
	0x00000009,	/* SMEM coding of  -89 dB */
	0x0000000A,	/* SMEM coding of  -88 dB */
	0x0000000B,	/* SMEM coding of  -87 dB */
	0x0000000D,	/* SMEM coding of  -86 dB */
	0x0000000E,	/* SMEM coding of  -85 dB */
	0x00000010,	/* SMEM coding of  -84 dB */
	0x00000012,	/* SMEM coding of  -83 dB */
	0x00000014,	/* SMEM coding of  -82 dB */
	0x00000017,	/* SMEM coding of  -81 dB */
	0x0000001A,	/* SMEM coding of  -80 dB */
	0x0000001D,	/* SMEM coding of  -79 dB */
	0x00000021,	/* SMEM coding of  -78 dB */
	0x00000025,	/* SMEM coding of  -77 dB */
	0x00000029,	/* SMEM coding of  -76 dB */
	0x0000002E,	/* SMEM coding of  -75 dB */
	0x00000034,	/* SMEM coding of  -74 dB */
	0x0000003A,	/* SMEM coding of  -73 dB */
	0x00000041,	/* SMEM coding of  -72 dB */
	0x00000049,	/* SMEM coding of  -71 dB */
	0x00000052,	/* SMEM coding of  -70 dB */
	0x0000005D,	/* SMEM coding of  -69 dB */
	0x00000068,	/* SMEM coding of  -68 dB */
	0x00000075,	/* SMEM coding of  -67 dB */
	0x00000083,	/* SMEM coding of  -66 dB */
	0x00000093,	/* SMEM coding of  -65 dB */
	0x000000A5,	/* SMEM coding of  -64 dB */
	0x000000B9,	/* SMEM coding of  -63 dB */
	0x000000D0,	/* SMEM coding of  -62 dB */
	0x000000E9,	/* SMEM coding of  -61 dB */
	0x00000106,	/* SMEM coding of  -60 dB */
	0x00000126,	/* SMEM coding of  -59 dB */
	0x0000014A,	/* SMEM coding of  -58 dB */
	0x00000172,	/* SMEM coding of  -57 dB */
	0x0000019F,	/* SMEM coding of  -56 dB */
	0x000001D2,	/* SMEM coding of  -55 dB */
	0x0000020B,	/* SMEM coding of  -54 dB */
	0x0000024A,	/* SMEM coding of  -53 dB */
	0x00000292,	/* SMEM coding of  -52 dB */
	0x000002E2,	/* SMEM coding of  -51 dB */
	0x0000033C,	/* SMEM coding of  -50 dB */
	0x000003A2,	/* SMEM coding of  -49 dB */
	0x00000413,	/* SMEM coding of  -48 dB */
	0x00000492,	/* SMEM coding of  -47 dB */
	0x00000521,	/* SMEM coding of  -46 dB */
	0x000005C2,	/* SMEM coding of  -45 dB */
	0x00000676,	/* SMEM coding of  -44 dB */
	0x0000073F,	/* SMEM coding of  -43 dB */
	0x00000822,	/* SMEM coding of  -42 dB */
	0x00000920,	/* SMEM coding of  -41 dB */
	0x00000A3D,	/* SMEM coding of  -40 dB */
	0x00000B7D,	/* SMEM coding of  -39 dB */
	0x00000CE4,	/* SMEM coding of  -38 dB */
	0x00000E76,	/* SMEM coding of  -37 dB */
	0x0000103A,	/* SMEM coding of  -36 dB */
	0x00001235,	/* SMEM coding of  -35 dB */
	0x0000146E,	/* SMEM coding of  -34 dB */
	0x000016EC,	/* SMEM coding of  -33 dB */
	0x000019B8,	/* SMEM coding of  -32 dB */
	0x00001CDC,	/* SMEM coding of  -31 dB */
	0x00002061,	/* SMEM coding of  -30 dB */
	0x00002455,	/* SMEM coding of  -29 dB */
	0x000028C4,	/* SMEM coding of  -28 dB */
	0x00002DBD,	/* SMEM coding of  -27 dB */
	0x00003352,	/* SMEM coding of  -26 dB */
	0x00003995,	/* SMEM coding of  -25 dB */
	0x0000409C,	/* SMEM coding of  -24 dB */
	0x0000487E,	/* SMEM coding of  -23 dB */
	0x00005156,	/* SMEM coding of  -22 dB */
	0x00005B43,	/* SMEM coding of  -21 dB */
	0x00006666,	/* SMEM coding of  -20 dB */
	0x000072E5,	/* SMEM coding of  -19 dB */
	0x000080E9,	/* SMEM coding of  -18 dB */
	0x000090A4,	/* SMEM coding of  -17 dB */
	0x0000A24B,	/* SMEM coding of  -16 dB */
	0x0000B618,	/* SMEM coding of  -15 dB */
	0x0000CC50,	/* SMEM coding of  -14 dB */
	0x0000E53E,	/* SMEM coding of  -13 dB */
	0x00010137,	/* SMEM coding of  -12 dB */
	0x0001209A,	/* SMEM coding of  -11 dB */
	0x000143D1,	/* SMEM coding of  -10 dB */
	0x00016B54,	/* SMEM coding of   -9 dB */
	0x000197A9,	/* SMEM coding of   -8 dB */
	0x0001C967,	/* SMEM coding of   -7 dB */
	0x00020137,	/* SMEM coding of   -6 dB */
	0x00023FD6,	/* SMEM coding of   -5 dB */
	0x00028619,	/* SMEM coding of   -4 dB */
	0x0002D4EF,	/* SMEM coding of   -3 dB */
	0x00032D64,	/* SMEM coding of   -2 dB */
	0x000390A4,	/* SMEM coding of   -1 dB */
	0x00040000,	/* SMEM coding of    0 dB */
	0x00047CF2,	/* SMEM coding of    1 dB */
	0x00050923,	/* SMEM coding of    2 dB */
	0x0005A670,	/* SMEM coding of    3 dB */
	0x000656EE,	/* SMEM coding of    4 dB */
	0x00071CF5,	/* SMEM coding of    5 dB */
	0x0007FB26,	/* SMEM coding of    6 dB */
	0x0008F473,	/* SMEM coding of    7 dB */
	0x000A0C2B,	/* SMEM coding of    8 dB */
	0x000B4606,	/* SMEM coding of    9 dB */
	0x000CA62C,	/* SMEM coding of   10 dB */
	0x000E314A,	/* SMEM coding of   11 dB */
	0x000FEC9E,	/* SMEM coding of   12 dB */
	0x0011DE0A,	/* SMEM coding of   13 dB */
	0x00140C28,	/* SMEM coding of   14 dB */
	0x00167E60,	/* SMEM coding of   15 dB */
	0x00193D00,	/* SMEM coding of   16 dB */
	0x001C515D,	/* SMEM coding of   17 dB */
	0x001FC5EB,	/* SMEM coding of   18 dB */
	0x0023A668,	/* SMEM coding of   19 dB */
	0x00280000,	/* SMEM coding of   20 dB */
	0x002CE178,	/* SMEM coding of   21 dB */
	0x00325B65,	/* SMEM coding of   22 dB */
	0x00388062,	/* SMEM coding of   23 dB */
	0x003F654E,	/* SMEM coding of   24 dB */
	0x00472194,	/* SMEM coding of   25 dB */
	0x004FCF7C,	/* SMEM coding of   26 dB */
	0x00598C81,	/* SMEM coding of   27 dB */
	0x006479B7,	/* SMEM coding of   28 dB */
	0x0070BC3D,	/* SMEM coding of   29 dB */
	0x007E7DB9,	/* SMEM coding of   30 dB */
};


const abe_uint32 abe_sin_table [] = { 0 };
/*
 * ABE_DEBUG DATA
 */

/*
 * IRQ and trace pointer in DMEM:
 * FW updates a write pointer at "MCU_IRQ_FIFO_ptr_labelID", the read pointer is in HAL
 */
abe_uint32 abe_irq_dbg_read_ptr;

/* General circular buffer used to trace APIs calls and AE activity */
abe_uint32 abe_dbg_activity_log[D_DEBUG_HAL_TASK_sizeof];
abe_uint32 abe_dbg_activity_log_write_pointer;
abe_uint32 abe_dbg_mask;

/* Global variable holding parameter errors */
abe_uint32 abe_dbg_param;

/* Output of messages selector */
abe_uint32 abe_dbg_output;

/* Last parameters */
#define SIZE_PARAM      10

abe_uint32 param1[SIZE_PARAM];
abe_uint32 param2[SIZE_PARAM];
abe_uint32 param3[SIZE_PARAM];
abe_uint32 param4[SIZE_PARAM];
abe_uint32 param5[SIZE_PARAM];

volatile abe_uint32 just_to_avoid_the_many_warnings;
volatile abe_gain_t just_to_avoid_the_many_warnings_abe_gain_t;
volatile abe_ramp_t just_to_avoid_the_many_warnings_abe_ramp_t;
volatile abe_dma_t just_to_avoid_the_many_warnings_abe_dma_t;
volatile abe_port_id just_to_avoid_the_many_warnings_abe_port_id;
volatile abe_millis_t just_to_avoid_the_many_warnings_abe_millis_t;
volatile abe_micros_t just_to_avoid_the_many_warnings_abe_micros_t;
volatile abe_patch_rev just_to_avoid_the_many_warnings_abe_patch_rev;
volatile abe_sequence_t just_to_avoid_the_many_warnings_abe_sequence_t;
volatile abe_ana_port_id just_to_avoid_the_many_warnings_abe_ana_port_id;
volatile abe_time_stamp_t just_to_avoid_the_many_warnings_abe_time_stamp_t;
volatile abe_data_format_t just_to_avoid_the_many_warnings_abe_data_format_t;
volatile abe_port_protocol_t just_to_avoid_the_many_warnings_abe_port_protocol_t;
volatile abe_router_t just_to_avoid_the_many_warnings_abe_router_t;
volatile abe_router_id just_to_avoid_the_many_warnings_abe_router_id;

#ifdef __cplusplus
}
#endif

#endif	/* _ABE_DAT_H_ */
