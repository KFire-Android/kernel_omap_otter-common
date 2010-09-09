/*
 *  heapmemmp_ioctl.h
 *
 *  Heap module manages fixed size buffers that can be used
 *  in a multiprocessor system with shared memory.
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

#ifndef _HEAPMEMMP_IOCTL_
#define _HEAPMEMMP_IOCTL_

#include <linux/types.h>

#include <ipc_ioctl.h>
#include <heapmemmp.h>


enum CMD_HEAPMEM {
	HEAPMEMMP_GETCONFIG = HEAPMEMMP_BASE_CMD,
	HEAPMEMMP_SETUP,
	HEAPMEMMP_DESTROY,
	HEAPMEMMP_PARAMS_INIT,
	HEAPMEMMP_CREATE,
	HEAPMEMMP_DELETE,
	HEAPMEMMP_OPEN,
	HEAPMEMMP_OPENBYADDR,
	HEAPMEMMP_CLOSE,
	HEAPMEMMP_ALLOC,
	HEAPMEMMP_FREE,
	HEAPMEMMP_SHAREDMEMREQ,
	HEAPMEMMP_GETSTATS,
	HEAPMEMMP_GETEXTENDEDSTATS,
	HEAPMEMMP_RESTORE
};

/*
 *  Command for heapmemmp_get_config
 */
#define CMD_HEAPMEMMP_GETCONFIG		_IOWR(IPC_IOC_MAGIC, \
					HEAPMEMMP_GETCONFIG,\
					struct heapmemmp_cmd_args)

/*
 *  Command for heapmemmp_setup
 */
#define CMD_HEAPMEMMP_SETUP		_IOWR(IPC_IOC_MAGIC, \
					HEAPMEMMP_SETUP, \
					struct heapmemmp_cmd_args)
/*
 *  Command for heapmemmp_destroy
 */
#define CMD_HEAPMEMMP_DESTROY		_IOWR(IPC_IOC_MAGIC, \
					HEAPMEMMP_DESTROY, \
					struct heapmemmp_cmd_args)

/*
 *  Command for heapmemmp_prams_init
 */
#define CMD_HEAPMEMMP_PARAMS_INIT	_IOWR(IPC_IOC_MAGIC, \
					HEAPMEMMP_PARAMS_INIT, \
					struct heapmemmp_cmd_args)

/*
 *  Command for heapmemmp_create
 */
#define CMD_HEAPMEMMP_CREATE		_IOWR(IPC_IOC_MAGIC, \
					HEAPMEMMP_CREATE, \
					struct heapmemmp_cmd_args)

/*
 *  Command for heapmemmp_delete
 */
#define CMD_HEAPMEMMP_DELETE		_IOWR(IPC_IOC_MAGIC, \
					HEAPMEMMP_DELETE, \
					struct heapmemmp_cmd_args)

/*
 *  Command for heapmemmp_open
 */
#define CMD_HEAPMEMMP_OPEN		_IOWR(IPC_IOC_MAGIC, \
					HEAPMEMMP_OPEN, \
					struct heapmemmp_cmd_args)

/*
 *  Command for heapmemmp_open_by_addr
 */
#define CMD_HEAPMEMMP_OPENBYADDR	_IOWR(IPC_IOC_MAGIC, \
					HEAPMEMMP_OPENBYADDR, \
					struct heapmemmp_cmd_args)

/*
 *  Command for heapmemmp_close
 */
#define CMD_HEAPMEMMP_CLOSE		_IOWR(IPC_IOC_MAGIC, \
					HEAPMEMMP_CLOSE, \
					struct heapmemmp_cmd_args)

/*
 *  Command for heapmemmp_alloc
 */
#define CMD_HEAPMEMMP_ALLOC		_IOWR(IPC_IOC_MAGIC, \
					HEAPMEMMP_ALLOC, \
					struct heapmemmp_cmd_args)

/*
 *  Command for heapmemmp_free
 */
#define CMD_HEAPMEMMP_FREE		_IOWR(IPC_IOC_MAGIC, \
					HEAPMEMMP_FREE, \
					struct heapmemmp_cmd_args)

/*
 *  Command for heapmemmp_shared_mem_req
 */
#define CMD_HEAPMEMMP_SHAREDMEMREQ	_IOWR(IPC_IOC_MAGIC, \
					HEAPMEMMP_SHAREDMEMREQ, \
					struct heapmemmp_cmd_args)

/*
 *  Command for heapmemmp_get_stats
 */
#define CMD_HEAPMEMMP_GETSTATS		_IOWR(IPC_IOC_MAGIC, \
					HEAPMEMMP_GETSTATS, \
					struct heapmemmp_cmd_args)

/*
 *  Command for heapmemmp_get_extended_stats
 */
#define CMD_HEAPMEMMP_GETEXTENDEDSTATS	_IOWR(IPC_IOC_MAGIC, \
					HEAPMEMMP_GETEXTENDEDSTATS, \
					struct heapmemmp_cmd_args)

/*
 *  Command for heapmemmp_restore
 */
#define CMD_HEAPMEMMP_RESTORE		_IOWR(IPC_IOC_MAGIC, \
					HEAPMEMMP_RESTORE, \
					struct heapmemmp_cmd_args)


/*
 *  Command arguments for heapmem
 */
union heapmemmp_arg {
	struct {
		struct heapmemmp_params *params;
	} params_init;

	struct  {
		struct heapmemmp_config *config;
	} get_config;

	struct {
		struct heapmemmp_config *config;
	} setup;

	struct {
		void *handle;
		struct heapmemmp_params *params;
		u32 name_len;
		u32 *shared_addr_srptr;
		u32 *shared_buf_srptr;
		void *knl_gate;
	} create;

	struct {
		void *handle;
	} delete;

	struct {
		void *handle;
		char *name;
		u32 name_len;
	} open;

	struct {
		void *handle;
		u32 *shared_addr_srptr;
	} open_by_addr;

	struct {
		void *handle;
	} close;

	struct {
		void *handle;
		u32 size;
		u32 align;
		u32 *block_srptr;
	} alloc;

	struct {
		void *handle;
		u32 *block_srptr;
		u32 size;
	} free;

	struct {
		void *handle;
		struct memory_stats *stats;
	} get_stats;

	struct {
		void *handle;
	} restore;

	struct {
		void *handle;
		struct heapmemmp_extended_stats *stats;
	} get_extended_stats;

	struct {
		struct heapmemmp_params *params;
		u32 *shared_addr_srptr;
		u32 bytes;
	} shared_mem_req;
};

/*
 *  Command arguments for heapmem
 */
struct heapmemmp_cmd_args{
	union heapmemmp_arg args;
	s32 api_status;
};

/*
 *  This ioctl interface for heapmem module
 */
int heapmemmp_ioctl(struct inode *pinode, struct file *filp,
			unsigned int cmd, unsigned long  args, bool user);

#endif /* _HEAPMEMMP_IOCTL_ */
