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
#include <linux/mman.h>
#include "scxlnx_util.h"

/*----------------------------------------------------------------------------
 * Debug printing routines
 *----------------------------------------------------------------------------*/
#ifdef CONFIG_TF_DRIVER_DEBUG_SUPPORT

void addressCacheProperty(unsigned long va)
{
	unsigned long pa;
	unsigned long inner;
	unsigned long outer;

	asm volatile ("mcr p15, 0, %0, c7, c8, 0" : : "r" (va));
	asm volatile ("mrc p15, 0, %0, c7, c4, 0" : "=r" (pa));

	dprintk(KERN_INFO "VA:%x, PA:%x\n",
		(unsigned int) va,
		(unsigned int) pa);

	if (pa & 1) {
		dprintk(KERN_INFO "Prop Error\n");
		return;
	}

	outer = (pa >> 2) & 3;
	dprintk(KERN_INFO "\touter : %x", (unsigned int) outer);

	switch (outer) {
	case 3:
		dprintk(KERN_INFO "Write-Back, no Write-Allocate\n");
		break;
	case 2:
		dprintk(KERN_INFO "Write-Through, no Write-Allocate.\n");
		break;
	case 1:
		dprintk(KERN_INFO "Write-Back, Write-Allocate.\n");
		break;
	case 0:
		dprintk(KERN_INFO "Non-cacheable.\n");
		break;
	}

	inner = (pa >> 4) & 7;
	dprintk(KERN_INFO "\tinner : %x", (unsigned int)inner);

	switch (inner) {
	case 7:
		dprintk(KERN_INFO "Write-Back, no Write-Allocate\n");
		break;
	case 6:
		dprintk(KERN_INFO "Write-Through.\n");
		break;
	case 5:
		dprintk(KERN_INFO "Write-Back, Write-Allocate.\n");
		break;
	case 3:
		dprintk(KERN_INFO "Device.\n");
		break;
	case 1:
		dprintk(KERN_INFO "Strongly-ordered.\n");
		break;
	case 0:
		dprintk(KERN_INFO "Non-cacheable.\n");
		break;
	}

	if (pa & 0x00000002)
		dprintk(KERN_INFO "SuperSection.\n");
	if (pa & 0x00000080)
		dprintk(KERN_INFO "Memory is shareable.\n");
	else
		dprintk(KERN_INFO "Memory is non-shareable.\n");

	if (pa & 0x00000200)
		dprintk(KERN_INFO "Non-secure.\n");
}

#ifdef CONFIG_SMC_BENCH_SECURE_CYCLE

#define LOOP_SIZE (100000)

void runBogoMIPS(void)
{
	uint32_t nCycles;
	void *pAddress = &runBogoMIPS;

	dprintk(KERN_INFO "BogoMIPS:\n");

	setupCounters();
	nCycles = runCodeSpeed(LOOP_SIZE);
	dprintk(KERN_INFO "%u cycles with code access\n", nCycles);
	nCycles = runDataSpeed(LOOP_SIZE, (unsigned long)pAddress);
	dprintk(KERN_INFO "%u cycles to access %x\n", nCycles,
		(unsigned int) pAddress);
}

#endif /* CONFIG_SMC_BENCH_SECURE_CYCLE */

/*
 * Dump the L1 shared buffer.
 */
void SCXLNXDumpL1SharedBuffer(struct SCHANNEL_C1S_BUFFER *pBuf)
{
	dprintk(KERN_INFO
		"buffer@%p:\n"
		"  nConfigFlags_S=%08X\n"
		"  sVersionDescription=%64s\n"
		"  nStatus_S=%08X\n"
		"  nSyncSerial_N=%08X\n"
		"  nSyncSerial_S=%08X\n"
		"  sTime_N[0]=%016llX\n"
		"  sTime_N[1]=%016llX\n"
		"  sTimeout_S[0]=%016llX\n"
		"  sTimeout_S[1]=%016llX\n"
		"  nFirstCommand=%08X\n"
		"  nFirstFreeCommand=%08X\n"
		"  nFirstAnswer=%08X\n"
		"  nFirstFreeAnswer=%08X\n\n",
		pBuf,
		pBuf->nConfigFlags_S,
		pBuf->sVersionDescription,
		pBuf->nStatus_S,
		pBuf->nSyncSerial_N,
		pBuf->nSyncSerial_S,
		pBuf->sTime_N[0],
		pBuf->sTime_N[1],
		pBuf->sTimeout_S[0],
		pBuf->sTimeout_S[1],
		pBuf->nFirstCommand,
		pBuf->nFirstFreeCommand,
		pBuf->nFirstAnswer,
		pBuf->nFirstFreeAnswer);
}


/*
 * Dump the specified SChannel message using dprintk.
 */
void SCXLNXDumpMessage(union SCX_COMMAND_MESSAGE *pMessage)
{
	u32 i;

	dprintk(KERN_INFO "message@%p:\n", pMessage);

	switch (pMessage->sHeader.nMessageType) {
	case SCX_MESSAGE_TYPE_CREATE_DEVICE_CONTEXT:
		dprintk(KERN_INFO
			"   nMessageSize             = 0x%02X\n"
			"   nMessageType             = 0x%02X "
				"SCX_MESSAGE_TYPE_CREATE_DEVICE_CONTEXT\n"
			"   nOperationID             = 0x%08X\n"
			"   nDeviceContextID         = 0x%08X\n",
			pMessage->sHeader.nMessageSize,
			pMessage->sHeader.nMessageType,
			pMessage->sHeader.nOperationID,
			pMessage->sCreateDeviceContextMessage.nDeviceContextID
		);
		break;

	case SCX_MESSAGE_TYPE_DESTROY_DEVICE_CONTEXT:
		dprintk(KERN_INFO
			"   nMessageSize    = 0x%02X\n"
			"   nMessageType    = 0x%02X "
				"SCX_MESSAGE_TYPE_DESTROY_DEVICE_CONTEXT\n"
			"   nOperationID    = 0x%08X\n"
			"   hDeviceContext  = 0x%08X\n",
			pMessage->sHeader.nMessageSize,
			pMessage->sHeader.nMessageType,
			pMessage->sHeader.nOperationID,
			pMessage->sDestroyDeviceContextMessage.hDeviceContext);
		break;

	case SCX_MESSAGE_TYPE_OPEN_CLIENT_SESSION:
		dprintk(KERN_INFO
			"   nMessageSize                = 0x%02X\n"
			"   nMessageType                = 0x%02X "
				"SCX_MESSAGE_TYPE_OPEN_CLIENT_SESSION\n"
			"   nParamTypes                 = 0x%04X\n"
			"   nOperationID                = 0x%08X\n"
			"   hDeviceContext              = 0x%08X\n"
			"   nCancellationID             = 0x%08X\n"
			"   sTimeout                    = 0x%016llX\n"
			"   sDestinationUUID            = "
				"%08X-%04X-%04X-%02X%02X-"
				"%02X%02X%02X%02X%02X%02X\n",
			pMessage->sHeader.nMessageSize,
			pMessage->sHeader.nMessageType,
			pMessage->sOpenClientSessionMessage.nParamTypes,
			pMessage->sHeader.nOperationID,
			pMessage->sOpenClientSessionMessage.hDeviceContext,
			pMessage->sOpenClientSessionMessage.nCancellationID,
			pMessage->sOpenClientSessionMessage.sTimeout,
			pMessage->sOpenClientSessionMessage.sDestinationUUID.
				time_low,
			pMessage->sOpenClientSessionMessage.sDestinationUUID.
				time_mid,
			pMessage->sOpenClientSessionMessage.sDestinationUUID.
				time_hi_and_version,
			pMessage->sOpenClientSessionMessage.sDestinationUUID.
				clock_seq_and_node[0],
			pMessage->sOpenClientSessionMessage.sDestinationUUID.
				clock_seq_and_node[1],
			pMessage->sOpenClientSessionMessage.sDestinationUUID.
				clock_seq_and_node[2],
			pMessage->sOpenClientSessionMessage.sDestinationUUID.
				clock_seq_and_node[3],
			pMessage->sOpenClientSessionMessage.sDestinationUUID.
				clock_seq_and_node[4],
			pMessage->sOpenClientSessionMessage.sDestinationUUID.
				clock_seq_and_node[5],
			pMessage->sOpenClientSessionMessage.sDestinationUUID.
				clock_seq_and_node[6],
			pMessage->sOpenClientSessionMessage.sDestinationUUID.
				clock_seq_and_node[7]
		);

		for (i = 0; i < 4; i++) {
			uint32_t *pParam = (uint32_t *) &pMessage->
				sOpenClientSessionMessage.sParams[i];
			dprintk(KERN_INFO "   sParams[%d] = "
				"0x%08X:0x%08X:0x%08X\n",
				i, pParam[0], pParam[1], pParam[2]);
		}

		switch (SCX_LOGIN_GET_MAIN_TYPE(
			pMessage->sOpenClientSessionMessage.nLoginType)) {
		case SCX_LOGIN_PUBLIC:
			dprintk(
				KERN_INFO "   nLoginType               = "
					"SCX_LOGIN_PUBLIC\n");
			break;
		case SCX_LOGIN_USER:
			dprintk(
				KERN_INFO "   nLoginType               = "
					"SCX_LOGIN_USER\n");
			break;
		 case SCX_LOGIN_GROUP:
			dprintk(
				KERN_INFO "   nLoginType               = "
					"SCX_LOGIN_GROUP\n");
			break;
		case SCX_LOGIN_APPLICATION:
			dprintk(
				KERN_INFO "   nLoginType               = "
					"SCX_LOGIN_APPLICATION\n");
			break;
		case SCX_LOGIN_APPLICATION_USER:
			dprintk(
				KERN_INFO "   nLoginType               = "
					"SCX_LOGIN_APPLICATION_USER\n");
			break;
		case SCX_LOGIN_APPLICATION_GROUP:
			dprintk(
				KERN_INFO "   nLoginType               = "
					"SCX_LOGIN_APPLICATION_GROUP\n");
			break;
		case SCX_LOGIN_AUTHENTICATION:
			dprintk(
				KERN_INFO "   nLoginType               = "
					"SCX_LOGIN_AUTHENTICATION\n");
			break;
		case SCX_LOGIN_PRIVILEGED:
			dprintk(
				KERN_INFO "   nLoginType               = "
					"SCX_LOGIN_PRIVILEGED\n");
			break;
		default:
			dprintk(
				KERN_ERR "   nLoginType               = "
					"0x%08X (Unknown login type)\n",
				pMessage->sOpenClientSessionMessage.nLoginType);
			break;
		}

		dprintk(
			KERN_INFO "   sLoginData               = ");
		for (i = 0; i < 20; i++)
			dprintk(
				KERN_INFO "%d",
				pMessage->sOpenClientSessionMessage.
					sLoginData[i]);
		dprintk("\n");
		break;

	case SCX_MESSAGE_TYPE_CLOSE_CLIENT_SESSION:
		dprintk(KERN_INFO
			"   nMessageSize                = 0x%02X\n"
			"   nMessageType                = 0x%02X "
				"SCX_MESSAGE_TYPE_CLOSE_CLIENT_SESSION\n"
			"   nOperationID                = 0x%08X\n"
			"   hDeviceContext              = 0x%08X\n"
			"   hClientSession              = 0x%08X\n",
			pMessage->sHeader.nMessageSize,
			pMessage->sHeader.nMessageType,
			pMessage->sHeader.nOperationID,
			pMessage->sCloseClientSessionMessage.hDeviceContext,
			pMessage->sCloseClientSessionMessage.hClientSession
		);
		break;

	case SCX_MESSAGE_TYPE_REGISTER_SHARED_MEMORY:
		dprintk(KERN_INFO
			"   nMessageSize             = 0x%02X\n"
			"   nMessageType             = 0x%02X "
				"SCX_MESSAGE_TYPE_REGISTER_SHARED_MEMORY\n"
			"   nMemoryFlags             = 0x%04X\n"
			"   nOperationID             = 0x%08X\n"
			"   hDeviceContext           = 0x%08X\n"
			"   nBlockID                 = 0x%08X\n"
			"   nSharedMemSize           = 0x%08X\n"
			"   nSharedMemStartOffset    = 0x%08X\n"
			"   nSharedMemDescriptors[0] = 0x%08X\n"
			"   nSharedMemDescriptors[1] = 0x%08X\n"
			"   nSharedMemDescriptors[2] = 0x%08X\n"
			"   nSharedMemDescriptors[3] = 0x%08X\n"
			"   nSharedMemDescriptors[4] = 0x%08X\n"
			"   nSharedMemDescriptors[5] = 0x%08X\n"
			"   nSharedMemDescriptors[6] = 0x%08X\n"
			"   nSharedMemDescriptors[7] = 0x%08X\n",
			pMessage->sHeader.nMessageSize,
			pMessage->sHeader.nMessageType,
			pMessage->sRegisterSharedMemoryMessage.nMemoryFlags,
			pMessage->sHeader.nOperationID,
			pMessage->sRegisterSharedMemoryMessage.hDeviceContext,
			pMessage->sRegisterSharedMemoryMessage.nBlockID,
			pMessage->sRegisterSharedMemoryMessage.nSharedMemSize,
			pMessage->sRegisterSharedMemoryMessage.
				nSharedMemStartOffset,
			pMessage->sRegisterSharedMemoryMessage.
				nSharedMemDescriptors[0],
			pMessage->sRegisterSharedMemoryMessage.
				nSharedMemDescriptors[1],
			pMessage->sRegisterSharedMemoryMessage.
				nSharedMemDescriptors[2],
			pMessage->sRegisterSharedMemoryMessage.
				nSharedMemDescriptors[3],
			pMessage->sRegisterSharedMemoryMessage.
				nSharedMemDescriptors[4],
			pMessage->sRegisterSharedMemoryMessage.
				nSharedMemDescriptors[5],
			pMessage->sRegisterSharedMemoryMessage.
				nSharedMemDescriptors[6],
			pMessage->sRegisterSharedMemoryMessage.
				nSharedMemDescriptors[7]);
		break;

	case SCX_MESSAGE_TYPE_RELEASE_SHARED_MEMORY:
		dprintk(KERN_INFO
			"   nMessageSize    = 0x%02X\n"
			"   nMessageType    = 0x%02X "
				"SCX_MESSAGE_TYPE_RELEASE_SHARED_MEMORY\n"
			"   nOperationID    = 0x%08X\n"
			"   hDeviceContext  = 0x%08X\n"
			"   hBlock          = 0x%08X\n",
			pMessage->sHeader.nMessageSize,
			pMessage->sHeader.nMessageType,
			pMessage->sHeader.nOperationID,
			pMessage->sReleaseSharedMemoryMessage.hDeviceContext,
			pMessage->sReleaseSharedMemoryMessage.hBlock);
		break;

	case SCX_MESSAGE_TYPE_INVOKE_CLIENT_COMMAND:
		dprintk(KERN_INFO
			 "   nMessageSize                = 0x%02X\n"
			"   nMessageType                = 0x%02X "
				"SCX_MESSAGE_TYPE_INVOKE_CLIENT_COMMAND\n"
			"   nParamTypes                 = 0x%04X\n"
			"   nOperationID                = 0x%08X\n"
			"   hDeviceContext              = 0x%08X\n"
			"   hClientSession              = 0x%08X\n"
			"   sTimeout                    = 0x%016llX\n"
			"   nCancellationID             = 0x%08X\n"
			"   nClientCommandIdentifier    = 0x%08X\n",
			pMessage->sHeader.nMessageSize,
			pMessage->sHeader.nMessageType,
			pMessage->sInvokeClientCommandMessage.nParamTypes,
			pMessage->sHeader.nOperationID,
			pMessage->sInvokeClientCommandMessage.hDeviceContext,
			pMessage->sInvokeClientCommandMessage.hClientSession,
			pMessage->sInvokeClientCommandMessage.sTimeout,
			pMessage->sInvokeClientCommandMessage.nCancellationID,
			pMessage->sInvokeClientCommandMessage.
				nClientCommandIdentifier
		);

		for (i = 0; i < 4; i++) {
			uint32_t *pParam = (uint32_t *) &pMessage->
				sOpenClientSessionMessage.sParams[i];
			dprintk(KERN_INFO "   sParams[%d] = "
				"0x%08X:0x%08X:0x%08X\n", i,
				pParam[0], pParam[1], pParam[2]);
		}
		break;

	case SCX_MESSAGE_TYPE_CANCEL_CLIENT_COMMAND:
		dprintk(KERN_INFO
			"   nMessageSize       = 0x%02X\n"
			"   nMessageType       = 0x%02X "
				"SCX_MESSAGE_TYPE_CANCEL_CLIENT_COMMAND\n"
			"   nOperationID       = 0x%08X\n"
			"   hDeviceContext     = 0x%08X\n"
			"   hClientSession     = 0x%08X\n",
			pMessage->sHeader.nMessageSize,
			pMessage->sHeader.nMessageType,
			pMessage->sHeader.nOperationID,
			pMessage->sCancelClientOperationMessage.hDeviceContext,
			pMessage->sCancelClientOperationMessage.hClientSession);
		break;

	case SCX_MESSAGE_TYPE_MANAGEMENT:
		dprintk(KERN_INFO
			"   nMessageSize             = 0x%02X\n"
			"   nMessageType             = 0x%02X "
				"SCX_MESSAGE_TYPE_MANAGEMENT\n"
			"   nOperationID             = 0x%08X\n"
			"   nCommand                 = 0x%08X\n"
			"   nW3BSize                 = 0x%08X\n"
			"   nW3BStartOffset          = 0x%08X\n",
			pMessage->sHeader.nMessageSize,
			pMessage->sHeader.nMessageType,
			pMessage->sHeader.nOperationID,
			pMessage->sManagementMessage.nCommand,
			pMessage->sManagementMessage.nW3BSize,
			pMessage->sManagementMessage.nW3BStartOffset);
		break;

	default:
		dprintk(
			KERN_ERR "   nMessageType = 0x%08X "
				"(Unknown message type)\n",
			pMessage->sHeader.nMessageType);
		break;
	}
}


/*
 * Dump the specified SChannel answer using dprintk.
 */
void SCXLNXDumpAnswer(union SCX_ANSWER_MESSAGE *pAnswer)
{
	u32 i;
	dprintk(
		KERN_INFO "answer@%p:\n",
		pAnswer);

	switch (pAnswer->sHeader.nMessageType) {
	case SCX_MESSAGE_TYPE_CREATE_DEVICE_CONTEXT:
		dprintk(KERN_INFO
			"   nMessageSize    = 0x%02X\n"
			"   nMessageType    = 0x%02X "
				"SCX_ANSWER_CREATE_DEVICE_CONTEXT\n"
			"   nOperationID    = 0x%08X\n"
			"   nErrorCode      = 0x%08X\n"
			"   hDeviceContext  = 0x%08X\n",
			pAnswer->sHeader.nMessageSize,
			pAnswer->sHeader.nMessageType,
			pAnswer->sHeader.nOperationID,
			pAnswer->sCreateDeviceContextAnswer.nErrorCode,
			pAnswer->sCreateDeviceContextAnswer.hDeviceContext);
		break;

	case SCX_MESSAGE_TYPE_DESTROY_DEVICE_CONTEXT:
		dprintk(KERN_INFO
			"   nMessageSize     = 0x%02X\n"
			"   nMessageType     = 0x%02X "
				"ANSWER_DESTROY_DEVICE_CONTEXT\n"
			"   nOperationID     = 0x%08X\n"
			"   nErrorCode       = 0x%08X\n"
			"   nDeviceContextID = 0x%08X\n",
			pAnswer->sHeader.nMessageSize,
			pAnswer->sHeader.nMessageType,
			pAnswer->sHeader.nOperationID,
			pAnswer->sDestroyDeviceContextAnswer.nErrorCode,
			pAnswer->sDestroyDeviceContextAnswer.nDeviceContextID);
		break;


	case SCX_MESSAGE_TYPE_OPEN_CLIENT_SESSION:
		dprintk(KERN_INFO
			"   nMessageSize      = 0x%02X\n"
			"   nMessageType      = 0x%02X "
				"SCX_ANSWER_OPEN_CLIENT_SESSION\n"
			"   nReturnOrigin     = 0x%02X\n"
			"   nOperationID      = 0x%08X\n"
			"   nErrorCode        = 0x%08X\n"
			"   hClientSession    = 0x%08X\n",
			pAnswer->sHeader.nMessageSize,
			pAnswer->sHeader.nMessageType,
			pAnswer->sOpenClientSessionAnswer.nReturnOrigin,
			pAnswer->sHeader.nOperationID,
			pAnswer->sOpenClientSessionAnswer.nErrorCode,
			pAnswer->sOpenClientSessionAnswer.hClientSession);
		for (i = 0; i < 4; i++) {
			dprintk(KERN_INFO "   sAnswers[%d]=0x%08X:0x%08X\n",
				i,
				pAnswer->sOpenClientSessionAnswer.sAnswers[i].
					sValue.a,
				pAnswer->sOpenClientSessionAnswer.sAnswers[i].
					sValue.b);
		}
		break;

	case SCX_MESSAGE_TYPE_CLOSE_CLIENT_SESSION:
		dprintk(KERN_INFO
			"   nMessageSize      = 0x%02X\n"
			"   nMessageType      = 0x%02X "
				"ANSWER_CLOSE_CLIENT_SESSION\n"
			"   nOperationID      = 0x%08X\n"
			"   nErrorCode        = 0x%08X\n",
			pAnswer->sHeader.nMessageSize,
			pAnswer->sHeader.nMessageType,
			pAnswer->sHeader.nOperationID,
			pAnswer->sCloseClientSessionAnswer.nErrorCode);
		break;

	case SCX_MESSAGE_TYPE_REGISTER_SHARED_MEMORY:
		dprintk(KERN_INFO
			"   nMessageSize    = 0x%02X\n"
			"   nMessageType    = 0x%02X "
				"SCX_ANSWER_REGISTER_SHARED_MEMORY\n"
			"   nOperationID    = 0x%08X\n"
			"   nErrorCode      = 0x%08X\n"
			"   hBlock          = 0x%08X\n",
			pAnswer->sHeader.nMessageSize,
			pAnswer->sHeader.nMessageType,
			pAnswer->sHeader.nOperationID,
			pAnswer->sRegisterSharedMemoryAnswer.nErrorCode,
			pAnswer->sRegisterSharedMemoryAnswer.hBlock);
		break;

	case SCX_MESSAGE_TYPE_RELEASE_SHARED_MEMORY:
		dprintk(KERN_INFO
			"   nMessageSize    = 0x%02X\n"
			"   nMessageType    = 0x%02X "
				"ANSWER_RELEASE_SHARED_MEMORY\n"
			"   nOperationID    = 0x%08X\n"
			"   nErrorCode      = 0x%08X\n"
			"   nBlockID        = 0x%08X\n",
			pAnswer->sHeader.nMessageSize,
			pAnswer->sHeader.nMessageType,
			pAnswer->sHeader.nOperationID,
			pAnswer->sReleaseSharedMemoryAnswer.nErrorCode,
			pAnswer->sReleaseSharedMemoryAnswer.nBlockID);
		break;

	case SCX_MESSAGE_TYPE_INVOKE_CLIENT_COMMAND:
		dprintk(KERN_INFO
			"   nMessageSize      = 0x%02X\n"
			"   nMessageType      = 0x%02X "
				"SCX_ANSWER_INVOKE_CLIENT_COMMAND\n"
			"   nReturnOrigin     = 0x%02X\n"
			"   nOperationID      = 0x%08X\n"
			"   nErrorCode        = 0x%08X\n",
			pAnswer->sHeader.nMessageSize,
			pAnswer->sHeader.nMessageType,
			pAnswer->sInvokeClientCommandAnswer.nReturnOrigin,
			pAnswer->sHeader.nOperationID,
			pAnswer->sInvokeClientCommandAnswer.nErrorCode
			);
		for (i = 0; i < 4; i++) {
			dprintk(KERN_INFO "   sAnswers[%d]=0x%08X:0x%08X\n",
				i,
				pAnswer->sInvokeClientCommandAnswer.sAnswers[i].
					sValue.a,
				pAnswer->sInvokeClientCommandAnswer.sAnswers[i].
					sValue.b);
		}
		break;

	case SCX_MESSAGE_TYPE_CANCEL_CLIENT_COMMAND:
		dprintk(KERN_INFO
			"   nMessageSize      = 0x%02X\n"
			"   nMessageType      = 0x%02X "
				"SCX_ANSWER_CANCEL_CLIENT_COMMAND\n"
			"   nOperationID      = 0x%08X\n"
			"   nErrorCode        = 0x%08X\n",
			pAnswer->sHeader.nMessageSize,
			pAnswer->sHeader.nMessageType,
			pAnswer->sHeader.nOperationID,
			pAnswer->sCancelClientOperationAnswer.nErrorCode);
		break;

	case SCX_MESSAGE_TYPE_MANAGEMENT:
		dprintk(KERN_INFO
			"   nMessageSize      = 0x%02X\n"
			"   nMessageType      = 0x%02X "
				"SCX_MESSAGE_TYPE_MANAGEMENT\n"
			"   nOperationID      = 0x%08X\n"
			"   nErrorCode        = 0x%08X\n",
			pAnswer->sHeader.nMessageSize,
			pAnswer->sHeader.nMessageType,
			pAnswer->sHeader.nOperationID,
			pAnswer->sHeader.nErrorCode);
		break;

	default:
		dprintk(
			KERN_ERR "   nMessageType = 0x%02X "
				"(Unknown message type)\n",
			pAnswer->sHeader.nMessageType);
		break;

	}
}

#endif  /* defined(TF_DRIVER_DEBUG_SUPPORT) */

/*----------------------------------------------------------------------------
 * SHA-1 implementation
 * This is taken from the Linux kernel source crypto/sha1.c
 *----------------------------------------------------------------------------*/

struct sha1_ctx {
	u64 count;
	u32 state[5];
	u8 buffer[64];
};

static inline u32 rol(u32 value, u32 bits)
{
	return ((value) << (bits)) | ((value) >> (32 - (bits)));
}

/* blk0() and blk() perform the initial expand. */
/* I got the idea of expanding during the round function from SSLeay */
#define blk0(i) block32[i]

#define blk(i) (block32[i & 15] = rol( \
	block32[(i + 13) & 15] ^ block32[(i + 8) & 15] ^ \
	block32[(i + 2) & 15] ^ block32[i & 15], 1))

/* (R0+R1), R2, R3, R4 are the different operations used in SHA1 */
#define R0(v, w, x, y, z, i) do { \
	z += ((w & (x ^ y)) ^ y) + blk0(i) + 0x5A827999 + rol(v, 5); \
	w = rol(w, 30); } while (0)

#define R1(v, w, x, y, z, i) do { \
	z += ((w & (x ^ y)) ^ y) + blk(i) + 0x5A827999 + rol(v, 5); \
	w = rol(w, 30); } while (0)

#define R2(v, w, x, y, z, i) do { \
	z += (w ^ x ^ y) + blk(i) + 0x6ED9EBA1 + rol(v, 5); \
	w = rol(w, 30); } while (0)

#define R3(v, w, x, y, z, i) do { \
	z += (((w | x) & y) | (w & x)) + blk(i) + 0x8F1BBCDC + rol(v, 5); \
	w = rol(w, 30); } while (0)

#define R4(v, w, x, y, z, i) do { \
	z += (w ^ x ^ y) + blk(i) + 0xCA62C1D6 + rol(v, 5); \
	w = rol(w, 30); } while (0)


/* Hash a single 512-bit block. This is the core of the algorithm. */
static void sha1_transform(u32 *state, const u8 *in)
{
	u32 a, b, c, d, e;
	u32 block32[16];

	/* convert/copy data to workspace */
	for (a = 0; a < sizeof(block32)/sizeof(u32); a++)
		block32[a] = ((u32) in[4 * a]) << 24 |
			     ((u32) in[4 * a + 1]) << 16 |
			     ((u32) in[4 * a + 2]) <<  8 |
			     ((u32) in[4 * a + 3]);

	/* Copy context->state[] to working vars */
	a = state[0];
	b = state[1];
	c = state[2];
	d = state[3];
	e = state[4];

	/* 4 rounds of 20 operations each. Loop unrolled. */
	R0(a, b, c, d, e, 0); R0(e, a, b, c, d, 1);
	R0(d, e, a, b, c, 2); R0(c, d, e, a, b, 3);
	R0(b, c, d, e, a, 4); R0(a, b, c, d, e, 5);
	R0(e, a, b, c, d, 6); R0(d, e, a, b, c, 7);
	R0(c, d, e, a, b, 8); R0(b, c, d, e, a, 9);
	R0(a, b, c, d, e, 10); R0(e, a, b, c, d, 11);
	R0(d, e, a, b, c, 12); R0(c, d, e, a, b, 13);
	R0(b, c, d, e, a, 14); R0(a, b, c, d, e, 15);

	R1(e, a, b, c, d, 16); R1(d, e, a, b, c, 17);
	R1(c, d, e, a, b, 18); R1(b, c, d, e, a, 19);

	R2(a, b, c, d, e, 20); R2(e, a, b, c, d, 21);
	R2(d, e, a, b, c, 22); R2(c, d, e, a, b, 23);
	R2(b, c, d, e, a, 24); R2(a, b, c, d, e, 25);
	R2(e, a, b, c, d, 26); R2(d, e, a, b, c, 27);
	R2(c, d, e, a, b, 28); R2(b, c, d, e, a, 29);
	R2(a, b, c, d, e, 30); R2(e, a, b, c, d, 31);
	R2(d, e, a, b, c, 32); R2(c, d, e, a, b, 33);
	R2(b, c, d, e, a, 34); R2(a, b, c, d, e, 35);
	R2(e, a, b, c, d, 36); R2(d, e, a, b, c, 37);
	R2(c, d, e, a, b, 38); R2(b, c, d, e, a, 39);

	R3(a, b, c, d, e, 40); R3(e, a, b, c, d, 41);
	R3(d, e, a, b, c, 42); R3(c, d, e, a, b, 43);
	R3(b, c, d, e, a, 44); R3(a, b, c, d, e, 45);
	R3(e, a, b, c, d, 46); R3(d, e, a, b, c, 47);
	R3(c, d, e, a, b, 48); R3(b, c, d, e, a, 49);
	R3(a, b, c, d, e, 50); R3(e, a, b, c, d, 51);
	R3(d, e, a, b, c, 52); R3(c, d, e, a, b, 53);
	R3(b, c, d, e, a, 54); R3(a, b, c, d, e, 55);
	R3(e, a, b, c, d, 56); R3(d, e, a, b, c, 57);
	R3(c, d, e, a, b, 58); R3(b, c, d, e, a, 59);

	R4(a, b, c, d, e, 60); R4(e, a, b, c, d, 61);
	R4(d, e, a, b, c, 62); R4(c, d, e, a, b, 63);
	R4(b, c, d, e, a, 64); R4(a, b, c, d, e, 65);
	R4(e, a, b, c, d, 66); R4(d, e, a, b, c, 67);
	R4(c, d, e, a, b, 68); R4(b, c, d, e, a, 69);
	R4(a, b, c, d, e, 70); R4(e, a, b, c, d, 71);
	R4(d, e, a, b, c, 72); R4(c, d, e, a, b, 73);
	R4(b, c, d, e, a, 74); R4(a, b, c, d, e, 75);
	R4(e, a, b, c, d, 76); R4(d, e, a, b, c, 77);
	R4(c, d, e, a, b, 78); R4(b, c, d, e, a, 79);

	/* Add the working vars back into context.state[] */
	state[0] += a;
	state[1] += b;
	state[2] += c;
	state[3] += d;
	state[4] += e;
	/* Wipe variables */
	a = b = c = d = e = 0;
	memset(block32, 0x00, sizeof(block32));
}


static void sha1_init(void *ctx)
{
	struct sha1_ctx *sctx = ctx;
	static const struct sha1_ctx initstate = {
		0,
		{ 0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0 },
		{ 0, }
	};

	*sctx = initstate;
}


static void sha1_update(void *ctx, const u8 *data, unsigned int len)
{
	struct sha1_ctx *sctx = ctx;
	unsigned int i, j;

	j = (sctx->count >> 3) & 0x3f;
	sctx->count += len << 3;

	if ((j + len) > 63) {
		memcpy(&sctx->buffer[j], data, (i = 64 - j));
		sha1_transform(sctx->state, sctx->buffer);
		for ( ; i + 63 < len; i += 64)
			sha1_transform(sctx->state, &data[i]);
		j = 0;
	} else
		i = 0;
	memcpy(&sctx->buffer[j], &data[i], len - i);
}


/* Add padding and return the message digest. */
static void sha1_final(void *ctx, u8 *out)
{
	struct sha1_ctx *sctx = ctx;
	u32 i, j, index, padlen;
	u64 t;
	u8 bits[8] = { 0, };
	static const u8 padding[64] = { 0x80, };

	t = sctx->count;
	bits[7] = 0xff & t; t >>= 8;
	bits[6] = 0xff & t; t >>= 8;
	bits[5] = 0xff & t; t >>= 8;
	bits[4] = 0xff & t; t >>= 8;
	bits[3] = 0xff & t; t >>= 8;
	bits[2] = 0xff & t; t >>= 8;
	bits[1] = 0xff & t; t >>= 8;
	bits[0] = 0xff & t;

	/* Pad out to 56 mod 64 */
	index = (sctx->count >> 3) & 0x3f;
	padlen = (index < 56) ? (56 - index) : ((64+56) - index);
	sha1_update(sctx, padding, padlen);

	/* Append length */
	sha1_update(sctx, bits, sizeof(bits));

	/* Store state in digest */
	for (i = j = 0; i < 5; i++, j += 4) {
		u32 t2 = sctx->state[i];
		out[j+3] = t2 & 0xff; t2 >>= 8;
		out[j+2] = t2 & 0xff; t2 >>= 8;
		out[j+1] = t2 & 0xff; t2 >>= 8;
		out[j] = t2 & 0xff;
	}

	/* Wipe context */
	memset(sctx, 0, sizeof(*sctx));
}




/*----------------------------------------------------------------------------
 * Process identification
 *----------------------------------------------------------------------------*/

/* This function generates a processes hash table for authentication */
int SCXLNXConnGetCurrentProcessHash(void *pHash)
{
	int nResult = 0;
	void *buffer;
	struct mm_struct *mm;
	struct vm_area_struct *vma;

	buffer = internal_kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (buffer == NULL) {
		dprintk(
			KERN_ERR "SCXLNXConnGetCurrentProcessHash:"
			KERN_ERR " Out of memory for buffer!\n");
		return -ENOMEM;
	}

	mm = current->mm;

	down_read(&(mm->mmap_sem));
	for (vma = mm->mmap; vma != NULL; vma = vma->vm_next) {
		if ((vma->vm_flags & VM_EXECUTABLE) != 0 && vma->vm_file
				!= NULL) {
			struct dentry *dentry;
			unsigned long start;
			unsigned long cur;
			unsigned long end;
			struct sha1_ctx sha1Context;

			dentry = dget(vma->vm_file->f_dentry);

			dprintk(
				KERN_DEBUG "SCXLNXConnGetCurrentProcessHash: "
					"Found executable VMA for inode %lu "
					"(%lu bytes).\n",
					dentry->d_inode->i_ino,
					(unsigned long) (dentry->d_inode->
						i_size));

			start = do_mmap(vma->vm_file, 0,
				dentry->d_inode->i_size,
				PROT_READ | PROT_WRITE | PROT_EXEC,
				MAP_PRIVATE, 0);
			if (start < 0) {
				dprintk(
					KERN_ERR "SCXLNXConnGetCurrentProcess"
					"Hash: do_mmap failed (error %d)!\n",
					(int) start);
				dput(dentry);
				nResult = -EFAULT;
				goto vma_out;
			}

			end = start + dentry->d_inode->i_size;

			sha1_init(&sha1Context);
			cur = start;
			while (cur < end) {
				unsigned long chunk;

				chunk = end - cur;
				if (chunk > PAGE_SIZE)
					chunk = PAGE_SIZE;
				if (copy_from_user(buffer, (const void *) cur,
						chunk) != 0) {
					dprintk(
						KERN_ERR "SCXLNXConnGetCurrent"
						"ProcessHash: copy_from_user "
						"failed!\n");
					nResult = -EINVAL;
					(void) do_munmap(mm, start,
						dentry->d_inode->i_size);
					dput(dentry);
					goto vma_out;
				}
				sha1_update(&sha1Context, buffer, chunk);
				cur += chunk;
			}
			sha1_final(&sha1Context, pHash);
			nResult = 0;

			(void) do_munmap(mm, start, dentry->d_inode->i_size);
			dput(dentry);
			break;
		}
	}
vma_out:
	up_read(&(mm->mmap_sem));

	internal_kfree(buffer);

	if (nResult == -ENOENT)
		dprintk(
			KERN_ERR "SCXLNXConnGetCurrentProcessHash: "
				"No executable VMA found for process!\n");
	return nResult;
}


/* This function hashes the path of the current application.
 * If pData = NULL ,nothing else is added to the hash
		else add pData to the hash
  */
int SCXLNXConnHashApplicationPathAndData(char *pBuffer, void *pData,
	u32 nDataLen)
{
	int nResult = -ENOENT;
	char *buffer = NULL;
	struct mm_struct *mm;
	struct vm_area_struct *vma;

	buffer = internal_kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (buffer == NULL) {
		nResult = -ENOMEM;
		goto end;
	}

	mm = current->mm;

	down_read(&(mm->mmap_sem));
	for (vma = mm->mmap; vma != NULL; vma = vma->vm_next) {
		if ((vma->vm_flags & VM_EXECUTABLE) != 0
				&& vma->vm_file != NULL) {
			struct path *path;
			char *endpath;
			size_t pathlen;
			struct sha1_ctx sha1Context;
			u8 pHashData[SHA1_DIGEST_SIZE];

			path = &vma->vm_file->f_path;

			endpath = d_path(path, buffer, PAGE_SIZE);
			if (IS_ERR(path)) {
				nResult = PTR_ERR(endpath);
				up_read(&(mm->mmap_sem));
				goto end;
			}
			pathlen = (buffer + PAGE_SIZE) - endpath;

#ifdef CONFIG_TF_DRIVER_DEBUG_SUPPORT
			{
				char *pChar;
				dprintk(KERN_DEBUG "current process path = ");
				for (pChar = endpath;
				     pChar < buffer + PAGE_SIZE;
				     pChar++)
					dprintk("%c", *pChar);

				dprintk(", uid=%d, euid=%d\n", current_uid(),
					current_euid());
			}
#endif				/*defined(CONFIG_TF_DRIVER_DEBUG_SUPPORT) */

			sha1_init(&sha1Context);
			sha1_update(&sha1Context, endpath, pathlen);
			if (pData != NULL) {
				dprintk(KERN_INFO "SCXLNXConnHashApplication"
					"PathAndData:  Hashing additional"
					"data\n");
				sha1_update(&sha1Context, pData, nDataLen);
			}
			sha1_final(&sha1Context, pHashData);
			memcpy(pBuffer, pHashData, sizeof(pHashData));

			nResult = 0;

			break;
		}
	}
	up_read(&(mm->mmap_sem));

 end:
	if (buffer != NULL)
		internal_kfree(buffer);

	return nResult;
}

void *internal_kmalloc(size_t nSize, int nPriority)
{
	void *pResult;
	struct SCXLNX_DEVICE *pDevice = SCXLNXGetDevice();

	pResult = kmalloc(nSize, nPriority);

	if (pResult != NULL)
		atomic_inc(
			&pDevice->sDeviceStats.stat_memories_allocated);

	return pResult;
}

void internal_kfree(void *pMemory)
{
	struct SCXLNX_DEVICE *pDevice = SCXLNXGetDevice();

	if (pMemory != NULL)
		atomic_dec(
			&pDevice->sDeviceStats.stat_memories_allocated);
	return kfree(pMemory);
}

void internal_vunmap(void *pMemory)
{
	struct SCXLNX_DEVICE *pDevice = SCXLNXGetDevice();

	if (pMemory != NULL)
		atomic_dec(
			&pDevice->sDeviceStats.stat_memories_allocated);

	vunmap((void *) (((unsigned int)pMemory) & 0xFFFFF000));
}

void *internal_vmalloc(size_t nSize)
{
	void *pResult;
	struct SCXLNX_DEVICE *pDevice = SCXLNXGetDevice();

	pResult = vmalloc(nSize);

	if (pResult != NULL)
		atomic_inc(
			&pDevice->sDeviceStats.stat_memories_allocated);

	return pResult;
}

void internal_vfree(void *pMemory)
{
	struct SCXLNX_DEVICE *pDevice = SCXLNXGetDevice();

	if (pMemory != NULL)
		atomic_dec(
			&pDevice->sDeviceStats.stat_memories_allocated);
	return vfree(pMemory);
}

unsigned long internal_get_zeroed_page(int nPriority)
{
	unsigned long nResult;
	struct SCXLNX_DEVICE *pDevice = SCXLNXGetDevice();

	nResult = get_zeroed_page(nPriority);

	if (nResult != 0)
		atomic_inc(&pDevice->sDeviceStats.
				stat_pages_allocated);

	return nResult;
}

void internal_free_page(unsigned long pPage)
{
	struct SCXLNX_DEVICE *pDevice = SCXLNXGetDevice();

	if (pPage != 0)
		atomic_dec(
			&pDevice->sDeviceStats.stat_pages_allocated);
	return free_page(pPage);
}

int internal_get_user_pages(
		struct task_struct *tsk,
		struct mm_struct *mm,
		unsigned long start,
		int len,
		int write,
		int force,
		struct page **pages,
		struct vm_area_struct **vmas)
{
	int nResult;
	struct SCXLNX_DEVICE *pDevice = SCXLNXGetDevice();

	nResult = get_user_pages(
		tsk,
		mm,
		start,
		len,
		write,
		force,
		pages,
		vmas);

	if (nResult > 0)
		atomic_add(nResult,
			&pDevice->sDeviceStats.stat_pages_locked);

	return nResult;
}

void internal_get_page(struct page *page)
{
	struct SCXLNX_DEVICE *pDevice = SCXLNXGetDevice();

	atomic_inc(&pDevice->sDeviceStats.stat_pages_locked);

	get_page(page);
}

void internal_page_cache_release(struct page *page)
{
	struct SCXLNX_DEVICE *pDevice = SCXLNXGetDevice();

	atomic_dec(&pDevice->sDeviceStats.stat_pages_locked);

	page_cache_release(page);
}


