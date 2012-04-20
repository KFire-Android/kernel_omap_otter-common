/*
 * WlanDrvIf.c
 *
 * Copyright(c) 1998 - 2009 Texas Instruments. All rights reserved.      
 * All rights reserved.                                                  
 *                                                                       
 * Redistribution and use in source and binary forms, with or without    
 * modification, are permitted provided that the following conditions    
 * are met:                                                              
 *                                                                       
 *  * Redistributions of source code must retain the above copyright     
 *    notice, this list of conditions and the following disclaimer.      
 *  * Redistributions in binary form must reproduce the above copyright  
 *    notice, this list of conditions and the following disclaimer in    
 *    the documentation and/or other materials provided with the         
 *    distribution.                                                      
 *  * Neither the name Texas Instruments nor the names of its            
 *    contributors may be used to endorse or promote products derived    
 *    from this software without specific prior written permission.      
 *                                                                       
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS   
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT     
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR 
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT  
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT      
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT   
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


/*
 * src/esta_drv.c
 *
 * Kernel level portion of eSTA DK Linux module driver
 *
 */


/** \file   WlanDrvIf.c 
 *  \brief  The OS-Dependent interfaces of the WLAN driver with external applications:
 *          - Configuration utilities (including download, configuration and activation)
 *          - Network Stack (Tx and Rx)
 *          - Interrupts
 *          - Events to external applications
 *  
 *  \see    WlanDrvIf.h, Wext.c
 */


#include <net/sock.h>
#include <linux/etherdevice.h>
#include <linux/delay.h>
#include <linux/netlink.h>

#include "WlanDrvIf.h"
#include "osApi.h"
#include "host_platform.h"
#include "context.h"
#include "CmdHndlr.h"
#include "WlanDrvWext.h"
#include "DrvMain.h"
#include "txDataQueue_Api.h"
#include "txMgmtQueue_Api.h"
#include "TWDriver.h"
#include "Ethernet.h"
#ifdef TI_DBG
#include "tracebuf_api.h"
#endif
/* PM hooks */
#ifdef TI_CONFIG_PM_HOOKS
#include "SdioDrv.h"
static int wlanDrvIf_pm_resume(void);
static int wlanDrvIf_pm_suspend(void);
#endif
#include "bmtrace_api.h"
#ifdef STACK_PROFILE
#include "stack_profile.h"
#endif

/* save driver handle just for module cleanup */
static TWlanDrvIfObj *pDrvStaticHandle;  

#define OS_SPECIFIC_RAM_ALLOC_LIMIT			(0xFFFFFFFF)	/* assume OS never reach that limit */


MODULE_DESCRIPTION("TI WLAN Embedded Station Driver");
MODULE_LICENSE("GPL");


#if (LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 31))
static int wlanDrvIf_Xmit(struct sk_buff *skb, struct net_device *dev);
static int wlanDrvIf_XmitDummy(struct sk_buff *skb, struct net_device *dev);
static struct net_device_stats *wlanDrvIf_NetGetStat(struct net_device *dev);
int wlanDrvIf_Open(struct net_device *dev);
int wlanDrvIf_Release(struct net_device *dev);

static struct net_device_ops tiwlan_ops_pri = {
	.ndo_open = wlanDrvIf_Open,
	.ndo_stop = wlanDrvIf_Release,
	.ndo_get_stats = wlanDrvIf_NetGetStat,
	.ndo_do_ioctl = NULL,
	.ndo_start_xmit = wlanDrvIf_Xmit,
};

static struct net_device_ops tiwlan_ops_dummy = {
	.ndo_open = wlanDrvIf_Open,
	.ndo_stop = wlanDrvIf_Release,
	.ndo_get_stats = wlanDrvIf_NetGetStat,
	.ndo_do_ioctl = NULL,
	.ndo_start_xmit = wlanDrvIf_XmitDummy,
};
#endif

/** 
 * \fn     wlanDrvIf_Xmit
 * \brief  Packets transmission
 * 
 * The network stack calls this function in order to transmit a packet
 *     through the WLAN interface.
 * The packet is inserted to the drver Tx queues and its handling is continued
 *     after switching to the driver context.
 *
 * \note   
 * \param  skb - The Linux packet buffer structure
 * \param  dev - The driver network-interface handle
 * \return 0 (= OK)
 * \sa     
 */ 
static int wlanDrvIf_Xmit (struct sk_buff *skb, struct net_device *dev)
{
	TWlanDrvIfObj *drv = (TWlanDrvIfObj *)NETDEV_GET_PRIVATE(dev);
	TTxCtrlBlk *  pPktCtrlBlk;
	int status;

	CL_TRACE_START_L1();

	os_profile (drv, 0, 0);
	drv->stats.tx_packets++;
	drv->stats.tx_bytes += skb->len;

	/* Allocate a TxCtrlBlk for the Tx packet and save timestamp, length and packet handle */
	pPktCtrlBlk = TWD_txCtrlBlk_Alloc (drv->tCommon.hTWD);

	pPktCtrlBlk->tTxDescriptor.startTime    = os_timeStampMs(drv); /* remove use of skb->tstamp.off_usec */
	pPktCtrlBlk->tTxDescriptor.length       = skb->len;
	pPktCtrlBlk->tTxPktParams.pInputPkt     = skb;

	/* Point the first BDL buffer to the Ethernet header, and the second buffer to the rest of the packet */
	pPktCtrlBlk->tTxnStruct.aBuf[0] = skb->data;
	pPktCtrlBlk->tTxnStruct.aLen[0] = ETHERNET_HDR_LEN;
	pPktCtrlBlk->tTxnStruct.aBuf[1] = skb->data + ETHERNET_HDR_LEN;
	pPktCtrlBlk->tTxnStruct.aLen[1] = (TI_UINT16)skb->len - ETHERNET_HDR_LEN;
	pPktCtrlBlk->tTxnStruct.aLen[2] = 0;

	/* Send the packet to the driver for transmission. */
	status = txDataQ_InsertPacket (drv->tCommon.hTxDataQ, pPktCtrlBlk,(TI_UINT8)skb->priority);

	/* If failed (queue full or driver not running), drop the packet. */
    if (status != TI_OK)
    {
        drv->stats.tx_errors++;
    }
	os_profile (drv, 1, 0);

	CL_TRACE_END_L1("tiwlan_drv.ko", "OS", "TX", "");

	return 0;
}
/*--------------------------------------------------------------------------------------*/
/** 
 * \fn     wlanDrvIf_FreeTxPacket
 * \brief  Free the OS Tx packet
 * 
 * Free the OS Tx packet after driver processing is finished.
 *
 * \note   
 * \param  hOs          - The OAL object handle
 * \param  pPktCtrlBlk  - The packet CtrlBlk
 * \param  eStatus      - The packet transmission status (OK/NOK)
 * \return void
 * \sa     
 */ 
/*--------------------------------------------------------------------------------------*/

void wlanDrvIf_FreeTxPacket (TI_HANDLE hOs, TTxCtrlBlk *pPktCtrlBlk, TI_STATUS eStatus)
{
	dev_kfree_skb((struct sk_buff *)pPktCtrlBlk->tTxPktParams.pInputPkt);
}

/** 
 * \fn     wlanDrvIf_XmitDummy
 * \brief  Dummy transmission handler
 * 
 * This function is registered at the network stack interface as the packets-transmission
 *     handler (replacing wlanDrvIf_Xmit) when the driver is not operational.
 * Using this dummy handler is more efficient then checking the driver state for every 
 *     packet transmission.
 *
 * \note   
 * \param  skb - The Linux packet buffer structure
 * \param  dev - The driver network-interface handle
 * \return error 
 * \sa     wlanDrvIf_Xmit
 */ 
static int wlanDrvIf_XmitDummy (struct sk_buff *skb, struct net_device *dev)
{
	/* Just return error. The driver is not running (network stack frees the packet) */
	return -ENODEV;
}


/** 
 * \fn     wlanDrvIf_NetGetStat
 * \brief  Provides driver statistics
 * 
 * Provides driver Tx and Rx statistics to network stack.
 *
 * \note   
 * \param  dev - The driver network-interface handle
 * \return The statistics pointer 
 * \sa     
 */ 
static struct net_device_stats *wlanDrvIf_NetGetStat (struct net_device *dev)
{
	TWlanDrvIfObj *drv = (TWlanDrvIfObj *)NETDEV_GET_PRIVATE(dev);
	ti_dprintf (TIWLAN_LOG_OTHER, "wlanDrvIf_NetGetStat()\n");

	return &drv->stats;
}


/** 
 * \fn     wlanDrvIf_UpdateDriverState
 * \brief  Update the driver state
 * 
 * The DrvMain uses this function to update the OAL with the driver steady state
 *     that is relevant for the driver users.
 * 
 * \note   
 * \param  hOs          - The driver object handle
 * \param  eDriverState - The new driver state
 * \return void
 * \sa     
 */ 
void wlanDrvIf_UpdateDriverState (TI_HANDLE hOs, EDriverSteadyState eDriverState)
{
	TWlanDrvIfObj *drv = (TWlanDrvIfObj *)hOs;

	ti_dprintf(TIWLAN_LOG_OTHER, "wlanDrvIf_UpdateDriverState(): State = %d\n", eDriverState);

	/* Save the new state */
	drv->tCommon.eDriverState = eDriverState;

	/* If the new state is not RUNNING, replace the Tx handler to a dummy one. */
	if (eDriverState != DRV_STATE_RUNNING) {
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 31))
		drv->netdev->hard_start_xmit = wlanDrvIf_XmitDummy;
#else
		drv->netdev->netdev_ops = &tiwlan_ops_dummy;
#endif
	}
}


/** 
 * \fn     wlanDrvIf_HandleInterrupt
 * \brief  The WLAN interrupt handler
 * 
 * The WLAN driver interrupt handler called in the interrupt context.
 * The actual handling is done in the driver's context after switching to the workqueue.
 * 
 * \note   
 * \param  irq      - The interrupt type
 * \param  hDrv     - The driver object handle
 * \param  cpu_regs - The CPU registers
 * \return IRQ_HANDLED
 * \sa     
 */ 
irqreturn_t wlanDrvIf_HandleInterrupt (int irq, void *hDrv, struct pt_regs *cpu_regs)
{
	TWlanDrvIfObj *drv = (TWlanDrvIfObj *)hDrv;

	TWD_InterruptRequest (drv->tCommon.hTWD);

	return IRQ_HANDLED;
}


/** 
 * \fn     PollIrqHandler
 * \brief  WLAN IRQ polling handler - for debug!
 * 
 * A debug option to catch the WLAN events in polling instead of interrupts.
 * A timer calls this function periodically to check the interrupt status register.
 * 
 * \note   
 * \param  parm - The driver object handle
 * \return void
 * \sa     
 */ 
#ifdef PRIODIC_INTERRUPT
static void wlanDrvIf_PollIrqHandler (TI_HANDLE parm)
{
	TWlanDrvIfObj *drv = (TWlanDrvIfObj *)parm;

	wlanDrvIf_HandleInterrupt (0, drv, NULL);
	os_periodicIntrTimerStart (drv);
}
#endif


/** 
 * \fn     wlanDrvIf_DriverTask
 * \brief  The driver task
 * 
 * This is the driver's task, where most of its job is done.
 * External contexts just save required information and schedule the driver's
 *     task to continue the handling.
 * See more information in the context engine module (context.c).
 *
 * \note   
 * \param  hDrv - The driver object handle
 * \return void
 * \sa     
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,23)
static void wlanDrvIf_DriverTask (void *hDrv)
{
	TWlanDrvIfObj *drv = (TWlanDrvIfObj *)hDrv;
#else
static void wlanDrvIf_DriverTask(struct work_struct *work)
{
#ifdef STACK_PROFILE
	register unsigned long sp asm ("sp");
	unsigned long local_sp = sp;
#endif
	TWlanDrvIfObj *drv = container_of(work, TWlanDrvIfObj, tWork);
#endif

#ifdef STACK_PROFILE
	unsigned long curr1, base1;
	unsigned long curr2, base2;
	static unsigned long maximum_stack = 0;
#endif   
	os_profile (drv, 0, 0);

#ifdef STACK_PROFILE
	curr1 = check_stack_start(&base1, local_sp + 4, 0);
#endif

	/* Call the driver main task */
	context_DriverTask (drv->tCommon.hContext);

	os_profile (drv, 1, 0);
	os_wake_lock_timeout(drv);
	os_wake_unlock(drv);
#ifdef STACK_PROFILE
	curr2 = check_stack_stop(&base2, 0);
	if (base2 == base1) {
	/* if the current measurement is bigger then the maximum store it and print*/
		if ((curr1 - curr2) > maximum_stack) {
			printk("STACK PROFILER GOT THE LOCAL MAXIMMUM!!!! \n");
			printk("current operation stack use=%lu \n",(curr1 - curr2));
			printk("total stack use=%lu \n",8192 - curr2 + base2);
			printk("total stack usage=%lu percent \n",100 * (8192 - curr2 + base2) / 8192);
			maximum_stack = curr1 - curr2;
		}
	}
#endif
}


/** 
 * \fn     wlanDrvIf_LoadFiles
 * \brief  Load init files from loader
 * 
 * This function is called from the loader context right after the driver
 *     is created (in IDLE state).
 * It copies the following files to the driver's memory:
 *     - Ini-File - The driver's default parameters values
 *     - NVS-File - The NVS data for FW usage
 *     - FW-Image - The FW program image
 *
 * \note   
 * \param  drv - The driver object handle
 * \return void
 * \sa     wlanDrvIf_GetFile
 */ 
int wlanDrvIf_LoadFiles (TWlanDrvIfObj *drv, TLoaderFilesData *pInitFiles)
{
    if (!pInitFiles)
    {
        ti_dprintf (TIWLAN_LOG_ERROR, "No Init Files!\n");
        return -EINVAL;
    }

    if (drv->tCommon.eDriverState != DRV_STATE_IDLE)
    {
        ti_dprintf (TIWLAN_LOG_ERROR, "Trying to load files not in IDLE state!\n");
        return -EINVAL;
    }

    if (pInitFiles->uIniFileLength) 
    {
        drv->tCommon.tIniFile.uSize = pInitFiles->uIniFileLength;
        drv->tCommon.tIniFile.pImage = kmalloc (pInitFiles->uIniFileLength, GFP_KERNEL);
        #ifdef TI_MEM_ALLOC_TRACE        
          os_printf ("MTT:%s:%d ::kmalloc(%lu, %x) : %lu\n", __FUNCTION__, __LINE__, pInitFiles->uIniFileLength, GFP_KERNEL, pInitFiles->uIniFileLength);
        #endif
        if (!drv->tCommon.tIniFile.pImage)
        {
            ti_dprintf (TIWLAN_LOG_ERROR, "Cannot allocate buffer for Ini-File!\n");
            return -ENOMEM;
        }
        memcpy (drv->tCommon.tIniFile.pImage,
                &pInitFiles->data[pInitFiles->uNvsFileLength + pInitFiles->uFwFileLength],
                drv->tCommon.tIniFile.uSize);
    }

    if (pInitFiles->uNvsFileLength)
    {
        drv->tCommon.tNvsImage.uSize = pInitFiles->uNvsFileLength;
        drv->tCommon.tNvsImage.pImage = kmalloc (drv->tCommon.tNvsImage.uSize, GFP_KERNEL);
        #ifdef TI_MEM_ALLOC_TRACE        
          os_printf ("MTT:%s:%d ::kmalloc(%lu, %x) : %lu\n", 
              __FUNCTION__, __LINE__, drv->tCommon.tNvsImage.uSize, GFP_KERNEL, drv->tCommon.tNvsImage.uSize);
        #endif
        if (!drv->tCommon.tNvsImage.pImage)
        {
            ti_dprintf (TIWLAN_LOG_ERROR, "Cannot allocate buffer for NVS image\n");
            return -ENOMEM;
        }
        memcpy (drv->tCommon.tNvsImage.pImage, &pInitFiles->data[0], drv->tCommon.tNvsImage.uSize );
    }
    
    drv->tCommon.tFwImage.uSize = pInitFiles->uFwFileLength;
    if (!drv->tCommon.tFwImage.uSize)
    {
        ti_dprintf (TIWLAN_LOG_ERROR, "No firmware image\n");
        return -EINVAL;
    }
    drv->tCommon.tFwImage.pImage = os_memoryAlloc (drv, drv->tCommon.tFwImage.uSize);
    #ifdef TI_MEM_ALLOC_TRACE        
      os_printf ("MTT:%s:%d ::kmalloc(%lu, %x) : %lu\n", 
          __FUNCTION__, __LINE__, drv->tCommon.tFwImage.uSize, GFP_KERNEL, drv->tCommon.tFwImage.uSize);
    #endif
    if (!drv->tCommon.tFwImage.pImage)
    {
        ti_dprintf(TIWLAN_LOG_ERROR, "Cannot allocate buffer for firmware image\n");
        return -ENOMEM;
    }
    memcpy (drv->tCommon.tFwImage.pImage,
            &pInitFiles->data[pInitFiles->uNvsFileLength],
            drv->tCommon.tFwImage.uSize);

    ti_dprintf(TIWLAN_LOG_OTHER, "--------- Eeeprom=%p(%lu), Firmware=%p(%lu), IniFile=%p(%lu)\n", 
        drv->tCommon.tNvsImage.pImage, drv->tCommon.tNvsImage.uSize, 
        drv->tCommon.tFwImage.pImage,  drv->tCommon.tFwImage.uSize,
        drv->tCommon.tIniFile.pImage,  drv->tCommon.tIniFile.uSize);
    
    return 0;
}


/** 
 * \fn     wlanDrvIf_GetFile
 * \brief  Provides access to a requested init file
 * 
 * Provide the requested file information and call the requester callback.
 * Note that in Linux the files were previously loaded to driver memory 
 *     by the loader (see wlanDrvIf_LoadFiles).
 *
 * \note   
 * \param  hOs       - The driver object handle
 * \param  pFileInfo - The requested file's properties
 * \return TI_OK
 * \sa     wlanDrvIf_LoadFiles
 */ 
int wlanDrvIf_GetFile (TI_HANDLE hOs, TFileInfo *pFileInfo)
{
	TWlanDrvIfObj *drv = (TWlanDrvIfObj *)hOs;

	if (drv == NULL || pFileInfo == NULL) {
		ti_dprintf(TIWLAN_LOG_ERROR, "wlanDrv_GetFile: ERROR: Null File Handler, Exiting");
		return TI_NOK;
	}

	/* Future option for getting the FW image part by part */ 
	pFileInfo->hOsFileDesc = NULL;

	/* Fill the file's location and size in the file's info structure */
	switch (pFileInfo->eFileType) 
	{
	case FILE_TYPE_INI: 
		pFileInfo->pBuffer = (TI_UINT8 *)drv->tCommon.tIniFile.pImage; 
		pFileInfo->uLength = drv->tCommon.tIniFile.uSize; 
		break;
	case FILE_TYPE_NVS:     
		pFileInfo->pBuffer = (TI_UINT8 *)drv->tCommon.tNvsImage.pImage; 
		pFileInfo->uLength = drv->tCommon.tNvsImage.uSize; 
		break;
	case FILE_TYPE_FW:
		if (drv->tCommon.tFwImage.pImage == NULL)
		{
			ti_dprintf(TIWLAN_LOG_ERROR, "wlanDrv_GetFile: ERROR: no Firmware image, exiting\n");
			return TI_NOK;
		}

		pFileInfo->pBuffer = (TI_UINT8 *)drv->tCommon.tFwImage.pImage; 
		pFileInfo->bLast		= TI_FALSE;
		pFileInfo->uLength	= 0;
		pFileInfo->uOffset 				= 0;
		pFileInfo->uChunkBytesLeft 		= 0;
		pFileInfo->uChunksLeft 			= BYTE_SWAP_LONG( *((TI_UINT32*)(pFileInfo->pBuffer)) );
		/* check uChunksLeft's Validity */
		if (( pFileInfo->uChunksLeft == 0 ) || ( pFileInfo->uChunksLeft > MAX_CHUNKS_IN_FILE ))
		{
			ti_dprintf (TIWLAN_LOG_ERROR, "wlanDrvIf_GetFile() Read Invalid Chunks Left: %d!\n",pFileInfo->uChunksLeft);
			return TI_NOK;
		}
		pFileInfo->pBuffer += DRV_ADDRESS_SIZE;
		/* FALL THROUGH */
	case FILE_TYPE_FW_NEXT:
		/* check dec. validity */
		if ( pFileInfo->uChunkBytesLeft >= pFileInfo->uLength )
		{
			pFileInfo->uChunkBytesLeft 		-= pFileInfo->uLength;
		}
		/* invalid Dec. */
		else
		{
			ti_dprintf (TIWLAN_LOG_ERROR, "wlanDrvIf_GetFile() No. of Bytes Left < File Length\n");
			return TI_NOK;
		}
		pFileInfo->pBuffer 	+= pFileInfo->uLength; 

		/* Finished reading all Previous Chunk */
		if ( pFileInfo->uChunkBytesLeft == 0 )
		{
			/* check dec. validity */
			if ( pFileInfo->uChunksLeft > 0 )
			{
				pFileInfo->uChunksLeft--;
			}
			/* invalid Dec. */
			else
			{
				ti_dprintf (TIWLAN_LOG_ERROR, "No. of Bytes Left = 0 and Chunks Left <= 0\n");
				return TI_NOK;
			}
			/* read Chunk's address */
			pFileInfo->uAddress = BYTE_SWAP_LONG( *((TI_UINT32*)(pFileInfo->pBuffer)) );
			pFileInfo->pBuffer += DRV_ADDRESS_SIZE;
			/* read Portion's length */
			pFileInfo->uChunkBytesLeft = BYTE_SWAP_LONG( *((TI_UINT32*)(pFileInfo->pBuffer)) );
			pFileInfo->pBuffer += DRV_ADDRESS_SIZE;
		}
		/* Reading Chunk is NOT complete */
		else
		{
			pFileInfo->uAddress += pFileInfo->uLength;
		}
		
		if ( pFileInfo->uChunkBytesLeft < OS_SPECIFIC_RAM_ALLOC_LIMIT )
		{
			pFileInfo->uLength = pFileInfo->uChunkBytesLeft;
		}
		else
		{
			pFileInfo->uLength = OS_SPECIFIC_RAM_ALLOC_LIMIT;
		}

		/* If last chunk to download */
		if (( pFileInfo->uChunksLeft == 0 ) && 
			( pFileInfo->uLength == pFileInfo->uChunkBytesLeft ))
		{
			pFileInfo->bLast = TI_TRUE;
		}

		break;
	}

	/* Call the requester callback */
	if (pFileInfo->fCbFunc)
	{
		pFileInfo->fCbFunc (pFileInfo->hCbHndl);
	}

	return TI_OK;
}


/** 
 * \fn     wlanDrvIf_SetMacAddress
 * \brief  Set STA MAC address
 * 
 * Called by DrvMain from init process.
 * Copies STA MAC address to the network interface structure.
 *
 * \note   
 * \param  hOs      - The driver object handle
 * \param  pMacAddr - The STA MAC address
 * \return TI_OK
 * \sa     wlanDrvIf_LoadFiles
 */ 
void wlanDrvIf_SetMacAddress (TI_HANDLE hOs, TI_UINT8 *pMacAddr)
{
	TWlanDrvIfObj *drv = (TWlanDrvIfObj *)hOs;

	/* Copy STA MAC address to the network interface structure */
	MAC_COPY (drv->netdev->dev_addr, pMacAddr);
}


/** 
 * \fn     wlanDrvIf_Start
 * \brief  Start driver
 * 
 * Called by network stack upon opening network interface (ifconfig up).
 * Can also be called from user application or CLI for flight mode.
 * Start the driver initialization process up to OPERATIONAL state.
 *
 * \note   
 * \param  dev - The driver network-interface handle
 * \return 0 if succeeded, error if driver not available
 * \sa     wlanDrvIf_Stop
 */ 
int wlanDrvIf_Start (struct net_device *dev)
{
	TWlanDrvIfObj *drv = (TWlanDrvIfObj *)NETDEV_GET_PRIVATE(dev);
	int status;

	ti_dprintf (TIWLAN_LOG_OTHER, "wlanDrvIf_Start()\n");
	printk("%s\n", __func__);
    if (!drv->tCommon.hDrvMain)
    {
		ti_dprintf (TIWLAN_LOG_ERROR, "wlanDrvIf_Start() Driver not created!\n");
		return -ENODEV;
	}

	if (DRV_STATE_FAILED == drv->tCommon.eDriverState)
	{
		ti_dprintf (TIWLAN_LOG_ERROR, "wlanDrvIf_Start() Driver failed!\n");
		return -ENODEV;
	}

	/*
	 *  Insert Start command in DrvMain action queue, request driver scheduling 
	 *      and wait for action completion (all init process).
	 */
	os_wake_lock_timeout_enable(drv);
	status = drvMain_InsertAction (drv->tCommon.hDrvMain, ACTION_TYPE_START);
	return (status) ? -1 : 0;
}

int wlanDrvIf_Open (struct net_device *dev)
{
	TWlanDrvIfObj *drv = (TWlanDrvIfObj *)NETDEV_GET_PRIVATE(dev);
	int status = 0;

	ti_dprintf (TIWLAN_LOG_OTHER, "wlanDrvIf_Open()\n");
	printk("%s\n", __func__);
	if (!drv->tCommon.hDrvMain) {
		ti_dprintf (TIWLAN_LOG_ERROR, "wlanDrvIf_Open() Driver not created!\n");
		return -ENODEV;
	}

	if (drv->tCommon.eDriverState == DRV_STATE_STOPPED ||
	    drv->tCommon.eDriverState == DRV_STATE_IDLE) {
		status = wlanDrvIf_Start(dev);
	}

	/*
	 *  Finalize network interface setup
	 */
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 31))
	drv->netdev->hard_start_xmit = wlanDrvIf_Xmit;
#else
	drv->netdev->netdev_ops = &tiwlan_ops_pri;
#endif
	drv->netdev->addr_len = MAC_ADDR_LEN;
	netif_start_queue (dev);

	/* register 3430 PM hooks in our SDIO driver */
#ifdef TI_CONFIG_PM_HOOKS
#ifndef CONFIG_MMC_EMBEDDED_SDIO
	sdioDrv_register_pm(wlanDrvIf_pm_resume, wlanDrvIf_pm_suspend);
#endif
#endif
	return status;
}

/** 
 * \fn     wlanDrvIf_Stop
 * \brief  Stop driver
 * 
 * Called by network stack upon closing network interface (ifconfig down).
 * Can also be called from user application or CLI for flight mode.
 * Stop the driver and turn off the device.
 *
 * \note   
 * \param  dev - The driver network-interface handle
 * \return 0 (OK)
 * \sa     wlanDrvIf_Start
 */ 
int wlanDrvIf_Stop (struct net_device *dev)
{
	TWlanDrvIfObj *drv = (TWlanDrvIfObj *)NETDEV_GET_PRIVATE(dev);

	ti_dprintf (TIWLAN_LOG_OTHER, "wlanDrvIf_Stop()\n");
	printk("%s\n", __func__);

	if (DRV_STATE_FAILED == drv->tCommon.eDriverState)
	{
		return -ENODEV;
	}

	/* 
	 *  Insert Stop command in DrvMain action queue, request driver scheduling 
	 *      and wait for Stop process completion.
	 */
	os_wake_lock_timeout_enable(drv);
	drvMain_InsertAction (drv->tCommon.hDrvMain, ACTION_TYPE_STOP);
	return 0;
}

int wlanDrvIf_Release (struct net_device *dev)
{
	/* TWlanDrvIfObj *drv = (TWlanDrvIfObj *)NETDEV_GET_PRIVATE(dev); */

	ti_dprintf (TIWLAN_LOG_OTHER, "wlanDrvIf_Release()\n");
	printk("%s\n", __func__);
	/* Disable network interface queue */
	netif_stop_queue (dev);
	return 0;
}

/* 3430 PM hooks */
#ifdef TI_CONFIG_PM_HOOKS
static int wlanDrvIf_pm_resume(void)
{
    return(wlanDrvIf_Open(pDrvStaticHandle->netdev));
}

static int wlanDrvIf_pm_suspend(void)
{
    wlanDrvIf_Release(pDrvStaticHandle->netdev);
    return(wlanDrvIf_Stop(pDrvStaticHandle->netdev));
}
#endif

/** 
 * \fn     wlanDrvIf_SetupNetif
 * \brief  Setup driver network interface
 * 
 * Called in driver creation process.
 * Setup driver network interface.
 *
 * \note   
 * \param  drv - The driver object handle
 * \return 0 - OK, else - failure
 * \sa     
 */ 
static int wlanDrvIf_SetupNetif (TWlanDrvIfObj *drv)
{
	struct net_device *dev;
	int res;

	/* Allocate network interface structure for the driver */
	dev = alloc_etherdev (0);
	if (dev == NULL)
	{
		ti_dprintf (TIWLAN_LOG_ERROR, "alloc_etherdev() failed\n");
		return -ENOMEM;
	}

	/* Setup the network interface */
	ether_setup (dev);

	NETDEV_SET_PRIVATE(dev,drv);
	drv->netdev = dev;
	strcpy (dev->name, TIWLAN_DRV_IF_NAME);
	netif_carrier_off (dev);
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 31))
/* the following is required on at least BSP 23.8 and higher.
    Without it, the Open function of the driver will not be called
    when trying to 'ifconfig up' the interface */
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,23)
	dev->validate_addr = NULL;
#endif
	dev->open = wlanDrvIf_Open;
	dev->stop = wlanDrvIf_Release;
	dev->hard_start_xmit = wlanDrvIf_XmitDummy;
	dev->get_stats = wlanDrvIf_NetGetStat;
	dev->do_ioctl = NULL;
#else
	dev->netdev_ops = &tiwlan_ops_dummy;
#endif
	dev->tx_queue_len = 100;

	/* Initialize Wireless Extensions interface (WEXT) */
	wlanDrvWext_Init (dev);

	res = register_netdev (dev);
	if (res != 0)
	{
		ti_dprintf (TIWLAN_LOG_ERROR, "register_netdev() failed : %d\n", res);
		kfree (dev);
		return res;
	}
/*
On the latest Kernel there is no more support for the below macro.
*/
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
	SET_MODULE_OWNER (dev);
#endif
	return 0;
}

/** 
 * \fn     wlanDrvIf_CommandDone
 * \brief  Free current command semaphore.
 * 
 * This routine is called whenever a command has finished executing and Free current command semaphore.
 *
 * \note   
 * \param  hOs           - The driver object handle
 * \param  pSignalObject - handle to complete mechanism per OS
 * \param  CmdResp_p     - respond structure (TCmdRespUnion) for OSE OS only 
 * \return 0 - OK, else - failure
 * \sa     wlanDrvIf_Destroy
 */ 
void wlanDrvIf_CommandDone (TI_HANDLE hOs, void *pSignalObject, TI_UINT8 *CmdResp_p)
{
	/* Free semaphore */
	os_SignalObjectSet (hOs, pSignalObject);
}


/** 
 * \fn     wlanDrvIf_Create
 * \brief  Create the driver instance
 * 
 * Allocate driver object.
 * Initialize driver OS resources (IRQ, workqueue, events socket)
 * Setup driver network interface.
 * Create and link all driver modules.
 *
 * \note   
 * \param  void
 * \return 0 - OK, else - failure
 * \sa     wlanDrvIf_Destroy
 */ 
static int wlanDrvIf_Create (void)
{
	TWlanDrvIfObj *drv; /* Dm: Failure is not cleaned properly !!! */
	int rc;

	/* Allocate driver's structure */
	drv = kmalloc (sizeof(TWlanDrvIfObj), GFP_KERNEL);
    if (!drv)
    {
		return -ENOMEM;
	}
#ifdef TI_DBG
	tb_init(TB_OPTION_NONE);
#endif
	pDrvStaticHandle = drv;  /* save for module destroy */
#ifdef TI_MEM_ALLOC_TRACE        
	os_printf ("MTT:%s:%d ::kmalloc(%lu, %x) : %lu\n", __FUNCTION__, __LINE__, sizeof(TWlanDrvIfObj), GFP_KERNEL, sizeof(TWlanDrvIfObj));
#endif
	memset (drv, 0, sizeof(TWlanDrvIfObj));

	/* Dm:    drv->irq = TNETW_IRQ; */
	drv->tCommon.eDriverState = DRV_STATE_IDLE;

	drv->tiwlan_wq = create_freezeable_workqueue(DRIVERWQ_NAME);
	if (!drv->tiwlan_wq) {
		ti_dprintf (TIWLAN_LOG_ERROR, "wlanDrvIf_Create(): Failed to create workQ!\n");
		rc = -EINVAL;
		goto drv_create_end_1;
	}
	drv->wl_packet = 0;
	drv->wl_count = 0;
#ifdef CONFIG_HAS_WAKELOCK
	wake_lock_init(&drv->wl_wifi, WAKE_LOCK_SUSPEND, "wifi_wake");
	wake_lock_init(&drv->wl_rxwake, WAKE_LOCK_SUSPEND, "wifi_rx_wake");
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,23)
	INIT_WORK(&drv->tWork, wlanDrvIf_DriverTask, (void *)drv);
#else
	INIT_WORK(&drv->tWork, wlanDrvIf_DriverTask);
#endif
	spin_lock_init (&drv->lock);

	/* Setup driver network interface. */
	rc = wlanDrvIf_SetupNetif (drv);
	if (rc)	{
		goto drv_create_end_2;
	}

	/* Create the events socket interface */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,23)
	drv->wl_sock = netlink_kernel_create( NETLINK_USERSOCK, 0, NULL, THIS_MODULE );
#else
	drv->wl_sock = netlink_kernel_create(&init_net, NETLINK_USERSOCK, 0, NULL, NULL, THIS_MODULE );
#endif
	if (drv->wl_sock == NULL) {
	        ti_dprintf (TIWLAN_LOG_ERROR, "netlink_kernel_create() failed !\n");
		rc = -EINVAL;
		goto drv_create_end_3;
	}

	/* Create all driver modules and link their handles */
	rc = drvMain_Create (drv,
			&drv->tCommon.hDrvMain,
			&drv->tCommon.hCmdHndlr,
			&drv->tCommon.hContext,
			&drv->tCommon.hTxDataQ,
			&drv->tCommon.hTxMgmtQ,
			&drv->tCommon.hTxCtrl,
			&drv->tCommon.hTWD,
					&drv->tCommon.hEvHandler,
                    &drv->tCommon.hCmdDispatch,
		&drv->tCommon.hReport);
	if (rc != TI_OK) {
		ti_dprintf (TIWLAN_LOG_ERROR, "%s: Failed to dvrMain_Create!\n", __func__);
		rc = -EINVAL;
		goto drv_create_end_4;
	}
	/*
	 *  Initialize interrupts (or polling mode for debug):
	 */
#ifdef PRIODIC_INTERRUPT
	/* Debug mode: Polling (the timer is started by HwInit process) */
	drv->hPollTimer = os_timerCreate ((TI_HANDLE)drv, wlanDrvIf_PollIrqHandler, (TI_HANDLE)drv);
#else 
	/* Normal mode: Interrupts (the default mode) */
	rc = hPlatform_initInterrupt (drv, (void*)wlanDrvIf_HandleInterrupt);
	if (rc)	{
		ti_dprintf (TIWLAN_LOG_ERROR, "wlanDrvIf_Create(): Failed to register interrupt handler!\n");
		goto drv_create_end_5;
	}
#endif  /* PRIODIC_INTERRUPT */
	return 0;
drv_create_end_5:
	/* Destroy all driver modules */
	if (drv->tCommon.hDrvMain) {
		drvMain_Destroy (drv->tCommon.hDrvMain);
	}
drv_create_end_4:
	if (drv->wl_sock) {
		sock_release (drv->wl_sock->sk_socket);
	}

drv_create_end_3:
	/* Release the driver network interface */
	if (drv->netdev) {
		unregister_netdev (drv->netdev);
		free_netdev (drv->netdev);
	}

drv_create_end_2:
#ifdef CONFIG_HAS_WAKELOCK
	wake_lock_destroy(&drv->wl_wifi);
	wake_lock_destroy(&drv->wl_rxwake);
#endif
	if (drv->tiwlan_wq)
		destroy_workqueue(drv->tiwlan_wq);

drv_create_end_1:
	kfree(drv);
	printk("%s: Fail\n", __func__);
	return rc;
}


/** 
 * \fn     wlanDrvIf_Destroy
 * \brief  Destroy the driver instance
 * 
 * Destroy all driver modules.
 * Release driver OS resources (IRQ, workqueue, events socket)
 * Release driver network interface.
 * Free init files memory.
 * Free driver object.
 *
 * \note   
 * \param  drv - The driver object handle
 * \return void
 * \sa     wlanDrvIf_Create
 */ 
static void wlanDrvIf_Destroy (TWlanDrvIfObj *drv)
{
	if (!drv)
		return;

	if (drv->tiwlan_wq) {
		cancel_work_sync(&drv->tWork);
		flush_workqueue(drv->tiwlan_wq);
	}

	/* Release the driver network interface */
	if (drv->netdev) {
		netif_stop_queue  (drv->netdev);
		wlanDrvIf_Stop    (drv->netdev);
		unregister_netdev (drv->netdev);
		free_netdev (drv->netdev);
	}

	/* Destroy all driver modules */
	if (drv->tCommon.hDrvMain) {
		drvMain_Destroy (drv->tCommon.hDrvMain);
	}

	/* close the ipc_kernel socket*/
    if (drv && drv->wl_sock) 
    {
		sock_release (drv->wl_sock->sk_socket);
	}
	/* Release the driver interrupt (or polling timer) */
#ifdef PRIODIC_INTERRUPT
	os_timerDestroy (drv, drv->hPollTimer);
#else
    if (drv->irq)
    {
		hPlatform_freeInterrupt(drv);
	}
#endif
	if (drv->tiwlan_wq)
		destroy_workqueue(drv->tiwlan_wq);

#ifdef CONFIG_HAS_WAKELOCK
	wake_lock_destroy(&drv->wl_wifi);
	wake_lock_destroy(&drv->wl_rxwake);
#endif
	/* 
	 *  Free init files memory
	 */
    if (drv->tCommon.tFwImage.pImage)
    {
        os_memoryFree (drv, drv->tCommon.tFwImage.pImage, drv->tCommon.tFwImage.uSize);
        #ifdef TI_MEM_ALLOC_TRACE        
          os_printf ("MTT:%s:%d ::kfree(0x%p) : %d\n", 
              __FUNCTION__, __LINE__, drv->tCommon.tFwImage.uSize, -drv->tCommon.tFwImage.uSize);
        #endif
    }
    if (drv->tCommon.tNvsImage.pImage)
    {
        kfree (drv->tCommon.tNvsImage.pImage);
        #ifdef TI_MEM_ALLOC_TRACE        
          os_printf ("MTT:%s:%d ::kfree(0x%p) : %d\n", 
              __FUNCTION__, __LINE__, drv->tCommon.tNvsImage.uSize, -drv->tCommon.tNvsImage.uSize);
        #endif
    }
    if (drv->tCommon.tIniFile.pImage)
    {
        kfree (drv->tCommon.tIniFile.pImage);
        #ifdef TI_MEM_ALLOC_TRACE        
          os_printf ("MTT:%s:%d ::kfree(0x%p) : %d\n", 
              __FUNCTION__, __LINE__, drv->tCommon.tIniFile.uSize, -drv->tCommon.tIniFile.uSize);
        #endif
    }

    /* Free the driver object */
#ifdef TI_DBG
	tb_destroy();
#endif
	kfree (drv);
}


/** 
 * \fn     wlanDrvIf_ModuleInit  &  wlanDrvIf_ModuleExit
 * \brief  Linux Init/Exit functions
 * 
 * The driver Linux Init/Exit functions (insmod/rmmod) 
 *
 * \note   
 * \param  void
 * \return Init: 0 - OK, else - failure.   Exit: void
 * \sa     wlanDrvIf_Create, wlanDrvIf_Destroy
 */ 
#ifndef TI_SDIO_STANDALONE
static int sdc_ctrl = 2;
module_param(sdc_ctrl, int, S_IRUGO | S_IWUSR | S_IWGRP);

extern int sdioDrv_init(int sdcnum);
extern void sdioDrv_exit(void);
#endif

static int __init wlanDrvIf_ModuleInit (void)
{
	printk(KERN_INFO "TIWLAN: driver init\n");
#ifndef TI_SDIO_STANDALONE
#ifndef CONFIG_MMC_EMBEDDED_SDIO
	sdioDrv_init(sdc_ctrl);
#endif
#endif
	return wlanDrvIf_Create ();
}

static void __exit wlanDrvIf_ModuleExit (void)
{
	wlanDrvIf_Destroy (pDrvStaticHandle);
#ifndef TI_SDIO_STANDALONE
#ifndef CONFIG_MMC_EMBEDDED_SDIO
	sdioDrv_exit();
#endif
#endif
	printk (KERN_INFO "TI WLAN: driver unloaded\n");
}


/**
 * \fn     wlanDrvIf_StopTx
 * \brief  block Tx thread until wlanDrvIf_ResumeTx called .
 *
 * This routine is called whenever we need to stop the network stack to send us pakets since one of our Q's is full.
 *
 * \note
 * \param  hOs           - The driver object handle
* \return
 * \sa     wlanDrvIf_StopTx
 */
void wlanDrvIf_StopTx (TI_HANDLE hOs)
{
    TWlanDrvIfObj *drv = (TWlanDrvIfObj *)hOs;

    netif_stop_queue (drv->netdev);
}

/**
 * \fn     wlanDrvIf_ResumeTx
 * \brief  Resume Tx thread .
 *
 * This routine is called whenever we need to resume the network stack to send us pakets since our Q's are empty.
 *
 * \note
 * \param  hOs           - The driver object handle
 * \return
 * \sa     wlanDrvIf_ResumeTx
 */
void wlanDrvIf_ResumeTx (TI_HANDLE hOs)
{
    TWlanDrvIfObj *drv = (TWlanDrvIfObj *)hOs;

    netif_wake_queue (drv->netdev);
}

module_init (wlanDrvIf_ModuleInit);
module_exit (wlanDrvIf_ModuleExit);
