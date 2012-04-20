/*
 * SoftGeminiDbg.c
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

/** \file ScanCncnDbg.c
 *  \brief This file include the soft gemini debug module implementation
 *  \
 *  \date 14-Dec-2005
 */

#include "SoftGeminiApi.h"
#include "SoftGeminiDbg.h"
#include "report.h"
#include "osApi.h"


/**
 * \\n
 * \date 14-Dec-2005\n
 * \brief Main soft gemini debug function
 *
 * Function Scope \e Public.\n
 * \param hSoftGemini - handle to the soft gemini object.\n
 * \param funcType - the specific debug function.\n
 * \param pParam - parameters for the debug function.\n
 */
void SoftGeminiDebugFunction( TI_HANDLE hSoftGemini,TI_UINT32 funcType, void *pParam )
{
  	switch (funcType)
	{
	case DBG_SG_PRINT_HELP:
		printSoftGeminiDbgFunctions();
		break;

	case DBG_SG_PRINT_PARAMS:  
		printSoftGeminiParams(hSoftGemini);
		break;
		
	case DBG_SG_SENSE_MODE:
		SoftGemini_SenseIndicationCB(hSoftGemini,(char*) pParam,1);
		break;

	case DBG_SG_PROTECTIVE_MODE:
		SoftGemini_ProtectiveIndicationCB(hSoftGemini,(char*) pParam,1);
		break;

	default:
		WLAN_OS_REPORT(("Invalid function type in soft gemini debug function: %d\n", funcType));
		break;
	}
}

/**
 * \\n
 * \date 14-Dec-2005\n
 * \brief Prints soft gemini debug menu
 *
 * Function Scope \e Public.\n
 */
void printSoftGeminiDbgFunctions(void)
{
    WLAN_OS_REPORT(("   Scan Concentrator Debug Functions   \n"));
	WLAN_OS_REPORT(("---------------------------------------\n"));
	WLAN_OS_REPORT(("1800 - Print the soft gemini Debug Help\n"));
	WLAN_OS_REPORT(("1801 - Print soft gemini parameters    \n"));
	WLAN_OS_REPORT(("1802 - Trigger Sense Mode (enable - 1)	\n"));
	WLAN_OS_REPORT(("1803 - Trigger Protective Mode	(ON - 1)\n"));

}

/**
 * \\n
 * \date 14-Dec-2005\n
 * \brief Performs a driver scan from within an app scan.\n
 *
 * Function Scope \e Public.\n
 * \param hSoftGemini - handle to the soft gemini object.\n
 */
void printSoftGeminiParams( TI_HANDLE hSoftGemini )
{
   SoftGemini_printParams(hSoftGemini);
}
