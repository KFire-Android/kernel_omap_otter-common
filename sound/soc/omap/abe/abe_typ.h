/*
 * ==========================================================================
 *               Texas Instruments OMAP(TM) Platform Firmware
 * (c) Copyright 2009, Texas Instruments Incorporated.  All Rights Reserved.
 *
 *  Use of this firmware is controlled by the terms and conditions found
 *  in the license agreement under which this firmware has been supplied.
 * ==========================================================================
 */

#include "abe_def.h"
#include "abe_ext.h"
#include "abe_initxxx_labels.h"

#ifndef _ABE_TYP_H_
#define _ABE_TYP_H_

#ifdef __cplusplus
extern "C" {
#endif

/*
 * BASIC TYPES
 */

#define MAX_UINT8	((((1L <<  7) -1)<<1) +1)
#define MAX_UINT16	((((1L << 15) -1)<<1) +1)
#define MAX_UINT32	((((1L << 31) -1)<<1) +1)

typedef char	    abe_flag;
typedef unsigned char   abe_uint8;
typedef char	    abe_int8;
typedef unsigned short  abe_uint16;
typedef short	   abe_int16;
typedef unsigned long   abe_uint32;
typedef long	    abe_int32;
typedef float	   abe_float;
typedef double	  abe_double;

typedef abe_uint32 abe_errc_t;
typedef abe_int32  abe_millibel;

//typedef abe_uint32  abe_millisecond;
//typedef abe_uint32  abe_milliHertz;
//typedef abe_uint32  abe_millimeter;
//typedef abe_uint32  abe_millidegree;
//typedef abe_uint32  abe_permille;
//typedef abe_uint32  abe_microsecond;

typedef abe_uint32  abe_result;
typedef abe_millibel abe_gain_t;	/* smoothed gain amplitude and ramp */
typedef abe_uint32  abe_ramp_t;

typedef abe_uint32  abe_freq_t;		/* 4 bytes hertz */
typedef abe_uint32  abe_millis_t;	/* 4 bytes milliseconds	    */
typedef abe_uint32  abe_micros_t;	/* 4 bytes microseconds	    */

typedef abe_uint32  abe_dbg_mask_t;	/* 4 bytes bit field indicating the type of informations to be traced */
typedef abe_uint32  abe_time_stamp_t;	/* 4 bytes infinite loop 32bits counter incremented on each firmware loop */

/* scheduling task loops (250us / 272us with respectively 48kHz / 44.1kHz on Phoenix). */
typedef abe_uint32   abe_dbg_t;		 /* debug filter */

typedef abe_uint32  abe_seq_code_t;	     /* Index to the table of sequences */
typedef abe_uint32  abe_sub_code_t;	     /* Index to the table of subroutines called in the sequence */

typedef void (* abe_subroutine0)(void);					    /* subroutine with no parameter */
typedef void (* abe_subroutine1)(abe_uint32);				      /* subroutine with one parameter */
typedef void (* abe_subroutine2)(abe_uint32, abe_uint32);			  /* subroutine with two parameters */
typedef void (* abe_subroutine3)(abe_uint32, abe_uint32, abe_uint32);	      /* subroutine with three parameters */
typedef void (* abe_subroutine4)(abe_uint32, abe_uint32, abe_uint32, abe_uint32);  /* subroutine with four parameters */

/*
 *      CODE PORTABILITY - FUTURE PATCHES
 *
 *      32bits field for having the code compatible with future revisions of the hardware (audio integration)
 *      or evolution of the software partitionning. Used for the highest level APIs (launch_sequences)
 */
typedef abe_uint32 abe_patch_rev;

/*
 * ENUMS
 */

/*
 *      MEMORY CONFIG TYPE
 *
 *	 0: Ultra Lowest power consumption audio player
 *	 1: OPP 25% (simple multimedia features)
 *	 2: OPP 50% (multimedia and voice calls)
 *	 3: OPP100% (EANC, multimedia complex use-cases)
 */
typedef enum {
	ABE_AUDIO_PLAYER_ON_HEADSET_OR_EARPHONE = 1,
	ABE_DRIFT_MANAGEMENT_FOR_AUDIO_PLAYER,
	ABE_DRIFT_MANAGEMENT_FOR_VOICE_CALL,
	ABE_VOICE_CALL_ON_HEADSET_OR_EARPHONE_OR_BT,
	ABE_MULTIMEDIA_AUDIO_RECORDER,
	ABE_VIBRATOR_OR_HAPTICS,
	ABE_VOICE_CALL_ON_HANDS_FREE_SPEAKER,
	ABE_RINGER_TONES,
	ABE_VOICE_CALL_WITH_EARPHONE_ACTIVE_NOISE_CANCELLER,

	ABE_LAST_USE_CASE
} abe_use_case_id;

/*
 *      OPP TYPE
 *
 *	 0: Ultra Lowest power consumption audio player
 *	 1: OPP 25% (simple multimedia features)
 *	 2: OPP 50% (multimedia and voice calls)
 *	 3: OPP100% (EANC, multimedia complex use-cases)
 */
typedef enum {
	ABE_OPP0 = 0,
	ABE_OPP25, ABE_OPP50, ABE_OPP100
} abe_opp_t;

/*
 *      IIR TYPE
 *
 *	 0: Ultra Lowest power consumption audio player
 *	 1: OPP 25% (simple multimedia features)
 */
typedef enum {
	ABE_IIR_TYPE_1 = 0,
	ABE_IIR_TYPE_2
} abe_iir_t;

/*
 *      DMIC DECIMATION RATIO
 *
 */
typedef enum {
	ABE_DEC16 = 16,
	ABE_DEC25 = 25,
	ABE_DEC32 = 32,
	ABE_DEC40 = 40
} abe_dmic_ratio_t;

/*
 *      SAMPLES TYPE
 *
 *	mono 16bits sample LSB aligned, 16 MSB bits are unused
 *	mono right shifted to 16bits LSBs on a 32bits DMEM FIFO for McBSP TX purpose.
 *	mono sample MSB aligned (16/24/32bits)
 *      two successive mono samples in one 32bits container
 *	Two L/R 16bits samples in a 32bits container,
 *	Two channels defined with two MSB aligned samples
 *	Three channels defined with three MSB aligned samples (MIC)
 *	Four channels defined with four MSB aligned samples (MIC)
 *      . . .
 *	Eight channels defined with six MSB aligned samples (MIC)
 */
typedef enum {
	MONO_MSB = 1,
	MONO_RSHIFTED_16, STEREO_RSHIFTED_16,  /* only used for McBSP_TX */
	STEREO_16_16,
	STEREO_MSB, THREE_MSB, FOUR_MSB, FIVE_MSB, SIX_MSB, SEVEN_MSB, EIGHT_MSB, NINE_MSB, TEN_MSB,
} abe_samp_t;

/*
 *      PORT PROTOCOL TYPE
 */
typedef enum {
	SLIMBUS_PORT_PROT = 1,
	SERIAL_PORT_PROT,
	TDM_SERIAL_PORT_PROT,
	DMIC_PORT_PROT,
	MCPDMDL_PORT_PROT,
	MCPDMUL_PORT_PROT,
	PINGPONG_PORT_PROT,
	DMAREQ_PORT_PROT,
} abe_port_protocol_switch_id;

/*
 * PORT IDs, this list is aligned with the FW data mapping
 */
typedef enum {
	DMIC_PORT = 0,
	PDM_UL_PORT,	    /* analog MICs */
	BT_VX_UL_PORT,	  /* BT uplink (8/16 kHz)*/

	/* AE source ports - Uplink */
	MM_UL_PORT,	     /* up to 5 stereo channels */
	MM_UL2_PORT,	    /* stereo FM record path (4) */
	VX_UL_PORT,	     /* stereo FM record path */

	/* AE sink ports - Downlink */
	MM_DL_PORT,	     /* multimedia player audio path */
	VX_DL_PORT,
	TONES_DL_PORT, /* 8 */
	VIB_DL_PORT,

	/* AE source ports - Downlink */
	BT_VX_DL_PORT,
	PDM_DL_PORT,	/* ABE --> BT (8/16kHz) */
	MM_EXT_OUT_PORT, /* 12 */
	MM_EXT_IN_PORT,
	TDM_DL_PORT,
	TDM_UL_PORT,
	DEBUG_PORT,	/* 16 */

	LAST_PORT_ID /* dummy port used to declare the other tasks of the scheduler */
} abe_port_id;

/*
 * Definition for the compatibility with HAL05xx
 */
#define PDM_DL1_PORT PDM_DL_PORT
#define PDM_DL2_PORT PDM_DL_PORT
#define PDM_VIB_PORT PDM_DL_PORT
#define DMIC_PORT1 DMIC_PORT
#define DMIC_PORT2 DMIC_PORT
#define DMIC_PORT3 DMIC_PORT

/*
 *      ABE_DL_SRC_ID     source of samples
 */

typedef enum {
	SRC_DL1_MIXER_OUTPUT	= DL1_M_labelID,
	SRC_SDT_MIXER_OUTPUT	= SDT_M_labelID,
	SRC_DL1_GAIN_OUTPUT	= DL1_GAIN_out_labelID,
	SRC_DL1_EQ_OUTPUT	= DL1_EQ_labelID,
	SRC_DL2_GAIN_OUTPUT	= DL2_GAIN_out_labelID,
	SRC_DL2_EQ_OUTPUT	= DL2_EQ_labelID,
	SRC_MM_DL		= MM_DL_labelID,
	SRC_TONES_DL		= Tones_labelID,
	SRC_VX_DL		= VX_DL_labelID,
	SRC_VX_UL		= VX_UL_labelID,
	SRC_MM_UL2		= MM_UL2_labelID,
	SRC_MM_UL		= MM_UL_labelID,
} abe_dl_src_id;


/*
 *      ANA_PORT_ID     Analog companion audio port
 */
typedef enum {
	EAR_PHOENIX = 1,
	HS_L,
	HS_R,
	IHF_L,
	IHF_R,
	VIBRA1,
	VIBRA2
} abe_ana_port_id ;

/*
 *  abe_patched_pattern_id: selection of the audio engine
 *  signal to replace by a precomputed pattern
 */
typedef enum {
	DBG_PATCH_AMIC = 1,
	DBG_PATCH_DMIC1,
	DBG_PATCH_DMIC2,
	DBG_PATCH_DMIC3,
	DBG_PATCH_VX_REC,
	DBG_PATCH_BT_UL,
	DBG_PATCH_MM_DL,
	DBG_PATCH_DL2_EQ,
	DBG_PATCH_VIBRA,
	DBG_PATCH_MM_EXT_IN,
	DBG_PATCH_EANC_FBK_Out,
	DBG_PATCH_MIC4,
	DBG_PATCH_MM_DL_MIXDL1,
	DBG_PATCH_MM_DL_MIXDL2,
} abe_patched_pattern_id ;

typedef abe_int32 headset_offset_t;   /* Calibration data from the analog companion */

/*
 *      Signal processing module names - EQ APS MIX ROUT
 */
#define FEAT_EQ1	1		/* equalizer downlink path headset + earphone */
#define FEAT_EQ2L	FEAT_EQ1+1	/* equalizer downlink path integrated handsfree LEFT */
#define FEAT_EQ2R	FEAT_EQ2L+1	/* equalizer downlink path integrated handsfree RIGHT */
#define FEAT_EQSDT	FEAT_EQ2R+1	/* equalizer downlink path side-tone */
#define FEAT_EQMIC	FEAT_EQSDT+1	/* equalizer uplink path first DMIC pair */
#define FEAT_APS1	FEAT_EQMIC+1	/* Acoustic protection for headset */
#define FEAT_APS2	FEAT_APS1+1	/* acoustic protection high-pass filter for handsfree "Left" */
#define FEAT_APS3	FEAT_APS2+1	/* acoustic protection high-pass filter for handsfree "Right" */
#define FEAT_ASRC1	FEAT_APS3+1	/* asynchronous sample-rate-converter for the downlink voice path */
#define FEAT_ASRC2	FEAT_ASRC1+1	/* asynchronous sample-rate-converter for the uplink voice path */
#define FEAT_ASRC3	FEAT_ASRC2+1	/* asynchronous sample-rate-converter for the multimedia player */
#define FEAT_ASRC4	FEAT_ASRC3+1	/* asynchronous sample-rate-converter for the echo reference */
#define FEAT_MIXDL1	FEAT_ASRC4+1	/* mixer of the headset and earphone path */
#define FEAT_MIXDL2	FEAT_MIXDL1+1	/* mixer of the hands-free path */
#define FEAT_MIXAUDUL	FEAT_MIXDL2+1	/* mixer for audio being sent on the voice_ul path */
#define FEAT_MIXVXREC	FEAT_MIXAUDUL+1	/* mixer for voice communication recording */
#define FEAT_MIXSDT	FEAT_MIXVXREC+1	/* mixer for side-tone */
#define FEAT_MIXECHO	FEAT_MIXSDT+1	/* mixer for echo reference */
#define FEAT_UPROUTE	FEAT_MIXECHO+1	/* router of the uplink path */
#define FEAT_GAINS	FEAT_UPROUTE+1	/* all gains */
#define FEAT_GAINS_DMIC1 FEAT_GAINS+1
#define FEAT_GAINS_DMIC2 FEAT_GAINS_DMIC1+1
#define FEAT_GAINS_DMIC3 FEAT_GAINS_DMIC2+1
#define FEAT_GAINS_AMIC FEAT_GAINS_DMIC3+1
#define FEAT_GAINS_SPLIT	FEAT_GAINS_AMIC+1
#define FEAT_GAINS_DL1	FEAT_GAINS_SPLIT+1
#define FEAT_GAINS_DL2	FEAT_GAINS_DL1+1
#define FEAT_GAIN_EANC	FEAT_GAINS_DL2+1 /* active noise canceller */
#define FEAT_SEQ	FEAT_GAIN_EANC+1	/* sequencing queue of micro tasks */
#define FEAT_CTL	FEAT_SEQ+1	/* Phoenix control queue through McPDM */

#define MAXNBFEATURE	FEAT_CTL	/* list of features of the firmware */

typedef enum {
	EQ1 =   FEAT_EQ1,	/* equalizer downlink path headset + earphone */
	EQ2L =  FEAT_EQ2L,	/* equalizer downlink path integrated handsfree LEFT */
	EQ2R =  FEAT_EQ2R,
	EQSDT = FEAT_EQSDT,	/* equalizer downlink path side-tone */
	EQMIC = FEAT_EQMIC,
} abe_equ_id;

typedef enum {
	APS1 = FEAT_APS1,	/* Acoustic protection for headset */
	APS2L = FEAT_APS2,
	APS2R = FEAT_APS3
} abe_aps_id;

typedef enum {
	ASRC1 = FEAT_ASRC1,	/* asynchronous sample-rate-converter for the downlink voice path */
	ASRC2 = FEAT_ASRC2,	/* asynchronous sample-rate-converter for the uplink voice path */
	ASRC3 = FEAT_ASRC3,	/* asynchronous sample-rate-converter for the multimedia player */
	ASRC4 = FEAT_ASRC4,	/* asynchronous sample-rate-converter for the voice uplink echo_reference */
} abe_asrc_id;

typedef enum {
	MIXDL1 = FEAT_MIXDL1,
	MIXDL2 = FEAT_MIXDL2,
	MIXSDT = FEAT_MIXSDT,
	MIXECHO = FEAT_MIXECHO,
	MIXEANC = FEAT_GAIN_EANC,
	MIXAUDUL = FEAT_MIXAUDUL,
	MIXVXREC = FEAT_MIXVXREC,
} abe_mixer_id;

typedef enum {
	UPROUTE = FEAT_UPROUTE,	/* there is only one router up to now */
} abe_router_id;

typedef enum {
	GAINS = FEAT_GAINS,	/* Misc tasks of the scheduler */
	SEQUENCE = FEAT_SEQ,
	CONTROL = FEAT_CTL
} abe_schd_id;

/*
 * GAIN IDs
 */
typedef enum {
	GAINS_DMIC1 = FEAT_GAINS_DMIC1,
	GAINS_DMIC2 = FEAT_GAINS_DMIC2,
	GAINS_DMIC3 = FEAT_GAINS_DMIC3,
	GAINS_AMIC = FEAT_GAINS_AMIC,
	GAINS_SPLIT = FEAT_GAINS_SPLIT,
	GAINS_DL1 = FEAT_GAINS_DL1,
	GAINS_DL2 = FEAT_GAINS_DL2,
	GAINS_EANC = FEAT_GAIN_EANC,
} abe_gain_id;

#if 0
typedef enum {
	VX_DL_IN_GAIN = 1,	/* mixer's gain */
	MM_DL_IN_GAIN,
	TONES_DL_IN_GAIN,
	MM_VX_DL_IN_GAIN,
	MM_IHF_DL_IN_GAIN,	/* mixer's gain */
	MM_HS_DL_OUT_GAIN,	/* Output Left gain */
	MM_IHF_L_DL_OUT_GAIN,	/* Output Left gain */
	MM_IHF_R_DL_OUT_GAIN,	/* Output Right gain */
	MM_VIB1_DL_GAIN,
	MM_VIB2_DL_GAIN,	/* no gain in fact */
	DMIC_UL_IN_GAIN_0,
	DMIC_UL_IN_GAIN_1,	/* today = same GAIN on DMIC pairs */
	DMIC_UL_IN_GAIN_2,
	DMIC_UL_IN_GAIN_3,
	DMIC_UL_IN_GAIN_4,
	DMIC_UL_IN_GAIN_5,
	AMIC_UL_IN_GAIN_L,
	AMIC_UL_IN_GAIN_R,	/* today = same gain on AMIC pair */
	ECHO_REF_GAIN,
	BT_VX_DL_OUT_GAIN,
	BT_VX_UL_IN_GAIN,
} abe_gain_id;
#endif

/*
 * EVENT GENERATORS
 */
typedef enum {
	EVENT_MCPDM = 1,
	EVENT_DMIC, EVENT_TIMER,
	EVENT_McBSP, EVENT_McASP, EVENT_SLIMBUS, EVENT_44100, EVENT_DEFAULT,
} abe_event_id;

/*
 * SERIAL PORTS IDs
 */
typedef enum {
	MCBSP1_TX = MCBSP1_DMA_TX,
	MCBSP1_RX = MCBSP1_DMA_RX,
	MCBSP2_TX = MCBSP2_DMA_TX,
	MCBSP2_RX = MCBSP2_DMA_RX,
	MCBSP3_TX = MCBSP3_DMA_TX,
	MCBSP3_RX = MCBSP3_DMA_RX,
} abe_mcbsp_id;

/*
 * SERIAL PORTS IDs
 */
typedef enum {
	SLIMBUS1_TX0 = SLIMBUS1_DMA_TX0, /* SLIMBUS mod 1 - tx rqst channel 0 */
	SLIMBUS1_TX1 = SLIMBUS1_DMA_TX1, /* SLIMBUS mod 1 - tx rqst channel 1 */
	SLIMBUS1_TX2 = SLIMBUS1_DMA_TX2, /* SLIMBUS mod 1 - tx rqst channel 2 */
	SLIMBUS1_TX3 = SLIMBUS1_DMA_TX3, /* SLIMBUS mod 1 - tx rqst channel 3 */
	SLIMBUS1_TX4 = SLIMBUS1_DMA_TX4, /* SLIMBUS mod 1 - tx rqst channel 4 */
	SLIMBUS1_TX5 = SLIMBUS1_DMA_TX5, /* SLIMBUS mod 1 - tx rqst channel 5 */
	SLIMBUS1_TX6 = SLIMBUS1_DMA_TX6, /* SLIMBUS mod 1 - tx rqst channel 6 */
	SLIMBUS1_TX7 = SLIMBUS1_DMA_TX7, /* SLIMBUS mod 1 - tx rqst channel 7 */
	SLIMBUS1_RX0 = SLIMBUS1_DMA_RX0, /* SLIMBUS mod 1 - rx rqst channel 0 */
	SLIMBUS1_RX1 = SLIMBUS1_DMA_RX1, /* SLIMBUS mod 1 - rx rqst channel 1 */
	SLIMBUS1_RX2 = SLIMBUS1_DMA_RX2, /* SLIMBUS mod 1 - rx rqst channel 2 */
	SLIMBUS1_RX3 = SLIMBUS1_DMA_RX3, /* SLIMBUS mod 1 - rx rqst channel 3 */
	SLIMBUS1_RX4 = SLIMBUS1_DMA_RX4, /* SLIMBUS mod 1 - rx rqst channel 4 */
	SLIMBUS1_RX5 = SLIMBUS1_DMA_RX5, /* SLIMBUS mod 1 - rx rqst channel 5 */
	SLIMBUS1_RX6 = SLIMBUS1_DMA_RX6, /* SLIMBUS mod 1 - rx rqst channel 6 */
	SLIMBUS1_RX7 = SLIMBUS1_DMA_RX7, /* SLIMBUS mod 1 - rx rqst channel 7 */
	SLIMBUS_UNUSED = _DUMMY_FIFO_,
} abe_slimbus_id;

/*
 * TYPES USED FOR APIS
 */

/*
 * HARDWARE CONFIG TYPE
 */
typedef struct {
	abe_uint32 AESS_EVENT_GENERATOR_COUNTER__COUNTER_VALUE; /* EVENT_GENERATOR_COUNTER_DEFAULT gives about 96kHz */
	abe_uint32 AESS_EVENT_SOURCE_SELECTION__SELECTION; /* 0: DMAreq, 1:Counter */
	abe_uint32 AESS_AUDIO_ENGINE_SCHEDULER__DMA_REQ_SELECTION; /* 5bits DMAreq selection */
	abe_event_id HAL_EVENT_SELECTION;

	abe_uint32 MCPDM_CTRL__DIV_SEL;	 /* 0: 96kHz   1:192kHz */
	abe_uint32 MCPDM_CTRL__CMD_INT;	 /* 0: no command in the FIFO,  1: 6 data on each lines (with commands) */
	abe_uint32 MCPDM_CTRL__PDMOUTFORMAT;	/* 0:MSB aligned  1:LSB aligned */
	abe_uint32 MCPDM_CTRL__PDM_DN5_EN;
	abe_uint32 MCPDM_CTRL__PDM_DN4_EN;
	abe_uint32 MCPDM_CTRL__PDM_DN3_EN;
	abe_uint32 MCPDM_CTRL__PDM_DN2_EN;
	abe_uint32 MCPDM_CTRL__PDM_DN1_EN;
	abe_uint32 MCPDM_CTRL__PDM_UP3_EN;
	abe_uint32 MCPDM_CTRL__PDM_UP2_EN;
	abe_uint32 MCPDM_CTRL__PDM_UP1_EN;
	abe_uint32 MCPDM_FIFO_CTRL_DN__DN_TRESH;
	abe_uint32 MCPDM_FIFO_CTRL_UP__UP_TRESH;

	abe_uint32 DMIC_CTRL__DMIC_CLK_DIV;	/* 0:2.4MHz  1:3.84MHz */
	abe_uint32 DMIC_CTRL__DMICOUTFORMAT;	/* 0:MSB aligned  1:LSB aligned */
	abe_uint32 DMIC_CTRL__DMIC_UP3_EN;
	abe_uint32 DMIC_CTRL__DMIC_UP2_EN;
	abe_uint32 DMIC_CTRL__DMIC_UP1_EN;
	abe_uint32 DMIC_FIFO_CTRL__DMIC_TRESH;	/* 1*(DMIC_UP1_EN+ 2+ 3)*2 OCP read access every 96/88.1 KHz. */

	abe_uint32 MCBSP_SPCR1_REG__RJUST;	/* 1:MSB  2:LSB aligned */
	abe_uint32 MCBSP_THRSH2_REG_REG__XTHRESHOLD;	/* 1=MONO, 2=STEREO, 3=TDM_3_CHANNELS, 4=TDM_4_CHANNELS, */
	abe_uint32 MCBSP_THRSH1_REG_REG__RTHRESHOLD;	/* 1=MONO, 2=STEREO, 3=TDM_3_CHANNELS, 4=TDM_4_CHANNELS,  */
	abe_uint32 SLIMBUS_DCT_FIFO_SETUP_REG__SB_THRESHOLD;
} abe_hw_config_init_t;

/*
 * EANC_T
 *
 * TBD : coefficients of the EANC
 */
typedef struct {
	abe_int32 dmic_index;
	abe_int32 fir_coef[NBEANC1];
	abe_int32 lambda;
	abe_int32 iir_filter[NBEANC2];
	abe_int32 loop_gain;
} abe_eanc_t;

/*
 * EQU_T
 *
 * coefficients of the equalizer
 */
typedef struct {
	abe_iir_t equ_type;	/* type of filter */
	abe_uint32 equ_length;	/* filter length */
	union {			/* parameters are the direct and recursive coefficients in */
		abe_int32 type1[NBEQ1];		/* Q6.26 integer fixed-point format. */
		struct {
			abe_int32 freq [NBEQ2];		/* center frequency of the band [Hz] */
			abe_int32 gain [NBEQ2];		/* gain of each band. [dB]*/
			abe_int32 q    [NBEQ2];		/* Q factor of this band [dB] */
		} type2;
	} coef;
	abe_int32 equ_param3;
} abe_equ_t;

/*
 * APS_T
 *
 * coefficients of the Acoustics Protection and Safety
 */
typedef struct {
	abe_int32 coef1[NBAPS1];
	abe_int32 coef2[NBAPS2];
} abe_aps_t;

typedef struct {
		abe_millibel e1;	/* structure of two energy_t estimation for coil and membrane */
		abe_millibel e2;
} abe_aps_energy_t;

/*
 * ROUTER_T
 *
 * table of indexes in unsigned bytes
 */
typedef abe_uint16 abe_router_t;

/*
 * DATA_FORMAT_T
 *
 * used in port declaration
 */
typedef struct {
	abe_freq_t f;	/* Sampling frequency of the stream */
	abe_samp_t samp_format;	/* Sample format type  */
} abe_data_format_t;

/*
 * PORT_PROTOCOL_T
 *
 * port declaration
 */
typedef struct {
	abe_uint32 direction;		/* Direction=0 means input from AESS point of view */
	abe_port_protocol_switch_id protocol_switch; /* Protocol type (switch) during the data transfers */
	union {
		struct {			/* Slimbus peripheral connected to ATC */
			abe_uint32 desc_addr1;	/* Address of ATC Slimbus descriptor's index */
			abe_uint32 desc_addr2;	/* Second ATC index for SlimBus reception (or NULL) */
			abe_uint32 buf_addr1;	/* DMEM address 1 in bytes */
			abe_uint32 buf_addr2;	/* DMEM address 2 in bytes */
			abe_uint32 buf_size;	/* DMEM buffer size size in bytes */
			abe_uint32 iter;	/* ITERation on each DMAreq signals */
		} prot_slimbus;

		struct {
			abe_uint32 desc_addr;	/* McBSP/McASP peripheral connected to ATC */
			abe_uint32 buf_addr;	/* Address of ATC McBSP/McASP descriptor's in bytes */
			abe_uint32 buf_size;	/* DMEM address in bytes */
			abe_uint32 iter;	/* ITERation on each DMAreq signals */
		} prot_serial;

		struct {			/* DMIC peripheral connected to ATC */
			abe_uint32 buf_addr;	/* DMEM address in bytes */
			abe_uint32 buf_size;	/* DMEM buffer size in bytes */
			abe_uint32 nbchan;	/* Number of activated DMIC */
		} prot_dmic;

		struct {			/* McPDMDL peripheral connected to ATC */
			abe_uint32 buf_addr;	/* DMEM address in bytes */
			abe_uint32 buf_size;	/* DMEM size in bytes */
			abe_uint32 control;	/* Control allowed on McPDM DL */
		} prot_mcpdmdl;

		struct {			/* McPDMUL peripheral connected to ATC */
			abe_uint32 buf_addr;	/* DMEM address size in bytes */
			abe_uint32 buf_size;	/* DMEM buffer size size in bytes */
		} prot_mcpdmul;

		struct {			/* Ping-Pong interface to the Host using cache-flush */
			abe_uint32 desc_addr;	/* Address of ATC descriptor's */
			abe_uint32 buf_addr;	/* DMEM buffer base address in bytes */
			abe_uint32 buf_size;	/* DMEM size in bytes for each ping and pong buffers */
			abe_uint32 irq_addr;	/* IRQ address (either DMA (0) MCU (1) or DSP(2)) */
			abe_uint32 irq_data;	/* IRQ data content loaded in the AESS IRQ register */
			abe_uint32 callback;	/* Call-back function upon IRQ reception */
		} prot_pingpong;

		struct {			/* DMAreq line to CBPr */
			abe_uint32 desc_addr;   /* Address of ATC descriptor's */
			abe_uint32 buf_addr;	/* DMEM buffer address in bytes */
			abe_uint32 buf_size;	/* DMEM buffer size size in bytes */
			abe_uint32 iter;	/* ITERation on each DMAreq signals */
			abe_uint32 dma_addr;	/* DMAreq address */
			abe_uint32 dma_data;	/* DMA/AESS = 1 << #DMA */
		} prot_dmareq;

		struct {			/* Circular buffer - direct addressing to DMEM */
			abe_uint32 buf_addr;	/* DMEM buffer base address in bytes */
			abe_uint32 buf_size;	/* DMEM buffer size in bytes */
			abe_uint32 dma_addr;	/* DMAreq address */
			abe_uint32 dma_data;	/* DMA/AESS = 1 << #DMA */
		} prot_circular_buffer;
	}p;
} abe_port_protocol_t;

/*
 * DMA_T
 *
 * dma structure for easing programming
 */
typedef struct {
	void *data;		/* OCP L3 pointer to the first address of the */
				/* destination buffer (either DMA or Ping-Pong read/write pointers). */
	void *l3_dmem;		/* address L3 when addressing the DMEM buffer instead of CBPr */
	void *l4_dmem;		/* address L3 translated to L4 the ARM memory space */
	abe_uint32 iter;	/* number of iterations for the DMA data moves. */
} abe_dma_t;

typedef struct {
	abe_uint32 data;	/* Offset to the first address of the */
	abe_uint32 iter;	/* number of iterations for the DMA data moves. */
} abe_dma_t_offset;

/*
 *  SEQ_T
 *
 * struct {
 * micros_t time;	  Waiting time before executing next line
 * seq_code_t code	 Subroutine index interpreted in the HAL and translated to
 *	FW subroutine codes in case of ABE tasks
 * int32 param[2]	  Two parameters
 *} seq_t
 *
 */
typedef struct {
	abe_micros_t delta_time;
	abe_sub_code_t code;
	abe_uint32 param[4];
	abe_uint8  tag;
} abe_seq_t;

typedef struct {
	abe_uint32 mask;
	abe_seq_t seq1;
	abe_seq_t seq2;
} abe_sequence_t;

/*
 * DRIFT_T
 *
 * ASRC drift parameter in [ppm] value
 */
typedef abe_int32 abe_drift_t;

/*
 * INTERNAL DATA TYPES
 */

/*
 * ABE_IRQ_DATA_T
 *
 * IRQ FIFO content declaration
 * APS interrupts:
 *	IRQtag_APS to [31:28], APS_IRQs to [27:16], loopCounter to [15:0]
 * SEQ interrupts:
 *	 IRQtag_COUNT to [31:28], Count_IRQs to [27:16], loopCounter to [15:0]
 * Ping-Pong Interrupts:
 *	 IRQtag_PP to [31:28], PP_MCU_IRQ to [27:16], loopCounter to [15:0]
 */
typedef struct {
	unsigned int counter: 16;
	unsigned int data: 12;
	unsigned int tag: 4;
} abe_irq_data_t;

/*
 * ABE_PORT_T status / format / sampling / protocol(call_back) / features / gain / name ..
 *
 */
typedef struct {
	abe_uint16  status;			/* running / idled */
	abe_data_format_t format;		/* Sample format type  */
	abe_drift_t drift;			/* API : for ASRC */
	abe_uint16 callback;			/* optionnal call-back index for errors and ack */
	abe_uint16 smem_buffer1;		/* IO tasks buffers */
	abe_uint16 smem_buffer2;
	abe_port_protocol_t protocol;
	abe_dma_t_offset dma;			/* pointer and iteration counter of the xDMA */
	abe_uint16 feature_index [MAXFEATUREPORT]; /* list of features associated to a port (EQ, APS, ... , ends with 0) */
	 // abe_millibel gain_calibration;	/* gain tuning, default=0dB */
	char name[NBCHARPORTNAME];
} abe_port_t;

/*
 * ABE_SUBROUTINE_T
 *
 */
typedef struct {
	abe_uint32 sub_id;
	abe_int32 param[4];
} abe_subroutine_t;

/*
 * ABE_PORT_INFO_T
 *
 * OPP, subroutines to call on reset
 */
typedef struct {
	abe_opp_t  min_opp;
	abe_subroutine_t sub1;
	abe_subroutine_t sub2;
} abe_port_info_t;

/*
 * ABE_FEATURE_T
 *
 */
typedef struct {
	abe_uint16 enable_with_default_data;
	abe_uint16 disable_feature;
	abe_uint16 read_parameter;
	abe_uint16 write_parameter;
	abe_uint16 running_status;
	abe_uint16 fw_input_buffer_address;
	abe_uint16 fw_output_buffer_address;
	abe_uint16 fw_scheduler_slot_position;
	abe_uint16 fw_scheduler_subslot_position;
	abe_opp_t  min_opp;
	char name[NBCHARFEATURENAME];
} abe_feature_t;

#ifdef __cplusplus
}
#endif

#endif	/* _ABE_TYP_H_ */
