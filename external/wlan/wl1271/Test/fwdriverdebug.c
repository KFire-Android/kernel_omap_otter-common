/*
 * fwdriverdebug.c
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
/*    MODULE:   FW_debug.c                                                 */
/*    PURPOSE:  FW debug implementation                                    */
/*                                                                         */
/***************************************************************************/
#include "tidef.h"
#include "DrvMain.h"
#include "CmdQueue_api.h"
#include "debug.h"
#include "connDebug.h"
#include "siteMgrDebug.h"
#include "dataCtrlDbg.h"
#include "rsnDbg.h"
#include "osApi.h"
#include "report.h"
#include "qosMngrDbg.h"
#include "PowerMgrDebug.h"
#include "roamingMgrDebug.h"
#include "scanCncnDbg.h"
#include "ScanMngrDbg.h"
#include "scrDbg.h"
#include "SoftGeminiDbg.h"
#include "HealthMonitorDbg.h"
#include "smeDebug.h"
#include "TWDriver.h"
#include "CmdBld.h"
#include "commonTypes.h"
#include "siteHash.h"
#include "txCtrl.h"
#include "802_11Defs.h"
#include "fwdriverdebug.h"
#include "mlmeBuilder.h"
#include "Ethernet.h"

static TI_HANDLE dTimer;
static TI_HANDLE tmp_hMlme;
static TI_HANDLE tmp_hTWD;
static TI_HANDLE tmp_hTxCtrl;
static TI_BOOL SendFlag = 0;
static TI_BOOL bSendDataPkt;
static volatile TI_UINT32 numOfPackets;
static volatile TI_UINT8 infinitLoopFl = 0;
static TI_UINT32 packetsNum;
static TI_UINT32 packetLength;

typedef TI_STATUS (*TCmdCfgFuncB) (TI_HANDLE);

extern TI_STATUS cmdBld_CmdJoinBss (TI_HANDLE hCmdBld, TJoinBss *pJoinBssParams, void *fCb, TI_HANDLE hCb);
void sendMgmtPacket (TI_HANDLE hOs);
void sendDataPacket (TI_HANDLE hOs);

void FWDebugFunction(TI_HANDLE hDrvMain, 
					 TI_HANDLE hOs, 
					 TI_HANDLE hTWD, 
					 TI_HANDLE hMlme, 
					 TI_HANDLE hTxMgmtQ,
					 TI_HANDLE hTxCtrl,
					 unsigned long funcType, 
					 void *pParam)
{
	tmp_hMlme   = hMlme;
    tmp_hTWD    = hTWD;
    tmp_hTxCtrl = hTxCtrl;

	switch (funcType)
	{
	case DBG_FW_PRINT_HELP:
		 printFWDbgFunctions();
		 break;
	case DBG_FW_SEND_GENERAL_TEST_CMD:
		 FW_debugSendGeneralTestCmd(hTWD, pParam);
		 break;
	case DBG_FW_IBSS_CONNECTION:
		 FW_DebugSendJoinCommand(hTWD, hTxMgmtQ);
		 break;
	case DBG_FW_SEND_MGMT_PACKET:
         bSendDataPkt = TI_FALSE;
		 FW_DebugSendPacket(hDrvMain, hOs, hTxMgmtQ, pParam);
		 break;
    case DBG_FW_SEND_DATA_PACKET: 
         bSendDataPkt = TI_TRUE;
		 FW_DebugSendPacket(hDrvMain, hOs, hTxMgmtQ, pParam);
		 break;
	case DBG_FW_START_LOOPBACK:
		 FW_DebugStartLoopBack (hDrvMain, hTWD);
		 break;
	case DBG_FW_STOP_LOOPBACK:
		 FW_DebugStopLoopBack (hDrvMain, hTWD);
		 break;
	case DBG_FW_INFINIT_SEND:
		 FW_DebugInfinitSendPacket (hDrvMain, hTWD);
		 break;

	case DBG_FW_GENERAL:
		 FW_DebugGeneral (hTWD, pParam);
		 break;
	default:
		break;


 }
}

void printFWDbgFunctions(void)
{
    WLAN_OS_REPORT(("    FW Debug Functions     	          	\n"));
    WLAN_OS_REPORT(("---------------------------------------------------\n"));
    WLAN_OS_REPORT(("2200 		- Print the FW Debug Help		\n"));
    WLAN_OS_REPORT(("2201 		- Send General Test Command 	\n"));
    WLAN_OS_REPORT(("2202 		- create IBSS connection   	 	\n"));
    WLAN_OS_REPORT(("2203[n] 	- Send n<=999 Packets           \n"));
    WLAN_OS_REPORT(("2205 		- Start LoopBack            	\n"));
    WLAN_OS_REPORT(("2206 		- Stop LoopBack             	\n"));
}


void FW_debugSendGeneralTestCmd(TI_HANDLE hTWD, void *pParam)
{
	TTwd     *pTWD = (TTwd *)hTWD;
	FWDebugTestCmdParamter_t Cfg;
	unsigned char Len;
	Len = *(unsigned char *)pParam;
	
	os_memoryCopy(pTWD->hOs, Cfg.buf,(unsigned long*)pParam + sizeof(TI_UINT8),Len * 4 );
  
	Cfg.len = Len;
	
	/* Set information element header */
	Cfg.EleHdr.id = CMD_TEST;
	Cfg.EleHdr.len = sizeof(Cfg) - sizeof(EleHdrStruct);
	  
    cmdQueue_SendCommand (pTWD->hCmdQueue, CMD_CONFIGURE, (void*)&Cfg, sizeof(Cfg), NULL, NULL, NULL);
}

void FW_DebugSendJoinCommand(TI_HANDLE hTWD, TI_HANDLE hTxMgmtQ)
{
	TTwd     *pTWD = (TTwd *)hTWD;
	TCmdBld  *pCmdBld = (TCmdBld *)(pTWD->hCmdBld);
	TI_STATUS res;
	TJoinBss  JoinBss;
	char ssid[10];
	unsigned char Bssid[6] = {0x0,0x12,0x43,0xad,0xe5,0x10};

	os_memoryCopy(pTWD->hOs,ssid,"ronit",5);
  
	JoinBss.basicRateSet = DRV_RATE_2M;
	JoinBss.radioBand = RADIO_BAND_2_4_GHZ;
	JoinBss.pBSSID = Bssid;
	
	JoinBss.bssType = BSS_INDEPENDENT;
	JoinBss.pSSID =ssid;
	JoinBss.ssidLength = 5;
	JoinBss.channel = 1;
	JoinBss.beaconInterval = 100;
	JoinBss.dtimInterval = 2;
  
	pCmdBld->hReport = pTWD->hReport;
	pCmdBld->tDb.bss.BssId[0] = 0x0;
	pCmdBld->tDb.bss.BssId[1] = 0x12;
	pCmdBld->tDb.bss.BssId[2] = 0x43;
	pCmdBld->tDb.bss.BssId[3] = 0xad;
	pCmdBld->tDb.bss.BssId[4] = 0xe5;
	pCmdBld->tDb.bss.BssId[5] = 0x10;
	pCmdBld->tDb.bss.RadioChannel = 1;
   
	txMgmtQ_SetConnState(hTxMgmtQ,TX_CONN_STATE_MGMT);
  
	res = cmdBld_CmdJoinBss  (pCmdBld, 
							  &JoinBss, 
							  NULL,//cmdBld_ConfigSeq, 
							  NULL);//pTWD->hCmdBld
  
	if (res)
	{
		os_printf("\n failed to make IBSS connection\n");
	}
  
	os_printf("\n res = %d\n", res);
   
	txMgmtQ_SetConnState(hTxMgmtQ, TX_CONN_STATE_MGMT);

}


void sendDataPacket (TI_HANDLE hOs)
{
    TI_UINT32       i;
	TTxCtrlBlk *    pPktCtrlBlk;
    TI_UINT8 *      pPktBuf;
    TEthernetHeader tEthHeader;
	char SrcBssid[6] = {0x88,0x88,0x88,0x88,0x88,0x88};
	char DesBssid[6] = {0x22,0x22,0x22,0x22,0x22,0x22};

	/* Allocate a TxCtrlBlk for the Tx packet and save timestamp, length and packet handle */
    pPktCtrlBlk = TWD_txCtrlBlk_Alloc (tmp_hTWD);
    pPktCtrlBlk->tTxDescriptor.startTime = os_timeStampMs (hOs);
    pPktCtrlBlk->tTxDescriptor.length    = (TI_UINT16)packetLength + ETHERNET_HDR_LEN;
    pPktCtrlBlk->tTxDescriptor.tid       = 0;
    pPktCtrlBlk->tTxPktParams.uPktType   = TX_PKT_TYPE_ETHER;

    /* Allocate buffer with headroom for getting the IP header in a 4-byte aligned address */
    pPktBuf = txCtrl_AllocPacketBuffer (tmp_hTxCtrl, pPktCtrlBlk, packetLength + ETHERNET_HDR_LEN + 2);

    /* Prepare Ethernet header */
    tEthHeader.type = HTOWLANS(ETHERTYPE_IP);
    MAC_COPY (tEthHeader.src, SrcBssid);
    MAC_COPY (tEthHeader.dst, DesBssid);
    os_memoryCopy (hOs, pPktBuf + 2, &tEthHeader, ETHERNET_HDR_LEN);

    /* Build BDL */
    BUILD_TX_TWO_BUF_PKT_BDL (pPktCtrlBlk, 
                              pPktBuf + 2, 
                              ETHERNET_HDR_LEN, 
                              pPktBuf + 2 + ETHERNET_HDR_LEN, 
                              packetLength)

    /* Fill data buffer with incremented numbers */
    for (i = 0; i < packetLength; i++) 
    {
        *(pPktBuf + 2 + ETHERNET_HDR_LEN + i) = i;
    }

    /* Send Ether packet to TxCtrl */
    txCtrl_XmitData (tmp_hTxCtrl, pPktCtrlBlk);
}



void sendMgmtPacket(TI_HANDLE hOs)
{
    static TI_UINT8     aMsg[2000];
    TI_UINT32           i;
    dot11MgmtSubType_e  eMsgType = DE_AUTH;
    
    for (i = 0; i < packetLength; i++) 
    {
        aMsg[i] = i;
    }

    mlmeBuilder_sendFrame(tmp_hMlme, eMsgType, aMsg, packetLength, 0);
    
    numOfPackets++;
    if ((infinitLoopFl == 0) && (numOfPackets > packetsNum))
    {      
        os_timerStop(hOs, dTimer);
        os_printf("\n *********** Last Packet was sent **********");
        os_timerDestroy(hOs, dTimer);
    }
    else
    {
        os_timerStart(hOs, dTimer, 1000); 
    }
}

void FW_DebugSendPacket(TI_HANDLE hDrvMain ,TI_HANDLE hOs, TI_HANDLE hTxMgmtQ, void *pParam)
{
    void *fSendPkt;

	if ( pParam == NULL )
	{
        os_printf("\nFW_DebugSendPacket Error: received NULL parameter\n");
		return;
	}

    /* Open Tx path for all packet types */
    txMgmtQ_SetConnState (hTxMgmtQ, TX_CONN_STATE_MGMT);
    txMgmtQ_SetConnState (hTxMgmtQ, TX_CONN_STATE_EAPOL);
    txMgmtQ_SetConnState (hTxMgmtQ, TX_CONN_STATE_OPEN);

	infinitLoopFl	= 0;
	numOfPackets 	= 1;
	packetsNum 		= 1;
	packetLength    = *(TI_UINT32*)pParam;
    os_printf("\nFW_DebugSendPacket: packetsNum = %d, packetLength = %d\n",packetsNum, packetLength);
    fSendPkt = bSendDataPkt ? sendDataPacket : sendMgmtPacket;
    dTimer = os_timerCreate(hOs, fSendPkt, hDrvMain);
    os_timerStart(hOs, dTimer, 1000);       
    
    SendFlag = TI_TRUE;   
}

void FW_DebugInfinitSendPacket(TI_HANDLE hDrvMain ,TI_HANDLE hOs)
{
	infinitLoopFl = 1;
	numOfPackets = 1;

    dTimer = os_timerCreate(hOs, sendMgmtPacket, hDrvMain); 
    os_timerStart(hOs, dTimer, 1000);        
    SendFlag = TI_TRUE;   
}

void FW_DebugStartLoopBack (TI_HANDLE hDrvMain, TI_HANDLE hTWD)
{    
	TTwd     *pTWD = (TTwd *)hTWD;
	TTestCmd     Plt;
	  
	Plt.testCmdId = TEST_CMD_LOOPBACK_START;
	os_printf("\n send loopback start command");  
	cmdQueue_SendCommand (pTWD->hCmdQueue, CMD_TEST, (char*)&Plt, sizeof(Plt), NULL, NULL, NULL);
}


void FW_DebugStopLoopBack (TI_HANDLE hDrvMain, TI_HANDLE hTWD)
{    
	TTwd     *pTWD = (TTwd *)hTWD;
	TTestCmd     Plt;
		
	Plt.testCmdId = TEST_CMD_LOOPBACK_STOP;
	os_printf("\n send loopback stop command");
	cmdQueue_SendCommand (pTWD->hCmdQueue, CMD_TEST, (char*)&Plt, sizeof(Plt), NULL, NULL, NULL);  
}


void FW_DebugGeneral(TI_HANDLE hTWD, void *pParam)
{
	TTwd     *pTWD = (TTwd *)hTWD;
	TI_UINT32 size = *((TI_UINT32*) pParam) + sizeof(TI_UINT32);

	cmdQueue_SendCommand (pTWD->hCmdQueue, CMD_DEBUG, (char*)pParam, size, NULL, NULL, NULL);
}


/*
void FW_ComparePacket (char *buf)
 {
 char *ptemp;
 int i;

   if (SendFlag)
   {
       ptemp = CompBuf;
       os_printf("\n print before compare save buf");
       os_printf("\n");
       for (i=0;i<Len;i++) {
           os_printf(" 0x%02x  ",*ptemp++);
           if (i %5 == 0) { os_printf("\n");
           }

       }

       os_printf("\n print before compare recived packet");
       ptemp = buf;
       os_printf("\n");
       for (i=0;i<Len;i++) {
           os_printf(" 0x%02x  ",*ptemp++);
           if (i %5 == 0) { 
           }

       }


     if(memcmp(CompBuf,buf,Len))
       os_printf("\n LoopBack Packet failed");
      else
      os_printf("\n LoopBack Packet success");

      SendFlag = TI_FALSE;
   }

 }
*/


