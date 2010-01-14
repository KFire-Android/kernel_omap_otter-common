#include <linux/init.h>
#include <linux/module.h>
#include <linux/cdev.h>    /* struct cdev */
#include <linux/kdev_t.h>  /* MKDEV() */
#include <linux/fs.h>      /* register_chrdev_region() */
#include <linux/device.h>  /* struct class */
#include <linux/platform_device.h> /* platform_device() */
#include <linux/err.h>     /* IS_ERR() */
#include <linux/uaccess.h> /* copy_to_user */
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/sched.h>
#include <linux/errno.h>

#include "tiler_deprecate.h"

int createlist(struct node **listhead)
{
	int error = -1;
	void *ret = NULL;

	*listhead = kmalloc(sizeof(struct node), GFP_KERNEL);
	if (*listhead == NULL) {
		printk(KERN_ERR "%s():%d: ERROR!\n", __func__, __LINE__);
		return error;
	}
	ret = memset(*listhead, 0x0, sizeof(struct node));
	if (ret != *listhead) {
		printk(KERN_ERR "%s():%d: ERROR!\n", __func__, __LINE__);
		return error;
	} else {
		/* printk(KERN_ERR "%s():%d: success!\n", __func__, __LINE__);*/
	}
	return 0;
}

int addnode(struct node *listhead, struct tiler_buf_info *ptr)
{
	int error = -1;
	struct node *tmpnode = NULL;
	struct node *newnode = NULL;
	void *ret = NULL;

	/* assert(listhead != NULL); */
	newnode = kmalloc(sizeof(struct node), GFP_KERNEL);
	if (newnode == NULL) {
		printk(KERN_ERR "%s():%d: ERROR!\n", __func__, __LINE__);
		return error;
	}
	ret = memset(newnode, 0x0, sizeof(struct node));
	if (ret != newnode) {
		printk(KERN_ERR "%s():%d: ERROR!\n", __func__, __LINE__);
		return error;
	}
	newnode->ptr = ptr;
	tmpnode = listhead;

	while (tmpnode->nextnode != NULL)
		tmpnode = tmpnode->nextnode;
	tmpnode->nextnode = newnode;

	return 0;
}

int removenode(struct node *listhead, int offset)
{
	struct node *node = NULL;
	struct node *tmpnode = NULL;

	node = listhead;

	while (node->nextnode != NULL) {
		if (node->nextnode->ptr->offset == offset) {
			tmpnode = node->nextnode;
			node->nextnode = tmpnode->nextnode;
			kfree(tmpnode->ptr);
			kfree(tmpnode);
			tmpnode = NULL;
			return 0;
		}
		node = node->nextnode;
	}
	return -1;
}

int tiler_destroy_buf_info_list(struct node *listhead)
{
	struct node *tmpnode = NULL;
	struct node *node = NULL;

	node = listhead;

	while (node->nextnode != NULL) {
		tmpnode = node->nextnode;
		node->nextnode = tmpnode->nextnode;
		kfree(tmpnode);
		tmpnode = NULL;
	}
	kfree(listhead);
	return 0;
}

int tiler_get_buf_info(struct node *listhead, struct tiler_buf_info **pp, int offst)
{
	struct node *node = NULL;

	node = listhead;

	while (node->nextnode != NULL) {
		if (node->nextnode->ptr->offset == offst) {
			*pp = node->nextnode->ptr;
			return 0;
		}
		node = node->nextnode;
	}
	return -1;
}

