/*
 * rsnDbg.c
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


/****************************************************************************/
/*																			*/
/*		MODULE:																*/
/*    PURPOSE:																*/
/*												   							*/ 						
/****************************************************************************/
/* #include "osTITypes.h" */
#include "osApi.h"
#include "rsnApi.h"
#include "rsn.h"
#include "report.h"
#include "paramOut.h"
#include "rsnDbg.h"
#ifdef XCC_MODULE_INCLUDED
#include "XCCMngr.h"
#endif

void printRsnDbgFunctions(void);
void printRogueApTable(TI_HANDLE hRogueAp);

static TI_UINT8 infoBuf[480];

/*************************************************************************
 *																		 *
 *************************************************************************
DESCRIPTION:                  
                                                      
INPUT:       

OUTPUT:      

RETURN:     
                                                   
************************************************************************/
void rsnDebugFunction(TI_HANDLE hRsn, TI_UINT32 funcType, void *pParam)
{
	paramInfo_t 	param, *pRsnParam;
	TI_UINT32			value;
    rsnAuthEncrCapability_t rsnAuthEncrCap;

	switch (funcType)
	{
	case DBG_RSN_PRINT_HELP:
		printRsnDbgFunctions();
		break;

	case DBG_RSN_SET_DESIRED_AUTH:
		WLAN_OS_REPORT(("RSN DBG - Set desired Authentication suite \n"));
		value = *(TI_UINT32*)pParam;

		param.paramType = RSN_EXT_AUTHENTICATION_MODE;
		param.content.rsnDesiredAuthType = (EAuthSuite)value;

		rsn_setParam(hRsn, &param);
		break;

	case DBG_RSN_SET_DESIRED_CIPHER:
		WLAN_OS_REPORT(("RSN DBG - Set desired cipher suite \n"));
		value = *(TI_UINT32*)pParam;

		param.paramType = RSN_ENCRYPTION_STATUS_PARAM;
		param.content.rsnEncryptionStatus = (ECipherSuite)value;

		rsn_setParam(hRsn, &param);
		break;
	

	case DBG_RSN_GEN_MIC_FAILURE_REPORT:
		value = *(TI_UINT32*)pParam;
		/* generate unicast mic failure report to the OS and to the RSN module */
		rsn_reportMicFailure(hRsn, (TI_UINT8*)&value,1);
		break;

	case DBG_RSN_GET_PARAM_802_11_CAPABILITY:
			
		param.paramType = RSN_AUTH_ENCR_CAPABILITY;
        param.content.pRsnAuthEncrCapability = &rsnAuthEncrCap;
	
        /* Get 802_11 capability info */
		rsn_getParam(hRsn, &param);
		break;
		
	case DBG_RSN_GET_PMKID_CACHE:
		
		pRsnParam = (paramInfo_t *)&infoBuf;
		pRsnParam->paramType = RSN_PMKID_LIST;
		pRsnParam->paramLength = 480;

		/* Get PMKID list */
		rsn_getParam(hRsn, pRsnParam);
		break;

	case DBG_RSN_RESET_PMKID_CACHE:
		
		rsn_resetPMKIDList(hRsn);

		break;
#ifdef XCC_MODULE_INCLUDED
    case DBG_RSN_PRINT_ROGUE_AP_TABLE:
        printRogueApTable(((XCCMngr_t*)((rsn_t*)hRsn)->hXCCMngr)->hRogueAp);
        break;
#endif

    case DBG_RSN_SET_PORT_STATUS:
        WLAN_OS_REPORT(("Setting PORT STATUS to open\n"));
        rsn_setPortStatus(hRsn,TI_TRUE);
        break;

    case DBG_RSN_PRINT_PORT_STATUS:
        {
            TI_BOOL portStatus = TI_FALSE;
            portStatus = rsn_getPortStatus(((rsn_t*)hRsn));
            WLAN_OS_REPORT(("\n\nPORT is %s !!\n",(portStatus)?"OPEN":"CLOSE"));
        }

        break;
	default:
		WLAN_OS_REPORT(("Invalid function type in RSN Function Command: %d\n", funcType));
		break;
	}

} 


void printRsnDbgFunctions(void)
{
	WLAN_OS_REPORT(("   Rsn Debug Functions   \n"));
	WLAN_OS_REPORT(("-------------------------\n"));

	WLAN_OS_REPORT(("702 - Set default key id \n"));
	WLAN_OS_REPORT(("703 - Set desired Authentication suite \n"));
	WLAN_OS_REPORT(("704 - Set desired cipher suite \n"));

	WLAN_OS_REPORT(("706 - Generate MIC FAILURE report (after 2 clicks less then 1 minute - disassociate)\n"));
	WLAN_OS_REPORT(("707 - Get 802.11 authentication/encryption capability\n"));
	WLAN_OS_REPORT(("708 - Get PMKID cache \n"));
	WLAN_OS_REPORT(("709 - ReSet PMKID cache  \n"));
	WLAN_OS_REPORT(("710 - Print Rogue AP table\n"));


}
