#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#if HAVE_UDEV
#include <libudev.h>
#endif

#include "tdm_drm.h"
#include <tdm_helper.h>

//#define ENABLE_PP

#define TDM_DRM_NAME "vigs"

static tdm_drm_data *drm_data;

#ifdef HAVE_UDEV
static struct udev_device *
_tdm_find_primary_gpu(void)
{
	struct udev *udev;
	struct udev_enumerate *e;
	struct udev_list_entry *entry;
	const char *path, *id;
	struct udev_device *device, *drm_device, *pci;

	udev = udev_new();
	if (!udev) {
		TDM_ERR("fail to initialize udev context\n");
		return NULL;
	}

	e = udev_enumerate_new(udev);
	udev_enumerate_add_match_subsystem(e, "drm");
	udev_enumerate_add_match_sysname(e, "card[0-9]*");

	udev_enumerate_scan_devices(e);
	drm_device = NULL;
	udev_list_entry_foreach(entry, udev_enumerate_get_list_entry(e)) {
		path = udev_list_entry_get_name(entry);
		device = udev_device_new_from_syspath(udev, path);
		if (!device)
			continue;

		pci = udev_device_get_parent_with_subsystem_devtype(device,
		                "pci", NULL);
		if (pci) {
			id = udev_device_get_sysattr_value(pci, "boot_vga");
			if (id && !strcmp(id, "1")) {
				if (drm_device)
					udev_device_unref(drm_device);
				drm_device = device;
				break;
			}
		}

		if (!drm_device)
			drm_device = device;
		else
			udev_device_unref(device);
	}

	udev_enumerate_unref(e);
	return drm_device;
}

static tdm_error
_tdm_drm_udev_fd_handler(int fd, tdm_event_loop_mask mask, void *user_data)
{
	tdm_drm_data *edata = (tdm_drm_data*)user_data;
	struct udev_device *dev;
	const char *hotplug;
	struct stat s;
	dev_t udev_devnum;
	int ret;

	dev = udev_monitor_receive_device(edata->uevent_monitor);
	if (!dev) {
		TDM_ERR("couldn't receive device");
		return TDM_ERROR_OPERATION_FAILED;
	}

	udev_devnum = udev_device_get_devnum(dev);

	ret = fstat(edata->drm_fd, &s);
	if (ret == -1) {
		TDM_ERR("fstat failed");
		return TDM_ERROR_OPERATION_FAILED;
	}

	hotplug = udev_device_get_property_value(dev, "HOTPLUG");

	if (memcmp(&s.st_rdev, &udev_devnum, sizeof (dev_t)) == 0 &&
	        hotplug && atoi(hotplug) == 1)
	{
		TDM_INFO("HotPlug");
		tdm_drm_display_update_output_status(edata);
	}

	udev_device_unref(dev);

	return TDM_ERROR_NONE;
}

static void
_tdm_drm_udev_init(tdm_drm_data *edata)
{
	struct udev *u = NULL;
	struct udev_monitor *mon = NULL;

	u = udev_new();
	if (!u) {
		TDM_ERR("couldn't create udev");
		goto failed;
	}

	mon = udev_monitor_new_from_netlink(u, "udev");
	if (!mon) {
		TDM_ERR("couldn't create udev monitor");
		goto failed;
	}

	if (udev_monitor_filter_add_match_subsystem_devtype(mon, "drm", "drm_minor") > 0 ||
	    udev_monitor_enable_receiving(mon) < 0) {
		TDM_ERR("add match subsystem failed");
		goto failed;
	}

	edata->uevent_source =
		tdm_event_loop_add_fd_handler(edata->dpy, udev_monitor_get_fd(mon),
		                              TDM_EVENT_LOOP_READABLE,
		                              _tdm_drm_udev_fd_handler,
		                              edata, NULL);
	if (!edata->uevent_source) {
		TDM_ERR("couldn't create udev event source");
		goto failed;
	}

	edata->uevent_monitor = mon;

	TDM_INFO("hotplug monitor created");

	return;
failed:
	if (mon)
		udev_monitor_unref(mon);
	if (u)
		udev_unref(u);
}

static void
_tdm_drm_udev_deinit(tdm_drm_data *edata)
{
	if (edata->uevent_source) {
		tdm_event_loop_source_remove(edata->uevent_source);
		edata->uevent_source = NULL;
	}

	if (edata->uevent_monitor) {
		struct udev *u = udev_monitor_get_udev(edata->uevent_monitor);
		udev_monitor_unref(edata->uevent_monitor);
		udev_unref(u);
		edata->uevent_monitor = NULL;
		TDM_INFO("hotplug monitor destroyed");
	}
}
#endif

static int
_tdm_drm_open_drm(void)
{
	int fd = -1;

	fd = drmOpen(TDM_DRM_NAME, NULL);
	if (fd < 0) {
		TDM_WRN("Cannot open '%s' drm", TDM_DRM_NAME);
	}

#ifdef HAVE_UDEV
	if (fd < 0) {
		struct udev_device *drm_device = NULL;
		const char *filename;
		TDM_WRN("Cannot open drm device.. search by udev");

		drm_device = _tdm_find_primary_gpu();
		if (drm_device == NULL) {
			TDM_ERR("fail to find drm device\n");
			goto close_l;
		}

		filename = udev_device_get_devnode(drm_device);

		fd = open(filename, O_RDWR | O_CLOEXEC);
		if (fd < 0)
			TDM_ERR("Cannot open drm device(%s)\n", filename);

		TDM_DBG("open drm device (name:%s, fd:%d)", filename, fd);

		udev_device_unref(drm_device);
	}
close_l:
#endif
	return fd;
}

void
tdm_drm_deinit(tdm_backend_data *bdata)
{
	if (drm_data != bdata)
		return;

	TDM_INFO("deinit");

#ifdef HAVE_UDEV
	_tdm_drm_udev_deinit(drm_data);
#endif

	tdm_drm_display_destroy_output_list(drm_data);

	if (drm_data->plane_res)
		drmModeFreePlaneResources(drm_data->plane_res);
	if (drm_data->mode_res)
		drmModeFreeResources(drm_data->mode_res);
	if (drm_data->drm_fd >= 0)
		close(drm_data->drm_fd);

	free(drm_data);
	drm_data = NULL;
}

tdm_backend_data *
tdm_drm_init(tdm_display *dpy, tdm_error *error)
{
	tdm_func_display drm_func_display;
	tdm_func_output drm_func_output;
	tdm_func_layer drm_func_layer;
#ifdef ENABLE_PP
	tdm_func_pp drm_func_pp;
#endif
	tdm_error ret;

	if (!dpy) {
		TDM_ERR("display is null");
		if (error)
			*error = TDM_ERROR_INVALID_PARAMETER;
		return NULL;
	}

	if (drm_data) {
		TDM_ERR("failed: init twice");
		if (error)
			*error = TDM_ERROR_BAD_REQUEST;
		return NULL;
	}

	drm_data = calloc(1, sizeof(tdm_drm_data));
	if (!drm_data) {
		TDM_ERR("alloc failed");
		if (error)
			*error = TDM_ERROR_OUT_OF_MEMORY;
		return NULL;
	}

	LIST_INITHEAD(&drm_data->output_list);
	LIST_INITHEAD(&drm_data->buffer_list);

	memset(&drm_func_display, 0, sizeof(drm_func_display));
	drm_func_display.display_get_capabilitiy = drm_display_get_capabilitiy;
	drm_func_display.display_get_pp_capability = drm_display_get_pp_capability;
	drm_func_display.display_get_outputs = drm_display_get_outputs;
	drm_func_display.display_get_fd = drm_display_get_fd;
	drm_func_display.display_handle_events = drm_display_handle_events;
	drm_func_display.display_create_pp = drm_display_create_pp;

	memset(&drm_func_output, 0, sizeof(drm_func_output));
	drm_func_output.output_get_capability = drm_output_get_capability;
	drm_func_output.output_get_layers = drm_output_get_layers;
	drm_func_output.output_set_property = drm_output_set_property;
	drm_func_output.output_get_property = drm_output_get_property;
	drm_func_output.output_wait_vblank = drm_output_wait_vblank;
	drm_func_output.output_set_vblank_handler = drm_output_set_vblank_handler;
	drm_func_output.output_commit = drm_output_commit;
	drm_func_output.output_set_commit_handler = drm_output_set_commit_handler;
	drm_func_output.output_set_dpms = drm_output_set_dpms;
	drm_func_output.output_get_dpms = drm_output_get_dpms;
	drm_func_output.output_set_mode = drm_output_set_mode;
	drm_func_output.output_get_mode = drm_output_get_mode;
#ifdef HAVE_UDEV
	drm_func_output.output_set_status_handler = drm_output_set_status_handler;
#endif

	memset(&drm_func_layer, 0, sizeof(drm_func_layer));
	drm_func_layer.layer_get_capability = drm_layer_get_capability;
	drm_func_layer.layer_set_property = drm_layer_set_property;
	drm_func_layer.layer_get_property = drm_layer_get_property;
	drm_func_layer.layer_set_info = drm_layer_set_info;
	drm_func_layer.layer_get_info = drm_layer_get_info;
	drm_func_layer.layer_set_buffer = drm_layer_set_buffer;
	drm_func_layer.layer_unset_buffer = drm_layer_unset_buffer;

#ifdef ENABLE_PP
	memset(&drm_func_pp, 0, sizeof(drm_func_pp));
	drm_func_pp.pp_destroy = drm_pp_destroy;
	drm_func_pp.pp_set_info = drm_pp_set_info;
	drm_func_pp.pp_attach = drm_pp_attach;
	drm_func_pp.pp_commit = drm_pp_commit;
	drm_func_pp.pp_set_done_handler = drm_pp_set_done_handler;
#endif

	ret = tdm_backend_register_func_display(dpy, &drm_func_display);
	if (ret != TDM_ERROR_NONE)
		goto failed;

	ret = tdm_backend_register_func_output(dpy, &drm_func_output);
	if (ret != TDM_ERROR_NONE)
		goto failed;

	ret = tdm_backend_register_func_layer(dpy, &drm_func_layer);
	if (ret != TDM_ERROR_NONE)
		goto failed;

#ifdef ENABLE_PP
	ret = tdm_backend_register_func_pp(dpy, &drm_func_pp);
	if (ret != TDM_ERROR_NONE)
		goto failed;
#endif

	drm_data->dpy = dpy;

	/* The drm master fd can be opened by a tbm backend module in
	 * tbm_bufmgr_init() time. In this case, we just get it from
	 * TBM_DRM_MASTER_FD enviroment.
	 * 
	 */
	drm_data->drm_fd = tdm_helper_get_fd("TBM_DRM_MASTER_FD");
	if (drm_data->drm_fd < 0)
		drm_data->drm_fd = _tdm_drm_open_drm();

	if (drm_data->drm_fd < 0) {
		ret = TDM_ERROR_OPERATION_FAILED;
		goto failed;
	}

	/* To share the drm master fd with other modules in display server side. */
	tdm_helper_set_fd("TDM_DRM_MASTER_FD", drm_data->drm_fd);

#ifdef HAVE_UDEV
	_tdm_drm_udev_init(drm_data);
#endif

#if LIBDRM_MAJOR_VERSION >= 2 && LIBDRM_MINOR_VERSION >= 4  && LIBDRM_MICRO_VERSION >= 47
	if (drmSetClientCap(drm_data->drm_fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1) < 0) {
		TDM_WRN("Set DRM_CLIENT_CAP_UNIVERSAL_PLANES failed");
	} else {
		TDM_INFO("has universal planes");
		drm_data->has_universal_plane = 1;
	}
#endif

	drm_data->mode_res = drmModeGetResources(drm_data->drm_fd);
	if (!drm_data->mode_res) {
		TDM_ERR("no drm resource: %m");
		ret = TDM_ERROR_OPERATION_FAILED;
		goto failed;
	}

	drm_data->plane_res = drmModeGetPlaneResources(drm_data->drm_fd);
	if (!drm_data->plane_res) {
		TDM_ERR("no drm plane resource: %m");
		ret = TDM_ERROR_OPERATION_FAILED;
		goto failed;
	}

	if (drm_data->plane_res->count_planes <= 0) {
		TDM_ERR("no drm plane resource");
		ret = TDM_ERROR_OPERATION_FAILED;
		goto failed;
	}

	ret = tdm_drm_display_create_output_list(drm_data);
	if (ret != TDM_ERROR_NONE)
		goto failed;

	ret = tdm_drm_display_create_layer_list(drm_data);
	if (ret != TDM_ERROR_NONE)
		goto failed;

	if (error)
		*error = TDM_ERROR_NONE;

	TDM_INFO("init success!");

	return (tdm_backend_data *)drm_data;
failed:
	if (error)
		*error = ret;

	tdm_drm_deinit(drm_data);

	TDM_ERR("init failed!");
	return NULL;
}

tdm_backend_module tdm_backend_module_data = {
	"drm",
	"Samsung",
	TDM_BACKEND_ABI_VERSION,
	tdm_drm_init,
	tdm_drm_deinit
};
