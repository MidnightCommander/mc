
noinst_LTLIBRARIES = libmcevent.la

libmcevent_la_SOURCES = \
	callback.c \
	event.h \
	event.c \
	internal.h

libmcevent_la_CFLAGS=-I../ -I$(top_srcdir)/src \
	$(GLIB_CFLAGS) \
	-DDATADIR=\""$(pkgdatadir)/"\" -DLOCALEDIR=\""$(localedir)"\"

