/*
 *  sharedregion_ioctl.h
 *
 *  The sharedregion module is designed to be used in a
 *  multi-processor environment where there are memory regions
 *  that are shared and accessed across different processors
 *
 *  Copyright (C) 2008-2010 Texas Instruments, Inc.
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

#ifndef _SHAREDREGION_IOCTL_H
#define _SHAREDREGION_IOCTL_H

#include <linux/ioctl.h>
#include <linux/types.h>

#include <ipc_ioctl.h>
#include <sharedregion.h>

enum CMD_SHAREDREGION {
	SHAREDREGION_GETCONFIG = SHAREDREGION_BASE_CMD,
	SHAREDREGION_SETUP,
	SHAREDREGION_DESTROY,
	SHAREDREGION_START,
	SHAREDREGION_STOP,
	SHAREDREGION_ATTACH,
	SHAREDREGION_DETACH,
	SHAREDREGION_GETHEAP,
	SHAREDREGION_CLEARENTRY,
	SHAREDREGION_SETENTRY,
	SHAREDREGION_RESERVEMEMORY,
	SHAREDREGION_CLEARRESERVEDMEMORY,
	SHAREDREGION_GETREGIONINFO
};

/*
 *  IOCTL command IDs for sharedregion
 *
 */

/*
 *  Command for sharedregion_get_config
 */
#define CMD_SHAREDREGION_GETCONFIG	_IOWR(IPC_IOC_MAGIC,		\
					SHAREDREGION_GETCONFIG,		\
					struct sharedregion_cmd_args)
/*
 *  Command for sharedregion_setup
 */
#define CMD_SHAREDREGION_SETUP		_IOWR(IPC_IOC_MAGIC,		\
					SHAREDREGION_SETUP,		\
					struct sharedregion_cmd_args)
/*
 *  Command for sharedregion_setup
 */
#define CMD_SHAREDREGION_DESTROY	_IOWR(IPC_IOC_MAGIC,		\
					SHAREDREGION_DESTROY,		\
					struct sharedregion_cmd_args)
/*
 *  Command for sharedregion_start
 */
#define CMD_SHAREDREGION_START		_IOWR(IPC_IOC_MAGIC,		\
					SHAREDREGION_START,		\
					struct sharedregion_cmd_args)
/*
 *  Command for sharedregion_stop
 */
#define CMD_SHAREDREGION_STOP		_IOWR(IPC_IOC_MAGIC,		\
					SHAREDREGION_STOP,		\
					struct sharedregion_cmd_args)
/*
 *  Command for sharedregion_attach
 */
#define CMD_SHAREDREGION_ATTACH		_IOWR(IPC_IOC_MAGIC,		\
					SHAREDREGION_ATTACH,		\
					struct sharedregion_cmd_args)
/*
 *  Command for sharedregion_detach
 */
#define CMD_SHAREDREGION_DETACH		_IOWR(IPC_IOC_MAGIC,		\
					SHAREDREGION_DETACH,		\
					struct sharedregion_cmd_args)
/*
 *  Command for sharedregion_get_heap
 */
#define CMD_SHAREDREGION_GETHEAP	_IOWR(IPC_IOC_MAGIC,		\
					SHAREDREGION_GETHEAP,		\
					struct sharedregion_cmd_args)
/*
 *  Command for sharedregion_clear_entry
 */
#define CMD_SHAREDREGION_CLEARENTRY	_IOWR(IPC_IOC_MAGIC,		\
					SHAREDREGION_CLEARENTRY,	\
					struct sharedregion_cmd_args)
/*
 *  Command for sharedregion_set_entry
 */
#define CMD_SHAREDREGION_SETENTRY	_IOWR(IPC_IOC_MAGIC,		\
					SHAREDREGION_SETENTRY,		\
					struct sharedregion_cmd_args)
/*
 *  Command for sharedregion_reserve_memory
 */
#define CMD_SHAREDREGION_RESERVEMEMORY	_IOWR(IPC_IOC_MAGIC,		\
					SHAREDREGION_RESERVEMEMORY,	\
					struct sharedregion_cmd_args)
/*
 *  Command for sharedregion_clear_reserved_memory
 */
#define CMD_SHAREDREGION_CLEARRESERVEDMEMORY				\
					_IOWR(IPC_IOC_MAGIC,		\
					SHAREDREGION_CLEARRESERVEDMEMORY, \
					struct sharedregion_cmd_args)
/*
 *  Command for sharedregion_get_region_info
 */
#define CMD_SHAREDREGION_GETREGIONINFO	_IOWR(IPC_IOC_MAGIC,		\
					SHAREDREGION_GETREGIONINFO,	\
					struct sharedregion_cmd_args)

/*
 *  Command arguments for sharedregion
 */
union sharedregion_arg {
	struct {
		struct sharedregion_config *config;
	} get_config;

	struct {
		struct sharedregion_region *regions;
		struct sharedregion_config *config;
	} setup;

	struct {
		struct sharedregion_region *regions;
	} get_region_info;

	struct {
		u16 remote_proc_id;
	} attach;

	struct {
		u16 remote_proc_id;
	} detach;

	struct {
		u16 id;
		void *heap_handle;
	} get_heap;

	struct {
		u16 id;
		struct sharedregion_entry entry;
	} set_entry;

	struct {
		u16 id;
	} clear_entry;

	struct {
		u16 id;
		u32 size;
	} reserve_memory;
};

/*
 *  Command arguments for sharedregion
 */
struct sharedregion_cmd_args {
	union sharedregion_arg args;
	s32 api_status;
};

/*
 *  This ioctl interface for sharedregion module
 */
int sharedregion_ioctl(struct inode *inode, struct file *filp,
			unsigned int cmd, unsigned long args, bool user);

#endif /* _SHAREDREGION_IOCTL_H */
