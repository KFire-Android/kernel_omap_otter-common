/*
 * CmdHndlr.c
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

 
/** \file   CmdHndlr.c 
 *  \brief  The Command-Hnadler module.
 *  
 *  \see    CmdHndlr.h
 */

#define __FILE_ID__  FILE_ID_48
#include "tidef.h"
#include "commonTypes.h"
#include "osApi.h"
#include "report.h"
#include "queue.h"
#include "context.h"
#include "CmdHndlr.h"
#include "CmdInterpret.h"
#include "DrvMainModules.h"


/* The queue may contain only one command per configuration application but set as unlimited */
#define COMMANDS_QUE_SIZE   QUE_UNLIMITED_SIZE   

/* Command module internal data */
typedef struct 
{
   TI_HANDLE       hOs;            
   TI_HANDLE       hReport;        
   TI_HANDLE       hContext;       
   TI_HANDLE       hCmdInterpret;

   TI_HANDLE       hCmdQueue;       /* Handle to the commands queue */
   TI_BOOL         bProcessingCmds; /* Indicates if currently processing commands */
   TI_UINT32       uContextId;      /* ID allocated to this module on registration to context module */
   TConfigCommand *pCurrCmd;        /* Pointer to the command currently being processed */
} TCmdHndlrObj;

/* External functions prototypes */
extern void wlanDrvIf_CommandDone (TI_HANDLE hOs, void *pSignalObject, TI_UINT8 *CmdResp_p);

/** 
 * \fn     cmdHndlr_Create
 * \brief  Create the module
 * 
 * Create the module object
 * 
 * \note   
 * \param  hOs - Handle to the Os Abstraction Layer                           
 * \return Handle to the allocated module (NULL if failed) 
 * \sa     
 */ 
TI_HANDLE cmdHndlr_Create (TI_HANDLE hOs, TI_HANDLE hEvHandler)
{
    TCmdHndlrObj *pCmdHndlr = (TCmdHndlrObj *) os_memoryAlloc (hOs, sizeof(TCmdHndlrObj));

    if (pCmdHndlr == NULL) 
    {
        return NULL;
    }

    os_memoryZero (hOs, (void *)pCmdHndlr, sizeof(TCmdHndlrObj));

    pCmdHndlr->hOs = hOs;

    pCmdHndlr->hCmdInterpret = cmdInterpret_Create (hOs);

    if (pCmdHndlr->hCmdInterpret == NULL) 
    {
		cmdHndlr_Destroy ((TI_HANDLE) pCmdHndlr, (TI_HANDLE) hEvHandler);
        return NULL;
    }

    return (TI_HANDLE) pCmdHndlr;
}


/** 
 * \fn     cmdHndlr_Destroy
 * \brief  Destroy the module object
 * 
 * Destroy the module object.
 * 
 * \note   
 * \param  hCmdHndlr - The object                                          
 * \return TI_OK 
 * \sa     
 */ 
TI_STATUS cmdHndlr_Destroy (TI_HANDLE hCmdHndlr, TI_HANDLE hEvHandler)
{
    TCmdHndlrObj *pCmdHndlr = (TCmdHndlrObj *)hCmdHndlr;

	if (pCmdHndlr->hCmdInterpret)
	{
		cmdInterpret_Destroy (pCmdHndlr->hCmdInterpret, hEvHandler);
	}

    cmdHndlr_ClearQueue (hCmdHndlr);

	if (pCmdHndlr->hCmdQueue)
	{
		que_Destroy (pCmdHndlr->hCmdQueue);
	}

	os_memoryFree (pCmdHndlr->hOs, hCmdHndlr, sizeof(TCmdHndlrObj));

    return TI_OK;
}


/** 
 * \fn     cmdHndlr_ClearQueue
 * \brief  Clear commands queue
 * 
 * Dequeue and free all queued commands.
 * 
 * \note   
 * \param  hCmdHndlr - The object                                          
 * \return void 
 * \sa     
 */ 
void cmdHndlr_ClearQueue (TI_HANDLE hCmdHndlr)
{
    TCmdHndlrObj    *pCmdHndlr = (TCmdHndlrObj *)hCmdHndlr;
    TConfigCommand  *pCurrCmd;

    /* Dequeue and free all queued commands */
    do {
        context_EnterCriticalSection (pCmdHndlr->hContext);
        pCurrCmd = (TConfigCommand *)que_Dequeue(pCmdHndlr->hCmdQueue);
        context_LeaveCriticalSection (pCmdHndlr->hContext);
        if (pCurrCmd != NULL) {
            /* Just release the semaphore. The command is freed subsequently. */
            os_SignalObjectSet (pCmdHndlr->hOs, pCurrCmd->pSignalObject);
        }
    } while (pCurrCmd != NULL);
}


/** 
 * \fn     cmdHndlr_Init 
 * \brief  Init required handles and registries
 * 
 * Init required handles and module variables, create the commands-queue and 
 *     register as the context-engine client.
 * 
 * \note    
 * \param  pStadHandles  - The driver modules handles
 * \return void  
 * \sa     
 */ 
void cmdHndlr_Init (TStadHandlesList *pStadHandles)
{
    TCmdHndlrObj *pCmdHndlr = (TCmdHndlrObj *)(pStadHandles->hCmdHndlr);
    TI_UINT32     uNodeHeaderOffset;

    pCmdHndlr->hReport  = pStadHandles->hReport;
    pCmdHndlr->hContext = pStadHandles->hContext;

    cmdInterpret_Init (pCmdHndlr->hCmdInterpret, pStadHandles);

    /* The offset of the queue-node-header from the commands structure entry is needed by the queue */
    uNodeHeaderOffset = TI_FIELD_OFFSET(TConfigCommand, tQueNodeHdr); 

    /* Create and initialize the commands queue */
    pCmdHndlr->hCmdQueue = que_Create (pCmdHndlr->hOs, pCmdHndlr->hReport, COMMANDS_QUE_SIZE, uNodeHeaderOffset);

    /* Register to the context engine and get the client ID */
    pCmdHndlr->uContextId = context_RegisterClient (pCmdHndlr->hContext,
                                                    cmdHndlr_HandleCommands,
                                                    (TI_HANDLE)pCmdHndlr,
                                                    TI_FALSE,
                                                    "COMMAND",
                                                    sizeof("COMMAND"));

	if(pCmdHndlr->hReport != NULL)
	{
		os_setDebugOutputToLogger(TI_FALSE);
	}
}


/** 
 * \fn     cmdHndlr_InsertCommand 
 * \brief  Insert a new command to the driver
 * 
 * Insert a new command to the commands queue from user context.
 * If commands are not beeing processed set a request to start processing in the driver context.
 * Wait on the current command's signal until its processing is completed.
 * Note that this prevents the user application from sending further commands before completion.
 * 
 * \note   
 * \param  hCmdHndlr    - The module object
 * \param  cmd          - User request
 * \param  others       - The command flags, data and params
 * \return TI_OK if command processed successfully, TI_NOK if failed in processing or memory allocation.  
 * \sa     cmdHndlr_HandleCommands, cmdHndlr_Complete
 */ 
TI_STATUS cmdHndlr_InsertCommand (TI_HANDLE     hCmdHndlr,
                                  TI_UINT32     cmd,
                                  TI_UINT32     flags,
                                  void         *buffer1,
                                  TI_UINT32     buffer1_len,
                                  void         *buffer2,
                                  TI_UINT32     buffer2_len,
                                  TI_UINT32    *param3,
                                  TI_UINT32    *param4)
{
    TCmdHndlrObj     *pCmdHndlr = (TCmdHndlrObj *)hCmdHndlr;
	TConfigCommand   *pNewCmd;
	TI_STATUS         eStatus;

	/* Allocated command structure */
	pNewCmd = os_memoryAlloc (pCmdHndlr->hOs, sizeof (TConfigCommand));
	if (pNewCmd == NULL)
	{
		return TI_NOK;
	}
    os_memoryZero (pCmdHndlr->hOs, (void *)pNewCmd, sizeof(TConfigCommand));

	/* Copy user request into local structure */
	pNewCmd->cmd = cmd;
	pNewCmd->flags = flags;
	pNewCmd->buffer1 = buffer1;
	pNewCmd->buffer1_len = buffer1_len;
	pNewCmd->buffer2 = buffer2;
	pNewCmd->buffer2_len = buffer2_len;
	pNewCmd->param3 = param3;
	pNewCmd->param4 = param4;
	pNewCmd->pSignalObject = os_SignalObjectCreate (pCmdHndlr->hOs); /* initialize "complete-flag" */

	/* If creating the signal object failed */
	if (pNewCmd->pSignalObject == NULL)
	{
		os_printf("cmdPerform: Failed to create signalling object\n");
		/* free allocated memory and return error */
		os_memoryFree (pCmdHndlr->hOs, pNewCmd, sizeof (TConfigCommand));
		return TI_NOK;
	}

    /* Indicate the start of command process, from adding it to the queue until get return status form it */  
    pNewCmd->bWaitFlag = TI_TRUE;

    /* Enter critical section to protect queue access */
    context_EnterCriticalSection (pCmdHndlr->hContext);

    /* Enqueue the command (if failed, release memory and return NOK) */
    eStatus = que_Enqueue (pCmdHndlr->hCmdQueue, (TI_HANDLE)pNewCmd);
    if (eStatus != TI_OK) 
    {
        os_printf("cmdPerform: Failed to enqueue new command\n");
        os_SignalObjectFree (pCmdHndlr->hOs, pNewCmd->pSignalObject);
        pNewCmd->pSignalObject = NULL;
        os_memoryFree (pCmdHndlr->hOs, pNewCmd, sizeof (TConfigCommand));
        context_LeaveCriticalSection (pCmdHndlr->hContext);  /* Leave critical section */
        return TI_NOK;
    }

    /* 
     * Note: The bProcessingCmds flag is used for indicating if we are already processing
     *           the queued commands, so the context-engine shouldn't invoke cmdHndlr_HandleCommands.
     *       This is important because if we make this decision according to the queue being empty,
     *           there may be a command under processing (already dequeued) while the queue is empty.
     *       Note that although we are blocking the current command's originator, there may be another
     *           application that will issue a command.
     */

    if (pCmdHndlr->bProcessingCmds)
    {
        /* No need to schedule the driver (already handling commands) so just leave critical section */
        context_LeaveCriticalSection (pCmdHndlr->hContext);
    }
    else
    {
        /* Indicate that we are handling queued commands (before leaving critical section!) */
        pCmdHndlr->bProcessingCmds = TI_TRUE;

        /* Leave critical section */
        context_LeaveCriticalSection (pCmdHndlr->hContext);

        /* Request driver task schedule for command handling (after we left critical section!) */
        context_RequestSchedule (pCmdHndlr->hContext, pCmdHndlr->uContextId);
    }

	/* Wait until the command is executed */
	os_SignalObjectWait (pCmdHndlr->hOs, pNewCmd->pSignalObject);

	/* After "wait" - the command has already been processed by the drivers' context */
	/* Indicate the end of command process, from adding it to the queue until get return status form it */  
	pNewCmd->bWaitFlag = TI_FALSE;

	/* Copy the return code */
	eStatus = pNewCmd->return_code;

	/* Free signalling object and command structure */
	os_SignalObjectFree (pCmdHndlr->hOs, pNewCmd->pSignalObject);
	pNewCmd->pSignalObject = NULL;

	/* If command not completed in this context (Async) don't free the command memory */
	if(COMMAND_PENDING != pNewCmd->eCmdStatus)
	{
		os_memoryFree (pCmdHndlr->hOs, pNewCmd, sizeof (TConfigCommand));
	}

	/* Return to calling process with command return code */
	return eStatus;
}



/** 
 * \fn     cmdHndlr_HandleCommands 
 * \brief  Handle queued commands
 * 
 * While there are queued commands, dequeue a command and call the 
 *     commands interpreter (OID or WEXT selected at compile time).
 * If the command processing is not completed in this context (pending), we exit and 
 *     this function is called again upon commnad completion, so it can continue processing 
 *     further queued commands (if any).
 * 
 * \note    
 * \param  hCmdHndlr - The module object
 * \return void
 * \sa     cmdHndlr_InsertCommand, cmdHndlr_Complete
 */ 
void cmdHndlr_HandleCommands (TI_HANDLE hCmdHndlr)
{
    TCmdHndlrObj     *pCmdHndlr = (TCmdHndlrObj *)hCmdHndlr;

    while (1)
    {
        /* Enter critical section to protect queue access */
        context_EnterCriticalSection (pCmdHndlr->hContext);

        /* Dequeue a command */
        pCmdHndlr->pCurrCmd = (TConfigCommand *) que_Dequeue (pCmdHndlr->hCmdQueue);

        /* If we have got a command */
        if (pCmdHndlr->pCurrCmd) 
        {
            /* Leave critical section */
            context_LeaveCriticalSection (pCmdHndlr->hContext);

            /* Convert to driver structure and execute command */
            pCmdHndlr->pCurrCmd->eCmdStatus = cmdInterpret_convertAndExecute (pCmdHndlr->hCmdInterpret, pCmdHndlr->pCurrCmd);

            /* 
             *  If command not completed in this context (Async), return.
             *    (we'll be called back upon command completion) 
             */
            if(COMMAND_PENDING == pCmdHndlr->pCurrCmd->eCmdStatus)
            {
                return;
            }

            /* Command was completed so free the wait signal and continue to next command */
            wlanDrvIf_CommandDone(pCmdHndlr->hOs, pCmdHndlr->pCurrCmd->pSignalObject, pCmdHndlr->pCurrCmd->CmdRespBuffer); 

            pCmdHndlr->pCurrCmd = NULL;

        }

        /* Else, we don't have commands to handle */
        else
        {
            /* Indicate that we are not handling commands (before leaving critical section!) */
            pCmdHndlr->bProcessingCmds = TI_FALSE;

            /* Leave critical section */
            context_LeaveCriticalSection (pCmdHndlr->hContext);

            /* Exit (no more work) */
            return;
        }
    }
}


/** 
 * \fn     cmdHndlr_Complete 
 * \brief  called whenever a command has finished executing
 * 
 * This routine is called whenever a command has finished executing.
 * Either called by the cmdHndlr_HandleCommands if completed in the same context, 
 *     or by the CmdInterpreter module when tcompleted in a later context (Async).
 * 
 * \note    
 * \param  hCmdHndlr - The module object
 * \return void
 * \sa     cmdHndlr_InsertCommand, cmdHndlr_HandleCommands
 */ 
void cmdHndlr_Complete (TI_HANDLE hCmdHndlr)
{
    TCmdHndlrObj *pCmdHndlr = (TCmdHndlrObj *)hCmdHndlr;
    TI_BOOL       bLocalWaitFlag;

    if (pCmdHndlr->pCurrCmd)
    {
        /* set Status to COMPLETE */
        pCmdHndlr->pCurrCmd->eCmdStatus = TI_OK;
    
        /* save the wait flag before free semaphore */
        bLocalWaitFlag = pCmdHndlr->pCurrCmd->bWaitFlag;

        wlanDrvIf_CommandDone(pCmdHndlr->hOs, pCmdHndlr->pCurrCmd->pSignalObject, pCmdHndlr->pCurrCmd->CmdRespBuffer); 

        /* if cmdHndlr_InsertCommand() not wait to cmd complete? */
        if (TI_FALSE == bLocalWaitFlag)
        {
            /* no wait, free the command memory */
            os_memoryFree (pCmdHndlr->hOs, pCmdHndlr->pCurrCmd, sizeof (TConfigCommand));
        }

        pCmdHndlr->pCurrCmd = NULL;

        return;
    }

    TRACE0(pCmdHndlr->hReport, REPORT_SEVERITY_ERROR, "cmdHndlr_Complete(): pCurrCmd is NULL!\n");
}


/** 
 * \fn     cmdHndlr_GetStat
 * \brief  Get driver statistics
 * 
 * Get the driver statistics (Tx, Rx, signal quality).
 *
 * \note   
 * \param  hCmdHndlr - The object                                          
 * \return The driver statistics pointer 
 * \sa     
 */ 
void * cmdHndlr_GetStat (TI_HANDLE hCmdHndlr)
{
    TCmdHndlrObj *pCmdHndlr = (TCmdHndlrObj *)hCmdHndlr;

    return cmdInterpret_GetStat (pCmdHndlr->hCmdInterpret);
}


/** 
 * \fn     cmdHndlr_Enable & cmdHndlr_Disable
 * \brief  Enable/Disable invoking CmdHndlr module from driver-task
 * 
 * Called by the Driver-Main Init SM to enable/disable external inputs processing.
 * Calls the context-engine enable/disable function accordingly.
 *
 * \note   
 * \param  hCmdHndlr - The object                                          
 * \return void 
 * \sa     
 */ 
void cmdHndlr_Enable (TI_HANDLE hCmdHndlr)
{
    TCmdHndlrObj *pCmdHndlr = (TCmdHndlrObj *)hCmdHndlr;

    context_EnableClient (pCmdHndlr->hContext, pCmdHndlr->uContextId);
}

void cmdHndlr_Disable (TI_HANDLE hCmdHndlr)
{
    TCmdHndlrObj *pCmdHndlr = (TCmdHndlrObj *)hCmdHndlr;

    context_DisableClient (pCmdHndlr->hContext, pCmdHndlr->uContextId);
}

