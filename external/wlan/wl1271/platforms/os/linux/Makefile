
##
##
## Application Make Flags
##
##

#
# Statically link the CU
#
STATIC_LIB ?= y

#
# Rebuild the supplicant
#
BUILD_SUPPL ?= n

#
# Supplicant
# 
SUPPL ?= ANDROID

#
# XCC
# 
XCC ?= n
#ifeq ($(XCC),y)
#	SUPPL = DEVICESCAPE
#endif

#
# FW
# 
FW ?= 1273

#
# Full Async Mode
# 
FULL_ASYNC ?= n

#
# Build bmtrace performance tool
# 
BMTRACE ?= n

#
# Full Async Mode
# 
USE_IRQ_ACTIVE_HIGH ?= n

#
# bus test-driver
#
TEST ?= n

#
# Eth Support
#
ETH_SUPPORT ?= n

#
# Debug
#
DEBUG ?= y

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
    include $(M)/$(DK_ROOT)/stad/build/linux/common.inc
else
    include $(DK_ROOT)/stad/build/linux/common.inc
endif

#
# Location and filename of the driver .lib file.
#
DRIVER_LIB_DIR = $(DK_ROOT)/stad/build/linux
DRIVER_LIB = $(DRIVER_LIB_DIR)/libestadrv.a

#
# Location and filename of the OS .lib file.
#
OS_COMMON_DIR = $(DK_ROOT)/platforms/os/common/build/linux
OS_COMMON = $(OS_COMMON_DIR)/libuadrv.a

#
# Location and filename of the linux OS object file.
#
OS_LINUX_DIR = $(DK_ROOT)/platforms/os/linux/build
OS_LINUX = $(OS_LINUX_DIR)/tiwlan_drv_stub.o
OS_AUXILIARY_LIBS = ../$(DRIVER_LIB)\ ../$(OS_COMMON)

#
# Location and filename of the wlan user-mode programs root directory.
#
WLAN_CUDK_DIR = $(DK_ROOT)/CUDK

#
# Location and filename of the wlan configuraion utility CLI program.
#
WLAN_CU_CLI_DIR = $(DK_ROOT)/CUDK/configurationutility
WLAN_CU_CLI = $(WLAN_CUDK_DIR)/output/wlan_cu

#
# Location and filename of the wlan logger utility program.
#
WLAN_LOGGER_DIR = $(DK_ROOT)/CUDK/logger
WLAN_LOGGER = $(WLAN_CUDK_DIR)/output/wlan_logger

#
# Location and filename of the WLAN loader utility
#
WLAN_LOADER_DIR = $(DK_ROOT)/CUDK/tiwlan_loader/
WLAN_LOADER = $(WLAN_CUDK_DIR)/output/tiwlan_loader

#
# Location and filename of the Linux Wireless Tools
#

#LINUX_WIRELESS_TOOLS_DIR = $(DK_ROOT)/CUDK/wireless_tools
#LINUX_WIRELESS_TOOLS = $(LINUX_WIRELESS_TOOLS_DIR)/iwconfig

#
# The location of the supplicant depending on whether or not we rebuild it.
#
SUPPLICANT = $(WLAN_CUDK_DIR)/output/wpa_supplicant

#
# The combined linux module file.
#
OUTPUT_DIR = $(DK_ROOT)/platforms/os/linux
OUTPUT_FILE = $(OUTPUT_DIR)/tiwlan_drv.ko

##
##
## Build process
##
##

#ifneq ($(KERNELRELEASE),)


##
##
## This is the kernel build phase - set the appropriate arguments
##
##

#
# Intermediate object name - this should be renamed to the desired object name
# after the kernel makefile finishes its work.
#
#       obj-m = linux.o

#
# List of object files the kernel makefile needs to compile.
#
#       linux-y = $(DRIVER_LIB) $(OS_COMMON) $(OS_LINUX)


#else   # ifneq ($(KERNELRELEASE),)


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
# Dont use the original mkfs.jffs2 on TI's servers (voice.img won't work)
#
MAKEJFFS2 ?= /apps/crosstool/bin/mkfs.jffs2

#
# Used to check if the necessary packages are present.
#
HAVE_NONGPL = $(wildcard $(DRIVER_LIB_DIR)/Makefile)
HAVE_GPL = $(wildcard $(OS_LINUX_DIR)/Makefile)
#
# Combines all the three components to create the driver.
#
HOST_PLATFORM ?= *UNDEFINED*
ifeq "$(HOST_PLATFORM)" "omap2430"
TARGET = apps.tar
endif
ifeq "$(HOST_PLATFORM)" "omap3430"
TARGET = apps.tar
endif
ifeq "$(HOST_PLATFORM)" "zoom2"
TARGET = apps.tar
endif
ifeq "$(HOST_PLATFORM)" "zoom1"
TARGET = apps.tar
endif
ifeq "$(HOST_PLATFORM)" "sholes"
TARGET = apps.tar
endif
#
.PHONY: all
all: verifypackages .depend $(TARGET)
help:
	@echo 
	@echo 'In order to select host platform one of the scripts: omap2430_env.tcsh or omap3430_env.tcsh must be run:'
	@echo 'source omap2430_env.tcsh'
	@echo 'or'
	@echo 'source omap3430_env.tcsh'
	@echo
	@echo Default Compilation:
	@echo ====================
	@echo HOST_PLATFORM = $(HOST_PLATFORM)  // omap2430 or omap3430
	@echo DEBUG = $(DEBUG) 			// Include debug prints and statistics
	@echo INTR = $(INTR) 
	@echo WSPI = $(WSPI) 
	@echo XCC  = $(XCC) 
	@echo SUPPL  = $(SUPPL) 		// WPA or DEVICESCAPE
	@echo FPGA1273 = $(FPGA1273)		// FPGA device support
	@echo FULL_ASYNC = $(FULL_ASYNC)     	// Use only Asynchronous bus transactions
	@echo USE_IRQ_ACTIVE_HIGH = $(USE_IRQ_ACTIVE_HIGH)      // Use IRQ polarity high
	@echo TEST = $(TEST)		     	// Build also the bus test-driver
	@echo BMTRACE = $(BMTRACE)		// Build also the bmtrace performance tool
	@echo TARGET  = $(TARGET)
	@echo ETH_SUPPORT = $(ETH_SUPPORT)
	@echo
	@echo EXTRA CFLAGS  = $(EXTRA_CFLAGS)
	@echo
#
# Create the images
#
.PHONY: suppl.img voice.img apps.img apps.tar $(BUS_DRV)
MODULES_LIST = tiwlan_drv.ko
TAR_FILE = $(HOST_PLATFORM)Binaries.tar

apps.tar: apps
ifeq ($(STRIP),y)
	@echo stripping...
	cd $(OUTPUT_DIR) && $(CROSS_COMPILE)strip -g --strip-unneeded $(MODULES_LIST)
#	cd $(DK_ROOT)/external_drivers/$(HOST_PLATFORM)/Linux/$(BUS_DRV) && $(CROSS_COMPILE)strip -g $(BUS_DRIVER_MODULE)
endif



apps.img: apps
	mkcramfs apps apps.cram
	mkimgti -t data -o apps.img apps.cram

apps: $(OUTPUT_FILE)
#	rm -f $(OUTPUT_DIR)/$(BUS_DRV_REMOVE).ko $(OUTPUT_DIR)/$(BUS_DRV_REMOVE)_test
#	cp -f $(DK_ROOT)/external_drivers/$(HOST_PLATFORM)/Linux/$(BUS_DRV)/$(BUS_DRIVER_MODULE) $(OUTPUT_DIR)

voice.img: 
	$(MAKEJFFS2) -l -r voice -e 0x10000 -o voice.jffs2
	mkimgti -t data -o voice.img voice.jffs2

suppl.img:
ifeq ($(BUILD_SUPPL),y) 
	cp -f $(SUPPLICANT) .
endif
	mkcramfs suppl suppl.cram
	mkimgti -t data -o suppl.img suppl.cram

#
# Recursively cleans the driver, OS, bus and CLI files
#
.PHONY: clean
clean:
	$(MAKE) -C $(DRIVER_LIB_DIR) CROSS_COMPILE=$(CROSS_COMPILE) DEBUG=$(DEBUG) BUILD_SUPPL=$(BUILD_SUPPL) SUPPL=$(SUPPL) WSPI=$(WSPI) INTR=$(INTR) XCC=$(XCC) INFO=$(INFO) STATIC_LIB=$(STATIC_LIB) clean
	$(MAKE) -C $(OS_COMMON_DIR) CROSS_COMPILE=$(CROSS_COMPILE) DEBUG=$(DEBUG) BUILD_SUPPL=$(BUILD_SUPPL) SUPPL=$(SUPPL) WSPI=$(WSPI) INTR=$(INTR) XCC=$(XCC) INFO=$(INFO) STATIC_LIB=$(STATIC_LIB) clean
	$(MAKE) -C $(OS_LINUX_DIR) CROSS_COMPILE=$(CROSS_COMPILE) DEBUG=$(DEBUG) BUILD_SUPPL=$(BUILD_SUPPL) SUPPL=$(SUPPL) WSPI=$(WSPI) INTR=$(INTR) XCC=$(XCC) INFO=$(INFO) STATIC_LIB=$(STATIC_LIB) clean
#	$(MAKE) -C $(DK_ROOT)/external_drivers/$(HOST_PLATFORM)/Linux/$(BUS_DRV) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) KERNEL_DIR=$(KERNEL_DIR) OUTPUT_DIR=$(shell pwd)/$(OUTPUT_DIR) clean
#	$(MAKE) -C $(WLAN_CUDK_DIR) CROSS_COMPILE=$(CROSS_COMPILE) BUILD_SUPPL=$(BUILD_SUPPL) SUPPL=$(SUPPL) XCC=$(XCC) clean
#       $(MAKE) -C $(WLAN_CU_CLI_DIR) CROSS_COMPILE=$(CROSS_COMPILE) BUILD_SUPPL=$(BUILD_SUPPL) SUPPL=$(SUPPL) XCC=$(XCC) cleanall
#       $(MAKE) -C $(WLAN_LOADER_DIR) CROSS_COMPILE=$(CROSS_COMPILE) BUILD_SUPPL=$(BUILD_SUPPL) SUPPL=$(SUPPL) XCC=$(XCC) clean
#       $(MAKE) -C $(LINUX_WIRELESS_TOOLS_DIR) CROSS_COMPILE=$(CROSS_COMPILE) BUILD_SUPPL=$(BUILD_SUPPL) SUPPL=$(SUPPL) XCC=$(XCC) clean
#       $(MAKE) -C $(DBG_MDL) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) KERNEL_DIR=$(KERNEL_DIR) clean

#	@rm -f suppl.* apps.* voice.*
#	@rm -f *spi*.ko sdio.ko
#	@rm -f $(OUTPUT_DIR)/wlan_cu
	@rm -f $(OUTPUT_DIR)/tiwlan_drv.ko 
#	@rm -f wpa_supplicant 
#	@rm -f $(FW_IMAGE_DEST_FILE)
#	@rm -rf *.o *.a \.*.o.cmd *~ *.~* core .depend dep

#       @rm -f tiwlan_drv.ko $(OUTPUT_DIR)/gwsi_drv.o $(OUTPUT_DIR)/debug_tools.o
#       @rm -rf $(OUTPUT_DIR)/iwconfig apps/iwlist $(OUTPUT_DIR)/iwevent

# in order to remove all .*.o.cmd 
	@find ../../../. -type f -print | grep .o.cmd | xargs rm -f
# in order to remove all *.order and *.symvers
	@find ../../../. -type f -print | grep .order | xargs rm -f
	@find ../../../. -type f -print | grep .symvers | xargs rm -f
# in order to remove Module.markers file
	@find ../../../. -type f -print | grep Module.markers | xargs rm -f
# in order to remove the binaries tar
#	@rm -rf $(TAR_FILE)


#
# Verifies that all necessary packages are present.
#
.PHONY: verifypackages
verifypackages:
ifeq ($(strip $(HAVE_GPL)),)
	@echo "*** The GPL package does not seem to be present. You will need both the"
	@echo "*** GPL package and the Non GPL package to execute this makefile."
	exit 1
endif

ifeq ($(strip $(HAVE_NONGPL)),)
	@echo "*** The Non GPL package does not seem to be present. You will need both the"
	@echo "*** GPL package and the Non GPL package to execute this makefile."
	exit 1
endif


# Builds the WSPI or SDIO driver
#
$(BUS_DRV):
	$(MAKE) -C $(DK_ROOT)/external_drivers/$(HOST_PLATFORM)/Linux/$(BUS_DRV) ARCH=arm CROSS_COMPILE=$(CROSS_COMPILE) KERNEL_DIR=$(KERNEL_DIR) OUTPUT_DIR=$(shell pwd)/$(OUTPUT_DIR) all


#
# Causes the driver and the configuration utility object files to get rebuilt
#
.depend:
	rm -f $(OUTPUT_FILE) $(DRIVER_LIB) $(OS_COMMON) $(OS_LINUX) $(WLAN_CU_CLI) $(WLAN_LOADER) $(WLAN_LOGGER)


#
# Build the configuration utility; optionally rebuilds the supplicant
#
#$(SUPPLICANT): $(WLAN_CU_CLI)

.PHONY: CU_DK
CU_DK:
#	$(MAKE) -C $(WLAN_CUDK_DIR) CROSS_COMPILE=$(CROSS_COMPILE) DEBUG=$(DEBUG) BUILD_SUPPL=$(BUILD_SUPPL) SUPPL=$(SUPPL) XCC=$(XCC) STATIC_LIB=$(STATIC_LIB) SG=$(SG) ETH_SUPPORT=$(ETH_SUPPORT)

#$(WLAN_CU_CLI):
#        $(MAKE) -C $(WLAN_CU_CLI_DIR) CROSS_COMPILE=$(CROSS_COMPILE) DEBUG=$(DEBUG) BUILD_SUPPL=$(BUILD_SUPPL) SUPPL=$(SUPPL) XCC=$(XCC) STATIC_LIB=$(STATIC_LIB) SG=$(SG)

#$(LINUX_WIRELESS_TOOLS):
#       $(MAKE) -C $(LINUX_WIRELESS_TOOLS_DIR) CROSS_COMPILE=$(CROSS_COMPILE)

#$(WLAN_LOADER):
#        $(MAKE) -C $(WLAN_LOADER_DIR) CROSS_COMPILE=$(CROSS_COMPILE) DEBUG=$(DEBUG) XCC=$(XCC) STATIC_LIB=$(STATIC_LIB) SG=$(SG)


#
# Recursively builds the driver lib file
#
$(DRIVER_LIB):
	$(MAKE) -C $(DRIVER_LIB_DIR) CROSS_COMPILE=$(CROSS_COMPILE) DEBUG=$(DEBUG) BUILD_SUPPL=$(BUILD_SUPPL) SUPPL=$(SUPPL) WSPI=$(WSPI) INTR=$(INTR) XCC=$(XCC) INFO=$(INFO) STATIC_LIB=$(STATIC_LIB)


#
# Recursively builds the OS lib file
#
$(OS_COMMON):
	$(MAKE) -C $(OS_COMMON_DIR) CROSS_COMPILE=$(CROSS_COMPILE) DEBUG=$(DEBUG) BUILD_SUPPL=$(BUILD_SUPPL) SUPPL=$(SUPPL) WSPI=$(WSPI) INTR=$(INTR) XCC=$(XCC) INFO=$(INFO) STATIC_LIB=$(STATIC_LIB) OS_AUXILIARY_LIBS+=../../$(DRIVER_LIB)


#
# Recursively builds the linux OS stub object file
#
$(OS_LINUX):
	$(MAKE) -C $(OS_LINUX_DIR) CROSS_COMPILE=$(CROSS_COMPILE) DEBUG=$(DEBUG) BUILD_SUPPL=$(BUILD_SUPPL) SUPPL=$(SUPPL) WSPI=$(WSPI) INTR=$(INTR) XCC=$(XCC) INFO=$(INFO) STATIC_LIB=$(STATIC_LIB) OS_AUXILIARY_LIBS+=../$(DRIVER_LIB) OS_AUXILIARY_LIBS+=../$(OS_COMMON)


#
# Recursively builds the driver object file
#
$(OUTPUT_FILE): $(DRIVER_LIB) $(OS_COMMON) $(OS_LINUX)
	mv $(OS_LINUX) $(OUTPUT_FILE)

.PHONY: link
link:
	$(MAKE) -C $(KERNEL_DIR) M=`pwd` ARCH=arm CROSS_COMPILE=$(CROSS_COMPILE) modules
	mv tiwlan_drv.ko $(OUTPUT_FILE)


#
# Builds the debug module object file
#
#$(DBG_MDL)/debug_module.ko:
#       $(MAKE) -C $(DBG_MDL) KERNEL_DIR=$(KERNEL_DIR) 
#       mv $(DBG_MDL)/debug_module.ko $(OUTPUT_DIR)/debug_tools.o


#
# Not in use?
#
TAGS: $(SRCS)
	{ find ${DK_ROOT}/common -name '*.h' -print ; \
	  find . -name '*.h' -print ; } | etags -
	  etags -a $(SRCS)

CTAGS: $(SRCS)
	find ${DK_ROOT} -iname '*.[ch]' -print | ctags --c++-types=+px --excmd=pattern -L -


#endif  # ifneq ($(KERNELRELEASE),)
