/*
*  nameserver_ioctl.h
*
*  This provides the ioctl interface for nameserver module
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

#ifndef _NAMESERVER_IOCTL_H_
#define _NAMESERVER_IOCTL_H_

#include <linux/ioctl.h>
#include <linux/types.h>

#include <ipc_ioctl.h>
#include <nameserver.h>

enum CMD_NAMESERVER {
	NAMESERVER_SETUP = NAMESERVER_BASE_CMD,
	NAMESERVER_DESTROY,
	NAMESERVER_PARAMS_INIT,
	NAMESERVER_CREATE,
	NAMESERVER_DELETE,
	NAMESERVER_ADD,
	NAMESERVER_ADDUINT32,
	NAMESERVER_GET,
	NAMESERVER_GETLOCAL,
	NAMESERVER_MATCH,
	NAMESERVER_REMOVE,
	NAMESERVER_REMOVEENTRY,
	NAMESERVER_GETHANDLE,
	NAMESERVER_ISREGISTERED,
	NAMESERVER_GETCONFIG
};

/*
 *  IOCTL command IDs for nameserver
 *
 */
/*
 *  Command for nameserver_setup
 */
#define CMD_NAMESERVER_SETUP		_IOWR(IPC_IOC_MAGIC,		\
					NAMESERVER_SETUP,		\
					struct nameserver_cmd_args)

/*
 *  Command for nameserver_destroy
 */
#define CMD_NAMESERVER_DESTROY		_IOWR(IPC_IOC_MAGIC,		\
					NAMESERVER_DESTROY,		\
					struct nameserver_cmd_args)

/*
 *  Command for nameserver_params_init
 */
#define CMD_NAMESERVER_PARAMS_INIT	_IOWR(IPC_IOC_MAGIC,		\
					NAMESERVER_PARAMS_INIT,		\
					struct nameserver_cmd_args)

/*
 *  Command for nameserver_create
 */
#define CMD_NAMESERVER_CREATE		_IOWR(IPC_IOC_MAGIC,		\
					NAMESERVER_CREATE,		\
					struct nameserver_cmd_args)

/*
 *  Command for nameserver_delete
 */
#define CMD_NAMESERVER_DELETE		_IOWR(IPC_IOC_MAGIC,		\
					NAMESERVER_DELETE,		\
					struct nameserver_cmd_args)

/*
 *  Command for nameserver_add
 */
#define CMD_NAMESERVER_ADD		_IOWR(IPC_IOC_MAGIC,		\
					NAMESERVER_ADD,			\
					struct nameserver_cmd_args)

/*
 *  Command for nameserver_addu32
 */
#define CMD_NAMESERVER_ADDUINT32	_IOWR(IPC_IOC_MAGIC,		\
					NAMESERVER_ADDUINT32,		\
					struct nameserver_cmd_args)
/*
 *  Command for nameserver_get
 */
#define CMD_NAMESERVER_GET		_IOWR(IPC_IOC_MAGIC,		\
					NAMESERVER_GET,			\
					struct nameserver_cmd_args)

/*
 *  Command for nameserver_get_local
 */
#define CMD_NAMESERVER_GETLOCAL		_IOWR(IPC_IOC_MAGIC,		\
					NAMESERVER_GETLOCAL,		\
					struct nameserver_cmd_args)

/*
 *  Command for nameserver_match
 */
#define CMD_NAMESERVER_MATCH		_IOWR(IPC_IOC_MAGIC,		\
					NAMESERVER_MATCH,		\
					struct nameserver_cmd_args)

/*
 *  Command for nameserver_remove
 */
#define CMD_NAMESERVER_REMOVE		_IOWR(IPC_IOC_MAGIC,		\
					NAMESERVER_REMOVE,		\
					struct nameserver_cmd_args)

/*
 *  Command for nameserver_remove_entry
 */
#define CMD_NAMESERVER_REMOVEENTRY	_IOWR(IPC_IOC_MAGIC,		\
					NAMESERVER_REMOVEENTRY,		\
					struct nameserver_cmd_args)

/*
 *  Command for nameserver_get_handle
 */
#define CMD_NAMESERVER_GETHANDLE	_IOWR(IPC_IOC_MAGIC,		\
					NAMESERVER_GETHANDLE,		\
					struct nameserver_cmd_args)

/*
 *  Command for NameServer_isRegistered
 */
#define CMD_NAMESERVER_ISREGISTERED	_IOWR(IPC_IOC_MAGIC,		\
					NAMESERVER_ISREGISTERED,	\
					struct nameserver_cmd_args)

/*
 *  Command for NameServer_getConfig
 */
#define CMD_NAMESERVER_GETCONFIG	_IOWR(IPC_IOC_MAGIC,		\
					NAMESERVER_GETCONFIG,		\
					struct nameserver_cmd_args)

/*
 *  Command arguments for nameserver
 */
union nameserver_arg {
	struct {
		struct nameserver_config *config;
	} get_config;

	struct {
		struct nameserver_config *config;
	} setup;

	struct {
		struct nameserver_params *params;
	} params_init;

	struct {
		void *handle;
		char *name;
		u32 name_len;
		struct nameserver_params *params;
	} create;

	struct {
		void *handle;
	} delete_instance;

	struct {
		void *handle;
		char *name;
		u32 name_len;
		void *buf;
		s32 len;
		void *entry;
		void *node;
	} add;

	struct {
		void *handle;
		char *name;
		u32 name_len;
		u32 value;
		void *entry;
	} addu32;

	struct {
		void *handle;
		char *name;
		u32 name_len;
		void *value;
		u32 len;
		u16 *proc_id;
		u32 proc_len;
	} get;

	struct {
		void *handle;
		char *name;
		u32 name_len;
		void *value;
		u32 len;
	} get_local;

	struct {
		void *handle;
		char *name;
		u32 name_len;
		u32 value;
		u32 count;
	} match;

	struct {
		void *handle;
		char *name;
		u32 name_len;
	} remove;

	struct {
		void *handle;
		void *entry;
	} remove_entry;

	struct {
		void *handle;
		char *name;
		u32 name_len;
	} get_handle;

	struct {
		u16 proc_id;
		bool check;
	} is_registered;
};

/*
 *  Command arguments for nameserver
 */
struct nameserver_cmd_args {
	union nameserver_arg args;
	s32 api_status;
};

/*
 *  This ioctl interface for nameserver module
 */
int nameserver_ioctl(struct inode *inode, struct file *filp,
			unsigned int cmd, unsigned long args, bool user);

#endif	/* _NAMESERVER_IOCTL_H_ */
