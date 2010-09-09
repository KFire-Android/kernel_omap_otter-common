/*
 *  gatemp.h
 *
 *  Definitions of gatemp support proxies
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

#ifndef _GATEMPDEFS_H_
#define _GATEMPDEFS_H_


/* Utilities headers */
#include <gatepeterson.h>
#include <gatehwspinlock.h>
/* Enable once ported - GateMPSupportNull may not be needed
#include <_GateMPSupportNull.h>
#include <GateMPSupportNull.h>
*/

#if 1 /* Enable when SpinLock is available */
#define gatemp_remote_system_proxy_params_init	gatehwspinlock_params_init
#define gatemp_remote_custom1_proxy_params_init	gatepeterson_params_init
#define gatemp_remote_custom2_proxy_params_init	gatepeterson_params_init
#define gatemp_remote_system_proxy_create	gatehwspinlock_create
#define gatemp_remote_custom1_proxy_create	gatepeterson_create
#define gatemp_remote_custom2_proxy_create	gatepeterson_create
#define gatemp_remote_system_proxy_delete	gatehwspinlock_delete
#define gatemp_remote_custom1_proxy_delete	gatepeterson_delete
#define gatemp_remote_custom2_proxy_delete	gatepeterson_delete
#define gatemp_remote_system_proxy_params	struct gatehwspinlock_params
#define gatemp_remote_custom1_proxy_params	struct gatepeterson_params
#define gatemp_remote_custom2_proxy_params	struct gatepeterson_params
#define gatemp_remote_system_proxy_shared_mem_req	\
						gatehwspinlock_shared_mem_req
#define gatemp_remote_custom1_proxy_shared_mem_req	\
						gatepeterson_shared_mem_req
#define gatemp_remote_custom2_proxy_shared_mem_req	\
						gatepeterson_shared_mem_req
#define gatemp_remote_system_proxy_get_num_instances	\
						gatehwspinlock_get_num_instances
#define gatemp_remote_custom1_proxy_get_num_instances	\
						gatepeterson_get_num_instances
#define gatemp_remote_custom2_proxy_get_num_instances	\
						gatepeterson_get_num_instances
#define gatemp_remote_system_proxy_get_num_reserved	\
						gatehwspinlock_get_num_reserved
#define gatemp_remote_custom1_proxy_get_num_reserved	\
						gatepeterson_get_num_reserved
#define gatemp_remote_custom2_proxy_get_num_reserved	\
						gatepeterson_get_num_reserved
#define gatemp_remote_system_proxy_locks_init	gatehwspinlock_locks_init
#define gatemp_remote_custom1_proxy_locks_init	gatepeterson_locks_init
#define gatemp_remote_custom2_proxy_locks_init	gatepeterson_locks_init
#define gatemp_remote_system_proxy_handle	void *
#define gatemp_remote_custom1_proxy_handle	void *
#define gatemp_remote_custom2_proxy_handle	void *
#define gatemp_remote_system_proxy_open_by_addr	gatehwspinlock_open_by_addr
#define gatemp_remote_custom1_proxy_open_by_addr	\
						gatepeterson_open_by_addr
#define gatemp_remote_custom2_proxy_open_by_addr	\
						gatepeterson_open_by_addr
#define gatemp_remote_system_proxy_enter	gatehwspinlock_enter
#define gatemp_remote_system_proxy_leave	gatehwspinlock_leave
#define gatemp_remote_custom1_proxy_enter	gatepeterson_enter
#define gatemp_remote_custom1_proxy_leave	gatepeterson_leave
#define gatemp_remote_custom2_proxy_enter	gatepeterson_enter
#define gatemp_remote_custom2_proxy_leave	gatepeterson_leave
#else
#define gatemp_remote_system_proxy_params_init	gatepeterson_params_init
#define gatemp_remote_custom1_proxy_params_init	gatepeterson_params_init
#define gatemp_remote_custom2_proxy_params_init	gatepeterson_params_init
#define gatemp_remote_system_proxy_create	gatepeterson_create
#define gatemp_remote_custom1_proxy_create	gatepeterson_create
#define gatemp_remote_custom2_proxy_create	gatepeterson_create
#define gatemp_remote_system_proxy_delete	gatepeterson_delete
#define gatemp_remote_custom1_proxy_delete	gatepeterson_delete
#define gatemp_remote_custom2_proxy_delete	gatepeterson_delete
#define gatemp_remote_system_proxy_params	struct gatepeterson_params
#define gatemp_remote_custom1_proxy_params	struct gatepeterson_params
#define gatemp_remote_custom2_proxy_params	struct gatepeterson_params
#define gatemp_remote_system_proxy_shared_mem_req	\
						gatepeterson_shared_mem_req
#define gatemp_remote_custom1_proxy_shared_mem_req	\
						gatepeterson_shared_mem_req
#define gatemp_remote_custom2_proxy_shared_mem_req	\
						gatepeterson_shared_mem_req
#define gatemp_remote_system_proxy_get_num_instances	\
						gatepeterson_get_num_instances
#define gatemp_remote_custom1_proxy_get_num_instances	\
						gatepeterson_get_num_instances
#define gatemp_remote_custom2_proxy_get_num_instances	\
						gatepeterson_get_num_instances
#define gatemp_remote_system_proxy_get_num_reserved	\
						gatepeterson_get_num_reserved
#define gatemp_remote_custom1_proxy_get_num_reserved	\
						gatepeterson_get_num_reserved
#define gatemp_remote_custom2_proxy_get_num_reserved	\
						gatepeterson_get_num_reserved
#define gatemp_remote_system_proxy_locks_init	gatepeterson_locks_init
#define gatemp_remote_custom1_proxy_locks_init	gatepeterson_locks_init
#define gatemp_remote_custom2_proxy_locks_init	gatepeterson_locks_init
#define gatemp_remote_system_proxy_handle	void *
#define gatemp_remote_custom1_proxy_handle	void *
#define gatemp_remote_custom2_proxy_handle	void *
#define gatemp_remote_system_proxy_open_by_addr	gatepeterson_open_by_addr
#define gatemp_remote_custom1_proxy_open_by_addr	\
						gatepeterson_open_by_addr
#define gatemp_remote_custom2_proxy_open_by_addr	\
						gatepeterson_open_by_addr
#define gatemp_remote_system_proxy_enter	gatepeterson_enter
#define gatemp_remote_system_proxy_leave	gatepeterson_leave
#define gatemp_remote_custom1_proxy_enter	gatepeterson_enter
#define gatemp_remote_custom1_proxy_leave	gatepeterson_leave
#define gatemp_remote_custom2_proxy_enter	gatepeterson_enter
#define gatemp_remote_custom2_proxy_leave	gatepeterson_leave
#endif

#endif /* _GATEMPDEFS_H_ */
