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

#include "scxlnx_defs.h"
#include "scxlnx_util.h"
#include "scxlnx_mshield.h"
#include "scx_public_crypto.h"
#include "scx_public_dma.h"

#define IO_ADDRESS OMAP2_L4_IO_ADDRESS

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

#define RPC_AES1_CODE	PUBLIC_CRYPTO_HWA_AES1
#define RPC_AES2_CODE	PUBLIC_CRYPTO_HWA_AES2
#define RPC_DES_CODE	PUBLIC_CRYPTO_HWA_DES
#define RPC_SHA_CODE	PUBLIC_CRYPTO_HWA_SHA

#define RPC_CRYPTO_COMMAND_MASK	0x000003c0

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

#define ENABLE_CLOCK	true
#define DISABLE_CLOCK	false

/*---------------------------------------------------------------------------*/
/*RPC IN/OUT structures for CUS implementation                               */
/*---------------------------------------------------------------------------*/

struct RPC_INSTALL_SHORTCUT_LOCK_ACCELERATOR_OUT {
	u32 nShortcutID;
	u32 nError;
};

struct RPC_INSTALL_SHORTCUT_LOCK_ACCELERATOR_IN {
	u32 nDeviceContextID;
	u32 hClientSession;
	u32 nCommandID;
	u32 hKeyContext;
	/**
	 *The identifier of the HWA accelerator that this shortcut uses!
	 *Possible values are:
	 *- 1 (RPC_AES1_CODE)
	 *- 2 (RPC_AES2_CODE)
	 *- 4 (RPC_DES_CODE)
	 *- 8 (RPC_SHA_CODE)
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
	 *For the SHA accelerator, nHWA_CTRL contains the following bits:
	 *- ALGO (bit 2:1):
	 *  0x0: MD5
	 *  0x1: SHA1
	 *  0x2: SHA-224
	 *  0x3: SHA-256
	 **/
	u32 nHWA_CTRL;
	union PUBLIC_CRYPTO_OPERATION_STATE sOperationState;
};

struct RPC_LOCK_HWA_SUSPEND_SHORTCUT_OUT {
	union PUBLIC_CRYPTO_OPERATION_STATE sOperationState;
};

struct RPC_LOCK_HWA_SUSPEND_SHORTCUT_IN {
	u32 nShortcutID;
};

struct RPC_RESUME_SHORTCUT_UNLOCK_HWA_IN {
	u32 nShortcutID;
	u32 hAES1KeyContext;
	u32 hAES2KeyContext;
	u32 hDESKeyContext;
	union PUBLIC_CRYPTO_OPERATION_STATE sOperationState;
};

/*------------------------------------------------------------------------- */
/*
 * SCXGetCUSDeviceContext(struct CRYPTOKI_UPDATE_SHORTCUT_CONTEXT *pCUSContext)
 * search in the all the device context (conns) if the CUS context specified by
 * pCUSContext exist.
 *
 * If it is found, return the device context where the CUS context is.
 * If is is not found, return NULL.
 */
static struct SCXLNX_CONNECTION *SCXGetCUSDeviceContext(
	struct CRYPTOKI_UPDATE_SHORTCUT_CONTEXT *pCUSContext)
{
	struct SCXLNX_CONNECTION *pDeviceContext = NULL;
	struct CRYPTOKI_UPDATE_SHORTCUT_CONTEXT *pCUSContextFromList = NULL;
	struct SCXLNX_DEVICE *pDevice = SCXLNXGetDevice();

	spin_lock(&(pDevice->connsLock));
	list_for_each_entry(pDeviceContext, &(pDevice->conns),
		list) {
		spin_lock(&(pDeviceContext->shortcutListCriticalSectionLock));
		list_for_each_entry(pCUSContextFromList,
			&(pDeviceContext->ShortcutList), list) {
			if ((u32)pCUSContextFromList == (u32)pCUSContext) {
				spin_unlock(&(pDeviceContext->
					shortcutListCriticalSectionLock));
				spin_unlock(&(pDevice->
					connsLock));
				return pDeviceContext;
			}
		}
		spin_unlock(&(pDeviceContext->
			shortcutListCriticalSectionLock));
	}
	spin_unlock(&(pDevice->connsLock));

	/*pCUSContext does not exist */
	return NULL;
}

/*------------------------------------------------------------------------- */
/*
 * Get the shared memory from the memory block handle coming from secure.
 * Return NULL if it does not exist.
 */
static struct SCXLNX_SHMEM_DESC *PDrvCryptoGetSharedMemoryFromBlockHandle(
	struct SCXLNX_CONNECTION *pConn, u32 hBlock)
{
	struct SCXLNX_SHMEM_DESC *pSharedMemoryDescriptor = NULL;

	mutex_lock(&(pConn->sharedMemoriesMutex));

	list_for_each_entry(pSharedMemoryDescriptor,
			&(pConn->sUsedSharedMemoryList), list) {
		if ((u32) pSharedMemoryDescriptor->hIdentifier ==
				(u32) hBlock) {
			mutex_unlock(&(pConn->sharedMemoriesMutex));
			return pSharedMemoryDescriptor;
		}
	}

	/* hBlock does not exist */
	mutex_unlock(&(pConn->sharedMemoriesMutex));

	return NULL;
}

/*------------------------------------------------------------------------- */
/*
 * HWA public lock or unlock one HWA according algo specified by nHWAID
 */
void PDrvCryptoLockUnlockHWA(u32 nHWAID, bool bDoLock)
{
	int is_sem = 0;
	struct semaphore *s = NULL;
	struct mutex *m = NULL;
	struct SCXLNX_DEVICE *dev = SCXLNXGetDevice();

	dprintk(KERN_INFO "PDrvCryptoLockUnlockHWA:nHWAID=0x%04X bDoLock=%d\n",
		nHWAID, bDoLock);

	switch (nHWAID) {
	case RPC_AES1_CODE:
		s = &dev->sAES1CriticalSection;
		is_sem = 1;
		break;
	case RPC_AES2_CODE:
		s = &dev->sAES2CriticalSection;
		is_sem = 1;
		break;
	case RPC_DES_CODE:
		m = &dev->sDESCriticalSection;
		break;
	default:
	case RPC_SHA_CODE:
		m = &dev->sSHACriticalSection;
		break;
	}

	if (bDoLock == LOCK_HWA) {
		dprintk(KERN_INFO "PDrvCryptoLockUnlockHWA: "
			"Wait for HWAID=0x%04X\n", nHWAID);
		if (is_sem)
			down(s);
		else
			mutex_lock(m);
		dprintk(KERN_INFO "PDrvCryptoLockUnlockHWA: "
			"Locked on HWAID=0x%04X\n", nHWAID);
	} else {
		if (is_sem)
			up(s);
		else
			mutex_unlock(m);
		dprintk(KERN_INFO "PDrvCryptoLockUnlockHWA: "
			"Released for HWAID=0x%04X\n", nHWAID);
	}
}

/*------------------------------------------------------------------------- */
/*
 * HWAs public lock or unlock HWA's specified in the HWA H/A/D fields of RPC
 * command nRPCCommand
 */
static void PDrvCryptoLockUnlockHWAs(u32 nRPCCommand, bool bDoLock)
{
	dprintk(KERN_INFO
		"PDrvCryptoLockUnlockHWAs: nRPCCommand=0x%08x bDoLock=%d\n",
		nRPCCommand, bDoLock);

	/*perform the locks */
	if (nRPCCommand & RPC_AES1_CODE)
		PDrvCryptoLockUnlockHWA(RPC_AES1_CODE, bDoLock);

	if (nRPCCommand & RPC_AES2_CODE)
		PDrvCryptoLockUnlockHWA(RPC_AES2_CODE, bDoLock);

	if (nRPCCommand & RPC_DES_CODE)
		PDrvCryptoLockUnlockHWA(RPC_DES_CODE, bDoLock);

	if (nRPCCommand & RPC_SHA_CODE)
		PDrvCryptoLockUnlockHWA(RPC_SHA_CODE, bDoLock);
}

/*------------------------------------------------------------------------- */
/**
 *Initialize the public crypto DMA channels, global HWA semaphores and handles
 */
u32 SCXPublicCryptoInit(void)
{
	struct SCXLNX_DEVICE *pDevice = SCXLNXGetDevice();
	u32 nError = PUBLIC_CRYPTO_OPERATION_SUCCESS;

	/* Initialize HWAs */
	PDrvCryptoAESInit();
	PDrvCryptoDESInit();
	PDrvCryptoDigestInit();

	/*initialize the HWA semaphores */
	sema_init(&pDevice->sAES1CriticalSection, 1);
	sema_init(&pDevice->sAES2CriticalSection, 1);
	mutex_init(&pDevice->sDESCriticalSection);
	mutex_init(&pDevice->sSHACriticalSection);

	/*initialize the current key handle loaded in the AESn/DES HWA */
	pDevice->hAES1SecureKeyContext = 0;
	pDevice->hAES2SecureKeyContext = 0;
	pDevice->hDESSecureKeyContext = 0;
	pDevice->bSHAM1IsPublic = false;

	/*initialize the DMA semaphores */
	mutex_init(&pDevice->sm.sDMALock);

	/*allocate DMA buffer */
	pDevice->nDMABufferLength = PAGE_SIZE * 16;
	pDevice->pDMABuffer = dma_alloc_coherent(NULL,
		pDevice->nDMABufferLength,
		&(pDevice->pDMABufferPhys),
		GFP_KERNEL);
	if (pDevice->pDMABuffer == NULL) {
		printk(KERN_ERR
			"SCXPublicCryptoInit: Out of memory for DMA buffer\n");
		nError = S_ERROR_OUT_OF_MEMORY;
	}

	return nError;
}

/*------------------------------------------------------------------------- */
/*
 *Initialize the device context CUS fields (shortcut semaphore and public CUS
 *list)
 */
void SCXPublicCryptoInitDeviceContext(struct SCXLNX_CONNECTION *pDeviceContext)
{
	/*initialize the CUS list in the given device context */
	spin_lock_init(&(pDeviceContext->shortcutListCriticalSectionLock));
	INIT_LIST_HEAD(&(pDeviceContext->ShortcutList));
}

/*------------------------------------------------------------------------- */
/**
 *Terminate the public crypto (including DMA)
 */
void SCXPublicCryptoTerminate()
{
	struct SCXLNX_DEVICE *pDevice = SCXLNXGetDevice();

	if (pDevice->pDMABuffer != NULL) {
		dma_free_coherent(NULL, pDevice->nDMABufferLength,
			pDevice->pDMABuffer,
			pDevice->pDMABufferPhys);
		pDevice->pDMABuffer = NULL;
	}

	PDrvCryptoDigestExit();
	PDrvCryptoDESExit();
	PDrvCryptoAESExit();
}

/*------------------------------------------------------------------------- */
/*
 *Perform a crypto update operation.
 *THIS FUNCTION IS CALLED FROM THE IOCTL
 */
static bool SCXPublicCryptoUpdate(
	struct CRYPTOKI_UPDATE_SHORTCUT_CONTEXT *pCUSContext,
	struct CRYPTOKI_UPDATE_PARAMS *pCUSParams)
{
	bool status = true;
	dprintk(KERN_INFO
		"scxPublicCryptoUpdate(%x): "\
		"HWAID=0x%x, In=%p, Out=%p, Len=%u\n",
		(uint32_t) pCUSContext, pCUSContext->nHWAID,
		pCUSParams->pInputData,
		pCUSParams->pResultData, pCUSParams->nInputDataLength);

	/* Enable the clock and Process Data */
	switch (pCUSContext->nHWAID) {
	case RPC_AES1_CODE:
		SCXPublicCryptoEnableClock(PUBLIC_CRYPTO_AES1_CLOCK_REG);
		pCUSContext->sOperationState.aes.key_is_public = 0;
		pCUSContext->sOperationState.aes.CTRL = pCUSContext->nHWA_CTRL;
		status = PDrvCryptoUpdateAES(
			&pCUSContext->sOperationState.aes,
			pCUSParams->pInputData,
			pCUSParams->pResultData,
			pCUSParams->nInputDataLength / AES_BLOCK_SIZE);
		SCXPublicCryptoDisableClock(PUBLIC_CRYPTO_AES1_CLOCK_REG);
		break;

	case RPC_AES2_CODE:
		SCXPublicCryptoEnableClock(PUBLIC_CRYPTO_AES2_CLOCK_REG);
		pCUSContext->sOperationState.aes.key_is_public = 0;
		pCUSContext->sOperationState.aes.CTRL = pCUSContext->nHWA_CTRL;
		status = PDrvCryptoUpdateAES(
			&pCUSContext->sOperationState.aes,
			pCUSParams->pInputData,
			pCUSParams->pResultData,
			pCUSParams->nInputDataLength / AES_BLOCK_SIZE);
		SCXPublicCryptoDisableClock(PUBLIC_CRYPTO_AES2_CLOCK_REG);
		break;


	case RPC_DES_CODE:
		SCXPublicCryptoEnableClock(PUBLIC_CRYPTO_DES3DES_CLOCK_REG);
		status = PDrvCryptoUpdateDES(
			pCUSContext->nHWA_CTRL,
			&pCUSContext->sOperationState.des,
			pCUSParams->pInputData,
			pCUSParams->pResultData,
			pCUSParams->nInputDataLength / DES_BLOCK_SIZE);
		SCXPublicCryptoDisableClock(PUBLIC_CRYPTO_DES3DES_CLOCK_REG);
		break;

	case RPC_SHA_CODE:
		SCXPublicCryptoEnableClock(PUBLIC_CRYPTO_SHA2MD5_CLOCK_REG);
		pCUSContext->sOperationState.sha.CTRL = pCUSContext->nHWA_CTRL;
		PDrvCryptoUpdateHash(
			&pCUSContext->sOperationState.sha,
			pCUSParams->pInputData,
			pCUSParams->nInputDataLength);
		SCXPublicCryptoDisableClock(PUBLIC_CRYPTO_SHA2MD5_CLOCK_REG);
		break;

	default:
		BUG_ON(1);
		break;
	}

	dprintk(KERN_INFO "scxPublicCryptoUpdate: Done\n");
	return status;
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
 * struct CRYPTOKI_UPDATE_SHORTCUT_CONTEXT **ppPublicCUSContext : the public CUS
 *       if it is shortcuted
 *return: true or false
 *
 */
static bool SCXPublicCryptoIsShortcutedCommand(
	struct SCXLNX_CONNECTION *pDeviceContext,
	struct SCX_COMMAND_INVOKE_CLIENT_COMMAND *pCommand,
	struct CRYPTOKI_UPDATE_SHORTCUT_CONTEXT **ppCUSContext,
	bool incrementnUseCount)
{
	struct SCXLNX_DEVICE *pDevice = SCXLNXGetDevice();
	struct CRYPTOKI_UPDATE_SHORTCUT_CONTEXT *pCUSContext = NULL;
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
				/*For AES and DES/3DES operations,
				 *provisionally determine if the accelerator
				 *is loaded with the appropriate key before
				 *deciding to enter the accelerator critical
				 *section. In most cases, if some other thread
				 *or the secure world is currently using the
				 *accelerator, the key won't change.
				 *So, if the key doesn't match now, it is
				 *likely not to match later on, so we'd better
				 *not try to enter the critical section in this
				 *case: */

				if ((pCUSContext->nHWAID == RPC_AES1_CODE &&
					pCUSContext->
					hKeyContext != pDevice->
					hAES1SecureKeyContext) ||
				    (pCUSContext->nHWAID == RPC_AES2_CODE &&
					pCUSContext->
					hKeyContext != pDevice->
					hAES2SecureKeyContext)) {
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
						pDevice->
						hAES1SecureKeyContext);
					goto command_not_shortcutable;

				} else if (pCUSContext->nHWAID == RPC_DES_CODE
						&& pCUSContext->hKeyContext !=
						pDevice->
						hDESSecureKeyContext) {
					/*
					 * For DES/3DES atomically read
					 * hDESSecureKeyContext and check if
					 * it is equal to hKeyContext. If not,
					 * take the standard path <=> do not
					 * shortcut
					 */
					dprintk(KERN_INFO
						"shortcut exists but DES key "
						"not correct "
						"hDESSecureKeyContext = 0x%08x"
						" hKeyContext0x%08x\n",
						(u32)pDevice->
						hDESSecureKeyContext,
						(u32)pCUSContext->hKeyContext);
					goto command_not_shortcutable;
				} else if (pCUSContext->nHWAID == RPC_SHA_CODE
						&& !pDevice->bSHAM1IsPublic) {
					/*
					 * For digest operations, atomically
					 * read bSHAM1IsPublic and check if it
					 * is true. If not, no shortcut.
					 */
					 dprintk(KERN_INFO
						 "shortcut exists but SHAM1 "
						 "is not accessible in public");
					 goto command_not_shortcutable;
				}
			}

			dprintk(KERN_INFO "shortcut exists and enable\n");

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
 * Pre-process the client command (crypto update operation), i.e., parse the
 * command message (decode buffers, etc.) THIS FUNCTION IS CALLED FROM THE USER
 * THREAD (ioctl).
 *
 * For incorrect messages, an error is returned and the message will be sent to
 * secure
 */
static bool SCXPublicCryptoParseCommandMessage(struct SCXLNX_CONNECTION *pConn,
	struct CRYPTOKI_UPDATE_SHORTCUT_CONTEXT *pCUSContext,
	struct SCX_COMMAND_INVOKE_CLIENT_COMMAND *pCommand,
	struct CRYPTOKI_UPDATE_PARAMS *pCUSParams)
{
	u32 nParamType;
	u32 nInputDataLength;
	u32 nResultDataLength;
	u8 *pSharedMem;
	u8 *pInputData;
	u8 *pResultData;
	struct SCXLNX_SHMEM_DESC *pInputShmem = NULL;
	struct SCXLNX_SHMEM_DESC *pOutputShmem = NULL;

	dprintk(KERN_INFO
		"scxPublicCryptoParseCommandMessage(%p) : Session=0x%x\n",
		pCUSContext, pCUSContext->hClientSession);

	if (pCommand->sParams[0].sTempMemref.nSize == 0)
		return false;

	nParamType = SCX_GET_PARAM_TYPE(pCommand->nParamTypes, 0);
	switch (nParamType) {
	case SCX_PARAM_TYPE_MEMREF_TEMP_INPUT:
		if (pCommand->sParams[0].sTempMemref.nDescriptor == 0)
			return false;

		pInputData = internal_kmalloc(
			pCommand->sParams[0].sTempMemref.nSize,
			GFP_KERNEL);

		if (pInputData == NULL)
			return false;

		if (copy_from_user(pInputData,
				(u8 *) pCommand->sParams[0].sTempMemref.
					nDescriptor,
				pCommand->sParams[0].sTempMemref.nSize))
			goto err0;

		nInputDataLength = pCommand->sParams[0].sTempMemref.nSize;

		break;

	case SCX_PARAM_TYPE_MEMREF_INPUT:
		pInputData = internal_kmalloc(
			pCommand->sParams[0].sMemref.nSize, GFP_KERNEL);

		if (pInputData == NULL)
			return false;

		pInputShmem = PDrvCryptoGetSharedMemoryFromBlockHandle(pConn,
			pCommand->sParams[0].sMemref.hBlock);

		if (pInputShmem == NULL)
			goto err0;
		atomic_inc(&pInputShmem->nRefCnt);

		if (copy_from_user(pInputData, pInputShmem->pBuffer +
				pCommand->sParams[0].sMemref.nOffset,
				pCommand->sParams[0].sMemref.nSize))
			goto err0;

		nInputDataLength = pCommand->sParams[0].sMemref.nSize;

		break;

	default:
		return false;
	}

	if (pCUSContext->nHWAID != RPC_SHA_CODE) {
		if (pCommand->sParams[1].sTempMemref.nSize == 0)
			goto err0;

		/* We need an output buffer as well */
		nParamType = SCX_GET_PARAM_TYPE(pCommand->nParamTypes, 1);
		switch (nParamType) {
		case SCX_PARAM_TYPE_MEMREF_TEMP_OUTPUT:
			pResultData = internal_kmalloc(
				pCommand->sParams[1].sTempMemref.nSize,
				GFP_KERNEL);

			if (pResultData == NULL)
				goto err0;

			pSharedMem =
				(u8 *) pCommand->sParams[1].sTempMemref.
					nDescriptor;

			nResultDataLength =
				pCommand->sParams[1].sTempMemref.nSize;

			break;

		case SCX_PARAM_TYPE_MEMREF_OUTPUT:
			if (pCommand->sParams[1].sTempMemref.nDescriptor == 0)
				return false;

			pResultData = internal_kmalloc(
				pCommand->sParams[1].sMemref.nSize,
				GFP_KERNEL);

			if (pResultData == NULL)
				goto err0;

			pOutputShmem = PDrvCryptoGetSharedMemoryFromBlockHandle(
				pConn, pCommand->sParams[1].sMemref.hBlock);
			if (pOutputShmem == NULL)
				goto err1;
			atomic_inc(&pOutputShmem->nRefCnt);

			pSharedMem = pOutputShmem->pBuffer +
				pCommand->sParams[1].sMemref.nOffset;

			nResultDataLength = pCommand->sParams[1].sMemref.nSize;

			break;

		default:
			dprintk(KERN_ERR "SCXPublicCryptoParseCommandMessage: "
				"Encrypt/decrypt operations require an output "
				"buffer\n");

			goto err0;
		}

		if (nResultDataLength < nInputDataLength) {
			dprintk(KERN_ERR "SCXPublicCryptoParseCommandMessage: "
				"Short buffer: nResultDataLength = %d < "
				"nInputDataLength = %d\n",
				nResultDataLength, nInputDataLength);
			goto err1;
		}
	} else {
		nResultDataLength = 0;
		pResultData = NULL;
		pSharedMem = NULL;
	}

	/*
	 * Check if input length is compatible with the algorithm of the
	 * shortcut
	 */
	switch (pCUSContext->nHWAID) {
	case RPC_AES1_CODE:
	case RPC_AES2_CODE:
		/* Must be multiple of the AES block size */
		if ((nInputDataLength % AES_BLOCK_SIZE) != 0) {
			dprintk(KERN_ERR
				"scxPublicCryptoParseCommandMessage(%p): "\
				"Input Data Length invalid [%d] for AES\n",
				pCUSContext, nInputDataLength);
			goto err1;
		}
		break;
	case RPC_DES_CODE:
		/* Must be multiple of the DES block size */
		if ((nInputDataLength % DES_BLOCK_SIZE) != 0) {
			dprintk(KERN_ERR
				"scxPublicCryptoParseCommandMessage(%p): "\
				"Input Data Length invalid [%d] for DES\n",
				pCUSContext, nInputDataLength);
			goto err1;
		}
		break;
	default:
		/* SHA operation: no constraint on data length */
		break;
	}

	pCUSParams->pInputData = pInputData;
	pCUSParams->nInputDataLength = nInputDataLength;
	pCUSParams->pInputShmem = pInputShmem;
	pCUSParams->pResultData = pResultData;
	pCUSParams->nResultDataLength = nResultDataLength;
	pCUSParams->pOutputShmem = pOutputShmem;

	/*
	 * Save userspace buffer vaddr so we can copy back result data later on
	 */
	pCUSParams->pS2CDataBuffer = pSharedMem;

	return true;

err1:
	internal_kfree(pResultData);

	if (pOutputShmem)
		atomic_dec(&pOutputShmem->nRefCnt);
err0:
	internal_kfree(pInputData);

	if (pInputShmem)
		atomic_dec(&pInputShmem->nRefCnt);

	return false;
}

/*------------------------------------------------------------------------- */

/*
 *Post-process the client command (crypto update operation),
 *i.e. copy the result into the user output buffer and release the resources.
 *THIS FUNCTION IS CALLED FROM THE USER THREAD (ioctl).
 */
static void SCXPublicCryptoWriteAnswerMessage(
	struct CRYPTOKI_UPDATE_SHORTCUT_CONTEXT *pCUSContext,
	struct CRYPTOKI_UPDATE_PARAMS *pCUSParams,
	struct SCX_ANSWER_INVOKE_CLIENT_COMMAND *pAnswer)
{
	u32 nError = S_SUCCESS;

	dprintk(KERN_INFO
		"scxPublicCryptoWriteAnswerMessage(%p) : Session=0x%x\n",
		pCUSContext, pCUSContext->hClientSession);

	/* Do we need to copy data back to userland? */
	if (pCUSParams->pS2CDataBuffer != NULL) {
		if (copy_to_user(pCUSParams->pS2CDataBuffer,
				pCUSParams->pResultData,
				pCUSParams->nResultDataLength))
			nError = S_ERROR_NO_DATA;

		internal_kfree(pCUSParams->pResultData);
	}

	internal_kfree(pCUSParams->pInputData);

	/* Generate the answer */
	pAnswer->nMessageSize =
		(sizeof(struct SCX_ANSWER_INVOKE_CLIENT_COMMAND) -
		 sizeof(struct SCX_ANSWER_HEADER)) / 4;
	pAnswer->nMessageType = SCX_MESSAGE_TYPE_INVOKE_CLIENT_COMMAND;
	pAnswer->nReturnOrigin = SCX_ORIGIN_TRUSTED_APP;
	pAnswer->nOperationID = 0;
	pAnswer->nErrorCode = nError;
	pAnswer->sAnswers[1].sSize.nSize = pCUSParams->nResultDataLength;
}

/*------------------------------------------------------------------------- */

int SCXPublicCryptoTryShortcutedUpdate(struct SCXLNX_CONNECTION *pConn,
	struct SCX_COMMAND_INVOKE_CLIENT_COMMAND *pMessage,
	struct SCX_ANSWER_INVOKE_CLIENT_COMMAND *pAnswer)
{
	struct CRYPTOKI_UPDATE_SHORTCUT_CONTEXT *pCUSContext = NULL;

	if (SCXPublicCryptoIsShortcutedCommand(pConn,
			(struct SCX_COMMAND_INVOKE_CLIENT_COMMAND *) pMessage,
			&pCUSContext, false)) {
		u32 nHWAID = pCUSContext->nHWAID;

		/* Lock HWA */
		PDrvCryptoLockUnlockHWA(nHWAID, LOCK_HWA);

		if (SCXPublicCryptoIsShortcutedCommand(pConn,
				pMessage,
				&pCUSContext, true)) {
			struct CRYPTOKI_UPDATE_PARAMS sCUSParams;

			memset(&sCUSParams, 0, sizeof(sCUSParams));

			if (!SCXPublicCryptoParseCommandMessage(
					pConn,
					pCUSContext,
					pMessage,
					&sCUSParams)) {
				/* Decrement CUS context use count */
				pCUSContext->nUseCount--;

				/* Release HWA lock */
				PDrvCryptoLockUnlockHWA(pCUSContext->nHWAID,
					UNLOCK_HWA);

				return -1;
			}

			/* Perform the update in public <=> THE shortcut */
			if (!SCXPublicCryptoUpdate(pCUSContext, &sCUSParams)) {
				/* Decrement CUS context use count */
				pCUSContext->nUseCount--;

				/* Release HWA lock */
				PDrvCryptoLockUnlockHWA(pCUSContext->nHWAID,
					UNLOCK_HWA);

				return -1;
			}

			/* Write answer message */
			SCXPublicCryptoWriteAnswerMessage(pCUSContext,
				&sCUSParams, pAnswer);

			/* Decrement registered shmems use count if needed */
			if (sCUSParams.pInputShmem)
				atomic_dec(&sCUSParams.pInputShmem->nRefCnt);
			if (sCUSParams.pOutputShmem)
				atomic_dec(&sCUSParams.pOutputShmem->nRefCnt);

			/* Decrement CUS context use count */
			pCUSContext->nUseCount--;

			PDrvCryptoLockUnlockHWA(pCUSContext->nHWAID,
				UNLOCK_HWA);
		} else {
			PDrvCryptoLockUnlockHWA(nHWAID, UNLOCK_HWA);
			return -1;
		}
	} else {
		return -1;
	}

	return 0;
}

/*------------------------------------------------------------------------- */

void SCXPublicCryptoWaitForReadyBitInfinitely(u32 *pRegister, u32 vBit)
{
	while (!(INREG32(pRegister) & vBit))
		;
}

/*------------------------------------------------------------------------- */

u32 SCXPublicCryptoWaitForReadyBit(u32 *pRegister, u32 vBit)
{
	u32 timeoutCounter = PUBLIC_CRYPTO_TIMEOUT_CONST;

	while ((!(INREG32(pRegister) & vBit)) && ((--timeoutCounter) != 0))
		;

	if (timeoutCounter == 0)
		return PUBLIC_CRYPTO_ERR_TIMEOUT;

	return PUBLIC_CRYPTO_OPERATION_SUCCESS;
}

/*------------------------------------------------------------------------- */

void SCXPublicCryptoDisableClock(uint32_t vClockPhysAddr)
{
	u32 *pClockReg;
	u32 val;

	dprintk(KERN_INFO "SCXPublicCryptoDisableClock: " \
		"vClockPhysAddr=0x%08X\n",
		vClockPhysAddr);

	/* Ensure none concurrent access when changing clock registers */
	spin_lock(&SCXLNXGetDevice()->sm.lock);

	pClockReg = (u32 *)IO_ADDRESS(vClockPhysAddr);

	val = __raw_readl(pClockReg);
	val &= ~(0x3);
	__raw_writel(val, pClockReg);

	/* Wait for clock to be fully disabled */
	while ((__raw_readl(pClockReg) & 0x30000) == 0)
		;

	tf_l4sec_clkdm_allow_idle(false, true);

	spin_unlock(&SCXLNXGetDevice()->sm.lock);
}

/*------------------------------------------------------------------------- */

void SCXPublicCryptoEnableClock(uint32_t vClockPhysAddr)
{
	u32 *pClockReg;
	u32 val;

	dprintk(KERN_INFO "SCXPublicCryptoEnableClock: " \
		"vClockPhysAddr=0x%08X\n",
		vClockPhysAddr);

	/* Ensure none concurrent access when changing clock registers */
	spin_lock(&SCXLNXGetDevice()->sm.lock);

	tf_l4sec_clkdm_wakeup(false, true);

	pClockReg = (u32 *)IO_ADDRESS(vClockPhysAddr);

	val = __raw_readl(pClockReg);
	val |= 0x2;
	__raw_writel(val, pClockReg);

	/* Wait for clock to be fully enabled */
	while ((__raw_readl(pClockReg) & 0x30000) != 0)
		;

	spin_unlock(&SCXLNXGetDevice()->sm.lock);
}

/*------------------------------------------------------------------------- */
/*                     CUS RPCs                                             */
/*------------------------------------------------------------------------- */
/*
 * This RPC is used by the secure world to install a new shortcut.  Optionally,
 * for AES or DES/3DES operations, it can also lock the accelerator so that the
 * secure world can install a new key in it.
 */
static int SCXPublicCryptoInstallShortcutLockAccelerator(
	u32 nRPCCommand, void *pRPCSharedBuffer)
{
	struct CRYPTOKI_UPDATE_SHORTCUT_CONTEXT *pCUSContext = NULL;
	struct SCXLNX_CONNECTION *pDeviceContext = NULL;

	/* Reference the input/ouput data */
	struct RPC_INSTALL_SHORTCUT_LOCK_ACCELERATOR_OUT *pInstallShortcutOut =
		pRPCSharedBuffer;
	struct RPC_INSTALL_SHORTCUT_LOCK_ACCELERATOR_IN *pInstallShortcutIn =
		pRPCSharedBuffer;

	dprintk(KERN_INFO "SCXPublicCryptoInstallShortcutLockAccelerator: "
		"nRPCCommand=0x%08x; nHWAID=0x%08x\n",
		nRPCCommand, pInstallShortcutIn->nHWAID);

	pDeviceContext = (struct SCXLNX_CONNECTION *)
		pInstallShortcutIn->nDeviceContextID;

	if (pDeviceContext == NULL) {
		dprintk(KERN_INFO
			"SCXPublicCryptoInstallShortcutLockAccelerator: "
			"DeviceContext 0x%08x does not exist, "
			"cannot create Shortcut\n",
			pInstallShortcutIn->nDeviceContextID);
		pInstallShortcutOut->nError = -1;
		return 0;
	}

	/*
	 * Allocate a shortcut context. If the allocation fails,
	 * return S_ERROR_OUT_OF_MEMORY error code
	 */
	pCUSContext = (struct CRYPTOKI_UPDATE_SHORTCUT_CONTEXT *)
		internal_kmalloc(sizeof(*pCUSContext), GFP_KERNEL);
	if (pCUSContext == NULL) {
		dprintk(KERN_ERR
			"scxPublicCryptoInstallShortcutLockAccelerator: "\
			"Out of memory for public session\n");
		pInstallShortcutOut->nError = S_ERROR_OUT_OF_MEMORY;
		return 0;
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
			 sizeof(union PUBLIC_CRYPTO_OPERATION_STATE));

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
 * This RPC is used to perform one or several of the following operations
 * - Lock one or several accelerators for the exclusive use by the secure world,
 *   either because it is going to be switched to secure or because a new key is
 *   going to be loaded in the accelerator
 * - Suspend a shortcut, i.e., make it temporarily unavailable to the public
 *   world. This is used when a secure update is going to be performed on the
 *   operation. The answer to the RPC then contains the operation state
 *   necessary for the secure world to do the update.
 * - Uninstall the shortcut
 */
static int SCXPublicCryptoLockAcceleratorsSuspendShortcut(
	u32 nRPCCommand, void *pRPCSharedBuffer)
{
	u32 nTargetShortcut;
	struct CRYPTOKI_UPDATE_SHORTCUT_CONTEXT *pCUSContext = NULL;
	struct SCXLNX_CONNECTION *pDeviceContext = NULL;

	/*reference the input/ouput data */
	struct RPC_LOCK_HWA_SUSPEND_SHORTCUT_OUT *pSuspendShortcutOut =
		pRPCSharedBuffer;
	struct RPC_LOCK_HWA_SUSPEND_SHORTCUT_IN *pSuspendShortcutIn =
		pRPCSharedBuffer;

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
		pCUSContext = (struct CRYPTOKI_UPDATE_SHORTCUT_CONTEXT *)
			pSuspendShortcutIn->nShortcutID;

		/*preventive check1: return if shortcut does not exist */
		pDeviceContext = SCXGetCUSDeviceContext(pCUSContext);
		if (pDeviceContext == NULL) {
			dprintk(KERN_INFO
			"scxPublicCryptoLockAcceleratorsSuspendShortcut: "\
			"nShortcutID=0x%08x does not exist, cannot suspend "\
			"Shortcut\n",
				pSuspendShortcutIn->nShortcutID);
			return -1;
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
				 sizeof(union PUBLIC_CRYPTO_OPERATION_STATE));

		/*Uninstall shortcut if requiered  */
		if ((nRPCCommand &
		RPC_LOCK_ACCELERATORS_SUSPEND_SHORTCUT_UNINSTALL) != 0) {
			dprintk(KERN_INFO
			"scxPublicCryptoLockAcceleratorsSuspendShortcut:"\
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
			return 0;
		}

		/*release shortcutListCriticalSectionLock */
		spin_unlock(&pDeviceContext->shortcutListCriticalSectionLock);
	}

	return 0;
}

/*------------------------------------------------------------------------- */

/*
 * This RPC is used to perform one or several of the following operations:
 * -	Resume a shortcut previously suspended
 * -	Inform the public driver of the new keys installed in the DES and AES2
 *	accelerators
 * -	Unlock some of the accelerators
 */
static int SCXPublicCryptoResumeShortcutUnlockAccelerators(
	u32 nRPCCommand, void *pRPCSharedBuffer)
{
	struct SCXLNX_DEVICE *pDevice = SCXLNXGetDevice();
	struct SCXLNX_CONNECTION *pDeviceContext = NULL;
	struct CRYPTOKI_UPDATE_SHORTCUT_CONTEXT *pCUSContext = NULL;

	/*reference the input data */
	struct RPC_RESUME_SHORTCUT_UNLOCK_HWA_IN *pResumeShortcutIn =
		pRPCSharedBuffer;

	dprintk(KERN_INFO
		"scxPublicCryptoResumeShortcutUnlockAccelerators\n"
		"nRPCCommand=0x%08x\nnShortcutID=0x%08x\n",
		nRPCCommand, pResumeShortcutIn->nShortcutID);

	/*if nShortcutID not 0 resume the shortcut and unlock HWA
		else only unlock HWA */
	if (pResumeShortcutIn->nShortcutID != 0) {
		/*reference the CUSContext */
		pCUSContext = (struct CRYPTOKI_UPDATE_SHORTCUT_CONTEXT *)
			pResumeShortcutIn->nShortcutID;

		/*preventive check1: return if shortcut does not exist
		 *else, points to the public crypto monitor (inside the device
		 *context) */
		pDeviceContext = SCXGetCUSDeviceContext(pCUSContext);
		if (pDeviceContext == NULL) {
			dprintk(KERN_INFO
			"scxPublicCryptoResumeShortcutUnlockAccelerators(...):"\
			"nShortcutID 0x%08x does not exist, cannot suspend "\
			"Shortcut\n",
				pResumeShortcutIn->nShortcutID);
			return -1;
		}

		/*if S set and shortcut not yet suspended */
		if ((pCUSContext->bSuspended) &&
			 ((nRPCCommand &
			RPC_RESUME_SHORTCUT_UNLOCK_ACCELERATORS_RESUME) != 0)){
			/*Write sOperationStateData in the shortcut context */
			memcpy(&pCUSContext->sOperationState,
				&pResumeShortcutIn->sOperationState,
				sizeof(union PUBLIC_CRYPTO_OPERATION_STATE));
			/*resume the shortcut */
			pCUSContext->bSuspended = false;
		}
	}

	/*
	 * If A is set: Atomically set hAES1SecureKeyContext to
	 * hAES1KeyContext
	 */
	if ((nRPCCommand &
		RPC_RESUME_SHORTCUT_UNLOCK_ACCELERATORS_UNLOCK_AES1) != 0) {
		pDevice->hAES1SecureKeyContext =
			pResumeShortcutIn->hAES1KeyContext;
	}

	/*
	 * If B is set: Atomically set hAES2SecureKeyContext to
	 * hAES2KeyContext
	 */
	if ((nRPCCommand &
		RPC_RESUME_SHORTCUT_UNLOCK_ACCELERATORS_UNLOCK_AES2) != 0) {
		pDevice->hAES2SecureKeyContext =
			pResumeShortcutIn->hAES2KeyContext;
	}

	/*
	 * If D is set:
	 * Atomically set hDESSecureKeyContext to hDESKeyContext
	 */
	if ((nRPCCommand &
		RPC_RESUME_SHORTCUT_UNLOCK_ACCELERATORS_UNLOCK_DES) != 0) {
		pDevice->hDESSecureKeyContext =
					pResumeShortcutIn->hDESKeyContext;
	}

	/* H is never set by the PA: Atomically set bSHAM1IsPublic to true */
	pDevice->bSHAM1IsPublic = true;

	/* Unlock HWAs according nRPCCommand */
	PDrvCryptoLockUnlockHWAs(nRPCCommand, UNLOCK_HWA);

	return 0;
}

/*------------------------------------------------------------------------- */

/*
 * This RPC is used to notify the public driver that the key in the AES2, DES
 * accelerators has been cleared. This happens only when the key is no longer
 * referenced by any shortcuts. So, it is guaranteed that no-one has entered the
 * accelerators critical section and there is no need to enter it to implement
 * this RPC.
 */
static int SCXPublicCryptoClearGlobalKeyContext(
	u32 nRPCCommand, void *pRPCSharedBuffer)
{
	struct SCXLNX_DEVICE *pDevice = SCXLNXGetDevice();

	/*
	 * If A is set: Atomically set hAES1SecureKeyContext to 0
	 */
	if ((nRPCCommand &
		RPC_RESUME_SHORTCUT_UNLOCK_ACCELERATORS_UNLOCK_AES1) != 0) {
		pDevice->hAES1SecureKeyContext = 0;
	}

	if ((nRPCCommand &
		RPC_RESUME_SHORTCUT_UNLOCK_ACCELERATORS_UNLOCK_AES2) != 0) {
		pDevice->hAES2SecureKeyContext = 0;
	}

	/*
	 *If D is set: Atomically set hDESSecureKeyContext to 0
	 */
	if ((nRPCCommand &
		RPC_RESUME_SHORTCUT_UNLOCK_ACCELERATORS_UNLOCK_DES) != 0) {
		pDevice->hDESSecureKeyContext = 0;
	}

	return 0;
}

/*------------------------------------------------------------------------- */
/*
 * Execute a public crypto related RPC
 */

int SCXPublicCryptoExecuteRPCCommand(u32 nRPCCommand, void *pRPCSharedBuffer)
{
	switch (nRPCCommand & RPC_CRYPTO_COMMAND_MASK) {
	case RPC_INSTALL_SHORTCUT_LOCK_ACCELERATOR:
		dprintk(KERN_INFO "RPC_INSTALL_SHORTCUT_LOCK_ACCELERATOR\n");
		return SCXPublicCryptoInstallShortcutLockAccelerator(
			nRPCCommand, pRPCSharedBuffer);

	case RPC_LOCK_ACCELERATORS_SUSPEND_SHORTCUT:
		dprintk(KERN_INFO "RPC_LOCK_ACCELERATORS_SUSPEND_SHORTCUT\n");
		return SCXPublicCryptoLockAcceleratorsSuspendShortcut(
			nRPCCommand, pRPCSharedBuffer);

	case RPC_RESUME_SHORTCUT_UNLOCK_ACCELERATORS:
		dprintk(KERN_INFO "RPC_RESUME_SHORTCUT_UNLOCK_ACCELERATORS\n");
		return SCXPublicCryptoResumeShortcutUnlockAccelerators(
			nRPCCommand, pRPCSharedBuffer);

	case RPC_CLEAR_GLOBAL_KEY_CONTEXT:
		dprintk(KERN_INFO "RPC_CLEAR_GLOBAL_KEY_CONTEXT\n");
		return SCXPublicCryptoClearGlobalKeyContext(
			nRPCCommand, pRPCSharedBuffer);
	}

	return -1;
}



