/*
 * ctrlDbg.c
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
/*									*/
/***************************************************************************/
#include "tidef.h"
#include "Ctrl.h"
#include "DataCtrl_Api.h"
#include "dataCtrlDbg.h"
#include "osApi.h"
#include "report.h"
#include "TWDriver.h"
#include "paramOut.h"

void printCtrlDbgFunctions(void);


/*************************************************************************
 *																		 *
 *************************************************************************
DESCRIPTION:                  
                                                      
INPUT:       

OUTPUT:      

RETURN:     
                                                   
************************************************************************/
void ctrlDebugFunction(TI_HANDLE hCtrlData, TI_UINT32 funcType, void *pParam)
{
	ctrlData_t *pCtrlData = (ctrlData_t *)hCtrlData;
	paramInfo_t paramInfo;

	switch ((ECtrlDbgFunc)funcType)
	{
	case CTRL_PRINT_DBG_FUNCTIONS:
		printCtrlDbgFunctions();
		break;

	case CTRL_PRINT_CTRL_BLOCK:
		WLAN_OS_REPORT(("CTRL DBG - Print Ctrl Block \n\n"));
		ctrlData_printCtrlBlock(pCtrlData);
		break;

	case CTRL_PRINT_TX_PARAMETERS:
		WLAN_OS_REPORT(("CTRL DBG - Print tx paramters \n\n"));
		ctrlData_printTxParameters(pCtrlData);
		break;

	case CTRL_SET_CTS_TO_SELF:
		WLAN_OS_REPORT(("CTRL DBG - Set CtsToSelf = %s\n",((*(TI_UINT8*)pParam > 0) ? "TI_TRUE" : "TI_FALSE")));
		paramInfo.paramType = CTRL_DATA_CURRENT_PROTECTION_STATUS_PARAM;
		paramInfo.content.ctrlDataProtectionEnabled = ((*(TI_UINT8*)pParam > 0) ? 1 : 0);
		ctrlData_setParam(pCtrlData,&paramInfo);
		break;

	default:
		WLAN_OS_REPORT(("Invalid function type in Debug Ctrl Function Command: %d\n\n", funcType));
		break;
	}
} 


void printCtrlDbgFunctions(void)
{
	WLAN_OS_REPORT(("   Ctrl Dbg Functions   \n"));
	WLAN_OS_REPORT(("------------------------\n"));
	WLAN_OS_REPORT(("400 - Print this menu.\n"));
	WLAN_OS_REPORT(("401 - Print Ctrl Block.\n"));
	WLAN_OS_REPORT(("402 - Print Tx parameters.\n"));
  	WLAN_OS_REPORT(("403 - Set CtsToSelf.\n"));
}



