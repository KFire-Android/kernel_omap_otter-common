/*
 * GlobalTypes.h
 *
 * Syslink driver support for OMAP Processors.
 *
 * Copyright (C) 2008-2009 Texas Instruments, Inc.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef __GLOBALTYPES_H
#define __GLOBALTYPES_H

#define  REG volatile

/*
 * Definition: RET_CODE_BASE
 *
 * DESCRIPTION:  Base value for return code offsets
 *
 *
 */
#define RET_CODE_BASE        0

/*
 * TYPE: ReturnCode_t
 *
 * DESCRIPTION:  Return codes to be returned by all library functions
 *
 *
 */
enum ReturnCode_label {
RET_OK = 0,
RET_FAIL = -1,
RET_BAD_NULL_PARAM = -2,
RET_PARAM_OUT_OF_RANGE = -3,
RET_INVALID_ID = -4,
RET_EMPTY = -5,
RET_FULL = -6,
RET_TIMEOUT = -7,
RET_INVALID_OPERATION = -8,
/* Add new error codes at end of above list */
RET_NUM_RET_CODES     /* this should ALWAYS be LAST entry */
};



/*
 * MACRO: RD_MEM_32_VOLATILE, WR_MEM_32_VOLATILE
 *
 * DESCRIPTION:  32 bit register access macros
 *
 *
 */
#define RD_MEM_32_VOLATILE(addr) \
((unsigned long)(*((REG unsigned long *)(addr))))

#define WR_MEM_32_VOLATILE(addr, data) \
(*((REG unsigned long *)(addr)) = (unsigned long)(data))




#ifdef CHECK_INPUT_PARAMS
/*
 * MACRO: CHECK_INPUT_PARAMS
 *
 * DESCRIPTION:  Checks an input code and returns a specified value if code is
 *               invalid value, also writes spy value if error found.
 *
 * NOTE:         Can be disabled to save HW cycles.
 *
 *
 */
#define CHECK_INPUT_PARAM(actualValue, invalidValue,  \
returnCodeIfMismatch, spyCodeIfMisMatch) do {\
	if ((invalidValue) == (actualValue)) {\
		RES_Set((spyCodeIfMisMatch));\
		return returnCodeIfMismatch; \
	} \
} while (0)

/*
 * MACRO: CHECK_INPUT_RANGE
 *
 * DESCRIPTION:  Checks an input value and returns a specified value if not in
 *               specified range, also writes spy value if error found.
 *
* NOTE:         Can be disabled to save HW cycles.
 *
 *
 */
#define CHECK_INPUT_RANGE(actualValue, minValidValue, maxValidValue, \
returnCodeIfMismatch, spyCodeIfMisMatch) do {\
	if (((actualValue) < (minValidValue)) || \
		((actualValue) > (maxValidValue))) {\
		RES_Set((spyCodeIfMisMatch));\
		return returnCodeIfMismatch; \
	} \
} while (0)

/*
 * MACRO: CHECK_INPUT_RANGE_MIN0
 *
 * DESCRIPTION:  Checks an input value and returns a
 * specified value if not in
 * specified range, also writes spy value if error found.
 * The minimum
 * value is 0.
 *
 * NOTE:         Can be disabled to save HW cycles.
 *
 *
 */
#define CHECK_INPUT_RANGE_MIN0(actualValue, maxValidValue, \
returnCodeIfMismatch, spyCodeIfMisMatch) do {\
	if ((actualValue) > (maxValidValue)) {\
		RES_Set((spyCodeIfMisMatch));\
		return returnCodeIfMismatch; \
	} \
} while (0)

#else

#define CHECK_INPUT_PARAM(actualValue, invalidValue, returnCodeIfMismatch,\
spyCodeIfMisMatch)

#define CHECK_INPUT_PARAM_NO_SPY(actualValue, invalidValue, \
returnCodeIfMismatch)

#define CHECK_INPUT_RANGE(actualValue, minValidValue, maxValidValue, \
returnCodeIfMismatch, spyCodeIfMisMatch)

#define CHECK_INPUT_RANGE_NO_SPY(actualValue, minValidValue , \
maxValidValue, returnCodeIfMismatch)

#define CHECK_INPUT_RANGE_MIN0(actualValue, maxValidValue, \
returnCodeIfMismatch, spyCodeIfMisMatch)

#define CHECK_INPUT_RANGE_NO_SPY_MIN0(actualValue, \
maxValidValue, returnCodeIfMismatch)

#endif

#ifdef __cplusplus
}
#endif
#endif	/* __GLOBALTYPES_H */
