
STATIC_LIB ?= y

DEBUG ?= y
HOST_PLATFORM ?= wipp
WILINK_ROOT = ../..
CUDK_ROOT = $(WILINK_ROOT)/CUDK
TI_OS_LIB_DIR=$(CUDK_ROOT)/output
ifeq ($(STATIC_LIB),y)
        TI_OS_LIB = tiOsLib.a
else
        TI_OS_LIB = tiOsLib.so
endif

ifeq ($(DEBUG),y)
  DEBUGFLAGS = -O2 -g -DDEBUG -DTI_DBG -fno-builtin   # "-O" is needed to expand inlines
#  DEBUGFLAGS+= -DDEBUG_MESSAGES
else
  DEBUGFLAGS = -O2
endif

COMMON  = $(WILINK_ROOT)/stad

#ARMFLAGS  = -fno-common -pipe -msoft-float -mabi=apcs-gnu
ARMFLAGS  = -fno-common -pipe -g -fno-builtin -Wall 

INCLUDES = \
        -I $(CUDK_ROOT)/os/common/inc \
	-I $(WILINK_ROOT)/platforms/os/common/inc \
	-I $(WILINK_ROOT)/platforms/os/linux/inc \
	-I $(WILINK_ROOT)/stad/Export_Inc \
	-I $(WILINK_ROOT)/stad/src/Sta_Management \
	-I $(WILINK_ROOT)/stad/src/Connection_Managment \
	-I $(WILINK_ROOT)/stad/src/Application \
	-I $(WILINK_ROOT)/utils \
	-I $(WILINK_ROOT)/Txn \
	-I $(WILINK_ROOT)/TWD/TWDriver \
	-I $(WILINK_ROOT)/TWD/FirmwareApi \
	-I $(WILINK_ROOT)/TWD/FW_Transfer/Export_Inc \
	-I $(WILINK_ROOT)/TWD/TwIf \
	-I $(CUDK_ROOT)/os/linux/inc \
	-I $(KERNEL_DIR)/include 

OUTPUT_DIR ?= $(CUDK_ROOT)/output

TARGET = $(OUTPUT_DIR)/tiwlan_loader
SRCS := \
	tiwlan_loader.c

OBJS = $(SRCS:.c=.o)

DEPS = $(SRCS:%.c=%.d)

CFLAGS = -Wall -Wstrict-prototypes $(DEBUGFLAGS) $(INCLUDES) -D__LINUX__ $(DK_DEFINES)
CFLAGS += -D__BYTE_ORDER_LITTLE_ENDIAN
CFLAGS += -DDRV_NAME='"tiwlan"'

ifneq "$(CROSS_COMPILE)" ""		#compile for ARM
	CFLAGS += $(ARMFLAGS)
        # strip symbols
ifneq "$(DEBUG)" "y"
    LDFLAGS = -s
endif

endif    # CROSS_COMPILE != ""

.PHONY: all

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CROSS_COMPILE)gcc --static $(OBJS) $(TI_OS_LIB_DIR)/$(TI_OS_LIB) -L $(CUDK_ROOT)/output $(LDFLAGS) -lpthread -lc -o $@
	$(CROSS_COMPILE)strip -s $(TARGET)

ifeq ("$(findstring $(MAKECMDGOALS), clean cleanall)", "")
-include .depend
endif

%.o: %.cpp
	echo $@
	$(CROSS_COMPILE)gcc $(CFLAGS) -c $^ -o $@

%.o: %.c
	@echo $@
	@$(CROSS_COMPILE)gcc $(CFLAGS) -c $< -o $@

%.i: %.c
	$(CROSS_COMPILE)gcc $(CFLAGS) -E -c $< > $@

%.d: %.c
	@echo $@
	@$(CROSS_COMPILE)gcc $< -MT$(^:%.c=%.o) -M $(CFLAGS) > $@

%.d: %.cpp
	echo $@
	$(CROSS_COMPILE)gcc $< -MT$(^:%.cpp=%.o) -M $(CFLAGS) > $@

.depend: $(DEPS)
	rm -f $@
	for aa in $^; do cat $$aa >> $@; done

.PHONY: clean
clean:
	-rm -f $(TARGET) .depend $(OBJS) $(DEPS) $(SRCS:.c=.i) *.ko .*.cmd .*.flags *.mod.c *~ *.~*

