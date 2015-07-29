AUTOMAKE_OPTIONS = foreign
ACLOCAL_AMFLAGS = -I m4

libv4lgstbufferpool_LTLIBRARIES = libv4l-gst-bufferpool-rcar.la

libv4l_gst_bufferpool_rcar_la_SOURCES = libv4l-gst-bufferpool-rcar.c
libv4l_gst_bufferpool_rcar_la_CFLAGS = $(GST_CFLAGS) $(KMS_CFLAGS) -lgstallocators-1.0
libv4l_gst_bufferpool_rcar_la_CPPFLAGS = $(CFLAG_VISIBILITY)
libv4l_gst_bufferpool_rcar_la_LIBADD = $(GST_LIBS) $(KMS_LIBS)
libv4l_gst_bufferpool_rcar_la_LDFLAGS = -avoid-version -module -shared \
					-export-dynamic -lpthread
