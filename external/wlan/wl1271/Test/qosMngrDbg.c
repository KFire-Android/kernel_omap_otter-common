/*
 * qosMngrDbg.c
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
/*																			*/
/***************************************************************************/
#include "tidef.h"
#include "osApi.h"
#include "qosMngr.h"
#include "report.h"
#include "paramOut.h"
#include "qosMngrDbg.h"

void printQosMngrParams(TI_HANDLE hQosMngr);
void printQosDbgFunction(TI_HANDLE hQosMngr);


void qosMngrDebugFunction(TI_HANDLE hQosMngr, TI_UINT32 funcType, void *pParam)
{
	switch (funcType)
	{
	case DBG_QOS_MNGR_PRINT_HELP:
		printQosDbgFunction(hQosMngr);
		break;

	case DBG_QOS_MNGR_PRINT_PARAMS:
		printQosMngrParams(hQosMngr);
		break;

	default:
		WLAN_OS_REPORT(("\nInvalid function type in QOS MNGR Function Command: %d\n", funcType));
		break;
	}


}
void printQosDbgFunction(TI_HANDLE hQosMngr)
{
	WLAN_OS_REPORT(("\n   qosMngr Debug Functions   \n"));
	WLAN_OS_REPORT(("-----------------------------\n"));
	
	WLAN_OS_REPORT(("1000 - Print the QOS MNGR Debug Help\n"));

	WLAN_OS_REPORT(("1001 - print QOS parameters\n"));

}

void printQosMngrParams(TI_HANDLE hQosMngr)
{
	qosMngr_t *pQosMngr = (qosMngr_t *)hQosMngr;
    EQosProtocol protocol = QOS_NONE;

	WLAN_OS_REPORT(("QOS Parameters :\n"));

	switch(pQosMngr->activeProtocol){
	case QOS_WME:
		WLAN_OS_REPORT(("QOS protocol = QOS_WME\n"));
		protocol = QOS_WME;

		break;
	case QOS_NONE:
		WLAN_OS_REPORT(("QOS protocol = NONE\n"));
		break;

    default:
		break;

	}
	if(protocol != QOS_NONE)
	{
		WLAN_OS_REPORT(("PS POLL Modes:\n"));
		WLAN_OS_REPORT(("PS_POLL:	0\n"));
		WLAN_OS_REPORT(("UPSD	:	1\n"));
		WLAN_OS_REPORT(("Ps None: 2\n"));
	}

	if(pQosMngr->desiredPsMode == PS_SCHEME_UPSD_TRIGGER)
		WLAN_OS_REPORT(("Desired Power Save Mode = UPSD\n"));
	else
		WLAN_OS_REPORT(("Desired Power Save Mode = Legacy\n"));

	if(pQosMngr->currentPsMode == PS_SCHEME_UPSD_TRIGGER)
		WLAN_OS_REPORT(("Current Power Save Mode = UPSD\n"));
	else
		WLAN_OS_REPORT(("Current Power Save Mode = Legacy\n"));

	WLAN_OS_REPORT(("BK Parameters:\n"));
	WLAN_OS_REPORT(("aifsn :%d\n",pQosMngr->acParams[QOS_AC_BK].acQosParams.aifsn));
	WLAN_OS_REPORT(("cwMin :%d\n",pQosMngr->acParams[QOS_AC_BK].acQosParams.cwMin));
	WLAN_OS_REPORT(("cwMax :%d\n",pQosMngr->acParams[QOS_AC_BK].acQosParams.cwMax));
	WLAN_OS_REPORT(("txopLimit :%d\n",pQosMngr->acParams[QOS_AC_BK].acQosParams.txopLimit));
	if(protocol == QOS_WME)
	{
		WLAN_OS_REPORT(("Desired PsMode :%d\n",pQosMngr->acParams[QOS_AC_BK].desiredWmeAcPsMode));
		WLAN_OS_REPORT(("Current PsMode :%d\n",pQosMngr->acParams[QOS_AC_BK].currentWmeAcPsMode));
	}

	WLAN_OS_REPORT(("BE Parameters:\n"));
	WLAN_OS_REPORT(("aifsn :%d\n",pQosMngr->acParams[QOS_AC_BE].acQosParams.aifsn));
	WLAN_OS_REPORT(("cwMin :%d\n",pQosMngr->acParams[QOS_AC_BE].acQosParams.cwMin));
	WLAN_OS_REPORT(("cwMax :%d\n",pQosMngr->acParams[QOS_AC_BE].acQosParams.cwMax));
	WLAN_OS_REPORT(("txopLimit :%d\n",pQosMngr->acParams[QOS_AC_BE].acQosParams.txopLimit));
	if(protocol == QOS_WME)
	{
		WLAN_OS_REPORT(("Desired PsMode :%d\n",pQosMngr->acParams[QOS_AC_BE].desiredWmeAcPsMode));
		WLAN_OS_REPORT(("Current PsMode :%d\n",pQosMngr->acParams[QOS_AC_BE].currentWmeAcPsMode));
	}

	WLAN_OS_REPORT(("VI Parameters:\n"));
	WLAN_OS_REPORT(("aifsn :%d\n",pQosMngr->acParams[QOS_AC_VI].acQosParams.aifsn));
	WLAN_OS_REPORT(("cwMin :%d\n",pQosMngr->acParams[QOS_AC_VI].acQosParams.cwMin));
	WLAN_OS_REPORT(("cwMax :%d\n",pQosMngr->acParams[QOS_AC_VI].acQosParams.cwMax));
	WLAN_OS_REPORT(("txopLimit :%d\n",pQosMngr->acParams[QOS_AC_VI].acQosParams.txopLimit));
	if(protocol == QOS_WME)
	{
		WLAN_OS_REPORT(("Desired PsMode :%d\n",pQosMngr->acParams[QOS_AC_VI].desiredWmeAcPsMode));
		WLAN_OS_REPORT(("Current PsMode :%d\n",pQosMngr->acParams[QOS_AC_VI].currentWmeAcPsMode));
	}

	WLAN_OS_REPORT(("VO Parameters:\n"));
	WLAN_OS_REPORT(("aifsn :%d\n",pQosMngr->acParams[QOS_AC_VO].acQosParams.aifsn));
	WLAN_OS_REPORT(("cwMin :%d\n",pQosMngr->acParams[QOS_AC_VO].acQosParams.cwMin));
	WLAN_OS_REPORT(("cwMax :%d\n",pQosMngr->acParams[QOS_AC_VO].acQosParams.cwMax));
	WLAN_OS_REPORT(("txopLimit :%d\n",pQosMngr->acParams[QOS_AC_VO].acQosParams.txopLimit));
	if(protocol == QOS_WME)
	{
		WLAN_OS_REPORT(("Desired PsMode :%d\n",pQosMngr->acParams[QOS_AC_VO].desiredWmeAcPsMode));
		WLAN_OS_REPORT(("Current PsMode :%d\n",pQosMngr->acParams[QOS_AC_VO].currentWmeAcPsMode));
	}
}

