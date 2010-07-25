/*
 * ==========================================================================
 *               Texas Instruments OMAP(TM) Platform Firmware
 * (c) Copyright 2009, Texas Instruments Incorporated.  All Rights Reserved.
 *
 *  Use of this firmware is controlled by the terms and conditions found
 *  in the license agreement under which this firmware has been supplied.
 * ==========================================================================
 */

#include "abe_main.h"
#include <linux/io.h>

void __iomem *io_base;

void abe_init_mem()
{
	io_base = ioremap(L4_ABE_44XX_PHYS, SZ_1M);
}

#define ABE_PMEM_BASE_OFFSET_MPU	0xe0000
#define ABE_CMEM_BASE_OFFSET_MPU	0xa0000
#define ABE_SMEM_BASE_OFFSET_MPU	0xc0000
#define ABE_DMEM_BASE_OFFSET_MPU	0x80000
#define ABE_ATC_BASE_OFFSET_MPU		0xf1000

#if 0
/*
 *  ABE_TRANSLATE_TO_XMEM_FORMAT
 *
 *  Parameter  :
 *  Operations :
 *      translates a floating point data to the cmem/smem/dmem data format
 *  Return value :
 *      None.
 */
void abe_translate_to_xmem_format(abe_int32 memory_bank, abe_float fc, abe_uint32 *c)
{
	abe_int32 l;
	abe_float afc;

	l = 0;
	afc = absolute(fc);

	switch (memory_bank) {
	case ABE_CMEM:
		if (afc >= 1.0 && afc < 32.0) {
			/* ALU post shifter +6 */
			l = (abe_int32)(fc * (1 << 16));
			l = (l << 2) + 2;
		} else if (afc >= 0.5 && afc < 1.0) {
			/* ALU post shifter +1 */
			l = (abe_int32)(fc * (1 << 21));
			l = (l << 2) + 1;
		} else if (afc >= 0.25 && afc < 0.5) {
			/* ALU post shifter 0 */
			l = (abe_int32)(fc * (1 << 22));
			l = (l << 2) + 0;
		} else if (afc < 0.25) {
			/* ALU post shifter -6 */
			l = (abe_int32)(fc * (1 << 28));
			l = (l << 2) + 3;
		}
		break;
	case ABE_SMEM:
		/* Q23 data format */
		l = (abe_int32)(fc * (1 << 23));
		break;
	case ABE_DMEM:
		/* Q31 data format (1<<31)=0 */
		l = (abe_int32)(fc * 2* (1 << 30));
		break;
	default: /* ABE_PMEM */
		/* Q31 data format */
		l = (abe_int32)(2 * fc * 2* (1 << 30));
		break;
	}

	*c = l;
}

/*
 *  ABE_TRANSLATE_GAIN_FORMAT
 *
 *  Parameter  :
 *  Operations :
 *    f: original format name for gain or frequency.
 *       1=linear ABE => decibels
 *       2=decibels  => linear ABE firmware format
 *
 *      lin = power(2, decibel/602); lin = [0.0001 .. 30.0]
 *      decibel = 6.02 * log2(lin), decibel = [-70 .. +30]
 *
 *    g1: pointer to the original data
 *    g2: pointer to the translated gain data
 *
 *  Return value :
 *      None.
 */
void abe_translate_gain_format(abe_uint32 f, abe_float g1, abe_float *g2)
{
	abe_float g, frac_part, gg1, gg2;
	abe_int32 int_part, i;

	#define C_20LOG2 ((abe_float)6.020599913)

	gg1 = (g1);
	int_part = 0;
	frac_part = gg2 = 0;

	switch (f) {
	case DECIBELS_TO_LINABE:
		g = gg1 / C_20LOG2;
		int_part = (abe_int32) g;
		frac_part = g - int_part;

		gg2 = abe_power_of_two(frac_part);

		if (int_part > 0)
			gg2 = gg2 * (1 << int_part);
		else
			gg2 = gg2 / (1 << (-int_part));

		break;
	case LINABE_TO_DECIBELS:
		if (gg1 == 1.0) {
			gg2 = 0.0;
			return;
		}

		/* find the power of 2 by iteration */
		if (gg1 > 1.0) {
			for (i = 0; i < 63; i++) {
				if ((1 << i) > gg1) {
					int_part = (i-1);
					frac_part = gg1 / (1 << int_part);
					break;
				}
			}
			gg2 = C_20LOG2 * (int_part + abe_log_of_two(frac_part));
		} else {
			for (i = 0; i < 63; i++) {
				if (((1 << i) * gg1) > 1) {
					int_part = i;
					frac_part = gg1 * (1 << int_part);
					break;
				}
			}
			/* compute the dB using polynomial
			 * interpolation in the [1..2] range
			 */
			gg2 = C_20LOG2 * (((-1)*int_part) + abe_log_of_two(frac_part));}
		break;
	}

	*g2 = gg2;
 }

/*
 *  ABE_TRANSLATE_RAMP_FORMAT
 *
 *  Parameter  :
 *  Operations :
 *    f: original format name for gain or frequency.
 *       1=ABE IIR coef => microseconds
 *       2=microseconds => ABE IIR coef
 *
 *    g1: pointer to the original data
 *    g2: pointer to the translated gain data
 *
 *  Return value :
 *      None.
 */
void abe_translate_ramp_format(abe_float ramp, abe_float *ramp_iir1)
{
	*ramp_iir1   = 0.125;
}

/*
 *  ABE_TRANSLATE_EQU_FORMAT
 *
 *  Parameter  :
 *  Operations :
 *     Translate
 *
 *  Return value :
 *      None.
 */
void abe_translate_equ_format(abe_equ_t *p, abe_float *iir, abe_uint32 n)
{
#if 0
	switch (p->type
typedef struct {
		abe_iir_t equ_param1;	   /* type of filter */
		abe_uint32 equ_param2;	  /* filter length */
		union {			 /* parameters are the direct and recursive coefficients in */
		       abe_int32 type1 [NBEQ1];     /* Q6.26 integer fixed-point format. */
		       struct {
		       abe_int32 freq [NBEQ2];      /* parameters are the frequency (type "freq") and Q factors */
		       abe_int32 gain [NBEQ2];      /* (type "gain") of each band. */
		       } type2;
		} coef;
		abe_int32 equ_param3;
	} abe_equ_t;

	Fs = 48000;
	f0 = 19000;
	Q = sqrt(0.5)
	dBgain = -40


	A = sqrt (power (10, dBgain/20));
	w0 = 2*pi*f0/Fs
	alpha = sin(w0) / (2*Q);

	%PeakingEQ ==========================================
	b_peak = print_ABE_data ([1+alpha*A -2*cos(w0) 1-alpha*A],3);
	a_peak = print_ABE_data ([1+alpha/A -2*cos(w0) 1-alpha/A],3);

#endif
}
#endif

/*
 *  ABE_FPRINTF
 *
 *  Parameter  :
 *      character line to be printed
 *
 *  Operations :
 *
 *  Return value :
 *      None.
 */
#if 0
void abe_fprintf(char *line)
{
	switch (abe_dbg_output) {
	case NO_OUTPUT:
		break;
	case TERMINAL_OUTPUT:
		break;
	case LINE_OUTPUT:
		break;
	case DEBUG_TRACE_OUTPUT:
		break;
	default:
		break;
	}
}
#endif

/*
 *  ABE_READ_FEATURE_FROM_PORT
 *
 *  Parameter  :
 *      x : d
 *
 *  Operations :
 *
 *
 *  Return value :
 *
 */
void abe_read_feature_from_port(abe_uint32 x)
{
	just_to_avoid_the_many_warnings = x;
}

/*
 *  ABE_WRITE_FEATURE_TO_PORT
 *
 *  Parameter  :
 *      x : d
 *
 *  Operations :
 *
 *
 *  Return value :
 *
 */
void abe_write_feature_to_port(abe_uint32 x)
{
	just_to_avoid_the_many_warnings = x;
}

/*
 *  ABE_READ_FIFO
 *
 *  Parameter  :
 *      x : d
 *
 *  Operations :
 *
 *
 *  Return value :
 *
 */
void abe_read_fifo(abe_uint32 x)
{
	just_to_avoid_the_many_warnings = x;
}

/*
 *  ABE_WRITE_FIFO
 *
 *  Parameter  :
 *      x : d
 *
 *  Operations :
 *
 *
 *  Return value :
 *
 */
void abe_write_fifo(abe_uint32 x)
{
	just_to_avoid_the_many_warnings = x;
}

/*
 *  ABE_BLOCK_COPY
 *
 *  Parameter  :
 *      direction of the data move (Read/Write)
 *      memory bank among PMEM, DMEM, CMEM, SMEM, ATC/IO
 *      address of the memory copy (byte addressing)
 *      long pointer to the data
 *      number of data to move
 *
 *  Operations :
 *      block data move
 *
 *  Return value :
 *      none
 */
void abe_block_copy(abe_int32 direction, abe_int32 memory_bank, abe_int32 address, abe_uint32 *data, abe_uint32 nb_bytes)
{
#if PC_SIMULATION
	abe_uint32 *smem_tmp, smem_offset, smem_base, nb_words48;

	if (direction == COPY_FROM_HOST_TO_ABE) {
		switch (memory_bank) {
		case ABE_PMEM:
			target_server_write_pmem(address/4, data, nb_bytes/4);
			break;
		case ABE_CMEM:
			target_server_write_cmem(address/4, data, nb_bytes/4);
			break;
		case ABE_ATC:
			target_server_write_atc(address/4, data, nb_bytes/4);
			break;
		case ABE_SMEM:
			nb_words48 = (nb_bytes +7)>>3;
			/* temporary buffer manages the OCP access to 32bits boundaries */
			smem_tmp = malloc(nb_bytes + 64);
			/* address is on SMEM 48bits lines boundary */
			smem_base = address - (address & 7);
			target_server_read_smem(smem_base/8, smem_tmp, 2 + nb_words48);
			smem_offset = address & 7;
			memcpy(&(smem_tmp[smem_offset>>2]), data, nb_bytes);
			target_server_write_smem(smem_base/8, smem_tmp, 2 + nb_words48);
			free(smem_tmp);
			break;
		case ABE_DMEM:
			target_server_write_dmem(address, data, nb_bytes);
			break;
		default:
			abe_dbg_param |= ERR_LIB;
			abe_dbg_error_log(ABE_BLOCK_COPY_ERR);
			break;
		}
	} else {
		switch (memory_bank) {
		case ABE_PMEM:
			target_server_read_pmem(address/4, data, nb_bytes/4);
			break;
		case ABE_CMEM:
			target_server_read_cmem(address/4, data, nb_bytes/4);
			break;
		case ABE_ATC:
			target_server_read_atc(address/4, data, nb_bytes/4);
			break;
		case ABE_SMEM:
			nb_words48 = (nb_bytes +7)>>3;
			/* temporary buffer manages the OCP access to 32bits boundaries */
			smem_tmp = malloc(nb_bytes + 64);
			/* address is on SMEM 48bits lines boundary */
			smem_base = address - (address & 7);
			target_server_read_smem(smem_base/8, smem_tmp, 2 + nb_words48);
			smem_offset = address & 7;
			memcpy(data, &(smem_tmp[smem_offset>>2]), nb_bytes);
			free(smem_tmp);
			break;
		case ABE_DMEM:
			target_server_read_dmem(address, data, nb_bytes);
			break;
		default:
			abe_dbg_param |= ERR_LIB;
			abe_dbg_error_log(ABE_BLOCK_COPY_ERR);
			break;
		}
	}
#else
	abe_uint32 i;
	abe_uint32 base_address = 0, *src_ptr, *dst_ptr, n;

	switch (memory_bank) {
	case ABE_PMEM:
		base_address = (abe_uint32) io_base + ABE_PMEM_BASE_OFFSET_MPU;
		break;
	case ABE_CMEM:
		base_address = (abe_uint32) io_base + ABE_CMEM_BASE_OFFSET_MPU;
		break;
	case ABE_SMEM:
		base_address = (abe_uint32) io_base + ABE_SMEM_BASE_OFFSET_MPU;
		break;
	case ABE_DMEM:
		base_address = (abe_uint32) io_base + ABE_DMEM_BASE_OFFSET_MPU;
		break;
	case ABE_ATC:
		base_address = (abe_uint32) io_base + ABE_ATC_BASE_OFFSET_MPU;
		break;
	default:
		base_address = (abe_uint32) io_base + ABE_SMEM_BASE_OFFSET_MPU;
		abe_dbg_param |= ERR_LIB;
		abe_dbg_error_log(ABE_BLOCK_COPY_ERR);
		break;
	}

	if (direction == COPY_FROM_HOST_TO_ABE) {
		dst_ptr = (abe_uint32 *)(base_address + address);
		src_ptr = (abe_uint32 *)data;
	} else {
		dst_ptr = (abe_uint32 *)data;
		src_ptr = (abe_uint32 *)(base_address + address);
	}

	n = (nb_bytes/4);

	for (i = 0; i < n; i++)
		*dst_ptr++ = *src_ptr++;

#endif
}
/*
 *	ABE_RESET_MEM
 *
 *	Parameter  :
 *	memory bank among DMEM, SMEM
 *	address of the memory copy (byte addressing)
 *	number of data to move
 *
 *	Operations :
 *	reset memory
 *
 *	Return value :
 *	none
 */

void  abe_reset_mem(abe_int32 memory_bank, abe_int32 address, abe_uint32 nb_bytes)
{
#if PC_SIMULATION
	extern void target_server_write_smem(abe_uint32 address_48bits, abe_uint32 *data, abe_uint32 nb_words_48bits);
	extern void target_server_write_dmem(abe_uint32 address_byte, abe_uint32 *data, abe_uint32 nb_byte);

	abe_uint32 *smem_tmp, *data, smem_offset, smem_base, nb_words48;

	data = calloc(nb_bytes, 1);

	switch (memory_bank) {
	case ABE_SMEM:
		nb_words48 = (nb_bytes +7)>>3;
		/* temporary buffer manages the OCP access to 32bits boundaries */
		smem_tmp = malloc (nb_bytes + 64);
		/* address is on SMEM 48bits lines boundary */
		smem_base = address - (address & 7);
		target_server_read_smem (smem_base/8, smem_tmp, 2 + nb_words48);
		smem_offset = address & 7;
		memcpy (&(smem_tmp[smem_offset>>2]), data, nb_bytes);
		target_server_write_smem (smem_base/8, smem_tmp, 2 + nb_words48);
		free (smem_tmp);
		break;
	case ABE_DMEM:
		target_server_write_dmem(address, data, nb_bytes);
		break;
	default:
		abe_dbg_param |= ERR_LIB;
		abe_dbg_error_log(ABE_BLOCK_COPY_ERR);
	}
	free(data);
#else
	abe_uint32 i;
	abe_uint32 *dst_ptr, n;
	abe_uint32 base_address = 0;

	switch (memory_bank) {
	case ABE_SMEM:
		base_address = (abe_uint32) io_base + ABE_SMEM_BASE_OFFSET_MPU;
		break;
	case ABE_DMEM:
		base_address = (abe_uint32) io_base + ABE_DMEM_BASE_OFFSET_MPU;
		break;
	case ABE_CMEM:
		base_address = (abe_uint32) io_base + ABE_CMEM_BASE_OFFSET_MPU;
		break;
	}

	dst_ptr = (abe_uint32 *) (base_address + address);

	n = (nb_bytes/4);

	for (i = 0; i < n; i++)
		*dst_ptr++ = 0;
#endif
}

/*
 *  ABE_MONITORING
 *
 *  Parameter  :
 *
 *  Operations :
 *      checks the internal status of ABE and HAL
 *
 *  Return value :
 *      Call Backs on Errors
 */
void abe_monitoring(void)
{
	abe_dbg_param = 0;
}

/*
 *  ABE_FORMAT_SWITCH
 *
 *  Parameter  :
 *
 *  Operations :
 *      translates the sampling and data length to ITER number for the DMA
 *      and the multiplier factor to apply during data move with DMEM
 *
 *  Return value :
 *      Call Backs on Errors
 */
void abe_format_switch(abe_data_format_t *f, abe_uint32 *iter, abe_uint32 *mulfac)
{
	abe_uint32 n_freq;

#if FW_SCHED_LOOP_FREQ==4000
	switch (f->f) {
	/* nb of samples processed by scheduling loop */
	case  8000: n_freq = 2;  break;
	case 16000: n_freq = 4;  break;
	case 24000: n_freq = 6;  break;
	case 44100: n_freq = 12; break;
	case 96000: n_freq = 24; break;
	default /*case 48000*/: n_freq = 12; break;
	}
#else
	n_freq = 0;	/* erroneous cases */
#endif
	switch (f->samp_format) {
	case MONO_MSB:
	case MONO_RSHIFTED_16:
		*mulfac = 1;
		break;
	case STEREO_16_16:
		*mulfac = 1;
		break;
	case STEREO_MSB:
	case STEREO_RSHIFTED_16:
		*mulfac = 2;
		break;
	case THREE_MSB:
		*mulfac = 3;
		break;
	case FOUR_MSB:
		*mulfac = 4;
		break;
	case FIVE_MSB:
		*mulfac = 5;
		break;
	case SIX_MSB:
		*mulfac = 6;
		break;
	case SEVEN_MSB:
		*mulfac = 7;
		break;
	case EIGHT_MSB:
		*mulfac = 8;
		break;
	case NINE_MSB:
		*mulfac = 9;
		break;
	default:
		*mulfac = 1;
		break;
	}

	*iter = (n_freq * (*mulfac));
}

/*
 *  ABE_DMA_PORT_ITERATION
 *
 *  Parameter  :
 *
 *  Operations :
 *      translates the sampling and data length to ITER number for the DMA
 *
 *  Return value :
 *      Call Backs on Errors
 */
abe_uint32 abe_dma_port_iteration(abe_data_format_t *f)
{
	abe_uint32 iter, mulfac;

	abe_format_switch(f, &iter, &mulfac);

	return iter;
}

/*
 *  ABE_DMA_PORT_ITER_FACTOR
 *
 *  Parameter  :
 *
 *  Operations :
 *      returns the multiplier factor to apply during data move with DMEM
 *
 *  Return value :
 *      Call Backs on Errors
 */
abe_uint32 abe_dma_port_iter_factor(abe_data_format_t *f)
{
	abe_uint32 iter, mulfac;

	abe_format_switch(f, &iter, &mulfac);

	return mulfac;
}

/*
 *  ABE_DMA_PORT_COPY_SUBROUTINE_ID
 *
 *  Parameter  :
 *
 *  Operations :
 *      returns the index of the function doing the copy in I/O tasks
 *
 *  Return value :
 *      Call Backs on Errors
 */
abe_uint32 abe_dma_port_copy_subroutine_id(abe_port_id port_id)
{
	abe_uint32 sub_id;
	if (abe_port[port_id].protocol.direction == ABE_ATC_DIRECTION_IN) {
		switch (abe_port[port_id].format.samp_format) {
		case MONO_MSB:
			sub_id = D2S_MONO_MSB_CFPID;
			break;
		case MONO_RSHIFTED_16:
			sub_id = D2S_MONO_RSHIFTED_16_CFPID;
			break;
		case STEREO_RSHIFTED_16:
			sub_id = D2S_STEREO_RSHIFTED_16_CFPID;
			break;
		case STEREO_16_16:
			sub_id = D2S_STEREO_16_16_CFPID;
			break;
		case STEREO_MSB:
			sub_id = D2S_STEREO_MSB_CFPID;
			break;
		case SIX_MSB:
			if (port_id == DMIC_PORT) {
				sub_id = COPY_DMIC_CFPID;
				break;
			}
		default:
			sub_id = NULL_COPY_CFPID;
			break;
		}
	} else {
		switch (abe_port[port_id].format.samp_format) {
		case MONO_MSB:
			sub_id = S2D_MONO_MSB_CFPID;
			break;
		case MONO_RSHIFTED_16:
			sub_id = S2D_MONO_RSHIFTED_16_CFPID;
			break;
		case STEREO_RSHIFTED_16:
			sub_id = S2D_STEREO_RSHIFTED_16_CFPID;
			break;
		case STEREO_16_16:
			sub_id = S2D_STEREO_16_16_CFPID;
			break;
		case STEREO_MSB:
			sub_id = S2D_STEREO_MSB_CFPID;
			break;
		case SIX_MSB:
			if (port_id == PDM_DL_PORT) {
				sub_id = COPY_MCPDM_DL_CFPID;
				break;
			}
			if (port_id == MM_UL_PORT) {
				sub_id = COPY_MM_UL_CFPID;
				break;
			}
		case THREE_MSB:
		case FOUR_MSB:
		case FIVE_MSB:
		case SEVEN_MSB:
		case EIGHT_MSB:
		case NINE_MSB:
			sub_id = COPY_MM_UL_CFPID;
			break;
		default:
			sub_id = NULL_COPY_CFPID;
			break;
		}
	}
	return sub_id;
}
