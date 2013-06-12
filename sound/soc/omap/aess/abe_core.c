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
 * Copyright(c) 2010-2013 Texas Instruments Incorporated,
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

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/slab.h>

#include "abe.h"
#include "abe_gain.h"
#include "abe_aess.h"
#include "abe_port.h"
#include "abe_mem.h"
#include "abe_dbg.h"

/**
 * omap_aess_reset_hal - reset the ABE/HAL
 * @aess: Pointer on aess handle
 *
 * Operations : reset the ABE by reloading the static variables and
 * default AESS registers.
 * Called after a PRCM cold-start reset of ABE
 */
int omap_aess_reset_hal(struct omap_aess *aess)
{
	u32 i;

	/* IRQ & DBG circular read pointer in DMEM */
	aess->irq_dbg_read_ptr = 0;

	/* reset the default gain values */
	for (i = 0; i < MAX_NBGAIN_CMEM; i++) {
		aess->muted_gains_indicator[i] = 0;
		aess->desired_gains_decibel[i] = (u32) GAIN_MUTE;
		aess->desired_gains_linear[i] = 0;
		aess->desired_ramp_delay_ms[i] = 0;
		aess->muted_gains_decibel[i] = (u32) GAIN_TOOLOW;
	}
	omap_aess_hw_configuration(aess);
	return 0;
}
EXPORT_SYMBOL(omap_aess_reset_hal);

/**
 * omap_aess_wakeup - Wakeup ABE
 * @aess: Pointer on aess handle
 *
 * Wakeup ABE in case of retention
 */
int omap_aess_wakeup(struct omap_aess *aess)
{
	/* Restart event generator */
	omap_aess_write_event_generator(aess, EVENT_TIMER);

	/* reconfigure DMA Req and MCU Irq visibility */
	omap_aess_hw_configuration(aess);
	return 0;
}
EXPORT_SYMBOL(omap_aess_wakeup);

/**
 * abe_set_router_configuration
 * @aess: Pointer on aess handle
 * @param: list of output index of the route
 *
 * The uplink router takes its input from DMIC (6 samples), AMIC (2 samples)
 * and PORT1/2 (2 stereo ports). Each sample will be individually stored in
 * an intermediate table of 10 elements.
 *
 * Example of router table parameter for voice uplink with phoenix microphones
 *
 * indexes 0 .. 9 = MM_UL description (digital MICs and MMEXTIN)
 *	DMIC1_L_labelID, DMIC1_R_labelID, DMIC2_L_labelID, DMIC2_R_labelID,
 *	MM_EXT_IN_L_labelID, MM_EXT_IN_R_labelID, ZERO_labelID, ZERO_labelID,
 *	ZERO_labelID, ZERO_labelID,
 * indexes 10 .. 11 = MM_UL2 description (recording on DMIC3)
 *	DMIC3_L_labelID, DMIC3_R_labelID,
 * indexes 12 .. 13 = VX_UL description (VXUL based on PDMUL data)
 *	AMIC_L_labelID, AMIC_R_labelID,
 * indexes 14 .. 15 = RESERVED (NULL)
 *	ZERO_labelID, ZERO_labelID,
 */
int omap_aess_set_router_configuration(struct omap_aess *aess, u32 *param)
{
	omap_aess_mem_write(aess,
			    aess->fw_info->map[OMAP_AESS_DMEM_AUPLINKROUTING_ID],
			    param);
	return 0;
}
EXPORT_SYMBOL(omap_aess_set_router_configuration);

/**
 * abe_set_opp_processing - Set OPP mode for ABE Firmware
 * @aess: Pointer on aess handle
 * @opp: OOPP mode
 *
 * New processing network and OPP:
 * 0: Ultra Lowest power consumption audio player (no post-processing, no mixer)
 * 1: OPP 25% (simple multimedia features, including low-power player)
 * 2: OPP 50% (multimedia and voice calls)
 * 3: OPP100% ( multimedia complex use-cases)
 *
 * Rearranges the FW task network to the corresponding OPP list of features.
 * The corresponding AE ports are supposed to be set/reset accordingly before
 * this switch.
 *
 */
int omap_aess_set_opp_processing(struct omap_aess *aess, u32 opp)
{
	u32 dOppMode32;

	switch (opp) {
	case ABE_OPP25:
		/* OPP25% */
		dOppMode32 = DOPPMODE32_OPP25;
		break;
	case ABE_OPP50:
		/* OPP50% */
		dOppMode32 = DOPPMODE32_OPP50;
		break;
	default:
		aess_warm("Bad OPP value requested");
	case ABE_OPP100:
		/* OPP100% */
		dOppMode32 = DOPPMODE32_OPP100;
		break;
	}
	/* Write Multiframe inside DMEM */
	omap_aess_mem_write(aess,
			    aess->fw_info->map[OMAP_AESS_DMEM_MAXTASKBYTESINSLOT_ID],
			    &dOppMode32);

	return 0;

}
EXPORT_SYMBOL(omap_aess_set_opp_processing);
