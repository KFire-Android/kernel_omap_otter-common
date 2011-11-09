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

#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <stdint.h>
#endif

#define TF_SELF_TEST_DEVICE_BASE_NAME "tf_crypto_test"
#define TF_SELF_TEST_DEVICE_MAJOR_NUMBER 122
#define TF_SELF_TEST_DEVICE_MINOR_NUMBER 4

#define IOCTL_TF_SELF_TEST_POST _IO('z', 'P')

#define IOCTL_TF_SELF_TEST_HASH_INIT _IO('z', 'H')
struct tf_self_test_ioctl_param_hash_init {
	const char *alg_name;
	unsigned char *key; /*NULL for a simple hash, the key for an HMAC*/
	uint32_t key_size;
};

#define IOCTL_TF_SELF_TEST_BLKCIPHER_INIT _IO('z', 'B')
struct tf_self_test_ioctl_param_blkcipher_init {
	const char *alg_name;
	unsigned char *key;
	uint32_t key_size;
	unsigned char *iv;
	uint32_t iv_size;
	uint32_t decrypt; /*0 => encrypt, 1 => decrypt*/
};

#define IOCTL_TF_SELF_TEST_BLKCIPHER_UPDATE _IO('z', 'U')
struct tf_self_test_ioctl_param_blkcipher_update {
	unsigned char *in;
	unsigned char *out;
	uint32_t size;
};
