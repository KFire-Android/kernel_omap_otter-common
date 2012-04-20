/*
 * PowerSrv.h
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

/** \file powerSrv.h
 *  \brief This is the Power Manager module private (internal).
 *  \author Yaron Menashe
 *  \date 27-Apr-2004
 */

/****************************************************************************
 *                                                                          *
 *   MODULE:  Power Manager                                                 *
 *   PURPOSE: Power Manager Module private                                      *
 *                                                                          *
 ****************************************************************************/

#ifndef _POWER_SRV_H_
#define _POWER_SRV_H_

#include "TWDriverInternal.h"
#include "PowerSrv_API.h"

/*#include "PowerSrvSM.h"*/

/*****************************************************************************
 **         Constants                                                       **
 *****************************************************************************/


/*****************************************************************************
 **         Enumerations                                                    **
 *****************************************************************************/





/*new Power*/


typedef enum 
{
	DRIVER_MODE,
	USER_MODE
}powerSrvMode_e;  

typedef enum 
{
	NEW_REQUEST,
	PENDING_REQUEST,
	RUNNING_REQUEST,
	HANDLED_REQUEST
}powerSrvRequestState_e;


/*****************************************************************************
 **         Typedefs                                                        **
 *****************************************************************************/




/*****************************************************************************
 **         Structures                                                      **
 *****************************************************************************/



typedef struct
{
	powerSrvRequestState_e 		requestState;
	powerSrvMode_e				requestMode;
	E80211PsMode	            psMode;
	TI_BOOL 						sendNullDataOnExit;
	void* 						powerSaveCBObject;
	powerSaveCmpltCB_t    		powerSrvCompleteCB;
	powerSaveCmdResponseCB_t	powerSaveCmdResponseCB;
}	powerSrvRequest_t;   

/** \struct powerSrv_t
 * this structure contain the data of the PowerSrv object.
 */


typedef struct
{
	TI_HANDLE   			hPowerSrvSM;            /**<
                                             					* Hnadle to the Power Server State Machine object.
                                            			 		*/

	TI_HANDLE   			hOS;                        	/**<
                                             					* Handle to the OS object.
                                             					*/

    TI_HANDLE   			hReport;                    	/**<
                                            	 				* Handle to the Report module.
                                             					*/

	TI_HANDLE   			hEventMbox;         		/**< Handle to the power controller object via the WhalCtrl.
											 * Need for configure the desired power mode policy in the system.
											 */
    TI_HANDLE               hCmdBld;
											 
	powerSrvMode_e 		currentMode; 		/**<
											*holds the current mode of the PSS - driver or user...
											*/

	powerSrvMode_e 		userLastRequestMode;
											/**<
											*
											*/

	powerSrvRequest_t 	userRequest;		/**<
											*user request struct.
											*/

	powerSrvRequest_t 	driverRequest;		/**<
											*driver request struct
											*/

	powerSrvRequest_t*  	pCurrentRequest;	/**<
											*pointer to the current request - user/driver request
											*/
	
	TFailureEventCb		failureEventFunc;	/**<
											* upper layer Failure Event CB.
											* called when the scan command has been Timer Expiry
											*/
	TI_HANDLE			failureEventObj;		/**<
											* object parameter passed to the failureEventFunc
											* when it is called 
											*/
	
} powerSrv_t;



/*****************************************************************************
 **         External data definitions                                       **
 *****************************************************************************/


/*****************************************************************************
 **         External functions definitions                                  **
 *****************************************************************************/





/*****************************************************************************
 **         Public Function prototypes                                      **
 *****************************************************************************/

/*****************************************************************************
 **         Private Function prototypes                                     **
 *****************************************************************************/

/*****************************************************************************
 **         Private Function prototypes                                     **
 *****************************************************************************/

#endif /*  _POWER_SRV_H_  */

