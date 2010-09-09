/*
 *  platform.c
 *
 *  Implementation of platform initialization logic for Syslink IPC.
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


/* Standard header files */
#include <linux/types.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/slab.h>

/* SysLink device specific headers */
#include "../procmgr/proc4430/proc4430.h"

/* Ipu Power Management Header (ipu_pm) */
#include "../ipu_pm/ipu_pm.h"
/* Module level headers */
#include <multiproc.h>
#include <platform.h>
#include <gatemp.h>
#include <gatepeterson.h>
#include <gatehwspinlock.h>
#include <sharedregion.h>
#include <listmp.h>
#include <_listmp.h>
#include <heap.h>
#include <heapbufmp.h>
#include <heapmemmp.h>
#include <messageq.h>
#include <transportshm.h>
#include <notify.h>
#include <ipc.h>

#include <notify_ducatidriver.h>
#include <nameserver.h>
#include <nameserver_remote.h>
#include <nameserver_remotenotify.h>
#include <procmgr.h>

#include <platform_mem.h>


/** ============================================================================
 *  Macros.
 *  ============================================================================
 */
#define RESETVECTOR_SYMBOL          "_Ipc_ResetVector"

/** ============================================================================
 *  Application specific configuration, please change these value according to
 *  your application's need.
 *  ============================================================================
 */
/*! @brief Start of IPC shared memory */
#define SHAREDMEMORY_PHY_BASEADDR	CONFIG_DUCATI_BASEIMAGE_PHYS_ADDR
#define SHAREDMEMORY_PHY_BASESIZE	0x00100000

/*! @brief Start of IPC shared memory for SysM3 */
#define SHAREDMEMORY_PHY_BASEADDR_SR0	SHAREDMEMORY_PHY_BASEADDR
#define SHAREDMEMORY_PHY_BASESIZE_SR0	0x00054000

/*! @brief Start of IPC shared memory AppM3 */
#define SHAREDMEMORY_PHY_BASEADDR_SR1	(SHAREDMEMORY_PHY_BASEADDR + 0x54000)
#define SHAREDMEMORY_PHY_BASESIZE_SR1	0x000AC000

/*! @brief Start of IPC SHM for SysM3 */
#define SHAREDMEMORY_SLV_VRT_BASEADDR_SR0	0xA0000000
#define SHAREDMEMORY_SLV_VRT_DSP_BASEADDR_SR0	0x30000000
#define SHAREDMEMORY_SLV_VRT_BASESIZE_SR0	0x00054000

/*! @brief Start of IPC SHM for AppM3 */
#define SHAREDMEMORY_SLV_VRT_BASEADDR_SR1	0xA0054000
#define SHAREDMEMORY_SLV_VRT_DSP_BASEADDR_SR1	0x30054000
#define SHAREDMEMORY_SLV_VRT_BASESIZE_SR1	0x000AC000

/*! @brief Start of Code memory for SysM3 */
#define SHAREDMEMORY_SLV_VRT_CODE0_BASEADDR	0x00000000
#define SHAREDMEMORY_SLV_VRT_CODE0_BASESIZE	0x00100000

/*! @brief Start of Code section for SysM3 */
#define SHAREDMEMORY_PHY_CODE0_BASEADDR (SHAREDMEMORY_PHY_BASEADDR + 0x100000)
#define SHAREDMEMORY_PHY_CODE0_BASESIZE		0x00100000

/*! @brief Start of Code memory for SysM3 */
#define SHAREDMEMORY_SLV_VRT_CODE1_BASEADDR	0x00100000
#define SHAREDMEMORY_SLV_VRT_CODE1_BASESIZE	0x00300000

/*! @brief Start of Code section for SysM3 */
#define SHAREDMEMORY_PHY_CODE1_BASEADDR (SHAREDMEMORY_PHY_CODE0_BASEADDR + \
					SHAREDMEMORY_SLV_VRT_CODE1_BASEADDR)
#define SHAREDMEMORY_PHY_CODE1_BASESIZE		0x00300000

/*! @brief Start of Const section for SysM3 */
#define SHAREDMEMORY_PHY_CONST0_BASEADDR (SHAREDMEMORY_PHY_CODE0_BASEADDR + \
						0x1000000)
#define SHAREDMEMORY_PHY_CONST0_BASESIZE	0x00040000

/*! @brief Start of Const section for SysM3 */
#define SHAREDMEMORY_SLV_VRT_CONST0_BASEADDR	0x80000000
#define SHAREDMEMORY_SLV_VRT_CONST0_BASESIZE	0x00040000

/*! @brief Start of Const section for AppM3 */
#define SHAREDMEMORY_PHY_CONST1_BASEADDR (SHAREDMEMORY_PHY_CONST0_BASEADDR + \
						0x100000)
#define SHAREDMEMORY_PHY_CONST1_BASESIZE	0x00180000

/*! @brief Start of Const section for AppM3 */
#define SHAREDMEMORY_SLV_VRT_CONST1_BASEADDR	0x80100000
#define SHAREDMEMORY_SLV_VRT_CONST1_BASESIZE	0x00180000

/*! @brief Start of EXT_RAM section for Tesla */
#define SHAREDMEMORY_PHY_EXT_RAM_BASEADDR	\
				(CONFIG_DUCATI_BASEIMAGE_PHYS_ADDR - 0x2F00000)
#define SHAREDMEMORY_PHY_EXT_RAM_BASESIZE	0x02000000

/*! @brief Start of Code memory for Tesla */
#define SHAREDMEMORY_SLV_VRT_CODE_DSP_BASEADDR	0x20000000
#define SHAREDMEMORY_SLV_VRT_CODE_DSP_BASESIZE	0x00080000

/*! @brief Start of Code section for Tesla */
#define SHAREDMEMORY_PHY_CODE_DSP_BASEADDR	\
					(SHAREDMEMORY_PHY_BASEADDR - 0x300000)
#define SHAREDMEMORY_PHY_CODE_DSP_BASESIZE	0x00080000

/*! @brief Start of Const section for Tesla */
#define SHAREDMEMORY_SLV_VRT_CONST_DSP_BASEADDR	0x20080000
#define SHAREDMEMORY_SLV_VRT_CONST_DSP_BASESIZE	0x00080000

/*! @brief Start of Const section for Tesla */
#define SHAREDMEMORY_PHY_CONST_DSP_BASEADDR	\
				(SHAREDMEMORY_PHY_CODE_DSP_BASEADDR + \
					SHAREDMEMORY_PHY_CODE_DSP_BASESIZE)
#define SHAREDMEMORY_PHY_CONST_DSP_BASESIZE	0x00080000

/*! @brief Start of EXT_RAM section for Tesla */
#define SHAREDMEMORY_SLV_VRT_EXT_RAM_BASEADDR	0x20000000
#define SHAREDMEMORY_SLV_VRT_EXT_RAM_BASESIZE	0x02000000

#define USE_NEW_PROCMGR		0

/** ============================================================================
 *  Struct & Enums.
 *  ============================================================================
 */

/* Struct for reading platform specific gate peterson configuration values */
struct platform_gaterpeterson_params {
	u32 shared_mem_addr;	/* Shared memory address */
	u32 shared_mem_size;	/* Shared memory size */
	u32 remote_proc_id;	/* Remote processor identifier */
};

struct platform_notify_ducatidrv_params {
	u32 shared_mem_addr;	/* Shared memory address */
	u32 shared_mem_size;	/* Shared memory size */
	u16 remote_proc_id;	/* Remote processor identifier */
};

struct platform_nameserver_remotenotify_params {
	u32 shared_mem_addr;	/* Shared memory address */
	u32 shared_mem_size;	/* Shared memory size */
	u32 notify_event_no;	/* Notify Event number to used */
};

struct platform_heapbuf_params {
	u32 shared_mem_addr;	/* Shared memory address */
	u32 shared_mem_size;	/* Shared memory size */
	u32 shared_buf_addr;	/* Shared memory address */
	u32 shared_buf_size;	/* Shared memory size */
	u32 num_blocks;
	u32 block_size;
};

struct platform_transportshm_params {
	u32 shared_mem_addr;	/* Shared memory address */
	u32 shared_mem_size;	/* Shared memory size */
	u32 notify_event_no;	/* Notify Event number */
};

/** ============================================================================
 *  Application specific configuration, please change these value according to
 *  your application's need.
 *  ============================================================================
 */
/*
 * Structure defining config parameters for overall System.
 */
struct platform_config {
	struct multiproc_config			multiproc_config;
	/* multiproc_config parameter */

	struct gatemp_config			gatemp_config;
	/* gatemp_config parameter */

	struct gatepeterson_config		gatepeterson_config;
	/* gatepeterson_config parameter */

	struct gatehwspinlock_config		gatehwspinlock_config;
	/* gatehwspinlock parameter */

	struct sharedregion_config		sharedregion_config;
	/* sharedregion_config parameter */

	struct messageq_config			messageq_config;
	/* messageq_config parameter */

	struct notify_config			notify_config;
	/* notify config parameter */
	struct ipu_pm_config			ipu_pm_config;
	/* ipu_pm config parameter */

	struct proc_mgr_config			proc_mgr_config;
	/* processor manager config parameter */

	struct heapbufmp_config			heapbufmp_config;
	/* heapbufmp_config parameter */

	struct heapmemmp_config			heapmemmp_config;
	/* heapmemmp_config parameter */
#if 0

	struct heapmultibuf_config		heapmultibuf_config;
	/* heapmultibuf_config parameter */
#endif
	struct listmp_config			listmp_config;
	/* listmp_config parameter */

	struct transportshm_config		transportshm_config;
	/* transportshm_config parameter */
#if 0
	struct ringio_config			ringio_config;
	/* ringio_config parameter */

	struct ringiotransportshm_config	ringiotransportshm_config;
	/* ringiotransportshm_config parameter */
#endif
	struct notify_ducatidrv_config		notify_ducatidrv_config;
	/* notify_ducatidrv_config parameter */

	struct nameserver_remotenotify_config	nameserver_remotenotify_config;
	/* nameserver_remotenotify_config parameter */
#if 0
	struct clientnotifymgr_config		clinotifymgr_config_params;
	/* clientnotifymgr_config parameter */

	struct frameqbufmgr_config		frameqbufmgr_config_params;
	/* frameqbufmgr_config parameter */

	struct frameq_config			frameq_config_params;
	/* frameq_config parameter */
#endif
};


/* struct embedded into slave binary */
struct platform_slave_config {
	u32 cache_line_size;
	u32 br_offset;
	u32 sr0_memory_setup;
	u32 setup_messageq;
	u32 setup_notify;
	u32 setup_ipu_pm;
	u32 proc_sync;
	u32 num_srs;
};

struct platform_proc_config_params {
	u32 use_notify;
	u32 use_messageq;
	u32 use_heapbuf;
	u32 use_frameq;
	u32 use_ring_io;
	u32 use_listmp;
	u32 use_nameserver;
};

/* shared region configuration */
struct platform_slave_sr_config {
	u32 entry_base;
	u32 entry_len;
	u32 owner_proc_id;
	u32 id;
	u32 create_heap;
	u32 cache_line_size;
};

/* Shared region configuration information for host side. */
struct platform_host_sr_config {
	u16 ref_count;
};

/* structure for platform instance */
struct platform_object {
	void				*pm_handle;
	/* handle to the proc_mgr instance used */
	void				*phandle;
	/* handle to the processor instance used */
	struct platform_slave_config	slave_config;
	/* slave embedded config */
	struct platform_slave_sr_config	*slave_sr_config;
	/* shared region details from slave */
};


/* structure for platform instance */
struct platform_module_state {
	bool multiproc_init_flag;
	/* multiproc initialize flag */
	bool gatemp_init_flag;
	/* gatemp initialize flag */
	bool gatepeterson_init_flag;
	/* gatepeterson initialize flag */
	bool gatehwspinlock_init_flag;
	/* gatehwspinlock initialize flag */
	bool sharedregion_init_flag;
	/* sharedregion initialize flag */
	bool listmp_init_flag;
	/* listmp initialize flag */
	bool messageq_init_flag;
	/* messageq initialize flag */
	bool ringio_init_flag;
	/* ringio initialize flag */
	bool notify_init_flag;
	/* notify initialize flag */
	bool ipu_pm_init_flag;
	/* ipu_pm initialize flag */
	bool proc_mgr_init_flag;
	/* processor manager initialize flag */
	bool heapbufmp_init_flag;
	/* heapbufmp initialize flag */
	bool heapmemmp_init_flag;
	/* heapmemmp initialize flag */
	bool heapmultibuf_init_flag;
	/* heapbufmp initialize flag */
	bool nameserver_init_flag;
	/* nameserver initialize flag */
	bool transportshm_init_flag;
	/* transportshm initialize flag */
	bool ringiotransportshm_init_flag;
	/* ringiotransportshm initialize flag */
	bool notify_ducatidrv_init_flag;
	/* notify_ducatidrv initialize flag */
	bool nameserver_remotenotify_init_flag;
	/* nameserverremotenotify initialize flag */
	bool clientnotifymgr_init_flag;
	/* clientnotifymgr initialize flag */
	bool frameqbufmgr_init_flag;
	/* frameqbufmgr initialize flag */
	bool frameq_init_flag;
	/* frameq initialize flag */
	bool platform_init_flag;
	/* flag to indicate platform initialization status */
	bool platform_mem_init_flag;
	/* Platform memory manager initialize flag */
};


/* =============================================================================
 * GLOBALS
 * =============================================================================
 */
static struct platform_object platform_objects[MULTIPROC_MAXPROCESSORS];
static struct platform_module_state platform_module_state;
static struct platform_module_state *platform_module = &platform_module_state;
static u16 platform_num_srs_unmapped;
static struct platform_host_sr_config *platform_host_sr_config;

/* ============================================================================
 *  Forward declarations of internal functions.
 * ============================================================================
 */
static int _platform_setup(void);
static int _platform_destroy(void);

/* function to read slave memory */
static int
_platform_read_slave_memory(u16 proc_id,
			    u32 addr,
			    void *value,
			    u32 *num_bytes);

/* function to write slave memory */
static int
_platform_write_slave_memory(u16 proc_id,
			     u32 addr,
			     void *value,
			     u32 *num_bytes);


/** ============================================================================
 *  Macros and types
 *  ============================================================================
 */
/*!
 *  @brief  Number of slave memory entries for OMAP4430.
 */
#define NUM_MEM_ENTRIES			6
#define NUM_MEM_ENTRIES_DSP		4


/*!
 *  @brief  Position of reset vector memory region in the memEntries array.
 */
#define RESET_VECTOR_ENTRY_ID		0


/** ============================================================================
 *  Globals
 *  ============================================================================
 */
/*!
 *  @brief  Array of memory entries for OMAP4430
 */
static struct proc4430_mem_entry mem_entries[NUM_MEM_ENTRIES] = {
	{
		"DUCATI_CODE_SYSM3",	/* NAME	 : Name of the memory region */
		SHAREDMEMORY_PHY_CODE0_BASEADDR,
		/* PHYSADDR	 : Physical address */
		SHAREDMEMORY_SLV_VRT_CODE0_BASEADDR,
		/* SLAVEVIRTADDR  : Slave virtual address */
		(u32) -1u,
		/* MASTERVIRTADDR : Master virtual address (if known) */
		SHAREDMEMORY_SLV_VRT_CODE0_BASESIZE,
		/* SIZE		 : Size of the memory region */
		true,		/* SHARE : Shared access memory? */
	},
	{
		"DUCATI_CODE_APPM3",	/* NAME	 : Name of the memory region */
		SHAREDMEMORY_PHY_CODE1_BASEADDR,
		/* PHYSADDR : Physical address */
		SHAREDMEMORY_SLV_VRT_CODE1_BASEADDR,
		/* SLAVEVIRTADDR  : Slave virtual address */
		(u32) -1u,
		/* MASTERVIRTADDR : Master virtual address (if known) */
		SHAREDMEMORY_SLV_VRT_CODE1_BASESIZE,
		/* SIZE		 : Size of the memory region */
		true,		/* SHARE : Shared access memory? */
	},
	{
		"DUCATI_SHM_SR0",	/* NAME	 : Name of the memory region */
		SHAREDMEMORY_PHY_BASEADDR_SR0,
		/* PHYSADDR	 : Physical address */
		SHAREDMEMORY_SLV_VRT_BASEADDR_SR0,
		/* SLAVEVIRTADDR  : Slave virtual address */
		(u32) -1u,
		/* MASTERVIRTADDR : Master virtual address (if known) */
		SHAREDMEMORY_SLV_VRT_BASESIZE_SR0,
		/* SIZE		 : Size of the memory region */
		true,		/* SHARE : Shared access memory? */
	},
	{
		"DUCATI_SHM_SR1",	/* NAME	 : Name of the memory region */
		SHAREDMEMORY_PHY_BASEADDR_SR1,
		/* PHYSADDR : Physical address */
		SHAREDMEMORY_SLV_VRT_BASEADDR_SR1,
		/* SLAVEVIRTADDR  : Slave virtual address */
		(u32) -1u,
		/* MASTERVIRTADDR : Master virtual address (if known) */
		SHAREDMEMORY_SLV_VRT_BASESIZE_SR1,
		/* SIZE		 : Size of the memory region */
		true,		/* SHARE : Shared access memory? */
	},
	{
		"DUCATI_CONST_SYSM3",	/* NAME	 : Name of the memory region */
		SHAREDMEMORY_PHY_CONST0_BASEADDR,
		/* PHYSADDR	     : Physical address */
		SHAREDMEMORY_SLV_VRT_CONST0_BASEADDR,
		/* SLAVEVIRTADDR  : Slave virtual address */
		(u32) -1u,
		/* MASTERVIRTADDR : Master virtual address (if known) */
		SHAREDMEMORY_SLV_VRT_CONST0_BASESIZE,
		/* SIZE		: Size of the memory region */
		true,		/* SHARE : Shared access memory? */
	},
	{
		"DUCATI_CONST_APPM3",	/* NAME	 : Name of the memory region */
		SHAREDMEMORY_PHY_CONST1_BASEADDR,
		/* PHYSADDR	     : Physical address */
		SHAREDMEMORY_SLV_VRT_CONST1_BASEADDR,
		/* SLAVEVIRTADDR  : Slave virtual address */
		(u32) -1u,
		/* MASTERVIRTADDR : Master virtual address (if known) */
		SHAREDMEMORY_SLV_VRT_CONST1_BASESIZE,
		/* SIZE		: Size of the memory region */
		true,		/* SHARE : Shared access memory? */
	},
};


/*!
 *  @brief  Array of memory entries for OMAP4430
 */
static struct proc4430_mem_entry mem_entries_dsp[NUM_MEM_ENTRIES_DSP] = {
	{
		"TESLA_CODE_DSP",	/* NAME	 : Name of the memory region */
		SHAREDMEMORY_PHY_CODE_DSP_BASEADDR,
		/* PHYSADDR	 : Physical address */
		SHAREDMEMORY_SLV_VRT_CODE_DSP_BASEADDR,
		/* SLAVEVIRTADDR  : Slave virtual address */
		(u32) -1u,
		/* MASTERVIRTADDR : Master virtual address (if known) */
		SHAREDMEMORY_SLV_VRT_CODE_DSP_BASESIZE,
		/* SIZE		 : Size of the memory region */
		true,		/* SHARE : Shared access memory? */
	},
	{
		"TESLA_CONST_DSP",	/* NAME	 : Name of the memory region */
		SHAREDMEMORY_PHY_CONST_DSP_BASEADDR,
		/* PHYSADDR	     : Physical address */
		SHAREDMEMORY_SLV_VRT_CONST_DSP_BASEADDR,
		/* SLAVEVIRTADDR  : Slave virtual address */
		(u32) -1u,
		/* MASTERVIRTADDR : Master virtual address (if known) */
		SHAREDMEMORY_SLV_VRT_CONST_DSP_BASESIZE,
		/* SIZE		: Size of the memory region */
		true,		/* SHARE : Shared access memory? */
	},
	{
		"TESLA_SHM_SR0",	/* NAME	 : Name of the memory region */
		SHAREDMEMORY_PHY_BASEADDR_SR0,
		/* PHYSADDR	 : Physical address */
		SHAREDMEMORY_SLV_VRT_DSP_BASEADDR_SR0,
		/* SLAVEVIRTADDR  : Slave virtual address */
		(u32) -1u,
		/* MASTERVIRTADDR : Master virtual address (if known) */
		SHAREDMEMORY_SLV_VRT_BASESIZE_SR0,
		/* SIZE		 : Size of the memory region */
		true,		/* SHARE : Shared access memory? */
	},
	{
		"TESLA_SHM_SR1",	/* NAME	 : Name of the memory region */
		SHAREDMEMORY_PHY_BASEADDR_SR1,
		/* PHYSADDR : Physical address */
		SHAREDMEMORY_SLV_VRT_DSP_BASEADDR_SR1,
		/* SLAVEVIRTADDR  : Slave virtual address */
		(u32) -1u,
		/* MASTERVIRTADDR : Master virtual address (if known) */
		SHAREDMEMORY_SLV_VRT_BASESIZE_SR1,
		/* SIZE		 : Size of the memory region */
		true,		/* SHARE : Shared access memory? */
	}
};



/* =============================================================================
 * APIS
 * =============================================================================
 */

/*
 * ======== platform_get_config =======
 *  function to get the default values for confiurations.
 */
static void
platform_get_config(struct platform_config *config)
{
	int status = PLATFORM_S_SUCCESS;

	BUG_ON(config == NULL);
	if (config == NULL) {
		status = -EINVAL;
		goto exit;
	}

	/* get the gatepeterson default config */
	multiproc_get_config(&config->multiproc_config);

	/* get the gatemp default config */
	gatemp_get_config(&config->gatemp_config);

	/* get the gatepeterson default config */
	gatepeterson_get_config(&config->gatepeterson_config);

	/* get the gatehwspinlock default config */
	gatehwspinlock_get_config(&config->gatehwspinlock_config);

	/* get the sharedregion default config */
	sharedregion_get_config(&config->sharedregion_config);

	/* get the messageq default config */
	messageq_get_config(&config->messageq_config);

	/* get the notify default config */
	notify_get_config(&config->notify_config);
	/* get the ipu_pm default config */
	ipu_pm_get_config(&config->ipu_pm_config);

	/* get the procmgr default config */
	proc_mgr_get_config(&config->proc_mgr_config);

	/* get the heapbufmpfault config */
	heapbufmp_get_config(&config->heapbufmp_config);

	/* get the heapmemmpfault config */
	heapmemmp_get_config(&config->heapmemmp_config);
#if 0
	/* get the heapmultibuf default config */
	heapmultibuf_get_config(&config->heapmultibuf_config
#endif
	/* get the listmp default config */
	listmp_get_config(&config->listmp_config);

	/* get the transportshm default config */
	transportshm_get_config(&config->transportshm_config);
	/* get the notifyshmdriver default config */
	notify_ducatidrv_get_config(&config->notify_ducatidrv_config);

	/* get the nameserver_remotenotify default config */
	nameserver_remotenotify_get_config(&config->
				nameserver_remotenotify_config);
#if 0
	/* get the clientnotifymgr default config */
	clientnotifymgr_get_config(&config->clinotifymgr_config_params);

	/*  get the frameqbufmgr default config */
	frameqbufmgr_get_config(&config->frameqbufmgr_config_params);
	/*  get the frameq default config */
	frameq_get_config(&config->frameqcfg_params);

	/*  get the ringio default config */
	ringio_get_config(&config->ringio_config);

	/*  get the ringiotransportshm default config */
	ringiotransportshm_get_config(&config->ringiotransportshm_config);
#endif

exit:
	if (status < 0)
		printk(KERN_ERR "platform_get_config failed! status = 0x%x\n",
								status);
	return;
}


/*
 * ======== platform_override_config ======
 * Function to override the default configuration values.
 *
 */
static int
platform_override_config(struct platform_config *config)
{
	int status = PLATFORM_S_SUCCESS;

	BUG_ON(config == NULL);

	if (config == NULL) {
		status = -EINVAL;
		goto exit;
	}

	/* Override the multiproc_config default config */
	config->multiproc_config.num_processors = 4;
	config->multiproc_config.id = 3;
	strcpy(config->multiproc_config.name_list[0], "Tesla");
	strcpy(config->multiproc_config.name_list[1], "AppM3");
	strcpy(config->multiproc_config.name_list[2], "SysM3");
	strcpy(config->multiproc_config.name_list[3], "MPU");

	/* Override the gate,p default config */
	config->gatemp_config.num_resources = 32;

	/* Override the Sharedregion default config */
	config->sharedregion_config.cache_line_size = 128;

	/* Override the LISTMP default config */

	/* Override the MESSAGEQ default config */
	config->messageq_config.num_heaps = 2;

	/* Override the NOTIFY default config */

	/* Override the PROCMGR default config */

	/* Override the HeapBuf default config */
	config->heapbufmp_config.track_allocs = true;

	/* Override the LISTMPSHAREDMEMORY default config */

	/* Override the MESSAGEQTRANSPORTSHM default config */

	/* Override the NOTIFYSHMDRIVER default config */

	/* Override the NAMESERVERREMOTENOTIFY default config */

	/* Override the  ClientNotifyMgr default config */
	/* Override the  FrameQBufMgr default config */

	/* Override the FrameQ default config */

exit:
	if (status < 0)
		printk(KERN_ERR "platform_override_config failed! status "
				"= 0x%x\n", status);
	return status;
}

/*
 * ======= platform_setup ========
 *       function to setup platform.
 *              TBD: logic would change completely in the final system.
 */
int
platform_setup(void)
{
	int status = PLATFORM_S_SUCCESS;
	struct platform_config _config;
	struct platform_config *config;
	struct platform_mem_map_info m_info;

	platform_get_config(&_config);
	config = &_config;

	/* Initialize PlatformMem */
	status = platform_mem_setup();
	if (status < 0) {
		printk(KERN_ERR "platform_setup : platform_mem_setup "
			"failed [0x%x]\n", status);
	} else {
		printk(KERN_ERR "platform_mem_setup : status [0x%x]\n" ,
			status);
		platform_module->platform_mem_init_flag = true;
	}

	platform_override_config(config);

	status = multiproc_setup(&(config->multiproc_config));
	if (status < 0) {
		printk(KERN_ERR "platform_setup : multiproc_setup "
			"failed [0x%x]\n", status);
	} else {
		printk(KERN_ERR "platform_setup : status [0x%x]\n", status);
		platform_module->multiproc_init_flag = true;
	}

	/* Initialize ProcMgr */
	if (status >= 0) {
		status = proc_mgr_setup(&(config->proc_mgr_config));
		if (status < 0) {
			printk(KERN_ERR "platform_setup : proc_mgr_setup "
				"failed [0x%x]\n", status);
		} else {
			printk(KERN_ERR "proc_mgr_setup : status [0x%x]\n",
				status);
			platform_module->proc_mgr_init_flag = true;
		}
	}

	/* Initialize SharedRegion */
	if (status >= 0) {
		status = sharedregion_setup(&config->sharedregion_config);
		if (status < 0) {
			printk(KERN_ERR "platform_setup : sharedregion_setup "
				"failed [0x%x]\n", status);
		} else {
			printk(KERN_ERR "sharedregion_setup : status [0x%x]\n",
				status);
			platform_module->sharedregion_init_flag = true;
		}
	}

	/* Initialize Notify DucatiDriver */
	if (status >= 0) {
		status = notify_ducatidrv_setup(&config->
						notify_ducatidrv_config);
		if (status < 0) {
			printk(KERN_ERR "platform_setup : "
				"notify_ducatidrv_setup failed [0x%x]\n",
				status);
		} else {
			printk(KERN_ERR "notify_ducatidrv_setup : "
				"status [0x%x]\n", status);
			platform_module->notify_ducatidrv_init_flag = true;
		}
	}

	/* Initialize Notify */
	if (status >= 0) {
		status = notify_setup(&config->notify_config);
		if (status < 0) {
			printk(KERN_ERR "platform_setup : notify_setup "
				"failed [0x%x]\n", status);
		} else {
			printk(KERN_ERR "notify_setup : status [0x%x]\n",
				status);
			platform_module->notify_init_flag = true;
		}
	}

	/* Initialize ipu_pm */
	if (status >= 0) {
		status = ipu_pm_setup(&config->ipu_pm_config);
		if (status < 0) {
			printk(KERN_ERR "platform_setup : ipu_pm_setup "
				"failed [0x%x]\n", status);
		} else {
			printk(KERN_ERR "ipu_pm_setup : status [0x%x]\n",
				status);
			platform_module->ipu_pm_init_flag = true;
		}
	}
	/* Initialize NameServer */
	if (status >= 0) {
		status = nameserver_setup();
		if (status < 0) {
			printk(KERN_ERR "platform_setup : nameserver_setup "
				"failed [0x%x]\n", status);
		} else {
			printk(KERN_ERR "nameserver_setup : status [0x%x]\n",
				status);
			platform_module->nameserver_init_flag = true;
		}
	}

	/* Initialize GateMP */
	if (status >= 0) {
		status = gatemp_setup(&config->gatemp_config);
		if (status < 0) {
			printk(KERN_ERR "platform_setup : gatemp_setup "
				"failed [0x%x]\n", status);
		} else {
			printk(KERN_ERR "gatemp_setup : status [0x%x]\n",
				status);
			platform_module->gatemp_init_flag = true;
		}
	}

	/* Initialize GatePeterson */
	if (status >= 0) {
		status = gatepeterson_setup(&config->gatepeterson_config);
		if (status < 0) {
			printk(KERN_ERR "platform_setup : gatepeterson_setup "
				"failed [0x%x]\n", status);
		} else {
			printk(KERN_ERR "gatepeterson_setup : status [0x%x]\n",
				status);
			platform_module->gatepeterson_init_flag = true;
		}
	}

	/* Initialize GateHWSpinlock */
	if (status >= 0) {
		m_info.src  = 0x4A0F6000;
		m_info.size = 0x1000;
		m_info.is_cached = false;
		status = platform_mem_map(&m_info);
		if (status < 0) {
			printk(KERN_ERR "platform_setup : platform_mem_map "
				"failed [0x%x]\n", status);
		} else {
			config->gatehwspinlock_config.num_locks = 32;
			config->gatehwspinlock_config.base_addr = \
							m_info.dst + 0x800;
			status = gatehwspinlock_setup(&config->
							gatehwspinlock_config);
			if (status < 0) {
				printk(KERN_ERR "platform_setup : "
					"gatehwspinlock_setup failed [0x%x]\n",
					status);
			} else
				platform_module->gatehwspinlock_init_flag =
									true;
		}
	}

	/* Initialize MessageQ */
	if (status >= 0) {
		status = messageq_setup(&config->messageq_config);
		if (status < 0) {
			printk(KERN_ERR "platform_setup : messageq_setup "
				"failed [0x%x]\n", status);
		} else {
			printk(KERN_ERR "messageq_setup : status [0x%x]\n",
				status);
			platform_module->messageq_init_flag = true;
		}
	}
#if 0
	/* Initialize RingIO */
	if (status >= 0) {
		status = ringio_setup(&config->ringio_config);
		if (status < 0) {
			printk(KERN_ERR "platform_setup : ringio_setup "
				"failed [0x%x]\n", status);
		} else {
			printk(KERN_ERR "ringio_setup : status [0x%x]\n",
				status);
			platform_module->ringio_init_flag = true;
		}
	}

	/* Initialize RingIOTransportShm */
	if (status >= 0) {
		status = ringiotransportshm_setup(&config->
						ringiotransportshm_config);
		if (status < 0) {
			printk(KERN_ERR "platform_setup : "
				"ringiotransportshm_setup "
				"failed [0x%x]\n", status);
		} else {
			printk(KERN_ERR "ringiotransportshm_setup : status "
				"[0x%x]\n", status);
			platform_module->ringiotransportshm_init_flag = true;
		}
	}
#endif
	/* Initialize HeapBufMP */
	if (status >= 0) {
		status = heapbufmp_setup(&config->heapbufmp_config);
		if (status < 0) {
			printk(KERN_ERR "platform_setup : heapbufmp_setup "
				"failed [0x%x]\n", status);
		} else {
			printk(KERN_ERR "heapbufmp_setup : status [0x%x]\n",
				status);
			platform_module->heapbufmp_init_flag = true;
		}
	}

	/* Initialize HeapMemMP */
	if (status >= 0) {
		status = heapmemmp_setup(&config->heapmemmp_config);
		if (status < 0) {
			printk(KERN_ERR "platform_setup : heapmemmp_setup "
				"failed [0x%x]\n", status);
		} else {
			printk(KERN_ERR "heapmemmp_setup : status [0x%x]\n",
				status);
			platform_module->heapmemmp_init_flag = true;
		}
	}
#if 0
	/* Initialize HeapMultiBuf */
	if (status >= 0) {
		status = heapmultibuf_setup(&config->heapmultibuf_config);
		if (status < 0) {
			printk(KERN_ERR "platform_setup : heapmultibuf_setup "
				"failed [0x%x]\n", status);
		} else {
			printk(KERN_ERR "heapmultibuf_setup : status [0x%x]\n",
				status);
			platform_module->heapmultibuf_init_flag = true;
		}
	}
#endif
	/* Initialize ListMP */
	if (status >= 0) {
		status = listmp_setup(
				&config->listmp_config);
		if (status < 0) {
			printk(KERN_ERR "platform_setup : "
				"listmp_setup failed [0x%x]\n",
				status);
		} else {
			printk(KERN_ERR "listmp_setup : "
				"status [0x%x]\n", status);
			platform_module->listmp_init_flag = true;
		}
	}

	/* Initialize TransportShm */
	if (status >= 0) {
		status = transportshm_setup(
				 &config->transportshm_config);
		if (status < 0) {
			printk(KERN_ERR "platform_setup : "
				"transportshm_setup failed [0x%x]\n",
				status);
		} else {
			printk(KERN_ERR "transportshm_setup : "
				"status [0x%x]\n", status);
			platform_module->transportshm_init_flag = true;
		}
	}

	/* Initialize NameServerRemoteNotify */
	if (status >= 0) {
		status = nameserver_remotenotify_setup(
				 &config->nameserver_remotenotify_config);
		if (status < 0) {
			printk(KERN_ERR "platform_setup : "
				"nameserver_remotenotify_setup failed "
				"[0x%x]\n", status);
		} else {
			printk(KERN_ERR "nameserver_remotenotify_setup : "
				"status [0x%x]\n", status);
			platform_module->nameserver_remotenotify_init_flag =
									true;
		}
	}
#if 0
	/* Get the ClientNotifyMgr default config */
	if (status >= 0) {
		status = ClientNotifyMgr_setup(&config->cliNotifyMgrCfgParams);
		if (status < 0)
			GT_setFailureReason(curTrace,
					 GT_4CLASS,
					 "Platform_setup",
					 status,
					 "ClientNotifyMgr_setup failed!");
		else
			Platform_module->clientNotifyMgrInitFlag = true;
	}

	/* Get the FrameQBufMgr default config */
	if (status >= 0) {
		status = FrameQBufMgr_setup(&config->frameQBufMgrCfgParams);
		if (status < 0)
			GT_setFailureReason(curTrace,
					 GT_4CLASS,
					 "Platform_setup",
					 status,
					 "FrameQBufMgr_setup failed!");
		else
			Platform_module->frameQBufMgrInitFlag = true;
	}
	/* Get the FrameQ default config */
	if (status >= 0) {
		status = FrameQ_setup(&config->frameQCfgParams);
		if (status < 0)
			GT_setFailureReason(curTrace,
				 GT_4CLASS,
				 "Platform_setup",
				 status,
				 "FrameQ_setup failed!");
	else
		Platform_module->frameQInitFlag = true;
	}
#endif

	if (status >= 0) {
		memset(platform_objects, 0,
			(sizeof(struct platform_object) * \
				MULTIPROC_MAXPROCESSORS));
	}


	/* Initialize Platform */
	if (status >= 0) {
		status = _platform_setup();
		if (status < 0) {
			printk(KERN_ERR "platform_setup : _platform_setup "
				"failed [0x%x]\n", status);
		} else {
			printk(KERN_ERR "_platform_setup : status [0x%x]\n",
				status);
			platform_module->platform_init_flag = true;
		}

	}

	return status;
}


/*
 * =========== platform_destroy ==========
 *  Function to destroy the System.
 */
int
platform_destroy(void)
{
	int status = PLATFORM_S_SUCCESS;
	struct platform_mem_unmap_info u_info;

	/* Finalize Platform module*/
	if (platform_module->platform_init_flag == true) {
		status = _platform_destroy();
		if (status < 0) {
			printk(KERN_ERR "platform_destroy : _platform_destroy "
				"failed [0x%x]\n", status);
		} else {
			platform_module->platform_init_flag = false;
		}
	}
#if 0
	/* Finalize Frame module */
	if (Platform_module->frameQInitFlag == true) {
		status = FrameQ_destroy();
		if (status < 0)
			GT_setFailureReason(curTrace,
					 GT_4CLASS,
					 "Platform_destroy",
					 status,
					 "FrameQ_destroy failed!");
		else
			Platform_module->frameQInitFlag = false;
	}

	/* Finalize FrameQBufMgr module */
	if (Platform_module->frameQBufMgrInitFlag == true) {
		status = FrameQBufMgr_destroy();
		if (status < 0)
			GT_setFailureReason(curTrace,
					 GT_4CLASS,
					 "Platform_destroy",
					 status,
					 "FrameQBufMgr_destroy failed!");
		else
			Platform_module->frameQBufMgrInitFlag = false;
	}

	/* Finalize ClientNotifyMgr module */
	if (Platform_module->clientNotifyMgrInitFlag == true) {
		status = ClientNotifyMgr_destroy();
		if (status < 0)
			GT_setFailureReason(curTrace,
					 GT_4CLASS,
					 "Platform_destroy",
					 status,
					 "ClientNotifyMgr_destroy failed!");
		else
			Platform_module->clientNotifyMgrInitFlag = false;
	}
#endif
	/* Finalize NameServerRemoteNotify module */
	if (platform_module->nameserver_remotenotify_init_flag == true) {
		status = nameserver_remotenotify_destroy();
		if (status < 0) {
			printk(KERN_ERR "platform_destroy : "
				"nameserver_remotenotify_destroy "
				"failed [0x%x]\n", status);
		} else {
			platform_module->nameserver_remotenotify_init_flag \
				= false;
		}
	}

	/* Finalize TransportShm module */
	if (platform_module->transportshm_init_flag == true) {
		status = transportshm_destroy();
		if (status < 0) {
			printk(KERN_ERR "platform_destroy : "
				"transportshm_destroy failed "
				"[0x%x]\n", status);
		} else {
			platform_module->transportshm_init_flag = \
				false;
		}
	}

	/* Finalize ListMP module */
	if (platform_module->listmp_init_flag == true) {
		status = listmp_destroy();
		if (status < 0) {
			printk(KERN_ERR "platform_destroy : "
				"listmp_destroy failed [0x%x]\n",
				status);
		} else {
			platform_module->listmp_init_flag = \
				false;
		}
	}
#if 0
	/* Finalize HeapMultiBuf module */
	if (platform_module->heapmultibuf_init_flag == true) {
		status = heapmultibuf_destroy();
		if (status < 0) {
			printk(KERN_ERR "platform_destroy : "
				"heapmultibuf_destroy "
				"failed [0x%x]\n", status);
		} else {
			platform_module->heapmultibuf_init_flag = false;
		}
	}
#endif
	/* Finalize HeapBufMP module */
	if (platform_module->heapbufmp_init_flag == true) {
		status = heapbufmp_destroy();
		if (status < 0) {
			printk(KERN_ERR "platform_destroy : heapbufmp_destroy "
				"failed [0x%x]\n", status);
		} else {
			platform_module->heapbufmp_init_flag = false;
		}
	}

	/* Finalize HeapMemMP module */
	if (platform_module->heapmemmp_init_flag == true) {
		status = heapmemmp_destroy();
		if (status < 0) {
			printk(KERN_ERR "platform_destroy : heapmemmp_destroy "
				"failed [0x%x]\n", status);
		} else {
			platform_module->heapmemmp_init_flag = false;
		}
	}

	/* Finalize MessageQ module */
	if (platform_module->messageq_init_flag == true) {
		status = messageq_destroy();
		if (status < 0) {
			printk(KERN_ERR "platform_destroy : messageq_destroy "
				"failed [0x%x]\n", status);
		} else {
			platform_module->messageq_init_flag = false;
		}
	}
#if 0
	/* Finalize RingIO module */
	if (platform_module->ringio_init_flag == true) {
		status = ringio_destroy();
		if (status < 0) {
			printk(KERN_ERR "platform_destroy : ringio_destroy "
				"failed [0x%x]\n", status);
		} else {
			platform_module->ringio_init_flag = false;
		}
	}


	/* Finalize RingIOTransportShm module */
	if (platform_module->ringiotransportshm_init_flag == true) {
		status = ringiotransportshm_destroy();
		if (status < 0) {
			printk(KERN_ERR "platform_destroy : "
					"ringiotransportshm_destroy "
					"failed [0x%x]\n", status);
		} else {
			platform_module->ringiotransportshm_init_flag = false;
		}
	}
#endif
	/* Finalize GatePeterson module */
	if (platform_module->gatepeterson_init_flag == true) {
		status = gatepeterson_destroy();
		if (status < 0) {
			printk(KERN_ERR "platform_destroy : "
				"gatepeterson_destroy failed [0x%x]\n", status);
		} else {
			platform_module->gatepeterson_init_flag = false;
		}
	}

	/* Finalize GateHWSpinlock module */
	if (platform_module->gatehwspinlock_init_flag == true) {
		status = gatehwspinlock_destroy();
		if (status < 0) {
			printk(KERN_ERR "platform_destroy : "
				"gatehwspinlock_destroy failed "
				"[0x%x]\n", status);
		} else {
			platform_module->gatehwspinlock_init_flag = false;
		}

		u_info.addr = 0x4A0F6000;
		u_info.size = 0x1000;
		u_info.is_cached = false;
		status = platform_mem_unmap(&u_info);
		if (status < 0)
			printk(KERN_ERR "platform_destroy : platform_mem_unmap"
						" failed [0x%x]\n", status);
	}

	/* Finalize GateMP module */
	if (platform_module->gatemp_init_flag == true) {
		status = gatemp_destroy();
		if (status < 0) {
			printk(KERN_ERR "platform_destroy : "
				"gatemp_destroy failed [0x%x]\n", status);
		} else {
			platform_module->gatemp_init_flag = false;
		}
	}

	/* Finalize NameServer module */
	if (platform_module->nameserver_init_flag == true) {
		status = nameserver_destroy();
		if (status < 0) {
			printk(KERN_ERR "platform_destroy : nameserver_destroy "
				"failed [0x%x]\n", status);
		} else {
			platform_module->nameserver_init_flag = false;
		}
	}
	/* Finalize ipu_pm module */
	if (platform_module->ipu_pm_init_flag == true) {
		status = ipu_pm_destroy();
		if (status < 0) {
			printk(KERN_ERR "platform_destroy : ipu_pm_destroy "
				"failed [0x%x]\n", status);
		} else {
			platform_module->ipu_pm_init_flag = false;
		}
	}

	/* Finalize Notify Ducati Driver module */
	if (platform_module->notify_ducatidrv_init_flag == true) {
		status = notify_ducatidrv_destroy();
		if (status < 0) {
			printk(KERN_ERR "platform_destroy : "
				"notify_ducatidrv_destroy failed [0x%x]\n",
				status);
		} else {
			platform_module->notify_ducatidrv_init_flag = false;
		}
	}

	/* Finalize Notify module */
	if (platform_module->notify_init_flag == true) {
		status = notify_destroy();
		if (status < 0) {
			printk(KERN_ERR "platform_destroy : notify_destroy "
				"failed [0x%x]\n", status);
		} else {
			platform_module->notify_init_flag = false;
		}
	}

	/* Finalize SharedRegion module */
	if (platform_module->sharedregion_init_flag == true) {
		status = sharedregion_destroy();
		if (status < 0) {
			printk(KERN_ERR "platform_destroy : "
				"sharedregion_destroy failed [0x%x]\n", status);
		} else {
			platform_module->sharedregion_init_flag = false;
		}
	}

	/* Finalize ProcMgr module */
	if (platform_module->proc_mgr_init_flag == true) {
		status = proc_mgr_destroy();
		if (status < 0) {
			printk(KERN_ERR "platform_destroy : proc_mgr_destroy "
				"failed [0x%x]\n", status);
		} else {
			platform_module->proc_mgr_init_flag = false;
		}
	}

	/* Finalize MultiProc module */
	if (platform_module->multiproc_init_flag == true) {
		status = multiproc_destroy();
		if (status < 0) {
			printk(KERN_ERR "platform_destroy : multiproc_destroy "
				"failed [0x%x]\n", status);
		} else {
			platform_module->multiproc_init_flag = false;
		}
	}

	/* Finalize PlatformMem module */
	if (platform_module->platform_mem_init_flag == true) {
		status = platform_mem_destroy();
		if (status < 0) {
			printk(KERN_ERR "platform_destroy : "
				"platform_mem_destroy failed [0x%x]\n", status);
		} else {
			platform_module->platform_mem_init_flag = false;
		}
	}

	if (status >= 0)
		memset(platform_objects,
			0,
			(sizeof(struct platform_object) *
				MULTIPROC_MAXPROCESSORS));

	return status;
}

/*
 * ======== platform_setup ========
 *  Purpose:
 *  TBD: logic would change completely in the final system.
 */
static s32 _platform_setup(void)
{

	s32 status = 0;
	struct proc4430_config proc_config;
	struct proc_mgr_params params;
	struct proc4430_params proc_params;
	u16 proc_id;
	struct platform_object *handle;
	void *proc_mgr_handle;
	void *proc_mgr_proc_handle;

	/* Create the SysM3 ProcMgr object */
	proc4430_get_config(&proc_config);
	status = proc4430_setup(&proc_config);
	if (status < 0)
		goto exit;

	/* Get MultiProc ID by name. */
	proc_id = multiproc_get_id("SysM3");
	if (proc_id >= MULTIPROC_MAXPROCESSORS) {
		printk(KERN_ERR "multi proc returned invalid proc id\n");
		goto multiproc_id_fail;
	}
	handle = &platform_objects[proc_id];

	/* Create an instance of the Processor object for OMAP4430 */
	proc4430_params_init(NULL, &proc_params);
	/* TODO: SysLink-38 has these in individual Proc Objects */
	proc_params.num_mem_entries = NUM_MEM_ENTRIES;
	proc_params.mem_entries = mem_entries;
	proc_params.reset_vector_mem_entry = RESET_VECTOR_ENTRY_ID;
	proc_mgr_proc_handle = proc4430_create(proc_id, &proc_params);
	if (proc_mgr_proc_handle == NULL) {
		status = PLATFORM_E_FAIL;
		goto proc_create_fail;
	}

	/* Initialize parameters */
	proc_mgr_params_init(NULL, &params);
	params.proc_handle = proc_mgr_proc_handle;
	proc_mgr_handle = proc_mgr_create(proc_id, &params);
	if (proc_mgr_handle == NULL) {
		status = PLATFORM_E_FAIL;
		goto proc_mgr_create_fail;
	}

	/* SysM3 and AppM3 use the same handle */
	handle->phandle = proc_mgr_proc_handle;
	handle->pm_handle = proc_mgr_handle;

	proc_mgr_handle = NULL;
	proc_mgr_proc_handle = NULL;


	/* Create the AppM3 ProcMgr object */
	/* Get MultiProc ID by name. */
	proc_id = multiproc_get_id("AppM3");
	if (proc_id >= MULTIPROC_MAXPROCESSORS) {
		printk(KERN_ERR "multi proc returned invalid proc id\n");
		goto proc_mgr_create_fail;
	}
	handle = &platform_objects[proc_id];

	/* Create an instance of the Processor object for OMAP4430 */
	proc4430_params_init(NULL, &proc_params);
	proc_params.num_mem_entries = NUM_MEM_ENTRIES;
	proc_params.mem_entries = mem_entries;
	proc_params.reset_vector_mem_entry = RESET_VECTOR_ENTRY_ID;
	proc_mgr_proc_handle = proc4430_create(proc_id, &proc_params);
	if (proc_mgr_proc_handle == NULL) {
		status = PLATFORM_E_FAIL;
		goto proc_create_fail;
	}

	/* Initialize parameters */
	proc_mgr_params_init(NULL, &params);
	params.proc_handle = proc_mgr_proc_handle;
	proc_mgr_handle = proc_mgr_create(proc_id, &params);
	if (proc_mgr_handle == NULL) {
		status = PLATFORM_E_FAIL;
		goto proc_mgr_create_fail;
	}

	handle->phandle = proc_mgr_proc_handle;
	handle->pm_handle = proc_mgr_handle;

	/* Create the Tesla ProcMgr object */
	/* Get MultiProc ID by name. */
	proc_id = multiproc_get_id("Tesla");
	if (proc_id >= MULTIPROC_MAXPROCESSORS) {
		printk(KERN_ERR "multi proc returned invalid proc id\n");
		goto multiproc_id_fail;
	}
	handle = &platform_objects[proc_id];

	/* Create an instance of the Processor object for OMAP4430 */
	proc4430_params_init(NULL, &proc_params);
	proc_params.num_mem_entries = NUM_MEM_ENTRIES_DSP;
	proc_params.mem_entries = mem_entries_dsp;
	proc_params.reset_vector_mem_entry = RESET_VECTOR_ENTRY_ID;
	proc_mgr_proc_handle = proc4430_create(proc_id, &proc_params);
	if (proc_mgr_proc_handle == NULL) {
		status = PLATFORM_E_FAIL;
		goto proc_create_fail;
	}

	/* Initialize parameters */
	proc_mgr_params_init(NULL, &params);
	params.proc_handle = proc_mgr_proc_handle;
	proc_mgr_handle = proc_mgr_create(proc_id, &params);
	if (proc_mgr_handle == NULL) {
		status = PLATFORM_E_FAIL;
		goto proc_mgr_create_fail;
	}

	handle->phandle = proc_mgr_proc_handle;
	handle->pm_handle = proc_mgr_handle;

	/* TODO: See if we need to do proc_mgr_attach on both SysM3 & AppM3
	 * to set the memory maps before hand. Or fix ProcMgr_open &
	 * ProcMgr_attach from the userspace */
	return status;

multiproc_id_fail:
proc_create_fail:
proc_mgr_create_fail:
	/* Clean up created objects */
	_platform_destroy();
exit:
	return status;
}


/*
 * ======== platform_destroy ========
 *  Purpose:
 *  Function to finalize the platform.
 */
static s32 _platform_destroy(void)
{
	s32 status = 0;
	struct platform_object *handle;
	int i;

	for (i = 0; i < MULTIPROC_MAXPROCESSORS; i++) {
		handle = &platform_objects[i];

		/* Delete the Processor instances */
		if (handle->phandle != NULL) {
			status = proc4430_delete(&handle->phandle);
			WARN_ON(status < 0);
		}

		if (handle->pm_handle != NULL) {
			status = proc_mgr_delete(&handle->pm_handle);
			WARN_ON(status < 0);
		}
	}

	status = proc4430_destroy();
	WARN_ON(status < 0);

	return status;
}


/*
 * ======== platform_load_callback ========
 *  Purpose:
 *  Function called by proc_mgr when slave is in loaded state.
 */
int platform_load_callback(u16 proc_id, void *arg)
{
	int status = PLATFORM_S_SUCCESS;
	struct platform_object *handle;
	u32 start;
	u32 num_bytes;
	struct sharedregion_entry entry;
	u32 m_addr = 0;
	/*struct proc_mgr_addr_info ai;*/
	struct ipc_params ipc_params;
	int i;
	void *pm_handle;

	handle = &platform_objects[proc_id];
	pm_handle = handle->pm_handle;

	/* TODO: hack */
	start = (u32)arg;	/* start address passed in as argument */

	/* Read the slave config */
	num_bytes = sizeof(struct platform_slave_config);
	status = _platform_read_slave_memory(proc_id,
					start,
					&handle->slave_config,
					&num_bytes);
	if (status < 0) {
		status = PLATFORM_E_FAIL;
		goto exit;
	}

	if (platform_host_sr_config == NULL)
		platform_host_sr_config = kmalloc(sizeof(struct
				platform_host_sr_config) * handle->
					slave_config.num_srs, GFP_KERNEL);

	if (platform_host_sr_config == NULL) {
		status = -ENOMEM;
		goto alloced_host_sr_config_exit;
	}

	if (handle->slave_config.num_srs > 0) {
		num_bytes = handle->slave_config.num_srs * sizeof(struct
						platform_slave_sr_config);
		handle->slave_sr_config = kmalloc(num_bytes, GFP_KERNEL);
		if (handle->slave_sr_config == NULL) {
			status = -ENOMEM;
			goto exit;
		} else {
			status = _platform_read_slave_memory(
					proc_id,
					start + sizeof(struct
						platform_slave_config),
					handle->slave_sr_config,
					&num_bytes);
			if (status < 0) {
				status = PLATFORM_E_FAIL;
				goto alloced_slave_sr_config_exit;
			}
		}
	}

	if (status >= 0) {
		ipc_params.setup_messageq = handle->slave_config.setup_messageq;
		ipc_params.setup_notify = handle->slave_config.setup_notify;
		ipc_params.setup_ipu_pm = handle->slave_config.setup_ipu_pm;
		ipc_params.proc_sync = handle->slave_config.proc_sync;
		status = ipc_create(proc_id, &ipc_params);
		if (status < 0) {
			status = PLATFORM_E_FAIL;
			goto alloced_slave_sr_config_exit;
		}
	}

	/* Setup the shared memory for region with owner == host */
	/* TODO: May need to replace proc_mgr_map with platform_mem_map */
	for (i = 0; i < handle->slave_config.num_srs; i++) {
		status = sharedregion_get_entry(i, &entry);
		if (status < 0) {
			status = PLATFORM_E_FAIL;
			goto alloced_slave_sr_config_exit;
		}
		BUG_ON(!((entry.is_valid == false)
			|| ((entry.is_valid == true)
				&& (entry.len == (handle->
					slave_sr_config[i].entry_len)))));

		platform_host_sr_config[i].ref_count++;

		/* Add the entry only if previously not added */
		if (entry.is_valid == false) {
			/* Translate the slave address to master */

			/* This SharedRegion is already pre-mapped. So, no need
			 * to do a new mapping. Just need to translate to get
			 * the master virtual address */
			status = proc_mgr_translate_addr(pm_handle,
				(void **)&m_addr,
				PROC_MGR_ADDRTYPE_MASTERKNLVIRT,
				(void *)handle->slave_sr_config[i].entry_base,
				PROC_MGR_ADDRTYPE_SLAVEVIRT);
			if (status < 0) {
				status = PLATFORM_E_FAIL;
				goto alloced_slave_sr_config_exit;
			}

			/* TODO: compatibility with new procmgr */
			/* No need to map this to Slave. Slave is pre-mapped */
			/*status = proc_mgr_map(pm_handle,
				handle->slave_sr_config[i].entry_base,
				handle->slave_sr_config[i].entry_len,
				&ai.addr[PROC_MGR_ADDRTYPE_MASTERKNLVIRT],
				&handle->slave_sr_config[i].entry_len,
				PROC_MGR_MAPTYPE_VIRT);
			if (status < 0) {
				status = PLATFORM_E_FAIL;
				goto alloced_slave_sr_config_exit;
			}

			memset((u32 *)ai.addr[PROC_MGR_ADDRTYPE_MASTERKNLVIRT],
				0, handle->slave_sr_config[i].entry_len); */
			memset((u32 *)m_addr, 0,
				handle->slave_sr_config[i].entry_len);
			memset(&entry, 0, sizeof(struct sharedregion_entry));
			/*entry.base = (void *)ai.
					addr[PROC_MGR_ADDRTYPE_MASTERKNLVIRT];*/
			entry.base = (void *) m_addr;
			entry.len = handle->slave_sr_config[i].entry_len;
			entry.owner_proc_id = handle->slave_sr_config[i].
							owner_proc_id;
			entry.is_valid = true;
			entry.cache_line_size = handle->slave_sr_config[i].
							cache_line_size;
			entry.create_heap = handle->slave_sr_config[i].
								create_heap;
			_sharedregion_set_entry(handle->slave_sr_config[i].id,
						&entry);
		}
	}

	/* Read sr0_memory_setup */
	num_bytes = sizeof(struct platform_slave_config);
	handle->slave_config.sr0_memory_setup = 1;
	status = _platform_write_slave_memory(proc_id,
						start,
						&handle->slave_config,
						&num_bytes);
	if (status < 0) {
		status = PLATFORM_E_FAIL;
		goto alloced_slave_sr_config_exit;
	}

	status = ipc_start();
	if (status < 0) {
		status = PLATFORM_E_FAIL;
		goto alloced_slave_sr_config_exit;
	}

	return 0;

alloced_slave_sr_config_exit:
	kfree(handle->slave_sr_config);

alloced_host_sr_config_exit:
	kfree(platform_host_sr_config);
exit:
	if (status < 0)
		printk(KERN_ERR "platform_load_callback failed, status [0x%x]\n",
			status);

	return status;
}
EXPORT_SYMBOL(platform_load_callback);


/*
 * ======== platform_start_callback ========
 *  Purpose:
 *  Function called by proc_mgr when slave is in started state.
 *  FIXME: logic would change completely in the final system.
 */
int platform_start_callback(u16 proc_id, void *arg)
{
	int status = PLATFORM_S_SUCCESS;

	do {
		status = ipc_attach(proc_id);
		msleep(1);
	} while (status < 0);

	if (status < 0)
		printk(KERN_ERR "platform_load_callback failed, status [0x%x]\n",
			status);

	return status;
}
EXPORT_SYMBOL(platform_start_callback);
/* FIXME: since application has to call this API for now */


/*
 * ======== platform_stop_callback ========
 *  Purpose:
 *  Function called by proc_mgr when slave is in stopped state.
 *  FIXME: logic would change completely in the final system.
 */
int platform_stop_callback(u16 proc_id, void *arg)
{
	int status = PLATFORM_S_SUCCESS;
	u32 i;
	u32 m_addr;
	struct platform_object *handle;
	void *pm_handle;

	handle = (struct platform_object *)&platform_objects[proc_id];
	pm_handle = handle->pm_handle;
	/* delete the System manager instance here */
	for (i = 0;
		((handle->slave_sr_config != NULL) &&
			(i < handle->slave_config.num_srs));
		i++) {
			platform_host_sr_config[i].ref_count--;
			if (platform_host_sr_config[i].ref_count == 0) {
				platform_num_srs_unmapped++;
			/* Translate the slave address to master */
			/* TODO: backwards compatibility with old procmgr */
			status = proc_mgr_translate_addr(pm_handle,
				(void **)&m_addr,
				PROC_MGR_ADDRTYPE_MASTERKNLVIRT,
				(void *)handle->slave_sr_config[i].entry_base,
				PROC_MGR_ADDRTYPE_SLAVEVIRT);
			if (status < 0) {
				status = PLATFORM_E_FAIL;
				continue;
			}

		}
	}

	if (platform_num_srs_unmapped == handle->slave_config.num_srs) {
		if (handle->slave_sr_config != NULL) {
			kfree(handle->slave_sr_config);
			handle->slave_sr_config = NULL;
		}
		if (platform_host_sr_config != NULL) {
			kfree(platform_host_sr_config);
			platform_host_sr_config = NULL;
			platform_num_srs_unmapped = 0;
		}
	}

	ipc_detach(proc_id);

	ipc_stop();

	return status;
}
EXPORT_SYMBOL(platform_stop_callback);

/*  ============================================================================
 *  Internal functions
 *  ============================================================================
 */
/* Function to read slave memory */
static int
_platform_read_slave_memory(u16 proc_id,
			    u32 addr,
			    void *value,
			    u32 *num_bytes)
{
	int status = 0;
	bool done = false;
	struct platform_object *handle;
	u32 m_addr;
	void *pm_handle;

	handle = (struct platform_object *)&platform_objects[proc_id];
	BUG_ON(handle == NULL);
	if (handle == NULL) {
		status = -EINVAL;
		goto exit;
	}

	pm_handle = handle->pm_handle;
	BUG_ON(pm_handle == NULL);
	if (pm_handle == NULL) {
		status = -EINVAL;
		goto exit;
	}

	/* TODO: backwards compatibility with old procmgr */
	status = proc_mgr_translate_addr(pm_handle,
					(void **)&m_addr,
					PROC_MGR_ADDRTYPE_MASTERKNLVIRT,
					(void *)addr,
					PROC_MGR_ADDRTYPE_SLAVEVIRT);
	if (status >= 0) {
		memcpy(value, (void *) m_addr, *num_bytes);
		done = true;
		printk(KERN_ERR "_platform_read_slave_memory successful! "
			"status = 0x%x, proc_id = %d, addr = 0x%x, "
			"m_addr = 0x%x, size = 0x%x", status, proc_id, addr,
			m_addr, *num_bytes);
	} else {
		printk(KERN_ERR "_platform_read_slave_memory failed! "
			"status = 0x%x, proc_id = %d, addr = 0x%x, "
			"m_addr = 0x%x, size = 0x%x", status, proc_id, addr,
			m_addr, *num_bytes);
		status = PLATFORM_E_FAIL;
		goto exit;
	}

	if (done == false) {
		status = proc_mgr_read(pm_handle,
					addr,
					num_bytes,
					value);
		if (status < 0) {
			status = PLATFORM_E_FAIL;
			goto exit;
		}
	}

exit:
	return status;
}


/* Function to write slave memory */
static int _platform_write_slave_memory(u16 proc_id, u32 addr, void *value,
					u32 *num_bytes)
{
	int status = 0;
	bool done = false;
	struct platform_object *handle;
	u32 m_addr;
	void *pm_handle = NULL;

	handle = (struct platform_object *)&platform_objects[proc_id];
	BUG_ON(handle == NULL);
	if (handle == NULL) {
		status = -EINVAL;
		goto exit;
	}

	pm_handle = handle->pm_handle;
	BUG_ON(pm_handle == NULL);
	if (pm_handle == NULL) {
		status = -EINVAL;
		goto exit;
	}

	/* Translate the slave address to master address */
	/* TODO: backwards compatibility with old procmgr */
	status = proc_mgr_translate_addr(pm_handle,
					(void **)&m_addr,
					PROC_MGR_ADDRTYPE_MASTERKNLVIRT,
					(void *)addr,
					PROC_MGR_ADDRTYPE_SLAVEVIRT);
	if (status >= 0) {
		memcpy((void *) m_addr, value, *num_bytes);
		done = true;
		printk(KERN_ERR "_platform_write_slave_memory successful! "
			"status = 0x%x, proc_id = %d, addr = 0x%x, "
			"m_addr = 0x%x, size = 0x%x", status, proc_id, addr,
			m_addr, *num_bytes);
	} else {
		printk(KERN_ERR "_platform_write_slave_memory failed! "
			"status = 0x%x, proc_id = %d, addr = 0x%x, "
			"m_addr = 0x%x, size = 0x%x", status, proc_id, addr,
			m_addr, *num_bytes);
		status = PLATFORM_E_FAIL;
		goto exit;
	}

	if (done == false) {
		status = proc_mgr_write(pm_handle,
					addr,
					num_bytes,
					value);
		if (status < 0) {
			status = PLATFORM_E_FAIL;
			goto exit;
		}
	}

exit:
	return status;
}
