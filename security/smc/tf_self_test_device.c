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

#include "tf_crypto.h"
#include "tf_defs.h"
#include "tf_util.h"
#include "tf_self_test_io.h"

struct tf_self_test_hash_data {
	struct hash_desc desc;
	void *buf;
	unsigned buffer_size;
	unsigned digestsize;
	void *key;
};
struct tf_self_test_blkcipher_data {
	struct blkcipher_desc desc;
	void *key;
	bool decrypt; /*false => encrypt, true => decrypt*/
};

struct tf_self_test_file_data {
	unsigned type;
	struct scatterlist sg[2];
	union {
		struct tf_self_test_hash_data hash;
		struct tf_self_test_blkcipher_data cipher;
	};
};

static void tf_self_test_finalize(struct tf_self_test_file_data *data)
{
	switch (data->type & CRYPTO_ALG_TYPE_MASK) {
	case CRYPTO_ALG_TYPE_HASH:
	case CRYPTO_ALG_TYPE_SHASH:
	case CRYPTO_ALG_TYPE_AHASH:
		if (!IS_ERR_OR_NULL(data->hash.buf))
			kfree(data->hash.buf);
		if (!IS_ERR_OR_NULL(data->hash.desc.tfm))
			crypto_free_hash(data->hash.desc.tfm);
		if (!IS_ERR_OR_NULL(data->hash.key))
			kfree(data->hash.key);
		break;
	case CRYPTO_ALG_TYPE_BLKCIPHER:
	case CRYPTO_ALG_TYPE_ABLKCIPHER:
		if (!IS_ERR_OR_NULL(data->cipher.desc.tfm))
			crypto_free_blkcipher(data->cipher.desc.tfm);
		if (!IS_ERR_OR_NULL(data->cipher.key))
			kfree(data->cipher.key);
		break;
	}
	memset(data, 0, sizeof(*data));
}

static int tf_self_test_device_open(struct inode *inode, struct file *filp)
{
	struct tf_self_test_file_data *data;
	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (data == NULL)
		return -ENOMEM;
	data->type = 0;
	filp->private_data = data;
	return 0;
}

static int tf_self_test_device_release(struct inode *inode, struct file *filp)
{
	tf_self_test_finalize(filp->private_data);
	kfree(filp->private_data);
	return 0;
}

static long tf_self_test_ioctl_hash_init(struct file *filp,
					 void __user *user_params)
{
	struct tf_self_test_ioctl_param_hash_init params;
	struct tf_self_test_file_data *data = filp->private_data;
	void *ptr;
	size_t size;
	int ret;
	char alg_name[CRYPTO_MAX_ALG_NAME] = "?";
	INFO("invoked");

	/* Reset the connection data. */
	if (data->type != 0)
		tf_self_test_finalize(data);

	/* Copy parameters from user space */
	if (copy_from_user(&params, user_params, sizeof(params)))
		return -EFAULT;
	ret = strncpy_from_user(alg_name, params.alg_name, sizeof(alg_name));
	if (ret < 0)
		return -EFAULT;
	else if (ret >= sizeof(alg_name))
		return -ENAMETOOLONG;
	INFO("alg_name=%s", alg_name);

	/* Prepare for hashing */
	data->hash.desc.tfm = crypto_alloc_hash(alg_name, 0, 0);
	if (IS_ERR_OR_NULL(data->hash.desc.tfm)) {
		ret = (int)data->hash.desc.tfm;
		goto abort;
	}
	data->type = crypto_tfm_alg_type(&data->hash.desc.tfm->base);
	data->hash.digestsize = crypto_hash_digestsize(data->hash.desc.tfm);

	/* Set the key if provided */
	if (params.key != NULL) {
		u8 key[128];
		if (params.key_size > sizeof(key)) {
			ret = -EINVAL;
			goto abort;
		}
		if (copy_from_user(key, params.key, params.key_size)) {
			ret = -EFAULT;
			goto abort;
		}
		TF_TRACE_ARRAY(key, params.key_size);
		if (crypto_hash_setkey(data->hash.desc.tfm,
				       key, params.key_size)) {
			ret = -EIO;
			goto abort;
		}
		INFO("crypto_hash_setkey ok");
	}
	if (crypto_hash_init(&data->hash.desc)) {
		ret = -EIO;
		goto abort;
	}
	INFO("crypto_hash_init(%s) ok", alg_name);

	/* Allocate a staging buffer for the data or the result */
	size = PAGE_SIZE;
	BUG_ON(size < data->hash.digestsize);
	ptr = kzalloc(size, GFP_KERNEL);
	if (IS_ERR_OR_NULL(ptr)) {
		ret = -ENOMEM;
		goto abort;
	}
	data->hash.buf = ptr;
	data->hash.buffer_size = size;
	INFO("allocated a buffer of %zu bytes", size);

	/* Success */
	return 0;

abort:
	ERROR("alg_name=%s", alg_name);
	tf_self_test_finalize(data);
	return ret;
}

static long tf_self_test_ioctl_blkcipher_init(struct file *filp,
					      void __user *user_params)
{
	struct tf_self_test_ioctl_param_blkcipher_init params;
	struct tf_self_test_file_data *data = filp->private_data;
	int ret;
	char alg_name[CRYPTO_MAX_ALG_NAME] = "?";

	/* Reset the connection data. */
	if (data->type != 0)
		tf_self_test_finalize(data);

	/* Copy parameters from user space */
	if (copy_from_user(&params, user_params, sizeof(params)))
		return -EFAULT;
	ret = strncpy_from_user(alg_name, params.alg_name, sizeof(alg_name));
	if (ret < 0)
		return -EFAULT;
	else if (ret >= sizeof(alg_name))
		return -ENAMETOOLONG;
	data->cipher.decrypt = params.decrypt;

	/* Prepare for encryption/decryption */
	data->cipher.desc.tfm = crypto_alloc_blkcipher(alg_name, 0, 0);
	if (IS_ERR_OR_NULL(data->cipher.desc.tfm)) {
		ret = (int)data->cipher.desc.tfm;
		goto abort;
	}
	data->type = crypto_tfm_alg_type(&data->cipher.desc.tfm->base);
	INFO("crypto_alloc_blkcipher(%s) ok", alg_name);

	/* Set the key if provided */
	if (params.key != NULL) {
		u8 key[128];
		if (params.key_size > sizeof(key)) {
			ret = -EINVAL;
			goto abort;
		}
		if (copy_from_user(key, params.key, params.key_size)) {
			ret = -EFAULT;
			goto abort;
		}
		TF_TRACE_ARRAY(key, params.key_size);
		if (crypto_blkcipher_setkey(data->cipher.desc.tfm,
					    key, params.key_size)) {
			ret = -EIO;
			goto abort;
		}
		INFO("crypto_blkcipher_setkey ok");
	} else {
		/*A key is required for ciphers*/
		ret = -EINVAL;
		goto abort;
	}

	/* Set the IV if provided */
	if (params.iv != NULL) {
		unsigned char *iv =
			crypto_blkcipher_crt(data->cipher.desc.tfm)->iv;
		unsigned size = crypto_blkcipher_ivsize(data->cipher.desc.tfm);
		if (size != params.iv_size)
			WARNING("size=%u iv_size=%u", size, params.iv_size);
		if (size > params.iv_size)
			size = params.iv_size;
		if (copy_from_user(iv, params.iv, size)) {
			ret = -EFAULT;
			goto abort;
		}
		TF_TRACE_ARRAY(iv, params.iv_size);
	}

	/* Success */
	return 0;

abort:
	ERROR("alg_name=%s", alg_name);
	tf_self_test_finalize(data);
	return ret;
}

static long tf_self_test_ioctl_blkcipher_update(struct file *filp,
						void __user *user_params)
{
	struct tf_self_test_ioctl_param_blkcipher_update params;
	struct tf_self_test_file_data *data = filp->private_data;
	struct scatterlist sg_in, sg_out;
	unsigned char in[256] = {0};
	unsigned char out[sizeof(in)] = "Uninitialized!";
	struct blkcipher_tfm *crt;
	int (*crypt)(struct blkcipher_desc *, struct scatterlist *,
		     struct scatterlist *, unsigned int);


	switch (data->type & CRYPTO_ALG_TYPE_MASK) {
	case CRYPTO_ALG_TYPE_BLKCIPHER:
	case CRYPTO_ALG_TYPE_ABLKCIPHER:
		break;
	default:
		return -EINVAL;
		break;
	}

	/* Copy parameters from user space */
	if (copy_from_user(&params, user_params, sizeof(params)))
		return -EFAULT;
	if (params.in == NULL || params.out == NULL)
		return -EINVAL;
	if (params.size > sizeof(in) || params.size > sizeof(out))
		return -ENOSPC;
	if (copy_from_user(in, params.in, params.size))
		return -EFAULT;

	/* Perform the encryption or decryption */
	sg_init_one(&sg_in, in, params.size);
	sg_init_one(&sg_out, out, params.size);
	crt = crypto_blkcipher_crt(data->cipher.desc.tfm);
	data->cipher.desc.info = crt->iv;
	crypt = (data->cipher.decrypt ? crt->decrypt : crt->encrypt);
	if (crypt(&data->cipher.desc, &sg_out, &sg_in, params.size))
		return -EIO;

	/* Copy the result */
	if (copy_to_user(params.out, out, params.size))
		return -EFAULT;

	/* Success */
	return 0;
}

static long tf_self_test_device_ioctl(
	struct file *filp, unsigned int ioctl_num,
	unsigned long ioctl_param)
{
	switch (ioctl_num) {
	case IOCTL_TF_SELF_TEST_POST:
		return tf_self_test_post_vectors();
	case IOCTL_TF_SELF_TEST_HASH_INIT:
		return tf_self_test_ioctl_hash_init(filp, (void *)ioctl_param);
		break;
	case IOCTL_TF_SELF_TEST_BLKCIPHER_INIT:
		return tf_self_test_ioctl_blkcipher_init(filp,
							 (void *)ioctl_param);
		break;
	case IOCTL_TF_SELF_TEST_BLKCIPHER_UPDATE:
		return tf_self_test_ioctl_blkcipher_update(filp,
							   (void *)ioctl_param);
		break;
	default:
		return -ENOTTY;
	}
}

static ssize_t tf_self_test_device_read(struct file *filp,
					char __user *buf, size_t count,
					loff_t *pos)
{
	struct tf_self_test_file_data *data = filp->private_data;
	INFO("type=%03x tfm=%d pos=%lu",
	     data->type, data->hash.desc.tfm != NULL, (unsigned long)*pos);
	switch (data->type & CRYPTO_ALG_TYPE_MASK) {
	case CRYPTO_ALG_TYPE_HASH:
	case CRYPTO_ALG_TYPE_SHASH:
	case CRYPTO_ALG_TYPE_AHASH:
		if (data->hash.desc.tfm != NULL) {
			/*Stop accepting input and calculate the hash*/
			if (crypto_hash_final(&data->hash.desc,
					      data->hash.buf)) {
				ERROR("crypto_hash_final failed");
				return -EIO;
			}
			crypto_free_hash(data->hash.desc.tfm);
			data->hash.desc.tfm = NULL;
			{
				unsigned char *buf = data->hash.buf;
				TF_TRACE_ARRAY(buf, data->hash.digestsize);
			}
			/*Reset the file position for the read part*/
			*pos = 0;
		}
		if (*pos < 0 || *pos >= data->hash.digestsize)
			return 0;
		if (*pos + count > data->hash.digestsize)
			count = data->hash.digestsize - *pos;
		if (copy_to_user(buf, data->hash.buf, count))
			return -EFAULT;
		*pos += count;
		break;
	default:
		return -ENXIO;
		break;
	}
	return count;
}

static ssize_t tf_self_test_device_write(struct file *filp,
					 const char __user *buf, size_t count,
					 loff_t *pos)
{
	struct tf_self_test_file_data *data = filp->private_data;
	INFO("type=%03x tfm=%d pos=%lu",
	     data->type, data->hash.desc.tfm != NULL, (unsigned long)*pos);
	switch (data->type & CRYPTO_ALG_TYPE_MASK) {
	case CRYPTO_ALG_TYPE_HASH:
	case CRYPTO_ALG_TYPE_SHASH:
	case CRYPTO_ALG_TYPE_AHASH:
		if (IS_ERR_OR_NULL(data->hash.desc.tfm)) {
			/*We are no longer accepting input*/
			return -EROFS;
		}
		if (count > data->hash.buffer_size)
			count = data->hash.buffer_size;
		if (copy_from_user(data->hash.buf, buf, count))
			return -EFAULT;
		TF_TRACE_ARRAY(buf, count);
		{
			unsigned char *ptr = data->hash.buf;
			TF_TRACE_ARRAY(ptr, count);
		}
		sg_init_one(&data->sg[0], data->hash.buf, count);
		if (crypto_hash_update(&data->hash.desc,
				       &data->sg[0], count)) {
			ERROR("crypto_hash_update failed");
			tf_self_test_finalize(data);
			return -EIO;
		}
		*pos += count;
		break;
	default:
		return -ENXIO;
		break;
	}
	return count;
}



static const struct file_operations tf_self_test_device_file_ops = {
	.owner = THIS_MODULE,
	.open = tf_self_test_device_open,
	.release = tf_self_test_device_release,
	.unlocked_ioctl = tf_self_test_device_ioctl,
	.read = tf_self_test_device_read,
	.write = tf_self_test_device_write,
	.llseek = no_llseek,
};

struct tf_self_test_device {
	dev_t devno;
	struct cdev cdev;
	struct class *class;
	struct device *device;
};
static struct tf_self_test_device tf_self_test_device;

int __init tf_self_test_register_device(void)
{
	struct tf_device *tf_device = tf_get_device();
	int error;

	tf_self_test_device.devno =
		MKDEV(MAJOR(tf_device->dev_number),
		      TF_SELF_TEST_DEVICE_MINOR_NUMBER);
	error = register_chrdev_region(tf_self_test_device.devno, 1,
				       TF_SELF_TEST_DEVICE_BASE_NAME);
	if (error != 0) {
		ERROR("register_chrdev_region failed (error %d)", error);
		goto register_chrdev_region_failed;
	}
	cdev_init(&tf_self_test_device.cdev, &tf_self_test_device_file_ops);
	tf_self_test_device.cdev.owner = THIS_MODULE;
	error = cdev_add(&tf_self_test_device.cdev,
			 tf_self_test_device.devno, 1);
	if (error != 0) {
		ERROR("cdev_add failed (error %d)", error);
		goto cdev_add_failed;
	}
	tf_self_test_device.class =
		class_create(THIS_MODULE, TF_SELF_TEST_DEVICE_BASE_NAME);
	if (IS_ERR_OR_NULL(tf_self_test_device.class)) {
		ERROR("class_create failed (%d)",
		      (int)tf_self_test_device.class);
		goto class_create_failed;
	}
	tf_self_test_device.device =
		device_create(tf_self_test_device.class, NULL,
			      tf_self_test_device.devno,
			      NULL, TF_SELF_TEST_DEVICE_BASE_NAME);
	INFO("created device %s = %u:%u",
	     TF_SELF_TEST_DEVICE_BASE_NAME,
	     MAJOR(tf_self_test_device.devno),
	     MINOR(tf_self_test_device.devno));
	if (IS_ERR_OR_NULL(tf_self_test_device.device)) {
		ERROR("device_create failed (%d)",
		      (int)tf_self_test_device.device);
		goto device_create_failed;
	}

	return 0;

	/*__builtin_unreachable();*/
device_create_failed:
	if (!IS_ERR_OR_NULL(tf_self_test_device.class)) {
		device_destroy(tf_self_test_device.class,
			       tf_self_test_device.devno);
		class_destroy(tf_self_test_device.class);
	}
class_create_failed:
	tf_self_test_device.class = NULL;
	cdev_del(&tf_self_test_device.cdev);
cdev_add_failed:
	cdev_init(&tf_self_test_device.cdev, NULL);
	unregister_chrdev_region(tf_self_test_device.devno, 1);
register_chrdev_region_failed:
	return error;
}

void __exit tf_self_test_unregister_device(void)
{
	if (!IS_ERR_OR_NULL(tf_self_test_device.class)) {
		device_destroy(tf_self_test_device.class,
			       tf_self_test_device.devno);
		class_destroy(tf_self_test_device.class);
	}
	tf_self_test_device.class = NULL;
	if (tf_self_test_device.cdev.owner == THIS_MODULE)
		cdev_del(&tf_self_test_device.cdev);
	unregister_chrdev_region(tf_self_test_device.devno, 1);
}
