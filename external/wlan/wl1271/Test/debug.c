/*
 * debug.c
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

/** \file reportReplvl.c
 *  \brief Report level implementation
 *
 *  \see reportReplvl.h
 */

/***************************************************************************/
/*                                                                         */
/*      MODULE: reportReplvl.c                                             */
/*    PURPOSE:  Report level implementation                                */
/*                                                                         */
/***************************************************************************/
#include "tidef.h"
#include "debug.h"
#include "connDebug.h"
#include "siteMgrDebug.h"
#include "dataCtrlDbg.h"
#include "rsnDbg.h"
#include "osApi.h"
#include "report.h"
#include "context.h"
#include "timer.h"
#include "qosMngrDbg.h"
#include "PowerMgrDebug.h"
#include "roamingMgrDebug.h"
#include "scanCncnDbg.h"
#include "ScanMngrDbg.h"
#include "scrDbg.h"
#include "SoftGeminiDbg.h"
#include "HealthMonitorDbg.h"
#include "smeDebug.h"
#include "DrvMainModules.h"
#include "TWDriver.h"
#include "fwdriverdebug.h"
#include "MibDbg.h"
#include "TwIfDebug.h"
#include "tracebuf_api.h"

/* Following are the modules numbers */
typedef enum
{
	GENERAL_DEBUG				= 0,
	TEST_ASSOC_MODULE_PARAM		        = 1,
	TEST_UTILS_MODULE_PARAM	                = 2,
	TEST_RX_TX_DATA_MODULE_PARAM            = 3,
	TEST_CTRL_DATA_MODULE_PARAM	        = 4,
	TEST_SITE_MGR_MODULE_PARAM	        = 5,
	TEST_CONN_MODULE_PARAM		        = 6,
	TEST_RSN_MODULE_PARAM	         	= 7,
	TEST_TWD_MODULE_PARAM	                = 8,
	TEST_QOS_MNGR_MODULE_PARAM              = 10,
    TEST_MEASUREMENT_MODULE_PARAM           = 11,
    TEST_POWER_MGR_MODULE_PARAM             = 12,
	TEST_HAL_CTRL_BUFFER_PARAM	        = 13,
    TEST_SCAN_CNCN_MODULE_PARAM             = 14,
    TEST_SCAN_MNGR_MODULE_PARAM             = 15,
	TEST_ROAMING_MNGR_PARAM		        = 16,
    TEST_SCR_PARAM                          = 17,
    TEST_SG_PARAM				= 18,
	TEST_SME_PARAM				= 19,
	TEST_HEALTH_MONITOR_PARAM	        = 20,
    TEST_MIB_DEBUG_PARAM	            = 21,
    TEST_FW_DEBUG_PARAM	                = 22,
    TEST_TWIF_DEBUG_PARAM	            = 23,
	/*
    last module - DO NOT TOUCH!
    */
    NUMBER_OF_TEST_MODULES

}	testModuleParam_e;

#define MAX_PARAM_TYPE  (NUMBER_OF_TEST_MODULES - 1)

/* Utils debug functions */
#define DBG_UTILS_PRINT_HELP		    	 0
#define DBG_UTILS_PRINT_CONTEXT_INFO         1
#define DBG_UTILS_PRINT_TIMER_MODULE_INFO    2
#define DBG_UTILS_PRINT_TRACE_BUFFER         3
/* General Parameters Structure */
typedef struct 
{
	TI_UINT32	paramType;
	TI_UINT32	value;
} testParam_t;

extern void measurementDebugFunction(TI_HANDLE hMeasurementMgr, TI_HANDLE hSwitchChannel, TI_HANDLE hRegulatoryDomain, TI_UINT32 funcType, void *pParam);
static void printMenue(void);
static void utilsDebugFunction (TStadHandlesList *pStadHandles, TI_UINT32 funcType, void *pParam);
static void printUtilsDbgFunctions (void);


/******************************************************************
*                       FUNCTIONS  IMPLEMENTATION				  *
*******************************************************************/

/** 
 * \fn     debugFunction
 * \brief  The debug functions dispatcher
 * 
 * Decode from the debug functionNumber the relevant module and call its debug
 *   function with the provided parameters.
 * The functionNumber parameter is composed as follows:
 *   Module Number      = functionNumber / 100  
 *   Specific Functionc = functionNumber % 100  
 * 
 * \note   
 * \param  pStadHandles   - Pointer to the STAD modules handles                           
 * \param  functionNumber - The module and function numbers composed in a decimal number                           
 * \param  pParam         - The function parameters (optional).                           
 * \return 
 * \sa     
 */ 
TI_STATUS debugFunction(TStadHandlesList *pStadHandles, TI_UINT32 functionNumber, void *pParam)
{
    TI_UINT32  moduleNumber;

    moduleNumber = functionNumber / 100;

    if  (moduleNumber > MAX_PARAM_TYPE)
        return PARAM_MODULE_NUMBER_INVALID;

    switch (moduleNumber)
    {
    case GENERAL_DEBUG:
        printMenue();
        break;

    case TEST_ASSOC_MODULE_PARAM:
        break;

    case TEST_UTILS_MODULE_PARAM:
        utilsDebugFunction (pStadHandles, functionNumber % 100, pParam);
		break;

    case TEST_RX_TX_DATA_MODULE_PARAM:
        if( functionNumber < 350)
            rxTxDebugFunction(pStadHandles->hTxCtrl, functionNumber % 100, pParam);
        else
            rxTxDebugFunction(pStadHandles->hRxData, functionNumber % 100, pParam);
        break;

    case TEST_CTRL_DATA_MODULE_PARAM:
        ctrlDebugFunction(pStadHandles->hCtrlData, functionNumber % 100, pParam);
        break;

    case TEST_SITE_MGR_MODULE_PARAM:
        siteMgrDebugFunction(pStadHandles->hSiteMgr, pStadHandles, functionNumber % 100, pParam);
        break;

    case TEST_CONN_MODULE_PARAM:
        connDebugFunction(pStadHandles->hConn, functionNumber % 100, pParam);
        break;

    case TEST_RSN_MODULE_PARAM:
        rsnDebugFunction(pStadHandles->hRsn, functionNumber % 100, pParam);
        break;

    case TEST_TWD_MODULE_PARAM:
        TWD_Debug (pStadHandles->hTWD, functionNumber % 100, pParam);
        break;

    case TEST_QOS_MNGR_MODULE_PARAM:
        qosMngrDebugFunction(pStadHandles->hQosMngr, functionNumber % 100, pParam);
        break;

    case TEST_MEASUREMENT_MODULE_PARAM:
        measurementDebugFunction (pStadHandles->hMeasurementMgr, 
                                  pStadHandles->hSwitchChannel, 
                                  pStadHandles->hRegulatoryDomain, 
                                  functionNumber % 100, 
                                  pParam);
        break;

    case TEST_POWER_MGR_MODULE_PARAM:
        powerMgrDebugFunction(pStadHandles->hPowerMgr,
                              functionNumber % 100,
                              pParam);
        break;

    case TEST_SCAN_CNCN_MODULE_PARAM:
        scanConcentratorDebugFunction( pStadHandles->hScanCncn, pStadHandles->hTWD ,functionNumber % 100, pParam );
        break;

    case TEST_SCAN_MNGR_MODULE_PARAM:
        scanMngrDebugFunction( pStadHandles->hScanMngr, functionNumber  % 100, pParam, pStadHandles->hSiteMgr, pStadHandles->hCtrlData );
        break;

    case TEST_ROAMING_MNGR_PARAM:
        roamingMgrDebugFunction(pStadHandles->hRoamingMngr, functionNumber % 100, pParam);
        break;

    case TEST_SCR_PARAM:
        scrDebugFunction( pStadHandles->hSCR, functionNumber % 100, pParam );
        break;
    
    case TEST_SG_PARAM:
        SoftGeminiDebugFunction( pStadHandles->hSoftGemini, functionNumber % 100, pParam );
        break;

    case TEST_SME_PARAM:
        smeDebugFunction( pStadHandles->hSme, functionNumber % 100, pParam );
        break;

    case TEST_HEALTH_MONITOR_PARAM:
        healthMonitorDebugFunction (pStadHandles, functionNumber % 100, pParam);
        break;

    case TEST_MIB_DEBUG_PARAM:
         MibDebugFunction(pStadHandles->hTWD, functionNumber % 100, pParam);
        break;

    case TEST_FW_DEBUG_PARAM:
         FWDebugFunction(pStadHandles->hDrvMain, 
						 pStadHandles->hOs, 
						 pStadHandles->hTWD, 
						 pStadHandles->hMlmeSm, 
						 pStadHandles->hTxMgmtQ,
						 pStadHandles->hTxCtrl,
						 functionNumber % 100, 
						 pParam/*yael , packetNum*/);
        break;

    case TEST_TWIF_DEBUG_PARAM:
         twifDebugFunction (pStadHandles->hTWD, functionNumber % 100, pParam);
        break;

    default:
        WLAN_OS_REPORT(("Invalid debug function module number: %d\n\n", moduleNumber)); 
        break;
    }

    return TI_OK;
}

static void printMenue(void)
{
    WLAN_OS_REPORT(("   Debug main menu (p <num>)\n"));
    WLAN_OS_REPORT(("-----------------------------\n"));

    WLAN_OS_REPORT(("Association             100\n")); 
    WLAN_OS_REPORT(("Utils                   200\n"));
    WLAN_OS_REPORT(("Tx                      300\n"));
    WLAN_OS_REPORT(("Rx                      350\n"));
    WLAN_OS_REPORT(("Ctrl                    400\n"));
    WLAN_OS_REPORT(("SiteMgr                 500\n"));
    WLAN_OS_REPORT(("Connection              600\n"));
    WLAN_OS_REPORT(("Rsn                     700\n"));
    WLAN_OS_REPORT(("Hal Ctrl                800\n"));
    WLAN_OS_REPORT(("QOS                    1000\n"));
    WLAN_OS_REPORT(("Measurement            1100\n"));
    WLAN_OS_REPORT(("PowerMgr               1200\n"));
    WLAN_OS_REPORT(("HAL Ctrl Buffer        1300\n"));
    WLAN_OS_REPORT(("Scan concentrator      1400\n"));
    WLAN_OS_REPORT(("Scan Manager           1500\n"));
    WLAN_OS_REPORT(("Roaming Manager        1600\n"));
    WLAN_OS_REPORT(("SCR                    1700\n"));
    WLAN_OS_REPORT(("Soft Gemini            1800\n"));
    WLAN_OS_REPORT(("SME                    1900\n"));
    WLAN_OS_REPORT(("Health Monitor         2000\n"));
    WLAN_OS_REPORT(("MIB                    2100\n"));
    WLAN_OS_REPORT(("FW Debug               2200\n"));
    WLAN_OS_REPORT(("TwIf                   2300\n"));
}


/**
 * \fn    utilsDebugFunction
 * \brief Utils debug function
 *
 * \param pDrvMain - handle to the DrvMain object.\n
 * \param funcType - the specific debug function.\n
 * \param pParam - parameters for the debug function.\n
 */
static void utilsDebugFunction (TStadHandlesList *pStadHandles, TI_UINT32 funcType, void *pParam)
{
    switch (funcType)
    {
        case DBG_UTILS_PRINT_HELP:
            printUtilsDbgFunctions ();
            break;
    
        case DBG_UTILS_PRINT_CONTEXT_INFO:
            context_Print (pStadHandles->hContext);
            break;
    
        case DBG_UTILS_PRINT_TIMER_MODULE_INFO:
            tmr_PrintModule (pStadHandles->hTimer);
            break;
    
        case DBG_UTILS_PRINT_TRACE_BUFFER:
/*          tb_printf(); */
            break;
    
        default:
       		WLAN_OS_REPORT(("utilsDebugFunction(): Invalid function type: %d\n", funcType));
            break;
    }
}

/**
 * \fn    printUtilsDbgFunctions
 * \brief Prints Utils debug menu
 *
 */
static void printUtilsDbgFunctions (void)
{
    WLAN_OS_REPORT(("   Utils Debug Functions   \n"));
	WLAN_OS_REPORT(("---------------------------\n"));
	WLAN_OS_REPORT(("200 - Print the Utils Debug Help\n"));
	WLAN_OS_REPORT(("201 - Print Context module info\n"));
	WLAN_OS_REPORT(("202 - Print Timer module info\n"));
	WLAN_OS_REPORT(("203 - Print the trace buffer\n"));
}

