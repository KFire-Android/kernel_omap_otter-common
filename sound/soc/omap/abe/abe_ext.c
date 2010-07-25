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

/*
 *  ABE_DEFAULT_IRQ_PINGPONG_PLAYER
 *
 *
 *  Operations :
 *      generates data for the cache-flush buffer MODE 16+16
 *
 *  Return value :
 *      None.
 */
void abe_default_irq_pingpong_player(void)
{
	/* ping-pong access to MM_DL at 48kHz Mono with 20ms packet sizes */
	#define N_SAMPLES_MAX ((int)(1024))

	static abe_int32 idx;
	abe_uint32 i, dst, n_samples, n_bytes;
	abe_int32 temp [N_SAMPLES_MAX], audio_sample;
#define DATA_SIZE 20
	const abe_int32 audio_pattern [DATA_SIZE] =
	{
		0, 5063, 9630, 13254, 15581, 16383, 15581, 13254, 9630,
		5063, 0, -5063, -9630, -13254, -15581, -16383, -15581,
		-13254, -9630, -5063
	};

	/* read the address of the Pong buffer */
	abe_read_next_ping_pong_buffer(MM_DL_PORT, &dst, &n_bytes);

	n_samples = n_bytes / 4;
	/* generate a test pattern */
	for (i = 0; i < n_samples; i++) {
		audio_sample = audio_pattern [idx];
		idx = (idx >= (DATA_SIZE-1))? 0: (idx+1);
		temp[i] = ((audio_sample << 16) + audio_sample);
	}

	/* copy the pattern (flush it) to DMEM pointer update
	 * not necessary here because the buffer size do not
	 * change from one ping to the other pong
	 */
	abe_block_copy(COPY_FROM_HOST_TO_ABE, ABE_DMEM, dst, (abe_uint32 *)&(temp[0]), n_samples * 4);
	abe_set_ping_pong_buffer(MM_DL_PORT, n_bytes);
}

/*
 * ABE_DEFAULT_IRQ_PINGPONG_PLAYER_32BITS
 *
 * Operations:
 * generates data for the cache-flush buffer  MODE 32 BITS
 * Return value:
 * None.
 */
void abe_default_irq_pingpong_player_32bits(void)
{
/* ping-pong access to MM_DL at 48kHz Mono with 20ms packet sizes */
#define N_SAMPLES_MAX ((int)(1024))
	static abe_int32 idx;
	abe_uint32 i, dst, n_samples, n_bytes;
	abe_int32 temp[N_SAMPLES_MAX], audio_sample;
#define DATA_SIZE 20 /*  t = [0:N-1]/N; x = round(16383*sin(2*pi*t)) */
        const abe_int32 audio_pattern [DATA_SIZE] =
	{
		0, 5063, 9630, 13254, 15581, 16383, 15581, 13254,
		9630, 5063, 0, -5063, -9630, -13254, -15581, -16383,
		-15581, -13254, -9630, -5063
	};

	/* read the address of the Pong buffer */
	abe_read_next_ping_pong_buffer(MM_DL_PORT, &dst, &n_bytes);
	n_samples = n_bytes / 8;	/* each stereo sample weights 8 bytes (format 32|32) */

	/* generate a test pattern */
	for (i = 0; i < n_samples; i++) {
		/* circular addressing */
		audio_sample = audio_pattern[idx];
		idx = (idx >= (DATA_SIZE-1))? 0: (idx+1);
		temp[i*2 +0] = (audio_sample << 16);
		temp[i*2 +1] = (audio_sample << 16);
	}

	/* copy the pattern (flush it) to DMEM pointer update
	 * not necessary here because the buffer size do not
	 * change from one ping to the other pong
	 */
	abe_block_copy(COPY_FROM_HOST_TO_ABE, ABE_DMEM, dst,
			(abe_uint32 *)&(temp[0]), n_samples * 4 *2);

	abe_set_ping_pong_buffer(MM_DL_PORT, n_bytes);
}
/*
 * ABE_DEFAULT_IRQ_APS_ADAPTATION
 *
 * Operations :
 * updates the APS filter and gain
 *
 * Return value :
 * None.
 */
void abe_default_irq_aps_adaptation(void)
{
}

/*
 *  ABE_READ_SYS_CLOCK
 *
 *  Parameter  :
 *      pointer to the system clock
 *
 *  Operations :
 *      returns the current time indication for the LOG
 *
 *  Return value :
 *      None.
 */
void abe_read_sys_clock(abe_micros_t *time)
{
	static abe_micros_t clock;

	*time = clock;
	clock ++;
}

/*
 *  ABE_APS_TUNING
 *
 *  Parameter  :
 *
 *
 *  Operations :
 *
 *
 *  Return value :
 *
 */
void abe_aps_tuning(void)
{
}

/**
* @fn abe_lock_executione()
*
*  Operations : set a spin-lock and wait in case of collision
*
*
* @see	ABE_API.h
*/
void abe_lock_execution(void)
{
}

/**
* @fn abe_unlock_executione()
*
*  Operations : reset a spin-lock (end of subroutine)
*
*
* @see	ABE_API.h
*/
void abe_unlock_execution(void)
{
}
