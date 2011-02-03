/*

  This file is provided under a dual BSD/GPLv2 license.  When using or
  redistributing this file, you may do so under either license.

  GPL LICENSE SUMMARY

  Copyright(c) 2010-2011 Texas Instruments Incorporated,
  All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
  The full GNU General Public License is included in this distribution
  in the file called LICENSE.GPL.

  BSD LICENSE

  Copyright(c) 2010-2011 Texas Instruments Incorporated,
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in
      the documentation and/or other materials provided with the
      distribution.
    * Neither the name of Texas Instruments Incorporated nor the names of
      its contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/slab.h>

#include "abe.h"
#include "abe_dbg.h"
#include "abe_mem.h"

/**
 * omap_aess_dbg_reset
 * @dbg: Pointer on abe debug handle
 *
 * Called in order to reset Audio Back End debug global data.
 * This ensures that ABE debug trace pointer is reset correctly.
 */
int omap_aess_dbg_reset(struct omap_aess_dbg *dbg)
{
	if (dbg == NULL)
		dbg = kzalloc(sizeof(struct omap_aess_dbg), GFP_KERNEL);
	dbg->activity_log_write_pointer = 0;
	dbg->mask = 0;

	return 0;
}

/**
 * omap_aess_connect_debug_trace
 * @abe: Pointer on abe handle
 * @dma2:pointer to the DMEM trace buffer
 *
 * returns the address and size of the real-time debug trace buffer,
 * the content of which will vary from one firmware release to another
 */
int omap_aess_connect_debug_trace(struct omap_aess *abe,
				 struct omap_aess_dma *dma2)
{
	/* return tohe base address of the ping buffer in L3 and L4 spaces */
	(*dma2).data = (void *)(omap_aess_map[OMAP_AESS_DMEM_DEBUG_FIFO_ID].offset +
		ABE_DEFAULT_BASE_ADDRESS_L3 + ABE_DMEM_BASE_OFFSET_MPU);
	(*dma2).l3_dmem = (void *)(omap_aess_map[OMAP_AESS_DMEM_DEBUG_FIFO_ID].offset +
		ABE_DEFAULT_BASE_ADDRESS_L3 + ABE_DMEM_BASE_OFFSET_MPU);
	(*dma2).l4_dmem = (void *)(omap_aess_map[OMAP_AESS_DMEM_DEBUG_FIFO_ID].offset +
		ABE_DEFAULT_BASE_ADDRESS_L4 + ABE_DMEM_BASE_OFFSET_MPU);
	(*dma2).iter = (omap_aess_map[OMAP_AESS_DMEM_DEBUG_FIFO_ID].bytes +
		omap_aess_map[OMAP_AESS_DMEM_DEBUG_FIFO_HAL_ID].bytes)>>2;

	return 0;
}
EXPORT_SYMBOL(omap_aess_connect_debug_trace);

/**
 * omap_aess_set_debug_trace
 * @dbg: Pointer on abe debug handle
 * @debug: debug log level
 *
 * Set the debug level for ABE trace. This level allows to manage the number
 * of information put inside the ABE trace buffer. This buffer can contains
 * both AESS firmware and MPU traces.
 */
int omap_aess_set_debug_trace(struct omap_aess_dbg *dbg, int debug)
{
	dbg->mask = debug;

	return 0;
}
EXPORT_SYMBOL(omap_aess_set_debug_trace);

/**
 * omap_aess_dbg_log - Log ABE trace inside circular buffer
 * @x: data to be logged
 * @y: data to be logged
 * @z: data to be logged
 * @t: data to be logged
 *  Parameter  :
 *
 *	abe_dbg_activity_log : global circular buffer holding the data
 *	abe_dbg_activity_log_write_pointer : circular write pointer
 *
 *	saves data in the log file
 */
void omap_aess_dbg_log(struct omap_aess *abe, u32 x, u32 y, u32 z, u32 t)
{
	u32 time_stamp, data;
	struct omap_aess_dbg *dbg = abe->dbg;

	if (dbg->activity_log_write_pointer >=
			((omap_aess_map[OMAP_AESS_DMEM_DEBUG_HAL_TASK_ID].bytes >> 2) - 2))
		dbg->activity_log_write_pointer = 0;

	/* copy in DMEM trace buffer and CortexA9 local buffer and a small 7
	   words circular buffer of the DMA trace ending with 0x55555555
	   (tag for last word) */
	omap_aess_mem_read(abe, omap_aess_map[OMAP_AESS_DMEM_LOOPCOUNTER_ID],
			  (u32 *) &time_stamp);
	dbg->activity_log[dbg->activity_log_write_pointer] = time_stamp;

	omap_abe_mem_write(abe, OMAP_ABE_DMEM,
			   omap_aess_map[OMAP_AESS_DMEM_DEBUG_HAL_TASK_ID].offset +
			   (dbg->activity_log_write_pointer << 2),
			   (u32 *) &time_stamp, sizeof(time_stamp));
	dbg->activity_log_write_pointer++;

	data = ((x & MAX_UINT8) << 24) | ((y & MAX_UINT8) << 16) |
		((z & MAX_UINT8) << 8)
		| (t & MAX_UINT8);
	dbg->activity_log[dbg->activity_log_write_pointer] = data;
	omap_abe_mem_write(abe, OMAP_ABE_DMEM,
			   omap_aess_map[OMAP_AESS_DMEM_DEBUG_HAL_TASK_ID].offset +
			   (dbg->activity_log_write_pointer << 2),
			   (u32 *) &data, sizeof(data));

	omap_abe_mem_write(abe, OMAP_ABE_DMEM,
			   omap_aess_map[OMAP_AESS_DMEM_DEBUG_FIFO_HAL_ID].offset +
			   ((dbg->activity_log_write_pointer << 2) &
			   (OMAP_ABE_D_DEBUG_FIFO_HAL_SIZE - 1)), (u32 *) &data,
			   sizeof(data));

	data = ABE_DBG_MAGIC_NUMBER;
	omap_abe_mem_write(abe, OMAP_ABE_DMEM,
			   omap_aess_map[OMAP_AESS_DMEM_DEBUG_FIFO_HAL_ID].offset +
			   (((dbg->activity_log_write_pointer + 1) << 2) &
			   (omap_aess_map[OMAP_AESS_DMEM_DEBUG_FIFO_HAL_ID].bytes - 1)),
			   (u32 *) &data, sizeof(data));
	dbg->activity_log_write_pointer++;

	if (dbg->activity_log_write_pointer >= (omap_aess_map[OMAP_AESS_DMEM_DEBUG_HAL_TASK_ID].bytes>>2))
		dbg->activity_log_write_pointer = 0;
}

/**
 * omap_aess_dbg_error_log -  Log ABE error
 * @abe: Pointer on abe handle
 * @level: level of error
 * @error: error ID to log
 *
 * Log the ABE errors.
 */
void omap_aess_dbg_error(struct omap_aess *abe, int level, int error)
{
	omap_aess_dbg_log(abe, error, MAX_UINT8, MAX_UINT8, MAX_UINT8);
}
