#
# Copyright (C) 2008 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
LOCAL_PATH := $(call my-dir)

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

DK_ROOT = hardware/ti/wlan/$(BOARD_WLAN_DEVICE)
OS_ROOT = $(DK_ROOT)/platforms
STAD	= $(DK_ROOT)/stad
UTILS	= $(DK_ROOT)/utils
TWD	= $(DK_ROOT)/TWD
COMMON  = $(DK_ROOT)/common
TXN	= $(DK_ROOT)/Txn
CUDK	= $(DK_ROOT)/CUDK
LIB	= ../../lib

include $(WPA_SUPPL_DIR)/.config

INCLUDES = $(STAD)/Export_Inc \
	$(STAD)/src/Application \
	$(UTILS) \
	$(OS_ROOT)/os/linux/inc \
	$(OS_ROOT)/os/common/inc \
	$(TWD)/TWDriver \
	$(TWD)/FirmwareApi \
	$(TWD)/TwIf \
	$(TWD)/FW_Transfer/Export_Inc \
	$(TXN) \
	$(CUDK)/configurationutility/inc \
	$(CUDK)/os/common/inc \
	external/openssl/include \
	$(WPA_SUPPL_DIR_INCLUDE) \
	$(DK_ROOT)/../lib
  
L_CFLAGS = -DCONFIG_DRIVER_CUSTOM -DHOST_COMPILE -D__BYTE_ORDER_LITTLE_ENDIAN
L_CFLAGS += -DWPA_SUPPLICANT_$(WPA_SUPPLICANT_VERSION)
OBJS = driver_ti.c $(LIB)/scanmerge.c $(LIB)/shlist.c

# To force sizeof(enum) = 4
L_CFLAGS += -mabi=aapcs-linux

ifdef CONFIG_NO_STDOUT_DEBUG
L_CFLAGS += -DCONFIG_NO_STDOUT_DEBUG
endif

ifdef CONFIG_DEBUG_FILE
L_CFLAGS += -DCONFIG_DEBUG_FILE
endif

ifdef CONFIG_ANDROID_LOG
L_CFLAGS += -DCONFIG_ANDROID_LOG
endif

ifdef CONFIG_IEEE8021X_EAPOL
L_CFLAGS += -DIEEE8021X_EAPOL
endif

ifdef CONFIG_WPS
L_CFLAGS += -DCONFIG_WPS
endif

########################
 
include $(CLEAR_VARS)
LOCAL_MODULE := libCustomWifi
LOCAL_SHARED_LIBRARIES := libc libcutils
LOCAL_CFLAGS := $(L_CFLAGS)
LOCAL_SRC_FILES := $(OBJS)
LOCAL_C_INCLUDES := $(INCLUDES)
include $(BUILD_STATIC_LIBRARY)

########################
