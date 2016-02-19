#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <drm_fourcc.h>
#include <tbm_surface.h>

#include "tdm_drm.h"

typedef struct {
	tbm_format  tbm_format;
	uint32_t    drm_format;
} tbm_drm_format_data;

static const tbm_drm_format_data formats[] = {
	{TBM_FORMAT_C8, DRM_FORMAT_C8},
	{TBM_FORMAT_RGB332, DRM_FORMAT_RGB332},
	{TBM_FORMAT_BGR233, DRM_FORMAT_BGR233},
	{TBM_FORMAT_XRGB4444, DRM_FORMAT_XRGB4444},
	{TBM_FORMAT_XBGR4444, DRM_FORMAT_XBGR4444},
	{TBM_FORMAT_RGBX4444, DRM_FORMAT_RGBX4444},
	{TBM_FORMAT_BGRX4444, DRM_FORMAT_BGRX4444},
	{TBM_FORMAT_ARGB4444, DRM_FORMAT_ARGB4444},
	{TBM_FORMAT_ABGR4444, DRM_FORMAT_ABGR4444},
	{TBM_FORMAT_RGBA4444, DRM_FORMAT_RGBA4444},
	{TBM_FORMAT_BGRA4444, DRM_FORMAT_BGRA4444},
	{TBM_FORMAT_XRGB1555, DRM_FORMAT_XRGB1555},
	{TBM_FORMAT_XBGR1555, DRM_FORMAT_XBGR1555},
	{TBM_FORMAT_RGBX5551, DRM_FORMAT_RGBX5551},
	{TBM_FORMAT_BGRX5551, DRM_FORMAT_BGRX5551},
	{TBM_FORMAT_ARGB1555, DRM_FORMAT_ARGB1555},
	{TBM_FORMAT_ABGR1555, DRM_FORMAT_ABGR1555},
	{TBM_FORMAT_RGBA5551, DRM_FORMAT_RGBA5551},
	{TBM_FORMAT_BGRA5551, DRM_FORMAT_BGRA5551},
	{TBM_FORMAT_RGB565, DRM_FORMAT_RGB565},
	{TBM_FORMAT_BGR565, DRM_FORMAT_BGR565},
	{TBM_FORMAT_RGB888, DRM_FORMAT_RGB888},
	{TBM_FORMAT_BGR888, DRM_FORMAT_BGR888},
	{TBM_FORMAT_XRGB8888, DRM_FORMAT_XRGB8888},
	{TBM_FORMAT_XBGR8888, DRM_FORMAT_XBGR8888},
	{TBM_FORMAT_RGBX8888, DRM_FORMAT_RGBX8888},
	{TBM_FORMAT_BGRX8888, DRM_FORMAT_BGRX8888},
	{TBM_FORMAT_ARGB8888, DRM_FORMAT_ARGB8888},
	{TBM_FORMAT_ABGR8888, DRM_FORMAT_ABGR8888},
	{TBM_FORMAT_RGBA8888, DRM_FORMAT_RGBA8888},
	{TBM_FORMAT_BGRA8888, DRM_FORMAT_BGRA8888},
	{TBM_FORMAT_XRGB2101010, DRM_FORMAT_XRGB2101010},
	{TBM_FORMAT_XBGR2101010, DRM_FORMAT_XBGR2101010},
	{TBM_FORMAT_RGBX1010102, DRM_FORMAT_RGBX1010102},
	{TBM_FORMAT_BGRX1010102, DRM_FORMAT_BGRX1010102},
	{TBM_FORMAT_ARGB2101010, DRM_FORMAT_ARGB2101010},
	{TBM_FORMAT_ABGR2101010, DRM_FORMAT_ABGR2101010},
	{TBM_FORMAT_RGBA1010102, DRM_FORMAT_RGBA1010102},
	{TBM_FORMAT_BGRA1010102, DRM_FORMAT_BGRA1010102},
	{TBM_FORMAT_YUYV, DRM_FORMAT_YUYV},
	{TBM_FORMAT_YVYU, DRM_FORMAT_YVYU},
	{TBM_FORMAT_UYVY, DRM_FORMAT_UYVY},
	{TBM_FORMAT_VYUY, DRM_FORMAT_VYUY},
	{TBM_FORMAT_AYUV, DRM_FORMAT_AYUV},
	{TBM_FORMAT_NV12, DRM_FORMAT_NV12},
	{TBM_FORMAT_NV21, DRM_FORMAT_NV21},
	{TBM_FORMAT_NV16, DRM_FORMAT_NV16},
	{TBM_FORMAT_NV61, DRM_FORMAT_NV61},
	{TBM_FORMAT_YUV410, DRM_FORMAT_YUV410},
	{TBM_FORMAT_YVU410, DRM_FORMAT_YVU410},
	{TBM_FORMAT_YUV411, DRM_FORMAT_YUV411},
	{TBM_FORMAT_YVU411, DRM_FORMAT_YVU411},
	{TBM_FORMAT_YUV420, DRM_FORMAT_YUV420},
	{TBM_FORMAT_YVU420, DRM_FORMAT_YVU420},
	{TBM_FORMAT_YUV422, DRM_FORMAT_YUV422},
	{TBM_FORMAT_YVU422, DRM_FORMAT_YVU422},
	{TBM_FORMAT_YUV444, DRM_FORMAT_YUV444},
	{TBM_FORMAT_YVU444, DRM_FORMAT_YVU444},
};

#define NUM_FORMATS (sizeof(formats) / sizeof(formats[0]))

uint32_t
tdm_drm_format_to_drm_format(tbm_format format)
{
	int i;

	for (i = 0; i < NUM_FORMATS; i++)
		if (formats[i].tbm_format == format)
			return formats[i].drm_format;

	TDM_ERR("tbm format '%c%c%c%c' not found", FOURCC_STR(format));

	return 0;
}

tbm_format
tdm_drm_format_to_tbm_format(uint32_t format)
{
	int i;

	for (i = 0; i < NUM_FORMATS; i++)
		if (formats[i].drm_format == format)
			return formats[i].tbm_format;

	TDM_ERR("drm format '%c%c%c%c' not found", FOURCC_STR(format));

	return 0;
}
