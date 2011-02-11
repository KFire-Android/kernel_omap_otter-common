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

#ifndef __SCX_SM_PROTOCOL_H__
#define __SCX_SM_PROTOCOL_H__

/*----------------------------------------------------------------------------
 *
 *This header file defines the structure used in the
 *Secure Channel (SCX)protocol.
 *-------------------------------------------------------------------------- */

/*
 *The driver interface version returned by the version ioctl
 */
#define SCX_DRIVER_INTERFACE_VERSION	  0x03000000

/*
 *SM Protocol version handling
 */
#define SCX_SM_S_PROTOCOL_MAJOR_VERSION	(0x03)
#define GET_PROTOCOL_MAJOR_VERSION(a)(a >> 24)
#define GET_PROTOCOL_MINOR_VERSION(a)((a >> 16) & 0xFF)

/*
 *The TimeSlot field of the nSyncSerial_N register.
 */
#define SCX_SM_SYNC_SERIAL_TIMESLOT_N	  (1)

/*
 *nStatus_S related defines.
 */
#define SCX_SM_STATUS_P_MASK		  (0X00000001)
#define SCX_SM_STATUS_POWER_STATE_SHIFT	(3)
#define SCX_SM_STATUS_POWER_STATE_MASK \
				(0x1F << SCX_SM_STATUS_POWER_STATE_SHIFT)

/*
 *Possible power states of the POWER_STATE field of the nStatus_S register
 */
#define SCX_SM_POWER_MODE_ACTIVE	 (3)

/*
 *Possible nPowerCommand values for POWER_MANAGEMENT commands
 */
#define SCPM_PREPARE_SHUTDOWN		  (2)
#define SCPM_RESUME			  (3)

/*
 *The capacity of the Normal Word message queue, in number of slots.
 */
#define SCX_SM_N_MESSAGE_QUEUE_CAPACITY	(32)

/*
 *The capacity of the Secure World message answer queue, in number of slots.
 */
#define SCX_SM_S_ANSWER_QUEUE_CAPACITY	 (32)

/*
 *Identifies the init SMC.
 */
#define SCX_SM_SMC_INIT			(0XFFFFFFFF)

/*
 *representation of an UUID.
 */
typedef struct {
	u32 time_low;
	u16 time_mid;
	u16 time_hi_and_version;
	u8 clock_seq_and_node[8];
} SCX_UUID;

/*
 *Descriptor tables capacity
 */
#define SCX_MAX_COARSE_PAGES	(8)

/*
 *Buffer size for version description fields
 */
#define SCX_DESCRIPTION_BUFFER_LENGTH 64

/*
 *Shared memory type flags.
 */
#define SCX_SHMEM_TYPE_READ		(0x00000001)
#define SCX_SHMEM_TYPE_WRITE		(0x00000002)
#define SCX_SHMEM_TYPE_DIRECT		(0x80000000)
#define SCX_SHMEM_TYPE_DIRECT_FORCE	(0x40000000)

/*
 *Authentication types.
 */
#define S_LOGIN_PUBLIC			(0x00000000)
#define S_LOGIN_OS_IDENTIFICATION	(0x00000001)
#define S_LOGIN_AUTHENTICATION		(0x80000000)
#define S_LOGIN_PRIVILEGED		(0x80000002)
#define S_LOGIN_OS_GROUP_IDENTIFICATION	(0x80000003)

/*The size in bytes of the hash size used for client authentication */
#define SCX_PROCESS_HASH_SIZE  20

/*
 *Authentication and Identification information.
 */
typedef struct {
	u32 nAuthenticationType;
	union {
		SCX_UUID sUUID;
		u8 sClientHash[SCX_PROCESS_HASH_SIZE];
	} info;
} SCX_AUTHENTICATION;

/*
 *The SCX messages types.
 */
#define SCX_MESSAGE_TYPE_CREATE_DEVICE_CONTEXT	1
#define SCX_MESSAGE_TYPE_DESTROY_DEVICE_CONTEXT	2
#define SCX_MESSAGE_TYPE_OPEN_CLIENT_SESSION	3
#define SCX_MESSAGE_TYPE_CLOSE_CLIENT_SESSION	4
#define SCX_MESSAGE_TYPE_REGISTER_SHARED_MEMORY	5
#define SCX_MESSAGE_TYPE_RELEASE_SHARED_MEMORY	6
#define SCX_MESSAGE_TYPE_INVOKE_CLIENT_COMMAND	7
#define SCX_MESSAGE_TYPE_CANCEL_CLIENT_COMMAND	8
#define SCX_MESSAGE_TYPE_POWER_MANAGEMENT	9

/*
 *The error codes
 */
#define S_SUCCESS	0x00000000
#define S_PENDING	0xFFFF2000
#define S_ERROR_SDP_RUNTIME_INIT_ADDR_CHECK_FAIL     0xFFFF3090

/*
 *CREATE_DEVICE_CONTEXT command message.
 */
typedef struct {
	u32 nSharedMemDescriptors[SCX_MAX_COARSE_PAGES];
	u32 nSharedMemStartOffset;
	u32 nSharedMemSize;
} SCX_COMMAND_CREATE_DEVICE_CONTEXT;

/*
 *CREATE_DEVICE_CONTEXT answer message.
 */
typedef struct {
	u32 hDeviceContext;
	u32 nSharedMemDescriptor;
} SCX_ANSWER_CREATE_DEVICE_CONTEXT;

/*
 *DESTROY_DEVICE_CONTEXT command message.
 */
typedef struct {
	u32 hDeviceContext;
} SCX_COMMAND_DESTROY_DEVICE_CONTEXT;

/*
 *DESTROY_DEVICE_CONTEXT answer message.
 *The message does not provide message specific parameters.
 *Therefore no need to define a specific answer structure
 */

/*
 *OPEN_CLIENT_SESSION command message.
 */
typedef struct {
	u32 hDeviceContext;
	u32 nClientOperationID;
	u32 sTimeout[2];
	u32 nClientParameterStartOffset;
	u32 nClientParameterSize;
	u32 nClientAnswerStartOffset;
	u32 nClientAnswerSizeMax;
	u32 nLoginType;
	u8 sLoginData[20];
} SCX_COMMAND_OPEN_CLIENT_SESSION;

/*
 *OPEN_CLIENT_SESSION SM answer message.
 */
typedef struct {
	u32 nFrameworkStatus;
	u32 nServiceError;
	u32 hClientSession;
	u32 nClientAnswerSize;
} SCX_ANSWER_OPEN_CLIENT_SESSION;

/*
 *CLOSE_CLIENT_SESSION SM command message.
 */
typedef struct {
	u32 hDeviceContext;
	u32 hClientSession;
	u32 nClientParameterStartOffset;
	u32 nClientParameterSize;
	u32 nClientAnswerStartOffset;
	u32 nClientAnswerSizeMax;
} SCX_COMMAND_CLOSE_CLIENT_SESSION;

/*
 *CLOSE_CLIENT_SESSION SM answer message.
 */
typedef struct {
	u32 nFrameworkStatus;
	u32 nServiceError;
	u32 nClientAnswerSize;
} SCX_ANSWER_CLOSE_CLIENT_SESSION;

/*
 *REGISTER_SHARED_MEMORY SM message message.
 */
typedef struct {
	u32 hDeviceContext;
	u32 hClientSession;
	u32 nBlockID;
	u32 nSharedMemDescriptors[SCX_MAX_COARSE_PAGES];
	u32 nSharedMemStartOffset;
	u32 nSharedMemSize;
	u32 nFlags;
} SCX_COMMAND_REGISTER_SHARED_MEMORY;

/*
 *REGISTER_SHARED_MEMORY SM answer message.
 */
typedef struct {
	u32 hBlock;
	u32 nSharedMemDescriptor;
} SCX_ANSWER_REGISTER_SHARED_MEMORY;

/*
 *RELEASE_SHARED_MEMORY SM command message.
 */
typedef struct {
	u32 hDeviceContext;
	u32 hBlock;
} SCX_COMMAND_RELEASE_SHARED_MEMORY;

/*
 *RELEASE_SHARED_MEMORY SM answer message.
 */
typedef struct {
	u32 nBlockID;
} SCX_ANSWER_RELEASE_SHARED_MEMORY;

/*
 *INVOKE_CLIENT_COMMAND SM command message.
 */
typedef struct {
	u32 hDeviceContext;
	u32 hClientSession;
	u32 sTimeout[2];
	u32 nClientOperationID;
	u32 nClientCommandIdentifier;
	u32 nClientParameterBlock;
	u32 nClientParameterStartOffset;
	u32 nClientParameterSize;
	u32 hClientAnswerBlock;
	u32 nClientAnswerStartOffset;
	u32 nClientAnswerSizeMax;
} SCX_COMMAND_INVOKE_CLIENT_COMMAND;

/*
 *INVOKE_CLIENT_COMMAND SM command answer.
 */
typedef struct {
	u32 nFrameworkStatus;
	u32 nServiceError;
	u32 nClientAnswerSize;
} SCX_ANSWER_INVOKE_CLIENT_COMMAND;

/*
 *CANCEL_CLIENT_OPERATION SM command message.
 */
typedef struct {
	u32 hDeviceContext;
	u32 hClientSession;
	u32 nClientOperationID;
} SCX_COMMAND_CANCEL_CLIENT_OPERATION;

/*
 *CANCEL_CLIENT_OPERATION answer message.
 *The message does not provide message specific parameters.
 *Therefore no need to define a specific answer structure
 */

/*
 *POWER_MANAGEMENT SM command message.
 */
typedef struct {
	u32 nPowerCommand;
	u32 nSharedMemDescriptors[2];
	u8 sReserved[24];
	u32 nSharedMemStartOffset;
	u32 nSharedMemSize;
} SCX_COMMAND_POWER_MANAGEMENT;

/*
 *POWER_MANAGEMENT answer message.
 *The message does not provide message specific parameters.
 *Therefore no need to define a specific answer structure
 */

/*
 *Structure for L2 messages
 */
typedef struct {
	u32 nMessageType;
	u32 nOperationID;

	union {
	  SCX_COMMAND_CREATE_DEVICE_CONTEXT sCreateDeviceContextMessage;
	  SCX_COMMAND_DESTROY_DEVICE_CONTEXT sDestroyDeviceContextMessage;
	  SCX_COMMAND_OPEN_CLIENT_SESSION sOpenClientSessionMessage;
	  SCX_COMMAND_CLOSE_CLIENT_SESSION sCloseClientSessionMessage;
	  SCX_COMMAND_REGISTER_SHARED_MEMORY sRegisterSharedMemoryMessage;
	  SCX_COMMAND_RELEASE_SHARED_MEMORY sReleaseSharedMemoryMessage;
	  SCX_COMMAND_INVOKE_CLIENT_COMMAND sInvokeClientCommandMessage;
	  SCX_COMMAND_CANCEL_CLIENT_OPERATION
		    sCancelClientOperationMessage;
	  SCX_COMMAND_POWER_MANAGEMENT sPowerManagementMessage;
		/*
		 *guaranty the sBody is 56 bytes long
		 *and the full command message is 64 bytes long
		 */
		u8 sAligned[56];
	} sBody;
} SCX_COMMAND_MESSAGE;

/*
 *Structure for any L2 answer
 */
typedef struct {
	u32 nMessageType;
	u32 nOperationID;
	u32 nSChannelStatus;
	union {
		SCX_ANSWER_CREATE_DEVICE_CONTEXT sCreateDeviceContextAnswer;
		SCX_ANSWER_OPEN_CLIENT_SESSION sOpenClientSessionAnswer;
		SCX_ANSWER_CLOSE_CLIENT_SESSION sCloseClientSessionAnswer;
		SCX_ANSWER_REGISTER_SHARED_MEMORY sRegisterSharedMemoryAnswer;
		SCX_ANSWER_RELEASE_SHARED_MEMORY sReleaseSharedMemoryAnswer;
		SCX_ANSWER_INVOKE_CLIENT_COMMAND sInvokeClientCommandAnswer;
		/*
		 *guaranty the sBody is 20 bytes long
		 *and the full command message is 32 bytes long
		 */
		u8 sAligned[20];
	} sBody;
} SCX_ANSWER_MESSAGE;

/*
 *Structure for time definitions
 */
typedef struct {
	u32 nTime[2];
} SCX_TIME;

/*Structure of the Communication Buffer */
typedef struct {
	u32 nConfigFlags_S;
	u32 nW3BSizeMin_S;
	u8 sReserved1[56];
	u8 sVersionDescription[SCX_DESCRIPTION_BUFFER_LENGTH];
	u32 nStatus_S;
	u32 sReserved2;
	u32 nSyncSerial_N;
	u32 nSyncSerial_S;
	SCX_TIME sTime_N[2];
	SCX_TIME sTimeout_S[2];
	u32 nFirstCommand;
	u32 nFirstFreeCommand;
	u32 nFirstAnswer;
	u32 nFirstFreeAnswer;
	u8 sReserved3[832];
	SCX_COMMAND_MESSAGE sCommandQueue[SCX_SM_N_MESSAGE_QUEUE_CAPACITY];
	SCX_ANSWER_MESSAGE sAnswerQueue[SCX_SM_S_ANSWER_QUEUE_CAPACITY];
} SCHANNEL_C1S_BUFFER;

/*Generic structure of the PA Communication Buffer */
typedef struct {
	u32 nL1Command;
	u8 sReserved[4092];
} SCHANNEL_L0_BUFFER_INPUT;

/*SMC_INIT: Structure of the L0 shared buffer */
typedef struct {
	u32 nL1Command;
	u32 nL1SharedBufferLength;
	u32 nL1SharedBufferPhysAddr;
	u32 nBackingStoreAddr;
	u32 nBackExtStorageAddr;
	u32 nPropertiesBufferLength;
	u8 pPropertiesBuffer[1];
} SCHANNEL_L0_BUFFER_SMC_INIT_INPUT;

typedef struct {
	u32 nL1Status;
	u8 sReserved[4092];
} SCHANNEL_L0_BUFFER_OUTPUT;

/*
 *SCX_VERSION_INFORMATION_BUFFER structure description
 *Description of the sVersionBuffer handed over from user space to kernel space
 *This field is filled by the driver during a CREATE_DEVICE_CONTEXT ioctl
 *and handed back to user space
 */
typedef struct {
	u8 sDriverDescription[SCX_DESCRIPTION_BUFFER_LENGTH];
	u8 sSecureWorldDescription[SCX_DESCRIPTION_BUFFER_LENGTH];
} SCX_VERSION_INFORMATION_BUFFER;

/*
 *The SCX PA Control types.
 */
#define SCX_SMC_PA_CTRL_START	 1
#define SCX_SMC_PA_CTRL_STOP	  2

typedef struct {
	u32 nPACommand;		/*SCX_PA_CTRL_xxx */

	/*For the SCX_SMC_PA_CTRL_START command only*/
	u32 nPASize;		/*PA buffer size*/
	u8 *pPABuffer;		/*PA buffer*/
	u32 nConfSize;		/*Configuration buffer size, including the  */
	/*zero-terminating character (may be zero) */
	u8 *pConfBuffer;	/*Configuration buffer, zero-terminated*/
	/*string (may be NULL) */
	u32 nSDPBackingStoreAddr;
	u32 nSDPBkExtStoreAddr;
	u32 nClockTimerExpireMs;
} SCX_SMC_PA_CTRL;

/*The IOCTLs the driver supports */
#include <linux/ioctl.h>

#define IOCTL_SCX_GET_VERSION \
	_IO('z', 0)

#define IOCTL_SCX_EXCHANGE \
	_IOWR('z', 1, SCX_COMMAND_MESSAGE)

#define IOCTL_SCX_SMC_PA_CTRL \
	_IOWR('z', 0xFF, SCX_SMC_PA_CTRL)

#endif				/*!defined(__SCX_SM_PROTOCOL_H__) */
