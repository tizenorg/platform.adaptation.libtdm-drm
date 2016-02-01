#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#if HAVE_UDEV
#include <libudev.h>
#endif

#include "tdm_fbdev.h"

#include <tdm_helper.h>

#define TDM_FBDEV_NAME "fbdev"

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

static tdm_fbdev_data *fbdev_data;

static int
_tdm_fbdev_open_fbdev(void)
{
    const char *name = "/dev/fb0";
    int fd = -1;

    fd = open(name, O_RDWR);
    if (fd < 0)
    {
        TDM_ERR("Cannot open fbdev device.. search by udev");
        goto close;
    }

    /*
     * TODO: If we failed directly to open framebuffer device
     * we would try to open it through udev
     */

close:
    return fd;
}

static tdm_error
_tdm_fbdev_init_internal(void)
{
    struct fb_fix_screeninfo finfo;
    struct fb_var_screeninfo vinfo;
    int ret = -1;

    ret = ioctl(fbdev_data->fbdev_fd, FBIOGET_VSCREENINFO, &vinfo);
    if (ret < 0)
    {
        TDM_ERR("FBIOGET_VSCREENINFO ioctl failed errno=%d", errno);
        goto close_1;
    }

    vinfo.reserved[0] = 0;
    vinfo.reserved[1] = 0;
    vinfo.reserved[2] = 0;
    vinfo.xoffset = 0;
    vinfo.yoffset = 0;
    vinfo.activate = FB_ACTIVATE_NOW;

    /*
     * Explicitly request 32 bits per pixel colors with corresponding
     * red, blue and green color offsets and length of colors
     */
    vinfo.bits_per_pixel = 32;
    vinfo.red.offset     = 16;
    vinfo.red.length     = 8;
    vinfo.green.offset   = 8;
    vinfo.green.length   = 8;
    vinfo.blue.offset    = 0;
    vinfo.blue.length    = 8;
    vinfo.transp.offset  = 0;
    vinfo.transp.length  = 0;

    /*
     * Almost all framebuffers support off screen rendering.
     * The code bellow requests Framebuffer to allocate memory equals to three
     * buffers each of which width*height size. While the first drawn
     * framebuffer's area is displaying the second or the third is redrawing
     * or compositing by some application. When timer was expired or vblank
     * was received Framebufer's areas swap, thus the second or the third is
     * displaying and first is redrawing or compositing. Simple representation
     * of what was said bellow
     *
     *                    SWAP Event                SWAP Event
     *                        |                         |
     *    +-------------+     |     +-------------+     |      +-------------+
     *    |             |     |     |             |     |      |  Redrawing  |
     *    | Displaying  |     |     |   queued    |     |      |     or      |
     *    |             |     |     |             |     |      | Compositing |
     *    +-------------+     |     +-------------+     |      +-------------+
     *    |  Redrawing  |     |     |             |     |      |             |
     *    |     or      | +-------> | Displaying  | +--------> |   queued    |
     *    | Compositing |     |     |             |     |      |             |
     *    +-------------+     |     +-------------+     |      +-------------+
     *    |             |     |     |  Redrawing  |     |      |             |
     *    |   queued    |     |     |     or      |     |      | Displaying  |
     *    |             |     |     | Compositing |     |      |             |
     *    +-------------+     |     +-------------+     |      +-------------+
     *                        |                         |
     */
    vinfo.yres_virtual = vinfo.yres * MAX_BUF;

    ret = ioctl(fbdev_data->fbdev_fd, FBIOPAN_DISPLAY, vinfo);
    if(ret < 0)
    {
        TDM_INFO("page flip not supported,  errno=%d", errno);
        vinfo.yres_virtual = vinfo.yres;

        /*
         * TODO: Does FBIOPAN_DISPLAY ioctl must be invoked for the second time?
         */
        ret = ioctl(fbdev_data->fbdev_fd, FBIOPAN_DISPLAY, vinfo);
        if(ret < 0)
        {
            TDM_ERR("FBIOPAN_DISPLAY ioctl failed, errno=%d", errno);
            goto close_1;
        }
    }

    ret = ioctl(fbdev_data->fbdev_fd, FBIOGET_FSCREENINFO, finfo);
    if (ret < 0)
    {
        TDM_ERR("FBIOGET_FSCREENINFO ioctl failed, errno=%d", errno);
        goto close_1;
    }

    if (finfo.smem_len <= 0)
    {
        TDM_ERR("Length of frame buffer mem less then 0");
        goto close_1;
    }

    /*
     * Output framebuffer's related information
     */
    TDM_INFO("\n"
             " VInfo\n"
             "   fb           = %d\n"
             "   xres         = %d px \n"
             "   yres         = %d px \n"
             "   xres_virtual = %d px \n"
             "   yres_virtual = %d px \n"
             "   bpp          = %d    \n"
             "   r            = %2u:%u\n"
             "   g            = %2u:%u\n"
             "   b            = %2u:%u\n"
             "   t            = %2u:%u\n"
             "   active       = %d    \n"
             "   width        = %d mm \n"
             "   height       = %d mm \n",
             fbdev_data->fbdev_fd,
             vinfo.xres,
             vinfo.yres,
             vinfo.xres_virtual,
             vinfo.yres_virtual,
             vinfo.bits_per_pixel,
             vinfo.red.offset, vinfo.red.length,
             vinfo.green.offset, vinfo.green.length,
             vinfo.blue.offset, vinfo.blue.length,
             vinfo.transp.offset, vinfo.transp.length,
             vinfo.activate,
             vinfo.width,
             vinfo.height);

    TDM_INFO("\n"
             " FInfo\n"
             "   id          = %s\n"
             "   smem_len    = %d\n"
             "   line_length = %d\n",
             finfo.id,
             finfo.smem_len,
             finfo.line_length);

    return TDM_ERROR_NONE;
close_1:
    ret = TDM_ERROR_OPERATION_FAILED;
    return ret;
}

void
tdm_fbdev_deinit(tdm_backend_data *bdata)
{
    if (fbdev_data != bdata)
        return;

    TDM_INFO("deinit");

    close(fbdev_data->fbdev_fd);

    free(fbdev_data);
    fbdev_data = NULL;
}

tdm_backend_data*
tdm_fbdev_init(tdm_display *dpy, tdm_error *error)
{
    tdm_error ret;
    if (!dpy)
    {
        TDM_ERR("display is null");
        if(error)
            *error = TDM_ERROR_BAD_REQUEST;
        return NULL;
    }

    if(fbdev_data)
    {
        TDM_ERR("failed: init twice");
        if(error)
            *error = TDM_ERROR_BAD_REQUEST;
        return NULL;
    }

    fbdev_data = calloc(1, sizeof(fbdev_data));
    if (!fbdev_data)
    {
        TDM_ERR("alloc failed");
        if(error)
            *error = TDM_ERROR_OUT_OF_MEMORY;
        return NULL;
    }

    ret = tdm_backend_register_func_display(dpy, &fbdev_func_display);
    if (ret != TDM_ERROR_NONE)
        goto failed;

    fbdev_data->dpy = dpy;

    /*
     * TODO: does somebody open framebuffer device beside us?
     */
    fbdev_data->fbdev_fd = _tdm_fbdev_open_fbdev();
    if(fbdev_data->fbdev_fd < 0)
    {
        ret = TDM_ERROR_OPERATION_FAILED;
        goto failed;
    }

    ret = _tdm_fbdev_init_internal();
    if(ret != TDM_ERROR_NONE)
    {
        TDM_INFO("init of framebuffer failed");
        goto failed;
    }

    ret = tdm_fbdev_creat_output(fbdev_data);
    if (ret != TDM_ERROR_NONE)
    {
        TDM_INFO("init of output failed");
        goto failed_2;
    }


    TDM_INFO("init success!");

    if(error)
        *error = TDM_ERROR_NONE;

    return (tdm_backend_data*)fbdev_data;

failed_2:
    tdm_fbdev_destroy_output(fbdev_data);

failed:
    if(error)
        *error = ret;

    tdm_fbdev_deinit(fbdev_data);

    TDM_ERR("init failed!");
    return NULL;
}

tdm_backend_module tdm_backend_module_data =
{
    "fbdev",
    "Samsung",
    TDM_BACKEND_ABI_VERSION,
    tdm_fbdev_init,
    tdm_fbdev_deinit
};
