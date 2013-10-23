/*
 *  linux/drivers/mmc/core/mmc_ops.h
 *
 *  Copyright 2006-2007 Pierre Ossman
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 */

#include <linux/slab.h>
#include <linux/export.h>
#include <linux/types.h>
#include <linux/scatterlist.h>

#include <linux/mmc/host.h>
#include <linux/mmc/card.h>
#include <linux/mmc/mmc.h>

#include "core.h"
#include "mmc_ops.h"

static int _mmc_select_card(struct mmc_host *host, struct mmc_card *card)
{
	int err;
	struct mmc_command cmd = {0};

	BUG_ON(!host);

	cmd.opcode = MMC_SELECT_CARD;

	if (card) {
		cmd.arg = card->rca << 16;
		cmd.flags = MMC_RSP_R1 | MMC_CMD_AC;
	} else {
		cmd.arg = 0;
		cmd.flags = MMC_RSP_NONE | MMC_CMD_AC;
	}

	err = mmc_wait_for_cmd(host, &cmd, MMC_CMD_RETRIES);
	if (err)
		return err;

	return 0;
}

int mmc_select_card(struct mmc_card *card)
{
	BUG_ON(!card);

	return _mmc_select_card(card->host, card);
}

int mmc_deselect_cards(struct mmc_host *host)
{
	return _mmc_select_card(host, NULL);
}

int mmc_card_sleepawake(struct mmc_host *host, int sleep)
{
	struct mmc_command cmd = {0};
	struct mmc_card *card = host->card;
	int err;

	if (sleep)
		mmc_deselect_cards(host);

	cmd.opcode = MMC_SLEEP_AWAKE;
	cmd.arg = card->rca << 16;
	if (sleep)
		cmd.arg |= 1 << 15;

	cmd.flags = MMC_RSP_R1B | MMC_CMD_AC;
	err = mmc_wait_for_cmd(host, &cmd, 0);
	if (err)
		return err;

	/*
	 * If the host does not wait while the card signals busy, then we will
	 * will have to wait the sleep/awake timeout.  Note, we cannot use the
	 * SEND_STATUS command to poll the status because that command (and most
	 * others) is invalid while the card sleeps.
	 */
	if (!(host->caps & MMC_CAP_WAIT_WHILE_BUSY))
		mmc_delay(DIV_ROUND_UP(card->ext_csd.sa_timeout, 10000));

	if (!sleep)
		err = mmc_select_card(card);

	return err;
}

int mmc_go_idle(struct mmc_host *host)
{
	int err;
	struct mmc_command cmd = {0};

	/*
	 * Non-SPI hosts need to prevent chipselect going active during
	 * GO_IDLE; that would put chips into SPI mode.  Remind them of
	 * that in case of hardware that won't pull up DAT3/nCS otherwise.
	 *
	 * SPI hosts ignore ios.chip_select; it's managed according to
	 * rules that must accommodate non-MMC slaves which this layer
	 * won't even know about.
	 */
	if (!mmc_host_is_spi(host)) {
		mmc_set_chip_select(host, MMC_CS_HIGH);
		mmc_delay(1);
	}

	cmd.opcode = MMC_GO_IDLE_STATE;
	cmd.arg = 0;
	cmd.flags = MMC_RSP_SPI_R1 | MMC_RSP_NONE | MMC_CMD_BC;

	err = mmc_wait_for_cmd(host, &cmd, 0);

	mmc_delay(1);

	if (!mmc_host_is_spi(host)) {
		mmc_set_chip_select(host, MMC_CS_DONTCARE);
		mmc_delay(1);
	}

	host->use_spi_crc = 0;

	return err;
}

int mmc_send_op_cond(struct mmc_host *host, u32 ocr, u32 *rocr)
{
	struct mmc_command cmd = {0};
	int i, err = 0;

	BUG_ON(!host);

	cmd.opcode = MMC_SEND_OP_COND;
	cmd.arg = mmc_host_is_spi(host) ? 0 : ocr;
	cmd.flags = MMC_RSP_SPI_R1 | MMC_RSP_R3 | MMC_CMD_BCR;

	for (i = 100; i; i--) {
		err = mmc_wait_for_cmd(host, &cmd, 0);
		if (err)
			break;

		/* if we're just probing, do a single pass */
		if (ocr == 0)
			break;

		/* otherwise wait until reset completes */
		if (mmc_host_is_spi(host)) {
			if (!(cmd.resp[0] & R1_SPI_IDLE))
				break;
		} else {
			if (cmd.resp[0] & MMC_CARD_BUSY)
				break;
		}

		err = -ETIMEDOUT;

		mmc_delay(10);
	}

	if (rocr && !mmc_host_is_spi(host))
		*rocr = cmd.resp[0];

	return err;
}

int mmc_all_send_cid(struct mmc_host *host, u32 *cid)
{
	int err;
	struct mmc_command cmd = {0};

	BUG_ON(!host);
	BUG_ON(!cid);

	cmd.opcode = MMC_ALL_SEND_CID;
	cmd.arg = 0;
	cmd.flags = MMC_RSP_R2 | MMC_CMD_BCR;

	err = mmc_wait_for_cmd(host, &cmd, MMC_CMD_RETRIES);
	if (err)
		return err;

	memcpy(cid, cmd.resp, sizeof(u32) * 4);

	return 0;
}

int mmc_set_relative_addr(struct mmc_card *card)
{
	int err;
	struct mmc_command cmd = {0};

	BUG_ON(!card);
	BUG_ON(!card->host);

	cmd.opcode = MMC_SET_RELATIVE_ADDR;
	cmd.arg = card->rca << 16;
	cmd.flags = MMC_RSP_R1 | MMC_CMD_AC;

	err = mmc_wait_for_cmd(card->host, &cmd, MMC_CMD_RETRIES);
	if (err)
		return err;

	return 0;
}

static int
mmc_send_cxd_native(struct mmc_host *host, u32 arg, u32 *cxd, int opcode)
{
	int err;
	struct mmc_command cmd = {0};

	BUG_ON(!host);
	BUG_ON(!cxd);

	cmd.opcode = opcode;
	cmd.arg = arg;
	cmd.flags = MMC_RSP_R2 | MMC_CMD_AC;

	err = mmc_wait_for_cmd(host, &cmd, MMC_CMD_RETRIES);
	if (err)
		return err;

	memcpy(cxd, cmd.resp, sizeof(u32) * 4);

	return 0;
}

static int
mmc_send_cxd_data(struct mmc_card *card, struct mmc_host *host,
		u32 opcode, void *buf, unsigned len)
{
	struct mmc_request mrq = {NULL};
	struct mmc_command cmd = {0};
	struct mmc_data data = {0};
	struct scatterlist sg;
	void *data_buf;

	/* dma onto stack is unsafe/nonportable, but callers to this
	 * routine normally provide temporary on-stack buffers ...
	 */
	data_buf = kmalloc(len, GFP_KERNEL);
	if (data_buf == NULL)
		return -ENOMEM;

	mrq.cmd = &cmd;
	mrq.data = &data;

	cmd.opcode = opcode;
	cmd.arg = 0;

	/* NOTE HACK:  the MMC_RSP_SPI_R1 is always correct here, but we
	 * rely on callers to never use this with "native" calls for reading
	 * CSD or CID.  Native versions of those commands use the R2 type,
	 * not R1 plus a data block.
	 */
	cmd.flags = MMC_RSP_SPI_R1 | MMC_RSP_R1 | MMC_CMD_ADTC;

	data.blksz = len;
	data.blocks = 1;
	data.flags = MMC_DATA_READ;
	data.sg = &sg;
	data.sg_len = 1;

	sg_init_one(&sg, data_buf, len);

	if (opcode == MMC_SEND_CSD || opcode == MMC_SEND_CID) {
		/*
		 * The spec states that CSR and CID accesses have a timeout
		 * of 64 clock cycles.
		 */
		data.timeout_ns = 0;
		data.timeout_clks = 64;
	} else
		mmc_set_data_timeout(&data, card);

	mmc_wait_for_req(host, &mrq);

	memcpy(buf, data_buf, len);
	kfree(data_buf);

	if (cmd.error)
		return cmd.error;
	if (data.error)
		return data.error;

	return 0;
}

int mmc_send_csd(struct mmc_card *card, u32 *csd)
{
	int ret, i;

	if (!mmc_host_is_spi(card->host))
		return mmc_send_cxd_native(card->host, card->rca << 16,
				csd, MMC_SEND_CSD);

	ret = mmc_send_cxd_data(card, card->host, MMC_SEND_CSD, csd, 16);
	if (ret)
		return ret;

	for (i = 0;i < 4;i++)
		csd[i] = be32_to_cpu(csd[i]);

	return 0;
}

int mmc_send_cid(struct mmc_host *host, u32 *cid)
{
	int ret, i;

	if (!mmc_host_is_spi(host)) {
		if (!host->card)
			return -EINVAL;
		return mmc_send_cxd_native(host, host->card->rca << 16,
				cid, MMC_SEND_CID);
	}

	ret = mmc_send_cxd_data(NULL, host, MMC_SEND_CID, cid, 16);
	if (ret)
		return ret;

	for (i = 0;i < 4;i++)
		cid[i] = be32_to_cpu(cid[i]);

	return 0;
}

int mmc_send_ext_csd(struct mmc_card *card, u8 *ext_csd)
{
	return mmc_send_cxd_data(card, card->host, MMC_SEND_EXT_CSD,
			ext_csd, 512);
}

int mmc_spi_read_ocr(struct mmc_host *host, int highcap, u32 *ocrp)
{
	struct mmc_command cmd = {0};
	int err;

	cmd.opcode = MMC_SPI_READ_OCR;
	cmd.arg = highcap ? (1 << 30) : 0;
	cmd.flags = MMC_RSP_SPI_R3;

	err = mmc_wait_for_cmd(host, &cmd, 0);

	*ocrp = cmd.resp[1];
	return err;
}

int mmc_spi_set_crc(struct mmc_host *host, int use_crc)
{
	struct mmc_command cmd = {0};
	int err;

	cmd.opcode = MMC_SPI_CRC_ON_OFF;
	cmd.flags = MMC_RSP_SPI_R1;
	cmd.arg = use_crc;

	err = mmc_wait_for_cmd(host, &cmd, 0);
	if (!err)
		host->use_spi_crc = use_crc;
	return err;
}

/**
 *	mmc_switch - modify EXT_CSD register
 *	@card: the MMC card associated with the data transfer
 *	@set: cmd set values
 *	@index: EXT_CSD register index
 *	@value: value to program into EXT_CSD register
 *	@timeout_ms: timeout (ms) for operation performed by register write,
 *                   timeout of zero implies maximum possible timeout
 *
 *	Modifies the EXT_CSD register for selected card.
 */
int mmc_switch(struct mmc_card *card, u8 set, u8 index, u8 value,
	       unsigned int timeout_ms)
{
	int err;
	struct mmc_command cmd = {0};
	u32 status;

	BUG_ON(!card);
	BUG_ON(!card->host);

	cmd.opcode = MMC_SWITCH;
	cmd.arg = (MMC_SWITCH_MODE_WRITE_BYTE << 24) |
		  (index << 16) |
		  (value << 8) |
		  set;
	cmd.flags = MMC_RSP_SPI_R1B | MMC_RSP_R1B | MMC_CMD_AC;
	cmd.cmd_timeout_ms = timeout_ms;

	err = mmc_wait_for_cmd(card->host, &cmd, MMC_CMD_RETRIES);
	if (err)
		return err;

#ifdef CONFIG_MACH_OMAP4_BOWSER
	/* special case for Power Off Notification.
	 * It must be the last command before power off, otherwise
	 * PON state will be canceled by device. */
	if (set == EXT_CSD_CMD_SET_NORMAL &&
		index == EXT_CSD_POWER_OFF_NOTIFICATION &&
		(value  == 2 || value == 3)) {
		printk(KERN_DEBUG "MMC%d: Bypassing CMD13 for PON\n",
				card->host->index);
		if (!(card->host->caps & MMC_CAP_WAIT_WHILE_BUSY))
			msleep(timeout_ms);
		return 0;
	}
#endif

	/* Must check status to be sure of no errors */
	do {
		err = mmc_send_status(card, &status);
		if (err)
			return err;
		if (card->host->caps & MMC_CAP_WAIT_WHILE_BUSY)
			break;
		if (mmc_host_is_spi(card->host))
			break;
	} while (R1_CURRENT_STATE(status) == R1_STATE_PRG);

	if (mmc_host_is_spi(card->host)) {
		if (status & R1_SPI_ILLEGAL_COMMAND)
			return -EBADMSG;
	} else {
		if (status & 0xFDFFA000)
			pr_warning("%s: unexpected status %#x after "
			       "switch", mmc_hostname(card->host), status);
		if (status & R1_SWITCH_ERROR)
			return -EBADMSG;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(mmc_switch);

int mmc_send_status(struct mmc_card *card, u32 *status)
{
	int err;
	struct mmc_command cmd = {0};

	BUG_ON(!card);
	BUG_ON(!card->host);

	cmd.opcode = MMC_SEND_STATUS;
	if (!mmc_host_is_spi(card->host))
		cmd.arg = card->rca << 16;
	cmd.flags = MMC_RSP_SPI_R2 | MMC_RSP_R1 | MMC_CMD_AC;

	err = mmc_wait_for_cmd(card->host, &cmd, MMC_CMD_RETRIES);
	if (err)
		return err;

	/* NOTE: callers are required to understand the difference
	 * between "native" and SPI format status words!
	 */
	if (status)
		*status = cmd.resp[0];

	return 0;
}

static int
mmc_send_bus_test(struct mmc_card *card, struct mmc_host *host, u8 opcode,
		  u8 len)
{
	struct mmc_request mrq = {NULL};
	struct mmc_command cmd = {0};
	struct mmc_data data = {0};
	struct scatterlist sg;
	u8 *data_buf;
	u8 *test_buf;
	int i, err;
	static u8 testdata_8bit[8] = { 0x55, 0xaa, 0, 0, 0, 0, 0, 0 };
	static u8 testdata_4bit[4] = { 0x5a, 0, 0, 0 };

	/* dma onto stack is unsafe/nonportable, but callers to this
	 * routine normally provide temporary on-stack buffers ...
	 */
	data_buf = kmalloc(len, GFP_KERNEL);
	if (!data_buf)
		return -ENOMEM;

	if (len == 8)
		test_buf = testdata_8bit;
	else if (len == 4)
		test_buf = testdata_4bit;
	else {
		pr_err("%s: Invalid bus_width %d\n",
		       mmc_hostname(host), len);
		kfree(data_buf);
		return -EINVAL;
	}

	if (opcode == MMC_BUS_TEST_W)
		memcpy(data_buf, test_buf, len);

	mrq.cmd = &cmd;
	mrq.data = &data;
	cmd.opcode = opcode;
	cmd.arg = 0;

	/* NOTE HACK:  the MMC_RSP_SPI_R1 is always correct here, but we
	 * rely on callers to never use this with "native" calls for reading
	 * CSD or CID.  Native versions of those commands use the R2 type,
	 * not R1 plus a data block.
	 */
	cmd.flags = MMC_RSP_SPI_R1 | MMC_RSP_R1 | MMC_CMD_ADTC;

	data.blksz = len;
	data.blocks = 1;
	if (opcode == MMC_BUS_TEST_R)
		data.flags = MMC_DATA_READ;
	else
		data.flags = MMC_DATA_WRITE;

	data.sg = &sg;
	data.sg_len = 1;
	sg_init_one(&sg, data_buf, len);
	mmc_wait_for_req(host, &mrq);
	err = 0;
	if (opcode == MMC_BUS_TEST_R) {
		for (i = 0; i < len / 4; i++)
			if ((test_buf[i] ^ data_buf[i]) != 0xff) {
				err = -EIO;
				break;
			}
	}
	kfree(data_buf);

	if (cmd.error)
		return cmd.error;
	if (data.error)
		return data.error;

	return err;
}

int mmc_bus_test(struct mmc_card *card, u8 bus_width)
{
	int err, width;

	if (bus_width == MMC_BUS_WIDTH_8)
		width = 8;
	else if (bus_width == MMC_BUS_WIDTH_4)
		width = 4;
	else if (bus_width == MMC_BUS_WIDTH_1)
		return 0; /* no need for test */
	else
		return -EINVAL;

	/*
	 * Ignore errors from BUS_TEST_W.  BUS_TEST_R will fail if there
	 * is a problem.  This improves chances that the test will work.
	 */
	mmc_send_bus_test(card, card->host, MMC_BUS_TEST_W, width);
	err = mmc_send_bus_test(card, card->host, MMC_BUS_TEST_R, width);
	return err;
}

int mmc_send_hpi_cmd(struct mmc_card *card, u32 *status)
{
	struct mmc_command cmd = {0};
	unsigned int opcode;
	int err;

	if (!card->ext_csd.hpi) {
		pr_warning("%s: Card didn't support HPI command\n",
			   mmc_hostname(card->host));
		return -EINVAL;
	}

	opcode = card->ext_csd.hpi_cmd;
	if (opcode == MMC_STOP_TRANSMISSION)
		cmd.flags = MMC_RSP_R1B | MMC_CMD_AC;
	else if (opcode == MMC_SEND_STATUS)
		cmd.flags = MMC_RSP_R1 | MMC_CMD_AC;

	cmd.opcode = opcode;
	cmd.arg = card->rca << 16 | 1;
	cmd.cmd_timeout_ms = card->ext_csd.out_of_int_time;

	err = mmc_wait_for_cmd(card->host, &cmd, 0);
	if (err) {
		pr_warn("%s: error %d interrupting operation. "
			"HPI command response %#x\n", mmc_hostname(card->host),
			err, cmd.resp[0]);
		return err;
	}
	if (status)
		*status = cmd.resp[0];

	return 0;
}

#ifdef CONFIG_MMC_SAMSUNG_SMART
/*
 * Vendor-specific Samsung BACK-DOOR command
 */
static int mmc_movi_vendor_cmd(struct mmc_card *card, unsigned int arg)
{
	struct mmc_command cmd;
	int err;
	u32 status;

	/* CMD62 is vendor CMD, it's not defined in eMMC spec. */
	cmd.opcode = 62;
	cmd.flags = MMC_RSP_R1B | MMC_CMD_AC;
	cmd.arg = arg;
	cmd.error = 0;

	err = mmc_wait_for_cmd(card->host, &cmd, 0);
	if (err)
		return err;

	do {
		err = mmc_send_status(card, &status);
		if (err)
			return err;
		if (card->host->caps & MMC_CAP_WAIT_WHILE_BUSY)
			break;
		if (mmc_host_is_spi(card->host))
			break;
	} while (R1_CURRENT_STATE(status) == R1_STATE_PRG);

	return err;
}

static int mmc_movi_read_cmd(struct mmc_card *card, u8 *buffer)
{
	struct mmc_command wcmd;
	struct mmc_data wdata;
	struct mmc_request brq;
	struct scatterlist sg;

	brq.cmd = &wcmd;
	brq.data = &wdata;
	brq.stop = NULL;
	brq.sbc = NULL;

	wcmd.opcode = MMC_READ_SINGLE_BLOCK;
	wcmd.arg = 0;
	wcmd.flags = MMC_RSP_SPI_R1 | MMC_RSP_R1 | MMC_CMD_ADTC;
	wcmd.error = 0;
	wcmd.retries = 0;
	wdata.blksz = 512;
	wdata.error = 0;
	wdata.stop = NULL;
	wdata.blocks = 1;
	wdata.flags = MMC_DATA_READ;

	wdata.sg = &sg;
	wdata.sg_len = 1;

	sg_init_one(&sg, buffer, 512);

	mmc_set_data_timeout(&wdata, card);

	mmc_wait_for_req(card->host, &brq);
	if (wcmd.error)
		return wcmd.error;
	if (wdata.error)
		return wdata.error;
	return 0;
}

static int mmc_samsung_smart_read(struct mmc_card *card, u8 *rdblock)
{
	int err, errx;

	/* enter vendor Smart Report mode */
	err = mmc_movi_vendor_cmd(card, 0xEFAC62EC);
	if (err) {
		pr_err("Failed entering Smart Report mode(1, %d)\n", err);
		return err;
	}
	err = mmc_movi_vendor_cmd(card, 0x0000CCEE);
	if (err) {
		pr_err("Failed entering Smart Report mode(2, %d)\n", err);
		return err;
	}

	/* read Smart Report */
	err = mmc_movi_read_cmd(card, rdblock);
	if (err)
		pr_err("Failed reading Smart Report(%d)\n", err);
		/* Do NOT return yet; we must leave Smart Report mode.*/

	/* exit vendor Smart Report mode */
	errx = mmc_movi_vendor_cmd(card, 0xEFAC62EC);
	if (errx)
		pr_err("Failed exiting Smart Report mode(1, %d)\n", errx);
	else {
		errx = mmc_movi_vendor_cmd(card, 0x00DECCEE);
		if (errx)
			pr_err("Failed exiting Smart Report mode(2, %d)\n",
									errx);
	}
	if (err)
		return err;
	return errx;
}

ssize_t mmc_samsung_smart_parse(u32 *report, char *for_sysfs)
{
	unsigned size = PAGE_SIZE;
	unsigned wrote;
	unsigned i;
	u32 val;
	char *str;
	static const struct {
		char *fmt;
		unsigned val_index;
	} to_output[] = {
		{ "super block size              : %u\n", 1 },
		{ "super page size               : %u\n", 2 },
		{ "optimal write size            : %u\n", 3 },
		{ "read reclaim count            : %u\n", 20 },
		{ "optimal trim size             : %u\n", 21 },
		{ "number of banks               : %u\n", 4 },
		{ "initial bad blocks per bank   : %u",	  5 },
		{ ",%u",				  8 },
		{ ",%u",				  11 },
		{ ",%u\n",				  14 },
		{ "runtime bad blocks per bank   : %u",	  6 },
		{ ",%u",				  9 },
		{ ",%u",				  12 },
		{ ",%u\n",				  15 },
		{ "reserved blocks left per bank : %u",	  7 },
		{ ",%u",				  10 },
		{ ",%u",				  13 },
		{ ",%u\n",				  16 },
		{ "all erase counts (min,avg,max): %u",	  18 },
		{ ",%u",				  19 },
		{ ",%u\n",				  17 },
		{ "SLC erase counts (min,avg,max): %u",	  31 },
		{ ",%u",				  32 },
		{ ",%u\n",				  30 },
		{ "MLC erase counts (min,avg,max): %u",	  34 },
		{ ",%u",				  35 },
		{ ",%u\n",				  33 },
	};

	/* A version field just in case things change. */
	wrote = scnprintf(for_sysfs, size,
				"version                       : %u\n", 0);
	size -= wrote;
	for_sysfs += wrote;

	/* The error mode. */
	val = le32_to_cpu(report[0]);
	switch (val) {
	case 0xD2D2D2D2:
		str = "Normal";
		break;
	case 0x5C5C5C5C:
		str = "RuntimeFatalError";
		break;
	case 0xE1E1E1E1:
		str = "MetaBrokenError";
		break;
	case 0x37373737:
		str = "OpenFatalError";
		val = 0; /* Remaining data is unreliable. */
		break;
	default:
		str = "Invalid";
		val = 0; /* Remaining data is unreliable. */
		break;
	}
	wrote = scnprintf(for_sysfs, size,
				"error mode                    : %s\n", str);
	size -= wrote;
	for_sysfs += wrote;
	/* Exit if we can't rely on the remaining data. */
	if (!val)
		return PAGE_SIZE - size;

	for (i = 0; i < ARRAY_SIZE(to_output); i++) {
		wrote = scnprintf(for_sysfs, size, to_output[i].fmt,
				  le32_to_cpu(report[to_output[i].val_index]));
		size -= wrote;
		for_sysfs += wrote;
	}

	return PAGE_SIZE - size;
}

ssize_t mmc_samsung_smart_handle(struct mmc_card *card, char *buf)
{
	int err;
	u32 *buffer;
	ssize_t len;

	buffer = kmalloc(512, GFP_KERNEL);
	if (!buffer) {
		pr_err("Failed to alloc memory for Smart Report\n");
		return 0;
	}

	mmc_claim_host(card->host);
	err = mmc_samsung_smart_read(card, (u8 *)buffer);
	mmc_release_host(card->host);

	if (err)
		len = 0;
	else
		len = mmc_samsung_smart_parse(buffer, buf);

	kfree(buffer);
	return len;
}

#endif /* CONFIG_MMC_SAMSUNG_SMART */
