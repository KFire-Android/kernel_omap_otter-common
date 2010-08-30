/* =============================================================================
*             Texas Instruments OMAP(TM) Platform Software
*  (c) Copyright 2009 Texas Instruments Incorporated.  All Rights Reserved.
*
*  Use of this software is controlled by the terms and conditions found 
*  in the license agreement under which this software has been supplied.
* =========================================================================== */
/**
 * @file  ABE_TEST.H
 *
 * Variable references
 *
 * @path 
 * @rev     01.00
 */
/* ---------------------------------------------------------------------------- 
*! 
*! Revision History
*! ===================================
*! 27-August-2009 Original (LLF)
* =========================================================================== */

 
#ifndef abe_test_def
#define abe_test_def

/*
 *   ============  HAL test API =============================================================================
 */ 
#ifdef __cplusplus
extern "C" {
#endif
void abe_auto_check_data_format_translation (void);
void abe_check_opp (void);
void abe_check_dma (void);
void abe_debug_and_non_regression (void);
void abe_check_mixers_gain_update (void);
void abe_test_scenario (abe_int32 scenario_id);
#ifdef __cplusplus
}
#endif
/*
 *   ============  HAL test DATA =============================================================================
 */ 


#endif /* REFDEF */
