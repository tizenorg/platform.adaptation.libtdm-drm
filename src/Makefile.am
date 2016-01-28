AM_CFLAGS = \
	$(TDM_DRM_CFLAGS) \
	-I$(top_srcdir)/src

libtdm_fbdev_la_LTLIBRARIES = libtdm-fbdev.la
libtdm_fbdev_ladir = $(TDM_MODULE_PATH)
libtdm_fbdev_la_LDFLAGS = -module -avoid-version
libtdm_fbdev_la_LIBADD = $(TDM_DRM_LIBS) -ldl

libtdm_fbdev_la_SOURCES = \
	tdm_fbdev_display.c \
	tdm_fbdev.c
