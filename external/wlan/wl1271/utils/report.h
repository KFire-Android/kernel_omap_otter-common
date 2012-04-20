/*
 * report.h
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
/*                                                                          */
/*    MODULE:   report.h                                                    */
/*    PURPOSE:  Report module internal header API                           */
/*                                                                          */
/***************************************************************************/
#ifndef __REPORT_H__
#define __REPORT_H__

/** \file  report.h 
 * \brief Report module API	\n
 * APIs which are used for reporting messages to the User while running. \n\n
 * 
 * The report mechanism: Messages are reported per file and severity Level	\n
 * Therefore, each file has a report flag which indicate if reporting for that file is enabled, \n
 * and each severity has a severity flag which indicate if reporting for that severity is enabled.	\n
 * Only if both flags are enabled, the message is printed. \n
 * The report flags of all file are indicated in a bit map Table which is contained in the report module handle	\n
 * The report flags of all severities  are indicated in a bit map Table which is contained in the report module handle	\n
 */

/* in order to work without the driver logger use that definition here  
 * #define PRINTF_ROLLBACK
 */

#include "osApi.h"
#include "commonTypes.h"
    
#define MAX_STRING_LEN         32 


/*******************************/
/*      Report Files IDs       */
/*******************************/

typedef enum
{
	FILE_ID_0	   ,    /*   timer                    */
	FILE_ID_1	   ,    /*   measurementMgr			  */
	FILE_ID_2	   ,    /*   measurementMgrSM         */
	FILE_ID_3	   ,    /*   regulatoryDomain         */
	FILE_ID_4	   ,    /*   requestHandler           */
	FILE_ID_5	   ,    /*   SoftGemini               */
	FILE_ID_6	   ,    /*   spectrumMngmntMgr        */
	FILE_ID_7	   ,    /*   SwitchChannel            */
	FILE_ID_8	   ,    /*   roamingMngr              */
	FILE_ID_9	   ,    /*   scanMngr                 */
	FILE_ID_10	   ,    /*   admCtrlXCC               */
	FILE_ID_11	   ,    /*   XCCMngr                  */
	FILE_ID_12	   ,    /*   XCCRMMngr                */
	FILE_ID_13	   ,    /*   XCCTSMngr                */
	FILE_ID_14	   ,    /*   rogueAp                  */
	FILE_ID_15	   ,    /*   TransmitPowerXCC         */
	FILE_ID_16	   ,    /*   admCtrl                  */
	FILE_ID_17	   ,    /*   admCtrlNone              */
	FILE_ID_18	   ,    /*   admCtrlWep               */
	FILE_ID_19	   ,    /*   admCtrlWpa               */
	FILE_ID_20	   ,    /*   admCtrlWpa2              */
	FILE_ID_21	   ,    /*   apConn                   */
	FILE_ID_22	   ,    /*   broadcastKey802_1x       */
	FILE_ID_23	   ,    /*   broadcastKeyNone         */
	FILE_ID_24	   ,    /*   broadcastKeySM           */
	FILE_ID_25	   ,    /*   conn                     */
	FILE_ID_26	   ,    /*   connIbss                 */
	FILE_ID_27	   ,    /*   connInfra                */
	FILE_ID_28	   ,    /*   keyDerive                */
	FILE_ID_29	   ,    /*   keyDeriveAes             */
	FILE_ID_30	   ,    /*   keyDeriveCkip            */
	FILE_ID_31	   ,    /*   keyDeriveTkip            */
	FILE_ID_32	   ,    /*   keyDeriveWep             */
	FILE_ID_33	   ,    /*   keyParser                */
	FILE_ID_34	   ,    /*   keyParserExternal        */
	FILE_ID_35	   ,    /*   keyParserWep             */
	FILE_ID_36	   ,    /*   mainKeysSm               */
	FILE_ID_37	   ,    /*   mainSecKeysOnly          */
	FILE_ID_38	   ,    /*   mainSecNull              */
	FILE_ID_39	   ,    /*   mainSecSm                */
	FILE_ID_40	   ,    /*   rsn                      */
	FILE_ID_41	   ,    /*   sme                      */
	FILE_ID_42	   ,    /*   smeSelect                */
	FILE_ID_43	   ,    /*   smeSm                    */
	FILE_ID_44	   ,    /*   unicastKey802_1x         */
	FILE_ID_45	   ,    /*   unicastKeyNone           */
	FILE_ID_46	   ,    /*   unicastKeySM             */
	FILE_ID_47	   ,    /*   CmdDispatcher            */
	FILE_ID_48	   ,    /*   CmdHndlr                 */
	FILE_ID_49	   ,    /*   DrvMain                  */
	FILE_ID_50	   ,    /*   EvHandler                */
	FILE_ID_51	   ,    /*   Ctrl                     */
	FILE_ID_52	   ,    /*   GeneralUtil              */
	FILE_ID_53	   ,    /*   RateAdaptation           */
	FILE_ID_54	   ,    /*   rx                       */
	FILE_ID_55	   ,    /*   TrafficMonitor           */
	FILE_ID_56	   ,    /*   txCtrl                   */
	FILE_ID_57	   ,    /*   txCtrlParams             */
	FILE_ID_58	   ,    /*   txCtrlServ               */
	FILE_ID_59	   ,    /*   TxDataClsfr              */
	FILE_ID_60	   ,    /*   txDataQueue              */
	FILE_ID_61	   ,    /*   txMgmtQueue              */
	FILE_ID_62	   ,    /*   txPort                   */
	FILE_ID_63	   ,    /*   assocSM                  */
	FILE_ID_64	   ,    /*   authSm                   */
	FILE_ID_65	   ,    /*   currBss                  */
	FILE_ID_66	   ,    /*   healthMonitor            */
	FILE_ID_67	   ,    /*   mlmeBuilder              */
	FILE_ID_68	   ,    /*   mlmeParser               */
	FILE_ID_69	   ,    /*   mlmeSm                   */
	FILE_ID_70	   ,    /*   openAuthSm               */
	FILE_ID_71	   ,    /*   PowerMgr                 */
	FILE_ID_72	   ,    /*   PowerMgrDbgPrint         */
	FILE_ID_73	   ,    /*   PowerMgrKeepAlive        */
	FILE_ID_74	   ,    /*   qosMngr                  */
	FILE_ID_75	   ,    /*   roamingInt               */
	FILE_ID_76	   ,    /*   ScanCncn                 */
	FILE_ID_77	   ,    /*   ScanCncnApp              */
	FILE_ID_78	   ,    /*   ScanCncnOsSm             */
	FILE_ID_79	   ,    /*   ScanCncnSm               */
	FILE_ID_80	   ,    /*   ScanCncnSmSpecific       */
	FILE_ID_81	   ,    /*   scanResultTable          */
	FILE_ID_82	   ,    /*   scr                      */
	FILE_ID_83	   ,    /*   sharedKeyAuthSm          */
	FILE_ID_84	   ,    /*   siteHash                 */
	FILE_ID_85	   ,    /*   siteMgr                  */
	FILE_ID_86	   ,    /*   StaCap                   */
	FILE_ID_87	   ,    /*   systemConfig             */
	FILE_ID_88	   ,    /*   templates                */
	FILE_ID_89	   ,    /*   trafficAdmControl        */
	FILE_ID_90	   ,    /*   CmdBld                   */
	FILE_ID_91	   ,    /*   CmdBldCfg                */
	FILE_ID_92	   ,    /*   CmdBldCfgIE              */
	FILE_ID_93	   ,    /*   CmdBldCmd                */
	FILE_ID_94	   ,    /*   CmdBldCmdIE              */
	FILE_ID_95	   ,    /*   CmdBldItr                */
	FILE_ID_96	   ,    /*   CmdBldItrIE              */
	FILE_ID_97	   ,    /*   CmdQueue                 */
	FILE_ID_98	   ,    /*   RxQueue                  */
	FILE_ID_99	   ,    /*   txCtrlBlk                */
	FILE_ID_100	   ,    /*   txHwQueue                */
	FILE_ID_101	   ,    /*   CmdMBox                  */
	FILE_ID_102	   ,    /*   eventMbox                */
	FILE_ID_103	   ,    /*   fwDebug                  */
	FILE_ID_104	   ,    /*   FwEvent                  */
	FILE_ID_105	   ,    /*   HwInit                   */
	FILE_ID_106	   ,    /*   RxXfer                   */
	FILE_ID_107	   ,    /*   txResult                 */
	FILE_ID_108	   ,    /*   txXfer                   */
	FILE_ID_109	   ,    /*   MacServices              */
	FILE_ID_110	   ,    /*   MeasurementSrv           */
	FILE_ID_111	   ,    /*   measurementSrvDbgPrint   */
	FILE_ID_112	   ,    /*   MeasurementSrvSM         */
	FILE_ID_113	   ,    /*   PowerSrv                 */
	FILE_ID_114	   ,    /*   PowerSrvSM               */
	FILE_ID_115	   ,    /*   ScanSrv                  */
	FILE_ID_116	   ,    /*   ScanSrvSM                */
	FILE_ID_117	   ,    /*   TWDriver                 */
	FILE_ID_118	   ,    /*   TWDriverCtrl             */
	FILE_ID_119	   ,    /*   TWDriverRadio            */
	FILE_ID_120	   ,    /*   TWDriverTx               */
	FILE_ID_121	   ,    /*   TwIf                     */
	FILE_ID_122	   ,    /*   SdioBusDrv               */
	FILE_ID_123	   ,    /*   TxnQueue                 */
	FILE_ID_124	   ,    /*   WspiBusDrv               */
	FILE_ID_125	   ,    /*   context                  */
	FILE_ID_126	   ,    /*   freq                     */
	FILE_ID_127	   ,    /*   fsm                      */
	FILE_ID_128	   ,    /*   GenSM                    */
	FILE_ID_129	   ,    /*   mem                      */
	FILE_ID_130	   ,    /*   queue                    */
	FILE_ID_131	   ,    /*   rate                     */
	FILE_ID_132	   ,    /*   report                   */
	FILE_ID_133	   ,    /*   stack                    */
    FILE_ID_134	   ,    /*   externalSec              */
    FILE_ID_135	   ,    /*   roamingMngr_autoSM       */
    FILE_ID_136	   ,    /*   roamingMngr_manualSM     */
	FILE_ID_137	   ,    /*   cmdinterpretoid          */
    FILE_ID_138	   ,    /*   wlandrvif                */
	REPORT_FILES_NUM	/*   Number of files with trace reports   */

} EReportFiles;
 

/************************************/      
/*      Report Severity values      */
/************************************/

/** \enum EReportSeverity
 * \brief Report Severity Types
 * 
 * \par Description
 * All available severity Levels of the events which are reported to user.\n
 * 
 * \sa
 */
typedef enum
{
    REPORT_SEVERITY_INIT           =  1,	/**< Init Level (event happened during initialization)			*/
    REPORT_SEVERITY_INFORMATION    =  2,	/**< Information Level											*/
    REPORT_SEVERITY_WARNING        =  3,	/**< Warning Level												*/
    REPORT_SEVERITY_ERROR          =  4,	/**< Error Level (error accored)  		 						*/
    REPORT_SEVERITY_FATAL_ERROR    =  5,	/**< Fatal-Error Level (fatal-error accored)					*/
    REPORT_SEVERITY_SM             =  6,	/**< State-Machine Level (event happened in State-Machine)		*/
    REPORT_SEVERITY_CONSOLE        =  7,	/**< Consol Level (event happened in Consol) 					*/
    REPORT_SEVERITY_MAX            = REPORT_SEVERITY_CONSOLE + 1	/**< Maximum number of report severity levels	*/

} EReportSeverity;

/** \enum EProblemType
 * \brief used to handle SW problems according to their types.
 *
 * \par Description
 * All available SW problem types checked in run time.\n
 *
 * \sa
 */
typedef enum
{
    PROBLEM_BUF_SIZE_VIOLATION    =  1,
    PROBLEM_NULL_VALUE_PTR        =  2,

    PROBLEM_MAX_VALUE

} EProblemType;
/** \struct TReport
 * \brief Report Module Object
 * 
 * \par Description
 * All the Databases and other parameters which are needed for reporting messages to user
 * 
 * \sa
 */
typedef struct 
{
    TI_HANDLE       hOs;												/**< Handle to Operating System Object																									*/
    TI_UINT8        aSeverityTable[REPORT_SEVERITY_MAX];				/**< Severities Table: Table which holds for each severity level a flag which indicates whether the severity is enabled for reporting	*/
	char            aSeverityDesc[REPORT_SEVERITY_MAX][MAX_STRING_LEN];	/**< Severities Descriptors Table: Table which holds for each severity a string of its name, which is used in severity's reported messages		*/
    TI_UINT8        aFileEnable[REPORT_FILES_NUM];					    /**< Files table indicating per file if it is enabled for reporting	 */

#ifdef PRINTF_ROLLBACK
    char            aFileName[REPORT_FILES_NUM][MAX_STRING_LEN];	    /**< Files names table inserted in the file's reported messages		 */
#endif

} TReport;

/** \struct TReportParamInfo
 * \brief Report Parameter Information
 * 
 * \par Description
 * Struct which defines all the Databases and other parameters which are needed
 * for reporting messages to user. 
 * The actual Content of the Report Parameter Could be one of the followed (held in union): 
 * Severety Table | Module Table | Enable/Disable indication of debug module usage
 * 
 * \sa	EExternalParam, ETwdParam
 */
typedef struct
{
    TI_UINT32       paramType;								/**< The reported parameter type - one of External Parameters (which includes Set,Get, Module, internal number etc.)
															* of Report Module. Used by user for Setting or Getting Report Module Paramters, for exaple Set/Get severety table
															* to/from Report Module
															*/
    TI_UINT32       paramLength;							/**< Length of reported parameter	*/

    union
    {
        TI_UINT8    aSeverityTable[REPORT_SEVERITY_MAX];	/**< Table which holds severity flag for every available LOG severity level. 
															* This flag indicates for each severity - whether it is enabled for Logging or not.	
															* User can Set/Get this Tabel
															*/
        TI_UINT8    aFileEnable[REPORT_FILES_NUM]; 		/**< Table which holds file flag for every available LOG file.
															* This flag indicates for each file - whether it is enabled for Logging or not.				
															* User can Set/Get this Tabel
															*/															
        TI_UINT32   uReportPPMode;							/**< Used by user for Indicating if Debug Module should be enabled/disabled																	*/

    } content;

} TReportParamInfo;

/** \struct TReportInitParams
 * \brief Report Init Parameters
 * 
 * \par Description
 * Struct which defines all the Databases needed for init and set the defualts of the Report Module.
 * 
 */
typedef struct
{
    /* Note: The arrays sizes are aligned to 4 byte to avoid padding added by the compiler! */
	TI_UINT8   aSeverityTable[(REPORT_SEVERITY_MAX + 3) & ~3];	/**< Table in the size of all available LOG severity levels which indicates for each severity - whether it is enabled for Logging or not.	*/
	TI_UINT8   aFileEnable   [(REPORT_FILES_NUM    + 3) & ~3];	/**< Table in the size of all available LOG files which indicates for each file - whether it is enabled for Logging or not				*/				

} TReportInitParams;



/****************************/
/* report module Macros		*/
/****************************/

/* The report mechanism is like that:
    Each file has a report flag. Each severity has a severity flag.
    Only if bits are enabled, the message is printed */
/* The modules which have their report flag enable are indicated by a bit map in the reportModule 
    variable contained in the report handle*/
/* The severities which have are enabled are indicated by a bit map in the reportSeverity
    variable contained in the report handle*/
/* general trace messages */
#ifndef PRINTF_ROLLBACK

#define TRACE0(hReport, level, str) \
	do { if (hReport && (((TReport *)hReport)->aSeverityTable[level]) && (((TReport *)hReport)->aFileEnable[__FILE_ID__])) \
{ os_Trace((((TReport *)hReport)->hOs), level, __FILE_ID__, __LINE__, 0); } } while(0)

#define TRACE1(hReport, level, str, p1) \
	do { if (hReport && (((TReport *)hReport)->aSeverityTable[level]) && (((TReport *)hReport)->aFileEnable[__FILE_ID__])) \
{ os_Trace((((TReport *)hReport)->hOs), level, __FILE_ID__, __LINE__, 1, (TI_UINT32)p1); } } while(0)

#define TRACE2(hReport, level, str, p1, p2) \
	do { if (hReport && (((TReport *)hReport)->aSeverityTable[level]) && (((TReport *)hReport)->aFileEnable[__FILE_ID__])) \
{ os_Trace((((TReport *)hReport)->hOs), level, __FILE_ID__, __LINE__, 2, (TI_UINT32)p1, (TI_UINT32)p2); } } while(0)

#define TRACE3(hReport, level, str, p1, p2, p3) \
	do { if (hReport && (((TReport *)hReport)->aSeverityTable[level]) && (((TReport *)hReport)->aFileEnable[__FILE_ID__])) \
{ os_Trace((((TReport *)hReport)->hOs), level, __FILE_ID__, __LINE__, 3, (TI_UINT32)p1, (TI_UINT32)p2, (TI_UINT32)p3); } } while(0)

#define TRACE4(hReport, level, str, p1, p2, p3, p4) \
	do { if (hReport && (((TReport *)hReport)->aSeverityTable[level]) && (((TReport *)hReport)->aFileEnable[__FILE_ID__])) \
{ os_Trace((((TReport *)hReport)->hOs), level, __FILE_ID__, __LINE__, 4, (TI_UINT32)p1, (TI_UINT32)p2, (TI_UINT32)p3, (TI_UINT32)p4); } } while(0)

#define TRACE5(hReport, level, str, p1, p2, p3, p4, p5) \
	do { if (hReport && (((TReport *)hReport)->aSeverityTable[level]) && (((TReport *)hReport)->aFileEnable[__FILE_ID__])) \
{ os_Trace((((TReport *)hReport)->hOs), level, __FILE_ID__, __LINE__, 5, (TI_UINT32)p1, (TI_UINT32)p2, (TI_UINT32)p3, (TI_UINT32)p4, (TI_UINT32)p5); } } while(0)

#define TRACE6(hReport, level, str, p1, p2, p3, p4, p5, p6) \
	do { if (hReport && (((TReport *)hReport)->aSeverityTable[level]) && (((TReport *)hReport)->aFileEnable[__FILE_ID__])) \
{ os_Trace((((TReport *)hReport)->hOs), level, __FILE_ID__, __LINE__, 6, (TI_UINT32)p1, (TI_UINT32)p2, (TI_UINT32)p3, (TI_UINT32)p4, (TI_UINT32)p5, (TI_UINT32)p6); } } while(0)

#define TRACE7(hReport, level, str, p1, p2, p3, p4, p5, p6, p7) \
	do { if (hReport && (((TReport *)hReport)->aSeverityTable[level]) && (((TReport *)hReport)->aFileEnable[__FILE_ID__])) \
{ os_Trace((((TReport *)hReport)->hOs), level, __FILE_ID__, __LINE__, 7, (TI_UINT32)p1, (TI_UINT32)p2, (TI_UINT32)p3, (TI_UINT32)p4, (TI_UINT32)p5, (TI_UINT32)p6, (TI_UINT32)p7); } } while(0)

#define TRACE8(hReport, level, str, p1, p2, p3, p4, p5, p6, p7, p8) \
	do { if (hReport && (((TReport *)hReport)->aSeverityTable[level]) && (((TReport *)hReport)->aFileEnable[__FILE_ID__])) \
{ os_Trace((((TReport *)hReport)->hOs), level, __FILE_ID__, __LINE__, 8, (TI_UINT32)p1, (TI_UINT32)p2, (TI_UINT32)p3, (TI_UINT32)p4, (TI_UINT32)p5, (TI_UINT32)p6, (TI_UINT32)p7, (TI_UINT32)p8); } } while(0)

#define TRACE9(hReport, level, str, p1, p2, p3, p4, p5, p6, p7, p8, p9) \
	do { if (hReport && (((TReport *)hReport)->aSeverityTable[level]) && (((TReport *)hReport)->aFileEnable[__FILE_ID__])) \
{ os_Trace((((TReport *)hReport)->hOs), level, __FILE_ID__, __LINE__, 9, (TI_UINT32)p1, (TI_UINT32)p2, (TI_UINT32)p3, (TI_UINT32)p4, (TI_UINT32)p5, (TI_UINT32)p6, (TI_UINT32)p7, (TI_UINT32)p8, (TI_UINT32)p9); } } while(0)

#define TRACE10(hReport, level, str, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10) \
	do { if (hReport && (((TReport *)hReport)->aSeverityTable[level]) && (((TReport *)hReport)->aFileEnable[__FILE_ID__])) \
{ os_Trace((((TReport *)hReport)->hOs), level, __FILE_ID__, __LINE__, 10, (TI_UINT32)p1, (TI_UINT32)p2, (TI_UINT32)p3, (TI_UINT32)p4, (TI_UINT32)p5, (TI_UINT32)p6, (TI_UINT32)p7, (TI_UINT32)p8, (TI_UINT32)p9, (TI_UINT32)p10); } } while(0)

#define TRACE11(hReport, level, str, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11) \
	do { if (hReport && (((TReport *)hReport)->aSeverityTable[level]) && (((TReport *)hReport)->aFileEnable[__FILE_ID__])) \
{ os_Trace((((TReport *)hReport)->hOs), level, __FILE_ID__, __LINE__, 11, (TI_UINT32)p1, (TI_UINT32)p2, (TI_UINT32)p3, (TI_UINT32)p4, (TI_UINT32)p5, (TI_UINT32)p6, (TI_UINT32)p7, (TI_UINT32)p8, (TI_UINT32)p9, (TI_UINT32)p10, (TI_UINT32)p11); } } while(0)

#define TRACE12(hReport, level, str, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12) \
	do { if (hReport && (((TReport *)hReport)->aSeverityTable[level]) && (((TReport *)hReport)->aFileEnable[__FILE_ID__])) \
{ os_Trace((((TReport *)hReport)->hOs), level, __FILE_ID__, __LINE__, 12, (TI_UINT32)p1, (TI_UINT32)p2, (TI_UINT32)p3, (TI_UINT32)p4, (TI_UINT32)p5, (TI_UINT32)p6, (TI_UINT32)p7, (TI_UINT32)p8, (TI_UINT32)p9, (TI_UINT32)p10, (TI_UINT32)p11, (TI_UINT32)p12); } } while(0)

#define TRACE13(hReport, level, str, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13) \
	do { if (hReport && (((TReport *)hReport)->aSeverityTable[level]) && (((TReport *)hReport)->aFileEnable[__FILE_ID__])) \
{ os_Trace((((TReport *)hReport)->hOs), level, __FILE_ID__, __LINE__, 13, (TI_UINT32)p1, (TI_UINT32)p2, (TI_UINT32)p3, (TI_UINT32)p4, (TI_UINT32)p5, (TI_UINT32)p6, (TI_UINT32)p7, (TI_UINT32)p8, (TI_UINT32)p9, (TI_UINT32)p10, (TI_UINT32)p11, (TI_UINT32)p12, (TI_UINT32)p13); } } while(0)

#define TRACE14(hReport, level, str, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14) \
	do { if (hReport && (((TReport *)hReport)->aSeverityTable[level]) && (((TReport *)hReport)->aFileEnable[__FILE_ID__])) \
{ os_Trace((((TReport *)hReport)->hOs), level, __FILE_ID__, __LINE__, 14, (TI_UINT32)p1, (TI_UINT32)p2, (TI_UINT32)p3, (TI_UINT32)p4, (TI_UINT32)p5, (TI_UINT32)p6, (TI_UINT32)p7, (TI_UINT32)p8, (TI_UINT32)p9, (TI_UINT32)p10, (TI_UINT32)p11, (TI_UINT32)p12, (TI_UINT32)p13, (TI_UINT32)p14); } } while(0)

#define TRACE15(hReport, level, str, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15) \
	do { if (hReport && (((TReport *)hReport)->aSeverityTable[level]) && (((TReport *)hReport)->aFileEnable[__FILE_ID__])) \
{ os_Trace((((TReport *)hReport)->hOs), level, __FILE_ID__, __LINE__, 15, (TI_UINT32)p1, (TI_UINT32)p2, (TI_UINT32)p3, (TI_UINT32)p4, (TI_UINT32)p5, (TI_UINT32)p6, (TI_UINT32)p7, (TI_UINT32)p8, (TI_UINT32)p9, (TI_UINT32)p10, (TI_UINT32)p11, (TI_UINT32)p12, (TI_UINT32)p13, (TI_UINT32)p14, (TI_UINT32)p15); } } while(0)

#define TRACE16(hReport, level, str, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16) \
	do { if (hReport && (((TReport *)hReport)->aSeverityTable[level]) && (((TReport *)hReport)->aFileEnable[__FILE_ID__])) \
{ os_Trace((((TReport *)hReport)->hOs), level, __FILE_ID__, __LINE__, 16, (TI_UINT32)p1, (TI_UINT32)p2, (TI_UINT32)p3, (TI_UINT32)p4, (TI_UINT32)p5, (TI_UINT32)p6, (TI_UINT32)p7, (TI_UINT32)p8, (TI_UINT32)p9, (TI_UINT32)p10, (TI_UINT32)p11, (TI_UINT32)p12, (TI_UINT32)p13, (TI_UINT32)p14, (TI_UINT32)p15, (TI_UINT32)p16); } } while(0)

#define TRACE17(hReport, level, str, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17) \
	do { if (hReport && (((TReport *)hReport)->aSeverityTable[level]) && (((TReport *)hReport)->aFileEnable[__FILE_ID__])) \
{ os_Trace((((TReport *)hReport)->hOs), level, __FILE_ID__, __LINE__, 17, (TI_UINT32)p1, (TI_UINT32)p2, (TI_UINT32)p3, (TI_UINT32)p4, (TI_UINT32)p5, (TI_UINT32)p6, (TI_UINT32)p7, (TI_UINT32)p8, (TI_UINT32)p9, (TI_UINT32)p10, (TI_UINT32)p11, (TI_UINT32)p12, (TI_UINT32)p13, (TI_UINT32)p14, (TI_UINT32)p15, (TI_UINT32)p16, (TI_UINT32)p17); } } while(0)

#define TRACE18(hReport, level, str, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18) \
	do { if (hReport && (((TReport *)hReport)->aSeverityTable[level]) && (((TReport *)hReport)->aFileEnable[__FILE_ID__])) \
{ os_Trace((((TReport *)hReport)->hOs), level, __FILE_ID__, __LINE__, 18, (TI_UINT32)p1, (TI_UINT32)p2, (TI_UINT32)p3, (TI_UINT32)p4, (TI_UINT32)p5, (TI_UINT32)p6, (TI_UINT32)p7, (TI_UINT32)p8, (TI_UINT32)p9, (TI_UINT32)p10, (TI_UINT32)p11, (TI_UINT32)p12, (TI_UINT32)p13, (TI_UINT32)p14, (TI_UINT32)p15, (TI_UINT32)p16, (TI_UINT32)p17, (TI_UINT32)p18); } } while(0)

#define TRACE19(hReport, level, str, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19) \
	do { if (hReport && (((TReport *)hReport)->aSeverityTable[level]) && (((TReport *)hReport)->aFileEnable[__FILE_ID__])) \
{ os_Trace((((TReport *)hReport)->hOs), level, __FILE_ID__, __LINE__, 19, (TI_UINT32)p1, (TI_UINT32)p2, (TI_UINT32)p3, (TI_UINT32)p4, (TI_UINT32)p5, (TI_UINT32)p6, (TI_UINT32)p7, (TI_UINT32)p8, (TI_UINT32)p9, (TI_UINT32)p10, (TI_UINT32)p11, (TI_UINT32)p12, (TI_UINT32)p13, (TI_UINT32)p14, (TI_UINT32)p15, (TI_UINT32)p16, (TI_UINT32)p17, (TI_UINT32)p18, (TI_UINT32)p19); } } while(0)

#define TRACE20(hReport, level, str, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20) \
	do { if (hReport && (((TReport *)hReport)->aSeverityTable[level]) && (((TReport *)hReport)->aFileEnable[__FILE_ID__])) \
{ os_Trace((((TReport *)hReport)->hOs), level, __FILE_ID__, __LINE__, 20, (TI_UINT32)p1, (TI_UINT32)p2, (TI_UINT32)p3, (TI_UINT32)p4, (TI_UINT32)p5, (TI_UINT32)p6, (TI_UINT32)p7, (TI_UINT32)p8, (TI_UINT32)p9, (TI_UINT32)p10, (TI_UINT32)p11, (TI_UINT32)p12, (TI_UINT32)p13, (TI_UINT32)p14, (TI_UINT32)p15, (TI_UINT32)p16, (TI_UINT32)p17, (TI_UINT32)p18, (TI_UINT32)p19, (TI_UINT32)p20); } } while(0)

#define TRACE21(hReport, level, str, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20, p21) \
	do { if (hReport && (((TReport *)hReport)->aSeverityTable[level]) && (((TReport *)hReport)->aFileEnable[__FILE_ID__])) \
{ os_Trace((((TReport *)hReport)->hOs), level, __FILE_ID__, __LINE__, 21, (TI_UINT32)p1, (TI_UINT32)p2, (TI_UINT32)p3, (TI_UINT32)p4, (TI_UINT32)p5, (TI_UINT32)p6, (TI_UINT32)p7, (TI_UINT32)p8, (TI_UINT32)p9, (TI_UINT32)p10, (TI_UINT32)p11, (TI_UINT32)p12, (TI_UINT32)p13, (TI_UINT32)p14, (TI_UINT32)p15, (TI_UINT32)p16, (TI_UINT32)p17, (TI_UINT32)p18, (TI_UINT32)p19, (TI_UINT32)p20, (TI_UINT32)p21); } } while(0)

#define TRACE22(hReport, level, str, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20, p21, p22) \
	do { if (hReport && (((TReport *)hReport)->aSeverityTable[level]) && (((TReport *)hReport)->aFileEnable[__FILE_ID__])) \
{ os_Trace((((TReport *)hReport)->hOs), level, __FILE_ID__, __LINE__, 22, (TI_UINT32)p1, (TI_UINT32)p2, (TI_UINT32)p3, (TI_UINT32)p4, (TI_UINT32)p5, (TI_UINT32)p6, (TI_UINT32)p7, (TI_UINT32)p8, (TI_UINT32)p9, (TI_UINT32)p10, (TI_UINT32)p11, (TI_UINT32)p12, (TI_UINT32)p13, (TI_UINT32)p14, (TI_UINT32)p15, (TI_UINT32)p16, (TI_UINT32)p17, (TI_UINT32)p18, (TI_UINT32)p19, (TI_UINT32)p20, (TI_UINT32)p21, (TI_UINT32)p22); } } while(0)

#define TRACE25(hReport, level, str, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20, p21, p22, p23, p24, p25) \
	do { if (hReport && (((TReport *)hReport)->aSeverityTable[level]) && (((TReport *)hReport)->aFileEnable[__FILE_ID__])) \
{ os_Trace((((TReport *)hReport)->hOs), level, __FILE_ID__, __LINE__, 22, (TI_UINT32)p1, (TI_UINT32)p2, (TI_UINT32)p3, (TI_UINT32)p4, (TI_UINT32)p5, (TI_UINT32)p6, (TI_UINT32)p7, (TI_UINT32)p8, (TI_UINT32)p9, (TI_UINT32)p10, (TI_UINT32)p11, (TI_UINT32)p12, (TI_UINT32)p13, (TI_UINT32)p14, (TI_UINT32)p15, (TI_UINT32)p16, (TI_UINT32)p17, (TI_UINT32)p18, (TI_UINT32)p19, (TI_UINT32)p20, (TI_UINT32)p21, (TI_UINT32)p22, (TI_UINT32)p23, (TI_UINT32)p24, (TI_UINT32)p25); } } while(0)

#define TRACE31(hReport, level, str, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20, p21, p22, p23, p24, p25, p26, p27, p28, p29, p30, p31) \
	do { if (hReport && (((TReport *)hReport)->aSeverityTable[level]) && (((TReport *)hReport)->aFileEnable[__FILE_ID__])) \
{ os_Trace((((TReport *)hReport)->hOs), level, __FILE_ID__, __LINE__, 22, (TI_UINT32)p1, (TI_UINT32)p2, (TI_UINT32)p3, (TI_UINT32)p4, (TI_UINT32)p5, (TI_UINT32)p6, (TI_UINT32)p7, (TI_UINT32)p8, (TI_UINT32)p9, (TI_UINT32)p10, (TI_UINT32)p11, (TI_UINT32)p12, (TI_UINT32)p13, (TI_UINT32)p14, (TI_UINT32)p15, (TI_UINT32)p16, (TI_UINT32)p17, (TI_UINT32)p18, (TI_UINT32)p19, (TI_UINT32)p20, (TI_UINT32)p21, (TI_UINT32)p22, (TI_UINT32)p23, (TI_UINT32)p24, (TI_UINT32)p25, (TI_UINT32)p26, (TI_UINT32)p27, (TI_UINT32)p28, (TI_UINT32)p29, (TI_UINT32)p30, (TI_UINT32)p31); } } while(0)


#else /* PRINTF_ROLLBACK */

#define TRACE0(hReport, level, str) \
	do { if (hReport && (((TReport *)hReport)->aSeverityTable[level]) && (((TReport *)hReport)->aFileEnable[__FILE_ID__])) \
{ os_printf ("%s, %s:", ((TReport *)hReport)->aFileName[__FILE_ID__], ((TReport *)hReport)->aSeverityDesc[level]); os_printf (str); } } while(0)

#define TRACE1(hReport, level, str, p1) \
	do { if (hReport && (((TReport *)hReport)->aSeverityTable[level]) && (((TReport *)hReport)->aFileEnable[__FILE_ID__])) \
{ os_printf ("%s, %s:", ((TReport *)hReport)->aFileName[__FILE_ID__], ((TReport *)hReport)->aSeverityDesc[level]); os_printf (str, p1); } } while(0)

#define TRACE2(hReport, level, str, p1, p2) \
	do { if (hReport && (((TReport *)hReport)->aSeverityTable[level]) && (((TReport *)hReport)->aFileEnable[__FILE_ID__])) \
{ os_printf ("%s, %s:", ((TReport *)hReport)->aFileName[__FILE_ID__], ((TReport *)hReport)->aSeverityDesc[level]); os_printf (str, p1, p2); } } while(0)

#define TRACE3(hReport, level, str, p1, p2, p3) \
	do { if (hReport && (((TReport *)hReport)->aSeverityTable[level]) && (((TReport *)hReport)->aFileEnable[__FILE_ID__])) \
{ os_printf ("%s, %s:", ((TReport *)hReport)->aFileName[__FILE_ID__], ((TReport *)hReport)->aSeverityDesc[level]); os_printf (str, p1, p2, p3); } } while(0)

#define TRACE4(hReport, level, str, p1, p2, p3, p4) \
	do { if (hReport && (((TReport *)hReport)->aSeverityTable[level]) && (((TReport *)hReport)->aFileEnable[__FILE_ID__])) \
{ os_printf ("%s, %s:", ((TReport *)hReport)->aFileName[__FILE_ID__], ((TReport *)hReport)->aSeverityDesc[level]); os_printf (str, p1, p2, p3, p4); } } while(0)

#define TRACE5(hReport, level, str, p1, p2, p3, p4, p5) \
	do { if (hReport && (((TReport *)hReport)->aSeverityTable[level]) && (((TReport *)hReport)->aFileEnable[__FILE_ID__])) \
{ os_printf ("%s, %s:", ((TReport *)hReport)->aFileName[__FILE_ID__], ((TReport *)hReport)->aSeverityDesc[level]); os_printf (str, p1, p2, p3, p4, p5); } } while(0)

#define TRACE6(hReport, level, str, p1, p2, p3, p4, p5, p6) \
	do { if (hReport && (((TReport *)hReport)->aSeverityTable[level]) && (((TReport *)hReport)->aFileEnable[__FILE_ID__])) \
{ os_printf ("%s, %s:", ((TReport *)hReport)->aFileName[__FILE_ID__], ((TReport *)hReport)->aSeverityDesc[level]); os_printf (str, p1, p2, p3, p4, p5, p6); } } while(0)

#define TRACE7(hReport, level, str, p1, p2, p3, p4, p5, p6, p7) \
	do { if (hReport && (((TReport *)hReport)->aSeverityTable[level]) && (((TReport *)hReport)->aFileEnable[__FILE_ID__])) \
{ os_printf ("%s, %s:", ((TReport *)hReport)->aFileName[__FILE_ID__], ((TReport *)hReport)->aSeverityDesc[level]); os_printf (str, p1, p2, p3, p4, p5, p6, p7); } } while(0)

#define TRACE8(hReport, level, str, p1, p2, p3, p4, p5, p6, p7, p8) \
	do { if (hReport && (((TReport *)hReport)->aSeverityTable[level]) && (((TReport *)hReport)->aFileEnable[__FILE_ID__])) \
{ os_printf ("%s, %s:", ((TReport *)hReport)->aFileName[__FILE_ID__], ((TReport *)hReport)->aSeverityDesc[level]); os_printf (str, p1, p2, p3, p4, p5, p6, p7, p8); } } while(0)

#define TRACE9(hReport, level, str, p1, p2, p3, p4, p5, p6, p7, p8, p9) \
	do { if (hReport && (((TReport *)hReport)->aSeverityTable[level]) && (((TReport *)hReport)->aFileEnable[__FILE_ID__])) \
{ os_printf ("%s, %s:", ((TReport *)hReport)->aFileName[__FILE_ID__], ((TReport *)hReport)->aSeverityDesc[level]); os_printf (str, p1, p2, p3, p4, p5, p6, p7, p8, p9); } } while(0)

#define TRACE10(hReport, level, str, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10) \
	do { if (hReport && (((TReport *)hReport)->aSeverityTable[level]) && (((TReport *)hReport)->aFileEnable[__FILE_ID__])) \
{ os_printf ("%s, %s:", ((TReport *)hReport)->aFileName[__FILE_ID__], ((TReport *)hReport)->aSeverityDesc[level]); os_printf (str, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10); } } while(0)

#define TRACE11(hReport, level, str, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11) \
	do { if (hReport && (((TReport *)hReport)->aSeverityTable[level]) && (((TReport *)hReport)->aFileEnable[__FILE_ID__])) \
{ os_printf ("%s, %s:", ((TReport *)hReport)->aFileName[__FILE_ID__], ((TReport *)hReport)->aSeverityDesc[level]); os_printf (str, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11); } } while(0)

#define TRACE12(hReport, level, str, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12) \
	do { if (hReport && (((TReport *)hReport)->aSeverityTable[level]) && (((TReport *)hReport)->aFileEnable[__FILE_ID__])) \
{ os_printf ("%s, %s:", ((TReport *)hReport)->aFileName[__FILE_ID__], ((TReport *)hReport)->aSeverityDesc[level]); os_printf (str, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12); } } while(0)

#define TRACE13(hReport, level, str, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13) \
	do { if (hReport && (((TReport *)hReport)->aSeverityTable[level]) && (((TReport *)hReport)->aFileEnable[__FILE_ID__])) \
{ os_printf ("%s, %s:", ((TReport *)hReport)->aFileName[__FILE_ID__], ((TReport *)hReport)->aSeverityDesc[level]); os_printf (str, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13); } } while(0)

#define TRACE14(hReport, level, str, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14) \
	do { if (hReport && (((TReport *)hReport)->aSeverityTable[level]) && (((TReport *)hReport)->aFileEnable[__FILE_ID__])) \
{ os_printf ("%s, %s:", ((TReport *)hReport)->aFileName[__FILE_ID__], ((TReport *)hReport)->aSeverityDesc[level]); os_printf (str, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14); } } while(0)

#define TRACE15(hReport, level, str, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15) \
	do { if (hReport && (((TReport *)hReport)->aSeverityTable[level]) && (((TReport *)hReport)->aFileEnable[__FILE_ID__])) \
{ os_printf ("%s, %s:", ((TReport *)hReport)->aFileName[__FILE_ID__], ((TReport *)hReport)->aSeverityDesc[level]); os_printf (str, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15); } } while(0)

#define TRACE16(hReport, level, str, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16) \
	do { if (hReport && (((TReport *)hReport)->aSeverityTable[level]) && (((TReport *)hReport)->aFileEnable[__FILE_ID__])) \
{ os_printf ("%s, %s:", ((TReport *)hReport)->aFileName[__FILE_ID__], ((TReport *)hReport)->aSeverityDesc[level]); os_printf (str, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16); } } while(0)

#define TRACE17(hReport, level, str, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17) \
	do { if (hReport && (((TReport *)hReport)->aSeverityTable[level]) && (((TReport *)hReport)->aFileEnable[__FILE_ID__])) \
{ os_printf ("%s, %s:", ((TReport *)hReport)->aFileName[__FILE_ID__], ((TReport *)hReport)->aSeverityDesc[level]); os_printf (str, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17); } } while(0)

#define TRACE18(hReport, level, str, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18) \
	do { if (hReport && (((TReport *)hReport)->aSeverityTable[level]) && (((TReport *)hReport)->aFileEnable[__FILE_ID__])) \
{ os_printf ("%s, %s:", ((TReport *)hReport)->aFileName[__FILE_ID__], ((TReport *)hReport)->aSeverityDesc[level]); os_printf (str, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18); } } while(0)

#define TRACE19(hReport, level, str, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19) \
	do { if (hReport && (((TReport *)hReport)->aSeverityTable[level]) && (((TReport *)hReport)->aFileEnable[__FILE_ID__])) \
{ os_printf ("%s, %s:", ((TReport *)hReport)->aFileName[__FILE_ID__], ((TReport *)hReport)->aSeverityDesc[level]); os_printf (str, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19); } } while(0)

#define TRACE20(hReport, level, str, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20) \
	do { if (hReport && (((TReport *)hReport)->aSeverityTable[level]) && (((TReport *)hReport)->aFileEnable[__FILE_ID__])) \
{ os_printf ("%s, %s:", ((TReport *)hReport)->aFileName[__FILE_ID__], ((TReport *)hReport)->aSeverityDesc[level]); os_printf (str, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20); } } while(0)

#define TRACE21(hReport, level, str, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20, p21) \
	do { if (hReport && (((TReport *)hReport)->aSeverityTable[level]) && (((TReport *)hReport)->aFileEnable[__FILE_ID__])) \
{ os_printf ("%s, %s:", ((TReport *)hReport)->aFileName[__FILE_ID__], ((TReport *)hReport)->aSeverityDesc[level]); os_printf (str, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20, p21); } } while(0)

#define TRACE22(hReport, level, str, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20, p21, p22) \
	do { if (hReport && (((TReport *)hReport)->aSeverityTable[level]) && (((TReport *)hReport)->aFileEnable[__FILE_ID__])) \
{ os_printf ("%s, %s:", ((TReport *)hReport)->aFileName[__FILE_ID__], ((TReport *)hReport)->aSeverityDesc[level]); os_printf (str, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20, p21, p22); } } while(0)

#define TRACE25(hReport, level, str, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20, p21, p22, p23, p24, p25) \
	do { if (hReport && (((TReport *)hReport)->aSeverityTable[level]) && (((TReport *)hReport)->aFileEnable[__FILE_ID__])) \
{ os_printf ("%s, %s:", ((TReport *)hReport)->aFileName[__FILE_ID__], ((TReport *)hReport)->aSeverityDesc[level]); os_printf (str, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20, p21, p22, p23, p24, p25); } } while(0)

#define TRACE31(hReport, level, str, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20, p21, p22, p23, p24, p25, p26, p27, p28, p29, p30, p31) \
	do { if (hReport && (((TReport *)hReport)->aSeverityTable[level]) && (((TReport *)hReport)->aFileEnable[__FILE_ID__])) \
{ os_printf ("%s, %s:", ((TReport *)hReport)->aFileName[__FILE_ID__], ((TReport *)hReport)->aSeverityDesc[level]); os_printf (str, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20, p21, p22, p23, p24, p25, p27, p27, p28, p29, p30, p31); } } while(0)

#endif /* #ifdef PRINTF_ROLLBACK */


/****************************/
/* report module Macros		*/
/****************************/

/* The report mechanism is like that:
Each file has a report flag. Each severity has a severity flag.
Only if bits are enabled, the message is printed */
/* The modules which have their report flag enable are indicated by a bit map in the reportModule 
variable contained in the report handle*/
/* The severities which have are enabled are indicated by a bit map in the reportSeverity
variable contained in the report handle*/

#ifdef REPORT_LOG

/** \def WLAN_OS_REPORT
* \brief Macro which writes a message to user via specific Operating System printf.
* E.g. print is done using the relevant used OS printf method.
*/
#define WLAN_OS_REPORT(msg)                                           \
	do { os_printf msg;} while(0)


#ifdef INIT_MESSAGES
/** \def WLAN_INIT_REPORT
* \brief Macro which writes a message to user via specific Operating System printf.
* E.g. print is done using the relevant used OS printf method.
*/
#define WLAN_INIT_REPORT(msg)                                         \
	do { os_printf msg;} while(0)
#else
/** \def WLAN_INIT_REPORT
* \brief Macro which writes a message to user via specific Operating System printf.
* E.g. print is done using the relevant used OS printf method.
*/
#define WLAN_INIT_REPORT(msg) 
#endif
#define TRACE_INFO_HEX(hReport, data, datalen) \
	do { if (hReport && (((TReport *)hReport)->aSeverityTable[REPORT_SEVERITY_INFORMATION]) && (((TReport *)hReport)->aFileEnable[__FILE_ID__])) \
{ report_PrintDump (data, datalen); } } while(0)


#else   /* REPORT_LOG */

/* NOTE: Keep a dummy report function call to treat compilation warnings */

/** \def WLAN_OS_REPORT
* \brief Dummy macro: Nothing is done 
*/
#define WLAN_OS_REPORT(msg)                                           \
	do { } while (0)

/** \def WLAN_INIT_REPORT
* \brief Dummy macro: Nothing is done 
*/
#define WLAN_INIT_REPORT(msg)                                         \
	do { } while (0)


#define TRACE_INFO_HEX(hReport, data, datalen) \
	do { } while (0)

#endif  /* REPORT_LOG */


/****************************/
/* report module prototypes */
/****************************/

/** \brief  Create Report Module Object
 * \param  hOs   			- OS module object handle
 * \return Handle to the report module on success, NULL otherwise
 * 
 * \par Description
 * Report module creation function, called by the config mgr in creation phase	\n
 * performs the following:	\n
 *	-   Allocate the report handle
 */ 
TI_HANDLE report_Create (TI_HANDLE hOs);
/** \brief  Set Report Module Defaults
 * \param  hReport   	- Report module object handle
 * \param  pInitParams	- Pointer to Input Init Parameters
 * \return TI_OK on success or TI_NOK on failure
 * 
 * \par Description
 * Report module configuration function, called by the config mgr in configuration phase	\n
 * performs the following:	\n
 *	-   Reset & init local variables
 *	-   Resets all report modules bits
 *	-   Resets all severities bits
 *	-   Init the description strings * 
 */ 
TI_STATUS report_SetDefaults            (TI_HANDLE hReport, TReportInitParams *pInitParams);
/** \brief  Unload Report Module
 * \param  hReport   	- Report module object handle
 * \return TI_OK on success or TI_NOK on failure
 * 
 * \par Description
 * Report Module unload function, called by the config mgr in the unload phase	\n
 * performs the following:	\n
 *	-   Free all memory allocated by the module		
 */ 
TI_STATUS report_Unload                 (TI_HANDLE hReport);
/** \brief  Set Report Module
 * \param  hReport   	- Report module object handle
 * \return TI_OK on success or TI_NOK on failure
 * 
 * \par Description
 * Enables the relevant module for reporting -  according to the received module index 
 */ 
TI_STATUS report_SetReportModule        (TI_HANDLE hReport, TI_UINT8 module_index);
/** \brief  Clear Report Module
 * \param  hReport   	- Report module object handle
 * \param  module_index	- Index of file to be disable for reporting 
 * \return TI_OK on success or TI_NOK on failure
 * 
 * \par Description
 * Disables the relevant module for reporting -  according to the received module index 
 */ 
TI_STATUS report_ClearReportModule      (TI_HANDLE hReport, TI_UINT8 module_index);
/** \brief  Get Report files Table
 * \param  hReport   	- Report module object handle
 * \param  pFiles		- Pointer to output files Table
 * \return TI_OK on success or TI_NOK on failure
 * 
 * \par Description
 * Returns the Modules Table (the table which holds Modules reporting indication) held in Report Module Object
 */ 
TI_STATUS report_GetReportFilesTable    (TI_HANDLE hReport, TI_UINT8 *pFiles); 
/** \brief  Set Report Files Table
 * \param  hReport   	- Report module object handle
 * \param  pFiles		- Pointer to input files Table
 * \return TI_OK on success or TI_NOK on failure
 * 
 * \par Description
 * Updates the Modules Table: copies the input Modules Table (the table which holds Modules reporting indication) 
 * to the Modules Table which is held in Report Module Object
 */ 
TI_STATUS report_SetReportFilesTable    (TI_HANDLE hReport, TI_UINT8 *pFiles); 
/** \brief  Get Report Severity Table
 * \param  hReport   	- Report module object handle
 * \param  pSeverities	- Pointer to input Severity Table
 * \return TI_OK on success or TI_NOK on failure
 * 
 * \par Description
 * Returns the Severities Table (the table which holds Severities reporting indication) held in Report Module Object
 */ 
TI_STATUS report_GetReportSeverityTable (TI_HANDLE hReport, TI_UINT8 *pSeverities);
/** \brief  Set Report Severity Table
 * \param  hReport   	- Report module object handle
 * \param  pSeverities	- Pointer to input Severity Table
 * \return TI_OK on success or TI_NOK on failure
 * 
 * \par Description
 * Updates the Severities Table: copies the input Severities Table (the table which holds Severities reporting indication) 
 * to the Severities Table which is held in Report Module Object
 */ 
TI_STATUS report_SetReportSeverityTable (TI_HANDLE hReport, TI_UINT8 *pSeverities);
/** \brief  Set Report Parameters
 * \param  hReport   	- Report module object handle
 * \param  pParam		- Pointer to input Report Parameters Information
 * \return TI_OK on success or TI_NOK on failure
 * 
 * \par Description
 * Report set param function sets parameter/s to Report Module Object. 
 * Called by the following:
 *	-   configuration Manager in order to set a parameter/s from the OS abstraction layer
 *	-   Form inside the driver 
 */ 
TI_STATUS report_SetParam               (TI_HANDLE hReport, TReportParamInfo *pParam);                   
/** \brief  Get Report Parameters
 * \param  hReport   	- Report module object handle
 * \param  pParam		- Pointer to output Report Parameters Information
 * \return TI_OK on success or TI_NOK on failure
 * 
 * \par Description
 * Report get param function gets parameter/s from Report Module Object. 
 * Called by the following:
 *	-   configuration Manager in order to get a parameter/s from the OS abstraction layer
 *	-   from inside the driver 
 */ 
TI_STATUS report_GetParam               (TI_HANDLE hReport, TReportParamInfo *pParam); 
/** \brief Report Dump
 * \param  pBuffer  - Pointer to input HEX buffer
 * \param  pString	- Pointer to output string
 * \param  size		- size of output string
 * \return TI_OK on success or TI_NOK on failure
 * 
 * \par Description
 * Converts hex buffer to string buffer
 * NOTE:	1. The caller has to make sure that the string size is at lease: ((Size * 2) + 1)
 * 	      	2. A string terminator is inserted into lase char of the string 
 */ 
TI_STATUS report_Dump 					(TI_UINT8 *pBuffer, char *pString, TI_UINT32 size);
/** \brief Report Print Dump
 * \param  pData  	- Pointer to input data buffer
 * \param  datalen	- size of input data buffer
 * \return TI_OK on success or TI_NOK on failure
 * 
 * \par Description
 * Performs HEX dump of input Data buffer. Used for debug only! 
 */ 
TI_STATUS report_PrintDump 				(TI_UINT8 *pData, TI_UINT32 datalen);

/** \brief Sets bRedirectOutputToLogger
* \param  value  	    - True if to direct output to remote PC
* \param  hEvHandler	- Event handler
*/ 
void os_setDebugOutputToLogger(TI_BOOL value);

/** \brief Sets handles a SW running problem
* \param  problemType  	- used for different problem handling depending on problem type
*/
void handleRunProblem(EProblemType prType);

#endif /* __REPORT_H__ */


