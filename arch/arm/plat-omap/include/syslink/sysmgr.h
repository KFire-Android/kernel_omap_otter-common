/*
 *  sysmgr.h
 *
 *  Defines for System manager.
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

#ifndef _SYSMGR_H_
#define _SYSMGR_H_


/* Module headers */
#include <multiproc.h>
#include <gatepeterson.h>
#include <sharedregion.h>
#include <listmp.h>
#include <listmp_sharedmemory.h>
#include <messageq.h>
#include <messageq_transportshm.h>
#include <notify.h>
#include <notify_ducatidriver.h>
#include <nameserver.h>
#include <nameserver_remote.h>
#include <nameserver_remotenotify.h>
#include <procmgr.h>
#include <heap.h>
#include <heapbuf.h>
#include <sysmemmgr.h>


/*!
 *  @def    SYSMGR_MODULEID
 *  @brief  Unique module ID.
 */
#define SYSMGR_MODULEID			(0xF086)


/* =============================================================================
 * Module Success and Failure codes
 * =============================================================================
 */
/*!
 *  @def    SYSMGR_STATUSCODEBASE
 *  @brief  Error code base for System manager.
 */
#define SYSMGR_STATUSCODEBASE		(SYSMGR_MODULEID << 12u)

/*!
 *  @def    SYSMGR_MAKE_FAILURE
 *  @brief  Macro to make error code.
 */
#define SYSMGR_MAKE_FAILURE(x)			((s32)(0x80000000 + \
						(SYSMGR_STATUSCODEBASE + \
						(x))))

/*!
 *  @def    SYSMGR_MAKE_SUCCESS
 *  @brief  Macro to make success code.
 */
#define SYSMGR_MAKE_SUCCESS(x)		(SYSMGR_STATUSCODEBASE + (x))

/*!
 *  @def    SYSMGR_E_INVALIDARG
 *  @brief  Argument passed to a function is invalid.
 */
#define SYSMGR_E_INVALIDARG			SYSMGR_MAKE_FAILURE(1)

/*!
 *  @def    SYSMGR_E_MEMORY
 *  @brief  Memory allocation failed.
 */
#define SYSMGR_E_MEMORY			SYSMGR_MAKE_FAILURE(2)

/*!
 *  @def    SYSMGR_E_FAIL
 *  @brief  General failure.
 */
#define SYSMGR_E_FAIL				SYSMGR_MAKE_FAILURE(3)

/*!
 *  @def    SYSMGR_E_INVALIDSTATE
 *  @brief  Module is in invalid state.
 */
#define SYSMGR_E_INVALIDSTATE			SYSMGR_MAKE_FAILURE(4)

/*!
 *  @def    SYSMGR_E_OSFAILURE
 *  @brief  Failure in OS call.
 */
#define SYSMGR_E_OSFAILURE			SYSMGR_MAKE_FAILURE(5)

/*!
 *  @def    SYSMGR_S_ALREADYSETUP
 *  @brief  Module is already initialized.
 */
#define SYSMGR_S_ALREADYSETUP			SYSMGR_MAKE_SUCCESS(1)

/*!
 *  @def    SYSMGR_CMD_SCALABILITY
 *  @brief  Command ID for scalability info.
 */
#define SYSMGR_CMD_SCALABILITY			(0x00000000)

/*!
 *  @def    SYSMGR_CMD_SHAREDREGION_ENTRY_BASE
 *  @brief  Base of command IDs for entries used by Shared region.
 */
#define SYSMGR_CMD_SHAREDREGION_ENTRY_START	(0x00000001)
#define SYSMGR_CMD_SHAREDREGION_ENTRY_END		(0x00001000)


/* =============================================================================
 * Structures & Enums
 * =============================================================================
 */
/*!
 *  @brief  Structure defining config parameters for overall System.
 */
struct sysmgr_config {
	struct sysmemmgr_config sysmemmgr_cfg;
	/*!< System memory manager config parameter */

	struct multiproc_config multiproc_cfg;
	/*!< Multiproc config parameter */

	struct gatepeterson_config gatepeterson_cfg;
	/*!< Gatepeterson config parameter */

	struct sharedregion_config sharedregion_cfg;
	/*!< SharedRegion config parameter */

	struct messageq_config messageq_cfg;
	/*!< MessageQ config parameter */

	struct notify_config notify_cfg;
	/*!< Notify config parameter */

	struct proc_mgr_config proc_mgr_cfg;
	/*!< Processor manager config parameter */

	struct heapbuf_config heapbuf_cfg;
	/*!< Heap Buf config parameter */

	struct listmp_config listmp_sharedmemory_cfg;
	/*!< ListMPSharedMemory config parameter */

	struct messageq_transportshm_config messageq_transportshm_cfg;
	/*!< MessageQTransportShm config parameter */

	struct notify_ducatidrv_config notify_ducatidrv_cfg;
	/*!< NotifyDriverShm config parameter */

	struct nameserver_remotenotify_config nameserver_remotenotify_cfg;
	/*!< NameServerRemoteNotify config parameter */
};


/* =============================================================================
 * APIs
 * =============================================================================
 */
/* Function to initialize the parameter structure */
void sysmgr_get_config(struct sysmgr_config *config);

/* Function to initialize sysmgr module */
s32 sysmgr_setup(const struct sysmgr_config *config);

/* Function to Finalize sysmgr module */
s32 sysmgr_destroy(void);


#endif /* ifndef SYSMGR_H_0xF086 */
