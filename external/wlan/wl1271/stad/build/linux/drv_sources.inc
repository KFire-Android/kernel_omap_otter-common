
#start of TNETW_INCS#    do not remove or change this comment
TNETW_INCS = \
    $(COMMON)/Export_Inc \
    $(UTILS) \
    $(PLATFORMS)/os/common/inc \
    $(PLATFORMS)/os/linux/src \
    $(PLATFORMS)/os/linux/inc \
    $(PLATFORMS)/hw/linux \
    $(EXT_DRV)/$(BUS_DRV)/linux \
    $(EXT_APP)/Linux/bmtrace/Export_Inc \
    $(TWD) \
    $(TWD)/Ctrl \
    $(TWD)/Data_Service/Export_Inc \
    $(TWD)/FW_Transfer/Export_Inc \
    $(TWD)/FW_Transfer \
    $(TWD)/Ctrl/Export_Inc \
    $(TWD)/MacServices/Export_Inc \
    $(TWD)/FirmwareApi \
    $(TWD)/TwIf/ \
    $(TWD)/MacServices \
    $(TWD)/TWDriver \
    $(TXN)

ifeq ($(XCC),y)
#start of TNETW_INCS_XCC#   do not remove or change this comment
    TNETW_INCS += \
        $(COMMON)/Export_Inc/XCC \
        $(DK_ROOT)/utils/XCC

    TNETW_INCS_XCC = \
        $(COMMON)/Export_Inc/XCC

    TNETW_INCS += $(TNETW_INCS_XCC)
#end of TNETW_INCS_XCC#    do not remove or change this comment
endif

#end of TNETW_INCS#    do not remove or change this comment


#start of CORE_INCS#     do not remove or change this comment
CORE_INCS = \
    $(COMSRC)/AirLink_Managment \
    $(COMSRC)/Application \
    $(COMSRC)/XCC \
    $(COMSRC)/Connection_Managment \
    $(COMSRC)/Ctrl_Interface \
    $(COMSRC)/Data_link \
    $(COMSRC)/Connection_Managment \
    $(COMSRC)/Sta_Management \
    $(COMSRC)/Connection_Managment

#ifeq ($(DEBUG),y)
    CORE_INCS += \
        $(TESTSRC)/
#endif 

#start of CORE_INCS_XCC#     do not remove or change this comment
ifeq ($(XCC),y)
    CORE_INCS_XCC += \
        $(COMSRC)/XCC

    CORE_INCS += $(CORE_INCS_XCC)
endif
#end of CORE_INCS_XCC#   do not remove or change this comment

#end of CORE_INCS#   do not remove or change this comment





#start of UTIL_SRCS#    do not remove or change this comment
UTIL_SRCS = \
    $(UTILS)/fsm.c \
    $(UTILS)/GenSM.c \
    $(UTILS)/report.c \
    $(UTILS)/queue.c \
    $(UTILS)/context.c \
    $(UTILS)/timer.c \
    $(UTILS)/rate.c \
    $(UTILS)/freq.c
#end of UTIL_SRCS#      do not remove or change this comment


#start of TNETW_SRCS#   do not remove or change this comment
TNETW_SRCS = \
    $(TWD)/MacServices/MacServices.c \
    $(TWD)/MacServices/ScanSrv.c \
    $(TWD)/MacServices/ScanSrvSM.c \
    $(TWD)/MacServices/PowerSrv.c \
    $(TWD)/MacServices/PowerSrvSM.c \
    $(TWD)/MacServices/MeasurementSrv.c \
    $(TWD)/MacServices/MeasurementSrvSM.c \
    $(TWD)/MacServices/measurementSrvDbgPrint.c \
    $(TWD)/TWDriver/TWDriver.c \
    $(TWD)/TWDriver/TWDriverTx.c \
    $(TWD)/TWDriver/TWDriverCtrl.c \
    $(TWD)/TWDriver/TWDriverRadio.c \
    $(TWD)/Ctrl/CmdQueue.c \
    $(TWD)/Ctrl/CmdBld.c \
    $(TWD)/Ctrl/CmdBldCfg.c \
    $(TWD)/Ctrl/CmdBldCmd.c \
    $(TWD)/Ctrl/CmdBldItr.c \
    $(TWD)/Ctrl/CmdBldCfgIE.c \
    $(TWD)/Ctrl/CmdBldCmdIE.c \
    $(TWD)/Ctrl/CmdBldItrIE.c \
    $(TWD)/Data_Service/txHwQueue.c \
    $(TWD)/Data_Service/txCtrlBlk.c \
    $(TWD)/Data_Service/RxQueue.c \
    $(TWD)/FW_Transfer/CmdMBox.c \
    $(TWD)/FW_Transfer/txResult.c \
    $(TWD)/FW_Transfer/txXfer.c \
    $(TWD)/FW_Transfer/FwEvent.c \
    $(TWD)/FW_Transfer/RxXfer.c \
    $(TWD)/FW_Transfer/eventMbox.c \
    $(TWD)/FW_Transfer/HwInit.c \
    $(TWD)/FW_Transfer/fwDebug.c \
    $(TWD)/TwIf/TwIf.c \
    $(TXN)/TxnQueue.c

ifeq ($(WSPI),y)
    TNETW_SRCS += \
	$(TXN)/WspiBusDrv.c 
endif

ifeq ($(WSPI),n)
    TNETW_SRCS += \
	$(TXN)/SdioBusDrv.c 
endif

#start of TNETW_SRCS_XCC#   do not remove or change this comment
#end of TNETW_SRCS_XCC#   do not remove or change this comment

#end of TNETW_SRCS#    do not remove or change this comment

#start of CORE_SRCS#   do not remove or change this comment
CORE_SRCS = \
    $(COMSRC)/Ctrl_Interface/DrvMain.c \
    $(COMSRC)/Ctrl_Interface/EvHandler.c \
    $(COMSRC)/Ctrl_Interface/CmdHndlr.c \
    $(COMSRC)/Ctrl_Interface/CmdDispatcher.c \
    $(COMSRC)/Data_link/txCtrl.c \
    $(COMSRC)/Data_link/txCtrlServ.c \
    $(COMSRC)/Data_link/txCtrlParams.c \
    $(COMSRC)/Data_link/txPort.c \
    $(COMSRC)/Data_link/txDataQueue.c \
    $(COMSRC)/Data_link/TxDataClsfr.c \
    $(COMSRC)/Data_link/txMgmtQueue.c \
    $(COMSRC)/Data_link/Ctrl.c \
    $(COMSRC)/Data_link/GeneralUtil.c \
    $(COMSRC)/Data_link/TrafficMonitor.c \
    $(COMSRC)/Data_link/rx.c \
    $(COMSRC)/Sta_Management/PowerMgr.c \
    $(COMSRC)/Sta_Management/PowerMgrKeepAlive.c \
    $(COMSRC)/Sta_Management/PowerMgrDbgPrint.c \
    $(COMSRC)/Sta_Management/healthMonitor.c \
    $(COMSRC)/Sta_Management/siteMgr.c \
    $(COMSRC)/Sta_Management/StaCap.c \
    $(COMSRC)/Sta_Management/siteHash.c \
    $(COMSRC)/Sta_Management/systemConfig.c \
    $(COMSRC)/Sta_Management/templates.c \
    $(COMSRC)/Sta_Management/mlmeBuilder.c \
    $(COMSRC)/Sta_Management/mlmeParser.c \
    $(COMSRC)/Sta_Management/mlmeSm.c \
    $(COMSRC)/Sta_Management/assocSM.c \
    $(COMSRC)/Sta_Management/authSm.c \
    $(COMSRC)/Sta_Management/openAuthSm.c \
    $(COMSRC)/Sta_Management/sharedKeyAuthSm.c \
    $(COMSRC)/Sta_Management/scr.c \
    $(COMSRC)/Sta_Management/ScanCncn.c \
    $(COMSRC)/Sta_Management/ScanCncnSm.c \
    $(COMSRC)/Sta_Management/ScanCncnSmSpecific.c \
    $(COMSRC)/Sta_Management/ScanCncnOsSm.c \
    $(COMSRC)/Sta_Management/ScanCncnApp.c \
    $(COMSRC)/Sta_Management/scanResultTable.c \
    $(COMSRC)/Sta_Management/currBss.c \
    $(COMSRC)/Sta_Management/qosMngr.c \
    $(COMSRC)/Sta_Management/trafficAdmControl.c \
    $(COMSRC)/AirLink_Managment/SwitchChannel.c \
    $(COMSRC)/AirLink_Managment/measurementMgr.c \
    $(COMSRC)/AirLink_Managment/measurementMgrSM.c \
    $(COMSRC)/AirLink_Managment/spectrumMngmntMgr.c \
    $(COMSRC)/AirLink_Managment/requestHandler.c \
    $(COMSRC)/AirLink_Managment/SoftGemini.c \
    $(COMSRC)/AirLink_Managment/regulatoryDomain.c \
    $(COMSRC)/Connection_Managment/conn.c \
    $(COMSRC)/Connection_Managment/connIbss.c \
    $(COMSRC)/Connection_Managment/connInfra.c \
    $(COMSRC)/Connection_Managment/sme.c \
    $(COMSRC)/Connection_Managment/smeSm.c \
    $(COMSRC)/Connection_Managment/smeSelect.c \
    $(COMSRC)/Connection_Managment/rsn.c \
    $(COMSRC)/Connection_Managment/mainSecSm.c \
    $(COMSRC)/Connection_Managment/mainSecNull.c \
    $(COMSRC)/Connection_Managment/mainSecKeysOnly.c \
    $(COMSRC)/Connection_Managment/admCtrl.c \
    $(COMSRC)/Connection_Managment/admCtrlWep.c \
    $(COMSRC)/Connection_Managment/admCtrlNone.c \
    $(COMSRC)/Connection_Managment/admCtrlWpa.c \
    $(COMSRC)/Connection_Managment/admCtrlWpa2.c \
    $(COMSRC)/Connection_Managment/apConn.c \
    $(COMSRC)/Connection_Managment/keyDerive.c \
    $(COMSRC)/Connection_Managment/keyDeriveWep.c \
    $(COMSRC)/Connection_Managment/keyParser.c \
    $(COMSRC)/Connection_Managment/keyDeriveAes.c \
    $(COMSRC)/Connection_Managment/keyDeriveTkip.c \
    $(COMSRC)/Connection_Managment/broadcastKeySM.c \
    $(COMSRC)/Connection_Managment/unicastKeySM.c \
    $(COMSRC)/Connection_Managment/broadcastKey802_1x.c \
    $(COMSRC)/Connection_Managment/unicastKey802_1x.c \
    $(COMSRC)/Connection_Managment/broadcastKeyNone.c \
    $(COMSRC)/Connection_Managment/unicastKeyNone.c \
    $(COMSRC)/Connection_Managment/keyParserExternal.c \
    $(COMSRC)/Connection_Managment/keyParserWep.c \
    $(COMSRC)/Connection_Managment/mainKeysSm.c \
    $(COMSRC)/Connection_Managment/externalSec.c \
    $(COMSRC)/Application/scanMngr.c \
    $(COMSRC)/Application/roamingMngr.c \
    $(COMSRC)/Application/roamingMngr_manualSM.c \
    $(COMSRC)/Application/roamingMngr_autoSM.c

#ifeq ($(DEBUG),y)
    CORE_SRCS += \
        $(TESTSRC)/connDebug.c \
        $(TESTSRC)/siteMgrDebug.c \
        $(TESTSRC)/ctrlDbg.c \
        $(TESTSRC)/RxDbg.c \
        $(TESTSRC)/TxDbg.c \
        $(TESTSRC)/debug.c \
        $(TESTSRC)/PowerMgrDebug.c \
        $(TESTSRC)/rsnDbg.c \
        $(TESTSRC)/TWD_Debug.c \
        $(TESTSRC)/measurementDbg.c \
        $(TESTSRC)/scanCncnDbg.c \
        $(TESTSRC)/qosMngrDbg.c \
        $(TESTSRC)/ScanMngrDbg.c \
        $(TESTSRC)/scanCncnDbg.c \
        $(TESTSRC)/roamingMgrDebug.c \
        $(TESTSRC)/scrDbg.c \
        $(TESTSRC)/SoftGeminiDbg.c \
        $(TESTSRC)/HealthMonitorDbg.c \
        $(TESTSRC)/smeDebug.c \
        $(TESTSRC)/fwdriverdebug.c \
	$(TESTSRC)/MibDbg.c \
        $(TESTSRC)/TwIfDebug.c
#endif

#start of CORE_SRCS_XCC#   do not remove or change this comment
ifeq ($(XCC),y)
    CORE_SRCS_XCC += \
        $(COMSRC)/Connection_Managment/keyDeriveCkip.c \
        $(COMSRC)/XCC/admCtrlXCC.c \
        $(COMSRC)/XCC/XCCRMMngr.c \
        $(COMSRC)/XCC/XCCTSMngr.c \
        $(COMSRC)/XCC/XCCMngr.c \
        $(COMSRC)/XCC/rogueAp.c \
        $(COMSRC)/XCC/TransmitPowerXCC.c 

    CORE_SRCS += $(CORE_SRCS_XCC)
endif
#end of CORE_SRCS_XCC#   do not remove or change this comment

#end of CORE_SRCS#   do not remove or change this comment


DK_INCS = \
$(TNETW_INCS) \
$(CORE_INCS)

DK_SRCS = \
$(CORE_SRCS) \
$(TNETW_SRCS)

DK_OBJS = $(patsubst %.c, %.o, $(DK_SRCS) $(UTIL_SRCS))


