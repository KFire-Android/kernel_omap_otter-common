/*
 * GeneralUtil.c
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

#define __FILE_ID__  FILE_ID_52
#include "GeneralUtilApi.h"
#include "GeneralUtil.h"
#include "report.h"
#include "osApi.h"

/*************************************************************************
*                        LIST OBJ                                        *
**************************************************************************
**************************************************************************
*
 *	The list object mange the allocation and deallocation of generic element.
 *  The obj create a list of N generic elements and fined a free entry for the Alloc process.
 *  And free the entry for dealloc.
 *
 *
***************************************************************************/



/*************************************************************************
*                        List_create                                     *
**************************************************************************
* DESCRIPTION:	This function initializes the List data module.                 
*                                                      
* INPUT:		hOs - handle to Os Abstraction Layer
*				MaxNumOfElements - the number of elemnts that will be Managed by the list
                ContainerSize - The size of the basic data type managed by the list
* OUTPUT:		
*				
*
* RETURN:		Handle to the allocated List data control block
************************************************************************/
TI_HANDLE List_create(TI_HANDLE hOs,int MaxNumOfElements,int ContainerSize)
{
    int index;
    List_t *List;

	if( hOs  == NULL )
	{
	    WLAN_OS_REPORT(("FATAL ERROR:List_create(): OS handle Error - Aborting\n"));
		return NULL;
	}

	/* alocate List block */
	List = (List_t*)os_memoryAlloc(hOs, sizeof(List_t));
	if(List == NULL)
	    return NULL;

    
    /* alocate the List of Elements */
    List->ElementList =(ListElement_t*)os_memoryAlloc(hOs, (sizeof(ListElement_t)*MaxNumOfElements));
	if(List->ElementList == NULL)
    {
        os_memoryFree(List->hOs, List, sizeof(List_t));
        return NULL;
    }
    
    /*Allocate the Data containers*/
    for(index=0;index<MaxNumOfElements;index++)
    {
        List->ElementList[index].Container = os_memoryAlloc(hOs,ContainerSize);
        if(List->ElementList[index].Container == NULL)
            break;
        List->ElementList[index].Inuse = TI_FALSE;
    }
    if (index != MaxNumOfElements)  /*Not all the list element was allocated and*/
    {                                /*therefore we free the entire list and rap it up*/
       index--;
       for(;index>=0;index--)
            os_memoryFree(hOs,List->ElementList[index].Container,ContainerSize);
       os_memoryFree(List->hOs, List->ElementList, (sizeof(ListElement_t)*MaxNumOfElements));
       os_memoryFree(List->hOs,List,(sizeof(List_t)));
       return NULL;
   
    }
   
    List->MaxNumOfElements = MaxNumOfElements;
    List->ContainerSize = ContainerSize;
	return((TI_HANDLE)List);
}


/***************************************************************************
*							List_Destroy				                   *
****************************************************************************
* DESCRIPTION:	This function unload the List data module. 
* 
* INPUTS:		hCtrlData - the object
*		
* OUTPUT:		
* 
* RETURNS:		TI_OK - Unload succesfull
*				TI_NOK - Unload unsuccesfull
***************************************************************************/
TI_STATUS List_Destroy(TI_HANDLE hList)	
{
    List_t* List = (List_t*)hList;
    int index;
    
    if(List!=NULL)
    {
  	    if(List->ElementList != NULL)
        {
           for(index=0;index<List->MaxNumOfElements;index++)
             os_memoryFree(List->hOs,List->ElementList[index].Container,List->ContainerSize);

           os_memoryFree(List->hOs,List->ElementList,(sizeof(ListElement_t)*List->MaxNumOfElements));
        }
        os_memoryFree(List->hOs, List, sizeof(List_t));
    }
    return TI_OK;
}

/***************************************************************************
*							List_AllocElement   		                   *
****************************************************************************
*   
* 
*    Fined an empty entry in the list and returns 
*    a pointer to a memory that contains an element that not in use.
*
*    Note in multi Task environment we need to add semaphore to protect the
*    Function.   
* 
***************************************************************************/
TI_HANDLE List_AllocElement(TI_HANDLE hList)
{
    List_t* List = (List_t*)hList;
    int index;
    
    if (List == NULL)
        return NULL;

    for(index=0;index<List->MaxNumOfElements;index++)
    {
        if(!(List->ElementList[index].Inuse))
        {
           List->ElementList[index].Inuse = TI_TRUE;
           os_memoryZero(List->hOs,List->ElementList[index].Container,List->ContainerSize);
           return((TI_HANDLE)List->ElementList[index].Container);
        }
    }
    return NULL;  
}


/***************************************************************************
*							List_FreeElement				               *
****************************************************************************
*   
*   Marks the entry that was allocated as free.
*   An alloc process can use this space.
*
* 
*
***************************************************************************/
TI_STATUS List_FreeElement(TI_HANDLE hList,TI_HANDLE Container)
{
    List_t* List = (List_t*)hList;
    int index;

    if (List == NULL)
        return TI_NOK;

    for(index=0;index<List->MaxNumOfElements;index++)
    {
        if(List->ElementList[index].Container == Container) 
        {
            if(!List->ElementList[index].Inuse)
                return TI_NOK;  /*double free not legal*/
            List->ElementList[index].Inuse = TI_FALSE; 
            return TI_OK;
        }
    }
    return TI_NOK;
}


/***************************************************************************
*							List_GetFirst				                   *
****************************************************************************
*   
*  For purposes of searching the element list (going over all the element in the list)
*  Get first is used to reset an index for the search.
*  This function is work combined with GetNext. 
* 
*   Note this function can't be used in multi Task environment.
*
***************************************************************************/
TI_HANDLE List_GetFirst(TI_HANDLE hList)
{
    List_t* List = (List_t*)hList;
    int index;

    if (List == NULL)
        return NULL;

    for(index=0;index<List->MaxNumOfElements;index++)
    {
        if(List->ElementList[index].Inuse) 
        {
           List->CurrentIndex = index;
           return (List->ElementList[index].Container); 
        }
    }
    return NULL;
}


/***************************************************************************
*							List_GetNext				                   *
****************************************************************************
*   
*  This function returns the next element in the list till null
*  that indicate that there no more element or we have reached the end of the list.
*  This function is work combined with GetFirst. 
* 
*  Note this function can't be used in multi Task environment.
*
***************************************************************************/
TI_HANDLE List_GetNext(TI_HANDLE hList)
{
    List_t* List = (List_t*)hList;
    int index;

    if (List == NULL)
        return NULL;
    
    /* the code works fine even if the elment is the last*/
    for(index=List->CurrentIndex+1;index<List->MaxNumOfElements;index++)
    {
        if(List->ElementList[index].Inuse) 
        {
           List->CurrentIndex = index;
           return (List->ElementList[index].Container); 

        }
    }
    return NULL;
}




/***************************************************************************
*							DISTRIBUTOR MANAGER				               *
****************************************************************************
***************************************************************************
*
 *    	PURPOSE:The distributor manger supplies
 *       1. Register mechanism that has a callback function and the condition 
 *       (bit mask format) that will be used to distinguish if to call this callback. 
 *       2. Event occurrence function that go over all the registered function and compare 
 *       the input mask to the callback mask condition. 
 *
 *	
 *
***************************************************************************/



/***************************************************************************
*							DistributorMgr_Create		                   *
****************************************************************************
*
***************************************************************************/
TI_HANDLE DistributorMgr_Create(TI_HANDLE hOs , int MaxNotifReqElment)
{
    DistributorMgr_t *DistributorMgr;
    
	DistributorMgr = (DistributorMgr_t*)os_memoryAlloc(hOs, sizeof(DistributorMgr_t));
	if(DistributorMgr == NULL)
        return NULL;
    DistributorMgr->hOs = hOs;
    DistributorMgr->DistributionList = (List_t*)List_create(hOs,MaxNotifReqElment,sizeof(NotifReqElment_t));
    if (DistributorMgr->DistributionList == NULL)
    {
        os_memoryFree(hOs, DistributorMgr, sizeof(DistributorMgr_t));
        return NULL;
    } 
    return (TI_HANDLE)DistributorMgr;
}



/************************************************************************/
/*               DistributorMgr_Destroy                                 */
/************************************************************************/
TI_STATUS DistributorMgr_Destroy(TI_HANDLE hDistributorMgr)
{
    DistributorMgr_t *DistributorMgr =(DistributorMgr_t*)hDistributorMgr;
    
     if(DistributorMgr == NULL)
        return TI_NOK;

    List_Destroy(DistributorMgr->DistributionList);
    
    os_memoryFree(DistributorMgr->hOs, hDistributorMgr, sizeof(DistributorMgr_t));
    
    return TI_OK;

}

/***************************************************************************
*						DistributorMgr_Reg				                   *
****************************************************************************
*   
*  Use by the client to register a callback function 
*  with the mask condition that will trigger the call.
* 
* input
*    TI_UINT16 Mask 
*    TI_HANDLE CallBack
*    HANDLE Context 
*    TI_UINT32 Cookie
* 
*
***************************************************************************/
TI_HANDLE DistributorMgr_Reg(TI_HANDLE hDistributorMgr,TI_UINT16 Mask,TI_HANDLE CallBack,
                             TI_HANDLE Context,TI_UINT32 Cookie)
{
    DistributorMgr_t *DistributorMgr = (DistributorMgr_t*)hDistributorMgr;
    NotifReqElment_t *NotifReqElment;
        
   	if(DistributorMgr == NULL)
        return NULL;

    NotifReqElment = (NotifReqElment_t*)List_AllocElement(DistributorMgr->DistributionList);
    if (NotifReqElment == NULL) 
		return NULL  ;

    NotifReqElment->CallBack = (GeneralEventCall_t)CallBack;
    NotifReqElment->Mask = Mask;
    NotifReqElment->Context = Context;
    NotifReqElment->Cookie = Cookie;
    NotifReqElment->HaltReq = TI_FALSE;
    return (TI_HANDLE)NotifReqElment;
}


/***************************************************************************
*						DistributorMgr_ReReg				               *
****************************************************************************
*
***************************************************************************/
TI_STATUS DistributorMgr_ReReg(TI_HANDLE hDistributorMgr,TI_HANDLE ReqElmenth ,TI_UINT16 Mask,TI_HANDLE CallBack,TI_HANDLE Context,TI_UINT32 Cookie)
{
    DistributorMgr_t *DistributorMgr = (DistributorMgr_t*)hDistributorMgr;
    NotifReqElment_t *NotifReqElment = (NotifReqElment_t*)ReqElmenth;
    
   	if(DistributorMgr == NULL)
        return TI_NOK;

    if (NotifReqElment == NULL) 
		return TI_NOK;

    NotifReqElment->CallBack = (GeneralEventCall_t)CallBack;
    NotifReqElment->Mask = Mask;
    NotifReqElment->Context = Context;
    NotifReqElment->Cookie = Cookie;
    return TI_OK;
}


/***************************************************************************
*						DistributorMgr_AddToMask			               *
****************************************************************************
*   
* Use this function to add mask bit to the bit mask condition that triggers the Callback 
*
*
***************************************************************************/
TI_STATUS DistributorMgr_AddToMask(TI_HANDLE hDistributorMgr,TI_HANDLE ReqElmenth,TI_UINT16 Mask)
{
    DistributorMgr_t *DistributorMgr = (DistributorMgr_t*)hDistributorMgr;
    NotifReqElment_t *NotifReqElment = (NotifReqElment_t*)ReqElmenth;
     
    if(DistributorMgr == NULL)
        return TI_NOK;

    if (NotifReqElment == NULL) 
		return TI_NOK;

    NotifReqElment->Mask |= Mask;
    return TI_OK;
}


/***************************************************************************
*						DistributorMgr_HaltNotif			               *
****************************************************************************
*   
* Use this function to add mask bit to the bit mask condition that triggers the Callback 
*
*
***************************************************************************/
void DistributorMgr_HaltNotif(TI_HANDLE ReqElmenth)
{
    NotifReqElment_t *NotifReqElment = (NotifReqElment_t*)ReqElmenth;
       
    if (NotifReqElment == NULL) 
	    return;

    NotifReqElment->HaltReq = TI_TRUE;
   
}


/***************************************************************************
*						DistributorMgr_RestartNotif			               *
****************************************************************************
*   
* Use this function to add mask bit to the bit mask condition that triggers the Callback 
*
*
***************************************************************************/
void DistributorMgr_RestartNotif(TI_HANDLE ReqElmenth)
{
    NotifReqElment_t *NotifReqElment = (NotifReqElment_t*)ReqElmenth;
       
    if (NotifReqElment == NULL) 
	    return;

    NotifReqElment->HaltReq = TI_FALSE;
   
}
/***************************************************************************
*						DistributorMgr_UnReg				               *
****************************************************************************
* 
*
***************************************************************************/
TI_STATUS DistributorMgr_UnReg(TI_HANDLE hDistributorMgr,TI_HANDLE RegEventHandle)
{
    DistributorMgr_t *DistributorMgr = (DistributorMgr_t*)hDistributorMgr;

  	if(DistributorMgr == NULL)
        return TI_NOK;

    return List_FreeElement(DistributorMgr->DistributionList, RegEventHandle);
}


/***************************************************************************
*						DistributorMgr_EventCall		                   *
****************************************************************************
*   
* When the client needs to invoke the callback calls function that corresponds 
* to a specific event mask it will call this function with the desired mask. 
* And event count that can be used to aggregate the events. 
* that way calling this function not for every event
*
***************************************************************************/
void DistributorMgr_EventCall(TI_HANDLE hDistributorMgr,TI_UINT16 Mask,int EventCount)
{
    DistributorMgr_t *DistributorMgr = (DistributorMgr_t*)hDistributorMgr;
    NotifReqElment_t *NotifReqElment;

    if(DistributorMgr == NULL)
        return;
 
    NotifReqElment = (NotifReqElment_t*)List_GetFirst(DistributorMgr->DistributionList);
    
    while(NotifReqElment)
    {
        if((NotifReqElment->Mask & Mask) && !(NotifReqElment->HaltReq))
        {
            NotifReqElment->CallBack(NotifReqElment->Context,EventCount,Mask,
                                     NotifReqElment->Cookie);
        }
        NotifReqElment = (NotifReqElment_t*)List_GetNext(DistributorMgr->DistributionList);
    }
}



/*******************************************************/
