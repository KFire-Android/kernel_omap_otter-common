/*
 * mlmeParser.c
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

 /** \file mlmeBuilder.c
 *  \brief 802.11 MLME Parser
 *
 *  \see mlmeParser.h
 */


/***************************************************************************/
/*                                                                         */
/*      MODULE: mlmeParser.c                                              */
/*    PURPOSE:  802.11 MLME Parser                                        */
/*                                                                         */
/***************************************************************************/



#define __FILE_ID__  FILE_ID_68
#include "osApi.h"
#include "paramOut.h"
#include "report.h"
#include "DataCtrl_Api.h"
#include "smeApi.h"
#include "mlmeApi.h"
#include "mlmeSm.h"
#include "AssocSM.h"
#include "authSm.h"
#include "mlmeParser.h"
#include "measurementMgrApi.h"
#include "ScanCncn.h"
#include "siteMgrApi.h"
#include "spectrumMngmntMgr.h"
#include "currBss.h"
#include "apConn.h"
#include "SwitchChannelApi.h"
#include "regulatoryDomainApi.h"
#include "qosMngr_API.h"
#include "scanResultTable.h"
#include "RxBuf.h"


/* Constants */

/* Enumerations */

/* Typedefs */

/* Structures */

/* External data definitions */

/* External functions definitions */

/* Local function prototypes */

/* Functions */

#define CHECK_PARSING_ERROR_CONDITION_PRINT 0

extern int WMEQosTagToACTable[MAX_NUM_OF_802_1d_TAGS];

TI_STATUS mlmeParser_recv(TI_HANDLE hMlme, void *pBuffer, TRxAttr* pRxAttr)
{
    TI_STATUS              status;
    mlme_t                 *pHandle = (mlme_t *)hMlme;
    TI_UINT8               *pData;
    TI_INT32               bodyDataLen;
    TI_UINT32              readLen;
    dot11_eleHdr_t         *pEleHdr;
    dot11_mgmtFrame_t      *pMgmtFrame;
    dot11MgmtSubType_e     msgType;
    paramInfo_t            *pParam;
    TMacAddr               recvBssid;
    TMacAddr               recvSa;
    TI_UINT8               rsnIeIdx = 0;
    TI_UINT8               wpaIeOuiIe[] = WPA_IE_OUI;
#ifdef XCC_MODULE_INCLUDED
	TI_UINT8			   XCC_oui[] = XCC_OUI;
	XCCv4IEs_t			   *pXCCIeParameter;
#endif
    TI_BOOL				   ciscoIEPresent = TI_FALSE;

    if ((hMlme == NULL) || (pBuffer == NULL))
    {
        WLAN_OS_REPORT (("mlmeParser_recv: hMlme == %x, buf = %x\n", hMlme, pBuffer));
		return TI_NOK;
    }

    /* zero frame content */
	os_memoryZero (pHandle->hOs, &(pHandle->tempFrameInfo), sizeof(mlmeIEParsingParams_t));

    pMgmtFrame = (dot11_mgmtFrame_t*)RX_BUF_DATA(pBuffer);

    /* get frame type */
    status = mlmeParser_getFrameType(pHandle, (TI_UINT16 *)&pMgmtFrame->hdr.fc, &msgType);
    if (status != TI_OK)
    {
        RxBufFree(pHandle->hOs, pBuffer);
        return TI_NOK;
    }

    pParam = (paramInfo_t *)os_memoryAlloc(pHandle->hOs, sizeof(paramInfo_t));
    if (!pParam)
	{
        RxBufFree(pHandle->hOs, pBuffer);
        return TI_NOK;
    }

    pHandle->tempFrameInfo.frame.subType = msgType;

    /* We have to ignore management frames from other BSSIDs (except beacons & probe responses) */
    pParam->paramType = CTRL_DATA_CURRENT_BSSID_PARAM;
    ctrlData_getParam(pHandle->hCtrlData, pParam);

    MAC_COPY (recvBssid, pMgmtFrame->hdr.BSSID);
    MAC_COPY(recvSa, pMgmtFrame->hdr.SA);

    if (MAC_EQUAL (pParam->content.ctrlDataCurrentBSSID, recvBssid))
        pHandle->tempFrameInfo.myBssid = TI_TRUE;
    else
        pHandle->tempFrameInfo.myBssid = TI_FALSE;


    if (MAC_EQUAL (pParam->content.ctrlDataCurrentBSSID, recvSa))
		pHandle->tempFrameInfo.mySa = TI_TRUE;
	else
		pHandle->tempFrameInfo.mySa = TI_FALSE;

	/* The Default value of the myDst flag is false, only in case of unicast packet with the STA's destination address, the flag is set to True */
	pHandle->tempFrameInfo.myDst = TI_FALSE;


    /* check destination MAC address for broadcast */

    if (MAC_BROADCAST (pMgmtFrame->hdr.DA))
    {
        pHandle->tempFrameInfo.frame.extesion.destType = MSG_BROADCAST;
    }
        else
        {
        if (MAC_MULTICAST (pMgmtFrame->hdr.DA))
        {
            pHandle->tempFrameInfo.frame.extesion.destType = MSG_MULTICAST;
        }
        else
        {
            pHandle->tempFrameInfo.frame.extesion.destType = MSG_UNICAST;
			pParam->paramType = CTRL_DATA_MAC_ADDRESS;
		    ctrlData_getParam(pHandle->hCtrlData, pParam);

			/* Verifying whether the received unicast packet is for the STA, if yes we set the flag to True */
			if (MAC_EQUAL( (pParam->content.ctrlDataDeviceMacAddress), (pMgmtFrame->hdr.DA) ))
				pHandle->tempFrameInfo.myDst = TI_TRUE;

        }
    }

    MAC_COPY (pHandle->tempFrameInfo.bssid, pMgmtFrame->hdr.BSSID);

    pData = (TI_UINT8 *)(pMgmtFrame->body);

    /* length of body (BUF without 802.11 header and FCS) */
    bodyDataLen = RX_BUF_LEN(pBuffer) - WLAN_HDR_LEN;

    switch (msgType)
    {
    case ASSOC_REQUEST:
        TRACE0(pHandle->hReport, REPORT_SEVERITY_SM, "MLME_PARSER: recieved ASSOC_REQ message \n");
        break;
    case RE_ASSOC_REQUEST:
        TRACE0(pHandle->hReport, REPORT_SEVERITY_SM, "MLME_PARSER: recieved RE_ASSOC_REQ message \n");
        break;
    case RE_ASSOC_RESPONSE:
        TRACE0(pHandle->hReport, REPORT_SEVERITY_SM, "MLME_PARSER: recieved RE_ASSOC_RSP message \n");
      /*  break;*/
    case ASSOC_RESPONSE:
		/* if the assoc response is not directed to our STA or not from the current AP */
        if ((!pHandle->tempFrameInfo.myBssid) || (!pHandle->tempFrameInfo.mySa) || (pHandle->tempFrameInfo.myDst == TI_FALSE))
            break;

        /* Save the association response message */
        assoc_saveAssocRespMessage(pHandle->hAssoc, (TI_UINT8 *)(pMgmtFrame->body), bodyDataLen);

        /* init frame fields */
        pHandle->tempFrameInfo.frame.content.assocRsp.barkerPreambleMode = PREAMBLE_UNSPECIFIED;

        /* read capabilities */
	    COPY_WLAN_WORD(&pHandle->tempFrameInfo.frame.content.assocRsp.capabilities , pData);
        pData += 2;
        /* read status */
	    COPY_WLAN_WORD(&pHandle->tempFrameInfo.frame.content.assocRsp.status , pData);
        pData += 2;
        /* read AID */
	    COPY_WLAN_WORD(&pHandle->tempFrameInfo.frame.content.assocRsp.aid , pData);
        pHandle->tempFrameInfo.frame.content.assocRsp.aid &= ASSOC_RESP_AID_MASK;
        pData += 2;

        bodyDataLen -= ASSOC_RESP_FIXED_DATA_LEN;
        /***************************/

        pHandle->tempFrameInfo.frame.content.assocRsp.pRsnIe = NULL;
        pHandle->tempFrameInfo.frame.content.assocRsp.rsnIeLen = 0;
        while (bodyDataLen > 2)
        {
            pEleHdr = (dot11_eleHdr_t*)pData;

            if ((*pEleHdr)[1] > (bodyDataLen - 2))
            {
                TRACE3(pHandle->hReport, REPORT_SEVERITY_ERROR, "MLME_PARSER: IE %d with length %d out of bounds %d\n", (*pEleHdr)[0], (*pEleHdr)[1], (bodyDataLen - 2));
                status = TI_NOK;
                goto mlme_recv_end;
            }

            switch ((*pEleHdr)[0])
            {
                        /* read rates */
            case SUPPORTED_RATES_IE_ID:
                pHandle->tempFrameInfo.frame.content.assocRsp.pRates = &(pHandle->tempFrameInfo.rates);
                status = mlmeParser_readRates(pHandle, pData, bodyDataLen, &readLen, &(pHandle->tempFrameInfo.rates));
                if (status != TI_OK)
                {
                    TRACE0(pHandle->hReport, REPORT_SEVERITY_ERROR, "MLME_PARSER: error reading RATES\n");
                    goto mlme_recv_end;
                }
                break;

            case EXT_SUPPORTED_RATES_IE_ID:
                /* read rates */
                pHandle->tempFrameInfo.frame.content.assocRsp.pExtRates = &(pHandle->tempFrameInfo.extRates);
                status = mlmeParser_readRates(pHandle, pData, bodyDataLen, &readLen, &(pHandle->tempFrameInfo.extRates));
                if (status != TI_OK)
                {
                    TRACE0(pHandle->hReport, REPORT_SEVERITY_ERROR, "MLME_PARSER: error reading RATES\n");
                    goto mlme_recv_end;
                }
                break;

            case WPA_IE_ID:
                /* Note : WPA, WME, TSRS and msdu lifetime use the same Element ID */
                /*  Its assumes that:
                        TSRS and msdu lifetime use OUI = 0x00,0x40,0x96 (=Cisco) but
                        use different OUI Type:
                            TSRS          uses OUI Type 8
                            msdu lifetime uses OUI Type 9;
                        WPA and WME use the same OUI = 0x00,0x50,0xf2 but
                        use different OUI Type:
                            WPA - uses OUI Type with value  - 1
                            WME - uses OUI Type with value  - 2.
                */

                /* check if this is WME IE */
                if((os_memoryCompare(pHandle->hOs, wpaIeOuiIe, pData+2, DOT11_OUI_LEN - 1) == 0) && 
						((*(TI_UINT8*)(pData+5)) == dot11_WME_OUI_TYPE))
                {
                    pHandle->tempFrameInfo.frame.content.assocRsp.WMEParams = &(pHandle->tempFrameInfo.WMEParams);
                    status = mlmeParser_readWMEParams(pHandle, pData, bodyDataLen, &readLen, 
                                                      &(pHandle->tempFrameInfo.WMEParams),
													  &(pHandle->tempFrameInfo.frame.content.assocRsp));
                    if (status != TI_OK)
                    {
                        TRACE0(pHandle->hReport, REPORT_SEVERITY_ERROR, "MLME_PARSER: error reading WME parameters\n");
                        goto mlme_recv_end;
                    }
                }
#ifdef XCC_MODULE_INCLUDED
				/* check if this is XCC vendor specific OUI */
				else if (os_memoryCompare(pHandle->hOs, XCC_oui, pData+2, DOT11_OUI_LEN - 1) == 0) 
				{
					pXCCIeParameter = &(pHandle->tempFrameInfo.frame.content.assocRsp.XCCIEs[WMEQosTagToACTable[*(pData+6)]]);
					mlmeParser_readXCCOui(pData, bodyDataLen, &readLen, pXCCIeParameter);
				}
#endif
				else
                {
                    /* skip this IE */
                    readLen = (*pEleHdr)[1] + 2;
                }
                break;

            case XCC_EXT_1_IE_ID:
				ciscoIEPresent = TI_TRUE;
                pHandle->tempFrameInfo.frame.content.assocRsp.pRsnIe = &(pHandle->tempFrameInfo.rsnIe[0]);
                status = mlmeParser_readRsnIe(pHandle, pData, bodyDataLen, &readLen, 
                                              &(pHandle->tempFrameInfo.rsnIe[rsnIeIdx]));
                if (status != TI_OK)
                {
                    TRACE0(pHandle->hReport, REPORT_SEVERITY_ERROR, "MLME_PARSER: error reading XCC EXT1 IE\n");
                    goto mlme_recv_end;
                }
    
                pHandle->tempFrameInfo.frame.content.assocRsp.rsnIeLen += readLen;
                rsnIeIdx ++;
                break;

            case XCC_EXT_2_IE_ID:
				ciscoIEPresent = TI_TRUE;
                pHandle->tempFrameInfo.frame.content.assocRsp.pRsnIe   = &(pHandle->tempFrameInfo.rsnIe[0]);
                status = mlmeParser_readRsnIe(pHandle, pData, bodyDataLen, &readLen,
                                              &(pHandle->tempFrameInfo.rsnIe[rsnIeIdx]));
                if (status != TI_OK)
                {
                    TRACE0(pHandle->hReport, REPORT_SEVERITY_ERROR, "MLME_PARSER: error reading RSN IP ADDR IE\n");
                    goto mlme_recv_end;
                }
    
                pHandle->tempFrameInfo.frame.content.assocRsp.rsnIeLen += readLen;
                rsnIeIdx ++;
                break;

			case DOT11_QOS_CAPABILITY_ELE_ID:
				pHandle->tempFrameInfo.frame.content.assocRsp.QoSCapParameters = &(pHandle->tempFrameInfo.QosCapParams);
                status = mlmeParser_readQosCapabilityIE(pHandle, pData, bodyDataLen, &readLen, 
                                                        &(pHandle->tempFrameInfo.QosCapParams));
                if (status != TI_OK)
                {
                    TRACE0(pHandle->hReport, REPORT_SEVERITY_ERROR, "MLME_PARSER: error reading QOS\n");
                    goto mlme_recv_end;
                }
                break;

			case HT_CAPABILITIES_IE_ID:
				pHandle->tempFrameInfo.frame.content.assocRsp.pHtCapabilities = &(pHandle->tempFrameInfo.tHtCapabilities);
                status = mlmeParser_readHtCapabilitiesIE (pHandle, pData, bodyDataLen, &readLen,
                                                          &(pHandle->tempFrameInfo.tHtCapabilities));

                if (status != TI_OK)
                {
                    TRACE0(pHandle->hReport, REPORT_SEVERITY_ERROR, "MLME_PARSER: error reading HT Capabilities IE\n");
                    goto mlme_recv_end;
                }
                break;

			case HT_INFORMATION_IE_ID:
				pHandle->tempFrameInfo.frame.content.assocRsp.pHtInformation = &(pHandle->tempFrameInfo.tHtInformation);
                status = mlmeParser_readHtInformationIE (pHandle, pData, bodyDataLen, &readLen, 
                                                         &(pHandle->tempFrameInfo.tHtInformation));
                if (status != TI_OK)
                {
                    TRACE0(pHandle->hReport, REPORT_SEVERITY_ERROR, "MLME_PARSER: error reading HT Information IE\n");
                    goto mlme_recv_end;
                }
                break;

            default:
                TRACE1(pHandle->hReport, REPORT_SEVERITY_INFORMATION, "MLME_PARSER: unsupported IE found (%d)\n", (*pEleHdr)[1]);
                readLen = (*pEleHdr)[1] + 2;
                status = TI_OK;
                break;
            }

            pData += readLen;
            bodyDataLen -= readLen;
        }
        /***************************/

		/* set the appropriate flag in the association response frame */
		/* to indicate whether or not we encountered a Cisco IE, i.e., */
		/* if we have any indication as to whether the AP we've associated */
		/* with is a Cisco AP. */
		pHandle->tempFrameInfo.frame.content.assocRsp.ciscoIEPresent = ciscoIEPresent;

        TRACE1(pHandle->hReport, REPORT_SEVERITY_INFORMATION, "MLME_PARSER: ciscoIEPresent = %d\n", ciscoIEPresent);

        status = assoc_recv(pHandle->hAssoc, &(pHandle->tempFrameInfo.frame));
        break;

    case PROBE_REQUEST:
        TRACE0(pHandle->hReport, REPORT_SEVERITY_SM, "MLME_PARSER: recieved PROBE_REQ message \n");
        break;
    case PROBE_RESPONSE:

        TRACE0(pHandle->hReport, REPORT_SEVERITY_SM, "MLME_PARSER: recieved PROBE_RESPONSE message \n");

        if(RX_BUF_LEN(pBuffer)-WLAN_HDR_LEN-TIME_STAMP_LEN-4 > MAX_BEACON_BODY_LENGTH)
        {
            TRACE3(pHandle->hReport, REPORT_SEVERITY_ERROR, "mlmeParser_recv: probe response length out of range. length=%d, band=%d, channel=%d\n", RX_BUF_LEN(pBuffer)-WLAN_HDR_LEN-TIME_STAMP_LEN-4, pRxAttr->band, pRxAttr->channel);
            if ((pRxAttr->eScanTag > SCAN_RESULT_TAG_CURENT_BSS) && (pRxAttr->eScanTag != SCAN_RESULT_TAG_MEASUREMENT))
            {
                /* Notify the result CB of an invalid frame (to update the result counter) */
                scanCncn_MlmeResultCB( pHandle->hScanCncn, NULL, NULL, pRxAttr, NULL, 0);
            }
            status = TI_NOK;
            goto mlme_recv_end;
        }

        /* init frame fields */
        pHandle->tempFrameInfo.frame.content.iePacket.barkerPreambleMode = PREAMBLE_UNSPECIFIED;

        /* read time stamp */
        os_memoryCopy(pHandle->hOs, (void *)pHandle->tempFrameInfo.frame.content.iePacket.timestamp, pData, TIME_STAMP_LEN);
        pData += TIME_STAMP_LEN;

        bodyDataLen -= TIME_STAMP_LEN;
        /* read beacon interval */
	    COPY_WLAN_WORD(&pHandle->tempFrameInfo.frame.content.iePacket.beaconInerval , pData);
        pData += 2;
        /* read capabilities */
	    COPY_WLAN_WORD(&pHandle->tempFrameInfo.frame.content.iePacket.capabilities , pData);
        pData += 2;

        bodyDataLen -= 4;
        pHandle->tempFrameInfo.frame.content.iePacket.pRsnIe = NULL;
        pHandle->tempFrameInfo.frame.content.iePacket.rsnIeLen = 0;

		pHandle->tempFrameInfo.band = pRxAttr->band;
		pHandle->tempFrameInfo.rxChannel = pRxAttr->channel;

        if ((pRxAttr->band == RADIO_BAND_2_4_GHZ) && (pRxAttr->channel > NUM_OF_CHANNELS_24))
        {
            TRACE2(pHandle->hReport, REPORT_SEVERITY_ERROR, "mlmeParser_recv, band=%d, channel=%d\n", pRxAttr->band, pRxAttr->channel);
            /* Error in parsing Probe response packet - exit */
            if ((pRxAttr->eScanTag > SCAN_RESULT_TAG_CURENT_BSS) && (pRxAttr->eScanTag != SCAN_RESULT_TAG_MEASUREMENT))
            {
                /* Notify the result CB of an invalid frame (to update the result counter) */
                scanCncn_MlmeResultCB( pHandle->hScanCncn, NULL, NULL, pRxAttr, NULL, 0);
            }
            status = TI_NOK;
            goto mlme_recv_end;
        }
        else if ((pRxAttr->band == RADIO_BAND_5_0_GHZ) && (pRxAttr->channel <= NUM_OF_CHANNELS_24))
        {
            TRACE2(pHandle->hReport, REPORT_SEVERITY_ERROR, "mlmeParser_recv, band=%d, channel=%d\n", pRxAttr->band, pRxAttr->channel);
            /* Error in parsing Probe response packet - exit */
            if ((pRxAttr->eScanTag > SCAN_RESULT_TAG_CURENT_BSS) && (pRxAttr->eScanTag != SCAN_RESULT_TAG_MEASUREMENT))
            {
                /* Notify the result CB of an invalid frame (to update the result counter) */
                scanCncn_MlmeResultCB( pHandle->hScanCncn, NULL, NULL, pRxAttr, NULL, 0);
            }
            status = TI_NOK;
            goto mlme_recv_end;
        }
        if (mlmeParser_parseIEs(hMlme, pData, bodyDataLen, &(pHandle->tempFrameInfo)) != TI_OK)
        {
            TRACE0(pHandle->hReport, REPORT_SEVERITY_ERROR, "mlmeParser_recv: Error in parsing Probe response packet\n");

            /* Error in parsing Probe response packet - exit */
            if ((pRxAttr->eScanTag > SCAN_RESULT_TAG_CURENT_BSS) && (pRxAttr->eScanTag != SCAN_RESULT_TAG_MEASUREMENT))
            {
                /* Notify the result CB of an invalid frame (to update the result counter) */
                scanCncn_MlmeResultCB( pHandle->hScanCncn, NULL, NULL, pRxAttr, NULL, 0);
            }
            status = TI_NOK;
            goto mlme_recv_end;
        }

        /* updating CountryIE  */
        if ((pHandle->tempFrameInfo.frame.content.iePacket.country != NULL) && 
            (pHandle->tempFrameInfo.frame.content.iePacket.country->hdr[1] != 0))
        {
            /* set the country info in the regulatory domain - If a different code was detected earlier
               the regDomain will ignore it */
            pParam->paramType = REGULATORY_DOMAIN_COUNTRY_PARAM;
            pParam->content.pCountry = (TCountry *)pHandle->tempFrameInfo.frame.content.iePacket.country;
            regulatoryDomain_setParam (pHandle->hRegulatoryDomain, pParam);
        }

        /* if tag = MSR, forward to the MSR module.  */
        if (SCAN_RESULT_TAG_MEASUREMENT == pRxAttr->eScanTag)
        {
            measurementMgr_mlmeResultCB( pHandle->hMeasurementMgr, 
                                   &(pHandle->tempFrameInfo.bssid), 
                                   &(pHandle->tempFrameInfo.frame), 
                                   pRxAttr,
                                   (TI_UINT8 *)(pMgmtFrame->body+TIME_STAMP_LEN+4),
                                   RX_BUF_LEN(pBuffer)-WLAN_HDR_LEN-TIME_STAMP_LEN-4 );
        }

        /* only forward frames from the current BSS (according to the tag) to current BSS */
        else if (SCAN_RESULT_TAG_CURENT_BSS == pRxAttr->eScanTag)
        {
			currBSS_probRespReceivedCallb(pHandle->hCurrBss, 
                                          pRxAttr, 
                                          &(pHandle->tempFrameInfo.bssid), 
                                          &(pHandle->tempFrameInfo.frame), 
                                          (TI_UINT8 *)(pMgmtFrame->body+TIME_STAMP_LEN+4), 
                                          RX_BUF_LEN(pBuffer)-WLAN_HDR_LEN-TIME_STAMP_LEN-4);
        }

		/* Check if there is a scan in progress, and this is a scan or measurement result (according to tag) */
		else /* (SCAN_RESULT_TAG_CURENT_BSS!= pRxAttr->eScanTag) & (SCAN_RESULT_TAG_MEASUREMENT != pRxAttr->eScanTag) */
        {
            /* result CB is registered - results are sent to the registered callback */
            scanCncn_MlmeResultCB( pHandle->hScanCncn, 
                                   &(pHandle->tempFrameInfo.bssid), 
                                   &(pHandle->tempFrameInfo.frame), 
                                   pRxAttr,
                                   (TI_UINT8 *)(pMgmtFrame->body+TIME_STAMP_LEN+4),
                                   RX_BUF_LEN(pBuffer)-WLAN_HDR_LEN-TIME_STAMP_LEN-4 );
        }

        if(pHandle->tempFrameInfo.recvChannelSwitchAnnoncIE == TI_FALSE)
		{
			switchChannel_recvCmd(pHandle->hSwitchChannel, NULL, pRxAttr->channel);
		}

        break;
    case BEACON:

        TRACE1(pHandle->hReport, REPORT_SEVERITY_INFORMATION, "MLME_PARSER: recieved BEACON message, TS= %ld\n", os_timeStampMs(pHandle->hOs));
        TRACE0(pHandle->hReport, REPORT_SEVERITY_INFORMATION, "beacon BUF");

        if(RX_BUF_LEN(pBuffer)-WLAN_HDR_LEN-TIME_STAMP_LEN-4 > MAX_BEACON_BODY_LENGTH)
        {
            TRACE3(pHandle->hReport, REPORT_SEVERITY_ERROR, "mlmeParser_recv: beacon length out of range. length=%d, band=%d, channel=%d\n", RX_BUF_LEN(pBuffer)-WLAN_HDR_LEN-TIME_STAMP_LEN-4, pRxAttr->band, pRxAttr->channel);
            if ((pRxAttr->eScanTag > SCAN_RESULT_TAG_CURENT_BSS) && (pRxAttr->eScanTag != SCAN_RESULT_TAG_MEASUREMENT))
            {
			    /* Notify the result CB of an invalid frame (to update the result counter) */
			    scanCncn_MlmeResultCB( pHandle->hScanCncn, NULL, NULL, pRxAttr, NULL, 0);
            }
            status = TI_NOK;
            goto mlme_recv_end;
        }

        /* init frame fields */
        pHandle->tempFrameInfo.frame.content.iePacket.barkerPreambleMode = PREAMBLE_UNSPECIFIED;

        /* read time stamp */
        os_memoryCopy(pHandle->hOs, (void *)pHandle->tempFrameInfo.frame.content.iePacket.timestamp, pData, TIME_STAMP_LEN);
        pData += TIME_STAMP_LEN;

        bodyDataLen -= TIME_STAMP_LEN;
        /* read beacon interval */
	    COPY_WLAN_WORD(&pHandle->tempFrameInfo.frame.content.iePacket.beaconInerval , pData);
        pData += 2;
        /* read capabilities */
	    COPY_WLAN_WORD(&pHandle->tempFrameInfo.frame.content.iePacket.capabilities , pData);
        pData += 2;

        bodyDataLen -= 4;
        pHandle->tempFrameInfo.frame.content.iePacket.pRsnIe = NULL;
        pHandle->tempFrameInfo.frame.content.iePacket.rsnIeLen = 0;

        if ((pRxAttr->band == RADIO_BAND_2_4_GHZ) && (pRxAttr->channel > NUM_OF_CHANNELS_24))
        {
            TRACE2(pHandle->hReport, REPORT_SEVERITY_ERROR, "mlmeParser_recv, band=%d, channel=%d\n", pRxAttr->band, pRxAttr->channel);
            /* Error in parsing Probe response packet - exit */
            if ((pRxAttr->eScanTag > SCAN_RESULT_TAG_CURENT_BSS) && (pRxAttr->eScanTag != SCAN_RESULT_TAG_MEASUREMENT))
            {
                /* Notify the result CB of an invalid frame (to update the result counter) */
                scanCncn_MlmeResultCB( pHandle->hScanCncn, NULL, NULL, pRxAttr, NULL, 0);
            }
            status = TI_NOK;
            goto mlme_recv_end;
        }
        else if ((pRxAttr->band == RADIO_BAND_5_0_GHZ) && (pRxAttr->channel <= NUM_OF_CHANNELS_24))
        {
            TRACE2(pHandle->hReport, REPORT_SEVERITY_ERROR, "mlmeParser_recv, band=%d, channel=%d\n", pRxAttr->band, pRxAttr->channel);
            /* Error in parsing Probe response packet - exit */
            if ((pRxAttr->eScanTag > SCAN_RESULT_TAG_CURENT_BSS) && (pRxAttr->eScanTag != SCAN_RESULT_TAG_MEASUREMENT))
            {
                /* Notify the result CB of an invalid frame (to update the result counter) */
                scanCncn_MlmeResultCB( pHandle->hScanCncn, NULL, NULL, pRxAttr, NULL, 0);
            }
            status = TI_NOK;
            goto mlme_recv_end;
        }
		pHandle->tempFrameInfo.band = pRxAttr->band;
		pHandle->tempFrameInfo.rxChannel = pRxAttr->channel;

        if (mlmeParser_parseIEs(hMlme, pData, bodyDataLen, &(pHandle->tempFrameInfo)) != TI_OK)
        {
            TRACE0(pHandle->hReport, REPORT_SEVERITY_WARNING, "mlmeParser_parseIEs - Error in parsing Beacon \n");
            /* Error in parsing Probe response packet - exit */
            if ((pRxAttr->eScanTag > SCAN_RESULT_TAG_CURENT_BSS) && (pRxAttr->eScanTag != SCAN_RESULT_TAG_MEASUREMENT))
            {
                /* Notify the result CB of an invalid frame (to update the result counter) */
                scanCncn_MlmeResultCB( pHandle->hScanCncn, NULL, NULL, pRxAttr, NULL, 0);
            }
            status = TI_NOK;
            goto mlme_recv_end;
        }

        /* updating CountryIE  */
        if ((pHandle->tempFrameInfo.frame.content.iePacket.country != NULL) && 
            (pHandle->tempFrameInfo.frame.content.iePacket.country->hdr[1] != 0))
        {
            /* set the country info in the regulatory domain - If a different code was detected earlier
               the regDomain will ignore it */
            pParam->paramType = REGULATORY_DOMAIN_COUNTRY_PARAM;
            pParam->content.pCountry = (TCountry *)pHandle->tempFrameInfo.frame.content.iePacket.country;
            regulatoryDomain_setParam (pHandle->hRegulatoryDomain, pParam);
        }


        /* if tag = MSR, forward to the MSR module.  */
        if (SCAN_RESULT_TAG_MEASUREMENT == pRxAttr->eScanTag)
        {
            measurementMgr_mlmeResultCB( pHandle->hMeasurementMgr, 
                                   &(pHandle->tempFrameInfo.bssid), 
                                   &(pHandle->tempFrameInfo.frame), 
                                   pRxAttr,
                                   (TI_UINT8 *)(pMgmtFrame->body+TIME_STAMP_LEN+4),
                                   RX_BUF_LEN(pBuffer)-WLAN_HDR_LEN-TIME_STAMP_LEN-4 );
        }

        /* only forward frames from the current BSS (according to the tag) to current BSS */
        else if (SCAN_RESULT_TAG_CURENT_BSS == pRxAttr->eScanTag)
        {
			currBSS_beaconReceivedCallb(pHandle->hCurrBss, pRxAttr, 
                                    &(pHandle->tempFrameInfo.bssid), 
                                    &(pHandle->tempFrameInfo.frame), 
                                    (TI_UINT8 *)(pMgmtFrame->body+TIME_STAMP_LEN+4), 
                                    RX_BUF_LEN(pBuffer)-WLAN_HDR_LEN-TIME_STAMP_LEN-4);
        }

        /* Check if there is a scan in progress, and this is a scan or measurement result (according to tag) */
		else /* (SCAN_RESULT_TAG_CURENT_BSS!= pRxAttr->eScanTag) & (SCAN_RESULT_TAG_MEASUREMENT != pRxAttr->eScanTag) */
		{
			/* result CB is registered - results are sent to the registered callback */
            scanCncn_MlmeResultCB( pHandle->hScanCncn, 
                                   &(pHandle->tempFrameInfo.bssid), 
                                   &(pHandle->tempFrameInfo.frame), 
                                   pRxAttr,
                                   (TI_UINT8 *)(pMgmtFrame->body+TIME_STAMP_LEN+4),
                                   RX_BUF_LEN(pBuffer)-WLAN_HDR_LEN-TIME_STAMP_LEN-4 );
        }

        /* Counting the number of recieved beacons - used for statistics */
        pHandle->BeaconsCounterPS++;

        if (pHandle->tempFrameInfo.recvChannelSwitchAnnoncIE == TI_FALSE)
		{
			switchChannel_recvCmd(pHandle->hSwitchChannel, NULL, pRxAttr->channel);
		}

        break;
    case ATIM:
        if (!pHandle->tempFrameInfo.myBssid)
            break;

        TRACE0(pHandle->hReport, REPORT_SEVERITY_SM, "MLME_PARSER: recieved ATIM message \n");
        break;
    case DIS_ASSOC:
        if ((!pHandle->tempFrameInfo.myBssid) || (!pHandle->tempFrameInfo.mySa))
            break;

        /* read Reason interval */
	    COPY_WLAN_WORD(&pHandle->tempFrameInfo.frame.content.disAssoc.reason , pData);

   		{	/* Send roaming trigger */
			roamingEventData_u RoamingEventData;
			RoamingEventData.APDisconnect.uStatusCode = pHandle->tempFrameInfo.frame.content.disAssoc.reason;
			RoamingEventData.APDisconnect.bDeAuthenticate = TI_FALSE; /* i.e. This is not DeAuth packet */
			apConn_reportRoamingEvent(pHandle->hApConn, ROAMING_TRIGGER_AP_DISCONNECT, &RoamingEventData);
		}
        break;

    case AUTH:
        /* Auth response frame is should be directed to our STA, and from the current AP */
		if ( (!pHandle->tempFrameInfo.myBssid) || (!pHandle->tempFrameInfo.mySa) || (pHandle->tempFrameInfo.myDst == TI_FALSE) )
            break;

        /* read Algorithm interval */
	    COPY_WLAN_WORD(&pHandle->tempFrameInfo.frame.content.auth.authAlgo , pData);
        pData += 2;
        /* read Sequence number */
	    COPY_WLAN_WORD(&pHandle->tempFrameInfo.frame.content.auth.seqNum , pData);
        pData += 2;
        /* read status */
	    COPY_WLAN_WORD(&pHandle->tempFrameInfo.frame.content.auth.status , pData);
        pData += 2;

        TRACE3(pHandle->hReport, REPORT_SEVERITY_INFORMATION, "MLME_PARSER: Read Auth: algo=%d, seq=%d, status=%d\n", pHandle->tempFrameInfo.frame.content.auth.authAlgo, pHandle->tempFrameInfo.frame.content.auth.seqNum, pHandle->tempFrameInfo.frame.content.auth.status);
        bodyDataLen -= 6;
        /* read Challenge */
        pHandle->tempFrameInfo.frame.content.auth.pChallenge = &(pHandle->tempFrameInfo.challenge);
        status = mlmeParser_readChallange(pHandle, pData, bodyDataLen, &readLen, &(pHandle->tempFrameInfo.challenge));
        if (status != TI_OK)
        {
            pHandle->tempFrameInfo.challenge.hdr[1] = 0;
            readLen = 0;
        }
        pData += readLen;

        status = auth_recv(pHandle->hAuth, &(pHandle->tempFrameInfo.frame));
        break;
    case DE_AUTH:
        if ((!pHandle->tempFrameInfo.myBssid) || (!pHandle->tempFrameInfo.mySa))
            break;

        /* consider the Assoc frame if it is one of the following:
			1) unicast frame and directed to our STA
			2) broadcast frame
		*/
		if( (pHandle->tempFrameInfo.frame.extesion.destType == MSG_UNICAST) && (pHandle->tempFrameInfo.myDst == TI_FALSE) )
            break;

        /* read Reason */
	    COPY_WLAN_WORD(&pHandle->tempFrameInfo.frame.content.deAuth.reason , pData);

		{	/* Send roaming trigger */
			roamingEventData_u RoamingEventData;
			RoamingEventData.APDisconnect.uStatusCode = pHandle->tempFrameInfo.frame.content.disAssoc.reason;
			RoamingEventData.APDisconnect.bDeAuthenticate = TI_TRUE; /* i.e. This is DeAuth packet */
			apConn_reportRoamingEvent(pHandle->hApConn, ROAMING_TRIGGER_AP_DISCONNECT, &RoamingEventData);
		}
        break;

    case ACTION:
		pParam->paramType = CTRL_DATA_CURRENT_BSS_TYPE_PARAM;
		ctrlData_getParam(pHandle->hCtrlData, pParam);

        if ((!pHandle->tempFrameInfo.myBssid) || 
			((!pHandle->tempFrameInfo.mySa) && (pParam->content.ctrlDataCurrentBssType == BSS_INFRASTRUCTURE)))
            break;

		/* if the action frame is unicast and not directed to our STA, we should ignore it */
		if( (pHandle->tempFrameInfo.frame.extesion.destType == MSG_UNICAST) && (pHandle->tempFrameInfo.myDst == TI_FALSE) )
			break;

        /* read Category field */
        pHandle->tempFrameInfo.frame.content.action.category = *pData;
        pData ++;
        bodyDataLen --;

        /* Checking if the category field is valid */
		if(( pHandle->tempFrameInfo.frame.content.action.category != CATAGORY_SPECTRUM_MANAGEMENT) &&
			(pHandle->tempFrameInfo.frame.content.action.category != CATAGORY_QOS)  &&
			(pHandle->tempFrameInfo.frame.content.action.category != WME_CATAGORY_QOS) )   
        {
            TRACE1(pHandle->hReport, REPORT_SEVERITY_ERROR, "MLME_PARSER: Error category is invalid for action management frame %d \n", pHandle->tempFrameInfo.frame.content.action.category );
            break;
        }

		switch(pHandle->tempFrameInfo.frame.content.action.category)
		{
			case CATAGORY_QOS:
			case WME_CATAGORY_QOS:			 
				/* read action field */
				pHandle->tempFrameInfo.frame.content.action.action = *pData;
				pData ++;
				bodyDataLen --;

				QosMngr_receiveActionFrames(pHandle->hQosMngr, pData, pHandle->tempFrameInfo.frame.content.action.action, bodyDataLen); 
				break;
				
			case CATAGORY_SPECTRUM_MANAGEMENT:
				/* read action field */
				pHandle->tempFrameInfo.frame.content.action.action = *pData;
				pData ++;
				bodyDataLen --;
				
				switch(pHandle->tempFrameInfo.frame.content.action.action)
				{
					case MEASUREMENT_REQUEST:
						/* Checking the frame type  */
						if(pHandle->tempFrameInfo.frame.extesion.destType == MSG_BROADCAST)
							pHandle->tempFrameInfo.frame.content.action.frameType = MSR_FRAME_TYPE_BROADCAST;
						else
							pHandle->tempFrameInfo.frame.content.action.frameType = MSR_FRAME_TYPE_UNICAST;
						
							/*measurementMgr_receiveFrameRequest(pHandle->hMeasurementMgr,
							pHandle->tempFrameInfo.frame.content.action.frameType,
							bodyDataLen,pData);*/
						break;

					case TPC_REQUEST:
						/*measurementMgr_receiveTPCRequest(pHandle->hMeasurementMgr,(TI_UINT8)bodyDataLen,pData);*/
						break;
						
					case CHANNEL_SWITCH_ANNOUNCEMENT:
						if (pHandle->tempFrameInfo.myBssid)
						{   /* Ignore Switch Channel commands from non my BSSID */
							mlmeParser_readChannelSwitch(pHandle,pData,bodyDataLen,&readLen,&(pHandle->tempFrameInfo.channelSwitch),
								pRxAttr->channel);
						}
						break;
						
					default:
                        TRACE1(pHandle->hReport, REPORT_SEVERITY_ERROR, "MLME_PARSER: Error, category is invalid for action management frame %d \n",							pHandle->tempFrameInfo.frame.content.action.category );
						break;
				}
				
				break;
				
			default:
				status = TI_NOK;
				break;
					
		}
    }

mlme_recv_end:
    /* release BUF */
    os_memoryFree(pHandle->hOs, pParam, sizeof(paramInfo_t));
	RxBufFree(pHandle->hOs, pBuffer);
    if (status != TI_OK)
        return TI_NOK;
    return status;
}

TI_STATUS mlmeParser_getFrameType(mlme_t *pMlme, TI_UINT16* pFrameCtrl, dot11MgmtSubType_e *pType)
{
    TI_UINT16 fc;
	
	COPY_WLAN_WORD(&fc, pFrameCtrl); /* copy with endianess handling. */

	if ((fc & DOT11_FC_PROT_VERSION_MASK) != DOT11_FC_PROT_VERSION)
    {
        TRACE1(pMlme->hReport, REPORT_SEVERITY_ERROR, "MLME_PARSER: Error Wrong protocol version (not %d) \n", DOT11_FC_PROT_VERSION);
        return TI_NOK;
    }

    if ((fc & DOT11_FC_TYPE_MASK) != DOT11_FC_TYPE_MGMT)
    {
        TRACE0(pMlme->hReport, REPORT_SEVERITY_ERROR, "MLME_PARSER: Error not MANAGEMENT frame\n");
        return TI_NOK;
    }

    *pType = (dot11MgmtSubType_e)((fc & DOT11_FC_SUB_MASK) >> 4);

    return TI_OK;
}


#ifdef XCC_MODULE_INCLUDED
void mlmeParser_readXCCOui (TI_UINT8 *pData, TI_UINT32 dataLen, TI_UINT32 *pReadLen, XCCv4IEs_t *XCCIEs)
{
    TI_UINT8 ieLen;
	TI_UINT8 ouiType;

    ieLen = *(pData+1) + 2;

    if (dataLen < ieLen)
    {
		/* Wrong length of info-element, skip to the end of the packet */
		*pReadLen = dataLen;
		return;
    }

    *pReadLen = ieLen;
	ouiType = *(pData+5);

	switch (ouiType) 
	{
		case TS_METRIX_OUI_TYPE:
			XCCIEs->tsMetrixParameter = (dot11_TS_METRICS_IE_t *)pData;
			break;
		case TS_RATE_SET_OUI_TYPE:
			XCCIEs->trafficStreamParameter = (dot11_TSRS_IE_t *)pData;
			break;
		case EDCA_LIFETIME_OUI_TYPE:
			XCCIEs->edcaLifetimeParameter = (dot11_MSDU_LIFE_TIME_IE_t *)pData;
			break;
	}
    return;
}
#endif


TI_STATUS mlmeParser_readERP(mlme_t *pMlme, TI_UINT8 *pData, TI_UINT32 dataLen, TI_UINT32 *pReadLen,
                             TI_BOOL *useProtection, EPreamble *barkerPreambleMode)
{

    TI_UINT32 erpIElen;
    TI_UINT16 ctrl;

    erpIElen = *(pData+1);
    *pReadLen = erpIElen + 2;

    if (dataLen < (TI_UINT32)(erpIElen + 2))
    {
        return TI_NOK;
    }

    COPY_WLAN_WORD(&ctrl , pData + 2);

    *useProtection = (ctrl & 0x2) >>1;
    *barkerPreambleMode = ((ctrl & 0x4) >>2) ? PREAMBLE_LONG : PREAMBLE_SHORT;

    return TI_OK;
}


TI_STATUS mlmeParser_readRates(mlme_t *pMlme, TI_UINT8 *pData, TI_UINT32 dataLen, TI_UINT32 *pReadLen, dot11_RATES_t *pRates)
{
    pRates->hdr[0] = *pData;
    pRates->hdr[1] = *(pData+1);

    *pReadLen = pRates->hdr[1] + 2;

    if (dataLen < (TI_UINT32)(pRates->hdr[1] + 2))
    {
        return TI_NOK;
    }

    if (pRates->hdr[1] > DOT11_MAX_SUPPORTED_RATES)
    {
        return TI_NOK;
    }

    os_memoryCopy(pMlme->hOs, (void *)pRates->rates, pData+2, pRates->hdr[1]);

    return TI_OK;
}


TI_STATUS mlmeParser_readSsid(mlme_t *pMlme, TI_UINT8 *pData, TI_UINT32 dataLen, TI_UINT32 *pReadLen, dot11_SSID_t *pSsid)
{
    pSsid->hdr[0] = *pData;
    pSsid->hdr[1] = *(pData+1);

    *pReadLen = pSsid->hdr[1] + 2;

    if ((dataLen < 2) || (dataLen < (TI_UINT32)(pSsid->hdr[1] + 2)))
    {
        return TI_NOK;
    }

    if (pSsid->hdr[1] > MAX_SSID_LEN)
    {
        return TI_NOK;
    }

    os_memoryCopy(pMlme->hOs, (void *)pSsid->serviceSetId, pData+2, pSsid->hdr[1]);

    return TI_OK;
}


TI_STATUS mlmeParser_readFhParams(mlme_t *pMlme, TI_UINT8 *pData, TI_UINT32 dataLen, TI_UINT32 *pReadLen, dot11_FH_PARAMS_t *pFhParams)
{
    pFhParams->hdr[0] = *pData;
    pFhParams->hdr[1] = *(pData+1);
    pData += 2;

    if ((dataLen < 2) || (dataLen < (TI_UINT32)(pFhParams->hdr[1] + 2)))
    {
        return TI_NOK;
    }

    COPY_WLAN_WORD(&pFhParams->dwellTime , pData);
    pData += 2;

    pFhParams->hopSet = *pData;
    pFhParams->hopPattern = *(pData+1);
    pFhParams->hopIndex = *(pData+2);

    *pReadLen = pFhParams->hdr[1] + 2;

    return TI_OK;
}


TI_STATUS mlmeParser_readDsParams(mlme_t *pMlme, TI_UINT8 *pData, TI_UINT32 dataLen, TI_UINT32 *pReadLen, dot11_DS_PARAMS_t *pDsParams)
{
    pDsParams->hdr[0] = *pData;
    pDsParams->hdr[1] = *(pData+1);

    if ((dataLen < 2) || (dataLen < (TI_UINT32)(pDsParams->hdr[1] + 2)))
    {
        return TI_NOK;
    }

    pDsParams->currChannel = *(pData+2);

    *pReadLen = pDsParams->hdr[1] + 2;

    return TI_OK;
}


TI_STATUS mlmeParser_readCfParams(mlme_t *pMlme, TI_UINT8 *pData, TI_UINT32 dataLen, TI_UINT32 *pReadLen, dot11_CF_PARAMS_t *pCfParams)
{
    pCfParams->hdr[0] = *pData;
    pCfParams->hdr[1] = *(pData+1);
    pData += 2;

    if ((dataLen < 2) || (dataLen < (TI_UINT32)(pCfParams->hdr[1] + 2)))
    {
        return TI_NOK;
    }

    pCfParams->cfpCount = *pData;
    pCfParams->cfpPeriod = *(pData+1);
    pData += 2;

    COPY_WLAN_WORD(&pCfParams->cfpMaxDuration, pData);
    pData += 2;

    COPY_WLAN_WORD(&pCfParams->cfpDurRemain, pData);

    *pReadLen = pCfParams->hdr[1] + 2;

    return TI_OK;
}


TI_STATUS mlmeParser_readIbssParams(mlme_t *pMlme, TI_UINT8 *pData, TI_UINT32 dataLen, TI_UINT32 *pReadLen, dot11_IBSS_PARAMS_t *pIbssParams)
{
    pIbssParams->hdr[0] = *pData;
    pIbssParams->hdr[1] = *(pData+1);
    pData += 2;

    if ((dataLen < 2) || (dataLen < (TI_UINT32)(pIbssParams->hdr[1] + 2)))
    {
        return TI_NOK;
    }

    COPY_WLAN_WORD(&pIbssParams->atimWindow, pData);

    *pReadLen = pIbssParams->hdr[1] + 2;

    return TI_OK;
}


TI_STATUS mlmeParser_readTim(mlme_t *pMlme, TI_UINT8 *pData, TI_UINT32 dataLen, TI_UINT32 *pReadLen, dot11_TIM_t *pTim)
{
    pTim->hdr[0] = *pData;
    pTim->hdr[1] = *(pData+1);

    if ((dataLen < 2) || (dataLen < (TI_UINT32)(pTim->hdr[1] + 2)) || (pTim->hdr[1] < 3))
    {
        return TI_NOK;
    }

    pTim->dtimCount		= *(pData + 2);
    pTim->dtimPeriod	= *(pData + 3);
    pTim->bmapControl	= *(pData + 4);

    os_memoryCopy(pMlme->hOs, (void *)pTim->partialVirtualBmap, pData + 5, pTim->hdr[1] - 3);

    *pReadLen = pTim->hdr[1] + 2;

    return TI_OK;
}


TI_STATUS mlmeParser_readCountry(mlme_t *pMlme,TI_UINT8 *pData, TI_UINT32 dataLen, TI_UINT32 *pReadLen, dot11_COUNTRY_t *countryIE)
{
	TI_INT32 i, j;

    countryIE->hdr[0] = *pData;
    countryIE->hdr[1] = *(pData+1);

    *pReadLen = countryIE->hdr[1] + 2;

    if ((dataLen < 8) || (dataLen < (TI_UINT32)(countryIE->hdr[1] + 2)))
    {
        return TI_NOK;
    }

    if (countryIE->hdr[1] > DOT11_COUNTRY_ELE_LEN_MAX)
    {
        TRACE2(pMlme->hReport, REPORT_SEVERITY_ERROR, "MLME_PARSER: country IE error: eleLen=%d, maxLen=%d\n", countryIE->hdr[1], DOT11_COUNTRY_ELE_LEN_MAX);
        return TI_NOK;
    }

    os_memoryCopy(pMlme->hOs,&(countryIE->countryIE.CountryString), pData+2, DOT11_COUNTRY_STRING_LEN);

	/* Loop on all tripletChannels. Each item has three fields ('i' counts rows and 'j' counts bytes). */
	for (i = 0, j = 0;  j < (countryIE->hdr[1] - DOT11_COUNTRY_STRING_LEN);  i++, j+=3)
	{
		countryIE->countryIE.tripletChannels[i].firstChannelNumber	= *(pData + j + 5);
		countryIE->countryIE.tripletChannels[i].numberOfChannels	= *(pData + j + 6);
		countryIE->countryIE.tripletChannels[i].maxTxPowerLevel		= *(pData + j + 7);
	}

    return TI_OK;
}


TI_STATUS mlmeParser_readWMEParams(mlme_t *pMlme,TI_UINT8 *pData, TI_UINT32 dataLen, 
								   TI_UINT32 *pReadLen, dot11_WME_PARAM_t *pWMEParamIE, 
								   assocRsp_t *assocRsp)
{
	TI_UINT8 ieSubtype;
	TI_UINT8 ac;

	/* Note:  This function actually reads either the WME-Params IE or the WME-Info IE! */

	pWMEParamIE->hdr[0] = *pData;
	pWMEParamIE->hdr[1] = *(pData+1);

	*pReadLen = pWMEParamIE->hdr[1] + 2;

	if (dataLen < *pReadLen)
	{
		TRACE2(pMlme->hReport, REPORT_SEVERITY_ERROR, "MLME_PARSER: WME Parameter: eleLen=%d is too long (%d)\n", *pReadLen, dataLen);
		*pReadLen = dataLen;
		return TI_NOK;
	}

	if ((pWMEParamIE->hdr[1]> WME_TSPEC_IE_LEN) || (pWMEParamIE->hdr[1]< DOT11_WME_ELE_LEN))
	{
        TRACE1(pMlme->hReport, REPORT_SEVERITY_ERROR, "MLME_PARSER: WME Parameter IE error: eleLen=%d\n", pWMEParamIE->hdr[1]);
		return TI_NOK;
	}

	ieSubtype = *((TI_UINT8*)(pData+6));
	switch (ieSubtype)
	{
		case dot11_WME_OUI_SUB_TYPE_IE:
		case dot11_WME_OUI_SUB_TYPE_PARAMS_IE:
			/* Checking WME Version validity */
			if (*((TI_UINT8*)(pData+7)) != dot11_WME_VERSION )
			{
				TRACE1(pMlme->hReport, REPORT_SEVERITY_INFORMATION, "MLME_PARSER: WME Parameter IE error: Version =%d is unsupported\n",								  *((TI_UINT8*)(pData+7)) );
				return TI_NOK;
			}

			/* 
			 * Copy either the WME-Params IE or the WME-Info IE (Info is a subset of Params)!
			 * 
			 * Note that the WME_ACParameteres part is copied separately for two reasons:
			 *	1) It exists only in the WME-Params IE.
			 *	2) There is a gap of 2 bytes before the WME_ACParameteres if OS_PACKED is not supported.
			 */
			os_memoryCopy(pMlme->hOs,&(pWMEParamIE->OUI), pData+2, 8);
		
			if ( *((TI_UINT8*)(pData+6)) == dot11_WME_OUI_SUB_TYPE_PARAMS_IE )
			{
				os_memoryCopy(pMlme->hOs,&(pWMEParamIE->WME_ACParameteres), pData+10, pWMEParamIE->hdr[1] - 8);
			}

			break;

		case WME_TSPEC_IE_OUI_SUB_TYPE:
			/* Read renegotiated TSPEC parameters */
			if (assocRsp == NULL) 
			{
TRACE0(pMlme->hReport, REPORT_SEVERITY_ERROR, "MLME_PARSER: WME Parameter IE error: TSPEC Sub Type in beacon or probe resp\n");
				return TI_NOK;
			}

			ac = WMEQosTagToACTable [ GET_USER_PRIORITY_FROM_WME_TSPEC_IE(pData) ];

			if (ac == QOS_AC_VO)
			{
				assocRsp->tspecVoiceParameters = pData;
			}
			else if (ac == QOS_AC_VI)
			{
				assocRsp->tspecSignalParameters = pData;
			}
			break;

		default:
			/* Checking OUI Sub Type validity */
TRACE1(pMlme->hReport, REPORT_SEVERITY_ERROR, "MLME_PARSER: WME Parameter IE error: Sub Type =%d is invalid\n",							  ieSubtype);
			return TI_NOK;
	}
    return TI_OK;
}


static TI_UINT32 mlmeParser_getWSCReadLen(TI_UINT8 *pData)
{
    return *(pData+1) + 2;
}


static TI_STATUS mlmeParser_readWSCParams(mlme_t *pMlme,TI_UINT8 *pData, TI_UINT32 dataLen, TI_UINT32 *pReadLen, dot11_WSC_t *pWSC_IE)
{
	pWSC_IE->hdr[0] = *pData;
	pWSC_IE->hdr[1] = *(pData+1);

    *pReadLen = pWSC_IE->hdr[1] + 2;

    /* Length Sanity check of the WSC IE */
	if ((dataLen < 8) || (dataLen < (TI_UINT32)(pWSC_IE->hdr[1] + 2)))
    {
        TRACE2(pMlme->hReport, REPORT_SEVERITY_ERROR, "MLME_PARSER: WSC Parameter IE error: dataLen=%d, pWSC_IE->hdr[1]=%d\n", dataLen, pWSC_IE->hdr[1]);
		return TI_NOK;
    }

    /* Length Sanity check of the WSC IE */
	if (pWSC_IE->hdr[1] > ( sizeof(dot11_WSC_t) - sizeof(dot11_eleHdr_t) ))
    {
        TRACE2(pMlme->hReport, REPORT_SEVERITY_ERROR, "MLME_PARSER: WSC Parameter IE error: eleLen=%d, maxLen=%d\n", pWSC_IE->hdr[1], ( sizeof(dot11_WSC_t) - sizeof(dot11_eleHdr_t) ));
        return TI_NOK;
    }
    
	/* Copy the WSC Params IE */
	os_memoryCopy(pMlme->hOs,&(pWSC_IE->OUI), pData+2, pWSC_IE->hdr[1]);

    return TI_OK;
}


TI_STATUS mlmeParser_readQosCapabilityIE(mlme_t *pMlme,TI_UINT8 *pData, TI_UINT32 dataLen, TI_UINT32 *pReadLen, dot11_QOS_CAPABILITY_IE_t *QosCapParams)
{
    QosCapParams->hdr[0] = *pData;
    QosCapParams->hdr[1] = *(pData+1);

    *pReadLen = QosCapParams->hdr[1] + 2;

    if (dataLen < (TI_UINT32)(QosCapParams->hdr[1] + 2))
    {
        return TI_NOK;
    }

    if (QosCapParams->hdr[1] > DOT11_QOS_CAPABILITY_ELE_LEN)
    {
        TRACE2(pMlme->hReport, REPORT_SEVERITY_ERROR, "MLME_PARSER: QOS Capability  IE error: eleLen=%d, maxLen=%d\n", QosCapParams->hdr[1], DOT11_QOS_CAPABILITY_ELE_LEN);
        return TI_NOK;
    }

   QosCapParams->QosInfoField = (*(pData+1));
    return TI_OK;
}


TI_STATUS mlmeParser_readHtCapabilitiesIE(mlme_t *pMlme,TI_UINT8 *pData, TI_UINT32 dataLen, TI_UINT32 *pReadLen, Tdot11HtCapabilitiesUnparse *pHtCapabilities)
{
    pHtCapabilities->tHdr[0] = *pData;
    pHtCapabilities->tHdr[1] = *(pData+1);

    *pReadLen = pHtCapabilities->tHdr[1] + 2;

    if (dataLen < (TI_UINT32)(pHtCapabilities->tHdr[1] + 2))
    {
        return TI_NOK;
    }

    if (pHtCapabilities->tHdr[1] != DOT11_HT_CAPABILITIES_ELE_LEN)
    {
        TRACE2(pMlme->hReport, REPORT_SEVERITY_ERROR, "MLME_PARSER: HT Capability IE error: eleLen=%d, expectedLen=%d\n", pHtCapabilities->tHdr[1], DOT11_HT_CAPABILITIES_ELE_LEN);
        return TI_NOK;
    }

    os_memoryCopy(pMlme->hOs, (void*)pHtCapabilities->aHtCapabilitiesIe, pData + 2, pHtCapabilities->tHdr[1]);
  
    return TI_OK;
}


TI_STATUS mlmeParser_readHtInformationIE(mlme_t *pMlme,TI_UINT8 *pData, TI_UINT32 dataLen, TI_UINT32 *pReadLen, Tdot11HtInformationUnparse *pHtInformation)
{
    pHtInformation->tHdr[0] = *pData;
    pHtInformation->tHdr[1] = *(pData+1);

    *pReadLen = pHtInformation->tHdr[1] + 2;

    if (dataLen < (TI_UINT32)(pHtInformation->tHdr[1] + 2))
    {
        return TI_NOK;
    }

    if (pHtInformation->tHdr[1] < DOT11_HT_INFORMATION_ELE_LEN)
    {
        TRACE2(pMlme->hReport, REPORT_SEVERITY_ERROR, "MLME_PARSER: HT Information IE error: eleLen=%d, minimum Len=%d\n", pHtInformation->tHdr[1], DOT11_HT_INFORMATION_ELE_LEN);
        return TI_NOK;
    }

    os_memoryCopy(pMlme->hOs, (void*)pHtInformation->aHtInformationIe, pData + 2, pHtInformation->tHdr[1]);
  
    return TI_OK;
}


TI_STATUS mlmeParser_readChallange(mlme_t *pMlme, TI_UINT8 *pData, TI_UINT32 dataLen, TI_UINT32 *pReadLen, dot11_CHALLENGE_t *pChallange)
{
    if (dataLen < 2)
    {
        return TI_NOK;
    }

    pChallange->hdr[0] = *pData;
    pChallange->hdr[1] = *(pData+1);
    pData += 2;

    if ((dataLen < 2) || (dataLen < (TI_UINT32)(pChallange->hdr[1] + 2)))
    {
        return TI_NOK;
    }

    if (pChallange->hdr[1] > DOT11_CHALLENGE_TEXT_MAX)
    {
        return TI_NOK;
    }

    os_memoryCopy(pMlme->hOs, (void *)pChallange->text, pData, pChallange->hdr[1]);

    *pReadLen = pChallange->hdr[1] + 2;

    return TI_OK;
}

TI_STATUS mlmeParser_readRsnIe(mlme_t *pMlme, TI_UINT8 *pData, TI_UINT32 dataLen, TI_UINT32 *pReadLen, dot11_RSN_t *pRsnIe)
{
    pRsnIe->hdr[0] = *pData;
    pRsnIe->hdr[1] = *(pData+1);
    pData += 2;

    if ((dataLen < 2) || (dataLen < (TI_UINT32)(pRsnIe->hdr[1] + 2)))
    {
        return TI_NOK;
    }

    os_memoryCopy(pMlme->hOs, (void *)pRsnIe->rsnIeData, pData, pRsnIe->hdr[1]);

    *pReadLen = pRsnIe->hdr[1] + 2;

    return TI_OK;
}

TI_STATUS mlmeParser_readPowerConstraint(mlme_t *pMlme,TI_UINT8 *pData, TI_UINT32 dataLen, TI_UINT32 *pReadLen, dot11_POWER_CONSTRAINT_t *powerConstraintIE)
{
    powerConstraintIE->hdr[0] = *pData;
    powerConstraintIE->hdr[1] = *(pData+1);

    *pReadLen = powerConstraintIE->hdr[1] + 2;

    if ((dataLen < 2) || (dataLen < (TI_UINT32)(powerConstraintIE->hdr[1] + 2)))
    {
        return TI_NOK;
    }

    if (powerConstraintIE->hdr[1] > DOT11_POWER_CONSTRAINT_ELE_LEN)
    {
        return TI_NOK;
    }

    os_memoryCopy(pMlme->hOs,(void *)&(powerConstraintIE->powerConstraint), pData+2, powerConstraintIE->hdr[1]);

    return TI_OK;
}


TI_STATUS mlmeParser_readChannelSwitch(mlme_t *pMlme,TI_UINT8 *pData, TI_UINT32 dataLen, TI_UINT32 *pReadLen, dot11_CHANNEL_SWITCH_t *channelSwitch, TI_UINT8 channel)
{
    channelSwitch->hdr[0] = *pData++;
    channelSwitch->hdr[1] = *pData++;

    *pReadLen = channelSwitch->hdr[1] + 2;

    if ((dataLen < 2) || (dataLen < (TI_UINT32)(channelSwitch->hdr[1] + 2)))
    {
        return TI_NOK;
    }

    if (channelSwitch->hdr[1] > DOT11_CHANNEL_SWITCH_ELE_LEN)
    {
        return TI_NOK;
    }

    channelSwitch->channelSwitchMode = *pData++;
    channelSwitch->channelNumber = *pData++;
    channelSwitch->channelSwitchCount = *pData;


	switchChannel_recvCmd(pMlme->hSwitchChannel, channelSwitch, channel);
	return TI_OK;
}

TI_STATUS mlmeParser_readQuiet(mlme_t *pMlme,TI_UINT8 *pData, TI_UINT32 dataLen, TI_UINT32 *pReadLen, dot11_QUIET_t *quiet)
{
    quiet->hdr[0] = *pData++;
    quiet->hdr[1] = *pData++;

    *pReadLen = quiet->hdr[1] + 2;

    if ((dataLen < 2) || (dataLen < (TI_UINT32)(quiet->hdr[1] + 2)))
    {
        return TI_NOK;
    }

    if (quiet->hdr[1] > DOT11_QUIET_ELE_LEN)
    {
        return TI_NOK;
    }

    quiet->quietCount = *pData++;
    quiet->quietPeriod = *pData++;
    quiet->quietDuration = *((TI_UINT16*)pData);
    pData += sizeof(TI_UINT16);
    quiet->quietOffset = *((TI_UINT16*)pData);

	return TI_OK;
}


TI_STATUS mlmeParser_readTPCReport(mlme_t *pMlme,TI_UINT8 *pData, TI_UINT32 dataLen, TI_UINT32 *pReadLen, dot11_TPC_REPORT_t *TPCReport)
{
    TPCReport->hdr[0] = *pData;
    TPCReport->hdr[1] = *(pData+1);

    *pReadLen = TPCReport->hdr[1] + 2;

    if ((dataLen < 2) || (dataLen < (TI_UINT32)(TPCReport->hdr[1] + 2)))
    {
        return TI_NOK;
    }

    if (TPCReport->hdr[1] > DOT11_TPC_REPORT_ELE_LEN)
    {
        return TI_NOK;
    }

    TPCReport->transmitPower = *(pData+2);

    return TI_OK;
}


#ifdef XCC_MODULE_INCLUDED
TI_STATUS mlmeParser_readCellTP(mlme_t *pMlme, TI_UINT8 *pData, TI_UINT32 dataLen, TI_UINT32 *pReadLen, dot11_CELL_TP_t *cellTP)
{
    TI_UINT8 XCC_OUI[] = XCC_OUI;

    cellTP->hdr[0] = *pData++;
    cellTP->hdr[1] = *pData++;

    *pReadLen = cellTP->hdr[1] + 2;

    if ((dataLen < 2) || (dataLen < (TI_UINT32)(cellTP->hdr[1] + 2)))
    {
        return TI_NOK;
    }

    if (cellTP->hdr[1] > DOT11_CELL_TP_ELE_LEN)
    {
        return TI_NOK;
    }

	os_memoryCopy(pMlme->hOs, (void*)cellTP->oui, pData, cellTP->hdr[1]);

    if (os_memoryCompare(pMlme->hOs, (void*)cellTP->oui, XCC_OUI, 3) != 0)
    {
		return TI_NOK;
    }

    return TI_OK;
}
#endif

#if CHECK_PARSING_ERROR_CONDITION_PRINT
	#define CHECK_PARSING_ERROR_CONDITION(x, msg, bDump)	   \
			if ((x)) \
			{ \
                TRACE0(pHandle->hReport, REPORT_SEVERITY_ERROR, msg);    \
                if (bDump) {\
                    TRACE1(pHandle->hReport, REPORT_SEVERITY_ERROR, "Buff len = %d \n", packetLength);    \
                    report_PrintDump (pPacketBody, packetLength); }\
				return TI_NOK; \
			}
#else
	#define CHECK_PARSING_ERROR_CONDITION(x, msg, bDump) \
         if ((x)) return TI_NOK;
#endif

TI_STATUS mlmeParser_parseIEs(TI_HANDLE hMlme, 
							  TI_UINT8 *pData,
							  TI_INT32 bodyDataLen,
							  mlmeIEParsingParams_t *params)
{
    dot11_eleHdr_t 		*pEleHdr;
    TI_UINT32 			 readLen;
	TI_STATUS			 status = TI_NOK;
    TI_UINT8 			 rsnIeIdx = 0;
    TI_UINT8 			 wpaIeOuiIe[4] = { 0x00, 0x50, 0xf2, 0x01};
	beacon_probeRsp_t 	*frame = &(params->frame.content.iePacket);
	mlme_t 				*pHandle = (mlme_t *)hMlme;
#ifdef XCC_MODULE_INCLUDED
	TI_BOOL				allowCellTP = TI_TRUE;
#endif
#if CHECK_PARSING_ERROR_CONDITION_PRINT
	TI_INT32				packetLength = bodyDataLen;
	TI_UINT8				*pPacketBody = pData;
#endif

	params->recvChannelSwitchAnnoncIE = TI_FALSE;

	while (bodyDataLen > 1)
	{
		pEleHdr = (dot11_eleHdr_t *)pData;
	
#if CHECK_PARSING_ERROR_CONDITION_PRINT
		/* CHECK_PARSING_ERROR_CONDITION(((*pEleHdr)[1] > (bodyDataLen - 2)), ("MLME_PARSER: IE %d with length %d out of bounds %d\n", (*pEleHdr)[0], (*pEleHdr)[1], (bodyDataLen - 2)), TI_TRUE); */
		if ((*pEleHdr)[1] > (bodyDataLen - 2))
		{
			TRACE3(pHandle->hReport, REPORT_SEVERITY_ERROR, "MLME_PARSER: IE %d with length %d out of bounds %d\n", (*pEleHdr)[0], (*pEleHdr)[1], (bodyDataLen - 2));
		
			TRACE1(pHandle->hReport, REPORT_SEVERITY_ERROR, "Buff len = %d \n", packetLength);
			report_PrintDump (pPacketBody, packetLength); 
		}
#endif
        switch ((*pEleHdr)[0])
		{
		/* read SSID */
		case SSID_IE_ID:
			frame->pSsid = &params->ssid;
			status = mlmeParser_readSsid(pHandle, pData, bodyDataLen, &readLen, frame->pSsid);
			CHECK_PARSING_ERROR_CONDITION((status != TI_OK), ("MLME_PARSER: error reading SSID\n"),TI_TRUE);
			break;
		/* read rates */
		case SUPPORTED_RATES_IE_ID:
			frame->pRates = &params->rates;
			status = mlmeParser_readRates(pHandle, pData, bodyDataLen, &readLen, frame->pRates);
			CHECK_PARSING_ERROR_CONDITION((status != TI_OK), ("MLME_PARSER: error reading RATES\n"),TI_TRUE);
			break;
		case EXT_SUPPORTED_RATES_IE_ID:
			frame->pExtRates = &params->extRates;
			status = mlmeParser_readRates(pHandle, pData, bodyDataLen, &readLen, frame->pExtRates);
			CHECK_PARSING_ERROR_CONDITION((status != TI_OK), ("MLME_PARSER: error reading EXT RATES\n"),TI_TRUE);
			break;

		case ERP_IE_ID:
			status = mlmeParser_readERP(pHandle, pData, bodyDataLen, &readLen,
										(TI_BOOL *)&frame->useProtection,
										(EPreamble *)&frame->barkerPreambleMode);

			CHECK_PARSING_ERROR_CONDITION((status != TI_OK), ("MLME_PARSER: error reading ERP\n"),TI_TRUE);
			break;
		/* read FH parameter set */
		case FH_PARAMETER_SET_IE_ID:
			frame->pFHParamsSet = &params->fhParams;
			status = mlmeParser_readFhParams(pHandle, pData, bodyDataLen, &readLen, frame->pFHParamsSet);
			CHECK_PARSING_ERROR_CONDITION((status != TI_OK), ("MLME_PARSER: error reading FH parameters\n"),TI_TRUE);
			break;
		/* read DS parameter set */
		case DS_PARAMETER_SET_IE_ID:
			frame->pDSParamsSet = &params->dsParams;
			status = mlmeParser_readDsParams(pHandle, pData, bodyDataLen, &readLen, frame->pDSParamsSet);
			CHECK_PARSING_ERROR_CONDITION((status != TI_OK), ("MLME_PARSER: error reading DS parameters\n"),TI_TRUE);
			if (RADIO_BAND_2_4_GHZ == params->band )
			{
				if (frame->pDSParamsSet->currChannel != params->rxChannel)
				{
					TRACE2(pHandle->hReport, REPORT_SEVERITY_ERROR, "Channel ERROR - incompatible channel source information: Frame=%d Vs Radio=%d.\nparser ABORTED!!!\n",
						frame->pDSParamsSet->currChannel , params->rxChannel);

					return TI_NOK;
				}
			}
			break;
		/* read CF parameter set */
		case CF_PARAMETER_SET_IE_ID:
			frame->pCFParamsSet = &params->cfParams;
			status = mlmeParser_readCfParams(pHandle, pData, bodyDataLen, &readLen, frame->pCFParamsSet);
			CHECK_PARSING_ERROR_CONDITION((status != TI_OK), ("MLME_PARSER: error reading CF parameters\n"),TI_TRUE);
			break;
		/* read IBSS parameter set */
		case IBSS_PARAMETER_SET_IE_ID:
			frame->pIBSSParamsSet = &params->ibssParams;
			status = mlmeParser_readIbssParams(pHandle, pData, bodyDataLen, &readLen, frame->pIBSSParamsSet);
			CHECK_PARSING_ERROR_CONDITION((status != TI_OK), ("MLME_PARSER: error reading IBSS parameters\n"),TI_TRUE);
			break;

		/* read TIM */
		case TIM_IE_ID:
			frame->pTIM = &params->tim;
			status = mlmeParser_readTim(pHandle, pData, bodyDataLen, &readLen, frame->pTIM);
			CHECK_PARSING_ERROR_CONDITION((status != TI_OK), ("MLME_PARSER: error reading TIM\n"),TI_TRUE);
			break;

		/* read Country */
		case COUNTRY_IE_ID:
			frame->country = &params->country;
			status = mlmeParser_readCountry(pHandle, pData, bodyDataLen, &readLen, frame->country);
			CHECK_PARSING_ERROR_CONDITION((status != TI_OK), ("MLME_PARSER: error reading country parameters\n"),TI_TRUE);
			break;

		/* read Power Constraint */
		case POWER_CONSTRAINT_IE_ID:
#ifdef XCC_MODULE_INCLUDED
			allowCellTP = TI_FALSE;
#endif
			frame->powerConstraint = &params->powerConstraint;
			status = mlmeParser_readPowerConstraint(pHandle, pData, bodyDataLen, &readLen, frame->powerConstraint);
			CHECK_PARSING_ERROR_CONDITION((status != TI_OK), ("MLME_PARSER: error reading Power Constraint parameters\n"),TI_TRUE);
			break;

		/* read Channel Switch Mode */
        case CHANNEL_SWITCH_ANNOUNCEMENT_IE_ID:
            
            frame->channelSwitch = &params->channelSwitch;

            if (params->myBssid)
			{   /* Ignore Switch Channel commands from non my BSSID */
				params->recvChannelSwitchAnnoncIE = TI_TRUE;
				status = mlmeParser_readChannelSwitch(pHandle, pData, bodyDataLen, &readLen, frame->channelSwitch, params->rxChannel);
				if (status != TI_OK)
				{
					/*
					 * PATCH for working with AP-DK 4.0.51 that use IE 37 (with length 20) for RSNE
					 * Ignore the IE instead of rejecting the whole BUF (beacon or probe response)
					 */
                    TRACE0(pHandle->hReport, REPORT_SEVERITY_WARNING, "MLME_PARSER: error reading Channel Switch announcement parameters - ignore IE\n");
				}
			}
			break;

		/* read Quiet IE */
		case QUIET_IE_ID:
			frame->quiet = &params->quiet;
			status = mlmeParser_readQuiet(pHandle, pData, bodyDataLen, &readLen, frame->quiet);
			CHECK_PARSING_ERROR_CONDITION((status != TI_OK), ("MLME_PARSER: error reading Quiet parameters\n"),TI_TRUE);
			break;

		/* read TPC report IE */
		case TPC_REPORT_IE_ID:
			frame->TPCReport = &params->TPCReport;
			status = mlmeParser_readTPCReport(pHandle, pData, bodyDataLen, &readLen, frame->TPCReport);
			CHECK_PARSING_ERROR_CONDITION((status != TI_OK), ("MLME_PARSER: error reading TPC report parameters\n"),TI_TRUE);
			break;

		case XCC_EXT_1_IE_ID:
			frame->pRsnIe   = &params->rsnIe[0];
			status = mlmeParser_readRsnIe(pHandle, pData, bodyDataLen, &readLen, &params->rsnIe[rsnIeIdx]);
			CHECK_PARSING_ERROR_CONDITION((status != TI_OK), ("MLME_PARSER: error reading RSN IE\n"),TI_TRUE);

			frame->rsnIeLen += readLen;
			rsnIeIdx ++;
			break;

		case RSN_IE_ID:
			frame->pRsnIe = &params->rsnIe[0];
			status = mlmeParser_readRsnIe(pHandle, pData, bodyDataLen, &readLen, &params->rsnIe[rsnIeIdx]);
			CHECK_PARSING_ERROR_CONDITION((status != TI_OK), ("MLME_PARSER: error reading RSN IE\n"),TI_TRUE);

			frame->rsnIeLen += readLen;
			rsnIeIdx ++;
			break;

		case DOT11_QOS_CAPABILITY_ELE_ID:
			frame->QoSCapParameters = &params->QosCapParams;
			status = mlmeParser_readQosCapabilityIE(pHandle, pData, bodyDataLen, &readLen, &params->QosCapParams);
			CHECK_PARSING_ERROR_CONDITION((status != TI_OK), ("MLME_PARSER: error reading QOS CapabilityIE\n"),TI_TRUE);
			break;

	case HT_CAPABILITIES_IE_ID:
		frame->pHtCapabilities = &params->tHtCapabilities;
		status = mlmeParser_readHtCapabilitiesIE(pHandle, pData, bodyDataLen, &readLen, &params->tHtCapabilities);
		CHECK_PARSING_ERROR_CONDITION((status != TI_OK), ("MLME_PARSER: error reading HT Capabilitys IE\n"),TI_TRUE);
		break;

	case HT_INFORMATION_IE_ID:
		frame->pHtInformation = &params->tHtInformation;
		status = mlmeParser_readHtInformationIE(pHandle, pData, bodyDataLen, &readLen, &params->tHtInformation);
		CHECK_PARSING_ERROR_CONDITION((status != TI_OK), ("MLME_PARSER: error reading HT Information IE\n"),TI_TRUE);
		break;

		case WPA_IE_ID:
			if (!os_memoryCompare(pHandle->hOs, pData+2, wpaIeOuiIe, 3))
			{
				/* Note : WSC, WPA and WME use the same OUI */
				/*  Its assumes that:
						WPA uses OUI Type with value  - 1
						WME uses OUI Type with value  - 2
						WSC uses OUI Type with value  - 4
				*/

				/* Check the OUI sub Type to verify whether this is a WSC IE, WME IE or WPA IE*/
				if( (*(TI_UINT8*)(pData+5)) == dot11_WPA_OUI_TYPE)
				{
					/* If we are here - the following is WPA IE */
					frame->pRsnIe = &params->rsnIe[0];
					status = mlmeParser_readRsnIe(pHandle, pData, bodyDataLen,
												  &readLen, &params->rsnIe[rsnIeIdx]);
					frame->rsnIeLen += readLen;
					rsnIeIdx ++;

					CHECK_PARSING_ERROR_CONDITION((status != TI_OK), ("MLME_PARSER: error reading RSN IE\n"),TI_TRUE);
				}
				if( ( (*(TI_UINT8*)(pData+5)) == dot11_WME_OUI_TYPE ) && 
					( ( (*(TI_UINT8*)(pData+6)) == dot11_WME_OUI_SUB_TYPE_PARAMS_IE) ||
					  ( (*(TI_UINT8*)(pData+6)) == dot11_WME_OUI_SUB_TYPE_IE) ) )
				{
					/* If we are here - the following is WME-Params IE, WME-Info IE or TSPEC IE. */
					/* Note that we are using the WMEParams struct also to hold the WME-Info IE
					     which is a subset of WMEParams, and only one of them is sent in a frame. */
					frame->WMEParams = &params->WMEParams;
					status = mlmeParser_readWMEParams(pHandle, pData, bodyDataLen, &readLen, frame->WMEParams, NULL);
					
					CHECK_PARSING_ERROR_CONDITION((status != TI_OK), ("MLME_PARSER: error reading WME params\n"),TI_TRUE);
				}
				if( ( (*(TI_UINT8*)(pData+5)) == dot11_WSC_OUI_TYPE ))
				{
					/* If we are here - the following is WSC IE */
                    readLen = mlmeParser_getWSCReadLen (pData);
                    /* 
                     * This IE is not supposed to be found in beacons accroding to the standard
                     * definition. However, some APs do add it to beacons. It is read from beacons
                     * accroding to a registry key (which is false by default). Even if it is not
                     * read, the readLen must be set for the pointer to advance, which is done
                     * above.
                     */
                    if ((BEACON != params->frame.subType) || (TI_TRUE == pHandle->bParseBeaconWSC))
                    {
					frame->WSCParams = &params->WSCParams;
					status = mlmeParser_readWSCParams(pHandle, pData, bodyDataLen, &readLen, frame->WSCParams);
					CHECK_PARSING_ERROR_CONDITION((status != TI_OK), ("MLME_PARSER: error reading WSC params\n"),TI_TRUE);
				}
                }
				else
				{
					/* Unrecognized OUI type */
					readLen = (*pEleHdr)[1] + 2;
				}
			}
			else
			{
				readLen = (*pEleHdr)[1] + 2;
			}

			break;

#ifdef XCC_MODULE_INCLUDED
		case CELL_POWER_IE:
			/* We mustn't take the Cell Transmit Power IE into account if */
			/* there's a Power Constraint IE. Since the IEs must be in increasing */
			/* order, it's enough to perform the check here, because if the Power */
			/* Constraint IE is present it must have already been processed. */ 
			if (allowCellTP)
			{
				frame->cellTP = &params->cellTP;
				status = mlmeParser_readCellTP(pHandle, pData, bodyDataLen, &readLen, frame->cellTP);
				CHECK_PARSING_ERROR_CONDITION((status != TI_OK), ("MLME_PARSER: error reading Cell Transmit Power params.\n"),TI_TRUE);
			}
			break;
#endif

		default:
			TRACE1(pHandle->hReport, REPORT_SEVERITY_INFORMATION, "MLME_PARSER: unknown IE found (%d)\n", pData[0]);
			readLen = pData[1] + 2;
			status = TI_OK;
			os_memoryCopy( pHandle->hOs,
				       &params->unknownIe[params->frame.content.iePacket.unknownIeLen],
				       pData, readLen );
			params->frame.content.iePacket.pUnknownIe = params->unknownIe;
			params->frame.content.iePacket.unknownIeLen += readLen;
			break;
		}
		pData += readLen;
		bodyDataLen -= readLen;
#if CHECK_PARSING_ERROR_CONDITION_PRINT
		/* CHECK_PARSING_ERROR_CONDITION((bodyDataLen < 0), ("MLME_PARSER: negative bodyDataLen %d bytes\n", bodyDataLen),TI_TRUE); */
		if (bodyDataLen < 0)
		{
			TRACE1(pHandle->hReport, REPORT_SEVERITY_ERROR, "MLME_PARSER: negative bodyDataLen %d bytes\n", bodyDataLen);
			TRACE1(pHandle->hReport, REPORT_SEVERITY_ERROR, "Buff len = %d \n", packetLength);
			report_PrintDump (pPacketBody, packetLength);
		}
#endif
	}
	return TI_OK;
}

mlmeIEParsingParams_t *mlmeParser_getParseIEsBuffer(TI_HANDLE *hMlme)
{
	return (&(((mlme_t *)hMlme)->tempFrameInfo));
}


/**
*
* parseIeBuffer  - Parse a required information element.
*
* \b Description: 
*
* Parse an required information element
* and returns a pointer to the IE.
 * If given a matching buffer as well, returns a pointer to the first IE 
 * that matches the IE ID and the given buffer.
*
* \b ARGS:
*
*  I   - hOs - pointer to OS context
*  I   - pIeBuffer - pointer to the IE buffer  \n
*  I   - length - the length of the whole buffer
*  I   - desiredIeId - the desired IE ID 
*  O   - pDesiredIe - a pointer to the desired IE
*  I   - pMatchBuffer - a matching buffer in the IE buffer. Optional, if not required a NULL can be given. 
*  I   - matchBufferLen - the matching buffer length. Optional, if not required zero can be given. 
*  
*  
* \b RETURNS:
*
* TI_TRUE if IE pointer was found, TI_FALSE on failure. 
*
* \sa 
*/
TI_BOOL mlmeParser_ParseIeBuffer (TI_HANDLE hMlme, TI_UINT8 *pIeBuffer, TI_UINT32 length, TI_UINT8 desiredIeId, TI_UINT8 **pDesiredIe, TI_UINT8 *pMatchBuffer, TI_UINT32 matchBufferLen)
{
    mlme_t  		 *pMlme = (mlme_t *)hMlme;
    dot11_eleHdr_t   *eleHdr;
    TI_UINT8         *pCurIe;


    if (pDesiredIe!=NULL)
    {
        *pDesiredIe = NULL;
    }

    if ((pIeBuffer == NULL) || (length==0))
    {
       return TI_FALSE;    
    }

    pCurIe = pIeBuffer;
    
    while (length>0)
    {
        eleHdr = (dot11_eleHdr_t*)pCurIe;
        
        if ((TI_UINT8)length < ((*eleHdr)[1] + 2))
        {
            return TI_FALSE;
        }
        
        if ((*eleHdr)[0] == desiredIeId)
        {
            if ((matchBufferLen==0) || (pMatchBuffer == NULL) ||
                (!os_memoryCompare(pMlme->hOs, &pCurIe[2], pMatchBuffer, matchBufferLen)))
            {
                if (pDesiredIe!=NULL)
                {
                    *pDesiredIe = (TI_UINT8*)eleHdr;
                }
                return TI_TRUE;
            }

        }
        length -= (*eleHdr)[1] + 2;
        pCurIe += (*eleHdr)[1] + 2;
    }

    return TI_FALSE;
}


