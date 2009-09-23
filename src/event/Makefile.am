
noinst_LTLIBRARIES = libmcevent.la

libmcevent_la_SOURCES = \
	event.h \
	event.c \
	internal.h \
	manage.c \
	raise.c

libmcevent_la_CFLAGS=-I../ -I$(top_srcdir)/src \
	$(GLIB_CFLAGS) \
	-DDATADIR=\""$(pkgdatadir)/"\" -DLOCALEDIR=\""$(localedir)"\"

