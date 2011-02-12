/*
 * drv.c
 *
 * DSP-BIOS Bridge driver support functions for TI OMAP processors.
 *
 * DSP/BIOS Bridge resource allocation module.
 *
 * Copyright (C) 2005-2006 Texas Instruments, Inc.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

/*  ----------------------------------- Host OS */
#include <dspbridge/host_os.h>

/*  ----------------------------------- DSP/BIOS Bridge */
#include <dspbridge/std.h>
#include <dspbridge/dbdefs.h>

/*  ----------------------------------- Trace & Debug */
#include <dspbridge/dbc.h>

/*  ----------------------------------- OS Adaptation Layer */
#include <dspbridge/cfg.h>
#include <dspbridge/list.h>

/*  ----------------------------------- This */
#include <dspbridge/drv.h>
#include <dspbridge/dev.h>

#include <dspbridge/node.h>
#include <dspbridge/proc.h>
#include <dspbridge/strm.h>
#include <dspbridge/nodepriv.h>
#include <dspbridge/wmdchnl.h>
#include <dspbridge/resourcecleanup.h>

/*  ----------------------------------- Defines, Data Structures, Typedefs */
struct drv_object {
	struct lst_list *dev_list;
	struct lst_list *dev_node_string;
};

/*
 *  This is the Device Extension. Named with the Prefix
 *  DRV_ since it is living in this module
 */
struct drv_ext {
	struct list_head link;
	char sz_string[MAXREGPATHLENGTH];
};

/*  ----------------------------------- Globals */
static s32 refs;
static bool ext_phys_mem_pool_enabled;
struct ext_phys_mem_pool {
	u32 phys_mem_base;
	u32 phys_mem_size;
	u32 virt_mem_base;
	u32 next_phys_alloc_ptr;
};
static struct ext_phys_mem_pool ext_mem_pool;

/*  ----------------------------------- Function Prototypes */
static int request_bridge_resources(struct cfg_hostres *res);


/* GPP PROCESS CLEANUP CODE */

static int drv_proc_free_node_res(int id, void *p, void *data);
extern enum node_state node_get_state(bhandle hnode);

/* Allocate and add a node resource element
* This function is called from .Node_Allocate. */
int drv_insert_node_res_element(bhandle hnode, bhandle hNodeRes,
				       bhandle hPCtxt)
{
	struct node_res_object **node_res_obj =
	    (struct node_res_object **)hNodeRes;
	struct process_context *ctxt = (struct process_context *)hPCtxt;
	int status = 0;
	int retval;

	*node_res_obj = kzalloc(sizeof(struct node_res_object), GFP_KERNEL);
	if (!*node_res_obj) {
		status = -ENOMEM;
		goto func_end;
	}

	(*node_res_obj)->hnode = hnode;
	retval = idr_get_new(ctxt->node_idp, *node_res_obj,
						&(*node_res_obj)->id);
	if (retval == -EAGAIN) {
		if (!idr_pre_get(ctxt->node_idp, GFP_KERNEL)) {
			pr_err("%s: OUT OF MEMORY\n", __func__);
			status = -ENOMEM;
			goto func_end;
		}

		retval = idr_get_new(ctxt->node_idp, *node_res_obj,
						&(*node_res_obj)->id);
	}
	if (retval) {
		pr_err("%s: FAILED, IDR is FULL\n", __func__);
		status = -EPERM;
	}
func_end:
	if (DSP_FAILED(status))
		kfree(*node_res_obj);

	return status;
}

/* Release all Node resources and its context
 * Actual Node De-Allocation */
static int drv_proc_free_node_res(int id, void *p, void *data)
{
	struct process_context *ctxt = data;
	int status = 0;
	struct node_res_object *node_res_obj = p;
	u32 node_state;

	if (node_res_obj->node_allocated) {
		node_state = node_get_state(node_res_obj->hnode);
		if (node_state <= NODE_DELETING) {
			if ((node_state == NODE_RUNNING) ||
			    (node_state == NODE_PAUSED) ||
			    (node_state == NODE_TERMINATING))
				node_terminate(node_res_obj->hnode, &status);

			node_delete(node_res_obj, ctxt);
		}
	}

	return 0;
}

/* Release all Mapped and Reserved DMM resources */
int drv_remove_all_dmm_res_elements(bhandle hPCtxt)
{
	struct process_context *ctxt = (struct process_context *)hPCtxt;
	int status = 0;
	struct dmm_map_object *temp_map, *map_obj;
	struct dmm_rsv_object *temp_rsv, *rsv_obj;

	/* Free DMM mapped memory resources */
	list_for_each_entry_safe(map_obj, temp_map, &ctxt->dmm_map_list, link) {
		status = proc_un_map(ctxt->hprocessor,
				     (void *)map_obj->dsp_addr, ctxt);
		if (DSP_FAILED(status))
			pr_err("%s: proc_un_map failed!"
			       " status = %i\n", __func__, status);
	}

	/* Free DMM reserved memory resources */
	list_for_each_entry_safe(rsv_obj, temp_rsv, &ctxt->dmm_rsv_list, link) {
		status = proc_un_reserve_memory(ctxt->hprocessor, (void *)
						rsv_obj->dsp_reserved_addr,
						ctxt);
		if (DSP_FAILED(status))
			pr_err("%s: proc_un_reserve_memory failed!"
			       " status = %i\n", __func__, status);
	}
	return status;
}

/* Update Node allocation status */
void drv_proc_node_update_status(bhandle hNodeRes, s32 status)
{
	struct node_res_object *node_res_obj =
	    (struct node_res_object *)hNodeRes;
	DBC_ASSERT(hNodeRes != NULL);
	node_res_obj->node_allocated = status;
}

/* Update Node Heap status */
void drv_proc_node_update_heap_status(bhandle hNodeRes, s32 status)
{
	struct node_res_object *node_res_obj =
	    (struct node_res_object *)hNodeRes;
	DBC_ASSERT(hNodeRes != NULL);
	node_res_obj->heap_allocated = status;
}

/* Release all Node resources and its context
* This is called from .bridge_release.
 */
int drv_remove_all_node_res_elements(bhandle hPCtxt)
{
	struct process_context *ctxt = hPCtxt;

	idr_for_each(ctxt->node_idp, drv_proc_free_node_res, ctxt);
	idr_destroy(ctxt->node_idp);

	return 0;
}

/* Allocate the STRM resource element
* This is called after the actual resource is allocated
 */
int drv_proc_insert_strm_res_element(bhandle hStreamHandle,
					    bhandle hstrm_res, bhandle hPCtxt)
{
	struct strm_res_object **pstrm_res = hstrm_res;
	struct process_context *ctxt = hPCtxt;
	int status = 0;
	int retval;

	*pstrm_res = kzalloc(sizeof(struct strm_res_object), GFP_KERNEL);
	if (*pstrm_res == NULL) {
		status = -EFAULT;
		goto func_end;
	}

	(*pstrm_res)->hstream = hStreamHandle;
	retval = idr_get_new(ctxt->strm_idp, *pstrm_res,
						&(*pstrm_res)->id);
	if (retval == -EAGAIN) {
		if (!idr_pre_get(ctxt->strm_idp, GFP_KERNEL)) {
			pr_err("%s: OUT OF MEMORY\n", __func__);
			status = -ENOMEM;
			goto func_end;
		}

		retval = idr_get_new(ctxt->strm_idp, *pstrm_res,
						&(*pstrm_res)->id);
	}
	if (retval) {
		pr_err("%s: FAILED, IDR is FULL\n", __func__);
		status = -EPERM;
	}

func_end:
	return status;
}

static int drv_proc_free_strm_res(int id, void *p, void *data)
{
	struct process_context *ctxt = data;
	struct strm_res_object *strm_res = p;
	struct stream_info strm_info;
	struct dsp_streaminfo user;
	u8 **ap_buffer = NULL;
	u8 *buf_ptr;
	u32 ul_bytes;
	u32 dw_arg;
	s32 ul_buf_size;

	if (strm_res->num_bufs) {
		ap_buffer = kmalloc(strm_res->num_bufs * sizeof(u8 *),
					GFP_KERNEL);
		if (ap_buffer) {
			strm_free_buffer(strm_res, ap_buffer,
						strm_res->num_bufs, ctxt);
			kfree(ap_buffer);
		}
	}
	strm_info.user_strm = &user;
	user.number_bufs_in_stream = 0;
	strm_get_info(strm_res->hstream, &strm_info, sizeof(strm_info));
	while (user.number_bufs_in_stream--)
		strm_reclaim(strm_res->hstream, &buf_ptr, &ul_bytes,
			     (u32 *) &ul_buf_size, &dw_arg);
	strm_close(strm_res, ctxt);
	return 0;
}

/* Release all Stream resources and its context
* This is called from .bridge_release.
 */
int drv_remove_all_strm_res_elements(bhandle hPCtxt)
{
	struct process_context *ctxt = (struct process_context *)hPCtxt;

	idr_for_each(ctxt->strm_idp, drv_proc_free_strm_res, ctxt);
	idr_destroy(ctxt->strm_idp);

	return 0;
}

/* Updating the stream resource element */
int drv_proc_update_strm_res(u32 num_bufs, bhandle hstrm_res)
{
	int status = 0;
	struct strm_res_object **strm_res =
	    (struct strm_res_object **)hstrm_res;

	(*strm_res)->num_bufs = num_bufs;
	return status;
}

/* GPP PROCESS CLEANUP CODE END */

/*
 *  ======== = drv_create ======== =
 *  Purpose:
 *      DRV Object gets created only once during Driver Loading.
 */
int drv_create(OUT struct drv_object **phDRVObject)
{
	int status = 0;
	struct drv_object *pdrv_object = NULL;

	DBC_REQUIRE(phDRVObject != NULL);
	DBC_REQUIRE(refs > 0);

	pdrv_object = kzalloc(sizeof(struct drv_object), GFP_KERNEL);
	if (pdrv_object) {
		/* Create and Initialize List of device objects */
		pdrv_object->dev_list = kzalloc(sizeof(struct lst_list),
							GFP_KERNEL);
		if (pdrv_object->dev_list) {
			/* Create and Initialize List of device Extension */
			pdrv_object->dev_node_string =
				kzalloc(sizeof(struct lst_list), GFP_KERNEL);
			if (!(pdrv_object->dev_node_string)) {
				status = -EPERM;
			} else {
				INIT_LIST_HEAD(&pdrv_object->
					       dev_node_string->head);
				INIT_LIST_HEAD(&pdrv_object->dev_list->head);
			}
		} else {
			status = -ENOMEM;
		}
	} else {
		status = -ENOMEM;
	}
	/* Store the DRV Object in the Registry */
	if (DSP_SUCCEEDED(status))
		status = cfg_set_object((u32) pdrv_object, REG_DRV_OBJECT);
	if (DSP_SUCCEEDED(status)) {
		*phDRVObject = pdrv_object;
	} else if (pdrv_object) {
		kfree(pdrv_object->dev_list);
		kfree(pdrv_object->dev_node_string);
		/* Free the DRV Object */
		kfree(pdrv_object);
	}

	DBC_ENSURE(DSP_FAILED(status) || pdrv_object);
	return status;
}

/*
 *  ======== drv_exit ========
 *  Purpose:
 *      Discontinue usage of the DRV module.
 */
void drv_exit(void)
{
	DBC_REQUIRE(refs > 0);

	refs--;

	DBC_ENSURE(refs >= 0);
}

/*
 *  ======== = drv_destroy ======== =
 *  purpose:
 *      Invoked during bridge de-initialization
 */
int drv_destroy(struct drv_object *hDRVObject)
{
	int status = 0;
	struct drv_object *pdrv_object = (struct drv_object *)hDRVObject;

	DBC_REQUIRE(refs > 0);
	DBC_REQUIRE(pdrv_object);

	/*
	 *  Delete the List if it exists.Should not come here
	 *  as the drv_remove_dev_object and the Last drv_request_resources
	 *  removes the list if the lists are empty.
	 */
	kfree(pdrv_object->dev_list);
	kfree(pdrv_object->dev_node_string);
	kfree(pdrv_object);
	/* Update the DRV Object in Registry to be 0 */
	(void)cfg_set_object(0, REG_DRV_OBJECT);

	return status;
}

/*
 *  ======== drv_get_dev_object ========
 *  Purpose:
 *      Given a index, returns a handle to DevObject from the list.
 */
int drv_get_dev_object(u32 index, struct drv_object *hdrv_obj,
			      struct dev_object **phDevObject)
{
	int status = 0;
#ifdef CONFIG_BRIDGE_DEBUG
	/* used only for Assertions and debug messages */
	struct drv_object *pdrv_obj = (struct drv_object *)hdrv_obj;
#endif
	struct dev_object *dev_obj;
	u32 i;
	DBC_REQUIRE(pdrv_obj);
	DBC_REQUIRE(phDevObject != NULL);
	DBC_REQUIRE(index >= 0);
	DBC_REQUIRE(refs > 0);
	DBC_ASSERT(!(LST_IS_EMPTY(pdrv_obj->dev_list)));

	dev_obj = (struct dev_object *)drv_get_first_dev_object();
	for (i = 0; i < index; i++) {
		dev_obj =
		    (struct dev_object *)drv_get_next_dev_object((u32) dev_obj);
	}
	if (dev_obj) {
		*phDevObject = (struct dev_object *)dev_obj;
	} else {
		*phDevObject = NULL;
		status = -EPERM;
	}

	return status;
}

/*
 *  ======== drv_get_first_dev_object ========
 *  Purpose:
 *      Retrieve the first Device Object handle from an internal linked list of
 *      of DEV_OBJECTs maintained by DRV.
 */
u32 drv_get_first_dev_object(void)
{
	u32 dw_dev_object = 0;
	struct drv_object *pdrv_obj;

	if (DSP_SUCCEEDED(cfg_get_object((u32 *) &pdrv_obj, REG_DRV_OBJECT))) {
		if ((pdrv_obj->dev_list != NULL) &&
		    !LST_IS_EMPTY(pdrv_obj->dev_list))
			dw_dev_object = (u32) lst_first(pdrv_obj->dev_list);
	}

	return dw_dev_object;
}

/*
 *  ======== DRV_GetFirstDevNodeString ========
 *  Purpose:
 *      Retrieve the first Device Extension from an internal linked list of
 *      of Pointer to dev_node Strings maintained by DRV.
 */
u32 drv_get_first_dev_extension(void)
{
	u32 dw_dev_extension = 0;
	struct drv_object *pdrv_obj;

	if (DSP_SUCCEEDED(cfg_get_object((u32 *) &pdrv_obj, REG_DRV_OBJECT))) {

		if ((pdrv_obj->dev_node_string != NULL) &&
		    !LST_IS_EMPTY(pdrv_obj->dev_node_string)) {
			dw_dev_extension =
			    (u32) lst_first(pdrv_obj->dev_node_string);
		}
	}

	return dw_dev_extension;
}

/*
 *  ======== drv_get_next_dev_object ========
 *  Purpose:
 *      Retrieve the next Device Object handle from an internal linked list of
 *      of DEV_OBJECTs maintained by DRV, after having previously called
 *      drv_get_first_dev_object() and zero or more DRV_GetNext.
 */
u32 drv_get_next_dev_object(u32 hdev_obj)
{
	u32 dw_next_dev_object = 0;
	struct drv_object *pdrv_obj;

	DBC_REQUIRE(hdev_obj != 0);

	if (DSP_SUCCEEDED(cfg_get_object((u32 *) &pdrv_obj, REG_DRV_OBJECT))) {

		if ((pdrv_obj->dev_list != NULL) &&
		    !LST_IS_EMPTY(pdrv_obj->dev_list)) {
			dw_next_dev_object = (u32) lst_next(pdrv_obj->dev_list,
							    (struct list_head *)
							    hdev_obj);
		}
	}
	return dw_next_dev_object;
}

/*
 *  ======== drv_get_next_dev_extension ========
 *  Purpose:
 *      Retrieve the next Device Extension from an internal linked list of
 *      of pointer to DevNodeString maintained by DRV, after having previously
 *      called drv_get_first_dev_extension() and zero or more
 *      drv_get_next_dev_extension().
 */
u32 drv_get_next_dev_extension(u32 hDevExtension)
{
	u32 dw_dev_extension = 0;
	struct drv_object *pdrv_obj;

	DBC_REQUIRE(hDevExtension != 0);

	if (DSP_SUCCEEDED(cfg_get_object((u32 *) &pdrv_obj, REG_DRV_OBJECT))) {
		if ((pdrv_obj->dev_node_string != NULL) &&
		    !LST_IS_EMPTY(pdrv_obj->dev_node_string)) {
			dw_dev_extension =
			    (u32) lst_next(pdrv_obj->dev_node_string,
					   (struct list_head *)hDevExtension);
		}
	}

	return dw_dev_extension;
}

/*
 *  ======== drv_init ========
 *  Purpose:
 *      Initialize DRV module private state.
 */
int drv_init(void)
{
	s32 ret = 1;		/* function return value */

	DBC_REQUIRE(refs >= 0);

	if (ret)
		refs++;

	DBC_ENSURE((ret && (refs > 0)) || (!ret && (refs >= 0)));

	return ret;
}

/*
 *  ======== drv_insert_dev_object ========
 *  Purpose:
 *      Insert a DevObject into the list of Manager object.
 */
int drv_insert_dev_object(struct drv_object *hDRVObject,
				 struct dev_object *hdev_obj)
{
	int status = 0;
	struct drv_object *pdrv_object = (struct drv_object *)hDRVObject;

	DBC_REQUIRE(refs > 0);
	DBC_REQUIRE(hdev_obj != NULL);
	DBC_REQUIRE(pdrv_object);
	DBC_ASSERT(pdrv_object->dev_list);

	lst_put_tail(pdrv_object->dev_list, (struct list_head *)hdev_obj);

	DBC_ENSURE(DSP_SUCCEEDED(status)
		   && !LST_IS_EMPTY(pdrv_object->dev_list));

	return status;
}

/*
 *  ======== drv_remove_dev_object ========
 *  Purpose:
 *      Search for and remove a DeviceObject from the given list of DRV
 *      objects.
 */
int drv_remove_dev_object(struct drv_object *hDRVObject,
				 struct dev_object *hdev_obj)
{
	int status = -EPERM;
	struct drv_object *pdrv_object = (struct drv_object *)hDRVObject;
	struct list_head *cur_elem;

	DBC_REQUIRE(refs > 0);
	DBC_REQUIRE(pdrv_object);
	DBC_REQUIRE(hdev_obj != NULL);

	DBC_REQUIRE(pdrv_object->dev_list != NULL);
	DBC_REQUIRE(!LST_IS_EMPTY(pdrv_object->dev_list));

	/* Search list for p_proc_object: */
	for (cur_elem = lst_first(pdrv_object->dev_list); cur_elem != NULL;
	     cur_elem = lst_next(pdrv_object->dev_list, cur_elem)) {
		/* If found, remove it. */
		if ((struct dev_object *)cur_elem == hdev_obj) {
			lst_remove_elem(pdrv_object->dev_list, cur_elem);
			status = 0;
			break;
		}
	}
	/* Remove list if empty. */
	if (LST_IS_EMPTY(pdrv_object->dev_list)) {
		kfree(pdrv_object->dev_list);
		pdrv_object->dev_list = NULL;
	}
	DBC_ENSURE((pdrv_object->dev_list == NULL) ||
		   !LST_IS_EMPTY(pdrv_object->dev_list));

	return status;
}

/*
 *  ======== drv_request_resources ========
 *  Purpose:
 *      Requests  resources from the OS.
 */
int drv_request_resources(u32 dw_context, u32 *pDevNodeString)
{
	int status = 0;
	struct drv_object *pdrv_object;
	struct drv_ext *pszdev_node;

	DBC_REQUIRE(dw_context != 0);
	DBC_REQUIRE(pDevNodeString != NULL);

	/*
	 *  Allocate memory to hold the string. This will live untill
	 *  it is freed in the Release resources. Update the driver object
	 *  list.
	 */

	status = cfg_get_object((u32 *) &pdrv_object, REG_DRV_OBJECT);
	if (DSP_SUCCEEDED(status)) {
		pszdev_node = kzalloc(sizeof(struct drv_ext), GFP_KERNEL);
		if (pszdev_node) {
			lst_init_elem(&pszdev_node->link);
			strncpy(pszdev_node->sz_string,
				(char *)dw_context, MAXREGPATHLENGTH - 1);
			pszdev_node->sz_string[MAXREGPATHLENGTH - 1] = '\0';
			/* Update the Driver Object List */
			*pDevNodeString = (u32) pszdev_node->sz_string;
			lst_put_tail(pdrv_object->dev_node_string,
				     (struct list_head *)pszdev_node);
		} else {
			status = -ENOMEM;
			*pDevNodeString = 0;
		}
	} else {
		dev_dbg(bridge, "%s: Failed to get Driver Object from Registry",
			__func__);
		*pDevNodeString = 0;
	}

	DBC_ENSURE((DSP_SUCCEEDED(status) && pDevNodeString != NULL &&
		    !LST_IS_EMPTY(pdrv_object->dev_node_string)) ||
		   (DSP_FAILED(status) && *pDevNodeString == 0));

	return status;
}

/*
 *  ======== drv_release_resources ========
 *  Purpose:
 *      Releases  resources from the OS.
 */
int drv_release_resources(u32 dw_context, struct drv_object *hdrv_obj)
{
	int status = 0;
	struct drv_object *pdrv_object = (struct drv_object *)hdrv_obj;
	struct drv_ext *pszdev_node;

	/*
	 *  Irrespective of the status go ahead and clean it
	 *  The following will over write the status.
	 */
	for (pszdev_node = (struct drv_ext *)drv_get_first_dev_extension();
	     pszdev_node != NULL; pszdev_node = (struct drv_ext *)
	     drv_get_next_dev_extension((u32) pszdev_node)) {
		if (!pdrv_object->dev_node_string) {
			/* When this could happen? */
			continue;
		}
		if ((u32) pszdev_node == dw_context) {
			/* Found it */
			/* Delete from the Driver object list */
			lst_remove_elem(pdrv_object->dev_node_string,
					(struct list_head *)pszdev_node);
			kfree((void *)pszdev_node);
			break;
		}
		/* Delete the List if it is empty */
		if (LST_IS_EMPTY(pdrv_object->dev_node_string)) {
			kfree(pdrv_object->dev_node_string);
			pdrv_object->dev_node_string = NULL;
		}
	}
	return status;
}

/*
 *  ======== request_bridge_resources ========
 *  Purpose:
 *      Reserves shared memory for bridge.
 */
static int request_bridge_resources(struct cfg_hostres *res)
{
	int status = 0;
	struct cfg_hostres *host_res = res;

		/* num_mem_windows must not be more than CFG_MAXMEMREGISTERS */
		host_res->num_mem_windows = 2;
		/* First window is for DSP internal memory */

		host_res->dw_sys_ctrl_base = ioremap(OMAP_SYSC_BASE,
						     OMAP_SYSC_SIZE);
		dev_dbg(bridge, "dw_mem_base[0] 0x%x\n",
			host_res->dw_mem_base[0]);
		dev_dbg(bridge, "dw_mem_base[3] 0x%x\n",
			host_res->dw_mem_base[3]);
		dev_dbg(bridge, "dw_wd_timer_dsp_base %p\n",
			host_res->dw_wd_timer_dsp_base);
		dev_dbg(bridge, "dw_dmmu_base %p\n", host_res->dw_dmmu_base);

		/* for 24xx base port is not mapping the mamory for DSP
		 * internal memory TODO Do a ioremap here */
		/* Second window is for DSP external memory shared with MPU */

		/* for Linux, these are hard-coded values */
		host_res->birq_registers = 0;
		host_res->birq_attrib = 0;
		host_res->dw_offset_for_monitor = 0;
		host_res->dw_chnl_offset = 0;
		/* CHNL_MAXCHANNELS */
		host_res->dw_num_chnls = CHNL_MAXCHANNELS;
		host_res->dw_chnl_buf_size = 0x400;
	return status;
}

/*
 *  ======== drv_request_bridge_res_dsp ========
 *  Purpose:
 *      Reserves shared memory for bridge.
 */
int drv_request_bridge_res_dsp(void **phost_resources)
{
	int status = 0;
	struct cfg_hostres *host_res;
	u32 dw_buff_size;
	u32 dma_addr;
	u32 shm_size;
	struct drv_data *drv_datap = dev_get_drvdata(bridge);

	dw_buff_size = sizeof(struct cfg_hostres);

	host_res = kzalloc(dw_buff_size, GFP_KERNEL);

	if (host_res != NULL) {
		request_bridge_resources(host_res);
		/* num_mem_windows must not be more than CFG_MAXMEMREGISTERS */
		host_res->num_mem_windows = 4;

		host_res->dw_mem_base[0] = 0;
		host_res->dw_mem_base[2] = (u32) ioremap(OMAP_DSP_MEM1_BASE,
							 OMAP_DSP_MEM1_SIZE);
		host_res->dw_mem_base[3] = (u32) ioremap(OMAP_DSP_MEM2_BASE,
							 OMAP_DSP_MEM2_SIZE);
		host_res->dw_mem_base[4] = (u32) ioremap(OMAP_DSP_MEM3_BASE,
							 OMAP_DSP_MEM3_SIZE);
		host_res->dw_per_base = ioremap(OMAP_PER_CM_BASE,
						OMAP_PER_CM_SIZE);
		host_res->dw_per_pm_base = (u32) ioremap(OMAP_PER_PRM_BASE,
							 OMAP_PER_PRM_SIZE);
		host_res->dw_core_pm_base = (u32) ioremap(OMAP_CORE_PRM_BASE,
							  OMAP_CORE_PRM_SIZE);
		host_res->dw_dmmu_base = ioremap(OMAP_DMMU_BASE,
						 OMAP_DMMU_SIZE);
		host_res->dw_wd_timer_dsp_base = ioremap(OMAP_WDT3_BASE,
							 OMAP_WDT3_BASE);

		dev_dbg(bridge, "dw_mem_base[0] 0x%x\n",
			host_res->dw_mem_base[0]);
		dev_dbg(bridge, "dw_mem_base[1] 0x%x\n",
			host_res->dw_mem_base[1]);
		dev_dbg(bridge, "dw_mem_base[2] 0x%x\n",
			host_res->dw_mem_base[2]);
		dev_dbg(bridge, "dw_mem_base[3] 0x%x\n",
			host_res->dw_mem_base[3]);
		dev_dbg(bridge, "dw_mem_base[4] 0x%x\n",
			host_res->dw_mem_base[4]);
		dev_dbg(bridge, "dw_wd_timer_dsp_base %p\n",
			host_res->dw_wd_timer_dsp_base);
		dev_dbg(bridge, "dw_dmmu_base %p\n", host_res->dw_dmmu_base);

		shm_size = drv_datap->shm_size;
		if (shm_size >= 0x10000) {
			/* Allocate Physically contiguous,
			 * non-cacheable  memory */
			host_res->dw_mem_base[1] =
			    (u32) mem_alloc_phys_mem(shm_size, 0x100000,
						     &dma_addr);
			if (host_res->dw_mem_base[1] == 0) {
				status = -ENOMEM;
				pr_err("shm reservation Failed\n");
			} else {
				host_res->dw_mem_length[1] = shm_size;
				host_res->dw_mem_phys[1] = dma_addr;

				dev_dbg(bridge, "%s: Bridge shm address 0x%x "
					"dma_addr %x size %x\n", __func__,
					host_res->dw_mem_base[1],
					dma_addr, shm_size);
			}
		}
		if (DSP_SUCCEEDED(status)) {
			/* for Linux, these are hard-coded values */
			host_res->birq_registers = 0;
			host_res->birq_attrib = 0;
			host_res->dw_offset_for_monitor = 0;
			host_res->dw_chnl_offset = 0;
			/* CHNL_MAXCHANNELS */
			host_res->dw_num_chnls = CHNL_MAXCHANNELS;
			host_res->dw_chnl_buf_size = 0x400;
			dw_buff_size = sizeof(struct cfg_hostres);
		}
		*phost_resources = host_res;
	} else {
		status = -ENOMEM;
	}
	/* End Mem alloc */
	return status;
}

void mem_ext_phys_pool_init(u32 poolPhysBase, u32 poolSize)
{
	u32 pool_virt_base;

	/* get the virtual address for the physical memory pool passed */
	pool_virt_base = (u32) ioremap(poolPhysBase, poolSize);

	if ((void **)pool_virt_base == NULL) {
		pr_err("%s: external physical memory map failed\n", __func__);
		ext_phys_mem_pool_enabled = false;
	} else {
		ext_mem_pool.phys_mem_base = poolPhysBase;
		ext_mem_pool.phys_mem_size = poolSize;
		ext_mem_pool.virt_mem_base = pool_virt_base;
		ext_mem_pool.next_phys_alloc_ptr = poolPhysBase;
		ext_phys_mem_pool_enabled = true;
	}
}

void mem_ext_phys_pool_release(void)
{
	if (ext_phys_mem_pool_enabled) {
		iounmap((void *)(ext_mem_pool.virt_mem_base));
		ext_phys_mem_pool_enabled = false;
	}
}

/*
 *  ======== mem_ext_phys_mem_alloc ========
 *  Purpose:
 *     Allocate physically contiguous, uncached memory from external memory pool
 */

static void *mem_ext_phys_mem_alloc(u32 bytes, u32 align, OUT u32 * pPhysAddr)
{
	u32 new_alloc_ptr;
	u32 offset;
	u32 virt_addr;

	if (align == 0)
		align = 1;

	if (bytes > ((ext_mem_pool.phys_mem_base + ext_mem_pool.phys_mem_size)
		     - ext_mem_pool.next_phys_alloc_ptr)) {
		pPhysAddr = NULL;
		return NULL;
	} else {
		offset = (ext_mem_pool.next_phys_alloc_ptr & (align - 1));
		if (offset == 0)
			new_alloc_ptr = ext_mem_pool.next_phys_alloc_ptr;
		else
			new_alloc_ptr = (ext_mem_pool.next_phys_alloc_ptr) +
			    (align - offset);
		if ((new_alloc_ptr + bytes) <=
		    (ext_mem_pool.phys_mem_base + ext_mem_pool.phys_mem_size)) {
			/* we can allocate */
			*pPhysAddr = new_alloc_ptr;
			ext_mem_pool.next_phys_alloc_ptr =
			    new_alloc_ptr + bytes;
			virt_addr =
			    ext_mem_pool.virt_mem_base + (new_alloc_ptr -
							  ext_mem_pool.
							  phys_mem_base);
			return (void *)virt_addr;
		} else {
			*pPhysAddr = 0;
			return NULL;
		}
	}
}

/*
 *  ======== mem_alloc_phys_mem ========
 *  Purpose:
 *      Allocate physically contiguous, uncached memory
 */
void *mem_alloc_phys_mem(u32 byte_size, u32 ulAlign, OUT u32 * pPhysicalAddress)
{
	void *va_mem = NULL;
	dma_addr_t pa_mem;

	if (byte_size > 0) {
		if (ext_phys_mem_pool_enabled) {
			va_mem = mem_ext_phys_mem_alloc(byte_size, ulAlign,
							(u32 *) &pa_mem);
		} else
			va_mem = dma_alloc_coherent(NULL, byte_size, &pa_mem,
								GFP_KERNEL);
		if (va_mem == NULL)
			*pPhysicalAddress = 0;
		else
			*pPhysicalAddress = pa_mem;
	}
	return va_mem;
}

/*
 *  ======== mem_flush_cache ========
 *  Purpose:
 *      Flush cache
 */
void mem_flush_cache(void *pMemBuf, u32 byte_size, s32 FlushType)
{
	if (!pMemBuf)
		return;

	switch (FlushType) {
		/* invalidate only */
	case PROC_INVALIDATE_MEM:
		dmac_inv_range(pMemBuf, pMemBuf + byte_size);
		outer_inv_range(__pa((u32) pMemBuf), __pa((u32) pMemBuf +
							  byte_size));
		break;
		/* writeback only */
	case PROC_WRITEBACK_MEM:
		dmac_clean_range(pMemBuf, pMemBuf + byte_size);
		outer_clean_range(__pa((u32) pMemBuf), __pa((u32) pMemBuf +
							    byte_size));
		break;
		/* writeback and invalidate */
	case PROC_WRITEBACK_INVALIDATE_MEM:
		dmac_flush_range(pMemBuf, pMemBuf + byte_size);
		outer_flush_range(__pa((u32) pMemBuf), __pa((u32) pMemBuf +
							    byte_size));
		break;
	}

}

/*
 *  ======== mem_free_phys_mem ========
 *  Purpose:
 *      Free the given block of physically contiguous memory.
 */
void mem_free_phys_mem(void *pVirtualAddress, u32 pPhysicalAddress,
		       u32 byte_size)
{
	DBC_REQUIRE(pVirtualAddress != NULL);

	if (!ext_phys_mem_pool_enabled)
		dma_free_coherent(NULL, byte_size, pVirtualAddress,
				  pPhysicalAddress);
}
