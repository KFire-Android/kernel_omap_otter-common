
obj-$(CONFIG_RPC_OMAP) += omaprpc.o
omaprpc-y :=  omap_rpc.o \
              omap_rpc_tiler.o \
              omap_rpc_rproc.o

ifeq ($(CONFIG_ION_OMAP),y)
omaprpc-y += omap_rpc_ion.o
endif

ifeq ($(CONFIG_DMA_SHARED_BUFFER),y)
omaprpc-y += omap_rpc_dmabuf.o
endif




