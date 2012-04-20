/*
 * txResult.c
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


/****************************************************************************
 *
 *   MODULE:  txResult.c
 *   
 *   PURPOSE:  Handle packets Tx results upon Tx-complete from the FW. 
 * 
 *   DESCRIPTION:  
 *   ============
 *      This module is called upon Tx-complete from FW. 
 *      It retrieves the transmitted packets results from the FW TxResult table and
 *        calls the upper layer callback function for each packet with its results.
 *
 ****************************************************************************/

#define __FILE_ID__  FILE_ID_107
#include "tidef.h"
#include "osApi.h"
#include "report.h"
#include "TwIf.h"
#include "txCtrlBlk_api.h"
#include "txResult_api.h"
#include "TWDriver.h"
#include "FwEvent_api.h"



#define TX_RESULT_QUEUE_DEPTH_MASK  (TRQ_DEPTH - 1)

#if (TX_RESULT_QUEUE_DEPTH_MASK & TRQ_DEPTH) 
    #error  TRQ_DEPTH should be a power of 2 !!
#endif


/* Callback function definition for Tx sendPacketComplete */
typedef void (* TSendPacketCompleteCb)(TI_HANDLE hCbObj, TxResultDescriptor_t *pTxResultInfo);

/* Tx-Result SM states */
typedef enum
{
    TX_RESULT_STATE_IDLE,
    TX_RESULT_STATE_READING
} ETxResultState;

/* The host Tx-results counter write transaction structure. */
typedef struct
{
    TTxnStruct tTxnStruct;
    TI_UINT32  uCounter;              
} THostCounterWriteTxn;

/* The Tx-results counters and table read transaction structure. */
typedef struct
{
    TTxnStruct          tTxnStruct;
    TxResultInterface_t tTxResultInfo;
} TResultsInfoReadTxn;

/* The TxResult module object. */
typedef struct
{
    TI_HANDLE               hOs;
    TI_HANDLE               hReport;
    TI_HANDLE               hTwIf;

    TI_UINT32               uTxResultInfoAddr;       /* The HW Tx-Result Table address */
    TI_UINT32               uTxResultHostCounterAddr;/* The Tx-Result host counter address in SRAM */
    TI_UINT32               uHostResultsCounter;     /* Number of results read by host from queue since FW-init (updated to FW) */
    ETxResultState          eState;                  /* Current eState of SM */
    TSendPacketCompleteCb   fSendPacketCompleteCb;   /* Tx-Complete callback function */
    TI_HANDLE               hSendPacketCompleteHndl; /* Tx-Complete callback function handle */
    THostCounterWriteTxn    tHostCounterWriteTxn;    /* The structure used for writing host results counter to FW */
    TResultsInfoReadTxn     tResultsInfoReadTxn;     /* The structure used for reading Tx-results counters and table from  FW */
#ifdef TI_DBG
    TI_UINT32               uInterruptsCounter;         /* Count number of Tx-results */
#endif

} TTxResultObj;


static void txResult_Restart (TTxResultObj *pTxResult);
static void txResult_HandleNewResults (TTxResultObj *pTxResult);
static void txResult_StateMachine (TI_HANDLE hTxResult);



/****************************************************************************
 *                      txResult_Create()
 ****************************************************************************
 * DESCRIPTION: Create the Tx-Result object 
 * 
 * INPUTS:  hOs
 * 
 * OUTPUT:  None
 * 
 * RETURNS: The Created object
 ****************************************************************************/
TI_HANDLE txResult_Create(TI_HANDLE hOs)
{
    TTxResultObj *pTxResult;

    pTxResult = os_memoryAlloc(hOs, sizeof(TTxResultObj));
    if (pTxResult == NULL)
        return NULL;

    os_memoryZero(hOs, pTxResult, sizeof(TTxResultObj));

    pTxResult->hOs = hOs;

    return( (TI_HANDLE)pTxResult );
}


/****************************************************************************
 *                      txResult_Destroy()
 ****************************************************************************
 * DESCRIPTION: Destroy the Tx-Result object 
 * 
 * INPUTS:  hTxResult - The object to free
 * 
 * OUTPUT:  None
 * 
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS txResult_Destroy(TI_HANDLE hTxResult)
{
    TTxResultObj *pTxResult = (TTxResultObj *)hTxResult;

    if (pTxResult)
        os_memoryFree(pTxResult->hOs, pTxResult, sizeof(TTxResultObj));

    return TI_OK;
}


/****************************************************************************
 *               txResult_Init()
 ****************************************************************************
   DESCRIPTION:  
   ============
     Initialize the txResult module.
 ****************************************************************************/
TI_STATUS txResult_Init(TI_HANDLE hTxResult, TI_HANDLE hReport, TI_HANDLE hTwIf)
{
    TTxResultObj *pTxResult = (TTxResultObj *)hTxResult;
    TTxnStruct   *pTxn;

    pTxResult->hReport    = hReport;
    pTxResult->hTwIf      = hTwIf;

    /* Prepare Host-Results-Counter write transaction (HwAddr is filled before each transaction) */
    pTxn = &pTxResult->tHostCounterWriteTxn.tTxnStruct;
    TXN_PARAM_SET(pTxn, TXN_LOW_PRIORITY, TXN_FUNC_ID_WLAN, TXN_DIRECTION_WRITE, TXN_INC_ADDR)
    BUILD_TTxnStruct(pTxn, 0, &pTxResult->tHostCounterWriteTxn.uCounter, REGISTER_SIZE, NULL, NULL)

    /* Prepare Tx-Result counter and table read transaction (HwAddr is filled before each transaction) */
    pTxn = &pTxResult->tResultsInfoReadTxn.tTxnStruct;
    TXN_PARAM_SET(pTxn, TXN_LOW_PRIORITY, TXN_FUNC_ID_WLAN, TXN_DIRECTION_READ, TXN_INC_ADDR)
    BUILD_TTxnStruct(pTxn, 
                     0, 
                     &pTxResult->tResultsInfoReadTxn.tTxResultInfo, 
                     sizeof(TxResultInterface_t), 
                     (TTxnDoneCb)txResult_StateMachine, 
                     hTxResult)

    txResult_Restart (pTxResult);

    return TI_OK;
}


/****************************************************************************
 *               txResult_Restart()
 ****************************************************************************
   DESCRIPTION:  
   ============
     Restarts the Tx-Result module.
     Called upon init and recovery.
     Shouldn't be called upon disconnect, since the FW provides Tx-Complete
       for all pending packets in FW!!
 ****************************************************************************/
static void txResult_Restart (TTxResultObj *pTxResult)
{
	pTxResult->uHostResultsCounter = 0;
    pTxResult->eState = TX_RESULT_STATE_IDLE;      
}


/****************************************************************************
 *                      txResult_setHwInfo()
 ****************************************************************************
 * DESCRIPTION:  
 *      Called after the HW configuration upon init or recovery.
 *      Store the Tx-result table HW address.
 ****************************************************************************/
void  txResult_setHwInfo(TI_HANDLE hTxResult, TDmaParams *pDmaParams)
{
    TTxResultObj *pTxResult = (TTxResultObj *)hTxResult;

    pTxResult->uTxResultInfoAddr = (TI_UINT32)(pDmaParams->fwTxResultInterface);
	pTxResult->uTxResultHostCounterAddr = pTxResult->uTxResultInfoAddr + 
		TI_FIELD_OFFSET(TxResultControl_t, TxResultHostCounter);

    txResult_Restart (pTxResult);
} 


/****************************************************************************
 *                      txResult_TxCmpltIntrCb()
 ****************************************************************************
 * DESCRIPTION:   
 * ============
 *  Called upon DATA interrupt from the FW.
 *  If new Tx results are available, start handling them.
 * 
 * INPUTS:  hTxResult - the txResult object handle.
 *          pFwStatus - The FW status registers read by the FwEvent
 *  
 * OUTPUT:  None
 * 
 * RETURNS:  ETxnStatus 
 ***************************************************************************/
ETxnStatus txResult_TxCmpltIntrCb (TI_HANDLE hTxResult, FwStatus_t *pFwStatus)
{
    TTxResultObj   *pTxResult = (TTxResultObj *)hTxResult;
    TI_UINT32      uTempCounters;
    FwStatCntrs_t  *pFwStatusCounters;

#ifdef TI_DBG
    pTxResult->uInterruptsCounter++;

    if (pTxResult->eState != TX_RESULT_STATE_IDLE)
    {
        TRACE1(pTxResult->hReport, REPORT_SEVERITY_INFORMATION, ": called in eState %d, so exit\n", pTxResult->eState);
        return TXN_STATUS_COMPLETE;
    }
#endif

    /* If no new results - exit (may happen since Data interrupt is common to all Tx&Rx events) */
    uTempCounters = ENDIAN_HANDLE_LONG(pFwStatus->counters);
    pFwStatusCounters = (FwStatCntrs_t *)&uTempCounters;
    if (pFwStatusCounters->txResultsCntr == (TI_UINT8)pTxResult->uHostResultsCounter)
    {
        TRACE0(pTxResult->hReport, REPORT_SEVERITY_INFORMATION, ": No new Tx results\n");
        return TXN_STATUS_COMPLETE;
    }

    /* Call the SM to handle the new Tx results */
    txResult_StateMachine (hTxResult);
    return TXN_STATUS_COMPLETE;
}


/****************************************************************************
 *                      txResult_StateMachine()
 ****************************************************************************
 * DESCRIPTION:  
 *
 *  The main SM of the module. Called in IDLE eState by txResult_TxCmpltIntrCb() on 
 *      Data interrupt from the FW. 
 *  If no new results - exit (may happen since Data interrupt is common to all Tx&Rx events)
 *  Read all Tx-Result cyclic table.
 *  Go over the new Tx-results and call the upper layer callback function for each packet result.
 *  At the end - write the new host counter to the FW.
 *          
 * INPUTS:  
 *
 * OUTPUT:  
 * 
 * RETURNS: None 
 ****************************************************************************/
static void txResult_StateMachine (TI_HANDLE hTxResult)
{
    TTxResultObj *pTxResult  = (TTxResultObj *)hTxResult;
	ETxnStatus   eTwifStatus = TXN_STATUS_COMPLETE;  /* Last bus operation status: Complete (Sync) or Pending (Async). */
    TTxnStruct   *pTxn       = &(pTxResult->tResultsInfoReadTxn.tTxnStruct);
 
    /* Loop while processing is completed in current context (sync), or until fully completed */
    while (eTwifStatus == TXN_STATUS_COMPLETE)
    {
        TRACE2(pTxResult->hReport, REPORT_SEVERITY_INFORMATION, ": eState = %d, eTwifStatus = %d\n", pTxResult->eState, eTwifStatus);

        switch(pTxResult->eState) 
        {
        case TX_RESULT_STATE_IDLE:
            /* Read Tx-Result queue and counters. */
            pTxn->uHwAddr = pTxResult->uTxResultInfoAddr;
            eTwifStatus = twIf_Transact (pTxResult->hTwIf, pTxn);

            pTxResult->eState = TX_RESULT_STATE_READING;
            break;
    
        case TX_RESULT_STATE_READING:
            /* Process new Tx results, call upper layers to handle them and update host-index in the FW. */
            txResult_HandleNewResults (pTxResult);
            pTxResult->eState = TX_RESULT_STATE_IDLE;
            return;  /*********  Exit after all processing is finished  **********/

        default:
            TRACE1(pTxResult->hReport, REPORT_SEVERITY_ERROR, ": Unknown eState = %d\n", pTxResult->eState);
            return;
        }
    }

    if (eTwifStatus == TXN_STATUS_ERROR)
    {   
        TRACE2(pTxResult->hReport, REPORT_SEVERITY_ERROR, ": returning ERROR in eState %d, eTwifStatus=%d !!!\n", pTxResult->eState, eTwifStatus);
    }
}


/****************************************************************************
 *                      txResult_HandleNewResults()
 ****************************************************************************
 * DESCRIPTION:   
 * ============
 *	We now have the Tx Result table info from the FW so do as follows:
 *	1.	Find the number of new results (FW counter minus host counter), and if 0 exit.
 *  2.	Call the upper layers callback per Tx result. 
 *	3.	Update Host-Counter to be equal to the FW-Counter, and write it to the FW.
 ***************************************************************************/
static void txResult_HandleNewResults (TTxResultObj *pTxResult)
{
	TI_UINT32 uNumNewResults;    /* The number of new Tx-Result entries to be processed. */
	TI_UINT32 uFwResultsCounter; /* The FW current results counter (accumulated). */
	TI_UINT32 uTableIndex;
	TI_UINT32 i;
	TxResultDescriptor_t *pCurrentResult;
    TTxnStruct *pTxn = &(pTxResult->tHostCounterWriteTxn.tTxnStruct);

	/* The uFwResultsCounter is the accumulated number of Tx-Results provided by the FW, and the 
	 *   uHostResultsCounter is the accumulated number of Tx-Results processed by the host.
	 * The delta is the number of new Tx-results in the queue, waiting for host processing.
	 * Since the difference is always a small positive number, a simple subtraction is good
	 *   also for wrap around case.
	 */
	uFwResultsCounter = ENDIAN_HANDLE_LONG(pTxResult->tResultsInfoReadTxn.tTxResultInfo.TxResultControl.TxResultFwCounter);
	uNumNewResults = uFwResultsCounter - pTxResult->uHostResultsCounter;

#ifdef TI_DBG
	/* Verify there are new entries (was already checked in txResult_TxCmpltIntrCb) */
	if (uNumNewResults == 0)
	{
TRACE2(pTxResult->hReport, REPORT_SEVERITY_WARNING, ": No New Results although indicated by FwStatus!!  HostCount=%d, FwCount=%d\n", pTxResult->uHostResultsCounter, uFwResultsCounter);
		return;
	}
#endif

	/* Update host results-counter in FW to be equal to the FW counter (all new results were processed). */
	pTxResult->tHostCounterWriteTxn.uCounter = ENDIAN_HANDLE_LONG(uFwResultsCounter);
    pTxn->uHwAddr = pTxResult->uTxResultHostCounterAddr; 
    twIf_Transact(pTxResult->hTwIf, pTxn);

    TRACE3(pTxResult->hReport, REPORT_SEVERITY_INFORMATION, ": NumResults=%d, OriginalHostCount=%d, FwCount=%d\n", uNumNewResults, pTxResult->uHostResultsCounter, uFwResultsCounter);

	/* Loop over all new Tx-results and call Tx-complete callback with current entry pointer. */
    /* NOTE: THIS SHOULD COME LAST because it may lead to driver-stop process!! */
	for (i = 0; i < uNumNewResults; i++)
	{
		uTableIndex = pTxResult->uHostResultsCounter & TX_RESULT_QUEUE_DEPTH_MASK;
		pCurrentResult = &(pTxResult->tResultsInfoReadTxn.tTxResultInfo.TxResultQueue[uTableIndex]);
        pTxResult->uHostResultsCounter++;

        TRACE1(pTxResult->hReport, REPORT_SEVERITY_INFORMATION , ": call upper layer CB, Status = %d\n", pCurrentResult->status);

		pTxResult->fSendPacketCompleteCb (pTxResult->hSendPacketCompleteHndl, pCurrentResult);
	}
}


/****************************************************************************
 *                      txResult_RegisterCb()
 ****************************************************************************
 * DESCRIPTION:  Register the upper driver Tx-Result callback functions.
 ****************************************************************************/
void txResult_RegisterCb (TI_HANDLE hTxResult, TI_UINT32 uCallBackId, void *CBFunc, TI_HANDLE hCbObj)
{
    TTxResultObj* pTxResult = (TTxResultObj*)hTxResult;

    switch (uCallBackId)
    {
        /* Set Tx-Complete callback */
        case TWD_INT_SEND_PACKET_COMPLETE:
            pTxResult->fSendPacketCompleteCb   = (TSendPacketCompleteCb)CBFunc;
            pTxResult->hSendPacketCompleteHndl = hCbObj;
            break;

        default:
            TRACE0(pTxResult->hReport, REPORT_SEVERITY_ERROR, ": Illegal value\n");
            return;
    }
}


#ifdef TI_DBG      /*  Debug Functions   */

/****************************************************************************
 *                      txResult_PrintInfo()
 ****************************************************************************
 * DESCRIPTION:  Prints TX result debug information.
 ****************************************************************************/
void txResult_PrintInfo (TI_HANDLE hTxResult)
{
#ifdef REPORT_LOG
    TTxResultObj* pTxResult = (TTxResultObj*)hTxResult;

    WLAN_OS_REPORT(("Tx-Result Module Information:\n"));
    WLAN_OS_REPORT(("=============================\n"));
    WLAN_OS_REPORT(("uInterruptsCounter:     %d\n", pTxResult->uInterruptsCounter));
    WLAN_OS_REPORT(("uHostResultsCounter:    %d\n", pTxResult->uHostResultsCounter));
    WLAN_OS_REPORT(("=============================\n"));
#endif
}


/****************************************************************************
 *                      txResult_ClearInfo()
 ****************************************************************************
 * DESCRIPTION:  Clears TX result debug information.
 ****************************************************************************/
void txResult_ClearInfo (TI_HANDLE hTxResult)
{
    TTxResultObj* pTxResult = (TTxResultObj*)hTxResult;

    pTxResult->uInterruptsCounter = 0;
}

#endif  /* TI_DBG */


