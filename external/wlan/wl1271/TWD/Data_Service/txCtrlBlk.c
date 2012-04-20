/*
 * txCtrlBlk.c
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


/****************************************************************************
 *
 *   MODULE:  txCtrlBlk.c
 *   
 *   PURPOSE: Maintains active packets Tx attributes table (including descriptor). 
 * 
 *	 DESCRIPTION:  
 *   ============
 *		This module allocates and frees table entry for each packet in the Tx
 *		process (from sendPkt by upper driver until Tx-complete).
 *
 ****************************************************************************/
#define __FILE_ID__  FILE_ID_99
#include "osApi.h"
#include "tidef.h"
#include "report.h"
#include "context.h"
#include "TWDriver.h"
#include "txCtrlBlk_api.h"


/* The TxCtrlBlk module object - contains the control-block table. */
typedef struct
{
	TI_HANDLE   hOs;
	TI_HANDLE   hReport;
	TI_HANDLE   hContext;

	TTxCtrlBlk  aTxCtrlBlkTbl[CTRL_BLK_ENTRIES_NUM]; /* The table of control-block entries. */

#ifdef TI_DBG  /* Just for debug. */
	TI_UINT32	uNumUsedEntries;  
#endif

} TTxCtrlBlkObj;


/****************************************************************************
 *                      txCtrlBlk_Create()
 ****************************************************************************
 * DESCRIPTION:	Create the Tx control block table object 
 * 
 * INPUTS:	hOs
 * 
 * OUTPUT:	None
 * 
 * RETURNS:	The Created object
 ****************************************************************************/
TI_HANDLE txCtrlBlk_Create (TI_HANDLE hOs)
{
	TTxCtrlBlkObj *pTxCtrlBlk;

	pTxCtrlBlk = os_memoryAlloc (hOs, sizeof(TTxCtrlBlkObj));
	if (pTxCtrlBlk == NULL)
		return NULL;

	os_memoryZero (hOs, pTxCtrlBlk, sizeof(TTxCtrlBlkObj));

	pTxCtrlBlk->hOs = hOs;

	return( (TI_HANDLE)pTxCtrlBlk );
}


/****************************************************************************
 *                      txCtrlBlk_Destroy()
 ****************************************************************************
 * DESCRIPTION:	Destroy the Tx control block table object 
 * 
 * INPUTS:	hTxCtrlBlk - The object to free
 * 
 * OUTPUT:	None
 * 
 * RETURNS:	TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS txCtrlBlk_Destroy (TI_HANDLE hTxCtrlBlk)
{
	TTxCtrlBlkObj *pTxCtrlBlk = (TTxCtrlBlkObj *)hTxCtrlBlk;

	if (pTxCtrlBlk)
		os_memoryFree(pTxCtrlBlk->hOs, pTxCtrlBlk, sizeof(TTxCtrlBlkObj));

	return TI_OK;
}


/****************************************************************************
 *               txCtrlBlk_Init()
 ****************************************************************************
   DESCRIPTION:	 Initialize the Tx control block module.
 ****************************************************************************/
TI_STATUS txCtrlBlk_Init (TI_HANDLE hTxCtrlBlk, TI_HANDLE hReport, TI_HANDLE hContext)
{
	TTxCtrlBlkObj *pTxCtrlBlk = (TTxCtrlBlkObj *)hTxCtrlBlk;
	TTxnStruct    *pTxn;
	TI_UINT8       entry;

	pTxCtrlBlk->hReport  = hReport;
	pTxCtrlBlk->hContext = hContext;
	
	/* For all entries, write the entry index in the descriptor and the next entry address
		 in the next free entery pointer. Init also some other fields. */
	for(entry = 0; entry < CTRL_BLK_ENTRIES_NUM; entry++)
	{
		pTxCtrlBlk->aTxCtrlBlkTbl[entry].tTxDescriptor.descID = entry;
		pTxCtrlBlk->aTxCtrlBlkTbl[entry].pNextFreeEntry       = &(pTxCtrlBlk->aTxCtrlBlkTbl[entry + 1]);
		pTxCtrlBlk->aTxCtrlBlkTbl[entry].tTxDescriptor.aid    = 1;  /* The value for infrastructure BSS */
		pTxCtrlBlk->aTxCtrlBlkTbl[entry].tTxDescriptor.reserved  = 0;

        /* Prepare the Txn fields to the host-slave register (fixed address) */
        pTxn = &(pTxCtrlBlk->aTxCtrlBlkTbl[entry].tTxnStruct);
        TXN_PARAM_SET(pTxn, TXN_LOW_PRIORITY, TXN_FUNC_ID_WLAN, TXN_DIRECTION_WRITE, TXN_FIXED_ADDR)
	}

	/* Write null in the next-free index of the last entry. */
	pTxCtrlBlk->aTxCtrlBlkTbl[CTRL_BLK_ENTRIES_NUM - 1].pNextFreeEntry = NULL;

  #ifdef TI_DBG
	pTxCtrlBlk->uNumUsedEntries = 0;
  #endif

	return TI_OK;
}


/****************************************************************************
 *					txCtrlBlk_Alloc()
 ****************************************************************************
 * DESCRIPTION:	 
	Allocate a free control-block entry for the current Tx packet's parameters
	  (including the descriptor structure).
	Note that entry 0 in the list is never allocated and points to the
	  first free entry.
 ****************************************************************************/
TTxCtrlBlk *txCtrlBlk_Alloc (TI_HANDLE hTxCtrlBlk)
{
	TTxCtrlBlkObj   *pTxCtrlBlk = (TTxCtrlBlkObj *)hTxCtrlBlk;
	TTxCtrlBlk      *pCurrentEntry; /* The pointer of the new entry allocated for the packet. */
	TTxCtrlBlk      *pFirstFreeEntry; /* The first entry just points to the first free entry. */ 

	pFirstFreeEntry = &(pTxCtrlBlk->aTxCtrlBlkTbl[0]); 

    /* Protect block allocation from preemption (may be called from external context) */
    context_EnterCriticalSection (pTxCtrlBlk->hContext);

    pCurrentEntry = pFirstFreeEntry->pNextFreeEntry; /* Get free entry. */

#ifdef TI_DBG
	/* If no free entries, print error (not expected to happen) and return NULL. */
	if (pCurrentEntry->pNextFreeEntry == NULL)
	{
TRACE1(pTxCtrlBlk->hReport, REPORT_SEVERITY_ERROR, "txCtrlBlk_alloc():  No free entry,  UsedEntries=%d\n", pTxCtrlBlk->uNumUsedEntries);
        context_LeaveCriticalSection (pTxCtrlBlk->hContext);
		return NULL;
	}
	pTxCtrlBlk->uNumUsedEntries++;
#endif

	/* Link the first entry to the next free entry. */
	pFirstFreeEntry->pNextFreeEntry = pCurrentEntry->pNextFreeEntry;

    context_LeaveCriticalSection (pTxCtrlBlk->hContext);
	
	/* Clear the next-free-entry index just as an indication that our entry is not free. */
	pCurrentEntry->pNextFreeEntry = 0;

    pCurrentEntry->tTxPktParams.uFlags = 0;
    pCurrentEntry->tTxPktParams.uHeadroomSize = 0;

	return pCurrentEntry;
}


/****************************************************************************
 *					txCtrlBlk_Free()
 ****************************************************************************
 * DESCRIPTION:	
	Link the freed entry after entry 0, so now it is the first free entry to
	  be allocated.
 ****************************************************************************/
void txCtrlBlk_Free (TI_HANDLE hTxCtrlBlk, TTxCtrlBlk *pCurrentEntry)
{
	TTxCtrlBlkObj   *pTxCtrlBlk = (TTxCtrlBlkObj *)hTxCtrlBlk;
	TTxCtrlBlk *pFirstFreeEntry = &(pTxCtrlBlk->aTxCtrlBlkTbl[0]);

	if (!pTxCtrlBlk)
    {
		return;
	}

#ifdef TI_DBG
	/* If the pointed entry is already free, print error and exit (not expected to happen). */
	if (pCurrentEntry->pNextFreeEntry != 0)
	{
		TRACE2(pTxCtrlBlk->hReport, REPORT_SEVERITY_ERROR, "txCtrlBlk_free(): Entry %d alredy free, UsedEntries=%d\n", 			pCurrentEntry->tTxDescriptor.descID, pTxCtrlBlk->uNumUsedEntries);
		return;
	}
	pTxCtrlBlk->uNumUsedEntries--;
#endif

	/* Protect block freeing from preemption (may be called from external context) */
	context_EnterCriticalSection (pTxCtrlBlk->hContext);

	/* Link the freed entry between entry 0 and the next free entry. */
	pCurrentEntry->pNextFreeEntry   = pFirstFreeEntry->pNextFreeEntry;
	pFirstFreeEntry->pNextFreeEntry = pCurrentEntry;

	context_LeaveCriticalSection (pTxCtrlBlk->hContext);
}


/****************************************************************************
 *					txCtrlBlk_GetPointer()
 ****************************************************************************
 * DESCRIPTION:	 
	Return a pointer to the control block entry of the requested packet.
	Used upon tx-complete to retrieve info after getting the descId from the FW.
 ****************************************************************************/
TTxCtrlBlk *txCtrlBlk_GetPointer (TI_HANDLE hTxCtrlBlk, TI_UINT8 descId)
{
	TTxCtrlBlkObj *pTxCtrlBlk = (TTxCtrlBlkObj *)hTxCtrlBlk;
	return ( &(pTxCtrlBlk->aTxCtrlBlkTbl[descId]) );
}


/****************************************************************************
 *                      txCtrlBlk_PrintTable()
 ****************************************************************************
 * DESCRIPTION:	 Print the txCtrlBlk table main fields.
 ****************************************************************************/
#ifdef TI_DBG
void txCtrlBlk_PrintTable (TI_HANDLE hTxCtrlBlk)
{
#ifdef REPORT_LOG
	TTxCtrlBlkObj *pTxCtrlBlk = (TTxCtrlBlkObj *)hTxCtrlBlk;
	TI_UINT8 entry;

	WLAN_OS_REPORT((" Tx-Control-Block Information,  UsedEntries=%d\n", pTxCtrlBlk->uNumUsedEntries));
	WLAN_OS_REPORT(("==============================================\n"));
	
	for(entry = 0; entry < CTRL_BLK_ENTRIES_NUM; entry++)
	{
		WLAN_OS_REPORT(("Entry %d: DescID=%d, Next=0x%x, Len=%d, StartTime=%d, TID=%d, ExtraBlks=%d, TotalBlks=%d, Flags=0x%x\n", 
			entry, 
			pTxCtrlBlk->aTxCtrlBlkTbl[entry].tTxDescriptor.descID,
			pTxCtrlBlk->aTxCtrlBlkTbl[entry].pNextFreeEntry,
			ENDIAN_HANDLE_WORD(pTxCtrlBlk->aTxCtrlBlkTbl[entry].tTxDescriptor.length),
			ENDIAN_HANDLE_LONG(pTxCtrlBlk->aTxCtrlBlkTbl[entry].tTxDescriptor.startTime),
            pTxCtrlBlk->aTxCtrlBlkTbl[entry].tTxDescriptor.tid,
			pTxCtrlBlk->aTxCtrlBlkTbl[entry].tTxDescriptor.extraMemBlks,
            pTxCtrlBlk->aTxCtrlBlkTbl[entry].tTxDescriptor.totalMemBlks,
            pTxCtrlBlk->aTxCtrlBlkTbl[entry].tTxPktParams.uFlags));
	}
#endif
}
#endif /* TI_DBG */

