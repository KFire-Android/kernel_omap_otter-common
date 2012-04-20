/*
 * TxDbg.c
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


/***************************************************************************/
/*																									*/
/*		MODULE:																	*/
/*    PURPOSE:										*/
/*																									*/
/***************************************************************************/
#include "tidef.h"
#include "DataCtrl_Api.h"
#include "dataCtrlDbg.h"
#include "osApi.h"
#include "report.h"
#include "siteMgrApi.h"
#include "TWDriver.h"
#include "txCtrl.h"

void printTxRxDbgFunctions(void);



/*************************************************************************
 *							rxTxDebugFunction							 *
 *************************************************************************
DESCRIPTION:   Call the requested Tx or Rx debug print function.               
************************************************************************/

void rxTxDebugFunction(TI_HANDLE hRxTxHandle, TI_UINT32 funcType, void *pParam)
{
	txCtrl_t *pTxCtrl = (txCtrl_t *)hRxTxHandle;  /* Relevant only for some of the cases below! */

	switch ((ERxTxDbgFunc)funcType)
	{
	case TX_RX_DBG_FUNCTIONS:
		printTxRxDbgFunctions();
		break;

	/* 
	 *  TX DEBUG FUNCTIONS:
	 *  ===================
	 */
	case PRINT_TX_CTRL_INFO:
		txCtrlParams_printInfo (hRxTxHandle);
		break;

    case PRINT_TX_CTRL_COUNTERS:
        txCtrlParams_printDebugCounters (hRxTxHandle);
        break;

	case PRINT_TX_DATA_QUEUE_INFO:
		txDataQ_PrintModuleParams (pTxCtrl->hTxDataQ);
		break;

	case PRINT_TX_DATA_QUEUE_COUNTERS:
		txDataQ_PrintQueueStatistics (pTxCtrl->hTxDataQ);
		break;

	case PRINT_TX_MGMT_QUEUE_INFO:
		txMgmtQ_PrintModuleParams (pTxCtrl->hTxMgmtQ);
		break;

	case PRINT_TX_MGMT_QUEUE_COUNTERS:
		txMgmtQ_PrintQueueStatistics (pTxCtrl->hTxMgmtQ);
		break;

    case PRINT_TX_CTRL_BLK_INFO:
		TWD_PrintTxInfo (pTxCtrl->hTWD, TWD_PRINT_TX_CTRL_BLK_TBL);
        break;

    case PRINT_TX_HW_QUEUE_INFO:
		TWD_PrintTxInfo (pTxCtrl->hTWD, TWD_PRINT_TX_HW_QUEUE_INFO);
        break;

    case PRINT_TX_XFER_INFO:
		TWD_PrintTxInfo (pTxCtrl->hTWD, TWD_PRINT_TX_XFER_INFO);
        break;

	case PRINT_TX_RESULT_INFO:
		TWD_PrintTxInfo (pTxCtrl->hTWD, TWD_PRINT_TX_RESULT_INFO);
        break;

	case PRINT_TX_DATA_CLSFR_TABLE:
        txDataClsfr_PrintClsfrTable (pTxCtrl->hTxDataQ);
        break;


	case RESET_TX_CTRL_COUNTERS:
		txCtrlParams_resetDbgCounters (hRxTxHandle);
		break;

	case RESET_TX_DATA_QUEUE_COUNTERS:
		txDataQ_ResetQueueStatistics (pTxCtrl->hTxDataQ);
		break;

    case RESET_TX_DATA_CLSFR_TABLE:
        {
            EClsfrType  myLocalType;

            /* Setting again the current classifier type clears the table */
            txDataClsfr_GetClsfrType (pTxCtrl->hTxDataQ, &myLocalType);
            txDataClsfr_SetClsfrType (pTxCtrl->hTxDataQ, myLocalType);
        }
		break;

	case RESET_TX_MGMT_QUEUE_COUNTERS:
		txMgmtQ_ResetQueueStatistics (pTxCtrl->hTxMgmtQ);
		break;

	case RESET_TX_RESULT_COUNTERS:
		TWD_PrintTxInfo (pTxCtrl->hTWD, TWD_CLEAR_TX_RESULT_INFO);
		break;
        
	case RESET_TX_XFER_COUNTERS:
		TWD_PrintTxInfo (pTxCtrl->hTWD, TWD_CLEAR_TX_XFER_INFO);
		break;
        

	/* 
	 *  RX DEBUG FUNCTIONS:
	 *  ===================
	 */
	case PRINT_RX_BLOCK:
		WLAN_OS_REPORT(("RX DBG - Print Rx Block \n\n"));
		rxData_printRxBlock(hRxTxHandle);
		break;

	case PRINT_RX_COUNTERS:
		WLAN_OS_REPORT(("RX DBG - Print Rx counters \n\n"));
		rxData_printRxCounters(hRxTxHandle);
		break;

	case RESET_RX_COUNTERS:
		WLAN_OS_REPORT(("RX DBG - Reset Rx counters \n\n"));
		rxData_resetCounters(hRxTxHandle);
		rxData_resetDbgCounters(hRxTxHandle);
		break;

    case PRINT_RX_THROUGHPUT_START:
		rxData_startRxThroughputTimer (hRxTxHandle);
		break;

    case PRINT_RX_THROUGHPUT_STOP:
		rxData_stopRxThroughputTimer (hRxTxHandle);
		break;

	default:
		WLAN_OS_REPORT(("Invalid function type in Debug Tx Function Command: %d\n\n", funcType));
		break;
	}
}


void printTxRxDbgFunctions(void)
{
	WLAN_OS_REPORT(("\n          Tx Dbg Functions   \n"));
	WLAN_OS_REPORT(("--------------------------------------\n"));

	WLAN_OS_REPORT(("301 - Print TxCtrl info.\n"));
	WLAN_OS_REPORT(("302 - Print TxCtrl Statistics.\n"));
	WLAN_OS_REPORT(("303 - Print TxDataQueue info.\n"));
	WLAN_OS_REPORT(("304 - Print TxDataQueue Statistics.\n"));
	WLAN_OS_REPORT(("305 - Print TxMgmtQueue info.\n"));
	WLAN_OS_REPORT(("306 - Print TxMgmtQueue Statistics.\n"));
	WLAN_OS_REPORT(("307 - Print TxCtrlBlk table.\n"));
	WLAN_OS_REPORT(("308 - Print TxHwQueue info.\n"));
	WLAN_OS_REPORT(("309 - Print TxXfer info.\n"));
	WLAN_OS_REPORT(("310 - Print TxResult info.\n"));
	WLAN_OS_REPORT(("311 - Print TxDataClsfr Classifier Table.\n"));

	WLAN_OS_REPORT(("320 - Reset TxCtrl Statistics.\n"));
	WLAN_OS_REPORT(("321 - Reset TxDataQueue Statistics.\n"));
	WLAN_OS_REPORT(("322 - Reset TxDataClsfr Classifier Table.\n"));
	WLAN_OS_REPORT(("323 - Reset TxMgmtQueue Statistics.\n"));
	WLAN_OS_REPORT(("324 - Reset TxResult Statistics.\n"));
	WLAN_OS_REPORT(("325 - Reset TxXfer Statistics.\n"));

	WLAN_OS_REPORT(("\n          Rx Dbg Functions   \n"));
	WLAN_OS_REPORT(("--------------------------------------\n"));
	WLAN_OS_REPORT(("350 - Print Rx block.\n"));
	WLAN_OS_REPORT(("351 - Print Rx counters.\n"));
	WLAN_OS_REPORT(("352 - Reset Rx counters.\n"));
	WLAN_OS_REPORT(("353 - Start Rx throughput timer.\n"));
	WLAN_OS_REPORT(("354 - Stop  Rx throughput timer.\n"));
}


