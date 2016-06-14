#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <pixman.h>

#include "tdm_drm.h"
#include "tdm_helper.h"

typedef struct _tdm_drm_pp_buffer {
	tbm_surface_h src;
	tbm_surface_h dst;

	struct list_head link;
} tdm_drm_pp_buffer;

typedef struct _tdm_drm_pp_data {
	tdm_drm_data *drm_data;

	tdm_info_pp info;

	struct list_head pending_buffer_list;

	tdm_pp_done_handler done_func;
	void *done_user_data;

	struct list_head link;
} tdm_drm_pp_data;


static tbm_format pp_formats[] = {
	TBM_FORMAT_ARGB8888,
	TBM_FORMAT_XRGB8888,
	TBM_FORMAT_YUV420,
	TBM_FORMAT_YVU420
};

#define NUM_PP_FORMAT   (sizeof(pp_formats) / sizeof(pp_formats[0]))

static int pp_list_init;
static struct list_head pp_list;

static pixman_format_code_t
_tdm_drm_pp_pixman_get_format(tbm_format tbmfmt)
{
	switch (tbmfmt) {
	case TBM_FORMAT_ARGB8888:
		return PIXMAN_a8r8g8b8;
	case TBM_FORMAT_XRGB8888:
		return PIXMAN_x8r8g8b8;
	case TBM_FORMAT_YUV420:
	case TBM_FORMAT_YVU420:
		return PIXMAN_yv12;
	default:
		return 0;
	}
}

int
_tdm_drm_pp_pixman_convert(pixman_op_t op,
                           unsigned char *srcbuf, unsigned char *dstbuf,
                           pixman_format_code_t src_format, pixman_format_code_t dst_format,
                           int sbw, int sbh, int sx, int sy, int sw, int sh,
                           int dbw, int dbh, int dx, int dy, int dw, int dh,
                           int rotate, int hflip, int vflip)
{
	pixman_image_t    *src_img;
	pixman_image_t    *dst_img;
	struct pixman_f_transform ft;
	pixman_transform_t transform;
	int                src_stride, dst_stride;
	int                src_bpp;
	int                dst_bpp;
	double             scale_x, scale_y;
	int                rotate_step;
	int                ret = 0;

	RETURN_VAL_IF_FAIL(srcbuf != NULL, 0);
	RETURN_VAL_IF_FAIL(dstbuf != NULL, 0);

	TDM_DBG("src(%dx%d: %d,%d %dx%d) dst(%dx%d: %d,%d %dx%d) flip(%d,%d), rot(%d)",
	        sbw, sbh, sx, sy, sw, sh, dbw, dbh, dx, dy, dw, dh, hflip, vflip, rotate);

	src_bpp = PIXMAN_FORMAT_BPP(src_format) / 8;
	RETURN_VAL_IF_FAIL(src_bpp > 0, 0);

	dst_bpp = PIXMAN_FORMAT_BPP(dst_format) / 8;
	RETURN_VAL_IF_FAIL(dst_bpp > 0, 0);

	rotate_step = (rotate + 360) / 90 % 4;

	src_stride = sbw * src_bpp;
	dst_stride = dbw * dst_bpp;

	src_img = pixman_image_create_bits(src_format, sbw, sbh, (uint32_t *)srcbuf,
	                                   src_stride);
	dst_img = pixman_image_create_bits(dst_format, dbw, dbh, (uint32_t *)dstbuf,
	                                   dst_stride);

	GOTO_IF_FAIL(src_img != NULL, CANT_CONVERT);
	GOTO_IF_FAIL(dst_img != NULL, CANT_CONVERT);

	pixman_f_transform_init_identity(&ft);

	if (hflip) {
		pixman_f_transform_scale(&ft, NULL, -1, 1);
		pixman_f_transform_translate(&ft, NULL, dw, 0);
	}

	if (vflip) {
		pixman_f_transform_scale(&ft, NULL, 1, -1);
		pixman_f_transform_translate(&ft, NULL, 0, dh);
	}

	if (rotate_step > 0) {
		int c = 0, s = 0, tx = 0, ty = 0;
		switch (rotate_step) {
		case 1: /* 90 degrees */
			s = -1, tx = -dw;
			break;
		case 2: /* 180 degrees */
			c = -1, tx = -dw, ty = -dh;
			break;
		case 3: /* 270 degrees */
			s = 1, ty = -dh;
			break;
		default:
			break;
		}

		pixman_f_transform_translate(&ft, NULL, tx, ty);
		pixman_f_transform_rotate(&ft, NULL, c, s);
	}

	if (rotate_step % 2 == 0) {
		scale_x = (double)sw / dw;
		scale_y = (double)sh / dh;
	} else {
		scale_x = (double)sw / dh;
		scale_y = (double)sh / dw;
	}

	pixman_f_transform_scale(&ft, NULL, scale_x, scale_y);
	pixman_f_transform_translate(&ft, NULL, sx, sy);

	pixman_transform_from_pixman_f_transform(&transform, &ft);
	pixman_image_set_transform(src_img, &transform);

	pixman_image_composite(op, src_img, NULL, dst_img, 0, 0, 0, 0, dx, dy, dw, dh);

	ret = 1;

CANT_CONVERT:
	if (src_img)
		pixman_image_unref(src_img);
	if (dst_img)
		pixman_image_unref(dst_img);

	return ret;
}

static tdm_error
_tdm_drm_pp_convert(tdm_drm_pp_buffer *buffer, tdm_info_pp *info)
{
	tbm_surface_info_s src_info, dst_info;
	pixman_format_code_t src_format, dst_format;
	int sbw, dbw;
	int rotate = 0, hflip = 0;

	RETURN_VAL_IF_FAIL(buffer != NULL, TDM_ERROR_INVALID_PARAMETER);
	RETURN_VAL_IF_FAIL(buffer->src != NULL, TDM_ERROR_INVALID_PARAMETER);
	RETURN_VAL_IF_FAIL(buffer->dst != NULL, TDM_ERROR_INVALID_PARAMETER);
	RETURN_VAL_IF_FAIL(info != NULL, TDM_ERROR_INVALID_PARAMETER);

	/* not handle buffers which have 2 more gem handles */

	memset(&src_info, 0, sizeof(tbm_surface_info_s));
	tbm_surface_map(buffer->src, TBM_OPTION_READ, &src_info);
	GOTO_IF_FAIL(src_info.planes[0].ptr != NULL, fail_convert);

	memset(&dst_info, 0, sizeof(tbm_surface_info_s));
	tbm_surface_map(buffer->dst, TBM_OPTION_WRITE, &dst_info);
	GOTO_IF_FAIL(dst_info.planes[0].ptr != NULL, fail_convert);

	src_format = _tdm_drm_pp_pixman_get_format(src_info.format);
	GOTO_IF_FAIL(src_format > 0, fail_convert);
	dst_format = _tdm_drm_pp_pixman_get_format(dst_info.format);
	GOTO_IF_FAIL(dst_format > 0, fail_convert);

	if (src_info.format == TBM_FORMAT_YUV420) {
		if (dst_info.format == TBM_FORMAT_XRGB8888)
			dst_format = PIXMAN_x8b8g8r8;
		else if (dst_info.format == TBM_FORMAT_ARGB8888)
			dst_format = PIXMAN_a8b8g8r8;
		else if (dst_info.format == TBM_FORMAT_YVU420) {
			TDM_ERR("can't convert %c%c%c%c to %c%c%c%c",
			        FOURCC_STR(src_info.format), FOURCC_STR(dst_info.format));
			goto fail_convert;
		}
	}
	/* need checking for other formats also? */

	if (IS_RGB(src_info.format))
		sbw = src_info.planes[0].stride >> 2;
	else
		sbw = src_info.planes[0].stride;

	if (IS_RGB(dst_info.format))
		dbw = dst_info.planes[0].stride >> 2;
	else
		dbw = dst_info.planes[0].stride;

	rotate = (info->transform % 4) * 90;
	if (info->transform >= TDM_TRANSFORM_FLIPPED)
		hflip = 1;

	_tdm_drm_pp_pixman_convert(PIXMAN_OP_SRC,
	                           src_info.planes[0].ptr, dst_info.planes[0].ptr,
	                           src_format, dst_format,
	                           sbw, src_info.height,
	                           info->src_config.pos.x, info->src_config.pos.y,
	                           info->src_config.pos.w, info->src_config.pos.h,
	                           dbw, dst_info.height,
	                           info->dst_config.pos.x, info->dst_config.pos.y,
	                           info->dst_config.pos.w, info->dst_config.pos.h,
	                           rotate, hflip, 0);
	tbm_surface_unmap(buffer->src);
	tbm_surface_unmap(buffer->dst);

	return TDM_ERROR_NONE;
fail_convert:
	tbm_surface_unmap(buffer->src);
	tbm_surface_unmap(buffer->dst);
	return TDM_ERROR_OPERATION_FAILED;
}

tdm_error
tdm_drm_pp_get_capability(tdm_drm_data *drm_data, tdm_caps_pp *caps)
{
	int i;

	if (!caps) {
		TDM_ERR("invalid params");
		return TDM_ERROR_INVALID_PARAMETER;
	}

	caps->capabilities = TDM_PP_CAPABILITY_ASYNC;

	caps->format_count = NUM_PP_FORMAT;

	/* will be freed in frontend */
	caps->formats = calloc(1, sizeof pp_formats);
	if (!caps->formats) {
		TDM_ERR("alloc failed");
		return TDM_ERROR_OUT_OF_MEMORY;
	}
	for (i = 0; i < caps->format_count; i++)
		caps->formats[i] = pp_formats[i];

	caps->min_w = 16;
	caps->min_h = 8;
	caps->max_w = -1;   /* not defined */
	caps->max_h = -1;
	caps->preferred_align = 16;

	return TDM_ERROR_NONE;
}

tdm_pp *
tdm_drm_pp_create(tdm_drm_data *drm_data, tdm_error *error)
{
	tdm_drm_pp_data *pp_data = calloc(1, sizeof(tdm_drm_pp_data));
	if (!pp_data) {
		TDM_ERR("alloc failed");
		if (error)
			*error = TDM_ERROR_OUT_OF_MEMORY;
		return NULL;
	}

	pp_data->drm_data = drm_data;

	LIST_INITHEAD(&pp_data->pending_buffer_list);

	if (!pp_list_init) {
		pp_list_init = 1;
		LIST_INITHEAD(&pp_list);
	}
	LIST_ADDTAIL(&pp_data->link, &pp_list);

	return pp_data;
}

void
drm_pp_destroy(tdm_pp *pp)
{
	tdm_drm_pp_data *pp_data = pp;
	tdm_drm_pp_buffer *b = NULL, *bb = NULL;

	if (!pp_data)
		return;

	LIST_FOR_EACH_ENTRY_SAFE(b, bb, &pp_data->pending_buffer_list, link) {
		LIST_DEL(&b->link);
		free(b);
	}

	LIST_DEL(&pp_data->link);

	free(pp_data);
}

tdm_error
drm_pp_set_info(tdm_pp *pp, tdm_info_pp *info)
{
	tdm_drm_pp_data *pp_data = pp;

	RETURN_VAL_IF_FAIL(pp_data, TDM_ERROR_INVALID_PARAMETER);
	RETURN_VAL_IF_FAIL(info, TDM_ERROR_INVALID_PARAMETER);

	pp_data->info = *info;

	return TDM_ERROR_NONE;
}

tdm_error
drm_pp_attach(tdm_pp *pp, tbm_surface_h src, tbm_surface_h dst)
{
	tdm_drm_pp_data *pp_data = pp;
	tdm_drm_pp_buffer *buffer;

	RETURN_VAL_IF_FAIL(pp_data, TDM_ERROR_INVALID_PARAMETER);
	RETURN_VAL_IF_FAIL(src, TDM_ERROR_INVALID_PARAMETER);
	RETURN_VAL_IF_FAIL(dst, TDM_ERROR_INVALID_PARAMETER);

	if (tbm_surface_internal_get_num_bos(src) > 1 ||
	    tbm_surface_internal_get_num_bos(dst) > 1) {
		TDM_ERR("can't handle multiple tbm bos");
		return TDM_ERROR_OPERATION_FAILED;
	}

	buffer = calloc(1, sizeof(tdm_drm_pp_buffer));
	if (!buffer) {
		TDM_ERR("alloc failed");
		return TDM_ERROR_NONE;
	}

	LIST_ADDTAIL(&buffer->link, &pp_data->pending_buffer_list);

	buffer->src = src;
	buffer->dst = dst;

	return TDM_ERROR_NONE;
}

tdm_error
drm_pp_commit(tdm_pp *pp)
{
	tdm_drm_pp_data *pp_data = pp;
	tdm_drm_pp_buffer *b = NULL, *bb = NULL;

	RETURN_VAL_IF_FAIL(pp_data, TDM_ERROR_INVALID_PARAMETER);

	LIST_FOR_EACH_ENTRY_SAFE(b, bb, &pp_data->pending_buffer_list, link) {
		LIST_DEL(&b->link);

		_tdm_drm_pp_convert(b, &pp_data->info);

		if (pp_data->done_func)
			pp_data->done_func(pp_data,
			                   b->src,
			                   b->dst,
			                   pp_data->done_user_data);
		free(b);
	}

	return TDM_ERROR_NONE;
}

tdm_error
drm_pp_set_done_handler(tdm_pp *pp, tdm_pp_done_handler func, void *user_data)
{
	tdm_drm_pp_data *pp_data = pp;

	RETURN_VAL_IF_FAIL(pp_data, TDM_ERROR_INVALID_PARAMETER);
	RETURN_VAL_IF_FAIL(func, TDM_ERROR_INVALID_PARAMETER);

	pp_data->done_func = func;
	pp_data->done_user_data = user_data;

	return TDM_ERROR_NONE;
}
