/*
 * Copyright (c) 2010 Trusted Logic S.A.
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

#include <asm/div64.h>
#include <asm/system.h>
#include <asm/cputype.h>
#include <asm/uaccess.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/page-flags.h>
#include <linux/pagemap.h>
#include <linux/vmalloc.h>
#include <linux/version.h>
#include <linux/jiffies.h>

#include <asm/cacheflush.h>

#include <plat/clockdomain.h>

#include "scxlnx_defs.h"
#include "scxlnx_comm.h"
#include "scxlnx_util.h"
#include "scxlnx_conn.h"
#include "scxlnx_mshield.h"
#include "scx_public_crypto.h"

/*--------------------------------------------------------------------------
 * Internal constants
 *-------------------------------------------------------------------------- */

/* RPC commands */
#define RPC_CMD_YIELD	0x00
#define RPC_CMD_INIT	0x01
#define RPC_CMD_TRACE	0x02

/* Trace offset */
#define RPC_TRACE_OFFSET	0xC00

/* RPC return values to secure world */
#define RPC_SUCCESS			0x00000000
#define RPC_ERROR_BAD_PARAMETERS	0xFFFF0006
#define RPC_ERROR_CONNECTION_PROTOCOL	0xFFFF3020

/*
 * RPC call status
 *
 * 0: the secure world yielded due to an interrupt
 * 1: the secure world yielded on an RPC (no public world thread is handling it)
 * 2: the secure world yielded on an RPC and the response to that RPC is now in
 *    place
 */
#define RPC_ADVANCEMENT_NONE		0
#define RPC_ADVANCEMENT_PENDING		1
#define RPC_ADVANCEMENT_FINISHED	2

atomic_t g_RPC_advancement = ATOMIC_INIT(RPC_ADVANCEMENT_NONE);
u32 g_RPC_parameters[4] = {0, 0, 0, 0};
u32 g_secure_task_id;
u32 g_service_end;

/*
 * Secure ROMCode HAL API Identifiers
 */
#define API_HAL_LM_PALOAD_INDEX                 0x05
#define API_HAL_LM_PAUNLOADALL_INDEX            0x07
#define API_HAL_SDP_RUNTIMEINIT_INDEX           0x04
#define API_HAL_TASK_MGR_RPCINIT_INDEX          0x08
#define API_HAL_SEC_L3_RAM_RESIZE_INDEX         0x17
#define API_HAL_CONTEXT_SAVESECURERAM_INDEX     0x1A
#define API_HAL_KM_GETSECUREROMCODECRC_INDEX    0x0B

#define API_HAL_RET_VALUE_OK	0x0

/* SE entry flags */
#define FLAG_START_HAL_CRITICAL     0x4
#define FLAG_IRQFIQ_MASK            0x3
#define FLAG_IRQ_ENABLE             0x2
#define FLAG_FIQ_ENABLE             0x1

#define SMICODEPUB_IRQ_END	0xFE
#define SMICODEPUB_FIQ_END	0xFD
#define SMICODEPUB_RPC_END	0xFC

#define SEC_RAM_SIZE_40KB	0x0000A000
#define SEC_RAM_SIZE_48KB	0x0000C000
#define SEC_RAM_SIZE_52KB	0x0000D000
#define SEC_RAM_SIZE_60KB	0x0000F000
#define SEC_RAM_SIZE_64KB	0x00010000

struct NS_PA_INFO {
	void *pCertificate;
	void *pParameters;
	void *pResults;
};

/*
 * AFY: I would like to remove the L0 buffer altogether:
 * - you can use the L1 shared buffer to pass the RPC parameters and results:
 *    I think these easily fit in 256 bytes and you can use the area at
 *    offset 0x2C0-0x3BF in the L1 shared buffer
 */
struct SCHANNEL_L0_BUFFER_INPUT {
	u32 nL1Command;
	u8  sReserved[4092];
};

struct SCHANNEL_L0_BUFFER_SMC_INIT_INPUT {
	u32 nL1Command;
	u32 nL1SharedBufferLength;
	u32 nL1SharedBufferPhysAddr;
	u32 nL0SharedBufferPhysAddr;
	u32 nBackingStoreAddr;
	u32 nBackExtStorageAddr;
	u32 nDataAddr;
	u32 nPropertiesBufferLength;
	u8 pPropertiesBuffer[1];
};

#ifdef CONFIG_HAS_WAKELOCK
struct wake_lock g_smc_wake_lock;
#endif

/*
 * Function responsible for formatting parameters to pass from NS world to
 * S world
 *
 * AFY: add a note that this is used only for the 'administrative' commands
 * SChannel commands use schedule_secure_world, a function that does not
 * allocate nor flush the caches, etc.
 */
static u32 pub2sec_dispatcher(u32 app_id, u32 flags, u32 nargs,
	u32 arg1, u32 arg2, u32 arg3, u32 arg4)
{
	u32 nRet;
	u32 pub2sec_args[5] = {0, 0, 0, 0, 0};
#ifdef CONFIG_SMP
	cpumask_t saved_cpu_mask;
	cpumask_t local_cpu_mask = CPU_MASK_NONE;
#endif

	dprintk(KERN_INFO "pub2sec_dispatcher: "
		"app_id=0x%08x, flags=0x%08x, nargs=%u\n",
		app_id, flags, nargs);

	dprintk(KERN_INFO "pub2sec_dispatcher: "
		"args=%08x, %08x, %08x, %08x\n",
		arg1, arg2, arg3, arg4);

#ifdef CONFIG_HAS_WAKELOCK
	wake_lock(&g_smc_wake_lock);
#endif

#ifdef CONFIG_SMP
	cpu_set(0, local_cpu_mask);
	sched_getaffinity(0, &saved_cpu_mask);
	sched_setaffinity(0, &local_cpu_mask);
#endif

	/*
	 * Put L4 Secure clock domain to SW_WKUP so that modules are accessible
	 */
	SCXL4SECClockDomainEnable();

	pub2sec_args[0] = nargs;
	pub2sec_args[1] = arg1;
	pub2sec_args[2] = arg2;
	pub2sec_args[3] = arg3;
	pub2sec_args[4] = arg4;

	/* Make sure parameters are visible to the secure world */
	flush_cache_all();
	outer_flush_range(__pa(pub2sec_args),
		__pa(pub2sec_args) + 5*sizeof(u32));
	wmb();

	/* proc_id is always 0 */
	nRet = schedule_secure_world(app_id, 0, flags, __pa(pub2sec_args));

	/* Restore the HW_SUP on L4 Sec clock domain so hardware can idle */
	SCXL4SECClockDomainDisable();

#ifdef CONFIG_SMP
	sched_setaffinity(0, &saved_cpu_mask);
#endif

#ifdef CONFIG_HAS_WAKELOCK
	wake_unlock(&g_smc_wake_lock);
#endif

	return nRet;
}

/* Yields the Secure World */
int SCXLNXCommYield(struct SCXLNX_COMM *pComm)
{
	int ret;

	if (test_bit(SCXLNX_COMM_FLAG_L1_SHARED_ALLOCATED, &(pComm->nFlags)))
		SCXLNXCommSetCurrentTime(pComm);

	g_service_end = 0;
	/* yield to the Secure World */
	ret = pub2sec_dispatcher(SMICODEPUB_IRQ_END, /* app_id */
	   0, 0,        /* flags, nargs */
	   0, 0, 0, 0); /* arg1, arg2, arg3, arg4 */
	if (g_service_end != 0) {
		dprintk(KERN_ERR "Service End ret=%X\n", ret);
		test_and_clear_bit(SCXLNX_COMM_FLAG_PA_AVAILABLE,
			&pComm->nFlags);
		return -EFAULT;
	}

	return ret;
}

/* Initializes the SE (SDP, SRAM resize, RPC handler) */
static int SCXLNXCommSEInit(struct SCXLNX_COMM *pComm,
	u32 nSDPBackingStoreAddr, u32 nSDPBkExtStoreAddr)
{
	int nError;
	unsigned int nCRC;

	if (pComm->bSEInitialized) {
		dprintk(KERN_INFO "SCXLNXCommSEInit: SE already initialized... "
			"nothing to do\n");
		return 0;
	}

	/* Secure CRC read */
	dprintk(KERN_INFO "SCXLNXCommSEInit: Secure CRC Read...\n");

	nCRC = pub2sec_dispatcher(API_HAL_KM_GETSECUREROMCODECRC_INDEX, 0, 0,
		0, 0, 0, 0);
	printk(KERN_INFO "SMC: SecureCRC=0x%08X\n", nCRC);

	/*
	 * Flush caches before resize, just to be sure there is no
	 * pending public data writes back to SRAM that could trigger a
	 * security violation once their address space is marked as
	 * secure.
	 */
#define OMAP4_SRAM_PA   0x40300000
#define OMAP4_SRAM_SIZE 0xe000
	flush_cache_all();
	outer_flush_range(OMAP4_SRAM_PA,
			OMAP4_SRAM_PA + OMAP4_SRAM_SIZE);
	wmb();

	/* SRAM resize */
	dprintk(KERN_INFO "SCXLNXCommSEInit: SRAM resize (52KB)...\n");
	nError = pub2sec_dispatcher(API_HAL_SEC_L3_RAM_RESIZE_INDEX,
		FLAG_FIQ_ENABLE | FLAG_START_HAL_CRITICAL, 1,
		SEC_RAM_SIZE_52KB, 0, 0, 0);

	if (nError == API_HAL_RET_VALUE_OK) {
		dprintk(KERN_INFO "SCXLNXCommSEInit: SRAM resize OK\n");
	} else {
		dprintk(KERN_ERR "SCXLNXCommSEInit: "
			"SRAM resize failed [0x%x]\n", nError);
		goto error;
	}

	/* SDP init */
	dprintk(KERN_INFO "SCXLNXCommSEInit: SDP runtime init..."
		"(nSDPBackingStoreAddr=%x, nSDPBkExtStoreAddr=%x)\n",
		nSDPBackingStoreAddr, nSDPBkExtStoreAddr);
	nError = pub2sec_dispatcher(API_HAL_SDP_RUNTIMEINIT_INDEX,
		FLAG_FIQ_ENABLE | FLAG_START_HAL_CRITICAL, 2,
		nSDPBackingStoreAddr, nSDPBkExtStoreAddr, 0, 0);

	if (nError == API_HAL_RET_VALUE_OK) {
		dprintk(KERN_INFO "SCXLNXCommSEInit: SDP runtime init OK\n");
	} else {
		dprintk(KERN_ERR "SCXLNXCommSEInit: "
			"SDP runtime init failed [0x%x]\n", nError);
		goto error;
	}

	/* RPC init */
	dprintk(KERN_INFO "SCXLNXCommSEInit: RPC init...\n");
	nError = pub2sec_dispatcher(API_HAL_TASK_MGR_RPCINIT_INDEX,
		FLAG_START_HAL_CRITICAL, 1,
		(u32) (u32(*const) (u32, u32, u32, u32)) &rpc_handler, 0, 0, 0);

	if (nError == API_HAL_RET_VALUE_OK) {
		dprintk(KERN_INFO "SCXLNXCommSEInit: RPC init OK\n");
	} else {
		dprintk(KERN_ERR "SCXLNXCommSEInit: "
			"RPC init failed [0x%x]\n", nError);
		goto error;
	}

	pComm->bSEInitialized = true;

	return 0;

error:
	return -EFAULT;
}

/* Free the PA buffers and check protocol version returned by the PA */
static u32 SCXLNXCommRPCInit(struct SCXLNX_COMM *pComm)
{
	u32 nProtocolVersion;
	u32 nRPCError = RPC_SUCCESS;
	void *pPABuffer;
	void *pPABufferRaw;
	void *pPAInfo;
	void *pPAInfoRaw;

	dprintk(KERN_INFO "SCXLNXSMCommRPCInit(%p)\n", pComm);

	spin_lock(&(pComm->lock));

	pPABuffer = pComm->pPABuffer;
	pPABufferRaw = pComm->pPABufferRaw;
	pPAInfo = pComm->pPAInfo;
	pPAInfoRaw = pComm->pPAInfoRaw;

	if (pComm->pPABuffer != NULL) {
		pComm->pPABuffer = NULL;
		pComm->pPABufferRaw = NULL;
	}

	if (pComm->pPAInfo != NULL) {
		pComm->pPAInfo = NULL;
		pComm->pPAInfoRaw = NULL;
	}

	nProtocolVersion = *((u32 *) (pComm->pL0SharedBuffer));

	if ((GET_PROTOCOL_MAJOR_VERSION(nProtocolVersion))
			!= SCX_S_PROTOCOL_MAJOR_VERSION) {
		dprintk(KERN_ERR "SMC: Unsupported SMC Protocol PA Major "
			"Version (0x%02x, expected 0x%02x)!\n",
			GET_PROTOCOL_MAJOR_VERSION(nProtocolVersion),
			SCX_S_PROTOCOL_MAJOR_VERSION);
		nRPCError = RPC_ERROR_CONNECTION_PROTOCOL;
	} else {
		nRPCError = RPC_SUCCESS;
	}

	spin_unlock(&(pComm->lock));

	if (pPABuffer != NULL) {
		internal_vunmap(pPABuffer);
		internal_kfree(pPABufferRaw);
	}

	if (pPAInfo != NULL) {
		internal_vunmap(pPAInfo);
		internal_kfree(pPAInfoRaw);
	}

	return nRPCError;
}

/*
 * Handles RPC calls
 *
 * Returns:
 *  - RPC_NO if there was no RPC to execute
 *  - RPC_YIELD if there was a Yield RPC
 *  - RPC_NON_YIELD if there was a non-Yield RPC
 */

int SCXLNXCommExecuteRPCCommand(struct SCXLNX_COMM *pComm)
{
	u32 nRPCCommand;
	u32 nRPCId;
	u32 nRPCReturn = RPC_NO;

	/* Lock the RPC */
	mutex_lock(&(pComm->sRPCLock));

	nRPCCommand = g_RPC_parameters[1];
	nRPCId = g_RPC_parameters[0];

	if (atomic_read(&g_RPC_advancement) == RPC_ADVANCEMENT_PENDING) {
		dprintk(KERN_INFO "SCXLNXCommExecuteRPCCommand: "
			"Executing RPC ID=0x%x, CMD=0x%x\n", nRPCId,
			nRPCCommand);

		switch (nRPCCommand) {
		case RPC_CMD_YIELD:
			dprintk(KERN_INFO "SCXLNXCommExecuteRPCCommand: "
				"RPC_CMD_YIELD\n");

			set_bit(SCXLNX_COMM_FLAG_L1_SHARED_ALLOCATED,
				&(pComm->nFlags));
			wake_up(&(pComm->waitQueue));
			nRPCReturn = RPC_YIELD;
			g_RPC_parameters[0] = RPC_SUCCESS;
			break;

		case RPC_CMD_INIT:
			dprintk(KERN_INFO "SCXLNXCommExecuteRPCCommand: "
				"RPC_CMD_INIT\n");

			g_RPC_parameters[0] = SCXLNXCommRPCInit(pComm);
			nRPCReturn = RPC_NON_YIELD;
			break;

		case RPC_CMD_TRACE:
			dprintk(KERN_INFO "SCXLNXCommExecuteRPCCommand: "
				"RPC_CMD_TRACE\n");

#ifdef CONFIG_SECURE_TRACE
			spin_lock(&(pComm->lock));
			printk(KERN_INFO "SMC PA: %s",
				&(((struct SCHANNEL_L0_BUFFER_INPUT *)
					pComm->pL0SharedBuffer)->
						sReserved[RPC_TRACE_OFFSET]));
			spin_unlock(&(pComm->lock));
#endif
			nRPCReturn = RPC_NON_YIELD;
			g_RPC_parameters[0] = RPC_SUCCESS;
			break;

		default:
			if (SCXPublicCryptoExecuteRPCCommand(nRPCCommand,
					pComm->pL0SharedBuffer) != 0)
				g_RPC_parameters[0] = RPC_ERROR_BAD_PARAMETERS;
			else
				g_RPC_parameters[0] = RPC_SUCCESS;
			nRPCReturn = RPC_NON_YIELD;
			break;
		}
		atomic_set(&g_RPC_advancement, RPC_ADVANCEMENT_FINISHED);
	}

	mutex_unlock(&(pComm->sRPCLock));

	dprintk(KERN_INFO "SCXLNXCommExecuteRPCCommand: Return 0x%x\n",
		nRPCReturn);

	return nRPCReturn;
}

/*--------------------------------------------------------------------------
 * L4 SEC Clock domain handling
 *-------------------------------------------------------------------------- */

static struct clockdomain *smc_l4_sec_clkdm;
static atomic_t smc_l4_sec_clkdm_use_count = ATOMIC_INIT(0);

void SCXL4SECClockDomainEnable(void)
{
	atomic_inc(&smc_l4_sec_clkdm_use_count);
	omap2_clkdm_wakeup(smc_l4_sec_clkdm);
}

void SCXL4SECClockDomainDisable(void)
{
	if (atomic_dec_return(&smc_l4_sec_clkdm_use_count) == 0)
		omap2_clkdm_allow_idle(smc_l4_sec_clkdm);
}

/*--------------------------------------------------------------------------
 * Power management
 *-------------------------------------------------------------------------- */

int SCXLNXCommHibernate(struct SCXLNX_COMM *pComm)
{
	struct SCXLNX_DEVICE *pDevice = SCXLNXGetDevice();

	/*
	 * As we enter in CORE OFF, the keys are going to be cleared.
	 * Reset the global key context.
	 * When the system leaves CORE OFF, this will force the driver to go
	 * through the secure world which will reconfigure the accelerator as
	 * public.
	 */
	pDevice->hAES1SecureKeyContext = 0;
	pDevice->hAES2SecureKeyContext = 0;
	pDevice->hDESSecureKeyContext = 0;
	pDevice->bSHAM1IsPublic = false;

	return 0;
}


int SCXLNXCommResume(struct SCXLNX_COMM *pComm)
{
	return 0;
}

/*--------------------------------------------------------------------------
 * Initialization
 *-------------------------------------------------------------------------- */

int SCXLNXCommInit(struct SCXLNX_COMM *pComm)
{
	spin_lock_init(&(pComm->lock));
	pComm->nFlags = 0;
	pComm->pBuffer = NULL;
	pComm->pL0SharedBuffer = NULL;
	pComm->pPAInfo = NULL;
	pComm->pPABuffer = NULL;

	pComm->pBufferRaw = NULL;
	pComm->pL0SharedBufferRaw = NULL;
	pComm->pPAInfoRaw = NULL;
	pComm->pPABufferRaw = NULL;

	pComm->bSEInitialized = false;

	init_waitqueue_head(&(pComm->waitQueue));
	mutex_init(&(pComm->sRPCLock));

	g_secure_task_id = 0;

	smc_l4_sec_clkdm = clkdm_lookup("l4_secure_clkdm");
	if (smc_l4_sec_clkdm == NULL)
		return -EFAULT;

	if (SCXPublicCryptoInit() != PUBLIC_CRYPTO_OPERATION_SUCCESS)
		return -EFAULT;

	return 0;
}

static void SCXLNXCommStop(struct SCXLNX_COMM *pComm);

/* Start the SMC PA */
int SCXLNXCommStart(struct SCXLNX_COMM *pComm, u32 nSDPBackingStoreAddr,
	u32 nSDPBkExtStoreAddr, u32 nDataAddr, u8 *pPABufferVAddr,
	u8 *pPABufferVAddrRaw, u32 nPABufferSize, u8 *pPropertiesBuffer,
	u32 nPropertiesBufferLength)
{
	struct SCHANNEL_L0_BUFFER_INPUT *pL0SharedBufferRaw = NULL;
	struct SCHANNEL_L0_BUFFER_INPUT *pL0SharedBuffer = NULL;
	struct SCHANNEL_L0_BUFFER_INPUT *pL0SharedBufferRawDescr = NULL;
	struct SCHANNEL_C1S_BUFFER *pL1SharedBuffer = NULL;
	struct SCHANNEL_C1S_BUFFER *pL1SharedBufferRaw = NULL;
	struct SCHANNEL_C1S_BUFFER *pL1SharedBufferRawDescr = NULL;
	struct SCHANNEL_L0_BUFFER_SMC_INIT_INPUT *pSMCInitInput;
	struct NS_PA_INFO *paInfo = NULL;
	struct NS_PA_INFO *paInfoRaw = NULL;
	int nError;
	u32 descr;

#ifdef CONFIG_HAS_WAKELOCK
	wake_lock_init(&g_smc_wake_lock, WAKE_LOCK_SUSPEND,
		SCXLNX_DEVICE_BASE_NAME);
#endif

	/*
	 * Implementation notes:
	 *
	 * 1/ The PA buffer (pPABufferVAddr)is now owned by this function.
	 *    In case of error, it is responsible for releasing the buffer.
	 *
	 * 2/ The PA Info and PA Buffer will be freed through a RPC call
	 *    at the beginning of the PA entry in the SE.
	 */

	if (test_bit(SCXLNX_COMM_FLAG_PA_AVAILABLE, &pComm->nFlags)) {
		dprintk(KERN_ERR "SCXLNXCommStart(%p): "
			"The SM is already started\n", pComm);

		nError = -EFAULT;
		goto error1;
	}

	if ((sizeof(struct SCHANNEL_L0_BUFFER_INPUT) != PAGE_SIZE)
			|| (sizeof(struct SCHANNEL_C1S_BUFFER) != PAGE_SIZE)) {
		dprintk(KERN_ERR "SCXLNXCommStart(%p): "
			"The L0 or L1 structure size is incorrect!\n", pComm);
		nError = -EFAULT;
		goto error1;
	}

	nError = SCXLNXCommSEInit(pComm, nSDPBackingStoreAddr,
		nSDPBkExtStoreAddr);
	if (nError != 0) {
		dprintk(KERN_ERR "SCXLNXCommStart(%p): "
			"SE initialization failed\n", pComm);

		goto error1;
	}

	paInfo = internal_kmalloc_vmap((void **) &paInfoRaw,
		sizeof(struct NS_PA_INFO), GFP_KERNEL);
	pL0SharedBuffer =
		(struct SCHANNEL_L0_BUFFER_INPUT *)
			internal_get_zeroed_page_vmap(
				(void **) &pL0SharedBufferRaw, GFP_KERNEL);
	pL1SharedBuffer =
		(struct SCHANNEL_C1S_BUFFER *) internal_get_zeroed_page_vmap(
			(void **) &pL1SharedBufferRaw, GFP_KERNEL);

	if ((paInfo == NULL) || (paInfoRaw == NULL) ||
	   (pL0SharedBuffer == NULL) || (pL0SharedBufferRaw == NULL) ||
	   (pL1SharedBuffer == NULL) || (pL1SharedBufferRaw == NULL)) {
		dprintk(KERN_ERR "SCXLNXCommStart(%p): "
			"Ouf of memory!\n", pComm);

		nError = -ENOMEM;
		goto error1;
	}

	/*
	 * Ensure the pages storing SM communication buffers are mapped
	 */
	__set_page_locked(virt_to_page(pL0SharedBufferRaw));
	__set_page_locked(virt_to_page(pL1SharedBufferRaw));

	dprintk(KERN_INFO "SCXLNXCommStart(%p): "
		"L0SharedBuffer={0x%08x, 0x%08x}\n", pComm,
		(u32) pL0SharedBuffer, (u32) __pa(pL0SharedBufferRaw));
	dprintk(KERN_INFO "SCXLNXCommStart(%p): "
		"L1SharedBuffer={0x%08x, 0x%08x}\n", pComm,
		(u32) pL1SharedBuffer, (u32) __pa(pL1SharedBufferRaw));

	descr = SCXLNXCommGetL2DescriptorCommon((u32) pL0SharedBuffer,
			current->mm);
	pL0SharedBufferRawDescr = (struct SCHANNEL_L0_BUFFER_INPUT *) (
		((u32) __pa(pL0SharedBufferRaw) & 0xFFFFF000) |
		(descr & 0xFFF));

	descr = SCXLNXCommGetL2DescriptorCommon((u32) pL1SharedBuffer,
			current->mm);
	pL1SharedBufferRawDescr = (struct SCHANNEL_C1S_BUFFER *) (
		((u32) __pa(pL1SharedBufferRaw) & 0xFFFFF000) |
		(descr & 0xFFF));

	pComm->pPAInfo = paInfo;
	pComm->pPAInfoRaw = paInfoRaw;
	pComm->pPABuffer = pPABufferVAddr;
	pComm->pPABufferRaw = pPABufferVAddrRaw;
	pComm->pL0SharedBuffer = pL0SharedBuffer;
	pComm->pL0SharedBufferRaw = pL0SharedBufferRaw;
	pComm->pBuffer = pL1SharedBuffer;
	pComm->pBufferRaw = pL1SharedBufferRaw;

	paInfo->pCertificate = (void *) __pa(pPABufferVAddrRaw);
	paInfo->pParameters = (void *) __pa(pL0SharedBufferRaw);
	paInfo->pResults = (void *) __pa(pL0SharedBufferRaw);

	memset(pL0SharedBuffer, 0, sizeof(struct SCHANNEL_L0_BUFFER_INPUT));
	memset(pL1SharedBuffer, 0, sizeof(struct SCHANNEL_C1S_BUFFER));

	pSMCInitInput =
		(struct SCHANNEL_L0_BUFFER_SMC_INIT_INPUT *) pL0SharedBuffer;

	pSMCInitInput->nL1Command = SCX_SMC_INIT;
	pSMCInitInput->nL0SharedBufferPhysAddr = (u32) pL0SharedBufferRawDescr;
	pSMCInitInput->nL1SharedBufferLength =
		sizeof(struct SCHANNEL_C1S_BUFFER);
	pSMCInitInput->nL1SharedBufferPhysAddr = (u32) pL1SharedBufferRawDescr;

	pSMCInitInput->nBackingStoreAddr = nSDPBackingStoreAddr;
	pSMCInitInput->nBackExtStorageAddr = nSDPBkExtStoreAddr;
	pSMCInitInput->nDataAddr = nDataAddr;

	pSMCInitInput->nPropertiesBufferLength = nPropertiesBufferLength;
	if (nPropertiesBufferLength == 0) {
		pSMCInitInput->pPropertiesBuffer[0] = 0;
	} else {
		/* Test for overflow */
		if ((pSMCInitInput->pPropertiesBuffer + nPropertiesBufferLength
				> pSMCInitInput->pPropertiesBuffer) &&
			(nPropertiesBufferLength <=
				pSMCInitInput->nPropertiesBufferLength)) {
				memcpy(pSMCInitInput->pPropertiesBuffer,
					pPropertiesBuffer,
					 nPropertiesBufferLength);
		} else {
			printk(KERN_INFO "SCXLNXCommStart(%p): "
				"Configuration buffer size from userland is "
				"incorrect(%d, %d)\n",
				pComm, (u32) nPropertiesBufferLength,
				pSMCInitInput->nPropertiesBufferLength);
			nError = -EFAULT;
			goto error1;
		}
	}

	dprintk(KERN_INFO "SCXLNXSMCommStart(%p): "
		"System Configuration (%d bytes)\n", pComm,
		pSMCInitInput->nPropertiesBufferLength);
	dprintk(KERN_INFO "SCXLNXSMCommStart(%p): "
		"Starting PA (%d bytes)...\n", pComm, nPABufferSize);

	/*
	 * Set the OS current time in the L1 shared buffer first. The secure
	 * world uses it as itw boot reference time.
	 */
	SCXLNXCommSetCurrentTime(pComm);

	/*
	 * Make sure all data is visible to the secure world
	 */
	flush_cache_all();
	outer_flush_range(__pa(pL0SharedBufferRaw),
		__pa(pL0SharedBufferRaw) + PAGE_SIZE);
	outer_flush_range(__pa(pL1SharedBufferRaw),
		__pa(pL1SharedBufferRaw) + PAGE_SIZE);
	outer_flush_range(__pa(pPABufferVAddrRaw),
		__pa(pPABufferVAddrRaw) + nPABufferSize);
	outer_flush_range(__pa(paInfoRaw),
		__pa(paInfoRaw) + sizeof(struct NS_PA_INFO));
	wmb();

	/*
	 * Start the SMC PA
	 */
	nError = pub2sec_dispatcher(API_HAL_LM_PALOAD_INDEX,
		FLAG_IRQ_ENABLE | FLAG_FIQ_ENABLE | FLAG_START_HAL_CRITICAL, 1,
		__pa(pComm->pPAInfoRaw), 0, 0, 0);
	if (nError != API_HAL_RET_VALUE_OK) {
		printk(KERN_ERR "SMC: PA load failed [0x%x]\n", nError);
		test_and_clear_bit(SCXLNX_COMM_FLAG_PA_AVAILABLE,
			&pComm->nFlags);
		goto error2;
	}

	set_bit(SCXLNX_COMM_FLAG_PA_AVAILABLE, &pComm->nFlags);

	nError = SCXLNXCommSendReceive(pComm, NULL, NULL, NULL, false);
	if (nError != 0) {
		printk(KERN_ERR "SMC: Error while loading the PA [0x%x]\n",
			nError);
		test_and_clear_bit(SCXLNX_COMM_FLAG_PA_AVAILABLE,
			&pComm->nFlags);
		goto error2;
	}

	return 0;

error1:
	internal_vunmap(pPABufferVAddr);
	internal_vunmap(paInfo);
	internal_kfree(pPABufferVAddrRaw);
	internal_kfree(paInfoRaw);
	if (pL0SharedBuffer != NULL) {
		__clear_page_locked(virt_to_page(pL0SharedBufferRaw));
		internal_free_page_vunmap((unsigned long) pL0SharedBuffer);
		internal_free_page((unsigned long) pL0SharedBufferRaw);
	}
	if (pL1SharedBuffer != NULL) {
		__clear_page_locked(virt_to_page(pL1SharedBufferRaw));
		internal_free_page_vunmap((unsigned long) pL1SharedBuffer);
		internal_free_page((unsigned long) pL1SharedBufferRaw);
	}

	return nError;

error2:
	dprintk(KERN_ERR "SCXLNXCommStart(%p): Failure [%d]\n",
		pComm, nError);

	SCXLNXCommStop(pComm);

	return nError;
}

static void SCXLNXCommStop(struct SCXLNX_COMM *pComm)
{
	int nError;
	void *pBufferRaw;
	void *pBuffer;
	void *pL0SharedBufferRaw;
	void *pL0SharedBuffer;
	void *pPABuffer;
	void *pPABufferRaw;
	void *pPAInfo;
	void *pPAInfoRaw;

	dprintk(KERN_INFO "SCXLNXCommStop(%p)\n", pComm);

	nError = pub2sec_dispatcher(API_HAL_LM_PAUNLOADALL_INDEX,
		FLAG_START_HAL_CRITICAL, 0, 0, 0, 0, 0);

	if (nError != API_HAL_RET_VALUE_OK)
		dprintk(KERN_ERR "SCXLNXCommStop(%p): "
			"PA unload failed [0x%x]\n", pComm, nError);

	spin_lock(&(pComm->lock));

	pBufferRaw = pComm->pBufferRaw;
	pBuffer = pComm->pBuffer;
	pL0SharedBufferRaw = pComm->pL0SharedBufferRaw;
	pL0SharedBuffer = pComm->pL0SharedBuffer;
	pPABuffer = pComm->pPABuffer;
	pPABufferRaw = pComm->pPABufferRaw;
	pPAInfo = pComm->pPAInfo;
	pPAInfoRaw = pComm->pPAInfoRaw;

	if (pComm->pBuffer != NULL) {
		pComm->pBuffer = NULL;
		pComm->pBufferRaw = NULL;
	}

	if (pComm->pL0SharedBuffer != NULL) {
		pComm->pL0SharedBuffer = NULL;
		pComm->pL0SharedBufferRaw = NULL;
	}

	if (pComm->pPABuffer != NULL) {
		pComm->pPABuffer = NULL;
		pComm->pPABufferRaw = NULL;
	}

	if (pComm->pPAInfo != NULL) {
		pComm->pPAInfo = NULL;
		pComm->pPAInfoRaw = NULL;
	}

	spin_unlock(&(pComm->lock));

	if (pBuffer != NULL) {
		__clear_page_locked(virt_to_page(pBufferRaw));
		internal_free_page_vunmap((unsigned long) pBuffer);
	}

	if (pL0SharedBuffer != NULL) {
		__clear_page_locked(virt_to_page(pL0SharedBufferRaw));
		internal_free_page_vunmap((unsigned long) pL0SharedBuffer);
		internal_free_page((unsigned long) pL0SharedBufferRaw);
	}

	if (pPABuffer != NULL) {
		internal_vunmap(pPABuffer);
		internal_kfree(pPABufferRaw);
	}

	if (pPAInfo != NULL) {
		internal_vunmap(pPAInfo);
		internal_kfree(pPAInfoRaw);
	}

#ifdef CONFIG_HAS_WAKELOCK
	wake_lock_destroy(&g_smc_wake_lock);
#endif
}

void SCXLNXCommTerminate(struct SCXLNX_COMM *pComm)
{
	dprintk(KERN_INFO "SCXLNXCommTerminate(%p)\n", pComm);

	spin_lock(&(pComm->lock));

	SCXPublicCryptoTerminate();

	spin_unlock(&(pComm->lock));
}
