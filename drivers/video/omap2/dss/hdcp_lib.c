/*
 * hdcp_lib.c
 *
 * HDCP interface DSS driver setting for TI's OMAP4 family of processor.
 * Copyright (C) 2010-2011 Texas Instruments Incorporated - http://www.ti.com/
 * Authors: Fabrice Olivero
 *	Fabrice Olivero <f-olivero@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/delay.h>
#include "hdcp.h"
#include "hdmi.h"

#undef FLD_MASK
#define FLD_MASK(start, end)    (((1 << (start - end + 1)) - 1) << (end))
#undef FLD_VAL
#define FLD_VAL(val, start, end) (((val) << end) & FLD_MASK(start, end))
#define FLD_GET(val, start, end) (((val) & FLD_MASK(start, end)) >> (end))
#define FLD_MOD(orig, val, start, end) \
	(((orig) & ~FLD_MASK(start, end)) | FLD_VAL(val, start, end))

static void hdcp_lib_read_an(u8 *an);
static void hdcp_lib_read_aksv(u8 *ksv_data);
static void hdcp_lib_write_bksv(u8 *ksv_data);
static void hdcp_lib_generate_an(u8 *an);
static int hdcp_lib_r0_check(void);
static int hdcp_lib_sha_bstatus(struct hdcp_sha_in *sha);
static void hdcp_lib_set_repeater_bit_in_tx(enum hdcp_repeater rx_mode);
static void hdcp_lib_toggle_repeater_bit_in_tx(void);
static int hdcp_lib_initiate_step1(void);
static int hdcp_lib_check_ksv(uint8_t ksv[5]);

/*-----------------------------------------------------------------------------
 * Function: hdcp_lib_read_an
 *-----------------------------------------------------------------------------
 */
static void hdcp_lib_read_an(u8 *an)
{
	u8 i;

	for (i = 0; i < 8; i++) {
		an[i] = (RD_REG_32(hdcp.hdmi_wp_base_addr +
			 HDMI_IP_CORE_SYSTEM,
			 HDMI_IP_CORE_SYSTEM__AN0 +
			 i * sizeof(uint32_t))) & 0xFF;
	}
}

/*-----------------------------------------------------------------------------
 * Function: hdcp_lib_read_aksv
 *-----------------------------------------------------------------------------
 */
static void hdcp_lib_read_aksv(u8 *ksv_data)
{
	u8 i;
	for (i = 0; i < 5; i++) {
		ksv_data[i] = RD_REG_32(hdcp.hdmi_wp_base_addr +
				   HDMI_IP_CORE_SYSTEM,
				   HDMI_IP_CORE_SYSTEM__AKSV0 +
				   i * sizeof(uint32_t));

	}
}

/*-----------------------------------------------------------------------------
 * Function: hdcp_lib_write_bksv
 *-----------------------------------------------------------------------------
 */
static void hdcp_lib_write_bksv(u8 *ksv_data)
{
	u8 i;
	for (i = 0; i < 5; i++) {
		WR_REG_32(hdcp.hdmi_wp_base_addr +
			HDMI_IP_CORE_SYSTEM, HDMI_IP_CORE_SYSTEM__BKSV0 +
			i * sizeof(uint32_t), ksv_data[i]);
	}
}
/*-----------------------------------------------------------------------------
 * Function: hdcp_lib_generate_an
 *-----------------------------------------------------------------------------
 */
static void hdcp_lib_generate_an(u8 *an)
{
	/* Generate An using HDCP HW */
	HDCP_DBG("hdcp_lib_generate_an()");

	/* Start AN Gen */
	WR_FIELD_32(hdcp.hdmi_wp_base_addr + HDMI_IP_CORE_SYSTEM,
		    HDMI_IP_CORE_SYSTEM__HDCP_CTRL, 3, 3, 0);

	/* Delay of 10 ms */
	mdelay(10);

	/* Stop AN Gen */
	WR_FIELD_32(hdcp.hdmi_wp_base_addr + HDMI_IP_CORE_SYSTEM,
		    HDMI_IP_CORE_SYSTEM__HDCP_CTRL, 3, 3, 1);

	/* Must set 0x72:0x0F[3] twice to guarantee that takes effect */
	WR_FIELD_32(hdcp.hdmi_wp_base_addr + HDMI_IP_CORE_SYSTEM,
		    HDMI_IP_CORE_SYSTEM__HDCP_CTRL, 3, 3, 1);

	hdcp_lib_read_an(an);

	HDCP_DBG("AN: %x %x %x %x %x %x %x %x", an[0], an[1], an[2], an[3],
					   an[4], an[5], an[6], an[7]);
}

/*-----------------------------------------------------------------------------
 * Function: hdcp_lib_r0_check
 *-----------------------------------------------------------------------------
 */
static int hdcp_lib_r0_check(void)
{
	u8 ro_rx[2], ro_tx[2];

	HDCP_DBG("hdcp_lib_r0_check()");

	/* DDC: Read Ri' from RX */
	if (hdcp_ddc_read(DDC_RI_LEN, DDC_RI_ADDR , (u8 *)&ro_rx))
		return -HDCP_DDC_ERROR;

	/* Read Ri in HDCP IP */
	ro_tx[0] = RD_REG_32(hdcp.hdmi_wp_base_addr + HDMI_IP_CORE_SYSTEM,
			     HDMI_IP_CORE_SYSTEM__R1) & 0xFF;

	ro_tx[1] = RD_REG_32(hdcp.hdmi_wp_base_addr + HDMI_IP_CORE_SYSTEM,
			     HDMI_IP_CORE_SYSTEM__R2) & 0xFF;

	/* Compare values */
	HDCP_DBG("ROTX: %x%x RORX:%x%x",
		 ro_tx[0], ro_tx[1], ro_rx[0], ro_rx[1]);

	if ((ro_rx[0] == ro_tx[0]) && (ro_rx[1] == ro_tx[1]))
		return HDCP_OK;
	else
		return -HDCP_AUTH_FAILURE;
}

/*-----------------------------------------------------------------------------
 * Function: hdcp_lib_sha_bstatus
 *-----------------------------------------------------------------------------
 */
static int hdcp_lib_sha_bstatus(struct hdcp_sha_in *sha)
{
	u8 data[2];

	if (hdcp_ddc_read(DDC_BSTATUS_LEN, DDC_BSTATUS_ADDR, data))
		return -HDCP_DDC_ERROR;

	sha->data[sha->byte_counter++] = data[0];
	sha->data[sha->byte_counter++] = data[1];

	return HDCP_OK;
}

/*-----------------------------------------------------------------------------
 * Function: hdcp_lib_set_repeater_bit_in_tx
 *-----------------------------------------------------------------------------
 */
static void hdcp_lib_set_repeater_bit_in_tx(enum hdcp_repeater rx_mode)
{
	HDCP_DBG("hdcp_lib_set_repeater_bit_in_tx() value=%d", rx_mode);

	WR_FIELD_32(hdcp.hdmi_wp_base_addr + HDMI_IP_CORE_SYSTEM,
		HDMI_IP_CORE_SYSTEM__HDCP_CTRL, 4, 4, rx_mode);
}

/*-----------------------------------------------------------------------------
 * Function: hdcp_lib_toggle_repeater_bit_in_tx
 *-----------------------------------------------------------------------------
 */
static void hdcp_lib_toggle_repeater_bit_in_tx(void)
{
	if (hdcp_lib_check_repeater_bit_in_tx())
		hdcp_lib_set_repeater_bit_in_tx(HDCP_RECEIVER);
	else
		hdcp_lib_set_repeater_bit_in_tx(HDCP_REPEATER);
}

/*-----------------------------------------------------------------------------
 * Function: hdcp_lib_initiate_step1
 *-----------------------------------------------------------------------------
 */
static int hdcp_lib_initiate_step1(void)
{
	/* HDCP authentication steps:
	 *   1) Read Bksv - check validity (is HDMI Rx supporting HDCP ?)
	 *   2) Initializes HDCP (CP reset release)
	 *   3) Read Bcaps - is HDMI Rx a repeater ?
	 *   *** First part authentication ***
	 *   4) Read Bksv - check validity (is HDMI Rx supporting HDCP ?)
	 *   5) Generates An
	 *   6) DDC: Writes An, Aksv
	 *   7) DDC: Write Bksv
	 */
	uint8_t an_ksv_data[8], an_bksv_data[8];
	uint8_t rx_type;

	HDCP_DBG("hdcp_lib_initiate_step1()\n");

	/* DDC: Read BKSV from RX */
	if (hdcp_ddc_read(DDC_BKSV_LEN, DDC_BKSV_ADDR , an_ksv_data))
		return -HDCP_DDC_ERROR;

	if (hdcp.pending_disable)
		return -HDCP_CANCELLED_AUTH;

	HDCP_DBG("BKSV: %02x %02x %02x %02x %02x",
		 an_ksv_data[0], an_ksv_data[1],
		 an_ksv_data[2], an_ksv_data[3],
		 an_ksv_data[4]);

	if (hdcp_lib_check_ksv(an_ksv_data)) {
		HDCP_DBG("BKSV error (number of 0 and 1)");
		return -HDCP_AUTH_FAILURE;
	}

	/* TODO: Need to confirm it is required */
#ifndef _9032_AN_STOP_FIX_
	hdcp_lib_toggle_repeater_bit_in_tx();
#endif

	/* Release CP reset bit */
	WR_FIELD_32(hdcp.hdmi_wp_base_addr + HDMI_IP_CORE_SYSTEM,
		    HDMI_IP_CORE_SYSTEM__HDCP_CTRL, 2, 2, 1);

	/* Read BCAPS to determine if HDCP RX is a repeater */
	if (hdcp_ddc_read(DDC_BCAPS_LEN, DDC_BCAPS_ADDR, &rx_type))
		return -HDCP_DDC_ERROR;

	if (hdcp.pending_disable)
		return -HDCP_CANCELLED_AUTH;

	rx_type = FLD_GET(rx_type, DDC_BIT_REPEATER, DDC_BIT_REPEATER);

	/* Set repeater bit in HDCP CTRL */
	if (rx_type == 1) {
		hdcp_lib_set_repeater_bit_in_tx(HDCP_REPEATER);
		HDCP_DBG("HDCP RX is a repeater");
	} else {
		hdcp_lib_set_repeater_bit_in_tx(HDCP_RECEIVER);
		HDCP_DBG("HDCP RX is a receiver");
	}

/* Power debug code */
#ifdef POWER_TRANSITION_DBG
	HDCP_INFO("\n**************************\n"
		  "AUTHENTICATION: WAIT FOR DSS TRANSITION\n"
		  "*************************\n");
	mdelay(10000);
	HDCP_INFO("\n**************************\n"
		  "DONE\n"
		  "*************************\n");
#endif
	/* DDC: Read BKSV from RX */
	if (hdcp_ddc_read(DDC_BKSV_LEN, DDC_BKSV_ADDR , an_bksv_data))
		return -HDCP_DDC_ERROR;

	/* Generate An */
	hdcp_lib_generate_an(an_ksv_data);

	/* Authentication 1st step initiated HERE */

	/* DDC: Write An */
	if (hdcp_ddc_write(DDC_AN_LEN, DDC_AN_ADDR , an_ksv_data))
		return -HDCP_DDC_ERROR;

	if (hdcp.pending_disable)
		return -HDCP_CANCELLED_AUTH;

	/* Read AKSV from IP: (HDCP AKSV register) */
	hdcp_lib_read_aksv(an_ksv_data);

	HDCP_DBG("AKSV: %02x %02x %02x %02x %02x",
		 an_ksv_data[0], an_ksv_data[1],
		 an_ksv_data[2], an_ksv_data[3],
		 an_ksv_data[4]);

	if (hdcp_lib_check_ksv(an_ksv_data)) {
		HDCP_ERR("HDCP: AKSV error (number of 0 and 1)\n");
		return -HDCP_AKSV_ERROR;
	}

	if (hdcp.pending_disable)
		return -HDCP_CANCELLED_AUTH;

	/* DDC: Write AKSV */
	if (hdcp_ddc_write(DDC_AKSV_LEN, DDC_AKSV_ADDR, an_ksv_data))
		return -HDCP_DDC_ERROR;

	if (hdcp.pending_disable)
		return -HDCP_CANCELLED_AUTH;

	/* Write Bksv to IP */
	hdcp_lib_write_bksv(an_bksv_data);

	/* Check IP BKSV error */
	if (RD_FIELD_32(hdcp.hdmi_wp_base_addr + HDMI_IP_CORE_SYSTEM,
			HDMI_IP_CORE_SYSTEM__HDCP_CTRL, 5, 5))
		return -HDCP_AUTH_FAILURE;

	/* Here BSKV should be checked against revokation list */

	return HDCP_OK;
}

/*-----------------------------------------------------------------------------
 * Function: hdcp_lib_check_ksv
 *-----------------------------------------------------------------------------
 */
static int hdcp_lib_check_ksv(uint8_t ksv[5])
{
	int i, j;
	int zero = 0, one = 0;

	for (i = 0; i < 5; i++) {
		/* Count number of zero / one */
		for (j = 0; j < 8; j++) {
			if (ksv[i] & (0x01 << j))
				one++;
			else
				zero++;
		}
	}

	if (one == zero)
		return 0;
	else
		return -1;
}

/*-----------------------------------------------------------------------------
 * Function: hdcp_lib_disable
 *-----------------------------------------------------------------------------
 */
int hdcp_lib_disable()
{
	HDCP_DBG("hdcp_lib_disable() %u", jiffies_to_msecs(jiffies));

	/* CP reset */
	WR_FIELD_32(hdcp.hdmi_wp_base_addr + HDMI_IP_CORE_SYSTEM,
		    HDMI_IP_CORE_SYSTEM__HDCP_CTRL, 2, 2, 0);

	/* Clear AV mute in case it was set */
	hdcp_lib_set_av_mute(AV_MUTE_CLEAR);

	return HDCP_OK;
}

/*-----------------------------------------------------------------------------
 * Function: hdcp_lib_set_encryption
 *-----------------------------------------------------------------------------
 */
void hdcp_lib_set_encryption(enum encryption_state enc_state)
{
	unsigned long flags;

	spin_lock_irqsave(&hdcp.spinlock, flags);

	/* HDCP_CTRL::ENC_EN set/clear */
	WR_FIELD_32(hdcp.hdmi_wp_base_addr + HDMI_IP_CORE_SYSTEM,
		    HDMI_IP_CORE_SYSTEM__HDCP_CTRL, 0, 0, enc_state);

	/* Read to flush */
	RD_REG_32(hdcp.hdmi_wp_base_addr + HDMI_IP_CORE_SYSTEM,
		    HDMI_IP_CORE_SYSTEM__HDCP_CTRL);

	spin_unlock_irqrestore(&hdcp.spinlock, flags);

	pr_info("HDCP: Encryption state changed: %s hdcp_ctrl: %02x",
				enc_state == HDCP_ENC_OFF ? "OFF" : "ON",
				RD_REG_32(hdcp.hdmi_wp_base_addr +
					  HDMI_IP_CORE_SYSTEM,
					  HDMI_IP_CORE_SYSTEM__HDCP_CTRL));

}

/*-----------------------------------------------------------------------------
 * Function: hdcp_lib_set_av_mute
 *-----------------------------------------------------------------------------
 */
void hdcp_lib_set_av_mute(enum av_mute av_mute_state)
{
	struct hdmi_ip_data *ip_data;
	unsigned long flags;

	HDCP_DBG("hdcp_lib_set_av_mute() av_mute=%d", av_mute_state);

	ip_data = get_hdmi_ip_data();

	spin_lock_irqsave(&hdcp.spinlock, flags);

	if (!ti_hdmi_4xxx_set_av_mute(ip_data, av_mute_state))
		HDCP_ERR("HDCP: Changing AV Mute flag failed\n");

	spin_unlock_irqrestore(&hdcp.spinlock, flags);
}

/*-----------------------------------------------------------------------------
 * Function: hdcp_lib_check_repeater_bit_in_tx
 *-----------------------------------------------------------------------------
 */
u8 hdcp_lib_check_repeater_bit_in_tx(void)
{
	return RD_FIELD_32(hdcp.hdmi_wp_base_addr + HDMI_IP_CORE_SYSTEM,
			   HDMI_IP_CORE_SYSTEM__HDCP_CTRL, 4, 4);
}

/*-----------------------------------------------------------------------------
 * Function: hdcp_lib_auto_ri_check
 *-----------------------------------------------------------------------------
 */
void hdcp_lib_auto_ri_check(bool state)
{
	u8 reg_val;
	unsigned long flags;

	HDCP_DBG("hdcp_lib_auto_ri_check() state=%s",
		state == true ? "ON" : "OFF");

	spin_lock_irqsave(&hdcp.spinlock, flags);

	reg_val = RD_REG_32(hdcp.hdmi_wp_base_addr + HDMI_IP_CORE_SYSTEM,
			    HDMI_IP_CORE_SYSTEM__INT_UNMASK3);

	reg_val = (state == true) ? (reg_val | 0xB0) : (reg_val & ~0xB0);

	/* Turn on/off the following Auto Ri interrupts */
	WR_REG_32(hdcp.hdmi_wp_base_addr + HDMI_IP_CORE_SYSTEM,
		  HDMI_IP_CORE_SYSTEM__INT_UNMASK3, reg_val);

	/* Enable/Disable Ri */
	WR_FIELD_32(hdcp.hdmi_wp_base_addr + HDMI_IP_CORE_SYSTEM,
		    HDMI_IP_CORE_SYSTEM__RI_CMD, 0, 0,
		    ((state == true) ? 1 : 0));

	/* Read to flush */
	RD_REG_32(hdcp.hdmi_wp_base_addr + HDMI_IP_CORE_SYSTEM,
		    HDMI_IP_CORE_SYSTEM__RI_CMD);

	spin_unlock_irqrestore(&hdcp.spinlock, flags);
}

/*-----------------------------------------------------------------------------
 * Function: hdcp_lib_auto_bcaps_rdy_check
 *-----------------------------------------------------------------------------
 */
void hdcp_lib_auto_bcaps_rdy_check(bool state)
{
	u8 reg_val;
	unsigned long flags;

	HDCP_DBG("hdcp_lib_auto_bcaps_rdy_check() state=%s",
		state == true ? "ON" : "OFF");

	spin_lock_irqsave(&hdcp.spinlock, flags);

	/* Enable KSV_READY / BACP_DONE interrupt */
	WR_FIELD_32(hdcp.hdmi_wp_base_addr + HDMI_IP_CORE_SYSTEM,
		    HDMI_IP_CORE_SYSTEM__INT_UNMASK2, 7, 7,
		    ((state == true) ? 1 : 0));

	/* Enable/Disable Ri  & Bcap */
	reg_val = RD_REG_32(hdcp.hdmi_wp_base_addr + HDMI_IP_CORE_SYSTEM,
			    HDMI_IP_CORE_SYSTEM__RI_CMD);

	/* Enable RI_EN & BCAP_EN OR disable BCAP_EN */
	reg_val = (state == true) ? (reg_val | 0x3) : (reg_val & ~0x2);

	WR_REG_32(hdcp.hdmi_wp_base_addr + HDMI_IP_CORE_SYSTEM,
		  HDMI_IP_CORE_SYSTEM__RI_CMD, reg_val);

	/* Read to flush */
	RD_REG_32(hdcp.hdmi_wp_base_addr + HDMI_IP_CORE_SYSTEM,
		  HDMI_IP_CORE_SYSTEM__RI_CMD);

	spin_unlock_irqrestore(&hdcp.spinlock, flags);

	HDCP_DBG("hdcp_lib_auto_bcaps_rdy_check() Done\n");
}

/*-----------------------------------------------------------------------------
 * Function: hdcp_lib_step1_start
 *-----------------------------------------------------------------------------
 */
int hdcp_lib_step1_start(void)
{
	u8 hdmi_mode;
	int status;

	HDCP_DBG("hdcp_lib_step1_start() %u", jiffies_to_msecs(jiffies));

	/* Check if mode is HDMI or DVI */
	hdmi_mode = RD_REG_32(hdcp.hdmi_wp_base_addr + HDMI_CORE_AV_BASE,
			      HDMI_CORE_AV_HDMI_CTRL) &
			      HDMI_CORE_AV_HDMI_CTRL__HDMI_MODE;

	HDCP_DBG("RX mode: %s", hdmi_mode ? "HDMI" : "DVI");

	/* Set AV Mute */
	hdcp_lib_set_av_mute(AV_MUTE_SET);

	/* Must turn encryption off when AVMUTE */
	hdcp_lib_set_encryption(HDCP_ENC_OFF);

	status = hdcp_lib_initiate_step1();

	if (hdcp.pending_disable)
		return -HDCP_CANCELLED_AUTH;
	else
		return status;
}

/*-----------------------------------------------------------------------------
 * Function: hdcp_lib_step1_r0_check
 *-----------------------------------------------------------------------------
 */
int hdcp_lib_step1_r0_check(void)
{
	int status = HDCP_OK;

	/* HDCP authentication steps:
	 *   1) DDC: Read M0'
	 *   2) Compare M0 and M0'
	 *   if Rx is a receiver: switch to authentication step 3
	 *   3) Enable encryption / auto Ri check / disable AV mute
	 *   if Rx is a repeater: switch to authentication step 2
	 *   3) Get M0 from HDMI IP and store it for further processing (V)
	 *   4) Enable encryption / auto Ri check / auto BCAPS RDY polling
	 *      Disable AV mute
	 */

	HDCP_DBG("hdcp_lib_step1_r0_check() %u", jiffies_to_msecs(jiffies));

	status = hdcp_lib_r0_check();
	if (status < 0)
		return status;

	/* Authentication 1st step done */

	/* Now prepare 2nd step authentication in case of RX repeater and
	 * enable encryption / Ri check
	 */

	if (hdcp.pending_disable)
		return -HDCP_CANCELLED_AUTH;

	if (hdcp_lib_check_repeater_bit_in_tx()) {
		status = hdcp_user_space_task(HDCP_EVENT_STEP1);
		/* Wait for user space */
		if (status) {
			HDCP_ERR("HDCP: omap4_secure_dispatcher M0 error"
					" %d\n", status);
			return -HDCP_AUTH_FAILURE;
		}

		HDCP_DBG("hdcp_lib_set_encryption() %u",
			 jiffies_to_msecs(jiffies));

		/* Enable encryption */
		hdcp_lib_set_encryption(HDCP_ENC_ON);

#ifdef _9032_AUTO_RI_
		/* Enable Auto Ri */
		hdcp_lib_auto_ri_check(true);
#endif

#ifdef _9032_BCAP_
		/* Enable automatic BCAPS polling */
		hdcp_lib_auto_bcaps_rdy_check(true);
#endif

		/* Now, IP waiting for BCAPS ready bit */
	} else {
		/* Receiver: enable encryption and auto Ri check */
		hdcp_lib_set_encryption(HDCP_ENC_ON);

#ifdef _9032_AUTO_RI_
		/* Enable Auto Ri */
		hdcp_lib_auto_ri_check(true);
#endif

	}

	/* Clear AV mute */
	hdcp_lib_set_av_mute(AV_MUTE_CLEAR);

	return HDCP_OK;
}

/*-----------------------------------------------------------------------------
 * Function: hdcp_lib_step2
 *-----------------------------------------------------------------------------
 */
int hdcp_lib_step2(void)
{
	/* HDCP authentication steps:
	 *   1) Disable auto Ri check
	 *   2) DDC: read BStatus (nb of devices, MAX_DEV
	 */

	u8 bstatus[2];
	int status = HDCP_OK;

	HDCP_DBG("hdcp_lib_step2() %u", jiffies_to_msecs(jiffies));

#ifdef _9032_AUTO_RI_
	/* Disable Auto Ri */
	hdcp_lib_auto_ri_check(false);
#endif

	/* DDC: Read Bstatus (1st byte) from Rx */
	if (hdcp_ddc_read(DDC_BSTATUS_LEN, DDC_BSTATUS_ADDR, bstatus))
		return -HDCP_DDC_ERROR;

	/* Get KSV list size */
	HDCP_DBG("KSV list size: %d", bstatus[0] & DDC_BSTATUS0_DEV_COUNT);
	sha_input.byte_counter = (bstatus[0] & DDC_BSTATUS0_DEV_COUNT) * 5;

	/* Check BStatus topology errors */
	if (bstatus[0] & DDC_BSTATUS0_MAX_DEVS) {
		HDCP_DBG("MAX_DEV_EXCEEDED set");
		return -HDCP_AUTH_FAILURE;
	}

	if (bstatus[1] & DDC_BSTATUS1_MAX_CASC) {
		HDCP_DBG("MAX_CASCADE_EXCEEDED set");
		return -HDCP_AUTH_FAILURE;
	}

	HDCP_DBG("Retrieving KSV list...");

	/* Clear all SHA input data */
	/* TODO: should be done earlier at HDCP init */
	memset(sha_input.data, 0, MAX_SHA_DATA_SIZE);

	if (hdcp.pending_disable)
		return -HDCP_CANCELLED_AUTH;

	/* DDC: read KSV list */
	if (sha_input.byte_counter) {
		if (hdcp_ddc_read(sha_input.byte_counter, DDC_KSV_FIFO_ADDR,
				  (u8 *)&sha_input.data))
			return -HDCP_DDC_ERROR;
	}

	/* Read and add Bstatus */
	if (hdcp_lib_sha_bstatus(&sha_input))
		return -HDCP_DDC_ERROR;

	if (hdcp.pending_disable)
		return -HDCP_CANCELLED_AUTH;

	/* Read V' */
	if (hdcp_ddc_read(DDC_V_LEN, DDC_V_ADDR, sha_input.vprime))
		return -HDCP_DDC_ERROR;

	if (hdcp.pending_disable)
		return -HDCP_CANCELLED_AUTH;

	status = hdcp_user_space_task(HDCP_EVENT_STEP2);
	/* Wait for user space */
	if (status) {
		HDCP_ERR("HDCP: omap4_secure_dispatcher"
			 "CHECH_V error %d\n", status);
		return -HDCP_AUTH_FAILURE;
	}

	if (status == HDCP_OK) {
		/* Re-enable Ri check */
#ifdef _9032_AUTO_RI_
		hdcp_lib_auto_ri_check(true);
#endif
	}

	return status;
}
