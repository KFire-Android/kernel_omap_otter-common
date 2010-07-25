/*
 * ==========================================================================
 *               Texas Instruments OMAP(TM) Platform Firmware
 * (c) Copyright 2009, Texas Instruments Incorporated.  All Rights Reserved.
 *
 *  Use of this firmware is controlled by the terms and conditions found
 *  in the license agreement under which this firmware has been supplied.
 * ==========================================================================
 */

#ifdef __cplusplus
extern "C" {
#endif

void abe_init_mem(void);
void abe_translate_gain_format(abe_uint32 f, abe_float g1, abe_float *g2);
void abe_translate_ramp_format(abe_float g1, abe_float *g2);

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
void abe_fprintf(char *line);

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
void abe_read_feature_from_port(abe_uint32 x);

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
void abe_write_feature_to_port(abe_uint32 x);

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
void abe_read_fifo(abe_uint32 x);

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
void abe_write_fifo(abe_uint32 x);

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
void abe_block_copy(abe_int32 direction, abe_int32 memory_bank, abe_int32 address, abe_uint32 *data, abe_uint32 nb);

/*
 * ABE_RESET_MEM
 *
 *Parameter:
 * memory bank among DMEM, SMEM
 * address of the memory copy (byte addressing)
 * number of data to move
 *
 * Operations:
 * reset memory
 *
 * Return value:
 * none
 */
void abe_reset_mem(abe_int32 memory_bank, abe_int32 address, abe_uint32 nb_bytes);

#ifdef __cplusplus
}
#endif
