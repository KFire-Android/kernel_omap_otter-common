/*
 * TWD_Debug.c
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

#include "tidef.h"
#include "TWDriver.h"
#include "rxXfer_api.h"
#include "report.h"
#include "osApi.h"
#include "eventMbox_api.h"
#include "CmdQueue_api.h"
#include "CmdMBox_api.h"
#include "FwEvent_api.h"
#include "fwDebug_api.h"
#include "CmdBld.h"


/* Phony address used by host access */
#define BB_REGISTER_ADDR_BASE              0x820000

/* Used for TWD Debug Tests */
typedef enum
{
/*
 * General
 */
/*	0x00	*/	TWD_PRINT_HELP,
/*	0x01	*/	TWD_PRINT_SYS_INFO,
/*	0x02	*/	TWD_SET_GENERIC_ADDR,
/*	0x03	*/	TWD_READ_MEM,
/*	0x04	*/	TWD_WRITE_MEM,

/*	0x05	*/	TWD_PRINT_ISTART,

/*	0x06	*/	TWD_PRINT_MBOX_QUEUE_INFO,
/*	0x07	*/	TWD_PRINT_MBOX_PRINT_CMD,
/*	0x08	*/	TWD_MAILBOX_HISTORY_PRINT,

/*	0x09	*/	TWD_MAC_REG,
/*	0x0A	*/	TWD_SET_ARM_CLOCK,
/*	0x0B	*/	TWD_SET_MAC_CLOCK,

/*
 * Rx
 */
/*	0x0C	*/	TWD_PRINT_RX_INFO,
/*	0x0D	*/	TWD_CLEAR_RX_INFO,

/*
 * Acx
 */
/*	0x0E	*/	TWD_PRINT_ACX_MAP,
/*	0x0F	*/	TWD_PRINT_ACX_STAT,

/*
 * General Debug
 */
/*	0x10	*/	TWD_PWR_SV_DBG,

/*	0x11	*/	TWD_PRINT_LIST_REGS_THROG_MBOX,
/*	0x12	*/	TWD_PRINT_LIST_MEM_THROG_MBOX,
/*	0x13	*/	TWD_SET_MAC_REGISTER_THROG_MBOX,
/*	0x14	*/	TWD_SET_PHY_REGISTER_THROG_MBOX,
/*	0x15	*/	TWD_SET_MEMORY_THROG_MBOX,

/*
 * Recover Debug
 */
/*	0x16	*/	TWD_CHECK_HW,
/*	0x17	*/	TWD_PRINT_HW_STATUS,

/*
 * Event MailBox
 */
/*	0x18	*/	TWD_PRINT_EVENT_MBOX_INFO,
/*	0x19	*/	TWD_PRINT_EVENT_MBOX_MASK,
/*	0x1A	*/	TWD_PRINT_EVENT_MBOX_UNMASK,

/*
 * Other
 */
TWD_PRINT_FW_EVENT_INFO,
TWD_PRINT_TW_IF_INFO,
TWD_PRINT_MBOX_INFO,
TWD_FORCE_TEMPLATES_RATES,

				TWD_DEBUG_TEST_MAX = 0xFF	/* mast be last!!! */

} TWD_DebugTest_e;


/* Used for Memory or Registers reading/writing*/
typedef enum
{
   TNETW_INTERNAL_RAM,
   TNETW_MAC_REGISTERS,
   TNETW_PHY_REGISTERS

} readWrite_MemoryType_e;
    
static void TWD_PrintMemRegsCB (TI_HANDLE hTWD, TI_UINT32 cmdCbStatus)
{
    TTwd    *pTWD = (TTwd *)hTWD;    
    int      i;
    TI_UINT8   *pBuf;
    TI_UINT32   result;

    if (cmdCbStatus != TI_OK)
    {
        WLAN_OS_REPORT(("Command complete error \n\n"));
        return;
    }

    result = (((TI_UINT32)pTWD->tPrintRegsBuf.addr) & 0xFFFF0000);
    
    switch (result)
    {
        case REGISTERS_BASE:
                WLAN_OS_REPORT(("MAC REGS (Base=0x%08x) = 0x%08x\n", 
                               ((TI_UINT32)pTWD->tPrintRegsBuf.addr)&0xFFFF,
                               *(TI_UINT32*)(pTWD->tPrintRegsBuf.value)));
                break;

        case BB_REGISTER_ADDR_BASE:
                WLAN_OS_REPORT(("PHY REGS (Base=0x%08x) = 0x%08x\n", 
                               ((TI_UINT32)pTWD->tPrintRegsBuf.addr)&0xFFFF,
                               *(TI_UINT32*)(pTWD->tPrintRegsBuf.value)));
                break;

        default: /* Memory*/
                for (i=0, pBuf = pTWD->tPrintRegsBuf.value; i < 256; i += 16, pBuf += 16)
                {
                    WLAN_OS_REPORT(("PrintBuf: 0x%08x: 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n", 
									pTWD->tPrintRegsBuf.addr+i, 
									pBuf[0], pBuf[1], pBuf[2], pBuf[3], pBuf[4], pBuf[5], pBuf[6], pBuf[7], 
									pBuf[8], pBuf[9], pBuf[10], pBuf[11], pBuf[12], pBuf[13], pBuf[14], pBuf[15]));
                }
                break;
    }
}


static void TWD_PrintMemRegs (TI_HANDLE hTWD, TI_UINT32 address, TI_UINT32 len, readWrite_MemoryType_e memType)
{
    TTwd  *pTWD = (TTwd *)hTWD;    
    ReadWriteCommand_t  AcxCmd_ReadMemory; 
    ReadWriteCommand_t* pCmd = &AcxCmd_ReadMemory;

    os_memoryZero (pTWD->hOs, (void *)pCmd, sizeof (*pCmd));
    
    switch (memType)
    {
        case TNETW_INTERNAL_RAM:
            pCmd->addr = (TI_UINT32)ENDIAN_HANDLE_LONG (address);
            pCmd->size = ENDIAN_HANDLE_LONG (len);
            break;
             
        case TNETW_MAC_REGISTERS:
            pCmd->addr = (TI_UINT32)ENDIAN_HANDLE_LONG (((address&0xFFFF) | REGISTERS_BASE));
            pCmd->size = 4;
            break;
    
        case TNETW_PHY_REGISTERS:
            pCmd->addr = (TI_UINT32)ENDIAN_HANDLE_LONG (((address&0xFFFF) | BB_REGISTER_ADDR_BASE));
            pCmd->size = 4;
            break;
    
        default:
            WLAN_OS_REPORT(("Wrong memory type %d\n\n", memType));
            return;
    }
    
    os_memoryZero (pTWD->hOs, (void *)&pTWD->tPrintRegsBuf, sizeof(pTWD->tPrintRegsBuf));
    
    cmdQueue_SendCommand (pTWD->hCmdQueue, 
                      CMD_READ_MEMORY, 
                      (char *)pCmd, 
                      sizeof(*pCmd), 
                      (void *)TWD_PrintMemRegsCB, 
                      hTWD, 
                      &pTWD->tPrintRegsBuf);
}

static TI_STATUS TWD_PrintMemoryMapCb (TI_HANDLE hTWD, TI_STATUS status, void *pData)
{
#ifdef REPORT_LOG
    TTwd        *pTWD = (TTwd *)hTWD;    
    MemoryMap_t  *pMemMap = &pTWD->MemMap;

    /* Print the memory map */
    WLAN_OS_REPORT (("TWD_PrintMemoryMap:\n"));
    WLAN_OS_REPORT (("\tCode  (0x%08x, 0x%08x)\n\tWep  (0x%08x, 0x%08x)\n\tTmpl (0x%08x, 0x%08x)\n "
                    "\tQueue (0x%08x, 0x%08x)\n\tPool (0x%08x, 0x%08x)\n\tTraceBuffer (A = 0x%08x, B = 0x%08x)\n",
                    pMemMap->codeStart, 
                    pMemMap->codeEnd,
                    pMemMap->wepDefaultKeyStart, 
                    pMemMap->wepDefaultKeyEnd,
                    pMemMap->packetTemplateStart, 
                    pMemMap->packetTemplateEnd,
                    pMemMap->queueMemoryStart, 
                    pMemMap->queueMemoryEnd,
                    pMemMap->packetMemoryPoolStart, 
                    pMemMap->packetMemoryPoolEnd,
                    pMemMap->debugBuffer1Start, 
                    pMemMap->debugBuffer2Start));
#endif

    return TI_OK;
}


/****************************************************************************
 *                      TWD_PrintMemoryMap ()
 ****************************************************************************
 * DESCRIPTION: Print some of the MemoryMap information element fields
 *
 * INPUTS:
 *          HwMboxConfig_T* pHwMboxConfig pointer to the acx mailbox
 *
 * OUTPUT:  None
 *
 * RETURNS: None
 ****************************************************************************/
static void TWD_PrintMemoryMap (TI_HANDLE hTWD)
{
    TTwd *pTWD = (TTwd *)hTWD;    

    TWD_ItrMemoryMap (pTWD, &pTWD->MemMap, (void *)TWD_PrintMemoryMapCb, hTWD);
}


/****************************************************************************
 *                      TWD_StatisticsReadCB ()
 ****************************************************************************
 * DESCRIPTION: Interrogate Statistics from the wlan hardware
 *
 * INPUTS:  None
 *
 * OUTPUT:  None
 *
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
static TI_STATUS TWD_StatisticsReadCB (TI_HANDLE hTWD, TI_UINT16 MboxStatus, ACXStatistics_t* pElem)
{
    if (MboxStatus != TI_OK)
    {
        return TI_NOK;
    }

    /* 
     *  Handle FW statistics endianess
     *  ==============================
     */

    /* Ring */
    pElem->ringStat.numOfTxProcs       = ENDIAN_HANDLE_LONG(pElem->ringStat.numOfTxProcs);
    pElem->ringStat.numOfPreparedDescs = ENDIAN_HANDLE_LONG(pElem->ringStat.numOfPreparedDescs);
    pElem->ringStat.numOfTxXfr         = ENDIAN_HANDLE_LONG(pElem->ringStat.numOfTxXfr);
    pElem->ringStat.numOfTxDma         = ENDIAN_HANDLE_LONG(pElem->ringStat.numOfTxDma);
    pElem->ringStat.numOfTxCmplt       = ENDIAN_HANDLE_LONG(pElem->ringStat.numOfTxCmplt);
    pElem->ringStat.numOfRxProcs       = ENDIAN_HANDLE_LONG(pElem->ringStat.numOfRxProcs);
    pElem->ringStat.numOfRxData        = ENDIAN_HANDLE_LONG(pElem->ringStat.numOfRxData);

    /* Debug */
    pElem->debug.debug1                = ENDIAN_HANDLE_LONG(pElem->debug.debug1);
    pElem->debug.debug2                = ENDIAN_HANDLE_LONG(pElem->debug.debug2);
    pElem->debug.debug3                = ENDIAN_HANDLE_LONG(pElem->debug.debug3);
    pElem->debug.debug4                = ENDIAN_HANDLE_LONG(pElem->debug.debug4);
    pElem->debug.debug5                = ENDIAN_HANDLE_LONG(pElem->debug.debug5);
    pElem->debug.debug6                = ENDIAN_HANDLE_LONG(pElem->debug.debug6);

    /* Isr */
    pElem->isr.IRQs                    = ENDIAN_HANDLE_LONG(pElem->isr.IRQs);

    /* Rx */
    pElem->rx.RxOutOfMem               = ENDIAN_HANDLE_LONG(pElem->rx.RxOutOfMem            );
    pElem->rx.RxHdrOverflow            = ENDIAN_HANDLE_LONG(pElem->rx.RxHdrOverflow         );
    pElem->rx.RxHWStuck                = ENDIAN_HANDLE_LONG(pElem->rx.RxHWStuck             );
    pElem->rx.RxDroppedFrame           = ENDIAN_HANDLE_LONG(pElem->rx.RxDroppedFrame        );
    pElem->rx.RxCompleteDroppedFrame   = ENDIAN_HANDLE_LONG(pElem->rx.RxCompleteDroppedFrame);
    pElem->rx.RxAllocFrame             = ENDIAN_HANDLE_LONG(pElem->rx.RxAllocFrame          );
    pElem->rx.RxDoneQueue              = ENDIAN_HANDLE_LONG(pElem->rx.RxDoneQueue           );
    pElem->rx.RxDone                   = ENDIAN_HANDLE_LONG(pElem->rx.RxDone                );
    pElem->rx.RxDefrag                 = ENDIAN_HANDLE_LONG(pElem->rx.RxDefrag              );
    pElem->rx.RxDefragEnd              = ENDIAN_HANDLE_LONG(pElem->rx.RxDefragEnd           );
    pElem->rx.RxMic                    = ENDIAN_HANDLE_LONG(pElem->rx.RxMic                 );
    pElem->rx.RxMicEnd                 = ENDIAN_HANDLE_LONG(pElem->rx.RxMicEnd              );
    pElem->rx.RxXfr                    = ENDIAN_HANDLE_LONG(pElem->rx.RxXfr                 );
    pElem->rx.RxXfrEnd                 = ENDIAN_HANDLE_LONG(pElem->rx.RxXfrEnd              );
    pElem->rx.RxCmplt                  = ENDIAN_HANDLE_LONG(pElem->rx.RxCmplt               );
    pElem->rx.RxPreCmplt               = ENDIAN_HANDLE_LONG(pElem->rx.RxPreCmplt            );
    pElem->rx.RxCmpltTask              = ENDIAN_HANDLE_LONG(pElem->rx.RxCmpltTask           );
    pElem->rx.RxPhyHdr                 = ENDIAN_HANDLE_LONG(pElem->rx.RxPhyHdr              );
    pElem->rx.RxTimeout                = ENDIAN_HANDLE_LONG(pElem->rx.RxTimeout             );

    /* Tx */
    pElem->tx.numOfTxTemplatePrepared  	= ENDIAN_HANDLE_LONG(pElem->tx.numOfTxTemplatePrepared);
	pElem->tx.numOfTxDataPrepared  		= ENDIAN_HANDLE_LONG(pElem->tx.numOfTxDataPrepared);
	pElem->tx.numOfTxTemplateProgrammed = ENDIAN_HANDLE_LONG(pElem->tx.numOfTxTemplateProgrammed);
	pElem->tx.numOfTxDataProgrammed  	= ENDIAN_HANDLE_LONG(pElem->tx.numOfTxDataProgrammed);
	pElem->tx.numOfTxBurstProgrammed  	= ENDIAN_HANDLE_LONG(pElem->tx.numOfTxBurstProgrammed);
	pElem->tx.numOfTxStarts  			= ENDIAN_HANDLE_LONG(pElem->tx.numOfTxStarts);
	pElem->tx.numOfTxImmResp  			= ENDIAN_HANDLE_LONG(pElem->tx.numOfTxImmResp);
	pElem->tx.numOfTxStartTempaltes  	= ENDIAN_HANDLE_LONG(pElem->tx.numOfTxStartTempaltes);
	pElem->tx.numOfTxStartIntTemplate  	= ENDIAN_HANDLE_LONG(pElem->tx.numOfTxStartIntTemplate);
	pElem->tx.numOfTxStartFwGen  		= ENDIAN_HANDLE_LONG(pElem->tx.numOfTxStartFwGen);
	pElem->tx.numOfTxStartData  		= ENDIAN_HANDLE_LONG(pElem->tx.numOfTxStartData);
	pElem->tx.numOfTxStartNullFrame  	= ENDIAN_HANDLE_LONG(pElem->tx.numOfTxStartNullFrame);
	pElem->tx.numOfTxExch  				= ENDIAN_HANDLE_LONG(pElem->tx.numOfTxExch);
	pElem->tx.numOfTxRetryTemplate  	= ENDIAN_HANDLE_LONG(pElem->tx.numOfTxRetryTemplate);
	pElem->tx.numOfTxRetryData  		= ENDIAN_HANDLE_LONG(pElem->tx.numOfTxRetryData);
	pElem->tx.numOfTxExchPending  		= ENDIAN_HANDLE_LONG(pElem->tx.numOfTxExchPending);
	pElem->tx.numOfTxExchExpiry  		= ENDIAN_HANDLE_LONG(pElem->tx.numOfTxExchExpiry);
	pElem->tx.numOfTxExchMismatch  		= ENDIAN_HANDLE_LONG(pElem->tx.numOfTxExchMismatch);
	pElem->tx.numOfTxDoneTemplate  		= ENDIAN_HANDLE_LONG(pElem->tx.numOfTxDoneTemplate);
	pElem->tx.numOfTxDoneData  			= ENDIAN_HANDLE_LONG(pElem->tx.numOfTxDoneData);
	pElem->tx.numOfTxDoneIntTemplate  	= ENDIAN_HANDLE_LONG(pElem->tx.numOfTxDoneIntTemplate);
	pElem->tx.numOfTxPreXfr  			= ENDIAN_HANDLE_LONG(pElem->tx.numOfTxPreXfr);
	pElem->tx.numOfTxXfr  				= ENDIAN_HANDLE_LONG(pElem->tx.numOfTxXfr);
	pElem->tx.numOfTxXfrOutOfMem  		= ENDIAN_HANDLE_LONG(pElem->tx.numOfTxXfrOutOfMem);
	pElem->tx.numOfTxDmaProgrammed  	= ENDIAN_HANDLE_LONG(pElem->tx.numOfTxDmaProgrammed);
	pElem->tx.numOfTxDmaDone  			= ENDIAN_HANDLE_LONG(pElem->tx.numOfTxDmaDone);

    /* Dma */
    pElem->dma.RxDMAErrors             = ENDIAN_HANDLE_LONG(pElem->dma.RxDMAErrors);
    pElem->dma.TxDMAErrors             = ENDIAN_HANDLE_LONG(pElem->dma.TxDMAErrors);

    /* Wep */
    pElem->wep.WepAddrKeyCount         = ENDIAN_HANDLE_LONG(pElem->wep.WepAddrKeyCount);
    pElem->wep.WepDefaultKeyCount      = ENDIAN_HANDLE_LONG(pElem->wep.WepDefaultKeyCount);
    pElem->wep.WepKeyNotFound          = ENDIAN_HANDLE_LONG(pElem->wep.WepKeyNotFound);
    pElem->wep.WepDecryptFail          = ENDIAN_HANDLE_LONG(pElem->wep.WepDecryptFail);
    
    /* AES */
    pElem->aes.AesEncryptFail          = ENDIAN_HANDLE_LONG(pElem->aes.AesEncryptFail);     
    pElem->aes.AesDecryptFail          = ENDIAN_HANDLE_LONG(pElem->aes.AesDecryptFail);     
    pElem->aes.AesEncryptPackets       = ENDIAN_HANDLE_LONG(pElem->aes.AesEncryptPackets);  
    pElem->aes.AesDecryptPackets       = ENDIAN_HANDLE_LONG(pElem->aes.AesDecryptPackets);  
    pElem->aes.AesEncryptInterrupt     = ENDIAN_HANDLE_LONG(pElem->aes.AesEncryptInterrupt);
    pElem->aes.AesDecryptInterrupt     = ENDIAN_HANDLE_LONG(pElem->aes.AesDecryptInterrupt);

    /* Events */
    pElem->event.calibration           = ENDIAN_HANDLE_LONG(pElem->event.calibration);
    pElem->event.rxMismatch            = ENDIAN_HANDLE_LONG(pElem->event.rxMismatch); 
    pElem->event.rxMemEmpty            = ENDIAN_HANDLE_LONG(pElem->event.rxMemEmpty); 

    /* PS */
    pElem->pwr.MissingBcnsCnt          	= ENDIAN_HANDLE_LONG(pElem->pwr.MissingBcnsCnt);
    pElem->pwr.RcvdBeaconsCnt          	= ENDIAN_HANDLE_LONG(pElem->pwr.RcvdBeaconsCnt);
    pElem->pwr.ConnectionOutOfSync     	= ENDIAN_HANDLE_LONG(pElem->pwr.ConnectionOutOfSync);
    pElem->pwr.ContMissBcnsSpread[0]   	= ENDIAN_HANDLE_LONG(pElem->pwr.ContMissBcnsSpread[0]);
    pElem->pwr.ContMissBcnsSpread[1]   	= ENDIAN_HANDLE_LONG(pElem->pwr.ContMissBcnsSpread[1]);
    pElem->pwr.ContMissBcnsSpread[2]   	= ENDIAN_HANDLE_LONG(pElem->pwr.ContMissBcnsSpread[2]);
    pElem->pwr.ContMissBcnsSpread[3]   	= ENDIAN_HANDLE_LONG(pElem->pwr.ContMissBcnsSpread[3]);
    pElem->pwr.ContMissBcnsSpread[4]   	= ENDIAN_HANDLE_LONG(pElem->pwr.ContMissBcnsSpread[4]);
    pElem->pwr.ContMissBcnsSpread[5]   	= ENDIAN_HANDLE_LONG(pElem->pwr.ContMissBcnsSpread[5]);
    pElem->pwr.ContMissBcnsSpread[6]   	= ENDIAN_HANDLE_LONG(pElem->pwr.ContMissBcnsSpread[6]);
    pElem->pwr.ContMissBcnsSpread[7]   	= ENDIAN_HANDLE_LONG(pElem->pwr.ContMissBcnsSpread[7]);
    pElem->pwr.ContMissBcnsSpread[8]   	= ENDIAN_HANDLE_LONG(pElem->pwr.ContMissBcnsSpread[8]);
    pElem->pwr.ContMissBcnsSpread[9]   	= ENDIAN_HANDLE_LONG(pElem->pwr.ContMissBcnsSpread[9]);

    pElem->ps.psPollTimeOuts           	= ENDIAN_HANDLE_LONG(pElem->ps.psPollTimeOuts);
    pElem->ps.upsdTimeOuts             	= ENDIAN_HANDLE_LONG(pElem->ps.upsdTimeOuts);
    pElem->ps.upsdMaxAPturn            	= ENDIAN_HANDLE_LONG(pElem->ps.upsdMaxAPturn); 
    pElem->ps.psPollMaxAPturn          	= ENDIAN_HANDLE_LONG(pElem->ps.psPollMaxAPturn);
    pElem->ps.psPollUtilization        	= ENDIAN_HANDLE_LONG(pElem->ps.psPollUtilization);
    pElem->ps.upsdUtilization          	= ENDIAN_HANDLE_LONG(pElem->ps.upsdUtilization);

	pElem->rxFilter.arpFilter		   	= ENDIAN_HANDLE_LONG(pElem->rxFilter.arpFilter);
	pElem->rxFilter.beaconFilter	   	= ENDIAN_HANDLE_LONG(pElem->rxFilter.beaconFilter);
	pElem->rxFilter.dataFilter		   	= ENDIAN_HANDLE_LONG(pElem->rxFilter.dataFilter);
	pElem->rxFilter.dupFilter		   	= ENDIAN_HANDLE_LONG(pElem->rxFilter.dupFilter);
	pElem->rxFilter.MCFilter		   	= ENDIAN_HANDLE_LONG(pElem->rxFilter.MCFilter);
	pElem->rxFilter.ibssFilter		   	= ENDIAN_HANDLE_LONG(pElem->rxFilter.ibssFilter);

	pElem->radioCal.calStateFail	   	= ENDIAN_HANDLE_LONG(pElem->radioCal.calStateFail);
	pElem->radioCal.initCalTotal	   	= ENDIAN_HANDLE_LONG(pElem->radioCal.initCalTotal);
	pElem->radioCal.initRadioBandsFail 	= ENDIAN_HANDLE_LONG(pElem->radioCal.initRadioBandsFail);
	pElem->radioCal.initRxIqMmFail		= ENDIAN_HANDLE_LONG(pElem->radioCal.initRxIqMmFail);
	pElem->radioCal.initSetParams		= ENDIAN_HANDLE_LONG(pElem->radioCal.initSetParams);
	pElem->radioCal.initTxClpcFail		= ENDIAN_HANDLE_LONG(pElem->radioCal.initTxClpcFail);
	pElem->radioCal.tuneCalTotal		= ENDIAN_HANDLE_LONG(pElem->radioCal.tuneCalTotal);
	pElem->radioCal.tuneDrpwChanTune	= ENDIAN_HANDLE_LONG(pElem->radioCal.tuneDrpwChanTune);
	pElem->radioCal.tuneDrpwLnaTank		= ENDIAN_HANDLE_LONG(pElem->radioCal.tuneDrpwLnaTank);
	pElem->radioCal.tuneDrpwPdBufFail	= ENDIAN_HANDLE_LONG(pElem->radioCal.tuneDrpwPdBufFail);
	pElem->radioCal.tuneDrpwRTrimFail	= ENDIAN_HANDLE_LONG(pElem->radioCal.tuneDrpwRTrimFail);
	pElem->radioCal.tuneDrpwRxDac		= ENDIAN_HANDLE_LONG(pElem->radioCal.tuneDrpwRxDac);
	pElem->radioCal.tuneDrpwRxIf2Gain	= ENDIAN_HANDLE_LONG(pElem->radioCal.tuneDrpwRxIf2Gain);
	pElem->radioCal.tuneDrpwRxTxLpf		= ENDIAN_HANDLE_LONG(pElem->radioCal.tuneDrpwRxTxLpf);
	pElem->radioCal.tuneDrpwTaCal		= ENDIAN_HANDLE_LONG(pElem->radioCal.tuneDrpwTaCal);
	pElem->radioCal.tuneDrpwTxMixFreqFail = ENDIAN_HANDLE_LONG(pElem->radioCal.tuneDrpwTxMixFreqFail);
	pElem->radioCal.tuneRxAnaDcFail		= ENDIAN_HANDLE_LONG(pElem->radioCal.tuneRxAnaDcFail);
	pElem->radioCal.tuneRxIqMmFail		= ENDIAN_HANDLE_LONG(pElem->radioCal.tuneRxIqMmFail);
	pElem->radioCal.tuneTxClpcFail		= ENDIAN_HANDLE_LONG(pElem->radioCal.tuneTxClpcFail);
	pElem->radioCal.tuneTxIqMmFail		= ENDIAN_HANDLE_LONG(pElem->radioCal.tuneTxIqMmFail);
	pElem->radioCal.tuneTxLOLeakFail	= ENDIAN_HANDLE_LONG(pElem->radioCal.tuneTxLOLeakFail);
	pElem->radioCal.tuneTxPdetFail		= ENDIAN_HANDLE_LONG(pElem->radioCal.tuneTxPdetFail);
	pElem->radioCal.tuneTxPPAFail		= ENDIAN_HANDLE_LONG(pElem->radioCal.tuneTxPPAFail);


    /* 
     *  Print FW statistics 
     *  ===================
     */

    /* Ring */
    WLAN_OS_REPORT(("------  Ring statistics  -------------------\n"));
    WLAN_OS_REPORT(("numOfTxProcs       = %d\n", pElem->ringStat.numOfTxProcs));
    WLAN_OS_REPORT(("numOfPreparedDescs = %d\n", pElem->ringStat.numOfPreparedDescs));
    WLAN_OS_REPORT(("numOfTxXfr         = %d\n", pElem->ringStat.numOfTxXfr));
    WLAN_OS_REPORT(("numOfTxDma         = %d\n", pElem->ringStat.numOfTxDma));
    WLAN_OS_REPORT(("numOfTxCmplt       = %d\n", pElem->ringStat.numOfTxCmplt));
    WLAN_OS_REPORT(("numOfRxProcs       = %d\n", pElem->ringStat.numOfRxProcs));
    WLAN_OS_REPORT(("numOfRxData        = %d\n", pElem->ringStat.numOfRxData));

    /* Debug */
    WLAN_OS_REPORT(("------  Debug statistics  -------------------\n"));
    WLAN_OS_REPORT(("debug1 = %d\n", pElem->debug.debug1));
    WLAN_OS_REPORT(("debug2 = %d\n", pElem->debug.debug2));
    WLAN_OS_REPORT(("debug3 = %d\n", pElem->debug.debug3));
    WLAN_OS_REPORT(("debug4 = %d\n", pElem->debug.debug4));
    WLAN_OS_REPORT(("debug5 = %d\n", pElem->debug.debug5));
    WLAN_OS_REPORT(("debug6 = %d\n", pElem->debug.debug6));

    /* Isr */
    WLAN_OS_REPORT(("------  Isr statistics  -------------------\n"));
    WLAN_OS_REPORT(("IRQs = %d\n", pElem->isr.IRQs));

    /* Rx */
    WLAN_OS_REPORT(("------  Rx  statistics  -------------------\n"));
    WLAN_OS_REPORT(("RxOutOfMem             = %d\n", pElem->rx.RxOutOfMem            ));
    WLAN_OS_REPORT(("RxHdrOverflow          = %d\n", pElem->rx.RxHdrOverflow         ));
    WLAN_OS_REPORT(("RxHWStuck              = %d\n", pElem->rx.RxHWStuck             ));
    WLAN_OS_REPORT(("RxDroppedFrame         = %d\n", pElem->rx.RxDroppedFrame        ));
    WLAN_OS_REPORT(("RxCompleteDroppedFrame = %d\n", pElem->rx.RxCompleteDroppedFrame));
    WLAN_OS_REPORT(("RxAllocFrame           = %d\n", pElem->rx.RxAllocFrame          ));
    WLAN_OS_REPORT(("RxDoneQueue            = %d\n", pElem->rx.RxDoneQueue           ));
    WLAN_OS_REPORT(("RxDone                 = %d\n", pElem->rx.RxDone                ));
    WLAN_OS_REPORT(("RxDefrag               = %d\n", pElem->rx.RxDefrag              ));
    WLAN_OS_REPORT(("RxDefragEnd            = %d\n", pElem->rx.RxDefragEnd           ));
    WLAN_OS_REPORT(("RxMic                  = %d\n", pElem->rx.RxMic                 ));
    WLAN_OS_REPORT(("RxMicEnd               = %d\n", pElem->rx.RxMicEnd              ));
    WLAN_OS_REPORT(("RxXfr                  = %d\n", pElem->rx.RxXfr                 ));
    WLAN_OS_REPORT(("RxXfrEnd               = %d\n", pElem->rx.RxXfrEnd              ));
    WLAN_OS_REPORT(("RxCmplt                = %d\n", pElem->rx.RxCmplt               ));
    WLAN_OS_REPORT(("RxPreCmplt             = %d\n", pElem->rx.RxPreCmplt            ));
    WLAN_OS_REPORT(("RxCmpltTask            = %d\n", pElem->rx.RxCmpltTask           ));
    WLAN_OS_REPORT(("RxPhyHdr               = %d\n", pElem->rx.RxPhyHdr              ));
    WLAN_OS_REPORT(("RxTimeout              = %d\n", pElem->rx.RxTimeout             ));

    WLAN_OS_REPORT(("------  RxFilters statistics  --------------\n"));
	WLAN_OS_REPORT(("arpFilter    = %d\n", pElem->rxFilter.arpFilter));
	WLAN_OS_REPORT(("beaconFilter = %d\n", pElem->rxFilter.beaconFilter));
	WLAN_OS_REPORT(("dataFilter   = %d\n", pElem->rxFilter.dataFilter));
	WLAN_OS_REPORT(("dupFilter    = %d\n", pElem->rxFilter.dupFilter));
	WLAN_OS_REPORT(("MCFilter     = %d\n", pElem->rxFilter.MCFilter));
	WLAN_OS_REPORT(("ibssFilter   = %d\n", pElem->rxFilter.ibssFilter));

    /* Tx */
    WLAN_OS_REPORT(("------  Tx  statistics  -------------------\n"));
	WLAN_OS_REPORT(("numOfTxTemplatePrepared    = %d\n", pElem->tx.numOfTxTemplatePrepared));
	WLAN_OS_REPORT(("numOfTxDataPrepared        = %d\n", pElem->tx.numOfTxDataPrepared));
	WLAN_OS_REPORT(("numOfTxTemplateProgrammed  = %d\n", pElem->tx.numOfTxTemplateProgrammed));
	WLAN_OS_REPORT(("numOfTxDataProgrammed      = %d\n", pElem->tx.numOfTxDataProgrammed));
	WLAN_OS_REPORT(("numOfTxBurstProgrammed     = %d\n", pElem->tx.numOfTxBurstProgrammed));
	WLAN_OS_REPORT(("numOfTxStarts              = %d\n", pElem->tx.numOfTxStarts));
	WLAN_OS_REPORT(("numOfTxImmResp             = %d\n", pElem->tx.numOfTxImmResp));
	WLAN_OS_REPORT(("numOfTxStartTempaltes      = %d\n", pElem->tx.numOfTxStartTempaltes));
	WLAN_OS_REPORT(("numOfTxStartIntTemplate    = %d\n", pElem->tx.numOfTxStartIntTemplate));
	WLAN_OS_REPORT(("numOfTxStartFwGen          = %d\n", pElem->tx.numOfTxStartFwGen));
	WLAN_OS_REPORT(("numOfTxStartData           = %d\n", pElem->tx.numOfTxStartData));
	WLAN_OS_REPORT(("numOfTxStartNullFrame      = %d\n", pElem->tx.numOfTxStartNullFrame));
	WLAN_OS_REPORT(("numOfTxExch                = %d\n", pElem->tx.numOfTxExch));
	WLAN_OS_REPORT(("numOfTxRetryTemplate       = %d\n", pElem->tx.numOfTxRetryTemplate));
	WLAN_OS_REPORT(("numOfTxRetryData           = %d\n", pElem->tx.numOfTxRetryData));
	WLAN_OS_REPORT(("numOfTxExchPending         = %d\n", pElem->tx.numOfTxExchPending));
	WLAN_OS_REPORT(("numOfTxExchExpiry          = %d\n", pElem->tx.numOfTxExchExpiry));
	WLAN_OS_REPORT(("numOfTxExchMismatch        = %d\n", pElem->tx.numOfTxExchMismatch));
	WLAN_OS_REPORT(("numOfTxDoneTemplate        = %d\n", pElem->tx.numOfTxDoneTemplate));
	WLAN_OS_REPORT(("numOfTxDoneData            = %d\n", pElem->tx.numOfTxDoneData));
	WLAN_OS_REPORT(("numOfTxDoneIntTemplate     = %d\n", pElem->tx.numOfTxDoneIntTemplate));
	WLAN_OS_REPORT(("numOfTxPreXfr              = %d\n", pElem->tx.numOfTxPreXfr));
	WLAN_OS_REPORT(("numOfTxXfr                 = %d\n", pElem->tx.numOfTxXfr));
	WLAN_OS_REPORT(("numOfTxXfrOutOfMem         = %d\n", pElem->tx.numOfTxXfrOutOfMem));
	WLAN_OS_REPORT(("numOfTxDmaProgrammed       = %d\n", pElem->tx.numOfTxDmaProgrammed));
	WLAN_OS_REPORT(("numOfTxDmaDone             = %d\n", pElem->tx.numOfTxDmaDone));

    /* Dma */
    WLAN_OS_REPORT(("------  Dma  statistics  -------------------\n"));
    WLAN_OS_REPORT(("RxDMAErrors  = %d\n", pElem->dma.RxDMAErrors));
    WLAN_OS_REPORT(("TxDMAErrors  = %d\n", pElem->dma.TxDMAErrors));

    /* Wep */
    WLAN_OS_REPORT(("------  Wep statistics  -------------------\n"));
    WLAN_OS_REPORT(("WepAddrKeyCount   = %d\n", pElem->wep.WepAddrKeyCount));
    WLAN_OS_REPORT(("WepDefaultKeyCount= %d\n", pElem->wep.WepDefaultKeyCount));
    WLAN_OS_REPORT(("WepKeyNotFound    = %d\n", pElem->wep.WepKeyNotFound));
    WLAN_OS_REPORT(("WepDecryptFail    = %d\n", pElem->wep.WepDecryptFail));

    /* AES */
    WLAN_OS_REPORT(("------------  AES Statistics --------------\n"));
    WLAN_OS_REPORT(("AesEncryptFail      = %d\n", pElem->aes.AesEncryptFail));
    WLAN_OS_REPORT(("AesDecryptFail      = %d\n", pElem->aes.AesDecryptFail));
    WLAN_OS_REPORT(("AesEncryptPackets   = %d\n", pElem->aes.AesEncryptPackets));
    WLAN_OS_REPORT(("AesDecryptPackets   = %d\n", pElem->aes.AesDecryptPackets));
    WLAN_OS_REPORT(("AesEncryptInterrupt = %d\n", pElem->aes.AesEncryptInterrupt));
    WLAN_OS_REPORT(("AesDecryptInterrupt = %d\n", pElem->aes.AesDecryptInterrupt));

    /* Events */
    WLAN_OS_REPORT(("------  Events  -------------------\n"));
    WLAN_OS_REPORT(("Calibration   = %d\n", pElem->event.calibration));
    WLAN_OS_REPORT(("rxMismatch    = %d\n", pElem->event.rxMismatch));
    WLAN_OS_REPORT(("rxMemEmpty    = %d\n", pElem->event.rxMemEmpty));

   /* PsPoll/Upsd */ 
    WLAN_OS_REPORT(("----------- PsPoll / Upsd -----------\n"));
    WLAN_OS_REPORT(("psPollTimeOuts     = %d\n",pElem->ps.psPollTimeOuts));
    WLAN_OS_REPORT(("upsdTimeOuts       = %d\n",pElem->ps.upsdTimeOuts));
    WLAN_OS_REPORT(("upsdMaxAPturn      = %d\n",pElem->ps.upsdMaxAPturn));
    WLAN_OS_REPORT(("psPollMaxAPturn    = %d\n",pElem->ps.psPollMaxAPturn));
    WLAN_OS_REPORT(("psPollUtilization  = %d\n",pElem->ps.psPollUtilization));
    WLAN_OS_REPORT(("upsdUtilization    = %d\n",pElem->ps.upsdUtilization));



	/* Calibration */
	WLAN_OS_REPORT(("----------- Calibrations -------------\n"));
	WLAN_OS_REPORT(("calStateFail         	= %d\n", pElem->radioCal.calStateFail));
	WLAN_OS_REPORT(("initCalTotal   		= %d\n", pElem->radioCal.initCalTotal));   	 
	WLAN_OS_REPORT(("initRadioBandsFail   	= %d\n", pElem->radioCal.initRadioBandsFail));
	WLAN_OS_REPORT(("initRxIqMmFail   		= %d\n", pElem->radioCal.initRxIqMmFail));
	WLAN_OS_REPORT(("initSetParams   		= %d\n", pElem->radioCal.initSetParams));
	WLAN_OS_REPORT(("initTxClpcFail   		= %d\n", pElem->radioCal.initTxClpcFail));	
	WLAN_OS_REPORT(("tuneCalTotal   		= %d\n", pElem->radioCal.tuneCalTotal));	
	WLAN_OS_REPORT(("tuneDrpwChanTune		= %d\n", pElem->radioCal.tuneDrpwChanTune));
	WLAN_OS_REPORT(("tuneDrpwLnaTank		= %d\n", pElem->radioCal.tuneDrpwLnaTank));
	WLAN_OS_REPORT(("tuneDrpwPdBufFail		= %d\n", pElem->radioCal.tuneDrpwPdBufFail));
	WLAN_OS_REPORT(("tuneDrpwRTrimFail		= %d\n", pElem->radioCal.tuneDrpwRTrimFail));
	WLAN_OS_REPORT(("tuneDrpwRxDac			= %d\n", pElem->radioCal.tuneDrpwRxDac));
	WLAN_OS_REPORT(("tuneDrpwRxIf2Gain		= %d\n", pElem->radioCal.tuneDrpwRxIf2Gain));
	WLAN_OS_REPORT(("tuneDrpwRxTxLpf		= %d\n", pElem->radioCal.tuneDrpwRxTxLpf));
	WLAN_OS_REPORT(("tuneDrpwTaCal			= %d\n", pElem->radioCal.tuneDrpwTaCal));
	WLAN_OS_REPORT(("tuneDrpwTxMixFreqFail	= %d\n", pElem->radioCal.tuneDrpwTxMixFreqFail));
	WLAN_OS_REPORT(("tuneRxAnaDcFail   		= %d\n", pElem->radioCal.tuneRxAnaDcFail));		
	WLAN_OS_REPORT(("tuneRxIqMmFail   		= %d\n", pElem->radioCal.tuneRxIqMmFail));
	WLAN_OS_REPORT(("tuneTxClpcFail   		= %d\n", pElem->radioCal.tuneTxClpcFail));
	WLAN_OS_REPORT(("tuneTxIqMmFail   		= %d\n", pElem->radioCal.tuneTxIqMmFail));
	WLAN_OS_REPORT(("tuneTxLOLeakFail   	= %d\n", pElem->radioCal.tuneTxLOLeakFail));
	WLAN_OS_REPORT(("tuneTxPdetFail   		= %d\n", pElem->radioCal.tuneTxPdetFail));
	WLAN_OS_REPORT(("tuneTxPPAFail   		= %d\n", pElem->radioCal.tuneTxPPAFail)); 



    /* Power Save Counters */
    WLAN_OS_REPORT(("------  Power management  ----------\n"));
    if(pElem->pwr.RcvdBeaconsCnt != 0)
    {
        WLAN_OS_REPORT(("MissingBcnsCnt    = %d (percentage <= %d) \n", 
                pElem->pwr.MissingBcnsCnt,
                ((pElem->pwr.MissingBcnsCnt * 100) / (pElem->pwr.RcvdBeaconsCnt + pElem->pwr.MissingBcnsCnt)) ));
    }
    else
    {
        WLAN_OS_REPORT(("MissingBcnsCnt    = %d (percentage = 0) \n", pElem->pwr.MissingBcnsCnt));
    }
    WLAN_OS_REPORT(("RcvdBeaconsCnt    = %d\n", pElem->pwr.RcvdBeaconsCnt));
    WLAN_OS_REPORT(("ConnectionOutOfSync    = %d\n\n", pElem->pwr.ConnectionOutOfSync));
    WLAN_OS_REPORT(("Single Missed Beacon           = %d\n", (pElem->pwr.ContMissBcnsSpread[0] & 0xFFFF)));
    WLAN_OS_REPORT(("2 Continuous Missed Beacons    = %d\n", (pElem->pwr.ContMissBcnsSpread[1] & 0xFFFF)));
    WLAN_OS_REPORT(("3 Continuous Missed Beacons    = %d\n", (pElem->pwr.ContMissBcnsSpread[2] & 0xFFFF)));
    WLAN_OS_REPORT(("4 Continuous Missed Beacons    = %d\n", (pElem->pwr.ContMissBcnsSpread[3] & 0xFFFF)));
    WLAN_OS_REPORT(("5 Continuous Missed Beacons    = %d\n", (pElem->pwr.ContMissBcnsSpread[4] & 0xFFFF)));
    WLAN_OS_REPORT(("6 Continuous Missed Beacons    = %d\n", (pElem->pwr.ContMissBcnsSpread[5] & 0xFFFF)));
    WLAN_OS_REPORT(("7 Continuous Missed Beacons    = %d\n", (pElem->pwr.ContMissBcnsSpread[6] & 0xFFFF)));
    WLAN_OS_REPORT(("8 Continuous Missed Beacons    = %d\n", (pElem->pwr.ContMissBcnsSpread[7] & 0xFFFF)));
    WLAN_OS_REPORT(("9 Continuous Missed Beacons    = %d\n", (pElem->pwr.ContMissBcnsSpread[8] & 0xFFFF)));
    WLAN_OS_REPORT((">=10 Continuous Missed Beacons = %d\n\n", (pElem->pwr.ContMissBcnsSpread[9] & 0xFFFF)));

    WLAN_OS_REPORT(("RcvdAwakeBeaconsCnt    = %d\n", pElem->pwr.RcvdAwakeBeaconsCnt));
    WLAN_OS_REPORT(("Single Missed Beacon        [Awake] = %d\n", (pElem->pwr.ContMissBcnsSpread[0] >> 16)));
    WLAN_OS_REPORT(("2 Continuous Missed Beacons [Awake] = %d\n", (pElem->pwr.ContMissBcnsSpread[1] >> 16)));
    WLAN_OS_REPORT(("3 Continuous Missed Beacons [Awake] = %d\n", (pElem->pwr.ContMissBcnsSpread[2] >> 16)));
    WLAN_OS_REPORT(("4 Continuous Missed Beacons [Awake] = %d\n", (pElem->pwr.ContMissBcnsSpread[3] >> 16)));
    WLAN_OS_REPORT(("5 Continuous Missed Beacons [Awake] = %d\n", (pElem->pwr.ContMissBcnsSpread[4] >> 16)));
    WLAN_OS_REPORT(("6 Continuous Missed Beacons [Awake] = %d\n", (pElem->pwr.ContMissBcnsSpread[5] >> 16)));
    WLAN_OS_REPORT(("7 Continuous Missed Beacons [Awake] = %d\n", (pElem->pwr.ContMissBcnsSpread[6] >> 16)));
    WLAN_OS_REPORT(("8 Continuous Missed Beacons [Awake] = %d\n", (pElem->pwr.ContMissBcnsSpread[7] >> 16)));
    WLAN_OS_REPORT(("9 Continuous Missed Beacons [Awake] = %d\n", (pElem->pwr.ContMissBcnsSpread[8] >> 16)));
    WLAN_OS_REPORT((">=10 Continuous Missed Beacons [Awake] = %d\n", (pElem->pwr.ContMissBcnsSpread[9] >> 16)));

    return TI_OK;  
}

TI_STATUS TWD_Debug (TI_HANDLE hTWD, TI_UINT32 funcType, void *pParam)
{
    TTwd *pTWD			= (TTwd *)hTWD;    
    TI_UINT32 GenericVal;
    TFwDebugParams* pMemDebug	= (TFwDebugParams*)pParam;

    static TI_UINT32 GenericAddr;
#ifdef REPORT_LOG
    static int    iStart[100]; /* Note: it is not used properly anyway */
#endif
	/* check paramemters validity */
	if (pMemDebug == NULL)
	{
		WLAN_OS_REPORT(("TWD_Debug: Error - pParam is NULL\n"));
		return TI_NOK;
	}

    switch (funcType)
    {
	case TWD_PRINT_SYS_INFO:
		WLAN_OS_REPORT(("PLATFORM = TNETW125x\n"));
		WLAN_OS_REPORT(("ACCESS MODE = SLAVE\n"));
		break;

	case TWD_SET_GENERIC_ADDR:
		/* check paramemters validity */
		if (pParam == NULL)
		{
			WLAN_OS_REPORT(("TWD_Debug, TWD_SET_GENERIC_ADDR Error: No Perameter received\n"));		
			return TI_NOK;
		}
		GenericAddr = *(TI_UINT32 *)pParam;
        break;

	case TWD_READ_MEM:
        WLAN_OS_REPORT(("TWD_Debug, TWD_READ_MEM, Addr:	0x%X\n", pMemDebug->addr));				

		/* check paramemters validity */
		if (pMemDebug == NULL)
		{
			WLAN_OS_REPORT(("TWD_Debug, TWD_READ_MEM Error: No Perameters received\n"));		
			return TI_NOK;
		}
  
		/* validate length */
		*(TI_UINT32*)&pMemDebug->length = 4;		

		/* If Address in valid Memory area and there is enough space for Length to R/W */
		if (TWD_isValidMemoryAddr(hTWD, pMemDebug) == TI_TRUE)
		{
			WLAN_OS_REPORT(("TWD_Debug, TWD_READ_MEM: Reading Valid memory Address\n"));		

			/* Init buf before reading */
			os_memorySet(pTWD->hOs, (void*)pMemDebug->UBuf.buf8, 0, 4);
			if ( TWD_readMem (hTWD, pMemDebug, NULL, NULL) != TI_OK )
			{
				WLAN_OS_REPORT(("TWD_Debug, read memory failed\n"));	
				return TI_NOK;
			}
		}

		else if (TWD_isValidRegAddr(hTWD, pMemDebug) == TI_TRUE)
		{
			WLAN_OS_REPORT(("TWD_Debug, TWD_READ_MEM: Reading Valid register Address\n"));		

			/* Init buf before reading */
			*(TI_UINT32*)&pMemDebug->UBuf.buf32 = 0;

			if ( TWD_readMem (hTWD, pMemDebug, NULL, NULL) != TI_OK )
			{
				WLAN_OS_REPORT(("TWD_Debug, read register failed\n"));	
				return TI_NOK;
			}

		}

		/* address Not in valid Area */ 
		else
		{
			WLAN_OS_REPORT(("TWD_Debug, TWD_READ_MEM Address is not Valid\n"));		
			return TI_NOK;
		}

		/* print read memory */
		{

			WLAN_OS_REPORT(("Read from MEM Addr 0x%x the following values:\n", ((TFwDebugParams*)pMemDebug)->addr));

			WLAN_OS_REPORT(("0x%X	",((TFwDebugParams*)pMemDebug)->UBuf.buf32[0]));
			WLAN_OS_REPORT(("\n"));
		}

		break;

	case TWD_WRITE_MEM:
        WLAN_OS_REPORT(("TWD_Debug, TWD_WRITE_MEM, Addr:	0x%X\n", pMemDebug->addr));				

		/* check paramemters validity */
		if (pMemDebug == NULL)
		{
			WLAN_OS_REPORT(("TWD_Debug, TWD_WRITE_MEM Error: No Perameters received\n"));				
			return TI_NOK;
		}

		/* validate length */
		*(TI_UINT32*)&pMemDebug->length = 4;		

		/* If Address in valid Memory area and there is enough space for Length to R/W */
		if (TWD_isValidMemoryAddr(hTWD, pMemDebug) == TI_TRUE)
		{
			WLAN_OS_REPORT(("TWD_Debug, TWD_WRITE_MEM:	Writing Valid memory Address\n"));		


			return ( TWD_writeMem (hTWD, pMemDebug, NULL, NULL) );
		}
		
		else if (TWD_isValidRegAddr(hTWD, pMemDebug) == TI_TRUE)
		{

			WLAN_OS_REPORT(("TWD_Debug, TWD_WRITE_MEM: Writing Valid register Address\n"));		

			return ( TWD_writeMem (hTWD, pMemDebug, NULL, NULL) );
		}
		/* address Not in valid Area */ 

		else
		{
			WLAN_OS_REPORT(("TWD_Debug, TWD_WRITE_MEM Address is not Valid\n"));		
			return TI_NOK;
		}

		break;

         /*  HAL Control functions */

	case TWD_PRINT_MBOX_QUEUE_INFO:
		cmdQueue_Print (pTWD->hCmdQueue);         
        break;
            
	case TWD_PRINT_MBOX_PRINT_CMD:
		/* check paramemters validity */
		if (pParam == NULL)
		{
			WLAN_OS_REPORT(("TWD_Debug, TWD_PRINT_MBOX_PRINT_CMD Error: No Perameter received\n"));				
			return TI_NOK;
		}
		cmdQueue_PrintHistory (pTWD->hCmdQueue, *(int *)pParam);            
        break;

	case TWD_PRINT_EVENT_MBOX_INFO:
		eventMbox_Print (pTWD->hEventMbox);         
        break;
        
	case TWD_PRINT_EVENT_MBOX_MASK:
		/* check paramemters validity */
		if (pParam == NULL)
		{
			WLAN_OS_REPORT(("TWD_Debug, TWD_PRINT_EVENT_MBOX_MASK Error: No Perameter received\n"));				
			return TI_NOK;
		}
		if ( eventMbox_MaskEvent (pTWD->hEventMbox, *(int *)pParam, NULL, NULL) == TI_NOK )
		{
			WLAN_OS_REPORT(("TWD_Debug, TWD_PRINT_EVENT_MBOX_MASK Error: eventMbox_EvMask failed\n"));				
			return(TI_NOK);
		}
        break;
        
	case TWD_PRINT_EVENT_MBOX_UNMASK:
		/* check paramemters validity */
		if (pParam == NULL)
		{
			WLAN_OS_REPORT(("TWD_Debug, TWD_PRINT_EVENT_MBOX_UNMASK Error: No Perameter received\n"));				
			return TI_NOK;
		}
		if ( eventMbox_UnMaskEvent (pTWD->hEventMbox, *(int *)pParam, NULL, NULL) == TI_NOK )
		{
			WLAN_OS_REPORT(("TWD_Debug, TWD_PRINT_EVENT_MBOX_UNMASK Error: eventMbox_EvUnMask failed\n"));				
			return(TI_NOK);
		}
        break;

	case TWD_PRINT_ISTART:
		{
			int i;
            for (i=0; i<20; i+=4)
            {
				WLAN_OS_REPORT(("%4d: %08d %08d %08d %08d\n", 
								i, iStart[i+0], iStart[i+1], iStart[i+2], iStart[i+3]));
			}
		}
        break;

	case TWD_PRINT_LIST_REGS_THROG_MBOX:
		{
			int i;
            TI_UINT32 RegAddr;
    
			RegAddr = *(TI_UINT32 *)pParam;
			WLAN_OS_REPORT (("PrintListRegsThroughMbox ---------------------\n"));
    
			for (i = 0; i < 8; i++, RegAddr += 16)
            {
				TWD_PrintMemRegs (hTWD, RegAddr +  0, 4, TNETW_MAC_REGISTERS);
                TWD_PrintMemRegs (hTWD, RegAddr +  4, 4, TNETW_MAC_REGISTERS);
				TWD_PrintMemRegs (hTWD, RegAddr +  8, 4, TNETW_MAC_REGISTERS);
				TWD_PrintMemRegs (hTWD, RegAddr + 12, 4, TNETW_MAC_REGISTERS);
			}
		}
        break;
    
	case TWD_PRINT_LIST_MEM_THROG_MBOX:
		/* check paramemters validity */
		if (pParam == NULL)
		{
			WLAN_OS_REPORT(("TWD_Debug, TWD_PRINT_LIST_MEM_THROG_MBOX Error: No Perameter received\n"));				
			return TI_NOK;
		}
		TWD_PrintMemRegs (hTWD, *(TI_UINT32*)pParam, 256, TNETW_INTERNAL_RAM);
        break;
                  
	case TWD_SET_MAC_CLOCK:
		/* check paramemters validity */
		if (pParam == NULL)
		{
			WLAN_OS_REPORT(("TWD_Debug, TWD_SET_MAC_CLOCK Error: No Perameter received\n"));				
			return TI_NOK;
		}

		GenericVal = *(TI_UINT32*)pParam;
        TWD_CfgMacClock (hTWD, GenericVal);
        break;

#if defined(TNETW1150)
	case TWD_SET_ARM_CLOCK:
		/* check paramemters validity */
		if (pParam == NULL)
		{
			WLAN_OS_REPORT(("TWD_Debug, TWD_SET_ARM_CLOCK Error: No Perameter received\n"));				
			return TI_NOK;
		}

		GenericVal = *(TI_UINT32*)pParam;
        TWD_ArmClockSet (hTWD, GenericVal);
        break;
#endif

	/*
    * Rx functions 
    */
#ifdef TI_DBG
    case TWD_PRINT_RX_INFO:
		rxXfer_PrintStats (pTWD->hRxXfer);  
        break;

	case TWD_CLEAR_RX_INFO:
		rxXfer_ClearStats (pTWD->hRxXfer);  
        break;

#endif /* TI_DBG */

	/*
    * Acx functions 
    */
    case TWD_PRINT_ACX_MAP:
		TWD_PrintMemoryMap (hTWD);           
        break;
            
	case TWD_PRINT_ACX_STAT:
		TWD_ItrStatistics (hTWD, (void*)TWD_StatisticsReadCB, hTWD, (void *)&pTWD->acxStatistic);          
        break;
            			
	/*
	* General functions 
    */
    case TWD_PRINT_HELP:

		WLAN_OS_REPORT(("Registers: \n"));
        WLAN_OS_REPORT(("        %02d - TWD_PRINT_SYS_INFO \n\n", TWD_PRINT_SYS_INFO));
        WLAN_OS_REPORT(("        %02d - TWD_SET_GENERIC_ADDR \n", TWD_SET_GENERIC_ADDR));
		WLAN_OS_REPORT(("        %02d - TWD_READ_REG_OR_4_BYTES_MEM <addr (reg base=0x300000, mem base=0x40000)>\n", TWD_READ_MEM));
		WLAN_OS_REPORT(("        %02d - TWD_WRITE_REG_OR_4_BYTES_MEM <addr (reg base=0x300000, mem base=0x40000)> <val (chars<=4)>\n", TWD_WRITE_MEM));

        WLAN_OS_REPORT(("Control: \n"));
        WLAN_OS_REPORT(("        %02d - TWD_PRINT_MBOX_QUEUE_INFO \n",TWD_PRINT_MBOX_QUEUE_INFO));
        WLAN_OS_REPORT(("        %02d - TWD_PRINT_MBOX_QUEUE_PRINT_CMD \n",TWD_PRINT_MBOX_PRINT_CMD));
        WLAN_OS_REPORT(("        %02d - TWD_MAILBOX_HISTORY_PRINT \n", TWD_MAILBOX_HISTORY_PRINT));
        WLAN_OS_REPORT(("        %02d - TWD_MAC_REG \n", TWD_MAC_REG));
        WLAN_OS_REPORT(("        %02d - TWD_SET_ARM_CLOCK \n", TWD_SET_ARM_CLOCK));
        WLAN_OS_REPORT(("        %02d - TWD_SET_MAC_CLOCK \n", TWD_SET_MAC_CLOCK));

        WLAN_OS_REPORT(("Rx: \n"));
        WLAN_OS_REPORT(("        %02d - TWD_PRINT_RX_INFO \n", TWD_PRINT_RX_INFO));
        WLAN_OS_REPORT(("        %02d - TWD_CLEAR_RX_INFO \n", TWD_CLEAR_RX_INFO));

        WLAN_OS_REPORT(("ACX: \n"));
        WLAN_OS_REPORT(("        %02d - TWD_PRINT_ACX_MAP \n", TWD_PRINT_ACX_MAP));
		WLAN_OS_REPORT(("        %02d - TWD_PRINT_ACX_STAT \n", TWD_PRINT_ACX_STAT));

        WLAN_OS_REPORT(("General: \n")); 
        WLAN_OS_REPORT(("        %02d - TWD_PRINT_LIST_REGS_THROG_MBOX \n",  TWD_PRINT_LIST_REGS_THROG_MBOX));
        WLAN_OS_REPORT(("        %02d - TWD_PRINT_LIST_MEM_THROG_MBOX \n",  TWD_PRINT_LIST_MEM_THROG_MBOX));
            
        WLAN_OS_REPORT(("Recovery: \n")); 
        WLAN_OS_REPORT(("        %02d - TWD_CHECK_HW \n", TWD_CHECK_HW));
        WLAN_OS_REPORT(("        %02d - TWD_PRINT_HW_STATUS \n", TWD_PRINT_HW_STATUS));

		WLAN_OS_REPORT(("Event Mail Box: \n"));
        WLAN_OS_REPORT(("        %02d - PRINT EVENT MBOX INFO \n",  TWD_PRINT_EVENT_MBOX_INFO));
        WLAN_OS_REPORT(("        %02d - PRINT EVENT MBOX MASK \n",  TWD_PRINT_EVENT_MBOX_MASK));
        WLAN_OS_REPORT(("        %02d - PRINT EVENT MBOX UNMASK \n",TWD_PRINT_EVENT_MBOX_UNMASK));

		WLAN_OS_REPORT(("Other: \n"));
        WLAN_OS_REPORT(("        %02d - TWD_PRINT_FW_EVENT_INFO \n",  TWD_PRINT_FW_EVENT_INFO));
        WLAN_OS_REPORT(("        %02d - TWD_PRINT_TW_IF_INFO \n",  TWD_PRINT_TW_IF_INFO));
        WLAN_OS_REPORT(("        %02d - TWD_PRINT_MBOX_INFO \n",  TWD_PRINT_MBOX_INFO));
        WLAN_OS_REPORT(("        %02d - TWD_FORCE_TEMPLATES_RATES \n",  TWD_FORCE_TEMPLATES_RATES));
        break;
       
	case TWD_PRINT_FW_EVENT_INFO:
        fwEvent_PrintStat(pTWD->hFwEvent);
        break;
	case TWD_PRINT_TW_IF_INFO:
        twIf_PrintQueues(pTWD->hTwIf);
        break;
	case TWD_PRINT_MBOX_INFO:
        cmdMbox_PrintInfo(pTWD->hCmdMbox);
        break;

    /*
    * Recovery functions 
    */       
	case TWD_CHECK_HW:
		{
			int Stt;

            Stt = TWD_CmdHealthCheck (hTWD);
            WLAN_OS_REPORT(("CheckHwStatus=%d \n", Stt));
		}
        break;

	case TWD_MAILBOX_HISTORY_PRINT:
		WLAN_OS_REPORT (("PrintMailBoxHistory called \n"));
#ifdef TI_DBG
		/* check paramemters validity */
		if (pParam == NULL)
		{
			WLAN_OS_REPORT(("TWD_Debug, TWD_MAILBOX_HISTORY_PRINT Error: No Perameter received\n"));				
			return TI_NOK;
		}

        cmdQueue_PrintHistory (pTWD->hCmdQueue, *(int *)pParam);
#endif
        break;

	case TWD_FORCE_TEMPLATES_RATES:
		if (pParam == NULL)
		{
			WLAN_OS_REPORT(("TWD_Debug, TWD_FORCE_TEMPLATES_RATES Error: No Perameter received\n"));		
			return TI_NOK;
		}
		cmdBld_DbgForceTemplatesRates (pTWD->hCmdBld, *(TI_UINT32 *)pParam);
        break;


	default:
		WLAN_OS_REPORT (("Invalid function type=%d\n\n", funcType));
        break;

	} /* switch (funcType) */

    return TI_OK;
} 

