/*
 *Copyright (c)2006-2008 Trusted Logic S.A.
 *All Rights Reserved.
 *
 *This program is free software; you can redistribute it and/or
 *modify it under the terms of the GNU General Public License
 *version 2 as published by the Free Software Foundation.
 *
 *This program is distributed in the hope that it will be useful,
 *but WITHOUT ANY WARRANTY; without even the implied warranty of
 *MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *GNU General Public License for more details.
 *
 *You should have received a copy of the GNU General Public License
 *along with this program; if not, write to the Free Software
 *Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 *MA 02111-1307 USA
 */

#include <asm/atomic.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/list.h>
#include <linux/mm.h>
#include <linux/pagemap.h>
#include <linux/types.h>

#include "scxlnx_defs.h"
#include "scxlnx_util.h"
#include "scxlnx_sm_comm.h"
#include "scxlnx_conn.h"
#include "scx_public_crypto.h"

/*
 *This routine is called just before a message is sent to the SM
 *It checks the message is still valid
 *Validity is computed from the pConn nStatus field and
 *the pMessage nMessageType field.
 *pConn points to the connection descriptor
 *pMessage points to the message to be sent
 *returns 1 only if the message is still valid
 */
int SCXLNXConnCheckMessageValidity(SCXLNX_CONN_MONITOR *pConn,
					SCX_COMMAND_MESSAGE *pMessage)
{
	int nValid;

	spin_lock(&(pConn->stateLock));
	switch (pMessage->nMessageType) {
	case SCX_MESSAGE_TYPE_CREATE_DEVICE_CONTEXT:
	/*CREATE_DEVICE_CONTEXT only valid while no context created */
		if (pConn->nState != SCXLNX_CONN_STATE_NO_DEVICE_CONTEXT) {
			nValid = 0;
		} else {
			pConn->nState =
				 SCXLNX_CONN_STATE_CREATE_DEVICE_CONTEXT_SENT;
			nValid = 1;
		}
		break;

	case SCX_MESSAGE_TYPE_DESTROY_DEVICE_CONTEXT:
	/*DESTROY_DEVICE_CONTEXT only valid while a context is created */
		if (pConn->nState != SCXLNX_CONN_STATE_VALID_DEVICE_CONTEXT) {
			nValid = 0;
		} else {
			pConn->nState =
				 SCXLNX_CONN_STATE_DESTROY_DEVICE_CONTEXT_SENT;
			nValid = 1;
		}
		break;

	case SCX_MESSAGE_TYPE_POWER_MANAGEMENT:
	/*POWER_MANAGEMENT always valid */
		nValid = 1;
		break;

	default:
	/*All other messages are only valid when a device context exists */
		if (pConn->nState != SCXLNX_CONN_STATE_VALID_DEVICE_CONTEXT)
			nValid = 0;
		else
			nValid = 1;
		break;
	}
	spin_unlock(&(pConn->stateLock));

	return nValid;
}

/*--------------------------------------------------------------------------
 *Management of the shared memory blocks.
 *
 *Shared memory blocks are the blocks registered through
 *the commands CREATE_DEVICE_CONTEXT, REGISTER_SHARED_MEMORY
 *and POWER_MANAGEMENT
 *-------------------------------------------------------------------------- */

/**
 *Unmaps a shared memory descriptor
 **/
static void SCXLNXConnUnmapShmem(SCXLNX_SHMEM_MONITOR *pShmemMonitor,
				 SCXLNX_SHMEM_DESC *pShmemDesc,
				 u32 nFullCleanup)
{
	/*check pShmemDesc contains a descriptor */
	if (pShmemDesc == NULL)
		return;

	dprintk(KERN_INFO "SCXLNXConnUnmapShmem(%p) : \
		descriptor %p, refcnt=%d\n", pShmemMonitor,
		pShmemDesc, atomic_read(&(pShmemDesc->nRefCnt)));

wait_for_unused:
	down(&(pShmemMonitor->sharedMemoriesMutex));
	if (atomic_read(&(pShmemDesc->nRefCnt)) > 1) {
		dprintk(KERN_INFO "Descriptor in use\n");
		up(&(pShmemMonitor->sharedMemoriesMutex));
		schedule();
		goto wait_for_unused;
	}

	SCXLNXSMCommReleaseDescriptor(pShmemDesc, 1);

	list_del(&(pShmemDesc->list));

	if ((pShmemDesc->nType == SCXLNX_SHMEM_TYPE_REGISTERED_SHMEM)
		 || (nFullCleanup != 0)) {
		/*free descriptor */
		internal_kfree(pShmemDesc);

		atomic_dec(&(pShmemMonitor->nShmemAllocated));
	} else {
		/*
		 *This is a preallocated descriptor, add to free list
		 *Since the device context is unmapped last, it is
		 *always the first element of the free list if no
		 *device context has been created
		 */
		pShmemDesc->hIdentifier = 0;
		list_add(&(pShmemDesc->list),
			 &(pShmemMonitor->sFreeSharedMemoryList));
	}

	up(&(pShmemMonitor->sharedMemoriesMutex));
}

/*------------------------------------------------------------------------ */

/**
 *Find the first available slot for a new block of shared memory
 *and map the user buffer or allocate if required.
 *Update the pBufferAddr to a physical address
 *pShmemDesc is updated to the mapped shared memory descriptor
 *Check and remove pFlags driver specific flags
 **/
static int SCXLNXConnMapShmem(SCXLNX_SHMEM_MONITOR *pShmemMonitor,
				u32 *pnBufferAddr,
			/*flags for read-write access rights on the memory */
				u32 nFlags,
				u32 nBufferStartOffset,
				u32 nBufferSize,
				SCXLNX_SHMEM_DESC **ppShmemDesc)
{
	SCXLNX_SHMEM_DESC *pShmemDesc = NULL;
	int nError;

	BUG_ON(nBufferStartOffset != 0);

	dprintk(KERN_INFO
		"SCXLNXConnMapShmem(%p) : Buffer=0x%x, Size=%u bytes\n",
		pShmemMonitor, *pnBufferAddr, nBufferSize);

	down(&(pShmemMonitor->sharedMemoriesMutex));

	/*
	 *Check the list of free shared memory descriptors
	 *is not empty
	 */
	if (list_empty(&(pShmemMonitor->sFreeSharedMemoryList))) {
		if (atomic_read(&(pShmemMonitor->nShmemAllocated)) ==
			 SCXLNX_SHMEM_MAX_DESCRIPTORS) {
			dprintk(KERN_ERR
				"SMC: ConnMapShmem: Maximum shared memories \
				 already registered\n");
			nError = -ENOMEM;
			goto error;
		}

		atomic_inc(&(pShmemMonitor->nShmemAllocated));

		/*no descriptor available, allocate a new one */

		pShmemDesc =
			 (SCXLNX_SHMEM_DESC *) internal_kmalloc(sizeof(
								*pShmemDesc),
								GFP_KERNEL);
		if (pShmemDesc == NULL) {
			dprintk(KERN_ERR
				"SMC: ConnMapShmem: Failed to allocate \
				descriptor\n");
			nError = -ENOMEM;
			goto error;
		}

		/*Initialize the structure */
		pShmemDesc->nType = SCXLNX_SHMEM_TYPE_REGISTERED_SHMEM;
		pShmemDesc->pAllocatedBuffer = NULL;
		INIT_LIST_HEAD(&(pShmemDesc->list));
	} else {
		/*take the first free shared memory descriptor */
		pShmemDesc =
			 list_entry(pShmemMonitor->sFreeSharedMemoryList.next,
					 SCXLNX_SHMEM_DESC, list);
		list_del(&(pShmemDesc->list));
	}

	/*Add the descriptor to the used list */
	list_add(&(pShmemDesc->list), &(pShmemMonitor->sUsedSharedMemoryList));
	atomic_set(&(pShmemDesc->nRefCnt), 1);

	/*
	 *The descriptor is now allocated, so
	 *go on with the shared memory itself.
	 */

	nError = SCXLNXSMCommFillDescriptor(pShmemDesc,
					 pnBufferAddr, nBufferSize, &nFlags);
	if (nError != 0) {
		dprintk(KERN_ERR
			"SCXLNXConnMapShmem(%p) : SCXLNXSMCommFillDescriptor \
			failed\n",
			SCXLNXConnMapShmem);
		goto error;
	}

	/*
	 *Successful completion.
	 */
	*ppShmemDesc = pShmemDesc;
	up(&(pShmemMonitor->sharedMemoriesMutex));
	dprintk(KERN_INFO "SCXLNXConnMapShmem(%p) : descriptor %p\n",
		pShmemMonitor, pShmemDesc);
	return 0;

	/*
	 *Error handling.
	 */
 error:
	up(&(pShmemMonitor->sharedMemoriesMutex));

	dprintk(KERN_ERR "SCXLNXConnMapShmem(%p) : Failure [%d]\n",
		pShmemMonitor, nError);

	SCXLNXConnUnmapShmem(pShmemMonitor, pShmemDesc, 0);

	return nError;
}

/*------------------------------------------------------------------------ */

/*
 *This routine is called to verify if a shared memory buffer is valid.
 *Returns the buffer physical address if the buffer is valid,
 *otherwise, returns 0.
 */
u32 SCXLNXConnCheckSharedMem(SCXLNX_SHMEM_MONITOR *pShmemMonitor,
				  u8 *pBuffer, u32 nBufferSize)
{
	SCXLNX_SHMEM_DESC *pShmemDesc;

	dprintk(KERN_INFO
		"SCXLNXConnCheckSharedMem(%p) : Buffer=0x%x, Size=%u\n",
		pShmemMonitor, (u32) pBuffer, nBufferSize);

	down(&(pShmemMonitor->sharedMemoriesMutex));

	list_for_each_entry(pShmemDesc, &(pShmemMonitor->
							sUsedSharedMemoryList),
				 list) {
		if (pBuffer == pShmemDesc->pBuffer) {
			if (nBufferSize <= pShmemDesc->nAllocatedBufferSize) {
				u32 nBufferPhysAddr =
					 pShmemDesc->nBufferPhysAddr;
				up(&(pShmemMonitor->sharedMemoriesMutex));
				dprintk(KERN_INFO
					"SCXLNXConnCheckSharedMem(%p) : \
					Success (PhysAddr=0x%x)\n",
					pShmemMonitor, nBufferPhysAddr);
				return nBufferPhysAddr;
			} else {
				up(&(pShmemMonitor->sharedMemoriesMutex));
				dprintk(KERN_ERR
					"SCXLNXConnCheckSharedMem(%p) : \
					Incorrect size [Allocated=%d]\n",
					pShmemMonitor,
					pShmemDesc->nAllocatedBufferSize);
				return 0;
			}
		}
	}

	up(&(pShmemMonitor->sharedMemoriesMutex));
	dprintk(KERN_ERR "SCXLNXConnCheckSharedMem(%p) : Not found\n",
		pShmemMonitor);
	return 0;
}

/*
 *Clean up a list of shared memory descriptors.
 */
static void SCXLNXSharedMemoryCleanupList(SCXLNX_SHMEM_MONITOR *
					  pSharedMemoryMonitor,
					  struct list_head *pList)
{
	while (!list_empty(pList)) {
		SCXLNX_SHMEM_DESC *pShmemDesc;

		pShmemDesc = list_entry(pList->next, SCXLNX_SHMEM_DESC, list);

		SCXLNXConnUnmapShmem(pSharedMemoryMonitor, pShmemDesc, 1);
	}
}

/*
 *Clean up the shared memory monitor.
 *Releases all allocated pages.
 */
void SCXLNXSharedMemoryMonitorCleanup(SCXLNX_SHMEM_MONITOR *
						pSharedMemoryMonitor)
{
	/*clean up the list of used and free descriptors.
	 *done outside the mutex, because SCXLNXConnUnmapShmem already
	 *mutex()ed
	 */
	SCXLNXSharedMemoryCleanupList(pSharedMemoryMonitor,
						&pSharedMemoryMonitor->
						sUsedSharedMemoryList);
	SCXLNXSharedMemoryCleanupList(pSharedMemoryMonitor,
						&pSharedMemoryMonitor->
						sFreeSharedMemoryList);
}

/*
 *Initialize the shared memory monitor.
 *Allocates the minimum memory to be provided
 *for shared memory management
 */
int SCXLNXSharedMemoryMonitorInitialize(SCXLNX_SHMEM_MONITOR *
					pSharedMemoryMonitor)
{
	int nError;
	int nSharedMemoryDescriptorIndex;

	/*
	 *The connection has already set the pSharedMemoryMonitor to 0.
	 *We only need to initialize special elements and attempt to allocate
	 *the minimum shared memory descriptors we want to support
	 */

	init_MUTEX(&(pSharedMemoryMonitor->sharedMemoriesMutex));
	INIT_LIST_HEAD(&(pSharedMemoryMonitor->sFreeSharedMemoryList));
	INIT_LIST_HEAD(&(pSharedMemoryMonitor->sUsedSharedMemoryList));
	atomic_set(&(pSharedMemoryMonitor->nShmemAllocated), 0);

	/*
	 *Preallocate 1 page for the device context coarse page table
	 *Preallocate 2 extra pages for future shared memory registration.
	 *These 2 pages preallocation is to increase the connection chances
	 *of registering at least 2 shared memories
	 */
	for (nSharedMemoryDescriptorIndex = 0;
		nSharedMemoryDescriptorIndex < 3;
		nSharedMemoryDescriptorIndex++) {
		SCXLNX_SHMEM_DESC *pShmemDesc =
			 (SCXLNX_SHMEM_DESC *) internal_kmalloc(
						sizeof(*pShmemDesc),
						GFP_KERNEL);

		if (pShmemDesc == NULL) {
			dprintk(KERN_ERR
				"SMC: SharedMemoryMonitorInitialize: \
				Failed to pre-allocate descriptor %d\n",
				nSharedMemoryDescriptorIndex);
			nError = -ENOMEM;
			goto error;
		}

		if (nSharedMemoryDescriptorIndex == 0) {
			/*
			 *This one is used as a device context descriptor
			 */
			pShmemDesc->nType = SCXLNX_SHMEM_TYPE_DEVICE_CONTEXT;

			/*
			 * Store this descriptor in the device context
			 * descriptor pointer
			 */
			pSharedMemoryMonitor->pDeviceContextDesc = pShmemDesc;
		} else {
			/*
			 *This one is used as a registered shared memory
			 *descriptor
			 */
			pShmemDesc->nType =
				 SCXLNX_SHMEM_TYPE_PREALLOC_REGISTERED_SHMEM;
		}

		pShmemDesc->nAllocType = SCXLNX_SHMEM_ALLOC_TYPE_NONE;
		atomic_set(&(pShmemDesc->nRefCnt), 1);

		/*
		 *Add this preallocated descriptor to the list of free
		 *descriptors. Keep the device context specific one at
		 *the beginning of the list
		 */
		INIT_LIST_HEAD(&(pShmemDesc->list));
		list_add_tail(&(pShmemDesc->list),
					&(pSharedMemoryMonitor->
						sFreeSharedMemoryList));
	}

	return 0;

 error:
	SCXLNXSharedMemoryMonitorCleanup(pSharedMemoryMonitor);
	return nError;
}

/*
 * This function is a copy of the find_vma()function
 *in linux kernel 2.6.15 version with some fixes :
	- memory block may end on vm_end
	- check the full memory block is in the memory area
	- guarantee NULL is returned if no memory area is found
 */
struct vm_area_struct *SCXLNXConnFindVma(struct mm_struct *mm,
					 unsigned long addr,
					 unsigned long size)
{
	struct vm_area_struct *vma = NULL;

	if (mm) {
		/*Check the cache first. */
		/*(Cache hit rate is typically around 35%.) */
		vma = mm->mmap_cache;
		if (!
			 (vma && vma->vm_end >= (addr + size)
			  && vma->vm_start <= addr)) {
			struct rb_node *rb_node;

			rb_node = mm->mm_rb.rb_node;
			vma = NULL;

			while (rb_node) {
				struct vm_area_struct *vma_tmp;

				vma_tmp = rb_entry(rb_node,
							struct vm_area_struct,
							vm_rb);

				if (vma_tmp->vm_end >= (addr + size)) {
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

static int SCXLNXConnValidateSharedMemoryBlockAndFlags(void *pSharedMemory,
							u32 nSharedMemorySize,
							u32 nFlags)
{
#if 0
	struct vm_area_struct *vma;
	unsigned long nSharedMemory = (unsigned long)pSharedMemory;
#endif

	if ((nSharedMemorySize == 0)
		 || (((u32) pSharedMemory + nSharedMemorySize) <
		(u32) pSharedMemory)) {
		return -EINVAL;
	}

	if ((nFlags & ~(SCX_SHMEM_TYPE_READ |
			SCX_SHMEM_TYPE_WRITE |
			SCX_SHMEM_TYPE_DIRECT |
			SCX_SHMEM_TYPE_DIRECT_FORCE)) != 0) {
		return -EINVAL;
	}
#if 0
	down_read(&current->mm->mmap_sem);

	vma = SCXLNXConnFindVma(current->mm, nSharedMemory, nSharedMemorySize);

	if (vma == NULL)
		goto error;

	if ((nFlags & SCX_SHMEM_TYPE_READ) == SCX_SHMEM_TYPE_READ) {
		if ((vma->vm_flags & VM_READ) != VM_READ)
			goto error;
	}
	if ((nFlags & SCX_SHMEM_TYPE_WRITE) == SCX_SHMEM_TYPE_WRITE) {
		if ((vma->vm_flags & VM_WRITE) != VM_WRITE)
			goto error;
	}

	up_read(&current->mm->mmap_sem);
#endif
	return 0;

#if 0
 error:
	up_read(&current->mm->mmap_sem);
	return -EFAULT;
#endif
}

/*----------------------------------------------------------------------------
 *Connection operations to the SM
 *----------------------------------------------------------------------------
 */

int SCXLNXConnCreateDeviceContext(SCXLNX_CONN_MONITOR *pConn,
				  SCX_COMMAND_MESSAGE *pMessage,
				  SCX_ANSWER_MESSAGE *pAnswer)
{
	SCXLNX_SHMEM_DESC *pShmemDesc = NULL;
	SCX_VERSION_INFORMATION_BUFFER *pVersionBuffer;
	int nError = 0;
	u32 nFlags;

	dprintk(KERN_INFO
		"SCXLNXConnCreateDeviceContext(%p) : MasterHeap={0x%x, %u}\n",
		pConn,
		pMessage->sBody.sCreateDeviceContextMessage.
		nSharedMemStartOffset,
		pMessage->sBody.sCreateDeviceContextMessage.nSharedMemSize);

	/*
	 *Make sure the pVersionBuffer address is accessible for writing
	 *Its virtual address is in nSharedMemDescriptors[1]
	 */
	if (!access_ok(VERIFY_WRITE,
				 (void *)pMessage->
					sBody.sCreateDeviceContextMessage.
					nSharedMemDescriptors[1],
				 sizeof(SCX_VERSION_INFORMATION_BUFFER))) {
		dprintk(KERN_ERR
			"SCXLNXConnCreateDeviceContext(%p) : pVersionBuffer \
			descriptor is not accessible for writing 0x%08x\n",
			pConn,
			pMessage->sBody.sCreateDeviceContextMessage.
			nSharedMemDescriptors[1]);
		nError = -EFAULT;
		goto error0;
	}
	pVersionBuffer =
		 (SCX_VERSION_INFORMATION_BUFFER *) pMessage->sBody.
		 sCreateDeviceContextMessage.nSharedMemDescriptors[1];

	/*
	 *As we allocate memory here, make sure that the connection state
	 *is valid
	 */

	/*CREATE_DEVICE_CONTEXT only valid while no context created */
	spin_lock(&(pConn->stateLock));
	if (pConn->nState != SCXLNX_CONN_STATE_NO_DEVICE_CONTEXT)
		nError = -EFAULT;
	spin_unlock(&(pConn->stateLock));

	if (nError != 0) {
		dprintk(KERN_ERR
			"SCXLNXConnCreateDeviceContext(%p) : Invalid state\n",
			pConn);
		goto error0;
	}

	/*
	 *Device context memory is Read-Write
	 */
	nFlags = SCX_SHMEM_TYPE_READ | SCX_SHMEM_TYPE_WRITE |
		 SCX_SHMEM_TYPE_DIRECT | SCX_SHMEM_TYPE_DIRECT_FORCE;

	/*
	 *This is the first shared memory created.
	 *It will use the first pre-allocated descriptor, which is dedicated
	 *to the device context.
	 */
	nError = SCXLNXConnMapShmem(&(pConn->sSharedMemoryMonitor),
					&(pMessage->sBody.
						sCreateDeviceContextMessage.
						nSharedMemDescriptors[0]),
						nFlags,
					pMessage->
					 sBody.sCreateDeviceContextMessage.
					 nSharedMemStartOffset,
					pMessage->
					 sBody.sCreateDeviceContextMessage.
					 nSharedMemSize,
					&pShmemDesc);
	if (nError != 0) {
		dprintk(KERN_ERR
			"SCXLNXConnCreateDeviceContext(%p) : \
			SCXLNXConnMapShmem failed [%d]\n",
			pConn, nError);
		goto error0;
	}

	dprintk(KERN_INFO
		"SCXLNXConnCreateDeviceContext(%p) : MasterHeap=\
		{0x%x, 0x%x}\n",
		pConn, (u32) pShmemDesc->pBuffer,
		pMessage->sBody.sCreateDeviceContextMessage.
		 nSharedMemDescriptors[0]);

	nError = SCXLNXSMCommSendReceive(&pConn->pDevice->sm,
					 pMessage, pAnswer, pConn, 1);

	if ((nError != 0) || (pAnswer->nSChannelStatus != S_SUCCESS))
		goto error1;

	if (pAnswer->nSChannelStatus == S_SUCCESS) {
		/*
		 *CREATE_DEVICE_CONTEXT succeeded,
		 *store device context handle and update connection status
		 */
		pShmemDesc->hIdentifier =
			pAnswer->
			 sBody.sCreateDeviceContextAnswer.hDeviceContext;
		/*
		 *Return the buffer to map in the user space to the client
		 * if required
		 */
		if (pShmemDesc->nAllocType !=
			SCXLNX_SHMEM_ALLOC_TYPE_REGISTER) {
			pAnswer->sBody.sCreateDeviceContextAnswer.
				 nSharedMemDescriptor =
					(u32) pShmemDesc->pBuffer;
		} else {
			pAnswer->sBody.sCreateDeviceContextAnswer.
				 nSharedMemDescriptor = 0;
		}

		spin_lock(&(pConn->stateLock));
		pConn->nState = SCXLNX_CONN_STATE_VALID_DEVICE_CONTEXT;
		spin_unlock(&(pConn->stateLock));
	}

	/*
	 *Write back the version information buffer
	 */
	if (copy_to_user(pVersionBuffer->sDriverDescription,
			pConn->pDevice->sDriverDescription,
			SCX_DESCRIPTION_BUFFER_LENGTH)) {
		dprintk(KERN_WARNING
			"SCXLNXConnCreateDeviceContext(%p) : Failed to copy \
			back the driver description from %p to %p\n",
			pConn, pConn->pDevice->sDriverDescription,
			pVersionBuffer->sDriverDescription);
		nError = -EFAULT;
		goto error2;
	}

	if (copy_to_user(pVersionBuffer->sSecureWorldDescription,
			pConn->pDevice->sm.pBuffer->sVersionDescription,
			SCX_DESCRIPTION_BUFFER_LENGTH)) {
		dprintk(KERN_WARNING
			"SCXLNXConnCreateDeviceContext(%p) : Failed to copy \
			back the secure world description to %p\n",
			pConn, pVersionBuffer->sSecureWorldDescription);
		nError = -EFAULT;
		goto error2;
	}

	/*successful completion */
	dprintk(KERN_INFO
		"SCXLNXConnCreateDeviceContext(%p): Success \
		(hDeviceContext=0x%08x)\n",
		pConn,
		(unsigned int)(pAnswer->
			sBody.sCreateDeviceContextAnswer.hDeviceContext));
	return 0;

 error2:
	if (pConn->nState == SCXLNX_CONN_STATE_VALID_DEVICE_CONTEXT) {
		/*Device context created on secure side. We MUST destroy it */
		printk(KERN_WARNING
				"SCXLNXConnCreateDeviceContext(%p) : \
				Reverting Device creation\n",
				pConn);
		SCXLNXConnDestroyDeviceContext(pConn);
	}
 error1:
	SCXLNXConnUnmapShmem(&(pConn->sSharedMemoryMonitor), pShmemDesc, 0);

 error0:
	if (nError != 0) {
		printk(KERN_ERR "SCXLNXConnCreateDeviceContext(%p) : \
			Error [0x%08X]\n",
			pConn, nError);
	} else {
		/*
		 *We sent a DeviceCreateContext. The state is now
		 *SCXLNX_CONN_STATE_CREATE_DEVICE_CONTEXT_SENT
		 *It has to be reset if we ever want to send a
		 *DeviceCreateContext again
		 */
		spin_lock(&(pConn->stateLock));
		pConn->nState = SCXLNX_CONN_STATE_NO_DEVICE_CONTEXT;
		spin_unlock(&(pConn->stateLock));
		printk(KERN_INFO
			"SCXLNXConnCreateDeviceContext(%p): SChannel Status \
			[0x%08X]\n",
			pConn, pAnswer->nSChannelStatus);
	}

	return nError;
}

/*
 *Opens a client session to the SM
 */
int SCXLNXConnOpenClientSession(SCXLNX_CONN_MONITOR *pConn,
				SCX_COMMAND_MESSAGE *pMessage,
				SCX_ANSWER_MESSAGE *pAnswer)
{
	int nError = 0;
	SCX_UUID *pIdentifier;
	u8 *pHash;

	dprintk(KERN_INFO "SCXLNXConnOpenClientSession(%p)\n", pConn);

	switch (pMessage->sBody.sOpenClientSessionMessage.nLoginType) {
	case S_LOGIN_PUBLIC:
		break;

	case S_LOGIN_OS_IDENTIFICATION:
	case S_LOGIN_OS_GROUP_IDENTIFICATION:
		pIdentifier =
			(SCX_UUID *)&pMessage->
				sBody.sOpenClientSessionMessage.sLoginData[0];

		/*
		 *We need to send an OS identifier.
		 *On Linux: this is an UUID derived from the executable path,
		 *the UID, and EUID of the calling process
		 */
		nError = SCXLNXConnGetCurrentProcessUUID(pIdentifier);
		if (nError != 0) {
			dprintk(KERN_ERR
				"SCXLNXConnOpenClientSession: error in \
				SCXLNXConnGetCurrentProcessUUID\n");
			goto exit;
		}
		dprintk(KERN_DEBUG
			"SCXLNXConnOpenClientSession(%p) : process identity =\
			%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X\n",
			pConn, pIdentifier->time_low, pIdentifier->time_mid,
			pIdentifier->time_hi_and_version,
			pIdentifier->clock_seq_and_node[0],
			pIdentifier->clock_seq_and_node[1],
			pIdentifier->clock_seq_and_node[2],
			pIdentifier->clock_seq_and_node[3],
			pIdentifier->clock_seq_and_node[4],
			pIdentifier->clock_seq_and_node[5],
			pIdentifier->clock_seq_and_node[6],
			pIdentifier->clock_seq_and_node[7]);
		break;

	case S_LOGIN_AUTHENTICATION:
		pHash = &(pMessage->
				sBody.sOpenClientSessionMessage.sLoginData[0]);

		/*
		 *We need to send an OS identifier.
		 *On Linux: this is the hash of the executable binary
		 */
		nError = SCXLNXConnGetCurrentProcessHash(pHash);
		if (nError != 0) {
			dprintk(KERN_ERR
				"SCXLNXConnOpenClientSession: error in \
				SCXLNXConnGetCurrentProcessHash\n");
			goto exit;
		}
		dprintk(KERN_DEBUG
			"SCXLNXConnOpenClientSession(%p) : process hash ="
			"%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X\
			%02X%02X%02X%02X%02X%02X%02X%02X\n",
			pConn, pHash[0], pHash[1], pHash[2], pHash[3],
			pHash[4], pHash[5], pHash[6], pHash[7], pHash[8],
			pHash[9], pHash[10], pHash[11], pHash[12], pHash[13],
			pHash[14], pHash[15], pHash[16], pHash[17], pHash[18],
			pHash[19]);
		break;

	case S_LOGIN_PRIVILEGED:
		/*
		 *The only allowed user is the root user
		 *check the current process id to allow login
		 */
		if (CURRENT_EUID != 0 && CURRENT_EGID != 0) {
			dprintk(KERN_ERR "SCXLNXConnOpenClientSession: \
				user %d not allowed to open session with \
				S_LOGIN_PRIVILEGED\n",
				CURRENT_UID);
			nError = -EACCES;
			goto exit;
		}

		break;

	default:
		dprintk(KERN_ERR
			"SCXLNXConnOpenClientSession: \
			unknown nLoginType(%08X)\n",
			pMessage->sBody.sOpenClientSessionMessage.nLoginType);
		nError = -EOPNOTSUPP;
		goto exit;
	}

	pMessage->sBody.sOpenClientSessionMessage.hDeviceContext =
		 pConn->sSharedMemoryMonitor.pDeviceContextDesc->hIdentifier;

	nError = SCXLNXSMCommSendReceive(&pConn->pDevice->sm,
					pMessage, pAnswer, pConn, 1);

 exit:
	if (nError != 0) {
		dprintk(KERN_ERR "SCXLNXConnOpenClientSession(%p): \
			Error [0x%08X]\n",
			pConn, nError);
	} else {
		dprintk(KERN_INFO
			"SCXLNXConnOpenClientSession(%p): \
			SChannel Status [0x%08X]\n",
			pConn, pAnswer->nSChannelStatus);
	}

	return nError;
}

/*
 *Closes a client session from the SM
 */
int SCXLNXConnCloseClientSession(SCXLNX_CONN_MONITOR *pConn,
				SCX_COMMAND_MESSAGE *pMessage,
				SCX_ANSWER_MESSAGE *pAnswer)
{
	int nError = 0;

	dprintk(KERN_INFO
		"SCXLNXConnCloseClientSession(%p) hIdentifier=0x%08x\n", pConn,
		pConn->sSharedMemoryMonitor.pDeviceContextDesc->hIdentifier);

	pMessage->sBody.sCloseClientSessionMessage.hDeviceContext =
		 pConn->sSharedMemoryMonitor.pDeviceContextDesc->hIdentifier;

	nError = SCXLNXSMCommSendReceive(&pConn->pDevice->sm,
					pMessage, pAnswer, pConn, 1);

	if (nError != 0) {
		dprintk(KERN_ERR "SCXLNXConnCloseClientSession(%p): \
			Error [0x%08X]\n",
			pConn, nError);
	} else {
		dprintk(KERN_INFO
			"SCXLNXConnCloseClientSession(%p): \
			SChannel Status [0x%08X]\n",
			pConn, pAnswer->nSChannelStatus);
	}

	return nError;
}

/*
 *Registers a shared memory to the SM
 */
int SCXLNXConnRegisterSharedMemory(SCXLNX_CONN_MONITOR *pConn,
					SCX_COMMAND_MESSAGE *pMessage,
					SCX_ANSWER_MESSAGE *pAnswer)
{
	int nError = 0;
	SCXLNX_SHMEM_DESC *pShmemDesc = NULL;

	dprintk(KERN_INFO
		"SCXLNXConnRegisterSharedMemory(%p) : Block=0x%x, %u bytes\n",
		pConn,
		pMessage->sBody.sRegisterSharedMemoryMessage.
		nSharedMemDescriptors[0],
		pMessage->sBody.sRegisterSharedMemoryMessage.nSharedMemSize);

	nError = SCXLNXConnValidateSharedMemoryBlockAndFlags((void *)pMessage->
		sBody.sRegisterSharedMemoryMessage.nSharedMemDescriptors[0],
		pMessage->sBody.sRegisterSharedMemoryMessage.nSharedMemSize,
		pMessage->sBody.sRegisterSharedMemoryMessage.nFlags);
	if (nError != 0)
		goto error0;

	/*
	 * SCX_SHMEM_TYPE_DIRECT | SCX_SHMEM_TYPE_DIRECT_FORCE
	 * not implemented yet
	 */
	pMessage->sBody.sRegisterSharedMemoryMessage.nFlags &=
		 ~(SCX_SHMEM_TYPE_DIRECT | SCX_SHMEM_TYPE_DIRECT_FORCE);

	/*Map the shmem block and update the message */
	nError = SCXLNXConnMapShmem(&(pConn->sSharedMemoryMonitor),
			&(pMessage->sBody.sRegisterSharedMemoryMessage.
			 nSharedMemDescriptors[0]),
			pMessage->sBody.sRegisterSharedMemoryMessage.nFlags,
			pMessage->sBody.sRegisterSharedMemoryMessage.
			 nSharedMemStartOffset,
			pMessage->sBody.sRegisterSharedMemoryMessage.
			 nSharedMemSize,
			&pShmemDesc);

	if (nError != 0) {
		dprintk(KERN_ERR
			"SCXLNXConnRegisterSharedMemory(%p) : \
			SCXLNXConnMapShmem failed [%d]\n",
			pConn, nError);
		goto error0;
	}

	dprintk(KERN_INFO
		"SCXLNXConnRegisterSharedMemory(%p) : Block={0x%x, 0x%x}\n",
		pConn, (u32) pShmemDesc->pBuffer,
		pMessage->sBody.sRegisterSharedMemoryMessage.
		nSharedMemDescriptors[0]);

	/*
	 *write the correct device context handle
	 *and the address of the shared memory descriptor
	 *in the message
	 */
	pMessage->sBody.sRegisterSharedMemoryMessage.hDeviceContext =
		 pConn->sSharedMemoryMonitor.pDeviceContextDesc->hIdentifier;
	pMessage->sBody.sRegisterSharedMemoryMessage.nBlockID =
		 (u32) pShmemDesc;

	/*Send the updated message */
	nError = SCXLNXSMCommSendReceive(&pConn->pDevice->sm,
					 pMessage, pAnswer, pConn, 1);

	if ((nError != 0) || (pAnswer->nSChannelStatus != S_SUCCESS)) {
		dprintk(KERN_ERR
			"SCXLNXConnRegisterSharedMemory: operation failed. \
			Unmap block\n");
		goto error1;
	}

	/*Store the block handle returned by the Secure World... */
	pShmemDesc->hIdentifier =
		 pAnswer->sBody.sRegisterSharedMemoryAnswer.hBlock;
	/*
	 * Return the buffer to map in the user space to the client if required
	 */
	if (pShmemDesc->nAllocType != SCXLNX_SHMEM_ALLOC_TYPE_REGISTER) {
		pAnswer->sBody.sRegisterSharedMemoryAnswer.
			 nSharedMemDescriptor = (u32) pShmemDesc->pBuffer;
	} else {
		pAnswer->sBody.sRegisterSharedMemoryAnswer.
			 nSharedMemDescriptor = 0;
	}

	/*successful completion */
	dprintk(KERN_INFO
		"SCXLNXConnRegisterSharedMemory(%p) : \
		nBlockID=0x%08x hBlock=0x%08x\n",
		pConn, pMessage->sBody.sRegisterSharedMemoryMessage.nBlockID,
		pAnswer->sBody.sRegisterSharedMemoryAnswer.hBlock);
	return 0;

	/*error completion */
 error1:
	SCXLNXConnUnmapShmem(&(pConn->sSharedMemoryMonitor), pShmemDesc, 0);

 error0:
	if (nError != 0) {
		dprintk(KERN_ERR "SCXLNXConnRegisterSharedMemory(%p): \
			Error [0x%08X]\n",
			pConn, nError);
	} else {
		dprintk(KERN_INFO
			"SCXLNXConnRegisterSharedMemory(%p): \
			SChannel Status [0x%08X]\n",
			pConn, pAnswer->nSChannelStatus);
	}

	return nError;
}

/*
 *Releases a shared memory from the SM
 */
int SCXLNXConnReleaseSharedMemory(
				SCXLNX_CONN_MONITOR *pConn,
				SCX_COMMAND_MESSAGE *pMessage,
				SCX_ANSWER_MESSAGE *pAnswer)
{
	int nError = 0;

	dprintk(KERN_INFO "SCXLNXConnReleaseSharedMemory(%p)\n", pConn);

	pMessage->sBody.sReleaseSharedMemoryMessage.hDeviceContext =
		 pConn->sSharedMemoryMonitor.pDeviceContextDesc->hIdentifier;

	nError = SCXLNXSMCommSendReceive(&pConn->pDevice->sm,
					 pMessage, pAnswer, pConn, 1);

	if ((nError != 0) || (pAnswer->nSChannelStatus != S_SUCCESS))
		goto error;

	/*Use nBlockID to get back the pointer to pShmemDesc */
	SCXLNXConnUnmapShmem(&(pConn->sSharedMemoryMonitor),
				  (SCXLNX_SHMEM_DESC *) pAnswer->sBody.
				  sReleaseSharedMemoryAnswer.nBlockID, 0);

	/*successful completion */
	dprintk(KERN_INFO "SCXLNXConnReleaseSharedMemory(%p) :"
		" nBlockID=0x%08x hBlock=0x%08x\n",
		pConn, pAnswer->sBody.sReleaseSharedMemoryAnswer.nBlockID,
		pMessage->sBody.sReleaseSharedMemoryMessage.hBlock);
	return 0;

 error:
	if (nError != 0) {
		dprintk(KERN_ERR "SCXLNXConnReleaseSharedMemory(%p): \
			Error [0x%08X]\n",
			pConn, nError);
	} else {
		dprintk(KERN_INFO
			"SCXLNXConnReleaseSharedMemory(%p): \
			SChannel Status [0x%08X]\n",
			pConn, pAnswer->nSChannelStatus);
	}

	return nError;

}

/*
 *Invokes a client command to the SM with CUS processing
 */
int SCXLNXConnInvokeClientCommand(SCXLNX_CONN_MONITOR *pConn,
				SCX_COMMAND_MESSAGE *pMessage,
				SCX_ANSWER_MESSAGE *pAnswer)
{
	int nError = 0;
	CRYPTOKI_UPDATE_SHORTCUT_CONTEXT *pCUSContext = NULL;

	dprintk(KERN_INFO "SCXLNXConnInvokeClientCommand(%p)\n", pConn);

	pMessage->sBody.sInvokeClientCommandMessage.hDeviceContext =
		pConn->sSharedMemoryMonitor.pDeviceContextDesc->hIdentifier;

	/*check if the command is shortcuted without incrementing nUseCount,
		if YES, return the public crypto desc session in pCUSContext */
	if (scxPublicCryptoIsShortcutedCommand(pConn,
		&(pMessage->sBody.sInvokeClientCommandMessage),
		&pCUSContext, false)) {
		/*shortcut the command and run it in public */
		struct semaphore *pHWALock = NULL;

		dprintk(KERN_INFO "SCXLNXConnInvokeClientCommand: \
			CUS(%p): Command shortcuted: nHWAID=0x%08x\n",
			pCUSContext, pCUSContext->nHWAID);

		/*decode and lock HWA */
		switch (pCUSContext->nHWAID) {
		case RPC_AES_CODE:
			pHWALock = &g_SCXLNXDeviceMonitor.sAES1CriticalSection;
			break;
		case RPC_DES_CODE:
			pHWALock = &g_SCXLNXDeviceMonitor.sD3D2CriticalSection;
			break;
		default:
		case RPC_SHA_CODE:
			pHWALock = &g_SCXLNXDeviceMonitor.
					sSHAM1CriticalSection;
			break;
		}

		dprintk(KERN_INFO
			"SCXLNXConnInvokeClientCommand: CUS(%p): \
			Wait for HWAID=0x%04X\n",
			pCUSContext, pCUSContext->nHWAID);
		down(pHWALock);
		dprintk(KERN_INFO
			"SCXLNXConnInvokeClientCommand: CUS(%p): \
			Locked on HWAID=0x%04X\n",
			pCUSContext, pCUSContext->nHWAID);

		/*
		 * re-check if the command is shortcutable with increment
		 *  of nUseCount
		 */
		if (scxPublicCryptoIsShortcutedCommand(pConn,
			&(pMessage->sBody.sInvokeClientCommandMessage),
			&pCUSContext, true)) {

			CRYPTOKI_UPDATE_PARAMS sCUSParams;

			memset(&sCUSParams, 0, sizeof(sCUSParams));

			dprintk(KERN_INFO
				"SCXLNXConnInvokeClientCommand: CUS(%p): \
				Command shortcuted confirmed: nHWAID=0x%08x\n",
				pCUSContext, pCUSContext->nHWAID);

			/*
			 * the command is still shortcutable,
			 * then perform the operation in public
			 */
			/*decode input params eq pre-process inputs */
			if (!scxPublicCryptoParseCommandMessage(
				pConn,
				pConn->sSharedMemoryMonitor.
				pDeviceContextDesc,
				pCUSContext,
				&(pMessage->sBody.sInvokeClientCommandMessage),
				&sCUSParams)) {
				/*decrement nUseCount */
				pCUSContext->nUseCount--;
				/*release the HWA */
				up(pHWALock);
				if (sCUSParams.pInputShMem) {
					atomic_dec(&(sCUSParams.
						pInputShMem->nRefCnt));
				}
				if (sCUSParams.pOutputShMem) {
					atomic_dec(&(sCUSParams.
						pOutputShMem->nRefCnt));
				}
				dprintk(KERN_INFO
					"SCXLNXConnInvokeClientCommand: \
					CUS(%p): Parsing failed -> \
					forward command to secure\n",
					pCUSContext);
					goto command_invoke_perform_in_secure;
			}

			/*perform the HWA update in public <=> THE shortcut */
			scxPublicCryptoUpdate(pCUSContext, &sCUSParams);

			/* write the answer message */
			scxPublicCryptoWriteAnswerMessage(pCUSContext,
				&sCUSParams,
				&pAnswer->sBody.sInvokeClientCommandAnswer);

				if (sCUSParams.pInputShMem) {
					atomic_dec(&(sCUSParams.
						pInputShMem->nRefCnt));
				}
				if (sCUSParams.pOutputShMem) {
					atomic_dec(&(sCUSParams.
						pOutputShMem->nRefCnt));
				}

			/*decrement nUseCount */
			pCUSContext->nUseCount--;

			/*format the answer message */
			pAnswer->nMessageType = pMessage->nMessageType;
			pAnswer->nOperationID = 0;
			pAnswer->nSChannelStatus = S_SUCCESS;
			/*release the HWA */
			up(pHWALock);
			/*at this stage, unconditionaly return S_SUCCESS */
			goto command_invoke_success;
		}

		/*
		 *Command not shortcutable at 2nd
		 *scxPublicCryptoIsShortcutedCommand()
		 *call, then, process the command in secure as usual
		 */
		/*don't forget to release the HWA */
		up(pHWALock);
	}

command_invoke_perform_in_secure:
	{
		nError = SCXLNXSMCommSendReceive(&pConn->pDevice->sm,
						pMessage, pAnswer, pConn, 1);

		if ((nError != 0) || (pAnswer->nSChannelStatus != S_SUCCESS))
			goto command_invoke_error_from_secure;
		/*if no error in secure, follow with command_invoke_success */
	}

command_invoke_success:
	/*successful completion */
	dprintk(KERN_INFO "SCXLNXConnInvokeClientCommand(%p): Success:"
		" nClientOperationID=0x%08x\n",
		pConn,
		pMessage->
			sBody.sInvokeClientCommandMessage.nClientOperationID);

	return S_SUCCESS;

command_invoke_error_from_secure:
	/*command sent to secure has generated an error, then forward it */
	if (nError != 0) {
		dprintk(KERN_ERR
			"SCXLNXConnInvokeClientCommand(%p): Error [0x%08X]\n",
			pConn, nError);
	} else {
		dprintk(KERN_ERR
			"SCXLNXConnInvokeClientCommand(%p): \
			SChannel Error [0x%08X]\n",
			pConn, pAnswer->nSChannelStatus);
	}
	return nError;
}

/*
 *Cancels a client command from the SM
 */
int SCXLNXConnCancelClientCommand(
				SCXLNX_CONN_MONITOR *pConn,
				SCX_COMMAND_MESSAGE *pMessage,
				SCX_ANSWER_MESSAGE *pAnswer)
{
	int nError = 0;

	dprintk(KERN_INFO "SCXLNXConnCancelClientCommand(%p)\n", pConn);

	pMessage->sBody.sCancelClientOperationMessage.hDeviceContext =
		 pConn->sSharedMemoryMonitor.pDeviceContextDesc->hIdentifier;

	nError = SCXLNXSMCommSendReceive(&pConn->pDevice->sm,
					 pMessage, pAnswer, pConn, 1);

	if ((nError != 0) || (pAnswer->nSChannelStatus != S_SUCCESS))
		goto error;

	/*successful completion */
	dprintk(KERN_INFO "SCXLNXConnCancelClientCommand(%p) :"
		" nClientOperationID=0x%08x\n",
		pConn,
		pMessage->sBody.sCancelClientOperationMessage.
		nClientOperationID);
	return 0;

 error:
	if (nError != 0) {
		dprintk(KERN_ERR "SCXLNXConnCancelClientCommand(%p): \
			Error [0x%08X]\n",
			pConn, nError);
	} else {
		dprintk(KERN_INFO
			"SCXLNXConnCancelClientCommand(%p): \
			SChannel Status [0x%08X]\n",
			pConn, pAnswer->nSChannelStatus);
	}

	return nError;
}

/*
 *Destroys a device context from the SM
 */
int SCXLNXConnDestroyDeviceContext(SCXLNX_CONN_MONITOR *pConn)
{
	int nError;
	SCX_COMMAND_MESSAGE sMessage;
	SCX_ANSWER_MESSAGE sAnswer;

	dprintk(KERN_INFO
		"SCXLNXConnDestroyDeviceContext(%p) hIdentifier=0x%08x\n",
		pConn,
		pConn->sSharedMemoryMonitor.pDeviceContextDesc->hIdentifier);

	BUG_ON(pConn == NULL);

	sMessage.nMessageType = SCX_MESSAGE_TYPE_DESTROY_DEVICE_CONTEXT;

	/*
	 *fill in the device context handler
	 *it is guarantied that the first shared memory descriptor describes
	 *the device context
	 */
	sMessage.sBody.sDestroyDeviceContextMessage.hDeviceContext =
		 pConn->sSharedMemoryMonitor.pDeviceContextDesc->hIdentifier;

	nError = SCXLNXSMCommSendReceive(&pConn->pDevice->sm,
					&sMessage, &sAnswer, pConn, 0);

	if ((nError != 0) || (sAnswer.nSChannelStatus != S_SUCCESS))
		goto error;

	spin_lock(&(pConn->stateLock));
	pConn->nState = SCXLNX_CONN_STATE_NO_DEVICE_CONTEXT;
	spin_unlock(&(pConn->stateLock));

	/*successful completion */
	dprintk(KERN_INFO "SCXLNXConnDestroyDeviceContext(%p): Success\n",
		pConn);
	return 0;

 error:
	dprintk(KERN_ERR
		"SCXLNXConnDestroyDeviceContext(%p): \
		Error=0x%08X [nSChannelStatus=0x%x]\n",
		pConn, nError, sAnswer.nSChannelStatus);

	return nError;
}

/*----------------------------------------------------------------------------
 *Connection initialization and cleanup operations
 *----------------------------------------------------------------------------
*/

/*
 *Opens a connection to the specified device.
 *
 *The placeholder referenced by ppConn is set to the address of monitor of the
 *new connection; it is set to NULL upon failure.
 *
 *Returns zero upon successful completion, or an appropriate error code upon
 *failure.
 */
int SCXLNXConnOpen(SCXLNX_DEVICE_MONITOR *pDevice,
			struct file *file, SCXLNX_CONN_MONITOR **ppConn)
{
	int nError;
	SCXLNX_CONN_MONITOR *pConn = NULL;

	dprintk(KERN_INFO "SCXLNXConnOpen(%p, %p, %p)\n", pDevice, file,
		ppConn);

	/*
	 *Allocate and initialize the connection monitor.
	 *kmalloc only allocates sizeof(*pConn)virtual memory
	 */
	pConn =
		(SCXLNX_CONN_MONITOR *) internal_kmalloc(
						sizeof(*pConn), GFP_KERNEL);
	if (pConn == NULL) {
		dprintk(KERN_ERR "SMC: ConnOpen: Out of memory \
			for connection descriptor !\n");
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
	 *Initialize the shared memory monitor.
	 */
	nError = SCXLNXSharedMemoryMonitorInitialize(&(pConn->
			sSharedMemoryMonitor));
	if (nError != 0)
		goto error;

	/*
	 *initialize device context: semaphores and session list
	 *(eq CUSContext list for CUS)
	 */
	scxPublicCryptoInitDeviceContext(pConn);

	/*
	 *Successful completion.
	 */

	*ppConn = pConn;

	dprintk(KERN_INFO "SCXLNXConnOpen(%p) : Success (pConn=%p)\n",
		pDevice, pConn);
	return 0;

	/*
	 *Error handling.
	 */

 error:
	dprintk(KERN_ERR "SCXLNXConnOpen(%p) : Failure (error %d)\n",
		pDevice, nError);
	/*Deallocate the descriptor pages if necessary */
	internal_kfree(pConn);

	*ppConn = NULL;
	return nError;
}

/*
 *Closes the specified connection.
 *
 *Upon return, the connection referenced by pConn has been destroyed and cannot
 *be used anymore.
 *
 *This function does nothing if pConn is set to NULL.
 */
void SCXLNXConnClose(SCXLNX_CONN_MONITOR *pConn)
{
	int nError;
	SCXLNX_CONN_STATE nState;

	dprintk(KERN_DEBUG "SCXLNXConnClose(%p)\n", pConn);

	if (pConn == NULL)
		return;

	/*
	 *Assumption: Linux guarantees that no other operation is in progress
	 *and that no other operation will be started when close is called
	 */
	BUG_ON(atomic_read(&(pConn->nPendingOpCounter)) != 0);

	/*
	 *Exchange a Destroy Device Context message if needed.
	 */
	spin_lock(&(pConn->stateLock));
	nState = pConn->nState;
	spin_unlock(&(pConn->stateLock));
	if (nState == SCXLNX_CONN_STATE_VALID_DEVICE_CONTEXT) {
		/*
		 *A DestroyDeviceContext operation was not performed.
		 *Do it now.
		 */
		nError = SCXLNXConnDestroyDeviceContext(pConn);
		if (nError != 0) {
			/*avoid cleanup if destroy device context fails */
			goto error;
		}
	}

	/*
	 *Clean up the shared memory monitor.
	 */
	SCXLNXSharedMemoryMonitorCleanup(&(pConn->sSharedMemoryMonitor));

	internal_kfree(pConn);

	return;

 error:
	dprintk(KERN_DEBUG "SCXLNXConnClose(%p)failed with error code %d\n",
		pConn, nError);
}
