/*
 * FwEvent.c
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


/** \file  FwEvent.c
 *  \brief Handle firmware events
 * 
 *   
 * \par Description
 *      Call the appropriate event handler.
 *
 *  \see FwEvent.h
 */

#define __FILE_ID__  FILE_ID_104
#include "tidef.h"
#include "report.h"
#include "context.h"
#include "osApi.h"
#include "TWDriver.h"
#include "TWDriverInternal.h"
#include "txResult_api.h"
#include "CmdMBox_api.h"
#include "rxXfer_api.h" 
#include "txXfer_api.h" 
#include "txHwQueue_api.h"
#include "eventMbox_api.h"
#include "TwIf.h"
#include "public_host_int.h"
#include "FwEvent_api.h"
#ifdef TI_DBG
    #include "tracebuf_api.h"
#endif
#include "bmtrace_api.h"


#ifdef _VLCT_
extern int trigger_another_read;
#endif


/*
 * Address of FW-Status structure in FW memory ==> Special mapping, see note!!
 *
 * Note: This structure actually includes two separate areas in the FW:
 *          1) Interrupt-Status register - a 32 bit register (clear on read).
 *          2) FW-Status structure - 64 bytes memory area
 *       The two areas are read in a single transaction thanks to a special memory 
 *           partition that maps them as contiguous memory.
 */
#define FW_STATUS_ADDR           0x14FC0 + 0xA000


#define ALL_EVENTS_VECTOR        ACX_INTR_WATCHDOG | ACX_INTR_INIT_COMPLETE | ACX_INTR_EVENT_A |\
                                 ACX_INTR_EVENT_B | ACX_INTR_CMD_COMPLETE |ACX_INTR_HW_AVAILABLE |\
                                 ACX_INTR_DATA

#define TXN_FW_EVENT_SET_MASK_ADDR(pFwEvent)      pFwEvent->tMaskTxn.tTxnStruct.uHwAddr = HINT_MASK;
#define TXN_FW_EVENT_SET_FW_STAT_ADDR(pFwEvent)   pFwEvent->tFwStatusTxn.tTxnStruct.uHwAddr = FW_STATUS_ADDR;

#define UPDATE_PENDING_HANDLERS_NUMBER(eStatus)   if (eStatus == TXN_STATUS_PENDING) {pFwEvent->uNumPendHndlrs++;}


typedef enum
{
    FWEVENT_STATE_IDLE,    
    FWEVENT_STATE_WAIT_INTR_INFO,
    FWEVENT_STATE_WAIT_HANDLE_COMPLT

} EFwEventState;

typedef struct 
{
    TTxnStruct              tTxnStruct;
    TI_UINT32               uData; 

} TRegisterTxn;

typedef struct 
{
    TTxnStruct     tTxnStruct;
    FwStatus_t     tFwStatus;

} TFwStatusTxn;

/* The FwEvent module's main structure */
typedef struct 
{
    EFwEventState       eSmState;       /* State machine state */
    TI_UINT32           uEventMask;     /* Static interrupt event mask */
    TI_UINT32           uEventVector;   /* Saves the current active FW interrupts */
    TRegisterTxn        tMaskTxn;       /* The host mask register transaction */
    TFwStatusTxn        tFwStatusTxn;   /* The FW status structure transaction (read from FW memory) */
    TI_UINT32           uFwTimeOffset;  /* Offset in microseconds between driver and FW clocks */
    TI_UINT32           uContextId;     /* Client ID got upon registration to the context module */
    TI_BOOL             bIntrPending;   /* If TRUE a new interrupt is pending while handling the previous one */
    TI_UINT32           uNumPendHndlrs; /* Number of event handlers that didn't complete their event processing */

    /* Other modules handles */
    TI_HANDLE           hOs;                    	
    TI_HANDLE           hTWD;
    TI_HANDLE           hReport;                	
    TI_HANDLE           hContext;               	
    TI_HANDLE           hTwIf;                      
    TI_HANDLE           hHealthMonitor;             
    TI_HANDLE           hEventMbox;
    TI_HANDLE           hCmdMbox;
    TI_HANDLE           hRxXfer;
    TI_HANDLE           hTxXfer;
    TI_HANDLE           hTxHwQueue;
    TI_HANDLE           hTxResult;

} TfwEvent; 


static void       fwEvent_NewEvent       (TI_HANDLE hFwEvent);
static void       fwEvent_StateMachine   (TfwEvent *pFwEvent);
static ETxnStatus fwEvent_SmReadIntrInfo (TfwEvent *pFwEvent);
static ETxnStatus fwEvent_SmHandleEvents (TfwEvent *pFwEvent);
static ETxnStatus fwEvent_CallHandlers   (TfwEvent *pFwEvent);


/*
 * \brief	Create the FwEvent module object
 * 
 * \param  hOs  - OS module object handle
 * \return Handle to the created object
 * 
 * \par Description
 * Calling this function creates a FwEvent object
 * 
 * \sa fwEvent_Destroy
 */
TI_HANDLE fwEvent_Create (TI_HANDLE hOs)
{
    TfwEvent *pFwEvent;

    pFwEvent = os_memoryAlloc (hOs, sizeof(TfwEvent));
    if (pFwEvent == NULL)
    {
        return NULL;
    }

    os_memoryZero (hOs, pFwEvent, sizeof(TfwEvent));

    pFwEvent->hOs = hOs;

    return (TI_HANDLE)pFwEvent;
}


/*
 * \brief	Destroys the FwEvent object
 * 
 * \param  hFwEvent  - The object to free
 * \return TI_OK
 * 
 * \par Description
 * Calling this function destroys a FwEvent object
 * 
 * \sa fwEvent_Create
 */
TI_STATUS fwEvent_Destroy (TI_HANDLE hFwEvent)
{
    TfwEvent *pFwEvent = (TfwEvent *)hFwEvent;

    if (pFwEvent)
    {
        os_memoryFree (pFwEvent->hOs, pFwEvent, sizeof(TfwEvent));
    }

    return TI_OK;
}


/*
 * \brief	Config the FwEvent module object
 * 
 * \param  hFwEvent  - FwEvent Driver handle
 * \param  hTWD  - Handle to TWD module
 * \return TI_OK
 * 
 * \par Description
 * From hTWD we extract : hOs, hReport, hTwIf, hContext,
 *      hHealthMonitor, hEventMbox, hCmdMbox, hRxXfer, 
 *      hTxHwQueue, hTxResult
 * In this function we also register the FwEvent to the context engine
 * 
 * \sa
 */
TI_STATUS fwEvent_Init (TI_HANDLE hFwEvent, TI_HANDLE hTWD)
{
    TfwEvent  *pFwEvent = (TfwEvent *)hFwEvent;
    TTwd      *pTWD = (TTwd *)hTWD;
    TTxnStruct* pTxn;
    
    pFwEvent->hTWD              = hTWD;
    pFwEvent->hOs               = pTWD->hOs;
    pFwEvent->hReport           = pTWD->hReport;
    pFwEvent->hContext          = pTWD->hContext;
    pFwEvent->hTwIf             = pTWD->hTwIf;
    pFwEvent->hHealthMonitor    = pTWD->hHealthMonitor;
    pFwEvent->hEventMbox        = pTWD->hEventMbox;
    pFwEvent->hCmdMbox          = pTWD->hCmdMbox;
    pFwEvent->hRxXfer           = pTWD->hRxXfer;
    pFwEvent->hTxHwQueue        = pTWD->hTxHwQueue;
    pFwEvent->hTxXfer           = pTWD->hTxXfer;
    pFwEvent->hTxResult         = pTWD->hTxResult;

    pFwEvent->eSmState          = FWEVENT_STATE_IDLE;
    pFwEvent->bIntrPending      = TI_FALSE;
    pFwEvent->uNumPendHndlrs    = 0;
    pFwEvent->uEventMask        = 0;
    pFwEvent->uEventVector      = 0;

    /* Prepare Interrupts Mask regiter Txn structure */
    /* 
     * Note!!: The mask transaction is sent in low priority because it is used in the 
     *           init process which includes a long sequence of low priority transactions,
     *           and the order of this sequence is important so we must use the same priority
     */
    pTxn = (TTxnStruct*)&pFwEvent->tMaskTxn.tTxnStruct;
    TXN_PARAM_SET(pTxn, TXN_LOW_PRIORITY, TXN_FUNC_ID_WLAN, TXN_DIRECTION_WRITE, TXN_INC_ADDR)
    BUILD_TTxnStruct(pTxn, HINT_MASK, &pFwEvent->tMaskTxn.uData, REGISTER_SIZE, NULL, NULL)

    /* Prepare FW status Txn structure (includes 4 bytes interrupt status reg and 64 bytes FW-status from memory area) */
    /* Note: This is the only transaction that is sent in high priority.
     *       The original reason was to lower the interrupt latency, but we may consider using the 
     *         same priority as all other transaction for simplicity.
     */
    pTxn = (TTxnStruct*)&pFwEvent->tFwStatusTxn.tTxnStruct;
    TXN_PARAM_SET(pTxn, TXN_HIGH_PRIORITY, TXN_FUNC_ID_WLAN, TXN_DIRECTION_READ, TXN_INC_ADDR)
    BUILD_TTxnStruct(pTxn, FW_STATUS_ADDR, &pFwEvent->tFwStatusTxn.tFwStatus, sizeof(FwStatus_t), (TTxnDoneCb)fwEvent_StateMachine, hFwEvent)

    /* 
     *  Register the FwEvent to the context engine and get the client ID.
     *  The FwEvent() will be called from the context_DriverTask() after scheduled
     *    by a FW-Interrupt (see fwEvent_InterruptRequest()).
     */
    pFwEvent->uContextId = context_RegisterClient (pFwEvent->hContext,
                                                   fwEvent_NewEvent,
                                                   hFwEvent,
                                                   TI_FALSE,
                                                   "FW_EVENT",
                                                   sizeof("FW_EVENT"));
	
    return TI_OK;
}


/*
 * \brief	FW interrupt handler, just switch to WLAN context for handling
 * 
 * \param   hFwEvent - FwEvent Driver handle
 * \return  void
 * 
 * \par Description
 * Called by the FW-Interrupt ISR (external context!).
 * Requests the context engine to schedule the driver task for handling the FW-Events.
 * 
 * \sa
 */
void fwEvent_InterruptRequest (TI_HANDLE hFwEvent)
{
    TfwEvent *pFwEvent = (TfwEvent *)hFwEvent;
    CL_TRACE_START_L1();

    TRACE0(pFwEvent->hReport, REPORT_SEVERITY_INFORMATION, "fwEvent_InterruptRequest()\n");

    /* Request switch to driver context for handling the FW-Interrupt event */
    context_RequestSchedule (pFwEvent->hContext, pFwEvent->uContextId);

    CL_TRACE_END_L1("tiwlan_drv.ko", "IRQ", "FwEvent", "");
}


/*
 * \brief   The CB called in the driver context upon new interrupt
 * 
 * \param   hFwEvent - FwEvent Driver handle
 * \return  void
 * 
 * \par Description
 * Called by the context module after scheduled by fwEvent_InterruptRequest().
 * If IDLE, start the SM, and if not just indicate pending event for later.
 * 
 * \sa
 */
static void fwEvent_NewEvent (TI_HANDLE hFwEvent)
{
    TfwEvent *pFwEvent = (TfwEvent *)hFwEvent;
    CL_TRACE_START_L2();

    /* If the SM is idle, call it to start handling new events */
    if (pFwEvent->eSmState == FWEVENT_STATE_IDLE) 
    {
        TRACE0(pFwEvent->hReport, REPORT_SEVERITY_INFORMATION, "fwEvent_NewEvent: Start SM\n");

        fwEvent_StateMachine (pFwEvent);
    }
    /* Else - SM is busy so set flag to handle it when finished with current events */
    else 
    {
        TRACE0(pFwEvent->hReport, REPORT_SEVERITY_INFORMATION, "fwEvent_NewEvent: SM busy, set IntrPending flag\n");

        pFwEvent->bIntrPending = TI_TRUE;
    }

    CL_TRACE_END_L2("tiwlan_drv.ko", "CONTEXT", "FwEvent", "");
}


/*
 * \brief	FW-Event state machine
 * 
 * \param  hFwEvent  - FwEvent Driver handle
 * \return void
 * 
 * \par Description
 * 
 * Process the current FW events in a sequence that may progress in the same context,
 *     or exit if pending an Async transaction, which will call back the SM when finished.
 *
 * \sa
 */
static void fwEvent_StateMachine (TfwEvent *pFwEvent)
{
    ETxnStatus  eStatus = TXN_STATUS_ERROR; /* Set to error to detect if used uninitialized */
    CL_TRACE_START_L3();

	/* 
	 * Loop through the states sequence as long as the process is synchronous.
	 * Exit when finished or if an Asynchronous process is required. 
     * In this case the SM will be called back upon Async operation completion. 
	 */
	while (1)
	{
		switch (pFwEvent->eSmState)
		{
            /* IDLE: Update TwIf and read interrupt info from FW */
            case FWEVENT_STATE_IDLE:
            {
                CL_TRACE_START_L5();
                twIf_Awake(pFwEvent->hTwIf);
                eStatus = fwEvent_SmReadIntrInfo (pFwEvent);
                pFwEvent->eSmState = FWEVENT_STATE_WAIT_INTR_INFO;
                CL_TRACE_END_L5("tiwlan_drv.ko", "CONTEXT", "FwEvent", ".ReadInfo");
                break;
            }
            /* WAIT_INTR_INFO: We have the interrupt info so call the handlers accordingly */
            case FWEVENT_STATE_WAIT_INTR_INFO:
            {
                CL_TRACE_START_L5();
                eStatus = fwEvent_SmHandleEvents (pFwEvent);
                /* If state was changed to IDLE by recovery or stop process, exit (process terminated) */
                if (pFwEvent->eSmState == FWEVENT_STATE_IDLE) 
                {
                    CL_TRACE_END_L5("tiwlan_drv.ko", "CONTEXT", "FwEvent", ".HndlEvents");
                    CL_TRACE_END_L3("tiwlan_drv.ko", "CONTEXT", "FwEvent", "");
                    return;
                }
                pFwEvent->eSmState = FWEVENT_STATE_WAIT_HANDLE_COMPLT;
                CL_TRACE_END_L5("tiwlan_drv.ko", "CONTEXT", "FwEvent", ".HndlEvents");
                break;
            }
            /* WAIT_HANDLE_COMPLT: Current handling is completed. */
            case FWEVENT_STATE_WAIT_HANDLE_COMPLT:
            {
                /* If pending interrupt, read interrupt info (back to WAIT_INTR_INFO state) */
                if (pFwEvent->bIntrPending) 
                {
                    CL_TRACE_START_L5();
                    pFwEvent->bIntrPending = TI_FALSE;
                    eStatus = fwEvent_SmReadIntrInfo (pFwEvent);
                    pFwEvent->eSmState = FWEVENT_STATE_WAIT_INTR_INFO;
                    CL_TRACE_END_L5("tiwlan_drv.ko", "CONTEXT", "FwEvent", ".HndlCmplt");
                }
                /* Else - all done so release TwIf to sleep and exit */
                else 
                {
                    twIf_Sleep(pFwEvent->hTwIf);
                    pFwEvent->eSmState = FWEVENT_STATE_IDLE;

                    TRACE3(pFwEvent->hReport, REPORT_SEVERITY_INFORMATION, "fwEvent_StateMachine: Completed, NewState=%d, Status=%d, IntrPending=%d\n", pFwEvent->eSmState, eStatus, pFwEvent->bIntrPending);
                    CL_TRACE_END_L3("tiwlan_drv.ko", "CONTEXT", "FwEvent", "");

                    /**** Finished all current events handling so exit ****/
                    return;
                }
                break;
            }

        }  /* switch */

        TRACE3(pFwEvent->hReport, REPORT_SEVERITY_INFORMATION, "fwEvent_StateMachine: NewState=%d, Status=%d, IntrPending=%d\n", pFwEvent->eSmState, eStatus, pFwEvent->bIntrPending);
        
		/* If last status is Pending, exit the SM (to be called back upon Async operation completion) */
		if (eStatus == TXN_STATUS_PENDING)
		{
            CL_TRACE_END_L3("tiwlan_drv.ko", "CONTEXT", "FwEvent", "");
			return;
		}

        /* If error occured, stop the process and exit (should be cleaned by recovery process) */
		else if (eStatus == TXN_STATUS_ERROR)
		{
            TRACE5(pFwEvent->hReport, REPORT_SEVERITY_ERROR, "fwEvent_StateMachine: NewState=%d, Status=%d, IntrPending=%d, EventVector=0x%x, EventMask=0x%x\n", pFwEvent->eSmState, eStatus, pFwEvent->bIntrPending, pFwEvent->uEventVector, pFwEvent->uEventMask);
            CL_TRACE_END_L3("tiwlan_drv.ko", "CONTEXT", "FwEvent", "");
            fwEvent_Stop ((TI_HANDLE)pFwEvent);
			return;
		}

        /* If we got here the status is COMPLETE so continue in the while loop to the next state */

	}  /* while */
}


/*
 * \brief	Read interrupt info from FW
 * 
 * \param   hFwEvent  - FwEvent Driver handle
 * \return  void
 * 
 * \par Description
 * 
 * Indicate the TwIf that HW is available and initiate transactions for reading
 *     the Interrupt status and the FW status.
 *
 * \sa
 */
static ETxnStatus fwEvent_SmReadIntrInfo (TfwEvent *pFwEvent)
{
    ETxnStatus eStatus;
    CL_TRACE_START_L4();

#ifdef HOST_INTR_MODE_EDGE
    /* Acknowledge the host interrupt for EDGE mode (must be before HINT_STT_CLR register clear on read) */
    os_InterruptServiced (pFwEvent->hOs);
#endif

    /* Indicate that the chip is awake (since it interrupted us) */
    twIf_HwAvailable(pFwEvent->hTwIf);

    /*
     * Read FW-Status structure from HW ==> Special mapping, see note!!
     *
     * Note: This structure actually includes two separate areas in the FW:
     *          1) Interrupt-Status register - a 32 bit register (clear on read).
     *          2) FW-Status structure - 64 bytes memory area
     *       The two areas are read in a single transaction thanks to a special memory 
     *           partition that maps them as contiguous memory.
     */
    TXN_FW_EVENT_SET_FW_STAT_ADDR(pFwEvent)
    eStatus = twIf_TransactReadFWStatus (pFwEvent->hTwIf, &(pFwEvent->tFwStatusTxn.tTxnStruct));

    CL_TRACE_END_L4("tiwlan_drv.ko", "CONTEXT", "FwEvent", "");

    /* Return the status of the FwStatus read (complete, pending or error) */
    return eStatus;
}


/*
 * \brief	Handle the Fw Status information 
 * 
 * \param  hFwEvent  - FwEvent Driver handle
 * \return void
 * 
 * \par Description
 * This function is called from fwEvent_Handle on a sync read, or from TwIf as a CB on an async read.
 * It calls fwEvent_CallHandlers to handle the triggered interrupts.
 * 
 * \sa fwEvent_Handle
 */
static ETxnStatus fwEvent_SmHandleEvents (TfwEvent *pFwEvent)
{
    ETxnStatus eStatus;
    CL_TRACE_START_L4();

    /* Save delta between driver and FW time (needed for Tx packets lifetime) */
    pFwEvent->uFwTimeOffset = (os_timeStampMs (pFwEvent->hOs) * 1000) - 
                              ENDIAN_HANDLE_LONG (pFwEvent->tFwStatusTxn.tFwStatus.fwLocalTime);

#ifdef HOST_INTR_MODE_LEVEL
    /* Acknowledge the host interrupt for LEVEL mode (must be after HINT_STT_CLR register clear on read) */
    os_InterruptServiced (pFwEvent->hOs);
#endif

    /* Save the interrupts status retreived from the FW */
    pFwEvent->uEventVector = pFwEvent->tFwStatusTxn.tFwStatus.intrStatus;

    /* Mask unwanted interrupts */
    pFwEvent->uEventVector &= pFwEvent->uEventMask;

    /* Call the interrupts handlers */
    eStatus = fwEvent_CallHandlers (pFwEvent);

    TRACE5(pFwEvent->hReport, REPORT_SEVERITY_INFORMATION, "fwEvent_SmHandleEvents: Status=%d, EventVector=0x%x, IntrPending=%d, NumPendHndlrs=%d, FwTimeOfst=%d\n", eStatus, pFwEvent->uEventVector, pFwEvent->bIntrPending, pFwEvent->uNumPendHndlrs, pFwEvent->uFwTimeOffset);
    CL_TRACE_END_L4("tiwlan_drv.ko", "CONTEXT", "FwEvent", "");

    /* Return the status of the handlers processing (complete, pending or error) */
    return eStatus;
} 


/*
 * \brief	Call FwEvent clients event handlers
 * 
 * \param  hFwEvent  - FwEvent Driver handle
 * \return void
 * 
 * \par Description
 * 
 * \sa
 */
static ETxnStatus fwEvent_CallHandlers (TfwEvent *pFwEvent)
{
    ETxnStatus eStatus;
    CL_TRACE_START_L4();

    pFwEvent->uNumPendHndlrs = 0;

    if (pFwEvent->uEventVector & ACX_INTR_WATCHDOG)
    {
        /* Fw watchdog timeout has occured */
        eStatus = TWD_WdExpireEvent (pFwEvent->hTWD);
        UPDATE_PENDING_HANDLERS_NUMBER(eStatus)
    }

    if (pFwEvent->uEventVector & ACX_INTR_INIT_COMPLETE)
    {
        TRACE0(pFwEvent->hReport, REPORT_SEVERITY_INFORMATION, "fwEvent_CallHandlers: INIT_COMPLETE\n");
    }
	/* Note: Handle Cmd-MBOX before Event-MBOX to keep command response and command complete order (for WHA) */
    if (pFwEvent->uEventVector & ACX_INTR_CMD_COMPLETE)
    {
        /* Command Mbox completed */
        eStatus = cmdMbox_CommandComplete(pFwEvent->hCmdMbox);
        UPDATE_PENDING_HANDLERS_NUMBER(eStatus)
    }
    if (pFwEvent->uEventVector & ACX_INTR_EVENT_A)
    {
        eStatus = eventMbox_Handle(pFwEvent->hEventMbox,&pFwEvent->tFwStatusTxn.tFwStatus);
        UPDATE_PENDING_HANDLERS_NUMBER(eStatus)
    }
    if (pFwEvent->uEventVector & ACX_INTR_EVENT_B)
    {
        eStatus = eventMbox_Handle(pFwEvent->hEventMbox,&pFwEvent->tFwStatusTxn.tFwStatus);
        UPDATE_PENDING_HANDLERS_NUMBER(eStatus)
    }
    
    /* The DATA interrupt is shared by all data path events, so call all Tx and Rx clients */
    if (pFwEvent->uEventVector & ACX_INTR_DATA)
    {
        eStatus = rxXfer_RxEvent (pFwEvent->hRxXfer, &pFwEvent->tFwStatusTxn.tFwStatus);
        UPDATE_PENDING_HANDLERS_NUMBER(eStatus)

        eStatus = txHwQueue_UpdateFreeResources (pFwEvent->hTxHwQueue, &pFwEvent->tFwStatusTxn.tFwStatus);
        UPDATE_PENDING_HANDLERS_NUMBER(eStatus)

        eStatus = txResult_TxCmpltIntrCb (pFwEvent->hTxResult, &pFwEvent->tFwStatusTxn.tFwStatus);
        UPDATE_PENDING_HANDLERS_NUMBER(eStatus)
    } 

    CL_TRACE_END_L4("tiwlan_drv.ko", "CONTEXT", "FwEvent", "");

    /* Return COMPLETE if all handlers completed, and PENDING if not. */
    return ((pFwEvent->uNumPendHndlrs == 0) ? TXN_STATUS_COMPLETE : TXN_STATUS_PENDING);
}


/*
 * \brief	Called by any handler that completed after pending
 * 
 * \param  hFwEvent  - FwEvent Driver handle
 *
 * \par Description
 *
 * Decrement pending handlers counter and if 0 call the SM to complete its process.
 * 
 * \sa
 */
void fwEvent_HandlerCompleted (TI_HANDLE hFwEvent)
{
    TfwEvent *pFwEvent = (TfwEvent *)hFwEvent;

#ifdef TI_DBG
    TRACE2(pFwEvent->hReport, REPORT_SEVERITY_INFORMATION, "fwEvent_HandlerCompleted: state=%d, NumPendHndlrs=%d\n", pFwEvent->eSmState, pFwEvent->uNumPendHndlrs);
    /* Verify that we really have pending handlers, otherwise it an error */
    if (pFwEvent->uNumPendHndlrs == 0) 
    {
        TRACE0(pFwEvent->hReport, REPORT_SEVERITY_ERROR, "fwEvent_HandlerCompleted: Called while no handlers are pending\n");
        return;
    }
    /* Verify that we are in */
    if (pFwEvent->eSmState != FWEVENT_STATE_WAIT_HANDLE_COMPLT) 
    {
        TRACE1(pFwEvent->hReport, REPORT_SEVERITY_ERROR, "fwEvent_HandlerCompleted: Called while not in WAIT_HANDLE_COMPLT state (state=%d)\n", pFwEvent->eSmState);
        return;
    }
#endif

    /* Decrement the pending handlers counter and if zero call the SM to complete the process */
    pFwEvent->uNumPendHndlrs--;
    if (pFwEvent->uNumPendHndlrs == 0) 
    {
        fwEvent_StateMachine (pFwEvent);
    }
}


/*
 * \brief	Translate host to FW time (Usec)
 * 
 * \param  hFwEvent  - FwEvent Driver handle
 * \param  uHostTime - The host time in MS to translate
 *
 * \return FW Time in Usec
 * 
 * \par Description
 * 
 * \sa
 */
TI_UINT32 fwEvent_TranslateToFwTime (TI_HANDLE hFwEvent, TI_UINT32 uHostTime)
{
    TfwEvent *pFwEvent = (TfwEvent *)hFwEvent;

    return ((uHostTime * 1000) - pFwEvent->uFwTimeOffset);
}


/*
 * \brief	Unmask only cmd-cmplt and events interrupts (needed for init phase)
 * 
 * \param  hFwEvent  - FwEvent Driver handle
 * \return Event mask
 * 
 * \par Description
 * Unmask only cmd-cmplt and events interrupts (needed for init phase).
 * 
 * \sa
 */
void fwEvent_SetInitMask (TI_HANDLE hFwEvent)
{
    TfwEvent *pFwEvent = (TfwEvent *)hFwEvent;

    /* Unmask only the interrupts needed for the FW configuration process. */
    pFwEvent->uEventMask = ACX_INTR_CMD_COMPLETE | ACX_INTR_EVENT_A | ACX_INTR_EVENT_B;
    pFwEvent->tMaskTxn.uData = ~pFwEvent->uEventMask;
    TXN_FW_EVENT_SET_MASK_ADDR(pFwEvent)
    twIf_Transact(pFwEvent->hTwIf, &(pFwEvent->tMaskTxn.tTxnStruct));
}


/*
 * \brief	Stop & reset FwEvent (called by the driver stop process)
 * 
 * \param  hFwEvent  - FwEvent Driver handle
 * \return TI_OK
 * 
 * \par Description
 *
 * \sa
 */
TI_STATUS fwEvent_Stop (TI_HANDLE hFwEvent)
{
    TfwEvent *pFwEvent = (TfwEvent *)hFwEvent;

    pFwEvent->eSmState       = FWEVENT_STATE_IDLE;
    pFwEvent->bIntrPending  = TI_FALSE;
    pFwEvent->uNumPendHndlrs = 0;
    pFwEvent->uEventMask     = 0;
    pFwEvent->uEventVector   = 0;    
    
    return TI_OK;
}


/*
 * \brief	Unmask all interrupts
 * 
 * \param  hFwEvent  - FwEvent Driver handle
 * \return void
 * 
 * \par Description
 *
 * Called after driver Start or Recovery process are completed.
 * Unmask all interrupts.
 * 
 * \sa
 */
void fwEvent_EnableExternalEvents (TI_HANDLE hFwEvent)
{
    TfwEvent *pFwEvent = (TfwEvent *)hFwEvent;

    /* Unmask all interrupts */
    pFwEvent->uEventMask = ALL_EVENTS_VECTOR;
    pFwEvent->tMaskTxn.uData = ~pFwEvent->uEventMask;
    TXN_FW_EVENT_SET_MASK_ADDR(pFwEvent)
    twIf_Transact(pFwEvent->hTwIf, &(pFwEvent->tMaskTxn.tTxnStruct));
}


/*
 * \brief	Disable the FwEvent client in the context handler
 * 
 * \param  hFwEvent  - FwEvent Driver handle
 * \return void
 * 
 * \par Description
 *
 * \sa
 */
void fwEvent_DisableInterrupts(TI_HANDLE hFwEvent)
{
    TfwEvent  *pFwEvent = (TfwEvent *)hFwEvent;

    context_DisableClient (pFwEvent->hContext,pFwEvent->uContextId);
}


/*
 * \brief	Enable the FwEvent client in the context handler
 * 
 * \param  hFwEvent  - FwEvent Driver handle
 * \return void
 * 
 * \par Description
 *
 * \sa
 */
void fwEvent_EnableInterrupts(TI_HANDLE hFwEvent)
{
    TfwEvent  *pFwEvent = (TfwEvent *)hFwEvent;

    context_EnableClient (pFwEvent->hContext,pFwEvent->uContextId);
}


#ifdef TI_DBG

void fwEvent_PrintStat (TI_HANDLE hFwEvent)
{
#ifdef REPORT_LOG
    TfwEvent *pFwEvent = (TfwEvent *)hFwEvent;

    WLAN_OS_REPORT(("Print FW event module info\n"));
    WLAN_OS_REPORT(("==========================\n"));
    WLAN_OS_REPORT(("uEventVector   = 0x%x\n", pFwEvent->uEventVector));
    WLAN_OS_REPORT(("uEventMask     = 0x%x\n", pFwEvent->uEventMask));
    WLAN_OS_REPORT(("eSmState       = %d\n",   pFwEvent->eSmState));
    WLAN_OS_REPORT(("bIntrPending   = %d\n",   pFwEvent->bIntrPending));
    WLAN_OS_REPORT(("uNumPendHndlrs = %d\n",   pFwEvent->uNumPendHndlrs));
    WLAN_OS_REPORT(("uFwTimeOffset  = %d\n",   pFwEvent->uFwTimeOffset));
#endif
}

#endif  /* TI_DBG */



