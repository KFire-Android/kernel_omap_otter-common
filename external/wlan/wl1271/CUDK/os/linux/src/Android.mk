LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

STATIC_LIB ?= y
DEBUG ?= y
BUILD_SUPPL ?= n
WPA_ENTERPRISE ?= y

ifeq ($(DEBUG),y)
  DEBUGFLAGS = -O2 -g -DDEBUG -DTI_DBG -fno-builtin   
else
  DEBUGFLAGS = -O2
endif

WILINK_ROOT = ../../../..
CUDK_ROOT = $(WILINK_ROOT)/CUDK
ifndef WPA_SUPPLICANT_VERSION
WPA_SUPPLICANT_VERSION := VER_0_5_X
endif

ifeq ($(WPA_SUPPLICANT_VERSION),VER_0_5_X)
WPA_SUPPL_DIR = external/wpa_supplicant
else
WPA_SUPPL_DIR = external/wpa_supplicant_6/wpa_supplicant
endif
WPA_SUPPL_DIR_INCLUDE = $(WPA_SUPPL_DIR)
ifeq ($(WPA_SUPPLICANT_VERSION),VER_0_6_X)
WPA_SUPPL_DIR_INCLUDE += $(WPA_SUPPL_DIR)/src \
	$(WPA_SUPPL_DIR)/src/common \
	$(WPA_SUPPL_DIR)/src/drivers \
	$(WPA_SUPPL_DIR)/src/l2_packet \
	$(WPA_SUPPL_DIR)/src/utils \
	$(WPA_SUPPL_DIR)/src/wps
endif

DK_DEFINES = 
ifeq ($(WPA_ENTERPRISE), y)
DK_DEFINES += -D WPA_ENTERPRISE
endif

ifeq ($(BUILD_SUPPL), y)
DK_DEFINES += -D WPA_SUPPLICANT -D CONFIG_CTRL_IFACE -D CONFIG_CTRL_IFACE_UNIX
-include $(WPA_SUPPL_DIR)/.config
ifdef CONFIG_WPS
DK_DEFINES += -DCONFIG_WPS
endif
endif

LOCAL_CFLAGS += \
	-Wall -Wstrict-prototypes $(DEBUGFLAGS) -D__LINUX__ $(DK_DEFINES) -D__BYTE_ORDER_LITTLE_ENDIAN -fno-common #-pipe

LOCAL_SRC_FILES:= \
	cu_wext.c \
	ipc_sta.c \
	ipc_event.c \
	ipc_wpa.c \
	os_trans.c \
	ParsEvent.c \
	osapi.c

ifeq ($(BUILD_SUPPL), y)
	ifeq ($(WPA_SUPPLICANT_VERSION),VER_0_5_X)
	LOCAL_SRC_FILES += $(WPA_SUPPL_DIR)/wpa_ctrl.c
	else
	LOCAL_SRC_FILES += $(WPA_SUPPL_DIR)/common/src/wpa_ctrl.c
	endif
endif

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/../inc \
	$(LOCAL_PATH)/../../common/inc \
	$(LOCAL_PATH)/$(WILINK_ROOT)/stad/Export_Inc \
	$(LOCAL_PATH)/$(WILINK_ROOT)/stad/src/Sta_Management \
	$(LOCAL_PATH)/$(WILINK_ROOT)/stad/src/Application \
	$(LOCAL_PATH)/$(WILINK_ROOT)/utils \
	$(LOCAL_PATH)/$(WILINK_ROOT)/Txn \
	$(LOCAL_PATH)/$(WILINK_ROOT)/TWD/TWDriver \
	$(LOCAL_PATH)/$(WILINK_ROOT)/TWD/FirmwareApi \
	$(LOCAL_PATH)/$(WILINK_ROOT)/TWD/FW_Transfer/Export_Inc \
	$(LOCAL_PATH)/$(WILINK_ROOT)/TWD/TwIf \
	$(LOCAL_PATH)/$(WILINK_ROOT)/platforms/os/linux/inc \
	$(LOCAL_PATH)/$(WILINK_ROOT)/platforms/os/common/inc \
	$(LOCAL_PATH)/$(WILINK_ROOT)/TWD/FirmwareApi \
	$(LOCAL_PATH)/$(CUDK_ROOT)/configurationutility/inc \
	$(WPA_SUPPL_DIR_INCLUDE)

LOCAL_MODULE := libtiOsLib

include $(BUILD_STATIC_LIBRARY)
