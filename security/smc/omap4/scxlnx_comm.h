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

#ifndef __SCXLNX_COMM_H__
#define __SCXLNX_COMM_H__

#include "scxlnx_defs.h"
#include "scx_protocol.h"

/*----------------------------------------------------------------------------
 * Misc
 *----------------------------------------------------------------------------*/

void SCXLNXCommSetCurrentTime(struct SCXLNX_COMM *pComm);

/*
 * Atomic accesses to 32-bit variables in the L1 Shared buffer
 */
static inline u32 SCXLNXCommReadReg32(const u32 *pCommBuffer)
{
	u32 result;

	__asm__ __volatile__("@ SCXLNXCommReadReg32\n"
		"ldrex %0, [%1]\n"
		: "=&r" (result)
		: "r" (pCommBuffer)
	);

	return result;
}

static inline void SCXLNXCommWriteReg32(void *pCommBuffer, u32 nValue)
{
	u32 tmp;

	__asm__ __volatile__("@ SCXLNXCommWriteReg32\n"
		"1:	ldrex %0, [%2]\n"
		"	strex %0, %1, [%2]\n"
		"	teq   %0, #0\n"
		"	bne   1b"
		: "=&r" (tmp)
		: "r" (nValue), "r" (pCommBuffer)
		: "cc"
	);
}

/*
 * Atomic accesses to 64-bit variables in the L1 Shared buffer
 */
static inline u64 SCXLNXCommReadReg64(void *pCommBuffer)
{
	u64 result;

	__asm__ __volatile__("@ SCXLNXCommReadReg64\n"
		"ldrexd %0, [%1]\n"
		: "=&r" (result)
		: "r" (pCommBuffer)
	);

	return result;
}

static inline void SCXLNXCommWriteReg64(void *pCommBuffer, u64 nValue)
{
	u64 tmp;

	__asm__ __volatile__("@ SCXLNXCommWriteReg64\n"
		"1:	ldrexd %0, [%2]\n"
		"	strexd %0, %1, [%2]\n"
		"	teq    %0, #0\n"
		"	bne    1b"
		: "=&r" (tmp)
		: "r" (nValue), "r" (pCommBuffer)
		: "cc"
	);
}

/*----------------------------------------------------------------------------
 * SMC operations
 *----------------------------------------------------------------------------*/

/* RPC return values */
#define RPC_NO		0x00	/* No RPC to execute */
#define RPC_YIELD	0x01	/* Yield RPC */
#define RPC_NON_YIELD	0x02	/* non-Yield RPC */

int SCXLNXCommExecuteRPCCommand(struct SCXLNX_COMM *pComm);

/*----------------------------------------------------------------------------
 * Shared memory related operations
 *----------------------------------------------------------------------------*/

#define L1_DESCRIPTOR_FAULT            (0x00000000)
#define L2_DESCRIPTOR_FAULT            (0x00000000)

#define L2_DESCRIPTOR_ADDR_MASK         (0xFFFFF000)

#define DESCRIPTOR_V13_12_MASK      (0x3 << PAGE_SHIFT)
#define DESCRIPTOR_V13_12_GET(a)    ((a & DESCRIPTOR_V13_12_MASK) >> PAGE_SHIFT)

struct SCXLNX_COARSE_PAGE_TABLE *SCXLNXAllocateCoarsePageTable(
	struct SCXLNX_COARSE_PAGE_TABLE_ALLOCATION_CONTEXT *pAllocationContext,
	u32 nType);

void SCXLNXFreeCoarsePageTable(
	struct SCXLNX_COARSE_PAGE_TABLE_ALLOCATION_CONTEXT *pAllocationContext,
	struct SCXLNX_COARSE_PAGE_TABLE *pCoarsePageTable,
	int nForce);

void SCXLNXInitializeCoarsePageTableAllocator(
	struct SCXLNX_COARSE_PAGE_TABLE_ALLOCATION_CONTEXT *pAllocationContext);

void SCXLNXReleaseCoarsePageTableAllocator(
	struct SCXLNX_COARSE_PAGE_TABLE_ALLOCATION_CONTEXT *pAllocationContext);

struct page *SCXLNXCommL2PageDescriptorToPage(u32 nL2PageDescriptor);

u32 SCXLNXCommGetL2DescriptorCommon(u32 nVirtAddr, struct mm_struct *mm);

void SCXLNXCommReleaseSharedMemory(
	struct SCXLNX_COARSE_PAGE_TABLE_ALLOCATION_CONTEXT *pAllocationContext,
	struct SCXLNX_SHMEM_DESC *pShmemDesc,
	u32 nFullCleanup);

int SCXLNXCommFillDescriptorTable(
	struct SCXLNX_COARSE_PAGE_TABLE_ALLOCATION_CONTEXT *pAllocationContext,
	struct SCXLNX_SHMEM_DESC *pShmemDesc,
	u32 nBufferVAddr,
	struct vm_area_struct **ppVmas,
	u32 pDescriptors[SCX_MAX_COARSE_PAGES],
	u32 *pBufferSize,
	u32 *pBufferStartOffset,
	bool bInUserSpace,
	u32 nFlags,
	u32 *pnDescriptorCount);

/*----------------------------------------------------------------------------
 * Standard communication operations
 *----------------------------------------------------------------------------*/

#define STATUS_PENDING 0x00000001

int tf_schedule_secure_world(struct SCXLNX_COMM *pComm, bool prepare_exit);

int SCXLNXCommSendReceive(
	struct SCXLNX_COMM *pComm,
	union SCX_COMMAND_MESSAGE *pMessage,
	union SCX_ANSWER_MESSAGE *pAnswer,
	struct SCXLNX_CONNECTION *pConn,
	bool bKillable);


/**
 * get a pointer to the secure world description.
 * This points directly into the L1 shared buffer
 * and is valid only once the communication has
 * been initialized
 **/
u8 *SCXLNXCommGetDescription(struct SCXLNX_COMM *pComm);

/*----------------------------------------------------------------------------
 * Power management
 *----------------------------------------------------------------------------*/

enum SCXLNX_POWER_OPERATION {
	SCXLNX_POWER_OPERATION_HIBERNATE = 1,
	SCXLNX_POWER_OPERATION_SHUTDOWN = 2,
	SCXLNX_POWER_OPERATION_RESUME = 3,
};

int SCXLNXCommHibernate(struct SCXLNX_COMM *pComm);
int SCXLNXCommResume(struct SCXLNX_COMM *pComm);

int SCXLNXCommPowerManagement(struct SCXLNX_COMM *pComm,
	enum SCXLNX_POWER_OPERATION nOperation);


/*----------------------------------------------------------------------------
 * Communication initialization and termination
 *----------------------------------------------------------------------------*/

int SCXLNXCommInit(struct SCXLNX_COMM *pComm);

void SCXLNXCommTerminate(struct SCXLNX_COMM *pComm);


#endif  /* __SCXLNX_COMM_H__ */
