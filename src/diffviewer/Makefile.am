noinst_LTLIBRARIES = libdiffviewer.la

libdiffviewer_la_SOURCES = \
	internal.h \
	search.c \
	ydiff.c ydiff.h

AM_CPPFLAGS = \
	-DDATADIR=\""$(pkgdatadir)/"\" \
	-DLOCALEDIR=\""$(localedir)"\"

AM_CFLAGS = -I$(top_srcdir) $(GLIB_CFLAGS) $(PCRE_CFLAGS)

libdiffviewer_la_LIBADD = ../../lib/libmc.la
