/*
 * Remote Processor Framework
 *
 * Copyright(c) 2012 Texas Instruments, Inc.
 * All rights reserved.
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

#ifndef RPROC_DRM_H
#define RPROC_DRM_H

#define COMMON_SECURE_DRIVER_UUID {0xC2537CC3, 0x36F0, 0x48D9, \
			{0x82, 0x0E, 0x55, 0x96, 0x01, 0x47, 0x80, 0x29} }

/*
 * common definition shared between rproc and secure services
 * and the ducati secure signing tools
 */
struct rproc_iommu_ttbr_update {
	uint8_t  count;
	uint8_t  region;
	uint16_t index;
	uint32_t descriptor;
};

/*
 * common definition shared between ducati Ipc Memory Manager, rproc and
 * secure services and the ducati secure signing tools
 */
enum rproc_memregion_enum {
	RPROC_MEMREGION_UNKNOWN,
	RPROC_MEMREGION_2D,
	RPROC_MEMREGION_SMEM,
	RPROC_MEMREGION_CODE,
	RPROC_MEMREGION_DATA,
	RPROC_MEMREGION_1D,
	RPROC_MEMREGION_VRING
};

/*
 * common definition for shared params shared between rproc and secure
 * services and the ducati secure signing tools
 */
struct rproc_sec_params {
	/* remote proc shared memory */
	dma_addr_t ducati_smem;
	uint32_t ducati_smem_size;

	/* remote core code memory */
	dma_addr_t ducati_code;
	uint32_t ducati_code_size;

	/* remote core data memory */
	dma_addr_t ducati_data;
	uint32_t ducati_data_size;

	/* remote core vring (io buffers) memory */
	dma_addr_t ducati_vring_smem;
	uint32_t   ducati_vring_size;

	/* remote core secure input memory */
	dma_addr_t secure_buffer_address;
	uint32_t secure_buffer_size;

	/* remote core secure output memory */
	dma_addr_t decoded_buffer_address;
	uint32_t decoded_buffer_size;

	/* ducati base address for all memory TOC entries */
	dma_addr_t ducati_base_address;

	/* ducati TOC location */
	dma_addr_t ducati_toc_address;

	/* Address of ducati translation table */
	dma_addr_t ducati_page_table_address;
};

/*
 * Defined steps for entring secure playback
 * A0: reset all firewalls
 * A1: validate only the boot code
 * A2: validate all code and enable firewalls
 * ENTER_SECURE_PLAYBACK: enable all firewalls to enter secure playback
 * EXIT_SECURE_PLAYBACK: leave secure playback
 * ENTER_SECURE_PLAYBACK_AFTER_AUTHENTICATION: enter secure playback
 * after validation
 */
enum rproc_service_enum {
	AUTHENTICATION_A0 = 0x00002000,
	AUTHENTICATION_A1,
	AUTHENTICATION_A2,
	ENTER_SECURE_PLAYBACK = 0x00003000,
	EXIT_SECURE_PLAYBACK,
	ENTER_SECURE_PLAYBACK_AFTER_AUTHENTICATION
};

typedef int (*rproc_drm_invoke_service_t)(enum rproc_service_enum service,
					  struct rproc_sec_params
					  *rproc_sec_params);
int rproc_register_drm_service(rproc_drm_invoke_service_t drm_service);
int rproc_unregister_drm_service(rproc_drm_invoke_service_t drm_service);

#endif /* RPROC_DRM_H */
