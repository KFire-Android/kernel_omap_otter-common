/*
 * report.c
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


/** \file report.c
 *  \brief report module implementation
 *
 *  \see report.h
 */

#define __FILE_ID__  FILE_ID_132
#include "tidef.h"
#include "osApi.h"
#include "report.h"
#include "commonTypes.h"
#include "CmdInterfaceCodes.h"




/************************************************************************
 *                        report_create                              	*
 ************************************************************************/
TI_HANDLE report_Create (TI_HANDLE hOs)
{
    TReport *pReport;

    pReport = os_memoryAlloc(hOs, sizeof(TReport));
    if (!pReport)
    {
        return NULL;
    }

    pReport->hOs = hOs;

    os_memoryZero(hOs, pReport->aSeverityTable, sizeof(pReport->aSeverityTable));
    os_memoryZero(hOs, pReport->aFileEnable, sizeof(pReport->aFileEnable));


#ifdef PRINTF_ROLLBACK

	/* Fill the files names table */

    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_0  ]),  "timer                   "  ,  sizeof("timer                   "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_1  ]),  "measurementMgr          "  ,  sizeof("measurementMgr          "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_2  ]),  "measurementMgrSM        "  ,  sizeof("measurementMgrSM        "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_3  ]),  "regulatoryDomain        "  ,  sizeof("regulatoryDomain        "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_4  ]),  "requestHandler          "  ,  sizeof("requestHandler          "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_5  ]),  "SoftGemini              "  ,  sizeof("SoftGemini              "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_6  ]),  "spectrumMngmntMgr       "  ,  sizeof("spectrumMngmntMgr       "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_7  ]),  "SwitchChannel           "  ,  sizeof("SwitchChannel           "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_8  ]),  "roamingMngr             "  ,  sizeof("roamingMngr             "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_9  ]),  "scanMngr                "  ,  sizeof("scanMngr                "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_10 ]),  "admCtrlXCC              "  ,  sizeof("admCtrlXCC              "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_11 ]),  "XCCMngr                 "  ,  sizeof("XCCMngr                 "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_12 ]),  "XCCRMMngr               "  ,  sizeof("XCCRMMngr               "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_13 ]),  "XCCTSMngr               "  ,  sizeof("XCCTSMngr               "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_14 ]),  "rogueAp                 "  ,  sizeof("rogueAp                 "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_15 ]),  "TransmitPowerXCC        "  ,  sizeof("TransmitPowerXCC        "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_16 ]),  "admCtrl                 "  ,  sizeof("admCtrl                 "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_17 ]),  "admCtrlNone             "  ,  sizeof("admCtrlNone             "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_18 ]),  "admCtrlWep              "  ,  sizeof("admCtrlWep              "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_19 ]),  "admCtrlWpa              "  ,  sizeof("admCtrlWpa              "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_20 ]),  "admCtrlWpa2             "  ,  sizeof("admCtrlWpa2             "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_21 ]),  "apConn                  "  ,  sizeof("apConn                  "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_22 ]),  "broadcastKey802_1x      "  ,  sizeof("broadcastKey802_1x      "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_23 ]),  "broadcastKeyNone        "  ,  sizeof("broadcastKeyNone        "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_24 ]),  "broadcastKeySM          "  ,  sizeof("broadcastKeySM          "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_25 ]),  "conn                    "  ,  sizeof("conn                    "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_26 ]),  "connIbss                "  ,  sizeof("connIbss                "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_27 ]),  "connInfra               "  ,  sizeof("connInfra               "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_28 ]),  "keyDerive               "  ,  sizeof("keyDerive               "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_29 ]),  "keyDeriveAes            "  ,  sizeof("keyDeriveAes            "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_30 ]),  "keyDeriveCkip           "  ,  sizeof("keyDeriveCkip           "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_31 ]),  "keyDeriveTkip           "  ,  sizeof("keyDeriveTkip           "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_32 ]),  "keyDeriveWep            "  ,  sizeof("keyDeriveWep            "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_33 ]),  "keyParser               "  ,  sizeof("keyParser               "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_34 ]),  "keyParserExternal       "  ,  sizeof("keyParserExternal       "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_35 ]),  "keyParserWep            "  ,  sizeof("keyParserWep            "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_36 ]),  "mainKeysSm              "  ,  sizeof("mainKeysSm              "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_37 ]),  "mainSecKeysOnly         "  ,  sizeof("mainSecKeysOnly         "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_38 ]),  "mainSecNull             "  ,  sizeof("mainSecNull             "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_39 ]),  "mainSecSm               "  ,  sizeof("mainSecSm               "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_40 ]),  "rsn                     "  ,  sizeof("rsn                     "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_41 ]),  "sme                     "  ,  sizeof("sme                     "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_42 ]),  "smeSelect               "  ,  sizeof("smeSelect               "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_43 ]),  "smeSm                   "  ,  sizeof("smeSm                   "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_44 ]),  "unicastKey802_1x        "  ,  sizeof("unicastKey802_1x        "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_45 ]),  "unicastKeyNone          "  ,  sizeof("unicastKeyNone          "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_46 ]),  "unicastKeySM            "  ,  sizeof("unicastKeySM            "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_47 ]),  "CmdDispatcher           "  ,  sizeof("CmdDispatcher           "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_48 ]),  "CmdHndlr                "  ,  sizeof("CmdHndlr                "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_49 ]),  "DrvMain                 "  ,  sizeof("DrvMain                 "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_50 ]),  "EvHandler               "  ,  sizeof("EvHandler               "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_51 ]),  "Ctrl                    "  ,  sizeof("Ctrl                    "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_52 ]),  "GeneralUtil             "  ,  sizeof("GeneralUtil             "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_53 ]),  "RateAdaptation          "  ,  sizeof("RateAdaptation          "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_54 ]),  "rx                      "  ,  sizeof("rx                      "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_55 ]),  "TrafficMonitor          "  ,  sizeof("TrafficMonitor          "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_56 ]),  "txCtrl                  "  ,  sizeof("txCtrl                  "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_57 ]),  "txCtrlParams            "  ,  sizeof("txCtrlParams            "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_58 ]),  "txCtrlServ              "  ,  sizeof("txCtrlServ              "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_59 ]),  "TxDataClsfr             "  ,  sizeof("TxDataClsfr             "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_60 ]),  "txDataQueue             "  ,  sizeof("txDataQueue             "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_61 ]),  "txMgmtQueue             "  ,  sizeof("txMgmtQueue             "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_62 ]),  "txPort                  "  ,  sizeof("txPort                  "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_63 ]),  "assocSM                 "  ,  sizeof("assocSM                 "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_64 ]),  "authSm                  "  ,  sizeof("authSm                  "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_65 ]),  "currBss                 "  ,  sizeof("currBss                 "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_66 ]),  "healthMonitor           "  ,  sizeof("healthMonitor           "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_67 ]),  "mlmeBuilder             "  ,  sizeof("mlmeBuilder             "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_68 ]),  "mlmeParser              "  ,  sizeof("mlmeParser              "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_69 ]),  "mlmeSm                  "  ,  sizeof("mlmeSm                  "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_70 ]),  "openAuthSm              "  ,  sizeof("openAuthSm              "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_71 ]),  "PowerMgr                "  ,  sizeof("PowerMgr                "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_72 ]),  "PowerMgrDbgPrint        "  ,  sizeof("PowerMgrDbgPrint        "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_73 ]),  "PowerMgrKeepAlive       "  ,  sizeof("PowerMgrKeepAlive       "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_74 ]),  "qosMngr                 "  ,  sizeof("qosMngr                 "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_75 ]),  "roamingInt              "  ,  sizeof("roamingInt              "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_76 ]),  "ScanCncn                "  ,  sizeof("ScanCncn                "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_77 ]),  "ScanCncnApp             "  ,  sizeof("ScanCncnApp             "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_78 ]),  "ScanCncnOsSm            "  ,  sizeof("ScanCncnOsSm            "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_79 ]),  "ScanCncnSm              "  ,  sizeof("ScanCncnSm              "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_80 ]),  "ScanCncnSmSpecific      "  ,  sizeof("ScanCncnSmSpecific      "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_81 ]),  "scanResultTable         "  ,  sizeof("scanResultTable         "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_82 ]),  "scr                     "  ,  sizeof("scr                     "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_83 ]),  "sharedKeyAuthSm         "  ,  sizeof("sharedKeyAuthSm         "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_84 ]),  "siteHash                "  ,  sizeof("siteHash                "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_85 ]),  "siteMgr                 "  ,  sizeof("siteMgr                 "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_86 ]),  "StaCap                  "  ,  sizeof("StaCap                  "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_87 ]),  "systemConfig            "  ,  sizeof("systemConfig            "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_88 ]),  "templates               "  ,  sizeof("templates               "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_89 ]),  "trafficAdmControl       "  ,  sizeof("trafficAdmControl       "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_90 ]),  "CmdBld                  "  ,  sizeof("CmdBld                  "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_91 ]),  "CmdBldCfg               "  ,  sizeof("CmdBldCfg               "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_92 ]),  "CmdBldCfgIE             "  ,  sizeof("CmdBldCfgIE             "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_93 ]),  "CmdBldCmd               "  ,  sizeof("CmdBldCmd               "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_94 ]),  "CmdBldCmdIE             "  ,  sizeof("CmdBldCmdIE             "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_95 ]),  "CmdBldItr               "  ,  sizeof("CmdBldItr               "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_96 ]),  "CmdBldItrIE             "  ,  sizeof("CmdBldItrIE             "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_97 ]),  "CmdQueue                "  ,  sizeof("CmdQueue                "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_98 ]),  "RxQueue                 "  ,  sizeof("RxQueue                 "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_99 ]),  "txCtrlBlk               "  ,  sizeof("txCtrlBlk               "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_100]),  "txHwQueue               "  ,  sizeof("txHwQueue               "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_101]),  "CmdMBox                 "  ,  sizeof("CmdMBox                 "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_102]),  "eventMbox               "  ,  sizeof("eventMbox               "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_103]),  "fwDebug                 "  ,  sizeof("fwDebug                 "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_104]),  "FwEvent                 "  ,  sizeof("FwEvent                 "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_105]),  "HwInit                  "  ,  sizeof("HwInit                  "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_106]),  "RxXfer                  "  ,  sizeof("RxXfer                  "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_107]),  "txResult                "  ,  sizeof("txResult                "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_108]),  "txXfer                  "  ,  sizeof("txXfer                  "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_109]),  "MacServices             "  ,  sizeof("MacServices             "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_110]),  "MeasurementSrv          "  ,  sizeof("MeasurementSrv          "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_111]),  "measurementSrvDbgPrint  "  ,  sizeof("measurementSrvDbgPrint  "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_112]),  "MeasurementSrvSM        "  ,  sizeof("MeasurementSrvSM        "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_113]),  "PowerSrv                "  ,  sizeof("PowerSrv                "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_114]),  "PowerSrvSM              "  ,  sizeof("PowerSrvSM              "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_115]),  "ScanSrv                 "  ,  sizeof("ScanSrv                 "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_116]),  "ScanSrvSM               "  ,  sizeof("ScanSrvSM               "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_117]),  "TWDriver                "  ,  sizeof("TWDriver                "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_118]),  "TWDriverCtrl            "  ,  sizeof("TWDriverCtrl            "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_119]),  "TWDriverRadio           "  ,  sizeof("TWDriverRadio           "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_120]),  "TWDriverTx              "  ,  sizeof("TWDriverTx              "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_121]),  "TwIf                    "  ,  sizeof("TwIf                    "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_122]),  "SdioBusDrv              "  ,  sizeof("SdioBusDrv              "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_123]),  "TxnQueue                "  ,  sizeof("TxnQueue                "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_124]),  "WspiBusDrv              "  ,  sizeof("WspiBusDrv              "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_125]),  "context                 "  ,  sizeof("context                 "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_126]),  "freq                    "  ,  sizeof("freq                    "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_127]),  "fsm                     "  ,  sizeof("fsm                     "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_128]),  "GenSM                   "  ,  sizeof("GenSM                   "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_129]),  "mem                     "  ,  sizeof("mem                     "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_130]),  "queue                   "  ,  sizeof("queue                   "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_131]),  "rate                    "  ,  sizeof("rate                    "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_132]),  "report                  "  ,  sizeof("report                  "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_133]),  "stack                   "  ,  sizeof("stack                   "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_134]),  "externalSec             "  ,  sizeof("externalSec             "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_135]),  "roamingMngr_autoSM      "  ,  sizeof("roamingMngr_autoSM      "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_136]),  "roamingMngr_manualSM    "  ,  sizeof("roamingMngr_manualSM    "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_137]),  "cmdinterpretoid         "  ,  sizeof("cmdinterpretoid         "));
    os_memoryCopy(hOs, (void *)(pReport->aFileName[FILE_ID_138]),  "WlanDrvIf               "  ,  sizeof("WlanDrvIf               "));

#endif  /* PRINTF_ROLLBACK */

	/* Severity table description */

	os_memoryCopy(hOs, (char *)(pReport->aSeverityDesc[REPORT_SEVERITY_INIT]),              "INIT", sizeof("INIT"));
	os_memoryCopy(hOs, (char *)(pReport->aSeverityDesc[REPORT_SEVERITY_INFORMATION]),       "INFORMATION", sizeof("INFORMATION"));
	os_memoryCopy(hOs, (char *)(pReport->aSeverityDesc[REPORT_SEVERITY_WARNING]),           "WARNING", sizeof("WARNING"));
	os_memoryCopy(hOs, (char *)(pReport->aSeverityDesc[REPORT_SEVERITY_ERROR]),             "ERROR", sizeof("ERROR"));
	os_memoryCopy(hOs, (char *)(pReport->aSeverityDesc[REPORT_SEVERITY_FATAL_ERROR]),       "FATAL_ERROR", sizeof("FATAL_ERROR"));
	os_memoryCopy(hOs, (char *)(pReport->aSeverityDesc[REPORT_SEVERITY_SM]),                "SM", sizeof("SM"));
	os_memoryCopy(hOs, (char *)(pReport->aSeverityDesc[REPORT_SEVERITY_CONSOLE]),           "CONSOLE", sizeof("CONSOLE"));

    return (TI_HANDLE)pReport;
}

/************************************************************************
 *                        report_SetDefaults                            *
 ************************************************************************/
TI_STATUS report_SetDefaults (TI_HANDLE hReport, TReportInitParams *pInitParams)
{
	if( (NULL == hReport) || (NULL == pInitParams))
	{
		return TI_NOK;
	}

    report_SetReportFilesTable (hReport, (TI_UINT8 *)pInitParams->aFileEnable);
    report_SetReportSeverityTable (hReport, (TI_UINT8 *)pInitParams->aSeverityTable);
    
    return TI_OK;
}

/************************************************************************
 *                        report_unLoad                                 *
 ************************************************************************/
TI_STATUS report_Unload (TI_HANDLE hReport)
{
    TReport *pReport = (TReport *)hReport;

	if(NULL == pReport)
	{
		return TI_NOK;
	}

#if defined(TIWLN_WINCE30) && defined(TI_DBG)
    closeMyFile();
#endif

    os_memoryFree(pReport->hOs, pReport, sizeof(TReport));
    return TI_OK;
}


TI_STATUS report_SetReportModule(TI_HANDLE hReport, TI_UINT8 module_index)
{
	if(NULL == hReport)
	{
		return TI_NOK;
	}

    ((TReport *)hReport)->aFileEnable[module_index] = 1;

    return TI_OK;
}


TI_STATUS report_ClearReportModule(TI_HANDLE hReport, TI_UINT8 module_index)
{
	if(NULL == hReport)
	{
		return TI_NOK;
	}

    ((TReport *)hReport)->aFileEnable[module_index] = 0;

    return TI_OK;
}


TI_STATUS report_GetReportFilesTable(TI_HANDLE hReport, TI_UINT8 *pFiles)
{
    TI_UINT8 index;

	if( (NULL == hReport) || (NULL == pFiles))
	{
		return TI_NOK;
	}

    os_memoryCopy(((TReport *)hReport)->hOs, 
                  (void *)pFiles, 
                  (void *)(((TReport *)hReport)->aFileEnable), 
                  sizeof(((TReport *)hReport)->aFileEnable));

    for (index = 0; index < sizeof(((TReport *)hReport)->aFileEnable); index++)
    {
        pFiles[index] += '0';
    }

    return TI_OK;
}


TI_STATUS report_SetReportFilesTable(TI_HANDLE hReport, TI_UINT8 *pFiles)
{
    TI_UINT8 index;

	if( (NULL == hReport) || (NULL == pFiles))
	{
		return TI_NOK;
	}


    for (index = 0; index < sizeof(((TReport *)hReport)->aFileEnable); index++)
    {
        pFiles[index] -= '0';
    }

    os_memoryCopy(((TReport *)hReport)->hOs, 
                  (void *)(((TReport *)hReport)->aFileEnable), 
                  (void *)pFiles, 
                  sizeof(((TReport *)hReport)->aFileEnable));

    return TI_OK;
}


TI_STATUS report_GetReportSeverityTable(TI_HANDLE hReport, TI_UINT8 *pSeverities)
{
    TI_UINT8 index;

	if( (NULL == hReport) || (NULL == pSeverities))
	{

		return TI_NOK;
	}


    os_memoryCopy (((TReport *)hReport)->hOs, 
                   (void *)pSeverities, 
                   (void *)(((TReport *)hReport)->aSeverityTable), 
                   sizeof(((TReport *)hReport)->aSeverityTable));

    for (index = 0; index < sizeof(((TReport *)hReport)->aSeverityTable); index++)
    {
        pSeverities[index] += '0';
    }

    return TI_OK;
}


TI_STATUS report_SetReportSeverityTable(TI_HANDLE hReport, TI_UINT8 *pSeverities)
{
    TI_UINT8 index;

	if( (NULL == hReport) || (NULL == pSeverities))
	{
		return TI_NOK;
	}

    for (index = 0; index < sizeof(((TReport *)hReport)->aSeverityTable); index++)
    {
        pSeverities[index] -= '0';
    }

    os_memoryCopy(((TReport *)hReport)->hOs, 
                  (void *)(((TReport *)hReport)->aSeverityTable), 
                  (void *)pSeverities, 
                  sizeof(((TReport *)hReport)->aSeverityTable));

    return TI_OK;
}


/***********************************************************************
*                        report_setParam                                   
***********************************************************************/
TI_STATUS report_SetParam (TI_HANDLE hReport, TReportParamInfo *pParam)
{
	if( (NULL == hReport) || (NULL == pParam))
	{
		return TI_NOK;
	}

	switch (pParam->paramType)
	{
	case REPORT_MODULE_ON_PARAM:
		report_SetReportModule(hReport, pParam->content.aFileEnable[0]);
		break;

	case REPORT_MODULE_OFF_PARAM:
		report_ClearReportModule(hReport, pParam->content.aFileEnable[0]);
		break;

	case REPORT_MODULE_TABLE_PARAM:
		report_SetReportFilesTable(hReport, (TI_UINT8 *)pParam->content.aFileEnable);
		break;

	case REPORT_SEVERITY_TABLE_PARAM:
		report_SetReportSeverityTable(hReport, (TI_UINT8 *)pParam->content.aSeverityTable);
		break;

	case REPORT_PPMODE_VALUE_PARAM:
		os_setDebugMode((TI_BOOL)pParam->content.uReportPPMode);
		break;

	case REPORT_OUTPUT_TO_LOGGER_ON:
		os_setDebugOutputToLogger(TI_TRUE);
		break;

	case REPORT_OUTPUT_TO_LOGGER_OFF:
		os_setDebugOutputToLogger(TI_FALSE);
        break;

	default:
		TRACE1(hReport, REPORT_SEVERITY_ERROR, "Set param, Params is not supported, %d\n", pParam->paramType);
		return PARAM_NOT_SUPPORTED;
	}

	return TI_OK;
}

/***********************************************************************
 *                        report_getParam                                   
 ***********************************************************************/
TI_STATUS report_GetParam (TI_HANDLE hReport, TReportParamInfo *pParam)
{
	if( (NULL == hReport) || (NULL == pParam))
	{
		return TI_NOK;
	}

    switch (pParam->paramType)
    {
    case REPORT_MODULE_TABLE_PARAM:
        report_GetReportFilesTable(hReport, (TI_UINT8 *)pParam->content.aFileEnable);
        break;

    case REPORT_SEVERITY_TABLE_PARAM:
        report_GetReportSeverityTable(hReport, (TI_UINT8 *)pParam->content.aSeverityTable);
        break;

    default:
        TRACE1(hReport, REPORT_SEVERITY_ERROR, "Get param, Params is not supported, %d\n", pParam->paramType);
        return PARAM_NOT_SUPPORTED;
    }

    return TI_OK;
}


/************************************************************************
 *                        report_Dump                                 *
 ************************************************************************/
TI_STATUS report_Dump (TI_UINT8 *pBuffer, char *pString, TI_UINT32 size)
{
    TI_UINT32 index;
    TI_UINT8  temp_nibble;
	if( (NULL == pBuffer) || (NULL == pString))
	{
		return TI_NOK;
}

    /* Go over pBuffer and convert it to chars */ 
    for (index = 0; index < size; index++)
{
        /* First nibble */
        temp_nibble = (pBuffer[index] & 0x0F);
        if (temp_nibble <= 9)
	{
            pString[(index << 1) + 1] = temp_nibble + '0';
	}
        else
	{
            pString[(index << 1) + 1] = temp_nibble - 10 + 'A';
	}

        /* Second nibble */
        temp_nibble = ((pBuffer[index] & 0xF0) >> 4);
        if (temp_nibble <= 9)
{    
            pString[(index << 1)] = temp_nibble + '0';
	}   
        else
{
            pString[(index << 1)] = temp_nibble - 10 + 'A';
	 }
	}

    /* Put string terminator */
    pString[(size * 2)] = 0;

    return TI_OK;
	}


/* HEX DUMP for BDs !!! Debug code only !!! */
TI_STATUS report_PrintDump (TI_UINT8 *pData, TI_UINT32 datalen)
{
#ifdef TI_DBG
    TI_INT32  dbuflen=0;
    TI_UINT32 j;
    TI_CHAR   dbuf[50];
    static const TI_CHAR hexdigits[16] = "0123456789ABCDEF";

   
	if((NULL == pData)||(datalen <= 0)
)
	{
		return TI_NOK;
	}

   
    for(j=0; j < datalen;)
    {
        /* Add a byte to the line*/
        dbuf[dbuflen] =  hexdigits[(pData[j] >> 4)&0x0f];
        dbuf[dbuflen+1] = hexdigits[pData[j] & 0x0f];
        dbuf[dbuflen+2] = ' ';
        dbuf[dbuflen+3] = '\0';
        dbuflen += 3;
        j++;
        if((j % 16) == 0)
        {
            /* Dump a line every 16 hex digits*/
            WLAN_OS_REPORT(("%04.4x  %s\n", j-16, dbuf));
            dbuflen = 0;
        }
    }
    /* Flush if something has left in the line*/
    if(dbuflen)
        WLAN_OS_REPORT(("%04.4x  %s\n", j & 0xfff0, dbuf));
#endif
    return TI_OK;
}

void handleRunProblem(EProblemType prType)
{
    /* Here problem handling methods can be specified per problem type.
       They can be OS dependent and can contain debug prints, traceBack,
       fatal error initiation etc.
    */
    return; /* empty for now */
}
