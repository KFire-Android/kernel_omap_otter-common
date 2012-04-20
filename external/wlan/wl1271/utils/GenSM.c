/*
 * GenSM.c
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

/** \file GenSM.c
 *  \brief Generic state machine implementation
 *
 *  \see GenSM.h
 */


#define __FILE_ID__  FILE_ID_128
#include "tidef.h"
#include "osApi.h"
#include "report.h"
#include "GenSM.h"


/** 
 * \fn     genSM_Create 
 * \brief  Cerates a generic state machine object
 * 
 * Cerates a generic state machine object. Allocates system resources.
 * 
 * \note   event/action matrix and debug descriptions are used by reference, and are not copied! 
 * \param  hOS - handle to the OS object
 * \return Handle to the generic state machine object
 * \sa     GenSM_Unload, GenSM_Init, genSM_SetDefaults
 */
TI_HANDLE genSM_Create (TI_HANDLE hOS)
{
    TGenSM      *pGenSM = NULL;

    /* Allocate object storage */
    pGenSM = os_memoryAlloc (hOS, sizeof(TGenSM));
    if (NULL != pGenSM)
    {
        /* Store OS handle */
        pGenSM->hOS = hOS;
    }

    return (TI_HANDLE)pGenSM;
}

/** 
 * \fn     genSM_Unload 
 * \brief  Unloads a generic state machine object
 * 
 * Unloads a generic state machine object. Frees system resources consumed by the object.
 * 
 * \param  hGenSM - hanlde to the generic state machine object
 * \return None
 * \sa     GenSM_Create 
 */
void genSM_Unload (TI_HANDLE hGenSM)
{
    TGenSM      *pGenSM =       (TGenSM*)hGenSM;

    /* free the generic state machine object storage */
    os_memoryFree (pGenSM->hOS, hGenSM, sizeof (TGenSM));
}

/** 
 * \fn     genSM_Init 
 * \brief  Initializes the generic state machine object
 * 
 * Initializes the generic state machine object. Store handles to other modules.
 * 
 * \param  hGenSM - hanlde to the generic state machine object
 * \param  hReport - handle to the report module
 * \return None
 * \sa     GenSM_Create, genSM_SetDefaults
 */
void genSM_Init (TI_HANDLE hGenSM, TI_HANDLE hReport)
{
    TGenSM      *pGenSM =       (TGenSM*)hGenSM;

    /* store report handle */
    pGenSM->hReport = hReport;
}

/** 
 * \fn     genSM_SetDefaults 
 * \brief  Set default values to the generic state machine 
 * 
 * Set default values to the generic state machine
 * 
 * \note   event/action matrix and debug descriptions are used by reference, and are not copied! 
 * \param  hGenSM - hanlde to the generic state machine object
 * \param  uStateNum - number of states
 * \param  uEventNum - number of events
 * \param  pMatrix - pointer to the event/actions matrix
 * \param  uInitialState - the initial state
 * \param  pGenSMName - a string describing the state machine, for debug prints
 * \param  pStateDesc - strings describing the state machine states, for debug prints
 * \param  pEventDesc - strings describing the state machine events, for debug prints
 * \param  uModuleLogIndex - Log index used by the module using the state machine
 * \return None
 * \sa     genSM_Create, genSM_Init
 */
void genSM_SetDefaults (TI_HANDLE hGenSM, TI_UINT32 uStateNum, TI_UINT32 uEventNum,
                        TGenSM_matrix pMatrix, TI_UINT32 uInitialState, TI_INT8 *pGenSMName, 
                        TI_INT8 **pStateDesc, TI_INT8 **pEventDesc, TI_UINT32 uModuleLogIndex)
{
    TGenSM      *pGenSM =       (TGenSM*)hGenSM;

    /* set values */
    pGenSM->uStateNum       = uStateNum;
    pGenSM->uEventNum       = uEventNum;
    pGenSM->tMatrix         = pMatrix;
    pGenSM->uCurrentState   = uInitialState;
    pGenSM->pGenSMName      = pGenSMName;
    pGenSM->pStateDesc      = pStateDesc;
    pGenSM->pEventDesc      = pEventDesc;
    pGenSM->uModuleLogIndex = uModuleLogIndex;
    pGenSM->bEventPending   = TI_FALSE;
    pGenSM->bInAction       = TI_FALSE;
}

void genSM_Event (TI_HANDLE hGenSM, TI_UINT32 uEvent, void *pData)
{
    TGenSM              *pGenSM =       (TGenSM*)hGenSM;
    TGenSM_actionCell   *pCell;

	if (pGenSM == NULL)
	{
        TRACE0(pGenSM->hReport, REPORT_SEVERITY_ERROR , "genSM_Event: Handle is NULL!!\n");
		return;
	}

#ifdef TI_DBG
    /* sanity check */
    if (uEvent >= pGenSM->uEventNum)
    {
        TRACE3(pGenSM->hReport, REPORT_SEVERITY_ERROR , "genSM_Event: module: %d received event %d, which is out of events boundry %d\n", pGenSM->uModuleLogIndex, uEvent, pGenSM->uEventNum);
    }
    if (TI_TRUE == pGenSM->bEventPending)
    {
        TRACE3(pGenSM->hReport, REPORT_SEVERITY_ERROR , "genSM_Event: module: %d received event %d, when event %d is pending execution!\n", pGenSM->uModuleLogIndex, uEvent, pGenSM->uEvent);
    }
#endif

    /* mark that an event is pending */
    pGenSM->bEventPending = TI_TRUE;

    /* save event and data */
    pGenSM->uEvent = uEvent;
    pGenSM->pData = pData;

    /* if an event is currently executing, return (new event will be handled when current event is done) */
    if (TI_TRUE == pGenSM->bInAction)
    {
        TRACE1(pGenSM->hReport, REPORT_SEVERITY_INFORMATION , ": module: %d delaying execution of event \n", pGenSM->uModuleLogIndex);
        return;
    }

    /* execute events, until none is pending */
    while (TI_TRUE == pGenSM->bEventPending)
    {
        /* get the cell pointer for the current state and event */
        pCell = &(pGenSM->tMatrix[ (pGenSM->uCurrentState * pGenSM->uEventNum) + pGenSM->uEvent ]);
        

        /* print state transition information */
		TRACE4(pGenSM->hReport, REPORT_SEVERITY_INFORMATION, "genSM_Event: module %d <currentState = %d, event = %d> --> nextState = %d\n", pGenSM->uModuleLogIndex, pGenSM->uCurrentState, uEvent, pCell->uNextState);

        /* mark that event execution is in place */
        pGenSM->bInAction = TI_TRUE;

        /* mark that pending event is being handled */
        pGenSM->bEventPending = TI_FALSE;
        
        /* update current state */
        pGenSM->uCurrentState = pCell->uNextState;

        /* run transition function */
        (*(pCell->fAction)) (pGenSM->pData);

        /* mark that event execution is complete */
        pGenSM->bInAction = TI_FALSE;
    }
}

/** 
 * \fn     genSM_GetCurrentState
 * \brief  retrieves the state machine current state
 * 
 * retrieves the state machine current state
 * 
 * \param  hGenSM - hanlde to the generic state machine object
 * \return state machine current state
 */
TI_UINT32 genSM_GetCurrentState (TI_HANDLE hGenSM)
{
    TGenSM              *pGenSM =       (TGenSM*)hGenSM;

	if (pGenSM == NULL)
	{
        TRACE0(pGenSM->hReport, REPORT_SEVERITY_ERROR , "genSM_GetCurrentState: Handle is NULL!!\n");
		return 0;
	}
    return pGenSM->uCurrentState;
}
