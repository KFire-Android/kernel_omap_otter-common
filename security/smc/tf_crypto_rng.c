/**
 * Copyright (c) 2011 Trusted Logic S.A.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

/**
 * File            : tf_crypto_rng.c
 * Original-Author : Trusted Logic S.A. (Jeremie Corbier)
 * Created         : 07-12-2011
 */

#include <linux/types.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/hw_random.h>

#include "tee_client_api.h"

#define SERVICE_SYSTEM_UUID \
	{ 0x56304b83, 0x5c4e, 0x4428, \
		{ 0xb9, 0x9e, 0x60, 0x5c, 0x96, 0xae, 0x58, 0xd6 } }

#define CKF_SERIAL_SESSION	0x00000004

#define SERVICE_SYSTEM_PKCS11_C_GENERATERANDOM_COMMAND_ID	0x00000039
#define SERVICE_SYSTEM_PKCS11_C_OPEN_SESSION_COMMAND_ID		0x00000042
#define SERVICE_SYSTEM_PKCS11_C_CLOSE_SESSION_COMMAND_ID	0x00000043

static struct hwrng tf_crypto_rng;

static int tf_crypto_rng_read(struct hwrng *rng, void *data, size_t max,
	bool wait)
{
	void *buf;
	u32 cmd;
	u32 crypto_session;
	TEEC_Result ret;
	TEEC_Operation op;
	TEEC_Context teec_context;
	TEEC_Session teec_session;
	TEEC_UUID uuid = SERVICE_SYSTEM_UUID;
	int err = 0;

	buf = kmalloc(max, GFP_KERNEL);
	if (buf == NULL)
		return -ENOMEM;

	ret = TEEC_InitializeContext(NULL, &teec_context);
	if (ret != TEEC_SUCCESS) {
		kfree(buf);
		return -EFAULT;
	}

	/* Call C_OpenSession */
	memset(&op, 0, sizeof(TEEC_Operation));

	op.paramTypes = TEEC_PARAM_TYPES(TEEC_NONE, TEEC_NONE, TEEC_NONE,
		TEEC_NONE);

	ret = TEEC_OpenSession(&teec_context, &teec_session,
		&uuid, TEEC_LOGIN_PUBLIC, NULL, &op, NULL);
	if (ret != TEEC_SUCCESS) {
		kfree(buf);
		TEEC_FinalizeContext(&teec_context);
		return -EFAULT;
	}

	memset(&op, 0, sizeof(TEEC_Operation));

	op.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_INOUT, TEEC_NONE, TEEC_NONE,
		TEEC_NONE);
	op.params[0].value.a = 0;
	op.params[0].value.b = CKF_SERIAL_SESSION;

	cmd = SERVICE_SYSTEM_PKCS11_C_OPEN_SESSION_COMMAND_ID & 0x00007FFF;

	ret = TEEC_InvokeCommand(&teec_session, cmd, &op, NULL);
	if (ret != TEEC_SUCCESS) {
		printk(KERN_ERR "%s: TEEC_InvokeCommand returned 0x%08x\n",
			__func__, ret);
		err = -EFAULT;
		goto exit;
	}

	crypto_session = op.params[0].value.a;

	/* Call C_GenerateRandom */
	memset(&op, 0, sizeof(TEEC_Operation));

	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_OUTPUT,
		TEEC_NONE, TEEC_NONE, TEEC_NONE);
	op.params[0].tmpref.buffer = (uint8_t *) buf;
	op.params[0].tmpref.size = (uint32_t) max;

	cmd = (crypto_session << 16) |
		(SERVICE_SYSTEM_PKCS11_C_GENERATERANDOM_COMMAND_ID & 0x7fff);

	ret = TEEC_InvokeCommand(&teec_session, cmd, &op, NULL);
	if (ret != TEEC_SUCCESS) {
		printk(KERN_ERR "%s: TEEC_InvokeCommand returned 0x%08x\n",
			__func__, ret);
		err = -EFAULT;
	}

	/* Call C_CloseSession */
	memset(&op, 0, sizeof(TEEC_Operation));

	op.paramTypes = TEEC_PARAM_TYPES(TEEC_NONE, TEEC_NONE, TEEC_NONE,
		TEEC_NONE);

	cmd = (crypto_session << 16) |
		(SERVICE_SYSTEM_PKCS11_C_CLOSE_SESSION_COMMAND_ID & 0x7fff);

	ret = TEEC_InvokeCommand(&teec_session, cmd, &op, NULL);
	if (ret != TEEC_SUCCESS) {
		printk(KERN_ERR "%s: TEEC_InvokeCommand returned 0x%08x\n",
			__func__, ret);
		err = -EFAULT;
	}

exit:
	TEEC_CloseSession(&teec_session);
	TEEC_FinalizeContext(&teec_context);

	if (!err)
		memcpy(data, buf, max);

	kfree(buf);

	if (err)
		return err;
	else
		return max;
}

static int __init tf_crypto_rng_init(void)
{
	memset(&tf_crypto_rng, 0, sizeof(struct hwrng));

	tf_crypto_rng.name = "rng-smc";
	tf_crypto_rng.read = tf_crypto_rng_read;

	return hwrng_register(&tf_crypto_rng);
}
module_init(tf_crypto_rng_init);

static void __exit tf_crypto_rng_exit(void)
{
	hwrng_unregister(&tf_crypto_rng);
}
module_exit(tf_crypto_rng_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jeremie Corbier <jeremie.corbier@trusted-logic.com>");
MODULE_DESCRIPTION("OMAP4 TRNG driver.");
