#ifndef _TDM_DRM_PP_H_
#define _TDM_DRM_PP_H_

#include "tdm_drm.h"

tdm_error    tdm_drm_pp_get_capability(tdm_drm_data *drm_data, tdm_caps_pp *caps);
tdm_pp*      tdm_drm_pp_create(tdm_drm_data *drm_data, tdm_error *error);
void         tdm_drm_pp_handler(unsigned int prop_id, unsigned int *buf_idx,
                                   unsigned int tv_sec, unsigned int tv_usec, void *data);
void         tdm_drm_pp_cb(int fd, unsigned int prop_id, unsigned int *buf_idx,
                              unsigned int tv_sec, unsigned int tv_usec,
                              void *user_data);
#endif /* _TDM_DRM_PP_H_ */
