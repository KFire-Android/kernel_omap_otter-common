#
# Makefile for the linux kernel.
#

# Common support
obj-y := common.o sram.o clock.o devices.o dma.o mux.o gpio.o \
	 usb.o fb.o io.o timer-32k-sync.o hdmi_lib.o
obj-m :=
obj-n :=
obj-  :=

# OCPI interconnect support for 1710, 1610 and 5912
obj-$(CONFIG_ARCH_OMAP16XX) += ocpi.o

# OPP support in (OMAP3+ only at the moment)
ifdef CONFIG_PM
obj-$(CONFIG_ARCH_OMAP3) += opp.o
obj-$(CONFIG_ARCH_OMAP4) += opp.o
obj-$(CONFIG_TWL4030_CORE) += opp_twl_tps.o
endif

# omap_device support (OMAP2+ only at the moment)
obj-$(CONFIG_ARCH_OMAP2) += omap_device.o powerdomain.o
obj-$(CONFIG_ARCH_OMAP3) += omap_device.o powerdomain.o
obj-$(CONFIG_ARCH_OMAP4) += omap_device.o powerdomain.o

obj-$(CONFIG_OMAP_MCBSP) += mcbsp.o
obj-$(CONFIG_OMAP_IOMMU) += iommu.o iovmm.o
obj-$(CONFIG_OMAP_IOMMU_DEBUG) += iommu-debug.o

obj-$(CONFIG_OMAP_USER_DMM) += dmm_user.o iodmm.o

obj-$(CONFIG_CPU_FREQ) += cpu-omap.o
obj-$(CONFIG_OMAP_DM_TIMER) += dmtimer.o
obj-$(CONFIG_OMAP_DEBUG_DEVICES) += debug-devices.o
obj-$(CONFIG_OMAP_DEBUG_LEDS) += debug-leds.o
i2c-omap-$(CONFIG_I2C_OMAP) := i2c.o
obj-y += $(i2c-omap-m) $(i2c-omap-y)

# OMAP mailbox framework
obj-$(CONFIG_OMAP_MBOX_FWK) += mailbox.o
obj-$(CONFIG_OMAP_REMOTE_PROC) += remoteproc.o

obj-$(CONFIG_OMAP_PM_NOOP) += omap-pm-noop.o
obj-$(CONFIG_OMAP_PM) += omap-pm.o

obj-$(CONFIG_ARCH_OMAP4) += hwspinlock.o

# HDMI audio WA for OMAP4430 ES2.0,1,2 to use Notify module
ifdef CONFIG_OMAP_HDMI_AUDIO_WA
ccflags-y += -Iarch/arm/plat-omap/include/syslink
endif
