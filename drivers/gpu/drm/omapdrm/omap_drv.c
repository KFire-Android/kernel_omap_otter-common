/*
 * drivers/gpu/drm/omapdrm/omap_drv.c
 *
 * Copyright (C) 2011 Texas Instruments
 * Author: Rob Clark <rob@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "omap_drv.h"

#include "drm_crtc_helper.h"
#include "drm_fb_helper.h"
#include "omap_dmm_tiler.h"

#define DRIVER_NAME		MODULE_NAME
#define DRIVER_DESC		"OMAP DRM"
#define DRIVER_DATE		"20110917"
#define DRIVER_MAJOR		1
#define DRIVER_MINOR		0
#define DRIVER_PATCHLEVEL	0

struct drm_device *drm_device;

static int num_crtc = CONFIG_DRM_OMAP_NUM_CRTCS;

MODULE_PARM_DESC(num_crtc, "Number of overlays to use as CRTCs");
module_param(num_crtc, int, 0600);

/* TODO: think about how to handle more than one plugin.. ie. some ops
 * me might want to stop on the first plugin that doesn't return an
 * error, etc..
 */
LIST_HEAD(plugin_list);

/* keep track of whether we are already loaded.. we may need to call
 * plugin's load() if they register after we are already loaded
 */
static __read_mostly bool loaded = false;

/*
 * mode config funcs
 */

/* Notes about mapping DSS and DRM entities:
 *    CRTC:        overlay
 *    encoder:     manager.. with some extension to allow one primary CRTC
 *                 and zero or more video CRTC's to be mapped to one encoder?
 *    connector:   dssdev.. manager can be attached/detached from different
 *                 devices
 */

static void omap_fb_output_poll_changed(struct drm_device *dev)
{
	struct omap_drm_private *priv = dev->dev_private;
	DBG("dev=%p", dev);
	if (priv->fbdev)
		drm_fb_helper_hotplug_event(priv->fbdev);
}

static const struct drm_mode_config_funcs omap_mode_config_funcs = {
	.fb_create = omap_framebuffer_create,
	.output_poll_changed = omap_fb_output_poll_changed,
};

static int get_connector_type(struct omap_dss_device *dssdev)
{
	switch (dssdev->type) {
	case OMAP_DISPLAY_TYPE_HDMI:
		return DRM_MODE_CONNECTOR_HDMIA;
	case OMAP_DISPLAY_TYPE_DPI:
		if (!strcmp(dssdev->name, "dvi"))
			return DRM_MODE_CONNECTOR_DVID;
		/* fallthrough */
	default:
		return DRM_MODE_CONNECTOR_Unknown;
	}
}

static bool channel_used(struct drm_device *dev, enum omap_channel channel)
{
	struct omap_drm_private *priv = dev->dev_private;
	int i;

	for (i = 0; i < priv->num_crtcs; i++) {
		struct drm_crtc *crtc = priv->crtcs[i];

		if (omap_crtc_channel(crtc) == channel)
			return true;
	}

	return false;
}

static int omap_modeset_init(struct drm_device *dev)
{
	struct omap_drm_private *priv = dev->dev_private;
	struct omap_dss_device *dssdev = NULL;
	int num_ovls = dss_feat_get_num_ovls();
	int num_mgrs = dss_feat_get_num_mgrs();
	int num_crtcs;
	int i, id = 0;

	drm_mode_config_init(dev);

	omap_drm_irq_install(dev);

	/*
	 * We usually don't want to create a CRTC for each manager, at least
	 * not until we have a way to expose private planes to userspace.
	 * Otherwise there would not be enough video pipes left for drm planes.
	 * We use the num_crtc argument to limit the number of crtcs we create.
	 */
	num_crtcs = min3(num_crtc, num_mgrs, num_ovls);

	dssdev = NULL;

	for_each_dss_dev(dssdev) {
		struct drm_connector *connector;
		struct drm_encoder *encoder;
		enum omap_channel channel;

		if (!dssdev->driver) {
			dev_warn(dev->dev, "%s has no driver.. skipping it\n",
					dssdev->name);
			continue;
		}

		if (!(dssdev->driver->get_timings ||
					dssdev->driver->read_edid)) {
			dev_warn(dev->dev, "%s driver does not support "
				"get_timings or read_edid.. skipping it!\n",
				dssdev->name);
			continue;
		}

		encoder = omap_encoder_init(dev, dssdev);

		if (!encoder) {
			dev_err(dev->dev, "could not create encoder: %s\n",
					dssdev->name);
			return -ENOMEM;
		}

		connector = omap_connector_init(dev,
				get_connector_type(dssdev), dssdev, encoder);

		if (!connector) {
			dev_err(dev->dev, "could not create connector: %s\n",
					dssdev->name);
			return -ENOMEM;
		}

		BUG_ON(priv->num_encoders >= ARRAY_SIZE(priv->encoders));
		BUG_ON(priv->num_connectors >= ARRAY_SIZE(priv->connectors));

		priv->encoders[priv->num_encoders++] = encoder;
		priv->connectors[priv->num_connectors++] = connector;

		drm_mode_connector_attach_encoder(connector, encoder);

		/*
		 * if we have reached the limit of the crtcs we are allowed to
		 * create, let's not try to look for a crtc for this
		 * panel/encoder and onwards, we will, of course, populate the
		 * the possible_crtcs field for all the encoders with the final
		 * set of crtcs we create
		 */
		if (id == num_crtcs)
			continue;

		/*
		 * Getting the dssdev->channel for creation of crtc.
		 */
		channel = dssdev->channel;

		/*
		 * if this channel hasn't already been taken by a previously
		 * allocated crtc, we create a new crtc for it
		 */
		if (!channel_used(dev, channel)) {
			struct drm_plane *plane;
			struct drm_crtc *crtc;

			plane = omap_plane_init(dev, id, true);
			crtc = omap_crtc_init(dev, plane, channel, id);

			BUG_ON(priv->num_crtcs >= ARRAY_SIZE(priv->crtcs));
			priv->crtcs[id] = crtc;
			priv->num_crtcs++;

			priv->planes[id] = plane;
			priv->num_planes++;

			id++;
		}
	}

	/*
	 * we have allocated crtcs according to the need of the panels/encoders,
	 * adding more crtcs here if needed
	 */
	for (; id < num_crtcs; id++) {

		/* find a free manager for this crtc */
		for (i = 0; i < num_mgrs; i++) {
			if (!channel_used(dev, i)) {
				struct drm_plane *plane;
				struct drm_crtc *crtc;

				plane = omap_plane_init(dev, id, true);
				crtc = omap_crtc_init(dev, plane, i, id);

				BUG_ON(priv->num_crtcs >=
					ARRAY_SIZE(priv->crtcs));

				priv->crtcs[id] = crtc;
				priv->num_crtcs++;

				priv->planes[id] = plane;
				priv->num_planes++;

				break;
			} else {
				continue;
			}
		}

		if (i == num_mgrs) {
			/* this shouldn't really happen */
			dev_err(dev->dev, "no managers left for crtc\n");
			return -ENOMEM;
		}
	}

	/*
	 * Create normal planes for the remaining overlays:
	 */
	for (; id < num_ovls; id++) {
		struct drm_plane *plane = omap_plane_init(dev, id, false);

		BUG_ON(priv->num_planes >= ARRAY_SIZE(priv->planes));
		priv->planes[priv->num_planes++] = plane;
	}

	for (i = 0; i < priv->num_encoders; i++) {
		struct drm_encoder *encoder = priv->encoders[i];
		struct omap_dss_device *dssdev =
					omap_encoder_get_dssdev(encoder);

		/* figure out which crtc's we can connect the encoder to: */
		encoder->possible_crtcs = 0;
		for (id = 0; id < priv->num_crtcs; id++) {
			struct drm_crtc *crtc = priv->crtcs[id];
			enum omap_channel crtc_channel;
			enum omap_dss_output_id supported_outputs;

			crtc_channel = omap_crtc_channel(crtc);
			supported_outputs =
				dss_feat_get_supported_outputs(crtc_channel);

			if (supported_outputs & dssdev->output->id)
				encoder->possible_crtcs |= (1 << id);
		}
	}

	DBG("registered %d planes, %d crtcs, %d encoders and %d connectors\n",
		priv->num_planes, priv->num_crtcs, priv->num_encoders,
		priv->num_connectors);

	dev->mode_config.min_width = 32;
	dev->mode_config.min_height = 32;

	/* note: eventually will need some cpu_is_omapXYZ() type stuff here
	 * to fill in these limits properly on different OMAP generations..
	 */
	dev->mode_config.max_width = 4096;
	dev->mode_config.max_height = 2048;

	dev->mode_config.funcs = &omap_mode_config_funcs;

	return 0;
}

static void omap_modeset_free(struct drm_device *dev)
{
	drm_mode_config_cleanup(dev);
}

/*
 * drm ioctl funcs
 */


static int ioctl_get_param(struct drm_device *dev, void *data,
		struct drm_file *file_priv)
{
	struct omap_drm_private *priv = dev->dev_private;
	struct drm_omap_param *args = data;

	DBG("%p: param=%llu", dev, args->param);

	switch (args->param) {
	case OMAP_PARAM_CHIPSET_ID:
		args->value = priv->omaprev;
		break;
	default:
		DBG("unknown parameter %lld", args->param);
		return -EINVAL;
	}

	return 0;
}

static int ioctl_set_param(struct drm_device *dev, void *data,
		struct drm_file *file_priv)
{
	struct drm_omap_param *args = data;

	switch (args->param) {
	default:
		DBG("unknown parameter %lld", args->param);
		return -EINVAL;
	}

	return 0;
}

static int ioctl_get_base(struct drm_device *dev, void *data,
		struct drm_file *file_priv)
{
	struct drm_omap_get_base *args = data;
	struct omap_drm_plugin *plugin;

	/* be safe: */
	args->plugin_name[ARRAY_SIZE(args->plugin_name) - 1] = '\0';

	DBG("%p: plugin_name=%s", dev, args->plugin_name);

	list_for_each_entry(plugin, &plugin_list, list) {
		if (!strcmp(args->plugin_name, plugin->name)) {
			args->ioctl_base = plugin->ioctl_base;
			return 0;
		}
	}

	return -EINVAL;
}

static int ioctl_gem_new(struct drm_device *dev, void *data,
		struct drm_file *file_priv)
{
	struct drm_omap_gem_new *args = data;
	VERB("%p:%p: size=0x%08x, flags=%08x", dev, file_priv,
			args->size.bytes, args->flags);
	return omap_gem_new_handle(dev, file_priv, args->size,
			args->flags, &args->handle);
}

static int ioctl_gem_cpu_prep(struct drm_device *dev, void *data,
		struct drm_file *file_priv)
{
	struct drm_omap_gem_cpu_prep *args = data;
	struct drm_gem_object *obj;
	int ret;

	VERB("%p:%p: handle=%d, op=%x", dev, file_priv, args->handle, args->op);

	obj = drm_gem_object_lookup(dev, file_priv, args->handle);
	if (!obj)
		return -ENOENT;

	ret = omap_gem_op_sync(obj, args->op);

	if (!ret)
		ret = omap_gem_op_start(obj, args->op);

	drm_gem_object_unreference_unlocked(obj);

	return ret;
}

static int ioctl_gem_cpu_fini(struct drm_device *dev, void *data,
		struct drm_file *file_priv)
{
	struct drm_omap_gem_cpu_fini *args = data;
	struct drm_gem_object *obj;
	int ret;

	VERB("%p:%p: handle=%d", dev, file_priv, args->handle);

	obj = drm_gem_object_lookup(dev, file_priv, args->handle);
	if (!obj)
		return -ENOENT;

	/* XXX flushy, flushy */
	ret = 0;

	if (!ret)
		ret = omap_gem_op_finish(obj, args->op);

	drm_gem_object_unreference_unlocked(obj);

	return ret;
}

static int ioctl_gem_info(struct drm_device *dev, void *data,
		struct drm_file *file_priv)
{
	struct drm_omap_gem_info *args = data;
	struct drm_gem_object *obj;
	int ret = 0;

	VERB("%p:%p: handle=%d", dev, file_priv, args->handle);

	obj = drm_gem_object_lookup(dev, file_priv, args->handle);
	if (!obj)
		return -ENOENT;

	args->size = omap_gem_mmap_size(obj);
	args->offset = omap_gem_mmap_offset(obj);

	drm_gem_object_unreference_unlocked(obj);

	return ret;
}

struct drm_ioctl_desc ioctls[DRM_COMMAND_END - DRM_COMMAND_BASE] = {
	DRM_IOCTL_DEF_DRV(OMAP_GET_PARAM, ioctl_get_param, DRM_UNLOCKED|DRM_AUTH),
	DRM_IOCTL_DEF_DRV(OMAP_SET_PARAM, ioctl_set_param, DRM_UNLOCKED|DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),
	DRM_IOCTL_DEF_DRV(OMAP_GET_BASE, ioctl_get_base, DRM_UNLOCKED|DRM_AUTH),
	DRM_IOCTL_DEF_DRV(OMAP_GEM_NEW, ioctl_gem_new, DRM_UNLOCKED|DRM_AUTH),
	DRM_IOCTL_DEF_DRV(OMAP_GEM_CPU_PREP, ioctl_gem_cpu_prep, DRM_UNLOCKED|DRM_AUTH),
	DRM_IOCTL_DEF_DRV(OMAP_GEM_CPU_FINI, ioctl_gem_cpu_fini, DRM_UNLOCKED|DRM_AUTH),
	DRM_IOCTL_DEF_DRV(OMAP_GEM_INFO, ioctl_gem_info, DRM_UNLOCKED|DRM_AUTH),
};

/*
 * drm driver funcs
 */

/**
 * load - setup chip and create an initial config
 * @dev: DRM device
 * @flags: startup flags
 *
 * The driver load routine has to do several things:
 *   - initialize the memory manager
 *   - allocate initial config memory
 *   - setup the DRM framebuffer with the allocated memory
 */
static int dev_load(struct drm_device *dev, unsigned long flags)
{
	struct omap_drm_platform_data *pdata = dev->dev->platform_data;
	struct omap_drm_private *priv;
	struct omap_drm_plugin *plugin;
	int ret;

	DBG("load: dev=%p", dev);

	drm_device = dev;

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		dev_err(dev->dev, "could not allocate priv\n");
		return -ENOMEM;
	}

	priv->omaprev = pdata->omaprev;

	dev->dev_private = priv;

	priv->wq = alloc_ordered_workqueue("omapdrm", 0);

	INIT_LIST_HEAD(&priv->obj_list);

	omap_gem_init(dev);

	ret = omap_modeset_init(dev);
	if (ret) {
		dev_err(dev->dev, "omap_modeset_init failed: ret=%d\n", ret);
		dev->dev_private = NULL;
		kfree(priv);
		return ret;
	}

	ret = drm_vblank_init(dev, priv->num_crtcs);
	if (ret)
		dev_warn(dev->dev, "could not init vblank\n");

	priv->fbdev = omap_fbdev_init(dev);
	if (!priv->fbdev) {
		dev_warn(dev->dev, "omap_fbdev_init failed\n");
		/* well, limp along without an fbdev.. maybe X11 will work? */
	}

	/* store off drm_device for use in pm ops */
	dev_set_drvdata(dev->dev, dev);

	drm_kms_helper_poll_init(dev);

	loaded = true;

	list_for_each_entry(plugin, &plugin_list, list) {
		ret = plugin->load(dev, flags);
	}

	return 0;
}

static int dev_unload(struct drm_device *dev)
{
	struct omap_drm_private *priv = dev->dev_private;
	struct omap_drm_plugin *plugin;
	int ret;

	DBG("unload: dev=%p", dev);

	list_for_each_entry(plugin, &plugin_list, list) {
		ret = plugin->unload(dev);
	}

	drm_kms_helper_poll_fini(dev);
	drm_vblank_cleanup(dev);
	omap_drm_irq_uninstall(dev);

	omap_fbdev_free(dev);
	omap_modeset_free(dev);
	omap_gem_deinit(dev);

	flush_workqueue(priv->wq);
	destroy_workqueue(priv->wq);

	kfree(dev->dev_private);
	dev->dev_private = NULL;

	dev_set_drvdata(dev->dev, NULL);

	loaded = false;

	return 0;
}

static int dev_open(struct drm_device *dev, struct drm_file *file)
{
	struct omap_drm_plugin *plugin;
	bool found_pvr = false;
	int ret;

	file->driver_priv = kzalloc(sizeof(void *) * MAX_MAPPERS, GFP_KERNEL);

	DBG("open: dev=%p, file=%p", dev, file);

	list_for_each_entry(plugin, &plugin_list, list) {
		if (!strcmp(DRIVER_NAME "_pvr", plugin->name)) {
			found_pvr = true;
			break;
		}
	}

	if (!found_pvr) {
		DBG("open: PVR submodule not loaded.. let's try now");
		request_module(DRIVER_NAME "_pvr");
	}

	list_for_each_entry(plugin, &plugin_list, list) {
		ret = plugin->open(dev, file);
	}

	return 0;
}

static int dev_firstopen(struct drm_device *dev)
{
	DBG("firstopen: dev=%p", dev);
	return 0;
}

/**
 * lastclose - clean up after all DRM clients have exited
 * @dev: DRM device
 *
 * Take care of cleaning up after all DRM clients have exited.  In the
 * mode setting case, we want to restore the kernel's initial mode (just
 * in case the last client left us in a bad state).
 */
static void dev_lastclose(struct drm_device *dev)
{
	int i;

	/* we don't support vga-switcheroo.. so just make sure the fbdev
	 * mode is active
	 */
	struct omap_drm_private *priv = dev->dev_private;
	int ret;

	DBG("lastclose: dev=%p", dev);

	if (priv->rotation_prop) {
		/* need to restore default rotation state.. not sure
		 * if there is a cleaner way to restore properties to
		 * default state?  Maybe a flag that properties should
		 * automatically be restored to default state on
		 * lastclose?
		 */
		for (i = 0; i < priv->num_crtcs; i++) {
			drm_object_property_set_value(&priv->crtcs[i]->base,
					priv->rotation_prop, 0);
		}

		for (i = 0; i < priv->num_planes; i++) {
			drm_object_property_set_value(&priv->planes[i]->base,
					priv->rotation_prop, 0);
		}
	}

	mutex_lock(&dev->mode_config.mutex);
	ret = drm_fb_helper_restore_fbdev_mode(priv->fbdev);
	mutex_unlock(&dev->mode_config.mutex);
	if (ret)
		DBG("failed to restore crtc mode");
}

static void dev_preclose(struct drm_device *dev, struct drm_file *file)
{
	struct omap_drm_plugin *plugin;
	int ret;

	DBG("preclose: dev=%p", dev);

	list_for_each_entry(plugin, &plugin_list, list) {
		ret = plugin->release(dev, file);
	}

	kfree(file->driver_priv);
}

static void dev_postclose(struct drm_device *dev, struct drm_file *file)
{
	DBG("postclose: dev=%p, file=%p", dev, file);
}

static const struct vm_operations_struct omap_gem_vm_ops = {
	.fault = omap_gem_fault,
	.open = omap_gem_vm_open,
	.close = omap_gem_vm_close,
};

static const struct file_operations omapdriver_fops = {
		.owner = THIS_MODULE,
		.open = drm_open,
		.unlocked_ioctl = drm_ioctl,
		.release = drm_release,
		.mmap = omap_gem_mmap,
		.poll = drm_poll,
		.fasync = drm_fasync,
		.read = drm_read,
		.llseek = noop_llseek,
};

static struct drm_driver omap_drm_driver = {
		.driver_features =
				DRIVER_HAVE_IRQ | DRIVER_MODESET | DRIVER_GEM | DRIVER_PRIME,
		.load = dev_load,
		.unload = dev_unload,
		.open = dev_open,
		.firstopen = dev_firstopen,
		.lastclose = dev_lastclose,
		.preclose = dev_preclose,
		.postclose = dev_postclose,
		.get_vblank_counter = drm_vblank_count,
		.enable_vblank = omap_irq_enable_vblank,
		.disable_vblank = omap_irq_disable_vblank,
		.irq_preinstall = omap_irq_preinstall,
		.irq_postinstall = omap_irq_postinstall,
		.irq_uninstall = omap_irq_uninstall,
		.irq_handler = omap_irq_handler,
#ifdef CONFIG_DEBUG_FS
		.debugfs_init = omap_debugfs_init,
		.debugfs_cleanup = omap_debugfs_cleanup,
#endif
		.prime_handle_to_fd = drm_gem_prime_handle_to_fd,
		.prime_fd_to_handle = drm_gem_prime_fd_to_handle,
		.gem_prime_export = omap_gem_prime_export,
		.gem_prime_import = omap_gem_prime_import,
		.gem_init_object = omap_gem_init_object,
		.gem_free_object = omap_gem_free_object,
		.gem_vm_ops = &omap_gem_vm_ops,
		.dumb_create = omap_gem_dumb_create,
		.dumb_map_offset = omap_gem_dumb_map_offset,
		.dumb_destroy = omap_gem_dumb_destroy,
		.ioctls = ioctls,
		.num_ioctls = DRM_OMAP_NUM_IOCTLS,
		.fops = &omapdriver_fops,
		.name = DRIVER_NAME,
		.desc = DRIVER_DESC,
		.date = DRIVER_DATE,
		.major = DRIVER_MAJOR,
		.minor = DRIVER_MINOR,
		.patchlevel = DRIVER_PATCHLEVEL,
};

int omap_drm_register_plugin(struct omap_drm_plugin *plugin)
{
	struct drm_device *dev = drm_device;
	static int ioctl_base = DRM_OMAP_NUM_IOCTLS;
	int i;

	DBG("register plugin: %p (%s)", plugin, plugin->name);

	plugin->ioctl_base = ioctl_base;

	list_add_tail(&plugin->list, &plugin_list);

	/* register the plugin's ioctl's */
	for (i = 0; i < plugin->num_ioctls; i++) {
		int nr = i + ioctl_base;

		/* check for out of bounds ioctl nr or
		   already registered ioctl */
		if (nr > ARRAY_SIZE(ioctls) || ioctls[nr].func) {
			dev_err(dev->dev, "invalid ioctl: %d (nr=%d)\n", i, nr);
			return -EINVAL;
		}

		DBG("register ioctl: %d %08x", nr, plugin->ioctls[i].cmd);

		ioctls[nr] = plugin->ioctls[i];

		if (nr >= omap_drm_driver.num_ioctls)
			omap_drm_driver.num_ioctls = nr + 1;

	}

	ioctl_base += plugin->num_ioctls;

	if (loaded)
		plugin->load(dev, 0);

	return 0;
}
EXPORT_SYMBOL(omap_drm_register_plugin);

int omap_drm_unregister_plugin(struct omap_drm_plugin *plugin)
{
	list_del(&plugin->list);
	/* TODO remove ioctl fxns */
	return 0;
}
EXPORT_SYMBOL(omap_drm_unregister_plugin);

static int nmappers = 0;

/* create buffer mapper id, to access per-mapper private data.  See
 * omap_gem_{get,set}_priv().
 */
int omap_drm_register_mapper(void)
{
	if (nmappers >= MAX_MAPPERS)
		return -ENOMEM;
	return nmappers++;
}
EXPORT_SYMBOL(omap_drm_register_mapper);

/* retire a mapper id, previously acquired from omap_drm_register_mapper()
 */
void omap_drm_unregister_mapper(int mapper_id)
{
	/* currently no-op.. */
}
EXPORT_SYMBOL(omap_drm_unregister_mapper);

void *omap_drm_file_priv(struct drm_file *file, int mapper_id)
{
	BUG_ON((mapper_id >= MAX_MAPPERS) || (mapper_id < 0));
	if (file->driver_priv)
		return ((void **)file->driver_priv)[mapper_id];
	return NULL;
}
EXPORT_SYMBOL(omap_drm_file_priv);

void omap_drm_file_set_priv(struct drm_file *file, int mapper_id, void *priv)
{
	BUG_ON((mapper_id >= MAX_MAPPERS) || (mapper_id < 0));
	((void **)file->driver_priv)[mapper_id] = priv;
}
EXPORT_SYMBOL(omap_drm_file_set_priv);

static int pdev_suspend(struct platform_device *pDevice, pm_message_t state)
{
	DBG("");
	return 0;
}

static int pdev_resume(struct platform_device *device)
{
	DBG("");
	return 0;
}

static void pdev_shutdown(struct platform_device *device)
{
	DBG("");
}

static int pdev_probe(struct platform_device *device)
{
	DBG("%s", device->name);
	return drm_platform_init(&omap_drm_driver, device);
}

static int pdev_remove(struct platform_device *device)
{
	DBG("");
	drm_platform_exit(&omap_drm_driver, device);
	return 0;
}

#ifdef CONFIG_PM
static const struct dev_pm_ops omapdrm_pm_ops = {
	.resume = omap_gem_resume,
};
#endif

struct platform_driver pdev = {
		.driver = {
			.name = DRIVER_NAME,
			.owner = THIS_MODULE,
#ifdef CONFIG_PM
			.pm = &omapdrm_pm_ops,
#endif
		},
		.probe = pdev_probe,
		.remove = pdev_remove,
		.suspend = pdev_suspend,
		.resume = pdev_resume,
		.shutdown = pdev_shutdown,
};

static int __init omap_drm_init(void)
{
	DBG("init");
	return platform_driver_register(&pdev);
}

static void __exit omap_drm_fini(void)
{
	DBG("fini");
	platform_driver_unregister(&pdev);
}

/* need late_initcall() so we load after dss_driver's are loaded */
late_initcall(omap_drm_init);
module_exit(omap_drm_fini);

MODULE_AUTHOR("Rob Clark <rob@ti.com>");
MODULE_DESCRIPTION("OMAP DRM Display Driver");
MODULE_ALIAS("platform:" DRIVER_NAME);
MODULE_LICENSE("GPL v2");
