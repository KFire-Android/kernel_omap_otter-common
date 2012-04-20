/*
 * TrafficMonitor.c
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

/***************************************************************************/
/*                                                                         */
/*              MODULE:         TrafficMonitor.c                           */
/*              PURPOSE:        Traffic Monitor                            */
/*                                                                         */
/***************************************************************************/
#define __FILE_ID__  FILE_ID_55
#include "TrafficMonitorAPI.h"
#include "TrafficMonitor.h"
#include "DataCtrl_Api.h"
#include "osApi.h"
#include "report.h"
#include "timer.h"
#include "DrvMainModules.h"


/* Percentage of max down events test interval to use in our "traffic down" timer */ 
#define MIN_INTERVAL_PERCENT 50

/*#define TRAFF_TEST*/
#ifdef TRAFF_TEST
/*for TEST Function*/
TI_HANDLE TestTrafficMonitor;
TI_HANDLE TestEventTimer;
TI_HANDLE Alert1;
TI_HANDLE Alert2;
TI_HANDLE Alert3;
TI_HANDLE Alert4;
void PrintElertStus();
void TestEventFunc (TI_HANDLE hTrafficMonitor, TI_BOOL bTwdInitOccured);
#endif


/************************************************************************/
/*           Function prototype                                         */
/************************************************************************/
static void TimerMonitor_TimeOut (TI_HANDLE hTrafficMonitor, TI_BOOL bTwdInitOccured);
static void TrafficMonitor_updateBW(BandWidth_t *pBandWidth, TI_UINT32 uCurrentTS);
static TI_UINT32 TrafficMonitor_calcBW(BandWidth_t *pBandWidth, TI_UINT32 uCurrentTS);
static TI_BOOL isThresholdDown(TrafficAlertElement_t *AlertElement,TI_UINT32 CurrentTime);
static TI_BOOL isThresholdUp(TrafficAlertElement_t *AlertElement , TI_UINT32 CurrentTime);
static void SimpleByteAggregation(TI_HANDLE TraffElem,int Count);
static void SimpleFrameAggregation(TI_HANDLE TraffElem,int Count);
static TI_HANDLE TrafficMonitor_ExitFunc(TrafficMonitor_t *TrafficMonitor,TI_HANDLE hOs);
static TI_STATUS FindRstElemEntryIndex (TrafficMonitor_t *TrafficMonitor,TrafficAlertElement_t  *TrafficAlertElement,int *Index);
static TI_STATUS TrafficMonitor_SetMask(TrafficMonitor_t *TrafficMonitor,TrafficAlertElement_t *TrafficAlertElement,TraffEvntOptNum_t MaskType);

static void TrafficMonitor_UpdateDownTrafficTimerState (TI_HANDLE hTrafficMonitor);
static void TrafficMonitor_ChangeDownTimerStatus (TI_HANDLE hTrafficMonitor, TI_UINT32 downEventsFound, TI_UINT32 minIntervalTime);

/************************************************************************/
/*                      TrafficMonitor_create                           */
/************************************************************************/
TI_HANDLE TrafficMonitor_create(TI_HANDLE hOs)
{
    TrafficMonitor_t *TrafficMonitor;
    
    /* Allocate the data structure TrafficMonitor*/     
        TrafficMonitor = (TrafficMonitor_t*)os_memoryAlloc(hOs, sizeof(TrafficMonitor_t));
        if (TrafficMonitor == NULL)
        return NULL;

    os_memoryZero(hOs,TrafficMonitor,sizeof(TrafficMonitor_t));

#ifdef TRAFF_TEST
	TestEventTimer = NULL;
#endif

    TrafficMonitor->hOs = hOs;
    
    /*Creates the list that will hold all the registered alert requests*/
    TrafficMonitor->NotificationRegList = List_create(hOs,MAX_MONITORED_REQ,sizeof(TrafficAlertElement_t));
    if (TrafficMonitor->NotificationRegList == NULL)
        return TrafficMonitor_ExitFunc(TrafficMonitor,hOs);
 
    return (TI_HANDLE)TrafficMonitor;
}


/************************************************************************/
/*                    TrafficMonitor_ExitFunc                           */
/************************************************************************/
static TI_HANDLE TrafficMonitor_ExitFunc(TrafficMonitor_t *TrafficMonitor,TI_HANDLE hOs)
{
    if (TrafficMonitor)
    {
        if(TrafficMonitor->hTrafficMonTimer)
        {
            tmr_DestroyTimer (TrafficMonitor->hTrafficMonTimer);
        }
        os_memoryFree(hOs, TrafficMonitor, sizeof(TrafficMonitor_t));            
    }
    return NULL;
}



/************************************************************************/
/*                    TrafficMonitor_config                             */
/************************************************************************/
void TrafficMonitor_Init (TStadHandlesList *pStadHandles, TI_UINT32 BWwindowMs)
{
    TrafficMonitor_t *TrafficMonitor = (TrafficMonitor_t *)(pStadHandles->hTrafficMon);
    TI_UINT32 uCurrTS = os_timeStampMs (TrafficMonitor->hOs);
        
    /* Create the base threshold timer that will serve all the down thresholds*/
    TrafficMonitor->hTrafficMonTimer = tmr_CreateTimer (pStadHandles->hTimer);

    TrafficMonitor->Active = TI_FALSE;

    TrafficMonitor->hRxData = pStadHandles->hRxData;
    TrafficMonitor->hTxCtrl = pStadHandles->hTxCtrl;
    TrafficMonitor->hTimer  = pStadHandles->hTimer;

        /*Init All the bandwidth elements in the system */
	os_memoryZero(TrafficMonitor->hOs,&TrafficMonitor->DirectTxFrameBW,sizeof(BandWidth_t));
	os_memoryZero(TrafficMonitor->hOs,&TrafficMonitor->DirectRxFrameBW,sizeof(BandWidth_t));
	TrafficMonitor->DirectRxFrameBW.auFirstEventsTS[0] = uCurrTS;
	TrafficMonitor->DirectTxFrameBW.auFirstEventsTS[0] = uCurrTS;
    
    /*Registering to the RX module for notification.*/
    TrafficMonitor->RxRegReqHandle = rxData_RegNotif (pStadHandles->hRxData, 
                                                      DIRECTED_FRAMES_RECV,
                                                      TrafficMonitor_Event,
                                                      TrafficMonitor,
                                                      RX_TRAFF_MODULE);

    /*Registering to the TX module for notification .*/
    TrafficMonitor->TxRegReqHandle = txCtrlParams_RegNotif (pStadHandles->hTxCtrl, 
                                                            DIRECTED_FRAMES_XFER,
                                                            TrafficMonitor_Event,
                                                            TrafficMonitor,
                                                            TX_TRAFF_MODULE);

    TrafficMonitor->DownTimerEnabled = TI_FALSE;
    TrafficMonitor->trafficDownTestIntervalPercent = MIN_INTERVAL_PERCENT;

#ifdef TRAFF_TEST
    TestTrafficMonitor = TrafficMonitor;
    TestEventTimer = tmr_CreateTimer (pStadHandles->hTimer);
    tmr_StartTimer (TestEventTimer, TestEventFunc, (TI_HANDLE)TrafficMonitor, 5000, TI_TRUE);
#endif
}

/************************************************************************/
/*                TrafficMonitor_Start                                  */
/************************************************************************/
TI_STATUS TrafficMonitor_Start(TI_HANDLE hTrafficMonitor)       
{
    TrafficMonitor_t *TrafficMonitor =(TrafficMonitor_t*)hTrafficMonitor;
    TrafficAlertElement_t *AlertElement;
    TI_UINT32 CurentTime;
  

    if(TrafficMonitor == NULL)
        return TI_NOK;

    /*starts the bandwidth TIMER*/
    if(!TrafficMonitor->Active) /*To prevent double call to timer start*/
    {
        TrafficMonitor_UpdateDownTrafficTimerState (TrafficMonitor);	
    }
   
    AlertElement  = (TrafficAlertElement_t*)List_GetFirst(TrafficMonitor->NotificationRegList);
    CurentTime = os_timeStampMs(TrafficMonitor->hOs);
   
    /* go over all the Down elements and reload the timer*/    
    while(AlertElement)
    {
        if(AlertElement->CurrentState != ALERT_WAIT_FOR_RESET) 
        {
            AlertElement->EventCounter = 0;
            AlertElement->TimeOut = AlertElement->TimeIntervalMs + CurentTime;
        }
        AlertElement = (TrafficAlertElement_t*)List_GetNext(TrafficMonitor->NotificationRegList);
    }
    TrafficMonitor->Active = TI_TRUE;

    return TI_OK;
}



/************************************************************************/
/*              TrafficMonitor_Stop                                     */
/************************************************************************/
TI_STATUS TrafficMonitor_Stop(TI_HANDLE hTrafficMonitor)        
{
    TrafficMonitor_t       *pTrafficMonitor = (TrafficMonitor_t*)hTrafficMonitor;
    TrafficAlertElement_t  *AlertElement;
       
    if (pTrafficMonitor == NULL)
    {
        return TI_NOK;
    }
    
    if (pTrafficMonitor->Active) /*To prevent double call to timer stop*/
    {
    
        pTrafficMonitor->Active = TI_FALSE;  
   
        pTrafficMonitor->DownTimerEnabled = TI_FALSE;
        tmr_StopTimer (pTrafficMonitor->hTrafficMonTimer);
    }  

    /* Set all events state to ALERT_OFF to enable them to "kick" again once after TrafficMonitor is started */
    AlertElement = (TrafficAlertElement_t*)List_GetFirst(pTrafficMonitor->NotificationRegList);
    
    while(AlertElement)
    {
        AlertElement->CurrentState = ALERT_OFF;
        AlertElement = (TrafficAlertElement_t*)List_GetNext(pTrafficMonitor->NotificationRegList);
    }  
    
    return TI_OK;
}



/************************************************************************/
/*                  TrafficMonitor_Destroy                              */
/************************************************************************/
TI_STATUS TrafficMonitor_Destroy(TI_HANDLE hTrafficMonitor)     
{
    TrafficMonitor_t *TrafficMonitor = (TrafficMonitor_t*)hTrafficMonitor;

    if (TrafficMonitor)
    {
        /*Unregister from the RX/TX module for the required notification*/
        txCtrlParams_UnRegNotif(TrafficMonitor->hTxCtrl,TrafficMonitor->TxRegReqHandle);
        rxData_UnRegNotif(TrafficMonitor->hRxData,TrafficMonitor->RxRegReqHandle);

        if(TrafficMonitor->NotificationRegList)
        {
            List_Destroy(TrafficMonitor->NotificationRegList);
        }

        if(TrafficMonitor->hTrafficMonTimer)
        {
            tmr_DestroyTimer (TrafficMonitor->hTrafficMonTimer);
        }
        
#ifdef TRAFF_TEST
		if (TestEventTimer)
		{
			tmr_DestroyTimer (TestEventTimer);
		}
#endif    
    
        os_memoryFree(TrafficMonitor->hOs, TrafficMonitor, sizeof(TrafficMonitor_t)); 

        return TI_OK;
    }
    
    return TI_NOK;
}


/***********************************************************************
 *                        TrafficMonitor_RegEvent                               
 ***********************************************************************
DESCRIPTION: Reg event processing function, Perform the following:

                                
INPUT:          hTrafficMonitor -       Traffic Monitor the object.
                        
            TrafficAlertRegParm -       structure which include values to set for 
                                                the requested Alert event
                        
            AutoResetCreate - is only relevant to edge alerts. 
                  If AutoResetCreate flag is set to true then the registration function will create a conjunction reset element automatic
                 this reset element will be with the same threshold but opposite in direction
 
                 If AutoResetCreate flag is set to false then the reset element will be supplied afterward by the user with the function
                 TrafficMonitor_SetRstCondition() the alert will not be active till the reset function will be set.

OUTPUT:         

RETURN:     TrafficAlertElement pointer on success, NULL otherwise

************************************************************************/
TI_HANDLE TrafficMonitor_RegEvent(TI_HANDLE hTrafficMonitor,TrafficAlertRegParm_t *TrafficAlertRegParm,TI_BOOL AutoResetCreate)
{
    TrafficMonitor_t *TrafficMonitor =(TrafficMonitor_t*)hTrafficMonitor;
    TrafficAlertElement_t  *TrafficAlertElement;
    TI_UINT32 CurentTime ;
   
    if(TrafficMonitor == NULL)
       return NULL;

    CurentTime = os_timeStampMs(TrafficMonitor->hOs);

    /*Gets a TrafficAlertElement_t memory from the list to assign to the registered request*/
    TrafficAlertElement = (TrafficAlertElement_t*)List_AllocElement(TrafficMonitor->NotificationRegList);
    if (TrafficAlertElement == NULL) 
    {   /* add print*/
                return NULL  ;
    }

    /*Init the alert element with the registered parameters.*/
    TrafficAlertElement->CallBack = TrafficAlertRegParm->CallBack;
    TrafficAlertElement->Context  = TrafficAlertRegParm->Context;
    TrafficAlertElement->Cookie  = TrafficAlertRegParm->Cookie;
    TrafficAlertElement->Direction = TrafficAlertRegParm->Direction;
    TrafficAlertElement->Threshold = TrafficAlertRegParm->Threshold;
    TrafficAlertElement->Trigger = TrafficAlertRegParm->Trigger;
    TrafficAlertElement->TimeIntervalMs = TrafficAlertRegParm->TimeIntervalMs;
    TrafficAlertElement->TimeOut = CurentTime + TrafficAlertRegParm->TimeIntervalMs;
    TrafficAlertElement->EventCounter = 0;
    TrafficMonitor_SetMask(TrafficMonitor,TrafficAlertElement,TrafficAlertRegParm->MonitorType);
    
    TrafficAlertElement->CurrentState = ALERT_OFF;
    TrafficAlertElement->AutoCreated = TI_FALSE;
    TrafficAlertElement->Enabled = TI_FALSE;
    /*In case that this is an Edge alert there is a need for a reset condition element*/
    /*corresponding to the Alert request but opposite in the direction.*/
    /*Note that the reset condition for this (new) reset element, is the Alert Element it self.*/
    if(TrafficAlertElement->Trigger == TRAFF_EDGE)
    {
        if(AutoResetCreate)
        {
            /*Gets a TrafficAlertElement_t memory from the list to assign to the reset elemnt*/
            TrafficAlertElement->ResetElment[0] = (TrafficAlertElement_t*)List_AllocElement(TrafficMonitor->NotificationRegList);
            if( TrafficAlertElement->ResetElment[0] == NULL)
            {
                List_FreeElement(TrafficMonitor->NotificationRegList,TrafficAlertElement);
                return NULL;
            }

            /*
             copy the Traffic Element init params to the reset Elemnt Except for
             the direction and the call back that is set to null the CurrentState set to disable.   
             And the reset condition,that points to the muster alert.
             */
            os_memoryCopy(TrafficMonitor->hOs,TrafficAlertElement->ResetElment[0],TrafficAlertElement,sizeof(TrafficAlertElement_t));
            TrafficAlertElement->ResetElment[0]->CallBack = NULL;
            /*opposite in the direction from the TrafficAlertElement->Direction*/
            if (TrafficAlertRegParm->Direction == TRAFF_UP)
                TrafficAlertElement->ResetElment[0]->Direction = TRAFF_DOWN;
            else
                TrafficAlertElement->ResetElment[0]->Direction = TRAFF_UP;
            TrafficAlertElement->ResetElment[0]->CurrentState = ALERT_WAIT_FOR_RESET;
            TrafficAlertElement->ResetElment[0]->ResetElment[0] = TrafficAlertElement;
            TrafficAlertElement->ResetElment[0]->AutoCreated = TI_TRUE;
 
            TrafficAlertElement->ResetElment[0]->RstWasAssigned = TI_TRUE;
            TrafficAlertElement->RstWasAssigned = TI_TRUE;
   
        }
        else/* The reset element will be supplied afterward by the user in the meanwhile disable the alert till then*/
        {
            TrafficAlertElement->RstWasAssigned = TI_FALSE;
            TrafficAlertElement->CurrentState = ALERT_WAIT_FOR_RESET;
        }
        
    }

    TrafficMonitor_UpdateDownTrafficTimerState (TrafficMonitor);

    return TrafficAlertElement;
}
 

/************************************************************************/
/*                  FindRstElemEntryIndex                               */
/************************************************************************/
/* Gets a TrafficAlertElement_t memory from the list to assign to the reset elemnt
 * for internal use
 ************************************************************************/
static TI_STATUS FindRstElemEntryIndex (TrafficMonitor_t *TrafficMonitor,TrafficAlertElement_t  *TrafficAlertElement,int *Index)
{
    int i;
    /*Find an empty Rst element entry*/
    for(i=0;(i<MAX_RST_ELMENT_PER_ALERT) && TrafficAlertElement->ResetElment[i];i++);
    if(i == MAX_RST_ELMENT_PER_ALERT)
        return TI_NOK;
    *Index  = i;
    return TI_OK;
}

/************************************************************************/
/*                  TrafficMonitor_SetMask                              */
/************************************************************************/
/*
 *      Convert the Mask from the types that declared in the 
 *  TrafficMonitorAPI to the types that are used in the Rx Tx modules.
 *  And update the TX and RX module of the new event req
 *  Sets the aggregation function that corresponds to the specific mask type
 ************************************************************************/
static TI_STATUS TrafficMonitor_SetMask(TrafficMonitor_t *TrafficMonitor,TrafficAlertElement_t *TrafficAlertElement,TraffEvntOptNum_t MaskType)
{
    TI_UINT32 TxMask = 0;
    TI_UINT32 RxMask = 0;
    
   switch(MaskType) {
   case TX_RX_DIRECTED_FRAMES:
        TxMask = DIRECTED_FRAMES_XFER;
        RxMask = DIRECTED_FRAMES_RECV;
        TrafficAlertElement->ActionFunc = SimpleFrameAggregation;
        break;
   case TX_ALL_MSDU_FRAMES:
        TxMask = DIRECTED_FRAMES_XFER|MULTICAST_FRAMES_XFER|BROADCAST_FRAMES_XFER;
        TrafficAlertElement->ActionFunc = SimpleFrameAggregation;
    break;
   case RX_ALL_MSDU_FRAMES:
        RxMask = DIRECTED_FRAMES_RECV|MULTICAST_FRAMES_RECV|BROADCAST_FRAMES_RECV;
        TrafficAlertElement->ActionFunc = SimpleFrameAggregation;
        break;
   case TX_RX_ALL_MSDU_FRAMES:
        TxMask = DIRECTED_FRAMES_XFER|MULTICAST_FRAMES_XFER|BROADCAST_FRAMES_XFER;
        RxMask = DIRECTED_FRAMES_RECV|MULTICAST_FRAMES_RECV|BROADCAST_FRAMES_RECV;
        TrafficAlertElement->ActionFunc = SimpleFrameAggregation;
        break;
    case TX_RX_ALL_MSDU_IN_BYTES:
        TxMask = DIRECTED_BYTES_XFER|MULTICAST_BYTES_XFER|BROADCAST_BYTES_XFER;
        RxMask = DIRECTED_BYTES_RECV|MULTICAST_BYTES_RECV|BROADCAST_BYTES_RECV;
        TrafficAlertElement->ActionFunc = SimpleByteAggregation;
        break;
   case TX_RX_DIRECTED_IN_BYTES:
        TxMask = DIRECTED_BYTES_XFER;
        RxMask = DIRECTED_BYTES_RECV;
        TrafficAlertElement->ActionFunc = SimpleByteAggregation;
    break;
   case TX_RX_ALL_802_11_DATA_IN_BYTES:
        TxMask = DIRECTED_BYTES_XFER | MULTICAST_BYTES_XFER;
        RxMask = DIRECTED_BYTES_RECV | MULTICAST_BYTES_RECV;
        TrafficAlertElement->ActionFunc = SimpleByteAggregation;
    break;
   case TX_RX_ALL_802_11_DATA_FRAMES:
        TxMask = DIRECTED_FRAMES_XFER | MULTICAST_FRAMES_XFER;
        RxMask = DIRECTED_FRAMES_RECV | MULTICAST_FRAMES_RECV;
        TrafficAlertElement->ActionFunc = SimpleFrameAggregation;
    break;
   default:
        WLAN_OS_REPORT(("TrafficMonitor_SetMask - unknown parameter: %d\n", MaskType));
       return TI_NOK;
   }
 
   
   if(RxMask)
   {
       TrafficAlertElement->MonitorMask[RX_TRAFF_MODULE] = RxMask;
       if(rxData_AddToNotifMask(TrafficMonitor->hRxData,TrafficMonitor->RxRegReqHandle,RxMask) == TI_NOK)
           return TI_NOK;
   }
   
   if(TxMask)
   {
       TrafficAlertElement->MonitorMask[TX_TRAFF_MODULE] = TxMask;
       if(txCtrlParams_AddToNotifMask(TrafficMonitor->hTxCtrl,TrafficMonitor->TxRegReqHandle,TxMask) == TI_NOK)
           return TI_NOK;
   }

   return TI_OK;
}


/***********************************************************************
 *                        TrafficMonitor_SetRstCondition                        
 ***********************************************************************
DESCRIPTION: Reg event processing function, Perform the following:
             Sets the given reset element to the Alert element. 
             if MutualRst is set, then The operation is done vise versa .  
                                
INPUT:          hTrafficMonitor -       Traffic Monitor the object.
                        
            EventHandle -         Alert event
            
            ResetEventHandle  Alert Event that will be  used to as the rest for above.
                        
            MutualRst - if the 2 elements are used to reset One another. 

NOTE        If the reset element event condition is the same as the alert element the user 
            have to check the that threshold is bigger or smaller according to the direction 
            else it can create a deadlock

OUTPUT:         

RETURN:     TI_OK on success, TI_NOK otherwise

************************************************************************/
TI_STATUS TrafficMonitor_SetRstCondition(TI_HANDLE hTrafficMonitor, TI_HANDLE EventHandle,TI_HANDLE ResetEventHandle,TI_BOOL MutualRst)
{
    TrafficMonitor_t *TrafficMonitor =(TrafficMonitor_t*)hTrafficMonitor;
    TrafficAlertElement_t *TrafficAlertElement = (TrafficAlertElement_t*)EventHandle;
    TrafficAlertElement_t *TrafficResetAlertElement = (TrafficAlertElement_t*)ResetEventHandle;
    int i,x;
    TI_UINT32 CurentTime ;

    if((TrafficMonitor == NULL) || (EventHandle == NULL) || (TrafficResetAlertElement == NULL)) 
        return TI_NOK;

    
    CurentTime = os_timeStampMs(TrafficMonitor->hOs);

    /*
    Check that validity of the reset condition 
    1.The reset condition is edge.
    2.The direction is opposite from the main alert.
    3.The threshold is bigger or smaller according to the direction 
    This condition is not checked but the user have check it else it can create a deadlock..
    */
    if((TrafficResetAlertElement->Trigger != TRAFF_EDGE) || (TrafficAlertElement->Trigger != TRAFF_EDGE))
        return TI_NOK;
    if(TrafficResetAlertElement->Direction == TrafficAlertElement->Direction)
        return TI_NOK;


    /*Find an empty Rst element entry*/
    if(FindRstElemEntryIndex(TrafficMonitor,TrafficResetAlertElement,&i) == TI_NOK)
        return TI_NOK;
    
    TrafficResetAlertElement->ResetElment[i] = TrafficAlertElement;
    
    /*if we know for sure that No Rst Element was assigned  
    therefore that element was in disable mode and we have to enable it.*/
    if (!(TrafficAlertElement->RstWasAssigned))
    {
        TrafficAlertElement->RstWasAssigned = TI_TRUE;
        TrafficAlertElement->CurrentState = ALERT_OFF;
        TrafficAlertElement->TimeOut = CurentTime + TrafficAlertElement->TimeIntervalMs;
        TrafficAlertElement->EventCounter =0;
    }
    

    if(MutualRst)
    {
      /*Find an empty Rst element entry in the TempRstAlertElement*/
      if(FindRstElemEntryIndex(TrafficMonitor,TrafficAlertElement,&x) == TI_NOK)
      {
        /*this clean up is not complete*/
        TrafficResetAlertElement->ResetElment[i] = NULL;
        return TI_NOK;
      }

      TrafficAlertElement->ResetElment[x] = TrafficResetAlertElement;
      /*if know for sure that No Rst Element was assigned  
      therefore that element was in disable mode and we have to enable it.*/
      if (!(TrafficResetAlertElement->RstWasAssigned))
      {
        TrafficResetAlertElement->RstWasAssigned = TI_TRUE;
        TrafficResetAlertElement->CurrentState = ALERT_OFF;
        TrafficResetAlertElement->TimeOut = CurentTime + TrafficAlertElement->TimeIntervalMs;
        TrafficResetAlertElement->EventCounter = 0;
      }
    }
    return TI_OK;
}


/************************************************************************/
/*               TrafficMonitor_CleanRelatedRef                         */
/************************************************************************/
void TrafficMonitor_CleanRelatedRef(TrafficMonitor_t *TrafficMonitor,TrafficAlertElement_t *TrafficAlertElement)
{
 
    int i;
    TrafficAlertElement_t *AlertElement  = (TrafficAlertElement_t*)List_GetFirst(TrafficMonitor->NotificationRegList);
      
    /* go over all the Down elements and check for alert ResetElment that ref to TrafficAlertElement*/    
    while(AlertElement)
    {
        for(i=0;i<MAX_RST_ELMENT_PER_ALERT;i++)
        {
            if(AlertElement->ResetElment[i] == TrafficAlertElement)
                AlertElement->ResetElment[i] = NULL;
        }
        AlertElement = (TrafficAlertElement_t*)List_GetNext(TrafficMonitor->NotificationRegList);
    } 
}
 


/************************************************************************/
/*          TrafficMonitor_StopNotif                                   */
/************************************************************************/
void TrafficMonitor_StopEventNotif(TI_HANDLE hTrafficMonitor,TI_HANDLE EventHandle)
{
    TrafficMonitor_t *TrafficMonitor =(TrafficMonitor_t*)hTrafficMonitor;
    TrafficAlertElement_t *TrafficAlertElement = (TrafficAlertElement_t*)EventHandle;

    if(TrafficMonitor == NULL)
        return ;

    if(TrafficAlertElement == NULL)
        return ;

    TrafficAlertElement->Enabled = TI_FALSE;
    TrafficMonitor_UpdateDownTrafficTimerState (hTrafficMonitor);

}



/************************************************************************/
/*          TrafficMonitor_StartNotif                                   */
/************************************************************************/
void TrafficMonitor_StartEventNotif(TI_HANDLE hTrafficMonitor, TI_HANDLE EventHandle)
{
    TrafficMonitor_t *TrafficMonitor =(TrafficMonitor_t*)hTrafficMonitor;
    TrafficAlertElement_t *TrafficAlertElement = (TrafficAlertElement_t*)EventHandle;

    if(TrafficMonitor == NULL)
        return ;

    if(TrafficAlertElement == NULL)
        return ;

    TrafficAlertElement->Enabled = TI_TRUE;
    TrafficMonitor_UpdateDownTrafficTimerState (hTrafficMonitor);
}



/************************************************************************/
/*          TrafficMonitor_StartNotif                                   */
/************************************************************************/
void TrafficMonitor_ResetEvent(TI_HANDLE hTrafficMonitor, TI_HANDLE EventHandle)
{
    TrafficMonitor_t *TrafficMonitor =(TrafficMonitor_t*)hTrafficMonitor;
    TrafficAlertElement_t *TrafficAlertElement = (TrafficAlertElement_t*)EventHandle;

    if(TrafficMonitor == NULL)
        return ;

    if(TrafficAlertElement == NULL)
        return ;

    TrafficAlertElement->CurrentState = ALERT_OFF;

    TrafficMonitor_UpdateDownTrafficTimerState (TrafficMonitor);
}



/************************************************************************/
/*          TrafficMonitor_UnregEvent                                   */
/************************************************************************/
void TrafficMonitor_UnregEvent(TI_HANDLE hTrafficMonitor, TI_HANDLE EventHandle)
{
    TrafficMonitor_t *TrafficMonitor =(TrafficMonitor_t*)hTrafficMonitor;
    TrafficAlertElement_t *TrafficAlertElement = (TrafficAlertElement_t*)EventHandle;

    if(TrafficMonitor == NULL)
        return ;
    
    /*If it was an edge alert then there can be one more alert element to free.*/
    /*one is the alert, and the second is the reset element that corresponds to this alert*/
    /*if it was  Auto Created*/
    if (TrafficAlertElement->ResetElment[0])
      if (TrafficAlertElement->ResetElment[0]->AutoCreated)
       List_FreeElement(TrafficMonitor->NotificationRegList,TrafficAlertElement->ResetElment);

    TrafficMonitor_CleanRelatedRef(TrafficMonitor,TrafficAlertElement);

    List_FreeElement(TrafficMonitor->NotificationRegList,EventHandle);

    TrafficMonitor_UpdateDownTrafficTimerState (TrafficMonitor);
}
 


/***********************************************************************
 *                        isThresholdUp                 
 ***********************************************************************
DESCRIPTION: Evaluate if alert element as crossed his threshold 
             if yes it operate the callback registered for this alert and take care of the alert state.  
                         For alert with UP direction the following algorithm is preformed
             If the threshold is passed in the req time interval or less. then
             For Level
                The alert mode is changed to ON & the next timeout is set to the next interval.
             For Edge
                The alert mode is changed to wait for reset and the reset element is set to off.
                And his timeout is set

INPUT:                          
            EventHandle -         Alert event
            CurrentTime - the current time Time stamp 
            
OUTPUT:         

RETURN:     If  threshold crossed TI_TRUE else False

************************************************************************/
static TI_BOOL isThresholdUp(TrafficAlertElement_t *AlertElement , TI_UINT32 CurrentTime)
{  
    int i;
    
    if (AlertElement->TimeOut < CurrentTime)        
    {
        AlertElement->EventCounter = AlertElement->LastCounte;
        AlertElement->TimeOut = CurrentTime + AlertElement->TimeIntervalMs;
    }
    
    if (AlertElement->EventCounter > AlertElement->Threshold)
    {
        AlertElement->EventCounter = 0;
        /*Sets the new due time (time out)*/
        AlertElement->TimeOut = CurrentTime + AlertElement->TimeIntervalMs;
        
        /*For Edge alert change the alert status to wait for reset and 
        The corresponding reset element from wait for reset To off.
        That way toggling the two elements*/
        if(AlertElement->Trigger == TRAFF_EDGE)
        {
            AlertElement->CurrentState = ALERT_WAIT_FOR_RESET;
            for(i=0;i<MAX_RST_ELMENT_PER_ALERT;i++)
            {
                TrafficAlertElement_t *rstElmt = AlertElement->ResetElment[i];
                if(rstElmt != NULL)
                    if(rstElmt->CurrentState == ALERT_WAIT_FOR_RESET)
                    {
                        rstElmt->CurrentState = ALERT_OFF;
                        rstElmt->EventCounter = 0;
                        rstElmt->TimeOut = CurrentTime + rstElmt->TimeIntervalMs;
                    }
            }
        }
        else 
            AlertElement->CurrentState = ALERT_ON;
        
        /*Call the callback function*/
        if((AlertElement->CallBack != NULL) && AlertElement->Enabled)
            AlertElement->CallBack(AlertElement->Context,AlertElement->Cookie);
        return TI_TRUE;
    }
    
    return TI_FALSE;
}



/***********************************************************************
 *                        isThresholdDown               
 ***********************************************************************
DESCRIPTION: Evaluate if alert element as crossed his threshold 
             if yes it operate the callback registered for this alert and take care of the alert state.  
                         For alert with DOWN direction the following algorithm is preformed
             If the threshold is passed (EventCounter < Threshold) in the req time only. then
             For Level
               The alert mode is changed to ON & the next timeout is set to the next interval.
               If the alert condition will still be on.then the next alert will be in the next time interval
            For Edge
               The alert mode is changed to wait for reset and the reset element is set to off.
               And his timeout is set.

INPUT:                          
            EventHandle -         Alert event
            CurrentTime - the current time Time stamp 
            
OUTPUT:         

RETURN:     If threshold crossed TI_TRUE else False

************************************************************************/
static TI_BOOL isThresholdDown(TrafficAlertElement_t *AlertElement , TI_UINT32 CurrentTime)
{
    int i;
    TI_BOOL returnVal = TI_FALSE;

    /*
    if its end of window time.
    */
    if (AlertElement->TimeOut <= CurrentTime)        
    {
        /*
        if there was a down edge event.
        */
        if (AlertElement->EventCounter <= AlertElement->Threshold)
        {
            /*For Edge alert change the alert status to wait for reset and 
            The corresponding reset element from wait for reset To off.
            That way toggling the two elements*/
            if(AlertElement->Trigger == TRAFF_EDGE)
            {
                AlertElement->CurrentState = ALERT_WAIT_FOR_RESET;
                for(i=0;i<MAX_RST_ELMENT_PER_ALERT;i++)
                {
                    TrafficAlertElement_t *rstElmt = AlertElement->ResetElment[i];
                    if(rstElmt != NULL)
                        if(rstElmt->CurrentState == ALERT_WAIT_FOR_RESET)
                        {
                            rstElmt->CurrentState = ALERT_OFF;
                            rstElmt->EventCounter = 0;
                            rstElmt->TimeOut = CurrentTime + rstElmt->TimeIntervalMs;
                        }
                }
            }
            else 
                AlertElement->CurrentState = ALERT_ON;
            
            /*Call the callback function*/
            if((AlertElement->CallBack != NULL) && AlertElement->Enabled)
                AlertElement->CallBack(AlertElement->Context,AlertElement->Cookie);
            
            returnVal = TI_TRUE;
        }

        /* end of time window - clear the event counter for the new window.*/
        AlertElement->EventCounter = 0;
        /*Sets the new due time (time out)*/
        AlertElement->TimeOut = CurrentTime + AlertElement->TimeIntervalMs;
    }
    else
    {
        /*
        In case we find out that the alert condition will not Occur for this frame window,
        therefor start a new alert examine cycle (the next farme window).
        (Not wait till the timeout of this current frame window)
        */
        if(AlertElement->EventCounter > AlertElement->Threshold)
        {
            AlertElement->EventCounter = 0;
            AlertElement->TimeOut = CurrentTime + AlertElement->TimeIntervalMs;
        }
    }
    return returnVal;
}



/************************************************************************/
/*              TimerMonitor_TimeOut                                    */
/************************************************************************/
/*
 *      Timer function that is called for every x time interval 
 *   That will invoke a process if any down limit as occurred. 
 *
 ************************************************************************/
static void TimerMonitor_TimeOut (TI_HANDLE hTrafficMonitor, TI_BOOL bTwdInitOccured)
{
    
    TrafficMonitor_t *TrafficMonitor =(TrafficMonitor_t*)hTrafficMonitor;
    TrafficAlertElement_t *AlertElement;
    TI_UINT32 CurentTime;
    TI_UINT32 activeTrafDownEventsNum = 0;
    TI_UINT32 trafficDownMinTimeout = 0xFFFFFFFF;
  
    if(TrafficMonitor == NULL)
        return;

    AlertElement  = (TrafficAlertElement_t*)List_GetFirst(TrafficMonitor->NotificationRegList);
    CurentTime = os_timeStampMs(TrafficMonitor->hOs);
    
    
    /* go over all the Down elements and check for alert */    
    while(AlertElement)
    {
        if(AlertElement->CurrentState != ALERT_WAIT_FOR_RESET) 
        {
            if (AlertElement->Direction == TRAFF_DOWN)
            {
               isThresholdDown(AlertElement,CurentTime);
            }   
        }
 
         if ((AlertElement->Direction == TRAFF_DOWN) && (AlertElement->Trigger == TRAFF_EDGE) && (AlertElement->CurrentState == ALERT_OFF) && (AlertElement->Enabled == TI_TRUE))
{
            /* Increase counter of active traffic down events */
            activeTrafDownEventsNum++;

            /* Search for the alert with the most short Interval time - will be used to start timer */
            if ((AlertElement->TimeIntervalMs) < (trafficDownMinTimeout))
               trafficDownMinTimeout = AlertElement->TimeIntervalMs;
         }
    
        AlertElement = (TrafficAlertElement_t*)List_GetNext(TrafficMonitor->NotificationRegList);
    }   
    
   TrafficMonitor_ChangeDownTimerStatus (TrafficMonitor,activeTrafDownEventsNum,trafficDownMinTimeout);
    
}

/***********************************************************************
 *                        TrafficMonitor_IsEventOn
 ***********************************************************************
DESCRIPTION: Returns the current status of an event element.

INPUT:      TrafficAlertElement_t 


OUTPUT:    bool

RETURN:     True = ON  false = OFF

************************************************************************/
TI_BOOL TrafficMonitor_IsEventOn(TI_HANDLE EventHandle)
{
    TrafficAlertElement_t *TrafficAlertElement = (TrafficAlertElement_t*)EventHandle;

    if(TrafficAlertElement == NULL)
        return TI_FALSE;
    
    
    if (TrafficAlertElement->CurrentState == ALERT_OFF)
        return TI_FALSE;
    else
        return TI_TRUE;

}



/***********************************************************************
 *                        TrafficMonitor_GetFrameBandwidth                        
 ***********************************************************************
DESCRIPTION: Returns the total direct frames in the Rx and Tx per second. 
                                
INPUT:          hTrafficMonitor -       Traffic Monitor the object.
                        
            
OUTPUT:         

RETURN:     Total BW
************************************************************************/
int TrafficMonitor_GetFrameBandwidth(TI_HANDLE hTrafficMonitor)
{
	TrafficMonitor_t 	*pTrafficMonitor =(TrafficMonitor_t*)hTrafficMonitor;
	TI_UINT32 			uCurentTS;
   
	if(pTrafficMonitor == NULL)
        return TI_NOK;

	uCurentTS = os_timeStampMs(pTrafficMonitor->hOs);

	/* Calculate BW for Rx & Tx */
	return ( TrafficMonitor_calcBW(&pTrafficMonitor->DirectRxFrameBW, uCurentTS) +
			 TrafficMonitor_calcBW(&pTrafficMonitor->DirectTxFrameBW, uCurentTS) );
}

/***********************************************************************
*                        TrafficMonitor_updateBW                        
***********************************************************************
DESCRIPTION: Upon receiving an event of Tx/Rx (a packet was sent or received), This function is 
				called and performs BW calculation.

INPUT:          
				pBandWidth		- BW of Rx or Tx	
				uCurrentTS		- current TS of the recent event

OUTPUT:         pBandWidth		- updated counters and TS

************************************************************************/
void TrafficMonitor_updateBW(BandWidth_t *pBandWidth, TI_UINT32 uCurrentTS)
{
	/* Check if we should move to the next window */
	if ( (uCurrentTS - pBandWidth->auFirstEventsTS[pBandWidth->uCurrentWindow]) < (SIZE_OF_WINDOW_MS) )
	{	
		pBandWidth->auWindowCounter[pBandWidth->uCurrentWindow]++;
	}
	else	/* next window */
	{	
		/* increment current window and mark the first event received */	
		pBandWidth->uCurrentWindow = (pBandWidth->uCurrentWindow + 1) & CYCLIC_COUNTER_ELEMENT;
		pBandWidth->auFirstEventsTS[pBandWidth->uCurrentWindow] = uCurrentTS;
		pBandWidth->auWindowCounter[pBandWidth->uCurrentWindow] = 1;
	}
}
/***********************************************************************
*                        TrafficMonitor_calcBW                        
***********************************************************************
DESCRIPTION: Returns the total direct frames in Rx or Tx.
			 It is called when outside module request the BW. 
			 Calculate band width by summing up the sliding windows.

INPUT:       pBandWidth		- BW of Rx or Tx	
			 uCurrentTS		- current TS

RETURN:     Total BW
************************************************************************/
TI_UINT32 TrafficMonitor_calcBW(BandWidth_t *pBandWidth, TI_UINT32 uCurrentTS)
{
	TI_UINT32 uTotalTime = uCurrentTS - pBandWidth->auFirstEventsTS[pBandWidth->uCurrentWindow];
	TI_UINT32 uTotalBW = 0;
	TI_INT32  iter = (TI_INT32)pBandWidth->uCurrentWindow;
	TI_INT32  iNextIter = (iter - 1) & CYCLIC_COUNTER_ELEMENT;	/* Always one less than i */

	/* As long as the summed windows are less than BW_WINDOW_MS and we didn't loop the whole array */
	while ( (uTotalTime < BW_WINDOW_MS) && (iNextIter != pBandWidth->uCurrentWindow))
	{ 
		uTotalBW	+= pBandWidth->auWindowCounter[iter];
		/* add next window time - next loop will check if we exceeded the BW window */
		uTotalTime   = uCurrentTS - pBandWidth->auFirstEventsTS[iNextIter];

		iter = iNextIter; 
		iNextIter = (iter - 1) & CYCLIC_COUNTER_ELEMENT;					
	} ;

	/* 
	 * Note that if (iNextIter == pBandWidth->uCurrentWindow) than the calculated BW could be up to 
	 * SIZE_OF_WINDOW_MS less than BW_WINDOW_MS 
	 */
	return uTotalBW;
}


/***********************************************************************
 *                        TrafficMonitor_Event                  
 ***********************************************************************
DESCRIPTION: this function is called for every event that was requested from the Tx or Rx
             The function preformes update of the all the relevant Alert in the system 
             that corresponds to the event. checks the Alert Status due to this event.
             
 
                                
INPUT:          hTrafficMonitor -       Traffic Monitor the object.
                        
            Count - evnet count.
            Mask - the event mask that That triggered this function.
            
            MonitorModuleType Will hold the module type from where this function was called. 
            
OUTPUT:         

RETURN:     

************************************************************************/
void TrafficMonitor_Event(TI_HANDLE hTrafficMonitor,int Count,TI_UINT16 Mask,TI_UINT32 MonitorModuleType)
{
    TrafficMonitor_t *TrafficMonitor =(TrafficMonitor_t*)hTrafficMonitor;
    TrafficAlertElement_t *AlertElement;
    TI_UINT32 activeTrafDownEventsNum = 0;
    TI_UINT32 trafficDownMinTimeout = 0xFFFFFFFF;
    TI_UINT32 uCurentTS;

    if(TrafficMonitor == NULL)
        return;

    if(!TrafficMonitor->Active)   
        return;

	uCurentTS = os_timeStampMs(TrafficMonitor->hOs);

    /* for BW calculation */
    if(MonitorModuleType == RX_TRAFF_MODULE)
    {
        if(Mask & DIRECTED_FRAMES_RECV)
		{
            TrafficMonitor_updateBW(&TrafficMonitor->DirectRxFrameBW, uCurentTS); 
		} 
    }
    else if (MonitorModuleType == TX_TRAFF_MODULE)
    {
        if(Mask & DIRECTED_FRAMES_XFER)
		{
            TrafficMonitor_updateBW(&TrafficMonitor->DirectTxFrameBW, uCurentTS);
		}
    }
    else  
	{
        return; /* module type does not exist, error return */
	}

    AlertElement  = (TrafficAlertElement_t*)List_GetFirst(TrafficMonitor->NotificationRegList);
    
    /* go over all the elements and check for alert */    
    while(AlertElement)
    {
        if(AlertElement->CurrentState != ALERT_WAIT_FOR_RESET) 
        {
            if(AlertElement->MonitorMask[MonitorModuleType] & Mask)
            {
                AlertElement->ActionFunc(AlertElement,Count);
                if (AlertElement->Direction == TRAFF_UP)
                {
                    isThresholdUp(AlertElement, uCurentTS);
                }
            }

            if ((AlertElement->Direction == TRAFF_DOWN) && (AlertElement->Trigger == TRAFF_EDGE) && (AlertElement->CurrentState == ALERT_OFF) && (AlertElement->Enabled == TI_TRUE))
            {
               /* Increase counter of active traffic down events */
               activeTrafDownEventsNum++;

               /* Search for the alert with the most short Interval time - will be used to start timer */
               if ((AlertElement->TimeIntervalMs) < (trafficDownMinTimeout))
                  trafficDownMinTimeout = AlertElement->TimeIntervalMs;
            }

        }
        AlertElement = (TrafficAlertElement_t*)List_GetNext(TrafficMonitor->NotificationRegList);
    }

    TrafficMonitor_ChangeDownTimerStatus (TrafficMonitor,activeTrafDownEventsNum,trafficDownMinTimeout);

}


/*
 *      Used as the aggregation function that is used by the alerts for counting the events. 
 */
static void SimpleByteAggregation(TI_HANDLE TraffElem,int Count)
{
    TrafficAlertElement_t *AlertElement = TraffElem;
    AlertElement->EventCounter += Count;
    AlertElement->LastCounte = Count; 
}


/*
 *      Used as the aggregation function for frame. (count is not used)
 */
static void SimpleFrameAggregation(TI_HANDLE TraffElem,int Count)
{
    TrafficAlertElement_t *AlertElement = TraffElem;
    AlertElement->EventCounter++;
    AlertElement->LastCounte = 1; 
}

/*-----------------------------------------------------------------------------
Routine Name: TrafficMonitor_UpdateDownTrafficTimerState
Routine Description: called whenever a "down" alert is called, or any other change in the alert list.
                     used to either start or stop the "traffic down" timer.
                     loops through alert list, searches for active traffic down events.
Arguments:
Return Value:
-----------------------------------------------------------------------------*/
static void TrafficMonitor_UpdateDownTrafficTimerState (TI_HANDLE hTrafficMonitor)
{
	TrafficMonitor_t *TrafficMonitor =(TrafficMonitor_t*)hTrafficMonitor;
    TrafficAlertElement_t *AlertElement;
    TI_UINT32 activeTrafDownEventsNum = 0;
    TI_UINT32 trafficDownMinTimeout = 0xFFFFFFFF;

    AlertElement  = (TrafficAlertElement_t*)List_GetFirst(TrafficMonitor->NotificationRegList);
    
    while(AlertElement)
    {

      if ((AlertElement->Direction == TRAFF_DOWN) && (AlertElement->Trigger == TRAFF_EDGE) && (AlertElement->CurrentState == ALERT_OFF) && (AlertElement->Enabled == TI_TRUE))
      {
         /* Increase counter of active traffic down events */
         activeTrafDownEventsNum++;

         /* Search for the alert with the most short Interval time - will be used to start timer */
         if ((AlertElement->TimeIntervalMs) < (trafficDownMinTimeout))
            trafficDownMinTimeout = AlertElement->TimeIntervalMs;
      }

      AlertElement = (TrafficAlertElement_t*)List_GetNext(TrafficMonitor->NotificationRegList);

    }
   
    TrafficMonitor_ChangeDownTimerStatus (TrafficMonitor,activeTrafDownEventsNum,trafficDownMinTimeout);

}

/*-----------------------------------------------------------------------------
Routine Name: TrafficMonitor_ChangeDownTimerStatus
Routine Description: Start or stop down traffic timer according to number of down events found and minInterval time.
Arguments:
Return Value:
-----------------------------------------------------------------------------*/
static void TrafficMonitor_ChangeDownTimerStatus (TI_HANDLE hTrafficMonitor, TI_UINT32 downEventsFound, TI_UINT32 minIntervalTime)
{
	TrafficMonitor_t *pTrafficMonitor = (TrafficMonitor_t*)hTrafficMonitor;
    
    if ((downEventsFound == 0) && pTrafficMonitor->DownTimerEnabled)
    {
        pTrafficMonitor->DownTimerEnabled = TI_FALSE;
        tmr_StopTimer (pTrafficMonitor->hTrafficMonTimer);
        os_wake_unlock(pTrafficMonitor->hOs);
    }
    else if ((downEventsFound > 0) && (pTrafficMonitor->DownTimerEnabled == TI_FALSE))
    {
        os_wake_lock(pTrafficMonitor->hOs);
        pTrafficMonitor->DownTimerEnabled = TI_TRUE;
        /* Start the timer with user defined percentage of the the minimum interval discovered earlier */
        tmr_StartTimer (pTrafficMonitor->hTrafficMonTimer,
                        TimerMonitor_TimeOut,
                        (TI_HANDLE)pTrafficMonitor,
                        ((minIntervalTime * pTrafficMonitor->trafficDownTestIntervalPercent) / 100), 
                        TI_TRUE);
    }
}

#ifdef TI_DBG

/*-----------------------------------------------------------------------------
Routine Name: TrafficMonitor_UpdateActiveEventsCounters
Routine Description: 
Arguments:
Return Value:
-----------------------------------------------------------------------------*/
void TrafficMonitor_UpdateActiveEventsCounters (TI_HANDLE hTrafficMonitor)
{
	TrafficMonitor_t *TrafficMonitor =(TrafficMonitor_t*)hTrafficMonitor;
    TrafficAlertElement_t *AlertElement;
    TI_UINT32 activeTrafDownEventsNum = 0;

    AlertElement  = (TrafficAlertElement_t*)List_GetFirst(TrafficMonitor->NotificationRegList);
    
    while(AlertElement)
    {
      if ((AlertElement->Direction == TRAFF_DOWN) && (AlertElement->Trigger == TRAFF_EDGE) && (AlertElement->CurrentState == ALERT_OFF) && (AlertElement->Enabled == TI_TRUE))
      {
         activeTrafDownEventsNum++;
      }
      AlertElement = (TrafficAlertElement_t*)List_GetNext(TrafficMonitor->NotificationRegList);
    }

}


#endif

#ifdef TRAFF_TEST
/*
 *      TEST Function 
 */
void func1(TI_HANDLE Context,TI_UINT32 Cookie)
{
    switch(Cookie) {
    case 1:
                WLAN_OS_REPORT(("TRAFF - ALERT UP limit - 50 ON"));
        break;
    case 2:
                WLAN_OS_REPORT(("TRAFF - ALERT UP limit - 30 ON"));
    break;
    case 3:
                WLAN_OS_REPORT(("TRAFF - ALERT DOWN limit - 25 ON"));  
    break;
    case 4:
                WLAN_OS_REPORT(("TRAFF - ALERT DOWN limit - 10 ON"));  
    break;
   }
    
}


void PrintElertStus()
{
    TrafficMonitor_t *TrafficMonitor =(TrafficMonitor_t*)TestTrafficMonitor;
    TrafficAlertElement_t *AlertElement  = (TrafficAlertElement_t*)List_GetFirst(TrafficMonitor->NotificationRegList);
      
    /* go over all the Down elements and check for alert ResetElment that ref to TrafficAlertElement*/    
    while(AlertElement)
    {
        if(AlertElement->CurrentState == ALERT_WAIT_FOR_RESET)
            WLAN_OS_REPORT(("TRAFF - ALERT ALERT_WAIT_FOR_RESET"));
        else
            WLAN_OS_REPORT(("TRAFF - ALERT ENABLED"));

            
        AlertElement = (TrafficAlertElement_t*)List_GetNext(TrafficMonitor->NotificationRegList);
    } 
}

void TestEventFunc (TI_HANDLE hTrafficMonitor, TI_BOOL bTwdInitOccured)
{

    static flag = TI_TRUE;
    TrafficAlertRegParm_t TrafficAlertRegParm ;
    if(flag)
    {

        TrafficAlertRegParm.CallBack = func1;
        TrafficAlertRegParm.Context = NULL ; 
        TrafficAlertRegParm.Cookie =  1 ;    
        TrafficAlertRegParm.Direction = TRAFF_UP ;
        TrafficAlertRegParm.Trigger = TRAFF_EDGE;
        TrafficAlertRegParm.TimeIntervalMs = 1000;
        TrafficAlertRegParm.Threshold = 50;
        TrafficAlertRegParm.MonitorType = TX_RX_DIRECTED_FRAMES;
        Alert1 = TrafficMonitor_RegEvent(TestTrafficMonitor,&TrafficAlertRegParm,TI_FALSE);   
        
        TrafficAlertRegParm.CallBack = func1;
        TrafficAlertRegParm.Context = NULL ; 
        TrafficAlertRegParm.Cookie =  2 ;    
        TrafficAlertRegParm.Direction = TRAFF_UP ;
        TrafficAlertRegParm.Trigger = TRAFF_EDGE;
        TrafficAlertRegParm.TimeIntervalMs = 1000;
        TrafficAlertRegParm.Threshold = 30;
        TrafficAlertRegParm.MonitorType = TX_RX_DIRECTED_FRAMES;
        Alert2 = TrafficMonitor_RegEvent(TestTrafficMonitor,&TrafficAlertRegParm,TI_FALSE);
        
      
        TrafficAlertRegParm.CallBack = func1;
        TrafficAlertRegParm.Context = NULL ; 
        TrafficAlertRegParm.Cookie =  3 ;    
        TrafficAlertRegParm.Direction = TRAFF_DOWN ;
        TrafficAlertRegParm.Trigger = TRAFF_EDGE;
        TrafficAlertRegParm.TimeIntervalMs = 1000;
        TrafficAlertRegParm.Threshold = 25;
        TrafficAlertRegParm.MonitorType = TX_RX_DIRECTED_FRAMES;
        Alert3 = TrafficMonitor_RegEvent(TestTrafficMonitor,&TrafficAlertRegParm,TI_FALSE);   
        
        TrafficAlertRegParm.CallBack = func1;
        TrafficAlertRegParm.Context = NULL ; 
        TrafficAlertRegParm.Cookie =  4 ;    
        TrafficAlertRegParm.Direction = TRAFF_DOWN ;
        TrafficAlertRegParm.Trigger = TRAFF_LEVEL;
        TrafficAlertRegParm.TimeIntervalMs = 1000;
        TrafficAlertRegParm.Threshold = 10;
        TrafficAlertRegParm.MonitorType = TX_RX_DIRECTED_FRAMES;
        Alert4 = TrafficMonitor_RegEvent(TestTrafficMonitor,&TrafficAlertRegParm,TI_FALSE);
        
       TrafficMonitor_SetRstCondition(TestTrafficMonitor, Alert1,Alert3,TI_TRUE);
       TrafficMonitor_SetRstCondition(TestTrafficMonitor, Alert2,Alert3,TI_FALSE);
       flag = TI_FALSE;
    }

    PrintElertStus();

}

#endif
