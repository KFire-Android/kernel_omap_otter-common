/*
 * drivers/i2c/chips/SenseTek/stk_defines.h
 *
 * $Id: stk_defines.h,v 1.0 2011/03/05 11:12:08 jsgood Exp $
 *
 * Copyright (C) 2011 Patrick Chang <patrick_chang@sitronix.com.tw>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 *	SenseTek/Sitronix Proximity / Ambient Light / MEMS  Sensor Driver
 */

/*
-r--r--r-- root     root            4 2000-11-05 22:20 distance_range
-r--r--r-- root     root            4 2000-11-05 22:20 lux_range
-r--r--r-- root     root            4 2000-11-05 22:20 distance
-r--r--r-- root     root            4 2000-11-05 22:20 lux
-rw-rw-rw- root     root            1 2000-11-05 22:20 ps_enable
-rw-rw-rw- root     root            1 2000-11-05 22:20 als_enable
-r--r--r-- root     root            1 2000-11-05 22:20 ps_dist_mode
-rw-rw-rw- root     root            4 2000-11-05 22:20 ps_delay
-rw-rw-rw- root     root            4 2000-11-05 22:20 als_delay
-r--r--r-- root     root            4 2000-11-05 22:20 ps_min_delay
-r--r--r-- root     root            4 2000-11-05 22:20 als_min_delay
drwxr-xr-x root     root              2000-11-05 22:16 DBG
*/
#ifndef __STK_DEFINES_H
#define __STK_DEFINES_H

#define ALS_NAME "lightsensor-level"
#define PS_NAME "proximity"

#define STK_ENABLE_SENSOR_USE_BINARY_SYSFS 0

#define ps_enable_bin_path              "/sys/devices/platform/stk-oss/ps_enable_bin"
#define als_enable_bin_path             "/sys/devices/platform/stk-oss/als_enable_bin"
#define ps_distance_mode_bin_path       "/sys/devices/platform/stk-oss/ps_dist_mode_bin"
#define ps_distance_range_bin_path      "/sys/devices/platform/stk-oss/distance_range_bin"
#define als_lux_range_bin_path          "/sys/devices/platform/stk-oss/lux_range_bin"

/* <---- DEPRECATED */
#define ps_min_delay_bin_path			"/sys/devices/platform/stk-oss/ps_min_delay_bin"
#define als_min_delay_bin_path			"/sys/devices/platform/stk-oss/als_min_delay_bin"
#define ps_delay_bin_path				"/sys/devices/platform/stk-oss/ps_delay_bin"
#define als_delay_bin_path				"/sys/devices/platform/stk-oss/als_delay_bin"
/* DEPRECATED ----> */

#define ps_enable_path              "/sys/devices/platform/stk-oss/ps_enable"
#define als_enable_path             "/sys/devices/platform/stk-oss/als_enable"
#define ps_distance_mode_path       "/sys/devices/platform/stk-oss/dist_mode"
#define ps_distance_range_path      "/sys/devices/platform/stk-oss/distance_range"
#define als_lux_range_path          "/sys/devices/platform/stk-oss/lux_range"

#define STK_DRIVER_VER          "1.5.8"

#define EVENT_TYPE_PROXIMITY        ABS_DISTANCE
#define EVENT_TYPE_LIGHT            ABS_MISC

#endif // __STK_DEFINE_H
