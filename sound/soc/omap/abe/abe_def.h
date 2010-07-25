/*
 * ==========================================================================
 *		 Texas Instruments OMAP(TM) Platform Firmware
 * (c) Copyright 2009, Texas Instruments Incorporated.	All Rights Reserved.
 *
 *  Use of this firmware is controlled by the terms and conditions found
 *  in the license agreement under which this firmware has been supplied.
 * ==========================================================================
 */

#ifndef _ABE_DEF_H_
#define _ABE_DEF_H_

/*
 * HARDWARE AND PERIPHERAL DEFINITIONS
 */

#define ABE_DMAREQ_REGISTER(desc)	(abe_uint32 *)((desc/8) + CIRCULAR_BUFFER_PERIPHERAL_R__0)

//#define ABE_SEND_DMAREQ(dma)	(*((abe_uint32 *)(ABE_ATC_BASE_ADDRESS_MPU+ABE_DMASTATUS_RAW)) = (dma))

#define ABE_CBPR0_IDX		0	/* MM_DL */
#define ABE_CBPR1_IDX		1	/* VX_DL */
#define ABE_CBPR2_IDX		2	/* VX_UL */
#define ABE_CBPR3_IDX		3	/* MM_UL */
#define ABE_CBPR4_IDX		4	/* MM_UL2 */
#define ABE_CBPR5_IDX		5	/* TONES */
#define ABE_CBPR6_IDX		6	/* VIB */
#define ABE_CBPR7_IDX		7	/* DEBUG/CTL */

#define CIRCULAR_BUFFER_PERIPHERAL_R__0	(0x100 + ABE_CBPR0_IDX * 4)
#define CIRCULAR_BUFFER_PERIPHERAL_R__1	(CIRCULAR_BUFFER_PERIPHERAL_R__0 + ABE_CBPR1_IDX * 4)
#define CIRCULAR_BUFFER_PERIPHERAL_R__2	(CIRCULAR_BUFFER_PERIPHERAL_R__0 + ABE_CBPR2_IDX * 4)
#define CIRCULAR_BUFFER_PERIPHERAL_R__3	(CIRCULAR_BUFFER_PERIPHERAL_R__0 + ABE_CBPR3_IDX * 4)
#define CIRCULAR_BUFFER_PERIPHERAL_R__4	(CIRCULAR_BUFFER_PERIPHERAL_R__0 + ABE_CBPR4_IDX * 4)
#define CIRCULAR_BUFFER_PERIPHERAL_R__5	(CIRCULAR_BUFFER_PERIPHERAL_R__0 + ABE_CBPR5_IDX * 4)
#define CIRCULAR_BUFFER_PERIPHERAL_R__6	(CIRCULAR_BUFFER_PERIPHERAL_R__0 + ABE_CBPR6_IDX * 4)
#define CIRCULAR_BUFFER_PERIPHERAL_R__7	(CIRCULAR_BUFFER_PERIPHERAL_R__0 + ABE_CBPR7_IDX * 4)

/*
 * cache-flush mechanism
 */
#define NB_BYTES_CACHELINE_SHFT	4
#define NB_BYTES_IN_CACHE_LINE	(1<<NB_BYTES_CACHELINE_SHFT) /* there are 16 bytes in each cache lines	*/

/*
 * DEFINITIONS SHARED WITH VIRTAUDIO
 */

#define UC1_LP			1	/* MP3 low-power player use-case */
#define UC2_VOICE_CALL_AND_IHF_MMDL 2	/* enables voice ul/dl on earpiece + MM_DL on IHF */
#define UC5_PINGPONG_MMDL	5	/* Test MM_DL with Ping-Pong */
#define UC6_PINGPONG_MMDL_WITH_IRQ 6	/* ping-pong with IRQ instead of sDMA */

#define UC31_VOICE_CALL_8KMONO		31
#define UC32_VOICE_CALL_8KSTEREO	32
#define UC33_VOICE_CALL_16KMONO		33
#define UC34_VOICE_CALL_16KSTEREO	34
#define UC35_MMDL_MONO			35
#define UC36_MMDL_STEREO		36
#define UC37_MMUL2_MONO			37
#define UC38_MMUL2_STEREO		38

#define UC41_____		40
#define UC71_STOP_ALL		71	/* stop all activities */
#define UC72_ENABLE_ALL		72	/* stop all activities */
#define UC81_ROUTE_AMIC		81
#define UC82_ROUTE_DMIC01	82
#define UC83_ROUTE_DMIC23	83
#define UC84_ROUTE_DMIC45	84

#define UC91_ASRC_DRIFT1	91
#define UC92_ASRC_DRIFT2	92
#define UC93_EANC		93

#define PING_PONG_WITH_MCU_IRQ	1
#define PING_PONG_WITH_DSP_IRQ	2

#define HAL_RESET_HAL		10	/* abe_reset_hal () */
#define HAL_WRITE_MIXER		11	/* abe_write_mixer () */

#define COPY_FROM_ABE_TO_HOST	1	/* ID used for LIB memory copy subroutines */
#define COPY_FROM_HOST_TO_ABE	2

/*
 * INTERNAL DEFINITIONS
 */

#define CC_M1			0xFF	/* unsigned version of (-1) */
#define CS_M1			0xFFFF	/* unsigned version of (-1) */
#define CL_M1			0xFFFFFFFFL /* unsigned version of (-1) */

#define NBEANC1			20	/* 20 Q6.26 coef for the FIR	*/
#define NBEANC2			16	/* 16 Q6.26 coef for the IIR	*/

#define NBEQ1			25	/* 24 Q6.26 coefficients	*/
#define NBEQ2			13	/* 2x12 Q6.26 coefficients	 */

#define NBAPS1			10	/* TBD APS first set of parameters	*/
#define NBAPS2			10	/* TBD APS second set of parameters	 */

#define NBMIX_AUDIO_UL		2	/* Mixer used for sending tones to the uplink voice path */
#define NBMIX_DL1		4	/* Main downlink mixer */
#define NBMIX_DL2		4	/* Handsfree downlink mixer */
#define NBMIX_SDT		2	/* Side-tone mixer */
#define NBMIX_ECHO		2	/* Echo reference mixer */
#define NBMIX_VXREC		4	/* Voice record mixer */
	/*
	Mixer ID		Input port ID	Comments
	DL1_MIXER		0		MMDL path
				1		MMUL2 path
				2		VXDL path
				3		TONES path

	SDT_MIXER		0		Uplink path
				1		Downlink path

	ECHO_MIXER		0		DL1_MIXER  path
				1		DL2_MIXER path

	AUDUL_MIXER		0		TONES_DL path
				1		Uplink path
				2		MM_DL path

	VXREC_MIXER		0		TONES_DL path
				1		VX_DL path
				2		MM_DL path
				3		VX_UL path
	*/
#define MIX_VXUL_INPUT_MM_DL	(abe_port_id)0
#define MIX_VXUL_INPUT_TONES	(abe_port_id)1
#define MIX_VXUL_INPUT_VX_UL	(abe_port_id)2
#define MIX_VXUL_INPUT_VX_DL	(abe_port_id)3

#define MIX_DL1_INPUT_MM_DL	(abe_port_id)0
#define MIX_DL1_INPUT_MM_UL2	(abe_port_id)1
#define MIX_DL1_INPUT_VX_DL	(abe_port_id)2
#define MIX_DL1_INPUT_TONES	(abe_port_id)3

#define MIX_DL2_INPUT_MM_DL	(abe_port_id)0
#define MIX_DL2_INPUT_MM_UL2	(abe_port_id)1
#define MIX_DL2_INPUT_VX_DL	(abe_port_id)2
#define MIX_DL2_INPUT_TONES	(abe_port_id)3

#define MIX_SDT_INPUT_UP_MIXER	(abe_port_id)0
#define MIX_SDT_INPUT_DL1_MIXER (abe_port_id)1

#define MIX_AUDUL_INPUT_MM_DL	(abe_port_id)0
#define MIX_AUDUL_INPUT_TONES	(abe_port_id)1
#define MIX_AUDUL_INPUT_UPLINK	(abe_port_id)2
#define MIX_AUDUL_INPUT_VX_DL	(abe_port_id)3

#define MIX_VXREC_INPUT_MM_DL	(abe_port_id)0
#define MIX_VXREC_INPUT_TONES	(abe_port_id)1
#define MIX_VXREC_INPUT_VX_UL	(abe_port_id)2
#define MIX_VXREC_INPUT_VX_DL	(abe_port_id)3

#define NBROUTE_UL		16	/* nb of samples to route */
#define NBROUTE_CONFIG_MAX	10	/* 10 routing tables max */

#define NBROUTE_CONFIG		5	/* 5 pre-computed routing tables */
#define UPROUTE_CONFIG_AMIC	0	/* AMIC on VX_UL */
#define UPROUTE_CONFIG_DMIC1	1	/* DMIC first pair on VX_UL */
#define UPROUTE_CONFIG_DMIC2	2	/* DMIC second pair on VX_UL */
#define UPROUTE_CONFIG_DMIC3	3	/* DMIC last pair on VX_UL */
#define UPROUTE_CONFIG_BT	4	/* BT_UL on VX_UL */

#define ABE_PMEM		1
#define ABE_CMEM		2
#define ABE_SMEM		3
#define ABE_DMEM		4
#define ABE_ATC			5

#define MAXCALLBACK		100	/* call-back indexes */
#define MAXNBSUBROUTINE		100	/* subroutines */

#define MAXNBSEQUENCE		20	/* time controlled sequenced */
#define MAXACTIVESEQUENCE	20	/* maximum simultaneous active sequences */
#define MAXSEQUENCESTEPS	2	/* max number of steps in the sequences */
#define MAXFEATUREPORT		12	/* max number of feature associated to a port */
#define SUB_0_PARAM		0
#define SUB_1_PARAM		1	/* number of parameters per sequence calls */
#define SUB_2_PARAM		2
#define SUB_3_PARAM		3
#define SUB_4_PARAM		4

#define FREE_LINE		0	/* active sequence mask = 0 means the line is free */
#define NOMASK			(1 << 0) /* no ask for collision protection */
#define MASK_PDM_OFF		(1 << 1) /* do not allow a PDM OFF during the execution of this sequence */
#define MASK_PDM_ON		(1 << 2) /* do not allow a PDM ON during the execution of this sequence */

#define NBCHARFEATURENAME	16	/* explicit name of the feature */
#define NBCHARPORTNAME		16	/* explicit name of the port */

#define SNK_P			ABE_ATC_DIRECTION_IN	/* sink / input port from Host point of view (or AESS for DMIC/McPDM/.. */
#define SRC_P			ABE_ATC_DIRECTION_OUT	/* source / ouptut port */

#define NODRIFT			0	/* no ASRC applied */
#define FORCED_DRIFT_CONTROL	1	/* for abe_set_asrc_drift_control */
#define ADPATIVE_DRIFT_CONTROL	2	/* for abe_set_asrc_drift_control */

#define DOPPMODE32_OPP100	(0x00000010 | (0x00000000<<16))
#define DOPPMODE32_OPP50	(0x0000000C | (0x0000004<<16))
#define DOPPMODE32_OPP25	(0x0000004 | (0x0000000C<<16))

/*
 * ABE CONST AREA FOR PARAMETERS TRANSLATION
 */
#define min_mdb (-12000)
#define max_mdb (  3000)
#define sizeof_db2lin_table (1+ ((max_mdb - min_mdb)/100))

#define GAIN_MAXIMUM	(abe_gain_t)3000L
#define GAIN_24dB	(abe_gain_t)2400L
#define GAIN_18dB		   (abe_gain_t)1800L
#define GAIN_12dB		   (abe_gain_t)1200L
#define GAIN_6dB		    (abe_gain_t)600L
#define GAIN_0dB		     (abe_gain_t) 0L	    /* default gain = 1 */
#define GAIN_M6dB		   (abe_gain_t)-600L
#define GAIN_M12dB		  (abe_gain_t)-1200L
#define GAIN_M18dB		  (abe_gain_t)-1800L
#define GAIN_M24dB		  (abe_gain_t)-2400L
#define GAIN_M30dB		  (abe_gain_t)-3000L
#define GAIN_M40dB		  (abe_gain_t)-4000L
#define GAIN_M50dB		  (abe_gain_t)-5000L
#define MUTE_GAIN		 (abe_gain_t)-12000L

#define RAMP_0MS	(abe_ramp_t)0L           /* ramp_t is in milli- seconds */
#define RAMP_1MS	(abe_ramp_t)1L
#define RAMP_2MS	(abe_ramp_t)2L
#define RAMP_5MS        (abe_ramp_t)5L
#define RAMP_10MS       (abe_ramp_t)10L
#define RAMP_20MS       (abe_ramp_t)20L
#define RAMP_50MS       (abe_ramp_t)50L
#define RAMP_100MS      (abe_ramp_t)100L
#define RAMP_200MS      (abe_ramp_t) 200L
#define RAMP_500MS      (abe_ramp_t) 500L
#define RAMP_1000MS     (abe_ramp_t) 1000L
#define RAMP_MAXLENGTH  (abe_ramp_t) 10000L

#define LINABE_TO_DECIBELS	1	/* for abe_translate_gain_format */
#define DECIBELS_TO_LINABE	2
#define IIRABE_TO_MICROS	1	/* for abe_translate_ramp_format */
#define MICROS_TO_IIABE		2

#define IDLE_P			1	/* port idled */
#define RUN_P			2	/* port running */
#define NOCALLBACK		0
#define NOPARAMETER		0
					/* HAL 06: those numbers may be x4 */
#define MCPDM_UL_ITER		4	/* number of ATC access upon AMIC DMArequests, all the FIFOs are enabled */
#define MCPDM_DL_ITER		12	/* All the McPDM FIFOs are enabled simultaneously */
#define DMIC_ITER		6	/* All the DMIC FIFOs are enabled simultaneously */

#define DEFAULT_THR_READ	1	/* port / flow management */
#define DEFAULT_THR_WRITE	1	/* port / flow management */

#define DEFAULT_CONTROL_MCPDMDL 1	/* allows control on the PDM line */

#define MAX_PINGPONG_BUFFERS	2	/* TBD later if needed */

/*
 * Indexes to the subroutines
 */
#define SUB_WRITE_MIXER		1
#define SUB_WRITE_PORT_GAIN	2

/* OLD WAY */
#define c_feat_init_eq		1
#define c_feat_read_eq1		2
#define c_write_eq1		3
#define c_feat_read_eq2		4
#define c_write_eq2		5
#define c_feat_read_eq3		6
#define c_write_eq3		7

/*
 * MACROS
 */

#define LOAD_ABEREG(reg,data)	{abe_uint32 *ocp_addr = (abe_uint32 *)((reg)+ABE_ATC_BASE_ADDRESS_MPU); *ocp_addr= (data);}

#define maximum(a,b)		(((a)<(b))?(b):(a))
#define minimum(a,b)		(((a)>(b))?(b):(a))
#define absolute(a)		( ((a)>0) ? (a):((-1)*(a)) )

// Gives 1% errors
//#define abe_power_of_two(x) (abe_float)(1 + x*(0.69315 + x*(0.24022 + x*(0.056614 + x*(0.00975 )))))	 /* for x = [-1..+1] */
//#define abe_log_of_two(i)   (abe_float)(-2.4983 + i*(4.0321 + i*(-2.0843 + i*(0.63 + i*(-0.0793)))))	 /* for i = [+1..+2[ */
// Gives 0.1% errors
//#define abe_power_of_two(xx) (abe_float)(1 + xx*(0.69314718055995 + xx*(0.24022650695909 + xx*(0.05661419083812 + xx*(0.0096258236109 )))))	/* for x = [-1..+1] */
//#define abe_log_of_two(i)   (abe_float)(-3.02985297173966 + i*(6.07170945221999 + i*(-5.27332161514862 + i*(3.22638187067771 + i*(-1.23767101624897 + i*(0.26766043958616 + i*(-0.02490211314987)))))))   /* for i = [+1..+2[ */

#if 0
#define abe_power_of_two(xx) (abe_float)(0.9999999924494 + xx*(0.69314847688495 + xx*(0.24022677604481 + xx*(0.05549256818679 + xx*(0.00961666477618 + xx*(0.0013584351075 + xx*(0.00015654359307)))))))	/* for x = [-1..+1] */
#define abe_log_of_two(xx)   (abe_float)(-3.02985297175803 + xx*(6.07170945229365 + xx*(-5.27332161527062 + xx*(3.22638187078450 + xx*(-1.23767101630110 + xx*(0.26766043959961 + xx*(-0.02490211315130)))))))	/* for x = [+1..+2] */
#define abe_sine(xx)	 (abe_float)(-0.000000389441 + xx*(6.283360925789 + xx*(-0.011140658372 + xx*(-41.073653348384 + xx*(-3.121196875959 + xx*(100.619954580736 + xx*( -59.133359355846)))))))	  /* for x = [0 .. pi/2] */
#define abe_sqrt(xx)	 (abe_float)(0.32298238417665 + xx*(0.93621865220393 + xx*(-0.36276443369703 + xx*(0.13008602653101+ xx*(-0.03017833169073 + xx*(0.00393731964847 + xx*-0.00021858629159 ))))))     /* for x = [1 .. 4] */
#endif

#endif	/* _ABE_DEF_H_ */
