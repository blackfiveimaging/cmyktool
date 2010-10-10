# Makefile.am

SUBDIRS = po numlib rspl argyllsupport support imagesource imageutils miscwidgets profilemanager pixbufthumbnail gfx

localedir=$(datadir)/locale

AM_CXXFLAGS = -DLOCALEDIR=\"$(localedir)\" -Wall -I./support/ -I./imagesource/ -I./imageutils \
	-I./miscwidgets -I./profilemanager -I./pixbufthumbnail -I./numlib -I./rspl -I./argyllsupport \
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
	naivecmyktransform.h \
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

bin_PROGRAMS = cmyktool duotoner

check_PROGRAMS = queuetest convertcmyk deflatten recombine decompose psrip test rspltest dltest

duotoner_SOURCES = duotoner.cpp
dltest_SOURCES = dltest.cpp
rspltest_SOURCES = rspltest.cpp
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
	argyllsupport/libargyllsupport.la \
	imageutils/libimageutils.la	\
	miscwidgets/libmiscwidgets.la	\
	support/libsupport.la	\
	imagesource/libimagesource.la	\
	profilemanager/libprofilemanager.la	\
	support/libsupport.la	\
	pixbufthumbnail/libpixbufthumbnail.la	\
	rspl/librspl.la  \
	numlib/libargyllnum.la  \
	$(LIBINTL) $(LIBM_LIBS) $(GETOPT_LIBS) $(JPEG_LIBS) $(PNM_LIBS) $(TIFF_LIBS) $(LCMS_LIBS) $(GTK2_LIBS) $(X11_LIBS)


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
desktop_DATA = cmyktool.desktop duotoner.desktop

icondir = $(datadir)/icons/hicolor/48x48/apps/
icon_DATA = cmyktool.png duotoner.png



EXTRA_DIST = COPYING README TODO cmyktool.desktop cmyktool.png duotoner.desktop duotoner.png


