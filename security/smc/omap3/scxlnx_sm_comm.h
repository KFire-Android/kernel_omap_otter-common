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

#ifndef __SCXLNX_SM_COMM_H__
#define __SCXLNX_SM_COMM_H__

#include "scxlnx_defs.h"


/*----------------------------------------------------------------------------
 *PA communication (implemented in the assembler file)
 *-------------------------------------------------------------------------- */

u32 pub2sec_bridge_entry(u32 appl_id, u32 proc_ID, u32 flag, u32 paNextArgs);
u32 rpc_handler(u32 p1, u32 p2, u32 p3, u32 p4);

/*----------------------------------------------------------------------------
 *Shared memory related operations
 *-------------------------------------------------------------------------- */

void SCXLNXSMCommReleaseDescriptor(SCXLNX_SHMEM_DESC *pShmemDesc,
					u32 nFullCleanup);

int SCXLNXSMCommFillDescriptor(SCXLNX_SHMEM_DESC *pShmemDesc,
					 u32 *pnBufferAddr,
					 u32 nBufferSize, u32 *pFlags);

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

#endif	/*__SCXLNX_SM_COMM_H__ */
