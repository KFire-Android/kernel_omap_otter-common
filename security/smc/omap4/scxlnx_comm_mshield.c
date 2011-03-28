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
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/page-flags.h>
#include <linux/pagemap.h>
#include <linux/vmalloc.h>
#include <linux/version.h>
#include <linux/jiffies.h>
#include <linux/dma-mapping.h>
#include <linux/cpu.h>

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

u32 g_RPC_advancement;
u32 g_RPC_parameters[4] = {0, 0, 0, 0};
u32 g_secure_task_id;
u32 g_service_end;

/*
 * Secure ROMCode HAL API Identifiers
 */
#define API_HAL_SDP_RUNTIMEINIT_INDEX           0x04
#define API_HAL_LM_PALOAD_INDEX                 0x05
#define API_HAL_LM_PAUNLOADALL_INDEX            0x07
#define API_HAL_TASK_MGR_RPCINIT_INDEX          0x08
#define API_HAL_KM_GETSECUREROMCODECRC_INDEX    0x0B
#define API_HAL_SEC_L3_RAM_RESIZE_INDEX         0x17

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
struct SCHANNEL_INIT_BUFFER {
	u32 nInitStatus;
	u32 nProtocolVersion;
	u32 nL1SharedBufferPhysAddr;
	u32 nBackingStoreAddr;
	u32 nBackExtStorageAddr;
	u32 nWorkspaceAddr;
	u32 nWorkspaceSize;
	u32 nPropertiesBufferLength;
	u8 pPropertiesBuffer[1];
};

#ifdef CONFIG_HAS_WAKELOCK
static struct wake_lock g_tf_wake_lock;
static atomic_t tf_wake_lock_count = ATOMIC_INIT(0);
#endif

static struct clockdomain *smc_l4_sec_clkdm;
static atomic_t smc_l4_sec_clkdm_use_count = ATOMIC_INIT(0);

static int __init tf_early_init(void)
{
	g_secure_task_id = 0;

	dprintk(KERN_INFO "SMC early init\n");

	smc_l4_sec_clkdm = clkdm_lookup("l4_secure_clkdm");
	if (smc_l4_sec_clkdm == NULL)
		return -EFAULT;

#ifdef CONFIG_HAS_WAKELOCK
	wake_lock_init(&g_tf_wake_lock, WAKE_LOCK_SUSPEND,
		SCXLNX_DEVICE_BASE_NAME);
#endif

	return 0;
}
early_initcall(tf_early_init);

/*
 * Function responsible for formatting parameters to pass from NS world to
 * S world
 */
u32 omap4_secure_dispatcher(u32 app_id, u32 flags, u32 nargs,
	u32 arg1, u32 arg2, u32 arg3, u32 arg4)
{
	u32 ret;
	unsigned long nITFlags;
	u32 pub2sec_args[5] = {0, 0, 0, 0, 0};

	/*dprintk(KERN_INFO "omap4_secure_dispatcher: "
		"app_id=0x%08x, flags=0x%08x, nargs=%u\n",
		app_id, flags, nargs);*/

	/*if (nargs != 0)
		dprintk(KERN_INFO
		"omap4_secure_dispatcher: args=%08x, %08x, %08x, %08x\n",
		arg1, arg2, arg3, arg4);*/

	pub2sec_args[0] = nargs;
	pub2sec_args[1] = arg1;
	pub2sec_args[2] = arg2;
	pub2sec_args[3] = arg3;
	pub2sec_args[4] = arg4;

	/* Make sure parameters are visible to the secure world */
	dmac_clean_range((void *)pub2sec_args,
		(void *)(((u32)(pub2sec_args)) + 5*sizeof(u32)));
	outer_clean_range(__pa(pub2sec_args),
		__pa(pub2sec_args) + 5*sizeof(u32));
	wmb();

	/*
	 * Put L4 Secure clock domain to SW_WKUP so that modules are accessible
	 */
	tf_l4sec_clkdm_wakeup(true, false);

	local_irq_save(nITFlags);
#ifdef DEBUG
	BUG_ON((read_mpidr() & 0x00000003) != 0);
#endif
	/* proc_id is always 0 */
	ret = schedule_secure_world(app_id, 0, flags, __pa(pub2sec_args));
	local_irq_restore(nITFlags);

	/* Restore the HW_SUP on L4 Sec clock domain so hardware can idle */
	tf_l4sec_clkdm_allow_idle(true, false);

	/*dprintk(KERN_INFO "omap4_secure_dispatcher()\n");*/

	return ret;
}

/* Yields the Secure World */
int tf_schedule_secure_world(struct SCXLNX_COMM *pComm, bool prepare_exit)
{
	int status = 0;
	int ret;
	unsigned long nITFlags;
	u32 appli_id;

	SCXLNXCommSetCurrentTime(pComm);

	local_irq_save(nITFlags);

	switch (g_RPC_advancement) {
	case  RPC_ADVANCEMENT_NONE:
		/* Return from IRQ */
		appli_id = SMICODEPUB_IRQ_END;
		if (prepare_exit)
			status = STATUS_PENDING;
		break;
	case  RPC_ADVANCEMENT_PENDING:
		/* nothing to do in this case */
		goto exit;
	default:
	case RPC_ADVANCEMENT_FINISHED:
		if (prepare_exit)
			goto exit;
		appli_id = SMICODEPUB_RPC_END;
		g_RPC_advancement = RPC_ADVANCEMENT_NONE;
		break;
	}

	g_service_end = 1;
	/* yield to the Secure World */
	ret = omap4_secure_dispatcher(appli_id, /* app_id */
	   0, 0,        /* flags, nargs */
	   0, 0, 0, 0); /* arg1, arg2, arg3, arg4 */
	if (g_service_end != 0) {
		dprintk(KERN_ERR "Service End ret=%X\n", ret);

		if (ret == 0) {
			dmac_inv_range((void *)pComm->pInitSharedBuffer,
				(void *)(((u32)(pComm->pInitSharedBuffer)) +
					PAGE_SIZE));
			outer_inv_range(__pa(pComm->pInitSharedBuffer),
				__pa(pComm->pInitSharedBuffer) +
				PAGE_SIZE);

			ret = ((struct SCHANNEL_INIT_BUFFER *)
				(pComm->pInitSharedBuffer))->nInitStatus;

			dprintk(KERN_ERR "SMC PA failure ret=%X\n", ret);
			if (ret == 0)
				ret = -EFAULT;
		}
		clear_bit(SCXLNX_COMM_FLAG_PA_AVAILABLE, &pComm->nFlags);
		omap4_secure_dispatcher(API_HAL_LM_PAUNLOADALL_INDEX,
			FLAG_START_HAL_CRITICAL, 0, 0, 0, 0, 0);
		status = ret;
	}

exit:
	local_irq_restore(nITFlags);

	return status;
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

	nCRC = omap4_secure_dispatcher(API_HAL_KM_GETSECUREROMCODECRC_INDEX,
		0, 0, 0, 0, 0, 0);
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
	nError = omap4_secure_dispatcher(API_HAL_SEC_L3_RAM_RESIZE_INDEX,
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
	nError = omap4_secure_dispatcher(API_HAL_SDP_RUNTIMEINIT_INDEX,
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
	nError = omap4_secure_dispatcher(API_HAL_TASK_MGR_RPCINIT_INDEX,
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

/* Check protocol version returned by the PA */
static u32 tf_rpc_init(struct SCXLNX_COMM *pComm)
{
	u32 nProtocolVersion;
	u32 nRPCError = RPC_SUCCESS;

	dprintk(KERN_INFO "SCXLNXSMCommRPCInit(%p)\n", pComm);

	spin_lock(&(pComm->lock));

	dmac_inv_range((void *)pComm->pInitSharedBuffer,
		(void *)(((u32)(pComm->pInitSharedBuffer)) + PAGE_SIZE));
	outer_inv_range(__pa(pComm->pInitSharedBuffer),
		__pa(pComm->pInitSharedBuffer) +  PAGE_SIZE);

	nProtocolVersion = ((struct SCHANNEL_INIT_BUFFER *)
				(pComm->pInitSharedBuffer))->nProtocolVersion;

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

	register_smc_public_crypto_digest();
	register_smc_public_crypto_aes();

	return nRPCError;
}

static u32 tf_rpc_trace(struct SCXLNX_COMM *pComm)
{
	dprintk(KERN_INFO "tf_rpc_trace(%p)\n", pComm);

#ifdef CONFIG_SECURE_TRACE
	spin_lock(&(pComm->lock));
	printk(KERN_INFO "SMC PA: %s",
		pComm->pBuffer->sRPCTraceBuffer);
	spin_unlock(&(pComm->lock));
#endif
	return RPC_SUCCESS;
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
	u32 nRPCReturn = RPC_NO;

#ifdef DEBUG
	BUG_ON((read_mpidr() & 0x00000003) != 0);
#endif

	/* Lock the RPC */
	mutex_lock(&(pComm->sRPCLock));

	nRPCCommand = g_RPC_parameters[1];

	if (g_RPC_advancement == RPC_ADVANCEMENT_PENDING) {
		dprintk(KERN_INFO "SCXLNXCommExecuteRPCCommand: "
			"Executing CMD=0x%x\n",
			g_RPC_parameters[1]);

		switch (nRPCCommand) {
		case RPC_CMD_YIELD:
			dprintk(KERN_INFO "SCXLNXCommExecuteRPCCommand: "
				"RPC_CMD_YIELD\n");

			nRPCReturn = RPC_YIELD;
			g_RPC_parameters[0] = RPC_SUCCESS;
			break;

		case RPC_CMD_TRACE:
			nRPCReturn = RPC_NON_YIELD;
			g_RPC_parameters[0] = tf_rpc_trace(pComm);;
			break;

		default:
			if (SCXPublicCryptoExecuteRPCCommand(nRPCCommand,
				pComm->pBuffer->sRPCShortcutBuffer) != 0)
				g_RPC_parameters[0] = RPC_ERROR_BAD_PARAMETERS;
			else
				g_RPC_parameters[0] = RPC_SUCCESS;
			nRPCReturn = RPC_NON_YIELD;
			break;
		}
		g_RPC_advancement = RPC_ADVANCEMENT_FINISHED;
	}

	mutex_unlock(&(pComm->sRPCLock));

	dprintk(KERN_INFO "SCXLNXCommExecuteRPCCommand: Return 0x%x\n",
		nRPCReturn);

	return nRPCReturn;
}

/*--------------------------------------------------------------------------
 * L4 SEC Clock domain handling
 *-------------------------------------------------------------------------- */

void tf_l4sec_clkdm_wakeup(bool use_spin_lock, bool wakelock)
{
	if (use_spin_lock)
		spin_lock(&SCXLNXGetDevice()->sm.lock);
#ifdef CONFIG_HAS_WAKELOCK
	if (wakelock) {
		atomic_inc(&tf_wake_lock_count);
		wake_lock(&g_tf_wake_lock);
	}
#endif
	atomic_inc(&smc_l4_sec_clkdm_use_count);
	omap2_clkdm_wakeup(smc_l4_sec_clkdm);
	if (use_spin_lock)
		spin_unlock(&SCXLNXGetDevice()->sm.lock);
}

void tf_l4sec_clkdm_allow_idle(bool use_spin_lock, bool wakeunlock)
{
	if (use_spin_lock)
		spin_lock(&SCXLNXGetDevice()->sm.lock);
	if (atomic_dec_return(&smc_l4_sec_clkdm_use_count) == 0)
		omap2_clkdm_allow_idle(smc_l4_sec_clkdm);
#ifdef CONFIG_HAS_WAKELOCK
	if (wakeunlock)
		if (atomic_dec_return(&tf_wake_lock_count) == 0)
			wake_unlock(&g_tf_wake_lock);
#endif
	if (use_spin_lock)
		spin_unlock(&SCXLNXGetDevice()->sm.lock);
}

/*--------------------------------------------------------------------------
 * Power management
 *-------------------------------------------------------------------------- */

int SCXLNXCommHibernate(struct SCXLNX_COMM *pComm)
{
	struct SCXLNX_DEVICE *pDevice = SCXLNXGetDevice();

	dprintk(KERN_INFO "SCXLNXCommHibernate()\n");

	/*
	 * As we enter in CORE OFF, the keys are going to be cleared.
	 * Reset the global key context.
	 * When the system leaves CORE OFF, this will force the driver to go
	 * through the secure world which will reconfigure the accelerators.
	 */
	pDevice->hAES1SecureKeyContext = 0;
	pDevice->hAES2SecureKeyContext = 0;
	pDevice->hDESSecureKeyContext = 0;
#ifndef CONFIG_SMC_KERNEL_CRYPTO
	pDevice->bSHAM1IsPublic = false;
#endif
	return 0;
}


int SCXLNXCommResume(struct SCXLNX_COMM *pComm)
{
#ifdef CONFIG_SMC_KERNEL_CRYPTO
	union SCX_COMMAND_MESSAGE message;
	union SCX_ANSWER_MESSAGE answer;
	int ret;
#endif

	dprintk(KERN_INFO "SCXLNXCommResume()\n");
	#if 0
	{
		void *workspace_va;
		struct SCXLNX_DEVICE *pDevice = SCXLNXGetDevice();
		workspace_va = ioremap(pDevice->nWorkspaceAddr,
			pDevice->nWorkspaceSize);
		printk(KERN_INFO
		"Read first word of workspace [0x%x]\n",
		*(uint32_t *)workspace_va);
	}
	#endif

#ifdef CONFIG_SMC_KERNEL_CRYPTO
	/*
	 * When the system leaves CORE OFF, HWA are configured as secure.  We
	 * need them as public for the Linux Crypto API.
	 */
	memset(&message, 0, sizeof(message));

	message.sHeader.nMessageType = SCX_MESSAGE_TYPE_MANAGEMENT;
	message.sHeader.nMessageSize =
		(sizeof(struct SCX_COMMAND_MANAGEMENT) -
			sizeof(struct SCX_COMMAND_HEADER))/sizeof(u32);
	message.sManagementMessage.nCommand =
		SCX_MANAGEMENT_RESUME_FROM_CORE_OFF;

	ret = SCXLNXCommSendReceive(pComm, &message, &answer, NULL, false);
	if (ret) {
		dprintk(KERN_ERR "SCXLNXCommResume(%p): "
			"SCXLNXCommSendReceive failed (error %d)!\n",
			pComm, ret);

		unregister_smc_public_crypto_digest();
		unregister_smc_public_crypto_aes();
		return ret;
	}

	if (answer.sHeader.nErrorCode) {
		unregister_smc_public_crypto_digest();
		unregister_smc_public_crypto_aes();
	}

	return answer.sHeader.nErrorCode;
#else
	return 0;
#endif
}

/*--------------------------------------------------------------------------
 * Initialization
 *-------------------------------------------------------------------------- */

int SCXLNXCommInit(struct SCXLNX_COMM *pComm)
{
	spin_lock_init(&(pComm->lock));
	pComm->nFlags = 0;
	pComm->pBuffer = NULL;
	pComm->pInitSharedBuffer = NULL;

	pComm->bSEInitialized = false;

	init_waitqueue_head(&(pComm->waitQueue));
	mutex_init(&(pComm->sRPCLock));

	if (SCXPublicCryptoInit() != PUBLIC_CRYPTO_OPERATION_SUCCESS)
		return -EFAULT;

	return 0;
}

/* Start the SMC PA */
int SCXLNXCommStart(struct SCXLNX_COMM *pComm,
	u32 nWorkspaceAddr, u32 nWorkspaceSize,
	u8 *pPABufferVAddr, u32 nPABufferSize,
	u8 *pPropertiesBuffer, u32 nPropertiesBufferLength)
{
	struct SCHANNEL_INIT_BUFFER *pInitSharedBuffer = NULL;
	struct SCHANNEL_C1S_BUFFER *pL1SharedBuffer = NULL;
	struct SCHANNEL_C1S_BUFFER *pL1SharedBufferDescr = NULL;
	struct NS_PA_INFO sPAInfo;
	int ret;
	u32 descr;
	u32 nSDPBackingStoreAddr;
	u32 nSDPBkExtStoreAddr;
#ifdef CONFIG_SMP
	long ret_affinity;
	cpumask_t saved_cpu_mask;
	cpumask_t local_cpu_mask = CPU_MASK_NONE;

	cpu_set(0, local_cpu_mask);
	sched_getaffinity(0, &saved_cpu_mask);
	ret_affinity = sched_setaffinity(0, &local_cpu_mask);
	if (ret_affinity != 0)
		dprintk(KERN_ERR "sched_setaffinity #1 -> 0x%lX", ret_affinity);
#endif

	tf_l4sec_clkdm_wakeup(true, true);

	nWorkspaceSize -= SZ_1M;
	nSDPBackingStoreAddr = nWorkspaceAddr + nWorkspaceSize;
	nWorkspaceSize -= 0x20000;
	nSDPBkExtStoreAddr = nWorkspaceAddr + nWorkspaceSize;

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
			"The SMC PA is already started\n", pComm);

		ret = -EFAULT;
		goto error1;
	}

	if (sizeof(struct SCHANNEL_C1S_BUFFER) != PAGE_SIZE) {
		dprintk(KERN_ERR "SCXLNXCommStart(%p): "
			"The L1 structure size is incorrect!\n", pComm);
		ret = -EFAULT;
		goto error1;
	}

	ret = SCXLNXCommSEInit(pComm, nSDPBackingStoreAddr,
		nSDPBkExtStoreAddr);
	if (ret != 0) {
		dprintk(KERN_ERR "SCXLNXCommStart(%p): "
			"SE initialization failed\n", pComm);
		goto error1;
	}

	pInitSharedBuffer =
		(struct SCHANNEL_INIT_BUFFER *)
			internal_get_zeroed_page(GFP_KERNEL);
	if (pInitSharedBuffer == NULL) {
		dprintk(KERN_ERR "SCXLNXCommStart(%p): "
			"Ouf of memory!\n", pComm);

		ret = -ENOMEM;
		goto error1;
	}
	/* Ensure the page is mapped */
	__set_page_locked(virt_to_page(pInitSharedBuffer));

	pL1SharedBuffer =
		(struct SCHANNEL_C1S_BUFFER *)
			internal_get_zeroed_page(GFP_KERNEL);

	if (pL1SharedBuffer == NULL) {
		dprintk(KERN_ERR "SCXLNXCommStart(%p): "
			"Ouf of memory!\n", pComm);

		ret = -ENOMEM;
		goto error1;
	}
	/* Ensure the page is mapped */
	__set_page_locked(virt_to_page(pL1SharedBuffer));

	dprintk(KERN_INFO "SCXLNXCommStart(%p): "
		"L0SharedBuffer={0x%08x, 0x%08x}\n", pComm,
		(u32) pInitSharedBuffer, (u32) __pa(pInitSharedBuffer));
	dprintk(KERN_INFO "SCXLNXCommStart(%p): "
		"L1SharedBuffer={0x%08x, 0x%08x}\n", pComm,
		(u32) pL1SharedBuffer, (u32) __pa(pL1SharedBuffer));

	descr = SCXLNXCommGetL2DescriptorCommon((u32) pL1SharedBuffer,
			current->mm);
	pL1SharedBufferDescr = (struct SCHANNEL_C1S_BUFFER *) (
		((u32) __pa(pL1SharedBuffer) & 0xFFFFF000) |
		(descr & 0xFFF));

	sPAInfo.pCertificate = (void *) __pa(pPABufferVAddr);
	sPAInfo.pParameters = (void *) __pa(pInitSharedBuffer);
	sPAInfo.pResults = (void *) __pa(pInitSharedBuffer);

	pInitSharedBuffer->nL1SharedBufferPhysAddr = (u32) pL1SharedBufferDescr;

	pInitSharedBuffer->nBackingStoreAddr = nSDPBackingStoreAddr;
	pInitSharedBuffer->nBackExtStorageAddr = nSDPBkExtStoreAddr;
	pInitSharedBuffer->nWorkspaceAddr = nWorkspaceAddr;
	pInitSharedBuffer->nWorkspaceSize = nWorkspaceSize;

	pInitSharedBuffer->nPropertiesBufferLength = nPropertiesBufferLength;
	if (nPropertiesBufferLength == 0) {
		pInitSharedBuffer->pPropertiesBuffer[0] = 0;
	} else {
		/* Test for overflow */
		if ((pInitSharedBuffer->pPropertiesBuffer +
			nPropertiesBufferLength
				> pInitSharedBuffer->pPropertiesBuffer) &&
			(nPropertiesBufferLength <=
				pInitSharedBuffer->nPropertiesBufferLength)) {
				memcpy(pInitSharedBuffer->pPropertiesBuffer,
					pPropertiesBuffer,
					 nPropertiesBufferLength);
		} else {
			dprintk(KERN_INFO "SCXLNXCommStart(%p): "
				"Configuration buffer size from userland is "
				"incorrect(%d, %d)\n",
				pComm, (u32) nPropertiesBufferLength,
				pInitSharedBuffer->nPropertiesBufferLength);
			ret = -EFAULT;
			goto error1;
		}
	}

	dprintk(KERN_INFO "SCXLNXSMCommStart(%p): "
		"System Configuration (%d bytes)\n", pComm,
		pInitSharedBuffer->nPropertiesBufferLength);
	dprintk(KERN_INFO "SCXLNXSMCommStart(%p): "
		"Starting PA (%d bytes)...\n", pComm, nPABufferSize);

	/*
	 * Make sure all data is visible to the secure world
	 */
	dmac_clean_range((void *)pInitSharedBuffer,
		(void *)(((u32)pInitSharedBuffer) + PAGE_SIZE));
	outer_clean_range(__pa(pInitSharedBuffer),
		__pa(pInitSharedBuffer) + PAGE_SIZE);

	dmac_clean_range((void *)pPABufferVAddr,
		(void *)(pPABufferVAddr + nPABufferSize));
	outer_clean_range(__pa(pPABufferVAddr),
		__pa(pPABufferVAddr) + nPABufferSize);

	dmac_clean_range((void *)&sPAInfo,
		(void *)(((u32)&sPAInfo) + sizeof(struct NS_PA_INFO)));
	outer_clean_range(__pa(&sPAInfo),
		__pa(&sPAInfo) + sizeof(struct NS_PA_INFO));
	wmb();

	spin_lock(&(pComm->lock));
	pComm->pInitSharedBuffer = pInitSharedBuffer;
	pComm->pBuffer = pL1SharedBuffer;
	spin_unlock(&(pComm->lock));
	pInitSharedBuffer = NULL;
	pL1SharedBuffer = NULL;

	/*
	 * Set the OS current time in the L1 shared buffer first. The secure
	 * world uses it as itw boot reference time.
	 */
	SCXLNXCommSetCurrentTime(pComm);

	/* Workaround for issue #6082 */
	disable_nonboot_cpus();

	/*
	 * Start the SMC PA
	 */
	ret = omap4_secure_dispatcher(API_HAL_LM_PALOAD_INDEX,
		FLAG_IRQ_ENABLE | FLAG_FIQ_ENABLE | FLAG_START_HAL_CRITICAL, 1,
		__pa(&sPAInfo), 0, 0, 0);
	if (ret != API_HAL_RET_VALUE_OK) {
		printk(KERN_ERR "SMC: Error while loading the PA [0x%x]\n",
			ret);
		goto error2;
	}

	/* Loop until the first S Yield RPC is received */
loop:
	mutex_lock(&(pComm->sRPCLock));

	if (g_RPC_advancement == RPC_ADVANCEMENT_PENDING) {
		dprintk(KERN_INFO "SCXLNXCommExecuteRPCCommand: "
			"Executing CMD=0x%x\n",
			g_RPC_parameters[1]);

		switch (g_RPC_parameters[1]) {
		case RPC_CMD_YIELD:
			dprintk(KERN_INFO "SCXLNXCommExecuteRPCCommand: "
				"RPC_CMD_YIELD\n");
			set_bit(SCXLNX_COMM_FLAG_L1_SHARED_ALLOCATED,
				&(pComm->nFlags));
			g_RPC_parameters[0] = RPC_SUCCESS;
			break;

		case RPC_CMD_INIT:
			dprintk(KERN_INFO "SCXLNXCommExecuteRPCCommand: "
				"RPC_CMD_INIT\n");
			g_RPC_parameters[0] = tf_rpc_init(pComm);
			break;

		case RPC_CMD_TRACE:
			g_RPC_parameters[0] = tf_rpc_trace(pComm);;
			break;

		default:
			g_RPC_parameters[0] = RPC_ERROR_BAD_PARAMETERS;
			break;
		}
		g_RPC_advancement = RPC_ADVANCEMENT_FINISHED;
	}

	mutex_unlock(&(pComm->sRPCLock));

	ret = tf_schedule_secure_world(pComm, false);
	if (ret != 0) {
		printk(KERN_ERR "SMC: Error while loading the PA [0x%x]\n",
			ret);
		goto error2;
	}

	if (!test_bit(SCXLNX_COMM_FLAG_L1_SHARED_ALLOCATED, &(pComm->nFlags)))
		goto loop;

	set_bit(SCXLNX_COMM_FLAG_PA_AVAILABLE, &pComm->nFlags);
	wake_up(&(pComm->waitQueue));
	ret = 0;

	#if 0
	{
		void *workspace_va;
		workspace_va = ioremap(nWorkspaceAddr, nWorkspaceSize);
		printk(KERN_INFO
		"Read first word of workspace [0x%x]\n",
		*(uint32_t *)workspace_va);
	}
	#endif

	/* Workaround for issue #6082 */
	enable_nonboot_cpus();

	goto exit;

error2:
	/* Workaround for issue #6082 */
	enable_nonboot_cpus();

	spin_lock(&(pComm->lock));
	pL1SharedBuffer = pComm->pBuffer;
	pInitSharedBuffer = pComm->pInitSharedBuffer;
	pComm->pBuffer = NULL;
	pComm->pInitSharedBuffer = NULL;
	spin_unlock(&(pComm->lock));

error1:
	if (pInitSharedBuffer != NULL) {
		__clear_page_locked(virt_to_page(pInitSharedBuffer));
		internal_free_page((unsigned long) pInitSharedBuffer);
	}
	if (pL1SharedBuffer != NULL) {
		__clear_page_locked(virt_to_page(pL1SharedBuffer));
		internal_free_page((unsigned long) pL1SharedBuffer);
	}

exit:
#ifdef CONFIG_SMP
	ret_affinity = sched_setaffinity(0, &saved_cpu_mask);
	if (ret_affinity != 0)
		dprintk(KERN_ERR "sched_setaffinity #2 -> 0x%lX", ret_affinity);
#endif

	tf_l4sec_clkdm_allow_idle(true, true);

	if (ret > 0)
		ret = -EFAULT;

	return ret;
}

void SCXLNXCommTerminate(struct SCXLNX_COMM *pComm)
{
	dprintk(KERN_INFO "SCXLNXCommTerminate(%p)\n", pComm);

	spin_lock(&(pComm->lock));

	SCXPublicCryptoTerminate();

	spin_unlock(&(pComm->lock));
}
