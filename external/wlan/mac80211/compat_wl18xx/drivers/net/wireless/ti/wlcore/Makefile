wlcore-objs		= main.o cmd.o io.o event.o tx.o rx.o ps.o acx.o \
			  boot.o init.o debugfs.o scan.o

define filechk_version.h
	(echo 'static const char *wlcore_timestamp = __TIMESTAMP__;'; \
	echo 'static const char *wlcore_git_head = \
			"$(shell git describe --dirty)";')
endef

$(obj)/version.h: .git/HEAD .git/index .git/refs/tags
	@$(call filechk,version.h)

$(obj)/main.c: $(src)/version.h

wlcore_spi-objs 	= spi.o
wlcore_sdio-objs	= sdio.o

wlcore-$(CONFIG_NL80211_TESTMODE)	+= testmode.o
obj-$(CONFIG_WLCORE)			+= wlcore.o
obj-$(CONFIG_WLCORE_SPI)		+= wlcore_spi.o
obj-$(CONFIG_WLCORE_SDIO)		+= wlcore_sdio.o

# small builtin driver bit
obj-$(CONFIG_WL12XX_PLATFORM_DATA)	+= wl12xx_platform_data.o
