/*
 * scanCncnDbg.c
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

/** \file  ScanCncnDbg.c
 *  \brief This file include the scan concentrator debug module implementation
 *
 *  \see   ScanCncn.h, ScanCncn.c
 */


#include "ScanCncn.h"
#include "scanCncnDbg.h"
#include "report.h"
#include "TWDriver.h"


/**
 * \\n
 * \date 14-Feb-2005\n
 * \brief Main scan concentrator debug function
 *
 * Function Scope \e Public.\n
 * \param hScanCncn - handle to the scan concentrator object.\n
 * \param funcType - the specific debug function.\n
 * \param pParam - parameters for the debug function.\n
 */
void scanConcentratorDebugFunction( TI_HANDLE hScanCncn, TI_HANDLE hTWD, TI_UINT32 funcType, void *pParam )
{
    switch (funcType)
    {
    case DBG_SCAN_CNCN_PRINT_HELP:
        printScanConcentratorDbgFunctions();
        break;

    case DBG_SCAN_SRV_PRINT_STATUS:
        TWD_PrintMacServDebugStatus (hTWD);
        break;

    default:
        WLAN_OS_REPORT(("Invalid function type in scan concentrator debug function: %d\n", funcType));
        break;
    }
}

/**
 * \\n
 * \date 14-Feb-2005\n
 * \brief Prints scan concentrator debug menu
 *
 * Function Scope \e Public.\n
 */
void printScanConcentratorDbgFunctions(void)
{
    WLAN_OS_REPORT(("   Scan Concentrator Debug Functions   \n"));
    WLAN_OS_REPORT(("---------------------------------------\n"));
    WLAN_OS_REPORT(("1400 - Print the scan concentrator Debug Help\n"));
    WLAN_OS_REPORT(("1401 - Print the scan SRV status\n"));
}

