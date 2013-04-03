/*
 * Remote processor framework
 *
 * Copyright (C) 2011 Texas Instruments, Inc.
 * Copyright (C) 2011 Google, Inc.
 *
 * Ohad Ben-Cohen <ohad@wizery.com>
 * Brian Swetland <swetland@google.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef REMOTEPROC_INTERNAL_H
#define REMOTEPROC_INTERNAL_H

#include <linux/irqreturn.h>

struct rproc;

/* memory type for allocation requests */
enum {
	RPROC_MEM_FW,
	RPROC_MEM_IPC,
	RPROC_MEM_IPC_BUF,
};

/* from remoteproc_core.c */
void rproc_release(struct kref *kref);
irqreturn_t rproc_vq_interrupt(struct rproc *rproc, int vq_id);
void rproc_recover(struct rproc *rproc);
void *rproc_alloc_memory(struct rproc *rproc, u32 size, dma_addr_t *dma,
				int type);
void rproc_free_memory(struct rproc *rproc, u32 size, void *va, dma_addr_t dma);

/* from remoteproc_virtio.c */
int rproc_add_virtio_dev(struct rproc_vdev *rvdev, int id);
void rproc_remove_virtio_dev(struct rproc_vdev *rvdev);

/* from remoteproc_secure.c */
void rproc_secure_init(struct rproc *rproc);
void rproc_secure_reset(struct rproc *rproc);
int rproc_secure_parse_fw(struct rproc *rproc, const u8 *elf_data);
int rproc_secure_boot(struct rproc *rproc);
int rproc_secure_get_mode(struct rproc *rproc);
int rproc_secure_get_ttb(struct rproc *rproc);
bool rproc_is_secure(struct rproc *rproc);

/* from remoteproc_debugfs.c */
void rproc_remove_trace_file(struct dentry *tfile);
struct dentry *rproc_create_trace_file(const char *name, struct rproc *rproc,
					struct rproc_mem_entry *trace);
void rproc_delete_debug_dir(struct rproc *rproc);
void rproc_create_debug_dir(struct rproc *rproc);
void rproc_init_debugfs(void);
void rproc_exit_debugfs(void);

void rproc_free_vring(struct rproc_vring *rvring);
int rproc_alloc_vring(struct rproc_vdev *rvdev, int i);
#endif /* REMOTEPROC_INTERNAL_H */
