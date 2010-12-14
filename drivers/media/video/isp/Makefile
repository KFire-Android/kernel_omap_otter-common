# Makefile for OMAP3 ISP driver

isp-mod-objs += \
	isp.o ispccdc.o \
	isppreview.o ispresizer.o isph3a.o isphist.o isp_af.o ispcsi2.o \
	ispstat.o isp_csi.o isp_csi_memvs.o ispdss.o

obj-$(CONFIG_VIDEO_OMAP34XX_ISP_PREVIEWER) += \
	omap_previewer.o

obj-$(CONFIG_VIDEO_OMAP34XX_ISP_RESIZER) += \
	omap_resizer.o

obj-$(CONFIG_VIDEO_OMAP34XX_ISP_PREV2RESZ) += \
	omap_prev2resz.o

obj-$(CONFIG_VIDEO_OMAP34XX_ISP_DEBUG_FS) += \
	isp_dfs.o \
	ispccdc_dfs.o \
	isppreview_dfs.o \
	ispresizer_dfs.o \
	isph3a_dfs.o \
	ispcsi1_dfs.o \
	ispcsi2_dfs.o \
	ispaf_dfs.o

obj-$(CONFIG_VIDEO_OMAP3) += isp-mod.o
