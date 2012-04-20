/*
 * eventMbox.c
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


/** \file  eventMbox.c 
 *  \brief Handle any event interrupt from the FW
 *
 *  \see   
 */

#define __FILE_ID__  FILE_ID_102
#include "eventMbox_api.h"
#include "TwIf.h"
#include "osApi.h"
#include "report.h"
#include "CmdBld.h"
#include "FwEvent_api.h"
#include "TWDriver.h"
#include "BusDrv.h"



#define EVENT_MBOX_BUFFERS 2


typedef enum
{
    EVENT_MBOX_STATE_IDLE,
    EVENT_MBOX_STATE_READING
} EEventMboxState;

typedef struct {
	void*		fCb;            /* Event Callback function */
	TI_HANDLE	hCb;            /* Evant handle */
	TI_UINT8*	pDataOffset;    /* Event Data Offset */
	#ifdef TI_DBG
	TI_UINT32	uCount;
	#endif

}TRegisteredEventCb;

typedef struct 
{
    TI_UINT32           bitMask;/* Event bit mask */
    char*               str;    /* Event trace string */
    TI_UINT8            dataLen;/* Event data length */  

} TEventEntry;


typedef struct
{
	TTxnStruct	tTxnReg;
	TI_UINT32	iRegBuffer;

} tTxnGenReg;


typedef struct
{
	TTxnStruct		tEventMbox;
	EventMailBox_t	iEventMboxBuf;

} tTxnEventMbox;



typedef struct 
{
	TI_UINT32		   	EventMboxAddr[EVENT_MBOX_BUFFERS];  /* the Event Mbox addresses in the device */
	TI_UINT8		   	ActiveMbox;                         /* The current active Mbox */
	EEventMboxState		CurrentState;
    TRegisteredEventCb  CbTable[TWD_OWN_EVENT_MAX];         /* Callback table */

    /* Handles */
	TI_HANDLE           hTwif;
    TI_HANDLE           hOs;
    TI_HANDLE           hReport;
    TI_HANDLE           hCmdBld;

	/* HW params */
    /* use a struct to read or write register (4 byte size) from the bus */
	tTxnGenReg			iTxnGenRegSize;

	tTxnEventMbox		iTxnEventMbox;

	#ifdef TI_DBG
	TI_UINT32           uCompounEvCount;    /* Count the compound event */
	TI_UINT32           uTotalEvCount;      /* Count total number of event sending in the compound */
	#endif /* TI_DBG */
	
	fnotify_t			fCb;
	TI_HANDLE		   	hCb;

}TEventMbox;


/********************************************************************************/
/*                      Internal functions prototypes.                          */
/********************************************************************************/

static void eventMbox_ConfigCbTable(TI_HANDLE hEventMbox);
static void eventMbox_ReadAddrCb(TI_HANDLE hEventMbox, TI_HANDLE hTxn);
static void eventMbox_DummyCb(TI_HANDLE hEventMbox);
static void eventMbox_ReadCompleteCB(TI_HANDLE hEventMbox, TTxnStruct *pTxnStruct);


static const TEventEntry eventTable [TWD_OWN_EVENT_MAX] =
{   
/*==================================================================================
 *                                                                                     
 *                                    EVENT TABLE    
 *                                 
 *  Note that changes here should be reflected also in ETwdOwnEventId in TWDriver.h !!!
 *
 ===================================================================================
| Id  |     Event Mask Bit                    |   Event String            | Length |
 ===================================================================================*/

/* 0*/{ RSSI_SNR_TRIGGER_0_EVENT_ID,            "RSSI SNR TRIGGER 0 "     		, 1},
/* 1*/{ RSSI_SNR_TRIGGER_1_EVENT_ID,            "RSSI SNR TRIGGER 1 "     		, 1},
/* 2*/{ RSSI_SNR_TRIGGER_2_EVENT_ID,            "RSSI SNR TRIGGER 2 "     		, 1},
/* 3*/{ RSSI_SNR_TRIGGER_3_EVENT_ID,            "RSSI SNR TRIGGER 3 "     		, 1},
/* 4*/{ RSSI_SNR_TRIGGER_4_EVENT_ID,            "RSSI SNR TRIGGER 4 "     		, 1},
/* 5*/{ RSSI_SNR_TRIGGER_5_EVENT_ID,            "RSSI SNR TRIGGER 5 "     		, 1},
/* 6*/{ RSSI_SNR_TRIGGER_6_EVENT_ID,            "RSSI SNR TRIGGER 6 "     		, 1},
/* 7*/{ RSSI_SNR_TRIGGER_7_EVENT_ID,            "RSSI SNR TRIGGER 7 "     		, 1},
/* 8*/{ MEASUREMENT_START_EVENT_ID,             "MEASUREMENT START "      		, 0},    
/* 9*/{ MEASUREMENT_COMPLETE_EVENT_ID,          "BSS LOSE "               		, 0},
/*10*/{ SCAN_COMPLETE_EVENT_ID ,                "SCAN CMPLT "             		, 8},    
/*11*/{ SCHEDULED_SCAN_COMPLETE_EVENT_ID,       "SPS SCAN CMPLT "         		, 3},
/*12*/{ AP_DISCOVERY_COMPLETE_EVENT_ID,         "MAX TX RETRY "           		, 0},
/*13*/{ PS_REPORT_EVENT_ID,                     "PS_REPORT "              		, 1},
/*14*/{ PSPOLL_DELIVERY_FAILURE_EVENT_ID,       "PS-POLL DELIVERY FAILURE"		, 0},
/*15*/{ DISCONNECT_EVENT_COMPLETE_ID,           "DISCONNECT COMPLETE "    		, 0},
/*16*/{ JOIN_EVENT_COMPLETE_ID,                 "JOIN CMPLT "             		, 0},
/*17*/{ CHANNEL_SWITCH_COMPLETE_EVENT_ID,       "SWITCH CHANNEL CMPLT "   		, 0},
/*18*/{ BSS_LOSE_EVENT_ID,                      "BSS LOST "               		, 0},
/*19*/{ REGAINED_BSS_EVENT_ID,                  "REGAINED BSS "           		, 0},
/*20*/{ ROAMING_TRIGGER_MAX_TX_RETRY_EVENT_ID,  "MAX TX RETRY "           		, 0},
/*21*/{ BIT_21,									"RESERVED"				  		, 0},
/*22*/{ SOFT_GEMINI_SENSE_EVENT_ID,             "SOFT GEMINI SENSE "      		, 1},
/*23*/{ SOFT_GEMINI_PREDICTION_EVENT_ID,        "SOFT GEMINI PREDICTION " 		, 1},
/*24*/{ SOFT_GEMINI_AVALANCHE_EVENT_ID,         "SOFT GEMINI AVALANCHE "  		, 0},
/*25*/{ PLT_RX_CALIBRATION_COMPLETE_EVENT_ID,   "PLT RX CALIBR. COMPLETE "		, 0},
/*26*/{ DBG_EVENT_ID,							"DBG_EVENT_ID "			  		, 16},
/*27*/{ HEALTH_CHECK_REPLY_EVENT_ID,			"HEALTH_CHECK_REPLY_EVENT_ID"	, 0},
/*28*/{ PERIODIC_SCAN_COMPLETE_EVENT_ID,        "PERIODIC SCAN COMPLETE " 		, 8},
/*29*/{ PERIODIC_SCAN_REPORT_EVENT_ID,          "PERIODIC SCAN REPORT "   		, 8},
/*30*/{ BA_SESSION_TEAR_DOWN_EVENT_ID,			"BA_SESSION_TEAR_DOWN_EVENT_ID"	, 0},
/*31*/{ EVENT_MBOX_ALL_EVENT_ID,                "ALL EVENTS "             		, 0}
};


/*
 * \brief	Create the Bus Access mailbox object
 * 
 * \param  hOs - OS Handle
 * \returnThe Created object
 *
 * \sa 
 */

TI_HANDLE eventMbox_Create(TI_HANDLE hOs)
{
    TEventMbox *pEventMbox;
	pEventMbox = (TEventMbox*)os_memoryAlloc(hOs,sizeof(TEventMbox));
    if (pEventMbox == NULL)
    {
        WLAN_OS_REPORT (("eventMbox_Create: Error creating EventMbox object\n"));
        return NULL;
    }
    os_memoryZero (hOs, pEventMbox, sizeof(TEventMbox));
    pEventMbox->hOs = hOs;
    pEventMbox->iTxnEventMbox.iEventMboxBuf.eventsMask = EVENT_MBOX_ALL_EVENT_ID;
    return (TI_HANDLE)pEventMbox;
}


/*
 * \brief	Release all memory resource of EventMbox
 * 
 * \param  hEventMbox  - Handle to EventMbox
 * \return none
 * 
 * \par Description
 * This function should called after all interrupts was disabled.
 *
 * \sa 
 */
void eventMbox_Destroy(TI_HANDLE hEventMbox)
{
    TEventMbox *pEventMbox = (TEventMbox *)hEventMbox;

    if (pEventMbox)
    {
        os_memoryFree (pEventMbox->hOs, pEventMbox, sizeof(TEventMbox));
    }
}



/*
 * \brief	Stop the EventMbox clear state and event vector
 * 
 * \param  hEventMbox  - Handle to EventMbox
 * \return none
 * 
 * \par Description
 * This function should called to stop the EventMb.
 * Do Not clear the mask Event could use us again when restart/recovery!!!!
 * \sa 
 */
void eventMbox_Stop(TI_HANDLE hEventMbox)
{
	TEventMbox* pEventMbox 									= (TEventMbox*)hEventMbox;
	pEventMbox->ActiveMbox									= 0;
	pEventMbox->CurrentState								= EVENT_MBOX_STATE_IDLE;
	pEventMbox->iTxnEventMbox.iEventMboxBuf.eventsVector	= 0;
}



/*
 * \brief	Configure the object
 * 
 * \param  hEventMbox  - Handle to EventMbox
 * \param  hTwif	   - Handle to TWIF
 * \param  hReport	   - Handle to Report module
 * \param  hFwEvent    - Handle to FW Event module
 * \param  hCmdBld     - Handle to Command Build module
 * \return none
 *
 * \par Description
 * This function should called to configure the module.
 * \sa 
 */

void eventMbox_Config(TI_HANDLE hEventMbox, 
                            TI_HANDLE hTwif, 
                            TI_HANDLE hReport,    
                            TI_HANDLE hFwEvent, 
                            TI_HANDLE hCmdBld)
{
    TEventMbox *pEventMbox = (TEventMbox *)hEventMbox;
	pEventMbox->hTwif			= hTwif;
    pEventMbox->hReport = hReport;
    pEventMbox->hCmdBld = hCmdBld;
	pEventMbox->ActiveMbox		= 0;
	pEventMbox->CurrentState	= EVENT_MBOX_STATE_IDLE;
#ifdef TI_DBG
	pEventMbox->uCompounEvCount	= 0;
	pEventMbox->uTotalEvCount	= 0;
#endif
	eventMbox_ConfigCbTable(pEventMbox);
}



/* 
 * \brief	Initialization of callback table
 * 
 * \param  hEventMbox  - Handle to EventMbox
 * \return none
 * 
 * \par Description
 * This function is called to configure the CB table initialize the
 * CB functions and handle and set the Data offset.
 * 
 * \sa 
 */
static void eventMbox_ConfigCbTable(TI_HANDLE hEventMbox)
{
	TEventMbox* pEventMbox;
	TI_UINT8	EvID;
	
	pEventMbox = (TEventMbox*)hEventMbox;

	/* for all events set a dummy func and data offset */
	for (EvID = 0; EvID < TWD_OWN_EVENT_MAX;EvID++)
	{
		pEventMbox->CbTable[EvID].pDataOffset = (TI_UINT8*)&pEventMbox->iTxnEventMbox.iEventMboxBuf;
		pEventMbox->CbTable[EvID].fCb		  = (void*)eventMbox_DummyCb;
		pEventMbox->CbTable[EvID].hCb		  = pEventMbox;
	}
	/* set the data offset for Events with data only */
	for (EvID = 0;EvID < NUM_OF_RSSI_SNR_TRIGGERS;EvID++)
	{
		pEventMbox->CbTable[EvID].pDataOffset += TI_FIELD_OFFSET (EventMailBox_t, RSSISNRTriggerMetric[EvID]);
	}
	pEventMbox->CbTable[TWD_DBG_EVENT                       ].pDataOffset += TI_FIELD_OFFSET (EventMailBox_t, dbgEventRep);
	pEventMbox->CbTable[TWD_OWN_EVENT_SCAN_CMPLT            ].pDataOffset += TI_FIELD_OFFSET (EventMailBox_t,scanCompleteResults);
	pEventMbox->CbTable[TWD_OWN_EVENT_SPS_SCAN_CMPLT        ].pDataOffset += TI_FIELD_OFFSET (EventMailBox_t,scheduledScanAttendedChannels);
    pEventMbox->CbTable[TWD_OWN_EVENT_PERIODIC_SCAN_COMPLETE].pDataOffset += TI_FIELD_OFFSET (EventMailBox_t, scanCompleteResults);
    pEventMbox->CbTable[TWD_OWN_EVENT_PERIODIC_SCAN_REPORT  ].pDataOffset += TI_FIELD_OFFSET (EventMailBox_t, scanCompleteResults);
	pEventMbox->CbTable[TWD_OWN_EVENT_SOFT_GEMINI_SENSE     ].pDataOffset += TI_FIELD_OFFSET (EventMailBox_t,softGeminiSenseInfo);
	pEventMbox->CbTable[TWD_OWN_EVENT_SOFT_GEMINI_PREDIC    ].pDataOffset += TI_FIELD_OFFSET (EventMailBox_t,softGeminiProtectiveInfo);
	pEventMbox->CbTable[TWD_OWN_EVENT_SWITCH_CHANNEL_CMPLT  ].pDataOffset += TI_FIELD_OFFSET (EventMailBox_t,channelSwitchStatus);
	pEventMbox->CbTable[TWD_OWN_EVENT_PS_REPORT             ].pDataOffset += TI_FIELD_OFFSET (EventMailBox_t,psStatus);
}
    


static void eventMbox_DummyCb(TI_HANDLE hEventMbox)
{
	TEventMbox* pEventMbox = (TEventMbox*)hEventMbox;
TRACE0(pEventMbox->hReport, REPORT_SEVERITY_ERROR, "eventMbox_DummyCb : Called for unregistered event");
}


/*
 * \brief	Read mailbox address
 * 
 * \param  hEventMbox  - Handle to EventMbox
 * \param  fCb		   - CB function to return in Async mode
 * \param  hCb		   - CB Habdle 
 * \return TXN_STATUS_COMPLETE, TXN_STATUS_PENDING, TXN_STATUS_ERROR
 * 
 * \par Description
 * This function is called for initialize the Event MBOX addresses.
 * It issues a read transaction from the Twif with a CB.
 * 
 * \sa 
 */
TI_STATUS eventMbox_InitMboxAddr(TI_HANDLE hEventMbox, fnotify_t fCb, TI_HANDLE hCb)
{
	TTxnStruct  *pTxn;
	TEventMbox* pEventMbox;
	ETxnStatus  rc;
	pEventMbox = (TEventMbox*)hEventMbox;
	pTxn = &pEventMbox->iTxnGenRegSize.tTxnReg;

	/* Store the Callabck address of the modules that called us in case of Asynchronuous transaction that will complete later */
	pEventMbox->fCb = fCb;
    pEventMbox->hCb = hCb;

	/* Build the command TxnStruct */
    TXN_PARAM_SET(pTxn, TXN_LOW_PRIORITY, TXN_FUNC_ID_WLAN, TXN_DIRECTION_READ, TXN_INC_ADDR)
	/* Applying a CB in case of an async read */
    BUILD_TTxnStruct(pTxn, REG_EVENT_MAILBOX_PTR, &pEventMbox->iTxnGenRegSize.iRegBuffer, REGISTER_SIZE, eventMbox_ReadAddrCb, hEventMbox)
	rc = twIf_Transact(pEventMbox->hTwif,pTxn);
	if (rc == TXN_STATUS_COMPLETE)
	{
		pEventMbox->EventMboxAddr[0] = pEventMbox->iTxnGenRegSize.iRegBuffer;
		pEventMbox->EventMboxAddr[1] = pEventMbox->EventMboxAddr[0] + sizeof(EventMailBox_t);
TRACE3(pEventMbox->hReport, REPORT_SEVERITY_INIT , "eventMbox_ConfigHw: event A Address=0x%x, event B Address=0x%x, sizeof=%d\n", pEventMbox->EventMboxAddr[0], pEventMbox->EventMboxAddr[1], sizeof(EventMailBox_t));

	}
	return rc;
}


/*
 * \brief	Save the Event MBOX addresses
 * 
 * \param  hEventMbox  - Handle to EventMbox
 * \param  hTxn		   - Handle to TTxnStruct
 * \return none
 * 
 * \par Description
 * This function is called upon completion of thr read Event MBOX address register.
 * It save the addresses in EventMbox.
 * 
 * \sa 
 */
static void eventMbox_ReadAddrCb(TI_HANDLE hEventMbox, TI_HANDLE hTxn)
{
    TEventMbox* pEventMbox;
	
	pEventMbox = (TEventMbox*)hEventMbox;

	pEventMbox->EventMboxAddr[0] = pEventMbox->iTxnGenRegSize.iRegBuffer;
	pEventMbox->EventMboxAddr[1] = pEventMbox->EventMboxAddr[0] + sizeof(EventMailBox_t);
 
    TRACE3(pEventMbox->hReport, REPORT_SEVERITY_INIT , "eventMbox_ConfigHw: event A Address=0x%x, event B Address=0x%x, sizeof=%d\n", pEventMbox->EventMboxAddr[0], pEventMbox->EventMboxAddr[1], sizeof(EventMailBox_t));

	/* call back the module that called us before to read our self-address */
	pEventMbox->fCb(pEventMbox->hCb,TI_OK);
}


/*
 * \brief	confige the Mask vector in FW 
 * 
 * \param  hEventMbox  - Handle to EventMbox
 * \return none
 * 
 * \par Description
 * This function is called upon exit from init it will set the mask vector.
 * this function is mostly use for recovery
 * Note that at Init stage the FW is already configured to have all events masked but at Recovery stage 
 * The driver whishes to just set back previous event mask configuration
 * 
 * \sa 
 */
void eventMbox_InitComplete(TI_HANDLE hEventMbox)
{

    TEventMbox* pEventMbox;
	pEventMbox = (TEventMbox*)hEventMbox;
	
    TRACE1(pEventMbox->hReport, REPORT_SEVERITY_INFORMATION, "eventMbox_InitComplete: mask = 0x%x\n", pEventMbox->iTxnEventMbox.iEventMboxBuf.eventsMask);

	cmdBld_CfgEventMask(pEventMbox->hCmdBld,pEventMbox->iTxnEventMbox.iEventMboxBuf.eventsMask,NULL,NULL);
}
    


/*
 * \brief	Register an event 
 * 
 * \param  hEventMbox  - Handle to EventMbox
 * \param  EvID - the event ID to register
 * \param  fCb - CB function of the registered event
 * \param  hCb - CB handle of the registered event
 * \return TI_OK,TI_NOK
 *
 * \par Description
 * This function is called from the user upon request to register for event.
 * an Event can only be register to one user.
 * This function doesn't change the mask vector in FW!!!
 * 
 * \sa 
 */
TI_STATUS eventMbox_RegisterEvent(TI_HANDLE hEventMbox,TI_UINT32 EvID,void* fCb,TI_HANDLE hCb)
{
    TEventMbox *pEventMbox = (TEventMbox *)hEventMbox;
    if (fCb == NULL || hCb == NULL)
    { 
TRACE0(pEventMbox->hReport, REPORT_SEVERITY_ERROR, "eventMbox_RegisterEvent : NULL parameters\n");
        return TI_NOK;
    }
    if (EvID >= TWD_OWN_EVENT_ALL)
    {
TRACE0(pEventMbox->hReport, REPORT_SEVERITY_ERROR, "eventMbox_RegisterEvent : Event ID invalid\n");
		return TI_NOK;
    }
	pEventMbox->CbTable[EvID].fCb = fCb;
	pEventMbox->CbTable[EvID].hCb = hCb;
	return TI_OK;
}




/*
 * \brief	Replace event callback 
 * 
 * \param  hEventMbox  - Handle to EventMbox
 * \param  EvID - the event ID to register
 * \param  fNewCb - the new CB function of the registered event
 * \param  hNewCb - the new CB handle of the registered event
 * \param  pPrevCb - the old CB to save
 * \param  pPrevHndl - the old handle to save
 * \return TI_OK,TI_NOK
 * 
 * \par Description
 * Replace event callback function by another one.
 *              Provide the previous CB to the caller.
 *
 * \sa 
 */
TI_STATUS eventMbox_ReplaceEvent (TI_HANDLE hEventMbox,
                                    TI_UINT32   EvID, 
                                    void       *fNewCb, 
                                    TI_HANDLE   hNewCb,                                   
                                    void      **pPrevCb, 
                                    TI_HANDLE  *pPrevHndl)                                    
{
    TEventMbox *pEventMbox = (TEventMbox *)hEventMbox;
    if (fNewCb == NULL || hNewCb == NULL)
    { 
TRACE0(pEventMbox->hReport, REPORT_SEVERITY_ERROR , "eventMbox_ReplaceEvent: NULL parameters\n");
        return TI_NOK;
    }
    if (EvID >= TWD_OWN_EVENT_ALL)
    {
TRACE1(pEventMbox->hReport, REPORT_SEVERITY_ERROR, "eventMbox_ReplaceEvent: invalid ID. ID is %d\n", EvID);
        return TI_NOK;
    }

	/* Save the old CBs */
	*pPrevCb   = pEventMbox->CbTable[EvID].fCb;
	*pPrevHndl = pEventMbox->CbTable[EvID].hCb;

	/* store the new CBs */
	pEventMbox->CbTable[EvID].fCb = fNewCb;
	pEventMbox->CbTable[EvID].hCb = hNewCb;

    TRACE0(pEventMbox->hReport, REPORT_SEVERITY_INFORMATION, "eventMbox_ReplaceEvent: EVENT  has registered\n");
	return TI_OK;
}


/*
 * \brief	Un mask an event
 * 
 * \param  hEventMbox  - Handle to EventMbox
 * \param  EvID - the event ID to un mask
 * \param  fCb - CB function
 * \param  hCb - CB handle
 * \return TI_COMPLETE,TI_PENDING,TI_ERROR
 *
 * \par Description
 * This function is called from the user upon request to un mask an event.
 * This function change the mask vector in FW but doesn't register for it in the driver and 
 * doesn't set Cb function and Cb Handle in case of un mask event without registered for it an 
 * error will be handling!!!
 * 
 * \sa 
 */
TI_STATUS eventMbox_UnMaskEvent(TI_HANDLE hEventMbox,TI_UINT32 EvID,void* fCb,TI_HANDLE hCb)
{
	TI_UINT32*	pEventMask;
	TI_STATUS	aStatus;
    TEventMbox *pEventMbox = (TEventMbox *)hEventMbox;
	pEventMask = (TI_UINT32*)&pEventMbox->iTxnEventMbox.iEventMboxBuf.eventsMask;

    if (EvID >= TWD_OWN_EVENT_ALL)
    {
TRACE1(pEventMbox->hReport, REPORT_SEVERITY_ERROR, "eventMbox_UnMaskEvent : Un mask an Invalid event = 0x%x\n",EvID);
		return TXN_STATUS_ERROR;
    }
TRACE0(pEventMbox->hReport, REPORT_SEVERITY_INFORMATION, "eventMbox_UnMaskEvent : EVENT  is unmasked\n");

	*pEventMask &= ~eventTable[EvID].bitMask;

	aStatus = cmdBld_CfgEventMask (pEventMbox->hCmdBld, *pEventMask, fCb, hCb);
	return aStatus;
}


/*
 * \brief	mask an event
 *
 * \param  hEventMbox  - Handle to EventMbox
 * \param  EvID - the event ID to un mask
 * \param  fCb - CB function
 * \param  hCb - CB handle
 * \return TI_COMPLETE,TI_PENDING,TI_ERROR
 * 
 * \par Description
 * This function is called from the user upon request to mask an event.
 * This function change the mask vector in FW but doesn't unregister it in the driver. 
 * \sa 
 */
TI_STATUS eventMbox_MaskEvent(TI_HANDLE hEventMbox,TI_UINT32 EvID,void* fCb,TI_HANDLE hCb)
{
	TI_UINT32*	pEventMask;
	TI_STATUS	aStatus;
    TEventMbox *pEventMbox = (TEventMbox *)hEventMbox;
	pEventMask = (TI_UINT32*)&pEventMbox->iTxnEventMbox.iEventMboxBuf.eventsMask;
    
    if (EvID >= TWD_OWN_EVENT_ALL)
    {
TRACE1(pEventMbox->hReport, REPORT_SEVERITY_ERROR, "eventMbox_MaskEvent : Mask an Invalid event = 0x%x\n",EvID);
		return TXN_STATUS_ERROR;
    }

	*pEventMask |= eventTable[EvID].bitMask;

    TRACE0(pEventMbox->hReport, REPORT_SEVERITY_INFORMATION , "eventMbox_MaskEvent : EVENT  is masked\n");

	aStatus = cmdBld_CfgEventMask(pEventMbox->hCmdBld,*pEventMask,fCb,hCb);
	return aStatus;
}



/*
 * \brief	Handle the incoming event read the Mbox data
 * 
 * \param  hEventMbox  - Handle to EventMbox
 * \param  TFwStatus  - FW status
 * \return none
 *
 * \par Description
 * This function is called from the FW Event upon receiving MBOX event.
 * \sa 
 */
ETxnStatus eventMbox_Handle(TI_HANDLE hEventMbox,FwStatus_t* pFwStatus)
{
	ETxnStatus	rc;
	TTxnStruct  *pTxn;
    TEventMbox *pEventMbox = (TEventMbox *)hEventMbox;

	pTxn = &pEventMbox->iTxnEventMbox.tEventMbox;

TRACE1(pEventMbox->hReport, REPORT_SEVERITY_INFORMATION, "eventMbox_Handle : Reading from MBOX -- %d",pEventMbox->ActiveMbox);

#ifdef TI_DBG
    /* Check if missmatch MBOX */
	if (pEventMbox->ActiveMbox == 0)
    {
    	if (pFwStatus->intrStatus & ACX_INTR_EVENT_B)
        {
        TRACE0(pEventMbox->hReport, REPORT_SEVERITY_ERROR, "eventMbox_Handle : incorrect MBOX SW MBOX -- A FW MBOX -- B");
        }
    }
	else if (pEventMbox->ActiveMbox == 1)
    {
        if (pFwStatus->intrStatus & ACX_INTR_EVENT_A)
        {
            TRACE0(pEventMbox->hReport, REPORT_SEVERITY_ERROR, "eventMbox_Handle : incorrect MBOX SW MBOX -- B FW MBOX -- A");
        }
    }
#endif /* TI_DBG */
    
	if (pEventMbox->CurrentState != EVENT_MBOX_STATE_IDLE)
	{
TRACE0(pEventMbox->hReport, REPORT_SEVERITY_ERROR, "eventMbox_Handle : Receiving event not in Idle state");
    }
	pEventMbox->CurrentState = EVENT_MBOX_STATE_READING;

	/* Build the command TxnStruct */
    TXN_PARAM_SET(pTxn, TXN_LOW_PRIORITY, TXN_FUNC_ID_WLAN, TXN_DIRECTION_READ, TXN_INC_ADDR)
	/* Applying a CB in case of an async read */
    BUILD_TTxnStruct(pTxn, pEventMbox->EventMboxAddr[pEventMbox->ActiveMbox], &pEventMbox->iTxnEventMbox.iEventMboxBuf, sizeof(EventMailBox_t),(TTxnDoneCb)eventMbox_ReadCompleteCB, pEventMbox)
	rc = twIf_Transact(pEventMbox->hTwif,pTxn);

	pEventMbox->ActiveMbox = 1 - pEventMbox->ActiveMbox;
	if (rc == TXN_STATUS_COMPLETE)
    {   
		eventMbox_ReadCompleteCB(pEventMbox,pTxn);
    }

    return TXN_STATUS_COMPLETE;
}


/*
 * \brief	Process the event 
 *
 * \param  hEventMbox  - Handle to EventMbox
 * \param  pTxnStruct  - the Txn data
 * \return none
 * 
 * \par Description
 * This function is called from the upon reading completion of the event MBOX
 * it will call all registered event according to the pending bits in event MBOX vector.
 * \sa 
 */
static void eventMbox_ReadCompleteCB(TI_HANDLE hEventMbox, TTxnStruct *pTxnStruct)
{
	TI_UINT32	EvID;
	TTxnStruct*	pTxn;
    TEventMbox *pEventMbox = (TEventMbox *)hEventMbox;
	pTxn = &pEventMbox->iTxnGenRegSize.tTxnReg;

TRACE1(pEventMbox->hReport, REPORT_SEVERITY_INFORMATION, "eventMbox_ReadCompleteCB : event vector -- 0x%x\n",pEventMbox->iTxnEventMbox.iEventMboxBuf.eventsVector);
    
	pEventMbox->iTxnGenRegSize.iRegBuffer = INTR_TRIG_EVENT_ACK;

    for (EvID = 0; EvID < TWD_OWN_EVENT_ALL; EvID++)
    {
        if (pEventMbox->iTxnEventMbox.iEventMboxBuf.eventsVector & eventTable[EvID].bitMask)
                {                  
                    if (eventTable[EvID].dataLen)
                    {
                ((TEventMboxDataCb)pEventMbox->CbTable[EvID].fCb)(pEventMbox->CbTable[EvID].hCb,(TI_CHAR*)pEventMbox->CbTable[EvID].pDataOffset,eventTable[EvID].dataLen);
            }
            else
            {
                ((TEventMboxEvCb)pEventMbox->CbTable[EvID].fCb)(pEventMbox->CbTable[EvID].hCb);
            }
        }
    }     

    /* Check if the state is changed in the context of the event callbacks */
    if (pEventMbox->CurrentState == EVENT_MBOX_STATE_IDLE)
    {
        /*
         * When eventMbox_stop is called state is changed to IDLE
         * This is done in the context of the above events callbacks
         * Don't send the EVENT ACK transaction because the driver stop process includes power off
         */ 
        TRACE0(pEventMbox->hReport, REPORT_SEVERITY_WARNING, "eventMbox_ReadCompleteCB : State is IDLE ! don't send the EVENT ACK");
        return;
    }

	pEventMbox->CurrentState = EVENT_MBOX_STATE_IDLE;

	/* Build the command TxnStruct */
    TXN_PARAM_SET(pTxn, TXN_LOW_PRIORITY, TXN_FUNC_ID_WLAN, TXN_DIRECTION_WRITE, TXN_INC_ADDR)
	/* Applying a CB in case of an async read */
    BUILD_TTxnStruct(pTxn, ACX_REG_INTERRUPT_TRIG, &pEventMbox->iTxnGenRegSize.iRegBuffer, sizeof(pEventMbox->iTxnGenRegSize.iRegBuffer), NULL, NULL)
	twIf_Transact(pEventMbox->hTwif,pTxn);
}    


#ifdef TI_DBG

/*
 *  eventMbox_Print: print the Event Mailbox statistic :Number 890
 */
TI_STATUS eventMbox_Print (TI_HANDLE hEventMbox)
{
    TEventMbox *pEventMbox = (TEventMbox *)hEventMbox;
    TI_UINT32 i;
    TI_UINT32 EvMask   = pEventMbox->iTxnEventMbox.iEventMboxBuf.eventsMask;
    TI_UINT32 EvVector = pEventMbox->iTxnEventMbox.iEventMboxBuf.eventsVector;

    TRACE0(pEventMbox->hReport, REPORT_SEVERITY_CONSOLE, "------------------------- EventMbox  Print ----------------------------\n");

    TRACE1(pEventMbox->hReport, REPORT_SEVERITY_INFORMATION, " eventMbox_HandleEvent: Event Vector = 0x%x\n", EvVector);
    TRACE1(pEventMbox->hReport, REPORT_SEVERITY_INFORMATION, " eventMbox_HandleEvent: Event Mask = 0x%x\n", EvMask);
    TRACE1(pEventMbox->hReport, REPORT_SEVERITY_CONSOLE, " Total Number Of Compound Event = %d: \n", pEventMbox->uCompounEvCount);
    TRACE1(pEventMbox->hReport, REPORT_SEVERITY_CONSOLE, " Total Number Of Events = %d: \n", pEventMbox->uTotalEvCount);
    TRACE0(pEventMbox->hReport, REPORT_SEVERITY_CONSOLE, "\t\t\t\t *** Event Counters *** :\n");
    for (i = 0; i < TWD_OWN_EVENT_ALL; i++)
    {
        TRACE2(pEventMbox->hReport, REPORT_SEVERITY_CONSOLE, " %d) Event Name = EVENT , Number of Event = %d\n", i, pEventMbox->CbTable[i].uCount);
    }

    TRACE0(pEventMbox->hReport, REPORT_SEVERITY_CONSOLE, "------------------------- EventMbox  Print End ----------------------------\n");

    return TI_OK;
}


#endif /* TI_DBG */



