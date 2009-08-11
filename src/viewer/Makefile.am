
noinst_LTLIBRARIES = libmcviewer.la

libmcviewer_la_SOURCES = \
	actions_cmd.c \
	coord_cache.c \
	datasource.c \
	dialogs.c \
	display.c \
	growbuf.c \
	hex.c \
	internal.h \
	lib.c \
	mcviewer.c \
	mcviewer.h \
	move.c \
	nroff.c \
	plain.c \
	search.c

libmcviewer_la_CFLAGS=-I../ -I$(top_srcdir)/src \
	$(GLIB_CFLAGS) \
	-DDATADIR=\""$(pkgdatadir)/"\" -DLOCALEDIR=\""$(localedir)"\"

