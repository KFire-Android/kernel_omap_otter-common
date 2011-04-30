ccflags-$(CONFIG_CAIF_DEBUG) := -DDEBUG

# Serial interface
ifeq ($(CONFIG_CAIF_TTY_PM), y)
caif_serial_pm-objs := caif_serial.o
caif_serial_pm-objs += phy/omap_pm.o
obj-$(CONFIG_CAIF_TTY) += caif_serial_pm.o
ccflags-y += -DGPIO_MODEM_AWR=$(CONFIG_CAIF_PHY_GPIO_AWR)
ccflags-y += -DGPIO_MODEM_CWR=$(CONFIG_CAIF_PHY_GPIO_CWR)
else
obj-$(CONFIG_CAIF_TTY) += caif_serial.o
endif

# SPI slave physical interfaces module
cfspi_slave-objs := caif_spi.o caif_spi_slave.o
obj-$(CONFIG_CAIF_SPI_SLAVE) += cfspi_slave.o

# Shared memory
caif_shm-objs := caif_shmcore.o caif_shm_u5500.o
obj-$(CONFIG_CAIF_SHM) += caif_shm.o

ccflags-$(CONFIG_WAKELOCK) += -DCONFIG_WAKELOCK

# HSI interface
obj-$(CONFIG_CAIF_HSI) += caif_hsi.o
