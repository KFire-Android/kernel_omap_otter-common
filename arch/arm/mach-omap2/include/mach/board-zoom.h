/*
 * Defines for zoom boards
 */
#include <plat/display.h>

extern int __init zoom_debugboard_init(void);
extern void __init zoom_peripherals_init(void);
extern void __init zoom_display_init(enum omap_dss_venc_type venc_type);

#define ZOOM2_HEADSET_EXTMUTE_GPIO	153
