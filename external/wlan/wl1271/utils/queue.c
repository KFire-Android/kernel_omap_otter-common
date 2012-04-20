/*
 * queue.c
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



/** \file   queue.c 
 *  \brief  This module provides generic queueing services, including enqueue, dequeue
 *            and requeue of any object that contains TQueNodeHdr in its structure.                                  
 *
 *  \see    queue.h
 */



#define __FILE_ID__  FILE_ID_130
#include "report.h"
#include "queue.h"


/* Queue structure */
typedef struct 
{
    TQueNodeHdr tHead;          /* The queue first node */
    TI_UINT32   uCount;         /* Current number of nodes in queue */
    TI_UINT32   uLimit;         /* Upper limit of nodes in queue */
    TI_UINT32   uMaxCount;      /* Maximum uCount value (for debug) */
    TI_UINT32   uOverflow;      /* Number of overflow occurences - couldn't insert node (for debug) */
    TI_UINT32   uNodeHeaderOffset; /* Offset of NodeHeader field from the entry of the queued item */
	TI_HANDLE   hOs;
	TI_HANDLE   hReport; 
} TQueue;	



/*
 *              INTERNAL  FUNCTIONS 
 *        =============================== 
 */


/*
 *  InsertNode():  Insert new node between pPrev and pNext 
 */
static INLINE void InsertNode( TQueNodeHdr *pNode, TQueNodeHdr *pPrev, TQueNodeHdr *pNext)
{
	pNext->pPrev = pNode;
	pNode->pNext = pNext;
	pNode->pPrev = pPrev;
	pPrev->pNext = pNode;
}

/*
 *  RemoveNode():  Remove node from between pPrev and pNext 
 */
static INLINE void RemoveNode( TQueNodeHdr *pPrev, TQueNodeHdr *pNext)
{
	pNext->pPrev = pPrev;
	pPrev->pNext = pNext;
}

/*
 *  AddToHead():  Add node to queue head (last in queue) 
 */
static INLINE void AddToHead( TQueNodeHdr *pNode, TQueNodeHdr *pListHead)
{
	InsertNode (pNode, pListHead, pListHead->pNext);
}

/*
 *  AddToTail():  Add node to queue tail (first in queue)
 */
static INLINE void AddToTail( TQueNodeHdr *pNode, TQueNodeHdr *pListHead)
{
	InsertNode( pNode, pListHead->pPrev, pListHead );
}

/*
 *  DelFromTail():  Delete node from queue tail (first in queue)
 */
static INLINE void DelFromTail (TQueNodeHdr *pNode)
{
	RemoveNode (pNode->pPrev, pNode->pNext);
}



/*
 *              EXTERNAL  FUNCTIONS 
 *        =============================== 
 */


/** 
 * \fn     que_Create 
 * \brief  Create a queue. 
 * 
 * Allocate and init a queue object.
 * 
 * \note    
 * \param  hOs               - Handle to Os Abstraction Layer
 * \param  hReport           - Handle to report module
 * \param  uLimit            - Maximum items to store in queue
 * \param  uNodeHeaderOffset - Offset of NodeHeader field from the entry of the queued item.
 * \return Handle to the allocated queue 
 * \sa     que_Destroy
 */ 
TI_HANDLE que_Create (TI_HANDLE hOs, TI_HANDLE hReport, TI_UINT32 uLimit, TI_UINT32 uNodeHeaderOffset)
{
	TQueue *pQue;

	/* allocate queue module */
	pQue = os_memoryAlloc (hOs, sizeof(TQueue));
	
	if (!pQue)
	{
		WLAN_OS_REPORT (("Error allocating the Queue Module\n"));
		return NULL;
	}
	
    os_memoryZero (hOs, pQue, sizeof(TQueue));

	/* Intialize the queue header */
    pQue->tHead.pNext = pQue->tHead.pPrev = &pQue->tHead;

	/* Set the Queue parameters */
    pQue->hOs               = hOs;
    pQue->hReport           = hReport;
	pQue->uLimit            = uLimit;
	pQue->uNodeHeaderOffset = uNodeHeaderOffset;

	return (TI_HANDLE)pQue;
}


/** 
 * \fn     que_Destroy
 * \brief  Destroy the queue. 
 * 
 * Free the queue memory.
 * 
 * \note   The queue's owner should first free the queued items!
 * \param  hQue - The queue object
 * \return TI_OK on success or TI_NOK on failure 
 * \sa     que_Create
 */ 
TI_STATUS que_Destroy (TI_HANDLE hQue)
{
    TQueue *pQue = (TQueue *)hQue;

    if (pQue)
	{
        /* Alert if the queue is unloaded before it was cleared from items */
        if (pQue->uCount)
        {
            TRACE0(pQue->hReport, REPORT_SEVERITY_WARNING, "que_Destroy() Queue Not Empty!!");
        }
        /* free Queue object */
        os_memoryFree (pQue->hOs, pQue, sizeof(TQueue));
    }
	
    return TI_OK;
}


/** 
 * \fn     que_Init 
 * \brief  Init required handles 
 * 
 * Init required handles.
 * 
 * \note    
 * \param  hQue      - The queue object
 * \param  hOs       - Handle to Os Abstraction Layer
 * \param  hReport   - Handle to report module
 * \return TI_OK on success or TI_NOK on failure  
 * \sa     
 */ 
TI_STATUS que_Init (TI_HANDLE hQue, TI_HANDLE hOs, TI_HANDLE hReport)
{
	TQueue *pQue = (TQueue *)hQue;

    pQue->hOs = hOs;
    pQue->hReport = hReport;
	
	return TI_OK;
}


/** 
 * \fn     que_Enqueue
 * \brief  Enqueue an item 
 * 
 * Enqueue an item at the queue's head (last in queue).
 * 
 * \note   
 * \param  hQue   - The queue object
 * \param  hItem  - Handle to queued item
 * \return TI_OK if item was queued, or TI_NOK if not queued due to overflow
 * \sa     que_Dequeue, que_Requeue
 */ 
TI_STATUS que_Enqueue (TI_HANDLE hQue, TI_HANDLE hItem)
{
	TQueue      *pQue = (TQueue *)hQue;
    TQueNodeHdr *pQueNodeHdr;  /* the Node-Header in the given item */

    if (pQue)
	{
            /* Check queue limit */
            if(pQue->uCount < pQue->uLimit)
            {
                /* Find NodeHeader in the given item */
                pQueNodeHdr = (TQueNodeHdr *)((TI_UINT8*)hItem + pQue->uNodeHeaderOffset);
        
                /* Verify that pNext is NULL --> Sanity check that this item is not already linked to a queue */
                if (pQueNodeHdr->pNext)
                {
                    /* Not an error since we have a case where a timer may expire twice in a row (in TxDataQueue) */
                    TRACE0(pQue->hReport, REPORT_SEVERITY_WARNING, "que_Enqueue(): Trying to enqueue an item that is already enqueued!!");
                    return TI_NOK;
                }
        
                /* Enqueue item and increment items counter */
                AddToHead (pQueNodeHdr, &pQue->tHead);
                pQue->uCount++;
        
#ifdef TI_DBG
                    if (pQue->uCount > pQue->uMaxCount)
                    {
                        pQue->uMaxCount = pQue->uCount;
                    }
                    TRACE0(pQue->hReport, REPORT_SEVERITY_INFORMATION , "que_Enqueue(): Enqueued Successfully\n");
#endif
        
                return TI_OK;
            }

        /* 
         *  Queue is overflowed, return TI_NOK.
         */
#ifdef TI_DBG
        pQue->uOverflow++;
        TRACE0(pQue->hReport, REPORT_SEVERITY_WARNING , "que_Enqueue(): Queue Overflow\n");
#endif
    }
	return TI_NOK;
}


/** 
 * \fn     que_Dequeue
 * \brief  Dequeue an item 
 * 
 * Dequeue an item from the queue's tail (first in queue).
 * 
 * \note   
 * \param  hQue - The queue object
 * \return pointer to dequeued item or NULL if queue is empty
 * \sa     que_Enqueue, que_Requeue
 */ 
TI_HANDLE que_Dequeue (TI_HANDLE hQue)
{
    TQueue   *pQue = (TQueue *)hQue;
	TI_HANDLE hItem;

    if (pQue)
    {
        if (pQue->uCount)
        {
            /* Queue is not empty, take packet from the queue tail */
    
            /* find pointer to the node entry */
            hItem = (TI_HANDLE)((TI_UINT8*)pQue->tHead.pPrev - pQue->uNodeHeaderOffset); 

            DelFromTail (pQue->tHead.pPrev);    /* remove node from the queue */
            pQue->uCount--;

#ifdef TI_DBG
            /* Clear the pNext so we can do a sanity check when enqueuing this structre in the future */
            ((TQueNodeHdr *)((TI_UINT8*)hItem + pQue->uNodeHeaderOffset))->pNext = NULL;
#endif

            return (hItem);
        }
    }
    
	/* Queue is empty */
    TRACE0(pQue->hReport, REPORT_SEVERITY_INFORMATION , "que_Dequeue(): Queue is empty\n");
    return NULL;
}


/** 
 * \fn     que_Requeue
 * \brief  Requeue an item 
 * 
 * Requeue an item at the queue's tail (first in queue).
 * 
 * \note   
 * \param  hQue   - The queue object
 * \param  hItem  - Handle to queued item
 * \return TI_OK if item was queued, or TI_NOK if not queued due to overflow
 * \sa     que_Enqueue, que_Dequeue
 */ 
TI_STATUS que_Requeue (TI_HANDLE hQue, TI_HANDLE hItem)
{
    TQueue      *pQue = (TQueue *)hQue;
    TQueNodeHdr *pQueNodeHdr;  /* the NodeHeader in the given item */

    /* 
	 *  If queue's limits not exceeded add the packet to queue's tail and return TI_OK 
	 */
    if (pQue->uCount < pQue->uLimit)
	{
        /* Find NodeHeader in the given item */
        pQueNodeHdr = (TQueNodeHdr *)((TI_UINT8*)hItem + pQue->uNodeHeaderOffset);

        /* Verify that pNext is NULL --> Sanity check that this item is not already linked to a queue */
        if (pQueNodeHdr->pNext)
        {
            TRACE0(pQue->hReport, REPORT_SEVERITY_ERROR, "que_Requeue(): Trying to Requeue an item that is already enqueued!!");
            return TI_NOK;
        }

        /* Enqueue item and increment items counter */
		AddToTail (pQueNodeHdr, &pQue->tHead);
		pQue->uCount++;

#ifdef TI_DBG
		if (pQue->uCount > pQue->uMaxCount)
			pQue->uMaxCount = pQue->uCount;
TRACE0(pQue->hReport, REPORT_SEVERITY_INFORMATION , "que_Requeue(): Requeued successfully\n");
#endif

		return TI_OK;
    }
    

	/* 
	 *  Queue is overflowed, return TI_NOK.
	 *  Note: This is not expected in the current design, since Tx packet may be requeued
	 *          only right after it was dequeued in the same context so the queue can't be full.
	 */
#ifdef TI_DBG
    pQue->uOverflow++;
TRACE0(pQue->hReport, REPORT_SEVERITY_ERROR , "que_Requeue(): Queue Overflow\n");
#endif
    
    return TI_NOK;
}


/** 
 * \fn     que_Size
 * \brief  Return queue size 
 * 
 * Return number of items in queue.
 * 
 * \note   
 * \param  hQue - The queue object
 * \return TI_UINT32 - the items count
 * \sa     
 */ 
TI_UINT32 que_Size (TI_HANDLE hQue)
{
    TQueue *pQue = (TQueue *)hQue;
 
    return (pQue->uCount);
}

	
/** 
 * \fn     que_Print
 * \brief  Print queue status
 * 
 * Print the queue's parameters (not the content).
 * 
 * \note   
 * \param  hQue - The queue object
 * \return void
 * \sa     
 */ 

#ifdef TI_DBG

void que_Print(TI_HANDLE hQue)
{
#ifdef REPORT_LOG
    TQueue *pQue = (TQueue *)hQue;

    WLAN_OS_REPORT(("que_Print: Count=%u MaxCount=%u Limit=%u Overflow=%u NodeHeaderOffset=%u Next=0x%x Prev=0x%x\n",
                    pQue->uCount, pQue->uMaxCount, pQue->uLimit, pQue->uOverflow, 
                    pQue->uNodeHeaderOffset, pQue->tHead.pNext, pQue->tHead.pPrev));
#endif
}

#endif /* TI_DBG */
