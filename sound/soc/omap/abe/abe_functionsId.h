/*
 * ==========================================================================
 *               Texas Instruments OMAP(TM) Platform Firmware
 * (c) Copyright 2009, Texas Instruments Incorporated.  All Rights Reserved.
 *
 *  Use of this firmware is controlled by the terms and conditions found
 *  in the license agreement under which this firmware has been supplied.
 * ==========================================================================
 */

#ifndef _ABE_FUNCTIONSID_H_
#define _ABE_FUNCTIONSID_H_

/*
 * TASK function ID definitions
 */
#define C_ABE_FW_FUNCTION_IIR					0
#define C_ABE_FW_FUNCTION_monoToStereoPack			1
#define C_ABE_FW_FUNCTION_stereoToMonoSplit			2
#define C_ABE_FW_FUNCTION_decimator				3
#define C_ABE_FW_FUNCTION_OS0Fill				4
#define C_ABE_FW_FUNCTION_mixer2				5
#define C_ABE_FW_FUNCTION_mixer4				6
#define C_ABE_FW_FUNCTION_inplaceGain				7
#define C_ABE_FW_FUNCTION_StreamRouting				8
#define C_ABE_FW_FUNCTION_gainConverge				9
#define C_ABE_FW_FUNCTION_dualIir				10
#define C_ABE_FW_FUNCTION_DCOFFSET				11
#define C_ABE_FW_FUNCTION_IO_DL_pp				12
#define C_ABE_FW_FUNCTION_IO_generic				13
#define C_ABE_FW_FUNCTION_irq_fifo_debug			14
#define C_ABE_FW_FUNCTION_synchronize_pointers			15
#define C_ABE_FW_FUNCTION_VIBRA2				16
#define C_ABE_FW_FUNCTION_VIBRA1				17
#define C_ABE_FW_FUNCTION_APS_core				18
#define C_ABE_FW_FUNCTION_IIR_SRC_MIC				19
#define C_ABE_FW_FUNCTION_wrappers				20
#define C_ABE_FW_FUNCTION_EANCUpdateOutSample			21
#define C_ABE_FW_FUNCTION_EANC					22
#define C_ABE_FW_FUNCTION_ASRC_DL_wrapper			23
#define C_ABE_FW_FUNCTION_ASRC_UL_wrapper			24

/*
 * COPY function ID definitions
 */
#define NULL_COPY_CFPID						0
#define S2D_STEREO_16_16_CFPID					1
#define S2D_MONO_MSB_CFPID					2
#define S2D_STEREO_MSB_CFPID					3
#define S2D_STEREO_RSHIFTED_16_CFPID				4
#define S2D_MONO_RSHIFTED_16_CFPID				5
#define D2S_STEREO_16_16_CFPID					6
#define D2S_MONO_MSB_CFPID					7
#define D2S_STEREO_MSB_CFPID					8
#define D2S_STEREO_RSHIFTED_16_CFPID				9
#define D2S_MONO_RSHIFTED_16_CFPID				10
#define COPY_DMIC_CFPID						11
#define COPY_MCPDM_DL_CFPID					12
#define COPY_MM_UL_CFPID					13
#define SPLIT_SMEM_CFPID					14
#define MERGE_SMEM_CFPID					15
#define SPLIT_TDM_CFPID						16
#define MERGE_TDM_CFPID						17
#define ROUTE_MM_UL_CFPID					18
#define IO_DMAREQ_CFPID						19
#define IO_IP_CFPID						20

#endif	/* _ABE_FUNCTIONSID_H_ */
