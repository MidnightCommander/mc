
noinst_LTLIBRARIES = libmcviewer.la

libmcviewer_la_SOURCES = \
	actions_cmd.c \
	coord_cache.c \
	datasource.c \
	dialogs.c \
	display.c \
	growbuf.c \
	hex.c \
	inlines.h \
	internal.h \
	lib.c \
	mcviewer.c \
	mcviewer.h \
	move.c \
	nroff.c \
	plain.c \
	search.c

AM_CPPFLAGS = -I$(top_srcdir) $(GLIB_CFLAGS) $(PCRE_CPPFLAGS)

libmcviewer_la_LIBADD = ../../lib/libmc.la
