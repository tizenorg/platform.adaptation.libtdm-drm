#ifndef _TDM_fbdev_H_
#define _TDM_fbdev_H_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>

#include <linux/fb.h>

#include <tbm_surface.h>
#include <tbm_surface_internal.h>
#include <tdm_backend.h>
#include <tdm_log.h>
#include <tdm_list.h>

#define MAX_BUF 3

/* drm backend functions (display) */
tdm_error    fbdev_display_get_capabilitiy(tdm_backend_data *bdata, tdm_caps_display *caps);
tdm_output** fbdev_display_get_outputs(tdm_backend_data *bdata, int *count, tdm_error *error);
tdm_error    fbdev_display_get_fd(tdm_backend_data *bdata, int *fd);
tdm_error    fbdev_display_handle_events(tdm_backend_data *bdata);
tdm_pp*      fbdev_display_create_pp(tdm_backend_data *bdata, tdm_error *error);
tdm_error    fbdev_output_get_capability(tdm_output *output, tdm_caps_output *caps);
tdm_layer**  fbdev_output_get_layers(tdm_output *output, int *count, tdm_error *error);
tdm_error    fbdev_output_set_property(tdm_output *output, unsigned int id, tdm_value value);
tdm_error    fbdev_output_get_property(tdm_output *output, unsigned int id, tdm_value *value);
tdm_error    fbdev_output_wait_vblank(tdm_output *output, int interval, int sync, void *user_data);
tdm_error    fbdev_output_set_vblank_handler(tdm_output *output, tdm_output_vblank_handler func);
tdm_error    fbdev_output_commit(tdm_output *output, int sync, void *user_data);
tdm_error    fbdev_output_set_commit_handler(tdm_output *output, tdm_output_commit_handler func);
tdm_error    fbdev_output_set_dpms(tdm_output *output, tdm_output_dpms dpms_value);
tdm_error    fbdev_output_get_dpms(tdm_output *output, tdm_output_dpms *dpms_value);
tdm_error    fbdev_output_set_mode(tdm_output *output, const tdm_output_mode *mode);
tdm_error    fbdev_output_get_mode(tdm_output *output, const tdm_output_mode **mode);
tdm_error    fbdev_layer_get_capability(tdm_layer *layer, tdm_caps_layer *caps);
tdm_error    fbdev_layer_set_property(tdm_layer *layer, unsigned int id, tdm_value value);
tdm_error    fbdev_layer_get_property(tdm_layer *layer, unsigned int id, tdm_value *value);
tdm_error    fbdev_layer_set_info(tdm_layer *layer, tdm_info_layer *info);
tdm_error    fbdev_layer_get_info(tdm_layer *layer, tdm_info_layer *info);
tdm_error    fbdev_layer_set_buffer(tdm_layer *layer, tbm_surface_h buffer);
tdm_error    fbdev_layer_unset_buffer(tdm_layer *layer);

typedef struct _tdm_fbdev_data
{
    int fbdev_fd;

    tdm_display *dpy;

    struct fb_fix_screeninfo finfo;
    struct fb_var_screeninfo vinfo;

    void *vaddr;
    size_t size;
}tdm_fbdev_data;


#endif /* _TDM_fbdev_H_ */
