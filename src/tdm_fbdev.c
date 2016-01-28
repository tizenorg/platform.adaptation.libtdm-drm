#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#if HAVE_UDEV
#include <libudev.h>
#endif

#include "tdm_fbdev.h"

#include <tdm_helper.h>

/*
 * TODO: How should it be named?
 */
#define TDM_FBDEV_NAME "vigs"

static tdm_func_display fbdev_func_display =
{
    fbdev_display_get_capabilitiy,
    NULL,  //display_get_pp_capability,
    NULL,  //display_get_capture_capability
    fbdev_display_get_outputs,
    fbdev_display_get_fd,
    fbdev_display_handle_events,
    NULL,  //display_create_pp,
    fbdev_output_get_capability,
    fbdev_output_get_layers,
    fbdev_output_set_property,
    fbdev_output_get_property,
    fbdev_output_wait_vblank,
    fbdev_output_set_vblank_handler,
    fbdev_output_commit,
    fbdev_output_set_commit_handler,
    fbdev_output_set_dpms,
    fbdev_output_get_dpms,
    fbdev_output_set_mode,
    fbdev_output_get_mode,
    NULL,   //output_create_capture
    fbdev_layer_get_capability,
    fbdev_layer_set_property,
    fbdev_layer_get_property,
    fbdev_layer_set_info,
    fbdev_layer_get_info,
    fbdev_layer_set_buffer,
    fbdev_layer_unset_buffer,
    NULL,    //layer_set_video_pos
    NULL,    //layer_create_capture
};


void
tdm_fbdev_deinit(tdm_backend_data *bdata)
{

}

tdm_backend_data*
tdm_fbdev_init(tdm_display *dpy, tdm_error *error)
{
	(void) fbdev_func_display;

    return NULL;
}

tdm_backend_module tdm_backend_module_data =
{
    "vigs",
    "Samsung",
    TDM_BACKEND_ABI_VERSION,
    tdm_fbdev_init,
    tdm_fbdev_deinit
};
