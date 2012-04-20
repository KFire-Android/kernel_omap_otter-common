/*
 * CmdDispatcher.c
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


/** \file   CmdDispatcher.c 
 *  \brief  The CmdDispatcher module. Handles user commbands dispatching to the driver modules.
 *  
 *  \see    CmdDispatcher.h
 */

#define __FILE_ID__  FILE_ID_47
#include "tidef.h"
#include "osApi.h"
#include "report.h"
#include "DrvMain.h"
#include "connApi.h"
#include "siteMgrApi.h"
#include "smeApi.h"
#include "SoftGeminiApi.h"
#include "roamingMngrApi.h"
#include "qosMngr_API.h"
#include "PowerMgr_API.h"
#include "ScanCncn.h"
#include "scanMngrApi.h"
#include "regulatoryDomainApi.h"
#include "measurementMgrApi.h"
#include "TWDriver.h"
#include "debug.h"
#include "DrvMainModules.h"
#include "CmdDispatcher.h"
#include "healthMonitor.h"
#include "currBssApi.h"
#ifdef XCC_MODULE_INCLUDED
#include "XCCMngr.h"
#endif


/* Set/get params function prototype */
typedef TI_STATUS (*TParamFunc) (TI_HANDLE handle, paramInfo_t *pParam);

typedef struct
{
    TParamFunc          set; 
    TParamFunc          get; 
    TI_HANDLE           handle; 

} TParamAccess;

/* The module's object */
typedef struct
{
    /* Other modules handles */
    TI_HANDLE    hOs;
    TI_HANDLE    hReport;
    TI_HANDLE    hAuth;
    TI_HANDLE    hAssoc;
    TI_HANDLE    hRxData;
    TI_HANDLE    hTxCtrl;
    TI_HANDLE    hCtrlData;
    TI_HANDLE    hSiteMgr;
    TI_HANDLE    hConn;
    TI_HANDLE    hRsn;
    TI_HANDLE    hSme;
    TI_HANDLE    hScanCncn;
    TI_HANDLE    hScanMngr;
    TI_HANDLE    hMlmeSm;
    TI_HANDLE    hRegulatoryDomain;
    TI_HANDLE    hMeasurementMgr;
    TI_HANDLE    hRoamingMngr;
    TI_HANDLE    hSoftGemini;
    TI_HANDLE    hQosMngr;
    TI_HANDLE    hPowerMgr;
    TI_HANDLE    hHealthMonitor;
    TI_HANDLE    hTWD;
    TI_HANDLE    hCurrBss;
#ifdef XCC_MODULE_INCLUDED
    TI_HANDLE    hXCCMngr;
#endif

    /* Table of params set/get functions */
    TParamAccess paramAccessTable[MAX_PARAM_MODULE_NUMBER]; 

#ifdef TI_DBG
    TStadHandlesList *pStadHandles;  /* Save modules list pointer just for the debug functions */
#endif

} TCmdDispatchObj;


/* Internal functions prototypes */
static void      cmdDispatch_ConfigParamsAccessTable (TCmdDispatchObj *pCmdDispatch);
static TI_STATUS cmdDispatch_SetTwdParam  (TI_HANDLE hCmdDispatch, paramInfo_t *pParam);
static TI_STATUS cmdDispatch_GetTwdParam  (TI_HANDLE hCmdDispatch, paramInfo_t *pParam);

#ifdef TI_DBG
static TI_STATUS cmdDispatch_DebugFuncSet (TI_HANDLE hCmdDispatch, paramInfo_t *pParam);
static TI_STATUS cmdDispatch_DebugFuncGet (TI_HANDLE hCmdDispatch, paramInfo_t *pParam);	/*yael - this function is not in use*/
#endif



/** 
 * \fn     cmdDispatch_Create
 * \brief  Create the module
 * 
 * Create the Command-Dispatcher module
 * 
 * \note   
 * \param  hOs - Handle to the Os Abstraction Layer                           
 * \return Handle to the allocated module (NULL if failed) 
 * \sa     
 */ 
TI_HANDLE cmdDispatch_Create (TI_HANDLE hOs)
{
    TCmdDispatchObj *pCmdDispatch;

    /* allocate CmdDispatcher module */
    pCmdDispatch = os_memoryAlloc (hOs, (sizeof(TCmdDispatchObj)));
    
    if (!pCmdDispatch)
    {
        WLAN_OS_REPORT(("Error allocating the CmdDispatcher Module\n"));
        return NULL;
    }

    /* Reset CmdDispatcher module */
    os_memoryZero (hOs, pCmdDispatch, (sizeof(TCmdDispatchObj)));

    pCmdDispatch->hOs = hOs;

    return (TI_HANDLE)pCmdDispatch;
}


/** 
 * \fn     cmdDispatch_Init
 * \brief  Save modules handles and fill the configuration table
 * 
 * Save other modules handles, and fill the configuration table 
 *     with the Get/Set functions.
 * 
 * \note   
 * \param  pStadHandles  - The driver modules handles
 * \return void  
 * \sa     
 */ 
void cmdDispatch_Init (TStadHandlesList *pStadHandles)
{
    TCmdDispatchObj *pCmdDispatch = (TCmdDispatchObj *)(pStadHandles->hCmdDispatch);
    
    /* Save modules handles */
    pCmdDispatch->hReport           = pStadHandles->hReport;
    pCmdDispatch->hAuth             = pStadHandles->hAuth;
    pCmdDispatch->hAssoc            = pStadHandles->hAssoc;
    pCmdDispatch->hRxData           = pStadHandles->hRxData;
    pCmdDispatch->hTxCtrl           = pStadHandles->hTxCtrl;
    pCmdDispatch->hCtrlData         = pStadHandles->hCtrlData;
    pCmdDispatch->hSiteMgr          = pStadHandles->hSiteMgr;
    pCmdDispatch->hConn             = pStadHandles->hConn;
    pCmdDispatch->hRsn              = pStadHandles->hRsn;
    pCmdDispatch->hSme              = pStadHandles->hSme;
    pCmdDispatch->hScanCncn         = pStadHandles->hScanCncn;
    pCmdDispatch->hScanMngr         = pStadHandles->hScanMngr;
    pCmdDispatch->hMlmeSm           = pStadHandles->hMlmeSm; 
    pCmdDispatch->hRegulatoryDomain = pStadHandles->hRegulatoryDomain;
    pCmdDispatch->hMeasurementMgr   = pStadHandles->hMeasurementMgr;
    pCmdDispatch->hRoamingMngr      = pStadHandles->hRoamingMngr;
    pCmdDispatch->hSoftGemini       = pStadHandles->hSoftGemini;
    pCmdDispatch->hQosMngr          = pStadHandles->hQosMngr;
    pCmdDispatch->hPowerMgr         = pStadHandles->hPowerMgr;
    pCmdDispatch->hHealthMonitor    = pStadHandles->hHealthMonitor;
    pCmdDispatch->hTWD              = pStadHandles->hTWD;
    pCmdDispatch->hCurrBss          = pStadHandles->hCurrBss;
#ifdef XCC_MODULE_INCLUDED
    pCmdDispatch->hXCCMngr          = pStadHandles->hXCCMngr;
#endif

#ifdef TI_DBG
    pCmdDispatch->pStadHandles = pStadHandles;  /* Save modules list pointer just for the debug functions */
#endif

    /* Fill the configuration table with the Get/Set functions */
    cmdDispatch_ConfigParamsAccessTable (pCmdDispatch);
}


/** 
 * \fn     cmdDispatch_Destroy
 * \brief  Destroy the module object
 * 
 * Destroy the module object.
 * 
 * \note   
 * \param  hCmdDispatch - The object                                          
 * \return TI_OK - Unload succesfull, TI_NOK - Unload unsuccesfull 
 * \sa     
 */ 
TI_STATUS cmdDispatch_Destroy (TI_HANDLE hCmdDispatch)
{
    TCmdDispatchObj  *pCmdDispatch = (TCmdDispatchObj *)hCmdDispatch;

    /* Free the module object */
    os_memoryFree (pCmdDispatch->hOs, pCmdDispatch, sizeof(TCmdDispatchObj));

    return TI_OK;
}


/** 
 * \fn     cmdDispatch_ConfigParamsAccessTable
 * \brief  Fill the configuration table with the Get/Set functions
 * 
 * Called in the configuration phase by the driver, performs the following:
 *   - for each module that supply a Get/Set services to his parameters, 
 *        fill the corresponding entry in the params access table with the following:
 *          - Get function
 *          - Set function
 *          - Handle to the module
 * This table is used when Getting/Setting a parameter from the OS abstraction layer.
 * 
 * \note   
 * \param  pCmdDispatch - The object                                          
 * \return void
 * \sa     
 */ 
static void cmdDispatch_ConfigParamsAccessTable (TCmdDispatchObj *pCmdDispatch)
{
    pCmdDispatch->paramAccessTable[GET_PARAM_MODULE_NUMBER(AUTH_MODULE_PARAM) - 1].set = auth_setParam;
    pCmdDispatch->paramAccessTable[GET_PARAM_MODULE_NUMBER(AUTH_MODULE_PARAM) - 1].get = auth_getParam;
    pCmdDispatch->paramAccessTable[GET_PARAM_MODULE_NUMBER(AUTH_MODULE_PARAM) - 1].handle = pCmdDispatch->hAuth;

    pCmdDispatch->paramAccessTable[GET_PARAM_MODULE_NUMBER(ASSOC_MODULE_PARAM) - 1].set = assoc_setParam;
    pCmdDispatch->paramAccessTable[GET_PARAM_MODULE_NUMBER(ASSOC_MODULE_PARAM) - 1].get = assoc_getParam;
    pCmdDispatch->paramAccessTable[GET_PARAM_MODULE_NUMBER(ASSOC_MODULE_PARAM) - 1].handle = pCmdDispatch->hAssoc;

    pCmdDispatch->paramAccessTable[GET_PARAM_MODULE_NUMBER(RX_DATA_MODULE_PARAM) - 1].set = rxData_setParam;
    pCmdDispatch->paramAccessTable[GET_PARAM_MODULE_NUMBER(RX_DATA_MODULE_PARAM) - 1].get = rxData_getParam;
    pCmdDispatch->paramAccessTable[GET_PARAM_MODULE_NUMBER(RX_DATA_MODULE_PARAM) - 1].handle = pCmdDispatch->hRxData;

    pCmdDispatch->paramAccessTable[GET_PARAM_MODULE_NUMBER(TX_CTRL_MODULE_PARAM) - 1].set = txCtrlParams_setParam;
    pCmdDispatch->paramAccessTable[GET_PARAM_MODULE_NUMBER(TX_CTRL_MODULE_PARAM) - 1].get = txCtrlParams_getParam;
    pCmdDispatch->paramAccessTable[GET_PARAM_MODULE_NUMBER(TX_CTRL_MODULE_PARAM) - 1].handle = pCmdDispatch->hTxCtrl;

    pCmdDispatch->paramAccessTable[GET_PARAM_MODULE_NUMBER(CTRL_DATA_MODULE_PARAM) - 1].set = ctrlData_setParam;
    pCmdDispatch->paramAccessTable[GET_PARAM_MODULE_NUMBER(CTRL_DATA_MODULE_PARAM) - 1].get = ctrlData_getParam;
    pCmdDispatch->paramAccessTable[GET_PARAM_MODULE_NUMBER(CTRL_DATA_MODULE_PARAM) - 1].handle = pCmdDispatch->hCtrlData;

    pCmdDispatch->paramAccessTable[GET_PARAM_MODULE_NUMBER(SITE_MGR_MODULE_PARAM) - 1].set = siteMgr_setParam;
    pCmdDispatch->paramAccessTable[GET_PARAM_MODULE_NUMBER(SITE_MGR_MODULE_PARAM) - 1].get = siteMgr_getParam;
    pCmdDispatch->paramAccessTable[GET_PARAM_MODULE_NUMBER(SITE_MGR_MODULE_PARAM) - 1].handle = pCmdDispatch->hSiteMgr;

    pCmdDispatch->paramAccessTable[GET_PARAM_MODULE_NUMBER(CONN_MODULE_PARAM) - 1].set = conn_setParam;
    pCmdDispatch->paramAccessTable[GET_PARAM_MODULE_NUMBER(CONN_MODULE_PARAM) - 1].get = conn_getParam;
    pCmdDispatch->paramAccessTable[GET_PARAM_MODULE_NUMBER(CONN_MODULE_PARAM) - 1].handle = pCmdDispatch->hConn;

    pCmdDispatch->paramAccessTable[GET_PARAM_MODULE_NUMBER(RSN_MODULE_PARAM) - 1].set = (TParamFunc)rsn_setParam;
    pCmdDispatch->paramAccessTable[GET_PARAM_MODULE_NUMBER(RSN_MODULE_PARAM) - 1].get = (TParamFunc)rsn_getParam;
    pCmdDispatch->paramAccessTable[GET_PARAM_MODULE_NUMBER(RSN_MODULE_PARAM) - 1].handle = pCmdDispatch->hRsn;

    pCmdDispatch->paramAccessTable[GET_PARAM_MODULE_NUMBER(TWD_MODULE_PARAM) - 1].set = cmdDispatch_SetTwdParam;
    pCmdDispatch->paramAccessTable[GET_PARAM_MODULE_NUMBER(TWD_MODULE_PARAM) - 1].get = cmdDispatch_GetTwdParam;
    pCmdDispatch->paramAccessTable[GET_PARAM_MODULE_NUMBER(TWD_MODULE_PARAM) - 1].handle = (TI_HANDLE)pCmdDispatch;

    pCmdDispatch->paramAccessTable[GET_PARAM_MODULE_NUMBER(REPORT_MODULE_PARAM) - 1].set = (TParamFunc)report_SetParam;
    pCmdDispatch->paramAccessTable[GET_PARAM_MODULE_NUMBER(REPORT_MODULE_PARAM) - 1].get = (TParamFunc)report_GetParam;
    pCmdDispatch->paramAccessTable[GET_PARAM_MODULE_NUMBER(REPORT_MODULE_PARAM) - 1].handle = pCmdDispatch->hReport;

    pCmdDispatch->paramAccessTable[GET_PARAM_MODULE_NUMBER(SME_MODULE_PARAM) - 1].set = sme_SetParam;
    pCmdDispatch->paramAccessTable[GET_PARAM_MODULE_NUMBER(SME_MODULE_PARAM) - 1].get = sme_GetParam;
    pCmdDispatch->paramAccessTable[GET_PARAM_MODULE_NUMBER(SME_MODULE_PARAM) - 1].handle = pCmdDispatch->hSme;

    pCmdDispatch->paramAccessTable[GET_PARAM_MODULE_NUMBER(SCAN_CNCN_PARAM) - 1].set = scanCncnApp_SetParam;
    pCmdDispatch->paramAccessTable[GET_PARAM_MODULE_NUMBER(SCAN_CNCN_PARAM) - 1].get = scanCncnApp_GetParam;
    pCmdDispatch->paramAccessTable[GET_PARAM_MODULE_NUMBER(SCAN_CNCN_PARAM) - 1].handle = pCmdDispatch->hScanCncn;
    
    pCmdDispatch->paramAccessTable[GET_PARAM_MODULE_NUMBER(SCAN_MNGR_PARAM) - 1].set = scanMngr_setParam;
    pCmdDispatch->paramAccessTable[GET_PARAM_MODULE_NUMBER(SCAN_MNGR_PARAM) - 1].get = scanMngr_getParam;
    pCmdDispatch->paramAccessTable[GET_PARAM_MODULE_NUMBER(SCAN_MNGR_PARAM) - 1].handle = pCmdDispatch->hScanMngr;
    
    pCmdDispatch->paramAccessTable[GET_PARAM_MODULE_NUMBER(MLME_SM_MODULE_PARAM) - 1].set = mlme_setParam;
    pCmdDispatch->paramAccessTable[GET_PARAM_MODULE_NUMBER(MLME_SM_MODULE_PARAM) - 1].get = mlme_getParam;
    pCmdDispatch->paramAccessTable[GET_PARAM_MODULE_NUMBER(MLME_SM_MODULE_PARAM) - 1].handle = pCmdDispatch->hMlmeSm;

    pCmdDispatch->paramAccessTable[GET_PARAM_MODULE_NUMBER(REGULATORY_DOMAIN_MODULE_PARAM) - 1].set = regulatoryDomain_setParam;
    pCmdDispatch->paramAccessTable[GET_PARAM_MODULE_NUMBER(REGULATORY_DOMAIN_MODULE_PARAM) - 1].get = regulatoryDomain_getParam;
    pCmdDispatch->paramAccessTable[GET_PARAM_MODULE_NUMBER(REGULATORY_DOMAIN_MODULE_PARAM) - 1].handle = pCmdDispatch->hRegulatoryDomain;

    pCmdDispatch->paramAccessTable[GET_PARAM_MODULE_NUMBER(MEASUREMENT_MODULE_PARAM) - 1].set = measurementMgr_setParam;
    pCmdDispatch->paramAccessTable[GET_PARAM_MODULE_NUMBER(MEASUREMENT_MODULE_PARAM) - 1].get = measurementMgr_getParam;
    pCmdDispatch->paramAccessTable[GET_PARAM_MODULE_NUMBER(MEASUREMENT_MODULE_PARAM) - 1].handle = pCmdDispatch->hMeasurementMgr;

#ifdef XCC_MODULE_INCLUDED
    pCmdDispatch->paramAccessTable[GET_PARAM_MODULE_NUMBER(XCC_MANAGER_MODULE_PARAM) - 1].set = XCCMngr_setParam;
    pCmdDispatch->paramAccessTable[GET_PARAM_MODULE_NUMBER(XCC_MANAGER_MODULE_PARAM) - 1].get = XCCMngr_getParam;
    pCmdDispatch->paramAccessTable[GET_PARAM_MODULE_NUMBER(XCC_MANAGER_MODULE_PARAM) - 1].handle = pCmdDispatch->hXCCMngr;
#endif

    pCmdDispatch->paramAccessTable[GET_PARAM_MODULE_NUMBER(ROAMING_MANAGER_MODULE_PARAM) - 1].set = roamingMngr_setParam;
    pCmdDispatch->paramAccessTable[GET_PARAM_MODULE_NUMBER(ROAMING_MANAGER_MODULE_PARAM) - 1].get = roamingMngr_getParam;
    pCmdDispatch->paramAccessTable[GET_PARAM_MODULE_NUMBER(ROAMING_MANAGER_MODULE_PARAM) - 1].handle = pCmdDispatch->hRoamingMngr;

    pCmdDispatch->paramAccessTable[GET_PARAM_MODULE_NUMBER(SOFT_GEMINI_PARAM) - 1].set = SoftGemini_setParam;
    pCmdDispatch->paramAccessTable[GET_PARAM_MODULE_NUMBER(SOFT_GEMINI_PARAM) - 1].get = SoftGemini_getParam;
    pCmdDispatch->paramAccessTable[GET_PARAM_MODULE_NUMBER(SOFT_GEMINI_PARAM) - 1].handle = pCmdDispatch->hSoftGemini;

    pCmdDispatch->paramAccessTable[GET_PARAM_MODULE_NUMBER(QOS_MANAGER_PARAM) - 1].set = qosMngr_setParams;
    pCmdDispatch->paramAccessTable[GET_PARAM_MODULE_NUMBER(QOS_MANAGER_PARAM) - 1].get = qosMngr_getParams;
    pCmdDispatch->paramAccessTable[GET_PARAM_MODULE_NUMBER(QOS_MANAGER_PARAM) - 1].handle = pCmdDispatch->hQosMngr;

    pCmdDispatch->paramAccessTable[GET_PARAM_MODULE_NUMBER(POWER_MANAGER_PARAM) - 1].set = powerMgr_setParam;
    pCmdDispatch->paramAccessTable[GET_PARAM_MODULE_NUMBER(POWER_MANAGER_PARAM) - 1].get = powerMgr_getParam;
    pCmdDispatch->paramAccessTable[GET_PARAM_MODULE_NUMBER(POWER_MANAGER_PARAM) - 1].handle = pCmdDispatch->hPowerMgr;

#ifdef TI_DBG
    pCmdDispatch->paramAccessTable[GET_PARAM_MODULE_NUMBER(MISC_MODULE_PARAM) - 1].set = cmdDispatch_DebugFuncSet;
    pCmdDispatch->paramAccessTable[GET_PARAM_MODULE_NUMBER(MISC_MODULE_PARAM) - 1].get = cmdDispatch_DebugFuncGet;	/*yael - this function is not in use*/
    pCmdDispatch->paramAccessTable[GET_PARAM_MODULE_NUMBER(MISC_MODULE_PARAM) - 1].handle = (TI_HANDLE)pCmdDispatch;
#endif 

    pCmdDispatch->paramAccessTable[GET_PARAM_MODULE_NUMBER(HEALTH_MONITOR_MODULE_PARAM) - 1].set = healthMonitor_SetParam;
    pCmdDispatch->paramAccessTable[GET_PARAM_MODULE_NUMBER(HEALTH_MONITOR_MODULE_PARAM) - 1].get = healthMonitor_GetParam;
    pCmdDispatch->paramAccessTable[GET_PARAM_MODULE_NUMBER(HEALTH_MONITOR_MODULE_PARAM) - 1].handle = pCmdDispatch->hHealthMonitor;

    pCmdDispatch->paramAccessTable[GET_PARAM_MODULE_NUMBER(CURR_BSS_MODULE_PARAM) - 1].set = currBSS_setParam;
    pCmdDispatch->paramAccessTable[GET_PARAM_MODULE_NUMBER(CURR_BSS_MODULE_PARAM) - 1].get = currBSS_getParam;
    pCmdDispatch->paramAccessTable[GET_PARAM_MODULE_NUMBER(CURR_BSS_MODULE_PARAM) - 1].handle = pCmdDispatch->hCurrBss;
}


/** 
 * \fn     cmdDispatch_SetParam
 * \brief  Set a driver parameter
 * 
 * Called by the OS abstraction layer in order to set a parameter in the driver.
 * If the parameter can not be set from outside the driver it returns a failure status.
 * The parameters is set to the module that uses as its father in the system
 *     (refer to the file paramOut.h for more explanations).
 * 
 * \note   
 * \param  hCmdDispatch - The object                                          
 * \param  param        - The parameter information                                          
 * \return result of parameter setting 
 * \sa     
 */ 
TI_STATUS cmdDispatch_SetParam (TI_HANDLE hCmdDispatch, void *param)                          
{
    TCmdDispatchObj *pCmdDispatch = (TCmdDispatchObj *)hCmdDispatch;
    paramInfo_t     *pParam = (paramInfo_t *)param;
    TI_UINT32        moduleNumber;

    moduleNumber = GET_PARAM_MODULE_NUMBER(pParam->paramType);

    if  (moduleNumber > MAX_PARAM_MODULE_NUMBER)
    {
        return PARAM_MODULE_NUMBER_INVALID;
    }

	if ((pCmdDispatch->paramAccessTable[moduleNumber - 1].set == 0) ||
		(pCmdDispatch->paramAccessTable[moduleNumber - 1].get == 0) ||
		(pCmdDispatch->paramAccessTable[moduleNumber - 1].handle == 0))
	{
	    WLAN_OS_REPORT(("cmdDispatch_SetParam(): NULL pointers!!!, return, ParamType=0x%x\n", pParam->paramType));
		return TI_NOK;
	}

    TRACE2(pCmdDispatch->hReport, REPORT_SEVERITY_INFORMATION , "cmdDispatch_SetParam(): ParamType=0x%x, ModuleNumber=0x%x\n",							 pParam->paramType, moduleNumber);

    return pCmdDispatch->paramAccessTable[moduleNumber - 1].set(pCmdDispatch->paramAccessTable[moduleNumber - 1].handle, pParam);
}


/** 
 * \fn     cmdDispatch_GetParam
 * \brief  Get a driver parameter
 * 
 * Called by the OS abstraction layer in order to get a parameter the driver.
 * If the parameter can not be get from outside the driver it returns a failure status.
 * The parameter is get from the module that uses as its father in the system
 *    (refer to the file paramOut.h for more explanations).
 * 
 * \note   
 * \param  hCmdDispatch - The object                                          
 * \param  param        - The parameter information                                          
 * \return result of parameter getting 
 * \sa     
 */ 
TI_STATUS cmdDispatch_GetParam (TI_HANDLE hCmdDispatch, void *param)
{
    TCmdDispatchObj *pCmdDispatch = (TCmdDispatchObj *) hCmdDispatch;
    paramInfo_t     *pParam = (paramInfo_t *) param;
    TI_UINT32        moduleNumber;
    TI_STATUS        status;

    moduleNumber = GET_PARAM_MODULE_NUMBER(pParam->paramType);

    if  (moduleNumber > MAX_PARAM_MODULE_NUMBER)
    {
        return PARAM_MODULE_NUMBER_INVALID;
    }

	if ((pCmdDispatch->paramAccessTable[moduleNumber - 1].set == 0) ||
		(pCmdDispatch->paramAccessTable[moduleNumber - 1].get == 0) ||
		(pCmdDispatch->paramAccessTable[moduleNumber - 1].handle == 0))
	{
	    WLAN_OS_REPORT(("cmdDispatch_GetParam(): NULL pointers!!!, return, ParamType=0x%x\n", pParam->paramType));
		return TI_NOK;
	}

    TRACE2(pCmdDispatch->hReport, REPORT_SEVERITY_INFORMATION , "cmdDispatch_GetParam(): ParamType=0x%x, ModuleNumber=0x%x\n",							 pParam->paramType, moduleNumber);

    status = pCmdDispatch->paramAccessTable[moduleNumber - 1].get(pCmdDispatch->paramAccessTable[moduleNumber - 1].handle, pParam);

    return status;    
}


/** 
 * \fn     cmdDispatch_SetTwdParam / cmdDispatch_GetParam
 * \brief  Set/Get a TWD parameter
 * 
 * Set/Get a TWD parameter.
 * 
 * \note   
 * \param  hCmdDispatch - The object                                          
 * \param  param        - The parameter information                                          
 * \return parameter set/get result
 * \sa     
 */ 
static TI_STATUS cmdDispatch_SetTwdParam (TI_HANDLE hCmdDispatch, paramInfo_t *pParam)
{
    TCmdDispatchObj *pCmdDispatch = (TCmdDispatchObj *)hCmdDispatch;

    pParam->paramType &= ~(SET_BIT | GET_BIT | TWD_MODULE_PARAM | ASYNC_PARAM | ALLOC_NEEDED_PARAM);

    return TWD_SetParam (pCmdDispatch->hTWD, (TTwdParamInfo *)pParam);
}

static TI_STATUS cmdDispatch_GetTwdParam (TI_HANDLE hCmdDispatch, paramInfo_t *pParam)
{
    TCmdDispatchObj *pCmdDispatch = (TCmdDispatchObj *)hCmdDispatch;

    pParam->paramType &= ~(SET_BIT | GET_BIT | TWD_MODULE_PARAM | ASYNC_PARAM | ALLOC_NEEDED_PARAM);

    return TWD_GetParam (pCmdDispatch->hTWD, (TTwdParamInfo *)pParam);
}




/** 
 * \fn     cmdDispatch_DebugFuncSet / cmdDispatch_DebugFuncGet
 * \brief  Set/Get a debug function parameter
 * 
 * Set/Get a debug function parameter.
 * 
 * \note   
 * \param  hCmdDispatch - The object                                          
 * \param  param        - The parameter information                                          
 * \return parameter set/get result
 * \sa     
 */ 

#ifdef TI_DBG

static TI_STATUS cmdDispatch_DebugFuncSet (TI_HANDLE hCmdDispatch, paramInfo_t *pParam)
{
    TCmdDispatchObj *pCmdDispatch = (TCmdDispatchObj *)hCmdDispatch;
    
    if (hCmdDispatch == NULL || pParam == NULL)
    {
        return TI_NOK;
    }

    switch (pParam->paramType)
    {
	case DEBUG_ACTIVATE_FUNCTION:
		debugFunction (pCmdDispatch->pStadHandles, 
					   *(TI_UINT32*)&pParam->content, 
                       (void*)((TI_UINT32*)&pParam->content + 1));
		break;
	default:
TRACE1(pCmdDispatch->hReport, REPORT_SEVERITY_ERROR, "cmdDispatch_DebugFuncSet bad param=%X\n", pParam->paramType);
		break;
    }
    return TI_OK;
}       



/*yael - this function is not in use*/
static TI_STATUS cmdDispatch_DebugFuncGet (TI_HANDLE hCmdDispatch, paramInfo_t *pParam)
{
    if (hCmdDispatch == NULL || pParam == NULL)
    {
        return TI_NOK;
    }

	/*yael - no use for that function */

    return TI_OK;
}

#endif  /* TI_DBG */


