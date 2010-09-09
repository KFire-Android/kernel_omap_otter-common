/*
 *  gatemp_ioctl.h
 *
 *  Definitions of gatemp ioctl types and structures.
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

#ifndef _GATEMP_IOCTL_H_
#define _GATEMP_IOCTL_H_

#include <linux/ioctl.h>
#include <linux/types.h>

#include <ipc_ioctl.h>
#include <gatemp.h>

/* =============================================================================
 *  Macros and types
 * =============================================================================
 */
#define GATEMP_IOC_MAGIC		IPC_IOC_MAGIC
/*  IOCTL command ID definitions for GateMP */
enum CMD_GATEMP {
	GATEMP_GETCONFIG = GATEMP_BASE_CMD,
	GATEMP_SETUP,
	GATEMP_DESTROY,
	GATEMP_PARAMS_INIT,
	GATEMP_CREATE,
	GATEMP_DELETE,
	GATEMP_OPEN,
	GATEMP_CLOSE,
	GATEMP_ENTER,
	GATEMP_LEAVE,
	GATEMP_SHAREDMEMREQ,
	GATEMP_OPENBYADDR,
	GATEMP_GETDEFAULTREMOTE
};

/*
 *  IOCTL command IDs for GateMP
 */
/* Command for gatemp_get_config */
#define CMD_GATEMP_GETCONFIG		_IOWR(GATEMP_IOC_MAGIC,		\
					GATEMP_GETCONFIG,		\
					struct gatemp_cmd_args)
/* Command for gatemp_setup */
#define CMD_GATEMP_SETUP		_IOWR(GATEMP_IOC_MAGIC,		\
					GATEMP_SETUP,			\
					struct gatemp_cmd_args)
/* Command for gatemp_destroy */
#define CMD_GATEMP_DESTROY		_IOWR(GATEMP_IOC_MAGIC,		\
					GATEMP_DESTROY,			\
					struct gatemp_cmd_args)
/* Command for gatemp_params_init */
#define CMD_GATEMP_PARAMS_INIT		_IOWR(GATEMP_IOC_MAGIC,		\
					GATEMP_PARAMS_INIT,		\
					struct gatemp_cmd_args)
/* Command for gatemp_create */
#define CMD_GATEMP_CREATE		_IOWR(GATEMP_IOC_MAGIC,		\
					GATEMP_CREATE,			\
					struct gatemp_cmd_args)
/* Command for gatemp_delete */
#define CMD_GATEMP_DELETE		_IOWR(GATEMP_IOC_MAGIC,		\
					GATEMP_DELETE,			\
					struct gatemp_cmd_args)
/* Command for gatemp_open */
#define CMD_GATEMP_OPEN			_IOWR(GATEMP_IOC_MAGIC,		\
					GATEMP_OPEN,			\
					struct gatemp_cmd_args)
/* Command for gatemp_close */
#define CMD_GATEMP_CLOSE		_IOWR(GATEMP_IOC_MAGIC,		\
					GATEMP_CLOSE,			\
					struct gatemp_cmd_args)
/* Command for gatemp_enter */
#define CMD_GATEMP_ENTER		_IOWR(GATEMP_IOC_MAGIC,		\
					GATEMP_ENTER,			\
					struct gatemp_cmd_args)
/* Command for gatemp_leave */
#define CMD_GATEMP_LEAVE		_IOWR(GATEMP_IOC_MAGIC,		\
					GATEMP_LEAVE,			\
					struct gatemp_cmd_args)
/* Command for gatemp_shared_mem_req */
#define CMD_GATEMP_SHAREDMEMREQ		_IOWR(GATEMP_IOC_MAGIC,		\
					GATEMP_SHAREDMEMREQ,		\
					struct gatemp_cmd_args)
/* Command for gatemp_open_by_addr */
#define CMD_GATEMP_OPENBYADDR		_IOWR(GATEMP_IOC_MAGIC,		\
					GATEMP_OPENBYADDR,		\
					struct gatemp_cmd_args)
/* Command for gatemp_get_default_remote */
#define CMD_GATEMP_GETDEFAULTREMOTE	_IOWR(GATEMP_IOC_MAGIC,		\
					GATEMP_GETDEFAULTREMOTE,	\
					struct gatemp_cmd_args)

/* Command arguments for GateMP */
struct gatemp_cmd_args {
	union {
		struct {
			struct gatemp_params *params;
		} params_init;

		struct {
			struct gatemp_config *config;
		} get_config;

		struct {
			struct gatemp_config *config;
		} setup;

		struct {
			void *handle;
			struct gatemp_params *params;
			u32 name_len;
			u32 *shared_addr_srptr;
		} create;

		struct {
			void *handle;
		} delete_instance;

		struct {
			void *handle;
			char *name;
			u32 name_len;
			u32 *shared_addr_srptr;
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
			int *flags;
		} enter;

		struct {
			void *handle;
			int *flags;
		} leave;

		struct {
			struct gatemp_params *params;
			u32 ret_val;
		} shared_mem_req;

		struct {
			void *handle;
		} get_default_remote;
	} args;

	s32 api_status;
};


/*
 * ioctl interface function for gatemp
 */
int gatemp_ioctl(struct inode *inode, struct file *filp,
			unsigned int cmd, unsigned long args, bool user);

#endif /* _GATEMP_IOCTL_H_ */
