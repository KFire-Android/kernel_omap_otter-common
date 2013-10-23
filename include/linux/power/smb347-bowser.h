/*
 * summit_smb347.h
 *
 * Summit SMB347 Charger detection Driver
 *
 * Copyright (C) Amazon Technologies Inc. All rights reserved.
 * Donald Chan (hoiho@lab126.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#define GPIO_SMB347_IRQ		2
#define SMB347_NAME		"smb347"

struct smb347_platform_data {
	const char *regulator_name;
};
