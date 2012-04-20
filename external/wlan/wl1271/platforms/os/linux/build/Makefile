

##
##
## Make Flags
##
##

NO_ARCH_STRCMP ?= y
NO_USERMODE_WORKAROUND ?= n
MEM_ALLOC_TRACE ?= n
STACK_PROFILING_ON ?= n

##
##
## File lists and locations
##
##

#
# DK_ROOT must be set prior to including common.inc
#
DK_ROOT = ../../../..

#
# Includes common definitions and source file list
#
ifneq ($(KERNELRELEASE),)
    include $(M)/$(DK_ROOT)/stad/build/linux/common.inc
    include $(M)/$(DK_ROOT)/platforms/os/linux/build/os_sources.inc
else
    include $(DK_ROOT)/stad/build/linux/common.inc
    include $(DK_ROOT)/platforms/os/linux/build/os_sources.inc
endif


#
# List of linux OS files needed to be compiled
#
OS_SRCS = \
    $(DK_ROOT)/platforms/os/linux/src/WlanDrvIf.c \
    $(DK_ROOT)/platforms/os/linux/src/osapi.c \
    $(DK_ROOT)/platforms/os/linux/src/osmemapi.c \
    $(DK_ROOT)/platforms/os/linux/src/osRgstry_parser.c \
    $(DK_ROOT)/platforms/os/linux/src/ipc_k.c \
    $(DK_ROOT)/platforms/os/linux/src/WlanDrvWext.c \
    $(DK_ROOT)/platforms/os/linux/src/CmdInterpretWext.c \
    $(DK_ROOT)/platforms/os/linux/src/RxBuf.c \
    $(DK_ROOT)/platforms/hw/linux/host_platform.c \
    $(DK_ROOT)/external_drivers/sdio/linux/SdioDrv.c
#    $(DK_ROOT)/platforms/hw/host_platform_$(HOST_PLATFORM)/linux/host_platform.c

ifeq ($(STACK_PROFILING_ON),y)    
    OS_SRCS += $(DK_ROOT)/platforms/os/linux/src/stack_profile.c
    EXTRA_CFLAGS += -D STACK_PROFILE
endif     

ifeq ($(NO_ARCH_STRCMP),y)
    OS_SRCS += $(DK_ROOT)/platforms/os/linux/src/string.c 
endif

OS_INCS = $(DK_ROOT)/platforms/os/linux/inc $(DK_ROOT)/platforms/os/linux/src $(DK_ROOT)/platforms/os/common/inc
#OS_INCS += $(DK_ROOT)/stad/src/core/EvHandler $(DK_ROOT)/Test $(DK_ROOT)/platforms/hw/host_platform_$(HOST_PLATFORM)/linux
OS_INCS += $(DK_ROOT)/stad/src/core/EvHandler $(DK_ROOT)/Test $(DK_ROOT)/platforms/hw/linux

ifeq ($(WSPI),y)
    OS_SRCS += $(DK_ROOT)/platforms/hw/host_platform_$(HOST_PLATFORM)/linux/WspiAdapter.c 
    OS_INCS += $(EXT_DRV)/$(HOST_PLATFORM)/Linux/spi
endif

ifeq ($(WSPI),n)
#    OS_SRCS += $(DK_ROOT)/platforms/hw/host_platform_$(HOST_PLATFORM)/linux/SdioAdapter.c 
#    OS_INCS += $(EXT_DRV)/$(HOST_PLATFORM)/Linux/sdio
    OS_SRCS += $(DK_ROOT)/platforms/hw/linux/SdioAdapter.c 
    OS_INCS += $(EXT_DRV)/sdio/linux
endif

OS_OBJS = $(patsubst %.c, %.o, $(OS_SRCS))

#
# OS include paths required for compilation.
# 


#
# Firmware
#
FW_SRCS = ./firmware.c ./radio.c
FW_OBJS = $(patsubst %.c, %.o, $(FW_SRCS))
ifeq "$(FW)" "1273"
FW_IMAGE = $(DK_ROOT)/fw/Latest/Fw1273.bin
else
FW_IMAGE = $(DK_ROOT)/fw/Latest/Fw1251rc1.bin
endif
RAD_IMAGE = $(DK_ROOT)/fw/Latest/nvs_map.bin


#
# Location and filename of the linux OS stub object file created by this makefile.
#
OUTPUT_DIR = $(DK_ROOT)/platforms/os/linux/build
OUTPUT_FILE = $(OUTPUT_DIR)/tiwlan_drv_stub.o
OUTPUT_LIB = $(OUTPUT_DIR)/tiwlan_drv_stub.a





##
##
## Compilation Directives
##
##
ifeq ($(NO_USERMODE_WORKAROUND),y)
    EXTRA_CFLAGS += -D NO_USERMODE_WORKAROUND
endif

ifeq ($(POLLING_MODE),y)
    EXTRA_CFLAGS += -D TIWLAN_OMAP1610_IRQ=0
endif

ifeq ($(MEM_ALLOC_TRACE),y)
    EXTRA_CFLAGS += -D TI_MEM_ALLOC_TRACE
endif





##
##
## Build process
##
##

ifneq ($(KERNELRELEASE),)


	##
	##
	## This is the kernel build phase - set the appropriate arguments
	##
	##

	#
	# Adds the current directory as a prefix to all include directories.
	#
	EXTRA_CFLAGS += $(addprefix -I$(M)/, $(OS_DK_INCS) $(OS_INCS))

	#
	# Intermediate object name - this should be renamed to the desired object name
	# after the kernel makefile finishes its work.
	#
	obj-m = tiwlan_drv.o
	#
	# List of object files the kernel makefile needs to compile.
	#
	tiwlan_drv-y = $(OS_OBJS) $(OS_AUXILIARY_LIBS)


else	# ifneq ($(KERNELRELEASE),)


##
##
## This is the regular build phase - act according to the make actions
##
##

#
# The location of the kernel makefile
#
KERNEL_DIR ?= $(KERNEL_DIR)


#
# Build the linux OS stub object file
#
.PHONY: all
all: .depend $(OUTPUT_DIR) $(OUTPUT_FILE)

#
# Prints variables
#
.PHONY: help
help:
	@echo Default Compilation:	PLATFORM=$(PLATFORM) DEBUG=$(DEBUG) INTR=$(INTR) WSPI=$(WSPI) XCC=$(XCC) EXTRA CFLAGS: $(EXTRA_CFLAGS)


#
# Recursively cleans the linux OS stub object files
#
.PHONY: clean
clean:
	$(MAKE) -C $(KERNEL_DIR) M=`pwd` ARCH=arm CROSS_COMPILE=$(CROSS_COMPILE) clean
	@rm -f *.o *.a .*.o.cmd *~ *.~* core .depend dep $(OS_OBJS) $(FW_SRCS) $(FW_OBJS)


#
# Causes the linux OS stub object file to get rebuilt
#
.depend:
	rm -f $(OUTPUT_FILE)	

#
# Make sure the output directory exists
#
$(OUTPUT_DIR):
	mkdir -p $(OUTPUT_DIR)

#
# Recursively builds the linux OS stub object file
#
$(OUTPUT_FILE):
	$(MAKE) -C $(KERNEL_DIR) M=`pwd` ARCH=arm CROSS_COMPILE=$(CROSS_COMPILE) modules
	$(CROSS_COMPILE)$(AR) rcs $(OUTPUT_LIB) $(OS_OBJS)
	mv tiwlan_drv.ko $(OUTPUT_FILE)


#
# Handles firmware image and radio module for when firmware is statically linked
#
$(BIN2C): $(BIN2C).c
	gcc $^ -o $@

firmware.c: $(FW_IMAGE) $(BIN2C)
	$(BIN2C) $< tiwlan_fwimage > $@

radio.c: $(RAD_IMAGE) $(BIN2C)
	$(BIN2C) $< tiwlan_radimage > $@


endif	# ifneq ($(KERNELRELEASE),)
      
