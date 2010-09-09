/*
 *  sysipc_ioctl.h
 *
 *  Definitions of sysmgr driver types and structures..
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

#ifndef _SYSIPC_IOCTL_H_
#define _SYSIPC_IOCTL_H_

/* Standard headers */
#include <linux/types.h>

/* Syslink headers */
#include <ipc_ioctl.h>
#include <ipc.h>


/* =============================================================================
 *  Macros and types
 * =============================================================================
 */
/*  ----------------------------------------------------------------------------
 *  IOCTL command IDs for sysmgr
 *  ----------------------------------------------------------------------------
 */
/* IOC Magic Number for sysmgr */
#define SYSIPC_IOC_MAGIC		IPC_IOC_MAGIC

/* IOCTL command numbers for ipc/sysipc */
enum sysipc_drv_cmd {
	IPC_SETUP = IPC_BASE_CMD,
	IPC_DESTROY,
	IPC_CONTROL,
	IPC_READCONFIG,
	IPC_WRITECONFIG
 };

/* Command for ipc_setup */
#define CMD_IPC_SETUP \
			_IOWR(SYSIPC_IOC_MAGIC, IPC_SETUP, \
			struct sysipc_cmd_args)

/* Command for ipc_destroy */
#define CMD_IPC_DESTROY \
			_IOWR(SYSIPC_IOC_MAGIC, IPC_DESTROY, \
			struct sysipc_cmd_args)

/* Command for load callback */
#define CMD_IPC_CONTROL \
			_IOWR(SYSIPC_IOC_MAGIC, IPC_CONTROL, \
			struct sysipc_cmd_args)

/* Command for ipc_read_config */
#define CMD_IPC_READCONFIG \
			_IOWR(SYSIPC_IOC_MAGIC, IPC_READCONFIG, \
			struct sysipc_cmd_args)

/* Command for ipc_write_config */
#define CMD_IPC_WRITECONFIG \
			_IOWR(SYSIPC_IOC_MAGIC, IPC_WRITECONFIG, \
			struct sysipc_cmd_args)


/*  ----------------------------------------------------------------------------
 *  Command arguments for sysmgr
 *  ----------------------------------------------------------------------------
 */
/* Command arguments for sysmgr */
struct sysipc_cmd_args {
	union {
		struct {
			u16 proc_id;
			s32 cmd_id;
			void *arg;
		} control;

		struct {
			u16 remote_proc_id;
			u32 tag;
			void *cfg;
			u32 size;
		} read_config;

		struct {
			u16 remote_proc_id;
			u32 tag;
			void *cfg;
			u32 size;
		} write_config;

		struct {
			struct ipc_config *config;
		} setup;
	} args;

	s32 api_status;
};

/*  ----------------------------------------------------------------------------
 *  IOCTL functions for sysmgr module
 *  ----------------------------------------------------------------------------
 */
/* ioctl interface function for sysmgr */
int sysipc_ioctl(struct inode *inode, struct file *filp,
			unsigned int cmd, unsigned long args, bool user);

#endif /* _SYSIPC_IOCTL_H_ */
