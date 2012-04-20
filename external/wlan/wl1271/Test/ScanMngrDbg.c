/*
 * ScanMngrDbg.c
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

/** \file ScanMngrDbg.c
 *  \brief This file include the scan manager debug module implementation
 *  \
 *  \date 29-March-2005
 */

#include "tidef.h"
#include "report.h"
#include "paramOut.h"
#include "scanMngr.h"
#include "ScanMngrDbg.h"
#include "siteMgrApi.h"
#include "DataCtrl_Api.h"


/**
 * \\n
 * \date 29-March-2005\n
 * \brief Main scan manager debug function
 *
 * Function Scope \e Public.\n
 * \param hScanMngr - handle to the scan manager object.\n
 * \param funcType - the specific debug function.\n
 * \param pParam - parameters for the debug function.\n
 * \param hSiteMgr - handle to th esite manager object.\n
 * \param hCtrlData - handle to the data ctrl object.\n
 */
void scanMngrDebugFunction( TI_HANDLE hScanMngr, TI_UINT32 funcType, void *pParam, TI_HANDLE hSiteMgr, TI_HANDLE hCtrlData )
{
  	switch (funcType)
	{
	case DBG_SCAN_MNGR_PRINT_HELP:
		printScanMngrDbgFunctions();
		break;

	case DBG_SCAN_MNGR_START_CONT_SCAN:  
		startContScan( hScanMngr, hSiteMgr, hCtrlData );
		break;
		
	case DBG_SCAN_MNGR_STOP_CONT_SCAN:  
		scanMngr_stopContScan( hScanMngr );
		break;

    case DBG_SCAN_MNGR_START_IMMED_SCAN:
        scanMngr_startImmediateScan( hScanMngr, (1 == *((TI_INT32*)pParam) ? TI_TRUE : TI_FALSE) );
        break;

    case DBG_SCAN_MNGR_STOP_IMMED_SCAN:
        scanMngr_stopImmediateScan( hScanMngr );
        break;

    case DBG_SCAN_MNGR_PRINT_TRACK_LIST:
        scanMngrDebugPrintBSSList( hScanMngr );
        break;
        
    case DBG_SCAN_MNGR_PRINT_STATS:
        scanMngr_statsPrint( hScanMngr );
        break;

    case DBG_SCAN_MNGR_RESET_STATS:
        scanMngr_statsReset( hScanMngr );
        break;

    case DBG_SCAN_MNGR_PIRNT_NEIGHBOR_APS:
        scanMngrDebugPrintNeighborAPList( hScanMngr );
        break;

    case DBG_SCAN_MNGR_PRINT_POLICY:
        scanMngrTracePrintScanPolicy( &(((scanMngr_t*)hScanMngr)->scanPolicy) );
        break;

    case DBG_SCAN_MNGR_PRINT_OBJECT:
        scanMngrDebugPrintObject( hScanMngr );
        break;

    default:
		WLAN_OS_REPORT(("Invalid function type in scan manager debug function: %d\n", funcType));
		break;
	}
}

/**
 * \\n
 * \date 29-March-2005\n
 * \brief Prints scan manager debug menu
 *
 * Function Scope \e Public.\n
 */
void printScanMngrDbgFunctions(void)
{
    WLAN_OS_REPORT(("   Scan Manager Debug Functions   \n"));
	WLAN_OS_REPORT(("---------------------------------------\n"));
	WLAN_OS_REPORT(("1500 - Print the scan manager Debug Help\n"));
	WLAN_OS_REPORT(("1501 - Start continuous scan\n"));
	WLAN_OS_REPORT(("1502 - Stop continuous scan\n"));
    WLAN_OS_REPORT(("1503 - Start immediate scan\n"));
    WLAN_OS_REPORT(("1504 - Stop immediate scan\n"));
    WLAN_OS_REPORT(("1505 - Print tracking list\n"));
    WLAN_OS_REPORT(("1506 - Print statistics\n"));
    WLAN_OS_REPORT(("1507 - Reset statistics\n"));
    WLAN_OS_REPORT(("1508 - Print neighbor APs list\n"));
    WLAN_OS_REPORT(("1509 - Print Scan Policy\n"));
    WLAN_OS_REPORT(("1510 - Print scan manager object\n"));
}

/**
 * \\n
 * \date 29-March-2005\n
 * \brief Starts continuous scan process.\n
 *
 * Function Scope \e Public.\n
 * \param hScanMngr - handle to the scan manager object.\n
 * \param hSiteMgr - handle to the site manager object.\n\
 * \param hCtrlData - handle to the data ctrl object.\n
 */
void startContScan( TI_HANDLE hScanMngr, TI_HANDLE hSiteMgr, TI_HANDLE hCtrlData )
{
    paramInfo_t param;
    ERadioBand  radioBand;

    /* get current band */
    param.paramType = SITE_MGR_RADIO_BAND_PARAM;
    siteMgr_getParam( hSiteMgr, &param );
    radioBand = param.content.siteMgrRadioBand;

    /* get current BSSID */
	param.paramType = CTRL_DATA_CURRENT_BSSID_PARAM;
	ctrlData_getParam( hCtrlData, &param );
    
    /* start continuous scan */
    scanMngr_startContScan( hScanMngr, &(param.content.ctrlDataCurrentBSSID), radioBand );
}

