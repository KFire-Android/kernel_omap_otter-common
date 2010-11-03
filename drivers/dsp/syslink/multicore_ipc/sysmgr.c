/*
 *  sysmgr.c
 *
 *  Implementation of System manager.
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
#include <sysmemmgr.h>
#include <sysmgr.h>
#include <_sysmgr.h>
#include <platform.h>
#include <platform_mem.h>

#include <gatepeterson.h>
#include <sharedregion.h>
#include <listmp.h>
#include <messageq.h>
#include <messageq_transportshm.h>
#include <notify.h>
/*#include <notify_driver.h>*/
#include <notify_ducatidriver.h>

#include <nameserver.h>
#include <nameserver_remote.h>
#include <nameserver_remotenotify.h>
#include <procmgr.h>
#include <heap.h>
#include <heapbuf.h>

/* =============================================================================
 * Macros
 * =============================================================================
 */
/*!
 *  @def    BOOTLOADPAGESIZE
 *  @brief  Error code base for System manager.
 */
#define BOOTLOADPAGESIZE			(0x1000) /* 4K page size */

/*!
 *  @def    SYSMGR_ENTRYVALIDITYSTAMP
 *  @brief  Validity stamp for boot load page entries.
 */
#define SYSMGR_ENTRYVALIDITYSTAMP		(0xBABAC0C0)

/*!
 *  @def    SYSMGR_ENTRYVALIDSTAMP
 *  @brief  Validity stamp for boot load page entries.
 */
#define SYSMGR_ENTRYVALIDSTAMP			(0xBABAC0C0)

/*!
 *  @def    SYSMGR_SCALABILITYHANDSHAKESTAMP
 *  @brief  scalability configuration handshake value.
 */
#define SYSMGR_SCALABILITYHANDSHAKESTAMP	(0xBEEF0000)

/*!
 *  @def    SYSMGR_SETUPHANDSHAKESTAMP
 *  @brief  Platform configured handshake value.
 */
#define SYSMGR_SETUPHANDSHAKESTAMP		(0xBEEF0001)

/*!
 *  @def    SYSMGR_DESTROYHANDSHAKESTAMP
 *  @brief  Destroy handshake value.
 */
#define SYSMGR_DESTROYHANDSHAKESTAMP		(0xBEEF0002)

/*!
 *  @def    SYSMGR_BOOTLOADPAGESIZE
 *  @brief  Boot load page size.
 */
#define SYSMGR_BOOTLOADPAGESIZE			(0x00001000)

/* Macro to make a correct module magic number with ref_count */
#define SYSMGR_MAKE_MAGICSTAMP(x)		((SYSMGR_MODULEID << 12) | (x))


/* =============================================================================
 * Structures & Enums
 * =============================================================================
 */
/*! @brief structure for System manager boot load page entry */
struct sysmgr_bootload_page_entry {
	VOLATILE u32 offset;
	/* Offset of next entry (-1 if not present) */
	VOLATILE u32 valid;
	/* Validity of the entry */
	VOLATILE u32 size;
	/* Size of the entry data */
	VOLATILE u32 cmd_id;
	/* Command ID */
};

/*! @brief structure containg system manager state object */
struct sysmgr_boot_load_page {
	VOLATILE struct sysmgr_bootload_page_entry host_config;
	/* First entry, host specific configuration in the boot load page */
	u8 padding1[(BOOTLOADPAGESIZE/2) - \
		sizeof(struct sysmgr_bootload_page_entry)];
	/* Padding1 */
	VOLATILE u32 handshake;
	/* Handshake variable, wrote by slave to indicate configuration done. */
	VOLATILE struct sysmgr_bootload_page_entry slave_config;
	/* First entry, slave specific configuration in the boot load page */
	u8 padding2[(BOOTLOADPAGESIZE/2) - \
			sizeof(struct sysmgr_bootload_page_entry) - \
			sizeof(u32)];
	/* Padding2 */
};

/*! @brief structure for System manager module state */
struct sysmgr_module_object {
	atomic_t ref_count;
	/* Reference count */
	struct sysmgr_config config;
	/* Overall system configuration */
	struct sysmgr_boot_load_page *boot_load_page[MULTIPROC_MAXPROCESSORS];
	/* Boot load page of the slaves */
	bool platform_mem_init_flag;
	/* Platform memory manager initialize flag */
	bool multiproc_init_flag;
	/* Multiproc Initialize flag */
	bool gatepeterson_init_flag;
	/* Gatepeterson Initialize flag */
	bool sharedregion_init_flag;
	/* Sharedregion Initialize flag */
	bool listmp_init_flag;
	/* Listmp Initialize flag */
	bool messageq_init_flag;
	/* Messageq Initialize flag */
	bool notify_init_flag;
	/* Notify Initialize flag */
	bool proc_mgr_init_flag;
	/* Processor manager Initialize flag */
	bool heapbuf_init_flag;
	/* Heapbuf Initialize flag */
	bool nameserver_init_flag;
	/* Nameserver_remotenotify Initialize flag */
	bool listmp_sharedmemory_init_flag;
	/* Listmp_sharedmemory Initialize flag */
	bool messageq_transportshm_init_flag;
	/* Messageq_transportshm Initialize flag */
	bool notify_ducatidrv_init_flag;
	/* notify_ducatidrv Initialize flag */
	bool nameserver_remotenotify_init_flag;
	/* nameserver_remotenotify Initialize flag */
	bool platform_init_flag;
	/* Flag to indicate platform initialization status */
};


/* =============================================================================
 * Globals
 * =============================================================================
 */
/*!
 *  @var    sysmgr_state
 *
 *  @brief  Variable holding state of system manager.
 */
static struct sysmgr_module_object sysmgr_state;


/* =============================================================================
 * APIS
 * =============================================================================
 */
/*
 * ======== sysmgr_get_config ========
 *  Purpose:
 *  Function to get the default values for configuration.
 */
void sysmgr_get_config(struct sysmgr_config *config)
{
	s32 status = 0;

	if (WARN_ON(config == NULL)) {
		status = -EINVAL;
		pr_err("sysmgr_get_config [0x%x] : Argument of type"
				" (sysmgr_get_config *) passed is null!",
				status);
		return;
	}

	/* Get the gatepeterson default config */
	multiproc_get_config(&config->multiproc_cfg);

	/* Get the gatepeterson default config */
	gatepeterson_get_config(&config->gatepeterson_cfg);

	/* Get the sharedregion default config */
	sharedregion_get_config(&config->sharedregion_cfg);

	/* Get the messageq default config */
	messageq_get_config(&config->messageq_cfg);

	/* Get the notify default config */
	notify_get_config(&config->notify_cfg);

	/* Get the proc_mgr default config */
	proc_mgr_get_config(&config->proc_mgr_cfg);

	/* Get the heapbuf default config */
	heapbuf_get_config(&config->heapbuf_cfg);

	/* Get the listmp_sharedmemory default config */
	listmp_sharedmemory_get_config(&config->listmp_sharedmemory_cfg);

	/* Get the messageq_transportshm default config */
	messageq_transportshm_get_config(&config->messageq_transportshm_cfg);

	/* Get the notify_ducati driver default config */
	notify_ducatidrv_getconfig(&config->notify_ducatidrv_cfg);

	/* Get the nameserver_remotenotify default config */
	nameserver_remotenotify_get_config(
		&config->nameserver_remotenotify_cfg);
}
EXPORT_SYMBOL(sysmgr_get_config);

/*
 * ======== sysmgr_get_object_config ========
 *  Purpose:
 *  Function to get the SysMgr Object configuration from Slave.
 */
u32 sysmgr_get_object_config(u16 proc_id, void *config, u32 cmd_id, u32 size)
{
	struct sysmgr_bootload_page_entry *entry = NULL;
	u32 offset = 0;
	u32 ret = 0;
	struct sysmgr_boot_load_page *blp = NULL;

	if ((proc_id < 0) || (proc_id >= MULTIPROC_MAXPROCESSORS)) {
		ret = 0;
		goto exit;
	}

	blp = (struct sysmgr_boot_load_page *)
			sysmgr_state.boot_load_page[proc_id];

	entry = (struct sysmgr_bootload_page_entry *) &blp->slave_config;
	while (entry->valid == SYSMGR_ENTRYVALIDSTAMP) {
		if (entry->cmd_id == cmd_id) {
			if (size == entry->size) {
				memcpy(config, (void *)((u32)entry + \
				sizeof(struct sysmgr_bootload_page_entry)),
				size);
				ret = size;
				break;
			}
		}
		if (entry->offset != -1) {
			offset += entry->offset;
			entry = (struct sysmgr_bootload_page_entry *)
				((u32) &blp->slave_config + entry->offset);
		} else {
			break;
		}
	}

exit:
	/* return number of bytes wrote to the boot load page */
	return ret;
}


/*
 * ======== sysmgr_put_object_config ========
 *  Purpose:
 *  Function to put the SysMgr Object configuration to Slave.
 */
u32 sysmgr_put_object_config(u16 proc_id, void *config, u32 cmd_id, u32 size)
{
	struct sysmgr_bootload_page_entry *entry = NULL;
	struct sysmgr_bootload_page_entry *prev = NULL;
	u32 offset = 0;
	struct sysmgr_boot_load_page *blp = NULL;

	if ((proc_id < 0) || (proc_id >= MULTIPROC_MAXPROCESSORS)) {
		size = 0;
		goto exit;
	}

	/* Get the boot load page pointer */
	blp = sysmgr_state.boot_load_page[proc_id];

	/* Put the entry at the end of list */
	entry = (struct sysmgr_bootload_page_entry *) &blp->host_config;
	while (entry->valid == SYSMGR_ENTRYVALIDSTAMP) {
		prev  = entry;
		if (entry->offset != -1) {
			offset += entry->offset;
			entry = (struct sysmgr_bootload_page_entry *)
				((u32) &blp->host_config + entry->offset);
		} else {
			break;
		}
	}

	/* First entry has prev set to NULL */
	if (prev == NULL) {
		entry->offset = -1;
		entry->cmd_id  = cmd_id;
		entry->size   = size;
		memcpy((void *)((u32)entry + \
			sizeof(struct sysmgr_bootload_page_entry)),
			config, size);
		entry->valid  = SYSMGR_ENTRYVALIDSTAMP;
	} else {
		entry = (struct sysmgr_bootload_page_entry *)((u32)entry + \
				sizeof(struct sysmgr_bootload_page_entry) + \
				entry->size);
		entry->offset = -1;
		entry->cmd_id = cmd_id;
		entry->size = size;
		memcpy((void *)((u32)entry + \
			sizeof(struct sysmgr_bootload_page_entry)),
			config, size);
		entry->valid = SYSMGR_ENTRYVALIDSTAMP;

		/* Attach the new created entry */
		prev->offset = ((u32) entry - (u32) &blp->host_config);
	}

exit:
	/* return number of bytes wrote to the boot load page */
	return size;
}


/*
 * ======== sysmgr_setup ========
 *  Purpose:
 *  Function to setup the System.
 */
s32 sysmgr_setup(const struct sysmgr_config *cfg)
{
	s32 status = 0;
	struct sysmgr_config *config = NULL;

	/* This sets the ref_count variable is not initialized, upper 16 bits is
	 * written with module Id to ensure correctness of ref_count variable.
	 */
	atomic_cmpmask_and_set(&sysmgr_state.ref_count,
				SYSMGR_MAKE_MAGICSTAMP(0),
				SYSMGR_MAKE_MAGICSTAMP(0));

	if (atomic_inc_return(&sysmgr_state.ref_count)
			!= SYSMGR_MAKE_MAGICSTAMP(1)) {
		status = 1;
		goto exit;
	}

	if (cfg == NULL) {
		sysmgr_get_config(&sysmgr_state.config);
		config = &sysmgr_state.config;
	} else {
		memcpy((void *) (&sysmgr_state.config), (void *) cfg,
				sizeof(struct sysmgr_config));
		config = (struct sysmgr_config *) cfg;
	}

	/* Initialize PlatformMem */
	status = platform_mem_setup();
	if (status < 0) {
		pr_err("sysmgr_setup : platform_mem_setup "
			"failed [0x%x]\n", status);
	} else {
		pr_err("platform_mem_setup : status [0x%x]\n" ,
			status);
		sysmgr_state.platform_mem_init_flag = true;
	}

	/* Override the platform specific configuration */
	platform_override_config(config);

	status = multiproc_setup(&(config->multiproc_cfg));
	if (status < 0) {
		pr_err("sysmgr_setup : multiproc_setup "
			"failed [0x%x]\n", status);
	} else {
		pr_err("sysmgr_setup : status [0x%x]\n" , status);
		sysmgr_state.multiproc_init_flag = true;
	}

	/* Initialize ProcMgr */
	if (status >= 0) {
		status = proc_mgr_setup(&(config->proc_mgr_cfg));
		if (status < 0) {
			pr_err("sysmgr_setup : proc_mgr_setup "
				"failed [0x%x]\n", status);
		} else {
			pr_err("proc_mgr_setup : status [0x%x]\n" ,
				status);
			sysmgr_state.proc_mgr_init_flag = true;
		}
	}

	/* Initialize SharedRegion */
	if (status >= 0) {
		status = sharedregion_setup(&config->sharedregion_cfg);
		if (status < 0) {
			pr_err("sysmgr_setup : sharedregion_setup "
				"failed [0x%x]\n", status);
		} else {
			pr_err("sharedregion_setup : status [0x%x]\n" ,
				status);
			sysmgr_state.sharedregion_init_flag = true;
		}
	}

	/* Initialize Notify */
	if (status >= 0) {
		status = notify_setup(&config->notify_cfg);
		if (status < 0) {
			pr_err("sysmgr_setup : notify_setup "
				"failed [0x%x]\n", status);
		} else {
			pr_err("notify_setup : status [0x%x]\n" ,
				status);
			sysmgr_state.notify_init_flag = true;
		}
	}

	/* Initialize NameServer */
	if (status >= 0) {
		status = nameserver_setup();
		if (status < 0) {
			pr_err("sysmgr_setup : nameserver_setup "
				"failed [0x%x]\n", status);
		} else {
			pr_err("nameserver_setup : status [0x%x]\n" ,
				status);
			sysmgr_state.nameserver_init_flag = true;
		}
	}

	/* Initialize GatePeterson */
	if (status >= 0) {
		status = gatepeterson_setup(&config->gatepeterson_cfg);
		if (status < 0) {
			pr_err("sysmgr_setup : gatepeterson_setup "
				"failed [0x%x]\n", status);
		} else {
			pr_err("gatepeterson_setup : status [0x%x]\n" ,
				status);
			sysmgr_state.gatepeterson_init_flag = true;
		}
	}

	/* Intialize MessageQ */
	if (status >= 0) {
		status = messageq_setup(&config->messageq_cfg);
		if (status < 0) {
			pr_err("sysmgr_setup : messageq_setup "
				"failed [0x%x]\n", status);
		} else {
			pr_err("messageq_setup : status [0x%x]\n" ,
				status);
			sysmgr_state.messageq_init_flag = true;
		}
	}

	/* Intialize HeapBuf */
	if (status >= 0) {
		status = heapbuf_setup(&config->heapbuf_cfg);
		if (status < 0) {
			pr_err("sysmgr_setup : heapbuf_setup "
				"failed [0x%x]\n", status);
		} else {
			pr_err("heapbuf_setup : status [0x%x]\n" ,
				status);
			sysmgr_state.heapbuf_init_flag = true;
		}
	}

	/* Initialize ListMPSharedMemory */
	if (status >= 0) {
		status = listmp_sharedmemory_setup(
				&config->listmp_sharedmemory_cfg);
		if (status < 0) {
			pr_err("sysmgr_setup : "
				"listmp_sharedmemory_setup failed [0x%x]\n",
				status);
		} else {
			pr_err("listmp_sharedmemory_setup : "
				"status [0x%x]\n" , status);
			sysmgr_state.listmp_sharedmemory_init_flag = true;
		}
	}

	/* Initialize MessageQTransportShm */
	if (status >= 0) {
		status = messageq_transportshm_setup(
				 &config->messageq_transportshm_cfg);
		if (status < 0) {
			pr_err("sysmgr_setup : "
				"messageq_transportshm_setup failed [0x%x]\n",
				status);
		} else {
			pr_err("messageq_transportshm_setup : "
				"status [0x%x]\n", status);
			sysmgr_state.messageq_transportshm_init_flag = true;
		}
	}

	/* Initialize Notify DucatiDriver */
	if (status >= 0) {
		status = notify_ducatidrv_setup(&config->notify_ducatidrv_cfg);
		if (status < 0) {
			pr_err("sysmgr_setup : "
				"notify_ducatidrv_setup failed [0x%x]\n",
				status);
		} else {
			pr_err("notify_ducatidrv_setup : "
				"status [0x%x]\n" , status);
			sysmgr_state.notify_ducatidrv_init_flag = true;
		}
	}

	/* Initialize NameServerRemoteNotify */
	if (status >= 0) {
		status = nameserver_remotenotify_setup(
				 &config->nameserver_remotenotify_cfg);
		if (status < 0) {
			pr_err("sysmgr_setup : "
				"nameserver_remotenotify_setup failed [0x%x]\n",
				status);
		} else {
			pr_err("nameserver_remotenotify_setup : "
				"status [0x%x]\n" , status);
			sysmgr_state.nameserver_remotenotify_init_flag = true;
		}
	}

	if (status >= 0) {
		/* Call platform setup function */
		status = platform_setup(config);
		if (status < 0) {
			pr_err("sysmgr_setup : platform_setup "
				"failed [0x%x]\n", status);
		} else {
			pr_err("platform_setup : status [0x%x]\n" ,
				status);
			sysmgr_state.platform_init_flag = true;
		}
	}

exit:
	if (status < 0)
		atomic_set(&sysmgr_state.ref_count, SYSMGR_MAKE_MAGICSTAMP(0));

	return status;
}
EXPORT_SYMBOL(sysmgr_setup);

/*
 * ======== sysmgr_setup ========
 *  Purpose:
 *  Function to finalize the System.
 */
s32 sysmgr_destroy(void)
{
	s32 status = 0;

	if (atomic_cmpmask_and_lt(&(sysmgr_state.ref_count),
				SYSMGR_MAKE_MAGICSTAMP(0),
				SYSMGR_MAKE_MAGICSTAMP(1)) != false) {
		/*! @retval SYSMGR_E_INVALIDSTATE Module was not initialized */
		status = SYSMGR_E_INVALIDSTATE;
		goto exit;
	}

	if (atomic_dec_return(&sysmgr_state.ref_count)
			!= SYSMGR_MAKE_MAGICSTAMP(0)) {
		status = 1;
		goto exit;
	}

	/* Finalize Platform module*/
	if (sysmgr_state.platform_init_flag == true) {
		status = platform_destroy();
		if (status < 0) {
			pr_err("sysmgr_destroy : platform_destroy "
				"failed [0x%x]\n", status);
		} else {
			sysmgr_state.platform_init_flag = false;
		}
	}

	/* Finalize NameServerRemoteNotify module */
	if (sysmgr_state.nameserver_remotenotify_init_flag == true) {
		status = nameserver_remotenotify_destroy();
		if (status < 0) {
			pr_err("sysmgr_destroy : "
				"nameserver_remotenotify_destroy "
				"failed [0x%x]\n", status);
		} else {
			sysmgr_state.nameserver_remotenotify_init_flag \
				= false;
		}
	}

	/* Finalize Notify Ducati Driver module */
	if (sysmgr_state.notify_ducatidrv_init_flag == true) {
		status = notify_ducatidrv_destroy();
		if (status < 0) {
			pr_err("sysmgr_destroy : "
				"notify_ducatidrv_destroy failed [0x%x]\n",
				status);
		} else {
			sysmgr_state.notify_ducatidrv_init_flag = false;
		}
	}

	/* Finalize MessageQTransportShm module */
	if (sysmgr_state.messageq_transportshm_init_flag == true) {
		status = messageq_transportshm_destroy();
		if (status < 0) {
			pr_err("sysmgr_destroy : "
				"messageq_transportshm_destroy failed [0x%x]\n",
				status);
		} else {
			sysmgr_state.messageq_transportshm_init_flag = \
				false;
		}
	}

	/* Finalize ListMPSharedMemory module */
	if (sysmgr_state.listmp_sharedmemory_init_flag == true) {
		status = listmp_sharedmemory_destroy();
		if (status < 0) {
			pr_err("sysmgr_destroy : "
				"listmp_sharedmemory_destroy failed [0x%x]\n",
				status);
		} else {
			sysmgr_state.listmp_sharedmemory_init_flag = \
				false;
		}
	}

	/* Finalize HeapBuf module */
	if (sysmgr_state.heapbuf_init_flag == true) {
		status = heapbuf_destroy();
		if (status < 0) {
			pr_err("sysmgr_destroy : heapbuf_destroy "
				"failed [0x%x]\n", status);
		} else {
			sysmgr_state.heapbuf_init_flag = false;
		}
	}

	/* Finalize MessageQ module */
	if (sysmgr_state.messageq_init_flag == true) {
		status = messageq_destroy();
		if (status < 0) {
			pr_err("sysmgr_destroy : messageq_destroy "
				"failed [0x%x]\n", status);
		} else {
			sysmgr_state.messageq_init_flag = false;
		}
	}

	/* Finalize GatePeterson module */
	if (sysmgr_state.gatepeterson_init_flag == true) {
		status = gatepeterson_destroy();
		if (status < 0) {
			pr_err("sysmgr_destroy : "
				"gatepeterson_destroy failed [0x%x]\n", status);
		} else {
			sysmgr_state.gatepeterson_init_flag = false;
		}
	}

	/* Finalize NameServer module */
	if (sysmgr_state.nameserver_init_flag == true) {
		status = nameserver_destroy();
		if (status < 0) {
			pr_err("sysmgr_destroy : nameserver_destroy "
				"failed [0x%x]\n", status);
		} else {
			sysmgr_state.nameserver_init_flag = false;
		}
	}

	/* Finalize Notify module */
	if (sysmgr_state.notify_init_flag == true) {
		status = notify_destroy();
		if (status < 0) {
			pr_err("sysmgr_destroy : sysmgr_destroy "
				"failed [0x%x]\n", status);
		} else {
			sysmgr_state.notify_init_flag = false;
		}
	}

	/* Finalize SharedRegion module */
	if (sysmgr_state.sharedregion_init_flag == true) {
		status = sharedregion_destroy();
		if (status < 0) {
			pr_err("sysmgr_destroy : "
				"sharedregion_destroy failed [0x%x]\n", status);
		} else {
			sysmgr_state.sharedregion_init_flag = false;
		}
	}

	/* Finalize ProcMgr module */
	if (sysmgr_state.proc_mgr_init_flag == true) {
		status = proc_mgr_destroy();
		if (status < 0) {
			pr_err("sysmgr_destroy : proc_mgr_destroy "
				"failed [0x%x]\n", status);
		} else {
			sysmgr_state.proc_mgr_init_flag = false;
		}
	}

	/* Finalize MultiProc module */
	if (sysmgr_state.multiproc_init_flag == true) {
		status = multiproc_destroy();
		if (status < 0) {
			pr_err("sysmgr_destroy : multiproc_destroy "
				"failed [0x%x]\n", status);
		} else {
			sysmgr_state.proc_mgr_init_flag = false;
		}
	}

	/* Finalize PlatformMem module */
	if (sysmgr_state.platform_mem_init_flag == true) {
		status = platform_mem_destroy();
		if (status < 0) {
			pr_err("sysmgr_destroy : platform_mem_destroy "
				"failed [0x%x]\n", status);
		} else {
			sysmgr_state.platform_mem_init_flag = false;
		}
	}

	atomic_set(&sysmgr_state.ref_count, SYSMGR_MAKE_MAGICSTAMP(0));

exit:
	if (status < 0) {
		pr_err("sysmgr_destroy : failed with "
			"status = [0x%x]\n", status);
	}
	return status;
}
EXPORT_SYMBOL(sysmgr_destroy);

/*
 * ======== sysmgr_set_boot_load_page ========
 *  Purpose:
 *  Function to set the boot load page address for a slave.
 */
void sysmgr_set_boot_load_page(u16 proc_id, u32 boot_load_page)
{
	struct sysmgr_boot_load_page *temp = \
		(struct sysmgr_boot_load_page *) boot_load_page;

	if ((proc_id < 0) || (proc_id >= MULTIPROC_MAXPROCESSORS)) {
		pr_err(
		"sysmgr_set_boot_load_page failed: Invalid proc_id passed\n");
		return;
	}

	/* Initialize the host config area */
	sysmgr_state.boot_load_page[proc_id] = temp;
	temp->host_config.offset = -1;
	temp->host_config.valid = 0;
	temp->handshake = 0;
}


/*
 * ======== sysmgr_wait_for_scalability_info ========
 *  Purpose:
 *  Function to wait for scalability handshake value.
 */
void sysmgr_wait_for_scalability_info(u16 proc_id)
{
	VOLATILE struct sysmgr_boot_load_page *temp = NULL;

	if ((proc_id < 0) || (proc_id >= MULTIPROC_MAXPROCESSORS)) {
		pr_err("sysmgr_wait_for_scalability_info failed: "
			"Invalid proc_id passed\n");
		return;
	}
	temp = sysmgr_state.boot_load_page[proc_id];

	pr_err("sysmgr_wait_for_scalability_info: BF while temp->handshake:%x\n",
		temp->handshake);
	while (temp->handshake != SYSMGR_SCALABILITYHANDSHAKESTAMP)
		;
	pr_err("sysmgr_wait_for_scalability_info:AF while temp->handshake:%x\n",
		temp->handshake);

	/* Reset the handshake value for reverse synchronization */
	temp->handshake = 0;
}


/*
 * ======== sysmgr_wait_for_slave_setup ========
 *  Purpose:
 *  Function to wait for slave to complete setup.
 */
void sysmgr_wait_for_slave_setup(u16 proc_id)
{
	VOLATILE struct sysmgr_boot_load_page *temp = NULL;

	if ((proc_id < 0) || (proc_id >= MULTIPROC_MAXPROCESSORS)) {
		pr_err("sysmgr_wait_for_slave_setup failed: "
			"Invalid proc_id passed\n");
		return;
	}
	temp = sysmgr_state.boot_load_page[proc_id];

	while (temp->handshake != SYSMGR_SETUPHANDSHAKESTAMP)
		;

	/* Reset the handshake value for reverse synchronization */
	temp->handshake = 0;
}
