/*
 * ==========================================================================
 *               Texas Instruments OMAP(TM) Platform Firmware
 * (c) Copyright 2009, Texas Instruments Incorporated.  All Rights Reserved.
 *
 *  Use of this firmware is controlled by the terms and conditions found
 *  in the license agreement under which this firmware has been supplied.
 * ==========================================================================
 */

#ifndef _ABE_TEST_H_
#define _ABE_TEST_H_

/*
 * HAL test API
 */
void abe_auto_check_data_format_translation(void);
void abe_check_opp(void);
void abe_check_dma(void);
void abe_debug_and_non_regression(void);
void abe_check_mixers_gain_update(void);
void abe_test_scenario(abe_int32 scenario_id);

/*
 * HAL test DATA
 */

#endif	/* _ABE_TEST_H_ */
