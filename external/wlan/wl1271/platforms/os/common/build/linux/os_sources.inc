
#start of OS_DK_INCS_FILES#   do not remove or change this comment
OS_DK_INCS_FILES = \
        $(COMMON)/Export_Inc/Ethernet.h \
        $(COMMON)/Export_Inc/TI_IPC_Api.h \
        $(COMMON)/Export_Inc/paramOut.h \
        $(COMMON)/Export_Inc/tiwlnif.h \
        $(COMMON)/Export_Inc/STADExternalIf.h \
        $(COMMON)/Export_Inc/bssTypes.h \
        $(COMMON)/Export_Inc/InternalCmdCodes.h \
        $(COMMON)/Export_Inc/coreDefaultParams.h \
        $(COMMON)/Export_Inc/TI_Results.h \
        $(COMMON)/Export_Inc/privateCmd.h \
        $(COMMON)/Export_Inc/p_buffer.h \
        $(COMSRC)/Sta_Management/mlmeApi.h \
        $(COMSRC)/Sta_Management/healthMonitor.h \
        $(COMSRC)/Connection_Managment/rsnApi.h \
        $(COMSRC)/Ctrl_Interface/DrvMain.h \
        $(COMSRC)/Ctrl_Interface/CmdHndlr.h \
        $(COMSRC)/Ctrl_Interface/CmdDispatcher.h \
        $(COMSRC)/Ctrl_Interface/CmdInterpret.h \
        $(COMSRC)/Ctrl_Interface/EvHandler.h \
        $(COMSRC)/Data_link/txDataQueue_Api.h \
        $(COMSRC)/Data_link/txMgmtQueue_Api.h \
        $(COMSRC)/Ctrl_Interface/DrvMainModules.h \
        $(COMSRC)/Application/roamingMngrTypes.h \
        $(COMSRC)/Application/scanMngrTypes.h \
        $(UTILS)/fsm.h \
        $(UTILS)/report.h \
        $(UTILS)/utils.h \
        $(UTILS)/tidef.h \
        $(UTILS)/rate.h \
        $(UTILS)/802_11Defs.h \
        $(UTILS)/osDot11.h \
        $(UTILS)/commonTypes.h \
        $(UTILS)/CmdInterfaceCodes.h \
        $(UTILS)/tiQosTypes.h \
        $(UTILS)/version.h \
        $(TWD)/TWDriver/TWDriver.h \
        $(TWD)/TWDriver/TWDriverMsr.h \
        $(TWD)/TWDriver/TWDriverRate.h \
        $(TWD)/TWDriver/TWDriverScan.h \
        $(TWD)/TWDriver/Device.h \
        $(TWD)/TWDriver/Device1273.h \
        $(TWD)/Ctrl/Export_Inc/CmdQueue_api.h \
        $(TWD)/MacServices/Export_Inc/MacServices_api.h \
        $(TWD)/FW_Transfer/Export_Inc/rxXfer_api.h \
        $(TWD)/FW_Transfer/Export_Inc/fwDebug_api.h \
        $(TWD)/FirmwareApi/public_descriptors.h \
        $(TWD)/FirmwareApi/public_infoele.h \
        $(TWD)/FirmwareApi/public_types.h \
        $(TWD)/FirmwareApi/public_host_int.h \
        $(TWD)/FirmwareApi/public_radio.h \
        $(TWD)/FirmwareApi/public_commands.h \
        $(TWD)/FirmwareApi/public_event_mbox.h \
        $(TWD)/TwIf/TwIf.h \
        $(TXN)/BusDrv.h \
        $(TXN)/TxnQueue.h \
	$(EXT_APP)/Linux/bmtrace/Export_Inc \
        $(TESTSRC)/debug.h

#start of OS_DK_INCS_FILES_XCC#   do not remove or change this comment
ifeq ($(XCC),y)
    OS_DK_INCS_FILES_XCC = \
        $(COMMON)/Export_Inc/XCC/paramOutXCC.h \
        $(UTILS)/XCC/osDot11XCC.h

    OS_DK_INCS_FILES += $(OS_DK_INCS_FILES_XCC)
endif
#end of OS_DK_INCS_FILES_XCC#   do not remove or change this comment

#start of OS_DK_INCS_FILES#   do not remove or change this comment


#start of OS_DK_INCS#   do not remove or change this comment
OS_DK_INCS = \
        $(COMMON)/Export_Inc \
        $(COMSRC)/AirLink_Managment \
        $(COMSRC)/Application \
        $(COMSRC)/Connection_Managment \
        $(COMSRC)/Ctrl_Interface \
        $(COMSRC)/Data_link \
        $(COMSRC)/Sta_Management \
        $(UTILS) \
        $(TWD) \
        $(TWD)/Ctrl \
        $(TWD)/Data_Service/Export_Inc \
        $(TWD)/FW_Transfer/Export_Inc \
        $(TWD)/Ctrl/Export_Inc \
        $(TWD)/MacServices/Export_Inc \
        $(TWD)/FirmwareApi \
        $(TWD)/TwIf \
        $(TWD)/MacServices \
        $(TWD)/TWDriver \
        $(TESTSRC) \
        $(TXN)

#start of OS_DK_INCS_XCC#   do not remove or change this comment
ifeq ($(XCC),y)
    OS_DK_INCS_XCC = \
        $(COMMON)/Export_Inc/XCC \
        $(COMSRC)/XCC \
        $(UTILS)/XCC

        OS_DK_INCS += $(OS_DK_INCS_XCC)
endif
#end of OS_DK_INCS_XCC#   do not remove or change this comment

#start of OS_DK_INCS#   do not remove or change this comment
