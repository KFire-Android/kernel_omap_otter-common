/*
 *  messageq_ioctl.h
 *
 *  Definitions of messageq driver types and structures.
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

#ifndef _MESSAGEQ_IOCTL_H_
#define _MESSAGEQ_IOCTL_H_

/* Standard headers */
#include <linux/types.h>

/* Syslink headers */
#include <ipc_ioctl.h>
#include <messageq.h>
#include <heap.h>
#include <sharedregion.h>

/* =============================================================================
 *  Macros and types
 * =============================================================================
 */
#define MESSAGEQ_IOC_MAGIC		IPC_IOC_MAGIC
enum messageq_drv_cmd {
	MESSAGEQ_GETCONFIG = MESSAGEQ_BASE_CMD,
	MESSAGEQ_SETUP,
	MESSAGEQ_DESTROY,
	MESSAGEQ_PARAMS_INIT,
	MESSAGEQ_CREATE,
	MESSAGEQ_DELETE,
	MESSAGEQ_OPEN,
	MESSAGEQ_CLOSE,
	MESSAGEQ_COUNT,
	MESSAGEQ_ALLOC,
	MESSAGEQ_FREE,
	MESSAGEQ_PUT,
	MESSAGEQ_REGISTERHEAP,
	MESSAGEQ_UNREGISTERHEAP,
	MESSAGEQ_ATTACH,
	MESSAGEQ_DETACH,
	MESSAGEQ_GET,
	MESSAGEQ_SHAREDMEMREQ,
	MESSAGEQ_UNBLOCK
};

/*  ----------------------------------------------------------------------------
 *  IOCTL command IDs for messageq
 *  ----------------------------------------------------------------------------
 */
/* Base command ID for messageq */
#define MESSAGEQ_BASE_CMD			0x0

/* Command for messageq_get_config */
#define CMD_MESSAGEQ_GETCONFIG \
			_IOWR(MESSAGEQ_IOC_MAGIC, MESSAGEQ_GETCONFIG, \
			struct messageq_cmd_args)

/* Command for messageq_setup */
#define CMD_MESSAGEQ_SETUP \
			_IOWR(MESSAGEQ_IOC_MAGIC, MESSAGEQ_SETUP, \
			struct messageq_cmd_args)

/* Command for messageq_destroy */
#define CMD_MESSAGEQ_DESTROY \
			_IOWR(MESSAGEQ_IOC_MAGIC, MESSAGEQ_DESTROY, \
			struct messageq_cmd_args)

/* Command for messageq_params_init */
#define CMD_MESSAGEQ_PARAMS_INIT \
			_IOWR(MESSAGEQ_IOC_MAGIC, MESSAGEQ_PARAMS_INIT, \
			struct messageq_cmd_args)

/* Command for messageq_create */
#define CMD_MESSAGEQ_CREATE \
			_IOWR(MESSAGEQ_IOC_MAGIC, MESSAGEQ_CREATE, \
			struct messageq_cmd_args)

/* Command for messageq_delete */
#define CMD_MESSAGEQ_DELETE \
			_IOWR(MESSAGEQ_IOC_MAGIC, MESSAGEQ_DELETE, \
			struct messageq_cmd_args)

/* Command for messageq_open */
#define CMD_MESSAGEQ_OPEN \
			_IOWR(MESSAGEQ_IOC_MAGIC, MESSAGEQ_OPEN, \
			struct messageq_cmd_args)

/* Command for messageq_close */
#define CMD_MESSAGEQ_CLOSE \
			_IOWR(MESSAGEQ_IOC_MAGIC, MESSAGEQ_CLOSE, \
			struct messageq_cmd_args)

/* Command for messageq_count */
#define CMD_MESSAGEQ_COUNT \
			_IOWR(MESSAGEQ_IOC_MAGIC, MESSAGEQ_COUNT, \
			struct messageq_cmd_args)

/* Command for messageq_alloc */
#define CMD_MESSAGEQ_ALLOC \
			_IOWR(MESSAGEQ_IOC_MAGIC, MESSAGEQ_ALLOC, \
			struct messageq_cmd_args)

/* Command for messageq_free */
#define CMD_MESSAGEQ_FREE \
			_IOWR(MESSAGEQ_IOC_MAGIC, MESSAGEQ_FREE, \
			struct messageq_cmd_args)

/* Command for messageq_put */
#define CMD_MESSAGEQ_PUT \
			_IOWR(MESSAGEQ_IOC_MAGIC, MESSAGEQ_PUT, \
			struct messageq_cmd_args)

/* Command for messageq_unblock */
#define CMD_MESSAGEQ_UNBLOCK \
			_IOWR(MESSAGEQ_IOC_MAGIC, MESSAGEQ_UNBLOCK, \
			struct messageq_cmd_args)

/* Command for messageq_register_heap */
#define CMD_MESSAGEQ_REGISTERHEAP \
			_IOWR(MESSAGEQ_IOC_MAGIC, MESSAGEQ_REGISTERHEAP, \
			struct messageq_cmd_args)

/* Command for messageq_unregister_heap */
#define CMD_MESSAGEQ_UNREGISTERHEAP \
			_IOWR(MESSAGEQ_IOC_MAGIC, MESSAGEQ_UNREGISTERHEAP, \
			struct messageq_cmd_args)


/* Command for messageq_attach */
#define CMD_MESSAGEQ_ATTACH \
			_IOWR(MESSAGEQ_IOC_MAGIC, MESSAGEQ_ATTACH, \
			struct messageq_cmd_args)

/* Command for messageq_detach */
#define CMD_MESSAGEQ_DETACH \
			_IOWR(MESSAGEQ_IOC_MAGIC, MESSAGEQ_DETACH, \
			struct messageq_cmd_args)

/* Command for messageq_get */
#define CMD_MESSAGEQ_GET \
			_IOWR(MESSAGEQ_IOC_MAGIC, MESSAGEQ_GET, \
			struct messageq_cmd_args)

/* Command for messageq_sharedmem_req */
#define CMD_MESSAGEQ_SHAREDMEMREQ \
			_IOWR(MESSAGEQ_IOC_MAGIC, MESSAGEQ_SHAREDMEMREQ, \
			struct messageq_cmd_args)

/* Command arguments for messageq */
struct messageq_cmd_args {
	union {
		struct {
			void *messageq_handle;
			struct messageq_params *params;
		} params_init;

		struct {
			struct messageq_config *config;
		} get_config;

		struct {
			struct messageq_config *config;
		} setup;

		struct {
			void *messageq_handle;
			char *name;
			struct messageq_params *params;
			u32 name_len;
			u32 queue_id;
		} create;

		struct {
			void *messageq_handle;
		} delete_messageq;

		struct {
			char *name;
			u32 queue_id;
			u32 name_len;
		} open;

		struct {
			u32 queue_id;
		} close;

		struct {
			void *messageq_handle;
			u32 timeout;
			u32 *msg_srptr;
		} get;

		struct {
			void *messageq_handle;
			int count;
		} count;

		struct {
			u16 heap_id;
			u32 size;
			u32 *msg_srptr;
		} alloc;

		struct {
			u32 *msg_srptr;
		} free;

		struct {
			u32 queue_id;
			u32 *msg_srptr;
		} put;

		struct {
			void *heap_handle;
			u16 heap_id;
		} register_heap;

		struct {
			u16 heap_id;
		} unregister_heap;

		struct {
			u32 *shared_addr_srptr;
			uint mem_req;
		} shared_mem_req;

		struct {
			u16 remote_proc_id;
			u32 *shared_addr_srptr;
		} attach;

		struct {
			u16 remote_proc_id;
		} detach;

		struct {
			void *messageq_handle;
		} unblock;

	} args;

	int api_status;
};

/*  ----------------------------------------------------------------------------
 *  IOCTL functions for messageq module
 *  ----------------------------------------------------------------------------
 */
/*
 *  ioctl interface function for messageq
 */
int messageq_ioctl(struct inode *inode, struct file *filp,
			unsigned int cmd, unsigned long args, bool user);

#endif /* _MESSAGEQ_IOCTL_H_ */
