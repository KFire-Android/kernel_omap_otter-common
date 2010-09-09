/*
 *  heapbufmp_ioctl.h
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

#ifndef _HEAPBUFMP_IOCTL_
#define _HEAPBUFMP_IOCTL_

#include <linux/ioctl.h>
#include <linux/types.h>

#include <ipc_ioctl.h>
#include <heapbufmp.h>


enum CMD_HEAPBUF {
	HEAPBUFMP_GETCONFIG = HEAPBUFMP_BASE_CMD,
	HEAPBUFMP_SETUP,
	HEAPBUFMP_DESTROY,
	HEAPBUFMP_PARAMS_INIT,
	HEAPBUFMP_CREATE,
	HEAPBUFMP_DELETE,
	HEAPBUFMP_OPEN,
	HEAPBUFMP_OPENBYADDR,
	HEAPBUFMP_CLOSE,
	HEAPBUFMP_ALLOC,
	HEAPBUFMP_FREE,
	HEAPBUFMP_SHAREDMEMREQ,
	HEAPBUFMP_GETSTATS,
	HEAPBUFMP_GETEXTENDEDSTATS
};

/*
 *  Command for heapbufmp_get_config
 */
#define CMD_HEAPBUFMP_GETCONFIG		_IOWR(IPC_IOC_MAGIC, \
					HEAPBUFMP_GETCONFIG, \
					struct heapbufmp_cmd_args)

/*
 *  Command for heapbufmp_setup
 */
#define CMD_HEAPBUFMP_SETUP		_IOWR(IPC_IOC_MAGIC, \
					HEAPBUFMP_SETUP, \
					struct heapbufmp_cmd_args)
/*
 *  Command for heapbufmp_destroy
 */
#define CMD_HEAPBUFMP_DESTROY		_IOWR(IPC_IOC_MAGIC, \
					HEAPBUFMP_DESTROY, \
					struct heapbufmp_cmd_args)

/*
 *  Command for heapbufmp_prams_init
 */
#define CMD_HEAPBUFMP_PARAMS_INIT	_IOWR(IPC_IOC_MAGIC, \
					HEAPBUFMP_PARAMS_INIT, \
					struct heapbufmp_cmd_args)

/*
 *  Command for heapbufmp_create
 */
#define CMD_HEAPBUFMP_CREATE		_IOWR(IPC_IOC_MAGIC, \
					HEAPBUFMP_CREATE, \
					struct heapbufmp_cmd_args)

/*
 *  Command for heapbufmp_delete
 */
#define CMD_HEAPBUFMP_DELETE		_IOWR(IPC_IOC_MAGIC, \
					HEAPBUFMP_DELETE, \
					struct heapbufmp_cmd_args)

/*
 *  Command for heapbufmp_open
 */
#define CMD_HEAPBUFMP_OPEN		_IOWR(IPC_IOC_MAGIC, \
					HEAPBUFMP_OPEN, \
					struct heapbufmp_cmd_args)

/*
 *  Command for heapbufmp_open_by_addr
 */
#define CMD_HEAPBUFMP_OPENBYADDR	_IOWR(IPC_IOC_MAGIC, \
					HEAPBUFMP_OPENBYADDR, \
					struct heapbufmp_cmd_args)

/*
 *  Command for heapbufmp_close
 */
#define CMD_HEAPBUFMP_CLOSE		_IOWR(IPC_IOC_MAGIC, \
					HEAPBUFMP_CLOSE, \
					struct heapbufmp_cmd_args)

/*
 *  Command for heapbufmp_alloc
 */
#define CMD_HEAPBUFMP_ALLOC		_IOWR(IPC_IOC_MAGIC, \
					HEAPBUFMP_ALLOC, \
					struct heapbufmp_cmd_args)

/*
 *  Command for heapbufmp_free
 */
#define CMD_HEAPBUFMP_FREE		_IOWR(IPC_IOC_MAGIC, \
					HEAPBUFMP_FREE, \
					struct heapbufmp_cmd_args)

/*
 *  Command for heapbufmp_shared_mem_req
 */
#define CMD_HEAPBUFMP_SHAREDMEMREQ	_IOWR(IPC_IOC_MAGIC, \
					HEAPBUFMP_SHAREDMEMREQ, \
					struct heapbufmp_cmd_args)

/*
 *  Command for heapbufmp_get_stats
 */
#define CMD_HEAPBUFMP_GETSTATS		_IOWR(IPC_IOC_MAGIC, \
					HEAPBUFMP_GETSTATS, \
					struct heapbufmp_cmd_args)

/*
 *  Command for heapbufmp_get_extended_stats
 */
#define CMD_HEAPBUFMP_GETEXTENDEDSTATS	_IOWR(IPC_IOC_MAGIC, \
					HEAPBUFMP_GETEXTENDEDSTATS, \
					struct heapbufmp_cmd_args)


/*
 *  Command arguments for heapbuf
 */
union heapbufmp_arg {
	struct {
		struct heapbufmp_params *params;
	} params_init;

	struct  {
		struct heapbufmp_config *config;
	} get_config;

	struct {
		struct heapbufmp_config *config;
	} setup;

	struct {
		void *handle;
		struct heapbufmp_params *params;
		u32 name_len;
		u32 *shared_addr_srptr;
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
		struct heapbufmp_extended_stats *stats;
	} get_extended_stats;

	struct {
		void *handle;
		struct heapbufmp_params *params;
		u32 *shared_addr_srptr;
		u32 bytes;
	} shared_mem_req;
};

/*
 *  Command arguments for heapbuf
 */
struct heapbufmp_cmd_args{
	union heapbufmp_arg args;
	s32 api_status;
};

/*
 *  This ioctl interface for heapbuf module
 */
int heapbufmp_ioctl(struct inode *pinode, struct file *filp,
			unsigned int cmd, unsigned long  args, bool user);

#endif /* _HEAPBUFMP_IOCTL_ */
