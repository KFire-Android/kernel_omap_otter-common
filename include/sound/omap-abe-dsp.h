/*
 * omap-aess  --  OMAP4 ABE DSP
 *
 * Author: Liam Girdwood <lrg@slimlogic.co.uk>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _OMAP4_ABE_DSP_H
#define _OMAP4_ABE_DSP_H

#include <linux/platform_device.h>

struct omap4_abe_dsp_pdata {
	/* aess */
	int (*device_enable) (struct platform_device *pdev);
	int (*device_shutdown) (struct platform_device *pdev);
	int (*device_idle) (struct platform_device *pdev);
};

#endif
