/*
 *  ipc.c
 *
 *  This module is primarily used to configure IPC-wide settings and
 *           initialize IPC at runtime
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


/* Standard headers */
#include <linux/types.h>
#include <linux/module.h>

#include <syslink/atomic_linux.h>

/* Module headers */
#include <multiproc.h>
#include <ipc.h>
#include <platform.h>

#include <gatemp.h>
#include <sharedregion.h>
#include <notify.h>
#include <notify_ducatidriver.h>
#include <notify_setup_proxy.h>

#include <heap.h>
#include <heapbufmp.h>
#include <heapmemmp.h>

#include <messageq.h>
#include <nameserver.h>
#include <nameserver_remotenotify.h>

/* Ipu Power Management Header (ipu_pm) */
#include "../ipu_pm/ipu_pm.h"
/* =============================================================================
 * Macros
 * =============================================================================
 */
/* Macro to make a correct module magic number with ref_count */
#define IPC_MAKE_MAGICSTAMP(x)((IPC_MODULEID << 16u) | (x))

/* flag for starting processor synchronization */
#define IPC_PROCSYNCSTART    1

/* flag for finishing processor synchronization */
#define IPC_PROCSYNCFINISH   2

#define ROUND_UP(a, b)	(((a) + ((b) - 1)) & (~((b) - 1)))

/* =============================================================================
 * Enums & Structures
 * =============================================================================
 */

/* The structure used for reserving memory in SharedRegion */
struct ipc_reserved {
	VOLATILE u32	started_key;
	u32		*notify_sr_ptr;
	u32		*nsrn_sr_ptr;
	u32		*transport_sr_ptr;
	u32		*config_list_head;
};


/* head of the config list */
struct ipc_config_head {
	VOLATILE u32 first;
	/* Address of first config entry */
};


/*
 *  This structure captures Configuration details of a module/instance
 *  written by a slave to synchornize with a remote slave/HOST
 */
struct ipc_config_entry {
	VOLATILE u32 remote_proc_id;
	/* Remote processor identifier */
	VOLATILE u32 local_proc_id;
	/* Config Entry owner processor identifier */
	VOLATILE u32 tag;
	/* Unique tag to distinguish config from other config entries */
	VOLATILE u32 size;
	/* Size of the config pointer */
	VOLATILE u32 next;
	/* Address of next config entry (In SRPtr format) */
};

/*
 *  This structure defines the fields that are to be configured
 *  between the executing processor and a remote processor.
 */
struct ipc_entry {
	u16	remote_proc_id; /* the remote processor id   */
	bool	setup_notify; /* whether to setup Notify   */
	bool	setup_messageq; /* whether to setup messageq */
	bool	setup_ipu_pm; /* whether to setup ipu_pm */
};

/* Ipc instance structure. */
struct ipc_proc_entry {
	void			*local_config_list;
	void			*remote_config_list;
	void			*user_obj;
	bool			slave;
	struct ipc_entry	entry;
	bool			is_attached;
};


/* Module state structure */
struct ipc_module_state {
	s32			ref_count;
	atomic_t		start_ref_count;
	void			*ipc_shared_addr;
	void			*gatemp_shared_addr;
	void			*ipu_pm_shared_addr;
	enum ipc_proc_sync	proc_sync;
	struct ipc_config	cfg;
	struct ipc_proc_entry	proc_entry[MULTIPROC_MAXPROCESSORS];
};


/* =============================================================================
 * Forward declaration
 * =============================================================================
 */
/*
 *  ======== ipc_get_master_addr() ========
 */
static void *ipc_get_master_addr(u16 remote_proc_id, void *shared_addr);

/*
 *  ======== ipc_get_region0_reserved_size ========
 *  Returns the amount of memory to be reserved for Ipc in SharedRegion 0.
 *
 *  This is used for synchronizing processors.
 */
static u32 ipc_get_region0_reserved_size(void);

/*
 *  ======== ipc_get_slave_addr() ========
 */
static void *ipc_get_slave_addr(u16 remote_proc_id, void *shared_addr);

/*
 *  ======== ipc_proc_sync_start ========
 *  Starts the process of synchronizing processors.
 *
 *  Shared memory in region 0 will be used.  The processor which owns
 *  SharedRegion 0 writes its reserve memory address in region 0
 *  to let the other processors know it has started.  It then spins
 *  until the other processors start.  The other processors write their
 *  reserve memory address in region 0 to let the owner processor
 *  know they've started.  The other processors then spin until the
 *  owner processor writes to let them know its finished the process
 *  of synchronization before continuing.
 */
static int ipc_proc_sync_start(u16 remote_proc_id, void *shared_addr);

/*
 *  ======== ipc_proc_sync_finish ========
 *  Finishes the process of synchronizing processors.
 *
 *  Each processor writes its reserve memory address in SharedRegion 0
 *  to let the other processors know its finished the process of
 *  synchronization.
 */
static int ipc_proc_sync_finish(u16 remote_proc_id, void *shared_addr);

/*
 *  ======== ipc_reserved_size_per_proc ========
 *  The amount of memory required to be reserved per processor.
 */
static u32 ipc_reserved_size_per_proc(void);

/* TODO: figure these out */
#define gate_enter_system() 0
#define gate_leave_system(key) {}

/* =============================================================================
 * Globals
 * =============================================================================
 */
static struct ipc_module_state ipc_module_state = {
	.proc_sync = IPC_PROCSYNC_ALL,
	.ref_count = 0,
};
static struct ipc_module_state *ipc_module = &ipc_module_state;

/* =============================================================================
 * APIs
 * =============================================================================
 */
/*
 * ========== ipc_attach ==========
 * attaches to a remote processor
 */
int ipc_attach(u16 remote_proc_id)
{
	int status = 0;
#if 0
	u32 reserved_size = ipc_reserved_size_per_proc();
	bool cache_enabled = sharedregion_is_cache_enabled(0);
#endif
	void *notify_shared_addr;
	void *msgq_shared_addr;
	void *nsrn_shared_addr;
	u32 notify_mem_req;
	VOLATILE struct ipc_reserved *slave;
	struct ipc_proc_entry *ipc;

	if (remote_proc_id >= MULTIPROC_MAXPROCESSORS) {
		pr_err("Invalid remote_proc_id passed\n");
		return IPC_E_FAIL;
	}

	/* determine if self is master or slave */
	if (multiproc_self() < remote_proc_id)
		ipc_module->proc_entry[remote_proc_id].slave = true;
	else
		ipc_module->proc_entry[remote_proc_id].slave = false;

	/* determine the slave's slot */
	slave = ipc_get_slave_addr(remote_proc_id, ipc_module->ipc_shared_addr);
#if 0
	if (cache_enabled)
		Cache_inv((void *)slave, reserved_size, Cache_Type_ALL, true);
#endif
	/* get the attach paramters associated with remote_proc_id */
	ipc = &(ipc_module->proc_entry[remote_proc_id]);

	/* Synchronize the processors. */
	status = ipc_proc_sync_start(remote_proc_id, ipc_module->
							ipc_shared_addr);

	if (status < 0)
		pr_err("ipc_attach : ipc_proc_sync_start "
			"failed [0x%x]\n", status);
	else
		pr_err("ipc_proc_sync_start : status [0x%x]\n", status);


	if (status >= 0) {
		/* must be called before SharedRegion_attach */
		status = gatemp_attach(remote_proc_id, ipc_module->
							gatemp_shared_addr);
		if (status < 0)
			pr_err("ipc_attach : gatemp_attach "
				"failed [0x%x]\n", status);
		else
			pr_err("gatemp_attach : status [0x%x]\n", status);

	}

	/* retrieves the SharedRegion Heap handles */
	if (status >= 0) {
		status = sharedregion_attach(remote_proc_id);
		if (status < 0)
			pr_err("ipc_attach : sharedregion_attach "
				"failed [0x%x]\n", status);
		else
			pr_err("sharedregion_attach : status "
				"[0x%x]\n", status);
	}

	/* attach Notify if not yet attached and specified to set internal
								setup */
	if (status >= 0 && !notify_is_registered(remote_proc_id, 0) &&
		(ipc->entry.setup_notify)) {
		/* call notify_attach */
		if (ipc_module->proc_entry[remote_proc_id].slave) {
			notify_mem_req = notify_shared_mem_req(remote_proc_id,
				ipc_module->ipc_shared_addr);
			notify_shared_addr = sl_heap_alloc(
					sharedregion_get_heap(0),
					notify_mem_req,
					sharedregion_get_cache_line_size(0));
			memset(notify_shared_addr, 0, notify_mem_req);
			slave->notify_sr_ptr = sharedregion_get_srptr(
							notify_shared_addr, 0);
			if (slave->notify_sr_ptr ==
						SHAREDREGION_INVALIDSRPTR) {
				status = IPC_E_FAIL;
				pr_err("ipc_attach : "
					"sharedregion_get_srptr "
					"failed [0x%x]\n", status);
			} else {
				pr_err(
					"sharedregion_get_srptr : "
					"status [0x%x]\n", status);
			}
		} else {
			notify_shared_addr = sharedregion_get_ptr(slave->
							notify_sr_ptr);
			if (notify_shared_addr == NULL) {
				status = IPC_E_FAIL;
				pr_err("ipc_attach : "
					"sharedregion_get_ptr "
					"failed [0x%x]\n", status);
			} else {
				pr_err(
					"sharedregion_get_ptr : "
					"status [0x%x]\n", status);
			}
		}

		if (status >= 0) {
			status = notify_attach(remote_proc_id,
							notify_shared_addr);
			if (status < 0)
				pr_err("ipc_attach : "
					"notify_attach "
					"failed [0x%x]\n", status);
			else
					pr_err(
						"notify_attach : "
						"status [0x%x]\n", status);
		}
	}

	/* Must come before Notify because depends on default Notify */
	if (status >= 0 && ipc->entry.setup_notify && ipc->entry.setup_ipu_pm) {
		if (status >= 0) {
			status = ipu_pm_attach(remote_proc_id, ipc_module->
							ipu_pm_shared_addr);
			if (status < 0)
				pr_err("ipc_attach : "
					"ipu_pm_attach "
					"failed [0x%x]\n", status);
			else
				pr_err(
					"ipu_pm_attach : "
					"status [0x%x]\n", status);
		}
	}

	/* Must come after gatemp_start because depends on default GateMP */
	if (status >= 0 && ipc->entry.setup_notify) {
		if (ipc_module->proc_entry[remote_proc_id].slave) {
			nsrn_shared_addr = sl_heap_alloc(
					sharedregion_get_heap(0),
					nameserver_remotenotify_shared_mem_req(
									NULL),
					sharedregion_get_cache_line_size(0));

			slave->nsrn_sr_ptr =
				sharedregion_get_srptr(nsrn_shared_addr, 0);
			if (slave->nsrn_sr_ptr == SHAREDREGION_INVALIDSRPTR) {
				status = IPC_E_FAIL;
				pr_err("ipc_attach : "
					"sharedregion_get_srptr "
					"failed [0x%x]\n", status);
			} else {
				pr_err(
					"sharedregion_get_srptr : "
					"status [0x%x]\n", status);
			}
		} else {
			nsrn_shared_addr =
				sharedregion_get_ptr(slave->nsrn_sr_ptr);
			if (nsrn_shared_addr == NULL) {
				status = IPC_E_FAIL;
				pr_err("ipc_attach : "
					"sharedregion_get_ptr "
					"failed [0x%x]\n", status);
			} else {
				pr_err(
					"sharedregion_get_ptr : "
					"status [0x%x]\n", status);
			}
		}

		if (status >= 0) {
			/* create the nameserver_remotenotify instances */
			status = nameserver_remotenotify_attach(remote_proc_id,
							nsrn_shared_addr);

			if (status < 0)
				pr_err("ipc_attach : "
					"nameserver_remotenotify_attach "
					"failed [0x%x]\n", status);
			else
				pr_err(
					"nameserver_remotenotify_attach : "
					"status [0x%x]\n", status);
		}
	}

	/* Must come after gatemp_start because depends on default GateMP */
	if (status >= 0 && ipc->entry.setup_messageq) {
		if (ipc_module->proc_entry[remote_proc_id].slave) {
			msgq_shared_addr = sl_heap_alloc
					(sharedregion_get_heap(0),
					messageq_shared_mem_req(ipc_module->
							ipc_shared_addr),
					sharedregion_get_cache_line_size(0));

			slave->transport_sr_ptr =
				sharedregion_get_srptr(msgq_shared_addr, 0);
			if (slave->transport_sr_ptr ==
					SHAREDREGION_INVALIDSRPTR) {
				status = IPC_E_FAIL;
				pr_err("ipc_attach : "
					"sharedregion_get_srptr "
					"failed [0x%x]\n", status);
			} else {
				pr_err(
					"sharedregion_get_srptr : "
					"status [0x%x]\n", status);
			}
		} else {
			msgq_shared_addr = sharedregion_get_ptr(slave->
							transport_sr_ptr);
			if (msgq_shared_addr == NULL) {
				status = IPC_E_FAIL;
				pr_err("ipc_attach : "
					"sharedregion_get_ptr "
					"failed [0x%x]\n", status);
			} else {
				pr_err(
					"sharedregion_get_ptr : "
					"status [0x%x]\n", status);
			}
		}

		if (status >= 0) {
			/* create the messageq Transport instances */
			status = messageq_attach(remote_proc_id,
							msgq_shared_addr);
			if (status < 0)
				pr_err("ipc_attach : "
					"messageq_attach "
					"failed [0x%x]\n", status);
			else
				pr_err(
					"messageq_attach : "
					"status [0x%x]\n", status);
		}
	}
#if 0
	if (cache_enabled) {
		if (ipc_module->proc_entry[remote_proc_id].slave)
			Cache_wbInv((void *)slave, reserved_size,
						Cache_Type_ALL, true);
	}
#endif

	if (status >= 0) {
		/* Finish the processor synchronization */
		status = ipc_proc_sync_finish(remote_proc_id,
						ipc_module->ipc_shared_addr);
		if (status < 0)
			pr_err("ipc_attach : "
				"ipc_proc_sync_finish "
				"failed [0x%x]\n", status);
		else
			pr_err(
				"ipc_proc_sync_finish : "
				"status [0x%x]\n", status);
	}

	if (status >= 0)
		ipc->is_attached = true;
	else
		pr_err("ipc_attach failed! status = 0x%x\n", status);

	return status;
}


/*
 * ============= ipc_detach ==============
 * detaches from a remote processor
 */
int ipc_detach(u16 remote_proc_id)
{
	int status = 0;
#if 0
	u32 reserved_size = ipc_reserved_size_per_proc();
#endif
	bool cache_enabled = sharedregion_is_cache_enabled(0);
	void *notify_shared_addr;
	void *nsrn_shared_addr;
	void *msgq_shared_addr;
	VOLATILE struct ipc_reserved *slave;
	VOLATILE struct ipc_reserved *master;
	struct ipc_proc_entry *ipc;
	u32 nsrn_mem_req = nameserver_remotenotify_shared_mem_req(NULL);
		/* prefetching into local variable because of
					later space restrictions */

	/* get the paramters associated with remote_proc_id */
	ipc = &(ipc_module->proc_entry[remote_proc_id]);

	if (ipc->is_attached == false) {
		status = IPC_E_INVALIDSTATE;
		goto exit;
	}

	/* determine the slave's slot */
	slave = ipc_get_slave_addr(remote_proc_id, ipc_module->
							ipc_shared_addr);

	if (slave != NULL) {
#if 0
		if (unlikely(cache_enabled))
			Cache_inv((void *) slave, reserved_size,
						Cache_Type_ALL, true);
#endif
		if (ipc->entry.setup_messageq) {
			/* call messageq_detach for remote processor */
			status = messageq_detach(remote_proc_id);
			if (status < 0)
				pr_err("ipc_detach : "
					"messageq_detach "
					"failed [0x%x]\n", status);
			else
				pr_err(
					"messageq_detach : "
					"status [0x%x]\n", status);

			/* free the memory if slave processor */
			if (ipc_module->proc_entry[remote_proc_id].slave) {
				/* get the pointer to messageq transport
							instance */
				msgq_shared_addr = sharedregion_get_ptr(
						slave->transport_sr_ptr);

				if (msgq_shared_addr != NULL) {
					/* free the memory back to sharedregion
								0 heap */
					sl_heap_free(sharedregion_get_heap(0),
						msgq_shared_addr,
						messageq_shared_mem_req(
							msgq_shared_addr));
				}

				/* set the pointer for messageq transport
						instance back to invalid */
				slave->transport_sr_ptr =
						SHAREDREGION_INVALIDSRPTR;
			}
		}

		if (ipc->entry.setup_notify) {
			/* call nameserver_remotenotify_detach for
							remote processor */
			status = nameserver_remotenotify_detach(
							remote_proc_id);
			if (status < 0)
				pr_err("ipc_detach : "
					"nameserver_remotenotify_detach "
					"failed [0x%x]\n", status);
			else
				pr_err(
					"nameserver_remotenotify_detach : "
					"status [0x%x]\n", status);

			/* free the memory if slave processor */
			if (ipc_module->proc_entry[remote_proc_id].slave) {
				/* get the pointer to NSRN instance */
				nsrn_shared_addr = sharedregion_get_ptr(
							slave->nsrn_sr_ptr);

				if (nsrn_shared_addr != NULL)
					/* free the memory back to
							SharedRegion 0 heap */
					sl_heap_free(sharedregion_get_heap(0),
						nsrn_shared_addr,
						nsrn_mem_req);

				/* set the pointer for NSRN instance back
								to invalid */
				slave->nsrn_sr_ptr =
						SHAREDREGION_INVALIDSRPTR;
			}
		}

		if (ipc->entry.setup_notify && ipc->entry.setup_ipu_pm) {
			/* call ipu_pm_detach for remote processor */
			status = ipu_pm_detach(remote_proc_id);
			if (status < 0)
				pr_err("ipc_detach : "
					"ipu_pm_detach "
					"failed [0x%x]\n", status);
			else
				pr_err(
					"ipu_pm_detach : "
					"status [0x%x]\n", status);
		}

		if (ipc->entry.setup_notify) {
			/* call notify_detach for remote processor */
			status = notify_detach(remote_proc_id);
			if (status < 0)
				pr_err("ipc_detach : "
					"notify_detach "
					"failed [0x%x]\n", status);
			else
				pr_err(
					"notify_detach : "
					"status [0x%x]\n", status);

			/* free the memory if slave processor */
			if (ipc_module->proc_entry[remote_proc_id].slave) {
				/* get the pointer to Notify instance */
				notify_shared_addr = sharedregion_get_ptr(
							slave->notify_sr_ptr);

				if (notify_shared_addr != NULL) {
					/* free the memory back to
							SharedRegion 0 heap */
					sl_heap_free(sharedregion_get_heap(0),
						notify_shared_addr,
						notify_shared_mem_req(
							remote_proc_id,
							notify_shared_addr));
				}

				/* set the pointer for Notify instance
							back to invalid */
				slave->notify_sr_ptr =
						SHAREDREGION_INVALIDSRPTR;
			}
		}

		if (unlikely(cache_enabled)) {
			if (ipc_module->proc_entry[remote_proc_id].slave) {
				slave->started_key = 0;
				slave->config_list_head =
						SHAREDREGION_INVALIDSRPTR;
#if 0
				Cache_wbInv((void *)slave, reserved_size,
							Cache_Type_ALL, true);
#endif
			} else {
				/* determine the master's slot */
				master = ipc_get_master_addr(remote_proc_id,
						ipc_module->ipc_shared_addr);

				if (master != NULL) {
					master->started_key = 0;
					master->config_list_head =
						SHAREDREGION_INVALIDSRPTR;
#if 0
					Cache_wbInv((void *) master,
								reserved_size,
								Cache_Type_ALL,
								true);
#endif
				}
			}
		}

		/* Now detach the SharedRegion */
		status = sharedregion_detach(remote_proc_id);
		BUG_ON(status < 0);

		/* Now detach the GateMP */
		status = gatemp_detach(remote_proc_id, ipc_module->
							gatemp_shared_addr);
		BUG_ON(status < 0);
	}
	ipc->is_attached = false;

exit:
	if (status < 0)
		pr_err("ipc_detach failed with status [0x%x]\n", status);
	return status;
}


/*
 * ========= ipc_control ==========
 * Function to destroy an ipc instance for a slave
 */
int
ipc_control(u16 proc_id, u32 cmd_id, void *arg)
{
	int status = IPC_S_SUCCESS;

	switch (cmd_id) {
	case IPC_CONTROLCMD_LOADCALLBACK:
	{
#if defined CONFIG_SYSLINK_USE_SYSMGR
		status = platform_load_callback(proc_id, arg);
		if (status < 0)
			pr_err("ipc_control : platform_load_callback "
				"failed [0x%x]\n", status);
#endif
	}
	break;

	case IPC_CONTROLCMD_STARTCALLBACK:
	{
#if defined CONFIG_SYSLINK_USE_SYSMGR
		status = platform_start_callback(proc_id, arg);
		if (status < 0)
			pr_err("ipc_control : platform_start_callback"
				" failed [0x%x]\n", status);
#endif
	}
	break;

	case IPC_CONTROLCMD_STOPCALLBACK:
	{
#if defined CONFIG_SYSLINK_USE_SYSMGR
		status = platform_stop_callback(proc_id, arg);
		if (status < 0)
			pr_err("ipc_control : platform_stop_callback"
				" failed [0x%x]\n", status);
#endif
	}
	break;

	default:
	{
		status = -EINVAL;
		pr_err("ipc_control : invalid "
				" command code [0x%x]\n", cmd_id);
	}
	break;
	}

	return status;
}


/*
 *  ======== ipc_get_master_addr ========
 */
static void *ipc_get_master_addr(u16 remote_proc_id, void *shared_addr)
{
	u32 reserved_size = ipc_reserved_size_per_proc();
	int slot;
	u16 master_id;
	VOLATILE struct ipc_reserved *master;

	/* determine the master's proc_id and slot */
	if (multiproc_self() < remote_proc_id) {
		master_id = remote_proc_id;
		slot = multiproc_self();
	} else {
		master_id = multiproc_self();
		slot = remote_proc_id;
	}

	/* determine the reserve address for master between self and remote */
	master = (struct ipc_reserved *)((u32)shared_addr +
			((master_id * reserved_size) +
			(slot * sizeof(struct ipc_reserved))));

	return (void *)master;
}

/*
 *  ======== ipc_get_region0_reserved_size ========
 */
static u32 ipc_get_region0_reserved_size(void)
{
	u32 reserved_size = ipc_reserved_size_per_proc();

	/* Calculate the total amount to reserve */
	reserved_size = reserved_size * multiproc_get_num_processors();

	return reserved_size;
}

/*
 *  ======== Ipc_getSlaveAddr ========
 */
static void *ipc_get_slave_addr(u16 remote_proc_id, void *shared_addr)
{
	u32 reserved_size = ipc_reserved_size_per_proc();
	int slot;
	u16 slave_id;
	VOLATILE struct ipc_reserved *slave;

	/* determine the slave's proc_id and slot */
	if (multiproc_self() < remote_proc_id) {
		slave_id = multiproc_self();
		slot = remote_proc_id - 1;
	} else {
		slave_id = remote_proc_id;
		slot = multiproc_self() - 1;
	}

	/* determine the reserve address for slave between self and remote */
	slave = (struct ipc_reserved *)((u32)shared_addr +
			((slave_id * reserved_size) +
			(slot * sizeof(struct ipc_reserved))));

	return (void *)slave;
}

/*
 *  ======== Ipc_proc_syncStart ========
 *  The owner of SharedRegion 0 writes to its reserve memory address
 *  in region 0 to let the other processors know it has started.
 *  It then spins until the other processors start.
 *  The other processors write their reserve memory address in
 *  region 0 to let the owner processor know they've started.
 *  The other processors then spin until the owner processor writes
 *  to let them know that its finished the process of synchronization
 *  before continuing.
 */
static int ipc_proc_sync_start(u16 remote_proc_id, void *shared_addr)
{
#if 0
	u32 reserved_size = ipc_reserved_size_per_proc();
	bool cache_enabled = sharedregion_is_cache_enabled(0);
#endif
	int status = 0;
	VOLATILE struct ipc_reserved *self;
	VOLATILE struct ipc_reserved *remote;
	struct ipc_proc_entry *ipc;

	/* don't do any synchronization if proc_sync is NONE */
	if (ipc_module->proc_sync != IPC_PROCSYNC_NONE) {
		/* determine self and remote pointers */
		if (ipc_module->proc_entry[remote_proc_id].slave) {
			self = ipc_get_slave_addr(remote_proc_id, shared_addr);
			remote = ipc_get_master_addr(remote_proc_id,
								shared_addr);
		} else {
			self = ipc_get_master_addr(remote_proc_id, shared_addr);
			remote = ipc_get_slave_addr(remote_proc_id,
								shared_addr);
		}

		/* construct the config list */
		ipc = &(ipc_module->proc_entry[remote_proc_id]);

		ipc->local_config_list = (void *)&self->config_list_head;
		ipc->remote_config_list = (void *)&remote->config_list_head;

		((struct ipc_config_head *)ipc->local_config_list)->first =
						(u32)SHAREDREGION_INVALIDSRPTR;
#if 0
		if (cache_enabled) {
			Cache_wbInv(ipc->local_config_list,
					 reserved_size,
					 Cache_Type_ALL,
					 true);
		}
#endif

		if (ipc_module->proc_entry[remote_proc_id].slave) {
			/* set my processor's reserved key to start */
			self->started_key = IPC_PROCSYNCSTART;
#if 0
			/* write back my processor's reserve key */
			if (cache_enabled)
				Cache_wbInv((void *)self, reserved_size,
							Cache_Type_ALL, true);

			/* wait for remote processor to start */
			if (cache_enabled)
				Cache_inv((void *)remote, reserved_size,
							Cache_Type_ALL, true);
#endif
			if (remote->started_key != IPC_PROCSYNCSTART)
				status = IPC_E_FAIL;
			goto exit;
		}

#if 0
		/*  wait for remote processor to start */
		Cache_inv((void *)remote, reserved_size, Cache_Type_ALL, true);
#endif
		if ((self->started_key != IPC_PROCSYNCSTART) &&
			(remote->started_key != IPC_PROCSYNCSTART)) {
			status = IPC_E_FAIL;
			goto exit;
		}

		if (status >= 0) {
			/* set my processor's reserved key to start */
			self->started_key = IPC_PROCSYNCSTART;
#if 0
			/* write my processor's reserve key back */
			if (cache_enabled)
				Cache_wbInv((void *)self, reserved_size,
							Cache_Type_ALL, true);

			/* wait for remote processor to finish */
			Cache_inv((void *)remote, reserved_size,
							Cache_Type_ALL, true);
#endif
			if (remote->started_key != IPC_PROCSYNCFINISH) {
				status = IPC_E_FAIL;
				goto exit;
			}
		}
	}
exit:
	if (status < 0)
		pr_err("ipc_proc_sync_start failed: status [0x%x]\n", status);
	else
		pr_err("ipc_proc_sync_start done\n");

	return status;
}

/*
 *  ======== Ipc_proc_syncFinish ========
 *  Each processor writes its reserve memory address in SharedRegion 0
 *  to let the other processors know its finished the process of
 *  synchronization.
 */
static int ipc_proc_sync_finish(u16 remote_proc_id, void *shared_addr)
{
#if 0
	u32 reserved_size = ipc_reserved_size_per_proc();
	bool cache_enabled = sharedregion_is_cache_enabled(0);
#endif
	VOLATILE struct ipc_reserved *self;
	VOLATILE struct ipc_reserved *remote;

	/* don't do any synchronization if proc_sync is NONE */
	if (ipc_module->proc_sync != IPC_PROCSYNC_NONE) {
		/* determine self pointer */
		if (ipc_module->proc_entry[remote_proc_id].slave) {
			self = ipc_get_slave_addr(remote_proc_id, shared_addr);
			remote = ipc_get_master_addr(remote_proc_id,
					shared_addr);
		} else {
			self = ipc_get_master_addr(remote_proc_id,
								shared_addr);
			remote = ipc_get_slave_addr(remote_proc_id,
								shared_addr);
		}
		/* set my processor's reserved key to finish */
		self->started_key = IPC_PROCSYNCFINISH;
#if 0
		/* write back my processor's reserve key */
		if (cache_enabled)
			Cache_wbInv((void *)self, reserved_size,
						Cache_Type_ALL, true);
#endif
		/* if slave processor, wait for remote to finish sync */
		if (ipc_module->proc_entry[remote_proc_id].slave) {
			/* wait for remote processor to finish */
			do {
#if 0
				if (cacheEnabled) {
					Cache_inv((Ptr)remote, reservedSize,
						Cache_Type_ALL, TRUE);
				}
#endif
			} while (remote->started_key != IPC_PROCSYNCFINISH);
		}
	}

	return IPC_S_SUCCESS;
}

/*
 * ======== ipc_read_config ========
 */
int ipc_read_config(u16 remote_proc_id, u32 tag, void *cfg, u32 size)
{
#if 0
	bool cache_enabled = sharedregion_is_cache_enabled(0);
#endif
	int status = IPC_E_FAIL;
	VOLATILE struct ipc_config_entry *entry;

	if (ipc_module->ref_count == 0) {
		status = -ENODEV;
		goto exit;
	}

	if (ipc_module->proc_entry[remote_proc_id].is_attached == false) {
		status = -ENODEV;
		goto exit;
	}


#if 0
	if (cache_enabled) {
		Cache_inv(ipc_module->proc_entry[remote_proc_id].
						remote_config_list,
				sharedregion_get_cache_line_size(0),
				Cache_Type_ALL,
				true);
	}
#endif
	entry = (struct ipc_config_entry *)((struct ipc_config_head *)
		ipc_module->proc_entry[remote_proc_id].remote_config_list)->
									first;

	while ((u32 *)entry != SHAREDREGION_INVALIDSRPTR) {
		entry = (struct ipc_config_entry *)
				sharedregion_get_ptr((u32 *)entry);
		if (entry == NULL) {
			status = IPC_E_FAIL;
			goto exit;
		}
#if 0
		/* Traverse the list to find the tag */
		if (cache_enabled) {
			Cache_inv((void *)entry,
				size + sizeof(struct ipc_config_entry),
				Cache_Type_ALL,
				true);
		}
#endif

		if ((entry->remote_proc_id == multiproc_self()) &&
			(entry->local_proc_id == remote_proc_id) &&
			(entry->tag == tag)) {

			if (size == entry->size)
				memcpy(cfg,
					(void *)((u32)entry + sizeof(struct
							ipc_config_entry)),
					entry->size);
			else
				status = IPC_E_FAIL;
		} else {
			entry = (struct ipc_config_entry *)entry->next;
		}
	}


exit:
	if (status < 0)
		pr_err("ipc_read_config failed: status [0x%x]\n", status);

	return status;
}

/*
 *  ======== ipc_reserved_size_per_proc ========
 */
static u32 ipc_reserved_size_per_proc(void)
{
	u32 reserved_size = sizeof(struct ipc_reserved) *
					multiproc_get_num_processors();
	u32 cache_line_size = sharedregion_get_cache_line_size(0);

	/* Calculate amount to reserve per processor */
	if (cache_line_size > reserved_size)
		/* Use cache_line_size if larger than reserved_size */
		reserved_size = cache_line_size;
	else
		/* Round reserved_size to cache_line_size */
		reserved_size = ROUND_UP(reserved_size, cache_line_size);

	return reserved_size;
}

/*!
 *  ======== ipc_write_config ========
 */
int ipc_write_config(u16 remote_proc_id, u32 tag, void *cfg, u32 size)
{
#if 0
	bool cache_enabled = sharedregion_is_cache_enabled(0);
#endif
	u32 cache_line_size = sharedregion_get_cache_line_size(0);
	int status = IPC_S_SUCCESS;
	struct ipc_config_entry *entry;

	if (ipc_module->ref_count == 0) {
		status = -ENODEV;
		goto exit;
	}

	if (ipc_module->proc_entry[remote_proc_id].is_attached == false) {
		status = -ENODEV;
		goto exit;
	}

	/* Allocate memory from the shared heap (System Heap) */
	entry = sl_heap_alloc(sharedregion_get_heap(0),
				size + sizeof(struct ipc_config_entry),
				cache_line_size);

	if (entry == NULL) {
		status = IPC_E_FAIL;
		goto exit;
	}

	entry->remote_proc_id = remote_proc_id;
	entry->local_proc_id = multiproc_self();
	entry->tag = tag;
	entry->size = size;
	memcpy((void *)((u32)entry + sizeof(struct ipc_config_entry)),
								cfg, size);

	/* Create a linked-list of config */
	if (((struct ipc_config_head *)ipc_module->
		proc_entry[remote_proc_id].local_config_list)->first
		== (u32)SHAREDREGION_INVALIDSRPTR) {

		entry->next = (u32)SHAREDREGION_INVALIDSRPTR;
		((struct ipc_config_head *)ipc_module->
			proc_entry[remote_proc_id].local_config_list)->first =
				(u32)sharedregion_get_srptr(entry, 0);

		if (((struct ipc_config_head *)ipc_module->
			proc_entry[remote_proc_id].local_config_list)->first
			== (u32)SHAREDREGION_INVALIDSRPTR)
			status = IPC_E_FAIL;
	} else {
		entry->next = ((struct ipc_config_head *)ipc_module->
			proc_entry[remote_proc_id].local_config_list)->first;

		((struct ipc_config_head *)ipc_module->
			proc_entry[remote_proc_id].local_config_list)->first =
			(u32)sharedregion_get_srptr(entry, 0);
		if (((struct ipc_config_head *)ipc_module->
			proc_entry[remote_proc_id].local_config_list)->first
			== (u32)SHAREDREGION_INVALIDSRPTR)
			status = IPC_E_FAIL;
	}
#if 0
	if (cache_enabled) {
		Cache_wbInv(ipc_module->proc_entry[remote_proc_id].
							local_config_list,
			sharedregion_get_cache_line_size(0),
			Cache_Type_ALL,
			false);

		Cache_wbInv(entry,
			size + sizeof(struct ipc_config_entry),
			Cache_Type_ALL,
			true);
	}
#endif

exit:
	if (status < 0)
		pr_err("ipc_write_config failed: status [0x%x]\n", status);

	return status;
}


/*
 *  ======== ipc_start ========
 */
int ipc_start(void)
{
	int status = 0;
	int i;
	struct sharedregion_entry entry;
	void *ipc_shared_addr;
	void *gatemp_shared_addr;
	void *ipu_pm_shared_addr;
	struct gatemp_params gatemp_params;
	bool line_available;

	/* This sets the ref_count variable is not initialized, upper 16 bits
	 * is written with module Id to ensure correctness of ref_count
	 * variable.
	 */
	atomic_cmpmask_and_set(&(ipc_module->start_ref_count),
						IPC_MAKE_MAGICSTAMP(0),
						IPC_MAKE_MAGICSTAMP(0));
	if (atomic_inc_return(&(ipc_module->start_ref_count))
		!= IPC_MAKE_MAGICSTAMP(1u)) {
		status = IPC_S_SUCCESS;
		goto exit;
	}

	/* get region 0 information */
	sharedregion_get_entry(0, &entry);

	/* if entry is not valid then return */
	if (entry.is_valid == false) {
		status = IPC_E_FAIL;
		goto exit;
	}
	/*
	 *  Need to reserve memory in region 0 for processor synchronization.
	 *  This must done before SharedRegion_start().
	 */
	ipc_shared_addr = sharedregion_reserve_memory(0,
					ipc_get_region0_reserved_size());

	/* must reserve memory for gatemp before sharedregion_start() */
	gatemp_shared_addr = sharedregion_reserve_memory(0,
					gatemp_get_region0_reserved_size());

	/* Init params for default gate(must match those in gatemp_start() */
	gatemp_params_init(&gatemp_params);
	gatemp_params.local_protect = GATEMP_LOCALPROTECT_TASKLET;

	if (multiproc_get_num_processors() > 1)
		gatemp_params.remote_protect = GATEMP_REMOTEPROTECT_SYSTEM;
	else
		gatemp_params.remote_protect = GATEMP_REMOTEPROTECT_NONE;

	/* reserve memory for default gate before SharedRegion_start() */
	sharedregion_reserve_memory(0, gatemp_shared_mem_req(&gatemp_params));

	/* reserve memory for PM region */
	ipu_pm_shared_addr = sharedregion_reserve_memory(0,
						ipu_pm_mem_req(NULL));

	/* clear the reserved memory */
	sharedregion_clear_reserved_memory();

	/* Set shared addresses */
	ipc_module->ipc_shared_addr = ipc_shared_addr;
	ipc_module->gatemp_shared_addr = gatemp_shared_addr;
	ipc_module->ipu_pm_shared_addr = ipu_pm_shared_addr;

	/* create default GateMP, must be called before sharedregion_start */
	status = gatemp_start(ipc_module->gatemp_shared_addr);
	if (status < 0) {
		status = IPC_E_FAIL;
		goto exit;
	}

	/* create HeapMemMP in each SharedRegion */
	status = sharedregion_start();
	if (status < 0) {
		status = IPC_E_FAIL;
		goto exit;
	}

	/* Call attach for all procs if proc_sync is ALL */
	if (ipc_module->proc_sync == IPC_PROCSYNC_ALL) {
		/* Must attach to owner first to get default GateMP and
		 * HeapMemMP */
		if (multiproc_self() != entry.owner_proc_id) {
			do {
				status = ipc_attach(entry.owner_proc_id);
			} while (status < 0);
		}

		/* Loop to attach to all other processors */
		for (i = 0; i < multiproc_get_num_processors(); i++) {
			if ((i == multiproc_self())
					|| (i == entry.owner_proc_id))
				continue;
			line_available =
				notify_setup_proxy_int_line_available(i);
			if (!line_available)
				continue;
			/* call Ipc_attach for every remote processor */
			do {
				status = ipc_attach(i);
			} while (status < 0);
		}
	}

exit:
	if (status < 0)
		pr_err("ipc_start failed: status [0x%x]\n", status);

	return status;
}


/*
 *  ======== ipc_stop ========
 */
int ipc_stop(void)
{
	int status = IPC_S_SUCCESS;
	int tmp_status = IPC_S_SUCCESS;
	struct sharedregion_entry entry;
	struct gatemp_params gatemp_params;

	if (unlikely(atomic_cmpmask_and_lt(&(ipc_module->start_ref_count),
				   IPC_MAKE_MAGICSTAMP(0),
				   IPC_MAKE_MAGICSTAMP(1)) == true)) {
		status = IPC_E_FAIL;
		goto exit;
	}

	if (likely(atomic_dec_return(&ipc_module->start_ref_count)
				== IPC_MAKE_MAGICSTAMP(0))) {
		/* get region 0 information */
		sharedregion_get_entry(0, &entry);

		/* if entry is not valid then return */
		if (entry.is_valid == false) {
			status = IPC_E_FAIL;
			goto exit;
		}


		/*
		*  Need to unreserve memory in region 0 for processor
		*  synchronization. This must done before sharedregion_stop().
		*/
		sharedregion_unreserve_memory(0,
					ipc_get_region0_reserved_size());

		/* must unreserve memory for GateMP before
							sharedregion_stop() */
		sharedregion_unreserve_memory(0,
					gatemp_get_region0_reserved_size());

		/* Init params for default gate (must match those
						in gatemp_stop() */
		gatemp_params_init(&gatemp_params);
		gatemp_params.local_protect = GATEMP_LOCALPROTECT_TASKLET;

		if (multiproc_get_num_processors() > 1)
			gatemp_params.remote_protect =
					GATEMP_REMOTEPROTECT_SYSTEM;
		else
			gatemp_params.remote_protect =
					GATEMP_REMOTEPROTECT_NONE;

		/* unreserve memory for default gate before
					sharedregion_stop() */
		sharedregion_unreserve_memory(0,
			gatemp_shared_mem_req(&gatemp_params));

		/* must unreserve memory for PM before sharedregion_stop() */
		sharedregion_unreserve_memory(0, ipu_pm_mem_req(NULL));

		/* Delete heapmemmp in each sharedregion */
		status = sharedregion_stop();
		if (status < 0) {
			status = IPC_E_FAIL;
			goto exit;
		}

		/* delete default gatemp, must be called after
		 * sharedregion_stop
		 */
		tmp_status = gatemp_stop();
		if ((tmp_status < 0) && (status >= 0)) {
			status = IPC_E_FAIL;
			goto exit;
		}

		ipc_module->ipu_pm_shared_addr = NULL;
		ipc_module->gatemp_shared_addr = NULL;
		ipc_module->ipc_shared_addr = NULL;
	}
exit:
	if (status < 0)
		pr_err("ipc_stop failed: status [0x%x]\n", status);

	return status;
}


/*
 *  ======== ipc_get_config ========
 */
void ipc_get_config(struct ipc_config *cfg_params)
{
	int key;
	int status = 0;

	BUG_ON(cfg_params == NULL);

	if (cfg_params == NULL) {
		status = -EINVAL;
		goto exit;
	}


	key = gate_enter_system();
	if (ipc_module->ref_count == 0)
		cfg_params->proc_sync = IPC_PROCSYNC_ALL;
	else
		memcpy((void *) cfg_params, (void *) &ipc_module->cfg,
						sizeof(struct ipc_config));

	gate_leave_system(key);

exit:
	if (status < 0)
		pr_err("ipc_get_config failed: status [0x%x]\n", status);
}


/* Sets up ipc for this processor. */
int ipc_setup(const struct ipc_config *cfg)
{
	int status = IPC_S_SUCCESS;
	struct ipc_config tmp_cfg;
	int key;
	int i;

	key = gate_enter_system();
	ipc_module->ref_count++;

	/* This sets the ref_count variable is not initialized, upper 16 bits is
	 * written with module Id to ensure correctness of ref_count variable.
	 */
	if (ipc_module->ref_count > 1) {
		status = IPC_S_ALREADYSETUP;
		gate_leave_system(key);
		goto exit;
	}

	gate_leave_system(key);
	if (cfg == NULL) {
		ipc_get_config(&tmp_cfg);
		cfg = &tmp_cfg;
	}

	/* Copy the cfg */
	memcpy(&ipc_module->cfg, cfg, sizeof(struct ipc_config));

	ipc_module->proc_sync = cfg->proc_sync;

	status = platform_setup();
	if (status < 0) {
		key = gate_enter_system();
		ipc_module->ref_count--;
		gate_leave_system(key);
		status = IPC_E_FAIL;
		goto exit;
	}

	/* Following can be done regardless of status */
	for (i = 0; i < multiproc_get_num_processors(); i++)
		ipc_module->proc_entry[i].is_attached = false;

exit:
	if (status < 0)
		pr_err("ipc_setup failed: status [0x%x]\n", status);

	return status;
}


/*
 * =========== ipc_destroy ==========
 * Destroys ipc for this processor.
 */
int ipc_destroy(void)
{
	int status = IPC_S_SUCCESS;
	int key;

	key = gate_enter_system();
	ipc_module->ref_count--;

	if (ipc_module->ref_count < 0) {
		gate_leave_system(key);
		status = IPC_E_INVALIDSTATE;
		goto exit;
	}

	if (ipc_module->ref_count == 0) {
		gate_leave_system(key);
		if (unlikely(atomic_cmpmask_and_lt(
					&(ipc_module->start_ref_count),
					IPC_MAKE_MAGICSTAMP(0),
					IPC_MAKE_MAGICSTAMP(1)) == false)) {
			/*
			 * ipc_start was called, but ipc_stop never happened.
			 * Need to call ipc_stop here.
			 */
			/* Set the count to 1 so only need to call stop once. */
			atomic_set(&ipc_module->start_ref_count,
					IPC_MAKE_MAGICSTAMP(1));
			ipc_stop();
		}
		status = platform_destroy();
		if (status < 0) {
			status = IPC_E_FAIL;
			goto exit;
		}
	} else
		gate_leave_system(key);

exit:
	if (status < 0)
		pr_err("ipc_destroy failed: status [0x%x]\n", status);

	return status;
}


/*
 * ====== ipc_create =======
 *  Creates a IPC.
 */
int ipc_create(u16 remote_proc_id, struct ipc_params *params)
{
	ipc_module->proc_entry[remote_proc_id].entry.setup_messageq =
							params->setup_messageq;
	ipc_module->proc_entry[remote_proc_id].entry.setup_notify =
							params->setup_notify;
	ipc_module->proc_entry[remote_proc_id].entry.setup_ipu_pm =
							params->setup_ipu_pm;
	ipc_module->proc_entry[remote_proc_id].entry.remote_proc_id =
							remote_proc_id;

	/* Assert that the proc_sync is same as configured for the module. */
	BUG_ON(ipc_module->proc_sync != params->proc_sync);

	return IPC_S_SUCCESS;
}
