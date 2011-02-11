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

#include "scxlnx_defs.h"
#include "scxlnx_util.h"
#include "scx_public_crypto.h"
#include "scx_public_dma.h"

#define S_SUCCESS		0x00000000
#define S_ERROR_GENERIC		0xFFFF0000
#define S_ERROR_ACCESS_DENIED	0xFFFF0001
#define S_ERROR_BAD_FORMAT	0xFFFF0005
#define S_ERROR_BAD_PARAMETERS	0xFFFF0006
#define S_ERROR_OUT_OF_MEMORY	0xFFFF000C
#define S_ERROR_SHORT_BUFFER	0xFFFF0010
#define S_ERROR_UNREACHABLE	0xFFFF3013
#define S_ERROR_SERVICE		0xFFFF1000

#define CKR_OK			0x00000000

#define PUBLIC_CRYPTO_TIMEOUT_CONST	0x000FFFFF

/*---------------------------------------------------------------------------*/
/*RPC IN/OUT structures for CUS implementation                               */
/*---------------------------------------------------------------------------*/

typedef struct {
	u32 nShortcutID;
	u32 nError;
} RPC_INSTALL_SHORTCUT_LOCK_ACCELERATOR_OUT;

typedef struct {
	u32 hDeviceContext;
	u32 hClientSession;
	u32 nCommandID;
	u32 hKeyContext;
	/**
	 *The identifier of the HWA accelerator that this shortcut uses!
	 *Possible values are:
	 *- 1 (RPC_AES_CODE)
	 *- 2 (RPC_DES_CODE)
	 *- 4 (RPC_SHA_CODE)
	 **/
	u32 nHWAID;
	/**
	 *This field defines the algorithm, direction, mode, key size.
	 *It contains some of the bits of the corresponding "CTRL" register
	 *of the accelerator.
	 *
	 *More precisely:
	 *For AES1 accelerator, nHWA_CTRL contains the following bits:
	 *- CTR (bit 6):
	 * when 1, selects CTR mode.
	 * when 0, selects CBC or ECB mode (according to CBC bit)
	 *- CBC (bit 5)
	 * when 1, selects CBC mode (but only if CTR=0)
	 * when 0, selects EBC mode (but only if CTR=0)
	 *- DIRECTION (bit 2)
	 *  0: decryption
	 *  1: encryption
	 *
	 *For the DES2 accelerator, nHWA_CTRL contains the following bits:
	 *- CBC (bit 4): 1 for CBC, 0 for ECB
	 *- DIRECTION (bit 2): 0 for decryption, 1 for encryption
	 *
	 *For the SHAM1 accelerator, nHWA_CTRL contains the following bits:
	 *- ALGO (bit 2:1):
	 *  0x0: MD5
	 *  0x1: SHA1
	 *  0x2: SHA-224
	 *  0x3: SHA-256
	 **/
	u32 nHWA_CTRL;
	PUBLIC_CRYPTO_OPERATION_STATE sOperationState;
} RPC_INSTALL_SHORTCUT_LOCK_ACCELERATOR_IN;

typedef struct {
	PUBLIC_CRYPTO_OPERATION_STATE sOperationState;
} RPC_LOCK_ACCELERATORS_SUSPEND_SHORTCUT_OUT;

typedef struct {
	u32 nShortcutID;
} RPC_LOCK_ACCELERATORS_SUSPEND_SHORTCUT_IN;

typedef struct {
	u32 nShortcutID;
	u32 hAES1KeyContext;
	u32 hD3D2KeyContext;
	PUBLIC_CRYPTO_OPERATION_STATE sOperationState;
} RPC_RESUME_SHORTCUT_UNLOCK_ACCELERATORS_IN;

/*semaphores used to serialize HWA access */
struct semaphore g_sAES1CriticalSection;
struct semaphore g_sD3D2CriticalSection;
struct semaphore g_sSHAM1CriticalSection;

/*-------------------------------------------------------------------------- */

#define LIB_BEF_UINT32_TAG					0x36
#define LIB_BEF_UINT8_ARRAY_TAG			0x1F
#define LIB_BEF_MEMREF_TAG					0x01B6

/*
*Check that a buffer is entirely within another buffer
*/
static bool static_checkBuffer(u32 offset, u32 length, u32 containerLength)
{
	if (offset + length < offset) {
		/*Overflow */
		return false;
	}
	return (int)(offset + length) <= (int)containerLength;
}

/*-------------------------------------------------------------------------- */
/*
 *Read a value on 4 bytes
 */
static u8 *static_befEncoderReadUntaggedUint32(u8 *pDecoderArray, u32 *pnValue)
{
	*pnValue = (((u32) (pDecoderArray[0]))       |
					((u32) (pDecoderArray[1]) << 8)  |
					((u32) (pDecoderArray[2]) << 16) |
					((u32) (pDecoderArray[3]) << 24));
	pDecoderArray += 4;

	return pDecoderArray;
}

/*-------------------------------------------------------------------------- */
/*
 *read uint32_t: TAG[7:0]|UntaggedUint32[31:0]
 */
static u8 *static_befDecoderReadUint32(
						u8 *pDecoderArray,
						u8 *pDecoderArrayEnd,
						u32 *pnValue)
{
	u8 *pNextElement;

	/*Check that we can read the element without overflowing */
	/*5 = tag + u32 element */
	if (!static_checkBuffer(
		(u32)pDecoderArray, 5, (u32)pDecoderArrayEnd))
		return NULL;

	/*check element tag */
	if (*pDecoderArray != LIB_BEF_UINT32_TAG)
		return NULL;

	pDecoderArray++;

	pNextElement = static_befEncoderReadUntaggedUint32(pDecoderArray,
				pnValue);

	/*return pointer on next element, NULL if no more element */
	return pNextElement;
}

/*------------------------------------------------------------------------- */
/*
 *read uint8_t[] TAG[7:0]|nCount[31:0]|Uint8_t[nCount]
 */
static u8 *static_befDecoderReadUint8Array(
						u8 *pDecoderArray,
						u8 *pDecoderArrayEnd,
						u32 *pnValue,
						u32 *pnCount)
{
	u8 *pNextElement;

	/*Check that we can read the tag+nCount without overflowing */
	/*5 = tag + u32 element */
	if (!static_checkBuffer(
		(u32)pDecoderArray, 5, (u32)pDecoderArrayEnd))
		return NULL;

	/*check element tag */
	if (*pDecoderArray != LIB_BEF_UINT8_ARRAY_TAG)
		return NULL;

	pDecoderArray++;

	/*read element size value */
	pNextElement = static_befEncoderReadUntaggedUint32(pDecoderArray,
				pnCount);

	/*Check that we can read the element without overflowing */
	if (!static_checkBuffer((u32)pNextElement, (u32)*pnCount,
		(u32)pDecoderArrayEnd)) {
		return NULL;
	}

	/*set the pnValue */
	*pnValue = (u32)pNextElement;

	/*return pointer on next element,even if no more element */
	return pNextElement + *pnCount;
}

/*-------------------------------------------------------------------------- */
/*
 *read memory reference
 */
static u8 *static_befDecoderReadMemRef(
						u8 *pDecoderArray,
						u8 *pDecoderArrayEnd,
						u32 *phBlock,
						u32 *pnStartOffset,
						u32 *pnSize,
						u32 *pnSharedMemFlags)
{
	u8 *pNextElement;

	/*Check that we can read the element without overflowing */
	/*18 = 4 x u32 + 1 x u16 */
	if (!static_checkBuffer(
		(u32)pDecoderArray, 18, (u32)pDecoderArrayEnd))
		return NULL;

	/*check element tag */
	if (*(u16 *)pDecoderArray != LIB_BEF_MEMREF_TAG)
		return NULL;

	/*read element values */
	pDecoderArray += 2;

	/*memory reference argument validity are checked afterward by caller */
	pNextElement = static_befEncoderReadUntaggedUint32(pDecoderArray,
				phBlock);
	pNextElement = static_befEncoderReadUntaggedUint32(pNextElement,
				pnStartOffset);
	pNextElement = static_befEncoderReadUntaggedUint32(pNextElement,
				pnSize);
	pNextElement = static_befEncoderReadUntaggedUint32(pNextElement,
				pnSharedMemFlags);

	/*return pointer on next element, even if no more element */
	return pNextElement;
}

/*-------------------------------------------------------------------------- */
/*
 *read memory reference or uint32_t
 *Parse the pDecoderArray input string and setup the outputs according the
 *message type (mem ref or uint32)
 *
 *Input  : pDecoderArray -> current message slot pointer
 *       pDecoderArrayEnd -> pointer on the end of the message
 *
 *Output : *pnFlags -> LIB_BEF_MEMREF_TAG | LIB_BEF_UINT32_TAG
 *       *phBlockOrValue -> pointer on the mem ref data (if it is a mem ref)
 *       *pnStartOffset -> Offset of the mem ref  (if it is a mem ref)
 *       *pnSize -> value the uint32 (if it is a uint32)
 *       *pnSharedMemFlags -> shared memory flag
 *		( SCX_SHMEM_TYPE_READ | SCX_SHMEM_TYPE_WRITE |
 *		SCX_SHMEM_TYPE_DIRECT | SCX_SHMEM_TYPE_DIRECT_FORCE)
 *
 *Return : next message slot
 *
 */
static u8 *static_befDecoderReadMemRefOrUint32(
							u8 *pDecoderArray,
							u8 *pDecoderArrayEnd,
							u32 *phBlockOrValue,
							u32 *pnStartOffset,
							u32 *pnSize,
							u32 *pnFlags,
							u32 *pnSharedMemFlags)
{
	u8 *pDecoderArrayWork = pDecoderArray;

	*pnSharedMemFlags = 0;

	pDecoderArrayWork = static_befDecoderReadMemRef(
							pDecoderArray,
							pDecoderArrayEnd,
							phBlockOrValue,
							pnStartOffset,
							pnSize,
							pnSharedMemFlags);

	if (pDecoderArrayWork != NULL) {
		*pnFlags = LIB_BEF_MEMREF_TAG;
		return pDecoderArrayWork;
	}

	*pnFlags = LIB_BEF_UINT32_TAG;

	pDecoderArrayWork = static_befDecoderReadUint32(
							pDecoderArray,
							pDecoderArrayEnd,
							pnSize);

	return pDecoderArrayWork;
}

/*-------------------------------------------------------------------------- */
/*
 *read memory reference or uint8_t[] array
 *Parse the pDecoderArray input string and setup the outputs according the
 *message type (mem ref or uint8 array)
 *
 *Input  : pDecoderArray -> current message slot pointer
 *       pDecoderArrayEnd -> pointer on the end of the message
 *
 *Output : *pnFlags -> LIB_BEF_MEMREF_TAG | LIB_BEF_UINT8_ARRAY_TAG
 *       *phBlockOrValue -> pointer on the mem ref data (if it is a mem ref)
 *       *pnStartOffset -> Offset of the mem ref  (if it is a mem ref)
 *       *pnSize -> size of the uint8 array (if it is a uint8 array)
 *       *pnSharedMemFlags -> shared memory flag
 *       ( SCX_SHMEM_TYPE_READ | SCX_SHMEM_TYPE_WRITE |
 *       SCX_SHMEM_TYPE_DIRECT | SCX_SHMEM_TYPE_DIRECT_FORCE)
 *
 *Return : next message slot
 *
 */
static u8 *static_befDecoderReadMemRefOrUint8Array(
							u8 *pDecoderArray,
							u8 *pDecoderArrayEnd,
							u32 *phBlockOrValue,
							u32 *pnStartOffset,
							u32 *pnSize,
							u32 *pnFlags,
							u32 *pnSharedMemFlags)
{
	u8 *pDecoderArrayWork = pDecoderArray;

	*pnSharedMemFlags = 0;

	pDecoderArrayWork = static_befDecoderReadMemRef(
							pDecoderArray,
							pDecoderArrayEnd,
							phBlockOrValue,
							pnStartOffset,
							pnSize,
							pnSharedMemFlags);

	if (pDecoderArrayWork != NULL) {
		/*it is a memory reference */
		*pnFlags = LIB_BEF_MEMREF_TAG;
		return pDecoderArrayWork;
	}

	*pnFlags = LIB_BEF_UINT8_ARRAY_TAG;

	pDecoderArrayWork = static_befDecoderReadUint8Array(
							pDecoderArray,
							pDecoderArrayEnd,
							phBlockOrValue,
							pnSize);

	return pDecoderArrayWork;
}

/*------------------------------------------------------------------------- */
/*
 *Write a value on 4 bytes
 */
static u8 *static_befEncoderWriteUntaggedUint32(u8 *pEncoderArray, u32 nValue)
{
	*pEncoderArray++ = (u8)(nValue);
	*pEncoderArray++ = (u8)((nValue >> 8)  & 0xFF);
	*pEncoderArray++ = (u8)((nValue >> 16) & 0xFF);
	*pEncoderArray++ = (u8)((nValue >> 24) & 0xFF);

	return pEncoderArray;
}

/*------------------------------------------------------------------------- */
/*
 *Write a uin32_t
 */
static u8 *static_befEncoderWriteUint32(
					u8 *pEncoderArray,
					u32 nEncoderArrayEnd,
					u32 nValue)
{
	/*Check that we can write the tag+nValue without overflowing */
	/*5 = tag + u32 element */
	if (!static_checkBuffer((u32) pEncoderArray, 5, nEncoderArrayEnd))
		return NULL;

	*pEncoderArray++ = LIB_BEF_UINT32_TAG;

	pEncoderArray = static_befEncoderWriteUntaggedUint32(pEncoderArray,
					nValue);

	return pEncoderArray;
}

/*------------------------------------------------------------------------- */
/*
 *static_GetDeviceContext(CRYPTOKI_UPDATE_SHORTCUT_CONTEXT *pCUSContext)
 *search in the all the device context (conns) if the CUS context specified by
 *pCUSContext exist.
 *If it is found, return the device context where the CUS context is.
 *If is is not found, return NULL.
 *
 */
SCXLNX_CONN_MONITOR *static_GetDeviceContext(
	CRYPTOKI_UPDATE_SHORTCUT_CONTEXT * pCUSContext)
{
	SCXLNX_CONN_MONITOR *pDeviceContext = NULL;
	CRYPTOKI_UPDATE_SHORTCUT_CONTEXT *pCUSContextFromList = NULL;

	spin_lock(&(g_SCXLNXDeviceMonitor.connsLock));
	list_for_each_entry(pDeviceContext, &(g_SCXLNXDeviceMonitor.conns),
		list) {
		spin_lock(&(pDeviceContext->shortcutListCriticalSectionLock));
		list_for_each_entry(pCUSContextFromList,
			&(pDeviceContext->ShortcutList), list) {
			if ((u32)pCUSContextFromList == (u32)pCUSContext) {
				spin_unlock(&(pDeviceContext->
					shortcutListCriticalSectionLock));
				spin_unlock(&(g_SCXLNXDeviceMonitor.
					connsLock));
				return pDeviceContext;
			}
		}
		spin_unlock(&(pDeviceContext->
			shortcutListCriticalSectionLock));
	}
	spin_unlock(&(g_SCXLNXDeviceMonitor.connsLock));

	/*pCUSContext does not exist */
	return NULL;
}

/*------------------------------------------------------------------------- */

#ifdef SMODULE_SMC_OMAP3XXX_POWER_MANAGEMENT
void scxPublicCryptoPowerManagementTimerCbk(unsigned long aPtr)
{
	SCXLNX_SM_COMM_MONITOR *pSMComm = (SCXLNX_SM_COMM_MONITOR *) aPtr;
	u32 nHWADisableClock = 0;

	dprintk(KERN_INFO "scxPublicCryptoPowerManagementTimerCbk try lock\n");

	/*try to take the lock on the timer semaphore */

	if (down_trylock(&(pSMComm->sPowerManagementTimerLock)) == 0) {
		/*if one of the HWA is used (by secure or public) the timer
		 *function cuts all the HWA clocks */

		/*since HWA lock semaphores are now split by HWA,
		 *we run the semaphore lock
		 *tries in sequence, it avoids unnecessarily locking HWA. */
		if (down_trylock(
			&g_SCXLNXDeviceMonitor.sAES1CriticalSection) == 0)
			nHWADisableClock |= RPC_AES_CODE;

		if (down_trylock(
			&g_SCXLNXDeviceMonitor.sD3D2CriticalSection) == 0)
			nHWADisableClock |= RPC_DES_CODE;

		if (down_trylock(
			&g_SCXLNXDeviceMonitor.sSHAM1CriticalSection) == 0)
			nHWADisableClock |= RPC_SHA_CODE;

		/*perform the clock cuts if the 3 HWAs were unlocked */
		if (nHWADisableClock ==
			(RPC_AES_CODE | RPC_DES_CODE | RPC_SHA_CODE)) {
			dprintk(KERN_INFO "CBK cut HWA clocks\n");
			scxPublicCryptoDisableClock(
				PUBLIC_CRYPTO_AES1_CLOCK_BIT);
			scxPublicCryptoDisableClock(
				PUBLIC_CRYPTO_DES2_CLOCK_BIT);
			scxPublicCryptoDisableClock(
				PUBLIC_CRYPTO_SHAM1_CLOCK_BIT);
		}

		/*unlock the HWA locked by the above down_trylock */
		if ((nHWADisableClock & RPC_AES_CODE) != 0)
			up(&g_SCXLNXDeviceMonitor.sAES1CriticalSection);
		if ((nHWADisableClock & RPC_DES_CODE) != 0)
			up(&g_SCXLNXDeviceMonitor.sD3D2CriticalSection);
		if ((nHWADisableClock & RPC_SHA_CODE) != 0)
			up(&g_SCXLNXDeviceMonitor.sSHAM1CriticalSection);

		/*release the lock on the timer */
		up(&pSMComm->sPowerManagementTimerLock);

	} else {
		dprintk("scxPublicCryptoPowerManagementTimerCbk"
				  " will wait one more time\n");
		mod_timer(&pSMComm->pPowerManagementTimer,
			jiffies+
			msecs_to_jiffies(pSMComm->nInactivityTimerExpireMs));
	}
}
#endif	/*SMODULE_SMC_OMAP3XXX_POWER_MANAGEMENT */

/*------------------------------------------------------------------------- */
/**
 *Initialize the public crypto DMA channels, global HWA semaphores and handles
 */
u32 scxPublicCryptoInit(void)
{
	u32 nError = PUBLIC_CRYPTO_OPERATION_SUCCESS;

	/*initialize the HWA semaphores */
	init_MUTEX(&g_SCXLNXDeviceMonitor.sAES1CriticalSection);
	init_MUTEX(&g_SCXLNXDeviceMonitor.sD3D2CriticalSection);
	init_MUTEX(&g_SCXLNXDeviceMonitor.sSHAM1CriticalSection);

	/*initialize the current key handle loaded in the AES1/D3D2 HWA */
	g_SCXLNXDeviceMonitor.hAES1SecureKeyContext = 0;
	g_SCXLNXDeviceMonitor.hD3D2SecureKeyContext = 0;
	g_SCXLNXDeviceMonitor.bSHAM1IsPublic = false;

	/*initialize the DMA semaphores */
	init_MUTEX(&g_SCXLNXDeviceMonitor.sm.sDMALock);

	/*allocate DMA buffer */
	g_SCXLNXDeviceMonitor.nDMABufferLength = PAGE_SIZE * 16;
	g_SCXLNXDeviceMonitor.pDMABuffer = dma_alloc_coherent(NULL,
		g_SCXLNXDeviceMonitor.nDMABufferLength,
		&(g_SCXLNXDeviceMonitor.pDMABufferPhys),
		GFP_KERNEL);
	if (g_SCXLNXDeviceMonitor.pDMABuffer == NULL) {
		printk(KERN_ERR
			"scxPublicCryptoInit: Out of memory for DMA buffer\n");
		nError = S_ERROR_OUT_OF_MEMORY;
	}

	/*initialize the power managment timer semaphores */
#ifdef SMODULE_SMC_OMAP3XXX_POWER_MANAGEMENT
	init_timer(&g_SCXLNXDeviceMonitor.sm.pPowerManagementTimer);
	init_MUTEX(&g_SCXLNXDeviceMonitor.sm.sPowerManagementTimerLock);
#endif				/*SMODULE_SMC_OMAP3XXX_POWER_MANAGEMENT */

	return nError;
}

/*------------------------------------------------------------------------- */
/*
 *Initialize the device context CUS fields (shortcut semaphore and public CUS
 *list)
 */
void scxPublicCryptoInitDeviceContext(SCXLNX_CONN_MONITOR *pDeviceContext)
{
	/*initialize the CUS list in the given device context */
	spin_lock_init(&(pDeviceContext->shortcutListCriticalSectionLock));
	INIT_LIST_HEAD(&(pDeviceContext->ShortcutList));
}

/*------------------------------------------------------------------------- */
/**
 *Terminate the public crypto (including DMA)
 */
void scxPublicCryptoTerminate()
{
	if (g_SCXLNXDeviceMonitor.pDMABuffer != NULL) {
		dma_free_coherent(NULL, g_SCXLNXDeviceMonitor.nDMABufferLength,
			g_SCXLNXDeviceMonitor.pDMABuffer,
			g_SCXLNXDeviceMonitor.pDMABufferPhys);
		g_SCXLNXDeviceMonitor.pDMABuffer = NULL;
	}
}

/*------------------------------------------------------------------------- */
/*
 *Perform a crypto update operation.
 *THIS FUNCTION IS CALLED FROM THE IOCTL
 */
void scxPublicCryptoUpdate(
	CRYPTOKI_UPDATE_SHORTCUT_CONTEXT *pCUSContext,
	CRYPTOKI_UPDATE_PARAMS *pCUSParams)
{
/* Might need to remove */
	dprintk(KERN_INFO
		"scxPublicCryptoUpdate(%x) : "\
		"HWAID=0x%x, In=%p, Out=%p, Len=%u, InputSh=%x, OutputSh=%x\n",
		(uint32_t) pCUSContext, pCUSContext->nHWAID,
		pCUSParams->pDataBuffer,
		pCUSParams->pResultBuffer, pCUSParams->nDataLength,
		pCUSParams->pInputShMem, pCUSParams->pOutputShMem);
/* might need to remove */

#ifdef CONFIG_HAS_WAKELOCK
	wake_lock(&g_smc_wake_lock);
#endif

	/*Enable the clock and Process Data */

	switch (pCUSContext->nHWAID) {
	case RPC_AES_CODE:
		scxPublicCryptoEnableClock(PUBLIC_CRYPTO_AES1_CLOCK_BIT);
		pCUSContext->sOperationState.aes.key_is_public = 0;
		pCUSContext->sOperationState.aes.CTRL = pCUSContext->nHWA_CTRL;
		PDrvCryptoUpdateAES(
				&pCUSContext->sOperationState.aes,
				pCUSParams->pDataBuffer,
				pCUSParams->pResultBuffer,
				pCUSParams->nDataLength / AES_BLOCK_SIZE);
		break;

	case RPC_DES_CODE:
		scxPublicCryptoEnableClock(PUBLIC_CRYPTO_DES2_CLOCK_BIT);
		PDrvCryptoUpdateDES(
				pCUSContext->nHWA_CTRL,
				&pCUSContext->sOperationState.des,
				pCUSParams->pDataBuffer,
				pCUSParams->pResultBuffer,
				pCUSParams->nDataLength / DES_BLOCK_SIZE);
		break;

	case RPC_SHA_CODE:
		scxPublicCryptoEnableClock(PUBLIC_CRYPTO_SHAM1_CLOCK_BIT);
		pCUSContext->sOperationState.sha.CTRL = pCUSContext->nHWA_CTRL;
		PDrvCryptoUpdateHash(
				&pCUSContext->sOperationState.sha,
				pCUSParams->pDataBuffer,
				pCUSParams->nDataLength);
		break;

	default:
		BUG_ON(1);
		break;
	}

#ifdef CONFIG_HAS_WAKELOCK
	wake_unlock(&g_smc_wake_lock);
#endif

#ifdef SMODULE_SMC_OMAP3XXX_POWER_MANAGEMENT
	dprintk(KERN_INFO "scxPublicCryptoUpdate: start timer\n");
	scxPublicCryptoStartInactivityTimer();
#endif

	dprintk(KERN_INFO "scxPublicCryptoUpdate: Done\n");
}

/*------------------------------------------------------------------------- */

/*
 *Check if the command must be intercepted by a CUS or not.
 *THIS FUNCTION IS CALLED FROM THE USER THREAD (ioctl).
 *
 *inputs: SCX_PUBLIC_CRYPTO_MONITOR *pPubCrypto : current device context
 *       SCX_COMMAND_INVOKE_CLIENT_COMMAND *pCommand : the command
 *       bool incrementnUseCount : specify if the nUseCount must be incremented
 *output:
 *CRYPTOKI_UPDATE_SHORTCUT_CONTEXT **ppPublicCUSContext : the public CUS
 *       if it is shortcuted
 *return: true or false
 *
 */
bool scxPublicCryptoIsShortcutedCommand(SCXLNX_CONN_MONITOR *pDeviceContext,
			SCX_COMMAND_INVOKE_CLIENT_COMMAND *pCommand,
			CRYPTOKI_UPDATE_SHORTCUT_CONTEXT **ppCUSContext,
			bool incrementnUseCount)
{
	CRYPTOKI_UPDATE_SHORTCUT_CONTEXT *pCUSContext = NULL;
	*ppCUSContext = NULL;

	dprintk(KERN_INFO "scxPublicCryptoIsShortcutedCommand: "\
		"pDeviceContext=0x%08x, pCommand=0x%08x, "\
		"CltSession=0x%08x, CmdID=0x%08x\n",
		(uint32_t) pDeviceContext, (uint32_t) pCommand,
		(uint32_t) pCommand->hClientSession,
		pCommand->nClientCommandIdentifier);

	/*take shortcutListCriticalSectionLock for the device context
	 *in which the message is sent <=> make sure that nobody is
	 *going to change data while processing */
	spin_lock(&(pDeviceContext->shortcutListCriticalSectionLock));

	/*lookup in the list of shortcuts attached to the device context for a
	 *shortcut context that contains the same hClientSession as the command
	 *and such that nCommandID is equal to nClientCommandIdentifier of the
	 *INVOKE_CLIENT_COMMAND message. If no such shortcut exists, take the
	 *standard path */
	list_for_each_entry(
		pCUSContext, &(pDeviceContext->ShortcutList), list) {
		dprintk(KERN_INFO
			"scxPublicCryptoIsShortcutedCommand: "\
			"nCommandID = 0x%08x hClientSession = 0x%08x\n",
			pCUSContext->nCommandID, pCUSContext->hClientSession);

		if ((pCUSContext->hClientSession == pCommand->hClientSession)
			&&
			(pCUSContext->nCommandID == pCommand->
			nClientCommandIdentifier)) {
			dprintk(KERN_INFO
				"scxPublicCryptoIsShortcutedCommand: "\
				"shortcut is identified\n");
			/*find a CUS : check if is suspended or not */
			if (pCUSContext->bSuspended) {
				/*
				 * bSuspended of the shortcut context is set to
				 * true, it means that the secure world has
				 * suspended the shortcut to perform an update
				 * on its own. In this case, take the standard
				 * path. This should happen very rarely because
				 * the client and the service should generally
				 * communicate to avoid such a collision
				 */
				dprintk(KERN_INFO "shortcut exists but "\
					"suspended\n");
				goto command_not_shortcutable;

			} else {
				dprintk(KERN_INFO "shortcut exists\n");
				/*For AES, DES/3DES and digest operations,
				 *provisionally determine if the accelerator
				 *is loaded with the appropriate key and
				 *accessible in public before deciding to enter
				 *the accelerator critical section.
				 *In most cases, if some other thread
				 *or the secure world is currently using the
				 *accelerator, the key won't change.
				 *So, if the key doesn't match now, it is
				 *likely not to match later on, so we'd better
				 *not try to enter the critical section in this
				 *case: */

				if (pCUSContext->nHWAID == RPC_AES_CODE &&
					pCUSContext->
					hKeyContext != g_SCXLNXDeviceMonitor.
					hAES1SecureKeyContext) {
					/*For AES operations, atomically read
					 *g_hAES1SSecureKeyContext and check if
					 *it is equal to hKeyContext. If not,
					 *take the standard path  <=> do not
					 *shortcut */
					dprintk(KERN_INFO
						"shortcut exists but AES key "\
						"not correct\nhKeyContext="\
						"0x%08x vs 0x%08x\n",
						pCUSContext->hKeyContext,
						g_SCXLNXDeviceMonitor.
						hAES1SecureKeyContext);
					goto command_not_shortcutable;

				} else if (pCUSContext->nHWAID == RPC_DES_CODE
						&& pCUSContext->hKeyContext !=
						g_SCXLNXDeviceMonitor.
						hD3D2SecureKeyContext) {
					/*
					 * For DES/3DES atomically read
					 * hD3D2SecureKeyContext and check if
					 * it is equal to hKeyContext. If not,
					 * take the standard path <=> do not
					 * shortcut
					 */
					dprintk(KERN_INFO
						"shortcut exists but DES key "\
						"not correct "\
						"hD3D2SecureKeyContext = "\
						"0x%08x hKeyContext0x%08x\n",
						(u32)g_SCXLNXDeviceMonitor.
						hD3D2SecureKeyContext,
						(u32)pCUSContext->hKeyContext);
					goto command_not_shortcutable;

				} else if (pCUSContext->nHWAID == RPC_SHA_CODE
						&& !g_SCXLNXDeviceMonitor.
						bSHAM1IsPublic) {
					/*
					 * For digest atomically read
					 * bSHAM1IsPublic and check if
					 * it is true. If not,
					 * take the standard path <=> do not
					 * shortcut
					 */
					dprintk(KERN_INFO
						"shortcut exists but SHAM1 "\
						"is not accessible in public");
					goto command_not_shortcutable;
				}
			}

			dprintk(KERN_INFO "shortcut exists and enabled\n");

			/*Shortcut has been found and context fits with
			 *thread => YES! the command can be shortcuted */

			/*
			 *set the pointer on the corresponding session
			 *(eq CUS context)
			 */
			*ppCUSContext = pCUSContext;

			/*
			 *increment nUseCount if required
			 */
			if (incrementnUseCount)
				pCUSContext->nUseCount++;

			/*
			 *release shortcutListCriticalSectionLock
			 */
			spin_unlock(&(pDeviceContext->
				shortcutListCriticalSectionLock));
			return true;
		}
	}

 command_not_shortcutable:
	/*
	 *release shortcutListCriticalSectionLock
	 */
	spin_unlock(&(pDeviceContext->shortcutListCriticalSectionLock));
	*ppCUSContext = NULL;
	return false;
}

/*------------------------------------------------------------------------- */

/*
 *Pre-process the client command (crypto update operation),
 *i.e., parse the command message (decode buffers, etc.)
 *THIS FUNCTION IS CALLED FROM THE USER THREAD (ioctl).
 *
 *For incorrect messages, an error is returned and the
 *message will be sent to secure
 */
bool scxPublicCryptoParseCommandMessage(SCXLNX_CONN_MONITOR *pConn,
				SCXLNX_SHMEM_DESC *pDeviceContextDesc,
				CRYPTOKI_UPDATE_SHORTCUT_CONTEXT *pCUSContext,
				SCX_COMMAND_INVOKE_CLIENT_COMMAND *pCommand,
				CRYPTOKI_UPDATE_PARAMS *pCUSParams)
{
	/*
	 *We need to decode the input BEF buffer first.
	 *The BEF buffer is in the master heap starting at the
	 *{nClientParameterStartOffset} offset.
	 */

	u32 nInputDataLength;
	u8 *pInputData;

	u32 nResultDataLength;
	u8 *pResultData = NULL;

	u8 *pCurrentCommandMessageElement;
	u8 *pEndOfCommandMessage;
	u8 *pS2CDataBufferEnd;

	u32 hBlock;
	u32 nStartOffset;
	u32 nFlags = 0;
	u32 nSharedMemFlags;

	SCXLNX_SHMEM_DESC *pSharedMemoryDescriptor = NULL;

	dprintk(KERN_INFO
		"scxPublicCryptoParseCommandMessage(%p) : Session=0x%x\n",
		pCUSContext, pCUSContext->hClientSession);

	/*Check the input message offset */
	if (pCommand->nClientParameterSize == 0) {
		dprintk(KERN_ERR "scxPublicCryptoParseCommandMessage(%p) : "\
			"Bad message offset\n",
			pCUSContext);
		return false;
	}

	/*check that client parameters are correct and point
	 *inside the client buffer */
	if (!static_checkBuffer(pCommand->nClientParameterStartOffset,
				pCommand->nClientParameterSize,
				pDeviceContextDesc->nBufferSize)) {
		dprintk(KERN_ERR "scxPublicCryptoParseCommandMessage(%p) : "\
			"Incorrect parameters from client.\n",
			pCUSContext);
		return false;
	}

	pCurrentCommandMessageElement = pDeviceContextDesc->pBuffer +
		pCommand->nClientParameterStartOffset;

	pEndOfCommandMessage = pCurrentCommandMessageElement +
		pCommand->nClientParameterSize;

	dprintk(KERN_INFO
		"scxPublicCryptoParseCommandMessage(%p) : 0x%08x, 0x%08x\n\n",
		pCUSContext, (u32)pCurrentCommandMessageElement,
		(u32)pEndOfCommandMessage);

	/*memref: Input buffer pointer and length */
	pCurrentCommandMessageElement =
		static_befDecoderReadMemRefOrUint8Array(
		  pCurrentCommandMessageElement, pEndOfCommandMessage, &hBlock,
		  &nStartOffset, &nInputDataLength, &nFlags, &nSharedMemFlags);

	dprintk(KERN_INFO "scxPublicCryptoParseCommandMessage(0x%08x): "\
		"hBlock=0x%08x; nStartOffset=0x%08x; "\
		"nInputDataLength=%u; nFlags=0x%08x\n",
		(u32)pCurrentCommandMessageElement, hBlock, nStartOffset,
		nInputDataLength, nFlags);

	if (pCurrentCommandMessageElement == NULL)
		return false;

	/*
	 *Setup the input memory pointers
	 */
	if (nFlags == LIB_BEF_UINT8_ARRAY_TAG) {
		/*Memory is an int8_t array provided by the message element and
			directly accessible from public */
		pInputData = (u8 *)hBlock;
	} else {
		/*nFlags == LIB_BEF_MEMREF_TAG*/
		/*Memory is a shared memory ref in a shared memory block from
		 *secure it must be retrieved from shared memory list of the
		 *current Device context (pConn) */
		pSharedMemoryDescriptor =
			PDrvCryptoGetSharedMemoryFromBlockHandle(
								pConn, hBlock);

		if ((pSharedMemoryDescriptor == NULL) ||
			(!static_checkBuffer(nStartOffset,
				nInputDataLength,
				pSharedMemoryDescriptor->nBufferSize))) {
			/*Shared memory not found or memory reference
			 *arguments (offset and size) not compatible with
			 *shared memory length */
			dprintk(KERN_ERR
				"scxPublicCryptoParseCommandMessage(%p): "\
				"Bad Input buffer command info\n",
				pCUSContext);
			return false;
		}

		/*Check  memory flags against the shared memory
		 *and that the shared memory is an input shared memory */
		if (((pSharedMemoryDescriptor->nFlags & nSharedMemFlags) == 0)
			 || ((pSharedMemoryDescriptor->
				nFlags & SCX_SHMEM_TYPE_READ) == 0)) {
			dprintk(KERN_ERR
				"scxPublicCryptoParseCommandMessage(%p): "\
				"Bad memory flags\n",
				pCUSContext);
			return false;
		}

		pInputData = pSharedMemoryDescriptor->pBuffer + nStartOffset;
		pCUSParams->pInputShMem = pSharedMemoryDescriptor;
		atomic_inc(&(pSharedMemoryDescriptor->nRefCnt));
	}

	pCUSParams->pS2CDataBuffer = NULL;

	/*Check client offset */
	if (!static_checkBuffer(pCommand->nClientAnswerStartOffset,
				pCommand->nClientAnswerSizeMax,
				pDeviceContextDesc->nBufferSize)) {
		dprintk(KERN_INFO
			"scxPublicCryptoParseCommandMessage: "\
			"Wrong parameters from client\n");
		return false;
	}

	if (pCUSContext->nHWAID == RPC_SHA_CODE) {
		/*No more data should be encoded for digest operations */
		if (pCurrentCommandMessageElement != pEndOfCommandMessage) {
			dprintk(KERN_INFO
				"scxPublicCryptoParseCommandMessage: "\
				"Digest operation with output buffer: Error\n");
			return false;
		}
	} else {
		/*
		 * Encrypt/decrypt operations: Message is fully processed:
		 * means pOutput=NULL_PTR
		 * Forward the processing to the secure side...
		 */
		if (pCurrentCommandMessageElement == pEndOfCommandMessage) {
			dprintk(KERN_INFO
				"scxPublicCryptoParseCommandMessage: "\
				"Encrypt/Decrypt operation with no output "\
				"buffer\n");
			return false;
		}
	}
	/*
	 * Encrypt or Decrypt only:
	 * The message is not fully processed, there is a result buffer info.
	 */
	if (pCurrentCommandMessageElement < pEndOfCommandMessage) {
		/*
		 *There are some more data to decode:
		 *Result buffer information.
		 *Go on with the decoding process...
		 */
		dprintk(KERN_INFO "scxPublicCryptoParseCommandMessage: "\
			"Parsing result buffer info\n");

		/*set the output message command pointer */
		pCUSParams->pS2CDataBuffer = pDeviceContextDesc->pBuffer +
			pCommand->nClientAnswerStartOffset;

		pCurrentCommandMessageElement =
				static_befDecoderReadMemRefOrUint32(
			pCurrentCommandMessageElement, pEndOfCommandMessage,
			&hBlock, &nStartOffset, &nResultDataLength, &nFlags,
			&nSharedMemFlags);

		dprintk(KERN_INFO
			"scxPublicCryptoParseCommandMessage(0x%08x): "\
			"hBlock=0x%08x; nStartOffset=0x%08x; "\
			"nResultDataLength=0x%08x; nFlags=0x%08x; "\
			"nSharedMemFlags=0x%08x\n",
			(u32)pCurrentCommandMessageElement, hBlock,
			nStartOffset, nResultDataLength, nFlags,
			nSharedMemFlags);

		if (pCurrentCommandMessageElement == NULL) {
			dprintk(KERN_ERR
				"scxPublicCryptoParseCommandMessage(%p): "\
				"Bad Result buffer command format\n",
				pCUSContext);
			return false;
		}

		if (nResultDataLength < nInputDataLength) {
			/*Short buffer*/
			dprintk(KERN_ERR
				"scxPublicCryptoParseCommandMessage(%p): "\
				"Short buffer: nResultDataLength=%d < "\
				"nInputDataLength=%d\n",
				pCUSContext, nResultDataLength,
				nInputDataLength);
			return false;
		}

		if (pCurrentCommandMessageElement != pEndOfCommandMessage) {
			dprintk(KERN_INFO
				"scxPublicCryptoParseCommandMessage: Encrypt/"\
				"Decrypt operation with data after the output "\
				"buffer: Error\n");
			return false;
		}

		/*
		 *Setup the ouput memory pointers
		 */
		if (nFlags == LIB_BEF_UINT32_TAG) {
			/*Memory is the int8_t array output buffer directly
			 *accessible from public */
			pS2CDataBufferEnd = pCUSParams->pS2CDataBuffer;

			/*We should already start encoding the tag and the
			 *length of the response buffer. The actual value will
			 * be encoded after the processing of data.
			 */
			if (!static_checkBuffer((u32)pS2CDataBufferEnd, 5,
				pCommand->nClientAnswerSizeMax)) {
				dprintk(KERN_ERR
				"scxPublicCryptoParseCommandMessage(%p): Bad "\
				"result buffer. Can not encode tag and length."\
				"\n",
					pCUSContext);
				return false;
			}
			*pS2CDataBufferEnd++ = LIB_BEF_UINT8_ARRAY_TAG;
			pS2CDataBufferEnd =
			static_befEncoderWriteUntaggedUint32(pS2CDataBufferEnd,
				nResultDataLength); /*pS2CDataBufferEnd += 5;*/
			pResultData = pS2CDataBufferEnd;

		} else {
			/*nFlags == LIB_BEF_MEMREF_TAG*/
			/*
			 * Memory is a shared memory ref in a shared memory
			 * block from secure it must be retrieved from shared
			 * memory list of the current Device context (pConn) */
			pSharedMemoryDescriptor =
			PDrvCryptoGetSharedMemoryFromBlockHandle(pConn,
								hBlock);

			if ((pSharedMemoryDescriptor == NULL) ||
				 (!static_checkBuffer(nStartOffset,
						nResultDataLength,
						pSharedMemoryDescriptor->
						nBufferSize))) {
				/*Shared memory not found or memory reference
				 *arguments (offset and size) not compatible
				 *with shared memory length or shared mem flags
				 *not compatible with the acccess */
				dprintk(KERN_ERR
				"scxPublicCryptoParseCommandMessage(%p) : "\
				"Bad Result buffer command info\n",
					pCUSContext);
				return false;
			}

			/*Check  memory flags against the shared memory and
			 *that the shared memory is an output shared memory */
			if (((pSharedMemoryDescriptor->
				nFlags & nSharedMemFlags) == 0)
				|| ((pSharedMemoryDescriptor->
				nFlags & SCX_SHMEM_TYPE_WRITE) == 0)) {
				dprintk(KERN_ERR
				"scxPublicCryptoParseCommandMessage(%p) : "\
				"Bad memory flags\n",
					pCUSContext);
				return false;
			}
			pResultData = pSharedMemoryDescriptor->
				pBuffer + nStartOffset;
			pCUSParams->pOutputShMem = pSharedMemoryDescriptor;
			atomic_inc(&(pSharedMemoryDescriptor->nRefCnt));
		}
	}

	/*Check if input length is compatible with the algorithm of the
	 *shortcut*/
	switch (pCUSContext->nHWAID) {
	case RPC_AES_CODE:
		/*Must be multiple of the AES block size */
		if ((nInputDataLength % AES_BLOCK_SIZE) != 0) {
			dprintk(KERN_ERR
				"scxPublicCryptoParseCommandMessage(%p): "\
				"Input Data Length invalid [%d] for AES\n",
				pCUSContext, nInputDataLength);
			return false;
		}
		break;
	case RPC_DES_CODE:
		/*Must be multiple of the DES block size */
		if ((nInputDataLength % DES_BLOCK_SIZE) != 0) {
			dprintk(KERN_ERR
				"scxPublicCryptoParseCommandMessage(%p): "\
				"Input Data Length invalid [%d] for DES\n",
				pCUSContext, nInputDataLength);
			return false;
		}
		break;
	default:
		/*SHA operation: no constraint on data length */
		break;
	}

	pCUSParams->pDataBuffer = pInputData;
	pCUSParams->nDataLength = nInputDataLength;
	pCUSParams->pResultBuffer = pResultData;

	/*Used for checking buffer overflow when writing the BEF answer*/
	pCUSParams->nS2CDataBufferMaxLength = pCommand->nClientAnswerSizeMax;

	return true;
}

/*------------------------------------------------------------------------- */

/*
 *Post-process the client command (crypto update operation),
 *i.e. copy the result into the user output buffer and release the resources.
 *THIS FUNCTION IS CALLED FROM THE USER THREAD (ioctl).
 */
void scxPublicCryptoWriteAnswerMessage(
				CRYPTOKI_UPDATE_SHORTCUT_CONTEXT *pCUSContext,
				CRYPTOKI_UPDATE_PARAMS *pCUSParams,
				SCX_ANSWER_INVOKE_CLIENT_COMMAND *pAnswer)
{
	u32 nS2CBufferSize = 0;

	dprintk(KERN_INFO
		"scxPublicCryptoWriteAnswerMessage(%p) : Session=0x%x\n",
		pCUSContext, pCUSContext->hClientSession);

	/*
	 *The message has been processed in public...
	 */
	if (pCUSParams->pS2CDataBuffer != NULL) {

		u8 *pS2CDataBufferEnd = pCUSParams->pS2CDataBuffer;

		if (pCUSParams->pOutputShMem) {
			dprintk(KERN_INFO
				"scxPublicCryptoWriteAnswerMessage: "\
				"MemRef: Length = %d\n",
				pCUSParams->nDataLength);
			/*
			 *u32: Result buffer length in answer message
			 */
			pS2CDataBufferEnd = static_befEncoderWriteUint32(
				pS2CDataBufferEnd,
				pCUSParams->nS2CDataBufferMaxLength,
				pCUSParams->nDataLength);

		} else {
			dprintk(KERN_INFO
				"scxPublicCryptoWriteAnswerMessage: "\
				"U8 Array: Length = %d\n",
				pCUSParams->nDataLength);
			/*
			 *The tag and the length of the output data buffer is
			 *already in the answer buffer,as well as the output
			 *data. We only need to update the pointer;
			 */
			/*5 = TAG + LENGTH */
			pS2CDataBufferEnd += pCUSParams->nDataLength + 5;
		}

		/*Encoded output message size */
		nS2CBufferSize = pS2CDataBufferEnd - pCUSParams->
							pS2CDataBuffer;
	}

	/*
	 *Generate the answer
	 */
	pAnswer->nClientAnswerSize = nS2CBufferSize;
	pAnswer->nFrameworkStatus = S_SUCCESS;
	pAnswer->nServiceError = CKR_OK;
}

/*------------------------------------------------------------------------- */

void scxPublicCryptoWaitForReadyBitInfinitely(VU32 *pRegister, u32 vBit)
{
	while (!(INREG32(pRegister) & vBit)) {
	}
}

/*------------------------------------------------------------------------- */

u32 scxPublicCryptoWaitForReadyBit(VU32 *pRegister, u32 vBit)
{
	u32 timeoutCounter = PUBLIC_CRYPTO_TIMEOUT_CONST;

	while ((!(INREG32(pRegister) & vBit)) && ((--timeoutCounter) != 0)) {
	}

	if (timeoutCounter == 0)
		return PUBLIC_CRYPTO_ERR_TIMEOUT;

	return PUBLIC_CRYPTO_OPERATION_SUCCESS;
}

/*------------------------------------------------------------------------- */

void scxPublicCryptoDisableClock(uint32_t vClockBit)
{
	u32 *pClockReg;
	u32 *pAutoIdleReg;
	unsigned long nITFlags;

	dprintk(KERN_INFO "scxPublicCryptoDisableClock: vClockBit=0x%08X\n",
		vClockBit);

	/* Ensure none concurrent access when changing clock registers */
	local_irq_save(nITFlags);

	/*if vClockBit <= PUBLIC_CRYPTO_AES1_CLOCK_BIT (bit #3), then
	 *AES#1/SHA1#1/DES#1 HWAs are concerned ICLK2/AUTOIDLE2. Else (bit #26
	 *and greater) AES#2/SHA1#2/DES#2 are targeted ICLK1/AUTOIDLE1 */
	if (vClockBit <= PUBLIC_CRYPTO_AES1_CLOCK_BIT) {
		pClockReg = (u32 *)OMAP2_L4_IO_ADDRESS(PUBLIC_CRYPTO_HW_CLOCK_ADDR);
		pAutoIdleReg =
			(u32 *)OMAP2_L4_IO_ADDRESS(PUBLIC_CRYPTO_HW_AUTOIDLE_ADDR);
	} else {
	  pClockReg = (u32 *)OMAP2_L4_IO_ADDRESS(PUBLIC_CRYPTO_HW_CLOCK1_ADDR);
	  pAutoIdleReg = (u32 *)OMAP2_L4_IO_ADDRESS(PUBLIC_CRYPTO_HW_AUTOIDLE1_ADDR);
	}

	*(pClockReg) &= ~vClockBit;
	*(pAutoIdleReg) |= vClockBit;

	local_irq_restore(nITFlags);
}

/*------------------------------------------------------------------------- */

void scxPublicCryptoEnableClock(uint32_t vClockBit)
{
	u32 *pClockReg;
	u32 *pAutoIdleReg;
	unsigned long nITFlags;

#ifdef SMODULE_SMC_OMAP3XXX_POWER_MANAGEMENT
	SCXLNX_SM_COMM_MONITOR *pSMComm = &(g_SCXLNXDeviceMonitor.sm);
	/*take the lock on the timer */
	down(&pSMComm->sPowerManagementTimerLock);
	/*stop the timer if already running */
	if (timer_pending(&pSMComm->pPowerManagementTimer))
		del_timer(&pSMComm->pPowerManagementTimer);

	/*release the lock on the timer */
	up(&pSMComm->sPowerManagementTimerLock);
#endif				/*SMODULE_SMC_OMAP3XXX_POWER_MANAGEMENT */

	dprintk(KERN_INFO "scxPublicCryptoEnableClock: vClockBit=0x%08X\n",
		vClockBit);

	/* Ensure none concurrent access when changing clock registers */
	local_irq_save(nITFlags);

	/*if vClockBit <= PUBLIC_CRYPTO_AES1_CLOCK_BIT (bit #3), then
	 *AES#1/SHA1#1/DES#1 HWAs are concerned ICLK2/AUTOIDLE2. Else (bit #26
	 *and greater) AES#2/SHA1#2/DES#2 are targeted ICLK1/AUTOIDLE1 */
	if (vClockBit <= PUBLIC_CRYPTO_AES1_CLOCK_BIT) {
		pClockReg = (u32 *)OMAP2_L4_IO_ADDRESS(PUBLIC_CRYPTO_HW_CLOCK_ADDR);
		pAutoIdleReg = (u32 *)OMAP2_L4_IO_ADDRESS(
					PUBLIC_CRYPTO_HW_AUTOIDLE_ADDR);
	} else {
		pClockReg = (u32 *)OMAP2_L4_IO_ADDRESS(PUBLIC_CRYPTO_HW_CLOCK1_ADDR);
		pAutoIdleReg = (u32 *)OMAP2_L4_IO_ADDRESS(
					PUBLIC_CRYPTO_HW_AUTOIDLE1_ADDR);
	}

	*(pClockReg) |= vClockBit;
	*(pAutoIdleReg) &= ~vClockBit;

	local_irq_restore(nITFlags);
}

/*------------------------------------------------------------------------- */

#ifdef SMODULE_SMC_OMAP3XXX_POWER_MANAGEMENT
void scxPublicCryptoStartInactivityTimer(void)
{
	SCXLNX_SM_COMM_MONITOR *pSMComm = &(g_SCXLNXDeviceMonitor.sm);

	dprintk(KERN_INFO
		"scxPublicCryptoStartInactivityTimer devmon=0x%08x "\
		"pSMComm=0x%08x\n",
		(u32)&g_SCXLNXDeviceMonitor, (u32)pSMComm);

	/*take the lock on the timer */
	down(&pSMComm->sPowerManagementTimerLock);

	/*stop the timer if already running */
	if (timer_pending(&pSMComm->pPowerManagementTimer))
		del_timer(&pSMComm->pPowerManagementTimer);

	/*(re)configure the timer */
	pSMComm->pPowerManagementTimer.expires =
		 jiffies + msecs_to_jiffies(pSMComm->nInactivityTimerExpireMs);
	pSMComm->pPowerManagementTimer.data = (unsigned long)pSMComm;
	pSMComm->pPowerManagementTimer.function =
		 scxPublicCryptoPowerManagementTimerCbk;
	add_timer(&pSMComm->pPowerManagementTimer);

	/*release the timer */
	up(&pSMComm->sPowerManagementTimerLock);
}
#endif				/*SMODULE_SMC_OMAP3XXX_POWER_MANAGEMENT */

/*------------------------------------------------------------------------- */
/*                     CUS RPCs                                              */
/*------------------------------------------------------------------------- */
/*
This RPC is used by the secure world to install a new shortcut.
Optionally, for AES or DES/3DES operations, it can also lock the accelerator
so that the secure world can install a new key in it.
*/
u32 scxPublicCryptoInstallShortcutLockAccelerator(u32 nRPCCommand,
							void *pL0SharedBuffer)
{
	CRYPTOKI_UPDATE_SHORTCUT_CONTEXT *pCUSContext = NULL;
	SCXLNX_CONN_MONITOR *pDeviceContext = NULL;

	/*reference the input/ouput data */
	RPC_INSTALL_SHORTCUT_LOCK_ACCELERATOR_OUT *pInstallShortcutOut =
		(RPC_INSTALL_SHORTCUT_LOCK_ACCELERATOR_OUT *)pL0SharedBuffer;
	RPC_INSTALL_SHORTCUT_LOCK_ACCELERATOR_IN *pInstallShortcutIn =
		(RPC_INSTALL_SHORTCUT_LOCK_ACCELERATOR_IN *)pL0SharedBuffer;

	dprintk(KERN_INFO
		"scxPublicCryptoInstallShortcutLockAccelerator :"
		"nRPCCommand=0x%08x; nHWAID=0x%08x\n",
		nRPCCommand, pInstallShortcutIn->nHWAID);

/*preventive check1: check if pInstallShortcutIn->hDeviceContext exist */
	/*Look up in the driver tables for the device context matching
		hDeviceContext */
	pDeviceContext = PDrvCryptoGetDeviceContextFromHandle(
					pInstallShortcutIn->hDeviceContext);

	if (pDeviceContext == NULL) {
		dprintk(KERN_INFO
			"scxPublicCryptoInstallShortcutLockAccelerator: "\
			"DeviceContext 0x%08x does not exist, "\
			"cannot create Shortcut\n",
			pInstallShortcutIn->hDeviceContext);
		pInstallShortcutOut->nError = S_ERROR_BAD_PARAMETERS;
		return S_SUCCESS;
	}

	/*Allocate a shortcut context. If the allocation fails,
		return S_ERROR_OUT_OF_MEMORY error code  */
	pCUSContext = (CRYPTOKI_UPDATE_SHORTCUT_CONTEXT *)internal_kmalloc(
					sizeof(*pCUSContext), GFP_KERNEL);
	if (pCUSContext == NULL) {
		dprintk(KERN_ERR
			"scxPublicCryptoInstallShortcutLockAccelerator: "\
			"Out of memory for public session\n");
		pInstallShortcutOut->nError = S_ERROR_OUT_OF_MEMORY;
		return S_SUCCESS;
	}

	memset(pCUSContext, 0, sizeof(*pCUSContext));

	/*setup the shortcut */
	pCUSContext->nMagicNumber = CRYPTOKI_UPDATE_SHORTCUT_CONTEXT_MAGIC;
	pCUSContext->hClientSession = pInstallShortcutIn->hClientSession;
	pCUSContext->nCommandID = pInstallShortcutIn->nCommandID;
	pCUSContext->nHWAID = pInstallShortcutIn->nHWAID;
	pCUSContext->nHWA_CTRL = pInstallShortcutIn->nHWA_CTRL;
	pCUSContext->hKeyContext = pInstallShortcutIn->hKeyContext;
	pCUSContext->nUseCount = 0;
	pCUSContext->bSuspended = false;

	memcpy(&pCUSContext->sOperationState,
			 &pInstallShortcutIn->sOperationState,
			 sizeof(PUBLIC_CRYPTO_OPERATION_STATE));

	/*lock the shortcutListCriticalSectionLock for this device context */
	spin_lock(&pDeviceContext->shortcutListCriticalSectionLock);

	/*Insert the shortcut in the list of shortcuts in the device context */
	list_add(&(pCUSContext->list), &(pDeviceContext->ShortcutList));

	/*release shortcutListCriticalSectionLock */
	spin_unlock(&pDeviceContext->shortcutListCriticalSectionLock);

	/*fill the output structure */
	pInstallShortcutOut->nShortcutID = (u32) pCUSContext;
	pInstallShortcutOut->nError = S_SUCCESS;

	/*If the L bit is true, then:
	 * Enter the accelerator critical section. If an update is currently in
	 * progress on the accelerator (using g_hXXXKeyContext key), this will
	 * wait until the update has completed. This is call when secure wants
	 * to install a key in HWA, once it is done secure world will release
	 * the lock.  For SHA (activate shortcut is always called without LOCK
	 * fag):do nothing
	 */
	if ((nRPCCommand & RPC_INSTALL_SHORTCUT_LOCK_ACCELERATOR_LOCK) != 0) {
		/*Lock the HWA */
		PDrvCryptoLockUnlockHWA(pCUSContext->nHWAID, LOCK_HWA);
	}

	dprintk(KERN_INFO
		"scxPublicCryptoInstallShortcutLockAccelerator: Done\n");

	return S_SUCCESS;
}

/*------------------------------------------------------------------------- */

/*
This RPC is used to perform one or several of the following operations
- Lock one or several accelerators for the exclusive use by the secure world,
  either because it is going to be switched to secure or because a new key is
  going to be loaded in the accelerator
- Suspend a shortcut, i.e., make it temporarily unavailable to the public
  world. This is used when a secure update is going to be performed on the
  operation. The answer to the RPC then contains the operation state necessary
  for the secure world to do the update.
-  Uninstall the shortcut
*/
u32 scxPublicCryptoLockAcceleratorsSuspendShortcut(u32 nRPCCommand,
	void *pL0SharedBuffer)
{
	u32 nTargetShortcut;
	CRYPTOKI_UPDATE_SHORTCUT_CONTEXT *pCUSContext = NULL;
	SCXLNX_CONN_MONITOR *pDeviceContext = NULL;

	/*reference the input/ouput data */
	RPC_LOCK_ACCELERATORS_SUSPEND_SHORTCUT_OUT *pSuspendShortcutOut =
		(RPC_LOCK_ACCELERATORS_SUSPEND_SHORTCUT_OUT *)pL0SharedBuffer;
	RPC_LOCK_ACCELERATORS_SUSPEND_SHORTCUT_IN *pSuspendShortcutIn =
		(RPC_LOCK_ACCELERATORS_SUSPEND_SHORTCUT_IN *)pL0SharedBuffer;

	dprintk(KERN_INFO
		"scxPublicCryptoLockAcceleratorsSuspendShortcut: "\
		"pSuspendShortcutIn=0x%08x; nShortcutID=0x%08x\n",
		pSuspendShortcutIn->nShortcutID, (u32)pSuspendShortcutIn);

	nTargetShortcut = pSuspendShortcutIn->nShortcutID;

	/*lock HWAs */
	PDrvCryptoLockUnlockHWAs(nRPCCommand, LOCK_HWA);

	/*if pSuspendShortcutIn->nShortcutID != 0 and  if nRPCCommand.S != 0,
		then, suspend shortcut */
	if ((nTargetShortcut != 0) && ((nRPCCommand &
		RPC_LOCK_ACCELERATORS_SUSPEND_SHORTCUT_SUSPEND) != 0)) {
		/*reference the CUSContext */
		pCUSContext =
		  (CRYPTOKI_UPDATE_SHORTCUT_CONTEXT *)pSuspendShortcutIn->
								nShortcutID;

		/*preventive check1: return if shortcut does not exist */
		pDeviceContext = static_GetDeviceContext(pCUSContext);
		if (pDeviceContext == NULL) {
			dprintk(KERN_INFO
			"scxPublicCryptoLockAcceleratorsSuspendShortcut: "\
			"nShortcutID=0x%08x does not exist, cannot suspend "\
			"Shortcut\n",
				pSuspendShortcutIn->nShortcutID);
			return S_ERROR_BAD_PARAMETERS;
		}

loop_on_suspend:
		/*lock shortcutListCriticalSectionLock associated with the
		 *device context */
		spin_lock(&pDeviceContext->shortcutListCriticalSectionLock);

		/*Suspend shortcut */
		pCUSContext->bSuspended = true;

		if (pCUSContext->nUseCount != 0) {
			/*release shortcutListCriticalSectionLock */
			spin_unlock(&pDeviceContext->
				shortcutListCriticalSectionLock);
			schedule();
			goto loop_on_suspend;
		}

		/*Copy the operation state data stored in CUS Context into the
		 *answer to the RPC output assuming that HWA register has been
		 *saved at update time */
		memcpy(&pSuspendShortcutOut->sOperationState,
				 &pCUSContext->sOperationState,
				 sizeof(PUBLIC_CRYPTO_OPERATION_STATE));

		/*Uninstall shortcut if requiered  */
		if ((nRPCCommand &
		RPC_LOCK_ACCELERATORS_SUSPEND_SHORTCUT_UNINSTALL) != 0) {
			dprintk(KERN_INFO
			"scxPublicCryptoLockAcceleratorsSuspendShortcut: "\
			"Uninstall 0x%08x\n",
				nTargetShortcut);
			list_del(&(pCUSContext->list));
			/*list_del only remove the item in the list, the
			 *memory must be free afterward */
			/*release the lock before calling internal_kfree */
			spin_unlock(&pDeviceContext->
				shortcutListCriticalSectionLock);
			if (pCUSContext != NULL)
				internal_kfree(pCUSContext);
			return S_SUCCESS;
		}

		/*release shortcutListCriticalSectionLock */
		spin_unlock(&pDeviceContext->shortcutListCriticalSectionLock);
	}

	return S_SUCCESS;
}

/*------------------------------------------------------------------------- */

/*
This RPC is used to perform one or several of the following operations:
-	Resume a shortcut previously suspended
-	Inform the public driver of the new keys installed in the D3D2 and AES2
	accelerators
-	Unlock some of the accelerators
*/
u32 scxPublicCryptoResumeShortcutUnlockAccelerators(u32 nRPCCommand,
							void *pL0SharedBuffer)
{
	SCXLNX_CONN_MONITOR *pDeviceContext = NULL;
	CRYPTOKI_UPDATE_SHORTCUT_CONTEXT *pCUSContext = NULL;

	/*reference the input data */
	RPC_RESUME_SHORTCUT_UNLOCK_ACCELERATORS_IN *pResumeShortcutIn =
		(RPC_RESUME_SHORTCUT_UNLOCK_ACCELERATORS_IN *)pL0SharedBuffer;

	dprintk(KERN_INFO
		"scxPublicCryptoResumeShortcutUnlockAccelerators\n" \
		"nRPCCommand=0x%08x\nnShortcutID=0x%08x\n",
		nRPCCommand, pResumeShortcutIn->nShortcutID);

	/*if nShortcutID not 0 resume the shortcut and unlock HWA
		else only unlock HWA */
	if (pResumeShortcutIn->nShortcutID != 0) {
		/*reference the CUSContext */
		pCUSContext =
		(CRYPTOKI_UPDATE_SHORTCUT_CONTEXT *)pResumeShortcutIn->
								nShortcutID;

		/*preventive check1: return if shortcut does not exist
		 *else, points to the public crypto monitor (inside the device
		 *context) */
		pDeviceContext = static_GetDeviceContext(pCUSContext);
		if (pDeviceContext == NULL) {
			dprintk(KERN_INFO
			"scxPublicCryptoResumeShortcutUnlockAccelerators(...):"\
			"nShortcutID 0x%08x does not exist, cannot suspend "\
			"Shortcut\n",
				pResumeShortcutIn->nShortcutID);
			return S_ERROR_BAD_PARAMETERS;
		}

		/*if S set and shortcut not yet suspended */
		if ((pCUSContext->bSuspended) &&
			 ((nRPCCommand &
			RPC_RESUME_SHORTCUT_UNLOCK_ACCELERATORS_RESUME) != 0)){
			/*Write sOperationStateData in the shortcut context */
			memcpy(&pCUSContext->sOperationState,
					&pResumeShortcutIn->sOperationState,
					sizeof(PUBLIC_CRYPTO_OPERATION_STATE));
			/*resume the shortcut */
			pCUSContext->bSuspended = false;
		}
	}

	/*H is never set by the PA: Atomically set bSHAM1IsPublic to true */
	g_SCXLNXDeviceMonitor.bSHAM1IsPublic = true;

	/*If A is set: Atomically set hAES1SecureKeyContext to
	 *hAES1KeyContext */
	if ((nRPCCommand &
		RPC_RESUME_SHORTCUT_UNLOCK_ACCELERATORS_UNLOCK_AES) != 0) {
		g_SCXLNXDeviceMonitor.hAES1SecureKeyContext =
			pResumeShortcutIn->hAES1KeyContext;
	}

	/*If D is set:
	 * Atomically set hD3D2SecureKeyContext to hD3D2KeyContext */
	if ((nRPCCommand &
		RPC_RESUME_SHORTCUT_UNLOCK_ACCELERATORS_UNLOCK_DES) != 0) {
		g_SCXLNXDeviceMonitor.hD3D2SecureKeyContext =
					pResumeShortcutIn->hD3D2KeyContext;
	}

	/*Unlock HWAs according nRPCCommand */
	PDrvCryptoLockUnlockHWAs(nRPCCommand, UNLOCK_HWA);

	return S_SUCCESS;
}

/*------------------------------------------------------------------------- */

/*
  This RPC is used to notify the public driver that the key in the AES2, D3D2
  accelerators has been cleared. This happens only when the key is no longer
  referenced by any shortcuts. So, it is guaranteed that no-one has entered the
  accelerators critical section and there is no need to enter it to
  implement this RPC.
*/
u32 scxPublicCryptoClearGlobalKeyContext(u32 nRPCCommand,
						void *pL0SharedBuffer)
{
	/*
	 *If A is set: Atomically set hAES1SecureKeyContext to 0
	 */
	if ((nRPCCommand &
		RPC_RESUME_SHORTCUT_UNLOCK_ACCELERATORS_UNLOCK_AES) != 0) {
		g_SCXLNXDeviceMonitor.hAES1SecureKeyContext = 0;
	}

	/*
	 *If D is set: Atomically set hD3D2SecureKeyContext to 0
	 */
	if ((nRPCCommand &
		RPC_RESUME_SHORTCUT_UNLOCK_ACCELERATORS_UNLOCK_DES) != 0) {
		g_SCXLNXDeviceMonitor.hD3D2SecureKeyContext = 0;
	}

	return S_SUCCESS;
}

/*------------------------------------------------------------------------- */
/*
 *get the shared memory from memory block handle comming from secure
 *return NULL if it does not exit.
 *
 */
SCXLNX_SHMEM_DESC *PDrvCryptoGetSharedMemoryFromBlockHandle(SCXLNX_CONN_MONITOR
							 *pConn, u32 hBlock)
{
	SCXLNX_SHMEM_DESC *pSharedMemoryDescriptor = NULL;

	/*lock sharedMemoriesMutex */
	down(&(pConn->sSharedMemoryMonitor.sharedMemoriesMutex));

	/*scan the sUsedSharedMemoryList of the pConn->sSharedMemoryMonitor */
	list_for_each_entry(pSharedMemoryDescriptor,
			&(pConn->sSharedMemoryMonitor.sUsedSharedMemoryList),
			list) {
	  if ((u32) pSharedMemoryDescriptor->hIdentifier == (u32) hBlock) {
			/*unlock sharedMemoriesMutex */
			up(&(pConn->sSharedMemoryMonitor.sharedMemoriesMutex));
			return pSharedMemoryDescriptor;
		}
	}

	/*hBlock does not exist */
	/*unlock sharedMemoriesMutex */
	up(&(pConn->sSharedMemoryMonitor.sharedMemoriesMutex));
	return NULL;
}

/*------------------------------------------------------------------------- */
/*
 *get the device context from device context handle comming from secure
 *return NULL if it does not exit.
 *
 */
SCXLNX_CONN_MONITOR *PDrvCryptoGetDeviceContextFromHandle(u32 hDeviceContext)
{
	SCXLNX_CONN_MONITOR *pDeviceContext = NULL;

	/*lock the access to the conns list */
	spin_lock(&(g_SCXLNXDeviceMonitor.connsLock));

	list_for_each_entry(pDeviceContext, &(g_SCXLNXDeviceMonitor.conns),
		list) {
		dprintk("PDrvCryptoGetDeviceContextFromHandle: "\
			"DeviceContext 0x%08x vs 0x%08x\n",
			(u32)pDeviceContext->
			sSharedMemoryMonitor.pDeviceContextDesc->hIdentifier,
			(u32)hDeviceContext);

		if ((u32)pDeviceContext->
			sSharedMemoryMonitor.pDeviceContextDesc->hIdentifier ==
			hDeviceContext) {
			spin_unlock(&(g_SCXLNXDeviceMonitor.connsLock));
			return pDeviceContext;
		}
	}

	/*if here, hDeviceContext does not exist */
	spin_unlock(&(g_SCXLNXDeviceMonitor.connsLock));
	return NULL;
}

/*------------------------------------------------------------------------- */

/*------------------------------------------------------------------------- */
/*
 *HWAs public lock or unlock HWA's specified in the HWA H/A/D fields of RPC
 *command nRPCCommand
 */
void PDrvCryptoLockUnlockHWAs(u32 nRPCCommand, bool bDoLock)
{
	dprintk(KERN_INFO
		"PDrvCryptoLockUnlockHWAs: nRPCCommand=0x%08x bDoLock=%d\n",
		nRPCCommand, bDoLock);

	/*perform the locks */
	if (nRPCCommand & RPC_AES_CODE)
		PDrvCryptoLockUnlockHWA(RPC_AES_CODE, bDoLock);

	if (nRPCCommand & RPC_DES_CODE)
		PDrvCryptoLockUnlockHWA(RPC_DES_CODE, bDoLock);

	if (nRPCCommand & RPC_SHA_CODE)
		PDrvCryptoLockUnlockHWA(RPC_SHA_CODE, bDoLock);
}

/*------------------------------------------------------------------------- */
/*
 *HWA public lock or unlock one HWA according algo specified by nHWAID
 */
void PDrvCryptoLockUnlockHWA(u32 nHWAID, bool bDoLock)
{
	struct semaphore *pHWAToLockUnlock;

	dprintk(KERN_INFO "PDrvCryptoLockUnlockHWA:nHWAID=0x%04X bDoLock=%d\n",
		nHWAID, bDoLock);

	switch (nHWAID) {
	case RPC_AES_CODE:
		pHWAToLockUnlock = &g_SCXLNXDeviceMonitor.sAES1CriticalSection;
		break;
	case RPC_DES_CODE:
		pHWAToLockUnlock = &g_SCXLNXDeviceMonitor.sD3D2CriticalSection;
		break;
	default:
	case RPC_SHA_CODE:
		pHWAToLockUnlock = &g_SCXLNXDeviceMonitor.sSHAM1CriticalSection;
		break;
	}

	if (bDoLock == LOCK_HWA) {
		dprintk(KERN_INFO "PDrvCryptoLockUnlockHWA: "\
			"Wait for HWAID=0x%04X\n", nHWAID);
		down(pHWAToLockUnlock);
		dprintk(KERN_INFO "PDrvCryptoLockUnlockHWA: "\
			"Locked on HWAID=0x%04X\n", nHWAID);
	} else {
		up(pHWAToLockUnlock);
		dprintk(KERN_INFO "PDrvCryptoLockUnlockHWA: "\
			"Released for HWAID=0x%04X\n", nHWAID);
	}
}

/*------------------------------------------------------------------------- */
