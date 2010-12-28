#
# Makefile for the omap video device drivers.
#

# OMAP2/3/4 Overlay driver
omap-vout-y := omap_vout.o omap_voutlib.o
obj-$(CONFIG_VIDEO_OMAP2_VOUT) += omap-vout.o
# S3D overlay currently only supported on OMAP4
ifeq ($(CONFIG_ARCH_OMAP4),y)
obj-$(CONFIG_VIDEO_OMAP2_VOUT) += omap_s3d_overlay.o omap_wb.o
endif
