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

#include <linux/cdev.h>
#include <linux/crypto.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/scatterlist.h>
#include <linux/sysdev.h>

#include "tf_crypto.h"
#include "tf_defs.h"
#include "tf_util.h"



/*** Test vectors ***/

struct digest_test_vector {
	unsigned char *text;
	unsigned length;
	unsigned char *digest;
	unsigned char *key; /*used for HMAC, NULL for plain digests*/
	unsigned key_length;
};

struct blkcipher_test_vector {
	unsigned char *key;
	unsigned key_length;
	unsigned char *iv;
	unsigned char *plaintext;
	unsigned char *ciphertext;
	unsigned length;
};

/* From FIPS-180 */
struct digest_test_vector sha1_test_vector = {
	"abc",
	3,
	"\xa9\x99\x3e\x36\x47\x06\x81\x6a\xba\x3e\x25\x71\x78\x50\xc2\x6c"
	"\x9c\xd0\xd8\x9d"
};
struct digest_test_vector sha224_test_vector = {
	"abc",
	3,
	"\x23\x09\x7D\x22\x34\x05\xD8\x22\x86\x42\xA4\x77\xBD\xA2\x55\xB3"
	"\x2A\xAD\xBC\xE4\xBD\xA0\xB3\xF7\xE3\x6C\x9D\xA7"
};
struct digest_test_vector sha256_test_vector = {
	"abc",
	3,
	"\xba\x78\x16\xbf\x8f\x01\xcf\xea\x41\x41\x40\xde\x5d\xae\x22\x23"
	"\xb0\x03\x61\xa3\x96\x17\x7a\x9c\xb4\x10\xff\x61\xf2\x00\x15\xad"
};

/* From FIPS-198 */
struct digest_test_vector hmac_sha1_test_vector = {
	"Sample message for keylen<blocklen",
	34,
	"\x4c\x99\xff\x0c\xb1\xb3\x1b\xd3\x3f\x84\x31\xdb\xaf\x4d\x17\xfc"
	"\xd3\x56\xa8\x07",
	"\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f"
	"\x10\x11\x12\x13",
	20,
};
struct digest_test_vector hmac_sha224_test_vector = {
	"Sample message for keylen<blocklen",
	34,
	"\xe3\xd2\x49\xa8\xcf\xb6\x7e\xf8\xb7\xa1\x69\xe9\xa0\xa5\x99\x71"
	"\x4a\x2c\xec\xba\x65\x99\x9a\x51\xbe\xb8\xfb\xbe",
	"\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f"
	"\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b",
	28,
};
struct digest_test_vector hmac_sha256_test_vector = {
	"Sample message for keylen<blocklen",
	34,
	"\xa2\x8c\xf4\x31\x30\xee\x69\x6a\x98\xf1\x4a\x37\x67\x8b\x56\xbc"
	"\xfc\xbd\xd9\xe5\xcf\x69\x71\x7f\xec\xf5\x48\x0f\x0e\xbd\xf7\x90",
	"\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f"
	"\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f",
	32,
};

/* From FIPS-197 */
struct blkcipher_test_vector aes_ecb_128_test_vector = {
	"\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f",
	16,
	NULL,
	"\x00\x11\x22\x33\x44\x55\x66\x77\x88\x99\xaa\xbb\xcc\xdd\xee\xff",
	"\x69\xc4\xe0\xd8\x6a\x7b\x04\x30\xd8\xcd\xb7\x80\x70\xb4\xc5\x5a",
	16,
};
struct blkcipher_test_vector aes_ecb_192_test_vector = {
	"\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f"
	"\x10\x11\x12\x13\x14\x15\x16\x17",
	24,
	NULL,
	"\x00\x11\x22\x33\x44\x55\x66\x77\x88\x99\xaa\xbb\xcc\xdd\xee\xff",
	"\xdd\xa9\x7c\xa4\x86\x4c\xdf\xe0\x6e\xaf\x70\xa0\xec\x0d\x71\x91",
	16,
};
struct blkcipher_test_vector aes_ecb_256_test_vector = {
	"\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f"
	"\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f",
	32,
	NULL,
	"\x00\x11\x22\x33\x44\x55\x66\x77\x88\x99\xaa\xbb\xcc\xdd\xee\xff",
	"\x8e\xa2\xb7\xca\x51\x67\x45\xbf\xea\xfc\x49\x90\x4b\x49\x60\x89",
	16,
};

/* From RFC 3602 */
struct blkcipher_test_vector aes_cbc_128_test_vector = {
	"\x06\xa9\x21\x40\x36\xb8\xa1\x5b\x51\x2e\x03\xd5\x34\x12\x00\x06",
	16,
	"\x3d\xaf\xba\x42\x9d\x9e\xb4\x30\xb4\x22\xda\x80\x2c\x9f\xac\x41",
	"Single block msg",
	"\xe3\x53\x77\x9c\x10\x79\xae\xb8\x27\x08\x94\x2d\xbe\x77\x18\x1a",
	16
};
/* From NISP SP800-38A */
struct blkcipher_test_vector aes_cbc_192_test_vector = {
	"\x8e\x73\xb0\xf7\xda\x0e\x64\x52\xc8\x10\xf3\x2b\x80\x90\x79\xe5"
	"\x62\xf8\xea\xd2\x52\x2c\x6b\x7b",
	24,
	"\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f",
	"\x6b\xc1\xbe\xe2\x2e\x40\x9f\x96\xe9\x3d\x7e\x11\x73\x93\x17\x2a"
	"\xae\x2d\x8a\x57\x1e\x03\xac\x9c\x9e\xb7\x6f\xac\x45\xaf\x8e\x51"
	"\x30\xc8\x1c\x46\xa3\x5c\xe4\x11\xe5\xfb\xc1\x19\x1a\x0a\x52\xef"
	"\xf6\x9f\x24\x45\xdf\x4f\x9b\x17\xad\x2b\x41\x7b\xe6\x6c\x37\x10",
	"\x4f\x02\x1d\xb2\x43\xbc\x63\x3d\x71\x78\x18\x3a\x9f\xa0\x71\xe8"
	"\xb4\xd9\xad\xa9\xad\x7d\xed\xf4\xe5\xe7\x38\x76\x3f\x69\x14\x5a"
	"\x57\x1b\x24\x20\x12\xfb\x7a\xe0\x7f\xa9\xba\xac\x3d\xf1\x02\xe0"
	"\x08\xb0\xe2\x79\x88\x59\x88\x81\xd9\x20\xa9\xe6\x4f\x56\x15\xcd",
	64
};
struct blkcipher_test_vector aes_cbc_256_test_vector = {
	"\x60\x3d\xeb\x10\x15\xca\x71\xbe\x2b\x73\xae\xf0\x85\x7d\x77\x81"
	"\x1f\x35\x2c\x07\x3b\x61\x08\xd7\x2d\x98\x10\xa3\x09\x14\xdf\xf4",
	32,
	"\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f",
	"\x6b\xc1\xbe\xe2\x2e\x40\x9f\x96\xe9\x3d\x7e\x11\x73\x93\x17\x2a"
	"\xae\x2d\x8a\x57\x1e\x03\xac\x9c\x9e\xb7\x6f\xac\x45\xaf\x8e\x51"
	"\x30\xc8\x1c\x46\xa3\x5c\xe4\x11\xe5\xfb\xc1\x19\x1a\x0a\x52\xef"
	"\xf6\x9f\x24\x45\xdf\x4f\x9b\x17\xad\x2b\x41\x7b\xe6\x6c\x37\x10",
	"\xf5\x8c\x4c\x04\xd6\xe5\xf1\xba\x77\x9e\xab\xfb\x5f\x7b\xfb\xd6"
	"\x9c\xfc\x4e\x96\x7e\xdb\x80\x8d\x67\x9f\x77\x7b\xc6\x70\x2c\x7d"
	"\x39\xf2\x33\x69\xa9\xd9\xba\xcf\xa5\x30\xe2\x63\x04\x23\x14\x61"
	"\xb2\xeb\x05\xe2\xc3\x9b\xe9\xfc\xda\x6c\x19\x07\x8c\x6a\x9d\x1b",
	64
};



/*** Helper functions ***/

static int tf_self_test_digest(const char *alg_name,
			       const struct digest_test_vector *tv)
{
	unsigned char digest[64];
	unsigned char input[256];
	struct scatterlist sg;
	struct hash_desc desc = {NULL, 0};
	int error;
	size_t digest_length;

	desc.tfm = crypto_alloc_hash(alg_name, 0, 0);
	if (IS_ERR_OR_NULL(desc.tfm)) {
		ERROR("crypto_alloc_hash(%s) failed", alg_name);
		error = (desc.tfm == NULL ? -ENOMEM : (int)desc.tfm);
		goto abort;
	}

	digest_length = crypto_hash_digestsize(desc.tfm);
	INFO("alg_name=%s driver_name=%s digest_length=%u",
	     alg_name,
	     crypto_tfm_alg_driver_name(crypto_hash_tfm(desc.tfm)),
	     digest_length);
	if (digest_length > sizeof(digest)) {
		ERROR("digest length too large (%zu > %zu)",
		      digest_length, sizeof(digest));
		error = -ENOMEM;
		goto abort;
	}

	if (tv->key != NULL) {
		error = crypto_hash_setkey(desc.tfm, tv->key, tv->key_length);
		if (error) {
			ERROR("crypto_hash_setkey(%s) failed: %d",
			      alg_name, error);
			goto abort;
		}
		TF_TRACE_ARRAY(tv->key, tv->key_length);
	}
	error = crypto_hash_init(&desc);
	if (error) {
		ERROR("crypto_hash_init(%s) failed: %d", alg_name, error);
		goto abort;
	}

	/* The test vector data is in vmalloc'ed memory since it's a module
	   global. Copy it to the stack, since the crypto API doesn't support
	   vmalloc'ed memory. */
	if (tv->length > sizeof(input)) {
		ERROR("data too large (%zu > %zu)",
		      tv->length, sizeof(input));
		error = -ENOMEM;
		goto abort;
	}
	memcpy(input, tv->text, tv->length);
	INFO("sg_init_one(%p, %p, %u)", &sg, input, tv->length);
	sg_init_one(&sg, input, tv->length);

	TF_TRACE_ARRAY(input, tv->length);
	error = crypto_hash_update(&desc, &sg, tv->length);
	if (error) {
		ERROR("crypto_hash_update(%s) failed: %d",
		      alg_name, error);
		goto abort;
	}

	error = crypto_hash_final(&desc, digest);
	if (error) {
		ERROR("crypto_hash_final(%s) failed: %d", alg_name, error);
		goto abort;
	}

	crypto_free_hash(desc.tfm);
	desc.tfm = NULL;

	if (memcmp(digest, tv->digest, digest_length)) {
		TF_TRACE_ARRAY(digest, digest_length);
		ERROR("wrong %s digest value", alg_name);
		pr_err("[SMC Driver] error: SMC Driver POST FAILURE (%s)\n",
		       alg_name);
		error = -EINVAL;
	} else {
		INFO("%s: digest successful", alg_name);
		error = 0;
	}
	return error;

abort:
	if (!IS_ERR_OR_NULL(desc.tfm))
		crypto_free_hash(desc.tfm);
	pr_err("[SMC Driver] error: SMC Driver POST FAILURE (%s)\n", alg_name);
	return error;
}

static int tf_self_test_perform_blkcipher(
	const char *alg_name,
	const struct blkcipher_test_vector *tv,
	bool decrypt)
{
	struct blkcipher_desc desc = {0};
	struct scatterlist sg_in, sg_out;
	unsigned char *in = NULL;
	unsigned char *out = NULL;
	unsigned in_size, out_size;
	int error;

	desc.tfm = crypto_alloc_blkcipher(alg_name, 0, 0);
	if (IS_ERR_OR_NULL(desc.tfm)) {
		ERROR("crypto_alloc_blkcipher(%s) failed", alg_name);
		error = (desc.tfm == NULL ? -ENOMEM : (int)desc.tfm);
		goto abort;
	}
	INFO("%s alg_name=%s driver_name=%s key_size=%u block_size=%u",
	     decrypt ? "decrypt" : "encrypt", alg_name,
	     crypto_tfm_alg_driver_name(crypto_blkcipher_tfm(desc.tfm)),
	     tv->key_length * 8,
	     crypto_blkcipher_blocksize(desc.tfm));

	in_size = tv->length;
	in = kmalloc(in_size, GFP_KERNEL);
	if (IS_ERR_OR_NULL(in)) {
		ERROR("kmalloc(%u) failed: %d", in_size, (int)in);
		error = (in == NULL ? -ENOMEM : (int)in);
		goto abort;
	}
	memcpy(in, decrypt ? tv->ciphertext : tv->plaintext,
	       tv->length);

	out_size = tv->length + crypto_blkcipher_blocksize(desc.tfm);
	out = kmalloc(out_size, GFP_KERNEL);
	if (IS_ERR_OR_NULL(out)) {
		ERROR("kmalloc(%u) failed: %d", out_size, (int)out);
		error = (out == NULL ? -ENOMEM : (int)out);
		goto abort;
	}

	error = crypto_blkcipher_setkey(desc.tfm, tv->key, tv->key_length);
	if (error) {
		ERROR("crypto_alloc_setkey(%s) failed", alg_name);
		goto abort;
	}
	TF_TRACE_ARRAY(tv->key, tv->key_length);
	if (tv->iv != NULL) {
		unsigned iv_length = crypto_blkcipher_ivsize(desc.tfm);
		crypto_blkcipher_set_iv(desc.tfm, tv->iv, iv_length);
		TF_TRACE_ARRAY(tv->iv, iv_length);
	}

	sg_init_one(&sg_in, in, tv->length);
	sg_init_one(&sg_out, out, tv->length);
	TF_TRACE_ARRAY(in, tv->length);
	(decrypt ? crypto_blkcipher_decrypt : crypto_blkcipher_encrypt)
		(&desc, &sg_out, &sg_in, tv->length);
	if (error) {
		ERROR("crypto_blkcipher_%s(%s) failed",
		      decrypt ? "decrypt" : "encrypt", alg_name);
		goto abort;
	}
	TF_TRACE_ARRAY(out, tv->length);

	crypto_free_blkcipher(desc.tfm);

	if (memcmp((decrypt ? tv->plaintext : tv->ciphertext),
		   out, tv->length)) {
		ERROR("Wrong %s/%u %s result", alg_name, tv->key_length * 8,
		      decrypt ? "decryption" : "encryption");
		error = -EINVAL;
	} else {
		INFO("%s/%u: %s successful", alg_name, tv->key_length * 8,
		     decrypt ? "decryption" : "encryption");
		error = 0;
	}
	kfree(in);
	kfree(out);
	return error;

abort:
	if (!IS_ERR_OR_NULL(desc.tfm))
		crypto_free_blkcipher(desc.tfm);
	if (!IS_ERR_OR_NULL(out))
		kfree(out);
	if (!IS_ERR_OR_NULL(in))
		kfree(in);
	return error;
}

static int tf_self_test_blkcipher(const char *alg_name,
			   const struct blkcipher_test_vector *tv)
{
	int encryption_outcome =
		tf_self_test_perform_blkcipher(alg_name, tv, false);
	int decryption_outcome =
		tf_self_test_perform_blkcipher(alg_name, tv, true);
	if (encryption_outcome == 0 && decryption_outcome == 0) {
		return 0;
	} else {
		pr_err("[SMC Driver] error: SMC Driver POST FAILURE (%s/%u)",
		       alg_name, tv->key_length * 8);
		return -EINVAL;
	}
}



/*** Integrity check ***/

#if defined(CONFIG_MODULE_EXTRA_COPY) && defined(MODULE)

static ssize_t scan_hex(unsigned char *const buf,
			size_t buf_size,
			const char *const hex)
{
	size_t bi = 0;
	size_t hi;
	unsigned prev = -1, cur;
	for (hi = 0; hex[hi] != 0; hi++) {
		if (hex[hi] >= '0' && hex[hi] <= '9')
			cur = hex[hi] - '0';
		else if (hex[hi] >= 'a' && hex[hi] <= 'f')
			cur = hex[hi] - 'a' + 10;
		else if (hex[hi] >= 'A' && hex[hi] <= 'f')
			cur = hex[hi] - 'F' + 10;
		else if (hex[hi] == '-' || hex[hi] == ' ')
			continue;
		else {
			ERROR("invalid character at %zu (%u)", hi, hex[hi]);
			return -EINVAL;
		}
		if (prev == -1)
			prev = cur;
		else {
			if (bi >= buf_size) {
				ERROR("buffer too large at %zu", hi);
				return -ENOSPC;
			}
			buf[bi++] = prev << 4 | cur;
			prev = -1;
		}
	}
	return bi;
}

/* Allocate a scatterlist for a vmalloc block. The scatterlist is allocated
   with kmalloc. Buffers of arbitrary alignment are supported.
   This function is derived from other vmalloc_to_sg functions in the kernel
   tree, but note that its second argument is a size in bytes, not in pages.
 */
static struct scatterlist *vmalloc_to_sg(unsigned char *const buf,
					 size_t const bytes)
{
	struct scatterlist *sg_array = NULL;
	struct page *pg;
	/* Allow non-page-aligned pointers, so the first and last page may
	   both be partial. */
	unsigned const page_count = bytes / PAGE_SIZE + 2;
	unsigned char *ptr;
	unsigned i;

	sg_array = kcalloc(page_count, sizeof(*sg_array), GFP_KERNEL);
	if (sg_array == NULL)
		goto abort;
	sg_init_table(sg_array, page_count);
	for (i = 0, ptr = (void *)((unsigned long)buf & PAGE_MASK);
	     ptr < buf + bytes;
	     i++, ptr += PAGE_SIZE) {
		pg = vmalloc_to_page(ptr);
		if (pg == NULL)
			goto abort;
		sg_set_page(&sg_array[i], pg, PAGE_SIZE, 0);
	}
	/* Rectify the first page which may be partial. The last page may
	   also be partial but its offset is correct so it doesn't matter. */
	sg_array[0].offset = offset_in_page(buf);
	sg_array[0].length = PAGE_SIZE - offset_in_page(buf);
	return sg_array;
abort:
	if (sg_array != NULL)
		kfree(sg_array);
	return NULL;
}

static unsigned char tf_integrity_hmac_sha256_key[] = {
	0x6c, 0x99, 0x2c, 0x8a, 0x26, 0x98, 0xd1, 0x09,
	0x5c, 0x18, 0x20, 0x42, 0x51, 0xaf, 0xf7, 0xad,
	0x6b, 0x42, 0xfb, 0x1d, 0x4b, 0x44, 0xfa, 0xcc,
	0x37, 0x7b, 0x05, 0x6d, 0x57, 0x24, 0x5f, 0x46,
};

static int tf_self_test_integrity(const char *alg_name, struct module *mod)
{
	unsigned char expected[32];
	unsigned char actual[32];
	struct scatterlist *sg = NULL;
	struct hash_desc desc = {NULL, 0};
	size_t digest_length;
	unsigned char *const key = tf_integrity_hmac_sha256_key;
	size_t const key_length = sizeof(tf_integrity_hmac_sha256_key);
	int error;

	if (mod->raw_binary_ptr == NULL)
		return -ENXIO;
	if (tf_integrity_hmac_sha256_expected_value == NULL)
		return -ENOENT;
	INFO("expected=%s", tf_integrity_hmac_sha256_expected_value);
	error = scan_hex(expected, sizeof(expected),
			 tf_integrity_hmac_sha256_expected_value);
	if (error < 0) {
		pr_err("tf_driver: Badly formatted hmac_sha256 parameter "
		       "(should be a hex string)\n");
		return -EIO;
	};

	desc.tfm = crypto_alloc_hash(alg_name, 0, 0);
	if (IS_ERR_OR_NULL(desc.tfm)) {
		ERROR("crypto_alloc_hash(%s) failed", alg_name);
		error = (desc.tfm == NULL ? -ENOMEM : (int)desc.tfm);
		goto abort;
	}
	digest_length = crypto_hash_digestsize(desc.tfm);
	INFO("alg_name=%s driver_name=%s digest_length=%u",
	     alg_name,
	     crypto_tfm_alg_driver_name(crypto_hash_tfm(desc.tfm)),
	     digest_length);

	error = crypto_hash_setkey(desc.tfm, key, key_length);
	if (error) {
		ERROR("crypto_hash_setkey(%s) failed: %d",
		      alg_name, error);
		goto abort;
	}

	sg = vmalloc_to_sg(mod->raw_binary_ptr, mod->raw_binary_size);
	if (IS_ERR_OR_NULL(sg)) {
		ERROR("vmalloc_to_sg(%lu) failed: %d",
		      mod->raw_binary_size, (int)sg);
		error = (sg == NULL ? -ENOMEM : (int)sg);
		goto abort;
	}

	error = crypto_hash_digest(&desc, sg, mod->raw_binary_size, actual);
	if (error) {
		ERROR("crypto_hash_digest(%s) failed: %d",
		      alg_name, error);
		goto abort;
	}

	kfree(sg);
	crypto_free_hash(desc.tfm);

#ifdef CONFIG_TF_DRIVER_FAULT_INJECTION
	if (tf_fault_injection_mask & TF_CRYPTO_ALG_INTEGRITY) {
		pr_warning("TF: injecting fault in integrity check!\n");
		actual[0] = 0xff;
		actual[1] ^= 0xff;
	}
#endif
	TF_TRACE_ARRAY(expected, digest_length);
	TF_TRACE_ARRAY(actual, digest_length);
	if (memcmp(expected, actual, digest_length)) {
		ERROR("wrong %s digest value", alg_name);
		error = -EINVAL;
	} else {
		INFO("%s: digest successful", alg_name);
		error = 0;
	}

	return error;

abort:
	if (!IS_ERR_OR_NULL(sg))
		kfree(sg);
	if (!IS_ERR_OR_NULL(desc.tfm))
		crypto_free_hash(desc.tfm);
	return error == -ENOMEM ? error : -EIO;
}

#endif /*defined(CONFIG_MODULE_EXTRA_COPY) && defined(MODULE)*/


/*** Sysfs entries ***/

struct tf_post_data {
	unsigned failures;
	struct kobject kobj;
};

static const struct attribute tf_post_failures_attr = {
	.name = "failures",
	.mode = 0444,
};

static ssize_t tf_post_kobject_show(struct kobject *kobj,
				    struct attribute *attribute,
				    char *buf)
{
	struct tf_post_data *data =
		container_of(kobj, struct tf_post_data, kobj);
	return (data->failures == 0 ?
		snprintf(buf, PAGE_SIZE, "0\n") :
		snprintf(buf, PAGE_SIZE, "0x%08x\n", data->failures));
}

static const struct sysfs_ops tf_post_sysfs_ops = {
	.show = tf_post_kobject_show,
};

static struct kobj_type tf_post_data_ktype = {
	.sysfs_ops = &tf_post_sysfs_ops,
};

static struct tf_post_data tf_post_data;



/*** POST entry point ***/

unsigned tf_self_test_post_vectors(void)
{
	unsigned failures = 0;

	dpr_info("[SMC Driver] Starting POST\n");
#ifdef CONFIG_TF_DRIVER_FAULT_INJECTION
	dpr_info("%s: fault=0x%08x\n", __func__, tf_fault_injection_mask);
#endif

	if (tf_self_test_blkcipher("aes-ecb-smc", &aes_ecb_128_test_vector))
		failures |= TF_CRYPTO_ALG_AES_ECB_128;
	if (tf_self_test_blkcipher("aes-ecb-smc", &aes_ecb_192_test_vector))
		failures |= TF_CRYPTO_ALG_AES_ECB_192;
	if (tf_self_test_blkcipher("aes-ecb-smc", &aes_ecb_256_test_vector))
		failures |= TF_CRYPTO_ALG_AES_ECB_256;
	if (tf_self_test_blkcipher("aes-cbc-smc", &aes_cbc_128_test_vector))
		failures |= TF_CRYPTO_ALG_AES_CBC_128;
	if (tf_self_test_blkcipher("aes-cbc-smc", &aes_cbc_192_test_vector))
		failures |= TF_CRYPTO_ALG_AES_CBC_192;
	if (tf_self_test_blkcipher("aes-cbc-smc", &aes_cbc_256_test_vector))
		failures |= TF_CRYPTO_ALG_AES_CBC_256;

	if (tf_self_test_digest("sha1-smc", &sha1_test_vector))
		failures |= TF_CRYPTO_ALG_SHA1;
	if (tf_self_test_digest("sha224-smc", &sha224_test_vector))
		failures |= TF_CRYPTO_ALG_SHA224;
	if (tf_self_test_digest("sha256-smc", &sha256_test_vector))
		failures |= TF_CRYPTO_ALG_SHA256;

	if (tf_self_test_digest("tf_hmac(sha1-smc)",
				&hmac_sha1_test_vector))
		failures |= TF_CRYPTO_ALG_HMAC_SHA1;
	if (tf_self_test_digest("tf_hmac(sha224-smc)",
				&hmac_sha224_test_vector))
		failures |= TF_CRYPTO_ALG_HMAC_SHA224;
	if (tf_self_test_digest("tf_hmac(sha256-smc)",
				&hmac_sha256_test_vector))
		failures |= TF_CRYPTO_ALG_HMAC_SHA256;

#if defined(CONFIG_MODULE_EXTRA_COPY) && defined(MODULE)
	switch (tf_self_test_integrity("tf_hmac(sha256-smc)", &__this_module)) {
	case 0:
		pr_notice("[SMC Driver] integrity check passed\n");
		break;
	case -ENXIO:
		pr_warning("[SMC Driver] integrity check can only be run "
			   "when the module is loaded");
		if (tf_post_data.failures & TF_CRYPTO_ALG_INTEGRITY) {
			pr_notice("[SMC Driver] "
				  "integrity check initially failed\n");
			failures |= TF_CRYPTO_ALG_INTEGRITY;
		} else
			pr_notice("[SMC Driver] "
				  "integrity check initially passed\n");
		break;
	case -ENOENT:
		pr_warning("[SCM Driver] "
			   "integrity check cannot be made\n");
		pr_notice("[SCM Driver] "
			  "you must pass the hmac_sha256 parameter\n");
		/* FALLTHROUGH */
	default:
		pr_err("[SCM Driver] error: "
		       "SMC Driver POST FAILURE "
		       "(Integrity check HMAC-SHA-256)\n");
		failures |= TF_CRYPTO_ALG_INTEGRITY;
		break;
	}
#endif

	if (failures) {
		pr_notice("[SMC Driver] failures in POST (0x%08x)\n",
			  failures);
	} else {
		pr_notice("[SMC Driver] init successful\n");
	}

	tf_post_data.failures = failures;
	return failures;
}

void tf_self_test_post_exit(void)
{
	tf_post_data.failures = 0;
	kobject_put(&tf_post_data.kobj);
}

int __init tf_self_test_post_init(struct kobject *parent)
{
	int error = 0;

	if (parent != NULL) {
		error = kobject_init_and_add(&tf_post_data.kobj,
					     &tf_post_data_ktype,
					     parent,
					     "post");
		if (error)
			goto abort;
		error = sysfs_create_file(&tf_post_data.kobj,
					  &tf_post_failures_attr);
		if (error)
			goto abort;
	}

	return tf_self_test_post_vectors();

abort:
	tf_self_test_post_exit();
	if (error == 0)
		error = -ENOMEM;
	return error;
}
