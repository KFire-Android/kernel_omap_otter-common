STATIC_LIB ?= y
DEBUG ?= y
BUILD_SUPPL ?= y
SUPPL ?= ANDROID
WPA_ENTERPRISE ?= y
ETH_SUPPORT ?= n

ifeq ($(DEBUG),y)
  DEBUGFLAGS = -O2 -g -DDEBUG -DTI_DBG -fno-builtin   
else
  DEBUGFLAGS = -O2
endif

WILINK_ROOT = ../../../..
CUDK_ROOT = $(WILINK_ROOT)/CUDK
LIB_NAME = tiOsLib
OUTPUT_DIR ?= $(CUDK_ROOT)/output

DK_DEFINES = 
ifeq ($(WPA_ENTERPRISE), y)
        DK_DEFINES += -D WPA_ENTERPRISE
endif

ifeq ($(BUILD_SUPPL), y)
  DK_DEFINES += -D WPA_SUPPLICANT -D CONFIG_CTRL_IFACE -D CONFIG_CTRL_IFACE_UNIX
  -include $(CUDK_ROOT)/$(TI_SUPP_LIB_DIR)/.config
  ifeq ($(CONFIG_EAP_WSC), y)
    DK_DEFINES += -DCONFIG_EAP_WSC
  endif
endif

ifeq ($(STATIC_LIB),y)
        TARGET = $(OUTPUT_DIR)/$(LIB_NAME).a
else
        TARGET = $(OUTPUT_DIR)/$(LIB_NAME).so
endif

ifeq ($(ETH_SUPPORT), y)
	DK_DEFINES += -DETH_SUPPORT
endif

SRCS  := \
        cu_wext.c \
        ipc_sta.c \
        ipc_event.c \
        ipc_wpa.c \
        os_trans.c \
        ParsEvent.c \
        osapi.c
ifeq ($(BUILD_SUPPL), y)
SRCS  +=  \
        $(CUDK_ROOT)/$(TI_SUPP_LIB_DIR)/wpa_ctrl.c
endif
        
OBJS = $(SRCS:.c=.o)

DEPS = $(SRCS:%.c=%.d)

ARMFLAGS  = -fno-common -pipe

INCLUDES = \
        -I ../inc \
        -I ../../common/inc \
        -I $(WILINK_ROOT)/stad/Export_Inc \
        -I $(WILINK_ROOT)/stad/src/Sta_Management \
        -I $(WILINK_ROOT)/stad/src/Application \
        -I $(WILINK_ROOT)/utils \
        -I $(WILINK_ROOT)/Txn \
        -I $(WILINK_ROOT)/TWD/TWDriver \
		-I $(WILINK_ROOT)/TWD/FirmwareApi \
		-I $(WILINK_ROOT)/TWD/FW_Transfer/Export_Inc \
		-I $(WILINK_ROOT)/TWD/TwIf \
        -I $(WILINK_ROOT)/platforms/os/linux/inc \
        -I $(WILINK_ROOT)/platforms/os/common/inc \
        -I $(WILINK_ROOT)/TWD/FirmwareApi \
        -I $(CUDK_ROOT)/$(TI_SUPP_LIB_DIR) \
        -I $(CUDK_ROOT)/configurationutility/inc
        
CFLAGS = -Wall -Wstrict-prototypes $(DEBUGFLAGS) $(INCLUDES) -D__LINUX__ $(DK_DEFINES) -D__BYTE_ORDER_LITTLE_ENDIAN

ifneq "$(CROSS_COMPILE)" ""             #compile for ARM
        CFLAGS += $(ARMFLAGS)
        # strip symbols
ifneq "$(DEBUG)" "y"
    LDFLAGS = -s
endif

endif    # CROSS_COMPILE != ""

.PHONY: all

all: $(TARGET)

$(OUTPUT_DIR)/$(LIB_NAME).a: $(OBJS)
	rm -f $@
	$(CROSS_COMPILE)$(AR) -q $@ $^

$(OUTPUT_DIR)/$(LIB_NAME).so: $(OBJS)
	rm -f $@
	$(CROSS_COMPILE)gcc -shared $(CFLAGS) $(OBJS) -lc -o $@
	$(CROSS_COMPILE)strip $@

ifeq ("$(findstring $(MAKECMDGOALS), clean cleanall)", "")
-include .depend
endif

%.o: %.cpp
	@echo $@
	@$(CROSS_COMPILE)gcc $(CFLAGS) -c $^ -o $@

%.o: %.c
	@echo $@
	@$(CROSS_COMPILE)gcc -s $(CFLAGS) -c $< -o $@

%.i: %.c
	@$(CROSS_COMPILE)gcc $(CFLAGS) -E -c $< > $@

%.d: %.c
	@echo $@
	@$(CROSS_COMPILE)gcc $< -MT$(^:%.c=%.o) -M $(CFLAGS) > $@

%.d: %.cpp
	@echo $@
	@$(CROSS_COMPILE)gcc $< -MT$(^:%.cpp=%.o) -M $(CFLAGS) > $@

.depend: $(DEPS)
	rm -f $@
	for aa in $^; do cat $$aa >> $@; done

.PHONY: clean
clean:
	@echo cleaning OS library...
	rm -f .depend $(OBJS) $(DEPS) $(SRCS:.c=.i) *.ko .*.cmd .*.flags *.mod.c *~ *.~* $(TARGET)

cleanall: clean 

