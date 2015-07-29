# -*- Autoconf -*-

# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT([v4l-gst-bufferpool-rcar],[1.0.0])

AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([Makefile.am])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile])

AC_GNU_SOURCE

AM_INIT_AUTOMAKE([1.9 subdir-objects no-dist-gzip dist-bzip2 -Wno-portability])

m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

# Checks for programs.
AC_PROG_CC_C99
AC_PROG_INSTALL
AC_PROG_LIBTOOL
AC_PROG_MKDIR_P

PKG_PROG_PKG_CONFIG
PKG_CHECK_MODULES(KMS, libkms libdrm)
PKG_CHECK_MODULES(GST, gstreamer-video-1.0 >= 1.4 gstreamer-app-1.0,
	[AC_DEFINE([GST_14], [1], [Gstreamer version 1.4])],
	[PKG_CHECK_MODULES(GST, gstreamer-video-1.0 gstreamer-app-1.0)])

OLD_CFLAGS="$CFLAGS"
CFLAGS="$CFLAGS $GST_CFLAGS"
AC_CHECK_HEADER([libv4l-gst-bufferpool.h], [], AC_MSG_ERROR([libv4l-gst-bufferpool.h is needed]))
CFLAGS="$OLD_CFLAGS"

AC_ARG_WITH(libv4l-gst-bufferpool-libdir,
        AS_HELP_STRING([--with-libv4l-gst-bufferpool-libdir=DIR], [set libv4l-gst external bufferpool library dir (default=libv4l/plugins/v4l-gst-bufferpool)]),
        [],
        [with_poollibdir="libv4l/plugins/v4l-gst-bufferpool"])

AC_SUBST([libv4lgstbufferpooldir], [$libdir/$with_poollibdir])

AC_DEFINE_DIR([POOL_LIB_DIR], [libv4lgstbufferpooldir], [libv4l-gst external bufferpool library directory])

CPPFLAGS="-Wall -Wpointer-arith $CPPFLAGS"

AC_OUTPUT
