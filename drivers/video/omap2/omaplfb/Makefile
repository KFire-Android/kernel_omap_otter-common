obj-$(CONFIG_OMAPLFB) := omaplfb.o

ccflags-y += \
	-I$(KERNELDIR)/drivers/video/omap2 \
	-I$(KERNELDIR)/arch/arm/plat-omap/include

ccflags-y += \
	-DLINUX \
	-D__linux__ \
	-D__KERNEL__ \
	-DPVR_LINUX_USING_WORKQUEUES

omaplfb-y += \
	omaplfb_linux.o \
	omaplfb_displayclass.o

omaplfb-$(CONFIG_GCBV) += \
	omaplfb_bv.o
