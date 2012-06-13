/*
 * Remote processor omap resources
 *
 * Copyright(c) 2012 Texas Instruments. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 * * Neither the name Texas Instruments nor the names of its
 *   contributors may be used to endorse or promote products derived
 *   from this software without specific prior written permission.
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
 */

#ifndef _LINUX_OMAP_RPMSG_RESMGR_H
#define _LINUX_OMAP_RPMSG_RESMGR_H

#define MAX_NUM_SDMA_CHANNELS   16

/**
 * struct rprm_gpt - resource manager parameters for GPTimer
 * @id:		id of the requested GPTimer
 */
struct rprm_gpt {
	u32 id;
	u32 src_clk;
} __packed;

/**
 * struct rprm_sdma - resource manager parameters for SDMA channels
 * @num_chs:	number of channels requested
 * @channels:	Array which contain the ids of the channel that were allocated
 */
struct rprm_sdma {
	u32 num_chs;
	s32 channels[MAX_NUM_SDMA_CHANNELS];
} __packed;

/**
 * struct rprm_auxclk - resource manager parameters for auxiliary clock
 * @cl_id:	id of the auxclk
 * @clk_rate:	rate in Hz for the auxclk
 * @pclk_id:	id of the auxclk source's parent
 * @pclk_rate:	rate in Hz for the auxclk source's parent
 */
struct rprm_auxclk {
	u32 clk_id;
	unsigned clk_rate;
	u32 pclk_id;
	unsigned pclk_rate;
} __packed;

/**
 * struct rprm_regulator - resource manager parameters for regulator
 * @reg_id:	regulator id
 * @min_uv	minimum voltage in micro volts
 * @max_uv	maximum voltage in micro volts
 */
struct rprm_regulator {
	u32 reg_id;
	u32 min_uv;
	u32 max_uv;
} __packed;

#endif /* _LINUX_RPMSG_RESMGR_COMMON_H */
