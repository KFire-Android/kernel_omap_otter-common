/*
 * ==========================================================================
 *               Texas Instruments OMAP(TM) Platform Firmware
 * (c) Copyright 2009, Texas Instruments Incorporated.  All Rights Reserved.
 *
 *  Use of this firmware is controlled by the terms and conditions found
 *  in the license agreement under which this firmware has been supplied.
 * ==========================================================================
 */

#ifndef _ABE_API_H_
#define _ABE_API_H_

#ifdef __cplusplus
extern "C" {
#endif

/*
 * External API
 */
#if PC_SIMULATION
extern void target_server_read_pmem(abe_uint32 address,	 abe_uint32 *data, abe_uint32 nb_words_32bits);
extern void target_server_write_pmem(abe_uint32 address,	 abe_uint32 *data, abe_uint32 nb_words_32bits);
extern void target_server_read_cmem(abe_uint32 address,	 abe_uint32 *data, abe_uint32 nb_words_32bits);
extern void target_server_write_cmem(abe_uint32 address,	 abe_uint32 *data, abe_uint32 nb_words_32bits);
extern void target_server_read_atc(abe_uint32 address,	 abe_uint32 *data, abe_uint32 nb_words_32bits);
extern void target_server_write_atc(abe_uint32 address,        abe_uint32 *data, abe_uint32 nb_words_32bits);
extern void target_server_read_smem(abe_uint32 address_48bits, abe_uint32 *data, abe_uint32 nb_words_48bits);
extern void target_server_write_smem(abe_uint32 address_48bits, abe_uint32 *data, abe_uint32 nb_words_48bits);
extern void target_server_read_dmem(abe_uint32 address_byte,   abe_uint32 *data, abe_uint32 nb_byte);
extern void target_server_write_dmem(abe_uint32 address_byte,   abe_uint32 *data, abe_uint32 nb_byte);

extern void target_server_activate_mcpdm_ul(void);
extern void target_server_activate_mcpdm_dl(void);
extern void target_server_activate_dmic(void);
extern void target_server_set_voice_sampling(int dVirtAudioVoiceMode, int dVirtAudioVoiceSampleFrequency);
extern void target_server_set_dVirtAudioMultimediaMode(int dVirtAudioMultimediaMode);
#endif

/*
 * Internal API
 */

/**
* abe_read_sys_clock() description for void abe_read_sys_clock().
*
*  Operations : returns the current time indication for the LOG
*
*  Parameter  : No parameter
* @param
*
* @pre	  no pre-condition
*
* @post
*
* @return       error code
*
* @see
*/
void abe_read_sys_clock(abe_micros_t *time);

/**
* abe_fprintf() description for void abe_fprintf().
*
*  Operations : returns the current time indication for the LOG
*
*  Parameter  : No parameter
* @param
*
* @pre	  no pre-condition
*
* @post
*
* @return       error code
*
* @see
*/
//void abe_fprintf(char *line);

/*
 * API as part of the HAL paper documentation
 */

/**
* abe_reset_hal() description for void abe_reset_hal().
*
*  Operations : reset the HAL by reloading the static variables and default AESS registers.
*	Called after a PRCM cold-start reset of ABE
*
*  Parameter  : No parameter
* @param
*
* @pre	  no pre-condition
*
* @post
*
* @return       error code
*
* @see
*/
void abe_reset_hal(void);

/**
*  abe_read_use_case_opp() description for void abe_read_use_case_opp().
*
*  Operations : returns the expected min OPP for a given use_case list
*
*  Parameter  : No parameter
* @param
*
* @pre	  no pre-condition
*
* @post
*
* @return       error code
*
* @see
*/
void abe_read_use_case_opp(abe_use_case_id *u, abe_opp_t *o);

/**
* abe_load_fw() description for void abe_load_fw().
*
*  Operations :
*      loads the Audio Engine firmware, generate a single pulse on the Event generator
*      to let execution start, read the version number returned from this execution.
*
*  Parameter  : No parameter
* @param
*
* @pre	  no pre-condition
*
* @post
*
* @return       error code in case the firmware does not start.
*
* @see
*/
void abe_load_fw(void);

/**
* abe_read_port_address() description for void abe_read_port_address().
*
*  Operations :
*      This API returns the address of the DMA register used on this audio port.
*
*  Parameter  : No parameter
* @param	dma : output pointer to the DMA iteration and data destination pointer
*
* @pre	  no pre-condition
*
* @post
*
* @return       error code
*
* @see
*/
void abe_read_port_address(abe_port_id port, abe_dma_t *dma);

/**
* abe_irq_processing() description for void abe_irq_processing().
*
*  Parameter  :
*      No parameter
*
*  Operations :
*      This subroutine will check the IRQ_FIFO from the AE and act accordingly.
*      Some IRQ source are originated for the delivery of "end of time sequenced tasks"
*      notifications, some are originated from the Ping-Pong protocols, some are generated from
*      the embedded debugger when the firmware stops on programmable break-points, etc …
*
* @param	dma : output pointer to the DMA iteration and data destination pointer
*
* @pre	  no pre-condition
*
* @post
*
* @return       error code
*
* @see
*/
void abe_irq_processing(void);

/**
* abe_write_event_generator () description for void abe_event_generator_switch().
*
*  Operations :
*      load the AESS event generator hardware source. Loads the firmware parameters
*      accordingly. Indicates to the FW which data stream is the most important to preserve
*      in case all the streams are asynchronous. If the parameter is "default", let the HAL
*      decide which Event source is the best appropriate based on the opened ports.
*
* @param	e: Event Generation Counter, McPDM, DMIC or default.
*
* @pre	  no pre-condition
*
* @post
*
* @return       error code
*
* @see
*/
void abe_write_event_generator(abe_event_id e);

/**
* abe_set_opp_processing() description for void abe_set_opp_processing().
*
*  Parameter  :
*      New processing network and OPP:
*	  0: Ultra Lowest power consumption audio player (no post-processing, no mixer);
*	  1: OPP 25% (simple multimedia features, including low-power player);
*	  2: OPP 50% (multimedia and voice calls);
*	  3: OPP100% (EANC, multimedia complex use-cases);
*
*  Operations :
*      Rearranges the FW task network to the corresponding OPP list of features.
*      The corresponding AE ports are supposed to be set/reset accordingly before this switch.
*
* @param	o: desired opp
*
* @pre	  no pre-condition
*
* @post
*
* @return       error code
*
* @see
*/
void abe_set_opp_processing(abe_opp_t opp);

/**
* abe_set_ping_pong_bufferg() description for void abe_set_ping_pong_buffer().
*
*  Parameter  :
*      Port_ID :
*      Pointer name : Read or Write pointer
*      New data
*
*  Operations :
*      Updates the ping-pong read/write pointer with the input data.
*
* @param
*
* @pre	  no pre-condition
*
* @post
*
* @return       error code
*
* @see
*/
void abe_set_ping_pong_buffer(abe_port_id port, abe_uint32 n);

/**
* @fn abe_connect_irq_ping_pong_port()
*
*  Operations : enables the data echanges between a direct access to the DMEM
*       memory of ABE using cache flush. On each IRQ activation a subroutine
*       registered with "abe_plug_subroutine" will be called. This subroutine
*       will generate an amount of samples, send them to DMEM memory and call
*       "abe_set_ping_pong_buffer" to notify the new amount of samples in the
*       pong buffer.
*
*    Parameters :
*       id: port name
*       f : desired data format
*       I : index of the call-back subroutine to call
*       s : half-buffer (ping) size
*
*       p: returned base address of the first (ping) buffer)
*
* @see	ABE_API.h
*/
void abe_connect_irq_ping_pong_port(abe_port_id id, abe_data_format_t *f, abe_uint32 d,
				abe_uint32 s, abe_uint32 *p, abe_uint32 dsp_mcu_flag);

/**
* abe_plug_subroutine() description for void abe_plug_subroutine().
*
* Parameter :
*  id: returned sequence index after plugging a new subroutine
*  f : subroutine address to be inserted
*
*  Operations :
*      register a list of subroutines for call-back purpose.
*
* @param
*
* @pre	  no pre-condition
*
* @post
*
* @return       error code
*
* @see
*/
void abe_plug_subroutine(abe_uint32 *id, abe_subroutine2 f, abe_uint32 n, abe_uint32 *params);

/*
 *  ABE_RESET_PORT
 *
 *  Parameters :
 *  id: port name
 *
 *  Returned value : error code
 *
 *  Operations : stop the port activity and reload default parameters on the associated processing features.
 *
 */
void abe_reset_port(abe_port_id id);

/*
 *  ABE_READ_REMAINING_DATA
 *
 *  Parameter  :
 *      Port_ID :
 *      size : pointer to the remaining number of 32bits words
 *
 *  Operations :
 *      computes the remaining amount of data in the buffer.
 *
 *  Return value :
 *      error code
 */
void abe_read_remaining_data(abe_port_id port, abe_uint32 *n);

/*
 *  ABE_DISABLE_DATA_TRANSFER
 *
 *  Parameter  :
 *      p: port indentifier
 *
 *  Operations :
 *      disables the ATC descriptor
 *
 *  Return value :
 *      None.
 */
void abe_disable_data_transfer (abe_port_id p);

/*
 *  ABE_ENABLE_DATA_TRANSFER
 *
 *  Parameter  :
 *      p: port indentifier
 *
 *  Operations :
 *      enables the ATC descriptor
 *
 *  Return value :
 *      None.
 */
void abe_enable_data_transfer(abe_port_id p);

/*
 *  ABE_SET_DMIC_FILTER
 *
 *  Parameter  :
 *      DMIC decimation ratio : 16/25/32/40
 *
 *  Operations :
 *      Loads in CMEM a specific list of coefficients depending on the DMIC sampling
 *      frequency (2.4MHz or 3.84MHz);. This table compensates the DMIC decimator roll-off at 20kHz.
 *      The default table is loaded with the DMIC 2.4MHz recommended configuration.
 *
 *  Return value :
 *      None.
 */
void abe_set_dmic_filter(abe_dmic_ratio_t d);

/**
* @fn abe_connect_cbpr_dmareq_port()
*
*  Operations : enables the data echange between a DMA and the ABE through the
*       CBPr registers of AESS.
*
*   Parameters :
*   id: port name
*   f : desired data format
*   d : desired dma_request line (0..7)
*   a : returned pointer to the base address of the CBPr register and number of
*       samples to exchange during a DMA_request.
*
* @see	ABE_API.h
*/
void abe_connect_cbpr_dmareq_port(abe_port_id id, abe_data_format_t *f, abe_uint32 d, abe_dma_t *a);

/**
* @fn abe_connect_dmareq_ping_pong_port()
*
*  Operations : enables the data echanges between a DMA and a direct access to the
*       DMEM memory of ABE. On each dma_request activation the DMA will exchange "s"
*       bytes and switch to the "pong" buffer for a new buffer exchange.ABE
*
*    Parameters :
*    id: port name
*    f : desired data format
*    d : desired dma_request line (0..7)
*    s : half-buffer (ping) size
*
*    a : returned pointer to the base address of the ping-pong buffer and number of samples to exchange during a DMA_request.
*
* @see	ABE_API.h
*/
void abe_connect_dmareq_ping_pong_port(abe_port_id id, abe_data_format_t *f, abe_uint32 d, abe_uint32 s, abe_dma_t *a);

/**
* @fn abe_connect_serial_port()
*
*  Operations : enables the data echanges between a McBSP and an ATC buffer in
*   DMEM. This API is used connect 48kHz McBSP streams to MM_DL and 8/16kHz
*   voice streams to VX_UL, VX_DL, BT_VX_UL, BT_VX_DL. It abstracts the
*   abe_write_port API.
*
*   Parameters :
*    id: port name
*    f : data format
*    i : peripheral ID (McBSP #1, #2, #3)
*
* @see	ABE_API.h
*/
void abe_connect_serial_port(abe_port_id id, abe_data_format_t *f, abe_mcbsp_id i);

/**
* @fn abe_connect_slimbus_port()
*
*  Operations : enables the data echanges between 1/2 SB and an ATC buffers in
*   DMEM.
*
*   Parameters :
*    id: port name
*    f : data format
*    i : peripheral ID (McBSP #1, #2, #3)
*    j : peripheral ID (McBSP #1, #2, #3)
*
* @see        ABE_API.h
*/
void abe_connect_slimbus_port(abe_port_id id, abe_data_format_t *f,
			abe_slimbus_id sb_port1, abe_slimbus_id sb_port2);


/*
 *  ABE_WRITE_GAIN
 *
 *  Parameter  :
 *      port : name of the port (VX_DL_PORT, MM_DL_PORT, MM_EXT_DL_PORT, TONES_DL_PORT, …);
 *      dig_gain_port pointer to returned port gain and time constant
 *
 *  Operations :
 *      saves the gain data in the local HAL-L0 table of gains in native format.
 *      Translate the gain to the AE-FW format and load it in CMEM
 *
 *  Return value :
 *      error code in case the gain_id is not compatible with the current OPP value.
 */

void abe_write_gain(abe_gain_id id, abe_gain_t f_g, abe_ramp_t f_ramp, abe_port_id p);

/*
 *  ABE_WRITE_EQUALIZER
 *
 *  Parameter  :
 *      Id  : name of the equalizer
 *      Param : equalizer coefficients
 *
 *  Operations :
 *      Load the coefficients in CMEM. This API can be called when the corresponding equalizer
 *      is not activated. After reloading the firmware the default coefficients corresponds to
 *      "no equalizer feature". Loading all coefficients with zeroes disables the feature.
 *
 *  Return value :
 *      None.
 */
void abe_write_equalizer(abe_equ_id id, abe_equ_t *param);

/*
 * ABE_SELECT_MAIN_PORT
 *
 * Parameter  :
 * id : audio port name
 * Operations :
 * tells the FW which is the reference stream for adjusting the processing on 23/24/25 slots
 *
 * Return value :
 * None.
 */
void abe_select_main_port(abe_port_id id);

/*
 * ABE_WRITE_ASRC
 *
 *  Parameter  :
 *      Id  : name of the asrc
 *      param : drift value t compensate
 *
 *  Operations :
 *      Load the drift coefficients in FW memory. This API can be called when the corresponding
 *      ASRC is not activated. After reloading the firmware the default coefficients corresponds
 *      to "no ASRC activated". Loading the drift value with zero disables the feature.
 *
 *  Return value :
 *      None.
 */
void abe_write_asrc(abe_asrc_id id, abe_drift_t param);
void abe_set_asrc_drift_control(abe_asrc_id id, abe_uint32 f);

/*
 *  ABE_WRITE_APS
 *
 *  Parameter  :
 *      Id  : name of the aps filter
 *      param : table of filter coefficients
 *
 *  Operations :
 *      Load the filters and thresholds coefficients in FW memory. This API can be called when
 *      the corresponding APS is not activated. After reloading the firmware the default coefficients
 *      corresponds to "no APS activated". Loading all the coefficients value with zero disables
 *      the feature.
 *
 *  Return value :
 *      None.
 */
void abe_write_aps(abe_aps_id id, abe_aps_t *param);

/*
 *  ABE_WRITE_MIXER
 *
 *  Parameter  :
 *  Id  : name of the mixer
 *  param : list of input gains of the mixer
 *  p : list of ports corresponding to the above gains
 *
 *  Operations :
 *      Load the gain coefficients in FW memory. This API can be called when the corresponding
 *      MIXER is not activated. After reloading the firmware the default coefficients corresponds
 *      to "all input and output mixer's gain in mute state". A mixer is disabled with a network
 *      reconfiguration corresponding to an OPP value.
 *
 *  Return value :
 *      None.
 */
void abe_write_mixer(abe_mixer_id id, abe_gain_t g, abe_ramp_t ramp, abe_port_id p);

/*
 *  ABE_SET_ROUTER_CONFIGURATION
 *
 *  Parameter  :
 *      Id  : name of the router
 *      Conf : id of the configuration
 *      param : list of output index of the route
 *
 *  Operations :
 *      The uplink router takes its input from DMIC (6 samples), AMIC (2 samples) and
 *      PORT1/2 (2 stereo ports). Each sample will be individually stored in an intermediate
 *      table of 10 elements. The intermediate table is used to route the samples to
 *      three directions : REC1 mixer, 2 EANC DMIC source of filtering and MM recording audio path.
 *      For example, a use case consisting in AMIC used for uplink voice communication, DMIC 0,1,2,3
 *      used for multimedia recording, , DMIC 5 used for EANC filter, DMIC 4 used for the feedback channel,
 *      will be implemented with the following routing table index list :
 *      [3, 2 , 1, 0, 0, 0 (two dummy indexes to data that will not be on MM_UL), 4, 5, 7, 6]
 *
 *  Return value :
 *      None.
 */
void abe_set_router_configuration(abe_router_id id, abe_uint32 configuration, abe_router_t *param);

/*
 *  ABE_READ_DEBUG_TRACE
 *
 *  Parameter  :
 *      data destination pointer
 *      max number of data read
 *
 *  Operations :
 *      reads the AE circular data pointer holding pairs of debug data+timestamps, and store
 *      the pairs in linear addressing to the parameter pointer. Stops the copy when the max
 *	parameter is reached or when the FIFO is empty.
 *
 *  Return value :
 *      None.
 */
void abe_read_debug_trace(abe_uint32 *data, abe_uint32 *n);

/*
 *  ABE_READ_DEBUG_TRACE
 *
 *  Parameter  :
 *      data destination pointer
 *      max number of data read
 *
 *  Operations :
 *      reads the AE circular data pointer holding pairs of debug data+timestamps, and store
 *      the pairs in linear addressing to the parameter pointer. Stops the copy when the max
 *        parameter is reached or when the FIFO is empty.
 *
 *  Return value :
 *      None.
 */

void abe_read_debug_trace(abe_uint32 *data, abe_uint32 *n);

/*
 *  ABE_CONNECT_DEBUG_TRACE
 *
 *  Parameter  :
 *      pointer to the DMEM trace buffer
 *
 *  Operations :
 *      returns the address and size of the real-time debug trace buffer,
 *	the content of which will vary from one firmware release to an other
 *
 *  Return value :
 *      None.
 */

void abe_connect_debug_trace(abe_dma_t *a);

/*
 *  ABE_SET_DEBUG_TRACE
 *
 *  Parameter  :
 *      debug ID from a list to be defined
 *
 *  Operations :
 *      load a mask which filters the debug trace to dedicated types of data
 *
 *  Return value :
 *      None.
 */
void abe_set_debug_trace(abe_dbg_t debug);

/*
 *  ABE_ENABLE_TEST_PATTERN
 *
 *  Parameter  :
 *
 *  Operations :
 *
 *  Return value :
 *      None.
 */

void abe_enable_test_pattern(abe_patched_pattern_id smem_id, abe_uint32 on_off);

#ifdef __cplusplus
}
#endif

#endif	/* _ABE_API_H_ */
