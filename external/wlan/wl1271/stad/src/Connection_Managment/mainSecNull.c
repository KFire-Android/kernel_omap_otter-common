/*
 * mainSecNull.c
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

/** \file mainSecSm.c
 *  \brief 802.1X finite state machine header file
 *
 *  \see mainSecSm.h
 */


/***************************************************************************/
/*																		   */
/*		MODULE:	mainSecSm.c											   	   */
/*    PURPOSE:	Main Security State Machine API					   		   */
/*																	 	   */
/***************************************************************************/

#define __FILE_ID__  FILE_ID_38
#include "osApi.h"
#include "paramOut.h"
#include "report.h"
#include "DataCtrl_Api.h"
#include "smeApi.h"
#include "rsn.h"
#include "rsnApi.h"
#include "mainSecSm.h"
#include "mainSecNull.h"

/* Constants */

/* Enumerations */

/* Typedefs */

/* Structures */

/* External data definitions */

/* External functions definitions */

/* Global variables */

/* Local function prototypes */

/* functions */

/**
*
* rsn_mainSecSmFullInit
*
* \b Description: 
*
* Init main security state machine state machine
*
* \b ARGS:
*
*  none
*
* \b RETURNS:
*
*  TI_OK on success, TI_NOK otherwise.
*
* \sa 
*/
TI_STATUS mainSecSmNull_config(mainSec_t *pMainSec, 
                            TRsnPaeConfig *pPaeConfig)
{
    pMainSec->start = (mainSecSmStart_t)mainSecSmNull_start;
	pMainSec->stop = mainSecSmNull_stop;
	pMainSec->reportAuthStatus = NULL;
	pMainSec->reportKeysStatus = mainSecNull_reportKeysStatus;
	pMainSec->reportReAuthenticate = NULL;
	pMainSec->getAuthIdentity  = NULL;
	pMainSec->setAuthIdentity  = (mainSecSm_getAuthIdentity_t)mainSecNull_setAuthIdentity;
    pMainSec->getAuthState = mainSecNull_getAuthState;
    pMainSec->reportAuthFailure  = (mainSecSm_reportAuthFailure_t)mainSecNull_reportAuthFailure;
    return TI_OK;
}

/**
*
* mainSecSmNull_Start
*
* \b Description: 
*
* Start the NULL main security SM. Reports success to the rsn module immediately.
*
* \b ARGS:
*
*  none
*
* \b RETURNS:
*
*  TI_OK on success, TI_NOK otherwise.
*
* \sa 
*/
TI_STATUS mainSecSmNull_start(mainSec_t *pMainSec)
{
    TI_STATUS status;

    status = rsn_reportStatus(pMainSec->pParent, TI_OK); 
    
    return status;
}

/**
*
* mainSecSmNull_Stop
*
* \b Description: 
*
* Start the NULL main security SM. Reports success to the rsn module immediately.
*
* \b ARGS:
*
*  none
*
* \b RETURNS:
*
*  TI_OK on success, TI_NOK otherwise.
*
* \sa 
*/
TI_STATUS mainSecSmNull_stop(mainSec_t *pMainSec)
{
    return TI_OK;
}

/**
*
* mainSecNull_reportKeysStatus
*
* \b Description: 
*
* Start the NULL main security SM. Reports success to the rsn module immediately.
*
* \b ARGS:
*
*  none
*
* \b RETURNS:
*
*  TI_OK on success, TI_NOK otherwise.
*
* \sa 
*/
TI_STATUS mainSecNull_reportKeysStatus(mainSec_t *pMainSec, TI_STATUS keysStatus)
{

	return TI_OK;
} 
/**
*
* mainSecKeysOnly_getAuthState:  \n 
*
* \b Description: 
*
* Get authentication state from supp1x SM.
*
* \b ARGS:
*
*  I   - pMainSec - pMainSec SM context  \n
*  I   - authIdentity - pointer to authentication state \n
*
* \b RETURNS:
*
*  TI_OK if successful, TI_NOK otherwise.
*
* \sa 
*/

TI_STATUS mainSecNull_getAuthState(mainSec_t *pMainSec, TIWLN_SECURITY_STATE *secState)
{
	*secState = eSecurityStateHalted; 
	return TI_OK;

} /*mainSecKeysOnly_getAuthState*/


TI_STATUS mainSecSmNull_nop(mainSec_t *pMainSec)
{
	return TI_OK;

} /*mainSecKeysOnly_getAuthState*/


TI_STATUS mainSecNull_reportAuthFailure(mainSec_t *pMainSec, EAuthStatus authStatus) 
{
    return TI_OK;
}


TI_STATUS mainSecNull_setAuthIdentity(mainSec_t *pMainSec, authIdentity_t *authIdentity)
{

	return TI_OK;
}
