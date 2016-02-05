AC_PREREQ([2.60])
AC_INIT([libtdm-fbdev],
        [1.0.0],
        [https://www.tizen.org],
        [libtdm-fbdev])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_SRCDIR([Makefile.am])
AM_INIT_AUTOMAKE([1.10 foreign dist-bzip2])
AM_MAINTAINER_MODE([enable])

# Check for programs
AC_PROG_CC

AC_USE_SYSTEM_EXTENSIONS
AC_SYS_LARGEFILE
AC_FUNC_ALLOCA

# Initialize libtool
LT_PREREQ([2.2])
LT_INIT([disable-static])

# Enable quiet compiles on automake 1.11.
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

PKG_CHECK_MODULES(TDM_DRM, libtdm libtbm libdrm)
PKG_CHECK_MODULES(UDEV, libudev, [udev=yes], [udev=no])
if test x"$udev" = xyes; then
	AC_DEFINE(HAVE_UDEV,1,[Enable udev-based monitor hotplug detection])
	TDM_DRM_CFLAGS="$TDM_DRM_CFLAGS $UDEV_CFLAGS"
	TDM_DRM_LIBS="$TDM_DRM_LIBS $UDEV_LIBS"
fi

AC_SUBST(TDM_DRM_CFLAGS)
AC_SUBST(TDM_DRM_LIBS)

AC_DEFINE_UNQUOTED(LIBDRM_MAJOR_VERSION, [`pkg-config --modversion libdrm | cut -d '.' -f 1`], dnl
                   [libdrm major version])
AC_DEFINE_UNQUOTED(LIBDRM_MIROR_VERSION, [`pkg-config --modversion libdrm | cut -d '.' -f 2`], dnl
                   [libdrm major version])
AC_DEFINE_UNQUOTED(LIBDRM_MICRO_VERSION, [`pkg-config --modversion libdrm | cut -d '.' -f 3`], dnl
                   [libdrm major version])

# set the dir for the tbm module
DEFAULT_TDM_MODULE_PATH="${libdir}/tdm"
AC_ARG_WITH(tdm-module-path, AS_HELP_STRING([--with-tdm-module-path=PATH], [tdm module dir]),
				[ TDM_MODULE_PATH="$withval" ],
				[ TDM_MODULE_PATH="${DEFAULT_TDM_MODULE_PATH}" ])
AC_SUBST(TDM_MODULE_PATH)

# For enumerating devices in test case
AC_OUTPUT([
	Makefile
	src/Makefile])

echo ""
echo "$PACKAGE_STRING will be compiled with:"
echo ""
echo "TDM_DRM_CFLAGS : $TDM_DRM_CFLAGS"
echo "TDM_DRM_LIBS   : $TDM_DRM_LIBS"
echo "TDM_MODULE_DIR    : $TDM_MODULE_PATH"
echo ""
