/*
 *  transportshm_setup_proxy.h
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

#ifndef _TRANSPORTSHM_SETUP_PROXY_H_
#define _TRANSPORTSHM_SETUP_PROXY_H_

/* Module headers */
#include <transportshm_setup.h>

/* =============================================================================
 *  APIs
 * =============================================================================
 */
/* function that will be called in messageq_attach */
#define messageq_setup_transport_proxy_attach(remote_proc_id, shared_addr) \
				transportshm_setup_attach(remote_proc_id, \
								shared_addr)

/* function that will be called in messageq_detach */
#define messageq_setup_transport_proxy_detach(remote_proc_id) \
				transportshm_setup_detach(remote_proc_id)

/* Shared memory req function */
#define messageq_setup_transport_proxy_shared_mem_req(shared_addr) \
				transportshm_setup_shared_mem_req(shared_addr)

/* is_registered function */
#define messageq_setup_transport_proxy_is_registered(remote_proc_id) \
				transportshm_setup_is_registered(remote_proc_id)

#endif /* _TRANSPORTSHM_SETUP_PROXY_H_ */
