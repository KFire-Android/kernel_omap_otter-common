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
#include "abe_aess.h"
#include "abe_gain.h"
#include "abe_mem.h"
#include "abe_port.h"
#include "abe_seq.h"

const u32 aess_firmware_array[ABE_FIRMWARE_MAX_SIZE] = {
#include "abe_firmware.c"
};

/* FW version that this HAL supports.
 * We cheat and since we include the FW in the driver atm we can get the
 * FW version based on aess_firmware_array[5]. This should be updated when the
 * FW is removed from HAL code for modular builds.
 */
#define OMAP_ABE_SUPPORTED_FW_VERSION aess_firmware_array[5]

/**
 * omap_aess_reset_port
 * @id: ABE port ID
 *
 * stop the port activity and reload default parameters on the associated
 * processing features.
 * Clears the internal AE buffers.
 */
int omap_aess_reset_port(u32 id)
{
	abe_port[id] = ((struct omap_aess_port *) abe_port_init)[id];
	return 0;
}

/**
 * abe_reset_all_ports
 *
 * load default configuration for all features
 */
void omap_aess_reset_all_ports(struct omap_aess *abe)
{
	u16 i;
	for (i = 0; i < LAST_PORT_ID; i++)
		omap_aess_reset_port(i);
	/* mixers' configuration */
	omap_aess_write_mixer(abe, OMAP_AESS_MIXDL1_MM_DL, MUTE_GAIN);
	omap_aess_write_mixer(abe, OMAP_AESS_MIXDL1_MM_UL2, MUTE_GAIN);
	omap_aess_write_mixer(abe, OMAP_AESS_MIXDL1_VX_DL, MUTE_GAIN);
	omap_aess_write_mixer(abe, OMAP_AESS_MIXDL1_TONES, MUTE_GAIN);
	omap_aess_write_mixer(abe, OMAP_AESS_MIXDL2_TONES, MUTE_GAIN);
	omap_aess_write_mixer(abe, OMAP_AESS_MIXDL2_VX_DL, MUTE_GAIN);
	omap_aess_write_mixer(abe, OMAP_AESS_MIXDL2_MM_DL, MUTE_GAIN);
	omap_aess_write_mixer(abe, OMAP_AESS_MIXDL2_MM_UL2, MUTE_GAIN);
	omap_aess_write_mixer(abe, OMAP_AESS_MIXSDT_UL, MUTE_GAIN);
	omap_aess_write_mixer(abe, OMAP_AESS_MIXSDT_DL, GAIN_0dB);
	omap_aess_write_mixer(abe, OMAP_AESS_MIXECHO_DL1, MUTE_GAIN);
	omap_aess_write_mixer(abe, OMAP_AESS_MIXECHO_DL2, MUTE_GAIN);
	omap_aess_write_mixer(abe, OMAP_AESS_MIXAUDUL_MM_DL, MUTE_GAIN);
	omap_aess_write_mixer(abe, OMAP_AESS_MIXAUDUL_TONES, MUTE_GAIN);
	omap_aess_write_mixer(abe, OMAP_AESS_MIXAUDUL_UPLINK, GAIN_0dB);
	omap_aess_write_mixer(abe, OMAP_AESS_MIXAUDUL_VX_DL, MUTE_GAIN);
	omap_aess_write_mixer(abe, OMAP_AESS_MIXVXREC_TONES, MUTE_GAIN);
	omap_aess_write_mixer(abe, OMAP_AESS_MIXVXREC_VX_DL, MUTE_GAIN);
	omap_aess_write_mixer(abe, OMAP_AESS_MIXVXREC_MM_DL, MUTE_GAIN);
	omap_aess_write_mixer(abe, OMAP_AESS_MIXVXREC_VX_UL, MUTE_GAIN);
	omap_aess_write_gain(abe, OMAP_AESS_GAIN_DMIC1_LEFT, GAIN_0dB);
	omap_aess_write_gain(abe, OMAP_AESS_GAIN_DMIC1_RIGHT, GAIN_0dB);
	omap_aess_write_gain(abe, OMAP_AESS_GAIN_DMIC2_LEFT, GAIN_0dB);
	omap_aess_write_gain(abe, OMAP_AESS_GAIN_DMIC2_RIGHT, GAIN_0dB);
	omap_aess_write_gain(abe, OMAP_AESS_GAIN_DMIC3_LEFT, GAIN_0dB);
	omap_aess_write_gain(abe, OMAP_AESS_GAIN_DMIC3_RIGHT, GAIN_0dB);
	omap_aess_write_gain(abe, OMAP_AESS_GAIN_AMIC_LEFT, GAIN_0dB);
	omap_aess_write_gain(abe, OMAP_AESS_GAIN_AMIC_RIGHT, GAIN_0dB);
	omap_aess_write_gain(abe, OMAP_AESS_GAIN_SPLIT_LEFT, GAIN_0dB);
	omap_aess_write_gain(abe, OMAP_AESS_GAIN_SPLIT_RIGHT, GAIN_0dB);
	omap_aess_write_gain(abe, OMAP_AESS_GAIN_DL1_LEFT, GAIN_0dB);
	omap_aess_write_gain(abe, OMAP_AESS_GAIN_DL1_RIGHT, GAIN_0dB);
	omap_aess_write_gain(abe, OMAP_AESS_GAIN_DL2_LEFT, GAIN_0dB);
	omap_aess_write_gain(abe, OMAP_AESS_GAIN_DL2_RIGHT, GAIN_0dB);
	omap_aess_write_gain(abe, OMAP_AESS_GAIN_BTUL_LEFT, GAIN_0dB);
	omap_aess_write_gain(abe, OMAP_AESS_GAIN_BTUL_RIGHT, GAIN_0dB);
}

/*
 * initialize the default values for call-backs to subroutines
 * - FIFO IRQ call-backs for sequenced tasks
 * - FIFO IRQ call-backs for audio player/recorders (ping-pong protocols)
 * - Remote debugger interface
 * - Error monitoring
 * - Activity Tracing
 */

/**
 * abe_init_mem - Allocate Kernel space memory map for ABE
 *
 * Memory map of ABE memory space for PMEM/DMEM/SMEM/DMEM
 */
int omap_aess_init_mem(struct omap_aess *abe, void __iomem **_io_base)
{
	int i;

	for (i = 0; i < 5; i++)
		abe->io_base[i] = _io_base[i];

	mutex_init(&abe->mutex);
	return 0;

}
EXPORT_SYMBOL(omap_aess_init_mem);


int omap_aess_load_fw_param(struct omap_aess *abe, u32 *data)
{
	u32 pmem_size, dmem_size, smem_size, cmem_size;
	u32 *pmem_ptr, *dmem_ptr, *smem_ptr, *cmem_ptr, *fw_ptr;

	fw_ptr = data;
	abe->firmware_version_number = *fw_ptr++;
	pmem_size = *fw_ptr++;
	cmem_size = *fw_ptr++;
	dmem_size = *fw_ptr++;
	smem_size = *fw_ptr++;
	pmem_ptr = fw_ptr;
	cmem_ptr = pmem_ptr + (pmem_size >> 2);
	dmem_ptr = cmem_ptr + (cmem_size >> 2);
	smem_ptr = dmem_ptr + (dmem_size >> 2);
	/* do not load PMEM */
	if (abe->warm_boot) {
		/* Stop the event Generator */
		omap_aess_stop_event_generator(abe);

		/* Now we are sure the firmware is stalled */
		omap_abe_mem_write(abe, OMAP_ABE_CMEM, 0, cmem_ptr,
			       cmem_size);
		omap_abe_mem_write(abe, OMAP_ABE_SMEM, 0, smem_ptr,
			       smem_size);
		omap_abe_mem_write(abe, OMAP_ABE_DMEM, 0, dmem_ptr,
			       dmem_size);

		/* Restore the event Generator status */
		omap_aess_start_event_generator(abe);
	} else {
		omap_abe_mem_write(abe, OMAP_ABE_PMEM, 0, pmem_ptr,
			       pmem_size);
		omap_abe_mem_write(abe, OMAP_ABE_CMEM, 0, cmem_ptr,
			       cmem_size);
		omap_abe_mem_write(abe, OMAP_ABE_SMEM, 0, smem_ptr,
			       smem_size);
		omap_abe_mem_write(abe, OMAP_ABE_DMEM, 0, dmem_ptr,
			       dmem_size);
	}
	abe->warm_boot = 1;
	return 0;
}
EXPORT_SYMBOL(omap_aess_load_fw_param);

/**
 * omap_aess_load_fw - Load ABE Firmware and initialize memories
 * @abe: Pointer on abe handle
 *
 */
int omap_aess_load_fw(struct omap_aess *abe, u32 *firmware)
{
	omap_aess_load_fw_param(abe, firmware);
	omap_aess_reset_all_ports(abe);
	omap_aess_build_scheduler_table(abe);
	omap_aess_reset_all_sequence(abe);
	omap_aess_select_main_port(abe, OMAP_ABE_PDM_DL_PORT);
	return 0;
}
EXPORT_SYMBOL(omap_aess_load_fw);

/**
 * abe_reload_fw - Reload ABE Firmware after OFF mode
 */
int omap_aess_reload_fw(struct omap_aess *abe, u32 *firmware)
{
	abe->warm_boot = 0;
	omap_aess_load_fw(abe, firmware);
	omap_aess_build_scheduler_table(abe);
	omap_aess_dbg_reset(abe->dbg);
	/* IRQ circular read pointer in DMEM */
	abe->irq_dbg_read_ptr = 0;
	/* Restore Gains not managed by the drivers */
	/*SEBG: RAMP 2ms */
	omap_aess_write_gain(abe, OMAP_AESS_GAIN_SPLIT_LEFT, GAIN_0dB);
	omap_aess_write_gain(abe, OMAP_AESS_GAIN_SPLIT_RIGHT, GAIN_0dB);

	return 0;
}
EXPORT_SYMBOL(omap_aess_reload_fw);

/**
 * omap_abe_get_default_fw
 *
 * Get default ABE firmware
 */
u32 *omap_aess_get_default_fw()
{
	return (u32 *)aess_firmware_array;
}
EXPORT_SYMBOL(omap_aess_get_default_fw);

/*
 * omap_abe_get_supported_fw_version - return supported FW version number.
 */
u32 omap_abe_get_supported_fw_version(void)
{
       return OMAP_ABE_SUPPORTED_FW_VERSION;
}
EXPORT_SYMBOL(omap_abe_get_supported_fw_version);


