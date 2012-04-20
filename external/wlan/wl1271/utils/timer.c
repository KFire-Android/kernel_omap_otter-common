/*
 * timer.c
 *
 * Copyright(c) 1998 - 2010 Texas Instruments. All rights reserved.      
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


/** \file   timer.c 
 *  \brief  The timers services OS-Independent layer over the OS-API timer services which are OS-Dependent.
 *  
 *  \see    timer.h, osapi.c
 */

#define __FILE_ID__  FILE_ID_0
#include "osApi.h"
#include "report.h"
#include "queue.h"
#include "context.h"
#include "timer.h"


#define EXPIRY_QUE_SIZE  QUE_UNLIMITED_SIZE

/* The timer module structure (common to all timers) */
typedef struct 
{
    TI_HANDLE   hOs;
    TI_HANDLE   hReport;
    TI_HANDLE   hContext;
    TI_UINT32   uContextId;     /* The ID allocated to this module on registration to context module */
    TI_HANDLE   hInitQueue;     /* Handle of the Init-Queue */
    TI_HANDLE   hOperQueue;     /* Handle of the Operational-Queue */
    TI_BOOL     bOperState;     /* TRUE when the driver is in operational state (not init or recovery) */
    TI_UINT32   uTwdInitCount;  /* Increments on each TWD init (i.e. recovery) */
    TI_UINT32   uTimersCount;   /* Number of created timers */
} TTimerModule;	

/* Per timer structure */
typedef struct 
{
    TI_HANDLE    hTimerModule;             /* The timer module handle (see TTimerModule, needed on expiry) */
    TI_HANDLE    hOsTimerObj;              /* The OS-API timer object handle */
    TQueNodeHdr  tQueNodeHdr;              /* The header used for queueing the timer */
    TTimerCbFunc fExpiryCbFunc;            /* The CB-function provided by the timer user for expiration */
    TI_HANDLE    hExpiryCbHndl;            /* The CB-function handle */
    TI_UINT32    uIntervalMsec;            /* The timer duration in Msec */
    TI_BOOL      bPeriodic;                /* If TRUE, restarted after each expiry */
    TI_BOOL      bOperStateWhenStarted;    /* The bOperState value when the timer was started */
    TI_UINT32    uTwdInitCountWhenStarted; /* The uTwdInitCount value when the timer was started */
} TTimerInfo;	




/** 
 * \fn     tmr_Create 
 * \brief  Create the timer module
 * 
 * Allocate and clear the timer module object.
 * 
 * \note   This is NOT a specific timer creation! (see tmr_CreateTimer)
 * \param  hOs - Handle to Os Abstraction Layer
 * \return Handle of the allocated object 
 * \sa     tmr_Destroy
 */ 
TI_HANDLE tmr_Create (TI_HANDLE hOs)
{
    TI_HANDLE hTimerModule;

    /* allocate module object */
    hTimerModule = os_memoryAlloc (hOs, sizeof(TTimerModule));
	
    if (!hTimerModule)
    {
        WLAN_OS_REPORT (("tmr_Create():  Allocation failed!!\n"));
        return NULL;
    }
	
    os_memoryZero (hOs, hTimerModule, (sizeof(TTimerModule)));

    return (hTimerModule);
}


/** 
 * \fn     tmr_Destroy
 * \brief  Destroy the module. 
 * 
 * Free the module's queues and object.
 * 
 * \note   This is NOT a specific timer destruction! (see tmr_DestroyTimer)
 * \param  hTimerModule - The module object
 * \return TI_OK on success or TI_NOK on failure 
 * \sa     tmr_Create
 */ 
TI_STATUS tmr_Destroy (TI_HANDLE hTimerModule)
{
    TTimerModule *pTimerModule = (TTimerModule *)hTimerModule;

    if (!pTimerModule)
    {
        WLAN_OS_REPORT (("tmr_Destroy(): ERROR - NULL timer!\n"));
        return TI_NOK;
    }

    /* Alert if there are still timers that were not destroyed */
    if (pTimerModule->uTimersCount)
    {
        WLAN_OS_REPORT (("tmr_Destroy():  ERROR - Destroying Timer module but not all timers were destroyed!!\n"));
    }

    /* Destroy the module's queues (protect in critical section)) */
    context_EnterCriticalSection (pTimerModule->hContext);
    que_Destroy (pTimerModule->hInitQueue);
    que_Destroy (pTimerModule->hOperQueue);
    context_LeaveCriticalSection (pTimerModule->hContext);

    /* free module object */
    os_memoryFree (pTimerModule->hOs, pTimerModule, sizeof(TTimerModule));
	
    return TI_OK;
}
/** 
 * \fn     tmr_Free
 * \brief  Free the memory. 
 * 
 * Free the module's queues and object.
 * 
 * \param  hTimerModule - The module object
 * \return TI_OK on success or TI_NOK on failure 
 * \sa     tmr_Create
 */ 
TI_STATUS tmr_Free(TI_HANDLE hTimerModule)
{
    TTimerModule *pTimerModule = (TTimerModule *)hTimerModule;

    if (!pTimerModule)
    {
        WLAN_OS_REPORT (("tmr_Free(): ERROR - NULL timer!\n"));
        return TI_NOK;
    }

    /* free module object */
    os_memoryFree (pTimerModule->hOs, pTimerModule, sizeof(TTimerModule));
	
    return TI_OK;
}


/** 
 * \fn     tmr_ClearInitQueue & tmr_ClearOperQueue
 * \brief  Clear Init/Operationsl queue
 * 
 * Dequeue all queued timers.
 * 
 * \note   
 * \param  hTimerModule - The object                                          
 * \return void 
 * \sa     
 */ 
void tmr_ClearInitQueue (TI_HANDLE hTimerModule)
{
    TTimerModule *pTimerModule = (TTimerModule *)hTimerModule;

    context_EnterCriticalSection (pTimerModule->hContext);
    while (que_Dequeue (pTimerModule->hInitQueue) != NULL) {}
    context_LeaveCriticalSection (pTimerModule->hContext);
}

void tmr_ClearOperQueue (TI_HANDLE hTimerModule)
{
    TTimerModule *pTimerModule = (TTimerModule *)hTimerModule;

    context_EnterCriticalSection (pTimerModule->hContext);
    while (que_Dequeue (pTimerModule->hOperQueue) != NULL) {}
    context_LeaveCriticalSection (pTimerModule->hContext);
}


/** 
 * \fn     tmr_Init 
 * \brief  Init required handles 
 * 
 * Init required handles and module variables, create the init-queue and 
 *     operational-queue, and register as the context-engine client.
 * 
 * \note    
 * \param  hTimerModule  - The queue object
 * \param  hOs       - Handle to Os Abstraction Layer
 * \param  hReport   - Handle to report module
 * \param  hContext  - Handle to context module
 * \return void        
 * \sa     
 */ 
void tmr_Init (TI_HANDLE hTimerModule, TI_HANDLE hOs, TI_HANDLE hReport, TI_HANDLE hContext)
{
    TTimerModule *pTimerModule = (TTimerModule *)hTimerModule;
    TI_UINT32     uNodeHeaderOffset;

    if (!pTimerModule)
    {
        WLAN_OS_REPORT (("tmr_Init(): ERROR - NULL timer!\n"));
        return;
    }

    pTimerModule->hOs           = hOs;
    pTimerModule->hReport       = hReport;
    pTimerModule->hContext      = hContext;
	
    pTimerModule->bOperState    = TI_FALSE;
    pTimerModule->uTimersCount  = 0;
    pTimerModule->uTwdInitCount = 0;

    /* The offset of the queue-node-header from timer structure entry is needed by the queue */
    uNodeHeaderOffset = TI_FIELD_OFFSET(TTimerInfo, tQueNodeHdr); 

    /* Create and initialize the Init and Operational queues (for timers expiry events) */
    pTimerModule->hInitQueue = que_Create (pTimerModule->hOs, 
                                           pTimerModule->hReport, 
                                           EXPIRY_QUE_SIZE, 
                                           uNodeHeaderOffset);
    pTimerModule->hOperQueue = que_Create (pTimerModule->hOs, 
                                           pTimerModule->hReport, 
                                           EXPIRY_QUE_SIZE, 
                                           uNodeHeaderOffset);

    /* Register to the context engine and get the client ID */
    pTimerModule->uContextId = context_RegisterClient (pTimerModule->hContext,
                                                       tmr_HandleExpiry,
                                                       hTimerModule,
                                                       TI_TRUE,
                                                       "TIMER",
                                                       sizeof("TIMER"));
}


/** 
 * \fn     tmr_UpdateDriverState 
 * \brief  Update driver state 
 * 
 * Under critical section, update driver state (operational or not),
 *   and if opertional, clear init queue.
 * Leave critical section and if operational state, request schedule for handling 
 *   timer events in driver context (if any).
 * 
 * \note    
 * \param  hTimerModule - The timer module object
 * \param  bOperState   - TRUE if driver state is now operational, FALSE if not.
 * \return void  
 * \sa     
 */ 
void tmr_UpdateDriverState (TI_HANDLE hTimerModule, TI_BOOL bOperState)
{
    TTimerModule *pTimerModule = (TTimerModule *)hTimerModule;

    if (!pTimerModule)
    {
        WLAN_OS_REPORT (("tmr_UpdateDriverState(): ERROR - NULL timer!\n"));
        return;
    }

    /* Enter critical section */
    context_EnterCriticalSection (pTimerModule->hContext);

    if (bOperState == pTimerModule->bOperState) 
    {
        context_LeaveCriticalSection (pTimerModule->hContext);
        TRACE1(pTimerModule->hReport, REPORT_SEVERITY_ERROR, "tmr_UpdateDriverState(): New bOperState (%d) is as current!\n", bOperState);
        return;
    }

    /* Save new state (TRUE means operational). */
    pTimerModule->bOperState = bOperState;

    /* If new state is operational */
    if (bOperState) 
    {
        /* Increment the TWD initializations counter (for detecting recovery events). */
        pTimerModule->uTwdInitCount++;

        /* Empty the init queue (obsolete). */
        while (que_Dequeue (pTimerModule->hInitQueue) != NULL) {}
    }

    /* Leave critical section */
    context_LeaveCriticalSection (pTimerModule->hContext);

    /* If new state is operational, request switch to driver context for handling timer events */
    if (bOperState) 
    {
        context_RequestSchedule (pTimerModule->hContext, pTimerModule->uContextId);
    }
}



/** 
 * \fn     tmr_CreateTimer
 * \brief  Create a new timer
 * 
 * Create a new timer object, icluding creating a timer in the OS-API.  
 * 
 * \note   This timer creation may be used only after tmr_Create() and tmr_Init() were executed!!
 * \param  hTimerModule - The module handle
 * \return TI_HANDLE    - The created timer handle
 * \sa     tmr_DestroyTimer
 */ 
TI_HANDLE tmr_CreateTimer (TI_HANDLE hTimerModule)
{
    TTimerModule *pTimerModule = (TTimerModule *)hTimerModule; /* The timer module handle */
    TTimerInfo   *pTimerInfo;  /* The created timer handle */

    if (!pTimerModule)
    {
        WLAN_OS_REPORT (("tmr_CreateTimer(): ERROR - NULL timer!\n"));
        return NULL;
    }

    /* Allocate timer object */
    pTimerInfo = os_memoryAlloc (pTimerModule->hOs, sizeof(TTimerInfo));
    if (!pTimerInfo)
    {
        WLAN_OS_REPORT (("tmr_CreateTimer():  Timer allocation failed!!\n"));
        return NULL;
    }
    os_memoryZero (pTimerModule->hOs, pTimerInfo, (sizeof(TTimerInfo)));

    /* Allocate OS-API timer, providing the common expiry callback with the current timer handle */
    pTimerInfo->hOsTimerObj = os_timerCreate(pTimerModule->hOs, tmr_GetExpiry, (TI_HANDLE)pTimerInfo);
    if (!pTimerInfo->hOsTimerObj)
    {
        TRACE0(pTimerModule->hReport, REPORT_SEVERITY_CONSOLE ,"tmr_CreateTimer():  OS-API Timer allocation failed!!\n");
        os_memoryFree (pTimerModule->hOs, pTimerInfo, sizeof(TTimerInfo));
        WLAN_OS_REPORT (("tmr_CreateTimer():  OS-API Timer allocation failed!!\n"));
        return NULL;
    }

    /* Save the timer module handle in the created timer object (needed for the expiry callback) */
    pTimerInfo->hTimerModule = hTimerModule;
    pTimerModule->uTimersCount++;  /* count created timers */

    /* Return the created timer handle */
    return (TI_HANDLE)pTimerInfo;
}


/** 
 * \fn     tmr_DestroyTimer
 * \brief  Destroy the specified timer
 * 
 * Destroy the specified timer object, icluding the timer in the OS-API.  
 * 
 * \note   This timer destruction function should be used before tmr_Destroy() is executed!!
 * \param  hTimerInfo - The timer handle
 * \return TI_OK on success or TI_NOK on failure 
 * \sa     tmr_CreateTimer
 */ 
TI_STATUS tmr_DestroyTimer (TI_HANDLE hTimerInfo)
{
    TTimerInfo *pTimerInfo = (TTimerInfo *)hTimerInfo;  /* The timer handle */     
    TTimerModule *pTimerModule;                  /* The timer module handle */

    if (!pTimerInfo)
    {
        return TI_NOK;
    }
    pTimerModule = (TTimerModule *)pTimerInfo->hTimerModule;
    if (!pTimerModule)
    {
        WLAN_OS_REPORT (("tmr_DestroyTimer(): ERROR - NULL timer!\n"));
        return TI_NOK;
    }

    /* Free the OS-API timer */
    if (pTimerInfo->hOsTimerObj) {
        os_timerDestroy (pTimerModule->hOs, pTimerInfo->hOsTimerObj);
        pTimerModule->uTimersCount--;  /* update created timers number */
    }
    /* Free the timer object */
    os_memoryFree (pTimerModule->hOs, hTimerInfo, sizeof(TTimerInfo));
    return TI_OK;
}


/** 
 * \fn     tmr_StartTimer
 * \brief  Start a timer
 * 
 * Start the specified timer running.
 * 
 * \note   Periodic-Timer may be used by applications that serve the timer expiry 
 *           in a single context.
 *         If an application can't finish serving the timer expiry in a single context, 
 *           e.g. periodic scan, then it isn't recommended to use the periodic timer service.
 *         If such an application uses the periodic timer then it should protect itself from cases
 *            where the timer expires again before the previous timer expiry processing is finished!!
 * \param  hTimerInfo    - The specific timer handle
 * \param  fExpiryCbFunc - The timer's expiry callback function.
 * \param  hExpiryCbHndl - The client's expiry callback function handle.
 * \param  uIntervalMsec - The timer's duration in Msec.
 * \param  bPeriodic     - If TRUE, the timer is restarted after expiry.
 * \return void
 * \sa     tmr_StopTimer, tmr_GetExpiry
 */ 
void tmr_StartTimer (TI_HANDLE     hTimerInfo,
                     TTimerCbFunc  fExpiryCbFunc,
                     TI_HANDLE     hExpiryCbHndl,
                     TI_UINT32     uIntervalMsec,
                     TI_BOOL       bPeriodic)
{
    TTimerInfo   *pTimerInfo   = (TTimerInfo *)hTimerInfo;                 /* The timer handle */     
    TTimerModule *pTimerModule = (TTimerModule *)pTimerInfo->hTimerModule; /* The timer module handle */

    if (!pTimerModule)
    {
        WLAN_OS_REPORT (("tmr_StartTimer(): ERROR - NULL timer!\n"));
        return;
    }

    /* Save the timer parameters. */
    pTimerInfo->fExpiryCbFunc            = fExpiryCbFunc;
    pTimerInfo->hExpiryCbHndl            = hExpiryCbHndl;
    pTimerInfo->uIntervalMsec            = uIntervalMsec;
    pTimerInfo->bPeriodic                = bPeriodic;
    pTimerInfo->bOperStateWhenStarted    = pTimerModule->bOperState;
    pTimerInfo->uTwdInitCountWhenStarted = pTimerModule->uTwdInitCount;

    /* Start OS-API timer running */
    os_timerStart(pTimerModule->hOs, pTimerInfo->hOsTimerObj, uIntervalMsec);
}


/** 
 * \fn     tmr_StopTimer
 * \brief  Stop a running timer
 * 
 * Stop the specified timer.
 * 
 * \note   When using this function, it must be considered that timer expiry may happen
 *           right before the timer is stopped, so it can't be assumed that this completely 
 *           prevents the timer expiry event!
 * \param  hTimerInfo - The specific timer handle
 * \return void
 * \sa     tmr_StartTimer
 */ 
void tmr_StopTimer (TI_HANDLE hTimerInfo)
{
    TTimerInfo   *pTimerInfo   = (TTimerInfo *)hTimerInfo;                 /* The timer handle */     
    TTimerModule *pTimerModule = (TTimerModule *)pTimerInfo->hTimerModule; /* The timer module handle */

    if (!pTimerModule)
    {
        WLAN_OS_REPORT (("tmr_StopTimer(): ERROR - NULL timer!\n"));
        return;
    }

    /* Stop OS-API timer running */
    os_timerStop(pTimerModule->hOs, pTimerInfo->hOsTimerObj);

    /* Clear periodic flag to prevent timer restart if we are in tmr_HandleExpiry context. */
    pTimerInfo->bPeriodic = TI_FALSE;
}


/** 
 * \fn     tmr_GetExpiry
 * \brief  Called by OS-API upon any timer expiry
 * 
 * This is the common callback function called upon expiartion of any timer.
 * It is called by the OS-API in timer expiry context and handles the transition
 *   to the driver's context for handling the expiry event.
 * 
 * \note   
 * \param  hTimerInfo - The specific timer handle
 * \return void
 * \sa     tmr_HandleExpiry
 */ 
void tmr_GetExpiry (TI_HANDLE hTimerInfo)
{
    TTimerInfo   *pTimerInfo   = (TTimerInfo *)hTimerInfo;                 /* The timer handle */     
    TTimerModule *pTimerModule = (TTimerModule *)pTimerInfo->hTimerModule; /* The timer module handle */

    if (!pTimerModule)
    {
        WLAN_OS_REPORT (("tmr_GetExpiry(): ERROR - NULL timer!\n"));
        return;
    }

    /* Enter critical section */
    context_EnterCriticalSection (pTimerModule->hContext);

    /* 
     * If the expired timer was started when the driver's state was Operational,
     *   insert it to the Operational-queue 
     */
    if (pTimerInfo->bOperStateWhenStarted)
    {
        que_Enqueue (pTimerModule->hOperQueue, hTimerInfo);
    }

    /* 
     * Else (started when driver's state was NOT-Operational), if now the state is still
     *   NOT Operational insert it to the Init-queue.
     *   (If state changed from non-operational to operational the event is ignored)
     */
    else if (!pTimerModule->bOperState)
    {
        que_Enqueue (pTimerModule->hInitQueue, hTimerInfo);
    }

    /* Leave critical section */
    context_LeaveCriticalSection (pTimerModule->hContext);

    /* Request switch to driver context for handling timer events */
    context_RequestSchedule (pTimerModule->hContext, pTimerModule->uContextId);
}


/** 
 * \fn     tmr_HandleExpiry
 * \brief  Handles queued expiry events in driver context
 * 
 * This is the Timer module's callback that is registered to the ContextEngine module to be invoked
 *   from the driver task (after requested by tmr_GetExpiry through context_RequestSchedule ()).
 * It dequeues all expiry events from the queue that correlates to the current driver state,
 *   and calls their users callbacks.
 * 
 * \note   
 * \param  hTimerModule - The module object
 * \return void
 * \sa     tmr_GetExpiry
 */ 
void tmr_HandleExpiry (TI_HANDLE hTimerModule)
{
    TTimerModule *pTimerModule = (TTimerModule *)hTimerModule; /* The timer module handle */
    TTimerInfo   *pTimerInfo;      /* The timer handle */     
    TI_BOOL       bTwdInitOccured; /* Indicates if TWD init occured since timer start */

    if (!pTimerModule)
    {
        WLAN_OS_REPORT (("tmr_HandleExpiry(): ERROR - NULL timer!\n"));
        return;
    }

    while (1)
    {
        /* Enter critical section */
        context_EnterCriticalSection (pTimerModule->hContext);
    
        /* If current driver state is Operational, dequeue timer object from Operational-queue */
        if (pTimerModule->bOperState)
        {
            pTimerInfo = (TTimerInfo *) que_Dequeue (pTimerModule->hOperQueue);
        }
    
        /* Else (driver state is NOT-Operational), dequeue timer object from Init-queue */
        else
        {
            pTimerInfo = (TTimerInfo *) que_Dequeue (pTimerModule->hInitQueue);
        }
    
        /* Leave critical section */
        context_LeaveCriticalSection (pTimerModule->hContext);

        /* If no more objects in queue, exit */
        if (!pTimerInfo) 
        {
            return;  /** EXIT Point **/
        }

        /* If current TWD-Init-Count is different than when the timer was started, Init occured. */
        bTwdInitOccured = (pTimerModule->uTwdInitCount != pTimerInfo->uTwdInitCountWhenStarted);

        /* Call specific timer callback function */
        pTimerInfo->fExpiryCbFunc (pTimerInfo->hExpiryCbHndl, bTwdInitOccured);

        /* If the expired timer is periodic, start it again. */
        if (pTimerInfo->bPeriodic) 
        {
            tmr_StartTimer ((TI_HANDLE)pTimerInfo,
                            pTimerInfo->fExpiryCbFunc,
                            pTimerInfo->hExpiryCbHndl,
                            pTimerInfo->uIntervalMsec,
                            pTimerInfo->bPeriodic);
        }
    }
}


/** 
 * \fn     tmr_PrintModule / tmr_PrintTimer
 * \brief  Print module / timer information
 * 
 * Print the module's information / a specific timer information.
 * 
 * \note   
 * \param  The module / timer handle
 * \return void
 * \sa     
 */ 

#ifdef TI_DBG

void tmr_PrintModule (TI_HANDLE hTimerModule)
{
    TTimerModule *pTimerModule = (TTimerModule *)hTimerModule;

    if (!pTimerModule)
    {
        WLAN_OS_REPORT (("tmr_PrintModule(): ERROR - NULL timer!\n"));
        return;
    }

    /* Print module parameters */
    WLAN_OS_REPORT(("tmr_PrintModule(): uContextId=%d, bOperState=%d, uTwdInitCount=%d, uTimersCount=%d\n", 
    pTimerModule->uContextId, pTimerModule->bOperState, 
    pTimerModule->uTwdInitCount, pTimerModule->uTimersCount));

    /* Print Init Queue Info */
    WLAN_OS_REPORT(("tmr_PrintModule(): Init-Queue:\n")); 
    que_Print(pTimerModule->hInitQueue);

    /* Print Operational Queue Info */
    WLAN_OS_REPORT(("tmr_PrintModule(): Operational-Queue:\n")); 
    que_Print(pTimerModule->hOperQueue);
}

void tmr_PrintTimer (TI_HANDLE hTimerInfo)
{
#ifdef REPORT_LOG
    TTimerInfo   *pTimerInfo   = (TTimerInfo *)hTimerInfo;                 /* The timer handle */     

    WLAN_OS_REPORT(("tmr_PrintTimer(): uIntervalMs=%d, bPeriodic=%d, bOperStateWhenStarted=%d, uTwdInitCountWhenStarted=%d, hOsTimerObj=0x%x, fExpiryCbFunc=0x%x\n", 
    pTimerInfo->uIntervalMsec, pTimerInfo->bPeriodic, pTimerInfo->bOperStateWhenStarted, 
    pTimerInfo->uTwdInitCountWhenStarted, pTimerInfo->hOsTimerObj, pTimerInfo->fExpiryCbFunc));
#endif
}

#endif /* TI_DBG */
