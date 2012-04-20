/*
 * WspiBusDrv.c
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

 
/** \file   WspiBusDrv.c 
 *  \brief  The WSPI bus driver upper layer. Platform independent. 
 *          Uses the SpiAdapter API. 
 *          Introduces a generic bus-independent API upwards.
 *  
 *  \see    BusDrv.h, SpiAdapter.h, SpiAdapter.c
 */

#include "tidef.h"
#include "report.h"
#include "osApi.h"
#include "wspi.h"
#include "BusDrv.h"
#include "TxnDefs.h"
#define __FILE_ID__		FILE_ID_124

/************************************************************************
 * Defines
 ************************************************************************/
#define WSPI_FIXED_BUSY_LEN     1
#define WSPI_INIT_CMD_MASK      0


/************************************************************************
 * Types
 ************************************************************************/

/* The busDrv module Object */
typedef struct _TBusDrvObj
{
    TI_HANDLE	    hOs;		   	 
    TI_HANDLE	    hReport;
	TI_HANDLE	    hDriver;
	TI_HANDLE	    hWspi;

	TTxnDoneCb      fTxnDoneCb;         /* The callback to call upon full transaction completion. */
	TI_HANDLE       hCbHandle;          /* The callback handle */
    TTxnStruct *    pCurrTxn;           /* The transaction currently being processed */
    TI_STATUS       eCurrTxnStatus;     /* COMPLETE, PENDING or ERROR */
    TI_UINT32       uCurrTxnBufsCount;
    TI_BOOL         bPendingByte;
    TTxnDoneCb      fTxnConnectDoneCb;         /* The callback to call upon full transaction completion. */    
    

} TBusDrvObj;


/************************************************************************
 * Internal functions prototypes
 ************************************************************************/

static void asyncEnded_CB(TI_HANDLE hBusTxn, int status);
static void ConnectDone_CB(TI_HANDLE hBusDrv, int status);

/************************************************************************
 *
 *   Module functions implementation
 *
 ************************************************************************/

/** 
 * \fn     busDrv_Create 
 * \brief  Create the module
 * 
 * Allocate and clear the module's object.
 * 
 * \note   
 * \param  hOs - Handle to Os Abstraction Layer
 * \return Handle of the allocated object, NULL if allocation failed 
 * \sa     busDrv_Destroy
 */ 
TI_HANDLE busDrv_Create (TI_HANDLE hOs)
{
    TI_HANDLE   hBusDrv;
    TBusDrvObj *pBusDrv;

    hBusDrv = os_memoryAlloc(hOs, sizeof(TBusDrvObj));
    if (hBusDrv == NULL)
        return NULL;
    
    pBusDrv = (TBusDrvObj *)hBusDrv;

    os_memoryZero(hOs, hBusDrv, sizeof(TBusDrvObj));
    
    pBusDrv->hOs = hOs;

    // addapt to WSPI

    pBusDrv->hWspi= WSPI_Open(hOs); 

    return pBusDrv;
}


/** 
 * \fn     busDrv_Destroy
 * \brief  Destroy the module. 
 * 
 * Close SPI lower bus driver and free the module's object.
 * 
 * \note   
 * \param  The module's object
 * \return TI_OK on success or TI_NOK on failure 
 * \sa     busDrv_Create
 */ 
TI_STATUS busDrv_Destroy (TI_HANDLE hBusDrv)
{
    TBusDrvObj *pBusDrv = (TBusDrvObj*)hBusDrv;

    if (pBusDrv)
    {
        // addapt to WSPI
        WSPI_Close(pBusDrv->hWspi);
        os_memoryFree (pBusDrv->hOs, pBusDrv, sizeof(TBusDrvObj));     
    }
    return TI_OK;
}


/****************************************************************************
 *                      busDrv_Init
 ****************************************************************************
 * DESCRIPTION: config the module.
 *
 * INPUTS:  hBusDrv - handle to the module context
 *          hReport - handle to report module context that is used when we output debug messages
 *          CBFunc  - The callback to call upon transaction complete (calls the tasklet).
 *          CBArg   - The handle for the CBFunc.
 *
 * OUTPUT:  none.
 *
 * RETURNS: one of the error codes (0 => TI_OK)
 ****************************************************************************/
void busDrv_Init (TI_HANDLE    hBusDrv, 
                       TI_HANDLE    hReport)
{
    TBusDrvObj *pBusDrv = (TBusDrvObj*) hBusDrv;

    

    pBusDrv->hReport    = hReport;
    

    
}






/** 
 * \fn     busDrv_ConnectBus
 * \brief  Configure bus driver
 * 
 * Called by TxnQ.
 * Configure the bus driver with its connection configuration (such as baud-rate, bus width etc) 
 *     and establish the physical connection. 
 * Done once upon init (and not per functional driver startup). 
 * 
 * \note   
 * \param  hBusDrv    - The module's object
 * \param  pBusDrvCfg - A union used for per-bus specific configuration. 
 * \param  fCbFunc    - CB function for Async transaction completion (after all txn parts are completed).
 * \param  hCbArg     - The CB function handle
 * \return TI_OK / TI_NOK
 * \sa     
 */ 
TI_STATUS busDrv_ConnectBus (TI_HANDLE        hBusDrv, 
                             TBusDrvCfg       *pBusDrvCfg,
                             TBusDrvTxnDoneCb fCbFunc,
                             TI_HANDLE        hCbArg,
                             TBusDrvTxnDoneCb fConnectCbFunc
                             )
{
    TBusDrvObj *pBusDrv = (TBusDrvObj*)hBusDrv;
    int         iStatus;
    WSPIConfig_t wspi_config;
    WSPI_CB_T cb;

    /* Save the parameters (TxnQ callback for TxnDone events) */
    pBusDrv->fTxnDoneCb    = fCbFunc;
    pBusDrv->hCbHandle     = hCbArg;
    pBusDrv->fTxnConnectDoneCb = fConnectCbFunc;
   
    /* Configure the WSPI driver parameters  */
   
    wspi_config.isFixedAddress = TI_FALSE;
    wspi_config.fixedBusyLength = WSPI_FIXED_BUSY_LEN;
    wspi_config.mask = WSPI_INIT_CMD_MASK;

    cb.CBFunc = ConnectDone_CB;/* The BusTxn callback called upon Async transaction end. */
    cb.CBArg  = hBusDrv;   /* The handle for the BusDrv. */
 
    /* Configure the WSPI module */
    iStatus = WSPI_Configure(pBusDrv->hWspi, pBusDrv->hReport, &wspi_config, &cb);


    if ((iStatus == 0) || (iStatus == 1)) 
    {
        TRACE1 (pBusDrv->hReport, REPORT_SEVERITY_INIT, "busDrv_ConnectBus: called Status %d\n",iStatus);

    
        TRACE2 (pBusDrv->hReport, REPORT_SEVERITY_INFORMATION, "busDrv_ConnectBus: Successful Status %d\n",iStatus);
        return TI_OK;
    }
    else 
    {
        TRACE2(pBusDrv->hReport, REPORT_SEVERITY_ERROR, "busDrv_ConnectBus: Status = %d,\n", iStatus);
        return TI_NOK;
    }
}
/** 
 * \fn     busDrv_DisconnectBus
 * \brief  Disconnect SDIO driver
 * 
 * Called by TxnQ. Disconnect the SDIO driver.
 *  
 * \note   
 * \param  hBusDrv - The module's object
 * \return TI_OK / TI_NOK
 * \sa     
 */ 
TI_STATUS   busDrv_DisconnectBus   (TI_HANDLE hBusDrv)
{   
    return TI_OK;
}


/** 
 * \fn     busDrv_Transact
 * \brief  Process transaction 
 * 
 * Called by the TxnQ module to initiate a new transaction.
 * Call either write or read functions according to Txn direction field.
 * 
 * \note   It's assumed that this function is called only when idle (i.e. previous Txn is done).
 * \param  hBusDrv - The module's object
 * \param  pTxn    - The transaction object 
 * \return COMPLETE if Txn completed in this context, PENDING if not, ERROR if failed
 * \sa     busDrv_PrepareTxnParts, busDrv_SendTxnParts
 */ 
ETxnStatus busDrv_Transact (TI_HANDLE hBusDrv, TTxnStruct *pTxn)
{
    TBusDrvObj *pBusDrv = (TBusDrvObj*)hBusDrv;
    WSPI_CB_T cb;
    TI_UINT8 * tempReadBuff;
    TI_UINT8 * tempWriteBuff; 
    pBusDrv->pCurrTxn       = pTxn;
    pBusDrv->eCurrTxnStatus = TXN_STATUS_COMPLETE;  /* The Txn is Sync as long as it continues in this context */
    cb.CBFunc = asyncEnded_CB;  /* The BusTxn callback called upon Async transaction end. */
    cb.CBArg  = hBusDrv;   /* The handle for the BusTxnCB. */
  
    /* If write command */
    if (TXN_PARAM_GET_DIRECTION(pTxn) == TXN_DIRECTION_WRITE)
    {

        //WLAN_REPORT_INIT(pBusDrv->hReport, TNETW_DRV_MODULE_LOG,
        //    ("busDrv_Transact:  Write to pTxn->uHwAddr %x\n", pTxn->uHwAddr ));
    

         /*WLAN_REPORT_ERROR(pBusDrv->hReport, BUS_DRV_MODULE_LOG,
            ("busDrv_Transact: Buff= %x, Len=%d\n", pTxn->aBuf[0], pTxn->aLen[0]));*/
        /*1.write memory*/
          /* Decrease the data pointer to the beginning of the WSPI padding */
          tempWriteBuff = pTxn->aBuf[0];
          tempWriteBuff -= WSPI_PAD_LEN_WRITE;

          /* Write the data to the WSPI in Aync mode (not completed in the current context). */
          pBusDrv->eCurrTxnStatus = WSPI_WriteAsync(pBusDrv->hWspi, pTxn->uHwAddr,tempWriteBuff,pTxn->aLen[0], &cb, TI_TRUE, TI_TRUE,TXN_PARAM_GET_FIXED_ADDR(pTxn));      
          
    }

    /* If read command */
    else 
    {
        //WLAN_REPORT_INIT(pBusDrv->hReport, TNETW_DRV_MODULE_LOG,
        //    ("busDrv_Transact:  Read from pTxn->uHwAddr %x pTxn %x \n", pTxn->uHwAddr,pTxn));

        /*1. Read mem */
         /* Decrease the tempReadBuff pointer to the beginning of the WSPI padding */
        tempReadBuff = pTxn->aBuf[0];
        tempReadBuff -= WSPI_PAD_LEN_READ;
        /* Read the required data from the WSPI in Aync mode (not completed in the current context). */
        pBusDrv->eCurrTxnStatus  = WSPI_ReadAsync(pBusDrv->hWspi, pTxn->uHwAddr,tempReadBuff,pTxn->aLen[0], &cb, TI_TRUE, TI_TRUE,TXN_PARAM_GET_FIXED_ADDR(pTxn));

        
    }

    /* return transaction status - COMPLETE, PENDING or ERROR */
    return (pBusDrv->eCurrTxnStatus == WSPI_TXN_COMPLETE ? TXN_STATUS_COMPLETE : 
			(pBusDrv->eCurrTxnStatus == WSPI_TXN_PENDING ? TXN_STATUS_PENDING : TXN_STATUS_ERROR));

    
}


/****************************************************************************
 *                      asyncEnded_CB()
 ****************************************************************************
 * DESCRIPTION:  
 *      Called back by the WSPI driver from Async transaction end interrupt (ISR context).
 *      Calls the upper layers callback.
 * 
 * INPUTS:  status -    
 * 
 * OUTPUT:  None
 * 
 * RETURNS: None
 ****************************************************************************/
static void asyncEnded_CB(TI_HANDLE hBusDrv, int status)
{   
    TBusDrvObj *pBusDrv = (TBusDrvObj*)hBusDrv;
    /* If the last transaction failed, call failure CB and exit. */
    if (status != 0)
    {
        TRACE2(pBusDrv->hReport, REPORT_SEVERITY_ERROR, "asyncEnded_CB : Status = %d, fTxnDoneCb = 0x%x\n", status,pBusDrv->fTxnDoneCb);

        TXN_PARAM_SET_STATUS(pBusDrv->pCurrTxn, TXN_PARAM_STATUS_ERROR);
    }
	else
	{
		TRACE2(pBusDrv->hReport, REPORT_SEVERITY_INFORMATION,"asyncEnded_CB: Successful async cb done pBusDrv->pCurrTxn %x\n", pBusDrv->pCurrTxn);
	}

    /* Call the upper layer CB */
    
    pBusDrv->fTxnDoneCb(pBusDrv->hCbHandle,pBusDrv->pCurrTxn);
}


/****************************************************************************
 *                      ConnectDone_CB()
 ****************************************************************************
 * DESCRIPTION:  
 *      Called back by the WSPI driver from Async transaction end interrupt (ISR context).
 *      Calls the upper layers callback.
 * 
 * INPUTS:  status -    
 * 
 * OUTPUT:  None
 * 
 * RETURNS: None
 ****************************************************************************/
static void ConnectDone_CB(TI_HANDLE hBusDrv, int status)
{   
    TBusDrvObj *pBusDrv = (TBusDrvObj*)hBusDrv;
    /* If the last transaction failed, call failure CB and exit. */
    if (status != 0)
    {
        TRACE2(pBusDrv->hReport, REPORT_SEVERITY_ERROR, "ConnectDone_CB : Status = %d, fTxnConnectDoneCb = 0x%x\n", status,pBusDrv->fTxnConnectDoneCb);

        TXN_PARAM_SET_STATUS(pBusDrv->pCurrTxn, TXN_PARAM_STATUS_ERROR);
    }
	else
	{
		TRACE1 (pBusDrv->hReport, REPORT_SEVERITY_INIT, "ConnectDone_CB: Successful Connect Async cb done \n");
	}

    /* Call the upper layer CB */
    
    pBusDrv->fTxnConnectDoneCb(pBusDrv->hCbHandle,pBusDrv->pCurrTxn);
}


