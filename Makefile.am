# Makefile.am

SUBDIRS = po support imagesource imageutils miscwidgets profilemanager pixbufthumbnail

localedir=$(datadir)/locale

AM_CXXFLAGS = -DLOCALEDIR=\"$(localedir)\" -Wall -I./support/ -I./imagesource/ -I./imageutils \
	-I./miscwidgets -I./profilemanager -I./pixbufthumbnail \
	$(LCMS_CFLAGS) $(GTK2_CFLAGS)

noinst_LTLIBRARIES = libdeflatten.la

libdeflatten_la_SOURCES = \
	cmyktypes.h \
	imagesource_deflatten.cpp	\
	imagesource_deflatten.h \
	imagesource_flatten.cpp	\
	imagesource_flatten.h \
	imagesource_plane.cpp \
	imagesource_plane.h \
	imagesource_extractchannel.cpp \
	imagesource_extractchannel.h

bin_PROGRAMS = deflatten recombine decompose psrip

recombine_SOURCES = recombine.cpp
decompose_SOURCES = decompose.cpp
deflatten_SOURCES = deflatten.cpp
psrip_SOURCES = psrip.cpp

LDADD = \
	imageutils/libimageutils.la	\
	miscwidgets/libmiscwidgets.la	\
	support/libsupport.la	\
	imagesource/libimagesource.la	\
	support/libsupport.la	\
	profilemanager/libprofilemanager.la	\
	pixbufthumbnail/libpixbufthumbnail.la	\
	libdeflatten.la \
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


EXTRA_DIST = COPYING README TODO


