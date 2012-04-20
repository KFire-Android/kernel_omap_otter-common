#
# Build options
#
# compiling the OAL as static lib
STATIC_LIB ?= y
# compiling in debug mode
DEBUG ?= y
# build the supplicant
BUILD_SUPPL ?= y
# type of supplicant to build
# WPA = Point to TI's supplicant
# ANDROID = Will point to Android's supplicant
SUPPL?=ANDROID
# enable WPA enterprise authorization modes
WPA_ENTERPRISE ?= y
# add XCC support
XCC ?= n

#
# Directories and files
#
WILINK_ROOT = ..
CUDK_ROOT ?= $(WILINK_ROOT)/CUDK
CU_DIR = $(CUDK_ROOT)/configurationutility

LOGGER_DIR = $(CUDK_ROOT)/logger/linux
OUTPUT_DIR ?= $(CUDK_ROOT)/output
TI_OS_LIB_DIR=$(CUDK_ROOT)/os/linux/src
ifeq ($(STATIC_LIB),y)
        TI_OS_LIB = tiOsLib.a
else
        TI_OS_LIB = tiOsLib.so
endif
WLAN_LOADER_DIR = $(CUDK_ROOT)/tiwlan_loader/


#
# Targets
#
CU_TARGET = $(OUTPUT_DIR)/wlan_cu
OS_TARGET = $(OUTPUT_DIR)/$(TI_OS_LIB)
LOADER_TARGET = $(OUTPUT_DIR)/tiwlan_loader
ALL_TARGETS = $(OS_TARGET) $(LOADER_TARGET) $(CU_TARGET)
#Supplicant directory, file and target

ifeq ($(SUPPL),WPA)
  SUPPL_TARGET = $(OUTPUT_DIR)/wpa_supplicant
  TI_SUPP_LIB_DIR = $(CUDK_ROOT)/wpa_suppl
  ifeq ($(BUILD_SUPPL),y)
    ALL_TARGETS += $(SUPPL_TARGET)
  endif
endif

ifeq ($(SUPPL),DEVICESCAPE)
  SUPPL_TARGET = $(OUTPUT_DIR)/wpa_supplicant
  TI_SUPP_LIB_DIR = $(CUDK_ROOT)/devicescape-dsa-XCC-wps-2.0.1-008
  ifeq ($(BUILD_SUPPL),y)
    ALL_TARGETS += $(SUPPL_TARGET)
  endif
endif

ifeq ($(SUPPL),ANDROID)
  TI_SUPP_LIB_DIR = $(CUDK_ROOT)/../../../../../external/wpa_supplicant
endif

.PHONY: all
all: $(ALL_TARGETS)

.PHONY: $(SUPPL_TARGET)
$(SUPPL_TARGET):
ifeq ($(SUPPL),DEVICESCAPE)
	$(MAKE) -e -C $(TI_SUPP_LIB_DIR) DEBUG=$(DEBUG) XCC=$(XCC) CROSS_COMPILE=$(CROSS_COMPILE) STATIC_LIB=$(STATIC_LIB) TI_WLAN_DRIVER=y CONFIG_CTRL_IFACE=unix 
else
	$(MAKE) -e -C $(TI_SUPP_LIB_DIR) DEBUG=$(DEBUG) XCC=$(XCC) CROSS_COMPILE=$(CROSS_COMPILE) STATIC_LIB=$(STATIC_LIB) TI_WLAN_DRIVER=y CONFIG_CTRL_IFACE=unix CONFIG_EAP_WSC=y CONFIG_WPS=y

endif
	$(CROSS_COMPILE)strip -s $(TI_SUPP_LIB_DIR)/wpa_supplicant
	cp -f $(TI_SUPP_LIB_DIR)/wpa_supplicant $(OUTPUT_DIR)

.PHONY: $(OS_TARGET)
$(OS_TARGET):
	@echo ALL_TARGETS=$(ALL_TARGETS)
	@echo SUPPL=$(SUPPL)
	@echo BUILD_SUPPL=$(BUILD_SUPPL) 
	$(MAKE) -C $(TI_OS_LIB_DIR) CROSS_COMPILE=$(CROSS_COMPILE) DEBUG=$(DEBUG) STATIC_LIB=$(STATIC_LIB) XCC=$(XCC) WPA_ENTERPRISE=$(WPA_ENTERPRISE) SUPPL=$(SUPPL) BUILD_SUPPL=$(BUILD_SUPPL) TI_SUPP_LIB_DIR=$(TI_SUPP_LIB_DIR)

.PHONY: $(CU_TARGET)
$(CU_TARGET):
	$(MAKE) -C $(CU_DIR) CROSS_COMPILE=$(CROSS_COMPILE) DEBUG=$(DEBUG) STATIC_LIB=$(STATIC_LIB) XCC=$(XCC) WPA_ENTERPRISE=$(WPA_ENTERPRISE) SUPPL=$(SUPPL) BUILD_SUPPL=$(BUILD_SUPPL) TI_SUPP_LIB_DIR=$(TI_SUPP_LIB_DIR)

.PHONY: $(LOADER_TARGET)
$(LOADER_TARGET):
	$(MAKE) -C $(WLAN_LOADER_DIR) CROSS_COMPILE=$(CROSS_COMPILE) DEBUG=$(DEBUG) STATIC_LIB=$(STATIC_LIB)

.PHONY: clean
clean:
	$(MAKE) -C $(CU_DIR) CROSS_COMPILE=$(CROSS_COMPILE) BUILD_SUPPL=$(BUILD_SUPPL) XCC=$(XCC) clean
	$(MAKE) -C $(TI_OS_LIB_DIR) CROSS_COMPILE=$(CROSS_COMPILE) BUILD_SUPPL=$(BUILD_SUPPL) XCC=$(XCC) clean
	$(MAKE) -C $(WLAN_LOADER_DIR) CROSS_COMPILE=$(CROSS_COMPILE) clean
ifeq ($(BUILD_SUPPL), y)
	$(MAKE) -e -C $(TI_SUPP_LIB_DIR) CROSS_COMPILE=$(CROSS_COMPILE) clean
endif
ifeq ($(SUPPL), WPA)
	rm -f $(OUTPUT_DIR)/wpa_supplicant
	$(MAKE) -C $(TI_SUPP_LIB_DIR) CROSS_COMPILE=$(CROSS_COMPILE) BUILD_SUPPL=$(BUILD_SUPPL) XCC=$(XCC) clean
endif


ifeq ($(ETH_SUPPORT), y)
	DK_DEFINES += -D ETH_SUPPORT
endif 
