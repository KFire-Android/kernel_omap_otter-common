/*
 * osapi.c
 *
 * Copyright(c) 1998 - 2010 Texas Instruments. All rights reserved.      
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
 * src/osapi.c
 *
 */
#include "tidef.h"
#include "arch_ti.h"

#include <linux/stddef.h>
#include <linux/string.h>
#include <linux/time.h>
#include <linux/timer.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/completion.h>
#include <linux/etherdevice.h>
#include <linux/vmalloc.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/time.h>
#include <linux/list.h>
#include <stdarg.h>
#include <asm/io.h>
#include "RxBuf_linux.h"

/*#include "debug_module.h"*/
#include "host_platform.h"
#include "WlanDrvIf.h"
#include "bmtrace_api.h"
#include "TI_IPC_Api.h"
#include "802_11Defs.h"
#include "osApi.h"
#include "txMgmtQueue_Api.h"
#include "EvHandler.h"

#ifdef ESTA_TIMER_DEBUG
#define esta_timer_log(fmt,args...)  printk(fmt, ## args)
#else
#define esta_timer_log(fmt,args...)
#endif

#define FRAG_SIZE        200

typedef struct timer_list TOsTimer;

TI_BOOL bRedirectOutputToLogger = TI_FALSE;
TI_BOOL use_debug_module = TI_FALSE;

/****************************************************************************************
 *                        																*
 *						OS Report API													*       
 *																						*
 ****************************************************************************************/
static void SendLoggerData (TI_HANDLE OsContext, TI_UINT8 *pMsg, TI_UINT16 len)
{    
	TWlanDrvIfObj *drv = (TWlanDrvIfObj *)OsContext;

	if (len > 0)
	{
		EvHandlerSendEvent(drv->tCommon.hEvHandler, IPC_EVENT_LOGGER, pMsg, len);
	}   
}

void os_setDebugOutputToLogger(TI_BOOL value)
{
	bRedirectOutputToLogger = value;
}
/****************************************************************************************
 *                        os_setDebugMode()                                 
 ****************************************************************************************
DESCRIPTION:  	Set the Debug Mode 

INPUT:            

RETURN:			None   

NOTES:         	
*****************************************************************************************/
void os_setDebugMode(TI_BOOL enable)
{
	use_debug_module = enable;
}


/****************************************************************************************
 *                        os_printf()                                 
 ****************************************************************************************
DESCRIPTION:  	Print formatted output. 

INPUT:          format -  Specifies the string, to be printed

RETURN:			None   

NOTES:         	
*****************************************************************************************/
void os_printf(const char *format ,...)
{
	static int from_new_line = 1;		/* Used to save the last message EOL */
	va_list ap;
	static char msg[MAX_MESSAGE_SIZE];
	char *p_msg = msg;					/* Pointer to the message */
	TI_UINT16 message_len;					
	TI_UINT32 sec = 0;
	TI_UINT32 uSec = 0;
	os_memoryZero(NULL,msg, MAX_MESSAGE_SIZE);

	/* Format the message and keep the message length */
	va_start(ap,format);
	message_len = vsnprintf(&msg[0], sizeof(msg) -1 , format, ap);
	if( from_new_line )
        {
            if (msg[1] == '$')
            {
                p_msg += 4;
            }
            
            sec = os_timeStampUs(NULL);
            uSec = sec % MICROSECOND_IN_SECONDS;
            sec /= MICROSECOND_IN_SECONDS;
            
            printk(KERN_INFO DRIVER_NAME ": %d.%06d: %s",sec,uSec,p_msg);
        }
        else
        {
		printk(&msg[0]);
        }
        
	from_new_line = ( msg[message_len - 1] == '\n' );

	va_end(ap);
}

/****************************************************************************************
 *                        																*
 *							OS TIMER API												*
 *																						*
 ****************************************************************************************/

/****************************************************************************************
 *                        os_timerCreate()                                 
 ****************************************************************************************
DESCRIPTION:    This function creates and initializes an OS timer object associated with a
				caller's pRoutine function.

ARGUMENTS:		OsContext   - The OS handle
                pRoutine    - The user callback function
                hFuncHandle - The user callback handle

RETURN:			A handle of the created OS timer.

NOTES:         	1) The user's callback is called directly from OS timer context when expired.
                2) In some OSs, it may be needed to use an intermediate callback in the 
                   osapi layer (use os_timerHandlr for that).

*****************************************************************************************/
TI_HANDLE os_timerCreate (TI_HANDLE OsContext, fTimerFunction pRoutine, TI_HANDLE hFuncHandle)
{
	TOsTimer *pOsTimer = os_memoryAlloc (OsContext, sizeof(TOsTimer));

    if(pOsTimer)
    {
        init_timer (pOsTimer);
        pOsTimer->function = (void *)pRoutine;
        pOsTimer->data     = (int)hFuncHandle;
    }

    return (TI_HANDLE)pOsTimer;
}


/****************************************************************************************
 *                        os_timerDestroy()                                 
 ****************************************************************************************
DESCRIPTION:    This function destroys the OS timer object.

ARGUMENTS:		

RETURN:			

NOTES:         	
*****************************************************************************************/
void os_timerDestroy (TI_HANDLE OsContext, TI_HANDLE TimerHandle)
{
	os_timerStop (OsContext, TimerHandle);
	os_memoryFree (OsContext, TimerHandle, sizeof(TOsTimer));
}


/****************************************************************************************
 *                        os_timerStart()                                 
 ****************************************************************************************
DESCRIPTION:    This function start the timer object.

ARGUMENTS:		

RETURN:			

NOTES:         	
*****************************************************************************************/
void os_timerStart (TI_HANDLE OsContext, TI_HANDLE TimerHandle, TI_UINT32 DelayMs)
{
	TI_UINT32 jiffie_cnt = msecs_to_jiffies (DelayMs);

	mod_timer ((TOsTimer *)TimerHandle, jiffies + jiffie_cnt);
}


/****************************************************************************************
 *                        os_stopTimer()                                 
 ****************************************************************************************
DESCRIPTION:    This function stop the timer object.

ARGUMENTS:		

RETURN:			

NOTES:         	
*****************************************************************************************/
void os_timerStop (TI_HANDLE OsContext, TI_HANDLE TimerHandle)
{
	del_timer_sync((TOsTimer *)TimerHandle);
}


/****************************************************************************************
 *                        os_periodicIntrTimerStart()                                 
 ****************************************************************************************
DESCRIPTION:    This function starts the periodic interrupt mechanism. This mode is used 
				when interrupts that usually received from the Fw is now masked, and we are
				checking for any need of Fw handling in time periods.

ARGUMENTS:		

RETURN:			

NOTES:         	Power level of the CHIP should be always awake in this mode (no ELP)
*****************************************************************************************/
#ifdef PRIODIC_INTERRUPT
void os_periodicIntrTimerStart (TI_HANDLE OsContext)
{
	TWlanDrvIfObj *drv = (TWlanDrvIfObj *)OsContext;

	mod_timer (drv->hPollTimer, jiffies + TIWLAN_IRQ_POLL_INTERVAL);
}
#endif


/****************************************************************************************
 *                        os_timeStampMs()                                 
 ****************************************************************************************
DESCRIPTION:	This function returns the number of milliseconds that have elapsed since
				the system was booted.

ARGUMENTS:		OsContext - our adapter context.

RETURN:			

NOTES:         	
*****************************************************************************************/
TI_UINT32 os_timeStampMs (TI_HANDLE OsContext)
{
	struct timeval tv;
	do_gettimeofday(&tv);
	return tv.tv_sec*1000 + tv.tv_usec/1000;
}


/****************************************************************************************
 *                        os_timeStampUs()                                 
 ****************************************************************************************
DESCRIPTION:	This function returns the number of microseconds that have elapsed since
				the system was booted.

ARGUMENTS:		OsContext - our adapter context.
				Note that sometimes this function will be called with NULL(!!!) as argument!

RETURN:			

NOTES:         	
*****************************************************************************************/
TI_UINT32 os_timeStampUs (TI_HANDLE OsContext)
{
	struct timeval tv;
	do_gettimeofday(&tv);
	return tv.tv_sec*1000000 + tv.tv_usec;
}


/****************************************************************************************
 *                        os_StalluSec()                                 
 ****************************************************************************************
DESCRIPTION:	This function make delay in microseconds.

ARGUMENTS:		OsContext - our adapter context.
				uSec - delay time in microseconds

RETURN:			

NOTES:         	
*****************************************************************************************/
void os_StalluSec (TI_HANDLE OsContext, TI_UINT32 uSec)
{
	udelay (uSec);
}


/****************************************************************************************
 *                        																*
 *							Protection services	API										*
 *																						*
 ****************************************************************************************
 * OS protection is implemented as spin_lock_irqsave and spin_unlock_irqrestore  								*
 ****************************************************************************************/


/****************************************************************************************
 *                        os_protectCreate()                                 
 ****************************************************************************************
DESCRIPTION:	

ARGUMENTS:		OsContext - our adapter context.

RETURN:			A handle of the created mutex/spinlock.
				TI_HANDLE_INVALID if there is insufficient memory available or problems
				initializing the mutex

NOTES:         	
*****************************************************************************************/
TI_HANDLE os_protectCreate (TI_HANDLE OsContext)
{
	return NULL;
}


/****************************************************************************************
 *                        os_protectDestroy()                                 
 ****************************************************************************************
DESCRIPTION:		

ARGUMENTS:		OsContext - our adapter context.

RETURN:			None - This had better work since there is not a return value to the user

NOTES:         	
*****************************************************************************************/
void os_protectDestroy (TI_HANDLE OsContext, TI_HANDLE ProtectCtx)
{
}


/****************************************************************************************
 *                        os_protectLock()                                 
 ****************************************************************************************
DESCRIPTION:		

ARGUMENTS:		OsContext - our adapter context.

RETURN:			None - This had better work since there is not a return value to the user

NOTES:         	
*****************************************************************************************/
void os_protectLock (TI_HANDLE OsContext, TI_HANDLE ProtectContext)
{
	TWlanDrvIfObj *drv = (TWlanDrvIfObj *)OsContext;

	spin_lock_irqsave (&drv->lock, drv->flags);
}


/****************************************************************************************
 *                        os_protectUnlock()                                 
 ****************************************************************************************
DESCRIPTION:		

ARGUMENTS:		OsContext - our adapter context.

RETURN:			None - This had better work since there is not a return value to the user

NOTES:         	
*****************************************************************************************/
void os_protectUnlock (TI_HANDLE OsContext, TI_HANDLE ProtectContext)
{
    TWlanDrvIfObj *drv = (TWlanDrvIfObj *)OsContext;
    
    spin_unlock_irqrestore (&drv->lock, drv->flags);
}
/****************************************************************************************
 *                        os_receivePacket()                                 
 ****************************************************************************************
DESCRIPTION:		

ARGUMENTS:		

RETURN:			

NOTES:         	
*****************************************************************************************/
TI_BOOL os_receivePacket(TI_HANDLE OsContext, void *pRxDesc ,void *pPacket, TI_UINT16 Length)
{
   TWlanDrvIfObj  *drv     = (TWlanDrvIfObj *)OsContext;
   unsigned char  *pdata   = (unsigned char *)((TI_UINT32)pPacket & ~(TI_UINT32)0x3);
   rx_head_t      *rx_head = (rx_head_t *)(pdata -  WSPI_PAD_BYTES - RX_HEAD_LEN_ALIGNED);
   struct sk_buff *skb     = rx_head->skb;
	
#ifdef TI_DBG
   if ((TI_UINT32)pPacket & 0x3)
   {
     if ((TI_UINT32)pPacket - (TI_UINT32)skb->data != 2)
	 {
	   printk("os_receivePacket() address error skb=0x%x skb->data=0x%x pPacket=0x%x !!!\n",(int)skb, (int)skb->data, (int)pPacket);
	 }
   }
   else
   {
	 if ((TI_UINT32)skb->data != (TI_UINT32)pPacket)
	 {
	   printk("os_receivePacket() address error skb=0x%x skb->data=0x%x pPacket=0x%x !!!\n",(int)skb, (int)skb->data, (int)pPacket);
	 }
   }
   if (Length != RX_ETH_PKT_LEN(pPacket))
   {
	 printk("os_receivePacket() Length=%d !=  RX_ETH_PKT_LEN(pPacket)=%d!!!\n",(int)Length, RX_ETH_PKT_LEN(pPacket));
   }

#endif
/*
   printk("-->> os_receivePacket() pPacket=0x%x Length=%d skb=0x%x skb->data=0x%x skb->head=0x%x skb->len=%d\n",
		  (int)pPacket, (int)Length, (int)skb, (int)skb->data, (int)skb->head, (int)skb->len);
*/
   /* Use skb_reserve, it updates both skb->data and skb->tail. */
   skb->data = RX_ETH_PKT_DATA(pPacket);
   skb->tail = skb->data;
   skb_put(skb, RX_ETH_PKT_LEN(pPacket));
/*
   printk("-->> os_receivePacket() skb=0x%x skb->data=0x%x skb->head=0x%x skb->len=%d\n",
		  (int)skb, (int)skb->data, (int)skb->head, (int)skb->len);
*/
   ti_nodprintf(TIWLAN_LOG_INFO, "os_receivePacket - Received EAPOL len-%d\n", WBUF_LEN(pWbuf));

   skb->dev       = drv->netdev;
   skb->protocol  = eth_type_trans(skb, drv->netdev);
   skb->ip_summed = CHECKSUM_NONE;

   drv->stats.rx_packets++;
   drv->stats.rx_bytes += skb->len;

   /* Send the skb to the TCP stack. 
    * it responsibly of the Linux kernel to free the skb
    */
   {
       CL_TRACE_START_L1();

       os_wake_lock_timeout_enable(drv);

       netif_rx_ni(skb);

       /* Note: Don't change this trace (needed to exclude OS processing from Rx CPU utilization) */
       CL_TRACE_END_L1("tiwlan_drv.ko", "OS", "RX", "");
   }

   return TI_TRUE;
}

/*-----------------------------------------------------------------------------
  
Routine Name:  os_timerHandlr

Routine Description:

    Just a place holder for timer expiration handling in other OSs.
    In Linux, user callback is called directly on OS timer expiry.

Arguments:     parm - timer object handle

Return Value:  None.

Notes:

-----------------------------------------------------------------------------*/
void os_timerHandlr(unsigned long parm)
{
    /* Not needed in Linux (user callback is called directly on OS timer expiry). */
}


/*-----------------------------------------------------------------------------
Routine Name:  os_connectionStatus

Routine Description:

The eSTA-DK will call this API so the OS stack is aware that the
WLAN layer is ready to function.

Arguments:
cStatus = 1; WLAN in ready for network packets
cStatus = 0; WLAN in not ready for network packets

Return Value:  None
-----------------------------------------------------------------------------*/
TI_INT32 os_IndicateEvent (TI_HANDLE OsContext, IPC_EV_DATA* pData)
{
	IPC_EVENT_PARAMS *pInParam =  &pData->EvParams;
	TWlanDrvIfObj    *drv = (TWlanDrvIfObj *)OsContext;
	/*TI_UINT8 AuthBuf[sizeof(TI_UINT32) + sizeof(OS_802_11_AUTHENTICATION_REQUEST)];*/

	ti_nodprintf(TIWLAN_LOG_INFO, "\n  os_ConnectionStatus Event 0x%08x \n", CsStatus->Event);
   
	switch(pInParam->uEventType)
	{
		case IPC_EVENT_ASSOCIATED:
			if (drv->netdev != NULL)
            	netif_carrier_on(drv->netdev);
         	break;

       case IPC_EVENT_DISASSOCIATED:
         	if (drv->netdev != NULL)
        		netif_carrier_off(drv->netdev);
      		break;

      case IPC_EVENT_LINK_SPEED:
         	drv->tCommon.uLinkSpeed = (*(TI_UINT32*)pData->uBuffer * 10000) / 2;
         	ti_nodprintf(TIWLAN_LOG_INFO, "\n  Link Speed = 0x%08x \n",drv->tCommon.uLinkSpeed);
      		break;
   }

   return TI_OK;
}



/******************************************************************************/

void os_disableIrq (TI_HANDLE OsContext)
{
	TWlanDrvIfObj *drv = (TWlanDrvIfObj *)OsContext;
	disable_irq (drv->irq);
}

void os_enableIrq (TI_HANDLE OsContext)
{
	TWlanDrvIfObj *drv = (TWlanDrvIfObj *)OsContext;
	enable_irq (drv->irq);
}

/*-----------------------------------------------------------------------------
Routine Name:  os_InterruptServiced

Routine Description: Called when IRQ line is not asserted any more 
					(i.e. we can enable IRQ in Level sensitive platform)

Arguments:	OsContext - handle to OS context

Return Value:  none
-----------------------------------------------------------------------------*/
void os_InterruptServiced (TI_HANDLE OsContext)
{
	/* To be implemented with Level IRQ */
}

/*-----------------------------------------------------------------------------
Routine Name:  os_wake_lock_timeout

Routine Description: Called to prevent system from suspend for 1 sec

Arguments:     OsContext - handle to OS context

Return Value:  packet counter
-----------------------------------------------------------------------------*/
int os_wake_lock_timeout (TI_HANDLE OsContext)
{
	TWlanDrvIfObj *drv = (TWlanDrvIfObj *)OsContext;
	int ret = 0;
	unsigned long flags;

	if (drv) {
		spin_lock_irqsave(&drv->lock, flags);
		ret = drv->wl_packet;
		if (drv->wl_packet) {
			drv->wl_packet = 0;
#ifdef CONFIG_HAS_WAKELOCK
			wake_lock_timeout(&drv->wl_rxwake, HZ);
#endif
		}
		spin_unlock_irqrestore(&drv->lock, flags);
	}
	/* printk("%s: %d\n", __func__, ret); */
	return ret;
}

/*-----------------------------------------------------------------------------
Routine Name:  os_wake_lock_timeout_enable

Routine Description: Called to set flag for suspend prevention for some time

Arguments:     OsContext - handle to OS context

Return Value:  packet counter
-----------------------------------------------------------------------------*/
int os_wake_lock_timeout_enable (TI_HANDLE OsContext)
{
	TWlanDrvIfObj *drv = (TWlanDrvIfObj *)OsContext;
	unsigned long flags;
	int ret = 0;

	if (drv) {
		spin_lock_irqsave(&drv->lock, flags);
		ret = drv->wl_packet = 1;
		spin_unlock_irqrestore(&drv->lock, flags);
	}
	return ret;
}

/*-----------------------------------------------------------------------------
Routine Name:  os_wake_lock

Routine Description: Called to prevent system from suspend

Arguments:     OsContext - handle to OS context

Return Value:  wake_lock counter
-----------------------------------------------------------------------------*/
int os_wake_lock (TI_HANDLE OsContext)
{
	TWlanDrvIfObj *drv = (TWlanDrvIfObj *)OsContext;
	int ret = 0;
	unsigned long flags;

	if (drv) {
		spin_lock_irqsave(&drv->lock, flags);
#ifdef CONFIG_HAS_WAKELOCK
		if (!drv->wl_count)
			wake_lock(&drv->wl_wifi);
#endif
		drv->wl_count++;
		ret = drv->wl_count;
		spin_unlock_irqrestore(&drv->lock, flags);
	}
	/* printk("%s: %d\n", __func__, ret); */
	return ret;
}

/*-----------------------------------------------------------------------------
Routine Name:  os_wake_unlock

Routine Description: Called to allow system to suspend

Arguments:     OsContext - handle to OS context

Return Value:  wake_lock counter
-----------------------------------------------------------------------------*/
int os_wake_unlock (TI_HANDLE OsContext)
{
	TWlanDrvIfObj *drv = (TWlanDrvIfObj *)OsContext;
	int ret = 0;
	unsigned long flags;

	if (drv) {
		spin_lock_irqsave(&drv->lock, flags);
		if (drv->wl_count) {
			drv->wl_count--;
#ifdef CONFIG_HAS_WAKELOCK
			if (!drv->wl_count)
				wake_unlock(&drv->wl_wifi);
#endif
			ret = drv->wl_count;
		}
		spin_unlock_irqrestore(&drv->lock, flags);
	}
	/* printk("%s: %d\n", __func__, ret); */
	return ret;
}

/*-----------------------------------------------------------------------------
Routine Name:  os_RequestSchedule

Routine Description:

Arguments:

Return Value:  TI_OK
-----------------------------------------------------------------------------*/
int os_RequestSchedule (TI_HANDLE OsContext)
{
	TWlanDrvIfObj *drv = (TWlanDrvIfObj *)OsContext;

	/* Note: The performance trace below doesn't inclose the schedule
	 *   itself because the rescheduling can occur immediately and call
	 *   os_RequestSchedule again which will confuse the trace tools */
	CL_TRACE_START_L3();
	CL_TRACE_END_L3("tiwlan_drv.ko", "OS", "TASK", "");

	if( !queue_work(drv->tiwlan_wq, &drv->tWork) ) {
		/* printk("%s: Fail\n",__func__); */
		return TI_NOK;
	}

	return TI_OK;
}


/*-----------------------------------------------------------------------------
Routine Name: os_SignalObjectCreate

Routine Description:

Arguments:

Return Value: TI_OK
-----------------------------------------------------------------------------*/
void *os_SignalObjectCreate (TI_HANDLE OsContext)
{
	struct completion *myPtr;
	myPtr = os_memoryAlloc(OsContext, sizeof(struct completion));
	if (myPtr)
		init_completion (myPtr);
	return (myPtr);
}


/*-----------------------------------------------------------------------------
Routine Name: os_SignalObjectWait

Routine Description:

Arguments:

Return Value: TI_OK
-----------------------------------------------------------------------------*/
int os_SignalObjectWait (TI_HANDLE OsContext, void *signalObject)
{
	if (!signalObject)
		return TI_NOK;
	if (!wait_for_completion_timeout((struct completion *)signalObject,
					msecs_to_jiffies(10000))) {
		printk("tiwlan: 10 sec %s timeout\n", __func__);
	}
	return TI_OK;
}


/*-----------------------------------------------------------------------------
Routine Name: os_SignalObjectSet

Routine Description:

Arguments:

Return Value: TI_OK
-----------------------------------------------------------------------------*/
int os_SignalObjectSet (TI_HANDLE OsContext, void *signalObject)
{
	if (!signalObject)
		return TI_NOK;
	complete ((struct completion *)signalObject);
	return TI_OK;
}


/*-----------------------------------------------------------------------------
Routine Name: os_SignalObjectFree

Routine Description:

Arguments:

Return Value: TI_OK
-----------------------------------------------------------------------------*/
int os_SignalObjectFree (TI_HANDLE OsContext, void *signalObject)
{
	if (!signalObject)
		return TI_NOK;
	os_memoryFree(OsContext, signalObject, sizeof(struct completion));
	return TI_OK;
}


/** 
 * \fn     os_Trace
 * \brief  Prepare and send trace message to the logger.
 * 
 * \param  OsContext    - The OS handle
 * \param  uLevel   	- Severity level of the trace message
 * \param  uFileId  	- Source file ID of the trace message
 * \param  uLineNum 	- Line number of the trace message
 * \param  uParamsNum   - Number of parameters in the trace message
 * \param  ...          - The trace message parameters
 * 
 * \return void
 */
void os_Trace (TI_HANDLE OsContext, TI_UINT32 uLevel, TI_UINT32 uFileId, TI_UINT32 uLineNum, TI_UINT32 uParamsNum, ...)
{
	TI_UINT32	index;
	TI_UINT32	uParam;
	TI_UINT32	uMaxParamValue = 0;
	TI_UINT32	uMsgLen	= TRACE_MSG_MIN_LENGTH;
	TI_UINT8    aMsg[TRACE_MSG_MAX_LENGTH] = {0};
	TTraceMsg   *pMsgHdr  = (TTraceMsg *)&aMsg[0];
	TI_UINT8    *pMsgData = &aMsg[0] + sizeof(TTraceMsg);
	va_list	    list;

	if (!bRedirectOutputToLogger)
	{
		return;
	}

	if (uParamsNum > TRACE_MSG_MAX_PARAMS)
	{
		uParamsNum = TRACE_MSG_MAX_PARAMS;
	}

	/* sync on the parameters */
	va_start(list, uParamsNum);

	/* find the longest parameter */
	for (index = 0; index < uParamsNum; index++)
	{
		/* get parameter from the stack */
		uParam = va_arg (list, TI_UINT32);

		/* save the longest parameter at variable 'uMaxParamValue' */
		if (uParam > uMaxParamValue)
        {
			uMaxParamValue = uParam;
        }

		/* 32 bit parameter is the longest possible - get out of the loop */
		if (uMaxParamValue > UINT16_MAX_VAL)
        {
			break;
        }
	}

	/* Set msg length and format according to the biggest parameter value (8/16/32 bits) */
	if (uMaxParamValue > UINT16_MAX_VAL)		
	{
		pMsgHdr->uFormat = TRACE_FORMAT_32_BITS_PARAMS;
		uMsgLen += uParamsNum * sizeof(TI_UINT32);
	}
	else if (uMaxParamValue > UINT8_MAX_VAL)	
	{
		pMsgHdr->uFormat = TRACE_FORMAT_16_BITS_PARAMS;
		uMsgLen += uParamsNum * sizeof(TI_UINT16);
	}
	else							
	{
		pMsgHdr->uFormat = TRACE_FORMAT_8_BITS_PARAMS;
		uMsgLen += uParamsNum;
	}

	/* Fill all other header information */
	pMsgHdr->uLevel     = (TI_UINT8)uLevel;
	pMsgHdr->uParamsNum = (TI_UINT8)uParamsNum;
	pMsgHdr->uFileId    = (TI_UINT16)uFileId;
	pMsgHdr->uLineNum   = (TI_UINT16)uLineNum;

	/* re-sync on the parameters */
	va_start(list, uParamsNum);

	/* add the parameters */
	for (index = 0; index < uParamsNum; index++)
	{
		/* get parameter from the stack */
		uParam = va_arg(list, TI_UINT32);

		/* insert the parameter and increment msg pointer */
		switch(pMsgHdr->uFormat)
		{
		case TRACE_FORMAT_8_BITS_PARAMS:
			INSERT_BYTE(pMsgData, uParam);
			break;

		case TRACE_FORMAT_16_BITS_PARAMS:
			INSERT_2_BYTES(pMsgData, uParam);
			break;

		case TRACE_FORMAT_32_BITS_PARAMS:
			INSERT_4_BYTES(pMsgData, uParam);
			break;

        default:
            va_end(list);
            return;
		}     
	}

	va_end(list);

	/* Send the trace message to the logger */
	SendLoggerData(OsContext, aMsg, (TI_UINT16)uMsgLen);
}

/*--------------------------------------------------------------------------------------*/

/** 
 * \fn     os_SetDrvThreadPriority
 * \brief  Called upon init to set WLAN driver thread priority.
 *         Currently not supported in Linux.
 * 
 * \param  OsContext              - The OS handle
 * \param  uWlanDrvThreadPriority - The WLAN driver thread priority
 * \return 
 */
void os_SetDrvThreadPriority (TI_HANDLE OsContext, TI_UINT32 uWlanDrvThreadPriority)
{
}
