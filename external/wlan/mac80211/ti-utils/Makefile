CC = $(CROSS_COMPILE)gcc
CFLAGS = -O2 -Wall
CFLAGS += -DCONFIG_LIBNL20 -I$(NFSROOT)/usr/include -I$(NFSROOT)/include

LDFLAGS += -L$(NFSROOT)/lib
LIBS += -lnl -lnl-genl -lm

OBJS = nvs.o misc_cmds.o calibrator.o plt.o wl18xx_plt.o ini.o

%.o: %.c calibrator.h nl80211.h plt.h nvs_dual_band.h
	$(CC) $(CFLAGS) -c -o $@ $<

all: $(OBJS) 
	$(CC) $(LDFLAGS) $(OBJS) $(LIBS) -o calibrator

uim:
	$(CC) $(CFLAGS) $(LDFLAGS) uim_rfkill/$@.c -o $@

static: $(OBJS) 
	$(CC) $(LDFLAGS) --static $(OBJS) $(LIBS) -o calibrator

install:
	@echo Copy files to $(NFSROOT)/home/root
	@cp -f ./calibrator $(NFSROOT)/home/root
	@chmod 755 $(NFSROOT)/home/root/calibrator
	@cp -f ./scripts/wl12xx-tool.sh $(NFSROOT)/home/root
	@chmod 755 $(NFSROOT)/home/root/wl12xx-tool.sh

clean:
	@rm -f *.o calibrator uim
