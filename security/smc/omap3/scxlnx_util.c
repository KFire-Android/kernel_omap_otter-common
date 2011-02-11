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

#include <linux/mman.h>
#include "scxlnx_util.h"
#include "scxlnx_defs.h"

/*----------------------------------------------------------------------------
 *Debug printing routines
 *-------------------------------------------------------------------------- */
#ifdef DEBUG

/*
 *Dump the L1 shared buffer.
 */
void SCXLNXDumpL1SharedBuffer(SCHANNEL_C1S_BUFFER *pBuf)
{
	dprintk(KERN_DEBUG "buffer@%p: \n", pBuf);

	dprintk(KERN_DEBUG "  nConfigFlags_S=%08X\n"
		KERN_DEBUG "  sVersionDescription=%s\n"
		KERN_DEBUG "  nStatus_S=%08X\n"
		KERN_DEBUG "  nSyncSerial_N=%08X\n"
		KERN_DEBUG "  nSyncSerial_S=%08X\n"
		KERN_DEBUG "  sTime_N[0]=%08X%08X\n"
		KERN_DEBUG "  sTime_N[1]=%08X%08X\n"
		KERN_DEBUG "  sTimeout_S[0]=%08X%08X\n"
		KERN_DEBUG "  sTimeout_S[1]=%08X%08X\n"
		KERN_DEBUG "  nFirstCommand=%08X\n"
		KERN_DEBUG "  nFirstFreeCommand=%08X\n"
		KERN_DEBUG "  nFirstAnswer=%08X\n"
		KERN_DEBUG "  nFirstFreeAnswer=%08X\n\n",
		pBuf->nConfigFlags_S,
		pBuf->sVersionDescription,
		pBuf->nStatus_S,
		pBuf->nSyncSerial_N,
		pBuf->nSyncSerial_S,
		pBuf->sTime_N[0].nTime[0], pBuf->sTime_N[0].nTime[1],
		pBuf->sTime_N[1].nTime[0], pBuf->sTime_N[1].nTime[1],
		pBuf->sTimeout_S[0].nTime[0], pBuf->sTimeout_S[0].nTime[1],
		pBuf->sTimeout_S[1].nTime[0], pBuf->sTimeout_S[1].nTime[1],
		pBuf->nFirstCommand,
		pBuf->nFirstFreeCommand,
		pBuf->nFirstAnswer, pBuf->nFirstFreeAnswer);

}

/*
 *Dump the specified SChannel message using dprintk.
 */
void SCXLNXDumpMessage(SCX_COMMAND_MESSAGE *pMessage)
{
	SCX_UUID *pIdentifier;
	u8 *pHash;

	dprintk(KERN_DEBUG "message@%p: \n", pMessage);

	switch (pMessage->nMessageType) {
	case SCX_MESSAGE_TYPE_CREATE_DEVICE_CONTEXT:
		dprintk(KERN_DEBUG "   nMessageType             = \
			SCX_MESSAGE_TYPE_CREATE_DEVICE_CONTEXT\n"
			KERN_DEBUG "   nOperationID             = 0x%08X\n"
			KERN_DEBUG "   nSharedMemDescriptors[0] = 0x%08X\n"
			KERN_DEBUG "   nSharedMemDescriptors[1] = 0x%08X\n"
			KERN_DEBUG "   nSharedMemDescriptors[2] = 0x%08X\n"
			KERN_DEBUG "   nSharedMemDescriptors[3] = 0x%08X\n"
			KERN_DEBUG "   nSharedMemDescriptors[4] = 0x%08X\n"
			KERN_DEBUG "   nSharedMemDescriptors[5] = 0x%08X\n"
			KERN_DEBUG "   nSharedMemDescriptors[6] = 0x%08X\n"
			KERN_DEBUG "   nSharedMemDescriptors[7] = 0x%08X\n"
			KERN_DEBUG "   nSharedMemStartOffset    = 0x%08X\n"
			KERN_DEBUG "   nSharedMemSize         = 0x%08X\n",
			pMessage->nOperationID,
			pMessage->sBody.sCreateDeviceContextMessage.
			nSharedMemDescriptors[0],
			pMessage->sBody.sCreateDeviceContextMessage.
			nSharedMemDescriptors[1],
			pMessage->sBody.sCreateDeviceContextMessage.
			nSharedMemDescriptors[2],
			pMessage->sBody.sCreateDeviceContextMessage.
			nSharedMemDescriptors[3],
			pMessage->sBody.sCreateDeviceContextMessage.
			nSharedMemDescriptors[4],
			pMessage->sBody.sCreateDeviceContextMessage.
			nSharedMemDescriptors[5],
			pMessage->sBody.sCreateDeviceContextMessage.
			nSharedMemDescriptors[6],
			pMessage->sBody.sCreateDeviceContextMessage.
			nSharedMemDescriptors[7],
			pMessage->sBody.sCreateDeviceContextMessage.
			nSharedMemStartOffset,
			pMessage->sBody.sCreateDeviceContextMessage.
			nSharedMemSize);
		break;

	case SCX_MESSAGE_TYPE_DESTROY_DEVICE_CONTEXT:
		dprintk(KERN_DEBUG "   nMessageType    = \
			SCX_MESSAGE_TYPE_DESTROY_DEVICE_CONTEXT\n"
			KERN_DEBUG "   nOperationID    = 0x%08X\n"
			KERN_DEBUG "   hDeviceContext  = 0x%08X\n",
			pMessage->nOperationID,
			pMessage->sBody.sDestroyDeviceContextMessage.
			hDeviceContext);
		break;

	case SCX_MESSAGE_TYPE_OPEN_CLIENT_SESSION:
		dprintk(KERN_DEBUG "   nMessageType                = \
			SCX_MESSAGE_TYPE_OPEN_CLIENT_SESSION\n"
			KERN_DEBUG "   nOperationID             = 0x%08X\n"
			KERN_DEBUG "   hDeviceContext           = 0x%08X\n"
			KERN_DEBUG "   nClientOperationID       = 0x%08X\n"
			KERN_DEBUG "   sTimeout                 = 0x%08X%08X\n"
			KERN_DEBUG "   nClientParameterStartOffset = 0x%08X\n"
			KERN_DEBUG "   nClientParameterSize      = 0x%08X\n"
			KERN_DEBUG "   nClientAnswerStartOffset  = 0x%08X\n"
			KERN_DEBUG "   nClientAnswerSizeMax      = 0x%08X\n",
			pMessage->nOperationID,
			pMessage->sBody.sOpenClientSessionMessage.
			hDeviceContext,
			pMessage->sBody.sOpenClientSessionMessage.
			nClientOperationID,
			pMessage->sBody.sOpenClientSessionMessage.sTimeout[0],
			pMessage->sBody.sOpenClientSessionMessage.sTimeout[1],
			pMessage->sBody.sOpenClientSessionMessage.
			nClientParameterStartOffset,
			pMessage->sBody.sOpenClientSessionMessage.
			nClientParameterSize,
			pMessage->sBody.sOpenClientSessionMessage.
			nClientAnswerStartOffset,
			pMessage->sBody.sOpenClientSessionMessage.
			nClientAnswerSizeMax);

		switch (pMessage->sBody.sOpenClientSessionMessage.nLoginType) {
		case S_LOGIN_PUBLIC:
			dprintk(KERN_DEBUG "   nLoginType              = \
				S_LOGIN_PUBLIC\n");
			break;

		case S_LOGIN_OS_IDENTIFICATION:
			pIdentifier = (SCX_UUID *)&(pMessage->
				sBody.sOpenClientSessionMessage.sLoginData[0]);
			dprintk(KERN_DEBUG "   nLoginType              = \
				S_LOGIN_OS_IDENTIFICATION\n   \
				sUUID                  = \
			%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X\n",
				pIdentifier->time_low, pIdentifier->time_mid,
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

		case S_LOGIN_OS_GROUP_IDENTIFICATION:
			pIdentifier =
				(SCX_UUID *)&(pMessage->
				sBody.sOpenClientSessionMessage.sLoginData[0]);
			dprintk(KERN_DEBUG "   nLoginType              = \
				S_LOGIN_OS_GROUP_IDENTIFICATION\n"
				KERN_DEBUG
				"   sUUID                  = \
			%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X\n",
				pIdentifier->time_low, pIdentifier->time_mid,
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
			pHash =
				 &(pMessage->sBody.sOpenClientSessionMessage.
					sLoginData[0]);
			dprintk(KERN_DEBUG
				"   nLoginType              = \
				S_LOGIN_AUTHENTICATION\n"
				KERN_DEBUG "   ssClientHash             = \
				%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X\
				%02X%02X%02X%02X%02X%02X%02X%02X%02X\n",
				pHash[0], pHash[1], pHash[2], pHash[3],
				pHash[4], pHash[5], pHash[6], pHash[7],
				pHash[8], pHash[9], pHash[10], pHash[11],
				pHash[12], pHash[13], pHash[14], pHash[15],
				pHash[16], pHash[17], pHash[18], pHash[19]);
			break;

		case S_LOGIN_PRIVILEGED:
			dprintk(KERN_DEBUG
				"   nLoginType              = \
				S_LOGIN_PRIVILEGED\n");
			break;

		default:
			dprintk(KERN_ERR
				"  nLoginType              = 0x%08X \
				(Unknown login type)\n",
				pMessage->sBody.sOpenClientSessionMessage.
				nLoginType);
			break;
		}
		break;

	case SCX_MESSAGE_TYPE_CLOSE_CLIENT_SESSION:
		dprintk(KERN_DEBUG
			"   nMessageType                = \
			SCX_MESSAGE_TYPE_CLOSE_CLIENT_SESSION\n"
			KERN_DEBUG "   nOperationID              = 0x%08X\n"
			KERN_DEBUG "   hDeviceContext            = 0x%08X\n"
			KERN_DEBUG "   hClientSession            = 0x%08X\n"
			KERN_DEBUG "   nClientParameterStartOffset = 0x%08X\n"
			KERN_DEBUG "   nClientParameterSize        = 0x%08X\n"
			KERN_DEBUG "   nClientAnswerStartOffset    = 0x%08X\n"
			KERN_DEBUG "   nClientAnswerSizeMax        = 0x%08X\n",
			pMessage->nOperationID,
			pMessage->sBody.sCloseClientSessionMessage.
			hDeviceContext,
			pMessage->sBody.sCloseClientSessionMessage.
			hClientSession,
			pMessage->sBody.sCloseClientSessionMessage.
			nClientParameterStartOffset,
			pMessage->sBody.sCloseClientSessionMessage.
			nClientParameterSize,
			pMessage->sBody.sCloseClientSessionMessage.
			nClientAnswerStartOffset,
			pMessage->sBody.sCloseClientSessionMessage.
			nClientAnswerSizeMax);
		break;

	case SCX_MESSAGE_TYPE_REGISTER_SHARED_MEMORY:
		dprintk(KERN_DEBUG
			"   nMessageType             = \
			SCX_MESSAGE_TYPE_REGISTER_SHARED_MEMORY\n"
			KERN_DEBUG "   nOperationID             = 0x%08X\n"
			KERN_DEBUG "   hDeviceContext         = 0x%08X\n"
			KERN_DEBUG "   hClientSession         = 0x%08X\n"
			KERN_DEBUG "   nBlockID               = 0x%08X\n"
			KERN_DEBUG "   nSharedMemDescriptors[0] = 0x%08X\n"
			KERN_DEBUG "   nSharedMemDescriptors[1] = 0x%08X\n"
			KERN_DEBUG "   nSharedMemDescriptors[2] = 0x%08X\n"
			KERN_DEBUG "   nSharedMemDescriptors[3] = 0x%08X\n"
			KERN_DEBUG "   nSharedMemDescriptors[4] = 0x%08X\n"
			KERN_DEBUG "   nSharedMemDescriptors[5] = 0x%08X\n"
			KERN_DEBUG "   nSharedMemDescriptors[6] = 0x%08X\n"
			KERN_DEBUG "   nSharedMemDescriptors[7] = 0x%08X\n"
			KERN_DEBUG "   nSharedMemStartOffset    = 0x%08X\n"
			KERN_DEBUG "   nSharedMemSize         = 0x%08X\n"
			KERN_DEBUG "   nFlags                   = 0x%08X\n",
			pMessage->nOperationID,
			pMessage->sBody.sRegisterSharedMemoryMessage.
			hDeviceContext,
			pMessage->sBody.sRegisterSharedMemoryMessage.
			hClientSession,
			pMessage->sBody.sRegisterSharedMemoryMessage.nBlockID,
			pMessage->sBody.sRegisterSharedMemoryMessage.
			nSharedMemDescriptors[0],
			pMessage->sBody.sRegisterSharedMemoryMessage.
			nSharedMemDescriptors[1],
			pMessage->sBody.sRegisterSharedMemoryMessage.
			nSharedMemDescriptors[2],
			pMessage->sBody.sRegisterSharedMemoryMessage.
			nSharedMemDescriptors[3],
			pMessage->sBody.sRegisterSharedMemoryMessage.
			nSharedMemDescriptors[4],
			pMessage->sBody.sRegisterSharedMemoryMessage.
			nSharedMemDescriptors[5],
			pMessage->sBody.sRegisterSharedMemoryMessage.
			nSharedMemDescriptors[6],
			pMessage->sBody.sRegisterSharedMemoryMessage.
			nSharedMemDescriptors[7],
			pMessage->sBody.sRegisterSharedMemoryMessage.
			nSharedMemStartOffset,
			pMessage->sBody.sRegisterSharedMemoryMessage.
			nSharedMemSize,
			pMessage->sBody.sRegisterSharedMemoryMessage.nFlags);
		break;

	case SCX_MESSAGE_TYPE_RELEASE_SHARED_MEMORY:
		dprintk(KERN_DEBUG
			"   nMessageType    = \
			SCX_MESSAGE_TYPE_RELEASE_SHARED_MEMORY\n"
			KERN_DEBUG "   nOperationID    = 0x%08X\n"
			KERN_DEBUG "   hDeviceContext  = 0x%08X\n"
			KERN_DEBUG "   hBlock          = 0x%08X\n",
			pMessage->nOperationID,
			pMessage->sBody.sReleaseSharedMemoryMessage.
			hDeviceContext,
			pMessage->sBody.sReleaseSharedMemoryMessage.hBlock);
		break;

	case SCX_MESSAGE_TYPE_INVOKE_CLIENT_COMMAND:
		dprintk(KERN_DEBUG
			"   nMessageType                = \
			SCX_MESSAGE_TYPE_INVOKE_CLIENT_COMMAND\n"
			KERN_DEBUG "   nOperationID             = 0x%08X\n"
			KERN_DEBUG "   hDeviceContext           = 0x%08X\n"
			KERN_DEBUG "   hClientSession           = 0x%08X\n"
			KERN_DEBUG "   sTimeout                 = 0x%08X%08X\n"
			KERN_DEBUG "   nClientOperationID          = 0x%08X\n"
			KERN_DEBUG "   nClientCommandIdentifier    = 0x%08X\n"
			KERN_DEBUG "   nClientParameterBlock       = 0x%08X\n"
			KERN_DEBUG "   nClientParameterStartOffset = 0x%08X\n"
			KERN_DEBUG "   nClientParameterSize      = 0x%08X\n"
			KERN_DEBUG "   hClientAnswerBlock        = 0x%08X\n"
			KERN_DEBUG "   nClientAnswerStartOffset  = 0x%08X\n"
			KERN_DEBUG "   nClientAnswerSizeMax      = 0x%08X\n",
			pMessage->nOperationID,
			pMessage->sBody.sInvokeClientCommandMessage.
			hDeviceContext,
			pMessage->sBody.sInvokeClientCommandMessage.
			hClientSession,
			pMessage->sBody.sInvokeClientCommandMessage.
				sTimeout[0],
			pMessage->sBody.sInvokeClientCommandMessage.
				sTimeout[1],
			pMessage->sBody.sInvokeClientCommandMessage.
			nClientOperationID,
			pMessage->sBody.sInvokeClientCommandMessage.
			nClientCommandIdentifier,
			pMessage->sBody.sInvokeClientCommandMessage.
			nClientParameterBlock,
			pMessage->sBody.sInvokeClientCommandMessage.
			nClientParameterStartOffset,
			pMessage->sBody.sInvokeClientCommandMessage.
			nClientParameterSize,
			pMessage->sBody.sInvokeClientCommandMessage.
			hClientAnswerBlock,
			pMessage->sBody.sInvokeClientCommandMessage.
			nClientAnswerStartOffset,
			pMessage->sBody.sInvokeClientCommandMessage.
			nClientAnswerSizeMax);
		break;

	case SCX_MESSAGE_TYPE_CANCEL_CLIENT_COMMAND:
		dprintk(KERN_DEBUG
			"   nMessageType       = \
			SCX_MESSAGE_TYPE_CANCEL_CLIENT_COMMAND\n"
			KERN_DEBUG "   nOperationID       = 0x%08X\n"
			KERN_DEBUG "   hDeviceContext     = 0x%08X\n"
			KERN_DEBUG "   hClientSession     = 0x%08X\n"
			KERN_DEBUG "   nClientOperationID = 0x%08X\n",
			pMessage->nOperationID,
			pMessage->sBody.sCancelClientOperationMessage.
			hDeviceContext,
			pMessage->sBody.sCancelClientOperationMessage.
			hClientSession,
			pMessage->sBody.sCancelClientOperationMessage.
			nClientOperationID);
		break;

	case SCX_MESSAGE_TYPE_POWER_MANAGEMENT:
		dprintk(KERN_DEBUG
			"   nMessageType             = \
			SCX_MESSAGE_TYPE_POWER_MANAGEMENT\n"
			KERN_DEBUG "   nOperationID             = 0x%08X\n"
			KERN_DEBUG "   nPowerCommand           = 0x%08X\n"
			KERN_DEBUG "   nSharedMemDescriptors[0] = 0x%08X\n"
			KERN_DEBUG "   nSharedMemDescriptors[1] = 0x%08X\n"
			KERN_DEBUG "   nSharedMemStartOffset    = 0x%08X\n"
			KERN_DEBUG "   nSharedMemSize         = 0x%08X\n",
			pMessage->nOperationID,
			pMessage->sBody.sPowerManagementMessage.nPowerCommand,
			pMessage->sBody.sPowerManagementMessage.
			nSharedMemDescriptors[0],
			pMessage->sBody.sPowerManagementMessage.
			nSharedMemDescriptors[1],
			pMessage->sBody.sPowerManagementMessage.
			nSharedMemStartOffset,
			pMessage->sBody.sPowerManagementMessage.
			nSharedMemSize);
		break;

	default:
		dprintk(KERN_ERR
			"  nMessageType = 0x%08X (Unknown message type)\n",
			pMessage->nMessageType);
		break;
	}
}

/*
 *Dump the specified SChannel answer using dprintk.
 */
void SCXLNXDumpAnswer(SCX_ANSWER_MESSAGE *pAnswer)
{
	dprintk(KERN_DEBUG "answer@%p: \n", pAnswer);

	switch (pAnswer->nMessageType) {
	case SCX_MESSAGE_TYPE_CREATE_DEVICE_CONTEXT:
		dprintk(KERN_DEBUG
			"   nMessageType    = \
			SCX_ANSWER_CREATE_DEVICE_CONTEXT\n"
			KERN_DEBUG "   nOperationID    = 0x%08X\n"
			KERN_DEBUG "   nSChannelStatus = 0x%08X\n"
			KERN_DEBUG "   hDeviceContext  = 0x%08X\n",
			pAnswer->nOperationID,
			pAnswer->nSChannelStatus,
			pAnswer->sBody.sCreateDeviceContextAnswer.
			hDeviceContext);
		break;

	case SCX_MESSAGE_TYPE_DESTROY_DEVICE_CONTEXT:
		dprintk(KERN_DEBUG
			"   nMessageType    = ANSWER_DESTROY_DEVICE_CONTEXT\n"
			KERN_DEBUG "   nOperationID    = 0x%08X\n"
			KERN_DEBUG "   nSChannelStatus = 0x%08X\n",
			pAnswer->nOperationID,
			pAnswer->nSChannelStatus);
		break;

	case SCX_MESSAGE_TYPE_OPEN_CLIENT_SESSION:
		dprintk(KERN_DEBUG
			"   nMessageType      = \
			SCX_ANSWER_OPEN_CLIENT_SESSION\n"
			KERN_DEBUG "   nOperationID      = 0x%08X\n"
			KERN_DEBUG "   nSChannelStatus   = 0x%08X\n"
			KERN_DEBUG "   nFrameworkStatus  = 0x%08X\n"
			KERN_DEBUG "   nServiceError    = 0x%08X\n"
			KERN_DEBUG "   hClientSession  = 0x%08X\n"
			KERN_DEBUG "   nClientAnswerSize = 0x%08X\n",
			pAnswer->nOperationID, pAnswer->nSChannelStatus,
			pAnswer->sBody.sOpenClientSessionAnswer.
			nFrameworkStatus,
			pAnswer->sBody.sOpenClientSessionAnswer.nServiceError,
			pAnswer->sBody.sOpenClientSessionAnswer.hClientSession,
			pAnswer->sBody.sOpenClientSessionAnswer.
			nClientAnswerSize);
		break;

	case SCX_MESSAGE_TYPE_CLOSE_CLIENT_SESSION:
		dprintk(KERN_DEBUG
			"   nMessageType      = ANSWER_CLOSE_CLIENT_SESSION\n"
			KERN_DEBUG "   nOperationID      = 0x%08X\n"
			KERN_DEBUG "   nSChannelStatus   = 0x%08X\n"
			KERN_DEBUG "   nFrameworkStatus  = 0x%08X\n"
			KERN_DEBUG "   nServiceError    = 0x%08X\n"
			KERN_DEBUG "   nClientAnswerSize = 0x%08X\n",
			pAnswer->nOperationID, pAnswer->nSChannelStatus,
			pAnswer->sBody.sCloseClientSessionAnswer.
			nFrameworkStatus,
			pAnswer->sBody.sCloseClientSessionAnswer.nServiceError,
			pAnswer->sBody.sCloseClientSessionAnswer.
			nClientAnswerSize);
		break;

	case SCX_MESSAGE_TYPE_REGISTER_SHARED_MEMORY:
		dprintk(KERN_DEBUG
			"   nMessageType    = \
			SCX_ANSWER_REGISTER_SHARED_MEMORY\n"
			KERN_DEBUG "   nOperationID    = 0x%08X\n"
			KERN_DEBUG "   nSChannelStatus = 0x%08X\n"
			KERN_DEBUG "   hBlock          = 0x%08X\n",
			pAnswer->nOperationID,
			pAnswer->nSChannelStatus,
			pAnswer->sBody.sRegisterSharedMemoryAnswer.hBlock);
		break;

	case SCX_MESSAGE_TYPE_RELEASE_SHARED_MEMORY:
		dprintk(KERN_DEBUG
			"   nMessageType    = ANSWER_RELEASE_SHARED_MEMORY\n"
			KERN_DEBUG "   nOperationID    = 0x%08X\n"
			KERN_DEBUG "   nSChannelStatus = 0x%08X\n"
			KERN_DEBUG "   nBlockID      = 0x%08X\n",
			pAnswer->nOperationID,
			pAnswer->nSChannelStatus,
			pAnswer->sBody.sReleaseSharedMemoryAnswer.nBlockID);
		break;

	case SCX_MESSAGE_TYPE_INVOKE_CLIENT_COMMAND:
		dprintk(KERN_DEBUG "   nMessageType      = \
			SCX_ANSWER_INVOKE_CLIENT_COMMAND\n"
			KERN_DEBUG "   nOperationID      = 0x%08X\n"
			KERN_DEBUG "   nSChannelStatus   = 0x%08X\n"
			KERN_DEBUG "   nFrameworkStatus  = 0x%08X\n"
			KERN_DEBUG "   nServiceError    = 0x%08X\n"
			KERN_DEBUG "   nClientAnswerSize = 0x%08X\n",
			pAnswer->nOperationID, pAnswer->nSChannelStatus,
			pAnswer->sBody.sInvokeClientCommandAnswer.
			nFrameworkStatus,
			pAnswer->sBody.sInvokeClientCommandAnswer.
			nServiceError,
			pAnswer->sBody.sInvokeClientCommandAnswer.
			nClientAnswerSize);
		break;

	case SCX_MESSAGE_TYPE_CANCEL_CLIENT_COMMAND:
		dprintk(KERN_DEBUG "   nMessageType      = \
			SCX_ANSWER_CANCEL_CLIENT_COMMAND\n"
			KERN_DEBUG "   nOperationID      = 0x%08X\n"
			KERN_DEBUG "   nSChannelStatus   = 0x%08X\n",
			pAnswer->nOperationID, pAnswer->nSChannelStatus);
		break;

	case SCX_MESSAGE_TYPE_POWER_MANAGEMENT:
		dprintk(KERN_DEBUG "   nMessageType      = \
			SCX_ANSWER_POWER_MANAGEMENT\n"
			KERN_DEBUG "   nOperationID      = 0x%08X\n"
			KERN_DEBUG "   nSChannelStatus   = 0x%08X\n",
			pAnswer->nOperationID, pAnswer->nSChannelStatus);
		break;

	default:
		dprintk(KERN_ERR
			"  nMessageType = 0x%08X (Unknown message type)\n",
			pAnswer->nMessageType);
		break;

	}
}

/*
 * Print memory configuration
 */
void SCXLNXDumpAttributes_CP15(u32 *va)
{
	static u8 staticBuffer[256];
	u8 * pBuffer = (u8 *)staticBuffer;
	u32 inner, outter;
	u32 pa;

	asm volatile ("mcr p15, 0, %0, c7, c8, 0" : : "r" (va));
	asm volatile ("mrc p15, 0, %0, c7, c4, 0" : "=r" (pa));

	if (pa&1) {
		pa = 0; /* abort */
		dprintk(KERN_ERR "SCXLNXDumpAttributes_CP15 ERROR.\n");
		return;
	}

	pa = pa&(0x3FF); /*1111111111*/
	outter = pa>>2;
	outter &= 0x3;
	inner = pa>>4;
	inner &= 7;

	dprintk(KERN_INFO "cache cfg = \n");
	dprintk(KERN_INFO "    pa:0x%x \n", pa);
	dprintk(KERN_INFO "    inner:0x%x \n", inner);
	dprintk(KERN_INFO "    outer:0x%x \n", outter);

}

#endif	/*defined(DEBUG) */

/*----------------------------------------------------------------------------
 *SHA-1 implementation
 *-------------------------------------------------------------------------- */

#define SHA1_DIGEST_SIZE   20

struct sha1_ctx {
	u64 count;
	u32 state[5];
	u8 buffer[64];
};

static inline u32 rol(u32 value, u32 bits)
{
	return ((value) << (bits)) | ((value) >> (32 - (bits)));
}

/*blk0()and blk()perform the initial expand. */
/*I got the idea of expanding during the round function from SSLeay */
# define blk0(i)block32[i]

#define blk(i)(block32[i&15] = rol(block32[(i+13) & 15]^block32[(i+8) & 15] \
	 ^block32[(i+2) & 15]^block32[i&15], 1))

/*(R0+R1), R2, R3, R4 are the different operations used in SHA1 */
#define R0(v, w, x, y, z, i) do {\
	z += ((w & (x^y))^y) + blk0(i) + 0x5A827999+rol(v, 5);\
	 w = rol(w, 30); } while (0);
#define R1(v, w, x, y, z, i) do {\
	z += ((w & (x^y))^y) + blk(i) + 0x5A827999+rol(v, 5);\
	w = rol(w, 30); } while (0);
#define R2(v, w, x, y, z, i) do {\
	z += (w^x^y) + blk(i) + 0x6ED9EBA1+rol(v, 5);\
	w = rol(w, 30); } while (0);
#define R3(v, w, x, y, z, i) do {\
	z += (((w|x) & y) | (w&x)) + blk(i) + 0x8F1BBCDC+rol(v, 5);\
	w = rol(w, 30); } while (0);
#define R4(v, w, x, y, z, i) do {\
	z += (w^x^y) + blk(i) + 0xCA62C1D6+rol(v, 5);\
	w = rol(w, 30); } while (0);

void trans1(u32 *a, u32 *b, u32 *c, u32 *d, u32 *e, u32 *block32)
{
	R2(*e, *a, *b, *c, *d, 21);
	R2(*d, *e, *a, *b, *c, 22);
	R2(*c, *d, *e, *a, *b, 23);
	R2(*b, *c, *d, *e, *a, 24);
	R2(*a, *b, *c, *d, *e, 25);
	R2(*e, *a, *b, *c, *d, 26);
	R2(*d, *e, *a, *b, *c, 27);
	R2(*c, *d, *e, *a, *b, 28);
	R2(*b, *c, *d, *e, *a, 29);
	R2(*a, *b, *c, *d, *e, 30);
	R2(*e, *a, *b, *c, *d, 31);
	R2(*d, *e, *a, *b, *c, 32);
	R2(*c, *d, *e, *a, *b, 33);
	R2(*b, *c, *d, *e, *a, 34);
	R2(*a, *b, *c, *d, *e, 35);
	R2(*e, *a, *b, *c, *d, 36);
	R2(*d, *e, *a, *b, *c, 37);
	R2(*c, *d, *e, *a, *b, 38);
	R2(*b, *c, *d, *e, *a, 39);
	R3(*a, *b, *c, *d, *e, 40);
	R3(*e, *a, *b, *c, *d, 41);
	R3(*d, *e, *a, *b, *c, 42);
	R3(*c, *d, *e, *a, *b, 43);
	R3(*b, *c, *d, *e, *a, 44);
	R3(*a, *b, *c, *d, *e, 45);
	R3(*e, *a, *b, *c, *d, 46);
	R3(*d, *e, *a, *b, *c, 47);
	R3(*c, *d, *e, *a, *b, 48);
	R3(*b, *c, *d, *e, *a, 49);
}
void trans2(u32 *a, u32 *b, u32 *c, u32 *d, u32 *e, u32 *block32)
{
	R3(*a, *b, *c, *d, *e, 50);
	R3(*e, *a, *b, *c, *d, 51);
	R3(*d, *e, *a, *b, *c, 52);
	R3(*c, *d, *e, *a, *b, 53);
	R3(*b, *c, *d, *e, *a, 54);
	R3(*a, *b, *c, *d, *e, 55);
	R3(*e, *a, *b, *c, *d, 56);
	R3(*d, *e, *a, *b, *c, 57);
	R3(*c, *d, *e, *a, *b, 58);
	R3(*b, *c, *d, *e, *a, 59);
	R4(*a, *b, *c, *d, *e, 60);
	R4(*e, *a, *b, *c, *d, 61);
	R4(*d, *e, *a, *b, *c, 62);
	R4(*c, *d, *e, *a, *b, 63);
	R4(*b, *c, *d, *e, *a, 64);
	R4(*a, *b, *c, *d, *e, 65);
	R4(*e, *a, *b, *c, *d, 66);
	R4(*d, *e, *a, *b, *c, 67);
	R4(*c, *d, *e, *a, *b, 68);
	R4(*b, *c, *d, *e, *a, 69);
	R4(*a, *b, *c, *d, *e, 70);
	R4(*e, *a, *b, *c, *d, 71);
	R4(*d, *e, *a, *b, *c, 72);
	R4(*c, *d, *e, *a, *b, 73);
	R4(*b, *c, *d, *e, *a, 74);
	R4(*a, *b, *c, *d, *e, 75);
	R4(*e, *a, *b, *c, *d, 76);
	R4(*d, *e, *a, *b, *c, 77);
	R4(*c, *d, *e, *a, *b, 78);
	R4(*b, *c, *d, *e, *a, 79);
}


/*Hash a single 512-bit block. This is the core of the algorithm. */
static void sha1_transform(u32 *state, const u8 *in)
{
	u32 a, b, c, d, e;
	u32 block32[16];

	/*convert/copy data to workspace */
	for (a = 0; a < sizeof(block32) / sizeof(u32); a++)
		block32[a] =
			 ((u32) in[4 * a]) << 24 |
			 ((u32) in[4 * a + 1]) << 16 |
			 ((u32) in[4 * a + 2]) << 8 | ((u32) in[4 * a + 3]);

	/*Copy context->state[] to working vars */
	a = state[0];
	b = state[1];
	c = state[2];
	d = state[3];
	e = state[4];

	/*4 rounds of 20 operations each. Loop unrolled. */
	R0(a, b, c, d, e, 0);
	R0(e, a, b, c, d, 1);
	R0(d, e, a, b, c, 2);
	R0(c, d, e, a, b, 3);
	R0(b, c, d, e, a, 4);
	R0(a, b, c, d, e, 5);
	R0(e, a, b, c, d, 6);
	R0(d, e, a, b, c, 7);
	R0(c, d, e, a, b, 8);
	R0(b, c, d, e, a, 9);
	R0(a, b, c, d, e, 10);
	R0(e, a, b, c, d, 11);
	R0(d, e, a, b, c, 12);
	R0(c, d, e, a, b, 13);
	R0(b, c, d, e, a, 14);
	R0(a, b, c, d, e, 15);
	R1(e, a, b, c, d, 16);
	R1(d, e, a, b, c, 17);
	R1(c, d, e, a, b, 18);
	R1(b, c, d, e, a, 19);
	R2(a, b, c, d, e, 20);

	trans1(&a, &b, &c, &d, &e, block32);
	trans2(&a, &b, &c, &d, &e, block32);

	/*Add the working vars back into context.state[] */
	state[0] += a;
	state[1] += b;
	state[2] += c;
	state[3] += d;
	state[4] += e;
	/*Wipe variables */
	a = 0;
	b = 0;
	c = 0;
	d = 0;
	e = 0;
	memset(block32, 0x00, sizeof block32);
}

static void sha1_init(void *ctx)
{
	struct sha1_ctx *sctx = ctx;
	static const struct sha1_ctx initstate = {
		0,
		{0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0},
		{0,}
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
		for (; i + 63 < len; i += 64)
			sha1_transform(sctx->state, &data[i]);

		j = 0;
	} else
		i = 0;

	memcpy(&sctx->buffer[j], &data[i], len - i);
}

/*Add padding and return the message digest. */
static void sha1_final(void *ctx, u8 *out)
{
	struct sha1_ctx *sctx = ctx;
	u32 i, j, index, padlen;
	u64 t;
	u8 bits[8] = { 0, };
	static const u8 padding[64] = { 0x80, };

	t = sctx->count;
	bits[7] = 0xff & t;
	t >>= 8;
	bits[6] = 0xff & t;
	t >>= 8;
	bits[5] = 0xff & t;
	t >>= 8;
	bits[4] = 0xff & t;
	t >>= 8;
	bits[3] = 0xff & t;
	t >>= 8;
	bits[2] = 0xff & t;
	t >>= 8;
	bits[1] = 0xff & t;
	t >>= 8;
	bits[0] = 0xff & t;

	/*Pad out to 56 mod 64 */
	index = (sctx->count >> 3) & 0x3f;
	padlen = (index < 56) ? (56 - index) : ((64 + 56) - index);
	sha1_update(sctx, padding, padlen);

	/*Append length */
	sha1_update(sctx, bits, sizeof bits);

	/*Store state in digest */
	for (i = j = 0; i < 5; i++, j += 4) {
		u32 t2 = sctx->state[i];
		out[j + 3] = t2 & 0xff;
		t2 >>= 8;
		out[j + 2] = t2 & 0xff;
		t2 >>= 8;
		out[j + 1] = t2 & 0xff;
		t2 >>= 8;
		out[j] = t2 & 0xff;
	}

	/*Wipe context */
	memset(sctx, 0, sizeof *sctx);
}

/*----------------------------------------------------------------------------
 *Process identification
 *-------------------------------------------------------------------------- */

/*
 *Generates an UUID from the specified SHA1 hash value, as specified in section
 *4.3, Algorithm for Creating a Name-Based UUID, of RFC 4122.
 *
 *This function assumes that the hash value is at least 160-bit long.
 */
static void SCXLNXConnUUIDFromHash(const u8 *pHashData, SCX_UUID *pIdentifier)
{
	/*
	 *Based on the algorithm described in section 4.3,
	 *"Algorithm for Creating a Name-Based UUID", of RFC 4122.
	 */

	pIdentifier->time_low = (((u32) (pHashData[0])) << 24)
		 | (((u32) (pHashData[1])) << 16)
		 | (((u32) (pHashData[2])) << 8)
		 | ((u32) (pHashData[3]));
	pIdentifier->time_mid = (((u16) (pHashData[4])) << 8)
		 | ((u16) (pHashData[5]));
	pIdentifier->time_hi_and_version = (((((u16) (pHashData[6])) << 8)
						  | ((u16) (pHashData[7])))
						 & 0x0FFF)
		 | 0x5000;		/*Version 5 = UUID from SHA-1 hash */
	pIdentifier->clock_seq_and_node[0] = (pHashData[8] & 0x3F) | 0x80;
	pIdentifier->clock_seq_and_node[1] = pHashData[9];
	memcpy(&(pIdentifier->clock_seq_and_node[2]), &(pHashData[10]), 6);
}

/*This function generates a processes hash table for authentication */
int SCXLNXConnGetCurrentProcessHash(void *pHash)
{
	int nResult = 0;
	void *buffer;
	struct mm_struct *mm;
	struct vm_area_struct *vma;

	buffer = internal_kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (buffer == NULL) {
		dprintk(KERN_ERR "SCXLNXConnGetCurrentProcessHash:"
			KERN_ERR " Out of memory for buffer!\n");
		return -ENOMEM;
	}

	mm = current->mm;

	down_read(&(mm->mmap_sem));
	for (vma = mm->mmap; vma != NULL; vma = vma->vm_next) {
		if ((vma->vm_flags & VM_EXECUTABLE) != 0
			 && vma->vm_file != NULL) {
			struct dentry *dentry;
			unsigned long start;
			unsigned long cur;
			unsigned long end;
			struct sha1_ctx sha1Context;

			dentry = dget(vma->vm_file->f_dentry);

			dprintk(KERN_DEBUG
				"SCXLNXConnGetCurrentProcessHash: Found \
				executable VMA for inode %lu (%lu bytes).\n",
				dentry->d_inode->i_ino,
				(unsigned long)(dentry->d_inode->i_size));

			start =
			  do_mmap(vma->vm_file, 0, dentry->d_inode->i_size,
					 PROT_READ | PROT_WRITE | PROT_EXEC,
					 MAP_PRIVATE, 0);
			if ((int)start < 0) {
				dprintk(KERN_ERR
					"SCXLNXConnGetCurrentProcessHash:"
					KERN_ERR
					" do_mmap failed (error %d) !\n",
					(int)start);
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

				if (copy_from_user
				  (buffer, (const void *)cur, chunk) != 0) {
					dprintk(KERN_ERR
					"SCXLNXConnGetCurrentProcessHash: \
					copy_from_user failed!\n");
					nResult = -EINVAL;
					(void)do_munmap(mm, start,
							dentry->d_inode->
							i_size);
					dput(dentry);
					goto vma_out;
				}
				sha1_update(&sha1Context, buffer, chunk);
				cur += chunk;
			}
			sha1_final(&sha1Context, pHash);
			nResult = 0;

			(void)do_munmap(mm, start, dentry->d_inode->i_size);
			dput(dentry);
			break;
		}
	}
 vma_out:
	up_read(&(mm->mmap_sem));

	internal_kfree(buffer);

	if (nResult == -ENOENT) {
		dprintk(KERN_ERR "SCXLNXConnGetCurrentProcessHash:"
			KERN_ERR " No executable VMA found for process!\n");
	}
	return nResult;
}

/*This function checks a process UUID for identification */
int SCXLNXConnGetCurrentProcessUUID(SCX_UUID *pUUID)
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
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 27)
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
#else
			struct vfsmount *vfsmnt;
			struct dentry *dentry;
			char *endpath;
			size_t pathlen;
			struct sha1_ctx sha1Context;
			u8 pHashData[SHA1_DIGEST_SIZE];

			vfsmnt = mntget(vma->vm_file->f_vfsmnt);
			dentry = dget(vma->vm_file->f_dentry);

			endpath = d_path(dentry, vfsmnt, buffer, PAGE_SIZE);
			if (IS_ERR(endpath)) {
				nResult = PTR_ERR(endpath);
				dput(dentry);
				mntput(vfsmnt);
				up_read(&(mm->mmap_sem));
				goto end;
			}
			pathlen = (buffer + PAGE_SIZE) - endpath;
#endif

#ifndef NDEBUG
			{
				char *pChar;
				dprintk(KERN_DEBUG "current process path = ");
				for (pChar = endpath;
				  pChar < buffer + PAGE_SIZE; pChar++) {
					dprintk("%c", *pChar);
				}
				dprintk(", uid=%d, euid=%d\n", CURRENT_UID,
					CURRENT_EUID);
			}
#endif				/*defined(NDEBUG) */

			sha1_init(&sha1Context);
			sha1_update(&sha1Context, endpath, pathlen);
			sha1_update(&sha1Context, (const u8 *)&(CURRENT_UID),
					 sizeof(CURRENT_UID));
			sha1_update(&sha1Context, (const u8 *)&(CURRENT_EUID),
					 sizeof(CURRENT_EUID));
			sha1_final(&sha1Context, pHashData);

			SCXLNXConnUUIDFromHash(pHashData, pUUID);

			nResult = 0;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 27)
			/*Nothing to do */
#else
			dput(dentry);
			mntput(vfsmnt);
#endif

			break;
		}
	}
	up_read(&(mm->mmap_sem));

 end:
	if (buffer != NULL)
		internal_kfree(buffer);

	return nResult;
}

/*
 * Allocates a buffer with the cacheable flag.
 */
void *internal_kmalloc_vmap(void **pBufferRaw, size_t nSize, int nPriority)
{
	void *pResult;
	struct page **page_map;
	pgprot_t prot;
	int nbPages, i;

	dprintk(KERN_INFO "internal_kmalloc_vmap priority=%x size=%x\n",
		nPriority, nSize);
	*pBufferRaw = kmalloc(nSize, nPriority);

	if (*pBufferRaw == NULL) {
		dprintk(KERN_ERR "Cannot allocate %d bytes in \
			internal_get_zeroed_page_vmap\n",
			nSize);
		return NULL;
	}
	atomic_inc(&g_SCXLNXDeviceMonitor.sDeviceStats.
			stat_memories_allocated);

	nbPages = 1 + (((unsigned int)(*pBufferRaw) & 0xFFF) + nSize) / 0x1000;

	dprintk(KERN_INFO "internal_kmalloc_vmap: size %d, pages %d\n", nSize,
		nbPages);
	page_map = vmalloc(nbPages * sizeof(struct page *));
	for (i = 0; i < nbPages; i++) {
		unsigned int temp;
		/*point to each page on which we map */
		temp = ((unsigned int)(*pBufferRaw)) + i * 0x1000;
		page_map[i] =
			pfn_to_page(virt_to_phys((void *)temp) >> PAGE_SHIFT);
	}
	/*
	 * pgprot_kernel is a global variable of pgprot_t type.
	 * It should be declared in pgtable.h
	 */
	prot = pgprot_kernel;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32)
	/*erase current cache config and replace it by WRITETHROUGH */
	prot &= CLEAN_CACHE_CFG_MASK;
	prot |= L_PTE_MT_WRITETHROUGH;
#else
	/*remove bufferable, add cacheable */
	prot ^= L_PTE_BUFFERABLE;
	prot |= L_PTE_CACHEABLE;
#endif
	pResult = vmap(page_map, nbPages, VM_READ | VM_WRITE, prot);

	pResult =
		(void *)(((unsigned int)pResult & 0xFFFFF000) |
			(((unsigned int)(*pBufferRaw)) & 0xFFF));
#ifndef NDEBUG
	{
		unsigned int pa, inner, outter;
		dprintk(KERN_INFO "%d pages\n", nbPages);
		pa = VA2PA(*pBufferRaw, &inner, &outter);
		dprintk(KERN_INFO
			"Allocated : %x/%x/%x/%x [va/pa/inner/outter]\n",
			(unsigned int)*pBufferRaw, pa, inner, outter);
		pa = VA2PA((void *)pResult, &inner, &outter);
		dprintk(KERN_INFO
			"Remapped : %x/%x/%x/%x [va/pa/inner/outter]\n",
			(unsigned int)pResult, pa, inner, outter);

		SCXLNXDumpAttributes_CP15(pResult);
	}
#endif

	/*Note: pBufferRaw needs to be freed together with pResult !! */
	vfree(page_map);
	if (pResult == NULL) {
		kfree(pBufferRaw);
		atomic_dec(&g_SCXLNXDeviceMonitor.sDeviceStats.
				stat_memories_allocated);
		*pBufferRaw = NULL;
	}
	return pResult;
}

void *internal_kmalloc(size_t nSize, int nPriority)
{
	void *pResult;

	pResult = kmalloc(nSize, nPriority);

	if (pResult != NULL) {
		atomic_inc(&g_SCXLNXDeviceMonitor.sDeviceStats.
				stat_memories_allocated);
	}

	return pResult;
}

void internal_kfree(void *pMemory)
{
	if (pMemory != NULL) {
		atomic_dec(&g_SCXLNXDeviceMonitor.sDeviceStats.
				stat_memories_allocated);
	}
	return kfree(pMemory);
}

void internal_vunmap(void *pMemory)
{
	/*
	 *We suppose that the buffer to free was allocated through
	 *internal_kmalloc_vmap.  To free the buffer allocated through vmap,
	 *we need to remove the offset added at the end of
	 *internal_kmalloc_vamp function.
	 */
	if (pMemory != NULL) {
		atomic_dec(&g_SCXLNXDeviceMonitor.sDeviceStats.
				stat_memories_allocated);
	}
	vunmap((void *)(((unsigned int)pMemory) & 0xFFFFF000));
}

void *internal_vmalloc(size_t nSize)
{
	void *pResult;

	pResult = vmalloc(nSize);

	if (pResult != NULL) {
		atomic_inc(&g_SCXLNXDeviceMonitor.sDeviceStats.
				stat_memories_allocated);
	}

	return pResult;
}

void internal_vfree(void *pMemory)
{
	if (pMemory != NULL) {
		atomic_dec(&g_SCXLNXDeviceMonitor.sDeviceStats.
				stat_memories_allocated);
	}
	return vfree(pMemory);
}

unsigned long internal_get_zeroed_page_vmap(void **pBufferRaw, int nPriority)
{
	unsigned long nResult;
	struct page **page_map;
	pgprot_t prot;

	*pBufferRaw = (void *)get_zeroed_page(nPriority);

	if ((unsigned long)*pBufferRaw == 0) {
		dprintk(KERN_ERR "Cannot allocate %lu bytes in \
			internal_get_zeroed_page_vmap\n",
			1 * PAGE_SIZE);
		return 0;
	}
	atomic_inc(&g_SCXLNXDeviceMonitor.sDeviceStats.stat_pages_allocated);

	page_map = vmalloc(1 * sizeof(struct page *));
	page_map[0] = pfn_to_page(virt_to_phys(*pBufferRaw) >> PAGE_SHIFT);

	/*
	 * pgprot_kernel is a global variable of pgprot_t type.
	 * It should be declared in pgtable.h
	 */
	prot = pgprot_kernel;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32)
	/*erase current cache config and replace it by WRITETHROUGH */
	prot &= CLEAN_CACHE_CFG_MASK;
	prot |= L_PTE_MT_WRITETHROUGH;
#else
	/*remove bufferable, add cacheable */
	prot ^= L_PTE_BUFFERABLE;
	prot |= L_PTE_CACHEABLE;
#endif
	nResult = (unsigned int)vmap(page_map, 1, VM_READ | VM_WRITE, prot);

#ifndef NDEBUG
	{
		unsigned int pa, inner, outter;
		pa = VA2PA(*pBufferRaw, &inner, &outter);
		dprintk(KERN_INFO "internal_get_zeroed_page_vmap. Allocated : \
			%x/%x/%x/%x [va/pa/inner/outter]\n",
			(unsigned int)*pBufferRaw, pa, inner, outter);
		pa = VA2PA((void *)nResult, &inner, &outter);
		dprintk(KERN_INFO "internal_get_zeroed_page_vmap. Remapped : \
			%x/%x/%x/%x [va/pa/inner/outter]\n",
			(unsigned int)nResult, pa, inner, outter);

		SCXLNXDumpAttributes_CP15(nResult);
	}
#endif

	vfree(page_map);
	if (nResult == 0) {
		free_page((unsigned long)*pBufferRaw);
		atomic_dec(&g_SCXLNXDeviceMonitor.sDeviceStats.
				stat_pages_allocated);
		*pBufferRaw = NULL;
	}
	return nResult;
}

unsigned long internal_get_zeroed_page(int nPriority)
{
	unsigned long nResult;

	nResult = get_zeroed_page(nPriority);

	if (nResult != 0) {
		atomic_inc(&g_SCXLNXDeviceMonitor.sDeviceStats.
				stat_pages_allocated);
	}

	return nResult;
}

void internal_free_page(unsigned long pPage)
{
	if (pPage != 0) {
		atomic_dec(&g_SCXLNXDeviceMonitor.sDeviceStats.
				stat_pages_allocated);
	}
	return free_page(pPage);
}

void internal_free_page_vunmap(unsigned long pPage)
{
	if (pPage != 0) {
		atomic_dec(&g_SCXLNXDeviceMonitor.sDeviceStats.
				stat_pages_allocated);
	}
	return vunmap((void *)pPage);
}

unsigned long internal_get_free_pages(int nPriority, unsigned int order)
{
	unsigned long nResult;

	nResult = __get_free_pages(nPriority, order);

	if (nResult != 0) {
		atomic_add((0x1 << order),
				&g_SCXLNXDeviceMonitor.sDeviceStats.
				stat_pages_allocated);
	}

	return nResult;
}

unsigned long internal_get_free_pages_vmap(void **pBufferRaw, int nPriority,
						unsigned int order)
{
	unsigned long nResult;
	struct page **page_map;
	int i, nbPages;
	pgprot_t prot;

	dprintk(KERN_INFO
		"internal_get_free_pages_vmap priority=%x order=%x\n",
		nPriority, order);
	*pBufferRaw = (void *)__get_free_pages(nPriority, order);

	if (*pBufferRaw == 0) {
		dprintk(KERN_ERR "Cannot allocate %lu bytes in \
			internal_get_free_pages_vmap\n",
			PAGE_SIZE * (2 << order));
		return 0;
	}
	atomic_add((0x1 << order),
		&g_SCXLNXDeviceMonitor.sDeviceStats.stat_pages_allocated);

	nbPages = 2 << order;
	page_map = vmalloc(nbPages * sizeof(struct page *));

	for (i = 0; i < nbPages; i++) {
		unsigned int temp;
		/*point to each page on which we map */
		temp = ((unsigned int)(*pBufferRaw)) + i * 0x1000;
		page_map[i] =
			 pfn_to_page(virt_to_phys((void *)temp) >> PAGE_SHIFT);
	}

	/*
	 * pgprot_kernel is a global variable of pgprot_t type.
	 * It should be declared in pgtable.h
	 */
	prot = pgprot_kernel;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32)
	/*erase current cache config and replace it by WRITETHROUGH */
	prot &= CLEAN_CACHE_CFG_MASK;
	prot |= L_PTE_MT_WRITETHROUGH;
#else
	/*remove bufferable, add cacheable */
	prot ^= L_PTE_BUFFERABLE;
	prot |= L_PTE_CACHEABLE;
#endif
	nResult = (unsigned int)vmap(page_map, nbPages, VM_READ | VM_WRITE,
									prot);

#ifndef NDEBUG
	{
		unsigned int pa, inner, outter;
		dprintk(KERN_INFO "%d pages. Allocated\n", nbPages);
		pa = VA2PA(*pBufferRaw, &inner, &outter);
		dprintk(KERN_INFO "internal_get_zeroed_page_vmap. Allocated : \
			%x/%x/%x/%x [va/pa/inner/outter]\n",
			(unsigned int)*pBufferRaw, pa, inner, outter);
		pa = VA2PA((void *)nResult, &inner, &outter);
		dprintk(KERN_INFO "internal_get_zeroed_page_vmap. Remapped : \
			%x/%x/%x/%x [va/pa/inner/outter]\n",
			(unsigned int)nResult, pa, inner, outter);

		SCXLNXDumpAttributes_CP15(nResult);
	}
#endif

	vfree(page_map);
	if (nResult == 0) {
		free_page((unsigned long)*pBufferRaw);
		atomic_sub((0x1 << order),
				&g_SCXLNXDeviceMonitor.sDeviceStats.
				stat_pages_allocated);
		*pBufferRaw = NULL;
	}
	return nResult;
}

void internal_free_pages(unsigned long addr, unsigned int order)
{
	if (addr != 0) {
		atomic_sub((0x1 << order),
				&g_SCXLNXDeviceMonitor.sDeviceStats.
				stat_pages_allocated);
	}
	return free_pages(addr, order);
}

int internal_get_user_pages(struct task_struct *tsk,
				struct mm_struct *mm,
				unsigned long start,
				int len,
				int write,
				int force,
				struct page **pages,
				struct vm_area_struct **vmas)
{
	int nResult;

	nResult = get_user_pages(tsk,
				 mm, start, len, write, force, pages, vmas);

	if (nResult > 0) {
		atomic_add(nResult,
				&g_SCXLNXDeviceMonitor.sDeviceStats.
				stat_pages_locked);
	}

	return nResult;
}

void internal_get_page(struct page *page)
{
	atomic_inc(&g_SCXLNXDeviceMonitor.sDeviceStats.stat_pages_locked);

	get_page(page);
}

void internal_page_cache_release(struct page *page)
{
	atomic_dec(&g_SCXLNXDeviceMonitor.sDeviceStats.stat_pages_locked);
	page_cache_release(page);
}

