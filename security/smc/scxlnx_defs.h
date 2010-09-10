/*
 * Copyright (c) 2006-2010 Trusted Logic S.A.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA 02111-1307 USA
 */

#ifndef __SCXLNX_DEFS_H__
#define __SCXLNX_DEFS_H__

#include <linux/version.h>
#include <asm/atomic.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/completion.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/sysdev.h>
#include <linux/sysfs.h>
#include <linux/timer.h>

#include "scx_sm_protocol.h"

/*-------------------------------------------------------------------------- */

#define SIZE_1KB 0x400

/*RPC for CUS implementation */
#define RPC_AES1_CODE		0x001
#define RPC_AES2_CODE		0x002
#define RPC_DES_CODE		0x004
#define RPC_SHA_CODE		0x008

#define RPC_COMMAND_MASK	0x000003c0

#define RPC_INSTALL_SHORTCUT_LOCK_ACCELERATOR		0x200
#define RPC_INSTALL_SHORTCUT_LOCK_ACCELERATOR_UNLOCK	0x000
#define RPC_INSTALL_SHORTCUT_LOCK_ACCELERATOR_LOCK	0x001

#define RPC_LOCK_ACCELERATORS_SUSPEND_SHORTCUT			0x240
#define RPC_LOCK_ACCELERATORS_SUSPEND_SHORTCUT_LOCK_AES1	RPC_AES1_CODE
#define RPC_LOCK_ACCELERATORS_SUSPEND_SHORTCUT_LOCK_AES2	RPC_AES2_CODE
#define RPC_LOCK_ACCELERATORS_SUSPEND_SHORTCUT_LOCK_DES		RPC_DES_CODE
#define RPC_LOCK_ACCELERATORS_SUSPEND_SHORTCUT_LOCK_SHA		RPC_SHA_CODE
#define RPC_LOCK_ACCELERATORS_SUSPEND_SHORTCUT_SUSPEND		0x010
#define RPC_LOCK_ACCELERATORS_SUSPEND_SHORTCUT_UNINSTALL	0x020

#define RPC_RESUME_SHORTCUT_UNLOCK_ACCELERATORS			0x280
#define RPC_RESUME_SHORTCUT_UNLOCK_ACCELERATORS_UNLOCK_AES1	RPC_AES1_CODE
#define RPC_RESUME_SHORTCUT_UNLOCK_ACCELERATORS_UNLOCK_AES2	RPC_AES2_CODE
#define RPC_RESUME_SHORTCUT_UNLOCK_ACCELERATORS_UNLOCK_DES	RPC_DES_CODE
#define RPC_RESUME_SHORTCUT_UNLOCK_ACCELERATORS_UNLOCK_SHA	RPC_SHA_CODE
#define RPC_RESUME_SHORTCUT_UNLOCK_ACCELERATORS_RESUME		0x010

#define RPC_CLEAR_GLOBAL_KEY_CONTEXT			0x2c0
#define RPC_CLEAR_GLOBAL_KEY_CONTEXT_CLEARED_AES	0x001
#define RPC_CLEAR_GLOBAL_KEY_CONTEXT_CLEARED_DES	0x002

#define LOCK_HWA	true
#define UNLOCK_HWA	false

#define ENABLE_CLOCK	true
#define DISABLE_CLOCK	false

/*-------------------------------------------------------------------------- */

/*
 *Shared memory max allowed descriptors
 */
#define SCXLNX_SHMEM_MAX_DESCRIPTORS	(64)

/*
 *This typedef describes the possible types of shared memories
 *
 *SCXLNX_SHMEM_TYPE_DEVICE_CONTEXT :
 *The descriptor describes a device context shared memory
 *SCXLNX_SHMEM_TYPE_PREALLOC_REGISTERED_SHMEM :
 *The descriptor describes a registered shared memory.
 *Its coarse pages are preallocated when initilizing the
 *shared memory monitor
 *SCXLNX_SHMEM_TYPE_REGISTERED_SHMEM :
 *The descriptor describes a registered shared memory.
 *Its coarse pages are not preallocated
 */
typedef enum {
	SCXLNX_SHMEM_TYPE_DEVICE_CONTEXT = 0,
	SCXLNX_SHMEM_TYPE_PREALLOC_REGISTERED_SHMEM,
	SCXLNX_SHMEM_TYPE_REGISTERED_SHMEM,
} SCXLNX_SHMEM_TYPE;

/*
 *This typedef describes the possible types of shared memories
 *
 *SCXLNX_SHMEM_TYPE_DEVICE_CONTEXT :
 *The descriptor describes a device context shared memory
 *SCXLNX_SHMEM_TYPE_PREALLOC_REGISTERED_SHMEM :
 *The descriptor describes a registered shared memory.
 *Its coarse pages are preallocated when initilizing the
 *shared memory monitor
 *SCXLNX_SHMEM_TYPE_REGISTERED_SHMEM :
 *The descriptor describes a registered shared memory.
 *Its coarse pages are not preallocated
 */
typedef enum {
	SCXLNX_SHMEM_ALLOC_TYPE_NONE = 0,
	SCXLNX_SHMEM_ALLOC_TYPE_REGISTER,
	SCXLNX_SHMEM_ALLOC_TYPE_KMALLOC,
	SCXLNX_SHMEM_ALLOC_TYPE_PAGES,
} SCXLNX_SHMEM_ALLOC_TYPE;

/*
 * This structure contains a pointer on a coarse page table
 */
typedef struct {
	/*
	 * Identifies the coarse page table descriptor in
	 * sFreeCoarsePageTables list
	 */
	struct list_head list;

	/*
	 * The address of the coarse page table
	 */
	u32 *pDescriptors;

	/*
	 * The address of the array containing this coarse page table
	 */
	struct SCXLNX_COARSE_PAGE_TABLE_ARRAY *pParent;
} SCXLNX_COARSE_PAGE_TABLE;


#define SCXLNX_PAGE_DESCRIPTOR_TYPE_NORMAL       0
#define SCXLNX_PAGE_DESCRIPTOR_TYPE_PREALLOCATED 1

/*
 * This structure describes an array of up to 4 coarse page tables
 * allocated within a single 4KB page.
 */
typedef struct SCXLNX_COARSE_PAGE_TABLE_ARRAY {
	/*
	 * identifies the element in the sCoarsePageTableArrays list
	 */
	struct list_head list;

	/*
	 * Type of page descriptor
	 * can take any of SCXLNX_PAGE_DESCRIPTOR_TYPE_XXX value
	 */
	u32 nType;

	SCXLNX_COARSE_PAGE_TABLE sCoarsePageTables[4];

	/*
	 * A counter of the number of coarse pages currently used the max value
	 * should be 4 (one coarse page table is 1KB while one page is 4KB)
	 */
	u8 nReferenceCount;
} SCXLNX_COARSE_PAGE_TABLE_ARRAY;


/*
 * This structure describes a list of coarse page table arrays
 * with some of the coarse page tables free. It is used
 * when the driver needs to allocate a new coarse page
 * table.
 */
typedef struct {
	/*
	 * The spin lock protecting concurrent access to the structure.
	 */
	spinlock_t lock;

	/*
	 * The list of allocated coarse page table arrays
	 */
	struct list_head sCoarsePageTableArrays;

	/*
	 * The list of free coarse page tables
	 */
	struct list_head sFreeCoarsePageTables;
} SCXLNX_COARSE_PAGE_TABLE_ALLOCATION_CONTEXT;

/*
 *Fully describes a shared memory block (device context or not)
 */
typedef struct {
	/*
	 *Identifies the shared memory descriptor in the list of
	 *free shared memory descriptors
	 */
	struct list_head list;

	/*
	 *Identifies the type of shared memory descriptor
	 */
	SCXLNX_SHMEM_TYPE nType;

	/*
	 *The identifier of the block of shared memory, as returned by
	 *the smodule. This identifier can be the hBlock field of a
	 *REGISTER_SHARED_MEMORY answer or the hDeviceContext field of
	 *a CREATE_DEVICE_CONTEXT answer
	 */
	u32 hIdentifier;

	/*
	 *Buffer description (aligned on a page boundary)
	 *See SCXLNX_SHMEM_ALLOC_TYPE_xxx
	 */
	SCXLNX_SHMEM_ALLOC_TYPE nAllocType;

	/*
	 *Client buffer allocated or registered
	 */
	u8 *pBuffer;

	/*
	 *Client buffer size
	 */
	u32 nBufferSize;
	/*
	 * Access flags;
	 */
	u32 nFlags;

	/*
	 *SCXLNX_SHMEM_ALLOC_TYPE_REGISTER: Allocated pages
	 */
	struct page **ppPages;

	/*
	 *SCXLNX_SHMEM_ALLOC_TYPE_REGISTER: Number of pages in ppPages
	 *SCXLNX_SHMEM_ALLOC_TYPE_KMALLOC: Number of bytes allocated
	 *SCXLNX_SHMEM_ALLOC_TYPE_PAGES: Number of bytes allocated
	 */
	u32 nAllocatedBufferSize;

	/*
	 *SCXLNX_SHMEM_ALLOC_TYPE_KMALLOC: Buffer allocated
	 *SCXLNX_SHMEM_ALLOC_TYPE_PAGES: Buffer allocated
	 */
	u8 *pAllocatedBuffer;

	/*
	 *SCXLNX_SHMEM_ALLOC_TYPE_PAGES: Buffer order
	 */
	u32 nBufferOrder;

	/*
	 *Physical buffer address
	 */
	u32 nBufferPhysAddr;

	/*
	 * Reference counter
	 */
	atomic_t nRefCnt;

   /* Up to eight coarse page table context */
   SCXLNX_COARSE_PAGE_TABLE *pCoarsePageTable[SCX_MAX_COARSE_PAGES];

   u32 nNumberOfCoarsePageTables;

} SCXLNX_SHMEM_DESC;

/*-------------------------------------------------------------------------- */
/*
 *The magic word.
 */
#define CRYPTOKI_UPDATE_SHORTCUT_CONTEXT_MAGIC		0x45EF683C

/*-------------------------------------------------------------------------- */
/* CUS context structure                                                     */
/*-------------------------------------------------------------------------- */

/*State of an AES operation */
typedef struct {
	u32 AES_IV_0;
	u32 AES_IV_1;
	u32 AES_IV_2;
	u32 AES_IV_3;
} PUBLIC_CRYPTO_AES_OPERATION_STATE;

typedef struct {
	u32 DES_IV_L;
	u32 DES_IV_H;
} PUBLIC_CRYPTO_DES_OPERATION_STATE;

#define HASH_BLOCK_BYTES_LENGTH		64

typedef struct {
	/*Current digest */
	u32 SHA_DIGEST_A;
	u32 SHA_DIGEST_B;
	u32 SHA_DIGEST_C;
	u32 SHA_DIGEST_D;
	u32 SHA_DIGEST_E;
	u32 SHA_DIGEST_F;
	u32 SHA_DIGEST_G;
	u32 SHA_DIGEST_H;

	/*This buffer contains a partial chunk */
	u8 pChunkBuffer[HASH_BLOCK_BYTES_LENGTH];

	/*Number of bytes stored in pChunkBuffer (0..64) */
	u32 nChunkLength;

	/*
	 * Total number of bytes processed so far
	 * (not including the partial chunk)
	 */
	u32 nBytesProcessed;

} PUBLIC_CRYPTO_SHA_OPERATION_STATE;

typedef union {
	PUBLIC_CRYPTO_AES_OPERATION_STATE aes;
	PUBLIC_CRYPTO_DES_OPERATION_STATE des;
	PUBLIC_CRYPTO_SHA_OPERATION_STATE sha;
} PUBLIC_CRYPTO_OPERATION_STATE;

/*
 *Fully describes a public crypto operation
 *(i.e., an operation that has a shortcut attached).
 */
typedef struct {
	/*
	 *Identifies the public crypto operation in the list of all public
	 *operations.
	 */
	struct list_head list;

	u32 nMagicNumber;	/*Must be set to
				 *{CRYPTOKI_UPDATE_SHORTCUT_CONTEXT_MAGIC} */

	/*basic fields */
	u32 hClientSession;
	u32 nCommandID;
	u32 nHWAID;
	u32 nHWA_CTRL;
	u32 hKeyContext;
	PUBLIC_CRYPTO_OPERATION_STATE sOperationState;
	u32 nUseCount;
	bool bSuspended;
} CRYPTOKI_UPDATE_SHORTCUT_CONTEXT;

typedef struct {
	/*fields for data processing of an update command */
	u32 nDataLength;
	u8 *pDataBuffer;

	u8 *pResultBuffer;

	u8 *pS2CDataBuffer;
	u32 nS2CDataBufferMaxLength;

	/*
	 *A non-null buffer means that the input/output buffer
	 *uses shared memory
	 */
	SCXLNX_SHMEM_DESC *pInputShMem;
	SCXLNX_SHMEM_DESC *pOutputShMem;

}
CRYPTOKI_UPDATE_PARAMS;


/*-------------------------------------------------------------------------- */
/*
 *This is a global structure that contains information about all the
 *public crypto operations.
 */
/*-------------------------------------------------------------------------- */

/*
 *Monitors the communication with a SM instance.
 */
typedef struct {
	/*
	 *The spin lock protecting concurrent access to the structure.
	 */
	spinlock_t lock;

	/*
	 *Bit vector with the following possible flags:
	 *- SCXLNX_SM_COMM_FLAG_POLLING_THREAD_STARTED: If set, indicates that
	 *the polling thread has been successfully started.
	 *- SCXLNX_SM_COMM_FLAG_TERMINATING: If set, indicates that the
	 *communication with the PA is being terminated. Transmissions
	 *to the PA are not permitted
	 *
	 *This bit vector must be accessed with the kernel's atomic bitwise
	 *operations.
	 */
	unsigned long nFlags;

	/*
	 *The virtual address of the SM communication buffer.
	 */
	SCHANNEL_C1S_BUFFER *pBuffer;

	/*
	 *The wait queue the polling thread is waiting on.
	 */
	wait_queue_head_t waitQueue;

	/*
	 *Used to synchronize processes while stopping the polling thread.
	 *usage : A parent process tells the polling thread to stop
	 *and waits for pollingThreadDeath to complete.
	 *The polling thread stops and completes the pollingThreadDeath,
	 *waking up the parent process
	 */
	struct completion pollingThreadDeath;

	/*
	 *The SE SDP can be initialized only once...
	 */
	bool bSDPInitialized;

	/*
	 *The virtual address of the L0 communication buffer.
	 */
	void *pL0SharedBuffer;

	/*
	 *Temporary buffers used to load and start the PA
	 */
	void *pPAInfo;
	void *pPABuffer;

	/*
	 *(Temporary) RAW Buffer used before changing the cache descriptors.
	 */
	SCHANNEL_C1S_BUFFER *pBufferRaw;	/*SM communication buffer */
	void *pL0SharedBufferRaw;	/*L0 communucation buffer */
	void *pPAInfoRaw;		/*Temporary buffers used to load
					 *and start the PA */
	void *pPABufferRaw;		/*Temporary buffers used to load
					 * and start the PA */

	/*
	 *The address passed to SDP_RUNTIME_INIT :
	 *Used for the power management cbk
	 */
	uint32_t nSDPBkExtStoreAddr;

	/*
	 *The value in ms used for the power management timer delay.
	 */
	uint32_t nInactivityTimerExpireMs;

	/*Global variables locks */
	/*
	 *Lock to protect concurrent access to the dma channels.
	 */
	struct semaphore sDMALock;

#ifdef SMC_OMAP4_POWER_MANAGEMENT
	/*
	 *Lock to protect concurrent access to the inactivity timer.
	 */
	struct semaphore sPowerManagementTimerLock;
	/*
	 *The timeout timer used to power off clocks
	 */
	struct timer_list pPowerManagementTimer;
#endif				/*SMC_OMAP4_POWER_MANAGEMENT */

	/*
	 * If not 0, indicate the error that happened while loading the PA
	 */
	uint32_t nPALoadError;

} SCXLNX_SM_COMM_MONITOR;

#define SCXLNX_SM_COMM_FLAG_POLLING_THREAD_STARTED  (1)
#define SCXLNX_SM_COMM_FLAG_TERMINATING				 (2)
#define SCXLNX_SM_COMM_FLAG_PA_LOADING				 (4)

/*-------------------------------------------------------------------------- */

typedef struct {
	struct kobject kobj;

	struct kobj_type kobj_type;

	struct attribute kobj_stat_attribute;

	struct attribute *kobj_attribute_list[2];

	struct sysfs_ops kobj_sysfs_operations;

	atomic_t stat_pages_allocated;
	atomic_t stat_memories_allocated;
	atomic_t stat_pages_locked;
} SCXLNX_DEVICE_STATS;

/*
 *Device monitoring descriptor.
 *This is actually the driver private data
 *describing device specific information
 */
typedef struct {
	/*
	 *field describing driver version
	 */
	u8 sDriverDescription[SCX_DESCRIPTION_BUFFER_LENGTH];

	/*
	 *Bit vector with the following flags:
	 *- SCXLNX_DEVICE_FLAG_CDEV_INITIALIZED:
	 *	If set, cdev has been initialized
	 *- SCXLNX_DEVICE_FLAG_SYSDEV_CLASS_REGISTERED:
	 *	If set, sysdev_class has been registered
	 *- SCXLNX_DEVICE_FLAG_SYSDEV_REGISTERED:
	 *	If set, sysdev has been registered
	 *- SCXLNX_DEVICE_FLAG_CDEV_REGISTERED:
	 *	If set, cdev has been registered
	 *- SCXLNX_DEVICE_FLAG_CDEV_ADDED:
	 *	If set, cdev has been added and the character device
	 *	callbacks may be called by the kernel
	 *Use the atomic bit operations to manipulate these flags
	 */
	unsigned long nFlags;

	/*
	 *The device number for the device.
	 */
	dev_t nDevNum;

	/*
	 *Interfaces the system device with the kernel.
	 */
	struct sys_device sysdev;

	/*
	 *Interfaces the char device with the kernel.
	 */
	struct cdev cdev;

	/*
	 *Monitors the communication with the PA SMC.
	 */
	SCXLNX_SM_COMM_MONITOR sm;

	/*
	 *Lists the connections attached to this device.
	 *A connection is created each time a user space
	 *application "opens" a file descriptor on the driver
	 *(ie: list of device context)
	 *(SCXLNX_CONN_MONITOR*)
	 */
	struct list_head conns;

	/*
	 *The spin lock used to protect concurrent access to the
	 *connection list.
	 */
	spinlock_t connsLock;

	SCXLNX_DEVICE_STATS sDeviceStats;

	/*
	 *global variables for CUS
	 */
	/*current key handle loaded in the AES1/D3D2 HWA */
	u32 hAES1SecureKeyContext;
	u32 hAES2SecureKeyContext;
	u32 hD3D2SecureKeyContext;

	/*semaphores used to serialize HWA access */
	struct semaphore sAES1CriticalSection;
	struct semaphore sAES2CriticalSection;
	struct semaphore sD3D2CriticalSection;
	struct semaphore sSHAM1CriticalSection;

	/*
	 *An alligned and correctly shaped pre-allocated buffer used
	 *for the DMA
	 */
	u32 nDMABufferLength;
	u8 *pDMABuffer;
	dma_addr_t pDMABufferPhys;

} SCXLNX_DEVICE_MONITOR;

/*the bits of the nFlags field of the SCXLNX_DEVICE_MONITOR structure */
#define SCXLNX_DEVICE_FLAG_CDEV_INITIALIZED		(0)
#define SCXLNX_DEVICE_FLAG_SYSDEV_CLASS_REGISTERED	(1)
#define SCXLNX_DEVICE_FLAG_SYSDEV_REGISTERED		(2)
#define SCXLNX_DEVICE_FLAG_CDEV_REGISTERED		(3)
#define SCXLNX_DEVICE_FLAG_CDEV_ADDED			(4)
#define SCXLNX_DEVICE_SYSFS_REGISTERED			(5)

/*-------------------------------------------------------------------------- */

/*
 *Monitors shared memories allocated or registered by the driver.
 */
typedef struct {
	/*
	 *handle the device context descriptor separately
	 */
	SCXLNX_SHMEM_DESC *pDeviceContextDesc;

	/*
	 *Lists the used shared memory descriptors
	 */
	struct list_head sUsedSharedMemoryList;

	/*
	 *Lists the free shared memory descriptors
	 */
	struct list_head sFreeSharedMemoryList;

	/*
	 *A mutex to use to access this structure
	 */
	struct semaphore sharedMemoriesMutex;

	/*
	 *Counts the number of shared memories registered.
	 */
	atomic_t nShmemAllocated;

	/*
	* Page to retrieve memory properties when
	* registering shared memory through
	* CREATE_DEVICE_CONTEXT of REGISTER_SHARED_MEMORY
	* messages
	*/
	struct vm_area_struct **ppVmas;

	/*
	* coarse page table allocation context
	*/
	SCXLNX_COARSE_PAGE_TABLE_ALLOCATION_CONTEXT sAllocationContext;

} SCXLNX_SHMEM_MONITOR;

/*-------------------------------------------------------------------------- */
/*
 *This type describes a connection state.
 *This is used to determine whether a
 *message is valid or not.
 *
 *Messages are only valid in a certain device state.
 *Messages may be invalidated between
 *the start of the ioctl call and
 *the moment the message is sent to the SM.
 *
 *SCXLNX_CONN_STATE_NO_DEVICE_CONTEXT :
 *The connection has no DEVICE_CONTEXT created and no
 *CREATE_DEVICE_CONTEXT being processed by the SM
 *SCXLNX_CONN_STATE_CREATE_DEVICE_CONTEXT_SENT :
 *The connection has a CREATE_DEVICE_CONTEXT being processed by the SM
 *SCXLNX_CONN_STATE_VALID_DEVICE_CONTEXT :
 *The connection has a DEVICE_CONTEXT created and no
 *DESTROY_DEVICE_CONTEXT is being processed by the SM
 *SCXLNX_CONN_STATE_DESTROY_DEVICE_CONTEXT_SENT :
 *The connection has a DESTROY_DEVICE_CONTEXT being processed by the SM
 */
typedef enum {
	SCXLNX_CONN_STATE_NO_DEVICE_CONTEXT = 0,
	SCXLNX_CONN_STATE_CREATE_DEVICE_CONTEXT_SENT,
	SCXLNX_CONN_STATE_VALID_DEVICE_CONTEXT,
	SCXLNX_CONN_STATE_DESTROY_DEVICE_CONTEXT_SENT
} SCXLNX_CONN_STATE;

/*
 *Monitors a connection instance.
 *A connection is created each time an application
 *opens a file descriptor on the driver
 */
typedef struct {
	/*
	 *Identifies the connection in the list of the connections attached
	 *to the same device.
	 */
	struct list_head list;

	/*
	 *State of the connection.
	 */
	SCXLNX_CONN_STATE nState;

	/*
	 *A spinlock to use to access nState
	 */
	spinlock_t stateLock;

	/*
	 *A pointer to the monitor of the device the connection is attached to.
	 */
	SCXLNX_DEVICE_MONITOR *pDevice;

	/**
	 *The monitor for the shared memory blocks
	 **/
	SCXLNX_SHMEM_MONITOR sSharedMemoryMonitor;

	/*
	 *Counts the number of operations currently pending on the connection.
	 *(for debug only)
	 */
	atomic_t nPendingOpCounter;

	/*The SCXLNX_CONN_MONITOR is considered as the device context
	 * then, the CUS context list must be located here */
	/*
	 *Lists all the public Cryptoki Update Shortcuts CUS
	 *(CRYPTOKI_UPDATE_SHORTCUT_CONTEXT *).
	 */
	struct list_head ShortcutList;

	/*
	 *Lock to protect concurrent access to the above ShortcutList list
	 *of shortcuts associated to this connection (ie: device context)
	 */
	spinlock_t shortcutListCriticalSectionLock;

} SCXLNX_CONN_MONITOR;

extern SCXLNX_DEVICE_MONITOR g_SCXLNXDeviceMonitor;

/*-------------------------------------------------------------------------- */

/**
 *The ASCII-C string representation of the base name of the devices managed by
 *the SModule driver.
 */
#define SCXLNX_DEVICE_BASE_NAME		"smodule"

/**
 *The major and minor numbers of the registered character device driver.
 *Only 1 SModule exists, hence only 1 instance of the driver is supported.
 */
#define SCXLNX_DEVICE_MINOR_NUMBER		0

/*-------------------------------------------------------------------------- */
/*
 *Kernel Differences
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 31)
#define IO_ADDRESS OMAP2_L4_IO_ADDRESS
#define CLEAN_CACHE_CFG_MASK (~0xC) /* 1111 0011 */
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 29)

#define SET_PAGE_LOCKED		__set_page_locked
#define CLEAR_PAGE_LOCKED	__clear_page_locked
#define CURRENT_UID			current_uid()
#define CURRENT_EUID			current_euid()
#define CURRENT_EGID			current_egid()

#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 27)

#define SET_PAGE_LOCKED		set_page_locked
#define CLEAR_PAGE_LOCKED	clear_page_locked
#define CURRENT_UID			(current->uid)
#define CURRENT_EUID			(current->euid)
#define CURRENT_EGID			(current->egid)

#else

#define SET_PAGE_LOCKED 	SetPageLocked
#define CLEAR_PAGE_LOCKED	ClearPageLocked
#define CURRENT_UID			(current->uid)
#define CURRENT_EUID			(current->euid)
#define CURRENT_EGID			(current->egid)

#endif

#endif	/*!defined(__SCXLNX_DEFS_H__) */
