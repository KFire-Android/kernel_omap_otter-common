/*
 * Copyright (c) 2006-2010 Trusted Logic S.A.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <asm/atomic.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/list.h>
#include <linux/mm.h>
#include <linux/pagemap.h>
#include <linux/types.h>

#include "s_version.h"

#include "scx_protocol.h"
#include "scxlnx_defs.h"
#include "scxlnx_util.h"
#include "scxlnx_comm.h"
#include "scxlnx_conn.h"

#ifdef CONFIG_TF_MSHIELD
#include "scx_public_crypto.h"
#endif

/*----------------------------------------------------------------------------
 * Management of the shared memory blocks.
 *
 * Shared memory blocks are the blocks registered through
 * the commands REGISTER_SHARED_MEMORY and POWER_MANAGEMENT
 *----------------------------------------------------------------------------*/

/**
 * Unmaps a shared memory
 **/
static void SCXLNXConnUnmapShmem(
		struct SCXLNX_CONNECTION *pConn,
		struct SCXLNX_SHMEM_DESC *pShmemDesc,
		u32 nFullCleanup)
{
	/* check pShmemDesc contains a descriptor */
	if (pShmemDesc == NULL)
		return;

	dprintk(KERN_DEBUG "SCXLNXConnUnmapShmem(%p)\n", pShmemDesc);

retry:
	mutex_lock(&(pConn->sharedMemoriesMutex));
	if (atomic_read(&pShmemDesc->nRefCnt) > 1) {
		/*
		 * Shared mem still in use, wait for other operations completion
		 * before actually unmapping it.
		 */
		dprintk(KERN_INFO "Descriptor in use\n");
		mutex_unlock(&(pConn->sharedMemoriesMutex));
		schedule();
		goto retry;
	}

	SCXLNXCommReleaseSharedMemory(
			&(pConn->sAllocationContext),
			pShmemDesc,
			nFullCleanup);

	list_del(&(pShmemDesc->list));

	if ((pShmemDesc->nType == SCXLNX_SHMEM_TYPE_REGISTERED_SHMEM) ||
			(nFullCleanup != 0)) {
		internal_kfree(pShmemDesc);

		atomic_dec(&(pConn->nShmemAllocated));
	} else {
		/*
		 * This is a preallocated shared memory, add to free list
		 * Since the device context is unmapped last, it is
		 * always the first element of the free list if no
		 * device context has been created
		 */
		pShmemDesc->hIdentifier = 0;
		list_add(&(pShmemDesc->list), &(pConn->sFreeSharedMemoryList));
	}

	mutex_unlock(&(pConn->sharedMemoriesMutex));
}


/**
 * Find the first available slot for a new block of shared memory
 * and map the user buffer.
 * Update the pDescriptors to L1 descriptors
 * Update the pBufferStartOffset and pBufferSize fields
 * pShmemDesc is updated to the mapped shared memory descriptor
 **/
static int SCXLNXConnMapShmem(
		struct SCXLNX_CONNECTION *pConn,
		u32 nBufferVAddr,
		/* flags for read-write access rights on the memory */
		u32 nFlags,
		bool bInUserSpace,
		u32 pDescriptors[SCX_MAX_COARSE_PAGES],
		u32 *pBufferStartOffset,
		u32 *pBufferSize,
		struct SCXLNX_SHMEM_DESC **ppShmemDesc,
		u32 *pnDescriptorCount)
{
	struct SCXLNX_SHMEM_DESC *pShmemDesc = NULL;
	int nError;

	dprintk(KERN_INFO "SCXLNXConnMapShmem(%p, %p, flags = 0x%08x)\n",
					pConn,
					(void *) nBufferVAddr,
					nFlags);

	mutex_lock(&(pConn->sharedMemoriesMutex));

	/*
	 * Check the list of free shared memory
	 * is not empty
	 */
	if (list_empty(&(pConn->sFreeSharedMemoryList))) {
		if (atomic_read(&(pConn->nShmemAllocated)) ==
				SCXLNX_SHMEM_MAX_COUNT) {
			printk(KERN_ERR "SCXLNXConnMapShmem(%p):"
				" maximum shared memories already registered\n",
				pConn);
			nError = -ENOMEM;
			goto error;
		}

		atomic_inc(&(pConn->nShmemAllocated));

		/* no descriptor available, allocate a new one */

		pShmemDesc = (struct SCXLNX_SHMEM_DESC *) internal_kmalloc(
			sizeof(*pShmemDesc), GFP_KERNEL);
		if (pShmemDesc == NULL) {
			printk(KERN_ERR "SCXLNXConnMapShmem(%p):"
				" failed to allocate descriptor\n",
				pConn);
			nError = -ENOMEM;
			goto error;
		}

		/* Initialize the structure */
		pShmemDesc->nType = SCXLNX_SHMEM_TYPE_REGISTERED_SHMEM;
		atomic_set(&pShmemDesc->nRefCnt, 1);
		INIT_LIST_HEAD(&(pShmemDesc->list));
	} else {
		/* take the first free shared memory descriptor */
		pShmemDesc = list_entry(pConn->sFreeSharedMemoryList.next,
			struct SCXLNX_SHMEM_DESC, list);
		list_del(&(pShmemDesc->list));
	}

	/* Add the descriptor to the used list */
	list_add(&(pShmemDesc->list), &(pConn->sUsedSharedMemoryList));

	nError = SCXLNXCommFillDescriptorTable(
			&(pConn->sAllocationContext),
			pShmemDesc,
			nBufferVAddr,
			pConn->ppVmas,
			pDescriptors,
			pBufferSize,
			pBufferStartOffset,
			bInUserSpace,
			nFlags,
			pnDescriptorCount);

	if (nError != 0) {
		dprintk(KERN_ERR "SCXLNXConnMapShmem(%p):"
			" SCXLNXCommFillDescriptorTable failed with error "
			"code %d!\n",
			pConn,
			nError);
		goto error;
	}
	pShmemDesc->pBuffer = (u8 *) nBufferVAddr;

	/*
	 * Successful completion.
	 */
	*ppShmemDesc = pShmemDesc;
	mutex_unlock(&(pConn->sharedMemoriesMutex));
	dprintk(KERN_DEBUG "SCXLNXConnMapShmem: success\n");
	return 0;


	/*
	 * Error handling.
	 */
error:
	mutex_unlock(&(pConn->sharedMemoriesMutex));
	dprintk(KERN_ERR "SCXLNXConnMapShmem: failure with error code %d\n",
		nError);

	SCXLNXConnUnmapShmem(
			pConn,
			pShmemDesc,
			0);

	return nError;
}



/* This function is a copy of the find_vma() function
in linux kernel 2.6.15 version with some fixes :
	- memory block may end on vm_end
	- check the full memory block is in the memory area
	- guarantee NULL is returned if no memory area is found */
struct vm_area_struct *SCXLNXConnFindVma(struct mm_struct *mm,
	unsigned long addr, unsigned long size)
{
	struct vm_area_struct *vma = NULL;

	dprintk(KERN_INFO
		"SCXLNXConnFindVma addr=0x%lX size=0x%lX\n", addr, size);

	if (mm) {
		/* Check the cache first. */
		/* (Cache hit rate is typically around 35%.) */
		vma = mm->mmap_cache;
		if (!(vma && vma->vm_end >= (addr+size) &&
				vma->vm_start <= addr))	{
			struct rb_node *rb_node;

			rb_node = mm->mm_rb.rb_node;
			vma = NULL;

			while (rb_node) {
				struct vm_area_struct *vma_tmp;

				vma_tmp = rb_entry(rb_node,
					struct vm_area_struct, vm_rb);

				dprintk(KERN_INFO
					"vma_tmp->vm_start=0x%lX"
					"vma_tmp->vm_end=0x%lX\n",
					vma_tmp->vm_start,
					vma_tmp->vm_end);

				if (vma_tmp->vm_end >= (addr+size)) {
					vma = vma_tmp;
					if (vma_tmp->vm_start <= addr)
						break;

					rb_node = rb_node->rb_left;
				} else {
					rb_node = rb_node->rb_right;
				}
			}

			if (vma)
				mm->mmap_cache = vma;
			if (rb_node == NULL)
				vma = NULL;
		}
	}
	return vma;
}

static int SCXLNXConnValidateSharedMemoryBlockAndFlags(
	void *pSharedMemory,
	u32 nSharedMemorySize,
	u32 nFlags)
{
	struct vm_area_struct *vma;
	unsigned long nSharedMemory = (unsigned long) pSharedMemory;
	u32 nChunk;

	if (nSharedMemorySize == 0)
		/* This is always valid */
		return 0;

	if ((nSharedMemory + nSharedMemorySize) < nSharedMemory)
		/* Overflow */
		return -EINVAL;

	down_read(&current->mm->mmap_sem);

	/*
	 *  When looking for a memory address, split buffer into chunks of
	 *  size=PAGE_SIZE.
	 */
	nChunk = PAGE_SIZE - (nSharedMemory & (PAGE_SIZE-1));
	if (nChunk > nSharedMemorySize)
		nChunk = nSharedMemorySize;

	do {
		vma = SCXLNXConnFindVma(current->mm, nSharedMemory, nChunk);

		if (vma == NULL)
			goto error;

		if (nFlags & SCX_SHMEM_TYPE_READ)
			if (!(vma->vm_flags & VM_READ))
				goto error;
		if (nFlags & SCX_SHMEM_TYPE_WRITE)
			if (!(vma->vm_flags & VM_WRITE))
				goto error;

		nSharedMemorySize -= nChunk;
		nSharedMemory += nChunk;
		nChunk = (nSharedMemorySize <= PAGE_SIZE ?
				nSharedMemorySize : PAGE_SIZE);
	} while (nSharedMemorySize != 0);

	up_read(&current->mm->mmap_sem);
	return 0;

error:
	up_read(&current->mm->mmap_sem);
	dprintk(KERN_ERR "SCXLNXConnValidateSharedMemoryBlockAndFlags: "
		"return error\n");
	return -EFAULT;
}


static int SCXLNXConnMapTempShMem(struct SCXLNX_CONNECTION *pConn,
	 struct SCX_COMMAND_PARAM_TEMP_MEMREF *pTempMemRef,
	 u32 nParamType,
	 struct SCXLNX_SHMEM_DESC **ppShmemDesc)
{
	u32 nFlags;
	u32 nError = S_SUCCESS;

	dprintk(KERN_INFO "SCXLNXConnMapTempShMem(%p, "
		"0x%08x[size=0x%08x], offset=0x%08x)\n",
		pConn,
		pTempMemRef->nDescriptor,
		pTempMemRef->nSize,
		pTempMemRef->nOffset);

	switch (nParamType) {
	case SCX_PARAM_TYPE_MEMREF_TEMP_INPUT:
		nFlags = SCX_SHMEM_TYPE_READ;
		break;
	case SCX_PARAM_TYPE_MEMREF_TEMP_OUTPUT:
		nFlags = SCX_SHMEM_TYPE_WRITE;
		break;
	case SCX_PARAM_TYPE_MEMREF_TEMP_INOUT:
		nFlags = SCX_SHMEM_TYPE_WRITE | SCX_SHMEM_TYPE_READ;
		break;
	default:
		nError = -EINVAL;
		goto error;
	}

	if (pTempMemRef->nDescriptor == 0) {
		/* NULL tmpref */
		pTempMemRef->nOffset = 0;
		*ppShmemDesc = NULL;
	} else if ((pTempMemRef->nDescriptor != 0) &&
			(pTempMemRef->nSize == 0)) {
		/* Empty tmpref */
		pTempMemRef->nOffset = pTempMemRef->nDescriptor;
		pTempMemRef->nDescriptor = 0;
		pTempMemRef->nSize = 0;
		*ppShmemDesc = NULL;
	} else {
		/* Map the temp shmem block */

		u32 nSharedMemDescriptors[SCX_MAX_COARSE_PAGES];
		u32 nDescriptorCount;

		nError = SCXLNXConnValidateSharedMemoryBlockAndFlags(
				(void *) pTempMemRef->nDescriptor,
				pTempMemRef->nSize,
				nFlags);
		if (nError != 0)
			goto error;

		nError = SCXLNXConnMapShmem(
				pConn,
				pTempMemRef->nDescriptor,
				nFlags,
				true,
				nSharedMemDescriptors,
				&(pTempMemRef->nOffset),
				&(pTempMemRef->nSize),
				ppShmemDesc,
				&nDescriptorCount);
		pTempMemRef->nDescriptor = nSharedMemDescriptors[0];
	 }

error:
	 return nError;
}

/*
 * Clean up a list of shared memory descriptors.
 */
static void SCXLNXSharedMemoryCleanupList(
		struct SCXLNX_CONNECTION *pConn,
		struct list_head *pList)
{
	while (!list_empty(pList)) {
		struct SCXLNX_SHMEM_DESC *pShmemDesc;

		pShmemDesc = list_entry(pList->next, struct SCXLNX_SHMEM_DESC,
			list);

		SCXLNXConnUnmapShmem(pConn, pShmemDesc, 1);
	}
}


/*
 * Clean up the shared memory information in the connection.
 * Releases all allocated pages.
 */
void SCXLNXConnCleanupSharedMemory(struct SCXLNX_CONNECTION *pConn)
{
	/* clean up the list of used and free descriptors.
	 * done outside the mutex, because SCXLNXConnUnmapShmem already
	 * mutex()ed
	 */
	SCXLNXSharedMemoryCleanupList(pConn,
		&pConn->sUsedSharedMemoryList);
	SCXLNXSharedMemoryCleanupList(pConn,
		&pConn->sFreeSharedMemoryList);

	mutex_lock(&(pConn->sharedMemoriesMutex));

	/* Free the Vmas page */
	if (pConn->ppVmas) {
		internal_free_page((unsigned long) pConn->ppVmas);
		pConn->ppVmas = NULL;
	}

	SCXLNXReleaseCoarsePageTableAllocator(
		&(pConn->sAllocationContext));

	mutex_unlock(&(pConn->sharedMemoriesMutex));
}


/*
 * Initialize the shared memory in a connection.
 * Allocates the minimum memory to be provided
 * for shared memory management
 */
int SCXLNXConnInitSharedMemory(struct SCXLNX_CONNECTION *pConn)
{
	int nError;
	int nSharedMemoryDescriptorIndex;
	int nCoarsePageIndex;

	/*
	 * We only need to initialize special elements and attempt to allocate
	 * the minimum shared memory descriptors we want to support
	 */

	mutex_init(&(pConn->sharedMemoriesMutex));
	INIT_LIST_HEAD(&(pConn->sFreeSharedMemoryList));
	INIT_LIST_HEAD(&(pConn->sUsedSharedMemoryList));
	atomic_set(&(pConn->nShmemAllocated), 0);

	SCXLNXInitializeCoarsePageTableAllocator(
		&(pConn->sAllocationContext));


	/*
	 * Preallocate 3 pages to increase the chances that a connection
	 * succeeds in allocating shared mem
	 */
	for (nSharedMemoryDescriptorIndex = 0;
	     nSharedMemoryDescriptorIndex < 3;
	     nSharedMemoryDescriptorIndex++) {
		struct SCXLNX_SHMEM_DESC *pShmemDesc =
			(struct SCXLNX_SHMEM_DESC *) internal_kmalloc(
				sizeof(*pShmemDesc), GFP_KERNEL);

		if (pShmemDesc == NULL) {
			printk(KERN_ERR "SCXLNXConnInitSharedMemory(%p):"
				" failed to pre allocate descriptor %d\n",
				pConn,
				nSharedMemoryDescriptorIndex);
			nError = -ENOMEM;
			goto error;
		}

		for (nCoarsePageIndex = 0;
		     nCoarsePageIndex < SCX_MAX_COARSE_PAGES;
		     nCoarsePageIndex++) {
			struct SCXLNX_COARSE_PAGE_TABLE *pCoarsePageTable;

			pCoarsePageTable = SCXLNXAllocateCoarsePageTable(
				&(pConn->sAllocationContext),
				SCXLNX_PAGE_DESCRIPTOR_TYPE_PREALLOCATED);

			if (pCoarsePageTable == NULL) {
				printk(KERN_ERR "SCXLNXConnInitSharedMemory(%p)"
					": descriptor %d coarse page %d - "
					"SCXLNXConnAllocateCoarsePageTable() "
					"failed\n",
					pConn,
					nSharedMemoryDescriptorIndex,
					nCoarsePageIndex);
				nError = -ENOMEM;
				goto error;
			}

			pShmemDesc->pCoarsePageTable[nCoarsePageIndex] =
				pCoarsePageTable;
		}
		pShmemDesc->nNumberOfCoarsePageTables = 0;

		pShmemDesc->nType = SCXLNX_SHMEM_TYPE_PREALLOC_REGISTERED_SHMEM;
		atomic_set(&pShmemDesc->nRefCnt, 1);

		/*
		 * add this preallocated descriptor to the list of free
		 * descriptors Keep the device context specific one at the
		 * beginning of the list
		 */
		INIT_LIST_HEAD(&(pShmemDesc->list));
		list_add_tail(&(pShmemDesc->list),
			&(pConn->sFreeSharedMemoryList));
	}

	/* allocate memory for the vmas structure */
	pConn->ppVmas =
		(struct vm_area_struct **) internal_get_zeroed_page(GFP_KERNEL);
	if (pConn->ppVmas == NULL) {
		printk(KERN_ERR "SCXLNXConnInitSharedMemory(%p):"
			" ppVmas - failed to get_zeroed_page\n",
			pConn);
		nError = -ENOMEM;
		goto error;
	}

	return 0;

error:
	SCXLNXConnCleanupSharedMemory(pConn);
	return nError;
}

/*----------------------------------------------------------------------------
 * Connection operations to the Secure World
 *----------------------------------------------------------------------------*/

int SCXLNXConnCreateDeviceContext(
	struct SCXLNX_CONNECTION *pConn)
{
	union SCX_COMMAND_MESSAGE sMessage;
	union SCX_ANSWER_MESSAGE  sAnswer;
	int nError = 0;

	dprintk(KERN_INFO "SCXLNXConnCreateDeviceContext(%p)\n",
			pConn);

	sMessage.sCreateDeviceContextMessage.nMessageType =
		SCX_MESSAGE_TYPE_CREATE_DEVICE_CONTEXT;
	sMessage.sCreateDeviceContextMessage.nMessageSize =
		(sizeof(struct SCX_COMMAND_CREATE_DEVICE_CONTEXT)
			- sizeof(struct SCX_COMMAND_HEADER))/sizeof(u32);
	sMessage.sCreateDeviceContextMessage.nOperationID = (u32) &sAnswer;
	sMessage.sCreateDeviceContextMessage.nDeviceContextID = (u32) pConn;

	nError = SCXLNXCommSendReceive(
		&pConn->pDevice->sm,
		&sMessage,
		&sAnswer,
		pConn,
		true);

	if ((nError != 0) ||
		(sAnswer.sCreateDeviceContextAnswer.nErrorCode != S_SUCCESS))
		goto error;

	/*
	 * CREATE_DEVICE_CONTEXT succeeded,
	 * store device context handler and update connection status
	 */
	pConn->hDeviceContext =
		sAnswer.sCreateDeviceContextAnswer.hDeviceContext;
	spin_lock(&(pConn->stateLock));
	pConn->nState = SCXLNX_CONN_STATE_VALID_DEVICE_CONTEXT;
	spin_unlock(&(pConn->stateLock));

	/* successful completion */
	dprintk(KERN_INFO "SCXLNXConnCreateDeviceContext(%p):"
		" hDeviceContext=0x%08x\n",
		pConn,
		sAnswer.sCreateDeviceContextAnswer.hDeviceContext);
	return 0;

error:
	if (nError != 0) {
		dprintk(KERN_ERR "SCXLNXConnCreateDeviceContext failed with "
			"error %d\n", nError);
	} else {
		/*
		 * We sent a DeviceCreateContext. The state is now
		 * SCXLNX_CONN_STATE_CREATE_DEVICE_CONTEXT_SENT It has to be
		 * reset if we ever want to send a DeviceCreateContext again
		 */
		spin_lock(&(pConn->stateLock));
		pConn->nState = SCXLNX_CONN_STATE_NO_DEVICE_CONTEXT;
		spin_unlock(&(pConn->stateLock));
		dprintk(KERN_ERR "SCXLNXConnCreateDeviceContext failed with "
			"nErrorCode 0x%08X\n",
			sAnswer.sCreateDeviceContextAnswer.nErrorCode);
		if (sAnswer.sCreateDeviceContextAnswer.nErrorCode ==
			S_ERROR_OUT_OF_MEMORY)
			nError = -ENOMEM;
		else
			nError = -EFAULT;
	}

   return nError;
}

/* Check that the current application belongs to the
 * requested GID */
static bool SCXLNXConnCheckGID(gid_t nRequestedGID)
{
	if (nRequestedGID == current_egid()) {
		return true;
	} else {
		u32    nSize;
		u32    i;
		/* Look in the supplementary GIDs */
		get_group_info(GROUP_INFO);
		nSize = GROUP_INFO->ngroups;
		for (i = 0; i < nSize; i++)
			if (nRequestedGID == GROUP_AT(GROUP_INFO , i))
				return true;
	}
	return false;
}

/*
 * Opens a client session to the Secure World
 */
int SCXLNXConnOpenClientSession(
	struct SCXLNX_CONNECTION *pConn,
	union SCX_COMMAND_MESSAGE *pMessage,
	union SCX_ANSWER_MESSAGE *pAnswer)
{
	int nError = 0;
	struct SCXLNX_SHMEM_DESC *pShmemDesc[4] = {NULL};
	u32 i;

	dprintk(KERN_INFO "SCXLNXConnOpenClientSession(%p)\n", pConn);

	/*
	 * Initialize the message size with no login data. This will be later
	 * adjusted the the cases below
	 */
	pMessage->sOpenClientSessionMessage.nMessageSize =
		(sizeof(struct SCX_COMMAND_OPEN_CLIENT_SESSION) - 20
			- sizeof(struct SCX_COMMAND_HEADER))/4;

	switch (pMessage->sOpenClientSessionMessage.nLoginType) {
	case SCX_LOGIN_PUBLIC:
		 /* Nothing to do */
		 break;

	case SCX_LOGIN_USER:
		/*
		 * Send the EUID of the calling application in the login data.
		 * Update message size.
		 */
		*(u32 *) &pMessage->sOpenClientSessionMessage.sLoginData =
			current_euid();
#ifndef CONFIG_ANDROID
		pMessage->sOpenClientSessionMessage.nLoginType =
			(u32) SCX_LOGIN_USER_LINUX_EUID;
#else
		pMessage->sOpenClientSessionMessage.nLoginType =
			(u32) SCX_LOGIN_USER_ANDROID_EUID;
#endif

		/* Added one word */
		pMessage->sOpenClientSessionMessage.nMessageSize += 1;
		break;

	case SCX_LOGIN_GROUP: {
		/* Check requested GID */
		gid_t  nRequestedGID =
			*(u32 *) pMessage->sOpenClientSessionMessage.sLoginData;

		if (!SCXLNXConnCheckGID(nRequestedGID)) {
			dprintk(KERN_ERR "SCXLNXConnOpenClientSession(%p) "
				"SCX_LOGIN_GROUP: requested GID (0x%x) does "
				"not match real eGID (0x%x)"
				"or any of the supplementary GIDs\n",
				pConn, nRequestedGID, current_egid());
			nError = -EACCES;
			goto error;
		}
#ifndef CONFIG_ANDROID
		pMessage->sOpenClientSessionMessage.nLoginType =
			SCX_LOGIN_GROUP_LINUX_GID;
#else
		pMessage->sOpenClientSessionMessage.nLoginType =
			SCX_LOGIN_GROUP_ANDROID_GID;
#endif

		pMessage->sOpenClientSessionMessage.nMessageSize += 1; /* GID */
		break;
	}

#ifndef CONFIG_ANDROID
	case SCX_LOGIN_APPLICATION: {
		/*
		 * Compute SHA-1 hash of the application fully-qualified path
		 * name.  Truncate the hash to 16 bytes and send it as login
		 * data.  Update message size.
		 */
		u8 pSHA1Hash[SHA1_DIGEST_SIZE];

		nError = SCXLNXConnHashApplicationPathAndData(pSHA1Hash,
			NULL, 0);
		if (nError != 0) {
			dprintk(KERN_ERR "SCXLNXConnOpenClientSession: "
				"error in SCXLNXConnHashApplicationPath"
				"AndData\n");
			goto error;
		}
		memcpy(&pMessage->sOpenClientSessionMessage.sLoginData,
			pSHA1Hash, 16);
		pMessage->sOpenClientSessionMessage.nLoginType =
			SCX_LOGIN_APPLICATION_LINUX_PATH_SHA1_HASH;
		/* 16 bytes */
		pMessage->sOpenClientSessionMessage.nMessageSize += 4;
		break;
	}
#else
	case SCX_LOGIN_APPLICATION:
		/*
		 * Send the real UID of the calling application in the login
		 * data. Update message size.
		 */
		*(u32 *) &pMessage->sOpenClientSessionMessage.sLoginData =
			current_uid();

		pMessage->sOpenClientSessionMessage.nLoginType =
			(u32) SCX_LOGIN_APPLICATION_ANDROID_UID;

		/* Added one word */
		pMessage->sOpenClientSessionMessage.nMessageSize += 1;
		break;
#endif

#ifndef CONFIG_ANDROID
	case SCX_LOGIN_APPLICATION_USER: {
		/*
		 * Compute SHA-1 hash of the concatenation of the application
		 * fully-qualified path name and the EUID of the calling
		 * application.  Truncate the hash to 16 bytes and send it as
		 * login data.  Update message size.
		 */
		u8 pSHA1Hash[SHA1_DIGEST_SIZE];

		nError = SCXLNXConnHashApplicationPathAndData(pSHA1Hash,
			(u8 *) &(current_euid()), sizeof(current_euid()));
		if (nError != 0) {
			dprintk(KERN_ERR "SCXLNXConnOpenClientSession: "
				"error in SCXLNXConnHashApplicationPath"
				"AndData\n");
			goto error;
		}
		memcpy(&pMessage->sOpenClientSessionMessage.sLoginData,
			pSHA1Hash, 16);
		pMessage->sOpenClientSessionMessage.nLoginType =
			SCX_LOGIN_APPLICATION_USER_LINUX_PATH_EUID_SHA1_HASH;

		/* 16 bytes */
		pMessage->sOpenClientSessionMessage.nMessageSize += 4;

		break;
	}
#else
	case SCX_LOGIN_APPLICATION_USER:
		/*
		 * Send the real UID and the EUID of the calling application in
		 * the login data. Update message size.
		 */
		*(u32 *) &pMessage->sOpenClientSessionMessage.sLoginData =
			current_uid();
		*(u32 *) &pMessage->sOpenClientSessionMessage.sLoginData[4] =
			current_euid();

		pMessage->sOpenClientSessionMessage.nLoginType =
			SCX_LOGIN_APPLICATION_USER_ANDROID_UID_EUID;

		/* Added two words */
		pMessage->sOpenClientSessionMessage.nMessageSize += 2;
		break;
#endif

#ifndef CONFIG_ANDROID
	case SCX_LOGIN_APPLICATION_GROUP: {
		/*
		 * Check requested GID.  Compute SHA-1 hash of the concatenation
		 * of the application fully-qualified path name and the
		 * requested GID.  Update message size
		 */
		gid_t  nRequestedGID;
		u8     pSHA1Hash[SHA1_DIGEST_SIZE];

		nRequestedGID =	*(u32 *) &pMessage->sOpenClientSessionMessage.
			sLoginData;

		if (!SCXLNXConnCheckGID(nRequestedGID)) {
			dprintk(KERN_ERR "SCXLNXConnOpenClientSession(%p) "
			"SCX_LOGIN_APPLICATION_GROUP: requested GID (0x%x) "
			"does not match real eGID (0x%x)"
			"or any of the supplementary GIDs\n",
			pConn, nRequestedGID, current_egid());
			nError = -EACCES;
			goto error;
		}

		nError = SCXLNXConnHashApplicationPathAndData(pSHA1Hash,
			&nRequestedGID, sizeof(u32));
		if (nError != 0) {
			dprintk(KERN_ERR "SCXLNXConnOpenClientSession: "
				"error in SCXLNXConnHashApplicationPath"
				"AndData\n");
			goto error;
		}

		memcpy(&pMessage->sOpenClientSessionMessage.sLoginData,
			pSHA1Hash, 16);
		pMessage->sOpenClientSessionMessage.nLoginType =
			SCX_LOGIN_APPLICATION_GROUP_LINUX_PATH_GID_SHA1_HASH;

		/* 16 bytes */
		pMessage->sOpenClientSessionMessage.nMessageSize += 4;
		break;
	}
#else
	case SCX_LOGIN_APPLICATION_GROUP: {
		/*
		 * Check requested GID. Send the real UID and the requested GID
		 * in the login data. Update message size.
		 */
		gid_t nRequestedGID;

		nRequestedGID =	*(u32 *) &pMessage->sOpenClientSessionMessage.
			sLoginData;

		if (!SCXLNXConnCheckGID(nRequestedGID)) {
			dprintk(KERN_ERR "SCXLNXConnOpenClientSession(%p) "
			"SCX_LOGIN_APPLICATION_GROUP: requested GID (0x%x) "
			"does not match real eGID (0x%x)"
			"or any of the supplementary GIDs\n",
			pConn, nRequestedGID, current_egid());
			nError = -EACCES;
			goto error;
		}

		*(u32 *) &pMessage->sOpenClientSessionMessage.sLoginData =
			current_uid();
		*(u32 *) &pMessage->sOpenClientSessionMessage.sLoginData[4] =
			nRequestedGID;

		pMessage->sOpenClientSessionMessage.nLoginType =
			SCX_LOGIN_APPLICATION_GROUP_ANDROID_UID_GID;

		/* Added two words */
		pMessage->sOpenClientSessionMessage.nMessageSize += 2;

		break;
	}
#endif

	case SCX_LOGIN_PRIVILEGED:
		/*
		 * Check that calling application either hash EUID=0 or has
		 * EGID=0
		 */
		if (current_euid() != 0 && current_egid() != 0) {
			dprintk(KERN_ERR "SCXLNXConnOpenClientSession: "
				" user %d, group %d not allowed to open "
				"session with SCX_LOGIN_PRIVILEGED\n",
				current_euid(), current_egid());
			nError = -EACCES;
			goto error;
		}
		pMessage->sOpenClientSessionMessage.nLoginType =
			SCX_LOGIN_PRIVILEGED;
		break;

	case SCX_LOGIN_AUTHENTICATION: {
		/*
		 * Compute SHA-1 hash of the application binary
		 * Send this hash as the login data (20 bytes)
		 */

		u8 *pHash;
		pHash = &(pMessage->sOpenClientSessionMessage.sLoginData[0]);

		nError = SCXLNXConnGetCurrentProcessHash(pHash);
		if (nError != 0) {
			dprintk(KERN_ERR "SCXLNXConnOpenClientSession: "
				"error in SCXLNXConnGetCurrentProcessHash\n");
			goto error;
		}
		pMessage->sOpenClientSessionMessage.nLoginType =
			SCX_LOGIN_AUTHENTICATION_BINARY_SHA1_HASH;

		/* 20 bytes */
		pMessage->sOpenClientSessionMessage.nMessageSize += 5;
		break;
	}

	default:
		 dprintk(KERN_ERR "SCXLNXConnOpenClientSession: "
			"unknown nLoginType(%08X)\n",
			pMessage->sOpenClientSessionMessage.nLoginType);
		 nError = -EOPNOTSUPP;
		 goto error;
	}

	/* Map the temporary memory references */
	for (i = 0; i < 4; i++) {
		int nParamType;
		nParamType = SCX_GET_PARAM_TYPE(
			pMessage->sOpenClientSessionMessage.nParamTypes, i);
		if ((nParamType & (SCX_PARAM_TYPE_MEMREF_FLAG |
				   SCX_PARAM_TYPE_REGISTERED_MEMREF_FLAG))
				== SCX_PARAM_TYPE_MEMREF_FLAG) {
			/* Map temp mem ref */
			nError = SCXLNXConnMapTempShMem(pConn,
				&pMessage->sOpenClientSessionMessage.
					sParams[i].sTempMemref,
				nParamType,
				&pShmemDesc[i]);
			if (nError != 0) {
				dprintk(KERN_ERR "SCXLNXConnOpenClientSession: "
					"unable to map temporary memory block "
					"(%08X)\n", nError);
				goto error;
			}
		}
	}

	/* Fill the handle of the Device Context */
	pMessage->sOpenClientSessionMessage.hDeviceContext =
		pConn->hDeviceContext;

	nError = SCXLNXCommSendReceive(
		&pConn->pDevice->sm,
		pMessage,
		pAnswer,
		pConn,
		true);

error:
	/* Unmap the temporary memory references */
	for (i = 0; i < 4; i++)
		if (pShmemDesc[i] != NULL)
			SCXLNXConnUnmapShmem(pConn, pShmemDesc[i], 0);

	if (nError != 0)
		dprintk(KERN_ERR "SCXLNXConnOpenClientSession returns %d\n",
			nError);
	else
		dprintk(KERN_ERR "SCXLNXConnOpenClientSession returns "
			"nErrorCode 0x%08X\n",
			pAnswer->sOpenClientSessionAnswer.nErrorCode);

	return nError;
}


/*
 * Closes a client session from the Secure World
 */
int SCXLNXConnCloseClientSession(
	struct SCXLNX_CONNECTION *pConn,
	union SCX_COMMAND_MESSAGE *pMessage,
	union SCX_ANSWER_MESSAGE *pAnswer)
{
	int nError = 0;

	dprintk(KERN_DEBUG "SCXLNXConnCloseClientSession(%p)\n", pConn);

	pMessage->sCloseClientSessionMessage.nMessageSize =
		(sizeof(struct SCX_COMMAND_CLOSE_CLIENT_SESSION) -
			sizeof(struct SCX_COMMAND_HEADER)) / 4;
	pMessage->sCloseClientSessionMessage.hDeviceContext =
		pConn->hDeviceContext;

	nError = SCXLNXCommSendReceive(
		&pConn->pDevice->sm,
		pMessage,
		pAnswer,
		pConn,
		true);

	if (nError != 0)
		dprintk(KERN_ERR "SCXLNXConnCloseClientSession returns %d\n",
			nError);
	else
		dprintk(KERN_ERR "SCXLNXConnCloseClientSession returns "
			"nError 0x%08X\n",
			pAnswer->sCloseClientSessionAnswer.nErrorCode);

	return nError;
}


/*
 * Registers a shared memory to the Secure World
 */
int SCXLNXConnRegisterSharedMemory(
	struct SCXLNX_CONNECTION *pConn,
	union SCX_COMMAND_MESSAGE *pMessage,
	union SCX_ANSWER_MESSAGE *pAnswer)
{
	int nError = 0;
	struct SCXLNX_SHMEM_DESC *pShmemDesc = NULL;

	dprintk(KERN_INFO "SCXLNXConnRegisterSharedMemory(%p) "
		"%p[0x%08X][0x%08x]\n",
		pConn,
		(void *) pMessage->sRegisterSharedMemoryMessage.
			nSharedMemDescriptors[0],
		pMessage->sRegisterSharedMemoryMessage.nSharedMemSize,
		(u32)pMessage->sRegisterSharedMemoryMessage.nMemoryFlags);

	nError = SCXLNXConnValidateSharedMemoryBlockAndFlags(
		(void *) pMessage->sRegisterSharedMemoryMessage.
			nSharedMemDescriptors[0],
		pMessage->sRegisterSharedMemoryMessage.nSharedMemSize,
		(u32)pMessage->sRegisterSharedMemoryMessage.nMemoryFlags);
	if (nError != 0)
		goto error;

	/* Initialize nMessageSize with no descriptors */
	pMessage->sRegisterSharedMemoryMessage.nMessageSize
		= (sizeof(struct SCX_COMMAND_REGISTER_SHARED_MEMORY) -
			sizeof(struct SCX_COMMAND_HEADER)) / 4;

	/* Map the shmem block and update the message */
	if (pMessage->sRegisterSharedMemoryMessage.nSharedMemSize == 0) {
		/* Empty shared mem */
		pMessage->sRegisterSharedMemoryMessage.nSharedMemStartOffset =
			pMessage->sRegisterSharedMemoryMessage.
				nSharedMemDescriptors[0];
	} else {
		u32 nDescriptorCount;
		nError = SCXLNXConnMapShmem(
			pConn,
			pMessage->sRegisterSharedMemoryMessage.
				nSharedMemDescriptors[0],
			pMessage->sRegisterSharedMemoryMessage.nMemoryFlags,
			true,
			pMessage->sRegisterSharedMemoryMessage.
				nSharedMemDescriptors,
			&(pMessage->sRegisterSharedMemoryMessage.
				nSharedMemStartOffset),
			&(pMessage->sRegisterSharedMemoryMessage.
				nSharedMemSize),
			&pShmemDesc,
			&nDescriptorCount);
		if (nError != 0) {
			dprintk(KERN_ERR "SCXLNXConnRegisterSharedMemory: "
				"unable to map shared memory block\n");
			goto error;
		}
		pMessage->sRegisterSharedMemoryMessage.nMessageSize +=
			nDescriptorCount;
	}

	/*
	 * write the correct device context handle and the address of the shared
	 * memory descriptor in the message
	 */
	pMessage->sRegisterSharedMemoryMessage.hDeviceContext =
		 pConn->hDeviceContext;
	pMessage->sRegisterSharedMemoryMessage.nBlockID = (u32) pShmemDesc;

	/* Send the updated message */
	nError = SCXLNXCommSendReceive(
		&pConn->pDevice->sm,
		pMessage,
		pAnswer,
		pConn,
		true);

	if ((nError != 0) ||
		(pAnswer->sRegisterSharedMemoryAnswer.nErrorCode
			!= S_SUCCESS)) {
		dprintk(KERN_ERR "SCXLNXConnRegisterSharedMemory: "
			"operation failed. Unmap block\n");
		goto error;
	}

	/* Saves the block handle returned by the secure world */
	if (pShmemDesc != NULL)
		pShmemDesc->hIdentifier =
			pAnswer->sRegisterSharedMemoryAnswer.hBlock;

	/* successful completion */
	dprintk(KERN_INFO "SCXLNXConnRegisterSharedMemory(%p):"
		" nBlockID=0x%08x hBlock=0x%08x\n",
		pConn, pMessage->sRegisterSharedMemoryMessage.nBlockID,
		pAnswer->sRegisterSharedMemoryAnswer.hBlock);
	return 0;

	/* error completion */
error:
	SCXLNXConnUnmapShmem(
		pConn,
		pShmemDesc,
		0);

	if (nError != 0)
		dprintk(KERN_ERR "SCXLNXConnRegisterSharedMemory returns %d\n",
			nError);
	else
		dprintk(KERN_ERR "SCXLNXConnRegisterSharedMemory returns "
			"nErrorCode 0x%08X\n",
			pAnswer->sRegisterSharedMemoryAnswer.nErrorCode);

	return nError;
}


/*
 * Releases a shared memory from the Secure World
 */
int SCXLNXConnReleaseSharedMemory(
	struct SCXLNX_CONNECTION *pConn,
	union SCX_COMMAND_MESSAGE *pMessage,
	union SCX_ANSWER_MESSAGE *pAnswer)
{
	int nError = 0;

	dprintk(KERN_DEBUG "SCXLNXConnReleaseSharedMemory(%p)\n", pConn);

	pMessage->sReleaseSharedMemoryMessage.nMessageSize =
		(sizeof(struct SCX_COMMAND_RELEASE_SHARED_MEMORY) -
			sizeof(struct SCX_COMMAND_HEADER)) / 4;
	pMessage->sReleaseSharedMemoryMessage.hDeviceContext =
		pConn->hDeviceContext;

	nError = SCXLNXCommSendReceive(
		&pConn->pDevice->sm,
		pMessage,
		pAnswer,
		pConn,
		true);

	if ((nError != 0) ||
		(pAnswer->sReleaseSharedMemoryAnswer.nErrorCode != S_SUCCESS))
		goto error;

	/* Use nBlockID to get back the pointer to pShmemDesc */
	SCXLNXConnUnmapShmem(
		pConn,
		(struct SCXLNX_SHMEM_DESC *)
			pAnswer->sReleaseSharedMemoryAnswer.nBlockID,
		0);

	/* successful completion */
	dprintk(KERN_INFO "SCXLNXConnReleaseSharedMemory(%p):"
		" nBlockID=0x%08x hBlock=0x%08x\n",
		pConn, pAnswer->sReleaseSharedMemoryAnswer.nBlockID,
		pMessage->sReleaseSharedMemoryMessage.hBlock);
	return 0;


error:
	if (nError != 0)
		dprintk(KERN_ERR "SCXLNXConnReleaseSharedMemory returns %d\n",
			nError);
	else
		dprintk(KERN_ERR "SCXLNXConnReleaseSharedMemory returns "
			"nChannelStatus 0x%08X\n",
			pAnswer->sReleaseSharedMemoryAnswer.nErrorCode);

	return nError;

}


/*
 * Invokes a client command to the Secure World
 */
int SCXLNXConnInvokeClientCommand(
	struct SCXLNX_CONNECTION *pConn,
	union SCX_COMMAND_MESSAGE *pMessage,
	union SCX_ANSWER_MESSAGE *pAnswer)
{
	int nError = 0;
	struct SCXLNX_SHMEM_DESC *pShmemDesc[4] = {NULL};
	int i;

	dprintk(KERN_INFO "SCXLNXConnInvokeClientCommand(%p)\n", pConn);

	pMessage->sReleaseSharedMemoryMessage.nMessageSize =
		(sizeof(struct SCX_COMMAND_INVOKE_CLIENT_COMMAND) -
			sizeof(struct SCX_COMMAND_HEADER)) / 4;

#ifdef CONFIG_TF_MSHIELD
	nError = SCXPublicCryptoTryShortcutedUpdate(pConn,
		(struct SCX_COMMAND_INVOKE_CLIENT_COMMAND *) pMessage,
		(struct SCX_ANSWER_INVOKE_CLIENT_COMMAND *) pAnswer);
	if (nError == 0)
		return nError;
#endif

	/* Map the tmprefs */
	for (i = 0; i < 4; i++) {
		int nParamType = SCX_GET_PARAM_TYPE(
			pMessage->sInvokeClientCommandMessage.nParamTypes, i);

		if ((nParamType & (SCX_PARAM_TYPE_MEMREF_FLAG |
				   SCX_PARAM_TYPE_REGISTERED_MEMREF_FLAG))
				== SCX_PARAM_TYPE_MEMREF_FLAG) {
			/* A temporary memref: map it */
			nError = SCXLNXConnMapTempShMem(pConn,
					&pMessage->sInvokeClientCommandMessage.
						sParams[i].sTempMemref,
					nParamType, &pShmemDesc[i]);
			if (nError != 0) {
				dprintk(KERN_ERR
					"SCXLNXConnInvokeClientCommand: "
					"unable to map temporary memory "
					"block\n (%08X)", nError);
				goto error;
			}
		}
	}

	pMessage->sInvokeClientCommandMessage.hDeviceContext =
		pConn->hDeviceContext;

	nError = SCXLNXCommSendReceive(&pConn->pDevice->sm, pMessage,
		pAnswer, pConn, true);

error:
	/* Unmap de temp mem refs */
	for (i = 0; i < 4; i++) {
		if (pShmemDesc[i] != NULL) {
			dprintk(KERN_INFO "SCXLNXConnInvokeClientCommand: "
				"UnMapTempMemRef %d\n ", i);

			SCXLNXConnUnmapShmem(pConn, pShmemDesc[i], 0);
		}
	}

	if (nError != 0)
		dprintk(KERN_ERR "SCXLNXConnInvokeClientCommand returns %d\n",
			nError);
	else
		dprintk(KERN_ERR "SCXLNXConnInvokeClientCommand returns "
			"nErrorCode 0x%08X\n",
			pAnswer->sInvokeClientCommandAnswer.nErrorCode);

	return nError;
}


/*
 * Cancels a client command from the Secure World
 */
int SCXLNXConnCancelClientCommand(
	struct SCXLNX_CONNECTION *pConn,
	union SCX_COMMAND_MESSAGE *pMessage,
	union SCX_ANSWER_MESSAGE *pAnswer)
{
	int nError = 0;

	dprintk(KERN_DEBUG "SCXLNXConnCancelClientCommand(%p)\n", pConn);

	pMessage->sCancelClientOperationMessage.hDeviceContext =
		pConn->hDeviceContext;
	pMessage->sCancelClientOperationMessage.nMessageSize =
		(sizeof(struct SCX_COMMAND_CANCEL_CLIENT_OPERATION) -
			sizeof(struct SCX_COMMAND_HEADER)) / 4;

	nError = SCXLNXCommSendReceive(
		&pConn->pDevice->sm,
		pMessage,
		pAnswer,
		pConn,
		true);

	if ((nError != 0) ||
		(pAnswer->sCancelClientOperationAnswer.nErrorCode != S_SUCCESS))
		goto error;


	/* successful completion */
	return 0;

error:
	if (nError != 0)
		dprintk(KERN_ERR "SCXLNXConnCancelClientCommand returns %d\n",
			nError);
	else
		dprintk(KERN_ERR "SCXLNXConnCancelClientCommand returns "
			"nChannelStatus 0x%08X\n",
			pAnswer->sCancelClientOperationAnswer.nErrorCode);

	return nError;
}



/*
 * Destroys a device context from the Secure World
 */
int SCXLNXConnDestroyDeviceContext(
	struct SCXLNX_CONNECTION *pConn)
{
	int nError;
	/*
	 * AFY: better use the specialized SCX_COMMAND_DESTROY_DEVICE_CONTEXT
	 * structure: this will save stack
	 */
	union SCX_COMMAND_MESSAGE sMessage;
	union SCX_ANSWER_MESSAGE sAnswer;

	dprintk(KERN_INFO "SCXLNXConnDestroyDeviceContext(%p)\n", pConn);

	BUG_ON(pConn == NULL);

	sMessage.sHeader.nMessageType = SCX_MESSAGE_TYPE_DESTROY_DEVICE_CONTEXT;
	sMessage.sHeader.nMessageSize =
		(sizeof(struct SCX_COMMAND_DESTROY_DEVICE_CONTEXT) -
			sizeof(struct SCX_COMMAND_HEADER))/sizeof(u32);

	/*
	 * fill in the device context handler
	 * it is guarantied that the first shared memory descriptor describes
	 * the device context
	 */
	sMessage.sDestroyDeviceContextMessage.hDeviceContext =
		pConn->hDeviceContext;

	nError = SCXLNXCommSendReceive(
		&pConn->pDevice->sm,
		&sMessage,
		&sAnswer,
		pConn,
		false);

	if ((nError != 0) ||
		(sAnswer.sDestroyDeviceContextAnswer.nErrorCode != S_SUCCESS))
		goto error;

	spin_lock(&(pConn->stateLock));
	pConn->nState = SCXLNX_CONN_STATE_NO_DEVICE_CONTEXT;
	spin_unlock(&(pConn->stateLock));

	/* successful completion */
	dprintk(KERN_INFO "SCXLNXConnDestroyDeviceContext(%p)\n",
		pConn);
	return 0;

error:
	if (nError != 0) {
		dprintk(KERN_ERR "SCXLNXConnDestroyDeviceContext failed with "
			"error %d\n", nError);
	} else {
		dprintk(KERN_ERR "SCXLNXConnDestroyDeviceContext failed with "
			"nErrorCode 0x%08X\n",
			sAnswer.sDestroyDeviceContextAnswer.nErrorCode);
		if (sAnswer.sDestroyDeviceContextAnswer.nErrorCode ==
			S_ERROR_OUT_OF_MEMORY)
			nError = -ENOMEM;
		else
			nError = -EFAULT;
	}

   return nError;
}


/*----------------------------------------------------------------------------
 * Connection initialization and cleanup operations
 *----------------------------------------------------------------------------*/

/*
 * Opens a connection to the specified device.
 *
 * The placeholder referenced by ppConn is set to the address of the
 * new connection; it is set to NULL upon failure.
 *
 * Returns zero upon successful completion, or an appropriate error code upon
 * failure.
 */
int SCXLNXConnOpen(struct SCXLNX_DEVICE *pDevice,
	struct file *file,
	struct SCXLNX_CONNECTION **ppConn)
{
	int nError;
	struct SCXLNX_CONNECTION *pConn = NULL;

	dprintk(KERN_INFO "SCXLNXConnOpen(%p, %p)\n", file, ppConn);

	/*
	 * Allocate and initialize the connection.
	 * kmalloc only allocates sizeof(*pConn) virtual memory
	 */
	pConn = (struct SCXLNX_CONNECTION *) internal_kmalloc(sizeof(*pConn),
		GFP_KERNEL);
	if (pConn == NULL) {
		printk(KERN_ERR "SCXLNXConnOpen(): "
			"Out of memory for connection!\n");
		nError = -ENOMEM;
		goto error;
	}

	memset(pConn, 0, sizeof(*pConn));

	INIT_LIST_HEAD(&(pConn->list));
	pConn->nState = SCXLNX_CONN_STATE_NO_DEVICE_CONTEXT;
	pConn->pDevice = pDevice;
	spin_lock_init(&(pConn->stateLock));
	atomic_set(&(pConn->nPendingOpCounter), 0);

	/*
	 * Initialize the shared memory
	 */
	nError = SCXLNXConnInitSharedMemory(pConn);
	if (nError != 0)
		goto error;

#ifdef CONFIG_TF_MSHIELD
	/*
	 * Initialize CUS specifics
	 */
	SCXPublicCryptoInitDeviceContext(pConn);
#endif

	/*
	 * Successful completion.
	 */

	*ppConn = pConn;

	dprintk(KERN_INFO "SCXLNXConnOpen(): Success (pConn=%p)\n", pConn);
	return 0;

	/*
	 * Error handling.
	 */

error:
	dprintk(KERN_ERR "SCXLNXConnOpen(): Failure (error %d)\n", nError);
	/* Deallocate the descriptor pages if necessary */
	internal_kfree(pConn);
	*ppConn = NULL;
	return nError;
}


/*
 * Closes the specified connection.
 *
 * Upon return, the connection referenced by pConn has been destroyed and cannot
 * be used anymore.
 *
 * This function does nothing if pConn is set to NULL.
 */
void SCXLNXConnClose(struct SCXLNX_CONNECTION *pConn)
{
	int nError;
	enum SCXLNX_CONN_STATE nState;

	dprintk(KERN_DEBUG "SCXLNXConnClose(%p)\n", pConn);

	if (pConn == NULL)
		return;

	/*
	 * Assumption: Linux guarantees that no other operation is in progress
	 * and that no other operation will be started when close is called
	 */
	BUG_ON(atomic_read(&(pConn->nPendingOpCounter)) != 0);

	/*
	 * Exchange a Destroy Device Context message if needed.
	 */
	spin_lock(&(pConn->stateLock));
	nState = pConn->nState;
	spin_unlock(&(pConn->stateLock));
	if (nState == SCXLNX_CONN_STATE_VALID_DEVICE_CONTEXT) {
		/*
		 * A DestroyDeviceContext operation was not performed. Do it
		 * now.
		 */
		nError = SCXLNXConnDestroyDeviceContext(pConn);
		if (nError != 0)
			/* avoid cleanup if destroy device context fails */
			goto error;
	}

	/*
	 * Clean up the shared memory
	 */
	SCXLNXConnCleanupSharedMemory(pConn);

	internal_kfree(pConn);

	return;

error:
	dprintk(KERN_DEBUG "SCXLNXConnClose(%p) failed with error code %d\n",
		pConn, nError);
}

