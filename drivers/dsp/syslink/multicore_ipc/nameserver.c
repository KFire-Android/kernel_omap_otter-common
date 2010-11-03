/*
 *  nameserver.c
 *
 *  The nameserver module manages local name/value pairs that
 *  enables an application and other modules to store and retrieve
 *  values based on a name.
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
#include <linux/module.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <syslink/atomic_linux.h>

#include <nameserver.h>
#include <multiproc.h>
#include <nameserver_remote.h>

#define NS_MAX_NAME_LEN				32
#define NS_MAX_RUNTIME_ENTRY			(~0)
#define NS_MAX_VALUE_LEN			4

/*
 *  The dynamic name/value table looks like the following. This approach allows
 *  each instance table to have different value and different name lengths.
 *  The names block is allocated on the create. The size of that block is
 *  (max_runtime_entries * max_name_en). That block is sliced and diced up and
 *  given to each table entry.
 *  The same thing is done for the values block.
 *
 *     names                    table                  values
 *   -------------           -------------          -------------
 *   |           |<-\        |   elem    |   /----->|           |
 *   |           |   \-------|   name    |  /       |           |
 *   |           |           |   value   |-/        |           |
 *   |           |           |   len     |          |           |
 *   |           |<-\        |-----------|          |           |
 *   |           |   \       |   elem    |          |           |
 *   |           |    \------|   name    |  /------>|           |
 *   |           |           |   value   |-/        |           |
 *   -------------           |   len     |          |           |
 *                           -------------          |           |
 *                                                  |           |
 *                                                  |           |
 *                                                  -------------
 *
 *  There is an optimization for small values (e.g. <= sizeof(UInt32).
 *  In this case, there is no values block allocated. Instead the value
  *  field is used directly.  This optimization occurs and is managed when
 *  obj->max_value_len <= sizeof(Us3232).
 *
 *  The static create is a little different. The static entries point directly
 *  to a name string (and value). Since it points directly to static items,
 *  this entries cannot be removed.
 *  If max_runtime_entries is non-zero, a names and values block is created.
 *  Here is an example of a table with 1 static entry and 2 dynamic entries
 *
 *                           ------------
 *  this entries cannot be removed.
 *  If max_runtime_entries is non-zero, a names and values block is created.
 *  Here is an example of a table with 1 static entry and 2 dynamic entries
 *
 *                           ------------
 *                           |   elem   |
 *      "myName" <-----------|   name   |----------> someValue
 *                           |   value  |
 *     names                 |   len    |              values
 *   -------------           -------------          -------------
 *   |           |<-\        |   elem    |   /----->|           |
 *   |           |   \-------|   name    |  /       |           |
 *   |           |           |   value   |-/        |           |
 *   |           |           |   len     |          |           |
 *   |           |<-\        |-----------|          |           |
 *   |           |   \       |   elem    |          |           |
 *   |           |    \------|   name    |  /------>|           |
 *   |           |           |   value   |-/        |           |
 *   -------------           |   len     |          |           |
 *                           -------------          |           |
 *                                                  |           |
 *                                                  |           |
 *                                                  -------------
 *
 *  NameServerD uses a freeList and namelist to maintain the empty
 *  and filled-in entries. So when a name/value pair is added, an entry
 *  is pulled off the freeList, filled-in and placed on the namelist.
 *  The reverse happens on a remove.
 *
 *  For static adds, the entries are placed on the namelist statically.
 *
 *  For dynamic creates, the freeList is populated in postInt and there are no
 *  entries placed on the namelist (this happens when the add is called).
 *
 */

/* Macro to make a correct module magic number with refCount */
#define NAMESERVER_MAKE_MAGICSTAMP(x) ((NAMESERVER_MODULEID << 12u) | (x))

/*
 *  A name/value table entry
 */
struct nameserver_table_entry {
	struct list_head elem; /* List element */
	u32 hash; /* Hash value */
	char *name; /* Name portion of name/value pair */
	u32 len; /* Length of the value field. */
	void *buf; /* Value portion of name/value entry */
	bool collide; /* Does the hash collides? */
	struct nameserver_table_entry *next; /* Pointer to the next entry,
					used incase of collision only */
};

/*
 * A nameserver instance object
 */
struct nameserver_object {
	struct list_head elem;
	char *name; /* Name of the instance */
	struct list_head name_list; /* Filled entries list */
	struct mutex *gate_handle; /* Gate for critical regions */
	struct nameserver_params params; /* The parameter structure */
	u32 count; /* Counter for entries */
};


/* nameserver module state object */
struct nameserver_module_object {
	struct list_head obj_list; /* List holding created objects */
	struct mutex *mod_gate_handle; /* Handle to module gate */
	struct nameserver_remote_object **remote_handle_list;
	/* List of Remote driver handles for processors */
	atomic_t ref_count; /* Reference count */
	struct nameserver_params def_inst_params;
	/* Default instance paramters */
	struct nameserver_config def_cfg; /* Default module configuration */
	struct nameserver_config cfg; /* Module configuration */

};

/*
 * Variable for holding state of the nameserver module.
 */
static struct nameserver_module_object nameserver_state = {
	.def_cfg.reserved = 0x0,
	.def_inst_params.max_runtime_entries = 0u,
	.def_inst_params.table_heap = NULL,
	.def_inst_params.check_existing = true,
	.def_inst_params.max_value_len = 0u,
	.def_inst_params.max_name_len = 16u,
	.mod_gate_handle = NULL,
	.remote_handle_list = NULL,
};

/*
 *  Pointer to the SharedRegion module state
 */
static struct nameserver_module_object *nameserver_module = &(nameserver_state);

/*
 * Lookup table for CRC calculation.
 */
static const u32 nameserver_crc_table[256u] = {
  0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
  0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
  0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
  0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
  0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,
  0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
  0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c,
  0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
  0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
  0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
  0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190, 0x01db7106,
  0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
  0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
  0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
  0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
  0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
  0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
  0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
  0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa,
  0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
  0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
  0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
  0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
  0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
  0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
  0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
  0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
  0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
  0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
  0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
  0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
  0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
  0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
  0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
  0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
  0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
  0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
  0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
  0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
  0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
  0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693,
  0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
  0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d,
};

/* Function to calculate hash for a string */
static u32 _nameserver_string_hash(const char *string);

#if 0
/* This will return true if the entry is found in the table */
static bool _nameserver_is_entry_found(const char *name, u32 hash,
				struct list_head *list,
				struct nameserver_table_entry **entry);
#endif

/* This will return true if the hash is found in the table */
static bool _nameserver_is_hash_found(const char *name, u32 hash,
				struct list_head *list,
				struct nameserver_table_entry **entry);

/* This will return true if entry is found in the hash collide list */
static bool _nameserver_check_for_entry(const char *name,
					struct nameserver_table_entry **entry);

/* Function to get the default configuration for the NameServer module. */
void nameserver_get_config(struct nameserver_config *cfg)
{
	s32 retval = 0;

	if (WARN_ON(cfg == NULL)) {
		retval = -EINVAL;
		goto exit;
	}

	if (atomic_cmpmask_and_lt(&(nameserver_module->ref_count),
				NAMESERVER_MAKE_MAGICSTAMP(0),
				NAMESERVER_MAKE_MAGICSTAMP(1)) == true) {
		/* (If setup has not yet been called) */
		memcpy(cfg, &nameserver_module->def_cfg,
			sizeof(struct nameserver_config));
	} else {
		memcpy(cfg, &nameserver_module->cfg,
			sizeof(struct nameserver_config));
	}

exit:
	if (retval < 0)
		pr_err("nameserver_get_config failed! retval = 0x%x", retval);
	return;
}
EXPORT_SYMBOL(nameserver_get_config);

/* This will setup the nameserver module */
int nameserver_setup(void)
{
	struct nameserver_remote_object **list = NULL;
	s32 retval = 0;
	u16 nr_procs = 0;

	/* This sets the ref_count variable if not initialized, upper 16 bits is
	*   written with module Id to ensure correctness of refCount variable
	*/
	atomic_cmpmask_and_set(&nameserver_module->ref_count,
				NAMESERVER_MAKE_MAGICSTAMP(0),
				NAMESERVER_MAKE_MAGICSTAMP(0));

	if (atomic_inc_return(&nameserver_module->ref_count)
				!= NAMESERVER_MAKE_MAGICSTAMP(1)) {
		return 1;
	}

	INIT_LIST_HEAD(&nameserver_state.obj_list),

	nameserver_module->mod_gate_handle = kmalloc(sizeof(struct mutex),
						GFP_KERNEL);
	if (nameserver_module->mod_gate_handle == NULL) {
		retval = -ENOMEM;
		goto exit;
	}
	/* mutex is initialized with state = UNLOCKED */
	mutex_init(nameserver_module->mod_gate_handle);

	nr_procs = multiproc_get_num_processors();
	list = kmalloc(nr_procs * sizeof(struct nameserver_remote_object *),
					GFP_KERNEL);
	if (list == NULL) {
		retval = -ENOMEM;
		goto remote_alloc_fail;
	}
	memset(list, 0, nr_procs * sizeof(struct nameserver_remote_object *));
	nameserver_module->remote_handle_list = list;

	return 0;

remote_alloc_fail:
	kfree(nameserver_module->mod_gate_handle);
exit:
	pr_err("nameserver_setup failed, retval: %x\n", retval);
	return retval;
}
EXPORT_SYMBOL(nameserver_setup);

/* This will destroy the nameserver module */
int nameserver_destroy(void)
{
	s32 retval = 0;
	struct mutex *lock = NULL;

	if (WARN_ON(unlikely(atomic_cmpmask_and_lt(
				&(nameserver_module->ref_count),
				NAMESERVER_MAKE_MAGICSTAMP(0),
				NAMESERVER_MAKE_MAGICSTAMP(1)) == true))) {
		retval = -ENODEV;
		goto exit;
	}

	if (!(atomic_dec_return(&nameserver_module->ref_count)
					== NAMESERVER_MAKE_MAGICSTAMP(0))) {
		retval = 1;
		goto exit;
	}

	if (WARN_ON(nameserver_module->mod_gate_handle == NULL)) {
		retval = -ENODEV;
		goto exit;
	}

	/* If a nameserver instance exist, do not proceed  */
	if (!list_empty(&nameserver_module->obj_list)) {
		retval = -EBUSY;
		goto exit;
	}

	retval = mutex_lock_interruptible(nameserver_module->mod_gate_handle);
	if (retval)
		goto exit;

	lock = nameserver_module->mod_gate_handle;
	nameserver_module->mod_gate_handle = NULL;
	mutex_unlock(lock);
	kfree(lock);
	kfree(nameserver_module->remote_handle_list);
	nameserver_module->remote_handle_list = NULL;
	return 0;

exit:
	if (retval < 0)
		pr_err("nameserver_destroy failed, retval: %x\n", retval);
	return retval;
}
EXPORT_SYMBOL(nameserver_destroy);

/* Initialize this config-params structure with supplier-specified
 * defaults before instance creation. */
void nameserver_params_init(struct nameserver_params *params)
{
	s32 retval = 0;

	if (WARN_ON(unlikely(atomic_cmpmask_and_lt(
				&(nameserver_module->ref_count),
				NAMESERVER_MAKE_MAGICSTAMP(0),
				NAMESERVER_MAKE_MAGICSTAMP(1)) == true))) {
		retval = -ENODEV;
		goto exit;
	}
	if (WARN_ON(unlikely(params == NULL))) {
		retval = -EINVAL;
		goto exit;
	}

	memcpy(params, &nameserver_module->def_inst_params,
		sizeof(struct nameserver_params));

exit:
	if (retval < 0)
		pr_err("nameserver_params_init failed! status = 0x%x", retval);
	return;
}
EXPORT_SYMBOL(nameserver_params_init);

/* This will create a name server instance */
void *nameserver_create(const char *name,
			const struct nameserver_params *params)
{
	struct nameserver_object *new_obj = NULL;
	u32 name_len;
	s32 retval = 0;

	if (WARN_ON(unlikely(atomic_cmpmask_and_lt(
				&(nameserver_module->ref_count),
				NAMESERVER_MAKE_MAGICSTAMP(0),
				NAMESERVER_MAKE_MAGICSTAMP(1)) == true))) {
		retval = -ENODEV;
		goto exit;
	}
	if (WARN_ON(unlikely(params == NULL))) {
		retval = -EINVAL;
		goto exit;
	}
	if (WARN_ON(unlikely(name == NULL))) {
		retval = -EINVAL;
		goto exit;
	}

	name_len = strlen(name) + 1;
	if (name_len > params->max_name_len) {
		retval = -E2BIG;
		goto exit;
	}

	retval = mutex_lock_interruptible(nameserver_module->mod_gate_handle);
	if (retval)
		goto exit;

	/* check if the name is already registered or not */
	new_obj = nameserver_get_handle(name);
	if (new_obj != NULL) {
		retval = -EEXIST;
		goto error_handle;
	}

	new_obj = kmalloc(sizeof(struct nameserver_object), GFP_KERNEL);
	if (new_obj == NULL) {
		retval = -ENOMEM;
		goto error;
	}

	new_obj->name = kmalloc(name_len, GFP_ATOMIC);
	if (new_obj->name == NULL) {
		retval = -ENOMEM;
		goto error;
	}

	strncpy(new_obj->name, name, name_len);
	memcpy(&new_obj->params, params, sizeof(struct nameserver_params));
	if (params->max_value_len < sizeof(u32))
		new_obj->params.max_value_len = sizeof(u32);
	else
		new_obj->params.max_value_len = params->max_value_len;

	new_obj->gate_handle = kmalloc(sizeof(struct mutex), GFP_KERNEL);
	if (new_obj->gate_handle == NULL) {
			retval = -ENOMEM;
			goto error_mutex;
	}

	mutex_init(new_obj->gate_handle);
	new_obj->count = 0;
	/* Put in the nameserver instance to local list */
	INIT_LIST_HEAD(&new_obj->name_list);
	INIT_LIST_HEAD(&new_obj->elem);
	list_add(&new_obj->elem, &nameserver_module->obj_list);
	mutex_unlock(nameserver_module->mod_gate_handle);
	return (void *)new_obj;

error_mutex:
	kfree(new_obj->name);
error:
	kfree(new_obj);
error_handle:
	mutex_unlock(nameserver_module->mod_gate_handle);
exit:
	pr_err("nameserver_create failed retval:%x\n", retval);
	return NULL;
}
EXPORT_SYMBOL(nameserver_create);

/* Function to construct a name server. */
void nameserver_construct(void *handle, const char *name,
				const struct nameserver_params *params)
{
	struct nameserver_object *obj = NULL;
	u32 name_len = 0;
	s32 retval = 0;

	if (WARN_ON(unlikely(atomic_cmpmask_and_lt(
				&(nameserver_module->ref_count),
				NAMESERVER_MAKE_MAGICSTAMP(0),
				NAMESERVER_MAKE_MAGICSTAMP(1)) == true))) {
		retval = -ENODEV;
		goto exit;
	}
	if (WARN_ON(unlikely(handle == NULL))) {
		retval = -EINVAL;
		goto exit;
	}
	if (WARN_ON(unlikely(params == NULL))) {
		retval = -EINVAL;
		goto exit;
	}
	if (WARN_ON(unlikely(name == NULL))) {
		retval = -EINVAL;
		goto exit;
	}
	if (WARN_ON(unlikely(params->table_heap == NULL))) {
		retval = -EINVAL;
		goto exit;
	}
	/* check if the name is already registered or not */
	if (nameserver_get_handle(name)) {
		retval = -EEXIST; /* NameServer_E_ALREADYEXISTS */
		goto exit;
	}
	name_len = strlen(name) + 1;
	if (name_len > params->max_name_len) {
		retval = -E2BIG;
		goto exit;
	}

	obj = (struct nameserver_object *) handle;
	/* Allocate memory for the name */
	obj->name = kmalloc(name_len, GFP_ATOMIC);
	if (obj->name == NULL) {
		retval = -ENOMEM;
		goto exit;
	}

	/* Copy the name */
	strncpy(obj->name, name, strlen(name) + 1u);
	/* Copy the params */
	memcpy((void *) &obj->params, (void *) params,
			sizeof(struct nameserver_params));

	if (params->max_value_len < sizeof(u32))
		obj->params.max_value_len = sizeof(u32);
	else
		obj->params.max_value_len = params->max_value_len;

	/* Construct the list */
	INIT_LIST_HEAD(&obj->name_list);

	obj->gate_handle = kmalloc(sizeof(struct mutex), GFP_KERNEL);
	if (obj->gate_handle == NULL) {
		retval = -ENOMEM;
		goto exit;
	}
	mutex_init(obj->gate_handle);

	/* Initialize the count */
	obj->count = 0u;

	/* Put in the local list */
	retval = mutex_lock_interruptible(nameserver_module->mod_gate_handle);
	if (retval)
		goto exit;
	INIT_LIST_HEAD(&obj->elem);
	list_add(&obj->elem, &nameserver_module->obj_list);
	mutex_unlock(nameserver_module->mod_gate_handle);

exit:
	if (retval < 0)
		pr_err("nameserver_construct failed! retval = 0x%x", retval);
	return;
}

/* This will delete a name server instance */
int nameserver_delete(void **handle)
{
	struct nameserver_object *temp_obj = NULL;
	struct mutex *gate_handle = NULL;
	s32 retval = 0;

	if (WARN_ON(unlikely(atomic_cmpmask_and_lt(
				&(nameserver_module->ref_count),
				NAMESERVER_MAKE_MAGICSTAMP(0),
				NAMESERVER_MAKE_MAGICSTAMP(1)) == true))) {
		retval = -ENODEV;
		goto exit;
	}
	if (WARN_ON(unlikely(handle == NULL))) {
		retval = -EINVAL;
		goto exit;
	}
	if (WARN_ON(unlikely(*handle == NULL))) {
		retval = -EINVAL;
		goto exit;
	}

	temp_obj = (struct nameserver_object *) (*handle);
	if (WARN_ON(unlikely((temp_obj->name == NULL) &&
		(nameserver_get_handle(temp_obj->name) == NULL)))) {
		retval = -EINVAL;
		goto exit;
	}

	gate_handle = temp_obj->gate_handle;
	retval = mutex_lock_interruptible(gate_handle);
	if (retval)
		goto exit;

	/* Do not proceed if an entry in the in the table */
	if (temp_obj->count != 0) {
		retval = -EBUSY;
		goto error;
	}

	retval = mutex_lock_interruptible(nameserver_module->mod_gate_handle);
	if (retval)
		goto error;
	list_del(&temp_obj->elem);
	mutex_unlock(nameserver_module->mod_gate_handle);

	/* free the memory allocated for instance name */
	kfree(temp_obj->name);
	temp_obj->name = NULL;

	/* Free the memory used for handle */
	INIT_LIST_HEAD(&temp_obj->name_list);
	kfree(temp_obj);
	*handle = NULL;
	mutex_unlock(gate_handle);
	kfree(gate_handle);
	return 0;

error:
	mutex_unlock(gate_handle);
exit:
	pr_err("nameserver_delete failed retval:%x\n", retval);
	return retval;
}
EXPORT_SYMBOL(nameserver_delete);

/* Function to destroy a name server. */
void nameserver_destruct(void *handle)
{
	struct nameserver_object *obj = NULL;
	struct mutex *gate_handle = NULL;
	s32 retval = 0;

	if (WARN_ON(unlikely(atomic_cmpmask_and_lt(
				&(nameserver_module->ref_count),
				NAMESERVER_MAKE_MAGICSTAMP(0),
				NAMESERVER_MAKE_MAGICSTAMP(1)) == true))) {
		retval = -ENODEV;
		goto exit;
	}
	if (WARN_ON(unlikely(handle == NULL))) {
		retval = -EINVAL;
		goto exit;
	}

	obj = (struct nameserver_object *) handle;
	if (nameserver_get_handle(obj->name) == NULL) {
		retval = -EINVAL;
		goto exit;
	}

	/* enter the critical section */
	gate_handle = obj->gate_handle;
	retval = mutex_lock_interruptible(gate_handle);
	if (retval)
		goto exit;
	/* Do not proceed if an entry in the in the table */
	if (obj->count != 0) {
		retval = -EBUSY;
		goto error;
	}

	retval = mutex_lock_interruptible(nameserver_module->mod_gate_handle);
	if (retval)
		goto error;
	list_del(&obj->elem);
	mutex_unlock(nameserver_module->mod_gate_handle);

	/* free the memory allocated for the name */
	kfree(obj->name);
	obj->name = NULL;

	/* Destruct the list */
	INIT_LIST_HEAD(&obj->name_list);

	/* Free the memory used for obj */
	memset(obj, 0, sizeof(struct nameserver_object));

	/* leave the critical section */
	mutex_unlock(gate_handle);
	kfree(gate_handle);
	return;

error:
	/* leave the critical section */
	mutex_unlock(obj->gate_handle);

exit:
	pr_err("nameserver_destruct failed! status = 0x%x", retval);
	return;
}

/* This will add an entry into a nameserver instance */
void *nameserver_add(void *handle, const char *name,
		void *buf, u32 len)
{
	struct nameserver_table_entry *node = NULL;
	struct nameserver_table_entry *new_node = NULL;
	struct nameserver_object *temp_obj = NULL;
	bool found = false;
	bool exact_entry = false;
	u32 hash;
	u32 name_len;
	s32 retval = 0;

	if (WARN_ON(unlikely(atomic_cmpmask_and_lt(
				&(nameserver_module->ref_count),
				NAMESERVER_MAKE_MAGICSTAMP(0),
				NAMESERVER_MAKE_MAGICSTAMP(1)) == true))) {
		retval = -ENODEV;
		goto exit;
	}
	if (WARN_ON(unlikely(handle == NULL))) {
		retval = -EINVAL;
		goto exit;
	}
	if (WARN_ON(unlikely(name == NULL))) {
		retval = -EINVAL;
		goto exit;
	}
	if (WARN_ON(unlikely(buf == NULL))) {
		retval = -EINVAL;
		goto exit;
	}
	if (WARN_ON(unlikely(len == 0))) {
		retval = -EINVAL;
		goto exit;
	}

	temp_obj = (struct nameserver_object *)handle;
	retval = mutex_lock_interruptible(temp_obj->gate_handle);
	if (retval)
		goto exit;
	if (temp_obj->count >= temp_obj->params.max_runtime_entries) {
		retval = -ENOSPC;
		goto error;
	}

	/* make the null char in to account */
	name_len = strlen(name) + 1;
	if (name_len > temp_obj->params.max_name_len) {
		retval = -E2BIG;
		goto error;
	}

	/* TODO : hash and collide ?? */
	hash = _nameserver_string_hash(name);
	found = _nameserver_is_hash_found(name, hash,
					&temp_obj->name_list, &node);
	if (found == true)
		exact_entry = _nameserver_check_for_entry(name, &node);

	if (exact_entry == true && temp_obj->params.check_existing == true) {
		retval = -EEXIST;
		goto error;
	}

	new_node = kmalloc(sizeof(struct nameserver_table_entry), GFP_KERNEL);
	if (new_node == NULL) {
		retval = -ENOMEM;
		goto error;
	}

	new_node->hash    = hash;
	new_node->collide = found;
	new_node->len     = len;
	new_node->next    = NULL;
	new_node->name    = kmalloc(name_len, GFP_KERNEL);
	if (new_node->name == NULL) {
		retval = -ENOMEM;
		goto error_name;
	}
	new_node->buf  = kmalloc(len, GFP_KERNEL);
	if (new_node->buf == NULL) {
		retval = -ENOMEM;
		goto error_buf;
	}

	strncpy(new_node->name, name, name_len);
	memcpy(new_node->buf, buf, len);
	if (found == true) {
		/* If hash is found, need to stitch the list to link the
		* new node to the existing node with the same hash. */
		new_node->next = node->next;
		node->next = new_node;
		node->collide = found;
	} else
		list_add(&new_node->elem, &temp_obj->name_list);
	temp_obj->count++;
	mutex_unlock(temp_obj->gate_handle);
	return new_node;

error_buf:
	kfree(new_node->name);
error_name:
	kfree(new_node);
error:
	mutex_unlock(temp_obj->gate_handle);
exit:
	pr_err("nameserver_add failed status: %x\n", retval);
	return NULL;
}
EXPORT_SYMBOL(nameserver_add);

/* This will add a Uint32 value into a nameserver instance */
void *nameserver_add_uint32(void *handle, const char *name,
			u32 value)
{
	s32 retval = 0;
	struct nameserver_table_entry *new_node = NULL;

	if (WARN_ON(unlikely(atomic_cmpmask_and_lt(
				&(nameserver_module->ref_count),
				NAMESERVER_MAKE_MAGICSTAMP(0),
				NAMESERVER_MAKE_MAGICSTAMP(1)) == true))) {
		retval = -ENODEV;
		goto exit;
	}
	if (WARN_ON(unlikely(handle == NULL))) {
		retval = -EINVAL;
		goto exit;
	}
	if (WARN_ON(unlikely(name == NULL))) {
		retval = -EINVAL;
		goto exit;
	}

	new_node = nameserver_add(handle, name, &value, sizeof(u32));

exit:
	if (retval < 0 || new_node == NULL) {
		pr_err("nameserver_add_uint32 failed! status = 0x%x "
			"new_node = 0x%x", retval, (u32)new_node);
	}
	return new_node;
}
EXPORT_SYMBOL(nameserver_add_uint32);

/* This will remove a name/value pair from a name server */
int nameserver_remove(void *handle, const char *name)
{
	struct nameserver_object *temp_obj = NULL;
	struct nameserver_table_entry *entry = NULL;
	struct nameserver_table_entry *prev = NULL;
	bool found = false;
	u32 hash;
	u32 name_len;
	s32 retval = 0;

	if (WARN_ON(unlikely(atomic_cmpmask_and_lt(
				&(nameserver_module->ref_count),
				NAMESERVER_MAKE_MAGICSTAMP(0),
				NAMESERVER_MAKE_MAGICSTAMP(1)) == true))) {
		retval = -ENODEV;
		goto exit;
	}
	if (WARN_ON(unlikely(handle == NULL))) {
		retval = -EINVAL;
		goto exit;
	}
	if (WARN_ON(unlikely(name == NULL))) {
		retval = -EINVAL;
		goto exit;
	}

	temp_obj = (struct nameserver_object *)handle;
	name_len = strlen(name) + 1;
	if (name_len > temp_obj->params.max_name_len) {
		retval = -E2BIG;
		goto exit;
	}

	retval = mutex_lock_interruptible(temp_obj->gate_handle);
	if (retval)
		goto exit;

	hash = _nameserver_string_hash(name);
	found = _nameserver_is_hash_found(name, hash,
					&temp_obj->name_list, &entry);
	if (found == false) {
		retval = -ENOENT;
		goto error;
	}

	if (entry->collide == true) {
		if (strcmp(entry->name, name) == 0u) {
			kfree(entry->buf);
			kfree(entry->name);
			entry->hash = entry->next->hash;
			entry->name = entry->next->name;
			entry->len = entry->next->len;
			entry->buf = entry->next->buf;
			entry->collide = entry->next->collide;
			entry->next = entry->next->next;
			kfree(entry->next);
			temp_obj->count--;
		} else {
			found = false;
			prev = entry;
			entry = entry->next;
			while (entry) {
				if (strcmp(entry->name, name) == 0u) {
					kfree(entry->buf);
					kfree(entry->name);
					prev->next = entry->next;
					kfree(entry);
					temp_obj->count--;
					found = true;
					break;
				}
				prev = entry;
				entry = entry->next;
			}
			if (found == false) {
				retval = -ENOENT;
				goto error;
			}
		}
	} else {
		kfree(entry->buf);
		kfree(entry->name);
		list_del(&entry->elem);
		kfree(entry);
		temp_obj->count--;
	}

	mutex_unlock(temp_obj->gate_handle);
	return 0;

error:
	mutex_unlock(temp_obj->gate_handle);
exit:
	pr_err("nameserver_remove failed status:%x\n", retval);
	return retval;
}
EXPORT_SYMBOL(nameserver_remove);

/* This will  remove a name/value pair from a name server */
int nameserver_remove_entry(void *nshandle, void *nsentry)
{
	struct nameserver_table_entry *node = NULL;
	struct nameserver_object *obj = NULL;
	s32 retval = 0;

	if (WARN_ON(unlikely(atomic_cmpmask_and_lt(
				&(nameserver_module->ref_count),
				NAMESERVER_MAKE_MAGICSTAMP(0),
				NAMESERVER_MAKE_MAGICSTAMP(1)) == true))) {
		retval = -ENODEV;
		goto exit;
	}
	if (WARN_ON(unlikely(nshandle == NULL))) {
		retval = -EINVAL;
		goto exit;
	}
	if (WARN_ON(unlikely(nsentry == NULL))) {
		retval = -EINVAL;
		goto exit;
	}

	obj = (struct nameserver_object *)nshandle;
	node = (struct nameserver_table_entry *)nsentry;
	retval = mutex_lock_interruptible(obj->gate_handle);
	if (retval)
		goto exit;

	kfree(node->buf);
	kfree(node->name);
	list_del(&node->elem);
	kfree(node);
	obj->count--;
	mutex_unlock(obj->gate_handle);
	return 0;

exit:
	pr_err("nameserver_remove_entry failed status:%x\n", retval);
	return retval;
}
EXPORT_SYMBOL(nameserver_remove_entry);


/* This will retrieve the value portion of a name/value
 * pair from local table */
int nameserver_get_local(void *handle, const char *name,
			void *value, u32 *len)
{
	struct nameserver_object *temp_obj = NULL;
	struct nameserver_table_entry *entry = NULL;
	bool found = false;
	u32 hash;
	u32 length = 0;
	s32 retval = 0;

	if (WARN_ON(unlikely(atomic_cmpmask_and_lt(
				&(nameserver_module->ref_count),
				NAMESERVER_MAKE_MAGICSTAMP(0),
				NAMESERVER_MAKE_MAGICSTAMP(1)) == true))) {
		retval = -ENODEV;
		goto exit;
	}
	if (WARN_ON(unlikely(handle == NULL))) {
		retval = -EINVAL;
		goto exit;
	}
	if (WARN_ON(unlikely(name == NULL))) {
		retval = -EINVAL;
		goto exit;
	}
	if (WARN_ON(unlikely(value == NULL))) {
		retval = -EINVAL;
		goto exit;
	}
	if (WARN_ON(unlikely(len == NULL))) {
		retval = -EINVAL;
		goto exit;
	}
	if (WARN_ON(unlikely(*len == 0))) {
		retval = -EINVAL;
		goto exit;
	}

	length = *len;
	temp_obj = (struct nameserver_object *)handle;
	retval = mutex_lock_interruptible(temp_obj->gate_handle);
	if (retval)
		goto exit;

	hash = _nameserver_string_hash(name);
	found = _nameserver_is_hash_found(name, hash,
					&temp_obj->name_list, &entry);
	if (found == false) {
		retval = -ENOENT;
		goto error;
	}

	if (entry->collide == true) {
		found = _nameserver_check_for_entry(name, &entry);
		if (found == false) {
			retval = -ENOENT;
			goto error;
		}
	}

	if (entry->len >= length) {
		memcpy(value, entry->buf, length);
		*len = length;
	} else {
		memcpy(value, entry->buf, entry->len);
		*len = entry->len;
	}

error:
	mutex_unlock(temp_obj->gate_handle);

exit:
	if (retval < 0)
		pr_err("nameserver_get_local entry not found!\n");
	return retval;
}
EXPORT_SYMBOL(nameserver_get_local);

/* This will retrieve the value portion of a name/value
 * pair from local table */
int nameserver_get(void *handle, const char *name,
		void *value, u32 *len, u16 proc_id[])
{
	struct nameserver_object *temp_obj = NULL;
	u16 max_proc_id;
	u16 local_proc_id;
	s32 retval = -ENOENT;
	u32 i;

	if (WARN_ON(unlikely(atomic_cmpmask_and_lt(
				&(nameserver_module->ref_count),
				NAMESERVER_MAKE_MAGICSTAMP(0),
				NAMESERVER_MAKE_MAGICSTAMP(1)) == true))) {
		retval = -ENODEV;
		goto exit;
	}
	if (WARN_ON(unlikely(handle == NULL))) {
		retval = -EINVAL;
		goto exit;
	}
	if (WARN_ON(unlikely(name == NULL))) {
		retval = -EINVAL;
		goto exit;
	}
	if (WARN_ON(unlikely(value == NULL))) {
		retval = -EINVAL;
		goto exit;
	}
	if (WARN_ON(unlikely(len == NULL))) {
		retval = -EINVAL;
		goto exit;
	}
	if (WARN_ON(unlikely(*len == 0))) {
		retval = -EINVAL;
		goto exit;
	}

	temp_obj = (struct nameserver_object *)handle;
	max_proc_id = multiproc_get_num_processors();
	local_proc_id = multiproc_self();
	if (proc_id == NULL) {
		retval = nameserver_get_local(temp_obj, name, value, len);
		if (retval == -ENOENT) {
			for (i = 0; i < max_proc_id; i++) {
				/* Skip current processor */
				if (i == local_proc_id)
					continue;

				if (nameserver_module->remote_handle_list[i] \
					== NULL)
					continue;

				retval = nameserver_remote_get(
						nameserver_module->
						remote_handle_list[i],
						temp_obj->name, name, value,
						len, NULL);
				if (retval >= 0 || ((retval < 0) &&
					(retval != -ENOENT)))
					break;
			}
		}
		goto exit;
	}

	for (i = 0; i < max_proc_id; i++) {
		/* Skip processor with invalid id */
		if (proc_id[i] == MULTIPROC_INVALIDID)
			continue;

		if (i == local_proc_id) {
			retval = nameserver_get_local(temp_obj,
							name, value, len);
		} else {
			retval = nameserver_remote_get(
					nameserver_module->
					remote_handle_list[proc_id[i]],
					temp_obj->name, name, value, len, NULL);
		}
		if (retval >= 0 || ((retval < 0) && (retval != -ENOENT)))
			break;
	}

exit:
	if (retval < 0)
		pr_err("nameserver_get failed: status=%x\n", retval);
	return retval;
}
EXPORT_SYMBOL(nameserver_get);

/* Gets a 32-bit value by name */
int nameserver_get_uint32(void *handle, const char *name, void *value,
				u16 proc_id[])
{
	/* Initialize retval to not found */
	int retval = -ENOENT;
	u32 len = sizeof(u32);

	if (WARN_ON(unlikely(atomic_cmpmask_and_lt(
				&(nameserver_module->ref_count),
				NAMESERVER_MAKE_MAGICSTAMP(0),
				NAMESERVER_MAKE_MAGICSTAMP(1)) == true))) {
		retval = -ENODEV;
		goto exit;
	}
	if (WARN_ON(unlikely(handle == NULL))) {
		retval = -EINVAL;
		goto exit;
	}
	if (WARN_ON(unlikely(name == NULL))) {
		retval = -EINVAL;
		goto exit;
	}
	if (WARN_ON(unlikely(value == NULL))) {
		retval = -EINVAL;
		goto exit;
	}

	retval = nameserver_get(handle, name, value, &len, proc_id);

exit:
	/* -ENOENT is a valid run-time failure. */
	if ((retval < 0) && (retval != -ENOENT))
		pr_err("nameserver_get_uint32 failed! status = 0x%x", retval);
	return retval;
}
EXPORT_SYMBOL(nameserver_get_uint32);

/* Gets a 32-bit value by name from the local table
 *
 * If the name is found, the 32-bit value is copied into the value
 * argument and a success retval is returned.
 *
 * If the name is not found, zero is returned in len and the contents
 * of value are not modified. Not finding a name is not considered
 * an error.
 *
 * This function only searches the local name/value table. */
int nameserver_get_local_uint32(void *handle, const char *name, void *value)
{
	int retval = 0;
	u32 len = sizeof(u32);

	if (WARN_ON(unlikely(atomic_cmpmask_and_lt(
				&(nameserver_module->ref_count),
				NAMESERVER_MAKE_MAGICSTAMP(0),
				NAMESERVER_MAKE_MAGICSTAMP(1)) == true))) {
		retval = -ENODEV;
		goto exit;
	}
	if (WARN_ON(unlikely(handle == NULL))) {
		retval = -EINVAL;
		goto exit;
	}
	if (WARN_ON(unlikely(name == NULL))) {
		retval = -EINVAL;
		goto exit;
	}
	if (WARN_ON(unlikely(value == NULL))) {
		retval = -EINVAL;
		goto exit;
	}

	retval = nameserver_get_local(handle, name, value, &len);

exit:
	/* -ENOENT is a valid run-time failure. */
	if ((retval < 0) && (retval != -ENOENT)) {
		pr_err("nameserver_get_local_uint32 failed! "
			"status = 0x%x", retval);
	}
	return retval;
}
EXPORT_SYMBOL(nameserver_get_local_uint32);

/* This will retrieve the value portion of a name/value
 * pair from local table. Returns the number of characters that
 * matched with an entry. So if "abc" was an entry and you called
 * match with "abcd", this function will have the "abc" entry.
 * The return would be 3 since three characters matched */
int nameserver_match(void *handle, const char *name, u32 *value)
{
	struct nameserver_object *temp_obj = NULL;
	struct nameserver_table_entry *node = NULL;
	struct nameserver_table_entry *temp = NULL;
	u32 len = 0;
	u32 found_len = 0;
	s32 retval = 0;

	if (WARN_ON(unlikely(atomic_cmpmask_and_lt(
				&(nameserver_module->ref_count),
				NAMESERVER_MAKE_MAGICSTAMP(0),
				NAMESERVER_MAKE_MAGICSTAMP(1)) == true))) {
		retval = -ENODEV;
		goto exit;
	}
	if (WARN_ON(unlikely(handle == NULL))) {
		retval = -EINVAL;
		goto exit;
	}
	if (WARN_ON(unlikely(name == NULL))) {
		retval = -EINVAL;
		goto exit;
	}
	if (WARN_ON(unlikely(value == NULL))) {
		retval = -EINVAL;
		goto exit;
	}

	temp_obj = (struct nameserver_object *)handle;
	retval = mutex_lock_interruptible(temp_obj->gate_handle);
	if (retval)
		goto exit;
	list_for_each_entry(node, &temp_obj->name_list, elem) {
		temp = node;
		while (temp) {
			len = strlen(temp->name);
			if (len > found_len) {
				if (strncmp(temp->name, name, len) == 0u) {
					*value = (u32)temp->buf;
					found_len = len;
				}
			}
			temp = temp->next;
		}
	}
	mutex_unlock(temp_obj->gate_handle);

exit:
	if (retval < 0)
		pr_err("nameserver_match failed status:%x\n", retval);
	return found_len;
}
EXPORT_SYMBOL(nameserver_match);

/* This will get the handle of a nameserver instance from name */
void *nameserver_get_handle(const char *name)
{
	struct nameserver_object *obj = NULL;
	bool found = false;
	s32 retval = 0;

	if (WARN_ON(unlikely(atomic_cmpmask_and_lt(
				&(nameserver_module->ref_count),
				NAMESERVER_MAKE_MAGICSTAMP(0),
				NAMESERVER_MAKE_MAGICSTAMP(1)) == true))) {
		retval = -ENODEV;
		goto exit;
	}
	if (WARN_ON(unlikely(name == NULL))) {
		retval = -EINVAL;
		goto exit;
	}

	list_for_each_entry(obj, &nameserver_module->obj_list, elem) {
		if (strcmp(obj->name, name) == 0) {
			found = true;
			break;
		}
	}
	if (found == false) {
		retval = -ENOENT;
		goto exit;
	}
	return (void *)obj;

exit:
	pr_err("nameserver_get_handle failed! status = 0x%x", retval);
	return (void *)NULL;
}
EXPORT_SYMBOL(nameserver_get_handle);

/* =============================================================================
 * Internal functions
 * =============================================================================
 */
/* Function to register a remote driver for a processor */
int nameserver_register_remote_driver(void *handle, u16 proc_id)
{
	s32 retval = 0;
	u16 proc_count;

	if (WARN_ON(unlikely(atomic_cmpmask_and_lt(
				&(nameserver_module->ref_count),
				NAMESERVER_MAKE_MAGICSTAMP(0),
				NAMESERVER_MAKE_MAGICSTAMP(1)) == true))) {
		retval = -ENODEV;
		goto exit;
	}
	if (WARN_ON(unlikely(handle == NULL))) {
		retval = -EINVAL;
		goto exit;
	}

	proc_count = multiproc_get_num_processors();
	if (WARN_ON(unlikely(proc_id >= proc_count))) {
		retval = -EINVAL;
		goto exit;
	}

	nameserver_module->remote_handle_list[proc_id] = \
		(struct nameserver_remote_object *)handle;
	return 0;

exit:
	pr_err("nameserver_register_remote_driver failed! "
		"status:%x\n", retval);
	return retval;
}
EXPORT_SYMBOL(nameserver_register_remote_driver);

/* Function to unregister a remote driver. */
int nameserver_unregister_remote_driver(u16 proc_id)
{
	s32 retval = 0;
	u16 proc_count;

	if (WARN_ON(unlikely(atomic_cmpmask_and_lt(
				&(nameserver_module->ref_count),
				NAMESERVER_MAKE_MAGICSTAMP(0),
				NAMESERVER_MAKE_MAGICSTAMP(1)) == true))) {
		retval = -ENODEV;
		goto exit;
	}

	proc_count = multiproc_get_num_processors();
	if (WARN_ON(proc_id >= proc_count)) {
		retval = -EINVAL;
		goto exit;
	}

	nameserver_module->remote_handle_list[proc_id] = NULL;
	return 0;

exit:
	pr_err("nameserver_unregister_remote_driver failed! "
		"status:%x\n", retval);
	return retval;
}
EXPORT_SYMBOL(nameserver_unregister_remote_driver);

/* Determines if a remote driver is registered for the specified id. */
bool nameserver_is_registered(u16 proc_id)
{
	bool registered = false;
	s32 retval = 0;

	if (WARN_ON(unlikely(atomic_cmpmask_and_lt(
				&(nameserver_module->ref_count),
				NAMESERVER_MAKE_MAGICSTAMP(0),
				NAMESERVER_MAKE_MAGICSTAMP(1)) == true))) {
		retval = -ENODEV;
		goto exit;
	}
	if (WARN_ON(unlikely(proc_id >= multiproc_get_num_processors()))) {
		retval = -EINVAL;
		goto exit;
	}

	registered = (nameserver_module->remote_handle_list[proc_id] != NULL);

exit:
	if (retval < 0) {
		pr_err("nameserver_is_registered failed! "
			"status = 0x%x", retval);
	}
	return registered;
}
EXPORT_SYMBOL(nameserver_is_registered);

/* Function to calculate hash for a string */
static u32 _nameserver_string_hash(const char *string)
{
	u32 i;
	u32 hash ;
	u32 len = strlen(string);

	for (i = 0, hash = len; i < len; i++)
		hash = (hash >> 8) ^
			nameserver_crc_table[(hash & 0xff)] ^ string[i];

	return hash;
}

#if 0
/* This will return true if the entry is found in the table */
static bool _nameserver_is_entry_found(const char *name, u32 hash,
				struct list_head *list,
				struct nameserver_table_entry **entry)
{
	struct nameserver_table_entry *node = NULL;
	bool hash_match = false;
	bool name_match = false;

	list_for_each_entry(node, list, elem) {
		/* Hash not matches, take next node */
		if (node->hash == hash)
			hash_match = true;
		else
			continue;
		/* If the name matches, incase hash is duplicate */
		if (strcmp(node->name, name) == 0)
			name_match = true;

		if (hash_match && name_match) {
			if (entry != NULL)
				*entry = node;
			return true;
		}

		hash_match = false;
		name_match = false;
	}
	return false;
}
#endif

/* This will return true if the hash is found in the table */
static bool _nameserver_is_hash_found(const char *name, u32 hash,
				struct list_head *list,
				struct nameserver_table_entry **entry)
{
	struct nameserver_table_entry *node = NULL;

	/* No parameter checking as function is internal */

	list_for_each_entry(node, list, elem) {
		/* Hash not matches, take next node */
		if (node->hash == hash) {
			*entry = node;
			return true;
		}
	}
	return false;
}

/* This will return true if entry is found in the hash collide list */
static bool _nameserver_check_for_entry(const char *name,
					struct nameserver_table_entry **entry)
{
	struct nameserver_table_entry *temp = NULL;
	bool found = false;

	/* No parameter checking as function is internal */

	temp = *entry;
	while (temp) {
		if (strcmp(((struct nameserver_table_entry *)temp)->name,
				name) == 0u) {
			*entry = temp;
			found = true;
			break;
		}
		temp = temp->next;
	}

	return found;
}
