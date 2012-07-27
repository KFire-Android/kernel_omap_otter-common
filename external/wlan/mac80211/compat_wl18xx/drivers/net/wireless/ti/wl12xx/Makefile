wl12xx-objs	= main.o cmd.o acx.o debugfs.o

define filechk_version.h
	(echo 'static const char *wl12xx_git_head = \
			 "$(shell git describe --dirty)";')
endef

$(obj)/version.h: .git/HEAD .git/index .git/refs/tags
	@$(call filechk,version.h)

$(obj)/main.c: $(src)/version.h

obj-$(CONFIG_WL12XX)		+= wl12xx.o
