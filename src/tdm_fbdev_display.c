#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <tdm_helper.h>
#include <stdint.h>
#include "tdm_fbdev.h"

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
    size = fbdev_data->vinfo.xres * fbdev_data->vinfo.yres * fbdev_data->vinfo.bits_per_pixel / 8 * MAX_BUF;

    output->vaddr = mmap(0, size, PROT_READ|PROT_WRITE,
            MAP_SHARED, fbdev_data->fbdev_fd, 0);
    if (output->vaddr == MAP_FAILED)
    {
        TDM_ERR("MMap framebuffer failed, errno=%d", errno);
        return TDM_ERROR_OPERATION_FAILED;
    }

    memset(output->vaddr, 0, size);

    output->width = fbdev_data->vinfo.xres;
    output->height = fbdev_data->vinfo.yres;
    output->pitch = fbdev_data->vinfo.width;
    output->bpp = fbdev_data->vinfo.bits_per_pixel;
    output->size = size;
    output->max_width  = fbdev_data->vinfo.xres_virtual;
    output->max_height = fbdev_data->vinfo.yres_virtual;

    output->status = TDM_OUTPUT_CONN_STATUS_CONNECTED;
    output->connector_type = TDM_OUTPUT_TYPE_LVDS;

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
        output->output_modes[i].width = fbdev_data->vinfo.xres;
        output->output_modes[i].height = fbdev_data->vinfo.yres;
        output->output_modes[i].refresh = 60;
        output->output_modes[i].flags = -1;
        output->output_modes[i].type = -1;

        sprintf(output->output_modes[i].name, "%dx%d",
                fbdev_data->vinfo.width,
                fbdev_data->vinfo.height);
    }

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

    if (fbdev_output->vaddr == NULL)
        goto close_2;

    munmap(fbdev_output->vaddr, fbdev_output->size);

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
    tdm_fbdev_layer_data *layer = NULL;

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

    layer->fbdev_data = fbdev_data;
    layer->fbdev_output = fbdev_data->fbdev_output;

    return TDM_ERROR_NONE;
}

void
tdm_fbdev_destroy_layer(tdm_fbdev_data *fbdev_data)
{

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

tdm_error
fbdev_display_get_fd(tdm_backend_data *bdata, int *fd)
{
    RETURN_VAL_IF_FAIL(bdata, TDM_ERROR_INVALID_PARAMETER);
    RETURN_VAL_IF_FAIL(fd, TDM_ERROR_INVALID_PARAMETER);

    /*
     * TODO: Shloud we implement this call, because it is tricky place since we don't know how drm
     *  file descriptor is used by software above in drm backend;
     *
     */
    *fd = -1;

    return TDM_ERROR_NONE;
}

tdm_error
fbdev_display_handle_events(tdm_backend_data *bdata)
{
    /*
     * Framebuffer does not produce events
     */
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

    caps->mmWidth = fbdev_data->vinfo.width;;
    caps->mmHeight = fbdev_data->vinfo.height;
    caps->subpixel = -1;

    caps->min_w = fbdev_output->width;
    caps->min_h = fbdev_output->height;
    caps->max_w = fbdev_output->max_width;
    caps->max_h = fbdev_output->max_height;
    caps->preferred_align = -1;

    /*
     * Framebuffer does not have properties
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
    tdm_fbdev_layer_data *fbdev_layer = NULL;
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

    fbdev_layer = fbdev_output->fbdev_layer;

    layers[0] = fbdev_layer;

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
     * Framebuffer does not have properties
     */
    return TDM_ERROR_NONE;
}

tdm_error
fbdev_output_get_property(tdm_output *output, unsigned int id, tdm_value *value)
{
    /*
     * Framebuffer does not have properties
     */
    return TDM_ERROR_NONE;
}

tdm_error
fbdev_output_wait_vblank(tdm_output *output, int interval, int sync, void *user_data)
{
    return TDM_ERROR_NONE;
}

tdm_error
fbdev_output_set_vblank_handler(tdm_output *output, tdm_output_vblank_handler func)
{
    return TDM_ERROR_NONE;
}

tdm_error
fbdev_output_commit(tdm_output *output, int sync, void *user_data)
{
    return TDM_ERROR_NONE;
}

tdm_error
fbdev_output_set_commit_handler(tdm_output *output, tdm_output_commit_handler func)
{
    return TDM_ERROR_NONE;
}

tdm_error
fbdev_output_set_dpms(tdm_output *output, tdm_output_dpms dpms_value)
{
    return TDM_ERROR_NONE;
}

tdm_error
fbdev_output_get_dpms(tdm_output *output, tdm_output_dpms *dpms_value)
{
    return TDM_ERROR_NONE;
}

tdm_error
fbdev_output_set_mode(tdm_output *output, const tdm_output_mode *mode)
{
    return TDM_ERROR_NONE;
}

tdm_error
fbdev_output_get_mode(tdm_output *output, const tdm_output_mode **mode)
{
    return TDM_ERROR_NONE;
}

tdm_error
fbdev_layer_get_capability(tdm_layer *layer, tdm_caps_layer *caps)
{
    return TDM_ERROR_NONE;
}

tdm_error
fbdev_layer_set_property(tdm_layer *layer, unsigned int id, tdm_value value)
{
    return TDM_ERROR_NONE;
}

tdm_error
fbdev_layer_get_property(tdm_layer *layer, unsigned int id, tdm_value *value)
{
    return TDM_ERROR_NONE;
}

tdm_error
fbdev_layer_set_info(tdm_layer *layer, tdm_info_layer *info)
{
    return TDM_ERROR_NONE;
}

tdm_error
fbdev_layer_get_info(tdm_layer *layer, tdm_info_layer *info)
{
    return TDM_ERROR_NONE;
}

tdm_error
fbdev_layer_set_buffer(tdm_layer *layer, tbm_surface_h buffer)
{
    return TDM_ERROR_NONE;
}

tdm_error
fbdev_layer_unset_buffer(tdm_layer *layer)
{
    return TDM_ERROR_NONE;
}
