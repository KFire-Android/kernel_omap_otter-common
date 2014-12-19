#
# OMAP Remote Procedure Call Driver.
#
# Copyright(c) 2012 Texas Instruments. All rights reserved.
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License version 2 as published by
# the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
# more details.
#
# You should have received a copy of the GNU General Public License along with
# this program.  If not, see <http://www.gnu.org/licenses/>.

obj-$(CONFIG_RPC_OMAP) += omaprpc.o
omaprpc-y :=  omap_rpc.o \
              omap_rpc_tiler.o \
              omap_rpc_rproc.o

ifeq ($(CONFIG_ION_OMAP),y)
omaprpc-y += omap_rpc_ion.o
else
ifeq ($(CONFIG_DMA_SHARED_BUFFER),y)
omaprpc-y += omap_rpc_dmabuf.o
endif
endif
