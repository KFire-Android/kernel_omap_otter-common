/*
 * TwIf.c
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

 
/** \file   TwIf.c 
 *  \brief  The TWD bottom API towards the Txn-Queue. 
 *
 * The TwIf module is the lowest WLAN-specific layer and presents a common interface to all Xfer modules.
 * As such, it is responsible for the common functionalities related to device access, which includes:
 *    - transactions submission
 *    - interface power control
 *    - address translation (paging) when needed (depends on bus attributes).
 * The TwIf has no OS, platform or bus type dependencies.
 * 
 *  \see    TwIf.h, TxnQueue.c, TxnQueue.h
 */

#define __FILE_ID__  FILE_ID_121
#include "tidef.h"
#include "report.h"
#include "context.h"
#include "timer.h"
#include "TxnDefs.h"
#include "TxnQueue.h"
#include "TwIf.h"
#include "TWDriver.h"
 

/************************************************************************
 * Defines
 ************************************************************************/
#define TXN_DONE_QUE_SIZE       QUE_UNLIMITED_SIZE
#define PEND_RESTART_TIMEOUT    100   /* timeout in msec for completion of last DMA transaction during restart */

/* Values to write to the ELP register for sleep/awake */
#define ELP_CTRL_REG_SLEEP      0
#define ELP_CTRL_REG_AWAKE      1

/* 
 * Device interface-control registers addresses (at the end ot the 17-bit address space):
 */
#define PARTITION_REGISTERS_ADDR        (0x1FFC0)   /* Four 32 bit register:                      */
                                                    /*    Memory region size            (0x1FFC0) */
                                                    /*    Memory region base address    (0x1FFC4) */
                                                    /*    Registers region size         (0x1FFC8) */
                                                    /*    Registers region base address (0x1FFCC) */

#define ELP_CTRL_REG_ADDR		        (0x1FFFC)   /* ELP control register address */



/************************************************************************
 * Types
 ************************************************************************/

/* TwIf SM States */
typedef enum
{
	SM_STATE_AWAKE,           /* HW is awake and Txn-Queue is running */
	SM_STATE_SLEEP,           /* HW is asleep and Txn-Queue is stopped */
	SM_STATE_WAIT_HW          /* Waiting for HW to wake up (after triggering it), Txn-Queue is stopped */
} ESmState;

/* TwIf SM Events */
typedef enum
{
	SM_EVENT_START,           /* Need to wake up the device to handle transactions */
	SM_EVENT_HW_AVAILABLE,    /* The device woke up */
	SM_EVENT_SLEEP            /* Need to let the device go to sleep */
} ESmEvent;

/* The addresses partitioning configuration Txn data */
typedef struct 
{
    TI_UINT32       uMemSize;        /* The HW memory region size. */
    TI_UINT32       uMemAddr;        /* The HW memory region address. */
    TI_UINT32       uRegSize;        /* The HW registers region size. */
    TI_UINT32       uRegAddr;        /* The HW registers region address. */

} TPartitionTxnData;

/* The addresses partitioning configuration Txn */
typedef struct 
{
    TTxnStruct          tHdr;        /* The generic transaction structure */
    TPartitionTxnData   tData;       /* The addresses partitioning configuration data */

} TPartitionTxn;

/* The addresses partitioning configuration Txn */
typedef struct 
{
    TTxnStruct      tHdr;           /* The generic transaction structure */
    TI_UINT32   tData;       /* The addresses partitioning configuration data for one register */

} TPartitionRegTxn;

/* The addresses partitioning configuration Txn */
typedef struct 
{
    TTxnStruct      tHdr;           /* The generic transaction structure */
    TI_UINT8        uElpData;       /* The value to write to the ELP register */

} TElpTxn;

/* The TwIf module Object */
typedef struct _TTwIfObj
{
    /* Other modules handles */
    TI_HANDLE	    hOs;		   	 
    TI_HANDLE	    hReport;
	TI_HANDLE       hContext;
	TI_HANDLE       hTimer;
	TI_HANDLE	    hTxnQ;

    ESmState        eState;          /* SM current state */
    TI_HANDLE       hTxnDoneQueue;   /* Queue for completed transactions not reported yet to the upper layer */
    TI_UINT32       uContextId;      /* The ID allocated to this module on registration to context module */
    TFailureEventCb fErrCb;          /* The upper layer CB function for error handling */
    TI_HANDLE       hErrCb;          /* The CB function handle */
    TRecoveryCb     fRecoveryCb;     /* The upper layer CB for restart complete */
    TI_HANDLE       hRecoveryCb;     /* The CB function handle */
    TI_UINT32       uAwakeReqCount;  /* Increment on awake requests and decrement on sleep requests */
    TI_UINT32       uPendingTxnCount;/* Count pending transactions (sent to TxnQ and not completed yet) */
    TElpTxn         tElpTxnSleep;    /* Transaction structure for writing sleep to ELP register  */
    TElpTxn         tElpTxnAwake;    /* Transaction structure for writing awake to ELP register  */

    /* HW Addresses partitioning */
    TI_UINT32       uMemAddr1;        /* The HW memory region start address. */
    TI_UINT32       uMemSize1;        /* The HW memory region end address. */
    TI_UINT32       uMemAddr2;        /* The HW registers region start address. */
    TI_UINT32       uMemSize2;        /* The HW registers region end address. */
    TI_UINT32       uMemAddr3;        /* The INT Status registers region start address. */
    TI_UINT32       uMemSize3;        /* The INT Status registers region end address. */
    TI_UINT32       uMemAddr4;        /* The FW Status mem registers region start address. */
    

#ifdef TI_DBG
    /* Debug counters */
    TI_UINT32       uDbgCountAwake;      /* Count calls to twIf_Awake */
    TI_UINT32       uDbgCountSleep;      /* Count calls to twIf_Sleep */
    TI_UINT32       uDbgCountTxn;        /* Count calls to twIf_SendTransaction (including TwIf internal Txns) */
    TI_UINT32       uDbgCountTxnPending; /* Count transactions that returned PENDING */
    TI_UINT32       uDbgCountTxnComplete;/* Count transactions that returned COMPLETE */
    TI_UINT32       uDbgCountTxnDoneCb;  /* Count calls to twIf_TxnDoneCb */
#endif
   
    TI_BOOL         bTxnDoneInRecovery;      /* Indicate that current TxnDone is within recovery process */
    TI_BOOL         bPendRestartTimerRunning;/* Indicate that the restart guard timer is running */ 
    TI_HANDLE       hPendRestartTimer;       /* The restart process guard timer */

} TTwIfObj;


/************************************************************************
 * Internal functions prototypes
 ************************************************************************/
static void        twIf_WriteElpReg        (TTwIfObj *pTwIf, TI_UINT32 uValue);
static void        twIf_PartitionTxnDoneCb (TI_HANDLE hTwIf, void *hTxn);
static ETxnStatus  twIf_SendTransaction    (TTwIfObj *pTwIf, TTxnStruct *pTxn);
static void        twIf_HandleSmEvent      (TTwIfObj *pTwIf, ESmEvent eEvent);
static void        twIf_TxnDoneCb          (TI_HANDLE hTwIf, TTxnStruct *pTxn);
static void        twIf_HandleTxnDone      (TI_HANDLE hTwIf);
static void        twIf_ClearTxnDoneQueue  (TI_HANDLE hTwIf);
static void        twIf_PendRestratTimeout (TI_HANDLE hTwIf, TI_BOOL bTwdInitOccured);


/************************************************************************
 *
 *   Module functions implementation
 *
 ************************************************************************/

/** 
 * \fn     twIf_Create 
 * \brief  Create the module
 * 
 * Allocate and clear the module's object.
 * 
 * \note   
 * \param  hOs - Handle to Os Abstraction Layer
 * \return Handle of the allocated object, NULL if allocation failed 
 * \sa     twIf_Destroy
 */ 
TI_HANDLE twIf_Create (TI_HANDLE hOs)
{
    TI_HANDLE  hTwIf;
    TTwIfObj  *pTwIf;

    hTwIf = os_memoryAlloc (hOs, sizeof(TTwIfObj));
    if (hTwIf == NULL)
        return NULL;
    
    pTwIf = (TTwIfObj *)hTwIf;

    os_memoryZero (hOs, hTwIf, sizeof(TTwIfObj));
    
    pTwIf->hOs = hOs;

    return pTwIf;
}


/** 
 * \fn     twIf_Destroy
 * \brief  Destroy the module. 
 * 
 * Unregister from TxnQ and free the TxnDone-queue and the module's object.
 * 
 * \note   
 * \param  The module's object
 * \return TI_OK on success or TI_NOK on failure 
 * \sa     twIf_Create
 */ 
TI_STATUS twIf_Destroy (TI_HANDLE hTwIf)
{
    TTwIfObj *pTwIf = (TTwIfObj*)hTwIf;

    if (pTwIf)
    {
        txnQ_Close (pTwIf->hTxnQ, TXN_FUNC_ID_WLAN);
        if (pTwIf->hTxnDoneQueue)
        {
            que_Destroy (pTwIf->hTxnDoneQueue);
        }
        if (pTwIf->hPendRestartTimer)
        {
            tmr_DestroyTimer (pTwIf->hPendRestartTimer);
        }
        os_memoryFree (pTwIf->hOs, pTwIf, sizeof(TTwIfObj));     
    }
    return TI_OK;
}


/** 
 * \fn     twIf_Init 
 * \brief  Init module 
 * 
 * - Init required handles and module variables
 * - Create the TxnDone-queue
 * - Register to TxnQ
 * - Register to context module
 * 
 * \note    
 * \param  hTwIf       - The module's object
 * \param  hXxx        - Handles to other modules
 * \param  fRecoveryCb - Callback function for recovery completed after TxnDone
 * \param  hRecoveryCb - Handle for fRecoveryCb
 * \return void        
 * \sa     
 */ 
void twIf_Init (TI_HANDLE hTwIf, 
                TI_HANDLE hReport, 
                TI_HANDLE hContext, 
                TI_HANDLE hTimer, 
                TI_HANDLE hTxnQ, 
                TRecoveryCb fRecoveryCb, 
                TI_HANDLE hRecoveryCb)
{
    TTwIfObj   *pTwIf = (TTwIfObj*)hTwIf;
    TI_UINT32   uNodeHeaderOffset;
    TTxnStruct *pTxnHdr;   /* The ELP transactions header (as used in the TxnQ API) */

    pTwIf->hReport          = hReport;
    pTwIf->hContext         = hContext;
    pTwIf->hTimer           = hTimer;
    pTwIf->hTxnQ            = hTxnQ;
    pTwIf->fRecoveryCb      = fRecoveryCb;
    pTwIf->hRecoveryCb      = hRecoveryCb;

    /* Prepare ELP sleep transaction */
    pTwIf->tElpTxnSleep.uElpData = ELP_CTRL_REG_SLEEP;
    pTxnHdr = &(pTwIf->tElpTxnSleep.tHdr);
    TXN_PARAM_SET(pTxnHdr, TXN_LOW_PRIORITY, TXN_FUNC_ID_WLAN, TXN_DIRECTION_WRITE, TXN_INC_ADDR)
    TXN_PARAM_SET_MORE(pTxnHdr, 0);         /* Sleep is the last transaction! */
    /* NOTE: Function id for single step will be replaced to 0 by the bus driver */
    TXN_PARAM_SET_SINGLE_STEP(pTxnHdr, 1);  /* ELP write is always single step (TxnQ is topped)! */
    BUILD_TTxnStruct(pTxnHdr, ELP_CTRL_REG_ADDR, &(pTwIf->tElpTxnSleep.uElpData), sizeof(TI_UINT8), NULL, NULL)

    /* Prepare ELP awake transaction */
    pTwIf->tElpTxnAwake.uElpData = ELP_CTRL_REG_AWAKE;
    pTxnHdr = &(pTwIf->tElpTxnAwake.tHdr);
    TXN_PARAM_SET(pTxnHdr, TXN_LOW_PRIORITY, TXN_FUNC_ID_WLAN, TXN_DIRECTION_WRITE, TXN_INC_ADDR)
    TXN_PARAM_SET_MORE(pTxnHdr, 1);         
    /* NOTE: Function id for single step will be replaced to 0 by the bus driver */
    TXN_PARAM_SET_SINGLE_STEP(pTxnHdr, 1);  /* ELP write is always single step (TxnQ is topped)! */
    BUILD_TTxnStruct(pTxnHdr, ELP_CTRL_REG_ADDR, &(pTwIf->tElpTxnAwake.uElpData), sizeof(TI_UINT8), NULL, NULL)

    /* Create the TxnDone queue. */
    uNodeHeaderOffset = TI_FIELD_OFFSET(TTxnStruct, tTxnQNode); 
    pTwIf->hTxnDoneQueue = que_Create (pTwIf->hOs, pTwIf->hReport, TXN_DONE_QUE_SIZE, uNodeHeaderOffset);
    if (pTwIf->hTxnDoneQueue == NULL)
    {
        TRACE0(pTwIf->hReport, REPORT_SEVERITY_ERROR, "twIf_Init: TxnDone queue creation failed!\n");
    }

    /* Register to the context engine and get the client ID */
    pTwIf->uContextId = context_RegisterClient (pTwIf->hContext,
                                                twIf_HandleTxnDone,
                                                hTwIf,
                                                TI_TRUE,
                                                "TWIF",
                                                sizeof("TWIF"));

	/* Allocate timer */
	pTwIf->hPendRestartTimer = tmr_CreateTimer (hTimer);
	if (pTwIf->hPendRestartTimer == NULL)
	{
        TRACE0(pTwIf->hReport, REPORT_SEVERITY_ERROR, "twIf_Init: Failed to create PendRestartTimer!\n");
        return;
	}
    pTwIf->bPendRestartTimerRunning = TI_FALSE;

    /* Register to TxnQ */
    txnQ_Open (pTwIf->hTxnQ, TXN_FUNC_ID_WLAN, TXN_NUM_PRIORITYS, (TTxnQueueDoneCb)twIf_TxnDoneCb, hTwIf);

    /* Restart TwIf and TxnQ modules */
    twIf_Restart (hTwIf);
}


/** 
 * \fn     twIf_Restart
 * \brief  Restart module upon driver stop or recovery
 * 
 * Called upon driver stop command or upon recovery. 
 * Calls txnQ_Restart to clear the WLAN queues and call the TxnDone CB on each tansaction.
 * If no transaction in progress, the queues are cleared immediately. 
 * If a transaction is in progress, it is done upon TxnDone.
 * The status in transactions that were dropped due to restart is TXN_STATUS_RECOVERY,
 *     and its originator (Xfer module) handles it if required (if its CB was written in the Txn).
 * 
 * \note   
 * \param  hTwIf - The module's object
 * \return COMPLETE if the WLAN queues were restarted, PENDING if waiting for TxnDone to restart queues
 * \sa     
 */ 
ETxnStatus twIf_Restart (TI_HANDLE hTwIf)
{
    TTwIfObj    *pTwIf = (TTwIfObj*)hTwIf;
    ETxnStatus  eStatus;

    pTwIf->eState           = SM_STATE_SLEEP;
    pTwIf->uAwakeReqCount   = 0;

    pTwIf->uPendingTxnCount = 0;

    /* Clear done queue */
    twIf_ClearTxnDoneQueue(hTwIf);

    /* Restart WLAN queues */
    eStatus = txnQ_Restart (pTwIf->hTxnQ, TXN_FUNC_ID_WLAN);

    /* If pending upon ongoing transaction, start guard timer in case SDIO does not call us back */
    if (eStatus == TXN_STATUS_PENDING) 
    {
        pTwIf->bPendRestartTimerRunning = TI_TRUE;
		tmr_StartTimer (pTwIf->hPendRestartTimer, twIf_PendRestratTimeout, hTwIf, PEND_RESTART_TIMEOUT, TI_FALSE);
    }

    /* Return result (COMPLETE if completed or PENDING if will be completed in TxnDone context) */
    return eStatus;
}


/** 
 * \fn     twIf_RegisterErrCb
 * \brief  Register Error CB
 * 
 * Register upper layer (health monitor) CB for bus error
 * 
 * \note   
 * \param  hTwIf  - The module's object
 * \param  fErrCb - The upper layer CB function for error handling 
 * \param  hErrCb - The CB function handle
 * \return void
 * \sa     
 */ 
void twIf_RegisterErrCb (TI_HANDLE hTwIf, void *fErrCb, TI_HANDLE hErrCb)
{
    TTwIfObj *pTwIf = (TTwIfObj*) hTwIf;

    /* Save upper layer (health monitor) CB for bus error */
    pTwIf->fErrCb = (TFailureEventCb)fErrCb;
    pTwIf->hErrCb = hErrCb;
}


/** 
 * \fn     twIf_WriteElpReg
 * \brief  write ELP register
 * 
 * \note   
 * \param  pTwIf   - The module's object
 * \param  uValue  - ELP_CTRL_REG_SLEEP or ELP_CTRL_REG_AWAKE
 * \return void
 * \sa     
 */ 
static void twIf_WriteElpReg (TTwIfObj *pTwIf, TI_UINT32 uValue)
{
    TRACE1(pTwIf->hReport, REPORT_SEVERITY_INFORMATION, "twIf_WriteElpReg:  ELP Txn data = 0x%x\n", uValue);
    /* Send ELP (awake or sleep) transaction to TxnQ */
    if (uValue == ELP_CTRL_REG_AWAKE)
    {
        txnQ_Transact (pTwIf->hTxnQ, &(pTwIf->tElpTxnAwake.tHdr));
    }
    else
    {
        txnQ_Transact (pTwIf->hTxnQ, &(pTwIf->tElpTxnSleep.tHdr));
    }
}


/** 
 * \fn     twIf_SetPartition
 * \brief  Set HW addresses partition
 * 
 * Called by the HwInit module to set the HW address ranges for download or working access.
 * Generate and configure the bus access address mapping table.
 * The partition is split between register (fixed partition of 24KB size, exists in all modes), 
 *     and memory (dynamically changed during init and gets constant value in run-time, 104KB size).
 * The TwIf configures the memory mapping table on the device by issuing write transaction to 
 *     table address (note that the TxnQ and bus driver see this as a regular transaction). 
 * 
 * \note In future versions, a specific bus may not support partitioning (as in wUART), 
 *       In this case the HwInit module shall not call this function (will learn the bus 
 *       configuration from the INI file).
 *
 * \param  hTwIf          - The module's object
 * \param  uMemAddr  - The memory partition base address
 * \param  uMemSize  - The memory partition size
 * \param  uRegAddr  - The registers partition base address
 * \param  uRegSize  - The register partition size
 * \return void
 * \sa     
 */ 

void twIf_SetPartition (TI_HANDLE hTwIf,
                        TPartition *pPartition)
{
    TTwIfObj          *pTwIf = (TTwIfObj*) hTwIf;
    TPartitionRegTxn  *pPartitionRegTxn;/* The partition transaction structure for one register */
    TTxnStruct        *pTxnHdr;         /* The partition transaction header (as used in the TxnQ API) */
    ETxnStatus         eStatus;
    int i;

    /* Save partition information for translation and validation. */
    pTwIf->uMemAddr1 = pPartition[0].uMemAdrr;
    pTwIf->uMemSize1 = pPartition[0].uMemSize;
    pTwIf->uMemAddr2 = pPartition[1].uMemAdrr;
    pTwIf->uMemSize2 = pPartition[1].uMemSize;
    pTwIf->uMemAddr3 = pPartition[2].uMemAdrr;
    pTwIf->uMemSize3 = pPartition[2].uMemSize;
    pTwIf->uMemAddr4 = pPartition[3].uMemAdrr;

    /* Allocate memory for the current 4 partition transactions */
    pPartitionRegTxn = (TPartitionRegTxn *) os_memoryAlloc (pTwIf->hOs, 7*sizeof(TPartitionRegTxn));
    pTxnHdr       = &(pPartitionRegTxn->tHdr);

    /* Zero the allocated memory to be certain that unused fields will be initialized */
    os_memoryZero(pTwIf->hOs, pPartitionRegTxn, 7*sizeof(TPartitionRegTxn));

    /* Prepare partition transaction data */
    pPartitionRegTxn[0].tData  = ENDIAN_HANDLE_LONG(pTwIf->uMemAddr1);
    pPartitionRegTxn[1].tData  = ENDIAN_HANDLE_LONG(pTwIf->uMemSize1);
    pPartitionRegTxn[2].tData  = ENDIAN_HANDLE_LONG(pTwIf->uMemAddr2);
    pPartitionRegTxn[3].tData  = ENDIAN_HANDLE_LONG(pTwIf->uMemSize2);
    pPartitionRegTxn[4].tData  = ENDIAN_HANDLE_LONG(pTwIf->uMemAddr3);
    pPartitionRegTxn[5].tData  = ENDIAN_HANDLE_LONG(pTwIf->uMemSize3);
    pPartitionRegTxn[6].tData  = ENDIAN_HANDLE_LONG(pTwIf->uMemAddr4);


    /* Prepare partition Txn header */
    for (i=0; i<7; i++)
    {
        pTxnHdr = &(pPartitionRegTxn[i].tHdr);
        TXN_PARAM_SET(pTxnHdr, TXN_LOW_PRIORITY, TXN_FUNC_ID_WLAN, TXN_DIRECTION_WRITE, TXN_INC_ADDR)
        TXN_PARAM_SET_MORE(pTxnHdr, 1);         
        TXN_PARAM_SET_SINGLE_STEP(pTxnHdr, 0);
    }


    /* Memory address */
    pTxnHdr = &(pPartitionRegTxn[0].tHdr);
    BUILD_TTxnStruct(pTxnHdr, PARTITION_REGISTERS_ADDR+4,  &(pPartitionRegTxn[0].tData), REGISTER_SIZE, 0, 0)
    twIf_SendTransaction (pTwIf, pTxnHdr);

    /* Memory size */
    pTxnHdr = &(pPartitionRegTxn[1].tHdr);
    BUILD_TTxnStruct(pTxnHdr, PARTITION_REGISTERS_ADDR+0,  &(pPartitionRegTxn[1].tData), REGISTER_SIZE, 0, 0)
    twIf_SendTransaction (pTwIf, pTxnHdr);

    /* Registers address */
    pTxnHdr = &(pPartitionRegTxn[2].tHdr);
    BUILD_TTxnStruct(pTxnHdr, PARTITION_REGISTERS_ADDR+12, &(pPartitionRegTxn[2].tData), REGISTER_SIZE, 0, 0)
    twIf_SendTransaction (pTwIf, pTxnHdr);

    /* Registers size */
    pTxnHdr = &(pPartitionRegTxn[3].tHdr);
    BUILD_TTxnStruct(pTxnHdr, PARTITION_REGISTERS_ADDR+8,  &(pPartitionRegTxn[3].tData), REGISTER_SIZE, 0, 0)
    eStatus = twIf_SendTransaction (pTwIf, pTxnHdr);

 /* Registers address */
    pTxnHdr = &(pPartitionRegTxn[4].tHdr);
    BUILD_TTxnStruct(pTxnHdr, PARTITION_REGISTERS_ADDR+20,  &(pPartitionRegTxn[4].tData), REGISTER_SIZE, 0, 0)
    twIf_SendTransaction (pTwIf, pTxnHdr);

 /* Registers size */
    pTxnHdr = &(pPartitionRegTxn[5].tHdr);
    BUILD_TTxnStruct(pTxnHdr, PARTITION_REGISTERS_ADDR+16,  &(pPartitionRegTxn[5].tData), REGISTER_SIZE, 0, 0)
    eStatus = twIf_SendTransaction (pTwIf, pTxnHdr);

 /* Registers address */
    pTxnHdr = &(pPartitionRegTxn[6].tHdr);
    BUILD_TTxnStruct(pTxnHdr, PARTITION_REGISTERS_ADDR+24,  &(pPartitionRegTxn[6].tData), REGISTER_SIZE, twIf_PartitionTxnDoneCb, pTwIf)
    twIf_SendTransaction (pTwIf, pTxnHdr);

    /* If the transaction is done, free the allocated memory (otherwise freed in the partition CB) */
    if (eStatus != TXN_STATUS_PENDING) 
    {
        os_memoryFree (pTwIf->hOs, pPartitionRegTxn,7*sizeof(TPartitionRegTxn));
    }
}


static void twIf_PartitionTxnDoneCb (TI_HANDLE hTwIf, void *hTxn)
{
    TTwIfObj *pTwIf = (TTwIfObj*) hTwIf;

    /* Free the partition transaction buffer after completed (see transaction above) */
    os_memoryFree (pTwIf->hOs, 
                   (char *)hTxn - (6 * sizeof(TPartitionRegTxn)),  /* Move back to the first Txn start */
                   7 * sizeof(TPartitionRegTxn)); 
}


/** 
 * \fn     twIf_Awake
 * \brief  Request to keep the device awake
 * 
 * Used by the Xfer modules to request to keep the device awake until twIf_Sleep() is called.
 * Each call to this function increments AwakeReq counter. Once the device is awake (upon transaction), 
 *     the TwIf SM keeps it awake as long as this counter is not zero.
 * 
 * \note   
 * \param  hTwIf - The module's object
 * \return void
 * \sa     twIf_Sleep
 */ 
void twIf_Awake (TI_HANDLE hTwIf)
{
    TTwIfObj *pTwIf = (TTwIfObj*) hTwIf;

    /* Increment awake requests counter */
    pTwIf->uAwakeReqCount++;

#ifdef TI_DBG
    pTwIf->uDbgCountAwake++;
    TRACE1(pTwIf->hReport, REPORT_SEVERITY_INFORMATION, "twIf_Awake: uAwakeReqCount = %d\n", pTwIf->uAwakeReqCount);
#endif
}


/** 
 * \fn     twIf_Sleep
 * \brief  Remove request to keep the device awake
 * 
 * Each call to this function decrements AwakeReq counter.
 * Once this counter is zeroed, if the TxnQ is empty (no WLAN transactions), the TwIf SM is 
 *     invoked to stop the TxnQ and enable the device to sleep (write 0 to ELP register).
 * 
 * \note   
 * \param  hTwIf - The module's object
 * \return void
 * \sa     twIf_Awake
 */ 
void twIf_Sleep (TI_HANDLE hTwIf)
{
    TTwIfObj *pTwIf = (TTwIfObj*) hTwIf;

    /* Decrement awake requests counter */
    if (pTwIf->uAwakeReqCount > 0) /* in case of redundant call after recovery */
    {   
    pTwIf->uAwakeReqCount--;
    }

#ifdef TI_DBG
    pTwIf->uDbgCountSleep++;
    TRACE1(pTwIf->hReport, REPORT_SEVERITY_INFORMATION, "twIf_Sleep: uAwakeReqCount = %d\n", pTwIf->uAwakeReqCount);
#endif

    /* If Awake not required and no pending transactions in TxnQ, issue Sleep event to SM */
    if ((pTwIf->uAwakeReqCount == 0) && (pTwIf->uPendingTxnCount == 0))
    {
        twIf_HandleSmEvent (pTwIf, SM_EVENT_SLEEP);
    }
}


/** 
 * \fn     twIf_HwAvailable
 * \brief  The device is awake
 * 
 * This is an indication from the FwEvent that the device is awake.
 * Issue HW_AVAILABLE event to the SM.
 * 
 * \note   
 * \param  hTwIf - The module's object
 * \return void
 * \sa     
 */ 
void twIf_HwAvailable (TI_HANDLE hTwIf)
{
    TTwIfObj *pTwIf = (TTwIfObj*) hTwIf;

    TRACE0(pTwIf->hReport, REPORT_SEVERITY_INFORMATION, "twIf_HwAvailable: HW is Available\n");

    /* Issue HW_AVAILABLE event to the SM */
    twIf_HandleSmEvent (pTwIf, SM_EVENT_HW_AVAILABLE);
}


/** 
 * \fn     twIf_Transact
 * \brief  Issue a transaction
 * 
 * This method is used by the Xfer modules to issue all transaction types.
 * Translate HW address according to bus partition and call twIf_SendTransaction().
 * 
 * \note   
 * \param  hTwIf - The module's object
 * \param  pTxn  - The transaction object 
 * \return COMPLETE if the transaction was completed in this context, PENDING if not, ERROR if failed
 * \sa     twIf_SendTransaction
 */ 
ETxnStatus twIf_Transact (TI_HANDLE hTwIf, TTxnStruct *pTxn)
{
    TTwIfObj  *pTwIf   = (TTwIfObj*)hTwIf;

    /* Translate HW address for registers region */
    if ((pTxn->uHwAddr >= pTwIf->uMemAddr2) && (pTxn->uHwAddr <= pTwIf->uMemAddr2 + pTwIf->uMemSize2))
    {
        pTxn->uHwAddr = pTxn->uHwAddr - pTwIf->uMemAddr2 + pTwIf->uMemSize1;
    }
    /* Translate HW address for memory region */
    else 
    {
        pTxn->uHwAddr = pTxn->uHwAddr - pTwIf->uMemAddr1;
    }

    /* Regular transaction are not the last and are not single step (only ELP write is) */
    TXN_PARAM_SET_MORE(pTxn, 1);
    TXN_PARAM_SET_SINGLE_STEP(pTxn, 0);

    /* Send the transaction to the TxnQ and update the SM if needed. */
    return twIf_SendTransaction (pTwIf, pTxn);
}

ETxnStatus twIf_TransactReadFWStatus (TI_HANDLE hTwIf, TTxnStruct *pTxn)
{
    TTwIfObj  *pTwIf   = (TTwIfObj*)hTwIf;

    /* Regular transaction are not the last and are not single step (only ELP write is) */
    TXN_PARAM_SET_MORE(pTxn, 1);         
    TXN_PARAM_SET_SINGLE_STEP(pTxn, 0);

    /* Send the transaction to the TxnQ and update the SM if needed. */  
    return twIf_SendTransaction (pTwIf, pTxn);
}


/** 
 * \fn     twIf_SendTransaction
 * \brief  Send a transaction to the device
 * 
 * This method is used by the Xfer modules and the TwIf to send all transaction types to the device.
 * Send the transaction to the TxnQ and update the SM if needed.
 * 
 * \note   
 * \param  pTwIf - The module's object
 * \param  pTxn  - The transaction object 
 * \return COMPLETE if the transaction was completed in this context, PENDING if not, ERROR if failed
 * \sa     
 */ 
static ETxnStatus twIf_SendTransaction (TTwIfObj *pTwIf, TTxnStruct *pTxn)
{
    ETxnStatus eStatus;
#ifdef TI_DBG
    TI_UINT32  data = 0;

    /* Verify that the Txn HW-Address is 4-bytes aligned */
    if (pTxn->uHwAddr & 0x3)
    {
        TRACE2(pTwIf->hReport, REPORT_SEVERITY_ERROR, "twIf_SendTransaction: Unaligned HwAddr! HwAddr=0x%x, Params=0x%x\n", pTxn->uHwAddr, pTxn->uTxnParams);
		return TXN_STATUS_ERROR;
    }	
#endif

    context_EnterCriticalSection (pTwIf->hContext);
    /* increment pending Txn counter */
    pTwIf->uPendingTxnCount++;
    context_LeaveCriticalSection (pTwIf->hContext);

    /* Send transaction to TxnQ */
    eStatus = txnQ_Transact(pTwIf->hTxnQ, pTxn);

#ifdef TI_DBG
    pTwIf->uDbgCountTxn++;
    if      (eStatus == TXN_STATUS_COMPLETE) { pTwIf->uDbgCountTxnComplete++; }
    else if (eStatus == TXN_STATUS_PENDING ) { pTwIf->uDbgCountTxnPending++;  }

	COPY_WLAN_LONG(&data,&(pTxn->aBuf[0]));
    TRACE8(pTwIf->hReport, REPORT_SEVERITY_INFORMATION, "twIf_SendTransaction: Status = %d, Params=0x%x, HwAddr=0x%x, Len0=%d, Len1=%d, Len2=%d, Len3=%d, Data=0x%x \n", eStatus, pTxn->uTxnParams, pTxn->uHwAddr, pTxn->aLen[0], pTxn->aLen[1], pTxn->aLen[2], pTxn->aLen[3],data);
#endif

    /* If Txn status is PENDING issue Start event to the SM */
    if (eStatus == TXN_STATUS_PENDING)
    {
        twIf_HandleSmEvent (pTwIf, SM_EVENT_START);
    }

    /* Else (COMPLETE or ERROR) */
    else
    {
        context_EnterCriticalSection (pTwIf->hContext);
        /* decrement pending Txn counter in case of sync transact*/
        pTwIf->uPendingTxnCount--;
        context_LeaveCriticalSection (pTwIf->hContext);

        /* If Awake not required and no pending transactions in TxnQ, issue Sleep event to SM */
        if ((pTwIf->uAwakeReqCount == 0) && (pTwIf->uPendingTxnCount == 0))
        {
            twIf_HandleSmEvent (pTwIf, SM_EVENT_SLEEP);
        }

        /* If Txn failed and error CB available, call it to initiate recovery */
        if (eStatus == TXN_STATUS_ERROR)
        {
            TRACE6(pTwIf->hReport, REPORT_SEVERITY_ERROR, "twIf_SendTransaction: Txn failed!!  Params=0x%x, HwAddr=0x%x, Len0=%d, Len1=%d, Len2=%d, Len3=%d\n", pTxn->uTxnParams, pTxn->uHwAddr, pTxn->aLen[0], pTxn->aLen[1], pTxn->aLen[2], pTxn->aLen[3]);

            if (pTwIf->fErrCb)
            {
                pTwIf->fErrCb (pTwIf->hErrCb, BUS_FAILURE);
            }
        }
    }

    /* Return the Txn status (COMPLETE if completed in this context, PENDING if not, ERROR if failed) */
    return eStatus;
}

/** 
 * \fn     twIf_HandleSmEvent
 * \brief  The TwIf SM implementation
 * 
 * Handle SM event.
 * Control the device awake/sleep states and the TxnQ run/stop states according to the event.
 *  
 * \note   
 * \param  hTwIf - The module's object
 * \return void
 * \sa     
 */ 
static void twIf_HandleSmEvent (TTwIfObj *pTwIf, ESmEvent eEvent)
{
	ESmState eState = pTwIf->eState;  /* The state before handling the event */

    /* Switch by current state and handle event */
    switch (eState)
    {
    case SM_STATE_AWAKE:
        /* SLEEP event:  AWAKE ==> SLEEP,  stop TxnQ and set ELP reg to sleep */
        if (eEvent == SM_EVENT_SLEEP)
        {
            pTwIf->eState = SM_STATE_SLEEP;
            txnQ_Stop (pTwIf->hTxnQ, TXN_FUNC_ID_WLAN);
            twIf_WriteElpReg (pTwIf, ELP_CTRL_REG_SLEEP);
        }
        break;
    case SM_STATE_SLEEP:
        /* START event:  SLEEP ==> WAIT_HW,  set ELP reg to wake-up */
        if (eEvent == SM_EVENT_START)
        {
            pTwIf->eState = SM_STATE_WAIT_HW;
            twIf_WriteElpReg (pTwIf, ELP_CTRL_REG_AWAKE);
        }
        /* HW_AVAILABLE event:  SLEEP ==> AWAKE,  set ELP reg to wake-up and run TxnQ */
        else if (eEvent == SM_EVENT_HW_AVAILABLE)
        {
            pTwIf->eState = SM_STATE_AWAKE;
            twIf_WriteElpReg (pTwIf, ELP_CTRL_REG_AWAKE);
            txnQ_Run (pTwIf->hTxnQ, TXN_FUNC_ID_WLAN);
        }
        break;
    case SM_STATE_WAIT_HW:
        /* HW_AVAILABLE event:  WAIT_HW ==> AWAKE,  run TxnQ */
        if (eEvent == SM_EVENT_HW_AVAILABLE)
        {
            pTwIf->eState = SM_STATE_AWAKE;
            txnQ_Run (pTwIf->hTxnQ, TXN_FUNC_ID_WLAN);
        }
        break;
    }

	TRACE3(pTwIf->hReport, REPORT_SEVERITY_INFORMATION, "twIf_HandleSmEvent: <currentState = %d, event = %d> --> nextState = %d\n", eState, eEvent, pTwIf->eState);
}


/** 
 * \fn     twIf_TxnDoneCb
 * \brief  Transaction completion CB
 * 
 * This callback is called by the TxnQ upon transaction completion, unless is was completed in
 *     the original context where it was issued.
 * It may be called from bus driver external context (TxnDone ISR) or from WLAN driver context.
 *  
 * \note   
 * \param  hTwIf - The module's object
 * \param  pTxn  - The completed transaction object 
 * \return void
 * \sa     twIf_HandleTxnDone
 */ 
static void twIf_TxnDoneCb (TI_HANDLE hTwIf, TTxnStruct *pTxn)
{
    TTwIfObj *pTwIf = (TTwIfObj*)hTwIf;

#ifdef TI_DBG
    pTwIf->uDbgCountTxnDoneCb++;
    TRACE6(pTwIf->hReport, REPORT_SEVERITY_INFORMATION, "twIf_TxnDoneCb: Params=0x%x, HwAddr=0x%x, Len0=%d, Len1=%d, Len2=%d, Len3=%d\n", pTxn->uTxnParams, pTxn->uHwAddr, pTxn->aLen[0], pTxn->aLen[1], pTxn->aLen[2], pTxn->aLen[3]);
#endif

    /* In case of recovery flag, Call directly restart callback */
    if (TXN_PARAM_GET_STATUS(pTxn) == TXN_PARAM_STATUS_RECOVERY)
    {
        if (pTwIf->fRecoveryCb)
        {
            TRACE0(pTwIf->hReport, REPORT_SEVERITY_INFORMATION, "twIf_TxnDoneCb: During Recovery\n");
            pTwIf->bTxnDoneInRecovery = TI_TRUE;
            /* Request schedule to continue handling in driver context (will call twIf_HandleTxnDone()) */
            context_RequestSchedule (pTwIf->hContext, pTwIf->uContextId);
            return;
        }
    }

    /* If the completed Txn is ELP, nothing to do (not counted) so exit */
    if (TXN_PARAM_GET_SINGLE_STEP(pTxn)) 
    {
        return;
    }

    if (pTxn->fTxnDoneCb)
    {
        TI_STATUS eStatus;

        /* In critical section, enqueue the completed transaction in the TxnDoneQ. */
        context_EnterCriticalSection (pTwIf->hContext);
        eStatus = que_Enqueue (pTwIf->hTxnDoneQueue, (TI_HANDLE)pTxn);
        if (eStatus != TI_OK)
        {
            TRACE3(pTwIf->hReport, REPORT_SEVERITY_ERROR, "twIf_TxnDoneCb(): Enqueue failed, pTxn=0x%x, HwAddr=0x%x, Len0=%d\n", pTxn, pTxn->uHwAddr, pTxn->aLen[0]);
        }
        context_LeaveCriticalSection (pTwIf->hContext);
    }
    else
    {
        context_EnterCriticalSection (pTwIf->hContext);
         /* Decrement pending Txn counter, It's value will be checked in twIf_HandleTxnDone() */
        if (pTwIf->uPendingTxnCount > 0) /* in case of callback on recovery after restart */
        {
            pTwIf->uPendingTxnCount--;
        }
        context_LeaveCriticalSection (pTwIf->hContext);

    }
    
    /* Request schedule to continue handling in driver context (will call twIf_HandleTxnDone()) */
    context_RequestSchedule (pTwIf->hContext, pTwIf->uContextId);
}

/** 
 * \fn     twIf_HandleTxnDone
 * \brief  Completed transactions handler
 * 
 * The completed transactions handler, called upon TxnDone event, either from the context engine
 *     or directly from twIf_TxnDoneCb() if we are already in the WLAN driver's context.
 * Dequeue all completed transactions in critical section, and call their callbacks if available.
 * If awake is not required and no pending transactions in TxnQ, issue Sleep event to SM.
 *  
 * \note   
 * \param  hTwIf - The module's object
 * \return void
 * \sa     
 */ 
static void twIf_HandleTxnDone (TI_HANDLE hTwIf)
{
    TTwIfObj   *pTwIf = (TTwIfObj*)hTwIf;
    TTxnStruct *pTxn;

    /* In case of recovery, call the recovery callback and exit */
    if (pTwIf->bTxnDoneInRecovery)
    {
        TRACE0(pTwIf->hReport, REPORT_SEVERITY_INFORMATION, "twIf_HandleTxnDone: call RecoveryCb\n");
        pTwIf->bTxnDoneInRecovery = TI_FALSE;
        if (pTwIf->bPendRestartTimerRunning) 
        {
            tmr_StopTimer (pTwIf->hPendRestartTimer);
            pTwIf->bPendRestartTimerRunning = TI_FALSE;
        }
        pTwIf->fRecoveryCb(pTwIf->hRecoveryCb);
        return;
    }

    /* Loop while there are completed transactions to handle */
    while (1) 
    {
        /* In critical section, dequeue completed transaction from the TxnDoneQ. */
        context_EnterCriticalSection (pTwIf->hContext);
        pTxn = (TTxnStruct *) que_Dequeue (pTwIf->hTxnDoneQueue);
        context_LeaveCriticalSection (pTwIf->hContext);

        /* If no more transactions to handle, exit */
        if (pTxn != NULL)
        {
            context_EnterCriticalSection (pTwIf->hContext);
            /* Decrement pending Txn counter */ 
            if (pTwIf->uPendingTxnCount > 0) /* in case of callback on recovery after restart */
            {   
                pTwIf->uPendingTxnCount--;
            }
            context_LeaveCriticalSection (pTwIf->hContext);

            TRACE4(pTwIf->hReport, REPORT_SEVERITY_INFORMATION, "twIf_HandleTxnDone: Completed-Txn: Params=0x%x, HwAddr=0x%x, Len0=%d, fTxnDoneCb=0x%x\n", pTxn->uTxnParams, pTxn->uHwAddr, pTxn->aLen[0], pTxn->fTxnDoneCb);
    
            /* If Txn failed and error CB available, call it to initiate recovery */
            if (TXN_PARAM_GET_STATUS(pTxn) == TXN_PARAM_STATUS_ERROR)
            {
                TRACE6(pTwIf->hReport, REPORT_SEVERITY_ERROR, "twIf_HandleTxnDone: Txn failed!!  Params=0x%x, HwAddr=0x%x, Len0=%d, Len1=%d, Len2=%d, Len3=%d\n", pTxn->uTxnParams, pTxn->uHwAddr, pTxn->aLen[0], pTxn->aLen[1], pTxn->aLen[2], pTxn->aLen[3]);
    
                if (pTwIf->fErrCb)
                {
                    pTwIf->fErrCb (pTwIf->hErrCb, BUS_FAILURE);
                }
                /* in error do not continue */
		return;
            }
    
            /* If Txn specific CB available, call it (may free Txn resources and issue new Txns) */
            if (pTxn->fTxnDoneCb != NULL)
            {
                ((TTxnDoneCb)(pTxn->fTxnDoneCb)) (pTxn->hCbHandle, pTxn);
            } 
        }

        /*If uPendingTxnCount == 0 and awake not required, issue Sleep event to SM */
        if ((pTwIf->uAwakeReqCount == 0) && (pTwIf->uPendingTxnCount == 0))
        {
            twIf_HandleSmEvent (pTwIf, SM_EVENT_SLEEP);
        }

        if (pTxn == NULL)
        {
            return;
        }
    }
} 

/** 
 * \fn     twIf_ClearTxnDoneQueue
 * \brief  Clean the DoneQueue
 * 
 * Clear the specified done queue - don't call the callbacks.
 *  
 * \note   
 * \param  hTwIf - The module's object
 * \return void
 * \sa     
 */ 
static void twIf_ClearTxnDoneQueue (TI_HANDLE hTwIf)
{
    TTwIfObj   *pTwIf = (TTwIfObj*)hTwIf;
    TTxnStruct *pTxn;

    /* Loop while there are completed transactions to handle */
    while (1) 
    {
        /* In critical section, dequeue completed transaction from the TxnDoneQ. */
        context_EnterCriticalSection (pTwIf->hContext);
        pTxn = (TTxnStruct *) que_Dequeue (pTwIf->hTxnDoneQueue);
        context_LeaveCriticalSection (pTwIf->hContext);

        /* If no more transactions to handle, exit */
        if (pTxn != NULL)
        {
            /* Decrement pending Txn counter */ 
            if (pTwIf->uPendingTxnCount > 0) /* in case of callback on recovery after restart */
            {   
                pTwIf->uPendingTxnCount--;
            }
            
            /* 
             * Drop on Recovery 
             * do not call pTxn->fTxnDoneCb (pTxn->hCbHandle, pTxn) callback 
             */
        }

        if (pTxn == NULL)
        {
            return;
        }
    }
}


/** 
 * \fn     twIf_PendRestratTimeout
 * \brief  Pending restart process timeout handler
 * 
 * Called if timer expires upon fail to complete the last bus transaction that was 
 *   pending during restart process.
 * Calls the recovery callback to continue the restart process.
 *  
 * \note   
 * \param  hTwIf - The module's object
 * \return void
 * \sa     
 */ 
static void twIf_PendRestratTimeout (TI_HANDLE hTwIf, TI_BOOL bTwdInitOccured)
{
    TTwIfObj *pTwIf = (TTwIfObj*)hTwIf;

    TRACE0(pTwIf->hReport, REPORT_SEVERITY_ERROR, "twIf_PendRestratTimeout: restart timer expired!\n");

    pTwIf->bPendRestartTimerRunning = TI_FALSE;

    /* Clear the Txn queues since TxnDone wasn't called so it wasn't done by the TxnQ module */
    txnQ_ClearQueues (pTwIf->hTxnQ, TXN_FUNC_ID_WLAN);

    /* Call the recovery callback to continue the restart process */
    pTwIf->fRecoveryCb(pTwIf->hRecoveryCb);
}


TI_BOOL	twIf_isValidMemoryAddr(TI_HANDLE hTwIf, TI_UINT32 Address, TI_UINT32 Length)
{
    TTwIfObj   *pTwIf = (TTwIfObj*)hTwIf;

	if ((Address >= pTwIf->uMemAddr1) &&
			(Address + Length < pTwIf->uMemAddr1 + pTwIf->uMemSize1 ))
	return TI_TRUE;

	return TI_FALSE;
}

TI_BOOL	twIf_isValidRegAddr(TI_HANDLE hTwIf, TI_UINT32 Address, TI_UINT32 Length)
{
    TTwIfObj   *pTwIf = (TTwIfObj*)hTwIf;

	if ((Address >= pTwIf->uMemAddr2 ) &&
		( Address < pTwIf->uMemAddr2 + pTwIf->uMemSize2 ))
	return TI_TRUE;

	return TI_FALSE;
}

/*******************************************************************************
*                       DEBUG  FUNCTIONS  IMPLEMENTATION					   *
********************************************************************************/

#ifdef TI_DBG

/** 
 * \fn     twIf_PrintModuleInfo
 * \brief  Print module's parameters (debug)
 * 
 * This function prints the module's parameters.
 * 
 * \note   
 * \param  hTwIf - The module's object                                          
 * \return void 
 * \sa     
 */ 
void twIf_PrintModuleInfo (TI_HANDLE hTwIf) 
{
#ifdef REPORT_LOG
	TTwIfObj *pTwIf = (TTwIfObj*)hTwIf;
	
	WLAN_OS_REPORT(("-------------- TwIf Module Info-- ------------------------\n"));
	WLAN_OS_REPORT(("==========================================================\n"));
	WLAN_OS_REPORT(("eSmState             = %d\n",   pTwIf->eState					));
	WLAN_OS_REPORT(("uContextId           = %d\n",   pTwIf->uContextId              ));
	WLAN_OS_REPORT(("fErrCb               = %d\n",   pTwIf->fErrCb                  ));
	WLAN_OS_REPORT(("hErrCb               = %d\n",   pTwIf->hErrCb                  ));
	WLAN_OS_REPORT(("uAwakeReqCount       = %d\n",   pTwIf->uAwakeReqCount          ));
	WLAN_OS_REPORT(("uPendingTxnCount     = %d\n",   pTwIf->uPendingTxnCount        ));
	WLAN_OS_REPORT(("uMemAddr             = 0x%x\n", pTwIf->uMemAddr1               ));
	WLAN_OS_REPORT(("uMemSize             = 0x%x\n", pTwIf->uMemSize1               ));
	WLAN_OS_REPORT(("uRegAddr             = 0x%x\n", pTwIf->uMemAddr2               ));
	WLAN_OS_REPORT(("uRegSize             = 0x%x\n", pTwIf->uMemSize2               ));
	WLAN_OS_REPORT(("uDbgCountAwake       = %d\n",   pTwIf->uDbgCountAwake          ));
	WLAN_OS_REPORT(("uDbgCountSleep       = %d\n",   pTwIf->uDbgCountSleep          ));
	WLAN_OS_REPORT(("uDbgCountTxn         = %d\n",   pTwIf->uDbgCountTxn            ));
	WLAN_OS_REPORT(("uDbgCountTxnPending  = %d\n",   pTwIf->uDbgCountTxnPending     ));
	WLAN_OS_REPORT(("uDbgCountTxnComplete = %d\n",   pTwIf->uDbgCountTxnComplete    ));
	WLAN_OS_REPORT(("uDbgCountTxnDone     = %d\n",   pTwIf->uDbgCountTxnDoneCb      ));
	WLAN_OS_REPORT(("==========================================================\n\n"));
#endif
} 


void twIf_PrintQueues (TI_HANDLE hTwIf)
{
    TTwIfObj *pTwIf = (TTwIfObj*)hTwIf;

    txnQ_PrintQueues(pTwIf->hTxnQ);
}

#endif /* TI_DBG */
