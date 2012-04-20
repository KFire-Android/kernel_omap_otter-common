

##
##
## Directory Locations
##
##

COMMON      := $(DK_ROOT)/stad
COMSRC      := $(COMMON)/src
TWD         := $(DK_ROOT)/TWD
UTILS       := $(DK_ROOT)/utils
TESTSRC     := $(DK_ROOT)/Test
LINUX_PFORM := $(DK_ROOT)/platforms/os/linux
PLATFORMS   := $(DK_ROOT)/platforms
EXT_DRV     := $(DK_ROOT)/external_drivers
TXN         := $(DK_ROOT)/Txn

##
##
## Make Flags
##
##

#
# Compile with FW1273 or FW1251
#
FW ?= 1273

#
# Compile with XCC support or not
#
XCC ?= n


#
# Compile with GEM support or not
#
GEM ?= n

#
# Choose the bus type (n for SDIO, y for WSPI)
#
WSPI ?= n

#
# Use periodic interrupt
#
INTR ?= y

#
# Enable or disable debugging
#
DEBUG ?= y
KERNEL_DEBUGGER ?= n
STRIP ?= y
BMTRACE ?= n
#
# Enable or disable OS and init messages
#
INFO ?= n

#
# 1273 FPGA platform
#
FPGA1273 ?= n

#
# Enable trace buffer
#
TI_TRACE_BUFFER ?= n

##
##
## Driver Compilation Directives
##
##
ifeq "$(HOST_PLATFORM)" "omap2430"	
	DK_DEFINES += -D HOST_PLATFORM_OMAP2430
endif
ifeq "$(HOST_PLATFORM)" "omap3430"
	DK_DEFINES += -D HOST_PLATFORM_OMAP3430
endif
ifeq "$(HOST_PLATFORM)" "zoom2"
	DK_DEFINES += -D HOST_PLATFORM_ZOOM2
endif
ifeq "$(HOST_PLATFORM)" "zoom1"
	DK_DEFINES += -D HOST_PLATFORM_ZOOM1
endif

ifeq ($(XCC),y)
    DK_DEFINES += -D XCC_MODULE_INCLUDED
endif

ifeq ($(GEM),y)
    DK_DEFINES += -D GEM_SUPPORTED
endif

ifeq ($(INTR),n)
    DK_DEFINES += -D PRIODIC_INTERRUPT
endif    

ifeq ($(DEBUG),y)
    DK_DEFINES += -D TI_DBG
    DK_DEFINES += -D REPORT_LOG
    DK_DEFINES += -O2
    DK_DEFINES += -D PRINTF_ROLLBACK
else
    DK_DEFINES += -D TI_DBG
    DK_DEFINES += -U REPORT_LOG
    DK_DEFINES += -O2
endif
ifeq ($(KERNEL_DEBUGGER),y)
    DK_DEFINES += -g -O2
	STRIP = n
endif
export KERNEL_DEBUGGER

DK_DEFINES += -D __BYTE_ORDER_LITTLE_ENDIAN

ifeq ($(TI_TRACE_BUFFER),y)
   DK_DEFINES += -D TI_TRACE_BUF
endif

ifeq ($(BMTRACE),y)
   DK_DEFINES += -D TIWLAN_BMTRACE
endif

##
##
## Platform Compilation Directives
##
##

#
# Bus Driver
#
ifeq ($(WSPI),y)
    BUS_DRV = spi
    BUS_DRV_REMOVE = sdio
    BUS_DRIVER_MODULE = spi.ko
    PFORM_DEFINES += -D WSPI
else
    BUS_DRV = sdio
    BUS_DRV_REMOVE = spi
    BUS_DRV = sdio
    BUS_DRV_REMOVE = spi
    BUS_DRIVER_MODULE = sdio.ko
endif

ifeq ($(INFO),y)
    PFORM_DEFINES += -D OS_INFO_MESSAGES
    PFORM_DEFINES += -D INIT_MESSAGES
endif

PFORM_DEFINES += -D HOST_COMPILE
PFORM_DEFINES += -D FW_RUNNING_AS_STA
PFORM_DEFINES += -D TNETW1273

ifeq ($(FPGA1273),y)
    PFORM_DEFINES += -D FPGA1273_STAGE_
endif

ifeq ($(FULL_ASYNC),y)
    PFORM_DEFINES += -D FULL_ASYNC_MODE
endif

ifeq ($(USE_IRQ_ACTIVE_HIGH),y)
    PFORM_DEFINES += -D USE_IRQ_ACTIVE_HIGH
endif

##
##
## Miscellaneous Compilation Directivcs
##
##

EXTRA_CFLAGS += -fsigned-char
EXTRA_CFLAGS += -D __LINUX__
EXTRA_CFLAGS += $(DK_DEFINES)
EXTRA_CFLAGS += $(PFORM_DEFINES)
