#
# Makefile for sensor chip drivers.
#

obj-$(CONFIG_THERMAL)		+= thermal_sys.o
obj-$(CONFIG_SPEAR_THERMAL)		+= spear_thermal.o
obj-$(CONFIG_OMAP_BANDGAP)	+= omap-thermal.o
obj-$(CONFIG_OMAP4_BG_TEMP_SENSOR_DATA)	+= omap4-bg-sen-data.o
ifdef CONFIG_ARCH_OMAP5_ES1
obj-$(CONFIG_OMAP5_BG_TEMP_SENSOR_DATA)	+= omap5-bg-sen-data_es1.o
else
obj-$(CONFIG_OMAP5_BG_TEMP_SENSOR_DATA)	+= omap5-bg-sen-data.o
endif
omap-thermal-y			:= omap-bandgap.o
