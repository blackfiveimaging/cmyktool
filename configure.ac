#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT([cmyktool], [0.1.6-pre2], [blackfive@fakenhamweb.co.uk])
AC_CONFIG_AUX_DIR([scripts])
AC_CONFIG_SRCDIR([deflatten.cpp])
AM_CONFIG_HEADER([config.h])
AM_INIT_AUTOMAKE([1.9 foreign])

AM_GNU_GETTEXT_VERSION([0.18.1])
AM_GNU_GETTEXT([external])

# Checks for programs.
AC_PROG_LIBTOOL
AC_PROG_CXX
AC_PROG_CC
AC_PROG_MAKE_SET
AC_PROG_CPP

# Are we building for Win32?
case $host_os in
  *mingw32* ) echo "Configuring for mingw"; LIBS="-lwinspool $LIBS"; CFLAGS="$CFLAGS -DNT"; CXXFLAGS="$CXXFLAGS -DNT";
esac

# By default we build with GTK.  Disable with --without-ui
AC_ARG_WITH([ui],
[AS_HELP_STRING([--without-ui],
  [disable user-interface support])],
[],
[with_ui=yes])

AS_IF([test "x$with_ui" != xno],
[
	PKG_CHECK_MODULES([GLIB2], [glib-2.0])
	PKG_CHECK_MODULES([GTHREAD], [gthread-2.0])
	PKG_CHECK_MODULES([GTK2], [gtk+-2.0],AC_DEFINE([HAVE_GTK],[1],[Use GTK+]))
	PKG_CHECK_MODULES([GDKPIXBUF2], [gdk-pixbuf-2.0])
	# Since the ProfileManager fetches an X atom from the server, we need to link with X11
	# explicitly on some systems
    AC_CHECK_LIB(X11, XInternAtom, LIBX11_LIBS="-lX11")
    AC_SUBST([LIBX11_LIBS])
])

PKG_CHECK_MODULES([LCMS2], [lcms2],[AC_DEFINE([HAVE_LCMS2], [1], [Use LCMS2])],
    [PKG_CHECK_MODULES([LCMS], [lcms],[AC_DEFINE([HAVE_LCMS], [1], [Use LCMS1])])])


AC_CHECK_LIB(m,pow, LIBM_LIBS="-lm")
AC_SUBST([LIBM_LIBS])
AC_CHECK_LIB([pnm], [pnm_readpaminit],,,[-lm])
AC_CHECK_LIB([netpbm], [pnm_readpaminit],,,[-lm])

# Some distributions have the pam.h header in a netpbm subfolder
AC_CHECK_HEADERS([netpbm/pam.h])

AC_CHECK_LIB([tiff], [TIFFOpen], [TIFF_LIBS="-ltiff"],
	[unset ac_cv_lib_tiff_TIFFOpen
		AC_CHECK_LIB([tiff], [TIFFOpen],[TIFF_LIBS="-ltiff"],
			AC_CHECK_LIB([tiff3], [TIFFOpen],[TIFF_LIBS="-ltiff3"],
				AC_MSG_FAILURE([libtiff or devel files not found])), [-lm])
])
AC_SUBST([TIFF_LIBS])
AC_CHECK_LIB([jpeg], [jpeg_read_header],,
	AC_CHECK_LIB([jpeg62], [jpeg_read_header],JPEG_LIBS="-ljpeg62",AC_MSG_FAILURE([libjpeg or devel files not found])))
AC_SUBST([JPEG_LIBS])

AC_CHECK_LIB([pthread], [pthread_attr_init])
AC_CHECK_LIB([pthreadGC2], [pthread_attr_init],,)

AC_CHECK_LIB([gnugetopt], [getopt_long], [GETOPT_LIBS=-lgnugetopt])
AC_SUBST([GETOPT_LIBS])


# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_CONST
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_STAT
AC_CHECK_FUNCS([memset mkdir strcasecmp strdup strncasecmp])

AC_CONFIG_FILES([Makefile po/Makefile.in
                 library/argyllsupport/Makefile
                 library/imagesource/Makefile
                 library/imageutils/Makefile
                 library/support/Makefile
		 library/miscwidgets/Makefile
		 library/pixbufthumbnail/Makefile
                 library/profilemanager/Makefile
				 gfx/Makefile])
AC_OUTPUT
