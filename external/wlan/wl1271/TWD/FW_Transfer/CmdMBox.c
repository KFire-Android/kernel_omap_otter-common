/*
 * CmdMBox.c
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


/** \file  CmdMBox.c
 *  \brief Handle the wlan hardware command mailbox
 *
 *  \see CmdMBox.h, CmdMBox_api.h, CmdQueue.c
 */

#define __FILE_ID__  FILE_ID_101
#include "tidef.h"
#include "osApi.h"
#include "timer.h"
#include "report.h"
#include "FwEvent_api.h"
#include "CmdMBox_api.h"
#include "CmdMBox.h"
#include "CmdQueue_api.h"
#include "TWDriverInternal.h"
#include "TwIf.h"

/*****************************************************************************
 **         Internal functions definitions                                  **
 *****************************************************************************/

/*
 * \brief	Handle cmdMbox timeout.
 * 
 * \param  hCmdMbox  - Handle to CmdMbox
 * \return TI_OK
 * 
 * \par Description
 * Call fErrorCb() to handle the error.
 * 
 * \sa cmdMbox_SendCommand
 */
static void cmdMbox_TimeOut (TI_HANDLE hCmdMbox, TI_BOOL bTwdInitOccured);
static void cmdMbox_ConfigHwCb (TI_HANDLE hCmdMbox, TTxnStruct *pTxn);

/*
 * \brief	Create the mailbox object
 * 
 * \param  hOs  - OS module object handle
 * \return Handle to the created object
 * 
 * \par Description
 * Calling this function creates a CmdMbox object
 * 
 * \sa cmdMbox_Destroy
 */
TI_HANDLE cmdMbox_Create (TI_HANDLE hOs)
{
    TCmdMbox   *pCmdMbox;

    pCmdMbox = os_memoryAlloc (hOs, sizeof (TCmdMbox));
    if (pCmdMbox == NULL)
    {
        WLAN_OS_REPORT (("FATAL ERROR: cmdMbox_Create(): Error Creating CmdMbox - Aborting\n"));
        return NULL;
    }

    /* reset control module control block */
    os_memoryZero (hOs, pCmdMbox, sizeof (TCmdMbox));
    pCmdMbox->hOs = hOs;

    return pCmdMbox;
}


/*
 * \brief	Destroys the mailbox object
 * 
 * \param  hCmdMbox  - The object to free
 * \return TI_OK
 * 
 * \par Description
 * Calling this function destroys a CmdMbox object
 * 
 * \sa cmdMbox_Create
 */
TI_STATUS cmdMbox_Destroy (TI_HANDLE hCmdMbox)
{
    TCmdMbox   *pCmdMbox = (TCmdMbox *)hCmdMbox;

    /* free timer */
    if (pCmdMbox->hCmdMboxTimer)
    {
        tmr_DestroyTimer (pCmdMbox->hCmdMboxTimer);
    }

    /* free context */
    os_memoryFree (pCmdMbox->hOs, pCmdMbox, sizeof (TCmdMbox));

    return TI_OK;
}


/*
 * \brief	Configure the CmdMbox object
 * 
 * \param  hCmdMbox  - Handle to CmdMbox
 * \param  hReport  - Handle to report module
 * \param  hTwIf  - Handle to TwIf
 * \param  hTimer  - Handle to os timer
 * \param  hCmdQueue  - Handle to CmdQueue
 * \param  fErrorCb  - Handle to error handling function
 * \return TI_OK on success or TI_NOK on failure
 * 
 * \par Description
 * 
 * \sa
 */
TI_STATUS cmdMbox_Init (TI_HANDLE hCmdMbox,
                          TI_HANDLE             hReport, 
                        TI_HANDLE hTwIf,
                          TI_HANDLE             hTimer, 
                        TI_HANDLE hCmdQueue,
                          TCmdMboxErrorCb       fErrorCb)
{
    TCmdMbox   *pCmdMbox = (TCmdMbox *)hCmdMbox;

    pCmdMbox->hCmdQueue = hCmdQueue;
    pCmdMbox->hTwIf = hTwIf;
    pCmdMbox->hReport = hReport;

    pCmdMbox->uFwAddr = 0;
    pCmdMbox->uReadLen = 0;
    pCmdMbox->uWriteLen = 0;
    pCmdMbox->bCmdInProgress = TI_FALSE;
    pCmdMbox->fErrorCb = fErrorCb;

	/* allocate OS timer memory */
    pCmdMbox->hCmdMboxTimer = tmr_CreateTimer (hTimer);
	if (pCmdMbox->hCmdMboxTimer == NULL)
	{
        TRACE0(pCmdMbox->hReport, REPORT_SEVERITY_ERROR, "cmdMbox_Init(): Failed to create hCmdMboxTimer!\n");
		return TI_NOK;
	}

    return TI_OK;
}


/*
 * \brief	Send the Command to the Mailbox
 * 
 * \param  hCmdMbox  - Handle to CmdMbox
 * \param  cmdType  - 
 * \param  pParamsBuf  - The buffer that will be written to the mailbox
 * \param  uWriteLen  - Length of data to write to the mailbox
 * \param  uReadLen  - Length of data to read from the mailbox (when the result is received)
 * \return TI_PENDING
 * 
 * \par Description
 * Copy the buffer given to a local struct, update the write & read lengths
 * and send to the FW's mailbox.
 *             
 *       ------------------------------------------------------
 *      | CmdMbox Header | Cmd Header    | Command parameters |
 *      ------------------------------------------------------
 *      | ID   | Status  | Type | Length | Command parameters |
 *      ------------------------------------------------------
 *       16bit   16bit    16bit   16bit     
 *
 * \sa cmdMbox_CommandComplete
 */
TI_STATUS cmdMbox_SendCommand       (TI_HANDLE hCmdMbox, Command_e cmdType, TI_UINT8* pParamsBuf, TI_UINT32 uWriteLen, TI_UINT32 uReadLen)
{
    TCmdMbox   *pCmdMbox = (TCmdMbox *)hCmdMbox;
    TTxnStruct *pCmdTxn = (TTxnStruct*)&pCmdMbox->aCmdTxn[0].tTxnStruct;
    TTxnStruct *pRegTxn = (TTxnStruct*)&pCmdMbox->aRegTxn[0].tTxnStruct;
    Command_t  *pCmd = (Command_t*)&pCmdMbox->aCmdTxn[0].tCmdMbox;
    

    if (pCmdMbox->bCmdInProgress)
    {
        TRACE0(pCmdMbox->hReport, REPORT_SEVERITY_ERROR, "cmdMbox_SendCommand(): Trying to send Cmd while other Cmd is still in progres!\n");
        return TI_NOK;
    }

    /* Add the CMDMBOX_HEADER_LEN to the read length, used when reading the result later on */
    pCmdMbox->uReadLen = uReadLen + CMDMBOX_HEADER_LEN;
    /* Prepare the Cmd Hw template */
    pCmd->cmdID = cmdType;
    pCmd->cmdStatus = TI_OK;
    os_memoryCopy (pCmdMbox->hOs, (void *)pCmd->parameters, (void *)pParamsBuf, uWriteLen);

    /* Add the CMDMBOX_HEADER_LEN to the write length */
    pCmdMbox->uWriteLen = uWriteLen + CMDMBOX_HEADER_LEN;

    /* Must make sure that the length is multiple of 32 bit */
    if (pCmdMbox->uWriteLen & 0x3)
    {
        TRACE1(pCmdMbox->hReport, REPORT_SEVERITY_WARNING, "cmdMbox_SendCommand(): Command length isn't 32bit aligned! CmdId=%d\n", pCmd->cmdID);
        pCmdMbox->uWriteLen = (pCmdMbox->uWriteLen + 4) & 0xFFFFFFFC;
    }

    /* no other command can start the send process  till bCmdInProgress will return to TI_FALSE*/
    pCmdMbox->bCmdInProgress = TI_TRUE;

    /* Build the command TxnStruct */
    TXN_PARAM_SET(pCmdTxn, TXN_LOW_PRIORITY, TXN_FUNC_ID_WLAN, TXN_DIRECTION_WRITE, TXN_INC_ADDR)
    BUILD_TTxnStruct(pCmdTxn, pCmdMbox->uFwAddr, pCmd, pCmdMbox->uWriteLen, NULL, NULL)
    /* Send the command */
    twIf_Transact(pCmdMbox->hTwIf, pCmdTxn);

    /* Build the trig TxnStruct */
    pCmdMbox->aRegTxn[0].uRegister = INTR_TRIG_CMD;
    TXN_PARAM_SET(pRegTxn, TXN_LOW_PRIORITY, TXN_FUNC_ID_WLAN, TXN_DIRECTION_WRITE, TXN_INC_ADDR)
    BUILD_TTxnStruct(pRegTxn, ACX_REG_INTERRUPT_TRIG, &(pCmdMbox->aRegTxn[0].uRegister), REGISTER_SIZE, NULL, NULL)

    /* start the CmdMbox timer */
    tmr_StartTimer (pCmdMbox->hCmdMboxTimer, cmdMbox_TimeOut, hCmdMbox, CMDMBOX_WAIT_TIMEOUT, TI_FALSE);

    /* Send the FW trigger */
    twIf_Transact(pCmdMbox->hTwIf, pRegTxn);


    return TXN_STATUS_PENDING;
}


/*
 * \brief	Read the command's result
 * 
 * \param  hCmdMbox  - Handle to CmdMbox
 * \return void
 * 
 * \par Description
 * This function is called from FwEvent module uppon receiving command complete interrupt.
 * It issues a read transaction from the mailbox with a CB.
 * 
 * \sa cmdMbox_SendCommand, cmdMbox_TransferComplete
 */
ETxnStatus cmdMbox_CommandComplete (TI_HANDLE hCmdMbox)
{
    TCmdMbox   *pCmdMbox = (TCmdMbox *)hCmdMbox;
    TTxnStruct *pCmdTxn = (TTxnStruct*)&pCmdMbox->aCmdTxn[1].tTxnStruct;
    Command_t  *pCmd = (Command_t*)&pCmdMbox->aCmdTxn[1].tCmdMbox;
    ETxnStatus  rc;

    /* stop the CmdMbox timer */
    tmr_StopTimer(pCmdMbox->hCmdMboxTimer);

    /* Build the command TxnStruct */
    TXN_PARAM_SET(pCmdTxn, TXN_LOW_PRIORITY, TXN_FUNC_ID_WLAN, TXN_DIRECTION_READ, TXN_INC_ADDR)
    /* Applying a CB in case of an async read */
    BUILD_TTxnStruct(pCmdTxn, pCmdMbox->uFwAddr, pCmd, pCmdMbox->uReadLen,(TTxnDoneCb)cmdMbox_TransferComplete, hCmdMbox)
    /* Send the command */
    rc = twIf_Transact(pCmdMbox->hTwIf, pCmdTxn);

    /* In case of a sync read, call the CB directly */
    if (rc == TXN_STATUS_COMPLETE)
    {
        cmdMbox_TransferComplete(hCmdMbox);
    }

    return TXN_STATUS_COMPLETE;
}


/*
 * \brief	Calls the cmdQueue_ResultReceived.
 * 
 * \param  hCmdMbox  - Handle to CmdMbox
 * \return TI_OK
 * 
 * \par Description
 * This function is called from cmdMbox_CommandComplete on a sync read, or from TwIf as a CB on an async read.
 * It calls cmdQueue_ResultReceived to continue the result handling procces & switch the bCmdInProgress flag to TI_FALSE, 
 * meaning other commands can be sent to the FW.
 * 
 * \sa cmdMbox_SendCommand, cmdMbox_TransferComplete
 */
TI_STATUS cmdMbox_TransferComplete(TI_HANDLE hCmdMbox)
{
    TCmdMbox   *pCmdMbox = (TCmdMbox *)hCmdMbox;

    /* Other commands can be sent to the FW */
    pCmdMbox->bCmdInProgress = TI_FALSE;

    cmdQueue_ResultReceived(pCmdMbox->hCmdQueue);
    
    return TI_OK;
}


/*
 * \brief	Handle cmdMbox timeout.
 * 
 * \param  hCmdMbox  - Handle to CmdMbox
 * \return TI_OK
 * 
 * \par Description
 * Call fErrorCb() to handle the error.
 * 
 * \sa cmdMbox_SendCommand
 */
static void cmdMbox_TimeOut (TI_HANDLE hCmdMbox, TI_BOOL bTwdInitOccured)
{
    TCmdMbox   *pCmdMbox = (TCmdMbox *)hCmdMbox;
    Command_t  *pCmd = (Command_t*)&pCmdMbox->aCmdTxn[0].tCmdMbox;

    TRACE0(pCmdMbox->hReport, REPORT_SEVERITY_ERROR , "cmdMbox_TimeOut: Timeout occured in CmdMbox\n");

    /* Call error CB */
    if (pCmdMbox->fErrorCb != NULL)
    {
        pCmdMbox->fErrorCb (pCmdMbox->hCmdQueue, 
                            (TI_UINT32)pCmd->cmdID, 
                            CMD_STATUS_TIMEOUT, 
                            (void *)pCmd->parameters);
    }
}


/*
 * \brief	configure the mailbox address.
 * 
 * \param  hCmdMbox  - Handle to CmdMbox
 * \param  fCb  - Pointer to the CB
 * \param  hCb  - Cb's handle
 * \return TI_OK or TI_PENDING
 * 
 * \par Description
 * Called from HwInit to read the command mailbox address.
 * 
 * \sa
 */
TI_STATUS cmdMbox_ConfigHw (TI_HANDLE hCmdMbox, fnotify_t fCb, TI_HANDLE hCb)
{
    TCmdMbox   *pCmdMbox = (TCmdMbox *)hCmdMbox;
    TTxnStruct *pRegTxn = (TTxnStruct*)&pCmdMbox->aRegTxn[1].tTxnStruct;
    TI_STATUS   rc;

    pCmdMbox->fCb = fCb;
    pCmdMbox->hCb = hCb;
    /* Build the command TxnStruct */
    TXN_PARAM_SET(pRegTxn, TXN_LOW_PRIORITY, TXN_FUNC_ID_WLAN, TXN_DIRECTION_READ, TXN_INC_ADDR)
    BUILD_TTxnStruct(pRegTxn, REG_COMMAND_MAILBOX_PTR, &(pCmdMbox->aRegTxn[1].uRegister), REGISTER_SIZE,(TTxnDoneCb)cmdMbox_ConfigHwCb, hCmdMbox)
    /* Get the command mailbox address */
    rc = twIf_Transact(pCmdMbox->hTwIf, pRegTxn);
    if (rc == TXN_STATUS_COMPLETE)
    {
        pCmdMbox->uFwAddr = pCmdMbox->aRegTxn[1].uRegister;
    }

    return rc;
}


/*
 * \brief	Cb to cmdMbox_ConfigHw
 * 
 * \param  hCmdMbox  - Handle to CmdMbox
 * \return TI_OK
 * 
 * \par Description
 * 
 * \sa
 */
static void cmdMbox_ConfigHwCb (TI_HANDLE hCmdMbox, TTxnStruct *pTxn)
{
    TCmdMbox   *pCmdMbox = (TCmdMbox *)hCmdMbox;

    pCmdMbox->uFwAddr = pCmdMbox->aRegTxn[1].uRegister;

    /* Call back the original State Machine */
    pCmdMbox->fCb(pCmdMbox->hCb, TI_OK);
}


/*
 * \brief	Restart the module upon driver stop or restart
 * 
 * \param  hCmdMbox  - Handle to CmdMbox
 * \return TI_OK
 * 
 * \par Description
 * 
 * \sa
 */
TI_STATUS cmdMbox_Restart (TI_HANDLE hCmdMbox)
{
    TCmdMbox   *pCmdMbox = (TCmdMbox *)hCmdMbox;

    /* Stop the timeout timer if running and reset the state */
    tmr_StopTimer (pCmdMbox->hCmdMboxTimer);
    pCmdMbox->bCmdInProgress = TI_FALSE;
    pCmdMbox->uReadLen       = 0;
    pCmdMbox->uWriteLen      = 0;

    return TI_OK;
}


/*
 * \brief	Return the latest command status
 * 
 * \param  hCmdMbox  - Handle to CmdMbox
 * \return TI_OK or TI_NOK
 * 
 * \par Description
 * 
 * \sa
 */
TI_STATUS cmdMbox_GetStatus (TI_HANDLE hCmdMbox, CommandStatus_e *cmdStatus)
{
    TCmdMbox   *pCmdMbox = (TCmdMbox *)hCmdMbox;
    Command_t  *pCmd = (Command_t*)&pCmdMbox->aCmdTxn[1].tCmdMbox;
    TI_STATUS   status;

    status = (pCmd->cmdStatus == CMD_STATUS_SUCCESS) ? TI_OK : TI_NOK;
    TRACE2(pCmdMbox->hReport, REPORT_SEVERITY_INFORMATION , "cmdMbox_GetStatus: TI_STATUS = (%d) <= pCmdMbox->tCmdMbox.cmdStatus = %d\n", status, pCmd->cmdStatus);
    *cmdStatus = pCmd->cmdStatus;
    return status;
}

/*
 * \brief	Return the MBox address
 * 
 * \param  hCmdMbox  - Handle to CmdMbox
 * \return MBox address
 * 
 * \par Description
 * 
 * \sa
 */
TI_UINT32 cmdMbox_GetMboxAddress (TI_HANDLE hCmdMbox)
{
    TCmdMbox *pCmdMbox = (TCmdMbox *)hCmdMbox;

    return pCmdMbox->uFwAddr;
}


/*
 * \brief	Return the Command parameters buffer
 * 
 * \param  hCmdMbox  - Handle to CmdMbox
 * \param  pParamBuf  - Holds the returned buffer
 * \return
 * 
 * \par Description
 * Copying the command's data to pParamBuf
 * 
 * \sa
 */
void cmdMbox_GetCmdParams (TI_HANDLE hCmdMbox, TI_UINT8* pParamBuf)
{
    TCmdMbox *pCmdMbox = (TCmdMbox *)hCmdMbox;
    Command_t  *pCmd = (Command_t*)&pCmdMbox->aCmdTxn[1].tCmdMbox;

    /* 
     * Copy the results to the caller buffer:
     * We need to copy only the data without the cmdMbox header, 
     * otherwise we will overflow the pParambuf 
     */
    os_memoryCopy (pCmdMbox->hOs,
                   (void *)pParamBuf,
                   (void *)pCmd->parameters,
                   pCmdMbox->uReadLen - CMDMBOX_HEADER_LEN);

}


#ifdef TI_DBG

void cmdMbox_PrintInfo(TI_HANDLE hCmdMbox)
{
#ifdef REPORT_LOG
    TCmdMbox *pCmdMbox = (TCmdMbox *)hCmdMbox;

    WLAN_OS_REPORT(("Print cmdMbox module info\n"));
    WLAN_OS_REPORT(("=========================\n"));
    WLAN_OS_REPORT(("bCmdInProgress = %d\n", pCmdMbox->bCmdInProgress));
#endif
}

#endif  /* TI_DBG */


