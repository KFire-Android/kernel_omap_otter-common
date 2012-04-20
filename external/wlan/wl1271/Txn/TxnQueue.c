/*
 * TxnQueue.c
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

 
/** \file   TxnQueue.c 
 *  \brief  The transaction-queue module. 
 *
 * The Transaction Queue encapsulates the bus access from a functional driver (WLAN, BT).
 * This TI proprietary module presents the same interface and same behavior for different 
 *     bus configuration: SDIO (multi or single function) or SPI and for different modes 
 *     of operation: Synchronous, a-synchronous or combination of both. 
 * It will also be used over the RS232 interface (using wUART protocol) which is applicable 
 *     for RS applications (on PC).
 * 
 * The TxnQ module provides the following requirements:
 *     Inter process protection on queue's internal database and synchronization between 
 *         functional drivers that share the bus.
 *     Support multiple queues per function, handled by priority. 
 *     Support the TTxnStruct API (as the Bus Driver) with the ability to manage commands 
 *         queuing of multiple functions on top of the Bus Driver. 
 *     The TxnQ (as well as the layers above it) is agnostic to the bus driver used beneath it 
 *         (SDIO, WSPI or wUART), since all bus drivers introduce the same API and hide bus details. 
 *     The TxnQ has no OS dependencies. It supports access from multiple OS threads.
 * Note: It is assumed that any transaction forwarded to the TxnQ has enough resources in HW. 
 * 
 *  \see    TxnQueue.h
 */

#define __FILE_ID__  FILE_ID_123
#include "tidef.h"
#include "report.h"
#include "context.h"
#include "osApi.h"
#include "TxnDefs.h"
#include "BusDrv.h"
#include "TxnQueue.h"



/************************************************************************
 * Defines
 ************************************************************************/
#define MAX_FUNCTIONS       4   /* Maximum 4 functional drivers (including Func 0 which is for bus control) */
#define MAX_PRIORITY        2   /* Maximum 2 prioritys per functional driver */
#define TXN_QUE_SIZE        QUE_UNLIMITED_SIZE
#define TXN_DONE_QUE_SIZE   QUE_UNLIMITED_SIZE


/************************************************************************
 * Types
 ************************************************************************/

/* Functional driver's SM States */
typedef enum
{
    FUNC_STATE_NONE,              /* Function not registered */
	FUNC_STATE_STOPPED,           /* Queues are stopped */
	FUNC_STATE_RUNNING,           /* Queues are running */
	FUNC_STATE_RESTART            /* Wait for current Txn to finish before restarting queues */
} EFuncState;

/* The functional drivers registered to TxnQ */
typedef struct 
{
    EFuncState      eState;             /* Function crrent state */
    TI_UINT32       uNumPrios;          /* Number of queues (priorities) for this function */
	TTxnQueueDoneCb fTxnQueueDoneCb;    /* The CB called by the TxnQueue upon full transaction completion. */
	TI_HANDLE       hCbHandle;          /* The callback handle */
    TTxnStruct *    pSingleStep;        /* A single step transaction waiting to be sent */

} TFuncInfo;


/* The TxnQueue module Object */
typedef struct _TTxnQObj
{
    TI_HANDLE	    hOs;		   	 
    TI_HANDLE	    hReport;
    TI_HANDLE	    hContext;
	TI_HANDLE	    hBusDrv;

    TFuncInfo       aFuncInfo[MAX_FUNCTIONS];  /* Registered functional drivers - see above */
    TI_HANDLE       aTxnQueues[MAX_FUNCTIONS][MAX_PRIORITY];  /* Handle of the Transactions-Queue */
    TI_HANDLE       hTxnDoneQueue;      /* Queue for completed transactions not reported to yet to the upper layer */
    TTxnStruct *    pCurrTxn;           /* The transaction currently processed in the bus driver (NULL if none) */
    TI_UINT32       uMinFuncId;         /* The minimal function ID actually registered (through txnQ_Open) */
    TI_UINT32       uMaxFuncId;         /* The maximal function ID actually registered (through txnQ_Open) */
    TI_BOOL         bSchedulerBusy;     /* If set, the scheduler is currently running so it shouldn't be reentered */
    TI_BOOL         bSchedulerPend;     /* If set, a call to the scheduler was postponed because it was busy */
    
    /* Environment dependent: TRUE if needed and allowed to protect TxnDone in critical section */
    TTxnDoneCb      fConnectCb;
    TI_HANDLE       hConnectCb;

#ifdef TI_DBG
    TI_HANDLE       pAggregQueue;       /* While Tx aggregation in progress, saves its queue pointer to ensure continuity */
#endif

} TTxnQObj;


/************************************************************************
 * Internal functions prototypes
 ************************************************************************/
static void         txnQ_TxnDoneCb    (TI_HANDLE hTxnQ, void *hTxn);
static ETxnStatus   txnQ_RunScheduler (TTxnQObj *pTxnQ, TTxnStruct *pInputTxn);
static ETxnStatus   txnQ_Scheduler    (TTxnQObj *pTxnQ, TTxnStruct *pInputTxn);
static TTxnStruct  *txnQ_SelectTxn    (TTxnQObj *pTxnQ);
static void         txnQ_ConnectCB    (TI_HANDLE hTxnQ, void *hTxn);



/************************************************************************
 *
 *   Module functions implementation
 *
 ************************************************************************/

TI_HANDLE txnQ_Create (TI_HANDLE hOs)
{
    TI_HANDLE  hTxnQ;
    TTxnQObj  *pTxnQ;
    TI_UINT32  i;

    hTxnQ = os_memoryAlloc(hOs, sizeof(TTxnQObj));
    if (hTxnQ == NULL)
        return NULL;
    
    pTxnQ = (TTxnQObj *)hTxnQ;

    os_memoryZero(hOs, hTxnQ, sizeof(TTxnQObj));
    
    pTxnQ->hOs             = hOs;
    pTxnQ->pCurrTxn        = NULL;
    pTxnQ->uMinFuncId      = MAX_FUNCTIONS; /* Start at maximum and save minimal value in txnQ_Open */
    pTxnQ->uMaxFuncId      = 0;             /* Start at minimum and save maximal value in txnQ_Open */
#ifdef TI_DBG
    pTxnQ->pAggregQueue    = NULL;
#endif

    for (i = 0; i < MAX_FUNCTIONS; i++)
    {
        pTxnQ->aFuncInfo[i].eState          = FUNC_STATE_NONE;
        pTxnQ->aFuncInfo[i].uNumPrios       = 0;
        pTxnQ->aFuncInfo[i].pSingleStep     = NULL;
        pTxnQ->aFuncInfo[i].fTxnQueueDoneCb = NULL;
        pTxnQ->aFuncInfo[i].hCbHandle       = NULL;
    }
    
    /* Create the Bus-Driver module */
    pTxnQ->hBusDrv = busDrv_Create (hOs);
    if (pTxnQ->hBusDrv == NULL)
    {
        WLAN_OS_REPORT(("%s: Error - failed to create BusDrv\n", __FUNCTION__));
        txnQ_Destroy (hTxnQ);
        return NULL;
    }

    return pTxnQ;
}

TI_STATUS txnQ_Destroy (TI_HANDLE hTxnQ)
{
    TTxnQObj *pTxnQ = (TTxnQObj*)hTxnQ;

    if (pTxnQ)
    {
        if (pTxnQ->hBusDrv) 
        {
            busDrv_Destroy (pTxnQ->hBusDrv);
        }
        if (pTxnQ->hTxnDoneQueue) 
        {
            que_Destroy (pTxnQ->hTxnDoneQueue);
        }
        os_memoryFree (pTxnQ->hOs, pTxnQ, sizeof(TTxnQObj));     
    }
    return TI_OK;
}

void txnQ_Init (TI_HANDLE hTxnQ, TI_HANDLE hOs, TI_HANDLE hReport, TI_HANDLE hContext)
{
    TTxnQObj  *pTxnQ = (TTxnQObj*)hTxnQ;
    TI_UINT32  uNodeHeaderOffset;

    pTxnQ->hOs             = hOs;
    pTxnQ->hReport         = hReport;
    pTxnQ->hContext        = hContext;

    /* Create the TxnDone queue. */
    uNodeHeaderOffset = TI_FIELD_OFFSET(TTxnStruct, tTxnQNode); 
    pTxnQ->hTxnDoneQueue = que_Create (pTxnQ->hOs, pTxnQ->hReport, TXN_DONE_QUE_SIZE, uNodeHeaderOffset);
    if (pTxnQ->hTxnDoneQueue == NULL)
    {
        TRACE0(pTxnQ->hReport, REPORT_SEVERITY_ERROR, ": TxnDone queue creation failed!\n");
    }

    busDrv_Init (pTxnQ->hBusDrv, hReport);
}

TI_STATUS txnQ_ConnectBus (TI_HANDLE  hTxnQ,
                           TBusDrvCfg *pBusDrvCfg,
                           TTxnDoneCb fConnectCb,
                           TI_HANDLE  hConnectCb,
                           TI_UINT32  *pRxDmaBufLen,
                           TI_UINT32  *pTxDmaBufLen)
{
    TTxnQObj *pTxnQ = (TTxnQObj*) hTxnQ;

    TRACE0(pTxnQ->hReport, REPORT_SEVERITY_INFORMATION, "txnQ_ConnectBus()\n");

    pTxnQ->fConnectCb = fConnectCb;
    pTxnQ->hConnectCb = hConnectCb;

    return busDrv_ConnectBus (pTxnQ->hBusDrv, pBusDrvCfg, txnQ_TxnDoneCb, hTxnQ, txnQ_ConnectCB, pRxDmaBufLen, pTxDmaBufLen);
}

TI_STATUS txnQ_DisconnectBus (TI_HANDLE hTxnQ)
{
    TTxnQObj *pTxnQ = (TTxnQObj*) hTxnQ;

    return busDrv_DisconnectBus (pTxnQ->hBusDrv);
}

TI_STATUS txnQ_Open (TI_HANDLE       hTxnQ, 
                     TI_UINT32       uFuncId, 
                     TI_UINT32       uNumPrios, 
                     TTxnQueueDoneCb fTxnQueueDoneCb,
                     TI_HANDLE       hCbHandle)
{
    TTxnQObj     *pTxnQ = (TTxnQObj*) hTxnQ;
    TI_UINT32     uNodeHeaderOffset;
    TI_UINT32     i;

    if (uFuncId >= MAX_FUNCTIONS  ||  uNumPrios > MAX_PRIORITY) 
    {
        TRACE2(pTxnQ->hReport, REPORT_SEVERITY_ERROR, ": Invalid Params!  uFuncId = %d, uNumPrios = %d\n", uFuncId, uNumPrios);
        return TI_NOK;
    }

    context_EnterCriticalSection (pTxnQ->hContext);

    /* Save functional driver info */
    pTxnQ->aFuncInfo[uFuncId].uNumPrios       = uNumPrios;
    pTxnQ->aFuncInfo[uFuncId].fTxnQueueDoneCb = fTxnQueueDoneCb;
    pTxnQ->aFuncInfo[uFuncId].hCbHandle       = hCbHandle;
    pTxnQ->aFuncInfo[uFuncId].eState          = FUNC_STATE_STOPPED;
    
    /* Create the functional driver's queues. */
    uNodeHeaderOffset = TI_FIELD_OFFSET(TTxnStruct, tTxnQNode); 
    for (i = 0; i < uNumPrios; i++)
    {
        pTxnQ->aTxnQueues[uFuncId][i] = que_Create (pTxnQ->hOs, pTxnQ->hReport, TXN_QUE_SIZE, uNodeHeaderOffset);
        if (pTxnQ->aTxnQueues[uFuncId][i] == NULL)
        {
            TRACE0(pTxnQ->hReport, REPORT_SEVERITY_ERROR, ": Queues creation failed!\n");
            context_LeaveCriticalSection (pTxnQ->hContext);
            return TI_NOK;
        }
    }

    /* Update functions actual range (to optimize Txn selection loops - see txnQ_SelectTxn) */
    if (uFuncId < pTxnQ->uMinFuncId) 
    {
        pTxnQ->uMinFuncId = uFuncId;
    }
    if (uFuncId > pTxnQ->uMaxFuncId) 
    {
        pTxnQ->uMaxFuncId = uFuncId;
    }

    context_LeaveCriticalSection (pTxnQ->hContext);

    TRACE2(pTxnQ->hReport, REPORT_SEVERITY_INFORMATION, ": Function %d registered successfully, uNumPrios = %d\n", uFuncId, uNumPrios);

    return TI_OK;
}

void txnQ_Close (TI_HANDLE  hTxnQ, TI_UINT32 uFuncId)
{
    TTxnQObj     *pTxnQ = (TTxnQObj*)hTxnQ;
    TI_UINT32     i;

    context_EnterCriticalSection (pTxnQ->hContext);

    /* Destroy the functional driver's queues */
    for (i = 0; i < pTxnQ->aFuncInfo[uFuncId].uNumPrios; i++)
    {
        que_Destroy (pTxnQ->aTxnQueues[uFuncId][i]);
    }

    /* Clear functional driver info */
    pTxnQ->aFuncInfo[uFuncId].uNumPrios       = 0;
    pTxnQ->aFuncInfo[uFuncId].fTxnQueueDoneCb = NULL;
    pTxnQ->aFuncInfo[uFuncId].hCbHandle       = NULL;
    pTxnQ->aFuncInfo[uFuncId].eState          = FUNC_STATE_NONE;
    
    /* Update functions actual range (to optimize Txn selection loops - see txnQ_SelectTxn) */
    pTxnQ->uMinFuncId      = MAX_FUNCTIONS; 
    pTxnQ->uMaxFuncId      = 0;             
    for (i = 0; i < MAX_FUNCTIONS; i++) 
    {
        if (pTxnQ->aFuncInfo[i].eState != FUNC_STATE_NONE) 
        {
            if (i < pTxnQ->uMinFuncId) 
            {
                pTxnQ->uMinFuncId = i;
            }
            if (i > pTxnQ->uMaxFuncId) 
            {
                pTxnQ->uMaxFuncId = i;
            }
        }
    }

    context_LeaveCriticalSection (pTxnQ->hContext);

    TRACE1(pTxnQ->hReport, REPORT_SEVERITY_INFORMATION, ": Function %d Unregistered\n", uFuncId);
}

ETxnStatus txnQ_Restart (TI_HANDLE hTxnQ, TI_UINT32 uFuncId)
{
    TTxnQObj *pTxnQ = (TTxnQObj*) hTxnQ;

    TRACE0(pTxnQ->hReport, REPORT_SEVERITY_INFORMATION, "txnQ_Restart()\n");

    context_EnterCriticalSection (pTxnQ->hContext);

    /* If a Txn from the calling function is in progress, set state to RESTART return PENDING */
    if (pTxnQ->pCurrTxn) 
    {
        if (TXN_PARAM_GET_FUNC_ID(pTxnQ->pCurrTxn) == uFuncId)
        {
            pTxnQ->aFuncInfo[uFuncId].eState = FUNC_STATE_RESTART;

            context_LeaveCriticalSection (pTxnQ->hContext);

            TRACE0(pTxnQ->hReport, REPORT_SEVERITY_INFORMATION, "txnQ_Restart(): pCurrTxn pending\n");

            /* Return PENDING to indicate that the restart will be completed later (in TxnDone) */
            return TXN_STATUS_PENDING;
        }
    }

    context_LeaveCriticalSection (pTxnQ->hContext);

    /* Clear the calling function's queues (call function CB with status=RECOVERY) */
    txnQ_ClearQueues (hTxnQ, uFuncId);

    /* Return COMPLETE to indicate that the restart was completed */
    return TXN_STATUS_COMPLETE;
}

void txnQ_Run (TI_HANDLE hTxnQ, TI_UINT32 uFuncId)
{
    TTxnQObj *pTxnQ = (TTxnQObj*) hTxnQ;

#ifdef TI_DBG
    TRACE0(pTxnQ->hReport, REPORT_SEVERITY_INFORMATION, "txnQ_Run()\n");
    if (pTxnQ->aFuncInfo[uFuncId].eState != FUNC_STATE_STOPPED) 
    {
        TRACE2(pTxnQ->hReport, REPORT_SEVERITY_WARNING, "txnQ_Run(): Called while func %d state is %d!\n", uFuncId, pTxnQ->aFuncInfo[uFuncId].eState);
    }
#endif

    /* Enable function's queues */
    pTxnQ->aFuncInfo[uFuncId].eState = FUNC_STATE_RUNNING;

    /* Send queued transactions as possible */
    txnQ_RunScheduler (pTxnQ, NULL); 
}

void txnQ_Stop (TI_HANDLE hTxnQ, TI_UINT32 uFuncId)
{
    TTxnQObj *pTxnQ = (TTxnQObj*) hTxnQ;

#ifdef TI_DBG
    TRACE0(pTxnQ->hReport, REPORT_SEVERITY_INFORMATION, "txnQ_Stop()\n");
    if (pTxnQ->aFuncInfo[uFuncId].eState != FUNC_STATE_RUNNING) 
    {
        TRACE2(pTxnQ->hReport, REPORT_SEVERITY_ERROR, "txnQ_Stop(): Called while func %d state is %d!\n", uFuncId, pTxnQ->aFuncInfo[uFuncId].eState);
    }
#endif

    /* Enable function's queues */
    pTxnQ->aFuncInfo[uFuncId].eState = FUNC_STATE_STOPPED;
}

ETxnStatus txnQ_Transact (TI_HANDLE hTxnQ, TTxnStruct *pTxn)
{
    TTxnQObj    *pTxnQ   = (TTxnQObj*)hTxnQ;
    TI_UINT32    uFuncId = TXN_PARAM_GET_FUNC_ID(pTxn);
    ETxnStatus   rc;

    if (TXN_PARAM_GET_SINGLE_STEP(pTxn)) 
    {
        pTxnQ->aFuncInfo[uFuncId].pSingleStep = pTxn;
        TRACE0(pTxnQ->hReport, REPORT_SEVERITY_INFORMATION, "txnQ_Transact(): Single step Txn\n");
    }
    else 
    {
        TI_STATUS eStatus;
        TI_HANDLE hQueue = pTxnQ->aTxnQueues[uFuncId][TXN_PARAM_GET_PRIORITY(pTxn)];
        context_EnterCriticalSection (pTxnQ->hContext);
        eStatus = que_Enqueue (hQueue, (TI_HANDLE)pTxn);
        context_LeaveCriticalSection (pTxnQ->hContext);
        if (eStatus != TI_OK)
        {
            TRACE3(pTxnQ->hReport, REPORT_SEVERITY_ERROR, "txnQ_Transact(): Enqueue failed, pTxn=0x%x, HwAddr=0x%x, Len0=%d\n", pTxn, pTxn->uHwAddr, pTxn->aLen[0]);
            return TXN_STATUS_ERROR;
        }
        TRACE0(pTxnQ->hReport, REPORT_SEVERITY_INFORMATION, "txnQ_Transact(): Regular Txn\n");
    }

    /* Send queued transactions as possible */
    rc = txnQ_RunScheduler (pTxnQ, pTxn); 

    return rc;
}


/** 
 * \fn     txnQ_ConnectCB
 * \brief  Pending Connection completion CB
 * 
 *  txnQ_ConnectBus CB
 * 
 * \note   
 * \param  hTxnQ - The module's object
 * \param  pTxn  - The completed transaction object 
 * \return void
 * \sa     
 */ 
static void txnQ_ConnectCB (TI_HANDLE hTxnQ, void *hTxn)
{
    TTxnQObj   *pTxnQ   = (TTxnQObj*)hTxnQ;

    /* Call the Client Connect CB */
    pTxnQ->fConnectCb (pTxnQ->hConnectCb, NULL);
}


/** 
 * \fn     txnQ_TxnDoneCb
 * \brief  Pending Transaction completion CB
 * 
 * Called back by bus-driver upon pending transaction completion in TxnDone context (external!).
 * Enqueue completed transaction in TxnDone queue and call scheduler to send queued transactions.
 * 
 * \note   
 * \param  hTxnQ - The module's object
 * \param  pTxn  - The completed transaction object 
 * \return void
 * \sa     
 */ 
static void txnQ_TxnDoneCb (TI_HANDLE hTxnQ, void *hTxn)
{
    TTxnQObj   *pTxnQ   = (TTxnQObj*)hTxnQ;
    TTxnStruct *pTxn    = (TTxnStruct *)hTxn;
    TI_UINT32   uFuncId = TXN_PARAM_GET_FUNC_ID(pTxn);

#ifdef TI_DBG
    TRACE0(pTxnQ->hReport, REPORT_SEVERITY_INFORMATION, "txnQ_TxnDoneCb()\n");
    if (pTxn != pTxnQ->pCurrTxn) 
    {
        TRACE2(pTxnQ->hReport, REPORT_SEVERITY_ERROR, "txnQ_TxnDoneCb(): CB returned pTxn 0x%x  while pCurrTxn is 0x%x !!\n", pTxn, pTxnQ->pCurrTxn);
    }
#endif

    /* If the function of the completed Txn is waiting for restart */
    if (pTxnQ->aFuncInfo[uFuncId].eState == FUNC_STATE_RESTART) 
    {
        TRACE0(pTxnQ->hReport, REPORT_SEVERITY_INFORMATION, "txnQ_TxnDoneCb(): Handling restart\n");

        /* First, Clear the restarted function queues  */
        txnQ_ClearQueues (hTxnQ, uFuncId);

        /* Call function CB for current Txn with restart indication */
        TXN_PARAM_SET_STATUS(pTxn, TXN_PARAM_STATUS_RECOVERY);
        pTxnQ->aFuncInfo[uFuncId].fTxnQueueDoneCb (pTxnQ->aFuncInfo[uFuncId].hCbHandle, pTxn);
    }

    /* In the normal case (no restart), enqueue completed transaction in TxnDone queue */
    else 
    {
        TI_STATUS eStatus;

        context_EnterCriticalSection (pTxnQ->hContext);
        eStatus = que_Enqueue (pTxnQ->hTxnDoneQueue, (TI_HANDLE)pTxn);
        if (eStatus != TI_OK)
        {
            TRACE3(pTxnQ->hReport, REPORT_SEVERITY_ERROR, "txnQ_TxnDoneCb(): Enqueue failed, pTxn=0x%x, HwAddr=0x%x, Len0=%d\n", pTxn, pTxn->uHwAddr, pTxn->aLen[0]);
        }
        context_LeaveCriticalSection (pTxnQ->hContext);
    }

    /* Indicate that no transaction is currently processed in the bus-driver */
    pTxnQ->pCurrTxn = NULL;

    /* Send queued transactions as possible (TRUE indicates we are in external context) */
    txnQ_RunScheduler (pTxnQ, NULL); 
}


/** 
 * \fn     txnQ_RunScheduler
 * \brief  Send queued transactions
 * 
 * Run the scheduler, which issues transactions as long as possible.
 * Since this function is called from either internal or external (TxnDone) context,
 *   it handles reentry by setting a bSchedulerPend flag, and running the scheduler again
 *   when its current iteration is finished.
 * 
 * \note   
 * \param  pTxnQ     - The module's object
 * \param  pInputTxn - The transaction inserted in the current context (NULL if none)
 * \return COMPLETE if pCurrTxn completed in this context, PENDING if not, ERROR if failed
 * \sa     
 */ 
static ETxnStatus txnQ_RunScheduler (TTxnQObj *pTxnQ, TTxnStruct *pInputTxn)
{
    TI_BOOL bFirstIteration;  
	ETxnStatus eStatus = TXN_STATUS_NONE;

    TRACE0(pTxnQ->hReport, REPORT_SEVERITY_INFORMATION, "txnQ_RunScheduler()\n");

    context_EnterCriticalSection (pTxnQ->hContext);

    /* If the scheduler is currently busy, set bSchedulerPend flag and exit */
    if (pTxnQ->bSchedulerBusy)
    {
        TRACE0(pTxnQ->hReport, REPORT_SEVERITY_INFORMATION, "txnQ_RunScheduler(): Scheduler is busy\n");
        pTxnQ->bSchedulerPend = TI_TRUE;
        context_LeaveCriticalSection (pTxnQ->hContext);
        return TXN_STATUS_PENDING;
    }

    /* Indicate that the scheduler is currently busy */
    pTxnQ->bSchedulerBusy = TI_TRUE;

    context_LeaveCriticalSection (pTxnQ->hContext);

    bFirstIteration = TI_TRUE;  

    /* 
     * Run the scheduler while it has work to do 
     */
    while (1)
    {
        /* If first scheduler iteration, save its return code to return the original Txn result */
        if (bFirstIteration) 
        {
            eStatus = txnQ_Scheduler (pTxnQ, pInputTxn);
            bFirstIteration = TI_FALSE;
        }
        /* This is for handling pending calls when the scheduler was busy (see above) */
        else 
        {
            TRACE0(pTxnQ->hReport, REPORT_SEVERITY_INFORMATION, "txnQ_RunScheduler(): Handle pending scheduler call\n");
            txnQ_Scheduler (pTxnQ, NULL);
        }

        context_EnterCriticalSection (pTxnQ->hContext);

        /* If no pending calls, clear the busy flag and return the original caller Txn status */
        if (!pTxnQ->bSchedulerPend) 
        {
            pTxnQ->bSchedulerBusy = TI_FALSE;
            context_LeaveCriticalSection (pTxnQ->hContext);
            return eStatus;
        }
        pTxnQ->bSchedulerPend = TI_FALSE;

        context_LeaveCriticalSection (pTxnQ->hContext);
    }
}


/** 
 * \fn     txnQ_Scheduler
 * \brief  Send queued transactions
 * 
 * Issue transactions as long as they are available and the bus is not occupied.
 * Call CBs of completed transactions, except completion of pInputTxn (covered by the return value).
 * Note that this function is called from either internal or external (TxnDone) context.
 * However, the txnQ_RunScheduler which calls it, prevents scheduler reentry.
 * 
 * \note   
 * \param  pTxnQ     - The module's object
 * \param  pInputTxn - The transaction inserted in the current context (NULL if none)
 * \return COMPLETE if pInputTxn completed in this context, PENDING if not, ERROR if failed
 * \sa     txnQ_RunScheduler
 */ 
static ETxnStatus txnQ_Scheduler (TTxnQObj *pTxnQ, TTxnStruct *pInputTxn)
{
    ETxnStatus eInputTxnStatus;  

    /* Use as return value the status of the input transaction (PENDING unless sent and completed here) */
    eInputTxnStatus = TXN_STATUS_PENDING;  

    /* if a previous transaction is in progress, return PENDING */
    if (pTxnQ->pCurrTxn)
    {
        TRACE1(pTxnQ->hReport, REPORT_SEVERITY_INFORMATION, "txnQ_Scheduler(): pCurrTxn isn't null (0x%x) so exit\n", pTxnQ->pCurrTxn);
        return TXN_STATUS_PENDING;
    }

    /* Loop while transactions are available and can be sent to bus driver */
    while (1)
    {
        TTxnStruct   *pSelectedTxn;
        ETxnStatus    eStatus;

        /* Get next enabled transaction by priority. If none, exit loop. */
        context_EnterCriticalSection (pTxnQ->hContext);
        pSelectedTxn = txnQ_SelectTxn (pTxnQ);
        context_LeaveCriticalSection (pTxnQ->hContext);
        if (pSelectedTxn == NULL)
        {
            break;
        }

        /* Save transaction in case it will be async (to indicate that the bus driver is busy) */
        pTxnQ->pCurrTxn = pSelectedTxn;

        /* Send selected transaction to bus driver */
        eStatus = busDrv_Transact (pTxnQ->hBusDrv, pSelectedTxn);

        /* If we've just sent the input transaction, use the status as the return value */
        if (pSelectedTxn == pInputTxn)
        {
            eInputTxnStatus = eStatus;
        }

        TRACE3(pTxnQ->hReport, REPORT_SEVERITY_INFORMATION, "txnQ_Scheduler(): Txn 0x%x sent, status = %d, eInputTxnStatus = %d\n", pSelectedTxn, eStatus, eInputTxnStatus);

        /* If transaction completed */
        if (eStatus != TXN_STATUS_PENDING)
        {
            pTxnQ->pCurrTxn = NULL;

            /* If it's not the input transaction, enqueue it in TxnDone queue */
            if (pSelectedTxn != pInputTxn)
            {
                TI_STATUS eStatus;

                context_EnterCriticalSection (pTxnQ->hContext);
                eStatus = que_Enqueue (pTxnQ->hTxnDoneQueue, (TI_HANDLE)pSelectedTxn);
                if (eStatus != TI_OK)
                {
                    TRACE3(pTxnQ->hReport, REPORT_SEVERITY_ERROR, "txnQ_Scheduler(): Enqueue failed, pTxn=0x%x, HwAddr=0x%x, Len0=%d\n", pSelectedTxn, pSelectedTxn->uHwAddr, pSelectedTxn->aLen[0]);
                }
                context_LeaveCriticalSection (pTxnQ->hContext);
            }
        }

        /* If pending Exit loop! */
        else 
        {
            break;
        }
    }

    /* Dequeue completed transactions and call their functional driver CB */
    /* Note that it's the functional driver CB and not the specific CB in the Txn! */
    while (1)
    {
        TTxnStruct      *pCompletedTxn;
        TI_UINT32        uFuncId;
        TTxnQueueDoneCb  fTxnQueueDoneCb;
        TI_HANDLE        hCbHandle;

        context_EnterCriticalSection (pTxnQ->hContext);
        pCompletedTxn   = (TTxnStruct *) que_Dequeue (pTxnQ->hTxnDoneQueue);
        context_LeaveCriticalSection (pTxnQ->hContext);
        if (pCompletedTxn == NULL)
        {
            /* Return the status of the input transaction (PENDING unless sent and completed here) */
            return eInputTxnStatus;
        }

        TRACE1(pTxnQ->hReport, REPORT_SEVERITY_INFORMATION, "txnQ_Scheduler(): Calling TxnDone for Txn 0x%x\n", pCompletedTxn);

        uFuncId         = TXN_PARAM_GET_FUNC_ID(pCompletedTxn);
        fTxnQueueDoneCb = pTxnQ->aFuncInfo[uFuncId].fTxnQueueDoneCb;
        hCbHandle       = pTxnQ->aFuncInfo[uFuncId].hCbHandle;

        fTxnQueueDoneCb (hCbHandle, pCompletedTxn);
    }
}


/** 
 * \fn     txnQ_SelectTxn
 * \brief  Select transaction to send
 * 
 * Called from txnQ_RunScheduler() which is protected in critical section.
 * Select the next enabled transaction by priority.
 * 
 * \note   
 * \param  pTxnQ - The module's object
 * \return The selected transaction to send (NULL if none available)
 * \sa     
 */ 
static TTxnStruct *txnQ_SelectTxn (TTxnQObj *pTxnQ)
{
    TTxnStruct *pSelectedTxn;
    TI_UINT32   uFunc;
    TI_UINT32   uPrio;

#ifdef TI_DBG
    /* If within Tx aggregation, dequeue Txn from same queue, and if not NULL return it */
    if (pTxnQ->pAggregQueue)
    {
        pSelectedTxn = (TTxnStruct *) que_Dequeue (pTxnQ->pAggregQueue);
        if (pSelectedTxn != NULL)
        {
            /* If aggregation ended, reset the aggregation-queue pointer */
            if (TXN_PARAM_GET_AGGREGATE(pSelectedTxn) == TXN_AGGREGATE_OFF) 
            {
                if ((TXN_PARAM_GET_FIXED_ADDR(pSelectedTxn) != TXN_FIXED_ADDR) ||
                    (TXN_PARAM_GET_DIRECTION(pSelectedTxn)  != TXN_DIRECTION_WRITE))
                {
                    TRACE2(pTxnQ->hReport, REPORT_SEVERITY_ERROR, "txnQ_SelectTxn: Mixed transaction during aggregation, HwAddr=0x%x, TxnParams=0x%x\n", pSelectedTxn->uHwAddr, pSelectedTxn->uTxnParams);
                }
                pTxnQ->pAggregQueue = NULL;
            }
            return pSelectedTxn;
        }
        return NULL;
    }
#endif

    /* For all functions, if single-step Txn waiting, return it (sent even if function is stopped) */
    for (uFunc = pTxnQ->uMinFuncId; uFunc <= pTxnQ->uMaxFuncId; uFunc++)
    {
        pSelectedTxn = pTxnQ->aFuncInfo[uFunc].pSingleStep;
        if (pSelectedTxn != NULL)
        {
            pTxnQ->aFuncInfo[uFunc].pSingleStep = NULL;
            return pSelectedTxn;
        }
    }

    /* For all priorities from high to low */
    for (uPrio = 0; uPrio < MAX_PRIORITY; uPrio++)
    {
        /* For all functions */
        for (uFunc = pTxnQ->uMinFuncId; uFunc <= pTxnQ->uMaxFuncId; uFunc++)
        {
            /* If function running and uses this priority */
            if (pTxnQ->aFuncInfo[uFunc].eState == FUNC_STATE_RUNNING  &&
                pTxnQ->aFuncInfo[uFunc].uNumPrios > uPrio)
            {
                /* Dequeue Txn from current func and priority queue, and if not NULL return it */
                pSelectedTxn = (TTxnStruct *) que_Dequeue (pTxnQ->aTxnQueues[uFunc][uPrio]);
                if (pSelectedTxn != NULL)
                {
#ifdef TI_DBG
                    /* If aggregation begins, save the aggregation-queue pointer to ensure continuity */
                    if (TXN_PARAM_GET_AGGREGATE(pSelectedTxn) == TXN_AGGREGATE_ON) 
                    {
                        pTxnQ->pAggregQueue = pTxnQ->aTxnQueues[uFunc][uPrio];
                    }
#endif
                    return pSelectedTxn;
                }
            }
        }
    }

    /* If no transaction was selected, return NULL */
    return NULL;
}


/** 
 * \fn     txnQ_ClearQueues
 * \brief  Clear the function queues
 * 
 * Clear the specified function queues and call its CB for each Txn with status=RECOVERY.
 * 
 * \note   Called in critical section.
 * \param  hTxnQ            - The module's object
 * \param  uFuncId          - The calling functional driver
 * \return void
 * \sa     
 */ 
void txnQ_ClearQueues (TI_HANDLE hTxnQ, TI_UINT32 uFuncId)
{
    TTxnQObj        *pTxnQ = (TTxnQObj*)hTxnQ;
    TTxnStruct      *pTxn;
    TI_UINT32        uPrio;

    context_EnterCriticalSection (pTxnQ->hContext);

    pTxnQ->aFuncInfo[uFuncId].pSingleStep = NULL;

    /* For all function priorities */
    for (uPrio = 0; uPrio < pTxnQ->aFuncInfo[uFuncId].uNumPrios; uPrio++)
    {
        do
        {
            /* Dequeue Txn from current priority queue */
            pTxn = (TTxnStruct *) que_Dequeue (pTxnQ->aTxnQueues[uFuncId][uPrio]);

            /* 
             * Drop on Restart 
             * do not call fTxnQueueDoneCb (hCbHandle, pTxn) callback 
             */
        } while (pTxn != NULL);
    }

    /* Clear state - for restart (doesn't call txnQ_Open) */
    pTxnQ->aFuncInfo[uFuncId].eState = FUNC_STATE_RUNNING;

    context_LeaveCriticalSection (pTxnQ->hContext);
}

#ifdef TI_DBG
void txnQ_PrintQueues (TI_HANDLE hTxnQ)
{
    TTxnQObj    *pTxnQ   = (TTxnQObj*)hTxnQ;

    WLAN_OS_REPORT(("Print TXN queues\n"));
    WLAN_OS_REPORT(("================\n"));
    que_Print(pTxnQ->aTxnQueues[TXN_FUNC_ID_WLAN][TXN_LOW_PRIORITY]);
    que_Print(pTxnQ->aTxnQueues[TXN_FUNC_ID_WLAN][TXN_HIGH_PRIORITY]);
}
#endif /* TI_DBG */
