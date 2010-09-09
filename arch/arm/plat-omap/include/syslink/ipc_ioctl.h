/*
 *  ipc_ioctl.h
 *
 *  Base file for all TI OMAP IPC ioctl's.
 *  Linux-OMAP IPC has allocated base 0xEE with a range of 0x00-0xFF.
 *  (need to get  the real one from open source maintainers)
 *
 *  Copyright (C) 2008-2009 Texas Instruments, Inc.
 *
 *  This package is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 *  IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 *  WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 *  PURPOSE.
 */

#ifndef _IPC_IOCTL_H
#define _IPC_IOCTL_H

#include <linux/ioctl.h>
#include <linux/types.h>
#include <linux/fs.h>

#define IPC_IOC_MAGIC		0xE0
#define IPC_IOC_BASE		2

enum ipc_command_count {
	MULTIPROC_CMD_NOS = 4,
	NAMESERVER_CMD_NOS = 15,
	HEAPBUFMP_CMD_NOS = 14,
	SHAREDREGION_CMD_NOS = 13,
	GATEMP_CMD_NOS = 13,
	LISTMP_CMD_NOS = 19,
	MESSAGEQ_CMD_NOS = 18,
	IPC_CMD_NOS = 5,
	SYSMEMMGR_CMD_NOS = 6,
	HEAPMEMMP_CMD_NOS = 15,
	NOTIFY_CMD_NOS = 18
};

enum ipc_command_ranges {
	MULTIPROC_BASE_CMD = IPC_IOC_BASE,
	MULTIPROC_END_CMD = (MULTIPROC_BASE_CMD + \
				MULTIPROC_CMD_NOS - 1),

	NAMESERVER_BASE_CMD = 10,
	NAMESERVER_END_CMD = (NAMESERVER_BASE_CMD + \
				NAMESERVER_CMD_NOS - 1),

	HEAPBUFMP_BASE_CMD = 30,
	HEAPBUFMP_END_CMD = (HEAPBUFMP_BASE_CMD + \
				HEAPBUFMP_CMD_NOS - 1),

	SHAREDREGION_BASE_CMD = 50,
	SHAREDREGION_END_CMD = (SHAREDREGION_BASE_CMD + \
				SHAREDREGION_CMD_NOS - 1),

	GATEMP_BASE_CMD = 70,
	GATEMP_END_CMD = (GATEMP_BASE_CMD + \
				GATEMP_CMD_NOS - 1),

	LISTMP_BASE_CMD = 90,
	LISTMP_END_CMD = (LISTMP_BASE_CMD + \
				LISTMP_CMD_NOS - 1),

	MESSAGEQ_BASE_CMD = 110,
	MESSAGEQ_END_CMD = (MESSAGEQ_BASE_CMD + \
				MESSAGEQ_CMD_NOS - 1),

	IPC_BASE_CMD = 130,
	IPC_END_CMD = (IPC_BASE_CMD + \
				IPC_CMD_NOS - 1),

	SYSMEMMGR_BASE_CMD = 140,
	SYSMEMMGR_END_CMD = (SYSMEMMGR_BASE_CMD + \
				SYSMEMMGR_CMD_NOS - 1),

	HEAPMEMMP_BASE_CMD = 150,
	HEAPMEMMP_END_CMD = (HEAPMEMMP_BASE_CMD + \
				HEAPMEMMP_CMD_NOS - 1),

	NOTIFY_BASE_CMD = 170,
	NOTIFY_END_CMD = (NOTIFY_BASE_CMD + \
				NOTIFY_CMD_NOS - 1)
};

struct resource_info {
	struct list_head res; /* Pointer to res entry */
	unsigned int cmd; /* command */
	void *data; /* Some private data */
};

/* Process Context */
struct ipc_process_context {
	/* List of Resources */
	struct list_head resources;
	spinlock_t res_lock;

	struct ipc_device *dev;
};

void add_pr_res(struct ipc_process_context *pr_ctxt, unsigned int cmd,
		void *data);

void remove_pr_res(struct ipc_process_context *pr_ctxt,
			struct resource_info *info);

int ipc_ioc_router(u32 cmd, ulong arg, struct file *filp, bool user);

#endif /* _IPC_IOCTL_H */
