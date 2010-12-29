/*
 * ALSA SoC OMAP ABE driver
 *
 * Author:	Laurent Le Faucheur <l-le-faucheur@ti.com>
 * 		Liam Girdwood <lrg@slimlogic.co.uk>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */
#ifndef _ABE_TYPEDEF_H_
#define _ABE_TYPEDEF_H_
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
typedef ABE_uchar *pABE_uchar;
typedef ABE_char *pABE_char;
typedef ABE_uint16 *pABE_uint16;
typedef ABE_int16 *pABE_int16;
typedef ABE_int32 *pABE_int32;
typedef ABE_uint32 *pABE_uint32;
/*
 * Commonly used structures
 */
typedef struct abetaskTag {
	/* 0 ... Index of called function */
	ABE_uint16 iF;
	/* 2 ... for INITPTR of A0 */
	ABE_uint16 A0;
	/* 4 ... for INITPTR of A1 */
	ABE_uint16 A1;
	/* 6 ... for INITPTR of A2 & A3 */
	ABE_uint16 A2_3;
	/* 8 ... for INITPTR of A4 & A5 */
	ABE_uint16 A4_5;
	/* 10 ... for INITREG of R0, R1, R2, R3 */
	ABE_uint16 R;
	/* 12 */
	ABE_uint16 misc0;
	/* 14 */
	ABE_uint16 misc1;
} ABE_STask;
typedef ABE_STask *pABE_STask;
typedef ABE_STask **ppABE_STask;
typedef struct {
	/* 0 */
	ABE_uint16 drift_ASRC;
	/* 2 */
	ABE_uint16 drift_io;
	/* 4 "Function index" of XLS sheet "Functions" */
	ABE_uchar io_type_idx;
	/* 5 1 = MONO or Stereo1616, 2= STEREO, ... */
	ABE_uchar samp_size;
	/* 6 drift "issues" for ASRC */
	ABE_int16 flow_counter;
	/* 8 address for IRQ or DMArequests */
	ABE_uint16 hw_ctrl_addr;
	/* 10 DMA request bit-field or IRQ (DSP/MCU) */
	ABE_uchar atc_irq_data;
	/* 11 0 = Read, 3 = Write */
	ABE_uchar direction_rw;
	/* 12 */
	ABE_uchar repeat_last_samp;
	/* 13 12 at 48kHz, ... */
	ABE_uchar nsamp;
	/* 14 nsamp x samp_size */
	ABE_uchar x_io;
	/* 15 ON = 0x80, OFF = 0x00 */
	ABE_uchar on_off;
	/* 16 For Slimbus and TDM purpose */
	ABE_uint16 split_addr1;
	/* 18 */
	ABE_uint16 split_addr2;
	/* 20 */
	ABE_uint16 split_addr3;
	/* 22 */
	ABE_uchar before_f_index;
	/* 23 */
	ABE_uchar after_f_index;
	/* 24 SM/CM INITPTR field */
	ABE_uint16 smem_addr1;
	/* 26 in bytes */
	ABE_uint16 atc_address1;
	/* 28 DMIC_ATC_PTR, MCPDM_UL_ATC_PTR, ... */
	ABE_uint16 atc_pointer_saved1;
	/* 30 samp_size (except in TDM or Slimbus) */
	ABE_uchar data_size1;
	/* 31 "Function index" of XLS sheet "Functions" */
	ABE_uchar copy_f_index1;
	/* 32 For Slimbus and TDM purpose */
	ABE_uint16 smem_addr2;
	/* 34 */
	ABE_uint16 atc_address2;
	/* 36 */
	ABE_uint16 atc_pointer_saved2;
	/* 38 */
	ABE_uchar data_size2;
	/* 39 */
	ABE_uchar copy_f_index2;
} ABE_SIODescriptor;
/* [w] asrc output used for the next asrc call (+/- 1 / 0) */
#define drift_asrc_ 0
/* [w] asrc output used for controlling the number of samples to be
    exchanged (+/- 1 / 0) */
#define drift_io_ 2
/* address of the IO subroutine */
#define io_type_idx_ 4
#define samp_size_ 5
/* flow error counter */
#define flow_counter_ 6
/* dmareq address or host irq buffer address (atc address) */
#define hw_ctrl_addr_ 8
/* data content to be loaded to "hw_ctrl_addr" */
#define atc_irq_data_ 10
/* read dmem =0, write dmem =3 (atc offset of the access pointer) */
#define direction_rw_ 11
/* flag set to allow repeating the last sample on downlink paths */
#define repeat_last_samp_ 12
/* number of samples (either mono stereo...) */
#define nsamp_ 13
/* x number of raw DMEM data moved */
#define x_io_ 14
#define on_off_ 15
/* internal smem buffer initptr pointer index */
#define split_addr1_ 16
/* internal smem buffer initptr pointer index */
#define split_addr2_ 18
/* internal smem buffer initptr pointer index */
#define split_addr3_ 20
/* index of the copy subroutine */
#define before_f_index_ 22
/* index of the copy subroutine */
#define after_f_index_ 23
#define minidesc1_ 24
/* internal smem buffer initptr pointer index */
#define rel_smem_ 0
/* atc descriptor address (byte address x4) */
#define rel_atc_ 2
/* location of the saved ATC pointer (+debug info) */
#define rel_atc_saved 4
/* size of each sample (1:mono/1616 2:stereo ... ) */
#define rel_size_ 6
/* index of the copy subroutine */
#define rel_f_ 7
#define s_mem_mm_ul 24
#define s_mm_ul_size 30
#define minidesc2_ 32
#define Struct_Size 40
typedef struct {
	/* 0: [W] asrc output used for the next ASRC call (+/- 1 / 0) */
	ABE_uint16 drift_ASRC;
	/* 2: [W] asrc output used for controlling the number of
	   samples to be exchanged (+/- 1 / 0) */
	ABE_uint16 drift_io;
	/* 4: DMAReq address or HOST IRQ buffer address (ATC ADDRESS) */
	ABE_uint16 hw_ctrl_addr;
	/* 6: index of the copy subroutine */
	ABE_uchar copy_func_index;
	/* 7: X number of SMEM samples to move */
	ABE_uchar x_io;
	/* 8: 0 for mono data, 1 for stereo data */
	ABE_uchar data_size;
	/* 9: internal SMEM buffer INITPTR pointer index */
	ABE_uchar smem_addr;
	/* 10: data content to be loaded to "hw_ctrl_addr" */
	ABE_uchar atc_irq_data;
	/* 11: ping/pong buffer flag */
	ABE_uchar counter;
	/* 12: current Base address of the working buffer */
	ABE_uint16 workbuff_BaseAddr;
	/* 14: samples left in the working buffer */
	ABE_uint16 workbuff_Samples;
	/* 16: Base address of the ping/pong buffer 0 */
	ABE_uint16 nextbuff0_BaseAddr;
	/* 18: samples available in the ping/pong buffer 0 */
	ABE_uint16 nextbuff0_Samples;
	/* 20: Base address of the ping/pong buffer 1 */
	ABE_uint16 nextbuff1_BaseAddr;
	/* 22: samples available in the ping/pong buffer 1 */
	ABE_uint16 nextbuff1_Samples;
} ABE_SPingPongDescriptor;
#endif/* _ABE_TYPEDEF_H_ */
