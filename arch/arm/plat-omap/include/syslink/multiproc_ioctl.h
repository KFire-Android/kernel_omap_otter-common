/*
*  multiproc_ioctl.h
*
*  This provides the ioctl interface for multiproc module
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

#ifndef _MULTIPROC_IOCTL_H_
#define _MULTIPROC_IOCTL_H_

#include <linux/ioctl.h>
#include <linux/types.h>

#include <ipc_ioctl.h>
#include <multiproc.h>

enum CMD_MULTIPROC {
	MULTIPROC_SETUP = MULTIPROC_BASE_CMD,
	MULTIPROC_DESTROY,
	MULTIPROC_GETCONFIG,
	MULTIPROC_SETLOCALID
};

/*  ----------------------------------------------------------------------------
 *  IOCTL command IDs for MultiProc
 *  ----------------------------------------------------------------------------
 */

/*
 *  Command for multiproc_setup
 */
#define CMD_MULTIPROC_SETUP	_IOWR(IPC_IOC_MAGIC, MULTIPROC_SETUP,          \
				struct multiproc_cmd_args)

/*
 *  Command for multiproc_destroy
 */
#define CMD_MULTIPROC_DESTROY	_IOWR(IPC_IOC_MAGIC, MULTIPROC_DESTROY,        \
				struct multiproc_cmd_args)

/*
 *  Command for multiproc_get_config
 */
#define CMD_MULTIPROC_GETCONFIG _IOWR(IPC_IOC_MAGIC, MULTIPROC_GETCONFIG,      \
				struct multiproc_cmd_args)

/*
 *  Command for multiproc_set_local_id
 */
#define CMD_MULTIPROC_SETLOCALID _IOWR(IPC_IOC_MAGIC, MULTIPROC_SETLOCALID,    \
				struct multiproc_cmd_args)

/*
 *  Command arguments for multiproc
 */
union multiproc_arg {
	struct {
		struct multiproc_config *config;
	} get_config;

	struct {
		struct multiproc_config *config;
	} setup;

	struct {
		u16 id;
	} set_local_id;
};

/*
 *  Command arguments for multiproc
 */
struct multiproc_cmd_args {
	union multiproc_arg args;
	s32 api_status;
};

/*
 *  This ioctl interface for multiproc module
 */
int multiproc_ioctl(struct inode *inode, struct file *filp,
			unsigned int cmd, unsigned long args, bool user);

#endif	/* _MULTIPROC_IOCTL_H_ */
