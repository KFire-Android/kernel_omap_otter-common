/*
 * TwIfDebug.c
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

 
/** \file   TwIfDebug.c 
 *  \brief  TwIf debug commands 
 *
 * TwIf debug commands 
 * 
 *  \see    TwIf.c, TwIf.h
 */

#include "tidef.h"
#include "report.h"
#include "TwIf.h"
#include "TwIfDebug.h"
#include "TWDriverInternal.h"


static void printTwIfDbgFunctions (void);


/** 
 * \fn     twifDebugFunction
 * \brief  Main TwIf debug function
 * 
 * Main TwIf debug function
 * 
 * \param  hTwIf     - handle to the TWIF object
 * \param  uFuncType - the specific debug function
 * \param  pParam    - parameters for the debug function
 * \return None
 */
void twifDebugFunction (TI_HANDLE hTWD, TI_UINT32 uFuncType, void *pParam)
{
    TTwd      *pTWD  = (TTwd *)hTWD;
    TI_HANDLE  hTwIf = pTWD->hTwIf;

    switch (uFuncType)
    {
    case DBG_TWIF_PRINT_HELP:
        printTwIfDbgFunctions();
        break;

    case DBG_TWIF_PRINT_INFO:
        twIf_PrintModuleInfo (hTwIf);
        break;

	default:
   		WLAN_OS_REPORT(("Invalid function type in TWIF debug function: %d\n", uFuncType));
        break;
    }
}


/** 
 * \fn     printTwIfDbgFunctions
 * \brief  Print the TwIf debug menu
 * 
 * Print the TwIf debug menu
 * 
 * \return None
 */
static void printTwIfDbgFunctions (void)
{
    WLAN_OS_REPORT(("   TwIf Debug Functions   \n"));
	WLAN_OS_REPORT(("--------------------------\n"));
	WLAN_OS_REPORT(("2300 - Print the TwIf Debug Help\n"));
	WLAN_OS_REPORT(("2301 - Print the TwIf Information\n"));
}







