/*
 * TWDriverTx.c
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
 *   MODULE:    TNETW_Driver_Tx.c
 *
 *   PURPOSE:   TNETW_Driver Tx API functions needed externally to the driver.
 *
 ****************************************************************************/

#define __FILE_ID__  FILE_ID_120
#include "report.h"
#include "TWDriver.h"
#include "txCtrlBlk_api.h"
#include "txHwQueue_api.h"
#include "txXfer_api.h"
#include "txResult_api.h"
#include "CmdBld.h"

/** \file  TWDriverTx.c 
 *  \brief TI WLAN HW TX Access Driver
 *
 *  \see   TWDriver.h 
 */


/****************************************************************************
 *                  Tx Control Block API functions                          *
 ****************************************************************************/

TTxCtrlBlk *TWD_txCtrlBlk_Alloc (TI_HANDLE hTWD)
{
    TTwd *pTWD = (TTwd *)hTWD;

    return txCtrlBlk_Alloc (pTWD->hTxCtrlBlk);
}

void TWD_txCtrlBlk_Free (TI_HANDLE hTWD, TTxCtrlBlk *pCurrentEntry)
{
    TTwd *pTWD = (TTwd *)hTWD;

    txCtrlBlk_Free (pTWD->hTxCtrlBlk, pCurrentEntry);
}

TTxCtrlBlk *TWD_txCtrlBlk_GetPointer (TI_HANDLE hTWD, TI_UINT8 descId)
{
    TTwd *pTWD = (TTwd *)hTWD;

    return txCtrlBlk_GetPointer (pTWD->hTxCtrlBlk, descId);
}



/****************************************************************************
 *                      Tx HW Queue API functions                           *
 ****************************************************************************/
ETxHwQueStatus TWD_txHwQueue_AllocResources (TI_HANDLE hTWD, TTxCtrlBlk *pTxCtrlBlk)
{
    TTwd *pTWD = (TTwd *)hTWD;

    return txHwQueue_AllocResources (pTWD->hTxHwQueue, pTxCtrlBlk);
}

/****************************************************************************
 *                          Tx Xfer API functions                           *
 ****************************************************************************/

ETxnStatus TWD_txXfer_SendPacket (TI_HANDLE hTWD, TTxCtrlBlk *pPktCtrlBlk)
{
    TTwd *pTWD = (TTwd *)hTWD;

    return txXfer_SendPacket (pTWD->hTxXfer, pPktCtrlBlk);
}

void TWD_txXfer_EndOfBurst (TI_HANDLE hTWD)
{
    TTwd *pTWD = (TTwd *)hTWD;

    txXfer_EndOfBurst (pTWD->hTxXfer);
}


