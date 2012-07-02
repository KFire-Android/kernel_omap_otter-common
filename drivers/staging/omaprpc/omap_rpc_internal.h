/*
 * OMAP Remote Procedure Call Driver.
 *
 * Copyright(c) 2012 Texas Instruments. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 *	 notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 *	 notice, this list of conditions and the following disclaimer in
 *	 the documentation and/or other materials provided with the
 *	 distribution.
 * * Neither the name Texas Instruments nor the names of its
 *	 contributors may be used to endorse or promote products derived
 *	 from this software without specific prior written permission.
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

#ifndef _OMAP_RPC_INTERNAL_H_
#define _OMAP_RPC_INTERNAL_H_

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/idr.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/cdev.h>
#include <linux/jiffies.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/skbuff.h>
#include <linux/sched.h>
#include <linux/completion.h>
#include <linux/remoteproc.h>

#if defined(CONFIG_RPMSG) || defined(CONFIG_RPMSG_MODULE)
#include <linux/rpmsg.h>
#else
#error "OMAP RPC requireds RPMSG"
#endif

#if defined(CONFIG_RPC_OMAP) || defined(CONFIG_RPC_OMAP_MODULE)
#include <linux/omap_rpc.h>
#endif

#if defined(CONFIG_TI_TILER) || defined(CONFIG_TI_TILER_MODULE)
#include <mach/tiler.h>
#endif

#if defined(CONFIG_DMA_SHARED_BUFFER) \
|| defined(CONFIG_DMA_SHARED_BUFFER_MODULE)
#include <linux/dma-buf.h>
#endif

#if defined(CONFIG_ION_OMAP) || defined(CONFIG_ION_OMAP_MODULE)
#include <linux/omap_ion.h>
extern struct ion_device *omap_ion_device;
#if defined(CONFIG_PVR_SGX) || defined(CONFIG_PVR_SGX_MODULE)
#include "../../gpu/pvr/ion.h"
#endif
#endif

#if defined(CONFIG_TI_TILER) || defined(CONFIG_TI_TILER_MODULE)
#define OMAPRPC_USE_TILER
#else
#undef	OMAPRPC_USE_TILER
#endif

#if defined(CONFIG_DMA_SHARED_BUFFER) \
|| defined(CONFIG_DMA_SHARED_BUFFER_MODULE)
#define OMAPRPC_USE_DMABUF
#undef	OMAPRPC_USE_RPROC_LOOKUP /* genernic linux does not support yet. */
#else
#undef	OMAPRPC_USE_DMABUF
#endif

#if defined(CONFIG_ION_OMAP) || defined(CONFIG_ION_OMAP_MODULE)
#define OMAPRPC_USE_ION
#define OMAPRPC_USE_RPROC_LOOKUP /* android supports this. */
#if defined(CONFIG_PVR_SGX) || defined(CONFIG_PVR_SGX_MODULE)
#define OMAPRPC_USE_PVR
#else
#undef	OMAPRPC_USE_PVR
#endif
#else
#undef	OMAPRPC_USE_ION
#endif

/* Testing and debugging defines, leave undef'd in production */
#undef OMAPRPC_DEBUGGING
#undef OMAPRPC_VERY_VERBOSE
#undef OMAPRPC_PERF_MEASUREMENT

#if defined(OMAPRPC_DEBUGGING)
#define OMAPRPC_INFO(dev, fmt, ...)	dev_info(dev, fmt, ## __VA_ARGS__)
#define OMAPRPC_ERR(dev, fmt, ...)	dev_err(dev, fmt, ## __VA_ARGS__)
#else
#define OMAPRPC_INFO(dev, fmt, ...)
#define OMAPRPC_ERR(dev, fmt, ...)	dev_err(dev, fmt, ## __VA_ARGS__)
#endif

#ifdef CONFIG_PHYS_ADDR_T_64BIT
typedef u64 virt_addr_t;
#else
typedef u32 virt_addr_t;
#endif

enum omaprpc_service_state_e {
	OMAPRPC_SERVICE_STATE_DOWN,
	OMAPRPC_SERVICE_STATE_UP,
};

struct omaprpc_service_t {
	struct list_head list;
	struct cdev cdev;
	struct device *dev;
	struct rpmsg_channel *rpdev;
	int minor;
	struct list_head instance_list;
	struct mutex lock;
	struct completion comp;
	int state;
#if defined(OMAPRPC_USE_ION)
	struct ion_client *ion_client;
#endif
};

struct omaprpc_call_function_list_t {
	struct list_head list;
	struct omaprpc_call_function_t *function;
	u16 msgId;
};

struct omaprpc_instance_t {
	struct list_head list;
	struct omaprpc_service_t *rpcserv;
	struct sk_buff_head queue;
	struct mutex lock;
	wait_queue_head_t readq;
	struct completion reply_arrived;
	struct rpmsg_endpoint *ept;
	int transisioning;
	u32 dst;
	int state;
	u32 core;
#if defined(OMAPRPC_USE_ION)
	struct ion_client *ion_client;
#elif defined(OMAPRPC_USE_DMABUF)
	struct list_head dma_list;
#endif
	u16 msgId;
	struct list_head fxn_list;
};

#if defined(OMAPRPC_USE_DMABUF)
struct dma_info_t {
	struct list_head list;
	int fd;
	struct dma_buf *dbuf;
	struct dma_buf_attachment *attach;
	struct sg_table *sgt;
};
#endif

/*!
 * A Wrapper function to translate local physical addresses to the remote core
 * memory maps. Initialially we can only use an internal static table until
 * rproc support querying.
 */
#if defined(OMAPRPC_USE_RPROC_LOOKUP)
phys_addr_t rpmsg_local_to_remote_pa(struct omaprpc_instance_t *rpc,
	phys_addr_t pa);
#else
phys_addr_t rpmsg_local_to_remote_pa(uint32_t core, phys_addr_t pa);
#endif

/*!
 * This function translates all the pointers within the function call
 * structure and the translation structures.
 */
int omaprpc_xlate_buffers(struct omaprpc_instance_t *rpc,
			struct omaprpc_call_function_t *function,
			int direction);

/*!
 * Converts a buffer to a remote core address.
 */
phys_addr_t omaprpc_buffer_lookup(struct omaprpc_instance_t *rpc,
				uint32_t core,
				virt_addr_t uva,
				virt_addr_t buva,
				void *reserved);

/*!
 * Used to recalculate the offset of a buffer and handles cases where Tiler
 * 2d regions are concerned.
 */
long omaprpc_recalc_off(phys_addr_t lpa, long uoff);


#endif

