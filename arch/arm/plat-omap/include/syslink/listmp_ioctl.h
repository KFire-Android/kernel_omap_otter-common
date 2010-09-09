/*
 *  listmp_ioctl.h
 *
 *  Definitions of listmp driver types and structures.
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

#ifndef _LISTMP_IOCTL_H_
#define _LISTMP_IOCTL_H_

/* Standard headers */
#include <linux/types.h>

/* Syslink headers */
#include <ipc_ioctl.h>
#include <listmp.h>
#include <sharedregion.h>

/* =============================================================================
 *  Macros and types
 * =============================================================================
 */
/* Base command ID for listmp */
#define LISTMP_IOC_MAGIC		IPC_IOC_MAGIC
enum listmp_drv_cmd {
	LISTMP_GETCONFIG = LISTMP_BASE_CMD,
	LISTMP_SETUP,
	LISTMP_DESTROY,
	LISTMP_PARAMS_INIT,
	LISTMP_CREATE,
	LISTMP_DELETE,
	LISTMP_OPEN,
	LISTMP_CLOSE,
	LISTMP_ISEMPTY,
	LISTMP_GETHEAD,
	LISTMP_GETTAIL,
	LISTMP_PUTHEAD,
	LISTMP_PUTTAIL,
	LISTMP_INSERT,
	LISTMP_REMOVE,
	LISTMP_NEXT,
	LISTMP_PREV,
	LISTMP_SHAREDMEMREQ,
	LISTMP_OPENBYADDR
};

/*  ----------------------------------------------------------------------------
 *  IOCTL command IDs for listmp
 *  ----------------------------------------------------------------------------
 */
/* Command for listmp_get_config */
#define CMD_LISTMP_GETCONFIG		_IOWR(LISTMP_IOC_MAGIC,		\
					LISTMP_GETCONFIG,		\
					struct listmp_cmd_args)

/* Command for listmp_setup */
#define CMD_LISTMP_SETUP		_IOWR(LISTMP_IOC_MAGIC,		\
					LISTMP_SETUP,			\
					struct listmp_cmd_args)

/* Command for listmp_destroy */
#define CMD_LISTMP_DESTROY		_IOWR(LISTMP_IOC_MAGIC,		\
					LISTMP_DESTROY,			\
					struct listmp_cmd_args)

/* Command for listmp_params_init */
#define CMD_LISTMP_PARAMS_INIT		_IOWR(LISTMP_IOC_MAGIC,		\
					LISTMP_PARAMS_INIT,		\
					struct listmp_cmd_args)

/* Command for listmp_create */
#define CMD_LISTMP_CREATE		_IOWR(LISTMP_IOC_MAGIC,		\
					LISTMP_CREATE,			\
					struct listmp_cmd_args)

/* Command for listmp_delete */
#define CMD_LISTMP_DELETE		_IOWR(LISTMP_IOC_MAGIC,		\
					LISTMP_DELETE,			\
					struct listmp_cmd_args)

/* Command for listmp_open */
#define CMD_LISTMP_OPEN			_IOWR(LISTMP_IOC_MAGIC,		\
					LISTMP_OPEN,			\
					struct listmp_cmd_args)

/* Command for listmp_close */
#define CMD_LISTMP_CLOSE		_IOWR(LISTMP_IOC_MAGIC,		\
					LISTMP_CLOSE,			\
					struct listmp_cmd_args)

/* Command for listmp_is_empty */
#define CMD_LISTMP_ISEMPTY		_IOWR(LISTMP_IOC_MAGIC,		\
					LISTMP_ISEMPTY,			\
					struct listmp_cmd_args)

/* Command for listmp_get_head */
#define CMD_LISTMP_GETHEAD		_IOWR(LISTMP_IOC_MAGIC,		\
					LISTMP_GETHEAD,			\
					struct listmp_cmd_args)

/* Command for listmp_get_tail */
#define CMD_LISTMP_GETTAIL		_IOWR(LISTMP_IOC_MAGIC,		\
					LISTMP_GETTAIL,			\
					struct listmp_cmd_args)

/* Command for listmp_put_head */
#define CMD_LISTMP_PUTHEAD		_IOWR(LISTMP_IOC_MAGIC,		\
					LISTMP_PUTHEAD,			\
					struct listmp_cmd_args)

/* Command for listmp_put_tail */
#define CMD_LISTMP_PUTTAIL		_IOWR(LISTMP_IOC_MAGIC,		\
					LISTMP_PUTTAIL,			\
					struct listmp_cmd_args)

/* Command for listmp_insert */
#define CMD_LISTMP_INSERT		_IOWR(LISTMP_IOC_MAGIC,		\
					LISTMP_INSERT,			\
					struct listmp_cmd_args)

/* Command for listmp_remove */
#define CMD_LISTMP_REMOVE		_IOWR(LISTMP_IOC_MAGIC,		\
					LISTMP_REMOVE,			\
					struct listmp_cmd_args)

/* Command for listmp_next */
#define CMD_LISTMP_NEXT			_IOWR(LISTMP_IOC_MAGIC,		\
					LISTMP_NEXT,			\
					struct listmp_cmd_args)

/* Command for listmp_prev */
#define CMD_LISTMP_PREV			_IOWR(LISTMP_IOC_MAGIC,		\
					LISTMP_PREV,			\
					struct listmp_cmd_args)

/* Command for listmp_shared_mem_req */
#define CMD_LISTMP_SHAREDMEMREQ		_IOWR(LISTMP_IOC_MAGIC,		\
					LISTMP_SHAREDMEMREQ,		\
					struct listmp_cmd_args)

/* Command for listmp_open_by_addr */
#define CMD_LISTMP_OPENBYADDR		_IOWR(LISTMP_IOC_MAGIC,		\
					LISTMP_OPENBYADDR,		\
					struct listmp_cmd_args)

/* Command arguments for listmp */
struct listmp_cmd_args {
	union {
		struct {
			struct listmp_params *params;
		} params_init;

		struct {
			struct listmp_config *config;
		} get_config;

		struct {
			struct listmp_config *config;
		} setup;

		struct {
			void *listmp_handle;
			struct listmp_params *params;
			u32 name_len;
			u32 shared_addr_srptr;
			void *knl_gate;
		} create;

		struct {
			void *listmp_handle;
		} delete_instance;

		struct {
			void *listmp_handle;
			u32 name_len;
			char *name;
		} open;

		struct {
			void *listmp_handle;
			u32 shared_addr_srptr;
		} open_by_addr;


		struct {
			void *listmp_handle;
		} close;

		struct {
			void *listmp_handle;
			bool is_empty;
		} is_empty;

		struct {
			void *listmp_handle;
			u32 *elem_srptr;
		} get_head;

		struct {
			void *listmp_handle;
			u32 *elem_srptr;
		} get_tail;

		struct {
			void *listmp_handle;
			u32 *elem_srptr;
		} put_head;

		struct {
			void *listmp_handle;
			u32 *elem_srptr;
		} put_tail;

		struct {
			void *listmp_handle;
			u32 *new_elem_srptr;
			u32 *cur_elem_srptr;
		} insert;

		struct {
			void *listmp_handle;
			u32 *elem_srptr;
		} remove;

		struct {
			void *listmp_handle;
			u32 *elem_srptr;
			u32 *next_elem_srptr;
		} next;

		struct {
			void *listmp_handle;
			u32 *elem_srptr;
			u32 *prev_elem_srptr;
		} prev;

		struct {
			struct listmp_params *params;
			u32 bytes;
			void *knl_gate;
			u32 *shared_addr_srptr;
			u32 name_len;
			void *listmp_handle;
		} shared_mem_req;
	} args;

	int api_status;
};

/*  ----------------------------------------------------------------------------
 *  IOCTL functions for listmp module
 *  ----------------------------------------------------------------------------
 */
/* ioctl interface function for listmp */
int listmp_ioctl(struct inode *inode, struct file *filp,
			unsigned int cmd, unsigned long args, bool user);

#endif /* _LISTMP_IOCTL_H_ */
