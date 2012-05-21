/*
+ * SMSC_ECE1099 Keypad driver header
+ *
+ * Copyright (C) 2011 Texas Instruments Incorporated - http://www.ti.com/
+ *
+ * This program is free software; you can redistribute it and/or modify
+ * it under the terms of the GNU General Public License version 2 as
+ * published by the Free Software Foundation.
+ */

#ifndef __SMSC_H_
#define __SMSC_H_

struct smsc_keypad_data {
	const struct matrix_keymap_data *keymap_data;
	unsigned rows;
	unsigned cols;
	bool rep;
};

#endif         /* __SMSC_H_ */
