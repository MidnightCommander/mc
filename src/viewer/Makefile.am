
noinst_LTLIBRARIES = libmcviewer.la

libmcviewer_la_SOURCES = \
	actions_cmd.c \
	dialogs.c \
	hex.c \
	internal.h \
	lib.c \
	mcviewer.c \
	mcviewer.h \
	nroff.c \
	plain.c

libmcviewer_la_CFLAGS=-I../ -I$(top_srcdir)/src \
	$(GLIB_CFLAGS) \
	-DDATADIR=\""$(pkgdatadir)/"\" -DLOCALEDIR=\""$(localedir)"\"

