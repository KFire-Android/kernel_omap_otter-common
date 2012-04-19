/*
 * SGX display class driver platform resources
 *
 * Copyright (C) 2012 Texas Instruments
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef _ARCH_ARM_PLAT_OMAP_SGX_OMAPLFB_H
#define _ARCH_ARM_PLAT_OMAP_SGX_OMAPLFB_H

enum sgx_omaplfb_flags {
	/*
	 * This flag should be set when we do not want the primary display
	 * swap chain buffers to be used with an external display.
	 *
	 * The number of tiler2d and vram buffers need to be set appropriately
	 */
	SGX_OMAPLFB_FLAGS_SDMA_TO_TILER2D_EXTERNAL = 0x00000001,
};

/*
 * The settings this platform data entry will determine the type and number of
 * buffers to use by omaplfb.
 */
struct sgx_omaplfb_config {
	/*
	 * Number of tiler2d buffers required for display rendering,
	 * the number of buffers indicated by swap_chain_length will be used
	 * for the swap chain unless flags indicate otherwise
	 */
	u32 tiler2d_buffers;
	/*
	 * Number of vram buffers required for display rendering, if no tiler
	 * buffers are required or flags indicate then the number of buffers
	 * indicated by swap_chain_length will be used for the swap chain.
	 */
	u32 vram_buffers;
	/*
	 * Indicate any additional vram that needs to be reserved
	 */
	u32 vram_reserve;
	/*
	 * Tells omaplfb the number of buffers in the primary swapchain,
	 * if not set it defaults to 2.
	 */
	u32 swap_chain_length;
	enum sgx_omaplfb_flags flags;
};

struct sgx_omaplfb_platform_data {
	u32 num_configs;
	struct sgx_omaplfb_config *configs;
};

int sgx_omaplfb_set(unsigned int fbix, struct sgx_omaplfb_config *data);
struct sgx_omaplfb_config *sgx_omaplfb_get(unsigned int fbix);

#endif
