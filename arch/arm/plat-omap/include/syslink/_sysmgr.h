/*
 *  _sysmgr.h
 *
 *  Defines for system manager functions
 *
 *  Copyright (C) 2009 Texas Instruments, Inc.
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

#ifndef __SYSMGR_H_
#define __SYSMGR_H_

/* Structure to retrieve the scalability proc info from the slave */
struct sysmgr_proc_config {
	u32 proc_id;
	u32 use_notify;
	u32 use_messageq;
	u32 use_heapbuf;
	u32 use_frameq;
	u32 use_ringio;
	u32 use_listmp;
	u32 use_nameserver;
	u32 boot_mode;
};

/* Function to set the boot load page address for a slave */
void sysmgr_set_boot_load_page(u16 proc_id, u32 boot_load_page);

/* Function to get configuration values for a host object(component/instance) */
u32 sysmgr_get_object_config(u16 proc_id, void *config, u32 cmd_id, u32 size);

/* Function to put configuration values for a slave object(component/instance)*/
u32 sysmgr_put_object_config(u16 proc_id, void *config, u32 cmd_id, u32 size);

/* Function to wait for scalability handshake value. */
void sysmgr_wait_for_scalability_info(u16 proc_id);

/* Function to wait for slave to complete setup */
void sysmgr_wait_for_slave_setup(u16 proc_id);


#endif /* ifndef __SYSMGR_H_ */
