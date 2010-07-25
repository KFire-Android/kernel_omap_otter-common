/*
 * ==========================================================================
 *		 Texas Instruments OMAP(TM) Platform Firmware
 * (c) Copyright 2009, Texas Instruments Incorporated.	All Rights Reserved.
 *
 *	Use of this firmware is controlled by the terms and conditions found
 *	in the license agreement under which this firmware has been supplied.
 * ==========================================================================
 */

#ifndef _ABE_TYPEDEF_H_
#define _ABE_TYPEDEF_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "abe_define.h"

/*
 * Basic types definition
*/
typedef unsigned char ABE_uchar;
typedef char ABE_char;
typedef unsigned short ABE_uint16;
typedef short ABE_int16;
typedef long ABE_int32;
typedef unsigned long ABE_uint32;

typedef ABE_uchar* pABE_uchar;
typedef ABE_char* pABE_char;
typedef ABE_uint16* pABE_uint16;
typedef ABE_int16* pABE_int16;
typedef ABE_int32* pABE_int32;
typedef ABE_uint32* pABE_uint32;

/*
 * Hard-coded data generated in the XLS sheet (to be removed later@@@@)
 */
#ifdef __chess__
typedef struct abeatcdescTag {
	unsigned long a;
	unsigned long b;
} ABE_SAtcDescriptor;
typedef void (*pABE_voidFunction)()clobbers(R0, R1, R2, R3, R4, R5, R6, R7, R13);
typedef void (*pABE_voidFunctionsList[])()clobbers(R0, R1, R2, R3, R4, R5, R6, R7, R13);
typedef void (*pABE_cmdFunction)() clobbers(R0, R1, R2, R3, R4, R5, R6, R7, R13);
typedef void (*pABE_cmdFunctionsList[])() clobbers(R0, R1, R2, R3, R4, R5, R6, R7, R13);
typedef void (*pABE_copyFunction)(ABE_uint16 chess_storage(R13))clobbers(R13);
typedef void (*pABE_copyFunctionsList[])(ABE_uint16 chess_storage(R13))clobbers(R13);
#endif
/*
 * Commonly used structures
 */

typedef struct abetaskTag{
	ABE_uint16 iF;	/* 0 Index of called function */
	ABE_uint16 A0;	/* 2 for INITPTR  of A0 */
	ABE_uint16 A1;	/* 4 for INITPTR  of A1 */
	ABE_uint16 A2_3;	/* 6 for INITPTR  of A2 & A3 */
	ABE_uint16 A4_5;	/* 8 for INITPTR  of A4 & A5 */
	ABE_uint16 R;	/* 10 for INITREG of R0, R1, R2, R3 */
	ABE_uint16 misc0;	/* 12 */
	ABE_uint16 misc1;	/* 14 */
} ABE_STask;
typedef ABE_STask* pABE_STask;
typedef ABE_STask** ppABE_STask;

typedef struct {
	ABE_uint16 drift_ASRC;	/* 0 */
	ABE_uint16 drift_io;	/* 2 */
	ABE_uchar io_type_idx;	/* 4 */
	ABE_uchar samp_size;	/* 5 */
	ABE_uint16 flow_counter;	/* 6 */

	ABE_uint16 hw_ctrl_addr;	/* 8 */
	ABE_uchar atc_irq_data;		/* 10 */
	ABE_uchar direction_rw;		/* 11 */
	ABE_uchar unused1;		/* 12 */
	ABE_uchar nsamp;		/* 13 */
	ABE_uchar x_io;			/* 14 */
	ABE_uchar on_off;		/* 15 */

	ABE_uint16 split_addr1;		/* 16 */
	ABE_uint16 split_addr2;		/* 18 */
	ABE_uint16 split_addr3;		/* 20 */
	ABE_uchar before_f_index;	/* 22 */
	ABE_uchar after_f_index;	/* 23 */

	ABE_uint16 smem_addr1;		/* 24 */
	ABE_uint16 atc_address1;	/* 26 */
	ABE_uint16 atc_pointer_saved1;	/* 28 */
	ABE_uchar data_size1;		/* 30 */
	ABE_uchar copy_f_index1;	/* 31 */

	ABE_uint16 smem_addr2;		/* 32 */
	ABE_uint16 atc_address2;	/* 34 */
	ABE_uint16 atc_pointer_saved2;	/* 36 */
	ABE_uchar data_size2;		/* 38 */
	ABE_uchar copy_f_index2;	/* 39 */

} ABE_SIODescriptor;


#define drift_asrc_	0	/* [w] asrc output used for the next asrc call (+/- 1 / 0) */
#define drift_io_	2	/* [w] asrc output used for controlling the number of samples to be exchanged (+/- 1 / 0) */
#define io_type_idx_	4	/* address of the IO subroutine */
#define samp_size_	5
#define unused1		6
#define unused2		7
#define hw_ctrl_addr_	8	/* dmareq address or host irq buffer address (atc address) */
#define atc_irq_data_	10	/* data content to be loaded to "hw_ctrl_addr" */
#define direction_rw_	11	/* read dmem =0, write dmem =3 (atc offset of the access pointer) */
#define flow_counter_	6	/* flow error counter */
#define nsamp_		13	/* number of samples (either mono stereo...) */
#define x_io_		14	/* x number of raw DMEM data moved */
#define on_off_		15

#define split_addr1_	16	/* internal smem buffer initptr pointer index */
#define split_addr2_	18	/* internal smem buffer initptr pointer index */
#define split_addr3_	20	/* internal smem buffer initptr pointer index */
#define before_f_index_	22	/* index of the copy subroutine */
#define after_f_index_	23	/* index of the copy subroutine */

#define minidesc1_	24
#define rel_smem_	0	/* internal smem buffer initptr pointer index */
#define rel_atc_	2	/* atc descriptor address (byte address x4) */
#define rel_atc_saved	4	/* location of the saved ATC pointer (+debug info) */
#define rel_size_	6	/* size of each sample (1:mono/1616 2:stereo ) */
#define rel_f_		7	/* index of the copy subroutine */

#define s_mem_mm_ul	24
#define s_mm_ul_size	30

#define minidesc2_	32
#define Struct_Size	40

typedef ABE_SIODescriptor* pABE_SIODescriptor;
typedef ABE_SIODescriptor** ppABE_SIODescriptor;

typedef struct abepingpongdescriptorTag{
	ABE_uint16 drift_ASRC;	/* 0 [W] asrc output used for the next ASRC call (+/- 1 / 0)*/
	ABE_uint16 drift_io;	/* 2 [W] asrc output used for controlling the number of samples to be exchanged (+/- 1 / 0) */
	ABE_uint16 hw_ctrl_addr;	/* 4 DMAReq address or HOST IRQ buffer address (ATC ADDRESS) */
	ABE_uchar  copy_func_index;	/* 6 index of the copy subroutine */
	ABE_uchar x_io;			/* 7 X number of SMEM samples to move */
	ABE_uchar data_size;	/* 8 0 for mono data, 1 for stereo data */
	ABE_uchar smem_addr;	/* 9 internal SMEM buffer INITPTR pointer index */
	ABE_uchar atc_irq_data;	/* 10 data content to be loaded to "hw_ctrl_addr" */
	ABE_uchar counter;	/* 11 ping/pong buffer flag */
	ABE_uint16 workbuff_BaseAddr;	/* 12 current Base address of the working buffer */
	ABE_uint16 workbuff_Samples;	/* 14 samples left in the working buffer */
	ABE_uint16 nextbuff0_BaseAddr;	/* 6 Base address of the ping/pong buffer 0 */
	ABE_uint16 nextbuff0_Samples;	/* 18 samples available in the ping/pong buffer 0 */
	ABE_uint16 nextbuff1_BaseAddr;	/* 20 Base address of the ping/pong buffer 1 */
	ABE_uint16 nextbuff1_Samples;	/* 22 samples available in the ping/pong buffer 1 */
} ABE_SPingPongDescriptor;

typedef ABE_SPingPongDescriptor* pABE_SPingPongDescriptor;

#ifdef __chess__
#define drift_ASRC	0	/* [W] asrc output used for the next ASRC call (+/- 1 / 0)*/
#define drift_io	2	/* [W] asrc output used for controlling the number of samples to be exchanged (+/- 1 / 0) */
#define hw_ctrl_addr	4	/* DMAReq address or HOST IRQ buffer address (ATC ADDRESS) */
#define copy_func_index 6	/* index of the copy subroutine */
#define x_io		7	/* X number of SMEM samples to move */
#define data_size	8	/* 0 for mono data, 1 for stereo data */
#define smem_addr	9	/* internal SMEM buffer INITPTR pointer index */
#define atc_irq_data	10	/* data content to be loaded to "hw_ctrl_addr"	*/
#define atc_address	11	/* ATC descriptor address */
#define threshold_1	12	/* THR1; For stereo data, THR1 is provided by HAL as THR1<<1 */
#define threshold_2	13	/* THR2; For stereo data, THR2 is provided by HAL as THR2<<1 */
#define update_1	14	/* UP_1; For stereo data, UP_1 is provided by HAL as UP_1<<1 */
#define update_2	15	/* UP_2; For stereo data, UP_2 is provided by HAL as UP_2<<1 */
#define flow_counter	16	/* Flow error counter */
#define direction_rw	17	/* Read DMEM =0, Write DMEM =3 (ATC offset of the access pointer) */
#define counter		11	/* ping/pong buffer flag */
#define workbuff_BaseAddr	12	/* current Base address of the working buffer */
#define workbuff_Samples	14	/* samples left in the working buffer */
#define nextbuff0_BaseAddr	16	/* Base address of the ping/pong buffer 0 */
#define nextbuff0_Samples	18	/* samples available in the ping/pong buffer 0 */
#define nextbuff1_BaseAddr	20	/* Base address of the ping/pong buffer 1 */
#define nextbuff1_Samples	22	/* samples available in the ping/pong buffer 1 */
#endif

#endif	/* _ABE_TYPEDEF_H_ */
