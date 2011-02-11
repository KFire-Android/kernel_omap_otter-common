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

#ifndef __SCX_PUBLIC_CRYPTO_H
#define __SCX_PUBLIC_CRYPTO_H

#include "scxlnx_defs.h"
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 27)
#include <mach/io.h>
#else
#include <asm-arm/arch-omap/io.h>
#endif

typedef volatile unsigned int		VU32;
typedef volatile unsigned short	VU16;
typedef volatile unsigned char	VU8;

#define OUTREG32(a, b)	(*(volatile unsigned int *)(a) = (unsigned int)b)
#define INREG32(a)		(*(volatile unsigned int *)(a))
#define SETREG32(x, y)	OUTREG32(x, INREG32(x) | (y))
#define CLRREG32(x, y)	OUTREG32(x, INREG32(x) & ~(y))

#define PUBLIC_CRYPTO_DES1_CLOCK_BIT		(1<<0)
#define PUBLIC_CRYPTO_SHAM1_CLOCK_BIT		(1<<1)
#define PUBLIC_CRYPTO_AES1_CLOCK_BIT		(1<<3)

#define PUBLIC_CRYPTO_DES2_CLOCK_BIT		(1<<26)

#define BYTES_TO_LONG(a)(u32)(a[0] | (a[1]<<8) | (a[2]<<16) | (a[3]<<24))
#define LONG_TO_BYTE(a, b) {  a[0] = (u8)((b) & 0xFF);		 \
				a[1] = (u8)(((b) >> 8) & 0xFF);  \
				a[2] = (u8)(((b) >> 16) & 0xFF); \
				a[3] = (u8)(((b) >> 24) & 0xFF); }

#define IS_4_BYTES_ALIGNED(x)((!((x) & 0x3)) ? true : false)

#define SMODULE_SMC_OMAP3XXX_PUBLIC_DMA

/*
 *The size limit to trigger DMA for AES, DES and Digest.
 *0xFFFFFFFF means "never"
 */
#define DMA_TRIGGER_IRQ_AES				128
#define DMA_TRIGGER_IRQ_DES				128
#define DMA_TRIGGER_IRQ_DIGEST				1024

/*Error code constants */
#define PUBLIC_CRYPTO_OPERATION_SUCCESS	0x00000000
#define PUBLIC_CRYPTO_ERR_ACCESS_DENIED	0x00000001
#define PUBLIC_CRYPTO_ERR_OUT_OF_MEMORY	0x00000002
#define PUBLIC_CRYPTO_ERR_BAD_PARAMETERS	0x00000003
#define PUBLIC_CRYPTO_ERR_TIMEOUT		0x00000004

/*DMA mode constants */
#define PUBLIC_CRYPTO_DMA_USE_NONE	0x00000000	/*No DMA used*/
/*DMA with active polling used */
#define PUBLIC_CRYPTO_DMA_USE_POLLING	0x00000001
#define PUBLIC_CRYPTO_DMA_USE_IRQ	0x00000002	/*DMA with IRQ used*/

#define PUBLIC_CRYPTO_REG_SET_BIT(x, y)	OUTREG32(x, INREG32(x) | y);
#define PUBLIC_CRYPTO_REG_UNSET_BIT(x, y)	OUTREG32(x, INREG32(x) & (~y));

#define AES_BLOCK_SIZE			16
#define DES_BLOCK_SIZE			8
#define HASH_BLOCK_SIZE			64

#define HASH_MD5_LENGTH			16
#define HASH_SHA1_LENGTH		20
#define HASH_SHA224_LENGTH		28
#define HASH_SHA256_LENGTH		32

#define PUBLIC_CRYPTO_DIGEST_MAX_SIZE	32
#define PUBLIC_CRYPTO_IV_MAX_SIZE	16

#define PUBLIC_CRYPTO_HW_CLOCK_ADDR		(0x48004A14)
#define PUBLIC_CRYPTO_HW_AUTOIDLE_ADDR		(0x48004A34)

#define PUBLIC_CRYPTO_HW_CLOCK1_ADDR		(0x48004A10)
#define PUBLIC_CRYPTO_HW_AUTOIDLE1_ADDR	(0x48004A30)

#define DIGEST_CTRL_ALGO_MD5		0
#define DIGEST_CTRL_ALGO_SHA1		1
#define DIGEST_CTRL_ALGO_SHA224		2
#define DIGEST_CTRL_ALGO_SHA256		3

/*-------------------------------------------------------------------------- */
/*
 *Public crypto API (Top level)
 */

/*
*Initialize the public crypto DMA chanels and global HWA semaphores
 */
u32 scxPublicCryptoInit(void);

/*
 *Initialize the device context CUS fields
 *(shortcut semaphore and public CUS list)
 */
void scxPublicCryptoInitDeviceContext(SCXLNX_CONN_MONITOR *pDeviceContext);

/**
 *Terminate the public crypto (including DMA)
 */
void scxPublicCryptoTerminate(void);

bool scxPublicCryptoParseCommandMessage(
				SCXLNX_CONN_MONITOR *pConn,
				SCXLNX_SHMEM_DESC *pDeviceContextDesc,
				CRYPTOKI_UPDATE_SHORTCUT_CONTEXT *pCUSContext,
				SCX_COMMAND_INVOKE_CLIENT_COMMAND *pCommand,
				CRYPTOKI_UPDATE_PARAMS *pParams);

/*
 *Perform a crypto update operation.
 *THIS FUNCTION IS CALLED FROM THE IOCTL
 */
void scxPublicCryptoUpdate(
	CRYPTOKI_UPDATE_SHORTCUT_CONTEXT *pCUSContext,
	CRYPTOKI_UPDATE_PARAMS *pCUSParams);

/*
 *i.e. copy the result into the user output buffer and release the resources.
 *THIS FUNCTION IS CALLED FROM THE USER THREAD (ioctl).
 */
void scxPublicCryptoWriteAnswerMessage(
				CRYPTOKI_UPDATE_SHORTCUT_CONTEXT *pCUSContext,
				CRYPTOKI_UPDATE_PARAMS *pCUSParams,
				SCX_ANSWER_INVOKE_CLIENT_COMMAND *pAnswer);

/*----------------------------------------------------------------------*/
/*				CUS RPCs				*/
/*----------------------------------------------------------------------*/

u32 scxPublicCryptoInstallShortcutLockAccelerator(
					u32 nRPCCommand,
					void *pL0SharedBuffer);

u32 scxPublicCryptoLockAcceleratorsSuspendShortcut(
					u32 nRPCCommand,
					void *pL0SharedBuffer);

u32 scxPublicCryptoResumeShortcutUnlockAccelerators(
					u32 nRPCCommand,
					void *pL0SharedBuffer);

u32 scxPublicCryptoClearGlobalKeyContext(
					u32 nRPCCommand,
					void *pL0SharedBuffer);

/*----------------------------------------------------------------------*/
/*			generic HWA utilities for CUS			*/
/*--------------------------------------------------------------------- */

/*HWA public lock or unlock one HWA according algo specified by nHWAID */
void PDrvCryptoLockUnlockHWA(u32 nHWAID, bool bDoLock);

/*HWAs public lock or unlock HWA's specified in the HWA H/A/D fields of RPC
	command nRPCCommand */
void PDrvCryptoLockUnlockHWAs(u32 nRPCCommand, bool bDoLock);

/*Check if the command must be intercepted by a CUS or not. */
bool scxPublicCryptoIsShortcutedCommand(
			SCXLNX_CONN_MONITOR *pDeviceContext,
			SCX_COMMAND_INVOKE_CLIENT_COMMAND *pCommand,
			CRYPTOKI_UPDATE_SHORTCUT_CONTEXT **ppCUSContext,
			bool incrementnUseCount);

/*get the device context conn monitor from memory hDeviceContext handle
	comming from secure; return NULL if it does not exit.*/
SCXLNX_CONN_MONITOR *PDrvCryptoGetDeviceContextFromHandle(u32 hDeviceContext);

/*get the shared memory from memory block handle comming from secure
	return NULL if it does not exit.*/
SCXLNX_SHMEM_DESC *PDrvCryptoGetSharedMemoryFromBlockHandle(
					SCXLNX_CONN_MONITOR *pConn,
					u32 hBlock);

/*-------------------------------------------------------------------------- */
/*
 *Helper methods
 */
u32 scxPublicCryptoWaitForReadyBit(VU32 *pRegister, u32 vBit);
void scxPublicCryptoWaitForReadyBitInfinitely(VU32 *pRegister, u32 vBit);
void scxPublicCryptoEnableClock(uint32_t vClockBit);
void scxPublicCryptoDisableClock(uint32_t vClockBit);

/*---------------------------------------------------------------------------*/
/*                               AES operations                              */
/*---------------------------------------------------------------------------*/

void PDrvCryptoAESRegister(void);
void PDrvCryptoAESUnregister(void);

/**
 *This function performs an AES update operation.
 *
 *The AES1 accelerator is assumed loaded with the correct key
 *
 *AES_CTRL:		defines the mode and direction
 *pAESState:	defines the operation IV
 *pSrc:			Input buffer to process.
 *pDest:			Output buffer containing the processed data.
 *
 *nbBlocks number of block(s)to process.
 */
void PDrvCryptoUpdateAES(
			PUBLIC_CRYPTO_AES_OPERATION_STATE * pAESState,
			u8 *pSrc,
			u8 *pDest,
			u32 nbBlocks);

/*---------------------------------------------------------------------------*/
/*                              DES/DES3 operations                          */
/*---------------------------------------------------------------------------*/

/**
 *This function performs a DES update operation.
 *
 *The DES accelerator is assumed loaded with the correct key
 *
 *DES_CTRL:		defines the mode and direction
 *pDESState:	defines the operation IV
 *pSrc:			Input buffer to process.
 *pDest:			Output buffer containing the processed data.
 *nbBlocks:		Number of block(s)to process.
 */
void PDrvCryptoUpdateDES(
			u32 DES_CTRL,
			PUBLIC_CRYPTO_DES_OPERATION_STATE *pDESState,
			u8 *pSrc,
			u8 *pDest,
			u32 nbBlocks);

/*---------------------------------------------------------------------------*/
/*                               Digest operations                           */
/*---------------------------------------------------------------------------*/

void PDrvCryptoHashRegister(void);
void PDrvCryptoHashUnregister(void);

/**
 *This function performs a HASH update Operation.
 *
 *pSHAState:	State of the operation
 *pData:		Input buffer to process
 *dataLength:	Length in bytes of the input buffer.
 */
void PDrvCryptoUpdateHash(
			PUBLIC_CRYPTO_SHA_OPERATION_STATE * pSHAState,
			u8 *pData,
			u32 dataLength);


#ifdef SMODULE_SMC_OMAP3XXX_POWER_MANAGEMENT

/**
 *Activates an inactivity timer for power management.
 *If an update command has not been received in a time period
 *(the delay of the timer)then the clocks will be disabled by
 *the timer callabck.
 */
void scxPublicCryptoStartInactivityTimer(void);

#endif	/*SMODULE_SMC_OMAP3XXX_POWER_MANAGEMENT */

#endif /*__SCX_PUBLIC_CRYPTO_H */
