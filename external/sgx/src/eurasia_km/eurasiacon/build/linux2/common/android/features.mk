########################################################################### ###
#@Copyright     Copyright (c) Imagination Technologies Ltd. All Rights Reserved
#@License       Dual MIT/GPLv2
# 
# The contents of this file are subject to the MIT license as set out below.
# 
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
# 
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
# 
# Alternatively, the contents of this file may be used under the terms of
# the GNU General Public License Version 2 ("GPL") in which case the provisions
# of GPL are applicable instead of those above.
# 
# If you wish to allow use of your version of this file only under the terms of
# GPL, and not to allow others to use your version of this file under the terms
# of the MIT license, indicate your decision by deleting the provisions above
# and replace them with the notice and other provisions required by GPL as set
# out in the file called "GPL-COPYING" included in this distribution. If you do
# not delete the provisions above, a recipient may use your version of this file
# under the terms of either the MIT license or GPL.
# 
# This License is also included in this distribution in the file called
# "MIT-COPYING".
# 
# EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
# PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
# BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
# PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
# COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
# IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
# CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
#   
### ###########################################################################

include ../common/android/platform_version.mk

# Basic support option tuning for Android
#
SUPPORT_ANDROID_PLATFORM := 1
SUPPORT_OPENGLES1_V1_ONLY := 1
SUPPORT_OPENVG := 0
SUPPORT_OPENGL := 0

# Meminfo IDs are required for buffer stamps
#
SUPPORT_MEMINFO_IDS := 1

# Need multi-process support in PDUMP
#
SUPPORT_PDUMP_MULTI_PROCESS := 1

# Always print debugging after 5 seconds of no activity
#
CLIENT_DRIVER_DEFAULT_WAIT_RETRIES := 50

# Android WSEGL is always the same
#
OPK_DEFAULT := libpvrANDROID_WSEGL.so

# srvkm is always built, but bufferclass_example is only built
# before EGL_image_external was generally available.
#
KERNEL_COMPONENTS := srvkm
ifeq ($(is_at_least_honeycomb),0)
#KERNEL_COMPONENTS += bufferclass_example
endif

# Kernel modules are always installed here under Android
#
PVRSRV_MODULE_BASEDIR := /system/modules/

# Use the new PVR_DPF implementation to allow lower message levels
# to be stripped from production drivers
#
PVRSRV_NEW_PVR_DPF := 1

# Production Android builds don't want PVRSRVGetDCSystemBuffer
#
SUPPORT_PVRSRV_GET_DC_SYSTEM_BUFFER := 0

# Prefer to limit the 3D parameters heap to <16MB and move the
# extra 48MB to the general heap. This only affects cores with
# 28bit MMUs (520, 530, 531, 540).
#
SUPPORT_LARGE_GENERAL_HEAP := 1

##############################################################################
# EGL connect/disconnect hooks only available since Froyo
# Obsolete in future versions
#
ifeq ($(is_at_least_froyo),1)
ifeq ($(is_at_least_icecream_sandwich),0)
PVR_ANDROID_HAS_CONNECT_DISCONNECT := 1
endif
endif

##############################################################################
# Override surface field name for older versions
#
ifeq ($(is_at_least_gingerbread),0)
PVR_ANDROID_SURFACE_FIELD_NAME := \"mSurface\"
endif

##############################################################################
# Provide ANativeWindow{Buffer,} typedefs for older versions
#
ifeq ($(is_at_least_gingerbread),0)
PVR_ANDROID_NEEDS_ANATIVEWINDOW_TYPEDEF := 1
endif
ifeq ($(is_at_least_icecream_sandwich),0)
PVR_ANDROID_NEEDS_ANATIVEWINDOWBUFFER_TYPEDEF := 1
endif

##############################################################################
# Handle various platform includes for unittests
#
UNITTEST_INCLUDES := eurasiacon/android

ifeq ($(is_at_least_gingerbread),1)
UNITTEST_INCLUDES += $(ANDROID_ROOT)/frameworks/base/native/include
endif

ifeq ($(is_future_version),1)
UNITTEST_INCLUDES += \
 $(ANDROID_ROOT)/frameworks/native/include \
 $(ANDROID_ROOT)/frameworks/native/opengl/include \
 $(ANDROID_ROOT)/libnativehelper/include
# FIXME: This is the old location for the JNI header.
UNITTEST_INCLUDES += $(ANDROID_ROOT)/dalvik/libnativehelper/include
else
UNITTEST_INCLUDES += \
 $(ANDROID_ROOT)/frameworks/base/opengl/include \
 $(ANDROID_ROOT)/dalvik/libnativehelper/include
endif

# But it doesn't have OpenVG headers
#
UNITTEST_INCLUDES += eurasiacon/unittests/include

##############################################################################
# Future versions moved proprietary libraries to a vendor directory
#
ifeq ($(is_at_least_gingerbread),1)
SHLIB_DESTDIR := /system/vendor/lib
DEMO_DESTDIR := /system/vendor/bin
else
SHLIB_DESTDIR := /system/lib
DEMO_DESTDIR := /system/bin
endif

# EGL libraries go in a special place
#
EGL_DESTDIR := $(SHLIB_DESTDIR)/egl

##############################################################################
# We can support OpenCL in the build since Froyo (stlport was added in 2.2)
#
ifeq ($(is_at_least_froyo),1)
SYS_CXXFLAGS := \
 -fuse-cxa-atexit \
 $(SYS_CFLAGS) \
 -I$(ANDROID_ROOT)/bionic \
 -I$(ANDROID_ROOT)/external/stlport/stlport
else
SYS_CXXFLAGS := \
 $(SYS_CFLAGS) \
 -I$(ANDROID_ROOT)/bionic/libstdc++/include
endif

##############################################################################
# Composition bypass feature, supported since Froyo.
# In ICS, hardware composer (HWC) should be used instead.
#
ifeq ($(is_at_least_froyo),1)
ifeq ($(is_at_least_honeycomb),0)
PVR_ANDROID_HAS_NATIVE_BUFFER_TRANSFORM := 1
SUPPORT_ANDROID_COMPOSITION_BYPASS := 1
endif
endif

##############################################################################
# In ICS, we have hardware composer (HWC) support.
#
# SUPPORT_ANDROID_COMPOSER_HAL adds Post2() to the framebuffer HAL interface
# and is intended for inter-op with external HWC modules. It is always
# enabled (but we allow it to be compiled out just in case).
#
# SUPPORT_ANDROID_COMPOSITION_BYPASS adds a new buffer type (client buffers
# allocated from the framebuffer pool) which maximizes compatibility with
# most 3rdparty display controllers. It is orthogonal to HWC support.
#
ifeq ($(is_at_least_honeycomb),1)
SUPPORT_ANDROID_COMPOSER_HAL := 1
endif

##############################################################################
# We have some extra GRALLOC_USAGE bits we need to handle in ICS
#
ifeq ($(is_at_least_honeycomb),1)
PVR_ANDROID_HAS_GRALLOC_USAGE_EXTERNAL_DISP := 1
PVR_ANDROID_HAS_GRALLOC_USAGE_PROTECTED := 1
PVR_ANDROID_HAS_GRALLOC_USAGE_PRIVATE := 1
endif

##############################################################################
# Support the new OES_EGL_image_external extension + YV12 buffers
#
ifeq ($(is_at_least_honeycomb),1)
PVR_ANDROID_HAS_HAL_PIXEL_FORMAT_YV12 := 1
GLES1_EXTENSION_EGL_IMAGE_EXTERNAL := 1
GLES2_EXTENSION_EGL_IMAGE_EXTERNAL := 1
endif

##############################################################################
# Gingerbread adds the native window cancelBuffer operation
#
ifeq ($(is_at_least_gingerbread),1)
PVR_ANDROID_HAS_CANCELBUFFER := 1
endif

##############################################################################
# Versions prior to ICS have another header we must include
#
ifeq ($(is_at_least_icecream_sandwich),0)
PVR_ANDROID_HAS_ANDROID_NATIVE_BUFFER_H := 1
endif

##############################################################################
# ICS added dump() hook to gralloc alloc_device_t API
#
ifeq ($(is_at_least_honeycomb),1)
PVR_ANDROID_HAS_GRALLOC_DUMP := 1
endif

##############################################################################
# ICS added support for the BGRX pixel format, and allows drivers to advertise
# configs in this format instead of RGBX.
#
# The DDK provides a private definition of HAL_PIXEL_FORMAT_BGRX_8888. This
# option exposes it as the native visual for 8888 configs with alpha ignored
#
ifeq ($(is_at_least_icecream_sandwich),1)
SUPPORT_HAL_PIXEL_FORMAT_BGRX := 1
endif

##############################################################################
# ICS added the ability for GL clients to pre-rotate their rendering to the
# orientation desired by the compositor. The SGX DDK can use TRANSFORM_HINT
# to access this functionality.
#
# This is required by some HWC implementations that cannot use the display
# to rotate buffers, otherwise the HWC optimization cannot be used when
# rotating the device.
#
ifeq ($(is_at_least_icecream_sandwich),1)
PVR_ANDROID_HAS_WINDOW_TRANSFORM_HINT := 1
endif

##############################################################################
# ICS requires that at least one driver EGLConfig advertises the
# EGL_RECORDABLE_ANDROID attribute. The platform requires that surfaces
# rendered with this config can be consumed by an OMX video encoder.
#
ifeq ($(is_at_least_icecream_sandwich),1)
EGL_EXTENSION_ANDROID_RECORDABLE := 1
endif

##############################################################################
# ICS added a new usage bit. USAGE_HW_COMPOSER indicates that a buffer might
# be used with HWComposer. In practice this is all non-MM buffers.
#
ifeq ($(is_at_least_icecream_sandwich),1)
PVR_ANDROID_HAS_GRALLOC_USAGE_HW_COMPOSER := 1
endif

##############################################################################
# ICS added the EGL_ANDROID_blob_cache extension. Enable support for this
# extension in EGL/GLESv2.
#
ifeq ($(is_at_least_icecream_sandwich),1)
EGL_EXTENSION_ANDROID_BLOB_CACHE := 1
endif

##############################################################################
# ICS MR1 added a new usage bit. USAGE_HW_VIDEO_ENCODER indicates that a
# buffer might be used with the video encoder.
#
ifeq ($(is_at_least_icecream_sandwich_mr1),1)
PVR_ANDROID_HAS_GRALLOC_USAGE_HW_VIDEO_ENCODER := 1
endif

##############################################################################
# ICS and earlier should rate-limit composition by waiting for 3D renders
# to complete in the compositor's eglSwapBuffers().
#
ifeq ($(is_future_version),0)
PVR_ANDROID_COMPOSITOR_WAIT_FOR_RENDER := 1
endif

# Placeholder for future version handling
#
ifeq ($(is_future_version),1)
-include ../common/android/future_version.mk
endif
