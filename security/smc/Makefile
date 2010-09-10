#
# Copyright (c) 2006-2007 Trusted Logic S.A.
# All Rights Reserved.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation; either version 2 of
# the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston,
# MA 02111-1307 USA
#

ifdef S_VERSION_BUILD
EXTRA_CFLAGS += -DS_VERSION_BUILD=$(S_VERSION_BUILD)
endif

#EXTRA_CFLAGS += -DDEBUG

smc_driver-objs += scxlnx_util.o
smc_driver-objs += scxlnx_conn.o
smc_driver-objs += scxlnx_device.o
smc_driver-objs += scxlnx_sm_comm.o
smc_driver-objs += scx_public_crypto.o
smc_driver-objs += scx_public_crypto_Digest.o
smc_driver-objs += scx_public_crypto_AES.o
smc_driver-objs += scx_public_crypto_DES.o
smc_driver-objs += scx_public_dma.o
smc_driver-objs += bridge_pub2sec.o

obj-$(CONFIG_SECURITY_MIDDLEWARE_COMPONENT) += smc_driver.o
