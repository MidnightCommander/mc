noinst_LTLIBRARIES = libdiffviewer.la

libdiffviewer_la_SOURCES = \
	internal.h \
	search.c \
	ydiff.c ydiff.h

AM_CPPFLAGS = -I$(top_srcdir) $(GLIB_CFLAGS) $(PCRE_CPPFLAGS)

libdiffviewer_la_LIBADD = ../../lib/libmc.la
