/*
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2010-2012 Texas Instruments Incorporated,
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 * The full GNU General Public License is included in this distribution
 * in the file called LICENSE.GPL.
 *
 * BSD LICENSE
 *
 * Copyright(c) 2010-2012 Texas Instruments Incorporated,
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Texas Instruments Incorporated nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
#define DEBUG

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/device.h>

#include "abe.h"
#include "abe_aess.h"
#include "abe_gain.h"
#include "abe_mem.h"
#include "abe_port.h"
#include "abe_seq.h"

/* FW version that this HAL supports.
 * We cheat and since we include the FW in the driver atm we can get the
 * FW version based on aess_firmware_array[5]. This should be updated when the
 * FW is removed from HAL code for modular builds.
 */
#define OMAP_ABE_SUPPORTED_FW_VERSION 0x09590

/**
 * abe_reset_all_ports
 * @abe: Pointer on aess handle
 *
 * load default configuration for all features
 */
static void omap_aess_reset_all_ports(struct omap_aess *abe)
{
	u16 i;

	for (i = 0; i < LAST_PORT_ID; i++)
		omap_aess_reset_port(abe, i);

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

/**
 * omap_aess_init_gain_ramp
 * @abe: Pointer on aess handle
 *
 * load default gain ramp configuration.
 */
static void omap_aess_init_gain_ramp(struct omap_aess *abe)
{
	/* Ramps configuration */
	omap_aess_write_gain_ramp(abe, OMAP_AESS_MIXDL1_MM_DL, RAMP_2MS);
	omap_aess_write_gain_ramp(abe, OMAP_AESS_MIXDL1_MM_UL2, RAMP_2MS);
	omap_aess_write_gain_ramp(abe, OMAP_AESS_MIXDL1_VX_DL, RAMP_2MS);
	omap_aess_write_gain_ramp(abe, OMAP_AESS_MIXDL1_TONES, RAMP_2MS);
	omap_aess_write_gain_ramp(abe, OMAP_AESS_MIXDL2_TONES, RAMP_2MS);
	omap_aess_write_gain_ramp(abe, OMAP_AESS_MIXDL2_VX_DL, RAMP_2MS);
	omap_aess_write_gain_ramp(abe, OMAP_AESS_MIXDL2_MM_DL, RAMP_2MS);
	omap_aess_write_gain_ramp(abe, OMAP_AESS_MIXDL2_MM_UL2, RAMP_2MS);
	omap_aess_write_gain_ramp(abe, OMAP_AESS_MIXSDT_UL, RAMP_2MS);
	omap_aess_write_gain_ramp(abe, OMAP_AESS_MIXSDT_DL, RAMP_2MS);
	omap_aess_write_gain_ramp(abe, OMAP_AESS_MIXECHO_DL1, RAMP_2MS);
	omap_aess_write_gain_ramp(abe, OMAP_AESS_MIXECHO_DL2, RAMP_2MS);
	omap_aess_write_gain_ramp(abe, OMAP_AESS_MIXAUDUL_MM_DL, RAMP_2MS);
	omap_aess_write_gain_ramp(abe, OMAP_AESS_MIXAUDUL_TONES, RAMP_2MS);
	omap_aess_write_gain_ramp(abe, OMAP_AESS_MIXAUDUL_UPLINK, RAMP_2MS);
	omap_aess_write_gain_ramp(abe, OMAP_AESS_MIXAUDUL_VX_DL, RAMP_2MS);
	omap_aess_write_gain_ramp(abe, OMAP_AESS_MIXVXREC_TONES, RAMP_2MS);
	omap_aess_write_gain_ramp(abe, OMAP_AESS_MIXVXREC_VX_DL, RAMP_2MS);
	omap_aess_write_gain_ramp(abe, OMAP_AESS_MIXVXREC_MM_DL, RAMP_2MS);
	omap_aess_write_gain_ramp(abe, OMAP_AESS_MIXVXREC_VX_UL, RAMP_2MS);
	omap_aess_write_gain_ramp(abe, OMAP_AESS_GAIN_DMIC1_LEFT, RAMP_2MS);
	omap_aess_write_gain_ramp(abe, OMAP_AESS_GAIN_DMIC1_RIGHT, RAMP_2MS);
	omap_aess_write_gain_ramp(abe, OMAP_AESS_GAIN_DMIC2_LEFT, RAMP_2MS);
	omap_aess_write_gain_ramp(abe, OMAP_AESS_GAIN_DMIC2_RIGHT, RAMP_2MS);
	omap_aess_write_gain_ramp(abe, OMAP_AESS_GAIN_DMIC3_LEFT, RAMP_2MS);
	omap_aess_write_gain_ramp(abe, OMAP_AESS_GAIN_DMIC3_RIGHT, RAMP_2MS);
	omap_aess_write_gain_ramp(abe, OMAP_AESS_GAIN_AMIC_LEFT, RAMP_2MS);
	omap_aess_write_gain_ramp(abe, OMAP_AESS_GAIN_AMIC_RIGHT, RAMP_2MS);
	omap_aess_write_gain_ramp(abe, OMAP_AESS_GAIN_SPLIT_LEFT, RAMP_2MS);
	omap_aess_write_gain_ramp(abe, OMAP_AESS_GAIN_SPLIT_RIGHT, RAMP_2MS);
	omap_aess_write_gain_ramp(abe, OMAP_AESS_GAIN_DL1_LEFT, RAMP_2MS);
	omap_aess_write_gain_ramp(abe, OMAP_AESS_GAIN_DL1_RIGHT, RAMP_2MS);
	omap_aess_write_gain_ramp(abe, OMAP_AESS_GAIN_DL2_LEFT, RAMP_2MS);
	omap_aess_write_gain_ramp(abe, OMAP_AESS_GAIN_DL2_RIGHT, RAMP_2MS);
	omap_aess_write_gain_ramp(abe, OMAP_AESS_GAIN_BTUL_LEFT, RAMP_2MS);
	omap_aess_write_gain_ramp(abe, OMAP_AESS_GAIN_BTUL_RIGHT, RAMP_2MS);
}

/**
 * omap_aess_init_mem - Allocate Kernel space memory map for ABE
 * @abe: Pointer on abe handle
 * @dev: Pointer on device handle
 * @_io_base: Pointer on the different AESS memory banks (DT or HW data)
 * @fw_header: Pointer on the firmware header commit from the FS.
 *
 * Memory map of ABE memory space for PMEM/DMEM/SMEM/DMEM
 */
int omap_aess_init_mem(struct omap_aess *abe, struct device *dev,
	void __iomem **_io_base, u32 *fw_header)
{
	int i, offset = 0;
	u32 count;

	abe->dev = dev;

	for (i = 0; i < 5; i++)
		abe->io_base[i] = _io_base[i];

	dev_dbg(abe->dev, "DMEM bank at 0x%p\n", abe->io_base[OMAP_ABE_DMEM]);
	dev_dbg(abe->dev, "CMEM bank at 0x%p\n", abe->io_base[OMAP_ABE_CMEM]);
	dev_dbg(abe->dev, "SMEM bank at 0x%p\n", abe->io_base[OMAP_ABE_SMEM]);
	dev_dbg(abe->dev, "PMEM bank at 0x%p\n", abe->io_base[OMAP_ABE_PMEM]);
	dev_dbg(abe->dev, "AESS bank at 0x%p\n", abe->io_base[OMAP_ABE_AESS]);

	abe->fw_info = kzalloc(sizeof(struct omap_aess_mapping), GFP_KERNEL);
	if (abe->fw_info == NULL)
		return -ENOMEM;

	abe->fw_info->init_table = kzalloc(sizeof(struct omap_aess_init_task), GFP_KERNEL);
	if (abe->fw_info->init_table == NULL) {
		kfree(abe->fw_info);
		return -ENOMEM;
	}

	/* get mapping */
	count = fw_header[offset];
	dev_dbg(abe->dev, "Map %d items of size 0x%x at offset 0x%x\n", count,
		sizeof(struct omap_aess_addr), offset << 2);
	abe->fw_info->map = (struct omap_aess_addr *)&fw_header[++offset];
	offset += (sizeof(struct omap_aess_addr) * count) / 4;

	/* get label IDs */
	count = fw_header[offset];
	dev_dbg(abe->dev, "Labels %d at offset 0x%x\n", count, offset << 2);
	abe->fw_info->label_id = &fw_header[++offset];
	offset += count;

	/* get function IDs */
	count = fw_header[offset];
	dev_dbg(abe->dev, "Functions %d at offset 0x%x\n", count,
		offset << 2);
	abe->fw_info->fct_id = &fw_header[++offset];
	offset += count;

	/* get tasks */
	count = fw_header[offset];
	dev_dbg(abe->dev, "Tasks %d of size 0x%x at offset 0x%x\n", count,
		sizeof(struct omap_aess_task), offset << 2);
	abe->fw_info->init_table->nb_task = count;
	abe->fw_info->init_table->task = (struct omap_aess_task *)&fw_header[++offset];
	offset += (sizeof(struct omap_aess_task) * count) / 4;

	/* get ports */
	count = fw_header[offset];
	dev_dbg(abe->dev, "Ports %d of size 0x%x at offset 0x%x\n", count,
		sizeof(struct omap_aess_port), offset << 2);
	abe->fw_info->port = (struct omap_aess_port *)&fw_header[++offset];
	offset += (sizeof(struct omap_aess_port) * count) / 4;

	/* get ping pong port */
	dev_dbg(abe->dev, "Ping pong port at offset 0x%x\n", offset << 2);
	abe->fw_info->ping_pong = (struct omap_aess_port *)&fw_header[offset];
	offset += sizeof(struct omap_aess_port) / 4;

	/* get DL1 mono mixer */
	dev_dbg(abe->dev, "DL1 mono mixer at offset 0x%x\n", offset << 2);
	abe->fw_info->dl1_mono_mixer = (struct omap_aess_task *)&fw_header[offset];
	offset += (sizeof(struct omap_aess_task) / 4) * 2;

	/* get DL2 mono mixer */
	dev_dbg(abe->dev, "DL2 mono mixer at offset 0x%x\n", offset << 2);
	abe->fw_info->dl2_mono_mixer = (struct omap_aess_task *)&fw_header[offset];
	offset += (sizeof(struct omap_aess_task) / 4) * 2;

	/* get AUDUL mono mixer */
	dev_dbg(abe->dev, "AUDUL mixer at offset 0x%x\n", offset << 2);
	abe->fw_info->audul_mono_mixer = (struct omap_aess_task *)&fw_header[offset];
	offset += (sizeof(struct omap_aess_task) / 4) * 2;

	/* ASRC */
	dev_dbg(abe->dev, "ASRC at offset 0x%x\n", offset << 2);
	abe->fw_info->asrc = &fw_header[offset];

	mutex_init(&abe->mutex);
	return 0;

}
EXPORT_SYMBOL(omap_aess_init_mem);

/**
 * omap_aess_load_fw_param - Load the ABE FW inside AESS memories
 * @abe: Pointer on abe handle
 * @data: Pointer on the ABE firmware (after the header)
 *
 * Load the different AESS memories PMEM/DMEM/SMEM/DMEM
 */
static int omap_aess_load_fw_param(struct omap_aess *abe, u32 *data)
{
	u32 pmem_size, dmem_size, smem_size, cmem_size;
	u32 *pmem_ptr, *dmem_ptr, *smem_ptr, *cmem_ptr, *fw_ptr;

	/* Analyze FW memories banks sizes */
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

	omap_abe_mem_write(abe, OMAP_ABE_PMEM, 0, pmem_ptr, pmem_size);
	omap_abe_mem_write(abe, OMAP_ABE_CMEM, 0, cmem_ptr, cmem_size);
	omap_abe_mem_write(abe, OMAP_ABE_SMEM, 0, smem_ptr, smem_size);
	omap_abe_mem_write(abe, OMAP_ABE_DMEM, 0, dmem_ptr, dmem_size);

	return 0;
}
EXPORT_SYMBOL(omap_aess_load_fw_param);

/**
 * omap_aess_load_fw - Load ABE Firmware and initialize memories
 * @abe: Pointer on aess handle
 * @firmware: Pointer on the ABE firmware (after the header)
 *
 */
int omap_aess_load_fw(struct omap_aess *abe, u32 *firmware)
{
	omap_aess_load_fw_param(abe, firmware);
	omap_aess_reset_all_ports(abe);
	omap_aess_init_gain_ramp(abe);
	omap_aess_build_scheduler_table(abe);
	omap_aess_reset_all_sequence(abe);
	omap_aess_select_main_port(abe, OMAP_ABE_PDM_DL_PORT);
	return 0;
}
EXPORT_SYMBOL(omap_aess_load_fw);

/**
 * abe_reload_fw - Reload ABE Firmware after OFF mode
 * @abe: Pointer on aess handle
 * @firmware: Pointer on the ABE firmware (after the header)
 */
int omap_aess_reload_fw(struct omap_aess *abe, u32 *firmware)
{
	omap_aess_load_fw_param(abe, firmware);
	omap_aess_init_gain_ramp(abe);
	omap_aess_build_scheduler_table(abe);
	/* IRQ circular read pointer in DMEM */
	abe->irq_dbg_read_ptr = 0;
	/* Restore Gains not managed by the drivers */
	omap_aess_write_gain(abe, OMAP_AESS_GAIN_SPLIT_LEFT, GAIN_0dB);
	omap_aess_write_gain(abe, OMAP_AESS_GAIN_SPLIT_RIGHT, GAIN_0dB);

	return 0;
}
EXPORT_SYMBOL(omap_aess_reload_fw);

/*
 * omap_abe_get_supported_fw_version - return supported FW version number.
 */
u32 omap_abe_get_supported_fw_version(void)
{
	return OMAP_ABE_SUPPORTED_FW_VERSION;
}
EXPORT_SYMBOL(omap_abe_get_supported_fw_version);
