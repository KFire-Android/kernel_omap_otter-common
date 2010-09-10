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

#ifndef __SCXLNX_SM_COMM_H__
#define __SCXLNX_SM_COMM_H__

#include "scxlnx_defs.h"


/*----------------------------------------------------------------------------
 *PA communication (implemented in the assembler file)
 *-------------------------------------------------------------------------- */

u32 pub2sec_bridge_entry(u32 appl_id, u32 proc_ID, u32 flag, u32 paNextArgs);
u32 rpc_handler(u32 p1, u32 p2, u32 p3, u32 p4);

extern void v7_flush_kern_cache_all(void);

/*----------------------------------------------------------------------------
 *Shared memory related operations
 *-------------------------------------------------------------------------- */

#define L1_DESCRIPTOR_FAULT            (0x00000000)
#define L2_DESCRIPTOR_FAULT            (0x00000000)

SCXLNX_COARSE_PAGE_TABLE *SCXLNXAllocateCoarsePageTable(
   SCXLNX_COARSE_PAGE_TABLE_ALLOCATION_CONTEXT *pAllocationContext,
   u32 nType);

void SCXLNXInitializeCoarsePageTableAllocator(
   SCXLNX_COARSE_PAGE_TABLE_ALLOCATION_CONTEXT *pAllocationContext);

void SCXLNXSMCommReleaseDescriptor(SCXLNX_SHMEM_DESC *pShmemDesc,
					u32 nFullCleanup);

void SCXLNXCommReleaseSharedMemory(
      SCXLNX_COARSE_PAGE_TABLE_ALLOCATION_CONTEXT *pAllocationContext,
      SCXLNX_SHMEM_DESC *pShmemDesc,
      u32 nFullCleanup);

int SCXLNXCommFillDescriptorTable(
      SCXLNX_COARSE_PAGE_TABLE_ALLOCATION_CONTEXT *pAllocationContext,
      SCXLNX_SHMEM_DESC *pShmemDesc,
      struct vm_area_struct **ppVmas,
      u32 pBufferVAddr[SCX_MAX_COARSE_PAGES],
      u32 *pBufferSize,
      u32 *pBufferStartOffset,
      int bInUserSpace,
      u32 *pFlags);

/*----------------------------------------------------------------------------
 *Standard communication operations
 *-------------------------------------------------------------------------- */

int SCXLNXSMCommSendReceive(SCXLNX_SM_COMM_MONITOR *pSMComm,
				 SCX_COMMAND_MESSAGE *pMessage,
				 SCX_ANSWER_MESSAGE *pAnswer,
				 SCXLNX_CONN_MONITOR *pConn, int bKillable);

/*----------------------------------------------------------------------------
 *Power management
 *-------------------------------------------------------------------------- */

typedef enum {
	SCXLNX_SM_POWER_OPERATION_HIBERNATE = 1,
	SCXLNX_SM_POWER_OPERATION_SHUTDOWN = 2,
	SCXLNX_SM_POWER_OPERATION_RESUME = 3,
} SCXLNX_SM_POWER_OPERATION;

int SCXLNXSMCommPowerManagement(SCXLNX_SM_COMM_MONITOR *pSMComm,
				SCXLNX_SM_POWER_OPERATION nOperation);

/*----------------------------------------------------------------------------
 *Communication initialization and termination
 *-------------------------------------------------------------------------- */

int SCXLNXSMCommInit(SCXLNX_SM_COMM_MONITOR *pSMComm);

void SCXLNXSMCommTerminate(SCXLNX_SM_COMM_MONITOR *pSMComm);

int SCXLNXSMCommStart(SCXLNX_SM_COMM_MONITOR *pSMComm,
			u32 nSDPBackingStoreAddr, u32 nSDPBkExtStoreAddr,
			u8 *pPABufferVAddr, u8 *pPABufferVAddrRaw,
			u32 nPABufferSize, u8 *pPropertiesBuffer,
			u32 nPropertiesBufferLength);

void SCXLNXSMCommStop(SCXLNX_SM_COMM_MONITOR *pSMComm);

#ifdef SMC_OMAP4_POWER_MANAGEMENT
int SCXLNXCommSaveContext(uint32_t nPhysicalAddress);
#endif

#endif	/*__SCXLNX_SM_COMM_H__ */
