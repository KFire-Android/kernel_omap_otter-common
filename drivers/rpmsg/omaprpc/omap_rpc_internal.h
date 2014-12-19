/*
 * OMAP Remote Procedure Call Driver.
 *
 * Copyright(c) 2012 Texas Instruments. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
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
#error "OMAP RPC requires RPMSG"
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

#if defined(CONFIG_ION_OMAP) || defined(CONFIG_ION_OMAP_MODULE)
#define OMAPRPC_USE_ION
#undef	OMAPRPC_USE_DMABUF
#define OMAPRPC_USE_RPROC_LOOKUP	/* android supports this. */
#if defined(CONFIG_PVR_SGX) || defined(CONFIG_PVR_SGX_MODULE)
#define OMAPRPC_USE_PVR
#else
#undef	OMAPRPC_USE_PVR
#endif
#elif defined(CONFIG_DMA_SHARED_BUFFER) \
|| defined(CONFIG_DMA_SHARED_BUFFER_MODULE)
#undef	OMAPRPC_USE_ION
#define OMAPRPC_USE_DMABUF
/* genernic linux does not support it. */
#undef	OMAPRPC_USE_RPROC_LOOKUP
#else
#error "OMAP RPC requires either ION_OMAP or DMA_SHARED_BUFFER"
#endif

#define OMAPRPC_ZONE_INFO	(0x1)
#define OMAPRPC_ZONE_PERF	(0x2)
#define OMAPRPC_ZONE_VERBOSE	(0x4)

extern unsigned int omaprpc_debug;

#define OMAPRPC_PRINT(flag, dev, fmt, ...) { \
	if ((flag) & omaprpc_debug) \
		dev_info((dev), (fmt), ## __VA_ARGS__); \
}

#define OMAPRPC_ERR(dev, fmt, ...)	dev_err((dev), (fmt), ## __VA_ARGS__)

#ifdef CONFIG_PHYS_ADDR_T_64BIT
typedef u64 virt_addr_t;
#else
typedef u32 virt_addr_t;
#endif

enum omaprpc_service_state_e {
	OMAPRPC_SERVICE_STATE_DOWN,
	OMAPRPC_SERVICE_STATE_UP,
};

/**
 * struct omaprpc_service_t - The per-service (endpoint) data. Contains the
 * list of instances.
 */
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

/**
 * struct omaprpc_call_function_list_t - The list of all outstanding function
 * calls in this instance.
 */
struct omaprpc_call_function_list_t {
	struct list_head list;
	struct omaprpc_call_function_t *function;
	u16 msgId;
};

/**
 * struct omaprpc_instance_t - The per-instance data structure (per user).
 */
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
/**
 * struct dma_info_t - The DMA Info structure tracks the dma_buf relevant
 * variables.
 */
struct dma_info_t {
	struct list_head list;
	int fd;
	struct dma_buf *dbuf;
	struct dma_buf_attachment *attach;
	struct sg_table *sgt;
};
#endif

/*!
 * A wrapper function to translate local physical addresses to the remote core
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
				  virt_addr_t buva, void *reserved);

/*!
 * Used to recalculate the offset of a buffer and handles cases where Tiler
 * 2d regions are concerned.
 */
long omaprpc_recalc_off(phys_addr_t lpa, long uoff);

#endif
