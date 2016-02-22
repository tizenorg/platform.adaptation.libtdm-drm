#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#if HAVE_UDEV
#include <libudev.h>
#endif

#include "tdm_drm.h"
#include <tdm_helper.h>

#define TDM_DRM_NAME "vigs"

static tdm_func_display drm_func_display =
{
    drm_display_get_capabilitiy,
    NULL,  //display_get_pp_capability,
    NULL,  //display_get_capture_capability
    drm_display_get_outputs,
    drm_display_get_fd,
    drm_display_get_fd,
    drm_display_handle_events,
    NULL,  //display_create_pp,
};

static tdm_func_output drm_func_output =
{
    drm_output_get_capability,
    drm_output_get_layers,
    drm_output_set_property,
    drm_output_get_property,
    drm_output_wait_vblank,
    drm_output_set_vblank_handler,
    drm_output_commit,
    drm_output_set_commit_handler,
    drm_output_set_dpms,
    drm_output_get_dpms,
    drm_output_set_mode,
    drm_output_get_mode,
    NULL,   //output_create_capture
};

static tdm_func_layer drm_func_layer =
{
    drm_layer_get_capability,
    drm_layer_set_property,
    drm_layer_get_property,
    drm_layer_set_info,
    drm_layer_get_info,
    drm_layer_set_buffer,
    drm_layer_unset_buffer,
    NULL,    //layer_set_video_pos
    NULL,    //layer_create_capture
};

static tdm_func_pp drm_func_pp =
{
    drm_pp_destroy,
    drm_pp_set_info,
    drm_pp_attach,
    drm_pp_commit,
    drm_pp_set_done_handler,
};

static tdm_drm_data *drm_data;

#ifdef HAVE_UDEV
static struct udev_device*
_tdm_find_primary_gpu(void)
{
    struct udev *udev;
	struct udev_enumerate *e;
	struct udev_list_entry *entry;
	const char *path, *id;
	struct udev_device *device, *drm_device, *pci;

    udev = udev_new();
    if (!udev)
    {
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
#endif

static int
_tdm_drm_open_drm(void)
{
    int fd = -1;

    fd = drmOpen(TDM_DRM_NAME, NULL);
    if (fd < 0)
    {
        TDM_ERR("Cannot open '%s' drm", TDM_DRM_NAME);
    }

#ifdef HAVE_UDEV
    if (fd < 0)
    {
        struct udev_device *drm_device = NULL;
        const char *filename;
        TDM_WRN("Cannot open drm device.. search by udev");

        drm_device = _tdm_find_primary_gpu();
        if (drm_device == NULL)
        {
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

tdm_backend_data*
tdm_drm_init(tdm_display *dpy, tdm_error *error)
{
    tdm_error ret;

    if (!dpy)
    {
        TDM_ERR("display is null");
        if (error)
            *error = TDM_ERROR_INVALID_PARAMETER;
        return NULL;
    }

    if (drm_data)
    {
        TDM_ERR("failed: init twice");
        if (error)
            *error = TDM_ERROR_BAD_REQUEST;
        return NULL;
    }

    drm_data = calloc(1, sizeof(tdm_drm_data));
    if (!drm_data)
    {
        TDM_ERR("alloc failed");
        if (error)
            *error = TDM_ERROR_OUT_OF_MEMORY;
        return NULL;
    }

    LIST_INITHEAD(&drm_data->output_list);
    LIST_INITHEAD(&drm_data->buffer_list);

    ret = tdm_backend_register_func_display(dpy, &drm_func_display);
    if (ret != TDM_ERROR_NONE)
        goto failed;

    ret = tdm_backend_register_func_output(dpy, &drm_func_output);
    if (ret != TDM_ERROR_NONE)
        goto failed;

    ret = tdm_backend_register_func_layer(dpy, &drm_func_layer);
    if (ret != TDM_ERROR_NONE)
        goto failed;

    ret = tdm_backend_register_func_pp(dpy, &drm_func_pp);
    if (ret != TDM_ERROR_NONE)
        goto failed;

    drm_data->dpy = dpy;

    drm_data->drm_fd = _tdm_drm_open_drm();
    if (drm_data->drm_fd < 0)
    {
        ret = TDM_ERROR_OPERATION_FAILED;
        goto failed;
    }

#if LIBDRM_MAJOR_VERSION >= 2 && LIBDRM_MINOR_VERSION >= 4  && LIBDRM_MICRO_VERSION >= 47
    if (drmSetClientCap(drm_data->drm_fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1) < 0)
        TDM_WRN("Set DRM_CLIENT_CAP_UNIVERSAL_PLANES failed");
#endif

    drm_data->mode_res = drmModeGetResources(drm_data->drm_fd);
    if (!drm_data->mode_res)
    {
        TDM_ERR("no drm resource: %m");
        ret = TDM_ERROR_OPERATION_FAILED;
        goto failed;
    }

    drm_data->plane_res = drmModeGetPlaneResources(drm_data->drm_fd);
    if (!drm_data->plane_res)
    {
        TDM_ERR("no drm plane resource: %m");
        ret = TDM_ERROR_OPERATION_FAILED;
        goto failed;
    }

    if (drm_data->plane_res->count_planes <= 0)
    {
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

    return (tdm_backend_data*)drm_data;
failed:
    if (error)
        *error = ret;

    tdm_drm_deinit(drm_data);

    TDM_ERR("init failed!");
    return NULL;
}

tdm_backend_module tdm_backend_module_data =
{
    "vigs",
    "Samsung",
    TDM_BACKEND_ABI_VERSION,
    tdm_drm_init,
    tdm_drm_deinit
};
