
##
##
## File lists and locations
##
##

#
# DK_ROOT must be set prior to including common.inc
#
DK_ROOT = ../../..

#
# Includes common definitions and source file list
#
ifneq ($(KERNELRELEASE),)
    include $(M)/common.inc
    include $(M)/drv_sources.inc
else
    include common.inc
    include drv_sources.inc
endif

#
# Include directory for Odyssey supplicant files
#
ifeq ($(BUILD_SUPPL),y)
    ODD_INC = $(DK_ROOT)/CUDK/Supplicant/odyssey/odSupp/linux/inc 
else
    ODD_INC = 
endif

#
# OS include paths required for compilation.
# 
OS_INCS = $(DK_ROOT)/platforms/os/linux/inc $(DK_ROOT)/platforms/os/common/inc
#OS_INCS += $(DK_ROOT)/stad/src/core/EvHandler $(DK_ROOT)/Test $(DK_ROOT)/platforms/hw/host_platform_$(HOST_PLATFORM)/linux
OS_INCS += $(DK_ROOT)/stad/src/core/EvHandler $(DK_ROOT)/Test $(DK_ROOT)/platforms/hw/linux

#
# Location and filename of the driver .lib file created by this makefile.
#
OUTPUT_DIR = $(DK_ROOT)/stad/build/linux
OUTPUT_FILE = $(OUTPUT_DIR)/libestadrv.a





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
	EXTRA_CFLAGS += $(addprefix -I$(M)/, $(DK_INCS) $(OS_INCS) $(ODD_INC))

#
# Intermediate object name - this should be converted to the appropriate library file
# after the kernel makefile finishes its work.
#
	obj-m = tiwlan_drv.o

#
# List of object files the kernel makefile needs to compile.
#
	tiwlan_drv-y = $(DK_OBJS)


else	# ifneq ($(KERNELRELEASE),)


##
##
## This is the regular build phase - act according to the make actions
##
##

#
# The location of the kernel makefile
#
KERNEL_DIR ?= 


#
# Build the driver lib file
#
.PHONY: all
all: .depend $(OUTPUT_FILE)


#
# Prints variables
#
.PHONY: help
help:
	@echo Default Compilation:	PLATFORM=$(PLATFORM) DEBUG=$(DEBUG) INTR=$(INTR) WSPI=$(WSPI) XCC=$(XCC) EXTRA CFLAGS: $(EXTRA_CFLAGS)
#
# Recursively cleans the driver files.
#
.PHONY: clean
clean:
	$(MAKE) -C $(KERNEL_DIR) M=`pwd` ARCH=arm CROSS_COMPILE=$(CROSS_COMPILE) clean
	@rm -f *.o *.a .*.o.cmd *~ *.~* core .depend dep $(DK_OBJS) $(DEPS)


#
# Causes the library file to get rebuilt.
#
.depend:
	rm -f $(OUTPUT_FILE)
#
# Recursively builds the library file.
#
$(OUTPUT_FILE):
	$(MAKE) -C $(KERNEL_DIR) M=`pwd` ARCH=arm CROSS_COMPILE=$(CROSS_COMPILE) modules
	@$(CROSS_COMPILE)$(AR) rcs $@ $(DK_OBJS)


endif	# ifneq ($(KERNELRELEASE),)
