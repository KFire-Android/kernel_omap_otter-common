/*
 * CmdBldItr.c
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


/** \file  CmdBldItr.c 
 *  \brief Command builder. Interrogate commands
 *
 *  \see   CmdBld.h 
 */

#define __FILE_ID__  FILE_ID_95
#include "tidef.h"
#include "CmdBld.h"
#include "CmdBldItrIE.h"


TI_STATUS cmdBld_ItrMemoryMap (TI_HANDLE hCmdBld, MemoryMap_t *apMap, void *fCb, TI_HANDLE hCb)
{
    return cmdBld_ItrIeMemoryMap (hCmdBld, apMap, fCb, hCb);
}


TI_STATUS cmdBld_ItrRoamimgStatisitics (TI_HANDLE hCmdBld, void *fCb, TI_HANDLE hCb, void *pCb)
{
    return cmdBld_ItrIeRoamimgStatisitics (hCmdBld, fCb, hCb, pCb);
}


TI_STATUS cmdBld_ItrErrorCnt (TI_HANDLE hCmdBld, void * fCb, TI_HANDLE hCb, void * pCb)
{
    return cmdBld_ItrIeErrorCnt (hCmdBld, fCb, hCb, pCb);
}


/*
 * ----------------------------------------------------------------------------
 * Function : TWD_GetAverageRSSI
 *
 * Input    :   averageRSSI - pointer for return verage RSSI result
 *
 * Output   :   averageRSSI
 * Process  :
 * -----------------------------------------------------------------------------
 */
TI_STATUS cmdBld_ItrRSSI (TI_HANDLE hCmdBld, void *fCb, TI_HANDLE hCb, void *pCb)
{
    return cmdBld_ItrIeRSSI (hCmdBld, fCb, hCb, pCb);
}


/****************************************************************************
 *                      cmdBld_GetSoftGeminiParams()
 ****************************************************************************
 * DESCRIPTION: Get Soft Gemini config parameter
 *
 * INPUTS:
 *
 * OUTPUT:
 *
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_ItrSg (TI_HANDLE hCmdBld, void *fCb, TI_HANDLE hCb, void* pCb)
{                  
    return cmdBld_ItrIeSg (hCmdBld, fCb, hCb, pCb);
}

TI_STATUS cmdBld_ItrRateParams(TI_HANDLE hCmdBld, void *fCb, TI_HANDLE hCb, void* pCb)
{
    TTwd *pTWD = (TTwd *)hCmdBld;

    return cmdBld_ItrIeRateParams (pTWD->hCmdBld, fCb, hCb, pCb);

}


TI_STATUS cmdBld_ItrStatistics (TI_HANDLE hCmdBld, void *fCb, TI_HANDLE hCb, void *pCb)
{
    return cmdBld_ItrIeStatistics (hCmdBld, fCb, hCb, pCb);
}


TI_STATUS cmdBld_ItrPowerConsumptionstat (TI_HANDLE hTWD, void *fCb, TI_HANDLE hCb, void* pCb)
{
    TTwd *pTWD = (TTwd *)hTWD;

    return  cmdBld_ItrIePowerConsumptionstat (pTWD->hCmdBld, fCb, hCb, pCb);
}

 /****************************************************************************
 *                      cmdBld_ItrDataFilterStatistics()
 ****************************************************************************
 * DESCRIPTION: Get the ACX GWSI counters 
 *
 * INPUTS:
 *
 * OUTPUT:  None
 *
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_ItrDataFilterStatistics (TI_HANDLE hCmdBld, void *fCb, TI_HANDLE hCb, void *pCb)
{
    return cmdBld_ItrIeDataFilterStatistics (hCmdBld, fCb, hCb, pCb);
}

