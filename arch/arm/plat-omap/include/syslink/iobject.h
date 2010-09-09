/*
 *  iobject.h
 *
 *  Interface to provide object creation facilities.
 *
 *  Copyright (C) 2008-2009 Texas Instruments, Inc.
 *
 *  This package is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 *  IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 *  WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 *  PURPOSE.
 */


#ifndef _IOBJECT_H_
#define _IOBJECT_H_

/* ObjType */
enum ipc_obj_type {
	IPC_OBJTYPE_CREATESTATIC = 0x1,
	IPC_OBJTYPE_CREATESTATIC_REGION = 0x2,
	IPC_OBJTYPE_CREATEDYNAMIC = 0x4,
	IPC_OBJTYPE_CREATEDYNAMIC_REGION = 0x8,
	IPC_OBJTYPE_OPENDYNAMIC = 0x10,
	IPC_OBJTYPE_LOCAL = 0x20
};


/* Object embedded in other module's object */
#define IOBJECT_SUPEROBJECT		\
	void *next;			\
	int status;

/* Generic macro to define a create/delete function for a module */
#define IOBJECT_CREATE0(MNAME) \
static struct MNAME##_object *MNAME##_first_object;\
\
\
void *MNAME##_create(const struct MNAME##_params *params)\
{ \
	int *key;\
	struct MNAME##_object *obj = (struct MNAME##_object *)\
					kmalloc(sizeof(struct MNAME##_object),\
					GFP_KERNEL);\
	if (!obj)\
		return NULL;\
	memset(obj, 0, sizeof(struct MNAME##_object));\
	obj->status = MNAME##_instance_init(obj, params);\
	if (obj->status == 0) { \
		key = gate_enter_system();\
		if (MNAME##_first_object == NULL) { \
			MNAME##_first_object = obj;\
			obj->next = NULL;\
		} else { \
			obj->next = MNAME##_first_object;\
			MNAME##_first_object = obj;\
		} \
		gate_leave_system(key);\
	} else { \
		kfree(obj);\
		obj = NULL;\
	} \
	return obj;\
} \
\
\
int MNAME##_delete(void **handle)\
{ \
	int *key;\
	struct MNAME##_object *temp;\
	struct MNAME##_object *obj;\
	\
	if (handle == NULL) \
		return -EINVAL;\
	if (*handle == NULL) \
		return -EINVAL;\
	obj = (struct MNAME##_object *)(*handle);\
	key = gate_enter_system();\
	if (obj == MNAME##_first_object) \
		MNAME##_first_object = obj->next;\
	else { \
		temp = MNAME##_first_object;\
		while (temp) { \
			if (temp->next == obj) { \
				temp->next = obj->next;\
				break;\
			} else { \
				temp = temp->next;\
			} \
		} \
		if (temp == NULL) { \
			gate_leave_system(key);\
			return -EINVAL;\
		} \
	} \
	gate_leave_system(key);\
	MNAME##_instance_finalize(obj, obj->status);\
	kfree(obj);\
	*handle = NULL;\
	return 0;\
}

#define IOBJECT_CREATE1(MNAME, ARG) \
static struct MNAME##_object *MNAME##_first_object;\
\
\
void *MNAME##_create(ARG arg, const struct MNAME##_params *params)\
{ \
	int *key;\
	struct MNAME##_object *obj = (struct MNAME##_object *) \
					kmalloc(sizeof(struct MNAME##_object),\
					GFP_KERNEL);\
	if (!obj) \
		return NULL;\
	memset(obj, 0, sizeof(struct MNAME##_object));\
	obj->status = MNAME##_instance_init(obj, arg, params);\
	if (obj->status == 0) { \
		key = gate_enter_system();\
		if (MNAME##_first_object == NULL) { \
			MNAME##_first_object = obj;\
			obj->next = NULL;\
		} else { \
			obj->next = MNAME##_first_object;\
			MNAME##_first_object = obj;\
		} \
		gate_leave_system(key);\
	} else { \
		kfree(obj);\
		obj = NULL;\
	} \
	return obj;\
} \
\
\
int MNAME##_delete(void **handle)\
{ \
	int *key;\
	struct MNAME##_object *temp;\
	struct MNAME##_object *obj;\
	\
	if (handle == NULL) \
		return -EINVAL;\
	if (*handle == NULL) \
		return -EINVAL;\
	obj = (struct MNAME##_object *)(*handle);\
	key = gate_enter_system();\
	if (obj == MNAME##_first_object) \
		MNAME##_first_object = obj->next;\
	else { \
		temp = MNAME##_first_object;\
		while (temp) { \
			if (temp->next == obj) { \
				temp->next = obj->next;\
				break;\
			} else { \
				temp = temp->next;\
			} \
		} \
		if (temp == NULL) { \
			gate_leave_system(key);\
			return -EINVAL;\
		} \
	} \
	gate_leave_system(key);\
	MNAME##_instance_finalize(obj, obj->status);\
	kfree(obj);\
	*handle = NULL;\
	return 0;\
}


#endif /* ifndef _IOBJECT_H_ */
