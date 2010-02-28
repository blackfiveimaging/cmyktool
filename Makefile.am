# Makefile.am

SUBDIRS = po support imagesource imageutils miscwidgets profilemanager pixbufthumbnail gfx

localedir=$(datadir)/locale

AM_CXXFLAGS = -DLOCALEDIR=\"$(localedir)\" -Wall -I./support/ -I./imagesource/ -I./imageutils \
	-I./miscwidgets -I./profilemanager -I./pixbufthumbnail \
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
	imagesource_extractchannel.cpp \
	imagesource_extractchannel.h	\
	naivecmyktransform.h \
	externalghostscript.h \
	psrip_core.cpp	\
	psrip_core.h	\
	gettext.h


libdeflattenui_la_SOURCES = \
	cmykuitab.cpp	\
	cmykuitab.h	\
	conversionoptsdialog.cpp \
	conversionoptsdialog.h \
	psripui.cpp	\
	psripui.h	\
	dialogs.cpp	\
	dialogs.h

bin_PROGRAMS = cmyktool

check_PROGRAMS = queuetest convertcmyk deflatten recombine decompose psrip test

test_SOURCES = test.cpp
convertcmyk_SOURCES = convertcmyk.cpp
queuetest_SOURCES = queuetest.cpp
recombine_SOURCES = recombine.cpp
decompose_SOURCES = decompose.cpp
deflatten_SOURCES = deflatten.cpp
psrip_SOURCES = psrip.cpp
cmyktool_SOURCES = cmyktool.cpp
tabui_SOURCES = tabui.cpp

LDADD = \
	libdeflattenui.la \
	libdeflatten.la \
	imageutils/libimageutils.la	\
	miscwidgets/libmiscwidgets.la	\
	support/libsupport.la	\
	imagesource/libimagesource.la	\
	profilemanager/libprofilemanager.la	\
	support/libsupport.la	\
	pixbufthumbnail/libpixbufthumbnail.la	\
	$(LIBINTL) $(LIBM_LIBS) $(GETOPT_LIBS) $(JPEG_LIBS) $(PNM_LIBS) $(TIFF_LIBS) $(LCMS_LIBS) $(GTK2_LIBS)


imagesource/libimagesource.la:
	cd imagesource
	$(MAKE) libimagesource.la
	
support/libsupport.la:
	cd support
	$(MAKE) libsupport.la

profilemanager/libprofilemanager.la:
	cd profilemanager
	$(MAKE) libprofilemanager.la

pixbufthumbnail/libpixbufthumbnail.la:
	cd pixbufthumbnail
	$(MAKE) libpixbufthumbnail.la


desktopdir = $(datadir)/applications
desktop_DATA = cmyktool.desktop

icondir = $(datadir)/icons/hicolor/48x48/apps/
icon_DATA = cmyktool.png



EXTRA_DIST = COPYING README TODO cmyktool.desktop cmyktool.png

