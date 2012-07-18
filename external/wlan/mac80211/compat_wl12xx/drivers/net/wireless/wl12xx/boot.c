/*
 * This file is part of wl1271
 *
 * Copyright (C) 2008-2010 Nokia Corporation
 *
 * Contact: Luciano Coelho <luciano.coelho@nokia.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#include <linux/slab.h>
#include <linux/wl12xx.h>
#include <linux/export.h>

#include "debug.h"
#include "acx.h"
#include "reg.h"
#include "boot.h"
#include "io.h"
#include "event.h"
#include "rx.h"

static int wl1271_boot_set_ecpu_ctrl(struct wl1271 *wl, u32 flag)
{
	u32 cpu_ctrl;
	int ret;

	/* 10.5.0 run the firmware (I) */
	ret = wl1271_read32(wl, ACX_REG_ECPU_CONTROL, &cpu_ctrl);
	if (ret < 0)
		goto out;

	/* 10.5.1 run the firmware (II) */
	cpu_ctrl |= flag;
	ret = wl1271_write32(wl, ACX_REG_ECPU_CONTROL, cpu_ctrl);

out:
	return ret;
}

static unsigned int wl12xx_get_fw_ver_quirks(struct wl1271 *wl)
{
	unsigned int quirks = 0;
	unsigned int *fw_ver = wl->chip.fw_ver;

	/* Only new station firmwares support routing fw logs to the host */
	if ((fw_ver[FW_VER_IF_TYPE] == FW_VER_IF_TYPE_STA) &&
	    (fw_ver[FW_VER_MINOR] < FW_VER_MINOR_FWLOG_STA_MIN))
		quirks |= WL12XX_QUIRK_FWLOG_NOT_IMPLEMENTED;

	/* This feature is not yet supported for AP mode */
	if (fw_ver[FW_VER_IF_TYPE] == FW_VER_IF_TYPE_AP)
		quirks |= WL12XX_QUIRK_FWLOG_NOT_IMPLEMENTED;

	return quirks;
}

static void wl1271_parse_fw_ver(struct wl1271 *wl)
{
	int ret;

	ret = sscanf(wl->chip.fw_ver_str + 4, "%u.%u.%u.%u.%u",
		     &wl->chip.fw_ver[0], &wl->chip.fw_ver[1],
		     &wl->chip.fw_ver[2], &wl->chip.fw_ver[3],
		     &wl->chip.fw_ver[4]);

	if (ret != 5) {
		wl1271_warning("fw version incorrect value");
		memset(wl->chip.fw_ver, 0, sizeof(wl->chip.fw_ver));
		return;
	}

	/* Check if any quirks are needed with older fw versions */
	wl->quirks |= wl12xx_get_fw_ver_quirks(wl);
}

static int wl1271_boot_fw_version(struct wl1271 *wl)
{
	struct wl1271_static_data static_data;
	int ret;

	ret = wl1271_read(wl, wl->cmd_box_addr, &static_data,
			  sizeof(static_data), false);
	if (ret < 0)
		return ret;

	strncpy(wl->chip.fw_ver_str, static_data.fw_version,
		sizeof(wl->chip.fw_ver_str));

	/* make sure the string is NULL-terminated */
	wl->chip.fw_ver_str[sizeof(wl->chip.fw_ver_str) - 1] = '\0';

	wl1271_parse_fw_ver(wl);

	return 0;
}

static int wl1271_boot_upload_firmware_chunk(struct wl1271 *wl, void *buf,
					     size_t fw_data_len, u32 dest)
{
	struct wl1271_partition_set partition;
	int addr, chunk_num, partition_limit;
	u8 *p, *chunk;
	int ret;

	/* whal_FwCtrl_LoadFwImageSm() */

	wl1271_debug(DEBUG_BOOT, "starting firmware upload");

	wl1271_debug(DEBUG_BOOT, "fw_data_len %zd chunk_size %d",
		     fw_data_len, CHUNK_SIZE);

	if ((fw_data_len % 4) != 0) {
		wl1271_error("firmware length not multiple of four");
		return -EIO;
	}

	chunk = kmalloc(CHUNK_SIZE, GFP_KERNEL);
	if (!chunk) {
		wl1271_error("allocation for firmware upload chunk failed");
		return -ENOMEM;
	}

	memcpy(&partition, &wl12xx_part_table[PART_DOWN], sizeof(partition));
	partition.mem.start = dest;
	ret = wl1271_set_partition(wl, &partition);
	if (ret < 0)
		goto out;

	/* 10.1 set partition limit and chunk num */
	chunk_num = 0;
	partition_limit = wl12xx_part_table[PART_DOWN].mem.size;

	while (chunk_num < fw_data_len / CHUNK_SIZE) {
		/* 10.2 update partition, if needed */
		addr = dest + (chunk_num + 2) * CHUNK_SIZE;
		if (addr > partition_limit) {
			addr = dest + chunk_num * CHUNK_SIZE;
			partition_limit = chunk_num * CHUNK_SIZE +
				wl12xx_part_table[PART_DOWN].mem.size;
			partition.mem.start = addr;
			ret = wl1271_set_partition(wl, &partition);
			if (ret < 0)
				goto out;
		}

		/* 10.3 upload the chunk */
		addr = dest + chunk_num * CHUNK_SIZE;
		p = buf + chunk_num * CHUNK_SIZE;
		memcpy(chunk, p, CHUNK_SIZE);
		wl1271_debug(DEBUG_BOOT, "uploading fw chunk 0x%p to 0x%x",
			     p, addr);
		ret = wl1271_write(wl, addr, chunk, CHUNK_SIZE, false);
		if (ret < 0)
			goto out;

		chunk_num++;
	}

	/* 10.4 upload the last chunk */
	addr = dest + chunk_num * CHUNK_SIZE;
	p = buf + chunk_num * CHUNK_SIZE;
	memcpy(chunk, p, fw_data_len % CHUNK_SIZE);
	wl1271_debug(DEBUG_BOOT, "uploading fw last chunk (%zd B) 0x%p to 0x%x",
		     fw_data_len % CHUNK_SIZE, p, addr);
	ret = wl1271_write(wl, addr, chunk, fw_data_len % CHUNK_SIZE, false);

out:
	kfree(chunk);
	return ret;
}

static int wl1271_boot_upload_firmware(struct wl1271 *wl)
{
	u32 chunks, addr, len;
	int ret = 0;
	u8 *fw;

	fw = wl->fw;
	chunks = be32_to_cpup((__be32 *) fw);
	fw += sizeof(u32);

	wl1271_debug(DEBUG_BOOT, "firmware chunks to be uploaded: %u", chunks);

	while (chunks--) {
		addr = be32_to_cpup((__be32 *) fw);
		fw += sizeof(u32);
		len = be32_to_cpup((__be32 *) fw);
		fw += sizeof(u32);

		if (len > 300000) {
			wl1271_info("firmware chunk too long: %u", len);
			return -EINVAL;
		}
		wl1271_debug(DEBUG_BOOT, "chunk %d addr 0x%x len %u",
			     chunks, addr, len);
		ret = wl1271_boot_upload_firmware_chunk(wl, fw, len, addr);
		if (ret != 0)
			break;
		fw += len;
	}

	return ret;
}

static int wl1271_boot_upload_nvs(struct wl1271 *wl)
{
	size_t nvs_len, burst_len;
	int i;
	u32 dest_addr, val;
	u8 *nvs_ptr, *nvs_aligned;
	int ret;

	if (wl->nvs == NULL)
		return -ENODEV;

	if (wl->chip.id == CHIP_ID_1283_PG20) {
		struct wl128x_nvs_file *nvs = (struct wl128x_nvs_file *)wl->nvs;

		if (wl->nvs_len == sizeof(struct wl128x_nvs_file)) {
			if (nvs->general_params.dual_mode_select)
				wl->enable_11a = true;
		} else {
			wl1271_error("nvs size is not as expected: %zu != %zu",
				     wl->nvs_len,
				     sizeof(struct wl128x_nvs_file));
			kfree(wl->nvs);
			wl->nvs = NULL;
			wl->nvs_len = 0;
			return -EILSEQ;
		}

		/* only the first part of the NVS needs to be uploaded */
		nvs_len = sizeof(nvs->nvs);
		nvs_ptr = (u8 *)nvs->nvs;

	} else {
		struct wl1271_nvs_file *nvs =
			(struct wl1271_nvs_file *)wl->nvs;
		/*
		 * FIXME: the LEGACY NVS image support (NVS's missing the 5GHz
		 * band configurations) can be removed when those NVS files stop
		 * floating around.
		 */
		if (wl->nvs_len == sizeof(struct wl1271_nvs_file) ||
		    wl->nvs_len == WL1271_INI_LEGACY_NVS_FILE_SIZE) {
			if (nvs->general_params.dual_mode_select)
				wl->enable_11a = true;
		}

		if (wl->nvs_len != sizeof(struct wl1271_nvs_file) &&
		    (wl->nvs_len != WL1271_INI_LEGACY_NVS_FILE_SIZE ||
		     wl->enable_11a)) {
			wl1271_error("nvs size is not as expected: %zu != %zu",
				wl->nvs_len, sizeof(struct wl1271_nvs_file));
			kfree(wl->nvs);
			wl->nvs = NULL;
			wl->nvs_len = 0;
			return -EILSEQ;
		}

		/* only the first part of the NVS needs to be uploaded */
		nvs_len = sizeof(nvs->nvs);
		nvs_ptr = (u8 *) nvs->nvs;
	}

	/* update current MAC address to NVS */
	nvs_ptr[11] = wl->addresses[0].addr[0];
	nvs_ptr[10] = wl->addresses[0].addr[1];
	nvs_ptr[6] = wl->addresses[0].addr[2];
	nvs_ptr[5] = wl->addresses[0].addr[3];
	nvs_ptr[4] = wl->addresses[0].addr[4];
	nvs_ptr[3] = wl->addresses[0].addr[5];

	/*
	 * Layout before the actual NVS tables:
	 * 1 byte : burst length.
	 * 2 bytes: destination address.
	 * n bytes: data to burst copy.
	 *
	 * This is ended by a 0 length, then the NVS tables.
	 */

	/* FIXME: Do we need to check here whether the LSB is 1? */
	while (nvs_ptr[0]) {
		burst_len = nvs_ptr[0];
		dest_addr = (nvs_ptr[1] & 0xfe) | ((u32)(nvs_ptr[2] << 8));

		/*
		 * Due to our new wl1271_translate_reg_addr function,
		 * we need to add the REGISTER_BASE to the destination
		 */
		dest_addr += REGISTERS_BASE;

		/* We move our pointer to the data */
		nvs_ptr += 3;

		for (i = 0; i < burst_len; i++) {
			if (nvs_ptr + 3 >= (u8 *) wl->nvs + nvs_len)
				goto out_badnvs;

			val = (nvs_ptr[0] | (nvs_ptr[1] << 8)
			       | (nvs_ptr[2] << 16) | (nvs_ptr[3] << 24));

			wl1271_debug(DEBUG_BOOT,
				     "nvs burst write 0x%x: 0x%x",
				     dest_addr, val);
			ret = wl1271_write32(wl, dest_addr, val);
			if (ret < 0)
				return ret;

			nvs_ptr += 4;
			dest_addr += 4;
		}

		if (nvs_ptr >= (u8 *) wl->nvs + nvs_len)
			goto out_badnvs;
	}

	/*
	 * We've reached the first zero length, the first NVS table
	 * is located at an aligned offset which is at least 7 bytes further.
	 * NOTE: The wl->nvs->nvs element must be first, in order to
	 * simplify the casting, we assume it is at the beginning of
	 * the wl->nvs structure.
	 */
	nvs_ptr = (u8 *)wl->nvs +
			ALIGN(nvs_ptr - (u8 *)wl->nvs + 7, 4);

	if (nvs_ptr >= (u8 *) wl->nvs + nvs_len)
		goto out_badnvs;

	nvs_len -= nvs_ptr - (u8 *)wl->nvs;

	/* Now we must set the partition correctly */
	ret = wl1271_set_partition(wl, &wl12xx_part_table[PART_WORK]);
	if (ret < 0)
		return ret;

	/* Copy the NVS tables to a new block to ensure alignment */
	nvs_aligned = kmemdup(nvs_ptr, nvs_len, GFP_KERNEL);
	if (!nvs_aligned)
		return -ENOMEM;

	/* And finally we upload the NVS tables */
	ret = wl1271_write(wl, CMD_MBOX_ADDRESS, nvs_aligned, nvs_len, false);
	if (ret < 0)
		return ret;

	kfree(nvs_aligned);
	return 0;

out_badnvs:
	wl1271_error("nvs data is malformed");
	return -EILSEQ;
}

static int wl1271_boot_enable_interrupts(struct wl1271 *wl)
{
	int ret;

	wl1271_enable_interrupts(wl);
	ret = wl1271_write32(wl, ACX_REG_INTERRUPT_MASK,
			     WL1271_ACX_INTR_ALL & ~(WL1271_INTR_MASK));
	if (ret < 0)
		goto out;

	ret = wl1271_write32(wl, HI_CFG, HI_CFG_DEF_VAL);

out:
	return ret;
}

static int wl1271_boot_soft_reset(struct wl1271 *wl)
{
	unsigned long timeout;
	u32 boot_data;
	int ret;

	/* perform soft reset */
	ret = wl1271_write32(wl, ACX_REG_SLV_SOFT_RESET,
			     ACX_SLV_SOFT_RESET_BIT);
	if (ret < 0)
		goto out;

	/* SOFT_RESET is self clearing */
	timeout = jiffies + usecs_to_jiffies(SOFT_RESET_MAX_TIME);
	while (1) {
		ret = wl1271_read32(wl, ACX_REG_SLV_SOFT_RESET, &boot_data);
		if (ret < 0)
			goto out;

		wl1271_debug(DEBUG_BOOT, "soft reset bootdata 0x%x", boot_data);
		if ((boot_data & ACX_SLV_SOFT_RESET_BIT) == 0)
			break;

		if (time_after(jiffies, timeout)) {
			/* 1.2 check pWhalBus->uSelfClearTime if the
			 * timeout was reached */
			wl1271_error("soft reset timeout");
			ret = -ETIMEDOUT;
			goto out;
		}

		udelay(SOFT_RESET_STALL_TIME);
	}

	/* disable Rx/Tx */
	ret = wl1271_write32(wl, ENABLE, 0x0);
	if (ret < 0)
		goto out;

	/* disable auto calibration on start*/
	ret = wl1271_write32(wl, SPARE_A2, 0xffff);

out:
	return ret;
}

static int wl1271_boot_run_firmware(struct wl1271 *wl)
{
	int loop, ret;
	u32 chip_id, intr;

	ret = wl1271_boot_set_ecpu_ctrl(wl, ECPU_CONTROL_HALT);
	if (ret < 0)
		goto out;

	ret = wl1271_read32(wl, CHIP_ID_B, &chip_id);
	if (ret < 0)
		goto out;

	wl1271_debug(DEBUG_BOOT, "chip id after firmware boot: 0x%x", chip_id);

	if (chip_id != wl->chip.id) {
		wl1271_error("chip id doesn't match after firmware boot");
		ret = -EIO;
		goto out;
	}

	/* wait for init to complete */
	loop = 0;
	while (loop++ < INIT_LOOP) {
		udelay(INIT_LOOP_DELAY);
		ret = wl1271_read32(wl, ACX_REG_INTERRUPT_NO_CLEAR, &intr);
		if (ret < 0)
			goto out;

		if (intr == 0xffffffff) {
			wl1271_error("error reading hardware complete "
				     "init indication");
			ret = -EIO;
			goto out;
		}
		/* check that ACX_INTR_INIT_COMPLETE is enabled */
		else if (intr & WL1271_ACX_INTR_INIT_COMPLETE) {
			ret = wl1271_write32(wl, ACX_REG_INTERRUPT_ACK,
					     WL1271_ACX_INTR_INIT_COMPLETE);
			if (ret < 0)
				goto out;
			break;
		}
	}

	if (loop > INIT_LOOP) {
		wl1271_error("timeout waiting for the hardware to "
			     "complete initialization");
		ret = -EIO;
		goto out;
	}

	/* get hardware config command mail box */
	ret = wl1271_read32(wl, REG_COMMAND_MAILBOX_PTR, &wl->cmd_box_addr);
	if (ret < 0)
		goto out;

	/* get hardware config event mail box */
	ret = wl1271_read32(wl, REG_EVENT_MAILBOX_PTR, &wl->event_box_addr);
	if (ret < 0)
		goto out;

	/* set the working partition to its "running" mode offset */
	ret = wl1271_set_partition(wl, &wl12xx_part_table[PART_WORK]);
	if (ret < 0)
		goto out;

	wl1271_debug(DEBUG_MAILBOX, "cmd_box_addr 0x%x event_box_addr 0x%x",
		     wl->cmd_box_addr, wl->event_box_addr);

	ret = wl1271_boot_fw_version(wl);
	if (ret < 0)
		goto out;

	/*
	 * in case of full asynchronous mode the firmware event must be
	 * ready to receive event from the command mailbox
	 */

	/* unmask required mbox events  */
	wl->event_mask = BSS_LOSE_EVENT_ID |
		SCAN_COMPLETE_EVENT_ID |
		ROLE_STOP_COMPLETE_EVENT_ID |
		RSSI_SNR_TRIGGER_0_EVENT_ID |
		PSPOLL_DELIVERY_FAILURE_EVENT_ID |
		SOFT_GEMINI_SENSE_EVENT_ID |
		PERIODIC_SCAN_REPORT_EVENT_ID |
		PERIODIC_SCAN_COMPLETE_EVENT_ID |
		DUMMY_PACKET_EVENT_ID |
		PEER_REMOVE_COMPLETE_EVENT_ID |
		BA_SESSION_RX_CONSTRAINT_EVENT_ID |
		REMAIN_ON_CHANNEL_COMPLETE_EVENT_ID |
		INACTIVE_STA_EVENT_ID |
		MAX_TX_RETRY_EVENT_ID |
		CHANNEL_SWITCH_COMPLETE_EVENT_ID;

	ret = wl1271_event_unmask(wl);
	if (ret < 0) {
		wl1271_error("EVENT mask setting failed");
		goto out;
	}

	ret = wl1271_event_mbox_config(wl);

	/* firmware startup completed */
out:
	return ret;
}

static int wl1271_boot_write_irq_polarity(struct wl1271 *wl)
{
	u16 polarity;
	int ret;

	ret = wl1271_top_reg_read(wl, OCP_REG_POLARITY, &polarity);
	if (ret < 0)
		return ret;

	/* We use HIGH polarity, so unset the LOW bit */
	polarity &= ~POLARITY_LOW;
	ret = wl1271_top_reg_write(wl, OCP_REG_POLARITY, polarity);

	return ret;
}

static int wl128x_switch_tcxo_to_fref(struct wl1271 *wl)
{
	u16 spare_reg;
	int ret;

	/* Mask bits [2] & [8:4] in the sys_clk_cfg register */
	ret = wl1271_top_reg_read(wl, WL_SPARE_REG, &spare_reg);
	if (ret < 0)
		return ret;

	if (spare_reg == 0xFFFF)
		return -EFAULT;
	spare_reg |= (BIT(3) | BIT(5) | BIT(6));
	ret = wl1271_top_reg_write(wl, WL_SPARE_REG, spare_reg);
	if (ret < 0)
		return ret;

	/* Enable FREF_CLK_REQ & mux MCS and coex PLLs to FREF */
	ret = wl1271_top_reg_write(wl, SYS_CLK_CFG_REG,
				   WL_CLK_REQ_TYPE_PG2 | MCS_PLL_CLK_SEL_FREF);
	if (ret < 0)
		return ret;

	/* Delay execution for 15msec, to let the HW settle */
	mdelay(15);

	return 0;
}

static bool wl128x_is_tcxo_valid(struct wl1271 *wl)
{
	u16 tcxo_detection;
	int ret;

	ret = wl1271_top_reg_read(wl, TCXO_CLK_DETECT_REG, &tcxo_detection);
	if (ret < 0)
		return false;

	if (tcxo_detection & TCXO_DET_FAILED)
		return false;

	return true;
}

static bool wl128x_is_fref_valid(struct wl1271 *wl)
{
	u16 fref_detection;
	int ret;

	ret = wl1271_top_reg_read(wl, FREF_CLK_DETECT_REG, &fref_detection);
	if (ret < 0)
		return false;

	if (fref_detection & FREF_CLK_DETECT_FAIL)
		return false;

	return true;
}

static int wl128x_manually_configure_mcs_pll(struct wl1271 *wl)
{
	int ret;

	ret = wl1271_top_reg_write(wl, MCS_PLL_M_REG, MCS_PLL_M_REG_VAL);
	if (ret < 0)
		goto out;

	ret = wl1271_top_reg_write(wl, MCS_PLL_N_REG, MCS_PLL_N_REG_VAL);
	if (ret < 0)
		goto out;

	ret = wl1271_top_reg_write(wl, MCS_PLL_CONFIG_REG,
				   MCS_PLL_CONFIG_REG_VAL);

out:
	return ret;
}

static int wl128x_configure_mcs_pll(struct wl1271 *wl, int clk)
{
	u16 spare_reg;
	u16 pll_config;
	u8 input_freq;
	int ret;

	/* Mask bits [3:1] in the sys_clk_cfg register */
	ret = wl1271_top_reg_read(wl, WL_SPARE_REG, &spare_reg);
	if (ret < 0)
		return ret;

	if (spare_reg == 0xFFFF)
		return -EFAULT;
	spare_reg |= BIT(2);

	ret = wl1271_top_reg_write(wl, WL_SPARE_REG, spare_reg);
	if (ret < 0)
		return ret;

	/* Handle special cases of the TCXO clock */
	if (wl->tcxo_clock == WL12XX_TCXOCLOCK_16_8 ||
	    wl->tcxo_clock == WL12XX_TCXOCLOCK_33_6)
		return wl128x_manually_configure_mcs_pll(wl);

	/* Set the input frequency according to the selected clock source */
	input_freq = (clk & 1) + 1;

	ret = wl1271_top_reg_read(wl, MCS_PLL_CONFIG_REG, &pll_config);
	if (ret < 0)
		return ret;

	if (pll_config == 0xFFFF)
		return -EFAULT;
	pll_config |= (input_freq << MCS_SEL_IN_FREQ_SHIFT);
	pll_config |= MCS_PLL_ENABLE_HP;

	ret = wl1271_top_reg_write(wl, MCS_PLL_CONFIG_REG, pll_config);

	return ret;
}

/*
 * WL128x has two clocks input - TCXO and FREF.
 * TCXO is the main clock of the device, while FREF is used to sync
 * between the GPS and the cellular modem.
 * In cases where TCXO is 32.736MHz or 16.368MHz, the FREF will be used
 * as the WLAN/BT main clock.
 */
int wl128x_boot_clk(struct wl1271 *wl, int *selected_clock)
{
	u16 sys_clk_cfg;
	int ret;

	/* For XTAL-only modes, FREF will be used after switching from TCXO */
	if (wl->ref_clock == WL12XX_REFCLOCK_26_XTAL ||
	    wl->ref_clock == WL12XX_REFCLOCK_38_XTAL) {
		if (!wl128x_switch_tcxo_to_fref(wl))
			return -EINVAL;
		goto fref_clk;
	}

	/* Query the HW, to determine which clock source we should use */
	ret = wl1271_top_reg_read(wl, SYS_CLK_CFG_REG, &sys_clk_cfg);
	if (ret < 0)
		return ret;

	if (sys_clk_cfg == 0xFFFF)
		return -EINVAL;
	if (sys_clk_cfg & PRCM_CM_EN_MUX_WLAN_FREF)
		goto fref_clk;

	/* If TCXO is either 32.736MHz or 16.368MHz, switch to FREF */
	if (wl->tcxo_clock == WL12XX_TCXOCLOCK_16_368 ||
	    wl->tcxo_clock == WL12XX_TCXOCLOCK_32_736) {
		if (!wl128x_switch_tcxo_to_fref(wl))
			return -EINVAL;
		goto fref_clk;
	}

	/* TCXO clock is selected */
	if (!wl128x_is_tcxo_valid(wl))
		return -EINVAL;
	*selected_clock = wl->tcxo_clock;
	goto config_mcs_pll;

fref_clk:
	/* FREF clock is selected */
	if (!wl128x_is_fref_valid(wl))
		return -EINVAL;
	*selected_clock = wl->ref_clock;

config_mcs_pll:
	return wl128x_configure_mcs_pll(wl, *selected_clock);
}

int wl127x_boot_clk(struct wl1271 *wl)
{
	u32 pause;
	u32 clk;
	int ret;

	if (WL127X_PG_GET_MAJOR(wl->hw_pg_ver) < 3)
		wl->quirks |= WL12XX_QUIRK_END_OF_TRANSACTION;

	if (wl->ref_clock == CONF_REF_CLK_19_2_E ||
	    wl->ref_clock == CONF_REF_CLK_38_4_E ||
	    wl->ref_clock == CONF_REF_CLK_38_4_M_XTAL)
		/* ref clk: 19.2/38.4/38.4-XTAL */
		clk = 0x3;
	else if (wl->ref_clock == CONF_REF_CLK_26_E ||
		 wl->ref_clock == CONF_REF_CLK_52_E)
		/* ref clk: 26/52 */
		clk = 0x5;
	else
		return -EINVAL;

	if (wl->ref_clock != CONF_REF_CLK_19_2_E) {
		u16 val;
		/* Set clock type (open drain) */
		ret = wl1271_top_reg_read(wl, OCP_REG_CLK_TYPE, &val);
		if (ret < 0)
			return ret;

		val &= FREF_CLK_TYPE_BITS;
		ret = wl1271_top_reg_write(wl, OCP_REG_CLK_TYPE, val);
		if (ret < 0)
			return ret;

		/* Set clock pull mode (no pull) */
		ret = wl1271_top_reg_read(wl, OCP_REG_CLK_PULL, &val);
		if (ret < 0)
			return ret;

		val |= NO_PULL;
		ret = wl1271_top_reg_write(wl, OCP_REG_CLK_PULL, val);
		if (ret < 0)
			return ret;
	} else {
		u16 val;
		/* Set clock polarity */
		ret = wl1271_top_reg_read(wl, OCP_REG_CLK_POLARITY, &val);
		if (ret < 0)
			return ret;

		val &= FREF_CLK_POLARITY_BITS;
		val |= CLK_REQ_OUTN_SEL;
		ret = wl1271_top_reg_write(wl, OCP_REG_CLK_POLARITY, val);
		if (ret < 0)
			return ret;
	}

	ret = wl1271_write32(wl, PLL_PARAMETERS, clk);
	if (ret < 0)
		goto out;

	ret = wl1271_read32(wl, PLL_PARAMETERS, &pause);
	if (ret < 0)
		goto out;

	wl1271_debug(DEBUG_BOOT, "pause1 0x%x", pause);

	pause &= ~(WU_COUNTER_PAUSE_VAL);
	pause |= WU_COUNTER_PAUSE_VAL;
	ret = wl1271_write32(wl, WU_COUNTER_PAUSE, pause);

out:
	return ret;
}

/* uploads NVS and firmware */
int wl1271_load_firmware(struct wl1271 *wl)
{
	int ret = 0;
	u32 tmp, clk;
	int selected_clock = -1;

	ret = wl12xx_init_pll_clock(wl, &selected_clock);
	if (ret < 0)
		goto out;

	ret = wl1271_set_partition(wl, &wl12xx_part_table[PART_DRPW]);
	if (ret < 0)
		goto out;

	/* Read-modify-write DRPW_SCRATCH_START register (see next state)
	   to be used by DRPw FW. The RTRIM value will be added by the FW
	   before taking DRPw out of reset */

	wl1271_debug(DEBUG_BOOT, "DRPW_SCRATCH_START %08x", DRPW_SCRATCH_START);
	ret = wl1271_read32(wl, DRPW_SCRATCH_START, &clk);
	if (ret < 0)
		goto out;

	wl1271_debug(DEBUG_BOOT, "clk2 0x%x", clk);

	if (wl->chip.id == CHIP_ID_1283_PG20) {
		clk |= ((selected_clock & 0x3) << 1) << 4;
	} else {
		clk |= (wl->ref_clock << 1) << 4;
	}

	ret = wl1271_write32(wl, DRPW_SCRATCH_START, clk);
	if (ret < 0)
		goto out;

	ret = wl1271_set_partition(wl, &wl12xx_part_table[PART_WORK]);
	if (ret < 0)
		goto out;

	/* Disable interrupts */
	ret = wl1271_write32(wl, ACX_REG_INTERRUPT_MASK, WL1271_ACX_INTR_ALL);
	if (ret < 0)
		goto out;

	ret = wl1271_boot_soft_reset(wl);
	if (ret < 0)
		goto out;

	/* 2. start processing NVS file */
	ret = wl1271_boot_upload_nvs(wl);
	if (ret < 0)
		goto out;

	/* write firmware's last address (ie. it's length) to
	 * ACX_EEPROMLESS_IND_REG */
	wl1271_debug(DEBUG_BOOT, "ACX_EEPROMLESS_IND_REG");

	ret = wl1271_write32(wl, ACX_EEPROMLESS_IND_REG,
			     ACX_EEPROMLESS_IND_REG);
	if (ret < 0)
		goto out;

	ret = wl1271_read32(wl, CHIP_ID_B, &tmp);
	if (ret < 0)
		goto out;

	wl1271_debug(DEBUG_BOOT, "chip id 0x%x", tmp);

	/* 6. read the EEPROM parameters */
	ret = wl1271_read32(wl, SCR_PAD2, &tmp);
	if (ret < 0)
		goto out;

	/* WL1271: The reference driver skips steps 7 to 10 (jumps directly
	 * to upload_fw) */

	if (wl->chip.id == CHIP_ID_1283_PG20) {
		ret = wl1271_top_reg_write(wl, SDIO_IO_DS, wl->conf.hci_io_ds);
		if (ret < 0)
			goto out;
	}

	ret = wl1271_boot_upload_firmware(wl);
	if (ret < 0)
		goto out;

	/* update loaded fw type */
	wl->fw_type = wl->saved_fw_type;
out:
	return ret;
}
EXPORT_SYMBOL_GPL(wl1271_load_firmware);

int wl1271_boot(struct wl1271 *wl)
{
	int ret;

	/* polarity must be set before the firmware is loaded */
	ret = wl1271_boot_write_irq_polarity(wl);
	if (ret < 0)
		goto out;

	/* upload NVS and firmware */
	ret = wl1271_load_firmware(wl);
	if (ret)
		return ret;

	/* 10.5 start firmware */
	ret = wl1271_boot_run_firmware(wl);
	if (ret < 0)
		goto out;

	ret = wl1271_write32(wl, ACX_REG_INTERRUPT_MASK,
			     WL1271_ACX_ALL_EVENTS_VECTOR);
	if (ret < 0)
		goto out;

	/* Enable firmware interrupts now */
	ret = wl1271_boot_enable_interrupts(wl);
	if (ret < 0)
		goto out;

	ret = wl1271_event_mbox_config(wl);

out:
	return ret;
}
