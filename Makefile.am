# Makefile.am

SUBDIRS = po library/argyllsupport library/support library/imagesource library/imageutils library/miscwidgets library/profilemanager library/pixbufthumbnail gfx

localedir=$(datadir)/locale

AM_CXXFLAGS = -DLOCALEDIR=\"$(localedir)\" -Wall -Ilibrary/support/ -Ilibrary/imagesource/ -Ilibrary/imageutils \
	-Ilibrary/miscwidgets -Ilibrary/profilemanager -Ilibrary/pixbufthumbnail -Ilibrary/argyllsupport \
	$(LCMS_CFLAGS) $(GTK2_CFLAGS)

noinst_LTLIBRARIES = libdeflatten.la libdeflattenui.la

libdeflatten_la_SOURCES = \
	cmyktool_core.cpp \
	cmyktool_core.h \
	conversionopts.cpp \
	conversionopts.h \
	cmtransformworker.h	\
	imagesource_deflatten.cpp	\
	imagesource_deflatten.h \
	imagesource_flatten.cpp	\
	imagesource_flatten.h \
	imagesource_combine.cpp \
	imagesource_combine.h \
	externalghostscript.h \
	psrip_core.cpp	\
	psrip_core.h	\
	devicelink.cpp	\
	devicelink.h	\
	gettext.h


libdeflattenui_la_SOURCES = \
	cmykuitab.cpp	\
	cmykuitab.h	\
	conversionoptsdialog.cpp \
	conversionoptsdialog.h \
	devicelinkdialog.cpp \
	devicelinkdialog.h \
	psripui.cpp	\
	psripui.h	\
	dialogs.cpp	\
	dialogs.h

bin_PROGRAMS = cmyktool

check_PROGRAMS = queuetest convertcmyk deflatten recombine decompose psrip test dltest

dltest_SOURCES = dltest.cpp
test_SOURCES = test.cpp
convertcmyk_SOURCES = convertcmyk.cpp
queuetest_SOURCES = queuetest.cpp
recombine_SOURCES = recombine.cpp
decompose_SOURCES = decompose.cpp
deflatten_SOURCES = deflatten.cpp
psrip_SOURCES = psrip.cpp
cmyktool_SOURCES = cmyktool.cpp

LDADD = \
	libdeflattenui.la \
	libdeflatten.la \
	library/argyllsupport/libargyllsupport.la \
	library/imageutils/libimageutils.la	\
	library/miscwidgets/libmiscwidgets.la	\
	library/support/libsupport.la	\
	library/imagesource/libimagesource.la	\
	library/profilemanager/libprofilemanager.la	\
	library/support/libsupport.la	\
	library/pixbufthumbnail/libpixbufthumbnail.la	\
	$(LIBINTL) $(LIBM_LIBS) $(GETOPT_LIBS) $(JPEG_LIBS) $(PNM_LIBS) $(TIFF_LIBS) $(LCMS_LIBS) $(GTK2_LIBS) $(X11_LIBS)


library/imagesource/libimagesource.la:
	cd library/imagesource
	$(MAKE) libimagesource.la
	
library/support/libsupport.la:
	cd library/support
	$(MAKE) libsupport.la

library/profilemanager/libprofilemanager.la:
	cd library/profilemanager
	$(MAKE) libprofilemanager.la

library/pixbufthumbnail/libpixbufthumbnail.la:
	cd library/pixbufthumbnail
	$(MAKE) libpixbufthumbnail.la


desktopdir = $(datadir)/applications
desktop_DATA = cmyktool.desktop

icondir = $(datadir)/icons/hicolor/48x48/apps/
icon_DATA = cmyktool.png



EXTRA_DIST = COPYING README TODO cmyktool.desktop cmyktool.png

