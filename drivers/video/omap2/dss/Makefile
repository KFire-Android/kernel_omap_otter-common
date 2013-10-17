obj-$(CONFIG_OMAP2_DSS) += omapdss.o
# Core DSS files
omapdss-y := core.o dss.o dss_dpll.o dss_features.o dispc.o dispc_coefs.o \
	display.o output.o
# DSS compat layer files
omapdss-y += manager.o manager-sysfs.o overlay.o overlay-sysfs.o apply.o \
	dispc-compat.o display-sysfs.o wb.o
omapdss-$(CONFIG_OMAP2_DSS_DPI) += dpi.o dpi_common.o
omapdss-$(CONFIG_OMAP2_DSS_DRA7XX_DPI) += dra7xx_dpi.o dpi_common.o
omapdss-$(CONFIG_OMAP2_DSS_RFBI) += rfbi.o
omapdss-$(CONFIG_OMAP2_DSS_VENC) += venc.o venc_panel.o
omapdss-$(CONFIG_OMAP2_DSS_SDI) += sdi.o
omapdss-$(CONFIG_OMAP2_DSS_DSI) += dsi.o
omapdss-$(CONFIG_OMAP4_DSS_HDMI) += hdmi.o \
				    hdmi_panel.o ti_hdmi_4xxx_ip.o
omapdss-$(CONFIG_OMAP4_HDCP_SUPPORT) += hdcp_top.o hdcp_lib.o hdcp_ddc.o
omapdss-$(CONFIG_OMAP5_DSS_HDMI) += hdmi.o \
				hdmi_panel.o ti_hdmi_4xxx_ip.o ti_hdmi_5xxx_ip.o \
                                hdcp_top.o
ccflags-$(CONFIG_OMAP2_DSS_DEBUG) += -DDEBUG
ccflags-y += -Iarch/arm
