/*
 *  transportshm_setup.h
 *
 *  Shared Memory Transport setup layer
 *
 *  This file contains the declarations of types and APIs as part
 *  of interface of the shared memory transport.
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

#ifndef _TRANSPORTSHM_SETUP_H_
#define _TRANSPORTSHM_SETUP_H_

/* Standard headers */
#include <linux/types.h>

/* Utilities headers */
#include <linux/list.h>

/* =============================================================================
 *  APIs called by applications
 * =============================================================================
 */

/* Function that will be called in messageq_attach */
int transportshm_setup_attach(u16 remote_proc_id, u32 *shared_addr);

/* Function that will be called in messageq_detach */
int transportshm_setup_detach(u16 remote_proc_id);

/* Function that returns the amount of shared memory required */
u32 transportshm_setup_shared_mem_req(u32 *shared_addr);

/* Determines if a transport has been registered to a remote processor */
bool transportshm_setup_is_registered(u16 remote_proc_id);

#endif /* _TRANSPORTSHM_SETUP_H_ */
