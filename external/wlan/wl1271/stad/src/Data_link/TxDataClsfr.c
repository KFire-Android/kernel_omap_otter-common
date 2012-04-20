/*
 * TxDataClsfr.c
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


/** \file   txDataClsfr.c 
 *  \brief  The Tx Data Classifier sub-module (under txDataQueue module).
 *  
 *  \see    txDataQueue.h (the classifier uses the same object as txDataQueue)
 */

#define __FILE_ID__  FILE_ID_59
#include "paramOut.h"
#include "osApi.h"
#include "report.h"
#include "context.h"
#include "Ethernet.h"
#include "TWDriver.h"
#include "txDataQueue.h"



/** 
 * \fn     txDataClsfr_Config 
 * \brief  Configure the classifier paramters
 * 
 * Configure the classifier parameters according to the init parameters.
 * Called from the txDataQueue configuration function.
 * 
 * \note   
 * \param  hTxDataQ     - The object handle                                         
 * \param  pClsfrInitParams - Pointer to the classifier init params
 * \return TI_OK on success or TI_NOK on failure 
 * \sa     
 */ 
TI_STATUS txDataClsfr_Config (TI_HANDLE hTxDataQ, TClsfrParams *pClsfrInitParams)
{
    TTxDataQ     *pTxDataQ = (TTxDataQ *)hTxDataQ;
    TClsfrParams *pParams  = &pTxDataQ->tClsfrParams; /* where to save the new params */
    TI_UINT32     uActualEntryCount;
    TI_UINT32     i, j;
    TI_BOOL       bConflictFound;
    
    /* Active classification algorithm */
    pParams->eClsfrType = pClsfrInitParams->eClsfrType;

    /* the number of active entries */
    if (pClsfrInitParams->uNumActiveEntries <= NUM_OF_CLSFR_TABLE_ENTRIES)
        pParams->uNumActiveEntries = pClsfrInitParams->uNumActiveEntries;
    else 
        pParams->uNumActiveEntries = NUM_OF_CLSFR_TABLE_ENTRIES;

    /* Initialization of the classification table */
    switch (pParams->eClsfrType)
    {
        case D_TAG_CLSFR:
			pParams->uNumActiveEntries = 0;
        break;
        
        case DSCP_CLSFR:
            uActualEntryCount=0;
            for (i = 0; i < pParams->uNumActiveEntries; i++)
            {
               bConflictFound = TI_FALSE;
                /* check conflict */
                for (j = 0; j < i; j++)
                {
                   /* Detect both duplicate and conflicting entries */
                    if (pParams->ClsfrTable[j].Dscp.CodePoint == pClsfrInitParams->ClsfrTable[i].Dscp.CodePoint)
                    {
                        TRACE0(pTxDataQ->hReport, REPORT_SEVERITY_WARNING , "ERROR: txDataClsfr_Config(): duplicate/conflicting classifier entries\n");
                        bConflictFound = TI_TRUE;
                    }
                }
                if (bConflictFound == TI_FALSE)
                {
                  pParams->ClsfrTable[uActualEntryCount].Dscp.CodePoint = pClsfrInitParams->ClsfrTable[i].Dscp.CodePoint;
                  pParams->ClsfrTable[uActualEntryCount].DTag = pClsfrInitParams->ClsfrTable[i].DTag;
                  uActualEntryCount++;
                }
            }
            pParams->uNumActiveEntries = uActualEntryCount;
        break;

        case PORT_CLSFR:
           uActualEntryCount=0;
            for (i = 0; (i < pParams->uNumActiveEntries) ; i++)
            {
				bConflictFound = TI_FALSE;
                /* check conflict */
                for (j = 0; j < i; j++)
                {
                    /* Detect both duplicate and conflicting entries */
                    if (pParams->ClsfrTable[j].Dscp.DstPortNum == pClsfrInitParams->ClsfrTable[i].Dscp.DstPortNum)
                    {
                        TRACE0(pTxDataQ->hReport, REPORT_SEVERITY_WARNING , "ERROR: txDataClsfr_Config(): classifier entries conflict\n");
                        bConflictFound = TI_TRUE;
                    }
                }
                if (bConflictFound == TI_FALSE)
                {
                  pParams->ClsfrTable[uActualEntryCount].Dscp.DstPortNum = pClsfrInitParams->ClsfrTable[i].Dscp.DstPortNum;
                  pParams->ClsfrTable[uActualEntryCount].DTag = pClsfrInitParams->ClsfrTable[i].DTag;
                  uActualEntryCount++;
                }
            }
            pParams->uNumActiveEntries = uActualEntryCount;
        break;  

        case IPPORT_CLSFR:
           uActualEntryCount=0;
            for (i=0; (i < pParams->uNumActiveEntries ) ; i++)
            {
				bConflictFound = TI_FALSE;
                /* check conflict */
                for (j = 0; j < i; j++)
                {
                   /* Detect both duplicate and conflicting entries */
                    if ((pParams->ClsfrTable[j].Dscp.DstIPPort.DstIPAddress == pClsfrInitParams->ClsfrTable[i].Dscp.DstIPPort.DstIPAddress)&& 
                    (pParams->ClsfrTable[j].Dscp.DstIPPort.DstPortNum == pClsfrInitParams->ClsfrTable[i].Dscp.DstIPPort.DstPortNum))
                    {
                        TRACE0(pTxDataQ->hReport, REPORT_SEVERITY_WARNING , "ERROR: txDataClsfr_Config(): classifier entries conflict\n");
                        bConflictFound = TI_TRUE;
                    }
                }
                if (bConflictFound == TI_FALSE)
                {
                  pParams->ClsfrTable[uActualEntryCount].Dscp.DstIPPort.DstIPAddress = pClsfrInitParams->ClsfrTable[i].Dscp.DstIPPort.DstIPAddress;
                  pParams->ClsfrTable[uActualEntryCount].Dscp.DstIPPort.DstPortNum = pClsfrInitParams->ClsfrTable[i].Dscp.DstIPPort.DstPortNum;
                  pParams->ClsfrTable[uActualEntryCount].DTag = pClsfrInitParams->ClsfrTable[i].DTag;
                  uActualEntryCount++;
                }
            }
            pParams->uNumActiveEntries = uActualEntryCount;
        break;  

        default:
            TRACE0(pTxDataQ->hReport, REPORT_SEVERITY_WARNING , "ERROR: txDataClsfr_Config(): Classifier type -- unknown --> set to D-Tag\n");
            pParams->eClsfrType = D_TAG_CLSFR;
			pParams->uNumActiveEntries = 0;
        break;  
    }
    
    return TI_OK;
}


/** 
 * \fn     getIpAndUdpHeader 
 * \brief  Get IP & UDP headers addresses if exist
 * 
 * This function gets the addresses of the IP and UDP headers
 *
 * \note   A local inline function!
 * \param  pTxDataQ    - The object handle                                         
 * \param  pPktCtrlBlk - Pointer to the packet
 * \param  pIpHeader   - Pointer to pointer to IP header
 * \param  pUdpHeader  - Pointer to pointer to UDP header 
 * \return TI_OK on success, TI_NOK if it's not an IP packet
 * \sa     
 */ 
static inline TI_STATUS getIpAndUdpHeader(TTxDataQ   *pTxDataQ, 
                                          TTxCtrlBlk *pPktCtrlBlk,
                                          TI_UINT8  **pIpHeader, 
                                          TI_UINT8  **pUdpHeader)
{
    TI_UINT8 *pEthHead = pPktCtrlBlk->tTxnStruct.aBuf[0];
	TI_UINT8  ipHeaderLen = 0;
  
	/* check if frame is IP according to ether type */
    if( ( HTOWLANS(((TEthernetHeader *)pEthHead)->type) ) != ETHERTYPE_IP)
    {
        TRACE0(pTxDataQ->hReport, REPORT_SEVERITY_INFORMATION, " getIpAndUdpHeader: EthTypeLength is not 0x0800 \n");
        return TI_NOK;
    }

    /* set the pointer to the beginning of the IP header and calculate it's size */
    *pIpHeader  = pPktCtrlBlk->tTxnStruct.aBuf[1];
    ipHeaderLen = ((*(unsigned char*)(*pIpHeader) & 0x0f) * 4);

    /* Set the pointer to the beggining of the TCP/UDP header */
    if (ipHeaderLen == pPktCtrlBlk->tTxnStruct.aLen[1])
    {
        *pUdpHeader = pPktCtrlBlk->tTxnStruct.aBuf[2];  
    }
    else 
    {
        *pUdpHeader = *pIpHeader + ipHeaderLen;  
    }

    return TI_OK;
}


/** 
 * \fn     txDataClsfr_ClassifyTxPacket 
 * \brief  Configure the classifier paramters
 * 
 * This function classifies the given Tx packet according to the classifier parameters. 
 * It sets the TID field with the classification result.
 * The classification is according to one of the following methods:
 * - D-Tag  - Transparent (TID = Dtag)
 * - DSCP   - According to the DSCP field in the IP header - the default method!
 * - Dest UDP-Port   
 * - Dest IP-Addr & UDP-Port
 *
 * \note   
 * \param  hTxDataQ    - The object handle                                         
 * \param  pPktCtrlBlk - Pointer to the classified packet
 * \param  uPacketDtag - The packet priority optionaly set by the OAL
 * \return TI_OK on success, PARAM_VALUE_NOT_VALID in case of input parameters problems.
 * \sa     
 */ 
TI_STATUS txDataClsfr_ClassifyTxPacket (TI_HANDLE hTxDataQ, TTxCtrlBlk *pPktCtrlBlk, TI_UINT8 uPacketDtag)
{
    TTxDataQ     *pTxDataQ = (TTxDataQ *)hTxDataQ;
    TClsfrParams *pClsfrParams = &pTxDataQ->tClsfrParams;
    TI_UINT8     *pUdpHeader = NULL;
    TI_UINT8     *pIpHeader = NULL;
    TI_UINT8   uDscp;
    TI_UINT16  uDstUdpPort;
    TI_UINT32  uDstIpAdd;
    TI_UINT32  i;

    pPktCtrlBlk->tTxDescriptor.tid = 0;

    switch(pClsfrParams->eClsfrType)
    {
            /* Trivial mapping D-tag to D-tag */
        case D_TAG_CLSFR:
            if (uPacketDtag > MAX_NUM_OF_802_1d_TAGS)
            {
                TRACE0(pTxDataQ->hReport, REPORT_SEVERITY_ERROR , "txDataClsfr_ClassifyTxPacket(): uPacketDtag error\n");
                return PARAM_VALUE_NOT_VALID;
            }
            pPktCtrlBlk->tTxDescriptor.tid = uPacketDtag;
            TRACE1(pTxDataQ->hReport, REPORT_SEVERITY_INFORMATION , "Classifier D_TAG_CLSFR. uPacketDtag = %d\n", uPacketDtag);
        break;

        case DSCP_CLSFR:
            if( (getIpAndUdpHeader(pTxDataQ, pPktCtrlBlk, &pIpHeader, &pUdpHeader) != TI_OK) 
                || (pIpHeader == NULL) )
            {
                TRACE0(pTxDataQ->hReport, REPORT_SEVERITY_INFORMATION , "txDataClsfr_ClassifyTxPacket(): DSCP clsfr, getIpAndUdpHeader mismatch\n");
				return PARAM_VALUE_NOT_VALID; 
            }

            /* DSCP to D-tag mapping */
            uDscp =  *((TI_UINT8 *)(pIpHeader + 1)); /* Fetching the DSCP from the header */
            uDscp = (uDscp >> 2);
            
            /* looking for the specific DSCP, if found, its corresponding D-tag is set to the TID */
            for(i = 0; i < pClsfrParams->uNumActiveEntries; i++)
            {
                if (pClsfrParams->ClsfrTable[i].Dscp.CodePoint == uDscp)
				{
                    pPktCtrlBlk->tTxDescriptor.tid = pClsfrParams->ClsfrTable[i].DTag;
                    TRACE2(pTxDataQ->hReport, REPORT_SEVERITY_INFORMATION , "Classifier DSCP_CLSFR found match - entry %d - Tid = %d\n",i,pPktCtrlBlk->tTxDescriptor.tid);
					break;
				}
            }
        break;

        case PORT_CLSFR:
            if( (getIpAndUdpHeader(pTxDataQ, pPktCtrlBlk, &pIpHeader, &pUdpHeader) != TI_OK) ||
                (pUdpHeader == NULL) )
            {
                TRACE0(pTxDataQ->hReport, REPORT_SEVERITY_INFORMATION, " txDataClsfr_ClassifyTxPacket() : DstPort clsfr, getIpAndUdpHeader error\n");
                return PARAM_VALUE_NOT_VALID; 
            }

            uDstUdpPort = *((TI_UINT16 *)(pUdpHeader + 2));
            uDstUdpPort = HTOWLANS(uDstUdpPort);
            
            /* Looking for the specific port number. If found, its corresponding D-tag is set to the TID. */
            for(i = 0; i < pClsfrParams->uNumActiveEntries; i++)
            {
                if (pClsfrParams->ClsfrTable[i].Dscp.DstPortNum == uDstUdpPort)
				{
                    pPktCtrlBlk->tTxDescriptor.tid = pClsfrParams->ClsfrTable[i].DTag;
                    TRACE2(pTxDataQ->hReport, REPORT_SEVERITY_INFORMATION , "Classifier PORT_CLSFR found match - entry %d - Tid = %d\n", i, pPktCtrlBlk->tTxDescriptor.tid);
					break;
				}
            }
        break;

        case IPPORT_CLSFR: 
            if ( (getIpAndUdpHeader(pTxDataQ, pPktCtrlBlk, &pIpHeader, &pUdpHeader) != TI_OK) 
                 || (pIpHeader == NULL) || (pUdpHeader == NULL) )
            {
                TRACE0(pTxDataQ->hReport, REPORT_SEVERITY_INFORMATION, "txDataClsfr_ClassifyTxPacket(): Dst IP&Port clsfr, getIpAndUdpHeader error\n");
                return PARAM_VALUE_NOT_VALID; 
            }

            uDstUdpPort = *((TI_UINT16 *)(pUdpHeader + 2));
            uDstUdpPort = HTOWLANS(uDstUdpPort);
            uDstIpAdd = *((TI_UINT32 *)(pIpHeader + 16));
            
            /* 
             * Looking for the specific pair of dst IP address and dst port number.
             * If found, its corresponding D-tag is set to the TID.                                                         
             */
            for(i = 0; i < pClsfrParams->uNumActiveEntries; i++)
            {
                if ((pClsfrParams->ClsfrTable[i].Dscp.DstIPPort.DstIPAddress == uDstIpAdd) &&
                    (pClsfrParams->ClsfrTable[i].Dscp.DstIPPort.DstPortNum == uDstUdpPort))
				{
                    pPktCtrlBlk->tTxDescriptor.tid = pClsfrParams->ClsfrTable[i].DTag;
                    TRACE2(pTxDataQ->hReport, REPORT_SEVERITY_INFORMATION , "Classifier IPPORT_CLSFR found match - entry %d - Tid = %d\n", i, pPktCtrlBlk->tTxDescriptor.tid);
					break;
				}
            }
        break;
        
        default:
            TRACE0(pTxDataQ->hReport, REPORT_SEVERITY_ERROR, "txDataClsfr_ClassifyTxPacket(): eClsfrType error\n");
    }

    return TI_OK;
}


/** 
 * \fn     txDataClsfr_InsertClsfrEntry 
 * \brief  Insert a new entry to classifier table
 * 
 * Add a new entry to the classification table.
 * If the new entry is invalid or conflicts with existing entries, the operation is canceled.
 *
 * \note   
 * \param  hTxDataQ - The object handle                                         
 * \param  pNewEntry    - Pointer to the new entry to insert
 * \return TI_OK on success, PARAM_VALUE_NOT_VALID in case of input parameters problems.
 * \sa     txDataClsfr_RemoveClsfrEntry
 */ 
TI_STATUS txDataClsfr_InsertClsfrEntry(TI_HANDLE hTxDataQ, TClsfrTableEntry *pNewEntry)
{
    TTxDataQ      *pTxDataQ     = (TTxDataQ *)hTxDataQ;
    TClsfrParams  *pClsfrParams = &pTxDataQ->tClsfrParams;
    TI_UINT32     i;

	if(pNewEntry == NULL)
    {
        TRACE0(pTxDataQ->hReport, REPORT_SEVERITY_ERROR, "Classifier_InsertClsfrEntry(): NULL ConfigBuffer pointer Error - Aborting\n");
		return PARAM_VALUE_NOT_VALID;
    }
    
    /* If no available entries, exit */
    if (pClsfrParams->uNumActiveEntries == NUM_OF_CLSFR_TABLE_ENTRIES)
    {
        TRACE0(pTxDataQ->hReport, REPORT_SEVERITY_ERROR, "Classifier_InsertClsfrEntry(): Bad Number Of Entries - Aborting\n");
        return PARAM_VALUE_NOT_VALID;
    }
    
    if (pClsfrParams->eClsfrType == D_TAG_CLSFR)
    {
        TRACE0(pTxDataQ->hReport, REPORT_SEVERITY_ERROR, "Classifier_InsertClsfrEntry(): D-Tag classifier - Aborting\n");
        return PARAM_VALUE_NOT_VALID;
    }

    /* Check new entry and conflict with existing entries and if OK, insert to classifier table */
    switch (pClsfrParams->eClsfrType)
    {
        case DSCP_CLSFR:
    
            /* Check entry */
            if ( (pNewEntry->Dscp.CodePoint > CLASSIFIER_CODE_POINT_MAX) || 
                 (pNewEntry->DTag > CLASSIFIER_DTAG_MAX) ) 
            {
                TRACE0(pTxDataQ->hReport, REPORT_SEVERITY_ERROR, "Classifier_InsertClsfrEntry(): bad parameter - Aborting\n");
                return PARAM_VALUE_NOT_VALID;
            }
            
            /* Check conflict*/
            for (i = 0; i < pClsfrParams->uNumActiveEntries; i++)
            {
               /* Detect both duplicate and conflicting entries */
                if (pClsfrParams->ClsfrTable[i].Dscp.CodePoint == pNewEntry->Dscp.CodePoint)
                {
                    TRACE0(pTxDataQ->hReport, REPORT_SEVERITY_ERROR, "Classifier_InsertClsfrEntry(): classifier entries conflict - Aborting\n");
                    return PARAM_VALUE_NOT_VALID;
                }
            } 

            /* Insert new entry to classifier table. */
            /* Note: Protect from txDataClsfr_ClassifyTxPacket context preemption. */
            context_EnterCriticalSection (pTxDataQ->hContext);
            pClsfrParams->ClsfrTable[pClsfrParams->uNumActiveEntries].Dscp.CodePoint = pNewEntry->Dscp.CodePoint;
            pClsfrParams->ClsfrTable[pClsfrParams->uNumActiveEntries].DTag = pNewEntry->DTag;
            context_LeaveCriticalSection (pTxDataQ->hContext);
            
        break;
        
        case PORT_CLSFR:

            /* Check entry */
            if ((pNewEntry->DTag > CLASSIFIER_DTAG_MAX) || 
                (pNewEntry->Dscp.DstPortNum > CLASSIFIER_PORT_MAX-1) || 
                (pNewEntry->Dscp.DstPortNum < CLASSIFIER_PORT_MIN) )
            {
                TRACE0(pTxDataQ->hReport, REPORT_SEVERITY_ERROR, "Classifier_InsertClsfrEntry(): bad parameter - Aborting\n");
                return PARAM_VALUE_NOT_VALID;
            }
            
            /* Check conflict*/
            for (i = 0; i < pClsfrParams->uNumActiveEntries; i++)
            {
               /* Detect both duplicate and conflicting entries */
                if ((pClsfrParams->ClsfrTable[i].Dscp.DstPortNum == pNewEntry->Dscp.DstPortNum))
                {
                    TRACE0(pTxDataQ->hReport, REPORT_SEVERITY_ERROR, "Classifier_InsertClsfrEntry(): classifier entries conflict - Aborting\n");
                     return PARAM_VALUE_NOT_VALID;
                }
            }     

            /* Insert new entry to classifier table. */            
            /* Note: Protect from txDataClsfr_ClassifyTxPacket context preemption. */
            context_EnterCriticalSection (pTxDataQ->hContext);
            pClsfrParams->ClsfrTable[pClsfrParams->uNumActiveEntries].Dscp.DstPortNum = pNewEntry->Dscp.DstPortNum;
            pClsfrParams->ClsfrTable[pClsfrParams->uNumActiveEntries].DTag = pNewEntry->DTag;
            context_LeaveCriticalSection (pTxDataQ->hContext);
                                
        break;
        
        case IPPORT_CLSFR:

            /* Check entry */
            if ( (pNewEntry->DTag > CLASSIFIER_DTAG_MAX) || 
                 (pNewEntry->Dscp.DstIPPort.DstPortNum > CLASSIFIER_PORT_MAX-1) || 
                 (pNewEntry->Dscp.DstIPPort.DstPortNum < CLASSIFIER_PORT_MIN) || 
                 (pNewEntry->Dscp.DstIPPort.DstIPAddress > CLASSIFIER_IPADDRESS_MAX-1) || 
                 (pNewEntry->Dscp.DstIPPort.DstIPAddress < CLASSIFIER_IPADDRESS_MIN+1) )
            {
                TRACE0(pTxDataQ->hReport, REPORT_SEVERITY_ERROR, "Classifier_InsertClsfrEntry(): bad parameter - Aborting\n");
                return PARAM_VALUE_NOT_VALID;
            }

            /* Check conflict*/
            for (i = 0; i < pClsfrParams->uNumActiveEntries; i++)
            {
               /* Detect both duplicate and conflicting entries */
                if ( (pClsfrParams->ClsfrTable[i].Dscp.DstIPPort.DstIPAddress == pNewEntry->Dscp.DstIPPort.DstIPAddress) && 
                     (pClsfrParams->ClsfrTable[i].Dscp.DstIPPort.DstPortNum == pNewEntry->Dscp.DstIPPort.DstPortNum))
                {
                    TRACE0(pTxDataQ->hReport, REPORT_SEVERITY_ERROR, "Classifier_InsertClsfrEntry(): classifier entries conflict - Aborting\n");
                     return PARAM_VALUE_NOT_VALID;
                }
            }  

            /* Insert new entry to classifier table */            
            /* Note: Protect from txDataClsfr_ClassifyTxPacket context preemption. */
            context_EnterCriticalSection (pTxDataQ->hContext);
            pClsfrParams->ClsfrTable[pClsfrParams->uNumActiveEntries].Dscp.DstIPPort.DstIPAddress = pNewEntry->Dscp.DstIPPort.DstIPAddress;
            pClsfrParams->ClsfrTable[pClsfrParams->uNumActiveEntries].Dscp.DstIPPort.DstPortNum = pNewEntry->Dscp.DstIPPort.DstPortNum;
            pClsfrParams->ClsfrTable[pClsfrParams->uNumActiveEntries].DTag = pNewEntry->DTag;
            context_LeaveCriticalSection (pTxDataQ->hContext);

        break;

        default:
TRACE0(pTxDataQ->hReport, REPORT_SEVERITY_ERROR, "Classifier_InsertClsfrEntry(): Classifier type -- unknown - Aborting\n");
            
    } 
    
    /* Increment the number of classifier active entries */
    pClsfrParams->uNumActiveEntries++;

    return TI_OK;
}


/** 
 * \fn     txDataClsfr_RemoveClsfrEntry 
 * \brief  Remove an entry from classifier table
 * 
 * Remove an entry from classifier table.
 *
 * \note   
 * \param  hTxDataQ - The object handle                                         
 * \param  pRemEntry    - Pointer to the entry to remove
 * \return TI_OK on success, PARAM_VALUE_NOT_VALID in case of input parameters problems.
 * \sa     txDataClsfr_InsertClsfrEntry
 */ 
TI_STATUS txDataClsfr_RemoveClsfrEntry(TI_HANDLE hTxDataQ, TClsfrTableEntry *pRemEntry)
{
    TTxDataQ      *pTxDataQ     = (TTxDataQ *)hTxDataQ;
    TClsfrParams  *pClsfrParams = &pTxDataQ->tClsfrParams;
    TI_UINT32     i, j;

	if(pRemEntry == NULL)
    {
        TRACE0(pTxDataQ->hReport, REPORT_SEVERITY_ERROR, "classifier_RemoveClsfrEntry(): NULL ConfigBuffer pointer Error - Aborting\n");
		return PARAM_VALUE_NOT_VALID;
    }
    
    if (pClsfrParams->uNumActiveEntries == 0)
    {
        TRACE0(pTxDataQ->hReport, REPORT_SEVERITY_ERROR, "classifier_RemoveClsfrEntry(): Classifier table is empty - Aborting\n");
		return PARAM_VALUE_NOT_VALID;
    }
   
    if (pClsfrParams->eClsfrType == D_TAG_CLSFR)
    {
        TRACE0(pTxDataQ->hReport, REPORT_SEVERITY_ERROR, "classifier_RemoveClsfrEntry(): D-Tag classifier - Aborting\n");
        return PARAM_VALUE_NOT_VALID;
    }

    /* Check conflicts with classifier table entries */
    /* check all conflicts, if all entries are TI_OK --> insert to classifier table*/
 
    switch (pClsfrParams->eClsfrType)
    {
        case DSCP_CLSFR:

           /* Find the classifier entry */
           i = 0;
            while ((i < pClsfrParams->uNumActiveEntries) &&
                  ((pClsfrParams->ClsfrTable[i].Dscp.CodePoint != pRemEntry->Dscp.CodePoint) ||
                  (pClsfrParams->ClsfrTable[i].DTag != pRemEntry->DTag)))
            {   
              i++;
            }

           /* If we have reached the number of active entries, it means we couldn't find the requested entry */
            if (i == pClsfrParams->uNumActiveEntries)
           {
                TRACE0(pTxDataQ->hReport, REPORT_SEVERITY_ERROR, "classifier_RemoveClsfrEntry(): Entry not found - Aborting\n");
               return PARAM_VALUE_NOT_VALID;
           }

            /* Shift all entries above the removed one downward */
            /* Note: Protect from txDataClsfr_ClassifyTxPacket context preemption. */
            context_EnterCriticalSection (pTxDataQ->hContext);
            for (j = i; j < pClsfrParams->uNumActiveEntries - 1; j++)
                {
                   /* Move entries */
                pClsfrParams->ClsfrTable[j].Dscp.CodePoint = pClsfrParams->ClsfrTable[j+1].Dscp.CodePoint;
                pClsfrParams->ClsfrTable[j].DTag = pClsfrParams->ClsfrTable[j+1].DTag;
                } 
            context_LeaveCriticalSection (pTxDataQ->hContext);
          
        break;
        
        case PORT_CLSFR:

           /* Find the classifier entry */
           i = 0;
            while ((i < pClsfrParams->uNumActiveEntries) &&
                  ((pClsfrParams->ClsfrTable[i].Dscp.DstPortNum != pRemEntry->Dscp.DstPortNum) ||
                  (pClsfrParams->ClsfrTable[i].DTag != pRemEntry->DTag)))
            {   
              i++;
            }

           /* If we have reached the number of active entries, it means we couldn't find the requested entry */
            if (i == pClsfrParams->uNumActiveEntries)
           {
                TRACE0(pTxDataQ->hReport, REPORT_SEVERITY_ERROR, "classifier_RemoveClsfrEntry(): Entry not found - Aborting\n");
               return PARAM_VALUE_NOT_VALID;
           }

            /* Shift all entries above the removed one downward */
            /* Note: Protect from txDataClsfr_ClassifyTxPacket context preemption. */
            context_EnterCriticalSection (pTxDataQ->hContext);
			for (j = i; j < pClsfrParams->uNumActiveEntries - 1; j++)
                {
                pClsfrParams->ClsfrTable[j].Dscp.DstPortNum = pClsfrParams->ClsfrTable[j+1].Dscp.DstPortNum;
                pClsfrParams->ClsfrTable[j].DTag = pClsfrParams->ClsfrTable[j+1].DTag;
            } 
            context_LeaveCriticalSection (pTxDataQ->hContext);
                                
        break;
        
        case IPPORT_CLSFR:

            /* Find the classifier entry */
            i = 0;
            while ((i < pClsfrParams->uNumActiveEntries) &&
                  ((pClsfrParams->ClsfrTable[i].Dscp.DstIPPort.DstIPAddress != pRemEntry->Dscp.DstIPPort.DstIPAddress) ||
                  (pClsfrParams->ClsfrTable[i].Dscp.DstIPPort.DstPortNum != pRemEntry->Dscp.DstIPPort.DstPortNum) ||
                  (pClsfrParams->ClsfrTable[i].DTag != pRemEntry->DTag)))
            {   
                i++;
            }

            /* If we have reached the number of active entries, it means we couldn't find the requested entry */
            if (i == pClsfrParams->uNumActiveEntries)
            {
                TRACE0(pTxDataQ->hReport, REPORT_SEVERITY_ERROR, "classifier_RemoveClsfrEntry(): Entry not found - Aborting\n");
                return PARAM_VALUE_NOT_VALID;
            }

            /* Shift all entries above the removed one downward. */
            /* Note: Protect from txDataClsfr_ClassifyTxPacket context preemption. */
            context_EnterCriticalSection (pTxDataQ->hContext);
			for (j = i; j < pClsfrParams->uNumActiveEntries - 1; j++)
            {
                pClsfrParams->ClsfrTable[j].Dscp.DstIPPort.DstIPAddress = pClsfrParams->ClsfrTable[j+1].Dscp.DstIPPort.DstIPAddress;
                pClsfrParams->ClsfrTable[j].Dscp.DstIPPort.DstPortNum = pClsfrParams->ClsfrTable[j+1].Dscp.DstIPPort.DstPortNum;
                pClsfrParams->ClsfrTable[j].DTag = pClsfrParams->ClsfrTable[j+1].DTag;
            } 
            context_LeaveCriticalSection (pTxDataQ->hContext);

        break;

        default:
TRACE0(pTxDataQ->hReport, REPORT_SEVERITY_ERROR, "classifier_RemoveClsfrEntry(): Classifier type -- unknown - Aborting\n");
    } 
    
    /* Decrement the number of classifier active entries */
    pClsfrParams->uNumActiveEntries--;

    return TI_OK;
}


/** 
 * \fn     txDataClsfr_SetClsfrType & txDataClsfr_GetClsfrType 
 * \brief  Set / Get classifier type
 * 
 * Set / Get classifier type.
 * When setting type, the table is emptied!
 *
 * \note   
 * \param  hTxDataQ  - The object handle                                         
 * \param  eNewClsfrType - New type
 * \return TI_OK on success, PARAM_VALUE_NOT_VALID in case of input parameters problems.
 * \sa     
 */ 
TI_STATUS txDataClsfr_SetClsfrType (TI_HANDLE hTxDataQ, EClsfrType eNewClsfrType)
{
    TTxDataQ *pTxDataQ = (TTxDataQ *)hTxDataQ;

	if (eNewClsfrType > CLSFR_TYPE_MAX)
	{
TRACE0(pTxDataQ->hReport, REPORT_SEVERITY_ERROR, "Classifier_setClsfrType(): classifier type exceed its MAX \n");
		return PARAM_VALUE_NOT_VALID;
    }
	
	if (pTxDataQ->tClsfrParams.eClsfrType == eNewClsfrType)
	{
TRACE0(pTxDataQ->hReport, REPORT_SEVERITY_WARNING, "Classifier_setClsfrType(): equal classifier type --> will empty classifier table \n");
    }
	
	/* Update type and empty table. */
    /* Note: Protect from txDataClsfr_ClassifyTxPacket context preemption. */
    context_EnterCriticalSection (pTxDataQ->hContext);
    pTxDataQ->tClsfrParams.eClsfrType = eNewClsfrType;
	pTxDataQ->tClsfrParams.uNumActiveEntries = 0;
    context_LeaveCriticalSection (pTxDataQ->hContext);

    return TI_OK;
}

TI_STATUS txDataClsfr_GetClsfrType (TI_HANDLE hTxDataQ, EClsfrType *pClsfrType)
{
    TTxDataQ *pTxDataQ = (TTxDataQ *)hTxDataQ;

	*pClsfrType = pTxDataQ->tClsfrParams.eClsfrType;
	return TI_OK;
}



#ifdef TI_DBG

/** 
 * \fn     txDataClsfr_PrintClsfrTable 
 * \brief  Print classifier table
 * 
 * Print the classifier table for debug
 *
 * \note   
 * \param  hTxDataQ  - The object handle                                         
 * \return void
 * \sa     
 */ 
void txDataClsfr_PrintClsfrTable (TI_HANDLE hTxDataQ)
{
    TTxDataQ      *pTxDataQ     = (TTxDataQ *)hTxDataQ;
    TClsfrParams  *pClsfrParams = &pTxDataQ->tClsfrParams;
	TI_UINT32      uIpAddr, i;

	if (pClsfrParams->eClsfrType == D_TAG_CLSFR)
	{
		WLAN_OS_REPORT (("D_TAG classifier type selected...Nothing to print...\n"));
		return;
	}

	WLAN_OS_REPORT (("Number of active entries in classifier table : %d\n",pClsfrParams->uNumActiveEntries));

	switch (pClsfrParams->eClsfrType)
	{
		case DSCP_CLSFR:
			WLAN_OS_REPORT (("+------+-------+\n"));
			WLAN_OS_REPORT (("| Code | D-Tag |\n"));
			WLAN_OS_REPORT (("+------+-------+\n"));

			for (i = 0; i < pClsfrParams->uNumActiveEntries; i++)  
            {
				WLAN_OS_REPORT (("| %5d | %5d |\n",
                    pClsfrParams->ClsfrTable[i].Dscp.CodePoint,pClsfrParams->ClsfrTable[i].DTag));
			}

			WLAN_OS_REPORT (("+-------+-------+\n"));
			break;

		case PORT_CLSFR:
			WLAN_OS_REPORT (("+-------+-------+\n"));
			WLAN_OS_REPORT (("| Port  | D-Tag |\n"));
			WLAN_OS_REPORT (("+-------+-------+\n"));

			for (i = 0; i < pClsfrParams->uNumActiveEntries; i++)  
            {
				WLAN_OS_REPORT (("| %5d | %5d |\n",
                    pClsfrParams->ClsfrTable[i].Dscp.DstPortNum,pClsfrParams->ClsfrTable[i].DTag));
			}

			WLAN_OS_REPORT (("+-------+-------+\n"));
			break;

		case IPPORT_CLSFR:

			WLAN_OS_REPORT (("+-------------+-------+-------+\n"));
			WLAN_OS_REPORT (("| IP Address  | Port  | D-Tag |\n"));
			WLAN_OS_REPORT (("+-------------+-------+-------+\n"));

			for (i = 0; i < pClsfrParams->uNumActiveEntries; i++)  
            {
				uIpAddr = pClsfrParams->ClsfrTable[i].Dscp.DstIPPort.DstIPAddress;
				WLAN_OS_REPORT (("| %02x.%02x.%02x.%02x | %5d | %5d |\n",
                    (uIpAddr & 0xFF),((uIpAddr >> 8) & (0xFF)),((uIpAddr >> 16) & (0xFF)),((uIpAddr >> 24) & (0xFF)),
                    pClsfrParams->ClsfrTable[i].Dscp.DstIPPort.DstPortNum, pClsfrParams->ClsfrTable[i].DTag));
			}

			WLAN_OS_REPORT (("+-------------+-------+-------+\n"));
			break;

		default:
TRACE0(pTxDataQ->hReport, REPORT_SEVERITY_ERROR, "Classifier_InsertClsfrEntry(): Classifier type -- unknown - Aborting\n");
			break;
	}
}

#endif  /* TI_DBG */



