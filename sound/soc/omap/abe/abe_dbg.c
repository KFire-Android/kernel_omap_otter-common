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
 *  ABE_DBG_LOG
 *
 *  Parameter  :
 *	x : data to be logged
 *
 *	abe_dbg_activity_log : global circular buffer holding the data
 *	abe_dbg_activity_log_write_pointer : circular write pointer
 *
 *  Operations :
 *	saves data in the log file
 *
 *  Return value :
 *	none
 */

void abe_dbg_log(abe_uint32 x, abe_uint32 y, abe_uint32 z, abe_uint32 t)
{
	abe_uint32 time_stamp, data;

	abe_block_copy(COPY_FROM_ABE_TO_HOST, ABE_DMEM, D_loopCounter_ADDR,
				(abe_uint32 *)&time_stamp, sizeof (time_stamp));
	abe_dbg_activity_log [abe_dbg_activity_log_write_pointer] = time_stamp;
	abe_block_copy(COPY_FROM_HOST_TO_ABE, ABE_DMEM, D_DEBUG_HAL_TASK_ADDR +
		(abe_dbg_activity_log_write_pointer<<2),
			(abe_uint32 *)&time_stamp, sizeof (time_stamp));
	abe_dbg_activity_log_write_pointer ++;

	data = ((x&MAX_UINT8)<< 24) | ((y&MAX_UINT8)<< 16) | ((z&MAX_UINT8)<< 8) | (t&MAX_UINT8);
	abe_dbg_activity_log[abe_dbg_activity_log_write_pointer] = data;
	abe_block_copy(COPY_FROM_HOST_TO_ABE, ABE_DMEM, D_DEBUG_HAL_TASK_ADDR +
		(abe_dbg_activity_log_write_pointer<<2),
			(abe_uint32 *)&data, sizeof (data));
	abe_dbg_activity_log_write_pointer ++;

	if (abe_dbg_activity_log_write_pointer >= D_DEBUG_HAL_TASK_sizeof)
		abe_dbg_activity_log_write_pointer = 0;

}

/*
 *  ABE_DEBUG_OUTPUT_PINS
 *
 *  Parameter  :
 *	x : d
 *
 *  Operations :
 *	set the debug output pins of AESS
 *
 *  Return value :
 *
 */
void	abe_debug_output_pins (abe_uint32 x)
{
just_to_avoid_the_many_warnings = x;
}


/*
 *  ABE_DBG_ERROR_LOG
 *
 *  Parameter  :
 *	x : d
 *
 *  Operations :
 *	log the error codes
 *
 *  Return value :
 *
 */
void	abe_dbg_error_log (abe_uint32 x)
{
	abe_dbg_log (x,  MAX_UINT8,  MAX_UINT8,  MAX_UINT8);
}


/*
 *  ABE_DEBUGGER
 *
 *  Parameter  :
 *	x : d
 *
 *  Operations :
 *
 *
 *  Return value :
 *
 */
void	abe_debugger (abe_uint32 x)
{
just_to_avoid_the_many_warnings = x;
}




/*
	S = power (2, 31) * 0.25;
	N =  4; B = 2; F=[1/N 1/N]; gen_and_save('dbg_8k_2.txt',  B, F, N, S);
	N =  8; B = 2; F=[1/N 2/N]; gen_and_save('dbg_16k_2.txt', B, F, N, S);
	N = 12; B = 2; F=[1/N 2/N]; gen_and_save('dbg_48k_2.txt', B, F, N, S);
	N = 60; B = 2; F=[4/N 8/N]; gen_and_save('dbg_amic.txt', B, F, N, S);
	N = 10; B = 6; F=[1/N 2/N 3/N 1/N 2/N 3/N]; gen_and_save('dbg_dmic.txt', B, F, N, S);
*/
void abe_load_embeddded_patterns (void)
{
    abe_uint32 i;

#define patterns_96k_len 48
const long patterns_96k[patterns_96k_len] = {
	1620480,	1452800,
	1452800,	838656,
	1186304,	0,
	838656,	-838912,
	434176,	-1453056,
	0,	-1677824,
	-434432,	-1453056,
	-838912,	-838912,
	-1186560,	-256,
	-1453056,	838656,
	-1620736,	1452800,
	-1677824,	1677568,
	-1620736,	1452800,
	-1453056,	838656,
	-1186560,	0,
	-838912,	-838912,
	-434432,	-1453056,
	-256,	-1677824,
	434176,	-1453056,
	838656,	-838912,
	1186304,	-256,
	1452800,	838656,
	1620480,	1452800,
	1677568,	1677568,
};
#define patterns_48k_len 24
const long patterns_48k[patterns_48k_len] = {
	1452800,	838656,
	838656,	-838912,
	0,	-1677824,
	-838912,	-838912,
	-1453056,	838656,
	-1677824,	1677568,
	-1453056,	838656,
	-838912,	-838912,
	-256,	-1677824,
	838656,	-838912,
	1452800,	838656,
	1677568,	1677568,
};
#define patterns_24k_len 12
	const long patterns_24k[patterns_24k_len] = {
	838656,		-838912,
	-838912,	-838912,
	-1677824,	1677568,
	-838912,	-838912,
	838656,		-838912,
	1677568,	1677568,
};
#define patterns_16k_len 8
const long patterns_16k[patterns_16k_len] = {
	0,		0,
	-1677824,	-1677824,
	-256,		-256,
	1677568,	1677568,
};
#define patterns_8k_len 4
const long patterns_8k[patterns_8k_len] = {
	1677568,	-1677824,
	1677568,	1677568,
};

	for (i = 0; i < patterns_8k_len; i++)
		abe_block_copy(COPY_FROM_HOST_TO_ABE, ABE_SMEM,
			(S_DBG_8K_PATTERN_ADDR *8)+(i*4),
				(abe_uint32 *)(&(patterns_8k[i])), 4);
	for (i = 0; i < patterns_16k_len; i++)
		abe_block_copy(COPY_FROM_HOST_TO_ABE, ABE_SMEM,
			(S_DBG_16K_PATTERN_ADDR *8)+(i*4),
				(abe_uint32 *)(&(patterns_16k[i])), 4);
	for (i = 0; i < patterns_24k_len; i++)
		abe_block_copy(COPY_FROM_HOST_TO_ABE, ABE_SMEM,
			(S_DBG_24K_PATTERN_ADDR *8)+(i*4),
				(abe_uint32 *)(&(patterns_24k[i])), 4);
	for (i = 0; i < patterns_48k_len; i++)
		abe_block_copy(COPY_FROM_HOST_TO_ABE, ABE_SMEM,
			(S_DBG_48K_PATTERN_ADDR *8)+(i*4),
				(abe_uint32 *)(&(patterns_48k[i])), 4);
	for (i = 0; i < patterns_96k_len; i++)
		abe_block_copy(COPY_FROM_HOST_TO_ABE, ABE_SMEM,
			(S_DBG_96K_PATTERN_ADDR *8)+(i*4),
				(abe_uint32 *)(&(patterns_96k[i])), 4);
}
