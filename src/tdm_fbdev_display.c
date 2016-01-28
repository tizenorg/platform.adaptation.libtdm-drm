#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <tdm_helper.h>
#include "tdm_fbdev.h"

tdm_error
fbdev_display_get_capabilitiy(tdm_backend_data *bdata, tdm_caps_display *caps)
{
    return TDM_ERROR_NONE;
}

tdm_output**
fbdev_display_get_outputs(tdm_backend_data *bdata, int *count, tdm_error *error)
{
    return NULL;
}

tdm_error
fbdev_display_get_fd(tdm_backend_data *bdata, int *fd)
{
    return TDM_ERROR_NONE;
}

tdm_error
fbdev_display_handle_events(tdm_backend_data *bdata)
{
    return TDM_ERROR_NONE;
}

tdm_error
fbdev_output_get_capability(tdm_output *output, tdm_caps_output *caps)
{
    return TDM_ERROR_NONE;
}

tdm_layer**
fbdev_output_get_layers(tdm_output *output,  int *count, tdm_error *error)
{
    return NULL;
}

tdm_error
fbdev_output_set_property(tdm_output *output, unsigned int id, tdm_value value)
{
    return TDM_ERROR_NONE;
}

tdm_error
fbdev_output_get_property(tdm_output *output, unsigned int id, tdm_value *value)
{
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
