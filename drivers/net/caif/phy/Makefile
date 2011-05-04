ccflags-y := $(CAIF_FLAGS)
ccflags-m := $(CAIF_FLAGS)

clean-dirs:= .tmp_versions
clean-files:= Module.symvers modules.order *.cmd *~ *.o *.ko *.mod.c

# CAIF OMAP SPI device
obj-$(CONFIG_CAIF_PHY_SPI) += omap2_spi.o

# CAIF OMAP HSI device
obj-$(CONFIG_CAIF_HSI_OMAP) += hsi_omap.o
