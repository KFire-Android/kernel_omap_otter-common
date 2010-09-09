/*
 *  sysmemmgr_ioctl.h
 *
 *  Definitions of sysmemmgr driver types and structures..
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

#ifndef _SYSMEMMGR_IOCTL_H_
#define _SYSMEMMGR_IOCTL_H_

/* Standard headers */
#include <linux/types.h>

/* Syslink headers */
#include <ipc_ioctl.h>
#include <sysmgr.h>


/* =============================================================================
 *  Macros and types
 * =============================================================================
 */
/*  ----------------------------------------------------------------------------
 *  IOCTL command IDs for sysmemmgr
 *  ----------------------------------------------------------------------------
 */
/* IOC Magic Number for sysmemmgr */
#define SYSMEMMGR_IOC_MAGIC		IPC_IOC_MAGIC

/* IOCTL command numbers for sysmemmgr */
enum sysmemmgr_drv_cmd {
	SYSMEMMGR_GETCONFIG = SYSMEMMGR_BASE_CMD,
	SYSMEMMGR_SETUP,
	SYSMEMMGR_DESTROY,
	SYSMEMMGR_ALLOC,
	SYSMEMMGR_FREE,
	SYSMEMMGR_TRANSLATE
};

/* Command for sysmemmgr_getConfig */
#define CMD_SYSMEMMGR_GETCONFIG \
			_IOWR(SYSMEMMGR_IOC_MAGIC, SYSMEMMGR_GETCONFIG, \
			struct sysmemmgr_cmd_args)

/* Command for sysmemmgr_setup */
#define CMD_SYSMEMMGR_SETUP \
			_IOWR(SYSMEMMGR_IOC_MAGIC, SYSMEMMGR_SETUP, \
			struct sysmemmgr_cmd_args)

/* Command for sysmemmgr_destroy */
#define CMD_SYSMEMMGR_DESTROY \
			_IOWR(SYSMEMMGR_IOC_MAGIC, SYSMEMMGR_DESTROY, \
			struct sysmemmgr_cmd_args)

/* Command for sysmemmgr_alloc */
#define CMD_SYSMEMMGR_ALLOC \
			_IOWR(SYSMEMMGR_IOC_MAGIC, SYSMEMMGR_ALLOC, \
			struct sysmemmgr_cmd_args)

/* Command for sysmemmgr_free */
#define CMD_SYSMEMMGR_FREE \
			_IOWR(SYSMEMMGR_IOC_MAGIC, SYSMEMMGR_FREE, \
			struct sysmemmgr_cmd_args)

/* Command for sysmemmgr_translate */
#define CMD_SYSMEMMGR_TRANSLATE \
			_IOWR(SYSMEMMGR_IOC_MAGIC, SYSMEMMGR_TRANSLATE, \
			struct sysmemmgr_cmd_args)


/*  ----------------------------------------------------------------------------
 *  Command arguments for sysmemmgr
 *  ----------------------------------------------------------------------------
 */
/* Command arguments for sysmemmgr */
struct sysmemmgr_cmd_args {
	union {
		struct {
			struct sysmemmgr_config *config;
		} get_config;

		struct {
			struct sysmemmgr_config *config;
		} setup;

		struct {
			u32 size;
			void *buf;
			void *phys;
			void *kbuf;
			enum sysmemmgr_allocflag flags;
		} alloc;

		struct {
			u32 size;
			void *buf;
			void *phys;
			void *kbuf;
			enum sysmemmgr_allocflag flags;
		} free;

		struct {
			void *buf;
			void *ret_ptr;
			enum sysmemmgr_xltflag flags;
		} translate;
	} args;

	s32 api_status;
};

/*  ----------------------------------------------------------------------------
 *  IOCTL functions for sysmemmgr module
 *  ----------------------------------------------------------------------------
 */
/* ioctl interface function for sysmemmgr */
int sysmemmgr_ioctl(struct inode *inode, struct file *filp,
				unsigned int cmd, unsigned long args);

#endif /* SYSMEMMGR_DRVDEFS_H_0xF414 */
