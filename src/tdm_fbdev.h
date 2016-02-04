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

/* fbdev backend functions (display) */
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

/* Framebuffer moudel's internal macros, functions, structures */
#define NEVER_GET_HERE() TDM_ERR("** NEVER GET HERE **")

#define RETURN_VAL_IF_FAIL(cond, val) {\
    if (!(cond)) {\
        TDM_ERR("'%s' failed", #cond);\
        return val;\
    }\
}

typedef struct _tdm_fbdev_output_data tdm_fbdev_output_data;
typedef struct _tdm_fbdev_layer_data tdm_fbdev_layer_data;
typedef struct _tdm_fbdev_display_buffer tdm_fbdev_display_buffer;

typedef struct _tdm_fbdev_data
{
    tdm_fbdev_output_data *fbdev_output;

    int fbdev_fd;

    tdm_display *dpy;

    struct fb_fix_screeninfo *finfo;
    struct fb_var_screeninfo *vinfo;

    struct list_head buffer_list;
} tdm_fbdev_data;

struct _tdm_fbdev_output_data
{
    tdm_fbdev_data *fbdev_data;
    tdm_fbdev_layer_data *fbdev_layer;

    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t bpp;
    size_t   size;
    uint32_t max_width;
    uint32_t max_height;

    /*
     * Poinetr to Framebuffers's mapped memory
     */
    void *mem;

    int count_modes;
    tdm_output_mode *output_modes;
    int mode_changed;

    /*
     * We currently support only one mode
     */
    const tdm_output_mode *current_mode;

    tdm_output_type connector_type;
    tdm_output_conn_status status;
    unsigned int connector_type_id;

    tdm_output_dpms dpms_value;

    /*
     * Event handlers
     */
    tdm_output_vblank_handler vblank_func;
    tdm_output_commit_handler commit_func;
};

struct _tdm_fbdev_layer_data
{
    tdm_fbdev_data *fbdev_data;
    tdm_fbdev_output_data *fbdev_output;

    tdm_fbdev_display_buffer *display_buffer;
    int display_buffer_changed;

    tdm_layer_capability capabilities;
    tdm_info_layer info;
    int info_changed;
};

struct _tdm_fbdev_display_buffer
{
    struct list_head link;

    int width;
    int height;
    int size;

    /*
     * Buffer's mapped memory
     */
    void *mem;

    tbm_surface_h buffer;
};

tdm_error    tdm_fbdev_creat_output(tdm_fbdev_data *fbdev_data);
void         tdm_fbdev_destroy_output(tdm_fbdev_data *fbdev_data);

tdm_error    tdm_fbdev_creat_layer(tdm_fbdev_data *fbdev_data);
void         tdm_fbdev_destroy_layer(tdm_fbdev_data *fbdev_data);

#endif /* _TDM_fbdev_H_ */
