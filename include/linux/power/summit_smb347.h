/*
 * Charger driver for TI BQ27541
 *
 * Copyright (C) Quanta Computer Inc. All rights reserved.
 *  Eric Nien <Eric.Nien@quantatw.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SUMMIT_SMB347_H_
#define __SUMMIT_SMB347_H_

struct summit_smb347_platform_data {
	int	mbid;
	int	pin_en;
	char	*pin_en_name;
	int	pin_susp;
	char	*pin_susp_name;
	int	initial_max_aicl;
	int	initial_pre_max_aicl;
	int	initial_charge_current;
	void (*led_callback)(u8 green_value, u8 orange_value);
};

#endif /* __SUMMIT_SMB347_H_ */
