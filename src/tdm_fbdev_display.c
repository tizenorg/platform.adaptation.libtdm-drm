#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <tdm_helper.h>
#include <stdint.h>
#include "tdm_fbdev.h"

#include <xf86drm.h>
#include <libudev.h>

/*
 * Framebuffer device supported formats
 */
static tbm_format supported_formats[] =
{
/*
    TBM_FORMAT_XRGB8888,
    TBM_FORMAT_XBGR8888,
    TBM_FORMAT_RGBX8888,
    TBM_FORMAT_BGRX8888,
*/
    TBM_FORMAT_ARGB8888,
    TBM_FORMAT_ABGR8888,
    TBM_FORMAT_RGBA8888,
    TBM_FORMAT_BGRA8888,
};

static tdm_fbdev_display_buffer*
_tdm_fbdev_display_find_buffer(tdm_fbdev_data *fbdev_data, tbm_surface_h buffer)
{
    tdm_fbdev_display_buffer *display_buffer = NULL;

    LIST_FOR_EACH_ENTRY(display_buffer, &fbdev_data->buffer_list, link)
    {
        if (display_buffer->buffer == buffer)
            return display_buffer;
    }

    return NULL;
}

static void
_tdm_fbdev_display_cb_destroy_buffer(tbm_surface_h buffer, void *user_data)
{
    tdm_fbdev_data *fbdev_data;
    tdm_fbdev_display_buffer *display_buffer;
    tbm_bo bo;

    if (!user_data)
    {
        TDM_ERR("no user_data");
        return;
    }
    if (!buffer)
    {
        TDM_ERR("no buffer");
        return;
    }

    fbdev_data = (tdm_fbdev_data *)user_data;

    display_buffer = _tdm_fbdev_display_find_buffer(fbdev_data, buffer);
    if (!display_buffer)
    {
        TDM_ERR("no display_buffer");
        return;
    }

    LIST_DEL(&display_buffer->link);

    if(display_buffer->mem != NULL)
    {
        bo = tbm_surface_internal_get_bo(buffer, 0);

        /*
         * TODO: Check tbm_bo_unmap return status
         */
        tbm_bo_unmap(bo);

        TDM_DBG("unmap success");
    }
    else
        TDM_DBG("unmap was not called");

    free(display_buffer);
}

/*
static int
_tdm_fbdev_layer_is_supproted_format()
{

}
*/

tdm_error
tdm_fbdev_creat_output(tdm_fbdev_data *fbdev_data)
{
    tdm_fbdev_output_data *output = NULL;
    size_t size;
    int i = 0;

    output = calloc(1, sizeof(tdm_fbdev_output_data));
    if (output == NULL)
    {
        TDM_ERR("alloc output failed");
        return TDM_ERROR_OUT_OF_MEMORY;
    }

    /*
     * TODO: Size of framebuffer must be aligned to system page size before
     *  it is mapped
     */
    size = fbdev_data->vinfo->xres * fbdev_data->vinfo->yres * fbdev_data->vinfo->bits_per_pixel / 8 * MAX_BUF;

    TDM_INFO("\n"
             "MMaped size: %zu\n",
             size);

    output->mem = mmap(0, size, PROT_READ|PROT_WRITE,
            MAP_SHARED, fbdev_data->fbdev_fd, 0);
    if (output->mem == MAP_FAILED)
    {
        TDM_ERR("MMap framebuffer failed, errno=%d", errno);
        return TDM_ERROR_OPERATION_FAILED;
    }

    memset(output->mem, 0, size);

    output->width = fbdev_data->vinfo->xres;
    output->height = fbdev_data->vinfo->yres;
    output->pitch = fbdev_data->vinfo->width;
    output->bpp = fbdev_data->vinfo->bits_per_pixel;
    output->size = size;
    output->max_width  = fbdev_data->vinfo->xres_virtual;
    output->max_height = fbdev_data->vinfo->yres_virtual;

    output->status = TDM_OUTPUT_CONN_STATUS_CONNECTED;
    output->connector_type = TDM_OUTPUT_TYPE_LVDS;

    output->is_vblank = DOWN;
    output->is_commit = DOWN;

    /*
     * TODO: connector_type_id field relates to libdrm connector which framebuffer
     *  does not know. It have to be checked whether softaware above us use this
     *  field for its purposes.
     */
    output->connector_type_id = 1;

    /*
     * TODO: Check does Framebuffer support multiple modes and
     *  switching between them?
     */
    output->count_modes = 1;
    output->output_modes = calloc(output->count_modes, sizeof(tdm_output_mode));
    if (!output->output_modes)
    {
        TDM_ERR("failed: alloc memory");
        return TDM_ERROR_OUT_OF_MEMORY;
    }

    for(i = 0; i < output->count_modes ; i++)
    {
        output->output_modes[i].width = fbdev_data->vinfo->xres;
        output->output_modes[i].height = fbdev_data->vinfo->yres;
        output->output_modes[i].refresh = 60;
        output->output_modes[i].flags = -1;
        output->output_modes[i].type = -1;

        sprintf(output->output_modes[i].name, "%dx%d",
                fbdev_data->vinfo->xres,
                fbdev_data->vinfo->yres);
    }

    /*
     * We currently support only one mode
     */
    if (output->count_modes == 1)
        output->current_mode = &output->output_modes[0];

    output->fbdev_data = fbdev_data;
    fbdev_data->fbdev_output = output;

    return TDM_ERROR_NONE;
}

void
tdm_fbdev_destroy_output(tdm_fbdev_data *fbdev_data)
{
    tdm_fbdev_output_data *fbdev_output = fbdev_data->fbdev_output;

    if (fbdev_output == NULL)
        goto close;

    if (fbdev_output->mem == NULL)
        goto close_2;

    munmap(fbdev_output->mem, fbdev_output->size);

    if (fbdev_output->output_modes == NULL)
        goto close_2;

    free(fbdev_output->output_modes);

close_2:
    free(fbdev_output);
close:
    return;
}

tdm_error
tdm_fbdev_creat_layer(tdm_fbdev_data *fbdev_data)
{
    tdm_fbdev_layer_data *layer;
    tdm_fbdev_output_data *fbdev_output = fbdev_data->fbdev_output;

    /*
     * Framebuffer does not support layer, therefore create only
     *  one layer by libtdm's demand;
     */
    layer = calloc(1, sizeof(tdm_fbdev_layer_data));
    if (layer == NULL)
    {
        TDM_ERR("alloc output failed");
        return TDM_ERROR_OUT_OF_MEMORY;
    }

    layer->capabilities = TDM_LAYER_CAPABILITY_PRIMARY | TDM_LAYER_CAPABILITY_GRAPHIC;

    layer->fbdev_data = fbdev_data;
    layer->fbdev_output = fbdev_output;
    fbdev_output->fbdev_layer = layer;

    return TDM_ERROR_NONE;
}

void
tdm_fbdev_destroy_layer(tdm_fbdev_data *fbdev_data)
{
    tdm_fbdev_output_data *fbdev_output = fbdev_data->fbdev_output;
    tdm_fbdev_layer_data *layer = fbdev_output->fbdev_layer;

    if(layer != NULL)
        free(layer);
}

tdm_error
fbdev_display_get_capabilitiy(tdm_backend_data *bdata, tdm_caps_display *caps)
{
    RETURN_VAL_IF_FAIL(caps, TDM_ERROR_INVALID_PARAMETER);

    /*
     * Framebuffer does not support layer, therefore create only
     *  one layer by libtdm's demand;
     */
    caps->max_layer_count = 1;

    return TDM_ERROR_NONE;
}

tdm_output**
fbdev_display_get_outputs(tdm_backend_data *bdata, int *count, tdm_error *error)
{
    tdm_fbdev_data *fbdev_data = bdata;
    tdm_fbdev_output_data *fbdev_output = fbdev_data->fbdev_output;
    tdm_output **outputs;
    tdm_error ret;

    RETURN_VAL_IF_FAIL(fbdev_data, NULL);
    RETURN_VAL_IF_FAIL(count, NULL);

    if (fbdev_output == NULL)
    {
        ret = TDM_ERROR_NONE;
        goto failed_get;
    }

    /*
     * Since it is Framebuffer device there is only one output
     */
    *count = 1;

    /* will be freed in frontend */
    outputs = calloc(*count, sizeof(tdm_fbdev_output_data*));
    if (!outputs)
    {
        TDM_ERR("failed: alloc memory");
        *count = 0;
        ret = TDM_ERROR_OUT_OF_MEMORY;
        goto failed_get;
    }

    outputs[0] = fbdev_output;

    if (error)
        *error = TDM_ERROR_NONE;

    return outputs;

failed_get:
    if (error)
        *error = ret;
    return NULL;
}

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

tdm_error
fbdev_display_get_fd(tdm_backend_data *bdata, int *fd)
{
    tdm_fbdev_data *fbdev_data = (tdm_fbdev_data *)bdata;

    RETURN_VAL_IF_FAIL(fbdev_data, TDM_ERROR_INVALID_PARAMETER);
    RETURN_VAL_IF_FAIL(fd, TDM_ERROR_INVALID_PARAMETER);

    int drm_fd = -1;

    /*
     * TODO: Temp code. Open drm device for novice test utility, It needs
     *  it to open and using tbm buffer manger
     */
    drm_fd = drmOpen("/dev/card0", NULL);
    if (drm_fd < 0)
    {
        TDM_ERR("Cannot open '%s' drm", "/dev/card0");
    }

    if (drm_fd < 0)
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

         drm_fd = open(filename, O_RDWR | O_CLOEXEC);
         if (drm_fd < 0)
             TDM_ERR("Cannot open drm device(%s)\n", filename);

         TDM_DBG("open drm device (name:%s, fd:%d)", filename, fd);

         udev_device_unref(drm_device);
     }

    *fd = drm_fd;

close_l:
    return TDM_ERROR_NONE;
}

tdm_error
fbdev_display_handle_events(tdm_backend_data *bdata)
{
    tdm_fbdev_data *fbdev_data = (tdm_fbdev_data *) bdata;
    void *user_data;
    tdm_fbdev_output_data *fbdev_output;
    unsigned int sequence = 0;
    unsigned int tv_sec = 0;
    unsigned int tv_usec = 0;

    RETURN_VAL_IF_FAIL(fbdev_data, TDM_ERROR_INVALID_PARAMETER);

    fbdev_output = fbdev_data->fbdev_output;
    user_data = fbdev_output->user_data;

    RETURN_VAL_IF_FAIL(user_data, TDM_ERROR_INVALID_PARAMETER);

    /*
    (tdm_output *output, unsigned int sequence,
     unsigned int tv_sec, unsigned int tv_usec,
     void *user_data);
    */

    /*
     * Framebuffer does not produce events
     * Fake flags are used to simulate event-operated back end. Since tdm
     *  library assumes its back ends to be event-operated and Framebuffer
     *  device is not event-operated we have to make fake events
     */
    if (fbdev_output->is_vblank && fbdev_output->vblank_func)
    {
        fbdev_output->is_vblank = DOWN;

        fbdev_output->vblank_func((tdm_output *)fbdev_data,
                                   sequence, tv_sec,
                                   tv_usec, user_data);
    }

    if (fbdev_output->is_commit && fbdev_output->commit_func)
    {
        fbdev_output->is_commit = DOWN;

        fbdev_output->commit_func((tdm_output *)fbdev_data,
                                   sequence, tv_sec,
                                   tv_usec, user_data);
    }

    return TDM_ERROR_NONE;
}

tdm_error
fbdev_output_get_capability(tdm_output *output, tdm_caps_output *caps)
{
    tdm_fbdev_output_data *fbdev_output = (tdm_fbdev_output_data *)output;
    tdm_fbdev_data *fbdev_data = NULL;
    tdm_error ret;
    int i = 0;

    RETURN_VAL_IF_FAIL(fbdev_output, TDM_ERROR_INVALID_PARAMETER);
    RETURN_VAL_IF_FAIL(caps, TDM_ERROR_INVALID_PARAMETER);

    memset(caps, 0, sizeof(tdm_caps_output));

    fbdev_data = fbdev_output->fbdev_data;

    caps->status = fbdev_output->status;
    caps->type = fbdev_output->connector_type;
    caps->type_id = fbdev_output->connector_type_id;

    caps->mode_count = fbdev_output->count_modes;
    caps->modes = calloc(caps->mode_count, sizeof(tdm_output_mode));
    if (!caps->modes)
    {
        ret = TDM_ERROR_OUT_OF_MEMORY;
        TDM_ERR("alloc failed\n");
        goto failed_get;
    }

    for (i = 0; i < caps->mode_count; i++)
        caps->modes[i] = fbdev_output->output_modes[i];

    caps->mmWidth = fbdev_data->vinfo->width;;
    caps->mmHeight = fbdev_data->vinfo->height;
    caps->subpixel = -1;

    caps->min_w = fbdev_output->width;
    caps->min_h = fbdev_output->height;
    caps->max_w = fbdev_output->max_width;
    caps->max_h = fbdev_output->max_height;
    caps->preferred_align = -1;

    /*
     * Framebuffer does not have output properties
     */
    caps->prop_count = 0;
    caps->props = NULL;

    return TDM_ERROR_NONE;

failed_get:
    memset(caps, 0, sizeof(tdm_caps_output));
    return ret;
}

tdm_layer**
fbdev_output_get_layers(tdm_output *output,  int *count, tdm_error *error)
{
    tdm_fbdev_output_data *fbdev_output= (tdm_fbdev_output_data *)output;
    tdm_layer **layers;
    tdm_error ret;

    RETURN_VAL_IF_FAIL(fbdev_output, NULL);
    RETURN_VAL_IF_FAIL(count, NULL);

    /*
     * Framebuffer does not support layer, therefore create only
     *  one layer by libtdm's demand;
     */
    *count = 1;

    layers = calloc(*count, sizeof(tdm_layer));
    if (layers == NULL)
    {
        TDM_ERR("failed: alloc memory");
        *count = 0;
        ret = TDM_ERROR_OUT_OF_MEMORY;
        goto failed_get;
    }

    layers[0] = fbdev_output->fbdev_layer;;

    if (error)
        *error = TDM_ERROR_NONE;

    return layers;

failed_get:
    if (error)
        *error = ret;

    return NULL;
}

tdm_error
fbdev_output_set_property(tdm_output *output, unsigned int id, tdm_value value)
{
    /*
     * Framebuffer does not have output properties
     */
    return TDM_ERROR_NONE;
}

tdm_error
fbdev_output_get_property(tdm_output *output, unsigned int id, tdm_value *value)
{
    /*
     * Framebuffer does not have output properties
     */
    return TDM_ERROR_NONE;
}

tdm_error
fbdev_output_wait_vblank(tdm_output *output, int interval, int sync, void *user_data)
{
    tdm_fbdev_output_data *fbdev_output = (tdm_fbdev_output_data *)output;
    tdm_fbdev_data *fbdev_data;

    int crtc = 0;
    int ret;

    RETURN_VAL_IF_FAIL(fbdev_output, TDM_ERROR_INVALID_PARAMETER);

    /*
     * Frame buffer device does not support asynchronous events, that is
     *  why clients must get responsibility on handling asynchronous
     *  events.
     */
    if (!sync)
    {
        TDM_WRN("Framebuffer back end does not support asynchronous vblank");
        return TDM_ERROR_NONE;
    }

    fbdev_data = fbdev_output->fbdev_data;

    ret = ioctl(fbdev_data->fbdev_fd, FBIO_WAITFORVSYNC, &crtc);
    if (ret < 0)
    {
        TDM_ERR("FBIO_WAITFORVSYNC ioctl failed, errno=%d", errno);
        return TDM_ERROR_OPERATION_FAILED;
    }

    /*
     * Up fake flag to simulate vsync event
     */
    fbdev_output->is_vblank = UP;
    fbdev_output->user_data = user_data;

    return TDM_ERROR_NONE;
}

tdm_error
fbdev_output_set_vblank_handler(tdm_output *output, tdm_output_vblank_handler func)
{
    tdm_fbdev_output_data *fbdev_output = (tdm_fbdev_output_data *)output;

    RETURN_VAL_IF_FAIL(fbdev_output, TDM_ERROR_INVALID_PARAMETER);
    RETURN_VAL_IF_FAIL(func, TDM_ERROR_INVALID_PARAMETER);

    fbdev_output->vblank_func = func;

    return TDM_ERROR_NONE;
}

tdm_error
fbdev_output_commit(tdm_output *output, int sync, void *user_data)
{
    tdm_fbdev_output_data *fbdev_output = (tdm_fbdev_output_data *)output;
    tdm_fbdev_layer_data *fbdev_layer;
    tdm_fbdev_display_buffer *display_buffer;

    RETURN_VAL_IF_FAIL(fbdev_output, TDM_ERROR_INVALID_PARAMETER);

    fbdev_layer = fbdev_output->fbdev_layer;

    if (!fbdev_layer->display_buffer_changed)
        return TDM_ERROR_NONE;

    fbdev_output->mode_changed = 0;
    fbdev_layer->display_buffer_changed = 0;
    fbdev_layer->info_changed = 0;

    display_buffer = fbdev_layer->display_buffer;

    /*
     * Display buffer's content to screen
     */
    memcpy(fbdev_output->mem, display_buffer->mem, display_buffer->size * sizeof(char) );


    /*
     * Up fake flag to simulate page flip event
     */
    fbdev_output->is_commit = UP;
    fbdev_output->user_data = user_data;

    return TDM_ERROR_NONE;
}

tdm_error
fbdev_output_set_commit_handler(tdm_output *output, tdm_output_commit_handler func)
{
    tdm_fbdev_output_data *fbdev_output = (tdm_fbdev_output_data *)output;

    RETURN_VAL_IF_FAIL(fbdev_output, TDM_ERROR_INVALID_PARAMETER);
    RETURN_VAL_IF_FAIL(func, TDM_ERROR_INVALID_PARAMETER);

    fbdev_output->commit_func = func;

    return TDM_ERROR_NONE;
}

tdm_error
fbdev_output_set_dpms(tdm_output *output, tdm_output_dpms dpms_value)
{
    tdm_fbdev_output_data *fbdev_output = (tdm_fbdev_output_data *)output;
    tdm_fbdev_data *fbdev_data;
    int fbmode = 0;
    int ret;

    RETURN_VAL_IF_FAIL(fbdev_output, TDM_ERROR_INVALID_PARAMETER);

    /*
     * Bypass context switching overhead
     */
    if (fbdev_output->dpms_value == dpms_value)
        return TDM_ERROR_NONE;

    fbdev_data = fbdev_output->fbdev_data;

    switch (dpms_value)
    {
        case TDM_OUTPUT_DPMS_ON:
            fbmode = FB_BLANK_UNBLANK;
            break;

        case TDM_OUTPUT_DPMS_OFF:
            fbmode = FB_BLANK_POWERDOWN;
            break;

        case TDM_OUTPUT_DPMS_STANDBY:
            fbmode = FB_BLANK_VSYNC_SUSPEND;
            break;

        case TDM_OUTPUT_DPMS_SUSPEND:
            fbmode = FB_BLANK_HSYNC_SUSPEND;
            break;

        default:
            NEVER_GET_HERE();
            break;
    }

    ret = ioctl(fbdev_data->fbdev_fd, FBIOBLANK, (void *)fbmode);
    if (ret < 0)
    {
        TDM_ERR("FBIOBLANK ioctl failed, errno=%d", errno);
        return TDM_ERROR_OPERATION_FAILED;
    }

    fbdev_output->dpms_value = dpms_value;
    fbmode = 0;

    return TDM_ERROR_NONE;
}

tdm_error
fbdev_output_get_dpms(tdm_output *output, tdm_output_dpms *dpms_value)
{
    tdm_fbdev_output_data *fbdev_output = (tdm_fbdev_output_data *)output;

    RETURN_VAL_IF_FAIL(fbdev_output, TDM_ERROR_INVALID_PARAMETER);
    RETURN_VAL_IF_FAIL(dpms_value, TDM_ERROR_INVALID_PARAMETER);

    *dpms_value = fbdev_output->dpms_value;

    return TDM_ERROR_NONE;
}

tdm_error
fbdev_output_set_mode(tdm_output *output, const tdm_output_mode *mode)
{
    tdm_fbdev_output_data *fbdev_output = (tdm_fbdev_output_data *)output;

    RETURN_VAL_IF_FAIL(fbdev_output, TDM_ERROR_INVALID_PARAMETER);
    RETURN_VAL_IF_FAIL(mode, TDM_ERROR_INVALID_PARAMETER);

    /*
     * We currently support only one mode
     */
    if (fbdev_output->count_modes == 1)
        return TDM_ERROR_NONE;

    /*
     * TODO: Implement mode switching
     */
    fbdev_output->mode_changed = 0;

    return TDM_ERROR_NONE;
}

tdm_error
fbdev_output_get_mode(tdm_output *output, const tdm_output_mode **mode)
{
    tdm_fbdev_output_data *fbdev_output = (tdm_fbdev_output_data *)output;

    RETURN_VAL_IF_FAIL(fbdev_output, TDM_ERROR_INVALID_PARAMETER);
    RETURN_VAL_IF_FAIL(*mode, TDM_ERROR_INVALID_PARAMETER);

    *mode = fbdev_output->current_mode;

    return TDM_ERROR_NONE;
}

tdm_error
fbdev_layer_get_capability(tdm_layer *layer, tdm_caps_layer *caps)
{
    tdm_fbdev_layer_data *fbdev_layer = (tdm_fbdev_layer_data *) layer;
    tdm_error ret;
    int i;

    RETURN_VAL_IF_FAIL(fbdev_layer, TDM_ERROR_INVALID_PARAMETER);
    RETURN_VAL_IF_FAIL(caps, TDM_ERROR_INVALID_PARAMETER);

    memset(caps, 0, sizeof(tdm_caps_layer));

    caps->capabilities = fbdev_layer->capabilities;

    /*
     * Make our single GRAPHIC layer the lowest one aka "Prime"
     */
    caps->zpos = 0;

    caps->format_count = sizeof(supported_formats)/sizeof(supported_formats[0]);
    caps->formats = calloc(caps->format_count, sizeof(tbm_format));
    if (!caps->formats)
    {
        ret = TDM_ERROR_OUT_OF_MEMORY;
        TDM_ERR("alloc failed\n");
        goto failed_get;
    }

    for( i = 0; i < caps->format_count; i++)
        caps->formats[i] = supported_formats[i];

    /*
     * Framebuffer does not have layer properties
     */
    caps->prop_count = 0;
    caps->props = NULL;

    return TDM_ERROR_NONE;

failed_get:

    memset(caps, 0, sizeof(tdm_caps_layer));
    return ret;
}

tdm_error
fbdev_layer_set_property(tdm_layer *layer, unsigned int id, tdm_value value)
{
   /*
    * Framebuffer does not have layer properties
    */
    return TDM_ERROR_NONE;
}

tdm_error
fbdev_layer_get_property(tdm_layer *layer, unsigned int id, tdm_value *value)
{
    /*
     * Framebuffer does not have layer properties
     */
    return TDM_ERROR_NONE;
}

tdm_error
fbdev_layer_set_info(tdm_layer *layer, tdm_info_layer *info)
{
    tdm_fbdev_layer_data *fbdev_layer = (tdm_fbdev_layer_data *)layer;

    RETURN_VAL_IF_FAIL(fbdev_layer, TDM_ERROR_INVALID_PARAMETER);
    RETURN_VAL_IF_FAIL(info, TDM_ERROR_INVALID_PARAMETER);

    fbdev_layer->info = *info;

    /*
     * We do not use info in this backend, therefore just ignore
     *  info's changing
     */
    fbdev_layer->info_changed = 0;

    return TDM_ERROR_NONE;
}

tdm_error
fbdev_layer_get_info(tdm_layer *layer, tdm_info_layer *info)
{
    tdm_fbdev_layer_data *fbdev_layer = (tdm_fbdev_layer_data *)layer;

    RETURN_VAL_IF_FAIL(fbdev_layer, TDM_ERROR_INVALID_PARAMETER);
    RETURN_VAL_IF_FAIL(info, TDM_ERROR_INVALID_PARAMETER);

    *info = fbdev_layer->info;

    return TDM_ERROR_NONE;
}

tdm_error
fbdev_layer_set_buffer(tdm_layer *layer, tbm_surface_h buffer)
{
    tdm_fbdev_layer_data *fbdev_layer = (tdm_fbdev_layer_data *)layer;
    tdm_fbdev_display_buffer *display_buffer;
    tdm_fbdev_data *fbdev_data;
    int opt = 0;
    unsigned int format;
    tbm_bo bo;
    tbm_bo_handle bo_handle;
    tdm_error err = TDM_ERROR_NONE;

    RETURN_VAL_IF_FAIL(fbdev_layer, TDM_ERROR_INVALID_PARAMETER);
    RETURN_VAL_IF_FAIL(buffer, TDM_ERROR_INVALID_PARAMETER);

    if (tbm_surface_internal_get_num_bos(buffer) > 1)
    {
        TDM_ERR("Framebuffer backend does not support surfaces with more than one bos");
        return TDM_ERROR_INVALID_PARAMETER;
    }

    fbdev_data = fbdev_layer->fbdev_data;

    display_buffer = _tdm_fbdev_display_find_buffer(fbdev_data, buffer);
    if(!display_buffer)
    {
        display_buffer = calloc(1, sizeof(tdm_fbdev_display_buffer));
        if (display_buffer == NULL)
        {
            TDM_ERR("alloc failed");
            return TDM_ERROR_OUT_OF_MEMORY;
        }

        display_buffer->buffer = buffer;

        err = tdm_buffer_add_destroy_handler(buffer, _tdm_fbdev_display_cb_destroy_buffer, fbdev_data);
        if (err != TDM_ERROR_NONE)
        {
            TDM_ERR("add destroy handler fail");
            free(display_buffer);
            return TDM_ERROR_OPERATION_FAILED;
        }

        LIST_ADDTAIL(&display_buffer->link, &fbdev_data->buffer_list);
    }

    if (display_buffer->mem == NULL)
    {
        display_buffer->width = tbm_surface_get_width(buffer);
        display_buffer->height = tbm_surface_get_height(buffer);

        /*
         * TODO: We have got drm format here, have to be checked whether
         *  Framebuffer device supports this format.
         *  Do we need it at all?
         */
        format = tbm_surface_get_format(buffer);
        (void)format;

        bo = tbm_surface_internal_get_bo(buffer, 0);
        bo_handle = tbm_bo_map(bo, TBM_DEVICE_CPU, opt);

        display_buffer->size = tbm_bo_size(bo);

        /*
         * When surface will be about to be destroyed there will have been
         *  invoked our destroy handler which will unmap bo and free
         *  display_buffer memory
         */
        display_buffer->mem = bo_handle.ptr;

        TDM_DBG("mmap success");
    }

    fbdev_layer->display_buffer = display_buffer;
    fbdev_layer->display_buffer_changed = 1;

    return TDM_ERROR_NONE;
}

tdm_error
fbdev_layer_unset_buffer(tdm_layer *layer)
{
    tdm_fbdev_layer_data *fbdev_layer = (tdm_fbdev_layer_data *)layer;

    RETURN_VAL_IF_FAIL(fbdev_layer, TDM_ERROR_INVALID_PARAMETER);

    fbdev_layer->display_buffer = NULL;
    fbdev_layer->display_buffer_changed = 1;

    return TDM_ERROR_NONE;
}
