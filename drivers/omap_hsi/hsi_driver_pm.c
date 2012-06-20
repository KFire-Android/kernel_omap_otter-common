/*
 * hsi_driver_pm.c
 *
 * Implements HSI driver Power management functionality.
 *
 * Copyright (C) 2012 Texas Instruments, Inc.
 *
 * Author: Djamil ELAIDI <d-elaidi@ti.com>
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include "hsi_driver.h"


int hsi_pm_change_hsi_speed(struct hsi_dev *hsi_ctrl, bool hi_speed)
{
	struct hsi_platform_data *pdata;
	int err = 0;

	hsi_ctrl->hsi_fclk_req = hi_speed ? HSI_FCLK_HI_SPEED :
					    HSI_FCLK_LOW_SPEED;

	if (hsi_ctrl->hsi_fclk_req == hsi_ctrl->hsi_fclk_current) {
		dev_dbg(hsi_ctrl->dev, "HSI FClk already @%ldHz\n",
			 hsi_ctrl->hsi_fclk_current);
		return 0;
	}

	pdata = dev_get_platdata(hsi_ctrl->dev);

	/* Set the HSI FCLK to requested value. */
	err = pdata->device_scale(hsi_ctrl->dev, hsi_ctrl->hsi_fclk_req);
	if (err == -EBUSY) {
		/* PM framework init is late_initcall, so it may not yet be */
		/* initialized, so be prepared to retry later on open. */
		dev_warn(hsi_ctrl->dev,
			 "Cannot set HSI FClk to default value: %ld. "
			 "Will retry on next open\n",
			 pdata->default_hsi_fclk);
	} else if (err < 0) {
		dev_err(hsi_ctrl->dev,
			"%s: Error: Cannot set HSI FClk to %ldHz, err %d\n",
			__func__, hsi_ctrl->hsi_fclk_req, err);
	} else {
		dev_info(hsi_ctrl->dev,
			 "HSI FClk changed from %ldHz to %ldHz\n",
			 hsi_ctrl->hsi_fclk_current, hsi_ctrl->hsi_fclk_req);
		hsi_ctrl->hsi_fclk_current = hsi_ctrl->hsi_fclk_req;
	}

	return err;
}


