noinst_LTLIBRARIES = libmcfilehighlight.la

libmcfilehighlight_la_SOURCES = \
	common.c \
	get-color.c \
	ini-file-read.c \
	internal.h

AM_CPPFLAGS = \
	-DDATADIR=\""$(pkgdatadir)/"\" \
	-DLOCALEDIR=\""$(localedir)"\"

AM_CFLAGS = -I$(top_srcdir) $(GLIB_CFLAGS) $(PCRE_CFLAGS)
