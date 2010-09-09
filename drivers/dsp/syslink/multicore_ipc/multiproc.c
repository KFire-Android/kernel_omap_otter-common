/*
*  multiproc.c
*
*  Many multi-processor modules have the concept of processor id. MultiProc
*  centeralizes the processor id management.
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

/*
 *  ======== multiproc.c ========
 *  Notes:
 *  The processor id start at 0 and ascend without skipping values till maximum_
 *  no_of_processors - 1
 */

/* Standard headers */
#include <linux/types.h>
#include <linux/module.h>
#include <syslink/atomic_linux.h>
/* Utilities headers */
#include <linux/string.h>

/* Module level headers */
#include <multiproc.h>

/* Macro to make a correct module magic number with ref_count */
#define MULTIPROC_MAKE_MAGICSTAMP(x) ((MULTIPROC_MODULEID << 12u) | (x))

/*
 *  multiproc module state object
 */
struct multiproc_module_object {
	struct multiproc_config cfg; /*  Module configuration structure */
	struct multiproc_config def_cfg; /* Default module configuration */
	atomic_t ref_count; /* Reference count */
	u16 id;	/* Local processor ID */
};

static struct multiproc_module_object multiproc_state = {
	.def_cfg.num_processors = 4,
	.def_cfg.name_list[0][0] = 'T',
	.def_cfg.name_list[0][1] = 'e',
	.def_cfg.name_list[0][2] = 's',
	.def_cfg.name_list[0][3] = 'l',
	.def_cfg.name_list[0][4] = 'a',
	.def_cfg.name_list[1][0] = 'A',
	.def_cfg.name_list[1][1] = 'p',
	.def_cfg.name_list[1][2] = 'p',
	.def_cfg.name_list[1][3] = 'M',
	.def_cfg.name_list[1][4] = '3',
	.def_cfg.name_list[2][0] = 'S',
	.def_cfg.name_list[2][1] = 'y',
	.def_cfg.name_list[2][2] = 's',
	.def_cfg.name_list[2][3] = 'M',
	.def_cfg.name_list[2][4] = '3',
	.def_cfg.name_list[3][0] = 'M',
	.def_cfg.name_list[3][1] = 'P',
	.def_cfg.name_list[3][2] = 'U',
	.def_cfg.id = 3,
	.id = MULTIPROC_INVALIDID
};

/*
 * ========= multiproc_module =========
 *  Pointer to the MultiProc module state.
 */
static struct multiproc_module_object *multiproc_module = &multiproc_state;


/*
 * ======== multiproc_get_config ========
 *  Purpose:
 *  This will get the default configuration for the multiproc module
 */
void multiproc_get_config(struct multiproc_config *cfg)
{
	BUG_ON(cfg == NULL);
	if (atomic_cmpmask_and_lt(
			&(multiproc_module->ref_count),
			MULTIPROC_MAKE_MAGICSTAMP(0),
			MULTIPROC_MAKE_MAGICSTAMP(1)) == true) {
			/* (If setup has not yet been called) */
		memcpy(cfg, &multiproc_module->def_cfg,
				sizeof(struct multiproc_config));
	} else {
		memcpy(cfg, &multiproc_module->cfg,
				sizeof(struct multiproc_config));
	}
}
EXPORT_SYMBOL(multiproc_get_config);

/*
 * ======== multiproc_setup ========
 *  Purpose:
 *  This function sets up the multiproc module. This function
 *  must be called before any other instance-level APIs can be
 *  invoked
 */
s32 multiproc_setup(struct multiproc_config *cfg)
{
	s32	status = 0;
	struct multiproc_config tmp_cfg;

	/* This sets the ref_count variable is not initialized, upper 16 bits is
	* written with module Id to ensure correctness of ref_count variable.
	*/
	atomic_cmpmask_and_set(&multiproc_module->ref_count,
				MULTIPROC_MAKE_MAGICSTAMP(0),
				MULTIPROC_MAKE_MAGICSTAMP(0));

	if (atomic_inc_return(&multiproc_module->ref_count)
					!= MULTIPROC_MAKE_MAGICSTAMP(1u)) {
		status = 1;
	} else {
		if (cfg == NULL) {
			multiproc_get_config(&tmp_cfg);
			cfg = &tmp_cfg;
		}

		memcpy(&multiproc_module->cfg, cfg,
				sizeof(struct multiproc_config));
		multiproc_module->id = cfg->id;
	}

	return status;
}
EXPORT_SYMBOL(multiproc_setup);

/*
 * ======== multiproc_setup ========
 *  Purpose:
 *  This function destroy the multiproc module.
 *  Once this function is called, other multiproc module APIs,
 *  except for the multiproc_get_config API cannot be called
 *  anymore.
 */
s32 multiproc_destroy(void)
{
	int status = 0;

	if (atomic_cmpmask_and_lt(
			&(multiproc_module->ref_count),
			MULTIPROC_MAKE_MAGICSTAMP(0),
			MULTIPROC_MAKE_MAGICSTAMP(1)) == true) {
		status = -ENODEV;
		goto exit;
	}

	atomic_dec_return(&multiproc_module->ref_count);

exit:
	return status;
}
EXPORT_SYMBOL(multiproc_destroy);

/*
 * ======== multiProc_set_local_id ========
 *  Purpose:
 *  This will set the processor id of local processor on run time
 */
int multiproc_set_local_id(u16 proc_id)
{
	int status = 0;

	if (WARN_ON(atomic_cmpmask_and_lt(
			&(multiproc_module->ref_count),
			MULTIPROC_MAKE_MAGICSTAMP(0),
			MULTIPROC_MAKE_MAGICSTAMP(1)) == true)) {
		status = -ENODEV;
		goto exit;
	}

	if (WARN_ON(proc_id >= MULTIPROC_MAXPROCESSORS)) {
		status = -EINVAL;
		goto exit;
	}

	multiproc_module->cfg.id = proc_id;

exit:
	return status;
}
EXPORT_SYMBOL(multiproc_set_local_id);

/*
 * ======== multiProc_get_local_id ========
 *  Purpose:
 *  This will get the processor id from proccessor name
 */
u16 multiproc_get_id(const char *proc_name)
{
	s32 i;
	u16 proc_id = MULTIPROC_INVALIDID;

	if (WARN_ON(atomic_cmpmask_and_lt(
			&(multiproc_module->ref_count),
			MULTIPROC_MAKE_MAGICSTAMP(0),
			MULTIPROC_MAKE_MAGICSTAMP(1)) == true))
		goto exit;

	/* If the name is NULL, just return the local id */
	if (proc_name == NULL)
		proc_id = multiproc_module->cfg.id;
	else {
		for (i = 0; i < multiproc_module->cfg.num_processors ; i++) {
			if (strcmp(proc_name,
				&multiproc_module->cfg.name_list[i][0]) == 0) {
				proc_id = i;
				break;
			}
		}
	}

exit:
	return proc_id;
}
EXPORT_SYMBOL(multiproc_get_id);

/*
 * ======== multiProc_set_local_id ========
 *  Purpose:
 *  This will get the processor name from proccessor id
 */
char *multiproc_get_name(u16 proc_id)
{
	char *proc_name = NULL;

	/* On error condition return NULL pointer, else entry from name list */
	if (WARN_ON(atomic_cmpmask_and_lt(
			&(multiproc_module->ref_count),
			MULTIPROC_MAKE_MAGICSTAMP(0),
			MULTIPROC_MAKE_MAGICSTAMP(1)) == true))
		goto exit;

	if (WARN_ON(proc_id >= MULTIPROC_MAXPROCESSORS))
		goto exit;

	proc_name = multiproc_module->cfg.name_list[proc_id];

exit:
	return proc_name;
}
EXPORT_SYMBOL(multiproc_get_name);

/*
 * ======== multiproc_get_num_processors ========
 *  Purpose:
 *  This will get the number of processors in the system
 */
u16 multiproc_get_num_processors(void)
{
	return multiproc_module->cfg.num_processors;
}
EXPORT_SYMBOL(multiproc_get_num_processors);

/*
 * ======== multiproc_self ========
 *  Purpose:
 *  Return Id of current processor
 */
u16 multiproc_self(void)
{
	return multiproc_module->id;
}
EXPORT_SYMBOL(multiproc_self);

/*
 * ======== multiproc_get_slot ========
 * Determines the offset for any two processors.
 */
u32 multiproc_get_slot(u16 remote_proc_id)
{
	u32 slot = 0u;
	u32 i;
	u32 j;
	u32 small_id;
	u32 large_id;

	if (WARN_ON(atomic_cmpmask_and_lt(
			&(multiproc_module->ref_count),
			MULTIPROC_MAKE_MAGICSTAMP(0),
			MULTIPROC_MAKE_MAGICSTAMP(1)) == true))
		goto exit;

	if (remote_proc_id > multiproc_self()) {
		small_id = multiproc_self();
		large_id = remote_proc_id;
	} else {
		large_id = multiproc_self();
		small_id = remote_proc_id;
	}

	/* determine what offset to create for the remote Proc Id */
	for (i = 0; i < multiproc_module->cfg.num_processors; i++) {
		for (j = i + 1; j < multiproc_module->cfg.num_processors; j++) {
			if ((small_id == i) && (large_id == j))
				break;
			slot++;
		}
	}

exit:
	return slot;
}
EXPORT_SYMBOL(multiproc_get_slot);
